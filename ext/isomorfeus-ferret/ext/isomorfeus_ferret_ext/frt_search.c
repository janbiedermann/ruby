#include <string.h>
#include <limits.h>
#include "frt_search.h"
#include "frt_array.h"

#undef close

/***************************************************************************
 *
 * FrtExplanation - Used to give details for query scores
 *
 ***************************************************************************/

FrtExplanation *frt_expl_new(float value, const char *description, ...)
{
    FrtExplanation *expl = FRT_ALLOC(FrtExplanation);

    va_list args;
    va_start(args, description);
    expl->description = frt_vstrfmt(description, args);
    va_end(args);

    expl->value = value;
    expl->details = frt_ary_new_type_capa(FrtExplanation *,
                                      FRT_EXPLANATION_DETAILS_START_SIZE);
    return expl;
}

void frt_expl_destroy(FrtExplanation *expl)
{
    frt_ary_destroy((void **)expl->details, (frt_free_ft)frt_expl_destroy);
    free(expl->description);
    free(expl);
}

FrtExplanation *frt_expl_add_detail(FrtExplanation *expl, FrtExplanation *detail)
{
    frt_ary_push(expl->details, detail);
    return expl;
}

char *frt_expl_to_s_depth(FrtExplanation *expl, int depth)
{
    int i;
    char *buffer = FRT_ALLOC_N(char, depth * 2 + 1);
    const int num_details = frt_ary_size(expl->details);

    memset(buffer, ' ', sizeof(char) * depth * 2);
    buffer[depth*2] = 0;

    buffer = frt_estrcat(buffer, frt_strfmt("%f = %s\n",
                                    expl->value, expl->description));
    for (i = 0; i < num_details; i++) {
        buffer = frt_estrcat(buffer, frt_expl_to_s_depth(expl->details[i], depth + 1));
    }

    return buffer;
}

char *frt_expl_to_html(FrtExplanation *expl)
{
    int i;
    char *buffer;
    const int num_details = frt_ary_size(expl->details);

    buffer = frt_strfmt("<ul>\n<li>%f = %s</li>\n", expl->value, expl->description);

    for (i = 0; i < num_details; i++) {
        frt_estrcat(buffer, frt_expl_to_html(expl->details[i]));
    }

    FRT_REALLOC_N(buffer, char, strlen(buffer) + 10);
    return strcat(buffer, "</ul>\n");
}

/***************************************************************************
 *
 * Hit
 *
 ***************************************************************************/

static bool hit_lt(FrtHit *hit1, FrtHit *hit2)
{
    if (hit1->score == hit2->score) {
        return hit1->doc > hit2->doc;
    } else {
        return hit1->score < hit2->score;
    }
}

static void hit_pq_down(FrtPriorityQueue *pq)
{
    register int i = 1;
    register int j = 2;     /* i << 1; */
    register int k = 3;     /* j + 1;  */
    FrtHit **heap = (FrtHit **)pq->heap;
    FrtHit *node = heap[i];    /* save top node */

    if ((k <= pq->size) && hit_lt(heap[k], heap[j])) {
        j = k;
    }

    while ((j <= pq->size) && hit_lt(heap[j], node)) {
        heap[i] = heap[j];  /* shift up child */
        i = j;
        j = i << 1;
        k = j + 1;
        if ((k <= pq->size) && hit_lt(heap[k], heap[j])) {
            j = k;
        }
    }
    heap[i] = node;
}

static FrtHit *hit_pq_pop(FrtPriorityQueue *pq)
{
    if (pq->size > 0) {
        FrtHit **heap = (FrtHit **)pq->heap;
        FrtHit *result = heap[1];    /* save first value */
        heap[1] = heap[pq->size]; /* move last to first */
        heap[pq->size] = NULL;
        pq->size--;
        hit_pq_down(pq);          /* adjust heap */
        return result;
    }
    else {
        return NULL;
    }
}

static void hit_pq_up(FrtPriorityQueue *pq)
{
    FrtHit **heap = (FrtHit **)pq->heap;
    FrtHit *node;
    int i = pq->size;
    int j = i >> 1;
    node = heap[i];

    while ((j > 0) && hit_lt(node, heap[j])) {
        heap[i] = heap[j];
        i = j;
        j = j >> 1;
    }
    heap[i] = node;
}

static void hit_pq_insert(FrtPriorityQueue *pq, FrtHit *hit)
{
    if (pq->size < pq->capa) {
        FrtHit *new_hit = FRT_ALLOC(FrtHit);
        memcpy(new_hit, hit, sizeof(FrtHit));
        pq->size++;
        if (pq->size >= pq->mem_capa) {
            pq->mem_capa <<= 1;
            FRT_REALLOC_N(pq->heap, void *, pq->mem_capa);
        }
        pq->heap[pq->size] = new_hit;
        hit_pq_up(pq);
    } else if (pq->size > 0 && hit_lt((FrtHit *)pq->heap[1], hit)) {
        memcpy(pq->heap[1], hit, sizeof(FrtHit));
        hit_pq_down(pq);
    }
}

static void hit_pq_multi_insert(FrtPriorityQueue *pq, FrtHit *hit)
{
    hit_pq_insert(pq, hit);
    free(hit);
}

/***************************************************************************
 *
 * TopDocs
 *
 ***************************************************************************/

FrtTopDocs *frt_td_new(int total_hits, int size, FrtHit **hits, float max_score)
{
    FrtTopDocs *td = FRT_ALLOC(FrtTopDocs);
    td->total_hits = total_hits;
    td->size = size;
    td->hits = hits;
    td->max_score = max_score;
    return td;
}

void frt_td_destroy(FrtTopDocs *td)
{
    int i;

    for (i = 0; i < td->size; i++) {
        free(td->hits[i]);
    }
    free(td->hits);
    free(td);
}

char *frt_td_to_s(FrtTopDocs *td)
{
    int i;
    FrtHit *hit;
    char *buffer = frt_strfmt("%d hits sorted by <score, doc_num>\n",
                          td->total_hits);
    for (i = 0; i < td->size; i++) {
        hit = td->hits[i];
        frt_estrcat(buffer, frt_strfmt("\t%d:%f\n", hit->doc, hit->score));
    }
    return buffer;
}

/***************************************************************************
 *
 * Weight
 *
 ***************************************************************************/

FrtQuery *frt_w_get_query(FrtWeight *self)
{
    return self->query;
}

float frt_w_get_value(FrtWeight *self)
{
    return self->value;
}

float frt_w_sum_of_squared_weights(FrtWeight *self)
{
    self->qweight = self->idf * self->query->boost;
    return self->qweight * self->qweight;   /* square it */
}

void frt_w_normalize(FrtWeight *self, float normalization_factor)
{
    self->qnorm = normalization_factor;
    self->qweight *= normalization_factor;  /* normalize query weight */
    self->value = self->qweight * self->idf;/* idf for document */
}

void frt_w_destroy(FrtWeight *self)
{
    frt_q_deref(self->query);
    free(self);
}

FrtWeight *frt_w_create(size_t size, FrtQuery *query)
{
    FrtWeight *self                    = (FrtWeight *)frt_ecalloc(size);
#ifdef DEBUG
    if (size < sizeof(FrtWeight)) {
        FRT_RAISE(FRT_ARG_ERROR, "size of weight <%d> should be at least <%d>",
              (int)size, (int)sizeof(FrtWeight));
    }
#endif
    FRT_REF(query);
    self->query                     = query;
    self->get_query                 = &frt_w_get_query;
    self->get_value                 = &frt_w_get_value;
    self->normalize                 = &frt_w_normalize;
    self->destroy                   = &frt_w_destroy;
    self->sum_of_squared_weights    = &frt_w_sum_of_squared_weights;
    return self;
}

/***************************************************************************
 *
 * Query
 *
 ***************************************************************************/

static const char *QUERY_NAMES[] = {
    "TermQuery",
    "MultiTermQuery",
    "BooleanQuery",
    "PhraseQuery",
    "ConstantScoreQuery",
    "FilteredQuery",
    "MatchAllQuery",
    "RangeQuery",
    "WildCardQuery",
    "FuzzyQuery",
    "PrefixQuery",
    "SpanTermQuery",
    "SpanMultiTermQuery",
    "SpanPrefixQuery",
    "SpanFirstQuery",
    "SpanOrQuery",
    "SpanNotQuery",
    "SpanNearQuery"
};

static const char *UNKNOWN_QUERY_NAME = "UnkownQuery";

const char *frt_q_get_query_name(FrtQueryType type) {
    if (type >= FRT_NELEMS(QUERY_NAMES)) {
        return UNKNOWN_QUERY_NAME;
    }
    else {
        return QUERY_NAMES[type];
    }
}

static FrtQuery *q_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    (void)ir;
    self->ref_cnt++;
    return self;
}

static void q_extract_terms(FrtQuery *self, FrtHashSet *terms)
{
    /* do nothing by default */
    (void)self;
    (void)terms;
}

FrtSimilarity *frt_q_get_similarity_i(FrtQuery *self, FrtSearcher *searcher)
{
    (void)self;
    return searcher->get_similarity(searcher);
}

void frt_q_destroy_i(FrtQuery *self)
{
    free(self);
}

void frt_q_deref(FrtQuery *self)
{
    if (--(self->ref_cnt) == 0) {
        self->destroy_i(self);
    }
}

FrtWeight *frt_q_create_weight_unsup(FrtQuery *self, FrtSearcher *searcher)
{
    (void)self;
    (void)searcher;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR,
          "Create weight is unsupported for this type of query");
    return NULL;
}

FrtWeight *frt_q_weight(FrtQuery *self, FrtSearcher *searcher)
{
    FrtQuery      *query   = searcher->rewrite(searcher, self);
    FrtWeight     *weight  = query->create_weight_i(query, searcher);
    float       sum     = weight->sum_of_squared_weights(weight);
    FrtSimilarity *sim     = query->get_similarity(query, searcher);
    float       norm    = frt_sim_query_norm(sim, sum);
    frt_q_deref(query);

    weight->normalize(weight, norm);
    return self->weight = weight;
}

#define BQ(query) ((FrtBooleanQuery *)(query))
FrtQuery *frt_q_combine(FrtQuery **queries, int q_cnt)
{
    int i;
    FrtQuery *q, *ret_q;
    FrtHashSet *uniques = frt_hs_new((frt_hash_ft)&frt_q_hash, (frt_eq_ft)&frt_q_eq, NULL);
    for (i = 0; i < q_cnt; i++) {
        q = queries[i];
        if (q->type == BOOLEAN_QUERY) {
            int j;
            bool splittable = true;
            if (BQ(q)->coord_disabled == false) {
                splittable = false;
            } else {
                for (j = 0; j < BQ(q)->clause_cnt; j++) {
                    if (BQ(q)->clauses[j]->occur != FRT_BC_SHOULD) {
                        splittable = false;
                        break;
                    }
                }
            }
            if (splittable) {
                for (j = 0; j < BQ(q)->clause_cnt; j++) {
                    FrtQuery *sub_q = BQ(q)->clauses[j]->query;
                    frt_hs_add(uniques, sub_q);
                }
            } else {
                frt_hs_add(uniques, q);
            }
        } else {
            frt_hs_add(uniques, q);
        }
    }

    if (uniques->size == 1) {
        ret_q = (FrtQuery *)uniques->first->elem;
        FRT_REF(ret_q);
    } else {
        FrtHashSetEntry *hse;
        ret_q = frt_bq_new(true);
        for (hse = uniques->first; hse; hse = hse->next) {
            q = (FrtQuery *)hse->elem;
            frt_bq_add_query(ret_q, q, FRT_BC_SHOULD);
        }
    }
    frt_hs_destroy(uniques);

    return ret_q;
}

unsigned long long frt_q_hash(FrtQuery *self)
{
    return (self->hash(self) << 5) | self->type;
}

int frt_q_eq(FrtQuery *self, FrtQuery *o)
{
    return (self == o)
        || ((self->type == o->type)
            && (self->boost == o->boost)
            && self->eq(self, o));
}

static FrtMatchVector *q_get_matchv_i(FrtQuery *self, FrtMatchVector *mv, FrtTermVector *tv)
{
    /* be default we don't add any matches */
    (void)self; (void)tv;
    return mv;
}

FrtQuery *frt_q_create(size_t size)
{
    FrtQuery *self = (FrtQuery *)frt_ecalloc(size);
#ifdef DEBUG
    if (size < sizeof(FrtQuery)) {
        FRT_RAISE(FRT_ARG_ERROR, "Size of a query <%d> should never be smaller than "
              "the size of a Query struct <%d>", (int)size, (int)sizeof(FrtQuery));
    }
#endif
    self->boost             = 1.0f;
    self->rewrite           = &q_rewrite;
    self->get_similarity    = &frt_q_get_similarity_i;
    self->extract_terms     = &q_extract_terms;
    self->get_matchv_i      = &q_get_matchv_i;
    self->weight            = NULL;
    self->ref_cnt           = 1;
    return self;
}

/***************************************************************************
 *
 * Scorer
 *
 ***************************************************************************/

void frt_scorer_destroy_i(FrtScorer *scorer)
{
    free(scorer);
}

FrtScorer *frt_scorer_create(size_t size, FrtSimilarity *similarity)
{
    FrtScorer *self        = (FrtScorer *)frt_ecalloc(size);
#ifdef DEBUG
    if (size < sizeof(FrtScorer)) {
        FRT_RAISE(FRT_ARG_ERROR, "size of scorer <%d> should be at least <%d>",
              (int)size, (int)sizeof(FrtScorer));
    }
#endif
    self->destroy       = &frt_scorer_destroy_i;
    self->similarity    = similarity;
    return self;
}

bool frt_scorer_doc_less_than(const FrtScorer *s1, const FrtScorer *s2)
{
    return s1->doc < s2->doc;
}

int frt_scorer_doc_cmp(const void *p1, const void *p2)
{
    return (*(FrtScorer **)p1)->doc - (*(FrtScorer **)p2)->doc;
}

/***************************************************************************
 *
 * Highlighter
 *
 ***************************************************************************/

/* ** MatchRange ** */
static int match_range_cmp(const void *p1, const void *p2)
{
    int diff = ((FrtMatchRange *)p1)->start - ((FrtMatchRange *)p2)->start;
    if (diff != 0) {
        return diff;
    }
    else {
        return ((FrtMatchRange *)p2)->end - ((FrtMatchRange *)p1)->end;
    }
}



/* ** FrtMatchVector ** */
FrtMatchVector *frt_matchv_new(void) {
    FrtMatchVector *matchv = FRT_ALLOC(FrtMatchVector);

    matchv->size = 0;
    matchv->capa = FRT_MATCH_VECTOR_INIT_CAPA;
    matchv->matches = FRT_ALLOC_N(FrtMatchRange, FRT_MATCH_VECTOR_INIT_CAPA);

    return matchv;
}

FrtMatchVector *frt_matchv_add(FrtMatchVector *self, int start, int end)
{
    if (self->size >= self->capa) {
        self->capa <<= 1;
        FRT_REALLOC_N(self->matches, FrtMatchRange, self->capa);
    }
    self->matches[self->size].start = start;
    self->matches[self->size].end   = end;
    self->matches[self->size].score = 1.0;
    self->size++;
    return self;
}

FrtMatchVector *frt_matchv_sort(FrtMatchVector *self)
{
    qsort(self->matches, self->size, sizeof(FrtMatchRange), &match_range_cmp);
    return self;
}

FrtMatchVector *frt_matchv_compact(FrtMatchVector *self)
{
    int left, right;
    frt_matchv_sort(self);
    for (right = left = 0; right < self->size; right++) {
        /* Note the end + 1. This compacts a range 3:5 and 6:8 inleft 3:8 */
        if (self->matches[right].start > self->matches[left].end + 1) {
            left++;
            self->matches[left].start = self->matches[right].start;
            self->matches[left].end = self->matches[right].end;
            self->matches[left].score = self->matches[right].score;
        }
        else if (self->matches[right].end > self->matches[left].end) {
            self->matches[left].end = self->matches[right].end;
        }
        else {
            self->matches[left].score += self->matches[right].score;
        }
    }
    self->size = left + 1;
    return self;
}

FrtMatchVector *frt_matchv_compact_with_breaks(FrtMatchVector *self)
{
    int left, right;
    frt_matchv_sort(self);
    for (right = left = 0; right < self->size; right++) {
        /* Note: no end + 1. Unlike above won't compact ranges 3:5 and 6:8 */
        if (self->matches[right].start > self->matches[left].end) {
            left++;
            self->matches[left].start = self->matches[right].start;
            self->matches[left].end = self->matches[right].end;
            self->matches[left].score = self->matches[right].score;
        }
        else if (self->matches[right].end > self->matches[left].end) {
            self->matches[left].end = self->matches[right].end;
            self->matches[left].score += self->matches[right].score;
        }
        else if (right > left) {
            self->matches[left].score += self->matches[right].score;
        }
    }
    self->size = left + 1;
    return self;
}


static FrtMatchVector *matchv_set_offsets(FrtMatchVector *mv, FrtOffset *offsets)
{
    int i;
    for (i = 0; i < mv->size; i++) {
        mv->matches[i].start_offset = offsets[mv->matches[i].start].start;
        mv->matches[i].end_offset = offsets[mv->matches[i].end].end;
    }
    return mv;
}

void frt_matchv_destroy(FrtMatchVector *self)
{
    free(self->matches);
    free(self);
}

/***************************************************************************
 *
 * Searcher
 *
 ***************************************************************************/

FrtMatchVector *frt_searcher_get_match_vector(FrtSearcher *self,
                                       FrtQuery *query,
                                       const int doc_num,
                                       FrtSymbol field)
{
    FrtMatchVector *mv = frt_matchv_new();
    bool rewrite = query->get_matchv_i == q_get_matchv_i;
    FrtTermVector *tv = self->get_term_vector(self, doc_num, field);
    if (rewrite) {
        query = self->rewrite(self, query);
    }
    if (tv && tv->term_cnt > 0 && tv->terms[0].positions != NULL) {
        mv = query->get_matchv_i(query, mv, tv);
        frt_tv_destroy(tv);
    }
    if (rewrite) {
        frt_q_deref(query);
    }
    return mv;
}

typedef struct Excerpt
{
    int start;
    int end;
    int start_pos;
    int end_pos;
    int start_offset;
    int end_offset;
    double score;
} Excerpt;

/*
static int excerpt_cmp(const void *p1, const void *p2)
{
    double score1 = (*((Excerpt **)p1))->score;
    double score2 = (*((Excerpt **)p2))->score;
    if (score1 > score2) return 1;
    if (score1 < score2) return -1;
    return 0;
}
*/

static int excerpt_start_cmp(const void *p1, const void *p2)
{
    return (*((Excerpt **)p1))->start - (*((Excerpt **)p2))->start;
}

static int excerpt_lt(Excerpt *e1, Excerpt *e2)
{
    return e1->score > e2->score; /* want the highest score at top */
}

static Excerpt *excerpt_new(int start, int end, double score)
{
    Excerpt *excerpt = FRT_ALLOC_AND_ZERO(Excerpt);
    excerpt->start = start;
    excerpt->end = end;
    excerpt->score = score;
    return excerpt;
}

static Excerpt *excerpt_recalc_score(Excerpt *e, FrtMatchVector *mv)
{
    int i;
    double score = 0.0;
    for (i = e->start; i <= e->end; i++) {
        score += mv->matches[i].score;
    }
    e->score = score;
    return e;
}

/* expand an excerpt to it's largest possible size */
static Excerpt *excerpt_expand(Excerpt *e, const int len, FrtTermVector *tv)
{
    FrtOffset *offsets = tv->offsets;
    int offset_cnt = tv->offset_cnt;
    bool did_expansion = true;
    int i;
    /* fill in skipped offsets */
    for (i = 1; i < offset_cnt; i++) {
        if (offsets[i].start == 0) {
            offsets[i].start = offsets[i-1].start;
        }
        if (offsets[i].end == 0) {
            offsets[i].end = offsets[i-1].end;
        }
    }

    while (did_expansion) {
        did_expansion = false;
        if (e->start_pos > 0
            && (e->end_offset - offsets[e->start_pos - 1].start) < len) {
            e->start_pos--;
            e->start_offset = offsets[e->start_pos].start;
            did_expansion = true;
        }
        if (e->end_pos < (offset_cnt - 1)
            && (offsets[e->end_pos + 1].end - e->start_offset) < len) {
            e->end_pos++;
            e->end_offset = offsets[e->end_pos].end;
            did_expansion = true;
        }
    }
    return e;
}

static char *excerpt_get_str(Excerpt *e, FrtMatchVector *mv,
                             FrtLazyDocField *lazy_df,
                             const char *pre_tag,
                             const char *post_tag,
                             const char *ellipsis)
{
    int i, len;
    int last_offset = e->start_offset;
    const int num_matches = e->end - e->start + 1;
    const int pre_tag_len = (int)strlen(pre_tag);
    const int post_tag_len = (int)strlen(post_tag);
    const int ellipsis_len = (int)strlen(ellipsis);
    char *excerpt_str = FRT_ALLOC_N(char,
                                10 + e->end_offset - e->start_offset
                                + (num_matches * (pre_tag_len + post_tag_len))
                                + (2 * ellipsis_len));
    char *e_ptr = excerpt_str;
    if (e->start_offset > 0) {
        memcpy(e_ptr, ellipsis, ellipsis_len);
        e_ptr += ellipsis_len;
    }
    for (i = e->start; i <= e->end; i++) {
        FrtMatchRange *mr = mv->matches + i;
        len = mr->start_offset - last_offset;
        if (len) {
            frt_lazy_df_get_bytes(lazy_df, e_ptr, last_offset, len);
            e_ptr += len;
        }
        memcpy(e_ptr, pre_tag, pre_tag_len);
        e_ptr += pre_tag_len;
        len = mr->end_offset - mr->start_offset;
        if (len) {
            frt_lazy_df_get_bytes(lazy_df, e_ptr, mr->start_offset, len);
            e_ptr += len;
        }
        memcpy(e_ptr, post_tag, post_tag_len);
        e_ptr += post_tag_len;
        last_offset = mr->end_offset;
    }
    if ((lazy_df->len - e->end_offset) <= ellipsis_len) {
        /* no point using ellipsis if it takes up more space */
        e->end_offset = lazy_df->len;
    }
    len = e->end_offset - last_offset;
    if (len) {
        frt_lazy_df_get_bytes(lazy_df, e_ptr, last_offset, len);
        e_ptr += len;
    }
    if (e->end_offset < lazy_df->len) {
        memcpy(e_ptr, ellipsis, ellipsis_len);
        e_ptr += ellipsis_len;
    }
    *e_ptr = '\0';
    return excerpt_str;
}

static char *highlight_field(FrtMatchVector *mv,
                             FrtLazyDocField *lazy_df,
                             FrtTermVector *tv,
                             const char *pre_tag,
                             const char *post_tag)
{
    const int pre_len = (int)strlen(pre_tag);
    const int post_len = (int)strlen(post_tag);
    char *excerpt_str =
        FRT_ALLOC_N(char, 10 + lazy_df->len + (mv->size * (pre_len + post_len)));
    if (mv->size > 0) {
        int last_offset = 0;
        int i, len;
        char *e_ptr = excerpt_str;
        frt_matchv_compact_with_breaks(mv);
        matchv_set_offsets(mv, tv->offsets);
        for (i = 0; i < mv->size; i++) {
            FrtMatchRange *mr = mv->matches + i;
            len = mr->start_offset - last_offset;
            if (len) {
                frt_lazy_df_get_bytes(lazy_df, e_ptr, last_offset, len);
                e_ptr += len;
            }
            memcpy(e_ptr, pre_tag, pre_len);
            e_ptr += pre_len;
            len = mr->end_offset - mr->start_offset;
            if (len) {
                frt_lazy_df_get_bytes(lazy_df, e_ptr, mr->start_offset, len);
                e_ptr += len;
            }
            memcpy(e_ptr, post_tag, post_len);
            e_ptr += post_len;
            last_offset = mr->end_offset;
        }
        len = lazy_df->len - last_offset;
        if (len) {
            frt_lazy_df_get_bytes(lazy_df, e_ptr, last_offset, len);
            e_ptr += len;
        }
        *e_ptr = '\0';
    }
    else {
        frt_lazy_df_get_bytes(lazy_df, excerpt_str, 0, lazy_df->len);
        excerpt_str[lazy_df->len] = '\0';
    }
    return excerpt_str;
}

char **frt_searcher_highlight(FrtSearcher *self,
                          FrtQuery *query,
                          const int doc_num,
                          FrtSymbol field,
                          const int excerpt_len,
                          const int num_excerpts,
                          const char *pre_tag,
                          const char *post_tag,
                          const char *ellipsis)
{
    char **excerpt_strs = NULL;
    FrtTermVector *tv = self->get_term_vector(self, doc_num, field);
    FrtLazyDoc *lazy_doc = self->get_lazy_doc(self, doc_num);
    FrtLazyDocField *lazy_df = NULL;
    if (lazy_doc) {
        lazy_df = frt_lazy_doc_get(lazy_doc, field);
    }
    if (tv && lazy_df && tv->term_cnt > 0 && tv->terms[0].positions != NULL
        && tv->offsets != NULL) {
        FrtMatchVector *mv;
        query = self->rewrite(self, query);
        mv = query->get_matchv_i(query, frt_matchv_new(), tv);
        frt_q_deref(query);
        if (lazy_df->len < (excerpt_len * num_excerpts)) {
            excerpt_strs = frt_ary_new_type_capa(char *, 1);
            frt_ary_push(excerpt_strs,
                     highlight_field(mv, lazy_df, tv, pre_tag, post_tag));
        }
        else if (mv->size > 0) {
            Excerpt **excerpts = FRT_ALLOC_AND_ZERO_N(Excerpt *, num_excerpts);
            int e_start, e_end, i, j;
            FrtMatchRange *matches = mv->matches;
            double running_score = 0.0;
            FrtOffset *offsets = tv->offsets;
            FrtPriorityQueue *excerpt_pq;

            frt_matchv_compact_with_breaks(mv);
            matchv_set_offsets(mv, offsets);
            excerpt_pq = frt_pq_new(mv->size, (frt_lt_ft)&excerpt_lt, &free);
            /* add all possible excerpts to the priority queue */

            for (e_start = e_end = 0; e_start < mv->size; e_start++) {
                const int start_offset = matches[e_start].start_offset;
                if (e_start > e_end) {
                    running_score = 0.0;
                    e_end = e_start;
                }
                while (e_end < mv->size && (matches[e_end].end_offset
                                             <= start_offset + excerpt_len)) {
                    running_score += matches[e_end].score;
                    e_end++;
                }
                frt_pq_push(excerpt_pq,
                        excerpt_new(e_start, e_end - 1, running_score));
                /* - 0.1 so that earlier matches take priority */
                running_score -= matches[e_start].score;
            }

            for (i = 0; i < num_excerpts && excerpt_pq->size > 0; i++) {
                excerpts[i] = (Excerpt *)frt_pq_pop(excerpt_pq);
                if (i < num_excerpts - 1) {
                    /* set match ranges alread included to 0 */
                    Excerpt *e = excerpts[i];
                    for (j = e->start; j <= e->end; j++) {
                        matches[j].score = 0.0;
                    }
                    e = NULL;
                    while (e != (Excerpt *)frt_pq_top(excerpt_pq)) {
                        e = (Excerpt *)frt_pq_top(excerpt_pq);
                        excerpt_recalc_score(e, mv);
                        frt_pq_down(excerpt_pq);
                    }
                }
            }

            qsort(excerpts, i, sizeof(Excerpt *), &excerpt_start_cmp);
            for (j = 0; j < i; j++) {
                Excerpt *e = excerpts[j];
                e->start_pos = matches[e->start].start;
                e->end_pos = matches[e->end].end;
                e->start_offset = offsets[e->start_pos].start;
                e->end_offset = offsets[e->end_pos].end;
            }

            if (i < num_excerpts) {
                const int diff = num_excerpts - i;
                memmove(excerpts + (diff), excerpts,
                        i * sizeof(Excerpt *));
                for (j = 0; j < diff; j++) {
                    /* these new excerpts will grow into one long excerpt at
                     * the start */
                    excerpts[j] = FRT_ALLOC_AND_ZERO(Excerpt);
                    excerpts[j]->end = -1;
                }
            }

            excerpt_strs = frt_ary_new_type_capa(char *, num_excerpts);
            /* merge excerpts where possible */
            for (i = 0; i < num_excerpts;) {
                Excerpt *ei = excerpts[i];
                int merged = 1; /* 1 means a single excerpt, ie no merges */
                for (j = i + 1; j < num_excerpts; j++) {
                    Excerpt *ej = excerpts[j];
                    if ((ej->end_offset - ei->start_offset)
                        < (j - i + 1) * excerpt_len) {
                        ei->end = ej->end;
                        ei->end_pos = ej->end_pos;
                        ei->end_offset = ej->end_offset;
                        merged = j - i + 1;
                    }
                }
                excerpt_expand(ei, merged * excerpt_len, tv);
                frt_ary_push(excerpt_strs,
                         excerpt_get_str(ei, mv, lazy_df,
                                         pre_tag, post_tag, ellipsis));
                i += merged;
            }
            for (i = 0; i < num_excerpts; i++) {
                free(excerpts[i]);
            }
            free(excerpts);
            frt_pq_destroy(excerpt_pq);
        }
        frt_matchv_destroy(mv);
    }
    if (tv) frt_tv_destroy(tv);
    if (lazy_doc) frt_lazy_doc_close(lazy_doc);
    return excerpt_strs;
}

static FrtWeight *sea_create_weight(FrtSearcher *self, FrtQuery *query)
{
    return frt_q_weight(query, self);
}

static void sea_check_args(int num_docs, int first_doc)
{
    if (num_docs <= 0) {
        FRT_RAISE(FRT_ARG_ERROR, ":num_docs was set to %d but should be greater "
              "than 0 : %d <= 0", num_docs, num_docs);
    }

    if (first_doc < 0) {
        FRT_RAISE(FRT_ARG_ERROR, ":first_doc was set to %d but should be greater "
              "than or equal to 0 : %d < 0", first_doc, first_doc);
    }
}

static FrtSimilarity *sea_get_similarity(FrtSearcher *self)
{
    return self->similarity;
}

/***************************************************************************
 *
 * IndexSearcher
 *
 ***************************************************************************/

#define ISEA(searcher) ((FrtIndexSearcher *)(searcher))

int frt_isea_doc_freq(FrtSearcher *self, FrtSymbol field, const char *term)
{
    return frt_ir_doc_freq(ISEA(self)->ir, field, term);
}

static FrtDocument *isea_get_doc(FrtSearcher *self, int doc_num)
{
    FrtIndexReader *ir = ISEA(self)->ir;
    return ir->get_doc(ir, doc_num);
}

static FrtLazyDoc *isea_get_lazy_doc(FrtSearcher *self, int doc_num)
{
    FrtIndexReader *ir = ISEA(self)->ir;
    return ir->get_lazy_doc(ir, doc_num);
}

static int isea_max_doc(FrtSearcher *self)
{
    FrtIndexReader *ir = ISEA(self)->ir;
    return ir->max_doc(ir);
}

#define IS_FILTERED(bits, post_filter, scorer, searcher) \
((bits && !frt_bv_get(bits, scorer->doc))\
 || (post_filter \
     && !(filter_factor = \
          post_filter->filter_func(scorer->doc, scorer->score(scorer),\
                                   searcher, post_filter->arg))))

static FrtTopDocs *isea_search_w(FrtSearcher *self,
                              FrtWeight *weight,
                              int first_doc,
                              int num_docs,
                              FrtFilter *filter,
                              FrtSort *sort,
                              FrtPostFilter *post_filter,
                              bool load_fields)
{
    int max_size = num_docs + (num_docs == INT_MAX ? 0 : first_doc);
    int i;
    int total_hits = 0;
    FrtHit **score_docs = NULL;

    FrtPriorityQueue *hq;
    FrtHit *(*hq_pop)(FrtPriorityQueue *pq);
    void (*hq_insert)(FrtPriorityQueue *pq, FrtHit *hit);
    void (*hq_destroy)(FrtPriorityQueue *self);

    FrtScorer *scorer;
    FrtHit hit;

    float max_score = 0.0f;
    float score = 0.0f;
    float filter_factor = 1.0f;

    FrtBitVector *bits = (filter ? frt_filt_get_bv(filter, ISEA(self)->ir) : NULL);

    sea_check_args(num_docs, first_doc);

    if (sort) {
        hq = frt_fshq_pq_new(max_size, sort, ISEA(self)->ir);
        hq_insert = &frt_fshq_pq_insert;
        hq_destroy = &frt_fshq_pq_destroy;
        if (load_fields) {
            hq_pop = &frt_fshq_pq_pop_fd;
        } else {
            hq_pop = &frt_fshq_pq_pop;
        }
    } else {
        hq = frt_pq_new(max_size, (frt_lt_ft)&hit_lt, &free);
        hq_pop = &hit_pq_pop;
        hq_insert = &hit_pq_insert;
        hq_destroy = &frt_pq_destroy;
    }

    scorer = weight->scorer(weight, ISEA(self)->ir);
    if (!scorer || 0 == ISEA(self)->ir->num_docs(ISEA(self)->ir)) {
        if (scorer) scorer->destroy(scorer);
        return frt_td_new(0, 0, NULL, 0.0);
    }

    while (scorer->next(scorer)) {
        if (bits && !frt_bv_get(bits, scorer->doc)) continue;
        score = scorer->score(scorer);
        if (post_filter &&
            !(filter_factor = post_filter->filter_func(scorer->doc,
                                                       score,
                                                       self,
                                                       post_filter->arg))) {
            continue;
        }
        total_hits++;
        if (filter_factor < 1.0f) score *= filter_factor;
        if (score > max_score) max_score = score;
        hit.doc = scorer->doc; hit.score = score;
        hq_insert(hq, &hit);
    }
    scorer->destroy(scorer);
    if (hq->size > first_doc) {
        if ((hq->size - first_doc) < num_docs) {
            num_docs = hq->size - first_doc;
        }
        score_docs = FRT_ALLOC_N(FrtHit *, num_docs);
        for (i = num_docs - 1; i >= 0; i--) {
            score_docs[i] = hq_pop(hq);
        }
    } else {
        num_docs = 0;
    }
    frt_pq_clear(hq);
    hq_destroy(hq);
    return frt_td_new(total_hits, num_docs, score_docs, max_score);
}

static FrtTopDocs *isea_search(FrtSearcher *self,
                            FrtQuery *query,
                            int first_doc,
                            int num_docs,
                            FrtFilter *filter,
                            FrtSort *sort,
                            FrtPostFilter *post_filter,
                            bool load_fields)
{
    FrtTopDocs *td;
    FrtWeight *weight = frt_q_weight(query, self);
    td = isea_search_w(self, weight, first_doc, num_docs, filter, sort, post_filter, load_fields);
    weight->destroy(weight);
    return td;
}

static void isea_search_each_w(FrtSearcher *self, FrtWeight *weight, FrtFilter *filter,
                               FrtPostFilter *post_filter,
                               void (*fn)(FrtSearcher *, int, float, void *),
                               void *arg)
{
    FrtScorer *scorer;
    float filter_factor = 1.0f;
    FrtBitVector *bits = (filter
                       ? frt_filt_get_bv(filter, ISEA(self)->ir)
                       : NULL);

    scorer = weight->scorer(weight, ISEA(self)->ir);
    if (!scorer) {
        return;
    }

    while (scorer->next(scorer)) {
        float score;
        if (bits && !frt_bv_get(bits, scorer->doc)) continue;
        score = scorer->score(scorer);
        if (post_filter &&
            !(filter_factor = post_filter->filter_func(scorer->doc,
                                                       score,
                                                       self,
                                                       post_filter->arg))) {
            continue;
        }
        fn(self, scorer->doc, filter_factor * score, arg);
    }
    scorer->destroy(scorer);
}

static void isea_search_each(FrtSearcher *self, FrtQuery *query, FrtFilter *filter,
                             FrtPostFilter *post_filter,
                             void (*fn)(FrtSearcher *, int, float, void *),
                             void *arg)
{
    FrtWeight *weight = frt_q_weight(query, self);
    isea_search_each_w(self, weight, filter, post_filter, fn, arg);
    weight->destroy(weight);
}

/*
 * Scan the index for all documents that match a query and write the results
 * to a buffer. It will stop scanning once the limit is reached and it starts
 * scanning from offset_docnum.
 *
 * Note: Unlike the offset_docnum in other search methods, this offset_docnum
 * refers to document number and not hit.
 */
static int isea_search_unscored_w(FrtSearcher *self, FrtWeight *weight, int *buf, int limit, int offset_docnum) {
    int count = 0;
    FrtScorer *scorer = weight->scorer(weight, ISEA(self)->ir);
    if (scorer) {
        if (scorer->skip_to(scorer, offset_docnum)) {
            do {
                buf[count++] = scorer->doc;
            } while (count < limit && scorer->next(scorer));
        }
        scorer->destroy(scorer);
    }
    return count;
}

static int isea_search_unscored(FrtSearcher *self, FrtQuery *query, int *buf, int limit, int offset_docnum) {
    int count;
    FrtWeight *weight = frt_q_weight(query, self);
    count = isea_search_unscored_w(self, weight, buf, limit, offset_docnum);
    weight->destroy(weight);
    return count;
}

static FrtQuery *isea_rewrite(FrtSearcher *self, FrtQuery *original) {
    int q_is_destroyed = false;
    FrtQuery *query = original;
    FrtQuery *rewritten_query = query->rewrite(query, ISEA(self)->ir);
    while (q_is_destroyed || (query != rewritten_query)) {
        query = rewritten_query;
        rewritten_query = query->rewrite(query, ISEA(self)->ir);
        q_is_destroyed = (query->ref_cnt <= 1);
        frt_q_deref(query); /* destroy intermediate queries */
    }
    return query;
}

static FrtExplanation *isea_explain(FrtSearcher *self, FrtQuery *query, int doc_num) {
    FrtWeight *weight = frt_q_weight(query, self);
    FrtExplanation *e = weight->explain(weight, ISEA(self)->ir, doc_num);
    weight->destroy(weight);
    return e;
}

static FrtExplanation *isea_explain_w(FrtSearcher *self, FrtWeight *w, int doc_num) {
    return w->explain(w, ISEA(self)->ir, doc_num);
}

static FrtTermVector *isea_get_term_vector(FrtSearcher *self, const int doc_num, FrtSymbol field) {
    FrtIndexReader *ir = ISEA(self)->ir;
    return ir->term_vector(ir, doc_num, field);
}

static void isea_close(FrtSearcher *self) {
    if (ISEA(self)->ir && ISEA(self)->close_ir) {
        frt_ir_close(ISEA(self)->ir);
    }
    free(self);
}

FrtSearcher *frt_isea_alloc(void) {
    return (FrtSearcher *)FRT_ALLOC(FrtIndexSearcher);
}

FrtSearcher *frt_isea_init(FrtSearcher *self, FrtIndexReader *ir) {
    ISEA(self)->ir          = ir;
    ISEA(self)->close_ir    = true;

    self->similarity        = frt_sim_create_default();
    self->doc_freq          = &frt_isea_doc_freq;
    self->get_doc           = &isea_get_doc;
    self->get_lazy_doc      = &isea_get_lazy_doc;
    self->max_doc           = &isea_max_doc;
    self->create_weight     = &sea_create_weight;
    self->search            = &isea_search;
    self->search_w          = &isea_search_w;
    self->search_each       = &isea_search_each;
    self->search_each_w     = &isea_search_each_w;
    self->search_unscored   = &isea_search_unscored;
    self->search_unscored_w = &isea_search_unscored_w;
    self->rewrite           = &isea_rewrite;
    self->explain           = &isea_explain;
    self->explain_w         = &isea_explain_w;
    self->get_term_vector   = &isea_get_term_vector;
    self->get_similarity    = &sea_get_similarity;
    self->close             = &isea_close;

    return self;
}

FrtSearcher *frt_isea_new(FrtIndexReader *ir) {
    FrtSearcher *self = frt_isea_alloc();
    return frt_isea_init(self, ir);
}

/***************************************************************************
 *
 * CachedDFSearcher
 *
 ***************************************************************************/

#define CDFSEA(searcher) ((CachedDFSearcher *)(searcher))
typedef struct CachedDFSearcher
{
    FrtSearcher super;
    FrtHash     *df_map;
    int      max_doc;
} CachedDFSearcher;

static int cdfsea_doc_freq(FrtSearcher *self, FrtSymbol field, const char *text)
{
    FrtTerm term;
    int *df;
    term.field = field;
    term.text = (char *)text;
    df = (int *)frt_h_get(CDFSEA(self)->df_map, &term);
    return df ? *df : 0;
}

static FrtDocument *cdfsea_get_doc(FrtSearcher *self, int doc_num)
{
    (void)self; (void)doc_num;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static int cdfsea_max_doc(FrtSearcher *self)
{
    (void)self;
    return CDFSEA(self)->max_doc;
}

static FrtWeight *cdfsea_create_weight(FrtSearcher *self, FrtQuery *query)
{
    (void)self; (void)query;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static FrtTopDocs *cdfsea_search_w(FrtSearcher *self, FrtWeight *w, int fd, int nd,
                                FrtFilter *f, FrtSort *s, FrtPostFilter *pf, bool load)
{
    (void)self; (void)w; (void)fd; (void)nd;
    (void)f; (void)s; (void)pf; (void)load;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static FrtTopDocs *cdfsea_search(FrtSearcher *self, FrtQuery *q, int fd, int nd,
                              FrtFilter *f, FrtSort *s, FrtPostFilter *pf, bool load)
{
    (void)self; (void)q; (void)fd; (void)nd;
    (void)f; (void)s; (void)pf; (void)load;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static void cdfsea_search_each(FrtSearcher *self, FrtQuery *query, FrtFilter *filter,
                               FrtPostFilter *pf,
                               void (*fn)(FrtSearcher *, int, float, void *),
                               void *arg)
{
    (void)self; (void)query; (void)filter; (void)pf; (void)fn; (void)arg;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
}

static void cdfsea_search_each_w(FrtSearcher *self, FrtWeight *w, FrtFilter *filter,
                                 FrtPostFilter *pf,
                                 void (*fn)(FrtSearcher *, int, float, void *),
                                 void *arg)
{
    (void)self; (void)w; (void)filter; (void)pf; (void)fn; (void)arg;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
}

static FrtQuery *cdfsea_rewrite(FrtSearcher *self, FrtQuery *original)
{
    (void)self;
    original->ref_cnt++;
    return original;
}

static FrtExplanation *cdfsea_explain(FrtSearcher *self, FrtQuery *query, int doc_num)
{
    (void)self; (void)query; (void)doc_num;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static FrtExplanation *cdfsea_explain_w(FrtSearcher *self, FrtWeight *w, int doc_num)
{
    (void)self; (void)w; (void)doc_num;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static FrtTermVector *cdfsea_get_term_vector(FrtSearcher *self, const int doc_num,
                                          FrtSymbol field)
{
    (void)self; (void)doc_num; (void)field;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static FrtSimilarity *cdfsea_get_similarity(FrtSearcher *self)
{
    return self->similarity;
}

static void cdfsea_close(FrtSearcher *self)
{
    frt_h_destroy(CDFSEA(self)->df_map);
    free(self);
}

static FrtSearcher *cdfsea_new(FrtHash *df_map, int max_doc)
{
    FrtSearcher *self          = (FrtSearcher *)FRT_ALLOC(CachedDFSearcher);

    CDFSEA(self)->df_map    = df_map;
    CDFSEA(self)->max_doc   = max_doc;

    self->similarity        = frt_sim_create_default();
    self->doc_freq          = &cdfsea_doc_freq;
    self->get_doc           = &cdfsea_get_doc;
    self->max_doc           = &cdfsea_max_doc;
    self->create_weight     = &cdfsea_create_weight;
    self->search            = &cdfsea_search;
    self->search_w          = &cdfsea_search_w;
    self->search_each       = &cdfsea_search_each;
    self->search_each_w     = &cdfsea_search_each_w;
    self->rewrite           = &cdfsea_rewrite;
    self->explain           = &cdfsea_explain;
    self->explain_w         = &cdfsea_explain_w;
    self->get_term_vector   = &cdfsea_get_term_vector;
    self->get_similarity    = &cdfsea_get_similarity;
    self->close             = &cdfsea_close;
    return self;
}

/***************************************************************************
 *
 * MultiSearcher
 *
 ***************************************************************************/

#define MSEA(searcher) ((FrtMultiSearcher *)(searcher))
static int msea_get_searcher_index(FrtSearcher *self, int n)
{
    FrtMultiSearcher *msea = MSEA(self);
    int lo = 0;                 /* search starts array */
    int hi = msea->s_cnt - 1;   /* for 1st element < n, return its index */
    int mid, mid_val;

    while (hi >= lo) {
        mid = (lo + hi) >> 1;
        mid_val = msea->starts[mid];
        if (n < mid_val) {
            hi = mid - 1;
        }
        else if (n > mid_val) {
            lo = mid + 1;
        }
        else {                  /* found a match */
            while (((mid+1) < msea->s_cnt)
                   && (msea->starts[mid+1] == mid_val)) {
                mid++;          /* scan to last match */
            }
            return mid;
        }
    }
    return hi;
}

static int msea_doc_freq(FrtSearcher *self, FrtSymbol field, const char *term)
{
    int i;
    int doc_freq = 0;
    FrtMultiSearcher *msea = MSEA(self);
    for (i = 0; i < msea->s_cnt; i++) {
        FrtSearcher *s = msea->searchers[i];
        doc_freq += s->doc_freq(s, field, term);
    }

    return doc_freq;
}

static FrtDocument *msea_get_doc(FrtSearcher *self, int doc_num)
{
    FrtMultiSearcher *msea = MSEA(self);
    int i = msea_get_searcher_index(self, doc_num);
    FrtSearcher *s = msea->searchers[i];
    return s->get_doc(s, doc_num - msea->starts[i]);
}

static FrtLazyDoc *msea_get_lazy_doc(FrtSearcher *self, int doc_num)
{
    FrtMultiSearcher *msea = MSEA(self);
    int i = msea_get_searcher_index(self, doc_num);
    FrtSearcher *s = msea->searchers[i];
    return s->get_lazy_doc(s, doc_num - msea->starts[i]);
}

static int msea_max_doc(FrtSearcher *self)
{
    return MSEA(self)->max_doc;
}

static int *msea_get_doc_freqs(FrtSearcher *self, FrtHashSet *terms)
{
    int i;
    FrtHashSetEntry *hse;
    int *doc_freqs = FRT_ALLOC_N(int, terms->size);
    for (i = 0, hse = terms->first; hse; ++i, hse = hse->next) {
        FrtTerm *t = (FrtTerm *)hse->elem;
        doc_freqs[i] = msea_doc_freq(self, t->field, t->text);
    }
    return doc_freqs;
}

static FrtWeight *msea_create_weight(FrtSearcher *self, FrtQuery *query)
{
    int i, *doc_freqs;
    FrtSearcher *cdfsea;
    FrtWeight *w;
    FrtHash *df_map = frt_h_new((frt_hash_ft)&frt_term_hash,
                         (frt_eq_ft)&frt_term_eq,
                         (frt_free_ft)frt_term_destroy,
                         free);
    FrtQuery *rewritten_query = self->rewrite(self, query);
    /* terms get copied directly to df_map so no need to free here */
    FrtHashSet *terms = frt_hs_new((frt_hash_ft)&frt_term_hash,
                            (frt_eq_ft)&frt_term_eq,
                            (frt_free_ft)NULL);
    FrtHashSetEntry *hse;

    rewritten_query->extract_terms(rewritten_query, terms);
    doc_freqs = msea_get_doc_freqs(self, terms);

    for (hse = terms->first, i = 0; hse; ++i, hse = hse->next) {
        frt_h_set(df_map, hse->elem, frt_imalloc(doc_freqs[i]));
    }
    frt_hs_destroy(terms);
    free(doc_freqs);

    cdfsea = cdfsea_new(df_map, MSEA(self)->max_doc);

    w = frt_q_weight(rewritten_query, cdfsea);
    frt_q_deref(rewritten_query);
    cdfsea->close(cdfsea);

    return w;
}

struct MultiSearchEachArg {
    int start;
    void *arg;
    void (*fn)(FrtSearcher *, int, float, void *);
};

static void msea_search_each_i(FrtSearcher *self, int doc_num, float score, void *arg)
{
    struct MultiSearchEachArg *mse_arg = (struct MultiSearchEachArg *)arg;

    mse_arg->fn(self, doc_num + mse_arg->start, score, mse_arg->arg);
}

static void msea_search_each_w(FrtSearcher *self, FrtWeight *w, FrtFilter *filter,
                               FrtPostFilter *post_filter,
                               void (*fn)(FrtSearcher *, int, float, void *),
                               void *arg)
{
    int i;
    struct MultiSearchEachArg mse_arg;
    FrtMultiSearcher *msea = MSEA(self);
    FrtSearcher *s;

    mse_arg.fn = fn;
    mse_arg.arg = arg;
    for (i = 0; i < msea->s_cnt; i++) {
        s = msea->searchers[i];
        mse_arg.start = msea->starts[i];
        s->search_each_w(s, w, filter, post_filter,
                         &msea_search_each_i, &mse_arg);
    }
}

static void msea_search_each(FrtSearcher *self, FrtQuery *query, FrtFilter *filter,
                             FrtPostFilter *post_filter,
                             void (*fn)(FrtSearcher *, int, float, void *),
                             void *arg)
{
    FrtWeight *weight = frt_q_weight(query, self);
    msea_search_each_w(self, weight, filter, post_filter, fn, arg);
    weight->destroy(weight);
}

static int msea_search_unscored_w(FrtSearcher *self,
                                  FrtWeight *w,
                                  int *buf,
                                  int limit,
                                  int offset_docnum)
{
    int i, count = 0;
    FrtMultiSearcher *msea = MSEA(self);

    for (i = 0; count < limit && i < msea->s_cnt; i++) {
        /* if offset_docnum falls in this or previous indexes */
        if (offset_docnum < msea->starts[i+1]) {
            FrtSearcher *searcher = msea->searchers[i];
            const int index_offset = msea->starts[i];
            int current_limit = limit - count;
            /* if offset_docnum occurs in the current index then adjust,
             * otherwise set it to zero as it occurred in a previous index */
            int current_offset_docnum = offset_docnum > index_offset
                ? offset_docnum - index_offset
                : 0;

            /* record current count as we'll need to update docnums by the
             * index's offset */
            int j = count;
            count += searcher->search_unscored_w(searcher, w, buf + count,
                                                 current_limit,
                                                 current_offset_docnum);
            /* update doc nums with the current index's offsets */
            for (; j < count; j++) {
                buf[j] += index_offset;
            }
        }
    }
    return count;
}

static int msea_search_unscored(FrtSearcher *self,
                                FrtQuery *query,
                                int *buf,
                                int limit,
                                int offset_docnum) {
    int count;
    FrtWeight *weight = frt_q_weight(query, self);
    count = msea_search_unscored_w(self, weight, buf, limit, offset_docnum);
    weight->destroy(weight);
    return count;
}

struct MultiSearchArg {
    int total_hits, max_size;
    FrtPriorityQueue *hq;
    void (*hq_insert)(FrtPriorityQueue *pq, FrtHit *hit);
};

/*
 * FIXME Not used anywhere. Is it needed?
static void msea_search_i(FrtSearcher *self, int doc_num, float score, void *arg)
{
    struct MultiSearchArg *ms_arg = (struct MultiSearchArg *)arg;
    FrtHit hit;
    (void)self;

    ms_arg->total_hits++;
    hit.doc = doc_num;
    hit.score = score;
    ms_arg->hq_insert(ms_arg->hq, &hit);
}
*/

static FrtTopDocs *msea_search_w(FrtSearcher *self,
                              FrtWeight *weight,
                              int first_doc,
                              int num_docs,
                              FrtFilter *filter,
                              FrtSort *sort,
                              FrtPostFilter *post_filter,
                              bool load_fields) {
    int max_size = num_docs + (num_docs == INT_MAX ? 0 : first_doc);
    int i;
    int total_hits = 0;
    FrtHit **score_docs = NULL;

    FrtPriorityQueue *hq;
    FrtHit *(*hq_pop)(FrtPriorityQueue *pq);
    void (*hq_insert)(FrtPriorityQueue *pq, FrtHit *hit);

    float max_score = 0.0f;

    (void)load_fields; /* does it automatically */

    sea_check_args(num_docs, first_doc);

    if (sort) {
        hq = frt_pq_new(max_size, (frt_lt_ft)&frt_fdshq_lt, &free);
        hq_insert = (void (*)(FrtPriorityQueue *pq, FrtHit *hit))&frt_pq_insert;
        hq_pop = (FrtHit *(*)(FrtPriorityQueue *pq))&frt_pq_pop;
    } else {
        hq = frt_pq_new(max_size, (frt_lt_ft)&hit_lt, &free);
        hq_insert = &hit_pq_multi_insert;
        hq_pop = &hit_pq_pop;
    }

    for (i = 0; i < MSEA(self)->s_cnt; i++) {
        FrtSearcher *s = MSEA(self)->searchers[i];
        FrtTopDocs *td = s->search_w(s, weight, 0, max_size, filter, sort, post_filter, true);
        if (td->size > 0) {
            int j;
            int start = MSEA(self)->starts[i];
            for (j = 0; j < td->size; j++) {
                FrtHit *hit = td->hits[j];
                hit->doc += start;
                hq_insert(hq, hit);
            }
            td->size = 0;
            if (td->max_score > max_score) max_score = td->max_score;
        }
        total_hits += td->total_hits;
        frt_td_destroy(td);
    }

    if (hq->size > first_doc) {
        if ((hq->size - first_doc) < num_docs) {
            num_docs = hq->size - first_doc;
        }
        score_docs = FRT_ALLOC_N(FrtHit *, num_docs);
        for (i = num_docs - 1; i >= 0; i--) {
            score_docs[i] = hq_pop(hq);
        }
    } else {
        num_docs = 0;
    }
    frt_pq_clear(hq);
    frt_pq_destroy(hq);

    return frt_td_new(total_hits, num_docs, score_docs, max_score);
}

static FrtTopDocs *msea_search(FrtSearcher *self,
                            FrtQuery *query,
                            int first_doc,
                            int num_docs,
                            FrtFilter *filter,
                            FrtSort *sort,
                            FrtPostFilter *post_filter,
                            bool load_fields) {
    FrtTopDocs *td;
    FrtWeight *weight = frt_q_weight(query, self);
    td = msea_search_w(self, weight, first_doc, num_docs, filter,
                       sort, post_filter, load_fields);
    weight->destroy(weight);
    return td;
}

static FrtQuery *msea_rewrite(FrtSearcher *self, FrtQuery *original) {
    int i;
    FrtSearcher *s;
    FrtMultiSearcher *msea = MSEA(self);
    FrtQuery **queries = FRT_ALLOC_N(FrtQuery *, msea->s_cnt), *rewritten;

    for (i = 0; i < msea->s_cnt; i++) {
        s = msea->searchers[i];
        queries[i] = s->rewrite(s, original);
    }
    rewritten = frt_q_combine(queries, msea->s_cnt);

    for (i = 0; i < msea->s_cnt; i++) {
        frt_q_deref(queries[i]);
    }
    free(queries);
    return rewritten;
}

static FrtExplanation *msea_explain(FrtSearcher *self, FrtQuery *query, int doc_num) {
    FrtMultiSearcher *msea = MSEA(self);
    int i = msea_get_searcher_index(self, doc_num);
    FrtWeight *w = frt_q_weight(query, self);
    FrtSearcher *s = msea->searchers[i];
    FrtExplanation *e = s->explain_w(s, w, doc_num - msea->starts[i]);
    w->destroy(w);
    return e;
}

static FrtExplanation *msea_explain_w(FrtSearcher *self, FrtWeight *w, int doc_num) {
    FrtMultiSearcher *msea = MSEA(self);
    int i = msea_get_searcher_index(self, doc_num);
    FrtSearcher *s = msea->searchers[i];
    FrtExplanation *e = s->explain_w(s, w, doc_num - msea->starts[i]);
    return e;
}

static FrtTermVector *msea_get_term_vector(FrtSearcher *self, const int doc_num, FrtSymbol field) {
    FrtMultiSearcher *msea = MSEA(self);
    int i = msea_get_searcher_index(self, doc_num);
    FrtSearcher *s = msea->searchers[i];
    return s->get_term_vector(s, doc_num - msea->starts[i], field);
}

static FrtSimilarity *msea_get_similarity(FrtSearcher *self) {
    return self->similarity;
}

static void msea_close(FrtSearcher *self) {
    int i;
    FrtSearcher *s;
    FrtMultiSearcher *msea = MSEA(self);
    if (msea->close_subs) {
        for (i = 0; i < msea->s_cnt; i++) {
            s = msea->searchers[i];
            s->close(s);
        }
    }
    free(msea->searchers);
    free(msea->starts);
    free(self);
}

FrtSearcher *frt_msea_alloc(void) {
    return (FrtSearcher *)FRT_ALLOC(FrtMultiSearcher);
}

FrtSearcher *frt_msea_init(FrtSearcher *self, FrtSearcher **searchers, int s_cnt, bool close_subs) {
    int i, max_doc = 0;
    int *starts = FRT_ALLOC_N(int, s_cnt + 1);
    for (i = 0; i < s_cnt; i++) {
        starts[i] = max_doc;
        max_doc += searchers[i]->max_doc(searchers[i]);
    }
    starts[i] = max_doc;

    MSEA(self)->s_cnt           = s_cnt;
    MSEA(self)->searchers       = searchers;
    MSEA(self)->starts          = starts;
    MSEA(self)->max_doc         = max_doc;
    MSEA(self)->close_subs      = close_subs;

    self->similarity            = frt_sim_create_default();
    self->doc_freq              = &msea_doc_freq;
    self->get_doc               = &msea_get_doc;
    self->get_lazy_doc          = &msea_get_lazy_doc;
    self->max_doc               = &msea_max_doc;
    self->create_weight         = &msea_create_weight;
    self->search                = &msea_search;
    self->search_w              = &msea_search_w;
    self->search_each           = &msea_search_each;
    self->search_each_w         = &msea_search_each_w;
    self->search_unscored       = &msea_search_unscored;
    self->search_unscored_w     = &msea_search_unscored_w;
    self->rewrite               = &msea_rewrite;
    self->explain               = &msea_explain;
    self->explain_w             = &msea_explain_w;
    self->get_term_vector       = &msea_get_term_vector;
    self->get_similarity        = &msea_get_similarity;
    self->close                 = &msea_close;
    return self;
}

FrtSearcher *frt_msea_new(FrtSearcher **searchers, int s_cnt, bool close_subs) {
    FrtSearcher *self = frt_msea_alloc();
    return frt_msea_init(self, searchers, s_cnt, close_subs);
}
