/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include "test.h"

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_reciprocal.h>

#define MAX_ITERATIONS	(1ULL << 32)
#define DIVIDE_ITER	(100)

static int
test_reciprocal(void)
{
	int result = 0;
	uint32_t divisor_u32 = 0;
	uint32_t dividend_u32;
	uint32_t nresult_u32;
	uint32_t rresult_u32;
	uint64_t i, j;
	uint64_t divisor_u64 = 0;
	uint64_t dividend_u64;
	uint64_t nresult_u64;
	uint64_t rresult_u64;
	struct rte_reciprocal reci_u32 = {0};
	struct rte_reciprocal_u64 reci_u64 = {0};

	rte_srand(rte_rdtsc());
	printf("Validating unsigned 32bit division.\n");
	for (i = 0; i < MAX_ITERATIONS; i++) {
		/* Change divisor every DIVIDE_ITER iterations. */
		if (i % DIVIDE_ITER == 0) {
			divisor_u32 = rte_rand();
			reci_u32 = rte_reciprocal_value(divisor_u32);
		}

		dividend_u32 = rte_rand();
		nresult_u32 = dividend_u32 / divisor_u32;
		rresult_u32 = rte_reciprocal_divide(dividend_u32,
				reci_u32);
		if (nresult_u32 != rresult_u32) {
			printf("Division failed, %"PRIu32"/%"PRIu32" = "
					"expected %"PRIu32" result %"PRIu32"\n",
					dividend_u32, divisor_u32,
					nresult_u32, rresult_u32);
			result = 1;
			break;
		}
	}

	printf("Validating unsigned 64bit division.\n");
	for (i = 0; i < MAX_ITERATIONS; i++) {
		/* Change divisor every DIVIDE_ITER iterations. */
		if (i % DIVIDE_ITER == 0) {
			divisor_u64 = rte_rand();
			reci_u64 = rte_reciprocal_value_u64(divisor_u64);
		}

		dividend_u64 = rte_rand();
		nresult_u64 = dividend_u64 / divisor_u64;
		rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
				&reci_u64);
		if (nresult_u64 != rresult_u64) {
			printf("Division failed,  %"PRIu64"/%"PRIu64" = "
					"expected %"PRIu64" result %"PRIu64"\n",
					dividend_u64, divisor_u64,
					nresult_u64, rresult_u64);
			result = 1;
			break;
		}
	}

	printf("Validating unsigned 64bit division with 32bit divisor.\n");
	for (i = 0; i < MAX_ITERATIONS; i++) {
		/* Change divisor every DIVIDE_ITER iterations. */
		if (i % DIVIDE_ITER == 0) {
			divisor_u64 = rte_rand() >> 32;
			reci_u64 = rte_reciprocal_value_u64(divisor_u64);
		}

		dividend_u64 = rte_rand();

		nresult_u64 = dividend_u64 / divisor_u64;
		rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
				&reci_u64);

		if (nresult_u64 != rresult_u64) {
			printf("Division failed, %"PRIu64"/%"PRIu64" = "
					"expected %"PRIu64" result %"PRIu64"\n",
					dividend_u64, divisor_u64,
					nresult_u64, rresult_u64);
			result = 1;
			break;
		}
	}

	printf("Validating division by power of 2.\n");
	for (i = 0; i < 32; i++) {
		divisor_u64 = 1ull << i;
		reci_u64 = rte_reciprocal_value_u64(divisor_u64);
		reci_u32 = rte_reciprocal_value((uint32_t)divisor_u64);

		for (j = 0; j < MAX_ITERATIONS >> 4; j++) {
			dividend_u64 = rte_rand();

			nresult_u64 = dividend_u64 / divisor_u64;
			rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
					&reci_u64);

			if (nresult_u64 != rresult_u64) {
				printf(
				"Division 64 failed, %"PRIu64"/%"PRIu64" = "
					"expected %"PRIu64" result %"PRIu64"\n",
						dividend_u64, divisor_u64,
						nresult_u64, rresult_u64);
				result = 1;
			}

			nresult_u32 = (dividend_u64 >> 32) / divisor_u64;
			rresult_u32 = rte_reciprocal_divide(
					(dividend_u64 >> 32), reci_u32);

			if (nresult_u32 != rresult_u32) {
				printf(
				"Division 32 failed, %"PRIu64"/%"PRIu64" = "
					"expected %"PRIu64" result %"PRIu64"\n",
						dividend_u64 >> 32, divisor_u64,
						nresult_u64, rresult_u64);
				result = 1;
				break;
			}
		}
	}

	for (; i < 64; i++) {
		divisor_u64 = 1ull << i;
		reci_u64 = rte_reciprocal_value_u64(divisor_u64);

		for (j = 0; j < MAX_ITERATIONS >> 4; j++) {
			dividend_u64 = rte_rand();

			nresult_u64 = dividend_u64 / divisor_u64;
			rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
					&reci_u64);

			if (nresult_u64 != rresult_u64) {
				printf("Division failed, %"PRIu64"/%"PRIu64" = "
					"expected %"PRIu64" result %"PRIu64"\n",
						dividend_u64, divisor_u64,
						nresult_u64, rresult_u64);
				result = 1;
				break;
			}
		}
	}

	return result;
}

REGISTER_TEST_COMMAND(reciprocal_division, test_reciprocal);
