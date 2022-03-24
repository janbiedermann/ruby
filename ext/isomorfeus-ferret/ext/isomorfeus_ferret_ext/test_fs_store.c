#include "frt_store.h"
#include "test_store.h"
#include "test.h"

/**
 * Test a FileSystem store
 */
TestSuite *ts_fs_store(TestSuite *suite)
{

#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
    FrtStore *store = frt_open_fs_store(".\\test\\testdir\\store");
#else
    FrtStore *store = frt_open_fs_store("./test/testdir/store");
#endif
    store->clear(store);

    suite = ADD_SUITE(suite);

    create_test_store_suite(suite, store);

    frt_store_deref(store);

    return suite;
}
