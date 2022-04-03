#include <string.h>
#include <limits.h>
#include "frt_global.h"
#include "frt_search.h"
#include "frt_array.h"

#undef close

#define PhQ(query) ((FrtPhraseQuery *)(query))

/**
 * Use to sort the phrase positions into positional order. For phrase
 * positions matching at the same position (a very unusual case) we order by
 * first terms. The only real reason for the sorting by first terms is to get
 * consistant order of positions when testing. Functionally it makes no
 * difference.
 */
static int phrase_pos_cmp(const void *p1, const void *p2) {
    int pos1 = ((FrtPhrasePosition *)p1)->pos;
    int pos2 = ((FrtPhrasePosition *)p2)->pos;
    if (pos1 > pos2) {
        return 1;
    }
    if (pos1 < pos2) {
        return -1;
    }
    return strcmp(((FrtPhrasePosition *)p1)->terms[0], ((FrtPhrasePosition *)p2)->terms[0]);
}


/***************************************************************************
 *
 * PhraseScorer
 *
 ***************************************************************************/

/***************************************************************************
 * PhPos
 ***************************************************************************/

#define PP(p) ((PhPos *)(p))
typedef struct PhPos {
    FrtTermDocEnum *tpe;
    int            offset;
    int            count;
    int            doc;
    int            position;
} PhPos;

static bool pp_next(PhPos *self) {
    FrtTermDocEnum *tpe = self->tpe;
    assert(tpe);

    if (!tpe->next(tpe)) {
        tpe->close(tpe);            /* close stream */
        self->tpe = NULL;
        self->doc = INT_MAX;        /* sentinel value */
        return false;
    }
    self->doc = tpe->doc_num(tpe);
    self->position = 0;
    return true;
}

static bool pp_skip_to(PhPos *self, int doc_num)
{
    FrtTermDocEnum *tpe = self->tpe;
    assert(tpe);

    if (!tpe->skip_to(tpe, doc_num)) {
        tpe->close(tpe);            /* close stream */
        self->tpe = NULL;
        self->doc = INT_MAX;        /* sentinel value */
        return false;
    }
    self->doc = tpe->doc_num(tpe);
    self->position = 0;
    return true;
}

static bool pp_next_position(PhPos *self)
{
    FrtTermDocEnum *tpe = self->tpe;
    self->count--;
    if (self->count >= 0) {         /* read subsequent pos's */
        self->position = tpe->next_position(tpe) - self->offset;
        return true;
    } else {
        return false;
    }
}

static bool pp_first_position(PhPos *self)
{
    FrtTermDocEnum *tpe = self->tpe;
    self->count = tpe->freq(tpe);   /* read first pos */
    return pp_next_position(self);
}

#define PP_pp(p) (*(PhPos **)p)
static int pp_cmp(const void *const p1, const void *const p2)
{
    int cmp = PP_pp(p1)->doc - PP_pp(p2)->doc;
    if (cmp == 0) {
        cmp = PP_pp(p1)->position - PP_pp(p2)->position;
        if (cmp == 0) {
            return PP_pp(p1)->offset - PP_pp(p2)->offset;
        }
    }
    return cmp;
}

static int pp_pos_cmp(const void *const p1, const void *const p2)
{
    return PP_pp(p1)->position - PP_pp(p2)->position;
}

static bool pp_less_than(const PhPos *pp1, const PhPos *pp2)
{
    if (pp1->position == pp2->position) {
        return pp1->offset < pp2->offset;
    } else {
        return pp1->position < pp2->position;
    }
}

static void pp_destroy(PhPos *pp)
{
    if (pp->tpe) {
        pp->tpe->close(pp->tpe);
    }
    free(pp);
}

static PhPos *pp_new(FrtTermDocEnum *tpe, int offset)
{
    PhPos *self = FRT_ALLOC(PhPos);

    self->tpe = tpe;
    self->count = self->doc = self->position = -1;
    self->offset = offset;

    return self;
}

/***************************************************************************
 * PhraseScorer
 ***************************************************************************/

#define PhSc(scorer) ((PhraseScorer *)(scorer))

typedef struct PhraseScorer
{
    FrtScorer  super;
    float (*phrase_freq)(FrtScorer *self);
    float   freq;
    frt_uchar  *norms;
    float   value;
    FrtWeight *weight;
    PhPos **phrase_pos;
    int     pp_first_idx;
    int     pp_cnt;
    int     slop;
    bool    first_time : 1;
    bool    more : 1;
    bool    check_repeats : 1;
} PhraseScorer;

static void phsc_init(PhraseScorer *phsc)
{
    int i;
    for (i = phsc->pp_cnt - 1; i >= 0; i--) {
        if (!(phsc->more = pp_next(phsc->phrase_pos[i]))) break;
    }

    if (phsc->more) {
        qsort(phsc->phrase_pos, phsc->pp_cnt,
              sizeof(PhPos *), &pp_cmp);
        phsc->pp_first_idx = 0;
    }
}

static bool phsc_do_next(FrtScorer *self)
{
    PhraseScorer *phsc = PhSc(self);
    const int pp_cnt = phsc->pp_cnt;
    int pp_first_idx = phsc->pp_first_idx;
    PhPos **phrase_positions = phsc->phrase_pos;

    PhPos *first = phrase_positions[pp_first_idx];
    PhPos *last  = phrase_positions[FRT_PREV_NUM(pp_first_idx, pp_cnt)];
    while (phsc->more) {
        /* find doc with all the terms */
        while (phsc->more && first->doc < last->doc) {
            /* skip first upto last */
            phsc->more = pp_skip_to(first, last->doc);
            last = first;
            pp_first_idx = FRT_NEXT_NUM(pp_first_idx, pp_cnt);
            first = phrase_positions[pp_first_idx];
        }

        if (phsc->more) {
            /* pp_first_idx will be used by phrase_freq */
            phsc->pp_first_idx = pp_first_idx;
            /* found a doc with all of the terms */
            phsc->freq = phsc->phrase_freq(self);

            if (phsc->freq == 0.0) {            /* no match */
                /* continuing search so re-set first and last */
                pp_first_idx = phsc->pp_first_idx;
                first = phrase_positions[pp_first_idx];
                last =  phrase_positions[FRT_PREV_NUM(pp_first_idx, pp_cnt)];
                phsc->more = pp_next(last);     /* trigger further scanning */
            } else {
                self->doc = first->doc;
                return true;                    /* found a match */
            }

        }
    }
    return false;
}

static float phsc_score(FrtScorer *self)
{
    PhraseScorer *phsc = PhSc(self);
    float raw_score = frt_sim_tf(self->similarity, phsc->freq) * phsc->value;
    /* normalize */
    return raw_score * frt_sim_decode_norm(
        self->similarity,
        phsc->norms[self->doc]);
}

static bool phsc_next(FrtScorer *self)
{
    PhraseScorer *phsc = PhSc(self);
    if (phsc->first_time) {
        phsc_init(phsc);
        phsc->first_time = false;
    } else if (phsc->more) {
        /* trigger further scanning */
        phsc->more = pp_next(phsc->phrase_pos[FRT_PREV_NUM(phsc->pp_first_idx, phsc->pp_cnt)]);
    }
    return phsc_do_next(self);
}

static bool phsc_skip_to(FrtScorer *self, int doc_num)
{
    PhraseScorer *phsc = PhSc(self);
    int i;
    for (i = phsc->pp_cnt - 1; i >= 0; i--) {
        if (!(phsc->more = pp_skip_to(phsc->phrase_pos[i], doc_num))) {
            break;
        }
    }

    if (phsc->more) {
        qsort(phsc->phrase_pos, phsc->pp_cnt,
              sizeof(PhPos *), &pp_cmp);
        phsc->pp_first_idx = 0;
    }
    return phsc_do_next(self);
}

static FrtExplanation *phsc_explain(FrtScorer *self, int doc_num)
{
    PhraseScorer *phsc = PhSc(self);
    float phrase_freq;

    phsc_skip_to(self, doc_num);

    phrase_freq = (self->doc == doc_num) ? phsc->freq : 0.0f;
    return frt_expl_new(frt_sim_tf(self->similarity, phrase_freq),
                    "tf(phrase_freq=%f)", phrase_freq);
}

static void phsc_destroy(FrtScorer *self)
{
    PhraseScorer *phsc = PhSc(self);
    int i;
    for (i = phsc->pp_cnt - 1; i >= 0; i--) {
        pp_destroy(phsc->phrase_pos[i]);
    }
    free(phsc->phrase_pos);
    frt_scorer_destroy_i(self);
}

static FrtScorer *phsc_new(FrtWeight *weight,
                        FrtTermDocEnum **term_pos_enum,
                        FrtPhrasePosition *positions, int pos_cnt,
                        FrtSimilarity *similarity,
                        frt_uchar *norms,
                        int slop)
{
    int i;
    FrtScorer *self             = frt_scorer_new(PhraseScorer, similarity);
    FrtHashSet *term_set        = NULL;

    PhSc(self)->weight          = weight;
    PhSc(self)->norms           = norms;
    PhSc(self)->value           = weight->value;
    PhSc(self)->phrase_pos      = FRT_ALLOC_N(PhPos *, pos_cnt);
    PhSc(self)->pp_first_idx    = 0;
    PhSc(self)->pp_cnt          = pos_cnt;
    PhSc(self)->slop            = slop;
    PhSc(self)->first_time      = true;
    PhSc(self)->more            = true;
    PhSc(self)->check_repeats   = false;

    if (slop) {
        term_set = frt_hs_new_str((frt_free_ft)NULL);
    }
    for (i = 0; i < pos_cnt; i++) {
        /* check for repeats */
        if (slop && !PhSc(self)->check_repeats) {
            char **terms = positions[i].terms;
            const int t_cnt = frt_ary_size(terms);
            int j;
            for (j = 0; j < t_cnt; j++) {
                if (frt_hs_add(term_set, terms[j])) {
                    PhSc(self)->check_repeats = true;
                    break;
                }
            }
        }
        PhSc(self)->phrase_pos[i] = pp_new(term_pos_enum[i], positions[i].pos);
    }

    if (slop) {
        frt_hs_destroy(term_set);
    }

    self->score     = &phsc_score;
    self->next      = &phsc_next;
    self->skip_to   = &phsc_skip_to;
    self->explain   = &phsc_explain;
    self->destroy   = &phsc_destroy;

    return self;
}

/***************************************************************************
 * ExactPhraseScorer
 ***************************************************************************/

static float ephsc_phrase_freq(FrtScorer *self)
{
    PhraseScorer *phsc = PhSc(self);
    int i;
    int pp_first_idx = 0;
    const int pp_cnt = phsc->pp_cnt;
    float freq = 0.0f;
    PhPos **phrase_positions = phsc->phrase_pos;
    PhPos *first;
    PhPos *last;

    for (i = 0; i < pp_cnt; i++) {
        pp_first_position(phrase_positions[i]);
    }
    qsort(phrase_positions, pp_cnt, sizeof(PhPos *), &pp_pos_cmp);

    first = phrase_positions[0];
    last =  phrase_positions[pp_cnt - 1];

    /* scan to position with all terms */
    do {
        /* scan forward in first */
        while (first->position < last->position) {
            do {
                if (! pp_next_position(first)) {
                    /* maintain first position */
                    phsc->pp_first_idx = pp_first_idx;
                    return freq;
                }
            } while (first->position < last->position);
            last = first;
            pp_first_idx = FRT_NEXT_NUM(pp_first_idx, pp_cnt);
            first = phrase_positions[pp_first_idx];
        }
        freq += 1.0f; /* all equal: a match */
    } while (pp_next_position(last));

    /* maintain first position */
    phsc->pp_first_idx = pp_first_idx;
    return freq;
}

static FrtScorer *exact_phrase_scorer_new(FrtWeight *weight,
                                       FrtTermDocEnum **term_pos_enum,
                                       FrtPhrasePosition *positions, int pp_cnt,
                                       FrtSimilarity *similarity, frt_uchar *norms)
{
    FrtScorer *self = phsc_new(weight,
                            term_pos_enum,
                            positions,
                            pp_cnt,
                            similarity,
                            norms,
                            0);

    PhSc(self)->phrase_freq = &ephsc_phrase_freq;
    return self;
}

/***************************************************************************
 * SloppyPhraseScorer
 ***************************************************************************/

static bool sphsc_check_repeats(PhPos *pp,
                                PhPos **positions,
                                const int p_cnt)
{
    int j;
    for (j = 0; j < p_cnt; j++) {
        PhPos *ppj = positions[j];
        /* If offsets are equal, either we are at the current PhPos +pp+ or
         * +pp+ and +ppj+ are supposed to match in the same position in which
         * case we don't need to check. */
        if (ppj->offset == pp->offset) {
            continue;
        }
        /* the two phrase positions are matching on the same term
         * which we want to avoid */
        if ((ppj->position + ppj->offset) == (pp->position + pp->offset)) {
            if (!pp_next_position(pp)) {
                /* We have no matches for this document */
                return false;
            }
            /* we changed the position so we need to start check again */
            j = -1;
        }
    }
    return true;
}

static float sphsc_phrase_freq(FrtScorer *self)
{
    PhraseScorer *phsc = PhSc(self);
    PhPos *pp;
    FrtPriorityQueue *pq = frt_pq_new(phsc->pp_cnt, (frt_lt_ft)&pp_less_than, NULL);
    const int pp_cnt = phsc->pp_cnt;

    int last_pos = 0, pos, next_pos, start, match_length, i;
    bool done = false;
    bool check_repeats = phsc->check_repeats;
    float freq = 0.0f;

    for (i = 0; i < pp_cnt; i++) {
        bool res;
        pp = phsc->phrase_pos[i];
        /* we should always have at least one position or this functions
         * shouldn't have been called. */
        res = pp_first_position(pp);
        assert(res);(void)res;
        if (check_repeats && (i > 0)) {
            if (!sphsc_check_repeats(pp, phsc->phrase_pos, i - 1)) {
                goto return_freq;
            }
        }
        if (pp->position > last_pos) {
            last_pos = pp->position;
        }
        frt_pq_push(pq, pp);
    }

    do {
        pp = (PhPos *)frt_pq_pop(pq);
        pos = start = pp->position;
        next_pos = PP(frt_pq_top(pq))->position;
        while (pos <= next_pos) {
            start = pos;        /* advance pp to min window */
            if (!pp_next_position(pp) || (check_repeats && !sphsc_check_repeats(pp, phsc->phrase_pos, pp_cnt))) {
                done = true;
                break;
            }
            pos = pp->position;
        }
        match_length = last_pos - start;
        if (match_length <= phsc->slop) {
            /* score match */
            freq += frt_sim_sloppy_freq(self->similarity, match_length);
        }
        if (pp->position > last_pos) {
            last_pos = pp->position;
        }
        frt_pq_push(pq, pp);        /* restore pq */
    } while (!done);

return_freq:
    frt_pq_destroy(pq);
    return freq;
}

static FrtScorer *sloppy_phrase_scorer_new(FrtWeight *weight,
                                        FrtTermDocEnum **term_pos_enum,
                                        FrtPhrasePosition *positions,
                                        int pp_cnt, FrtSimilarity *similarity,
                                        int slop, frt_uchar *norms)
{
    FrtScorer *self = phsc_new(weight,
                            term_pos_enum,
                            positions,
                            pp_cnt,
                            similarity,
                            norms,
                            slop);

    PhSc(self)->phrase_freq = &sphsc_phrase_freq;
    return self;
}

/***************************************************************************
 *
 * PhraseWeight
 *
 ***************************************************************************/

static char *phw_to_s(FrtWeight *self)
{
    return frt_strfmt("PhraseWeight(%f)", self->value);
}

static FrtScorer *phw_scorer(FrtWeight *self, FrtIndexReader *ir)
{
    int i;
    FrtScorer *phsc = NULL;
    FrtPhraseQuery *phq = PhQ(self->query);
    FrtTermDocEnum **tps;
    FrtPhrasePosition *positions = phq->positions;
    const int pos_cnt = phq->pos_cnt;
    const int field_num = frt_fis_get_field_num(ir->fis, phq->field);

    if (pos_cnt == 0 || field_num < 0) {
        return NULL;
    }

    tps = FRT_ALLOC_N(FrtTermDocEnum *, pos_cnt);

    for (i = 0; i < pos_cnt; i++) {
        char **terms = positions[i].terms;
        const int t_cnt = frt_ary_size(terms);
        if (t_cnt == 1) {
            tps[i] = ir->term_positions(ir);
            assert(NULL != tps[i]); /* neither frt_mtdpe_new nor ir->term_positions should return NULL */
            tps[i]->seek(tps[i], field_num, terms[0]);
        } else {
            tps[i] = frt_mtdpe_new(ir, field_num, terms, t_cnt);
            assert(NULL != tps[i]); /* neither frt_mtdpe_new nor ir->term_positions should return NULL */
        }
    }

    if (phq->slop == 0) {       /* optimize exact (common) case */
        phsc = exact_phrase_scorer_new(self, tps, positions, pos_cnt,
                                       self->similarity,
                                       frt_ir_get_norms_i(ir, field_num));
    } else {
        phsc = sloppy_phrase_scorer_new(self, tps, positions, pos_cnt,
                                        self->similarity, phq->slop,
                                        frt_ir_get_norms_i(ir, field_num));
    }
    free(tps);
    return phsc;
}

static FrtExplanation *phw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num)
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
    FrtPhraseQuery *phq = PhQ(self->query);
    const int pos_cnt = phq->pos_cnt;
    FrtPhrasePosition *positions = phq->positions;
    int i, j;
    char *doc_freqs = NULL;
    size_t len = 0, pos = 0;
    const int field_num = frt_fis_get_field_num(ir->fis, phq->field);
    const char *field_name = rb_id2name(phq->field);

    if (field_num < 0) {
        return frt_expl_new(0.0, "field \"%s\" does not exist in the index", field_name);
    }

    query_str = self->query->to_s(self->query, (FrtSymbol)NULL);

    expl = frt_expl_new(0.0, "weight(%s in %d), product of:", query_str, doc_num);

    /* ensure the phrase positions are in order for explanation */
    qsort(positions, pos_cnt, sizeof(FrtPhrasePosition), &phrase_pos_cmp);

    for (i = 0; i < phq->pos_cnt; i++) {
        char **terms = phq->positions[i].terms;
        for (j = frt_ary_size(terms) - 1; j >= 0; j--) {
            len += strlen(terms[j]) + 30;
        }
    }
    doc_freqs = FRT_ALLOC_N(char, len);
    for (i = 0; i < phq->pos_cnt; i++) {
        char **terms = phq->positions[i].terms;
        const int t_cnt = frt_ary_size(terms);
        for (j = 0; j < t_cnt; j++) {
            char *term = terms[j];
            pos += sprintf(doc_freqs + pos, "%s=%d, ",
                           term, ir->doc_freq(ir, field_num, term));
        }
    }
    pos -= 2; /* remove ", " from the end */
    doc_freqs[pos] = 0;

    idf_expl1 = frt_expl_new(self->idf, "idf(%s:<%s>)", field_name, doc_freqs);
    idf_expl2 = frt_expl_new(self->idf, "idf(%s:<%s>)", field_name, doc_freqs);
    free(doc_freqs);

    /* explain query weight */
    query_expl = frt_expl_new(0.0, "query_weight(%s), product of:", query_str);

    if (self->query->boost != 1.0) {
        frt_expl_add_detail(query_expl, frt_expl_new(self->query->boost, "boost"));
    }
    frt_expl_add_detail(query_expl, idf_expl1);

    qnorm_expl = frt_expl_new(self->qnorm, "query_norm");
    frt_expl_add_detail(query_expl, qnorm_expl);

    query_expl->value = self->query->boost * self->idf * self->qnorm;

    frt_expl_add_detail(expl, query_expl);

    /* explain field weight */
    field_expl = frt_expl_new(0.0, "field_weight(%s in %d), product of:",
                          query_str, doc_num);
    free(query_str);

    scorer = self->scorer(self, ir);
    tf_expl = scorer->explain(scorer, doc_num);
    scorer->destroy(scorer);
    frt_expl_add_detail(field_expl, tf_expl);
    frt_expl_add_detail(field_expl, idf_expl2);

    field_norms = ir->get_norms(ir, field_num);
    field_norm = (field_norms != NULL)
        ? frt_sim_decode_norm(self->similarity, field_norms[doc_num])
        : (float)0.0;
    field_norm_expl = frt_expl_new(field_norm, "field_norm(field=%s, doc=%d)",
                               field_name, doc_num);

    frt_expl_add_detail(field_expl, field_norm_expl);

    field_expl->value = tf_expl->value * self->idf * field_norm;

    /* combine them */
    if (query_expl->value == 1.0) {
        frt_expl_destroy(expl);
        return field_expl;
    }
    else {
        expl->value = (query_expl->value * field_expl->value);
        frt_expl_add_detail(expl, field_expl);
        return expl;
    }
}

static FrtWeight *phw_new(FrtQuery *query, FrtSearcher *searcher)
{
    FrtWeight *self        = w_new(FrtWeight, query);

    self->scorer        = &phw_scorer;
    self->explain       = &phw_explain;
    self->to_s          = &phw_to_s;

    self->similarity    = query->get_similarity(query, searcher);
    self->value         = query->boost;
    self->idf           = frt_sim_idf_phrase(self->similarity, PhQ(query)->field,
                                         PhQ(query)->positions,
                                         PhQ(query)->pos_cnt, searcher);
    return self;
}

/***************************************************************************
 *
 * PhraseQuery
 *
 ***************************************************************************/

/* ** TVPosEnum ** */
typedef struct TVPosEnum
{
    int index;
    int size;
    int offset;
    int pos;
    int positions[1];
} TVPosEnum;

static bool tvpe_next(TVPosEnum *self)
{
    if (++(self->index) < self->size) {
        self->pos = self->positions[self->index] - self->offset;
        return true;
    }
    else {
        self->pos = -1;
        return false;
    }
}

static int tvpe_skip_to(TVPosEnum *self, int position)
{
    int i;
    int search_pos = position + self->offset;
    for (i = self->index + 1; i < self->size; i++) {
        if (self->positions[i] >= search_pos) {
            self->pos = self->positions[i] - self->offset;
            break;
        }
    }
    self->index = i;
    if (i == self->size) {
        self->pos = -1;
        return false;
    }
    return true;
}

static bool tvpe_lt(TVPosEnum *tvpe1, TVPosEnum *tvpe2)
{
    return tvpe1->pos < tvpe2->pos;
}

static TVPosEnum *tvpe_new(int *positions, int size, int offset)
{
    TVPosEnum *self = (TVPosEnum*)frt_emalloc(sizeof(TVPosEnum) + size*sizeof(int));
    memcpy(self->positions, positions, size * sizeof(int));
    self->size = size;
    self->offset = offset;
    self->index = -1;
    self->pos = -1;
    return self;
}

static TVPosEnum *tvpe_new_merge(char **terms, int t_cnt, FrtTermVector *tv, int offset)
{
    int i, total_positions = 0;
    FrtPriorityQueue *tvpe_pq = frt_pq_new(t_cnt, (frt_lt_ft)tvpe_lt, &free);
    TVPosEnum *self = NULL;

    for (i = 0; i < t_cnt; i++) {
        FrtTVTerm *tv_term = frt_tv_get_tv_term(tv, terms[i]);
        if (tv_term) {
            TVPosEnum *tvpe = tvpe_new(tv_term->positions, tv_term->freq, 0);
            /* got tv_term so tvpe_next should always return true once here */
            bool res = tvpe_next(tvpe);
            assert(res);(void)res;
            frt_pq_push(tvpe_pq, tvpe);
            total_positions += tv_term->freq;
        }
    }
    if (tvpe_pq->size == 0) {
        frt_pq_destroy(tvpe_pq);
    } else {
        int index = 0;
        self = (TVPosEnum *)frt_emalloc(sizeof(TVPosEnum)
                                    + total_positions * sizeof(int));
        self->size = total_positions;
        self->offset = offset;
        self->index = -1;
        self->pos = -1;
        while (tvpe_pq->size > 0) {
            TVPosEnum *top = (TVPosEnum *)frt_pq_top(tvpe_pq);
            self->positions[index++] = top->pos;
            if (!tvpe_next(top)) {
                frt_pq_pop(tvpe_pq);
                free(top);
            } else {
                frt_pq_down(tvpe_pq);
            }
        }
        frt_pq_destroy(tvpe_pq);
    }
    return self;
}

static TVPosEnum *get_tvpe(FrtTermVector *tv, char **terms, int t_cnt, int offset)
{
    TVPosEnum *tvpe = NULL;
    if (t_cnt == 1) {
        FrtTVTerm *tv_term = frt_tv_get_tv_term(tv, terms[0]);
        if (tv_term) {
            tvpe = tvpe_new(tv_term->positions, tv_term->freq, offset);
        }
    } else {
        tvpe = tvpe_new_merge(terms, t_cnt, tv, offset);
    }
    return tvpe;
}

static FrtMatchVector *phq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv, FrtTermVector *tv)
{
    if (tv->field == PhQ(self)->field) {
        const int pos_cnt = PhQ(self)->pos_cnt;
        int i;
        int slop = PhQ(self)->slop;
        bool done = false;

        if (slop > 0) {
            FrtPriorityQueue *tvpe_pq = frt_pq_new(pos_cnt, (frt_lt_ft)tvpe_lt, &free);
            int last_pos = 0;
            for (i = 0; i < pos_cnt; i++) {
                FrtPhrasePosition *pp = &(PhQ(self)->positions[i]);
                const int t_cnt = frt_ary_size(pp->terms);
                TVPosEnum *tvpe = get_tvpe(tv, pp->terms, t_cnt, pp->pos);
                if (tvpe && tvpe_next(tvpe)) {
                    if (tvpe->pos > last_pos) {
                        last_pos = tvpe->pos;
                    }
                    frt_pq_push(tvpe_pq, tvpe);
                } else {
                    done = true;
                    free(tvpe);
                    break;
                }
            }
            while (!done) {
                TVPosEnum *tvpe = (TVPosEnum *)frt_pq_pop(tvpe_pq);
                int pos;
                int start = pos = tvpe->pos;
                int next_pos = ((TVPosEnum *)frt_pq_top(tvpe_pq))->pos;
                while (pos <= next_pos) {
                    start = pos;
                    if (!tvpe_next(tvpe)) {
                        done = true;
                        break;
                    }
                    pos = tvpe->pos;
                }

                if ((last_pos - start) <= slop) {
                    int min, max = min = start + tvpe->offset;
                    for (i = tvpe_pq->size; i > 0; i--) {
                        TVPosEnum *t = (TVPosEnum *)tvpe_pq->heap[i];
                        int p = t->pos + t->offset;
                        max = p > max ? p : max;
                        min = p < min ? p : min;
                    }
                    frt_matchv_add(mv, min, max);
                }
                if (tvpe->pos > last_pos) {
                    last_pos = tvpe->pos;
                }
                frt_pq_push(tvpe_pq, tvpe);
            }

            frt_pq_destroy(tvpe_pq);
        } else { /* exact match */
            TVPosEnum **tvpe_a = FRT_ALLOC_AND_ZERO_N(TVPosEnum *, pos_cnt);
            TVPosEnum *first, *last;
            int first_index = 0;
            done = false;
            qsort(PhQ(self)->positions, pos_cnt, sizeof(FrtPhrasePosition),
                  &phrase_pos_cmp);
            for (i = 0; i < pos_cnt; i++) {
                FrtPhrasePosition *pp = &(PhQ(self)->positions[i]);
                const int t_cnt = frt_ary_size(pp->terms);
                TVPosEnum *tvpe = get_tvpe(tv, pp->terms, t_cnt, pp->pos);
                if (tvpe && ((i == 0 && tvpe_next(tvpe))
                             || tvpe_skip_to(tvpe, tvpe_a[i-1]->pos))) {
                    tvpe_a[i] = tvpe;
                }
                else {
                    done = true;
                    free(tvpe);
                    break;
                }
            }

            first = tvpe_a[0];
            last = tvpe_a[pos_cnt - 1];

            while (!done) {
                while (first->pos < last->pos) {
                    if (tvpe_skip_to(first, last->pos)) {
                        last = first;
                        first_index = FRT_NEXT_NUM(first_index, pos_cnt);
                        first = tvpe_a[first_index];
                    }
                    else {
                        done = true;
                        break;
                    }
                }
                if (!done) {
                    frt_matchv_add(mv, tvpe_a[0]->pos + tvpe_a[0]->offset,
                               tvpe_a[pos_cnt-1]->pos + tvpe_a[pos_cnt-1]->offset);
                }
                if (!tvpe_next(last)) {
                    done = true;
                }
            }
            for (i = 0; i < pos_cnt; i++) {
                free(tvpe_a[i]);
            }
            free(tvpe_a);
        }
    }
    return mv;
}


/* ** PhraseQuery besides highlighting stuff ** */

#define PhQ_INIT_CAPA 4

static void phq_extract_terms(FrtQuery *self, FrtHashSet *term_set)
{
    FrtPhraseQuery *phq = PhQ(self);
    int i, j;
    for (i = 0; i < phq->pos_cnt; i++) {
        char **terms = phq->positions[i].terms;
        for (j = frt_ary_size(terms) - 1; j >= 0; j--) {
            frt_hs_add(term_set, frt_term_new(phq->field, terms[j]));
        }
    }
}

static char *phq_to_s(FrtQuery *self, FrtSymbol default_field)
{
    FrtPhraseQuery *phq = PhQ(self);
    const int pos_cnt = phq->pos_cnt;
    FrtPhrasePosition *positions = phq->positions;
    const char *field_name = rb_id2name(phq->field);
    int flen = 0;
    if (field_name) {
        flen = strlen(field_name);
    } else {
        field_name = "";
        flen = 0;
    }
    int i, j, buf_index = 0, pos, last_pos;
    size_t len = 0;
    char *buffer;

    if (phq->pos_cnt == 0) {
        if (default_field != phq->field) {
            return frt_strfmt("%s:\"\"", field_name);
        }
        else {
            return frt_estrdup("\"\"");
        }
    }

    /* sort the phrase positions by position */
    qsort(positions, pos_cnt, sizeof(FrtPhrasePosition), &phrase_pos_cmp);

    len = flen + 1;

    for (i = 0; i < pos_cnt; i++) {
        char **terms = phq->positions[i].terms;
        for (j = frt_ary_size(terms) - 1; j >= 0; j--) {
            len += strlen(terms[j]) + 5;
        }
    }

    /* add space for extra <> characters and boost and slop */
    len += 100 + 3
        * (phq->positions[phq->pos_cnt - 1].pos - phq->positions[0].pos);

    buffer = FRT_ALLOC_N(char, len);

    if (default_field != phq->field) {
        memcpy(buffer, field_name, flen);
        buffer[flen] = ':';
        buf_index += flen + 1;
    }

    buffer[buf_index++] = '"';

    last_pos = positions[0].pos - 1;
    for (i = 0; i < pos_cnt; i++) {
        char **terms = positions[i].terms;
        const int t_cnt = frt_ary_size(terms);

        pos = positions[i].pos;
        if (pos == last_pos) {
            buffer[buf_index - 1] = '&';
        }
        else {
            for (j = last_pos; j < pos - 1; j++) {
                memcpy(buffer + buf_index, "<> ", 3);
                buf_index += 3;
            }
        }

        last_pos = pos;
        for (j = 0; j < t_cnt; j++) {
            char *term = terms[j];
            len = strlen(term);
            memcpy(buffer + buf_index, term, len);
            buf_index += len;
            buffer[buf_index++] = '|';
        }
        buffer[buf_index-1] = ' '; /* change last '|' to ' ' */
    }

    if (buffer[buf_index-1] == ' ') {
        buf_index--;
    }

    buffer[buf_index++] = '"';
    buffer[buf_index] = 0;

    if (phq->slop != 0) {
        buf_index += sprintf(buffer + buf_index, "~%d", phq->slop);
    }

    if (self->boost != 1.0) {
        buffer[buf_index++] = '^';
        frt_dbl_to_s(buffer + buf_index, self->boost);
    }

    return buffer;
}

static void phq_destroy(FrtQuery *self)
{
    FrtPhraseQuery *phq = PhQ(self);
    int i;
    for (i = 0; i < phq->pos_cnt; i++) {
        frt_ary_destroy(phq->positions[i].terms, &free);
    }
    free(phq->positions);
    frt_q_destroy_i(self);
}

static FrtQuery *phq_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    FrtPhraseQuery *phq = PhQ(self);
    (void)ir;
    if (phq->pos_cnt == 1) {
        /* optimize one-position case */
        char **terms = phq->positions[0].terms;
        const int t_cnt = frt_ary_size(terms);
        if (t_cnt == 1) {
            FrtQuery *tq = frt_tq_new(phq->field, terms[0]);
            tq->boost = self->boost;
            return tq;
        }
        else {
            FrtQuery *q = frt_multi_tq_new(phq->field);
            int i;
            for (i = 0; i < t_cnt; i++) {
                frt_multi_tq_add_term(q, terms[i]);
            }
            q->boost = self->boost;
            return q;
        }
    } else {
        self->ref_cnt++;
        return self;
    }
}

static unsigned long long phq_hash(FrtQuery *self)
{
    int i, j;
    FrtPhraseQuery *phq = PhQ(self);
    unsigned long long hash = frt_str_hash(rb_id2name(phq->field));
    for (i = 0; i < phq->pos_cnt; i++) {
        char **terms = phq->positions[i].terms;
        for (j = frt_ary_size(terms) - 1; j >= 0; j--) {
            hash = (hash << 1) ^ (frt_str_hash(terms[j])
                               ^ phq->positions[i].pos);
        }
    }
    return (hash ^ phq->slop);
}

static int phq_eq(FrtQuery *self, FrtQuery *o)
{
    int i, j;
    FrtPhraseQuery *phq1 = PhQ(self);
    FrtPhraseQuery *phq2 = PhQ(o);
    if (phq1->slop != phq2->slop
        || phq1->field != phq2->field
        || phq1->pos_cnt != phq2->pos_cnt) {
        return false;
    }
    for (i = 0; i < phq1->pos_cnt; i++) {
        char **terms1 = phq1->positions[i].terms;
        char **terms2 = phq2->positions[i].terms;
        const int t_cnt = frt_ary_size(terms1);
        if (t_cnt != frt_ary_size(terms2)
            || phq1->positions[i].pos != phq2->positions[i].pos) {
            return false;
        }
        for (j = 0; j < t_cnt; j++) {
            if (strcmp(terms1[j], terms2[j]) != 0) {
                return false;
            }
        }
    }
    return true;
}

FrtQuery *frt_phq_alloc(void) {
    return frt_q_new(FrtPhraseQuery);
}

FrtQuery *frt_phq_init(FrtQuery *self, FrtSymbol field) {
    PhQ(self)->field        = field;
    PhQ(self)->pos_cnt      = 0;
    PhQ(self)->pos_capa     = PhQ_INIT_CAPA;
    PhQ(self)->positions    = FRT_ALLOC_N(FrtPhrasePosition, PhQ_INIT_CAPA);

    self->type              = PHRASE_QUERY;
    self->rewrite           = &phq_rewrite;
    self->extract_terms     = &phq_extract_terms;
    self->to_s              = &phq_to_s;
    self->hash              = &phq_hash;
    self->eq                = &phq_eq;
    self->destroy_i         = &phq_destroy;
    self->create_weight_i   = &phw_new;
    self->get_matchv_i      = &phq_get_matchv_i;
    return self;
}

FrtQuery *frt_phq_new(FrtSymbol field) {
    FrtQuery *self = frt_phq_alloc();
    return frt_phq_init(self, field);
}

void frt_phq_add_term_abs(FrtQuery *self, const char *term, int position)
{
    FrtPhraseQuery *phq = PhQ(self);
    int index = phq->pos_cnt;
    FrtPhrasePosition *pp;
    if (index >= phq->pos_capa) {
        phq->pos_capa <<= 1;
        FRT_REALLOC_N(phq->positions, FrtPhrasePosition, phq->pos_capa);
    }
    pp = &(phq->positions[index]);
    pp->terms = frt_ary_new_type_capa(char *, 2);
    frt_ary_push(pp->terms, frt_estrdup(term));
    pp->pos = position;
    phq->pos_cnt++;
}

void frt_phq_add_term(FrtQuery *self, const char *term, int pos_inc)
{
    FrtPhraseQuery *phq = PhQ(self);
    int position;
    if (phq->pos_cnt == 0) {
        position = 0;
    }
    else {
        position = phq->positions[phq->pos_cnt - 1].pos + pos_inc;
    }
    frt_phq_add_term_abs(self, term, position);
}

void frt_phq_append_multi_term(FrtQuery *self, const char *term)
{
    FrtPhraseQuery *phq = PhQ(self);
    int index = phq->pos_cnt - 1;

    if (index < 0) {
        frt_phq_add_term(self, term, 0);
    }
    else {
        frt_ary_push(phq->positions[index].terms, frt_estrdup(term));
    }
}

void frt_phq_set_slop(FrtQuery *self, int slop)
{
    PhQ(self)->slop = slop;
}
