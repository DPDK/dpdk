/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_IRQ_H
#define SXE2_IRQ_H

#include <ethdev_driver.h>

#include "sxe2_drv_cmd.h"

#define SXE2_IRQ_MAX_CNT 2048

#define SXE2_LAN_MSIX_MIN_CNT 1

#define SXE2_EVENT_IRQ_IDX 0

#define SXE2_MAX_INTR_QUEUE_NUM   256

#define SXE2_IRQ_NAME_MAX_LEN     (IFNAMSIZ + 16)

#define SXE2_ITR_1000K  1
#define SXE2_ITR_500K   2
#define SXE2_ITR_50K    20

#define SXE2_ITR_INTERVAL_NORMAL  (SXE2_ITR_50K)
#define SXE2_ITR_INTERVAL_LOW     (SXE2_ITR_1000K)

struct sxe2_fwc_msix_caps;
struct sxe2_adapter;

struct sxe2_irq_context {
	struct rte_intr_handle *reset_handle;
	int32_t reset_event_fd;
	int32_t other_event_fd;

	uint16_t max_cnt_hw;
	uint16_t base_idx_in_func;

	uint16_t rxq_avail_cnt;
	uint16_t rxq_base_idx_in_pf;

	uint16_t rxq_irq_cnt;
	uint32_t *rxq_msix_idx;
	int32_t *rxq_event_fd;
};

uint32_t sxe2_drv_dev_other_cause_get(struct sxe2_adapter *adapter);

int32_t sxe2_intr_init(struct rte_eth_dev *dev);

void sxe2_intr_uninit(struct rte_eth_dev *dev);

int32_t sxe2_sw_irq_ctxt_init(struct rte_eth_dev *dev);

void sxe2_sw_irq_ctxt_uninit(struct rte_eth_dev *dev);

void sxe2_sw_irq_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_msix_caps *msix_caps);

int32_t sxe2_rxq_intr_enable(struct rte_eth_dev *dev);

void sxe2_rxq_intr_disable(struct rte_eth_dev *dev);

int32_t sxe2_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);

int32_t sxe2_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);

#endif /* SXE2_IRQ_H */
