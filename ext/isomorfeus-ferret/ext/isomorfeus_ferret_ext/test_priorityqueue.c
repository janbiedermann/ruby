#include "frt_priorityqueue.h"
#include "frt_global.h"
#include <string.h>
#include "test.h"

/**
 * Simple string less than function used for testing adding of strings to the
 * priority queue.
 */
static bool str_lt(void *p1, void *p2)
{
    return (strcmp((char *) p1, (char *) p2) < 0);
}

static bool str_lt_rev(void *p1, void *p2)
{
    return (strcmp((char *) p1, (char *) p2) > 0);
}

/**
 * Test basic FrtPriorityQueue functions.
 */
static void test_pq(TestCase *tc, void *data)
{
    char *tmp;
    FrtPriorityQueue *pq = frt_pq_new(4, (frt_lt_ft)&str_lt, &free);
    (void)data; /* suppress unused argument warning */

    Aiequal(0, pq->size);
    Aiequal(4, pq->capa);
    frt_pq_push(pq, frt_estrdup("bword"));
    Aiequal(1, pq->size);
    Asequal("bword", (char *) frt_pq_top(pq));
    frt_pq_push(pq, frt_estrdup("cword"));
    Aiequal(2, pq->size);
    Asequal("bword", (char *) frt_pq_top(pq));
    frt_pq_push(pq, frt_estrdup("aword"));
    Aiequal(3, pq->size);
    Asequal("aword", (char *) frt_pq_top(pq));
    frt_pq_push(pq, frt_estrdup("dword"));
    Aiequal(4, pq->size);
    Asequal("aword", (char *) frt_pq_top(pq));
    Asequal("aword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(3, pq->size);
    free(tmp);
    Asequal("bword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(2, pq->size);
    free(tmp);
    Asequal("cword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(1, pq->size);
    free(tmp);
    Asequal("dword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(0, pq->size);
    free(tmp);
    frt_pq_destroy(pq);
}

/**
 * Test basic FrtPriorityQueue functions.
 */
static void test_pq_reverse(TestCase *tc, void *data)
{
    char *tmp;
    FrtPriorityQueue *pq = frt_pq_new(4, (frt_lt_ft)&str_lt_rev, &free);
    (void)data; /* suppress unused argument warning */

    Aiequal(0, pq->size);
    Aiequal(4, pq->capa);
    frt_pq_push(pq, frt_estrdup("bword"));
    Aiequal(1, pq->size);
    Asequal("bword", (char *) frt_pq_top(pq));
    frt_pq_push(pq, frt_estrdup("cword"));
    Aiequal(2, pq->size);
    Asequal("cword", (char *) frt_pq_top(pq));
    frt_pq_push(pq, frt_estrdup("aword"));
    Aiequal(3, pq->size);
    Asequal("cword", (char *) frt_pq_top(pq));
    frt_pq_push(pq, frt_estrdup("dword"));
    Aiequal(4, pq->size);
    Asequal("dword", (char *) frt_pq_top(pq));
    Asequal("dword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(3, pq->size);
    free(tmp);
    Asequal("cword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(2, pq->size);
    free(tmp);
    Asequal("bword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(1, pq->size);
    free(tmp);
    Asequal("aword", tmp = (char *) frt_pq_pop(pq));
    Aiequal(0, pq->size);
    free(tmp);
    frt_pq_destroy(pq);
}

/**
 * Free mock used to test that the FrtPriorityQueue is being destroyed correctly
 * including it's elements
 */
static void pq_free_mock(void *p)
{
    char *str = (char *) p;
    strcpy(str, "free");
}

/**
 * Test pq_clear function
 */
static void test_pq_clear(TestCase *tc, void *data)
{
    char word1[10] = "word1";
    char word2[10] = "word2";
    char word3[10] = "word3";
    FrtPriorityQueue *pq = frt_pq_new(3, (frt_lt_ft)&str_lt, &pq_free_mock);
    (void)data; /* suppress unused argument warning */

    frt_pq_push(pq, word1);
    frt_pq_push(pq, word2);
    frt_pq_push(pq, word3);
    Aiequal(3, pq->size);
    frt_pq_clear(pq);
    Aiequal(0, pq->size);
    Asequal("free", word1);
    Asequal("free", word2);
    Asequal("free", word3);
    frt_pq_destroy(pq);
}

/**
 * Test that FrtPriorityQueue will handle insert overflow. That is, when you
 * insert more than the PriorityQueue's capacity, the extra elements that drop
 * off the bottom are destroyed.
 */
static void test_pq_insert_overflow(TestCase *tc, void *data)
{
    char word1[10] = "word1";
    char word2[10] = "word2";
    char word3[10] = "word3";
    char word4[10] = "word4";
    char word5[10] = "word5";
    char word6[10] = "word6";
    FrtPriorityQueue *pq = frt_pq_new(3, (frt_lt_ft)&str_lt, &pq_free_mock);
    (void)data; /* suppress unused argument warning */

    Aiequal(FRT_PQ_ADDED, frt_pq_insert(pq, word2));
    Aiequal(FRT_PQ_ADDED, frt_pq_insert(pq, word3));
    Aiequal(FRT_PQ_ADDED, frt_pq_insert(pq, word4));
    Aiequal(FRT_PQ_INSERTED, frt_pq_insert(pq, word5));
    Aiequal(FRT_PQ_INSERTED, frt_pq_insert(pq, word6));
    Aiequal(FRT_PQ_DROPPED, frt_pq_insert(pq, word1));
    Aiequal(3, pq->size);
    Asequal("free", word1);
    Asequal("free", word2);
    Asequal("free", word3);
    Asequal("word4", word4);
    Asequal("word5", word5);
    Asequal("word6", word6);
    frt_pq_clear(pq);
    Aiequal(0, pq->size);
    Asequal("free", word4);
    Asequal("free", word5);
    Asequal("free", word6);
    frt_pq_destroy(pq);
}

/**
 * Stress test the PriorityQueue. Make PQ_STRESS_SIZE much larger if you want
 * to really stress test PriorityQueue.
 */
#define PQ_STRESS_SIZE 1000
static void stress_pq(TestCase *tc, void *data)
{
    int i;
    char buf[100], *prev, *curr;
    FrtPriorityQueue *pq = frt_pq_new(PQ_STRESS_SIZE, (frt_lt_ft)&str_lt, &free);
    (void)data; /* suppress unused argument warning */

    for (i = 0; i < PQ_STRESS_SIZE; i++) {
        sprintf(buf, "<%d>", rand());
        frt_pq_push(pq, frt_estrdup(buf));
    }
    Aiequal(PQ_STRESS_SIZE, pq->size);

    prev = (char *) frt_pq_pop(pq);
    for (i = 0; i < PQ_STRESS_SIZE - 1; i++) {
        curr = (char *) frt_pq_pop(pq);
        if (str_lt(curr, prev) == true) {
            Assert(false, "previous should be less than or equal to current");
            Tmsg("%d: %s, %s\n", i, prev, curr);
        }
        free(prev);
        prev = curr;
    }
    free(prev);
    frt_pq_clear(pq);
    frt_pq_destroy(pq);
}

/**
 * PriorityQueue's test suite
 */
TestSuite *ts_priorityqueue(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_pq, NULL);
    tst_run_test(suite, test_pq_reverse, NULL);
    tst_run_test(suite, test_pq_clear, NULL);
    tst_run_test(suite, test_pq_insert_overflow, NULL);
    tst_run_test(suite, stress_pq, NULL);

    return suite;
}
