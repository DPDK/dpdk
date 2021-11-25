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

static int
alloc_gpu_memory(uint16_t gpu_id)
{
	void *ptr_1 = NULL;
	void *ptr_2 = NULL;
	size_t buf_bytes = 1024;
	int ret;

	printf("\n=======> TEST: Allocate GPU memory\n\n");

	/* Alloc memory on GPU 0 */
	ptr_1 = rte_gpu_mem_alloc(gpu_id, buf_bytes);
	if (ptr_1 == NULL) {
		fprintf(stderr, "rte_gpu_mem_alloc GPU memory returned error\n");
		goto error;
	}
	printf("GPU memory allocated at 0x%p size is %zd bytes\n",
			ptr_1, buf_bytes);

	ptr_2 = rte_gpu_mem_alloc(gpu_id, buf_bytes);
	if (ptr_2 == NULL) {
		fprintf(stderr, "rte_gpu_mem_alloc GPU memory returned error\n");
		goto error;
	}
	printf("GPU memory allocated at 0x%p size is %zd bytes\n",
			ptr_2, buf_bytes);

	ret = rte_gpu_mem_free(gpu_id, (uint8_t *)(ptr_1)+0x700);
	if (ret < 0) {
		printf("GPU memory 0x%p NOT freed: GPU driver didn't find this memory address internally.\n",
				(uint8_t *)(ptr_1)+0x700);
	} else {
		fprintf(stderr, "ERROR: rte_gpu_mem_free freed GPU memory 0x%p\n",
				(uint8_t *)(ptr_1)+0x700);
		goto error;
	}

	ret = rte_gpu_mem_free(gpu_id, ptr_2);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_mem_free returned error %d\n", ret);
		goto error;
	}
	printf("GPU memory 0x%p freed\n", ptr_2);

	ret = rte_gpu_mem_free(gpu_id, ptr_1);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_mem_free returned error %d\n", ret);
		goto error;
	}
	printf("GPU memory 0x%p freed\n", ptr_1);

	printf("\n=======> TEST: PASSED\n");
	return 0;

error:

	rte_gpu_mem_free(gpu_id, ptr_1);
	rte_gpu_mem_free(gpu_id, ptr_2);

	printf("\n=======> TEST: FAILED\n");
	return -1;
}

static int
register_cpu_memory(uint16_t gpu_id)
{
	void *ptr = NULL;
	size_t buf_bytes = 1024;
	int ret;

	printf("\n=======> TEST: Register CPU memory\n\n");

	/* Alloc memory on CPU visible from GPU 0 */
	ptr = rte_zmalloc(NULL, buf_bytes, 0);
	if (ptr == NULL) {
		fprintf(stderr, "Failed to allocate CPU memory.\n");
		goto error;
	}

	ret = rte_gpu_mem_register(gpu_id, buf_bytes, ptr);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_mem_register CPU memory returned error %d\n", ret);
		goto error;
	}
	printf("CPU memory registered at 0x%p %zdB\n", ptr, buf_bytes);

	ret = rte_gpu_mem_unregister(gpu_id, (uint8_t *)(ptr)+0x700);
	if (ret < 0) {
		printf("CPU memory 0x%p NOT unregistered: GPU driver didn't find this memory address internally\n",
				(uint8_t *)(ptr)+0x700);
	} else {
		fprintf(stderr, "ERROR: rte_gpu_mem_unregister unregistered GPU memory 0x%p\n",
				(uint8_t *)(ptr)+0x700);
		goto error;
	}

	ret = rte_gpu_mem_unregister(gpu_id, ptr);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_mem_unregister returned error %d\n", ret);
		goto error;
	}
	printf("CPU memory 0x%p unregistered\n", ptr);

	rte_free(ptr);

	printf("\n=======> TEST: PASSED\n");
	return 0;

error:

	rte_gpu_mem_unregister(gpu_id, ptr);
	rte_free(ptr);
	printf("\n=======> TEST: FAILED\n");
	return -1;
}

static int
create_update_comm_flag(uint16_t gpu_id)
{
	struct rte_gpu_comm_flag devflag;
	int ret = 0;
	uint32_t set_val;
	uint32_t get_val;

	printf("\n=======> TEST: Communication flag\n\n");

	ret = rte_gpu_comm_create_flag(gpu_id, &devflag, RTE_GPU_COMM_FLAG_CPU);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_create_flag returned error %d\n", ret);
		goto error;
	}

	set_val = 25;
	ret = rte_gpu_comm_set_flag(&devflag, set_val);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_set_flag returned error %d\n", ret);
		goto error;
	}

	ret = rte_gpu_comm_get_flag_value(&devflag, &get_val);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_get_flag_value returned error %d\n", ret);
		goto error;
	}

	printf("Communication flag value at 0x%p was set to %d and current value is %d\n",
			devflag.ptr, set_val, get_val);

	set_val = 38;
	ret = rte_gpu_comm_set_flag(&devflag, set_val);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_set_flag returned error %d\n", ret);
		goto error;
	}

	ret = rte_gpu_comm_get_flag_value(&devflag, &get_val);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_get_flag_value returned error %d\n", ret);
		goto error;
	}

	printf("Communication flag value at 0x%p was set to %d and current value is %d\n",
			devflag.ptr, set_val, get_val);

	ret = rte_gpu_comm_destroy_flag(&devflag);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_destroy_flags returned error %d\n", ret);
		goto error;
	}

	printf("\n=======> TEST: PASSED\n");
	return 0;

error:

	rte_gpu_comm_destroy_flag(&devflag);
	printf("\n=======> TEST: FAILED\n");
	return -1;
}

static int
simulate_gpu_task(struct rte_gpu_comm_list *comm_list_item, int num_pkts)
{
	int idx;

	if (comm_list_item == NULL)
		return -1;

	for (idx = 0; idx < num_pkts; idx++) {
		/**
		 * consume(comm_list_item->pkt_list[idx].addr);
		 */
	}
	comm_list_item->status = RTE_GPU_COMM_LIST_DONE;

	return 0;
}

static int
create_update_comm_list(uint16_t gpu_id)
{
	int ret = 0;
	int i = 0;
	struct rte_gpu_comm_list *comm_list = NULL;
	uint32_t num_comm_items = 1024;
	struct rte_mbuf *mbufs[10];

	printf("\n=======> TEST: Communication list\n\n");

	comm_list = rte_gpu_comm_create_list(gpu_id, num_comm_items);
	if (comm_list == NULL) {
		fprintf(stderr, "rte_gpu_comm_create_list returned error %d\n", ret);
		goto error;
	}

	/**
	 * Simulate DPDK receive functions like rte_eth_rx_burst()
	 */
	for (i = 0; i < 10; i++) {
		mbufs[i] = rte_zmalloc(NULL, sizeof(struct rte_mbuf), 0);
		if (mbufs[i] == NULL) {
			fprintf(stderr, "Failed to allocate fake mbufs in CPU memory.\n");
			goto error;
		}

		memset(mbufs[i], 0, sizeof(struct rte_mbuf));
	}

	/**
	 * Populate just the first item of  the list
	 */
	ret = rte_gpu_comm_populate_list_pkts(&(comm_list[0]), mbufs, 10);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_populate_list_pkts returned error %d\n", ret);
		goto error;
	}

	ret = rte_gpu_comm_cleanup_list(&(comm_list[0]));
	if (ret == 0) {
		fprintf(stderr, "rte_gpu_comm_cleanup_list erroneously cleaned the list even if packets have not been consumed yet\n");
		goto error;
	}
	printf("Communication list not cleaned because packets have not been consumed yet.\n");

	/**
	 * Simulate a GPU tasks going through the packet list to consume
	 * mbufs packets and release them
	 */
	printf("Consuming packets...\n");
	simulate_gpu_task(&(comm_list[0]), 10);

	/**
	 * Packets have been consumed, now the communication item
	 * and the related mbufs can be all released
	 */
	ret = rte_gpu_comm_cleanup_list(&(comm_list[0]));
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_cleanup_list returned error %d\n", ret);
		goto error;
	}

	printf("Communication list cleaned because packets have been consumed now.\n");

	ret = rte_gpu_comm_destroy_list(comm_list, num_comm_items);
	if (ret < 0) {
		fprintf(stderr, "rte_gpu_comm_destroy_list returned error %d\n", ret);
		goto error;
	}

	for (i = 0; i < 10; i++)
		rte_free(mbufs[i]);

	printf("\n=======> TEST: PASSED\n");
	return 0;

error:

	rte_gpu_comm_destroy_list(comm_list, num_comm_items);
	for (i = 0; i < 10; i++)
		rte_free(mbufs[i]);
	printf("\n=======> TEST: FAILED\n");
	return -1;
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

	if (nb_gpus == 0) {
		fprintf(stderr, "Need at least one GPU on the system to run the example\n");
		return EXIT_FAILURE;
	}

	gpu_id = 0;

	/**
	 * Memory tests
	 */
	alloc_gpu_memory(gpu_id);
	register_cpu_memory(gpu_id);

	/**
	 * Communication items test
	 */
	create_update_comm_flag(gpu_id);
	create_update_comm_list(gpu_id);

	/* clean up the EAL */
	rte_eal_cleanup();

	return EXIT_SUCCESS;
}
