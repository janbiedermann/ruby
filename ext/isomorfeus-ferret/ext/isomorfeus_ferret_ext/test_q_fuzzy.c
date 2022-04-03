#include "frt_search.h"
#include "test.h"

#define ARRAY_SIZE 20

static FrtSymbol field;

static void add_doc(const char *text, FrtIndexWriter *iw)
{
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(field), (char *)text, enc));
    frt_iw_add_doc(iw, doc);
    frt_doc_destroy(doc);
}

void check_to_s(TestCase *tc, FrtQuery *query, FrtSymbol field, const char *q_str);

static void do_prefix_test(TestCase *tc, FrtSearcher *searcher, const char *qstr, const char *expected_hits, int pre_len, float min_sim)
{
    FrtQuery *fq = frt_fuzq_new_conf(field, qstr, min_sim, pre_len, 10);
    tst_check_hits(tc, searcher, fq, expected_hits, -1);
    frt_q_deref(fq);
}

static void test_fuzziness(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtTopDocs *top_docs;
    FrtQuery *q;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    frt_index_create(store, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(NULL, store, frt_whitespace_analyzer_new(false), NULL);

    add_doc("aaaaa", iw);
    add_doc("aaaab", iw);
    add_doc("aaabb", iw);
    add_doc("aabbb", iw);
    add_doc("abbbb", iw);
    add_doc("bbbbb", iw);
    add_doc("ddddd", iw);
    add_doc("ddddddddddddddddddddd", iw);   /* test max_distances problem */
    add_doc("aaaaaaaaaaaaaaaaaaaaaaa", iw); /* test max_distances problem */
    frt_iw_close(iw);

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    q = frt_fuzq_new_conf(field, "aaaaa", 0.0, 5, 10);
    tst_check_hits(tc, sea, q, "0", -1);
    frt_q_deref(q);

    q = frt_fuzq_new(rb_intern("not a field"), "aaaaa");
    tst_check_hits(tc, sea, q, "", -1);
    frt_q_deref(q);

    /* test prefix length */
    do_prefix_test(tc, sea, "aaaaaaaaaaaaaaaaaaaaaa", "8", 1, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0,1,2", 0, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0,1,2", 1, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0,1,2", 2, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0,1,2", 3, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0,1", 4, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0", 5, 0.0);
    do_prefix_test(tc, sea, "aaaaa", "0", 6, 0.0);
    /* test where term will equal prefix but not whole query string */
    do_prefix_test(tc, sea, "aaaaaaa", "0", 5, 0.0);

    /* test minimum similarity */
    do_prefix_test(tc, sea, "aaaaa", "0,1,2,3", 0, 0.2);
    do_prefix_test(tc, sea, "aaaaa", "0,1,2", 1, 0.4);
    do_prefix_test(tc, sea, "aaaaa", "0,1", 1, 0.6);
    do_prefix_test(tc, sea, "aaaaa", "0", 1, 0.8);

    /* test where no terms will have any similarity */
    do_prefix_test(tc, sea, "xxxxx", "", 0, 0.0);

    /* test where no terms will have enough similarity to match */
    do_prefix_test(tc, sea, "aaccc", "", 0, 0.0);

    /* test prefix length but with non-matching term (aaaac does not exit in
     * the index) */
    do_prefix_test(tc, sea, "aaaac", "0,1,2", 0, 0.0);
    do_prefix_test(tc, sea, "aaaac", "0,1,2", 1, 0.0);
    do_prefix_test(tc, sea, "aaaac", "0,1,2", 2, 0.0);
    do_prefix_test(tc, sea, "aaaac", "0,1,2", 3, 0.0);
    do_prefix_test(tc, sea, "aaaac", "0,1", 4, 0.0);
    do_prefix_test(tc, sea, "aaaac", "", 5, 0.0);

    /* test really long string never matches */
    do_prefix_test(tc, sea, "ddddX", "6", 0, 0.0);
    do_prefix_test(tc, sea, "ddddX", "6", 1, 0.0);
    do_prefix_test(tc, sea, "ddddX", "6", 2, 0.0);
    do_prefix_test(tc, sea, "ddddX", "6", 3, 0.0);
    do_prefix_test(tc, sea, "ddddX", "6", 4, 0.0);
    do_prefix_test(tc, sea, "ddddX", "", 5, 0.0);

    /* test non-existing field doesn't break search */
    q = frt_fuzq_new_conf(rb_intern("anotherfield"), "ddddX", 0.0, 10, 100);
    top_docs = frt_searcher_search(sea, q, 0, 1, NULL, NULL, NULL);
    frt_q_deref(q);
    Aiequal(0, top_docs->total_hits);
    frt_td_destroy(top_docs);

    frt_searcher_close(sea);
}

static void test_fuzziness_long(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw;
    FrtSearcher *sea;
    FrtIndexReader *ir;
    FrtTopDocs *top_docs;
    FrtQuery *q;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    frt_index_create(store, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(NULL, store, frt_whitespace_analyzer_new(false), NULL);

    add_doc("aaaaaaa", iw);
    add_doc("segment", iw);
    frt_iw_close(iw);
    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    /* not similar enough: */
    do_prefix_test(tc, sea, "xxxxx", "", 0, 0.0);

    /* edit distance to "aaaaaaa" = 3, this matches because the string is longer than
     * in testDefaultFuzziness so a bigger difference is allowed: */
    do_prefix_test(tc, sea, "aaaaccc", "0", 0, 0.0);

    /* now with prefix */
    do_prefix_test(tc, sea, "aaaaccc", "0", 1, 0.0);
    do_prefix_test(tc, sea, "aaaaccc", "0", 4, 0.0);
    do_prefix_test(tc, sea, "aaaaccc", "", 5, 0.0);

    /* no match, more than half of the characters is wrong: */
    do_prefix_test(tc, sea, "aaacccc", "", 0, 0.0);

    /* now with prefix */
    do_prefix_test(tc, sea, "aaacccc", "", 1, 0.0);

    /* "student" and "stellent" are indeed similar to "segment" by default: */
    do_prefix_test(tc, sea, "student", "1", 0, 0.0);

    /* now with prefix */
    do_prefix_test(tc, sea, "student", "", 2, 0.0);
    do_prefix_test(tc, sea, "stellent", "", 2, 0.0);

    /* "student" doesn't match anymore thanks to increased min-similarity: */
    q = frt_fuzq_new_conf(field, "student", (float)0.6, 0, 100);
    top_docs = frt_searcher_search(sea, q, 0, 1, NULL, NULL, NULL);
    frt_q_deref(q);
    Aiequal(0, top_docs->total_hits);
    frt_td_destroy(top_docs);

    frt_searcher_close(sea);
}

/**
 * Test query->to_s functionality
 */
static void test_fuzzy_query_to_s(TestCase *tc, void *data)
{
    FrtQuery *q;
    (void)data;

    q = frt_fuzq_new_conf(rb_intern("A"), "a", 0.4f, 2, 100);
    check_to_s(tc, q, rb_intern("A"), "a~0.4");
    check_to_s(tc, q, rb_intern("B"), "A:a~0.4");
    frt_q_deref(q);

    q = frt_fuzq_new_conf(rb_intern("field"), "mispell", 0.5f, 2, 100);
    check_to_s(tc, q, rb_intern("field"), "mispell~");
    check_to_s(tc, q, rb_intern("notfield"), "field:mispell~");
    frt_q_deref(q);

}

/**
 * Test query hashing functionality
 */
static void test_fuzzy_query_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_fuzq_new_conf(rb_intern("A"), "a", 0.4f, 2, 100);
    q2 = frt_fuzq_new_conf(rb_intern("A"), "a", 0.4f, 2, 100);

    Assert(frt_q_eq(q1, q1), "Test same queries are equal");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_fuzq_new_conf(rb_intern("A"), "a", 0.4f, 0, 100);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "prelen differs");
    Assert(!frt_q_eq(q1, q2), "prelen differs");
    frt_q_deref(q2);

    q2 = frt_fuzq_new_conf(rb_intern("A"), "a", 0.5f, 2, 100);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "similarity differs");
    Assert(!frt_q_eq(q1, q2), "similarity differs");
    frt_q_deref(q2);

    q2 = frt_fuzq_new_conf(rb_intern("A"), "b", 0.4f, 2, 100);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "term differs");
    Assert(!frt_q_eq(q1, q2), "term differs");
    frt_q_deref(q2);

    q2 = frt_fuzq_new_conf(rb_intern("B"), "a", 0.4f, 2, 100);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "field differs");
    Assert(!frt_q_eq(q1, q2), "field differs");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

TestSuite *ts_q_fuzzy(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);

    field = rb_intern("field");

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_fuzziness, (void *)store);
    tst_run_test(suite, test_fuzziness_long, (void *)store);
    tst_run_test(suite, test_fuzzy_query_hash, (void *)store);
    tst_run_test(suite, test_fuzzy_query_to_s, (void *)store);

    frt_store_deref(store);
    return suite;
}
