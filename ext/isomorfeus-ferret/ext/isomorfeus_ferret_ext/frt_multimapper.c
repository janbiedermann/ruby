#include "frt_multimapper.h"
#include "frt_array.h"
#include "frt_bitvector.h"
#include <string.h>

#define St(state) ((FrtState *)(state))
#define UCtoI(val) ((int)(unsigned char)(val))

static void state_destroy(FrtState *state)
{
    state->destroy_i(state);
}

typedef struct LetterState
{
    FrtState super;
    int c;
    int val;
    char *mapping;
} LetterState;
#define LSt(state) ((LetterState *)(state))


static int lstate_next(LetterState *self, int c, int *states)
{
    if (c == self->c) {
        states[0] = self->val;
        return 1;
    }
    else {
        return 0;
    }
}

static int lstate_is_match(LetterState *self, char **mapping)
{
    if (self->val < 0) {
        *mapping = self->mapping;
        return self->val;
    }
    else {
        return 0;
    }
}

static LetterState *lstate_new(int c, int val)
{
    LetterState *self   = FRT_ALLOC(LetterState);
    self->c             = c;
    self->val           = val;
    self->mapping       = NULL;
    St(self)->next      = (int (*)(FrtState *, int, int *))&lstate_next;
    St(self)->destroy_i = (void (*)(FrtState *))&free;
    St(self)->is_match  = (int (*)(FrtState *, char **))&lstate_is_match;
    return self;
}

typedef struct NonDeterministicState
{
    FrtState super;
    int *states[256];
    int size[256];
    int capa[256];
} NonDeterministicState;

static int ndstate_next(NonDeterministicState *self, int c, int *states)
{
    int size = self->size[c];
    memcpy(states, self->states[c], size * sizeof(int));
    return size;
}

static void ndstate_add(NonDeterministicState *self, int c, int state)
{
    if (self->capa[c] <= self->size[c]) {
        if (self->capa[c] == 0) {
            self->capa[c] = 4;
        }
        else {
            self->capa[c] <<= 1;
        }
        FRT_REALLOC_N(self->states[c], int, self->capa[c]);
    }
    self->states[c][self->size[c]++] = state;
}

static void ndstate_destroy_i(NonDeterministicState *self)
{
    int i;
    for (i = 0; i < 256; i++) {
        free(self->states[i]);
    }
    free(self);
}

static int ndstate_is_match(FrtState *self, char **mapping)
{
    (void)self; (void)mapping;
    return 0;
}

static NonDeterministicState *ndstate_new(void) {
    NonDeterministicState *self = FRT_ALLOC_AND_ZERO(NonDeterministicState);
    St(self)->next              = (int (*)(FrtState *, int, int *))&ndstate_next;
    St(self)->destroy_i         = (void (*)(FrtState *))&ndstate_destroy_i;
    St(self)->is_match          = &ndstate_is_match;
    return self;
}

FrtMultiMapper *frt_mulmap_new(void) {
    FrtMultiMapper *self = FRT_ALLOC_AND_ZERO(FrtMultiMapper);
    self->capa = 128;
    self->mappings = FRT_ALLOC_N(FrtMapping *, 128);
    self->d_capa = 128;
    self->dstates = FRT_ALLOC_N(FrtDeterministicState *, 128);
    self->dstates_map = NULL;
    self->nstates = NULL;
    self->ref_cnt = 1;
    return self;
}

static void mulmap_free_dstates(FrtMultiMapper *self)
{
    if (self->d_size > 0) {
        int i;
        for (i = self->d_size - 1; i >= 0; i--) {
            free(self->dstates[i]);
        }
        self->d_size = 0;
    }
}

void frt_mulmap_add_mapping(FrtMultiMapper *self, const char *pattern, const char *rep)
{
    if (pattern == NULL || pattern[0] == '\0') {
        FRT_RAISE(FRT_ARG_ERROR, "Tried to add empty pattern to multi_mapper");
    } else {
        FrtMapping *mapping = FRT_ALLOC(FrtMapping);
        if (self->size >= self->capa) {
            self->capa <<= 1;
            FRT_REALLOC_N(self->mappings, FrtMapping *, self->capa);
        }
        mapping->pattern = frt_estrdup(pattern);
        mapping->replacement = frt_estrdup(rep);
        self->mappings[self->size++] = mapping;
        mulmap_free_dstates(self);
    }
}


static void mulmap_bv_set_states(FrtBitVector *bv, int *states, int cnt)
{
    int i;
    for (i = cnt - 1; i >= 0; i--) {
        frt_bv_set(bv, states[i]);
    }
}

static FrtDeterministicState *mulmap_process_state(FrtMultiMapper *self, FrtBitVector *bv)
{
    FrtDeterministicState *current_state
        = (FrtDeterministicState *)frt_h_get(self->dstates_map, bv);
    if (current_state == NULL) {
        int bit, i;
        int match_len = 0, max_match_len = 0;
        FrtState *start = self->nstates[0];
        FrtDeterministicState *start_ds;
        current_state = FRT_ALLOC_AND_ZERO(FrtDeterministicState);
        frt_h_set(self->dstates_map, bv, current_state);
        if (self->d_size >= self->d_capa) {
            self->d_capa <<= 1;
            FRT_REALLOC_N(self->dstates, FrtDeterministicState *, self->d_capa);
        }
        self->dstates[self->d_size++] = current_state;
        start_ds = self->dstates[0];
        for (i = 0; i < 256; i++) {
            current_state->next[i] = start_ds;
        }
        while ((bit = frt_bv_scan_next(bv)) >= 0) {
            char *mapping;
            FrtState *st = self->nstates[bit];
            if ((match_len = -st->is_match(st, &mapping)) > max_match_len) {
                current_state->longest_match = max_match_len = match_len;
                current_state->mapping = mapping;
                current_state->mapping_len = strlen(mapping);
            }
        }
        for (i = self->a_size - 1; i >= 0; i--) {
            unsigned char c = self->alphabet[i];
            FrtBitVector *nxt_bv = frt_bv_new_capa(self->nsize);
            mulmap_bv_set_states(nxt_bv, self->next_states,
                                 start->next(start, (int)c, self->next_states));
            frt_bv_scan_reset(bv);
            while ((bit = frt_bv_scan_next(bv)) >= 0) {
                FrtState *state = self->nstates[bit];
                mulmap_bv_set_states(nxt_bv, self->next_states,
                                     state->next(state, (int)c, self->next_states));
            }
            current_state->next[(int)c] = mulmap_process_state(self, nxt_bv);
        }
    }
    else {
        frt_bv_destroy(bv);
    }
    return current_state;
}

void frt_mulmap_compile(FrtMultiMapper *self)
{
    NonDeterministicState *start = ndstate_new();
    int i, j;
    int size = 1;
    int capa = 128;
    LetterState *ls;
    FrtState **nstates = FRT_ALLOC_N(FrtState *, capa);
    FrtMapping **mappings = self->mappings;
    unsigned char alphabet[256];
    nstates[0] = (FrtState *)start;
    memset(alphabet, 0, 256);

    for (i = self->size - 1; i >= 0; i--) {
        const char *pattern = mappings[i]->pattern;
        const int plen = (int)strlen(pattern);
        ndstate_add(start, UCtoI(pattern[0]), size);
        if (size + plen + 1 >= capa) {
            capa <<= 2;
            FRT_REALLOC_N(nstates, FrtState *, capa);
        }
        for (j = 0; j < plen; j++) {
            alphabet[UCtoI(pattern[j])] = 1;
            size += 1;
            nstates[size-1] = (FrtState *)lstate_new(UCtoI(pattern[j+1]), size);
        }
        ls = LSt(nstates[size-1]);
        ls->mapping = mappings[i]->replacement;
        ls->val = -plen;
        ls->c = -1;
    }
    for (i = j = 0; i < 256; i++) {
        if (alphabet[i]) self->alphabet[j++] = i;
    }
    self->a_size = j;
    mulmap_free_dstates(self);
    self->nstates = nstates;
    self->nsize = size;
    self->next_states = FRT_ALLOC_N(int, size);
    self->dstates_map = frt_h_new((frt_hash_ft)&frt_bv_hash, (frt_eq_ft)&frt_bv_eq,
                              (frt_free_ft)&frt_bv_destroy, (frt_free_ft)NULL);
    mulmap_process_state(self, frt_bv_new_capa(0));
    frt_h_destroy(self->dstates_map);
    for (i = size - 1; i >= 0; i--) {
        state_destroy(nstates[i]);
    }
    free(self->next_states);
    free(nstates);
}

int frt_mulmap_map_len(FrtMultiMapper *self, char *to, char *from, int capa)
{
    FrtDeterministicState *start = self->dstates[0];
    FrtDeterministicState *state = start;
    char *s = from, *d = to, *end = to + capa - 1;
    if (self->d_size == 0) {
        frt_mulmap_compile(self);
    }
    while (*s && d < end) {
        state = state->next[UCtoI(*s)];
        if (state->mapping) {
            int len = state->mapping_len;
            d -= (state->longest_match - 1);
            if ((d + len) > end) {
                len = end - d;
            }
            memcpy(d, state->mapping, len);
            d += len;
            state = start;
        }
        else {
            *(d++) = *s;
        }
        s++;
    }
    *d = '\0';
    return d - to;
}

char *frt_mulmap_map(FrtMultiMapper *self, char *to, char *from, int capa)
{
    frt_mulmap_map_len(self, to, from, capa);
    return to;
}

/* Maps a string to a dynamically allocated string */
char *frt_mulmap_dynamic_map(FrtMultiMapper *self, char *from)
{
    FrtDeterministicState *start = self->dstates[0];
    FrtDeterministicState *state = start;
    int capa = strlen(from);
    char *to = (char *)frt_ecalloc(capa);
    char *s = from, *d = to, *end = to + capa - 1;
    if (self->d_size == 0) {
        frt_mulmap_compile(self);
    }
    do {
        while (*s && d < end) {
            state = state->next[UCtoI(*s)];
            if (state->mapping) {
                int len = state->mapping_len;
                d -= (state->longest_match - 1);
                if ((d + len) > end) {
                    len = end - d;
                }
                memcpy(d, state->mapping, len);
                d += len;
                state = start;
            }
            else {
                *(d++) = *s;
            }
            s++;
        }
        if (*s) {
            capa += 1024;
            frt_erealloc(to, capa);
            end = to + capa - 1;
        }
    } while(*s);
    *d = '\0';
    return to;
}

void frt_mulmap_destroy(FrtMultiMapper *self)
{
    if (--(self->ref_cnt) <= 0) {
        int i;
        mulmap_free_dstates(self);
        for (i = self->size - 1; i >= 0; i--) {
            FrtMapping *mapping = self->mappings[i];
            free(mapping->pattern);
            free(mapping->replacement);
            free(mapping);
        }
        free(self->mappings);
        free(self->dstates);
        free(self);
    }
}
