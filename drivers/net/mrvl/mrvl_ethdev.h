/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#ifndef _MRVL_ETHDEV_H_
#define _MRVL_ETHDEV_H_

#include <rte_spinlock.h>
#include <rte_flow_driver.h>

#include <env/mv_autogen_comp_flags.h>
#include <drivers/mv_pp2.h>
#include <drivers/mv_pp2_bpool.h>
#include <drivers/mv_pp2_cls.h>
#include <drivers/mv_pp2_hif.h>
#include <drivers/mv_pp2_ppio.h>

/** Maximum number of rx queues per port */
#define MRVL_PP2_RXQ_MAX 32

/** Maximum number of tx queues per port */
#define MRVL_PP2_TXQ_MAX 8

/** Minimum number of descriptors in tx queue */
#define MRVL_PP2_TXD_MIN 16

/** Maximum number of descriptors in tx queue */
#define MRVL_PP2_TXD_MAX 2048

/** Tx queue descriptors alignment */
#define MRVL_PP2_TXD_ALIGN 16

/** Minimum number of descriptors in rx queue */
#define MRVL_PP2_RXD_MIN 16

/** Maximum number of descriptors in rx queue */
#define MRVL_PP2_RXD_MAX 2048

/** Rx queue descriptors alignment */
#define MRVL_PP2_RXD_ALIGN 16

/** Maximum number of descriptors in tx aggregated queue */
#define MRVL_PP2_AGGR_TXQD_MAX 2048

/** Maximum number of Traffic Classes. */
#define MRVL_PP2_TC_MAX 8

/** Packet offset inside RX buffer. */
#define MRVL_PKT_OFFS 64

/** Maximum number of descriptors in shadow queue. Must be power of 2 */
#define MRVL_PP2_TX_SHADOWQ_SIZE MRVL_PP2_TXD_MAX

/** Shadow queue size mask (since shadow queue size is power of 2) */
#define MRVL_PP2_TX_SHADOWQ_MASK (MRVL_PP2_TX_SHADOWQ_SIZE - 1)

/** Minimum number of sent buffers to release from shadow queue to BM */
#define MRVL_PP2_BUF_RELEASE_BURST_SIZE	64

struct mrvl_priv {
	/* Hot fields, used in fast path. */
	struct pp2_bpool *bpool;  /**< BPool pointer */
	struct pp2_ppio	*ppio;    /**< Port handler pointer */
	rte_spinlock_t lock;	  /**< Spinlock for checking bpool status */
	uint16_t bpool_max_size;  /**< BPool maximum size */
	uint16_t bpool_min_size;  /**< BPool minimum size  */
	uint16_t bpool_init_size; /**< Configured BPool size  */

	/** Mapping for DPDK rx queue->(TC, MRVL relative inq) */
	struct {
		uint8_t tc;  /**< Traffic Class */
		uint8_t inq; /**< Relative in-queue number */
	} rxq_map[MRVL_PP2_RXQ_MAX] __rte_cache_aligned;

	/* Configuration data, used sporadically. */
	uint8_t pp_id;
	uint8_t ppio_id;
	uint8_t bpool_bit;
	uint8_t rss_hf_tcp;
	uint8_t uc_mc_flushed;
	uint8_t vlan_flushed;
	uint8_t isolated;

	struct pp2_ppio_params ppio_params;
	struct pp2_cls_qos_tbl_params qos_tbl_params;
	struct pp2_cls_tbl *qos_tbl;
	uint16_t nb_rx_queues;

	struct pp2_cls_tbl_params cls_tbl_params;
	struct pp2_cls_tbl *cls_tbl;
	uint32_t cls_tbl_pattern;
	LIST_HEAD(mrvl_flows, rte_flow) flows;

	struct pp2_cls_plcr *policer;
};

/** Flow operations forward declaration. */
extern const struct rte_flow_ops mrvl_flow_ops;
#endif /* _MRVL_ETHDEV_H_ */
