/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

static __rte_always_inline uint32_t
rxq_cq_to_pkt_type(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe);

static __rte_always_inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, volatile struct mlx5_mini_cqe8 **mcqe);

static __rte_always_inline uint32_t
rxq_cq_to_ol_flags(volatile struct mlx5_cqe *cqe);

static __rte_always_inline void
rxq_cq_to_mbuf(struct mlx5_rxq_data *rxq, struct rte_mbuf *pkt,
	       volatile struct mlx5_cqe *cqe, uint32_t rss_hash_res);

static __rte_always_inline void
mprq_buf_replace(struct mlx5_rxq_data *rxq, uint16_t rq_idx);

uint32_t mlx5_ptype_table[] __rte_cache_aligned = {
	[0xff] = RTE_PTYPE_ALL_MASK, /* Last entry for errored packet. */
};

uint8_t mlx5_cksum_table[1 << 10] __rte_cache_aligned;
uint8_t mlx5_swp_types_table[1 << 10] __rte_cache_aligned;

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
 * Return the size of tailroom of WQ.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param addr
 *   Pointer to tail of WQ.
 *
 * @return
 *   Size of tailroom.
 */
static inline size_t
tx_mlx5_wq_tailroom(struct mlx5_txq_data *txq, void *addr)
{
	size_t tailroom;
	tailroom = (uintptr_t)(txq->wqes) +
		   (1 << txq->wqe_n) * MLX5_WQE_SIZE -
		   (uintptr_t)addr;
	return tailroom;
}

/**
 * Copy data to tailroom of circular queue.
 *
 * @param dst
 *   Pointer to destination.
 * @param src
 *   Pointer to source.
 * @param n
 *   Number of bytes to copy.
 * @param base
 *   Pointer to head of queue.
 * @param tailroom
 *   Size of tailroom from dst.
 *
 * @return
 *   Pointer after copied data.
 */
static inline void *
mlx5_copy_to_wq(void *dst, const void *src, size_t n,
		void *base, size_t tailroom)
{
	void *ret;

	if (n > tailroom) {
		rte_memcpy(dst, src, tailroom);
		rte_memcpy(base, (void *)((uintptr_t)src + tailroom),
			   n - tailroom);
		ret = (uint8_t *)base + n - tailroom;
	} else {
		rte_memcpy(dst, src, n);
		ret = (n == tailroom) ? base : (uint8_t *)dst + n;
	}
	return ret;
}

/**
 * Inline TSO headers into WQE.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int
inline_tso(struct mlx5_txq_data *txq, struct rte_mbuf *buf,
	   uint32_t *length,
	   uintptr_t *addr,
	   uint16_t *pkt_inline_sz,
	   uint8_t **raw,
	   uint16_t *max_wqe,
	   uint16_t *tso_segsz,
	   uint16_t *tso_header_sz)
{
	uintptr_t end = (uintptr_t)(((uintptr_t)txq->wqes) +
				    (1 << txq->wqe_n) * MLX5_WQE_SIZE);
	unsigned int copy_b;
	uint8_t vlan_sz = (buf->ol_flags & PKT_TX_VLAN_PKT) ? 4 : 0;
	const uint8_t tunneled = txq->tunnel_en && (buf->ol_flags &
				 PKT_TX_TUNNEL_MASK);
	uint16_t n_wqe;

	*tso_segsz = buf->tso_segsz;
	*tso_header_sz = buf->l2_len + vlan_sz + buf->l3_len + buf->l4_len;
	if (unlikely(*tso_segsz == 0 || *tso_header_sz == 0)) {
		txq->stats.oerrors++;
		return -EINVAL;
	}
	if (tunneled)
		*tso_header_sz += buf->outer_l2_len + buf->outer_l3_len;
	/* First seg must contain all TSO headers. */
	if (unlikely(*tso_header_sz > MLX5_MAX_TSO_HEADER) ||
		     *tso_header_sz > DATA_LEN(buf)) {
		txq->stats.oerrors++;
		return -EINVAL;
	}
	copy_b = *tso_header_sz - *pkt_inline_sz;
	if (!copy_b || ((end - (uintptr_t)*raw) < copy_b))
		return -EAGAIN;
	n_wqe = (MLX5_WQE_DS(copy_b) - 1 + 3) / 4;
	if (unlikely(*max_wqe < n_wqe))
		return -EINVAL;
	*max_wqe -= n_wqe;
	rte_memcpy((void *)*raw, (void *)*addr, copy_b);
	*length -= copy_b;
	*addr += copy_b;
	copy_b = MLX5_WQE_DS(copy_b) * MLX5_WQE_DWORD_SIZE;
	*pkt_inline_sz += copy_b;
	*raw += copy_b;
	return 0;
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
	struct mlx5_txq_data *txq = tx_queue;
	uint16_t used;

	mlx5_tx_complete(txq);
	used = txq->elts_head - txq->elts_tail;
	if (offset < used)
		return RTE_ETH_TX_DESC_FULL;
	return RTE_ETH_TX_DESC_DONE;
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
	const unsigned int cqe_cnt = cqe_n - 1;
	unsigned int cq_ci;
	unsigned int used;

	/* if we are processing a compressed cqe */
	if (zip->ai) {
		used = zip->cqe_cnt - zip->ca;
		cq_ci = zip->cq_ci;
	} else {
		used = 0;
		cq_ci = rxq->cq_ci;
	}
	cqe = &(*rxq->cqes)[cq_ci & cqe_cnt];
	while (check_cqe(cqe, cqe_n, cq_ci) == 0) {
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
	used = RTE_MIN(used, (1U << rxq->elts_n) - 1);
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

	if (dev->rx_pkt_burst != mlx5_rx_burst) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (offset >= (1 << rxq->elts_n)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (offset < rx_queue_count(rxq))
		return RTE_ETH_RX_DESC_DONE;
	return RTE_ETH_RX_DESC_AVAIL;
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

	if (dev->rx_pkt_burst != mlx5_rx_burst) {
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

/**
 * DPDK callback for TX.
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
mlx5_tx_burst(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	uint16_t max_elts;
	uint16_t max_wqe;
	unsigned int comp;
	volatile struct mlx5_wqe_ctrl *last_wqe = NULL;
	unsigned int segs_n = 0;
	const unsigned int max_inline = txq->max_inline;
	uint64_t addr_64;

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(*pkts);
	/* Start processing. */
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		struct rte_mbuf *buf = *pkts; /* First_seg. */
		uint8_t *raw;
		volatile struct mlx5_wqe_v *wqe = NULL;
		volatile rte_v128u32_t *dseg = NULL;
		uint32_t length;
		unsigned int ds = 0;
		unsigned int sg = 0; /* counter of additional segs attached. */
		uintptr_t addr;
		uint16_t pkt_inline_sz = MLX5_WQE_DWORD_SIZE + 2;
		uint16_t tso_header_sz = 0;
		uint16_t ehdr;
		uint8_t cs_flags;
		uint8_t tso = txq->tso_en && (buf->ol_flags & PKT_TX_TCP_SEG);
		uint32_t swp_offsets = 0;
		uint8_t swp_types = 0;
		rte_be32_t metadata;
		uint16_t tso_segsz = 0;
#ifdef MLX5_PMD_SOFT_COUNTERS
		uint32_t total_length = 0;
#endif
		int ret;

		segs_n = buf->nb_segs;
		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max_elts < segs_n)
			break;
		max_elts -= segs_n;
		sg = --segs_n;
		if (unlikely(--max_wqe == 0))
			break;
		wqe = (volatile struct mlx5_wqe_v *)
			tx_mlx5_wqe(txq, txq->wqe_ci);
		rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
		if (pkts_n - i > 1)
			rte_prefetch0(*(pkts + 1));
		addr = rte_pktmbuf_mtod(buf, uintptr_t);
		length = DATA_LEN(buf);
		ehdr = (((uint8_t *)addr)[1] << 8) |
		       ((uint8_t *)addr)[0];
#ifdef MLX5_PMD_SOFT_COUNTERS
		total_length = length;
#endif
		if (length < (MLX5_WQE_DWORD_SIZE + 2)) {
			txq->stats.oerrors++;
			break;
		}
		/* Update element. */
		(*txq->elts)[elts_head & elts_m] = buf;
		/* Prefetch next buffer data. */
		if (pkts_n - i > 1)
			rte_prefetch0(
			    rte_pktmbuf_mtod(*(pkts + 1), volatile void *));
		cs_flags = txq_ol_cksum_to_cs(buf);
		txq_mbuf_to_swp(txq, buf, (uint8_t *)&swp_offsets, &swp_types);
		raw = ((uint8_t *)(uintptr_t)wqe) + 2 * MLX5_WQE_DWORD_SIZE;
		/* Copy metadata from mbuf if valid */
		metadata = buf->ol_flags & PKT_TX_METADATA ? buf->tx_metadata :
							     0;
		/* Replace the Ethernet type by the VLAN if necessary. */
		if (buf->ol_flags & PKT_TX_VLAN_PKT) {
			uint32_t vlan = rte_cpu_to_be_32(0x81000000 |
							 buf->vlan_tci);
			unsigned int len = 2 * ETHER_ADDR_LEN - 2;

			addr += 2;
			length -= 2;
			/* Copy Destination and source mac address. */
			memcpy((uint8_t *)raw, ((uint8_t *)addr), len);
			/* Copy VLAN. */
			memcpy((uint8_t *)raw + len, &vlan, sizeof(vlan));
			/* Copy missing two bytes to end the DSeg. */
			memcpy((uint8_t *)raw + len + sizeof(vlan),
			       ((uint8_t *)addr) + len, 2);
			addr += len + 2;
			length -= (len + 2);
		} else {
			memcpy((uint8_t *)raw, ((uint8_t *)addr) + 2,
			       MLX5_WQE_DWORD_SIZE);
			length -= pkt_inline_sz;
			addr += pkt_inline_sz;
		}
		raw += MLX5_WQE_DWORD_SIZE;
		if (tso) {
			ret = inline_tso(txq, buf, &length,
					 &addr, &pkt_inline_sz,
					 &raw, &max_wqe,
					 &tso_segsz, &tso_header_sz);
			if (ret == -EINVAL) {
				break;
			} else if (ret == -EAGAIN) {
				/* NOP WQE. */
				wqe->ctrl = (rte_v128u32_t){
					rte_cpu_to_be_32(txq->wqe_ci << 8),
					rte_cpu_to_be_32(txq->qp_num_8s | 1),
					0,
					0,
				};
				ds = 1;
#ifdef MLX5_PMD_SOFT_COUNTERS
				total_length = 0;
#endif
				k++;
				goto next_wqe;
			}
		}
		/* Inline if enough room. */
		if (max_inline || tso) {
			uint32_t inl = 0;
			uintptr_t end = (uintptr_t)
				(((uintptr_t)txq->wqes) +
				 (1 << txq->wqe_n) * MLX5_WQE_SIZE);
			unsigned int inline_room = max_inline *
						   RTE_CACHE_LINE_SIZE -
						   (pkt_inline_sz - 2) -
						   !!tso * sizeof(inl);
			uintptr_t addr_end;
			unsigned int copy_b;

pkt_inline:
			addr_end = RTE_ALIGN_FLOOR(addr + inline_room,
						   RTE_CACHE_LINE_SIZE);
			copy_b = (addr_end > addr) ?
				 RTE_MIN((addr_end - addr), length) : 0;
			if (copy_b && ((end - (uintptr_t)raw) >
				       (copy_b + sizeof(inl)))) {
				/*
				 * One Dseg remains in the current WQE.  To
				 * keep the computation positive, it is
				 * removed after the bytes to Dseg conversion.
				 */
				uint16_t n = (MLX5_WQE_DS(copy_b) - 1 + 3) / 4;

				if (unlikely(max_wqe < n))
					break;
				max_wqe -= n;
				if (tso) {
					assert(inl == 0);
					inl = rte_cpu_to_be_32(copy_b |
							       MLX5_INLINE_SEG);
					rte_memcpy((void *)raw,
						   (void *)&inl, sizeof(inl));
					raw += sizeof(inl);
					pkt_inline_sz += sizeof(inl);
				}
				rte_memcpy((void *)raw, (void *)addr, copy_b);
				addr += copy_b;
				length -= copy_b;
				pkt_inline_sz += copy_b;
			}
			/*
			 * 2 DWORDs consumed by the WQE header + ETH segment +
			 * the size of the inline part of the packet.
			 */
			ds = 2 + MLX5_WQE_DS(pkt_inline_sz - 2);
			if (length > 0) {
				if (ds % (MLX5_WQE_SIZE /
					  MLX5_WQE_DWORD_SIZE) == 0) {
					if (unlikely(--max_wqe == 0))
						break;
					dseg = (volatile rte_v128u32_t *)
					       tx_mlx5_wqe(txq, txq->wqe_ci +
							   ds / 4);
				} else {
					dseg = (volatile rte_v128u32_t *)
						((uintptr_t)wqe +
						 (ds * MLX5_WQE_DWORD_SIZE));
				}
				goto use_dseg;
			} else if (!segs_n) {
				goto next_pkt;
			} else {
				/*
				 * Further inline the next segment only for
				 * non-TSO packets.
				 */
				if (!tso) {
					raw += copy_b;
					inline_room -= copy_b;
				} else {
					inline_room = 0;
				}
				/* Move to the next segment. */
				--segs_n;
				buf = buf->next;
				assert(buf);
				addr = rte_pktmbuf_mtod(buf, uintptr_t);
				length = DATA_LEN(buf);
#ifdef MLX5_PMD_SOFT_COUNTERS
				total_length += length;
#endif
				(*txq->elts)[++elts_head & elts_m] = buf;
				goto pkt_inline;
			}
		} else {
			/*
			 * No inline has been done in the packet, only the
			 * Ethernet Header as been stored.
			 */
			dseg = (volatile rte_v128u32_t *)
				((uintptr_t)wqe + (3 * MLX5_WQE_DWORD_SIZE));
			ds = 3;
use_dseg:
			/* Add the remaining packet as a simple ds. */
			addr_64 = rte_cpu_to_be_64(addr);
			*dseg = (rte_v128u32_t){
				rte_cpu_to_be_32(length),
				mlx5_tx_mb2mr(txq, buf),
				addr_64,
				addr_64 >> 32,
			};
			++ds;
			if (!segs_n)
				goto next_pkt;
		}
next_seg:
		assert(buf);
		assert(ds);
		assert(wqe);
		/*
		 * Spill on next WQE when the current one does not have
		 * enough room left. Size of WQE must a be a multiple
		 * of data segment size.
		 */
		assert(!(MLX5_WQE_SIZE % MLX5_WQE_DWORD_SIZE));
		if (!(ds % (MLX5_WQE_SIZE / MLX5_WQE_DWORD_SIZE))) {
			if (unlikely(--max_wqe == 0))
				break;
			dseg = (volatile rte_v128u32_t *)
			       tx_mlx5_wqe(txq, txq->wqe_ci + ds / 4);
			rte_prefetch0(tx_mlx5_wqe(txq,
						  txq->wqe_ci + ds / 4 + 1));
		} else {
			++dseg;
		}
		++ds;
		buf = buf->next;
		assert(buf);
		length = DATA_LEN(buf);
#ifdef MLX5_PMD_SOFT_COUNTERS
		total_length += length;
#endif
		/* Store segment information. */
		addr_64 = rte_cpu_to_be_64(rte_pktmbuf_mtod(buf, uintptr_t));
		*dseg = (rte_v128u32_t){
			rte_cpu_to_be_32(length),
			mlx5_tx_mb2mr(txq, buf),
			addr_64,
			addr_64 >> 32,
		};
		(*txq->elts)[++elts_head & elts_m] = buf;
		if (--segs_n)
			goto next_seg;
next_pkt:
		if (ds > MLX5_DSEG_MAX) {
			txq->stats.oerrors++;
			break;
		}
		++elts_head;
		++pkts;
		++i;
		j += sg;
		/* Initialize known and common part of the WQE structure. */
		if (tso) {
			wqe->ctrl = (rte_v128u32_t){
				rte_cpu_to_be_32((txq->wqe_ci << 8) |
						 MLX5_OPCODE_TSO),
				rte_cpu_to_be_32(txq->qp_num_8s | ds),
				0,
				0,
			};
			wqe->eseg = (rte_v128u32_t){
				swp_offsets,
				cs_flags | (swp_types << 8) |
				(rte_cpu_to_be_16(tso_segsz) << 16),
				metadata,
				(ehdr << 16) | rte_cpu_to_be_16(tso_header_sz),
			};
		} else {
			wqe->ctrl = (rte_v128u32_t){
				rte_cpu_to_be_32((txq->wqe_ci << 8) |
						 MLX5_OPCODE_SEND),
				rte_cpu_to_be_32(txq->qp_num_8s | ds),
				0,
				0,
			};
			wqe->eseg = (rte_v128u32_t){
				swp_offsets,
				cs_flags | (swp_types << 8),
				metadata,
				(ehdr << 16) | rte_cpu_to_be_16(pkt_inline_sz),
			};
		}
next_wqe:
		txq->wqe_ci += (ds + 3) / 4;
		/* Save the last successful WQE for completion request */
		last_wqe = (volatile struct mlx5_wqe_ctrl *)wqe;
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += total_length;
#endif
	} while (i < pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely((i + k) == 0))
		return 0;
	txq->elts_head += (i + j);
	/* Check whether completion threshold has been reached. */
	comp = txq->elts_comp + i + j + k;
	if (comp >= MLX5_TX_COMP_THRESH) {
		/* A CQE slot must always be available. */
		assert((1u << txq->cqe_n) - (txq->cq_pi++ - txq->cq_ci));
		/* Request completion on last WQE. */
		last_wqe->ctrl2 = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		last_wqe->ctrl3 = txq->elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq, (volatile struct mlx5_wqe *)last_wqe);
	return i;
}

/**
 * Open a MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 * @param length
 *   Packet length.
 */
static inline void
mlx5_mpw_new(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw, uint32_t length)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);
	volatile struct mlx5_wqe_data_seg (*dseg)[MLX5_MPW_DSEG_MAX] =
		(volatile struct mlx5_wqe_data_seg (*)[])
		tx_mlx5_wqe(txq, idx + 1);

	mpw->state = MLX5_MPW_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->len = length;
	mpw->total_len = 0;
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->eseg.mss = rte_cpu_to_be_16(length);
	mpw->wqe->eseg.inline_hdr_sz = 0;
	mpw->wqe->eseg.rsvd0 = 0;
	mpw->wqe->eseg.rsvd1 = 0;
	mpw->wqe->eseg.flow_table_metadata = 0;
	mpw->wqe->ctrl[0] = rte_cpu_to_be_32((MLX5_OPC_MOD_MPW << 24) |
					     (txq->wqe_ci << 8) |
					     MLX5_OPCODE_TSO);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	mpw->data.dseg[0] = (volatile struct mlx5_wqe_data_seg *)
		(((uintptr_t)mpw->wqe) + (2 * MLX5_WQE_DWORD_SIZE));
	mpw->data.dseg[1] = (volatile struct mlx5_wqe_data_seg *)
		(((uintptr_t)mpw->wqe) + (3 * MLX5_WQE_DWORD_SIZE));
	mpw->data.dseg[2] = &(*dseg)[0];
	mpw->data.dseg[3] = &(*dseg)[1];
	mpw->data.dseg[4] = &(*dseg)[2];
}

/**
 * Close a MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 */
static inline void
mlx5_mpw_close(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw)
{
	unsigned int num = mpw->pkts_n;

	/*
	 * Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = rte_cpu_to_be_32(txq->qp_num_8s | (2 + num));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	if (num < 3)
		++txq->wqe_ci;
	else
		txq->wqe_ci += 2;
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci));
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
}

/**
 * DPDK callback for TX with MPW support.
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
mlx5_tx_burst_mpw(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	uint16_t max_elts;
	uint16_t max_wqe;
	unsigned int comp;
	struct mlx5_mpw mpw = {
		.state = MLX5_MPW_STATE_CLOSED,
	};

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci));
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
	/* Start processing. */
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		struct rte_mbuf *buf = *(pkts++);
		uint32_t length;
		unsigned int segs_n = buf->nb_segs;
		uint32_t cs_flags;
		rte_be32_t metadata;

		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max_elts < segs_n)
			break;
		/* Do not bother with large packets MPW cannot handle. */
		if (segs_n > MLX5_MPW_DSEG_MAX) {
			txq->stats.oerrors++;
			break;
		}
		max_elts -= segs_n;
		--pkts_n;
		cs_flags = txq_ol_cksum_to_cs(buf);
		/* Copy metadata from mbuf if valid */
		metadata = buf->ol_flags & PKT_TX_METADATA ? buf->tx_metadata :
							     0;
		/* Retrieve packet information. */
		length = PKT_LEN(buf);
		assert(length);
		/* Start new session if packet differs. */
		if ((mpw.state == MLX5_MPW_STATE_OPENED) &&
		    ((mpw.len != length) ||
		     (segs_n != 1) ||
		     (mpw.wqe->eseg.flow_table_metadata != metadata) ||
		     (mpw.wqe->eseg.cs_flags != cs_flags)))
			mlx5_mpw_close(txq, &mpw);
		if (mpw.state == MLX5_MPW_STATE_CLOSED) {
			/*
			 * Multi-Packet WQE consumes at most two WQE.
			 * mlx5_mpw_new() expects to be able to use such
			 * resources.
			 */
			if (unlikely(max_wqe < 2))
				break;
			max_wqe -= 2;
			mlx5_mpw_new(txq, &mpw, length);
			mpw.wqe->eseg.cs_flags = cs_flags;
			mpw.wqe->eseg.flow_table_metadata = metadata;
		}
		/* Multi-segment packets must be alone in their MPW. */
		assert((segs_n == 1) || (mpw.pkts_n == 0));
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
		length = 0;
#endif
		do {
			volatile struct mlx5_wqe_data_seg *dseg;
			uintptr_t addr;

			assert(buf);
			(*txq->elts)[elts_head++ & elts_m] = buf;
			dseg = mpw.data.dseg[mpw.pkts_n];
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			*dseg = (struct mlx5_wqe_data_seg){
				.byte_count = rte_cpu_to_be_32(DATA_LEN(buf)),
				.lkey = mlx5_tx_mb2mr(txq, buf),
				.addr = rte_cpu_to_be_64(addr),
			};
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
			length += DATA_LEN(buf);
#endif
			buf = buf->next;
			++mpw.pkts_n;
			++j;
		} while (--segs_n);
		assert(length == mpw.len);
		if (mpw.pkts_n == MLX5_MPW_DSEG_MAX)
			mlx5_mpw_close(txq, &mpw);
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		++i;
	} while (pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	/* "j" includes both packets and segments. */
	comp = txq->elts_comp + j;
	if (comp >= MLX5_TX_COMP_THRESH) {
		volatile struct mlx5_wqe *wqe = mpw.wqe;

		/* A CQE slot must always be available. */
		assert((1u << txq->cqe_n) - (txq->cq_pi++ - txq->cq_ci));
		/* Request completion on last WQE. */
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	if (mpw.state == MLX5_MPW_STATE_OPENED)
		mlx5_mpw_close(txq, &mpw);
	mlx5_tx_dbrec(txq, mpw.wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * Open a MPW inline session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 * @param length
 *   Packet length.
 */
static inline void
mlx5_mpw_inline_new(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw,
		    uint32_t length)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);
	struct mlx5_wqe_inl_small *inl;

	mpw->state = MLX5_MPW_INL_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->len = length;
	mpw->total_len = 0;
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->ctrl[0] = rte_cpu_to_be_32((MLX5_OPC_MOD_MPW << 24) |
					     (txq->wqe_ci << 8) |
					     MLX5_OPCODE_TSO);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	mpw->wqe->eseg.mss = rte_cpu_to_be_16(length);
	mpw->wqe->eseg.inline_hdr_sz = 0;
	mpw->wqe->eseg.cs_flags = 0;
	mpw->wqe->eseg.rsvd0 = 0;
	mpw->wqe->eseg.rsvd1 = 0;
	mpw->wqe->eseg.flow_table_metadata = 0;
	inl = (struct mlx5_wqe_inl_small *)
		(((uintptr_t)mpw->wqe) + 2 * MLX5_WQE_DWORD_SIZE);
	mpw->data.raw = (uint8_t *)&inl->raw;
}

/**
 * Close a MPW inline session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 */
static inline void
mlx5_mpw_inline_close(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw)
{
	unsigned int size;
	struct mlx5_wqe_inl_small *inl = (struct mlx5_wqe_inl_small *)
		(((uintptr_t)mpw->wqe) + (2 * MLX5_WQE_DWORD_SIZE));

	size = MLX5_WQE_SIZE - MLX5_MWQE64_INL_DATA + mpw->total_len;
	/*
	 * Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = rte_cpu_to_be_32(txq->qp_num_8s |
					     MLX5_WQE_DS(size));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	inl->byte_cnt = rte_cpu_to_be_32(mpw->total_len | MLX5_INLINE_SEG);
	txq->wqe_ci += (size + (MLX5_WQE_SIZE - 1)) / MLX5_WQE_SIZE;
}

/**
 * DPDK callback for TX with MPW inline support.
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
mlx5_tx_burst_mpw_inline(void *dpdk_txq, struct rte_mbuf **pkts,
			 uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	uint16_t max_elts;
	uint16_t max_wqe;
	unsigned int comp;
	unsigned int inline_room = txq->max_inline * RTE_CACHE_LINE_SIZE;
	struct mlx5_mpw mpw = {
		.state = MLX5_MPW_STATE_CLOSED,
	};
	/*
	 * Compute the maximum number of WQE which can be consumed by inline
	 * code.
	 * - 2 DSEG for:
	 *   - 1 control segment,
	 *   - 1 Ethernet segment,
	 * - N Dseg from the inline request.
	 */
	const unsigned int wqe_inl_n =
		((2 * MLX5_WQE_DWORD_SIZE +
		  txq->max_inline * RTE_CACHE_LINE_SIZE) +
		 RTE_CACHE_LINE_SIZE - 1) / RTE_CACHE_LINE_SIZE;

	if (unlikely(!pkts_n))
		return 0;
	/* Prefetch first packet cacheline. */
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci));
	rte_prefetch0(tx_mlx5_wqe(txq, txq->wqe_ci + 1));
	/* Start processing. */
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	do {
		struct rte_mbuf *buf = *(pkts++);
		uintptr_t addr;
		uint32_t length;
		unsigned int segs_n = buf->nb_segs;
		uint8_t cs_flags;
		rte_be32_t metadata;

		/*
		 * Make sure there is enough room to store this packet and
		 * that one ring entry remains unused.
		 */
		assert(segs_n);
		if (max_elts < segs_n)
			break;
		/* Do not bother with large packets MPW cannot handle. */
		if (segs_n > MLX5_MPW_DSEG_MAX) {
			txq->stats.oerrors++;
			break;
		}
		max_elts -= segs_n;
		--pkts_n;
		/*
		 * Compute max_wqe in case less WQE were consumed in previous
		 * iteration.
		 */
		max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
		cs_flags = txq_ol_cksum_to_cs(buf);
		/* Copy metadata from mbuf if valid */
		metadata = buf->ol_flags & PKT_TX_METADATA ? buf->tx_metadata :
							     0;
		/* Retrieve packet information. */
		length = PKT_LEN(buf);
		/* Start new session if packet differs. */
		if (mpw.state == MLX5_MPW_STATE_OPENED) {
			if ((mpw.len != length) ||
			    (segs_n != 1) ||
			    (mpw.wqe->eseg.flow_table_metadata != metadata) ||
			    (mpw.wqe->eseg.cs_flags != cs_flags))
				mlx5_mpw_close(txq, &mpw);
		} else if (mpw.state == MLX5_MPW_INL_STATE_OPENED) {
			if ((mpw.len != length) ||
			    (segs_n != 1) ||
			    (length > inline_room) ||
			    (mpw.wqe->eseg.flow_table_metadata != metadata) ||
			    (mpw.wqe->eseg.cs_flags != cs_flags)) {
				mlx5_mpw_inline_close(txq, &mpw);
				inline_room =
					txq->max_inline * RTE_CACHE_LINE_SIZE;
			}
		}
		if (mpw.state == MLX5_MPW_STATE_CLOSED) {
			if ((segs_n != 1) ||
			    (length > inline_room)) {
				/*
				 * Multi-Packet WQE consumes at most two WQE.
				 * mlx5_mpw_new() expects to be able to use
				 * such resources.
				 */
				if (unlikely(max_wqe < 2))
					break;
				max_wqe -= 2;
				mlx5_mpw_new(txq, &mpw, length);
				mpw.wqe->eseg.cs_flags = cs_flags;
				mpw.wqe->eseg.flow_table_metadata = metadata;
			} else {
				if (unlikely(max_wqe < wqe_inl_n))
					break;
				max_wqe -= wqe_inl_n;
				mlx5_mpw_inline_new(txq, &mpw, length);
				mpw.wqe->eseg.cs_flags = cs_flags;
				mpw.wqe->eseg.flow_table_metadata = metadata;
			}
		}
		/* Multi-segment packets must be alone in their MPW. */
		assert((segs_n == 1) || (mpw.pkts_n == 0));
		if (mpw.state == MLX5_MPW_STATE_OPENED) {
			assert(inline_room ==
			       txq->max_inline * RTE_CACHE_LINE_SIZE);
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
			length = 0;
#endif
			do {
				volatile struct mlx5_wqe_data_seg *dseg;

				assert(buf);
				(*txq->elts)[elts_head++ & elts_m] = buf;
				dseg = mpw.data.dseg[mpw.pkts_n];
				addr = rte_pktmbuf_mtod(buf, uintptr_t);
				*dseg = (struct mlx5_wqe_data_seg){
					.byte_count =
					       rte_cpu_to_be_32(DATA_LEN(buf)),
					.lkey = mlx5_tx_mb2mr(txq, buf),
					.addr = rte_cpu_to_be_64(addr),
				};
#if defined(MLX5_PMD_SOFT_COUNTERS) || !defined(NDEBUG)
				length += DATA_LEN(buf);
#endif
				buf = buf->next;
				++mpw.pkts_n;
				++j;
			} while (--segs_n);
			assert(length == mpw.len);
			if (mpw.pkts_n == MLX5_MPW_DSEG_MAX)
				mlx5_mpw_close(txq, &mpw);
		} else {
			unsigned int max;

			assert(mpw.state == MLX5_MPW_INL_STATE_OPENED);
			assert(length <= inline_room);
			assert(length == DATA_LEN(buf));
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			(*txq->elts)[elts_head++ & elts_m] = buf;
			/* Maximum number of bytes before wrapping. */
			max = ((((uintptr_t)(txq->wqes)) +
				(1 << txq->wqe_n) *
				MLX5_WQE_SIZE) -
			       (uintptr_t)mpw.data.raw);
			if (length > max) {
				rte_memcpy((void *)(uintptr_t)mpw.data.raw,
					   (void *)addr,
					   max);
				mpw.data.raw = (volatile void *)txq->wqes;
				rte_memcpy((void *)(uintptr_t)mpw.data.raw,
					   (void *)(addr + max),
					   length - max);
				mpw.data.raw += length - max;
			} else {
				rte_memcpy((void *)(uintptr_t)mpw.data.raw,
					   (void *)addr,
					   length);

				if (length == max)
					mpw.data.raw =
						(volatile void *)txq->wqes;
				else
					mpw.data.raw += length;
			}
			++mpw.pkts_n;
			mpw.total_len += length;
			++j;
			if (mpw.pkts_n == MLX5_MPW_DSEG_MAX) {
				mlx5_mpw_inline_close(txq, &mpw);
				inline_room =
					txq->max_inline * RTE_CACHE_LINE_SIZE;
			} else {
				inline_room -= length;
			}
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		++i;
	} while (pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	/* "j" includes both packets and segments. */
	comp = txq->elts_comp + j;
	if (comp >= MLX5_TX_COMP_THRESH) {
		volatile struct mlx5_wqe *wqe = mpw.wqe;

		/* A CQE slot must always be available. */
		assert((1u << txq->cqe_n) - (txq->cq_pi++ - txq->cq_ci));
		/* Request completion on last WQE. */
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
	} else {
		txq->elts_comp = comp;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	/* Ring QP doorbell. */
	if (mpw.state == MLX5_MPW_INL_STATE_OPENED)
		mlx5_mpw_inline_close(txq, &mpw);
	else if (mpw.state == MLX5_MPW_STATE_OPENED)
		mlx5_mpw_close(txq, &mpw);
	mlx5_tx_dbrec(txq, mpw.wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * Open an Enhanced MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 * @param length
 *   Packet length.
 */
static inline void
mlx5_empw_new(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw, int padding)
{
	uint16_t idx = txq->wqe_ci & ((1 << txq->wqe_n) - 1);

	mpw->state = MLX5_MPW_ENHANCED_STATE_OPENED;
	mpw->pkts_n = 0;
	mpw->total_len = sizeof(struct mlx5_wqe);
	mpw->wqe = (volatile struct mlx5_wqe *)tx_mlx5_wqe(txq, idx);
	mpw->wqe->ctrl[0] =
		rte_cpu_to_be_32((MLX5_OPC_MOD_ENHANCED_MPSW << 24) |
				 (txq->wqe_ci << 8) |
				 MLX5_OPCODE_ENHANCED_MPSW);
	mpw->wqe->ctrl[2] = 0;
	mpw->wqe->ctrl[3] = 0;
	memset((void *)(uintptr_t)&mpw->wqe->eseg, 0, MLX5_WQE_DWORD_SIZE);
	if (unlikely(padding)) {
		uintptr_t addr = (uintptr_t)(mpw->wqe + 1);

		/* Pad the first 2 DWORDs with zero-length inline header. */
		*(volatile uint32_t *)addr = rte_cpu_to_be_32(MLX5_INLINE_SEG);
		*(volatile uint32_t *)(addr + MLX5_WQE_DWORD_SIZE) =
			rte_cpu_to_be_32(MLX5_INLINE_SEG);
		mpw->total_len += 2 * MLX5_WQE_DWORD_SIZE;
		/* Start from the next WQEBB. */
		mpw->data.raw = (volatile void *)(tx_mlx5_wqe(txq, idx + 1));
	} else {
		mpw->data.raw = (volatile void *)(mpw->wqe + 1);
	}
}

/**
 * Close an Enhanced MPW session.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param mpw
 *   Pointer to MPW session structure.
 *
 * @return
 *   Number of consumed WQEs.
 */
static inline uint16_t
mlx5_empw_close(struct mlx5_txq_data *txq, struct mlx5_mpw *mpw)
{
	uint16_t ret;

	/* Store size in multiple of 16 bytes. Control and Ethernet segments
	 * count as 2.
	 */
	mpw->wqe->ctrl[1] = rte_cpu_to_be_32(txq->qp_num_8s |
					     MLX5_WQE_DS(mpw->total_len));
	mpw->state = MLX5_MPW_STATE_CLOSED;
	ret = (mpw->total_len + (MLX5_WQE_SIZE - 1)) / MLX5_WQE_SIZE;
	txq->wqe_ci += ret;
	return ret;
}

/**
 * TX with Enhanced MPW support.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static inline uint16_t
txq_burst_empw(struct mlx5_txq_data *txq, struct rte_mbuf **pkts,
	       uint16_t pkts_n)
{
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	unsigned int i = 0;
	unsigned int j = 0;
	uint16_t max_elts;
	uint16_t max_wqe;
	unsigned int max_inline = txq->max_inline * RTE_CACHE_LINE_SIZE;
	unsigned int mpw_room = 0;
	unsigned int inl_pad = 0;
	uint32_t inl_hdr;
	uint64_t addr_64;
	struct mlx5_mpw mpw = {
		.state = MLX5_MPW_STATE_CLOSED,
	};

	if (unlikely(!pkts_n))
		return 0;
	/* Start processing. */
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	if (unlikely(!max_wqe))
		return 0;
	do {
		struct rte_mbuf *buf = *(pkts++);
		uintptr_t addr;
		unsigned int do_inline = 0; /* Whether inline is possible. */
		uint32_t length;
		uint8_t cs_flags;
		rte_be32_t metadata;

		/* Multi-segmented packet is handled in slow-path outside. */
		assert(NB_SEGS(buf) == 1);
		/* Make sure there is enough room to store this packet. */
		if (max_elts - j == 0)
			break;
		cs_flags = txq_ol_cksum_to_cs(buf);
		/* Copy metadata from mbuf if valid */
		metadata = buf->ol_flags & PKT_TX_METADATA ? buf->tx_metadata :
							     0;
		/* Retrieve packet information. */
		length = PKT_LEN(buf);
		/* Start new session if:
		 * - multi-segment packet
		 * - no space left even for a dseg
		 * - next packet can be inlined with a new WQE
		 * - cs_flag differs
		 */
		if (mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED) {
			if ((inl_pad + sizeof(struct mlx5_wqe_data_seg) >
			     mpw_room) ||
			    (length <= txq->inline_max_packet_sz &&
			     inl_pad + sizeof(inl_hdr) + length >
			     mpw_room) ||
			     (mpw.wqe->eseg.flow_table_metadata != metadata) ||
			    (mpw.wqe->eseg.cs_flags != cs_flags))
				max_wqe -= mlx5_empw_close(txq, &mpw);
		}
		if (unlikely(mpw.state == MLX5_MPW_STATE_CLOSED)) {
			/* In Enhanced MPW, inline as much as the budget is
			 * allowed. The remaining space is to be filled with
			 * dsegs. If the title WQEBB isn't padded, it will have
			 * 2 dsegs there.
			 */
			mpw_room = RTE_MIN(MLX5_WQE_SIZE_MAX,
					   (max_inline ? max_inline :
					    pkts_n * MLX5_WQE_DWORD_SIZE) +
					   MLX5_WQE_SIZE);
			if (unlikely(max_wqe * MLX5_WQE_SIZE < mpw_room))
				break;
			/* Don't pad the title WQEBB to not waste WQ. */
			mlx5_empw_new(txq, &mpw, 0);
			mpw_room -= mpw.total_len;
			inl_pad = 0;
			do_inline = length <= txq->inline_max_packet_sz &&
				    sizeof(inl_hdr) + length <= mpw_room &&
				    !txq->mpw_hdr_dseg;
			mpw.wqe->eseg.cs_flags = cs_flags;
			mpw.wqe->eseg.flow_table_metadata = metadata;
		} else {
			/* Evaluate whether the next packet can be inlined.
			 * Inlininig is possible when:
			 * - length is less than configured value
			 * - length fits for remaining space
			 * - not required to fill the title WQEBB with dsegs
			 */
			do_inline =
				length <= txq->inline_max_packet_sz &&
				inl_pad + sizeof(inl_hdr) + length <=
				 mpw_room &&
				(!txq->mpw_hdr_dseg ||
				 mpw.total_len >= MLX5_WQE_SIZE);
		}
		if (max_inline && do_inline) {
			/* Inline packet into WQE. */
			unsigned int max;

			assert(mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED);
			assert(length == DATA_LEN(buf));
			inl_hdr = rte_cpu_to_be_32(length | MLX5_INLINE_SEG);
			addr = rte_pktmbuf_mtod(buf, uintptr_t);
			mpw.data.raw = (volatile void *)
				((uintptr_t)mpw.data.raw + inl_pad);
			max = tx_mlx5_wq_tailroom(txq,
					(void *)(uintptr_t)mpw.data.raw);
			/* Copy inline header. */
			mpw.data.raw = (volatile void *)
				mlx5_copy_to_wq(
					  (void *)(uintptr_t)mpw.data.raw,
					  &inl_hdr,
					  sizeof(inl_hdr),
					  (void *)(uintptr_t)txq->wqes,
					  max);
			max = tx_mlx5_wq_tailroom(txq,
					(void *)(uintptr_t)mpw.data.raw);
			/* Copy packet data. */
			mpw.data.raw = (volatile void *)
				mlx5_copy_to_wq(
					  (void *)(uintptr_t)mpw.data.raw,
					  (void *)addr,
					  length,
					  (void *)(uintptr_t)txq->wqes,
					  max);
			++mpw.pkts_n;
			mpw.total_len += (inl_pad + sizeof(inl_hdr) + length);
			/* No need to get completion as the entire packet is
			 * copied to WQ. Free the buf right away.
			 */
			rte_pktmbuf_free_seg(buf);
			mpw_room -= (inl_pad + sizeof(inl_hdr) + length);
			/* Add pad in the next packet if any. */
			inl_pad = (((uintptr_t)mpw.data.raw +
					(MLX5_WQE_DWORD_SIZE - 1)) &
					~(MLX5_WQE_DWORD_SIZE - 1)) -
				  (uintptr_t)mpw.data.raw;
		} else {
			/* No inline. Load a dseg of packet pointer. */
			volatile rte_v128u32_t *dseg;

			assert(mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED);
			assert((inl_pad + sizeof(*dseg)) <= mpw_room);
			assert(length == DATA_LEN(buf));
			if (!tx_mlx5_wq_tailroom(txq,
					(void *)((uintptr_t)mpw.data.raw
						+ inl_pad)))
				dseg = (volatile void *)txq->wqes;
			else
				dseg = (volatile void *)
					((uintptr_t)mpw.data.raw +
					 inl_pad);
			(*txq->elts)[elts_head++ & elts_m] = buf;
			addr_64 = rte_cpu_to_be_64(rte_pktmbuf_mtod(buf,
								    uintptr_t));
			*dseg = (rte_v128u32_t) {
				rte_cpu_to_be_32(length),
				mlx5_tx_mb2mr(txq, buf),
				addr_64,
				addr_64 >> 32,
			};
			mpw.data.raw = (volatile void *)(dseg + 1);
			mpw.total_len += (inl_pad + sizeof(*dseg));
			++j;
			++mpw.pkts_n;
			mpw_room -= (inl_pad + sizeof(*dseg));
			inl_pad = 0;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Increment sent bytes counter. */
		txq->stats.obytes += length;
#endif
		++i;
	} while (i < pkts_n);
	/* Take a shortcut if nothing must be sent. */
	if (unlikely(i == 0))
		return 0;
	/* Check whether completion threshold has been reached. */
	if (txq->elts_comp + j >= MLX5_TX_COMP_THRESH ||
			(uint16_t)(txq->wqe_ci - txq->mpw_comp) >=
			 (1 << txq->wqe_n) / MLX5_TX_COMP_THRESH_INLINE_DIV) {
		volatile struct mlx5_wqe *wqe = mpw.wqe;

		/* A CQE slot must always be available. */
		assert((1u << txq->cqe_n) - (txq->cq_pi++ - txq->cq_ci));
		/* Request completion on last WQE. */
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		/* Save elts_head in unused "immediate" field of WQE. */
		wqe->ctrl[3] = elts_head;
		txq->elts_comp = 0;
		txq->mpw_comp = txq->wqe_ci;
	} else {
		txq->elts_comp += j;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment sent packets counter. */
	txq->stats.opackets += i;
#endif
	if (mpw.state == MLX5_MPW_ENHANCED_STATE_OPENED)
		mlx5_empw_close(txq, &mpw);
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq, mpw.wqe);
	txq->elts_head = elts_head;
	return i;
}

/**
 * DPDK callback for TX with Enhanced MPW support.
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
mlx5_tx_burst_empw(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	uint16_t nb_tx = 0;

	while (pkts_n > nb_tx) {
		uint16_t n;
		uint16_t ret;

		n = txq_count_contig_multi_seg(&pkts[nb_tx], pkts_n - nb_tx);
		if (n) {
			ret = mlx5_tx_burst(dpdk_txq, &pkts[nb_tx], n);
			if (!ret)
				break;
			nb_tx += ret;
		}
		n = txq_count_contig_single_seg(&pkts[nb_tx], pkts_n - nb_tx);
		if (n) {
			ret = txq_burst_empw(txq, &pkts[nb_tx], n);
			if (!ret)
				break;
			nb_tx += ret;
		}
	}
	return nb_tx;
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
rxq_cq_to_pkt_type(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe)
{
	uint8_t idx;
	uint8_t pinfo = cqe->pkt_info;
	uint16_t ptype = cqe->hdr_type_etc;

	/*
	 * The index to the array should have:
	 * bit[1:0] = l3_hdr_type
	 * bit[4:2] = l4_hdr_type
	 * bit[5] = ip_frag
	 * bit[6] = tunneled
	 * bit[7] = outer_l3_type
	 */
	idx = ((pinfo & 0x3) << 6) | ((ptype & 0xfc00) >> 10);
	return mlx5_ptype_table[idx] | rxq->tunnel * !!(idx & (1 << 6));
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
 *   Packet size in bytes (0 if there is none), -1 in case of completion
 *   with error.
 */
static inline int
mlx5_rx_poll_len(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cqe,
		 uint16_t cqe_cnt, volatile struct mlx5_mini_cqe8 **mcqe)
{
	struct rxq_zip *zip = &rxq->zip;
	uint16_t cqe_n = cqe_cnt + 1;
	int len = 0;
	uint16_t idx, end;

	/* Process compressed data in the CQE and mini arrays. */
	if (zip->ai) {
		volatile struct mlx5_mini_cqe8 (*mc)[8] =
			(volatile struct mlx5_mini_cqe8 (*)[8])
			(uintptr_t)(&(*rxq->cqes)[zip->ca & cqe_cnt].pkt_info);

		len = rte_be_to_cpu_32((*mc)[zip->ai & 7].byte_cnt);
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
			 * Increment consumer index to skip the number of
			 * CQEs consumed. Hardware leaves holes in the CQ
			 * ring for software use.
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
	/* No compressed data, get next CQE and verify if it is compressed. */
	} else {
		int ret;
		int8_t op_own;

		ret = check_cqe(cqe, cqe_n, rxq->cq_ci);
		if (unlikely(ret == 1))
			return 0;
		++rxq->cq_ci;
		op_own = cqe->op_own;
		rte_cio_rmb();
		if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED) {
			volatile struct mlx5_mini_cqe8 (*mc)[8] =
				(volatile struct mlx5_mini_cqe8 (*)[8])
				(uintptr_t)(&(*rxq->cqes)[rxq->cq_ci &
							  cqe_cnt].pkt_info);

			/* Fix endianness. */
			zip->cqe_cnt = rte_be_to_cpu_32(cqe->byte_cnt);
			/*
			 * Current mini array position is the one returned by
			 * check_cqe64().
			 *
			 * If completion comprises several mini arrays, as a
			 * special case the second one is located 7 CQEs after
			 * the initial CQE instead of 8 for subsequent ones.
			 */
			zip->ca = rxq->cq_ci;
			zip->na = zip->ca + 7;
			/* Compute the next non compressed CQE. */
			--rxq->cq_ci;
			zip->cq_ci = rxq->cq_ci + zip->cqe_cnt;
			/* Get packet size to return. */
			len = rte_be_to_cpu_32((*mc)[0].byte_cnt);
			*mcqe = &(*mc)[0];
			zip->ai = 1;
			/* Prefetch all the entries to be invalidated */
			idx = zip->ca;
			end = zip->cq_ci;
			while (idx != end) {
				rte_prefetch0(&(*rxq->cqes)[(idx) & cqe_cnt]);
				++idx;
			}
		} else {
			len = rte_be_to_cpu_32(cqe->byte_cnt);
		}
		/* Error while receiving packet. */
		if (unlikely(MLX5_CQE_OPCODE(op_own) == MLX5_CQE_RESP_ERR))
			return -1;
	}
	return len;
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
	       volatile struct mlx5_cqe *cqe, uint32_t rss_hash_res)
{
	/* Update packet information. */
	pkt->packet_type = rxq_cq_to_pkt_type(rxq, cqe);
	if (rss_hash_res && rxq->rss_hash) {
		pkt->hash.rss = rss_hash_res;
		pkt->ol_flags |= PKT_RX_RSS_HASH;
	}
	if (rxq->mark && MLX5_FLOW_MARK_IS_VALID(cqe->sop_drop_qpn)) {
		pkt->ol_flags |= PKT_RX_FDIR;
		if (cqe->sop_drop_qpn !=
		    rte_cpu_to_be_32(MLX5_FLOW_MARK_DEFAULT)) {
			uint32_t mark = cqe->sop_drop_qpn;

			pkt->ol_flags |= PKT_RX_FDIR_ID;
			pkt->hash.fdir.hi = mlx5_flow_mark_get(mark);
		}
	}
	if (rxq->csum)
		pkt->ol_flags |= rxq_cq_to_ol_flags(cqe);
	if (rxq->vlan_strip &&
	    (cqe->hdr_type_etc & rte_cpu_to_be_16(MLX5_CQE_VLAN_STRIPPED))) {
		pkt->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		pkt->vlan_tci = rte_be_to_cpu_16(cqe->vlan_info);
	}
	if (rxq->hw_timestamp) {
		pkt->timestamp = rte_be_to_cpu_64(cqe->timestamp);
		pkt->ol_flags |= PKT_RX_TIMESTAMP;
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
		uint32_t rss_hash_res;

		if (pkt)
			NEXT(seg) = rep;
		seg = rep;
		rte_prefetch0(seg);
		rte_prefetch0(cqe);
		rte_prefetch0(wqe);
		rep = rte_mbuf_raw_alloc(rxq->mp);
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
				assert(pkt != (*rxq->elts)[idx]);
				rep = NEXT(pkt);
				NEXT(pkt) = NULL;
				NB_SEGS(pkt) = 1;
				rte_mbuf_raw_free(pkt);
				pkt = rep;
			}
			break;
		}
		if (!pkt) {
			cqe = &(*rxq->cqes)[rxq->cq_ci & cqe_cnt];
			len = mlx5_rx_poll_len(rxq, cqe, cqe_cnt, &mcqe);
			if (!len) {
				rte_mbuf_raw_free(rep);
				break;
			}
			if (unlikely(len == -1)) {
				/* RX error, packet is likely too large. */
				rte_mbuf_raw_free(rep);
				++rxq->stats.idropped;
				goto skip;
			}
			pkt = seg;
			assert(len >= (rxq->crc_present << 2));
			pkt->ol_flags = 0;
			/* If compressed, take hash result from mini-CQE. */
			rss_hash_res = rte_be_to_cpu_32(mcqe == NULL ?
							cqe->rx_hash_res :
							mcqe->rx_hash_result);
			rxq_cq_to_mbuf(rxq, pkt, cqe, rss_hash_res);
			if (rxq->crc_present)
				len -= ETHER_CRC_LEN;
			PKT_LEN(pkt) = len;
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
skip:
		/* Align consumer index to the next stride. */
		rq_ci >>= sges_n;
		++rq_ci;
		rq_ci <<= sges_n;
	}
	if (unlikely((i == 0) && ((rq_ci >> sges_n) == rxq->rq_ci)))
		return 0;
	/* Update the consumer index. */
	rxq->rq_ci = rq_ci >> sges_n;
	rte_cio_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	rte_cio_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Increment packets counter. */
	rxq->stats.ipackets += i;
#endif
	return i;
}

void
mlx5_mprq_buf_free_cb(void *addr __rte_unused, void *opaque)
{
	struct mlx5_mprq_buf *buf = opaque;

	if (rte_atomic16_read(&buf->refcnt) == 1) {
		rte_mempool_put(buf->mp, buf);
	} else if (rte_atomic16_add_return(&buf->refcnt, -1) == 0) {
		rte_atomic16_set(&buf->refcnt, 1);
		rte_mempool_put(buf->mp, buf);
	}
}

void
mlx5_mprq_buf_free(struct mlx5_mprq_buf *buf)
{
	mlx5_mprq_buf_free_cb(NULL, buf);
}

static inline void
mprq_buf_replace(struct mlx5_rxq_data *rxq, uint16_t rq_idx)
{
	struct mlx5_mprq_buf *rep = rxq->mprq_repl;
	volatile struct mlx5_wqe_data_seg *wqe =
		&((volatile struct mlx5_wqe_mprq *)rxq->wqes)[rq_idx].dseg;
	void *addr;

	assert(rep != NULL);
	/* Replace MPRQ buf. */
	(*rxq->mprq_bufs)[rq_idx] = rep;
	/* Replace WQE. */
	addr = mlx5_mprq_buf_addr(rep);
	wqe->addr = rte_cpu_to_be_64((uintptr_t)addr);
	/* If there's only one MR, no need to replace LKey in WQE. */
	if (unlikely(mlx5_mr_btree_len(&rxq->mr_ctrl.cache_bh) > 1))
		wqe->lkey = mlx5_rx_addr2mr(rxq, (uintptr_t)addr);
	/* Stash a mbuf for next replacement. */
	if (likely(!rte_mempool_get(rxq->mprq_mp, (void **)&rep)))
		rxq->mprq_repl = rep;
	else
		rxq->mprq_repl = NULL;
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
	const unsigned int strd_n = 1 << rxq->strd_num_n;
	const unsigned int strd_sz = 1 << rxq->strd_sz_n;
	const unsigned int strd_shift =
		MLX5_MPRQ_STRIDE_SHIFT_BYTE * rxq->strd_shift_en;
	const unsigned int cq_mask = (1 << rxq->cqe_n) - 1;
	const unsigned int wq_mask = (1 << rxq->elts_n) - 1;
	volatile struct mlx5_cqe *cqe = &(*rxq->cqes)[rxq->cq_ci & cq_mask];
	unsigned int i = 0;
	uint32_t rq_ci = rxq->rq_ci;
	uint16_t consumed_strd = rxq->consumed_strd;
	struct mlx5_mprq_buf *buf = (*rxq->mprq_bufs)[rq_ci & wq_mask];

	while (i < pkts_n) {
		struct rte_mbuf *pkt;
		void *addr;
		int ret;
		unsigned int len;
		uint16_t strd_cnt;
		uint16_t strd_idx;
		uint32_t offset;
		uint32_t byte_cnt;
		volatile struct mlx5_mini_cqe8 *mcqe = NULL;
		uint32_t rss_hash_res = 0;

		if (consumed_strd == strd_n) {
			/* Replace WQE only if the buffer is still in use. */
			if (rte_atomic16_read(&buf->refcnt) > 1) {
				mprq_buf_replace(rxq, rq_ci & wq_mask);
				/* Release the old buffer. */
				mlx5_mprq_buf_free(buf);
			} else if (unlikely(rxq->mprq_repl == NULL)) {
				struct mlx5_mprq_buf *rep;

				/*
				 * Currently, the MPRQ mempool is out of buffer
				 * and doing memcpy regardless of the size of Rx
				 * packet. Retry allocation to get back to
				 * normal.
				 */
				if (!rte_mempool_get(rxq->mprq_mp,
						     (void **)&rep))
					rxq->mprq_repl = rep;
			}
			/* Advance to the next WQE. */
			consumed_strd = 0;
			++rq_ci;
			buf = (*rxq->mprq_bufs)[rq_ci & wq_mask];
		}
		cqe = &(*rxq->cqes)[rxq->cq_ci & cq_mask];
		ret = mlx5_rx_poll_len(rxq, cqe, cq_mask, &mcqe);
		if (!ret)
			break;
		if (unlikely(ret == -1)) {
			/* RX error, packet is likely too large. */
			++rxq->stats.idropped;
			continue;
		}
		byte_cnt = ret;
		strd_cnt = (byte_cnt & MLX5_MPRQ_STRIDE_NUM_MASK) >>
			   MLX5_MPRQ_STRIDE_NUM_SHIFT;
		assert(strd_cnt);
		consumed_strd += strd_cnt;
		if (byte_cnt & MLX5_MPRQ_FILLER_MASK)
			continue;
		if (mcqe == NULL) {
			rss_hash_res = rte_be_to_cpu_32(cqe->rx_hash_res);
			strd_idx = rte_be_to_cpu_16(cqe->wqe_counter);
		} else {
			/* mini-CQE for MPRQ doesn't have hash result. */
			strd_idx = rte_be_to_cpu_16(mcqe->stride_idx);
		}
		assert(strd_idx < strd_n);
		assert(!((rte_be_to_cpu_16(cqe->wqe_id) ^ rq_ci) & wq_mask));
		/*
		 * Currently configured to receive a packet per a stride. But if
		 * MTU is adjusted through kernel interface, device could
		 * consume multiple strides without raising an error. In this
		 * case, the packet should be dropped because it is bigger than
		 * the max_rx_pkt_len.
		 */
		if (unlikely(strd_cnt > 1)) {
			++rxq->stats.idropped;
			continue;
		}
		pkt = rte_pktmbuf_alloc(rxq->mp);
		if (unlikely(pkt == NULL)) {
			++rxq->stats.rx_nombuf;
			break;
		}
		len = (byte_cnt & MLX5_MPRQ_LEN_MASK) >> MLX5_MPRQ_LEN_SHIFT;
		assert((int)len >= (rxq->crc_present << 2));
		if (rxq->crc_present)
			len -= ETHER_CRC_LEN;
		offset = strd_idx * strd_sz + strd_shift;
		addr = RTE_PTR_ADD(mlx5_mprq_buf_addr(buf), offset);
		/* Initialize the offload flag. */
		pkt->ol_flags = 0;
		/*
		 * Memcpy packets to the target mbuf if:
		 * - The size of packet is smaller than mprq_max_memcpy_len.
		 * - Out of buffer in the Mempool for Multi-Packet RQ.
		 */
		if (len <= rxq->mprq_max_memcpy_len || rxq->mprq_repl == NULL) {
			/*
			 * When memcpy'ing packet due to out-of-buffer, the
			 * packet must be smaller than the target mbuf.
			 */
			if (unlikely(rte_pktmbuf_tailroom(pkt) < len)) {
				rte_pktmbuf_free_seg(pkt);
				++rxq->stats.idropped;
				continue;
			}
			rte_memcpy(rte_pktmbuf_mtod(pkt, void *), addr, len);
		} else {
			rte_iova_t buf_iova;
			struct rte_mbuf_ext_shared_info *shinfo;
			uint16_t buf_len = strd_cnt * strd_sz;

			/* Increment the refcnt of the whole chunk. */
			rte_atomic16_add_return(&buf->refcnt, 1);
			assert((uint16_t)rte_atomic16_read(&buf->refcnt) <=
			       strd_n + 1);
			addr = RTE_PTR_SUB(addr, RTE_PKTMBUF_HEADROOM);
			/*
			 * MLX5 device doesn't use iova but it is necessary in a
			 * case where the Rx packet is transmitted via a
			 * different PMD.
			 */
			buf_iova = rte_mempool_virt2iova(buf) +
				   RTE_PTR_DIFF(addr, buf);
			shinfo = rte_pktmbuf_ext_shinfo_init_helper(addr,
					&buf_len, mlx5_mprq_buf_free_cb, buf);
			/*
			 * EXT_ATTACHED_MBUF will be set to pkt->ol_flags when
			 * attaching the stride to mbuf and more offload flags
			 * will be added below by calling rxq_cq_to_mbuf().
			 * Other fields will be overwritten.
			 */
			rte_pktmbuf_attach_extbuf(pkt, addr, buf_iova, buf_len,
						  shinfo);
			rte_pktmbuf_reset_headroom(pkt);
			assert(pkt->ol_flags == EXT_ATTACHED_MBUF);
			/*
			 * Prevent potential overflow due to MTU change through
			 * kernel interface.
			 */
			if (unlikely(rte_pktmbuf_tailroom(pkt) < len)) {
				rte_pktmbuf_free_seg(pkt);
				++rxq->stats.idropped;
				continue;
			}
		}
		rxq_cq_to_mbuf(rxq, pkt, cqe, rss_hash_res);
		PKT_LEN(pkt) = len;
		DATA_LEN(pkt) = len;
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
	rte_cio_wmb();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	if (rq_ci != rxq->rq_ci) {
		rxq->rq_ci = rq_ci;
		rte_cio_wmb();
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
mlx5_tx_burst_raw_vec(void *dpdk_txq __rte_unused,
		      struct rte_mbuf **pkts __rte_unused,
		      uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak uint16_t
mlx5_tx_burst_vec(void *dpdk_txq __rte_unused,
		  struct rte_mbuf **pkts __rte_unused,
		  uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak uint16_t
mlx5_rx_burst_vec(void *dpdk_txq __rte_unused,
		  struct rte_mbuf **pkts __rte_unused,
		  uint16_t pkts_n __rte_unused)
{
	return 0;
}

__rte_weak int
mlx5_check_raw_vec_tx_support(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}

__rte_weak int
mlx5_check_vec_tx_support(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
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
