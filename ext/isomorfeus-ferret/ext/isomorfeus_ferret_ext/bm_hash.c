#include <string.h>
#include "frt_hash.h"
#include "benchmark.h"

#define N 20

static void ferret_hash(void) {
    int i;
    void *res = NULL;
    for (i = 0; i < N; i++) {
        FrtHash *h = frt_h_new_str(NULL, NULL);
        const char **word;
        char buf[100];
        for (word = WORD_LIST; *word; word++) {
            frt_h_set(h, *word, (void *)1);
        }
        for (word = WORD_LIST; *word; word++) {
            strcpy(buf, *word);
            res = frt_h_get(h, buf);
        }
        frt_h_destroy(h);
    }
    (void)res;
}

BENCH(hash_implementations) {
    BM_ADD(ferret_hash);
}

static void standard_hash(void) {
    int i;
    void *res = NULL;
    for (i = 0; i < N; i++) {
        FrtHash *h = frt_h_new_str(NULL, NULL);
        const char **word;
        char buf[100];
        for (word = WORD_LIST; *word; word++) {
            frt_h_set(h, *word, (void *)1);
            strcpy(buf, *word);
            res = frt_h_get(h, buf);
        }
        frt_h_destroy(h);
    }
    (void)res;
}

#define PERTURB_SHIFT 5
static const char *dummy_key = "";
static FrtHashEntry *h_lookup_str(FrtHash *ht, register const void *key) {
    register const unsigned long hash = frt_str_hash((const char *)key);
    register unsigned int perturb;
    register int mask = ht->mask;
    register FrtHashEntry *he0 = ht->table;
    register int i = hash & mask;
    register FrtHashEntry *he = &he0[i];
    register FrtHashEntry *freeslot = NULL;

    if (he->key == NULL || he->key == key) {
        he->hash = hash;
        return he;
    }
    if (he->key == dummy_key) {
        freeslot = he;
    }
    else {
        if ((he->hash == hash)
            && 0 == strcmp((const char *)he->key, (const char *)key)) {
            return he;
        }
    }

    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        he = &he0[i & mask];
        if (he->key == NULL) {
            if (freeslot != NULL) {
                he = freeslot;
            }
            he->hash = hash;
            return he;
        }
        if (he->key == key
            || (he->hash == hash
                && he->key != dummy_key
                && 0 == strcmp((const char *)he->key, (const char *)key))) {
            return he;
        }
        if (he->key == dummy_key && freeslot == NULL) {
            freeslot = he;
        }
    }
}

static void string_hash(void) {
    int i;
    void *res = NULL;
    for (i = 0; i < N; i++) {
        FrtHash *h = frt_h_new_str(NULL, NULL);
        const char **word;
        char buf[100];
        h->lookup_i = &h_lookup_str;
        for (word = WORD_LIST; *word; word++) {
            frt_h_set(h, *word, (void *)1);
            strcpy(buf, *word);
            res = frt_h_get(h, buf);
        }
        frt_h_destroy(h);
    }
    (void)res;
}

BENCH(specialized_string_hash) {
    BM_ADD(standard_hash);
    BM_ADD(string_hash);
}
