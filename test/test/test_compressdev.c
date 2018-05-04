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
#define NUM_MAX_XFORMS 1
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

struct comp_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *op_pool;
	struct rte_comp_xform def_comp_xform;
	struct rte_comp_xform def_decomp_xform;
};

static struct comp_testsuite_params testsuite_params = { 0 };

static void
testsuite_teardown(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;

	rte_mempool_free(ts_params->mbuf_pool);
	rte_mempool_free(ts_params->op_pool);
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
						0, 0, rte_socket_id());
	if (ts_params->op_pool == NULL) {
		RTE_LOG(ERR, USER1, "Operation pool could not be created\n");
		goto exit;
	}

	/* Initializes default values for compress/decompress xforms */
	ts_params->def_comp_xform.type = RTE_COMP_COMPRESS;
	ts_params->def_comp_xform.compress.algo = RTE_COMP_ALGO_DEFLATE,
	ts_params->def_comp_xform.compress.deflate.huffman =
						RTE_COMP_HUFFMAN_DEFAULT;
	ts_params->def_comp_xform.compress.level = RTE_COMP_LEVEL_PMD_DEFAULT;
	ts_params->def_comp_xform.compress.chksum = RTE_COMP_CHECKSUM_NONE;
	ts_params->def_comp_xform.compress.window_size = DEFAULT_WINDOW_SIZE;

	ts_params->def_decomp_xform.type = RTE_COMP_DECOMPRESS;
	ts_params->def_decomp_xform.decompress.algo = RTE_COMP_ALGO_DEFLATE,
	ts_params->def_decomp_xform.decompress.chksum = RTE_COMP_CHECKSUM_NONE;
	ts_params->def_decomp_xform.decompress.window_size = DEFAULT_WINDOW_SIZE;

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
test_deflate_comp_decomp(const char *test_buffer,
		struct rte_comp_xform *compress_xform,
		struct rte_comp_xform *decompress_xform,
		enum rte_comp_op_type state,
		enum zlib_direction zlib_dir)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	int ret_status = -1;
	int ret;
	struct rte_mbuf *comp_buf = NULL;
	struct rte_mbuf *uncomp_buf = NULL;
	struct rte_comp_op *op = NULL;
	struct rte_comp_op *op_processed = NULL;
	void *priv_xform = NULL;
	uint16_t num_deqd;
	unsigned int deqd_retries = 0;
	char *data_ptr;

	/* Prepare the source mbuf with the data */
	uncomp_buf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (uncomp_buf == NULL) {
		RTE_LOG(ERR, USER1,
			"Source mbuf could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	data_ptr = rte_pktmbuf_append(uncomp_buf, strlen(test_buffer) + 1);
	snprintf(data_ptr, strlen(test_buffer) + 1, "%s", test_buffer);

	/* Prepare the destination mbuf */
	comp_buf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (comp_buf == NULL) {
		RTE_LOG(ERR, USER1,
			"Destination mbuf could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	rte_pktmbuf_append(comp_buf,
			strlen(test_buffer) * COMPRESS_BUF_SIZE_RATIO);

	/* Build the compression operations */
	op = rte_comp_op_alloc(ts_params->op_pool);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress operation could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	op->m_src = uncomp_buf;
	op->m_dst = comp_buf;
	op->src.offset = 0;
	op->src.length = rte_pktmbuf_pkt_len(uncomp_buf);
	op->dst.offset = 0;
	if (state == RTE_COMP_OP_STATELESS) {
		op->flush_flag = RTE_COMP_FLUSH_FINAL;
	} else {
		RTE_LOG(ERR, USER1,
			"Stateful operations are not supported "
			"in these tests yet\n");
		goto exit;
	}
	op->input_chksum = 0;

	/* Compress data (either with Zlib API or compressdev API */
	if (zlib_dir == ZLIB_COMPRESS || zlib_dir == ZLIB_ALL) {
		ret = compress_zlib(op,
			(const struct rte_comp_xform *)&compress_xform,
			DEFAULT_MEM_LEVEL);
		if (ret < 0)
			goto exit;

		op_processed = op;
	} else {
		/* Create compress xform private data */
		ret = rte_compressdev_private_xform_create(0,
			(const struct rte_comp_xform *)compress_xform,
			&priv_xform);
		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Compression private xform "
				"could not be created\n");
			goto exit;
		}

		/* Attach xform private data to operation */
		op->private_xform = priv_xform;

		/* Enqueue and dequeue all operations */
		ret = rte_compressdev_enqueue_burst(0, 0, &op, 1);
		if (ret == 0) {
			RTE_LOG(ERR, USER1,
				"The operation could not be enqueued\n");
			goto exit;
		}
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
					&op_processed, 1);

			deqd_retries++;
		} while (num_deqd < 1);

		deqd_retries = 0;

		/* Free compress private xform */
		rte_compressdev_private_xform_free(0, priv_xform);
		priv_xform = NULL;
	}

	enum rte_comp_huffman huffman_type =
		compress_xform->compress.deflate.huffman;
	RTE_LOG(DEBUG, USER1, "Buffer compressed from %u to %u bytes "
			"(level = %u, huffman = %s)\n",
			op_processed->consumed, op_processed->produced,
			compress_xform->compress.level,
			huffman_type_strings[huffman_type]);
	RTE_LOG(DEBUG, USER1, "Compression ratio = %.2f",
			(float)op_processed->produced /
			op_processed->consumed * 100);
	op = NULL;

	/*
	 * Check operation status and free source mbuf (destination mbuf and
	 * compress operation information is needed for the decompression stage)
	 */
	if (op_processed->status != RTE_COMP_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
			"Some operations were not successful\n");
		goto exit;
	}
	rte_pktmbuf_free(uncomp_buf);
	uncomp_buf = NULL;

	/* Allocate buffer for decompressed data */
	uncomp_buf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (uncomp_buf == NULL) {
		RTE_LOG(ERR, USER1,
			"Destination mbuf could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	rte_pktmbuf_append(uncomp_buf, strlen(test_buffer) + 1);

	/* Build the decompression operations */
	op = rte_comp_op_alloc(ts_params->op_pool);
	if (op == NULL) {
		RTE_LOG(ERR, USER1,
			"Decompress operation could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	/* Source buffer is the compressed data from the previous operation */
	op->m_src = op_processed->m_dst;
	op->m_dst = uncomp_buf;
	op->src.offset = 0;
	/*
	 * Set the length of the compressed data to the
	 * number of bytes that were produced in the previous stage
	 */
	op->src.length = op_processed->produced;
	op->dst.offset = 0;
	if (state == RTE_COMP_OP_STATELESS) {
		op->flush_flag = RTE_COMP_FLUSH_FINAL;
	} else {
		RTE_LOG(ERR, USER1,
			"Stateful operations are not supported "
			"in these tests yet\n");
		goto exit;
	}
	op->input_chksum = 0;

	/*
	 * Free the previous compress operation,
	 * as it is not needed anymore
	 */
	rte_comp_op_free(op_processed);
	op_processed = NULL;

	/* Decompress data (either with Zlib API or compressdev API */
	if (zlib_dir == ZLIB_DECOMPRESS || zlib_dir == ZLIB_ALL) {
		ret = decompress_zlib(op,
			(const struct rte_comp_xform *)&decompress_xform);
		if (ret < 0)
			goto exit;

		op_processed = op;
	} else {
		num_deqd = 0;
		/* Create decompress xform private data */
		ret = rte_compressdev_private_xform_create(0,
			(const struct rte_comp_xform *)decompress_xform,
			&priv_xform);
		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Decompression private xform "
				"could not be created\n");
			goto exit;
		}

		/* Attach xform private data to operation */
		op->private_xform = priv_xform;

		/* Enqueue and dequeue all operations */
		ret = rte_compressdev_enqueue_burst(0, 0, &op, 1);
		if (ret == 0) {
			RTE_LOG(ERR, USER1,
				"The operation could not be enqueued\n");
			goto exit;
		}
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
					&op_processed, 1);

			deqd_retries++;
		} while (num_deqd < 1);
	}

	RTE_LOG(DEBUG, USER1, "Buffer decompressed from %u to %u bytes\n",
			op_processed->consumed, op_processed->produced);
	op = NULL;
	/*
	 * Check operation status and free source mbuf (destination mbuf and
	 * compress operation information is still needed)
	 */
	if (op_processed->status != RTE_COMP_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1,
			"Some operations were not successful\n");
		goto exit;
	}
	rte_pktmbuf_free(comp_buf);
	comp_buf = NULL;

	/*
	 * Compare the original stream with the decompressed stream
	 * (in size and the data)
	 */
	if (compare_buffers(test_buffer, strlen(test_buffer) + 1,
			rte_pktmbuf_mtod(op_processed->m_dst, const char *),
			op_processed->produced) < 0)
		goto exit;

	ret_status = 0;

exit:
	/* Free resources */
	rte_pktmbuf_free(uncomp_buf);
	rte_pktmbuf_free(comp_buf);
	rte_comp_op_free(op);
	rte_comp_op_free(op_processed);

	if (priv_xform != NULL)
		rte_compressdev_private_xform_free(0, priv_xform);

	return ret_status;
}

static int
test_compressdev_deflate_stateless_fixed(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	const char *test_buffer;
	uint16_t i;
	struct rte_comp_xform compress_xform;

	memcpy(&compress_xform, &ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));
	compress_xform.compress.deflate.huffman = RTE_COMP_HUFFMAN_FIXED;

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		test_buffer = compress_test_bufs[i];

		/* Compress with compressdev, decompress with Zlib */
		if (test_deflate_comp_decomp(test_buffer,
				&compress_xform,
				&ts_params->def_decomp_xform,
				RTE_COMP_OP_STATELESS,
				ZLIB_DECOMPRESS) < 0)
			return TEST_FAILED;

		/* Compress with Zlib, decompress with compressdev */
		if (test_deflate_comp_decomp(test_buffer,
				&compress_xform,
				&ts_params->def_decomp_xform,
				RTE_COMP_OP_STATELESS,
				ZLIB_COMPRESS) < 0)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_compressdev_deflate_stateless_dynamic(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	const char *test_buffer;
	uint16_t i;
	struct rte_comp_xform compress_xform;

	memcpy(&compress_xform, &ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));
	compress_xform.compress.deflate.huffman = RTE_COMP_HUFFMAN_DYNAMIC;

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		test_buffer = compress_test_bufs[i];

		/* Compress with compressdev, decompress with Zlib */
		if (test_deflate_comp_decomp(test_buffer,
				&compress_xform,
				&ts_params->def_decomp_xform,
				RTE_COMP_OP_STATELESS,
				ZLIB_DECOMPRESS) < 0)
			return TEST_FAILED;

		/* Compress with Zlib, decompress with compressdev */
		if (test_deflate_comp_decomp(test_buffer,
				&compress_xform,
				&ts_params->def_decomp_xform,
				RTE_COMP_OP_STATELESS,
				ZLIB_COMPRESS) < 0)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static struct unit_test_suite compressdev_testsuite  = {
	.suite_name = "compressdev unit test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_fixed),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_dynamic),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_compressdev(void)
{
	return unit_test_suite_runner(&compressdev_testsuite);
}

REGISTER_TEST_COMMAND(compressdev_autotest, test_compressdev);
