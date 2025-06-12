/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _IXGBE_RXTX_H_
#define _IXGBE_RXTX_H_

#include "ixgbe_type.h"

#include "../common/rx.h"
#include "../common/tx.h"

/*
 * Rings setup and release.
 *
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary. This will
 * also optimize cache line size effect. H/W supports up to cache line size 128.
 */
#define	IXGBE_ALIGN	128

#define IXGBE_RXD_ALIGN	(IXGBE_ALIGN / sizeof(union ixgbe_adv_rx_desc))
#define IXGBE_TXD_ALIGN	(IXGBE_ALIGN / sizeof(union ixgbe_adv_tx_desc))

/*
 * Maximum number of Ring Descriptors.
 *
 * Since RDLEN/TDLEN should be multiple of 128 bytes, the number of ring
 * descriptors should meet the following condition:
 *      (num_ring_desc * sizeof(rx/tx descriptor)) % 128 == 0
 */
#define	IXGBE_MIN_RING_DESC	64
#define	IXGBE_MAX_RING_DESC	8192

#define IXGBE_TX_MAX_BURST            32
#define IXGBE_RX_MAX_BURST            CI_RX_MAX_BURST
#define IXGBE_TX_MAX_FREE_BUF_SZ      64

#define IXGBE_VPMD_DESCS_PER_LOOP     4

#define IXGBE_VPMD_RXQ_REARM_THRESH   32
#define IXGBE_VPMD_RX_BURST           IXGBE_VPMD_RXQ_REARM_THRESH

#define RX_RING_SZ ((IXGBE_MAX_RING_DESC + IXGBE_RX_MAX_BURST) * \
		    sizeof(union ixgbe_adv_rx_desc))

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)  rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)  do {} while(0)
#endif

#define IXGBE_REGISTER_POLL_WAIT_10_MS  10
#define IXGBE_WAIT_100_US               100
#define IXGBE_VMTXSW_REGISTER_COUNT     2

#define IXGBE_TX_MAX_SEG                    40

#define IXGBE_TX_MIN_PKT_LEN		     14

#define IXGBE_PACKET_TYPE_MASK_82599        0X7F
#define IXGBE_PACKET_TYPE_MASK_X550         0X10FF
#define IXGBE_PACKET_TYPE_MASK_TUNNEL       0XFF
#define IXGBE_PACKET_TYPE_TUNNEL_BIT        0X1000

#define IXGBE_PACKET_TYPE_MAX               0X80
#define IXGBE_PACKET_TYPE_TN_MAX            0X100
#define IXGBE_PACKET_TYPE_SHIFT             0X04

/**
 * IXGBE CTX Constants
 */
enum ixgbe_advctx_num {
	IXGBE_CTX_0    = 0, /**< CTX0 */
	IXGBE_CTX_1    = 1, /**< CTX1  */
	IXGBE_CTX_NUM  = 2, /**< CTX NUMBER  */
};

/** Offload features */
union ixgbe_tx_offload {
	uint64_t data[2];
	struct {
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size */
		uint64_t vlan_tci:16;
		/**< VLAN Tag Control Identifier (CPU order). */

		/* fields for TX offloading of tunnels */
		uint64_t outer_l3_len:8; /**< Outer L3 (IP) Hdr Length. */
		uint64_t outer_l2_len:8; /**< Outer L2 (MAC) Hdr Length. */
#ifdef RTE_LIB_SECURITY
		/* inline ipsec related*/
		uint64_t sa_idx:8;	/**< TX SA database entry index */
		uint64_t sec_pad_len:4;	/**< padding length */
#endif
	};
};

/*
 * Compare mask for vlan_macip_len.data,
 * should be in sync with ixgbe_vlan_macip.f layout.
 * */
#define TX_VLAN_CMP_MASK        0xFFFF0000  /**< VLAN length - 16-bits. */
#define TX_MAC_LEN_CMP_MASK     0x0000FE00  /**< MAC length - 7-bits. */
#define TX_IP_LEN_CMP_MASK      0x000001FF  /**< IP  length - 9-bits. */
/** MAC+IP  length. */
#define TX_MACIP_LEN_CMP_MASK   (TX_MAC_LEN_CMP_MASK | TX_IP_LEN_CMP_MASK)

/**
 * Structure to check if new context need be built
 */

struct ixgbe_advctx_info {
	uint64_t flags;           /**< ol_flags for context build. */
	/**< tx offload: vlan, tso, l2-l3-l4 lengths. */
	union ixgbe_tx_offload tx_offload;
	/** compare mask for tx offload. */
	union ixgbe_tx_offload tx_offload_mask;
};

struct ixgbe_txq_ops {
	void (*free_swring)(struct ci_tx_queue *txq);
	void (*reset)(struct ci_tx_queue *txq);
};

/*
 * Populate descriptors with the following info:
 * 1.) buffer_addr = phys_addr + headroom
 * 2.) cmd_type_len = DCMD_DTYP_FLAGS | pkt_len
 * 3.) olinfo_status = pkt_len << PAYLEN_SHIFT
 */

/* Defines for Tx descriptor */
#define DCMD_DTYP_FLAGS (IXGBE_ADVTXD_DTYP_DATA |\
			 IXGBE_ADVTXD_DCMD_IFCS |\
			 IXGBE_ADVTXD_DCMD_DEXT |\
			 IXGBE_ADVTXD_DCMD_EOP)


/* Takes an ethdev and a queue and sets up the tx function to be used based on
 * the queue parameters. Used in tx_queue_setup by primary process and then
 * in dev_init by secondary process when attaching to an existing ethdev.
 */
void ixgbe_set_tx_function(struct rte_eth_dev *dev, struct ci_tx_queue *txq);
int ixgbe_tx_burst_mode_get(struct rte_eth_dev *dev,
		uint16_t queue_id, struct rte_eth_burst_mode *mode);

/**
 * Sets the rx_pkt_burst callback in the ixgbe rte_eth_dev instance.
 *
 * Sets the callback based on the device parameters:
 *  - ixgbe_hw.rx_bulk_alloc_allowed
 *  - rte_eth_dev_data.scattered_rx
 *  - rte_eth_dev_data.lro
 *  - conditions checked in ixgbe_rx_vec_condition_check()
 *
 *  This means that the parameters above have to be configured prior to calling
 *  to this function.
 *
 * @dev rte_eth_dev handle
 */
void ixgbe_set_rx_function(struct rte_eth_dev *dev);
int ixgbe_rx_burst_mode_get(struct rte_eth_dev *dev,
		uint16_t queue_id, struct rte_eth_burst_mode *mode);

int ixgbe_check_supported_loopback_mode(struct rte_eth_dev *dev);
int ixgbe_dev_tx_done_cleanup(void *tx_queue, uint32_t free_cnt);

extern const uint32_t ptype_table[IXGBE_PACKET_TYPE_MAX];
extern const uint32_t ptype_table_tn[IXGBE_PACKET_TYPE_TN_MAX];

int ixgbe_write_default_ctx_desc(struct ci_tx_queue *txq, struct rte_mempool *mp, bool vec);
uint16_t ixgbe_recycle_tx_mbufs_reuse_vec(void *tx_queue,
		struct rte_eth_recycle_rxq_info *recycle_rxq_info);
void ixgbe_recycle_rx_descriptors_refill_vec(void *rx_queue, uint16_t nb_mbufs);

uint16_t ixgbe_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
				    uint16_t nb_pkts);
int ixgbe_write_default_ctx_desc(struct ci_tx_queue *txq, struct rte_mempool *mp, bool vec);

uint64_t ixgbe_get_tx_port_offloads(struct rte_eth_dev *dev);
uint64_t ixgbe_get_rx_queue_offloads(struct rte_eth_dev *dev);
uint64_t ixgbe_get_rx_port_offloads(struct rte_eth_dev *dev);
uint64_t ixgbe_get_tx_queue_offloads(struct rte_eth_dev *dev);
int ixgbe_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc);

/**
 * Check if the Tx descriptor DD bit is set.
 *
 * @param txq
 *   Pointer to the Tx queue structure.
 * @param idx
 *   Index of the Tx descriptor to check.
 *
 * @return
 *   1 if the Tx descriptor is done, 0 otherwise.
 */
static inline int
ixgbe_tx_desc_done(struct ci_tx_queue *txq, uint16_t idx)
{
	const uint32_t status = txq->ixgbe_tx_ring[idx].wb.status;

	return !!(status & rte_cpu_to_le_32(IXGBE_ADVTXD_STAT_DD));
}

#endif /* _IXGBE_RXTX_H_ */
