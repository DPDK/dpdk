/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef _IDPF_PTP_H_
#define _IDPF_PTP_H_

#include "base/idpf_osdep.h"
#include <rte_time.h>
#include "idpf_common_device.h"

struct idpf_ptp_cmd {
	uint32_t exec_cmd_mask;
	uint32_t shtime_enable_mask;
};

struct idpf_ptp_dev_clk_regs {
	volatile uint32_t *dev_clk_ns_l;
	volatile uint32_t *dev_clk_ns_h;
	volatile uint32_t *phy_clk_ns_l;
	volatile uint32_t *phy_clk_ns_h;
	volatile uint32_t *sys_time_ns_l;
	volatile uint32_t *sys_time_ns_h;
	volatile uint32_t *incval_l;
	volatile uint32_t *incval_h;
	volatile uint32_t *shadj_l;
	volatile uint32_t *shadj_h;
	volatile uint32_t *phy_incval_l;
	volatile uint32_t *phy_incval_h;
	volatile uint32_t *phy_shadj_l;
	volatile uint32_t *phy_shadj_h;
	volatile uint32_t *cmd;
	volatile uint32_t *phy_cmd;
	volatile uint32_t *cmd_sync;
};

enum idpf_ptp_access {
	IDPF_PTP_NONE = 0,
	IDPF_PTP_DIRECT,
	IDPF_PTP_MAILBOX,
};

struct idpf_ptp_secondary_mbx {
	uint16_t peer_mbx_q_id;
	uint16_t peer_id;
	bool valid:1;
};

enum idpf_ptp_tx_tstamp_state {
	IDPF_PTP_FREE,
	IDPF_PTP_REQUEST,
	IDPF_PTP_READ_VALUE,
};

struct idpf_ptp_tx_tstamp {
	uint64_t tstamp;
	uint32_t tx_latch_reg_offset_l;
	uint32_t tx_latch_reg_offset_h;
	uint32_t idx;
};

struct idpf_ptp_vport_tx_tstamp_caps {
	uint32_t vport_id;
	uint16_t num_entries;
	uint16_t tstamp_ns_lo_bit;
	uint16_t latched_idx;
	bool access:1;
	struct idpf_ptp_tx_tstamp tx_tstamp[];
};

struct idpf_ptp {
	uint64_t base_incval;
	uint64_t max_adj;
	struct idpf_ptp_cmd cmd;
	struct idpf_ptp_dev_clk_regs dev_clk_regs;
	uint32_t caps;
	uint8_t get_dev_clk_time_access:2;
	uint8_t get_cross_tstamp_access:2;
	uint8_t set_dev_clk_time_access:2;
	uint8_t adj_dev_clk_time_access:2;
	uint8_t tx_tstamp_access:2;
	uint8_t rsv:6;
	struct idpf_ptp_secondary_mbx secondary_mbx;
};

struct idpf_ptp_dev_timers {
	uint64_t sys_time_ns;
	uint64_t dev_clk_time_ns;
};

int idpf_ptp_get_caps(struct idpf_adapter *adapter);
int idpf_ptp_read_src_clk_reg(struct idpf_adapter *adapter, uint64_t *src_clk);
int idpf_ptp_get_dev_clk_time(struct idpf_adapter *adapter,
			     struct idpf_ptp_dev_timers *dev_clk_time);
int idpf_ptp_get_cross_time(struct idpf_adapter *adapter,
			    struct idpf_ptp_dev_timers *cross_time);
int idpf_ptp_set_dev_clk_time(struct idpf_adapter *adapter, uint64_t time);
int idpf_ptp_adj_dev_clk_fine(struct idpf_adapter *adapter, uint64_t incval);
int idpf_ptp_adj_dev_clk_time(struct idpf_adapter *adapter, int64_t delta);
int idpf_ptp_get_vport_tstamps_caps(struct idpf_vport *vport);
int idpf_ptp_get_tx_tstamp(struct idpf_vport *vport);

static inline uint64_t
idpf_tstamp_convert_32b_64b(struct idpf_adapter *ad, uint32_t flag,
			    bool is_rx, uint32_t in_timestamp)
{
	const uint64_t mask = 0xFFFFFFFFULL;
	uint32_t phc_time_lo, delta;
	uint64_t ns;

	if (flag != 0)
		idpf_ptp_read_src_clk_reg(ad, &ad->time_hw);

	phc_time_lo = (uint32_t)(ad->time_hw);
	delta = in_timestamp - phc_time_lo;

	if (delta > mask / 2) {
		delta = phc_time_lo - in_timestamp;
		ns = ad->time_hw - delta;
	} else {
		if (is_rx)
			ns = ad->time_hw - delta;
		else
			ns = ad->time_hw + delta;
	}

	return ns;
}

#endif /* _IDPF_PTP_H_ */
