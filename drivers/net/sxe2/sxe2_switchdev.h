/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_SWITCHDEV_H__
#define __SXE2_SWITCHDEV_H__
#include <ethdev_driver.h>

struct sxe2_adapter;

int32_t sxe2_uplink_clear(struct sxe2_adapter *adapter);

int32_t sxe2_uplink_set(struct sxe2_adapter *adapter);

int32_t sxe2_repr_clear(struct sxe2_adapter *adapter);

int32_t sxe2_repr_set(struct sxe2_adapter *adapter);

int32_t sxe2_switchdev_notify_callback(struct sxe2_adapter *adapter, bool set);

int32_t sxe2_switchdev_init(struct rte_eth_dev *dev);

int32_t sxe2_switchdev_uninit(struct rte_eth_dev *dev);

void sxe2_free_repr_info(struct rte_eth_dev *dev);

int32_t sxe2_switchdev_dev_info_init(struct rte_eth_dev *dev,
			struct sxe2_adapter *parent_adapter);

int32_t sxe2_switchdev_repr_private_data_init(struct rte_eth_dev *dev,
		struct sxe2_adapter *parent_adapter, uint16_t repr_id);

#endif /* __SXE2_SWITCHDEV_H__ */
