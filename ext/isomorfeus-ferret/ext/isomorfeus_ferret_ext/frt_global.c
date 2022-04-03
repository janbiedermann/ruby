#include "frt_global.h"
#include "frt_hash.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <ruby/encoding.h>

const char *FRT_EMPTY_STRING = "";

rb_encoding *utf8_encoding;
int utf8_mbmaxlen;
OnigCodePoint cp_apostrophe;
OnigCodePoint cp_dot;
OnigCodePoint cp_comma;
OnigCodePoint cp_backslash;
OnigCodePoint cp_slash;
OnigCodePoint cp_underscore;
OnigCodePoint cp_dash;
OnigCodePoint cp_hyphen;
OnigCodePoint cp_at;
OnigCodePoint cp_ampersand;
OnigCodePoint cp_colon;

int frt_scmp(const void *p1, const void *p2) {
    return strcmp(*(char **) p1, *(char **) p2);
}

void frt_strsort(char **str_array, int size) {
    qsort(str_array, size, sizeof(char *), &frt_scmp);
}

int frt_icmp(const void *p1, const void *p2) {
    int i1 = *(int *) p1;
    int i2 = *(int *) p2;

    if (i1 > i2) {
        return 1;
    }
    else if (i1 < i2) {
        return -1;
    }
    return 0;
}

int frt_icmp_risky(const void *p1, const void *p2) {
  return (*(int *)p1) - *((int *)p2);
}

unsigned int *frt_imalloc(unsigned int value) {
  unsigned int *p = FRT_ALLOC(unsigned int);
  *p = value;
  return p;
}

unsigned long *frt_lmalloc(unsigned long value) {
  unsigned long *p = FRT_ALLOC(unsigned long);
  *p = value;
  return p;
}

frt_u32 *frt_u32malloc(frt_u32 value) {
  frt_u32 *p = FRT_ALLOC(frt_u32);
  *p = value;
  return p;
}

frt_u64 *frt_u64malloc(frt_u64 value) {
  frt_u64 *p = FRT_ALLOC(frt_u64);
  *p = value;
  return p;
}

/* concatenate two strings freeing the second */
char *frt_estrcat(char *str1, char *str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    FRT_REALLOC_N(str1, char, len1 + len2 + 3);     /* leave room for <CR> */
    memcpy(str1 + len1, str2, len2 + 1);        /* make sure '\0' copied too */
    free(str2);
    return str1;
}

/* epstrdup: duplicate a string with a format, report if error */
char *frt_epstrdup(const char *fmt, int len, ...) {
    char *string;
    va_list args;
    len += (int) strlen(fmt);

    string = FRT_ALLOC_N(char, len + 1);
    va_start(args, len);
    vsprintf(string, fmt, args);
    va_end(args);

    return string;
}

/* frt_estrdup: duplicate a string, report if error */
char *frt_estrdup(const char *s) {
    char *t = FRT_ALLOC_N(char, strlen(s) + 1);
    strcpy(t, s);
    return t;
}

/* Pretty print a float to the buffer. The buffer should have at least 32
 * bytes available.
 */
char *frt_dbl_to_s(char *buf, double num) {
    char *p, *e;

#ifdef FRT_IS_C99
    if (isinf(num)) {
        return frt_estrdup(num < 0 ? "-Infinity" : "Infinity");
    } else if (isnan(num)) {
        return frt_estrdup("NaN");
    }
#endif

    sprintf(buf, FRT_DBL2S, num);
    if (!(e = strchr(buf, 'e'))) {
        e = buf + strlen(buf);
    }
    if (!isdigit(e[-1])) {
        /* reformat if ended with decimal point (ex 111111111111111.) */
        sprintf(buf, "%#.6e", num);
        if (!(e = strchr(buf, 'e'))) { e = buf + strlen(buf); }
    }
    p = e;
    while (p[-1] == '0' && isdigit(p[-2])) {
        p--;
    }

    memmove(p, e, strlen(e) + 1);
    return buf;
}

/**
 * frt_strapp: appends a string up to, but not including the \0 character to the
 * end of a string returning a pointer to the next unassigned character in the
 * string.
 */
char *frt_strapp(char *dst, const char *src) {
    while (*src != '\0') {
        *dst = *src;
        ++dst;
        ++src;
    }
    return dst;
}

/* strfmt: like sprintf except that it allocates memory for the string */
char *frt_vstrfmt(const char *fmt, va_list args) {
    char *string;
    char *p = (char *) fmt, *q;
    int len = (int) strlen(fmt) + 1;
    int slen, curlen;
    const char *s;
    long l;
    double d;

    q = string = FRT_ALLOC_N(char, len);

    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
            case 's':
                p++;
                s = va_arg(args, char *);
                /* to be consistent with printf print (null) for NULL */
                if (!s) {
                    s = "(null)";
                }
                slen = (int) strlen(s);
                len += slen;
                curlen = q - string;
                FRT_REALLOC_N(string, char, len);
                q = string + curlen;
                memcpy(q, s, slen);
                q += slen;
                continue;
            case 'f':
                p++;
                len += 32;
                *q = 0;
                FRT_REALLOC_N(string, char, len);
                q = string + strlen(string);
                d = va_arg(args, double);
                frt_dbl_to_s(q, d);
                q += strlen(q);
                continue;
            case 'd':
                p++;
                len += 20;
                *q = 0;
                FRT_REALLOC_N(string, char, len);
                q = string + strlen(string);
                l = va_arg(args, long);
                q += sprintf(q, "%ld", l);
                continue;
            default:
                break;
            }
        }
        *q = *p;
        p++;
        q++;
    }
    *q = 0;

    return string;
}

char *frt_strfmt(const char *fmt, ...) {
    va_list args;
    char *str;
    va_start(args, fmt);
    str = frt_vstrfmt(fmt, args);
    va_end(args);
    return str;
}

void frt_dummy_free(void *p) {
    (void)p; /* suppress unused argument warning */
}

#ifdef HAVE_GDB
#define CMD_BUF_SIZE (128 + FILENAME_MAX)
/* need to declare this as it is masked by default in linux */

static char *build_shell_command(void) {
    int   pid = getpid();
    char *buf = FRT_ALLOC_N(char, CMD_BUF_SIZE);
    char *command =
        "gdb -quiet -ex='bt' -ex='quit' %s %d 2>/dev/null | grep '^[ #]'";

    snprintf(buf, CMD_BUF_SIZE, command, frt_progname(), pid);
    return buf;
}

#endif

/**
 * Call out to gdb to get our stacktrace.
 */
char *frt_get_stacktrace(void) {
#ifdef HAVE_GDB
    FILE *stream;
    char *gdb_filename = NULL, *buf = NULL, *stack = NULL;
    int   offset = -FRT_BUFFER_SIZE;

    if ( !(buf = build_shell_command()) ) {
        fprintf(EXCEPTION_STREAM,
                "Unable to build stacktrace shell command\n");
        goto cleanup;
    }

    if ( !(stream = popen(buf, "r")) ) {
        fprintf(EXCEPTION_STREAM,
                "Unable to exec stacktrace shell command: '%s'\n", buf);
        goto cleanup;
    }

    do {
        offset += FRT_BUFFER_SIZE;
        FRT_REALLOC_N(stack, char, offset + FRT_BUFFER_SIZE);
        FRT_ZEROSET_N(stack + offset, char, FRT_BUFFER_SIZE);
    } while(fread(stack + offset, 1, FRT_BUFFER_SIZE, stream) == FRT_BUFFER_SIZE);

    pclose(stream);

 cleanup:
    if (gdb_filename) free(gdb_filename);
    if (buf) free(buf);
    return stack;
#else
    return NULL;
#endif
}

void frt_print_stacktrace(void) {
    char *stack = frt_get_stacktrace();

    if (stack) {
        fprintf(EXCEPTION_STREAM, "Stack trace:\n%s", stack);
        free(stack);
    } else {
        fprintf(EXCEPTION_STREAM, "Stack trace not avaialble\n");
    }
}

typedef struct FreeMe {
    void *p;
    frt_free_ft free_func;
} FreeMe;

static FreeMe *free_mes = NULL;
static int free_mes_size = 0;
static int free_mes_capa = 0;

void frt_register_for_cleanup(void *p, frt_free_ft free_func) {
    FreeMe *free_me;
    if (free_mes_capa == 0) {
        free_mes_capa = 16;
        free_mes = FRT_ALLOC_N(FreeMe, free_mes_capa);
    }
    else if (free_mes_capa <= free_mes_size) {
        free_mes_capa *= 2;
        FRT_REALLOC_N(free_mes, FreeMe, free_mes_capa);
    }
    free_me = free_mes + free_mes_size++;
    free_me->p = p;
    free_me->free_func = free_func;
}

#define MAX_PROG_NAME 200
static char name[MAX_PROG_NAME]; /* program name for error msgs */

/* frt_setprogname: set stored name of program */
void frt_setprogname(const char *str) {
    strncpy(name, str, sizeof(name) - 1);
}

const char *frt_progname(void) {
    return name;
}

static const char *signal_to_string(int signum) {
    switch (signum)
    {
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
#if !defined POSH_OS_WIN32 && !defined POSH_OS_WIN64
        case SIGBUS:  return "SIGBUS";
#endif
        case SIGSEGV: return "SIGSEGV";
    }

    return "Unknown Signal";
}

static void sighandler_crash(int signum) {
    frt_print_stacktrace();
    FRT_XEXIT("Signal", "Exiting on signal %s (%d)", signal_to_string(signum), signum);
}

#define SETSIG_IF_UNSET(sig, handler) do { \
    signal(sig, handler);                  \
} while(0)

void frt_init(int argc, const char *const argv[]) {
    if (argc > 0) {
        frt_setprogname(argv[0]);
    }

    SETSIG_IF_UNSET(SIGILL , sighandler_crash);
    SETSIG_IF_UNSET(SIGABRT, sighandler_crash);
    SETSIG_IF_UNSET(SIGFPE , sighandler_crash);
#if !defined POSH_OS_WIN32 && !defined POSH_OS_WIN64
    SETSIG_IF_UNSET(SIGBUS , sighandler_crash);
#endif
    SETSIG_IF_UNSET(SIGSEGV, sighandler_crash);

    atexit(&frt_hash_finalize);

    utf8_encoding = rb_enc_find("UTF-8");
    utf8_mbmaxlen = rb_enc_mbmaxlen(utf8_encoding);
    char *p = "'";
    cp_apostrophe = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = ".";
    cp_dot = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = ",";
    cp_comma = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "\\";
    cp_backslash = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "/";
    cp_slash = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "_";
    cp_underscore = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "-";
    cp_dash = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "\u2010";
    cp_hyphen = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "@";
    cp_at = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "&";
    cp_ampersand = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = ":";
    cp_colon = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
}

/**
 * For general use when testing
 *
 * TODO wrap in #ifdef
 */

static bool p_switch = false;
static bool p_switch_tmp = false;

void p(const char *format, ...) {
    va_list args;

    if (!p_switch) return;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void p_on(void) {
    fprintf(stderr, "> > > > > STARTING PRINT\n");
    p_switch = true;
}

void p_off(void) {
    fprintf(stderr, "< < < < < STOPPING PRINT\n");
    p_switch = false;
}

void frt_p_pause(void) {
    p_switch_tmp = p_switch;
    p_switch = false;
}

void frt_p_resume(void) {
    p_switch = p_switch_tmp;
}
