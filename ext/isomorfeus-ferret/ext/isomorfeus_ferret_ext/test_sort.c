#include "frt_global.h"
#include "testhelper.h"
#include "frt_search.h"
#include "test.h"

#define ARRAY_SIZE 20

static FrtSymbol search, string, integer, flt;

typedef struct SortTestData {
    const char *search;
    const char *string;
    const char *integer;
    const char *flt;
} SortTestData;

static void add_sort_test_data(SortTestData *std, FrtIndexWriter *iw)
{
    FrtDocument *doc = frt_doc_new();
    rb_encoding *enc = rb_enc_find("ASCII-8BIT");
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(search), (char *)std->search, enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(string), (char *)std->string, enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(integer), (char *)std->integer, enc));
    frt_doc_add_field(doc, frt_df_add_data(frt_df_new(flt), (char *)std->flt, enc));

    sscanf(std->flt, "%f", &doc->boost);

    frt_iw_add_doc(iw, doc);
    frt_doc_destroy(doc);
}

static SortTestData data[] = {     /* len mod */
    {"findall","a","6","0.01"},    /*  4   0  */
    {"findall","c","5","0.1"},     /*  3   3  */
    {"findall","e","2","0.001"},   /*  5   1  */
    {"findall","g","1","1.0"},     /*  3   3  */
    {"findall","i","3","0.0001"},  /*  6   2  */
    {"findall","", "4","10.0"},    /*  4   0  */
    {"findall","h","5","0.00001"}, /*  7   3  */
    {"findall","f","2","100.0"},   /*  5   1  */
    {"findall","d","3","1000.0"},  /*  6   2  */
    {"findall","b","4","0.000001"} /*  8   0  */
};

static void sort_test_setup(FrtStore *store)
{
    int i;
    FrtIndexWriter *iw;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    frt_index_create(store, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(store, frt_whitespace_analyzer_new(false), NULL);

    for (i = 0; i < FRT_NELEMS(data); i++) {
        add_sort_test_data(&data[i], iw);
    }
    frt_iw_close(iw);
}

static void sort_multi_test_setup(FrtStore *store1, FrtStore *store2)
{
    int i;
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_YES, FRT_INDEX_YES, FRT_TERM_VECTOR_YES);
    FrtIndexWriter *iw;

    frt_index_create(store1, fis);
    frt_index_create(store2, fis);
    frt_fis_deref(fis);

    iw = frt_iw_open(store1, frt_whitespace_analyzer_new(false), NULL);

    for (i = 0; i < FRT_NELEMS(data)/2; i++) {
        add_sort_test_data(&data[i], iw);
    }
    frt_iw_close(iw);

    iw = frt_iw_open(store2, frt_whitespace_analyzer_new(false), NULL);

    for (i = FRT_NELEMS(data)/2; i < FRT_NELEMS(data); i++) {
        add_sort_test_data(&data[i], iw);
    }
    frt_iw_close(iw);
}

#define R_START 3
#define R_END 6
static void do_test_top_docs(TestCase *tc, FrtSearcher *searcher, FrtQuery *query, const char *expected_hits, FrtSort *sort)
{
    static int num_array[ARRAY_SIZE];
    int i;
    int total_hits = s2l(expected_hits, num_array);
    FrtTopDocs *top_docs = frt_searcher_search(searcher, query, 0, total_hits, NULL, sort, NULL);
    Aiequal(total_hits, top_docs->total_hits);
    Aiequal(total_hits, top_docs->size);

    for (i = 0; i < top_docs->size; i++) {
        FrtHit *hit = top_docs->hits[i];
        if (false && sort && searcher->doc_freq != frt_isea_doc_freq) {
            FrtFieldDoc *fd = (FrtFieldDoc *)hit;
            int j;
            printf("%d == %d:%f ", num_array[i], hit->doc, hit->score);
            for (j = 0; j < fd->size; j++) {
                switch (fd->comparables[j].type) {
                    case FRT_SORT_TYPE_SCORE:
                        printf("sc:%f ", fd->comparables[j].val.f); break;
                    case FRT_SORT_TYPE_FLOAT:
                        printf("f:%f ", fd->comparables[j].val.f); break;
                    case FRT_SORT_TYPE_DOC:
                        printf("d:%ld ", fd->comparables[j].val.l); break;
                    case FRT_SORT_TYPE_INTEGER:
                        printf("i:%ld ", fd->comparables[j].val.l); break;
                    case FRT_SORT_TYPE_STRING:
                        printf("s:%s ", fd->comparables[j].val.s); break;
                    default:
                        printf("NA "); break;
                }
            }
            printf("\n");
        }
        Aiequal(num_array[i], hit->doc);
    }
    frt_td_destroy(top_docs);

    if (total_hits >= R_END) {
        top_docs = frt_searcher_search(searcher, query, R_START, R_END - R_START, NULL, sort, NULL);
        for (i = R_START; i < R_END; i++) {
            FrtHit *hit = top_docs->hits[i - R_START];
            Aiequal(num_array[i], hit->doc);
            /*
            printf("%d == %d\n", num_array[i], hit->doc);
            */
        }
        frt_td_destroy(top_docs);
    }
}

#define TEST_SF_TO_S(_str, _sf) \
    do {\
        FrtSortField *_sf_p = _sf;\
        char *_field = frt_sort_field_to_s(_sf_p);\
        Asequal(_str, _field);\
        free(_field);\
        frt_sort_field_destroy(_sf_p);\
    } while (0)


static void test_sort_field_to_s(TestCase *tc, void *data)
{
    (void)data;
    TEST_SF_TO_S("<SCORE>", frt_sort_field_score_new(false));
    TEST_SF_TO_S("<SCORE>!", frt_sort_field_score_new(true));
    TEST_SF_TO_S("<DOC>", frt_sort_field_doc_new(false));
    TEST_SF_TO_S("<DOC>!", frt_sort_field_doc_new(true));
    TEST_SF_TO_S("date:<integer>", frt_sort_field_int_new(rb_intern("date"), false));
    TEST_SF_TO_S("date:<integer>!", frt_sort_field_int_new(rb_intern("date"), true));
    TEST_SF_TO_S("price:<float>", frt_sort_field_float_new(rb_intern("price"), false));
    TEST_SF_TO_S("price:<float>!", frt_sort_field_float_new(rb_intern("price"), true));
    TEST_SF_TO_S("content:<string>", frt_sort_field_string_new(rb_intern("content"), false));
    TEST_SF_TO_S("content:<string>!", frt_sort_field_string_new(rb_intern("content"), true));
    TEST_SF_TO_S("auto_field:<auto>", frt_sort_field_auto_new(rb_intern("auto_field"), false));
    TEST_SF_TO_S("auto_field:<auto>!", frt_sort_field_auto_new(rb_intern("auto_field"), true));
}

#define TEST_SORT_TO_S(_expected_str, _sort) \
    do {\
        char *_str = frt_sort_to_s(_sort);\
        Asequal(_expected_str, _str);\
        free(_str);\
    } while (0)

static void test_sort_to_s(TestCase *tc, void *data)
{
    FrtSort *sort = frt_sort_new();
    (void)data;

    TEST_SORT_TO_S("Sort[]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_score_new(false));
    TEST_SORT_TO_S("Sort[<SCORE>]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_doc_new(true));
    TEST_SORT_TO_S("Sort[<SCORE>, <DOC>!]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_int_new(rb_intern("date"), true));
    TEST_SORT_TO_S("Sort[<SCORE>, <DOC>!, date:<integer>!]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_float_new(rb_intern("price"), false));
    TEST_SORT_TO_S("Sort[<SCORE>, <DOC>!, date:<integer>!, price:<float>]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_string_new(rb_intern("content"), true));
    TEST_SORT_TO_S("Sort[<SCORE>, <DOC>!, date:<integer>!, price:<float>, content:<string>!]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(rb_intern("auto_field"), false));
    TEST_SORT_TO_S("Sort[<SCORE>, <DOC>!, date:<integer>!, price:<float>, content:<string>!, auto_field:<auto>]", sort);
    frt_sort_clear(sort);
    frt_sort_add_sort_field(sort, frt_sort_field_string_new(rb_intern("content"), true));
    TEST_SORT_TO_S("Sort[content:<string>!]", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(rb_intern("auto_field"), false));
    TEST_SORT_TO_S("Sort[content:<string>!, auto_field:<auto>]", sort);
    frt_sort_destroy(sort);
}

static bool do_byte_test = true;
static void test_sorts(TestCase *tc, void *data)
{
    FrtSearcher *sea = (FrtSearcher *)data;
    FrtQuery *q;
    FrtSort *sort = NULL;

    q = frt_tq_new(search, "findall");
    do_test_top_docs(tc, sea, q, "8,7,5,3,1,0,2,4,6,9", NULL);

    sort = frt_sort_new();

    frt_sort_add_sort_field(sort, frt_sort_field_score_new(false));
    do_test_top_docs(tc, sea, q, "8,7,5,3,1,0,2,4,6,9", sort);
    sort->sort_fields[0]->reverse = true;
    do_test_top_docs(tc, sea, q, "9,6,4,2,0,1,3,5,7,8", sort);
    frt_sort_clear(sort);

    frt_sort_add_sort_field(sort, frt_sort_field_doc_new(false));
    do_test_top_docs(tc, sea, q, "0,1,2,3,4,5,6,7,8,9", sort);
    sort->sort_fields[0]->reverse = true;
    do_test_top_docs(tc, sea, q, "9,8,7,6,5,4,3,2,1,0", sort);
    frt_sort_clear(sort);

    frt_sort_add_sort_field(sort, frt_sort_field_int_new(integer, true));
    do_test_top_docs(tc, sea, q, "0,1,6,5,9,4,8,2,7,3", sort);
    frt_sort_add_sort_field(sort, frt_sort_field_score_new(false));
    do_test_top_docs(tc, sea, q, "0,1,6,5,9,8,4,7,2,3", sort);
    sort->size = 1; /* remove score sort_field */
    sort->sort_fields[0]->reverse = false;
    do_test_top_docs(tc, sea, q, "3,2,7,4,8,5,9,1,6,0", sort);
    sort->size = 2; /* re-add score sort_field */
    do_test_top_docs(tc, sea, q, "3,7,2,8,4,5,9,1,6,0", sort);
    frt_sort_clear(sort);

    if (do_byte_test) {
        frt_sort_add_sort_field(sort, frt_sort_field_byte_new(integer, true));
        do_test_top_docs(tc, sea, q, "0,1,6,5,9,4,8,2,7,3", sort);
        frt_sort_add_sort_field(sort, frt_sort_field_score_new(false));
        do_test_top_docs(tc, sea, q, "0,1,6,5,9,8,4,7,2,3", sort);
        sort->size = 1; /* remove score sort_field */
        sort->sort_fields[0]->reverse = false;
        do_test_top_docs(tc, sea, q, "3,2,7,4,8,5,9,1,6,0", sort);
        sort->size = 2; /* re-add score sort_field */
        do_test_top_docs(tc, sea, q, "3,7,2,8,4,5,9,1,6,0", sort);
        frt_sort_clear(sort);
    }

    frt_sort_add_sort_field(sort, frt_sort_field_float_new(flt, false));
    do_test_top_docs(tc, sea, q, "9,6,4,2,0,1,3,5,7,8", sort);
    sort->sort_fields[0]->reverse = true;
    do_test_top_docs(tc, sea, q, "8,7,5,3,1,0,2,4,6,9", sort);
    frt_sort_clear(sort);

    frt_sort_add_sort_field(sort, frt_sort_field_string_new(string, false));
    do_test_top_docs(tc, sea, q, "0,9,1,8,2,7,3,6,4,5", sort);
    sort->sort_fields[0]->reverse = true;
    do_test_top_docs(tc, sea, q, "5,4,6,3,7,2,8,1,9,0", sort);
    frt_sort_clear(sort);

    if (do_byte_test) {
        frt_sort_add_sort_field(sort, frt_sort_field_byte_new(string, false));
        do_test_top_docs(tc, sea, q, "5,0,9,1,8,2,7,3,6,4", sort);
        sort->sort_fields[0]->reverse = true;
        do_test_top_docs(tc, sea, q, "4,6,3,7,2,8,1,9,0,5", sort);
        frt_sort_clear(sort);
    }

    /* Test Auto FrtSort */
    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(string, false));
    do_test_top_docs(tc, sea, q, "0,9,1,8,2,7,3,6,4,5", sort);
    frt_sort_clear(sort);

    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(integer, false));
    do_test_top_docs(tc, sea, q, "3,2,7,4,8,5,9,1,6,0", sort);
    frt_sort_clear(sort);

    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(flt, false));
    do_test_top_docs(tc, sea, q, "9,6,4,2,0,1,3,5,7,8", sort);
    sort->sort_fields[0]->reverse = true;
    do_test_top_docs(tc, sea, q, "8,7,5,3,1,0,2,4,6,9", sort);
    frt_sort_clear(sort);

    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(integer, false));
    frt_sort_add_sort_field(sort, frt_sort_field_auto_new(string, false));
    do_test_top_docs(tc, sea, q, "3,2,7,8,4,9,5,1,6,0", sort);

    frt_sort_destroy(sort);
    frt_q_deref(q);
}

TestSuite *ts_sort(TestSuite *suite)
{
    FrtSearcher *sea, **searchers;
    FrtStore *store = frt_open_ram_store(), *fs_store;

    search = rb_intern("search");
    string = rb_intern("string");
    integer = rb_intern("integer");
    flt = rb_intern("flt");

    sort_test_setup(store);

    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_sort_field_to_s, NULL);
    tst_run_test(suite, test_sort_to_s, NULL);

    sea = frt_isea_new(frt_ir_open(store));

    tst_run_test(suite, test_sorts, (void *)sea);

    frt_searcher_close(sea);

    do_byte_test = false;

#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
    fs_store = frt_open_fs_store(".\\test\\testdir\\store");
#else
    fs_store = frt_open_fs_store("./test/testdir/store");
#endif
    sort_multi_test_setup(store, fs_store);

    searchers = FRT_ALLOC_N(FrtSearcher *, 2);

    searchers[0] = frt_isea_new(frt_ir_open(store));
    searchers[1] = frt_isea_new(frt_ir_open(fs_store));

    sea = frt_msea_new(searchers, 2, true);
    tst_run_test(suite, test_sorts, (void *)sea);
    frt_searcher_close(sea);

    frt_store_deref(store);
    frt_store_deref(fs_store);

    return suite;
}
