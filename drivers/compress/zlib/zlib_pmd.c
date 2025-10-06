/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#include <bus_vdev_driver.h>
#include <rte_common.h>
#include <stdlib.h>

#include "zlib_pmd_private.h"

/** Compute next mbuf in the list, assign data buffer and length,
 *  returns 0 if mbuf is NULL
 */
#define COMPUTE_BUF(mbuf, data, len)		\
		((mbuf = mbuf->next) ?		\
		(data = rte_pktmbuf_mtod(mbuf, uint8_t *)),	\
		(len = rte_pktmbuf_data_len(mbuf)) : 0)

#define BOTTOM_NIBBLE_OF_BYTE 0xf
#define TOP_NIBBLE_OF_BYTE 0xf0
#define BOTTOM_NIBBLE_OF_BYTES_IN_DOUBLE_WORD 0x0f0f0f0f
#define TOP_NIBBLE_OF_BYTE_IN_DOUBLE_WORD 0xf0f0f0f0
#define ZLIB_MAX_DICT_SIZE (1ULL << 15)

static void
process_zlib_deflate_chksum(struct rte_comp_op *op,
		z_stream *strm, enum rte_comp_checksum_type chksum)
{
	uint32_t dictionary_len = 0;
	uint8_t *dictionary = malloc(ZLIB_MAX_DICT_SIZE);
	uint32_t dictionary_start, dictionary_end, sum;
	uint8_t *sum_bytes = (uint8_t *)&sum;
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	switch (chksum) {
	case RTE_COMP_CHECKSUM_3GPP_PDCP_UDC:

		if (!dictionary) {
			ZLIB_PMD_ERR("Unable to fetch dictionary");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return;
		}

		if (deflateGetDictionary(strm, dictionary, &dictionary_len)) {
			ZLIB_PMD_ERR("Unable to fetch dictionary");
			op->status = RTE_COMP_OP_STATUS_CHECKSUM_VALIDATION_FAILED;
			free(dictionary);
			return;
		}

		dictionary_start = (uint32_t)(*dictionary);
		dictionary_end = (uint32_t)(*(dictionary + dictionary_len - 4));
		sum = (dictionary_start & BOTTOM_NIBBLE_OF_BYTES_IN_DOUBLE_WORD)
			+ (dictionary_start & (TOP_NIBBLE_OF_BYTE_IN_DOUBLE_WORD >> 4))
			+ (dictionary_end & BOTTOM_NIBBLE_OF_BYTES_IN_DOUBLE_WORD)
			+ (dictionary_end & (TOP_NIBBLE_OF_BYTE_IN_DOUBLE_WORD >> 4));

		op->output_chksum = ~(sum_bytes[0] + sum_bytes[1] + sum_bytes[2] + sum_bytes[3])
			 & BOTTOM_NIBBLE_OF_BYTE;
		break;
	case RTE_COMP_CHECKSUM_NONE:
		break;
	case RTE_COMP_CHECKSUM_CRC32:
	case RTE_COMP_CHECKSUM_ADLER32:
	case RTE_COMP_CHECKSUM_CRC32_ADLER32:
	default:
		ZLIB_PMD_ERR("Checksum not supported");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		free(dictionary);
		return;
	}
	free(dictionary);
}

static void
process_zlib_inflate_chksum(struct rte_comp_op *op,
		z_stream *strm,
		enum rte_comp_checksum_type chksum)
{
	uint32_t dictionary_len = 0;
	uint8_t *dictionary = malloc(ZLIB_MAX_DICT_SIZE);
	uint32_t dictionary_start, dictionary_end, sum;
	uint8_t *sum_bytes = (uint8_t *)&sum;
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	switch (chksum) {
	case RTE_COMP_CHECKSUM_3GPP_PDCP_UDC:
		if (!dictionary) {
			ZLIB_PMD_ERR("Unable to malloc dictionary");
			op->status = RTE_COMP_OP_STATUS_ERROR;
			return;
		}

		if (inflateGetDictionary(strm, dictionary, &dictionary_len)) {
			ZLIB_PMD_ERR("Unable to fetch dictionary");
			op->status = RTE_COMP_OP_STATUS_CHECKSUM_VALIDATION_FAILED;
			free(dictionary);
			return;
		}

		dictionary_start = (uint32_t)(*dictionary);
		dictionary_end = (uint32_t)(*(dictionary + dictionary_len - 4));
		sum = (dictionary_start & BOTTOM_NIBBLE_OF_BYTES_IN_DOUBLE_WORD)
			+ (dictionary_start & (TOP_NIBBLE_OF_BYTE_IN_DOUBLE_WORD >> 4))
			+ (dictionary_end & BOTTOM_NIBBLE_OF_BYTES_IN_DOUBLE_WORD)
			+ (dictionary_end & (TOP_NIBBLE_OF_BYTE_IN_DOUBLE_WORD >> 4));

		op->output_chksum = ~(sum_bytes[0] + sum_bytes[1] + sum_bytes[2] + sum_bytes[3])
			 & BOTTOM_NIBBLE_OF_BYTE;

		if (op->input_chksum != op->output_chksum) {
			ZLIB_PMD_ERR("Checksum does not match");
			op->status = RTE_COMP_OP_STATUS_CHECKSUM_VALIDATION_FAILED;
			free(dictionary);
			return;
		}
		break;
	case RTE_COMP_CHECKSUM_NONE:
		break;
	case RTE_COMP_CHECKSUM_CRC32:
	case RTE_COMP_CHECKSUM_ADLER32:
	case RTE_COMP_CHECKSUM_CRC32_ADLER32:
	default:
		ZLIB_PMD_ERR("Checksum not supported");
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		free(dictionary);
		return;
	}
}

static void
process_zlib_deflate(struct rte_comp_op *op, z_stream *strm)
{
	int ret, flush, fin_flush;
	unsigned long total_in_at_start, total_out_at_start;
	struct rte_mbuf *mbuf_src = op->m_src;
	struct rte_mbuf *mbuf_dst = op->m_dst;

	switch (op->flush_flag) {
	case RTE_COMP_FLUSH_FULL:
	case RTE_COMP_FLUSH_FINAL:
		fin_flush = Z_FINISH;
		break;
	case RTE_COMP_FLUSH_SYNC:
		fin_flush = Z_SYNC_FLUSH;
		break;
	default:
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_PMD_ERR("Invalid flush value");
		return;
	}

	if (unlikely(!strm)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_PMD_ERR("Invalid z_stream");
		return;
	}
	/* Update z_stream with the inputs provided by application */
	strm->next_in = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *,
			op->src.offset);

	strm->avail_in = rte_pktmbuf_data_len(mbuf_src) - op->src.offset;

	strm->next_out = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
			op->dst.offset);

	strm->avail_out = rte_pktmbuf_data_len(mbuf_dst) - op->dst.offset;

	total_in_at_start = strm->total_in;
	total_out_at_start = strm->total_out;

	/* Set flush value to NO_FLUSH unless it is last mbuf */
	flush = Z_NO_FLUSH;
	/* Initialize status to SUCCESS */
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	do {
		/* Set flush value to Z_FINISH for last block */
		if ((op->src.length - (strm->total_in - total_in_at_start)) <= strm->avail_in) {
			strm->avail_in = (op->src.length - (strm->total_in - total_in_at_start));
			flush = fin_flush;
		}
		do {
			ret = deflate(strm, flush);
			if (unlikely(ret == Z_STREAM_ERROR)) {
				/* error return, do not process further */
				op->status =  RTE_COMP_OP_STATUS_ERROR;
				goto def_end;
			}
			/* Break if Z_STREAM_END is encountered */
			if (ret == Z_STREAM_END)
				goto def_end;

		/* Keep looping until input mbuf is consumed.
		 * Exit if destination mbuf gets exhausted.
		 */
		} while ((strm->avail_out == 0) &&
			COMPUTE_BUF(mbuf_dst, strm->next_out, strm->avail_out));

		if (!strm->avail_out) {
			/* there is no space for compressed output */
			op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
			break;
		}

	/* Update source buffer to next mbuf
	 * Exit if input buffers are fully consumed
	 */
	} while (COMPUTE_BUF(mbuf_src, strm->next_in, strm->avail_in));

def_end:
	/* Update op stats */
	switch (op->status) {
	case RTE_COMP_OP_STATUS_SUCCESS:
		op->consumed += strm->total_in - total_in_at_start;
	/* Fall-through */
	case RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED:
		op->produced += strm->total_out - total_out_at_start;
		break;
	default:
		ZLIB_PMD_ERR("stats not updated for status:%d",
				op->status);
	}

	if (op->flush_flag != RTE_COMP_FLUSH_SYNC)
		deflateReset(strm);
}

static void
process_zlib_inflate(struct rte_comp_op *op, z_stream *strm)
{
	int ret, flush;
	unsigned long total_in_at_start, total_out_at_start;
	struct rte_mbuf *mbuf_src = op->m_src;
	struct rte_mbuf *mbuf_dst = op->m_dst;

	if (unlikely(!strm)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_PMD_ERR("Invalid z_stream");
		return;
	}
	strm->next_in = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *,
			op->src.offset);

	strm->avail_in = rte_pktmbuf_data_len(mbuf_src) - op->src.offset;

	strm->next_out = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
			op->dst.offset);

	strm->avail_out = rte_pktmbuf_data_len(mbuf_dst) - op->dst.offset;

	total_in_at_start = strm->total_in;
	total_out_at_start = strm->total_out;

	/** Ignoring flush value provided from application for decompression */
	flush = Z_NO_FLUSH;
	/* initialize status to SUCCESS */
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	do {
		do {
			ret = inflate(strm, flush);

			switch (ret) {
			/* Fall-through */
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
			/* Fall-through */
			case Z_DATA_ERROR:
			/* Fall-through */
			case Z_MEM_ERROR:
			/* Fall-through */
			case Z_STREAM_ERROR:
				op->status = RTE_COMP_OP_STATUS_ERROR;
			/* Fall-through */
			case Z_STREAM_END:
				/* no further computation needed if
				 * Z_STREAM_END is encountered
				 */
				goto inf_end;
			default:
				/* success */
				break;

			}
		/* Keep looping until input mbuf is consumed.
		 * Exit if destination mbuf gets exhausted.
		 */
		} while ((strm->avail_out == 0) &&
			COMPUTE_BUF(mbuf_dst, strm->next_out, strm->avail_out));

		if (!strm->avail_out) {
			/* there is no more space for decompressed output */
			op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
			break;
		}
	/* Read next input buffer to be processed, exit if compressed
	 * blocks are fully read
	 */
	} while (COMPUTE_BUF(mbuf_src, strm->next_in, strm->avail_in));

inf_end:
	/* Update op stats */
	switch (op->status) {
	case RTE_COMP_OP_STATUS_SUCCESS:
		op->consumed += strm->total_in - total_in_at_start;
	/* Fall-through */
	case RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED:
		op->produced += strm->total_out - total_out_at_start;
		break;
	default:
		ZLIB_PMD_ERR("stats not produced for status:%d",
				op->status);
	}

	if (op->flush_flag != RTE_COMP_FLUSH_SYNC)
		inflateReset(strm);
}

/** Process comp operation for mbuf */
static inline int
process_zlib_op(struct zlib_qp *qp, struct rte_comp_op *op)
{
	struct zlib_stream *stream;
	struct zlib_priv_xform *private_xform;

	if ((op->op_type == RTE_COMP_OP_STATEFUL) ||
			(op->src.offset > rte_pktmbuf_data_len(op->m_src)) ||
			(op->dst.offset > rte_pktmbuf_data_len(op->m_dst))) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_PMD_ERR("Invalid source or destination buffers or "
				  "invalid Operation requested");
	} else {
		private_xform = (struct zlib_priv_xform *)op->private_xform;
		stream = &private_xform->stream;
		stream->chksum(op, &stream->strm, stream->chksum_type);
		if (op->status != RTE_COMP_OP_STATUS_SUCCESS)
			return -1;

		stream->comp(op, &stream->strm);
	}
	/* whatever is out of op, put it into completion queue with
	 * its status
	 */
	return rte_ring_enqueue(qp->processed_pkts, (void *)op);
}

/** Parse comp xform and set private xform/Stream parameters */
int
zlib_set_stream_parameters(const struct rte_comp_xform *xform,
		struct zlib_stream *stream)
{
	int strategy, level, wbits;
	z_stream *strm = &stream->strm;

	/* allocate deflate state */
	strm->zalloc = Z_NULL;
	strm->zfree = Z_NULL;
	strm->opaque = Z_NULL;

	switch (xform->type) {
	case RTE_COMP_COMPRESS:
		stream->comp = process_zlib_deflate;
		stream->free = deflateEnd;
		stream->chksum = process_zlib_deflate_chksum;
		/** Compression window bits */
		switch (xform->compress.algo) {
		case RTE_COMP_ALGO_DEFLATE:
			wbits = -(xform->compress.window_size);
			break;
		default:
			ZLIB_PMD_ERR("Compression algorithm not supported");
			return -1;
		}
		/** Compression Level */
		switch (xform->compress.level) {
		case RTE_COMP_LEVEL_PMD_DEFAULT:
			level = Z_DEFAULT_COMPRESSION;
			break;
		case RTE_COMP_LEVEL_NONE:
			level = Z_NO_COMPRESSION;
			break;
		case RTE_COMP_LEVEL_MIN:
			level = Z_BEST_SPEED;
			break;
		case RTE_COMP_LEVEL_MAX:
			level = Z_BEST_COMPRESSION;
			break;
		default:
			level = xform->compress.level;
			if (level < RTE_COMP_LEVEL_MIN ||
					level > RTE_COMP_LEVEL_MAX) {
				ZLIB_PMD_ERR("Compression level %d "
						"not supported",
						level);
				return -1;
			}
			break;
		}
		/** Compression strategy */
		switch (xform->compress.deflate.huffman) {
		case RTE_COMP_HUFFMAN_DEFAULT:
			strategy = Z_DEFAULT_STRATEGY;
			break;
		case RTE_COMP_HUFFMAN_FIXED:
			strategy = Z_FIXED;
			break;
		case RTE_COMP_HUFFMAN_DYNAMIC:
			strategy = Z_DEFAULT_STRATEGY;
			break;
		default:
			ZLIB_PMD_ERR("Compression strategy not supported");
			return -1;
		}

		/** Checksum used */
		stream->chksum_type = xform->compress.chksum;

		if (deflateInit2(strm, level,
					Z_DEFLATED, wbits,
					DEF_MEM_LEVEL, strategy) != Z_OK) {
			ZLIB_PMD_ERR("Deflate init failed");
			return -1;
		}

		if (xform->compress.deflate.dictionary) {
			if (deflateSetDictionary(strm, xform->compress.deflate.dictionary,
					xform->compress.deflate.dictionary_len)) {
				ZLIB_PMD_ERR("Deflate set dictionary failed");
				return -1;
			}
		}
		break;

	case RTE_COMP_DECOMPRESS:
		stream->comp = process_zlib_inflate;
		stream->free = inflateEnd;
		stream->chksum = process_zlib_inflate_chksum;
		/** window bits */
		switch (xform->decompress.algo) {
		case RTE_COMP_ALGO_DEFLATE:
			wbits = -(xform->decompress.window_size);
			break;
		default:
			ZLIB_PMD_ERR("Compression algorithm not supported");
			return -1;
		}

		/** Checksum used */
		stream->chksum_type = xform->decompress.chksum;

		if (inflateInit2(strm, wbits) != Z_OK) {
			ZLIB_PMD_ERR("Inflate init failed");
			return -1;
		}

		if (xform->decompress.inflate.dictionary) {
			if (inflateSetDictionary(strm, xform->decompress.inflate.dictionary,
					xform->decompress.inflate.dictionary_len)) {
				ZLIB_PMD_ERR("inflate set dictionary failed");
				return -1;
			}
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static uint16_t
zlib_pmd_enqueue_burst(void *queue_pair,
			struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct zlib_qp *qp = queue_pair;
	int ret;
	uint16_t i;
	uint16_t enqd = 0;
	for (i = 0; i < nb_ops; i++) {
		ret = process_zlib_op(qp, ops[i]);
		if (unlikely(ret < 0)) {
			/* increment count if failed to push to completion
			 * queue
			 */
			qp->qp_stats.enqueue_err_count++;
		} else {
			qp->qp_stats.enqueued_count++;
			enqd++;
		}
	}
	return enqd;
}

static uint16_t
zlib_pmd_dequeue_burst(void *queue_pair,
			struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct zlib_qp *qp = queue_pair;

	unsigned int nb_dequeued = 0;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int
zlib_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct zlib_private), init_params);
	if (dev == NULL) {
		ZLIB_PMD_ERR("driver %s: create failed", init_params->name);
		return -ENODEV;
	}

	dev->dev_ops = rte_zlib_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = zlib_pmd_dequeue_burst;
	dev->enqueue_burst = zlib_pmd_enqueue_burst;

	return 0;
}

static int
zlib_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id()
	};
	const char *name;
	const char *input_args;
	int retval;

	name = rte_vdev_device_name(vdev);

	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);

	retval = rte_compressdev_pmd_parse_input_args(&init_params, input_args);
	if (retval < 0) {
		ZLIB_PMD_LOG(ERR,
			"Failed to parse initialisation arguments[%s]",
			input_args);
		return -EINVAL;
	}

	return zlib_create(name, vdev, &init_params);
}

static int
zlib_remove(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compressdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_get_named_dev(name);
	if (compressdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_vdev_driver zlib_pmd_drv = {
	.probe = zlib_probe,
	.remove = zlib_remove
};

RTE_PMD_REGISTER_VDEV(COMPRESSDEV_NAME_ZLIB_PMD, zlib_pmd_drv);
RTE_LOG_REGISTER_DEFAULT(zlib_logtype_driver, INFO);
