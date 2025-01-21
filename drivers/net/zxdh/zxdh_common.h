/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_COMMON_H
#define ZXDH_COMMON_H

#include <stdint.h>
#include <rte_ethdev.h>

#include "zxdh_ethdev.h"

#define ZXDH_VF_LOCK_REG               0x90
#define ZXDH_VF_LOCK_ENABLE_MASK       0x1
#define ZXDH_ACQUIRE_CHANNEL_NUM_MAX   10

struct zxdh_res_para {
	uint64_t virt_addr;
	uint16_t pcie_id;
	uint16_t src_type; /* refer to BAR_DRIVER_TYPE */
};

int32_t zxdh_phyport_get(struct rte_eth_dev *dev, uint8_t *phyport);
int32_t zxdh_panelid_get(struct rte_eth_dev *dev, uint8_t *pannelid);
int32_t zxdh_hashidx_get(struct rte_eth_dev *dev, uint8_t *hash_idx);
uint32_t zxdh_read_bar_reg(struct rte_eth_dev *dev, uint32_t bar, uint32_t reg);
void zxdh_write_bar_reg(struct rte_eth_dev *dev, uint32_t bar, uint32_t reg, uint32_t val);
void zxdh_release_lock(struct zxdh_hw *hw);
int32_t zxdh_timedlock(struct zxdh_hw *hw, uint32_t us);
uint32_t zxdh_read_comm_reg(uint64_t pci_comm_cfg_baseaddr, uint32_t reg);
void zxdh_write_comm_reg(uint64_t pci_comm_cfg_baseaddr, uint32_t reg, uint32_t val);
int32_t zxdh_datach_set(struct rte_eth_dev *dev);

#endif /* ZXDH_COMMON_H */
