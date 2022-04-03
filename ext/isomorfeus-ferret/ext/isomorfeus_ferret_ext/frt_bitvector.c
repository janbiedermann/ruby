#include "frt_bitvector.h"
#include <string.h>

FrtBitVector *frt_bv_new_capa(int capa) {
    FrtBitVector *bv = FRT_ALLOC_AND_ZERO(FrtBitVector);

    /* The capacity passed by the user is number of bits allowed, however we
     * store capacity as the number of words (U32) allocated. */
    bv->capa = FRT_MAX(FRT_TO_WORD(capa), 4);
    bv->bits = FRT_ALLOC_AND_ZERO_N(frt_u32, bv->capa);
    bv->curr_bit = -1;
    bv->ref_cnt = 1;
    return bv;
}

FrtBitVector *frt_bv_new(void) {
    return frt_bv_new_capa(FRT_BV_INIT_CAPA);
}

void frt_bv_destroy(FrtBitVector *bv) {
    if (--(bv->ref_cnt) == 0) {
        free(bv->bits);
        free(bv);
    }
}

void frt_bv_clear(FrtBitVector *bv) {
    memset(bv->bits, 0, bv->capa * sizeof(frt_u32));
    bv->extends_as_ones = 0;
    bv->count = 0;
    bv->size = 0;
}

void frt_bv_scan_reset(FrtBitVector *bv) {
    bv->curr_bit = -1;
}

int frt_bv_eq(FrtBitVector *bv1, FrtBitVector *bv2) {
    frt_u32 *bits, *bits2;
    int min_size, word_size, ext_word_size = 0, i;
    if (bv1 == bv2) {
        return true;
    }

    if (bv1->extends_as_ones != bv2->extends_as_ones) {
        return false;
    }

    bits = bv1->bits;
    bits2 = bv2->bits;
    min_size = FRT_MIN(bv1->size, bv2->size);
    word_size = FRT_TO_WORD(min_size);

    for (i = 0; i < word_size; i++) {
        if (bits[i] != bits2[i]) {
            return false;
        }
    }
    if (bv1->size > min_size) {
        bits = bv1->bits;
        ext_word_size = FRT_TO_WORD(bv1->size);
    } else if (bv2->size > min_size) {
        bits = bv2->bits;
        ext_word_size = FRT_TO_WORD(bv2->size);
    }
    if (ext_word_size) {
        const frt_u32 expected = (bv1->extends_as_ones ? 0xFFFFFFFF : 0);
        for (i = word_size; i < ext_word_size; i++) {
            if (bits[i] != expected) {
                return false;
            }
        }
    }
    return true;
}

unsigned long long frt_bv_hash(FrtBitVector *bv) {
    unsigned long long hash = 0;
    const frt_u32 empty_word = bv->extends_as_ones ? 0xFFFFFFFF : 0;
    int i;
    for (i = FRT_TO_WORD(bv->size) - 1; i >= 0; i--) {
        const frt_u32 word = bv->bits[i];
        if (word != empty_word)
            hash = (hash << 1) ^ word;
    }
    return (hash << 1) | bv->extends_as_ones;
}
