#ifndef FRT_IND_H
#define FRT_IND_H

#include "frt_search.h"
#include "frt_index.h"
#include <ruby/encoding.h>

/***************************************************************************
 *
 * FrtIndex
 *
 ***************************************************************************/

typedef struct FrtIndex
{
    FrtConfig config;
    frt_mutex_t mutex;
    FrtStore *store;
    FrtAnalyzer *analyzer;
    FrtIndexReader *ir;
    FrtIndexWriter *iw;
    FrtSearcher *sea;
    FrtQParser *qp;
    FrtHashSet *key;
    FrtSymbol id_field;
    FrtSymbol def_field;
    /* for FrtIndexWriter */
    bool auto_flush : 1;
    bool has_writes : 1;
    bool check_latest : 1;
} FrtIndex;

extern FrtIndex *frt_index_new(FrtStore *store, FrtAnalyzer *analyzer, FrtHashSet *def_fields, bool create);
extern void frt_index_destroy(FrtIndex *self);
extern int frt_index_size(FrtIndex *self);
extern void frt_index_optimize(FrtIndex *self);
extern bool frt_index_is_deleted(FrtIndex *self, int doc_num);
extern void frt_index_add_doc(FrtIndex *self, FrtDocument *doc);
extern FrtTopDocs *frt_index_search_str(FrtIndex *self, char *query, int first_doc, int num_docs, FrtFilter *filter, FrtSort *sort, FrtPostFilter *post_filter, rb_encoding *encoding);
extern FrtQuery *frt_index_get_query(FrtIndex *self, char *qstr, rb_encoding *encoding);
extern FrtDocument *frt_index_get_doc(FrtIndex *self, int doc_num);
extern FrtDocument *frt_index_get_doc_ts(FrtIndex *self, int doc_num);
extern FrtDocument *frt_index_get_doc_id(FrtIndex *self, const char *id);
extern FrtDocument *frt_index_get_doc_term(FrtIndex *self, FrtSymbol field, const char *term);
extern void frt_index_delete(FrtIndex *self, int doc_num);
extern void frt_index_delete_term(FrtIndex *self, FrtSymbol field, const char *term);
extern void frt_index_delete_id(FrtIndex *self, const char *id);
extern void frt_index_delete_query(FrtIndex *self, FrtQuery *q, FrtFilter *f, FrtPostFilter *pf);
extern void frt_index_delete_query_str(FrtIndex *self, char *qstr,FrtFilter *f, FrtPostFilter *pf, rb_encoding *encoding);

extern void frt_ensure_searcher_open(FrtIndex *self);
extern void frt_ensure_reader_open(FrtIndex *self);
extern void frt_ensure_writer_open(FrtIndex *self);

#endif
