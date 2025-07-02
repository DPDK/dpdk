/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_NIC_EVENT_H_
#define _HINIC3_NIC_EVENT_H_

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
void get_port_info(struct hinic3_hwdev *hwdev, u8 link_state,
		   struct rte_eth_link *link);

int hinic3_vf_event_handler(void *hwdev, __rte_unused void *pri_handle, u16 cmd,
			    void *buf_in, u16 in_size, void *buf_out,
			    u16 *out_size);

void hinic3_pf_event_handler(void *hwdev, __rte_unused void *pri_handle,
			     u16 cmd, void *buf_in, u16 in_size, void *buf_out,
			     u16 *out_size);

int hinic3_vf_mag_event_handler(void *hwdev, void *pri_handle, u16 cmd,
				void *buf_in, u16 in_size, void *buf_out,
				u16 *out_size);

void hinic3_pf_mag_event_handler(void *hwdev, void *pri_handle, u16 cmd,
				 void *buf_in, u16 in_size, void *buf_out,
				 u16 *out_size);

u8 hinic3_nic_sw_aeqe_handler(__rte_unused void *hwdev, u8 event, u8 *data);

#endif /* _HINIC3_NIC_EVENT_H_ */
