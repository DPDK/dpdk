/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its 
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
 * 
 */

#ifndef _IXGBE_ETHDEV_H_
#define _IXGBE_ETHDEV_H_

/* need update link, bit flag */
#define IXGBE_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)

/*
 * Defines that were not part of ixgbe_type.h as they are not used by the
 * FreeBSD driver.
 */
#define IXGBE_ADVTXD_MAC_1588       0x00080000 /* IEEE1588 Timestamp packet */
#define IXGBE_RXD_STAT_TMST         0x10000    /* Timestamped Packet indication */
#define IXGBE_ADVTXD_TUCMD_L4T_RSV  0x00001800 /* L4 Packet TYPE, resvd  */
#define IXGBE_RXDADV_ERR_CKSUM_BIT  30
#define IXGBE_RXDADV_ERR_CKSUM_MSK  3
#define IXGBE_ADVTXD_MACLEN_SHIFT   9          /* Bit shift for l2_len */

#define IXGBE_VFTA_SIZE 128

/*
 * Information about the fdir mode.
 */
struct ixgbe_hw_fdir_info {
	uint16_t    collision;
	uint16_t    free;
	uint16_t    maxhash;
	uint8_t     maxlen;
	uint64_t    add;
	uint64_t    remove;
	uint64_t    f_add;
	uint64_t    f_remove;
};

/* structure for interrupt relative data */
struct ixgbe_interrupt {
	uint32_t flags;
};

/* local VFTA copy */
struct ixgbe_vfta {
	uint32_t vfta[IXGBE_VFTA_SIZE];
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct ixgbe_adapter {
	struct ixgbe_hw             hw;
	struct ixgbe_hw_stats       stats;
	struct ixgbe_hw_fdir_info   fdir;
	struct ixgbe_interrupt      intr;
	struct ixgbe_vfta           shadow_vfta;
};

#define IXGBE_DEV_PRIVATE_TO_HW(adapter)\
	(&((struct ixgbe_adapter *)adapter)->hw)

#define IXGBE_DEV_PRIVATE_TO_STATS(adapter) \
	(&((struct ixgbe_adapter *)adapter)->stats)

#define IXGBE_DEV_PRIVATE_TO_INTR(adapter) \
	(&((struct ixgbe_adapter *)adapter)->intr)

#define IXGBE_DEV_PRIVATE_TO_FDIR_INFO(adapter) \
	(&((struct ixgbe_adapter *)adapter)->fdir)

#define IXGBE_DEV_PRIVATE_TO_VFTA(adapter) \
	(&((struct ixgbe_adapter *)adapter)->shadow_vfta)


/*
 * RX/TX function prototypes
 */
void ixgbe_dev_clear_queues(struct rte_eth_dev *dev);

int ixgbe_dev_rx_queue_alloc(struct rte_eth_dev *dev, uint16_t nb_rx_queues);

int ixgbe_dev_tx_queue_alloc(struct rte_eth_dev *dev, uint16_t nb_tx_queues);

int  ixgbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int  ixgbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int ixgbe_dev_rx_init(struct rte_eth_dev *dev);

void ixgbe_dev_tx_init(struct rte_eth_dev *dev);

void ixgbe_dev_rxtx_start(struct rte_eth_dev *dev);

int ixgbevf_dev_rx_init(struct rte_eth_dev *dev);

void ixgbevf_dev_tx_init(struct rte_eth_dev *dev);

void ixgbevf_dev_rxtx_start(struct rte_eth_dev *dev);

uint16_t ixgbe_recv_pkts(struct igb_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t ixgbe_recv_scattered_pkts(struct igb_rx_queue *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t ixgbe_xmit_pkts(struct igb_tx_queue *txq, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

/*
 * Flow director function prototypes
 */
int ixgbe_fdir_configure(struct rte_eth_dev *dev);

int ixgbe_fdir_add_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint8_t queue);

int ixgbe_fdir_update_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint8_t queue);

int ixgbe_fdir_remove_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter);

void ixgbe_fdir_info_get(struct rte_eth_dev *dev,
		struct rte_eth_fdir *fdir);

int ixgbe_fdir_add_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint16_t soft_id,
		uint8_t queue, uint8_t drop);

int ixgbe_fdir_update_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter,uint16_t soft_id,
		uint8_t queue, uint8_t drop);

int ixgbe_fdir_remove_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint16_t soft_id);

int ixgbe_fdir_set_masks(struct rte_eth_dev *dev,
		struct rte_fdir_masks *fdir_masks);

#endif /* _IXGBE_ETHDEV_H_ */
