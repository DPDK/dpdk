/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#include <isa-l.h>

#include <bus_vdev_driver.h>
#include <rte_common.h>
#include <rte_cpuflags.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_compressdev_pmd.h>

#include "isal_compress_pmd_private.h"

#define RTE_COMP_ISAL_WINDOW_SIZE 15
#define RTE_COMP_ISAL_LEVEL_ZERO 0 /* ISA-L Level 0 used for fixed Huffman */
#define RTE_COMP_ISAL_LEVEL_ONE 1
#define RTE_COMP_ISAL_LEVEL_TWO 2
#define RTE_COMP_ISAL_LEVEL_THREE 3 /* Optimised for AVX512 & AVX2 only */
#define CHKSUM_SZ_CRC 8
#define CHKSUM_SZ_ADLER 4

#define STRINGIFY(s) #s
#define ISAL_TOSTRING(maj, min, patch) \
	STRINGIFY(maj)"."STRINGIFY(min)"."STRINGIFY(patch)
#define ISAL_VERSION_STRING \
	ISAL_TOSTRING(ISAL_MAJOR_VERSION, ISAL_MINOR_VERSION, ISAL_PATCH_VERSION)

/* Verify and set private xform parameters */
int
isal_comp_set_priv_xform_parameters(struct isal_priv_xform *priv_xform,
		const struct rte_comp_xform *xform)
{
	if (xform == NULL)
		return -EINVAL;

	/* Set compression private xform variables */
	if (xform->type == RTE_COMP_COMPRESS) {
		/* Set private xform type - COMPRESS/DECOMPRESS */
		priv_xform->type = RTE_COMP_COMPRESS;

		/* Set private xform algorithm */
		if (xform->compress.algo != RTE_COMP_ALGO_DEFLATE) {
			if (xform->compress.algo == RTE_COMP_ALGO_NULL) {
				ISAL_PMD_LOG(ERR, "By-pass not supported\n");
				return -ENOTSUP;
			}
			ISAL_PMD_LOG(ERR, "Algorithm not supported\n");
			return -ENOTSUP;
		}
		priv_xform->compress.algo = RTE_COMP_ALGO_DEFLATE;

		/* Set private xform window size, 32K supported */
		if (xform->compress.window_size == RTE_COMP_ISAL_WINDOW_SIZE)
			priv_xform->compress.window_size =
					RTE_COMP_ISAL_WINDOW_SIZE;
		else {
			ISAL_PMD_LOG(ERR, "Window size not supported\n");
			return -ENOTSUP;
		}

		/* Set private xform huffman type */
		switch (xform->compress.deflate.huffman) {
		case(RTE_COMP_HUFFMAN_DEFAULT):
			priv_xform->compress.deflate.huffman =
					RTE_COMP_HUFFMAN_DEFAULT;
			break;
		case(RTE_COMP_HUFFMAN_FIXED):
			priv_xform->compress.deflate.huffman =
					RTE_COMP_HUFFMAN_FIXED;
			break;
		case(RTE_COMP_HUFFMAN_DYNAMIC):
			priv_xform->compress.deflate.huffman =
					RTE_COMP_HUFFMAN_DYNAMIC;
			break;
		default:
			ISAL_PMD_LOG(ERR, "Huffman code not supported\n");
			return -ENOTSUP;
		}

		/* Set private xform checksum */
		switch (xform->compress.chksum) {
		/* Raw deflate by default */
		case(RTE_COMP_CHECKSUM_NONE):
			priv_xform->compress.chksum = IGZIP_DEFLATE;
			break;
		case(RTE_COMP_CHECKSUM_CRC32):
			priv_xform->compress.chksum = IGZIP_GZIP_NO_HDR;
			break;
		case(RTE_COMP_CHECKSUM_ADLER32):
			priv_xform->compress.chksum = IGZIP_ZLIB_NO_HDR;
			break;
		case(RTE_COMP_CHECKSUM_CRC32_ADLER32):
			ISAL_PMD_LOG(ERR, "Combined CRC and ADLER checksum not"
					" supported\n");
			return -ENOTSUP;
		default:
			ISAL_PMD_LOG(ERR, "Checksum type not supported\n");
			priv_xform->compress.chksum = IGZIP_DEFLATE;
			break;
		}

		/* Set private xform level.
		 * Checking compliance with compressdev API, -1 <= level => 9
		 */
		if (xform->compress.level < RTE_COMP_LEVEL_PMD_DEFAULT ||
				xform->compress.level > RTE_COMP_LEVEL_MAX) {
			ISAL_PMD_LOG(ERR, "Compression level out of range\n");
			return -EINVAL;
		}
		/* Check for Compressdev API level 0, No compression
		 * not supported in ISA-L
		 */
		else if (xform->compress.level == RTE_COMP_LEVEL_NONE) {
			ISAL_PMD_LOG(ERR, "No Compression not supported\n");
			return -ENOTSUP;
		}
		/* If using fixed huffman code, level must be 0 */
		else if (priv_xform->compress.deflate.huffman ==
				RTE_COMP_HUFFMAN_FIXED) {
			ISAL_PMD_LOG(DEBUG, "ISA-L level 0 used due to a"
					" fixed huffman code\n");
			priv_xform->compress.level = RTE_COMP_ISAL_LEVEL_ZERO;
			priv_xform->level_buffer_size =
					ISAL_DEF_LVL0_DEFAULT;
		} else {
			/* Mapping API levels to ISA-L levels 1,2 & 3 */
			switch (xform->compress.level) {
			case RTE_COMP_LEVEL_PMD_DEFAULT:
				/* Default is 1 if not using fixed huffman */
				priv_xform->compress.level =
						RTE_COMP_ISAL_LEVEL_ONE;
				priv_xform->level_buffer_size =
						ISAL_DEF_LVL1_DEFAULT;
				break;
			case RTE_COMP_LEVEL_MIN:
				priv_xform->compress.level =
						RTE_COMP_ISAL_LEVEL_ONE;
				priv_xform->level_buffer_size =
						ISAL_DEF_LVL1_DEFAULT;
				break;
			case RTE_COMP_ISAL_LEVEL_TWO:
				priv_xform->compress.level =
						RTE_COMP_ISAL_LEVEL_TWO;
				priv_xform->level_buffer_size =
						ISAL_DEF_LVL2_DEFAULT;
				break;
			/* Level 3 or higher requested */
			default:
#ifdef RTE_ARCH_X86
				/* Check for AVX512, to use ISA-L level 3 */
				if (rte_cpu_get_flag_enabled(
						RTE_CPUFLAG_AVX512F)) {
					priv_xform->compress.level =
						RTE_COMP_ISAL_LEVEL_THREE;
					priv_xform->level_buffer_size =
						ISAL_DEF_LVL3_DEFAULT;
				}
				/* Check for AVX2, to use ISA-L level 3 */
				else if (rte_cpu_get_flag_enabled(
						RTE_CPUFLAG_AVX2)) {
					priv_xform->compress.level =
						RTE_COMP_ISAL_LEVEL_THREE;
					priv_xform->level_buffer_size =
						ISAL_DEF_LVL3_DEFAULT;
				} else
#endif
				{
					ISAL_PMD_LOG(DEBUG, "Requested ISA-L level"
						" 3 or above; Level 3 optimized"
						" for AVX512 & AVX2 only."
						" level changed to 2.\n");
					priv_xform->compress.level =
						RTE_COMP_ISAL_LEVEL_TWO;
					priv_xform->level_buffer_size =
						ISAL_DEF_LVL2_DEFAULT;
				}
			}
		}
	}

	/* Set decompression private xform variables */
	else if (xform->type == RTE_COMP_DECOMPRESS) {

		/* Set private xform type - COMPRESS/DECOMPRESS */
		priv_xform->type = RTE_COMP_DECOMPRESS;

		/* Set private xform algorithm */
		if (xform->decompress.algo != RTE_COMP_ALGO_DEFLATE) {
			if (xform->decompress.algo == RTE_COMP_ALGO_NULL) {
				ISAL_PMD_LOG(ERR, "By pass not supported\n");
				return -ENOTSUP;
			}
			ISAL_PMD_LOG(ERR, "Algorithm not supported\n");
			return -ENOTSUP;
		}
		priv_xform->decompress.algo = RTE_COMP_ALGO_DEFLATE;

		/* Set private xform checksum */
		switch (xform->decompress.chksum) {
		/* Raw deflate by default */
		case(RTE_COMP_CHECKSUM_NONE):
			priv_xform->decompress.chksum = ISAL_DEFLATE;
			break;
		case(RTE_COMP_CHECKSUM_CRC32):
			priv_xform->decompress.chksum = ISAL_GZIP_NO_HDR;
			break;
		case(RTE_COMP_CHECKSUM_ADLER32):
			priv_xform->decompress.chksum = ISAL_ZLIB_NO_HDR;
			break;
		case(RTE_COMP_CHECKSUM_CRC32_ADLER32):
			ISAL_PMD_LOG(ERR, "Combined CRC and ADLER checksum not"
					" supported\n");
			return -ENOTSUP;
		default:
			ISAL_PMD_LOG(ERR, "Checksum type not supported\n");
			priv_xform->decompress.chksum = ISAL_DEFLATE;
			break;
		}

		/* Set private xform window size, 32K supported */
		if (xform->decompress.window_size == RTE_COMP_ISAL_WINDOW_SIZE)
			priv_xform->decompress.window_size =
					RTE_COMP_ISAL_WINDOW_SIZE;
		else {
			ISAL_PMD_LOG(ERR, "Window size not supported\n");
			return -ENOTSUP;
		}
	}
	return 0;
}

/* Compression using chained mbufs for input/output data */
static int
chained_mbuf_compression(struct rte_comp_op *op, struct isal_comp_qp *qp)
{
	int ret;
	uint32_t remaining_offset;
	uint32_t remaining_data = op->src.length;
	struct rte_mbuf *src = op->m_src;
	struct rte_mbuf *dst = op->m_dst;

	/* check for source/destination offset passing multiple segments
	 * and point compression stream to input/output buffer.
	 */
	remaining_offset = op->src.offset;
	while (remaining_offset >= src->data_len) {
		remaining_offset -= src->data_len;
		src = src->next;
	}
	qp->stream->avail_in = RTE_MIN(src->data_len - remaining_offset,
			op->src.length);
	qp->stream->next_in = rte_pktmbuf_mtod_offset(src, uint8_t *,
			remaining_offset);

	remaining_offset = op->dst.offset;
	while (remaining_offset >= dst->data_len) {
		remaining_offset -= dst->data_len;
		dst = dst->next;
	}
	qp->stream->avail_out = dst->data_len - remaining_offset;
	qp->stream->next_out = rte_pktmbuf_mtod_offset(dst, uint8_t *,
			remaining_offset);

	if (unlikely(!qp->stream->next_in || !qp->stream->next_out)) {
		ISAL_PMD_LOG(ERR, "Invalid source or destination buffer\n");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		return -1;
	}

	while (qp->stream->internal_state.state != ZSTATE_END) {
		/* Last segment of data */
		if (remaining_data <= src->data_len)
			qp->stream->end_of_stream = 1;

		/* Execute compression operation */
		ret = isal_deflate(qp->stream);

		remaining_data = op->src.length - qp->stream->total_in;

		if (ret != COMP_OK) {
			ISAL_PMD_LOG(ERR, "Compression operation failed\n");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return ret;
		}

		if (qp->stream->avail_in == 0 &&
				qp->stream->total_in != op->src.length) {
			if (src->next != NULL) {
				src = src->next;
				qp->stream->next_in =
						rte_pktmbuf_mtod(src, uint8_t *);
				qp->stream->avail_in =
					RTE_MIN(remaining_data, src->data_len);
			} else {
				ISAL_PMD_LOG(ERR,
				"Not enough input buffer segments\n");
				op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
				return -1;
			}
		}

		if (qp->stream->avail_out == 0 &&
				qp->stream->internal_state.state != ZSTATE_END) {
			if (dst->next != NULL) {
				dst = dst->next;
				qp->stream->next_out =
						rte_pktmbuf_mtod(dst, uint8_t *);
				qp->stream->avail_out = dst->data_len;
			} else {
				ISAL_PMD_LOG(ERR,
				"Not enough output buffer segments\n");
				op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
				return -1;
			}
		}
	}

	return 0;
}

/* Decompression using chained mbufs for input/output data */
static int
chained_mbuf_decompression(struct rte_comp_op *op, struct isal_comp_qp *qp)
{
	int ret;
	uint32_t consumed_data, src_remaining_offset, dst_remaining_offset;
	uint32_t remaining_data = op->src.length;
	struct rte_mbuf *src = op->m_src;
	struct rte_mbuf *dst = op->m_dst;

	/* check for offset passing multiple segments
	 * and point decompression state to input/output buffer
	 */
	src_remaining_offset = op->src.offset;
	while (src_remaining_offset >= src->data_len) {
		src_remaining_offset -= src->data_len;
		src = src->next;
	}
	qp->state->avail_in = RTE_MIN(src->data_len - src_remaining_offset,
			op->src.length);
	qp->state->next_in = rte_pktmbuf_mtod_offset(src, uint8_t *,
			src_remaining_offset);

	dst_remaining_offset = op->dst.offset;
	while (dst_remaining_offset >= dst->data_len) {
		dst_remaining_offset -= dst->data_len;
		dst = dst->next;
	}
	qp->state->avail_out = dst->data_len - dst_remaining_offset;
	qp->state->next_out = rte_pktmbuf_mtod_offset(dst, uint8_t *,
			dst_remaining_offset);

	while (qp->state->block_state != ISAL_BLOCK_FINISH) {

		ret = isal_inflate(qp->state);

		/* Check for first segment, offset needs to be accounted for */
		if (remaining_data == op->src.length) {
			consumed_data = src->data_len - src_remaining_offset;
		} else
			consumed_data = src->data_len;

		if (qp->state->avail_in == 0
				&& op->consumed != op->src.length) {
			op->consumed += consumed_data;
			remaining_data -= consumed_data;

			if (src->next != NULL) {
				src = src->next;
				qp->state->next_in =
						rte_pktmbuf_mtod(src, uint8_t *);
				qp->state->avail_in =
					RTE_MIN(remaining_data, src->data_len);
			}
		}

		if (ret == ISAL_OUT_OVERFLOW) {
			ISAL_PMD_LOG(ERR, "Decompression operation ran "
				"out of space, but can be recovered.\n%d bytes "
				"consumed\t%d bytes produced\n",
				consumed_data, qp->state->total_out);
				op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE;
			return ret;
		} else if (ret < 0) {
			ISAL_PMD_LOG(ERR, "Decompression operation failed\n");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return ret;
		}

		if (qp->state->avail_out == 0 &&
				qp->state->block_state != ISAL_BLOCK_FINISH) {
			if (dst->next != NULL) {
				dst = dst->next;
				qp->state->next_out =
						rte_pktmbuf_mtod(dst, uint8_t *);
				qp->state->avail_out = dst->data_len;
			} else {
				ISAL_PMD_LOG(ERR,
				"Not enough output buffer segments\n");
				op->status =
				RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
				return -1;
			}
		}
	}

	return 0;
}

/* Stateless Compression Function */
static int
process_isal_deflate(struct rte_comp_op *op, struct isal_comp_qp *qp,
		struct isal_priv_xform *priv_xform)
{
	int ret = 0;
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	/* Required due to init clearing level_buf */
	uint8_t *temp_level_buf = qp->stream->level_buf;

	/* Initialize compression stream */
	isal_deflate_init(qp->stream);

	qp->stream->level_buf = temp_level_buf;

	/* Set Checksum flag */
	qp->stream->gzip_flag = priv_xform->compress.chksum;

	/* Stateless operation, input will be consumed in one go */
	qp->stream->flush = NO_FLUSH;

	/* set compression level & intermediate level buffer size */
	qp->stream->level = priv_xform->compress.level;
	qp->stream->level_buf_size = priv_xform->level_buffer_size;

	/* Set op huffman code */
	if (priv_xform->compress.deflate.huffman == RTE_COMP_HUFFMAN_FIXED)
		isal_deflate_set_hufftables(qp->stream, NULL,
				IGZIP_HUFFTABLE_STATIC);
	else if (priv_xform->compress.deflate.huffman ==
			RTE_COMP_HUFFMAN_DEFAULT)
		isal_deflate_set_hufftables(qp->stream, NULL,
			IGZIP_HUFFTABLE_DEFAULT);
	/* Dynamically change the huffman code to suit the input data */
	else if (priv_xform->compress.deflate.huffman ==
			RTE_COMP_HUFFMAN_DYNAMIC)
		isal_deflate_set_hufftables(qp->stream, NULL,
				IGZIP_HUFFTABLE_DEFAULT);

	if (op->m_src->pkt_len < (op->src.length + op->src.offset)) {
		ISAL_PMD_LOG(ERR, "Input mbuf(s) not big enough.\n");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		return -1;
	}

	if (op->dst.offset >= op->m_dst->pkt_len) {
		ISAL_PMD_LOG(ERR, "Output mbuf(s) not big enough"
				" for offset provided.\n");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		return -1;
	}

	/* Chained mbufs */
	if (op->m_src->nb_segs > 1 || op->m_dst->nb_segs > 1) {
		ret = chained_mbuf_compression(op, qp);
		if (ret < 0)
			return ret;
	} else {
		/* Linear buffer */
		qp->stream->end_of_stream = 1; /* All input consumed in one */
		/* Point compression stream to input buffer */
		qp->stream->avail_in = op->src.length;
		qp->stream->next_in = rte_pktmbuf_mtod_offset(op->m_src,
				uint8_t *, op->src.offset);

		/*  Point compression stream to output buffer */
		qp->stream->avail_out = op->m_dst->data_len - op->dst.offset;
		qp->stream->next_out  = rte_pktmbuf_mtod_offset(op->m_dst,
				uint8_t *, op->dst.offset);

		if (unlikely(!qp->stream->next_in || !qp->stream->next_out)) {
			ISAL_PMD_LOG(ERR, "Invalid source or destination"
					" buffers\n");
			op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			return -1;
		}

		/* Execute compression operation */
		ret =  isal_deflate_stateless(qp->stream);

		/* Check that output buffer did not run out of space */
		if (ret == STATELESS_OVERFLOW) {
			ISAL_PMD_LOG(ERR, "Output buffer not big enough\n");
			op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
			return ret;
		}

		/* Check that input buffer has been fully consumed */
		if (qp->stream->avail_in != (uint32_t)0) {
			ISAL_PMD_LOG(ERR, "Input buffer could not be read"
					" entirely\n");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return -1;
		}

		if (ret != COMP_OK) {
			ISAL_PMD_LOG(ERR, "Compression operation failed\n");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return ret;
		}
	}

	op->consumed = qp->stream->total_in;
	if (qp->stream->gzip_flag == IGZIP_DEFLATE) {
		op->produced = qp->stream->total_out;
	} else if (qp->stream->gzip_flag == IGZIP_ZLIB_NO_HDR) {
		op->produced = qp->stream->total_out - CHKSUM_SZ_ADLER;
		op->output_chksum = qp->stream->internal_state.crc + 1;
	} else {
		op->produced = qp->stream->total_out - CHKSUM_SZ_CRC;
		op->output_chksum = qp->stream->internal_state.crc;
	}

	return ret;
}

/* Stateless Decompression Function */
static int
process_isal_inflate(struct rte_comp_op *op, struct isal_comp_qp *qp,
		struct isal_priv_xform *priv_xform)
{
	int ret = 0;

	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	/* Initialize decompression state */
	isal_inflate_init(qp->state);

	/* Set Checksum flag */
	qp->state->crc_flag = priv_xform->decompress.chksum;

	if (op->m_src->pkt_len < (op->src.length + op->src.offset)) {
		ISAL_PMD_LOG(ERR, "Input mbuf(s) not big enough.\n");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		return -1;
	}

	if (op->dst.offset >= op->m_dst->pkt_len) {
		ISAL_PMD_LOG(ERR, "Output mbuf not big enough for "
				"offset provided.\n");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		return -1;
	}

	/* Chained mbufs */
	if (op->m_src->nb_segs > 1 || op->m_dst->nb_segs > 1) {
		ret = chained_mbuf_decompression(op, qp);
		if (ret !=  0)
			return ret;
	} else {
		/* Linear buffer */
		/* Point decompression state to input buffer */
		qp->state->avail_in = op->src.length;
		qp->state->next_in = rte_pktmbuf_mtod_offset(op->m_src,
				uint8_t *, op->src.offset);

		/* Point decompression state to output buffer */
		qp->state->avail_out = op->m_dst->data_len - op->dst.offset;
		qp->state->next_out  = rte_pktmbuf_mtod_offset(op->m_dst,
				uint8_t *, op->dst.offset);

		if (unlikely(!qp->state->next_in || !qp->state->next_out)) {
			ISAL_PMD_LOG(ERR, "Invalid source or destination"
					" buffers\n");
			op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			return -1;
		}

		/* Execute decompression operation */
		ret = isal_inflate_stateless(qp->state);

		if (ret == ISAL_OUT_OVERFLOW) {
			ISAL_PMD_LOG(ERR, "Output buffer not big enough\n");
			op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
			return ret;
		}

		/* Check that input buffer has been fully consumed */
		if (qp->state->avail_in != (uint32_t)0) {
			ISAL_PMD_LOG(ERR, "Input buffer could not be read"
					" entirely\n");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return -1;
		}

		if (ret != ISAL_DECOMP_OK && ret != ISAL_END_INPUT) {
			ISAL_PMD_LOG(ERR, "Decompression operation failed\n");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return ret;
		}
		op->consumed = op->src.length - qp->state->avail_in;
	}
	op->produced = qp->state->total_out;
	op->output_chksum = qp->state->crc;

	return ret;
}

/* Process compression/decompression operation */
static int
process_op(struct isal_comp_qp *qp, struct rte_comp_op *op,
		struct isal_priv_xform *priv_xform)
{
	switch (priv_xform->type) {
	case RTE_COMP_COMPRESS:
		process_isal_deflate(op, qp, priv_xform);
		break;
	case RTE_COMP_DECOMPRESS:
		process_isal_inflate(op, qp, priv_xform);
		break;
	default:
		ISAL_PMD_LOG(ERR, "Operation Not Supported\n");
		return -ENOTSUP;
	}
	return 0;
}

/* Enqueue burst */
static uint16_t
isal_comp_pmd_enqueue_burst(void *queue_pair, struct rte_comp_op **ops,
			uint16_t nb_ops)
{
	struct isal_comp_qp *qp = queue_pair;
	uint16_t i;
	int retval;
	int16_t num_enq = RTE_MIN(qp->num_free_elements, nb_ops);

	for (i = 0; i < num_enq; i++) {
		if (unlikely(ops[i]->op_type != RTE_COMP_OP_STATELESS)) {
			ops[i]->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			ISAL_PMD_LOG(ERR, "Stateful operation not Supported\n");
			qp->qp_stats.enqueue_err_count++;
			continue;
		}
		retval = process_op(qp, ops[i], ops[i]->private_xform);
		if (unlikely(retval < 0) ||
				ops[i]->status != RTE_COMP_OP_STATUS_SUCCESS) {
			qp->qp_stats.enqueue_err_count++;
		}
	}

	retval = rte_ring_enqueue_burst(qp->processed_pkts, (void *)ops,
			num_enq, NULL);
	qp->num_free_elements -= retval;
	qp->qp_stats.enqueued_count += retval;

	return retval;
}

/* Dequeue burst */
static uint16_t
isal_comp_pmd_dequeue_burst(void *queue_pair, struct rte_comp_op **ops,
		uint16_t nb_ops)
{
	struct isal_comp_qp *qp = queue_pair;
	uint16_t nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts, (void **)ops,
			nb_ops, NULL);
	qp->num_free_elements += nb_dequeued;
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/* Create ISA-L compression device */
static int
compdev_isal_create(const char *name, struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct isal_comp_private), init_params);
	if (dev == NULL) {
		ISAL_PMD_LOG(ERR, "failed to create compressdev vdev");
		return -EFAULT;
	}

	dev->dev_ops = isal_compress_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = isal_comp_pmd_dequeue_burst;
	dev->enqueue_burst = isal_comp_pmd_enqueue_burst;

	ISAL_PMD_LOG(INFO, "\nISA-L library version used: "ISAL_VERSION_STRING);

	return 0;
}

/** Remove compression device */
static int
compdev_isal_remove_dev(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compdev = rte_compressdev_pmd_get_named_dev(name);
	if (compdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compdev);
}

/** Initialise ISA-L compression device */
static int
compdev_isal_probe(struct rte_vdev_device *dev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id(),
	};
	const char *name, *args;
	int retval;

	name = rte_vdev_device_name(dev);
	if (name == NULL)
		return -EINVAL;

	args = rte_vdev_device_args(dev);

	retval = rte_compressdev_pmd_parse_input_args(&init_params, args);
	if (retval) {
		ISAL_PMD_LOG(ERR,
			"Failed to parse initialisation arguments[%s]\n", args);
		return -EINVAL;
	}

	return compdev_isal_create(name, dev, &init_params);
}

static struct rte_vdev_driver compdev_isal_pmd_drv = {
	.probe = compdev_isal_probe,
	.remove = compdev_isal_remove_dev,
};

RTE_PMD_REGISTER_VDEV(COMPDEV_NAME_ISAL_PMD, compdev_isal_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(COMPDEV_NAME_ISAL_PMD,
	"socket_id=<int>");
RTE_LOG_REGISTER_DEFAULT(isal_logtype_driver, INFO);
