/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Functions to estimate the bit cost of Huffman trees. */

#include "brotli_enc_bit_cost.h"

#include "brotli_common_constants.h"
#include "brotli_common_platform.h"
#include "brotli_types.h"
#include "brotli_enc_fast_log.h"
#include "brotli_enc_histogram.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define FN(X) X ## Literal
#include "brotli_enc_bit_cost_inc.h"  /* NOLINT(build/include) */
#undef FN

#define FN(X) X ## Command
#include "brotli_enc_bit_cost_inc.h"  /* NOLINT(build/include) */
#undef FN

#define FN(X) X ## Distance
#include "brotli_enc_bit_cost_inc.h"  /* NOLINT(build/include) */
#undef FN

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif
