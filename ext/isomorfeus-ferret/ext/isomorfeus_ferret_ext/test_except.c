#include "frt_except.h"
#include "test.h"
#include "fio_tmpfile.h"

static const char *msg1 = "Message One";
static const char *msg2 = "Message Two";

static void raise_exception()
{
    FRT_RAISE(FRT_EXCEPTION, "%s", msg1);
}

static void inner_try(TestCase *tc)
{
    volatile bool exception_handled = false;
    volatile bool ioerror_called = false;

    FRT_TRY
        raise_exception();
        Assert(false, "Exception should have been raised");
        break;
    case FRT_EXCEPTION:
        /* This should be called */
        Astrstr(xcontext.msg, msg1);
        exception_handled = true;
        FRT_HANDLED();
        FRT_RAISE(FRT_IO_ERROR, "%s", msg2);
        Assert(false, "Exception should have been raised");
        break;
    case FRT_IO_ERROR:
        ioerror_called = true;
        break;
    default:
        Assert(false, "Exception should have been known");
        break;
    case FRT_FINALLY:
        Assert(exception_handled, "%s", "Exception wasn't handled");
        Assert(ioerror_called, "IO_ERROR wasn't called");
    FRT_ENDTRY
}

static void test_nested_except(TestCase *tc, void *data)
{
    volatile bool ioerror_handled = false;
    bool finally_handled = false;
    (void)data;

    FRT_TRY
        inner_try(tc);
        Assert(false, "Exception should have been raised");
        break;
    case FRT_IO_ERROR:
        /* This should be called */
        Astrstr(xcontext.msg, msg2);
        ioerror_handled = true;
        FRT_HANDLED();
        break;
    case FRT_EXCEPTION:
        /* This should be called */
        Assert(false, "Exception should not have been raised");
        break;
    default:
        Assert(false, "Exception should have been known");
        break;
    case FRT_FINALLY:
        finally_handled = true;
    FRT_ENDTRY
    Assert(ioerror_handled, "Exception wasn't handled");
    Assert(finally_handled, "Finally wasn't handled");
}

static void test_function_except(TestCase *tc, void *data)
{
    volatile bool exception_handled = false;
    bool finally_handled = false;
    (void)data; /* suppress warning */

    FRT_TRY
        raise_exception();
        Assert(false, "Exception should have been raised");
        break;
    case FRT_EXCEPTION:
        /* This should be called */
        Astrstr(xcontext.msg, msg1);
#if defined(__func__) && defined(FRT_HAS_VARARGS)
        Astrstr("raise_exception", msg1);
#endif
        exception_handled = true;
        FRT_HANDLED();
        break;
    default:
        Assert(false, "Exception should have been known");
        break;
    case FRT_FINALLY:
        finally_handled = true;
    FRT_ENDTRY
    Assert(exception_handled, "Exception wasn't handled");
    Assert(finally_handled, "Finally wasn't handled");
}

static void test_simple_except(TestCase *tc, void *data)
{
    volatile bool exception_handled = false;
    bool finally_handled = false;
    (void)data; /* suppress warning */

    FRT_TRY
        FRT_RAISE(FRT_EXCEPTION, "error message %s %d", "string", 20);
        Assert(false, "Exception should have been raised");
        break;
    case FRT_EXCEPTION:
        /* This should be called */
        Astrstr(xcontext.msg, "error message string 20");
#if defined(__func__) && defined(FRT_HAS_VARARGS)
        Astrstr(xcontext.msg, __func__);
#endif
#if defined(FRT_HAS_VARARGS)
        Astrstr(xcontext.msg, __FILE__);
#endif
        exception_handled = true;
        FRT_HANDLED();
        break;
    default:
        Assert(false, "Exception should have been known");
        break;
    case FRT_FINALLY:
        finally_handled = true;
    FRT_ENDTRY
    Assert(exception_handled, "Exception wasn't handled");
    Assert(finally_handled, "Finally wasn't handled");
}

static void try_xfinally1(TestCase *tc)
{
    bool finally_handled = false;

    FRT_TRY
        Assert(true, "No exception raised");
    FRT_XFINALLY
        FRT_RAISE(EXCEPTION, "%s", msg1);
        finally_handled = true;
    FRT_XENDTRY
    Assert(finally_handled, "Finally wasn't handled");
    Atrue(finally_handled);
}

static void try_xfinally2(TestCase *tc)
{
    bool finally_handled = false;

    FRT_TRY
        FRT_RAISE(EXCEPTION, "%s", msg1);
        Assert(false, "Exception should have been raised");
    FRT_XFINALLY
        FRT_RAISE(EXCEPTION, "%s", msg1);
        finally_handled = true;
    FRT_XENDTRY
    Assert(finally_handled, "Finally wasn't handled");
    Atrue(finally_handled);
}

static void try_xcatchall(TestCase *tc)
{
    bool catchall_handled = false;

    FRT_TRY
        FRT_RAISE(EXCEPTION, "%s", msg1);
        Assert(false, "Exception should have been raised");
    FRT_XCATCHALL
        FRT_HANDLED();
        FRT_RAISE(EXCEPTION, "%s", msg1);
        catchall_handled = true;
    FRT_XENDTRY
    Assert(catchall_handled, "Finally wasn't handled");
    Atrue(catchall_handled);
}

static void test_xfinally(TestCase *tc, void *data)
{
    volatile bool exception_handled = false;
    bool finally_handled = false;
    (void)data; /* suppress warning */

    FRT_TRY
        try_xfinally1(tc);
        try_xfinally2(tc);
        try_xcatchall(tc);
        Assert(false, "Exception should have been raised");
        break;
    case FRT_EXCEPTION:
        /* This should be called */
        Astrstr(xcontext.msg, msg1);
        exception_handled = true;
        FRT_HANDLED();
        break;
    default:
        Assert(false, "Exception should have been known");
        break;
    case FRT_FINALLY:
        finally_handled = true;
    FRT_ENDTRY
    Assert(exception_handled, "Exception wasn't handled");
    Assert(finally_handled, "Finally wasn't handled");
}

static void test_uncaught_except(TestCase *tc, void *data)
{
    bool old_abort_setting = frt_x_abort_on_exception;
    FILE *old_stream_setting = frt_x_exception_stream;
    int tfd = fio_tmpfile();
    FILE *exception_output = fdopen(tfd, "w+");
    (void)data, (void)tc; /* suppress warning */


    frt_x_abort_on_exception = false;
    frt_x_exception_stream = exception_output;

    /* Unhandled exception in try block */
    FRT_TRY
        raise_exception();
    FRT_ENDTRY
    Assert(frt_x_has_aborted, "Unhandled exception in try block didn't abort");

    /* Unhandled exception outside of try block */
    frt_x_has_aborted = false;
    FRT_RAISE(FRT_EXCEPTION, "%s:", msg1);
    Assert(frt_x_has_aborted, "Unhandled exception didn't cause an abort");

    frt_x_abort_on_exception = old_abort_setting;
    frt_x_exception_stream = old_stream_setting;
    fclose(exception_output);
}

TestSuite *ts_except(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_simple_except, NULL);
    tst_run_test(suite, test_function_except, NULL);
    tst_run_test(suite, test_nested_except, NULL);
    tst_run_test(suite, test_xfinally, NULL);
    tst_run_test(suite, test_uncaught_except, NULL);
    return suite;
}
