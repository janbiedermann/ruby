/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "frt_q_parser.y"

/*****************************************************************************
 * QueryParser
 * ===========
 *
 * Brief Overview
 * --------------
 *
 * === Creating a QueryParser
 *
 *  +qp_new+ allocates a new QueryParser and assigns three very important
 *  HashSets; +qp->def_fields+, +qp->tkz_fields+ and +qp->all_fields+. The
 *  query language allows you to assign a field or a set of fields to each
 *  part of the query.
 *
 *    - +qp->def_fields+ is the set of fields that a query is applied to by
 *      default when no fields are specified.
 *    - +qp->all_fields+ is the set of fields that gets searched when the user
 *      requests a search of all fields.
 *    - +qp->tkz_fields+ is the set of fields that gets tokenized before being
 *      added to the query parser.
 *
 * === qp_parse
 *
 *  The main QueryParser method is +qp_parse+. It gets called with a the query
 *  string and returns a Query object which can then be passed to the
 *  IndexSearcher. The first thing it does is to clean the query string if
 *  +qp->clean_str+ is set to true. The cleaning is done with the
 *  +qp_clean_str+.
 *
 *  It then calls the yacc parser which will set +qp->result+ to the parsed
 *  query. If parsing fails in any way, +qp->result+ should be set to NULL, in
 *  which case qp_parse does one of two things depending on the value of
 *  +qp->handle_parse_errors+;
 *
 *    - If it is set to true, qp_parse attempts to do a very basic parsing of
 *      the query by ignoring all special characters and parsing the query as
 *      a plain boolean query.
 *    - If it is set to false, qp_parse will raise a PARSE_ERROR and hopefully
 *      free all allocated memory.
 *
 * === The Lexer
 *
 *  +yylex+ is the lexing method called by the QueryParser. It breaks the
 *  query up into special characters;
 *
 *      ( "&:()[]{}!\"~^|<>=*?+-" )
 *
 *  and tokens;
 *
 *    - QWRD
 *    - WILD_STR
 *    - AND['AND', '&&']
 *    - OR['OR', '||']
 *    - REQ['REQ', '+']
 *    - NOT['NOT', '-', '~']
 *
 *  QWRD tokens are query word tokens which are made up of characters other
 *  than the special characters. They can also contain special characters when
 *  escaped with a backslash '\'. WILD_STR is the same as QWRD except that it
 *  may also contain '?' and '*' characters.
 *
 * === The Parser
 *
 *  For a better understanding of the how the query parser works, it is a good
 *  idea to study the Ferret Query Language (FQL) described below. Once you
 *  understand FQL the one tricky part that needs to be mentioned is how
 *  fields are handled. This is where +qp->def_fields+ and +qp->all_fields
 *  come into play. When no fields are specified then the default fields are
 *  used. The '*:' field specifier will search all fields contained in the
 *  all_fields set.  Otherwise all fields specified in the field descripter
 *  separated by '|' will be searched. For example 'title|content:' will
 *  search the title and content fields. When fields are specified like this,
 *  the parser will push the fields onto a stack and all queries modified by
 *  the field specifier will be applied to the fields on top of the stack.
 *  The parser uses the FLDS macro to handle the current fields. It takes the
 *  current query building function in the parser and calls it for all the
 *  current search fields (on top of the stack).
 *
 * Ferret Query Language (FQL)
 * ---------------------------
 *
 * FIXME to be continued...
 *****************************************************************************/

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "frt_global.h"
#include "frt_except.h"
#include "frt_search.h"
#include "frt_array.h"
#include <ruby/encoding.h>

extern rb_encoding *utf8_encoding;
extern int utf8_mbmaxlen;

typedef struct Phrase {
    int             size;
    int             capa;
    int             pos_inc;
    FrtPhrasePosition *positions;
} Phrase;

#define BCA_INIT_CAPA 4
typedef struct FrtBCArray {
    int size;
    int capa;
    FrtBooleanClause **clauses;
} FrtBCArray;

float frt_qp_default_fuzzy_min_sim = 0.5;
int frt_qp_default_fuzzy_pre_len = 0;


#line 187 "frt_q_parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif


/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    QWRD = 258,                    /* QWRD  */
    WILD_STR = 259,                /* WILD_STR  */
    LOW = 260,                     /* LOW  */
    AND = 261,                     /* AND  */
    OR = 262,                      /* OR  */
    REQ = 263,                     /* REQ  */
    NOT = 264,                     /* NOT  */
    HIGH = 265                     /* HIGH  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 116 "frt_q_parser.y"

    FrtQuery *query;
    FrtBooleanClause *bcls;
    FrtBCArray *bclss;
    FrtHashSet *hashset;
    Phrase *phrase;
    char *str;

#line 253 "frt_q_parser.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif




int yyparse (FrtQParser *qp, rb_encoding *encoding);



/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_QWRD = 3,                       /* QWRD  */
  YYSYMBOL_WILD_STR = 4,                   /* WILD_STR  */
  YYSYMBOL_LOW = 5,                        /* LOW  */
  YYSYMBOL_AND = 6,                        /* AND  */
  YYSYMBOL_OR = 7,                         /* OR  */
  YYSYMBOL_REQ = 8,                        /* REQ  */
  YYSYMBOL_NOT = 9,                        /* NOT  */
  YYSYMBOL_10_ = 10,                       /* ':'  */
  YYSYMBOL_HIGH = 11,                      /* HIGH  */
  YYSYMBOL_12_ = 12,                       /* '^'  */
  YYSYMBOL_13_ = 13,                       /* '('  */
  YYSYMBOL_14_ = 14,                       /* ')'  */
  YYSYMBOL_15_ = 15,                       /* '~'  */
  YYSYMBOL_16_ = 16,                       /* '*'  */
  YYSYMBOL_17_ = 17,                       /* '|'  */
  YYSYMBOL_18_ = 18,                       /* '"'  */
  YYSYMBOL_19_ = 19,                       /* '<'  */
  YYSYMBOL_20_ = 20,                       /* '>'  */
  YYSYMBOL_21_ = 21,                       /* '['  */
  YYSYMBOL_22_ = 22,                       /* ']'  */
  YYSYMBOL_23_ = 23,                       /* '}'  */
  YYSYMBOL_24_ = 24,                       /* '{'  */
  YYSYMBOL_25_ = 25,                       /* '='  */
  YYSYMBOL_YYACCEPT = 26,                  /* $accept  */
  YYSYMBOL_bool_q = 27,                    /* bool_q  */
  YYSYMBOL_bool_clss = 28,                 /* bool_clss  */
  YYSYMBOL_bool_cls = 29,                  /* bool_cls  */
  YYSYMBOL_boosted_q = 30,                 /* boosted_q  */
  YYSYMBOL_q = 31,                         /* q  */
  YYSYMBOL_term_q = 32,                    /* term_q  */
  YYSYMBOL_wild_q = 33,                    /* wild_q  */
  YYSYMBOL_field_q = 34,                   /* field_q  */
  YYSYMBOL_35_1 = 35,                      /* $@1  */
  YYSYMBOL_36_2 = 36,                      /* $@2  */
  YYSYMBOL_37_3 = 37,                      /* $@3  */
  YYSYMBOL_field = 38,                     /* field  */
  YYSYMBOL_phrase_q = 39,                  /* phrase_q  */
  YYSYMBOL_ph_words = 40,                  /* ph_words  */
  YYSYMBOL_range_q = 41                    /* range_q  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Second part of user prologue.  */
#line 124 "frt_q_parser.y"

static int yylex(YYSTYPE *lvalp, FrtQParser *qp);
static int yyerror(FrtQParser *qp, rb_encoding *encoding, char const *msg);

#define PHRASE_INIT_CAPA 4
static FrtQuery *get_bool_q(FrtBCArray *bca);

static FrtBCArray *first_cls(FrtBooleanClause *boolean_clause);
static FrtBCArray *add_and_cls(FrtBCArray *bca, FrtBooleanClause *clause);
static FrtBCArray *add_or_cls(FrtBCArray *bca, FrtBooleanClause *clause);
static FrtBCArray *add_default_cls(FrtQParser *qp, FrtBCArray *bca, FrtBooleanClause *clause);
static void bca_destroy(FrtBCArray *bca);

static FrtBooleanClause *get_bool_cls(FrtQuery *q, FrtBCType occur);

static FrtQuery *get_term_q(FrtQParser *qp, FrtSymbol field, char *word, rb_encoding *encoding);
static FrtQuery *get_fuzzy_q(FrtQParser *qp, FrtSymbol field, char *word, char *slop, rb_encoding *encoding);
static FrtQuery *get_wild_q(FrtQParser *qp, FrtSymbol field, char *pattern, rb_encoding *encoding);

static FrtHashSet *first_field(FrtQParser *qp, const char *field_name);
static FrtHashSet *add_field(FrtQParser *qp, const char *field_name);

static FrtQuery *get_phrase_q(FrtQParser *qp, Phrase *phrase, char *slop, rb_encoding *encoding);

static Phrase *ph_first_word(char *word);
static Phrase *ph_add_word(Phrase *self, char *word);
static Phrase *ph_add_multi_word(Phrase *self, char *word);
static void ph_destroy(Phrase *self);

static FrtQuery *get_r_q(FrtQParser *qp, FrtSymbol field, char *from, char *to, bool inc_lower, bool inc_upper, rb_encoding *encoding);

static void qp_push_fields(FrtQParser *self, FrtHashSet *fields, bool destroy);
static void qp_pop_fields(FrtQParser *self);

/**
 * +FLDS+ calls +func+ for all fields on top of the field stack. +func+
 * must return a query. If there is more than one field on top of FieldStack
 * then +FLDS+ will combing all the queries returned by +func+ into a single
 * BooleanQuery which it than assigns to +q+. If there is only one field, the
 * return value of +func+ is assigned to +q+ directly.
 */
#define FLDS(q, func) do {\
    FRT_TRY {\
        FrtSymbol field;\
        if (qp->fields->size == 0) {\
            q = NULL;\
        } else if (qp->fields->size == 1) {\
            field = (FrtSymbol)qp->fields->first->elem;\
            q = func;\
        } else {\
            FrtQuery *volatile sq; FrtHashSetEntry *volatile hse;\
            q = frt_bq_new_max(false, qp->max_clauses);\
            for (hse = qp->fields->first; hse; hse = hse->next) {\
                field = (FrtSymbol)hse->elem;\
                sq = func;\
                FRT_TRY\
                  if (sq) frt_bq_add_query_nr(q, sq, FRT_BC_SHOULD);\
                FRT_XCATCHALL\
                  if (sq) frt_q_deref(sq);\
                FRT_XENDTRY\
            }\
            if (((FrtBooleanQuery *)q)->clause_cnt == 0) {\
                frt_q_deref(q);\
                q = NULL;\
            }\
        }\
    } FRT_XCATCHALL\
        qp->destruct = true;\
        FRT_HANDLED();\
    FRT_XENDTRY\
    if (qp->destruct && !qp->recovering && q) {frt_q_deref(q); q = NULL;}\
} while (0)

#define Y if (qp->destruct) goto yyerrorlab;
#define T FRT_TRY
#define E\
  FRT_XCATCHALL\
    qp->destruct = true;\
    FRT_HANDLED();\
  FRT_XENDTRY\
  if (qp->destruct) Y;

#line 402 "frt_q_parser.c"


#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  39
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   126

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  26
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  16
/* YYNRULES -- Number of rules.  */
#define YYNRULES  51
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  80

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   265


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    18,     2,     2,     2,     2,     2,
      13,    14,    16,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    10,     2,
      19,    25,    20,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    21,     2,    22,    12,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    24,    17,    23,    15,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    11
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   226,   226,   227,   229,   230,   231,   232,   234,   235,
     236,   238,   239,   241,   242,   243,   244,   245,   246,   247,
     249,   250,   251,   253,   255,   255,   257,   257,   257,   260,
     261,   263,   264,   265,   266,   268,   269,   270,   271,   272,
     274,   275,   276,   277,   278,   279,   280,   281,   282,   283,
     284,   285
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "QWRD", "WILD_STR",
  "LOW", "AND", "OR", "REQ", "NOT", "':'", "HIGH", "'^'", "'('", "')'",
  "'~'", "'*'", "'|'", "'\"'", "'<'", "'>'", "'['", "']'", "'}'", "'{'",
  "'='", "$accept", "bool_q", "bool_clss", "bool_cls", "boosted_q", "q",
  "term_q", "wild_q", "field_q", "$@1", "$@2", "$@3", "field", "phrase_q",
  "ph_words", "range_q", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-30)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-30)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      83,    -4,   -30,   102,   102,    64,   -30,     7,    -2,    -1,
       6,    15,    31,    45,   -30,   -30,    29,   -30,   -30,   -30,
      -5,   -30,   -30,    40,   -30,   -30,   -30,    26,    47,   -30,
      55,    42,    19,   -15,    68,   -30,    71,     0,     1,   -30,
      83,    83,   -30,    72,   102,    73,   -30,   -30,   102,    76,
     -30,   -30,    78,    74,    70,   -30,   -30,   -30,   -30,    -6,
     -30,    33,   -30,   -30,   -30,   -30,   -30,   -30,   -30,   -30,
     -30,    90,   -30,   -30,   -30,   -30,   -30,   -30,   -30,   -30
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       2,    20,    23,     0,     0,     0,    26,     0,     0,     0,
       0,     0,     0,     3,     4,    10,    11,    13,    19,    16,
       0,    17,    18,    22,     8,     9,    14,     0,     0,    35,
      33,     0,     0,    48,     0,    51,     0,     0,     0,     1,
       0,     0,     7,     0,     0,     0,    21,    15,     0,     0,
      36,    37,     0,    31,     0,    45,    44,    49,    50,     0,
      46,     0,    47,     5,     6,    12,    24,    30,    27,    34,
      39,     0,    38,    40,    41,    42,    43,    25,    28,    32
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -30,   -30,    89,   -13,    56,   -29,   -30,   -30,   -30,   -30,
     -30,   -30,   -30,   -30,   -30,   -30
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,    12,    13,    14,    15,    16,    17,    18,    19,    77,
      28,    78,    20,    21,    32,    22
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      42,    33,    35,    59,    61,    44,   -29,    55,    56,    37,
      29,    23,    45,   -29,    42,    66,    73,    74,    38,    68,
      60,    62,    51,    34,    36,    30,    31,    63,    64,     1,
       2,    39,    40,    41,     3,     4,    52,    53,    54,     5,
      47,    43,     6,    46,     7,     8,     9,    10,     1,     2,
      11,    40,    41,     3,     4,    75,    76,    48,     5,    24,
      25,     6,    50,     7,     8,     9,    10,     1,     2,    11,
      49,    57,     3,     4,    58,    65,    67,     5,    26,    69,
       6,    70,     7,     8,     9,    10,     1,     2,    11,    71,
      72,     3,     4,    79,    27,     0,     5,     0,     0,     6,
       0,     7,     8,     9,    10,     1,     2,    11,     0,     0,
       0,     0,     0,     0,     0,     5,     0,     0,     6,     0,
       7,     8,     9,    10,     0,     0,    11
};

static const yytype_int8 yycheck[] =
{
      13,     3,     3,     3,     3,    10,    10,    22,    23,     3,
       3,    15,    17,    17,    27,    44,    22,    23,     3,    48,
      20,    20,     3,    25,    25,    18,    19,    40,    41,     3,
       4,     0,     6,     7,     8,     9,    17,    18,    19,    13,
      14,    12,    16,     3,    18,    19,    20,    21,     3,     4,
      24,     6,     7,     8,     9,    22,    23,    10,    13,     3,
       4,    16,    20,    18,    19,    20,    21,     3,     4,    24,
      15,     3,     8,     9,     3,     3,     3,    13,    14,     3,
      16,     3,    18,    19,    20,    21,     3,     4,    24,    15,
      20,     8,     9,     3,     5,    -1,    13,    -1,    -1,    16,
      -1,    18,    19,    20,    21,     3,     4,    24,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    13,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    -1,    -1,    24
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     8,     9,    13,    16,    18,    19,    20,
      21,    24,    27,    28,    29,    30,    31,    32,    33,    34,
      38,    39,    41,    15,    30,    30,    14,    28,    36,     3,
      18,    19,    40,     3,    25,     3,    25,     3,     3,     0,
       6,     7,    29,    12,    10,    17,     3,    14,    10,    15,
      20,     3,    17,    18,    19,    22,    23,     3,     3,     3,
      20,     3,    20,    29,    29,     3,    31,     3,    31,     3,
       3,    15,    20,    22,    23,    22,    23,    35,    37,     3
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    26,    27,    27,    28,    28,    28,    28,    29,    29,
      29,    30,    30,    31,    31,    31,    31,    31,    31,    31,
      32,    32,    32,    33,    35,    34,    36,    37,    34,    38,
      38,    39,    39,    39,    39,    40,    40,    40,    40,    40,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     1,     1,     3,     3,     2,     2,     2,
       1,     1,     3,     1,     2,     3,     1,     1,     1,     1,
       1,     3,     2,     1,     0,     4,     0,     0,     5,     1,
       3,     3,     5,     2,     4,     1,     2,     2,     3,     3,
       4,     4,     4,     4,     3,     3,     3,     3,     2,     3,
       3,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (qp, encoding, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, qp, encoding); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, FrtQParser *qp, rb_encoding *encoding)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (qp);
  YY_USE (encoding);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, FrtQParser *qp, rb_encoding *encoding)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, qp, encoding);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, FrtQParser *qp, rb_encoding *encoding)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], qp, encoding);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, qp, encoding); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, FrtQParser *qp, rb_encoding *encoding)
{
  YY_USE (yyvaluep);
  YY_USE (qp);
  YY_USE (encoding);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_bool_q: /* bool_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1154 "frt_q_parser.c"
        break;

    case YYSYMBOL_bool_clss: /* bool_clss  */
#line 223 "frt_q_parser.y"
            { if (((*yyvaluep).bclss) && qp->destruct) bca_destroy(((*yyvaluep).bclss)); }
#line 1160 "frt_q_parser.c"
        break;

    case YYSYMBOL_bool_cls: /* bool_cls  */
#line 222 "frt_q_parser.y"
            { if (((*yyvaluep).bcls) && qp->destruct) frt_bc_deref(((*yyvaluep).bcls)); }
#line 1166 "frt_q_parser.c"
        break;

    case YYSYMBOL_boosted_q: /* boosted_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1172 "frt_q_parser.c"
        break;

    case YYSYMBOL_q: /* q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1178 "frt_q_parser.c"
        break;

    case YYSYMBOL_term_q: /* term_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1184 "frt_q_parser.c"
        break;

    case YYSYMBOL_wild_q: /* wild_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1190 "frt_q_parser.c"
        break;

    case YYSYMBOL_field_q: /* field_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1196 "frt_q_parser.c"
        break;

    case YYSYMBOL_phrase_q: /* phrase_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1202 "frt_q_parser.c"
        break;

    case YYSYMBOL_ph_words: /* ph_words  */
#line 224 "frt_q_parser.y"
            { if (((*yyvaluep).phrase) && qp->destruct) ph_destroy(((*yyvaluep).phrase)); }
#line 1208 "frt_q_parser.c"
        break;

    case YYSYMBOL_range_q: /* range_q  */
#line 221 "frt_q_parser.y"
            { if (((*yyvaluep).query) && qp->destruct) frt_q_deref(((*yyvaluep).query)); }
#line 1214 "frt_q_parser.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (FrtQParser *qp, rb_encoding *encoding)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, qp);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* bool_q: %empty  */
#line 226 "frt_q_parser.y"
                                      {   qp->result = (yyval.query) = NULL; }
#line 1490 "frt_q_parser.c"
    break;

  case 3: /* bool_q: bool_clss  */
#line 227 "frt_q_parser.y"
                                      { T qp->result = (yyval.query) = get_bool_q((yyvsp[0].bclss)); E }
#line 1496 "frt_q_parser.c"
    break;

  case 4: /* bool_clss: bool_cls  */
#line 229 "frt_q_parser.y"
                                      { T (yyval.bclss) = first_cls((yyvsp[0].bcls)); E }
#line 1502 "frt_q_parser.c"
    break;

  case 5: /* bool_clss: bool_clss AND bool_cls  */
#line 230 "frt_q_parser.y"
                                      { T (yyval.bclss) = add_and_cls((yyvsp[-2].bclss), (yyvsp[0].bcls)); E }
#line 1508 "frt_q_parser.c"
    break;

  case 6: /* bool_clss: bool_clss OR bool_cls  */
#line 231 "frt_q_parser.y"
                                      { T (yyval.bclss) = add_or_cls((yyvsp[-2].bclss), (yyvsp[0].bcls)); E }
#line 1514 "frt_q_parser.c"
    break;

  case 7: /* bool_clss: bool_clss bool_cls  */
#line 232 "frt_q_parser.y"
                                      { T (yyval.bclss) = add_default_cls(qp, (yyvsp[-1].bclss), (yyvsp[0].bcls)); E }
#line 1520 "frt_q_parser.c"
    break;

  case 8: /* bool_cls: REQ boosted_q  */
#line 234 "frt_q_parser.y"
                                      { T (yyval.bcls) = get_bool_cls((yyvsp[0].query), FRT_BC_MUST); E }
#line 1526 "frt_q_parser.c"
    break;

  case 9: /* bool_cls: NOT boosted_q  */
#line 235 "frt_q_parser.y"
                                      { T (yyval.bcls) = get_bool_cls((yyvsp[0].query), FRT_BC_MUST_NOT); E }
#line 1532 "frt_q_parser.c"
    break;

  case 10: /* bool_cls: boosted_q  */
#line 236 "frt_q_parser.y"
                                      { T (yyval.bcls) = get_bool_cls((yyvsp[0].query), FRT_BC_SHOULD); E }
#line 1538 "frt_q_parser.c"
    break;

  case 12: /* boosted_q: q '^' QWRD  */
#line 239 "frt_q_parser.y"
                                      { T if ((yyvsp[-2].query)) sscanf((yyvsp[0].str),"%f",&((yyvsp[-2].query)->boost));  (yyval.query)=(yyvsp[-2].query); E }
#line 1544 "frt_q_parser.c"
    break;

  case 14: /* q: '(' ')'  */
#line 242 "frt_q_parser.y"
                                      { T (yyval.query) = frt_bq_new_max(true, qp->max_clauses); E }
#line 1550 "frt_q_parser.c"
    break;

  case 15: /* q: '(' bool_clss ')'  */
#line 243 "frt_q_parser.y"
                                      { T (yyval.query) = get_bool_q((yyvsp[-1].bclss)); E }
#line 1556 "frt_q_parser.c"
    break;

  case 20: /* term_q: QWRD  */
#line 249 "frt_q_parser.y"
                                      { FLDS((yyval.query), get_term_q(qp, field, (yyvsp[0].str), encoding)); Y}
#line 1562 "frt_q_parser.c"
    break;

  case 21: /* term_q: QWRD '~' QWRD  */
#line 250 "frt_q_parser.y"
                                      { FLDS((yyval.query), get_fuzzy_q(qp, field, (yyvsp[-2].str), (yyvsp[0].str), encoding)); Y}
#line 1568 "frt_q_parser.c"
    break;

  case 22: /* term_q: QWRD '~'  */
#line 251 "frt_q_parser.y"
                                      { FLDS((yyval.query), get_fuzzy_q(qp, field, (yyvsp[-1].str), NULL, encoding)); Y}
#line 1574 "frt_q_parser.c"
    break;

  case 23: /* wild_q: WILD_STR  */
#line 253 "frt_q_parser.y"
                                      { FLDS((yyval.query), get_wild_q(qp, field, (yyvsp[0].str), encoding)); Y}
#line 1580 "frt_q_parser.c"
    break;

  case 24: /* $@1: %empty  */
#line 255 "frt_q_parser.y"
                        { qp_pop_fields(qp); }
#line 1586 "frt_q_parser.c"
    break;

  case 25: /* field_q: field ':' q $@1  */
#line 256 "frt_q_parser.y"
                                      { (yyval.query) = (yyvsp[-1].query); }
#line 1592 "frt_q_parser.c"
    break;

  case 26: /* $@2: %empty  */
#line 257 "frt_q_parser.y"
                { qp_push_fields(qp, qp->all_fields, false); }
#line 1598 "frt_q_parser.c"
    break;

  case 27: /* $@3: %empty  */
#line 257 "frt_q_parser.y"
                                                                     { qp_pop_fields(qp); }
#line 1604 "frt_q_parser.c"
    break;

  case 28: /* field_q: '*' $@2 ':' q $@3  */
#line 258 "frt_q_parser.y"
                                      { (yyval.query) = (yyvsp[-1].query); }
#line 1610 "frt_q_parser.c"
    break;

  case 29: /* field: QWRD  */
#line 260 "frt_q_parser.y"
                                      { (yyval.hashset) = first_field(qp, (yyvsp[0].str)); }
#line 1616 "frt_q_parser.c"
    break;

  case 30: /* field: field '|' QWRD  */
#line 261 "frt_q_parser.y"
                                      { (yyval.hashset) = add_field(qp, (yyvsp[0].str));}
#line 1622 "frt_q_parser.c"
    break;

  case 31: /* phrase_q: '"' ph_words '"'  */
#line 263 "frt_q_parser.y"
                                      { (yyval.query) = get_phrase_q(qp, (yyvsp[-1].phrase), NULL, encoding); }
#line 1628 "frt_q_parser.c"
    break;

  case 32: /* phrase_q: '"' ph_words '"' '~' QWRD  */
#line 264 "frt_q_parser.y"
                                      { (yyval.query) = get_phrase_q(qp, (yyvsp[-3].phrase), (yyvsp[0].str), encoding); }
#line 1634 "frt_q_parser.c"
    break;

  case 33: /* phrase_q: '"' '"'  */
#line 265 "frt_q_parser.y"
                                      { (yyval.query) = NULL; }
#line 1640 "frt_q_parser.c"
    break;

  case 34: /* phrase_q: '"' '"' '~' QWRD  */
#line 266 "frt_q_parser.y"
                                      { (yyval.query) = NULL; (void)(yyvsp[0].str);}
#line 1646 "frt_q_parser.c"
    break;

  case 35: /* ph_words: QWRD  */
#line 268 "frt_q_parser.y"
                              { (yyval.phrase) = ph_first_word((yyvsp[0].str)); }
#line 1652 "frt_q_parser.c"
    break;

  case 36: /* ph_words: '<' '>'  */
#line 269 "frt_q_parser.y"
                              { (yyval.phrase) = ph_first_word(NULL); }
#line 1658 "frt_q_parser.c"
    break;

  case 37: /* ph_words: ph_words QWRD  */
#line 270 "frt_q_parser.y"
                              { (yyval.phrase) = ph_add_word((yyvsp[-1].phrase), (yyvsp[0].str)); }
#line 1664 "frt_q_parser.c"
    break;

  case 38: /* ph_words: ph_words '<' '>'  */
#line 271 "frt_q_parser.y"
                              { (yyval.phrase) = ph_add_word((yyvsp[-2].phrase), NULL); }
#line 1670 "frt_q_parser.c"
    break;

  case 39: /* ph_words: ph_words '|' QWRD  */
#line 272 "frt_q_parser.y"
                              { (yyval.phrase) = ph_add_multi_word((yyvsp[-2].phrase), (yyvsp[0].str));  }
#line 1676 "frt_q_parser.c"
    break;

  case 40: /* range_q: '[' QWRD QWRD ']'  */
#line 274 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[-2].str),  (yyvsp[-1].str),  true,  true,  encoding)); Y}
#line 1682 "frt_q_parser.c"
    break;

  case 41: /* range_q: '[' QWRD QWRD '}'  */
#line 275 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[-2].str),  (yyvsp[-1].str),  true,  false, encoding)); Y}
#line 1688 "frt_q_parser.c"
    break;

  case 42: /* range_q: '{' QWRD QWRD ']'  */
#line 276 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[-2].str),  (yyvsp[-1].str),  false, true,  encoding)); Y}
#line 1694 "frt_q_parser.c"
    break;

  case 43: /* range_q: '{' QWRD QWRD '}'  */
#line 277 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[-2].str),  (yyvsp[-1].str),  false, false, encoding)); Y}
#line 1700 "frt_q_parser.c"
    break;

  case 44: /* range_q: '<' QWRD '}'  */
#line 278 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, NULL,(yyvsp[-1].str),  false, false, encoding)); Y}
#line 1706 "frt_q_parser.c"
    break;

  case 45: /* range_q: '<' QWRD ']'  */
#line 279 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, NULL,(yyvsp[-1].str),  false, true,  encoding)); Y}
#line 1712 "frt_q_parser.c"
    break;

  case 46: /* range_q: '[' QWRD '>'  */
#line 280 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[-1].str),  NULL,true,  false, encoding)); Y}
#line 1718 "frt_q_parser.c"
    break;

  case 47: /* range_q: '{' QWRD '>'  */
#line 281 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[-1].str),  NULL,false, false, encoding)); Y}
#line 1724 "frt_q_parser.c"
    break;

  case 48: /* range_q: '<' QWRD  */
#line 282 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, NULL,(yyvsp[0].str),  false, false, encoding)); Y}
#line 1730 "frt_q_parser.c"
    break;

  case 49: /* range_q: '<' '=' QWRD  */
#line 283 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, NULL,(yyvsp[0].str),  false, true,  encoding)); Y}
#line 1736 "frt_q_parser.c"
    break;

  case 50: /* range_q: '>' '=' QWRD  */
#line 284 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[0].str),  NULL,true,  false, encoding)); Y}
#line 1742 "frt_q_parser.c"
    break;

  case 51: /* range_q: '>' QWRD  */
#line 285 "frt_q_parser.y"
                              { FLDS((yyval.query), get_r_q(qp, field, (yyvsp[0].str),  NULL,false, false, encoding)); Y}
#line 1748 "frt_q_parser.c"
    break;


#line 1752 "frt_q_parser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (qp, encoding, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, qp, encoding);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, qp, encoding);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (qp, encoding, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, qp, encoding);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, qp, encoding);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 287 "frt_q_parser.y"


static const char *special_char = "&:()[]{}!\"~^|<>=*?+-";
static const char *not_word =   " \t()[]{}!\"~^|<>=";

/**
 * +get_word+ gets the next query-word from the query string. A query-word is
 * basically a string of non-special or escaped special characters. It is
 * FrtAnalyzer agnostic. It is up to the get_*_q methods to tokenize the word and
 * turn it into a +Query+. See the documentation for each get_*_q method to
 * see how it handles tokenization.
 *
 * Note that +get_word+ is also responsible for returning field names and
 * matching the special tokens 'AND', 'NOT', 'REQ' and 'OR'.
 */
static int get_word(YYSTYPE *lvalp, FrtQParser *qp)
{
    bool is_wild = false;
    int len;
    char c;
    char *buf = qp->buf[qp->buf_index];
    char *bufp = buf;
    qp->buf_index = (qp->buf_index + 1) % FRT_QP_CONC_WORDS;

    if (qp->dynbuf) {
        free(qp->dynbuf);
        qp->dynbuf = NULL;
    }

    qp->qstrp--; /* need to back up one character */

    while (!strchr(not_word, (c = *qp->qstrp++))) {
        switch (c) {
            case '\\':
                if ((c = *qp->qstrp) == '\0') {
                    *bufp++ = '\\';
                }
                else {
                    *bufp++ = c;
                    qp->qstrp++;
                }
                break;
            case ':':
                if ((*qp->qstrp) == ':') {
                    qp->qstrp++;
                    *bufp++ = ':';
                    *bufp++ = ':';
                }
                else {
                   goto get_word_done;
                }
                break;
            case '*': case '?':
                is_wild = true;
                /* fall through */
            default:
                *bufp++ = c;
        }
        /* we've exceeded the static buffer. switch to the dynamic one. The
         * dynamic buffer is allocated enough space to hold the whole query
         * string so it's capacity doesn't need to be checked again once
         * allocated. */
        if (!qp->dynbuf && ((bufp - buf) == FRT_MAX_WORD_SIZE)) {
            qp->dynbuf = FRT_ALLOC_AND_ZERO_N(char, strlen(qp->qstr) + 1);
            strncpy(qp->dynbuf, buf, FRT_MAX_WORD_SIZE);
            buf = qp->dynbuf;
            bufp = buf + FRT_MAX_WORD_SIZE;
        }
    }
get_word_done:
    qp->qstrp--;
    /* check for keywords. There are only four so we have a bit of a hack
     * which just checks for all of them. */
    *bufp = '\0';
    len = (int)(bufp - buf);
    if (qp->use_keywords) {
        if (len == 3) {
            if (buf[0] == 'A' && buf[1] == 'N' && buf[2] == 'D') return AND;
            if (buf[0] == 'N' && buf[1] == 'O' && buf[2] == 'T') return NOT;
            if (buf[0] == 'R' && buf[1] == 'E' && buf[2] == 'Q') return REQ;
        }
        if (len == 2 && buf[0] == 'O' && buf[1] == 'R') return OR;
    }

    /* found a word so return it. */
    lvalp->str = buf;
    if (is_wild) {
        return WILD_STR;
    }
    return QWRD;
}

/**
 * +yylex+ is the lexing method called by the QueryParser. It breaks the
 * query up into special characters;
 *
 *     ( "&:()[]{}!\"~^|<>=*?+-" )
 *
 * and tokens;
 *
 *   - QWRD
 *   - WILD_STR
 *   - AND['AND', '&&']
 *   - OR['OR', '||']
 *   - REQ['REQ', '+']
 *   - NOT['NOT', '-', '~']
 *
 * QWRD tokens are query word tokens which are made up of characters other
 * than the special characters. They can also contain special characters when
 * escaped with a backslash '\'. WILD_STR is the same as QWRD except that it
 * may also contain '?' and '*' characters.
 *
 * If any of the special chars are seen they will usually be returned straight
 * away. The exceptions are the wild chars '*' and '?', and '&' which will be
 * treated as a plain old word character unless followed by another '&'.
 *
 * If no special characters or tokens are found then yylex delegates to
 * +get_word+ which will fetch the next query-word.
 */
static int yylex(YYSTYPE *lvalp, FrtQParser *qp)
{
    char c, nc;

    while ((c=*qp->qstrp++) == ' ' || c == '\t') {
    }

    if (c == '\0') return 0;

    if (strchr(special_char, c)) {   /* comment */
        nc = *qp->qstrp;
        switch (c) {
            case '-': case '!': return NOT;
            case '+': return REQ;
            case '*':
                if (nc == ':') return c;
                break;
            case '?':
                break;
            case '&':
                if (nc == '&') {
                    qp->qstrp++;
                    return AND;
                }
                break; /* Don't return single & character. Use in word. */
            case '|':
                if (nc == '|') {
                    qp->qstrp++;
                    return OR;
                }
            default:
                return c;
        }
    }

    return get_word(lvalp, qp);
}

/**
 * yyerror gets called if there is an parse error with the yacc parser.
 * It is responsible for clearing any memory that was allocated during the
 * parsing process.
 */
static int yyerror(FrtQParser *qp, rb_encoding *encoding, char const *msg)
{
    (void)encoding;
    qp->destruct = true;
    if (!qp->handle_parse_errors) {
        char buf[1024];
        buf[1023] = '\0';
        strncpy(buf, qp->qstr, 1023);
        if (qp->clean_str) {
            free(qp->qstr);
        }
        frt_mutex_unlock(&qp->mutex);
        snprintf(frt_xmsg_buffer, FRT_XMSG_BUFFER_SIZE,
                 "couldn't parse query ``%s''. Error message "
                 " was %s", buf, (char *)msg);
    }
    while (qp->fields_top->next != NULL) {
        qp_pop_fields(qp);
    }
    return 0;
}

#define BQ(query) ((FrtBooleanQuery *)(query))

/**
 * The QueryParser caches a tokenizer for each field so that it doesn't need
 * to allocate a new tokenizer for each term in the query. This would be quite
 * expensive as tokenizers use quite a large hunk of memory.
 *
 * This method returns the query parser for a particular field and sets it up
 * with the text to be tokenized.
 */
static FrtTokenStream *get_cached_ts(FrtQParser *qp, FrtSymbol field, char *text, rb_encoding *encoding)
{
    FrtTokenStream *ts;
    if (frt_hs_exists(qp->tokenized_fields, (void *)field)) {
        ts = (FrtTokenStream *)frt_h_get(qp->ts_cache, (void *)field);
        if (!ts) {
            ts = frt_a_get_ts(qp->analyzer, field, text, encoding);
            frt_h_set(qp->ts_cache, (void *)field, ts);
        }
        else {
            ts->reset(ts, text, encoding);
        }
    }
    else {
        ts = qp->non_tokenizer;
        ts->reset(ts, text, encoding);
    }
    return ts;
}

/**
 * Turns a BooleanClause array into a BooleanQuery. It will optimize the query
 * if 0 or 1 clauses are present to NULL or the actual query in the clause
 * respectively.
 */
static FrtQuery *get_bool_q(FrtBCArray *bca)
{
    FrtQuery *q;
    const int clause_count = bca->size;

    if (clause_count == 0) {
        q = NULL;
        free(bca->clauses);
    }
    else if (clause_count == 1) {
        FrtBooleanClause *bc = bca->clauses[0];
        if (bc->is_prohibited) {
            q = frt_bq_new(false);
            frt_bq_add_query_nr(q, bc->query, FRT_BC_MUST_NOT);
            frt_bq_add_query_nr(q, frt_maq_new(), FRT_BC_MUST);
        }
        else {
            q = bc->query;
        }
        free(bc);
        free(bca->clauses);
    }
    else {
        q = frt_bq_new(false);
        /* copy clauses into query */

        BQ(q)->clause_cnt = clause_count;
        BQ(q)->clause_capa = bca->capa;
        free(BQ(q)->clauses);
        BQ(q)->clauses = bca->clauses;
    }
    free(bca);
    return q;
}

/**
 * Base method for appending BooleanClauses to a FrtBooleanClause array. This
 * method doesn't care about the type of clause (MUST, SHOULD, MUST_NOT).
 */
static void bca_add_clause(FrtBCArray *bca, FrtBooleanClause *clause)
{
    if (bca->size >= bca->capa) {
        bca->capa <<= 1;
        FRT_REALLOC_N(bca->clauses, FrtBooleanClause *, bca->capa);
    }
    bca->clauses[bca->size] = clause;
    bca->size++;
}

/**
 * Add the first clause to a BooleanClause array. This method is also
 * responsible for allocating a new BooleanClause array.
 */
static FrtBCArray *first_cls(FrtBooleanClause *clause)
{
    FrtBCArray *bca = FRT_ALLOC_AND_ZERO(FrtBCArray);
    bca->capa = BCA_INIT_CAPA;
    bca->clauses = FRT_ALLOC_N(FrtBooleanClause *, BCA_INIT_CAPA);
    if (clause) {
        bca_add_clause(bca, clause);
    }
    return bca;
}

/**
 * Add AND clause to the BooleanClause array. The means that it will set the
 * clause being added and the previously added clause from SHOULD clauses to
 * MUST clauses. (If they are currently MUST_NOT clauses they stay as they
 * are.)
 */
static FrtBCArray *add_and_cls(FrtBCArray *bca, FrtBooleanClause *clause)
{
    if (clause) {
        if (bca->size == 1) {
            if (!bca->clauses[0]->is_prohibited) {
                frt_bc_set_occur(bca->clauses[0], FRT_BC_MUST);
            }
        }
        if (!clause->is_prohibited) {
            frt_bc_set_occur(clause, FRT_BC_MUST);
        }
        bca_add_clause(bca, clause);
    }
    return bca;
}

/**
 * Add SHOULD clause to the BooleanClause array.
 */
static FrtBCArray *add_or_cls(FrtBCArray *bca, FrtBooleanClause *clause)
{
    if (clause) {
        bca_add_clause(bca, clause);
    }
    return bca;
}

/**
 * Add AND or OR clause to the BooleanClause array, depending on the default
 * clause type.
 */
static FrtBCArray *add_default_cls(FrtQParser *qp, FrtBCArray *bca,
                                FrtBooleanClause *clause)
{
    if (qp->or_default) {
        add_or_cls(bca, clause);
    }
    else {
        add_and_cls(bca, clause);
    }
    return bca;
}

/**
 * destroy array of BooleanClauses
 */
static void bca_destroy(FrtBCArray *bca)
{
    int i;
    for (i = 0; i < bca->size; i++) {
        frt_bc_deref(bca->clauses[i]);
    }
    free(bca->clauses);
    free(bca);
}

/**
 * Turn a query into a BooleanClause for addition to a BooleanQuery.
 */
static FrtBooleanClause *get_bool_cls(FrtQuery *q, FrtBCType occur)
{
    if (q) {
        return frt_bc_new(q, occur);
    }
    else {
        return NULL;
    }
}

/**
 * Create a TermQuery. The word will be tokenized and if the tokenization
 * produces more than one token, a PhraseQuery will be returned. For example,
 * if the word is dbalmain@gmail.com and a LetterTokenizer is used then a
 * PhraseQuery "dbalmain gmail com" will be returned which is actually exactly
 * what we want as it will match any documents containing the same email
 * address and tokenized with the same tokenizer.
 */
static FrtQuery *get_term_q(FrtQParser *qp, FrtSymbol field, char *word, rb_encoding *encoding)
{
    FrtQuery *q;
    FrtToken *token;
    FrtTokenStream *stream = get_cached_ts(qp, field, word, encoding);

    if ((token = frt_ts_next(stream)) == NULL) {
        q = NULL;
    }
    else {
        q = frt_tq_new(field, token->text);
        if ((token = frt_ts_next(stream)) != NULL) {
            /* Less likely case, destroy the term query and create a
             * phrase query instead */
            FrtQuery *phq = frt_phq_new(field);
            frt_phq_add_term(phq, ((FrtTermQuery *)q)->term, 0);
            q->destroy_i(q);
            q = phq;
            do {
                if (token->pos_inc) {
                    frt_phq_add_term(q, token->text, token->pos_inc);
                    /* add some slop since single term was expected */
                    ((FrtPhraseQuery *)q)->slop++;
                }
                else {
                    frt_phq_append_multi_term(q, token->text);
                }
            } while ((token = frt_ts_next(stream)) != NULL);
        }
    }
    return q;
}

/**
 * Create a FuzzyQuery. The word will be tokenized and only the first token
 * will be used. If there are any more tokens after tokenization, they will be
 * ignored.
 */
static FrtQuery *get_fuzzy_q(FrtQParser *qp, FrtSymbol field, char *word, char *slop_str, rb_encoding *encoding)
{
    FrtQuery *q;
    FrtToken *token;
    FrtTokenStream *stream = get_cached_ts(qp, field, word, encoding);

    if ((token = frt_ts_next(stream)) == NULL) {
        q = NULL;
    }
    else {
        /* it only makes sense to find one term in a fuzzy query */
        float slop = frt_qp_default_fuzzy_min_sim;
        if (slop_str) {
            sscanf(slop_str, "%f", &slop);
        }
        q = frt_fuzq_new_conf(field, token->text, slop, frt_qp_default_fuzzy_pre_len,
                          qp->max_clauses);
    }
    return q;
}

/**
 * Downcase a string taking encoding into account and works for multibyte character sets.
 */
static char *lower_str(char *str, int len, rb_encoding *enc) {
    OnigCaseFoldType fold_type = ONIGENC_CASE_DOWNCASE;
    const int max_len = len + 20; // CASE_MAPPING_ADDITIONAL_LENGTH
    char *buf = FRT_ALLOC_N(char, max_len);
    char *buf_end = buf + max_len + 19;
    const OnigUChar *t = (const OnigUChar *)str;

    len = enc->case_map(&fold_type, &t, (const OnigUChar *)(str + len), (OnigUChar *)buf, (OnigUChar *)buf_end, enc);
    memcpy(str, buf, len);
    str[len] = '\0';
    free(buf);

    return str;
}

/**
 * Create a WildCardQuery. No tokenization will be performed on the pattern
 * but the pattern will be downcased if +qp->wild_lower+ is set to true and
 * the field in question is a tokenized field.
 *
 * Note: this method will not always return a WildCardQuery. It could be
 * optimized to a MatchAllQuery if the pattern is '*' or a PrefixQuery if the
 * only wild char (*, ?) in the pattern is a '*' at the end of the pattern.
 */
static FrtQuery *get_wild_q(FrtQParser *qp, FrtSymbol field, char *pattern, rb_encoding *encoding) {
    FrtQuery *q;
    bool is_prefix = false;
    char *p;
    int len = (int)strlen(pattern);

    if (qp->wild_lower
        && (!qp->tokenized_fields || frt_hs_exists(qp->tokenized_fields, (void *)field))) {
        lower_str(pattern, len, encoding);
    }

    /* simplify the wildcard query to a prefix query if possible. Basically a
     * prefix query is any wildcard query that has a '*' as the last character
     * and no other wildcard characters before it. "*" by itself will expand
     * to a MatchAllQuery */
    if (strcmp(pattern, "*") == 0) {
        return frt_maq_new();
    }
    if (pattern[len - 1] == '*') {
        is_prefix = true;
        for (p = &pattern[len - 2]; p >= pattern; p--) {
            if (*p == '*' || *p == '?') {
                is_prefix = false;
                break;
            }
        }
    }
    if (is_prefix) {
        /* chop off the '*' temporarily to create the query */
        pattern[len - 1] = 0;
        q = frt_prefixq_new(field, pattern);
        pattern[len - 1] = '*';
    }
    else {
        q = frt_wcq_new(field, pattern);
    }
    FrtMTQMaxTerms(q) = qp->max_clauses;
    return q;
}

/**
 * Adds another field to the top of the FieldStack.
 */
static FrtHashSet *add_field(FrtQParser *qp, const char *field_name)
{
    FrtSymbol field = rb_intern(field_name);
    if (qp->allow_any_fields || frt_hs_exists(qp->all_fields, (void *)field)) {
        frt_hs_add(qp->fields, (void *)field);
    }
    return qp->fields;
}

/**
 * The method gets called when a field modifier ("field1|field2:") is seen. It
 * will push a new FieldStack object onto the stack and add +field+ to its
 * fields set.
 */
static FrtHashSet *first_field(FrtQParser *qp, const char *field_name)
{
    qp_push_fields(qp, frt_hs_new_ptr(NULL), true);
    return add_field(qp, field_name);
}

/**
 * Destroy a phrase object freeing all allocated memory.
 */
static void ph_destroy(Phrase *self)
{
    int i;
    for (i = 0; i < self->size; i++) {
        frt_ary_destroy(self->positions[i].terms, &free);
    }
    free(self->positions);
    free(self);
}


/**
 * Allocate a new Phrase object
 */
static Phrase *ph_new()
{
  Phrase *self = FRT_ALLOC_AND_ZERO(Phrase);
  self->capa = PHRASE_INIT_CAPA;
  self->positions = FRT_ALLOC_AND_ZERO_N(FrtPhrasePosition, PHRASE_INIT_CAPA);
  return self;
}

/**
 * Add the first word to the phrase. This method is also in charge of
 * allocating a new Phrase object.
 */
static Phrase *ph_first_word(char *word)
{
    Phrase *self = ph_new();
    if (word) { /* no point in adding NULL in start */
        self->positions[0].terms = frt_ary_new_type_capa(char *, 1);
        frt_ary_push(self->positions[0].terms, frt_estrdup(word));
        self->size = 1;
    }
    return self;
}

/**
 * Add a new word to the Phrase
 */
static Phrase *ph_add_word(Phrase *self, char *word)
{
    if (word) {
        const int index = self->size;
        FrtPhrasePosition *pp = self->positions;
        if (index >= self->capa) {
            self->capa <<= 1;
            FRT_REALLOC_N(pp, FrtPhrasePosition, self->capa);
            self->positions = pp;
        }
        pp[index].pos = self->pos_inc;
        pp[index].terms = frt_ary_new_type_capa(char *, 1);
        frt_ary_push(pp[index].terms, frt_estrdup(word));
        self->size++;
        self->pos_inc = 0;
    }
    else {
        self->pos_inc++;
    }
    return self;
}

/**
 * Adds a word to the Phrase object in the same position as the previous word
 * added to the Phrase. This will later be turned into a multi-PhraseQuery.
 */
static Phrase *ph_add_multi_word(Phrase *self, char *word)
{
    const int index = self->size - 1;
    FrtPhrasePosition *pp = self->positions;

    if (word) {
        frt_ary_push(pp[index].terms, frt_estrdup(word));
    }
    return self;
}

/**
 * Build a phrase query for a single field. It might seem like a better idea
 * to build the PhraseQuery once and duplicate it for each field but this
 * would be buggy in the case of PerFieldAnalyzers in which case a different
 * tokenizer could be used for each field.
 *
 * Note that the query object returned by this method is not always a
 * PhraseQuery. If there is only one term in the query then the query is
 * simplified to a TermQuery. If there are multiple terms but only a single
 * position, then a MultiTermQuery is retured.
 *
 * Note that each word in the query gets tokenized. Unlike get_term_q, if the
 * word gets tokenized into more than one token, the rest of the tokens are
 * ignored. For example, if you have the phrase;
 *
 *      "email: dbalmain@gmail.com"
 *
 * the Phrase object will contain two positions with the words 'email:' and
 * 'dbalmain@gmail.com'. Now, if you are using a LetterTokenizer then the
 * second word will be tokenized into the tokens ['dbalmain', 'gmail', 'com']
 * and only the first token will be used, so the resulting phrase query will
 * actually look like this;
 *
 *      "email dbalmain"
 *
 * This problem can easily be solved by using the StandardTokenizer or any
 * custom tokenizer which will leave dbalmain@gmail.com as a single token.
 */
static FrtQuery *get_phrase_query(FrtQParser *qp, FrtSymbol field, Phrase *phrase, char *slop_str, rb_encoding *encoding)
{
    const int pos_cnt = phrase->size;
    FrtQuery *q = NULL;

    if (pos_cnt == 1) {
        char **words = phrase->positions[0].terms;
        const int word_count = frt_ary_size(words);
        if (word_count == 1) {
            q = get_term_q(qp, field, words[0], encoding);
        }
        else {
            int i;
            int term_cnt = 0;
            FrtToken *token;
            char *last_word = NULL;

            for (i = 0; i < word_count; i++) {
                token = frt_ts_next(get_cached_ts(qp, field, words[i], encoding));
                if (token) {
                    free(words[i]);
                    last_word = words[i] = frt_estrdup(token->text);
                    ++term_cnt;
                }
                else {
                    /* empty words will later be ignored */
                    words[i][0] = '\0';
                }
            }

            switch (term_cnt) {
                case 0:
                    q = frt_bq_new(false);
                    break;
                case 1:
                    q = frt_tq_new(field, last_word);
                    break;
                default:
                    q = frt_multi_tq_new_conf(field, term_cnt, 0.0);
                    for (i = 0; i < word_count; i++) {
                        /* ignore empty words */
                        if (words[i][0]) {
                            frt_multi_tq_add_term(q, words[i]);
                        }
                    }
                    break;
            }
        }
    }
    else if (pos_cnt > 1) {
        FrtToken *token;
        FrtTokenStream *stream;
        int i, j;
        int pos_inc = 0;
        q = frt_phq_new(field);
        if (slop_str) {
            int slop;
            sscanf(slop_str,"%d",&slop);
            ((FrtPhraseQuery *)q)->slop = slop;
        }

        for (i = 0; i < pos_cnt; i++) {
            char **words = phrase->positions[i].terms;
            const int word_count = frt_ary_size(words);
            if (pos_inc) {
                ((FrtPhraseQuery *)q)->slop++;
            }
            pos_inc += phrase->positions[i].pos + 1; /* Actually holds pos_inc*/

            if (word_count == 1) {
                stream = get_cached_ts(qp, field, words[0], encoding);
                while ((token = frt_ts_next(stream))) {
                    if (token->pos_inc) {
                        frt_phq_add_term(q, token->text,
                                     pos_inc ? pos_inc : token->pos_inc);
                    }
                    else {
                        frt_phq_append_multi_term(q, token->text);
                        ((FrtPhraseQuery *)q)->slop++;
                    }
                    pos_inc = 0;
                }
            }
            else {
                bool added_position = false;

                for (j = 0; j < word_count; j++) {
                    stream = get_cached_ts(qp, field, words[j], encoding);
                    if ((token = frt_ts_next(stream))) {
                        if (!added_position) {
                            frt_phq_add_term(q, token->text,
                                         pos_inc ? pos_inc : token->pos_inc);
                            added_position = true;
                            pos_inc = 0;
                        }
                        else {
                            frt_phq_append_multi_term(q, token->text);
                        }
                    }
                }
            }
        }
    }
    return q;
}

/**
 * Get a phrase query from the Phrase object. The Phrase object is built up by
 * the query parser as the all PhraseQuery didn't work well for this. Once the
 * PhraseQuery has been built the Phrase object needs to be destroyed.
 */
static FrtQuery *get_phrase_q(FrtQParser *qp, Phrase *phrase, char *slop_str, rb_encoding *encoding)
{
    FrtQuery *volatile q = NULL;
    FLDS(q, get_phrase_query(qp, field, phrase, slop_str, encoding));
    ph_destroy(phrase);
    return q;
}

/**
 * Gets a RangeQuery object.
 *
 * Just like with WildCardQuery, RangeQuery needs to downcase its terms if the
 * tokenizer also downcased its terms.
 */
static FrtQuery *get_r_q(FrtQParser *qp, FrtSymbol field, char *from, char *to, bool inc_lower, bool inc_upper, rb_encoding *encoding) {
    FrtQuery *rq;
    if (qp->wild_lower
        && (!qp->tokenized_fields || frt_hs_exists(qp->tokenized_fields, (void *)field))) {
        if (from)
            lower_str(from, strlen(from), encoding);
        if (to)
            lower_str(to, strlen(to), encoding);
    }
/*
 * terms don't get tokenized as it doesn't really make sense to do so for
 * range queries.

    if (from) {
        FrtTokenStream *stream = get_cached_ts(qp, field, from, encoding);
        FrtToken *token = frt_ts_next(stream);
        from = token ? frt_estrdup(token->text) : NULL;
    }
    if (to) {
        FrtTokenStream *stream = get_cached_ts(qp, field, to, encoding);
        FrtToken *token = frt_ts_next(stream);
        to = token ? frt_estrdup(token->text) : NULL;
    }
*/

    rq = qp->use_typed_range_query ?
        frt_trq_new(field, from, to, inc_lower, inc_upper) :
        frt_rq_new(field, from, to, inc_lower, inc_upper);
    return rq;
}

/**
 * Every time the query parser sees a new field modifier ("field1|field2:")
 * it pushes a new FieldStack object onto the stack and sets its fields to the
 * fields specified in the fields modifier. If the field modifier is '*',
 * fs->fields is set to all_fields. fs->fields is set to +qp->def_field+ at
 * the bottom of the stack (ie the very first set of fields pushed onto the
 * stack).
 */
static void qp_push_fields(FrtQParser *self, FrtHashSet *fields, bool destroy)
{
    FrtFieldStack *fs = FRT_ALLOC(FrtFieldStack);

    fs->next    = self->fields_top;
    fs->fields  = fields;
    fs->destroy = destroy;

    self->fields_top = fs;
    self->fields = fields;
}

/**
 * Pops the top of the fields stack and frees any memory used by it. This will
 * get called when query modified by a field modifier ("field1|field2:") has
 * been fully parsed and the field specifier no longer applies.
 */
static void qp_pop_fields(FrtQParser *self)
{
    FrtFieldStack *fs = self->fields_top;

    if (fs->destroy) {
        frt_hs_destroy(fs->fields);
    }
    self->fields_top = fs->next;
    if (self->fields_top) {
        self->fields = self->fields_top->fields;
    }
    free(fs);
}

/**
 * Free all memory allocated by the QueryParser.
 */
void frt_qp_destroy(FrtQParser *self)
{
    if (self->tokenized_fields != self->all_fields) {
        frt_hs_destroy(self->tokenized_fields);
    }
    if (self->def_fields != self->all_fields) {
        frt_hs_destroy(self->def_fields);
    }
    frt_hs_destroy(self->all_fields);

    qp_pop_fields(self);
    assert(NULL == self->fields_top);

    frt_h_destroy(self->ts_cache);
    frt_tk_destroy(self->non_tokenizer);
    frt_a_deref(self->analyzer);
    free(self);
}

/**
 * Creates a new QueryParser setting all boolean parameters to their defaults.
 * If +def_fields+ is NULL then +all_fields+ is used in place of +def_fields+.
 * Not also that this method ensures that all fields that exist in
 * +def_fields+ must also exist in +all_fields+. This should make sense.
 */
FrtQParser *frt_qp_new(FrtAnalyzer *analyzer)
{
    FrtQParser *self = FRT_ALLOC(FrtQParser);
    self->or_default = true;
    self->wild_lower = true;
    self->clean_str = false;
    self->max_clauses = FRT_QP_MAX_CLAUSES;
    self->handle_parse_errors = false;
    self->allow_any_fields = false;
    self->use_keywords = true;
    self->use_typed_range_query = false;
    self->def_slop = 0;

    self->def_fields = frt_hs_new_ptr(NULL);
    self->all_fields = frt_hs_new_ptr(NULL);
    self->tokenized_fields = frt_hs_new_ptr(NULL);
    self->fields_top = NULL;

    /* make sure all_fields contains the default fields */
    qp_push_fields(self, self->def_fields, false);

    self->analyzer = analyzer;
    self->ts_cache = frt_h_new_ptr((frt_free_ft)&frt_ts_deref);
    self->buf_index = 0;
    self->dynbuf = NULL;
    self->non_tokenizer = frt_non_tokenizer_new();
    frt_mutex_init(&self->mutex, NULL);
    return self;
}

void frt_qp_add_field(FrtQParser *self, FrtSymbol field, bool is_default, bool is_tokenized)
{
    frt_hs_add(self->all_fields, (void *)field);
    if (is_default) {
        frt_hs_add(self->def_fields, (void *)field);
    }
    if (is_tokenized) {
        frt_hs_add(self->tokenized_fields, (void *)field);
    }
}

/* these chars have meaning within phrases */
static const char *PHRASE_CHARS = "<>|\"";

/**
 * +str_insert_char+ inserts a character at the beginning of a string by
 * shifting the rest of the string right.
 */
static void str_insert_char(char *str, int len, char chr)
{
    memmove(str+1, str, len*sizeof(char));
    *str = chr;
}

/**
 * +frt_qp_clean_str+ basically scans the query string and ensures that all open
 * and close parentheses '()' and quotes '"' are balanced. It does this by
 * inserting or appending extra parentheses or quotes to the string. This
 * obviously won't necessarily be exactly what the user wanted but we are
 * never going to know that anyway. The main job of this method is to help the
 * query at least parse correctly.
 *
 * It also checks that all special characters within phrases (ie between
 * quotes) are escaped correctly unless they have meaning within a phrase
 * ( <>,|," ). Note that '<' and '>' will also be escaped unless the appear
 * together like so; '<>'.
 */
char *frt_qp_clean_str(char *str)
{
    int b, pb = -1;
    int br_cnt = 0;
    bool quote_open = false;
    char *sp, *nsp;

    /* leave a little extra */
    char *new_str = FRT_ALLOC_N(char, strlen(str)*2 + 1);

    for (sp = str, nsp = new_str; *sp; sp++) {
        b = *sp;
        /* ignore escaped characters */
        if (pb == '\\') {
            if (quote_open && strrchr(PHRASE_CHARS, b)) {
                *nsp++ = '\\'; /* this was left off the first time through */
            }
            *nsp++ = b;
            /* \ has escaped itself so has no power. Assign pb random char 'r' */
            pb = ((b == '\\') ? 'r' : b);
            continue;
        }
        switch (b) {
            case '\\':
                if (!quote_open) { /* We do our own escaping below */
                    *nsp++ = b;
                }
                break;
            case '"':
                quote_open = !quote_open;
                *nsp++ = b;
                break;
            case '(':
              if (!quote_open) {
                  br_cnt++;
              }
              else {
                  *nsp++ = '\\';
              }
              *nsp++ = b;
              break;
            case ')':
                if (!quote_open) {
                    if (br_cnt == 0) {
                        str_insert_char(new_str, (int)(nsp - new_str), '(');
                        nsp++;
                    }
                    else {
                        br_cnt--;
                    }
                }
                else {
                    *nsp++ = '\\';
                }
                *nsp++ = b;
                break;
            case '>':
                if (quote_open) {
                    if (pb == '<') {
                        /* remove the escape character */
                        nsp--;
                        nsp[-1] = '<';
                    }
                    else {
                        *nsp++ = '\\';
                    }
                }
                *nsp++ = b;
                break;
            default:
                if (quote_open) {
                    if (strrchr(special_char, b) && b != '|') {
                        *nsp++ = '\\';
                    }
                }
                *nsp++ = b;
        }
        pb = b;
    }
    if (quote_open) {
        *nsp++ = '"';
    }
    for (;br_cnt > 0; br_cnt--) {
      *nsp++ = ')';
    }
    *nsp = '\0';
    return new_str;
}

/**
 * Takes a string and finds whatever tokens it can using the QueryParser's
 * analyzer. It then turns these tokens (if any) into a boolean query. If it
 * fails to find any tokens, this method will return NULL.
 */
static FrtQuery *qp_get_bad_query(FrtQParser *qp, char *str, rb_encoding *encoding)
{
    FrtQuery *volatile q = NULL;
    qp->recovering = true;
    assert(qp->fields_top->next == NULL);
    FLDS(q, get_term_q(qp, field, str, encoding));
    return q;
}

/**
 * +qp_parse+ takes a string and turns it into a Query object using Ferret's
 * query language. It must either raise an error or return a query object. It
 * must not return NULL. If the yacc parser fails it will use a very basic
 * boolean query parser which takes whatever tokens it can find in the query
 * and turns them into a boolean query on the default fields.
 */

FrtQuery *qp_parse(FrtQParser *self, char *query_string, rb_encoding *encoding)
{
    FrtQuery *result = NULL;
    char *qstr;
    unsigned char *dp_start = NULL;

    frt_mutex_lock(&self->mutex);
    /* if qp->fields_top->next is not NULL we have a left over field-stack
     * object that was not popped during the last query parse */
    assert(NULL == self->fields_top->next);

    /* encode query_string to utf8 for futher processing unless it is utf8 encoded */
    if (encoding == utf8_encoding) {
        qstr = query_string;
    } else {
        /* assume query is sbc encoded und encoding to utf results in maximum utf mbc expansion */
        const unsigned char *sp = (unsigned char *)query_string;
        int query_string_len = strlen(query_string);
        int dp_length = query_string_len * utf8_mbmaxlen + 1;
        unsigned char *dp = FRT_ALLOC_N(unsigned char, dp_length);
        dp_start = dp;
        rb_econv_t *ec = rb_econv_open(rb_enc_name(encoding), rb_enc_name(utf8_encoding), RUBY_ECONV_INVALID_REPLACE);
        assert(ec != NULL);
        rb_econv_convert(ec, &sp, (unsigned char *)query_string + query_string_len, &dp, (unsigned char *)dp + dp_length - 1, 0);
        rb_econv_close(ec);
        *dp = '\0';
        qstr = dp_start;
    }

    self->recovering = self->destruct = false;

    if (self->clean_str) {
        self->qstrp = self->qstr = frt_qp_clean_str(qstr);
    } else {
        self->qstrp = self->qstr = qstr;
    }
    self->fields = self->def_fields;
    self->result = NULL;

    if (0 == yyparse(self, encoding))
      result = self->result;

    if (!result && self->handle_parse_errors) {
        self->destruct = false;
        result = qp_get_bad_query(self, self->qstr, encoding);
    }
    if (self->destruct && !self->handle_parse_errors)
        FRT_RAISE(FRT_PARSE_ERROR, frt_xmsg_buffer);

    if (!result)
        result = frt_bq_new(false);

    if (self->clean_str)
        free(self->qstr);
    if (dp_start)
        free(dp_start);

    frt_mutex_unlock(&self->mutex);
    return result;
}
