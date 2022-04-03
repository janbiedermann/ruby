#include "frt_global.h"
#include "frt_similarity.h"
#include "frt_search.h"
#include "frt_array.h"
#include "frt_helper.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/****************************************************************************
 *
 * Term
 *
 ****************************************************************************/

FrtTerm *frt_term_new(FrtSymbol field, const char *text)
{
    FrtTerm *t = FRT_ALLOC(FrtTerm);
    t->field = field;
    t->text = frt_estrdup(text);
    return t;
}

void frt_term_destroy(FrtTerm *self)
{
    free(self->text);
    free(self);
}

int frt_term_eq(const void *t1, const void *t2)
{
    return ((strcmp(((FrtTerm *)t1)->text, ((FrtTerm *)t2)->text) == 0) &&
        (((FrtTerm *)t1)->field == ((FrtTerm *)t2)->field));
}

unsigned long long frt_term_hash(const void *t)
{
    return frt_str_hash(((FrtTerm *)t)->text) * frt_str_hash(rb_id2name(((FrtTerm *)t)->field));
}

/****************************************************************************
 *
 * Similarity
 *
 ****************************************************************************/

static float simdef_length_norm(FrtSimilarity *s, FrtSymbol field, int num_terms)
{
    (void)s;
    (void)field;
    return (float)(1.0 / sqrt(num_terms));
}

static float simdef_query_norm(struct FrtSimilarity *s, float sum_of_squared_weights)
{
    (void)s;
    return (float)(1.0 / sqrt(sum_of_squared_weights));
}

static float simdef_tf(struct FrtSimilarity *s, float freq)
{
    (void)s;
    return (float)sqrt(freq);
}

static float simdef_sloppy_freq(struct FrtSimilarity *s, int distance)
{
    (void)s;
    return (float)(1.0 / (double)(distance + 1));
}

static float simdef_idf_term(struct FrtSimilarity *s, FrtSymbol field, char *term,
                      FrtSearcher *searcher)
{
    return s->idf(s, searcher->doc_freq(searcher, field, term),
                  searcher->max_doc(searcher));
}

static float simdef_idf_phrase(struct FrtSimilarity *s, FrtSymbol field,
                        FrtPhrasePosition *positions,
                        int pp_cnt, FrtSearcher *searcher)
{
    float idf = 0.0f;
    int i, j;
    for (i = 0; i < pp_cnt; i++) {
        char **terms = positions[i].terms;
        for (j = frt_ary_size(terms) - 1; j >= 0; j--) {
            idf += frt_sim_idf_term(s, field, terms[j], searcher);
        }
    }
    return idf;
}

static float simdef_idf(struct FrtSimilarity *s, int doc_freq, int num_docs)
{
    (void)s;
    return (float)(log((float)num_docs/(float)(doc_freq+1)) + 1.0);
}

static float simdef_coord(struct FrtSimilarity *s, int overlap, int max_overlap)
{
    (void)s;
    return (float)((double)overlap / (double)max_overlap);
}

static float simdef_decode_norm(struct FrtSimilarity *s, frt_uchar b)
{
    return s->norm_table[b];
}

static frt_uchar simdef_encode_norm(struct FrtSimilarity *s, float f)
{
    (void)s;
    return frt_float2byte(f);
}

static void simdef_destroy(FrtSimilarity *s)
{
    (void)s;
    /* nothing to do here */
}

static FrtSimilarity default_similarity = {
    NULL,
    {0},
    &simdef_length_norm,
    &simdef_query_norm,
    &simdef_tf,
    &simdef_sloppy_freq,
    &simdef_idf_term,
    &simdef_idf_phrase,
    &simdef_idf,
    &simdef_coord,
    &simdef_decode_norm,
    &simdef_encode_norm,
    &simdef_destroy
};

FrtSimilarity *frt_sim_create_default(void) {
    int i;
    if (!default_similarity.data) {
        for (i = 0; i < 256; i++) {
            default_similarity.norm_table[i] = frt_byte2float((unsigned char)i);
        }

        default_similarity.data = &default_similarity;
    }
    return &default_similarity;
}
