/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "test_ring_stress.h"

static int
run_test(const struct test *test)
{
	int32_t rc;
	uint32_t i, k;

	for (i = 0, k = 0; i != test->nb_case; i++) {

		printf("TEST-CASE %s %s START\n",
			test->name, test->cases[i].name);

		rc = test->cases[i].func(test->cases[i].wfunc);
		k += (rc == 0);

		if (rc != 0)
			printf("TEST-CASE %s %s FAILED\n",
				test->name, test->cases[i].name);
		else
			printf("TEST-CASE %s %s OK\n",
				test->name, test->cases[i].name);
	}

	return k;
}

static int
test_ring_stress(void)
{
	uint32_t n, k;

	n = 0;
	k = 0;

	n += test_ring_mpmc_stress.nb_case;
	k += run_test(&test_ring_mpmc_stress);

	n += test_ring_rts_stress.nb_case;
	k += run_test(&test_ring_rts_stress);

	n += test_ring_hts_stress.nb_case;
	k += run_test(&test_ring_hts_stress);

	n += test_ring_mt_peek_stress.nb_case;
	k += run_test(&test_ring_mt_peek_stress);

	n += test_ring_st_peek_stress.nb_case;
	k += run_test(&test_ring_st_peek_stress);

	printf("Number of tests:\t%u\nSuccess:\t%u\nFailed:\t%u\n",
		n, k, n - k);
	return (k != n);
}

REGISTER_TEST_COMMAND(ring_stress_autotest, test_ring_stress);
