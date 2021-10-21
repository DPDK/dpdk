/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_ETHDEV_H_
#define _NGBE_ETHDEV_H_

#include "ngbe_ptypes.h"

/* need update link, bit flag */
#define NGBE_FLAG_NEED_LINK_UPDATE  ((uint32_t)(1 << 0))
#define NGBE_FLAG_MAILBOX           ((uint32_t)(1 << 1))
#define NGBE_FLAG_PHY_INTERRUPT     ((uint32_t)(1 << 2))
#define NGBE_FLAG_MACSEC            ((uint32_t)(1 << 3))
#define NGBE_FLAG_NEED_LINK_CONFIG  ((uint32_t)(1 << 4))

#define NGBE_VFTA_SIZE 128
#define NGBE_VLAN_TAG_SIZE 4
/*Default value of Max Rx Queue*/
#define NGBE_MAX_RX_QUEUE_NUM	8

#ifndef NBBY
#define NBBY	8	/* number of bits in a byte */
#endif
#define NGBE_HWSTRIP_BITMAP_SIZE \
	(NGBE_MAX_RX_QUEUE_NUM / (sizeof(uint32_t) * NBBY))

#define NGBE_QUEUE_ITR_INTERVAL_DEFAULT	500 /* 500us */

/* The overhead from MTU to max frame size. */
#define NGBE_ETH_OVERHEAD (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN)

#define NGBE_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define NGBE_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

/* structure for interrupt relative data */
struct ngbe_interrupt {
	uint32_t flags;
	uint32_t mask_misc;
	uint32_t mask_misc_orig; /* save mask during delayed handler */
	uint64_t mask;
	uint64_t mask_orig; /* save mask during delayed handler */
};

struct ngbe_vfta {
	uint32_t vfta[NGBE_VFTA_SIZE];
};

struct ngbe_hwstrip {
	uint32_t bitmap[NGBE_HWSTRIP_BITMAP_SIZE];
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct ngbe_adapter {
	struct ngbe_hw             hw;
	struct ngbe_interrupt      intr;
	struct ngbe_vfta           shadow_vfta;
	struct ngbe_hwstrip        hwstrip;
	bool                       rx_bulk_alloc_allowed;
};

static inline struct ngbe_adapter *
ngbe_dev_adapter(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = dev->data->dev_private;

	return ad;
}

static inline struct ngbe_hw *
ngbe_dev_hw(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = ngbe_dev_adapter(dev);
	struct ngbe_hw *hw = &ad->hw;

	return hw;
}

static inline struct ngbe_interrupt *
ngbe_dev_intr(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = ngbe_dev_adapter(dev);
	struct ngbe_interrupt *intr = &ad->intr;

	return intr;
}

#define NGBE_DEV_VFTA(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->shadow_vfta)

#define NGBE_DEV_HWSTRIP(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->hwstrip)

/*
 * Rx/Tx function prototypes
 */
void ngbe_dev_clear_queues(struct rte_eth_dev *dev);

void ngbe_dev_free_queues(struct rte_eth_dev *dev);

void ngbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

void ngbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

int  ngbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int  ngbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int ngbe_dev_rx_init(struct rte_eth_dev *dev);

void ngbe_dev_tx_init(struct rte_eth_dev *dev);

int ngbe_dev_rxtx_start(struct rte_eth_dev *dev);

void ngbe_dev_save_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id);
void ngbe_dev_store_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id);
void ngbe_dev_save_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id);
void ngbe_dev_store_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id);

int ngbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int ngbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int ngbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int ngbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int
ngbe_rx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode);
int
ngbe_tx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode);

uint16_t ngbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t ngbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
				    uint16_t nb_pkts);

uint16_t ngbe_recv_pkts_sc_single_alloc(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t ngbe_recv_pkts_sc_bulk_alloc(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t ngbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t ngbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t ngbe_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

void ngbe_set_ivar_map(struct ngbe_hw *hw, int8_t direction,
			       uint8_t queue, uint8_t msix_vector);

void ngbe_configure_port(struct rte_eth_dev *dev);

int
ngbe_dev_link_update_share(struct rte_eth_dev *dev,
		int wait_to_complete);

/*
 * misc function prototypes
 */
void ngbe_vlan_hw_filter_enable(struct rte_eth_dev *dev);

void ngbe_vlan_hw_filter_disable(struct rte_eth_dev *dev);

void ngbe_vlan_hw_strip_config(struct rte_eth_dev *dev);

#define NGBE_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define NGBE_LINK_UP_CHECK_TIMEOUT   1000 /* ms */
#define NGBE_VMDQ_NUM_UC_MAC         4096 /* Maximum nb. of UC MAC addr. */

/*
 *  Default values for Rx/Tx configuration
 */
#define NGBE_DEFAULT_RX_FREE_THRESH  32
#define NGBE_DEFAULT_RX_PTHRESH      8
#define NGBE_DEFAULT_RX_HTHRESH      8
#define NGBE_DEFAULT_RX_WTHRESH      0

#define NGBE_DEFAULT_TX_FREE_THRESH  32
#define NGBE_DEFAULT_TX_PTHRESH      32
#define NGBE_DEFAULT_TX_HTHRESH      0
#define NGBE_DEFAULT_TX_WTHRESH      0

const uint32_t *ngbe_dev_supported_ptypes_get(struct rte_eth_dev *dev);
void ngbe_vlan_hw_strip_bitmap_set(struct rte_eth_dev *dev,
		uint16_t queue, bool on);
void ngbe_config_vlan_strip_on_all_queues(struct rte_eth_dev *dev,
						  int mask);

#endif /* _NGBE_ETHDEV_H_ */
