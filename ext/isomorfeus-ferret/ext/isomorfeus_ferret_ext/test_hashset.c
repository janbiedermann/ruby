#include "frt_global.h"
#include "frt_hashset.h"
#include "test.h"

/**
 * Test basic FrtHashSet functions like adding elements and testing for
 * existence.
 */
static void test_hs(TestCase *tc, void *data)
{
    char *two = frt_estrdup("two");
    FrtHashSet *hs = frt_hs_new_str(&free);
    (void)data; /* suppress unused argument warning */

    Atrue(FRT_HASH_KEY_EQUAL);
    Atrue(!FRT_HASH_KEY_DOES_NOT_EXIST);
    Aiequal(0, hs->size);

    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_add(hs, frt_estrdup("one")));
    Aiequal(1, hs->size);
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, two));

    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_add(hs, frt_estrdup("one")));
    Aiequal(1, hs->size);
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "one"));

    frt_hs_add(hs, two);
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "two"));
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_exists(hs, two));
    Apequal(two, frt_hs_orig(hs, "two"));
    Apequal(two, frt_hs_orig(hs, two));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "three"));
    Aiequal(2, hs->size);

    frt_hs_add(hs, frt_estrdup("three"));
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "two"));
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_exists(hs, two));
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "three"));
    Aiequal(3, hs->size);

    frt_hs_del(hs, "two");
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "two"));
    Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, "three"));
    Aiequal(2, hs->size);
    Asequal("one", hs->first->elem);
    Asequal("three", hs->first->next->elem);

    frt_hs_clear(hs);
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "two"));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "three"));
    Aiequal(0, hs->size);

    frt_hs_destroy(hs);
}

static void test_hs_ptr(TestCase *tc, void *data)
{
    FrtHashSet *hs = frt_hs_new_ptr(NULL);
    FrtSymbol word1 = rb_intern("one"), word2 = rb_intern("two");
    char *word_one = frt_estrdup("one");
    (void)data; /* suppress unused argument warning */

    Aiequal(0, hs->size);

    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_add(hs, (void *)word1));
    Aiequal(1, hs->size);
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_exists(hs, (void *)word1));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_add(hs, (void *)word1));
    Aiequal(1, hs->size);

    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_add(hs, (void *)word2));
    Aiequal(2, hs->size);
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_exists(hs, (void *)word2));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "two"));
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_add(hs, (void *)word2));
    Aiequal(2, hs->size);

    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_add(hs, (void *)word_one));
    Aiequal(3, hs->size);
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_exists(hs, word_one));
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs, "one"));
    Aiequal(FRT_HASH_KEY_SAME, frt_hs_add(hs, (void *)word_one));
    Aiequal(3, hs->size);

    free(word_one);
    frt_hs_destroy(hs);
}

/**
 * Test hs_add_safe
 */

static void test_hs_add_safe(TestCase *tc, void *data)
{
    char *str = frt_estrdup("one");
    FrtHashSet *hs = frt_hs_new_str(&free);
    (void)data; /* suppress unused argument warning */

    Atrue(frt_hs_add_safe(hs, str));
    Atrue(frt_hs_add_safe(hs, (char *)str));
    Atrue(!frt_hs_add_safe(hs, (char *)"one"));

    int to_add = 100;
    int idx = 0;
    for (; to_add >= 0; --to_add)
    {
        snprintf(str, sizeof(str)/sizeof(str[0]), "%d", idx);
        Atrue(frt_hs_add_safe(hs, frt_estrdup(str)));
        ++idx;
    }

    for (idx = 0; idx <= to_add; ++idx)
    {
        snprintf(str, sizeof(str)/sizeof(str[0]), "%d", idx);
        Aiequal(FRT_HASH_KEY_EQUAL, frt_hs_exists(hs, str));
    }

    frt_hs_destroy(hs);
}

/**
 * Test merging of two HashSets. When one FrtHashSet is merged into another the
 * FrtHashSet that was merged should be destroyed including all elements that
 * weren't added to the final HashSet.
 */
static void test_hs_merge(TestCase *tc, void *data)
{
    FrtHashSet *hs1 = frt_hs_new_str(&free);
    FrtHashSet *hs2 = frt_hs_new_str(&free);
    FrtHashSetEntry *hse;
    (void)data; /* suppress unused argument warning */

    frt_hs_add(hs1, frt_estrdup("one"));
    frt_hs_add(hs1, frt_estrdup("two"));
    frt_hs_add(hs1, frt_estrdup("three"));
    frt_hs_add(hs2, frt_estrdup("two"));
    frt_hs_add(hs2, frt_estrdup("three"));
    frt_hs_add(hs2, frt_estrdup("four"));
    Aiequal(3, hs1->size);
    Aiequal(3, hs2->size);
    frt_hs_merge(hs1, hs2);
    Aiequal(4, hs1->size);
    hse = hs1->first;
    Asequal("one", hse->elem);
    hse = hse->next;
    Asequal("two", hse->elem);
    hse = hse->next;
    Asequal("three", hse->elem);
    hse = hse->next;
    Asequal("four", hse->elem);
    hse = hse->next;
    Apnull(hse);
    frt_hs_destroy(hs1);
}

/**
 * Free Mock used to test that certain elements are being freed when the
 * FrtHashSet is destroyed.
 */
static void hs_free_mock(void *p)
{
    char *str = (char *) p;
    strcpy(str, "free");
}

/**
 * Test that HashSets are freed correctly. That is, make sure that when a
 * FrtHashSet is destroyed, all elements have the correct free function called on
 * them
 */
static void test_hs_free(TestCase *tc, void *data)
{
    char str1[10], str2[10], str3[10], str4[10], str5[10];
    FrtHashSet *hs1 = frt_hs_new_str(&hs_free_mock);
    FrtHashSet *hs2 = frt_hs_new_str(&hs_free_mock);
    FrtHashSetEntry *hse;
    (void)data; /* suppress unused argument warning */

    strcpy(str1, "one");
    strcpy(str2, "one");
    Atrue(frt_hs_add_safe(hs1, (char *)str1));
    Atrue(frt_hs_add_safe(hs1, (char *)str1));
    Atrue(!frt_hs_add_safe(hs1, (char *)"one"));
    Asequal("one", str1);

    frt_hs_add(hs1, str2);
    Asequal("free", str2);
    frt_hs_rem(hs1, "one");
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs1, "one"));
    Asequal("one", str1);
    frt_hs_add(hs1, str1);
    frt_hs_del(hs1, "one");
    Aiequal(FRT_HASH_KEY_DOES_NOT_EXIST, frt_hs_exists(hs1, "one"));
    Asequal("free", str1);

    strcpy(str1, "one");
    strcpy(str2, "two");
    strcpy(str3, "three");
    strcpy(str4, "three");
    strcpy(str5, "four");
    frt_hs_add(hs1, str1);
    frt_hs_add(hs1, str2);
    frt_hs_add(hs1, str3);
    frt_hs_add(hs2, str2);
    frt_hs_add(hs2, str4);
    frt_hs_add(hs2, str5);
    Aiequal(3, hs1->size);
    Aiequal(3, hs2->size);
    frt_hs_merge(hs1, hs2);
    Aiequal(4, hs1->size);
    Asequal("free", str4);
    hse = hs1->first;
    Apequal(str1, hse->elem);
    hse = hse->next;
    Apequal(str2, hse->elem);
    hse = hse->next;
    Apequal(str3, hse->elem);
    hse = hse->next;
    Apequal(str5, hse->elem);
    hse = hse->next;
    Apnull(hse);
    Asequal("one", str1);
    Asequal("two", str2);
    Asequal("three", str3);
    Asequal("four", str5);
    frt_hs_destroy(hs1);
    Asequal("free", str1);
    Asequal("free", str2);
    Asequal("free", str3);
    Asequal("free", str5);
}

/**
 * FrtHashSet stress test. Make sure the FrtHashSet works under load.
 */
#define HS_STRESS_NUM 10000 /* number of adds to the FrtHashSet */
#define HS_STRESS_MAX 100   /* number of elements allowed in the FrtHashSet */
static void stress_hs(TestCase *tc, void *data)
{
    int i;
    char buf[100];
    FrtHashSet *hs = frt_hs_new_str(&free);
    (void)data; /* suppress unused argument warning */

    for (i = 0; i < HS_STRESS_NUM; i++) {
        sprintf(buf, "<%d>", rand() % HS_STRESS_MAX);
        frt_hs_add(hs, frt_estrdup(buf));
    }
    Assert(hs->size <= HS_STRESS_MAX,
           "all numbers should be between 0 and %d", HS_STRESS_MAX);

    /* make sure none of the slots were left out for next test */
    for (i = 0; i < HS_STRESS_MAX; i++) {
        sprintf(buf, "<%d>", i);
        frt_hs_add(hs, frt_estrdup(buf));
    }

    for (i = 0; i < HS_STRESS_MAX; i++) {
        sprintf(buf, "<%d>", i);
        if (!Atrue(frt_hs_exists(hs, buf))) {
            Tmsg("Couldn't find \"%s\"", buf);
        }
    }
    frt_hs_destroy(hs);
}

/**
 * FrtHashSet Test Suite
 */
TestSuite *ts_hashset(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_hs, NULL);
    tst_run_test(suite, test_hs_ptr, NULL);
    tst_run_test(suite, test_hs_add_safe, NULL);
    tst_run_test(suite, test_hs_merge, NULL);
    tst_run_test(suite, test_hs_free, NULL);
    tst_run_test(suite, stress_hs, NULL);

    return suite;
}
