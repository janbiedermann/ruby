#include "frt_search.h"
#include <string.h>

/***************************************************************************
 *
 * MatchAllScorer
 *
 ***************************************************************************/

#define MASc(scorer) ((MatchAllScorer *)(scorer))

typedef struct MatchAllScorer
{
    FrtScorer          super;
    FrtIndexReader    *ir;
    int             max_doc;
    float           score;
} MatchAllScorer;

static float masc_score(FrtScorer *self)
{
    return MASc(self)->score;
}

static bool masc_next(FrtScorer *self)
{
    while (self->doc < (MASc(self)->max_doc - 1)) {
        self->doc++;
        if (!MASc(self)->ir->is_deleted(MASc(self)->ir, self->doc)) {
            return true;
        }
    }
    return false;
}

static bool masc_skip_to(FrtScorer *self, int doc_num)
{
    self->doc = doc_num - 1;
    return masc_next(self);
}

static FrtExplanation *masc_explain(FrtScorer *self, int doc_num)
{
    (void)self;
    (void)doc_num;
    return frt_expl_new(1.0, "MatchAllScorer");
}

static FrtScorer *masc_new(FrtWeight *weight, FrtIndexReader *ir)
{
    FrtScorer *self        = frt_scorer_new(MatchAllScorer, weight->similarity);

    MASc(self)->ir      = ir;
    MASc(self)->max_doc = ir->max_doc(ir);
    MASc(self)->score   = weight->value;

    self->doc           = -1;
    self->score         = &masc_score;
    self->next          = &masc_next;
    self->skip_to       = &masc_skip_to;
    self->explain       = &masc_explain;
    self->destroy       = &frt_scorer_destroy_i;

    return self;
}

/***************************************************************************
 *
 * FrtWeight
 *
 ***************************************************************************/

static char *maw_to_s(FrtWeight *self)
{
    return frt_strfmt("MatchAllWeight(%f)", self->value);
}

static FrtExplanation *maw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num)
{
    FrtExplanation *expl;
    if (!ir->is_deleted(ir, doc_num)) {
        expl = frt_expl_new(self->value, "MatchAllQuery: product of:");
        frt_expl_add_detail(expl, frt_expl_new(self->query->boost, "boost"));
        frt_expl_add_detail(expl, frt_expl_new(self->qnorm, "query_norm"));
    } else {
        expl = frt_expl_new(self->value, "MatchAllQuery: doc %d was deleted", doc_num);
    }

    return expl;
}

static FrtWeight *maw_new(FrtQuery *query, FrtSearcher *searcher)
{
    FrtWeight *self        = w_new(FrtWeight, query);

    self->scorer        = &masc_new;
    self->explain       = &maw_explain;
    self->to_s          = &maw_to_s;

    self->similarity    = query->get_similarity(query, searcher);
    self->idf           = 1.0f;

    return self;
}

/***************************************************************************
 *
 * MatchAllQuery
 *
 ***************************************************************************/

static char *maq_to_s(FrtQuery *self, FrtSymbol default_field)
{
    (void)default_field;
    if (self->boost == 1.0) {
        return frt_estrdup("*");
    } else {
        return frt_strfmt("*^%f", self->boost);
    }
}

static unsigned long long maq_hash(FrtQuery *self)
{
    (void)self;
    return 0;
}

static int maq_eq(FrtQuery *self, FrtQuery *o)
{
    (void)self; (void)o;
    return true;
}

FrtQuery *frt_maq_new()
{
    FrtQuery *self = frt_q_new(FrtQuery);

    self->type = MATCH_ALL_QUERY;
    self->to_s = &maq_to_s;
    self->hash = &maq_hash;
    self->eq = &maq_eq;
    self->destroy_i = &frt_q_destroy_i;
    self->create_weight_i = &maw_new;

    return self;
}

