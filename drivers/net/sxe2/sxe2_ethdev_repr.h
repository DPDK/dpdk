/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SX2_ETHDEV_REPR_H__
#define __SX2_ETHDEV_REPR_H__
#include <rte_compat.h>
#include <rte_kvargs.h>
#include <rte_time.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_tm_driver.h>
#include <rte_io.h>
#include <rte_ethdev.h>
#include <rte_alarm.h>
#include <rte_dev_info.h>

#include "sxe2_vsi.h"
#include "sxe2_irq.h"
#include "sxe2_queue.h"
struct sxe2_adapter;

void sxe2_repr_all_close(struct rte_eth_dev *dev);

int32_t sxe2_repr_dev_init(struct rte_eth_dev *dev,
		       struct sxe2_adapter *parent_adapter,
		       uint16_t repr_id);

int32_t sxe2_switchdev_repr_devs_init(struct sxe2_adapter *adapter,
				  struct rte_eth_devargs *req_eth_da);

#endif /* __SX2_ETHDEV_REPR_H__ */
