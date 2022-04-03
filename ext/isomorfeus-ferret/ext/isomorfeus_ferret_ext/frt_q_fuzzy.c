#include <string.h>
#include "frt_search.h"
#include "frt_helper.h"

/****************************************************************************
 *
 * FuzzyStuff
 *
 * The main method here is the fuzq_score_mn method which scores a term
 * against another term. The other methods all act in support.
 *
 * To learn more about the fuzzy scoring algorithm see;
 *
 *     http://en.wikipedia.org/wiki/Levenshtein_distance
 *
 ****************************************************************************/

/**
 * Calculate the maximum nomber of allowed edits (or maximum edit distance)
 * for a word to be a match.
 *
 * Note that fuzq->text_len and m are both the lengths text *after* the prefix
 * so `FRT_MIN(fuzq->text_len, m) + fuzq->pre_len)` actually gets the byte length
 * of the shorter string out of the query string and the index term being
 * compared.
 */
static int fuzq_calculate_max_distance(FrtFuzzyQuery *fuzq, int m) {
    return (int)((1.0 - fuzq->min_sim) * (FRT_MIN(fuzq->text_len, m) + fuzq->pre_len));
}

/**
 * The max-distance formula gets used a lot - it needs to be calculated for
 * every possible match in the index - so we cache the results for all
 * lengths up to the FRT_TYPICAL_LONGEST_WORD limit. For words longer than this we
 * calculate the value live.
 */
static void fuzq_initialize_max_distances(FrtFuzzyQuery *fuzq) {
    int i;
    for (i = 0; i < FRT_TYPICAL_LONGEST_WORD; i++) {
        fuzq->max_distances[i] = fuzq_calculate_max_distance(fuzq, i);
    }
}

/**
 * Return the cached max-distance value if the word is within the
 * FRT_TYPICAL_LONGEST_WORD limit.
 */
static int fuzq_get_max_distance(FrtFuzzyQuery *fuzq, int m) {
    if (m < FRT_TYPICAL_LONGEST_WORD)
        return fuzq->max_distances[m];
    return fuzq_calculate_max_distance(fuzq, m);
}

/**
 * Calculate the similarity score for the +target+ against the query.
 *
 * @params fuzq The Fuzzy Query
 * @params target *the term to compare against minus the prefix
 * @params m the string length of +target+
 * @params n the string length of the query string minus length of the prefix
 */
static float fuzq_score_mn(FrtFuzzyQuery *fuzq, const char *target, const int m, const int n) {
    int i, j, prune;
    int *d_curr, *d_prev;
    const char *text = fuzq->text;
    const int max_distance = fuzq_get_max_distance(fuzq, m);

    /* Just adding the characters of m to n or vice-versa results in
     * too many edits for example "pre" length is 3 and "prefixes"
     * length is 8. We can see that given this optimal circumstance,
     * the edit distance cannot be less than 5 which is 8-3 or more
     * precisesly Math.abs(3-8). If our maximum edit distance is 4,
     * then we can discard this word without looking at it. */
    if (max_distance < FRT_ABS(m-n)) {
        return 0.0f;
    }

    d_curr = fuzq->da;
    d_prev = d_curr + n + 1;

    /* init array */
    for (j = 0; j <= n; j++) {
        d_curr[j] = j;
    }

    /* start computing edit distance */
    for (i = 0; i < m;) {
        char s_i = target[i];
        /* swap d_current into d_prev */
        int *d_tmp = d_prev;
        d_prev = d_curr;
        d_curr = d_tmp;
        prune = (d_curr[0] = ++i) > max_distance;

        for (j = 0; j < n; j++) {
            d_curr[j + 1] = (s_i == text[j])
                ? FRT_MIN3(d_prev[j + 1] + 1, d_curr[j] + 1, d_prev[j])
                : FRT_MIN3(d_prev[j + 1], d_curr[j], d_prev[j]) + 1;
            if (prune && d_curr[j + 1] <= max_distance) {
                prune = false;
            }
        }
        if (prune) {
            return 0.0f;
        }
    }

    /* this will return less than 0.0 when the edit distance is greater
     * than the number of characters in the shorter word.  but this was
     * the formula that was previously used in FuzzyTermEnum, so it has
     * not been changed (even though min_sim must be greater than 0.0) */
    return (float)(1.0f - ((float)d_curr[n] / (float) (fuzq->pre_len + FRT_MIN(n, m))));
}

/**
 * The following algorithm is taken from Bob Carpenter's FuzzyTermEnum
 * implentation here;
 *
 * http://mail-archives.apache.org/mod_mbox/lucene-java-dev/200606.mbox/%3c448F0E8C.3050901@alias-i.com%3e
 */
float frt_fuzq_score(FrtFuzzyQuery *fuzq, const char *target) {
    const int m = (int)strlen(target);
    const int n = fuzq->text_len;

    /* we don't have anything to compare.  That means if we just add
     * the letters for m we get the new word */
    if (m == 0 || n == 0) {
        if (fuzq->pre_len == 0)
            return 0.0f;
        return 1.0f - ((float) (m+n) / fuzq->pre_len);
    }

    return fuzq_score_mn(fuzq, target, m, n);
}

/****************************************************************************
 *
 * FuzzyQuery
 *
 ****************************************************************************/

#define FzQ(query) ((FrtFuzzyQuery *)(query))

static char *fuzq_to_s(FrtQuery *self, FrtSymbol curr_field) {
    char *buffer, *bptr;
    char *term = FzQ(self)->term;
    FrtSymbol field = FzQ(self)->field;
    const char *field_name = rb_id2name(field);
    bptr = buffer = FRT_ALLOC_N(char, strlen(term) + strlen(field_name) + 70);

    if (curr_field != field) {
        bptr += sprintf(bptr, "%s:", field_name);
    }

    bptr += sprintf(bptr, "%s~", term);
    if (FzQ(self)->min_sim != 0.5) {
        frt_dbl_to_s(bptr, FzQ(self)->min_sim);
        bptr += strlen(bptr);
    }

    if (self->boost != 1.0) {
        *bptr = '^';
        frt_dbl_to_s(++bptr, self->boost);
    }

    return buffer;
}

static FrtQuery *fuzq_rewrite(FrtQuery *self, FrtIndexReader *ir) {
    FrtQuery *q;
    FrtFuzzyQuery *fuzq = FzQ(self);

    int pre_len = fuzq->pre_len;
    char *prefix = NULL;
    const char *term = fuzq->term;
    const int field_num = frt_fis_get_field_num(ir->fis, fuzq->field);
    FrtTermEnum *te;

    if (field_num < 0) {
        return frt_bq_new(true);
    }
    if (fuzq->pre_len >= (int)strlen(term)) {
        return frt_tq_new(fuzq->field, term);
    }

    q = frt_multi_tq_new_conf(fuzq->field, FrtMTQMaxTerms(self), fuzq->min_sim);
    if (pre_len > 0) {
        prefix = FRT_ALLOC_N(char, pre_len + 1);
        strncpy(prefix, term, pre_len);
        prefix[pre_len] = '\0';
        te = ir->terms_from(ir, field_num, prefix);
    } else {
        te = ir->terms(ir, field_num);
    }

    assert(NULL != te);

    fuzq->scale_factor = (float)(1.0 / (1.0 - fuzq->min_sim));
    fuzq->text = term + pre_len;
    fuzq->text_len = (int)strlen(fuzq->text);
    FRT_REALLOC_N(fuzq->da, int, fuzq->text_len * 2 + 2);
    fuzq_initialize_max_distances(fuzq);

    do {
        const char *curr_term = te->curr_term;
        const char *curr_suffix = curr_term + pre_len;
        float score = 0.0f;

        if (prefix && strncmp(curr_term, prefix, pre_len) != 0)
            break;

        score = frt_fuzq_score(fuzq, curr_suffix);
        frt_multi_tq_add_term_boost(q, curr_term, score);
    } while (te->next(te) != NULL);

    te->close(te);
    if (prefix) free(prefix);
    return q;
}

static void fuzq_destroy(FrtQuery *self) {
    free(FzQ(self)->term);
    free(FzQ(self)->da);
    frt_q_destroy_i(self);
}

static unsigned long long fuzq_hash(FrtQuery *self) {
    return frt_str_hash(FzQ(self)->term) ^ frt_str_hash(rb_id2name(FzQ(self)->field))
        ^ frt_float2int(FzQ(self)->min_sim) ^ FzQ(self)->pre_len;
}

static int fuzq_eq(FrtQuery *self, FrtQuery *o) {
    FrtFuzzyQuery *fq1 = FzQ(self);
    FrtFuzzyQuery *fq2 = FzQ(o);

    return (strcmp(fq1->term, fq2->term) == 0)
        && (fq1->field == fq2->field)
        && (fq1->pre_len == fq2->pre_len)
        && (fq1->min_sim == fq2->min_sim);
}

FrtQuery *frt_fuzq_alloc(void) {
    return frt_q_new(FrtFuzzyQuery);
}

FrtQuery *frt_fuzq_init_conf(FrtQuery *self, FrtSymbol field, const char *term, float min_sim, int pre_len, int max_terms) {
    FzQ(self)->field      = field;
    FzQ(self)->term       = frt_estrdup(term);
    FzQ(self)->pre_len    = pre_len ? pre_len : FRT_DEF_PRE_LEN;
    FzQ(self)->min_sim    = min_sim ? min_sim : FRT_DEF_MIN_SIM;
    FzQ(self)->da         = NULL;
    FrtMTQMaxTerms(self)  = max_terms ? max_terms : FRT_DEF_MAX_TERMS;

    self->type            = FUZZY_QUERY;
    self->to_s            = &fuzq_to_s;
    self->hash            = &fuzq_hash;
    self->eq              = &fuzq_eq;
    self->rewrite         = &fuzq_rewrite;
    self->destroy_i       = &fuzq_destroy;
    self->create_weight_i = &frt_q_create_weight_unsup;

    return self;
}

FrtQuery *frt_fuzq_new_conf(FrtSymbol field, const char *term, float min_sim, int pre_len, int max_terms) {
    FrtQuery *self = frt_fuzq_alloc();
    return frt_fuzq_init_conf(self, field, term, min_sim, pre_len, max_terms);
}

FrtQuery *frt_fuzq_new(FrtSymbol field, const char *term) {
    return frt_fuzq_new_conf(field, term, 0.0f, 0, 0);
}
