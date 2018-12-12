/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_compressdev.h>

#include "comp_perf_options.h"

int
main(int argc, char **argv)
{
	int ret;
	struct comp_test_data *test_data;

	/* Initialise DPDK EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments!\n");
	argc -= ret;
	argv += ret;

	test_data = rte_zmalloc_socket(NULL, sizeof(struct comp_test_data),
					0, rte_socket_id());

	if (test_data == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memory in socket %d\n",
				rte_socket_id());

	comp_perf_options_default(test_data);

	if (comp_perf_options_parse(test_data, argc, argv) < 0) {
		RTE_LOG(ERR, USER1,
			"Parsing one or more user options failed\n");
		ret = EXIT_FAILURE;
		goto err;
	}

	if (comp_perf_options_check(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto err;
	}

	ret = EXIT_SUCCESS;

err:
	rte_free(test_data);

	return ret;
}
