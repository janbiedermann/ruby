#include <ctype.h>
#include "frt_search.h"
#include "frt_array.h"
#include "frt_helper.h"
#include "testhelper.h"
#include "test.h"

#define ARRAY_SIZE 40

static FrtSymbol date, field, cat, number;

static void test_byte_float_conversion(TestCase *tc, void *data)
{
    int i;
    (void)data;

    for (i = 0; i < 256; i++) {
        Aiequal(i, frt_float2byte(frt_byte2float((char)i)));
    }
}

static int my_doc_freq(FrtSearcher *searcher, FrtSymbol field,
                       const char *term)
{
    (void)searcher; (void)field; (void)term;
    return 9;
}

static int my_max_doc(FrtSearcher *searcher)
{
    (void)searcher;
    return 10;
}

static void test_explanation(TestCase *tc, void *data)
{
    FrtExplanation *expl = frt_expl_new(1.6f, "short description");
    char *str = frt_expl_to_s(expl);
    (void)data;
    Asequal("1.6 = short description\n", str);
    free(str);
    frt_expl_add_detail(expl, frt_expl_new(0.8f, "half the score"));
    frt_expl_add_detail(expl, frt_expl_new(2.0f, "to make the difference"));
    frt_expl_add_detail(expl->details[1], frt_expl_new(0.5f, "sub-sub"));
    frt_expl_add_detail(expl->details[1], frt_expl_new(4.0f, "another sub-sub"));
    frt_expl_add_detail(expl->details[0], frt_expl_new(0.8f, "and sub-sub for 1st sub"));

    str = frt_expl_to_s(expl);
    Asequal("1.6 = short description\n"
            "  0.8 = half the score\n"
            "    0.8 = and sub-sub for 1st sub\n"
            "  2.0 = to make the difference\n"
            "    0.5 = sub-sub\n"
            "    4.0 = another sub-sub\n", str);
    frt_expl_destroy(expl);
    free(str);
}

static void test_default_similarity(TestCase *tc, void *data)
{
    FrtPhrasePosition positions[4];
    FrtSearcher searcher;
    FrtSimilarity *dsim = frt_sim_create_default();
    (void)data;
    positions[0].pos = 0;
    positions[0].terms = frt_ary_new_type(char *);
    frt_ary_push(positions[0].terms, (char *)"term1");
    frt_ary_push(positions[0].terms, (char *)"term2");
    frt_ary_push(positions[0].terms, (char *)"term3");

    positions[1].pos = 0;
    positions[1].terms = frt_ary_new_type(char *);
    frt_ary_push(positions[0].terms, (char *)"term1");
    frt_ary_push(positions[0].terms, (char *)"term2");

    positions[2].pos = -100;
    positions[2].terms = frt_ary_new_type(char *);
    frt_ary_push(positions[0].terms, (char *)"term1");

    positions[3].pos = 100;
    positions[3].terms = frt_ary_new_type(char *);
    frt_ary_push(positions[3].terms, (char *)"term1");
    frt_ary_push(positions[3].terms, (char *)"term2");
    frt_ary_push(positions[3].terms, (char *)"term2");
    frt_ary_push(positions[3].terms, (char *)"term3");
    frt_ary_push(positions[3].terms, (char *)"term4");
    frt_ary_push(positions[3].terms, (char *)"term5");

    Afequal(1.0/4, frt_sim_length_norm(dsim, field, 16));
    Afequal(1.0/4, frt_sim_query_norm(dsim, 16));
    Afequal(3.0, frt_sim_tf(dsim, 9));
    Afequal(1.0/10, frt_sim_sloppy_freq(dsim, 9));
    Afequal(1.0, frt_sim_idf(dsim, 9, 10));
    Afequal(4.0, frt_sim_coord(dsim, 12, 3));
    searcher.doc_freq = &my_doc_freq;
    searcher.max_doc = &my_max_doc;
    Afequal(1.0, frt_sim_idf_term(dsim, field, positions[0].terms[0], &searcher));
    Afequal(12.0, frt_sim_idf_phrase(dsim, field, positions, 4, &searcher));

    frt_ary_free(positions[0].terms);
    frt_ary_free(positions[1].terms);
    frt_ary_free(positions[2].terms);
    frt_ary_free(positions[3].terms);
}

typedef struct DoubleFilter {
    FrtTokenFilter super;
    FrtToken *tk;
} DoubleFilter;

static FrtToken *dbl_tf_next(FrtTokenStream *ts)
{
    FrtToken *tk;
    tk = ((DoubleFilter *)ts)->tk;
    if (tk && islower(tk->text[0])) {
        char *t = tk->text;
        while (*t) {
            *t = toupper(*t);
            t++;
        }
        tk->pos_inc = 1;
    }
    else {
        tk = ((DoubleFilter *)ts)->tk
            = ((FrtTokenFilter *)ts)->sub_ts->next(((FrtTokenFilter *)ts)->sub_ts);
        if (tk && islower(tk->text[0])) {
            tk->pos_inc = 0;
        }
    }
    return tk;
}

static FrtTokenStream *dbl_tf_clone_i(FrtTokenStream *ts)
{
    return frt_filter_clone_size(ts, sizeof(DoubleFilter));
}

static FrtTokenStream *dbl_tf_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(DoubleFilter, sub_ts);
    ts->next           = &dbl_tf_next;
    ts->clone_i        = &dbl_tf_clone_i;
    return ts;
}

FrtAnalyzer *dbl_analyzer_new()
{
    FrtTokenStream *ts;
    ts = dbl_tf_new(frt_whitespace_tokenizer_new(false));
    return frt_analyzer_new(ts, NULL, NULL);
}

struct Data {
    const char *date;
    const char *field;
    const char *cat;
    const char *number;
};

#define SEARCH_DOCS_SIZE 18
struct Data test_data[SEARCH_DOCS_SIZE] = {
    {"20050930", "word1",
        "cat1/",                ".123"},
    {"20051001", "word1 word2 the quick brown fox the quick brown fox",
        "cat1/sub1",            "0.0"},
    {"20051002", "word1 word3 one two one",
        "cat1/sub1/subsub1",    "908.123434"},
    {"20051003", "word1 word3 one two",
        "cat1/sub2",            "3999"},
    /* we have 33 * "word2" below to cause cache miss in TermQuery */
    {"20051004", "word1 word2 word2 word2 word2 word2 word2 word2 word2 "
                 "word2 word2 word2 word2 word2 word2 word2 word2 word2 "
                 "word2 word2 word2 word2 word2 word2 word2 word2 word2 "
                 "word2 word2 word2 word2 word2 word2 word2",
        "cat1/sub2/subsub2",    "+.3413"},
    {"20051005", "word1 one two x x x x x one two",
        "cat2/sub1",            "-1.1298"},
    {"20051006", "word1 word3",
        "cat2/sub1",            "2"},
    {"20051007", "word1",
        "cat2/sub1",            "+8.894"},
    {"20051008", "word1 word2 word3 the fast brown fox",
        "cat2/sub1",            "+84783.13747"},
    {"20051009", "word1",
        "cat3/sub1",            "10.0"},
    {"20051010", "word1",
        "cat3/sub1",            "1"},
    {"20051011", "word1 word3 the quick red fox",
        "cat3/sub1",            "-12518419"},
    {"20051012", "word1",
        "cat3/sub1",            "10"},
    {"20051013", "word1",
        "cat1/sub2",            "15682954"},
    {"20051014", "word1 word3 the quick hairy fox",
        "cat1/sub1",            "98132"},
    {"20051015", "word1",
        "cat1/sub2/subsub1",    "-.89321"},
    {"20051016", "word1 the quick fox is brown and hairy and a little red",
        "cat1/sub1/subsub2",    "-89"},
    {"20051017", "word1 the brown fox is quick and red",
        "cat1/",                "-1.0"}
};

static void prepare_search_index(FrtStore *store)
{
    int i;
    FrtIndexWriter *iw;

    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    FrtFieldInfo *fi = frt_fi_new(rb_intern("empty-field"), FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
    frt_fis_add_field(fis, fi);
    frt_index_create(store, fis);
    frt_fis_deref(fis);
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    iw = frt_iw_open(store, dbl_analyzer_new(), NULL);
    for (i = 0; i < SEARCH_DOCS_SIZE; i++) {
        FrtDocument *doc = frt_doc_new();
        doc->boost = (float)(i+1);
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(date), (char *)test_data[i].date, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(field), (char *)test_data[i].field, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(cat), (char *)test_data[i].cat, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(number), (char *)test_data[i].number, enc));
        frt_iw_add_doc(iw, doc);
       frt_doc_destroy(doc);
    }
    frt_iw_close(iw);
}

static void test_get_doc(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtDocument *doc;
    FrtDocField *df;
    Aiequal(SEARCH_DOCS_SIZE, frt_searcher_max_doc(searcher));

    doc = frt_searcher_get_doc(searcher, 0);
    df = frt_doc_get_field(doc, date);
    Aiequal(1, df->size);
    Asequal("20050930", df->data[0]);
   frt_doc_destroy(doc);

    doc = frt_searcher_get_doc(searcher, 4);
    df = frt_doc_get_field(doc, cat);
    Aiequal(1, df->size);
    Asequal("cat1/sub2/subsub2", df->data[0]);
   frt_doc_destroy(doc);

    doc = frt_searcher_get_doc(searcher, 12);
    df = frt_doc_get_field(doc, date);
    Aiequal(1, df->size);
    Asequal("20051012", df->data[0]);
   frt_doc_destroy(doc);
}

void check_to_s(TestCase *tc, FrtQuery *query, FrtSymbol field, const char *q_str)
{
    char *q_res = query->to_s(query, field);
    Asequal(q_str, q_res);
    free(q_res);
}

void tst_check_hits(TestCase *tc, FrtSearcher *searcher, FrtQuery *query, const char *expected_hits, int top)
{
    static int num_array[ARRAY_SIZE];
    static int num_array2[ARRAY_SIZE];
    int i, count;
    int total_hits = s2l(expected_hits, num_array);
    FrtTopDocs *top_docs = frt_searcher_search(searcher, query, 0, total_hits + 1, NULL, NULL, NULL);
    frt_p_pause();
    if (!tc->failed && !Aiequal(total_hits, top_docs->total_hits)) {
        int i;
        Tmsg_nf("\texpected docs:\n\t    ");
        for (i = 0; i < total_hits; i++) {
            Tmsg_nf("%d ", num_array[i]);
        }
        Tmsg_nf("\n\tseen docs:\n\t    ");
        for (i = 0; i < top_docs->size; i++) {
            Tmsg_nf("%d ", top_docs->hits[i]->doc);
        }
        Tmsg_nf("\n");
    }
    Aiequal(total_hits, top_docs->size);
    /* FIXME add this code once search_unscored is working
    frt_searcher_search_unscored(searcher, query, buf, ARRAY_SIZE, 0);
    Aaiequal(num_array, buf, total_hits);
    */

    if ((top >= 0) && top_docs->size)
        Aiequal(top, top_docs->hits[0]->doc);
    for (i = 0; i < top_docs->size; i++) {
        FrtHit *hit = top_docs->hits[i];
        float normalized_score = hit->score / top_docs->max_score;
        Assert(0.0 < normalized_score && normalized_score <= 1.0, "hit->score <%f> is out of range (0.0..1.0]", normalized_score);
        Assert(frt_ary_includes(num_array, total_hits, hit->doc), "doc %d was found unexpectedly", hit->doc);
        /* only check the explanation if we got the correct docs. Obviously we
         * might want to remove this to visually check the explanations */
        if (!tc->failed && total_hits == top_docs->total_hits) {
            FrtExplanation *e = frt_searcher_explain(searcher, query, hit->doc);
            if (! Afequal(hit->score, e->value)) {
               char *t;
               Tmsg("\n\"\"\"\n%d>>\n%f\n%s\n\"\"\"\n", hit->doc, hit->score, t = frt_expl_to_s(e));
               free(t);
            }
            frt_expl_destroy(e);
        }
    }
    frt_td_destroy(top_docs);

    /* test search_unscored method */
    qsort(num_array, total_hits, sizeof(int), &frt_icmp_risky);
    count = frt_searcher_search_unscored(searcher, query, num_array2, ARRAY_SIZE, 0);
    Aaiequal(num_array, num_array2, total_hits);
    if (count > 3) {
        count = frt_searcher_search_unscored(searcher, query, num_array2, ARRAY_SIZE, num_array2[3]);
        Aaiequal(num_array + 3, num_array2, count);
    }
    frt_p_resume();
}

void check_match_vector(TestCase *tc, FrtSearcher *searcher, FrtQuery *query,
                        int doc, FrtSymbol field, const char *ranges)
{
    static int range_array[ARRAY_SIZE];
    FrtMatchVector *mv = frt_searcher_get_match_vector(searcher, query, doc, field);
    int num_matches = s2l(ranges, range_array)/2;
    if (Aiequal(num_matches, mv->size)) {
        int i;
        for (i = 0; i < num_matches; i++) {
            Aiequal(range_array[i*2    ], mv->matches[i].start);
            Aiequal(range_array[i*2 + 1], mv->matches[i].end);
        }
    }
    frt_matchv_destroy(mv);
}

static void test_term_query(TestCase *tc, void *data)
{
    FrtHashSet *hs;
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtTopDocs *top_docs;
    FrtWeight *w;
    char *t, e[100];
    FrtQuery *tq = frt_tq_new(field, "word2");
    check_to_s(tc, tq, field, "word2");
    check_to_s(tc, tq, (FrtSymbol)NULL, "field:word2");
    tq->boost = 100;
    tst_check_hits(tc, searcher, tq, "4, 8, 1", -1);
    check_to_s(tc, tq, field, "word2^100.0");
    check_to_s(tc, tq, (FrtSymbol)NULL, "field:word2^100.0");

    /* test TermWeight.to_s */
    w = searcher->create_weight(searcher, tq);
    sprintf(e, "TermWeight("FRT_DBL2S")", w->value);
    t = w->to_s(w); Asequal(e, t); free(t);
    tq->boost = 10.5f;
    sprintf(e, "TermWeight("FRT_DBL2S")", w->value);
    t = w->to_s(w); Asequal(e, t); free(t);
    w->destroy(w);

    frt_q_deref(tq);

    tq = frt_tq_new(field, "2342");
    tst_check_hits(tc, searcher, tq, "", -1);
    frt_q_deref(tq);

    tq = frt_tq_new(field, "");
    tst_check_hits(tc, searcher, tq, "", -1);
    frt_q_deref(tq);

    tq = frt_tq_new(rb_intern("not_a_field"), "word2");
    tst_check_hits(tc, searcher, tq, "", -1);
    frt_q_deref(tq);

    tq = frt_tq_new(field, "word1");
    top_docs = frt_searcher_search(searcher, tq, 0, 10, NULL, NULL, NULL);
    Aiequal(SEARCH_DOCS_SIZE, top_docs->total_hits);
    Aiequal(10, top_docs->size);
    frt_td_destroy(top_docs);

    top_docs = frt_searcher_search(searcher, tq, 0, 20, NULL, NULL, NULL);
    Aiequal(SEARCH_DOCS_SIZE, top_docs->total_hits);
    Aiequal(SEARCH_DOCS_SIZE, top_docs->size);
    frt_td_destroy(top_docs);

    top_docs = frt_searcher_search(searcher, tq, 10, 20, NULL, NULL, NULL);
    Aiequal(SEARCH_DOCS_SIZE, top_docs->total_hits);
    Aiequal(SEARCH_DOCS_SIZE - 10, top_docs->size);
    frt_td_destroy(top_docs);
    frt_q_deref(tq);

    tq = frt_tq_new(field, "quick");
    /* test get_matchv_i */
    tst_check_hits(tc, searcher, tq, "1,11,14,16,17", -1);
    check_match_vector(tc, searcher, tq, 1, field, "3,3,7,7");

    /* test extract_terms */
    hs = frt_hs_new((frt_hash_ft)&frt_term_hash, (frt_eq_ft)&frt_term_eq, (frt_free_ft)&frt_term_destroy);
    tq->extract_terms(tq, hs);
    Aiequal(1, hs->size);
    Asequal("quick", ((FrtTerm *)hs->first->elem)->text);
    Apequal(rb_id2name(field), rb_id2name(((FrtTerm *)hs->first->elem)->field));
    frt_hs_destroy(hs);
    frt_q_deref(tq);
}

static void test_term_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_tq_new(rb_intern("A"), "a");

    q2 = frt_tq_new(rb_intern("A"), "a");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_tq_new(rb_intern("A"), "b");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "texts differ");
    Assert(!frt_q_eq(q1, q2), "texts differ");
    frt_q_deref(q2);

    q2 = frt_tq_new(rb_intern("B"), "a");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "fields differ");
    Assert(!frt_q_eq(q1, q2), "fields differ");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_boolean_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *bq = frt_bq_new(false);
    FrtQuery *tq1 = frt_tq_new(field, "word1");
    FrtQuery *tq2 = frt_tq_new(field, "word3");
    FrtQuery *tq3 = frt_tq_new(field, "word2");
    frt_bq_add_query_nr(bq, tq1, FRT_BC_MUST);
    frt_bq_add_query_nr(bq, tq2, FRT_BC_MUST);
    tst_check_hits(tc, searcher, bq, "2, 3, 6, 8, 11, 14", 14);

    frt_bq_add_query_nr(bq, tq3, FRT_BC_SHOULD);
    tst_check_hits(tc, searcher, bq, "2, 3, 6, 8, 11, 14", 8);
    frt_q_deref(bq);

    tq2 = frt_tq_new(field, "word3");
    tq3 = frt_tq_new(field, "word2");
    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, tq2, FRT_BC_MUST);
    frt_bq_add_query_nr(bq, tq3, FRT_BC_MUST_NOT);
    tst_check_hits(tc, searcher, bq, "2, 3, 6, 11, 14", -1);
    frt_q_deref(bq);

    tq2 = frt_tq_new(field, "word3");
    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, tq2, FRT_BC_MUST_NOT);
    tst_check_hits(tc, searcher, bq, "0,1,4,5,7,9,10,12,13,15,16,17", -1);
    frt_q_deref(bq);

    tq2 = frt_tq_new(field, "word3");
    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, tq2, FRT_BC_SHOULD);
    tst_check_hits(tc, searcher, bq, "2, 3, 6, 8, 11, 14", 14);
    frt_q_deref(bq);

    tq2 = frt_tq_new(field, "word3");
    tq3 = frt_tq_new(field, "word2");
    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, tq2, FRT_BC_SHOULD);
    frt_bq_add_query_nr(bq, tq3, FRT_BC_SHOULD);
    tst_check_hits(tc, searcher, bq, "1, 2, 3, 4, 6, 8, 11, 14", -1);
    frt_q_deref(bq);

    bq = frt_bq_new(false);
    tq1 = frt_tq_new(rb_intern("not a field"), "word1");
    tq2 = frt_tq_new(rb_intern("not a field"), "word3");
    tq3 = frt_tq_new(field, "word2");
    frt_bq_add_query_nr(bq, tq1, FRT_BC_SHOULD);
    frt_bq_add_query_nr(bq, tq2, FRT_BC_SHOULD);
    tst_check_hits(tc, searcher, bq, "", -1);

    frt_bq_add_query_nr(bq, tq3, FRT_BC_SHOULD);
    tst_check_hits(tc, searcher, bq, "1, 4, 8", 4);

    frt_q_deref(bq);
}

static void test_boolean_query_hash(TestCase *tc, void *data)
{
    FrtQuery *tq1, *tq2, *tq3, *q1, *q2;
    (void)data;

    tq1 = frt_tq_new(rb_intern("A"), "1");
    tq2 = frt_tq_new(rb_intern("B"), "2");
    tq3 = frt_tq_new(rb_intern("C"), "3");
    q1 = frt_bq_new(false);
    frt_bq_add_query(q1, tq1, FRT_BC_MUST);
    frt_bq_add_query(q1, tq2, FRT_BC_MUST);

    q2 = frt_bq_new(false);
    frt_bq_add_query(q2, tq1, FRT_BC_MUST);
    frt_bq_add_query(q2, tq2, FRT_BC_MUST);

    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q1), "Queries are equal");
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_hash(q1) != frt_q_hash(tq1), "Queries are not equal");
    Assert(!frt_q_eq(q1, tq1), "Queries are not equal");
    Assert(!frt_q_eq(tq1, q1), "Queries are not equal");
    frt_q_deref(q2);

    q2 = frt_bq_new(true);
    frt_bq_add_query(q2, tq1, FRT_BC_MUST);
    frt_bq_add_query(q2, tq2, FRT_BC_MUST);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);

    q2 = frt_bq_new(false);
    frt_bq_add_query(q2, tq1, FRT_BC_SHOULD);
    frt_bq_add_query(q2, tq2, FRT_BC_MUST_NOT);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);

    q2 = frt_bq_new(false);
    frt_bq_add_query(q2, tq1, FRT_BC_MUST);
    frt_bq_add_query(q2, tq2, FRT_BC_MUST);
    frt_bq_add_query(q2, tq3, FRT_BC_MUST);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");

    frt_bq_add_query(q1, tq3, FRT_BC_MUST);

    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    frt_q_deref(q1);
    frt_q_deref(tq1);
    frt_q_deref(tq2);
    frt_q_deref(tq3);
}

static void test_phrase_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtExplanation *explanation;
    FrtQuery *q;
    FrtQuery *phq = frt_phq_new(field);
    FrtWeight *w;
    char *t, e[100];
    check_to_s(tc, phq, field, "\"\"");
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"\"");

    frt_phq_add_term(phq, "quick", 1);
    frt_phq_add_term(phq, "brown", 1);
    frt_phq_add_term(phq, "fox", 1);
    check_to_s(tc, phq, field, "\"quick brown fox\"");
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick brown fox\"");
    tst_check_hits(tc, searcher, phq, "1", 1);

    frt_phq_set_slop(phq, 4);
    tst_check_hits(tc, searcher, phq, "1, 16, 17", 17);

    /* test PhraseWeight.to_s */
    w = searcher->create_weight(searcher, phq);
    sprintf(e, "PhraseWeight("FRT_DBL2S")", w->value);
    t = w->to_s(w); Asequal(e, t); free(t);
    phq->boost = 10.5f;
    sprintf(e, "PhraseWeight("FRT_DBL2S")", w->value);
    t = w->to_s(w); Asequal(e, t); free(t);
    w->destroy(w);

    frt_q_deref(phq);

    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "quick", 1);
    frt_phq_add_term(phq, "fox", 2);
    check_to_s(tc, phq, field, "\"quick <> fox\"");
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick <> fox\"");
    tst_check_hits(tc, searcher, phq, "1, 11, 14", 14);

    frt_phq_set_slop(phq, 1);
    tst_check_hits(tc, searcher, phq, "1, 11, 14, 16", 14);

    frt_phq_set_slop(phq, 4);
    tst_check_hits(tc, searcher, phq, "1, 11, 14, 16, 17", 14);
    frt_phq_add_term(phq, "red", -1);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick red fox\"~4");
    tst_check_hits(tc, searcher, phq, "11", 11);
    frt_phq_add_term(phq, "RED", 0);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick red RED&fox\"~4");
    tst_check_hits(tc, searcher, phq, "11", 11);
    frt_phq_add_term(phq, "QUICK", -1);
    frt_phq_add_term(phq, "red", 0);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick QUICK&red&red RED&fox\"~4");
    tst_check_hits(tc, searcher, phq, "11", 11);
    frt_phq_add_term(phq, "green", 0);
    frt_phq_add_term(phq, "yellow", 0);
    frt_phq_add_term(phq, "sentinel", 1);
    check_to_s(tc, phq, (FrtSymbol)NULL,
               "field:\"quick QUICK&red&red RED&fox&green&yellow sentinel\"~4");
    tst_check_hits(tc, searcher, phq, "", -1);
    frt_q_deref(phq);

    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "the", 0);
    frt_phq_add_term(phq, "WORD3", 0);
    tst_check_hits(tc, searcher, phq, "8, 11, 14", 14);
    frt_phq_add_term(phq, "THE", 1);
    frt_phq_add_term(phq, "quick", 0);
    frt_phq_add_term(phq, "QUICK", 1);
    tst_check_hits(tc, searcher, phq, "11, 14", 14);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"WORD3&the THE&quick QUICK\"");
    frt_q_deref(phq);

    /* test repeating terms check */
    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "one", 0);
    frt_phq_add_term(phq, "two", 1);
    frt_phq_add_term(phq, "one", 1);
    tst_check_hits(tc, searcher, phq, "2", 2);
    frt_phq_set_slop(phq, 2);
    tst_check_hits(tc, searcher, phq, "2", 2);
    frt_q_deref(phq);

    phq = frt_phq_new(rb_intern("not a field"));
    frt_phq_add_term(phq, "the", 0);
    frt_phq_add_term(phq, "quick", 1);
    tst_check_hits(tc, searcher, phq, "", -1);
    explanation = searcher->explain(searcher, phq, 0);
    Afequal(0.0, explanation->value);
    frt_expl_destroy(explanation);
    frt_q_deref(phq);

    /* test single-term case, query is rewritten to TermQuery */
    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "word2", 1);
    tst_check_hits(tc, searcher, phq, "4, 8, 1", -1);
    q = frt_searcher_rewrite(searcher, phq);
    Aiequal(q->type, TERM_QUERY);
    frt_q_deref(q);

    /* test single-position/multi-term query is rewritten as MultiTermQuery */
    frt_phq_append_multi_term(phq, "word3");
    tst_check_hits(tc, searcher, phq, "1,2,3,4,6,8,11,14", -1);
    q = frt_searcher_rewrite(searcher, phq);
    Aiequal(q->type, MULTI_TERM_QUERY);
    frt_q_deref(q);

    /* check boost doesn't break anything */;
    frt_phq_add_term(phq, "one", 1); /* make sure it won't be rewritten */
    phq->boost = 10.0;
    tst_check_hits(tc, searcher, phq, "2,3", -1);
    frt_q_deref(phq);

    /* test get_matchv_i */
    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "quick", 0);
    frt_phq_add_term(phq, "brown", 1);
    tst_check_hits(tc, searcher, phq, "1", -1);
    check_match_vector(tc, searcher, phq, 1, field, "3,4,7,8");

    frt_phq_set_slop(phq, 4);
    tst_check_hits(tc, searcher, phq, "1,16,17", -1);
    check_match_vector(tc, searcher, phq, 16, field, "2,5");

    frt_phq_add_term(phq, "chicken", 1);
    tst_check_hits(tc, searcher, phq, "", -1);
    check_match_vector(tc, searcher, phq, 16, field, "");
    frt_q_deref(phq);
}

static void test_phrase_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_phq_new(field);
    frt_phq_add_term(q1, "quick", 1);
    frt_phq_add_term(q1, "brown", 2);
    frt_phq_add_term(q1, "fox", 0);

    q2 = frt_phq_new(field);
    frt_phq_add_term(q2, "quick", 1);
    frt_phq_add_term(q2, "brown", 2);
    frt_phq_add_term(q2, "fox", 0);

    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q1), "Test query equals itself");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_phq_set_slop(q2, 5);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);

    q2 = frt_phq_new(field);
    frt_phq_add_term(q2, "quick", 1);
    frt_phq_add_term(q2, "brown", 1);
    frt_phq_add_term(q2, "fox", 1);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);

    q2 = frt_phq_new(field);
    frt_phq_add_term(q2, "fox", 1);
    frt_phq_add_term(q2, "brown", 2);
    frt_phq_add_term(q2, "quick", 0);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);

    q2 = frt_phq_new(rb_intern("other_field"));
    frt_phq_add_term(q2, "quick", 1);
    frt_phq_add_term(q2, "brown", 2);
    frt_phq_add_term(q2, "fox", 0);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);
    frt_q_deref(q1);
}

static void test_multi_phrase_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *phq, *q;


    phq = frt_phq_new(field);
    /* ok to use append_multi_term to start */
    frt_phq_append_multi_term(phq, "quick");
    frt_phq_append_multi_term(phq, "fast");
    tst_check_hits(tc, searcher, phq, "1, 8, 11, 14, 16, 17", -1);
    check_to_s(tc, phq, field, "\"quick|fast\"");
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick|fast\"");

    frt_phq_add_term(phq, "brown", 1);
    frt_phq_append_multi_term(phq, "red");
    frt_phq_append_multi_term(phq, "hairy");
    frt_phq_add_term(phq, "fox", 1);
    check_to_s(tc, phq, field, "\"quick|fast brown|red|hairy fox\"");
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick|fast brown|red|hairy fox\"");
    tst_check_hits(tc, searcher, phq, "1, 8, 11, 14", -1);

    frt_phq_set_slop(phq, 4);
    tst_check_hits(tc, searcher, phq, "1, 8, 11, 14, 16, 17", -1);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"quick|fast brown|red|hairy fox\"~4");

    frt_phq_add_term(phq, "QUICK", -1);
    frt_phq_append_multi_term(phq, "FAST");
    tst_check_hits(tc, searcher, phq, "1, 8, 11, 14, 16, 17", -1);
    check_to_s(tc, phq, (FrtSymbol)NULL,
               "field:\"quick|fast QUICK|FAST&brown|red|hairy fox\"~4");

    frt_phq_add_term(phq, "WORD3", -3);
    frt_phq_append_multi_term(phq, "WORD2");
    tst_check_hits(tc, searcher, phq, "1, 8, 11, 14", -1);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"WORD3|WORD2 quick|fast "
               "QUICK|FAST&brown|red|hairy fox\"~4");

    frt_q_deref(phq);

    /* test repeating terms check */
    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "WORD3", 0);
    frt_phq_append_multi_term(phq, "x");
    frt_phq_add_term(phq, "one", 0);
    frt_phq_add_term(phq, "two", 1);
    frt_phq_add_term(phq, "one", 1);
    tst_check_hits(tc, searcher, phq, "2", -1);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"WORD3|x&one two one\"");

    frt_phq_set_slop(phq, 4);
    tst_check_hits(tc, searcher, phq, "2", -1);
    check_to_s(tc, phq, (FrtSymbol)NULL, "field:\"WORD3|x&one two one\"~4");
    frt_q_deref(phq);

    /* test phrase query on non-existing field doesn't break anything */
    phq = frt_phq_new(rb_intern("not a field"));
    frt_phq_add_term(phq, "the", 0);
    frt_phq_add_term(phq, "quick", 1);
    frt_phq_append_multi_term(phq, "THE");
    tst_check_hits(tc, searcher, phq, "", -1);
    frt_q_deref(phq);

    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "word2", 1);
    frt_phq_append_multi_term(phq, "word3");
    tst_check_hits(tc, searcher, phq, "1, 2, 3, 4, 6, 8, 11, 14", -1);
    q = frt_searcher_rewrite(searcher, phq);
    Aiequal(q->type, MULTI_TERM_QUERY);
    frt_q_deref(phq);
    frt_q_deref(q);

    /* test get_matchv_i */
    phq = frt_phq_new(field);
    frt_phq_add_term(phq, "quick", 0);
    frt_phq_add_term(phq, "brown", 1);
    frt_phq_append_multi_term(phq, "dirty");
    frt_phq_append_multi_term(phq, "red");
    tst_check_hits(tc, searcher, phq, "1,11", -1);
    check_match_vector(tc, searcher, phq, 1, field, "3,4,7,8");

    frt_phq_set_slop(phq, 1);
    tst_check_hits(tc, searcher, phq, "1,11,17", -1);
    check_match_vector(tc, searcher, phq, 1, field, "3,4,7,8");
    check_match_vector(tc, searcher, phq, 17, field, "5,7");

    frt_phq_add_term(phq, "chicken", 1);
    frt_phq_append_multi_term(phq, "turtle");
    tst_check_hits(tc, searcher, phq, "", -1);
    check_match_vector(tc, searcher, phq, 17, field, "");
    frt_q_deref(phq);
}

static void test_multi_phrase_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_phq_new(field);
    frt_phq_add_term(q1, "quick", 1);
    frt_phq_append_multi_term(q1, "fast");
    frt_phq_add_term(q1, "brown", 1);
    frt_phq_append_multi_term(q1, "red");
    frt_phq_append_multi_term(q1, "hairy");
    frt_phq_add_term(q1, "fox", 1);

    q2 = frt_phq_new(field);
    frt_phq_add_term(q2, "quick", 1);
    frt_phq_append_multi_term(q2, "fast");
    frt_phq_add_term(q2, "brown", 1);
    frt_phq_append_multi_term(q2, "red");
    frt_phq_append_multi_term(q2, "hairy");
    frt_phq_add_term(q2, "fox", 1);

    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q1), "Test query equals itself");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_phq_set_slop(q2, 5);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");

    frt_phq_append_multi_term(q2, "hairy");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);

    /* test same but different order */
    q2 = frt_phq_new(field);
    frt_phq_add_term(q2, "quick", 1);
    frt_phq_append_multi_term(q2, "fast");
    frt_phq_add_term(q2, "fox", 1);
    frt_phq_add_term(q2, "brown", 1);
    frt_phq_append_multi_term(q2, "red");
    frt_phq_append_multi_term(q2, "hairy");

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);

    /* test same but different pos values */
    q2 = frt_phq_new(field);
    frt_phq_add_term(q2, "quick", 1);
    frt_phq_append_multi_term(q2, "fast");
    frt_phq_add_term(q2, "brown", 1);
    frt_phq_append_multi_term(q2, "red");
    frt_phq_append_multi_term(q2, "hairy");
    frt_phq_add_term(q2, "fox", 2);

    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void mtq_zero_max_terms(void *p)
{ (void)p; frt_multi_tq_new_conf(field, 0, 0.5); }

static void test_multi_term_query(TestCase *tc, void *data)
{
    FrtWeight *w;
    char *t, e[100];
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *mtq, *bq;
    FrtExplanation *exp;

    Araise(FRT_ARG_ERROR, &mtq_zero_max_terms, NULL);

    mtq = frt_multi_tq_new_conf(field, 4, 0.5);
    tst_check_hits(tc, searcher, mtq, "", -1);
    check_to_s(tc, mtq, field, "\"\"");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"\"");

    frt_multi_tq_add_term(mtq, "brown");
    tst_check_hits(tc, searcher, mtq, "1, 8, 16, 17", -1);
    check_to_s(tc, mtq, field, "\"brown\"");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"brown\"");


    /* 0.4f boost is below the 0.5 threshold so term is ignored */
    frt_multi_tq_add_term_boost(mtq, "fox", 0.4f);
    tst_check_hits(tc, searcher, mtq, "1, 8, 16, 17", -1);
    check_to_s(tc, mtq, field, "\"brown\"");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"brown\"");

    /* 0.6f boost is above the 0.5 threshold so term is included */
    frt_multi_tq_add_term_boost(mtq, "fox", 0.6f);
    tst_check_hits(tc, searcher, mtq, "1, 8, 11, 14, 16, 17", -1);
    check_to_s(tc, mtq, field, "\"fox^0.6|brown\"");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"fox^0.6|brown\"");

    frt_multi_tq_add_term_boost(mtq, "fast", 50.0f);
    tst_check_hits(tc, searcher, mtq, "1, 8, 11, 14, 16, 17", 8);
    check_to_s(tc, mtq, field, "\"fox^0.6|brown|fast^50.0\"");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"fox^0.6|brown|fast^50.0\"");


    mtq->boost = 80.1f;
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"fox^0.6|brown|fast^50.0\"^80.1");
    frt_multi_tq_add_term(mtq, "word1");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"fox^0.6|brown|word1|fast^50.0\"^80.1");
    frt_multi_tq_add_term(mtq, "word2");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"brown|word1|word2|fast^50.0\"^80.1");
    frt_multi_tq_add_term(mtq, "word3");
    check_to_s(tc, mtq, (FrtSymbol)NULL, "field:\"brown|word1|word2|fast^50.0\"^80.1");

    /* test MultiTermWeight.to_s */
    w = searcher->create_weight(searcher, mtq);
    sprintf(e, "MultiTermWeight("FRT_DBL2S")", w->value);
    t = w->to_s(w); Asequal(e, t); free(t);
    mtq->boost = 10.5f;
    sprintf(e, "MultiTermWeight("FRT_DBL2S")", w->value);
    t = w->to_s(w); Asequal(e, t); free(t);
    w->destroy(w);

    frt_q_deref(mtq);

    /* exercise tdew_skip_to */
    mtq = frt_multi_tq_new_conf(field, 4, 0.5);
    frt_multi_tq_add_term(mtq, "brown");
    frt_multi_tq_add_term_boost(mtq, "fox", 0.6f);
    frt_multi_tq_add_term(mtq, "word1");
    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, frt_tq_new(field, "quick"), FRT_BC_MUST);
    frt_bq_add_query(bq, mtq, FRT_BC_MUST);
    tst_check_hits(tc, searcher, bq, "1, 11, 14, 16, 17", -1);
    check_to_s(tc, bq, field, "+quick +\"fox^0.6|brown|word1\"");
    check_to_s(tc, bq, (FrtSymbol)NULL, "+field:quick +field:\"fox^0.6|brown|word1\"");
    frt_q_deref(bq);
    frt_q_deref(mtq);

    /* test incorrect field explanation */
    mtq = frt_multi_tq_new_conf(rb_intern("hello"), 4, 0.5);
    frt_multi_tq_add_term(mtq, "brown");
    frt_multi_tq_add_term(mtq, "quick");
    exp = frt_searcher_explain(searcher, mtq, 0);
    Afequal(0.0, exp->value);
    Asequal("field \"hello\" does not exist in the index", exp->description);

    frt_q_deref(mtq);
}

static void test_multi_term_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1 = frt_multi_tq_new_conf(field, 100, 0.4);
    FrtQuery *q2 = frt_multi_tq_new(field);
    (void)data;


    check_to_s(tc, q1, (FrtSymbol)NULL, "field:\"\"");
    Assert(frt_q_hash(q1) == frt_q_hash(q2), "Queries should be equal");
    Assert(frt_q_eq(q1, q1), "Same queries should be equal");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_multi_tq_add_term(q1, "word1");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");

    frt_multi_tq_add_term(q2, "word1");
    Assert(frt_q_hash(q1) == frt_q_hash(q2), "Queries should be equal");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_multi_tq_add_term(q1, "word2");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");

    frt_multi_tq_add_term_boost(q2, "word2", 1.5);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");

    frt_q_deref(q1);
    frt_q_deref(q2);
}

static void test_prefix_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *prq = frt_prefixq_new(cat, "cat1");
    check_to_s(tc, prq, cat, "cat1*");
    tst_check_hits(tc, searcher, prq, "0, 1, 2, 3, 4, 13, 14, 15, 16, 17", -1);
    frt_q_deref(prq);

    prq = frt_prefixq_new(cat, "cat1/sub2");
    check_to_s(tc, prq, cat, "cat1/sub2*");
    prq->boost = 20.0f;
    check_to_s(tc, prq, cat, "cat1/sub2*^20.0");
    tst_check_hits(tc, searcher, prq, "3, 4, 13, 15", -1);
    frt_q_deref(prq);

    prq = frt_prefixq_new(cat, "cat1/sub");
    check_to_s(tc, prq, cat, "cat1/sub*");
    tst_check_hits(tc, searcher, prq, "1, 2, 3, 4, 13, 14, 15, 16", -1);
    frt_q_deref(prq);

    prq = frt_prefixq_new(rb_intern("unknown field"), "cat1/sub");
    check_to_s(tc, prq, cat, "unknown field:cat1/sub*");
    tst_check_hits(tc, searcher, prq, "", -1);
    frt_q_deref(prq);

    prq = frt_prefixq_new(cat, "unknown_term");
    check_to_s(tc, prq, cat, "unknown_term*");
    tst_check_hits(tc, searcher, prq, "", -1);
    frt_q_deref(prq);
}

static void test_prefix_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_prefixq_new(rb_intern("A"), "a");

    q2 = frt_prefixq_new(rb_intern("A"), "a");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "TermQueries are equal");
    Assert(frt_q_eq(q1, q1), "TermQueries are same");
    frt_q_deref(q2);

    q2 = frt_prefixq_new(rb_intern("A"), "b");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "TermQueries are not equal");
    Assert(!frt_q_eq(q1, q2), "TermQueries are not equal");
    frt_q_deref(q2);

    q2 = frt_prefixq_new(rb_intern("B"), "a");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "TermQueries are not equal");
    Assert(!frt_q_eq(q1, q2), "TermQueries are not equal");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void rq_new_lower_gt_upper(void *p)
{ (void)p; frt_rq_new(date, "20050101", "20040101", true, true); }

static void rq_new_include_lower_and_null_lower(void *p)
{ (void)p; frt_rq_new(date, NULL, "20040101", true, true); }

static void rq_new_include_upper_and_null_upper(void *p)
{ (void)p; frt_rq_new(date, "20050101", NULL, true, true); }

static void rq_new_null_lower_and_upper(void *p)
{ (void)p; frt_rq_new(date, NULL, NULL, false, false); }

static void test_range_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *rq;

    Araise(FRT_ARG_ERROR, &rq_new_lower_gt_upper, NULL);
    Araise(FRT_ARG_ERROR, &rq_new_include_lower_and_null_lower, NULL);
    Araise(FRT_ARG_ERROR, &rq_new_include_upper_and_null_upper, NULL);
    Araise(FRT_ARG_ERROR, &rq_new_null_lower_and_upper, NULL);

    rq = frt_rq_new(date, "20051006", "20051010", true, true);
    tst_check_hits(tc, searcher, rq, "6,7,8,9,10", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, "20051006", "20051010", false, true);
    tst_check_hits(tc, searcher, rq, "7,8,9,10", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, "20051006", "20051010", true, false);
    tst_check_hits(tc, searcher, rq, "6,7,8,9", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, "20051006", "20051010", false, false);
    tst_check_hits(tc, searcher, rq, "7,8,9", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, NULL, "20051003", false, true);
    tst_check_hits(tc, searcher, rq, "0,1,2,3", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, NULL, "20051003", false, false);
    tst_check_hits(tc, searcher, rq, "0,1,2", -1);
    frt_q_deref(rq);

    rq = frt_rq_new_less(date, "20051003", true);
    tst_check_hits(tc, searcher, rq, "0,1,2,3", -1);
    frt_q_deref(rq);

    rq = frt_rq_new_less(date, "20051003", false);
    tst_check_hits(tc, searcher, rq, "0,1,2", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, "20051014", NULL, true, false);
    tst_check_hits(tc, searcher, rq, "14,15,16,17", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(date, "20051014", NULL, false, false);
    tst_check_hits(tc, searcher, rq, "15,16,17", -1);
    frt_q_deref(rq);

    rq = frt_rq_new_more(date, "20051014", true);
    tst_check_hits(tc, searcher, rq, "14,15,16,17", -1);
    frt_q_deref(rq);

    rq = frt_rq_new_more(date, "20051014", false);
    tst_check_hits(tc, searcher, rq, "15,16,17", -1);
    frt_q_deref(rq);

    rq = frt_rq_new(rb_intern("not_a_field"), "20051006", "20051010", false, false);
    tst_check_hits(tc, searcher, rq, "", -1);
    frt_q_deref(rq);

    /* below range - no results */
    rq = frt_rq_new(date, "10051006", "10051010", false, false);
    tst_check_hits(tc, searcher, rq, "", -1);
    frt_q_deref(rq);

    /* above range - no results */
    rq = frt_rq_new(date, "30051006", "30051010", false, false);
    tst_check_hits(tc, searcher, rq, "", -1);
    frt_q_deref(rq);

    /* test get_matchv_i */
    /* NOTE: if you are reading this to learn how to use RangeQuery the
     * following is not a good idea. You should usually only use a RangeQuery
     * on an untokenized field. This is just done for testing purposes to
     * check that it works correctly. */
    rq = frt_rq_new(field, "word1", "word3", true, true);
    tst_check_hits(tc, searcher, rq, "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17", -1);
    check_match_vector(tc, searcher, rq, 2, rb_intern("not a field"), "");
    check_match_vector(tc, searcher, rq, 2, field, "0,0,1,1");
    frt_q_deref(rq);

    rq = frt_rq_new(field, "word1", "word3", false, true);
    check_match_vector(tc, searcher, rq, 2, field, "1,1");
    frt_q_deref(rq);

    rq = frt_rq_new(field, "word1", "word3", true, false);
    check_match_vector(tc, searcher, rq, 2, field, "0,0");
    frt_q_deref(rq);

    rq = frt_rq_new(field, "word1", "word3", false, false);
    check_match_vector(tc, searcher, rq, 2, field, "");
    frt_q_deref(rq);
}

static void test_range_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_rq_new(date, "20051006", "20051010", true, true);
    q2 = frt_rq_new(date, "20051006", "20051010", true, true);

    Assert(frt_q_eq(q1, q1), "Test same queries are equal");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_rq_new(date, "20051006", "20051010", true, false);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Upper bound include differs");
    Assert(!frt_q_eq(q1, q2), "Upper bound include differs");
    frt_q_deref(q2);

    q2 = frt_rq_new(date, "20051006", "20051010", false, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Lower bound include differs");
    Assert(!frt_q_eq(q1, q2), "Lower bound include differs");
    frt_q_deref(q2);

    q2 = frt_rq_new(date, "20051006", "20051011", true, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Upper bound differs");
    Assert(!frt_q_eq(q1, q2), "Upper bound differs");
    frt_q_deref(q2);

    q2 = frt_rq_new(date, "20051005", "20051010", true, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Lower bound differs");
    Assert(!frt_q_eq(q1, q2), "Lower bound differs");
    frt_q_deref(q2);

    q2 = frt_rq_new(date, "20051006", NULL, true, false);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Upper bound is NULL");
    Assert(!frt_q_eq(q1, q2), "Upper bound is NULL");
    frt_q_deref(q2);

    q2 = frt_rq_new(date, NULL, "20051010", false, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Lower bound is NULL");
    Assert(!frt_q_eq(q1, q2), "Lower bound is NULL");
    frt_q_deref(q2);

    q2 = frt_rq_new(field, "20051006", "20051010", true, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Field differs");
    Assert(!frt_q_eq(q1, q2), "Field differs");
    frt_q_deref(q2);
    frt_q_deref(q1);

    q1 = frt_rq_new(date, NULL, "20051010", false, true);
    q2 = frt_rq_new(date, NULL, "20051010", false, true);
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);
    frt_q_deref(q1);

    q1 = frt_rq_new(date, "20051010", NULL, true, false);
    q2 = frt_rq_new(date, "20051010", NULL, true, false);
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);
    frt_q_deref(q1);
}

static void trq_new_int_lower_gt_upper(void *p)
{ (void)p; frt_trq_new(date, "20050101", "20040101", true, true); }

static void trq_new_float_lower_gt_upper(void *p)
{ (void)p; frt_trq_new(number, "2.5", "-2.5", true, true); }

static void trq_new_string_lower_gt_upper(void *p)
{ (void)p; frt_trq_new(cat, "cat_b", "cat_a", true, true); }

static void trq_new_include_lower_and_null_lower(void *p)
{ (void)p; frt_trq_new(date, NULL, "20040101", true, true); }

static void trq_new_include_upper_and_null_upper(void *p)
{ (void)p; frt_trq_new(date, "20050101", NULL, true, true); }

static void trq_new_null_lower_and_upper(void *p)
{ (void)p; frt_trq_new(date, NULL, NULL, false, false); }

static void test_typed_range_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *trq;

    Araise(FRT_ARG_ERROR, trq_new_int_lower_gt_upper, NULL);
    Araise(FRT_ARG_ERROR, trq_new_float_lower_gt_upper, NULL);
    Araise(FRT_ARG_ERROR, trq_new_string_lower_gt_upper, NULL);
    Araise(FRT_ARG_ERROR, trq_new_include_lower_and_null_lower, NULL);
    Araise(FRT_ARG_ERROR, trq_new_include_upper_and_null_upper, NULL);
    Araise(FRT_ARG_ERROR, trq_new_null_lower_and_upper, NULL);

    trq = frt_trq_new(number, "-1.0", "1.0", true, true);
    tst_check_hits(tc, searcher, trq, "0,1,4,10,15,17", -1);
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    check_match_vector(tc, searcher, trq, 10, number, "0,0");
    check_match_vector(tc, searcher, trq, 17, number, "0,0");
    check_match_vector(tc, searcher, trq, 2, number, "");
    frt_q_deref(trq);

    trq = frt_trq_new(number, "-1.0", "1.0", false, false);
    tst_check_hits(tc, searcher, trq, "0,1,4,15", -1);
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    check_match_vector(tc, searcher, trq, 10, number, "");
    check_match_vector(tc, searcher, trq, 17, number, "");
    frt_q_deref(trq);

    trq = frt_trq_new(number, "-1.0", "1.0", false, true);
    tst_check_hits(tc, searcher, trq, "0,1,4,10,15", -1);
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    check_match_vector(tc, searcher, trq, 10, number, "0,0");
    check_match_vector(tc, searcher, trq, 17, number, "");
    frt_q_deref(trq);

    trq = frt_trq_new(number, "-1.0", "1.0", true, false);
    tst_check_hits(tc, searcher, trq, "0,1,4,15,17", -1);
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    check_match_vector(tc, searcher, trq, 10, number, "");
    check_match_vector(tc, searcher, trq, 17, number, "0,0");
    frt_q_deref(trq);

    /* test field with no numbers */
    trq = frt_trq_new(field, "-1.0", "1.0", false, true);
    tst_check_hits(tc, searcher, trq, "", -1);
    check_match_vector(tc, searcher, trq, 0, number, "");
    frt_q_deref(trq);

    /* test empty field */
    trq = frt_trq_new(rb_intern("empty-field"), "-1.0", "1.0", false, true);
    tst_check_hits(tc, searcher, trq, "", -1);
    check_match_vector(tc, searcher, trq, 0, number, "");
    frt_q_deref(trq);

    /* FIXME: This was a hexidecimal test but unfortunately scanf doesn't do
     * hexidecimal on some machines. Would be nice to test for this in
     * ./configure when we eventually integrate autotools */
    /* text hexadecimal */
    trq = frt_trq_new(number, "1.0", "10", false, true);
    tst_check_hits(tc, searcher, trq, "6,7,9,12", -1);
    frt_q_deref(trq);

    /* test single bound */
    trq = frt_trq_new(number, NULL, "0", false, true);
    tst_check_hits(tc, searcher, trq, "1,5,11,15,16,17", -1);
    check_match_vector(tc, searcher, trq, 1, number, "0.0");
    check_match_vector(tc, searcher, trq, 5, number, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new_less(number, "0", true);
    tst_check_hits(tc, searcher, trq, "1,5,11,15,16,17", -1);
    check_match_vector(tc, searcher, trq, 1, number, "0.0");
    check_match_vector(tc, searcher, trq, 5, number, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new(number, NULL, "0", false, false);
    tst_check_hits(tc, searcher, trq, "5,11,15,16,17", -1);
    check_match_vector(tc, searcher, trq, 1, number, "");
    check_match_vector(tc, searcher, trq, 5, number, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new_less(number, "0", false);
    tst_check_hits(tc, searcher, trq, "5,11,15,16,17", -1);
    check_match_vector(tc, searcher, trq, 1, number, "");
    check_match_vector(tc, searcher, trq, 5, number, "0,0");
    frt_q_deref(trq);

    /* test single bound */
    trq = frt_trq_new(number, "0", NULL, true, false);
    tst_check_hits(tc, searcher, trq, "0,1,2,3,4,6,7,8,9,10,12,13,14", -1);
    check_match_vector(tc, searcher, trq, 1, number, "0.0");
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new_more(number, "0", true);
    tst_check_hits(tc, searcher, trq, "0,1,2,3,4,6,7,8,9,10,12,13,14", -1);
    check_match_vector(tc, searcher, trq, 1, number, "0.0");
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new(number, "0", NULL, false, false);
    tst_check_hits(tc, searcher, trq, "0,2,3,4,6,7,8,9,10,12,13,14", -1);
    check_match_vector(tc, searcher, trq, 1, number, "");
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new_more(number, "0", false);
    tst_check_hits(tc, searcher, trq, "0,2,3,4,6,7,8,9,10,12,13,14", -1);
    check_match_vector(tc, searcher, trq, 1, number, "");
    check_match_vector(tc, searcher, trq, 0, number, "0,0");
    frt_q_deref(trq);

    /* below range - no results */
    trq = frt_trq_new(number, "10051006", "10051010", false, false);
    tst_check_hits(tc, searcher, trq, "", -1);
    frt_q_deref(trq);

    /* above range - no results */
    trq = frt_trq_new(number, "-12518421", "-12518420", true, true);
    tst_check_hits(tc, searcher, trq, "", -1);
    frt_q_deref(trq);

    /* should be normal range query for string fields */
    trq = frt_trq_new(cat, "cat2", NULL, true, false);
    tst_check_hits(tc, searcher, trq, "5,6,7,8,9,10,11,12", -1);
    frt_q_deref(trq);

    /* test get_matchv_i */
    /* NOTE: if you are reading this to learn how to use RangeQuery the
     * following is not a good idea. You should usually only use a RangeQuery
     * on an untokenized field. This is just done for testing purposes to
     * check that it works correctly. */
    /* The following tests should use the basic RangeQuery functionality */
    trq = frt_trq_new(field, "word1", "word3", true, true);
    tst_check_hits(tc, searcher, trq, "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17", -1);
    check_match_vector(tc, searcher, trq, 2, rb_intern("not a field"), "");
    check_match_vector(tc, searcher, trq, 2, field, "0,0,1,1");
    frt_q_deref(trq);

    trq = frt_trq_new(field, "word1", "word3", false, true);
    check_match_vector(tc, searcher, trq, 2, field, "1,1");
    frt_q_deref(trq);

    trq = frt_trq_new(field, "word1", "word3", true, false);
    check_match_vector(tc, searcher, trq, 2, field, "0,0");
    frt_q_deref(trq);

    trq = frt_trq_new(field, "word1", "word3", false, false);
    check_match_vector(tc, searcher, trq, 2, field, "");
    frt_q_deref(trq);
}

static void test_typed_range_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_trq_new(date, "20051006", "20051010", true, true);
    q2 = frt_trq_new(date, "20051006", "20051010", true, true);

    Assert(frt_q_eq(q1, q1), "Test same queries are equal");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_trq_new(date, "20051006", "20051010", true, false);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Upper bound include differs");
    Assert(!frt_q_eq(q1, q2), "Upper bound include differs");
    frt_q_deref(q2);

    q2 = frt_trq_new(date, "20051006", "20051010", false, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Lower bound include differs");
    Assert(!frt_q_eq(q1, q2), "Lower bound include differs");
    frt_q_deref(q2);

    q2 = frt_trq_new(date, "20051006", "20051011", true, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Upper bound differs");
    Assert(!frt_q_eq(q1, q2), "Upper bound differs");
    frt_q_deref(q2);

    q2 = frt_trq_new(date, "20051005", "20051010", true, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Lower bound differs");
    Assert(!frt_q_eq(q1, q2), "Lower bound differs");
    frt_q_deref(q2);

    q2 = frt_trq_new(date, "20051006", NULL, true, false);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Upper bound is NULL");
    Assert(!frt_q_eq(q1, q2), "Upper bound is NULL");
    frt_q_deref(q2);

    q2 = frt_trq_new(date, NULL, "20051010", false, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Lower bound is NULL");
    Assert(!frt_q_eq(q1, q2), "Lower bound is NULL");
    frt_q_deref(q2);

    q2 = frt_trq_new(field, "20051006", "20051010", true, true);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Field differs");
    Assert(!frt_q_eq(q1, q2), "Field differs");
    frt_q_deref(q2);
    frt_q_deref(q1);

    q1 = frt_trq_new(date, NULL, "20051010", false, true);
    q2 = frt_trq_new(date, NULL, "20051010", false, true);
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);
    frt_q_deref(q1);

    q1 = frt_trq_new(date, "20051010", NULL, true, false);
    q2 = frt_trq_new(date, "20051010", NULL, true, false);
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);
    frt_q_deref(q1);

    q1 = frt_trq_new(date, "20051010", NULL, true, false);
    q2 = frt_rq_new(date, "20051010", NULL, true, false);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "TypedRangeQuery is not RangeQuery");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);
    frt_q_deref(q1);
}

static void test_wildcard_match(TestCase *tc, void *data)
{
    (void)data;
    (void)tc;
    Assert(!frt_wc_match("", "abc"), "Empty pattern matches nothing");
    Assert(frt_wc_match("*", "asdasdg"), "Star matches everything");
    Assert(frt_wc_match("asd*", "asdasdg"), "Star matches everything after");
    Assert(frt_wc_match("*dg", "asdasdg"), "Star matches everything before");
    Assert(frt_wc_match("a?d*", "asdasdg"), "Q-mark matchs one char");
    Assert(frt_wc_match("?sd*", "asdasdg"), "Q-mark can come first");
    Assert(frt_wc_match("asd?", "asdg"), "Q-mark can come last");
    Assert(frt_wc_match("asdg", "asdg"), "No special chars");
    Assert(!frt_wc_match("asdf", "asdi"), "Do not match");
    Assert(!frt_wc_match("asd??", "asdg"), "Q-mark must match");
    Assert(frt_wc_match("as?g", "asdg"), "Q-mark matches in");
    Assert(!frt_wc_match("as??g", "asdg"), "Q-mark must match");
    Assert(frt_wc_match("a*?f", "asdf"), "Q-mark and star can appear together");
    Assert(frt_wc_match("a?*f", "asdf"), "Q-mark and star can appear together");
    Assert(frt_wc_match("a*?df", "asdf"), "Q-mark and star can appear together");
    Assert(frt_wc_match("a?*df", "asdf"), "Q-mark and star can appear together");
    Assert(!frt_wc_match("as*?df", "asdf"), "Q-mark must match");
    Assert(!frt_wc_match("as?*df", "asdf"), "Q-mark must match");
    Assert(frt_wc_match("asdf*", "asdf"), "Star can match nothing");
    Assert(frt_wc_match("asd*f", "asdf"), "Star can match nothing");
    Assert(frt_wc_match("*asdf*", "asdf"), "Star can match nothing");
    Assert(frt_wc_match("asd?*****", "asdf"), "Can have multiple stars");
    Assert(frt_wc_match("as?*****g", "asdg"), "Can have multiple stars");
    Assert(!frt_wc_match("*asdf", "asdi"), "Do not match");
    Assert(!frt_wc_match("asdf*", "asdi"), "Do not match");
    Assert(!frt_wc_match("*asdf*", "asdi"), "Do not match");
    Assert(!frt_wc_match("cat1*", "cat2/sub1"), "Do not match");
}

static void test_wildcard_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *wq = frt_wcq_new(cat, "cat1*"), *bq;
    tst_check_hits(tc, searcher, wq, "0, 1, 2, 3, 4, 13, 14, 15, 16, 17", -1);
    frt_q_deref(wq);

    wq = frt_wcq_new(cat, "cat1*/s*sub2");
    tst_check_hits(tc, searcher, wq, "4, 16", -1);
    frt_q_deref(wq);

    wq = frt_wcq_new(cat, "cat1/sub?/su??ub2");
    tst_check_hits(tc, searcher, wq, "4, 16", -1);
    frt_q_deref(wq);

    wq = frt_wcq_new(cat, "cat1/");
    tst_check_hits(tc, searcher, wq, "0, 17", -1);
    frt_q_deref(wq);

    wq = frt_wcq_new(rb_intern("unknown_field"), "cat1/");
    tst_check_hits(tc, searcher, wq, "", -1);
    frt_q_deref(wq);

    wq = frt_wcq_new(cat, "unknown_term");
    tst_check_hits(tc, searcher, wq, "", -1);
    frt_q_deref(wq);

    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, frt_tq_new(field, "word1"), FRT_BC_MUST);
    wq = frt_wcq_new(cat, "cat1*");
    tst_check_hits(tc, searcher, wq, "0, 1, 2, 3, 4, 13, 14, 15, 16, 17", -1);

    frt_bq_add_query_nr(bq, wq, FRT_BC_MUST);
    tst_check_hits(tc, searcher, bq, "0, 1, 2, 3, 4, 13, 14, 15, 16, 17", -1);

    frt_q_deref(bq);
}

static void test_wildcard_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_wcq_new(rb_intern("A"), "a*");

    q2 = frt_wcq_new(rb_intern("A"), "a*");
    Assert(frt_q_eq(q1, q1), "Test same queries are equal");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_wcq_new(rb_intern("A"), "a?");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);

    q2 = frt_wcq_new(rb_intern("B"), "a?");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_match_all_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_maq_new();
    q2 = frt_maq_new();

    Assert(frt_q_eq(q1, q1), "Test same queries are equal");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_wcq_new(rb_intern("A"), "a*");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_search_unscored(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    int buf[5], expected[5], count;
    FrtQuery *tq = frt_tq_new(field, "word1");
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 0);
    Aiequal(s2l("0, 1, 2, 3, 4", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 1);
    Aiequal(s2l("1, 2, 3, 4, 5", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 12);
    Aiequal(s2l("12, 13, 14, 15, 16", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 15);
    Aiequal(s2l("15, 16, 17", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 16);
    Aiequal(s2l("16, 17", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 17);
    Aiequal(s2l("17", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 5, 18);
    Aiequal(s2l("", expected), count);
    Aaiequal(expected, buf, count);
    frt_q_deref(tq);

    tq = frt_tq_new(field, "word3");
    count = frt_searcher_search_unscored(searcher, tq, buf, 3, 0);
    Aiequal(s2l("2, 3, 6", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 3, 7);
    Aiequal(s2l("8, 11, 14", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 3, 6);
    Aiequal(s2l("6, 8, 11", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 3, 11);
    Aiequal(s2l("11, 14", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 3, 14);
    Aiequal(s2l("14", expected), count);
    Aaiequal(expected, buf, count);
    count = frt_searcher_search_unscored(searcher, tq, buf, 3, 15);
    Aiequal(s2l("", expected), count);
    Aaiequal(expected, buf, count);
    frt_q_deref(tq);
}

TestSuite *ts_search(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store();
    FrtIndexReader *ir;
    FrtSearcher *searcher;

    date    = rb_intern("date");
    field   = rb_intern("field");
    cat     = rb_intern("cat");
    number  = rb_intern("number");

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_explanation, NULL);
    tst_run_test(suite, test_byte_float_conversion, NULL);
    tst_run_test(suite, test_default_similarity, NULL);

    prepare_search_index(store);
    ir = frt_ir_open(store);
    searcher = frt_isea_new(ir);

    tst_run_test(suite, test_get_doc, (void *)searcher);

    tst_run_test(suite, test_term_query, (void *)searcher);
    tst_run_test(suite, test_term_query_hash, NULL);

    tst_run_test(suite, test_boolean_query, (void *)searcher);
    tst_run_test(suite, test_boolean_query_hash, NULL);

    tst_run_test(suite, test_phrase_query, (void *)searcher);
    tst_run_test(suite, test_phrase_query_hash, NULL);

    tst_run_test(suite, test_multi_phrase_query, (void *)searcher);
    tst_run_test(suite, test_multi_phrase_query_hash, NULL);

    tst_run_test(suite, test_multi_term_query, (void *)searcher);
    tst_run_test(suite, test_multi_term_query_hash, NULL);

    tst_run_test(suite, test_prefix_query, (void *)searcher);
    tst_run_test(suite, test_prefix_query_hash, NULL);

    tst_run_test(suite, test_range_query, (void *)searcher);
    tst_run_test(suite, test_range_query_hash, NULL);

    tst_run_test(suite, test_typed_range_query, (void *)searcher);
    tst_run_test(suite, test_typed_range_query_hash, NULL);

    tst_run_test(suite, test_wildcard_match, (void *)searcher);
    tst_run_test(suite, test_wildcard_query, (void *)searcher);
    tst_run_test(suite, test_wildcard_query_hash, NULL);

    tst_run_test(suite, test_match_all_query_hash, NULL);

    tst_run_test(suite, test_search_unscored, (void *)searcher);

    frt_store_deref(store);
    frt_searcher_close(searcher);
    return suite;
}



static void prepare_multi_search_index(FrtStore *store, struct Data data[],
                                       int d_cnt, int w)
{
    int i;
    FrtIndexWriter *iw;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES,
                              FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    FrtFieldInfo *fi = frt_fi_new(rb_intern("empty-field"), FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
    frt_fis_add_field(fis, fi);
    frt_index_create(store, fis);
    frt_fis_deref(fis);
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    iw = frt_iw_open(store, dbl_analyzer_new(), NULL);
    for (i = 0; i < d_cnt; i++) {
        FrtDocument *doc = frt_doc_new();
        doc->boost = (float)(i+w);
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(date), (char *)data[i].date, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(field), (char *)data[i].field, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(cat), (char *)data[i].cat, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(number), (char *)data[i].number, enc));
        frt_iw_add_doc(iw, doc);
       frt_doc_destroy(doc);
    }
    frt_iw_close(iw);
}

static void test_query_combine(TestCase *tc, void *data)
{
    FrtQuery *q, *cq, **queries;
    FrtBooleanQuery *bq;
    (void)data;

    queries = FRT_ALLOC_N(FrtQuery *, 3);
    queries[0] = frt_tq_new(rb_intern("A"), "a");
    queries[1] = frt_tq_new(rb_intern("A"), "a");
    queries[2] = frt_tq_new(rb_intern("A"), "a");

    cq = frt_q_combine(queries, 3);
    Assert(frt_q_eq(cq, queries[1]), "One unique query submitted");
    frt_q_deref(cq);

    Aiequal(1, queries[1]->ref_cnt);
    frt_q_deref(queries[1]);

    q = frt_bq_new(false);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);

    queries[1] = q;

    cq = frt_q_combine(queries, 3);
    bq = (FrtBooleanQuery *)cq;
    Aiequal(2, bq->clause_cnt);
    Assert(frt_q_eq(bq->clauses[0]->query, queries[0]), "Query should be equal");
    Assert(frt_q_eq(bq->clauses[1]->query, queries[1]), "Query should be equal");
    frt_q_deref(cq);
    frt_q_deref(queries[1]); /* queries[1] */

    q = frt_bq_new(true);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);

    queries[1] = q;

    cq = frt_q_combine(queries, 3);
    Assert(frt_q_eq(cq, queries[0]), "Again only one unique query submitted");
    frt_q_deref(cq);
    Aiequal(1, queries[0]->ref_cnt);

    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("B"), "b"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("C"), "c"), FRT_BC_SHOULD);

    cq = frt_q_combine(queries, 3);
    Aiequal(BOOLEAN_QUERY, cq->type);

    bq = (FrtBooleanQuery *)cq;
    Aiequal(3, bq->clause_cnt);
    q = frt_tq_new(rb_intern("A"), "a");
    Assert(frt_q_eq(bq->clauses[0]->query, q), "Query should be equal");
    frt_q_deref(q);
    q = frt_tq_new(rb_intern("B"), "b");
    Assert(frt_q_eq(bq->clauses[1]->query, q), "Query should be equal");
    frt_q_deref(q);
    q = frt_tq_new(rb_intern("C"), "c");
    Assert(frt_q_eq(bq->clauses[2]->query, q), "Query should be equal");
    frt_q_deref(q);

    frt_q_deref(cq);
    Aiequal(1, queries[0]->ref_cnt);

    Aiequal(1, queries[2]->ref_cnt);
    frt_q_deref(queries[2]);

    q = frt_bq_new(true);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("A"), "a"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("B"), "b"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(q, frt_tq_new(rb_intern("C"), "c"), FRT_BC_MUST);
    queries[2] = q;

    cq = frt_q_combine(queries, 3);
    Aiequal(BOOLEAN_QUERY, cq->type);

    bq = (FrtBooleanQuery *)cq;
    Aiequal(4, bq->clause_cnt);
    q = frt_tq_new(rb_intern("A"), "a");
    Assert(frt_q_eq(bq->clauses[0]->query, q), "Query should be equal");
    frt_q_deref(q);
    q = frt_tq_new(rb_intern("B"), "b");
    Assert(frt_q_eq(bq->clauses[1]->query, q), "Query should be equal");
    frt_q_deref(q);
    q = frt_tq_new(rb_intern("C"), "c");
    Assert(frt_q_eq(bq->clauses[2]->query, q), "Query should be equal");
    frt_q_deref(q);
    Assert(frt_q_eq(bq->clauses[3]->query, queries[2]), "Query should be equal");

    frt_q_deref(cq);
    Aiequal(1, queries[0]->ref_cnt);

    frt_q_deref(queries[0]);
    frt_q_deref(queries[1]);
    frt_q_deref(queries[2]);
    free(queries);
}

TestSuite *ts_multi_search(TestSuite *suite)
{
    FrtStore *store0 = frt_open_ram_store();
    FrtStore *store1 = frt_open_ram_store();

    FrtIndexReader *ir0, *ir1;
    FrtSearcher **searchers;
    FrtSearcher *searcher;

    date    = rb_intern("date");
    field   = rb_intern("field");
    cat     = rb_intern("cat");
    number  = rb_intern("number");

    suite = tst_add_suite(suite, "test_multi_search");

    prepare_multi_search_index(store0, test_data, 9, 1);
    prepare_multi_search_index(store1, test_data + 9, FRT_NELEMS(test_data) - 9, 10);

    ir0 = frt_ir_open(store0);
    ir1 = frt_ir_open(store1);
    searchers = FRT_ALLOC_N(FrtSearcher *, 2);
    searchers[0] = frt_isea_new(ir0);
    searchers[1] = frt_isea_new(ir1);
    searcher = frt_msea_new(searchers, 2, true);

    tst_run_test(suite, test_get_doc, (void *)searcher);

    tst_run_test(suite, test_term_query, (void *)searcher);
    tst_run_test(suite, test_boolean_query, (void *)searcher);
    tst_run_test(suite, test_multi_term_query, (void *)searcher);
    tst_run_test(suite, test_phrase_query, (void *)searcher);
    tst_run_test(suite, test_multi_phrase_query, (void *)searcher);
    tst_run_test(suite, test_prefix_query, (void *)searcher);
    tst_run_test(suite, test_range_query, (void *)searcher);
    tst_run_test(suite, test_typed_range_query, (void *)searcher);
    tst_run_test(suite, test_wildcard_query, (void *)searcher);
    tst_run_test(suite, test_search_unscored, (void *)searcher);

    tst_run_test(suite, test_query_combine, NULL);

    frt_store_deref(store0);
    frt_store_deref(store1);
    frt_searcher_close(searcher);
    return suite;
}

