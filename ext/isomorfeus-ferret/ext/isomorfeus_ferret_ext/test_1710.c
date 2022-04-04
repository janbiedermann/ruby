#include "frt_index.h"
#include "test.h"
#include "testhelper.h"
#include <stdio.h>

extern rb_encoding *utf8_encoding;

static FrtFieldInfos *create_fis(void) {
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    return fis;
}

static FrtIndexWriter *create_iw(FrtStore *store)
{
    FrtFieldInfos *fis = create_fis();
    frt_index_create(store, fis);
    frt_fis_deref(fis);
    return frt_iw_open(NULL, store, frt_standard_analyzer_new(true), &frt_default_config);
}

static FrtDocument *prep_doc(void) {
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = utf8_encoding;
    frt_doc_add_field(
        doc,
        frt_df_add_data(
            frt_df_new(rb_intern("content")),
            frt_estrdup("http://_____________________________________________________"),
            enc
            )
        )->destroy_data = true;
    return doc;
}

static void test_problem_text(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw = create_iw(store);
    FrtDocument *problem_text = prep_doc();

    frt_iw_add_doc(iw, problem_text);
    Aiequal(1, frt_iw_doc_count(iw));
    Assert(!store->exists(store, "_0.cfs"), "data shouldn't have been written yet");
    frt_iw_commit(iw);
    Assert(store->exists(store, "_0.cfs"), "data should now be written");
    frt_iw_close(iw);
    Assert(store->exists(store, "_0.cfs"), "data should still be there");
}

TestSuite *ts_1710(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_problem_text, store);

    frt_store_deref(store);

    return suite;
}

