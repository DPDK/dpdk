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

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx5_hw.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

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

/* CQE value to inform that VLAN is stripped. */
#define MLX5_CQE_VLAN_STRIPPED 0x1

/* Maximum number of packets a multi-packet WQE can handle. */
#define MLX5_MPW_DSEG_MAX 5

/* WQE DWORD size */
#define MLX5_WQE_DWORD_SIZE 16

/* WQE size */
#define MLX5_WQE_SIZE (4 * MLX5_WQE_DWORD_SIZE)

/* Compute the number of DS. */
#define MLX5_WQE_DS(n) \
	(((n) + MLX5_WQE_DWORD_SIZE - 1) / MLX5_WQE_DWORD_SIZE)

/* Room for inline data in multi-packet WQE. */
#define MLX5_MWQE64_INL_DATA 28

#ifndef HAVE_VERBS_MLX5_OPCODE_TSO
#define MLX5_OPCODE_TSO MLX5_OPCODE_LSO_MPW /* Compat with OFED 3.3. */
#endif

/* IPv4 packet. */
#define MLX5_CQE_RX_IPV4_PACKET (1u << 2)

/* IPv6 packet. */
#define MLX5_CQE_RX_IPV6_PACKET (1u << 3)

/* Outer IPv4 packet. */
#define MLX5_CQE_RX_OUTER_IPV4_PACKET (1u << 7)

/* Outer IPv6 packet. */
#define MLX5_CQE_RX_OUTER_IPV6_PACKET (1u << 8)

/* Tunnel packet bit in the CQE. */
#define MLX5_CQE_RX_TUNNEL_PACKET (1u << 4)

/* Outer IP checksum OK. */
#define MLX5_CQE_RX_OUTER_IP_CSUM_OK (1u << 5)

/* Outer UDP header and checksum OK. */
#define MLX5_CQE_RX_OUTER_TCP_UDP_CSUM_OK (1u << 6)

/* Subset of struct mlx5_wqe_eth_seg. */
struct mlx5_wqe_eth_seg_small {
	uint32_t rsvd0;
	uint8_t	cs_flags;
	uint8_t	rsvd1;
	uint16_t mss;
	uint32_t rsvd2;
	uint16_t inline_hdr_sz;
	uint8_t inline_hdr[2];
};

struct mlx5_wqe_inl_small {
	uint32_t byte_cnt;
	uint8_t raw;
};

/* Small common part of the WQE. */
struct mlx5_wqe {
	uint32_t ctrl[4];
	struct mlx5_wqe_eth_seg_small eseg;
};

/* WQE. */
struct mlx5_wqe64 {
	struct mlx5_wqe hdr;
	uint8_t raw[32];
} __rte_aligned(64);

/* MPW session status. */
enum mlx5_mpw_state {
	MLX5_MPW_STATE_OPENED,
	MLX5_MPW_INL_STATE_OPENED,
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
	uint8_t hds_ip_ext;
	uint8_t l4_hdr_type_etc;
	uint16_t vlan_info;
	uint8_t rsvd2[12];
	uint32_t byte_cnt;
	uint64_t timestamp;
	uint8_t rsvd3[4];
	uint16_t wqe_counter;
	uint8_t rsvd4;
	uint8_t op_own;
};

#endif /* RTE_PMD_MLX5_PRM_H_ */
