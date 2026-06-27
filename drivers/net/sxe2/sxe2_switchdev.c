/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_os.h>
#include <rte_tailq.h>
#include "sxe2_osal.h"
#include "sxe2_mac.h"
#include "sxe2_common_log.h"
#include "sxe2_ethdev.h"
#include "sxe2_switchdev.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_host_regs.h"

int32_t sxe2_uplink_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_INFO(adapter, DRV, "Current mode is not switchdev");
		goto l_end;
	}

	ret = sxe2_drv_switchdev_uplink_config(adapter, false);
l_end:
	return ret;
}

int32_t sxe2_uplink_set(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_INFO(adapter, DRV, "Current mode is not switchdev");
		goto l_end;
	}

	ret = sxe2_drv_switchdev_uplink_config(adapter, true);
l_end:
	return ret;
}

int32_t sxe2_repr_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	uint16_t repr_id = 0;
	struct rte_eth_dev *repr_dev;
	struct sxe2_adapter *repr_adapter;
	struct sxe2_switchdev_repr_info repr_vf;

	if (!adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_INFO(adapter, DRV, "Current mode is not switchdev");
		goto l_end;
	}

	for (repr_id = 0; repr_id < adapter->repr_ctxt.nb_repr_vf; repr_id++) {
		repr_dev = adapter->repr_ctxt.vf_rep_eth_dev[repr_id];
		if (!repr_dev)
			continue;
		repr_adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(repr_dev);
		if (repr_adapter &&
		    repr_adapter->repr_priv_data &&
		    repr_adapter->repr_priv_data->cp_vsi) {
			memset(&repr_vf, 0, sizeof(struct sxe2_switchdev_repr_info));

			repr_vf.repr_pf_id = repr_adapter->repr_priv_data->repr_pf_id;
			repr_vf.repr_vf_id = repr_adapter->repr_priv_data->repr_vf_id;
			repr_vf.cp_vsi_id = repr_adapter->repr_priv_data->cp_vsi->vsi_id;
			repr_vf.repr_q_id = repr_adapter->repr_priv_data->repr_q_id;
			ret = sxe2_drv_switchdev_repr_vf_config(adapter, &repr_vf,
								false);
		}
	}
l_end:
	return ret;
}

int32_t sxe2_repr_set(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	uint16_t repr_id = 0;
	struct rte_eth_dev *repr_dev;
	struct sxe2_adapter *repr_adapter;
	struct sxe2_switchdev_repr_info repr_vf;

	if (!adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_INFO(adapter, DRV, "Current mode is not switchdev");
		goto l_end;
	}

	for (repr_id = 0; repr_id < adapter->repr_ctxt.nb_repr_vf; repr_id++) {
		repr_dev = adapter->repr_ctxt.vf_rep_eth_dev[repr_id];
		if (!repr_dev)
			continue;
		repr_adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(repr_dev);
		if (repr_adapter &&
		    repr_adapter->repr_priv_data &&
		    repr_adapter->repr_priv_data->cp_vsi) {
			memset(&repr_vf, 0, sizeof(struct sxe2_switchdev_repr_info));

			repr_vf.repr_pf_id = repr_adapter->repr_priv_data->repr_pf_id;
			repr_vf.repr_vf_id = repr_adapter->repr_priv_data->repr_vf_id;
			repr_vf.cp_vsi_id = repr_adapter->repr_priv_data->cp_vsi->vsi_id;
			repr_vf.repr_q_id = repr_adapter->repr_priv_data->repr_q_id;
			ret = sxe2_drv_switchdev_repr_vf_config(adapter, &repr_vf, true);
		}
	}

l_end:
	return ret;
}

void sxe2_free_repr_info(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	if (adapter->repr_ctxt.vf_rep_eth_dev) {
		rte_free(adapter->repr_ctxt.vf_rep_eth_dev);
		adapter->repr_ctxt.vf_rep_eth_dev = NULL;
	}

	adapter->repr_ctxt.nb_repr_vf = 0;
}

static int32_t sxe2_switchdev_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (!adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_INFO(adapter, DRV, "Current mode is not switchdev");
		goto l_end;
	}

	adapter->switchdev_info.is_switchdev = false;

	if (!adapter->flow_isolate_cfg && adapter->flow_isolated)
		adapter->flow_isolated = false;

	ret = sxe2_l2_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update l2 rule");

	ret = sxe2_switchdev_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update switchdev rule");

l_end:
	return ret;
}

static int32_t sxe2_switchdev_set(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;

	if (adapter->switchdev_info.is_switchdev) {
		PMD_DEV_LOG_INFO(adapter, DRV, "Current mode switch dev");
		goto l_end;
	}

	adapter->switchdev_info.is_switchdev = true;

	if (adapter->flow_isolate_cfg && !adapter->flow_isolated)
		adapter->flow_isolated = true;

	ret = sxe2_l2_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update l2 rule");

	ret = sxe2_switchdev_rule_update(adapter);
	if (ret != 0)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to update switchdev rule");

l_end:
	return ret;
}

int32_t sxe2_switchdev_notify_callback(struct sxe2_adapter *adapter, bool set)
{
	struct rte_eth_dev *dev = &rte_eth_devices[adapter->dev_info.dev_data->port_id];
	int32_t ret = 0;
	bool cur_switchdev_set = false;

	if (adapter->repr_ctxt.nb_repr_vf) {
		PMD_DEV_LOG_WARN(adapter, DRV, "switch dev notify remove dev");
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RMV, NULL);
		goto l_end;
	}

	ret = sxe2_drv_switchdev_mode_get(adapter, &cur_switchdev_set);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to get switchdev mode");
		ret = -EINVAL;
		goto l_end;
	}

	if (set != cur_switchdev_set) {
		PMD_DEV_LOG_INFO(adapter, DRV, "current switchdev mode miss macth");
		goto l_end;
	}

	if (set) {
		ret = sxe2_switchdev_set(adapter);
		if (ret != 0) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set switchdev");
			goto l_end;
		}
	} else {
		ret = sxe2_switchdev_clear(adapter);
		if (ret != 0) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to clear switchdev");
			goto l_end;
		}
	}
l_end:
	return ret;
}

int32_t sxe2_switchdev_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	PMD_DEV_LOG_INFO(adapter, INIT, "switchdev init");

	if (adapter->switchdev_info.is_switchdev)
		adapter->flow_isolated = true;

	adapter->repr_priv_data = NULL;
	adapter->repr_ctxt.nb_repr_vf = 0;

	return 0;
}

int32_t sxe2_switchdev_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	PMD_DEV_LOG_INFO(adapter, INIT, "switchdev uinit");

	if (adapter->repr_priv_data) {
		rte_free(adapter->repr_priv_data);
		adapter->repr_priv_data = NULL;
	}

	return 0;
}

int32_t sxe2_switchdev_dev_info_init(struct rte_eth_dev *dev,
			struct sxe2_adapter *parent_adapter)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_dev_info *dev_info = &adapter->dev_info;
	struct sxe2_dev_info *parent_dev_info = &parent_adapter->dev_info;
	struct sxe2_drv_dev_info_resp dev_info_resp = {0};
	struct sxe2_drv_dev_fw_info_resp dev_fw_info_resp = {0};
	int32_t ret = 0;

	PMD_INIT_FUNC_TRACE();

	dev_info->pci = parent_dev_info->pci;
	dev_info->pci.max_vfs = 0;

	ret = sxe2_drv_dev_info_get(adapter, &dev_info_resp);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get device info, ret=[%d]", ret);
		goto l_end;
	}

	ret = sxe2_drv_dev_fw_info_get(adapter, &dev_fw_info_resp);
	if (ret) {
		PMD_LOG_ERR(INIT, "Failed to get device fw info, ret=[%d]", ret);
		goto l_end;
	}
	dev_info->fw.build_id = dev_fw_info_resp.build_id;
	dev_info->fw.fix_version_id = dev_fw_info_resp.fix_version_id;
	dev_info->fw.sub_version_id = dev_fw_info_resp.sub_version_id;
	dev_info->fw.main_version_id = dev_fw_info_resp.main_version_id;

	rte_ether_addr_copy((struct rte_ether_addr *)dev_info_resp.mac_addr,
						(struct rte_ether_addr *)dev_info->mac.perm_addr);

l_end:
	return ret;
}

int32_t sxe2_switchdev_repr_private_data_init(struct rte_eth_dev *dev,
		struct sxe2_adapter *parent_adapter, uint16_t repr_id)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_repr_private_data *repr_priv_data = adapter->repr_priv_data;
	int32_t ret = 0;

	if (repr_priv_data != NULL)
		goto l_end;

	repr_priv_data = rte_zmalloc("sxe2_repr_priv_data",
			sizeof(struct sxe2_repr_private_data), 0);
	if (repr_priv_data == NULL) {
		PMD_LOG_ERR(INIT, "Failed to malloc representor private data.");
		ret = -EINVAL;
		goto l_end;
	}

	repr_priv_data->parent_adapter = parent_adapter;
	repr_priv_data->repr_id = repr_id;
	repr_priv_data->cp_vsi =
		TAILQ_FIRST(&parent_adapter->vsi_ctxt.other_vsi_list);
	if (repr_priv_data->cp_vsi == NULL) {
		PMD_LOG_ERR(INIT, "Failed to get cp vsi.");
		ret = -EINVAL;
		goto l_free;
	}
	repr_priv_data->repr_q_id = repr_id;
	repr_priv_data->repr_pf_id = parent_adapter->pf_idx;
	repr_priv_data->repr_vf_id = repr_id;
	repr_priv_data->repr_vf_k_vsi_id =
		parent_adapter->repr_ctxt.repr_vf_id[repr_id].kernel_vsi_id;
	repr_priv_data->repr_vf_u_vsi_id =
		parent_adapter->repr_ctxt.repr_vf_id[repr_id].dpdk_vsi_id;

	repr_priv_data->repr_vf_vsi_id =
		parent_adapter->repr_ctxt.repr_vf_id[repr_id].kernel_vsi_id !=
		SXE2_INVALID_VSI_ID ?
		parent_adapter->repr_ctxt.repr_vf_id[repr_id].kernel_vsi_id :
		parent_adapter->repr_ctxt.repr_vf_id[repr_id].dpdk_vsi_id;

	adapter->repr_priv_data = repr_priv_data;
	goto l_end;
l_free:
	rte_free(repr_priv_data);
l_end:
	return ret;
}
