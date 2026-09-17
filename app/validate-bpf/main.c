/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

#include <rte_bpf.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_errno.h>

RTE_LOG_REGISTER(validate_bpf_logtype, validate-bpf, NOTICE);

static int
test_bpf_load(struct rte_bpf_prm_ex *prm)
{
	struct rte_bpf * const bpf = rte_bpf_load_ex(prm);

	const int rc = -rte_errno;

	rte_bpf_destroy(bpf);

	return bpf == NULL ? rc : 0;
}

/* Re-starts validation of asked from interactive debugger. */
static int
test_bpf_load_with_restarts(struct rte_bpf_prm_ex *prm)
{
	for (;;) {
		const int rc = test_bpf_load(prm);

		if (rc == -ECANCELED && debug_validate_again())
			continue;

		if (rc != 0)
			fprintf(stderr, "Error %d loading BPF: %s\n",
				-rc, strerror(-rc));
		else
			fprintf(stderr, "Validation succeeded.\n");

		return rc;
	}
}

int
main(int argc, char *argv[])
{
	int rc;

	struct args * const args = args_parse(argc, argv);
	if (args == NULL || args->show_help
			|| args->bpf_prm.elf_file.path == NULL) {
		args_destroy(args);
		print_usage(argv[0]);
		if (args == NULL)
			/* Could not parse arguments. */
			return 2;
		print_supported_types();
		print_defaults();
		return 0;
	}

	const int eal_init_argc = get_eal_init_argc();
	char ** const eal_init_argv = get_eal_init_argv(argv[0]);
	rc = rte_eal_init(eal_init_argc, eal_init_argv);
	if (rc < 0) {
		fprintf(stderr, "Error %d initializing EAL: %s\n",
			rte_errno, strerror(rte_errno));
		args_destroy(args);
		return EXIT_FAILURE;
	}
	RTE_VERIFY(rc == eal_init_argc - 1);

	const int ret = test_bpf_load_with_restarts(&args->bpf_prm);

	args_destroy(args);

	rc = rte_eal_cleanup();
	if (rc < 0) {
		fprintf(stderr, "Error %d cleaning up EAL: %s\n",
			rte_errno, strerror(rte_errno));
		return EXIT_FAILURE;
	}

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
