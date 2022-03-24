#include <string.h>
#include "frt_global.h"
#include "frt_index.h"
#include "frt_array.h"
#include "frt_helper.h"

/****************************************************************************
 *
 * TermVector
 *
 ****************************************************************************/

void frt_tv_destroy(FrtTermVector *tv)
{
    int i = tv->term_cnt;
    while (i > 0) {
        i--;
        free(tv->terms[i].text);
        free(tv->terms[i].positions);
    }
    free(tv->offsets);
    free(tv->terms);
    free(tv);
}

int frt_tv_scan_to_term_index(FrtTermVector *tv, const char *term)
{
    int lo = 0;                 /* search starts array */
    int hi = tv->term_cnt - 1;  /* for 1st element < n, return its index */
    int mid;
    int cmp;
    char *mid_term;

    while (hi >= lo) {
        mid = (lo + hi) >> 1;
        mid_term = tv->terms[mid].text;
        cmp = strcmp(term, mid_term);
        if (cmp < 0) {
            hi = mid - 1;
        }
        else if (cmp > 0) {
            lo = mid + 1;
        }
        else {                  /* found a match */
            return mid;
        }
    }
    return lo;
}

int frt_tv_get_term_index(FrtTermVector *tv, const char *term)
{
    int index = frt_tv_scan_to_term_index(tv, term);
    if (index < tv->term_cnt && (0 == strcmp(term, tv->terms[index].text))) {
        /* found term */
        return index;
    }
    else {
        return -1;
    }
}

FrtTVTerm *frt_tv_get_tv_term(FrtTermVector *tv, const char *term)
{
    int index = frt_tv_get_term_index(tv, term);
    if (index >= 0) {
        return &(tv->terms[index]);
    }
    else {
        return NULL;
    }
}
