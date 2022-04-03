#include <string.h>
#include <limits.h>
#include "frt_global.h"
#include "frt_search.h"
#include "frt_hashset.h"

#undef close

#define CLAUSE_INIT_CAPA 4

/*****************************************************************************
 *
 * SpanQuery
 *
 *****************************************************************************/

#define SpQ(query) ((FrtSpanQuery *)(query))

static unsigned long long spanq_hash(FrtQuery *self)
{
    return SpQ(self)->field ? frt_str_hash(rb_id2name(SpQ(self)->field)) : 0;
}

static int spanq_eq(FrtQuery *self, FrtQuery *o)
{
    return SpQ(self)->field == SpQ(o)->field;
}

static void spanq_destroy_i(FrtQuery *self)
{
    frt_q_destroy_i(self);
}

static FrtMatchVector *mv_to_term_mv(FrtMatchVector *term_mv, FrtMatchVector *full_mv,
                                  FrtHashSet *terms, FrtTermVector *tv)
{
    FrtHashSetEntry *hse;
    for (hse = terms->first; hse; hse = hse->next) {
        char *term = (char *)hse->elem;
        FrtTVTerm *tv_term = frt_tv_get_tv_term(tv, term);
        if (tv_term) {
            int i, m_idx = 0;
            for (i = 0; i < tv_term->freq; i++) {
                int pos = tv_term->positions[i];
                for (; m_idx < full_mv->size; m_idx++) {
                    if (pos <= full_mv->matches[m_idx].end) {
                        if (pos >= full_mv->matches[m_idx].start) {
                            frt_matchv_add(term_mv, pos, pos);
                        }
                        break;
                    }
                }
            }
        }
    }

    return term_mv;
}

/***************************************************************************
 * TVTermDocEnum
 * dummy TermDocEnum used by the highlighter to find matches
 ***************************************************************************/

#define TV_TDE(tde) ((TVTermDocEnum *)(tde))

typedef struct TVTermDocEnum
{
    FrtTermDocEnum super;
    int         doc;
    int         index;
    int         freq;
    int        *positions;
    FrtTermVector *tv;
} TVTermDocEnum;

static void tv_tde_seek(FrtTermDocEnum *tde, int field_num, const char *term)
{
    TVTermDocEnum *tv_tde = TV_TDE(tde);
    FrtTVTerm *tv_term = frt_tv_get_tv_term(tv_tde->tv, term);
    (void)field_num;
    if (tv_term) {
        tv_tde->doc = -1;
        tv_tde->index = 0;
        tv_tde->freq = tv_term->freq;
        tv_tde->positions = tv_term->positions;
    }
    else {
        tv_tde->doc = INT_MAX;
    }
}

static bool tv_tde_next(FrtTermDocEnum *tde)
{
    if (TV_TDE(tde)->doc == -1) {
        TV_TDE(tde)->doc = 0;
        return true;
    }
    else {
        TV_TDE(tde)->doc = INT_MAX;
        return false;
    }
}

static bool tv_tde_skip_to(FrtTermDocEnum *tde, int doc_num)
{
    if (doc_num == 0) {
        TV_TDE(tde)->doc = 0;
        return true;
    }
    else {
        TV_TDE(tde)->doc = INT_MAX;
        return false;
    }
}

static int tv_tde_next_position(FrtTermDocEnum *tde)
{
    return TV_TDE(tde)->positions[TV_TDE(tde)->index++];
}

static int tv_tde_freq(FrtTermDocEnum *tde)
{
    return TV_TDE(tde)->freq;
}

static int tv_tde_doc_num(FrtTermDocEnum *tde)
{
    return TV_TDE(tde)->doc;
}

static FrtTermDocEnum *spanq_ir_term_positions(FrtIndexReader *ir)
{
    TVTermDocEnum *tv_tde = FRT_ALLOC(TVTermDocEnum);
    FrtTermDocEnum *tde      = (FrtTermDocEnum *)tv_tde;
    tv_tde->tv            = (FrtTermVector *)ir->store;
    tde->seek             = &tv_tde_seek;
    tde->doc_num          = &tv_tde_doc_num;
    tde->freq             = &tv_tde_freq;
    tde->next             = &tv_tde_next;
    tde->skip_to          = &tv_tde_skip_to;
    tde->next_position    = &tv_tde_next_position;
    tde->close            = (void (*)(FrtTermDocEnum *tde))&free;

    return tde;
}

static FrtMatchVector *spanq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv,
                                       FrtTermVector *tv)
{
    if (SpQ(self)->field == tv->field) {
        FrtSpanEnum *sp_enum;
        FrtIndexReader *ir = FRT_ALLOC(FrtIndexReader);
        FrtMatchVector *full_mv = frt_matchv_new();
        FrtHashSet *terms = SpQ(self)->get_terms(self);
        /* FIXME What is going on here? Need to document this! */
        ir->fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
        frt_fis_add_field(ir->fis,
                      frt_fi_new(tv->field, FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO));
        ir->store = (FrtStore *)tv;
        ir->term_positions = &spanq_ir_term_positions;
        sp_enum = SpQ(self)->get_spans(self, ir);
        while (sp_enum->next(sp_enum)) {
            frt_matchv_add(full_mv,
                       sp_enum->start(sp_enum),
                       sp_enum->end(sp_enum) - 1);
        }
        sp_enum->destroy(sp_enum);

        frt_fis_deref(ir->fis);
        free(ir);

        frt_matchv_compact(full_mv);
        mv_to_term_mv(mv, full_mv, terms, tv);
        frt_matchv_destroy(full_mv);
        frt_hs_destroy(terms);
    }
    return mv;
}

/***************************************************************************
 *
 * SpanScorer
 *
 ***************************************************************************/

#define SpSc(scorer) ((SpanScorer *)(scorer))
typedef struct SpanScorer
{
    FrtScorer          super;
    FrtIndexReader    *ir;
    FrtSpanEnum       *spans;
    FrtSimilarity     *sim;
    frt_uchar          *norms;
    FrtWeight         *weight;
    float           value;
    float           freq;
    bool            first_time : 1;
    bool            more : 1;
} SpanScorer;

static float spansc_score(FrtScorer *self)
{
    SpanScorer *spansc = SpSc(self);
    float raw = frt_sim_tf(spansc->sim, spansc->freq) * spansc->value;

    /* normalize */
    return raw * frt_sim_decode_norm(self->similarity, spansc->norms[self->doc]);
}

static bool spansc_next(FrtScorer *self)
{
    SpanScorer *spansc = SpSc(self);
    FrtSpanEnum *se = spansc->spans;
    int match_length;

    if (spansc->first_time) {
        spansc->more = se->next(se);
        spansc->first_time = false;
    }

    if (!spansc->more) {
        return false;
    }

    spansc->freq = 0.0f;
    self->doc = se->doc(se);

    do {
        match_length = se->end(se) - se->start(se);
        spansc->freq += frt_sim_sloppy_freq(spansc->sim, match_length);
        spansc->more = se->next(se);
    } while (spansc->more && (self->doc == se->doc(se)));

    return (spansc->more || (spansc->freq != 0.0));
}

static bool spansc_skip_to(FrtScorer *self, int target)
{
    SpanScorer *spansc = SpSc(self);
    FrtSpanEnum *se = spansc->spans;

    spansc->more = se->skip_to(se, target);
    if (!spansc->more) {
        return false;
    }

    spansc->freq = 0.0f;
    self->doc = se->doc(se);

    while (spansc->more && (se->doc(se) == target)) {
        spansc->freq += frt_sim_sloppy_freq(spansc->sim, se->end(se) - se->start(se));
        spansc->more = se->next(se);
        if (spansc->first_time) {
            spansc->first_time = false;
        }
    }

    return (spansc->more || (spansc->freq != 0.0));
}

static FrtExplanation *spansc_explain(FrtScorer *self, int target)
{
    FrtExplanation *tf_explanation;
    SpanScorer *spansc = SpSc(self);
    float phrase_freq;
    self->skip_to(self, target);
    phrase_freq = (self->doc == target) ? spansc->freq : (float)0.0;

    tf_explanation = frt_expl_new(frt_sim_tf(self->similarity, phrase_freq),
                              "tf(phrase_freq(%f)", phrase_freq);

    return tf_explanation;
}

static void spansc_destroy(FrtScorer *self)
{
    SpanScorer *spansc = SpSc(self);
    if (spansc->spans) {
        spansc->spans->destroy(spansc->spans);
    }
    frt_scorer_destroy_i(self);
}

static FrtScorer *spansc_new(FrtWeight *weight, FrtIndexReader *ir)
{
    FrtScorer *self = NULL;
    const int field_num = frt_fis_get_field_num(ir->fis, SpQ(weight->query)->field);
    if (field_num >= 0) {
        FrtQuery *spanq = weight->query;
        self = frt_scorer_new(SpanScorer, weight->similarity);

        SpSc(self)->first_time  = true;
        SpSc(self)->more        = true;
        SpSc(self)->spans       = SpQ(spanq)->get_spans(spanq, ir);
        SpSc(self)->sim         = weight->similarity;
        SpSc(self)->norms       = ir->get_norms(ir, field_num);
        SpSc(self)->weight      = weight;
        SpSc(self)->value       = weight->value;
        SpSc(self)->freq        = 0.0f;

        self->score             = &spansc_score;
        self->next              = &spansc_next;
        self->skip_to           = &spansc_skip_to;
        self->explain           = &spansc_explain;
        self->destroy           = &spansc_destroy;
    }
    return self;
}

/*****************************************************************************
 * SpanTermEnum
 *****************************************************************************/

#define SpTEn(span_enum) ((SpanTermEnum *)(span_enum))
#define SpTQ(query) ((FrtSpanTermQuery *)(query))

typedef struct SpanTermEnum
{
    FrtSpanEnum     super;
    FrtTermDocEnum *positions;
    int          position;
    int          doc;
    int          count;
    int          freq;
} SpanTermEnum;


static bool spante_next(FrtSpanEnum *self)
{
    SpanTermEnum *ste = SpTEn(self);
    FrtTermDocEnum *tde = ste->positions;

    if (ste->count == ste->freq) {
        if (! tde->next(tde)) {
            ste->doc = INT_MAX;
            return false;
        }
        ste->doc = tde->doc_num(tde);
        ste->freq = tde->freq(tde);
        ste->count = 0;
    }
    ste->position = tde->next_position(tde);
    ste->count++;
    return true;
}

static bool spante_skip_to(FrtSpanEnum *self, int target)
{
    SpanTermEnum *ste = SpTEn(self);
    FrtTermDocEnum *tde = ste->positions;

    /* are we already at the correct position? */
    /* FIXME: perhaps this the the better solution but currently it ->skip_to
     * does a ->next not matter what
    if (ste->doc >= target) {
        return true;
    }
    */

    if (! tde->skip_to(tde, target)) {
        ste->doc = INT_MAX;
        return false;
    }

    ste->doc = tde->doc_num(tde);
    ste->freq = tde->freq(tde);
    ste->count = 0;

    ste->position = tde->next_position(tde);
    ste->count++;
    return true;
}

static int spante_doc(FrtSpanEnum *self)
{
    return SpTEn(self)->doc;
}

static int spante_start(FrtSpanEnum *self)
{
    return SpTEn(self)->position;
}

static int spante_end(FrtSpanEnum *self)
{
    return SpTEn(self)->position + 1;
}

static char *spante_to_s(FrtSpanEnum *self)
{
    char *query_str = self->query->to_s(self->query, (FrtSymbol)NULL);
    char pos_str[20];
    size_t len = strlen(query_str);
    int pos;
    char *str = FRT_ALLOC_N(char, len + 40);

    if (self->doc(self) < 0) {
        sprintf(pos_str, "START");
    }
    else {
        if (self->doc(self) == INT_MAX) {
            sprintf(pos_str, "END");
        }
        else {
            pos = SpTEn(self)->position;
            sprintf(pos_str, "%d", self->doc(self) - pos);
        }
    }
    sprintf(str, "SpanTermEnum(%s)@%s", query_str, pos_str);
    free(query_str);
    return str;
}

static void spante_destroy(FrtSpanEnum *self)
{
    FrtTermDocEnum *tde = SpTEn(self)->positions;
    tde->close(tde);
    free(self);
}

static FrtSpanEnum *spante_new(FrtQuery *query, FrtIndexReader *ir)
{
    char *term = SpTQ(query)->term;
    FrtSpanEnum *self = (FrtSpanEnum *)FRT_ALLOC(SpanTermEnum);

    SpTEn(self)->positions  = frt_ir_term_positions_for(ir, SpQ(query)->field,
                                                    term);
    SpTEn(self)->position   = -1;
    SpTEn(self)->doc        = -1;
    SpTEn(self)->count      = 0;
    SpTEn(self)->freq       = 0;

    self->query             = query;
    self->next              = &spante_next;
    self->skip_to           = &spante_skip_to;
    self->doc               = &spante_doc;
    self->start             = &spante_start;
    self->end               = &spante_end;
    self->destroy           = &spante_destroy;
    self->to_s              = &spante_to_s;

    return self;
}

/*****************************************************************************
 * SpanMultiTermEnum
 *****************************************************************************/

/* * TermPosEnumWrapper * */
#define TPE_READ_SIZE 16

typedef struct TermPosEnumWrapper
{
    const char  *term;
    FrtTermDocEnum *tpe;
    int          doc;
    int          pos;
} TermPosEnumWrapper;

static bool tpew_less_than(const TermPosEnumWrapper *tpew1,
                           const TermPosEnumWrapper *tpew2)
{
    return (tpew1->doc < tpew2->doc)
        || (tpew1->doc == tpew2->doc && tpew1->pos < tpew2->pos);
}

static bool tpew_next(TermPosEnumWrapper *self)
{
    FrtTermDocEnum *tpe = self->tpe;
    if (0 > (self->pos = tpe->next_position(tpe))) {
        if (!tpe->next(tpe)) return false;
        self->doc = tpe->doc_num(tpe);
        self->pos = tpe->next_position(tpe);
    }
    return true;
}

static bool tpew_skip_to(TermPosEnumWrapper *self, int doc_num)
{
    FrtTermDocEnum *tpe = self->tpe;

    if (tpe->skip_to(tpe, doc_num)) {
        self->doc = tpe->doc_num(tpe);
        self->pos = tpe->next_position(tpe);
        return true;
    }
    else {
        return false;
    }
}

static void tpew_destroy(TermPosEnumWrapper *self)
{
    self->tpe->close(self->tpe);
    free(self);
}

static TermPosEnumWrapper *tpew_new(const char *term, FrtTermDocEnum *tpe)
{
    TermPosEnumWrapper *self = FRT_ALLOC_AND_ZERO(TermPosEnumWrapper);
    self->term = term;
    self->tpe = tpe;
    self->doc = -1;
    self->pos = -1;
    return self;
}
#define SpMTEn(span_enum) ((SpanMultiTermEnum *)(span_enum))
#define SpMTQ(query) ((FrtSpanMultiTermQuery *)(query))

typedef struct SpanMultiTermEnum
{
    FrtSpanEnum             super;
    FrtPriorityQueue       *tpew_pq;
    TermPosEnumWrapper **tpews;
    int                  tpew_cnt;
    int                  pos;
    int                  doc;
} SpanMultiTermEnum;

static bool spanmte_next(FrtSpanEnum *self)
{
    int curr_doc, curr_pos;
    TermPosEnumWrapper *tpew;
    SpanMultiTermEnum *mte = SpMTEn(self);
    FrtPriorityQueue *tpew_pq = mte->tpew_pq;
    if (tpew_pq == NULL) {
        TermPosEnumWrapper **tpews = mte->tpews;
        int i;
        tpew_pq = frt_pq_new(mte->tpew_cnt, (frt_lt_ft)tpew_less_than, (frt_free_ft)NULL);
        for (i = mte->tpew_cnt - 1; i >= 0; i--) {
            if (tpew_next(tpews[i])) {
                frt_pq_push(tpew_pq, tpews[i]);
            }
        }
        mte->tpew_pq = tpew_pq;
    }

    tpew = (TermPosEnumWrapper *)frt_pq_top(tpew_pq);
    if (tpew == NULL) {
        return false;
    }

    mte->doc = curr_doc = tpew->doc;
    mte->pos = curr_pos = tpew->pos;

    do {
        if (tpew_next(tpew)) {
            frt_pq_down(tpew_pq);
        }
        else {
            frt_pq_pop(tpew_pq);
        }
    } while (((tpew = (TermPosEnumWrapper *)frt_pq_top(tpew_pq)) != NULL)
             && tpew->doc == curr_doc && tpew->pos == curr_pos);
    return true;
}

static bool spanmte_skip_to(FrtSpanEnum *self, int target)
{
    SpanMultiTermEnum *mte = SpMTEn(self);
    FrtPriorityQueue *tpew_pq = mte->tpew_pq;
    TermPosEnumWrapper *tpew;
    if (tpew_pq == NULL) {
        TermPosEnumWrapper **tpews = mte->tpews;
        int i;
        tpew_pq = frt_pq_new(mte->tpew_cnt, (frt_lt_ft)tpew_less_than, (frt_free_ft)NULL);
        for (i = mte->tpew_cnt - 1; i >= 0; i--) {
            tpew_skip_to(tpews[i], target);
            frt_pq_push(tpew_pq, tpews[i]);
        }
        mte->tpew_pq = tpew_pq;
    }
    if (tpew_pq->size == 0) {
        mte->doc = -1;
        return false;
    }
    while ((tpew = (TermPosEnumWrapper *)frt_pq_top(tpew_pq)) != NULL
           && (target > tpew->doc)) {
        if (tpew_skip_to(tpew, target)) {
            frt_pq_down(tpew_pq);
        }
        else {
            frt_pq_pop(tpew_pq);
        }
    }
    return spanmte_next(self);
}

static int spanmte_doc(FrtSpanEnum *self)
{
    return SpMTEn(self)->doc;
}

static int spanmte_start(FrtSpanEnum *self)
{
    return SpMTEn(self)->pos;
}

static int spanmte_end(FrtSpanEnum *self)
{
    return SpMTEn(self)->pos + 1;
}

static void spanmte_destroy(FrtSpanEnum *self)
{
    SpanMultiTermEnum *mte = SpMTEn(self);
    int i;
    if (mte->tpew_pq) frt_pq_destroy(mte->tpew_pq);
    for (i = 0; i < mte->tpew_cnt; i++) {
        tpew_destroy(mte->tpews[i]);
    }
    free(mte->tpews);
    free(self);
}

static FrtSpanEnum *spanmte_new(FrtQuery *query, FrtIndexReader *ir)
{
    FrtSpanEnum *self = (FrtSpanEnum *)FRT_ALLOC(SpanMultiTermEnum);
    SpanMultiTermEnum *smte = SpMTEn(self);
    FrtSpanMultiTermQuery *smtq = SpMTQ(query);
    int i;


    smte->tpews = FRT_ALLOC_N(TermPosEnumWrapper *, smtq->term_cnt);
    for (i = 0; i < smtq->term_cnt; i++) {
        char *term = smtq->terms[i];
        smte->tpews[i] = tpew_new(term,
            frt_ir_term_positions_for(ir, SpQ(query)->field, term));
    }
    smte->tpew_cnt          = smtq->term_cnt;
    smte->tpew_pq           = NULL;
    smte->pos               = -1;
    smte->doc               = -1;

    self->query             = query;
    self->next              = &spanmte_next;
    self->skip_to           = &spanmte_skip_to;
    self->doc               = &spanmte_doc;
    self->start             = &spanmte_start;
    self->end               = &spanmte_end;
    self->destroy           = &spanmte_destroy;
    self->to_s              = &spante_to_s;

    return self;
}


/*****************************************************************************
 * SpanFirstEnum
 *****************************************************************************/

#define SpFEn(span_enum) ((SpanFirstEnum *)(span_enum))
#define SpFQ(query) ((FrtSpanFirstQuery *)(query))

typedef struct SpanFirstEnum
{
    FrtSpanEnum    super;
    FrtSpanEnum   *sub_enum;
} SpanFirstEnum;


static bool spanfe_next(FrtSpanEnum *self)
{
    FrtSpanEnum *sub_enum = SpFEn(self)->sub_enum;
    int end = SpFQ(self->query)->end;
    while (sub_enum->next(sub_enum)) { /* scan to next match */
        if (sub_enum->end(sub_enum) <= end) {
            return true;
        }
    }
    return false;
}

static bool spanfe_skip_to(FrtSpanEnum *self, int target)
{
    FrtSpanEnum *sub_enum = SpFEn(self)->sub_enum;
    int end = SpFQ(self->query)->end;

    if (! sub_enum->skip_to(sub_enum, target)) {
        return false;
    }

    if (sub_enum->end(sub_enum) <= end) {   /* there is a match */
        return true;
    }

    return spanfe_next(self);        /* scan to next match */
}

static int spanfe_doc(FrtSpanEnum *self)
{
    FrtSpanEnum *sub_enum = SpFEn(self)->sub_enum;
    return sub_enum->doc(sub_enum);
}

static int spanfe_start(FrtSpanEnum *self)
{
    FrtSpanEnum *sub_enum = SpFEn(self)->sub_enum;
    return sub_enum->start(sub_enum);
}

static int spanfe_end(FrtSpanEnum *self)
{
    FrtSpanEnum *sub_enum = SpFEn(self)->sub_enum;
    return sub_enum->end(sub_enum);
}

static char *spanfe_to_s(FrtSpanEnum *self)
{
    char *query_str = self->query->to_s(self->query, (FrtSymbol)NULL);
    char *res = frt_strfmt("SpanFirstEnum(%s)", query_str);
    free(query_str);
    return res;
}

static void spanfe_destroy(FrtSpanEnum *self)
{
    FrtSpanEnum *sub_enum = SpFEn(self)->sub_enum;
    sub_enum->destroy(sub_enum);
    free(self);
}

static FrtSpanEnum *spanfe_new(FrtQuery *query, FrtIndexReader *ir)
{
    FrtSpanEnum *self          = (FrtSpanEnum *)FRT_ALLOC(SpanFirstEnum);
    FrtSpanFirstQuery *sfq     = SpFQ(query);

    SpFEn(self)->sub_enum   = SpQ(sfq->match)->get_spans(sfq->match, ir);

    self->query     = query;
    self->next      = &spanfe_next;
    self->skip_to   = &spanfe_skip_to;
    self->doc       = &spanfe_doc;
    self->start     = &spanfe_start;
    self->end       = &spanfe_end;
    self->destroy   = &spanfe_destroy;
    self->to_s      = &spanfe_to_s;

    return self;
}


/*****************************************************************************
 * SpanOrEnum
 *****************************************************************************/

#define SpOEn(span_enum) ((SpanOrEnum *)(span_enum))
#define SpOQ(query) ((FrtSpanOrQuery *)(query))

typedef struct SpanOrEnum
{
    FrtSpanEnum        super;
    FrtPriorityQueue  *queue;
    FrtSpanEnum      **span_enums;
    int             s_cnt;
    bool            first_time : 1;
} SpanOrEnum;


static bool span_less_than(FrtSpanEnum *s1, FrtSpanEnum *s2)
{
    int doc_diff, start_diff;
    doc_diff = s1->doc(s1) - s2->doc(s2);
    if (doc_diff == 0) {
        start_diff = s1->start(s1) - s2->start(s2);
        if (start_diff == 0) {
            return s1->end(s1) < s2->end(s2);
        }
        else {
            return start_diff < 0;
        }
    }
    else {
        return doc_diff < 0;
    }
}

static bool spanoe_next(FrtSpanEnum *self)
{
    SpanOrEnum *soe = SpOEn(self);
    FrtSpanEnum *se;
    int i;

    if (soe->first_time) { /* first time -- initialize */
        for (i = 0; i < soe->s_cnt; i++) {
            se = soe->span_enums[i];
            if (se->next(se)) { /* move to first entry */
                frt_pq_push(soe->queue, se);
            }
        }
        soe->first_time = false;
        return soe->queue->size != 0;
    }

    if (soe->queue->size == 0) {
        return false; /* all done */
    }

    se = (FrtSpanEnum *)frt_pq_top(soe->queue);
    if (se->next(se)) { /* move to next */
        frt_pq_down(soe->queue);
        return true;
    }

    frt_pq_pop(soe->queue); /* exhausted a clause */

    return soe->queue->size != 0;
}

static bool spanoe_skip_to(FrtSpanEnum *self, int target)
{
    SpanOrEnum *soe = SpOEn(self);
    FrtSpanEnum *se;
    int i;

    if (soe->first_time) { /* first time -- initialize */
        for (i = 0; i < soe->s_cnt; i++) {
            se = soe->span_enums[i];
            if (se->skip_to(se, target)) {/* move to target */
                frt_pq_push(soe->queue, se);
            }
        }
        soe->first_time = false;
    }
    else {
        while ((soe->queue->size != 0) &&
               ((se = (FrtSpanEnum *)frt_pq_top(soe->queue)) != NULL) &&
               (se->doc(se) < target)) {
            if (se->skip_to(se, target)) {
                frt_pq_down(soe->queue);
            }
            else {
                frt_pq_pop(soe->queue);
            }
        }
    }

    return soe->queue->size != 0;
}

#define SpOEn_Top_SE(self) (FrtSpanEnum *)frt_pq_top(SpOEn(self)->queue)

static int spanoe_doc(FrtSpanEnum *self)
{
    FrtSpanEnum *se = SpOEn_Top_SE(self);
    return se->doc(se);
}

static int spanoe_start(FrtSpanEnum *self)
{
    FrtSpanEnum *se = SpOEn_Top_SE(self);
    return se->start(se);
}

static int spanoe_end(FrtSpanEnum *self)
{
    FrtSpanEnum *se = SpOEn_Top_SE(self);
    return se->end(se);
}

static char *spanoe_to_s(FrtSpanEnum *self)
{
    SpanOrEnum *soe = SpOEn(self);
    char *query_str = self->query->to_s(self->query, (FrtSymbol)NULL);
    char doc_str[62];
    size_t len = strlen(query_str);
    char *str = FRT_ALLOC_N(char, len + 80);

    if (soe->first_time) {
        sprintf(doc_str, "START");
    }
    else {
        if (soe->queue->size == 0) {
            sprintf(doc_str, "END");
        }
        else {
            sprintf(doc_str, "%d:%d-%d", self->doc(self),
                    self->start(self), self->end(self));
        }
    }
    sprintf(str, "SpanOrEnum(%s)@%s", query_str, doc_str);
    free(query_str);
    return str;
}

static void spanoe_destroy(FrtSpanEnum *self)
{
    FrtSpanEnum *se;
    SpanOrEnum *soe = SpOEn(self);
    int i;
    frt_pq_destroy(soe->queue);
    for (i = 0; i < soe->s_cnt; i++) {
        se = soe->span_enums[i];
        se->destroy(se);
    }
    free(soe->span_enums);
    free(self);
}

static FrtSpanEnum *spanoe_new(FrtQuery *query, FrtIndexReader *ir)
{
    FrtQuery *clause;
    FrtSpanEnum *self      = (FrtSpanEnum *)FRT_ALLOC(SpanOrEnum);
    FrtSpanOrQuery *soq    = SpOQ(query);
    int i;

    SpOEn(self)->first_time = true;
    SpOEn(self)->s_cnt      = soq->c_cnt;
    SpOEn(self)->span_enums = FRT_ALLOC_N(FrtSpanEnum *, SpOEn(self)->s_cnt);

    for (i = 0; i < SpOEn(self)->s_cnt; i++) {
        clause = soq->clauses[i];
        SpOEn(self)->span_enums[i] = SpQ(clause)->get_spans(clause, ir);
    }

    SpOEn(self)->queue      = frt_pq_new(SpOEn(self)->s_cnt, (frt_lt_ft)&span_less_than,
                                     (frt_free_ft)NULL);

    self->query             = query;
    self->next              = &spanoe_next;
    self->skip_to           = &spanoe_skip_to;
    self->doc               = &spanoe_doc;
    self->start             = &spanoe_start;
    self->end               = &spanoe_end;
    self->destroy           = &spanoe_destroy;
    self->to_s              = &spanoe_to_s;

    return self;
}

/*****************************************************************************
 * SpanNearEnum
 *****************************************************************************/

#define SpNEn(span_enum) ((SpanNearEnum *)(span_enum))
#define SpNQ(query) ((FrtSpanNearQuery *)(query))

typedef struct SpanNearEnum
{
    FrtSpanEnum    super;
    FrtSpanEnum  **span_enums;
    int         s_cnt;
    int         slop;
    int         current;
    int         doc;
    int         start;
    int         end;
    bool        first_time : 1;
    bool        in_order : 1;
} SpanNearEnum;


#define SpNEn_NEXT() do {\
    sne->current = (sne->current+1) % sne->s_cnt;\
    se = sne->span_enums[sne->current];\
} while (0);

static bool sne_init(SpanNearEnum *sne)
{
    FrtSpanEnum *se = sne->span_enums[sne->current];
    int prev_doc = se->doc(se);
    int i;

    for (i = 1; i < sne->s_cnt; i++) {
        SpNEn_NEXT();
        if (!se->skip_to(se, prev_doc)) {
            return false;
        }
        prev_doc = se->doc(se);
    }
    return true;
}

static bool sne_goto_next_doc(SpanNearEnum *sne)
{
    FrtSpanEnum *se = sne->span_enums[sne->current];
    int prev_doc = se->doc(se);

    SpNEn_NEXT();

    while (se->doc(se) < prev_doc) {
        if (! se->skip_to(se, prev_doc)) {
            return false;
        }
        prev_doc = se->doc(se);
        SpNEn_NEXT();
    }
    return true;
}

static bool sne_next_unordered_match(FrtSpanEnum *self)
{
    SpanNearEnum *sne = SpNEn(self);
    FrtSpanEnum *se, *min_se = NULL;
    int i;
    int max_end, end, min_start, start, doc;
    int lengths_sum;

    while (true) {
        max_end = 0;
        min_start = INT_MAX;
        lengths_sum = 0;

        for (i = 0; i < sne->s_cnt; i++) {
            se = sne->span_enums[i];
            if ((end=se->end(se)) > max_end) {
                max_end = end;
            }
            if ((start=se->start(se)) < min_start) {
                min_start = start;
                min_se = se;
                sne->current = i; /* current should point to the minimum span */
            }
            lengths_sum += end - start;
        }

        if ((max_end - min_start - lengths_sum) <= sne->slop) {
            /* we have a match */
            sne->start = min_start;
            sne->end = max_end;
            sne->doc = min_se->doc(min_se);
            return true;
        }

        /* increment the minimum span_enum and try again */
        doc = min_se->doc(min_se);
        if (!min_se->next(min_se)) {
            return false;
        }
        if (doc < min_se->doc(min_se)) {
            if (!sne_goto_next_doc(sne)) return false;
        }
    }
}

static bool sne_next_ordered_match(FrtSpanEnum *self)
{
    SpanNearEnum *sne = SpNEn(self);
    FrtSpanEnum *se;
    int i;
    int prev_doc, prev_start, prev_end;
    int doc=0, start=0, end=0;
    int lengths_sum;

    while (true) {
        se = sne->span_enums[0];

        prev_doc = se->doc(se);
        sne->start = prev_start = se->start(se);
        prev_end = se->end(se);

        i = 1;
        lengths_sum = prev_end - prev_start;

        while (i < sne->s_cnt) {
            se = sne->span_enums[i];
            doc = se->doc(se);
            start = se->start(se);
            end = se->end(se);
            while ((doc == prev_doc) && ((start < prev_start) ||
                                         ((start == prev_start) && (end < prev_end)))) {
                if (!se->next(se)) {
                    return false;
                }
                doc = se->doc(se);
                start = se->start(se);
                end = se->end(se);
            }
            if (doc != prev_doc) {
                sne->current = i;
                if (!sne_goto_next_doc(sne)) {
                    return false;
                }
                break;
            }
            i++;
            lengths_sum += end - start;
            prev_doc = doc;
            prev_start = start;
            prev_end = end;
        }
        if (i == sne->s_cnt) {
            if ((end - sne->start - lengths_sum) <= sne->slop) {
                /* we have a match */
                sne->end = end;
                sne->doc = doc;

                /* the minimum span is always the first span so it needs to be
                 * incremented next time around */
                sne->current = 0;
                return true;

            }
            else {
                se = sne->span_enums[0];
                if (!se->next(se)) {
                    return false;
                }
                if (se->doc(se) != prev_doc) {
                    sne->current = 0;
                    if (!sne_goto_next_doc(sne)) {
                        return false;
                    }
                }
            }
        }
    }
}

static bool sne_next_match(FrtSpanEnum *self)
{
    SpanNearEnum *sne = SpNEn(self);
    FrtSpanEnum *se_curr, *se_next;

    if (!sne->first_time) {
        if (!sne_init(sne)) {
            return false;
        }
        sne->first_time = false;
    }
    se_curr = sne->span_enums[sne->current];
    se_next = sne->span_enums[(sne->current+1)%sne->s_cnt];
    if (se_curr->doc(se_curr) > se_next->doc(se_next)) {
        if (!sne_goto_next_doc(sne)) {
            return false;
        }
    }

    if (sne->in_order) {
        return sne_next_ordered_match(self);
    }
    else {
        return sne_next_unordered_match(self);
    }
}

static bool spanne_next(FrtSpanEnum *self)
{
    SpanNearEnum *sne = SpNEn(self);
    FrtSpanEnum *se;

    se = sne->span_enums[sne->current];
    if (!se->next(se)) return false;

    return sne_next_match(self);
}

static bool spanne_skip_to(FrtSpanEnum *self, int target)
{
    FrtSpanEnum *se = SpNEn(self)->span_enums[SpNEn(self)->current];
    if (!se->skip_to(se, target)) {
        return false;
    }

    return sne_next_match(self);
}

static int spanne_doc(FrtSpanEnum *self)
{
    return SpNEn(self)->doc;
}

static int spanne_start(FrtSpanEnum *self)
{
    return SpNEn(self)->start;
}

static int spanne_end(FrtSpanEnum *self)
{
    return SpNEn(self)->end;
}

static char *spanne_to_s(FrtSpanEnum *self)
{
    SpanNearEnum *sne = SpNEn(self);
    char *query_str = self->query->to_s(self->query, (FrtSymbol)NULL);
    char doc_str[62];
    size_t len = strlen(query_str);
    char *str = FRT_ALLOC_N(char, len + 80);

    if (sne->first_time) {
        sprintf(doc_str, "START");
    }
    else {
        sprintf(doc_str, "%d:%d-%d", self->doc(self),
                self->start(self), self->end(self));
    }
    sprintf(str, "SpanNearEnum(%s)@%s", query_str, doc_str);
    free(query_str);
    return str;
}

static void spanne_destroy(FrtSpanEnum *self)
{
    FrtSpanEnum *se;
    SpanNearEnum *sne = SpNEn(self);
    int i;
    for (i = 0; i < sne->s_cnt; i++) {
        se = sne->span_enums[i];
        se->destroy(se);
    }
    free(sne->span_enums);
    free(self);
}

static FrtSpanEnum *spanne_new(FrtQuery *query, FrtIndexReader *ir)
{
    int i;
    FrtQuery *clause;
    FrtSpanEnum *self          = (FrtSpanEnum *)FRT_ALLOC(SpanNearEnum);
    FrtSpanNearQuery *snq      = SpNQ(query);

    SpNEn(self)->first_time = true;
    SpNEn(self)->in_order   = snq->in_order;
    SpNEn(self)->slop       = snq->slop;
    SpNEn(self)->s_cnt      = snq->c_cnt;
    SpNEn(self)->span_enums = FRT_ALLOC_N(FrtSpanEnum *, SpNEn(self)->s_cnt);

    for (i = 0; i < SpNEn(self)->s_cnt; i++) {
        clause = snq->clauses[i];
        SpNEn(self)->span_enums[i] = SpQ(clause)->get_spans(clause, ir);
    }
    SpNEn(self)->current    = 0;

    SpNEn(self)->doc        = -1;
    SpNEn(self)->start      = -1;
    SpNEn(self)->end        = -1;

    self->query             = query;
    self->next              = &spanne_next;
    self->skip_to           = &spanne_skip_to;
    self->doc               = &spanne_doc;
    self->start             = &spanne_start;
    self->end               = &spanne_end;
    self->destroy           = &spanne_destroy;
    self->to_s              = &spanne_to_s;

    return self;
}

/*****************************************************************************
 *
 * SpanNotEnum
 *
 *****************************************************************************/

#define SpXEn(span_enum) ((SpanNotEnum *)(span_enum))
#define SpXQ(query) ((FrtSpanNotQuery *)(query))

typedef struct SpanNotEnum
{
    FrtSpanEnum    super;
    FrtSpanEnum   *inc;
    FrtSpanEnum   *exc;
    bool        more_inc : 1;
    bool        more_exc : 1;
} SpanNotEnum;


static bool spanxe_next(FrtSpanEnum *self)
{
    SpanNotEnum *sxe = SpXEn(self);
    FrtSpanEnum *inc = sxe->inc, *exc = sxe->exc;
    if (sxe->more_inc) {                        /*  move to next incl */
        sxe->more_inc = inc->next(inc);
    }

    while (sxe->more_inc && sxe->more_exc) {
        if (inc->doc(inc) > exc->doc(exc)) {    /*  skip excl */
            sxe->more_exc = exc->skip_to(exc, inc->doc(inc));
        }

        while (sxe->more_exc                    /*  while excl is before */
               && (inc->doc(inc) == exc->doc(exc))
               && (exc->end(exc) <= inc->start(inc))) {
            sxe->more_exc = exc->next(exc);     /*  increment excl */
        }

        if (! sxe->more_exc ||                  /*  if no intersection */
            (inc->doc(inc) != exc->doc(exc)) ||
            inc->end(inc) <= exc->start(exc)) {
            break;                              /*  we found a match */
        }

        sxe->more_inc = inc->next(inc);         /*  intersected: keep scanning */
    }
    return sxe->more_inc;
}

static bool spanxe_skip_to(FrtSpanEnum *self, int target)
{
    SpanNotEnum *sxe = SpXEn(self);
    FrtSpanEnum *inc = sxe->inc, *exc = sxe->exc;
    int doc;

    if (sxe->more_inc) {                        /*  move to next incl */
        if (!(sxe->more_inc=sxe->inc->skip_to(sxe->inc, target))) return false;
    }

    if (sxe->more_inc && ((doc=inc->doc(inc)) > exc->doc(exc))) {
        sxe->more_exc = exc->skip_to(exc, doc);
    }

    while (sxe->more_exc                       /*  while excl is before */
           && inc->doc(inc) == exc->doc(exc)
           && exc->end(exc) <= inc->start(inc)) {
        sxe->more_exc = exc->next(exc);        /*  increment excl */
    }

    if (!sxe->more_exc ||                      /*  if no intersection */
        inc->doc(inc) != exc->doc(exc) ||
        inc->end(inc) <= exc->start(exc)) {
        return true;                           /*  we found a match */
    }

    return spanxe_next(self);                  /*  scan to next match */
}

static int spanxe_doc(FrtSpanEnum *self)
{
    FrtSpanEnum *inc = SpXEn(self)->inc;
    return inc->doc(inc);
}

static int spanxe_start(FrtSpanEnum *self)
{
    FrtSpanEnum *inc = SpXEn(self)->inc;
    return inc->start(inc);
}

static int spanxe_end(FrtSpanEnum *self)
{
    FrtSpanEnum *inc = SpXEn(self)->inc;
    return inc->end(inc);
}

static char *spanxe_to_s(FrtSpanEnum *self)
{
    char *query_str = self->query->to_s(self->query, (FrtSymbol)NULL);
    char *res = frt_strfmt("SpanNotEnum(%s)", query_str);
    free(query_str);
    return res;
}

static void spanxe_destroy(FrtSpanEnum *self)
{
    SpanNotEnum *sxe = SpXEn(self);
    sxe->inc->destroy(sxe->inc);
    sxe->exc->destroy(sxe->exc);
    free(self);
}

static FrtSpanEnum *spanxe_new(FrtQuery *query, FrtIndexReader *ir)
{
    FrtSpanEnum *self      = (FrtSpanEnum *)FRT_ALLOC(SpanNotEnum);
    SpanNotEnum *sxe    = SpXEn(self);
    FrtSpanNotQuery *sxq   = SpXQ(query);

    sxe->inc            = SpQ(sxq->inc)->get_spans(sxq->inc, ir);
    sxe->exc            = SpQ(sxq->exc)->get_spans(sxq->exc, ir);
    sxe->more_inc       = true;
    sxe->more_exc       = sxe->exc->next(sxe->exc);

    self->query         = query;
    self->next          = &spanxe_next;
    self->skip_to       = &spanxe_skip_to;
    self->doc           = &spanxe_doc;
    self->start         = &spanxe_start;
    self->end           = &spanxe_end;
    self->destroy       = &spanxe_destroy;
    self->to_s          = &spanxe_to_s;

    return self;
}

/*****************************************************************************
 *
 * SpanWeight
 *
 *****************************************************************************/

#define SpW(weight) ((SpanWeight *)(weight))
typedef struct SpanWeight
{
    FrtWeight      super;
    FrtHashSet    *terms;
} SpanWeight;

static FrtExplanation *spanw_explain(FrtWeight *self, FrtIndexReader *ir, int target)
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
    const char *field_name = rb_id2name(SpQ(self->query)->field);

    char *query_str;
    FrtHashSet *terms = SpW(self)->terms;
    const int field_num = frt_fis_get_field_num(ir->fis, SpQ(self->query)->field);
    char *doc_freqs = NULL;
    size_t df_i = 0;
    FrtHashSetEntry *hse;

    if (field_num < 0) {
        return frt_expl_new(0.0, "field \"%s\" does not exist in the index", field_name);
    }

    query_str = self->query->to_s(self->query, (FrtSymbol)NULL);

    for (hse = terms->first; hse; hse = hse->next) {
        char *term = (char *)hse->elem;
        FRT_REALLOC_N(doc_freqs, char, df_i + strlen(term) + 23);
        df_i += sprintf(doc_freqs + df_i, "%s=%d, ", term,
                        ir->doc_freq(ir, field_num, term));
    }
    /* remove the ',' at the end of the string if it exists */
    if (terms->size > 0) {
        df_i -= 2;
        doc_freqs[df_i] = '\0';
    }
    else {
        doc_freqs = frt_estrdup("");
    }

    expl = frt_expl_new(0.0, "weight(%s in %d), product of:", query_str, target);

    /* We need two of these as it's included in both the query explanation
     * and the field explanation */
    idf_expl1 = frt_expl_new(self->idf, "idf(%s: %s)", field_name, doc_freqs);
    idf_expl2 = frt_expl_new(self->idf, "idf(%s: %s)", field_name, doc_freqs);
    if (terms->size > 0) {
        free(doc_freqs); /* only free if allocated */
    }

    /* explain query weight */
    query_expl = frt_expl_new(0.0, "query_weight(%s), product of:", query_str);

    if (self->query->boost != 1.0) {
        frt_expl_add_detail(query_expl, frt_expl_new(self->query->boost, "boost"));
    }

    frt_expl_add_detail(query_expl, idf_expl1);

    qnorm_expl = frt_expl_new(self->qnorm, "query_norm");
    frt_expl_add_detail(query_expl, qnorm_expl);

    query_expl->value = self->query->boost * idf_expl1->value * qnorm_expl->value;

    frt_expl_add_detail(expl, query_expl);

    /* explain field weight */
    field_expl = frt_expl_new(0.0, "field_weight(%s:%s in %d), product of:", field_name, query_str, target);
    free(query_str);

    scorer = self->scorer(self, ir);
    tf_expl = scorer->explain(scorer, target);
    scorer->destroy(scorer);
    frt_expl_add_detail(field_expl, tf_expl);
    frt_expl_add_detail(field_expl, idf_expl2);

    field_norms = ir->get_norms(ir, field_num);
    field_norm = (field_norms
                  ? frt_sim_decode_norm(self->similarity, field_norms[target])
                  : (float)0.0);
    field_norm_expl = frt_expl_new(field_norm, "field_norm(field=%s, doc=%d)",
                               field_name, target);
    frt_expl_add_detail(field_expl, field_norm_expl);

    field_expl->value = tf_expl->value * idf_expl2->value * field_norm_expl->value;

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

static char *spanw_to_s(FrtWeight *self)
{
    return frt_strfmt("SpanWeight(%f)", self->value);
}

static void spanw_destroy(FrtWeight *self)
{
    frt_hs_destroy(SpW(self)->terms);
    frt_w_destroy(self);
}

static FrtWeight *spanw_new(FrtQuery *query, FrtSearcher *searcher)
{
    FrtHashSetEntry *hse;
    FrtWeight *self        = w_new(SpanWeight, query);
    FrtHashSet *terms      = SpQ(query)->get_terms(query);

    SpW(self)->terms    = terms;
    self->scorer        = &spansc_new;
    self->explain       = &spanw_explain;
    self->to_s          = &spanw_to_s;
    self->destroy       = &spanw_destroy;

    self->similarity    = query->get_similarity(query, searcher);

    self->idf           = 0.0f;

    for (hse = terms->first; hse; hse = hse->next) {
        self->idf += frt_sim_idf_term(self->similarity, SpQ(query)->field,
                                  (char *)hse->elem, searcher);
    }

    return self;
}

/*****************************************************************************
 * FrtSpanTermQuery
 *****************************************************************************/

static char *spantq_to_s(FrtQuery *self, FrtSymbol default_field) {
    if (default_field && default_field == SpQ(self)->field) {
        return frt_strfmt("span_terms(%s)", SpTQ(self)->term);
    } else {
        return frt_strfmt("span_terms(%s:%s)", rb_id2name(SpQ(self)->field), SpTQ(self)->term);
    }
}

static void spantq_destroy_i(FrtQuery *self) {
    free(SpTQ(self)->term);
    spanq_destroy_i(self);
}

static void spantq_extract_terms(FrtQuery *self, FrtHashSet *terms) {
    frt_hs_add(terms, frt_term_new(SpQ(self)->field, SpTQ(self)->term));
}

static FrtHashSet *spantq_get_terms(FrtQuery *self) {
    FrtHashSet *terms = frt_hs_new_str(&free);
    frt_hs_add(terms, frt_estrdup(SpTQ(self)->term));
    return terms;
}

static unsigned long long spantq_hash(FrtQuery *self) {
    return spanq_hash(self) ^ frt_str_hash(SpTQ(self)->term);
}

static int spantq_eq(FrtQuery *self, FrtQuery *o) {
    return spanq_eq(self, o) && strcmp(SpTQ(self)->term, SpTQ(o)->term) == 0;
}

FrtQuery *frt_spantq_alloc(void) {
    return frt_q_new(FrtSpanTermQuery);
}

FrtQuery *frt_spantq_init(FrtQuery *self, FrtSymbol field, const char *term) {
    SpTQ(self)->term        = frt_estrdup(term);
    SpQ(self)->field        = field;
    SpQ(self)->get_spans    = &spante_new;
    SpQ(self)->get_terms    = &spantq_get_terms;

    self->type              = SPAN_TERM_QUERY;
    self->extract_terms     = &spantq_extract_terms;
    self->to_s              = &spantq_to_s;
    self->hash              = &spantq_hash;
    self->eq                = &spantq_eq;
    self->destroy_i         = &spantq_destroy_i;
    self->create_weight_i   = &spanw_new;
    self->get_matchv_i      = &spanq_get_matchv_i;
    return self;
}

FrtQuery *frt_spantq_new(FrtSymbol field, const char *term) {
    FrtQuery *self = frt_spantq_alloc();
    return frt_spantq_init(self, field, term);
}

/*****************************************************************************
 * SpanMultiTermQuery
 *****************************************************************************/

static char *spanmtq_to_s(FrtQuery *self, FrtSymbol field) {
    char *terms = NULL, *p;
    int len = 3, i;
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    for (i = 0; i < smtq->term_cnt; i++) {
        len += strlen(smtq->terms[i]) + 2;
    }
    p = terms = FRT_ALLOC_N(char, len);
    *(p++) = '[';
    for (i = 0; i < smtq->term_cnt; i++) {
        if (i != 0) *(p++) = ',';
        strcpy(p, smtq->terms[i]);
        p += strlen(smtq->terms[i]);
    }
    *(p++) = ']';
    *p = '\0';

    if (field == SpQ(self)->field) {
        p = frt_strfmt("span_terms(%s)", terms);
    } else {
        p = frt_strfmt("span_terms(%s:%s)", rb_id2name(SpQ(self)->field), terms);
    }
    free(terms);
    return p;
}

static void spanmtq_destroy_i(FrtQuery *self) {
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    int i;
    for (i = 0; i < smtq->term_cnt; i++) {
        free(smtq->terms[i]);
    }
    free(smtq->terms);
    spanq_destroy_i(self);
}

static void spanmtq_extract_terms(FrtQuery *self, FrtHashSet *terms) {
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    int i;
    for (i = 0; i < smtq->term_cnt; i++) {
        frt_hs_add(terms, frt_term_new(SpQ(self)->field, smtq->terms[i]));
    }
}

static FrtHashSet *spanmtq_get_terms(FrtQuery *self) {
    FrtHashSet *terms = frt_hs_new_str(&free);
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    int i;
    for (i = 0; i < smtq->term_cnt; i++) {
        frt_hs_add(terms, frt_estrdup(smtq->terms[i]));
    }
    return terms;
}

static unsigned long long spanmtq_hash(FrtQuery *self) {
    unsigned long long hash = spanq_hash(self);
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    int i;
    for (i = 0; i < smtq->term_cnt; i++) {
        hash ^= frt_str_hash(smtq->terms[i]);
    }
    return hash;
}

static int spanmtq_eq(FrtQuery *self, FrtQuery *o) {
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    FrtSpanMultiTermQuery *smtqo = SpMTQ(o);
    int i;
    if (!spanq_eq(self, o)) return false;
    if (smtq->term_cnt != smtqo->term_cnt) return false;
    for (i = 0; i < smtq->term_cnt; i++) {
        if (strcmp(smtq->terms[i], smtqo->terms[i]) != 0) return false;
    }
    return true;;
}

FrtQuery *frt_spanmtq_alloc(void) {
    return frt_q_new(FrtSpanMultiTermQuery);
}

FrtQuery *frt_spanmtq_init_conf(FrtQuery *self, FrtSymbol field, int max_terms) {
    SpMTQ(self)->terms      = FRT_ALLOC_N(char *, max_terms);
    SpMTQ(self)->term_cnt   = 0;
    SpMTQ(self)->term_capa  = max_terms;

    SpQ(self)->field        = field;
    SpQ(self)->get_spans    = &spanmte_new;
    SpQ(self)->get_terms    = &spanmtq_get_terms;

    self->type              = SPAN_MULTI_TERM_QUERY;
    self->extract_terms     = &spanmtq_extract_terms;
    self->to_s              = &spanmtq_to_s;
    self->hash              = &spanmtq_hash;
    self->eq                = &spanmtq_eq;
    self->destroy_i         = &spanmtq_destroy_i;
    self->create_weight_i   = &spanw_new;
    self->get_matchv_i      = &spanq_get_matchv_i;

    return self;
}

FrtQuery *frt_spanmtq_new_conf(FrtSymbol field, int max_terms) {
    FrtQuery *self = frt_spanmtq_alloc();
    return frt_spanmtq_init_conf(self, field, max_terms);
}

FrtQuery *frt_spanmtq_init(FrtQuery *self, FrtSymbol field) {
    return frt_spanmtq_init_conf(self, field, SPAN_MULTI_TERM_QUERY_CAPA);
}

FrtQuery *frt_spanmtq_new(FrtSymbol field) {
    return frt_spanmtq_new_conf(field, SPAN_MULTI_TERM_QUERY_CAPA);
}

void frt_spanmtq_add_term(FrtQuery *self, const char *term) {
    FrtSpanMultiTermQuery *smtq = SpMTQ(self);
    if (smtq->term_cnt < smtq->term_capa) {
        smtq->terms[smtq->term_cnt++] = frt_estrdup(term);
    }
}

/*****************************************************************************
 *
 * SpanFirstQuery
 *
 *****************************************************************************/

static char *spanfq_to_s(FrtQuery *self, FrtSymbol field) {
    FrtQuery *match = SpFQ(self)->match;
    char *q_str = match->to_s(match, field);
    char *res = frt_strfmt("span_first(%s, %d)", q_str, SpFQ(self)->end);
    free(q_str);
    return res;
}

static void spanfq_extract_terms(FrtQuery *self, FrtHashSet *terms) {
    SpFQ(self)->match->extract_terms(SpFQ(self)->match, terms);
}

static FrtHashSet *spanfq_get_terms(FrtQuery *self) {
    FrtSpanFirstQuery *sfq = SpFQ(self);
    return SpQ(sfq->match)->get_terms(sfq->match);
}

static FrtQuery *spanfq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtQuery *q, *rq;

    q = SpFQ(self)->match;
    rq = q->rewrite(q, ir);
    frt_q_deref(q);
    SpFQ(self)->match = rq;

    self->ref_cnt++;
    return self;                    /* no clauses rewrote */
}

static void spanfq_destroy_i(FrtQuery *self) {
    frt_q_deref(SpFQ(self)->match);
    spanq_destroy_i(self);
}

static unsigned long long spanfq_hash(FrtQuery *self) {
    return spanq_hash(self) ^ SpFQ(self)->match->hash(SpFQ(self)->match)
        ^ SpFQ(self)->end;
}

static int spanfq_eq(FrtQuery *self, FrtQuery *o) {
    FrtSpanFirstQuery *sfq1 = SpFQ(self);
    FrtSpanFirstQuery *sfq2 = SpFQ(o);
    return spanq_eq(self, o) && sfq1->match->eq(sfq1->match, sfq2->match)
        && (sfq1->end == sfq2->end);
}

FrtQuery *frt_spanfq_alloc(void) {
    return frt_q_new(FrtSpanFirstQuery);
}

FrtQuery *frt_spanfq_init_nr(FrtQuery *self, FrtQuery *match, int end) {
    SpFQ(self)->match       = match;
    SpFQ(self)->end         = end;

    SpQ(self)->field        = SpQ(match)->field;
    SpQ(self)->get_spans    = &spanfe_new;
    SpQ(self)->get_terms    = &spanfq_get_terms;

    self->type              = SPAN_FIRST_QUERY;
    self->rewrite           = &spanfq_rewrite;
    self->extract_terms     = &spanfq_extract_terms;
    self->to_s              = &spanfq_to_s;
    self->hash              = &spanfq_hash;
    self->eq                = &spanfq_eq;
    self->destroy_i         = &spanfq_destroy_i;
    self->create_weight_i   = &spanw_new;
    self->get_matchv_i      = &spanq_get_matchv_i;

    return self;
}

FrtQuery *frt_spanfq_new_nr(FrtQuery *match, int end) {
    FrtQuery *self = frt_spanfq_alloc();
    return frt_spanfq_init_nr(self, match, end);
}

FrtQuery *frt_spanfq_init(FrtQuery *self, FrtQuery *match, int end) {
    FRT_REF(match);
    return frt_spanfq_init_nr(self, match, end);
}

FrtQuery *frt_spanfq_new(FrtQuery *match, int end) {
    FRT_REF(match);
    return frt_spanfq_new_nr(match, end);
}

/*****************************************************************************
 *
 * FrtSpanOrQuery
 *
 *****************************************************************************/

static char *spanoq_to_s(FrtQuery *self, FrtSymbol field) {
    int i;
    FrtSpanOrQuery *soq = SpOQ(self);
    char *res, *res_p;
    char **q_strs = FRT_ALLOC_N(char *, soq->c_cnt);
    int len = 50;
    for (i = 0; i < soq->c_cnt; i++) {
        FrtQuery *clause = soq->clauses[i];
        q_strs[i] = clause->to_s(clause, field);
        len += strlen(q_strs[i])  + 2;
    }

    res_p = res = FRT_ALLOC_N(char, len);
    res_p += sprintf(res_p, "span_or[");
    for (i = 0; i < soq->c_cnt; i++) {
        if (i != 0) *(res_p++) = ',';
        res_p += sprintf(res_p, "%s", q_strs[i]);
        free(q_strs[i]);
    }
    free(q_strs);

    *(res_p)++ = ']';
    *res_p = 0;
    return res;
}

static void spanoq_extract_terms(FrtQuery *self, FrtHashSet *terms) {
    FrtSpanOrQuery *soq = SpOQ(self);
    int i;
    for (i = 0; i < soq->c_cnt; i++) {
        FrtQuery *clause = soq->clauses[i];
        clause->extract_terms(clause, terms);
    }
}

static FrtHashSet *spanoq_get_terms(FrtQuery *self) {
    FrtSpanOrQuery *soq = SpOQ(self);
    FrtHashSet *terms = frt_hs_new_str(&free);
    int i;
    for (i = 0; i < soq->c_cnt; i++) {
        FrtQuery *clause = soq->clauses[i];
        FrtHashSet *sub_terms = SpQ(clause)->get_terms(clause);
        frt_hs_merge(terms, sub_terms);
    }

    return terms;
}

static FrtSpanEnum *spanoq_get_spans(FrtQuery *self, FrtIndexReader *ir) {
    FrtSpanOrQuery *soq = SpOQ(self);
    if (soq->c_cnt == 1) {
        FrtQuery *q = soq->clauses[0];
        return SpQ(q)->get_spans(q, ir);
    }

    return spanoe_new(self, ir);
}

static FrtQuery *spanoq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtSpanOrQuery *soq = SpOQ(self);
    int i;

    /* replace clauses with their rewritten queries */
    for (i = 0; i < soq->c_cnt; i++) {
        FrtQuery *clause = soq->clauses[i];
        FrtQuery *rewritten = clause->rewrite(clause, ir);
        frt_q_deref(clause);
        soq->clauses[i] = rewritten;
    }

    self->ref_cnt++;
    return self;
}

static void spanoq_destroy_i(FrtQuery *self) {
    FrtSpanOrQuery *soq = SpOQ(self);

    int i;
    for (i = 0; i < soq->c_cnt; i++) {
        FrtQuery *clause = soq->clauses[i];
        frt_q_deref(clause);
    }
    free(soq->clauses);

    spanq_destroy_i(self);
}

static unsigned long long spanoq_hash(FrtQuery *self) {
    int i;
    unsigned long long hash = spanq_hash(self);
    FrtSpanOrQuery *soq = SpOQ(self);

    for (i = 0; i < soq->c_cnt; i++) {
        FrtQuery *q = soq->clauses[i];
        hash ^= q->hash(q);
    }
    return hash;
}

static int spanoq_eq(FrtQuery *self, FrtQuery *o) {
    int i;
    FrtQuery *q1, *q2;
    FrtSpanOrQuery *soq1 = SpOQ(self);
    FrtSpanOrQuery *soq2 = SpOQ(o);

    if (!spanq_eq(self, o) || soq1->c_cnt != soq2->c_cnt) {
        return false;
    }
    for (i = 0; i < soq1->c_cnt; i++) {
        q1 = soq1->clauses[i];
        q2 = soq2->clauses[i];
        if (!q1->eq(q1, q2)) {
            return false;
        }
    }
    return true;
}

FrtQuery *frt_spanoq_alloc(void) {
    return frt_q_new(FrtSpanOrQuery);
}

FrtQuery *frt_spanoq_init(FrtQuery *self) {
    SpOQ(self)->clauses     = FRT_ALLOC_N(FrtQuery *, CLAUSE_INIT_CAPA);
    SpOQ(self)->c_capa      = CLAUSE_INIT_CAPA;

    SpQ(self)->field        = (FrtSymbol)NULL;
    SpQ(self)->get_spans    = &spanoq_get_spans;
    SpQ(self)->get_terms    = &spanoq_get_terms;

    self->type              = SPAN_OR_QUERY;
    self->rewrite           = &spanoq_rewrite;
    self->extract_terms     = &spanoq_extract_terms;
    self->to_s              = &spanoq_to_s;
    self->hash              = &spanoq_hash;
    self->eq                = &spanoq_eq;
    self->destroy_i         = &spanoq_destroy_i;
    self->create_weight_i   = &spanw_new;
    self->get_matchv_i      = &spanq_get_matchv_i;

    return self;
}

FrtQuery *frt_spanoq_new(void) {
    FrtQuery *self = frt_spanoq_alloc();
    return frt_spanoq_init(self);
}

FrtQuery *frt_spanoq_add_clause_nr(FrtQuery *self, FrtQuery *clause)
{
    const int curr_index = SpOQ(self)->c_cnt++;
    if (clause->type < SPAN_TERM_QUERY || clause->type > SPAN_NEAR_QUERY) {
        FRT_RAISE(FRT_ARG_ERROR, "Tried to add a %s to a SpanOrQuery. This is not a "
              "SpanQuery.", frt_q_get_query_name(clause->type));
    }
    if (curr_index == 0) {
        SpQ(self)->field = SpQ(clause)->field;
    }
    else if (SpQ(self)->field != SpQ(clause)->field) {
        FRT_RAISE(FRT_ARG_ERROR, "All clauses in a SpanQuery must have the same field. "
              "Attempted to add a SpanQuery with field \"%s\" to a SpanOrQuery "
              "with field \"%s\"", rb_id2name(SpQ(clause)->field), rb_id2name(SpQ(self)->field));
    }
    if (curr_index >= SpOQ(self)->c_capa) {
        SpOQ(self)->c_capa <<= 1;
        FRT_REALLOC_N(SpOQ(self)->clauses, FrtQuery *, SpOQ(self)->c_capa);
    }
    SpOQ(self)->clauses[curr_index] = clause;
    return clause;
}

FrtQuery *frt_spanoq_add_clause(FrtQuery *self, FrtQuery *clause)
{
    FRT_REF(clause);
    return frt_spanoq_add_clause_nr(self, clause);
}

/*****************************************************************************
 *
 * SpanNearQuery
 *
 *****************************************************************************/

static char *spannq_to_s(FrtQuery *self, FrtSymbol field)
{
    int i;
    FrtSpanNearQuery *snq = SpNQ(self);
    char *res, *res_p;
    char **q_strs = FRT_ALLOC_N(char *, snq->c_cnt);
    int len = 50;
    for (i = 0; i < snq->c_cnt; i++) {
        FrtQuery *clause = snq->clauses[i];
        q_strs[i] = clause->to_s(clause, field);
        len += strlen(q_strs[i]);
    }

    res_p = res = FRT_ALLOC_N(char, len);
    res_p += sprintf(res_p, "span_near[");
    for (i = 0; i < snq->c_cnt; i++) {
        if (i != 0) *(res_p)++ = ',';
        res_p += sprintf(res_p, "%s", q_strs[i]);
        free(q_strs[i]);
    }
    free(q_strs);

    *(res_p++) = ']';
    *res_p = 0;
    return res;
}

static void spannq_extract_terms(FrtQuery *self, FrtHashSet *terms)
{
    FrtSpanNearQuery *snq = SpNQ(self);
    int i;
    for (i = 0; i < snq->c_cnt; i++) {
        FrtQuery *clause = snq->clauses[i];
        clause->extract_terms(clause, terms);
    }
}

static FrtHashSet *spannq_get_terms(FrtQuery *self)
{
    FrtSpanNearQuery *snq = SpNQ(self);
    FrtHashSet *terms = frt_hs_new_str(&free);
    int i;
    for (i = 0; i < snq->c_cnt; i++) {
        FrtQuery *clause = snq->clauses[i];
        FrtHashSet *sub_terms = SpQ(clause)->get_terms(clause);
        frt_hs_merge(terms, sub_terms);
    }

    return terms;
}

static FrtSpanEnum *spannq_get_spans(FrtQuery *self, FrtIndexReader *ir)
{
    FrtSpanNearQuery *snq = SpNQ(self);

    if (snq->c_cnt == 1) {
        FrtQuery *q = snq->clauses[0];
        return SpQ(q)->get_spans(q, ir);
    }

    return spanne_new(self, ir);
}

static FrtQuery *spannq_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    FrtSpanNearQuery *snq = SpNQ(self);
    int i;
    for (i = 0; i < snq->c_cnt; i++) {
        FrtQuery *clause = snq->clauses[i];
        FrtQuery *rewritten = clause->rewrite(clause, ir);
        frt_q_deref(clause);
        snq->clauses[i] = rewritten;
    }

    self->ref_cnt++;
    return self;
}

static void spannq_destroy(FrtQuery *self)
{
    FrtSpanNearQuery *snq = SpNQ(self);

    int i;
    for (i = 0; i < snq->c_cnt; i++) {
        FrtQuery *clause = snq->clauses[i];
        frt_q_deref(clause);
    }
    free(snq->clauses);

    spanq_destroy_i(self);
}

static unsigned long long spannq_hash(FrtQuery *self)
{
    int i;
    unsigned long long hash = spanq_hash(self);
    FrtSpanNearQuery *snq = SpNQ(self);

    for (i = 0; i < snq->c_cnt; i++) {
        FrtQuery *q = snq->clauses[i];
        hash ^= q->hash(q);
    }
    return ((hash ^ snq->slop) << 1) | snq->in_order;
}

static int spannq_eq(FrtQuery *self, FrtQuery *o)
{
    int i;
    FrtQuery *q1, *q2;
    FrtSpanNearQuery *snq1 = SpNQ(self);
    FrtSpanNearQuery *snq2 = SpNQ(o);
    if (! spanq_eq(self, o)
        || (snq1->c_cnt != snq2->c_cnt)
        || (snq1->slop != snq2->slop)
        || (snq1->in_order != snq2->in_order)) {
        return false;
    }

    for (i = 0; i < snq1->c_cnt; i++) {
        q1 = snq1->clauses[i];
        q2 = snq2->clauses[i];
        if (!q1->eq(q1, q2)) {
            return false;
        }
    }

    return true;
}

FrtQuery *frt_spannq_alloc(void) {
    return frt_q_new(FrtSpanNearQuery);
}

FrtQuery *frt_spannq_init(FrtQuery *self, int slop, bool in_order) {
    SpNQ(self)->clauses     = FRT_ALLOC_N(FrtQuery *, CLAUSE_INIT_CAPA);
    SpNQ(self)->c_capa      = CLAUSE_INIT_CAPA;
    SpNQ(self)->slop        = slop;
    SpNQ(self)->in_order    = in_order;

    SpQ(self)->get_spans    = &spannq_get_spans;
    SpQ(self)->get_terms    = &spannq_get_terms;
    SpQ(self)->field        = (FrtSymbol)NULL;

    self->type              = SPAN_NEAR_QUERY;
    self->rewrite           = &spannq_rewrite;
    self->extract_terms     = &spannq_extract_terms;
    self->to_s              = &spannq_to_s;
    self->hash              = &spannq_hash;
    self->eq                = &spannq_eq;
    self->destroy_i         = &spannq_destroy;
    self->create_weight_i   = &spanw_new;
    self->get_matchv_i      = &spanq_get_matchv_i;

    return self;
}

FrtQuery *frt_spannq_new(int slop, bool in_order) {
    FrtQuery *self = frt_spannq_alloc();
    return frt_spannq_init(self, slop, in_order);
}

FrtQuery *frt_spannq_add_clause_nr(FrtQuery *self, FrtQuery *clause)
{
    const int curr_index = SpNQ(self)->c_cnt++;
    if (clause->type < SPAN_TERM_QUERY || clause->type > SPAN_NEAR_QUERY) {
        FRT_RAISE(FRT_ARG_ERROR, "Tried to add a %s to a SpanNearQuery. This is not a "
              "SpanQuery.", frt_q_get_query_name(clause->type));
    }
    if (curr_index == 0) {
        SpQ(self)->field = SpQ(clause)->field;
    }
    else if (SpQ(self)->field != SpQ(clause)->field) {
        FRT_RAISE(FRT_ARG_ERROR, "All clauses in a SpanQuery must have the same field. "
              "Attempted to add a SpanQuery with field \"%s\" to SpanNearQuery "
              "with field \"%s\"", rb_id2name(SpQ(clause)->field), rb_id2name(SpQ(self)->field));
    }
    if (curr_index >= SpNQ(self)->c_capa) {
        SpNQ(self)->c_capa <<= 1;
        FRT_REALLOC_N(SpNQ(self)->clauses, FrtQuery *, SpNQ(self)->c_capa);
    }
    SpNQ(self)->clauses[curr_index] = clause;
    return clause;
}

FrtQuery *frt_spannq_add_clause(FrtQuery *self, FrtQuery *clause)
{
    FRT_REF(clause);
    return frt_spannq_add_clause_nr(self, clause);
}

/*****************************************************************************
 *
 * FrtSpanNotQuery
 *
 *****************************************************************************/

static char *spanxq_to_s(FrtQuery *self, FrtSymbol field) {
    FrtSpanNotQuery *sxq = SpXQ(self);
    char *inc_s = sxq->inc->to_s(sxq->inc, field);
    char *exc_s = sxq->exc->to_s(sxq->exc, field);
    char *res = frt_strfmt("span_not(inc:<%s>, exc:<%s>)", inc_s, exc_s);

    free(inc_s);
    free(exc_s);
    return res;
}

static void spanxq_extract_terms(FrtQuery *self, FrtHashSet *terms) {
    SpXQ(self)->inc->extract_terms(SpXQ(self)->inc, terms);
}

static FrtHashSet *spanxq_get_terms(FrtQuery *self) {
    return SpQ(SpXQ(self)->inc)->get_terms(SpXQ(self)->inc);
}

static FrtQuery *spanxq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtSpanNotQuery *sxq = SpXQ(self);
    FrtQuery *q, *rq;

    /* rewrite inclusive query */
    q = sxq->inc;
    rq = q->rewrite(q, ir);
    frt_q_deref(q);
    sxq->inc = rq;

    /* rewrite exclusive query */
    q = sxq->exc;
    rq = q->rewrite(q, ir);
    frt_q_deref(q);
    sxq->exc = rq;

    self->ref_cnt++;
    return self;
}

static void spanxq_destroy(FrtQuery *self) {
    FrtSpanNotQuery *sxq = SpXQ(self);

    frt_q_deref(sxq->inc);
    frt_q_deref(sxq->exc);

    spanq_destroy_i(self);
}

static unsigned long long spanxq_hash(FrtQuery *self) {
    FrtSpanNotQuery *sxq = SpXQ(self);
    return spanq_hash(self) ^ sxq->inc->hash(sxq->inc)
        ^ sxq->exc->hash(sxq->exc);
}

static int spanxq_eq(FrtQuery *self, FrtQuery *o) {
    FrtSpanNotQuery *sxq1 = SpXQ(self);
    FrtSpanNotQuery *sxq2 = SpXQ(o);
    return spanq_eq(self, o) && sxq1->inc->eq(sxq1->inc, sxq2->inc)
        && sxq1->exc->eq(sxq1->exc, sxq2->exc);
}

FrtQuery *frt_spanxq_alloc(void) {
    return frt_q_new(FrtSpanNotQuery);
}

FrtQuery *frt_spanxq_init_nr(FrtQuery *self, FrtQuery *inc, FrtQuery *exc) {
    if (SpQ(inc)->field != SpQ(exc)->field) {
        free(self);
        FRT_RAISE(FRT_ARG_ERROR, "All clauses in a SpanQuery must have the same field. "
              "Attempted to add a SpanQuery with field \"%s\" along with a "
              "SpanQuery with field \"%s\" to an SpanNotQuery",
              rb_id2name(SpQ(inc)->field), rb_id2name(SpQ(exc)->field));
    }

    SpXQ(self)->inc         = inc;
    SpXQ(self)->exc         = exc;

    SpQ(self)->field        = SpQ(inc)->field;
    SpQ(self)->get_spans    = &spanxe_new;
    SpQ(self)->get_terms    = &spanxq_get_terms;

    self->type              = SPAN_NOT_QUERY;
    self->rewrite           = &spanxq_rewrite;
    self->extract_terms     = &spanxq_extract_terms;
    self->to_s              = &spanxq_to_s;
    self->hash              = &spanxq_hash;
    self->eq                = &spanxq_eq;
    self->destroy_i         = &spanxq_destroy;
    self->create_weight_i   = &spanw_new;
    self->get_matchv_i      = &spanq_get_matchv_i;

    return self;
}

FrtQuery *frt_spanxq_new_nr(FrtQuery *inc, FrtQuery *exc) {
    FrtQuery *self = frt_spanxq_alloc();
    return frt_spanxq_init_nr(self, inc, exc);
}

FrtQuery *frt_spanxq_init(FrtQuery *self, FrtQuery *inc, FrtQuery *exc) {
    FRT_REF(inc);
    FRT_REF(exc);
    return frt_spanxq_init_nr(self, inc, exc);
}

FrtQuery *frt_spanxq_new(FrtQuery *inc, FrtQuery *exc) {
    FRT_REF(inc);
    FRT_REF(exc);
    return frt_spanxq_new_nr(inc, exc);
}

/*****************************************************************************
 *
 * Rewritables
 *
 *****************************************************************************/

/*****************************************************************************
 *
 * FrtSpanPrefixQuery
 *
 *****************************************************************************/

#define SpPfxQ(query) ((FrtSpanPrefixQuery *)(query))

static char *spanprq_to_s(FrtQuery *self, FrtSymbol default_field) {
    char *buffer, *bptr;
    const char *prefix = SpPfxQ(self)->prefix;
    size_t plen = strlen(prefix);
    FrtSymbol field = SpQ(self)->field;
    const char *field_name = rb_id2name(field);
    size_t flen = strlen(field_name);


    bptr = buffer = FRT_ALLOC_N(char, plen + flen + 35);

    if (default_field == (FrtSymbol)NULL || (field != default_field)) {
        bptr += sprintf(bptr, "%s:", field_name);
    }

    bptr += sprintf(bptr, "%s*", prefix);
    if (self->boost != 1.0) {
        *bptr = '^';
        frt_dbl_to_s(++bptr, self->boost);
    }

    return buffer;
}

static FrtQuery *spanprq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    const int field_num = frt_fis_get_field_num(ir->fis, SpQ(self)->field);
    FrtQuery *volatile q = frt_spanmtq_new_conf(SpQ(self)->field, SpPfxQ(self)->max_terms);
    q->boost = self->boost;        /* set the boost */

    if (field_num >= 0) {
        const char *prefix = SpPfxQ(self)->prefix;
        FrtTermEnum *te = ir->terms_from(ir, field_num, prefix);
        const char *term = te->curr_term;
        size_t prefix_len = strlen(prefix);

        FRT_TRY
            do {
                if (strncmp(term, prefix, prefix_len) != 0) {
                    break;
                }
                frt_spanmtq_add_term(q, term);       /* found a match */
            } while (te->next(te));
        FRT_XFINALLY
            te->close(te);
        FRT_XENDTRY
    }

    return q;
}

static void spanprq_destroy(FrtQuery *self) {
    free(SpPfxQ(self)->prefix);
    spanq_destroy_i(self);
}

static unsigned long long spanprq_hash(FrtQuery *self) {
    return frt_str_hash(rb_id2name(SpQ(self)->field)) ^ frt_str_hash(SpPfxQ(self)->prefix);
}

static int spanprq_eq(FrtQuery *self, FrtQuery *o) {
    return (strcmp(SpPfxQ(self)->prefix, SpPfxQ(o)->prefix) == 0)
        && (SpQ(self)->field == SpQ(o)->field);
}

FrtQuery *frt_spanprq_alloc(void) {
    return frt_q_new(FrtSpanPrefixQuery);
}

FrtQuery *frt_spanprq_init(FrtQuery *self, FrtSymbol field, const char *prefix) {
    SpQ(self)->field        = field;
    SpPfxQ(self)->prefix    = frt_estrdup(prefix);
    SpPfxQ(self)->max_terms = FRT_SPAN_PREFIX_QUERY_MAX_TERMS;

    self->type              = SPAN_PREFIX_QUERY;
    self->rewrite           = &spanprq_rewrite;
    self->to_s              = &spanprq_to_s;
    self->hash              = &spanprq_hash;
    self->eq                = &spanprq_eq;
    self->destroy_i         = &spanprq_destroy;
    self->create_weight_i   = &frt_q_create_weight_unsup;

    return self;
}

FrtQuery *frt_spanprq_new(FrtSymbol field, const char *prefix) {
    FrtQuery *self = frt_spanprq_alloc();
    return frt_spanprq_init(self, field, prefix);
}
