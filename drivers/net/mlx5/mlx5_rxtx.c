/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015-2019 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_flow.h>

#include <mlx5_prm.h>
#include <mlx5_common.h>

#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_mr.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"

/* TX burst subroutines return codes. */
enum mlx5_txcmp_code {
	MLX5_TXCMP_CODE_EXIT = 0,
	MLX5_TXCMP_CODE_ERROR,
	MLX5_TXCMP_CODE_SINGLE,
	MLX5_TXCMP_CODE_MULTI,
	MLX5_TXCMP_CODE_TSO,
	MLX5_TXCMP_CODE_EMPW,
};

/*
 * These defines are used to configure Tx burst routine option set
 * supported at compile time. The not specified options are optimized out
 * out due to if conditions can be explicitly calculated at compile time.
 * The offloads with bigger runtime check (require more CPU cycles to
 * skip) overhead should have the bigger index - this is needed to
 * select the better matching routine function if no exact match and
 * some offloads are not actually requested.
 */
#define MLX5_TXOFF_CONFIG_MULTI (1u << 0) /* Multi-segment packets.*/
#define MLX5_TXOFF_CONFIG_TSO (1u << 1) /* TCP send offload supported.*/
#define MLX5_TXOFF_CONFIG_SWP (1u << 2) /* Tunnels/SW Parser offloads.*/
#define MLX5_TXOFF_CONFIG_CSUM (1u << 3) /* Check Sums offloaded. */
#define MLX5_TXOFF_CONFIG_INLINE (1u << 4) /* Data inlining supported. */
#define MLX5_TXOFF_CONFIG_VLAN (1u << 5) /* VLAN insertion supported.*/
#define MLX5_TXOFF_CONFIG_METADATA (1u << 6) /* Flow metadata. */
#define MLX5_TXOFF_CONFIG_EMPW (1u << 8) /* Enhanced MPW supported.*/
#define MLX5_TXOFF_CONFIG_MPW (1u << 9) /* Legacy MPW supported.*/
#define MLX5_TXOFF_CONFIG_TXPP (1u << 10) /* Scheduling on timestamp.*/

/* The most common offloads groups. */
#define MLX5_TXOFF_CONFIG_NONE 0
#define MLX5_TXOFF_CONFIG_FULL (MLX5_TXOFF_CONFIG_MULTI | \
				MLX5_TXOFF_CONFIG_TSO | \
				MLX5_TXOFF_CONFIG_SWP | \
				MLX5_TXOFF_CONFIG_CSUM | \
				MLX5_TXOFF_CONFIG_INLINE | \
				MLX5_TXOFF_CONFIG_VLAN | \
				MLX5_TXOFF_CONFIG_METADATA)

#define MLX5_TXOFF_CONFIG(mask) (olx & MLX5_TXOFF_CONFIG_##mask)

#define MLX5_TXOFF_DECL(func, olx) \
static uint16_t mlx5_tx_burst_##func(void *txq, \
				     struct rte_mbuf **pkts, \
				    uint16_t pkts_n) \
{ \
	return mlx5_tx_burst_tmpl((struct mlx5_txq_data *)txq, \
		    pkts, pkts_n, (olx)); \
}

#define MLX5_TXOFF_INFO(func, olx) {mlx5_tx_burst_##func, olx},

static __rte_always_inline uint32_t
rxq_cq_to_pkt_type(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
				   volatile struct mlx5_mini_cqe8 *mcqe);

static __rte_always_inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, volatile struct mlx5_mini_cqe8 **mcqe);

static __rte_always_inline uint32_t
rxq_cq_to_ol_flags(volatile struct mlx5_cqe *cqe);

static __rte_always_inline void
rxq_cq_to_mbuf(struct mlx5_rxq_data *rxq, struct rte_mbuf *pkt,
	       volatile struct mlx5_cqe *cqe,
	       volatile struct mlx5_mini_cqe8 *mcqe);

static int
mlx5_queue_state_modify(struct rte_eth_dev *dev,
			struct mlx5_mp_arg_queue_state_modify *sm);

static inline void
mlx5_lro_update_tcp_hdr(struct rte_tcp_hdr *__rte_restrict tcp,
			volatile struct mlx5_cqe *__rte_restrict cqe,
			uint32_t phcsum, uint8_t l4_type);

static inline void
mlx5_lro_update_hdr(uint8_t *__rte_restrict padd,
		    volatile struct mlx5_cqe *__rte_restrict cqe,
			volatile struct mlx5_mini_cqe8 *mcqe,
		    struct mlx5_rxq_data *rxq, uint32_t len);

uint32_t mlx5_ptype_table[] __rte_cache_aligned = {
	[0xff] = RTE_PTYPE_ALL_MASK, /* Last entry for errored packet. */
};

uint8_t mlx5_cksum_table[1 << 10] __rte_cache_aligned;
uint8_t mlx5_swp_types_table[1 << 10] __rte_cache_aligned;

uint64_t rte_net_mlx5_dynf_inline_mask;
#define PKT_TX_DYNF_NOINLINE rte_net_mlx5_dynf_inline_mask

/**
 * Build a table to translate Rx completion flags to packet type.
 *
 * @note: fix mlx5_dev_supported_ptypes_get() if any change here.
 */
void
mlx5_set_ptype_table(void)
{
	unsigned int i;
	uint32_t (*p)[RTE_DIM(mlx5_ptype_table)] = &mlx5_ptype_table;

	/* Last entry must not be overwritten, reserved for errored packet. */
	for (i = 0; i < RTE_DIM(mlx5_ptype_table) - 1; ++i)
		(*p)[i] = RTE_PTYPE_UNKNOWN;
	/*
	 * The index to the array should have:
	 * bit[1:0] = l3_hdr_type
	 * bit[4:2] = l4_hdr_type
	 * bit[5] = ip_frag
	 * bit[6] = tunneled
	 * bit[7] = outer_l3_type
	 */
	/* L2 */
	(*p)[0x00] = RTE_PTYPE_L2_ETHER;
	/* L3 */
	(*p)[0x01] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	(*p)[0x02] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	/* Fragmented */
	(*p)[0x21] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	(*p)[0x22] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	/* TCP */
	(*p)[0x05] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x06] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x0d] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x0e] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x11] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x12] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	/* UDP */
	(*p)[0x09] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	(*p)[0x0a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	/* Repeat with outer_l3_type being set. Just in case. */
	(*p)[0x81] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	(*p)[0x82] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_NONFRAG;
	(*p)[0xa1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	(*p)[0xa2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_FRAG;
	(*p)[0x85] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x86] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x8d] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x8e] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x91] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x92] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_TCP;
	(*p)[0x89] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	(*p)[0x8a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_L4_UDP;
	/* Tunneled - L3 */
	(*p)[0x40] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	(*p)[0x41] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	(*p)[0x42] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	(*p)[0xc0] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	(*p)[0xc1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	(*p)[0xc2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_NONFRAG;
	/* Tunneled - Fragmented */
	(*p)[0x61] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	(*p)[0x62] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	(*p)[0xe1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	(*p)[0xe2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_FRAG;
	/* Tunneled - TCP */
	(*p)[0x45] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0x46] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0x4d] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0x4e] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0x51] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0x52] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xc5] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xc6] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xcd] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xce] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xd1] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	(*p)[0xd2] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_TCP;
	/* Tunneled - UDP */
	(*p)[0x49] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	(*p)[0x4a] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	(*p)[0xc9] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
	(*p)[0xca] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		     RTE_PTYPE_INNER_L4_UDP;
}

/**
 * Build a table to translate packet to checksum type of Verbs.
 */
void
mlx5_set_cksum_table(void)
{
	unsigned int i;
	uint8_t v;

	/*
	 * The index should have:
	 * bit[0] = PKT_TX_TCP_SEG
	 * bit[2:3] = PKT_TX_UDP_CKSUM, PKT_TX_TCP_CKSUM
	 * bit[4] = PKT_TX_IP_CKSUM
	 * bit[8] = PKT_TX_OUTER_IP_CKSUM
	 * bit[9] = tunnel
	 */
	for (i = 0; i < RTE_DIM(mlx5_cksum_table); ++i) {
		v = 0;
		if (i & (1 << 9)) {
			/* Tunneled packet. */
			if (i & (1 << 8)) /* Outer IP. */
				v |= MLX5_ETH_WQE_L3_CSUM;
			if (i & (1 << 4)) /* Inner IP. */
				v |= MLX5_ETH_WQE_L3_INNER_CSUM;
			if (i & (3 << 2 | 1 << 0)) /* L4 or TSO. */
				v |= MLX5_ETH_WQE_L4_INNER_CSUM;
		} else {
			/* No tunnel. */
			if (i & (1 << 4)) /* IP. */
				v |= MLX5_ETH_WQE_L3_CSUM;
			if (i & (3 << 2 | 1 << 0)) /* L4 or TSO. */
				v |= MLX5_ETH_WQE_L4_CSUM;
		}
		mlx5_cksum_table[i] = v;
	}
}

/**
 * Build a table to translate packet type of mbuf to SWP type of Verbs.
 */
void
mlx5_set_swp_types_table(void)
{
	unsigned int i;
	uint8_t v;

	/*
	 * The index should have:
	 * bit[0:1] = PKT_TX_L4_MASK
	 * bit[4] = PKT_TX_IPV6
	 * bit[8] = PKT_TX_OUTER_IPV6
	 * bit[9] = PKT_TX_OUTER_UDP
	 */
	for (i = 0; i < RTE_DIM(mlx5_swp_types_table); ++i) {
		v = 0;
		if (i & (1 << 8))
			v |= MLX5_ETH_WQE_L3_OUTER_IPV6;
		if (i & (1 << 9))
			v |= MLX5_ETH_WQE_L4_OUTER_UDP;
		if (i & (1 << 4))
			v |= MLX5_ETH_WQE_L3_INNER_IPV6;
		if ((i & 3) == (PKT_TX_UDP_CKSUM >> 52))
			v |= MLX5_ETH_WQE_L4_INNER_UDP;
		mlx5_swp_types_table[i] = v;
	}
}

/**
 * Set Software Parser flags and offsets in Ethernet Segment of WQE.
 * Flags must be preliminary initialized to zero.
 *
 * @param loc
 *   Pointer to burst routine local context.
 * @param swp_flags
 *   Pointer to store Software Parser flags
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Software Parser offsets packed in dword.
 *   Software Parser flags are set by pointer.
 */
static __rte_always_inline uint32_t
txq_mbuf_to_swp(struct mlx5_txq_local *__rte_restrict loc,
		uint8_t *swp_flags,
		unsigned int olx)
{
	uint64_t ol, tunnel;
	unsigned int idx, off;
	uint32_t set;

	if (!MLX5_TXOFF_CONFIG(SWP))
		return 0;
	ol = loc->mbuf->ol_flags;
	tunnel = ol & PKT_TX_TUNNEL_MASK;
	/*
	 * Check whether Software Parser is required.
	 * Only customized tunnels may ask for.
	 */
	if (likely(tunnel != PKT_TX_TUNNEL_UDP && tunnel != PKT_TX_TUNNEL_IP))
		return 0;
	/*
	 * The index should have:
	 * bit[0:1] = PKT_TX_L4_MASK
	 * bit[4] = PKT_TX_IPV6
	 * bit[8] = PKT_TX_OUTER_IPV6
	 * bit[9] = PKT_TX_OUTER_UDP
	 */
	idx = (ol & (PKT_TX_L4_MASK | PKT_TX_IPV6 | PKT_TX_OUTER_IPV6)) >> 52;
	idx |= (tunnel == PKT_TX_TUNNEL_UDP) ? (1 << 9) : 0;
	*swp_flags = mlx5_swp_types_table[idx];
	/*
	 * Set offsets for SW parser. Since ConnectX-5, SW parser just
	 * complements HW parser. SW parser starts to engage only if HW parser
	 * can't reach a header. For the older devices, HW parser will not kick
	 * in if any of SWP offsets is set. Therefore, all of the L3 offsets
	 * should be set regardless of HW offload.
	 */
	off = loc->mbuf->outer_l2_len;
	if (MLX5_TXOFF_CONFIG(VLAN) && ol & PKT_TX_VLAN_PKT)
		off += sizeof(struct rte_vlan_hdr);
	set = (off >> 1) << 8; /* Outer L3 offset. */
	off += loc->mbuf->outer_l3_len;
	if (tunnel == PKT_TX_TUNNEL_UDP)
		set |= off >> 1; /* Outer L4 offset. */
	if (ol & (PKT_TX_IPV4 | PKT_TX_IPV6)) { /* Inner IP. */
		const uint64_t csum = ol & PKT_TX_L4_MASK;
			off += loc->mbuf->l2_len;
		set |= (off >> 1) << 24; /* Inner L3 offset. */
		if (csum == PKT_TX_TCP_CKSUM ||
		    csum == PKT_TX_UDP_CKSUM ||
		    (MLX5_TXOFF_CONFIG(TSO) && ol & PKT_TX_TCP_SEG)) {
			off += loc->mbuf->l3_len;
			set |= (off >> 1) << 16; /* Inner L4 offset. */
		}
	}
	set = rte_cpu_to_le_32(set);
	return set;
}

/**
 * Convert the Checksum offloads to Verbs.
 *
 * @param buf
 *   Pointer to the mbuf.
 *
 * @return
 *   Converted checksum flags.
 */
static __rte_always_inline uint8_t
txq_ol_cksum_to_cs(struct rte_mbuf *buf)
{
	uint32_t idx;
	uint8_t is_tunnel = !!(buf->ol_flags & PKT_TX_TUNNEL_MASK);
	const uint64_t ol_flags_mask = PKT_TX_TCP_SEG | PKT_TX_L4_MASK |
				       PKT_TX_IP_CKSUM | PKT_TX_OUTER_IP_CKSUM;

	/*
	 * The index should have:
	 * bit[0] = PKT_TX_TCP_SEG
	 * bit[2:3] = PKT_TX_UDP_CKSUM, PKT_TX_TCP_CKSUM
	 * bit[4] = PKT_TX_IP_CKSUM
	 * bit[8] = PKT_TX_OUTER_IP_CKSUM
	 * bit[9] = tunnel
	 */
	idx = ((buf->ol_flags & ol_flags_mask) >> 50) | (!!is_tunnel << 9);
	return mlx5_cksum_table[idx];
}

/**
 * Internal function to compute the number of used descriptors in an RX queue
 *
 * @param rxq
 *   The Rx queue.
 *
 * @return
 *   The number of used rx descriptor.
 */
static uint32_t
rx_queue_count(struct mlx5_rxq_data *rxq)
{
	struct rxq_zip *zip = &rxq->zip;
	volatile struct mlx5_cqe *cqe;
	const unsigned int cqe_n = (1 << rxq->cqe_n);
	const unsigned int sges_n = (1 << rxq->sges_n);
	const unsigned int elts_n = (1 << rxq->elts_n);
	const unsigned int strd_n = (1 << rxq->strd_num_n);
	const unsigned int cqe_cnt = cqe_n - 1;
	unsigned int cq_ci, used;

	/* if we are processing a compressed cqe */
	if (zip->ai) {
		used = zip->cqe_cnt - zip->ai;
		cq_ci = zip->cq_ci;
	} else {
		used = 0;
		cq_ci = rxq->cq_ci;
	}
	cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	while (check_cqe(cqe, cqe_n, cq_ci) != MLX5_CQE_STATUS_HW_OWN) {
		int8_t op_own;
		unsigned int n;

		op_own = cqe->op_own;
		if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED)
			n = rte_be_to_cpu_32(cqe->byte_cnt);
		else
			n = 1;
		cq_ci += n;
		used += n;
		cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	}
	used = RTE_MIN(used * sges_n, elts_n * strd_n);
	return used;
}

/**
 * DPDK callback to check the status of a rx descriptor.
 *
 * @param rx_queue
 *   The Rx queue.
 * @param[in] offset
 *   The index of the descriptor in the ring.
 *
 * @return
 *   The status of the tx descriptor.
 */
int
mlx5_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct mlx5_rxq_data *rxq = rx_queue;
	struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	struct rte_eth_dev *dev = ETH_DEV(rxq_ctrl->priv);

	if (dev->rx_pkt_burst == NULL ||
	    dev->rx_pkt_burst == removed_rx_burst) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (offset >= (1 << rxq->cqe_n)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (offset < rx_queue_count(rxq))
		return RTE_ETH_RX_DESC_DONE;
	return RTE_ETH_RX_DESC_AVAIL;
}

/**
 * DPDK callback to get the RX queue information
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param rx_queue_id
 *   Rx queue identificator.
 *
 * @param qinfo
 *   Pointer to the RX queue information structure.
 *
 * @return
 *   None.
 */

void
mlx5_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		  struct rte_eth_rxq_info *qinfo)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[rx_queue_id];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);

	if (!rxq)
		return;
	qinfo->mp = mlx5_rxq_mprq_enabled(rxq) ?
					rxq->mprq_mp : rxq->mp;
	qinfo->conf.rx_thresh.pthresh = 0;
	qinfo->conf.rx_thresh.hthresh = 0;
	qinfo->conf.rx_thresh.wthresh = 0;
	qinfo->conf.rx_free_thresh = rxq->rq_repl_thresh;
	qinfo->conf.rx_drop_en = 1;
	qinfo->conf.rx_deferred_start = rxq_ctrl ? 0 : 1;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = mlx5_rxq_mprq_enabled(rxq) ?
		(1 << rxq->elts_n) * (1 << rxq->strd_num_n) :
		(1 << rxq->elts_n);
}

/**
 * DPDK callback to get the RX packet burst mode information
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param rx_queue_id
 *   Rx queue identificatior.
 *
 * @param mode
 *   Pointer to the burts mode information.
 *
 * @return
 *   0 as success, -EINVAL as failure.
 */

int
mlx5_rx_burst_mode_get(struct rte_eth_dev *dev,
		       uint16_t rx_queue_id __rte_unused,
		       struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq;

	rxq = (*priv->rxqs)[rx_queue_id];
	if (!rxq) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (pkt_burst == mlx5_rx_burst) {
		snprintf(mode->info, sizeof(mode->info), "%s", "Scalar");
	} else if (pkt_burst == mlx5_rx_burst_mprq) {
		snprintf(mode->info, sizeof(mode->info), "%s", "Multi-Packet RQ");
	} else if (pkt_burst == mlx5_rx_burst_vec) {
#if defined RTE_ARCH_X86_64
		snprintf(mode->info, sizeof(mode->info), "%s", "Vector SSE");
#elif defined RTE_ARCH_ARM64
		snprintf(mode->info, sizeof(mode->info), "%s", "Vector Neon");
#elif defined RTE_ARCH_PPC_64
		snprintf(mode->info, sizeof(mode->info), "%s", "Vector AltiVec");
#else
		return -EINVAL;
#endif
	} else if (pkt_burst == mlx5_rx_burst_mprq_vec) {
#if defined RTE_ARCH_X86_64
		snprintf(mode->info, sizeof(mode->info), "%s", "MPRQ Vector SSE");
#elif defined RTE_ARCH_ARM64
		snprintf(mode->info, sizeof(mode->info), "%s", "MPRQ Vector Neon");
#elif defined RTE_ARCH_PPC_64
		snprintf(mode->info, sizeof(mode->info), "%s", "MPRQ Vector AltiVec");
#else
		return -EINVAL;
#endif
	} else {
		return -EINVAL;
	}
	return 0;
}

/**
 * DPDK callback to get the number of used descriptors in a RX queue
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param rx_queue_id
 *   The Rx queue.
 *
 * @return
 *   The number of used rx descriptor.
 *   -EINVAL if the queue is invalid
 */
uint32_t
mlx5_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq;

	if (dev->rx_pkt_burst == NULL ||
	    dev->rx_pkt_burst == removed_rx_burst) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	rxq = (*priv->rxqs)[rx_queue_id];
	if (!rxq) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return rx_queue_count(rxq);
}

#define MLX5_SYSTEM_LOG_DIR "/var/log"
/**
 * Dump debug information to log file.
 *
 * @param fname
 *   The file name.
 * @param hex_title
 *   If not NULL this string is printed as a header to the output
 *   and the output will be in hexadecimal view.
 * @param buf
 *   This is the buffer address to print out.
 * @param len
 *   The number of bytes to dump out.
 */
void
mlx5_dump_debug_information(const char *fname, const char *hex_title,
			    const void *buf, unsigned int hex_len)
{
	FILE *fd;

	MKSTR(path, "%s/%s", MLX5_SYSTEM_LOG_DIR, fname);
	fd = fopen(path, "a+");
	if (!fd) {
		DRV_LOG(WARNING, "cannot open %s for debug dump", path);
		MKSTR(path2, "./%s", fname);
		fd = fopen(path2, "a+");
		if (!fd) {
			DRV_LOG(ERR, "cannot open %s for debug dump", path2);
			return;
		}
		DRV_LOG(INFO, "New debug dump in file %s", path2);
	} else {
		DRV_LOG(INFO, "New debug dump in file %s", path);
	}
	if (hex_title)
		rte_hexdump(fd, hex_title, buf, hex_len);
	else
		fprintf(fd, "%s", (const char *)buf);
	fprintf(fd, "\n\n\n");
	fclose(fd);
}

/**
 * Move QP from error state to running state and initialize indexes.
 *
 * @param txq_ctrl
 *   Pointer to TX queue control structure.
 *
 * @return
 *   0 on success, else -1.
 */
static int
tx_recover_qp(struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5_mp_arg_queue_state_modify sm = {
			.is_wq = 0,
			.queue_id = txq_ctrl->txq.idx,
	};

	if (mlx5_queue_state_modify(ETH_DEV(txq_ctrl->priv), &sm))
		return -1;
	txq_ctrl->txq.wqe_ci = 0;
	txq_ctrl->txq.wqe_pi = 0;
	txq_ctrl->txq.elts_comp = 0;
	return 0;
}

/* Return 1 if the error CQE is signed otherwise, sign it and return 0. */
static int
check_err_cqe_seen(volatile struct mlx5_err_cqe *err_cqe)
{
	static const uint8_t magic[] = "seen";
	int ret = 1;
	unsigned int i;

	for (i = 0; i < sizeof(magic); ++i)
		if (!ret || err_cqe->rsvd1[i] != magic[i]) {
			ret = 0;
			err_cqe->rsvd1[i] = magic[i];
		}
	return ret;
}

/**
 * Handle error CQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param error_cqe
 *   Pointer to the error CQE.
 *
 * @return
 *   Negative value if queue recovery failed, otherwise
 *   the error completion entry is handled successfully.
 */
static int
mlx5_tx_error_cqe_handle(struct mlx5_txq_data *__rte_restrict txq,
			 volatile struct mlx5_err_cqe *err_cqe)
{
	if (err_cqe->syndrome != MLX5_CQE_SYNDROME_WR_FLUSH_ERR) {
		const uint16_t wqe_m = ((1 << txq->wqe_n) - 1);
		struct mlx5_txq_ctrl *txq_ctrl =
				container_of(txq, struct mlx5_txq_ctrl, txq);
		uint16_t new_wqe_pi = rte_be_to_cpu_16(err_cqe->wqe_counter);
		int seen = check_err_cqe_seen(err_cqe);

		if (!seen && txq_ctrl->dump_file_n <
		    txq_ctrl->priv->config.max_dump_files_num) {
			MKSTR(err_str, "Unexpected CQE error syndrome "
			      "0x%02x CQN = %u SQN = %u wqe_counter = %u "
			      "wq_ci = %u cq_ci = %u", err_cqe->syndrome,
			      txq->cqe_s, txq->qp_num_8s >> 8,
			      rte_be_to_cpu_16(err_cqe->wqe_counter),
			      txq->wqe_ci, txq->cq_ci);
			MKSTR(name, "dpdk_mlx5_port_%u_txq_%u_index_%u_%u",
			      PORT_ID(txq_ctrl->priv), txq->idx,
			      txq_ctrl->dump_file_n, (uint32_t)rte_rdtsc());
			mlx5_dump_debug_information(name, NULL, err_str, 0);
			mlx5_dump_debug_information(name, "MLX5 Error CQ:",
						    (const void *)((uintptr_t)
						    txq->cqes),
						    sizeof(*err_cqe) *
						    (1 << txq->cqe_n));
			mlx5_dump_debug_information(name, "MLX5 Error SQ:",
						    (const void *)((uintptr_t)
						    txq->wqes),
						    MLX5_WQE_SIZE *
						    (1 << txq->wqe_n));
			txq_ctrl->dump_file_n++;
		}
		if (!seen)
			/*
			 * Count errors in WQEs units.
			 * Later it can be improved to count error packets,
			 * for example, by SQ parsing to find how much packets
			 * should be counted for each WQE.
			 */
			txq->stats.oerrors += ((txq->wqe_ci & wqe_m) -
						new_wqe_pi) & wqe_m;
		if (tx_recover_qp(txq_ctrl)) {
			/* Recovering failed - retry later on the same WQE. */
			return -1;
		}
		/* Release all the remaining buffers. */
		txq_free_elts(txq_ctrl);
	}
	return 0;
}

/**
 * Translate RX completion flags to packet type.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @note: fix mlx5_dev_supported_ptypes_get() if any change here.
 *
 * @return
 *   Packet type for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_pkt_type(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
				   volatile struct mlx5_mini_cqe8 *mcqe)
{
	uint8_t idx;
	uint8_t ptype;
	uint8_t pinfo = (cqe->pkt_info & 0x3) << 6;

	/* Get l3/l4 header from mini-CQE in case L3/L4 format*/
	if (mcqe == NULL ||
	    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_L34H_STRIDX)
		ptype = (cqe->hdr_type_etc & 0xfc00) >> 10;
	else
		ptype = mcqe->hdr_type >> 2;
	/*
	 * The index to the array should have:
	 * bit[1:0] = l3_hdr_type
	 * bit[4:2] = l4_hdr_type
	 * bit[5] = ip_frag
	 * bit[6] = tunneled
	 * bit[7] = outer_l3_type
	 */
	idx = pinfo | ptype;
	return mlx5_ptype_table[idx] | rxq->tunnel * !!(idx & (1 << 6));
}

/**
 * Initialize Rx WQ and indexes.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 */
void
mlx5_rxq_initialize(struct mlx5_rxq_data *rxq)
{
	const unsigned int wqe_n = 1 << rxq->elts_n;
	unsigned int i;

	for (i = 0; (i != wqe_n); ++i) {
		volatile struct mlx5_wqe_data_seg *scat;
		uintptr_t addr;
		uint32_t byte_count;

		if (mlx5_rxq_mprq_enabled(rxq)) {
			struct mlx5_mprq_buf *buf = (*rxq->mprq_bufs)[i];

			scat = &((volatile struct mlx5_wqe_mprq *)
				rxq->wqes)[i].dseg;
			addr = (uintptr_t)mlx5_mprq_buf_addr(buf,
							 1 << rxq->strd_num_n);
			byte_count = (1 << rxq->strd_sz_n) *
					(1 << rxq->strd_num_n);
		} else {
			struct rte_mbuf *buf = (*rxq->elts)[i];

			scat = &((volatile struct mlx5_wqe_data_seg *)
					rxq->wqes)[i];
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			byte_count = DATA_LEN(buf);
		}
		/* scat->addr must be able to store a pointer. */
		MLX5_ASSERT(sizeof(scat->addr) >= sizeof(uintptr_t));
		*scat = (struct mlx5_wqe_data_seg){
			.addr = rte_cpu_to_be_64(addr),
			.byte_count = rte_cpu_to_be_32(byte_count),
			.lkey = mlx5_rx_addr2mr(rxq, addr),
		};
	}
	rxq->consumed_strd = 0;
	rxq->decompressed = 0;
	rxq->rq_pi = 0;
	rxq->zip = (struct rxq_zip){
		.ai = 0,
	};
	rxq->elts_ci = mlx5_rxq_mprq_enabled(rxq) ?
		(wqe_n >> rxq->sges_n) * (1 << rxq->strd_num_n) : 0;
	/* Update doorbell counter. */
	rxq->rq_ci = wqe_n >> rxq->sges_n;
	rte_io_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
}

/**
 * Modify a Verbs/DevX queue state.
 * This must be called from the primary process.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param sm
 *   State modify request parameters.
 *
 * @return
 *   0 in case of success else non-zero value and rte_errno is set.
 */
int
mlx5_queue_state_modify_primary(struct rte_eth_dev *dev,
			const struct mlx5_mp_arg_queue_state_modify *sm)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (sm->is_wq) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[sm->queue_id];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);

		ret = priv->obj_ops.rxq_obj_modify(rxq_ctrl->obj, sm->state);
		if (ret) {
			DRV_LOG(ERR, "Cannot change Rx WQ state to %u  - %s",
					sm->state, strerror(errno));
			rte_errno = errno;
			return ret;
		}
	} else {
		struct mlx5_txq_data *txq = (*priv->txqs)[sm->queue_id];
		struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq, struct mlx5_txq_ctrl, txq);

		ret = priv->obj_ops.txq_obj_modify(txq_ctrl->obj,
						   MLX5_TXQ_MOD_ERR2RDY,
						   (uint8_t)priv->dev_port);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 * Modify a Verbs queue state.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param sm
 *   State modify request parameters.
 *
 * @return
 *   0 in case of success else non-zero value.
 */
static int
mlx5_queue_state_modify(struct rte_eth_dev *dev,
			struct mlx5_mp_arg_queue_state_modify *sm)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;

	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		ret = mlx5_queue_state_modify_primary(dev, sm);
		break;
	case RTE_PROC_SECONDARY:
		ret = mlx5_mp_req_queue_state_modify(&priv->mp_id, sm);
		break;
	default:
		break;
	}
	return ret;
}

/**
 * Handle a Rx error.
 * The function inserts the RQ state to reset when the first error CQE is
 * shown, then drains the CQ by the caller function loop. When the CQ is empty,
 * it moves the RQ state to ready and initializes the RQ.
 * Next CQE identification and error counting are in the caller responsibility.
 *
 * @param[in] rxq
 *   Pointer to RX queue structure.
 * @param[in] vec
 *   1 when called from vectorized Rx burst, need to prepare mbufs for the RQ.
 *   0 when called from non-vectorized Rx burst.
 *
 * @return
 *   -1 in case of recovery error, otherwise the CQE status.
 */
int
mlx5_rx_err_handle(struct mlx5_rxq_data *rxq, uint8_t vec)
{
	const uint16_t cqe_n = 1 << rxq->cqe_n;
	const uint16_t cqe_mask = cqe_n - 1;
	const uint16_t wqe_n = 1 << rxq->elts_n;
	const uint16_t strd_n = 1 << rxq->strd_num_n;
	struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	union {
		volatile struct mlx5_cqe *cqe;
		volatile struct mlx5_err_cqe *err_cqe;
	} u = {
		.cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_mask],
	};
	struct mlx5_mp_arg_queue_state_modify sm;
	int ret;

	switch (rxq->err_state) {
	case MLX5_RXQ_ERR_STATE_NO_ERROR:
		rxq->err_state = MLX5_RXQ_ERR_STATE_NEED_RESET;
		/* Fall-through */
	case MLX5_RXQ_ERR_STATE_NEED_RESET:
		sm.is_wq = 1;
		sm.queue_id = rxq->idx;
		sm.state = IBV_WQS_RESET;
		if (mlx5_queue_state_modify(ETH_DEV(rxq_ctrl->priv), &sm))
			return -1;
		if (rxq_ctrl->dump_file_n <
		    rxq_ctrl->priv->config.max_dump_files_num) {
			MKSTR(err_str, "Unexpected CQE error syndrome "
			      "0x%02x CQN = %u RQN = %u wqe_counter = %u"
			      " rq_ci = %u cq_ci = %u", u.err_cqe->syndrome,
			      rxq->cqn, rxq_ctrl->wqn,
			      rte_be_to_cpu_16(u.err_cqe->wqe_counter),
			      rxq->rq_ci << rxq->sges_n, rxq->cq_ci);
			MKSTR(name, "dpdk_mlx5_port_%u_rxq_%u_%u",
			      rxq->port_id, rxq->idx, (uint32_t)rte_rdtsc());
			mlx5_dump_debug_information(name, NULL, err_str, 0);
			mlx5_dump_debug_information(name, "MLX5 Error CQ:",
						    (const void *)((uintptr_t)
								    rxq->cqes),
						    sizeof(*u.cqe) * cqe_n);
			mlx5_dump_debug_information(name, "MLX5 Error RQ:",
						    (const void *)((uintptr_t)
								    rxq->wqes),
						    16 * wqe_n);
			rxq_ctrl->dump_file_n++;
		}
		rxq->err_state = MLX5_RXQ_ERR_STATE_NEED_READY;
		/* Fall-through */
	case MLX5_RXQ_ERR_STATE_NEED_READY:
		ret = check_cqe(u.cqe, cqe_n, rxq->cq_ci);
		if (ret == MLX5_CQE_STATUS_HW_OWN) {
			rte_io_wmb();
			*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
			rte_io_wmb();
			/*
			 * The RQ consumer index must be zeroed while moving
			 * from RESET state to RDY state.
			 */
			*rxq->rq_db = rte_cpu_to_be_32(0);
			rte_io_wmb();
			sm.is_wq = 1;
			sm.queue_id = rxq->idx;
			sm.state = IBV_WQS_RDY;
			if (mlx5_queue_state_modify(ETH_DEV(rxq_ctrl->priv),
						    &sm))
				return -1;
			if (vec) {
				const uint32_t elts_n =
					mlx5_rxq_mprq_enabled(rxq) ?
					wqe_n * strd_n : wqe_n;
				const uint32_t e_mask = elts_n - 1;
				uint32_t elts_ci =
					mlx5_rxq_mprq_enabled(rxq) ?
					rxq->elts_ci : rxq->rq_ci;
				uint32_t elt_idx;
				struct rte_mbuf **elt;
				int i;
				unsigned int n = elts_n - (elts_ci -
							  rxq->rq_pi);

				for (i = 0; i < (int)n; ++i) {
					elt_idx = (elts_ci + i) & e_mask;
					elt = &(*rxq->elts)[elt_idx];
					*elt = rte_mbuf_raw_alloc(rxq->mp);
					if (!*elt) {
						for (i--; i >= 0; --i) {
							elt_idx = (elts_ci +
								   i) & elts_n;
							elt = &(*rxq->elts)
								[elt_idx];
							rte_pktmbuf_free_seg
								(*elt);
						}
						return -1;
					}
				}
				for (i = 0; i < (int)elts_n; ++i) {
					elt = &(*rxq->elts)[i];
					DATA_LEN(*elt) =
						(uint16_t)((*elt)->buf_len -
						rte_pktmbuf_headroom(*elt));
				}
				/* Padding with a fake mbuf for vec Rx. */
				for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
					(*rxq->elts)[elts_n + i] =
								&rxq->fake_mbuf;
			}
			mlx5_rxq_initialize(rxq);
			rxq->err_state = MLX5_RXQ_ERR_STATE_NO_ERROR;
		}
		return ret;
	default:
		return -1;
	}
}

/**
 * Get size of the next packet for a given CQE. For compressed CQEs, the
 * consumer index is updated only once all packets of the current one have
 * been processed.
 *
 * @param rxq
 *   Pointer to RX queue.
 * @param cqe
 *   CQE to process.
 * @param[out] mcqe
 *   Store pointer to mini-CQE if compressed. Otherwise, the pointer is not
 *   written.
 *
 * @return
 *   0 in case of empty CQE, otherwise the packet size in bytes.
 */
static inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, volatile struct mlx5_mini_cqe8 **mcqe)
{
	struct rxq_zip *zip = &rxq->zip;
	uint16_t cqe_n = cqe_cnt + 1;
	int len;
	uint16_t idx, end;

	do {
		len = 0;
		/* Process compressed data in the CQE and mini arrays. */
		if (zip->ai) {
			volatile struct mlx5_mini_cqe8 (*mc)[8] =
				(volatile struct mlx5_mini_cqe8 (*)[8])
				(uintptr_t)(&(*rxq->cqes)[zip->ca &
							  cqe_cnt].pkt_info);
			len = rte_be_to_cpu_32((*mc)[zip->ai & 7].byte_cnt &
					       rxq->byte_mask);
			*mcqe = &(*mc)[zip->ai & 7];
			if ((++zip->ai & 7) == 0) {
				/* Invalidate consumed CQEs */
				idx = zip->ca;
				end = zip->na;
				while (idx != end) {
					(*rxq->cqes)[idx & cqe_cnt].op_own =
						MLX5_CQE_INVALIDATE;
					++idx;
				}
				/*
				 * Increment consumer index to skip the number
				 * of CQEs consumed. Hardware leaves holes in
				 * the CQ ring for software use.
				 */
				zip->ca = zip->na;
				zip->na += 8;
			}
			if (unlikely(rxq->zip.ai == rxq->zip.cqe_cnt)) {
				/* Invalidate the rest */
				idx = zip->ca;
				end = zip->cq_ci;

				while (idx != end) {
					(*rxq->cqes)[idx & cqe_cnt].op_own =
						MLX5_CQE_INVALIDATE;
					++idx;
				}
				rxq->cq_ci = zip->cq_ci;
				zip->ai = 0;
			}
		/*
		 * No compressed data, get next CQE and verify if it is
		 * compressed.
		 */
		} else {
			int ret;
			int8_t op_own;
			uint32_t cq_ci;

			ret = check_cqe(cqe, cqe_n, rxq->cq_ci);
			if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
				if (unlikely(ret == MLX5_CQE_STATUS_ERR ||
					     rxq->err_state)) {
					ret = mlx5_rx_err_handle(rxq, 0);
					if (ret == MLX5_CQE_STATUS_HW_OWN ||
					    ret == -1)
						return 0;
				} else {
					return 0;
				}
			}
			/*
			 * Introduce the local variable to have queue cq_ci
			 * index in queue structure always consistent with
			 * actual CQE boundary (not pointing to the middle
			 * of compressed CQE session).
			 */
			cq_ci = rxq->cq_ci + 1;
			op_own = cqe->op_own;
			if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED) {
				volatile struct mlx5_mini_cqe8 (*mc)[8] =
					(volatile struct mlx5_mini_cqe8 (*)[8])
					(uintptr_t)(&(*rxq->cqes)
						[cq_ci & cqe_cnt].pkt_info);

				/* Fix endianness. */
				zip->cqe_cnt = rte_be_to_cpu_32(cqe->byte_cnt);
				/*
				 * Current mini array position is the one
				 * returned by check_cqe64().
				 *
				 * If completion comprises several mini arrays,
				 * as a special case the second one is located
				 * 7 CQEs after the initial CQE instead of 8
				 * for subsequent ones.
				 */
				zip->ca = cq_ci;
				zip->na = zip->ca + 7;
				/* Compute the next non compressed CQE. */
				zip->cq_ci = rxq->cq_ci + zip->cqe_cnt;
				/* Get packet size to return. */
				len = rte_be_to_cpu_32((*mc)[0].byte_cnt &
						       rxq->byte_mask);
				*mcqe = &(*mc)[0];
				zip->ai = 1;
				/* Prefetch all to be invalidated */
				idx = zip->ca;
				end = zip->cq_ci;
				while (idx != end) {
					rte_prefetch0(&(*rxq->cqes)[(idx) &
								    cqe_cnt]);
					++idx;
				}
			} else {
				rxq->cq_ci = cq_ci;
				len = rte_be_to_cpu_32(cqe->byte_cnt);
			}
		}
		if (unlikely(rxq->err_state)) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			++rxq->stats.idropped;
		} else {
			return len;
		}
	} while (1);
}

/**
 * Translate RX completion flags to offload flags.
 *
 * @param[in] cqe
 *   Pointer to CQE.
 *
 * @return
 *   Offload flags (ol_flags) for struct rte_mbuf.
 */
static inline uint32_t
rxq_cq_to_ol_flags(volatile struct mlx5_cqe *cqe)
{
	uint32_t ol_flags = 0;
	uint16_t flags = rte_be_to_cpu_16(cqe->hdr_type_etc);

	ol_flags =
		TRANSPOSE(flags,
			  MLX5_CQE_RX_L3_HDR_VALID,
			  PKT_RX_IP_CKSUM_GOOD) |
		TRANSPOSE(flags,
			  MLX5_CQE_RX_L4_HDR_VALID,
			  PKT_RX_L4_CKSUM_GOOD);
	return ol_flags;
}

/**
 * Fill in mbuf fields from RX completion flags.
 * Note that pkt->ol_flags should be initialized outside of this function.
 *
 * @param rxq
 *   Pointer to RX queue.
 * @param pkt
 *   mbuf to fill.
 * @param cqe
 *   CQE to process.
 * @param rss_hash_res
 *   Packet RSS Hash result.
 */
static inline void
rxq_cq_to_mbuf(struct mlx5_rxq_data *rxq, struct rte_mbuf *pkt,
	       volatile struct mlx5_cqe *cqe,
	       volatile struct mlx5_mini_cqe8 *mcqe)
{
	/* Update packet information. */
	pkt->packet_type = rxq_cq_to_pkt_type(rxq, cqe, mcqe);

	if (rxq->rss_hash) {
		uint32_t rss_hash_res = 0;

		/* If compressed, take hash result from mini-CQE. */
		if (mcqe == NULL ||
		    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_HASH)
			rss_hash_res = rte_be_to_cpu_32(cqe->rx_hash_res);
		else
			rss_hash_res = rte_be_to_cpu_32(mcqe->rx_hash_result);
		if (rss_hash_res) {
			pkt->hash.rss = rss_hash_res;
			pkt->ol_flags |= PKT_RX_RSS_HASH;
		}
	}
	if (rxq->mark) {
		uint32_t mark = 0;

		/* If compressed, take flow tag from mini-CQE. */
		if (mcqe == NULL ||
		    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_FTAG_STRIDX)
			mark = cqe->sop_drop_qpn;
		else
			mark = ((mcqe->byte_cnt_flow & 0xff) << 8) |
				(mcqe->flow_tag_high << 16);
		if (MLX5_FLOW_MARK_IS_VALID(mark)) {
			pkt->ol_flags |= PKT_RX_FDIR;
			if (mark != RTE_BE32(MLX5_FLOW_MARK_DEFAULT)) {
				pkt->ol_flags |= PKT_RX_FDIR_ID;
				pkt->hash.fdir.hi = mlx5_flow_mark_get(mark);
			}
		}
	}
	if (rxq->dynf_meta) {
		uint32_t meta = cqe->flow_table_metadata &
				rxq->flow_meta_port_mask;

		if (meta) {
			pkt->ol_flags |= rxq->flow_meta_mask;
			*RTE_MBUF_DYNFIELD(pkt, rxq->flow_meta_offset,
						uint32_t *) = meta;
		}
	}
	if (rxq->csum)
		pkt->ol_flags |= rxq_cq_to_ol_flags(cqe);
	if (rxq->vlan_strip) {
		bool vlan_strip;

		if (mcqe == NULL ||
		    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_L34H_STRIDX)
			vlan_strip = cqe->hdr_type_etc &
				     RTE_BE16(MLX5_CQE_VLAN_STRIPPED);
		else
			vlan_strip = mcqe->hdr_type &
				     RTE_BE16(MLX5_CQE_VLAN_STRIPPED);
		if (vlan_strip) {
			pkt->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
			pkt->vlan_tci = rte_be_to_cpu_16(cqe->vlan_info);
		}
	}
	if (rxq->hw_timestamp) {
		uint64_t ts = rte_be_to_cpu_64(cqe->timestamp);

		if (rxq->rt_timestamp)
			ts = mlx5_txpp_convert_rx_ts(rxq->sh, ts);
		mlx5_timestamp_set(pkt, rxq->timestamp_offset, ts);
		pkt->ol_flags |= rxq->timestamp_rx_flag;
	}
}

/**
 * DPDK callback for RX.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_rxq_data *rxq = dpdk_rxq;
	const unsigned int wqe_cnt = (1 << rxq->elts_n) - 1;
	const unsigned int cqe_cnt = (1 << rxq->cqe_n) - 1;
	const unsigned int sges_n = rxq->sges_n;
	struct rte_mbuf *pkt = NULL;
	struct rte_mbuf *seg = NULL;
	volatile struct mlx5_cqe *cqe =
		&(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
	unsigned int i = 0;
	unsigned int rq_ci = rxq->rq_ci << sges_n;
	int len = 0; /* keep its value across iterations. */

	while (pkts_n) {
		unsigned int idx = rq_ci & wqe_cnt;
		volatile struct mlx5_wqe_data_seg *wqe =
			&((volatile struct mlx5_wqe_data_seg *)rxq->wqes)[idx];
		struct rte_mbuf *rep = (*rxq->elts)[idx];
		volatile struct mlx5_mini_cqe8 *mcqe = NULL;

		if (pkt)
			NEXT(seg) = rep;
		seg = rep;
		rte_prefetch0(seg);
		rte_prefetch0(cqe);
		rte_prefetch0(wqe);
		/* Allocate the buf from the same pool. */
		rep = rte_mbuf_raw_alloc(seg->pool);
		if (unlikely(rep == NULL)) {
			++rxq->stats.rx_nombuf;
			if (!pkt) {
				/*
				 * no buffers before we even started,
				 * bail out silently.
				 */
				break;
			}
			while (pkt != seg) {
				MLX5_ASSERT(pkt != (*rxq->elts)[idx]);
				rep = NEXT(pkt);
				NEXT(pkt) = NULL;
				NB_SEGS(pkt) = 1;
				rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			rq_ci >>= sges_n;
			++rq_ci;
			rq_ci <<= sges_n;
			break;
		}
		if (!pkt) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			len = mlx5_rx_poll_len(rxq, cqe, cqe_cnt, &mcqe);
			if (!len) {
				rte_mbuf_raw_free(rep);
				break;
			}
			pkt = seg;
			MLX5_ASSERT(len >= (rxq->crc_present << 2));
			pkt->ol_flags &= EXT_ATTACHED_MBUF;
			rxq_cq_to_mbuf(rxq, pkt, cqe, mcqe);
			if (rxq->crc_present)
				len -= RTE_ETHER_CRC_LEN;
			PKT_LEN(pkt) = len;
			if (cqe->lro_num_seg > 1) {
				mlx5_lro_update_hdr
					(rte_pktmbuf_mtod(pkt, uint8_t *), cqe,
					 mcqe, rxq, len);
				pkt->ol_flags |= PKT_RX_LRO;
				pkt->tso_segsz = len / cqe->lro_num_seg;
			}
		}
		DATA_LEN(rep) = DATA_LEN(seg);
		PKT_LEN(rep) = PKT_LEN(seg);
		SET_DATA_OFF(rep, DATA_OFF(seg));
		PORT(rep) = PORT(seg);
		(*rxq->elts)[idx] = rep;
		/*
		 * Fill NIC descriptor with the new buffer.  The lkey and size
		 * of the buffers are already known, only the buffer address
		 * changes.
		 */
		wqe->addr = rte_cpu_to_be_64(rte_pktmbuf_mtod(rep, uintptr_t));
		/* If there's only one MR, no need to replace LKey in WQE. */
		if (unlikely(mlx5_mr_btree_len(&rxq->mr_ctrl.cache_bh) > 1))
			wqe->lkey = mlx5_rx_mb2mr(rxq, rep);
		if (len > DATA_LEN(seg)) {
			len -= DATA_LEN(seg);
			++NB_SEGS(pkt);
			++rq_ci;
			continue;
		}
		DATA_LEN(seg) = len;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += PKT_LEN(pkt);
#endif
		/* Return packet. */
		*(pkts++) = pkt;
		pkt = NULL;
		--pkts_n;
		++i;
		/* Align consumer index to the next stride. */
		rq_ci >>= sges_n;
		++rq_ci;
		rq_ci <<= sges_n;
	}
	if (unlikely((i == 0) && ((rq_ci >> sges_n) == rxq->rq_ci)))
		return 0;
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci >> sges_n;
	rte_io_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	rte_io_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
#endif
	return i;
}

/**
 * Update LRO packet TCP header.
 * The HW LRO feature doesn't update the TCP header after coalescing the
 * TCP segments but supplies information in CQE to fill it by SW.
 *
 * @param tcp
 *   Pointer to the TCP header.
 * @param cqe
 *   Pointer to the completion entry..
 * @param phcsum
 *   The L3 pseudo-header checksum.
 */
static inline void
mlx5_lro_update_tcp_hdr(struct rte_tcp_hdr *__rte_restrict tcp,
			volatile struct mlx5_cqe *__rte_restrict cqe,
			uint32_t phcsum, uint8_t l4_type)
{
	/*
	 * The HW calculates only the TCP payload checksum, need to complete
	 * the TCP header checksum and the L3 pseudo-header checksum.
	 */
	uint32_t csum = phcsum + cqe->csum;

	if (l4_type == MLX5_L4_HDR_TYPE_TCP_EMPTY_ACK ||
	    l4_type == MLX5_L4_HDR_TYPE_TCP_WITH_ACL) {
		tcp->tcp_flags |= RTE_TCP_ACK_FLAG;
		tcp->recv_ack = cqe->lro_ack_seq_num;
		tcp->rx_win = cqe->lro_tcp_win;
	}
	if (cqe->lro_tcppsh_abort_dupack & MLX5_CQE_LRO_PUSH_MASK)
		tcp->tcp_flags |= RTE_TCP_PSH_FLAG;
	tcp->cksum = 0;
	csum += rte_raw_cksum(tcp, (tcp->data_off >> 4) * 4);
	csum = ((csum & 0xffff0000) >> 16) + (csum & 0xffff);
	csum = (~csum) & 0xffff;
	if (csum == 0)
		csum = 0xffff;
	tcp->cksum = csum;
}

/**
 * Update LRO packet headers.
 * The HW LRO feature doesn't update the L3/TCP headers after coalescing the
 * TCP segments but supply information in CQE to fill it by SW.
 *
 * @param padd
 *   The packet address.
 * @param cqe
 *   Pointer to the completion entry..
 * @param len
 *   The packet length.
 */
static inline void
mlx5_lro_update_hdr(uint8_t *__rte_restrict padd,
		    volatile struct mlx5_cqe *__rte_restrict cqe,
		    volatile struct mlx5_mini_cqe8 *mcqe,
		    struct mlx5_rxq_data *rxq, uint32_t len)
{
	union {
		struct rte_ether_hdr *eth;
		struct rte_vlan_hdr *vlan;
		struct rte_ipv4_hdr *ipv4;
		struct rte_ipv6_hdr *ipv6;
		struct rte_tcp_hdr *tcp;
		uint8_t *hdr;
	} h = {
			.hdr = padd,
	};
	uint16_t proto = h.eth->ether_type;
	uint32_t phcsum;
	uint8_t l4_type;

	h.eth++;
	while (proto == RTE_BE16(RTE_ETHER_TYPE_VLAN) ||
	       proto == RTE_BE16(RTE_ETHER_TYPE_QINQ)) {
		proto = h.vlan->eth_proto;
		h.vlan++;
	}
	if (proto == RTE_BE16(RTE_ETHER_TYPE_IPV4)) {
		h.ipv4->time_to_live = cqe->lro_min_ttl;
		h.ipv4->total_length = rte_cpu_to_be_16(len - (h.hdr - padd));
		h.ipv4->hdr_checksum = 0;
		h.ipv4->hdr_checksum = rte_ipv4_cksum(h.ipv4);
		phcsum = rte_ipv4_phdr_cksum(h.ipv4, 0);
		h.ipv4++;
	} else {
		h.ipv6->hop_limits = cqe->lro_min_ttl;
		h.ipv6->payload_len = rte_cpu_to_be_16(len - (h.hdr - padd) -
						       sizeof(*h.ipv6));
		phcsum = rte_ipv6_phdr_cksum(h.ipv6, 0);
		h.ipv6++;
	}
	if (mcqe == NULL ||
	    rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_L34H_STRIDX)
		l4_type = (rte_be_to_cpu_16(cqe->hdr_type_etc) &
			   MLX5_CQE_L4_TYPE_MASK) >> MLX5_CQE_L4_TYPE_SHIFT;
	else
		l4_type = (rte_be_to_cpu_16(mcqe->hdr_type) &
			   MLX5_CQE_L4_TYPE_MASK) >> MLX5_CQE_L4_TYPE_SHIFT;
	mlx5_lro_update_tcp_hdr(h.tcp, cqe, phcsum, l4_type);
}

void
mlx5_mprq_buf_free_cb(void *addr __rte_unused, void *opaque)
{
	struct mlx5_mprq_buf *buf = opaque;

	if (__atomic_load_n(&buf->refcnt, __ATOMIC_RELAXED) == 1) {
		rte_mempool_put(buf->mp, buf);
	} else if (unlikely(__atomic_sub_fetch(&buf->refcnt, 1,
					       __ATOMIC_RELAXED) == 0)) {
		__atomic_store_n(&buf->refcnt, 1, __ATOMIC_RELAXED);
		rte_mempool_put(buf->mp, buf);
	}
}

void
mlx5_mprq_buf_free(struct mlx5_mprq_buf *buf)
{
	mlx5_mprq_buf_free_cb(NULL, buf);
}

/**
 * DPDK callback for RX with Multi-Packet RQ support.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst_mprq(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_rxq_data *rxq = dpdk_rxq;
	const uint32_t strd_n = 1 << rxq->strd_num_n;
	const uint32_t strd_sz = 1 << rxq->strd_sz_n;
	const uint32_t cq_mask = (1 << rxq->cqe_n) - 1;
	const uint32_t wq_mask = (1 << rxq->elts_n) - 1;
	volatile struct mlx5_cqe *cqe = &(*rxq->cqes)[rxq->cq_ci & cq_mask];
	unsigned int i = 0;
	uint32_t rq_ci = rxq->rq_ci;
	uint16_t consumed_strd = rxq->consumed_strd;
	struct mlx5_mprq_buf *buf = (*rxq->mprq_bufs)[rq_ci & wq_mask];

	while (i < pkts_n) {
		struct rte_mbuf *pkt;
		int ret;
		uint32_t len;
		uint16_t strd_cnt;
		uint16_t strd_idx;
		uint32_t byte_cnt;
		volatile struct mlx5_mini_cqe8 *mcqe = NULL;
		enum mlx5_rqx_code rxq_code;

		if (consumed_strd == strd_n) {
			/* Replace WQE if the buffer is still in use. */
			mprq_buf_replace(rxq, rq_ci & wq_mask);
			/* Advance to the next WQE. */
			consumed_strd = 0;
			++rq_ci;
			buf = (*rxq->mprq_bufs)[rq_ci & wq_mask];
		}
		cqe = &(*rxq->cqes)[rxq->cq_ci & cq_mask];
		ret = mlx5_rx_poll_len(rxq, cqe, cq_mask, &mcqe);
		if (!ret)
			break;
		byte_cnt = ret;
		len = (byte_cnt & MLX5_MPRQ_LEN_MASK) >> MLX5_MPRQ_LEN_SHIFT;
		MLX5_ASSERT((int)len >= (rxq->crc_present << 2));
		if (rxq->crc_present)
			len -= RTE_ETHER_CRC_LEN;
		if (mcqe &&
		    rxq->mcqe_format == MLX5_CQE_RESP_FORMAT_FTAG_STRIDX)
			strd_cnt = (len / strd_sz) + !!(len % strd_sz);
		else
			strd_cnt = (byte_cnt & MLX5_MPRQ_STRIDE_NUM_MASK) >>
				   MLX5_MPRQ_STRIDE_NUM_SHIFT;
		MLX5_ASSERT(strd_cnt);
		consumed_strd += strd_cnt;
		if (byte_cnt & MLX5_MPRQ_FILLER_MASK)
			continue;
		strd_idx = rte_be_to_cpu_16(mcqe == NULL ?
					cqe->wqe_counter :
					mcqe->stride_idx);
		MLX5_ASSERT(strd_idx < strd_n);
		MLX5_ASSERT(!((rte_be_to_cpu_16(cqe->wqe_id) ^ rq_ci) &
			    wq_mask));
		pkt = rte_pktmbuf_alloc(rxq->mp);
		if (unlikely(pkt == NULL)) {
			++rxq->stats.rx_nombuf;
			break;
		}
		len = (byte_cnt & MLX5_MPRQ_LEN_MASK) >> MLX5_MPRQ_LEN_SHIFT;
		MLX5_ASSERT((int)len >= (rxq->crc_present << 2));
		if (rxq->crc_present)
			len -= RTE_ETHER_CRC_LEN;
		rxq_code = mprq_buf_to_pkt(rxq, pkt, len, buf,
					   strd_idx, strd_cnt);
		if (unlikely(rxq_code != MLX5_RXQ_CODE_EXIT)) {
			rte_pktmbuf_free_seg(pkt);
			if (rxq_code == MLX5_RXQ_CODE_DROPPED) {
				++rxq->stats.idropped;
				continue;
			}
			if (rxq_code == MLX5_RXQ_CODE_NOMBUF) {
				++rxq->stats.rx_nombuf;
				break;
			}
		}
		rxq_cq_to_mbuf(rxq, pkt, cqe, mcqe);
		if (cqe->lro_num_seg > 1) {
			mlx5_lro_update_hdr(rte_pktmbuf_mtod(pkt, uint8_t *),
					    cqe, mcqe, rxq, len);
			pkt->ol_flags |= PKT_RX_LRO;
			pkt->tso_segsz = len / cqe->lro_num_seg;
		}
		PKT_LEN(pkt) = len;
		PORT(pkt) = rxq->port_id;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment bytes counter. */
		rxq->stats.ibytes += PKT_LEN(pkt);
#endif
		/* Return packet. */
		*(pkts++) = pkt;
		++i;
	}
	/* Update the consumer indexes. */
	rxq->consumed_strd = consumed_strd;
	rte_io_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	if (rq_ci != rxq->rq_ci) {
		rxq->rq_ci = rq_ci;
		rte_io_wmb();
		*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
#endif
	return i;
}

/**
 * Dummy DPDK callback for TX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
removed_tx_burst(void *dpdk_txq __rte_unused,
		 struct rte_mbuf **pkts __rte_unused,
		 uint16_t pkts_n __rte_unused)
{
	rte_mb();
	return 0;
}

/**
 * Dummy DPDK callback for RX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
removed_rx_burst(void *dpdk_txq __rte_unused,
		 struct rte_mbuf **pkts __rte_unused,
		 uint16_t pkts_n __rte_unused)
{
	rte_mb();
	return 0;
}

/*
 * Vectorized Rx/Tx routines are not compiled in when required vector
 * instructions are not supported on a target architecture. The following null
 * stubs are needed for linkage when those are not included outside of this file
 * (e.g.  mlx5_rxtx_vec_sse.c for x86).
 */

__rte_weak uint16_t
mlx5_rx_burst_vec(void *dpdk_txq __rte_unused,
		  struct rte_mbuf **pkts __rte_unused,
		  uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak uint16_t
mlx5_rx_burst_mprq_vec(void *dpdk_txq __rte_unused,
		       struct rte_mbuf **pkts __rte_unused,
		       uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak int
mlx5_rxq_check_vec_support(struct mlx5_rxq_data *rxq __rte_unused)
{
	return -ENOTSUP;
}

__rte_weak int
mlx5_check_vec_rx_support(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}

/**
 * Free the mbufs from the linear array of pointers.
 *
 * @param pkts
 *   Pointer to array of packets to be free.
 * @param pkts_n
 *   Number of packets to be freed.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_free_mbuf(struct rte_mbuf **__rte_restrict pkts,
		  unsigned int pkts_n,
		  unsigned int olx __rte_unused)
{
	struct rte_mempool *pool = NULL;
	struct rte_mbuf **p_free = NULL;
	struct rte_mbuf *mbuf;
	unsigned int n_free = 0;

	/*
	 * The implemented algorithm eliminates
	 * copying pointers to temporary array
	 * for rte_mempool_put_bulk() calls.
	 */
	MLX5_ASSERT(pkts);
	MLX5_ASSERT(pkts_n);
	for (;;) {
		for (;;) {
			/*
			 * Decrement mbuf reference counter, detach
			 * indirect and external buffers if needed.
			 */
			mbuf = rte_pktmbuf_prefree_seg(*pkts);
			if (likely(mbuf != NULL)) {
				MLX5_ASSERT(mbuf == *pkts);
				if (likely(n_free != 0)) {
					if (unlikely(pool != mbuf->pool))
						/* From different pool. */
						break;
				} else {
					/* Start new scan array. */
					pool = mbuf->pool;
					p_free = pkts;
				}
				++n_free;
				++pkts;
				--pkts_n;
				if (unlikely(pkts_n == 0)) {
					mbuf = NULL;
					break;
				}
			} else {
				/*
				 * This happens if mbuf is still referenced.
				 * We can't put it back to the pool, skip.
				 */
				++pkts;
				--pkts_n;
				if (unlikely(n_free != 0))
					/* There is some array to free.*/
					break;
				if (unlikely(pkts_n == 0))
					/* Last mbuf, nothing to free. */
					return;
			}
		}
		for (;;) {
			/*
			 * This loop is implemented to avoid multiple
			 * inlining of rte_mempool_put_bulk().
			 */
			MLX5_ASSERT(pool);
			MLX5_ASSERT(p_free);
			MLX5_ASSERT(n_free);
			/*
			 * Free the array of pre-freed mbufs
			 * belonging to the same memory pool.
			 */
			rte_mempool_put_bulk(pool, (void *)p_free, n_free);
			if (unlikely(mbuf != NULL)) {
				/* There is the request to start new scan. */
				pool = mbuf->pool;
				p_free = pkts++;
				n_free = 1;
				--pkts_n;
				if (likely(pkts_n != 0))
					break;
				/*
				 * This is the last mbuf to be freed.
				 * Do one more loop iteration to complete.
				 * This is rare case of the last unique mbuf.
				 */
				mbuf = NULL;
				continue;
			}
			if (likely(pkts_n == 0))
				return;
			n_free = 0;
			break;
		}
	}
}

/**
 * Free the mbuf from the elts ring buffer till new tail.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param tail
 *   Index in elts to free up to, becomes new elts tail.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_free_elts(struct mlx5_txq_data *__rte_restrict txq,
		  uint16_t tail,
		  unsigned int olx __rte_unused)
{
	uint16_t n_elts = tail - txq->elts_tail;

	MLX5_ASSERT(n_elts);
	MLX5_ASSERT(n_elts <= txq->elts_s);
	/*
	 * Implement a loop to support ring buffer wraparound
	 * with single inlining of mlx5_tx_free_mbuf().
	 */
	do {
		unsigned int part;

		part = txq->elts_s - (txq->elts_tail & txq->elts_m);
		part = RTE_MIN(part, n_elts);
		MLX5_ASSERT(part);
		MLX5_ASSERT(part <= txq->elts_s);
		mlx5_tx_free_mbuf(&txq->elts[txq->elts_tail & txq->elts_m],
				  part, olx);
		txq->elts_tail += part;
		n_elts -= part;
	} while (n_elts);
}

/**
 * Store the mbuf being sent into elts ring buffer.
 * On Tx completion these mbufs will be freed.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param pkts
 *   Pointer to array of packets to be stored.
 * @param pkts_n
 *   Number of packets to be stored.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_copy_elts(struct mlx5_txq_data *__rte_restrict txq,
		  struct rte_mbuf **__rte_restrict pkts,
		  unsigned int pkts_n,
		  unsigned int olx __rte_unused)
{
	unsigned int part;
	struct rte_mbuf **elts = (struct rte_mbuf **)txq->elts;

	MLX5_ASSERT(pkts);
	MLX5_ASSERT(pkts_n);
	part = txq->elts_s - (txq->elts_head & txq->elts_m);
	MLX5_ASSERT(part);
	MLX5_ASSERT(part <= txq->elts_s);
	/* This code is a good candidate for vectorizing with SIMD. */
	rte_memcpy((void *)(elts + (txq->elts_head & txq->elts_m)),
		   (void *)pkts,
		   RTE_MIN(part, pkts_n) * sizeof(struct rte_mbuf *));
	txq->elts_head += pkts_n;
	if (unlikely(part < pkts_n))
		/* The copy is wrapping around the elts array. */
		rte_memcpy((void *)elts, (void *)(pkts + part),
			   (pkts_n - part) * sizeof(struct rte_mbuf *));
}

/**
 * Update completion queue consuming index via doorbell
 * and flush the completed data buffers.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param valid CQE pointer
 *   if not NULL update txq->wqe_pi and flush the buffers
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_comp_flush(struct mlx5_txq_data *__rte_restrict txq,
		   volatile struct mlx5_cqe *last_cqe,
		   unsigned int olx __rte_unused)
{
	if (likely(last_cqe != NULL)) {
		uint16_t tail;

		txq->wqe_pi = rte_be_to_cpu_16(last_cqe->wqe_counter);
		tail = txq->fcqs[(txq->cq_ci - 1) & txq->cqe_m];
		if (likely(tail != txq->elts_tail)) {
			mlx5_tx_free_elts(txq, tail, olx);
			MLX5_ASSERT(tail == txq->elts_tail);
		}
	}
}

/**
 * Manage TX completions. This routine checks the CQ for
 * arrived CQEs, deduces the last accomplished WQE in SQ,
 * updates SQ producing index and frees all completed mbufs.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * NOTE: not inlined intentionally, it makes tx_burst
 * routine smaller, simple and faster - from experiments.
 */
static void
mlx5_tx_handle_completion(struct mlx5_txq_data *__rte_restrict txq,
			  unsigned int olx __rte_unused)
{
	unsigned int count = MLX5_TX_COMP_MAX_CQE;
	volatile struct mlx5_cqe *last_cqe = NULL;
	bool ring_doorbell = false;
	int ret;

	static_assert(MLX5_CQE_STATUS_HW_OWN < 0, "Must be negative value");
	static_assert(MLX5_CQE_STATUS_SW_OWN < 0, "Must be negative value");
	do {
		volatile struct mlx5_cqe *cqe;

		cqe = &txq->cqes[txq->cq_ci & txq->cqe_m];
		ret = check_cqe(cqe, txq->cqe_s, txq->cq_ci);
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (likely(ret != MLX5_CQE_STATUS_ERR)) {
				/* No new CQEs in completion queue. */
				MLX5_ASSERT(ret == MLX5_CQE_STATUS_HW_OWN);
				break;
			}
			/*
			 * Some error occurred, try to restart.
			 * We have no barrier after WQE related Doorbell
			 * written, make sure all writes are completed
			 * here, before we might perform SQ reset.
			 */
			rte_wmb();
			ret = mlx5_tx_error_cqe_handle
				(txq, (volatile struct mlx5_err_cqe *)cqe);
			if (unlikely(ret < 0)) {
				/*
				 * Some error occurred on queue error
				 * handling, we do not advance the index
				 * here, allowing to retry on next call.
				 */
				return;
			}
			/*
			 * We are going to fetch all entries with
			 * MLX5_CQE_SYNDROME_WR_FLUSH_ERR status.
			 * The send queue is supposed to be empty.
			 */
			ring_doorbell = true;
			++txq->cq_ci;
			txq->cq_pi = txq->cq_ci;
			last_cqe = NULL;
			continue;
		}
		/* Normal transmit completion. */
		MLX5_ASSERT(txq->cq_ci != txq->cq_pi);
#ifdef RTE_LIBRTE_MLX5_DEBUG
		MLX5_ASSERT((txq->fcqs[txq->cq_ci & txq->cqe_m] >> 16) ==
			    cqe->wqe_counter);
#endif
		ring_doorbell = true;
		++txq->cq_ci;
		last_cqe = cqe;
		/*
		 * We have to restrict the amount of processed CQEs
		 * in one tx_burst routine call. The CQ may be large
		 * and many CQEs may be updated by the NIC in one
		 * transaction. Buffers freeing is time consuming,
		 * multiple iterations may introduce significant
		 * latency.
		 */
		if (likely(--count == 0))
			break;
	} while (true);
	if (likely(ring_doorbell)) {
		/* Ring doorbell to notify hardware. */
		rte_compiler_barrier();
		*txq->cq_db = rte_cpu_to_be_32(txq->cq_ci);
		mlx5_tx_comp_flush(txq, last_cqe, olx);
	}
}

/**
 * Check if the completion request flag should be set in the last WQE.
 * Both pushed mbufs and WQEs are monitored and the completion request
 * flag is set if any of thresholds is reached.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_request_completion(struct mlx5_txq_data *__rte_restrict txq,
			   struct mlx5_txq_local *__rte_restrict loc,
			   unsigned int olx)
{
	uint16_t head = txq->elts_head;
	unsigned int part;

	part = MLX5_TXOFF_CONFIG(INLINE) ?
	       0 : loc->pkts_sent - loc->pkts_copy;
	head += part;
	if ((uint16_t)(head - txq->elts_comp) >= MLX5_TX_COMP_THRESH ||
	     (MLX5_TXOFF_CONFIG(INLINE) &&
	     (uint16_t)(txq->wqe_ci - txq->wqe_comp) >= txq->wqe_thres)) {
		volatile struct mlx5_wqe *last = loc->wqe_last;

		MLX5_ASSERT(last);
		txq->elts_comp = head;
		if (MLX5_TXOFF_CONFIG(INLINE))
			txq->wqe_comp = txq->wqe_ci;
		/* Request unconditional completion on last WQE. */
		last->cseg.flags = RTE_BE32(MLX5_COMP_ALWAYS <<
					    MLX5_COMP_MODE_OFFSET);
		/* Save elts_head in dedicated free on completion queue. */
#ifdef RTE_LIBRTE_MLX5_DEBUG
		txq->fcqs[txq->cq_pi++ & txq->cqe_m] = head |
			  (last->cseg.opcode >> 8) << 16;
#else
		txq->fcqs[txq->cq_pi++ & txq->cqe_m] = head;
#endif
		/* A CQE slot must always be available. */
		MLX5_ASSERT((txq->cq_pi - txq->cq_ci) <= txq->cqe_s);
	}
}

/**
 * DPDK callback to check the status of a tx descriptor.
 *
 * @param tx_queue
 *   The tx queue.
 * @param[in] offset
 *   The index of the descriptor in the ring.
 *
 * @return
 *   The status of the tx descriptor.
 */
int
mlx5_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct mlx5_txq_data *__rte_restrict txq = tx_queue;
	uint16_t used;

	mlx5_tx_handle_completion(txq, 0);
	used = txq->elts_head - txq->elts_tail;
	if (offset < used)
		return RTE_ETH_TX_DESC_FULL;
	return RTE_ETH_TX_DESC_DONE;
}

/**
 * Build the Control Segment with specified opcode:
 * - MLX5_OPCODE_SEND
 * - MLX5_OPCODE_ENHANCED_MPSW
 * - MLX5_OPCODE_TSO
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Control Segment.
 * @param ds
 *   Supposed length of WQE in segments.
 * @param opcode
 *   SQ WQE opcode to put into Control Segment.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_cseg_init(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc __rte_unused,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int ds,
		  unsigned int opcode,
		  unsigned int olx __rte_unused)
{
	struct mlx5_wqe_cseg *__rte_restrict cs = &wqe->cseg;

	/* For legacy MPW replace the EMPW by TSO with modifier. */
	if (MLX5_TXOFF_CONFIG(MPW) && opcode == MLX5_OPCODE_ENHANCED_MPSW)
		opcode = MLX5_OPCODE_TSO | MLX5_OPC_MOD_MPW << 24;
	cs->opcode = rte_cpu_to_be_32((txq->wqe_ci << 8) | opcode);
	cs->sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	cs->flags = RTE_BE32(MLX5_COMP_ONLY_FIRST_ERR <<
			     MLX5_COMP_MODE_OFFSET);
	cs->misc = RTE_BE32(0);
}

/**
 * Build the Synchronize Queue Segment with specified completion index.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Control Segment.
 * @param wci
 *   Completion index in Clock Queue to wait.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_wseg_init(struct mlx5_txq_data *restrict txq,
		  struct mlx5_txq_local *restrict loc __rte_unused,
		  struct mlx5_wqe *restrict wqe,
		  unsigned int wci,
		  unsigned int olx __rte_unused)
{
	struct mlx5_wqe_qseg *qs;

	qs = RTE_PTR_ADD(wqe, MLX5_WSEG_SIZE);
	qs->max_index = rte_cpu_to_be_32(wci);
	qs->qpn_cqn = rte_cpu_to_be_32(txq->sh->txpp.clock_queue.cq->id);
	qs->reserved0 = RTE_BE32(0);
	qs->reserved1 = RTE_BE32(0);
}

/**
 * Build the Ethernet Segment without inlined data.
 * Supports Software Parser, Checksums and VLAN
 * insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_eseg_none(struct mlx5_txq_data *__rte_restrict txq __rte_unused,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;

	/*
	 * Calculate and set check sum flags first, dword field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	es->flags = rte_cpu_to_le_32(csum);
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & PKT_TX_DYNF_METADATA ?
		       *RTE_FLOW_DYNF_METADATA(loc->mbuf) : 0 : 0;
	/* Engage VLAN tag insertion feature if requested. */
	if (MLX5_TXOFF_CONFIG(VLAN) &&
	    loc->mbuf->ol_flags & PKT_TX_VLAN_PKT) {
		/*
		 * We should get here only if device support
		 * this feature correctly.
		 */
		MLX5_ASSERT(txq->vlan_en);
		es->inline_hdr = rte_cpu_to_be_32(MLX5_ETH_WQE_VLAN_INSERT |
						  loc->mbuf->vlan_tci);
	} else {
		es->inline_hdr = RTE_BE32(0);
	}
}

/**
 * Build the Ethernet Segment with minimal inlined data
 * of MLX5_ESEG_MIN_INLINE_SIZE bytes length. This is
 * used to fill the gap in single WQEBB WQEs.
 * Supports Software Parser, Checksums and VLAN
 * insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param vlan
 *   Length of VLAN tag insertion if any.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_eseg_dmin(struct mlx5_txq_data *__rte_restrict txq __rte_unused,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int vlan,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;
	uint8_t *psrc, *pdst;

	/*
	 * Calculate and set check sum flags first, dword field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	es->flags = rte_cpu_to_le_32(csum);
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & PKT_TX_DYNF_METADATA ?
		       *RTE_FLOW_DYNF_METADATA(loc->mbuf) : 0 : 0;
	static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
				(sizeof(uint16_t) +
				 sizeof(rte_v128u32_t)),
		      "invalid Ethernet Segment data size");
	static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
				(sizeof(uint16_t) +
				 sizeof(struct rte_vlan_hdr) +
				 2 * RTE_ETHER_ADDR_LEN),
		      "invalid Ethernet Segment data size");
	psrc = rte_pktmbuf_mtod(loc->mbuf, uint8_t *);
	es->inline_hdr_sz = RTE_BE16(MLX5_ESEG_MIN_INLINE_SIZE);
	es->inline_data = *(unaligned_uint16_t *)psrc;
	psrc +=	sizeof(uint16_t);
	pdst = (uint8_t *)(es + 1);
	if (MLX5_TXOFF_CONFIG(VLAN) && vlan) {
		/* Implement VLAN tag insertion as part inline data. */
		memcpy(pdst, psrc, 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t));
		pdst += 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		psrc +=	2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		/* Insert VLAN ethertype + VLAN tag. */
		*(unaligned_uint32_t *)pdst = rte_cpu_to_be_32
						((RTE_ETHER_TYPE_VLAN << 16) |
						 loc->mbuf->vlan_tci);
		pdst += sizeof(struct rte_vlan_hdr);
		/* Copy the rest two bytes from packet data. */
		MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, sizeof(uint16_t)));
		*(uint16_t *)pdst = *(unaligned_uint16_t *)psrc;
	} else {
		/* Fill the gap in the title WQEBB with inline data. */
		rte_mov16(pdst, psrc);
	}
}

/**
 * Build the Ethernet Segment with entire packet
 * data inlining. Checks the boundary of WQEBB and
 * ring buffer wrapping, supports Software Parser,
 * Checksums and VLAN insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param vlan
 *   Length of VLAN tag insertion if any.
 * @param inlen
 *   Length of data to inline (VLAN included, if any).
 * @param tso
 *   TSO flag, set mss field from the packet.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment (aligned and wrapped around).
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_eseg_data(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int vlan,
		  unsigned int inlen,
		  unsigned int tso,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;
	uint8_t *psrc, *pdst;
	unsigned int part;

	/*
	 * Calculate and set check sum flags first, dword field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	if (tso) {
		csum <<= 24;
		csum |= loc->mbuf->tso_segsz;
		es->flags = rte_cpu_to_be_32(csum);
	} else {
		es->flags = rte_cpu_to_le_32(csum);
	}
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & PKT_TX_DYNF_METADATA ?
		       *RTE_FLOW_DYNF_METADATA(loc->mbuf) : 0 : 0;
	static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
				(sizeof(uint16_t) +
				 sizeof(rte_v128u32_t)),
		      "invalid Ethernet Segment data size");
	static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
				(sizeof(uint16_t) +
				 sizeof(struct rte_vlan_hdr) +
				 2 * RTE_ETHER_ADDR_LEN),
		      "invalid Ethernet Segment data size");
	psrc = rte_pktmbuf_mtod(loc->mbuf, uint8_t *);
	es->inline_hdr_sz = rte_cpu_to_be_16(inlen);
	es->inline_data = *(unaligned_uint16_t *)psrc;
	psrc +=	sizeof(uint16_t);
	pdst = (uint8_t *)(es + 1);
	if (MLX5_TXOFF_CONFIG(VLAN) && vlan) {
		/* Implement VLAN tag insertion as part inline data. */
		memcpy(pdst, psrc, 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t));
		pdst += 2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		psrc +=	2 * RTE_ETHER_ADDR_LEN - sizeof(uint16_t);
		/* Insert VLAN ethertype + VLAN tag. */
		*(unaligned_uint32_t *)pdst = rte_cpu_to_be_32
						((RTE_ETHER_TYPE_VLAN << 16) |
						 loc->mbuf->vlan_tci);
		pdst += sizeof(struct rte_vlan_hdr);
		/* Copy the rest two bytes from packet data. */
		MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, sizeof(uint16_t)));
		*(uint16_t *)pdst = *(unaligned_uint16_t *)psrc;
		psrc += sizeof(uint16_t);
	} else {
		/* Fill the gap in the title WQEBB with inline data. */
		rte_mov16(pdst, psrc);
		psrc += sizeof(rte_v128u32_t);
	}
	pdst = (uint8_t *)(es + 2);
	MLX5_ASSERT(inlen >= MLX5_ESEG_MIN_INLINE_SIZE);
	MLX5_ASSERT(pdst < (uint8_t *)txq->wqes_end);
	inlen -= MLX5_ESEG_MIN_INLINE_SIZE;
	if (!inlen) {
		MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE));
		return (struct mlx5_wqe_dseg *)pdst;
	}
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, inlen);
	do {
		rte_memcpy(pdst, psrc, part);
		inlen -= part;
		if (likely(!inlen)) {
			/*
			 * If return value is not used by the caller
			 * the code below will be optimized out.
			 */
			pdst += part;
			pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			if (unlikely(pdst >= (uint8_t *)txq->wqes_end))
				pdst = (uint8_t *)txq->wqes;
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		psrc += part;
		part = inlen;
	} while (true);
}

/**
 * Copy data from chain of mbuf to the specified linear buffer.
 * Checksums and VLAN insertion Tx offload features. If data
 * from some mbuf copied completely this mbuf is freed. Local
 * structure is used to keep the byte stream state.
 *
 * @param pdst
 *   Pointer to the destination linear buffer.
 * @param loc
 *   Pointer to burst routine local context.
 * @param len
 *   Length of data to be copied.
 * @param must
 *   Length of data to be copied ignoring no inline hint.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Number of actual copied data bytes. This is always greater than or
 *   equal to must parameter and might be lesser than len in no inline
 *   hint flag is encountered.
 */
static __rte_always_inline unsigned int
mlx5_tx_mseg_memcpy(uint8_t *pdst,
		    struct mlx5_txq_local *__rte_restrict loc,
		    unsigned int len,
		    unsigned int must,
		    unsigned int olx __rte_unused)
{
	struct rte_mbuf *mbuf;
	unsigned int part, dlen, copy = 0;
	uint8_t *psrc;

	MLX5_ASSERT(len);
	MLX5_ASSERT(must <= len);
	do {
		/* Allow zero length packets, must check first. */
		dlen = rte_pktmbuf_data_len(loc->mbuf);
		if (dlen <= loc->mbuf_off) {
			/* Exhausted packet, just free. */
			mbuf = loc->mbuf;
			loc->mbuf = mbuf->next;
			rte_pktmbuf_free_seg(mbuf);
			loc->mbuf_off = 0;
			MLX5_ASSERT(loc->mbuf_nseg > 1);
			MLX5_ASSERT(loc->mbuf);
			--loc->mbuf_nseg;
			if (loc->mbuf->ol_flags & PKT_TX_DYNF_NOINLINE) {
				unsigned int diff;

				if (copy >= must) {
					/*
					 * We already copied the minimal
					 * requested amount of data.
					 */
					return copy;
				}
				diff = must - copy;
				if (diff <= rte_pktmbuf_data_len(loc->mbuf)) {
					/*
					 * Copy only the minimal required
					 * part of the data buffer.
					 */
					len = diff;
				}
			}
			continue;
		}
		dlen -= loc->mbuf_off;
		psrc = rte_pktmbuf_mtod_offset(loc->mbuf, uint8_t *,
					       loc->mbuf_off);
		part = RTE_MIN(len, dlen);
		rte_memcpy(pdst, psrc, part);
		copy += part;
		loc->mbuf_off += part;
		len -= part;
		if (!len) {
			if (loc->mbuf_off >= rte_pktmbuf_data_len(loc->mbuf)) {
				loc->mbuf_off = 0;
				/* Exhausted packet, just free. */
				mbuf = loc->mbuf;
				loc->mbuf = mbuf->next;
				rte_pktmbuf_free_seg(mbuf);
				loc->mbuf_off = 0;
				MLX5_ASSERT(loc->mbuf_nseg >= 1);
				--loc->mbuf_nseg;
			}
			return copy;
		}
		pdst += part;
	} while (true);
}

/**
 * Build the Ethernet Segment with inlined data from
 * multi-segment packet. Checks the boundary of WQEBB
 * and ring buffer wrapping, supports Software Parser,
 * Checksums and VLAN insertion Tx offload features.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet Segment.
 * @param vlan
 *   Length of VLAN tag insertion if any.
 * @param inlen
 *   Length of data to inline (VLAN included, if any).
 * @param tso
 *   TSO flag, set mss field from the packet.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment (aligned and
 *   possible NOT wrapped around - caller should do
 *   wrapping check on its own).
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_eseg_mdat(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe *__rte_restrict wqe,
		  unsigned int vlan,
		  unsigned int inlen,
		  unsigned int tso,
		  unsigned int olx)
{
	struct mlx5_wqe_eseg *__rte_restrict es = &wqe->eseg;
	uint32_t csum;
	uint8_t *pdst;
	unsigned int part, tlen = 0;

	/*
	 * Calculate and set check sum flags first, uint32_t field
	 * in segment may be shared with Software Parser flags.
	 */
	csum = MLX5_TXOFF_CONFIG(CSUM) ? txq_ol_cksum_to_cs(loc->mbuf) : 0;
	if (tso) {
		csum <<= 24;
		csum |= loc->mbuf->tso_segsz;
		es->flags = rte_cpu_to_be_32(csum);
	} else {
		es->flags = rte_cpu_to_le_32(csum);
	}
	/*
	 * Calculate and set Software Parser offsets and flags.
	 * These flags a set for custom UDP and IP tunnel packets.
	 */
	es->swp_offs = txq_mbuf_to_swp(loc, &es->swp_flags, olx);
	/* Fill metadata field if needed. */
	es->metadata = MLX5_TXOFF_CONFIG(METADATA) ?
		       loc->mbuf->ol_flags & PKT_TX_DYNF_METADATA ?
		       *RTE_FLOW_DYNF_METADATA(loc->mbuf) : 0 : 0;
	static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
				(sizeof(uint16_t) +
				 sizeof(rte_v128u32_t)),
		      "invalid Ethernet Segment data size");
	static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
				(sizeof(uint16_t) +
				 sizeof(struct rte_vlan_hdr) +
				 2 * RTE_ETHER_ADDR_LEN),
		      "invalid Ethernet Segment data size");
	MLX5_ASSERT(inlen >= MLX5_ESEG_MIN_INLINE_SIZE);
	pdst = (uint8_t *)&es->inline_data;
	if (MLX5_TXOFF_CONFIG(VLAN) && vlan) {
		/* Implement VLAN tag insertion as part inline data. */
		mlx5_tx_mseg_memcpy(pdst, loc,
				    2 * RTE_ETHER_ADDR_LEN,
				    2 * RTE_ETHER_ADDR_LEN, olx);
		pdst += 2 * RTE_ETHER_ADDR_LEN;
		*(unaligned_uint32_t *)pdst = rte_cpu_to_be_32
						((RTE_ETHER_TYPE_VLAN << 16) |
						 loc->mbuf->vlan_tci);
		pdst += sizeof(struct rte_vlan_hdr);
		tlen += 2 * RTE_ETHER_ADDR_LEN + sizeof(struct rte_vlan_hdr);
	}
	MLX5_ASSERT(pdst < (uint8_t *)txq->wqes_end);
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, inlen - tlen);
	MLX5_ASSERT(part);
	do {
		unsigned int copy;

		/*
		 * Copying may be interrupted inside the routine
		 * if run into no inline hint flag.
		 */
		copy = tso ? inlen : txq->inlen_mode;
		copy = tlen >= copy ? 0 : (copy - tlen);
		copy = mlx5_tx_mseg_memcpy(pdst, loc, part, copy, olx);
		tlen += copy;
		if (likely(inlen <= tlen) || copy < part) {
			es->inline_hdr_sz = rte_cpu_to_be_16(tlen);
			pdst += copy;
			pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		part = inlen - tlen;
	} while (true);
}

/**
 * Build the Data Segment of pointer type.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to WQE to fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_dseg_ptr(struct mlx5_txq_data *__rte_restrict txq,
		 struct mlx5_txq_local *__rte_restrict loc,
		 struct mlx5_wqe_dseg *__rte_restrict dseg,
		 uint8_t *buf,
		 unsigned int len,
		 unsigned int olx __rte_unused)

{
	MLX5_ASSERT(len);
	dseg->bcount = rte_cpu_to_be_32(len);
	dseg->lkey = mlx5_tx_mb2mr(txq, loc->mbuf);
	dseg->pbuf = rte_cpu_to_be_64((uintptr_t)buf);
}

/**
 * Build the Data Segment of pointer type or inline
 * if data length is less than buffer in minimal
 * Data Segment size.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to WQE to fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 */
static __rte_always_inline void
mlx5_tx_dseg_iptr(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc,
		  struct mlx5_wqe_dseg *__rte_restrict dseg,
		  uint8_t *buf,
		  unsigned int len,
		  unsigned int olx __rte_unused)

{
	uintptr_t dst, src;

	MLX5_ASSERT(len);
	if (len > MLX5_DSEG_MIN_INLINE_SIZE) {
		dseg->bcount = rte_cpu_to_be_32(len);
		dseg->lkey = mlx5_tx_mb2mr(txq, loc->mbuf);
		dseg->pbuf = rte_cpu_to_be_64((uintptr_t)buf);

		return;
	}
	dseg->bcount = rte_cpu_to_be_32(len | MLX5_ETH_WQE_DATA_INLINE);
	/* Unrolled implementation of generic rte_memcpy. */
	dst = (uintptr_t)&dseg->inline_data[0];
	src = (uintptr_t)buf;
	if (len & 0x08) {
#ifdef RTE_ARCH_STRICT_ALIGN
		MLX5_ASSERT(dst == RTE_PTR_ALIGN(dst, sizeof(uint32_t)));
		*(uint32_t *)dst = *(unaligned_uint32_t *)src;
		dst += sizeof(uint32_t);
		src += sizeof(uint32_t);
		*(uint32_t *)dst = *(unaligned_uint32_t *)src;
		dst += sizeof(uint32_t);
		src += sizeof(uint32_t);
#else
		*(uint64_t *)dst = *(unaligned_uint64_t *)src;
		dst += sizeof(uint64_t);
		src += sizeof(uint64_t);
#endif
	}
	if (len & 0x04) {
		*(uint32_t *)dst = *(unaligned_uint32_t *)src;
		dst += sizeof(uint32_t);
		src += sizeof(uint32_t);
	}
	if (len & 0x02) {
		*(uint16_t *)dst = *(unaligned_uint16_t *)src;
		dst += sizeof(uint16_t);
		src += sizeof(uint16_t);
	}
	if (len & 0x01)
		*(uint8_t *)dst = *(uint8_t *)src;
}

/**
 * Build the Data Segment of inlined data from single
 * segment packet, no VLAN insertion.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to WQE to fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment after inlined data.
 *   Ring buffer wraparound check is needed. We do not
 *   do it here because it may not be needed for the
 *   last packet in the eMPW session.
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_dseg_empw(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc __rte_unused,
		  struct mlx5_wqe_dseg *__rte_restrict dseg,
		  uint8_t *buf,
		  unsigned int len,
		  unsigned int olx __rte_unused)
{
	unsigned int part;
	uint8_t *pdst;

	if (!MLX5_TXOFF_CONFIG(MPW)) {
		/* Store the descriptor byte counter for eMPW sessions. */
		dseg->bcount = rte_cpu_to_be_32(len | MLX5_ETH_WQE_DATA_INLINE);
		pdst = &dseg->inline_data[0];
	} else {
		/* The entire legacy MPW session counter is stored on close. */
		pdst = (uint8_t *)dseg;
	}
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, len);
	do {
		rte_memcpy(pdst, buf, part);
		len -= part;
		if (likely(!len)) {
			pdst += part;
			if (!MLX5_TXOFF_CONFIG(MPW))
				pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			/* Note: no final wraparound check here. */
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		buf += part;
		part = len;
	} while (true);
}

/**
 * Build the Data Segment of inlined data from single
 * segment packet with VLAN insertion.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dseg
 *   Pointer to the dseg fill with built Data Segment.
 * @param buf
 *   Data buffer to point.
 * @param len
 *   Data buffer length.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Pointer to the next Data Segment after inlined data.
 *   Ring buffer wraparound check is needed.
 */
static __rte_always_inline struct mlx5_wqe_dseg *
mlx5_tx_dseg_vlan(struct mlx5_txq_data *__rte_restrict txq,
		  struct mlx5_txq_local *__rte_restrict loc __rte_unused,
		  struct mlx5_wqe_dseg *__rte_restrict dseg,
		  uint8_t *buf,
		  unsigned int len,
		  unsigned int olx __rte_unused)

{
	unsigned int part;
	uint8_t *pdst;

	MLX5_ASSERT(len > MLX5_ESEG_MIN_INLINE_SIZE);
	static_assert(MLX5_DSEG_MIN_INLINE_SIZE ==
				 (2 * RTE_ETHER_ADDR_LEN),
		      "invalid Data Segment data size");
	if (!MLX5_TXOFF_CONFIG(MPW)) {
		/* Store the descriptor byte counter for eMPW sessions. */
		dseg->bcount = rte_cpu_to_be_32
				((len + sizeof(struct rte_vlan_hdr)) |
				 MLX5_ETH_WQE_DATA_INLINE);
		pdst = &dseg->inline_data[0];
	} else {
		/* The entire legacy MPW session counter is stored on close. */
		pdst = (uint8_t *)dseg;
	}
	memcpy(pdst, buf, MLX5_DSEG_MIN_INLINE_SIZE);
	buf += MLX5_DSEG_MIN_INLINE_SIZE;
	pdst += MLX5_DSEG_MIN_INLINE_SIZE;
	len -= MLX5_DSEG_MIN_INLINE_SIZE;
	/* Insert VLAN ethertype + VLAN tag. Pointer is aligned. */
	MLX5_ASSERT(pdst == RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE));
	if (unlikely(pdst >= (uint8_t *)txq->wqes_end))
		pdst = (uint8_t *)txq->wqes;
	*(uint32_t *)pdst = rte_cpu_to_be_32((RTE_ETHER_TYPE_VLAN << 16) |
					      loc->mbuf->vlan_tci);
	pdst += sizeof(struct rte_vlan_hdr);
	/*
	 * The WQEBB space availability is checked by caller.
	 * Here we should be aware of WQE ring buffer wraparound only.
	 */
	part = (uint8_t *)txq->wqes_end - pdst;
	part = RTE_MIN(part, len);
	do {
		rte_memcpy(pdst, buf, part);
		len -= part;
		if (likely(!len)) {
			pdst += part;
			if (!MLX5_TXOFF_CONFIG(MPW))
				pdst = RTE_PTR_ALIGN(pdst, MLX5_WSEG_SIZE);
			/* Note: no final wraparound check here. */
			return (struct mlx5_wqe_dseg *)pdst;
		}
		pdst = (uint8_t *)txq->wqes;
		buf += part;
		part = len;
	} while (true);
}

/**
 * Build the Ethernet Segment with optionally inlined data with
 * VLAN insertion and following Data Segments (if any) from
 * multi-segment packet. Used by ordinary send and TSO.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param wqe
 *   Pointer to WQE to fill with built Ethernet/Data Segments.
 * @param vlan
 *   Length of VLAN header to insert, 0 means no VLAN insertion.
 * @param inlen
 *   Data length to inline. For TSO this parameter specifies
 *   exact value, for ordinary send routine can be aligned by
 *   caller to provide better WQE space saving and data buffer
 *   start address alignment. This length includes VLAN header
 *   being inserted.
 * @param tso
 *   Zero means ordinary send, inlined data can be extended,
 *   otherwise this is TSO, inlined data length is fixed.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   Actual size of built WQE in segments.
 */
static __rte_always_inline unsigned int
mlx5_tx_mseg_build(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_txq_local *__rte_restrict loc,
		   struct mlx5_wqe *__rte_restrict wqe,
		   unsigned int vlan,
		   unsigned int inlen,
		   unsigned int tso,
		   unsigned int olx __rte_unused)
{
	struct mlx5_wqe_dseg *__rte_restrict dseg;
	unsigned int ds;

	MLX5_ASSERT((rte_pktmbuf_pkt_len(loc->mbuf) + vlan) >= inlen);
	loc->mbuf_nseg = NB_SEGS(loc->mbuf);
	loc->mbuf_off = 0;

	dseg = mlx5_tx_eseg_mdat(txq, loc, wqe, vlan, inlen, tso, olx);
	if (!loc->mbuf_nseg)
		goto dseg_done;
	/*
	 * There are still some mbuf remaining, not inlined.
	 * The first mbuf may be partially inlined and we
	 * must process the possible non-zero data offset.
	 */
	if (loc->mbuf_off) {
		unsigned int dlen;
		uint8_t *dptr;

		/*
		 * Exhausted packets must be dropped before.
		 * Non-zero offset means there are some data
		 * remained in the packet.
		 */
		MLX5_ASSERT(loc->mbuf_off < rte_pktmbuf_data_len(loc->mbuf));
		MLX5_ASSERT(rte_pktmbuf_data_len(loc->mbuf));
		dptr = rte_pktmbuf_mtod_offset(loc->mbuf, uint8_t *,
					       loc->mbuf_off);
		dlen = rte_pktmbuf_data_len(loc->mbuf) - loc->mbuf_off;
		/*
		 * Build the pointer/minimal data Data Segment.
		 * Do ring buffer wrapping check in advance.
		 */
		if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
			dseg = (struct mlx5_wqe_dseg *)txq->wqes;
		mlx5_tx_dseg_iptr(txq, loc, dseg, dptr, dlen, olx);
		/* Store the mbuf to be freed on completion. */
		MLX5_ASSERT(loc->elts_free);
		txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
		--loc->elts_free;
		++dseg;
		if (--loc->mbuf_nseg == 0)
			goto dseg_done;
		loc->mbuf = loc->mbuf->next;
		loc->mbuf_off = 0;
	}
	do {
		if (unlikely(!rte_pktmbuf_data_len(loc->mbuf))) {
			struct rte_mbuf *mbuf;

			/* Zero length segment found, just skip. */
			mbuf = loc->mbuf;
			loc->mbuf = loc->mbuf->next;
			rte_pktmbuf_free_seg(mbuf);
			if (--loc->mbuf_nseg == 0)
				break;
		} else {
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
			mlx5_tx_dseg_iptr
				(txq, loc, dseg,
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 rte_pktmbuf_data_len(loc->mbuf), olx);
			MLX5_ASSERT(loc->elts_free);
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			--loc->elts_free;
			++dseg;
			if (--loc->mbuf_nseg == 0)
				break;
			loc->mbuf = loc->mbuf->next;
		}
	} while (true);

dseg_done:
	/* Calculate actual segments used from the dseg pointer. */
	if ((uintptr_t)wqe < (uintptr_t)dseg)
		ds = ((uintptr_t)dseg - (uintptr_t)wqe) / MLX5_WSEG_SIZE;
	else
		ds = (((uintptr_t)dseg - (uintptr_t)wqe) +
		      txq->wqe_s * MLX5_WQE_SIZE) / MLX5_WSEG_SIZE;
	return ds;
}

/**
 * The routine checks timestamp flag in the current packet,
 * and push WAIT WQE into the queue if scheduling is required.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_SINGLE - continue processing with the packet.
 *   MLX5_TXCMP_CODE_MULTI - the WAIT inserted, continue processing.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_schedule_send(struct mlx5_txq_data *restrict txq,
		      struct mlx5_txq_local *restrict loc,
		      unsigned int olx)
{
	if (MLX5_TXOFF_CONFIG(TXPP) &&
	    loc->mbuf->ol_flags & txq->ts_mask) {
		struct mlx5_wqe *wqe;
		uint64_t ts;
		int32_t wci;

		/*
		 * Estimate the required space quickly and roughly.
		 * We would like to ensure the packet can be pushed
		 * to the queue and we won't get the orphan WAIT WQE.
		 */
		if (loc->wqe_free <= MLX5_WQE_SIZE_MAX / MLX5_WQE_SIZE ||
		    loc->elts_free < NB_SEGS(loc->mbuf))
			return MLX5_TXCMP_CODE_EXIT;
		/* Convert the timestamp into completion to wait. */
		ts = *RTE_MBUF_DYNFIELD(loc->mbuf, txq->ts_offset, uint64_t *);
		wci = mlx5_txpp_convert_tx_ts(txq->sh, ts);
		if (unlikely(wci < 0))
			return MLX5_TXCMP_CODE_SINGLE;
		/* Build the WAIT WQE with specified completion. */
		wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		mlx5_tx_cseg_init(txq, loc, wqe, 2, MLX5_OPCODE_WAIT, olx);
		mlx5_tx_wseg_init(txq, loc, wqe, wci, olx);
		++txq->wqe_ci;
		--loc->wqe_free;
		return MLX5_TXCMP_CODE_MULTI;
	}
	return MLX5_TXCMP_CODE_SINGLE;
}

/**
 * Tx one packet function for multi-segment TSO. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_TSO to build WQEs,
 * sends one packet per WQE.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_packet_multi_tso(struct mlx5_txq_data *__rte_restrict txq,
			struct mlx5_txq_local *__rte_restrict loc,
			unsigned int olx)
{
	struct mlx5_wqe *__rte_restrict wqe;
	unsigned int ds, dlen, inlen, ntcp, vlan = 0;

	if (MLX5_TXOFF_CONFIG(TXPP)) {
		enum mlx5_txcmp_code wret;

		/* Generate WAIT for scheduling if requested. */
		wret = mlx5_tx_schedule_send(txq, loc, olx);
		if (wret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (wret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
	}
	/*
	 * Calculate data length to be inlined to estimate
	 * the required space in WQE ring buffer.
	 */
	dlen = rte_pktmbuf_pkt_len(loc->mbuf);
	if (MLX5_TXOFF_CONFIG(VLAN) && loc->mbuf->ol_flags & PKT_TX_VLAN_PKT)
		vlan = sizeof(struct rte_vlan_hdr);
	inlen = loc->mbuf->l2_len + vlan +
		loc->mbuf->l3_len + loc->mbuf->l4_len;
	if (unlikely((!inlen || !loc->mbuf->tso_segsz)))
		return MLX5_TXCMP_CODE_ERROR;
	if (loc->mbuf->ol_flags & PKT_TX_TUNNEL_MASK)
		inlen += loc->mbuf->outer_l2_len + loc->mbuf->outer_l3_len;
	/* Packet must contain all TSO headers. */
	if (unlikely(inlen > MLX5_MAX_TSO_HEADER ||
		     inlen <= MLX5_ESEG_MIN_INLINE_SIZE ||
		     inlen > (dlen + vlan)))
		return MLX5_TXCMP_CODE_ERROR;
	MLX5_ASSERT(inlen >= txq->inlen_mode);
	/*
	 * Check whether there are enough free WQEBBs:
	 * - Control Segment
	 * - Ethernet Segment
	 * - First Segment of inlined Ethernet data
	 * - ... data continued ...
	 * - Data Segments of pointer/min inline type
	 */
	ds = NB_SEGS(loc->mbuf) + 2 + (inlen -
				       MLX5_ESEG_MIN_INLINE_SIZE +
				       MLX5_WSEG_SIZE +
				       MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
	if (unlikely(loc->wqe_free < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_EXIT;
	/* Check for maximal WQE size. */
	if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_ERROR;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes/packets counters. */
	ntcp = (dlen - (inlen - vlan) + loc->mbuf->tso_segsz - 1) /
		loc->mbuf->tso_segsz;
	/*
	 * One will be added for mbuf itself
	 * at the end of the mlx5_tx_burst from
	 * loc->pkts_sent field.
	 */
	--ntcp;
	txq->stats.opackets += ntcp;
	txq->stats.obytes += dlen + vlan + ntcp * inlen;
#endif
	wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
	loc->wqe_last = wqe;
	mlx5_tx_cseg_init(txq, loc, wqe, 0, MLX5_OPCODE_TSO, olx);
	ds = mlx5_tx_mseg_build(txq, loc, wqe, vlan, inlen, 1, olx);
	wqe->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
	return MLX5_TXCMP_CODE_MULTI;
}

/**
 * Tx one packet function for multi-segment SEND. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_SEND to build WQEs,
 * sends one packet per WQE, without any data inlining in
 * Ethernet Segment.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_packet_multi_send(struct mlx5_txq_data *__rte_restrict txq,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	struct mlx5_wqe_dseg *__rte_restrict dseg;
	struct mlx5_wqe *__rte_restrict wqe;
	unsigned int ds, nseg;

	MLX5_ASSERT(NB_SEGS(loc->mbuf) > 1);
	if (MLX5_TXOFF_CONFIG(TXPP)) {
		enum mlx5_txcmp_code wret;

		/* Generate WAIT for scheduling if requested. */
		wret = mlx5_tx_schedule_send(txq, loc, olx);
		if (wret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (wret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
	}
	/*
	 * No inline at all, it means the CPU cycles saving
	 * is prioritized at configuration, we should not
	 * copy any packet data to WQE.
	 */
	nseg = NB_SEGS(loc->mbuf);
	ds = 2 + nseg;
	if (unlikely(loc->wqe_free < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_EXIT;
	/* Check for maximal WQE size. */
	if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_ERROR;
	/*
	 * Some Tx offloads may cause an error if
	 * packet is not long enough, check against
	 * assumed minimal length.
	 */
	if (rte_pktmbuf_pkt_len(loc->mbuf) <= MLX5_ESEG_MIN_INLINE_SIZE)
		return MLX5_TXCMP_CODE_ERROR;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes counter. */
	txq->stats.obytes += rte_pktmbuf_pkt_len(loc->mbuf);
	if (MLX5_TXOFF_CONFIG(VLAN) &&
	    loc->mbuf->ol_flags & PKT_TX_VLAN_PKT)
		txq->stats.obytes += sizeof(struct rte_vlan_hdr);
#endif
	/*
	 * SEND WQE, one WQEBB:
	 * - Control Segment, SEND opcode
	 * - Ethernet Segment, optional VLAN, no inline
	 * - Data Segments, pointer only type
	 */
	wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
	loc->wqe_last = wqe;
	mlx5_tx_cseg_init(txq, loc, wqe, ds, MLX5_OPCODE_SEND, olx);
	mlx5_tx_eseg_none(txq, loc, wqe, olx);
	dseg = &wqe->dseg[0];
	do {
		if (unlikely(!rte_pktmbuf_data_len(loc->mbuf))) {
			struct rte_mbuf *mbuf;

			/*
			 * Zero length segment found, have to
			 * correct total size of WQE in segments.
			 * It is supposed to be rare occasion, so
			 * in normal case (no zero length segments)
			 * we avoid extra writing to the Control
			 * Segment.
			 */
			--ds;
			wqe->cseg.sq_ds -= RTE_BE32(1);
			mbuf = loc->mbuf;
			loc->mbuf = mbuf->next;
			rte_pktmbuf_free_seg(mbuf);
			if (--nseg == 0)
				break;
		} else {
			mlx5_tx_dseg_ptr
				(txq, loc, dseg,
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 rte_pktmbuf_data_len(loc->mbuf), olx);
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			--loc->elts_free;
			if (--nseg == 0)
				break;
			++dseg;
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
			loc->mbuf = loc->mbuf->next;
		}
	} while (true);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
	return MLX5_TXCMP_CODE_MULTI;
}

/**
 * Tx one packet function for multi-segment SEND. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_SEND to build WQEs,
 * sends one packet per WQE, with data inlining in
 * Ethernet Segment and minimal Data Segments.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 * Local context variables partially updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_packet_multi_inline(struct mlx5_txq_data *__rte_restrict txq,
			    struct mlx5_txq_local *__rte_restrict loc,
			    unsigned int olx)
{
	struct mlx5_wqe *__rte_restrict wqe;
	unsigned int ds, inlen, dlen, vlan = 0;

	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
	MLX5_ASSERT(NB_SEGS(loc->mbuf) > 1);
	if (MLX5_TXOFF_CONFIG(TXPP)) {
		enum mlx5_txcmp_code wret;

		/* Generate WAIT for scheduling if requested. */
		wret = mlx5_tx_schedule_send(txq, loc, olx);
		if (wret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (wret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
	}
	/*
	 * First calculate data length to be inlined
	 * to estimate the required space for WQE.
	 */
	dlen = rte_pktmbuf_pkt_len(loc->mbuf);
	if (MLX5_TXOFF_CONFIG(VLAN) && loc->mbuf->ol_flags & PKT_TX_VLAN_PKT)
		vlan = sizeof(struct rte_vlan_hdr);
	inlen = dlen + vlan;
	/* Check against minimal length. */
	if (inlen <= MLX5_ESEG_MIN_INLINE_SIZE)
		return MLX5_TXCMP_CODE_ERROR;
	MLX5_ASSERT(txq->inlen_send >= MLX5_ESEG_MIN_INLINE_SIZE);
	if (inlen > txq->inlen_send ||
	    loc->mbuf->ol_flags & PKT_TX_DYNF_NOINLINE) {
		struct rte_mbuf *mbuf;
		unsigned int nxlen;
		uintptr_t start;

		mbuf = loc->mbuf;
		nxlen = rte_pktmbuf_data_len(mbuf);
		/*
		 * Packet length exceeds the allowed inline
		 * data length, check whether the minimal
		 * inlining is required.
		 */
		if (txq->inlen_mode) {
			MLX5_ASSERT(txq->inlen_mode >=
				    MLX5_ESEG_MIN_INLINE_SIZE);
			MLX5_ASSERT(txq->inlen_mode <= txq->inlen_send);
			inlen = txq->inlen_mode;
		} else if (vlan && !txq->vlan_en) {
			/*
			 * VLAN insertion is requested and hardware does not
			 * support the offload, will do with software inline.
			 */
			inlen = MLX5_ESEG_MIN_INLINE_SIZE;
		} else if (mbuf->ol_flags & PKT_TX_DYNF_NOINLINE ||
			   nxlen > txq->inlen_send) {
			return mlx5_tx_packet_multi_send(txq, loc, olx);
		} else {
			goto do_first;
		}
		/*
		 * Now we know the minimal amount of data is requested
		 * to inline. Check whether we should inline the buffers
		 * from the chain beginning to eliminate some mbufs.
		 */
		if (unlikely(nxlen <= txq->inlen_send)) {
			/* We can inline first mbuf at least. */
			if (nxlen < inlen) {
				unsigned int smlen;

				/* Scan mbufs till inlen filled. */
				do {
					smlen = nxlen;
					mbuf = NEXT(mbuf);
					MLX5_ASSERT(mbuf);
					nxlen = rte_pktmbuf_data_len(mbuf);
					nxlen += smlen;
				} while (unlikely(nxlen < inlen));
				if (unlikely(nxlen > txq->inlen_send)) {
					/* We cannot inline entire mbuf. */
					smlen = inlen - smlen;
					start = rte_pktmbuf_mtod_offset
						    (mbuf, uintptr_t, smlen);
					goto do_align;
				}
			}
do_first:
			do {
				inlen = nxlen;
				mbuf = NEXT(mbuf);
				/* There should be not end of packet. */
				MLX5_ASSERT(mbuf);
				nxlen = inlen + rte_pktmbuf_data_len(mbuf);
			} while (unlikely(nxlen < txq->inlen_send));
		}
		start = rte_pktmbuf_mtod(mbuf, uintptr_t);
		/*
		 * Check whether we can do inline to align start
		 * address of data buffer to cacheline.
		 */
do_align:
		start = (~start + 1) & (RTE_CACHE_LINE_SIZE - 1);
		if (unlikely(start)) {
			start += inlen;
			if (start <= txq->inlen_send)
				inlen = start;
		}
	}
	/*
	 * Check whether there are enough free WQEBBs:
	 * - Control Segment
	 * - Ethernet Segment
	 * - First Segment of inlined Ethernet data
	 * - ... data continued ...
	 * - Data Segments of pointer/min inline type
	 *
	 * Estimate the number of Data Segments conservatively,
	 * supposing no any mbufs is being freed during inlining.
	 */
	MLX5_ASSERT(inlen <= txq->inlen_send);
	ds = NB_SEGS(loc->mbuf) + 2 + (inlen -
				       MLX5_ESEG_MIN_INLINE_SIZE +
				       MLX5_WSEG_SIZE +
				       MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
	if (unlikely(loc->wqe_free < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_EXIT;
	/* Check for maximal WQE size. */
	if (unlikely((MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE) < ((ds + 3) / 4)))
		return MLX5_TXCMP_CODE_ERROR;
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes/packets counters. */
	txq->stats.obytes += dlen + vlan;
#endif
	wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
	loc->wqe_last = wqe;
	mlx5_tx_cseg_init(txq, loc, wqe, 0, MLX5_OPCODE_SEND, olx);
	ds = mlx5_tx_mseg_build(txq, loc, wqe, vlan, inlen, 0, olx);
	wqe->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
	return MLX5_TXCMP_CODE_MULTI;
}

/**
 * Tx burst function for multi-segment packets. Supports all
 * types of Tx offloads, uses MLX5_OPCODE_SEND/TSO to build WQEs,
 * sends one packet per WQE. Function stops sending if it
 * encounters the single-segment packet.
 *
 * This routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 *   MLX5_TXCMP_CODE_SINGLE - single-segment packet encountered.
 *   MLX5_TXCMP_CODE_TSO - TSO single-segment packet encountered.
 * Local context variables updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_mseg(struct mlx5_txq_data *__rte_restrict txq,
		   struct rte_mbuf **__rte_restrict pkts,
		   unsigned int pkts_n,
		   struct mlx5_txq_local *__rte_restrict loc,
		   unsigned int olx)
{
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		enum mlx5_txcmp_code ret;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) > 1);
		/*
		 * Estimate the number of free elts quickly but
		 * conservatively. Some segment may be fully inlined
		 * and freed, ignore this here - precise estimation
		 * is costly.
		 */
		if (loc->elts_free < NB_SEGS(loc->mbuf))
			return MLX5_TXCMP_CODE_EXIT;
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    unlikely(loc->mbuf->ol_flags & PKT_TX_TCP_SEG)) {
			/* Proceed with multi-segment TSO. */
			ret = mlx5_tx_packet_multi_tso(txq, loc, olx);
		} else if (MLX5_TXOFF_CONFIG(INLINE)) {
			/* Proceed with multi-segment SEND with inlining. */
			ret = mlx5_tx_packet_multi_inline(txq, loc, olx);
		} else {
			/* Proceed with multi-segment SEND w/o inlining. */
			ret = mlx5_tx_packet_multi_send(txq, loc, olx);
		}
		if (ret == MLX5_TXCMP_CODE_EXIT)
			return MLX5_TXCMP_CODE_EXIT;
		if (ret == MLX5_TXCMP_CODE_ERROR)
			return MLX5_TXCMP_CODE_ERROR;
		/* WQE is built, go to the next packet. */
		++loc->pkts_sent;
		--pkts_n;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		if (likely(NB_SEGS(loc->mbuf) > 1))
			continue;
		/* Here ends the series of multi-segment packets. */
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    unlikely(loc->mbuf->ol_flags & PKT_TX_TCP_SEG))
			return MLX5_TXCMP_CODE_TSO;
		return MLX5_TXCMP_CODE_SINGLE;
	}
	MLX5_ASSERT(false);
}

/**
 * Tx burst function for single-segment packets with TSO.
 * Supports all types of Tx offloads, except multi-packets.
 * Uses MLX5_OPCODE_TSO to build WQEs, sends one packet per WQE.
 * Function stops sending if it encounters the multi-segment
 * packet or packet without TSO requested.
 *
 * The routine is responsible for storing processed mbuf
 * into elts ring buffer and update elts_head if inline
 * offloads is requested due to possible early freeing
 * of the inlined mbufs (can not store pkts array in elts
 * as a batch).
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 *   MLX5_TXCMP_CODE_SINGLE - single-segment packet encountered.
 *   MLX5_TXCMP_CODE_MULTI - multi-segment packet encountered.
 * Local context variables updated.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_tso(struct mlx5_txq_data *__rte_restrict txq,
		  struct rte_mbuf **__rte_restrict pkts,
		  unsigned int pkts_n,
		  struct mlx5_txq_local *__rte_restrict loc,
		  unsigned int olx)
{
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe_dseg *__rte_restrict dseg;
		struct mlx5_wqe *__rte_restrict wqe;
		unsigned int ds, dlen, hlen, ntcp, vlan = 0;
		uint8_t *dptr;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		dlen = rte_pktmbuf_data_len(loc->mbuf);
		if (MLX5_TXOFF_CONFIG(VLAN) &&
		    loc->mbuf->ol_flags & PKT_TX_VLAN_PKT) {
			vlan = sizeof(struct rte_vlan_hdr);
		}
		/*
		 * First calculate the WQE size to check
		 * whether we have enough space in ring buffer.
		 */
		hlen = loc->mbuf->l2_len + vlan +
		       loc->mbuf->l3_len + loc->mbuf->l4_len;
		if (unlikely((!hlen || !loc->mbuf->tso_segsz)))
			return MLX5_TXCMP_CODE_ERROR;
		if (loc->mbuf->ol_flags & PKT_TX_TUNNEL_MASK)
			hlen += loc->mbuf->outer_l2_len +
				loc->mbuf->outer_l3_len;
		/* Segment must contain all TSO headers. */
		if (unlikely(hlen > MLX5_MAX_TSO_HEADER ||
			     hlen <= MLX5_ESEG_MIN_INLINE_SIZE ||
			     hlen > (dlen + vlan)))
			return MLX5_TXCMP_CODE_ERROR;
		/*
		 * Check whether there are enough free WQEBBs:
		 * - Control Segment
		 * - Ethernet Segment
		 * - First Segment of inlined Ethernet data
		 * - ... data continued ...
		 * - Finishing Data Segment of pointer type
		 */
		ds = 4 + (hlen - MLX5_ESEG_MIN_INLINE_SIZE +
			  MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
		if (loc->wqe_free < ((ds + 3) / 4))
			return MLX5_TXCMP_CODE_EXIT;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Update sent data bytes/packets counters. */
		ntcp = (dlen + vlan - hlen +
			loc->mbuf->tso_segsz - 1) /
			loc->mbuf->tso_segsz;
		/*
		 * One will be added for mbuf itself at the end
		 * of the mlx5_tx_burst from loc->pkts_sent field.
		 */
		--ntcp;
		txq->stats.opackets += ntcp;
		txq->stats.obytes += dlen + vlan + ntcp * hlen;
#endif
		/*
		 * Build the TSO WQE:
		 * - Control Segment
		 * - Ethernet Segment with hlen bytes inlined
		 * - Data Segment of pointer type
		 */
		wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		loc->wqe_last = wqe;
		mlx5_tx_cseg_init(txq, loc, wqe, ds,
				  MLX5_OPCODE_TSO, olx);
		dseg = mlx5_tx_eseg_data(txq, loc, wqe, vlan, hlen, 1, olx);
		dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *) + hlen - vlan;
		dlen -= hlen - vlan;
		mlx5_tx_dseg_ptr(txq, loc, dseg, dptr, dlen, olx);
		/*
		 * WQE is built, update the loop parameters
		 * and go to the next packet.
		 */
		txq->wqe_ci += (ds + 3) / 4;
		loc->wqe_free -= (ds + 3) / 4;
		if (MLX5_TXOFF_CONFIG(INLINE))
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
		--loc->elts_free;
		++loc->pkts_sent;
		--pkts_n;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		if (MLX5_TXOFF_CONFIG(MULTI) &&
		    unlikely(NB_SEGS(loc->mbuf) > 1))
			return MLX5_TXCMP_CODE_MULTI;
		if (likely(!(loc->mbuf->ol_flags & PKT_TX_TCP_SEG)))
			return MLX5_TXCMP_CODE_SINGLE;
		/* Continue with the next TSO packet. */
	}
	MLX5_ASSERT(false);
}

/**
 * Analyze the packet and select the best method to send.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 * @param newp
 *   The predefined flag whether do complete check for
 *   multi-segment packets and TSO.
 *
 * @return
 *  MLX5_TXCMP_CODE_MULTI - multi-segment packet encountered.
 *  MLX5_TXCMP_CODE_TSO - TSO required, use TSO/LSO.
 *  MLX5_TXCMP_CODE_SINGLE - single-segment packet, use SEND.
 *  MLX5_TXCMP_CODE_EMPW - single-segment packet, use MPW.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_able_to_empw(struct mlx5_txq_data *__rte_restrict txq,
		     struct mlx5_txq_local *__rte_restrict loc,
		     unsigned int olx,
		     bool newp)
{
	/* Check for multi-segment packet. */
	if (newp &&
	    MLX5_TXOFF_CONFIG(MULTI) &&
	    unlikely(NB_SEGS(loc->mbuf) > 1))
		return MLX5_TXCMP_CODE_MULTI;
	/* Check for TSO packet. */
	if (newp &&
	    MLX5_TXOFF_CONFIG(TSO) &&
	    unlikely(loc->mbuf->ol_flags & PKT_TX_TCP_SEG))
		return MLX5_TXCMP_CODE_TSO;
	/* Check if eMPW is enabled at all. */
	if (!MLX5_TXOFF_CONFIG(EMPW))
		return MLX5_TXCMP_CODE_SINGLE;
	/* Check if eMPW can be engaged. */
	if (MLX5_TXOFF_CONFIG(VLAN) &&
	    unlikely(loc->mbuf->ol_flags & PKT_TX_VLAN_PKT) &&
		(!MLX5_TXOFF_CONFIG(INLINE) ||
		 unlikely((rte_pktmbuf_data_len(loc->mbuf) +
			   sizeof(struct rte_vlan_hdr)) > txq->inlen_empw))) {
		/*
		 * eMPW does not support VLAN insertion offload,
		 * we have to inline the entire packet but
		 * packet is too long for inlining.
		 */
		return MLX5_TXCMP_CODE_SINGLE;
	}
	return MLX5_TXCMP_CODE_EMPW;
}

/**
 * Check the next packet attributes to match with the eMPW batch ones.
 * In addition, for legacy MPW the packet length is checked either.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param es
 *   Pointer to Ethernet Segment of eMPW batch.
 * @param loc
 *   Pointer to burst routine local context.
 * @param dlen
 *   Length of previous packet in MPW descriptor.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *  true - packet match with eMPW batch attributes.
 *  false - no match, eMPW should be restarted.
 */
static __rte_always_inline bool
mlx5_tx_match_empw(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_wqe_eseg *__rte_restrict es,
		   struct mlx5_txq_local *__rte_restrict loc,
		   uint32_t dlen,
		   unsigned int olx)
{
	uint8_t swp_flags = 0;

	/* Compare the checksum flags, if any. */
	if (MLX5_TXOFF_CONFIG(CSUM) &&
	    txq_ol_cksum_to_cs(loc->mbuf) != es->cs_flags)
		return false;
	/* Compare the Software Parser offsets and flags. */
	if (MLX5_TXOFF_CONFIG(SWP) &&
	    (es->swp_offs != txq_mbuf_to_swp(loc, &swp_flags, olx) ||
	     es->swp_flags != swp_flags))
		return false;
	/* Fill metadata field if needed. */
	if (MLX5_TXOFF_CONFIG(METADATA) &&
		es->metadata != (loc->mbuf->ol_flags & PKT_TX_DYNF_METADATA ?
				 *RTE_FLOW_DYNF_METADATA(loc->mbuf) : 0))
		return false;
	/* Legacy MPW can send packets with the same lengt only. */
	if (MLX5_TXOFF_CONFIG(MPW) &&
	    dlen != rte_pktmbuf_data_len(loc->mbuf))
		return false;
	/* There must be no VLAN packets in eMPW loop. */
	if (MLX5_TXOFF_CONFIG(VLAN))
		MLX5_ASSERT(!(loc->mbuf->ol_flags & PKT_TX_VLAN_PKT));
	/* Check if the scheduling is requested. */
	if (MLX5_TXOFF_CONFIG(TXPP) &&
	    loc->mbuf->ol_flags & txq->ts_mask)
		return false;
	return true;
}

/*
 * Update send loop variables and WQE for eMPW loop
 * without data inlining. Number of Data Segments is
 * equal to the number of sent packets.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param ds
 *   Number of packets/Data Segments/Packets.
 * @param slen
 *   Accumulated statistics, bytes sent
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *  true - packet match with eMPW batch attributes.
 *  false - no match, eMPW should be restarted.
 */
static __rte_always_inline void
mlx5_tx_sdone_empw(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_txq_local *__rte_restrict loc,
		   unsigned int ds,
		   unsigned int slen,
		   unsigned int olx __rte_unused)
{
	MLX5_ASSERT(!MLX5_TXOFF_CONFIG(INLINE));
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes counter. */
	 txq->stats.obytes += slen;
#else
	(void)slen;
#endif
	loc->elts_free -= ds;
	loc->pkts_sent += ds;
	ds += 2;
	loc->wqe_last->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | ds);
	txq->wqe_ci += (ds + 3) / 4;
	loc->wqe_free -= (ds + 3) / 4;
}

/*
 * Update send loop variables and WQE for eMPW loop
 * with data inlining. Gets the size of pushed descriptors
 * and data to the WQE.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param loc
 *   Pointer to burst routine local context.
 * @param len
 *   Total size of descriptor/data in bytes.
 * @param slen
 *   Accumulated statistics, data bytes sent.
 * @param wqem
 *   The base WQE for the eMPW/MPW descriptor.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *  true - packet match with eMPW batch attributes.
 *  false - no match, eMPW should be restarted.
 */
static __rte_always_inline void
mlx5_tx_idone_empw(struct mlx5_txq_data *__rte_restrict txq,
		   struct mlx5_txq_local *__rte_restrict loc,
		   unsigned int len,
		   unsigned int slen,
		   struct mlx5_wqe *__rte_restrict wqem,
		   unsigned int olx __rte_unused)
{
	struct mlx5_wqe_dseg *dseg = &wqem->dseg[0];

	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Update sent data bytes counter. */
	 txq->stats.obytes += slen;
#else
	(void)slen;
#endif
	if (MLX5_TXOFF_CONFIG(MPW) && dseg->bcount == RTE_BE32(0)) {
		/*
		 * If the legacy MPW session contains the inline packets
		 * we should set the only inline data segment length
		 * and align the total length to the segment size.
		 */
		MLX5_ASSERT(len > sizeof(dseg->bcount));
		dseg->bcount = rte_cpu_to_be_32((len - sizeof(dseg->bcount)) |
						MLX5_ETH_WQE_DATA_INLINE);
		len = (len + MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE + 2;
	} else {
		/*
		 * The session is not legacy MPW or contains the
		 * data buffer pointer segments.
		 */
		MLX5_ASSERT((len % MLX5_WSEG_SIZE) == 0);
		len = len / MLX5_WSEG_SIZE + 2;
	}
	wqem->cseg.sq_ds = rte_cpu_to_be_32(txq->qp_num_8s | len);
	txq->wqe_ci += (len + 3) / 4;
	loc->wqe_free -= (len + 3) / 4;
	loc->wqe_last = wqem;
}

/**
 * The set of Tx burst functions for single-segment packets
 * without TSO and with Multi-Packet Writing feature support.
 * Supports all types of Tx offloads, except multi-packets
 * and TSO.
 *
 * Uses MLX5_OPCODE_EMPW to build WQEs if possible and sends
 * as many packet per WQE as it can. If eMPW is not configured
 * or packet can not be sent with eMPW (VLAN insertion) the
 * ordinary SEND opcode is used and only one packet placed
 * in WQE.
 *
 * Functions stop sending if it encounters the multi-segment
 * packet or packet with TSO requested.
 *
 * The routines are responsible for storing processed mbuf
 * into elts ring buffer and update elts_head if inlining
 * offload is requested. Otherwise the copying mbufs to elts
 * can be postponed and completed at the end of burst routine.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param loc
 *   Pointer to burst routine local context.
 * @param olx
 *   Configured Tx offloads mask. It is fully defined at
 *   compile time and may be used for optimization.
 *
 * @return
 *   MLX5_TXCMP_CODE_EXIT - sending is done or impossible.
 *   MLX5_TXCMP_CODE_ERROR - some unrecoverable error occurred.
 *   MLX5_TXCMP_CODE_MULTI - multi-segment packet encountered.
 *   MLX5_TXCMP_CODE_TSO - TSO packet encountered.
 *   MLX5_TXCMP_CODE_SINGLE - used inside functions set.
 *   MLX5_TXCMP_CODE_EMPW - used inside functions set.
 *
 * Local context variables updated.
 *
 *
 * The routine sends packets with MLX5_OPCODE_EMPW
 * without inlining, this is dedicated optimized branch.
 * No VLAN insertion is supported.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_empw_simple(struct mlx5_txq_data *__rte_restrict txq,
			  struct rte_mbuf **__rte_restrict pkts,
			  unsigned int pkts_n,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	/*
	 * Subroutine is the part of mlx5_tx_burst_single()
	 * and sends single-segment packet with eMPW opcode
	 * without data inlining.
	 */
	MLX5_ASSERT(!MLX5_TXOFF_CONFIG(INLINE));
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(EMPW));
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	static_assert(MLX5_EMPW_MIN_PACKETS >= 2, "invalid min size");
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe_dseg *__rte_restrict dseg;
		struct mlx5_wqe_eseg *__rte_restrict eseg;
		enum mlx5_txcmp_code ret;
		unsigned int part, loop;
		unsigned int slen = 0;

next_empw:
		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		part = RTE_MIN(pkts_n, MLX5_TXOFF_CONFIG(MPW) ?
				       MLX5_MPW_MAX_PACKETS :
				       MLX5_EMPW_MAX_PACKETS);
		if (unlikely(loc->elts_free < part)) {
			/* We have no enough elts to save all mbufs. */
			if (unlikely(loc->elts_free < MLX5_EMPW_MIN_PACKETS))
				return MLX5_TXCMP_CODE_EXIT;
			/* But we still able to send at least minimal eMPW. */
			part = loc->elts_free;
		}
		/* Check whether we have enough WQEs */
		if (unlikely(loc->wqe_free < ((2 + part + 3) / 4))) {
			if (unlikely(loc->wqe_free <
				((2 + MLX5_EMPW_MIN_PACKETS + 3) / 4)))
				return MLX5_TXCMP_CODE_EXIT;
			part = (loc->wqe_free * 4) - 2;
		}
		if (likely(part > 1))
			rte_prefetch0(*pkts);
		loc->wqe_last = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		/*
		 * Build eMPW title WQEBB:
		 * - Control Segment, eMPW opcode
		 * - Ethernet Segment, no inline
		 */
		mlx5_tx_cseg_init(txq, loc, loc->wqe_last, part + 2,
				  MLX5_OPCODE_ENHANCED_MPSW, olx);
		mlx5_tx_eseg_none(txq, loc, loc->wqe_last,
				  olx & ~MLX5_TXOFF_CONFIG_VLAN);
		eseg = &loc->wqe_last->eseg;
		dseg = &loc->wqe_last->dseg[0];
		loop = part;
		/* Store the packet length for legacy MPW. */
		if (MLX5_TXOFF_CONFIG(MPW))
			eseg->mss = rte_cpu_to_be_16
					(rte_pktmbuf_data_len(loc->mbuf));
		for (;;) {
			uint32_t dlen = rte_pktmbuf_data_len(loc->mbuf);
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			slen += dlen;
#endif
			mlx5_tx_dseg_ptr
				(txq, loc, dseg,
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 dlen, olx);
			if (unlikely(--loop == 0))
				break;
			loc->mbuf = *pkts++;
			if (likely(loop > 1))
				rte_prefetch0(*pkts);
			ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
			/*
			 * Unroll the completion code to avoid
			 * returning variable value - it results in
			 * unoptimized sequent checking in caller.
			 */
			if (ret == MLX5_TXCMP_CODE_MULTI) {
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_MULTI;
			}
			MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
			if (ret == MLX5_TXCMP_CODE_TSO) {
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_TSO;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE) {
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_SINGLE;
			}
			if (ret != MLX5_TXCMP_CODE_EMPW) {
				MLX5_ASSERT(false);
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				return MLX5_TXCMP_CODE_ERROR;
			}
			/*
			 * Check whether packet parameters coincide
			 * within assumed eMPW batch:
			 * - check sum settings
			 * - metadata value
			 * - software parser settings
			 * - packets length (legacy MPW only)
			 * - scheduling is not required
			 */
			if (!mlx5_tx_match_empw(txq, eseg, loc, dlen, olx)) {
				MLX5_ASSERT(loop);
				part -= loop;
				mlx5_tx_sdone_empw(txq, loc, part, slen, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				pkts_n -= part;
				goto next_empw;
			}
			/* Packet attributes match, continue the same eMPW. */
			++dseg;
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
		}
		/* eMPW is built successfully, update loop parameters. */
		MLX5_ASSERT(!loop);
		MLX5_ASSERT(pkts_n >= part);
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Update sent data bytes counter. */
		txq->stats.obytes += slen;
#endif
		loc->elts_free -= part;
		loc->pkts_sent += part;
		txq->wqe_ci += (2 + part + 3) / 4;
		loc->wqe_free -= (2 + part + 3) / 4;
		pkts_n -= part;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
		if (unlikely(ret != MLX5_TXCMP_CODE_EMPW))
			return ret;
		/* Continue sending eMPW batches. */
	}
	MLX5_ASSERT(false);
}

/**
 * The routine sends packets with MLX5_OPCODE_EMPW
 * with inlining, optionally supports VLAN insertion.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_empw_inline(struct mlx5_txq_data *__rte_restrict txq,
			  struct rte_mbuf **__rte_restrict pkts,
			  unsigned int pkts_n,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	/*
	 * Subroutine is the part of mlx5_tx_burst_single()
	 * and sends single-segment packet with eMPW opcode
	 * with data inlining.
	 */
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(EMPW));
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	static_assert(MLX5_EMPW_MIN_PACKETS >= 2, "invalid min size");
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe_dseg *__rte_restrict dseg;
		struct mlx5_wqe *__rte_restrict wqem;
		enum mlx5_txcmp_code ret;
		unsigned int room, part, nlim;
		unsigned int slen = 0;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		/*
		 * Limits the amount of packets in one WQE
		 * to improve CQE latency generation.
		 */
		nlim = RTE_MIN(pkts_n, MLX5_TXOFF_CONFIG(MPW) ?
				       MLX5_MPW_INLINE_MAX_PACKETS :
				       MLX5_EMPW_MAX_PACKETS);
		/* Check whether we have minimal amount WQEs */
		if (unlikely(loc->wqe_free <
			    ((2 + MLX5_EMPW_MIN_PACKETS + 3) / 4)))
			return MLX5_TXCMP_CODE_EXIT;
		if (likely(pkts_n > 1))
			rte_prefetch0(*pkts);
		wqem = txq->wqes + (txq->wqe_ci & txq->wqe_m);
		/*
		 * Build eMPW title WQEBB:
		 * - Control Segment, eMPW opcode, zero DS
		 * - Ethernet Segment, no inline
		 */
		mlx5_tx_cseg_init(txq, loc, wqem, 0,
				  MLX5_OPCODE_ENHANCED_MPSW, olx);
		mlx5_tx_eseg_none(txq, loc, wqem,
				  olx & ~MLX5_TXOFF_CONFIG_VLAN);
		dseg = &wqem->dseg[0];
		/* Store the packet length for legacy MPW. */
		if (MLX5_TXOFF_CONFIG(MPW))
			wqem->eseg.mss = rte_cpu_to_be_16
					 (rte_pktmbuf_data_len(loc->mbuf));
		room = RTE_MIN(MLX5_WQE_SIZE_MAX / MLX5_WQE_SIZE,
			       loc->wqe_free) * MLX5_WQE_SIZE -
					MLX5_WQE_CSEG_SIZE -
					MLX5_WQE_ESEG_SIZE;
		/* Limit the room for legacy MPW sessions for performance. */
		if (MLX5_TXOFF_CONFIG(MPW))
			room = RTE_MIN(room,
				       RTE_MAX(txq->inlen_empw +
					       sizeof(dseg->bcount) +
					       (MLX5_TXOFF_CONFIG(VLAN) ?
					       sizeof(struct rte_vlan_hdr) : 0),
					       MLX5_MPW_INLINE_MAX_PACKETS *
					       MLX5_WQE_DSEG_SIZE));
		/* Build WQE till we have space, packets and resources. */
		part = room;
		for (;;) {
			uint32_t dlen = rte_pktmbuf_data_len(loc->mbuf);
			uint8_t *dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *);
			unsigned int tlen;

			MLX5_ASSERT(room >= MLX5_WQE_DSEG_SIZE);
			MLX5_ASSERT((room % MLX5_WQE_DSEG_SIZE) == 0);
			MLX5_ASSERT((uintptr_t)dseg < (uintptr_t)txq->wqes_end);
			/*
			 * Some Tx offloads may cause an error if
			 * packet is not long enough, check against
			 * assumed minimal length.
			 */
			if (unlikely(dlen <= MLX5_ESEG_MIN_INLINE_SIZE)) {
				part -= room;
				if (unlikely(!part))
					return MLX5_TXCMP_CODE_ERROR;
				/*
				 * We have some successfully built
				 * packet Data Segments to send.
				 */
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				return MLX5_TXCMP_CODE_ERROR;
			}
			/* Inline or not inline - that's the Question. */
			if (dlen > txq->inlen_empw ||
			    loc->mbuf->ol_flags & PKT_TX_DYNF_NOINLINE)
				goto pointer_empw;
			if (MLX5_TXOFF_CONFIG(MPW)) {
				if (dlen > txq->inlen_send)
					goto pointer_empw;
				tlen = dlen;
				if (part == room) {
					/* Open new inline MPW session. */
					tlen += sizeof(dseg->bcount);
					dseg->bcount = RTE_BE32(0);
					dseg = RTE_PTR_ADD
						(dseg, sizeof(dseg->bcount));
				} else {
					/*
					 * No pointer and inline descriptor
					 * intermix for legacy MPW sessions.
					 */
					if (wqem->dseg[0].bcount)
						break;
				}
			} else {
				tlen = sizeof(dseg->bcount) + dlen;
			}
			/* Inline entire packet, optional VLAN insertion. */
			if (MLX5_TXOFF_CONFIG(VLAN) &&
			    loc->mbuf->ol_flags & PKT_TX_VLAN_PKT) {
				/*
				 * The packet length must be checked in
				 * mlx5_tx_able_to_empw() and packet
				 * fits into inline length guaranteed.
				 */
				MLX5_ASSERT((dlen +
					     sizeof(struct rte_vlan_hdr)) <=
					    txq->inlen_empw);
				tlen += sizeof(struct rte_vlan_hdr);
				if (room < tlen)
					break;
				dseg = mlx5_tx_dseg_vlan(txq, loc, dseg,
							 dptr, dlen, olx);
#ifdef MLX5_PMD_SOFT_COUNTERS
				/* Update sent data bytes counter. */
				slen +=	sizeof(struct rte_vlan_hdr);
#endif
			} else {
				if (room < tlen)
					break;
				dseg = mlx5_tx_dseg_empw(txq, loc, dseg,
							 dptr, dlen, olx);
			}
			if (!MLX5_TXOFF_CONFIG(MPW))
				tlen = RTE_ALIGN(tlen, MLX5_WSEG_SIZE);
			MLX5_ASSERT(room >= tlen);
			room -= tlen;
			/*
			 * Packet data are completely inlined,
			 * free the packet immediately.
			 */
			rte_pktmbuf_free_seg(loc->mbuf);
			goto next_mbuf;
pointer_empw:
			/*
			 * No pointer and inline descriptor
			 * intermix for legacy MPW sessions.
			 */
			if (MLX5_TXOFF_CONFIG(MPW) &&
			    part != room &&
			    wqem->dseg[0].bcount == RTE_BE32(0))
				break;
			/*
			 * Not inlinable VLAN packets are
			 * proceeded outside of this routine.
			 */
			MLX5_ASSERT(room >= MLX5_WQE_DSEG_SIZE);
			if (MLX5_TXOFF_CONFIG(VLAN))
				MLX5_ASSERT(!(loc->mbuf->ol_flags &
					    PKT_TX_VLAN_PKT));
			mlx5_tx_dseg_ptr(txq, loc, dseg, dptr, dlen, olx);
			/* We have to store mbuf in elts.*/
			txq->elts[txq->elts_head++ & txq->elts_m] = loc->mbuf;
			room -= MLX5_WQE_DSEG_SIZE;
			/* Ring buffer wraparound is checked at the loop end.*/
			++dseg;
next_mbuf:
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			slen += dlen;
#endif
			loc->pkts_sent++;
			loc->elts_free--;
			pkts_n--;
			if (unlikely(!pkts_n || !loc->elts_free)) {
				/*
				 * We have no resources/packets to
				 * continue build descriptors.
				 */
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				return MLX5_TXCMP_CODE_EXIT;
			}
			loc->mbuf = *pkts++;
			if (likely(pkts_n > 1))
				rte_prefetch0(*pkts);
			ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
			/*
			 * Unroll the completion code to avoid
			 * returning variable value - it results in
			 * unoptimized sequent checking in caller.
			 */
			if (ret == MLX5_TXCMP_CODE_MULTI) {
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_MULTI;
			}
			MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
			if (ret == MLX5_TXCMP_CODE_TSO) {
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_TSO;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE) {
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				if (unlikely(!loc->elts_free ||
					     !loc->wqe_free))
					return MLX5_TXCMP_CODE_EXIT;
				return MLX5_TXCMP_CODE_SINGLE;
			}
			if (ret != MLX5_TXCMP_CODE_EMPW) {
				MLX5_ASSERT(false);
				part -= room;
				mlx5_tx_idone_empw(txq, loc, part,
						   slen, wqem, olx);
				return MLX5_TXCMP_CODE_ERROR;
			}
			/* Check if we have minimal room left. */
			nlim--;
			if (unlikely(!nlim || room < MLX5_WQE_DSEG_SIZE))
				break;
			/*
			 * Check whether packet parameters coincide
			 * within assumed eMPW batch:
			 * - check sum settings
			 * - metadata value
			 * - software parser settings
			 * - packets length (legacy MPW only)
			 * - scheduling is not required
			 */
			if (!mlx5_tx_match_empw(txq, &wqem->eseg,
						loc, dlen, olx))
				break;
			/* Packet attributes match, continue the same eMPW. */
			if ((uintptr_t)dseg >= (uintptr_t)txq->wqes_end)
				dseg = (struct mlx5_wqe_dseg *)txq->wqes;
		}
		/*
		 * We get here to close an existing eMPW
		 * session and start the new one.
		 */
		MLX5_ASSERT(pkts_n);
		part -= room;
		if (unlikely(!part))
			return MLX5_TXCMP_CODE_EXIT;
		mlx5_tx_idone_empw(txq, loc, part, slen, wqem, olx);
		if (unlikely(!loc->elts_free ||
			     !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		/* Continue the loop with new eMPW session. */
	}
	MLX5_ASSERT(false);
}

/**
 * The routine sends packets with ordinary MLX5_OPCODE_SEND.
 * Data inlining and VLAN insertion are supported.
 */
static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_single_send(struct mlx5_txq_data *__rte_restrict txq,
			  struct rte_mbuf **__rte_restrict pkts,
			  unsigned int pkts_n,
			  struct mlx5_txq_local *__rte_restrict loc,
			  unsigned int olx)
{
	/*
	 * Subroutine is the part of mlx5_tx_burst_single()
	 * and sends single-segment packet with SEND opcode.
	 */
	MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	MLX5_ASSERT(pkts_n > loc->pkts_sent);
	pkts += loc->pkts_sent + 1;
	pkts_n -= loc->pkts_sent;
	for (;;) {
		struct mlx5_wqe *__rte_restrict wqe;
		enum mlx5_txcmp_code ret;

		MLX5_ASSERT(NB_SEGS(loc->mbuf) == 1);
		if (MLX5_TXOFF_CONFIG(TXPP)) {
			enum mlx5_txcmp_code wret;

			/* Generate WAIT for scheduling if requested. */
			wret = mlx5_tx_schedule_send(txq, loc, olx);
			if (wret == MLX5_TXCMP_CODE_EXIT)
				return MLX5_TXCMP_CODE_EXIT;
			if (wret == MLX5_TXCMP_CODE_ERROR)
				return MLX5_TXCMP_CODE_ERROR;
		}
		if (MLX5_TXOFF_CONFIG(INLINE)) {
			unsigned int inlen, vlan = 0;

			inlen = rte_pktmbuf_data_len(loc->mbuf);
			if (MLX5_TXOFF_CONFIG(VLAN) &&
			    loc->mbuf->ol_flags & PKT_TX_VLAN_PKT) {
				vlan = sizeof(struct rte_vlan_hdr);
				inlen += vlan;
				static_assert((sizeof(struct rte_vlan_hdr) +
					       sizeof(struct rte_ether_hdr)) ==
					       MLX5_ESEG_MIN_INLINE_SIZE,
					       "invalid min inline data size");
			}
			/*
			 * If inlining is enabled at configuration time
			 * the limit must be not less than minimal size.
			 * Otherwise we would do extra check for data
			 * size to avoid crashes due to length overflow.
			 */
			MLX5_ASSERT(txq->inlen_send >=
				    MLX5_ESEG_MIN_INLINE_SIZE);
			if (inlen <= txq->inlen_send) {
				unsigned int seg_n, wqe_n;

				rte_prefetch0(rte_pktmbuf_mtod
						(loc->mbuf, uint8_t *));
				/* Check against minimal length. */
				if (inlen <= MLX5_ESEG_MIN_INLINE_SIZE)
					return MLX5_TXCMP_CODE_ERROR;
				if (loc->mbuf->ol_flags &
				    PKT_TX_DYNF_NOINLINE) {
					/*
					 * The hint flag not to inline packet
					 * data is set. Check whether we can
					 * follow the hint.
					 */
					if ((!MLX5_TXOFF_CONFIG(EMPW) &&
					      txq->inlen_mode) ||
					    (MLX5_TXOFF_CONFIG(MPW) &&
					     txq->inlen_mode)) {
						if (inlen <= txq->inlen_send)
							goto single_inline;
						/*
						 * The hardware requires the
						 * minimal inline data header.
						 */
						goto single_min_inline;
					}
					if (MLX5_TXOFF_CONFIG(VLAN) &&
					    vlan && !txq->vlan_en) {
						/*
						 * We must insert VLAN tag
						 * by software means.
						 */
						goto single_part_inline;
					}
					goto single_no_inline;
				}
single_inline:
				/*
				 * Completely inlined packet data WQE:
				 * - Control Segment, SEND opcode
				 * - Ethernet Segment, no VLAN insertion
				 * - Data inlined, VLAN optionally inserted
				 * - Alignment to MLX5_WSEG_SIZE
				 * Have to estimate amount of WQEBBs
				 */
				seg_n = (inlen + 3 * MLX5_WSEG_SIZE -
					 MLX5_ESEG_MIN_INLINE_SIZE +
					 MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
				/* Check if there are enough WQEBBs. */
				wqe_n = (seg_n + 3) / 4;
				if (wqe_n > loc->wqe_free)
					return MLX5_TXCMP_CODE_EXIT;
				wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
				loc->wqe_last = wqe;
				mlx5_tx_cseg_init(txq, loc, wqe, seg_n,
						  MLX5_OPCODE_SEND, olx);
				mlx5_tx_eseg_data(txq, loc, wqe,
						  vlan, inlen, 0, olx);
				txq->wqe_ci += wqe_n;
				loc->wqe_free -= wqe_n;
				/*
				 * Packet data are completely inlined,
				 * free the packet immediately.
				 */
				rte_pktmbuf_free_seg(loc->mbuf);
			} else if ((!MLX5_TXOFF_CONFIG(EMPW) ||
				     MLX5_TXOFF_CONFIG(MPW)) &&
					txq->inlen_mode) {
				/*
				 * If minimal inlining is requested the eMPW
				 * feature should be disabled due to data is
				 * inlined into Ethernet Segment, which can
				 * not contain inlined data for eMPW due to
				 * segment shared for all packets.
				 */
				struct mlx5_wqe_dseg *__rte_restrict dseg;
				unsigned int ds;
				uint8_t *dptr;

				/*
				 * The inline-mode settings require
				 * to inline the specified amount of
				 * data bytes to the Ethernet Segment.
				 * We should check the free space in
				 * WQE ring buffer to inline partially.
				 */
single_min_inline:
				MLX5_ASSERT(txq->inlen_send >= txq->inlen_mode);
				MLX5_ASSERT(inlen > txq->inlen_mode);
				MLX5_ASSERT(txq->inlen_mode >=
					    MLX5_ESEG_MIN_INLINE_SIZE);
				/*
				 * Check whether there are enough free WQEBBs:
				 * - Control Segment
				 * - Ethernet Segment
				 * - First Segment of inlined Ethernet data
				 * - ... data continued ...
				 * - Finishing Data Segment of pointer type
				 */
				ds = (MLX5_WQE_CSEG_SIZE +
				      MLX5_WQE_ESEG_SIZE +
				      MLX5_WQE_DSEG_SIZE +
				      txq->inlen_mode -
				      MLX5_ESEG_MIN_INLINE_SIZE +
				      MLX5_WQE_DSEG_SIZE +
				      MLX5_WSEG_SIZE - 1) / MLX5_WSEG_SIZE;
				if (loc->wqe_free < ((ds + 3) / 4))
					return MLX5_TXCMP_CODE_EXIT;
				/*
				 * Build the ordinary SEND WQE:
				 * - Control Segment
				 * - Ethernet Segment, inline inlen_mode bytes
				 * - Data Segment of pointer type
				 */
				wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
				loc->wqe_last = wqe;
				mlx5_tx_cseg_init(txq, loc, wqe, ds,
						  MLX5_OPCODE_SEND, olx);
				dseg = mlx5_tx_eseg_data(txq, loc, wqe, vlan,
							 txq->inlen_mode,
							 0, olx);
				dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *) +
				       txq->inlen_mode - vlan;
				inlen -= txq->inlen_mode;
				mlx5_tx_dseg_ptr(txq, loc, dseg,
						 dptr, inlen, olx);
				/*
				 * WQE is built, update the loop parameters
				 * and got to the next packet.
				 */
				txq->wqe_ci += (ds + 3) / 4;
				loc->wqe_free -= (ds + 3) / 4;
				/* We have to store mbuf in elts.*/
				MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
				txq->elts[txq->elts_head++ & txq->elts_m] =
						loc->mbuf;
				--loc->elts_free;
			} else {
				uint8_t *dptr;
				unsigned int dlen;

				/*
				 * Partially inlined packet data WQE, we have
				 * some space in title WQEBB, we can fill it
				 * with some packet data. It takes one WQEBB,
				 * it is available, no extra space check:
				 * - Control Segment, SEND opcode
				 * - Ethernet Segment, no VLAN insertion
				 * - MLX5_ESEG_MIN_INLINE_SIZE bytes of Data
				 * - Data Segment, pointer type
				 *
				 * We also get here if VLAN insertion is not
				 * supported by HW, the inline is enabled.
				 */
single_part_inline:
				wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
				loc->wqe_last = wqe;
				mlx5_tx_cseg_init(txq, loc, wqe, 4,
						  MLX5_OPCODE_SEND, olx);
				mlx5_tx_eseg_dmin(txq, loc, wqe, vlan, olx);
				dptr = rte_pktmbuf_mtod(loc->mbuf, uint8_t *) +
				       MLX5_ESEG_MIN_INLINE_SIZE - vlan;
				/*
				 * The length check is performed above, by
				 * comparing with txq->inlen_send. We should
				 * not get overflow here.
				 */
				MLX5_ASSERT(inlen > MLX5_ESEG_MIN_INLINE_SIZE);
				dlen = inlen - MLX5_ESEG_MIN_INLINE_SIZE;
				mlx5_tx_dseg_ptr(txq, loc, &wqe->dseg[1],
						 dptr, dlen, olx);
				++txq->wqe_ci;
				--loc->wqe_free;
				/* We have to store mbuf in elts.*/
				MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE));
				txq->elts[txq->elts_head++ & txq->elts_m] =
						loc->mbuf;
				--loc->elts_free;
			}
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			txq->stats.obytes += vlan +
					rte_pktmbuf_data_len(loc->mbuf);
#endif
		} else {
			/*
			 * No inline at all, it means the CPU cycles saving
			 * is prioritized at configuration, we should not
			 * copy any packet data to WQE.
			 *
			 * SEND WQE, one WQEBB:
			 * - Control Segment, SEND opcode
			 * - Ethernet Segment, optional VLAN, no inline
			 * - Data Segment, pointer type
			 */
single_no_inline:
			wqe = txq->wqes + (txq->wqe_ci & txq->wqe_m);
			loc->wqe_last = wqe;
			mlx5_tx_cseg_init(txq, loc, wqe, 3,
					  MLX5_OPCODE_SEND, olx);
			mlx5_tx_eseg_none(txq, loc, wqe, olx);
			mlx5_tx_dseg_ptr
				(txq, loc, &wqe->dseg[0],
				 rte_pktmbuf_mtod(loc->mbuf, uint8_t *),
				 rte_pktmbuf_data_len(loc->mbuf), olx);
			++txq->wqe_ci;
			--loc->wqe_free;
			/*
			 * We should not store mbuf pointer in elts
			 * if no inlining is configured, this is done
			 * by calling routine in a batch copy.
			 */
			MLX5_ASSERT(!MLX5_TXOFF_CONFIG(INLINE));
			--loc->elts_free;
#ifdef MLX5_PMD_SOFT_COUNTERS
			/* Update sent data bytes counter. */
			txq->stats.obytes += rte_pktmbuf_data_len(loc->mbuf);
			if (MLX5_TXOFF_CONFIG(VLAN) &&
			    loc->mbuf->ol_flags & PKT_TX_VLAN_PKT)
				txq->stats.obytes +=
					sizeof(struct rte_vlan_hdr);
#endif
		}
		++loc->pkts_sent;
		--pkts_n;
		if (unlikely(!pkts_n || !loc->elts_free || !loc->wqe_free))
			return MLX5_TXCMP_CODE_EXIT;
		loc->mbuf = *pkts++;
		if (pkts_n > 1)
			rte_prefetch0(*pkts);
		ret = mlx5_tx_able_to_empw(txq, loc, olx, true);
		if (unlikely(ret != MLX5_TXCMP_CODE_SINGLE))
			return ret;
	}
	MLX5_ASSERT(false);
}

static __rte_always_inline enum mlx5_txcmp_code
mlx5_tx_burst_single(struct mlx5_txq_data *__rte_restrict txq,
		     struct rte_mbuf **__rte_restrict pkts,
		     unsigned int pkts_n,
		     struct mlx5_txq_local *__rte_restrict loc,
		     unsigned int olx)
{
	enum mlx5_txcmp_code ret;

	ret = mlx5_tx_able_to_empw(txq, loc, olx, false);
	if (ret == MLX5_TXCMP_CODE_SINGLE)
		goto ordinary_send;
	MLX5_ASSERT(ret == MLX5_TXCMP_CODE_EMPW);
	for (;;) {
		/* Optimize for inline/no inline eMPW send. */
		ret = (MLX5_TXOFF_CONFIG(INLINE)) ?
			mlx5_tx_burst_empw_inline
				(txq, pkts, pkts_n, loc, olx) :
			mlx5_tx_burst_empw_simple
				(txq, pkts, pkts_n, loc, olx);
		if (ret != MLX5_TXCMP_CODE_SINGLE)
			return ret;
		/* The resources to send one packet should remain. */
		MLX5_ASSERT(loc->elts_free && loc->wqe_free);
ordinary_send:
		ret = mlx5_tx_burst_single_send(txq, pkts, pkts_n, loc, olx);
		MLX5_ASSERT(ret != MLX5_TXCMP_CODE_SINGLE);
		if (ret != MLX5_TXCMP_CODE_EMPW)
			return ret;
		/* The resources to send one packet should remain. */
		MLX5_ASSERT(loc->elts_free && loc->wqe_free);
	}
}

/**
 * DPDK Tx callback template. This is configured template
 * used to generate routines optimized for specified offload setup.
 * One of this generated functions is chosen at SQ configuration
 * time.
 *
 * @param txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 * @param olx
 *   Configured offloads mask, presents the bits of MLX5_TXOFF_CONFIG_xxx
 *   values. Should be static to take compile time static configuration
 *   advantages.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static __rte_always_inline uint16_t
mlx5_tx_burst_tmpl(struct mlx5_txq_data *__rte_restrict txq,
		   struct rte_mbuf **__rte_restrict pkts,
		   uint16_t pkts_n,
		   unsigned int olx)
{
	struct mlx5_txq_local loc;
	enum mlx5_txcmp_code ret;
	unsigned int part;

	MLX5_ASSERT(txq->elts_s >= (uint16_t)(txq->elts_head - txq->elts_tail));
	MLX5_ASSERT(txq->wqe_s >= (uint16_t)(txq->wqe_ci - txq->wqe_pi));
	if (unlikely(!pkts_n))
		return 0;
	loc.pkts_sent = 0;
	loc.pkts_copy = 0;
	loc.wqe_last = NULL;

send_loop:
	loc.pkts_loop = loc.pkts_sent;
	/*
	 * Check if there are some CQEs, if any:
	 * - process an encountered errors
	 * - process the completed WQEs
	 * - free related mbufs
	 * - doorbell the NIC about processed CQEs
	 */
	rte_prefetch0(*(pkts + loc.pkts_sent));
	mlx5_tx_handle_completion(txq, olx);
	/*
	 * Calculate the number of available resources - elts and WQEs.
	 * There are two possible different scenarios:
	 * - no data inlining into WQEs, one WQEBB may contains up to
	 *   four packets, in this case elts become scarce resource
	 * - data inlining into WQEs, one packet may require multiple
	 *   WQEBBs, the WQEs become the limiting factor.
	 */
	MLX5_ASSERT(txq->elts_s >= (uint16_t)(txq->elts_head - txq->elts_tail));
	loc.elts_free = txq->elts_s -
				(uint16_t)(txq->elts_head - txq->elts_tail);
	MLX5_ASSERT(txq->wqe_s >= (uint16_t)(txq->wqe_ci - txq->wqe_pi));
	loc.wqe_free = txq->wqe_s -
				(uint16_t)(txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!loc.elts_free || !loc.wqe_free))
		goto burst_exit;
	for (;;) {
		/*
		 * Fetch the packet from array. Usually this is
		 * the first packet in series of multi/single
		 * segment packets.
		 */
		loc.mbuf = *(pkts + loc.pkts_sent);
		/* Dedicated branch for multi-segment packets. */
		if (MLX5_TXOFF_CONFIG(MULTI) &&
		    unlikely(NB_SEGS(loc.mbuf) > 1)) {
			/*
			 * Multi-segment packet encountered.
			 * Hardware is able to process it only
			 * with SEND/TSO opcodes, one packet
			 * per WQE, do it in dedicated routine.
			 */
enter_send_multi:
			MLX5_ASSERT(loc.pkts_sent >= loc.pkts_copy);
			part = loc.pkts_sent - loc.pkts_copy;
			if (!MLX5_TXOFF_CONFIG(INLINE) && part) {
				/*
				 * There are some single-segment mbufs not
				 * stored in elts. The mbufs must be in the
				 * same order as WQEs, so we must copy the
				 * mbufs to elts here, before the coming
				 * multi-segment packet mbufs is appended.
				 */
				mlx5_tx_copy_elts(txq, pkts + loc.pkts_copy,
						  part, olx);
				loc.pkts_copy = loc.pkts_sent;
			}
			MLX5_ASSERT(pkts_n > loc.pkts_sent);
			ret = mlx5_tx_burst_mseg(txq, pkts, pkts_n, &loc, olx);
			if (!MLX5_TXOFF_CONFIG(INLINE))
				loc.pkts_copy = loc.pkts_sent;
			/*
			 * These returned code checks are supposed
			 * to be optimized out due to routine inlining.
			 */
			if (ret == MLX5_TXCMP_CODE_EXIT) {
				/*
				 * The routine returns this code when
				 * all packets are sent or there is no
				 * enough resources to complete request.
				 */
				break;
			}
			if (ret == MLX5_TXCMP_CODE_ERROR) {
				/*
				 * The routine returns this code when
				 * some error in the incoming packets
				 * format occurred.
				 */
				txq->stats.oerrors++;
				break;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE) {
				/*
				 * The single-segment packet was encountered
				 * in the array, try to send it with the
				 * best optimized way, possible engaging eMPW.
				 */
				goto enter_send_single;
			}
			if (MLX5_TXOFF_CONFIG(TSO) &&
			    ret == MLX5_TXCMP_CODE_TSO) {
				/*
				 * The single-segment TSO packet was
				 * encountered in the array.
				 */
				goto enter_send_tso;
			}
			/* We must not get here. Something is going wrong. */
			MLX5_ASSERT(false);
			txq->stats.oerrors++;
			break;
		}
		/* Dedicated branch for single-segment TSO packets. */
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    unlikely(loc.mbuf->ol_flags & PKT_TX_TCP_SEG)) {
			/*
			 * TSO might require special way for inlining
			 * (dedicated parameters) and is sent with
			 * MLX5_OPCODE_TSO opcode only, provide this
			 * in dedicated branch.
			 */
enter_send_tso:
			MLX5_ASSERT(NB_SEGS(loc.mbuf) == 1);
			MLX5_ASSERT(pkts_n > loc.pkts_sent);
			ret = mlx5_tx_burst_tso(txq, pkts, pkts_n, &loc, olx);
			/*
			 * These returned code checks are supposed
			 * to be optimized out due to routine inlining.
			 */
			if (ret == MLX5_TXCMP_CODE_EXIT)
				break;
			if (ret == MLX5_TXCMP_CODE_ERROR) {
				txq->stats.oerrors++;
				break;
			}
			if (ret == MLX5_TXCMP_CODE_SINGLE)
				goto enter_send_single;
			if (MLX5_TXOFF_CONFIG(MULTI) &&
			    ret == MLX5_TXCMP_CODE_MULTI) {
				/*
				 * The multi-segment packet was
				 * encountered in the array.
				 */
				goto enter_send_multi;
			}
			/* We must not get here. Something is going wrong. */
			MLX5_ASSERT(false);
			txq->stats.oerrors++;
			break;
		}
		/*
		 * The dedicated branch for the single-segment packets
		 * without TSO. Often these ones can be sent using
		 * MLX5_OPCODE_EMPW with multiple packets in one WQE.
		 * The routine builds the WQEs till it encounters
		 * the TSO or multi-segment packet (in case if these
		 * offloads are requested at SQ configuration time).
		 */
enter_send_single:
		MLX5_ASSERT(pkts_n > loc.pkts_sent);
		ret = mlx5_tx_burst_single(txq, pkts, pkts_n, &loc, olx);
		/*
		 * These returned code checks are supposed
		 * to be optimized out due to routine inlining.
		 */
		if (ret == MLX5_TXCMP_CODE_EXIT)
			break;
		if (ret == MLX5_TXCMP_CODE_ERROR) {
			txq->stats.oerrors++;
			break;
		}
		if (MLX5_TXOFF_CONFIG(MULTI) &&
		    ret == MLX5_TXCMP_CODE_MULTI) {
			/*
			 * The multi-segment packet was
			 * encountered in the array.
			 */
			goto enter_send_multi;
		}
		if (MLX5_TXOFF_CONFIG(TSO) &&
		    ret == MLX5_TXCMP_CODE_TSO) {
			/*
			 * The single-segment TSO packet was
			 * encountered in the array.
			 */
			goto enter_send_tso;
		}
		/* We must not get here. Something is going wrong. */
		MLX5_ASSERT(false);
		txq->stats.oerrors++;
		break;
	}
	/*
	 * Main Tx loop is completed, do the rest:
	 * - set completion request if thresholds are reached
	 * - doorbell the hardware
	 * - copy the rest of mbufs to elts (if any)
	 */
	MLX5_ASSERT(MLX5_TXOFF_CONFIG(INLINE) ||
		    loc.pkts_sent >= loc.pkts_copy);
	/* Take a shortcut if nothing is sent. */
	if (unlikely(loc.pkts_sent == loc.pkts_loop))
		goto burst_exit;
	/* Request CQE generation if limits are reached. */
	mlx5_tx_request_completion(txq, &loc, olx);
	/*
	 * Ring QP doorbell immediately after WQE building completion
	 * to improve latencies. The pure software related data treatment
	 * can be completed after doorbell. Tx CQEs for this SQ are
	 * processed in this thread only by the polling.
	 *
	 * The rdma core library can map doorbell register in two ways,
	 * depending on the environment variable "MLX5_SHUT_UP_BF":
	 *
	 * - as regular cached memory, the variable is either missing or
	 *   set to zero. This type of mapping may cause the significant
	 *   doorbell register writing latency and requires explicit
	 *   memory write barrier to mitigate this issue and prevent
	 *   write combining.
	 *
	 * - as non-cached memory, the variable is present and set to
	 *   not "0" value. This type of mapping may cause performance
	 *   impact under heavy loading conditions but the explicit write
	 *   memory barrier is not required and it may improve core
	 *   performance.
	 *
	 * - the legacy behaviour (prior 19.08 release) was to use some
	 *   heuristics to decide whether write memory barrier should
	 *   be performed. This behavior is supported with specifying
	 *   tx_db_nc=2, write barrier is skipped if application
	 *   provides the full recommended burst of packets, it
	 *   supposes the next packets are coming and the write barrier
	 *   will be issued on the next burst (after descriptor writing,
	 *   at least).
	 */
	mlx5_tx_dbrec_cond_wmb(txq, loc.wqe_last, !txq->db_nc &&
			(!txq->db_heu || pkts_n % MLX5_TX_DEFAULT_BURST));
	/* Not all of the mbufs may be stored into elts yet. */
	part = MLX5_TXOFF_CONFIG(INLINE) ? 0 : loc.pkts_sent - loc.pkts_copy;
	if (!MLX5_TXOFF_CONFIG(INLINE) && part) {
		/*
		 * There are some single-segment mbufs not stored in elts.
		 * It can be only if the last packet was single-segment.
		 * The copying is gathered into one place due to it is
		 * a good opportunity to optimize that with SIMD.
		 * Unfortunately if inlining is enabled the gaps in
		 * pointer array may happen due to early freeing of the
		 * inlined mbufs.
		 */
		mlx5_tx_copy_elts(txq, pkts + loc.pkts_copy, part, olx);
		loc.pkts_copy = loc.pkts_sent;
	}
	MLX5_ASSERT(txq->elts_s >= (uint16_t)(txq->elts_head - txq->elts_tail));
	MLX5_ASSERT(txq->wqe_s >= (uint16_t)(txq->wqe_ci - txq->wqe_pi));
	if (pkts_n > loc.pkts_sent) {
		/*
		 * If burst size is large there might be no enough CQE
		 * fetched from completion queue and no enough resources
		 * freed to send all the packets.
		 */
		goto send_loop;
	}
burst_exit:
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += loc.pkts_sent;
#endif
	return loc.pkts_sent;
}

/* Generate routines with Enhanced Multi-Packet Write support. */
MLX5_TXOFF_DECL(full_empw,
		MLX5_TXOFF_CONFIG_FULL | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(none_empw,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(md_empw,
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mt_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mtsc_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mti_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mtv_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mtiv_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(sc_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(sci_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(scv_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(sciv_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(i_empw,
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(v_empw,
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(iv_empw,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

/* Generate routines without Enhanced Multi-Packet Write support. */
MLX5_TXOFF_DECL(full,
		MLX5_TXOFF_CONFIG_FULL)

MLX5_TXOFF_DECL(none,
		MLX5_TXOFF_CONFIG_NONE)

MLX5_TXOFF_DECL(md,
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(mt,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(mtsc,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(mti,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA)


MLX5_TXOFF_DECL(mtv,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)


MLX5_TXOFF_DECL(mtiv,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(sc,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(sci,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA)


MLX5_TXOFF_DECL(scv,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)


MLX5_TXOFF_DECL(sciv,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(i,
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(v,
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_DECL(iv,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

/* Generate routines with timestamp scheduling. */
MLX5_TXOFF_DECL(full_ts_nompw,
		MLX5_TXOFF_CONFIG_FULL | MLX5_TXOFF_CONFIG_TXPP)

MLX5_TXOFF_DECL(full_ts_nompwi,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP)

MLX5_TXOFF_DECL(full_ts,
		MLX5_TXOFF_CONFIG_FULL | MLX5_TXOFF_CONFIG_TXPP |
		MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(full_ts_noi,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(none_ts,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_TXPP |
		MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mdi_ts,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mti_ts,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_DECL(mtiv_ts,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_TXPP |
		MLX5_TXOFF_CONFIG_EMPW)

/*
 * Generate routines with Legacy Multi-Packet Write support.
 * This mode is supported by ConnectX-4 Lx only and imposes
 * offload limitations, not supported:
 *   - ACL/Flows (metadata are becoming meaningless)
 *   - WQE Inline headers
 *   - SRIOV (E-Switch offloads)
 *   - VLAN insertion
 *   - tunnel encapsulation/decapsulation
 *   - TSO
 */
MLX5_TXOFF_DECL(none_mpw,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_DECL(mci_mpw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_DECL(mc_mpw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_EMPW | MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_DECL(i_mpw,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

/*
 * Array of declared and compiled Tx burst function and corresponding
 * supported offloads set. The array is used to select the Tx burst
 * function for specified offloads set at Tx queue configuration time.
 */
const struct {
	eth_tx_burst_t func;
	unsigned int olx;
} txoff_func[] = {
MLX5_TXOFF_INFO(full_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(none_empw,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(md_empw,
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mt_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mtsc_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mti_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mtv_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mtiv_empw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(sc_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(sci_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(scv_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(sciv_empw,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(i_empw,
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(v_empw,
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(iv_empw,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(full_ts_nompw,
		MLX5_TXOFF_CONFIG_FULL | MLX5_TXOFF_CONFIG_TXPP)

MLX5_TXOFF_INFO(full_ts_nompwi,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP)

MLX5_TXOFF_INFO(full_ts,
		MLX5_TXOFF_CONFIG_FULL | MLX5_TXOFF_CONFIG_TXPP |
		MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(full_ts_noi,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(none_ts,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_TXPP |
		MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mdi_ts,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mti_ts,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_METADATA |
		MLX5_TXOFF_CONFIG_TXPP | MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(mtiv_ts,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA | MLX5_TXOFF_CONFIG_TXPP |
		MLX5_TXOFF_CONFIG_EMPW)

MLX5_TXOFF_INFO(full,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(none,
		MLX5_TXOFF_CONFIG_NONE)

MLX5_TXOFF_INFO(md,
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(mt,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(mtsc,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(mti,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(mtv,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(mtiv,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_TSO |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(sc,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(sci,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(scv,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(sciv,
		MLX5_TXOFF_CONFIG_SWP |	MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(i,
		MLX5_TXOFF_CONFIG_INLINE |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(v,
		MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(iv,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_VLAN |
		MLX5_TXOFF_CONFIG_METADATA)

MLX5_TXOFF_INFO(none_mpw,
		MLX5_TXOFF_CONFIG_NONE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_INFO(mci_mpw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_INFO(mc_mpw,
		MLX5_TXOFF_CONFIG_MULTI | MLX5_TXOFF_CONFIG_CSUM |
		MLX5_TXOFF_CONFIG_EMPW | MLX5_TXOFF_CONFIG_MPW)

MLX5_TXOFF_INFO(i_mpw,
		MLX5_TXOFF_CONFIG_INLINE | MLX5_TXOFF_CONFIG_EMPW |
		MLX5_TXOFF_CONFIG_MPW)
};

/**
 * Configure the Tx function to use. The routine checks configured
 * Tx offloads for the device and selects appropriate Tx burst
 * routine. There are multiple Tx burst routines compiled from
 * the same template in the most optimal way for the dedicated
 * Tx offloads set.
 *
 * @param dev
 *   Pointer to private data structure.
 *
 * @return
 *   Pointer to selected Tx burst function.
 */
eth_tx_burst_t
mlx5_select_tx_function(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	uint64_t tx_offloads = dev->data->dev_conf.txmode.offloads;
	unsigned int diff = 0, olx = 0, i, m;

	static_assert(MLX5_WQE_SIZE_MAX / MLX5_WSEG_SIZE <=
		      MLX5_DSEG_MAX, "invalid WQE max size");
	static_assert(MLX5_WQE_CSEG_SIZE == MLX5_WSEG_SIZE,
		      "invalid WQE Control Segment size");
	static_assert(MLX5_WQE_ESEG_SIZE == MLX5_WSEG_SIZE,
		      "invalid WQE Ethernet Segment size");
	static_assert(MLX5_WQE_DSEG_SIZE == MLX5_WSEG_SIZE,
		      "invalid WQE Data Segment size");
	static_assert(MLX5_WQE_SIZE == 4 * MLX5_WSEG_SIZE,
		      "invalid WQE size");
	MLX5_ASSERT(priv);
	if (tx_offloads & DEV_TX_OFFLOAD_MULTI_SEGS) {
		/* We should support Multi-Segment Packets. */
		olx |= MLX5_TXOFF_CONFIG_MULTI;
	}
	if (tx_offloads & (DEV_TX_OFFLOAD_TCP_TSO |
			   DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
			   DEV_TX_OFFLOAD_GRE_TNL_TSO |
			   DEV_TX_OFFLOAD_IP_TNL_TSO |
			   DEV_TX_OFFLOAD_UDP_TNL_TSO)) {
		/* We should support TCP Send Offload. */
		olx |= MLX5_TXOFF_CONFIG_TSO;
	}
	if (tx_offloads & (DEV_TX_OFFLOAD_IP_TNL_TSO |
			   DEV_TX_OFFLOAD_UDP_TNL_TSO |
			   DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM)) {
		/* We should support Software Parser for Tunnels. */
		olx |= MLX5_TXOFF_CONFIG_SWP;
	}
	if (tx_offloads & (DEV_TX_OFFLOAD_IPV4_CKSUM |
			   DEV_TX_OFFLOAD_UDP_CKSUM |
			   DEV_TX_OFFLOAD_TCP_CKSUM |
			   DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM)) {
		/* We should support IP/TCP/UDP Checksums. */
		olx |= MLX5_TXOFF_CONFIG_CSUM;
	}
	if (tx_offloads & DEV_TX_OFFLOAD_VLAN_INSERT) {
		/* We should support VLAN insertion. */
		olx |= MLX5_TXOFF_CONFIG_VLAN;
	}
	if (tx_offloads & DEV_TX_OFFLOAD_SEND_ON_TIMESTAMP &&
	    rte_mbuf_dynflag_lookup
			(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL) >= 0 &&
	    rte_mbuf_dynfield_lookup
			(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL) >= 0) {
		/* Offload configured, dynamic entities registered. */
		olx |= MLX5_TXOFF_CONFIG_TXPP;
	}
	if (priv->txqs_n && (*priv->txqs)[0]) {
		struct mlx5_txq_data *txd = (*priv->txqs)[0];

		if (txd->inlen_send) {
			/*
			 * Check the data inline requirements. Data inline
			 * is enabled on per device basis, we can check
			 * the first Tx queue only.
			 *
			 * If device does not support VLAN insertion in WQE
			 * and some queues are requested to perform VLAN
			 * insertion offload than inline must be enabled.
			 */
			olx |= MLX5_TXOFF_CONFIG_INLINE;
		}
	}
	if (config->mps == MLX5_MPW_ENHANCED &&
	    config->txq_inline_min <= 0) {
		/*
		 * The NIC supports Enhanced Multi-Packet Write
		 * and does not require minimal inline data.
		 */
		olx |= MLX5_TXOFF_CONFIG_EMPW;
	}
	if (rte_flow_dynf_metadata_avail()) {
		/* We should support Flow metadata. */
		olx |= MLX5_TXOFF_CONFIG_METADATA;
	}
	if (config->mps == MLX5_MPW) {
		/*
		 * The NIC supports Legacy Multi-Packet Write.
		 * The MLX5_TXOFF_CONFIG_MPW controls the
		 * descriptor building method in combination
		 * with MLX5_TXOFF_CONFIG_EMPW.
		 */
		if (!(olx & (MLX5_TXOFF_CONFIG_TSO |
			     MLX5_TXOFF_CONFIG_SWP |
			     MLX5_TXOFF_CONFIG_VLAN |
			     MLX5_TXOFF_CONFIG_METADATA)))
			olx |= MLX5_TXOFF_CONFIG_EMPW |
			       MLX5_TXOFF_CONFIG_MPW;
	}
	/*
	 * Scan the routines table to find the minimal
	 * satisfying routine with requested offloads.
	 */
	m = RTE_DIM(txoff_func);
	for (i = 0; i < RTE_DIM(txoff_func); i++) {
		unsigned int tmp;

		tmp = txoff_func[i].olx;
		if (tmp == olx) {
			/* Meets requested offloads exactly.*/
			m = i;
			break;
		}
		if ((tmp & olx) != olx) {
			/* Does not meet requested offloads at all. */
			continue;
		}
		if ((olx ^ tmp) & MLX5_TXOFF_CONFIG_MPW)
			/* Do not enable legacy MPW if not configured. */
			continue;
		if ((olx ^ tmp) & MLX5_TXOFF_CONFIG_EMPW)
			/* Do not enable eMPW if not configured. */
			continue;
		if ((olx ^ tmp) & MLX5_TXOFF_CONFIG_INLINE)
			/* Do not enable inlining if not configured. */
			continue;
		if ((olx ^ tmp) & MLX5_TXOFF_CONFIG_TXPP)
			/* Do not enable scheduling if not configured. */
			continue;
		/*
		 * Some routine meets the requirements.
		 * Check whether it has minimal amount
		 * of not requested offloads.
		 */
		tmp = __builtin_popcountl(tmp & ~olx);
		if (m >= RTE_DIM(txoff_func) || tmp < diff) {
			/* First or better match, save and continue. */
			m = i;
			diff = tmp;
			continue;
		}
		if (tmp == diff) {
			tmp = txoff_func[i].olx ^ txoff_func[m].olx;
			if (__builtin_ffsl(txoff_func[i].olx & ~tmp) <
			    __builtin_ffsl(txoff_func[m].olx & ~tmp)) {
				/* Lighter not requested offload. */
				m = i;
			}
		}
	}
	if (m >= RTE_DIM(txoff_func)) {
		DRV_LOG(DEBUG, "port %u has no selected Tx function"
			       " for requested offloads %04X",
				dev->data->port_id, olx);
		return NULL;
	}
	DRV_LOG(DEBUG, "port %u has selected Tx function"
		       " supporting offloads %04X/%04X",
			dev->data->port_id, olx, txoff_func[m].olx);
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_MULTI)
		DRV_LOG(DEBUG, "\tMULTI (multi segment)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_TSO)
		DRV_LOG(DEBUG, "\tTSO   (TCP send offload)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_SWP)
		DRV_LOG(DEBUG, "\tSWP   (software parser)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_CSUM)
		DRV_LOG(DEBUG, "\tCSUM  (checksum offload)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_INLINE)
		DRV_LOG(DEBUG, "\tINLIN (inline data)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_VLAN)
		DRV_LOG(DEBUG, "\tVLANI (VLAN insertion)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_METADATA)
		DRV_LOG(DEBUG, "\tMETAD (tx Flow metadata)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_TXPP)
		DRV_LOG(DEBUG, "\tMETAD (tx Scheduling)");
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_EMPW) {
		if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_MPW)
			DRV_LOG(DEBUG, "\tMPW   (Legacy MPW)");
		else
			DRV_LOG(DEBUG, "\tEMPW  (Enhanced MPW)");
	}
	return txoff_func[m].func;
}

/**
 * DPDK callback to get the TX queue information
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param tx_queue_id
 *   Tx queue identificator.
 *
 * @param qinfo
 *   Pointer to the TX queue information structure.
 *
 * @return
 *   None.
 */

void
mlx5_txq_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		  struct rte_eth_txq_info *qinfo)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq = (*priv->txqs)[tx_queue_id];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq, struct mlx5_txq_ctrl, txq);

	if (!txq)
		return;
	qinfo->nb_desc = txq->elts_s;
	qinfo->conf.tx_thresh.pthresh = 0;
	qinfo->conf.tx_thresh.hthresh = 0;
	qinfo->conf.tx_thresh.wthresh = 0;
	qinfo->conf.tx_rs_thresh = 0;
	qinfo->conf.tx_free_thresh = 0;
	qinfo->conf.tx_deferred_start = txq_ctrl ? 0 : 1;
	qinfo->conf.offloads = dev->data->dev_conf.txmode.offloads;
}

/**
 * DPDK callback to get the TX packet burst mode information
 *
 * @param dev
 *   Pointer to the device structure.
 *
 * @param tx_queue_id
 *   Tx queue identificatior.
 *
 * @param mode
 *   Pointer to the burts mode information.
 *
 * @return
 *   0 as success, -EINVAL as failure.
 */

int
mlx5_tx_burst_mode_get(struct rte_eth_dev *dev,
		       uint16_t tx_queue_id __rte_unused,
		       struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	unsigned int i, olx;

	for (i = 0; i < RTE_DIM(txoff_func); i++) {
		if (pkt_burst == txoff_func[i].func) {
			olx = txoff_func[i].olx;
			snprintf(mode->info, sizeof(mode->info),
				 "%s%s%s%s%s%s%s%s%s",
				 (olx & MLX5_TXOFF_CONFIG_EMPW) ?
				 ((olx & MLX5_TXOFF_CONFIG_MPW) ?
				 "Legacy MPW" : "Enhanced MPW") : "No MPW",
				 (olx & MLX5_TXOFF_CONFIG_MULTI) ?
				 " + MULTI" : "",
				 (olx & MLX5_TXOFF_CONFIG_TSO) ?
				 " + TSO" : "",
				 (olx & MLX5_TXOFF_CONFIG_SWP) ?
				 " + SWP" : "",
				 (olx & MLX5_TXOFF_CONFIG_CSUM) ?
				 "  + CSUM" : "",
				 (olx & MLX5_TXOFF_CONFIG_INLINE) ?
				 " + INLINE" : "",
				 (olx & MLX5_TXOFF_CONFIG_VLAN) ?
				 " + VLAN" : "",
				 (olx & MLX5_TXOFF_CONFIG_METADATA) ?
				 " + METADATA" : "",
				 (olx & MLX5_TXOFF_CONFIG_TXPP) ?
				 " + TXPP" : "");
			return 0;
		}
	}
	return -EINVAL;
}
