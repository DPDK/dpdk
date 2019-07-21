/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015-2019 Mellanox Technologies, Ltd
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
#include <rte_cycles.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

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

static int
mlx5_queue_state_modify(struct rte_eth_dev *dev,
			struct mlx5_mp_arg_queue_state_modify *sm);

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
		DRV_LOG(WARNING, "cannot open %s for debug dump\n",
			path);
		MKSTR(path2, "./%s", fname);
		fd = fopen(path2, "a+");
		if (!fd) {
			DRV_LOG(ERR, "cannot open %s for debug dump\n",
				path2);
			return;
		}
		DRV_LOG(INFO, "New debug dump in file %s\n", path2);
	} else {
		DRV_LOG(INFO, "New debug dump in file %s\n", path);
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
 *   The last Tx buffer element to free.
 */
uint16_t
mlx5_tx_error_cqe_handle(struct mlx5_txq_data *txq,
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
		if (tx_recover_qp(txq_ctrl) == 0) {
			txq->cq_ci++;
			/* Release all the remaining buffers. */
			return txq->elts_head;
		}
		/* Recovering failed - try again later on the same WQE. */
	} else {
		txq->cq_ci++;
	}
	/* Do not release buffers. */
	return txq->elts_tail;
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
			addr = (uintptr_t)mlx5_mprq_buf_addr(buf);
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
		assert(sizeof(scat->addr) >= sizeof(uintptr_t));
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
	/* Update doorbell counter. */
	rxq->rq_ci = wqe_n >> rxq->sges_n;
	rte_cio_wmb();
	*rxq->rq_db = rte_cpu_to_be_32(rxq->rq_ci);
}

/**
 * Modify a Verbs queue state.
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
		struct ibv_wq_attr mod = {
			.attr_mask = IBV_WQ_ATTR_STATE,
			.wq_state = sm->state,
		};
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[sm->queue_id];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct mlx5_rxq_ctrl, rxq);

		ret = mlx5_glue->modify_wq(rxq_ctrl->ibv->wq, &mod);
		if (ret) {
			DRV_LOG(ERR, "Cannot change Rx WQ state to %u  - %s\n",
					sm->state, strerror(errno));
			rte_errno = errno;
			return ret;
		}
	} else {
		struct mlx5_txq_data *txq = (*priv->txqs)[sm->queue_id];
		struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq, struct mlx5_txq_ctrl, txq);
		struct ibv_qp_attr mod = {
			.qp_state = IBV_QPS_RESET,
			.port_num = (uint8_t)priv->ibv_port,
		};
		struct ibv_qp *qp = txq_ctrl->ibv->qp;

		ret = mlx5_glue->modify_qp(qp, &mod, IBV_QP_STATE);
		if (ret) {
			DRV_LOG(ERR, "Cannot change the Tx QP state to RESET "
				"%s\n", strerror(errno));
			rte_errno = errno;
			return ret;
		}
		mod.qp_state = IBV_QPS_INIT;
		ret = mlx5_glue->modify_qp(qp, &mod,
					   (IBV_QP_STATE | IBV_QP_PORT));
		if (ret) {
			DRV_LOG(ERR, "Cannot change Tx QP state to INIT %s\n",
				strerror(errno));
			rte_errno = errno;
			return ret;
		}
		mod.qp_state = IBV_QPS_RTR;
		ret = mlx5_glue->modify_qp(qp, &mod, IBV_QP_STATE);
		if (ret) {
			DRV_LOG(ERR, "Cannot change Tx QP state to RTR %s\n",
				strerror(errno));
			rte_errno = errno;
			return ret;
		}
		mod.qp_state = IBV_QPS_RTS;
		ret = mlx5_glue->modify_qp(qp, &mod, IBV_QP_STATE);
		if (ret) {
			DRV_LOG(ERR, "Cannot change Tx QP state to RTS %s\n",
				strerror(errno));
			rte_errno = errno;
			return ret;
		}
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
	int ret = 0;

	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		ret = mlx5_queue_state_modify_primary(dev, sm);
		break;
	case RTE_PROC_SECONDARY:
		ret = mlx5_mp_req_queue_state_modify(dev, sm);
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
 * @param[in] mbuf_prepare
 *   Whether to prepare mbufs for the RQ.
 *
 * @return
 *   -1 in case of recovery error, otherwise the CQE status.
 */
int
mlx5_rx_err_handle(struct mlx5_rxq_data *rxq, uint8_t mbuf_prepare)
{
	const uint16_t cqe_n = 1 << rxq->cqe_n;
	const uint16_t cqe_mask = cqe_n - 1;
	const unsigned int wqe_n = 1 << rxq->elts_n;
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
			rte_cio_wmb();
			*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
			rte_cio_wmb();
			/*
			 * The RQ consumer index must be zeroed while moving
			 * from RESET state to RDY state.
			 */
			*rxq->rq_db = rte_cpu_to_be_32(0);
			rte_cio_wmb();
			sm.is_wq = 1;
			sm.queue_id = rxq->idx;
			sm.state = IBV_WQS_RDY;
			if (mlx5_queue_state_modify(ETH_DEV(rxq_ctrl->priv),
						    &sm))
				return -1;
			if (mbuf_prepare) {
				const uint16_t q_mask = wqe_n - 1;
				uint16_t elt_idx;
				struct rte_mbuf **elt;
				int i;
				unsigned int n = wqe_n - (rxq->rq_ci -
							  rxq->rq_pi);

				for (i = 0; i < (int)n; ++i) {
					elt_idx = (rxq->rq_ci + i) & q_mask;
					elt = &(*rxq->elts)[elt_idx];
					*elt = rte_mbuf_raw_alloc(rxq->mp);
					if (!*elt) {
						for (i--; i >= 0; --i) {
							elt_idx = (rxq->rq_ci +
								   i) & q_mask;
							elt = &(*rxq->elts)
								[elt_idx];
							rte_pktmbuf_free_seg
								(*elt);
						}
						return -1;
					}
				}
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
			++rxq->cq_ci;
			op_own = cqe->op_own;
			if (MLX5_CQE_FORMAT(op_own) == MLX5_COMPRESSED) {
				volatile struct mlx5_mini_cqe8 (*mc)[8] =
					(volatile struct mlx5_mini_cqe8 (*)[8])
					(uintptr_t)(&(*rxq->cqes)
						[rxq->cq_ci &
						 cqe_cnt].pkt_info);

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
				zip->ca = rxq->cq_ci;
				zip->na = zip->ca + 7;
				/* Compute the next non compressed CQE. */
				--rxq->cq_ci;
				zip->cq_ci = rxq->cq_ci + zip->cqe_cnt;
				/* Get packet size to return. */
				len = rte_be_to_cpu_32((*mc)[0].byte_cnt);
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
			pkt = seg;
			assert(len >= (rxq->crc_present << 2));
			pkt->ol_flags = 0;
			/* If compressed, take hash result from mini-CQE. */
			rss_hash_res = rte_be_to_cpu_32(mcqe == NULL ?
							cqe->rx_hash_res :
							mcqe->rx_hash_result);
			rxq_cq_to_mbuf(rxq, pkt, cqe, rss_hash_res);
			if (rxq->crc_present)
				len -= RTE_ETHER_CRC_LEN;
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
			len -= RTE_ETHER_CRC_LEN;
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
mlx5_rx_burst_vec(void *dpdk_txq __rte_unused,
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
	(void)tx_queue;
	(void)offset;
	return RTE_ETH_TX_DESC_FULL;
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
mlx5_tx_burst_tmpl(struct mlx5_txq_data *restrict txq,
		   struct rte_mbuf **restrict pkts,
		   uint16_t pkts_n,
		   unsigned int olx)
{
	(void)txq;
	(void)pkts;
	(void)pkts_n;
	(void)olx;
	return 0;
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
	assert(priv);
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
		 * The NIC supports Enhanced Multi-Packet Write.
		 * We do not support legacy MPW due to its
		 * hardware related problems, so we just ignore
		 * legacy MLX5_MPW settings. There should be no
		 * minimal required inline data.
		 */
		olx |= MLX5_TXOFF_CONFIG_EMPW;
	}
	if (tx_offloads & DEV_TX_OFFLOAD_MATCH_METADATA) {
		/* We should support Flow metadata. */
		olx |= MLX5_TXOFF_CONFIG_METADATA;
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
		if ((olx ^ tmp) & MLX5_TXOFF_CONFIG_EMPW)
			/* Do not enable eMPW if not configured. */
			continue;
		if ((olx ^ tmp) & MLX5_TXOFF_CONFIG_INLINE)
			/* Do not enable inlining if not configured. */
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
	if (txoff_func[m].olx & MLX5_TXOFF_CONFIG_EMPW)
		DRV_LOG(DEBUG, "\tEMPW  (Enhanced MPW)");
	return txoff_func[m].func;
}


