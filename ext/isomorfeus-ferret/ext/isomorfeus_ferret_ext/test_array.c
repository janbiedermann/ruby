#include "frt_array.h"
#include <string.h>
#include "test.h"

static void frt_ary_free_mock(void *p)
{
    char *str = (char *)p;
    strcpy(str, "free");
}

static void test_ary(TestCase *tc, void *data)
{
    int i;
    int raised = 0;
    char *tmp;
    void **ary = frt_ary_new();
    (void)data;

    Aiequal(0, frt_ary_sz(ary));
    Aiequal(FRT_ARY_INIT_CAPA, frt_ary_capa(ary));
    frt_ary_free(ary);

    ary = frt_ary_new();
    frt_ary_push(ary, (char *)"one");
    Aiequal(1, frt_ary_sz(ary));
    frt_ary_unshift(ary, (char *)"zero");
    Aiequal(2, frt_ary_sz(ary));
    Asequal("zero", ary[0]);
    Asequal("one", ary[1]);
    Apnull(frt_ary_remove(ary, 2));

    FRT_TRY
        frt_ary_set(ary, -3, (char *)"minusone");
    FRT_XCATCHALL
        FRT_HANDLED();
        raised = 1;
    FRT_XENDTRY
    Aiequal(1, raised);
    frt_ary_free(ary);

    ary = frt_ary_new_capa(10);
    Aiequal(0, frt_ary_sz(ary));
    Aiequal(10, frt_ary_capa(ary));
    frt_ary_set(ary, 1, (char *)"one");
    Aiequal(2, frt_ary_sz(ary));
    Asequal("one", ary[1]);
    Apnull(ary[0]);
    Apnull(frt_ary_get(ary, 0));
    Apnull(ary[2]);
    Apnull(frt_ary_get(ary, 2));

    /* cannot use the simple reference outside of the allocated range */
    Asequal("one", frt_ary_get(ary, -1));
    Apnull(frt_ary_get(ary, 22));
    Apnull(frt_ary_get(ary, -22));

    frt_ary_set(ary, 2, (char *)"two");
    Aiequal(3, frt_ary_sz(ary));
    Asequal("one", ary[1]);
    Asequal("two", ary[2]);
    Apnull(ary[0]);
    Apnull(ary[3]);
    Asequal("one", frt_ary_get(ary, -2));
    Asequal("two", frt_ary_get(ary, -1));
    frt_ary_set(ary, -1, (char *)"two");
    frt_ary_set(ary, -3, (char *)"zero");

    Asequal("zero", ary[0]);
    Asequal("one", ary[1]);
    Asequal("two", ary[2]);
    Aiequal(3, frt_ary_sz(ary));

    frt_ary_set(ary, 19, (char *)"nineteen");
    Aiequal(20, frt_ary_sz(ary));
    for (i = 4; i < 19; i++) {
        Apnull(ary[i]);
    }

    frt_ary_push(ary, (char *)"twenty");
    Aiequal(21, frt_ary_sz(ary));
    Asequal("twenty", frt_ary_pop(ary));
    Aiequal(20, frt_ary_sz(ary));

    Asequal("nineteen", frt_ary_pop(ary));
    Aiequal(19, frt_ary_sz(ary));

    Apnull(frt_ary_pop(ary));
    Aiequal(18, frt_ary_sz(ary));

    frt_ary_push(ary, (char *)"eighteen");
    Aiequal(19, frt_ary_sz(ary));
    Asequal("eighteen", ary[18]);
    Asequal("eighteen", frt_ary_get(ary, -1));
    Asequal("zero", frt_ary_get(ary, -19));
    Asequal("one", frt_ary_get(ary, -18));
    Asequal("two", frt_ary_get(ary, -17));
    Apnull(frt_ary_get(ary, -16));
    Apnull(frt_ary_get(ary, -20));

    Asequal("zero", frt_ary_shift(ary));
    Aiequal(18, frt_ary_sz(ary));
    Asequal("eighteen", ary[17]);
    Apnull(ary[18]);
    Asequal("one", frt_ary_get(ary, -18));
    Asequal("two", frt_ary_get(ary, -17));
    Apnull(frt_ary_get(ary, -16));
    Apnull(frt_ary_get(ary, -19));

    Asequal("one", frt_ary_shift(ary));
    Aiequal(17, frt_ary_sz(ary));
    Asequal("eighteen", ary[16]);
    Apnull(ary[18]);
    Apnull(ary[17]);
    Asequal("two", frt_ary_get(ary, -17));
    Apnull(frt_ary_get(ary, -16));
    Apnull(frt_ary_get(ary, -18));

    ary[5] = (char *)"five";
    ary[6] = (char *)"six";
    ary[7] = (char *)"seven";

    Asequal("five", frt_ary_get(ary, 5));
    Asequal("six", frt_ary_get(ary, 6));
    Asequal("seven", frt_ary_get(ary, 7));

    frt_ary_remove(ary, 6);
    Aiequal(16, frt_ary_sz(ary));

    Asequal("five", frt_ary_get(ary, 5));
    Asequal("seven", frt_ary_get(ary, 6));
    Apnull(frt_ary_get(ary, 4));
    Apnull(frt_ary_get(ary, 7));
    Asequal("eighteen", ary[15]);
    Asequal("two", frt_ary_get(ary, -16));
    Apnull(frt_ary_get(ary, -15));
    Apnull(frt_ary_get(ary, -17));
    Asequal("five", frt_ary_get(ary, 5));
    Asequal("seven", frt_ary_get(ary, 6));

    tmp = frt_estrdup("sixsix");
    ary[6] = tmp;
    ary[7] = (char *)"seven";
    Asequal("sixsix", frt_ary_get(ary, 6));
    Asequal("seven", frt_ary_get(ary, 7));

    frt_ary_delete(ary, 6, &frt_ary_free_mock);
    Aiequal(15, frt_ary_sz(ary));
    Asequal("free", tmp);
    free(tmp);

    Asequal("five", frt_ary_get(ary, 5));
    Asequal("seven", frt_ary_get(ary, 6));
    Apnull(frt_ary_get(ary, 4));
    Apnull(frt_ary_get(ary, 7));

    frt_ary_free(ary);
}

void test_ary_destroy(TestCase *tc, void *data)
{
    void **ary = frt_ary_new();
    char str1[10] = "alloc1";
    char str2[10] = "alloc2";
    (void)data;

    frt_ary_set(ary, 0, str1);
    Aiequal(1, frt_ary_sz(ary));
    Asequal("alloc1", ary[0]);
    frt_ary_set(ary, 0, str2);
    Asequal("alloc2", ary[0]);
    frt_ary_push(ary, str1);
    Aiequal(2, frt_ary_sz(ary));
    Asequal("alloc1", ary[1]);
    frt_ary_delete(ary, 0, &frt_ary_free_mock);
    Aiequal(1, frt_ary_sz(ary));
    Asequal("free", str2);
    frt_ary_destroy(ary, &frt_ary_free_mock);
    Asequal("free", str1);
}

#define ARY_STRESS_SIZE 1000
void stress_ary(TestCase *tc, void *data)
{
    int i;
    char buf[100], *t;
    void **ary = frt_ary_new();
    (void)data;

    for (i = 0; i < ARY_STRESS_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        frt_ary_push(ary, frt_estrdup(buf));
    }

    for (i = 0; i < ARY_STRESS_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        t = (char *)frt_ary_shift(ary);
        Asequal(buf, t);
        free(t);
    }

    Aiequal(0, frt_ary_sz(ary));

    for (i = 0; i < ARY_STRESS_SIZE; i++) {
        Apnull(ary[i]);
    }
    frt_ary_destroy(ary, &free);
}

struct TestPoint {
    int x;
    int y;
};

#define tp_ary_set(ary, i, x_val, y_val) do {\
    frt_ary_resize(ary, i);\
    ary[i].x = x_val;\
    ary[i].y = y_val;\
} while (0)

void test_typed_ary(TestCase *tc, void *data)
{
    struct TestPoint *points = frt_ary_new_type_capa(struct TestPoint, 5);
    (void)data;

    Aiequal(5, frt_ary_capa(points));
    Aiequal(0, frt_ary_sz(points));
    Aiequal(sizeof(struct TestPoint), frt_ary_type_size(points));

    tp_ary_set(points, 0, 1, 2);
    Aiequal(5, frt_ary_capa(points));
    Aiequal(1, frt_ary_sz(points));
    Aiequal(sizeof(struct TestPoint), frt_ary_type_size(points));
    Aiequal(1, points[0].x);
    Aiequal(2, points[0].y);

    tp_ary_set(points, 5, 15, 20);
    Aiequal(6, frt_ary_size(points));
    Aiequal(15, points[5].x);
    Aiequal(20, points[5].y);

    tp_ary_set(points, 1, 1, 1);
    tp_ary_set(points, 2, 2, 2);
    tp_ary_set(points, 3, 3, 3);
    tp_ary_set(points, 4, 4, 4);

    Aiequal(6, frt_ary_size(points));
    Aiequal(1, points[0].x);
    Aiequal(2, points[0].y);
    Aiequal(1, points[1].x);
    Aiequal(1, points[1].y);
    Aiequal(2, points[2].x);
    Aiequal(2, points[2].y);
    Aiequal(3, points[3].x);
    Aiequal(3, points[3].y);
    Aiequal(4, points[4].x);
    Aiequal(4, points[4].y);
    Aiequal(15, points[5].x);
    Aiequal(20, points[5].y);
    frt_ary_free(points);
}

TestSuite *ts_array(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_ary, NULL);
    tst_run_test(suite, test_ary_destroy, NULL);
    tst_run_test(suite, stress_ary, NULL);
    tst_run_test(suite, test_typed_ary, NULL);

    return suite;
}
