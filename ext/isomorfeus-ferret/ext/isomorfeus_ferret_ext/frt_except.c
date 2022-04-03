#include <stdarg.h>
#include "frt_global.h"
#include "frt_except.h"
#include "frt_threading.h"

const char *const ERROR_TYPES[] = {
    "Body",
    "Finally",
    "Exception",
    "IO Error",
    "File Not Found Error",
    "Argument Error",
    "End-of-File Error",
    "Unsupported Function Error",
    "State Error",
    "Parse Error",
    "Memory Error",
    "Index Error",
    "Lock Error"
};

bool frt_x_do_logging = false;
bool frt_x_abort_on_exception = true;
bool frt_x_has_aborted = false;
FILE *frt_x_exception_stream = NULL;

const char *const FRT_UNSUPPORTED_ERROR_MSG = "Unsupported operation";
const char *const FRT_EOF_ERROR_MSG = "Read past end of file";
char frt_xmsg_buffer[FRT_XMSG_BUFFER_SIZE];
char frt_xmsg_buffer_final[FRT_XMSG_BUFFER_FINAL_SIZE];

static frt_thread_key_t exception_stack_key;
static frt_thread_once_t exception_stack_key_once = FRT_THREAD_ONCE_INIT;

static void exception_stack_alloc(void) {
    frt_thread_key_create(&exception_stack_key, NULL);
}

void frt_xpush_context(frt_xcontext_t *context) {
    frt_xcontext_t *top_context;
    frt_thread_once(&exception_stack_key_once, *exception_stack_alloc);
    top_context = (frt_xcontext_t *)frt_thread_getspecific(exception_stack_key);
    context->next = top_context;
    frt_thread_setspecific(exception_stack_key, context);
    context->handled = true;
    context->in_finally = false;
}

static void frt_xraise_context(frt_xcontext_t *context, volatile int excode, const char *const msg) {
    context->msg = msg;
    context->excode = excode;
    context->handled = false;
    longjmp(context->jbuf, excode);
}

void frt_xraise(int excode, const char *const msg) {
    frt_xcontext_t *top_context;
    frt_thread_once(&exception_stack_key_once, *exception_stack_alloc);
    top_context = (frt_xcontext_t *)frt_thread_getspecific(exception_stack_key);

    if (!top_context) {
        FRT_XEXIT(ERROR_TYPES[excode], msg);
    }
    else if (!top_context->in_finally) {
        frt_xraise_context(top_context, excode, msg);
    }
    else if (top_context->handled) {
        top_context->msg = msg;
        top_context->excode = excode;
        top_context->handled = false;
    }
}

void frt_xpop_context(void) {
    frt_xcontext_t *top_cxt, *context;
    frt_thread_once(&exception_stack_key_once, *exception_stack_alloc);
    top_cxt = (frt_xcontext_t *)frt_thread_getspecific(exception_stack_key);
    context = top_cxt->next;
    frt_thread_setspecific(exception_stack_key, context);
    if (!top_cxt->handled) {
        if (context) {
            frt_xraise_context(context, top_cxt->excode, top_cxt->msg);
        }
        else {
            FRT_XEXIT(ERROR_TYPES[top_cxt->excode], top_cxt->msg);
        }
    }
}
