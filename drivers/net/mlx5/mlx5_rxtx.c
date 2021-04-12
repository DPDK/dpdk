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
#include "mlx5_rx.h"
#include "mlx5_tx.h"

#define MLX5_TXOFF_INFO(func, olx) {mlx5_tx_burst_##func, olx},

/* static asserts */
static_assert(MLX5_CQE_STATUS_HW_OWN < 0, "Must be negative value");
static_assert(MLX5_CQE_STATUS_SW_OWN < 0, "Must be negative value");
static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
		(sizeof(uint16_t) +
		 sizeof(rte_v128u32_t)),
		"invalid Ethernet Segment data size");
static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
		(sizeof(uint16_t) +
		 sizeof(struct rte_vlan_hdr) +
		 2 * RTE_ETHER_ADDR_LEN),
		"invalid Ethernet Segment data size");
static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
		(sizeof(uint16_t) +
		 sizeof(rte_v128u32_t)),
		"invalid Ethernet Segment data size");
static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
		(sizeof(uint16_t) +
		 sizeof(struct rte_vlan_hdr) +
		 2 * RTE_ETHER_ADDR_LEN),
		"invalid Ethernet Segment data size");
static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
		(sizeof(uint16_t) +
		 sizeof(rte_v128u32_t)),
		"invalid Ethernet Segment data size");
static_assert(MLX5_ESEG_MIN_INLINE_SIZE ==
		(sizeof(uint16_t) +
		 sizeof(struct rte_vlan_hdr) +
		 2 * RTE_ETHER_ADDR_LEN),
		"invalid Ethernet Segment data size");
static_assert(MLX5_DSEG_MIN_INLINE_SIZE ==
		(2 * RTE_ETHER_ADDR_LEN),
		"invalid Data Segment data size");
static_assert(MLX5_EMPW_MIN_PACKETS >= 2, "invalid min size");
static_assert(MLX5_EMPW_MIN_PACKETS >= 2, "invalid min size");
static_assert((sizeof(struct rte_vlan_hdr) +
			sizeof(struct rte_ether_hdr)) ==
		MLX5_ESEG_MIN_INLINE_SIZE,
		"invalid min inline data size");
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

uint32_t mlx5_ptype_table[] __rte_cache_aligned = {
	[0xff] = RTE_PTYPE_ALL_MASK, /* Last entry for errored packet. */
};

uint8_t mlx5_cksum_table[1 << 10] __rte_cache_aligned;
uint8_t mlx5_swp_types_table[1 << 10] __rte_cache_aligned;

uint64_t rte_net_mlx5_dynf_inline_mask;

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
int
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
void
mlx5_tx_handle_completion(struct mlx5_txq_data *__rte_restrict txq,
			  unsigned int olx __rte_unused)
{
	unsigned int count = MLX5_TX_COMP_MAX_CQE;
	volatile struct mlx5_cqe *last_cqe = NULL;
	bool ring_doorbell = false;
	int ret;

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
		       uint16_t tx_queue_id,
		       struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq = (*priv->txqs)[tx_queue_id];
	unsigned int i, olx;

	for (i = 0; i < RTE_DIM(txoff_func); i++) {
		if (pkt_burst == txoff_func[i].func) {
			olx = txoff_func[i].olx;
			snprintf(mode->info, sizeof(mode->info),
				 "%s%s%s%s%s%s%s%s%s%s",
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
				 " + TXPP" : "",
				 (txq && txq->fast_free) ?
				 " + Fast Free" : "");
			return 0;
		}
	}
	return -EINVAL;
}
