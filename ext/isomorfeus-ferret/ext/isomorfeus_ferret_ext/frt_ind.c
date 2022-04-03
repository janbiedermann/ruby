#include "frt_ind.h"
#include "frt_array.h"
#include <string.h>

#undef close

static const char *NON_UNIQUE_KEY_ERROR_MSG =
    "Tried to use a key that was not unique";

#define INDEX_CLOSE_READER(self) do { \
    if (self->sea) {                  \
        frt_searcher_close(self->sea);    \
        self->sea = NULL;             \
        self->ir = NULL;              \
    } else if (self->ir) {            \
        frt_ir_close(self->ir);           \
        self->ir = NULL;              \
    }                                 \
} while (0)

#define AUTOFLUSH_IR(self) do {                 \
     if (self->auto_flush) frt_ir_commit(self->ir); \
    else self->has_writes = true;               \
} while(0)

#define AUTOFLUSH_IW(self) do {  \
    if (self->auto_flush) {      \
        frt_iw_close(self->iw);      \
        self->iw = NULL;         \
    } else {                     \
        self->has_writes = true; \
    }                            \
} while (0)

FrtIndex *frt_index_new(FrtStore *store, FrtAnalyzer *analyzer, FrtHashSet *def_fields,
                 bool create)
{
    FrtIndex *self = FRT_ALLOC_AND_ZERO(FrtIndex);
    FrtHashSetEntry *hse;
    /* FIXME: need to add these to the query parser */
    self->config = frt_default_config;
    frt_mutex_init(&self->mutex, NULL);
    self->has_writes = false;
    if (store) {
        FRT_REF(store);
        self->store = store;
    } else {
        self->store = frt_open_ram_store(NULL);
        create = true;
    }
    if (analyzer) {
        self->analyzer = analyzer;
        FRT_REF(analyzer);
    } else {
        self->analyzer = frt_standard_analyzer_new(true);
    }

    if (create) {
        FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
        frt_index_create(self->store, fis);
        frt_fis_deref(fis);
    }

    /* options */
    self->key = NULL;
    self->id_field = rb_intern("id");
    self->def_field = rb_intern("id");
    self->auto_flush = false;
    self->check_latest = true;

    FRT_REF(self->analyzer);
    self->qp = frt_qp_new(self->analyzer);
    for (hse = def_fields->first; hse; hse = hse->next) {
        frt_qp_add_field(self->qp, (FrtSymbol)hse->elem, true, true);
    }
    /* Index is a convenience class so set qp convenience options */
    self->qp->allow_any_fields = true;
    self->qp->clean_str = true;
    self->qp->handle_parse_errors = true;

    return self;
}

void frt_index_destroy(FrtIndex *self)
{
    frt_mutex_destroy(&self->mutex);
    INDEX_CLOSE_READER(self);
    if (self->iw) frt_iw_close(self->iw);
    frt_store_deref(self->store);
    frt_a_deref(self->analyzer);
    if (self->qp) frt_qp_destroy(self->qp);
    if (self->key) frt_hs_destroy(self->key);
    free(self);
}


void frt_ensure_writer_open(FrtIndex *self)
{
    if (!self->iw) {
        INDEX_CLOSE_READER(self);

        /* make sure the analzyer isn't deleted by the FrtIndexWriter */
        FRT_REF(self->analyzer);
        self->iw = frt_iw_open(NULL, self->store, self->analyzer, false);
        self->iw->config.use_compound_file = self->config.use_compound_file;
    }
}

void frt_ensure_reader_open(FrtIndex *self)
{
    if (self->ir) {
        if (self->check_latest && !frt_ir_is_latest(self->ir)) {
            INDEX_CLOSE_READER(self);
            self->ir = frt_ir_open(NULL, self->store);
        }
        return;
    }
    if (self->iw) {
        frt_iw_close(self->iw);
        self->iw = NULL;
    }
    self->ir = frt_ir_open(NULL, self->store);
}

void frt_ensure_searcher_open(FrtIndex *self)
{
    frt_ensure_reader_open(self);
    if (!self->sea) {
        self->sea = frt_isea_new(self->ir);
    }
}

int frt_index_size(FrtIndex *self)
{
    int size;
    frt_mutex_lock(&self->mutex);
    {
        frt_ensure_reader_open(self);
        size = self->ir->num_docs(self->ir);
    }
    frt_mutex_unlock(&self->mutex);
    return size;
}

void frt_index_optimize(FrtIndex *self)
{
    frt_mutex_lock(&self->mutex);
    {
        frt_ensure_writer_open(self);
        frt_iw_optimize(self->iw);
        AUTOFLUSH_IW(self);
    }
    frt_mutex_unlock(&self->mutex);
}

bool frt_index_is_deleted(FrtIndex *self, int doc_num)
{
    bool is_del;
    frt_mutex_lock(&self->mutex);
    {
        frt_ensure_reader_open(self);
        is_del = self->ir->is_deleted(self->ir, doc_num);
    }
    frt_mutex_unlock(&self->mutex);
    return is_del;
}

static void index_del_doc_with_key_i(FrtIndex *self, FrtDocument *doc,
                                            FrtHashSet *key)
{
    FrtQuery *q;
    FrtTopDocs *td;
    FrtDocField *df;
    FrtHashSetEntry *hse;

    if (key->size == 1) {
        FrtSymbol field = (FrtSymbol)key->first->elem;
        frt_ensure_writer_open(self);
        df = frt_doc_get_field(doc, field);
        if (df) {
            frt_iw_delete_term(self->iw, field, df->data[0]);
        }
        return;
    }

    q = frt_bq_new(false);
    frt_ensure_searcher_open(self);

    for (hse = key->first; hse; hse = hse->next) {
        FrtSymbol field = (FrtSymbol)hse->elem;
        df = frt_doc_get_field(doc, field);
        if (!df) continue;
        frt_bq_add_query(q, frt_tq_new(field, df->data[0]), FRT_BC_MUST);
    }
    td = frt_searcher_search(self->sea, q, 0, 1, NULL, NULL, NULL);
    if (td->total_hits > 1) {
        frt_td_destroy(td);
        FRT_RAISE(FRT_ARG_ERROR, "%s", NON_UNIQUE_KEY_ERROR_MSG);
    } else if (td->total_hits == 1) {
        frt_ir_delete_doc(self->ir, td->hits[0]->doc);
    }
    frt_q_deref(q);
    frt_td_destroy(td);
}

static void index_add_doc_i(FrtIndex *self, FrtDocument *doc)
{
    if (self->key) {
        index_del_doc_with_key_i(self, doc, self->key);
    }
    frt_ensure_writer_open(self);
    frt_iw_add_doc(self->iw, doc);
    AUTOFLUSH_IW(self);
}

void frt_index_add_doc(FrtIndex *self, FrtDocument *doc)
{
    frt_mutex_lock(&self->mutex);
    {
        index_add_doc_i(self, doc);
    }
    frt_mutex_unlock(&self->mutex);
}

FrtQuery *frt_index_get_query(FrtIndex *self, char *qstr, rb_encoding *encoding)
{
    int i;
    FrtFieldInfos *fis;
    frt_ensure_searcher_open(self);
    fis = self->ir->fis;
    for (i = fis->size - 1; i >= 0; i--) {
        frt_hs_add(self->qp->all_fields, (void *)fis->fields[i]->name);
    }
    return qp_parse(self->qp, qstr, encoding);
}

FrtTopDocs *frt_index_search_str(FrtIndex *self, char *qstr, int first_doc,
                          int num_docs, FrtFilter *filter, FrtSort *sort,
                          FrtPostFilter *post_filter, rb_encoding *encoding)
{
    FrtQuery *query;
    FrtTopDocs *td;
    query = frt_index_get_query(self, qstr, encoding); /* will ensure_searcher is open */
    td = frt_searcher_search(self->sea, query, first_doc, num_docs,
                         filter, sort, post_filter);
    frt_q_deref(query);
    return td;
}

FrtDocument *frt_index_get_doc(FrtIndex *self, int doc_num)
{
    FrtDocument *doc;
    frt_ensure_reader_open(self);
    doc = self->ir->get_doc(self->ir, doc_num);
    return doc;
}

FrtDocument *frt_index_get_doc_ts(FrtIndex *self, int doc_num)
{
    FrtDocument *doc;
    frt_mutex_lock(&self->mutex);
    {
        doc = frt_index_get_doc(self, doc_num);
    }
    frt_mutex_unlock(&self->mutex);
    return doc;
}

FrtDocument *frt_index_get_doc_term(FrtIndex *self, FrtSymbol field,
                             const char *term)
{
    FrtDocument *doc = NULL;
    FrtTermDocEnum *tde;
    frt_mutex_lock(&self->mutex);
    {
        frt_ensure_reader_open(self);
        tde = ir_term_docs_for(self->ir, field, term);
        if (tde->next(tde)) {
            doc = frt_index_get_doc(self, tde->doc_num(tde));
        }
        tde->close(tde);
    }
    frt_mutex_unlock(&self->mutex);
    return doc;
}

FrtDocument *frt_index_get_doc_id(FrtIndex *self, const char *id)
{
    return frt_index_get_doc_term(self, self->id_field, id);
}

void frt_index_delete(FrtIndex *self, int doc_num)
{
    frt_mutex_lock(&self->mutex);
    {
        frt_ensure_reader_open(self);
        frt_ir_delete_doc(self->ir, doc_num);
        AUTOFLUSH_IR(self);
    }
    frt_mutex_unlock(&self->mutex);
}

void frt_index_delete_term(FrtIndex *self, FrtSymbol field, const char *term)
{
    FrtTermDocEnum *tde;
    frt_mutex_lock(&self->mutex);
    {
        if (self->ir) {
            tde = ir_term_docs_for(self->ir, field, term);
            FRT_TRY
                while (tde->next(tde)) {
                    frt_ir_delete_doc(self->ir, tde->doc_num(tde));
                    AUTOFLUSH_IR(self);
                }
            FRT_XFINALLY
                tde->close(tde);
            FRT_XENDTRY
        } else {
            frt_ensure_writer_open(self);
            frt_iw_delete_term(self->iw, field, term);
        }
    }
    frt_mutex_unlock(&self->mutex);
}

void frt_index_delete_id(FrtIndex *self, const char *id)
{
    frt_index_delete_term(self, self->id_field, id);
}

static void index_qdel_i(FrtSearcher *sea, int doc_num, float score, void *arg)
{
    (void)score; (void)arg;
    frt_ir_delete_doc(((FrtIndexSearcher *)sea)->ir, doc_num);
}

void frt_index_delete_query(FrtIndex *self, FrtQuery *q, FrtFilter *f,
                        FrtPostFilter *post_filter)
{
    frt_mutex_lock(&self->mutex);
    {
        frt_ensure_searcher_open(self);
        frt_searcher_search_each(self->sea, q, f, post_filter, &index_qdel_i, 0);
        AUTOFLUSH_IR(self);
    }
    frt_mutex_unlock(&self->mutex);
}

void frt_index_delete_query_str(FrtIndex *self, char *qstr, FrtFilter *f,
                            FrtPostFilter *post_filter, rb_encoding *encoding)
{
    FrtQuery *q = frt_index_get_query(self, qstr, encoding);
    frt_index_delete_query(self, q, f, post_filter);
    frt_q_deref(q);
}
