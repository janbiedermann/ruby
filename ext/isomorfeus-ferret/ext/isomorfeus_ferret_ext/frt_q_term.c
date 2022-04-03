#include "frt_global.h"
#include <string.h>
#include "frt_search.h"

#undef close
#undef read

#define TQ(query) ((FrtTermQuery *)(query))
#define TSc(scorer) ((TermScorer *)(scorer))

/***************************************************************************
 *
 * TermScorer
 *
 ***************************************************************************/

#define SCORE_CACHE_SIZE 32
#define TDE_READ_SIZE 32

typedef struct TermScorer {
    FrtScorer      super;
    int            docs[TDE_READ_SIZE];
    int            freqs[TDE_READ_SIZE];
    int            pointer;
    int            pointer_max;
    float          score_cache[SCORE_CACHE_SIZE];
    FrtWeight      *weight;
    FrtTermDocEnum *tde;
    frt_uchar      *norms;
    float          weight_value;
} TermScorer;

static float tsc_score(FrtScorer *self) {
    TermScorer *ts = TSc(self);
    int freq = ts->freqs[ts->pointer];
    float score;
    /* compute tf(f)*weight */
    if (freq < SCORE_CACHE_SIZE) {    /* check cache */
        score = ts->score_cache[freq];  /* cache hit */
    } else {
        /* cache miss */
        score = frt_sim_tf(self->similarity, (float)freq) * ts->weight_value;
    }
    /* normalize for field */
    score *= frt_sim_decode_norm(self->similarity, ts->norms[self->doc]);
    return score;
}

static bool tsc_next(FrtScorer *self) {
    TermScorer *ts = TSc(self);

    ts->pointer++;
    if (ts->pointer >= ts->pointer_max) {
        /* refill buffer */
        ts->pointer_max = ts->tde->read(ts->tde, ts->docs, ts->freqs, TDE_READ_SIZE);
        if (ts->pointer_max != 0) {
            ts->pointer = 0;
        } else {
            return false;
        }
    }
    self->doc = ts->docs[ts->pointer];
    return true;
}

static bool tsc_skip_to(FrtScorer *self, int doc_num) {
    TermScorer *ts = TSc(self);
    FrtTermDocEnum *tde = ts->tde;

    /* first scan in cache */
    while (++(ts->pointer) < ts->pointer_max) {
        if (ts->docs[ts->pointer] >= doc_num) {
            self->doc = ts->docs[ts->pointer];
            return true;
        }
    }

    /* not found in cache, seek underlying stream */
    if (tde->skip_to(tde, doc_num)) {
        ts->pointer_max = 1;
        ts->pointer = 0;
        ts->docs[0] = self->doc = tde->doc_num(tde);
        ts->freqs[0] = tde->freq(tde);
        return true;
    } else {
        return false;
    }
}

static FrtExplanation *tsc_explain(FrtScorer *self, int doc_num) {
    TermScorer *ts = TSc(self);
    FrtQuery *query = ts->weight->get_query(ts->weight);
    int tf = 0;

    tsc_skip_to(self, doc_num);
    if (self->doc == doc_num) {
        tf = ts->freqs[ts->pointer];
    }
    return frt_expl_new(frt_sim_tf(self->similarity, (float)tf),
                    "tf(term_freq(%s:%s)=%d)",
                    rb_id2name(TQ(query)->field), TQ(query)->term, tf);
}

static void tsc_destroy(FrtScorer *self) {
    TSc(self)->tde->close(TSc(self)->tde);
    frt_scorer_destroy_i(self);
}

static FrtScorer *tsc_new(FrtWeight *weight, FrtTermDocEnum *tde, frt_uchar *norms) {
    int i;
    FrtScorer *self            = frt_scorer_new(TermScorer, weight->similarity);
    TSc(self)->weight       = weight;
    TSc(self)->tde          = tde;
    TSc(self)->norms        = norms;
    TSc(self)->weight_value = weight->value;

    for (i = 0; i < SCORE_CACHE_SIZE; i++) {
        TSc(self)->score_cache[i]
            = frt_sim_tf(self->similarity, (float)i) * TSc(self)->weight_value;
    }

    self->score             = &tsc_score;
    self->next              = &tsc_next;
    self->skip_to           = &tsc_skip_to;
    self->explain           = &tsc_explain;
    self->destroy           = &tsc_destroy;
    return self;
}

/***************************************************************************
 *
 * TermWeight
 *
 ***************************************************************************/

static FrtScorer *tw_scorer(FrtWeight *self, FrtIndexReader *ir) {
    FrtTermQuery *tq = TQ(self->query);
    FrtTermDocEnum *tde = ir_term_docs_for(ir, tq->field, tq->term);
    /* ir_term_docs_for should always return a TermDocEnum */
    assert(NULL != tde);

    return tsc_new(self, tde, frt_ir_get_norms(ir, tq->field));
}

static FrtExplanation *tw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num) {
    FrtExplanation *qnorm_expl;
    FrtExplanation *field_expl;
    FrtScorer *scorer;
    FrtExplanation *tf_expl;
    frt_uchar *field_norms;
    float field_norm;
    FrtExplanation *field_norm_expl;
    char *query_str = self->query->to_s(self->query, (FrtSymbol)NULL);
    FrtTermQuery *tq = TQ(self->query);
    char *term = tq->term;
    FrtExplanation *expl = frt_expl_new(0.0, "weight(%s in %d), product of:", query_str, doc_num);
    /* We need two of these as it's included in both the query explanation
     * and the field explanation */
    FrtExplanation *idf_expl1 = frt_expl_new(self->idf, "idf(doc_freq=%d)", frt_ir_doc_freq(ir, tq->field, term));
    FrtExplanation *idf_expl2 = frt_expl_new(self->idf, "idf(doc_freq=%d)", frt_ir_doc_freq(ir, tq->field, term));
    /* explain query weight */
    FrtExplanation *query_expl = frt_expl_new(0.0, "query_weight(%s), product of:", query_str);
    free(query_str);
    if (self->query->boost != 1.0) {
        frt_expl_add_detail(query_expl, frt_expl_new(self->query->boost, "boost"));
    }
    frt_expl_add_detail(query_expl, idf_expl1);
    qnorm_expl = frt_expl_new(self->qnorm, "query_norm");
    frt_expl_add_detail(query_expl, qnorm_expl);
    query_expl->value = self->query->boost
        * idf_expl1->value * qnorm_expl->value;
    frt_expl_add_detail(expl, query_expl);
    /* explain field weight */
    field_expl = frt_expl_new(0.0, "field_weight(%s:%s in %d), product of:", rb_id2name(tq->field), term, doc_num);
    scorer = self->scorer(self, ir);
    tf_expl = scorer->explain(scorer, doc_num);
    scorer->destroy(scorer);
    frt_expl_add_detail(field_expl, tf_expl);
    frt_expl_add_detail(field_expl, idf_expl2);
    field_norms = frt_ir_get_norms(ir, tq->field);
    field_norm = (field_norms ? frt_sim_decode_norm(self->similarity, field_norms[doc_num]) : (float)0.0);
    field_norm_expl = frt_expl_new(field_norm, "field_norm(field=%s, doc=%d)", rb_id2name(tq->field), doc_num);
    frt_expl_add_detail(field_expl, field_norm_expl);
    field_expl->value = tf_expl->value * idf_expl2->value * field_norm_expl->value;
    /* combine them */
    if (query_expl->value == 1.0) {
        frt_expl_destroy(expl);
        return field_expl;
    } else {
        expl->value = (query_expl->value * field_expl->value);
        frt_expl_add_detail(expl, field_expl);
        return expl;
    }
}

static char *tw_to_s(FrtWeight *self) {
    return frt_strfmt("TermWeight(%f)", self->value);
}

static FrtWeight *tw_new(FrtQuery *query, FrtSearcher *searcher) {
    FrtWeight *self = w_new(FrtWeight, query);
    self->scorer    = &tw_scorer;
    self->explain   = &tw_explain;
    self->to_s      = &tw_to_s;

    self->similarity = query->get_similarity(query, searcher);
    self->idf = frt_sim_idf(self->similarity,
                        searcher->doc_freq(searcher,
                                           TQ(query)->field,
                                           TQ(query)->term),
                        searcher->max_doc(searcher)); /* compute idf */

    return self;
}

/***************************************************************************
 *
 * TermQuery
 *
 ***************************************************************************/

static void tq_destroy(FrtQuery *self) {
    free(TQ(self)->term);
    frt_q_destroy_i(self);
}

static char *tq_to_s(FrtQuery *self, FrtSymbol default_field) {
    const char *field_name = rb_id2name(TQ(self)->field);
    size_t flen = strlen(field_name);
    const char *term = TQ(self)->term;
    size_t tlen = strlen(term);
    char *buffer = FRT_ALLOC_N(char, 34 + flen + tlen);
    char *b = buffer;
    if (default_field != TQ(self)->field) {
        memcpy(b, field_name, sizeof(char) * flen);
        b[flen] = ':';
        b += flen + 1;
    }
    memcpy(b, term, tlen);
    b += tlen;
    *b = 0;
    if (self->boost != 1.0) {
        *b = '^';
        frt_dbl_to_s(b+1, self->boost);
    }
    return buffer;
}

static void tq_extract_terms(FrtQuery *self, FrtHashSet *terms) {
    frt_hs_add(terms, frt_term_new(TQ(self)->field, TQ(self)->term));
}

static unsigned long long tq_hash(FrtQuery *self) {
    return frt_str_hash(TQ(self)->term) ^ frt_str_hash(rb_id2name(TQ(self)->field));
}

static int tq_eq(FrtQuery *self, FrtQuery *o) {
    return (strcmp(TQ(self)->term, TQ(o)->term) == 0) && (TQ(self)->field == TQ(o)->field);
}

static FrtMatchVector *tq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv, FrtTermVector *tv) {
    if (tv->field == TQ(self)->field) {
        int i;
        FrtTVTerm *tv_term = frt_tv_get_tv_term(tv, TQ(self)->term);
        if (tv_term) {
            for (i = 0; i < tv_term->freq; i++) {
                int pos = tv_term->positions[i];
                frt_matchv_add(mv, pos, pos);
            }
        }
    }
    return mv;
}

FrtQuery *frt_tq_alloc(void) {
    return frt_q_new(FrtTermQuery);
}

FrtQuery *frt_tq_init(FrtQuery *self, FrtSymbol field, const char *term) {
    TQ(self)->field         = field;
    TQ(self)->term          = frt_estrdup(term);
    self->type              = TERM_QUERY;
    self->extract_terms     = &tq_extract_terms;
    self->to_s              = &tq_to_s;
    self->hash              = &tq_hash;
    self->eq                = &tq_eq;

    self->destroy_i         = &tq_destroy;
    self->create_weight_i   = &tw_new;
    self->get_matchv_i      = &tq_get_matchv_i;

    return self;
}

FrtQuery *frt_tq_new(FrtSymbol field, const char *term) {
    FrtQuery *self          = frt_tq_alloc();
    return frt_tq_init(self, field, term);
}
