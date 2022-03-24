#include "testhelper.h"
#include "frt_multimapper.h"
#include "test.h"

static void test_multimapper(TestCase *tc, void *data)
{
    char text[] = "abc cabc abd cabcd";
    char dest[1000];
    FrtMultiMapper *mapper = frt_mulmap_new();
    (void)data;

    frt_mulmap_add_mapping(mapper, "abc", "hello");

    frt_mulmap_compile(mapper);
    Aiequal(24, frt_mulmap_map_len(mapper, dest, text, 1000));
    Asequal("hello chello abd chellod", frt_mulmap_map(mapper, dest, text, 1000));
    Asequal("hello chello abd chel", frt_mulmap_map(mapper, dest, text, 22));
    Aiequal(21, frt_mulmap_map_len(mapper, dest, text, 22));
    Asequal("hello chello a", frt_mulmap_map(mapper, dest, text, 15));

    frt_mulmap_add_mapping(mapper, "abcd", "hello");
    frt_mulmap_compile(mapper);
    Asequal("hello chello abd chellod", frt_mulmap_map(mapper, dest, text, 1000));

    frt_mulmap_add_mapping(mapper, "cab", "taxi");
    frt_mulmap_compile(mapper);
    Asequal("hello taxic abd taxicd", frt_mulmap_map(mapper, dest, text, 1000));

    frt_mulmap_destroy(mapper);
}

static void test_multimapper_utf8(TestCase *tc, void *data)
{
    char text[] = "zàáâãäåāăz";
    char dest[1000];
    char *dest_dynamic;
    FrtMultiMapper *mapper = frt_mulmap_new();
    (void)data;

    frt_mulmap_add_mapping(mapper, "à", "a");
    frt_mulmap_add_mapping(mapper, "á", "a");
    frt_mulmap_add_mapping(mapper, "â", "a");
    frt_mulmap_add_mapping(mapper, "ã", "a");
    frt_mulmap_add_mapping(mapper, "ä", "a");
    frt_mulmap_add_mapping(mapper, "å", "a");
    frt_mulmap_add_mapping(mapper, "ā", "a");
    frt_mulmap_add_mapping(mapper, "ă", "a");
    frt_mulmap_compile(mapper);
    Asequal("zaaaaaaaaz", frt_mulmap_map(mapper, dest, text, 1000));
    dest_dynamic = frt_mulmap_dynamic_map(mapper, text);
    Asequal("zaaaaaaaaz", dest_dynamic);
    free(dest_dynamic);
    frt_mulmap_destroy(mapper);
}

TestSuite *ts_multimapper(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_multimapper, NULL);
    tst_run_test(suite, test_multimapper_utf8, NULL);

    return suite;
}
