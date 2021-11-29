/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 - 2019 Intel Corporation
 */
#include <string.h>
#include <zlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_compressdev.h>
#include <rte_string_fns.h>

#include "test_compressdev_test_buffer.h"
#include "test.h"

#define DIV_CEIL(a, b)  ((a) / (b) + ((a) % (b) != 0))

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
#define COMPRESS_BUF_SIZE_RATIO_DISABLED 1.0
#define COMPRESS_BUF_SIZE_RATIO_OVERFLOW 0.2
#define NUM_LARGE_MBUFS 16
#define SMALL_SEG_SIZE 256
#define MAX_SEGS 16
#define NUM_OPS 16
#define NUM_MAX_XFORMS 16
#define NUM_MAX_INFLIGHT_OPS 128
#define CACHE_SIZE 0

#define ZLIB_CRC_CHECKSUM_WINDOW_BITS 31
#define ZLIB_HEADER_SIZE 2
#define ZLIB_TRAILER_SIZE 4
#define GZIP_HEADER_SIZE 10
#define GZIP_TRAILER_SIZE 8

#define OUT_OF_SPACE_BUF 1

#define MAX_MBUF_SEGMENT_SIZE 65535
#define MAX_DATA_MBUF_SIZE (MAX_MBUF_SEGMENT_SIZE - RTE_PKTMBUF_HEADROOM)
#define NUM_BIG_MBUFS (512 + 1)
#define BIG_DATA_TEST_SIZE (MAX_DATA_MBUF_SIZE * 2)

/* constants for "im buffer" tests start here */

/* number of mbufs lower than number of inflight ops */
#define IM_BUF_NUM_MBUFS 3
/* above threshold (QAT_FALLBACK_THLD) and below max mbuf size */
#define IM_BUF_DATA_TEST_SIZE_LB 59600
/* data size smaller than the queue capacity */
#define IM_BUF_DATA_TEST_SIZE_SGL (MAX_DATA_MBUF_SIZE * IM_BUF_NUM_MBUFS)
/* number of mbufs bigger than number of inflight ops */
#define IM_BUF_NUM_MBUFS_OVER (NUM_MAX_INFLIGHT_OPS + 1)
/* data size bigger than the queue capacity */
#define IM_BUF_DATA_TEST_SIZE_OVER (MAX_DATA_MBUF_SIZE * IM_BUF_NUM_MBUFS_OVER)
/* number of mid-size mbufs */
#define IM_BUF_NUM_MBUFS_MID ((NUM_MAX_INFLIGHT_OPS / 3) + 1)
/* capacity of mid-size mbufs */
#define IM_BUF_DATA_TEST_SIZE_MID (MAX_DATA_MBUF_SIZE * IM_BUF_NUM_MBUFS_MID)


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

enum varied_buff {
	LB_BOTH = 0,	/* both input and output are linear*/
	SGL_BOTH,	/* both input and output are chained */
	SGL_TO_LB,	/* input buffer is chained */
	LB_TO_SGL	/* output buffer is chained */
};

enum overflow_test {
	OVERFLOW_DISABLED,
	OVERFLOW_ENABLED
};

enum ratio_switch {
	RATIO_DISABLED,
	RATIO_ENABLED
};

enum operation_type {
	OPERATION_COMPRESSION,
	OPERATION_DECOMPRESSION
};

struct priv_op_data {
	uint16_t orig_idx;
};

struct comp_testsuite_params {
	struct rte_mempool *large_mbuf_pool;
	struct rte_mempool *small_mbuf_pool;
	struct rte_mempool *big_mbuf_pool;
	struct rte_mempool *op_pool;
	struct rte_comp_xform *def_comp_xform;
	struct rte_comp_xform *def_decomp_xform;
};

struct interim_data_params {
	const char * const *test_bufs;
	unsigned int num_bufs;
	uint16_t *buf_idx;
	struct rte_comp_xform **compress_xforms;
	struct rte_comp_xform **decompress_xforms;
	unsigned int num_xforms;
};

struct test_data_params {
	enum rte_comp_op_type compress_state;
	enum rte_comp_op_type decompress_state;
	enum varied_buff buff_type;
	enum zlib_direction zlib_dir;
	unsigned int out_of_space;
	unsigned int big_data;
	/* stateful decompression specific parameters */
	unsigned int decompress_output_block_size;
	unsigned int decompress_steps_max;
	/* external mbufs specific parameters */
	unsigned int use_external_mbufs;
	unsigned int inbuf_data_size;
	const struct rte_memzone *inbuf_memzone;
	const struct rte_memzone *compbuf_memzone;
	const struct rte_memzone *uncompbuf_memzone;
	/* overflow test activation */
	enum overflow_test overflow;
	enum ratio_switch ratio;
};

struct test_private_arrays {
	struct rte_mbuf **uncomp_bufs;
	struct rte_mbuf **comp_bufs;
	struct rte_comp_op **ops;
	struct rte_comp_op **ops_processed;
	void **priv_xforms;
	uint64_t *compress_checksum;
	uint32_t *compressed_data_size;
	void **stream;
	char **all_decomp_data;
	unsigned int *decomp_produced_data_size;
	uint16_t num_priv_xforms;
};

static struct comp_testsuite_params testsuite_params = { 0 };


static void
testsuite_teardown(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;

	if (rte_mempool_in_use_count(ts_params->large_mbuf_pool))
		RTE_LOG(ERR, USER1, "Large mbuf pool still has unfreed bufs\n");
	if (rte_mempool_in_use_count(ts_params->small_mbuf_pool))
		RTE_LOG(ERR, USER1, "Small mbuf pool still has unfreed bufs\n");
	if (rte_mempool_in_use_count(ts_params->big_mbuf_pool))
		RTE_LOG(ERR, USER1, "Big mbuf pool still has unfreed bufs\n");
	if (rte_mempool_in_use_count(ts_params->op_pool))
		RTE_LOG(ERR, USER1, "op pool still has unfreed ops\n");

	rte_mempool_free(ts_params->large_mbuf_pool);
	rte_mempool_free(ts_params->small_mbuf_pool);
	rte_mempool_free(ts_params->big_mbuf_pool);
	rte_mempool_free(ts_params->op_pool);
	rte_free(ts_params->def_comp_xform);
	rte_free(ts_params->def_decomp_xform);
}

static int
testsuite_setup(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint32_t max_buf_size = 0;
	unsigned int i;

	if (rte_compressdev_count() == 0) {
		RTE_LOG(WARNING, USER1, "Need at least one compress device\n");
		return TEST_SKIPPED;
	}

	RTE_LOG(NOTICE, USER1, "Running tests on device %s\n",
				rte_compressdev_name_get(0));

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++)
		max_buf_size = RTE_MAX(max_buf_size,
				strlen(compress_test_bufs[i]) + 1);

	/*
	 * Buffers to be used in compression and decompression.
	 * Since decompressed data might be larger than
	 * compressed data (due to block header),
	 * buffers should be big enough for both cases.
	 */
	max_buf_size *= COMPRESS_BUF_SIZE_RATIO;
	ts_params->large_mbuf_pool = rte_pktmbuf_pool_create("large_mbuf_pool",
			NUM_LARGE_MBUFS,
			CACHE_SIZE, 0,
			max_buf_size + RTE_PKTMBUF_HEADROOM,
			rte_socket_id());
	if (ts_params->large_mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Large mbuf pool could not be created\n");
		return TEST_FAILED;
	}

	/* Create mempool with smaller buffers for SGL testing */
	ts_params->small_mbuf_pool = rte_pktmbuf_pool_create("small_mbuf_pool",
			NUM_LARGE_MBUFS * MAX_SEGS,
			CACHE_SIZE, 0,
			SMALL_SEG_SIZE + RTE_PKTMBUF_HEADROOM,
			rte_socket_id());
	if (ts_params->small_mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Small mbuf pool could not be created\n");
		goto exit;
	}

	/* Create mempool with big buffers for SGL testing */
	ts_params->big_mbuf_pool = rte_pktmbuf_pool_create("big_mbuf_pool",
			NUM_BIG_MBUFS + 1,
			CACHE_SIZE, 0,
			MAX_MBUF_SEGMENT_SIZE,
			rte_socket_id());
	if (ts_params->big_mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Big mbuf pool could not be created\n");
		goto exit;
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
		.max_nb_streams = 1
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
		.max_nb_streams = 1
	};
	struct rte_compressdev_info dev_info;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");

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
	int ret = TEST_FAILED;
	uint8_t *single_src_buf = NULL;
	uint8_t *single_dst_buf = NULL;

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
	if (xform->compress.chksum == RTE_COMP_CHECKSUM_ADLER32)
		window_bits *= -1;
	else if (xform->compress.chksum == RTE_COMP_CHECKSUM_CRC32)
		window_bits = ZLIB_CRC_CHECKSUM_WINDOW_BITS;

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
	/* SGL Input */
	if (op->m_src->nb_segs > 1) {
		single_src_buf = rte_malloc(NULL,
				rte_pktmbuf_pkt_len(op->m_src), 0);
		if (single_src_buf == NULL) {
			RTE_LOG(ERR, USER1, "Buffer could not be allocated\n");
			goto exit;
		}

		if (rte_pktmbuf_read(op->m_src, op->src.offset,
					rte_pktmbuf_pkt_len(op->m_src) -
					op->src.offset,
					single_src_buf) == NULL) {
			RTE_LOG(ERR, USER1,
				"Buffer could not be read entirely\n");
			goto exit;
		}

		stream.avail_in = op->src.length;
		stream.next_in = single_src_buf;

	} else {
		stream.avail_in = op->src.length;
		stream.next_in = rte_pktmbuf_mtod_offset(op->m_src, uint8_t *,
				op->src.offset);
	}
	/* SGL output */
	if (op->m_dst->nb_segs > 1) {

		single_dst_buf = rte_malloc(NULL,
				rte_pktmbuf_pkt_len(op->m_dst), 0);
			if (single_dst_buf == NULL) {
				RTE_LOG(ERR, USER1,
					"Buffer could not be allocated\n");
			goto exit;
		}

		stream.avail_out = op->m_dst->pkt_len;
		stream.next_out = single_dst_buf;

	} else {/* linear output */
		stream.avail_out = op->m_dst->data_len;
		stream.next_out = rte_pktmbuf_mtod_offset(op->m_dst, uint8_t *,
				op->dst.offset);
	}

	/* Stateless operation, all buffer will be compressed in one go */
	zlib_flush = map_zlib_flush_flag(op->flush_flag);
	ret = deflate(&stream, zlib_flush);

	if (stream.avail_in != 0) {
		RTE_LOG(ERR, USER1, "Buffer could not be read entirely\n");
		goto exit;
	}

	if (ret != Z_STREAM_END)
		goto exit;

	/* Copy data to destination SGL */
	if (op->m_dst->nb_segs > 1) {
		uint32_t remaining_data = stream.total_out;
		uint8_t *src_data = single_dst_buf;
		struct rte_mbuf *dst_buf = op->m_dst;

		while (remaining_data > 0) {
			uint8_t *dst_data = rte_pktmbuf_mtod_offset(dst_buf,
						uint8_t *, op->dst.offset);
			/* Last segment */
			if (remaining_data < dst_buf->data_len) {
				memcpy(dst_data, src_data, remaining_data);
				remaining_data = 0;
			} else {
				memcpy(dst_data, src_data, dst_buf->data_len);
				remaining_data -= dst_buf->data_len;
				src_data += dst_buf->data_len;
				dst_buf = dst_buf->next;
			}
		}
	}

	op->consumed = stream.total_in;
	if (xform->compress.chksum == RTE_COMP_CHECKSUM_ADLER32) {
		rte_pktmbuf_adj(op->m_dst, ZLIB_HEADER_SIZE);
		rte_pktmbuf_trim(op->m_dst, ZLIB_TRAILER_SIZE);
		op->produced = stream.total_out - (ZLIB_HEADER_SIZE +
				ZLIB_TRAILER_SIZE);
	} else if (xform->compress.chksum == RTE_COMP_CHECKSUM_CRC32) {
		rte_pktmbuf_adj(op->m_dst, GZIP_HEADER_SIZE);
		rte_pktmbuf_trim(op->m_dst, GZIP_TRAILER_SIZE);
		op->produced = stream.total_out - (GZIP_HEADER_SIZE +
				GZIP_TRAILER_SIZE);
	} else
		op->produced = stream.total_out;

	op->status = RTE_COMP_OP_STATUS_SUCCESS;
	op->output_chksum = stream.adler;

	deflateReset(&stream);

	ret = 0;
exit:
	deflateEnd(&stream);
	rte_free(single_src_buf);
	rte_free(single_dst_buf);

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
	uint8_t *single_src_buf = NULL;
	uint8_t *single_dst_buf = NULL;

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
	/* SGL */
	if (op->m_src->nb_segs > 1) {
		single_src_buf = rte_malloc(NULL,
				rte_pktmbuf_pkt_len(op->m_src), 0);
		if (single_src_buf == NULL) {
			RTE_LOG(ERR, USER1, "Buffer could not be allocated\n");
			goto exit;
		}
		single_dst_buf = rte_malloc(NULL,
				rte_pktmbuf_pkt_len(op->m_dst), 0);
		if (single_dst_buf == NULL) {
			RTE_LOG(ERR, USER1, "Buffer could not be allocated\n");
			goto exit;
		}
		if (rte_pktmbuf_read(op->m_src, 0,
					rte_pktmbuf_pkt_len(op->m_src),
					single_src_buf) == NULL) {
			RTE_LOG(ERR, USER1,
				"Buffer could not be read entirely\n");
			goto exit;
		}

		stream.avail_in = op->src.length;
		stream.next_in = single_src_buf;
		stream.avail_out = rte_pktmbuf_pkt_len(op->m_dst);
		stream.next_out = single_dst_buf;

	} else {
		stream.avail_in = op->src.length;
		stream.next_in = rte_pktmbuf_mtod(op->m_src, uint8_t *);
		stream.avail_out = op->m_dst->data_len;
		stream.next_out = rte_pktmbuf_mtod(op->m_dst, uint8_t *);
	}

	/* Stateless operation, all buffer will be compressed in one go */
	zlib_flush = map_zlib_flush_flag(op->flush_flag);
	ret = inflate(&stream, zlib_flush);

	if (stream.avail_in != 0) {
		RTE_LOG(ERR, USER1, "Buffer could not be read entirely\n");
		goto exit;
	}

	if (ret != Z_STREAM_END)
		goto exit;

	if (op->m_src->nb_segs > 1) {
		uint32_t remaining_data = stream.total_out;
		uint8_t *src_data = single_dst_buf;
		struct rte_mbuf *dst_buf = op->m_dst;

		while (remaining_data > 0) {
			uint8_t *dst_data = rte_pktmbuf_mtod(dst_buf,
					uint8_t *);
			/* Last segment */
			if (remaining_data < dst_buf->data_len) {
				memcpy(dst_data, src_data, remaining_data);
				remaining_data = 0;
			} else {
				memcpy(dst_data, src_data, dst_buf->data_len);
				remaining_data -= dst_buf->data_len;
				src_data += dst_buf->data_len;
				dst_buf = dst_buf->next;
			}
		}
	}

	op->consumed = stream.total_in;
	op->produced = stream.total_out;
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	inflateReset(&stream);

	ret = 0;
exit:
	inflateEnd(&stream);

	return ret;
}

static int
prepare_sgl_bufs(const char *test_buf, struct rte_mbuf *head_buf,
		uint32_t total_data_size,
		struct rte_mempool *small_mbuf_pool,
		struct rte_mempool *large_mbuf_pool,
		uint8_t limit_segs_in_sgl,
		uint16_t seg_size)
{
	uint32_t remaining_data = total_data_size;
	uint16_t num_remaining_segs = DIV_CEIL(remaining_data, seg_size);
	struct rte_mempool *pool;
	struct rte_mbuf *next_seg;
	uint32_t data_size;
	char *buf_ptr;
	const char *data_ptr = test_buf;
	uint16_t i;
	int ret;

	if (limit_segs_in_sgl != 0 && num_remaining_segs > limit_segs_in_sgl)
		num_remaining_segs = limit_segs_in_sgl - 1;

	/*
	 * Allocate data in the first segment (header) and
	 * copy data if test buffer is provided
	 */
	if (remaining_data < seg_size)
		data_size = remaining_data;
	else
		data_size = seg_size;

	buf_ptr = rte_pktmbuf_append(head_buf, data_size);
	if (buf_ptr == NULL) {
		RTE_LOG(ERR, USER1,
			"Not enough space in the 1st buffer\n");
		return -1;
	}

	if (data_ptr != NULL) {
		/* Copy characters without NULL terminator */
		memcpy(buf_ptr, data_ptr, data_size);
		data_ptr += data_size;
	}
	remaining_data -= data_size;
	num_remaining_segs--;

	/*
	 * Allocate the rest of the segments,
	 * copy the rest of the data and chain the segments.
	 */
	for (i = 0; i < num_remaining_segs; i++) {

		if (i == (num_remaining_segs - 1)) {
			/* last segment */
			if (remaining_data > seg_size)
				pool = large_mbuf_pool;
			else
				pool = small_mbuf_pool;
			data_size = remaining_data;
		} else {
			data_size = seg_size;
			pool = small_mbuf_pool;
		}

		next_seg = rte_pktmbuf_alloc(pool);
		if (next_seg == NULL) {
			RTE_LOG(ERR, USER1,
				"New segment could not be allocated "
				"from the mempool\n");
			return -1;
		}
		buf_ptr = rte_pktmbuf_append(next_seg, data_size);
		if (buf_ptr == NULL) {
			RTE_LOG(ERR, USER1,
				"Not enough space in the buffer\n");
			rte_pktmbuf_free(next_seg);
			return -1;
		}
		if (data_ptr != NULL) {
			/* Copy characters without NULL terminator */
			memcpy(buf_ptr, data_ptr, data_size);
			data_ptr += data_size;
		}
		remaining_data -= data_size;

		ret = rte_pktmbuf_chain(head_buf, next_seg);
		if (ret != 0) {
			rte_pktmbuf_free(next_seg);
			RTE_LOG(ERR, USER1,
				"Segment could not chained\n");
			return -1;
		}
	}

	return 0;
}

static void
extbuf_free_callback(void *addr __rte_unused, void *opaque __rte_unused)
{
}

static int
test_run_enqueue_dequeue(struct rte_comp_op **ops,
			 struct rte_comp_op **ops_processed,
			 unsigned int num_bufs)
{
	uint16_t num_enqd, num_deqd, num_total_deqd;
	unsigned int deqd_retries = 0;
	int res = 0;

	/* Enqueue and dequeue all operations */
	num_enqd = rte_compressdev_enqueue_burst(0, 0, ops, num_bufs);
	if (num_enqd < num_bufs) {
		RTE_LOG(ERR, USER1,
			"Some operations could not be enqueued\n");
		res = -1;
	}

	/* dequeue ops even on error (same number of ops as was enqueued) */

	num_total_deqd = 0;
	while (num_total_deqd < num_enqd) {
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
					"Not all operations could be dequeued\n");
				res = -1;
				break;
			}
			usleep(DEQUEUE_WAIT_TIME);
		}
		num_deqd = rte_compressdev_dequeue_burst(0, 0,
				&ops_processed[num_total_deqd], num_bufs);
		num_total_deqd += num_deqd;
		deqd_retries++;

	}

	return res;
}

/**
 * Arrays initialization. Input buffers preparation for compression.
 *
 * API that initializes all the private arrays to NULL
 * and allocates input buffers to perform compression operations.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_setup_com_bufs(const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		const struct test_private_arrays *test_priv_data)
{
	/* local variables: */
	unsigned int i;
	uint32_t data_size;
	char *buf_ptr;
	int ret;
	char **all_decomp_data = test_priv_data->all_decomp_data;

	struct comp_testsuite_params *ts_params = &testsuite_params;

	/* from int_data: */
	const char * const *test_bufs = int_data->test_bufs;
	unsigned int num_bufs = int_data->num_bufs;

	/* from test_data: */
	unsigned int buff_type = test_data->buff_type;
	unsigned int big_data = test_data->big_data;

	/* from test_priv_data: */
	struct rte_mbuf **uncomp_bufs = test_priv_data->uncomp_bufs;
	struct rte_mempool *buf_pool;

	static struct rte_mbuf_ext_shared_info inbuf_info;

	size_t array_size = sizeof(void *) * num_bufs;

	/* Initialize all arrays to NULL */
	memset(test_priv_data->uncomp_bufs, 0, array_size);
	memset(test_priv_data->comp_bufs, 0, array_size);
	memset(test_priv_data->ops, 0, array_size);
	memset(test_priv_data->ops_processed, 0, array_size);
	memset(test_priv_data->priv_xforms, 0, array_size);
	memset(test_priv_data->compressed_data_size,
	       0, sizeof(uint32_t) * num_bufs);

	if (test_data->decompress_state == RTE_COMP_OP_STATEFUL) {
		data_size = strlen(test_bufs[0]) + 1;
		*all_decomp_data = rte_malloc(NULL, data_size,
					     RTE_CACHE_LINE_SIZE);
	}

	if (big_data)
		buf_pool = ts_params->big_mbuf_pool;
	else if (buff_type == SGL_BOTH)
		buf_pool = ts_params->small_mbuf_pool;
	else
		buf_pool = ts_params->large_mbuf_pool;

	/* for compression uncomp_bufs is used as a source buffer */
	/* allocation from buf_pool (mempool type) */
	ret = rte_pktmbuf_alloc_bulk(buf_pool,
				uncomp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Source mbufs could not be allocated "
			"from the mempool\n");
		return -1;
	}

	if (test_data->use_external_mbufs) {
		inbuf_info.free_cb = extbuf_free_callback;
		inbuf_info.fcb_opaque = NULL;
		rte_mbuf_ext_refcnt_set(&inbuf_info, 1);
		for (i = 0; i < num_bufs; i++) {
			rte_pktmbuf_attach_extbuf(uncomp_bufs[i],
					test_data->inbuf_memzone->addr,
					test_data->inbuf_memzone->iova,
					test_data->inbuf_data_size,
					&inbuf_info);
			buf_ptr = rte_pktmbuf_append(uncomp_bufs[i],
					test_data->inbuf_data_size);
			if (buf_ptr == NULL) {
				RTE_LOG(ERR, USER1,
					"Append extra bytes to the source mbuf failed\n");
				return -1;
			}
		}
	} else if (buff_type == SGL_BOTH || buff_type == SGL_TO_LB) {
		for (i = 0; i < num_bufs; i++) {
			data_size = strlen(test_bufs[i]) + 1;
			if (prepare_sgl_bufs(test_bufs[i], uncomp_bufs[i],
			    data_size,
			    big_data ? buf_pool : ts_params->small_mbuf_pool,
			    big_data ? buf_pool : ts_params->large_mbuf_pool,
			    big_data ? 0 : MAX_SEGS,
			    big_data ? MAX_DATA_MBUF_SIZE : SMALL_SEG_SIZE) < 0)
				return -1;
		}
	} else {
		for (i = 0; i < num_bufs; i++) {
			data_size = strlen(test_bufs[i]) + 1;

			buf_ptr = rte_pktmbuf_append(uncomp_bufs[i], data_size);
			if (buf_ptr == NULL) {
				RTE_LOG(ERR, USER1,
					"Append extra bytes to the source mbuf failed\n");
				return -1;
			}
			strlcpy(buf_ptr, test_bufs[i], data_size);
		}
	}

	return 0;
}

/**
 * Data size calculation (for both compression and decompression).
 *
 * Calculate size of anticipated output buffer required for both
 * compression and decompression operations based on input int_data.
 *
 * @param op_type
 *   Operation type: compress or decompress
 * @param out_of_space_and_zlib
 *   Boolean value to switch into "out of space" buffer if set.
 *   To test "out-of-space" data size, zlib_decompress must be set as well.
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param i
 *   current buffer index
 * @return
 *   data size
 */
static inline uint32_t
test_mbufs_calculate_data_size(
		enum operation_type op_type,
		unsigned int out_of_space_and_zlib,
		const struct test_private_arrays *test_priv_data,
		const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		unsigned int i)
{
	/* local variables: */
	uint32_t data_size;
	struct priv_op_data *priv_data;
	float ratio_val;
	enum ratio_switch ratio = test_data->ratio;

	uint8_t not_zlib_compr; /* true if zlib isn't current compression dev */
	enum overflow_test overflow = test_data->overflow;

	/* from test_priv_data: */
	struct rte_comp_op **ops_processed = test_priv_data->ops_processed;

	/* from int_data: */
	const char * const *test_bufs = int_data->test_bufs;

	if (out_of_space_and_zlib)
		data_size = OUT_OF_SPACE_BUF;
	else {
		if (op_type == OPERATION_COMPRESSION) {
			not_zlib_compr = (test_data->zlib_dir == ZLIB_DECOMPRESS
				|| test_data->zlib_dir == ZLIB_NONE);

			ratio_val = (ratio == RATIO_ENABLED) ?
					COMPRESS_BUF_SIZE_RATIO :
					COMPRESS_BUF_SIZE_RATIO_DISABLED;

			ratio_val = (not_zlib_compr &&
				(overflow == OVERFLOW_ENABLED)) ?
				COMPRESS_BUF_SIZE_RATIO_OVERFLOW :
				ratio_val;

			data_size = strlen(test_bufs[i]) * ratio_val;
		} else {
			priv_data = (struct priv_op_data *)
					(ops_processed[i] + 1);
			data_size = strlen(test_bufs[priv_data->orig_idx]) + 1;
		}
	}

	return data_size;
}


/**
 * Memory buffers preparation (for both compression and decompression).
 *
 * Function allocates output buffers to perform compression
 * or decompression operations depending on value of op_type.
 *
 * @param op_type
 *   Operation type: compress or decompress
 * @param out_of_space_and_zlib
 *   Boolean value to switch into "out of space" buffer if set.
 *   To test "out-of-space" data size, zlib_decompress must be set as well.
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param current_extbuf_info,
 *   The structure containing all the information related to external mbufs
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_setup_output_bufs(
		enum operation_type op_type,
		unsigned int out_of_space_and_zlib,
		const struct test_private_arrays *test_priv_data,
		const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		struct rte_mbuf_ext_shared_info *current_extbuf_info)
{
	/* local variables: */
	unsigned int i;
	uint32_t data_size;
	int ret;
	char *buf_ptr;

	/* from test_priv_data: */
	struct rte_mbuf **current_bufs;

	/* from int_data: */
	unsigned int num_bufs = int_data->num_bufs;

	/* from test_data: */
	unsigned int buff_type = test_data->buff_type;
	unsigned int big_data = test_data->big_data;
	const struct rte_memzone *current_memzone;

	struct comp_testsuite_params *ts_params = &testsuite_params;
	struct rte_mempool *buf_pool;

	if (big_data)
		buf_pool = ts_params->big_mbuf_pool;
	else if (buff_type == SGL_BOTH)
		buf_pool = ts_params->small_mbuf_pool;
	else
		buf_pool = ts_params->large_mbuf_pool;

	if (op_type == OPERATION_COMPRESSION)
		current_bufs = test_priv_data->comp_bufs;
	else
		current_bufs = test_priv_data->uncomp_bufs;

	/* the mbufs allocation*/
	ret = rte_pktmbuf_alloc_bulk(buf_pool, current_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Destination mbufs could not be allocated "
			"from the mempool\n");
		return -1;
	}

	if (test_data->use_external_mbufs) {
		current_extbuf_info->free_cb = extbuf_free_callback;
		current_extbuf_info->fcb_opaque = NULL;
		rte_mbuf_ext_refcnt_set(current_extbuf_info, 1);
		if (op_type == OPERATION_COMPRESSION)
			current_memzone = test_data->compbuf_memzone;
		else
			current_memzone = test_data->uncompbuf_memzone;

		for (i = 0; i < num_bufs; i++) {
			rte_pktmbuf_attach_extbuf(current_bufs[i],
					current_memzone->addr,
					current_memzone->iova,
					current_memzone->len,
					current_extbuf_info);
			rte_pktmbuf_append(current_bufs[i],
					current_memzone->len);
		}
	} else {
		for (i = 0; i < num_bufs; i++) {

			enum rte_comp_huffman comp_huffman =
			ts_params->def_comp_xform->compress.deflate.huffman;

			/* data size calculation */
			data_size = test_mbufs_calculate_data_size(
					op_type,
					out_of_space_and_zlib,
					test_priv_data,
					int_data,
					test_data,
					i);

			if (comp_huffman != RTE_COMP_HUFFMAN_DYNAMIC) {
				if (op_type == OPERATION_DECOMPRESSION)
					data_size *= COMPRESS_BUF_SIZE_RATIO;
			}

			/* data allocation */
			if (buff_type == SGL_BOTH || buff_type == LB_TO_SGL) {
				ret = prepare_sgl_bufs(NULL, current_bufs[i],
				      data_size,
				      big_data ? buf_pool :
						ts_params->small_mbuf_pool,
				      big_data ? buf_pool :
						ts_params->large_mbuf_pool,
				      big_data ? 0 : MAX_SEGS,
				      big_data ? MAX_DATA_MBUF_SIZE :
						 SMALL_SEG_SIZE);
				if (ret < 0)
					return -1;
			} else {
				buf_ptr = rte_pktmbuf_append(current_bufs[i],
						data_size);
				if (buf_ptr == NULL) {
					RTE_LOG(ERR, USER1,
						"Append extra bytes to the destination mbuf failed\n");
					return -1;
				}
			}
		}
	}

	return 0;
}

/**
 * The main compression function.
 *
 * Function performs compression operation.
 * Operation(s) configuration, depending on CLI parameters.
 * Operation(s) processing.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_deflate_comp_run(const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		const struct test_private_arrays *test_priv_data)
{
	/* local variables: */
	struct priv_op_data *priv_data;
	unsigned int i;
	uint16_t num_priv_xforms = 0;
	int ret;
	int ret_status = 0;
	char *buf_ptr;

	struct comp_testsuite_params *ts_params = &testsuite_params;

	/* from test_data: */
	enum rte_comp_op_type operation_type = test_data->compress_state;
	unsigned int zlib_compress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_COMPRESS);

	/* from int_data: */
	struct rte_comp_xform **compress_xforms = int_data->compress_xforms;
	unsigned int num_xforms = int_data->num_xforms;
	unsigned int num_bufs = int_data->num_bufs;

	/* from test_priv_data: */
	struct rte_mbuf **comp_bufs = test_priv_data->comp_bufs;
	struct rte_mbuf **uncomp_bufs = test_priv_data->uncomp_bufs;
	struct rte_comp_op **ops = test_priv_data->ops;
	struct rte_comp_op **ops_processed = test_priv_data->ops_processed;
	void **priv_xforms = test_priv_data->priv_xforms;

	const struct rte_compressdev_capabilities *capa =
		rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);

	/* Build the compression operations */
	ret = rte_comp_op_bulk_alloc(ts_params->op_pool, ops, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Compress operations could not be allocated "
			"from the mempool\n");
		ret_status = -1;
		goto exit;
	}

	for (i = 0; i < num_bufs; i++) {
		ops[i]->m_src = uncomp_bufs[i];
		ops[i]->m_dst = comp_bufs[i];
		ops[i]->src.offset = 0;
		ops[i]->src.length = rte_pktmbuf_pkt_len(uncomp_bufs[i]);
		ops[i]->dst.offset = 0;

		RTE_LOG(DEBUG, USER1,
				"Uncompressed buffer length = %u compressed buffer length = %u",
				rte_pktmbuf_pkt_len(uncomp_bufs[i]),
				rte_pktmbuf_pkt_len(comp_bufs[i]));

		if (operation_type == RTE_COMP_OP_STATELESS) {
			ops[i]->flush_flag = RTE_COMP_FLUSH_FINAL;
		} else {
			RTE_LOG(ERR, USER1,
				"Compression: stateful operations are not "
				"supported in these tests yet\n");
			ret_status = -1;
			goto exit;
		}
		ops[i]->input_chksum = 0;
		/*
		 * Store original operation index in private data,
		 * since ordering does not have to be maintained,
		 * when dequeuing from compressdev, so a comparison
		 * at the end of the test can be done.
		 */
		priv_data = (struct priv_op_data *) (ops[i] + 1);
		priv_data->orig_idx = i;
	}

	/* Compress data (either with Zlib API or compressdev API */
	if (zlib_compress) {
		for (i = 0; i < num_bufs; i++) {
			const struct rte_comp_xform *compress_xform =
				compress_xforms[i % num_xforms];
			ret = compress_zlib(ops[i], compress_xform,
					DEFAULT_MEM_LEVEL);
			if (ret < 0) {
				ret_status = -1;
				goto exit;
			}

			ops_processed[i] = ops[i];
		}
	} else {
		/* Create compress private xform data */
		for (i = 0; i < num_xforms; i++) {
			ret = rte_compressdev_private_xform_create(0,
				(const struct rte_comp_xform *)
					compress_xforms[i],
				&priv_xforms[i]);
			if (ret < 0) {
				RTE_LOG(ERR, USER1,
					"Compression private xform "
					"could not be created\n");
				ret_status = -1;
				goto exit;
			}
			num_priv_xforms++;
		}
		if (capa->comp_feature_flags &
				RTE_COMP_FF_SHAREABLE_PRIV_XFORM) {
			/* Attach shareable private xform data to ops */
			for (i = 0; i < num_bufs; i++)
				ops[i]->private_xform =
						priv_xforms[i % num_xforms];
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
					ret_status = -1;
					goto exit;
				}
				num_priv_xforms++;
			}
			/* Attach non shareable private xform data to ops */
			for (i = 0; i < num_bufs; i++)
				ops[i]->private_xform = priv_xforms[i];
		}

recovery_lb:
		ret = test_run_enqueue_dequeue(ops, ops_processed, num_bufs);
		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Compression: enqueue/dequeue operation failed\n");
			ret_status = -1;
			goto exit;
		}

		for (i = 0; i < num_bufs; i++) {
			test_priv_data->compressed_data_size[i] +=
					ops_processed[i]->produced;

			if (ops_processed[i]->status ==
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE) {

				ops[i]->status =
					RTE_COMP_OP_STATUS_NOT_PROCESSED;
				ops[i]->src.offset +=
					ops_processed[i]->consumed;
				ops[i]->src.length -=
					ops_processed[i]->consumed;
				ops[i]->dst.offset +=
					ops_processed[i]->produced;

				buf_ptr = rte_pktmbuf_append(
					ops[i]->m_dst,
					ops_processed[i]->produced);

				if (buf_ptr == NULL) {
					RTE_LOG(ERR, USER1,
						"Data recovery: append extra bytes to the current mbuf failed\n");
					ret_status = -1;
					goto exit;
				}
				goto recovery_lb;
			}
		}
	}

exit:
	/* Free resources */
	if (ret_status < 0)
		for (i = 0; i < num_bufs; i++) {
			rte_comp_op_free(ops[i]);
			ops[i] = NULL;
			ops_processed[i] = NULL;
		}

	/* Free compress private xforms */
	for (i = 0; i < num_priv_xforms; i++) {
		if (priv_xforms[i] != NULL) {
			rte_compressdev_private_xform_free(0, priv_xforms[i]);
			priv_xforms[i] = NULL;
		}
	}

	return ret_status;
}

/**
 * Prints out the test report. Memory freeing.
 *
 * Called after successful compression.
 * Operation(s) status validation and decompression buffers freeing.

 * -1 returned if function fail.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @return
 *   - 2: Some operation is not supported
 *   - 1: Decompression should be skipped
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_deflate_comp_finalize(const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		const struct test_private_arrays *test_priv_data)
{
	/* local variables: */
	unsigned int i;
	struct priv_op_data *priv_data;

	/* from int_data: */
	unsigned int num_xforms = int_data->num_xforms;
	struct rte_comp_xform **compress_xforms = int_data->compress_xforms;
	unsigned int num_bufs = int_data->num_bufs;

	/* from test_priv_data: */
	struct rte_comp_op **ops_processed = test_priv_data->ops_processed;
	uint64_t *compress_checksum = test_priv_data->compress_checksum;
	struct rte_mbuf **uncomp_bufs = test_priv_data->uncomp_bufs;
	struct rte_comp_op **ops = test_priv_data->ops;

	/* from test_data: */
	unsigned int out_of_space = test_data->out_of_space;
	unsigned int zlib_compress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_COMPRESS);
	unsigned int zlib_decompress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_DECOMPRESS);

	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		uint16_t xform_idx = priv_data->orig_idx % num_xforms;
		const struct rte_comp_compress_xform *compress_xform =
				&compress_xforms[xform_idx]->compress;
		enum rte_comp_huffman huffman_type =
			compress_xform->deflate.huffman;
		char engine[] = "zlib (directly, not PMD)";
		if (zlib_decompress)
			strlcpy(engine, "PMD", sizeof(engine));

		RTE_LOG(DEBUG, USER1, "Buffer %u compressed by %s from %u to"
			" %u bytes (level = %d, huffman = %s)\n",
			i, engine,
			ops_processed[i]->consumed, ops_processed[i]->produced,
			compress_xform->level,
			huffman_type_strings[huffman_type]);
		RTE_LOG(DEBUG, USER1, "Compression ratio = %.2f\n",
			ops_processed[i]->consumed == 0 ? 0 :
			(float)ops_processed[i]->produced /
			ops_processed[i]->consumed * 100);
		if (compress_xform->chksum != RTE_COMP_CHECKSUM_NONE)
			compress_checksum[i] = ops_processed[i]->output_chksum;
		ops[i] = NULL;
	}

	/*
	 * Check operation status and free source mbufs (destination mbuf and
	 * compress operation information is needed for the decompression stage)
	 */
	for (i = 0; i < num_bufs; i++) {
		if (out_of_space && !zlib_compress) {
			if (ops_processed[i]->status !=
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED) {
				RTE_LOG(ERR, USER1,
					"Operation without expected out of "
					"space status error\n");
				return -1;
			} else
				continue;
		}

		if (ops_processed[i]->status != RTE_COMP_OP_STATUS_SUCCESS) {
			if (test_data->overflow == OVERFLOW_ENABLED) {
				if (ops_processed[i]->status ==
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED) {
					RTE_LOG(INFO, USER1,
					"Out-of-space-recoverable functionality"
					" is not supported on this device\n");
					return 2;
				}
			}

			RTE_LOG(ERR, USER1,
				"Comp: Some operations were not successful\n");
			return -1;
		}
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_free(uncomp_bufs[priv_data->orig_idx]);
		uncomp_bufs[priv_data->orig_idx] = NULL;
	}

	if (out_of_space && !zlib_compress)
		return 1;

	return 0;
}

/**
 * The main decompression function.
 *
 * Function performs decompression operation.
 * Operation(s) configuration, depending on CLI parameters.
 * Operation(s) processing.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_deflate_decomp_run(const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		struct test_private_arrays *test_priv_data)
{

	/* local variables: */
	struct priv_op_data *priv_data;
	unsigned int i;
	uint16_t num_priv_xforms = 0;
	int ret;
	int ret_status = 0;

	struct comp_testsuite_params *ts_params = &testsuite_params;

	/* from test_data: */
	enum rte_comp_op_type operation_type = test_data->decompress_state;
	unsigned int zlib_decompress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_DECOMPRESS);

	/* from int_data: */
	struct rte_comp_xform **decompress_xforms = int_data->decompress_xforms;
	unsigned int num_xforms = int_data->num_xforms;
	unsigned int num_bufs = int_data->num_bufs;

	/* from test_priv_data: */
	struct rte_mbuf **uncomp_bufs = test_priv_data->uncomp_bufs;
	struct rte_mbuf **comp_bufs = test_priv_data->comp_bufs;
	struct rte_comp_op **ops = test_priv_data->ops;
	struct rte_comp_op **ops_processed = test_priv_data->ops_processed;
	void **priv_xforms = test_priv_data->priv_xforms;
	uint32_t *compressed_data_size = test_priv_data->compressed_data_size;
	void **stream = test_priv_data->stream;

	const struct rte_compressdev_capabilities *capa =
		rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);

	ret = rte_comp_op_bulk_alloc(ts_params->op_pool, ops, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Decompress operations could not be allocated "
			"from the mempool\n");
		ret_status = -1;
		goto exit;
	}

	/* Source buffer is the compressed data from the previous operations */
	for (i = 0; i < num_bufs; i++) {
		ops[i]->m_src = comp_bufs[i];
		ops[i]->m_dst = uncomp_bufs[i];
		ops[i]->src.offset = 0;
		/*
		 * Set the length of the compressed data to the
		 * number of bytes that were produced in the previous stage
		 */

		if (compressed_data_size[i])
			ops[i]->src.length = compressed_data_size[i];
		else
			ops[i]->src.length = ops_processed[i]->produced;

		ops[i]->dst.offset = 0;

		if (operation_type == RTE_COMP_OP_STATELESS) {
			ops[i]->flush_flag = RTE_COMP_FLUSH_FINAL;
			ops[i]->op_type = RTE_COMP_OP_STATELESS;
		} else if (!zlib_decompress) {
			ops[i]->flush_flag = RTE_COMP_FLUSH_SYNC;
			ops[i]->op_type = RTE_COMP_OP_STATEFUL;
		} else {
			RTE_LOG(ERR, USER1,
				"Decompression: stateful operations are"
				" not supported in these tests yet\n");
			ret_status = -1;
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
	 * as they are not needed anymore
	 */
	rte_comp_op_bulk_free(ops_processed, num_bufs);

	/* Decompress data (either with Zlib API or compressdev API */
	if (zlib_decompress) {
		for (i = 0; i < num_bufs; i++) {
			priv_data = (struct priv_op_data *)(ops[i] + 1);
			uint16_t xform_idx = priv_data->orig_idx % num_xforms;
			const struct rte_comp_xform *decompress_xform =
				decompress_xforms[xform_idx];

			ret = decompress_zlib(ops[i], decompress_xform);
			if (ret < 0) {
				ret_status = -1;
				goto exit;
			}

			ops_processed[i] = ops[i];
		}
	} else {
		if (operation_type == RTE_COMP_OP_STATELESS) {
			/* Create decompress private xform data */
			for (i = 0; i < num_xforms; i++) {
				ret = rte_compressdev_private_xform_create(0,
					(const struct rte_comp_xform *)
					decompress_xforms[i],
					&priv_xforms[i]);
				if (ret < 0) {
					RTE_LOG(ERR, USER1,
						"Decompression private xform "
						"could not be created\n");
					ret_status = -1;
					goto exit;
				}
				num_priv_xforms++;
			}

			if (capa->comp_feature_flags &
					RTE_COMP_FF_SHAREABLE_PRIV_XFORM) {
				/* Attach shareable private xform data to ops */
				for (i = 0; i < num_bufs; i++) {
					priv_data = (struct priv_op_data *)
							(ops[i] + 1);
					uint16_t xform_idx =
					       priv_data->orig_idx % num_xforms;
					ops[i]->private_xform =
							priv_xforms[xform_idx];
				}
			} else {
				/* Create rest of the private xforms */
				/* for the other ops */
				for (i = num_xforms; i < num_bufs; i++) {
					ret =
					 rte_compressdev_private_xform_create(0,
					      decompress_xforms[i % num_xforms],
					      &priv_xforms[i]);
					if (ret < 0) {
						RTE_LOG(ERR, USER1,
							"Decompression private xform"
							" could not be created\n");
						ret_status = -1;
						goto exit;
					}
					num_priv_xforms++;
				}

				/* Attach non shareable private xform data */
				/* to ops */
				for (i = 0; i < num_bufs; i++) {
					priv_data = (struct priv_op_data *)
							(ops[i] + 1);
					uint16_t xform_idx =
							priv_data->orig_idx;
					ops[i]->private_xform =
							priv_xforms[xform_idx];
				}
			}
		} else {
			/* Create a stream object for stateful decompression */
			ret = rte_compressdev_stream_create(0,
					decompress_xforms[0], stream);
			if (ret < 0) {
				RTE_LOG(ERR, USER1,
					"Decompression stream could not be created, error %d\n",
					ret);
				ret_status = -1;
				goto exit;
			}
			/* Attach stream to ops */
			for (i = 0; i < num_bufs; i++)
				ops[i]->stream = *stream;
		}

		test_priv_data->num_priv_xforms = num_priv_xforms;
	}

exit:
	return ret_status;
}

/**
 * Prints out the test report. Memory freeing.
 *
 * Called after successful decompression.
 * Operation(s) status validation and compression buffers freeing.

 * -1 returned if function fail.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @return
 *   - 2: Next step must be executed by the caller (stateful decompression only)
 *   - 1: On success (caller should stop and exit)
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_deflate_decomp_finalize(const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		const struct test_private_arrays *test_priv_data)
{
	/* local variables: */
	unsigned int i;
	struct priv_op_data *priv_data;
	static unsigned int step;

	/* from int_data: */
	unsigned int num_bufs = int_data->num_bufs;
	const char * const *test_bufs = int_data->test_bufs;
	struct rte_comp_xform **compress_xforms = int_data->compress_xforms;

	/* from test_priv_data: */
	struct rte_comp_op **ops_processed = test_priv_data->ops_processed;
	struct rte_mbuf **comp_bufs = test_priv_data->comp_bufs;
	struct rte_comp_op **ops = test_priv_data->ops;
	uint64_t *compress_checksum = test_priv_data->compress_checksum;
	unsigned int *decomp_produced_data_size =
			test_priv_data->decomp_produced_data_size;
	char **all_decomp_data = test_priv_data->all_decomp_data;

	/* from test_data: */
	unsigned int out_of_space = test_data->out_of_space;
	enum rte_comp_op_type operation_type = test_data->decompress_state;

	unsigned int zlib_compress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_COMPRESS);
	unsigned int zlib_decompress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_DECOMPRESS);

	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		char engine[] = "zlib, (directly, no PMD)";
		if (zlib_compress)
			strlcpy(engine, "pmd", sizeof(engine));
		RTE_LOG(DEBUG, USER1,
			"Buffer %u decompressed by %s from %u to %u bytes\n",
			i, engine,
			ops_processed[i]->consumed, ops_processed[i]->produced);
		ops[i] = NULL;
	}

	/*
	 * Check operation status and free source mbuf (destination mbuf and
	 * compress operation information is still needed)
	 */
	for (i = 0; i < num_bufs; i++) {
		if (out_of_space && !zlib_decompress) {
			if (ops_processed[i]->status !=
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED) {

				RTE_LOG(ERR, USER1,
					"Operation without expected out of "
					"space status error\n");
				return -1;
			} else
				continue;
		}

		if (operation_type == RTE_COMP_OP_STATEFUL
			&& (ops_processed[i]->status ==
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE
			    || ops_processed[i]->status ==
				RTE_COMP_OP_STATUS_SUCCESS)) {

			RTE_LOG(DEBUG, USER1,
					".............RECOVERABLE\n");

			/* collect the output into all_decomp_data */
			const void *ptr = rte_pktmbuf_read(
					ops_processed[i]->m_dst,
					ops_processed[i]->dst.offset,
					ops_processed[i]->produced,
					*all_decomp_data +
						*decomp_produced_data_size);
			if (ptr != *all_decomp_data +
					*decomp_produced_data_size)
				rte_memcpy(*all_decomp_data +
					   *decomp_produced_data_size,
					   ptr, ops_processed[i]->produced);

			*decomp_produced_data_size +=
					ops_processed[i]->produced;
			if (ops_processed[i]->src.length >
					ops_processed[i]->consumed) {
				if (ops_processed[i]->status ==
						RTE_COMP_OP_STATUS_SUCCESS) {
					RTE_LOG(ERR, USER1,
					      "Operation finished too early\n");
					return -1;
				}
				step++;
				if (step >= test_data->decompress_steps_max) {
					RTE_LOG(ERR, USER1,
					  "Operation exceeded maximum steps\n");
					return -1;
				}
				ops[i] = ops_processed[i];
				ops[i]->status =
					       RTE_COMP_OP_STATUS_NOT_PROCESSED;
				ops[i]->src.offset +=
						ops_processed[i]->consumed;
				ops[i]->src.length -=
						ops_processed[i]->consumed;
				/* repeat the operation */
				return 2;
			} else {
				/* Compare the original stream with the */
				/* decompressed stream (in size and the data) */
				priv_data = (struct priv_op_data *)
						(ops_processed[i] + 1);
				const char *buf1 =
						test_bufs[priv_data->orig_idx];
				const char *buf2 = *all_decomp_data;

				if (compare_buffers(buf1, strlen(buf1) + 1,
					  buf2, *decomp_produced_data_size) < 0)
					return -1;
				/* Test checksums */
				if (compress_xforms[0]->compress.chksum
						!= RTE_COMP_CHECKSUM_NONE) {
					if (ops_processed[i]->output_chksum
						      != compress_checksum[i]) {
						RTE_LOG(ERR, USER1,
			"The checksums differ\n"
			"Compression Checksum: %" PRIu64 "\tDecompression "
			"Checksum: %" PRIu64 "\n", compress_checksum[i],
					       ops_processed[i]->output_chksum);
						return -1;
					}
				}
			}
		} else if (ops_processed[i]->status !=
			   RTE_COMP_OP_STATUS_SUCCESS) {
			RTE_LOG(ERR, USER1,
					"Decomp: Some operations were not successful, status = %u\n",
					ops_processed[i]->status);
			return -1;
		}
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_free(comp_bufs[priv_data->orig_idx]);
		comp_bufs[priv_data->orig_idx] = NULL;
	}

	if (out_of_space && !zlib_decompress)
		return 1;

	return 0;
}

/**
 * Validation of the output (compression/decompression) data.
 *
 * The function compares the source stream with the output stream,
 * after decompression, to check if compression/decompression
 * was correct.
 * -1 returned if function fail.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @param test_priv_data
 *   A container used for aggregation all the private test arrays.
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static int
test_results_validation(const struct interim_data_params *int_data,
		const struct test_data_params *test_data,
		const struct test_private_arrays *test_priv_data)
{
	/* local variables: */
	unsigned int i;
	struct priv_op_data *priv_data;
	const char *buf1;
	const char *buf2;
	char *contig_buf = NULL;
	uint32_t data_size;

	/* from int_data: */
	struct rte_comp_xform **compress_xforms = int_data->compress_xforms;
	unsigned int num_bufs = int_data->num_bufs;
	const char * const *test_bufs = int_data->test_bufs;

	/* from test_priv_data: */
	uint64_t *compress_checksum = test_priv_data->compress_checksum;
	struct rte_comp_op **ops_processed = test_priv_data->ops_processed;

	/*
	 * Compare the original stream with the decompressed stream
	 * (in size and the data)
	 */
	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		buf1 = test_data->use_external_mbufs ?
				test_data->inbuf_memzone->addr :
				test_bufs[priv_data->orig_idx];
		data_size = test_data->use_external_mbufs ?
				test_data->inbuf_data_size :
				strlen(buf1) + 1;

		contig_buf = rte_malloc(NULL, ops_processed[i]->produced, 0);
		if (contig_buf == NULL) {
			RTE_LOG(ERR, USER1, "Contiguous buffer could not "
					"be allocated\n");
			goto exit;
		}

		buf2 = rte_pktmbuf_read(ops_processed[i]->m_dst, 0,
				ops_processed[i]->produced, contig_buf);
		if (compare_buffers(buf1, data_size,
				buf2, ops_processed[i]->produced) < 0)
			goto exit;

		/* Test checksums */
		if (compress_xforms[0]->compress.chksum !=
				RTE_COMP_CHECKSUM_NONE) {
			if (ops_processed[i]->output_chksum !=
					compress_checksum[i]) {
				RTE_LOG(ERR, USER1, "The checksums differ\n"
			"Compression Checksum: %" PRIu64 "\tDecompression "
			"Checksum: %" PRIu64 "\n", compress_checksum[i],
			ops_processed[i]->output_chksum);
				goto exit;
			}
		}

		rte_free(contig_buf);
		contig_buf = NULL;
	}
	return 0;

exit:
	rte_free(contig_buf);
	return -1;
}

/**
 * Compresses and decompresses input stream with compressdev API and Zlib API
 *
 * Basic test function. Common for all the functional tests.
 * -1 returned if function fail.
 *
 * @param int_data
 *   Interim data containing session/transformation objects.
 * @param test_data
 *   The test parameters set by users (command line parameters).
 * @return
 *   - 1: Some operation not supported
 *   - 0: On success.
 *   - -1: On error.
 */

static int
test_deflate_comp_decomp(const struct interim_data_params *int_data,
		const struct test_data_params *test_data)
{
	unsigned int num_bufs = int_data->num_bufs;
	unsigned int out_of_space = test_data->out_of_space;

	void *stream = NULL;
	char *all_decomp_data = NULL;
	unsigned int decomp_produced_data_size = 0;

	int ret_status = -1;
	int ret;
	struct rte_mbuf *uncomp_bufs[num_bufs];
	struct rte_mbuf *comp_bufs[num_bufs];
	struct rte_comp_op *ops[num_bufs];
	struct rte_comp_op *ops_processed[num_bufs];
	void *priv_xforms[num_bufs];
	unsigned int i;

	uint64_t compress_checksum[num_bufs];
	uint32_t compressed_data_size[num_bufs];
	char *contig_buf = NULL;

	struct rte_mbuf_ext_shared_info compbuf_info;
	struct rte_mbuf_ext_shared_info decompbuf_info;

	const struct rte_compressdev_capabilities *capa;

	/* Compressing with CompressDev */
	unsigned int zlib_compress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_COMPRESS);
	unsigned int zlib_decompress =
			(test_data->zlib_dir == ZLIB_ALL ||
			test_data->zlib_dir == ZLIB_DECOMPRESS);

	struct test_private_arrays test_priv_data;

	test_priv_data.uncomp_bufs = uncomp_bufs;
	test_priv_data.comp_bufs = comp_bufs;
	test_priv_data.ops = ops;
	test_priv_data.ops_processed = ops_processed;
	test_priv_data.priv_xforms = priv_xforms;
	test_priv_data.compress_checksum = compress_checksum;
	test_priv_data.compressed_data_size = compressed_data_size;

	test_priv_data.stream = &stream;
	test_priv_data.all_decomp_data = &all_decomp_data;
	test_priv_data.decomp_produced_data_size = &decomp_produced_data_size;

	test_priv_data.num_priv_xforms = 0; /* it's used for decompression only */

	capa = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	if (capa == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress device does not support DEFLATE\n");
		return -1;
	}

	/* Prepare the source mbufs with the data */
	ret = test_setup_com_bufs(int_data, test_data, &test_priv_data);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	}

	RTE_LOG(DEBUG, USER1, "<<< COMPRESSION >>>\n");

/* COMPRESSION  */

	/* Prepare output (destination) mbufs for compressed data */
	ret = test_setup_output_bufs(
			OPERATION_COMPRESSION,
			out_of_space == 1 && !zlib_compress,
			&test_priv_data,
			int_data,
			test_data,
			&compbuf_info);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	}

	/* Run compression */
	ret = test_deflate_comp_run(int_data, test_data, &test_priv_data);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	}

	ret = test_deflate_comp_finalize(int_data, test_data, &test_priv_data);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	} else if (ret == 1) {
		ret_status = 0;
		goto exit;
	} else if (ret == 2) {
		ret_status = 1;	 /* some operation not supported */
		goto exit;
	}

/* DECOMPRESSION  */

	RTE_LOG(DEBUG, USER1, "<<< DECOMPRESSION >>>\n");

	/* Prepare output (destination) mbufs for decompressed data */
	ret = test_setup_output_bufs(
			OPERATION_DECOMPRESSION,
			out_of_space == 1 && !zlib_decompress,
			&test_priv_data,
			int_data,
			test_data,
			&decompbuf_info);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	}

	/* Run decompression */
	ret = test_deflate_decomp_run(int_data, test_data, &test_priv_data);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	}

	if (!zlib_decompress) {
next_step:	/* next step for stateful decompression only */
		ret = test_run_enqueue_dequeue(ops, ops_processed, num_bufs);
		if (ret < 0) {
			ret_status = -1;
			RTE_LOG(ERR, USER1,
				"Decompression: enqueue/dequeue operation failed\n");
		}
	}

	ret = test_deflate_decomp_finalize(int_data, test_data, &test_priv_data);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	} else if (ret == 1) {
		ret_status = 0;
		goto exit;
	} else if (ret == 2) {
		goto next_step;
	}

/* FINAL PROCESSING  */

	ret = test_results_validation(int_data, test_data, &test_priv_data);
	if (ret < 0) {
		ret_status = -1;
		goto exit;
	}
	ret_status = 0;

exit:
	/* Free resources */

	if (stream != NULL)
		rte_compressdev_stream_free(0, stream);
	if (all_decomp_data != NULL)
		rte_free(all_decomp_data);

	/* Free compress private xforms */
	for (i = 0; i < test_priv_data.num_priv_xforms; i++) {
		if (priv_xforms[i] != NULL) {
			rte_compressdev_private_xform_free(0, priv_xforms[i]);
			priv_xforms[i] = NULL;
		}
	}
	for (i = 0; i < num_bufs; i++) {
		rte_pktmbuf_free(uncomp_bufs[i]);
		rte_pktmbuf_free(comp_bufs[i]);
		rte_comp_op_free(ops[i]);
		rte_comp_op_free(ops_processed[i]);
	}
	rte_free(contig_buf);

	return ret_status;
}

static int
test_compressdev_deflate_stateless_fixed(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i;
	int ret;
	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_FIXED) == 0)
		return -ENOTSUP;

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

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&compress_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			goto exit;

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			goto exit;
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
	uint16_t i;
	int ret;
	struct rte_comp_xform *compress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);

	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if (compress_xform == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress xform could not be created\n");
		ret = TEST_FAILED;
		goto exit;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));
	compress_xform->compress.deflate.huffman = RTE_COMP_HUFFMAN_DYNAMIC;

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&compress_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			goto exit;

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			goto exit;
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
	int ret;

	for (i = 0; i < num_bufs; i++)
		buf_idx[i] = i;

	struct interim_data_params int_data = {
		compress_test_bufs,
		num_bufs,
		buf_idx,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	/* Compress with compressdev, decompress with Zlib */
	test_data.zlib_dir = ZLIB_DECOMPRESS;
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		return ret;

	/* Compress with Zlib, decompress with compressdev */
	test_data.zlib_dir = ZLIB_COMPRESS;
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		return ret;

	return TEST_SUCCESS;
}

static int
test_compressdev_deflate_stateless_multi_level(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
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

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&compress_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		for (level = RTE_COMP_LEVEL_MIN; level <= RTE_COMP_LEVEL_MAX;
				level++) {
			compress_xform->compress.level = level;
			/* Compress with compressdev, decompress with Zlib */
			test_data.zlib_dir = ZLIB_DECOMPRESS;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				goto exit;
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

	struct interim_data_params int_data = {
		test_buffers,
		num_bufs,
		buf_idx,
		compress_xforms,
		decompress_xforms,
		NUM_XFORMS
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	/* Compress with compressdev, decompress with Zlib */
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		goto exit;

	ret = TEST_SUCCESS;

exit:
	for (i = 0; i < NUM_XFORMS; i++) {
		rte_free(compress_xforms[i]);
		rte_free(decompress_xforms[i]);
	}

	return ret;
}

static int
test_compressdev_deflate_stateless_sgl(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i;
	int ret;
	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			return ret;

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			return ret;

		if (capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_LB_OUT) {
			/* Compress with compressdev, decompress with Zlib */
			test_data.zlib_dir = ZLIB_DECOMPRESS;
			test_data.buff_type = SGL_TO_LB;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				return ret;

			/* Compress with Zlib, decompress with compressdev */
			test_data.zlib_dir = ZLIB_COMPRESS;
			test_data.buff_type = SGL_TO_LB;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				return ret;
		}

		if (capab->comp_feature_flags & RTE_COMP_FF_OOP_LB_IN_SGL_OUT) {
			/* Compress with compressdev, decompress with Zlib */
			test_data.zlib_dir = ZLIB_DECOMPRESS;
			test_data.buff_type = LB_TO_SGL;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				return ret;

			/* Compress with Zlib, decompress with compressdev */
			test_data.zlib_dir = ZLIB_COMPRESS;
			test_data.buff_type = LB_TO_SGL;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				return ret;
		}
	}

	return TEST_SUCCESS;
}

static int
test_compressdev_deflate_stateless_checksum(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i;
	int ret;
	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	/* Check if driver supports any checksum */
	if ((capab->comp_feature_flags & RTE_COMP_FF_CRC32_CHECKSUM) == 0 &&
			(capab->comp_feature_flags &
			RTE_COMP_FF_ADLER32_CHECKSUM) == 0 &&
			(capab->comp_feature_flags &
			RTE_COMP_FF_CRC32_ADLER32_CHECKSUM) == 0)
		return -ENOTSUP;

	struct rte_comp_xform *compress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (compress_xform == NULL) {
		RTE_LOG(ERR, USER1, "Compress xform could not be created\n");
		return TEST_FAILED;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));

	struct rte_comp_xform *decompress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (decompress_xform == NULL) {
		RTE_LOG(ERR, USER1, "Decompress xform could not be created\n");
		rte_free(compress_xform);
		return TEST_FAILED;
	}

	memcpy(decompress_xform, ts_params->def_decomp_xform,
			sizeof(struct rte_comp_xform));

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&compress_xform,
		&decompress_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	/* Check if driver supports crc32 checksum and test */
	if ((capab->comp_feature_flags & RTE_COMP_FF_CRC32_CHECKSUM)) {
		compress_xform->compress.chksum = RTE_COMP_CHECKSUM_CRC32;
		decompress_xform->decompress.chksum = RTE_COMP_CHECKSUM_CRC32;

		for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
			/* Compress with compressdev, decompress with Zlib */
			int_data.test_bufs = &compress_test_bufs[i];
			int_data.buf_idx = &i;

			/* Generate zlib checksum and test against selected
			 * drivers decompression checksum
			 */
			test_data.zlib_dir = ZLIB_COMPRESS;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				goto exit;

			/* Generate compression and decompression
			 * checksum of selected driver
			 */
			test_data.zlib_dir = ZLIB_NONE;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				goto exit;
		}
	}

	/* Check if driver supports adler32 checksum and test */
	if ((capab->comp_feature_flags & RTE_COMP_FF_ADLER32_CHECKSUM)) {
		compress_xform->compress.chksum = RTE_COMP_CHECKSUM_ADLER32;
		decompress_xform->decompress.chksum = RTE_COMP_CHECKSUM_ADLER32;

		for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
			int_data.test_bufs = &compress_test_bufs[i];
			int_data.buf_idx = &i;

			/* Generate zlib checksum and test against selected
			 * drivers decompression checksum
			 */
			test_data.zlib_dir = ZLIB_COMPRESS;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				goto exit;
			/* Generate compression and decompression
			 * checksum of selected driver
			 */
			test_data.zlib_dir = ZLIB_NONE;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				goto exit;
		}
	}

	/* Check if driver supports combined crc and adler checksum and test */
	if ((capab->comp_feature_flags & RTE_COMP_FF_CRC32_ADLER32_CHECKSUM)) {
		compress_xform->compress.chksum =
				RTE_COMP_CHECKSUM_CRC32_ADLER32;
		decompress_xform->decompress.chksum =
				RTE_COMP_CHECKSUM_CRC32_ADLER32;

		for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
			int_data.test_bufs = &compress_test_bufs[i];
			int_data.buf_idx = &i;

			/* Generate compression and decompression
			 * checksum of selected driver
			 */
			test_data.zlib_dir = ZLIB_NONE;
			ret = test_deflate_comp_decomp(&int_data, &test_data);
			if (ret < 0)
				goto exit;
		}
	}

	ret = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	rte_free(decompress_xform);
	return ret;
}

static int
test_compressdev_out_of_space_buffer(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	int ret;
	uint16_t i;
	const struct rte_compressdev_capabilities *capab;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_FIXED) == 0)
		return -ENOTSUP;

	struct interim_data_params int_data = {
		&compress_test_bufs[0],
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 1,  /* run out-of-space test */
		.big_data = 0,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};
	/* Compress with compressdev, decompress with Zlib */
	test_data.zlib_dir = ZLIB_DECOMPRESS;
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		goto exit;

	/* Compress with Zlib, decompress with compressdev */
	test_data.zlib_dir = ZLIB_COMPRESS;
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		goto exit;

	if (capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) {
		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		test_data.buff_type = SGL_BOTH;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			goto exit;

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		test_data.buff_type = SGL_BOTH;
		ret = test_deflate_comp_decomp(&int_data, &test_data);
		if (ret < 0)
			goto exit;
	}

	ret  = TEST_SUCCESS;

exit:
	return ret;
}

static int
test_compressdev_deflate_stateless_dynamic_big(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret;
	unsigned int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, BIG_DATA_TEST_SIZE, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	struct interim_data_params int_data = {
		(const char * const *)&test_buffer,
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
						RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(BIG_DATA_TEST_SIZE);
	for (j = 0; j < BIG_DATA_TEST_SIZE - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;
	test_buffer[BIG_DATA_TEST_SIZE - 1] = 0;

	/* Compress with compressdev, decompress with Zlib */
	test_data.zlib_dir = ZLIB_DECOMPRESS;
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		goto exit;

	/* Compress with Zlib, decompress with compressdev */
	test_data.zlib_dir = ZLIB_COMPRESS;
	ret = test_deflate_comp_decomp(&int_data, &test_data);
	if (ret < 0)
		goto exit;

	ret = TEST_SUCCESS;

exit:
	ts_params->def_comp_xform->compress.deflate.huffman =
						RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_stateful_decomp(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	int ret;
	uint16_t i;
	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if (!(capab->comp_feature_flags & RTE_COMP_FF_STATEFUL_DECOMPRESSION))
		return -ENOTSUP;

	struct interim_data_params int_data = {
		&compress_test_bufs[0],
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATEFUL,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_COMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.decompress_output_block_size = 2000,
		.decompress_steps_max = 4,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	/* Compress with Zlib, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto exit;
	}

	if (capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) {
		/* Now test with SGL buffers */
		test_data.buff_type = SGL_BOTH;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
	}

	ret  = TEST_SUCCESS;

exit:
	return ret;
}

static int
test_compressdev_deflate_stateful_decomp_checksum(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	int ret;
	uint16_t i;
	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if (!(capab->comp_feature_flags & RTE_COMP_FF_STATEFUL_DECOMPRESSION))
		return -ENOTSUP;

	/* Check if driver supports any checksum */
	if (!(capab->comp_feature_flags &
	     (RTE_COMP_FF_CRC32_CHECKSUM | RTE_COMP_FF_ADLER32_CHECKSUM |
	      RTE_COMP_FF_CRC32_ADLER32_CHECKSUM)))
		return -ENOTSUP;

	struct rte_comp_xform *compress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (compress_xform == NULL) {
		RTE_LOG(ERR, USER1, "Compress xform could not be created\n");
		return TEST_FAILED;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
	       sizeof(struct rte_comp_xform));

	struct rte_comp_xform *decompress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (decompress_xform == NULL) {
		RTE_LOG(ERR, USER1, "Decompress xform could not be created\n");
		rte_free(compress_xform);
		return TEST_FAILED;
	}

	memcpy(decompress_xform, ts_params->def_decomp_xform,
	       sizeof(struct rte_comp_xform));

	struct interim_data_params int_data = {
		&compress_test_bufs[0],
		1,
		&i,
		&compress_xform,
		&decompress_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATEFUL,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_COMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.decompress_output_block_size = 2000,
		.decompress_steps_max = 4,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_ENABLED
	};

	/* Check if driver supports crc32 checksum and test */
	if (capab->comp_feature_flags & RTE_COMP_FF_CRC32_CHECKSUM) {
		compress_xform->compress.chksum = RTE_COMP_CHECKSUM_CRC32;
		decompress_xform->decompress.chksum = RTE_COMP_CHECKSUM_CRC32;
		/* Compress with Zlib, decompress with compressdev */
		test_data.buff_type = LB_BOTH;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
		if (capab->comp_feature_flags &
				RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) {
			/* Now test with SGL buffers */
			test_data.buff_type = SGL_BOTH;
			if (test_deflate_comp_decomp(&int_data,
						     &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
		}
	}

	/* Check if driver supports adler32 checksum and test */
	if (capab->comp_feature_flags & RTE_COMP_FF_ADLER32_CHECKSUM) {
		compress_xform->compress.chksum = RTE_COMP_CHECKSUM_ADLER32;
		decompress_xform->decompress.chksum = RTE_COMP_CHECKSUM_ADLER32;
		/* Compress with Zlib, decompress with compressdev */
		test_data.buff_type = LB_BOTH;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
		if (capab->comp_feature_flags &
				RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) {
			/* Now test with SGL buffers */
			test_data.buff_type = SGL_BOTH;
			if (test_deflate_comp_decomp(&int_data,
						     &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
		}
	}

	/* Check if driver supports combined crc and adler checksum and test */
	if (capab->comp_feature_flags & RTE_COMP_FF_CRC32_ADLER32_CHECKSUM) {
		compress_xform->compress.chksum =
				RTE_COMP_CHECKSUM_CRC32_ADLER32;
		decompress_xform->decompress.chksum =
				RTE_COMP_CHECKSUM_CRC32_ADLER32;
		/* Zlib doesn't support combined checksum */
		test_data.zlib_dir = ZLIB_NONE;
		/* Compress stateless, decompress stateful with compressdev */
		test_data.buff_type = LB_BOTH;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
		if (capab->comp_feature_flags &
				RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) {
			/* Now test with SGL buffers */
			test_data.buff_type = SGL_BOTH;
			if (test_deflate_comp_decomp(&int_data,
						     &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
		}
	}

	ret  = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	rte_free(decompress_xform);
	return ret;
}

static const struct rte_memzone *
make_memzone(const char *name, size_t size)
{
	unsigned int socket_id = rte_socket_id();
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *memzone;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "%s_%u", name, socket_id);
	memzone = rte_memzone_lookup(mz_name);
	if (memzone != NULL && memzone->len != size) {
		rte_memzone_free(memzone);
		memzone = NULL;
	}
	if (memzone == NULL) {
		memzone = rte_memzone_reserve_aligned(mz_name, size, socket_id,
				RTE_MEMZONE_IOVA_CONTIG, RTE_CACHE_LINE_SIZE);
		if (memzone == NULL)
			RTE_LOG(ERR, USER1, "Can't allocate memory zone %s",
				mz_name);
	}
	return memzone;
}

static int
test_compressdev_external_mbufs(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	size_t data_len = 0;
	uint16_t i;
	int ret = TEST_FAILED;

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++)
		data_len = RTE_MAX(data_len, strlen(compress_test_bufs[i]) + 1);

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.use_external_mbufs = 1,
		.inbuf_data_size = data_len,
		.inbuf_memzone = make_memzone("inbuf", data_len),
		.compbuf_memzone = make_memzone("compbuf", data_len *
						COMPRESS_BUF_SIZE_RATIO),
		.uncompbuf_memzone = make_memzone("decompbuf", data_len),
		.overflow = OVERFLOW_DISABLED
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		/* prepare input data */
		data_len = strlen(compress_test_bufs[i]) + 1;
		rte_memcpy(test_data.inbuf_memzone->addr, compress_test_bufs[i],
			   data_len);
		test_data.inbuf_data_size = data_len;
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
			goto exit;

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
			goto exit;
	}

	ret = TEST_SUCCESS;

exit:
	rte_memzone_free(test_data.inbuf_memzone);
	rte_memzone_free(test_data.compbuf_memzone);
	rte_memzone_free(test_data.uncompbuf_memzone);
	return ret;
}

static int
test_compressdev_deflate_stateless_fixed_oos_recoverable(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i;
	int ret;
	int comp_result;
	const struct rte_compressdev_capabilities *capab;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_FIXED) == 0)
		return -ENOTSUP;

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

	struct interim_data_params int_data = {
		NULL,
		1,
		NULL,
		&compress_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_DECOMPRESS,
		.out_of_space = 0,
		.big_data = 0,
		.overflow = OVERFLOW_ENABLED,
		.ratio = RATIO_ENABLED
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		comp_result = test_deflate_comp_decomp(&int_data, &test_data);
		if (comp_result < 0) {
			ret = TEST_FAILED;
			goto exit;
		} else if (comp_result > 0) {
			ret = -ENOTSUP;
			goto exit;
		}

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		comp_result = test_deflate_comp_decomp(&int_data, &test_data);
		if (comp_result < 0) {
			ret = TEST_FAILED;
			goto exit;
		} else if (comp_result > 0) {
			ret = -ENOTSUP;
			goto exit;
		}
	}

	ret = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_LB_1op(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_LB, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for 'im buffer' test\n");
		return TEST_FAILED;
	}

	struct interim_data_params int_data = {
		(const char * const *)&test_buffer,
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
				/* must be LB to SGL,
				 * input LB buffer reaches its maximum,
				 * if ratio 1.3 than another mbuf must be
				 * created and attached
				 */
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_LB);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_LB - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_LB_2ops_first(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[2];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_LB, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for 'im buffer' test\n");
		return TEST_FAILED;
	}

	test_buffers[0] = test_buffer;
	test_buffers[1] = compress_test_bufs[0];

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		2,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_LB);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_LB - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_LB_2ops_second(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[2];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_LB, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for 'im buffer' test\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		2,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_LB);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_LB - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_LB_3ops(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[3];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_LB, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for 'im buffer' test\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;
	test_buffers[2] = compress_test_bufs[1];

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		3,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_LB);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_LB - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_LB_4ops(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[4];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_LB, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for 'im buffer' test\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;
	test_buffers[2] = compress_test_bufs[1];
	test_buffers[3] = test_buffer;

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		4,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = LB_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_LB);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_LB - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}


static int
test_compressdev_deflate_im_buffers_SGL_1op(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_SGL, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	struct interim_data_params int_data = {
		(const char * const *)&test_buffer,
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_SGL);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_SGL - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_SGL_2ops_first(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[2];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_SGL, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	test_buffers[0] = test_buffer;
	test_buffers[1] = compress_test_bufs[0];

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		2,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_SGL);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_SGL - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_SGL_2ops_second(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[2];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_SGL, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		2,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_SGL);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_SGL - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_SGL_3ops(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[3];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_SGL, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;
	test_buffers[2] = compress_test_bufs[1];

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		3,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_SGL);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_SGL - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}


static int
test_compressdev_deflate_im_buffers_SGL_4ops(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[4];

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_SGL, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;
	test_buffers[2] = compress_test_bufs[1];
	test_buffers[3] = test_buffer;

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		4,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_SGL);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_SGL - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_SGL_over_1op(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_OVER, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	struct interim_data_params int_data = {
		(const char * const *)&test_buffer,
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_OVER);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_OVER - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_SUCCESS;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);

	return ret;
}


static int
test_compressdev_deflate_im_buffers_SGL_over_2ops_first(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[2];

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_OVER, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	test_buffers[0] = test_buffer;
	test_buffers[1] = compress_test_bufs[0];

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		2,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_OVER);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_OVER - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_SUCCESS;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
	return ret;
}

static int
test_compressdev_deflate_im_buffers_SGL_over_2ops_second(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i = 0;
	int ret = TEST_SUCCESS;
	int j;
	const struct rte_compressdev_capabilities *capab;
	char *test_buffer = NULL;
	const char *test_buffers[2];

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");

	capab = rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	TEST_ASSERT(capab != NULL, "Failed to retrieve device capabilities");

	if ((capab->comp_feature_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0)
		return -ENOTSUP;

	if ((capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0)
		return -ENOTSUP;

	test_buffer = rte_malloc(NULL, IM_BUF_DATA_TEST_SIZE_OVER, 0);
	if (test_buffer == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate buffer for big-data\n");
		return TEST_FAILED;
	}

	test_buffers[0] = compress_test_bufs[0];
	test_buffers[1] = test_buffer;

	struct interim_data_params int_data = {
		(const char * const *)test_buffers,
		2,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		.compress_state = RTE_COMP_OP_STATELESS,
		.decompress_state = RTE_COMP_OP_STATELESS,
		.buff_type = SGL_BOTH,
		.zlib_dir = ZLIB_NONE,
		.out_of_space = 0,
		.big_data = 1,
		.overflow = OVERFLOW_DISABLED,
		.ratio = RATIO_DISABLED
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(IM_BUF_DATA_TEST_SIZE_OVER);
	for (j = 0; j < IM_BUF_DATA_TEST_SIZE_OVER - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;

	/* Compress with compressdev, decompress with compressdev */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_SUCCESS;
		goto end;
	}

end:
	ts_params->def_comp_xform->compress.deflate.huffman =
			RTE_COMP_HUFFMAN_DEFAULT;
	rte_free(test_buffer);
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
			test_compressdev_deflate_stateless_dynamic_big),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_multi_op),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_multi_level),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_multi_xform),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_sgl),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateless_checksum),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_out_of_space_buffer),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateful_decomp),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_stateful_decomp_checksum),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_external_mbufs),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
		      test_compressdev_deflate_stateless_fixed_oos_recoverable),

		/* Positive test cases for IM buffer handling verification */
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_LB_1op),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_LB_2ops_first),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_LB_2ops_second),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_LB_3ops),

		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_LB_4ops),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_SGL_1op),

		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_SGL_2ops_first),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_SGL_2ops_second),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_SGL_3ops),
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_SGL_4ops),

		/* Negative test cases for IM buffer handling verification */

		/* For this test huge mempool is necessary.
		 * It tests one case:
		 * only one op containing big amount of data, so that
		 * number of requested descriptors higher than number
		 * of available descriptors (128)
		 */
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
			test_compressdev_deflate_im_buffers_SGL_over_1op),

		/* For this test huge mempool is necessary.
		 * 2 ops. First op contains big amount of data:
		 * number of requested descriptors higher than number
		 * of available descriptors (128), the second op is
		 * relatively small. In this case both ops are rejected
		 */
		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
		       test_compressdev_deflate_im_buffers_SGL_over_2ops_first),

		TEST_CASE_ST(generic_ut_setup, generic_ut_teardown,
		      test_compressdev_deflate_im_buffers_SGL_over_2ops_second),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_compressdev(void)
{
	return unit_test_suite_runner(&compressdev_testsuite);
}

REGISTER_TEST_COMMAND(compressdev_autotest, test_compressdev);
