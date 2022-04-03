#include "frt_search.h"
#include <string.h>

/***************************************************************************
 *
 * ConstantScoreScorer
 *
 ***************************************************************************/

#define CScQ(query) ((FrtConstantScoreQuery *)(query))
#define CScSc(scorer) ((ConstantScoreScorer *)(scorer))

typedef struct ConstantScoreScorer {
    FrtScorer    super;
    FrtBitVector *bv;
    float        score;
} ConstantScoreScorer;

static float cssc_score(FrtScorer *self) {
    return CScSc(self)->score;
}

static bool cssc_next(FrtScorer *self) {
    return ((self->doc = frt_bv_scan_next(CScSc(self)->bv)) >= 0);
}

static bool cssc_skip_to(FrtScorer *self, int doc_num) {
    return ((self->doc = frt_bv_scan_next_from(CScSc(self)->bv, doc_num)) >= 0);
}

static FrtExplanation *cssc_explain(FrtScorer *self, int doc_num) {
    (void)self; (void)doc_num;
    return frt_expl_new(1.0, "ConstantScoreScorer");
}

static FrtScorer *cssc_new(FrtWeight *weight, FrtIndexReader *ir) {
    FrtScorer *self    = frt_scorer_new(ConstantScoreScorer, weight->similarity);
    FrtFilter *filter  = CScQ(weight->query)->filter;

    CScSc(self)->score  = weight->value;
    CScSc(self)->bv     = frt_filt_get_bv(filter, ir);

    self->score     = &cssc_score;
    self->next      = &cssc_next;
    self->skip_to   = &cssc_skip_to;
    self->explain   = &cssc_explain;
    self->destroy   = &frt_scorer_destroy_i;
    return self;
}

/***************************************************************************
 *
 * ConstantScoreWeight
 *
 ***************************************************************************/

static char *csw_to_s(FrtWeight *self) {
    return frt_strfmt("ConstantScoreWeight(%f)", self->value);
}

static FrtExplanation *csw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num) {
    FrtFilter *filter = CScQ(self->query)->filter;
    FrtExplanation *expl;
    char *filter_str = filter->to_s(filter);
    FrtBitVector *bv = frt_filt_get_bv(filter, ir);

    if (frt_bv_get(bv, doc_num)) {
        expl = frt_expl_new(self->value, "ConstantScoreQuery(%s), product of:", filter_str);
        frt_expl_add_detail(expl, frt_expl_new(self->query->boost, "boost"));
        frt_expl_add_detail(expl, frt_expl_new(self->qnorm, "query_norm"));
    } else {
        expl = frt_expl_new(self->value, "ConstantScoreQuery(%s), does not match id %d", filter_str, doc_num);
    }
    free(filter_str);
    return expl;
}

static FrtWeight *csw_new(FrtQuery *query, FrtSearcher *searcher) {
    FrtWeight *self     = w_new(FrtWeight, query);

    self->scorer        = &cssc_new;
    self->explain       = &csw_explain;
    self->to_s          = &csw_to_s;

    self->similarity    = query->get_similarity(query, searcher);
    self->idf           = 1.0f;

    return self;
}

/***************************************************************************
 *
 * ConstantScoreQuery
 *
 ***************************************************************************/

static char *csq_to_s(FrtQuery *self, FrtSymbol default_field) {
    FrtFilter *filter = CScQ(self)->filter;
    char *filter_str = filter->to_s(filter);
    char *buffer;
    (void)default_field;
    if (self->boost == 1.0) {
        buffer = frt_strfmt("ConstantScore(%s)", filter_str);
    } else {
        buffer = frt_strfmt("ConstantScore(%s)^%f", filter_str, self->boost);
    }
    free(filter_str);
    return buffer;;
}

static void csq_destroy(FrtQuery *self) {
    frt_filt_deref(CScQ(self)->filter);
    frt_q_destroy_i(self);
}

static unsigned long long csq_hash(FrtQuery *self) {
    return frt_filt_hash(CScQ(self)->filter);
}

static int csq_eq(FrtQuery *self, FrtQuery *o) {
    return frt_filt_eq(CScQ(self)->filter, CScQ(o)->filter);
}

FrtQuery *frt_csq_alloc(void) {
    return frt_q_new(FrtConstantScoreQuery);
}

FrtQuery *frt_csq_init_nr(FrtQuery *self, FrtFilter *filter) {
    CScQ(self)->filter      = filter;

    self->type              = CONSTANT_QUERY;
    self->to_s              = &csq_to_s;
    self->hash              = &csq_hash;
    self->eq                = &csq_eq;
    self->destroy_i         = &csq_destroy;
    self->create_weight_i   = &csw_new;

    return self;
}

FrtQuery *frt_csq_new_nr(FrtFilter *filter) {
    FrtQuery *self = frt_csq_alloc();
    return frt_csq_init_nr(self, filter);
}

FrtQuery *frt_csq_init(FrtQuery *self, FrtFilter *filter) {
    FRT_REF(filter);
    return frt_csq_init_nr(self, filter);
}

FrtQuery *frt_csq_new(FrtFilter *filter) {
    FRT_REF(filter);
    return frt_csq_new_nr(filter);
}
