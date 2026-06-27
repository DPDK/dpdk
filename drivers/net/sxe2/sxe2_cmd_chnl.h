/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_CMD_CHNL_H
#define SXE2_CMD_CHNL_H

#include "sxe2_ethdev.h"
#include "sxe2_drv_cmd.h"
#include "sxe2_ioctl_chnl_func.h"

int32_t sxe2_drv_dev_caps_get(struct sxe2_adapter *adapter,
		struct sxe2_drv_dev_caps_resp *dev_caps);

int32_t sxe2_drv_dev_info_get(struct sxe2_adapter *adapter,
		struct sxe2_drv_dev_info_resp *dev_info_resp);

int32_t sxe2_drv_dev_fw_info_get(struct sxe2_adapter *adapter,
		struct sxe2_drv_dev_fw_info_resp *dev_fw_info_resp);

int32_t sxe2_drv_vsi_add(struct sxe2_adapter *adapter, struct sxe2_vsi *vsi);

int32_t sxe2_drv_vsi_del(struct sxe2_adapter *adapter, struct sxe2_vsi *vsi);

int32_t sxe2_drv_rxq_switch(struct sxe2_adapter *adapter, struct sxe2_rx_queue *rxq, bool enable);

int32_t sxe2_drv_txq_switch(struct sxe2_adapter *adapter, struct sxe2_tx_queue *txq, bool enable);

int32_t sxe2_drv_rxq_ctxt_cfg(struct sxe2_adapter *adapter,
			      struct sxe2_rx_queue *rxq,
			      uint16_t rxq_cnt);

int32_t sxe2_drv_txq_ctxt_cfg(struct sxe2_adapter *adapter,
			      struct sxe2_tx_queue *txq,
			      uint16_t txq_cnt);

int32_t sxe2_drv_mac_link_status_get(struct sxe2_adapter *adapter);

int32_t sxe2_drv_promisc_config(struct sxe2_adapter *adapter, bool set);

int32_t sxe2_drv_root_tree_release(struct rte_eth_dev *dev);

int32_t sxe2_drv_root_tree_alloc(struct rte_eth_dev *dev);

int32_t sxe2_drv_tm_commit(struct sxe2_adapter *adapter);

int32_t sxe2_drv_ipsec_resource_clear(struct sxe2_adapter *adapter);

int32_t sxe2_drv_ipsec_get_capa(struct sxe2_adapter *adapter);

int32_t sxe2_drv_ipsec_rxsa_add(struct sxe2_adapter *adapter,
			    struct sxe2_ipsec_rx_sa *rx_sa,
			    struct sxe2_ipsec_rx_tcam *rx_tcam,
			    struct sxe2_ipsec_rx_udp_group *rx_udp_group);

int32_t sxe2_drv_ipsec_txsa_add(struct sxe2_adapter *adapter,
			    struct sxe2_ipsec_tx_sa *tx_sa);

int32_t sxe2_drv_ipsec_rxsa_delete(struct sxe2_adapter *adapter,
			       struct sxe2_ipsec_rx_sa *rx_sa);

int32_t sxe2_drv_ipsec_txsa_delete(struct sxe2_adapter *adapter,
			       uint16_t sa_id);

int32_t sxe2_drv_promisc_config(struct sxe2_adapter *adapter, bool set);

int32_t sxe2_drv_mac_link_status_get(struct sxe2_adapter *adapter);

int32_t sxe2_drv_get_mac_stats(struct sxe2_adapter *adapter);

int32_t sxe2_drv_mac_stats_reset(struct sxe2_adapter *adapter);

int32_t sxe2_drv_get_vsi_stats(struct sxe2_adapter *adapter);

int32_t sxe2_drv_vsi_stats_reset(struct sxe2_adapter *adapter);

int32_t sxe2_drv_queue_info_get_update(struct sxe2_adapter *adapter,
				       struct eth_queue_stats *qstats);

int32_t sxe2_drv_rxq_mapping_set(struct rte_eth_dev *eth_dev, uint16_t queue_id, uint8_t pool_idx);

int32_t sxe2_drv_txq_mapping_set(struct rte_eth_dev *eth_dev, uint16_t queue_id, uint8_t pool_idx);

int32_t sxe2_drv_mapping_reset(struct rte_eth_dev *eth_dev);

int32_t sxe2_drv_mapping_stats_info_clear(struct rte_eth_dev *eth_dev);

int32_t sxe2_drv_rxq_mapping_set(struct rte_eth_dev *eth_dev, uint16_t queue_id, uint8_t pool_idx);

int32_t sxe2_drv_allmulti_config(struct sxe2_adapter *adapter, bool set);

int32_t sxe2_drv_uc_config(struct sxe2_adapter *adapter, struct rte_ether_addr *addr, bool add);

int32_t sxe2_drv_mc_config(struct sxe2_adapter *adapter, struct rte_ether_addr *addr, bool add);

int32_t sxe2_drv_vlan_config_query(struct sxe2_adapter *adapter);

int32_t sxe2_drv_vlan_filter_id_config(struct sxe2_adapter *adapter,
				       struct sxe2_vlan *vlan, bool on);

int32_t sxe2_drv_vlan_insert_strip_cfg(struct sxe2_adapter *adapter);

int32_t sxe2_drv_vlan_filter_switch(struct sxe2_adapter *adapter, bool on);

int32_t sxe2_drv_rss_key_set(struct sxe2_adapter *adapter, uint8_t *key, uint16_t key_size);

int32_t sxe2_drv_rss_lut_set(struct sxe2_adapter *adapter, uint8_t *lut, uint16_t lut_size);

int32_t sxe2_drv_rss_hash_ctrl_func(struct sxe2_adapter *adapter, enum sxe2_rss_hash_key_func func);

int32_t sxe2_drv_rss_hf_add(struct sxe2_adapter *adapter,
			struct sxe2_rss_hf_config *rss_conf);

int32_t sxe2_drv_rss_hf_del(struct sxe2_adapter *adapter,
				struct sxe2_rss_hf_config *rss_conf);

int32_t sxe2_drv_rss_hf_clear(struct sxe2_adapter *adapter);

int32_t sxe2_drv_ptp_gettime(struct sxe2_adapter *adapter, struct sxe2_rx_queue *rxq);

int32_t sxe2_drv_rxq_bind_irq(struct sxe2_adapter *adapter, uint16_t rxq_idx, uint16_t msix_idx);

int32_t sxe2_drv_rxq_unbind_irq(struct sxe2_adapter *adapter, uint16_t rxq_idx);

#endif /* SXE2_CMD_CHNL_H */
