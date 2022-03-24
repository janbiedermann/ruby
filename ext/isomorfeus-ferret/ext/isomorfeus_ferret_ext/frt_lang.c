#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "frt_lang.h"
#include "frt_except.h"
#include "frt_global.h"

/* emalloc: malloc and report if error */
void *frt_emalloc(size_t size)
{
    void *p = malloc(size);

    if (p == NULL) {
        FRT_RAISE(FRT_MEM_ERROR, "failed to allocate %d bytes", (int)size);
    }

    return p;
}

/* frt_ecalloc: malloc, zeroset and report if error */
void *frt_ecalloc(size_t size)
{
    void *p = calloc(1, size);

    if (p == NULL) {
        FRT_RAISE(FRT_MEM_ERROR, "failed to allocate %d bytes", (int)size);
    }

    return p;
}

/* frt_erealloc: realloc and report if error */
void *frt_erealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);

    if (p == NULL) {
        FRT_RAISE(FRT_MEM_ERROR, "failed to reallocate %d bytes", (int)size);
    }

    return p;
}

/* void micro_sleep(const int micro_seconds)
{
#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64) && !defined __MINGW32__
    Sleep(micro_seconds / 1000);
#else
    usleep(micro_seconds);
#endif
} */

/* xexit: print error message and exit */
# ifdef FRT_HAS_VARARGS
void frt_vexit(const char *file, int line_num, const char *func,
                      const char *err_type, const char *fmt, va_list args)
# else
void FRT_VEXIT(const char *err_type, const char *fmt, va_list args)
# endif
{
    fflush(stdout);
    fprintf(EXCEPTION_STREAM, "\n%s: ", frt_progname());

# ifdef FRT_HAS_VARARGS
    fprintf(EXCEPTION_STREAM, "%s occurred at <%s>:%d in %s\n",
            err_type, file, line_num, func);
# else
    fprintf(EXCEPTION_STREAM, "%s occurred:\n", err_type);
# endif
    vfprintf(EXCEPTION_STREAM, fmt, args);

    if (fmt[0] != '\0' && fmt[strlen(fmt) - 1] == ':') {
        fprintf(EXCEPTION_STREAM, " %s", strerror(errno));
    }

    fprintf(EXCEPTION_STREAM, "\n");
    frt_print_stacktrace();
    if (frt_x_abort_on_exception) {
        exit(2);                 /* conventional value for failed execution */
    }
    else {
        frt_x_has_aborted = true;
    }
}


# ifdef FRT_HAS_VARARGS
void frt_xexit(const char *file, int line_num, const char *func,
              const char *err_type, const char *fmt, ...)
# else
void FRT_XEXIT(const char *err_type, const char *fmt, ...)
# endif
{
    va_list args;
    va_start(args, fmt);
# ifdef FRT_HAS_VARARGS
    frt_vexit(file, line_num, func, err_type, fmt, args);
# else
    FRT_VEXIT(err_type, fmt, args);
# endif
    va_end(args);
}
