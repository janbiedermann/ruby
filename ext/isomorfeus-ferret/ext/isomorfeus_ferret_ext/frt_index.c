#include "frt_global.h"
#include "frt_index.h"
#include "frt_similarity.h"
#include "frt_helper.h"
#include "frt_array.h"
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "brotli_decode.h"
#include "brotli_encode.h"

extern void frt_micro_sleep(const int micro_seconds);

#define GET_LOCK(lock, name, store, err_msg) do {\
    lock = store->frt_open_lock(store, name);\
    if (!lock->obtain(lock)) {\
        FRT_RAISE(FRT_LOCK_ERROR, err_msg);\
    }\
} while(0)

#define RELEASE_LOCK(lock, store) do {\
    lock->release(lock);\
    store->close_lock(lock);\
} while (0)

const FrtConfig frt_default_config = {
    0x100000,       /* chunk size is 1Mb */
    0x1000000,      /* Max memory used for buffer is 16 Mb */
    FRT_INDEX_INTERVAL, /* index interval */
    FRT_SKIP_INTERVAL,  /* skip interval */
    10,             /* default merge factor */
    10000,          /* max_buffered_docs */
    INT_MAX,        /* max_merge_docs */
    10000,          /* maximum field length (number of terms) */
    true            /* use compound file by default */
};

static void ste_reset(FrtTermEnum *te);
static char *ste_next(FrtTermEnum *te);

#define FORMAT 0
#define SEGMENTS_GEN_FILE_NAME "segments"
#define MAX_EXT_LEN 10
#define COMPRESSION_BUFFER_SIZE 16348
#define COMPRESSION_LEVEL 9

/* *** Must be three characters *** */
static const char *INDEX_EXTENSIONS[] = {
    "frq", "prx", "fdx", "fdt", "tfx", "tix", "tis", "del", "gen", "cfs"
};

/* *** Must be three characters *** */
static const char *COMPOUND_EXTENSIONS[] = {
    "frq", "prx", "fdx", "fdt", "tfx", "tix", "tis"
};

static const char BASE36_DIGITMAP[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static char *u64_to_str36(char *buf, int buf_size, frt_u64 u)
{
    int i = buf_size - 1;
    buf[i] = '\0';
    for (i--; i >= 0; i--) {
        buf[i] = BASE36_DIGITMAP[u % 36];
        u /= 36;
        if (0 == u) {
            break;
        }
    }
    if (0 < u) {
        FRT_RAISE(FRT_INDEX_ERROR, "Max length of segment filename has been reached. "
              "Perhaps it's time to re-index.\n");
    }
    return buf + i;
}

static frt_u64 str36_to_u64(char *p)
{
    frt_u64 u = 0;
    while (true) {
        if ('0' <= *p && '9' >= *p) {
            u = u * 36 + *p - '0';
        }
        else if ('a' <= *p && 'z' >= *p) {
            u = u * 36 + *p - 'a' + 10;
        }
        else {
            break;
        }
        p++;
    }
    return u;
}

/*
 * Computes the full file name from base, extension and generation.  If the
 * generation is -1, the file name is NULL.  If it's 0, the file name is
 * <base><extension>.  If it's > 0, the file name is
 * <base>_<generation><extension>.
 *
 * @param buf buffer to write filename to
 * @param base main part of the file name
 * @param ext extension of the filename (including .)
 * @param gen generation
 */
char *frt_fn_for_generation(char *buf,
                        const char *base,
                        const char *ext,
                        frt_i64 gen)
{
    if (-1 == gen) {
        return NULL;
    }
    else {
        char b[FRT_SEGMENT_NAME_MAX_LENGTH];
        char *u = u64_to_str36(b, FRT_SEGMENT_NAME_MAX_LENGTH, (frt_u64)gen);
        if (ext == NULL) {
            sprintf(buf, "%s_%s", base, u);
        }
        else {
            sprintf(buf, "%s_%s.%s", base, u, ext);
        }
        return buf;
    }
}

static char *segfn_for_generation(char *buf, frt_u64 generation)
{
    char b[FRT_SEGMENT_NAME_MAX_LENGTH];
    char *u = u64_to_str36(b, FRT_SEGMENT_NAME_MAX_LENGTH, generation);
    sprintf(buf, FRT_SEGMENTS_FILE_NAME"_%s", u);
    return buf;
}

/*
 * Computes the field specific file name from base, extension, generation and
 * field number.  If the generation is -1, the file name is NULL.  If it's 0,
 * the file name is <base><extension>.  If it's > 0, the file name is
 * <base>_<generation><extension>.
 *
 * @param buf buffer to write filename to
 * @param base main part of the file name
 * @param ext extension of the filename (including .)
 * @param gen generation
 * @param field_num field number
 */
static char *fn_for_gen_field(char *buf,
                              const char *base,
                              const char *ext,
                              frt_i64 gen,
                              int field_num)
{
    if (-1 == gen) {
        return NULL;
    }
    else {
        char b[FRT_SEGMENT_NAME_MAX_LENGTH];
        sprintf(buf, "%s_%s.%s%d",
                base,
                u64_to_str36(b, FRT_SEGMENT_NAME_MAX_LENGTH, (frt_u64)gen),
                ext,
                field_num);
        return buf;
    }
}

/***************************************************************************
 *
 * CacheObject
 *
 ***************************************************************************/

static unsigned long long co_hash(const void *key)
{
    return (unsigned long long)key;
}

static int co_eq(const void *key1, const void *key2)
{
    return (key1 == key2);
}

static void co_destroy(FrtCacheObject *self)
{
    frt_h_rem(self->ref_tab1, self->ref2, false);
    frt_h_rem(self->ref_tab2, self->ref1, false);
    self->destroy(self->obj);
    free(self);
}

FrtCacheObject *frt_co_create(FrtHash *ref_tab1, FrtHash *ref_tab2,
                       void *ref1, void *ref2, frt_free_ft destroy, void *obj)
{
    FrtCacheObject *self = FRT_ALLOC(FrtCacheObject);
    frt_h_set(ref_tab1, ref2, self);
    frt_h_set(ref_tab2, ref1, self);
    self->ref_tab1 = ref_tab1;
    self->ref_tab2 = ref_tab2;
    self->ref1 = ref1;
    self->ref2 = ref2;
    self->destroy = destroy;
    self->obj = obj;
    return self;
}

FrtHash *frt_co_hash_create()
{
    return frt_h_new(&co_hash, &co_eq, (frt_free_ft)NULL, (frt_free_ft)&co_destroy);
}

/****************************************************************************
 *
 * FieldInfo
 *
 ****************************************************************************/

static void fi_set_store(FrtFieldInfo *fi, int store)
{
    switch (store) {
        case FRT_STORE_NO:
            break;
        case FRT_STORE_YES:
            fi->bits |= FRT_FI_IS_STORED_BM;
            break;
        case FRT_STORE_COMPRESS:
            fi->bits |= FRT_FI_IS_COMPRESSED_BM | FRT_FI_IS_STORED_BM;
            break;
    }
}

static void fi_set_index(FrtFieldInfo *fi, int index)
{
    switch (index) {
        case FRT_INDEX_NO:
            break;
        case FRT_INDEX_YES:
            fi->bits |= FRT_FI_IS_INDEXED_BM | FRT_FI_IS_TOKENIZED_BM;
            break;
        case FRT_INDEX_UNTOKENIZED:
            fi->bits |= FRT_FI_IS_INDEXED_BM;
            break;
        case FRT_INDEX_YES_OMIT_NORMS:
            fi->bits |= FRT_FI_OMIT_NORMS_BM | FRT_FI_IS_INDEXED_BM |
                FRT_FI_IS_TOKENIZED_BM;
            break;
        case FRT_INDEX_UNTOKENIZED_OMIT_NORMS:
            fi->bits |= FRT_FI_OMIT_NORMS_BM | FRT_FI_IS_INDEXED_BM;
            break;
    }
}

static void fi_set_term_vector(FrtFieldInfo *fi, int term_vector)
{
    switch (term_vector) {
        case FRT_TERM_VECTOR_NO:
            break;
        case FRT_TERM_VECTOR_YES:
            fi->bits |= FRT_FI_STORE_TERM_VECTOR_BM;
            break;
        case FRT_TERM_VECTOR_WITH_POSITIONS:
            fi->bits |= FRT_FI_STORE_TERM_VECTOR_BM | FRT_FI_STORE_POSITIONS_BM;
            break;
        case FRT_TERM_VECTOR_WITH_OFFSETS:
            fi->bits |= FRT_FI_STORE_TERM_VECTOR_BM | FRT_FI_STORE_OFFSETS_BM;
            break;
        case FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS:
            fi->bits |= FRT_FI_STORE_TERM_VECTOR_BM | FRT_FI_STORE_POSITIONS_BM |
                FRT_FI_STORE_OFFSETS_BM;
            break;
    }
}

static void fi_check_params(int store, int index, int term_vector)
{
    (void)store;
    if ((index == FRT_INDEX_NO) && (term_vector != FRT_TERM_VECTOR_NO)) {
        FRT_RAISE(FRT_ARG_ERROR,
              "You can't store the term vectors of an unindexed field");
    }
}

FrtFieldInfo *frt_fi_new(FrtSymbol name,
                  FrtStoreValue store,
                  FrtIndexValue index,
                  FrtTermVectorValue term_vector)
{
    FrtFieldInfo *fi = FRT_ALLOC(FrtFieldInfo);
    assert(NULL != name);
    fi_check_params(store, index, term_vector);
    fi->name = name;
    fi->boost = 1.0f;
    fi->bits = 0;
    fi_set_store(fi, store);
    fi_set_index(fi, index);
    fi_set_term_vector(fi, term_vector);
    fi->ref_cnt = 1;
    return fi;
}

void frt_fi_deref(FrtFieldInfo *fi)
{
    if (0 == --(fi->ref_cnt)) {
        free(fi);
    }
}

char *frt_fi_to_s(FrtFieldInfo *fi)
{
    const char *fi_name = rb_id2name(fi->name);
    char *str = FRT_ALLOC_N(char, strlen(fi_name) + 200);
    char *s = str;
    s += sprintf(str, "[\"%s\":(%s%s%s%s%s%s%s%s", fi_name,
                 fi_is_stored(fi) ? "is_stored, " : "",
                 fi_is_compressed(fi) ? "is_compressed, " : "",
                 fi_is_indexed(fi) ? "is_indexed, " : "",
                 fi_is_tokenized(fi) ? "is_tokenized, " : "",
                 fi_omit_norms(fi) ? "omit_norms, " : "",
                 fi_store_term_vector(fi) ? "store_term_vector, " : "",
                 fi_store_positions(fi) ? "store_positions, " : "",
                 fi_store_offsets(fi) ? "store_offsets, " : "");
    s -= 2;
    if (*s != ',') {
        s += 2;
    }

    sprintf(s, ")]");
    return str;
}

/****************************************************************************
 *
 * FieldInfos
 *
 ****************************************************************************/

FrtFieldInfos *frt_fis_new(FrtStoreValue store, FrtIndexValue index,
                    FrtTermVectorValue term_vector)
{
    FrtFieldInfos *fis = FRT_ALLOC(FrtFieldInfos);
    fi_check_params(store, index, term_vector);
    fis->field_dict = frt_h_new_ptr((frt_free_ft)&frt_fi_deref);
    fis->size = 0;
    fis->capa = FIELD_INFOS_INIT_CAPA;
    fis->fields = FRT_ALLOC_N(FrtFieldInfo *, fis->capa);
    fis->store = store;
    fis->index = index;
    fis->term_vector = term_vector;
    fis->ref_cnt = 1;
    return fis;
}

FrtFieldInfo *frt_fis_add_field(FrtFieldInfos *fis, FrtFieldInfo *fi)
{
    if (fis->size == fis->capa) {
        fis->capa <<= 1;
        FRT_REALLOC_N(fis->fields, FrtFieldInfo *, fis->capa);
    }
    if (!frt_h_set_safe(fis->field_dict, (void *)fi->name, fi)) {
        FRT_RAISE(FRT_ARG_ERROR, "Field :%s already exists", rb_id2name(fi->name));
    }
    fi->number = fis->size;
    fis->fields[fis->size] = fi;
    fis->size++;
    return fi;
}

FrtFieldInfo *frt_fis_get_field(FrtFieldInfos *fis, FrtSymbol name)
{
    return (FrtFieldInfo *)frt_h_get(fis->field_dict, (void *)name);
}

int frt_fis_get_field_num(FrtFieldInfos *fis, FrtSymbol name)
{
    FrtFieldInfo *fi = (FrtFieldInfo *)frt_h_get(fis->field_dict, (void *)name);
    if (fi) { return fi->number; }
    else { return -1; }
}

FrtFieldInfo *frt_fis_get_or_add_field(FrtFieldInfos *fis, FrtSymbol name)
{
    FrtFieldInfo *fi = (FrtFieldInfo *)frt_h_get(fis->field_dict, (void *)name);
    if (!fi) {
        fi = (FrtFieldInfo*)frt_fi_new(name, fis->store, fis->index, fis->term_vector);
        frt_fis_add_field(fis, fi);
    }
    return fi;
}

FrtFieldInfos *frt_fis_read(FrtInStream *is)
{
    FrtFieldInfos *volatile fis = NULL;
    char *field_name;
    FRT_TRY
        do {
            FrtStoreValue store_val;
            FrtIndexValue index_val;
            FrtTermVectorValue term_vector_val;
            volatile int i;
            union { frt_u32 i; float f; } tmp;
            FrtFieldInfo *volatile fi;
            store_val = (FrtStoreValue)frt_is_read_vint(is);
            index_val = (FrtIndexValue)frt_is_read_vint(is);
            term_vector_val = (FrtTermVectorValue)frt_is_read_vint(is);
            fis = frt_fis_new(store_val, index_val, term_vector_val);
            for (i = frt_is_read_vint(is); i > 0; i--) {
                fi = FRT_ALLOC_AND_ZERO(FrtFieldInfo);
                FRT_TRY
                    field_name = frt_is_read_string_safe(is);
                    fi->name = rb_intern(field_name);
                    free(field_name);
                    tmp.i = frt_is_read_u32(is);
                    fi->boost = tmp.f;
                    fi->bits = frt_is_read_vint(is);
                FRT_XCATCHALL
                    free(fi);
                FRT_XENDTRY
                frt_fis_add_field(fis, fi);
                fi->ref_cnt = 1;
            }
        } while (0);
    FRT_XCATCHALL
        frt_fis_deref(fis);
    FRT_XENDTRY
    return fis;
}

void frt_fis_write(FrtFieldInfos *fis, FrtOutStream *os)
{
    int i;
    union { frt_u32 i; float f; } tmp;
    FrtFieldInfo *fi;
    const int fis_size = fis->size;

    frt_os_write_vint(os, fis->store);
    frt_os_write_vint(os, fis->index);
    frt_os_write_vint(os, fis->term_vector);
    frt_os_write_vint(os, fis->size);

    for (i = 0; i < fis_size; i++) {
        fi = fis->fields[i];

        frt_os_write_string(os, rb_id2name(fi->name));
        tmp.f = fi->boost;
        frt_os_write_u32(os, tmp.i);
        frt_os_write_vint(os, fi->bits);
    }
}

static const char *store_str[] = {
    ":no",
    ":yes",
    "",
    ":compressed"
};

static const char *fi_store_str(FrtFieldInfo *fi)
{
    return store_str[fi->bits & 0x3];
}

static const char *index_str[] = {
    ":no",
    ":untokenized",
    "",
    ":yes",
    "",
    ":untokenized_omit_norms",
    "",
    ":omit_norms"
};

static const char *fi_index_str(FrtFieldInfo *fi)
{
    return index_str[(fi->bits >> 2) & 0x7];
}

static const char *term_vector_str[] = {
    ":no",
    ":yes",
    "",
    ":with_positions",
    "",
    ":with_offsets",
    "",
    ":with_positions_offsets"
};

static const char *fi_term_vector_str(FrtFieldInfo *fi)
{
    return term_vector_str[(fi->bits >> 5) & 0x7];
}

char *frt_fis_to_s(FrtFieldInfos *fis)
{
    int i, pos, capa = 200 + fis->size * 120;
    char *buf = FRT_ALLOC_N(char, capa);
    FrtFieldInfo *fi;
    const int fis_size = fis->size;

    pos = sprintf(buf,
                  "default:\n"
                  "  store: %s\n"
                  "  index: %s\n"
                  "  term_vector: %s\n"
                  "fields:\n",
                  store_str[fis->store],
                  index_str[fis->index],
                  term_vector_str[fis->term_vector]);
    for (i = 0; i < fis_size; i++) {
        fi = fis->fields[i];
        pos += sprintf(buf + pos,
                       "  %s:\n"
                       "    boost: %f\n"
                       "    store: %s\n"
                       "    index: %s\n"
                       "    term_vector: %s\n",
                       rb_id2name(fi->name), fi->boost, fi_store_str(fi),
                       fi_index_str(fi), fi_term_vector_str(fi));
    }

    return buf;
}

void frt_fis_deref(FrtFieldInfos *fis)
{
    if (0 == --(fis->ref_cnt)) {
        frt_h_destroy(fis->field_dict);
        free(fis->fields);
        free(fis);
    }
}

static bool fis_has_vectors(FrtFieldInfos *fis)
{
    int i;
    const int fis_size = fis->size;

    for (i = 0; i < fis_size; i++) {
        if (fi_store_term_vector(fis->fields[i])) {
            return true;
        }
    }
    return false;
}

/****************************************************************************
 *
 * SegmentInfo
 *
 ****************************************************************************/

FrtSegmentInfo *frt_si_new(char *name, int doc_cnt, FrtStore *store)
{
    FrtSegmentInfo *si = FRT_ALLOC(FrtSegmentInfo);
    si->name = name;
    si->doc_cnt = doc_cnt;
    si->store = store;
    si->del_gen = -1;
    si->norm_gens = NULL;
    si->norm_gens_size = 0;
    si->ref_cnt = 1;
    si->use_compound_file = false;
    return si;
}

static FrtSegmentInfo *si_read(FrtStore *store, FrtInStream *is)
{
    FrtSegmentInfo *volatile si = FRT_ALLOC_AND_ZERO(FrtSegmentInfo);
    FRT_TRY
        si->store = store;
        si->name = frt_is_read_string_safe(is);
        si->doc_cnt = frt_is_read_vint(is);
        si->del_gen = frt_is_read_vint(is);
        si->norm_gens_size = frt_is_read_vint(is);
        si->ref_cnt = 1;
        if (0 < si->norm_gens_size) {
            int i;
            si->norm_gens = FRT_ALLOC_N(int, si->norm_gens_size);
            for (i = si->norm_gens_size - 1; i >= 0; i--) {
                si->norm_gens[i] = frt_is_read_vint(is);
            }
        }
        si->use_compound_file = (bool)frt_is_read_byte(is);
    FRT_XCATCHALL
        free(si->name);
        free(si);
    FRT_XENDTRY
    return si;
}

static void si_write(FrtSegmentInfo *si, FrtOutStream *os)
{
    frt_os_write_string(os, si->name);
    frt_os_write_vint(os, si->doc_cnt);
    frt_os_write_vint(os, si->del_gen);
    frt_os_write_vint(os, si->norm_gens_size);
    if (0 < si->norm_gens_size) {
        int i;
        for (i = si->norm_gens_size - 1; i >= 0; i--) {
            frt_os_write_vint(os, si->norm_gens[i]);
        }
    }
    frt_os_write_byte(os, (frt_uchar)si->use_compound_file);
}

void frt_si_deref(FrtSegmentInfo *si)
{
    if (--si->ref_cnt <= 0) {
        free(si->name);
        free(si->norm_gens);
        free(si);
    }
}

bool frt_si_has_deletions(FrtSegmentInfo *si)
{
    return si->del_gen >= 0;
}

bool frt_si_has_separate_norms(FrtSegmentInfo *si)
{
    if (si->use_compound_file && si->norm_gens) {
        int i;
        for (i = si->norm_gens_size - 1; i >= 0; i--) {
            if (si->norm_gens[i] > 0) return true;
        }
    }
    return false;
}

void frt_si_advance_norm_gen(FrtSegmentInfo *si, int field_num)
{
    if (field_num >= si->norm_gens_size) {
        int i;
        FRT_REALLOC_N(si->norm_gens, int, field_num + 1);
        for (i = si->norm_gens_size; i <= field_num; i++) {
            si->norm_gens[i] = -1;
        }
        si->norm_gens_size = field_num + 1;
    }
    si->norm_gens[field_num]++;
}

static char *si_norm_file_name(FrtSegmentInfo *si, char *buf, int field_num)
{
    int norm_gen;
    if (field_num >= si->norm_gens_size
        || 0 > (norm_gen = si->norm_gens[field_num])) {
        return NULL;
    }
    else {
        const char *ext = (si->use_compound_file && norm_gen > 0) ? "s" : "f";
        return fn_for_gen_field(buf, si->name, ext, norm_gen, field_num);
    }
}

static void deleter_queue_file(FrtDeleter *dlr, const char *file_name);
#define DEL(file_name) deleter_queue_file(dlr, file_name)

static void si_delete_files(FrtSegmentInfo *si, FrtFieldInfos *fis, FrtDeleter *dlr)
{
    int i;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    size_t seg_len = strlen(si->name);
    char *ext;

    for (i = si->norm_gens_size - 1; i >= 0; i--) {
        if (0 <= si->norm_gens[i]) {
            DEL(si_norm_file_name(si, file_name, fis->fields[i]->number));
        }
    }

    memcpy(file_name, si->name, seg_len);
    file_name[seg_len] = '.';
    ext = file_name + seg_len + 1;

    if (si->use_compound_file) {
        memcpy(ext, "cfs", 4);
        DEL(file_name);
        if (0 <= si->del_gen) {
            DEL(frt_fn_for_generation(file_name, si->name, "del", si->del_gen));
        }
    }
    else {
        for (i = FRT_NELEMS(INDEX_EXTENSIONS) - 1; i >= 0; i--) {
            memcpy(ext, INDEX_EXTENSIONS[i], 4);
            DEL(file_name);
        }
    }
}

/****************************************************************************
 *
 * SegmentInfos
 *
 ****************************************************************************/

#include <time.h>
static char *new_segment(frt_i64 generation)
{
    char buf[FRT_SEGMENT_NAME_MAX_LENGTH];
    char *fn_p = u64_to_str36(buf, FRT_SEGMENT_NAME_MAX_LENGTH - 1,
                              (frt_u64)generation);
    *(--fn_p) = '_';
    return frt_estrdup(fn_p);
}

/****************************************************************************
 * FindSegmentsFile
 ****************************************************************************/

typedef struct FindSegmentsFile {
    frt_i64  generation;
    union {
      FrtSegmentInfos *sis;
      FrtIndexReader  *ir;
      frt_u64           uint64;
    } ret;
} FindSegmentsFile;

static void which_gen_i(const char *file_name, void *arg)
{
    frt_i64 *max_generation = (frt_i64 *)arg;
    if (0 == strncmp(FRT_SEGMENTS_FILE_NAME"_", file_name,
                     sizeof(FRT_SEGMENTS_FILE_NAME))) {
        char *p = strrchr(file_name, '_') + 1;
        frt_i64 generation = (frt_i64)str36_to_u64(p);
        if (generation > *max_generation) *max_generation = generation;
    }
}

static void si_put(FrtSegmentInfo *si, FILE *stream)
{
    int i;
    fprintf(stream, "\tSegmentInfo {\n");
    fprintf(stream, "\t\tname = %s\n", si->name);
    fprintf(stream, "\t\tdoc_cnt = %d\n", si->doc_cnt);
    fprintf(stream, "\t\tdel_gen = %d\n", si->del_gen);
    fprintf(stream, "\t\tnorm_gens_size = %d\n", si->norm_gens_size);
    fprintf(stream, "\t\tnorm_gens {\n");
    for (i = 0; i < si->norm_gens_size; i++) {
        fprintf(stream, "\t\t\t%d\n", si->norm_gens[i]);
    }
    fprintf(stream, "\t\t}\n");
    fprintf(stream, "\t\tref_cnt = %d\n", si->ref_cnt);
    fprintf(stream, "\t}\n");
}

void frt_sis_put(FrtSegmentInfos *sis, FILE *stream)
{
    int i;
    fprintf(stream, "SegmentInfos {\n");
    fprintf(stream, "\tcounter = %"POSH_I64_PRINTF_PREFIX"d\n", sis->counter);
    fprintf(stream, "\tversion = %"POSH_I64_PRINTF_PREFIX"d\n", sis->version);
    fprintf(stream, "\tgeneration = %"POSH_I64_PRINTF_PREFIX"d\n", sis->generation);
    fprintf(stream, "\tformat = %d\n", sis->format);
    fprintf(stream, "\tsize = %d\n", sis->size);
    fprintf(stream, "\tcapa = %d\n", sis->capa);
    for (i = 0; i < sis->size; i++) {
        si_put(sis->segs[i], stream);
    }
    fprintf(stream, "}\n");
}

/*
 * Get the generation (N) of the current segments_N file from a list of files.
 *
 * @param store - the Store to look in
 */
frt_i64 frt_sis_current_segment_generation(FrtStore *store)
{
    frt_i64 current_generation = -1;
    store->each(store, &which_gen_i, &current_generation);
    return current_generation;
}

/*
 * Get the current generation filename.
 *
 * @param buf - buffer to write filename to
 * @param store - the Store to look in
 * @return segments_N where N is the current generation
 */
char *frt_sis_curr_seg_file_name(char *buf, FrtStore *store)
{
    return segfn_for_generation(buf, frt_sis_current_segment_generation(store));
}

/*
 * Get the next generation filename.
 *
 * @param buf - buffer to write filename to
 * @param store - the Store to look in
 * @return segments_N where N is the +next+ generation
 */
/*
FIXME: not used
static char *sis_next_seg_file_name(char *buf, FrtStore *store)
{
    return segfn_for_generation(buf, frt_sis_current_segment_generation(store) + 1);
}
*/

#define GEN_FILE_RETRY_COUNT 10
#define GEN_LOOK_AHEAD_COUNT 10
static void sis_find_segments_file(FrtStore *store, FindSegmentsFile *fsf,
                            void (*run)(FrtStore *store, FindSegmentsFile *fsf))
{
    volatile int i;
    volatile int gen_look_ahead_count = 0;
    volatile bool retry = false;
    volatile int method = 0;
    volatile frt_i64 last_gen = -1;
    volatile frt_i64 gen = 0;

    /* Loop until we succeed in calling doBody() without hitting an
     * IOException. An IOException most likely means a commit was in process
     * and has finished, in the time it took us to load the now-old infos
     * files (and segments files). It's also possible it's a true error
     * (corrupt index). To distinguish these, on each retry we must see
     * "forward progress" on which generation we are trying to load. If we
     * don't, then the original error is real and we throw it.
     *
     * We have three methods for determining the current generation.  We try
     * each in sequence. */
    while (true) {
        /* Method 1: list the directory and use the highest segments_N file.
         * This method works well as long as there is no stale caching on the
         * directory contents: */
        if (0 == method) {
            gen = frt_sis_current_segment_generation(store);
            if (gen == -1) {
                // fprintf(stderr, ">>\n%s\n>>\n", frt_store_to_s(store));
                FRT_RAISE(FRT_FILE_NOT_FOUND_ERROR, "couldn't find segments file");
            }
        }

        /* Method 2 (fallback if Method 1 isn't reliable): if the directory
         * listing seems to be stale, try loading the "segments" file. */
        if (1 == method || (0 == method && last_gen == gen && retry)) {
            method = 1;
            for (i = 0; i < GEN_FILE_RETRY_COUNT; i++) {
                FrtInStream *gen_is;
                gen_is = NULL;
                FRT_TRY
                    gen_is = store->open_input(store, SEGMENTS_GEN_FILE_NAME);
                FRT_XCATCHALL
                    FRT_HANDLED();
                    /* TODO:LOG "segments open: FRT_IO_ERROR"*/
                FRT_XENDTRY

                if (NULL != gen_is) {
                    volatile frt_i64 gen0 = -1, gen1 = -1;

                    FRT_TRY
                        gen0 = frt_is_read_u64(gen_is);
                        gen1 = frt_is_read_u64(gen_is);
                    FRT_XFINALLY
                        /* if there is an error well simply try again */
                        FRT_HANDLED();
                        frt_is_close(gen_is);
                    FRT_XENDTRY
                    /* TODO:LOG "fallback check: " + gen0 + "; " + gen1 */
                    if (gen0 == gen1 && gen0 > gen) {
                        /* The file is consistent. */
                        gen = gen0;
                    }
                    break;
                }
                /* sleep for 50 milliseconds */
                frt_micro_sleep(50000);
            }
        }

        /* Method 3 (fallback if Methods 2 & 3 are not reliable): since both
         * directory cache and file contents cache seem to be stale, just
         * advance the generation. */
        if (2 == method || (1 == method && last_gen == gen && retry)) {
            method = 2;
            if (gen_look_ahead_count < GEN_LOOK_AHEAD_COUNT) {
                gen++;
                gen_look_ahead_count++;
                /* TODO:LOG "look ahead increment gen to " + gen */
            }
        }

        if (last_gen == gen) {
            /* This means we're about to try the same segments_N last tried.
             * This is allowed, exactly once, because writer could have been
             * in the process of writing segments_N last time. */
            if (retry) {
                /* OK, we've tried the same segments_N file twice in a row, so
                 * this must be a real error.  We throw the original exception
                 * we got. */
                char *listing, listing_buffer[1024];
                listing = frt_store_to_s(store);
                strncpy(listing_buffer, listing, 1023);
                listing_buffer[1023] = '\0';
                free(listing);
                FRT_RAISE(FRT_IO_ERROR,
                      "Error reading the segment infos. Store:\n %s\n",
                      listing_buffer);
            } else {
                frt_micro_sleep(50000);
                retry = true;
            }
        } else {
            /* Segment file has advanced since our last loop, so reset retry: */
            retry = false;
        }
        last_gen = gen;
        FRT_TRY
            fsf->generation = gen;
            run(store, fsf);
            FRT_RETURN_EARLY();
            return;
        case FRT_IO_ERROR: case FRT_FILE_NOT_FOUND_ERROR: case FRT_EOF_ERROR:
            FRT_HANDLED();
            /*
            if (gen != frt_sis_current_segment_generation(store)) {
                fprintf(stderr, "%lld != %lld\n",
                        gen, frt_sis_current_segment_generation(store));
                fprintf(stderr, "%s\n", xcontext.msg);
            }
            else {
                char *sl = frt_store_to_s(store);
                bool done = false;
                fprintf(stderr, "%s\n>>>\n%s", xcontext.msg, sl);
                free(sl);
                while (!done) {
                    FRT_TRY
                        frt_sis_put(frt_sis_read(store), stderr);
                        done = true;
                    FRT_XCATCHALL
                        FRT_HANDLED();
                    FRT_XENDTRY
                }
            }

            char *sl = frt_store_to_s(store);
            fprintf(stderr, "%s\n>>>\n%s", xcontext.msg, sl);
            free(sl);
            */

            /* Save the original root cause: */
            /* TODO:LOG "primary Exception on '" + segmentFileName + "': " +
             * err + "'; will retry: retry=" + retry + "; gen = " + gen */
            if (!retry && gen > 1) {
                /* This is our first time trying this segments file (because
                 * retry is false), and, there is possibly a segments_(N-1)
                 * (because gen > 1).  So, check if the segments_(N-1) exists
                 * and try it if so: */
                char prev_seg_file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
                segfn_for_generation(prev_seg_file_name, gen - 1);
                if (store->exists(store, prev_seg_file_name)) {
                    /* TODO:LOG "fallback to prior segment file '" +
                     * prevSegmentFileName + "'" */
                    FRT_TRY
                        fsf->generation = gen - 1;
                        run(store, fsf);
                        /* TODO:LOG "success on fallback " +
                         * prev_seg_file_name */

                        /* pop two contexts as we are in nested try blocks */
                        FRT_RETURN_EARLY();
                        FRT_RETURN_EARLY();
                        return;
                    case FRT_IO_ERROR: case FRT_FILE_NOT_FOUND_ERROR: case FRT_EOF_ERROR:
                        FRT_HANDLED();
                        /* TODO:LOG "secondary Exception on '" +
                         * prev_seg_file_name + "': " + err2 + "'; will retry"*/
                    FRT_XENDTRY
                }
            }
        FRT_XENDTRY
    }
}

FrtSegmentInfos *frt_sis_new(FrtFieldInfos *fis)
{
    FrtSegmentInfos *sis = FRT_ALLOC_AND_ZERO(FrtSegmentInfos);
    FRT_REF(fis);
    sis->fis = fis;
    sis->format = FORMAT;
    sis->version = (frt_u64)time(NULL);
    sis->size = 0;
    sis->counter = 0;
    sis->generation = -1;
    sis->capa = 4;
    sis->segs = FRT_ALLOC_N(FrtSegmentInfo *, sis->capa);
    return sis;
}

FrtSegmentInfo *frt_sis_new_segment(FrtSegmentInfos *sis, int doc_cnt, FrtStore *store)
{
    return frt_sis_add_si(sis, frt_si_new(new_segment(sis->counter++), doc_cnt, store));
}

void frt_sis_destroy(FrtSegmentInfos *sis)
{
    int i;
    const int sis_size = sis->size;
    for (i = 0; i < sis_size; i++) {
        frt_si_deref(sis->segs[i]);
    }
    if (sis->fis) frt_fis_deref(sis->fis);
    free(sis->segs);
    free(sis);
}

FrtSegmentInfo *frt_sis_add_si(FrtSegmentInfos *sis, FrtSegmentInfo *si)
{
    if (sis->size >= sis->capa) {
        sis->capa <<= 1;
        FRT_REALLOC_N(sis->segs, FrtSegmentInfo *, sis->capa);
    }
    sis->segs[sis->size++] = si;
    return si;
}

void frt_sis_del_at(FrtSegmentInfos *sis, int at)
{
    int i;
    const int sis_size = --(sis->size);
    frt_si_deref(sis->segs[at]);
    for (i = at; i < sis_size; i++) {
        sis->segs[i] = sis->segs[i+1];
    }
}

void frt_sis_del_from_to(FrtSegmentInfos *sis, int from, int to)
{
    int i, num_to_del = to - from;
    const int sis_size = sis->size -= num_to_del;
    for (i = from; i < to; i++) {
        frt_si_deref(sis->segs[i]);
    }
    for (i = from; i < sis_size; i++) {
        sis->segs[i] = sis->segs[i+num_to_del];
    }
}

static void frt_sis_read_i(FrtStore *store, FindSegmentsFile *fsf)
{
    int seg_cnt;
    int i;
    volatile bool success = false;
    char seg_file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtInStream *volatile is = NULL;
    FrtSegmentInfos *volatile sis = FRT_ALLOC_AND_ZERO(FrtSegmentInfos);
    segfn_for_generation(seg_file_name, fsf->generation);
    fsf->ret.sis = NULL;
    FRT_TRY
        is = store->open_input(store, seg_file_name);
        sis->store = store;
        sis->generation = fsf->generation;
        sis->format = frt_is_read_u32(is); /* do nothing. it's the first version */
        sis->version = frt_is_read_u64(is);
        sis->counter = frt_is_read_u64(is);
        seg_cnt = frt_is_read_vint(is);
        /* allocate space for segments */
        for (sis->capa = 4; sis->capa < seg_cnt; sis->capa <<= 1) {}
        sis->size = 0;
        sis->segs = FRT_ALLOC_N(FrtSegmentInfo *, sis->capa);
        for (i = 0; i < seg_cnt; i++) {
            frt_sis_add_si(sis, si_read(store, is));
        }
        sis->fis = frt_fis_read(is);
        success = true;
    FRT_XFINALLY
        if (is) frt_is_close(is);
        if (!success) {
            frt_sis_destroy(sis);
        }
    FRT_XENDTRY
    fsf->ret.sis = sis;
}

FrtSegmentInfos *frt_sis_read(FrtStore *store)
{
    FindSegmentsFile fsf;
    sis_find_segments_file(store, &fsf, &frt_sis_read_i);
    return fsf.ret.sis;
}

void frt_sis_write(FrtSegmentInfos *sis, FrtStore *store, FrtDeleter *deleter)
{
    int i;
    FrtOutStream *volatile os = NULL;
    const int sis_size = sis->size;
    char buf[FRT_SEGMENT_NAME_MAX_LENGTH];
    sis->generation++;
    FRT_TRY
        os = store->new_output(store, segfn_for_generation(buf, sis->generation));
        frt_os_write_u32(os, FORMAT);
        frt_os_write_u64(os, ++(sis->version)); /* every write changes the index */
        frt_os_write_u64(os, sis->counter);
        frt_os_write_vint(os, sis->size);
        for (i = 0; i < sis_size; i++) {
            si_write(sis->segs[i], os);
        }
        frt_fis_write(sis->fis, os);
    FRT_XFINALLY
        frt_os_close(os);
    FRT_XENDTRY

    FRT_TRY
        os = store->new_output(store, SEGMENTS_GEN_FILE_NAME);
        frt_os_write_u64(os, sis->generation);
        frt_os_write_u64(os, sis->generation);
    FRT_XFINALLY
        /* It's OK if we fail to write this file since it's
         * used only as one of the retry fallbacks. */
        FRT_HANDLED();
        frt_os_close(os);
    FRT_XENDTRY
    if (deleter && sis->generation > 0) {
        frt_deleter_delete_file(deleter, segfn_for_generation(buf, sis->generation - 1));
    }
}

static void frt_sis_read_ver_i(FrtStore *store, FindSegmentsFile *fsf)
{
    FrtInStream *is;
    frt_u64 version;
    char seg_file_name[FRT_SEGMENT_NAME_MAX_LENGTH];

    segfn_for_generation(seg_file_name, (frt_u64)fsf->generation);
    is = store->open_input(store, seg_file_name);
    version = 0;

    FRT_TRY
        frt_is_read_u32(is); // format
        version = frt_is_read_u64(is);
    FRT_XFINALLY
        frt_is_close(is);
    FRT_XENDTRY

    fsf->ret.uint64 = version;
}

frt_u64 frt_sis_read_current_version(FrtStore *store)
{
    FindSegmentsFile fsf;
    sis_find_segments_file(store, &fsf, &frt_sis_read_ver_i);
    return fsf.ret.uint64;
}

/****************************************************************************
 *
 * LazyDocField
 *
 ****************************************************************************/

static FrtLazyDocField *lazy_df_new(FrtSymbol name, const int size, bool is_compressed)
{
    FrtLazyDocField *self = FRT_ALLOC(FrtLazyDocField);
    self->name = name;
    self->size = size;
    self->data = FRT_ALLOC_AND_ZERO_N(FrtLazyDocFieldData, size);
    self->is_compressed = is_compressed;
    return self;
}

static void lazy_df_destroy(FrtLazyDocField *self)
{
    int i;
    for (i = self->size - 1; i >= 0; i--) {
        if (self->data[i].text) {
            free(self->data[i].text);
         }
    }
    free(self->data);
    free(self);
}

static void comp_raise()
{
    FRT_RAISE(EXCEPTION, "Compression error");
}

static char *is_read_compressed_bytes(FrtInStream *is, int compressed_len, int *len)
{
    int buf_out_idx = 0;
    int read_len;
    frt_uchar buf_in[COMPRESSION_BUFFER_SIZE];
    const frt_uchar *next_in;
    size_t available_in;
    frt_uchar *buf_out = NULL;
    frt_uchar *next_out;
    size_t available_out;

    BrotliDecoderState *b_state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    BrotliDecoderResult b_result = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
    if (!b_state) { comp_raise(); return NULL; }

    do {
        read_len = compressed_len > COMPRESSION_BUFFER_SIZE ? COMPRESSION_BUFFER_SIZE : compressed_len;
        frt_is_read_bytes(is, buf_in, read_len);
        compressed_len -= read_len;
        available_in = read_len;
        next_in = buf_in;
        available_out = COMPRESSION_BUFFER_SIZE;
        do {
            FRT_REALLOC_N(buf_out, frt_uchar, buf_out_idx + COMPRESSION_BUFFER_SIZE);
            next_out = buf_out + buf_out_idx;
            b_result = BrotliDecoderDecompressStream(b_state,
                &available_in, &next_in,
                &available_out, &next_out, NULL);
            if (b_result == BROTLI_DECODER_RESULT_ERROR) { comp_raise(); return NULL; }
            buf_out_idx += COMPRESSION_BUFFER_SIZE - available_out;
        } while (b_result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);
    } while (b_result != BROTLI_DECODER_RESULT_SUCCESS && compressed_len > 0);

    BrotliDecoderDestroyInstance(b_state);

    FRT_REALLOC_N(buf_out, frt_uchar, buf_out_idx + 1);
    buf_out[buf_out_idx] = '\0';
    *len = buf_out_idx;
    return (char *)buf_out;
}

char *frt_lazy_df_get_data(FrtLazyDocField *self, int i)
{
    char *text = NULL;
    if (i < self->size && i >= 0) {
        text = self->data[i].text;
        if (NULL == text) {
            const int read_len = self->data[i].length + 1;
            frt_is_seek(self->doc->fields_in, self->data[i].start);
            if (self->is_compressed) {
                self->data[i].text = text = is_read_compressed_bytes(self->doc->fields_in, read_len, &(self->data[i].length));
            } else {
                self->data[i].text = text = FRT_ALLOC_N(char, read_len);
                frt_is_read_bytes(self->doc->fields_in, (frt_uchar *)text, read_len);
                text[read_len - 1] = '\0';
            }
        }
    }

    return text;
}

void frt_lazy_df_get_bytes(FrtLazyDocField *self, char *buf, int start, int len)
{
    if (self->is_compressed == 1) {
        int i;
        self->len = 0;
        for (i = self->size-1; i >= 0; i--) {
            (void)frt_lazy_df_get_data(self, i);
            self->len += self->data[i].length + 1;
        }
        self->len--; /* each field separated by ' ' but no need to add to end */
        self->is_compressed = 2;
    }
    if (start < 0 || start >= self->len) {
        FRT_RAISE(FRT_IO_ERROR, "start out of range in LazyDocField#get_bytes. %d "
              "is not between 0 and %d", start, self->len);
    }
    if (len <= 0) {
        FRT_RAISE(FRT_IO_ERROR, "len = %d, but should be greater than 0", len);
    }
    if (start + len > self->len) {
        FRT_RAISE(FRT_IO_ERROR, "Tried to read past end of field. Field is only %d "
              "bytes long but tried to read to %d", self->len, start + len);
    }
    if (self->is_compressed) {
        int cur_start = 0, buf_start = 0, cur_end, i, copy_start, copy_len;
        for (i = 0; i < self->size; i++) {
            cur_end = cur_start + self->data[i].length;
            if (start < cur_end) {
                copy_start = start > cur_start ? start - cur_start : 0;
                copy_len = cur_end - cur_start - copy_start;
                if (copy_len >= len) {
                    copy_len = len;
                    len = 0;
                }
                else {
                    len -= copy_len;
                }
                memcpy(buf + buf_start,
                       self->data[i].text + copy_start,
                       copy_len);
                buf_start += copy_len;
                if (len > 0) {
                    buf[buf_start++] = ' ';
                    len--;
                }
                if (len == 0) break;
            }
            cur_start = cur_end + 1;
        }
    } else {
        frt_is_seek(self->doc->fields_in, self->data[0].start + start);
        frt_is_read_bytes(self->doc->fields_in, (frt_uchar *)buf, len);
    }
}

/****************************************************************************
 *
 * LazyDoc
 *
 ****************************************************************************/

static FrtLazyDoc *lazy_doc_new(int size, FrtInStream *fdt_in)
{
    FrtLazyDoc *self = FRT_ALLOC(FrtLazyDoc);
    self->field_dictionary = frt_h_new_ptr((frt_free_ft)&lazy_df_destroy);
    self->size = size;
    self->fields = FRT_ALLOC_AND_ZERO_N(FrtLazyDocField *, size);
    self->fields_in = frt_is_clone(fdt_in);
    return self;
}

void frt_lazy_doc_close(FrtLazyDoc *self)
{
    frt_h_destroy(self->field_dictionary);
    frt_is_close(self->fields_in);
    free(self->fields);
    free(self);
}

static void lazy_doc_add_field(FrtLazyDoc *self, FrtLazyDocField *lazy_df, int i)
{
    self->fields[i] = lazy_df;

    frt_h_set(self->field_dictionary, (void *)lazy_df->name, lazy_df);
    lazy_df->doc = self;
}

FrtLazyDocField *frt_lazy_doc_get(FrtLazyDoc *self, FrtSymbol field)
{
    return (FrtLazyDocField *)frt_h_get(self->field_dictionary, (void *)field);
}

/****************************************************************************
 *
 * FrtFieldsReader
 *
 ****************************************************************************/

#define FIELDS_IDX_PTR_SIZE 12

FrtFieldsReader *frt_fr_open(FrtStore *store, const char *segment, FrtFieldInfos *fis)
{
    FrtFieldsReader *fr = FRT_ALLOC(FrtFieldsReader);
    FrtInStream *fdx_in;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    size_t segment_len = strlen(segment);

    memcpy(file_name, segment, segment_len);

    fr->fis = fis;

    strcpy(file_name + segment_len, ".fdt");
    fr->fdt_in = store->open_input(store, file_name);
    strcpy(file_name + segment_len, ".fdx");
    fdx_in = fr->fdx_in = store->open_input(store, file_name);
    fr->size = frt_is_length(fdx_in) / FIELDS_IDX_PTR_SIZE;
    fr->store = store;

    return fr;
}

FrtFieldsReader *frt_fr_clone(FrtFieldsReader *orig)
{
    FrtFieldsReader *fr = FRT_ALLOC(FrtFieldsReader);

    memcpy(fr, orig, sizeof(FrtFieldsReader));
    fr->fdx_in = frt_is_clone(orig->fdx_in);
    fr->fdt_in = frt_is_clone(orig->fdt_in);

    return fr;
}

void frt_fr_close(FrtFieldsReader *fr)
{
    frt_is_close(fr->fdt_in);
    frt_is_close(fr->fdx_in);
    free(fr);
}

static FrtDocField *frt_fr_df_new(FrtSymbol name, int size, bool is_compressed)
{
    FrtDocField *df = FRT_ALLOC(FrtDocField);
    df->name = name;
    df->capa = df->size = size;
    df->data = FRT_ALLOC_N(char *, df->capa);
    df->lengths = FRT_ALLOC_N(int, df->capa);
    df->encodings = FRT_ALLOC_N(rb_encoding *, df->capa);
    df->destroy_data = true;
    df->boost = 1.0f;
    df->is_compressed = is_compressed;
    return df;
}

static void frt_fr_read_compressed_fields(FrtFieldsReader *fr, FrtDocField *df)
{
    int i;
    const int df_size = df->size;
    FrtInStream *fdt_in = fr->fdt_in;

    for (i = 0; i < df_size; i++) {
        const int compressed_len = df->lengths[i] + 1;
        df->data[i] = is_read_compressed_bytes(fdt_in, compressed_len, &(df->lengths[i]));
    }
}

FrtDocument *frt_fr_get_doc(FrtFieldsReader *fr, int doc_num)
{
    int i, j;
    off_t pos;
    int stored_cnt;
    FrtDocument *doc = frt_doc_new();
    FrtInStream *fdx_in = fr->fdx_in;
    FrtInStream *fdt_in = fr->fdt_in;

    frt_is_seek(fdx_in, doc_num * FIELDS_IDX_PTR_SIZE);
    pos = (off_t)frt_is_read_u64(fdx_in);
    frt_is_seek(fdt_in, pos);
    stored_cnt = frt_is_read_vint(fdt_in);

    for (i = 0; i < stored_cnt; i++) {
        const int field_num = frt_is_read_vint(fdt_in);
        FrtFieldInfo *fi = fr->fis->fields[field_num];
        const int df_size = frt_is_read_vint(fdt_in);
        FrtDocField *df = frt_fr_df_new(fi->name, df_size, fi_is_compressed(fi));

        for (j = 0; j < df_size; j++) {
            df->lengths[j] = frt_is_read_vint(fdt_in);
            df->encodings[j] = rb_enc_from_index(frt_is_read_vint(fdt_in));
        }

        frt_doc_add_field(doc, df);
    }
    for (i = 0; i < stored_cnt; i++) {
        FrtDocField *df = doc->fields[i];
        if (df->is_compressed) {
            frt_fr_read_compressed_fields(fr, df);
        } else {
            const int df_size = df->size;
            for (j = 0; j < df_size; j++) {
                const int read_len = df->lengths[j] + 1;
                df->data[j] = FRT_ALLOC_N(char, read_len);
                frt_is_read_bytes(fdt_in, (frt_uchar *)df->data[j], read_len);
                df->data[j][read_len - 1] = '\0';
            }
        }
    }

    return doc;
}

FrtLazyDoc *frt_fr_get_lazy_doc(FrtFieldsReader *fr, int doc_num)
{
    int start = 0;
    int i, j;
    off_t pos;
    int stored_cnt;
    FrtLazyDoc *lazy_doc;
    FrtInStream *fdx_in = fr->fdx_in;
    FrtInStream *fdt_in = fr->fdt_in;

    frt_is_seek(fdx_in, doc_num * FIELDS_IDX_PTR_SIZE);
    pos = (off_t)frt_is_read_u64(fdx_in);
    frt_is_seek(fdt_in, pos);
    stored_cnt = frt_is_read_vint(fdt_in);

    lazy_doc = lazy_doc_new(stored_cnt, fdt_in);
    for (i = 0; i < stored_cnt; i++) {
        FrtFieldInfo *fi = fr->fis->fields[frt_is_read_vint(fdt_in)];
        const int df_size = frt_is_read_vint(fdt_in);
        FrtLazyDocField *lazy_df = lazy_df_new(fi->name, df_size, fi_is_compressed(fi));
        const int field_start = start;
        /* get the starts relative positions this time around */

        for (j = 0; j < df_size; j++) {
            lazy_df->data[j].start = start;
            start += 1 + (lazy_df->data[j].length = frt_is_read_vint(fdt_in));
            lazy_df->data[j].encoding = rb_enc_from_index(frt_is_read_vint(fdt_in));
        }

        lazy_df->len = start - field_start - 1;
        lazy_doc_add_field(lazy_doc, lazy_df, i);
    }
    /* correct the starts to their correct absolute positions */
    const off_t abs_start = frt_is_pos(fdt_in);
    for (i = 0; i < stored_cnt; i++) {
        FrtLazyDocField *lazy_df = lazy_doc->fields[i];
        const int df_size = lazy_df->size;
        for (j = 0; j < df_size; j++) {
            lazy_df->data[j].start += abs_start;
        }
    }

    return lazy_doc;
}

static FrtTermVector *frt_fr_read_term_vector(FrtFieldsReader *fr, int field_num)
{
    FrtTermVector *tv = FRT_ALLOC_AND_ZERO(FrtTermVector);
    FrtInStream *fdt_in = fr->fdt_in;
    FrtFieldInfo *fi = fr->fis->fields[field_num];
    const int num_terms = frt_is_read_vint(fdt_in);

    tv->field_num = field_num;
    tv->field = fi->name;

    if (num_terms > 0) {
        int i, j, delta_start, delta_len, total_len, freq;
        int store_positions = fi_store_positions(fi);
        int store_offsets = fi_store_offsets(fi);
        frt_uchar buffer[FRT_MAX_WORD_SIZE];
        FrtTVTerm *term;

        tv->term_cnt = num_terms;
        tv->terms = FRT_ALLOC_AND_ZERO_N(FrtTVTerm, num_terms);

        for (i = 0; i < num_terms; i++) {
            term = &(tv->terms[i]);
            /* read delta encoded term */
            delta_start = frt_is_read_vint(fdt_in);
            delta_len = frt_is_read_vint(fdt_in);
            total_len = delta_start + delta_len;
            frt_is_read_bytes(fdt_in, buffer + delta_start, delta_len);
            buffer[total_len++] = '\0';
            term->text = (char *)memcpy(FRT_ALLOC_N(char, total_len),
                                        buffer, total_len);

            /* read freq */
            freq = term->freq = frt_is_read_vint(fdt_in);

            /* read positions if necessary */
            if (store_positions) {
                int *positions = term->positions = FRT_ALLOC_N(int, freq);
                int pos = 0;
                for (j = 0; j < freq; j++) {
                    positions[j] = pos += frt_is_read_vint(fdt_in);
                }
            }

            /* read offsets if necessary */
        }
        if (store_offsets) {
            int num_positions = tv->offset_cnt = frt_is_read_vint(fdt_in);
            FrtOffset *offsets = tv->offsets = FRT_ALLOC_N(FrtOffset, num_positions);
            frt_i64 offset = 0;
            for (i = 0; i < num_positions; i++) {
                offsets[i].start =
                    (off_t)(offset += (frt_i64)frt_is_read_vll(fdt_in));
                offsets[i].end =
                    (off_t)(offset += (frt_i64)frt_is_read_vll(fdt_in));
            }
        }
    }
    return tv;
}

FrtHash *frt_fr_get_tv(FrtFieldsReader *fr, int doc_num)
{
    FrtHash *term_vectors = frt_h_new_ptr((frt_free_ft)&frt_tv_destroy);
    int i;
    FrtInStream *fdx_in = fr->fdx_in;
    FrtInStream *fdt_in = fr->fdt_in;
    off_t data_ptr, field_index_ptr;
    int field_cnt;
    int *field_nums;

    if (doc_num >= 0 && doc_num < fr->size) {
        frt_is_seek(fdx_in, FIELDS_IDX_PTR_SIZE * doc_num);

        data_ptr = (off_t)frt_is_read_u64(fdx_in);
        field_index_ptr = data_ptr += (off_t)frt_is_read_u32(fdx_in);

        /* scan fields to get position of field_num's term vector */
        frt_is_seek(fdt_in, field_index_ptr);

        field_cnt = frt_is_read_vint(fdt_in);
        field_nums = FRT_ALLOC_N(int, field_cnt);

        for (i = field_cnt - 1; i >= 0; i--) {
            int tv_size;
            field_nums[i] = frt_is_read_vint(fdt_in);
            tv_size = frt_is_read_vint(fdt_in);
            data_ptr -= tv_size;
        }
        frt_is_seek(fdt_in, data_ptr);

        for (i = 0; i < field_cnt; i++) {
            FrtTermVector *tv = frt_fr_read_term_vector(fr, field_nums[i]);
            frt_h_set(term_vectors, (void *)tv->field, tv);
        }
        free(field_nums);
    }
    return term_vectors;
}

FrtTermVector *frt_fr_get_field_tv(FrtFieldsReader *fr, int doc_num, int field_num)
{
    FrtTermVector *tv = NULL;

    if (doc_num >= 0 && doc_num < fr->size) {
        int i, fnum = -1;
        off_t field_index_ptr;
        int field_cnt;
        int offset = 0;
        FrtInStream *fdx_in = fr->fdx_in;
        FrtInStream *fdt_in = fr->fdt_in;

        frt_is_seek(fdx_in, FIELDS_IDX_PTR_SIZE * doc_num);

        field_index_ptr =  (off_t)frt_is_read_u64(fdx_in);
        field_index_ptr += (off_t)frt_is_read_u32(fdx_in);

        /* scan fields to get position of field_num's term vector */
        frt_is_seek(fdt_in, field_index_ptr);

        field_cnt = frt_is_read_vint(fdt_in);
        for (i = field_cnt - 1; i >= 0 && fnum != field_num; i--) {
            fnum = frt_is_read_vint(fdt_in);
            offset += frt_is_read_vint(fdt_in); /* space taken by field */
        }

        if (fnum == field_num) {
            /* field was found */
            frt_is_seek(fdt_in, field_index_ptr - (off_t)offset);
            tv = frt_fr_read_term_vector(fr, field_num);
        }
    }
    return tv;
}

/****************************************************************************
 *
 * FrtFieldsWriter
 *
 ****************************************************************************/

FrtFieldsWriter *frt_fw_open(FrtStore *store, const char *segment, FrtFieldInfos *fis)
{
    FrtFieldsWriter *fw = FRT_ALLOC(FrtFieldsWriter);
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    size_t segment_len = strlen(segment);

    memcpy(file_name, segment, segment_len);

    strcpy(file_name + segment_len, ".fdt");
    fw->fdt_out = store->new_output(store, file_name);

    strcpy(file_name + segment_len, ".fdx");
    fw->fdx_out = store->new_output(store, file_name);

    fw->buffer = frt_ram_new_buffer();

    fw->fis = fis;
    fw->tv_fields = frt_ary_new_type_capa(FrtTVField, FRT_TV_FIELD_INIT_CAPA);

    return fw;
}

void frt_fw_close(FrtFieldsWriter *fw)
{
    frt_os_close(fw->fdt_out);
    frt_os_close(fw->fdx_out);
    frt_ram_destroy_buffer(fw->buffer);
    frt_ary_free(fw->tv_fields);
    free(fw);
}

static int frt_os_write_compressed_bytes(FrtOutStream* out_stream, frt_uchar *data, int length)
{
    size_t compressed_len = 0;
    const frt_uchar *next_in = data;
    size_t available_in = length;
    size_t available_out;
    frt_uchar compression_buffer[COMPRESSION_BUFFER_SIZE];
    frt_uchar *next_out;
    BrotliEncoderState *b_state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    if (!b_state) { comp_raise(); return -1; }

    BrotliEncoderSetParameter(b_state, BROTLI_PARAM_QUALITY, COMPRESSION_LEVEL);

    do {
        available_out = COMPRESSION_BUFFER_SIZE;
        next_out = compression_buffer;
        if (!BrotliEncoderCompressStream(b_state, BROTLI_OPERATION_FINISH,
            &available_in, &next_in,
            &available_out, &next_out, &compressed_len)) {
            BrotliEncoderDestroyInstance(b_state);
            comp_raise();
            return -1;
        }
        frt_os_write_bytes(out_stream, compression_buffer, COMPRESSION_BUFFER_SIZE - available_out);
    } while (!BrotliEncoderIsFinished(b_state));

    BrotliEncoderDestroyInstance(b_state);
    // fprintf(stderr, "Compressed: %i -> %i\n", length, (int)compressed_len);
    return (int)compressed_len;
}

void frt_fw_add_doc(FrtFieldsWriter *fw, FrtDocument *doc)
{
    int i, j, stored_cnt = 0;
    FrtDocField *df;
    FrtFieldInfo *fi;
    FrtOutStream *fdt_out = fw->fdt_out, *fdx_out = fw->fdx_out;
    const int doc_size = doc->size;

    for (i = 0; i < doc_size; i++) {
        df = doc->fields[i];
        if (fi_is_stored(frt_fis_get_or_add_field(fw->fis, df->name))) {
            stored_cnt++;
        }
    }

    fw->start_ptr = frt_os_pos(fdt_out);
    frt_ary_size(fw->tv_fields) = 0;
    frt_os_write_u64(fdx_out, fw->start_ptr);
    frt_os_write_vint(fdt_out, stored_cnt);
    frt_ramo_reset(fw->buffer);

    for (i = 0; i < doc_size; i++) {
        df = doc->fields[i];
        fi = frt_fis_get_field(fw->fis, df->name);
        if (fi_is_stored(fi)) {
            const int df_size = df->size;
            frt_os_write_vint(fdt_out, fi->number);
            frt_os_write_vint(fdt_out, df_size);

            if (fi_is_compressed(fi)) {
                for (j = 0; j < df_size; j++) {
                    const int length = df->lengths[j];
                    int compressed_len = frt_os_write_compressed_bytes(fw->buffer, (frt_uchar*)df->data[j], length);
                    frt_os_write_vint(fdt_out, compressed_len - 1);
                    frt_os_write_vint(fdt_out, rb_enc_to_index(df->encodings[j]));
                }
            } else {
                for (j = 0; j < df_size; j++) {
                    const int length = df->lengths[j];
                    frt_os_write_vint(fdt_out, length);
                    frt_os_write_vint(fdt_out, rb_enc_to_index(df->encodings[j]));
                    frt_os_write_bytes(fw->buffer, (frt_uchar*)df->data[j], length);
                    /* leave a space between fields as that is how they are analyzed */
                    frt_os_write_byte(fw->buffer, ' ');
                }
            }
        }
    }
    frt_ramo_write_to(fw->buffer, fdt_out);
}

void frt_fw_write_tv_index(FrtFieldsWriter *fw)
{
    int i;
    const int tv_cnt = frt_ary_size(fw->tv_fields);
    FrtOutStream *fdt_out = fw->fdt_out;
    frt_os_write_u32(fw->fdx_out, (frt_u32)(frt_os_pos(fdt_out) - fw->start_ptr));
    frt_os_write_vint(fdt_out, tv_cnt);
    /* write in reverse order so we can count back from the start position to
     * the beginning of the TermVector's data */
    for (i = tv_cnt - 1; i >= 0; i--) {
        frt_os_write_vint(fdt_out, fw->tv_fields[i].field_num);
        frt_os_write_vint(fdt_out, fw->tv_fields[i].size);
    }
}

void frt_fw_add_postings(FrtFieldsWriter *fw,
                     int field_num,
                     FrtPostingList **plists,
                     int posting_count,
                     FrtOffset *offsets,
                     int offset_count)
{
    int i, delta_start, delta_length;
    const char *last_term = FRT_EMPTY_STRING;
    FrtOutStream *fdt_out = fw->fdt_out;
    off_t fdt_start_pos = frt_os_pos(fdt_out);
    FrtPostingList *plist;
    FrtPosting *posting;
    FrtOccurence *occ;
    FrtFieldInfo *fi = fw->fis->fields[field_num];
    int store_positions = fi_store_positions(fi);

    frt_ary_grow(fw->tv_fields);
    frt_ary_last(fw->tv_fields).field_num = field_num;

    frt_os_write_vint(fdt_out, posting_count);
    for (i = 0; i < posting_count; i++) {
        plist = plists[i];
        posting = plist->last;
        delta_start = frt_hlp_string_diff(last_term, plist->term);
        delta_length = plist->term_len - delta_start;

        frt_os_write_vint(fdt_out, delta_start);  /* write shared prefix length */
        frt_os_write_vint(fdt_out, delta_length); /* write delta length */
        /* write delta chars */
        frt_os_write_bytes(fdt_out,
                       (frt_uchar *)(plist->term + delta_start),
                       delta_length);
        frt_os_write_vint(fdt_out, posting->freq);
        last_term = plist->term;

        if (store_positions) {
            /* use delta encoding for positions */
            int last_pos = 0;
            for (occ = posting->first_occ; occ; occ = occ->next) {
                frt_os_write_vint(fdt_out, occ->pos - last_pos);
                last_pos = occ->pos;
            }
        }

    }

    if (fi_store_offsets(fi)) {
        /* use delta encoding for offsets */
        frt_i64 last_end = 0;
        frt_os_write_vint(fdt_out, offset_count);  /* write shared prefix length */
        for (i = 0; i < offset_count; i++) {
            frt_i64 start = (frt_i64)offsets[i].start;
            frt_i64 end = (frt_i64)offsets[i].end;
            frt_os_write_vll(fdt_out, (frt_u64)(start - last_end));
            frt_os_write_vll(fdt_out, (frt_u64)(end - start));
            last_end = end;
        }
    }
    frt_ary_last(fw->tv_fields).size = frt_os_pos(fdt_out) - fdt_start_pos;
}

/****************************************************************************
 *
 * TermEnum
 *
 ****************************************************************************/

#define TE(ste) ((FrtTermEnum *)ste)

char *frt_te_get_term(FrtTermEnum *te)
{
    return (char *)memcpy(FRT_ALLOC_N(char, te->curr_term_len + 1),
                          te->curr_term, te->curr_term_len + 1);
}

FrtTermInfo *frt_te_get_ti(FrtTermEnum *te)
{
    return (FrtTermInfo*)memcpy(FRT_ALLOC(FrtTermInfo), &(te->curr_ti), sizeof(FrtTermInfo));
}

static char *te_skip_to(FrtTermEnum *te, const char *term)
{
    char *curr_term = te->curr_term;
    if (strcmp(curr_term, term) < 0) {
        while (NULL != ((curr_term = te->next(te)))
               && (strcmp(curr_term, term) < 0)) {
        }
    }
    return curr_term;
}

/****************************************************************************
 *
 * SegmentTermEnum
 *
 ****************************************************************************/

#define STE(te) ((FrtSegmentTermEnum *)te)

/****************************************************************************
 * SegmentTermIndex
 ****************************************************************************/

static void sti_destroy(FrtSegmentTermIndex *sti)
{
    if (sti->index_terms) {
        int i;
        const int sti_index_cnt = sti->index_cnt;
        for (i = 0; i < sti_index_cnt; i++) {
            free(sti->index_terms[i]);
        }
        free(sti->index_terms);
        free(sti->index_term_lens);
        free(sti->index_term_infos);
        free(sti->index_ptrs);
    }
    free(sti);
}

static void sti_ensure_index_is_read(FrtSegmentTermIndex *sti,
                                     FrtTermEnum *index_te)
{
    if (NULL == sti->index_terms) {
        int i;
        int index_cnt = sti->index_cnt;
        off_t index_ptr = 0;
        ste_reset(index_te);
        frt_is_seek(STE(index_te)->is, sti->index_ptr);
        STE(index_te)->size = sti->index_cnt;

        sti->index_terms = FRT_ALLOC_N(char *, index_cnt);
        sti->index_term_lens = FRT_ALLOC_N(int, index_cnt);
        sti->index_term_infos = FRT_ALLOC_N(FrtTermInfo, index_cnt);
        sti->index_ptrs = FRT_ALLOC_N(off_t, index_cnt);

        for (i = 0; NULL != ste_next(index_te); i++) {
#ifdef DEBUG
            if (i >= index_cnt) {
                FRT_RAISE(FRT_INDEX_ERROR, "index term enum read too many terms");
            }
#endif
            sti->index_terms[i] = frt_te_get_term(index_te);
            sti->index_term_lens[i] = index_te->curr_term_len;
            sti->index_term_infos[i] = index_te->curr_ti;
            index_ptr += frt_is_read_voff_t(STE(index_te)->is);
            sti->index_ptrs[i] = index_ptr;
        }
    }
}

static int sti_get_index_offset(FrtSegmentTermIndex *sti, const char *term)
{
    int lo = 0;
    int hi = sti->index_cnt - 1;
    int mid, delta;
    char **index_terms = sti->index_terms;

    while (hi >= lo) {
        mid = (lo + hi) >> 1;
        delta = strcmp(term, index_terms[mid]);
        if (delta < 0) {
            hi = mid - 1;
        }
        else if (delta > 0) {
            lo = mid + 1;
        }
        else {
            return mid;
        }
    }
    return hi;
}

/****************************************************************************
 * SegmentFieldIndex
 ****************************************************************************/

#define SFI_ENSURE_INDEX_IS_READ(sfi, sti) do {\
    if (NULL == sti->index_terms) {\
        frt_mutex_lock(&sfi->mutex);\
        sti_ensure_index_is_read(sti, sfi->index_te);\
        frt_mutex_unlock(&sfi->mutex);\
    }\
} while (0)

FrtSegmentFieldIndex *frt_sfi_open(FrtStore *store, const char *segment)
{
    int field_count;
    FrtSegmentFieldIndex *sfi = FRT_ALLOC(FrtSegmentFieldIndex);
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtInStream *is;

    frt_mutex_init(&sfi->mutex, NULL);

    sprintf(file_name, "%s.tfx", segment);
    is = store->open_input(store, file_name);
    field_count = (int)frt_is_read_u32(is);
    sfi->index_interval = frt_is_read_vint(is);
    sfi->skip_interval = frt_is_read_vint(is);

    sfi->field_dict = frt_h_new_int((frt_free_ft)&sti_destroy);

    for (; field_count > 0; field_count--) {
        int field_num = frt_is_read_vint(is);
        FrtSegmentTermIndex *sti = FRT_ALLOC_AND_ZERO(FrtSegmentTermIndex);
        sti->index_ptr = frt_is_read_voff_t(is);
        sti->ptr = frt_is_read_voff_t(is);
        sti->index_cnt = frt_is_read_vint(is);
        sti->size = frt_is_read_vint(is);
        frt_h_set_int(sfi->field_dict, field_num, sti);
    }
    frt_is_close(is);

    sprintf(file_name, "%s.tix", segment);
    is = store->open_input(store, file_name);
    sfi->index_te = frt_ste_new(is, sfi);
    return sfi;
}

void frt_sfi_close(FrtSegmentFieldIndex *sfi)
{
    frt_mutex_destroy(&sfi->mutex);
    frt_ste_close(sfi->index_te);
    frt_h_destroy(sfi->field_dict);
    free(sfi);
}

/****************************************************************************
 * SegmentTermEnum
 ****************************************************************************/

static int term_read(char *buf, FrtInStream *is)
{
    int start = (int)frt_is_read_vint(is);
    int length = (int)frt_is_read_vint(is);
    int total_length = start + length;
    frt_is_read_bytes(is, (frt_uchar *)(buf + start), length);
    buf[total_length] = '\0';
    return total_length;
}

static char *ste_next(FrtTermEnum *te)
{
    FrtTermInfo *ti;
    FrtInStream *is = STE(te)->is;

    STE(te)->pos++;
    if (STE(te)->pos >= STE(te)->size) {
        te->curr_term[0] = '\0';
        te->curr_term_len = 0;
        return NULL;
    }

    memcpy(te->prev_term, te->curr_term, te->curr_term_len + 1);
    te->curr_term_len = term_read(te->curr_term, is);

    ti = &(te->curr_ti);
    ti->doc_freq = frt_is_read_vint(is);     /* read doc freq */
    ti->frq_ptr += frt_is_read_voff_t(is);   /* read freq ptr */
    ti->prx_ptr += frt_is_read_voff_t(is);   /* read prox ptr */
    if (ti->doc_freq >= STE(te)->skip_interval) {
        ti->skip_offset = frt_is_read_voff_t(is);
    }

    return te->curr_term;
}

static void ste_reset(FrtTermEnum *te)
{
    STE(te)->pos = -1;
    te->curr_term[0] = '\0';
    te->curr_term_len = 0;
    FRT_ZEROSET(&(te->curr_ti), FrtTermInfo);
}

static FrtTermEnum *ste_set_field(FrtTermEnum *te, int field_num)
{
    FrtSegmentTermIndex *sti
        = (FrtSegmentTermIndex *)frt_h_get_int(STE(te)->sfi->field_dict, field_num);
    ste_reset(te);
    te->field_num = field_num;
    if (sti) {
        STE(te)->size = sti->size;
        frt_is_seek(STE(te)->is, sti->ptr);
    }
    else {
        STE(te)->size = 0;
    }
    return te;
}

static void frt_ste_index_seek(FrtTermEnum *te, FrtSegmentTermIndex *sti, int idx_offset)
{
    int term_len = sti->index_term_lens[idx_offset];
    frt_is_seek(STE(te)->is, sti->index_ptrs[idx_offset]);
    STE(te)->pos = STE(te)->sfi->index_interval * idx_offset - 1;
    memcpy(te->curr_term,
           sti->index_terms[idx_offset],
           term_len + 1);
    te->curr_term_len = term_len;
    te->curr_ti = sti->index_term_infos[idx_offset];
}

static char *ste_scan_to(FrtTermEnum *te, const char *term)
{
    FrtSegmentFieldIndex *sfi = STE(te)->sfi;
    FrtSegmentTermIndex *sti
        = (FrtSegmentTermIndex *)frt_h_get_int(sfi->field_dict, te->field_num);
    if (sti && sti->size > 0) {
        SFI_ENSURE_INDEX_IS_READ(sfi, sti);
        if (term[0] == '\0') {
            frt_ste_index_seek(te, sti, 0);
            return ste_next(te);;
        }
        /* if current term is less than seek term */
        if (STE(te)->pos < STE(te)->size && strcmp(te->curr_term, term) <= 0) {
            int enum_offset = (int)(STE(te)->pos / sfi->index_interval) + 1;
            /* if we are at the end of the index or before the next index
             * ptr then a simple scan suffices */
            if (sti->index_cnt == enum_offset ||
                strcmp(term, sti->index_terms[enum_offset]) < 0) {
                return te_skip_to(te, term);
            }
        }
        frt_ste_index_seek(te, sti, sti_get_index_offset(sti, term));
        return te_skip_to(te, term);
    }
    else {
        return NULL;
    }
}

static FrtSegmentTermEnum *ste_allocate()
{
    FrtSegmentTermEnum *ste = FRT_ALLOC_AND_ZERO(FrtSegmentTermEnum);

    TE(ste)->next = &ste_next;
    TE(ste)->set_field = &ste_set_field;
    TE(ste)->skip_to = &ste_scan_to;
    TE(ste)->close = &frt_ste_close;
    return ste;
}

FrtTermEnum *frt_ste_clone(FrtTermEnum *other_te)
{
    FrtSegmentTermEnum *ste = ste_allocate();

    memcpy(ste, other_te, sizeof(FrtSegmentTermEnum));
    ste->is = frt_is_clone(STE(other_te)->is);
    return TE(ste);
}

void frt_ste_close(FrtTermEnum *te)
{
    frt_is_close(STE(te)->is);
    free(te);
}


static char *frt_ste_get_term(FrtTermEnum *te, int pos)
{
    FrtSegmentTermEnum *ste = STE(te);
    if (pos >= ste->size) {
        return NULL;
    }
    else if (pos != ste->pos) {
        int idx_int = ste->sfi->index_interval;
        if ((pos < ste->pos) || pos > (1 + ste->pos / idx_int) * idx_int) {
            FrtSegmentTermIndex *sti = (FrtSegmentTermIndex *)frt_h_get_int(
                ste->sfi->field_dict, te->field_num);
            SFI_ENSURE_INDEX_IS_READ(ste->sfi, sti);
            frt_ste_index_seek(te, sti, pos / idx_int);
        }
        while (ste->pos < pos) {
            if (NULL == ste_next(te)) {
                return NULL;
            }
        }

    }
    return te->curr_term;
}

FrtTermEnum *frt_ste_new(FrtInStream *is, FrtSegmentFieldIndex *sfi)
{
    FrtSegmentTermEnum *ste = ste_allocate();

    TE(ste)->field_num = -1;
    ste->is = is;
    ste->size = 0;
    ste->pos = -1;
    ste->sfi = sfi;
    ste->skip_interval = sfi ? sfi->skip_interval : INT_MAX;

    return TE(ste);
}

/****************************************************************************
 * MultiTermEnum
 ****************************************************************************/

#define MTE(te) ((MultiTermEnum *)(te))

typedef struct TermEnumWrapper
{
    int index;
    FrtTermEnum *te;
    int *doc_map;
    FrtIndexReader *ir;
    char *term;
} TermEnumWrapper;

typedef struct MultiTermEnum
{
    FrtTermEnum te;
    int doc_freq;
    FrtPriorityQueue *tew_queue;
    TermEnumWrapper *tews;
    int size;
    int **field_num_map;
    int ti_cnt;
    FrtTermInfo *tis;
    int *ti_indexes;
} MultiTermEnum;

static bool tew_lt(const TermEnumWrapper *tew1, const TermEnumWrapper *tew2)
{
    int cmpres = strcmp(tew1->term, tew2->term);
    if (0 == cmpres) {
        return tew1->index < tew2->index;
    }
    else {
        return cmpres < 0;
    }
}

/*
static void tew_load_doc_map(TermEnumWrapper *tew)
{
    int j = 0, i;
    FrtIndexReader *ir = tew->ir;
    int max_doc = ir->max_doc(ir);
    int *doc_map = tew->doc_map = FRT_ALLOC_N(int, max_doc);

    for (i = 0; i < max_doc; i++) {
        if (ir->is_deleted(ir, i)) {
            doc_map[i] = -1;
        }
        else {
            doc_map[i] = j++;
        }
    }
}
*/

static char *tew_next(TermEnumWrapper *tew)
{
    return (tew->term = tew->te->next(tew->te));
}

static char *tew_skip_to(TermEnumWrapper *tew, const char *term)
{
    return (tew->term = tew->te->skip_to(tew->te, term));
}

static void tew_destroy(TermEnumWrapper *tew)
{
    if (tew->doc_map) {
        free(tew->doc_map);
    }
    tew->te->close(tew->te);
}

static TermEnumWrapper *tew_setup(TermEnumWrapper *tew, int index, FrtTermEnum *te,
                                  FrtIndexReader *ir)
{
    tew->index = index;
    tew->ir = ir;
    tew->te = te;
    tew->term = te->curr_term;
    tew->doc_map = NULL;
    return tew;
}


static char *mte_next(FrtTermEnum *te)
{
    TermEnumWrapper *top =
        (TermEnumWrapper *)frt_pq_top(MTE(te)->tew_queue);

    if (NULL == top) {
        te->curr_term[0] = '\0';
        te->curr_term_len = 0;
        return false;
    }

    memcpy(te->prev_term, te->curr_term, te->curr_term_len + 1);
    memcpy(te->curr_term, top->term, top->te->curr_term_len + 1);
    te->curr_term_len = top->te->curr_term_len;

    te->curr_ti.doc_freq = 0;

    MTE(te)->ti_cnt = 0;
    while ((NULL != top) && (0 == strcmp(te->curr_term, top->term))) {
        frt_pq_pop(MTE(te)->tew_queue);
        te->curr_ti.doc_freq += top->te->curr_ti.doc_freq;/* increment freq */
        MTE(te)->ti_indexes[MTE(te)->ti_cnt] = top->index;
        MTE(te)->tis[MTE(te)->ti_cnt++] = top->te->curr_ti;
        if (tew_next(top)) {
            frt_pq_push(MTE(te)->tew_queue, top); /* restore queue */
        }
        top = (TermEnumWrapper *)frt_pq_top(MTE(te)->tew_queue);
    }
    return te->curr_term;
}

static FrtTermEnum *mte_set_field(FrtTermEnum *te, int field_num)
{
    MultiTermEnum *mte = MTE(te);
    int i;
    const int size = mte->size;
    te->field_num = field_num;
    mte->tew_queue->size = 0;
    for (i = 0; i < size; i++) {
        TermEnumWrapper *tew = &(mte->tews[i]);
        FrtTermEnum *sub_te = tew->te;
        int fnum = mte->field_num_map
            ? mte->field_num_map[i][field_num]
            : field_num;

        if (fnum >= 0) {
            sub_te->set_field(sub_te, fnum);

            if (tew_next(tew)) {
                frt_pq_push(mte->tew_queue, tew); /* initialize queue */
            }
        }
        else {
            sub_te->field_num = -1;
        }

    }
    return te;
}

static char *mte_skip_to(FrtTermEnum *te, const char *term)
{
    MultiTermEnum *mte = MTE(te);
    int i;
    const int size = mte->size;

    mte->tew_queue->size = 0;
    for (i = 0; i < size; i++) {
        TermEnumWrapper *tew = &(mte->tews[i]);

        if (tew->te->field_num >= 0 && tew_skip_to(tew, term)) {
            frt_pq_push(mte->tew_queue, tew); /* initialize queue */
        }
    }
    return mte_next(te);
}

static void mte_close(FrtTermEnum *te)
{
    int i;
    const int size = MTE(te)->size;
    for (i = 0; i < size; i++) {
        tew_destroy(&(MTE(te)->tews[i]));
    }
    free(MTE(te)->tews);
    free(MTE(te)->tis);
    free(MTE(te)->ti_indexes);
    frt_pq_destroy(MTE(te)->tew_queue);
    free(te);
}

FrtTermEnum *frt_mte_new(FrtMultiReader *mr, int field_num, const char *term)
{
    FrtIndexReader **readers   = mr->sub_readers;
    int r_cnt               = mr->r_cnt;
    int i;
    FrtIndexReader *reader;
    MultiTermEnum *mte  = FRT_ALLOC_AND_ZERO(MultiTermEnum);

    TE(mte)->field_num  = field_num;
    TE(mte)->next       = &mte_next;
    TE(mte)->set_field  = &mte_set_field;
    TE(mte)->skip_to    = &mte_skip_to;
    TE(mte)->close      = &mte_close;

    mte->size           = r_cnt;
    mte->tis            = FRT_ALLOC_AND_ZERO_N(FrtTermInfo, r_cnt);
    mte->ti_indexes     = FRT_ALLOC_AND_ZERO_N(int, r_cnt);
    mte->tews           = FRT_ALLOC_AND_ZERO_N(TermEnumWrapper, r_cnt);
    mte->tew_queue      = frt_pq_new(r_cnt, (frt_lt_ft)&tew_lt, (frt_free_ft)NULL);
    mte->field_num_map  = mr->field_num_map;

    for (i = 0; i < r_cnt; i++) {
        int fnum = frt_mr_get_field_num(mr, i, field_num);
        FrtTermEnum *sub_te;
        reader = readers[i];

        if (fnum >= 0) {
            TermEnumWrapper *tew;

            if (NULL != term) {
                sub_te = reader->terms_from(reader, fnum, term);
            }
            else {
                sub_te = reader->terms(reader, fnum);
            }

            tew = tew_setup(&(mte->tews[i]), i, sub_te, reader);
            if (((NULL == term) && tew_next(tew))
                || (tew->term && (tew->term[0] != '\0'))) {
                frt_pq_push(mte->tew_queue, tew);          /* initialize queue */
            }
        }
        else {
            /* add the term_enum_wrapper just in case */
            sub_te = reader->terms(reader, 0);
            sub_te->field_num = -1;
            tew_setup(&(mte->tews[i]), i, sub_te, reader);
        }
    }

    if ((NULL != term) && (0 < mte->tew_queue->size)) {
        mte_next(TE(mte));
    }

    return TE(mte);
}

/****************************************************************************
 *
 * FrtTermInfosReader
 * (Segment Specific)
 *
 ****************************************************************************/

FrtTermInfosReader *frt_tir_open(FrtStore *store,
                          FrtSegmentFieldIndex *sfi, const char *segment)
{
    FrtTermInfosReader *tir = FRT_ALLOC(FrtTermInfosReader);
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];

    sprintf(file_name, "%s.tis", segment);
    tir->orig_te = frt_ste_new(store->open_input(store, file_name), sfi);
    frt_thread_key_create(&tir->thread_te, NULL);
    tir->te_bucket = frt_ary_new();
    tir->field_num = -1;

    return tir;
}

static FrtTermEnum *tir_enum(FrtTermInfosReader *tir)
{
    FrtTermEnum *te;
    if (NULL == (te = (FrtTermEnum *)frt_thread_getspecific(tir->thread_te))) {
        te = frt_ste_clone(tir->orig_te);
        ste_set_field(te, tir->field_num);
        frt_ary_push(tir->te_bucket, te);
        frt_thread_setspecific(tir->thread_te, te);
    }
    return te;
}

FrtTermInfosReader *frt_tir_set_field(FrtTermInfosReader *tir, int field_num)
{
    if (field_num != tir->field_num) {
        ste_set_field(tir_enum(tir), field_num);
        tir->field_num = field_num;
    }
    return tir;
}

FrtTermInfo *frt_tir_get_ti(FrtTermInfosReader *tir, const char *term)
{
    FrtTermEnum *te = tir_enum(tir);
    char *match;

    if (NULL != (match = ste_scan_to(te, term))
        && 0 == strcmp(match, term)) {
        return &(te->curr_ti);
    }
    return NULL;
}

static FrtTermInfo *tir_get_ti_field(FrtTermInfosReader *tir, int field_num,
                                  const char *term)
{
    FrtTermEnum *te = tir_enum(tir);
    char *match;

    if (field_num != tir->field_num) {
        ste_set_field(te, field_num);
        tir->field_num = field_num;
    }

    if (NULL != (match = ste_scan_to(te, term))
        && 0 == strcmp(match, term)) {
        return &(te->curr_ti);
    }
    return NULL;
}

char *frt_tir_get_term(FrtTermInfosReader *tir, int pos)
{
    if (pos < 0) {
        return NULL;
    }
    else {
        return frt_ste_get_term(tir_enum(tir), pos);
    }
}


void frt_tir_close(FrtTermInfosReader *tir)
{
    frt_ary_destroy(tir->te_bucket, (frt_free_ft)&frt_ste_close);
    frt_ste_close(tir->orig_te);

    /* fix for some dodgy old versions of pthread */
    frt_thread_setspecific(tir->thread_te, NULL);

    frt_thread_key_delete(tir->thread_te);
    free(tir);
}

/****************************************************************************
 *
 * FrtTermInfosWriter
 *
 ****************************************************************************/

static FrtTermWriter *tw_new(FrtStore *store, char *file_name)
{
    FrtTermWriter *tw = FRT_ALLOC_AND_ZERO(FrtTermWriter);
    tw->os = store->new_output(store, file_name);
    tw->last_term = FRT_EMPTY_STRING;
    return tw;
}

static void tw_close(FrtTermWriter *tw)
{
    frt_os_close(tw->os);
    free(tw);
}

FrtTermInfosWriter *frt_tiw_open(FrtStore *store,
                          const char *segment,
                          int index_interval,
                          int skip_interval)
{
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtTermInfosWriter *tiw = FRT_ALLOC(FrtTermInfosWriter);
    size_t segment_len = strlen(segment);

    memcpy(file_name, segment, segment_len);

    tiw->field_count = 0;
    tiw->index_interval = index_interval;
    tiw->skip_interval = skip_interval;
    tiw->last_index_ptr = 0;

    strcpy(file_name + segment_len, ".tix");
    tiw->tix_writer = tw_new(store, file_name);
    strcpy(file_name + segment_len, ".tis");
    tiw->tis_writer = tw_new(store, file_name);
    strcpy(file_name + segment_len, ".tfx");
    tiw->tfx_out = store->new_output(store, file_name);
    frt_os_write_u32(tiw->tfx_out, 0); /* make space for field_count */

    /* The following two numbers are the first numbers written to the field
     * index when frt_tiw_start_field is called. But they'll be zero to start with
     * so we'll write index interval and skip interval instead. */
    tiw->tix_writer->counter = tiw->index_interval;
    tiw->tis_writer->counter = tiw->skip_interval;

    return tiw;
}

static void tw_write_term(FrtTermWriter *tw,
                                 FrtOutStream *os,
                                 const char *term,
                                 int term_len)
{
    int start = frt_hlp_string_diff(tw->last_term, term);
    int length = term_len - start;

    frt_os_write_vint(os, start);                   /* write shared prefix length */
    frt_os_write_vint(os, length);                  /* write delta length */
    frt_os_write_bytes(os, (frt_uchar *)(term + start), length); /* write delta chars */

    tw->last_term = term;
}

static void tw_add(FrtTermWriter *tw,
                   const char *term,
                   int term_len,
                   FrtTermInfo *ti,
                   int skip_interval)
{
    FrtOutStream *os = tw->os;

#ifdef DEBUG
    if (strcmp(tw->last_term, term) > 0) {
        FRT_RAISE(FRT_STATE_ERROR, "\"%s\" > \"%s\" %d > %d",
              tw->last_term, term, *tw->last_term, *term);
    }
    if (ti->frq_ptr < tw->last_term_info.frq_ptr) {
        FRT_RAISE(FRT_STATE_ERROR, "%"FRT_OFF_T_PFX"d > %"FRT_OFF_T_PFX"d", ti->frq_ptr,
              tw->last_term_info.frq_ptr);
    }
    if (ti->prx_ptr < tw->last_term_info.prx_ptr) {
        FRT_RAISE(FRT_STATE_ERROR, "%"FRT_OFF_T_PFX"d > %"FRT_OFF_T_PFX"d", ti->prx_ptr,
              tw->last_term_info.prx_ptr);
    }
#endif

    tw_write_term(tw, os, term, term_len);  /* write term */
    frt_os_write_vint(os, ti->doc_freq);        /* write doc freq */
    frt_os_write_voff_t(os, ti->frq_ptr - tw->last_term_info.frq_ptr);
    frt_os_write_voff_t(os, ti->prx_ptr - tw->last_term_info.prx_ptr);
    if (ti->doc_freq >= skip_interval) {
        frt_os_write_voff_t(os, ti->skip_offset);
    }

    tw->last_term_info = *ti;
    tw->counter++;
}

void frt_tiw_add(FrtTermInfosWriter *tiw,
             const char *term,
             int term_len,
             FrtTermInfo *ti)
{
    off_t tis_pos;

    if (0 == (tiw->tis_writer->counter % tiw->index_interval)) {
        /* add an index term */
        tw_add(tiw->tix_writer,
               tiw->tis_writer->last_term,
               strlen(tiw->tis_writer->last_term),
               &(tiw->tis_writer->last_term_info),
               tiw->skip_interval);
        tis_pos = frt_os_pos(tiw->tis_writer->os);
        frt_os_write_voff_t(tiw->tix_writer->os, tis_pos - tiw->last_index_ptr);
        tiw->last_index_ptr = tis_pos;  /* write ptr */
    }

    tw_add(tiw->tis_writer, term, term_len, ti, tiw->skip_interval);
}

static void tw_reset(FrtTermWriter *tw)
{
    tw->counter = 0;
    tw->last_term = FRT_EMPTY_STRING;
    FRT_ZEROSET(&(tw->last_term_info), FrtTermInfo);
}

void frt_tiw_start_field(FrtTermInfosWriter *tiw, int field_num)
{
    FrtOutStream *tfx_out = tiw->tfx_out;
    frt_os_write_vint(tfx_out, tiw->tix_writer->counter);    /* write tix size */
    frt_os_write_vint(tfx_out, tiw->tis_writer->counter);    /* write tis size */
    frt_os_write_vint(tfx_out, field_num);
    frt_os_write_voff_t(tfx_out, frt_os_pos(tiw->tix_writer->os)); /* write tix ptr */
    frt_os_write_voff_t(tfx_out, frt_os_pos(tiw->tis_writer->os)); /* write tis ptr */
    tw_reset(tiw->tix_writer);
    tw_reset(tiw->tis_writer);
    tiw->last_index_ptr = 0;
    tiw->field_count++;
}

void frt_tiw_close(FrtTermInfosWriter *tiw)
{
    FrtOutStream *tfx_out = tiw->tfx_out;
    frt_os_write_vint(tfx_out, tiw->tix_writer->counter);
    frt_os_write_vint(tfx_out, tiw->tis_writer->counter);
    frt_os_seek(tfx_out, 0);
    frt_os_write_u32(tfx_out, tiw->field_count);
    frt_os_close(tfx_out);

    tw_close(tiw->tix_writer);
    tw_close(tiw->tis_writer);

    free(tiw);
}

/****************************************************************************
 *
 * TermDocEnum
 *
 ****************************************************************************/

/****************************************************************************
 * SegmentTermDocEnum
 ****************************************************************************/

#define STDE(tde) ((FrtSegmentTermDocEnum *)(tde))
#define TDE(stde) ((FrtTermDocEnum *)(stde))

#define CHECK_STATE(method) do {\
    if (0 == STDE(tde)->count) {\
        FRT_RAISE(FRT_STATE_ERROR, "Illegal state of TermDocEnum. You must call #next "\
              "before you call #"method);\
    }\
} while (0)

static void stde_seek_ti(FrtSegmentTermDocEnum *stde, FrtTermInfo *ti)
{
    if (NULL == ti) {
        stde->doc_freq = 0;
    } else {
        stde->count = 0;
        stde->doc_freq = ti->doc_freq;
        stde->doc_num = 0;
        stde->skip_doc = 0;
        stde->skip_count = 0;
        stde->num_skips = stde->doc_freq / stde->skip_interval;
        stde->frq_ptr = ti->frq_ptr;
        stde->prx_ptr = ti->prx_ptr;
        stde->skip_ptr = ti->frq_ptr + ti->skip_offset;
        frt_is_seek(stde->frq_in, ti->frq_ptr);
        stde->have_skipped = false;
    }
}

static void stde_seek(FrtTermDocEnum *tde, int field_num, const char *term)
{
    FrtTermInfo *ti = tir_get_ti_field(STDE(tde)->tir, field_num, term);
    stde_seek_ti(STDE(tde), ti);
}

static void stde_seek_te(FrtTermDocEnum *tde, FrtTermEnum *te)
{
#ifdef DEBUG
    if (te->set_field != &ste_set_field) {
        FRT_RAISE(FRT_ARG_ERROR, "Passed an incorrect TermEnum type");
    }
#endif
    stde_seek_ti(STDE(tde), &(te->curr_ti));
}

static int stde_doc_num(FrtTermDocEnum *tde)
{
    CHECK_STATE("doc_num");
    return STDE(tde)->doc_num;
}

static int stde_freq(FrtTermDocEnum *tde)
{
    CHECK_STATE("freq");
    return STDE(tde)->freq;
}

static bool stde_next(FrtTermDocEnum *tde)
{
    int doc_code;
    FrtSegmentTermDocEnum *stde = STDE(tde);

    while (true) {
        if (stde->count >= stde->doc_freq) {
            return false;
        }

        doc_code = frt_is_read_vint(stde->frq_in);
        stde->doc_num += doc_code >> 1;    /* shift off low bit */
        if (0 != (doc_code & 1)) {         /* if low bit is set */
            stde->freq = 1;                /* freq is one */
        } else {
            stde->freq = (int)frt_is_read_vint(stde->frq_in); /* read freq */
        }

        stde->count++;

        if (NULL == stde->deleted_docs || 0 == frt_bv_get(stde->deleted_docs, stde->doc_num)) {
            break; /* We found an undeleted doc so return */
        }

        stde->skip_prox(stde);
    }
    return true;
}

static int stde_read(FrtTermDocEnum *tde, int *docs, int *freqs, int req_num)
{
    FrtSegmentTermDocEnum *stde = STDE(tde);
    int i = 0;
    int doc_code;

    while (i < req_num && stde->count < stde->doc_freq) {
        /* manually inlined call to next() for speed */
        doc_code = frt_is_read_vint(stde->frq_in);
        stde->doc_num += (doc_code >> 1);            /* shift off low bit */
        if (0 != (doc_code & 1)) {                   /* if low bit is set */
            stde->freq = 1;                            /* freq is one */
        } else {
            stde->freq = frt_is_read_vint(stde->frq_in);  /* else read freq */
        }

        stde->count++;

        if (NULL == stde->deleted_docs
            || 0 == frt_bv_get(stde->deleted_docs, stde->doc_num)) {
            docs[i] = stde->doc_num;
            freqs[i] = stde->freq;
            i++;
        }
    }
    return i;
}

static bool stde_skip_to(FrtTermDocEnum *tde, int target_doc_num)
{
    FrtSegmentTermDocEnum *stde = STDE(tde);

    if (stde->doc_freq >= stde->skip_interval
        && target_doc_num > stde->doc_num) {       /* optimized case */
        int last_skip_doc;
        off_t last_frq_ptr;
        off_t last_prx_ptr;
        int num_skipped;

        if (NULL == stde->skip_in) {
            stde->skip_in = frt_is_clone(stde->frq_in);/* lazily clone */
        }

        if (!stde->have_skipped) {                 /* lazily seek skip stream */
            frt_is_seek(stde->skip_in, stde->skip_ptr);
            stde->have_skipped = true;
        }

        /* scan skip data */
        last_skip_doc = stde->skip_doc;
        last_frq_ptr = frt_is_pos(stde->frq_in);
        last_prx_ptr = -1;
        num_skipped = -1 - (stde->count % stde->skip_interval);

        while (target_doc_num > stde->skip_doc) {
            last_skip_doc = stde->skip_doc;
            last_frq_ptr = stde->frq_ptr;
            last_prx_ptr = stde->prx_ptr;

            if (0 != stde->skip_doc && stde->skip_doc >= stde->doc_num) {
                num_skipped += stde->skip_interval;
            }

            if (stde->skip_count >= stde->num_skips) {
                break;
            }

            stde->skip_doc += frt_is_read_vint(stde->skip_in);
            stde->frq_ptr  += frt_is_read_vint(stde->skip_in);
            stde->prx_ptr  += frt_is_read_vint(stde->skip_in);

            stde->skip_count++;
        }

        /* if we found something to skip, skip it */
        if (last_frq_ptr > frt_is_pos(stde->frq_in)) {
            frt_is_seek(stde->frq_in, last_frq_ptr);
            stde->seek_prox(stde, last_prx_ptr);

            stde->doc_num = last_skip_doc;
            stde->count += num_skipped;
        }
    }

    /* done skipping, now just scan */
    do {
        if (!tde->next(tde)) {
            return false;
        }
    } while (target_doc_num > stde->doc_num);
    return true;
}

static void stde_close(FrtTermDocEnum *tde)
{
    frt_is_close(STDE(tde)->frq_in);

    if (NULL != STDE(tde)->skip_in) {
        frt_is_close(STDE(tde)->skip_in);
    }

    free(tde);
}

static void stde_skip_prox(FrtSegmentTermDocEnum *stde)
{
    (void)stde;
}

static void stde_seek_prox(FrtSegmentTermDocEnum *stde, off_t prx_ptr)
{
    (void)stde;
    (void)prx_ptr;
}


FrtTermDocEnum *frt_stde_new(FrtTermInfosReader *tir,
                      FrtInStream *frq_in,
                      FrtBitVector *deleted_docs,
                      int skip_interval)
{
    FrtSegmentTermDocEnum *stde = FRT_ALLOC_AND_ZERO(FrtSegmentTermDocEnum);
    FrtTermDocEnum *tde         = (FrtTermDocEnum *)stde;

    /* TermDocEnum methods */
    tde->seek                = &stde_seek;
    tde->seek_te             = &stde_seek_te;
    tde->doc_num             = &stde_doc_num;
    tde->freq                = &stde_freq;
    tde->next                = &stde_next;
    tde->read                = &stde_read;
    tde->skip_to             = &stde_skip_to;
    tde->next_position       = NULL;
    tde->close               = &stde_close;

    /* SegmentTermDocEnum methods */
    stde->skip_prox          = &stde_skip_prox;
    stde->seek_prox          = &stde_seek_prox;

    /* Attributes */
    stde->tir                = tir;
    stde->frq_in             = frt_is_clone(frq_in);
    stde->deleted_docs       = deleted_docs;
    stde->skip_interval      = skip_interval;

    return tde;
}

/****************************************************************************
 * SegmentTermPosEnum
 ****************************************************************************/

static void stpe_seek_ti(FrtSegmentTermDocEnum *stde, FrtTermInfo *ti)
{
    if (NULL == ti) {
        stde->doc_freq = 0;
    }
    else {
        stde_seek_ti(stde, ti);
        frt_is_seek(stde->prx_in, ti->prx_ptr);
    }
}

static void stpe_seek(FrtTermDocEnum *tde, int field_num, const char *term)
{
    FrtSegmentTermDocEnum *stde = STDE(tde);
    FrtTermInfo *ti = tir_get_ti_field(stde->tir, field_num, term);
    stpe_seek_ti(stde, ti);
    stde->prx_cnt = 0;
}

static bool stpe_next(FrtTermDocEnum *tde)
{
    FrtSegmentTermDocEnum *stde = STDE(tde);
    frt_is_skip_vints(stde->prx_in, stde->prx_cnt);

    /* if super */
    if (stde_next(tde)) {
        stde->prx_cnt = stde->freq;
        stde->position = 0;
        return true;
    } else {
        stde->prx_cnt = stde->position = 0;
        return false;
    }
}

static int stpe_read(FrtTermDocEnum *tde, int *docs, int *freqs, int req_num)
{
    (void)tde; (void)docs; (void)freqs; (void)req_num;
    FRT_RAISE(FRT_ARG_ERROR, "TermPosEnum does not handle processing multiple documents"
                     " in one call. Use TermDocEnum instead.");
    return -1;
}

static int stpe_next_position(FrtTermDocEnum *tde)
{
    FrtSegmentTermDocEnum *stde = STDE(tde);
    return (stde->prx_cnt-- > 0) ? stde->position += frt_is_read_vint(stde->prx_in)
                                 : -1;
}

static void stpe_close(FrtTermDocEnum *tde)
{
    frt_is_close(STDE(tde)->prx_in);
    STDE(tde)->prx_in = NULL;
    stde_close(tde);
}

static void stpe_skip_prox(FrtSegmentTermDocEnum *stde)
{
    frt_is_skip_vints(stde->prx_in, stde->freq);
}

static void stpe_seek_prox(FrtSegmentTermDocEnum *stde, off_t prx_ptr)
{
    frt_is_seek(stde->prx_in, prx_ptr);
    stde->prx_cnt = 0;
}

FrtTermDocEnum *frt_stpe_new(FrtTermInfosReader *tir,
                      FrtInStream *frq_in,
                      FrtInStream *prx_in,
                      FrtBitVector *del_docs,
                      int skip_interval)
{
    FrtTermDocEnum *tde         = frt_stde_new(tir, frq_in, del_docs, skip_interval);
    FrtSegmentTermDocEnum *stde = STDE(tde);

    /* TermDocEnum methods */
    tde->seek                = &stpe_seek;
    tde->next                = &stpe_next;
    tde->read                = &stpe_read;
    tde->next_position       = &stpe_next_position;
    tde->close               = &stpe_close;

    /* SegmentTermDocEnum methods */
    stde->skip_prox          = &stpe_skip_prox;
    stde->seek_prox          = &stpe_seek_prox;

    /* Attributes */
    stde->prx_in             = frt_is_clone(prx_in);
    stde->prx_cnt            = 0;
    stde->position           = 0;

    return tde;
}

/****************************************************************************
 * MultiTermDocEnum
 ****************************************************************************/

#define MTDE(tde) ((MultiTermDocEnum *)(tde))

typedef struct MultiTermDocEnum
{
    FrtTermDocEnum tde;
    int *starts;
    int base;
    int ptr;
    int ir_cnt;
    char *state;
    FrtTermEnum *te;
    FrtIndexReader **irs;
    FrtTermDocEnum **irs_tde;
    FrtTermDocEnum *curr_tde;
} MultiTermDocEnum;

static FrtTermDocEnum *mtde_next_tde(MultiTermDocEnum *mtde)
{
    mtde->ptr++;
    while (mtde->ptr < mtde->ir_cnt && !mtde->state[mtde->ptr]) {
        mtde->ptr++;
    }
    if (mtde->ptr >= mtde->ir_cnt) {
        return mtde->curr_tde = NULL;
    }
    else {
        FrtTermDocEnum *tde = mtde->irs_tde[mtde->ptr];
        mtde->base = mtde->starts[mtde->ptr];
        return mtde->curr_tde = tde;
    }
}

#define CHECK_CURR_TDE(method) do {\
    if (NULL == MTDE(tde)->curr_tde) {\
        FRT_RAISE(FRT_STATE_ERROR, "Illegal state of TermDocEnum. You must call #next "\
              "before you call #"method);\
    }\
} while (0)

static void mtde_seek_te(FrtTermDocEnum *tde, FrtTermEnum *te)
{
    int i;
    MultiTermDocEnum *mtde = MTDE(tde);
    memset(mtde->state, 0, mtde->ir_cnt);
    for (i = MTE(te)->ti_cnt - 1; i >= 0; i--) {
        int index = MTE(te)->ti_indexes[i];
        FrtTermDocEnum *tde = mtde->irs_tde[index];
        mtde->state[index] = 1;
        if (tde->close == stde_close) {
            stde_seek_ti(STDE(tde), MTE(te)->tis + i);
        }
        else if (tde->close == stpe_close) {
            stpe_seek_ti(STDE(tde), MTE(te)->tis + i);
        }
        else {
            tde->seek(tde, MTE(te)->tews[index].te->field_num, te->curr_term);
        }
    }
    mtde->base = 0;
    mtde->ptr = -1;
    mtde_next_tde(mtde);
}

static void mtde_seek(FrtTermDocEnum *tde, int field_num, const char *term)
{
    MultiTermDocEnum *mtde = MTDE(tde);
    FrtTermEnum *te = mtde->te;
    char *t;
    te->set_field(te, field_num);
    if (NULL != (t = te->skip_to(te, term)) && 0 == strcmp(term, t)) {
        mtde_seek_te(tde, te);
    }
    else {
        memset(mtde->state, 0, mtde->ir_cnt);
    }
}

static int mtde_doc_num(FrtTermDocEnum *tde)
{
    CHECK_CURR_TDE("doc_num");
    return MTDE(tde)->base + MTDE(tde)->curr_tde->doc_num(MTDE(tde)->curr_tde);
}

static int mtde_freq(FrtTermDocEnum *tde)
{
    CHECK_CURR_TDE("freq");
    return MTDE(tde)->curr_tde->freq(MTDE(tde)->curr_tde);
}

static bool mtde_next(FrtTermDocEnum *tde)
{
    MultiTermDocEnum *mtde = MTDE(tde);
    if (NULL != mtde->curr_tde && mtde->curr_tde->next(mtde->curr_tde)) {
        return true;
    }
    else if (mtde_next_tde(mtde)) {
        return mtde_next(tde);
    }
    else {
        return false;
    }
}

static int mtde_read(FrtTermDocEnum *tde, int *docs, int *freqs, int req_num)
{
    int i, end = 0, last_end = 0, b;
    MultiTermDocEnum *mtde = MTDE(tde);
    while (true) {
        if (NULL == mtde->curr_tde) return end;
        end += mtde->curr_tde->read(mtde->curr_tde, docs + last_end,
                                    freqs + last_end, req_num - last_end);
        if (end == last_end) {              /* none left in segment */
            if (!mtde_next_tde(mtde)) return end;
        }
        else {                            /* got some */
            b = mtde->base;                 /* adjust doc numbers */
            for (i = last_end; i < end; i++) {
                docs[i] += b;
            }
            if (end == req_num) {
                return end;
            }
            else {
                last_end = end;
            }
        }
    }
}

static bool mtde_skip_to(FrtTermDocEnum *tde, int target_doc_num)
{
    MultiTermDocEnum *mtde = MTDE(tde);
    FrtTermDocEnum *curr_tde;
    while (NULL != (curr_tde = mtde->curr_tde)) {
        if (target_doc_num < mtde->starts[mtde->ptr + 1] &&
            (curr_tde->skip_to(curr_tde, target_doc_num - mtde->base))) {
            return true;
        }

        mtde_next_tde(mtde);
    }
    return false;
}

static void mtde_close(FrtTermDocEnum *tde)
{
    MultiTermDocEnum *mtde = MTDE(tde);
    FrtTermDocEnum *tmp_tde;
    int i = mtde->ir_cnt;
    mtde->te->close(mtde->te);
    while (i > 0) {
        i--;
        tmp_tde = mtde->irs_tde[i];
        tmp_tde->close(tmp_tde);
    }
    free(mtde->irs_tde);
    free(mtde->state);
    free(tde);
}

static FrtTermDocEnum *mtxe_new(FrtMultiReader *mr)
{
    MultiTermDocEnum *mtde  = FRT_ALLOC_AND_ZERO(MultiTermDocEnum);
    FrtTermDocEnum *tde        = TDE(mtde);
    tde->seek               = &mtde_seek;
    tde->seek_te            = &mtde_seek_te;
    tde->doc_num            = &mtde_doc_num;
    tde->freq               = &mtde_freq;
    tde->next               = &mtde_next;
    tde->read               = &mtde_read;
    tde->skip_to            = &mtde_skip_to;
    tde->close              = &mtde_close;

    mtde->state             = FRT_ALLOC_AND_ZERO_N(char, mr->r_cnt);
    mtde->te                = ((FrtIndexReader *)mr)->terms((FrtIndexReader *)mr, 0);
    mtde->starts            = mr->starts;
    mtde->ir_cnt            = mr->r_cnt;
    mtde->irs               = mr->sub_readers;
    mtde->irs_tde           = FRT_ALLOC_AND_ZERO_N(FrtTermDocEnum *, mr->r_cnt);

    return tde;
}

static FrtTermDocEnum *mtde_new(FrtMultiReader *mr)
{
    int i;
    FrtTermDocEnum *tde        = mtxe_new(mr);
    tde->next_position      = NULL;
    for (i = mr->r_cnt - 1; i >= 0; i--) {
        FrtIndexReader *ir = mr->sub_readers[i];
        MTDE(tde)->irs_tde[i] = ir->term_docs(ir);
    }
    return tde;
}

/****************************************************************************
 * MultiTermPosEnum
 ****************************************************************************/

static int mtpe_next_position(FrtTermDocEnum *tde)
{
    CHECK_CURR_TDE("next_position");
    return MTDE(tde)->curr_tde->next_position(MTDE(tde)->curr_tde);
}

static FrtTermDocEnum *mtpe_new(FrtMultiReader *mr)
{
    int i;
    FrtTermDocEnum *tde        = mtxe_new(mr);
    tde->next_position      = &mtpe_next_position;
    for (i = mr->r_cnt - 1; i >= 0; i--) {
        FrtIndexReader *ir = mr->sub_readers[i];
        MTDE(tde)->irs_tde[i] = ir->term_positions(ir);
    }
    return tde;
}

/****************************************************************************
 * MultipleTermDocPosEnum
 *
 * This enumerator is used by MultiPhraseQuery
 ****************************************************************************/

#define MTDPE(tde) ((MultipleTermDocPosEnum *)(tde))
#define  MTDPE_POS_QUEUE_INIT_CAPA 8

typedef struct
{
    FrtTermDocEnum tde;
    int doc_num;
    int freq;
    FrtPriorityQueue *pq;
    int *pos_queue;
    int pos_queue_index;
    int pos_queue_capa;
    int field_num;
} MultipleTermDocPosEnum;

static void tde_destroy(FrtTermDocEnum *tde) {
    tde->close(tde);
}

static void mtdpe_seek(FrtTermDocEnum *tde, int field_num, const char *term)
{
    (void)tde;
    (void)field_num;
    (void)term;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "MultipleTermDocPosEnum does not support "
          " the #seek operation");
}

static int mtdpe_doc_num(FrtTermDocEnum *tde)
{
    return MTDPE(tde)->doc_num;
}

static int mtdpe_freq(FrtTermDocEnum *tde)
{
    return MTDPE(tde)->freq;
}

static bool mtdpe_next(FrtTermDocEnum *tde)
{
    FrtTermDocEnum *sub_tde;
    int pos = 0, freq = 0;
    int doc;
    MultipleTermDocPosEnum *mtdpe = MTDPE(tde);

    if (0 == mtdpe->pq->size) {
        return false;
    }

    sub_tde = (FrtTermDocEnum *)frt_pq_top(mtdpe->pq);
    doc = sub_tde->doc_num(sub_tde);

    do {
        freq += sub_tde->freq(sub_tde);
        if (freq > mtdpe->pos_queue_capa) {
            do {
                mtdpe->pos_queue_capa <<= 1;
            } while (freq > mtdpe->pos_queue_capa);
            FRT_REALLOC_N(mtdpe->pos_queue, int, mtdpe->pos_queue_capa);
        }

        /* pos starts from where it was up to last time */
        for (; pos < freq; pos++) {
            mtdpe->pos_queue[pos] = sub_tde->next_position(sub_tde);
        }

        if (sub_tde->next(sub_tde)) {
            frt_pq_down(mtdpe->pq);
        }
        else {
            sub_tde = (FrtTermDocEnum *)frt_pq_pop(mtdpe->pq);
            sub_tde->close(sub_tde);
        }
        sub_tde = (FrtTermDocEnum *)frt_pq_top(mtdpe->pq);
    } while ((mtdpe->pq->size > 0) && (sub_tde->doc_num(sub_tde) == doc));

    qsort(mtdpe->pos_queue, freq, sizeof(int), &frt_icmp_risky);

    mtdpe->pos_queue_index = 0;
    mtdpe->freq = freq;
    mtdpe->doc_num = doc;

    return true;
}

static bool tdpe_less_than(FrtTermDocEnum *p1, FrtTermDocEnum *p2)
{
    return p1->doc_num(p1) < p2->doc_num(p2);
}

static bool mtdpe_skip_to(FrtTermDocEnum *tde, int target_doc_num)
{
    FrtTermDocEnum *sub_tde;
    FrtPriorityQueue *mtdpe_pq = MTDPE(tde)->pq;

    while (NULL != (sub_tde = (FrtTermDocEnum *)frt_pq_top(mtdpe_pq))
           && (target_doc_num > sub_tde->doc_num(sub_tde))) {
        if (sub_tde->skip_to(sub_tde, target_doc_num)) {
            frt_pq_down(mtdpe_pq);
        }
        else {
            sub_tde = (FrtTermDocEnum *)frt_pq_pop(mtdpe_pq);
            sub_tde->close(sub_tde);
        }
    }
    return tde->next(tde);
}

static int mtdpe_read(FrtTermDocEnum *tde, int *docs, int *freqs, int req_num)
{
    (void)tde;
    (void)docs;
    (void)freqs;
    FRT_RAISE(FRT_UNSUPPORTED_ERROR, "MultipleTermDocPosEnum does not support "
          " the #read operation");
    return req_num;
}

static int mtdpe_next_position(FrtTermDocEnum *tde)
{
    return MTDPE(tde)->pos_queue[MTDPE(tde)->pos_queue_index++];
}

static void mtdpe_close(FrtTermDocEnum *tde)
{
    frt_pq_clear(MTDPE(tde)->pq);
    frt_pq_destroy(MTDPE(tde)->pq);
    free(MTDPE(tde)->pos_queue);
    free(tde);
}

FrtTermDocEnum *frt_mtdpe_new(FrtIndexReader *ir, int field_num, char **terms, int t_cnt)
{
    int i;
    MultipleTermDocPosEnum *mtdpe = FRT_ALLOC_AND_ZERO(MultipleTermDocPosEnum);
    FrtTermDocEnum *tde = TDE(mtdpe);
    FrtPriorityQueue *pq;

    pq = mtdpe->pq = frt_pq_new(t_cnt, (frt_lt_ft)&tdpe_less_than, (frt_free_ft)&tde_destroy);
    mtdpe->pos_queue_capa = MTDPE_POS_QUEUE_INIT_CAPA;
    mtdpe->pos_queue = FRT_ALLOC_N(int, MTDPE_POS_QUEUE_INIT_CAPA);
    mtdpe->field_num = field_num;
    for (i = 0; i < t_cnt; i++) {
        FrtTermDocEnum *tpe = ir->term_positions(ir);
        tpe->seek(tpe, field_num, terms[i]);
        if (tpe->next(tpe)) {
            frt_pq_push(pq, tpe);
        }
        else {
            tpe->close(tpe);
        }
    }
    tde->close          = &mtdpe_close;
    tde->seek           = &mtdpe_seek;
    tde->next           = &mtdpe_next;
    tde->doc_num        = &mtdpe_doc_num;
    tde->freq           = &mtdpe_freq;
    tde->skip_to        = &mtdpe_skip_to;
    tde->read           = &mtdpe_read;
    tde->next_position  = &mtdpe_next_position;

    return tde;
}

/****************************************************************************
 *
 * FileNameFilter
 *
 ****************************************************************************/

static FrtHash *fn_extensions = NULL;
static void file_name_filter_init()
{
    int i;
    fn_extensions = frt_h_new_str((frt_free_ft)NULL, (frt_free_ft)NULL);
    for (i = 0; i < FRT_NELEMS(INDEX_EXTENSIONS); i++) {
      frt_h_set(fn_extensions, INDEX_EXTENSIONS[i], (char *)INDEX_EXTENSIONS[i]);
    }
    frt_register_for_cleanup(fn_extensions, (frt_free_ft)&frt_h_destroy);
}

bool frt_file_name_filter_is_index_file(const char *file_name, bool include_locks)
{
    char *p = strrchr(file_name, '.');
    if (NULL == fn_extensions) file_name_filter_init();
    if (NULL != p) {
        char *extension = p + 1;
        if (NULL != frt_h_get(fn_extensions, extension)) {
            return true;
        }
        else if ((*extension == 'f' || *extension == 's')
                 && *(extension + 1) >= '0'
                 && *(extension + 1) <= '9') {
            return true;
        }
        else if (include_locks && (strcmp(extension, "lck") == 0)
                               && (strncmp(file_name, "ferret", 6) == 0)) {
            return true;
        }
    }
    else if (0 == strncmp(FRT_SEGMENTS_FILE_NAME, file_name,
                          sizeof(FRT_SEGMENTS_FILE_NAME) - 1)) {
        return true;
    }
    return false;
}

/*
 * Returns true if this is a file that would be contained in a CFS file.  This
 * function should only be called on files that pass the above "accept" (ie,
 * are already known to be a Lucene index file).
 */
static bool file_name_filter_is_cfs_file(const char *file_name) {
    char *p = strrchr(file_name, '.');
    if (NULL != p) {
        char *extension = p + 1;
        if (NULL != frt_h_get(fn_extensions, extension)
            && 0 != strcmp(extension, "del")
            && 0 != strcmp(extension, "gen")
            && 0 != strcmp(extension, "cfs")) {
            return true;
        }
        else if ('f' == *extension
                 && '0' <= *(extension + 1)
                 && '9' >= *(extension + 1)) {
            return true;
        }
    }
    return false;
}

/****************************************************************************
 *
 * Deleter
 *
 ****************************************************************************/

#define DELETABLE_START_CAPA 8
FrtDeleter *frt_deleter_new(FrtSegmentInfos *sis, FrtStore *store)
{
    FrtDeleter *dlr = FRT_ALLOC(FrtDeleter);
    dlr->sis = sis;
    dlr->store = store;
    dlr->pending = frt_hs_new_str(&free);
    return dlr;
}

void frt_deleter_destroy(FrtDeleter *dlr)
{
    frt_hs_destroy(dlr->pending);
    free(dlr);
}

static void deleter_queue_file(FrtDeleter *dlr, const char *file_name)
{
    frt_hs_add(dlr->pending, frt_estrdup(file_name));
}

void frt_deleter_delete_file(FrtDeleter *dlr, char *file_name)
{
    FrtStore *store = dlr->store;
    FRT_TRY
        if (store->exists(store, file_name)) {
            store->remove(store, file_name);
        }
        frt_hs_del(dlr->pending, file_name);
    FRT_XCATCHALL
        frt_hs_add(dlr->pending, frt_estrdup(file_name));
    FRT_XENDTRY
}

static void deleter_commit_pending_deletions(FrtDeleter *dlr)
{
    FrtHashSetEntry *hse, *hse_next = dlr->pending->first;
    while ((hse = hse_next) != NULL) {
        hse_next = hse->next;
        frt_deleter_delete_file(dlr, (char *)hse->elem);
    }
}

void frt_deleter_delete_files(FrtDeleter *dlr, char **files, int file_cnt)
{
    int i;
    for (i = file_cnt - 1; i >= 0; i--) {
        deleter_queue_file(dlr, files[i]);
    }
    deleter_commit_pending_deletions(dlr);
}

struct DelFilesArg {
    char  curr_seg_file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtDeleter *dlr;
    FrtHash *current;
};

static void frt_deleter_find_deletable_files_i(const char *file_name, void *arg)
{
    struct DelFilesArg *dfa = (struct DelFilesArg *)arg;
    FrtDeleter *dlr = dfa->dlr;

    if (frt_file_name_filter_is_index_file(file_name, false)
        && 0 != strcmp(file_name, dfa->curr_seg_file_name)
        && 0 != strcmp(file_name, SEGMENTS_GEN_FILE_NAME)) {

        bool do_delete = false;
        FrtSegmentInfo *si;
        char segment_name[FRT_SEGMENT_NAME_MAX_LENGTH];
        char *extension, *p;
        strcpy(segment_name, file_name);

        p = strrchr(segment_name, '.');

        /* First remove any extension: */
        if (NULL != p) {
            *p = '\0';
            extension = p + 1;
        }
        else {
            extension = NULL;
        }

        /* Then, remove any generation count: */
        p = strrchr(segment_name + 1, '_');
        if (NULL != p) {
            *p = '\0';
        }

        /* Delete this file if it's not a "current" segment, or, it is a
         * single index file but there is now a corresponding compound file: */
        if (NULL == (si = (FrtSegmentInfo *)frt_h_get(dfa->current, segment_name))) {
            /* Delete if segment is not referenced: */
            do_delete = true;
        }
        else {
            char tmp_fn[FRT_SEGMENT_NAME_MAX_LENGTH];
            /* OK, segment is referenced, but file may still be orphan'd: */
            if (file_name_filter_is_cfs_file(file_name)
                && si->use_compound_file) {
                /* This file is stored in a CFS file for this segment: */
                do_delete = true;
            }
            else if (0 == strcmp("del", extension)) {
                /* This is a _segmentName_N.del file: */
                if (!frt_fn_for_generation(tmp_fn, segment_name, "del", si->del_gen)
                    || 0 != strcmp(file_name, tmp_fn)) {
                    /* If this is a seperate .del file, but it
                     * doesn't match the current del file name for
                     * this segment, then delete it: */
                    do_delete = true;
                }
            }
            else if (NULL != extension
                     && ('s' == *extension || 'f' == *extension)
                     && isdigit(extension[1])) {
                si_norm_file_name(si, tmp_fn, atoi(extension + 1));
                /* This is a _segmentName_N.sX file: */
                if (0 != strcmp(tmp_fn, file_name)) {
                    /* This is an orphan'd norms file: */
                    do_delete = true;
                }
            }
            else if (0 == strcmp("cfs", extension) && !si->use_compound_file) {
                /* This is a partially written _segmentName.cfs: */
                do_delete = true;
            }
        }

        if (do_delete) {
            deleter_queue_file(dlr, file_name);
        }
    }
}

/*
 * Determine index files that are no longer referenced and therefore should be
 * deleted.  This is called once (by the writer), and then subsequently we add
 * onto deletable any files that are no longer needed at the point that we
 * create the unused file (eg when merging segments), and we only remove from
 * deletable when a file is successfully deleted.
 */
void frt_deleter_find_deletable_files(FrtDeleter *dlr)
{
    /* Gather all "current" segments: */
    int i;
    FrtSegmentInfos *sis = dlr->sis;
    FrtStore *store = dlr->store;
    struct DelFilesArg dfa;
    FrtHash *current = dfa.current
                       = frt_h_new_str((frt_free_ft)NULL, (frt_free_ft)frt_si_deref);
    dfa.dlr = dlr;

    for(i = 0; i < sis->size; i++) {
        FrtSegmentInfo *si = (FrtSegmentInfo *)sis->segs[i];
        FRT_REF(si);
        frt_h_set(current, si->name, si);
    }

    /* Then go through all files in the Directory that are Ferret index files,
     * and add to deletable if they are not referenced by the current segments
     * info: */
    frt_sis_curr_seg_file_name(dfa.curr_seg_file_name, store);

    store->each(store, &frt_deleter_find_deletable_files_i, &dfa);
    frt_h_destroy(dfa.current);
}

static void deleter_delete_deletable_files(FrtDeleter *dlr)
{
    frt_deleter_find_deletable_files(dlr);
    deleter_commit_pending_deletions(dlr);
}

/*
TODO: currently not used. Why not?
static void deleter_clear_pending_deletions(FrtDeleter *dlr)
{
    frt_hs_clear(dlr->pending);
}
*/

/****************************************************************************
 *
 * IndexReader
 *
 ****************************************************************************/

static void ir_acquire_not_necessary(FrtIndexReader *ir)
{
    (void)ir;
}

#define I64_PFX POSH_I64_PRINTF_PREFIX
static void ir_acquire_write_lock(FrtIndexReader *ir)
{
    if (ir->is_stale) {
        FRT_RAISE(FRT_STATE_ERROR, "IndexReader out of date and no longer valid for "
                           "delete, undelete, or set_norm operations. To "
                           "perform any of these operations on the index you "
                           "need to close and reopen the index");
    }

    if (NULL == ir->write_lock) {
        ir->write_lock = frt_open_lock(ir->store, FRT_WRITE_LOCK_NAME);
        if (!ir->write_lock->obtain(ir->write_lock)) {/* obtain write lock */
            FRT_RAISE(FRT_LOCK_ERROR, "Could not obtain write lock when trying to "
                              "write changes to the index. Check that there "
                              "are no stale locks in the index. Look for "
                              "files with the \".lck\" prefix. If you know "
                              "there are no processes writing to the index "
                              "you can safely delete these files.");
        }

        /* we have to check whether index has changed since this reader was
         * opened.  if so, this reader is no longer valid for deletion */
        if (frt_sis_read_current_version(ir->store) > ir->sis->version) {
            ir->is_stale = true;
            ir->write_lock->release(ir->write_lock);
            frt_close_lock(ir->write_lock);
            ir->write_lock = NULL;
            FRT_RAISE(FRT_STATE_ERROR, "IndexReader out of date and no longer valid "
                               "for delete, undelete, or set_norm operations. "
                               "The current version is <%"I64_PFX"d>, but this "
                               "readers version is <%"I64_PFX"d>. To perform "
                               "any of these operations on the index you need "
                               "to close and reopen the index",
                               frt_sis_read_current_version(ir->store),
                               ir->sis->version);
        }
    }
}

static FrtIndexReader *ir_setup(FrtIndexReader *ir, FrtStore *store, FrtSegmentInfos *sis,
                      FrtFieldInfos *fis, int is_owner)
{
    frt_mutex_init(&ir->mutex, NULL);
    frt_mutex_init(&ir->field_index_mutex, NULL);

    if (store) {
        ir->store = store;
        FRT_REF(store);
    }
    ir->sis = sis;
    ir->fis = fis;
    ir->ref_cnt = 1;

    ir->is_owner = is_owner;
    if (is_owner) {
        ir->acquire_write_lock = &ir_acquire_write_lock;
    }
    else {
        ir->acquire_write_lock = &ir_acquire_not_necessary;
    }

    return ir;
}

int frt_ir_doc_freq(FrtIndexReader *ir, FrtSymbol field, const char *term)
{
    int field_num = frt_fis_get_field_num(ir->fis, field);
    if (field_num >= 0) {
        return ir->doc_freq(ir, field_num, term);
    }
    else {
        return 0;
    }
}

static void ir_set_norm_i(FrtIndexReader *ir, int doc_num, int field_num, frt_uchar val)
{
    frt_mutex_lock(&ir->mutex);
    ir->acquire_write_lock(ir);
    ir->set_norm_i(ir, doc_num, field_num, val);
    ir->has_changes = true;
    frt_mutex_unlock(&ir->mutex);
}

void frt_ir_set_norm(FrtIndexReader *ir, int doc_num, FrtSymbol field, frt_uchar val)
{
    int field_num = frt_fis_get_field_num(ir->fis, field);
    if (field_num >= 0) {
        ir_set_norm_i(ir, doc_num, field_num, val);
    }
}

frt_uchar *frt_ir_get_norms_i(FrtIndexReader *ir, int field_num)
{
    frt_uchar *norms = NULL;
    if (field_num >= 0) {
        norms = ir->get_norms(ir, field_num);
    }
    if (!norms) {
        if (NULL == ir->fake_norms) {
            ir->fake_norms = FRT_ALLOC_AND_ZERO_N(frt_uchar, ir->max_doc(ir));
        }
        norms = ir->fake_norms;
    }
    return norms;
}

frt_uchar *frt_ir_get_norms(FrtIndexReader *ir, FrtSymbol field)
{
    int field_num = frt_fis_get_field_num(ir->fis, field);
    return frt_ir_get_norms_i(ir, field_num);
}

frt_uchar *frt_ir_get_norms_into(FrtIndexReader *ir, FrtSymbol field, frt_uchar *buf)
{
    int field_num = frt_fis_get_field_num(ir->fis, field);
    if (field_num >= 0) {
        ir->get_norms_into(ir, field_num, buf);
    }
    else {
        memset(buf, 0, ir->max_doc(ir));
    }
    return buf;
}

void frt_ir_undelete_all(FrtIndexReader *ir)
{
    frt_mutex_lock(&ir->mutex);
    ir->acquire_write_lock(ir);
    ir->undelete_all_i(ir);
    ir->has_changes = true;
    frt_mutex_unlock(&ir->mutex);
}

void frt_ir_delete_doc(FrtIndexReader *ir, int doc_num)
{
    if (doc_num >= 0 && doc_num < ir->max_doc(ir)) {
        frt_mutex_lock(&ir->mutex);
        ir->acquire_write_lock(ir);
        ir->delete_doc_i(ir, doc_num);
        ir->has_changes = true;
        frt_mutex_unlock(&ir->mutex);
    }
}

FrtDocument *frt_ir_get_doc_with_term(FrtIndexReader *ir, FrtSymbol field, const char *term) {
    FrtTermDocEnum *tde = ir_term_docs_for(ir, field, term);
    FrtDocument *doc = NULL;

    if (tde) {
        if (tde->next(tde)) {
            doc = ir->get_doc(ir, tde->doc_num(tde));
        }
        tde->close(tde);
    }
    return doc;
}

FrtTermEnum *frt_ir_terms(FrtIndexReader *ir, FrtSymbol field)
{
    FrtTermEnum *te = NULL;
    int field_num = frt_fis_get_field_num(ir->fis, field);
    if (field_num >= 0) {
        te = ir->terms(ir, field_num);
    }
    return te;
}

FrtTermEnum *frt_ir_terms_from(FrtIndexReader *ir, FrtSymbol field,
                           const char *term)
{
    FrtTermEnum *te = NULL;
    int field_num = frt_fis_get_field_num(ir->fis, field);
    if (field_num >= 0) {
        te = ir->terms_from(ir, field_num, term);
    }
    return te;
}

FrtTermDocEnum *ir_term_docs_for(FrtIndexReader *ir, FrtSymbol field,
                              const char *term)
{
    int field_num = frt_fis_get_field_num(ir->fis, field);
    FrtTermDocEnum *tde = ir->term_docs(ir);
    if (field_num >= 0) {
        tde->seek(tde, field_num, term);
    }
    return tde;
}

FrtTermDocEnum *frt_ir_term_positions_for(FrtIndexReader *ir, FrtSymbol field,
                                   const char *term)
{
    int field_num = frt_fis_get_field_num(ir->fis, field);
    FrtTermDocEnum *tde = ir->term_positions(ir);
    if (field_num >= 0) {
        tde->seek(tde, field_num, term);
    }
    return tde;
}

static void ir_commit_i(FrtIndexReader *ir)
{
    if (ir->has_changes) {
        if (NULL == ir->deleter && NULL != ir->store) {
            /* In the MultiReader case, we share this deleter across all
             * SegmentReaders: */
            ir->set_deleter_i(ir, frt_deleter_new(ir->sis, ir->store));
        }
        if (ir->is_owner) {
            char curr_seg_fn[FRT_MAX_FILE_PATH];
            frt_mutex_lock(&ir->store->mutex);

            frt_sis_curr_seg_file_name(curr_seg_fn, ir->store);

            ir->commit_i(ir);
            frt_sis_write(ir->sis, ir->store, ir->deleter);

            if (ir->deleter) frt_deleter_delete_file(ir->deleter, curr_seg_fn);

            frt_mutex_unlock(&ir->store->mutex);

            if (NULL != ir->write_lock) {
                /* release write lock */
                ir->write_lock->release(ir->write_lock);
                frt_close_lock(ir->write_lock);
                ir->write_lock = NULL;
            }
        }
        else {
            ir->commit_i(ir);
        }
    }
    ir->has_changes = false;
}

void frt_ir_commit(FrtIndexReader *ir)
{
    frt_mutex_lock(&ir->mutex);
    ir_commit_i(ir);
    frt_mutex_unlock(&ir->mutex);
}

void frt_ir_close(FrtIndexReader *ir)
{
    frt_mutex_lock(&ir->mutex);
    if (0 == --(ir->ref_cnt)) {
        ir_commit_i(ir);
        ir->close_i(ir);
        if (ir->store) {
            frt_store_deref(ir->store);
        }
        if (ir->is_owner && ir->sis) {
            frt_sis_destroy(ir->sis);
        }
        if (ir->cache) {
            frt_h_destroy(ir->cache);
        }
        if (ir->field_index_cache) {
            frt_h_destroy(ir->field_index_cache);
        }
        if (ir->deleter && ir->is_owner) {
            frt_deleter_destroy(ir->deleter);
        }
        free(ir->fake_norms);

        frt_mutex_destroy(&ir->mutex);
        frt_mutex_destroy(&ir->field_index_mutex);
        free(ir);
    }
    else {
        frt_mutex_unlock(&ir->mutex);
    }

}

/**
 * Don't call this method if the cache already exists
 **/
void frt_ir_add_cache(FrtIndexReader *ir)
{
    if (NULL == ir->cache) {
        ir->cache = frt_co_hash_create();
    }
}

bool frt_ir_is_latest(FrtIndexReader *ir)
{
    return ir->is_latest_i(ir);
}

/****************************************************************************
 * Norm
 ****************************************************************************/

typedef struct Norm {
    int field_num;
    FrtInStream *is;
    frt_uchar *bytes;
    bool is_dirty : 1;
} Norm;

static Norm *norm_create(FrtInStream *is, int field_num)
{
    Norm *norm = FRT_ALLOC(Norm);

    norm->is = is;
    norm->field_num = field_num;
    norm->bytes = NULL;
    norm->is_dirty = false;

    return norm;
}

static void norm_destroy(Norm *norm)
{
    frt_is_close(norm->is);
    if (NULL != norm->bytes) {
        free(norm->bytes);
    }
    free(norm);
}

static void norm_rewrite(Norm *norm, FrtStore *store, FrtDeleter *dlr,
                         FrtSegmentInfo *si, int doc_count)
{
    FrtOutStream *os;
    char norm_file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    const int field_num = norm->field_num;

    if (si_norm_file_name(si, norm_file_name, field_num)) {
        deleter_queue_file(dlr, norm_file_name);
    }
    frt_si_advance_norm_gen(si, field_num);
    si_norm_file_name(si, norm_file_name, field_num);
    os = store->new_output(store, norm_file_name);
    frt_os_write_bytes(os, norm->bytes, doc_count);
    frt_os_close(os);
    norm->is_dirty = false;
}

/****************************************************************************
 * SegmentReader
 ****************************************************************************/

typedef struct SegmentReader {
    FrtIndexReader ir;
    FrtSegmentInfo *si;
    char *segment;
    FrtFieldsReader *fr;
    FrtBitVector *deleted_docs;
    FrtInStream *frq_in;
    FrtInStream *prx_in;
    FrtSegmentFieldIndex *sfi;
    FrtTermInfosReader *tir;
    frt_thread_key_t thread_fr;
    void **fr_bucket;
    FrtHash *norms;
    FrtStore *cfs_store;
    bool deleted_docs_dirty : 1;
    bool undelete_all : 1;
    bool norms_dirty : 1;
} SegmentReader;

#define IR(ir) ((FrtIndexReader *)(ir))

#define SR(ir) ((SegmentReader *)(ir))
#define SR_SIZE(ir) (SR(ir)->fr->size)

static FrtFieldsReader *sr_fr(SegmentReader *sr)
{
    FrtFieldsReader *fr;

    if (NULL == (fr = (FrtFieldsReader *)frt_thread_getspecific(sr->thread_fr))) {
        fr = frt_fr_clone(sr->fr);
        frt_ary_push(sr->fr_bucket, fr);
        frt_thread_setspecific(sr->thread_fr, fr);
    }
    return fr;
}

static bool sr_is_deleted_i(SegmentReader *sr, int doc_num)
{
    return (NULL != sr->deleted_docs && frt_bv_get(sr->deleted_docs, doc_num));
}

static void sr_get_norms_into_i(SegmentReader *sr, int field_num,
                                       frt_uchar *buf)
{
    Norm *norm = (Norm *)frt_h_get_int(sr->norms, field_num);
    if (NULL == norm) {
        memset(buf, 0, SR_SIZE(sr));
    }
    else if (NULL != norm->bytes) { /* can copy from cache */
        memcpy(buf, norm->bytes, SR_SIZE(sr));
    }
    else {
        FrtInStream *norm_in = frt_is_clone(norm->is);
        /* read from disk */
        frt_is_seek(norm_in, 0);
        frt_is_read_bytes(norm_in, buf, SR_SIZE(sr));
        frt_is_close(norm_in);
    }
}

static frt_uchar *sr_get_norms_i(SegmentReader *sr, int field_num)
{
    Norm *norm = (Norm *)frt_h_get_int(sr->norms, field_num);
    if (NULL == norm) {                           /* not an indexed field */
        return NULL;
    }

    if (NULL == norm->bytes) {                    /* value not yet read */
        frt_uchar *bytes = FRT_ALLOC_N(frt_uchar, SR_SIZE(sr));
        sr_get_norms_into_i(sr, field_num, bytes);
        norm->bytes = bytes;                        /* cache it */
    }
    return norm->bytes;
}

static void sr_set_norm_i(FrtIndexReader *ir, int doc_num, int field_num, frt_uchar b)
{
    Norm *norm = (Norm *)frt_h_get_int(SR(ir)->norms, field_num);
    if (NULL != norm) { /* has_norms */
        ir->has_changes = true;
        norm->is_dirty = true; /* mark it dirty */
        SR(ir)->norms_dirty = true;
        sr_get_norms_i(SR(ir), field_num)[doc_num] = b;
    }
}

static void sr_delete_doc_i(FrtIndexReader *ir, int doc_num)
{
    if (NULL == SR(ir)->deleted_docs) {
        SR(ir)->deleted_docs = frt_bv_new();
    }

    SR(ir)->deleted_docs_dirty = true;
    SR(ir)->undelete_all = false;
    ir->has_changes = true;
    frt_bv_set(SR(ir)->deleted_docs, doc_num);
}

static void sr_undelete_all_i(FrtIndexReader *ir)
{
    SR(ir)->undelete_all = true;
    SR(ir)->deleted_docs_dirty = false;
    ir->has_changes = true;
    if (NULL != SR(ir)->deleted_docs) {
        frt_bv_destroy(SR(ir)->deleted_docs);
    }
    SR(ir)->deleted_docs = NULL;
}

static void sr_set_deleter_i(FrtIndexReader *ir, FrtDeleter *deleter)
{
  ir->deleter = deleter;
}

static void bv_write(FrtBitVector *bv, FrtStore *store, char *name)
{
    int i;
    FrtOutStream *os = store->new_output(store, name);
    frt_os_write_vint(os, bv->size);
    for (i = ((bv->size-1) >> 5); i >= 0; i--) {
        frt_os_write_u32(os, bv->bits[i]);
    }
    frt_os_close(os);
}

static FrtBitVector *bv_read(FrtStore *store, char *name)
{
    int i;
    volatile bool success = false;
    FrtInStream *volatile is = store->open_input(store, name);
    FrtBitVector *volatile bv = FRT_ALLOC_AND_ZERO(FrtBitVector);
    bv->size = (int)frt_is_read_vint(is);
    bv->capa = (bv->size >> 5) + 1;
    bv->bits = FRT_ALLOC_AND_ZERO_N(frt_u32, bv->capa);
    bv->ref_cnt = 1;
    FRT_TRY
        for (i = ((bv->size-1) >> 5); i >= 0; i--) {
            bv->bits[i] = frt_is_read_u32(is);
        }
        frt_bv_recount(bv);
        success = true;
    FRT_XFINALLY
        frt_is_close(is);
        if (!success && bv) frt_bv_destroy(bv);
    FRT_XENDTRY
    return bv;
}

static bool sr_is_latest_i(FrtIndexReader *ir)
{
    return (frt_sis_read_current_version(ir->store) == ir->sis->version);
}

static void sr_commit_i(FrtIndexReader *ir)
{
    FrtSegmentInfo *si = SR(ir)->si;
    char *segment = SR(ir)->si->name;
    char tmp_file_name[FRT_SEGMENT_NAME_MAX_LENGTH];

    if (SR(ir)->undelete_all || SR(ir)->deleted_docs_dirty) {
        if (si->del_gen >= 0) {
            frt_fn_for_generation(tmp_file_name, segment, "del", si->del_gen);
            deleter_queue_file(ir->deleter, tmp_file_name);
        }
        if (SR(ir)->undelete_all) {
            si->del_gen = -1;
            SR(ir)->undelete_all = false;
        }
        else {
            /* (SR(ir)->deleted_docs_dirty) re-write deleted */
            si->del_gen++;
            frt_fn_for_generation(tmp_file_name, segment, "del", si->del_gen);
            bv_write(SR(ir)->deleted_docs, ir->store, tmp_file_name);
            SR(ir)->deleted_docs_dirty = false;
        }
    }
    if (SR(ir)->norms_dirty) { /* re-write norms */
        int i;
        const int field_cnt = ir->fis->size;
        FrtFieldInfo *fi;
        for (i = field_cnt - 1; i >= 0; i--) {
            fi = ir->fis->fields[i];
            if (fi_is_indexed(fi)) {
                Norm *norm = (Norm *)frt_h_get_int(SR(ir)->norms, fi->number);
                if (norm && norm->is_dirty) {
                    norm_rewrite(norm, ir->store, ir->deleter, SR(ir)->si,
                                 SR_SIZE(ir));
                }
            }
        }
        SR(ir)->norms_dirty = false;
    }
}

static void sr_close_i(FrtIndexReader *ir)
{
    SegmentReader *sr = SR(ir);

    if (sr->fr)           frt_fr_close(sr->fr);
    if (sr->tir)          frt_tir_close(sr->tir);
    if (sr->sfi)          frt_sfi_close(sr->sfi);
    if (sr->frq_in)       frt_is_close(sr->frq_in);
    if (sr->prx_in)       frt_is_close(sr->prx_in);
    if (sr->norms)        frt_h_destroy(sr->norms);
    if (sr->deleted_docs) frt_bv_destroy(sr->deleted_docs);
    if (sr->cfs_store)    frt_store_deref(sr->cfs_store);
    if (sr->fr_bucket) {
        frt_thread_setspecific(sr->thread_fr, NULL);
        frt_thread_key_delete(sr->thread_fr);
        frt_ary_destroy(sr->fr_bucket, (frt_free_ft)&frt_fr_close);
    }
}

static int sr_num_docs(FrtIndexReader *ir)
{
    int num_docs;

    frt_mutex_lock(&ir->mutex);
    num_docs = SR(ir)->fr->size;
    if (NULL != SR(ir)->deleted_docs) {
        num_docs -= SR(ir)->deleted_docs->count;
    }
    frt_mutex_unlock(&ir->mutex);
    return num_docs;
}

static int sr_max_doc(FrtIndexReader *ir)
{
    return SR(ir)->fr->size;
}

static FrtDocument *sr_get_doc(FrtIndexReader *ir, int doc_num)
{
    FrtDocument *doc;
    frt_mutex_lock(&ir->mutex);
    if (sr_is_deleted_i(SR(ir), doc_num)) {
        frt_mutex_unlock(&ir->mutex);
        FRT_RAISE(FRT_STATE_ERROR, "Document %d has already been deleted", doc_num);
    }
    doc = frt_fr_get_doc(SR(ir)->fr, doc_num);
    frt_mutex_unlock(&ir->mutex);
    return doc;
}

static FrtLazyDoc *sr_get_lazy_doc(FrtIndexReader *ir, int doc_num)
{
    FrtLazyDoc *lazy_doc;
    frt_mutex_lock(&ir->mutex);
    if (sr_is_deleted_i(SR(ir), doc_num)) {
        frt_mutex_unlock(&ir->mutex);
        FRT_RAISE(FRT_STATE_ERROR, "Document %d has already been deleted", doc_num);
    }
    lazy_doc = frt_fr_get_lazy_doc(SR(ir)->fr, doc_num);
    frt_mutex_unlock(&ir->mutex);
    return lazy_doc;
}

static frt_uchar *sr_get_norms(FrtIndexReader *ir, int field_num)
{
    frt_uchar *norms;
    frt_mutex_lock(&ir->mutex);
    norms = sr_get_norms_i(SR(ir), field_num);
    frt_mutex_unlock(&ir->mutex);
    return norms;
}

static frt_uchar *sr_get_norms_into(FrtIndexReader *ir, int field_num,
                              frt_uchar *buf)
{
    frt_mutex_lock(&ir->mutex);
    sr_get_norms_into_i(SR(ir), field_num, buf);
    frt_mutex_unlock(&ir->mutex);
    return buf;
}

static FrtTermEnum *sr_terms(FrtIndexReader *ir, int field_num)
{
    FrtTermEnum *te = SR(ir)->tir->orig_te;
    te = frt_ste_clone(te);
    return ste_set_field(te, field_num);
}

static FrtTermEnum *sr_terms_from(FrtIndexReader *ir, int field_num, const char *term)
{
    FrtTermEnum *te = SR(ir)->tir->orig_te;
    te = frt_ste_clone(te);
    ste_set_field(te, field_num);
    ste_scan_to(te, term);
    return te;
}

static int sr_doc_freq(FrtIndexReader *ir, int field_num, const char *term)
{
    FrtTermInfo *ti = frt_tir_get_ti(frt_tir_set_field(SR(ir)->tir, field_num), term);
    return ti ? ti->doc_freq : 0;
}

static FrtTermDocEnum *sr_term_docs(FrtIndexReader *ir)
{
    return frt_stde_new(SR(ir)->tir, SR(ir)->frq_in, SR(ir)->deleted_docs,
                    STE(SR(ir)->tir->orig_te)->skip_interval);
}

static FrtTermDocEnum *sr_term_positions(FrtIndexReader *ir)
{
    SegmentReader *sr = SR(ir);
    return frt_stpe_new(sr->tir, sr->frq_in, sr->prx_in, sr->deleted_docs,
                    STE(sr->tir->orig_te)->skip_interval);
}

static FrtTermVector *sr_term_vector(FrtIndexReader *ir, int doc_num,
                                  FrtSymbol field)
{
    FrtFieldInfo *fi = (FrtFieldInfo *)frt_h_get(ir->fis->field_dict, (void *)field);
    FrtFieldsReader *fr;

    if (!fi || !fi_store_term_vector(fi) || !SR(ir)->fr ||
        !(fr = sr_fr(SR(ir)))) {
        return NULL;
    }

    return frt_fr_get_field_tv(fr, doc_num, fi->number);
}

static FrtHash *sr_term_vectors(FrtIndexReader *ir, int doc_num)
{
    FrtFieldsReader *fr;
    if (!SR(ir)->fr || NULL == (fr = sr_fr(SR(ir)))) {
        return NULL;
    }

    return frt_fr_get_tv(fr, doc_num);
}

static bool sr_is_deleted(FrtIndexReader *ir, int doc_num)
{
    bool is_del;

    frt_mutex_lock(&ir->mutex);
    is_del = sr_is_deleted_i(SR(ir), doc_num);
    frt_mutex_unlock(&ir->mutex);

    return is_del;
}

static bool sr_has_deletions(FrtIndexReader *ir)
{
    return NULL != SR(ir)->deleted_docs;
}

static void sr_open_norms(FrtIndexReader *ir, FrtStore *cfs_store)
{
    int i;
    FrtSegmentInfo *si = SR(ir)->si;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];

    for (i = si->norm_gens_size - 1; i >= 0; i--) {
        FrtStore *store = (si->use_compound_file && si->norm_gens[i] == 0) ?
            cfs_store : ir->store;
        if (si_norm_file_name(si, file_name, i)) {
            frt_h_set_int(SR(ir)->norms, i,
                      norm_create(store->open_input(store, file_name), i));
        }
    }
    SR(ir)->norms_dirty = false;
}

static FrtIndexReader *sr_setup_i(SegmentReader *sr)
{
    FrtStore *volatile store = sr->si->store;
    FrtIndexReader *ir = IR(sr);
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    char *sr_segment = sr->si->name;

    ir->num_docs            = &sr_num_docs;
    ir->max_doc             = &sr_max_doc;
    ir->get_doc             = &sr_get_doc;
    ir->get_lazy_doc        = &sr_get_lazy_doc;
    ir->get_norms           = &sr_get_norms;
    ir->get_norms_into      = &sr_get_norms_into;
    ir->terms               = &sr_terms;
    ir->terms_from          = &sr_terms_from;
    ir->doc_freq            = &sr_doc_freq;
    ir->term_docs           = &sr_term_docs;
    ir->term_positions      = &sr_term_positions;
    ir->term_vector         = &sr_term_vector;
    ir->term_vectors        = &sr_term_vectors;
    ir->is_deleted          = &sr_is_deleted;
    ir->has_deletions       = &sr_has_deletions;

    ir->set_norm_i          = &sr_set_norm_i;
    ir->delete_doc_i        = &sr_delete_doc_i;
    ir->undelete_all_i      = &sr_undelete_all_i;
    ir->set_deleter_i       = &sr_set_deleter_i;
    ir->is_latest_i         = &sr_is_latest_i;
    ir->commit_i            = &sr_commit_i;
    ir->close_i             = &sr_close_i;

    sr->cfs_store   = NULL;

    FRT_TRY
        if (sr->si->use_compound_file) {
            sprintf(file_name, "%s.cfs", sr_segment);
            sr->cfs_store = frt_open_cmpd_store(store, file_name);
            store = sr->cfs_store;
        }

        sr->fr = frt_fr_open(store, sr_segment, ir->fis);
        sr->sfi = frt_sfi_open(store, sr_segment);
        sr->tir = frt_tir_open(store, sr->sfi, sr_segment);

        sr->deleted_docs = NULL;
        sr->deleted_docs_dirty = false;
        sr->undelete_all = false;
        if (frt_si_has_deletions(sr->si)) {
            frt_fn_for_generation(file_name, sr_segment, "del", sr->si->del_gen);
            sr->deleted_docs = bv_read(sr->si->store, file_name);
        }

        sprintf(file_name, "%s.frq", sr_segment);
        sr->frq_in = store->open_input(store, file_name);
        sprintf(file_name, "%s.prx", sr_segment);
        sr->prx_in = store->open_input(store, file_name);
        sr->norms = frt_h_new_int((frt_free_ft)&norm_destroy);
        sr_open_norms(ir, store);
        if (fis_has_vectors(ir->fis)) {
            frt_thread_key_create(&sr->thread_fr, NULL);
            sr->fr_bucket = frt_ary_new();
        }
    FRT_XCATCHALL
        ir->sis = NULL;
        frt_ir_close(ir);
    FRT_XENDTRY

    return ir;
}

static FrtIndexReader *sr_open(FrtSegmentInfos *sis, FrtFieldInfos *fis, int si_num,
                            bool is_owner)
{
    SegmentReader *sr = FRT_ALLOC_AND_ZERO(SegmentReader);
    sr->si = sis->segs[si_num];
    ir_setup(IR(sr), sr->si->store, sis, fis, is_owner);
    return sr_setup_i(sr);
}

/****************************************************************************
 * MultiReader
 ****************************************************************************/

#define MR(ir) ((FrtMultiReader *)(ir))

static int mr_reader_index_i(FrtMultiReader *mr, int doc_num)
{
    int lo = 0;                       /* search @starts array */
    int hi = mr->r_cnt - 1;            /* for first element less */
    int mid;
    int mid_value;

    while (hi >= lo) {
        mid = (lo + hi) >> 1;
        mid_value = mr->starts[mid];
        if (doc_num < mid_value) {
            hi = mid - 1;
        }
        else if (doc_num > mid_value) {
            lo = mid + 1;
        }
        else { /* found a match */
            while ((mid+1 < mr->r_cnt) && (mr->starts[mid+1] == mid_value)) {
                mid += 1; /* scan to last match in case we have empty segments */
            }
            return mid;
        }
    }
    return hi;
}

static int mr_num_docs(FrtIndexReader *ir)
{
    int i, num_docs;
    frt_mutex_lock(&ir->mutex);
    if (MR(ir)->num_docs_cache == -1) {
        const int mr_reader_cnt = MR(ir)->r_cnt;
        MR(ir)->num_docs_cache = 0;
        for (i = 0; i < mr_reader_cnt; i++) {
            FrtIndexReader *reader = MR(ir)->sub_readers[i];
            MR(ir)->num_docs_cache += reader->num_docs(reader);
        }
    }
    num_docs = MR(ir)->num_docs_cache;
    frt_mutex_unlock(&ir->mutex);

    return num_docs;
}

static int mr_max_doc(FrtIndexReader *ir)
{
    return MR(ir)->max_doc;
}

#define GET_READER()\
    int i = mr_reader_index_i(MR(ir), doc_num);\
    FrtIndexReader *reader = MR(ir)->sub_readers[i]

static FrtDocument *mr_get_doc(FrtIndexReader *ir, int doc_num)
{
    GET_READER();
    return reader->get_doc(reader, doc_num - MR(ir)->starts[i]);
}

static FrtLazyDoc *mr_get_lazy_doc(FrtIndexReader *ir, int doc_num)
{
    GET_READER();
    return reader->get_lazy_doc(reader, doc_num - MR(ir)->starts[i]);
}

int frt_mr_get_field_num(FrtMultiReader *mr, int ir_num, int f_num)
{
    if (mr->field_num_map) {
        return mr->field_num_map[ir_num][f_num];
    }
    else {
        return f_num;
    }
}

static frt_uchar *mr_get_norms(FrtIndexReader *ir, int field_num)
{
    frt_uchar *bytes;

    frt_mutex_lock(&ir->mutex);
    bytes = (frt_uchar *)frt_h_get_int(MR(ir)->norms_cache, field_num);
    if (NULL == bytes) {
        int i;
        const int mr_reader_cnt = MR(ir)->r_cnt;

        bytes = FRT_ALLOC_AND_ZERO_N(frt_uchar, MR(ir)->max_doc);

        for (i = 0; i < mr_reader_cnt; i++) {
            int fnum = frt_mr_get_field_num(MR(ir), i, field_num);
            if (fnum >= 0) {
                FrtIndexReader *reader = MR(ir)->sub_readers[i];
                reader->get_norms_into(reader, fnum, bytes + MR(ir)->starts[i]);
            }
        }
        frt_h_set_int(MR(ir)->norms_cache, field_num, bytes); /* update cache */
    }
    frt_mutex_unlock(&ir->mutex);

    return bytes;
}

static frt_uchar *mr_get_norms_into(FrtIndexReader *ir, int field_num, frt_uchar *buf)
{
    frt_uchar *bytes;

    frt_mutex_lock(&ir->mutex);
    bytes = (frt_uchar *)frt_h_get_int(MR(ir)->norms_cache, field_num);
    if (NULL != bytes) {
        memcpy(buf, bytes, MR(ir)->max_doc);
    }
    else {
        int i;
        const int mr_reader_cnt = MR(ir)->r_cnt;
        for (i = 0; i < mr_reader_cnt; i++) {
            int fnum = frt_mr_get_field_num(MR(ir), i, field_num);
            if (fnum >= 0) {
                FrtIndexReader *reader = MR(ir)->sub_readers[i];
                reader->get_norms_into(reader, fnum, buf + MR(ir)->starts[i]);
            }
        }
    }
    frt_mutex_unlock(&ir->mutex);
    return buf;
}

static FrtTermEnum *mr_terms(FrtIndexReader *ir, int field_num)
{
    return frt_mte_new(MR(ir), field_num, NULL);
}

static FrtTermEnum *mr_terms_from(FrtIndexReader *ir, int field_num, const char *term)
{
    return frt_mte_new(MR(ir), field_num, term);
}

static int mr_doc_freq(FrtIndexReader *ir, int field_num, const char *t)
{
    int total = 0;          /* sum freqs in segments */
    int i = MR(ir)->r_cnt;
    for (i = MR(ir)->r_cnt - 1; i >= 0; i--) {
        int fnum = frt_mr_get_field_num(MR(ir), i, field_num);
        if (fnum >= 0) {
            FrtIndexReader *reader = MR(ir)->sub_readers[i];
            total += reader->doc_freq(reader, fnum, t);
        }
    }
    return total;
}

static FrtTermDocEnum *mr_term_docs(FrtIndexReader *ir)
{
    return mtde_new(MR(ir));
}

static FrtTermDocEnum *mr_term_positions(FrtIndexReader *ir)
{
    return mtpe_new(MR(ir));
}

static FrtTermVector *mr_term_vector(FrtIndexReader *ir, int doc_num,
                                  FrtSymbol field)
{
    GET_READER();
    return reader->term_vector(reader, doc_num - MR(ir)->starts[i], field);
}

static FrtHash *mr_term_vectors(FrtIndexReader *ir, int doc_num)
{
    GET_READER();
    return reader->term_vectors(reader, doc_num - MR(ir)->starts[i]);
}

static bool mr_is_deleted(FrtIndexReader *ir, int doc_num)
{
    GET_READER();
    return reader->is_deleted(reader, doc_num - MR(ir)->starts[i]);
}

static bool mr_has_deletions(FrtIndexReader *ir)
{
    return MR(ir)->has_deletions;
}

static void mr_set_norm_i(FrtIndexReader *ir, int doc_num, int field_num, frt_uchar val)
{
    int i = mr_reader_index_i(MR(ir), doc_num);
    int fnum = frt_mr_get_field_num(MR(ir), i, field_num);
    if (fnum >= 0) {
        FrtIndexReader *reader = MR(ir)->sub_readers[i];
        ir->has_changes = true;
        frt_h_del_int(MR(ir)->norms_cache, fnum);/* clear cache */
        ir_set_norm_i(reader, doc_num - MR(ir)->starts[i], fnum, val);
    }
}

static void mr_delete_doc_i(FrtIndexReader *ir, int doc_num)
{
    GET_READER();
    MR(ir)->num_docs_cache = -1; /* invalidate cache */

    /* dispatch to segment reader */
    reader->delete_doc_i(reader, doc_num - MR(ir)->starts[i]);
    MR(ir)->has_deletions = true;
    ir->has_changes = true;
}

static void mr_undelete_all_i(FrtIndexReader *ir)
{
    int i;
    const int mr_reader_cnt = MR(ir)->r_cnt;

    MR(ir)->num_docs_cache = -1;                     /* invalidate cache */
    for (i = 0; i < mr_reader_cnt; i++) {
        FrtIndexReader *reader = MR(ir)->sub_readers[i];
        reader->undelete_all_i(reader);
    }
    MR(ir)->has_deletions = false;
    ir->has_changes = true;
}

static void mr_set_deleter_i(FrtIndexReader *ir, FrtDeleter *deleter)
{
    int i;
    ir->deleter = deleter;
    for (i = MR(ir)->r_cnt - 1; i >= 0; i--) {
        FrtIndexReader *reader = MR(ir)->sub_readers[i];
        reader->set_deleter_i(reader, deleter);
    }
}

static bool mr_is_latest_i(FrtIndexReader *ir)
{
    int i;
    const int mr_reader_cnt = MR(ir)->r_cnt;
    for (i = 0; i < mr_reader_cnt; i++) {
        if (!frt_ir_is_latest(MR(ir)->sub_readers[i])) {
            return false;
        }
    }
    return true;
}

static void mr_commit_i(FrtIndexReader *ir)
{
    int i;
    const int mr_reader_cnt = MR(ir)->r_cnt;
    for (i = 0; i < mr_reader_cnt; i++) {
        FrtIndexReader *reader = MR(ir)->sub_readers[i];
        ir_commit_i(reader);
    }
}

static void mr_close_i(FrtIndexReader *ir)
{
    int i;
    const int mr_reader_cnt = MR(ir)->r_cnt;
    for (i = 0; i < mr_reader_cnt; i++) {
        FrtIndexReader *reader = MR(ir)->sub_readers[i];
        frt_ir_close(reader);
    }
    free(MR(ir)->sub_readers);
    frt_h_destroy(MR(ir)->norms_cache);
    free(MR(ir)->starts);
}

static FrtIndexReader *mr_new(FrtIndexReader **sub_readers, const int r_cnt)
{
    int i;
    FrtMultiReader *mr = FRT_ALLOC_AND_ZERO(FrtMultiReader);
    FrtIndexReader *ir = IR(mr);

    mr->sub_readers         = sub_readers;
    mr->r_cnt               = r_cnt;
    mr->max_doc             = 0;
    mr->num_docs_cache      = -1;
    mr->has_deletions       = false;

    mr->starts              = FRT_ALLOC_N(int, (r_cnt+1));

    for (i = 0; i < r_cnt; i++) {
        FrtIndexReader *sub_reader = sub_readers[i];
        mr->starts[i] = mr->max_doc;
        mr->max_doc += sub_reader->max_doc(sub_reader); /* compute max_docs */

        if (sub_reader->has_deletions(sub_reader)) {
            mr->has_deletions = true;
        }
    }
    mr->starts[r_cnt]       = mr->max_doc;
    mr->norms_cache         = frt_h_new_int(&free);

    ir->num_docs            = &mr_num_docs;
    ir->max_doc             = &mr_max_doc;
    ir->get_doc             = &mr_get_doc;
    ir->get_lazy_doc        = &mr_get_lazy_doc;
    ir->get_norms           = &mr_get_norms;
    ir->get_norms_into      = &mr_get_norms_into;
    ir->terms               = &mr_terms;
    ir->terms_from          = &mr_terms_from;
    ir->doc_freq            = &mr_doc_freq;
    ir->term_docs           = &mr_term_docs;
    ir->term_positions      = &mr_term_positions;
    ir->term_vector         = &mr_term_vector;
    ir->term_vectors        = &mr_term_vectors;
    ir->is_deleted          = &mr_is_deleted;
    ir->has_deletions       = &mr_has_deletions;

    ir->set_norm_i          = &mr_set_norm_i;
    ir->delete_doc_i        = &mr_delete_doc_i;
    ir->undelete_all_i      = &mr_undelete_all_i;
    ir->set_deleter_i       = &mr_set_deleter_i;
    ir->is_latest_i         = &mr_is_latest_i;
    ir->commit_i            = &mr_commit_i;
    ir->close_i             = &mr_close_i;

    return ir;
}

static FrtIndexReader *frt_mr_open_i(FrtStore *store,
                       FrtSegmentInfos *sis,
                       FrtFieldInfos *fis,
                       FrtIndexReader **sub_readers,
                       const int r_cnt)
{
    FrtIndexReader *ir = mr_new(sub_readers, r_cnt);
    return ir_setup(ir, store, sis, fis, true);
}

static void mr_close_ext_i(FrtIndexReader *ir)
{
    int **field_num_map = MR(ir)->field_num_map;
    if (field_num_map) {
        int i;
        for (i = MR(ir)->r_cnt - 1; i >= 0; i--) {
            free(field_num_map[i]);
        }
        free(field_num_map);
    }
    frt_fis_deref(ir->fis);
    mr_close_i(ir);
}

FrtIndexReader *frt_mr_open(FrtIndexReader **sub_readers, const int r_cnt)
{
    FrtIndexReader *ir = mr_new(sub_readers, r_cnt);
    FrtMultiReader *mr = MR(ir);
    /* defaults don't matter, this is just for reading fields, not adding */
    FrtFieldInfos *fis = frt_fis_new(FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
    int i, j;
    bool need_field_map = false;

    /* Merge FieldInfos */
    for (i = 0; i < r_cnt; i++) {
        FrtFieldInfos *sub_fis = sub_readers[i]->fis;
        const int sub_fis_size = sub_fis->size;
        for (j = 0; j < sub_fis_size; j++) {
            FrtFieldInfo *fi = sub_fis->fields[j];
            FrtFieldInfo *new_fi = frt_fis_get_or_add_field(fis, fi->name);
            new_fi->bits |= fi->bits;
            if (fi->number != new_fi->number) {
                need_field_map = true;
            }
        }
    }

    /* we only need to build a field map if some of the sub FieldInfos didn't
     * match the new FieldInfos object */
    if (need_field_map) {
        mr->field_num_map = FRT_ALLOC_N(int *, r_cnt);
        for (i = 0; i < r_cnt; i++) {
            FrtFieldInfos *sub_fis = sub_readers[i]->fis;
            const int fis_size = fis->size;

            mr->field_num_map[i] = FRT_ALLOC_N(int, fis_size);
            for (j = 0; j < fis_size; j++) {
                FrtFieldInfo *fi = fis->fields[j];
                FrtFieldInfo *fi_sub = frt_fis_get_field(sub_fis, fi->name);
                /* set non existant field nums to -1. The MultiReader will
                 * skip readers which don't have needed fields */
                mr->field_num_map[i][j] = fi_sub ? fi_sub->number : -1;
            }
        }
    }
    else {
        mr->field_num_map = NULL;
    }


    ir->close_i = &mr_close_ext_i;

    return ir_setup(ir, NULL, NULL, fis, false);
}

/****************************************************************************
 * IndexReader
 ****************************************************************************/


static void ir_open_i(FrtStore *store, FindSegmentsFile *fsf)
{
    volatile bool success = false;
    FrtIndexReader *volatile ir = NULL;
    FrtSegmentInfos *volatile sis = NULL;
    FRT_TRY
    do {
        FrtFieldInfos *fis;
        frt_mutex_lock(&store->mutex);
        frt_sis_read_i(store, fsf);
        sis = fsf->ret.sis;
        fis = sis->fis;
        if (sis->size == 1) {
            ir = sr_open(sis, fis, 0, true);
        }
        else {
            volatile int i;
            FrtIndexReader **readers = FRT_ALLOC_N(FrtIndexReader *, sis->size);
            int num_segments = sis->size;
            for (i = num_segments - 1; i >= 0; i--) {
                FRT_TRY
                    readers[i] = sr_open(sis, fis, i, false);
                FRT_XCATCHALL
                    for (i++; i < num_segments; i++) {
                        frt_ir_close(readers[i]);
                    }
                    free(readers);
                FRT_XENDTRY
            }
            ir = frt_mr_open_i(store, sis, fis, readers, sis->size);
        }
        fsf->ret.ir = ir;
        success = true;
    } while (0);
    FRT_XFINALLY
        if (!success) {
            if (ir) {
                frt_ir_close(ir);
            }
            else if (sis) {
                frt_sis_destroy(sis);
            }
        }
        frt_mutex_unlock(&store->mutex);
    FRT_XENDTRY
}

/**
 * Will keep a reference to the store. To let this method delete the store
 * make sure you deref the store that you pass to it
 */
FrtIndexReader *frt_ir_open(FrtStore *store)
{
    FindSegmentsFile fsf;
    sis_find_segments_file(store, &fsf, &ir_open_i);
    return fsf.ret.ir;
}



/****************************************************************************
 *
 * Occurence
 *
 ****************************************************************************/

static FrtOccurence *occ_new(FrtMemoryPool *mp, int pos)
{
    FrtOccurence *occ = FRT_MP_ALLOC(mp, FrtOccurence);
    occ->pos = pos;
    occ->next = NULL;
    return occ;
}

/****************************************************************************
 *
 * Posting
 *
 ****************************************************************************/

FrtPosting *frt_p_new(FrtMemoryPool *mp, int doc_num, int pos)
{
    FrtPosting *p = FRT_MP_ALLOC(mp, FrtPosting);
    p->doc_num = doc_num;
    p->first_occ = occ_new(mp, pos);
    p->freq = 1;
    p->next = NULL;
    return p;
}

/****************************************************************************
 *
 * PostingList
 *
 ****************************************************************************/

FrtPostingList *frt_pl_new(FrtMemoryPool *mp, const char *term,
                           int term_len, FrtPosting *p)
{
    FrtPostingList *pl = FRT_MP_ALLOC(mp, FrtPostingList);
    pl->term = (char *)frt_mp_memdup(mp, term, term_len + 1);
    pl->term_len = term_len;
    pl->first = pl->last = p;
    pl->last_occ = p->first_occ;
    return pl;
}

void frt_pl_add_occ(FrtMemoryPool *mp, FrtPostingList *pl, int pos)
{
    pl->last_occ = pl->last_occ->next = occ_new(mp, pos);
    pl->last->freq++;
}

static void pl_add_posting(FrtPostingList *pl, FrtPosting *p)
{
    pl->last = pl->last->next = p;
    pl->last_occ = p->first_occ;
}

int frt_pl_cmp(const FrtPostingList **pl1, const FrtPostingList **pl2)
{
    return strcmp((*pl1)->term, (*pl2)->term);
}

/****************************************************************************
 *
 * FrtFieldInverter
 *
 ****************************************************************************/

static FrtFieldInverter *fld_inv_new(FrtDocWriter *dw, FrtFieldInfo *fi)
{
    FrtFieldInverter *fld_inv = FRT_MP_ALLOC(dw->mp, FrtFieldInverter);
    fld_inv->is_tokenized = fi_is_tokenized(fi);
    fld_inv->store_term_vector = fi_store_term_vector(fi);
    fld_inv->store_offsets = fi_store_offsets(fi);
    if ((fld_inv->has_norms = fi_has_norms(fi)) == true) {
        fld_inv->norms = FRT_MP_ALLOC_AND_ZERO_N(dw->mp, frt_uchar,
                                             dw->max_buffered_docs);
    }
    fld_inv->fi = fi;

    /* this will alloc it's own memory so must be destroyed */
    fld_inv->plists = frt_h_new_str(NULL, NULL);

    return fld_inv;
}

static void fld_inv_destroy(FrtFieldInverter *fld_inv)
{
    frt_h_destroy(fld_inv->plists);
}

/****************************************************************************
 *
 * SkipBuffer
 *
 ****************************************************************************/

typedef struct SkipBuffer
{
    FrtOutStream *buf;
    FrtOutStream *frq_out;
    FrtOutStream *prx_out;
    int last_doc;
    off_t last_frq_ptr;
    off_t last_prx_ptr;
} SkipBuffer;

static void skip_buf_reset(SkipBuffer *skip_buf)
{
    frt_ramo_reset(skip_buf->buf);
    skip_buf->last_doc = 0;
    skip_buf->last_frq_ptr = frt_os_pos(skip_buf->frq_out);
    skip_buf->last_prx_ptr = frt_os_pos(skip_buf->prx_out);
}

static SkipBuffer *skip_buf_new(FrtOutStream *frq_out, FrtOutStream *prx_out)
{
    SkipBuffer *skip_buf = FRT_ALLOC(SkipBuffer);
    skip_buf->buf = frt_ram_new_buffer();
    skip_buf->frq_out = frq_out;
    skip_buf->prx_out = prx_out;
    return skip_buf;
}

static void skip_buf_add(SkipBuffer *skip_buf, int doc)
{
    off_t frq_ptr = frt_os_pos(skip_buf->frq_out);
    off_t prx_ptr = frt_os_pos(skip_buf->prx_out);

    frt_os_write_vint(skip_buf->buf, doc - skip_buf->last_doc);
    frt_os_write_vint(skip_buf->buf, frq_ptr - skip_buf->last_frq_ptr);
    frt_os_write_vint(skip_buf->buf, prx_ptr - skip_buf->last_prx_ptr);

    skip_buf->last_doc = doc;
    skip_buf->last_frq_ptr = frq_ptr;
    skip_buf->last_prx_ptr = prx_ptr;
}

static off_t skip_buf_write(SkipBuffer *skip_buf)
{
  off_t skip_ptr = frt_os_pos(skip_buf->frq_out);
  frt_ramo_write_to(skip_buf->buf, skip_buf->frq_out);
  return skip_ptr;
}

static void skip_buf_destroy(SkipBuffer *skip_buf)
{
    frt_ram_destroy_buffer(skip_buf->buf);
    free(skip_buf);
}

/****************************************************************************
 *
 * FrtDocWriter
 *
 ****************************************************************************/

static void dw_write_norms(FrtDocWriter *dw, FrtFieldInverter *fld_inv)
{
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtOutStream *norms_out;
    frt_si_advance_norm_gen(dw->si, fld_inv->fi->number);
    si_norm_file_name(dw->si, file_name, fld_inv->fi->number);
    norms_out = dw->store->new_output(dw->store, file_name);
    frt_os_write_bytes(norms_out, fld_inv->norms, dw->doc_num);
    frt_os_close(norms_out);
}

/* we'll use the postings Hash's table area to sort the postings as it is
 * going to be zeroset soon anyway */
static FrtPostingList **dw_sort_postings(FrtHash *plists_ht)
{
    int i, j;
    FrtHashEntry *he;
    FrtPostingList **plists = (FrtPostingList **)plists_ht->table;
    const int num_entries = plists_ht->mask + 1;
    for (i = 0, j = 0; i < num_entries; i++) {
        he = &plists_ht->table[i];
        if (he->value) {
            plists[j++] = (FrtPostingList *)he->value;
        }
    }

    qsort(plists, plists_ht->size, sizeof(FrtPostingList *),
          (int (*)(const void *, const void *))&frt_pl_cmp);

    return plists;
}

static void dw_flush_streams(FrtDocWriter *dw)
{
    frt_mp_reset(dw->mp);
    frt_fw_close(dw->fw);
    dw->fw = NULL;
    frt_h_clear(dw->fields);
    dw->doc_num = 0;
}

static void dw_flush(FrtDocWriter *dw)
{
    int i, j, last_doc, doc_code, doc_freq, last_pos, posting_count;
    int skip_interval = dw->skip_interval;
    FrtFieldInfos *fis = dw->fis;
    const int fields_count = fis->size;
    FrtFieldInverter *fld_inv;
    FrtFieldInfo *fi;
    FrtPostingList **pls, *pl;
    FrtPosting *p;
    FrtOccurence *occ;
    FrtStore *store = dw->store;
    FrtTermInfosWriter *tiw = frt_tiw_open(store, dw->si->name,
                                    dw->index_interval, skip_interval);
    FrtTermInfo ti;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtOutStream *frq_out, *prx_out;
    SkipBuffer *skip_buf;

    sprintf(file_name, "%s.frq", dw->si->name);
    frq_out = store->new_output(store, file_name);
    sprintf(file_name, "%s.prx", dw->si->name);
    prx_out = store->new_output(store, file_name);
    skip_buf = skip_buf_new(frq_out, prx_out);

    for (i = 0; i < fields_count; i++) {
        fi = fis->fields[i];
        if (!fi_is_indexed(fi) || NULL ==
            (fld_inv = (FrtFieldInverter*)frt_h_get_int(dw->fields, fi->number))) {
            continue;
        }
        if (!fi_omit_norms(fi)) {
            dw_write_norms(dw, fld_inv);
        }

        pls = dw_sort_postings(fld_inv->plists);
        frt_tiw_start_field(tiw, fi->number);
        posting_count = fld_inv->plists->size;
        for (j = 0; j < posting_count; j++) {
            pl = pls[j];
            ti.frq_ptr = frt_os_pos(frq_out);
            ti.prx_ptr = frt_os_pos(prx_out);
            last_doc = 0;
            doc_freq = 0;
            skip_buf_reset(skip_buf);
            for (p = pl->first; NULL != p; p = p->next) {
                doc_freq++;
                if (0 == (doc_freq % dw->skip_interval)) {
                    skip_buf_add(skip_buf, last_doc);
                }

                doc_code = (p->doc_num - last_doc) << 1;
                last_doc = p->doc_num;

                if (p->freq == 1) {
                    frt_os_write_vint(frq_out, 1|doc_code);
                }
                else {
                    frt_os_write_vint(frq_out, doc_code);
                    frt_os_write_vint(frq_out, p->freq);
                }

                last_pos = 0;
                for (occ = p->first_occ; NULL != occ; occ = occ->next) {
                    frt_os_write_vint(prx_out, occ->pos - last_pos);
                    last_pos = occ->pos;
                }
            }
            ti.skip_offset = skip_buf_write(skip_buf) - ti.frq_ptr;
            ti.doc_freq = doc_freq;
            frt_tiw_add(tiw, pl->term, pl->term_len, &ti);
        }
    }
    frt_os_close(prx_out);
    frt_os_close(frq_out);
    frt_tiw_close(tiw);
    skip_buf_destroy(skip_buf);
    dw_flush_streams(dw);
}

FrtDocWriter *frt_dw_open(FrtIndexWriter *iw, FrtSegmentInfo *si)
{
    FrtStore *store = iw->store;
    FrtMemoryPool *mp = frt_mp_new_capa(iw->config.chunk_size,
        iw->config.max_buffer_memory/iw->config.chunk_size);

    FrtDocWriter *dw = FRT_ALLOC(FrtDocWriter);

    dw->mp          = mp;
    dw->analyzer    = iw->analyzer;
    dw->fis         = iw->fis;
    dw->store       = store;
    dw->fw          = frt_fw_open(store, si->name, iw->fis);
    dw->si          = si;

    dw->curr_plists = frt_h_new_str(NULL, NULL);
    dw->fields      = frt_h_new_int((frt_free_ft)fld_inv_destroy);
    dw->doc_num     = 0;

    dw->index_interval      = iw->config.index_interval;
    dw->skip_interval       = iw->config.skip_interval;
    dw->max_field_length    = iw->config.max_field_length;
    dw->max_buffered_docs   = iw->config.max_buffered_docs;

    dw->offsets             = FRT_ALLOC_AND_ZERO_N(FrtOffset, DW_OFFSET_INIT_CAPA);
    dw->offsets_size        = 0;
    dw->offsets_capa        = DW_OFFSET_INIT_CAPA;

    dw->similarity          = iw->similarity;
    return dw;
}

void frt_dw_new_segment(FrtDocWriter *dw, FrtSegmentInfo *si)
{
    dw->fw = frt_fw_open(dw->store, si->name, dw->fis);
    dw->si = si;
}

void frt_dw_close(FrtDocWriter *dw)
{
    if (dw->doc_num) {
        dw_flush(dw);
    }
    if (dw->fw) {
        frt_fw_close(dw->fw);
    }
    frt_h_destroy(dw->curr_plists);
    frt_h_destroy(dw->fields);
    frt_mp_destroy(dw->mp);
    free(dw->offsets);
    free(dw);
}

FrtFieldInverter *frt_dw_get_fld_inv(FrtDocWriter *dw, FrtFieldInfo *fi)
{
    FrtFieldInverter *fld_inv = (FrtFieldInverter*)frt_h_get_int(dw->fields, fi->number);

    if (!fld_inv) {
        fld_inv = fld_inv_new(dw, fi);
        frt_h_set_int(dw->fields, fi->number, fld_inv);
    }
    return fld_inv;
}

static void dw_add_posting(FrtMemoryPool *mp,
                           FrtHash *curr_plists,
                           FrtHash *fld_plists,
                           int doc_num,
                           const char *text,
                           int len,
                           int pos)
{
    FrtHashEntry *pl_he;
    if (frt_h_set_ext(curr_plists, text, &pl_he)) {
        FrtPosting *p =  frt_p_new(mp, doc_num, pos);
        FrtHashEntry *fld_pl_he;
        FrtPostingList *pl;

        if (frt_h_set_ext(fld_plists, text, &fld_pl_he)) {
            fld_pl_he->value = pl = frt_pl_new(mp, text, len, p);
            pl_he->key = fld_pl_he->key = (char *)pl->term;
        }
        else {
            pl = (FrtPostingList *)fld_pl_he->value;
            pl_add_posting(pl, p);
            pl_he->key = (char *)pl->term;
        }
        pl_he->value = pl;
    }
    else {
        frt_pl_add_occ(mp, (FrtPostingList *)pl_he->value, pos);
    }
}

static void dw_add_offsets(FrtDocWriter *dw, int pos, off_t start, off_t end)
{
    if (pos >= dw->offsets_capa) {
        int old_capa = dw->offsets_capa;
        while (pos >= dw->offsets_capa) {
            dw->offsets_capa <<= 1;
        }
        FRT_REALLOC_N(dw->offsets, FrtOffset, dw->offsets_capa);
        FRT_ZEROSET_N(dw->offsets + old_capa, FrtOffset, dw->offsets_capa - old_capa);
    }
    dw->offsets[pos].start = start;
    dw->offsets[pos].end = end;
    dw->offsets_size = pos + 1;
}

FrtHash *frt_dw_invert_field(FrtDocWriter *dw,
                           FrtFieldInverter *fld_inv,
                           FrtDocField *df)
{
    FrtMemoryPool *mp = dw->mp;
    FrtAnalyzer *a = dw->analyzer;
    FrtHash *curr_plists = dw->curr_plists;
    FrtHash *fld_plists = fld_inv->plists;
    const bool store_offsets = fld_inv->store_offsets;
    int doc_num = dw->doc_num;
    int i;
    const int df_size = df->size;
    off_t start_offset = 0;

    if (fld_inv->is_tokenized) {
        FrtToken *tk;
        int pos = -1, num_terms = 0;

        for (i = 0; i < df_size; i++) {
            FrtTokenStream *ts = frt_a_get_ts(a, df->name, df->data[i], df->encodings[i]);
            /* ts->reset(ts, df->data[i]); no longer being called */
            if (store_offsets) {
                while (NULL != (tk = ts->next(ts))) {
                    pos += tk->pos_inc;
                    /* if for some reason pos gets set to some number less
                     * than 0 the we'll start pos at 0 */
                    if (pos < 0) {
                        pos = 0;
                    }
                    dw_add_posting(mp, curr_plists, fld_plists, doc_num,
                                   tk->text, tk->len, pos);
                    dw_add_offsets(dw, pos,
                                   start_offset + tk->start,
                                   start_offset + tk->end);
                    if (num_terms++ >= dw->max_field_length) {
                        break;
                    }
                }
            }
            else {
                while (NULL != (tk = ts->next(ts))) {
                    pos += tk->pos_inc;
                    dw_add_posting(mp, curr_plists, fld_plists, doc_num,
                                   tk->text, tk->len, pos);
                    if (num_terms++ >= dw->max_field_length) {
                        break;
                    }
                }
            }
            frt_ts_deref(ts);
            start_offset += df->lengths[i] + 1;
        }
        fld_inv->length = num_terms;
    }
    else {
        char buf[FRT_MAX_WORD_SIZE];
        buf[FRT_MAX_WORD_SIZE - 1] = '\0';
        for (i = 0; i < df_size; i++) {
            int len = df->lengths[i];
            char *data_ptr = df->data[i];
            if (len > FRT_MAX_WORD_SIZE) {
                len = FRT_MAX_WORD_SIZE - 1;
                data_ptr = (char *)memcpy(buf, df->data[i], len);
            }
            dw_add_posting(mp, curr_plists, fld_plists, doc_num, data_ptr,
                           len, i);
            if (store_offsets) {
                dw_add_offsets(dw, i, start_offset,
                               start_offset + df->lengths[i]);
            }
            start_offset += df->lengths[i] + 1;
        }
        fld_inv->length = i;
    }
    return curr_plists;
}

void frt_dw_reset_postings(FrtHash *postings)
{
    FRT_ZEROSET_N(postings->table, FrtHashEntry, postings->mask + 1);
    postings->fill = postings->size = 0;
}

void frt_dw_add_doc(FrtDocWriter *dw, FrtDocument *doc)
{
    int i;
    float boost;
    FrtDocField *df;
    FrtFieldInverter *fld_inv;
    FrtHash *postings;
    FrtFieldInfo *fi;
    const int doc_size = doc->size;

    /* frt_fw_add_doc will add new fields as necessary */
    frt_fw_add_doc(dw->fw, doc);

    for (i = 0; i < doc_size; i++) {
        df = doc->fields[i];
        fi = frt_fis_get_field(dw->fis, df->name);
        if (!fi_is_indexed(fi)) {
            continue;
        }
        fld_inv = frt_dw_get_fld_inv(dw, fi);

        postings = frt_dw_invert_field(dw, fld_inv, df);
        if (fld_inv->store_term_vector) {
            frt_fw_add_postings(dw->fw, fld_inv->fi->number,
                            dw_sort_postings(postings), postings->size,
                            dw->offsets, dw->offsets_size);
        }

        if (fld_inv->has_norms) {
            boost = fld_inv->fi->boost * doc->boost * df->boost *
                frt_sim_length_norm(dw->similarity, fi->name, fld_inv->length);
            fld_inv->norms[dw->doc_num] =
                frt_sim_encode_norm(dw->similarity, boost);
        }
        frt_dw_reset_postings(postings);
        if (dw->offsets_size > 0) {
            FRT_ZEROSET_N(dw->offsets, FrtOffset, dw->offsets_size);
            dw->offsets_size = 0;
        }
    }
    frt_fw_write_tv_index(dw->fw);
    dw->doc_num++;
}

/****************************************************************************
 *
 * IndexWriter
 *
 ****************************************************************************/
/****************************************************************************
 * SegmentMergeInfo
 ****************************************************************************/

typedef struct SegmentMergeInfo {
    int base;
    int max_doc;
    int doc_cnt;
    FrtSegmentInfo *si;
    FrtStore *store;
    FrtStore *orig_store;
    FrtBitVector *deleted_docs;
    FrtSegmentFieldIndex *sfi;
    FrtTermEnum *te;
    FrtTermDocEnum *tde;
    char *term;
    int *doc_map;
    FrtInStream *frq_in;
    FrtInStream *prx_in;
} SegmentMergeInfo;

static bool smi_lt(const SegmentMergeInfo *smi1, const SegmentMergeInfo *smi2)
{
    int cmpres = strcmp(smi1->term, smi2->term);
    if (0 == cmpres) {
        return smi1->base < smi2->base;
    }
    else {
        return cmpres < 0;
    }
}

static void smi_load_doc_map(SegmentMergeInfo *smi)
{
    FrtBitVector *deleted_docs = smi->deleted_docs;
    const int max_doc = smi->max_doc;
    int j = 0, i;

    smi->doc_map = FRT_ALLOC_N(int, max_doc);
    for (i = 0; i < max_doc; i++) {
        if (frt_bv_get(deleted_docs, i)) {
            smi->doc_map[i] = -1;
        }
        else {
            smi->doc_map[i] = j++;
        }
    }
    smi->doc_cnt = j;
}

static SegmentMergeInfo *smi_new(int base, FrtStore *store, FrtSegmentInfo *si)
{
    SegmentMergeInfo *smi = FRT_ALLOC_AND_ZERO(SegmentMergeInfo);
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    char *segment = si->name;
    smi->base = base;
    smi->si = si;
    smi->orig_store = smi->store = store;
    sprintf(file_name, "%s.cfs", segment);
    if (store->exists(store, file_name)) {
        smi->store = frt_open_cmpd_store(store, file_name);
    }


    sprintf(file_name, "%s.fdx", segment);
    smi->doc_cnt = smi->max_doc
        = smi->store->length(smi->store, file_name) / FIELDS_IDX_PTR_SIZE;

    if (si->del_gen >= 0) {
        frt_fn_for_generation(file_name, segment, "del", si->del_gen);
        smi->deleted_docs = bv_read(store, file_name);
        smi_load_doc_map(smi);
    }
    return smi;
}

static void smi_load_term_input(SegmentMergeInfo *smi)
{
    FrtStore *store = smi->store;
    char *segment = smi->si->name;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    smi->sfi = frt_sfi_open(store, segment);
    sprintf(file_name, "%s.tis", segment);
    smi->te = TE(frt_ste_new(store->open_input(store, file_name), smi->sfi));
    sprintf(file_name, "%s.frq", segment);
    smi->frq_in = store->open_input(store, file_name);
    sprintf(file_name, "%s.prx", segment);
    smi->prx_in = store->open_input(store, file_name);
    smi->tde = frt_stpe_new(NULL, smi->frq_in, smi->prx_in, smi->deleted_docs,
                        STE(smi->te)->skip_interval);
}

static void smi_close_term_input(SegmentMergeInfo *smi)
{
    frt_ste_close(smi->te);
    frt_sfi_close(smi->sfi);
    stpe_close(smi->tde);
    frt_is_close(smi->frq_in);
    frt_is_close(smi->prx_in);
}

static void smi_destroy(SegmentMergeInfo *smi)
{
    if (smi->store != smi->orig_store) {
        frt_store_deref(smi->store);
    }
    if (smi->deleted_docs) {
        frt_bv_destroy(smi->deleted_docs);
        free(smi->doc_map);
    }
    free(smi);
}

static char *smi_next(SegmentMergeInfo *smi)
{
    return (smi->term = ste_next(smi->te));
}

/****************************************************************************
 * SegmentMerger
 ****************************************************************************/

typedef struct SegmentMerger {
    FrtTermInfo ti;
    FrtStore *store;
    FrtFieldInfos *fis;
    FrtSegmentInfo *si;
    SegmentMergeInfo **smis;
    int seg_cnt;
    int doc_cnt;
    FrtConfig *config;
    FrtTermInfosWriter *tiw;
    char *term_buf;
    int term_buf_ptr;
    int term_buf_size;
    FrtPriorityQueue *queue;
    SkipBuffer *skip_buf;
    FrtOutStream *frq_out;
    FrtOutStream *prx_out;
} SegmentMerger;

static SegmentMerger *sm_create(FrtIndexWriter *iw, FrtSegmentInfo *si,
                                FrtSegmentInfo **seg_infos, const int seg_cnt)
{
    int i;
    SegmentMerger *sm = FRT_ALLOC_AND_ZERO_N(SegmentMerger, seg_cnt);
    sm->store = iw->store;
    sm->fis = iw->fis;
    sm->si = si;
    sm->doc_cnt = 0;
    sm->smis = FRT_ALLOC_N(SegmentMergeInfo *, seg_cnt);
    for (i = 0; i < seg_cnt; i++) {
        sm->smis[i] = smi_new(sm->doc_cnt, seg_infos[i]->store,
                              seg_infos[i]);
        sm->doc_cnt += sm->smis[i]->doc_cnt;
    }
    sm->seg_cnt = seg_cnt;
    sm->config = &iw->config;
    return sm;
}

static void sm_destroy(SegmentMerger *sm)
{
    int i;
    const int seg_cnt = sm->seg_cnt;
    for (i = 0; i < seg_cnt; i++) {
        smi_destroy(sm->smis[i]);
    }
    free(sm->smis);
    free(sm);
}

static void sm_merge_fields(SegmentMerger *sm)
{
    int i, j;
    off_t start, end = 0;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtOutStream *fdt_out, *fdx_out;
    FrtStore *store = sm->store;
    const int seg_cnt = sm->seg_cnt;

    sprintf(file_name, "%s.fdt", sm->si->name);
    fdt_out = store->new_output(store, file_name);

    sprintf(file_name, "%s.fdx", sm->si->name);
    fdx_out = store->new_output(store, file_name);

    for (i = 0; i < seg_cnt; i++) {
        SegmentMergeInfo *smi = sm->smis[i];
        const int max_doc = smi->max_doc;
        FrtInStream *fdt_in, *fdx_in;
        char *segment = smi->si->name;
        store = smi->store;
        sprintf(file_name, "%s.fdt", segment);
        fdt_in = store->open_input(store, file_name);
        sprintf(file_name, "%s.fdx", segment);
        fdx_in = store->open_input(store, file_name);

        if (max_doc > 0) {
            end = (off_t)frt_is_read_u64(fdx_in);
        }
        for (j = 0; j < max_doc; j++) {
            frt_u32 tv_idx_offset = frt_is_read_u32(fdx_in);
            start = end;
            if (j == max_doc - 1) {
                end = frt_is_length(fdt_in);
            }
            else {
                end = (off_t)frt_is_read_u64(fdx_in);
            }
            /* skip deleted docs */
            if (!smi->deleted_docs || !frt_bv_get(smi->deleted_docs, j)) {
                frt_os_write_u64(fdx_out, frt_os_pos(fdt_out));
                frt_os_write_u32(fdx_out, tv_idx_offset);
                frt_is_seek(fdt_in, start);
                frt_is2os_copy_bytes(fdt_in, fdt_out, end - start);
            }
        }
        frt_is_close(fdt_in);
        frt_is_close(fdx_in);
    }
    frt_os_close(fdt_out);
    frt_os_close(fdx_out);
}

static int sm_append_postings(SegmentMerger *sm, SegmentMergeInfo **matches,
                              const int match_size)
{
    int i;
    int last_doc = 0, base, doc, doc_code, freq;
    int skip_interval = sm->config->skip_interval;
    int *doc_map = NULL;
    int df = 0;            /* number of docs w/ term */
    FrtTermDocEnum *tde;
    SegmentMergeInfo *smi;
    SkipBuffer *skip_buf = sm->skip_buf;
    skip_buf_reset(skip_buf);

    for (i = 0; i < match_size; i++) {
        smi = matches[i];
        base = smi->base;
        doc_map = smi->doc_map;
        tde = smi->tde;
        stpe_seek_ti(STDE(tde), &smi->te->curr_ti);

        /* since we are using copy_bytes below to copy the proximities we use
         * stde_next rather than stpe_next here */
        while (stde_next(tde)) {
            doc = stde_doc_num(tde);
            if (NULL != doc_map) {
                doc = doc_map[doc]; /* work around deletions */
            }
            doc += base;          /* convert to merged space */
            assert(doc == 0 || doc > last_doc);

            df++;
            if (0 == (df % skip_interval)) {
                skip_buf_add(skip_buf, last_doc);
            }

            doc_code = (doc - last_doc) << 1;    /* use low bit to flag freq=1 */
            last_doc = doc;

            freq = stde_freq(tde);
            if (freq == 1) {
                frt_os_write_vint(sm->frq_out, doc_code | 1); /* doc & freq=1 */
            }
            else {
                frt_os_write_vint(sm->frq_out, doc_code); /* write doc */
                frt_os_write_vint(sm->frq_out, freq);     /* write freqency in doc */
            }

            /* copy position deltas */
            frt_is2os_copy_vints(STDE(tde)->prx_in, sm->prx_out, freq);
        }
    }
    return df;
}

static char *sm_cache_term(SegmentMerger *sm, char *term, int term_len)
{
    term = (char *)memcpy(sm->term_buf + sm->term_buf_ptr, term, term_len + 1);
    sm->term_buf_ptr += term_len + 1;
    if (sm->term_buf_ptr > sm->term_buf_size) {
        sm->term_buf_ptr = 0;
    }
    return term;
}

static void sm_merge_term_info(SegmentMerger *sm, SegmentMergeInfo **matches,
                               int match_size)
{
    off_t frq_ptr = frt_os_pos(sm->frq_out);
    off_t prx_ptr = frt_os_pos(sm->prx_out);

    int df = sm_append_postings(sm, matches, match_size); /* append posting data */

    off_t skip_ptr = skip_buf_write(sm->skip_buf);

    if (df > 0) {
        /* add an entry to the dictionary with ptrs to prox and freq files */
        SegmentMergeInfo *first_match = matches[0];
        int term_len = first_match->te->curr_term_len;

        frt_ti_set(sm->ti, df, frq_ptr, prx_ptr,
               (skip_ptr - frq_ptr));
        frt_tiw_add(sm->tiw, sm_cache_term(sm, first_match->term, term_len),
                term_len, &sm->ti);
    }
}

static void sm_merge_term_infos(SegmentMerger *sm)
{
    int i, j, match_size;
    SegmentMergeInfo *smi, *top, **matches;
    char *term;
    const int seg_cnt = sm->seg_cnt;
    const int fis_size = sm->fis->size;

    matches = FRT_ALLOC_N(SegmentMergeInfo *, seg_cnt);

    for (j = 0; j < seg_cnt; j++) {
        smi_load_term_input(sm->smis[j]);
    }

    for (i = 0; i < fis_size; i++) {
        frt_tiw_start_field(sm->tiw, i);
        for (j = 0; j < seg_cnt; j++) {
            smi = sm->smis[j];
            ste_set_field(smi->te, i);
            if (NULL != smi_next(smi)) {
                frt_pq_push(sm->queue, smi); /* initialize @queue */
            }
        }
        while (sm->queue->size > 0) {
            match_size = 0;     /* pop matching terms */
            matches[0] = (SegmentMergeInfo *)frt_pq_pop(sm->queue);
            match_size++;
            term = matches[0]->term;
            top = (SegmentMergeInfo *)frt_pq_top(sm->queue);
            while ((NULL != top) && (0 == strcmp(term, top->term))) {
                matches[match_size] = (SegmentMergeInfo *)frt_pq_pop(sm->queue);
                match_size++;
                top = (SegmentMergeInfo *)frt_pq_top(sm->queue);
            }

            sm_merge_term_info(sm, matches, match_size);/* add new FrtTermInfo */

            while (match_size > 0) {
                match_size--;
                smi = matches[match_size];
                if (NULL != smi_next(smi)) {
                    frt_pq_push(sm->queue, smi);   /* restore queue */
                }
            }
        }
    }
    free(matches);
    for (j = 0; j < seg_cnt; j++) {
        smi_close_term_input(sm->smis[j]);
    }
}

static void sm_merge_terms(SegmentMerger *sm)
{
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];

    sprintf(file_name, "%s.frq", sm->si->name);
    sm->frq_out = sm->store->new_output(sm->store, file_name);
    sprintf(file_name, "%s.prx", sm->si->name);
    sm->prx_out = sm->store->new_output(sm->store, file_name);

    sm->tiw = frt_tiw_open(sm->store, sm->si->name, sm->config->index_interval,
                       sm->config->skip_interval);
    sm->skip_buf = skip_buf_new(sm->frq_out, sm->prx_out);

    /* terms_buf_ptr holds a buffer of terms since the FrtTermInfosWriter needs
     * to keep the last index_interval terms so that it can compare the last
     * term put in the index with the next one. So the size of the buffer must
     * by index_interval + 2. */
    sm->term_buf_ptr = 0;
    sm->term_buf_size = (sm->config->index_interval + 1) * FRT_MAX_WORD_SIZE;
    sm->term_buf = FRT_ALLOC_N(char, sm->term_buf_size + FRT_MAX_WORD_SIZE);

    sm->queue = frt_pq_new(sm->seg_cnt, (frt_lt_ft)&smi_lt, NULL);

    sm_merge_term_infos(sm);

    frt_os_close(sm->frq_out);
    frt_os_close(sm->prx_out);
    frt_tiw_close(sm->tiw);
    frt_pq_destroy(sm->queue);
    skip_buf_destroy(sm->skip_buf);
    free(sm->term_buf);
}

static void sm_merge_norms(SegmentMerger *sm)
{
    FrtSegmentInfo *si;
    int i, j, k;
    FrtStore *store;
    frt_uchar byte;
    FrtFieldInfo *fi;
    FrtOutStream *os;
    FrtInStream *is;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    SegmentMergeInfo *smi;
    const int seg_cnt = sm->seg_cnt;
    for (i = sm->fis->size - 1; i >= 0; i--) {
        fi = sm->fis->fields[i];
        if (fi_has_norms(fi))  {
            si = sm->si;
            frt_si_advance_norm_gen(si, i);
            si_norm_file_name(si, file_name, i);
            os = sm->store->new_output(sm->store, file_name);
            for (j = 0; j < seg_cnt; j++) {
                smi = sm->smis[j];
                si = smi->si;
                if (si_norm_file_name(si, file_name, i)) {
                    const int max_doc = smi->max_doc;
                    FrtBitVector *deleted_docs =  smi->deleted_docs;
                    store = (si->use_compound_file && si->norm_gens[i])
                             ? smi->orig_store : smi->store;
                    is = store->open_input(store, file_name);
                    if (deleted_docs) {
                        for (k = 0; k < max_doc; k++) {
                            byte = frt_is_read_byte(is);
                            if (!frt_bv_get(deleted_docs, k)) {
                                frt_os_write_byte(os, byte);
                            }
                        }
                    }
                    else {
                        frt_is2os_copy_bytes(is, os, max_doc);
                    }
                    frt_is_close(is);
                }
                else {
                    const int doc_cnt = smi->doc_cnt;
                    for (k = 0; k < doc_cnt; k++) {
                        frt_os_write_byte(os, '\0');
                    }
                }
            }
            frt_os_close(os);
        }
    }
}

static int sm_merge(SegmentMerger *sm)
{
    sm_merge_fields(sm);
    sm_merge_terms(sm);
    sm_merge_norms(sm);
    return sm->doc_cnt;
}


/****************************************************************************
 * IndexWriter
 ****************************************************************************/

/* prepare an index ready for writing */
void frt_index_create(FrtStore *store, FrtFieldInfos *fis)
{
    FrtSegmentInfos *sis = frt_sis_new(fis);
    store->clear_all(store);
    frt_sis_write(sis, store, NULL);
    frt_sis_destroy(sis);
}

bool frt_index_is_locked(FrtStore *store) {
    FrtLock *write_lock = frt_open_lock(store, FRT_WRITE_LOCK_NAME);
    bool is_locked = write_lock->is_locked(write_lock);
    frt_close_lock(write_lock);
    return is_locked;
}

int frt_iw_doc_count(FrtIndexWriter *iw)
{
    int i, doc_cnt = 0;
    frt_mutex_lock(&iw->mutex);
    for (i = iw->sis->size - 1; i >= 0; i--) {
        doc_cnt += iw->sis->segs[i]->doc_cnt;
    }
    if (iw->dw) {
        doc_cnt += iw->dw->doc_num;
    }
    frt_mutex_unlock(&iw->mutex);
    return doc_cnt;
}

#define MOVE_TO_COMPOUND_DIR(file_name)\
    deleter_queue_file(dlr, file_name);\
    frt_cw_add_file(cw, file_name)

static void iw_create_compound_file(FrtStore *store, FrtFieldInfos *fis,
                                    FrtSegmentInfo *si, char *cfs_file_name,
                                    FrtDeleter *dlr)
{
    int i;
    FrtCompoundWriter *cw;
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    char *ext;
    int seg_len = strlen(si->name);

    memcpy(file_name, si->name, seg_len);
    file_name[seg_len] = '.';
    ext = file_name + seg_len + 1;

    cw = frt_open_cw(store, cfs_file_name);
    for (i = 0; i < FRT_NELEMS(COMPOUND_EXTENSIONS); i++) {
        memcpy(ext, COMPOUND_EXTENSIONS[i], 4);
        MOVE_TO_COMPOUND_DIR(file_name);
    }

    /* Field norm file_names */
    for (i = fis->size - 1; i >= 0; i--) {
        if (fi_has_norms(fis->fields[i])
            && si_norm_file_name(si, file_name, i)) {
            MOVE_TO_COMPOUND_DIR(file_name);
        }
    }

    /* Perform the merge */
    frt_cw_close(cw);
}

static void iw_commit_compound_file(FrtIndexWriter *iw, FrtSegmentInfo *si)
{
    char cfs_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    sprintf(cfs_name, "%s.cfs", si->name);

    iw_create_compound_file(iw->store, iw->fis, si, cfs_name, iw->deleter);
}

static void iw_merge_segments(FrtIndexWriter *iw, const int min_seg,
                              const int max_seg)
{
    int i;
    FrtSegmentInfos *sis = iw->sis;
    FrtSegmentInfo *si = frt_sis_new_segment(sis, 0, iw->store);

    SegmentMerger *merger = sm_create(iw, si, &sis->segs[min_seg],
                                      max_seg - min_seg);

    /* This is where all the action happens. */
    si->doc_cnt = sm_merge(merger);

    frt_mutex_lock(&iw->store->mutex);
    /* delete merged segments */
    for (i = min_seg; i < max_seg; i++) {
        si_delete_files(sis->segs[i], iw->fis, iw->deleter);
    }

    frt_sis_del_from_to(sis, min_seg, max_seg);

    if (iw->config.use_compound_file) {
        iw_commit_compound_file(iw, si);
        si->use_compound_file = true;
    }

    frt_sis_write(sis, iw->store, iw->deleter);
    deleter_commit_pending_deletions(iw->deleter);

    frt_mutex_unlock(&iw->store->mutex);

    sm_destroy(merger);
}

static void iw_merge_segments_from(FrtIndexWriter *iw, int min_segment)
{
    iw_merge_segments(iw, min_segment, iw->sis->size);
}

static void iw_maybe_merge_segments(FrtIndexWriter *iw)
{
    int target_merge_docs = iw->config.merge_factor;
    int min_segment, merge_docs;
    FrtSegmentInfo *si;

    while (target_merge_docs > 0
           && target_merge_docs <= iw->config.max_merge_docs) {
        /* find segments smaller than current target size */
        min_segment = iw->sis->size - 1;
        merge_docs = 0;
        while (min_segment >= 0) {
            si = iw->sis->segs[min_segment];
            if (si->doc_cnt >= target_merge_docs) {
                break;
            }
            merge_docs += si->doc_cnt;
            min_segment--;
        }

        if (merge_docs >= target_merge_docs) { /* found a merge to do */
            iw_merge_segments_from(iw, min_segment + 1);
        }
        else if (min_segment <= 0) {
            break;
        }

        target_merge_docs *= iw->config.merge_factor;
    }
}

static void iw_flush_ram_segment(FrtIndexWriter *iw)
{
    FrtSegmentInfos *sis = iw->sis;
    FrtSegmentInfo *si;
    si = sis->segs[sis->size - 1];
    si->doc_cnt = iw->dw->doc_num;
    dw_flush(iw->dw);
    frt_mutex_lock(&iw->store->mutex);
    if (iw->config.use_compound_file) {
        iw_commit_compound_file(iw, si);
        si->use_compound_file = true;
    }
    /* commit the segments file and the fields file */
    frt_sis_write(iw->sis, iw->store, iw->deleter);
    deleter_commit_pending_deletions(iw->deleter);
    frt_mutex_unlock(&iw->store->mutex);
    iw_maybe_merge_segments(iw);
}

void frt_iw_add_doc(FrtIndexWriter *iw, FrtDocument *doc)
{
    frt_mutex_lock(&iw->mutex);
    if (NULL == iw->dw) {
        iw->dw = frt_dw_open(iw, frt_sis_new_segment(iw->sis, 0, iw->store));
    }
    else if (NULL == iw->dw->fw) {
        frt_dw_new_segment(iw->dw, frt_sis_new_segment(iw->sis, 0, iw->store));
    }
    frt_dw_add_doc(iw->dw, doc);
    if (frt_mp_used(iw->dw->mp) > iw->config.max_buffer_memory
        || iw->dw->doc_num >= iw->config.max_buffered_docs) {
        iw_flush_ram_segment(iw);
    }
    frt_mutex_unlock(&iw->mutex);
}

static void iw_commit_i(FrtIndexWriter *iw)
{
    if (iw->dw && iw->dw->doc_num > 0) {
        iw_flush_ram_segment(iw);
    }
}

void frt_iw_commit(FrtIndexWriter *iw)
{
    frt_mutex_lock(&iw->mutex);
    iw_commit_i(iw);
    frt_mutex_unlock(&iw->mutex);
}

void frt_iw_delete_term(FrtIndexWriter *iw, FrtSymbol field, const char *term)
{
    int field_num = frt_fis_get_field_num(iw->fis, field);
    if (field_num >= 0) {
        int i;
        frt_mutex_lock(&iw->mutex);
        iw_commit_i(iw);
        do {
            FrtSegmentInfos *sis = iw->sis;
            const int seg_cnt = sis->size;
            bool did_delete = false;
            for (i = 0; i < seg_cnt; i++) {
                FrtIndexReader *ir = sr_open(sis, iw->fis, i, false);
                FrtTermDocEnum *tde = ir->term_docs(ir);
                ir->deleter = iw->deleter;
                stde_seek(tde, field_num, term);
                while (tde->next(tde)) {
                    did_delete = true;
                    sr_delete_doc_i(ir, STDE(tde)->doc_num);
                }
                tde_destroy(tde);
                sr_commit_i(ir);
                frt_ir_close(ir);
            }
            if (did_delete) {
                frt_mutex_lock(&iw->store->mutex);
                frt_sis_write(iw->sis, iw->store, iw->deleter);
                frt_mutex_unlock(&iw->store->mutex);
            }
        } while (0);
        frt_mutex_unlock(&iw->mutex);
    }
}

void frt_iw_delete_terms(FrtIndexWriter *iw, FrtSymbol field,
                     char **terms, const int term_cnt)
{
    int field_num = frt_fis_get_field_num(iw->fis, field);
    if (field_num >= 0) {
        int i;
        frt_mutex_lock(&iw->mutex);
        iw_commit_i(iw);
        do {
            FrtSegmentInfos *sis = iw->sis;
            const int seg_cnt = sis->size;
            bool did_delete = false;
            for (i = 0; i < seg_cnt; i++) {
                FrtIndexReader *ir = sr_open(sis, iw->fis, i, false);
                FrtTermDocEnum *tde = ir->term_docs(ir);
                int j;
                for (j = 0 ; j < term_cnt; j++) {
                    const char *term = terms[j];
                    ir->deleter = iw->deleter;
                    stde_seek(tde, field_num, term);
                    while (tde->next(tde)) {
                        did_delete = true;
                        sr_delete_doc_i(ir, STDE(tde)->doc_num);
                    }
                }
                tde_destroy(tde);
                sr_commit_i(ir);
                frt_ir_close(ir);
            }
            if (did_delete) {
                frt_mutex_lock(&iw->store->mutex);
                frt_sis_write(iw->sis, iw->store, iw->deleter);
                frt_mutex_unlock(&iw->store->mutex);
            }
        } while (0);
        frt_mutex_unlock(&iw->mutex);
    }
}

static void iw_optimize_i(FrtIndexWriter *iw)
{
    int min_segment;
    iw_commit_i(iw);
    while (iw->sis->size > 1
           || (iw->sis->size == 1
               && (frt_si_has_deletions(iw->sis->segs[0])
                   || (iw->sis->segs[0]->store != iw->store)
                   || (iw->config.use_compound_file
                       && (!iw->sis->segs[0]->use_compound_file
                           || frt_si_has_separate_norms(iw->sis->segs[0])))))) {
        min_segment = iw->sis->size - iw->config.merge_factor;
        iw_merge_segments_from(iw, min_segment < 0 ? 0 : min_segment);
    }
}

void frt_iw_optimize(FrtIndexWriter *iw)
{
    frt_mutex_lock(&iw->mutex);
    iw_optimize_i(iw);
    frt_mutex_unlock(&iw->mutex);
}

void frt_iw_close(FrtIndexWriter *iw)
{
    frt_mutex_lock(&iw->mutex);
    iw_commit_i(iw);
    if (iw->dw) {
        frt_dw_close(iw->dw);
    }
    frt_a_deref(iw->analyzer);
    frt_sis_destroy(iw->sis);
    frt_fis_deref(iw->fis);
    frt_sim_destroy(iw->similarity);
    iw->write_lock->release(iw->write_lock);
    frt_close_lock(iw->write_lock);
    iw->write_lock = NULL;
    frt_store_deref(iw->store);
    frt_deleter_destroy(iw->deleter);
    frt_mutex_destroy(&iw->mutex);
    free(iw);
}

FrtIndexWriter *frt_iw_open(FrtStore *store, FrtAnalyzer *volatile analyzer,
                     const FrtConfig *config)
{
    FrtIndexWriter *iw = FRT_ALLOC_AND_ZERO(FrtIndexWriter);
    frt_mutex_init(&iw->mutex, NULL);
    iw->store = store;
    if (!config) {
        config = &frt_default_config;
    }
    iw->config = *config;

    FRT_TRY
        iw->write_lock = frt_open_lock(store, FRT_WRITE_LOCK_NAME);
        if (!iw->write_lock->obtain(iw->write_lock)) {
            FRT_RAISE(FRT_LOCK_ERROR,
                  "Couldn't obtain write lock when opening IndexWriter");
        }

        iw->sis = frt_sis_read(store);
        iw->fis = iw->sis->fis;
        FRT_REF(iw->fis);
    FRT_XCATCHALL
        if (iw->write_lock) {
            iw->write_lock->release(iw->write_lock);
            frt_close_lock(iw->write_lock);
            iw->write_lock = NULL;
        }
        if (iw->sis) frt_sis_destroy(iw->sis);
        if (analyzer) frt_a_deref((FrtAnalyzer *)analyzer);
        free(iw);
    FRT_XENDTRY

    iw->similarity = frt_sim_create_default();
    iw->analyzer = analyzer ? (FrtAnalyzer *)analyzer
                            : frt_standard_analyzer_new(true);

    iw->deleter = frt_deleter_new(iw->sis, store);
    deleter_delete_deletable_files(iw->deleter);

    FRT_REF(store);
    return iw;
}

/*******************/
/*** Add Indexes ***/
/*******************/
static void iw_cp_fields(FrtIndexWriter *iw, SegmentReader *sr,
                         const char *segment, int *map)
{
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtOutStream *fdt_out, *fdx_out;
    FrtInStream *fdt_in, *fdx_in;
    FrtStore *store_in = sr->cfs_store ? sr->cfs_store : sr->ir.store;
    FrtStore *store_out = iw->store;
    char *sr_segment = sr->si->name;

    sprintf(file_name, "%s.fdt", segment);
    fdt_out = store_out->new_output(store_out, file_name);
    sprintf(file_name, "%s.fdx", segment);
    fdx_out = store_out->new_output(store_out, file_name);

    sprintf(file_name, "%s.fdt", sr_segment);
    fdt_in = store_in->open_input(store_in, file_name);
    sprintf(file_name, "%s.fdx", sr_segment);
    fdx_in = store_in->open_input(store_in, file_name);

    sprintf(file_name, "%s.del", sr_segment);
    if (store_in->exists(store_in, file_name)) {
        FrtOutStream *del_out;
        FrtInStream *del_in = store_in->open_input(store_in, file_name);
        sprintf(file_name, "%s.del", segment);
        del_out = store_out->new_output(store_out, file_name);
        frt_is2os_copy_bytes(del_in, del_out, frt_is_length(del_in));
    }


    if (map) {
        int i;
        const int max_doc = sr_max_doc(IR(sr));
        for (i = 0; i < max_doc; i++) {
            int j, data_len = 0;
            const int field_cnt = frt_is_read_vint(fdt_in);
            int tv_cnt;
            off_t doc_start_ptr = frt_os_pos(fdt_out);

            frt_os_write_u64(fdx_out, doc_start_ptr);
            frt_os_write_vint(fdt_out, field_cnt);

            for (j = 0; j < field_cnt; j++) {
                int k;
                const int field_num = map[frt_is_read_vint(fdt_in)];
                const int df_size = frt_is_read_vint(fdt_in);
                frt_os_write_vint(fdt_out, field_num);
                frt_os_write_vint(fdt_out, df_size);
                /* sum total lengths of FrtDocField */
                for (k = 0; k < df_size; k++) {
                    /* Each field has one ' ' byte so add 1 */
                    const int flen = frt_is_read_vint(fdt_in);
                    frt_os_write_vint(fdt_out, flen);
                    data_len +=  flen + 1;
                }
            }
            frt_is2os_copy_bytes(fdt_in, fdt_out, data_len);

            /* Write TermVectors */
            /* write TVs up to TV index */
            frt_is2os_copy_bytes(fdt_in, fdt_out,
                             (int)(frt_is_read_u64(fdx_in)
                                   + (frt_u64)frt_is_read_u32(fdx_in)
                                   - (frt_u64)frt_is_pos(fdt_in)));

            /* Write TV index pos */
            frt_os_write_u32(fdx_out, (frt_u32)(frt_os_pos(fdt_out) - doc_start_ptr));
            tv_cnt = frt_is_read_vint(fdt_in);
            frt_os_write_vint(fdt_out, tv_cnt);
            for (j = 0; j < tv_cnt; j++) {
                const int field_num = map[frt_is_read_vint(fdt_in)];
                const int tv_size = frt_is_read_vint(fdt_in);
                frt_os_write_vint(fdt_out, field_num);
                frt_os_write_vint(fdt_out, tv_size);
            }
        }
    }
    else {
        frt_is2os_copy_bytes(fdt_in, fdt_out, frt_is_length(fdt_in));
        frt_is2os_copy_bytes(fdx_in, fdx_out, frt_is_length(fdx_in));
    }
    frt_is_close(fdt_in);
    frt_is_close(fdx_in);
    frt_os_close(fdt_out);
    frt_os_close(fdx_out);
}

static void iw_cp_terms(FrtIndexWriter *iw, SegmentReader *sr,
                        const char *segment, int *map)
{
    char file_name[FRT_SEGMENT_NAME_MAX_LENGTH];
    FrtOutStream *tix_out, *tis_out, *tfx_out, *frq_out, *prx_out;
    FrtInStream *tix_in, *tis_in, *tfx_in, *frq_in, *prx_in;
    FrtStore *store_out = iw->store;
    FrtStore *store_in = sr->cfs_store ? sr->cfs_store : sr->ir.store;
    char *sr_segment = sr->si->name;

    sprintf(file_name, "%s.tix", segment);
    tix_out = store_out->new_output(store_out, file_name);
    sprintf(file_name, "%s.tix", sr_segment);
    tix_in = store_in->open_input(store_in, file_name);

    sprintf(file_name, "%s.tis", segment);
    tis_out = store_out->new_output(store_out, file_name);
    sprintf(file_name, "%s.tis", sr_segment);
    tis_in = store_in->open_input(store_in, file_name);

    sprintf(file_name, "%s.tfx", segment);
    tfx_out = store_out->new_output(store_out, file_name);
    sprintf(file_name, "%s.tfx", sr_segment);
    tfx_in = store_in->open_input(store_in, file_name);

    sprintf(file_name, "%s.frq", segment);
    frq_out = store_out->new_output(store_out, file_name);
    sprintf(file_name, "%s.frq", sr_segment);
    frq_in = store_in->open_input(store_in, file_name);

    sprintf(file_name, "%s.prx", segment);
    prx_out = store_out->new_output(store_out, file_name);
    sprintf(file_name, "%s.prx", sr_segment);
    prx_in = store_in->open_input(store_in, file_name);

    if (map) {
        int field_cnt = frt_is_read_u32(tfx_in);
        frt_os_write_u32(tfx_out, field_cnt);
        frt_os_write_vint(tfx_out, frt_is_read_vint(tfx_in)); /* index_interval */
        frt_os_write_vint(tfx_out, frt_is_read_vint(tfx_in)); /* skip_interval */

        for (; field_cnt > 0; field_cnt--) {
            frt_os_write_vint(tfx_out, map[frt_is_read_vint(tfx_in)]);/* mapped field */
            frt_os_write_voff_t(tfx_out, frt_is_read_voff_t(tfx_in)); /* index ptr */
            frt_os_write_voff_t(tfx_out, frt_is_read_voff_t(tfx_in)); /* dict ptr */
            frt_os_write_vint(tfx_out, frt_is_read_vint(tfx_in));     /* index size */
            frt_os_write_vint(tfx_out, frt_is_read_vint(tfx_in));     /* dict size */
        }
    }
    else {
        frt_is2os_copy_bytes(tfx_in, tfx_out, frt_is_length(tfx_in));
    }
    frt_is2os_copy_bytes(tix_in, tix_out, frt_is_length(tix_in));
    frt_is2os_copy_bytes(tis_in, tis_out, frt_is_length(tis_in));
    frt_is2os_copy_bytes(frq_in, frq_out, frt_is_length(frq_in));
    frt_is2os_copy_bytes(prx_in, prx_out, frt_is_length(prx_in));

    frt_is_close(tix_in);
    frt_os_close(tix_out);
    frt_is_close(tis_in);
    frt_os_close(tis_out);
    frt_is_close(tfx_in);
    frt_os_close(tfx_out);
    frt_is_close(frq_in);
    frt_os_close(frq_out);
    frt_is_close(prx_in);
    frt_os_close(prx_out);
}

static void iw_cp_norms(FrtIndexWriter *iw, SegmentReader *sr,
                        FrtSegmentInfo *si, int *map)
{
    int i;
    FrtFieldInfos *fis = IR(sr)->fis;
    const int field_cnt = fis->size;
    FrtInStream *norms_in;
    FrtOutStream *norms_out;
    FrtStore *store_out = iw->store;
    char file_name_in[FRT_SEGMENT_NAME_MAX_LENGTH];
    char file_name_out[FRT_SEGMENT_NAME_MAX_LENGTH];

    for (i = 0; i < field_cnt; i++) {
        if (fi_has_norms(fis->fields[i])
            && si_norm_file_name(sr->si, file_name_in, i)) {
            FrtStore *store = (sr->si->use_compound_file
                            && sr->si->norm_gens[i] == 0) ? sr->cfs_store
                                                          : IR(sr)->store;
            int field_num = map ? map[i] : i;

            norms_in = store->open_input(store, file_name_in);
            frt_si_advance_norm_gen(si, field_num);
            si_norm_file_name(si, file_name_out, field_num);
            norms_out = store_out->new_output(store_out, file_name_out);
            frt_is2os_copy_bytes(norms_in, norms_out, frt_is_length(norms_in));
            frt_os_close(norms_out);
            frt_is_close(norms_in);
        }
    }
}

static void iw_cp_map_files(FrtIndexWriter *iw, SegmentReader *sr,
                            FrtSegmentInfo *si)
{
    int i;
    FrtFieldInfos *from_fis = IR(sr)->fis;
    FrtFieldInfos *to_fis = iw->fis;
    const int map_size = from_fis->size;
    int *field_map = FRT_ALLOC_N(int, map_size);

    for (i = 0; i < map_size; i++) {
        field_map[i] = frt_fis_get_field_num(to_fis, from_fis->fields[i]->name);
    }

    iw_cp_fields(iw, sr, si->name, field_map);
    iw_cp_terms( iw, sr, si->name, field_map);
    iw_cp_norms( iw, sr, si,       field_map);

    free(field_map);
}

static void iw_cp_files(FrtIndexWriter *iw, SegmentReader *sr,
                        FrtSegmentInfo *si)
{
    iw_cp_fields(iw, sr, si->name, NULL);
    iw_cp_terms( iw, sr, si->name, NULL);
    iw_cp_norms( iw, sr, si,       NULL);
}

static void iw_add_segment(FrtIndexWriter *iw, SegmentReader *sr)
{
    FrtSegmentInfo *si = frt_sis_new_segment(iw->sis, 0, iw->store);
    FrtFieldInfos *fis = iw->fis;
    FrtFieldInfos *sub_fis = sr->ir.fis;
    int j;
    const int fis_size = sub_fis->size;
    bool must_map_fields = false;

    si->doc_cnt = IR(sr)->max_doc(IR(sr));
    /* Merge FrtFieldInfos */
    for (j = 0; j < fis_size; j++) {
        FrtFieldInfo *fi = sub_fis->fields[j];
        FrtFieldInfo *new_fi = frt_fis_get_field(fis, fi->name);
        if (NULL == new_fi) {
            new_fi = frt_fi_new(fi->name, FRT_STORE_NO, FRT_INDEX_NO, FRT_TERM_VECTOR_NO);
            new_fi->bits = fi->bits;
            frt_fis_add_field(fis, new_fi);
        }
        new_fi->bits |= fi->bits;
        if (fi->number != new_fi->number) {
            must_map_fields = true;
        }
    }

    if (must_map_fields) {
        iw_cp_map_files(iw, sr, si);
    }
    else {
        iw_cp_files(iw, sr, si);
    }
}

static void iw_add_segments(FrtIndexWriter *iw, FrtIndexReader *ir)
{
    if (ir->num_docs == sr_num_docs) {
        iw_add_segment(iw, SR(ir));
    }
    else {
        int i;
        const int r_cnt = MR(ir)->r_cnt;
        FrtIndexReader **readers = MR(ir)->sub_readers;
        for (i = 0; i < r_cnt; i++) {
            iw_add_segments(iw, readers[i]);
        }
    }
}

void frt_iw_add_readers(FrtIndexWriter *iw, FrtIndexReader **readers, const int r_cnt)
{
    int i;
    frt_mutex_lock(&iw->mutex);
    iw_optimize_i(iw);

    for (i = 0; i < r_cnt; i++) {
        iw_add_segments(iw, readers[i]);
    }

    frt_mutex_lock(&iw->store->mutex);

    /* commit the segments file and the fields file */
    frt_sis_write(iw->sis, iw->store, iw->deleter);
    frt_mutex_unlock(&iw->store->mutex);

    iw_optimize_i(iw);
    frt_mutex_unlock(&iw->mutex);
}
