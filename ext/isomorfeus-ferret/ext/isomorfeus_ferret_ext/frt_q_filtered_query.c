#include "frt_search.h"
#include <string.h>

/***************************************************************************
 *
 * FilteredQueryScorer
 *
 ***************************************************************************/

#define FQSc(scorer) ((FilteredQueryScorer *)(scorer))
#define FQQ(query) ((FrtFilteredQuery *)(query))

typedef struct FilteredQueryScorer
{
    FrtScorer      super;
    FrtScorer     *sub_scorer;
    FrtBitVector  *bv;
} FilteredQueryScorer;

static float fqsc_score(FrtScorer *self)
{
    FrtScorer *sub_sc = FQSc(self)->sub_scorer;
    return sub_sc->score(sub_sc);
}

static bool fqsc_next(FrtScorer *self)
{
    FrtScorer *sub_sc = FQSc(self)->sub_scorer;
    FrtBitVector *bv = FQSc(self)->bv;
    while (sub_sc->next(sub_sc)) {
        self->doc = sub_sc->doc;
        if (frt_bv_get(bv, self->doc)) return true;
    }
    return false;
}

static bool fqsc_skip_to(FrtScorer *self, int doc_num)
{
    FrtScorer *sub_sc = FQSc(self)->sub_scorer;
    FrtBitVector *bv = FQSc(self)->bv;
    if (sub_sc->skip_to(sub_sc, doc_num)) {
        do {
            self->doc = sub_sc->doc;
            if (frt_bv_get(bv, self->doc)) {
                return true;
            }
        } while (sub_sc->next(sub_sc));
    }
    return false;
}

static FrtExplanation *fqsc_explain(FrtScorer *self, int doc_num)
{
    FrtScorer *sub_sc = FQSc(self)->sub_scorer;
    return sub_sc->explain(sub_sc, doc_num);
}

static void fqsc_destroy(FrtScorer *self)
{
    FilteredQueryScorer *fqsc = FQSc(self);
    fqsc->sub_scorer->destroy(fqsc->sub_scorer);
    frt_scorer_destroy_i(self);
}

static FrtScorer *fqsc_new(FrtScorer *scorer, FrtBitVector *bv, FrtSimilarity *sim)
{
    FrtScorer *self            = frt_scorer_new(FilteredQueryScorer, sim);

    FQSc(self)->sub_scorer  = scorer;
    FQSc(self)->bv          = bv;

    self->score   = &fqsc_score;
    self->next    = &fqsc_next;
    self->skip_to = &fqsc_skip_to;
    self->explain = &fqsc_explain;
    self->destroy = &fqsc_destroy;

    return self;
}

/***************************************************************************
 *
 * FrtWeight
 *
 ***************************************************************************/

#define FQW(weight) ((FilteredQueryWeight *)(weight))
typedef struct FilteredQueryWeight
{
    FrtWeight  super;
    FrtWeight *sub_weight;
} FilteredQueryWeight;

static char *fqw_to_s(FrtWeight *self)
{
    return frt_strfmt("FilteredQueryWeight(%f)", self->value);
}

static float fqw_sum_of_squared_weights(FrtWeight *self)
{
    FrtWeight *sub_weight = FQW(self)->sub_weight;
    return sub_weight->sum_of_squared_weights(sub_weight);
}

static void fqw_normalize(FrtWeight *self, float normalization_factor)
{
    FrtWeight *sub_weight = FQW(self)->sub_weight;
    sub_weight->normalize(sub_weight, normalization_factor);
}

static float fqw_get_value(FrtWeight *self)
{
    FrtWeight *sub_weight = FQW(self)->sub_weight;
    return sub_weight->get_value(sub_weight);
}

static FrtExplanation *fqw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num)
{
    FrtWeight *sub_weight = FQW(self)->sub_weight;
    return sub_weight->explain(sub_weight, ir, doc_num);
}

static FrtScorer *fqw_scorer(FrtWeight *self, FrtIndexReader *ir)
{
    FrtWeight *sub_weight = FQW(self)->sub_weight;
    FrtScorer *scorer = sub_weight->scorer(sub_weight, ir);
    FrtFilter *filter = FQQ(self->query)->filter;

    return fqsc_new(scorer, frt_filt_get_bv(filter, ir), self->similarity);
}

static void fqw_destroy(FrtWeight *self)
{
    FrtWeight *sub_weight = FQW(self)->sub_weight;
    sub_weight->destroy(sub_weight);
    frt_w_destroy(self);
}

static FrtWeight *fqw_new(FrtQuery *query, FrtWeight *sub_weight, FrtSimilarity *sim)
{
    FrtWeight *self = w_new(FilteredQueryWeight, query);

    FQW(self)->sub_weight           = sub_weight;

    self->get_value                 = &fqw_get_value;
    self->normalize                 = &fqw_normalize;
    self->scorer                    = &fqw_scorer;
    self->explain                   = &fqw_explain;
    self->to_s                      = &fqw_to_s;
    self->destroy                   = &fqw_destroy;
    self->sum_of_squared_weights    = &fqw_sum_of_squared_weights;

    self->similarity                = sim;
    self->idf                       = 1.0f;
    self->value                     = sub_weight->value;

    return self;
}

/***************************************************************************
 *
 * FilteredQuery
 *
 ***************************************************************************/

static char *fq_to_s(FrtQuery *self, FrtSymbol default_field)
{
    FrtFilteredQuery *fq = FQQ(self);
    char *filter_str = fq->filter->to_s(fq->filter);
    char *query_str = fq->query->to_s(fq->query, default_field);
    char *buffer;
    if (self->boost == 1.0) {
        buffer = frt_strfmt("FilteredQuery(query:%s, filter:%s)", query_str, filter_str);
    } else {
        buffer = frt_strfmt("FilteredQuery(query:%s, filter:%s)^%f", query_str, filter_str, self->boost);
    }
    free(filter_str);
    free(query_str);
    return buffer;;
}

static void fq_destroy(FrtQuery *self)
{
    frt_filt_deref(FQQ(self)->filter);
    frt_q_deref(FQQ(self)->query);
    frt_q_destroy_i(self);
}

static FrtWeight *fq_new_weight(FrtQuery *self, FrtSearcher *searcher)
{
    FrtQuery *sub_query = FQQ(self)->query;
    return fqw_new(self, frt_q_weight(sub_query, searcher),
                      searcher->similarity);
}

FrtQuery *frt_fq_new(FrtQuery *query, FrtFilter *filter)
{
    FrtQuery *self = frt_q_new(FrtFilteredQuery);

    FQQ(self)->query        = query;
    FQQ(self)->filter       = filter;

    self->type              = FILTERED_QUERY;
    self->to_s              = &fq_to_s;
    self->destroy_i         = &fq_destroy;
    self->create_weight_i   = &fq_new_weight;

    return self;
}
