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
