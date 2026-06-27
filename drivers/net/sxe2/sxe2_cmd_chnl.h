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

int32_t sxe2_drv_allmulti_config(struct sxe2_adapter *adapter, bool set);

int32_t sxe2_drv_uc_config(struct sxe2_adapter *adapter, struct rte_ether_addr *addr, bool add);

int32_t sxe2_drv_mc_config(struct sxe2_adapter *adapter, struct rte_ether_addr *addr, bool add);

int32_t sxe2_drv_vlan_config_query(struct sxe2_adapter *adapter);

int32_t sxe2_drv_vlan_filter_id_config(struct sxe2_adapter *adapter,
				       struct sxe2_vlan *vlan, bool on);

int32_t sxe2_drv_vlan_insert_strip_cfg(struct sxe2_adapter *adapter);

int32_t sxe2_drv_vlan_filter_switch(struct sxe2_adapter *adapter, bool on);

#endif /* SXE2_CMD_CHNL_H */
