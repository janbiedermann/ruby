#ifndef FRT_ANALYSIS_H
#define FRT_ANALYSIS_H

#include "frt_global.h"
#include "frt_hash.h"
#include "frt_multimapper.h"
#include <ruby/encoding.h>

/*****************************************************************************/
/*** FrtToken ****************************************************************/
/*****************************************************************************/

typedef struct FrtToken {
    char  text[FRT_MAX_WORD_SIZE];
    int   len;
    off_t start;
    off_t end;
    int   pos_inc;
} FrtToken;

extern FrtToken *frt_tk_new();
extern void      frt_tk_destroy(void *p);
extern FrtToken *frt_tk_set(FrtToken *tk, char *text, int tlen, off_t start, off_t end, int pos_inc, rb_encoding *encoding);
extern FrtToken *frt_tk_set_no_len(FrtToken *tk, char *text, off_t start, off_t end, int pos_inc, rb_encoding *encoding);
extern int       frt_tk_eq(FrtToken *tk1, FrtToken *tk2);
extern int       frt_tk_cmp(FrtToken *tk1, FrtToken *tk2);

/*****************************************************************************/
/*** FrtTokenStream **********************************************************/
/*****************************************************************************/

typedef struct FrtTokenStream FrtTokenStream;
struct FrtTokenStream {
    char            *t;             /* ptr used to scan text */
    char            *text;
    int              length;
    rb_encoding     *encoding;
    FrtToken        *(*next)(FrtTokenStream *ts);
    FrtTokenStream  *(*reset)(FrtTokenStream *ts, char *text, rb_encoding *encoding);
    FrtTokenStream  *(*clone_i)(FrtTokenStream *ts);
    void            (*destroy_i)(FrtTokenStream *ts);
    int              ref_cnt;
};

extern FrtTokenStream *frt_ts_new_i(size_t size);
extern FrtTokenStream *frt_ts_init(FrtTokenStream *ts);
extern FrtTokenStream *frt_ts_clone_size(FrtTokenStream *orig_ts, size_t size);

typedef struct FrtCachedTokenStream {
    FrtTokenStream super;
    FrtToken       token;
} FrtCachedTokenStream;

typedef struct FrtStandardTokenizer {
    FrtCachedTokenStream super;
} FrtStandardTokenizer;

typedef struct FrtTokenFilter {
    FrtTokenStream super;
    FrtTokenStream *sub_ts;
} FrtTokenFilter;

extern FrtTokenStream *frt_filter_clone_size(FrtTokenStream *ts, size_t size);
#define frt_tf_new(type, sub) frt_tf_new_i(sizeof(type), sub)
extern FrtTokenStream *frt_tf_new_i(size_t size, FrtTokenStream *sub_ts);

#define frt_ts_next(mts) mts->next(mts)
#define frt_ts_clone(mts) mts->clone_i(mts)

extern void frt_ts_deref(FrtTokenStream *ts);

extern FrtTokenStream *frt_non_tokenizer_new();

/*****************************************************************************/
/*** FrtWhiteSpaceTokenizer **************************************************/
/*****************************************************************************/

extern FrtTokenStream *frt_whitespace_tokenizer_alloc();
extern FrtTokenStream *frt_whitespace_tokenizer_init(FrtTokenStream *ts, bool lowercase);
extern FrtTokenStream *frt_whitespace_tokenizer_new(bool lowercase);

/*****************************************************************************/
/*** FrtLetterTokenizer ******************************************************/
/*****************************************************************************/

extern FrtTokenStream *frt_letter_tokenizer_alloc();
extern FrtTokenStream *frt_letter_tokenizer_init(FrtTokenStream *ts, bool lowercase);
extern FrtTokenStream *frt_letter_tokenizer_new(bool lowercase);

/*****************************************************************************/
/*** FrtStandardTokenizer ****************************************************/
/*****************************************************************************/

extern FrtTokenStream *frt_standard_tokenizer_alloc();
extern FrtTokenStream *frt_standard_tokenizer_init(FrtTokenStream *ts, bool lowercase);
extern FrtTokenStream *frt_standard_tokenizer_new(bool lowercase);

/*****************************************************************************/
/*** FrtHyphenFilter *********************************************************/
/*****************************************************************************/

typedef struct FrtHyphenFilter {
    FrtTokenFilter  super;
    char            text[FRT_MAX_WORD_SIZE];
    int             start;
    int             pos;
    int             len;
    FrtToken       *tk;
} FrtHyphenFilter;

extern FrtTokenStream *frt_hyphen_filter_alloc();
extern FrtTokenStream *frt_hyphen_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts);
extern FrtTokenStream *frt_hyphen_filter_new(FrtTokenStream *sub_ts);

/*****************************************************************************/
/*** FrtLowercaseFilter ******************************************************/
/*****************************************************************************/

extern FrtTokenStream *frt_lowercase_filter_alloc();
extern void            frt_lowercase_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts);
extern FrtTokenStream *frt_lowercase_filter_new(FrtTokenStream *sub_ts);

/*****************************************************************************/
/*** FrtStopFilter ***********************************************************/
/*****************************************************************************/

extern const char *FRT_ENGLISH_STOP_WORDS[];
extern const char *FRT_FULL_ENGLISH_STOP_WORDS[];
extern const char *FRT_EXTENDED_ENGLISH_STOP_WORDS[];
extern const char *FRT_FULL_FRENCH_STOP_WORDS[];
extern const char *FRT_FULL_SPANISH_STOP_WORDS[];
extern const char *FRT_FULL_PORTUGUESE_STOP_WORDS[];
extern const char *FRT_FULL_ITALIAN_STOP_WORDS[];
extern const char *FRT_FULL_GERMAN_STOP_WORDS[];
extern const char *FRT_FULL_DUTCH_STOP_WORDS[];
extern const char *FRT_FULL_SWEDISH_STOP_WORDS[];
extern const char *FRT_FULL_NORWEGIAN_STOP_WORDS[];
extern const char *FRT_FULL_DANISH_STOP_WORDS[];
extern const char *FRT_FULL_RUSSIAN_STOP_WORDS[];
extern const char *FRT_FULL_FINNISH_STOP_WORDS[];
extern const char *FRT_FULL_HUNGARIAN_STOP_WORDS[];

typedef struct FrtStopFilter {
    FrtTokenFilter  super;
    FrtHash        *words;
} FrtStopFilter;

extern FrtTokenStream *frt_stop_filter_alloc();
extern FrtTokenStream *frt_stop_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts);
extern void            frt_stop_filter_set_words(FrtTokenStream *ts, const char **words);
extern void            frt_stop_filter_set_words_len(FrtTokenStream *ts, const char **words, int len);
extern FrtTokenStream *frt_stop_filter_new(FrtTokenStream *sub_ts);
extern FrtTokenStream *frt_stop_filter_new_with_words(FrtTokenStream *sub_ts, const char **words);
extern FrtTokenStream *frt_stop_filter_new_with_words_len(FrtTokenStream *sub_ts, const char **words, int len);

/*****************************************************************************/
/*** FrtStemFilter ***********************************************************/
/*****************************************************************************/

typedef struct FrtStemFilter {
    FrtTokenFilter      super;
    struct sb_stemmer  *stemmer;
    char               *algorithm;
    char               *charenc;
} FrtStemFilter;

extern FrtTokenStream *frt_stem_filter_alloc();
extern void            frt_stem_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts, const char *algorithm);
extern FrtTokenStream *frt_stem_filter_new(FrtTokenStream *sub_ts, const char *algorithm);

/*****************************************************************************/
/*** FrtMappingFilter ********************************************************/
/*****************************************************************************/

typedef struct FrtMappingFilter {
    FrtTokenFilter  super;
    FrtMultiMapper *mapper;
} FrtMappingFilter;

extern FrtTokenStream *frt_mapping_filter_alloc();
extern void            frt_mapping_filter_init(FrtTokenStream *ts, FrtTokenStream *sub_ts);
extern FrtTokenStream *frt_mapping_filter_new(FrtTokenStream *sub_ts);
extern FrtTokenStream *frt_mapping_filter_add(FrtTokenStream *ts, const char *pattern, const char *replacement);

/*****************************************************************************/
/*** FrtAnalyzer *************************************************************/
/*****************************************************************************/

typedef struct FrtAnalyzer {
    FrtTokenStream *current_ts;
    FrtTokenStream *(*get_ts)(struct FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding);
    void           (*destroy_i)(struct FrtAnalyzer *a);
    int             ref_cnt;
} FrtAnalyzer;

extern void frt_a_deref(FrtAnalyzer *a);

#define frt_a_get_ts(ma, field, text, encoding) ma->get_ts(ma, field, text, encoding)

extern FrtAnalyzer *frt_analyzer_alloc();
extern void         frt_analyzer_init(FrtAnalyzer *a, FrtTokenStream *ts, void (*destroy)(FrtAnalyzer *a),
                                    FrtTokenStream *(*get_ts)(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding));
extern FrtAnalyzer *frt_analyzer_new(FrtTokenStream *ts, void (*destroy)(FrtAnalyzer *a),
                                    FrtTokenStream *(*get_ts)(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding));

/*****************************************************************************/
/*** FrtNonAnalyzer **********************************************************/
/*****************************************************************************/

extern FrtAnalyzer *frt_non_analyzer_new();

extern void frt_a_standard_destroy(FrtAnalyzer *a);

/*****************************************************************************/
/*** FrtWhiteSpaceAnalyzer ***************************************************/
/*****************************************************************************/

extern FrtAnalyzer *frt_whitespace_analyzer_alloc();
extern void         frt_whitespace_analyzer_init(FrtAnalyzer *a, bool lowercase);
extern FrtAnalyzer *frt_whitespace_analyzer_new(bool lowercase);

/*****************************************************************************/
/*** FrtLetterAnalyzer *******************************************************/
/*****************************************************************************/

extern FrtAnalyzer *frt_letter_analyzer_alloc();
extern void         frt_letter_analyzer_init(FrtAnalyzer *a, bool lowercase);
extern FrtAnalyzer *frt_letter_analyzer_new(bool lowercase);

/*****************************************************************************/
/*** FrtStandardAnalyzer *****************************************************/
/*****************************************************************************/

extern FrtAnalyzer *frt_standard_analyzer_alloc();
extern void         frt_standard_analyzer_init(FrtAnalyzer *a, bool lowercase, const char **words);
extern FrtAnalyzer *frt_standard_analyzer_new(bool lowercase);
extern FrtAnalyzer *frt_standard_analyzer_new_with_words(bool lowercase, const char **words);

/*****************************************************************************/
/*** FrtPerFieldAnalyzer *****************************************************/
/*****************************************************************************/

#define PFA(analyzer) ((FrtPerFieldAnalyzer *)(analyzer))

typedef struct FrtPerFieldAnalyzer {
    FrtAnalyzer  super;
    FrtHash     *dict;
    FrtAnalyzer *default_a;
} FrtPerFieldAnalyzer;

extern FrtAnalyzer *frt_per_field_analyzer_alloc();
extern void         frt_per_field_analyzer_init(FrtAnalyzer *a, FrtAnalyzer *default_a);
extern FrtAnalyzer *frt_per_field_analyzer_new(FrtAnalyzer *default_a);
extern void         frt_pfa_add_field(FrtAnalyzer *self, FrtSymbol field, FrtAnalyzer *analyzer);

#endif
