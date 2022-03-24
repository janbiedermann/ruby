#include <unistd.h>
#include <string.h>
#include "ruby.h"
#include "frt_global.h"
#include "benchmark.h"
#include "benchmarks_all.h"
#include "word_list.h"

extern VALUE mFerret;
static VALUE mBenchmark;

#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64)

#define RUSAGE_SELF		0

struct rusage
{
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
};

int getrusage(int who, struct rusage *rusage)
{
	FILETIME	starttime;
	FILETIME	exittime;
	FILETIME	kerneltime;
	FILETIME	usertime;
	ULARGE_INTEGER li;

	if (who != RUSAGE_SELF) {
		errno = EINVAL;
		return -1;
	}

	if (rusage == (struct rusage *) NULL) {
		errno = EFAULT;
		return -1;
	}
	memset(rusage, 0, sizeof(struct rusage));
	if (GetProcessTimes(GetCurrentProcess(), &starttime, &exittime, &kerneltime, &usertime) == 0) {
		fprintf(stderr, "getrusage: error in GetProcessTimes %lu\n", GetLastError());
		return -1;
	}

	memcpy(&li, &kerneltime, sizeof(FILETIME));
	li.QuadPart /= 10L;
	rusage->ru_stime.tv_sec = li.QuadPart / 1000000L;
	rusage->ru_stime.tv_usec = li.QuadPart % 1000000L;

	memcpy(&li, &usertime, sizeof(FILETIME));
	li.QuadPart /= 10L;
	rusage->ru_utime.tv_sec = li.QuadPart / 1000000L;
	rusage->ru_utime.tv_usec = li.QuadPart % 1000000L;
    return 0;
}
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif

static int bmtcmp(const void *p1, const void *p2)
{
    BenchMarkTimes *bmt1 = *(BenchMarkTimes **)p1;
    BenchMarkTimes *bmt2 = *(BenchMarkTimes **)p2;

    if (bmt1->rtime > bmt2->rtime) return 1;
    else if (bmt1->rtime < bmt2->rtime) return -1;
    else return 0;
}

void bm_add(BenchMark *benchmark, bm_run_ft run, const char *name)
{
    BenchMarkUnit *unit =
        (BenchMarkUnit *)frt_emalloc(sizeof(BenchMarkUnit) +
                                 benchmark->count * sizeof(BenchMarkTimes *));
    int i;
    unit->name = frt_estrdup(name);
    unit->run = run;
    unit->next = NULL;
    if (benchmark->count > 1) {
        for (i = 0; i < benchmark->count; i++) {
            unit->times[i] = FRT_ALLOC(BenchMarkTimes);
        }
    }
    if (benchmark->tail) {
        benchmark->tail = benchmark->tail->next = unit;
    }
    else {
        benchmark->tail = benchmark->head = unit;
    }
}

static void bm_clear(BenchMark *benchmark)
{
    BenchMarkUnit *unit, *next = benchmark->head;
    while (NULL != (unit = next)) {
        next = unit->next;
        if (benchmark->count > 1) {
            int i;
            for (i = 0; i < benchmark->count; i++) {
                free(unit->times[i]);
            }
        }
        free(unit->name);
        free(unit);
    }
    benchmark->head = benchmark->tail = NULL;
}

#define TVAL_TO_SEC(before, after) \
  ((double)after.tv_sec  + ((double)after.tv_usec/1000000)) - \
  ((double)before.tv_sec + ((double)before.tv_usec/1000000))

static void bm_single_run(BenchMarkUnit *unit, BenchMarkTimes *bm_times)
{
    struct timeval tv_before, tv_after;
    struct rusage ru_before, ru_after;

    if (gettimeofday(&tv_before, NULL) == -1)
        FRT_RAISE(FRT_UNSUPPORTED_ERROR, "gettimeofday failed\n");
    getrusage(RUSAGE_SELF, &ru_before);

    unit->run();

    if (gettimeofday(&tv_after, NULL) == -1)
        FRT_RAISE(FRT_UNSUPPORTED_ERROR, "gettimeofday failed\n");
    getrusage(RUSAGE_SELF, &ru_after);

    bm_times->rtime = TVAL_TO_SEC(tv_before, tv_after);
    bm_times->utime = TVAL_TO_SEC(ru_before.ru_utime, ru_after.ru_utime);
    bm_times->stime = TVAL_TO_SEC(ru_before.ru_stime, ru_after.ru_stime);
}

#define DO_SETUP(bm) if (bm->setup) bm->setup();
#define DO_TEARDOWN(bm) if (bm->teardown) bm->teardown();

static void bm_run(BenchMark *benchmark)
{
    int i;
    BenchMarkUnit *unit;
    int max_name_len = 0;
    char fmt[40];
    int start = 0, end = benchmark->count;
    if (benchmark->discard) {
        start += benchmark->discard;
        end   -= benchmark->discard;
    }
    if (benchmark->count > 1) {
        for (i = 0; i < benchmark->count; i++) {
            DO_SETUP(benchmark);
            for (unit = benchmark->head; unit; unit = unit->next) {
                bm_single_run(unit, unit->times[i]);
            }
            DO_TEARDOWN(benchmark);
        }
        for (unit = benchmark->head; unit; unit = unit->next) {
            double rtime = 0.0, utime = 0.0, stime = 0.0;
            int result_count = end - start;
            /* we only need to sort if we are discarding outliers */
            if (benchmark->discard > 0) {
                qsort(unit->times, benchmark->count,
                      sizeof(BenchMarkTimes*), &bmtcmp);
            }

            for (i = start; i < end; i++) {
                rtime += unit->times[i]->rtime;
                utime += unit->times[i]->utime;
                stime += unit->times[i]->stime;
            }
            unit->final_times.utime = utime/result_count;
            unit->final_times.stime = stime/result_count;
            unit->final_times.rtime = rtime/result_count;
        }
    }
    else {
        DO_SETUP(benchmark);
        for (unit = benchmark->head; unit; unit = unit->next) {
            bm_single_run(unit, &(unit->final_times));
        }
        DO_TEARDOWN(benchmark);
    }

    /* get maximum unit name length for print out */
    for (unit = benchmark->head; unit; unit = unit->next) {
        int name_len = (int)strlen(unit->name);
        if (name_len > max_name_len) {
            max_name_len = name_len;
        }
    }

    for (i = 0; i < max_name_len; i++) putchar(' ');
    puts("\t         user     system       real");
    sprintf(fmt, "\t%%%ds %%10.6lf %%10.6lf %%10.6lf\n", max_name_len);
    for (unit = benchmark->head; unit; unit = unit->next) {
        printf(fmt,
               unit->name,
               unit->final_times.utime,
               unit->final_times.stime,
               unit->final_times.rtime);
    }
}

static VALUE frb_bm_run_all(VALUE v) {
    int i;
    BenchMark benchmark;
    benchmark.head = benchmark.tail = NULL;

    for (i = 0; i < FRT_NELEMS(all_benchmarks); i++) {
        printf("\nBenching [%s]...\n", all_benchmarks[i].name);
        benchmark.count = 1;
        benchmark.discard = 0;
        benchmark.setup = benchmark.teardown = NULL;
        all_benchmarks[i].initialize(&benchmark);
        bm_run(&benchmark);
        bm_clear(&benchmark);
    }
    return INT2FIX(0);
}

void Init_Benchmark(void) {
    mBenchmark = rb_define_module_under(mFerret, "Benchmark");
    rb_define_singleton_method(mBenchmark, "run_all",                     frb_bm_run_all, 0);
}
