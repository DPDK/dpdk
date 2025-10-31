/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_GMF_H__
#define __NTHW_GMF_H__

struct nthw_gmf {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_gmf;
	int mn_instance;

	nthw_register_t *mp_ctrl;
	nthw_field_t *mp_ctrl_enable;
	nthw_field_t *mp_ctrl_ifg_enable;
	nthw_field_t *mp_ctrl_ifg_tx_now_always;
	nthw_field_t *mp_ctrl_ifg_tx_on_ts_always;
	nthw_field_t *mp_ctrl_ifg_tx_on_ts_adjust_on_set_clock;
	nthw_field_t *mp_ctrl_ifg_auto_adjust_enable;
	nthw_field_t *mp_ctrl_ts_inject_always;
	nthw_field_t *mp_ctrl_fcs_always;

	nthw_register_t *mp_speed;
	nthw_field_t *mp_speed_ifg_speed;

	nthw_register_t *mp_ifg_clock_delta;
	nthw_field_t *mp_ifg_clock_delta_delta;

	nthw_register_t *mp_ifg_clock_delta_adjust;
	nthw_field_t *mp_ifg_clock_delta_adjust_delta;

	nthw_register_t *mp_ifg_max_adjust_slack;
	nthw_field_t *mp_ifg_max_adjust_slack_slack;

	nthw_register_t *mp_debug_lane_marker;
	nthw_field_t *mp_debug_lane_marker_compensation;

	nthw_register_t *mp_stat_sticky;
	nthw_field_t *mp_stat_sticky_data_underflowed;
	nthw_field_t *mp_stat_sticky_ifg_adjusted;

	nthw_register_t *mp_stat_next_pkt;
	nthw_field_t *mp_stat_next_pkt_ns;

	nthw_register_t *mp_stat_max_delayed_pkt;
	nthw_field_t *mp_stat_max_delayed_pkt_ns;

	nthw_register_t *mp_ts_inject;
	nthw_field_t *mp_ts_inject_offset;
	nthw_field_t *mp_ts_inject_pos;
	int mn_param_gmf_ifg_speed_mul;
	int mn_param_gmf_ifg_speed_div;

	bool m_administrative_block;	/* Used to enforce license expiry */
};

typedef struct nthw_gmf nthw_gmf_t;

int nthw_gmf_init(nthw_gmf_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_gmf_set_enable_tsi(nthw_gmf_t *p, bool enable, int tsi_dynamic_offset,
	int tsi_static_offset, bool tsi_always);

void nthw_gmf_set_enable(nthw_gmf_t *p, bool enable);

#endif	/* __NTHW_GMF_H__ */
