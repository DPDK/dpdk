/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <rte_gpudev.h>

enum app_args {
	ARG_HELP,
	ARG_MEMPOOL
};

static void
usage(const char *prog_name)
{
	printf("%s [EAL options] --\n",
		prog_name);
}

static void
args_parse(int argc, char **argv)
{
	char **argvopt;
	int opt;
	int opt_idx;

	static struct option lgopts[] = {
		{ "help", 0, 0, ARG_HELP},
		/* End of options */
		{ 0, 0, 0, 0 }
	};

	argvopt = argv;
	while ((opt = getopt_long(argc, argvopt, "",
				lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case ARG_HELP:
			usage(argv[0]);
			break;
		default:
			usage(argv[0]);
			rte_exit(EXIT_FAILURE, "Invalid option: %s\n", argv[optind]);
			break;
		}
	}
}

int
main(int argc, char **argv)
{
	int ret;
	int nb_gpus = 0;
	int16_t gpu_id = 0;
	struct rte_gpu_info ginfo;

	/* Init EAL. */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed\n");
	argc -= ret;
	argv += ret;
	if (argc > 1)
		args_parse(argc, argv);
	argc -= ret;
	argv += ret;

	nb_gpus = rte_gpu_count_avail();
	printf("\n\nDPDK found %d GPUs:\n", nb_gpus);
	RTE_GPU_FOREACH(gpu_id)
	{
		if (rte_gpu_info_get(gpu_id, &ginfo))
			rte_exit(EXIT_FAILURE, "rte_gpu_info_get error - bye\n");

		printf("\tGPU ID %d\n\t\tparent ID %d GPU Bus ID %s NUMA node %d Tot memory %.02f MB, Tot processors %d\n",
				ginfo.dev_id,
				ginfo.parent,
				ginfo.name,
				ginfo.numa_node,
				(((float)ginfo.total_memory)/(float)1024)/(float)1024,
				ginfo.processor_count
			);
	}
	printf("\n\n");

	/* clean up the EAL */
	rte_eal_cleanup();
	printf("Bye...\n");

	return EXIT_SUCCESS;
}
