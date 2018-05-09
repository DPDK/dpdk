/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_BBDEV_OP_H_
#define _RTE_BBDEV_OP_H_

/**
 * @file rte_bbdev_op.h
 *
 * Defines wireless base band layer 1 operations and capabilities
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>

/* Number of columns in sub-block interleaver (36.212, section 5.1.4.1.1) */
#define RTE_BBDEV_C_SUBBLOCK (32)
/* Maximum size of Transport Block (36.213, Table, Table 7.1.7.2.5-1) */
#define RTE_BBDEV_MAX_TB_SIZE (391656)
/* Maximum size of Code Block (36.212, Table 5.1.3-3) */
#define RTE_BBDEV_MAX_CB_SIZE (6144)
/* Minimum size of Code Block (36.212, Table 5.1.3-3) */
#define RTE_BBDEV_MIN_CB_SIZE (40)
/* Maximum size of circular buffer */
#define RTE_BBDEV_MAX_KW (18528)
/*
 * Maximum number of Code Blocks in Transport Block. It is calculated based on
 * maximum size of one Code Block and one Transport Block (considering CRC24A
 * and CRC24B):
 * (391656 + 24) / (6144 - 24) = 64
 */
#define RTE_BBDEV_MAX_CODE_BLOCKS (64)

/** Flags for turbo decoder operation and capability structure */
enum rte_bbdev_op_td_flag_bitmasks {
	/**< If sub block de-interleaving is to be performed. */
	RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE = (1ULL << 0),
	/**< To use CRC Type 24B (otherwise use CRC Type 24A). */
	RTE_BBDEV_TURBO_CRC_TYPE_24B = (1ULL << 1),
	/**< If turbo equalization is to be performed. */
	RTE_BBDEV_TURBO_EQUALIZER = (1ULL << 2),
	/**< If set, saturate soft output to +/-127 */
	RTE_BBDEV_TURBO_SOFT_OUT_SATURATE = (1ULL << 3),
	/**< Set to 1 to start iteration from even, else odd; one iteration =
	 * max_iteration + 0.5
	 */
	RTE_BBDEV_TURBO_HALF_ITERATION_EVEN = (1ULL << 4),
	/**< If 0, TD stops after CRC matches; else if 1, runs to end of next
	 * odd iteration after CRC matches
	 */
	RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH = (1ULL << 5),
	/**< Set if soft output is required to be output  */
	RTE_BBDEV_TURBO_SOFT_OUTPUT = (1ULL << 6),
	/**< Set to enable early termination mode */
	RTE_BBDEV_TURBO_EARLY_TERMINATION = (1ULL << 7),
	/**< Set if a device supports decoder dequeue interrupts */
	RTE_BBDEV_TURBO_DEC_INTERRUPTS = (1ULL << 9),
	/**< Set if positive LLR encoded input is supported. Positive LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN
	 * when used to formalize the input data format.
	 */
	RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN = (1ULL << 10),
	/**< Set if negative LLR encoded input is supported. Negative LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN
	 * when used to formalize the input data format.
	 */
	RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN = (1ULL << 11),
	/**< Set if positive LLR soft output is supported. Positive LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with
	 * RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT when used to formalize
	 * the input data format.
	 */
	RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT = (1ULL << 12),
	/**< Set if negative LLR soft output is supported. Negative LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with
	 * RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT when used to formalize the
	 * input data format.
	 */
	RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT = (1ULL << 13),
	/**< Set if driver supports flexible parallel MAP engine decoding. If
	 * not supported, num_maps (number of MAP engines) argument is unusable.
	 */
	RTE_BBDEV_TURBO_MAP_DEC = (1ULL << 14),
	/**< Set if a device supports scatter-gather functionality */
	RTE_BBDEV_TURBO_DEC_SCATTER_GATHER = (1ULL << 15),
	/**< Set to keep CRC24B bits appended while decoding. Only usable when
	 * decoding Transport Blocks (code_block_mode = 0).
	 */
	RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP = (1ULL << 16)
};

/** Flags for turbo encoder operation and capability structure */
enum rte_bbdev_op_te_flag_bitmasks {
	/**< Ignore rv_index and set K0 = 0 */
	RTE_BBDEV_TURBO_RV_INDEX_BYPASS = (1ULL << 0),
	/**< If rate matching is to be performed */
	RTE_BBDEV_TURBO_RATE_MATCH = (1ULL << 1),
	/**< This bit must be set to enable CRC-24B generation */
	RTE_BBDEV_TURBO_CRC_24B_ATTACH = (1ULL << 2),
	/**< This bit must be set to enable CRC-24A generation */
	RTE_BBDEV_TURBO_CRC_24A_ATTACH = (1ULL << 3),
	/**< Set if a device supports encoder dequeue interrupts */
	RTE_BBDEV_TURBO_ENC_INTERRUPTS = (1ULL << 4),
	/**< Set if a device supports scatter-gather functionality */
	RTE_BBDEV_TURBO_ENC_SCATTER_GATHER = (1ULL << 5)
};

/**< Data input and output buffer for BBDEV operations */
struct rte_bbdev_op_data {
	/**< The mbuf data structure representing the data for BBDEV operation.
	 *
	 * This mbuf pointer can point to one Code Block (CB) data buffer or
	 * multiple CBs contiguously located next to each other.
	 * A Transport Block (TB) represents a whole piece of data that is
	 * divided into one or more CBs. Maximum number of CBs can be contained
	 * in one TB is defined by RTE_BBDEV_MAX_CODE_BLOCKS.
	 *
	 * An mbuf data structure cannot represent more than one TB. The
	 * smallest piece of data that can be contained in one mbuf is one CB.
	 * An mbuf can include one contiguous CB, subset of contiguous CBs that
	 * are belonging to one TB, or all contiguous CBs that are belonging to
	 * one TB.
	 *
	 * If a BBDEV PMD supports the extended capability "Scatter-Gather",
	 * then it is capable of collecting (gathering) non-contiguous
	 * (scattered) data from multiple locations in the memory.
	 * This capability is reported by the capability flags:
	 * - RTE_BBDEV_TURBO_ENC_SCATTER_GATHER and
	 * - RTE_BBDEV_TURBO_DEC_SCATTER_GATHER.
	 * Only if a BBDEV PMD supports this feature, chained mbuf data
	 * structures are accepted. A chained mbuf can represent one
	 * non-contiguous CB or multiple non-contiguous CBs.
	 * If BBDEV PMD does not support this feature, it will assume inbound
	 * mbuf data contains one segment.
	 *
	 * The output mbuf data though is always one segment, even if the input
	 * was a chained mbuf.
	 */
	struct rte_mbuf *data;
	/**< The starting point of the BBDEV (encode/decode) operation,
	 * in bytes.
	 *
	 * BBDEV starts to read data past this offset.
	 * In case of chained mbuf, this offset applies only to the first mbuf
	 * segment.
	 */
	uint32_t offset;
	/**< The total data length to be processed in one operation, in bytes.
	 *
	 * In case the mbuf data is representing one CB, this is the length of
	 * the CB undergoing the operation.
	 * If it's for multiple CBs, this is the total length of those CBs
	 * undergoing the operation.
	 * If it's for one TB, this is the total length of the TB under
	 * operation.
	 *
	 * In case of chained mbuf, this data length includes the lengths of the
	 * "scattered" data segments undergoing the operation.
	 */
	uint32_t length;
};

struct rte_bbdev_op_dec_cb_params {
	/**< The K size of the input CB, in bits [40:6144], as specified in
	 * 3GPP TS 36.212.
	 * This size is inclusive of CRC bits, regardless whether it was
	 * pre-calculated by the application or not.
	 */
	uint16_t k;
	/**< The E length of the CB rate matched LLR output, in bytes, as in
	 * 3GPP TS 36.212.
	 */
	uint32_t e;
};

struct rte_bbdev_op_dec_tb_params {
	/**< The K- size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r < C-, as in 3GPP TS 36.212.
	 */
	uint16_t k_neg;
	/**< The K+ size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r >= C-, as in 3GPP TS 36.212.
	 */
	uint16_t k_pos;
	/**< The number of CBs that have K- size, [0:63] */
	uint8_t c_neg;
	/**< The total number of CBs in the TB, [1:RTE_BBDEV_MAX_CODE_BLOCKS] */
	uint8_t c;
	/**< The number of CBs that uses Ea before switching to Eb, [0:63] */
	uint8_t cab;
	/**< The E size of the CB rate matched output to use in the Turbo
	 * operation when r < cab
	 */
	uint32_t ea;
	/**< The E size of the CB rate matched output to use in the Turbo
	 * operation when r >= cab
	 */
	uint32_t eb;
};

/**< Operation structure for Turbo decode.
 * An operation can perform on one CB at a time "CB-mode".
 * An operation can perform on one or multiple CBs that are logically belonging
 * to one TB "TB-mode".
 * The provided K size parameter of the CB is its size out coming from the
 * decode operation.
 * CRC24A/B check is requested by the application by setting the flag
 * RTE_BBDEV_TURBO_CRC_TYPE_24B for CRC24B check or CRC24A otherwise.
 * In TB-mode, BBDEV concatenates the decoded CBs one next to the other with
 * relevant CRC24B in between.
 *
 * The input encoded CB data is the Virtual Circular Buffer data stream, wk,
 * with the null padding included as described in 3GPP TS 36.212
 * section 5.1.4.1.2 and shown in 3GPP TS 36.212 section 5.1.4.1 Figure 5.1.4-1.
 * The size of the virtual circular buffer is 3*Kpi, where Kpi is the 32 byte
 * aligned value of K, as specified in 3GPP TS 36.212 section 5.1.4.1.1.
 *
 * Each byte in the input circular buffer is the LLR value of each bit of the
 * original CB.
 *
 * Hard output is a mandatory capability that all BBDEV PMDs support. This is
 * the decoded CBs of K sizes (CRC24A/B is the last 24-bit in each decoded CB).
 * Soft output is an optional capability for BBDEV PMDs. If supported, an LLR
 * rate matched output is computed in the soft_output buffer structure.
 *
 * The output mbuf data structure is expected to be allocated by the
 * application with enough room for the output data.
 */
struct rte_bbdev_op_turbo_dec {
	/**< The Virtual Circular Buffer, wk, size 3*Kpi for each CB */
	struct rte_bbdev_op_data input;
	/**< The hard decisions buffer for the decoded output,
	 * size K for each CB
	 */
	struct rte_bbdev_op_data hard_output;
	/**< The soft LLR output buffer - optional */
	struct rte_bbdev_op_data soft_output;

	uint32_t op_flags;  /**< Flags from rte_bbdev_op_td_flag_bitmasks */
	uint8_t rv_index;  /**< Rv index for rate matching [0:3] */
	/**< The minimum number of iterations to perform in decoding all CBs in
	 * this operation - input
	 */
	uint8_t iter_min:4;
	/**< The maximum number of iterations to perform in decoding all CBs in
	 * this operation - input
	 */
	uint8_t iter_max:4;
	/**< The maximum number of iterations that were perform in decoding all
	 * CBs in this decode operation - output
	 */
	uint8_t iter_count;
	/**< 5 bit extrinsic scale (scale factor on extrinsic info) */
	uint8_t ext_scale;
	/**< Number of MAP engines to use in decode,
	 * must be power of 2 (or 0 to auto-select)
	 */
	uint8_t num_maps;

	uint8_t code_block_mode; /**< [0 - TB : 1 - CB] */
	union {
		/**< Struct which stores Code Block specific parameters */
		struct rte_bbdev_op_dec_cb_params cb_params;
		/**< Struct which stores Transport Block specific parameters */
		struct rte_bbdev_op_dec_tb_params tb_params;
	};
};

struct rte_bbdev_op_enc_cb_params {
	/**< The K size of the input CB, in bits [40:6144], as specified in
	 * 3GPP TS 36.212.
	 * This size is inclusive of CRC24A, regardless whether it was
	 * pre-calculated by the application or not.
	 */
	uint16_t k;
	/**< The E length of the CB rate matched output, in bits, as in
	 * 3GPP TS 36.212.
	 */
	uint32_t e;
	/**< The Ncb soft buffer size of the CB rate matched output [K:3*Kpi],
	 * in bits, as specified in 3GPP TS 36.212.
	 */
	uint16_t ncb;
};

struct rte_bbdev_op_enc_tb_params {
	/**< The K- size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r < C-, as in 3GPP TS 36.212.
	 * This size is inclusive of CRC24B, regardless whether it was
	 * pre-calculated and appended by the application or not.
	 */
	uint16_t k_neg;
	/**< The K+ size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r >= C-, as in 3GPP TS 36.212.
	 * This size is inclusive of CRC24B, regardless whether it was
	 * pre-calculated and appended by the application or not.
	 */
	uint16_t k_pos;
	/**< The number of CBs that have K- size, [0:63] */
	uint8_t c_neg;
	/**< The total number of CBs in the TB, [1:RTE_BBDEV_MAX_CODE_BLOCKS] */
	uint8_t c;
	/**< The number of CBs that uses Ea before switching to Eb, [0:63] */
	uint8_t cab;
	/**< The E size of the CB rate matched output to use in the Turbo
	 * operation when r < cab
	 */
	uint32_t ea;
	/**< The E size of the CB rate matched output to use in the Turbo
	 * operation when r >= cab
	 */
	uint32_t eb;
	/**< The Ncb soft buffer size for the rate matched CB that is used in
	 * the Turbo operation when r < C-, [K:3*Kpi]
	 */
	uint16_t ncb_neg;
	/**< The Ncb soft buffer size for the rate matched CB that is used in
	 * the Turbo operation when r >= C-, [K:3*Kpi]
	 */
	uint16_t ncb_pos;
	/**< The index of the first CB in the inbound mbuf data, default is 0 */
	uint8_t r;
};

/**< Operation structure for Turbo encode.
 * An operation can perform on one CB at a time "CB-mode".
 * An operation can perform on one or multiple CBs that are logically
 * belonging to one TB "TB-mode".
 *
 * In CB-mode, CRC24A/B is an optional operation. K size parameter is not
 * affected by CRC24A/B inclusion, this only affects the inbound mbuf data
 * length. Not all BBDEV PMDs are capable of CRC24A/B calculation. Flags
 * RTE_BBDEV_TURBO_CRC_24A_ATTACH and RTE_BBDEV_TURBO_CRC_24B_ATTACH informs
 * the application with relevant capability. These flags can be set in the
 * op_flags parameter to indicate BBDEV to calculate and append CRC24A to CB
 * before going forward with Turbo encoding.
 *
 * In TB-mode, CRC24A is assumed to be pre-calculated and appended to the
 * inbound TB mbuf data buffer.
 *
 * The output mbuf data structure is expected to be allocated by the
 * application with enough room for the output data.
 */
struct rte_bbdev_op_turbo_enc {
	/**< The input CB or TB data */
	struct rte_bbdev_op_data input;
	/**< The rate matched CB or TB output buffer */
	struct rte_bbdev_op_data output;

	uint32_t op_flags;  /**< Flags from rte_bbdev_op_te_flag_bitmasks */
	uint8_t rv_index;  /**< Rv index for rate matching [0:3] */

	uint8_t code_block_mode; /**< [0 - TB : 1 - CB] */
	union {
		/**< Struct which stores Code Block specific parameters */
		struct rte_bbdev_op_enc_cb_params cb_params;
		/**< Struct which stores Transport Block specific parameters */
		struct rte_bbdev_op_enc_tb_params tb_params;
	};
};

/**< List of the capabilities for the Turbo Decoder */
struct rte_bbdev_op_cap_turbo_dec {
	/**< Flags from rte_bbdev_op_td_flag_bitmasks */
	uint32_t capability_flags;
	/** Maximal LLR absolute value. Acceptable LLR values lie in range
	 * [-max_llr_modulus, max_llr_modulus].
	 */
	int8_t max_llr_modulus;
	uint8_t num_buffers_src;  /**< Num input code block buffers */
	/**< Num hard output code block buffers */
	uint8_t num_buffers_hard_out;
	/**< Num soft output code block buffers if supported by the driver */
	uint8_t num_buffers_soft_out;
};

/**< List of the capabilities for the Turbo Encoder */
struct rte_bbdev_op_cap_turbo_enc {
	/**< Flags from rte_bbdev_op_te_flag_bitmasks */
	uint32_t capability_flags;
	uint8_t num_buffers_src;  /**< Num input code block buffers */
	uint8_t num_buffers_dst;  /**< Num output code block buffers */
};

/** Different operation types supported by the device */
enum rte_bbdev_op_type {
	RTE_BBDEV_OP_NONE,  /**< Dummy operation that does nothing */
	RTE_BBDEV_OP_TURBO_DEC,  /**< Turbo decode */
	RTE_BBDEV_OP_TURBO_ENC,  /**< Turbo encode */
	RTE_BBDEV_OP_TYPE_COUNT,  /**< Count of different op types */
};

/**< Bit indexes of possible errors reported through status field */
enum {
	RTE_BBDEV_DRV_ERROR,
	RTE_BBDEV_DATA_ERROR,
	RTE_BBDEV_CRC_ERROR,
};

/**< Structure specifying a single encode operation */
struct rte_bbdev_enc_op {
	int status;  /**< Status of operation that was performed */
	struct rte_mempool *mempool;  /**< Mempool which op instance is in */
	void *opaque_data;  /**< Opaque pointer for user data */
	/**< Contains encoder specific parameters */
	struct rte_bbdev_op_turbo_enc turbo_enc;
};

/**< Structure specifying a single decode operation */
struct rte_bbdev_dec_op {
	int status;  /**< Status of operation that was performed */
	struct rte_mempool *mempool;  /**< Mempool which op instance is in */
	void *opaque_data;  /**< Opaque pointer for user data */
	/**< Contains decoder specific parameters */
	struct rte_bbdev_op_turbo_dec turbo_dec;
};

/**< Operation capabilities supported by a device */
struct rte_bbdev_op_cap {
	enum rte_bbdev_op_type type;  /**< Type of operation */
	union {
		struct rte_bbdev_op_cap_turbo_dec turbo_dec;
		struct rte_bbdev_op_cap_turbo_enc turbo_enc;
	} cap;  /**< Operation-type specific capabilities */
};

/**< @internal Private data structure stored with operation pool. */
struct rte_bbdev_op_pool_private {
	enum rte_bbdev_op_type type;  /**< Type of operations in a pool */
};

/**
 * Converts queue operation type from enum to string
 *
 * @param op_type
 *   Operation type as enum
 *
 * @returns
 *   Operation type as string or NULL if op_type is invalid
 *
 */
const char*
rte_bbdev_op_type_str(enum rte_bbdev_op_type op_type);

/**
 * Creates a bbdev operation mempool
 *
 * @param name
 *   Pool name.
 * @param type
 *   Operation type, use RTE_BBDEV_OP_NONE for a pool which supports all
 *   operation types.
 * @param num_elements
 *   Number of elements in the pool.
 * @param cache_size
 *   Number of elements to cache on an lcore, see rte_mempool_create() for
 *   further details about cache size.
 * @param socket_id
 *   Socket to allocate memory on.
 *
 * @return
 *   - Pointer to a mempool on success,
 *   - NULL pointer on failure.
 */
struct rte_mempool *
rte_bbdev_op_pool_create(const char *name, enum rte_bbdev_op_type type,
		unsigned int num_elements, unsigned int cache_size,
		int socket_id);

/**
 * Bulk allocate encode operations from a mempool with parameter defaults reset.
 *
 * @param mempool
 *   Operation mempool, created by rte_bbdev_op_pool_create().
 * @param ops
 *   Output array to place allocated operations
 * @param num_ops
 *   Number of operations to allocate
 *
 * @returns
 *   - 0 on success
 *   - EINVAL if invalid mempool is provided
 */
static inline int
rte_bbdev_enc_op_alloc_bulk(struct rte_mempool *mempool,
		struct rte_bbdev_enc_op **ops, uint16_t num_ops)
{
	struct rte_bbdev_op_pool_private *priv;
	int ret;

	/* Check type */
	priv = (struct rte_bbdev_op_pool_private *)
			rte_mempool_get_priv(mempool);
	if (unlikely(priv->type != RTE_BBDEV_OP_TURBO_ENC))
		return -EINVAL;

	/* Get elements */
	ret = rte_mempool_get_bulk(mempool, (void **)ops, num_ops);
	if (unlikely(ret < 0))
		return ret;

	return 0;
}

/**
 * Bulk allocate decode operations from a mempool with parameter defaults reset.
 *
 * @param mempool
 *   Operation mempool, created by rte_bbdev_op_pool_create().
 * @param ops
 *   Output array to place allocated operations
 * @param num_ops
 *   Number of operations to allocate
 *
 * @returns
 *   - 0 on success
 *   - EINVAL if invalid mempool is provided
 */
static inline int
rte_bbdev_dec_op_alloc_bulk(struct rte_mempool *mempool,
		struct rte_bbdev_dec_op **ops, uint16_t num_ops)
{
	struct rte_bbdev_op_pool_private *priv;
	int ret;

	/* Check type */
	priv = (struct rte_bbdev_op_pool_private *)
			rte_mempool_get_priv(mempool);
	if (unlikely(priv->type != RTE_BBDEV_OP_TURBO_DEC))
		return -EINVAL;

	/* Get elements */
	ret = rte_mempool_get_bulk(mempool, (void **)ops, num_ops);
	if (unlikely(ret < 0))
		return ret;

	return 0;
}

/**
 * Free decode operation structures that were allocated by
 * rte_bbdev_dec_op_alloc_bulk().
 * All structures must belong to the same mempool.
 *
 * @param ops
 *   Operation structures
 * @param num_ops
 *   Number of structures
 */
static inline void
rte_bbdev_dec_op_free_bulk(struct rte_bbdev_dec_op **ops, unsigned int num_ops)
{
	if (num_ops > 0)
		rte_mempool_put_bulk(ops[0]->mempool, (void **)ops, num_ops);
}

/**
 * Free encode operation structures that were allocated by
 * rte_bbdev_enc_op_alloc_bulk().
 * All structures must belong to the same mempool.
 *
 * @param ops
 *   Operation structures
 * @param num_ops
 *   Number of structures
 */
static inline void
rte_bbdev_enc_op_free_bulk(struct rte_bbdev_enc_op **ops, unsigned int num_ops)
{
	if (num_ops > 0)
		rte_mempool_put_bulk(ops[0]->mempool, (void **)ops, num_ops);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BBDEV_OP_H_ */
