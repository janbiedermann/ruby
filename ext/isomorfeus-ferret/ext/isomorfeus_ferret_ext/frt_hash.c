#include "frt_hash.h"
#include "frt_global.h"
#include <string.h>

/****************************************************************************
 *
 * FrtHash
 *
 * This hash table is modeled after Python's dictobject and a description of
 * the algorithm can be found in the file dictobject.c in Python's src
 ****************************************************************************/

static const char *dummy_key = "";
static const char *dummy_int_key = "i";

#define PERTURB_SHIFT 5
#define MAX_FREE_HASH_TABLES 80

static FrtHash *free_hts[MAX_FREE_HASH_TABLES];
static int num_free_hts = 0;

unsigned long long frt_str_hash(const char *const str)
{
    register unsigned long long h = 0;
    register unsigned char *p = (unsigned char *)str;

    for (; *p; p++) {
        h = 37 * h + *p;
    }
    return h;
}

unsigned long long frt_ptr_hash(const void *const ptr)
{
    return (unsigned long long)ptr;
}

int frt_ptr_eq(const void *q1, const void *q2)
{
    return q1 == q2;
}

static int str_eq(const void *q1, const void *q2)
{
    return (q1 && q2 && (strcmp((const char *)q1, (const char *)q2) == 0));
}

typedef FrtHashEntry *(*lookup_ft)(struct FrtHash *self, register const void *key);

/**
 * Fast lookup function for resizing as we know there are no equal elements or
 * deletes to worry about.
 *
 * @param self the Hash to do the fast lookup in
 * @param the hashkey we are looking for
 */
static FrtHashEntry  *frt_h_resize_lookup(FrtHash *self,
                                         register const unsigned long long hash)
{
    register unsigned long perturb;
    register int mask = self->mask;
    register FrtHashEntry *he0 = self->table;
    register int i = hash & mask;
    register FrtHashEntry *he = &he0[i];

    if (he->key == NULL) {
        he->hash = hash;
        return he;
    }

    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        he = &he0[i & mask];
        if (he->key == NULL) {
            he->hash = hash;
            return he;
        }
    }
}

static FrtHashEntry  *frt_h_lookup_ptr(FrtHash *self, const void *key)
{
    register const unsigned long long hash = (unsigned long long)key;
    register unsigned long perturb;
    register int mask = self->mask;
    register FrtHashEntry *he0 = self->table;
    register int i = hash & mask;
    register FrtHashEntry *he = &he0[i];
    register FrtHashEntry *freeslot = NULL;

    if (he->key == NULL || he->hash == hash) {
        he->hash = hash;
        return he;
    }
    if (he->key == dummy_key) {
        freeslot = he;
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
        if (he->hash == hash) {
            return he;
        }
        if (he->key == dummy_key && freeslot == NULL) {
            freeslot = he;
        }
    }
}

FrtHashEntry *frt_h_lookup(FrtHash *self, register const void *key)
{
    register const unsigned long long hash = self->hash_i(key);
    register unsigned long perturb;
    register int mask = self->mask;
    register FrtHashEntry *he0 = self->table;
    register int i = hash & mask;
    register FrtHashEntry *he = &he0[i];
    register FrtHashEntry *freeslot = NULL;
    frt_eq_ft eq = self->eq_i;

    if (he->key == NULL || he->key == key) {
        he->hash = hash;
        return he;
    }
    if (he->key == dummy_key) {
        freeslot = he;
    } else {
        if ((he->hash == hash) && eq(he->key, key)) {
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
                && he->key != dummy_key && eq(he->key, key))) {
            return he;
        }
        if (he->key == dummy_key && freeslot == NULL) {
            freeslot = he;
        }
    }
}

FrtHash *frt_h_new_str(frt_free_ft free_key, frt_free_ft free_value)
{
    FrtHash *self;
    if (num_free_hts > 0) {
        self = free_hts[--num_free_hts];
    }
    else {
        self = FRT_ALLOC(FrtHash);
    }
    self->fill = 0;
    self->size = 0;
    self->mask = FRT_HASH_MINSIZE - 1;
    self->table = self->smalltable;
    memset(self->smalltable, 0, sizeof(self->smalltable));
    self->lookup_i = (lookup_ft)&frt_h_lookup;
    self->eq_i = str_eq;
    self->hash_i = (frt_hash_ft)frt_str_hash;

    self->free_key_i = free_key != NULL ? free_key : &frt_dummy_free;
    self->free_value_i = free_value != NULL ? free_value : &frt_dummy_free;
    self->ref_cnt = 1;
    return self;
}

FrtHash *frt_h_new_int(frt_free_ft free_value)
{
    FrtHash *self     = frt_h_new_str(NULL, free_value);

    self->lookup_i = &frt_h_lookup_ptr;
    self->eq_i     = NULL;
    self->hash_i   = NULL;

    return self;
}

FrtHash *frt_h_new(frt_hash_ft hash, frt_eq_ft eq, frt_free_ft free_key, frt_free_ft free_value)
{
    FrtHash *self     = frt_h_new_str(free_key, free_value);

    self->lookup_i = &frt_h_lookup;
    self->eq_i     = eq;
    self->hash_i   = hash;

    return self;
}

void frt_h_clear(FrtHash *self)
{
    int i;
    FrtHashEntry *he;
    frt_free_ft free_key   = self->free_key_i;
    frt_free_ft free_value = self->free_value_i;

    /* Clear all the hash values and keys as necessary */
    if (free_key != frt_dummy_free || free_value != frt_dummy_free) {
        for (i = 0; i <= self->mask; i++) {
            he = &self->table[i];
            if (he->key != NULL && he->key != dummy_key) {
                free_value(he->value);
                free_key(he->key);
            }
            he->key = NULL;
        }
    }
    FRT_ZEROSET_N(self->table, FrtHashEntry, self->mask + 1);
    self->size = 0;
    self->fill = 0;
}

void frt_h_destroy(FrtHash *self)
{
    if (--(self->ref_cnt) <= 0) {
        frt_h_clear(self);

        /* if a new table was created, be sure to free it */
        if (self->table != self->smalltable) {
            free(self->table);
        }

        if (num_free_hts < MAX_FREE_HASH_TABLES) {
            free_hts[num_free_hts++] = self;
        }
        else {
            free(self);
        }
    }
}

void *frt_h_get(FrtHash *self, const void *key)
{
    /* Note: lookup_i will never return NULL. */
    return self->lookup_i(self, key)->value;
}

int frt_h_del(FrtHash *self, const void *key)
{
    FrtHashEntry *he = self->lookup_i(self, key);

    if (he->key != NULL && he->key != dummy_key) {
        self->free_key_i(he->key);
        self->free_value_i(he->value);
        he->key = (char *)dummy_key;
        he->value = NULL;
        self->size--;
        return true;
    }
    else {
        return false;
    }
}

void *frt_h_rem(FrtHash *self, const void *key, bool destroy_key)
{
    void *val;
    FrtHashEntry *he = self->lookup_i(self, key);

    if (he->key != NULL && he->key != dummy_key) {
        if (destroy_key) {
            self->free_key_i(he->key);
        }

        he->key = (char *)dummy_key;
        val = he->value;
        he->value = NULL;
        self->size--;
        return val;
    }
    else {
        return NULL;
    }
}

static int frt_h_resize(FrtHash *self, int min_newsize)
{
    FrtHashEntry smallcopy[FRT_HASH_MINSIZE];
    FrtHashEntry *oldtable;
    FrtHashEntry *he_old, *he_new;
    int newsize, num_active;

    /* newsize will be a power of two */
    for (newsize = FRT_HASH_MINSIZE; newsize < min_newsize; newsize <<= 1) {
    }

    oldtable = self->table;
    if (newsize == FRT_HASH_MINSIZE) {
        if (self->table == self->smalltable) {
            /* need to copy the data out so we can rebuild the table into
             * the same space */
            memcpy(smallcopy, self->smalltable, sizeof(smallcopy));
            oldtable = smallcopy;
        }
        else {
            self->table = self->smalltable;
        }
    }
    else {
        self->table = FRT_ALLOC_N(FrtHashEntry, newsize);
    }
    memset(self->table, 0, sizeof(FrtHashEntry) * newsize);
    self->fill = self->size;
    self->mask = newsize - 1;

    for (num_active = self->size, he_old = oldtable; num_active > 0; he_old++) {
        if (he_old->key && he_old->key != dummy_key) {    /* active entry */
            /*he_new = self->lookup_i(self, he_old->key); */
            he_new = frt_h_resize_lookup(self, he_old->hash);
            he_new->key = he_old->key;
            he_new->value = he_old->value;
            num_active--;
        }                       /* else empty entry so nothing to do */
    }
    if (oldtable != smallcopy && oldtable != self->smalltable) {
        free(oldtable);
    }
    return 0;
}

bool frt_h_set_ext(FrtHash *self, const void *key, FrtHashEntry **he)
{
    *he = self->lookup_i(self, key);
    if ((*he)->key == NULL) {
        if (self->fill * 3 > self->mask * 2) {
            frt_h_resize(self, self->size * ((self->size > FRT_SLOW_DOWN) ? 4 : 2));
            *he = self->lookup_i(self, key);
        }
        self->fill++;
        self->size++;
        return true;
    } else if ((*he)->key == dummy_key) {
        self->size++;
        return true;
    }
    return false;
}

FrtHashKeyStatus frt_h_set(FrtHash *self, const void *key, void *value)
{
    FrtHashKeyStatus ret_val = FRT_HASH_KEY_DOES_NOT_EXIST;
    FrtHashEntry *he;
    if (!frt_h_set_ext(self, key, &he)) {
        if (he->key != key) {
            self->free_key_i(he->key);
            if (he->value != value) {
                self->free_value_i(he->value);
            }
            ret_val = FRT_HASH_KEY_EQUAL;
        }
        else {
            /* Only free old value if it isn't the new value */
            if (he->value != value) {
                self->free_value_i(he->value);
            }
            ret_val = FRT_HASH_KEY_SAME;
        }
    }
    he->key = (void *)key;
    he->value = value;

    return ret_val;
}

int frt_h_set_safe(FrtHash *self, const void *key, void *value)
{
    FrtHashEntry *he;
    if (frt_h_set_ext(self, key, &he)) {
        he->key = (void *)key;
        he->value = value;
        return true;
    }
    else {
        return false;
    }
}

FrtHashKeyStatus frt_h_has_key(FrtHash *self, const void *key)
{
    FrtHashEntry *he = self->lookup_i(self, key);
    if (he->key == NULL || he->key == dummy_key) {
        return FRT_HASH_KEY_DOES_NOT_EXIST;
    }
    else if (he->key == key) {
        return FRT_HASH_KEY_SAME;
    }
    return FRT_HASH_KEY_EQUAL;
}

void *frt_h_get_int(FrtHash *self, const unsigned long long key)
{
    return frt_h_get(self, (const void *)key);
}

int frt_h_del_int(FrtHash *self, const unsigned long long key)
{
    return frt_h_del(self, (const void *)key);
}

void *frt_h_rem_int(FrtHash *self, const unsigned long long key)
{
    return frt_h_rem(self, (const void *)key, false);
}

FrtHashKeyStatus frt_h_set_int(FrtHash *self,
                               const unsigned long long key,
                               void *value)
{
    FrtHashKeyStatus ret_val = FRT_HASH_KEY_DOES_NOT_EXIST;
    FrtHashEntry *he;
    if (!frt_h_set_ext(self, (const void *)key, &he)) {
        /* Only free old value if it isn't the new value */
        if (he->value != value) {
            self->free_value_i(he->value);
        }
        ret_val = FRT_HASH_KEY_EQUAL;
    }
    he->key = (char *)dummy_int_key;
    he->value = value;

    return ret_val;
}

int frt_h_set_safe_int(FrtHash *self, const unsigned long long key, void *value)
{
    FrtHashEntry *he;
    if (frt_h_set_ext(self, (const void *)key, &he)) {
        he->key = (char *)dummy_int_key;
        he->value = value;
        return true;
    }
    return false;
}

int frt_h_has_key_int(FrtHash *self, const unsigned long long key)
{
    return frt_h_has_key(self, (const void *)key);
}

void frt_h_each(FrtHash *self,
            void (*each_kv) (void *key, void *value, void *arg), void *arg)
{
    FrtHashEntry *he;
    int i = self->size;
    for (he = self->table; i > 0; he++) {
        if (he->key && he->key != dummy_key) {        /* active entry */
            each_kv(he->key, he->value, arg);
            i--;
        }
    }
}

FrtHash *frt_h_clone(FrtHash *self, frt_h_clone_ft clone_key, frt_h_clone_ft clone_value)
{
    void *key, *value;
    FrtHashEntry *he;
    int i = self->size;
    FrtHash *ht_clone;

    ht_clone = frt_h_new(self->hash_i,
                     self->eq_i,
                     self->free_key_i,
                     self->free_value_i);

    for (he = self->table; i > 0; he++) {
        if (he->key && he->key != dummy_key) {        /* active entry */
            key = clone_key ? clone_key(he->key) : he->key;
            value = clone_value ? clone_value(he->value) : he->value;
            frt_h_set(ht_clone, key, value);
            i--;
        }
    }
    return ht_clone;
}

void frt_h_str_print_keys(FrtHash *self, FILE *out)
{
    FrtHashEntry *he;
    int i = self->size;
    char **keys = FRT_ALLOC_N(char *, self->size);
    for (he = self->table; i > 0; he++) {
        if (he->key && he->key != dummy_key) {        /* active entry */
            i--;
            keys[i] = (char *)he->key;
        }
    }
    frt_strsort(keys, self->size);
    fprintf(out, "keys:\n");
    for (i = 0; i < self->size; i++) {
        fprintf(out, "\t%s\n", keys[i]);
    }
    free(keys);
}

void frt_hash_finalize()
{
    while (num_free_hts > 0) {
        free(free_hts[--num_free_hts]);
    }
}
