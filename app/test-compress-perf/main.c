/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_compressdev.h>

#include "comp_perf_options.h"
#include "comp_perf_test_verify.h"
#include "comp_perf_test_benchmark.h"

#define NUM_MAX_XFORMS 16
#define NUM_MAX_INFLIGHT_OPS 512

#define DIV_CEIL(a, b)  ((a) / (b) + ((a) % (b) != 0))

/* Cleanup state machine */
static enum cleanup_st {
	ST_CLEAR = 0,
	ST_TEST_DATA,
	ST_COMPDEV,
	ST_INPUT_DATA,
	ST_MEMORY_ALLOC,
	ST_PREPARE_BUF,
	ST_DURING_TEST
} cleanup = ST_CLEAR;

static int
param_range_check(uint16_t size, const struct rte_param_log2_range *range)
{
	unsigned int next_size;

	/* Check lower/upper bounds */
	if (size < range->min)
		return -1;

	if (size > range->max)
		return -1;

	/* If range is actually only one value, size is correct */
	if (range->increment == 0)
		return 0;

	/* Check if value is one of the supported sizes */
	for (next_size = range->min; next_size <= range->max;
			next_size += range->increment)
		if (size == next_size)
			return 0;

	return -1;
}

static int
comp_perf_check_capabilities(struct comp_test_data *test_data)
{
	const struct rte_compressdev_capabilities *cap;

	cap = rte_compressdev_capability_get(test_data->cdev_id,
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
	if (test_data->level.min == 0 &&
			(comp_flags & RTE_COMP_FF_NONCOMPRESSED_BLOCKS) == 0) {
		RTE_LOG(ERR, USER1, "Compress device does not support "
				"level 0 (no compression)\n");
		return -1;
	}

	return 0;
}

static uint32_t
find_buf_size(uint32_t input_size)
{
	uint32_t i;

	/* From performance point of view the buffer size should be a
	 * power of 2 but also should be enough to store incompressible data
	 */

	/* We're looking for nearest power of 2 buffer size, which is greather
	 * than input_size
	 */
	uint32_t size =
		!input_size ? MIN_COMPRESSED_BUF_SIZE : (input_size << 1);

	for (i = UINT16_MAX + 1; !(i & size); i >>= 1)
		;

	return i > ((UINT16_MAX + 1) >> 1)
			? (uint32_t)((float)input_size * EXPANSE_RATIO)
			: i;
}

static int
comp_perf_allocate_memory(struct comp_test_data *test_data)
{

	test_data->out_seg_sz = find_buf_size(test_data->seg_sz);
	/* Number of segments for input and output
	 * (compression and decompression)
	 */
	uint32_t total_segs = DIV_CEIL(test_data->input_data_sz,
			test_data->seg_sz);
	test_data->comp_buf_pool = rte_pktmbuf_pool_create("comp_buf_pool",
				total_segs,
				0, 0,
				test_data->out_seg_sz + RTE_PKTMBUF_HEADROOM,
				rte_socket_id());
	if (test_data->comp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	cleanup = ST_MEMORY_ALLOC;
	test_data->decomp_buf_pool = rte_pktmbuf_pool_create("decomp_buf_pool",
				total_segs,
				0, 0, test_data->seg_sz + RTE_PKTMBUF_HEADROOM,
				rte_socket_id());
	if (test_data->decomp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	test_data->total_bufs = DIV_CEIL(total_segs, test_data->max_sgl_segs);

	test_data->op_pool = rte_comp_op_pool_create("op_pool",
				  test_data->total_bufs,
				  0, 0, rte_socket_id());
	if (test_data->op_pool == NULL) {
		RTE_LOG(ERR, USER1, "Comp op mempool could not be created\n");
		return -1;
	}

	/*
	 * Compressed data might be a bit larger than input data,
	 * if data cannot be compressed
	 */
	test_data->compressed_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz * EXPANSE_RATIO
						+ MIN_COMPRESSED_BUF_SIZE, 0,
				rte_socket_id());
	if (test_data->compressed_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		return -1;
	}

	test_data->decompressed_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz, 0,
				rte_socket_id());
	if (test_data->decompressed_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		return -1;
	}

	test_data->comp_bufs = rte_zmalloc_socket(NULL,
			test_data->total_bufs * sizeof(struct rte_mbuf *),
			0, rte_socket_id());
	if (test_data->comp_bufs == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the compression mbufs"
				" could not be allocated\n");
		return -1;
	}

	test_data->decomp_bufs = rte_zmalloc_socket(NULL,
			test_data->total_bufs * sizeof(struct rte_mbuf *),
			0, rte_socket_id());
	if (test_data->decomp_bufs == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the decompression mbufs"
				" could not be allocated\n");
		return -1;
	}
	return 0;
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

	if (fseek(f, 0, SEEK_SET) != 0) {
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

static int
comp_perf_initialize_compressdev(struct comp_test_data *test_data)
{
	uint8_t enabled_cdev_count;
	uint8_t enabled_cdevs[RTE_COMPRESS_MAX_DEVS];

	enabled_cdev_count = rte_compressdev_devices_get(test_data->driver_name,
			enabled_cdevs, RTE_COMPRESS_MAX_DEVS);
	if (enabled_cdev_count == 0) {
		RTE_LOG(ERR, USER1, "No compress devices type %s available\n",
				test_data->driver_name);
		return -EINVAL;
	}

	if (enabled_cdev_count > 1)
		RTE_LOG(INFO, USER1,
			"Only the first compress device will be used\n");

	test_data->cdev_id = enabled_cdevs[0];

	if (comp_perf_check_capabilities(test_data) < 0)
		return -1;

	/* Configure compressdev (one device, one queue pair) */
	struct rte_compressdev_config config = {
		.socket_id = rte_socket_id(),
		.nb_queue_pairs = 1,
		.max_nb_priv_xforms = NUM_MAX_XFORMS,
		.max_nb_streams = 0
	};

	if (rte_compressdev_configure(test_data->cdev_id, &config) < 0) {
		RTE_LOG(ERR, USER1, "Device configuration failed\n");
		return -1;
	}

	if (rte_compressdev_queue_pair_setup(test_data->cdev_id, 0,
			NUM_MAX_INFLIGHT_OPS, rte_socket_id()) < 0) {
		RTE_LOG(ERR, USER1, "Queue pair setup failed\n");
		return -1;
	}

	if (rte_compressdev_start(test_data->cdev_id) < 0) {
		RTE_LOG(ERR, USER1, "Device could not be started\n");
		return -1;
	}

	return 0;
}

static int
prepare_bufs(struct comp_test_data *test_data)
{
	uint32_t remaining_data = test_data->input_data_sz;
	uint8_t *input_data_ptr = test_data->input_data;
	size_t data_sz;
	uint8_t *data_addr;
	uint32_t i, j;

	for (i = 0; i < test_data->total_bufs; i++) {
		/* Allocate data in input mbuf and copy data from input file */
		test_data->decomp_bufs[i] =
			rte_pktmbuf_alloc(test_data->decomp_buf_pool);
		if (test_data->decomp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}

		cleanup = ST_PREPARE_BUF;
		data_sz = RTE_MIN(remaining_data, test_data->seg_sz);
		data_addr = (uint8_t *) rte_pktmbuf_append(
					test_data->decomp_bufs[i], data_sz);
		if (data_addr == NULL) {
			RTE_LOG(ERR, USER1, "Could not append data\n");
			return -1;
		}
		rte_memcpy(data_addr, input_data_ptr, data_sz);

		input_data_ptr += data_sz;
		remaining_data -= data_sz;

		/* Already one segment in the mbuf */
		uint16_t segs_per_mbuf = 1;

		/* Chain mbufs if needed for input mbufs */
		while (segs_per_mbuf < test_data->max_sgl_segs
				&& remaining_data > 0) {
			struct rte_mbuf *next_seg =
				rte_pktmbuf_alloc(test_data->decomp_buf_pool);

			if (next_seg == NULL) {
				RTE_LOG(ERR, USER1,
					"Could not allocate mbuf\n");
				return -1;
			}

			data_sz = RTE_MIN(remaining_data, test_data->seg_sz);
			data_addr = (uint8_t *)rte_pktmbuf_append(next_seg,
				data_sz);

			if (data_addr == NULL) {
				RTE_LOG(ERR, USER1, "Could not append data\n");
				return -1;
			}

			rte_memcpy(data_addr, input_data_ptr, data_sz);
			input_data_ptr += data_sz;
			remaining_data -= data_sz;

			if (rte_pktmbuf_chain(test_data->decomp_bufs[i],
					next_seg) < 0) {
				RTE_LOG(ERR, USER1, "Could not chain mbufs\n");
				return -1;
			}
			segs_per_mbuf++;
		}

		/* Allocate data in output mbuf */
		test_data->comp_bufs[i] =
			rte_pktmbuf_alloc(test_data->comp_buf_pool);
		if (test_data->comp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}
		data_addr = (uint8_t *) rte_pktmbuf_append(
					test_data->comp_bufs[i],
					test_data->out_seg_sz);
		if (data_addr == NULL) {
			RTE_LOG(ERR, USER1, "Could not append data\n");
			return -1;
		}

		/* Chain mbufs if needed for output mbufs */
		for (j = 1; j < segs_per_mbuf; j++) {
			struct rte_mbuf *next_seg =
				rte_pktmbuf_alloc(test_data->comp_buf_pool);

			if (next_seg == NULL) {
				RTE_LOG(ERR, USER1,
					"Could not allocate mbuf\n");
				return -1;
			}

			data_addr = (uint8_t *)rte_pktmbuf_append(next_seg,
				test_data->out_seg_sz);

			if (data_addr == NULL) {
				RTE_LOG(ERR, USER1, "Could not append data\n");
				return -1;
			}

			if (rte_pktmbuf_chain(test_data->comp_bufs[i],
					next_seg) < 0) {
				RTE_LOG(ERR, USER1, "Could not chain mbufs\n");
				return -1;
			}
		}
	}

	return 0;
}

static void
free_bufs(struct comp_test_data *test_data)
{
	uint32_t i;

	for (i = 0; i < test_data->total_bufs; i++) {
		rte_pktmbuf_free(test_data->comp_bufs[i]);
		rte_pktmbuf_free(test_data->decomp_bufs[i]);
	}
}



int
main(int argc, char **argv)
{
	uint8_t level, level_idx = 0;
	int ret, i;
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

	ret = EXIT_SUCCESS;
	cleanup = ST_TEST_DATA;
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

	if (comp_perf_initialize_compressdev(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	cleanup = ST_COMPDEV;
	if (comp_perf_dump_input_data(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	cleanup = ST_INPUT_DATA;
	if (comp_perf_allocate_memory(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (prepare_bufs(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (test_data->level.inc != 0)
		level = test_data->level.min;
	else
		level = test_data->level.list[0];

	printf("Burst size = %u\n", test_data->burst_sz);
	printf("File size = %zu\n", test_data->input_data_sz);

	printf("%6s%12s%17s%19s%21s%15s%21s%23s%16s\n",
		"Level", "Comp size", "Comp ratio [%]",
		"Comp [Cycles/it]", "Comp [Cycles/Byte]", "Comp [Gbps]",
		"Decomp [Cycles/it]", "Decomp [Cycles/Byte]", "Decomp [Gbps]");

	cleanup = ST_DURING_TEST;
	while (level <= test_data->level.max) {

		/*
		 * Run a first iteration, to verify compression and
		 * get the compression ratio for the level
		 */
		if (cperf_verification(test_data, level) != EXIT_SUCCESS)
			break;

		/*
		 * Run benchmarking test
		 */
		if (cperf_benchmark(test_data, level) != EXIT_SUCCESS)
			break;

		printf("%6u%12zu%17.2f%19"PRIu64"%21.2f"
					"%15.2f%21"PRIu64"%23.2f%16.2f\n",
		       level, test_data->comp_data_sz, test_data->ratio,
		       test_data->comp_tsc_duration[level],
		       test_data->comp_tsc_byte, test_data->comp_gbps,
		       test_data->decomp_tsc_duration[level],
		       test_data->decomp_tsc_byte, test_data->decomp_gbps);

		if (test_data->level.inc != 0)
			level += test_data->level.inc;
		else {
			if (++level_idx == test_data->level.count)
				break;
			level = test_data->level.list[level_idx];
		}
	}

end:
	switch (cleanup) {

	case ST_DURING_TEST:
	case ST_PREPARE_BUF:
		free_bufs(test_data);
		/* fallthrough */
	case ST_MEMORY_ALLOC:
		rte_free(test_data->decomp_bufs);
		rte_free(test_data->comp_bufs);
		rte_free(test_data->decompressed_data);
		rte_free(test_data->compressed_data);
		rte_mempool_free(test_data->op_pool);
		rte_mempool_free(test_data->decomp_buf_pool);
		rte_mempool_free(test_data->comp_buf_pool);
		/* fallthrough */
	case ST_INPUT_DATA:
		rte_free(test_data->input_data);
		/* fallthrough */
	case ST_COMPDEV:
		if (test_data->cdev_id != -1)
			rte_compressdev_stop(test_data->cdev_id);
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
