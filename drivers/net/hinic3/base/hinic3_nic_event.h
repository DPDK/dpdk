/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_NIC_EVENT_H_
#define _HINIC3_NIC_EVENT_H_

#include "hinic3_hwdev.h"

/**
 * Get the Ethernet port link information based on the link state.
 *
 * @param[in] hwdev
 * The hardware device context.
 * @param[in] link_state
 * The current link state (0 = down, non-zero = up).
 * @param[out] link
 * Pointer to the `rte_eth_link` structure.
 */
void hinic3_get_link_port_info(struct hinic3_hwdev *hwdev, uint8_t link_state,
			       struct rte_eth_link *link);

int hinic3_vf_event_handler(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			    struct hinic3_handler_info *handler_info);

void hinic3_pf_event_handler(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			     struct hinic3_handler_info *handler_info);

int hinic3_vf_mag_event_handler(struct hinic3_hwdev *hwdev, void *pri_handle,
				struct hinic3_handler_info *handler_info);

void hinic3_pf_mag_event_handler(struct hinic3_hwdev *hwdev, void *pri_handle,
				 struct hinic3_handler_info *handler_info);

uint8_t hinic3_nic_sw_aeqe_handler(__rte_unused struct hinic3_hwdev *hwdev,
				   uint8_t event, uint8_t *data);

#endif /* _HINIC3_NIC_EVENT_H_ */
