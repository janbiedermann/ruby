#include "frt_index.h"
#include "testhelper.h"
#include <limits.h>
#include "test.h"

static const char *content_f = "content";
static const char *id_f = "id";
const FrtConfig lucene_config = {
    0x100000,       /* chunk size is 1Mb */
    0x1000000,      /* Max memory used for buffer is 16 Mb */
    FRT_INDEX_INTERVAL, /* index interval */
    FRT_SKIP_INTERVAL,  /* skip interval */
    10,             /* default merge factor */
    10,             /* max_buffered_docs */
    INT_MAX,        /* max_merged_docs */
    10000,          /* maximum field length (number of terms) */
    true            /* use compound file by default */
};


static FrtFieldInfos *prep_fis()
{
    return frt_fis_new(FRT_STORE_NO, FRT_INDEX_YES, FRT_TERM_VECTOR_NO);
}

static void create_index(FrtStore *store)
{
    FrtFieldInfos *fis = prep_fis();
    frt_index_create(store, fis);
    frt_fis_deref(fis);
}

static FrtIndexWriter *create_iw_lucene(FrtStore *store)
{
    create_index(store);
    return frt_iw_open(NULL, store, frt_whitespace_analyzer_new(false), &lucene_config);
}

static void add_doc(FrtIndexWriter *iw, int id)
{
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern(content_f)),
                                   frt_estrdup("aaa"), enc))->destroy_data = true;
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(rb_intern(id_f)),
                                   frt_strfmt("%d", id), enc))->destroy_data = true;
    frt_iw_add_doc(iw, doc);
   frt_doc_destroy(doc);
}

static void add_docs(FrtIndexWriter *iw, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        add_doc(iw, i);
    }
}

static void copy_file(FrtStore *store, const char *src, const char *dest)
{
    FrtInStream *is = store->open_input(store, src);
    FrtOutStream *os = store->new_output(store, dest);
    frt_is2os_copy_bytes(is, os, frt_is_length(is));
    frt_is_close(is);
    frt_os_close(os);
}

/*
 * Verify we can read the pre-XXX file format, do searches
 * against it, and add documents to it.
 */
static void test_delete_leftover_files(TestCase *tc, void *data)
{
    FrtStore *store = (FrtStore *)data;
    FrtIndexWriter *iw = create_iw_lucene(store);
    FrtIndexReader *ir;
    char *store_before, *store_after;
    add_docs(iw, 35);
    frt_iw_close(iw);

    /* Delete one doc so we get a .del file: */
    ir = frt_ir_open(NULL, store);
    frt_ir_delete_doc(ir, 7);
    Aiequal(1, ir->max_doc(ir) - ir->num_docs(ir));

    /* Set one norm so we get a .s0 file: */
    frt_ir_set_norm(ir, 21, rb_intern(content_f), 12);
    frt_ir_close(ir);
    store_before = frt_store_to_s(store);

    /* Create a bogus separate norms file for a
     * segment/field that actually has a separate norms file
     * already: */
    copy_file(store, "_2_1.s0", "_2_2.s0");

    /* Create a bogus separate norms file for a
     * segment/field that actually has a separate norms file
     * already, using the "not compound file" extension: */
    copy_file(store, "_2_1.s0", "_2_2.f0");

    /* Create a bogus separate norms file for a
     * segment/field that does not have a separate norms
     * file already: */
    copy_file(store, "_2_1.s0", "_1_1.s0");

    /* Create a bogus separate norms file for a
     * segment/field that does not have a separate norms
     * file already using the "not compound file" extension: */
    copy_file(store, "_2_1.s0", "_1_1.f0");

    /* Create a bogus separate del file for a
     * segment that already has a separate del file:  */
    copy_file(store, "_0_0.del", "_0_1.del");

    /* Create a bogus separate del file for a
     * segment that does not yet have a separate del file: */
    copy_file(store, "_0_0.del", "_1_1.del");

    /* Create a bogus separate del file for a
     * non-existent segment: */
    copy_file(store, "_0_0.del", "_188_1.del");

    /* Create a bogus segment file: */
    copy_file(store, "_0.cfs", "_188.cfs");

    /* Create a bogus frq file when the CFS already exists: */
    copy_file(store, "_0.cfs", "_0.frq");

    /* Create a bogus frq file when the CFS already exists: */
    copy_file(store, "_0.cfs", "_0.frq");
    copy_file(store, "_0.cfs", "_0.prx");
    copy_file(store, "_0.cfs", "_0.fdx");
    copy_file(store, "_0.cfs", "_0.fdt");
    copy_file(store, "_0.cfs", "_0.tfx");
    copy_file(store, "_0.cfs", "_0.tix");
    copy_file(store, "_0.cfs", "_0.tis");

    /* Create some old segments file: */
    copy_file(store, "segments_5", "segments");
    copy_file(store, "segments_5", "segments_2");


    /* Open & close a writer: should delete the above files and nothing more: */
    frt_iw_close(frt_iw_open(NULL, store, frt_whitespace_analyzer_new(false), &lucene_config));

    store_after = frt_store_to_s(store);

    Asequal(store_before, store_after);
    free(store_before);
    free(store_after);
}

/***************************************************************************
 *
 * IndexFileDeleterSuite
 *
 ***************************************************************************/

TestSuite *ts_file_deleter(TestSuite *suite)
{
    FrtStore *store = frt_open_ram_store(NULL);
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_delete_leftover_files, store);

    frt_store_deref(store);
    return suite;
}
