#include "testhelper.h"
#include "frt_search.h"
#include "test.h"

#define FILTER_DOCS_SIZE 10
#define ARRAY_SIZE 20

struct FilterData {
    const char *num;
    const char *date;
    const char *flipflop;
};

static FrtSymbol num, date, flipflop;

void prepare_filter_index(FrtStore *store)
{
    int i;
    FrtIndexWriter *iw;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_NO);
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    num      = rb_intern("num");
    date     = rb_intern("date");
    flipflop = rb_intern("flipflop");

    struct FilterData data[FILTER_DOCS_SIZE] = {
        {"0", "20040601", "on"},
        {"1", "20041001", "off"},
        {"2", "20051101", "on"},
        {"3", "20041201", "off"},
        {"4", "20051101", "on"},
        {"5", "20041201", "off"},
        {"6", "20050101", "on"},
        {"7", "20040701", "off"},
        {"8", "20050301", "on"},
        {"9", "20050401", "off"}
    };

    frt_index_create(store, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(NULL, store, frt_whitespace_analyzer_new(false), NULL);
    for (i = 0; i < FILTER_DOCS_SIZE; i++) {
        FrtDocument *doc = frt_doc_new();
        doc->boost = (float)(i+1);
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(num), (char *)data[i].num, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(date), (char *)data[i].date, enc));
        frt_doc_add_field(doc, frt_df_add_data(frt_df_new(flipflop), (char *)data[i].flipflop, enc));
        frt_iw_add_doc(iw, doc);
       frt_doc_destroy(doc);
    }
    frt_iw_close(iw);
    return;
}

static void check_filtered_hits(TestCase *tc, FrtSearcher *searcher, FrtQuery *query, FrtFilter *f, FrtPostFilter *post_filter, const char *expected_hits, int top)
{
    static int num_array[ARRAY_SIZE];
    int i;
    int total_hits = s2l(expected_hits, num_array);
    FrtTopDocs *top_docs = frt_searcher_search(searcher, query, 0, total_hits + 1,
                                        f, NULL, post_filter);
    Aiequal(total_hits, top_docs->total_hits);
    Aiequal(total_hits, top_docs->size);

    if ((top >= 0) && top_docs->size) {
        Aiequal(top, top_docs->hits[0]->doc);
    }

    for (i = 0; i < top_docs->size; i++) {
        FrtHit *hit = top_docs->hits[i];
        char buf[1000];
        sprintf(buf, "doc %d was found unexpectedly", hit->doc);
        Assert(frt_ary_includes(num_array, total_hits, hit->doc), buf);
        /* only check the explanation if we got the correct docs. Obviously we
         * might want to remove this to visually check the explanations */
        if (total_hits == top_docs->total_hits) {
            FrtExplanation *e = searcher->explain(searcher, query, hit->doc);
            float escore = e->value;
            if (post_filter) {
                escore *= post_filter->filter_func(hit->doc, escore, searcher,
                                                   post_filter->arg);
            }
            Afequal(hit->score, escore);
            frt_expl_destroy(e);
        }
    }
    frt_td_destroy(top_docs);
}

#define TEST_TO_S(mstr, mfilt) \
    do {\
        char *fstr = mfilt->to_s(mfilt);\
        Asequal(mstr, fstr);\
        free(fstr);\
    } while (0)

static void test_range_filter(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *q = frt_maq_new();
    FrtFilter *rf = frt_rfilt_new(num, "2", "6", true, true);
    check_filtered_hits(tc, searcher, q, rf, NULL, "2,3,4,5,6", -1);
    TEST_TO_S("RangeFilter< num:[2 6] >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, "2", "6", true, false);
    check_filtered_hits(tc, searcher, q, rf, NULL, "2,3,4,5", -1);
    TEST_TO_S("RangeFilter< num:[2 6} >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, "2", "6", false, true);
    check_filtered_hits(tc, searcher, q, rf, NULL, "3,4,5,6", -1);
    TEST_TO_S("RangeFilter< num:{2 6] >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, "2", "6", false, false);
    check_filtered_hits(tc, searcher, q, rf, NULL, "3,4,5", -1);
    TEST_TO_S("RangeFilter< num:{2 6} >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, "6", NULL, true, false);
    check_filtered_hits(tc, searcher, q, rf, NULL, "6,7,8,9", -1);
    TEST_TO_S("RangeFilter< num:[6> >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, "6", NULL, false, false);
    check_filtered_hits(tc, searcher, q, rf, NULL, "7,8,9", -1);
    TEST_TO_S("RangeFilter< num:{6> >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, NULL, "2", false, true);
    check_filtered_hits(tc, searcher, q, rf, NULL, "0,1,2", -1);
    TEST_TO_S("RangeFilter< num:<2] >", rf);
    frt_filt_deref(rf);
    rf = frt_rfilt_new(num, NULL, "2", false, false);
    check_filtered_hits(tc, searcher, q, rf, NULL, "0,1", -1);
    TEST_TO_S("RangeFilter< num:<2} >", rf);
    frt_filt_deref(rf);
    frt_q_deref(q);
}

static void test_range_filter_hash(TestCase *tc, void *data)
{
    FrtFilter *f1, *f2;
    (void)data;
    f1 = frt_rfilt_new(date, "20051006", "20051010", true, true);
    f2 = frt_rfilt_new(date, "20051006", "20051010", true, true);

    Assert(frt_filt_eq(f1, f1), "Test same queries are equal");
    Aiequal(frt_filt_hash(f1), frt_filt_hash(f2));
    Assert(frt_filt_eq(f1, f2), "Queries are equal");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(date, "20051006", "20051010", true, false);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Upper bound include differs");
    Assert(!frt_filt_eq(f1, f2), "Upper bound include differs");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(date, "20051006", "20051010", false, true);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Lower bound include differs");
    Assert(!frt_filt_eq(f1, f2), "Lower bound include differs");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(date, "20051006", "20051011", true, true);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Upper bound differs");
    Assert(!frt_filt_eq(f1, f2), "Upper bound differs");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(date, "20051005", "20051010", true, true);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Lower bound differs");
    Assert(!frt_filt_eq(f1, f2), "Lower bound differs");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(date, "20051006", NULL, true, false);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Upper bound is NULL");
    Assert(!frt_filt_eq(f1, f2), "Upper bound is NULL");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(date, NULL, "20051010", false, true);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Lower bound is NULL");
    Assert(!frt_filt_eq(f1, f2), "Lower bound is NULL");
    frt_filt_deref(f2);

    f2 = frt_rfilt_new(flipflop, "20051006", "20051010", true, true);
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "Field differs");
    Assert(!frt_filt_eq(f1, f2), "Field differs");
    frt_filt_deref(f2);
    frt_filt_deref(f1);

    f1 = frt_rfilt_new(date, NULL, "20051010", false, true);
    f2 = frt_rfilt_new(date, NULL, "20051010", false, true);
    Aiequal(frt_filt_hash(f1), frt_filt_hash(f2));
    Assert(frt_filt_eq(f1, f2), "Queries are equal");
    frt_filt_deref(f2);
    frt_filt_deref(f1);
}

static void test_query_filter(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *bq;
    FrtFilter *qf;
    FrtQuery *q = frt_maq_new();

    qf = frt_qfilt_new_nr(frt_tq_new(flipflop, "on"));
    TEST_TO_S("QueryFilter< flipflop:on >", qf);
    check_filtered_hits(tc, searcher, q, qf, NULL, "0,2,4,6,8", -1);
    frt_filt_deref(qf);

    bq = frt_bq_new(false);
    frt_bq_add_query_nr(bq, frt_tq_new(date, "20051101"), FRT_BC_SHOULD);
    frt_bq_add_query_nr(bq, frt_tq_new(date, "20041201"), FRT_BC_SHOULD);
    qf = frt_qfilt_new_nr(bq);
    check_filtered_hits(tc, searcher, q, qf, NULL, "2,3,4,5", -1);
    TEST_TO_S("QueryFilter< date:20051101 date:20041201 >", qf);
    frt_filt_deref(qf);

    frt_q_deref(q);
}

static void test_query_filter_hash(TestCase *tc, void *data)
{
    FrtFilter *f1, *f2;
    (void)data;
    f1 = frt_qfilt_new_nr(frt_tq_new(rb_intern("A"), "a"));
    f2 = frt_qfilt_new_nr(frt_tq_new(rb_intern("A"), "a"));

    Aiequal(frt_filt_hash(f1), frt_filt_hash(f2));
    Assert(frt_filt_eq(f1, f2), "Queries are equal");
    Assert(frt_filt_eq(f1, f1), "Queries are equal");
    frt_filt_deref(f2);

    f2 = frt_qfilt_new_nr(frt_tq_new(rb_intern("A"), "b"));
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "texts differ");
    Assert(!frt_filt_eq(f1, f2), "texts differ");
    frt_filt_deref(f2);

    f2 = frt_qfilt_new_nr(frt_tq_new(rb_intern("B"), "a"));
    Assert(frt_filt_hash(f1) != frt_filt_hash(f2), "fields differ");
    Assert(!frt_filt_eq(f1, f2), "fields differ");
    frt_filt_deref(f2);

    frt_filt_deref(f1);
}

static float odd_number_filter(int doc_num, float score, FrtSearcher *sea, void *arg)
{
    float is_ok = 0.0;
    FrtLazyDoc *lazy_doc = frt_searcher_get_lazy_doc(sea, doc_num);
    FrtLazyDocField *lazy_df = frt_lazy_doc_get(lazy_doc, rb_intern("num"));
    char *num = frt_lazy_df_get_data(lazy_df, 0);
    (void)score;
    (void)arg;

    if ((atoi(num) % 2) == 0) {
        is_ok = 1.0;
    }

    frt_lazy_doc_close(lazy_doc);
    return is_ok;
}

static float distance_filter(int doc_num, float score, FrtSearcher *sea, void *arg)
{
    int start_point = *((int *)arg);
    float distance = 0.0;
    FrtLazyDoc *lazy_doc = frt_searcher_get_lazy_doc(sea, doc_num);
    FrtLazyDocField *lazy_df = frt_lazy_doc_get(lazy_doc, rb_intern("num"));
    char *num = frt_lazy_df_get_data(lazy_df, 0);
    (void)score;

    distance = 1.0/(1 + (start_point - atoi(num)) * (start_point - atoi(num)));

    frt_lazy_doc_close(lazy_doc);
    return distance;
}

static void test_filter_func(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *q = frt_maq_new();
    FrtFilter *rf = frt_rfilt_new(num, "2", "6", true, true);
    FrtPostFilter odd_filter;
    odd_filter.filter_func = odd_number_filter;
    odd_filter.arg = NULL;

    check_filtered_hits(tc, searcher, q, NULL,
                        &odd_filter, "0,2,4,6,8", -1);
    check_filtered_hits(tc, searcher, q, rf,
                        &odd_filter, "2,4,6", -1);
    frt_filt_deref(rf);
    frt_q_deref(q);
}

static void test_score_altering_filter_func(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *q = frt_maq_new();
    FrtFilter *rf = frt_rfilt_new(num, "4", "8", true, true);
    int start_point = 7;
    FrtPostFilter dist_filter;
    dist_filter.filter_func = &distance_filter;
    dist_filter.arg = &start_point;

    check_filtered_hits(tc, searcher, q, NULL,
                        &dist_filter, "7,6,8,5,9,4,3,2,1,0", -1);
    check_filtered_hits(tc, searcher, q, rf,
                        &dist_filter, "7,6,8,5,4", -1);
    frt_filt_deref(rf);
    frt_q_deref(q);
}

TestSuite *ts_filter(TestSuite *suite)
{
    FrtStore *store;
    FrtIndexReader *ir;
    FrtSearcher *searcher;

    suite = ADD_SUITE(suite);

    store = frt_open_ram_store(NULL);
    prepare_filter_index(store);
    ir = frt_ir_open(NULL, store);
    searcher = frt_isea_new(ir);

    tst_run_test(suite, test_range_filter, (void *)searcher);
    tst_run_test(suite, test_range_filter_hash, NULL);
    tst_run_test(suite, test_query_filter, (void *)searcher);
    tst_run_test(suite, test_query_filter_hash, NULL);
    tst_run_test(suite, test_filter_func, searcher);
    tst_run_test(suite, test_score_altering_filter_func, searcher);

    frt_store_deref(store);
    searcher->close(searcher);
    return suite;
}
