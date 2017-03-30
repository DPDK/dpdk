/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   Copyright 2016 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTE_PMD_MLX5_PRM_H_
#define RTE_PMD_MLX5_PRM_H_

#include <assert.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx5_hw.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_vect.h>
#include "mlx5_autoconf.h"

/* Get CQE owner bit. */
#define MLX5_CQE_OWNER(op_own) ((op_own) & MLX5_CQE_OWNER_MASK)

/* Get CQE format. */
#define MLX5_CQE_FORMAT(op_own) (((op_own) & MLX5E_CQE_FORMAT_MASK) >> 2)

/* Get CQE opcode. */
#define MLX5_CQE_OPCODE(op_own) (((op_own) & 0xf0) >> 4)

/* Get CQE solicited event. */
#define MLX5_CQE_SE(op_own) (((op_own) >> 1) & 1)

/* Invalidate a CQE. */
#define MLX5_CQE_INVALIDATE (MLX5_CQE_INVALID << 4)

/* Maximum number of packets a multi-packet WQE can handle. */
#define MLX5_MPW_DSEG_MAX 5

/* WQE DWORD size */
#define MLX5_WQE_DWORD_SIZE 16

/* WQE size */
#define MLX5_WQE_SIZE (4 * MLX5_WQE_DWORD_SIZE)

/* Max size of a WQE session. */
#define MLX5_WQE_SIZE_MAX 960U

/* Compute the number of DS. */
#define MLX5_WQE_DS(n) \
	(((n) + MLX5_WQE_DWORD_SIZE - 1) / MLX5_WQE_DWORD_SIZE)

/* Room for inline data in multi-packet WQE. */
#define MLX5_MWQE64_INL_DATA 28

/* Default minimum number of Tx queues for inlining packets. */
#define MLX5_EMPW_MIN_TXQS 8

/* Default max packet length to be inlined. */
#define MLX5_EMPW_MAX_INLINE_LEN (4U * MLX5_WQE_SIZE)

#ifndef HAVE_VERBS_MLX5_OPCODE_TSO
#define MLX5_OPCODE_TSO MLX5_OPCODE_LSO_MPW /* Compat with OFED 3.3. */
#endif

#define MLX5_OPC_MOD_ENHANCED_MPSW 0
#define MLX5_OPCODE_ENHANCED_MPSW 0x29

/* CQE value to inform that VLAN is stripped. */
#define MLX5_CQE_VLAN_STRIPPED (1u << 0)

/* IPv4 options. */
#define MLX5_CQE_RX_IP_EXT_OPTS_PACKET (1u << 1)

/* IPv6 packet. */
#define MLX5_CQE_RX_IPV6_PACKET (1u << 2)

/* IPv4 packet. */
#define MLX5_CQE_RX_IPV4_PACKET (1u << 3)

/* TCP packet. */
#define MLX5_CQE_RX_TCP_PACKET (1u << 4)

/* UDP packet. */
#define MLX5_CQE_RX_UDP_PACKET (1u << 5)

/* IP is fragmented. */
#define MLX5_CQE_RX_IP_FRAG_PACKET (1u << 7)

/* L2 header is valid. */
#define MLX5_CQE_RX_L2_HDR_VALID (1u << 8)

/* L3 header is valid. */
#define MLX5_CQE_RX_L3_HDR_VALID (1u << 9)

/* L4 header is valid. */
#define MLX5_CQE_RX_L4_HDR_VALID (1u << 10)

/* Outer packet, 0 IPv4, 1 IPv6. */
#define MLX5_CQE_RX_OUTER_PACKET (1u << 1)

/* Tunnel packet bit in the CQE. */
#define MLX5_CQE_RX_TUNNEL_PACKET (1u << 0)

/* Inner L3 checksum offload (Tunneled packets only). */
#define MLX5_ETH_WQE_L3_INNER_CSUM (1u << 4)

/* Inner L4 checksum offload (Tunneled packets only). */
#define MLX5_ETH_WQE_L4_INNER_CSUM (1u << 5)

/* Is flow mark valid. */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define MLX5_FLOW_MARK_IS_VALID(val) ((val) & 0xffffff00)
#else
#define MLX5_FLOW_MARK_IS_VALID(val) ((val) & 0xffffff)
#endif

/* INVALID is used by packets matching no flow rules. */
#define MLX5_FLOW_MARK_INVALID 0

/* Maximum allowed value to mark a packet. */
#define MLX5_FLOW_MARK_MAX 0xfffff0

/* Default mark value used when none is provided. */
#define MLX5_FLOW_MARK_DEFAULT 0xffffff

/* Subset of struct mlx5_wqe_eth_seg. */
struct mlx5_wqe_eth_seg_small {
	uint32_t rsvd0;
	uint8_t	cs_flags;
	uint8_t	rsvd1;
	uint16_t mss;
	uint32_t rsvd2;
	uint16_t inline_hdr_sz;
	uint8_t inline_hdr[2];
} __rte_aligned(MLX5_WQE_DWORD_SIZE);

struct mlx5_wqe_inl_small {
	uint32_t byte_cnt;
	uint8_t raw;
} __rte_aligned(MLX5_WQE_DWORD_SIZE);

struct mlx5_wqe_ctrl {
	uint32_t ctrl0;
	uint32_t ctrl1;
	uint32_t ctrl2;
	uint32_t ctrl3;
} __rte_aligned(MLX5_WQE_DWORD_SIZE);

/* Small common part of the WQE. */
struct mlx5_wqe {
	uint32_t ctrl[4];
	struct mlx5_wqe_eth_seg_small eseg;
};

/* Vectorize WQE header. */
struct mlx5_wqe_v {
	rte_v128u32_t ctrl;
	rte_v128u32_t eseg;
};

/* WQE. */
struct mlx5_wqe64 {
	struct mlx5_wqe hdr;
	uint8_t raw[32];
} __rte_aligned(MLX5_WQE_SIZE);

/* MPW mode. */
enum mlx5_mpw_mode {
	MLX5_MPW_DISABLED,
	MLX5_MPW,
	MLX5_MPW_ENHANCED, /* Enhanced Multi-Packet Send WQE, a.k.a MPWv2. */
};

/* MPW session status. */
enum mlx5_mpw_state {
	MLX5_MPW_STATE_OPENED,
	MLX5_MPW_INL_STATE_OPENED,
	MLX5_MPW_ENHANCED_STATE_OPENED,
	MLX5_MPW_STATE_CLOSED,
};

/* MPW session descriptor. */
struct mlx5_mpw {
	enum mlx5_mpw_state state;
	unsigned int pkts_n;
	unsigned int len;
	unsigned int total_len;
	volatile struct mlx5_wqe *wqe;
	union {
		volatile struct mlx5_wqe_data_seg *dseg[MLX5_MPW_DSEG_MAX];
		volatile uint8_t *raw;
	} data;
};

/* CQ element structure - should be equal to the cache line size */
struct mlx5_cqe {
#if (RTE_CACHE_LINE_SIZE == 128)
	uint8_t padding[64];
#endif
	uint8_t pkt_info;
	uint8_t rsvd0[11];
	uint32_t rx_hash_res;
	uint8_t rx_hash_type;
	uint8_t rsvd1[11];
	uint16_t hdr_type_etc;
	uint16_t vlan_info;
	uint8_t rsvd2[12];
	uint32_t byte_cnt;
	uint64_t timestamp;
	uint32_t sop_drop_qpn;
	uint16_t wqe_counter;
	uint8_t rsvd4;
	uint8_t op_own;
};

/**
 * Convert a user mark to flow mark.
 *
 * @param val
 *   Mark value to convert.
 *
 * @return
 *   Converted mark value.
 */
static inline uint32_t
mlx5_flow_mark_set(uint32_t val)
{
	uint32_t ret;

	/*
	 * Add one to the user value to differentiate un-marked flows from
	 * marked flows, if the ID is equal to MLX5_FLOW_MARK_DEFAULT it
	 * remains untouched.
	 */
	if (val != MLX5_FLOW_MARK_DEFAULT)
		++val;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/*
	 * Mark is 24 bits (minus reserved values) but is stored on a 32 bit
	 * word, byte-swapped by the kernel on little-endian systems. In this
	 * case, left-shifting the resulting big-endian value ensures the
	 * least significant 24 bits are retained when converting it back.
	 */
	ret = rte_cpu_to_be_32(val) >> 8;
#else
	ret = val;
#endif
	return ret;
}

/**
 * Convert a mark to user mark.
 *
 * @param val
 *   Mark value to convert.
 *
 * @return
 *   Converted mark value.
 */
static inline uint32_t
mlx5_flow_mark_get(uint32_t val)
{
	/*
	 * Subtract one from the retrieved value. It was added by
	 * mlx5_flow_mark_set() to distinguish unmarked flows.
	 */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	return (val >> 8) - 1;
#else
	return val - 1;
#endif
}

#endif /* RTE_PMD_MLX5_PRM_H_ */
