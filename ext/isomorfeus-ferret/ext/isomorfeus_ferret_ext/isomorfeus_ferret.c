#include <errno.h>
#include "isomorfeus_ferret.h"
#include "frt_global.h"
#include "frt_except.h"
#include "frt_hash.h"
#include "frt_hashset.h"
#include "frb_threading.h"
#include "frb_lang.h"


/* Object Map */
static FrtHash *object_map;

/* IDs */
ID id_new;
ID id_call;
ID id_eql;
ID id_hash;
ID id_capacity;
ID id_less_than;
ID id_lt;
ID id_is_directory;
ID id_close;
ID id_cclass;
ID id_data;

static ID id_mkdir_p;

/* Symbols */
VALUE sym_yes;
VALUE sym_no;
VALUE sym_true;
VALUE sym_false;
VALUE sym_path;
VALUE sym_dir;

/* Modules */
VALUE mIsomorfeus;
VALUE mFerret;
VALUE mStore;
VALUE mStringHelper;
VALUE mSpans;

/* Classes */
VALUE cTerm;
VALUE cFileNotFoundError;
VALUE cParseError;
VALUE cStateError;

void Init_Benchmark(void);
void Init_Test(void);

unsigned long long value_hash(const void *key)
{
    return (unsigned long long)key;
}

int value_eq(const void *key1, const void *key2)
{
    return key1 == key2;
}

VALUE object_get(void *key)
{
    VALUE val = (VALUE)frt_h_get(object_map, key);
    if (!val) val = Qnil;
    return val;
}

//static int hash_cnt = 0;
void
//object_add(void *key, VALUE obj)
object_add2(void *key, VALUE obj, const char *file, int line)
{
    if (frt_h_get(object_map, key))
        printf("failed adding %lx to %lld; already contains %llx. %s:%d\n",
               (long)obj, (long long)key, (long long)frt_h_get(object_map, key), file, line);
    frt_h_set(object_map, key, (void *)obj);
}

void
//object_set(void *key, VALUE obj)
object_set2(void *key, VALUE obj, const char *file, int line)
{
    frt_h_set(object_map, key, (void *)obj);
}

void
//object_del(void *key)
object_del2(void *key, const char *file, int line)
{
    if (object_get(key) == Qnil)
        printf("failed deleting %lld. %s:%d\n", (long long)key, file, line);
    frt_h_del(object_map, key);
}

void frb_gc_mark(void *key)
{
    VALUE val = (VALUE)frt_h_get(object_map, key);
    if (val)
        rb_gc_mark(val);
}

VALUE frb_data_alloc(VALUE klass)
{
    return Frt_Make_Struct(klass);
}

void frb_deref_free(void *p)
{
    object_del(p);
}

void frb_thread_once(int *once_control, void (*init_routine) (void))
{
    if (*once_control) {
        init_routine();
        *once_control = 0;
    }
}

void frb_thread_key_create(frt_thread_key_t *key, void (*destr_function)(void *))
{
    *key = frt_h_new(&value_hash, &value_eq, NULL, destr_function);
}

void frb_thread_key_delete(frt_thread_key_t key)
{
    frt_h_destroy(key);
}

void frb_thread_setspecific(frt_thread_key_t key, const void *pointer)
{
    frt_h_set(key, (void *)rb_thread_current(), (void *)pointer);
}

void *frb_thread_getspecific(frt_thread_key_t key)
{
    return frt_h_get(key, (void *)rb_thread_current());
}

void frb_create_dir(VALUE rpath)
{
    VALUE mFileUtils;
    mFileUtils = rb_define_module("FileUtils");
    rb_funcall(mFileUtils, id_mkdir_p, 1, rpath);
}

VALUE frb_hs_to_rb_ary(FrtHashSet *hs)
{
    FrtHashSetEntry *hse;
    VALUE ary = rb_ary_new();

    for (hse = hs->first; hse; hse = hse->next) {
        rb_ary_push(ary, rb_str_new2(hse->elem));
    }
    return ary;
}

void *frb_rb_data_ptr(VALUE val)
{
    Check_Type(val, T_DATA);
    return DATA_PTR(val);
}

char *
rs2s(VALUE rstr)
{
    return (char *)(RSTRING_PTR(rstr) ? RSTRING_PTR(rstr) : FRT_EMPTY_STRING);
}

char *
rstrdup(VALUE rstr)
{
    char *old = rs2s(rstr);
    //int len = RSTRING_LEN(rstr);
    //char *new = FRT_ALLOC_N(char, len + 1);
    //memcpy(new, old, len);
    return frt_estrdup(old);
}

FrtSymbol
frb_field(VALUE rfield)
{
    switch (TYPE(rfield)) {
        case T_SYMBOL:
            return rb_to_id(rfield);
        case T_STRING:
            return rb_intern_str(rfield);
        default:
            rb_raise(rb_eArgError, "field name must be a symbol or string");
            return (ID)NULL;
    }
}

/*
 * Json Exportation - Loading each LazyDoc and formatting them into json
 * This code is designed to get a VERY FAST json string, the goal was speed,
 * not sexiness.
 * Jeremie 'ahFeel' BORDIER
 * ahFeel@rift.Fr
 */
char *
json_concat_string(char *s, char *field)
{
    *(s++) = '"';
	while (*field) {
		if (*field == '"') {
            *(s++) = '\'';
            *(s++) = *(field++);
            *(s++) = '\'';
        }
        else {
            *(s++) = *(field++);
        }
    }
    *(s++) = '"';
    return s;
}

static VALUE error_map;

VALUE frb_get_error(const char *err_type)
{
    VALUE error_class;
    if (Qnil != (error_class = rb_hash_aref(error_map, rb_intern(err_type)))) {
        return error_class;
    }
    return rb_eStandardError;
}

#define FRT_BUF_SIZ 2046
#ifdef FRT_HAS_VARARGS
void vfrt_rb_raise(const char *file, int line_num, const char *func,
                   const char *err_type, const char *fmt, va_list args)
#else
void V_FRT_EXIT(const char *err_type, const char *fmt, va_list args)
#endif
{
    char buf[FRT_BUF_SIZ];
    size_t so_far = 0;
#ifdef FRT_HAS_VARARGS
    snprintf(buf, FRT_BUF_SIZ, "%s occurred at <%s>:%d in %s\n",
            err_type, file, line_num, func);
#else
    snprintf(buf, FRT_BUF_SIZ, "%s occurred:\n", err_type);
#endif
    so_far = strlen(buf);
    vsnprintf(buf + so_far, FRT_BUF_SIZ - so_far, fmt, args);

    so_far = strlen(buf);
    if (fmt[0] != '\0' && fmt[strlen(fmt) - 1] == ':') {
        snprintf(buf + so_far, FRT_BUF_SIZ - so_far, " %s", strerror(errno));
        so_far = strlen(buf);
    }

    snprintf(buf + so_far, FRT_BUF_SIZ - so_far, "\n");
    rb_raise(frb_get_error(err_type), "%s", buf);
}

#ifdef FRT_HAS_VARARGS
void frb_rb_raise(const char *file, int line_num, const char *func,
                  const char *err_type, const char *fmt, ...)
#else
void FRT_EXIT(const char *err_type, const char *fmt, ...)
#endif
{
    va_list args;
    va_start(args, fmt);
#ifdef FRT_HAS_VARARGS
    vfrt_rb_raise(file, line_num, func, err_type, fmt, args);
#else
    V_FRT_EXIT(err_type, fmt, args);
#endif
    va_end(args);
}

/****************************************************************************
 *
 * Term Methods
 *
 ****************************************************************************/
static ID id_field;
static ID id_text;

VALUE frb_get_term(FrtSymbol field, const char *text)
{
    return rb_struct_new(cTerm,
                         ID2SYM(field),
                         rb_str_new_cstr(text),
                         NULL);
}

static VALUE frb_term_to_s(VALUE self)
{
    VALUE rstr;
    VALUE rfield = rb_funcall(self, id_field, 0);
    VALUE rtext = rb_funcall(self, id_text, 0);
    char *field = StringValuePtr(rfield);
    char *text = StringValuePtr(rtext);
    char *term_str = FRT_ALLOC_N(char, 5 + RSTRING_LEN(rfield) + RSTRING_LEN(rtext));
    sprintf(term_str, "%s:%s", field, text);
    rstr = rb_str_new2(term_str);
    free(term_str);
    return rstr;
}
/*
 *  Document-class: Ferret::Term
 *
 *  == Summary
 *
 *  A Term holds a term from a document and its field name (as a Symbol).
 */
void Init_Term(void)
{
    const char *term_class = "Term";
    cTerm = rb_struct_define(term_class, "field", "text", NULL);
    rb_set_class_path(cTerm, mFerret, term_class);
    rb_const_set(mFerret, rb_intern(term_class), cTerm);
    rb_define_method(cTerm, "to_s", frb_term_to_s, 0);
    id_field = rb_intern("field");
    id_text = rb_intern("text");
}

/*
 *  Document-module: Ferret
 *
 *  See the README
 */
void Init_Ferret(void)
{
    Init_Term();
    rb_require("fileutils");
}

void Init_isomorfeus_ferret_ext(void)
{
    const char *const progname[] = {"ruby"};

    frt_init(1, progname);

    /* initialize object map */
    object_map = frt_h_new(&value_hash, &value_eq, NULL, NULL);

    /* IDs */
    id_new = rb_intern("new");
    id_call = rb_intern("call");
    id_eql = rb_intern("eql?");
    id_hash = rb_intern("hash");

    id_capacity = rb_intern("capacity");
    id_less_than = rb_intern("less_than");
    id_lt = rb_intern("<");

    id_mkdir_p = rb_intern("mkdir_p");
    id_is_directory = rb_intern("directory?");
    id_close = rb_intern("close");

    id_cclass = rb_intern("cclass");

    id_data = rb_intern("@data");

    /* Symbols */
    sym_yes = ID2SYM(rb_intern("yes"));;
    sym_no = ID2SYM(rb_intern("no"));;
    sym_true = ID2SYM(rb_intern("true"));;
    sym_false = ID2SYM(rb_intern("false"));;
    sym_path = ID2SYM(rb_intern("path"));;
    sym_dir = ID2SYM(rb_intern("dir"));;

    mIsomorfeus = rb_define_module("Isomorfeus");
    mFerret = rb_define_module_under(mIsomorfeus, "Ferret");

    /* Inits */
    Init_Ferret();
    Init_Utils();
    Init_Analysis();
    Init_Store();
    Init_Index();
    Init_Search();
    Init_QueryParser();
    Init_Test();
    Init_Benchmark();

    /* Error Classes */
    cParseError = rb_define_class_under(mFerret, "ParseError", rb_eStandardError);
    cStateError = rb_define_class_under(mFerret, "StateError", rb_eStandardError);
    cFileNotFoundError = rb_define_class_under(mFerret, "FileNotFoundError", rb_eIOError);

    error_map = rb_hash_new();
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[2]), rb_eStandardError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[3]), rb_eIOError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[4]), cFileNotFoundError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[5]), rb_eArgError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[6]), rb_eEOFError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[7]), rb_eNotImpError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[8]), cStateError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[9]), cParseError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[10]), rb_eNoMemError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[11]), rb_eIndexError);
    rb_hash_aset(error_map, rb_intern(ERROR_TYPES[12]), cLockError);

    rb_define_const(mFerret, "EXCEPTION_MAP", error_map);
    rb_define_const(mFerret, "FIX_INT_MAX", INT2FIX(INT_MAX >> 1));
}

extern void frb_raise(int excode, const char *msg) {
    rb_raise(frb_get_error(ERROR_TYPES[excode]), "%s", msg);
}
