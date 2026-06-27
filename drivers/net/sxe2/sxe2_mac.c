/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_os.h>
#include "sxe2_osal.h"
#include "sxe2_mac.h"
#include "sxe2_common_log.h"
#include "sxe2_ethdev.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_host_regs.h"

int32_t sxe2_mac_default_cfg(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t                     ret;
	struct rte_ether_addr broadcast = {
		.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
	struct rte_ether_addr mac_addr;

	rte_ether_addr_copy((struct rte_ether_addr *)
		adapter->dev_info.mac.perm_addr, &mac_addr);
	ret = sxe2_uc_filter_add(adapter, &mac_addr, true);
	if (ret != 0) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add default MAC filter");
		goto l_end;
	}

	rte_ether_addr_copy(&broadcast, &mac_addr);
	ret = sxe2_mc_filter_add(adapter, &mac_addr, true);
	if (ret != 0) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add broadcast MAC filter");
		goto l_end;
	}

	ret = 0;
l_end:
	return ret;
}

int32_t sxe2_mac_addr_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret                         = -1;
	PMD_INIT_FUNC_TRACE();

	if (!rte_is_unicast_ether_addr
		((struct rte_ether_addr *)adapter->dev_info.mac.perm_addr)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Invalid MAC address");
		ret = -EINVAL;
		goto l_end;
	}

	dev->data->mac_addrs = rte_zmalloc("sxe2_mac_adds",
					sizeof(struct rte_ether_addr) * SXE2_NUM_MACADDR_MAX, 0);
	if (!dev->data->mac_addrs) {
		PMD_LOG_ERR(DRV, "Failed to allocate memory to store mac address");
		ret = -ENOMEM;
		goto l_end;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)adapter->dev_info.mac.perm_addr,
		&dev->data->mac_addrs[0]);

	ret = 0;

l_end:
	return ret;
}

void sxe2_mac_addr_uinit(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	if (dev != NULL && dev->data->mac_addrs != NULL) {
		rte_free(dev->data->mac_addrs);
		dev->data->mac_addrs = NULL;
	}
}

int32_t sxe2_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		      __rte_unused uint32_t index, __rte_unused uint32_t pool)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = -1;

	if (rte_is_zero_ether_addr(mac_addr)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Invalid MAC Address");
		ret = -EINVAL;
		goto l_end;
	}

	if (rte_is_multicast_ether_addr(mac_addr))
		ret = sxe2_mc_filter_add(adapter, mac_addr, true);
	else
		ret = sxe2_uc_filter_add(adapter, mac_addr, false);

	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add MAC filter");

l_end:
	return ret;
}

void sxe2_mac_addr_del(struct rte_eth_dev *dev,  uint32_t index)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct rte_ether_addr *mac_addr = &dev->data->mac_addrs[index];
	int32_t ret = -1;

	if (rte_is_multicast_ether_addr(mac_addr))
		ret = sxe2_mc_filter_del(adapter, mac_addr);
	else
		ret = sxe2_uc_filter_del(adapter, mac_addr);

	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to remove MAC filter");
}

int32_t sxe2_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	struct rte_ether_addr *old_addr = (struct rte_ether_addr *)&adapter->dev_info.mac.perm_addr;
	struct rte_ether_addr temp_addr;

	if (rte_is_same_ether_addr(old_addr, mac_addr))
		goto l_end;

	if (rte_is_multicast_ether_addr(mac_addr)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to set multicast addr");
		ret = -EINVAL;
		goto l_end;
	}

	ret = sxe2_uc_filter_del(adapter, old_addr);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to remove MAC filter");
		goto l_end;
	}

	rte_ether_addr_copy(old_addr, &temp_addr);

	rte_ether_addr_copy(mac_addr, old_addr);

	ret = sxe2_uc_filter_add(adapter, mac_addr, true);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add MAC filter");
		rte_ether_addr_copy(&temp_addr, old_addr);
		(void)sxe2_uc_filter_add(adapter, old_addr, true);
		goto l_end;
	}
l_end:
	return ret;
}

int32_t sxe2_set_mc_addr_list(struct rte_eth_dev *dev,
			struct rte_ether_addr *mc_addrs,
			uint32_t mc_addrs_num)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	uint32_t i;
	const uint8_t *mac;

	if (mc_addrs_num > SXE2_NUM_MACADDR_MAX) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Too many multicast MAC addresses, ");
		ret =  -1;
		goto l_end;
	}

	sxe2_mc_filter_clear(adapter, false);

	for (i = 0; i < mc_addrs_num; i++) {
		if (!rte_is_multicast_ether_addr(&mc_addrs[i])) {
			mac = mc_addrs[i].addr_bytes;
			PMD_DEV_LOG_ERR(adapter, DRV,
					"Invalid mac: %02x:%02x:%02x:%02x:%02x:%02x",
					mac[0], mac[1], mac[2], mac[3], mac[4],
					mac[5]);
			ret = -EINVAL;
			goto add_err;
		}

		ret = sxe2_mc_filter_add(adapter, &mc_addrs[i], false);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV,
			    "Failed to remove old multicast MAC filter list");
			goto add_err;
		}
	}
	goto l_end;
add_err:
	sxe2_mc_filter_clear(adapter, false);
l_end:
	return ret;
}

int32_t sxe2_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int32_t on)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vlan vlan = {
		.tpid = RTE_ETHER_TYPE_VLAN,
		.vid = vlan_id,
		.prio = 0
	};
	int32_t ret = 0;

	if (sxe2_dev_port_vlan_check(dev)) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Filter not supported with Port VLAN");
		ret = -ENOTSUP;
		goto l_end;
	}

	if (vlan_id == 0)
		goto l_end;

	if (on) {
		ret = sxe2_vlan_filter_add(adapter, &vlan, false);
		if (ret < 0) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add vlan filter");
			goto l_end;
		}
	} else {
		ret = sxe2_vlan_filter_del(adapter, &vlan);
		if (ret < 0) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to remove vlan filter");
			goto l_end;
		}
	}

l_end:
	return ret;
}

int32_t sxe2_dev_vlan_offload_set(struct rte_eth_dev *dev, int32_t mask)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;
	struct sxe2_vlan_info new_info = adapter->filter_ctxt.vlan_info;
	bool port_vlan = new_info.port_vlan_exist;

	uint8_t out_strip_mask = SXE2_DPDK_OFFLOAD_OUTER_STRIP_8021Q |
			    SXE2_DPDK_OFFLOAD_OUTER_STRIP_8021AD |
			    SXE2_DPDK_OFFLOAD_OUTER_STRIP_QINQ1;

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT) {
		if (!(txmode->offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT)) {
			PMD_DEV_LOG_ERR(adapter, DRV,
			    "VLAN INSERT must be enabled when QinQ INSERT is enabled");
			return -EINVAL;
		}
		if (port_vlan) {
			PMD_DEV_LOG_ERR(adapter, DRV,
					"QINQ INSERT not supported with Port VLAN");
			return -EINVAL;
		}
	}

	if (mask & RTE_ETH_QINQ_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP) {
			if (port_vlan) {
				PMD_DEV_LOG_ERR(adapter, DRV,
						"QinQ strip not supported with Port VLAN");
				return -EINVAL;
			}
			new_info.inner_strip = SXE2_VSI_TSR_ID_VLAN;
		} else {
			new_info.inner_strip = 0;
		}
	}

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) {
			new_info.outer_strip =
				port_vlan ? 0 : out_strip_mask;
			new_info.inner_strip =
				port_vlan ? new_info.inner_strip : new_info.inner_strip;
		} else {
			if (new_info.inner_strip != 0) {
				PMD_DEV_LOG_ERR(adapter, DRV,
					"Must disable QinQ strip before disabling VLAN strip");
				return -EINVAL;
			}
			new_info.outer_strip = 0;
		}
	}

	if (mask & (RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_QINQ_STRIP_MASK)) {
		struct sxe2_vlan_info old_info = adapter->filter_ctxt.vlan_info;
		adapter->filter_ctxt.vlan_info = new_info;

		ret = sxe2_drv_vlan_insert_strip_cfg(adapter);
		if (ret) {
			adapter->filter_ctxt.vlan_info = old_info;
			return ret;
		}
	}
	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (adapter->filter_ctxt.vlan_info.port_vlan_exist) {
			ret = 0;
			PMD_DEV_LOG_INFO(adapter, INIT, "vlan filter is not support when port vlan is enabled");
			goto l_end;
		}

		ret = sxe2_vlan_filter_ctrl(adapter,
			    !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER));
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV,
			    "sxe2_drv_vlan_filter_switch failed ret:%d", ret);
			goto l_end;
		}
	}

	PMD_DEV_LOG_DEBUG(adapter, DRV,
	    "mask:0x%x rx mode offload:0x%" PRIx64 " vlan offload set done",
	    mask, rxmode->offloads);
l_end:
	return ret;
}

static int32_t sxe2_vlan_filter_zero(struct sxe2_adapter *adapter)
{
	struct sxe2_vlan vlan;
	int32_t ret;
	uint16_t tpids[] = {RTE_ETHER_TYPE_VLAN, RTE_ETHER_TYPE_QINQ, RTE_ETHER_TYPE_QINQ1};
	uint8_t i;

	vlan = (struct sxe2_vlan){0, 0, 0};
	ret = sxe2_vlan_filter_add(adapter, &vlan, true);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add VLAN ID 0");
		goto l_end;
	}

	for (i = 0; i < RTE_DIM(tpids); i++) {
		vlan = (struct sxe2_vlan){tpids[i], 0, 0};
		ret = sxe2_vlan_filter_add(adapter, &vlan, true);
		if (ret) {
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add VLAN ID 0 when tpid:0x%x",
					tpids[i]);
			goto l_end;
		}
	}

l_end:
	return ret;
}

int32_t sxe2_vlan_cfg_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_drv_vlan_config_query(adapter);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to query vlan config, ret=%d", ret);
		goto l_end;
	}

	if (!sxe2_dev_port_vlan_check(dev))
		adapter->filter_ctxt.vlan_info.outer_insert =
			SXE2_DPDK_OFFLOAD_OUTER_INSERT_8021Q |
			SXE2_DPDK_OFFLOAD_INSERT_ENABLE;
	else
		adapter->filter_ctxt.vlan_info.outer_insert = 0;

	adapter->filter_ctxt.vlan_info.inner_insert =
			SXE2_DPDK_OFFLOAD_INNER_INSERT_QINQ1 | SXE2_DPDK_OFFLOAD_INSERT_ENABLE;

	if (!sxe2_dev_port_vlan_check(dev)) {
		ret = sxe2_vlan_filter_zero(adapter);
		if (ret != 0)
			PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add vlan filter switch:0 "
					"for port:%d", adapter->port_idx);
	}

l_end:
	return ret;
}

int32_t sxe2_vlan_default_cfg(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	ret = sxe2_dev_vlan_offload_set(dev, RTE_ETH_VLAN_STRIP_MASK |
					RTE_ETH_QINQ_STRIP_MASK |
					RTE_ETH_VLAN_FILTER_MASK);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cfg vlan offload, ret:%d", ret);

	return ret;
}

int32_t sxe2_promisc_enable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_promisc_add(adapter);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to enable promisc, ret:%d", ret);

	return ret;
}

int32_t sxe2_promisc_disable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_promisc_del(adapter);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to disable promisc, ret:%d", ret);

	return ret;
}

int32_t sxe2_allmulti_enable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_allmulti_add(adapter);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to enable allmulti, ret:%d", ret);

	return ret;
}

int32_t sxe2_allmulti_disable(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;

	ret = sxe2_allmulti_del(adapter);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to disable allmulti, ret:%d", ret);

	return ret;
}

int32_t sxe2_link_update_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret;

	PMD_INIT_FUNC_TRACE();

	rte_spinlock_init(&adapter->link_ctxt.link_lock);

	ret = sxe2_drv_mac_link_status_get(adapter);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to get link status, ret=%d", ret);
		goto l_end;
	}

	(void)sxe2_link_update(dev, 0);

l_end:
	return ret;
}
int32_t sxe2_link_update(struct rte_eth_dev *dev, __rte_unused int32_t wait_to_complete)
{
	struct rte_eth_link new_link;
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	memset(&new_link, 0, sizeof(new_link));

	switch (adapter->link_ctxt.speed) {
	case 0:
		new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;
	case 10:
		new_link.link_speed = RTE_ETH_SPEED_NUM_10M;
		break;
	case 100:
		new_link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case 1000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case 10000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case 20000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_20G;
		break;
	case 25000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_25G;
		break;
	case 40000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_40G;
		break;
	case 50000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_50G;
		break;
	case 100000:
		new_link.link_speed = RTE_ETH_SPEED_NUM_100G;
		break;
	default:
		new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;
	}

	new_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	new_link.link_status = adapter->link_ctxt.link_up ? RTE_ETH_LINK_UP :
					     RTE_ETH_LINK_DOWN;
	new_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				RTE_ETH_LINK_SPEED_FIXED);

	return rte_eth_linkstatus_set(dev, &new_link);
}

int32_t sxe2_mtu_set(struct rte_eth_dev *dev, uint16_t mtu __rte_unused)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_started != 0) {
		PMD_DEV_LOG_ERR(adapter, DRV, "port %d must be stopped before configuration",
				dev->data->port_id);
		return -EBUSY;
	}

	return 0;
}
