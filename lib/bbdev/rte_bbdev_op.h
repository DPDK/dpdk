/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_BBDEV_OP_H_
#define _RTE_BBDEV_OP_H_

/**
 * @file rte_bbdev_op.h
 *
 * Defines wireless base band layer 1 operations and capabilities
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
#define RTE_BBDEV_TURBO_C_SUBBLOCK (32)
/* Maximum size of Transport Block (36.213, Table, Table 7.1.7.2.5-1) */
#define RTE_BBDEV_TURBO_MAX_TB_SIZE (391656)
/* Maximum size of Code Block (36.212, Table 5.1.3-3) */
#define RTE_BBDEV_TURBO_MAX_CB_SIZE (6144)
/* Maximum size of Code Block */
#define RTE_BBDEV_LDPC_MAX_CB_SIZE (8448)
/* Minimum size of Code Block */
#define RTE_BBDEV_LDPC_MIN_CB_SIZE (40)
/* Maximum E size we can manage with default mbuf */
#define RTE_BBDEV_LDPC_E_MAX_MBUF (64000)
/* Minimum size of Code Block (36.212, Table 5.1.3-3) */
#define RTE_BBDEV_TURBO_MIN_CB_SIZE (40)
/* Maximum size of circular buffer */
#define RTE_BBDEV_TURBO_MAX_KW (18528)
/*
 * Turbo: Maximum number of Code Blocks in Transport Block. It is calculated
 * based on maximum size of one Code Block and one Transport Block
 * (considering CRC24A and CRC24B):
 * (391656 + 24) / (6144 - 24) = 64
 */
#define RTE_BBDEV_TURBO_MAX_CODE_BLOCKS (64)
/* LDPC:  Maximum number of Code Blocks in Transport Block.*/
#define RTE_BBDEV_LDPC_MAX_CODE_BLOCKS (256)

/** Flags for turbo decoder operation and capability structure */
enum rte_bbdev_op_td_flag_bitmasks {
	/** If sub block de-interleaving is to be performed. */
	RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE = (1ULL << 0),
	/** To use CRC Type 24B (otherwise use CRC Type 24A). */
	RTE_BBDEV_TURBO_CRC_TYPE_24B = (1ULL << 1),
	/** If turbo equalization is to be performed. */
	RTE_BBDEV_TURBO_EQUALIZER = (1ULL << 2),
	/** If set, saturate soft output to +/-127 */
	RTE_BBDEV_TURBO_SOFT_OUT_SATURATE = (1ULL << 3),
	/** Set to 1 to start iteration from even, else odd; one iteration =
	 * max_iteration + 0.5
	 */
	RTE_BBDEV_TURBO_HALF_ITERATION_EVEN = (1ULL << 4),
	/** If 0, TD stops after CRC matches; else if 1, runs to end of next
	 * odd iteration after CRC matches
	 */
	RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH = (1ULL << 5),
	/** Set if soft output is required to be output  */
	RTE_BBDEV_TURBO_SOFT_OUTPUT = (1ULL << 6),
	/** Set to enable early termination mode */
	RTE_BBDEV_TURBO_EARLY_TERMINATION = (1ULL << 7),
	/** Set if a device supports decoder dequeue interrupts */
	RTE_BBDEV_TURBO_DEC_INTERRUPTS = (1ULL << 9),
	/** Set if positive LLR encoded input is supported. Positive LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN
	 * when used to formalize the input data format.
	 */
	RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN = (1ULL << 10),
	/** Set if negative LLR encoded input is supported. Negative LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN
	 * when used to formalize the input data format.
	 */
	RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN = (1ULL << 11),
	/** Set if positive LLR soft output is supported. Positive LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with
	 * RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT when used to formalize
	 * the input data format.
	 */
	RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT = (1ULL << 12),
	/** Set if negative LLR soft output is supported. Negative LLR value
	 * represents the level of confidence for bit '1', and vice versa for
	 * bit '0'.
	 * This is mutually exclusive with
	 * RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT when used to formalize the
	 * input data format.
	 */
	RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT = (1ULL << 13),
	/** Set if driver supports flexible parallel MAP engine decoding. If
	 * not supported, num_maps (number of MAP engines) argument is unusable.
	 */
	RTE_BBDEV_TURBO_MAP_DEC = (1ULL << 14),
	/** Set if a device supports scatter-gather functionality */
	RTE_BBDEV_TURBO_DEC_SCATTER_GATHER = (1ULL << 15),
	/** Set to keep CRC24B bits appended while decoding. Only usable when
	 * decoding Transport Block mode.
	 */
	RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP = (1ULL << 16),
	/** Set to drop CRC24B bits not to be appended while decoding.
	 */
	RTE_BBDEV_TURBO_DEC_CRC_24B_DROP = (1ULL << 17)
};


/** Flags for turbo encoder operation and capability structure */
enum rte_bbdev_op_te_flag_bitmasks {
	/** Ignore rv_index and set K0 = 0 */
	RTE_BBDEV_TURBO_RV_INDEX_BYPASS = (1ULL << 0),
	/** If rate matching is to be performed */
	RTE_BBDEV_TURBO_RATE_MATCH = (1ULL << 1),
	/** This bit must be set to enable CRC-24B generation */
	RTE_BBDEV_TURBO_CRC_24B_ATTACH = (1ULL << 2),
	/** This bit must be set to enable CRC-24A generation */
	RTE_BBDEV_TURBO_CRC_24A_ATTACH = (1ULL << 3),
	/** Set if a device supports encoder dequeue interrupts */
	RTE_BBDEV_TURBO_ENC_INTERRUPTS = (1ULL << 4),
	/** Set if a device supports scatter-gather functionality */
	RTE_BBDEV_TURBO_ENC_SCATTER_GATHER = (1ULL << 5)
};

/** Flags for LDPC decoder operation and capability structure */
enum rte_bbdev_op_ldpcdec_flag_bitmasks {
	/** Set for transport block CRC-24A checking */
	RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK = (1ULL << 0),
	/** Set for code block CRC-24B checking */
	RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK = (1ULL << 1),
	/** Set to drop the last CRC bits decoding output */
	RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP = (1ULL << 2),
	/** Set for transport block CRC-16 checking */
	RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK = (1ULL << 3),
	/** Set for bit-level de-interleaver bypass on Rx stream. */
	RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS = (1ULL << 4),
	/** Set for HARQ combined input stream enable. */
	RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE = (1ULL << 5),
	/** Set for HARQ combined output stream enable. */
	RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE = (1ULL << 6),
	/** Set for LDPC decoder bypass.
	 *  RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE must be set.
	 */
	RTE_BBDEV_LDPC_DECODE_BYPASS = (1ULL << 7),
	/** Set for soft-output stream enable */
	RTE_BBDEV_LDPC_SOFT_OUT_ENABLE = (1ULL << 8),
	/** Set for Rate-Matching bypass on soft-out stream. */
	RTE_BBDEV_LDPC_SOFT_OUT_RM_BYPASS = (1ULL << 9),
	/** Set for bit-level de-interleaver bypass on soft-output stream. */
	RTE_BBDEV_LDPC_SOFT_OUT_DEINTERLEAVER_BYPASS = (1ULL << 10),
	/** Set for iteration stopping on successful decode condition
	 *  i.e. a successful syndrome check.
	 */
	RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE = (1ULL << 11),
	/** Set if a device supports decoder dequeue interrupts. */
	RTE_BBDEV_LDPC_DEC_INTERRUPTS = (1ULL << 12),
	/** Set if a device supports scatter-gather functionality. */
	RTE_BBDEV_LDPC_DEC_SCATTER_GATHER = (1ULL << 13),
	/** Set if a device supports input/output HARQ compression. */
	RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION = (1ULL << 14),
	/** Set if a device supports input LLR compression. */
	RTE_BBDEV_LDPC_LLR_COMPRESSION = (1ULL << 15),
	/** Set if a device supports HARQ input from
	 *  device's internal memory.
	 */
	RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE = (1ULL << 16),
	/** Set if a device supports HARQ output to
	 *  device's internal memory.
	 */
	RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE = (1ULL << 17),
	/** Set if a device supports loop-back access to
	 *  HARQ internal memory. Intended for troubleshooting.
	 */
	RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK = (1ULL << 18),
	/** Set if a device includes LLR filler bits in the circular buffer
	 *  for HARQ memory. If not set, it is assumed the filler bits are not
	 *  in HARQ memory and handled directly by the LDPC decoder.
	 */
	RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS = (1ULL << 19)
};

/** Flags for LDPC encoder operation and capability structure */
enum rte_bbdev_op_ldpcenc_flag_bitmasks {
	/** Set for bit-level interleaver bypass on output stream. */
	RTE_BBDEV_LDPC_INTERLEAVER_BYPASS = (1ULL << 0),
	/** If rate matching is to be performed */
	RTE_BBDEV_LDPC_RATE_MATCH = (1ULL << 1),
	/** Set for transport block CRC-24A attach */
	RTE_BBDEV_LDPC_CRC_24A_ATTACH = (1ULL << 2),
	/** Set for code block CRC-24B attach */
	RTE_BBDEV_LDPC_CRC_24B_ATTACH = (1ULL << 3),
	/** Set for code block CRC-16 attach */
	RTE_BBDEV_LDPC_CRC_16_ATTACH = (1ULL << 4),
	/** Set if a device supports encoder dequeue interrupts. */
	RTE_BBDEV_LDPC_ENC_INTERRUPTS = (1ULL << 5),
	/** Set if a device supports scatter-gather functionality. */
	RTE_BBDEV_LDPC_ENC_SCATTER_GATHER = (1ULL << 6),
	/** Set if a device supports concatenation of non byte aligned output */
	RTE_BBDEV_LDPC_ENC_CONCATENATION = (1ULL << 7)
};

/** Flags for the Code Block/Transport block mode  */
enum rte_bbdev_op_cb_mode {
	/** One operation is one or fraction of one transport block  */
	RTE_BBDEV_TRANSPORT_BLOCK = 0,
	/** One operation is one code block mode */
	RTE_BBDEV_CODE_BLOCK = 1,
};

/** Data input and output buffer for BBDEV operations */
struct rte_bbdev_op_data {
	/** The mbuf data structure representing the data for BBDEV operation.
	 *
	 * This mbuf pointer can point to one Code Block (CB) data buffer or
	 * multiple CBs contiguously located next to each other.
	 * A Transport Block (TB) represents a whole piece of data that is
	 * divided into one or more CBs. Maximum number of CBs can be contained
	 * in one TB is defined by RTE_BBDEV_(TURBO/LDPC)_MAX_CODE_BLOCKS.
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
	 * - RTE_BBDEV_(TURBO/LDPC)_ENC_SCATTER_GATHER and
	 * - RTE_BBDEV_(TURBO/LDPC)_DEC_SCATTER_GATHER.
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
	/** The starting point of the BBDEV (encode/decode) operation,
	 * in bytes.
	 *
	 * BBDEV starts to read data past this offset.
	 * In case of chained mbuf, this offset applies only to the first mbuf
	 * segment.
	 */
	uint32_t offset;
	/** The total data length to be processed in one operation, in bytes.
	 *
	 * In case the mbuf data is representing one CB, this is the length of
	 * the CB undergoing the operation.
	 * If it's for multiple CBs, this is the total length of those CBs
	 * undergoing the operation.
	 * If it is for one TB, this is the total length of the TB under
	 * operation.
	 *
	 * In case of chained mbuf, this data length includes the lengths of the
	 * "scattered" data segments undergoing the operation.
	 */
	uint32_t length;
};

/** Turbo decode code block parameters */
struct rte_bbdev_op_dec_turbo_cb_params {
	/** The K size of the input CB, in bits [40:6144], as specified in
	 * 3GPP TS 36.212.
	 * This size is inclusive of CRC bits, regardless whether it was
	 * pre-calculated by the application or not.
	 */
	uint16_t k;
	/** The E length of the CB rate matched LLR output, in bytes, as in
	 * 3GPP TS 36.212.
	 */
	uint32_t e;
};

/** LDPC decode code block parameters */
struct rte_bbdev_op_dec_ldpc_cb_params {
	/** Rate matching output sequence length in bits or LLRs.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint32_t e;
};

/** Turbo decode transport block parameters */
struct rte_bbdev_op_dec_turbo_tb_params {
	/** The K- size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r < C-, as in 3GPP TS 36.212.
	 */
	uint16_t k_neg;
	/** The K+ size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r >= C-, as in 3GPP TS 36.212.
	 */
	uint16_t k_pos;
	/** The number of CBs that have K- size, [0:63] */
	uint8_t c_neg;
	/** The total number of CBs in the TB,
	 * [1:RTE_BBDEV_TURBO_MAX_CODE_BLOCKS]
	 */
	uint8_t c;
	/** The number of CBs that uses Ea before switching to Eb, [0:63] */
	uint8_t cab;
	/** The E size of the CB rate matched output to use in the Turbo
	 * operation when r < cab
	 */
	uint32_t ea;
	/** The E size of the CB rate matched output to use in the Turbo
	 * operation when r >= cab
	 */
	uint32_t eb;
	/** The index of the first CB in the inbound mbuf data, default is 0 */
	uint8_t r;
};

/** LDPC decode transport block parameters */
struct rte_bbdev_op_dec_ldpc_tb_params {
	/** Ea, length after rate matching in bits, r < cab.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint32_t ea;
	/** Eb, length after rate matching in bits, r >= cab.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint32_t eb;
	/** The total number of CBs in the TB or partial TB
	 * [1:RTE_BBDEV_LDPC_MAX_CODE_BLOCKS]
	 */
	uint8_t c;
	/** The index of the first CB in the inbound mbuf data, default is 0 */
	uint8_t r;
	/** The number of CBs that use Ea before switching to Eb, [0:63] */
	uint8_t cab;
};

/** Operation structure for Turbo decode.
 * An operation can be performed on one CB at a time "CB-mode".
 * An operation can be performed on one or multiple CBs that logically
 * belong to one TB "TB-mode".
 * The provided K size parameter of the CB is its size coming from the
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
	/** The Virtual Circular Buffer, wk, size 3*Kpi for each CB */
	struct rte_bbdev_op_data input;
	/** The hard decisions buffer for the decoded output,
	 * size K for each CB
	 */
	struct rte_bbdev_op_data hard_output;
	/** The soft LLR output buffer - optional */
	struct rte_bbdev_op_data soft_output;

	/** Flags from rte_bbdev_op_td_flag_bitmasks */
	uint32_t op_flags;

	/** Rv index for rate matching [0:3] */
	uint8_t rv_index;
	/** The minimum number of iterations to perform in decoding all CBs in
	 * this operation - input
	 */
	uint8_t iter_min:4;
	/** The maximum number of iterations to perform in decoding all CBs in
	 * this operation - input
	 */
	uint8_t iter_max:4;
	/** The maximum number of iterations that were performed in decoding
	 * all CBs in this decode operation - output
	 */
	uint8_t iter_count;
	/** 5 bit extrinsic scale (scale factor on extrinsic info) */
	uint8_t ext_scale;
	/** Number of MAP engines to use in decode,
	 *  must be power of 2 (or 0 to auto-select)
	 */
	uint8_t num_maps;

	/** [0 - TB : 1 - CB] */
	uint8_t code_block_mode;
	union {
		/** Struct which stores Code Block specific parameters */
		struct rte_bbdev_op_dec_turbo_cb_params cb_params;
		/** Struct which stores Transport Block specific parameters */
		struct rte_bbdev_op_dec_turbo_tb_params tb_params;
	};
};

/** Operation structure for LDPC decode.
 *
 * An operation can be performed on one CB at a time "CB-mode".
 * An operation can also be performed on one or multiple CBs that logically
 * belong to a TB "TB-mode" (Currently not supported).
 *
 * The input encoded CB data is the Virtual Circular Buffer data stream.
 *
 * Each byte in the input circular buffer is the LLR value of each bit of the
 * original CB.
 *
 * Hard output is a mandatory capability that all BBDEV PMDs support. This is
 * the decoded CBs (CRC24A/B is the last 24-bit in each decoded CB).
 *
 * Soft output is an optional capability for BBDEV PMDs. If supported, an LLR
 * rate matched output is computed in the soft_output buffer structure.
 * These are A Posteriori Probabilities (APP) LLR samples for coded bits.
 *
 * HARQ combined output is an optional capability for BBDEV PMDs.
 * If supported, a LLR output is streamed to the harq_combined_output
 * buffer.
 *
 * HARQ combined input is an optional capability for BBDEV PMDs.
 * If supported, a LLR input is streamed from the harq_combined_input
 * buffer.
 *
 * The output mbuf data structure is expected to be allocated by the
 * application with enough room for the output data.
 */
struct rte_bbdev_op_ldpc_dec {
	/** The Virtual Circular Buffer for this code block, one LLR
	 * per bit of the original CB.
	 */
	struct rte_bbdev_op_data input;
	/** The hard decisions buffer for the decoded output,
	 * size K for each CB
	 */
	struct rte_bbdev_op_data hard_output;
	/** The soft LLR output LLR stream buffer - optional */
	struct rte_bbdev_op_data soft_output;
	/** The HARQ combined LLR stream input buffer - optional */
	struct rte_bbdev_op_data harq_combined_input;
	/** The HARQ combined LLR stream output buffer - optional */
	struct rte_bbdev_op_data harq_combined_output;

	/** Flags from rte_bbdev_op_ldpcdec_flag_bitmasks */
	uint32_t op_flags;

	/** Rate matching redundancy version
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint8_t rv_index;
	/** The maximum number of iterations to perform in decoding CB in
	 *  this operation - input
	 */
	uint8_t iter_max;
	/** The number of iterations that were performed in decoding
	 * CB in this decode operation - output
	 */
	uint8_t iter_count;
	/** 1: LDPC Base graph 1, 2: LDPC Base graph 2.
	 * [3GPP TS38.212, section 5.2.2]
	 */
	uint8_t basegraph;
	/** Zc, LDPC lifting size.
	 *  [3GPP TS38.212, section 5.2.2]
	 */
	uint16_t z_c;
	/** Ncb, length of the circular buffer in bits.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint16_t n_cb;
	/** Qm, modulation order {1,2,4,6,8}.
	 *  [3GPP TS38.212, section 5.4.2.2]
	 */
	uint8_t q_m;
	/** Number of Filler bits, n_filler = K – K’
	 *  [3GPP TS38.212 section 5.2.2]
	 */
	uint16_t n_filler;
	/** [0 - TB : 1 - CB] */
	uint8_t code_block_mode;
	union {
		/** Struct which stores Code Block specific parameters */
		struct rte_bbdev_op_dec_ldpc_cb_params cb_params;
		/** Struct which stores Transport Block specific parameters */
		struct rte_bbdev_op_dec_ldpc_tb_params tb_params;
	};
};

/** Turbo encode code block parameters */
struct rte_bbdev_op_enc_turbo_cb_params {
	/** The K size of the input CB, in bits [40:6144], as specified in
	 * 3GPP TS 36.212.
	 * This size is inclusive of CRC24A, regardless whether it was
	 * pre-calculated by the application or not.
	 */
	uint16_t k;
	/** The E length of the CB rate matched output, in bits, as in
	 * 3GPP TS 36.212.
	 */
	uint32_t e;
	/** The Ncb soft buffer size of the CB rate matched output [K:3*Kpi],
	 * in bits, as specified in 3GPP TS 36.212.
	 */
	uint16_t ncb;
};

/** Turbo encode transport block parameters */
struct rte_bbdev_op_enc_turbo_tb_params {
	/** The K- size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r < C-, as in 3GPP TS 36.212.
	 * This size is inclusive of CRC24B, regardless whether it was
	 * pre-calculated and appended by the application or not.
	 */
	uint16_t k_neg;
	/** The K+ size of the input CB, in bits [40:6144], that is in the
	 * Turbo operation when r >= C-, as in 3GPP TS 36.212.
	 * This size is inclusive of CRC24B, regardless whether it was
	 * pre-calculated and appended by the application or not.
	 */
	uint16_t k_pos;
	/** The number of CBs that have K- size, [0:63] */
	uint8_t c_neg;
	/** The total number of CBs in the TB,
	 * [1:RTE_BBDEV_TURBO_MAX_CODE_BLOCKS]
	 */
	uint8_t c;
	/** The number of CBs that uses Ea before switching to Eb, [0:63] */
	uint8_t cab;
	/** The E size of the CB rate matched output to use in the Turbo
	 * operation when r < cab
	 */
	uint32_t ea;
	/** The E size of the CB rate matched output to use in the Turbo
	 * operation when r >= cab
	 */
	uint32_t eb;
	/** The Ncb soft buffer size for the rate matched CB that is used in
	 * the Turbo operation when r < C-, [K:3*Kpi]
	 */
	uint16_t ncb_neg;
	/** The Ncb soft buffer size for the rate matched CB that is used in
	 * the Turbo operation when r >= C-, [K:3*Kpi]
	 */
	uint16_t ncb_pos;
	/** The index of the first CB in the inbound mbuf data, default is 0 */
	uint8_t r;
};

/** LDPC encode code block parameters */
struct rte_bbdev_op_enc_ldpc_cb_params {
	/** E, length after rate matching in bits.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint32_t e;
};

/** LDPC encode transport block parameters */
struct rte_bbdev_op_enc_ldpc_tb_params {
	/** Ea, length after rate matching in bits, r < cab.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint32_t ea;
	/** Eb, length after rate matching in bits, r >= cab.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint32_t eb;
	/** The total number of CBs in the TB or partial TB
	 * [1:RTE_BBDEV_LDPC_MAX_CODE_BLOCKS]
	 */
	uint8_t c;
	/** The index of the first CB in the inbound mbuf data, default is 0 */
	uint8_t r;
	/** The number of CBs that use Ea before switching to Eb, [0:63] */
	uint8_t cab;
};

/** Operation structure for Turbo encode.
 * An operation can be performed on one CB at a time "CB-mode".
 * An operation can pbe erformd on one or multiple CBs that logically
 * belong to one TB "TB-mode".
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
	/** The input CB or TB data */
	struct rte_bbdev_op_data input;
	/** The rate matched CB or TB output buffer */
	struct rte_bbdev_op_data output;
	/** Flags from rte_bbdev_op_te_flag_bitmasks */
	uint32_t op_flags;

	/** Rv index for rate matching [0:3] */
	uint8_t rv_index;
	/** [0 - TB : 1 - CB] */
	uint8_t code_block_mode;
	union {
		/** Struct which stores Code Block specific parameters */
		struct rte_bbdev_op_enc_turbo_cb_params cb_params;
		/** Struct which stores Transport Block specific parameters */
		struct rte_bbdev_op_enc_turbo_tb_params tb_params;
	};
};

/** Operation structure for LDPC encode.
 * An operation can be performed on one CB at a time "CB-mode".
 * An operation can be performed on one or multiple CBs that logically
 * belong to a TB "TB-mode".
 *
 * The input data is the CB or TB input to the decoder.
 *
 * The output data is the ratematched CB or TB data, or the output after
 * bit-selection if RTE_BBDEV_LDPC_INTERLEAVER_BYPASS is set.
 *
 * The output mbuf data structure is expected to be allocated by the
 * application with enough room for the output data.
 */
struct rte_bbdev_op_ldpc_enc {
	/** The input TB or CB data */
	struct rte_bbdev_op_data input;
	/** The rate matched TB or CB output buffer */
	struct rte_bbdev_op_data output;

	/** Flags from rte_bbdev_op_ldpcenc_flag_bitmasks */
	uint32_t op_flags;

	/** Rate matching redundancy version */
	uint8_t rv_index;
	/** 1: LDPC Base graph 1, 2: LDPC Base graph 2.
	 *  [3GPP TS38.212, section 5.2.2]
	 */
	uint8_t basegraph;
	/** Zc, LDPC lifting size.
	 *  [3GPP TS38.212, section 5.2.2]
	 */
	uint16_t z_c;
	/** Ncb, length of the circular buffer in bits.
	 *  [3GPP TS38.212, section 5.4.2.1]
	 */
	uint16_t n_cb;
	/** Qm, modulation order {2,4,6,8,10}.
	 *  [3GPP TS38.212, section 5.4.2.2]
	 */
	uint8_t q_m;
	/** Number of Filler bits, n_filler = K – K’
	 *  [3GPP TS38.212 section 5.2.2]
	 */
	uint16_t n_filler;
	/** [0 - TB : 1 - CB] */
	uint8_t code_block_mode;
	union {
		/** Struct which stores Code Block specific parameters */
		struct rte_bbdev_op_enc_ldpc_cb_params cb_params;
		/** Struct which stores Transport Block specific parameters */
		struct rte_bbdev_op_enc_ldpc_tb_params tb_params;
	};
};

/** List of the capabilities for the Turbo Decoder */
struct rte_bbdev_op_cap_turbo_dec {
	/** Flags from rte_bbdev_op_td_flag_bitmasks */
	uint32_t capability_flags;
	/** Maximal LLR absolute value. Acceptable LLR values lie in range
	 * [-max_llr_modulus, max_llr_modulus].
	 */
	int8_t max_llr_modulus;
	/** Num input code block buffers */
	uint8_t num_buffers_src;  /**< Num input code block buffers */
	/** Num hard output code block buffers */
	uint8_t num_buffers_hard_out;
	/** Num soft output code block buffers if supported by the driver */
	uint8_t num_buffers_soft_out;
};

/** List of the capabilities for the Turbo Encoder */
struct rte_bbdev_op_cap_turbo_enc {
	/** Flags from rte_bbdev_op_te_flag_bitmasks */
	uint32_t capability_flags;
	/** Num input code block buffers */
	uint8_t num_buffers_src;
	/** Num output code block buffers */
	uint8_t num_buffers_dst;
};

/** List of the capabilities for the LDPC Decoder */
struct rte_bbdev_op_cap_ldpc_dec {
	/** Flags from rte_bbdev_op_ldpcdec_flag_bitmasks */
	uint32_t capability_flags;
	/** LLR size in bits. LLR is a two’s complement number. */
	int8_t llr_size;
	/** LLR numbers of decimals bit for arithmetic representation */
	int8_t llr_decimals;
	/** Num input code block buffers */
	uint16_t num_buffers_src;
	/** Num hard output code block buffers */
	uint16_t num_buffers_hard_out;
	/** Num soft output code block buffers if supported by the driver */
	uint16_t num_buffers_soft_out;
};

/** List of the capabilities for the LDPC Encoder */
struct rte_bbdev_op_cap_ldpc_enc {
	/** Flags from rte_bbdev_op_ldpcenc_flag_bitmasks */
	uint32_t capability_flags;
	/** Num input code block buffers */
	uint16_t num_buffers_src;
	/** Num output code block buffers */
	uint16_t num_buffers_dst;
};

/** Different operation types supported by the device */
enum rte_bbdev_op_type {
	RTE_BBDEV_OP_NONE,  /**< Dummy operation that does nothing */
	RTE_BBDEV_OP_TURBO_DEC,  /**< Turbo decode */
	RTE_BBDEV_OP_TURBO_ENC,  /**< Turbo encode */
	RTE_BBDEV_OP_LDPC_DEC,  /**< LDPC decode */
	RTE_BBDEV_OP_LDPC_ENC,  /**< LDPC encode */
	RTE_BBDEV_OP_TYPE_COUNT,  /**< Count of different op types */
};

/** Bit indexes of possible errors reported through status field */
enum {
	RTE_BBDEV_DRV_ERROR,
	RTE_BBDEV_DATA_ERROR,
	RTE_BBDEV_CRC_ERROR,
	RTE_BBDEV_SYNDROME_ERROR
};

/** Structure specifying a single encode operation */
struct rte_bbdev_enc_op {
	/** Status of operation that was performed */
	int status;
	/** Mempool which op instance is in */
	struct rte_mempool *mempool;
	/** Opaque pointer for user data */
	void *opaque_data;
	union {
		/** Contains turbo decoder specific parameters */
		struct rte_bbdev_op_turbo_enc turbo_enc;
		/** Contains LDPC decoder specific parameters */
		struct rte_bbdev_op_ldpc_enc ldpc_enc;
	};
};

/** Structure specifying a single decode operation */
struct rte_bbdev_dec_op {
	/** Status of operation that was performed */
	int status;
	/** Mempool which op instance is in */
	struct rte_mempool *mempool;
	/** Opaque pointer for user data */
	void *opaque_data;
	union {
		/** Contains turbo decoder specific parameters */
		struct rte_bbdev_op_turbo_dec turbo_dec;
		/** Contains LDPC decoder specific parameters */
		struct rte_bbdev_op_ldpc_dec ldpc_dec;
	};
};

/** Operation capabilities supported by a device */
struct rte_bbdev_op_cap {
	enum rte_bbdev_op_type type;  /**< Type of operation */
	union {
		struct rte_bbdev_op_cap_turbo_dec turbo_dec;
		struct rte_bbdev_op_cap_turbo_enc turbo_enc;
		struct rte_bbdev_op_cap_ldpc_dec ldpc_dec;
		struct rte_bbdev_op_cap_ldpc_enc ldpc_enc;
	} cap;  /**< Operation-type specific capabilities */
};

/** @internal Private data structure stored with operation pool. */
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
	if (unlikely((priv->type != RTE_BBDEV_OP_TURBO_ENC) &&
					(priv->type != RTE_BBDEV_OP_LDPC_ENC)))
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
	if (unlikely((priv->type != RTE_BBDEV_OP_TURBO_DEC) &&
					(priv->type != RTE_BBDEV_OP_LDPC_DEC)))
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
