/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_PFVF_H_
#define _CXGBE_PFVF_H_

void cxgbe_dev_rx_queue_release(void *q);
void cxgbe_dev_tx_queue_release(void *q);
void cxgbe_dev_stop(struct rte_eth_dev *eth_dev);
void cxgbe_dev_close(struct rte_eth_dev *eth_dev);
void cxgbe_dev_info_get(struct rte_eth_dev *eth_dev,
			struct rte_eth_dev_info *device_info);
void cxgbe_dev_promiscuous_enable(struct rte_eth_dev *eth_dev);
void cxgbe_dev_promiscuous_disable(struct rte_eth_dev *eth_dev);
void cxgbe_dev_allmulticast_enable(struct rte_eth_dev *eth_dev);
void cxgbe_dev_allmulticast_disable(struct rte_eth_dev *eth_dev);
int cxgbe_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *addr);
int cxgbe_dev_configure(struct rte_eth_dev *eth_dev);
int cxgbe_dev_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t queue_idx,
			     uint16_t nb_desc, unsigned int socket_id,
			     const struct rte_eth_txconf *tx_conf);
int cxgbe_dev_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t queue_idx,
			     uint16_t nb_desc, unsigned int socket_id,
			     const struct rte_eth_rxconf *rx_conf,
			     struct rte_mempool *mp);
int cxgbe_dev_tx_queue_start(struct rte_eth_dev *eth_dev,
			     uint16_t tx_queue_id);
int cxgbe_dev_rx_queue_start(struct rte_eth_dev *eth_dev,
			     uint16_t tx_queue_id);
int cxgbe_dev_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id);
int cxgbe_dev_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id);
int cxgbe_dev_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu);
int cxgbe_dev_start(struct rte_eth_dev *eth_dev);
int cxgbe_dev_link_update(struct rte_eth_dev *eth_dev,
			  int wait_to_complete);
int cxgbe_dev_set_link_up(struct rte_eth_dev *dev);
int cxgbe_dev_set_link_down(struct rte_eth_dev *dev);
uint16_t cxgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts);
uint16_t cxgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts);
const uint32_t *cxgbe_dev_supported_ptypes_get(struct rte_eth_dev *eth_dev);
#endif /* _CXGBE_PFVF_H_ */
