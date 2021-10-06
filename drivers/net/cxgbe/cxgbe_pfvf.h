/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_PFVF_H_
#define _CXGBE_PFVF_H_

#define CXGBE_FW_PARAM_DEV(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_##param))

#define CXGBE_FW_PARAM_PFVF(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_##param) |  \
	 V_FW_PARAMS_PARAM_Y(0) | \
	 V_FW_PARAMS_PARAM_Z(0))

void cxgbe_dev_rx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid);
void cxgbe_dev_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid);
int cxgbe_dev_stop(struct rte_eth_dev *eth_dev);
int cxgbe_dev_close(struct rte_eth_dev *eth_dev);
int cxgbe_dev_info_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_dev_info *device_info);
int cxgbe_dev_promiscuous_enable(struct rte_eth_dev *eth_dev);
int cxgbe_dev_promiscuous_disable(struct rte_eth_dev *eth_dev);
int cxgbe_dev_allmulticast_enable(struct rte_eth_dev *eth_dev);
int cxgbe_dev_allmulticast_disable(struct rte_eth_dev *eth_dev);
int cxgbe_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *addr);
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
int cxgbe_dev_xstats_get_by_id(struct rte_eth_dev *dev,
			       const uint64_t *ids, uint64_t *values,
			       unsigned int n);
int cxgbe_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
				     const uint64_t *ids,
				     struct rte_eth_xstat_name *xnames,
				     unsigned int n);
int cxgbe_dev_xstats_get_names(struct rte_eth_dev *dev,
			       struct rte_eth_xstat_name *xstats_names,
			       unsigned int n);
int cxgbe_dev_xstats_get(struct rte_eth_dev *dev,
			 struct rte_eth_xstat *xstats, unsigned int n);
int cxgbe_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
			 size_t fw_size);
#endif /* _CXGBE_PFVF_H_ */
