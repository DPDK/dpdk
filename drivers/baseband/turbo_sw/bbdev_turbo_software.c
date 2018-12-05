/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_kvargs.h>
#include <rte_cycles.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>

#include <phy_turbo.h>
#include <phy_crc.h>
#include <phy_rate_match.h>
#include <divide.h>

#define DRIVER_NAME baseband_turbo_sw

/* Turbo SW PMD logging ID */
static int bbdev_turbo_sw_logtype;

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, bbdev_turbo_sw_logtype, fmt "\n", \
		##__VA_ARGS__)

#define rte_bbdev_log_debug(fmt, ...) \
	rte_bbdev_log(DEBUG, RTE_STR(__LINE__) ":%s() " fmt, __func__, \
		##__VA_ARGS__)

#define DEINT_INPUT_BUF_SIZE (((RTE_BBDEV_MAX_CB_SIZE >> 3) + 1) * 48)
#define DEINT_OUTPUT_BUF_SIZE (DEINT_INPUT_BUF_SIZE * 6)
#define ADAPTER_OUTPUT_BUF_SIZE ((RTE_BBDEV_MAX_CB_SIZE + 4) * 48)

/* private data structure */
struct bbdev_private {
	unsigned int max_nb_queues;  /**< Max number of queues */
};

/*  Initialisation params structure that can be used by Turbo SW driver */
struct turbo_sw_params {
	int socket_id;  /*< Turbo SW device socket */
	uint16_t queues_num;  /*< Turbo SW device queues number */
};

/* Accecptable params for Turbo SW devices */
#define TURBO_SW_MAX_NB_QUEUES_ARG  "max_nb_queues"
#define TURBO_SW_SOCKET_ID_ARG      "socket_id"

static const char * const turbo_sw_valid_params[] = {
	TURBO_SW_MAX_NB_QUEUES_ARG,
	TURBO_SW_SOCKET_ID_ARG
};

/* queue */
struct turbo_sw_queue {
	/* Ring for processed (encoded/decoded) operations which are ready to
	 * be dequeued.
	 */
	struct rte_ring *processed_pkts;
	/* Stores input for turbo encoder (used when CRC attachment is
	 * performed
	 */
	uint8_t *enc_in;
	/* Stores output from turbo encoder */
	uint8_t *enc_out;
	/* Alpha gamma buf for bblib_turbo_decoder() function */
	int8_t *ag;
	/* Temp buf for bblib_turbo_decoder() function */
	uint16_t *code_block;
	/* Input buf for bblib_rate_dematching_lte() function */
	uint8_t *deint_input;
	/* Output buf for bblib_rate_dematching_lte() function */
	uint8_t *deint_output;
	/* Output buf for bblib_turbodec_adapter_lte() function */
	uint8_t *adapter_output;
	/* Operation type of this queue */
	enum rte_bbdev_op_type type;
} __rte_cache_aligned;

static inline char *
mbuf_append(struct rte_mbuf *m_head, struct rte_mbuf *m, uint16_t len)
{
	if (unlikely(len > rte_pktmbuf_tailroom(m)))
		return NULL;

	char *tail = (char *)m->buf_addr + m->data_off + m->data_len;
	m->data_len = (uint16_t)(m->data_len + len);
	m_head->pkt_len  = (m_head->pkt_len + len);
	return tail;
}

/* Calculate index based on Table 5.1.3-3 from TS34.212 */
static inline int32_t
compute_idx(uint16_t k)
{
	int32_t result = 0;

	if (k < RTE_BBDEV_MIN_CB_SIZE || k > RTE_BBDEV_MAX_CB_SIZE)
		return -1;

	if (k > 2048) {
		if ((k - 2048) % 64 != 0)
			result = -1;

		result = 124 + (k - 2048) / 64;
	} else if (k <= 512) {
		if ((k - 40) % 8 != 0)
			result = -1;

		result = (k - 40) / 8 + 1;
	} else if (k <= 1024) {
		if ((k - 512) % 16 != 0)
			result = -1;

		result = 60 + (k - 512) / 16;
	} else { /* 1024 < k <= 2048 */
		if ((k - 1024) % 32 != 0)
			result = -1;

		result = 92 + (k - 1024) / 32;
	}

	return result;
}

/* Read flag value 0/1 from bitmap */
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
	return bitmap & bitmask;
}

/* Get device info */
static void
info_get(struct rte_bbdev *dev, struct rte_bbdev_driver_info *dev_info)
{
	struct bbdev_private *internals = dev->data->dev_private;

	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		{
			.type = RTE_BBDEV_OP_TURBO_DEC,
			.cap.turbo_dec = {
				.capability_flags =
					RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE |
					RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN |
					RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN |
					RTE_BBDEV_TURBO_CRC_TYPE_24B |
					RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP |
					RTE_BBDEV_TURBO_EARLY_TERMINATION,
				.max_llr_modulus = 16,
				.num_buffers_src = RTE_BBDEV_MAX_CODE_BLOCKS,
				.num_buffers_hard_out =
						RTE_BBDEV_MAX_CODE_BLOCKS,
				.num_buffers_soft_out = 0,
			}
		},
		{
			.type   = RTE_BBDEV_OP_TURBO_ENC,
			.cap.turbo_enc = {
				.capability_flags =
						RTE_BBDEV_TURBO_CRC_24B_ATTACH |
						RTE_BBDEV_TURBO_CRC_24A_ATTACH |
						RTE_BBDEV_TURBO_RATE_MATCH |
						RTE_BBDEV_TURBO_RV_INDEX_BYPASS,
				.num_buffers_src = RTE_BBDEV_MAX_CODE_BLOCKS,
				.num_buffers_dst = RTE_BBDEV_MAX_CODE_BLOCKS,
			}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static struct rte_bbdev_queue_conf default_queue_conf = {
		.queue_size = RTE_BBDEV_QUEUE_SIZE_LIMIT,
	};

	static const enum rte_cpu_flag_t cpu_flag = RTE_CPUFLAG_SSE4_2;

	default_queue_conf.socket = dev->data->socket_id;

	dev_info->driver_name = RTE_STR(DRIVER_NAME);
	dev_info->max_num_queues = internals->max_nb_queues;
	dev_info->queue_size_lim = RTE_BBDEV_QUEUE_SIZE_LIMIT;
	dev_info->hardware_accelerated = false;
	dev_info->max_dl_queue_priority = 0;
	dev_info->max_ul_queue_priority = 0;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->cpu_flag_reqs = &cpu_flag;
	dev_info->min_alignment = 64;

	rte_bbdev_log_debug("got device info from %u\n", dev->data->dev_id);
}

/* Release queue */
static int
q_release(struct rte_bbdev *dev, uint16_t q_id)
{
	struct turbo_sw_queue *q = dev->data->queues[q_id].queue_private;

	if (q != NULL) {
		rte_ring_free(q->processed_pkts);
		rte_free(q->enc_out);
		rte_free(q->enc_in);
		rte_free(q->ag);
		rte_free(q->code_block);
		rte_free(q->deint_input);
		rte_free(q->deint_output);
		rte_free(q->adapter_output);
		rte_free(q);
		dev->data->queues[q_id].queue_private = NULL;
	}

	rte_bbdev_log_debug("released device queue %u:%u",
			dev->data->dev_id, q_id);
	return 0;
}

/* Setup a queue */
static int
q_setup(struct rte_bbdev *dev, uint16_t q_id,
		const struct rte_bbdev_queue_conf *queue_conf)
{
	int ret;
	struct turbo_sw_queue *q;
	char name[RTE_RING_NAMESIZE];

	/* Allocate the queue data structure. */
	q = rte_zmalloc_socket(RTE_STR(DRIVER_NAME), sizeof(*q),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate queue memory");
		return -ENOMEM;
	}

	/* Allocate memory for encoder output. */
	ret = snprintf(name, RTE_RING_NAMESIZE, RTE_STR(DRIVER_NAME)"_enc_o%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->enc_out = rte_zmalloc_socket(name,
			((RTE_BBDEV_MAX_TB_SIZE >> 3) + 3) *
			sizeof(*q->enc_out) * 3,
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->enc_out == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Allocate memory for rate matching output. */
	ret = snprintf(name, RTE_RING_NAMESIZE,
			RTE_STR(DRIVER_NAME)"_enc_i%u:%u", dev->data->dev_id,
			q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->enc_in = rte_zmalloc_socket(name,
			(RTE_BBDEV_MAX_CB_SIZE >> 3) * sizeof(*q->enc_in),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->enc_in == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Allocate memory for Aplha Gamma temp buffer. */
	ret = snprintf(name, RTE_RING_NAMESIZE, RTE_STR(DRIVER_NAME)"_ag%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->ag = rte_zmalloc_socket(name,
			RTE_BBDEV_MAX_CB_SIZE * 10 * sizeof(*q->ag),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->ag == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Allocate memory for code block temp buffer. */
	ret = snprintf(name, RTE_RING_NAMESIZE, RTE_STR(DRIVER_NAME)"_cb%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->code_block = rte_zmalloc_socket(name,
			RTE_BBDEV_MAX_CB_SIZE * sizeof(*q->code_block),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->code_block == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Allocate memory for Deinterleaver input. */
	ret = snprintf(name, RTE_RING_NAMESIZE,
			RTE_STR(DRIVER_NAME)"_de_i%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->deint_input = rte_zmalloc_socket(name,
			DEINT_INPUT_BUF_SIZE * sizeof(*q->deint_input),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->deint_input == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Allocate memory for Deinterleaver output. */
	ret = snprintf(name, RTE_RING_NAMESIZE,
			RTE_STR(DRIVER_NAME)"_de_o%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->deint_output = rte_zmalloc_socket(NULL,
			DEINT_OUTPUT_BUF_SIZE * sizeof(*q->deint_output),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->deint_output == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Allocate memory for Adapter output. */
	ret = snprintf(name, RTE_RING_NAMESIZE,
			RTE_STR(DRIVER_NAME)"_ada_o%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->adapter_output = rte_zmalloc_socket(NULL,
			ADAPTER_OUTPUT_BUF_SIZE * sizeof(*q->adapter_output),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q->adapter_output == NULL) {
		rte_bbdev_log(ERR,
			"Failed to allocate queue memory for %s", name);
		goto free_q;
	}

	/* Create ring for packets awaiting to be dequeued. */
	ret = snprintf(name, RTE_RING_NAMESIZE, RTE_STR(DRIVER_NAME)"%u:%u",
			dev->data->dev_id, q_id);
	if ((ret < 0) || (ret >= (int)RTE_RING_NAMESIZE)) {
		rte_bbdev_log(ERR,
				"Creating queue name for device %u queue %u failed",
				dev->data->dev_id, q_id);
		return -ENAMETOOLONG;
	}
	q->processed_pkts = rte_ring_create(name, queue_conf->queue_size,
			queue_conf->socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (q->processed_pkts == NULL) {
		rte_bbdev_log(ERR, "Failed to create ring for %s", name);
		goto free_q;
	}

	q->type = queue_conf->op_type;

	dev->data->queues[q_id].queue_private = q;
	rte_bbdev_log_debug("setup device queue %s", name);
	return 0;

free_q:
	rte_ring_free(q->processed_pkts);
	rte_free(q->enc_out);
	rte_free(q->enc_in);
	rte_free(q->ag);
	rte_free(q->code_block);
	rte_free(q->deint_input);
	rte_free(q->deint_output);
	rte_free(q->adapter_output);
	rte_free(q);
	return -EFAULT;
}

static const struct rte_bbdev_ops pmd_ops = {
	.info_get = info_get,
	.queue_setup = q_setup,
	.queue_release = q_release
};

/* Checks if the encoder input buffer is correct.
 * Returns 0 if it's valid, -1 otherwise.
 */
static inline int
is_enc_input_valid(const uint16_t k, const int32_t k_idx,
		const uint16_t in_length)
{
	if (k_idx < 0) {
		rte_bbdev_log(ERR, "K Index is invalid");
		return -1;
	}

	if (in_length - (k >> 3) < 0) {
		rte_bbdev_log(ERR,
				"Mismatch between input length (%u bytes) and K (%u bits)",
				in_length, k);
		return -1;
	}

	if (k > RTE_BBDEV_MAX_CB_SIZE) {
		rte_bbdev_log(ERR, "CB size (%u) is too big, max: %d",
				k, RTE_BBDEV_MAX_CB_SIZE);
		return -1;
	}

	return 0;
}

/* Checks if the decoder input buffer is correct.
 * Returns 0 if it's valid, -1 otherwise.
 */
static inline int
is_dec_input_valid(int32_t k_idx, int16_t kw, int16_t in_length)
{
	if (k_idx < 0) {
		rte_bbdev_log(ERR, "K index is invalid");
		return -1;
	}

	if (in_length < kw) {
		rte_bbdev_log(ERR,
				"Mismatch between input length (%u) and kw (%u)",
				in_length, kw);
		return -1;
	}

	if (kw > RTE_BBDEV_MAX_KW) {
		rte_bbdev_log(ERR, "Input length (%u) is too big, max: %d",
				kw, RTE_BBDEV_MAX_KW);
		return -1;
	}

	return 0;
}

static inline void
process_enc_cb(struct turbo_sw_queue *q, struct rte_bbdev_enc_op *op,
		uint8_t r, uint8_t c, uint16_t k, uint16_t ncb,
		uint32_t e, struct rte_mbuf *m_in, struct rte_mbuf *m_out_head,
		struct rte_mbuf *m_out, uint16_t in_offset, uint16_t out_offset,
		uint16_t in_length, struct rte_bbdev_stats *q_stats)
{
	int ret;
	int16_t k_idx;
	uint16_t m;
	uint8_t *in, *out0, *out1, *out2, *tmp_out, *rm_out;
	uint64_t first_3_bytes = 0;
	struct rte_bbdev_op_turbo_enc *enc = &op->turbo_enc;
	struct bblib_crc_request crc_req;
	struct bblib_crc_response crc_resp;
	struct bblib_turbo_encoder_request turbo_req;
	struct bblib_turbo_encoder_response turbo_resp;
	struct bblib_rate_match_dl_request rm_req;
	struct bblib_rate_match_dl_response rm_resp;
#ifdef RTE_BBDEV_OFFLOAD_COST
	uint64_t start_time;
#else
	RTE_SET_USED(q_stats);
#endif

	k_idx = compute_idx(k);
	in = rte_pktmbuf_mtod_offset(m_in, uint8_t *, in_offset);

	/* CRC24A (for TB) */
	if ((enc->op_flags & RTE_BBDEV_TURBO_CRC_24A_ATTACH) &&
		(enc->code_block_mode == 1)) {
		ret = is_enc_input_valid(k - 24, k_idx, in_length);
		if (ret != 0) {
			op->status |= 1 << RTE_BBDEV_DATA_ERROR;
			return;
		}
		crc_req.data = in;
		crc_req.len = k - 24;
		/* Check if there is a room for CRC bits if not use
		 * the temporary buffer.
		 */
		if (mbuf_append(m_in, m_in, 3) == NULL) {
			rte_memcpy(q->enc_in, in, (k - 24) >> 3);
			in = q->enc_in;
		} else {
			/* Store 3 first bytes of next CB as they will be
			 * overwritten by CRC bytes. If it is the last CB then
			 * there is no point to store 3 next bytes and this
			 * if..else branch will be omitted.
			 */
			first_3_bytes = *((uint64_t *)&in[(k - 32) >> 3]);
		}

		crc_resp.data = in;
#ifdef RTE_BBDEV_OFFLOAD_COST
		start_time = rte_rdtsc_precise();
#endif
		/* CRC24A generation */
		bblib_lte_crc24a_gen(&crc_req, &crc_resp);
#ifdef RTE_BBDEV_OFFLOAD_COST
		q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif
	} else if (enc->op_flags & RTE_BBDEV_TURBO_CRC_24B_ATTACH) {
		/* CRC24B */
		ret = is_enc_input_valid(k - 24, k_idx, in_length);
		if (ret != 0) {
			op->status |= 1 << RTE_BBDEV_DATA_ERROR;
			return;
		}
		crc_req.data = in;
		crc_req.len = k - 24;
		/* Check if there is a room for CRC bits if this is the last
		 * CB in TB. If not use temporary buffer.
		 */
		if ((c - r == 1) && (mbuf_append(m_in, m_in, 3) == NULL)) {
			rte_memcpy(q->enc_in, in, (k - 24) >> 3);
			in = q->enc_in;
		} else if (c - r > 1) {
			/* Store 3 first bytes of next CB as they will be
			 * overwritten by CRC bytes. If it is the last CB then
			 * there is no point to store 3 next bytes and this
			 * if..else branch will be omitted.
			 */
			first_3_bytes = *((uint64_t *)&in[(k - 32) >> 3]);
		}

		crc_resp.data = in;
#ifdef RTE_BBDEV_OFFLOAD_COST
		start_time = rte_rdtsc_precise();
#endif
		/* CRC24B generation */
		bblib_lte_crc24b_gen(&crc_req, &crc_resp);
#ifdef RTE_BBDEV_OFFLOAD_COST
		q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif
	} else {
		ret = is_enc_input_valid(k, k_idx, in_length);
		if (ret != 0) {
			op->status |= 1 << RTE_BBDEV_DATA_ERROR;
			return;
		}
	}

	/* Turbo encoder */

	/* Each bit layer output from turbo encoder is (k+4) bits long, i.e.
	 * input length + 4 tail bits. That's (k/8) + 1 bytes after rounding up.
	 * So dst_data's length should be 3*(k/8) + 3 bytes.
	 * In Rate-matching bypass case outputs pointers passed to encoder
	 * (out0, out1 and out2) can directly point to addresses of output from
	 * turbo_enc entity.
	 */
	if (enc->op_flags & RTE_BBDEV_TURBO_RATE_MATCH) {
		out0 = q->enc_out;
		out1 = RTE_PTR_ADD(out0, (k >> 3) + 1);
		out2 = RTE_PTR_ADD(out1, (k >> 3) + 1);
	} else {
		out0 = (uint8_t *)mbuf_append(m_out_head, m_out,
				(k >> 3) * 3 + 2);
		if (out0 == NULL) {
			op->status |= 1 << RTE_BBDEV_DATA_ERROR;
			rte_bbdev_log(ERR,
					"Too little space in output mbuf");
			return;
		}
		enc->output.length += (k >> 3) * 3 + 2;
		/* rte_bbdev_op_data.offset can be different than the
		 * offset of the appended bytes
		 */
		out0 = rte_pktmbuf_mtod_offset(m_out, uint8_t *, out_offset);
		out1 = rte_pktmbuf_mtod_offset(m_out, uint8_t *,
				out_offset + (k >> 3) + 1);
		out2 = rte_pktmbuf_mtod_offset(m_out, uint8_t *,
				out_offset + 2 * ((k >> 3) + 1));
	}

	turbo_req.case_id = k_idx;
	turbo_req.input_win = in;
	turbo_req.length = k >> 3;
	turbo_resp.output_win_0 = out0;
	turbo_resp.output_win_1 = out1;
	turbo_resp.output_win_2 = out2;

#ifdef RTE_BBDEV_OFFLOAD_COST
	start_time = rte_rdtsc_precise();
#endif
	/* Turbo encoding */
	if (bblib_turbo_encoder(&turbo_req, &turbo_resp) != 0) {
		op->status |= 1 << RTE_BBDEV_DRV_ERROR;
		rte_bbdev_log(ERR, "Turbo Encoder failed");
		return;
	}
#ifdef RTE_BBDEV_OFFLOAD_COST
	q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif

	/* Restore 3 first bytes of next CB if they were overwritten by CRC*/
	if (first_3_bytes != 0)
		*((uint64_t *)&in[(k - 32) >> 3]) = first_3_bytes;

	/* Rate-matching */
	if (enc->op_flags & RTE_BBDEV_TURBO_RATE_MATCH) {
		uint8_t mask_id;
		/* Integer round up division by 8 */
		uint16_t out_len = (e + 7) >> 3;
		/* The mask array is indexed using E%8. E is an even number so
		 * there are only 4 possible values.
		 */
		const uint8_t mask_out[] = {0xFF, 0xC0, 0xF0, 0xFC};

		/* get output data starting address */
		rm_out = (uint8_t *)mbuf_append(m_out_head, m_out, out_len);
		if (rm_out == NULL) {
			op->status |= 1 << RTE_BBDEV_DATA_ERROR;
			rte_bbdev_log(ERR,
					"Too little space in output mbuf");
			return;
		}
		/* rte_bbdev_op_data.offset can be different than the offset
		 * of the appended bytes
		 */
		rm_out = rte_pktmbuf_mtod_offset(m_out, uint8_t *, out_offset);

		/* index of current code block */
		rm_req.r = r;
		/* total number of code block */
		rm_req.C = c;
		/* For DL - 1, UL - 0 */
		rm_req.direction = 1;
		/* According to 3ggp 36.212 Spec 5.1.4.1.2 section Nsoft, KMIMO
		 * and MDL_HARQ are used for Ncb calculation. As Ncb is already
		 * known we can adjust those parameters
		 */
		rm_req.Nsoft = ncb * rm_req.C;
		rm_req.KMIMO = 1;
		rm_req.MDL_HARQ = 1;
		/* According to 3ggp 36.212 Spec 5.1.4.1.2 section Nl, Qm and G
		 * are used for E calculation. As E is already known we can
		 * adjust those parameters
		 */
		rm_req.NL = e;
		rm_req.Qm = 1;
		rm_req.G = rm_req.NL * rm_req.Qm * rm_req.C;

		rm_req.rvidx = enc->rv_index;
		rm_req.Kidx = k_idx - 1;
		rm_req.nLen = k + 4;
		rm_req.tin0 = out0;
		rm_req.tin1 = out1;
		rm_req.tin2 = out2;
		rm_resp.output = rm_out;
		rm_resp.OutputLen = out_len;
		if (enc->op_flags & RTE_BBDEV_TURBO_RV_INDEX_BYPASS)
			rm_req.bypass_rvidx = 1;
		else
			rm_req.bypass_rvidx = 0;

#ifdef RTE_BBDEV_OFFLOAD_COST
		start_time = rte_rdtsc_precise();
#endif
		/* Rate-Matching */
		if (bblib_rate_match_dl(&rm_req, &rm_resp) != 0) {
			op->status |= 1 << RTE_BBDEV_DRV_ERROR;
			rte_bbdev_log(ERR, "Rate matching failed");
			return;
		}
#ifdef RTE_BBDEV_OFFLOAD_COST
		q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif

		/* SW fills an entire last byte even if E%8 != 0. Clear the
		 * superfluous data bits for consistency with HW device.
		 */
		mask_id = (e & 7) >> 1;
		rm_out[out_len - 1] &= mask_out[mask_id];
		enc->output.length += rm_resp.OutputLen;
	} else {
		/* Rate matching is bypassed */

		/* Completing last byte of out0 (where 4 tail bits are stored)
		 * by moving first 4 bits from out1
		 */
		tmp_out = (uint8_t *) --out1;
		*tmp_out = *tmp_out | ((*(tmp_out + 1) & 0xF0) >> 4);
		tmp_out++;
		/* Shifting out1 data by 4 bits to the left */
		for (m = 0; m < k >> 3; ++m) {
			uint8_t *first = tmp_out;
			uint8_t second = *(tmp_out + 1);
			*first = (*first << 4) | ((second & 0xF0) >> 4);
			tmp_out++;
		}
		/* Shifting out2 data by 8 bits to the left */
		for (m = 0; m < (k >> 3) + 1; ++m) {
			*tmp_out = *(tmp_out + 1);
			tmp_out++;
		}
		*tmp_out = 0;
	}
}

static inline void
enqueue_enc_one_op(struct turbo_sw_queue *q, struct rte_bbdev_enc_op *op,
		struct rte_bbdev_stats *queue_stats)
{
	uint8_t c, r, crc24_bits = 0;
	uint16_t k, ncb;
	uint32_t e;
	struct rte_bbdev_op_turbo_enc *enc = &op->turbo_enc;
	uint16_t in_offset = enc->input.offset;
	uint16_t out_offset = enc->output.offset;
	struct rte_mbuf *m_in = enc->input.data;
	struct rte_mbuf *m_out = enc->output.data;
	struct rte_mbuf *m_out_head = enc->output.data;
	uint32_t in_length, mbuf_total_left = enc->input.length;
	uint16_t seg_total_left;

	/* Clear op status */
	op->status = 0;

	if (mbuf_total_left > RTE_BBDEV_MAX_TB_SIZE >> 3) {
		rte_bbdev_log(ERR, "TB size (%u) is too big, max: %d",
				mbuf_total_left, RTE_BBDEV_MAX_TB_SIZE);
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return;
	}

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return;
	}

	if ((enc->op_flags & RTE_BBDEV_TURBO_CRC_24B_ATTACH) ||
		(enc->op_flags & RTE_BBDEV_TURBO_CRC_24A_ATTACH))
		crc24_bits = 24;

	if (enc->code_block_mode == 0) { /* For Transport Block mode */
		c = enc->tb_params.c;
		r = enc->tb_params.r;
	} else {/* For Code Block mode */
		c = 1;
		r = 0;
	}

	while (mbuf_total_left > 0 && r < c) {

		seg_total_left = rte_pktmbuf_data_len(m_in) - in_offset;

		if (enc->code_block_mode == 0) {
			k = (r < enc->tb_params.c_neg) ?
				enc->tb_params.k_neg : enc->tb_params.k_pos;
			ncb = (r < enc->tb_params.c_neg) ?
				enc->tb_params.ncb_neg : enc->tb_params.ncb_pos;
			e = (r < enc->tb_params.cab) ?
				enc->tb_params.ea : enc->tb_params.eb;
		} else {
			k = enc->cb_params.k;
			ncb = enc->cb_params.ncb;
			e = enc->cb_params.e;
		}

		process_enc_cb(q, op, r, c, k, ncb, e, m_in, m_out_head,
				m_out, in_offset, out_offset, seg_total_left,
				queue_stats);
		/* Update total_left */
		in_length = ((k - crc24_bits) >> 3);
		mbuf_total_left -= in_length;
		/* Update offsets for next CBs (if exist) */
		in_offset += (k - crc24_bits) >> 3;
		if (enc->op_flags & RTE_BBDEV_TURBO_RATE_MATCH)
			out_offset += e >> 3;
		else
			out_offset += (k >> 3) * 3 + 2;

		/* Update offsets */
		if (seg_total_left == in_length) {
			/* Go to the next mbuf */
			m_in = m_in->next;
			m_out = m_out->next;
			in_offset = 0;
			out_offset = 0;
		}
		r++;
	}

	/* check if all input data was processed */
	if (mbuf_total_left != 0) {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CBs sizes");
	}
}

static inline uint16_t
enqueue_enc_all_ops(struct turbo_sw_queue *q, struct rte_bbdev_enc_op **ops,
		uint16_t nb_ops, struct rte_bbdev_stats *queue_stats)
{
	uint16_t i;
#ifdef RTE_BBDEV_OFFLOAD_COST
	queue_stats->acc_offload_cycles = 0;
#endif

	for (i = 0; i < nb_ops; ++i)
		enqueue_enc_one_op(q, ops[i], queue_stats);

	return rte_ring_enqueue_burst(q->processed_pkts, (void **)ops, nb_ops,
			NULL);
}

static inline void
move_padding_bytes(const uint8_t *in, uint8_t *out, uint16_t k,
		uint16_t ncb)
{
	uint16_t d = k + 4;
	uint16_t kpi = ncb / 3;
	uint16_t nd = kpi - d;

	rte_memcpy(&out[nd], in, d);
	rte_memcpy(&out[nd + kpi + 64], &in[kpi], d);
	rte_memcpy(&out[(nd - 1) + 2 * (kpi + 64)], &in[2 * kpi], d);
}

static inline void
process_dec_cb(struct turbo_sw_queue *q, struct rte_bbdev_dec_op *op,
		uint8_t c, uint16_t k, uint16_t kw, struct rte_mbuf *m_in,
		struct rte_mbuf *m_out_head, struct rte_mbuf *m_out,
		uint16_t in_offset, uint16_t out_offset, bool check_crc_24b,
		uint16_t crc24_overlap, uint16_t in_length,
		struct rte_bbdev_stats *q_stats)
{
	int ret;
	int32_t k_idx;
	int32_t iter_cnt;
	uint8_t *in, *out, *adapter_input;
	int32_t ncb, ncb_without_null;
	struct bblib_turbo_adapter_ul_response adapter_resp;
	struct bblib_turbo_adapter_ul_request adapter_req;
	struct bblib_turbo_decoder_request turbo_req;
	struct bblib_turbo_decoder_response turbo_resp;
	struct rte_bbdev_op_turbo_dec *dec = &op->turbo_dec;
#ifdef RTE_BBDEV_OFFLOAD_COST
	uint64_t start_time;
#else
	RTE_SET_USED(q_stats);
#endif

	k_idx = compute_idx(k);

	ret = is_dec_input_valid(k_idx, kw, in_length);
	if (ret != 0) {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		return;
	}

	in = rte_pktmbuf_mtod_offset(m_in, uint8_t *, in_offset);
	ncb = kw;
	ncb_without_null = (k + 4) * 3;

	if (check_bit(dec->op_flags, RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE)) {
		struct bblib_deinterleave_ul_request deint_req;
		struct bblib_deinterleave_ul_response deint_resp;

		deint_req.circ_buffer = BBLIB_FULL_CIRCULAR_BUFFER;
		deint_req.pharqbuffer = in;
		deint_req.ncb = ncb;
		deint_resp.pinteleavebuffer = q->deint_output;

#ifdef RTE_BBDEV_OFFLOAD_COST
		start_time = rte_rdtsc_precise();
#endif
		bblib_deinterleave_ul(&deint_req, &deint_resp);
#ifdef RTE_BBDEV_OFFLOAD_COST
		q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif
	} else
		move_padding_bytes(in, q->deint_output, k, ncb);

	adapter_input = q->deint_output;

	if (dec->op_flags & RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN)
		adapter_req.isinverted = 1;
	else if (dec->op_flags & RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN)
		adapter_req.isinverted = 0;
	else {
		op->status |= 1 << RTE_BBDEV_DRV_ERROR;
		rte_bbdev_log(ERR, "LLR format wasn't specified");
		return;
	}

	adapter_req.ncb = ncb_without_null;
	adapter_req.pinteleavebuffer = adapter_input;
	adapter_resp.pharqout = q->adapter_output;

#ifdef RTE_BBDEV_OFFLOAD_COST
	start_time = rte_rdtsc_precise();
#endif
	/* Turbo decode adaptation */
	bblib_turbo_adapter_ul(&adapter_req, &adapter_resp);
#ifdef RTE_BBDEV_OFFLOAD_COST
	q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif

	out = (uint8_t *)mbuf_append(m_out_head, m_out,
			((k - crc24_overlap) >> 3));
	if (out == NULL) {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log(ERR, "Too little space in output mbuf");
		return;
	}
	/* rte_bbdev_op_data.offset can be different than the offset of the
	 * appended bytes
	 */
	out = rte_pktmbuf_mtod_offset(m_out, uint8_t *, out_offset);
	if (check_crc_24b)
		turbo_req.c = c + 1;
	else
		turbo_req.c = c;
	turbo_req.input = (int8_t *)q->adapter_output;
	turbo_req.k = k;
	turbo_req.k_idx = k_idx;
	turbo_req.max_iter_num = dec->iter_max;
	turbo_req.early_term_disable = !check_bit(dec->op_flags,
			RTE_BBDEV_TURBO_EARLY_TERMINATION);
	turbo_resp.ag_buf = q->ag;
	turbo_resp.cb_buf = q->code_block;
	turbo_resp.output = out;

#ifdef RTE_BBDEV_OFFLOAD_COST
	start_time = rte_rdtsc_precise();
#endif
	/* Turbo decode */
	iter_cnt = bblib_turbo_decoder(&turbo_req, &turbo_resp);
#ifdef RTE_BBDEV_OFFLOAD_COST
	q_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
#endif
	dec->hard_output.length += (k >> 3);

	if (iter_cnt > 0) {
		/* Temporary solution for returned iter_count from SDK */
		iter_cnt = (iter_cnt - 1) >> 1;
		dec->iter_count = RTE_MAX(iter_cnt, dec->iter_count);
	} else {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log(ERR, "Turbo Decoder failed");
		return;
	}
}

static inline void
enqueue_dec_one_op(struct turbo_sw_queue *q, struct rte_bbdev_dec_op *op,
		struct rte_bbdev_stats *queue_stats)
{
	uint8_t c, r = 0;
	uint16_t kw, k = 0;
	uint16_t crc24_overlap = 0;
	struct rte_bbdev_op_turbo_dec *dec = &op->turbo_dec;
	struct rte_mbuf *m_in = dec->input.data;
	struct rte_mbuf *m_out = dec->hard_output.data;
	struct rte_mbuf *m_out_head = dec->hard_output.data;
	uint16_t in_offset = dec->input.offset;
	uint16_t out_offset = dec->hard_output.offset;
	uint32_t mbuf_total_left = dec->input.length;
	uint16_t seg_total_left;

	/* Clear op status */
	op->status = 0;

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return;
	}

	if (dec->code_block_mode == 0) { /* For Transport Block mode */
		c = dec->tb_params.c;
	} else { /* For Code Block mode */
		k = dec->cb_params.k;
		c = 1;
	}

	if ((c > 1) && !check_bit(dec->op_flags,
		RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP))
		crc24_overlap = 24;

	while (mbuf_total_left > 0) {
		if (dec->code_block_mode == 0)
			k = (r < dec->tb_params.c_neg) ?
				dec->tb_params.k_neg : dec->tb_params.k_pos;

		seg_total_left = rte_pktmbuf_data_len(m_in) - in_offset;

		/* Calculates circular buffer size (Kw).
		 * According to 3gpp 36.212 section 5.1.4.2
		 *   Kw = 3 * Kpi,
		 * where:
		 *   Kpi = nCol * nRow
		 * where nCol is 32 and nRow can be calculated from:
		 *   D =< nCol * nRow
		 * where D is the size of each output from turbo encoder block
		 * (k + 4).
		 */
		kw = RTE_ALIGN_CEIL(k + 4, RTE_BBDEV_C_SUBBLOCK) * 3;

		process_dec_cb(q, op, c, k, kw, m_in, m_out_head, m_out,
				in_offset, out_offset, check_bit(dec->op_flags,
				RTE_BBDEV_TURBO_CRC_TYPE_24B), crc24_overlap,
				seg_total_left, queue_stats);
		/* To keep CRC24 attached to end of Code block, use
		 * RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP flag as it
		 * removed by default once verified.
		 */

		mbuf_total_left -= kw;

		/* Update offsets */
		if (seg_total_left == kw) {
			/* Go to the next mbuf */
			m_in = m_in->next;
			m_out = m_out->next;
			in_offset = 0;
			out_offset = 0;
		} else {
			/* Update offsets for next CBs (if exist) */
			in_offset += kw;
			out_offset += ((k - crc24_overlap) >> 3);
		}
		r++;
	}
	if (mbuf_total_left != 0) {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included Circular buffer sizes");
	}
}

static inline uint16_t
enqueue_dec_all_ops(struct turbo_sw_queue *q, struct rte_bbdev_dec_op **ops,
		uint16_t nb_ops, struct rte_bbdev_stats *queue_stats)
{
	uint16_t i;
#ifdef RTE_BBDEV_OFFLOAD_COST
	queue_stats->acc_offload_cycles = 0;
#endif

	for (i = 0; i < nb_ops; ++i)
		enqueue_dec_one_op(q, ops[i], queue_stats);

	return rte_ring_enqueue_burst(q->processed_pkts, (void **)ops, nb_ops,
			NULL);
}

/* Enqueue burst */
static uint16_t
enqueue_enc_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
	void *queue = q_data->queue_private;
	struct turbo_sw_queue *q = queue;
	uint16_t nb_enqueued = 0;

	nb_enqueued = enqueue_enc_all_ops(q, ops, nb_ops, &q_data->queue_stats);

	q_data->queue_stats.enqueue_err_count += nb_ops - nb_enqueued;
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/* Enqueue burst */
static uint16_t
enqueue_dec_ops(struct rte_bbdev_queue_data *q_data,
		 struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
	void *queue = q_data->queue_private;
	struct turbo_sw_queue *q = queue;
	uint16_t nb_enqueued = 0;

	nb_enqueued = enqueue_dec_all_ops(q, ops, nb_ops, &q_data->queue_stats);

	q_data->queue_stats.enqueue_err_count += nb_ops - nb_enqueued;
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/* Dequeue decode burst */
static uint16_t
dequeue_dec_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
	struct turbo_sw_queue *q = q_data->queue_private;
	uint16_t nb_dequeued = rte_ring_dequeue_burst(q->processed_pkts,
			(void **)ops, nb_ops, NULL);
	q_data->queue_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/* Dequeue encode burst */
static uint16_t
dequeue_enc_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
	struct turbo_sw_queue *q = q_data->queue_private;
	uint16_t nb_dequeued = rte_ring_dequeue_burst(q->processed_pkts,
			(void **)ops, nb_ops, NULL);
	q_data->queue_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/* Parse 16bit integer from string argument */
static inline int
parse_u16_arg(const char *key, const char *value, void *extra_args)
{
	uint16_t *u16 = extra_args;
	unsigned int long result;

	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;
	errno = 0;
	result = strtoul(value, NULL, 0);
	if ((result >= (1 << 16)) || (errno != 0)) {
		rte_bbdev_log(ERR, "Invalid value %lu for %s", result, key);
		return -ERANGE;
	}
	*u16 = (uint16_t)result;
	return 0;
}

/* Parse parameters used to create device */
static int
parse_turbo_sw_params(struct turbo_sw_params *params, const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;
	if (input_args) {
		kvlist = rte_kvargs_parse(input_args, turbo_sw_valid_params);
		if (kvlist == NULL)
			return -EFAULT;

		ret = rte_kvargs_process(kvlist, turbo_sw_valid_params[0],
					&parse_u16_arg, &params->queues_num);
		if (ret < 0)
			goto exit;

		ret = rte_kvargs_process(kvlist, turbo_sw_valid_params[1],
					&parse_u16_arg, &params->socket_id);
		if (ret < 0)
			goto exit;

		if (params->socket_id >= RTE_MAX_NUMA_NODES) {
			rte_bbdev_log(ERR, "Invalid socket, must be < %u",
					RTE_MAX_NUMA_NODES);
			goto exit;
		}
	}

exit:
	if (kvlist)
		rte_kvargs_free(kvlist);
	return ret;
}

/* Create device */
static int
turbo_sw_bbdev_create(struct rte_vdev_device *vdev,
		struct turbo_sw_params *init_params)
{
	struct rte_bbdev *bbdev;
	const char *name = rte_vdev_device_name(vdev);

	bbdev = rte_bbdev_allocate(name);
	if (bbdev == NULL)
		return -ENODEV;

	bbdev->data->dev_private = rte_zmalloc_socket(name,
			sizeof(struct bbdev_private), RTE_CACHE_LINE_SIZE,
			init_params->socket_id);
	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_release(bbdev);
		return -ENOMEM;
	}

	bbdev->dev_ops = &pmd_ops;
	bbdev->device = &vdev->device;
	bbdev->data->socket_id = init_params->socket_id;
	bbdev->intr_handle = NULL;

	/* register rx/tx burst functions for data path */
	bbdev->dequeue_enc_ops = dequeue_enc_ops;
	bbdev->dequeue_dec_ops = dequeue_dec_ops;
	bbdev->enqueue_enc_ops = enqueue_enc_ops;
	bbdev->enqueue_dec_ops = enqueue_dec_ops;
	((struct bbdev_private *) bbdev->data->dev_private)->max_nb_queues =
			init_params->queues_num;

	return 0;
}

/* Initialise device */
static int
turbo_sw_bbdev_probe(struct rte_vdev_device *vdev)
{
	struct turbo_sw_params init_params = {
		rte_socket_id(),
		RTE_BBDEV_DEFAULT_MAX_NB_QUEUES
	};
	const char *name;
	const char *input_args;

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);
	parse_turbo_sw_params(&init_params, input_args);

	rte_bbdev_log_debug(
			"Initialising %s on NUMA node %d with max queues: %d\n",
			name, init_params.socket_id, init_params.queues_num);

	return turbo_sw_bbdev_create(vdev, &init_params);
}

/* Uninitialise device */
static int
turbo_sw_bbdev_remove(struct rte_vdev_device *vdev)
{
	struct rte_bbdev *bbdev;
	const char *name;

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	bbdev = rte_bbdev_get_named_dev(name);
	if (bbdev == NULL)
		return -EINVAL;

	rte_free(bbdev->data->dev_private);

	return rte_bbdev_release(bbdev);
}

static struct rte_vdev_driver bbdev_turbo_sw_pmd_drv = {
	.probe = turbo_sw_bbdev_probe,
	.remove = turbo_sw_bbdev_remove
};

RTE_PMD_REGISTER_VDEV(DRIVER_NAME, bbdev_turbo_sw_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(DRIVER_NAME,
	TURBO_SW_MAX_NB_QUEUES_ARG"=<int> "
	TURBO_SW_SOCKET_ID_ARG"=<int>");
RTE_PMD_REGISTER_ALIAS(DRIVER_NAME, turbo_sw);

RTE_INIT(turbo_sw_bbdev_init_log)
{
	bbdev_turbo_sw_logtype = rte_log_register("pmd.bb.turbo_sw");
	if (bbdev_turbo_sw_logtype >= 0)
		rte_log_set_level(bbdev_turbo_sw_logtype, RTE_LOG_NOTICE);
}
