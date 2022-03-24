#include <string.h>
#include "frt_global.h"
#include "frt_search.h"
#include "frt_helper.h"

#define MTQ(query) ((FrtMultiTermQuery *)(query))

/***************************************************************************
 *
 * MultiTerm
 *
 ***************************************************************************/

/***************************************************************************
 * BoostedTerm
 ***************************************************************************/

typedef struct BoostedTerm
{
    char *term;
    float boost;
} BoostedTerm;

static bool boosted_term_less_than(const BoostedTerm *bt1, const BoostedTerm *bt2)
{
    // if (bt1->boost == bt2->boost) { return (strcmp(bt1->term, bt2->term) < 0); }
    // return (bt1->boost < bt2->boost);
    if (bt1->boost < bt2->boost) {
        return true;
    }
    if (bt1->boost > bt2->boost) {
        return false;
    }
    return (strcmp(bt1->term, bt2->term) < 0);
}

static void boosted_term_destroy(BoostedTerm *self)
{
    free(self->term);
    free(self);
}

static BoostedTerm *boosted_term_new(const char *term, float boost)
{
    BoostedTerm *self = FRT_ALLOC(BoostedTerm);
    self->term = frt_estrdup(term);
    self->boost = boost;
    return self;
}

/***************************************************************************
 * TermDocEnumWrapper
 ***************************************************************************/

#define TDE_READ_SIZE 16

typedef struct TermDocEnumWrapper
{
    const char  *term;
    FrtTermDocEnum *tde;
    float        boost;
    int          doc;
    int          freq;
    int          docs[TDE_READ_SIZE];
    int          freqs[TDE_READ_SIZE];
    int          pointer;
    int          pointer_max;
} TermDocEnumWrapper;

static bool tdew_less_than(const TermDocEnumWrapper *tdew1,
                           const TermDocEnumWrapper *tdew2)
{
    return (tdew1->doc < tdew2->doc);
}

static bool tdew_next(TermDocEnumWrapper *self)
{
    self->pointer++;
    if (self->pointer >= self->pointer_max) {
        /* refill buffer */
        self->pointer_max = self->tde->read(self->tde, self->docs, self->freqs, TDE_READ_SIZE);
        if (self->pointer_max != 0) {
            self->pointer = 0;
        }
        else {
            return false;
        }
    }
    self->doc = self->docs[self->pointer];
    self->freq = self->freqs[self->pointer];
    return true;
}

static bool tdew_skip_to(TermDocEnumWrapper *self, int doc_num)
{
    FrtTermDocEnum *tde = self->tde;

    while (++(self->pointer) < self->pointer_max) {
        if (self->docs[self->pointer] >= doc_num) {
            self->doc = self->docs[self->pointer];
            self->freq = self->freqs[self->pointer];
            return true;
        }
    }

    /* not found in cache, seek underlying stream */
    if (tde->skip_to(tde, doc_num)) {
        self->pointer_max = 1;
        self->pointer = 0;
        self->docs[0] = self->doc = tde->doc_num(tde);
        self->freqs[0] = self->freq = tde->freq(tde);
        return true;
    } else {
        return false;
    }
}

static void tdew_destroy(TermDocEnumWrapper *self)
{
    self->tde->close(self->tde);
    free(self);
}

static TermDocEnumWrapper *tdew_new(const char *term, FrtTermDocEnum *tde,
                                    float boost)
{
    TermDocEnumWrapper *self = FRT_ALLOC_AND_ZERO(TermDocEnumWrapper);
    self->term = term;
    self->tde = tde;
    self->boost = boost;
    self->doc = -1;
    return self;
}

/***************************************************************************
 * MultiTermScorer
 ***************************************************************************/

#define SCORE_CACHE_SIZE 32
#define MTSc(scorer) ((MultiTermScorer *)(scorer))

typedef struct MultiTermScorer
{
    FrtScorer                super;
    FrtSymbol                field;
    frt_uchar                *norms;
    FrtWeight               *weight;
    TermDocEnumWrapper  **tdew_a;
    int                   tdew_cnt;
    FrtPriorityQueue        *tdew_pq;
    float                 weight_value;
    float                 score_cache[SCORE_CACHE_SIZE];
    float                 total_score;
} MultiTermScorer;

static float multi_tsc_score(FrtScorer *self)
{
    return MTSc(self)->total_score * MTSc(self)->weight_value
        * frt_sim_decode_norm(self->similarity, MTSc(self)->norms[self->doc]);
}

static bool multi_tsc_next(FrtScorer *self)
{
    int curr_doc;
    float total_score = 0.0f;
    TermDocEnumWrapper *tdew;
    MultiTermScorer *mtsc = MTSc(self);
    FrtPriorityQueue *tdew_pq = mtsc->tdew_pq;
    if (tdew_pq == NULL) {
        TermDocEnumWrapper **tdew_a = mtsc->tdew_a;
        int i;
        tdew_pq = frt_pq_new(mtsc->tdew_cnt, (frt_lt_ft)tdew_less_than, (frt_free_ft)NULL);
        for (i = mtsc->tdew_cnt - 1; i >= 0; i--) {
            if (tdew_next(tdew_a[i])) {
                frt_pq_push(tdew_pq, tdew_a[i]);
            }
        }
        mtsc->tdew_pq = tdew_pq;
    }

    tdew = (TermDocEnumWrapper *)frt_pq_top(tdew_pq);
    if (tdew == NULL) {
        return false;
    }

    self->doc = curr_doc = tdew->doc;
    do {
        int freq = tdew->freq;
        if (freq < SCORE_CACHE_SIZE) {
            total_score += mtsc->score_cache[freq] * tdew->boost;
        }
        else {
            total_score += frt_sim_tf(self->similarity, (float)freq) * tdew->boost;
        }

        if (tdew_next(tdew)) {
            frt_pq_down(tdew_pq);
        }
        else {
            frt_pq_pop(tdew_pq);
        }

    } while (((tdew = (TermDocEnumWrapper *)frt_pq_top(tdew_pq)) != NULL)
             && tdew->doc == curr_doc);
    mtsc->total_score = total_score;
    return true;
}

static bool multi_tsc_advance_to(FrtScorer *self, int target_doc_num)
{
    FrtPriorityQueue *tdew_pq = MTSc(self)->tdew_pq;
    TermDocEnumWrapper *tdew;
    if (tdew_pq == NULL) {
        MultiTermScorer *mtsc = MTSc(self);
        TermDocEnumWrapper **tdew_a = mtsc->tdew_a;
        int i;
        tdew_pq = frt_pq_new(mtsc->tdew_cnt, (frt_lt_ft)tdew_less_than, (frt_free_ft)NULL);
        for (i = mtsc->tdew_cnt - 1; i >= 0; i--) {
            if (tdew_skip_to(tdew_a[i], target_doc_num)) {
                frt_pq_push(tdew_pq, tdew_a[i]);
            }
        }
        MTSc(self)->tdew_pq = tdew_pq;
    }
    if (tdew_pq->size == 0) {
        self->doc = -1;
        return false;
    }
    while ((tdew = (TermDocEnumWrapper *)frt_pq_top(tdew_pq)) != NULL
           && (target_doc_num > tdew->doc)) {
        if (tdew_skip_to(tdew, target_doc_num)) {
            frt_pq_down(tdew_pq);
        }
        else {
            frt_pq_pop(tdew_pq);
        }
    }
    tdew = (TermDocEnumWrapper *)frt_pq_top(tdew_pq);
    return (frt_pq_top(tdew_pq) == NULL) ? false : true;
}

static bool multi_tsc_skip_to(FrtScorer *self, int target_doc_num)
{
    return multi_tsc_advance_to(self, target_doc_num) && multi_tsc_next(self);
}

static FrtExplanation *multi_tsc_explain(FrtScorer *self, int doc_num)
{
    MultiTermScorer *mtsc = MTSc(self);
    TermDocEnumWrapper *tdew;

    if (multi_tsc_advance_to(self, doc_num) &&
        (tdew = (TermDocEnumWrapper *)frt_pq_top(mtsc->tdew_pq))->doc == doc_num) {

        FrtPriorityQueue *tdew_pq = MTSc(self)->tdew_pq;
        FrtExplanation *expl = frt_expl_new(0.0f, "The sum of:");
        int curr_doc = self->doc = tdew->doc;
        float total_score = 0.0f;

        do {
            int freq = tdew->freq;
            frt_expl_add_detail(expl,
                frt_expl_new(frt_sim_tf(self->similarity, (float)freq) * tdew->boost,
                         "tf(term_freq(%s:%s)=%d)^%f",
                         rb_id2name(mtsc->field), tdew->term, freq, tdew->boost));

            total_score += frt_sim_tf(self->similarity, (float)freq) * tdew->boost;

            /* maintain tdew queue, even though it probably won't get used
             * again */
            if (tdew_next(tdew)) {
                frt_pq_down(tdew_pq);
            }
            else {
                frt_pq_pop(tdew_pq);
            }

        } while (((tdew = (TermDocEnumWrapper *)frt_pq_top(tdew_pq)) != NULL)
                 && tdew->doc == curr_doc);
        expl->value = total_score;
        return expl;
    }
    else {
        return frt_expl_new(0.0f, "None of the required terms exist in the index");
    }
}

static void multi_tsc_destroy(FrtScorer *self)
{
    int i;
    TermDocEnumWrapper **tdew_a = MTSc(self)->tdew_a;
    for (i = MTSc(self)->tdew_cnt - 1; i >= 0; i--) {
        tdew_destroy(tdew_a[i]);
    }
    free(tdew_a);
    if (MTSc(self)->tdew_pq) frt_pq_destroy(MTSc(self)->tdew_pq);
    frt_scorer_destroy_i(self);
}

static FrtScorer *multi_tsc_new(FrtWeight *weight, FrtSymbol field,
                             TermDocEnumWrapper **tdew_a, int tdew_cnt,
                             frt_uchar *norms)
{
    int i;
    FrtScorer *self = frt_scorer_new(MultiTermScorer, weight->similarity);

    MTSc(self)->weight          = weight;
    MTSc(self)->field           = field;
    MTSc(self)->weight_value    = weight->value;
    MTSc(self)->tdew_a          = tdew_a;
    MTSc(self)->tdew_cnt        = tdew_cnt;
    MTSc(self)->norms           = norms;

    for (i = 0; i < SCORE_CACHE_SIZE; i++) {
        MTSc(self)->score_cache[i] = frt_sim_tf(self->similarity, (float)i);
    }

    self->score                 = &multi_tsc_score;
    self->next                  = &multi_tsc_next;
    self->skip_to               = &multi_tsc_skip_to;
    self->explain               = &multi_tsc_explain;
    self->destroy               = &multi_tsc_destroy;

    return self;
}

/***************************************************************************
 * MultiTermWeight
 ***************************************************************************/

static char *multi_tw_to_s(FrtWeight *self)
{
    return frt_strfmt("MultiTermWeight(%f)", self->value);
}

static FrtScorer *multi_tw_scorer(FrtWeight *self, FrtIndexReader *ir)
{
    FrtScorer *multi_tsc = NULL;
    FrtPriorityQueue *boosted_terms = MTQ(self->query)->boosted_terms;
    const int field_num = frt_fis_get_field_num(ir->fis, MTQ(self->query)->field);

    if (boosted_terms->size > 0 && field_num >= 0) {
        int i;
        FrtTermDocEnum *tde;
        FrtTermEnum *te = ir->terms(ir, field_num);
        TermDocEnumWrapper **tdew_a = FRT_ALLOC_N(TermDocEnumWrapper *,
                                             boosted_terms->size);
        int tdew_cnt = 0;
        /* Priority queues skip the first element */
        for (i = boosted_terms->size; i > 0; i--) {
            char *term;
            BoostedTerm *bt = (BoostedTerm *)boosted_terms->heap[i];
            if (((term = te->skip_to(te, bt->term)) != NULL) && (strcmp(term, bt->term) == 0)) {
                tde = ir->term_docs(ir);
                tde->seek_te(tde, te);
                tdew_a[tdew_cnt++] = tdew_new(bt->term, tde, bt->boost);
            }
        }
        te->close(te);
        if (tdew_cnt) {
            multi_tsc = multi_tsc_new(self, MTQ(self->query)->field, tdew_a,
                                      tdew_cnt, frt_ir_get_norms_i(ir, field_num));
        }
        else {
            free(tdew_a);
        }
    }
    return multi_tsc;
}

static FrtExplanation *multi_tw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num)
{
    FrtExplanation *expl;
    FrtExplanation *idf_expl1;
    FrtExplanation *idf_expl2;
    FrtExplanation *query_expl;
    FrtExplanation *qnorm_expl;
    FrtExplanation *field_expl;
    FrtExplanation *tf_expl;
    FrtScorer *scorer;
    frt_uchar *field_norms;
    float field_norm;
    FrtExplanation *field_norm_expl;

    char *query_str;
    FrtMultiTermQuery *mtq = MTQ(self->query);
    const char *field_name = rb_id2name(mtq->field);
    FrtPriorityQueue *bt_pq = mtq->boosted_terms;
    int i;
    int total_doc_freqs = 0;
    char *doc_freqs = NULL;
    size_t len = 0, pos = 0;
    const int field_num = frt_fis_get_field_num(ir->fis, mtq->field);

    if (field_num < 0) {
        return frt_expl_new(0.0f, "field \"%s\" does not exist in the index",
                        field_name);
    }

    query_str = self->query->to_s(self->query, (FrtSymbol)NULL);

    expl = frt_expl_new(0.0f, "weight(%s in %d), product of:", query_str, doc_num);

    len = 30;
    for (i = bt_pq->size; i > 0; i--) {
        len += strlen(((BoostedTerm *)bt_pq->heap[i])->term) + 30;
    }
    doc_freqs = FRT_ALLOC_N(char, len);
    for (i = bt_pq->size; i > 0; i--) {
        char *term = ((BoostedTerm *)bt_pq->heap[i])->term;
        int doc_freq = ir->doc_freq(ir, field_num, term);
        pos += sprintf(doc_freqs + pos, "(%s=%d) + ", term, doc_freq);
        total_doc_freqs += doc_freq;
    }
    pos -= 2; /* remove " + " from the end */
    sprintf(doc_freqs + pos, "= %d", total_doc_freqs);

    idf_expl1 = frt_expl_new(self->idf, "idf(%s:<%s>)", field_name, doc_freqs);
    idf_expl2 = frt_expl_new(self->idf, "idf(%s:<%s>)", field_name, doc_freqs);
    free(doc_freqs);

    /* explain query weight */
    query_expl = frt_expl_new(0.0f, "query_weight(%s), product of:", query_str);

    if (self->query->boost != 1.0f) {
        frt_expl_add_detail(query_expl, frt_expl_new(self->query->boost, "boost"));
    }
    frt_expl_add_detail(query_expl, idf_expl1);

    qnorm_expl = frt_expl_new(self->qnorm, "query_norm");
    frt_expl_add_detail(query_expl, qnorm_expl);

    query_expl->value = self->query->boost * self->idf * self->qnorm;

    frt_expl_add_detail(expl, query_expl);

    /* explain field weight */
    field_expl = frt_expl_new(0.0f, "field_weight(%s in %d), product of:",
                          query_str, doc_num);
    free(query_str);

    if ((scorer = self->scorer(self, ir)) != NULL) {
        tf_expl = scorer->explain(scorer, doc_num);
        scorer->destroy(scorer);
    }
    else {
        tf_expl = frt_expl_new(0.0f, "no terms were found");
    }
    frt_expl_add_detail(field_expl, tf_expl);
    frt_expl_add_detail(field_expl, idf_expl2);

    field_norms = ir->get_norms(ir, field_num);
    field_norm = (field_norms != NULL)
        ? frt_sim_decode_norm(self->similarity, field_norms[doc_num])
        : (float)0.0f;
    field_norm_expl = frt_expl_new(field_norm, "field_norm(field=%s, doc=%d)",
                               field_name, doc_num);

    frt_expl_add_detail(field_expl, field_norm_expl);

    field_expl->value = tf_expl->value * self->idf * field_norm;

    /* combine them */
    if (query_expl->value == 1.0f) {
        frt_expl_destroy(expl);
        return field_expl;
    }
    else {
        expl->value = (query_expl->value * field_expl->value);
        frt_expl_add_detail(expl, field_expl);
        return expl;
    }
}

static FrtWeight *multi_tw_new(FrtQuery *query, FrtSearcher *searcher)
{
    int i;
    int doc_freq         = 0;
    FrtWeight *self         = w_new(FrtWeight, query);
    FrtPriorityQueue *bt_pq = MTQ(query)->boosted_terms;

    self->scorer         = &multi_tw_scorer;
    self->explain        = &multi_tw_explain;
    self->to_s           = &multi_tw_to_s;

    self->similarity     = query->get_similarity(query, searcher);
    self->value          = query->boost;
    self->idf            = 0.0f;

    for (i = bt_pq->size; i > 0; i--) {
        doc_freq += searcher->doc_freq(searcher, MTQ(query)->field,
                                       ((BoostedTerm *)bt_pq->heap[i])->term);
    }
    self->idf += frt_sim_idf(self->similarity, doc_freq,
                         searcher->max_doc(searcher));

    return self;
}


/***************************************************************************
 * MultiTermQuery
 ***************************************************************************/

static char *multi_tq_to_s(FrtQuery *self, FrtSymbol default_field)
{
    int i;
    FrtPriorityQueue *boosted_terms = MTQ(self)->boosted_terms, *bt_pq_clone;
    BoostedTerm *bt;
    char *buffer, *bptr;
    const char *field_name = rb_id2name(MTQ(self)->field);
    int flen = strlen(field_name);
    int tlen = 0;

    /* Priority queues skip the first element */
    for (i = boosted_terms->size; i > 0; i--) {
        tlen += strlen(((BoostedTerm *)boosted_terms->heap[i])->term) + 35;
    }

    bptr = buffer = FRT_ALLOC_N(char, tlen + flen + 35);

    if (default_field != MTQ(self)->field) {
        bptr += sprintf(bptr, "%s:", field_name);
    }

    *(bptr++) = '"';
    bt_pq_clone = frt_pq_clone(boosted_terms);
    while ((bt = (BoostedTerm *)frt_pq_pop(bt_pq_clone)) != NULL) {
        bptr += sprintf(bptr, "%s", bt->term);

        if (bt->boost != 1.0f) {
            *bptr = '^';
            frt_dbl_to_s(++bptr, bt->boost);
            bptr += (int)strlen(bptr);
        }

        *(bptr++) = '|';
    }
    frt_pq_destroy(bt_pq_clone);

    if (bptr[-1] == '"') {
        bptr++; /* handle zero term case */
    }
    bptr[-1] =  '"'; /* delete last '|' char */
    bptr[ 0] = '\0';

    if (self->boost != 1.0f) {
        *bptr = '^';
        frt_dbl_to_s(++bptr, self->boost);
    }
    return buffer;
}

static void multi_tq_destroy_i(FrtQuery *self)
{
    frt_pq_destroy(MTQ(self)->boosted_terms);
    frt_q_destroy_i(self);
}

static void multi_tq_extract_terms(FrtQuery *self, FrtHashSet *terms)
{
    int i;
    FrtPriorityQueue *boosted_terms = MTQ(self)->boosted_terms;
    for (i = boosted_terms->size; i > 0; i--) {
        BoostedTerm *bt = (BoostedTerm *)boosted_terms->heap[i];
        frt_hs_add(terms, frt_term_new(MTQ(self)->field, bt->term));
    }
}

static unsigned long long multi_tq_hash(FrtQuery *self)
{
    int i;
    unsigned long long hash = frt_str_hash(rb_id2name(MTQ(self)->field));
    FrtPriorityQueue *boosted_terms = MTQ(self)->boosted_terms;
    for (i = boosted_terms->size; i > 0; i--) {
        BoostedTerm *bt = (BoostedTerm *)boosted_terms->heap[i];
        hash ^= frt_str_hash(bt->term) ^ frt_float2int(bt->boost);
    }
    return hash;
}

static int multi_tq_eq(FrtQuery *self, FrtQuery *o)
{
    int i;
    FrtPriorityQueue *boosted_terms1 = MTQ(self)->boosted_terms;
    FrtPriorityQueue *boosted_terms2 = MTQ(o)->boosted_terms;

    if ((MTQ(self)->field != MTQ(o)->field)
        || boosted_terms1->size != boosted_terms2->size) {
        return false;
    }
    for (i = boosted_terms1->size; i > 0; i--) {
        BoostedTerm *bt1 = (BoostedTerm *)boosted_terms1->heap[i];
        BoostedTerm *bt2 = (BoostedTerm *)boosted_terms2->heap[i];
        if ((strcmp(bt1->term, bt2->term) != 0) || (bt1->boost != bt2->boost)) {
            return false;
        }
    }
    return true;
}

static FrtMatchVector *multi_tq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv,
                                          FrtTermVector *tv)
{
    if (tv->field == MTQ(self)->field) {
        int i;
        FrtPriorityQueue *boosted_terms = MTQ(self)->boosted_terms;
        for (i = boosted_terms->size; i > 0; i--) {
            int j;
            BoostedTerm *bt = (BoostedTerm *)boosted_terms->heap[i];
            FrtTVTerm *tv_term = frt_tv_get_tv_term(tv, bt->term);
            if (tv_term) {
                for (j = 0; j < tv_term->freq; j++) {
                    int pos = tv_term->positions[j];
                    frt_matchv_add(mv, pos, pos);
                }
            }
        }
    }
    return mv;
}

FrtQuery *frt_multi_tq_new_conf(FrtSymbol field, int max_terms, float min_boost)
{
    FrtQuery *self;

    if (max_terms <= 0) {
        FRT_RAISE(FRT_ARG_ERROR, ":max_terms must be greater than or equal to zero. "
              "%d < 0. ", max_terms);
    }

    self                     = frt_q_new(FrtMultiTermQuery);

    MTQ(self)->field         = field;
    MTQ(self)->boosted_terms = frt_pq_new(max_terms,
                                      (frt_lt_ft)&boosted_term_less_than,
                                      (frt_free_ft)&boosted_term_destroy);
    MTQ(self)->min_boost     = min_boost;

    self->type               = MULTI_TERM_QUERY;
    self->to_s               = &multi_tq_to_s;
    self->extract_terms      = &multi_tq_extract_terms;
    self->hash               = &multi_tq_hash;
    self->eq                 = &multi_tq_eq;
    self->destroy_i          = &multi_tq_destroy_i;
    self->create_weight_i    = &multi_tw_new;
    self->get_matchv_i       = &multi_tq_get_matchv_i;

    return self;
}

FrtQuery *frt_multi_tq_new(FrtSymbol field)
{
    return frt_multi_tq_new_conf(field, MULTI_TERM_QUERY_MAX_TERMS, 0.0f);
}

void frt_multi_tq_add_term_boost(FrtQuery *self, const char *term, float boost)
{
    if (boost > MTQ(self)->min_boost && term && term[0]) {
        BoostedTerm *bt = boosted_term_new(term, boost);
        FrtPriorityQueue *bt_pq = MTQ(self)->boosted_terms;
        frt_pq_insert(bt_pq, bt);
        if (frt_pq_full(bt_pq)) {
            MTQ(self)->min_boost = ((BoostedTerm *)frt_pq_top(bt_pq))->boost;
        }
    }
}

void frt_multi_tq_add_term(FrtQuery *self, const char *term)
{
    frt_multi_tq_add_term_boost(self, term, 1.0f);
}
