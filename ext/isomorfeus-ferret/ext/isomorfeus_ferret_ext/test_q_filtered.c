#include "frt_search.h"
#include "test.h"

static FrtSymbol num, flipflop;

extern void prepare_filter_index(FrtStore *store);

static void test_filtered_query(TestCase *tc, void *data)
{
    FrtSearcher *searcher = (FrtSearcher *)data;
    FrtQuery *q;
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, "2", "6", true, true));
    tst_check_hits(tc, searcher, q, "2,3,4,5,6", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_tq_new(flipflop, "on"), frt_rfilt_new(num, "2", "6", true, true));
    tst_check_hits(tc, searcher, q, "2,4,6", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, "2", "6", true, false));
    tst_check_hits(tc, searcher, q, "2,3,4,5", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, "2", "6", false, true));
    tst_check_hits(tc, searcher, q, "3,4,5,6", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, "2", "6", false, false));
    tst_check_hits(tc, searcher, q, "3,4,5", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, "6", NULL, true, false));
    tst_check_hits(tc, searcher, q, "6,7,8,9", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, "6", NULL, false, false));
    tst_check_hits(tc, searcher, q, "7,8,9", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, NULL, "2", false, true));
    tst_check_hits(tc, searcher, q, "0,1,2", -1);
    frt_q_deref(q);
    q = frt_fq_new(frt_maq_new(), frt_rfilt_new(num, NULL, "2", false, false));
    tst_check_hits(tc, searcher, q, "0,1", -1);
    frt_q_deref(q);
}

TestSuite *ts_q_filtered(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store();
    FrtIndexReader *ir;
    FrtSearcher *searcher;

    num      = rb_intern("num");
    flipflop = rb_intern("flipflop");

    suite = ADD_SUITE(suite);

    prepare_filter_index(store);
    ir = frt_ir_open(store);
    searcher = frt_isea_new(ir);

    tst_run_test(suite, test_filtered_query, (void *)searcher);

    frt_store_deref(store);
    searcher->close(searcher);
    return suite;
}
