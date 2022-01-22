/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#ifndef _HNS3_COMMON_H_
#define _HNS3_COMMON_H_

#include <sys/time.h>

#include "hns3_ethdev.h"

#define HNS3_CONVERT_TO_DECIMAL 10
#define HNS3_CONVERT_TO_HEXADECIMAL 16

enum {
	HNS3_IO_FUNC_HINT_NONE = 0,
	HNS3_IO_FUNC_HINT_VEC,
	HNS3_IO_FUNC_HINT_SVE,
	HNS3_IO_FUNC_HINT_SIMPLE,
	HNS3_IO_FUNC_HINT_COMMON
};

#define HNS3_DEVARG_RX_FUNC_HINT	"rx_func_hint"
#define HNS3_DEVARG_TX_FUNC_HINT	"tx_func_hint"

#define HNS3_DEVARG_DEV_CAPS_MASK	"dev_caps_mask"

#define HNS3_DEVARG_MBX_TIME_LIMIT_MS	"mbx_time_limit_ms"

#define MSEC_PER_SEC              1000L
#define USEC_PER_MSEC             1000L

int hns3_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version,
			size_t fw_size);
int hns3_dev_infos_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_dev_info *info);

void hns3_clock_gettime(struct timeval *tv);
uint64_t hns3_clock_calctime_ms(struct timeval *tv);
uint64_t hns3_clock_gettime_ms(void);

void hns3_parse_devargs(struct rte_eth_dev *dev);

int hns3_configure_all_mc_mac_addr(struct hns3_adapter *hns, bool del);
int hns3_configure_all_mac_addr(struct hns3_adapter *hns, bool del);
int hns3_add_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		      __rte_unused uint32_t idx, __rte_unused uint32_t pool);

void hns3_remove_mac_addr(struct rte_eth_dev *dev, uint32_t idx);
int hns3_set_mc_mac_addr_list(struct rte_eth_dev *dev,
			      struct rte_ether_addr *mc_addr_set,
			      uint32_t nb_mc_addr);
void hns3_ether_format_addr(char *buf, uint16_t size,
			    const struct rte_ether_addr *ether_addr);
int hns3_init_mac_addrs(struct rte_eth_dev *dev);

int hns3_init_ring_with_vector(struct hns3_hw *hw);
int hns3_map_rx_interrupt(struct rte_eth_dev *dev);
void hns3_unmap_rx_interrupt(struct rte_eth_dev *dev);
int hns3_restore_rx_interrupt(struct hns3_hw *hw);

int hns3_get_pci_revision_id(struct hns3_hw *hw, uint8_t *revision_id);

#endif /* _HNS3_COMMON_H_ */
