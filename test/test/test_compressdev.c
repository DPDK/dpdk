/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#include <string.h>
#include <zlib.h>
#include <math.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_compressdev.h>

#include "test_compressdev_test_buffer.h"
#include "test.h"

#define DEFAULT_WINDOW_SIZE 15
#define DEFAULT_MEM_LEVEL 8
#define MAX_DEQD_RETRIES 10
#define DEQUEUE_WAIT_TIME 10000

/*
 * 30% extra size for compressed data compared to original data,
 * in case data size cannot be reduced and it is actually bigger
 * due to the compress block headers
 */
#define COMPRESS_BUF_SIZE_RATIO 1.3
#define NUM_MBUFS 16
#define NUM_OPS 16
#define NUM_MAX_XFORMS 16
#define NUM_MAX_INFLIGHT_OPS 128
#define CACHE_SIZE 0

const char *
huffman_type_strings[] = {
	[RTE_COMP_HUFFMAN_DEFAULT]	= "PMD default",
	[RTE_COMP_HUFFMAN_FIXED]	= "Fixed",
	[RTE_COMP_HUFFMAN_DYNAMIC]	= "Dynamic"
};

enum zlib_direction {
	ZLIB_NONE,
	ZLIB_COMPRESS,
	ZLIB_DECOMPRESS,
	ZLIB_ALL
};

struct priv_op_data {
	uint16_t orig_idx;
};

struct comp_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *op_pool;
	struct rte_comp_xform *def_comp_xform;
	struct rte_comp_xform *def_decomp_xform;
};

static struct comp_testsuite_params testsuite_params = { 0 };

static void
testsuite_teardown(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;

	rte_mempool_free(ts_params->mbuf_pool);
	rte_mempool_free(ts_params->op_pool);
	rte_free(ts_params->def_comp_xform);
	rte_free(ts_params->def_decomp_xform);
}

static int
testsuite_setup(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	unsigned int i;

	if (rte_compressdev_count() == 0) {
		RTE_LOG(ERR, USER1, "Need at least one compress device\n");
		return TEST_FAILED;
	}

	uint32_t max_buf_size = 0;
	for (i = 0; i < RTE_DIM(compress_test_bufs); i++)
		max_buf_size = RTE_MAX(max_buf_size,
				strlen(compress_test_bufs[i]) + 1);

	max_buf_size *= COMPRESS_BUF_SIZE_RATIO;
	/*
	 * Buffers to be used in compression and decompression.
	 * Since decompressed data might be larger than
	 * compressed data (due to block header),
	 * buffers should be big enough for both cases.
	 */
	ts_params->mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool",
			NUM_MBUFS,
			CACHE_SIZE, 0,
			max_buf_size + RTE_PKTMBUF_HEADROOM,
			rte_socket_id());
	if (ts_params->mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Large mbuf pool could not be created\n");
		return TEST_FAILED;
	}

	ts_params->op_pool = rte_comp_op_pool_create("op_pool", NUM_OPS,
				0, sizeof(struct priv_op_data),
				rte_socket_id());
	if (ts_params->op_pool == NULL) {
		RTE_LOG(ERR, USER1, "Operation pool could not be created\n");
		goto exit;
	}

	ts_params->def_comp_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (ts_params->def_comp_xform == NULL) {
		RTE_LOG(ERR, USER1,
			"Default compress xform could not be created\n");
		goto exit;
	}
	ts_params->def_decomp_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (ts_params->def_decomp_xform == NULL) {
		RTE_LOG(ERR, USER1,
			"Default decompress xform could not be created\n");
		goto exit;
	}

	/* Initializes default values for compress/decompress xforms */
	ts_params->def_comp_xform->type = RTE_COMP_COMPRESS;
	ts_params->def_comp_xform->compress.algo = RTE_COMP_ALGO_DEFLATE,
	ts_params->def_comp_xform->compress.deflate.huffman =
						RTE_COMP_HUFFMAN_DEFAULT;
	ts_params->def_comp_xform->compress.level = RTE_COMP_LEVEL_PMD_DEFAULT;
	ts_params->def_comp_xform->compress.chksum = RTE_COMP_CHECKSUM_NONE;
	ts_params->def_comp_xform->compress.window_size = DEFAULT_WINDOW_SIZE;

	ts_params->def_decomp_xform->type = RTE_COMP_DECOMPRESS;
	ts_params->def_decomp_xform->decompress.algo = RTE_COMP_ALGO_DEFLATE,
	ts_params->def_decomp_xform->decompress.chksum = RTE_COMP_CHECKSUM_NONE;
	ts_params->def_decomp_xform->decompress.window_size = DEFAULT_WINDOW_SIZE;

	return TEST_SUCCESS;

exit:
	testsuite_teardown();

	return TEST_FAILED;
}

static int
generic_ut_setup(void)
{
	/* Configure compressdev (one device, one queue pair) */
	struct rte_compressdev_config config = {
		.socket_id = rte_socket_id(),
		.nb_queue_pairs = 1,
		.max_nb_priv_xforms = NUM_MAX_XFORMS,
		.max_nb_streams = 0
	};

	if (rte_compressdev_configure(0, &config) < 0) {
		RTE_LOG(ERR, USER1, "Device configuration failed\n");
		return -1;
	}

	if (rte_compressdev_queue_pair_setup(0, 0, NUM_MAX_INFLIGHT_OPS,
			rte_socket_id()) < 0) {
		RTE_LOG(ERR, USER1, "Queue pair setup failed\n");
		return -1;
	}

	if (rte_compressdev_start(0) < 0) {
		RTE_LOG(ERR, USER1, "Device could not be started\n");
		return -1;
	}

	return 0;
}

static void
generic_ut_teardown(void)
{
	rte_compressdev_stop(0);
	if (rte_compressdev_close(0) < 0)
		RTE_LOG(ERR, USER1, "Device could not be closed\n");
}

static int
test_compressdev_invalid_configuration(void)
{
	struct rte_compressdev_config invalid_config;
	struct rte_compressdev_config valid_config = {
		.socket_id = rte_socket_id(),
		.nb_queue_pairs = 1,
		.max_nb_priv_xforms = NUM_MAX_XFORMS,
		.max_nb_streams = 0
	};
	struct rte_compressdev_info dev_info;

	/* Invalid configuration with 0 queue pairs */
	memcpy(&invalid_config, &valid_config,
			sizeof(struct rte_compressdev_config));
	invalid_config.nb_queue_pairs = 0;

	TEST_ASSERT_FAIL(rte_compressdev_configure(0, &invalid_config),
			"Device configuration was successful "
			"with no queue pairs (invalid)\n");

	/*
	 * Invalid configuration with too many queue pairs
	 * (if there is an actual maximum number of queue pairs)
	 */
	rte_compressdev_info_get(0, &dev_info);
	if (dev_info.max_nb_queue_pairs != 0) {
		memcpy(&invalid_config, &valid_config,
			sizeof(struct rte_compressdev_config));
		invalid_config.nb_queue_pairs = dev_info.max_nb_queue_pairs + 1;

		TEST_ASSERT_FAIL(rte_compressdev_configure(0, &invalid_config),
				"Device configuration was successful "
				"with too many queue pairs (invalid)\n");
	}

	/* Invalid queue pair setup, with no number of queue pairs set */
	TEST_ASSERT_FAIL(rte_compressdev_queue_pair_setup(0, 0,
				NUM_MAX_INFLIGHT_OPS, rte_socket_id()),
			"Queue pair setup was successful "
			"with no queue pairs set (invalid)\n");

	return TEST_SUCCESS;
}

static int
compare_buffers(const char *buffer1, uint32_t buffer1_len,
		const char *buffer2, uint32_t buffer2_len)
{
	if (buffer1_len != buffer2_len) {
		RTE_LOG(ERR, USER1, "Buffer lengths are different\n");
		return -1;
	}

	if (memcmp(buffer1, buffer2, buffer1_len) != 0) {
		RTE_LOG(ERR, USER1, "Buffers are different\n");
		return -1;
	}

	return 0;
}

/*
 * Maps compressdev and Zlib flush flags
 */
static int
map_zlib_flush_flag(enum rte_comp_flush_flag flag)
{
	switch (flag) {
	case RTE_COMP_FLUSH_NONE:
		return Z_NO_FLUSH;
	case RTE_COMP_FLUSH_SYNC:
		return Z_SYNC_FLUSH;
	case RTE_COMP_FLUSH_FULL:
		return Z_FULL_FLUSH;
	case RTE_COMP_FLUSH_FINAL:
		return Z_FINISH;
	/*
	 * There should be only the values above,
	 * so this should never happen
	 */
	default:
		return -1;
	}
}

static int
compress_zlib(struct rte_comp_op *op,
		const struct rte_comp_xform *xform, int mem_level)
{
	z_stream stream;
	int zlib_flush;
	int strategy, window_bits, comp_level;
	int ret = -1;

	/* initialize zlib stream */
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	if (xform->compress.deflate.huffman == RTE_COMP_HUFFMAN_FIXED)
		strategy = Z_FIXED;
	else
		strategy = Z_DEFAULT_STRATEGY;

	/*
	 * Window bits is the base two logarithm of the window size (in bytes).
	 * When doing raw DEFLATE, this number will be negative.
	 */
	window_bits = -(xform->compress.window_size);

	comp_level = xform->compress.level;

	if (comp_level != RTE_COMP_LEVEL_NONE)
		ret = deflateInit2(&stream, comp_level, Z_DEFLATED,
			window_bits, mem_level, strategy);
	else
		ret = deflateInit(&stream, Z_NO_COMPRESSION);

	if (ret != Z_OK) {
		printf("Zlib deflate could not be initialized\n");
		goto exit;
	}

	/* Assuming stateless operation */
	stream.avail_in = op->src.length;
	stream.next_in = rte_pktmbuf_mtod(op->m_src, uint8_t *);
	stream.avail_out = op->m_dst->data_len;
	stream.next_out = rte_pktmbuf_mtod(op->m_dst, uint8_t *);

	/* Stateless operation, all buffer will be compressed in one go */
	zlib_flush = map_zlib_flush_flag(op->flush_flag);
	ret = deflate(&stream, zlib_flush);

	if (stream.avail_in != 0) {
		RTE_LOG(ERR, USER1, "Buffer could not be read entirely\n");
		goto exit;
	}

	if (ret != Z_STREAM_END)
		goto exit;

	op->consumed = op->src.length - stream.avail_in;
	op->produced = op->m_dst->data_len - stream.avail_out;
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	deflateReset(&stream);

	ret = 0;
exit:
	deflateEnd(&stream);

	return ret;
}

static int
decompress_zlib(struct rte_comp_op *op,
		const struct rte_comp_xform *xform)
{
	z_stream stream;
	int window_bits;
	int zlib_flush;
	int ret = TEST_FAILED;

	/* initialize zlib stream */
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	/*
	 * Window bits is the base two logarithm of the window size (in bytes).
	 * When doing raw DEFLATE, this number will be negative.
	 */
	window_bits = -(xform->decompress.window_size);

	ret = inflateInit2(&stream, window_bits);

	if (ret != Z_OK) {
		printf("Zlib deflate could not be initialized\n");
		goto exit;
	}

	/* Assuming stateless operation */
	stream.avail_in = op->src.length;
	stream.next_in = rte_pktmbuf_mtod(op->m_src, uint8_t *);
	stream.avail_out = op->m_dst->data_len;
	stream.next_out = rte_pktmbuf_mtod(op->m_dst, uint8_t *);

	/* Stateless operation, all buffer will be compressed in one go */
	zlib_flush = map_zlib_flush_flag(op->flush_flag);
	ret = inflate(&stream, zlib_flush);

	if (stream.avail_in != 0) {
		RTE_LOG(ERR, USER1, "Buffer could not be read entirely\n");
		goto exit;
	}

	if (ret != Z_STREAM_END)
		goto exit;

	op->consumed = op->src.length - stream.avail_in;
	op->produced = op->m_dst->data_len - stream.avail_out;
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	inflateReset(&stream);

	ret = 0;
exit:
	inflateEnd(&stream);

	return ret;
}

/*
 * Compresses and decompresses buffer with compressdev API and Zlib API
 */
static int
test_deflate_comp_decomp(const char * const test_bufs[],
		unsigned int num_bufs,
		uint16_t buf_idx[],
		struct rte_comp_xform *compress_xforms[],
		struct rte_comp_xform *decompress_xforms[],
		unsigned int num_xforms,
		enum rte_comp_op_type state,
		enum zlib_direction zlib_dir)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	int ret_status = -1;
	int ret;
	struct rte_mbuf *uncomp_bufs[num_bufs];
	struct rte_mbuf *comp_bufs[num_bufs];
	struct rte_comp_op *ops[num_bufs];
	struct rte_comp_op *ops_processed[num_bufs];
	void *priv_xforms[num_bufs];
	uint16_t num_enqd, num_deqd, num_total_deqd;
	uint16_t num_priv_xforms = 0;
	unsigned int deqd_retries = 0;
	struct priv_op_data *priv_data;
	char *data_ptr;
	unsigned int i;
	const struct rte_compressdev_capabilities *capa =
		rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);

	/* Initialize all arrays to NULL */
	memset(uncomp_bufs, 0, sizeof(struct rte_mbuf *) * num_bufs);
	memset(comp_bufs, 0, sizeof(struct rte_mbuf *) * num_bufs);
	memset(ops, 0, sizeof(struct rte_comp_op *) * num_bufs);
	memset(ops_processed, 0, sizeof(struct rte_comp_op *) * num_bufs);
	memset(priv_xforms, 0, sizeof(void *) * num_bufs);

	/* Prepare the source mbufs with the data */
	ret = rte_pktmbuf_alloc_bulk(ts_params->mbuf_pool, uncomp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Source mbufs could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	for (i = 0; i < num_bufs; i++) {
		data_ptr = rte_pktmbuf_append(uncomp_bufs[i],
					strlen(test_bufs[i]) + 1);
		snprintf(data_ptr, strlen(test_bufs[i]) + 1, "%s",
					test_bufs[i]);
	}

	/* Prepare the destination mbufs */
	ret = rte_pktmbuf_alloc_bulk(ts_params->mbuf_pool, comp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Destination mbufs could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	for (i = 0; i < num_bufs; i++)
		rte_pktmbuf_append(comp_bufs[i],
			strlen(test_bufs[i]) * COMPRESS_BUF_SIZE_RATIO);

	/* Build the compression operations */
	ret = rte_comp_op_bulk_alloc(ts_params->op_pool, ops, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Compress operations could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	for (i = 0; i < num_bufs; i++) {
		ops[i]->m_src = uncomp_bufs[i];
		ops[i]->m_dst = comp_bufs[i];
		ops[i]->src.offset = 0;
		ops[i]->src.length = rte_pktmbuf_pkt_len(uncomp_bufs[i]);
		ops[i]->dst.offset = 0;
		if (state == RTE_COMP_OP_STATELESS) {
			ops[i]->flush_flag = RTE_COMP_FLUSH_FINAL;
		} else {
			RTE_LOG(ERR, USER1,
				"Stateful operations are not supported "
				"in these tests yet\n");
			goto exit;
		}
		ops[i]->input_chksum = 0;
		/*
		 * Store original operation index in private data,
		 * since ordering does not have to be maintained,
		 * when dequeueing from compressdev, so a comparison
		 * at the end of the test can be done.
		 */
		priv_data = (struct priv_op_data *) (ops[i] + 1);
		priv_data->orig_idx = i;
	}

	/* Compress data (either with Zlib API or compressdev API */
	if (zlib_dir == ZLIB_COMPRESS || zlib_dir == ZLIB_ALL) {
		for (i = 0; i < num_bufs; i++) {
			const struct rte_comp_xform *compress_xform =
				compress_xforms[i % num_xforms];
			ret = compress_zlib(ops[i], compress_xform,
					DEFAULT_MEM_LEVEL);
			if (ret < 0)
				goto exit;

			ops_processed[i] = ops[i];
		}
	} else {
		/* Create compress private xform data */
		for (i = 0; i < num_xforms; i++) {
			ret = rte_compressdev_private_xform_create(0,
				(const struct rte_comp_xform *)compress_xforms[i],
				&priv_xforms[i]);
			if (ret < 0) {
				RTE_LOG(ERR, USER1,
					"Compression private xform "
					"could not be created\n");
				goto exit;
			}
			num_priv_xforms++;
		}

		if (capa->comp_feature_flags & RTE_COMP_FF_SHAREABLE_PRIV_XFORM) {
			/* Attach shareable private xform data to ops */
			for (i = 0; i < num_bufs; i++)
				ops[i]->private_xform = priv_xforms[i % num_xforms];
		} else {
			/* Create rest of the private xforms for the other ops */
			for (i = num_xforms; i < num_bufs; i++) {
				ret = rte_compressdev_private_xform_create(0,
					compress_xforms[i % num_xforms],
					&priv_xforms[i]);
				if (ret < 0) {
					RTE_LOG(ERR, USER1,
						"Compression private xform "
						"could not be created\n");
					goto exit;
				}
				num_priv_xforms++;
			}

			/* Attach non shareable private xform data to ops */
			for (i = 0; i < num_bufs; i++)
				ops[i]->private_xform = priv_xforms[i];
		}

		/* Enqueue and dequeue all operations */
		num_enqd = rte_compressdev_enqueue_burst(0, 0, ops, num_bufs);
		if (num_enqd < num_bufs) {
			RTE_LOG(ERR, USER1,
				"The operations could not be enqueued\n");
			goto exit;
		}

		num_total_deqd = 0;
		do {
			/*
			 * If retrying a dequeue call, wait for 10 ms to allow
			 * enough time to the driver to process the operations
			 */
			if (deqd_retries != 0) {
				/*
				 * Avoid infinite loop if not all the
				 * operations get out of the device
				 */
				if (deqd_retries == MAX_DEQD_RETRIES) {
					RTE_LOG(ERR, USER1,
						"Not all operations could be "
						"dequeued\n");
					goto exit;
				}
				usleep(DEQUEUE_WAIT_TIME);
			}
			num_deqd = rte_compressdev_dequeue_burst(0, 0,
					&ops_processed[num_total_deqd], num_bufs);
			num_total_deqd += num_deqd;
			deqd_retries++;
		} while (num_total_deqd < num_enqd);

		deqd_retries = 0;

		/* Free compress private xforms */
		for (i = 0; i < num_priv_xforms; i++) {
			rte_compressdev_private_xform_free(0, priv_xforms[i]);
			priv_xforms[i] = NULL;
		}
		num_priv_xforms = 0;
	}

	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		uint16_t xform_idx = priv_data->orig_idx % num_xforms;
		const struct rte_comp_compress_xform *compress_xform =
				&compress_xforms[xform_idx]->compress;
		enum rte_comp_huffman huffman_type =
			compress_xform->deflate.huffman;
		RTE_LOG(DEBUG, USER1, "Buffer %u compressed from %u to %u bytes "
			"(level = %d, huffman = %s)\n",
			buf_idx[priv_data->orig_idx],
			ops_processed[i]->consumed, ops_processed[i]->produced,
			compress_xform->level,
			huffman_type_strings[huffman_type]);
		RTE_LOG(DEBUG, USER1, "Compression ratio = %.2f",
			(float)ops_processed[i]->produced /
			ops_processed[i]->consumed * 100);
		ops[i] = NULL;
	}

	/*
	 * Check operation status and free source mbufs (destination mbuf and
	 * compress operation information is needed for the decompression stage)
	 */
	for (i = 0; i < num_bufs; i++) {
		if (ops_processed[i]->status != RTE_COMP_OP_STATUS_SUCCESS) {
			RTE_LOG(ERR, USER1,
				"Some operations were not successful\n");
			goto exit;
		}
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_free(uncomp_bufs[priv_data->orig_idx]);
		uncomp_bufs[priv_data->orig_idx] = NULL;
	}

	/* Allocate buffers for decompressed data */
	ret = rte_pktmbuf_alloc_bulk(ts_params->mbuf_pool, uncomp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Destination mbufs could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_append(uncomp_bufs[i],
				strlen(test_bufs[priv_data->orig_idx]) + 1);
	}

	/* Build the decompression operations */
	ret = rte_comp_op_bulk_alloc(ts_params->op_pool, ops, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Decompress operations could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	/* Source buffer is the compressed data from the previous operations */
	for (i = 0; i < num_bufs; i++) {
		ops[i]->m_src = ops_processed[i]->m_dst;
		ops[i]->m_dst = uncomp_bufs[i];
		ops[i]->src.offset = 0;
		/*
		 * Set the length of the compressed data to the
		 * number of bytes that were produced in the previous stage
		 */
		ops[i]->src.length = ops_processed[i]->produced;
		ops[i]->dst.offset = 0;
		if (state == RTE_COMP_OP_STATELESS) {
			ops[i]->flush_flag = RTE_COMP_FLUSH_FINAL;
		} else {
			RTE_LOG(ERR, USER1,
				"Stateful operations are not supported "
				"in these tests yet\n");
			goto exit;
		}
		ops[i]->input_chksum = 0;
		/*
		 * Copy private data from previous operations,
		 * to keep the pointer to the original buffer
		 */
		memcpy(ops[i] + 1, ops_processed[i] + 1,
				sizeof(struct priv_op_data));
	}

	/*
	 * Free the previous compress operations,
	 * as it is not needed anymore
	 */
	for (i = 0; i < num_bufs; i++) {
		rte_comp_op_free(ops_processed[i]);
		ops_processed[i] = NULL;
	}

	/* Decompress data (either with Zlib API or compressdev API */
	if (zlib_dir == ZLIB_DECOMPRESS || zlib_dir == ZLIB_ALL) {
		for (i = 0; i < num_bufs; i++) {
			priv_data = (struct priv_op_data *)(ops[i] + 1);
			uint16_t xform_idx = priv_data->orig_idx % num_xforms;
			const struct rte_comp_xform *decompress_xform =
				decompress_xforms[xform_idx];

			ret = decompress_zlib(ops[i], decompress_xform);
			if (ret < 0)
				goto exit;

			ops_processed[i] = ops[i];
		}
	} else {
		/* Create decompress private xform data */
		for (i = 0; i < num_xforms; i++) {
			ret = rte_compressdev_private_xform_create(0,
				(const struct rte_comp_xform *)decompress_xforms[i],
				&priv_xforms[i]);
			if (ret < 0) {
				RTE_LOG(ERR, USER1,
					"Decompression private xform "
					"could not be created\n");
				goto exit;
			}
			num_priv_xforms++;
		}

		if (capa->comp_feature_flags & RTE_COMP_FF_SHAREABLE_PRIV_XFORM) {
			/* Attach shareable private xform data to ops */
			for (i = 0; i < num_bufs; i++) {
				priv_data = (struct priv_op_data *)(ops[i] + 1);
				uint16_t xform_idx = priv_data->orig_idx %
								num_xforms;
				ops[i]->private_xform = priv_xforms[xform_idx];
			}
		} else {
			/* Create rest of the private xforms for the other ops */
			for (i = num_xforms; i < num_bufs; i++) {
				ret = rte_compressdev_private_xform_create(0,
					decompress_xforms[i % num_xforms],
					&priv_xforms[i]);
				if (ret < 0) {
					RTE_LOG(ERR, USER1,
						"Decompression private xform "
						"could not be created\n");
					goto exit;
				}
				num_priv_xforms++;
			}

			/* Attach non shareable private xform data to ops */
			for (i = 0; i < num_bufs; i++) {
				priv_data = (struct priv_op_data *)(ops[i] + 1);
				uint16_t xform_idx = priv_data->orig_idx;
				ops[i]->private_xform = priv_xforms[xform_idx];
			}
		}

		/* Enqueue and dequeue all operations */
		num_enqd = rte_compressdev_enqueue_burst(0, 0, ops, num_bufs);
		if (num_enqd < num_bufs) {
			RTE_LOG(ERR, USER1,
				"The operations could not be enqueued\n");
			goto exit;
		}

		num_total_deqd = 0;
		do {
			/*
			 * If retrying a dequeue call, wait for 10 ms to allow
			 * enough time to the driver to process the operations
			 */
			if (deqd_retries != 0) {
				/*
				 * Avoid infinite loop if not all the
				 * operations get out of the device
				 */
				if (deqd_retries == MAX_DEQD_RETRIES) {
					RTE_LOG(ERR, USER1,
						"Not all operations could be "
						"dequeued\n");
					goto exit;
				}
				usleep(DEQUEUE_WAIT_TIME);
			}
			num_deqd = rte_compressdev_dequeue_burst(0, 0,
					&ops_processed[num_total_deqd], num_bufs);
			num_total_deqd += num_deqd;
			deqd_retries++;
		} while (num_total_deqd < num_enqd);

		deqd_retries = 0;
	}

	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		RTE_LOG(DEBUG, USER1, "Buffer %u decompressed from %u to %u bytes\n",
			buf_idx[priv_data->orig_idx],
			ops_processed[i]->consumed, ops_processed[i]->produced);
		ops[i] = NULL;
	}

	/*
	 * Check operation status and free source mbuf (destination mbuf and
	 * compress operation information is still needed)
	 */
	for (i = 0; i < num_bufs; i++) {
		if (ops_processed[i]->status != RTE_COMP_OP_STATUS_SUCCESS) {
			RTE_LOG(ERR, USER1,
				"Some operations were not successful\n");
			goto exit;
		}
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_free(comp_bufs[priv_data->orig_idx]);
		comp_bufs[priv_data->orig_idx] = NULL;
	}

	/*
	 * Compare the original stream with the decompressed stream
	 * (in size and the data)
	 */
	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		const char *buf1 = test_bufs[priv_data->orig_idx];
		const char *buf2 = rte_pktmbuf_mtod(ops_processed[i]->m_dst,
				const char *);

		if (compare_buffers(buf1, strlen(buf1) + 1,
				buf2, ops_processed[i]->produced) < 0)
			goto exit;
	}

	ret_status = 0;

exit:
	/* Free resources */
	for (i = 0; i < num_bufs; i++) {
		rte_pktmbuf_free(uncomp_bufs[i]);
		rte_pktmbuf_free(comp_bufs[i]);
		rte_comp_op_free(ops[i]);
		rte_comp_op_free(ops_processed[i]);
	}
	for (i = 0; i < num_priv_xforms; i++) {
		if (priv_xforms[i] != NULL)
			rte_compressdev_private_xform_free(0, priv_xforms[i]);
	}

	return ret_status;
}

static int
test_compressdev_deflate_stateless_fixed(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	const char *test_buffer;
	uint16_t i;
	int ret;
	struct rte_comp_xform *compress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);

	if (compress_xform == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress xform could not be created\n");
		ret = TEST_FAILED;
		goto exit;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));
	compress_xform->compress.deflate.huffman = RTE_COMP_HUFFMAN_FIXED;

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		test_buffer = compress_test_bufs[i];

		/* Compress with compressdev, decompress with Zlib */
		if (test_deflate_comp_decomp(&test_buffer, 1,
				&i,
				&compress_xform,
				&ts_params->def_decomp_xform,
				1,
				RTE_COMP_OP_STATELESS,
				ZLIB_DECOMPRESS) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}

		/* Compress with Zlib, decompress with compressdev */
		if (test_deflate_comp_decomp(&test_buffer, 1,
				&i,
				&compress_xform,
				&ts_params->def_decomp_xform,
				1,
				RTE_COMP_OP_STATELESS,
				ZLIB_COMPRESS) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
	}

	ret = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	return ret;
}

static int
test_compressdev_deflate_stateless_dynamic(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	const char *test_buffer;
	uint16_t i;
	int ret;
	struct rte_comp_xform *compress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);

	if (compress_xform == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress xform could not be created\n");
		ret = TEST_FAILED;
		goto exit;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));
	compress_xform->compress.deflate.huffman = RTE_COMP_HUFFMAN_DYNAMIC;

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		test_buffer = compress_test_bufs[i];

		/* Compress with compressdev, decompress with Zlib */
		if (test_deflate_comp_decomp(&test_buffer, 1,
				&i,
				&compress_xform,
				&ts_params->def_decomp_xform,
				1,
				RTE_COMP_OP_STATELESS,
				ZLIB_DECOMPRESS) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}

		/* Compress with Zlib, decompress with compressdev */
		if (test_deflate_comp_decomp(&test_buffer, 1,
				&i,
				&compress_xform,
				&ts_params->def_decomp_xform,
				1,
				RTE_COMP_OP_STATELESS,
				ZLIB_COMPRESS) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
	}

	ret = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	return ret;
}

static int
test_compressdev_deflate_stateless_multi_op(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t num_bufs = RTE_DIM(compress_test_bufs);
	uint16_t buf_idx[num_bufs];
	uint16_t i;

	for (i = 0; i < num_bufs; i++)
		buf_idx[i] = i;

	/* Compress with compressdev, decompress with Zlib */
	if (test_deflate_comp_decomp(compress_test_bufs, num_bufs,
			buf_idx,
			&ts_params->def_comp_xform,
			&ts_params->def_decomp_xform,
			1,
			RTE_COMP_OP_STATELESS,
			ZLIB_DECOMPRESS) < 0)
		return TEST_FAILED;

	/* Compress with Zlib, decompress with compressdev */
	if (test_deflate_comp_decomp(compress_test_bufs, num_bufs,
			buf_idx,
			&ts_params->def_comp_xform,
			&ts_params->def_decomp_xform,
			1,
			RTE_COMP_OP_STATELESS,
			ZLIB_COMPRESS) < 0)
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static int
test_compressdev_deflate_stateless_multi_level(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	const char *test_buffer;
	unsigned int level;
	uint16_t i;
	int ret;
	struct rte_comp_xform *compress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);

	if (compress_xform == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress xform could not be created\n");
		ret = TEST_FAILED;
		goto exit;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		test_buffer = compress_test_bufs[i];
		for (level = RTE_COMP_LEVEL_MIN; level <= RTE_COMP_LEVEL_MAX;
				level++) {
			compress_xform->compress.level = level;
			/* Compress with compressdev, decompress with Zlib */
			if (test_deflate_comp_decomp(&test_buffer, 1,
					&i,
					&compress_xform,
					&ts_params->def_decomp_xform,
					1,
					RTE_COMP_OP_STATELESS,
					ZLIB_DECOMPRESS) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
		}
	}

	ret = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	return ret;
}

#define NUM_XFORMS 3
static int
test_compressdev_deflate_stateless_multi_xform(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t num_bufs = NUM_XFORMS;
	struct rte_comp_xform *compress_xforms[NUM_XFORMS] = {NULL};
	struct rte_comp_xform *decompress_xforms[NUM_XFORMS] = {NULL};
	const char *test_buffers[NUM_XFORMS];
	uint16_t i;
	unsigned int level = RTE_COMP_LEVEL_MIN;
	uint16_t buf_idx[num_bufs];

	int ret;

	/* Create multiple xforms with various levels */
	for (i = 0; i < NUM_XFORMS; i++) {
		compress_xforms[i] = rte_malloc(NULL,
				sizeof(struct rte_comp_xform), 0);
		if (compress_xforms[i] == NULL) {
			RTE_LOG(ERR, USER1,
				"Compress xform could not be created\n");
			ret = TEST_FAILED;
			goto exit;
		}

		memcpy(compress_xforms[i], ts_params->def_comp_xform,
				sizeof(struct rte_comp_xform));
		compress_xforms[i]->compress.level = level;
		level++;

		decompress_xforms[i] = rte_malloc(NULL,
				sizeof(struct rte_comp_xform), 0);
		if (decompress_xforms[i] == NULL) {
			RTE_LOG(ERR, USER1,
				"Decompress xform could not be created\n");
			ret = TEST_FAILED;
			goto exit;
		}

		memcpy(decompress_xforms[i], ts_params->def_decomp_xform,
				sizeof(struct rte_comp_xform));
	}

	for (i = 0; i < NUM_XFORMS; i++) {
		buf_idx[i] = 0;
		/* Use the same buffer in all sessions */
		test_buffers[i] = compress_test_bufs[0];
	}
	/* Compress with compressdev, decompress with Zlib */
	if (test_deflate_comp_decomp(test_buffers, num_bufs,
			buf_idx,
			compress_xforms,
			decompress_xforms,
			NUM_XFORMS,
			RTE_COMP_OP_STATELESS,
			ZLIB_DECOMPRESS) < 0) {
		ret = TEST_FAILED;
		goto exit;
	}

	ret = TEST_SUCCESS;
exit:
	for (i = 0; i < NUM_XFORMS; i++) {
		rte_free(compress_xforms[i]);
		rte_free(decompress_xforms[i]);
	}

	return ret;
}

static struct unit_test_suite compressdev_testsuite  = {
	.suite_name = "compressdev unit test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL,
			test_compressdev_invalid_configuration),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_fixed),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_dynamic),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_multi_op),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_multi_level),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_multi_xform),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_compressdev(void)
{
	return unit_test_suite_runner(&compressdev_testsuite);
}

REGISTER_TEST_COMMAND(compressdev_autotest, test_compressdev);
