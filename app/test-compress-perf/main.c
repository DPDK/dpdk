/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_compressdev.h>

#include "comp_perf.h"
#include "comp_perf_options.h"
#include "comp_perf_test_common.h"
#include "comp_perf_test_cyclecount.h"
#include "comp_perf_test_throughput.h"
#include "comp_perf_test_verify.h"

#define NUM_MAX_XFORMS 16
#define NUM_MAX_INFLIGHT_OPS 512

__extension__
const char *comp_perf_test_type_strs[] = {
	[CPERF_TEST_TYPE_THROUGHPUT] = "throughput",
	[CPERF_TEST_TYPE_VERIFY] = "verify",
	[CPERF_TEST_TYPE_PMDCC] = "pmd-cyclecount"
};

__extension__
static const struct cperf_test cperf_testmap[] = {
	[CPERF_TEST_TYPE_THROUGHPUT] = {
			cperf_throughput_test_constructor,
			cperf_throughput_test_runner,
			cperf_throughput_test_destructor

	},
	[CPERF_TEST_TYPE_VERIFY] = {
			cperf_verify_test_constructor,
			cperf_verify_test_runner,
			cperf_verify_test_destructor
	},

	[CPERF_TEST_TYPE_PMDCC] = {
			cperf_cyclecount_test_constructor,
			cperf_cyclecount_test_runner,
			cperf_cyclecount_test_destructor
	}
};

static struct comp_test_data *test_data;

static int
comp_perf_check_capabilities(struct comp_test_data *test_data, uint8_t cdev_id)
{
	const struct rte_compressdev_capabilities *cap;

	cap = rte_compressdev_capability_get(cdev_id,
					     RTE_COMP_ALGO_DEFLATE);

	if (cap == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress device does not support DEFLATE\n");
		return -1;
	}

	uint64_t comp_flags = cap->comp_feature_flags;

	/* Huffman enconding */
	if (test_data->huffman_enc == RTE_COMP_HUFFMAN_FIXED &&
			(comp_flags & RTE_COMP_FF_HUFFMAN_FIXED) == 0) {
		RTE_LOG(ERR, USER1,
			"Compress device does not supported Fixed Huffman\n");
		return -1;
	}

	if (test_data->huffman_enc == RTE_COMP_HUFFMAN_DYNAMIC &&
			(comp_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0) {
		RTE_LOG(ERR, USER1,
			"Compress device does not supported Dynamic Huffman\n");
		return -1;
	}

	/* Window size */
	if (test_data->window_sz != -1) {
		if (param_range_check(test_data->window_sz, &cap->window_size)
				< 0) {
			RTE_LOG(ERR, USER1,
				"Compress device does not support "
				"this window size\n");
			return -1;
		}
	} else
		/* Set window size to PMD maximum if none was specified */
		test_data->window_sz = cap->window_size.max;

	/* Check if chained mbufs is supported */
	if (test_data->max_sgl_segs > 1  &&
			(comp_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0) {
		RTE_LOG(INFO, USER1, "Compress device does not support "
				"chained mbufs. Max SGL segments set to 1\n");
		test_data->max_sgl_segs = 1;
	}

	/* Level 0 support */
	if (test_data->level_lst.min == 0 &&
			(comp_flags & RTE_COMP_FF_NONCOMPRESSED_BLOCKS) == 0) {
		RTE_LOG(ERR, USER1, "Compress device does not support "
				"level 0 (no compression)\n");
		return -1;
	}

	return 0;
}

static int
comp_perf_initialize_compressdev(struct comp_test_data *test_data,
				 uint8_t *enabled_cdevs)
{
	uint8_t enabled_cdev_count, nb_lcores, cdev_id;
	unsigned int i, j;
	int ret;

	enabled_cdev_count = rte_compressdev_devices_get(test_data->driver_name,
			enabled_cdevs, RTE_COMPRESS_MAX_DEVS);
	if (enabled_cdev_count == 0) {
		RTE_LOG(ERR, USER1, "No compress devices type %s available,"
				    " please check the list of specified devices in EAL section\n",
				test_data->driver_name);
		return -EINVAL;
	}

	nb_lcores = rte_lcore_count() - 1;
	/*
	 * Use fewer devices,
	 * if there are more available than cores.
	 */
	if (enabled_cdev_count > nb_lcores) {
		if (nb_lcores == 0) {
			RTE_LOG(ERR, USER1, "Cannot run with 0 cores! Increase the number of cores\n");
			return -EINVAL;
		}
		enabled_cdev_count = nb_lcores;
		RTE_LOG(INFO, USER1,
			"There's more available devices than cores!"
			" The number of devices has been aligned to %d cores\n",
			nb_lcores);
	}

	/*
	 * Calculate number of needed queue pairs, based on the amount
	 * of available number of logical cores and compression devices.
	 * For instance, if there are 4 cores and 2 compression devices,
	 * 2 queue pairs will be set up per device.
	 * One queue pair per one core.
	 * if e.g.: there're 3 cores and 2 compression devices,
	 * 2 queue pairs will be set up per device but one queue pair
	 * will left unused in the last one device
	 */
	test_data->nb_qps = (nb_lcores % enabled_cdev_count) ?
				(nb_lcores / enabled_cdev_count) + 1 :
				nb_lcores / enabled_cdev_count;

	for (i = 0; i < enabled_cdev_count &&
			i < RTE_COMPRESS_MAX_DEVS; i++,
					nb_lcores -= test_data->nb_qps) {
		cdev_id = enabled_cdevs[i];

		struct rte_compressdev_info cdev_info;
		uint8_t socket_id = rte_compressdev_socket_id(cdev_id);

		rte_compressdev_info_get(cdev_id, &cdev_info);
		if (cdev_info.max_nb_queue_pairs &&
			test_data->nb_qps > cdev_info.max_nb_queue_pairs) {
			RTE_LOG(ERR, USER1,
				"Number of needed queue pairs is higher "
				"than the maximum number of queue pairs "
				"per device.\n");
			RTE_LOG(ERR, USER1,
				"Lower the number of cores or increase "
				"the number of crypto devices\n");
			return -EINVAL;
		}

		if (comp_perf_check_capabilities(test_data, cdev_id) < 0)
			return -EINVAL;

		/* Configure compressdev */
		struct rte_compressdev_config config = {
			.socket_id = socket_id,
			.nb_queue_pairs = nb_lcores > test_data->nb_qps
					? test_data->nb_qps : nb_lcores,
			.max_nb_priv_xforms = NUM_MAX_XFORMS,
			.max_nb_streams = 0
		};

		if (rte_compressdev_configure(cdev_id, &config) < 0) {
			RTE_LOG(ERR, USER1, "Device configuration failed\n");
			return -EINVAL;
		}

		for (j = 0; j < test_data->nb_qps; j++) {
			ret = rte_compressdev_queue_pair_setup(cdev_id, j,
					NUM_MAX_INFLIGHT_OPS, socket_id);
			if (ret < 0) {
				RTE_LOG(ERR, USER1,
			      "Failed to setup queue pair %u on compressdev %u",
					j, cdev_id);
				return -EINVAL;
			}
		}

		ret = rte_compressdev_start(cdev_id);
		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Failed to start device %u: error %d\n",
				cdev_id, ret);
			return -EPERM;
		}
	}

	return enabled_cdev_count;
}

static int
comp_perf_dump_input_data(struct comp_test_data *test_data)
{
	FILE *f = fopen(test_data->input_file, "r");
	int ret = -1;

	if (f == NULL) {
		RTE_LOG(ERR, USER1, "Input file could not be opened\n");
		return -1;
	}

	if (fseek(f, 0, SEEK_END) != 0) {
		RTE_LOG(ERR, USER1, "Size of input could not be calculated\n");
		goto end;
	}
	size_t actual_file_sz = ftell(f);
	/* If extended input data size has not been set,
	 * input data size = file size
	 */

	if (test_data->input_data_sz == 0)
		test_data->input_data_sz = actual_file_sz;

	if (test_data->input_data_sz <= 0 || actual_file_sz <= 0 ||
			fseek(f, 0, SEEK_SET) != 0) {
		RTE_LOG(ERR, USER1, "Size of input could not be calculated\n");
		goto end;
	}

	test_data->input_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz, 0, rte_socket_id());

	if (test_data->input_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		goto end;
	}

	size_t remaining_data = test_data->input_data_sz;
	uint8_t *data = test_data->input_data;

	while (remaining_data > 0) {
		size_t data_to_read = RTE_MIN(remaining_data, actual_file_sz);

		if (fread(data, data_to_read, 1, f) != 1) {
			RTE_LOG(ERR, USER1, "Input file could not be read\n");
			goto end;
		}
		if (fseek(f, 0, SEEK_SET) != 0) {
			RTE_LOG(ERR, USER1,
				"Size of input could not be calculated\n");
			goto end;
		}
		remaining_data -= data_to_read;
		data += data_to_read;
	}

	printf("\n");
	if (test_data->input_data_sz > actual_file_sz)
		RTE_LOG(INFO, USER1,
		  "%zu bytes read from file %s, extending the file %.2f times\n",
			test_data->input_data_sz, test_data->input_file,
			(double)test_data->input_data_sz/actual_file_sz);
	else
		RTE_LOG(INFO, USER1,
			"%zu bytes read from file %s\n",
			test_data->input_data_sz, test_data->input_file);

	ret = 0;

end:
	fclose(f);
	return ret;
}

static void
comp_perf_cleanup_on_signal(int signalNumber __rte_unused)
{
	test_data->perf_comp_force_stop = 1;
}

static void
comp_perf_register_cleanup_on_signal(void)
{
	signal(SIGTERM, comp_perf_cleanup_on_signal);
	signal(SIGINT, comp_perf_cleanup_on_signal);
}

int
main(int argc, char **argv)
{
	uint8_t level_idx = 0;
	int ret, i;
	void *ctx[RTE_MAX_LCORE] = {};
	uint8_t enabled_cdevs[RTE_COMPRESS_MAX_DEVS];
	int nb_compressdevs = 0;
	uint16_t total_nb_qps = 0;
	uint8_t cdev_id;
	uint32_t lcore_id;

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

	comp_perf_register_cleanup_on_signal();

	ret = EXIT_SUCCESS;
	test_data->cleanup = ST_TEST_DATA;
	comp_perf_options_default(test_data);

	if (comp_perf_options_parse(test_data, argc, argv) < 0) {
		RTE_LOG(ERR, USER1,
			"Parsing one or more user options failed\n");
		ret = EXIT_FAILURE;
		goto end;
	}

	if (comp_perf_options_check(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	nb_compressdevs =
		comp_perf_initialize_compressdev(test_data, enabled_cdevs);

	if (nb_compressdevs < 1) {
		ret = EXIT_FAILURE;
		goto end;
	}

	test_data->cleanup = ST_COMPDEV;
	if (comp_perf_dump_input_data(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	test_data->cleanup = ST_INPUT_DATA;

	if (test_data->level_lst.inc != 0)
		test_data->level = test_data->level_lst.min;
	else
		test_data->level = test_data->level_lst.list[0];

	printf("\nApp uses socket: %u\n", rte_socket_id());
	printf("Burst size = %u\n", test_data->burst_sz);
	printf("Input data size = %zu\n", test_data->input_data_sz);
	if (test_data->test == CPERF_TEST_TYPE_PMDCC)
		printf("Cycle-count delay = %u [us]\n",
		       test_data->cyclecount_delay);

	test_data->cleanup = ST_DURING_TEST;
	total_nb_qps = nb_compressdevs * test_data->nb_qps;

	i = 0;
	uint8_t qp_id = 0, cdev_index = 0;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {

		if (i == total_nb_qps)
			break;

		cdev_id = enabled_cdevs[cdev_index];
		ctx[i] = cperf_testmap[test_data->test].constructor(
							cdev_id, qp_id,
							test_data);
		if (ctx[i] == NULL) {
			RTE_LOG(ERR, USER1, "Test run constructor failed\n");
			goto end;
		}
		qp_id = (qp_id + 1) % test_data->nb_qps;
		if (qp_id == 0)
			cdev_index++;
		i++;
	}

	print_test_dynamics(test_data);

	while (test_data->level <= test_data->level_lst.max) {

		i = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {

			if (i == total_nb_qps)
				break;

			rte_eal_remote_launch(
					cperf_testmap[test_data->test].runner,
					ctx[i], lcore_id);
			i++;
		}
		i = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {

			if (i == total_nb_qps)
				break;
			ret |= rte_eal_wait_lcore(lcore_id);
			i++;
		}

		if (ret != EXIT_SUCCESS)
			break;

		if (test_data->level_lst.inc != 0)
			test_data->level += test_data->level_lst.inc;
		else {
			if (++level_idx == test_data->level_lst.count)
				break;
			test_data->level = test_data->level_lst.list[level_idx];
		}
	}

end:
	switch (test_data->cleanup) {

	case ST_DURING_TEST:
		i = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			if (i == total_nb_qps)
				break;

			if (ctx[i] && cperf_testmap[test_data->test].destructor)
				cperf_testmap[test_data->test].destructor(
									ctx[i]);
			i++;
		}
		/* fallthrough */
	case ST_INPUT_DATA:
		rte_free(test_data->input_data);
		/* fallthrough */
	case ST_COMPDEV:
		for (i = 0; i < nb_compressdevs &&
		     i < RTE_COMPRESS_MAX_DEVS; i++) {
			rte_compressdev_stop(enabled_cdevs[i]);
			rte_compressdev_close(enabled_cdevs[i]);
		}
		/* fallthrough */
	case ST_TEST_DATA:
		rte_free(test_data);
		/* fallthrough */
	case ST_CLEAR:
	default:
		i = rte_eal_cleanup();
		if (i) {
			RTE_LOG(ERR, USER1,
				"Error from rte_eal_cleanup(), %d\n", i);
			ret = i;
		}
		break;
	}
	return ret;
}

__rte_weak void *
cperf_cyclecount_test_constructor(uint8_t dev_id __rte_unused,
				 uint16_t qp_id __rte_unused,
				 struct comp_test_data *options __rte_unused)
{
	RTE_LOG(INFO, USER1, "Cycle count test is not supported yet\n");
	return NULL;
}

__rte_weak void
cperf_cyclecount_test_destructor(void *arg __rte_unused)
{
	RTE_LOG(INFO, USER1, "Something wrong happened!!!\n");
}

__rte_weak int
cperf_cyclecount_test_runner(void *test_ctx __rte_unused)
{
	return 0;
}

__rte_weak void *
cperf_throughput_test_constructor(uint8_t dev_id __rte_unused,
				 uint16_t qp_id __rte_unused,
				 struct comp_test_data *options __rte_unused)
{
	RTE_LOG(INFO, USER1, "Benchmark test is not supported yet\n");
	return NULL;
}

__rte_weak void
cperf_throughput_test_destructor(void *arg __rte_unused)
{

}

__rte_weak int
cperf_throughput_test_runner(void *test_ctx __rte_unused)
{
	return 0;
}
__rte_weak void *
cperf_verify_test_constructor(uint8_t dev_id __rte_unused,
				 uint16_t qp_id __rte_unused,
				 struct comp_test_data *options __rte_unused)
{
	RTE_LOG(INFO, USER1, "Verify test is not supported yet\n");
	return NULL;
}

__rte_weak void
cperf_verify_test_destructor(void *arg __rte_unused)
{

}

__rte_weak int
cperf_verify_test_runner(void *test_ctx __rte_unused)
{
	return 0;
}
