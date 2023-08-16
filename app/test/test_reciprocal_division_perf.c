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
#define DIVIDE_ITER	(1ULL << 28)

static int
test_reciprocal_division_perf(void)
{
	int result = 0;
	uint32_t divisor_u32 = 0;
	uint32_t dividend_u32;
	uint64_t divisor_u64 = 0;
	uint64_t dividend_u64;
	volatile uint32_t nresult_u32;
	volatile uint32_t rresult_u32;
	volatile uint64_t nresult_u64;
	volatile uint64_t rresult_u64;
	uint64_t start_cyc;
	uint64_t split_cyc;
	uint64_t end_cyc;
	uint64_t tot_cyc_n = 0;
	uint64_t tot_cyc_r = 0;
	uint64_t i;
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

		start_cyc = rte_rdtsc();
		nresult_u32 = dividend_u32 / divisor_u32;
		split_cyc = rte_rdtsc();
		rresult_u32 = rte_reciprocal_divide(dividend_u32,
				reci_u32);
		end_cyc = rte_rdtsc();

		tot_cyc_n += split_cyc - start_cyc;
		tot_cyc_r += end_cyc - split_cyc;
		if (nresult_u32 != rresult_u32) {
			printf("Division failed, expected %"PRIu32" "
					"result %"PRIu32"",
					nresult_u32, rresult_u32);
			result = 1;
			break;
		}
	}
	printf("32bit Division results:\n");
	printf("Total number of cycles normal division     : %"PRIu64"\n",
			tot_cyc_n);
	printf("Total number of cycles reciprocal division : %"PRIu64"\n",
			tot_cyc_r);
	if (i != 0) {
		printf("Cycles per division(normal) : %3.2f\n",
				((double)tot_cyc_n)/i);
		printf("Cycles per division(reciprocal) : %3.2f\n\n",
				((double)tot_cyc_r)/i);
	}

	tot_cyc_n = 0;
	tot_cyc_r = 0;

	printf("Validating unsigned 64bit division.\n");
	for (i = 0; i < MAX_ITERATIONS; i++) {
		/* Change divisor every DIVIDE_ITER iterations. */
		if (i % DIVIDE_ITER == 0) {
			divisor_u64 = rte_rand();
			reci_u64 = rte_reciprocal_value_u64(divisor_u64);
		}

		dividend_u64 = rte_rand();

		start_cyc = rte_rdtsc();
		nresult_u64 = dividend_u64 / divisor_u64;
		split_cyc = rte_rdtsc();
		rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
				&reci_u64);
		end_cyc = rte_rdtsc();

		tot_cyc_n += split_cyc - start_cyc;
		tot_cyc_r += end_cyc - split_cyc;
		if (nresult_u64 != rresult_u64) {
			printf("Division failed, expected %"PRIu64" "
					"result %"PRIu64"",
					nresult_u64, rresult_u64);
			result = 1;
			break;
		}
	}
	printf("64bit Division results:\n");
	printf("Total number of cycles normal division     : %"PRIu64"\n",
			tot_cyc_n);
	printf("Total number of cycles reciprocal division : %"PRIu64"\n",
			tot_cyc_r);
	if (i != 0) {
		printf("Cycles per division(normal) : %3.2f\n",
				((double)tot_cyc_n)/i);
		printf("Cycles per division(reciprocal) : %3.2f\n\n",
				((double)tot_cyc_r)/i);
	}
	tot_cyc_n = 0;
	tot_cyc_r = 0;

	printf("Validating unsigned 64bit division with 32bit divisor.\n");
	for (i = 0; i < MAX_ITERATIONS; i++) {
		/* Change divisor every DIVIDE_ITER iterations. */
		if (i % DIVIDE_ITER == 0) {
			divisor_u64 = rte_rand() >> 32;
			reci_u64 = rte_reciprocal_value_u64(divisor_u64);
		}

		dividend_u64 = rte_rand();

		start_cyc = rte_rdtsc();
		nresult_u64 = dividend_u64 / divisor_u64;
		split_cyc = rte_rdtsc();
		rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
				&reci_u64);
		end_cyc = rte_rdtsc();

		tot_cyc_n += split_cyc - start_cyc;
		tot_cyc_r += end_cyc - split_cyc;
		if (nresult_u64 != rresult_u64) {
			printf("Division failed, expected %"PRIu64" "
					"result %"PRIu64"",
					nresult_u64, rresult_u64);
			result = 1;
			break;
		}
	}

	printf("64bit Division results:\n");
	printf("Total number of cycles normal division     : %"PRIu64"\n",
			tot_cyc_n);
	printf("Total number of cycles reciprocal division : %"PRIu64"\n",
			tot_cyc_r);
	if (i != 0) {
		printf("Cycles per division(normal) : %3.2f\n",
				((double)tot_cyc_n)/i);
		printf("Cycles per division(reciprocal) : %3.2f\n\n",
				((double)tot_cyc_r)/i);
	}

	tot_cyc_n = 0;
	tot_cyc_r = 0;

	printf("Validating division by power of 2.\n");
	for (i = 0; i < 64; i++) {
		divisor_u64 = 1ull << i;
		reci_u64 = rte_reciprocal_value_u64(divisor_u64);

		dividend_u64 = rte_rand();

		start_cyc = rte_rdtsc();
		nresult_u64 = dividend_u64 / divisor_u64;
		split_cyc = rte_rdtsc();
		rresult_u64 = rte_reciprocal_divide_u64(dividend_u64,
				&reci_u64);
		end_cyc = rte_rdtsc();

		tot_cyc_n += split_cyc - start_cyc;
		tot_cyc_r += end_cyc - split_cyc;
		if (nresult_u64 != rresult_u64) {
			printf("Division 64 failed, %"PRIu64"/%"PRIu64" = "
					"expected %"PRIu64" result %"PRIu64"\n",
					dividend_u64, divisor_u64,
					nresult_u64, rresult_u64);
			result = 1;
			break;
		}
	}
	printf("64bit Division results:\n");
	printf("Total number of cycles normal division     : %"PRIu64"\n",
			tot_cyc_n);
	printf("Total number of cycles reciprocal division : %"PRIu64"\n",
			tot_cyc_r);
	if (i != 0) {
		printf("Cycles per division(normal) : %3.2f\n",
				((double)tot_cyc_n)/i);
		printf("Cycles per division(reciprocal) : %3.2f\n",
				((double)tot_cyc_r)/i);
	}

	return result;
}

REGISTER_PERF_TEST(reciprocal_division_perf, test_reciprocal_division_perf);
