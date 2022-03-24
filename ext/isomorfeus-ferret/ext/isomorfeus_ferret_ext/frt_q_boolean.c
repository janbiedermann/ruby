#include <string.h>
#include "frt_search.h"
#include "frt_array.h"

#define BQ(query) ((FrtBooleanQuery *)(query))
#define BW(weight) ((BooleanWeight *)(weight))

/***************************************************************************
 *
 * BooleanScorer
 *
 ***************************************************************************/

/***************************************************************************
 * Coordinator
 ***************************************************************************/

typedef struct Coordinator
{
    int max_coord;
    float *coord_factors;
    FrtSimilarity *similarity;
    int num_matches;
} Coordinator;

static Coordinator *coord_new(FrtSimilarity *similarity)
{
    Coordinator *self = FRT_ALLOC_AND_ZERO(Coordinator);
    self->similarity = similarity;
    return self;
}

static Coordinator *coord_init(Coordinator *self)
{
    int i;
    self->coord_factors = FRT_ALLOC_N(float, self->max_coord + 1);

    for (i = 0; i <= self->max_coord; i++) {
        self->coord_factors[i]
            = frt_sim_coord(self->similarity, i, self->max_coord);
    }

    return self;
}

/***************************************************************************
 * DisjunctionSumScorer
 ***************************************************************************/

#define DSSc(scorer) ((DisjunctionSumScorer *)(scorer))

typedef struct DisjunctionSumScorer
{
    FrtScorer          super;
    float           cum_score;
    int             num_matches;
    int             min_num_matches;
    FrtScorer        **sub_scorers;
    int             ss_cnt;
    FrtPriorityQueue  *scorer_queue;
    Coordinator    *coordinator;
} DisjunctionSumScorer;

static float dssc_score(FrtScorer *self)
{
    return DSSc(self)->cum_score;
}

static void dssc_init_scorer_queue(DisjunctionSumScorer *dssc)
{
    int i;
    FrtScorer *sub_scorer;
    FrtPriorityQueue *pq = dssc->scorer_queue
        = frt_pq_new(dssc->ss_cnt, (frt_lt_ft)&frt_scorer_doc_less_than, NULL);

    for (i = 0; i < dssc->ss_cnt; i++) {
        sub_scorer = dssc->sub_scorers[i];
        if (sub_scorer->next(sub_scorer)) {
            frt_pq_insert(pq, sub_scorer);
        }
    }
}

static bool dssc_advance_after_current(FrtScorer *self)
{
    DisjunctionSumScorer *dssc = DSSc(self);
    FrtPriorityQueue *scorer_queue = dssc->scorer_queue;

    /* repeat until minimum number of matches is found */
    while (true) {
        FrtScorer *top = (FrtScorer *)frt_pq_top(scorer_queue);
        self->doc = top->doc;
        dssc->cum_score = top->score(top);
        dssc->num_matches = 1;
        /* Until all sub-scorers are after self->doc */
        while (true) {
            if (top->next(top)) {
                frt_pq_down(scorer_queue);
            }
            else {
                frt_pq_pop(scorer_queue);
                if (scorer_queue->size
                    < (dssc->min_num_matches - dssc->num_matches)) {
                    /* Not enough subscorers left for a match on this
                     * document, also no more chance of any further match */
                    return false;
                }
                if (scorer_queue->size == 0) {
                    /* nothing more to advance, check for last match. */
                    break;
                }
            }
            top = (FrtScorer *)frt_pq_top(scorer_queue);
            if (top->doc != self->doc) {
                /* All remaining subscorers are after self->doc */
                break;
            }
            else {
                dssc->cum_score += top->score(top);
                dssc->num_matches++;
            }
        }

        if (dssc->num_matches >= dssc->min_num_matches) {
            return true;
        }
        else if (scorer_queue->size < dssc->min_num_matches) {
            return false;
        }
    }
}

static bool dssc_next(FrtScorer *self)
{
    if (DSSc(self)->scorer_queue == NULL) {
        dssc_init_scorer_queue(DSSc(self));
    }

    if (DSSc(self)->scorer_queue->size < DSSc(self)->min_num_matches) {
        return false;
    }
    else {
        return dssc_advance_after_current(self);
    }
}

static bool dssc_skip_to(FrtScorer *self, int doc_num)
{
    DisjunctionSumScorer *dssc = DSSc(self);
    FrtPriorityQueue *scorer_queue = dssc->scorer_queue;

    if (scorer_queue == NULL) {
        dssc_init_scorer_queue(dssc);
        scorer_queue = dssc->scorer_queue;
    }

    if (scorer_queue->size < dssc->min_num_matches) {
        return false;
    }
    if (doc_num <= self->doc) {
        doc_num = self->doc + 1;
    }
    while (true) {
        FrtScorer *top = (FrtScorer *)frt_pq_top(scorer_queue);
        if (top->doc >= doc_num) {
            return dssc_advance_after_current(self);
        }
        else if (top->skip_to(top, doc_num)) {
            frt_pq_down(scorer_queue);
        }
        else {
            frt_pq_pop(scorer_queue);
            if (scorer_queue->size < dssc->min_num_matches) {
                return false;
            }
        }
    }
}

static FrtExplanation *dssc_explain(FrtScorer *self, int doc_num)
{
    int i;
    DisjunctionSumScorer *dssc = DSSc(self);
    FrtScorer *sub_scorer;
    FrtExplanation *e
        = frt_expl_new(0.0, "At least %d of:", dssc->min_num_matches);
    for (i = 0; i < dssc->ss_cnt; i++) {
        sub_scorer = dssc->sub_scorers[i];
        frt_expl_add_detail(e, sub_scorer->explain(sub_scorer, doc_num));
    }
    return e;
}

static void dssc_destroy(FrtScorer *self)
{
    DisjunctionSumScorer *dssc = DSSc(self);
    int i;
    for (i = 0; i < dssc->ss_cnt; i++) {
        dssc->sub_scorers[i]->destroy(dssc->sub_scorers[i]);
    }
    if (dssc->scorer_queue) {
        frt_pq_destroy(dssc->scorer_queue);
    }
    frt_scorer_destroy_i(self);
}

static FrtScorer *disjunction_sum_scorer_new(FrtScorer **sub_scorers, int ss_cnt,
                                          int min_num_matches)
{
    FrtScorer *self = frt_scorer_new(DisjunctionSumScorer, NULL);
    DSSc(self)->ss_cnt = ss_cnt;

    /* The document number of the current match */
    self->doc = -1;
    DSSc(self)->cum_score = -1.0;

    /* The number of subscorers that provide the current match. */
    DSSc(self)->num_matches = -1;
    DSSc(self)->coordinator = NULL;

#ifdef DEBUG
    if (min_num_matches <= 0) {
        FRT_RAISE(FRT_ARG_ERROR, "The min_num_matches value <%d> should not be less "
              "than 0\n", min_num_matches);
    }
    if (ss_cnt <= 1) {
        FRT_RAISE(FRT_ARG_ERROR, "There should be at least 2 sub_scorers in a "
              "DiscjunctionSumScorer. <%d> is not enough", ss_cnt);
    }
#endif

    DSSc(self)->min_num_matches = min_num_matches;
    DSSc(self)->sub_scorers     = sub_scorers;
    DSSc(self)->scorer_queue    = NULL;

    self->score   = &dssc_score;
    self->next    = &dssc_next;
    self->skip_to = &dssc_skip_to;
    self->explain = &dssc_explain;
    self->destroy = &dssc_destroy;

    return self;
}

static float cdssc_score(FrtScorer *self)
{
    DSSc(self)->coordinator->num_matches += DSSc(self)->num_matches;
    return DSSc(self)->cum_score;
}

static FrtScorer *counting_disjunction_sum_scorer_new(
    Coordinator *coordinator, FrtScorer **sub_scorers, int ss_cnt,
    int min_num_matches)
{
    FrtScorer *self = disjunction_sum_scorer_new(sub_scorers, ss_cnt,
                                              min_num_matches);
    DSSc(self)->coordinator = coordinator;
    self->score = &cdssc_score;
    return self;
}

/***************************************************************************
 * ConjunctionScorer
 ***************************************************************************/

#define CSc(scorer) ((ConjunctionScorer *)(scorer))

typedef struct ConjunctionScorer
{
    FrtScorer          super;
    bool            first_time : 1;
    bool            more : 1;
    float           coord;
    FrtScorer        **sub_scorers;
    int             ss_cnt;
    int             first_idx;
    Coordinator    *coordinator;
    int             last_scored_doc;
} ConjunctionScorer;

static void csc_sort_scorers(ConjunctionScorer *csc)
{
    int i;
    FrtScorer *current = csc->sub_scorers[0], *previous;
    for (i = 1; i < csc->ss_cnt; i++) {
        previous = current;
        current = csc->sub_scorers[i];
        if (previous->doc > current->doc) {
            if (!current->skip_to(current, previous->doc)) {
                csc->more = false;
                return;
            }
        }
    }
    /*qsort(csc->sub_scorers, csc->ss_cnt, sizeof(FrtScorer *), &frt_scorer_doc_cmp);*/
    csc->first_idx = 0;
}

static void csc_init(FrtScorer *self, bool init_scorers)
{
    ConjunctionScorer *csc = CSc(self);
    const int sub_sc_cnt = csc->ss_cnt;

    /* compute coord factor */
    csc->coord = frt_sim_coord(self->similarity, sub_sc_cnt, sub_sc_cnt);

    csc->more = (sub_sc_cnt > 0);

    if (init_scorers) {
        int i;
        /* move each scorer to its first entry */
        for (i = 0; i < sub_sc_cnt; i++) {
            FrtScorer *sub_scorer = csc->sub_scorers[i];
            if (!csc->more) {
                break;
            }
            csc->more = sub_scorer->next(sub_scorer);
        }
        if (csc->more) {
            csc_sort_scorers(csc);
        }
    }

    csc->first_time = false;
}

static float csc_score(FrtScorer *self)
{
    ConjunctionScorer *csc = CSc(self);
    const int sub_sc_cnt = csc->ss_cnt;
    float score = 0.0f; /* sum scores */
    int i;
    for (i = 0; i < sub_sc_cnt; i++) {
        FrtScorer *sub_scorer = csc->sub_scorers[i];
        score += sub_scorer->score(sub_scorer);
    }
    score *= csc->coord;
    return score;
}

static bool csc_do_next(FrtScorer *self)
{
    ConjunctionScorer *csc = CSc(self);
    const int sub_sc_cnt = csc->ss_cnt;
    int first_idx = csc->first_idx;
    FrtScorer *first_sc = csc->sub_scorers[first_idx];
    FrtScorer *last_sc = csc->sub_scorers[FRT_PREV_NUM(first_idx, sub_sc_cnt)];

    /* skip to doc with all clauses */
    while (csc->more && (first_sc->doc < last_sc->doc)) {
        /* skip first upto last */
        csc->more = first_sc->skip_to(first_sc, last_sc->doc);
        /* move first to last */
        last_sc = first_sc;
        first_idx = FRT_NEXT_NUM(first_idx, sub_sc_cnt);
        first_sc = csc->sub_scorers[first_idx];
    }
    self->doc = first_sc->doc;
    csc->first_idx = first_idx;
    return csc->more;
}

static bool csc_next(FrtScorer *self)
{
    ConjunctionScorer *csc = CSc(self);
    if (csc->first_time) {
        csc_init(self, true);
    }
    else if (csc->more) {
        /* trigger further scanning */
        const int last_idx = FRT_PREV_NUM(csc->first_idx, csc->ss_cnt);
        FrtScorer *sub_scorer = csc->sub_scorers[last_idx];
        csc->more = sub_scorer->next(sub_scorer);
    }
    return csc_do_next(self);
}

static bool csc_skip_to(FrtScorer *self, int doc_num)
{
    ConjunctionScorer *csc = CSc(self);
    const int sub_sc_cnt = csc->ss_cnt;
    int i;
    bool more = csc->more;

    if (csc->first_time) {
        csc_init(self, false);
    }

    for (i = 0; i < sub_sc_cnt; i++) {
        if (!more) {
            break;
        }
        else {
            FrtScorer *sub_scorer = csc->sub_scorers[i];
            more = sub_scorer->skip_to(sub_scorer, doc_num);
        }
    }
    if (more) {
        /* resort the scorers */
        csc_sort_scorers(csc);
    }

    csc->more = more;
    return csc_do_next(self);
}

static void csc_destroy(FrtScorer *self)
{
    ConjunctionScorer *csc = CSc(self);
    const int sub_sc_cnt = csc->ss_cnt;
    int i;
    for (i = 0; i < sub_sc_cnt; i++) {
        csc->sub_scorers[i]->destroy(csc->sub_scorers[i]);
    }
    free(csc->sub_scorers);
    frt_scorer_destroy_i(self);
}

static FrtScorer *conjunction_scorer_new(FrtSimilarity *similarity)
{
    FrtScorer *self = frt_scorer_new(ConjunctionScorer, similarity);

    CSc(self)->first_time   = true;
    CSc(self)->more         = true;
    CSc(self)->coordinator  = NULL;

    self->score             = &csc_score;
    self->next              = &csc_next;
    self->skip_to           = &csc_skip_to;
    self->destroy           = &csc_destroy;

    return self;
}

static float ccsc_score(FrtScorer *self)
{
    ConjunctionScorer *csc = CSc(self);

    int doc;
    if ((doc = self->doc) > csc->last_scored_doc) {
        csc->last_scored_doc = doc;
        csc->coordinator->num_matches += csc->ss_cnt;
    }

    return csc_score(self);
}

static FrtScorer *counting_conjunction_sum_scorer_new(
    Coordinator *coordinator, FrtScorer **sub_scorers, int ss_cnt)
{
    FrtScorer *self = conjunction_scorer_new(frt_sim_create_default());
    ConjunctionScorer *csc = CSc(self);
    csc->coordinator = coordinator;
    csc->last_scored_doc = -1;
    csc->sub_scorers = FRT_ALLOC_N(FrtScorer *, ss_cnt);
    memcpy(csc->sub_scorers, sub_scorers, sizeof(FrtScorer *) * ss_cnt);
    csc->ss_cnt = ss_cnt;

    self->score = &ccsc_score;

    return self;
}

/***************************************************************************
 * SingleMatchScorer
 ***************************************************************************/

#define SMSc(scorer) ((SingleMatchScorer *)(scorer))

typedef struct SingleMatchScorer
{
    FrtScorer          super;
    Coordinator    *coordinator;
    FrtScorer         *scorer;
} SingleMatchScorer;


static float smsc_score(FrtScorer *self)
{
    SMSc(self)->coordinator->num_matches++;
    return SMSc(self)->scorer->score(SMSc(self)->scorer);
}

static bool smsc_next(FrtScorer *self)
{
    FrtScorer *scorer = SMSc(self)->scorer;
    if (scorer->next(scorer)) {
        self->doc = scorer->doc;
        return true;
    }
    return false;
}

static bool smsc_skip_to(FrtScorer *self, int doc_num)
{
    FrtScorer *scorer = SMSc(self)->scorer;
    if (scorer->skip_to(scorer, doc_num)) {
        self->doc = scorer->doc;
        return true;
    }
    return false;
}

static FrtExplanation *smsc_explain(FrtScorer *self, int doc_num)
{
    FrtScorer *scorer = SMSc(self)->scorer;
    return scorer->explain(scorer, doc_num);
}

static void smsc_destroy(FrtScorer *self)
{
    FrtScorer *scorer = SMSc(self)->scorer;
    scorer->destroy(scorer);
    frt_scorer_destroy_i(self);
}

static FrtScorer *single_match_scorer_new(Coordinator *coordinator,
                                       FrtScorer *scorer)
{
    FrtScorer *self = frt_scorer_new(SingleMatchScorer, scorer->similarity);
    SMSc(self)->coordinator = coordinator;
    SMSc(self)->scorer      = scorer;

    self->score             = &smsc_score;
    self->next              = &smsc_next;
    self->skip_to           = &smsc_skip_to;
    self->explain           = &smsc_explain;
    self->destroy           = &smsc_destroy;
    return self;
}

/***************************************************************************
 * ReqOptSumScorer
 ***************************************************************************/

#define ROSSc(scorer) ((ReqOptSumScorer *)(scorer))

typedef struct ReqOptSumScorer
{
    FrtScorer  super;
    FrtScorer *req_scorer;
    FrtScorer *opt_scorer;
    bool    first_time_opt;
} ReqOptSumScorer;

static float rossc_score(FrtScorer *self)
{
    ReqOptSumScorer *rossc = ROSSc(self);
    FrtScorer *req_scorer = rossc->req_scorer;
    FrtScorer *opt_scorer = rossc->opt_scorer;
    int cur_doc = req_scorer->doc;
    float req_score = req_scorer->score(req_scorer);

    if (rossc->first_time_opt) {
        rossc->first_time_opt = false;
        if (! opt_scorer->skip_to(opt_scorer, cur_doc)) {
            FRT_SCORER_NULLIFY(rossc->opt_scorer);
            return req_score;
        }
    }
    else if (opt_scorer == NULL) {
        return req_score;
    }
    else if ((opt_scorer->doc < cur_doc)
             && ! opt_scorer->skip_to(opt_scorer, cur_doc)) {
        FRT_SCORER_NULLIFY(rossc->opt_scorer);
        return req_score;
    }
    /* assert (@opt_scorer != nil) and (@opt_scorer.doc() >= cur_doc) */
    return (opt_scorer->doc == cur_doc)
        ? req_score + opt_scorer->score(opt_scorer)
        : req_score;
}

static bool rossc_next(FrtScorer *self)
{
    FrtScorer *req_scorer = ROSSc(self)->req_scorer;
    if (req_scorer->next(req_scorer)) {
        self->doc = req_scorer->doc;
        return true;
    }
    return false;
}

static bool rossc_skip_to(FrtScorer *self, int doc_num)
{
    FrtScorer *req_scorer = ROSSc(self)->req_scorer;
    if (req_scorer->skip_to(req_scorer, doc_num)) {
        self->doc = req_scorer->doc;
        return true;
    }
    return false;
}

static FrtExplanation *rossc_explain(FrtScorer *self, int doc_num)
{
    FrtScorer *req_scorer = ROSSc(self)->req_scorer;
    FrtScorer *opt_scorer = ROSSc(self)->opt_scorer;

    FrtExplanation *e = frt_expl_new(self->score(self),"required, optional:");
    frt_expl_add_detail(e, req_scorer->explain(req_scorer, doc_num));
    frt_expl_add_detail(e, opt_scorer->explain(opt_scorer, doc_num));
    return e;
}

static void rossc_destroy(FrtScorer *self)
{
    ReqOptSumScorer *rossc = ROSSc(self);
    if (rossc->req_scorer) {
        rossc->req_scorer->destroy(rossc->req_scorer);
    }
    if (rossc->opt_scorer) {
        rossc->opt_scorer->destroy(rossc->opt_scorer);
    }
    frt_scorer_destroy_i(self);
}


static FrtScorer *req_opt_sum_scorer_new(FrtScorer *req_scorer, FrtScorer *opt_scorer)
{
    FrtScorer *self = frt_scorer_new(ReqOptSumScorer, NULL);

    ROSSc(self)->req_scorer     = req_scorer;
    ROSSc(self)->opt_scorer     = opt_scorer;
    ROSSc(self)->first_time_opt = true;

    self->score   = &rossc_score;
    self->next    = &rossc_next;
    self->skip_to = &rossc_skip_to;
    self->explain = &rossc_explain;
    self->destroy = &rossc_destroy;

    return self;
}

/***************************************************************************
 * ReqExclScorer
 ***************************************************************************/

#define RXSc(scorer) ((ReqExclScorer *)(scorer))
typedef struct ReqExclScorer
{
    FrtScorer  super;
    FrtScorer *req_scorer;
    FrtScorer *excl_scorer;
    bool    first_time;
} ReqExclScorer;

static bool rxsc_to_non_excluded(FrtScorer *self)
{
    FrtScorer *req_scorer = RXSc(self)->req_scorer;
    FrtScorer *excl_scorer = RXSc(self)->excl_scorer;
    int excl_doc = excl_scorer->doc, req_doc;

    do {
        /* may be excluded */
        req_doc = req_scorer->doc;
        if (req_doc < excl_doc) {
            /* req_scorer advanced to before excl_scorer, ie. not excluded */
            self->doc = req_doc;
            return true;
        }
        else if (req_doc > excl_doc) {
            if (! excl_scorer->skip_to(excl_scorer, req_doc)) {
                /* emptied, no more exclusions */
                FRT_SCORER_NULLIFY(RXSc(self)->excl_scorer);
                self->doc = req_doc;
                return true;
            }
            excl_doc = excl_scorer->doc;
            if (excl_doc > req_doc) {
                self->doc = req_doc;
                return true; /* not excluded */
            }
        }
    } while (req_scorer->next(req_scorer));
    /* emptied, nothing left */
    FRT_SCORER_NULLIFY(RXSc(self)->req_scorer);
    return false;
}

static bool rxsc_next(FrtScorer *self)
{
    ReqExclScorer *rxsc = RXSc(self);
    FrtScorer *req_scorer = rxsc->req_scorer;
    FrtScorer *excl_scorer = rxsc->excl_scorer;

    if (rxsc->first_time) {
        if (! excl_scorer->next(excl_scorer)) {
            /* emptied at start */
            FRT_SCORER_NULLIFY(rxsc->excl_scorer);
            excl_scorer = NULL;
        }
        rxsc->first_time = false;
    }
    if (req_scorer == NULL) {
        return false;
    }
    if (! req_scorer->next(req_scorer)) {
        /* emptied, nothing left */
        FRT_SCORER_NULLIFY(rxsc->req_scorer);
        return false;
    }
    if (excl_scorer == NULL) {
        self->doc = req_scorer->doc;
        /* req_scorer->next() already returned true */
        return true;
    }
    return rxsc_to_non_excluded(self);
}

static bool rxsc_skip_to(FrtScorer *self, int doc_num)
{
    ReqExclScorer *rxsc = RXSc(self);
    FrtScorer *req_scorer = rxsc->req_scorer;
    FrtScorer *excl_scorer = rxsc->excl_scorer;

    if (rxsc->first_time) {
        rxsc->first_time = false;
        if (! excl_scorer->skip_to(excl_scorer, doc_num)) {
            /* emptied */
            FRT_SCORER_NULLIFY(rxsc->excl_scorer);
            excl_scorer = NULL;
        }
    }
    if (req_scorer == NULL) {
        return false;
    }
    if (excl_scorer == NULL) {
        if (req_scorer->skip_to(req_scorer, doc_num)) {
            self->doc = req_scorer->doc;
            return true;
        }
        return false;
    }
    if (! req_scorer->skip_to(req_scorer, doc_num)) {
        FRT_SCORER_NULLIFY(rxsc->req_scorer);
        return false;
    }
    return rxsc_to_non_excluded(self);
}

static float rxsc_score(FrtScorer *self)
{
    FrtScorer *req_scorer = RXSc(self)->req_scorer;
    return req_scorer->score(req_scorer);
}

static FrtExplanation *rxsc_explain(FrtScorer *self, int doc_num)
{
    ReqExclScorer *rxsc = RXSc(self);
    FrtScorer *req_scorer = rxsc->req_scorer;
    FrtScorer *excl_scorer = rxsc->excl_scorer;
    FrtExplanation *e;

    if (excl_scorer->skip_to(excl_scorer, doc_num)
        && excl_scorer->doc == doc_num) {
        e = frt_expl_new(0.0, "excluded:");
    }
    else {
        e = frt_expl_new(0.0, "not excluded:");
        frt_expl_add_detail(e, req_scorer->explain(req_scorer, doc_num));
    }
    return e;
}

static void rxsc_destroy(FrtScorer *self)
{
    ReqExclScorer *rxsc = RXSc(self);
    if (rxsc->req_scorer) {
        rxsc->req_scorer->destroy(rxsc->req_scorer);
    }
    if (rxsc->excl_scorer) {
        rxsc->excl_scorer->destroy(rxsc->excl_scorer);
    }
    frt_scorer_destroy_i(self);
}

static FrtScorer *req_excl_scorer_new(FrtScorer *req_scorer, FrtScorer *excl_scorer)
{
    FrtScorer *self            = frt_scorer_new(ReqExclScorer, NULL);
    RXSc(self)->req_scorer  = req_scorer;
    RXSc(self)->excl_scorer = excl_scorer;
    RXSc(self)->first_time  = true;

    self->score             = &rxsc_score;
    self->next              = &rxsc_next;
    self->skip_to           = &rxsc_skip_to;
    self->explain           = &rxsc_explain;
    self->destroy           = &rxsc_destroy;

    return self;
}

/***************************************************************************
 * NonMatchScorer
 ***************************************************************************/

static float nmsc_score(FrtScorer *self)
{
    (void)self;
    return 0.0;
}

static bool nmsc_next(FrtScorer *self)
{
    (void)self;
    return false;
}

static bool nmsc_skip_to(FrtScorer *self, int doc_num)
{
    (void)self; (void)doc_num;
    return false;
}

static FrtExplanation *nmsc_explain(FrtScorer *self, int doc_num)
{
    (void)self; (void)doc_num;
    return frt_expl_new(0.0, "No documents matched");
}

static FrtScorer *non_matching_scorer_new()
{
    FrtScorer *self    = frt_scorer_new(FrtScorer, NULL);
    self->score     = &nmsc_score;
    self->next      = &nmsc_next;
    self->skip_to   = &nmsc_skip_to;
    self->explain   = &nmsc_explain;

    return self;
}

/***************************************************************************
 * BooleanScorer
 ***************************************************************************/

#define BSc(scorer) ((BooleanScorer *)(scorer))
typedef struct BooleanScorer
{
    FrtScorer          super;
    FrtScorer        **required_scorers;
    int             rs_cnt;
    int             rs_capa;
    FrtScorer        **optional_scorers;
    int             os_cnt;
    int             os_capa;
    FrtScorer        **prohibited_scorers;
    int             ps_cnt;
    int             ps_capa;
    FrtScorer         *counting_sum_scorer;
    Coordinator    *coordinator;
} BooleanScorer;

static FrtScorer *counting_sum_scorer_create3(BooleanScorer *bsc,
                                           FrtScorer *req_scorer,
                                           FrtScorer *opt_scorer)
{
    if (bsc->ps_cnt == 0) {
        /* no prohibited */
        return req_opt_sum_scorer_new(req_scorer, opt_scorer);
    }
    else if (bsc->ps_cnt == 1) {
        /* 1 prohibited */
        return req_opt_sum_scorer_new(
            req_excl_scorer_new(req_scorer, bsc->prohibited_scorers[0]),
            opt_scorer);
    }
    else {
        /* more prohibited */
        return req_opt_sum_scorer_new(
            req_excl_scorer_new(
                req_scorer,
                disjunction_sum_scorer_new(bsc->prohibited_scorers,
                                              bsc->ps_cnt, 1)),
            opt_scorer);
    }
}

static FrtScorer *counting_sum_scorer_create2(BooleanScorer *bsc,
                                           FrtScorer *req_scorer,
                                           FrtScorer **optional_scorers,
                                           int os_cnt)
{
    if (os_cnt == 0) {
        if (bsc->ps_cnt == 0) {
            return req_scorer;
        }
        else if (bsc->ps_cnt == 1) {
            return req_excl_scorer_new(req_scorer,
                                       bsc->prohibited_scorers[0]);
        }
        else {
            /* no optional, more than 1 prohibited */
            return req_excl_scorer_new(
                req_scorer,
                disjunction_sum_scorer_new(bsc->prohibited_scorers,
                                           bsc->ps_cnt, 1));
        }
    }
    else if (os_cnt == 1) {
        return counting_sum_scorer_create3(
            bsc,
            req_scorer,
            single_match_scorer_new(bsc->coordinator, optional_scorers[0]));
    }
    else {
        /* more optional */
        return counting_sum_scorer_create3(
            bsc,
            req_scorer,
            counting_disjunction_sum_scorer_new(bsc->coordinator,
                                                optional_scorers, os_cnt, 1));
    }
}

static FrtScorer *counting_sum_scorer_create(BooleanScorer *bsc)
{
    if (bsc->rs_cnt == 0) {
        if (bsc->os_cnt == 0) {
            int i;
            /* only prohibited scorers so return non_matching scorer */
            for (i = 0; i < bsc->ps_cnt; i++) {
                bsc->prohibited_scorers[i]->destroy(
                    bsc->prohibited_scorers[i]);
            }
            return non_matching_scorer_new();
        }
        else if (bsc->os_cnt == 1) {
            /* the only optional scorer is required */
            return counting_sum_scorer_create2(
                bsc,
                single_match_scorer_new(bsc->coordinator,
                                           bsc->optional_scorers[0]),
                NULL, 0); /* no optional scorers left */
        }
        else {
            /* more than 1 optional_scorers, no required scorers */
            return counting_sum_scorer_create2(
                bsc,
                counting_disjunction_sum_scorer_new(bsc->coordinator,
                                                       bsc->optional_scorers,
                                                       bsc->os_cnt, 1),
                NULL, 0); /* no optional scorers left */
        }
    }
    else if (bsc->rs_cnt == 1) {
        /* 1 required */
        return counting_sum_scorer_create2(
            bsc,
            single_match_scorer_new(bsc->coordinator, bsc->required_scorers[0]),
            bsc->optional_scorers, bsc->os_cnt);
    }
    else {
        /* more required scorers */
        return counting_sum_scorer_create2(
            bsc,
            counting_conjunction_sum_scorer_new(bsc->coordinator,
                                                bsc->required_scorers,
                                                bsc->rs_cnt),
            bsc->optional_scorers, bsc->os_cnt);
    }
}

static FrtScorer *bsc_init_counting_sum_scorer(BooleanScorer *bsc)
{
    coord_init(bsc->coordinator);
    return bsc->counting_sum_scorer = counting_sum_scorer_create(bsc);
}

static void bsc_add_scorer(FrtScorer *self, FrtScorer *scorer, unsigned int occur)
{
    BooleanScorer *bsc = BSc(self);
    if (occur != FRT_BC_MUST_NOT) {
        bsc->coordinator->max_coord++;
    }

    switch (occur) {
        case FRT_BC_MUST:
            FRT_RECAPA(bsc, rs_cnt, rs_capa, required_scorers, FrtScorer *);
            bsc->required_scorers[bsc->rs_cnt++] = scorer;
            break;
        case FRT_BC_SHOULD:
            FRT_RECAPA(bsc, os_cnt, os_capa, optional_scorers, FrtScorer *);
            bsc->optional_scorers[bsc->os_cnt++] = scorer;
            break;
        case FRT_BC_MUST_NOT:
            FRT_RECAPA(bsc, ps_cnt, ps_capa, prohibited_scorers, FrtScorer *);
            bsc->prohibited_scorers[bsc->ps_cnt++] = scorer;
            break;
        default:
            FRT_RAISE(FRT_ARG_ERROR, "Invalid value for :occur. Try :should, :must or "
                  ":must_not instead");
    }
}

static float bsc_score(FrtScorer *self)
{
    BooleanScorer *bsc = BSc(self);
    Coordinator *coord = bsc->coordinator;
    float sum;
    coord->num_matches = 0;
    sum = bsc->counting_sum_scorer->score(bsc->counting_sum_scorer);
    return sum * coord->coord_factors[coord->num_matches];
}

static bool bsc_next(FrtScorer *self)
{
    FrtScorer *cnt_sum_sc = BSc(self)->counting_sum_scorer;

    if (!cnt_sum_sc) {
        cnt_sum_sc = bsc_init_counting_sum_scorer(BSc(self));
    }
    if (cnt_sum_sc->next(cnt_sum_sc)) {
        self->doc = cnt_sum_sc->doc;
        return true;
    }
    else {
        return false;
    }
}

static bool bsc_skip_to(FrtScorer *self, int doc_num)
{
    FrtScorer *cnt_sum_sc = BSc(self)->counting_sum_scorer;

    if (!BSc(self)->counting_sum_scorer) {
        cnt_sum_sc = bsc_init_counting_sum_scorer(BSc(self));
    }
    if (cnt_sum_sc->skip_to(cnt_sum_sc, doc_num)) {
        self->doc = cnt_sum_sc->doc;
        return true;
    }
    else {
        return false;
    }
}

static void bsc_destroy(FrtScorer *self)
{
    BooleanScorer *bsc = BSc(self);
    Coordinator *coord = bsc->coordinator;

    free(coord->coord_factors);
    free(coord);

    if (bsc->counting_sum_scorer) {
        bsc->counting_sum_scorer->destroy(bsc->counting_sum_scorer);
    }
    else {
        int i;
        for (i = 0; i < bsc->rs_cnt; i++) {
            bsc->required_scorers[i]->destroy(bsc->required_scorers[i]);
        }

        for (i = 0; i < bsc->os_cnt; i++) {
            bsc->optional_scorers[i]->destroy(bsc->optional_scorers[i]);
        }

        for (i = 0; i < bsc->ps_cnt; i++) {
            bsc->prohibited_scorers[i]->destroy(bsc->prohibited_scorers[i]);
        }
    }
    free(bsc->required_scorers);
    free(bsc->optional_scorers);
    free(bsc->prohibited_scorers);
    frt_scorer_destroy_i(self);
}

static FrtExplanation *bsc_explain(FrtScorer *self, int doc_num)
{
    (void)self; (void)doc_num;
    return frt_expl_new(0.0, "This explanation is not supported");
}

static FrtScorer *bsc_new(FrtSimilarity *similarity)
{
    FrtScorer *self = frt_scorer_new(BooleanScorer, similarity);
    BSc(self)->coordinator          = coord_new(similarity);
    BSc(self)->counting_sum_scorer  = NULL;

    self->score     = &bsc_score;
    self->next      = &bsc_next;
    self->skip_to   = &bsc_skip_to;
    self->explain   = &bsc_explain;
    self->destroy   = &bsc_destroy;
    return self;
}

/***************************************************************************
 *
 * BooleanWeight
 *
 ***************************************************************************/

typedef struct BooleanWeight
{
    FrtWeight w;
    FrtWeight **weights;
    int w_cnt;
} BooleanWeight;


static float bw_sum_of_squared_weights(FrtWeight *self)
{
    FrtBooleanQuery *bq = BQ(self->query);
    float sum = 0.0f;
    int i;

    for (i = 0; i < BW(self)->w_cnt; i++) {
        if (! bq->clauses[i]->is_prohibited) {
            FrtWeight *weight = BW(self)->weights[i];
            /* sum sub-weights */
            sum += weight->sum_of_squared_weights(weight);
        }
    }

    /* boost each sub-weight */
    sum *= self->value * self->value;
    return sum;
}

static void bw_normalize(FrtWeight *self, float normalization_factor)
{
    FrtBooleanQuery *bq = BQ(self->query);
    int i;

    normalization_factor *= self->value; /* multiply by query boost */

    for (i = 0; i < BW(self)->w_cnt; i++) {
        if (! bq->clauses[i]->is_prohibited) {
            FrtWeight *weight = BW(self)->weights[i];
            /* sum sub-weights */
            weight->normalize(weight, normalization_factor);
        }
    }
}

static FrtScorer *bw_scorer(FrtWeight *self, FrtIndexReader *ir)
{
    FrtScorer *bsc = bsc_new(self->similarity);
    FrtBooleanQuery *bq = BQ(self->query);
    int i;

    for (i = 0; i < BW(self)->w_cnt; i++) {
        FrtBooleanClause *clause = bq->clauses[i];
        FrtWeight *weight = BW(self)->weights[i];
        FrtScorer *sub_scorer = weight->scorer(weight, ir);
        if (sub_scorer) {
            bsc_add_scorer(bsc, sub_scorer, clause->occur);
        }
        else if (clause->is_required) {
            bsc->destroy(bsc);
            return NULL;
        }
    }

    return bsc;
}

static char *bw_to_s(FrtWeight *self)
{
    return frt_strfmt("BooleanWeight(%f)", self->value);
}

static void bw_destroy(FrtWeight *self)
{
    int i;

    for (i = 0; i < BW(self)->w_cnt; i++) {
        BW(self)->weights[i]->destroy(BW(self)->weights[i]);
    }

    free(BW(self)->weights);
    frt_w_destroy(self);
}

static FrtExplanation *bw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num)
{
    FrtBooleanQuery *bq = BQ(self->query);
    FrtExplanation *sum_expl = frt_expl_new(0.0f, "sum of:");
    FrtExplanation *explanation;
    int coord = 0;
    int max_coord = 0;
    float coord_factor = 0.0f;
    float sum = 0.0f;
    int i;
    for (i = 0; i < BW(self)->w_cnt; i++) {
        FrtWeight *weight = BW(self)->weights[i];
        FrtBooleanClause *clause = bq->clauses[i];
        explanation = weight->explain(weight, ir, doc_num);
        if (!clause->is_prohibited) {
            max_coord++;
        }
        if (explanation->value > 0.0f) {
            if (!clause->is_prohibited) {
                frt_expl_add_detail(sum_expl, explanation);
                sum += explanation->value;
                coord++;
            } else {
                frt_expl_destroy(explanation);
                frt_expl_destroy(sum_expl);
                return frt_expl_new(0.0, "match prohibited");
            }
        } else if (clause->is_required) {
            frt_expl_destroy(explanation);
            frt_expl_destroy(sum_expl);
            return frt_expl_new(0.0, "match required");
        } else {
            frt_expl_destroy(explanation);
        }
    }
    sum_expl->value = sum;
    if (coord == 1) {                /* only one clause matched */
        explanation = sum_expl;      /* eliminate wrapper */
        frt_ary_size(sum_expl->details) = 0;
        sum_expl = sum_expl->details[0];
        frt_expl_destroy(explanation);
    }
    coord_factor = frt_sim_coord(self->similarity, coord, max_coord);
    if (coord_factor == 1.0) {       /* coord is no-op */
        return sum_expl;             /* eliminate wrapper */
    } else {
        explanation = frt_expl_new(sum * coord_factor, "product of:");
        frt_expl_add_detail(explanation, sum_expl);
        frt_expl_add_detail(explanation, frt_expl_new(coord_factor, "coord(%d/%d)",
                                              coord, max_coord));
        return explanation;
    }
}

static FrtWeight *bw_new(FrtQuery *query, FrtSearcher *searcher)
{
    int i;
    FrtWeight *self = w_new(BooleanWeight, query);

    BW(self)->w_cnt = BQ(query)->clause_cnt;
    BW(self)->weights = FRT_ALLOC_N(FrtWeight *, BW(self)->w_cnt);
    for (i = 0; i < BW(self)->w_cnt; i++) {
        BW(self)->weights[i] = frt_q_weight(BQ(query)->clauses[i]->query, searcher);
    }

    self->normalize                 = &bw_normalize;
    self->scorer                    = &bw_scorer;
    self->explain                   = &bw_explain;
    self->to_s                      = &bw_to_s;
    self->destroy                   = &bw_destroy;
    self->sum_of_squared_weights    = &bw_sum_of_squared_weights;

    self->similarity                = query->get_similarity(query, searcher);
    self->value                     = query->boost;

    return self;
}

/***************************************************************************
 *
 * BooleanClause
 *
 ***************************************************************************/

void frt_bc_set_occur(FrtBooleanClause *self, FrtBCType occur)
{
    self->occur = occur;
    switch (occur) {
        case FRT_BC_SHOULD:
            self->is_prohibited = false;
            self->is_required = false;
            break;
        case FRT_BC_MUST:
            self->is_prohibited = false;
            self->is_required = true;
            break;
        case FRT_BC_MUST_NOT:
            self->is_prohibited = true;
            self->is_required = false;
            break;
        default:
            FRT_RAISE(FRT_ARG_ERROR, "Invalid value for :occur. Try :occur => :should, "
                  ":must or :must_not instead");
    }
}

void frt_bc_deref(FrtBooleanClause *self)
{
    if (--self->ref_cnt <= 0) {
        frt_q_deref(self->query);
        free(self);
    }
}

static unsigned long long bc_hash(FrtBooleanClause *self)
{
    return ((frt_q_hash(self->query) << 2) | self->occur);
}

static int  bc_eq(FrtBooleanClause *self, FrtBooleanClause *o)
{
    return ((self->occur == o->occur) && frt_q_eq(self->query, o->query));
}

FrtBooleanClause *frt_bc_new(FrtQuery *query, FrtBCType occur)
{
    FrtBooleanClause *self = FRT_ALLOC(FrtBooleanClause);
    self->ref_cnt = 1;
    self->query = query;
    frt_bc_set_occur(self, occur);
    return self;
}

/***************************************************************************
 *
 * BooleanQuery
 *
 ***************************************************************************/

static FrtMatchVector *bq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv,
                                    FrtTermVector *tv)
{
    int i;
    for (i = BQ(self)->clause_cnt - 1; i >= 0; i--) {
        if (BQ(self)->clauses[i]->occur != FRT_BC_MUST_NOT) {
            FrtQuery *q = BQ(self)->clauses[i]->query;
            q->get_matchv_i(q, mv, tv);
        }
    }
    return mv;
}

static FrtQuery *bq_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    int i;
    const int clause_cnt = BQ(self)->clause_cnt;
    bool rewritten = false;
    bool has_non_prohibited_clause = false;

    if (clause_cnt == 1) {
        /* optimize 1-clause queries */
        FrtBooleanClause *clause = BQ(self)->clauses[0];
        if (! clause->is_prohibited) {
            /* just return clause. Re-write first. */
            FrtQuery *q = clause->query->rewrite(clause->query, ir);

            if (self->boost != 1.0) {
                /* original_boost is initialized to 0.0. If it has been set to
                 * something else it means this query has already been boosted
                 * before so boost from the original value */
                if ((q == clause->query) && BQ(self)->original_boost) {
                    /* rewrite was no-op */
                    q->boost = BQ(self)->original_boost * self->boost;
                }
                else {
                    /* save original boost in case query is rewritten again */
                    BQ(self)->original_boost = q->boost;
                    q->boost *= self->boost;
                }
            }

            return q;
        }
    }

    self->ref_cnt++;
    /* replace each clause's query with its rewritten query */
    for (i = 0; i < clause_cnt; i++) {
        FrtBooleanClause *clause = BQ(self)->clauses[i];
        FrtQuery *rq = clause->query->rewrite(clause->query, ir);
        /* check for at least one non-prohibited clause */
        if (clause->is_prohibited == false) has_non_prohibited_clause = true;
        if (rq != clause->query) {
            if (!rewritten) {
                int j;
                FrtQuery *new_self = frt_q_new(FrtBooleanQuery);
                memcpy(new_self, self, sizeof(FrtBooleanQuery));
                BQ(new_self)->clauses = FRT_ALLOC_N(FrtBooleanClause *,
                                                BQ(self)->clause_capa);
                memcpy(BQ(new_self)->clauses, BQ(self)->clauses,
                       BQ(self)->clause_capa * sizeof(FrtBooleanClause *));
                for (j = 0; j < clause_cnt; j++) {
                    FRT_REF(BQ(self)->clauses[j]);
                }
                self->ref_cnt--;
                self = new_self;
                self->ref_cnt = 1;
                rewritten = true;
            }
            FRT_DEREF(clause);
            BQ(self)->clauses[i] = frt_bc_new(rq, clause->occur);
        } else {
            FRT_DEREF(rq);
        }
    }
    if (clause_cnt > 0 && !has_non_prohibited_clause) {
        frt_bq_add_query_nr(self, frt_maq_new(), FRT_BC_MUST);
    }

    return self;
}

static void bq_extract_terms(FrtQuery *self, FrtHashSet *terms)
{
    int i;
    for (i = 0; i < BQ(self)->clause_cnt; i++) {
        FrtBooleanClause *clause = BQ(self)->clauses[i];
        clause->query->extract_terms(clause->query, terms);
    }
}

static char *bq_to_s(FrtQuery *self, FrtSymbol field)
{
    int i;
    FrtBooleanClause *clause;
    FrtQuery *sub_query;
    char *buffer;
    char *clause_str;
    int bp = 0;
    int size = FRT_QUERY_STRING_START_SIZE;
    int needed;
    int clause_len;

    buffer = FRT_ALLOC_N(char, size);
    if (self->boost != 1.0) {
        buffer[0] = '(';
        bp++;
    }

    for (i = 0; i < BQ(self)->clause_cnt; i++) {
        clause = BQ(self)->clauses[i];
        clause_str = clause->query->to_s(clause->query, field);
        clause_len = (int)strlen(clause_str);
        needed = clause_len + 5;
        while ((size - bp) < needed) {
            size *= 2;
            FRT_REALLOC_N(buffer, char, size);
        }

        if (i > 0) {
            buffer[bp++] = ' ';
        }
        if (clause->is_prohibited) {
            buffer[bp++] = '-';
        }
        else if (clause->is_required) {
            buffer[bp++] = '+';
        }

        sub_query = clause->query;
        if (sub_query->type == BOOLEAN_QUERY) {
            /* wrap sub-bools in parens */
            buffer[bp++] = '(';
            memcpy(buffer + bp, clause_str, sizeof(char) * clause_len);
            bp += clause_len;
            buffer[bp++] = ')';
        }
        else {
            memcpy(buffer + bp, clause_str, sizeof(char) * clause_len);
            bp += clause_len;
        }
        free(clause_str);
    }

    if (self->boost != 1.0) {
        char *boost_str = frt_strfmt(")^%f", self->boost);
        int boost_len = (int)strlen(boost_str);
        FRT_REALLOC_N(buffer, char, bp + boost_len + 1);
        memcpy(buffer + bp, boost_str, sizeof(char) * boost_len);
        bp += boost_len;
        free(boost_str);
    }
    buffer[bp] = 0;
    return buffer;
}

static void bq_destroy(FrtQuery *self)
{
    int i;
    for (i = 0; i < BQ(self)->clause_cnt; i++) {
        frt_bc_deref(BQ(self)->clauses[i]);
    }
    free(BQ(self)->clauses);
    if (BQ(self)->similarity) {
        BQ(self)->similarity->destroy(BQ(self)->similarity);
    }
    frt_q_destroy_i(self);
}

static float bq_coord_disabled(FrtSimilarity *sim, int overlap, int max_overlap)
{
    (void)sim; (void)overlap; (void)max_overlap;
    return 1.0;
}

static FrtSimilarity *bq_get_similarity(FrtQuery *self, FrtSearcher *searcher)
{
    if (!BQ(self)->similarity) {
        FrtSimilarity *sim = frt_q_get_similarity_i(self, searcher);
        BQ(self)->similarity = FRT_ALLOC(FrtSimilarity);
        memcpy(BQ(self)->similarity, sim, sizeof(FrtSimilarity));
        BQ(self)->similarity->coord = &bq_coord_disabled;
        BQ(self)->similarity->destroy = (void (*)(FrtSimilarity *))&free;
    }

    return BQ(self)->similarity;
}

static unsigned long long bq_hash(FrtQuery *self)
{
    int i;
    unsigned long long hash = 0;
    for (i = 0; i < BQ(self)->clause_cnt; i++) {
        hash ^= bc_hash(BQ(self)->clauses[i]);
    }
    return (hash << 1) | BQ(self)->coord_disabled;
}

static int  bq_eq(FrtQuery *self, FrtQuery *o)
{
    int i;
    FrtBooleanQuery *bq1 = BQ(self);
    FrtBooleanQuery *bq2 = BQ(o);
    if ((bq1->coord_disabled != bq2->coord_disabled)
        || (bq1->max_clause_cnt != bq2->max_clause_cnt)
        || (bq1->clause_cnt != bq2->clause_cnt)) {
        return false;
    }

    for (i = 0; i < bq1->clause_cnt; i++) {
        if (!bc_eq(bq1->clauses[i], bq2->clauses[i])) {
            return false;
        }
    }
    return true;
}

FrtQuery *frt_bq_new(bool coord_disabled)
{
    FrtQuery *self = frt_q_new(FrtBooleanQuery);
    BQ(self)->coord_disabled = coord_disabled;
    if (coord_disabled) {
        self->get_similarity = &bq_get_similarity;
    }
    BQ(self)->max_clause_cnt = FRT_DEFAULT_MAX_CLAUSE_COUNT;
    BQ(self)->clause_cnt = 0;
    BQ(self)->clause_capa = FRT_BOOLEAN_CLAUSES_START_CAPA;
    BQ(self)->clauses = FRT_ALLOC_N(FrtBooleanClause *, FRT_BOOLEAN_CLAUSES_START_CAPA);
    BQ(self)->similarity = NULL;
    BQ(self)->original_boost = 0.0f;

    self->type = BOOLEAN_QUERY;
    self->rewrite = &bq_rewrite;
    self->extract_terms = &bq_extract_terms;
    self->to_s = &bq_to_s;
    self->hash = &bq_hash;
    self->eq = &bq_eq;
    self->destroy_i = &bq_destroy;
    self->create_weight_i = &bw_new;
    self->get_matchv_i = &bq_get_matchv_i;

    return self;
}

FrtQuery *frt_bq_new_max(bool coord_disabled, int max)
{
    FrtQuery *q = frt_bq_new(coord_disabled);
    BQ(q)->max_clause_cnt = max;
    return q;
}

FrtBooleanClause *frt_bq_add_clause_nr(FrtQuery *self, FrtBooleanClause *bc)
{
    if (BQ(self)->clause_cnt >= BQ(self)->max_clause_cnt) {
        FRT_RAISE(FRT_STATE_ERROR, "Two many clauses. The max clause limit is set to "
              "<%d> but your query has <%d> clauses. You can try increasing "
              ":max_clause_count for the BooleanQuery or using a different "
              "type of query.", BQ(self)->clause_cnt, BQ(self)->max_clause_cnt);
    }
    if (BQ(self)->clause_cnt >= BQ(self)->clause_capa) {
        BQ(self)->clause_capa *= 2;
        FRT_REALLOC_N(BQ(self)->clauses, FrtBooleanClause *, BQ(self)->clause_capa);
    }
    BQ(self)->clauses[BQ(self)->clause_cnt] = bc;
    BQ(self)->clause_cnt++;
    return bc;
}

FrtBooleanClause *frt_bq_add_clause(FrtQuery *self, FrtBooleanClause *bc)
{
    FRT_REF(bc);
    return frt_bq_add_clause_nr(self, bc);
}

FrtBooleanClause *frt_bq_add_query_nr(FrtQuery *self, FrtQuery *sub_query, FrtBCType occur)
{
    FrtBooleanClause *bc;
    if (BQ(self)->clause_cnt >= BQ(self)->max_clause_cnt) {
        FRT_RAISE(FRT_STATE_ERROR, "Two many clauses. The max clause limit is set to "
              "<%d> but your query has <%d> clauses. You can try increasing "
              ":max_clause_count for the BooleanQuery or using a different "
              "type of query.", BQ(self)->clause_cnt, BQ(self)->max_clause_cnt);
    }
    bc = frt_bc_new(sub_query, occur);
    frt_bq_add_clause(self, bc);
    frt_bc_deref(bc); /* bc was referenced unnecessarily */
    return bc;
}

FrtBooleanClause *frt_bq_add_query(FrtQuery *self, FrtQuery *sub_query, FrtBCType occur)
{
    FRT_REF(sub_query);
    return frt_bq_add_query_nr(self, sub_query, occur);
}

