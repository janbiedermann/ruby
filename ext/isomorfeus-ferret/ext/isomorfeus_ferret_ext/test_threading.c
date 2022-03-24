#include "frt_global.h"
#include "frt_search.h"
#include "frt_ind.h"
#include "testhelper.h"
#include "test.h"
#include "pthread.h"

extern char *num_to_str(int num);

#define test_num(n, expected) num = num_to_str(n); Asequal(expected, num); free(num)

static void test_number_to_str(TestCase *tc, void *data)
{
    char *num;
    (void)data;
    test_num(0, "zero");
    test_num(9, "nine");
    test_num(10, "ten");
    test_num(13, "thirteen");
    test_num(19, "nineteen");
    test_num(20, "twenty");
    test_num(21, "twenty one");
    test_num(99, "ninety nine");
    test_num(100, "one hundred");
    test_num(101, "one hundred and one");
    test_num(111, "one hundred and eleven");
    test_num(1111, "one thousand one hundred and eleven");
    test_num(22222, "twenty two thousand two hundred and twenty two");
    test_num(333333, "three hundred and thirty three thousand three hundred and thirty three");
    test_num(8712387, "eight million seven hundred and twelve thousand three hundred and eighty seven");
    test_num(1000000000, "one billion");
    test_num(-8712387, "negative eight million seven hundred and twelve thousand three hundred and eighty seven");

}

void dummy_log(const void *fmt, ...) {(void)fmt;}
#define ITERATIONS 10
#define NTHREADS 10
#ifdef FRT_HAS_VARARGS
#define tlog(...)
#else
#define tlog dummy_log
#endif
/*#define tlog printf */

static void do_optimize(FrtIndex *index)
{
    tlog("Optimizing the index\n");
    frt_index_optimize(index);
}

static void do_delete_doc(FrtIndex *index)
{
    int size;
    if ((size = frt_index_size(index)) > 0) {
        int doc_num = rand() % size;
        tlog("Deleting %d from index which has%s deletions\n",
             doc_num, (frt_index_has_del(index) ? "" : " no"));
        if (frt_index_is_deleted(index, doc_num)) {
            tlog("document was already deleted\n");
        } else {
            frt_index_delete(index, doc_num);
        }
    }
}

ID id;
ID contents;

static void do_add_doc(FrtIndex *index)
{
    FrtDocument *doc = frt_doc_new();
    int n = rand();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(id), frt_strfmt("%d", n), enc))->destroy_data = true;
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(contents), num_to_str(n), enc))->destroy_data = true;
    tlog("Adding %d\n", n);
    frt_index_add_doc(index, doc);
    frt_doc_destroy(doc);
}

static void do_search(FrtIndex *index)
{
    int n = rand(), i;
    char *query = num_to_str(n);
    FrtTopDocs *td;
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");

    tlog("Searching for %d\n", n);

    frt_mutex_lock(&index->mutex);
    td = frt_index_search_str(index, query, 0, 3, NULL, NULL, NULL, enc);
    free(query);
    for (i = 0; i < td->size; i++) {
        FrtHit *hit = td->hits[i];
        FrtDocument *doc = frt_index_get_doc(index, hit->doc);
        tlog("Hit for %d: %s - %f\n", hit->doc, frt_doc_get_field(doc, id)->data[0], hit->score);
        frt_doc_destroy(doc);
    }
    tlog("Searched for %d: total = %d\n", n, td->total_hits);
    frt_mutex_unlock(&index->mutex);

    frt_td_destroy(td);
}

static void *indexing_thread(void *p)
{
    int i, choice;
    FrtIndex *index = (FrtIndex *)p;

    for (i = 0; i < ITERATIONS; i++) {
        choice = rand() % 1000;

        if (choice > 999) {
            do_optimize(index);
        } else if (choice > 900) {
            do_delete_doc(index);
        } else if (choice > 700) {
            do_search(index);
        } else {
            do_add_doc(index);
        }
    }
    return NULL;
}

static void test_threading_test(TestCase *tc, void *data)
{
    FrtIndex *index = (FrtIndex *)data;
    (void)data;
    (void)tc;
    indexing_thread(index);
}

static void test_threading(TestCase *tc, void *data)
{
    int i;
    pthread_t thread_id[NTHREADS];
    FrtIndex *index = (FrtIndex *)data;
    (void)data;
    (void)tc;

    for(i=0; i < NTHREADS; i++) {
        pthread_create(&thread_id[i], NULL, &indexing_thread, index );
    }

    for(i=0; i < NTHREADS; i++) {
        pthread_join(thread_id[i], NULL);
    }
}

TestSuite *ts_threading(TestSuite *suite)
{
    id = rb_intern("id");
    contents = rb_intern("contents");

    FrtAnalyzer *a = frt_letter_analyzer_new(true);
    FrtStore *store = frt_open_fs_store("./test/testdir/store");
    FrtIndex *index;
    FrtHashSet *def_fields = frt_hs_new_ptr(NULL);
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                              FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
    frt_fis_add_field(fis, frt_fi_new(id, FRT_STORE_YES, FRT_INDEX_UNTOKENIZED,
                              FRT_TERM_VECTOR_YES));
    frt_index_create(store, fis);
    frt_fis_deref(fis);

    frt_hs_add(def_fields, (void *)contents);
    store->clear_all(store);
    index = frt_index_new(store, a, def_fields, true);
    frt_hs_destroy(def_fields);

    suite = ADD_SUITE(suite);

    frt_store_deref(store);
    frt_a_deref(a);

    tst_run_test(suite, test_number_to_str, NULL);
    tst_run_test(suite, test_threading_test, index);
    // tst_run_test(suite, test_threading, index);

    frt_index_destroy(index);

    store = frt_open_fs_store("./test/testdir/store");
    store->clear_all(store);
    frt_store_deref(store);

    return suite;
}
