#include "frt_hash.h"
#include "frt_global.h"
#include <stdlib.h>
#include <unistd.h>
#include "test.h"
#include "testhelper.h"

static int *malloc_int(int val)
{
    int *i = FRT_ALLOC(int);
    *i = val;
    return i;
}

static void mark_free(void *p)
{
    strcpy((char *)p, "freed");
}
/**
 * Basic test for string Hash. Make sure string can be retrieved
 */
static void test_hash_str(TestCase *tc, void *data)
{
    FrtHash *h = frt_h_new_str(NULL, &free);
    FILE *f;
    char buf[100], *t;
    memset(buf, 0, 100);
    (void)data; /* suppress unused argument warning */

    Assert(frt_h_get(h, "one") == NULL, "No entries added yet");

    Aiequal(0, frt_h_set(h, "one", malloc_int(1)));
    Aiequal(1, *(int *)frt_h_get(h, "one"));
    Aiequal(true, frt_h_del(h, "one"));

    Assert(frt_h_get(h, "one") == NULL, "The Hash Entry has been deleted");

    /* test that frt_h_has_key works even when value is set to null */
    frt_h_set(h, "one", NULL);
    Apnull(frt_h_get(h, "one"));
    Atrue(frt_h_has_key(h, "one"));
    frt_h_set(h, "two", malloc_int(2));
    frt_h_set(h, "three", malloc_int(3));
    frt_h_set(h, "four", malloc_int(4));

    size_t res;
    f = temp_open();
    frt_h_str_print_keys(h, f);
    fseek(f, 0, SEEK_SET);
    res = fread(buf, 1, 100, f);
    Atrue(res > 0);
    fclose(f);
    Asequal("keys:\n\tfour\n\tone\n\tthree\n\ttwo\n", buf);
    frt_h_destroy(h);

    /* test frt_h_rem with allocated key */
    strcpy(buf, "key");
    frt_h_new_str(&mark_free, (frt_free_ft)NULL);
    frt_h_set(h, buf, (char *)"val");
    Asequal("val", frt_h_get(h, "key"));
    t = (char *)frt_h_rem(h, "key", false);
    Asequal("val", t);
    Asequal("key", buf);
    frt_h_set(h, buf, (char *)"new val");
    Asequal("new val", frt_h_get(h, "key"));
    t = (char *)frt_h_rem(h, "key", true);
    Asequal("new val", t);
    Asequal("freed", buf);
    frt_h_destroy(h);
}

typedef struct Point
{
    int x;
    int y;
} Point;

static int point_eq(const void *q1, const void *q2)
{
    Point *p1 = (Point *)q1;
    Point *p2 = (Point *)q2;
    return p1->x == p2->x && p1->y == p2->y;
}

static unsigned long long point_hash(const void *q)
{
    Point *p = (Point *)q;
    return p->x * p->y;
}

static Point *point_new(int x, int y)
{
    Point *p = FRT_ALLOC(Point);
    p->x = x;
    p->y = y;
    return p;
}

/**
 * Basic test for standard Hash. Make sure a non-string structure can be
 * used to key the Hash
 */
static void test_hash_point(TestCase *tc, void *data)
{
    Point *p1 = point_new(1, 2);
    Point *p2 = point_new(2, 1);
    Point *p3 = point_new(1, 2);
    FrtHash *h = frt_h_new(&point_hash, &point_eq, NULL, &free);
    (void)data; /* suppress unused argument warning */

    Assert(point_eq(p1, p3), "should be equal");

    Assert(frt_h_get(h, p1) == NULL, "No entries added yet");
    Assert(frt_h_get(h, p2) == NULL, "No entries added yet");
    Assert(frt_h_get(h, p3) == NULL, "No entries added yet");
    Aiequal(0, h->size);
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_h_set(h, p1, malloc_int(0)));
    Aiequal(1, h->size);
    Aiequal(0, *(int *)frt_h_get(h, p1));
    Aiequal(FRT_HASH_KEY_SAME, frt_h_set(h, p1, malloc_int(1)));
    Aiequal(1, h->size);
    Aiequal(1, *(int *)frt_h_get(h, p1));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_h_set(h, p2, malloc_int(2)));
    Aiequal(2, h->size);
    Aiequal(2, *(int *)frt_h_get(h, p2));
    Aiequal(FRT_HASH_KEY_EQUAL, frt_h_set(h, p3, malloc_int(3)));
    Aiequal(2, h->size);
    Aiequal(3, *(int *)frt_h_get(h, p3));
    Aiequal(3, *(int *)frt_h_get(h, p1));
    Aiequal(true, frt_h_del(h, p1));
    Aiequal(1, h->size);
    Assert(frt_h_get(h, p1) == NULL, "Entry should be deleted");
    frt_h_clear(h);
    Assert(frt_h_get(h, p2) == NULL, "Entry should be deleted");
    Aiequal(0, h->size);
    frt_h_destroy(h);
    free(p1);
    free(p2);
    free(p3);
}

/**
 * Test using integers as the key. This is also an example as to how to use
 * integers as the key.
 */
#define HASH_INT_TEST_SIZE 1000
static void test_hash_int(TestCase *tc, void *data)
{
    int i;
    FrtHash *h = frt_h_new_int(&free);
    char buf[100];
    char *word;
    (void)data; /* suppress unused argument warning */

    Aiequal(0, h->size);
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_h_set_int(h, 0, frt_estrdup("one")));
    Aiequal(1, h->size);
    Atrue(frt_h_has_key_int(h, 0));
    Assert(frt_h_set_safe_int(h, 10, frt_estrdup("ten")), "Not existing");
    Assert(!frt_h_set_safe_int(h, 10, (char *)"10"), "Won't overwrite existing");
    Asequal("ten", frt_h_get_int(h, 10));
    Aiequal(2, h->size);
    Atrue(frt_h_has_key_int(h, 10));
    frt_h_set_int(h, 1000, frt_estrdup("thousand"));
    Aiequal(3, h->size);
    Atrue(frt_h_has_key_int(h, 1000));
    Asequal("thousand", word = (char *)frt_h_rem_int(h, 1000));
    Aiequal(2, h->size);
    Atrue(!frt_h_has_key_int(h, 1000));
    Atrue(frt_h_has_key_int(h, 10));
    Atrue(!frt_h_set_safe_int(h, 10, word));
    free(word);
    frt_h_del_int(h, 10);

    for (i = 0; i < HASH_INT_TEST_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        frt_h_set_int(h, i, frt_estrdup(buf));
    }
    Asequal("<0>", frt_h_get_int(h, 0));
    Asequal("<100>", frt_h_get_int(h, 100));
    for (i = 0; i < HASH_INT_TEST_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        Asequal(buf, frt_h_get_int(h, i));
    }

    for (i = 0; i < HASH_INT_TEST_SIZE; i++) {
        frt_h_del_int(h, i);
    }
    Aiequal(0, h->size);

    frt_h_destroy(h);
}

/**
 * Test using pointers as the key. This is also an example as to how to use
 * pointers as the key.
 */
#define HASH_INT_TEST_SIZE 1000
static void test_hash_ptr(TestCase *tc, void *data)
{
    FrtHash *h = frt_h_new_ptr(&free);
    FrtSymbol word1 = rb_intern("one");
    FrtSymbol word2 = rb_intern("two");
    char *word_one = frt_estrdup("one");
    int i;
    char buf[100];
    (void)data; /* suppress unused argument warning */

    Aiequal(frt_ptr_hash((void *)word1), rb_intern("one"));
    Atrue(frt_ptr_eq((void *)word1, (void *)rb_intern("one")));
    frt_h_set(h, (void *)word1, frt_estrdup("1"));
    frt_h_set(h, (void *)word2, frt_estrdup("2"));
    frt_h_set(h, word_one, frt_estrdup("3"));
    Asequal("1", frt_h_get(h, (void *)word1));
    Asequal("2", frt_h_get(h, (void *)word2));
    Asequal("3", frt_h_get(h, word_one));

    Aiequal(3, h->size);
    for (i = 0; i < HASH_INT_TEST_SIZE; i++) {
        char *str = frt_strfmt("<%d>", i);
        frt_h_set(h, (void *)rb_intern(str), str);
    }
    Asequal("<0>", frt_h_get(h, (void *)rb_intern("<0>")));
    Asequal("<100>", frt_h_get(h, (void *)rb_intern("<100>")));
    for (i = 0; i < HASH_INT_TEST_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        Asequal(buf, frt_h_get(h, (void *)rb_intern(buf)));
    }

    for (i = 0; i < HASH_INT_TEST_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        frt_h_del(h, (void *)rb_intern(buf));
    }
    Aiequal(3, h->size);

    frt_h_destroy(h);
    free(word_one);
}

/**
 * Stress test the Hash. This test makes sure that the Hash still
 * works as it grows in size. The test has been run with 20,000,000 elements
 * on a 1Gb machine, but STRESS_SIZE is kept lower generally so that the tests
 * don't take too long.
 */
#define STRESS_SIZE 1000
static void stress_hash(TestCase *tc, void *data)
{
    int i, j, k;
    char buf[20];
    (void)data; /* suppress unused argument warning */

    for (k = 0; k < 1; k++) {
        FrtHash *h = frt_h_new_str(&free, &free);
        for (i = 0; i < STRESS_SIZE; i++) {
            sprintf(buf, "(%d)", i);
            if (frt_h_get(h, buf) != NULL) {
                Assert(false,
                       "h_get returned a result when it shouldn't have\n");
                return;
            }
            frt_h_set(h, frt_estrdup(buf), malloc_int(i));
        }


        for (j = 0; j < 1; j++) {
            for (i = 0; i < STRESS_SIZE; i++) {
                sprintf(buf, "(%d)", i);
                if (i != *(int *)frt_h_get(h, buf)) {
                    Assert(false, "h_get returned an incorrect result\n");
                    return;
                }
            }
        }

        for (i = 0; i < STRESS_SIZE / 2; i++) {
            sprintf(buf, "(%d)", i);
            if (!frt_h_del(h, buf)) {
                Assert(false, "h_del returned an error code\n");
                return;
            }
            if (frt_h_get(h, buf) != NULL) {
                Assert(false, "h_get returned an incorrect result\n");
                return;
            }
        }

        Aiequal(STRESS_SIZE / 2, h->size);
        frt_h_destroy(h);
    }
}

/**
 * Test that the hash table is ok while constantly growing and shrinking in
 * size
 */
static void test_hash_up_and_down(TestCase *tc, void *data)
{
    int i, j;
    char buf[20];

    FrtHash *h = frt_h_new_str(&free, &free);
    (void)data; /* suppress unused argument warning */

    for (j = 0; j < 50; j++) {
        for (i = j * 10; i < j * 10 + 10; i++) {
            sprintf(buf, "(%d)", i);
            if (frt_h_get(h, buf) != NULL) {
                Assert(false,
                       "h_get returned a result when it shouldn't have\n");
                return;
            }
            frt_h_set(h, frt_estrdup(buf), malloc_int(i));
            if (i != *(int *)frt_h_get(h, buf)) {
                Assert(false, "h_get returned an incorrect result\n");
                return;
            }
        }

        for (i = j * 10; i < j * 10 + 10; i++) {
            sprintf(buf, "(%d)", i);
            if (!frt_h_del(h, buf)) {
                Assert(false, "h_del returned an error code\n");
                return;
            }
            if (frt_h_get(h, buf) != NULL) {
                Assert(false, "h_get returned an incorrect result\n");
                return;
            }
        }
    }
    Aiequal(0, h->size);
    frt_h_destroy(h);
}

/**
 * Method used in frt_h_each test
 */
static void test_each_ekv(void *key, void *value, FrtHash *h)
{
    if ((strlen((char *)key) % 2) == 0) {
        frt_h_del(h, key);
    }
    else {
        frt_h_del(h, value);
    }
}

/**
 * Test Hash cloning, ie. the frt_h_clone function
 *
 * There is also a test in here of the frt_h_each method.
 */
static void test_hash_each_and_clone(TestCase *tc, void *data)
{
    const char *strs[] = { "one", "two", "three", "four", "five", "six", "seven", NULL };
    const char **s = strs;
    FrtHash *h = frt_h_new_str(&free, &free);
    FrtHash *ht2;
    (void)data; /* suppress unused argument warning */

    while (*s) {
        frt_h_set(h, frt_estrdup(*s), frt_estrdup(*s));
        s++;
    }
    frt_h_del(h, "two");
    frt_h_del(h, "four");

    Aiequal(7, h->fill);
    Aiequal(5, h->size);

    ht2 = frt_h_clone(h, (frt_h_clone_ft)&frt_estrdup, (frt_h_clone_ft)&frt_estrdup);

    Aiequal(7, h->fill);
    Aiequal(5, h->size);
    Aiequal(5, ht2->fill);
    Aiequal(5, ht2->size);

    frt_h_del(h, "seven");

    Aiequal(7, h->fill);
    Aiequal(4, h->size);
    Aiequal(5, ht2->fill);
    Aiequal(5, ht2->size);

    frt_h_each(h, (void (*)(void *k, void *v, void *a))&test_each_ekv, ht2);

    Aiequal(7, h->fill);
    Aiequal(4, h->size);
    Aiequal(5, ht2->fill);
    Aiequal(1, ht2->size);

    Apnotnull(frt_h_get(ht2, "seven"));
    Apnull(frt_h_get(ht2, "one"));
    Apnull(frt_h_get(ht2, "two"));
    Apnull(frt_h_get(ht2, "three"));
    Apnull(frt_h_get(ht2, "four"));
    Apnull(frt_h_get(ht2, "five"));
    Apnull(frt_h_get(ht2, "six"));
    frt_h_destroy(h);
    frt_h_destroy(ht2);
}

/*
 * The following code is given as an example of how to use the frt_h_each
 * function
 */

struct StringArray {
    char **strings;
    int cnt;
    int size;
};

static void add_string_ekv(void *key, void *value, struct StringArray *str_arr)
{
    (void)key; /* suppress unused argument warning */
    str_arr->strings[str_arr->cnt] = (char *)value;
    str_arr->cnt++;
}

static struct StringArray  *frt_h_extract_strings(FrtHash *h)
{
    struct StringArray *str_arr = FRT_ALLOC(struct StringArray);

    str_arr->strings = FRT_ALLOC_N(char *, h->size);
    str_arr->cnt = 0;
    str_arr->size = h->size;

    frt_h_each(h, (frt_h_each_key_val_ft)add_string_ekv, str_arr);

    return str_arr;
}

/**
 * Again, test the frt_h_each function, this time testing the example given in the
 * documentation for the each function.
 */
static void test_hash_extract_strings(TestCase *tc, void *data)
{
    int i;
    struct StringArray *str_arr;
    const char *strs[] = {"one", "two", "three", "four", "five"};
    FrtHash *h = frt_h_new_str(NULL, NULL);
    (void)data; /* suppress unused argument warning */

    for (i = 0; i < (int)FRT_NELEMS(strs); i++) {
        frt_h_set(h, strs[i], (void *)strs[i]);
    }

    str_arr = frt_h_extract_strings(h);

    if (Aiequal(FRT_NELEMS(strs), str_arr->size)) {
        for (i = 0; i < (int)FRT_NELEMS(strs); i++) {
            int j;
            bool str_found = false;
            for (j = 0; j < (int)FRT_NELEMS(strs); j++) {
                if (strcmp(strs[i], str_arr->strings[j]) == 0) {
                    str_found = true;
                }
            }
            Assert(str_found, "String was not found where it should've been");
        }
    }

    frt_h_destroy(h);
    free(str_arr->strings);
    free(str_arr);
}

TestSuite *ts_hash(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_hash_str, NULL);
    tst_run_test(suite, test_hash_point, NULL);
    tst_run_test(suite, test_hash_int, NULL);
    tst_run_test(suite, test_hash_ptr, NULL);
    tst_run_test(suite, stress_hash, NULL);
    tst_run_test(suite, test_hash_up_and_down, NULL);
    tst_run_test(suite, test_hash_each_and_clone, NULL);
    tst_run_test(suite, test_hash_extract_strings, NULL);

    return suite;
}
