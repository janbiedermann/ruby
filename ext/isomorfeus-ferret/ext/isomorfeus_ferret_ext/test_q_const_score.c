#include "frt_search.h"
#include "test.h"

static FrtSymbol num;

extern void prepare_filter_index(FrtStore *store);

static void test_const_score_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *q;
    q = frt_csq_new_nr(frt_rfilt_new(num, "2", "6", true, true));
    tst_check_hits(tc, searcher, q, "2,3,4,5,6", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, "2", "6", true, false));
    tst_check_hits(tc, searcher, q, "2,3,4,5", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, "2", "6", false, true));
    tst_check_hits(tc, searcher, q, "3,4,5,6", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, "2", "6", false, false));
    tst_check_hits(tc, searcher, q, "3,4,5", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, "6", NULL, true, false));
    tst_check_hits(tc, searcher, q, "6,7,8,9", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, "6", NULL, false, false));
    tst_check_hits(tc, searcher, q, "7,8,9", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, NULL, "2", false, true));
    tst_check_hits(tc, searcher, q, "0,1,2", -1);
    frt_q_deref(q);
    q = frt_csq_new_nr(frt_rfilt_new(num, NULL, "2", false, false));
    tst_check_hits(tc, searcher, q, "0,1", -1);
    frt_q_deref(q);
}

static void test_const_score_query_hash(TestCase *tc, void *data)
{
    FrtFilter *f;
    FrtQuery *q1, *q2;
    (void)data;
    f = frt_rfilt_new(num, "2", "6", true, true);
    q1 = frt_csq_new_nr(f);
    q2 = frt_csq_new(f);

    Assert(frt_q_eq(q1, q1), "Test same queries are equal");
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_csq_new_nr(frt_rfilt_new(num, "2", "6", true, true));
    Aiequal(frt_q_hash(q1), frt_q_hash(q2));
    Assert(frt_q_eq(q1, q2), "Queries are equal");
    frt_q_deref(q2);

    q2 = frt_csq_new_nr(frt_rfilt_new(num, "3", "6", true, true));
    Assert(frt_q_hash(q1) != frt_q_hash(q2), "Queries are not equal");
    Assert(!frt_q_eq(q1, q2), "Queries are not equal");
    frt_q_deref(q2);
    frt_q_deref(q1);
}

TestSuite *ts_q_const_score(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);
    FrtIndexReader *ir;
    FrtSearcher *searcher;

    num = rb_intern("num");

    suite = ADD_SUITE(suite);

    prepare_filter_index(store);
    ir = frt_ir_open(NULL, store);
    searcher = frt_isea_new(ir);

    tst_run_test(suite, test_const_score_query, (void *)searcher);
    tst_run_test(suite, test_const_score_query_hash, NULL);

    frt_store_deref(store);
    frt_searcher_close(searcher);
    return suite;
}
