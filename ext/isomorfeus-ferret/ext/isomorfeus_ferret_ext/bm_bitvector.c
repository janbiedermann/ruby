#include <assert.h>
#include "frt_bitvector.h"
#include "benchmark.h"

#define N 10
#define DENSE_SCAN_SIZE 20000000
#define SCAN_INC 97
#define SCAN_SIZE DENSE_SCAN_SIZE * SCAN_INC

static FrtBitVector *bv;

static void setup()
{
    bv = frt_bv_new_capa(SCAN_SIZE);
}

static void teardown()
{
    frt_bv_destroy(bv);
}

static void ferret_bv_and_sparse()
{
    FrtBitVector * _bv = frt_bv_and(bv, bv);
    free(_bv);
}
static void ferret_bv_or_sparse()
{
    FrtBitVector * _bv = frt_bv_or(bv, bv);
    free(_bv);
}
static void ferret_bv_xor_sparse()
{
    FrtBitVector * _bv = frt_bv_xor(bv, bv);
    free(_bv);
}
static void ferret_bv_not_sparse()
{
    FrtBitVector * _bv = frt_bv_not(bv);
    free(_bv);
}
static void ferret_bv_and_dense()
{
    ferret_bv_and_sparse();
}
static void ferret_bv_or_dense()
{
    ferret_bv_or_sparse();
}
static void ferret_bv_xor_dense()
{
    ferret_bv_xor_sparse();
}
static void ferret_bv_not_dense()
{
    ferret_bv_not_sparse();
}

static void ferret_bv_set_sparse()
{
    int i;

    for (i = SCAN_INC; i < SCAN_SIZE; i += SCAN_INC) {
        frt_bv_set_fast(bv, i);
        assert(frt_bv_get(bv, i) == 1);
        assert(frt_bv_get(bv, i+1) == 0);
    }
}

static void ferret_bv_scan_sparse()
{
    int i, j;

    for (i = 0; i < N; i++) {
        frt_bv_scan_reset(bv);
        for (j = SCAN_INC; j < SCAN_SIZE; j += SCAN_INC) {
            assert(j == frt_bv_scan_next(bv));
        }
        assert(-1 == frt_bv_scan_next(bv));
    }
}

static void ferret_bv_set_dense()
{
    int i;
    frt_bv_clear(bv);
    for (i = 0; i < DENSE_SCAN_SIZE; i++) {
        frt_bv_set(bv, i);
    }
}

static void ferret_bv_scan_dense()
{
    int i, j;

    for (i = 0; i < N; i++) {
        frt_bv_scan_reset(bv);
        for (j = 0; j < DENSE_SCAN_SIZE; j++) {
            assert(j == frt_bv_scan_next(bv));
        }
        assert(-1 == frt_bv_scan_next(bv));
    }
}

BENCH(bitvector_implementations)
{
    BM_SETUP(setup);

    BM_ADD(ferret_bv_set_sparse);
    BM_ADD(ferret_bv_scan_sparse);
    BM_ADD(ferret_bv_and_sparse);
    BM_ADD(ferret_bv_or_sparse);
    BM_ADD(ferret_bv_not_sparse);
    BM_ADD(ferret_bv_xor_sparse);

    BM_ADD(ferret_bv_set_dense);
    BM_ADD(ferret_bv_scan_dense);
    BM_ADD(ferret_bv_and_dense);
    BM_ADD(ferret_bv_or_dense);
    BM_ADD(ferret_bv_not_dense);
    BM_ADD(ferret_bv_xor_dense);
    BM_TEARDOWN(teardown);
}
