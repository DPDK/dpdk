/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 - 2019 Intel Corporation
 */
#include <string.h>
#include <zlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

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
#define NUM_BIG_MBUFS 4
#define BIG_DATA_TEST_SIZE (MAX_DATA_MBUF_SIZE * NUM_BIG_MBUFS / 2)

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
	enum rte_comp_op_type state;
	enum varied_buff buff_type;
	enum zlib_direction zlib_dir;
	unsigned int out_of_space;
	unsigned int big_data;
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
		RTE_LOG(ERR, USER1, "Need at least one compress device\n");
		return TEST_FAILED;
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
		strncpy(buf_ptr, data_ptr, data_size);
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
			strncpy(buf_ptr, data_ptr, data_size);
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

/*
 * Compresses and decompresses buffer with compressdev API and Zlib API
 */
static int
test_deflate_comp_decomp(const struct interim_data_params *int_data,
		const struct test_data_params *test_data)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	const char * const *test_bufs = int_data->test_bufs;
	unsigned int num_bufs = int_data->num_bufs;
	uint16_t *buf_idx = int_data->buf_idx;
	struct rte_comp_xform **compress_xforms = int_data->compress_xforms;
	struct rte_comp_xform **decompress_xforms = int_data->decompress_xforms;
	unsigned int num_xforms = int_data->num_xforms;
	enum rte_comp_op_type state = test_data->state;
	unsigned int buff_type = test_data->buff_type;
	unsigned int out_of_space = test_data->out_of_space;
	unsigned int big_data = test_data->big_data;
	enum zlib_direction zlib_dir = test_data->zlib_dir;
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
	char *buf_ptr;
	unsigned int i;
	struct rte_mempool *buf_pool;
	uint32_t data_size;
	/* Compressing with CompressDev */
	unsigned int oos_zlib_decompress =
			(zlib_dir == ZLIB_NONE || zlib_dir == ZLIB_DECOMPRESS);
	/* Decompressing with CompressDev */
	unsigned int oos_zlib_compress =
			(zlib_dir == ZLIB_NONE || zlib_dir == ZLIB_COMPRESS);
	const struct rte_compressdev_capabilities *capa =
		rte_compressdev_capability_get(0, RTE_COMP_ALGO_DEFLATE);
	char *contig_buf = NULL;
	uint64_t compress_checksum[num_bufs];

	/* Initialize all arrays to NULL */
	memset(uncomp_bufs, 0, sizeof(struct rte_mbuf *) * num_bufs);
	memset(comp_bufs, 0, sizeof(struct rte_mbuf *) * num_bufs);
	memset(ops, 0, sizeof(struct rte_comp_op *) * num_bufs);
	memset(ops_processed, 0, sizeof(struct rte_comp_op *) * num_bufs);
	memset(priv_xforms, 0, sizeof(void *) * num_bufs);

	if (big_data)
		buf_pool = ts_params->big_mbuf_pool;
	else if (buff_type == SGL_BOTH)
		buf_pool = ts_params->small_mbuf_pool;
	else
		buf_pool = ts_params->large_mbuf_pool;

	/* Prepare the source mbufs with the data */
	ret = rte_pktmbuf_alloc_bulk(buf_pool,
				uncomp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Source mbufs could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	if (buff_type == SGL_BOTH || buff_type == SGL_TO_LB) {
		for (i = 0; i < num_bufs; i++) {
			data_size = strlen(test_bufs[i]) + 1;
			if (prepare_sgl_bufs(test_bufs[i], uncomp_bufs[i],
			    data_size,
			    big_data ? buf_pool : ts_params->small_mbuf_pool,
			    big_data ? buf_pool : ts_params->large_mbuf_pool,
			    big_data ? 0 : MAX_SEGS,
			    big_data ? MAX_DATA_MBUF_SIZE : SMALL_SEG_SIZE) < 0)
				goto exit;
		}
	} else {
		for (i = 0; i < num_bufs; i++) {
			data_size = strlen(test_bufs[i]) + 1;
			buf_ptr = rte_pktmbuf_append(uncomp_bufs[i], data_size);
			strlcpy(buf_ptr, test_bufs[i], data_size);
		}
	}

	/* Prepare the destination mbufs */
	ret = rte_pktmbuf_alloc_bulk(buf_pool, comp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Destination mbufs could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	if (buff_type == SGL_BOTH || buff_type == LB_TO_SGL) {
		for (i = 0; i < num_bufs; i++) {
			if (out_of_space == 1 && oos_zlib_decompress)
				data_size = OUT_OF_SPACE_BUF;
			else
				(data_size = strlen(test_bufs[i]) *
					COMPRESS_BUF_SIZE_RATIO);

			if (prepare_sgl_bufs(NULL, comp_bufs[i],
			      data_size,
			      big_data ? buf_pool : ts_params->small_mbuf_pool,
			      big_data ? buf_pool : ts_params->large_mbuf_pool,
			      big_data ? 0 : MAX_SEGS,
			      big_data ? MAX_DATA_MBUF_SIZE : SMALL_SEG_SIZE)
					< 0)
				goto exit;
		}

	} else {
		for (i = 0; i < num_bufs; i++) {
			if (out_of_space == 1 && oos_zlib_decompress)
				data_size = OUT_OF_SPACE_BUF;
			else
				(data_size = strlen(test_bufs[i]) *
					COMPRESS_BUF_SIZE_RATIO);

			rte_pktmbuf_append(comp_bufs[i], data_size);
		}
	}

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
		char engine[] = "zlib (directly, not PMD)";
		if (zlib_dir != ZLIB_COMPRESS || zlib_dir != ZLIB_ALL)
			strlcpy(engine, "PMD", sizeof(engine));

		RTE_LOG(DEBUG, USER1, "Buffer %u compressed by %s from %u to"
			" %u bytes (level = %d, huffman = %s)\n",
			buf_idx[priv_data->orig_idx], engine,
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
		if (out_of_space && oos_zlib_decompress) {
			if (ops_processed[i]->status !=
					RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED) {
				ret_status = -1;

				RTE_LOG(ERR, USER1,
					"Operation without expected out of "
					"space status error\n");
				goto exit;
			} else
				continue;
		}

		if (ops_processed[i]->status != RTE_COMP_OP_STATUS_SUCCESS) {
			RTE_LOG(ERR, USER1,
				"Some operations were not successful\n");
			goto exit;
		}
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_free(uncomp_bufs[priv_data->orig_idx]);
		uncomp_bufs[priv_data->orig_idx] = NULL;
	}

	if (out_of_space && oos_zlib_decompress) {
		ret_status = 0;
		goto exit;
	}

	/* Allocate buffers for decompressed data */
	ret = rte_pktmbuf_alloc_bulk(buf_pool, uncomp_bufs, num_bufs);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Destination mbufs could not be allocated "
			"from the mempool\n");
		goto exit;
	}

	if (buff_type == SGL_BOTH || buff_type == LB_TO_SGL) {
		for (i = 0; i < num_bufs; i++) {
			priv_data = (struct priv_op_data *)
					(ops_processed[i] + 1);
			if (out_of_space == 1 && oos_zlib_compress)
				data_size = OUT_OF_SPACE_BUF;
			else
				data_size =
				strlen(test_bufs[priv_data->orig_idx]) + 1;

			if (prepare_sgl_bufs(NULL, uncomp_bufs[i],
			       data_size,
			       big_data ? buf_pool : ts_params->small_mbuf_pool,
			       big_data ? buf_pool : ts_params->large_mbuf_pool,
			       big_data ? 0 : MAX_SEGS,
			       big_data ? MAX_DATA_MBUF_SIZE : SMALL_SEG_SIZE)
					< 0)
				goto exit;
		}

	} else {
		for (i = 0; i < num_bufs; i++) {
			priv_data = (struct priv_op_data *)
					(ops_processed[i] + 1);
			if (out_of_space == 1 && oos_zlib_compress)
				data_size = OUT_OF_SPACE_BUF;
			else
				data_size =
				strlen(test_bufs[priv_data->orig_idx]) + 1;

			rte_pktmbuf_append(uncomp_bufs[i], data_size);
		}
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
	 * as they are not needed anymore
	 */
	rte_comp_op_bulk_free(ops_processed, num_bufs);

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
		char engine[] = "zlib, (directly, no PMD)";
		if (zlib_dir != ZLIB_DECOMPRESS || zlib_dir != ZLIB_ALL)
			strlcpy(engine, "pmd", sizeof(engine));
		RTE_LOG(DEBUG, USER1,
			"Buffer %u decompressed by %s from %u to %u bytes\n",
			buf_idx[priv_data->orig_idx], engine,
			ops_processed[i]->consumed, ops_processed[i]->produced);
		ops[i] = NULL;
	}

	/*
	 * Check operation status and free source mbuf (destination mbuf and
	 * compress operation information is still needed)
	 */
	for (i = 0; i < num_bufs; i++) {
		if (out_of_space && oos_zlib_compress) {
			if (ops_processed[i]->status !=
					RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED) {
				ret_status = -1;

				RTE_LOG(ERR, USER1,
					"Operation without expected out of "
					"space status error\n");
				goto exit;
			} else
				continue;
		}

		if (ops_processed[i]->status != RTE_COMP_OP_STATUS_SUCCESS) {
			RTE_LOG(ERR, USER1,
				"Some operations were not successful\n");
			goto exit;
		}
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		rte_pktmbuf_free(comp_bufs[priv_data->orig_idx]);
		comp_bufs[priv_data->orig_idx] = NULL;
	}

	if (out_of_space && oos_zlib_compress) {
		ret_status = 0;
		goto exit;
	}

	/*
	 * Compare the original stream with the decompressed stream
	 * (in size and the data)
	 */
	for (i = 0; i < num_bufs; i++) {
		priv_data = (struct priv_op_data *)(ops_processed[i] + 1);
		const char *buf1 = test_bufs[priv_data->orig_idx];
		const char *buf2;
		contig_buf = rte_malloc(NULL, ops_processed[i]->produced, 0);
		if (contig_buf == NULL) {
			RTE_LOG(ERR, USER1, "Contiguous buffer could not "
					"be allocated\n");
			goto exit;
		}

		buf2 = rte_pktmbuf_read(ops_processed[i]->m_dst, 0,
				ops_processed[i]->produced, contig_buf);
		if (compare_buffers(buf1, strlen(buf1) + 1,
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
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
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
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
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

	struct interim_data_params int_data = {
		compress_test_bufs,
		num_bufs,
		buf_idx,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
	};

	/* Compress with compressdev, decompress with Zlib */
	test_data.zlib_dir = ZLIB_DECOMPRESS;
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
		return TEST_FAILED;

	/* Compress with Zlib, decompress with compressdev */
	test_data.zlib_dir = ZLIB_COMPRESS;
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
		return TEST_FAILED;

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
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		for (level = RTE_COMP_LEVEL_MIN; level <= RTE_COMP_LEVEL_MAX;
				level++) {
			compress_xform->compress.level = level;
			/* Compress with compressdev, decompress with Zlib */
			test_data.zlib_dir = ZLIB_DECOMPRESS;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
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

	struct interim_data_params int_data = {
		test_buffers,
		num_bufs,
		buf_idx,
		compress_xforms,
		decompress_xforms,
		NUM_XFORMS
	};

	struct test_data_params test_data = {
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
	};

	/* Compress with compressdev, decompress with Zlib */
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
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

static int
test_compressdev_deflate_stateless_sgl(void)
{
	struct comp_testsuite_params *ts_params = &testsuite_params;
	uint16_t i;
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
		RTE_COMP_OP_STATELESS,
		SGL_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
	};

	for (i = 0; i < RTE_DIM(compress_test_bufs); i++) {
		int_data.test_bufs = &compress_test_bufs[i];
		int_data.buf_idx = &i;

		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
			return TEST_FAILED;

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
			return TEST_FAILED;

		if (capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_LB_OUT) {
			/* Compress with compressdev, decompress with Zlib */
			test_data.zlib_dir = ZLIB_DECOMPRESS;
			test_data.buff_type = SGL_TO_LB;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
				return TEST_FAILED;

			/* Compress with Zlib, decompress with compressdev */
			test_data.zlib_dir = ZLIB_COMPRESS;
			test_data.buff_type = SGL_TO_LB;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
				return TEST_FAILED;
		}

		if (capab->comp_feature_flags & RTE_COMP_FF_OOP_LB_IN_SGL_OUT) {
			/* Compress with compressdev, decompress with Zlib */
			test_data.zlib_dir = ZLIB_DECOMPRESS;
			test_data.buff_type = LB_TO_SGL;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
				return TEST_FAILED;

			/* Compress with Zlib, decompress with compressdev */
			test_data.zlib_dir = ZLIB_COMPRESS;
			test_data.buff_type = LB_TO_SGL;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0)
				return TEST_FAILED;
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
		ret = TEST_FAILED;
		return ret;
	}

	memcpy(compress_xform, ts_params->def_comp_xform,
			sizeof(struct rte_comp_xform));

	struct rte_comp_xform *decompress_xform =
			rte_malloc(NULL, sizeof(struct rte_comp_xform), 0);
	if (decompress_xform == NULL) {
		RTE_LOG(ERR, USER1, "Decompress xform could not be created\n");
		rte_free(compress_xform);
		ret = TEST_FAILED;
		return ret;
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
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		0,
		0
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
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}

			/* Generate compression and decompression
			 * checksum of selected driver
			 */
			test_data.zlib_dir = ZLIB_NONE;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
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
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
			/* Generate compression and decompression
			 * checksum of selected driver
			 */
			test_data.zlib_dir = ZLIB_NONE;
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
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
			if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
				ret = TEST_FAILED;
				goto exit;
			}
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

	RTE_LOG(INFO, USER1, "This is a negative test errors are expected\n");

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

	struct interim_data_params int_data = {
		&compress_test_bufs[0],
		1,
		&i,
		&ts_params->def_comp_xform,
		&ts_params->def_decomp_xform,
		1
	};

	struct test_data_params test_data = {
		RTE_COMP_OP_STATELESS,
		LB_BOTH,
		ZLIB_DECOMPRESS,
		1,
		0
	};
	/* Compress with compressdev, decompress with Zlib */
	test_data.zlib_dir = ZLIB_DECOMPRESS;
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto exit;
	}

	/* Compress with Zlib, decompress with compressdev */
	test_data.zlib_dir = ZLIB_COMPRESS;
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto exit;
	}

	if (capab->comp_feature_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) {
		/* Compress with compressdev, decompress with Zlib */
		test_data.zlib_dir = ZLIB_DECOMPRESS;
		test_data.buff_type = SGL_BOTH;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}

		/* Compress with Zlib, decompress with compressdev */
		test_data.zlib_dir = ZLIB_COMPRESS;
		test_data.buff_type = SGL_BOTH;
		if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
			ret = TEST_FAILED;
			goto exit;
		}
	}

	ret  = TEST_SUCCESS;

exit:
	rte_free(compress_xform);
	return ret;
}

static int
test_compressdev_deflate_stateless_dynamic_big(void)
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
		RTE_COMP_OP_STATELESS,
		SGL_BOTH,
		ZLIB_DECOMPRESS,
		0,
		1
	};

	ts_params->def_comp_xform->compress.deflate.huffman =
						RTE_COMP_HUFFMAN_DYNAMIC;

	/* fill the buffer with data based on rand. data */
	srand(BIG_DATA_TEST_SIZE);
	for (j = 0; j < BIG_DATA_TEST_SIZE - 1; ++j)
		test_buffer[j] = (uint8_t)(rand() % ((uint8_t)-1)) | 1;
	test_buffer[BIG_DATA_TEST_SIZE-1] = 0;

	/* Compress with compressdev, decompress with Zlib */
	test_data.zlib_dir = ZLIB_DECOMPRESS;
	if (test_deflate_comp_decomp(&int_data, &test_data) < 0) {
		ret = TEST_FAILED;
		goto end;
	}

	/* Compress with Zlib, decompress with compressdev */
	test_data.zlib_dir = ZLIB_COMPRESS;
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
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_compressdev(void)
{
	return unit_test_suite_runner(&compressdev_testsuite);
}

REGISTER_TEST_COMMAND(compressdev_autotest, test_compressdev);
