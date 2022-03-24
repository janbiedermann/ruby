#ifndef FRT_DOCUMENT_H
#define FRT_DOCUMENT_H

#include "frt_global.h"
#include "frt_hash.h"
#include <ruby/encoding.h>

/****************************************************************************
 *
 * FrtDocField
 *
 ****************************************************************************/

#define FRT_DF_INIT_CAPA 1
typedef struct FrtDocField {
    FrtSymbol name;
    int size;
    int capa;
    int *lengths;
    rb_encoding **encodings; /* used for processing */
    char **data;
    float boost;
    bool destroy_data : 1;
    bool is_compressed : 1;
} FrtDocField;

extern FrtDocField *frt_df_new(FrtSymbol name);
extern FrtDocField *frt_df_add_data(FrtDocField *df, char *data, rb_encoding *encoding);
extern FrtDocField *frt_df_add_data_len(FrtDocField *df, char *data, int len, rb_encoding *encoding);
extern void frt_df_destroy(FrtDocField *df);
extern char *frt_df_to_s(FrtDocField *df);

/****************************************************************************
 *
 * FrtDocument
 *
 ****************************************************************************/

#define FRT_DOC_INIT_CAPA 8
typedef struct FrtDocument {
    FrtHash *field_dict;
    int size;
    int capa;
    FrtDocField **fields;
    float boost;
} FrtDocument;

extern FrtDocument *frt_doc_new();
extern FrtDocField *frt_doc_add_field(FrtDocument *doc, FrtDocField *df);
extern FrtDocField *frt_doc_get_field(FrtDocument *doc, FrtSymbol name);
extern void frt_doc_destroy(FrtDocument *doc);

#endif
