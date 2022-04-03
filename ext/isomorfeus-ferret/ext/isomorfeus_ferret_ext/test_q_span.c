#include "frt_search.h"
#include "test.h"

#undef close

#define ARRAY_SIZE 20
#define TEST_SE(query, ir, expected) do { \
    FrtSpanEnum *__se = ((FrtSpanQuery *)query)->get_spans(query, ir); \
    char *__tmp = __se->to_s(__se);                                    \
    Asequal(expected, __tmp);                                          \
    __se->destroy(__se);                                               \
    free(__tmp);                                                       \
} while(0)

static FrtSymbol field;

static void add_doc(const char *text, FrtIndexWriter *iw)
{
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(field), (char *)text, enc));
    frt_iw_add_doc(iw, doc);
   frt_doc_destroy(doc);
}


static void span_test_setup(FrtStore *store)
{
    const char **d;
    FrtIndexWriter *iw;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    const char *data[] = {
        "start finish one two three four five six seven",
        "start one finish two three four five six seven",
        "start one two finish three four five six seven flip",
        "start one two three finish four five six seven",
        "start one two three four finish five six seven flip",
        "start one two three four five finish six seven",
        "start one two three four five six finish seven eight",
        "start one two three four five six seven finish eight nine",
        "start one two three four five six finish seven eight",
        "start one two three four five finish six seven",
        "start one two three four finish five six seven",
        "start one two three finish four five six seven",
        "start one two finish three four five six seven flop",
        "start one finish two three four five six seven",
        "start finish one two three four five six seven toot",
        "start start  one two three four five six seven",
        "finish start one two three four five six seven flip flop",
        "finish one start two three four five six seven",
        "finish one two start three four five six seven",
        "finish one two three start four five six seven flip",
        "finish one two three four start five six seven",
        "finish one two three four five start six seven flip flop",
        "finish one two three four five six start seven eight",
        "finish one two three four five six seven start eight nine",
        "finish one two three four five six start seven eight",
        "finish one two three four five start six seven",
        "finish one two three four start five six seven",
        "finish one two three start four five six seven flop",
        "finish one two start three four five six seven",
        "finish one start two three four five six seven flip",
        "finish start one two three four five six seven",
        NULL
    };
    frt_index_create(store, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(NULL, store, frt_whitespace_analyzer_new(false), NULL);

    for (d = data; *d != NULL; d++) {
        add_doc(*d, iw);
    }
    frt_iw_close(iw);
}

static void test_span_term(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *tq;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    tq = frt_spantq_new(rb_intern("notafield"), "nine");
    tst_check_hits(tc, sea, tq, "", -1);
    TEST_SE(tq, ir, "SpanTermEnum(span_terms(notafield:nine))@START");
    frt_q_deref(tq);

    tq = frt_spantq_new(field, "nine");
    tst_check_hits(tc, sea, tq, "7,23", -1);
    TEST_SE(tq, ir, "SpanTermEnum(span_terms(field:nine))@START");
    frt_q_deref(tq);

    tq = frt_spantq_new(field, "eight");
    tst_check_hits(tc, sea, tq, "6,7,8,22,23,24", -1);
    TEST_SE(tq, ir, "SpanTermEnum(span_terms(field:eight))@START");
    frt_q_deref(tq);

    frt_searcher_close(sea);
}

static void test_span_term_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_spantq_new(rb_intern("A"), "a");

    q2 = frt_spantq_new(rb_intern("A"), "a");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_spantq_new(rb_intern("A"), "b");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Terms differ");
    Assert(!frt_q_eq(q1, q2), "Terms differ");
    frt_q_deref(q2);

    q2 = frt_spantq_new(rb_intern("B"), "a");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Fields differ");
    Assert(!frt_q_eq(q1, q2), "Fields differ");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_span_multi_term(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *mtq;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    mtq = frt_spanmtq_new(rb_intern("notafield"));
    tst_check_hits(tc, sea, mtq, "", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(notafield:[]))@START");

    frt_spanmtq_add_term(mtq, "nine");
    tst_check_hits(tc, sea, mtq, "", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(notafield:[nine]))@START");

    frt_spanmtq_add_term(mtq, "finish");
    tst_check_hits(tc, sea, mtq, "", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(notafield:[nine,finish]))@START");
    frt_q_deref(mtq);

    mtq = frt_spanmtq_new_conf(field, 4);
    tst_check_hits(tc, sea, mtq, "", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(field:[]))@START");

    frt_spanmtq_add_term(mtq, "nine");
    tst_check_hits(tc, sea, mtq, "7, 23", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(field:[nine]))@START");

    frt_spanmtq_add_term(mtq, "flop");
    tst_check_hits(tc, sea, mtq, "7, 12, 16, 21, 23, 27", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(field:[nine,flop]))@START");

    frt_spanmtq_add_term(mtq, "toot");
    tst_check_hits(tc, sea, mtq, "7, 12, 14, 16, 21, 23, 27", -1);
    TEST_SE(mtq, ir, "SpanTermEnum(span_terms(field:[nine,flop,toot]))@START");
    frt_q_deref(mtq);

    frt_searcher_close(sea);
}

static void test_span_multi_term_hash(TestCase *tc, void *data)
{
    FrtQuery *q1 = frt_spanmtq_new_conf(field, 100);
    FrtQuery *q2 = frt_spanmtq_new(field);
    (void)data, (void)tc;

    Assert(frt_q_hash(q1) == frt_q_hash(q2), "Queries should be equal");
    Assert(frt_q_eq(q1, q1), "Same queries should be equal");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_spanmtq_add_term(q1, "word1");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");

    frt_spanmtq_add_term(q2, "word1");
    Assert(frt_q_hash(q1) == frt_q_hash(q2), "Queries should be equal");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_spanmtq_add_term(q1, "word2");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries should not be equal");
    Assert(!frt_q_eq(q1, q2), "Queries should not be equal");

    frt_spanmtq_add_term(q2, "word2");
    Assert(frt_q_hash(q1) == frt_q_hash(q2), "Queries should be equal");
    Assert(frt_q_eq(q1, q2), "Queries should be equal");

    frt_q_deref(q1);
    frt_q_deref(q2);
}

static void test_span_prefix(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *prq;
    char *tmp;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    prq = frt_spanprq_new(rb_intern("notafield"), "fl");
    tmp = prq->to_s(prq, rb_intern("notafield"));
    Asequal("fl*", tmp);
    free(tmp);
    tmp = prq->to_s(prq, rb_intern("foo"));
    Asequal("notafield:fl*", tmp);
    free(tmp);
    tst_check_hits(tc, sea, prq, "", -1);
    frt_q_deref(prq);

    prq = frt_spanprq_new(field, "fl");
    tmp = prq->to_s(prq, rb_intern("field"));
    Asequal("fl*", tmp);
    free(tmp);
    tst_check_hits(tc, sea, prq, "2, 4, 12, 16, 19, 21, 27, 29", -1);
    frt_q_deref(prq);

    frt_searcher_close(sea);
}

static void test_span_prefix_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;
    q1 = frt_spanprq_new(rb_intern("A"), "a");

    q2 = frt_spanprq_new(rb_intern("A"), "a");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "SpanPrefixQueries are equal");
    Assert(frt_q_eq(q1, q1), "SpanPrefixQueries are same");
    frt_q_deref(q2);

    q2 = frt_spanprq_new(rb_intern("A"), "b");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "SpanPrefixQueries are not equal");
    Assert(!frt_q_eq(q1, q2), "SpanPrefixQueries are not equal");
    frt_q_deref(q2);

    q2 = frt_spanprq_new(rb_intern("B"), "a");
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "SpanPrefixQueries are not equal");
    Assert(!frt_q_eq(q1, q2), "SpanPrefixQueries are not equal");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_span_first(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *q;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    q = frt_spanfq_new_nr(frt_spantq_new(field, "finish"), 1);
    tst_check_hits(tc, sea, q, "16,17,18,19,20,21,22,23,24,25,26,27,28,29,30", -1);
    TEST_SE(q, ir, "SpanFirstEnum(span_first(span_terms(field:finish), 1))");
    frt_q_deref(q);

    q = frt_spanfq_new_nr(frt_spantq_new(field, "finish"), 5);
    tst_check_hits(tc, sea, q, "0,1,2,3,11,12,13,14,16,17,18,19,20,21,22,23,24,25,"
               "26,27,28,29,30", -1);
    TEST_SE(q, ir, "SpanFirstEnum(span_first(span_terms(field:finish), 5))");
    frt_q_deref(q);

    frt_searcher_close(sea);
}

static void test_span_first_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_spanfq_new_nr(frt_spantq_new(rb_intern("A"), "a"), 5);

    q2 = frt_spanfq_new_nr(frt_spantq_new(rb_intern("A"), "a"), 5);
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_spanfq_new_nr(frt_spantq_new(rb_intern("A"), "a"), 3);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Ends differ");
    Assert(!frt_q_eq(q1, q2), "Ends differ");
    frt_q_deref(q2);

    q2 = frt_spanfq_new_nr(frt_spantq_new(rb_intern("A"), "b"), 5);
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Terms differ");
    Assert(!frt_q_eq(q1, q2), "Terms differ");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_span_or(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *q;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);
    q = frt_spanoq_new();
    tst_check_hits(tc, sea, q, "", -1);
    TEST_SE(q, ir, "SpanOrEnum(span_or[])@START");
    frt_spanoq_add_clause_nr(q, frt_spantq_new(field, "flip"));
    tst_check_hits(tc, sea, q, "2, 4, 16, 19, 21, 29", -1);
    TEST_SE(q, ir, "SpanTermEnum(span_terms(field:flip))@START");
    frt_spanoq_add_clause_nr(q, frt_spantq_new(field, "flop"));
    tst_check_hits(tc, sea, q, "2, 4, 12, 16, 19, 21, 27, 29", -1);
    TEST_SE(q, ir, "SpanOrEnum(span_or[span_terms(field:flip),span_terms(field:flop)])@START");
    frt_q_deref(q);
    frt_searcher_close(sea);
}

static void test_span_or_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_spanoq_new();
    q2 = frt_spanoq_new();
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are same");

    frt_spanoq_add_clause_nr(q1, frt_spantq_new(field, "a"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Clause counts differ");
    Assert(!frt_q_eq(q1, q2), "Clause counts differ");

    frt_spanoq_add_clause_nr(q2, frt_spantq_new(field, "a"));
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are same");

    frt_spanoq_add_clause_nr(q1, frt_spantq_new(field, "b"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Clause counts differ");
    Assert(!frt_q_eq(q1, q2), "Clause counts differ");

    frt_spanoq_add_clause_nr(q2, frt_spantq_new(field, "c"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "2nd clause differs");
    Assert(!frt_q_eq(q1, q2), "2nd clause differs");

    frt_q_deref(q2);
    frt_q_deref(q1);
}

static void test_span_near(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *q;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    q = frt_spannq_new(0, true);
    TEST_SE(q, ir, "SpanNearEnum(span_near[])@START");
    frt_spannq_add_clause_nr(q, frt_spantq_new(field, "start"));
    TEST_SE(q, ir, "SpanTermEnum(span_terms(field:start))@START");
    frt_spannq_add_clause_nr(q, frt_spantq_new(field, "finish"));
    TEST_SE(q, ir, "SpanNearEnum(span_near[span_terms(field:start),span_terms(field:finish)])@START");
    tst_check_hits(tc, sea, q, "0, 14", -1);

    ((FrtSpanNearQuery *)q)->in_order = false;
    tst_check_hits(tc, sea, q, "0,14,16,30", -1);

    ((FrtSpanNearQuery *)q)->in_order = true;
    ((FrtSpanNearQuery *)q)->slop = 1;
    tst_check_hits(tc, sea, q, "0,1,13,14", -1);

    ((FrtSpanNearQuery *)q)->in_order = false;
    tst_check_hits(tc, sea, q, "0,1,13,14,16,17,29,30", -1);

    ((FrtSpanNearQuery *)q)->in_order = true;
    ((FrtSpanNearQuery *)q)->slop = 4;
    tst_check_hits(tc, sea, q, "0,1,2,3,4,10,11,12,13,14", -1);

    ((FrtSpanNearQuery *)q)->in_order = false;
    tst_check_hits(tc, sea, q, "0,1,2,3,4,10,11,12,13,14,16,17,18,19,20,26,27,"
               "28,29,30", -1);

    frt_q_deref(q);

    q = frt_spannq_new(0, true);
    frt_spannq_add_clause_nr(q, frt_spanprq_new(field, "fi"));
    frt_spannq_add_clause_nr(q, frt_spanprq_new(field, "fin"));
    frt_spannq_add_clause_nr(q, frt_spanprq_new(field, "si"));
    tst_check_hits(tc, sea, q, "5, 9, 4, 10", -1);
    frt_q_deref(q);

    frt_searcher_close(sea);
}

static void test_span_near_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_spannq_new(0, false);
    q2 = frt_spannq_new(0, false);
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are same");

    frt_spanoq_add_clause_nr(q1, frt_spantq_new(field, "a"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Clause counts differ");
    Assert(!frt_q_eq(q1, q2), "Clause counts differ");

    frt_spanoq_add_clause_nr(q2, frt_spantq_new(field, "a"));
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are same");

    frt_spanoq_add_clause_nr(q1, frt_spantq_new(field, "b"));
    frt_spanoq_add_clause_nr(q2, frt_spantq_new(field, "b"));
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are same");

    ((FrtSpanNearQuery *)q1)->in_order = true;
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "%d == %d, in_order differs",
           frt_q_hash(q1), frt_q_hash(q2));
    Assert(!frt_q_eq(q1, q2), "in_order differs");

    ((FrtSpanNearQuery *)q2)->in_order = true;
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");

    ((FrtSpanNearQuery *)q2)->slop = 4;
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "slop differs");
    Assert(!frt_q_eq(q1, q2), "slop differs");

    ((FrtSpanNearQuery *)q2)->slop = 0;
    frt_spanoq_add_clause_nr(q1, frt_spantq_new(field, "c"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Clause counts differ");
    Assert(!frt_q_eq(q1, q2), "Clause counts differ");

    frt_spanoq_add_clause_nr(q2, frt_spantq_new(field, "d"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "3rd clause differs");
    Assert(!frt_q_eq(q1, q2), "3rd clause differs");

    frt_q_deref(q2);

    frt_q_deref(q1);
}

static void test_span_not(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexReader *ir;
    FrtSearcher *sea;
    FrtQuery *q, *nearq0, *nearq1;

    ir = frt_ir_open(NULL, store);
    sea = frt_isea_new(ir);

    nearq0 = frt_spannq_new(4, true);
    TEST_SE(nearq0, ir, "SpanNearEnum(span_near[])@START");
    frt_spannq_add_clause_nr(nearq0, frt_spantq_new(field, "start"));
    TEST_SE(nearq0, ir, "SpanTermEnum(span_terms(field:start))@START");
    frt_spannq_add_clause_nr(nearq0, frt_spantq_new(field, "finish"));
    TEST_SE(nearq0, ir, "SpanNearEnum(span_near[span_terms(field:start),span_terms(field:finish)])@START");

    nearq1 = frt_spannq_new(4, true);
    TEST_SE(nearq1, ir, "SpanNearEnum(span_near[])@START");
    frt_spannq_add_clause_nr(nearq1, frt_spantq_new(field, "two"));
    TEST_SE(nearq1, ir, "SpanTermEnum(span_terms(field:two))@START");
    frt_spannq_add_clause_nr(nearq1, frt_spantq_new(field, "five"));
    TEST_SE(nearq1, ir, "SpanNearEnum(span_near[span_terms(field:two),span_terms(field:five)])@START");


    q = frt_spanxq_new(nearq0, nearq1);
    TEST_SE(q, ir, "SpanNotEnum(span_not(inc:<span_near[span_terms(field:start),span_terms(field:finish)]>, exc:<span_near[span_terms(field:two),span_terms(field:five)]>))");
    tst_check_hits(tc, sea, q, "0,1,13,14", -1);
    frt_q_deref(q);
    frt_q_deref(nearq0);

    nearq0 = frt_spannq_new(4, false);
    frt_spannq_add_clause_nr(nearq0, frt_spantq_new(field, "start"));
    frt_spannq_add_clause_nr(nearq0, frt_spantq_new(field, "finish"));

    q = frt_spanxq_new_nr(nearq0, nearq1);
    tst_check_hits(tc, sea, q, "0,1,13,14,16,17,29,30", -1);
    frt_q_deref(q);

    nearq0 = frt_spannq_new(4, true);
    frt_spannq_add_clause_nr(nearq0, frt_spantq_new(field, "start"));
    frt_spannq_add_clause_nr(nearq0, frt_spantq_new(field, "two"));

    nearq1 = frt_spannq_new(8, false);
    frt_spannq_add_clause_nr(nearq1, frt_spantq_new(field, "finish"));
    frt_spannq_add_clause_nr(nearq1, frt_spantq_new(field, "five"));

    q = frt_spanxq_new_nr(nearq0, nearq1);
    tst_check_hits(tc, sea, q, "2,3,4,5,6,7,8,9,10,11,12,15", -1);
    frt_q_deref(q);

    frt_searcher_close(sea);
}

static void test_span_not_hash(TestCase *tc, void *data)
{
    FrtQuery *q1, *q2;
    (void)data;

    q1 = frt_spanxq_new_nr(frt_spantq_new(rb_intern("A"), "a"),
                       frt_spantq_new(rb_intern("A"), "b"));
    q2 = frt_spanxq_new_nr(frt_spantq_new(rb_intern("A"), "a"),
                       frt_spantq_new(rb_intern("A"), "b"));

    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    Assert(frt_q_eq(q1, q1), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_spanxq_new_nr(frt_spantq_new(rb_intern("A"), "a"),
                       frt_spantq_new(rb_intern("A"), "c"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "exclude queries differ");
    Assert(!frt_q_eq(q1, q2), "exclude queries differ");
    frt_q_deref(q2);

    q2 = frt_spanxq_new_nr(frt_spantq_new(rb_intern("A"), "x"),
                       frt_spantq_new(rb_intern("A"), "b"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "include queries differ");
    Assert(!frt_q_eq(q1, q2), "include queries differ");
    frt_q_deref(q2);

    q2 = frt_spanxq_new_nr(frt_spantq_new(rb_intern("B"), "a"),
                       frt_spantq_new(rb_intern("B"), "b"));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "fields differ");
    Assert(!frt_q_eq(q1, q2), "fields differ");
    frt_q_deref(q2);

    frt_q_deref(q1);
}

TestSuite *ts_q_span(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);
    field = rb_intern("field");
    span_test_setup(store);

    suite = ADD_SUITE(suite);
    tst_run_test(suite, test_span_term, (void *)store);
    tst_run_test(suite, test_span_term_hash, NULL);
    tst_run_test(suite, test_span_multi_term, (void *)store);
    tst_run_test(suite, test_span_multi_term_hash, NULL);
    tst_run_test(suite, test_span_prefix, (void *)store);
    tst_run_test(suite, test_span_prefix_hash, NULL);
    tst_run_test(suite, test_span_first, (void *)store);
    tst_run_test(suite, test_span_first_hash, NULL);
    tst_run_test(suite, test_span_or, (void *)store);
    tst_run_test(suite, test_span_or_hash, NULL);
    tst_run_test(suite, test_span_near, (void *)store);
    tst_run_test(suite, test_span_near_hash, NULL);
    tst_run_test(suite, test_span_not, (void *)store);
    tst_run_test(suite, test_span_not_hash, NULL);
    frt_store_deref(store);
    return suite;
}
