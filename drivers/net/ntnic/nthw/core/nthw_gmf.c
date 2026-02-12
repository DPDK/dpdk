/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <limits.h>
#include <math.h>
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_gmf.h"

int nthw_gmf_init(nthw_gmf_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_GMF, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: GMF %d: no such instance",
			p_fpga->p_fpga_info->mp_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_gmf = mod;

	p->mp_ctrl = nthw_module_get_register(p->mp_mod_gmf, GMF_CTRL);
	p->mp_ctrl_enable = nthw_register_get_field(p->mp_ctrl, GMF_CTRL_ENABLE);
	p->mp_ctrl_ifg_enable = nthw_register_get_field(p->mp_ctrl, GMF_CTRL_IFG_ENABLE);
	p->mp_ctrl_ifg_auto_adjust_enable =
		nthw_register_get_field(p->mp_ctrl, GMF_CTRL_IFG_AUTO_ADJUST_ENABLE);
	p->mp_ctrl_ts_inject_always =
		nthw_register_query_field(p->mp_ctrl, GMF_CTRL_TS_INJECT_ALWAYS);
	p->mp_ctrl_fcs_always = nthw_register_query_field(p->mp_ctrl, GMF_CTRL_FCS_ALWAYS);

	p->mp_speed = nthw_module_get_register(p->mp_mod_gmf, GMF_SPEED);
	p->mp_speed_ifg_speed = nthw_register_get_field(p->mp_speed, GMF_SPEED_IFG_SPEED);

	p->mp_ifg_clock_delta = nthw_module_get_register(p->mp_mod_gmf, GMF_IFG_SET_CLOCK_DELTA);
	p->mp_ifg_clock_delta_delta =
		nthw_register_get_field(p->mp_ifg_clock_delta, GMF_IFG_SET_CLOCK_DELTA_DELTA);

	p->mp_ifg_max_adjust_slack =
		nthw_module_get_register(p->mp_mod_gmf, GMF_IFG_MAX_ADJUST_SLACK);
	p->mp_ifg_max_adjust_slack_slack = nthw_register_get_field(p->mp_ifg_max_adjust_slack,
			GMF_IFG_MAX_ADJUST_SLACK_SLACK);

	p->mp_debug_lane_marker = nthw_module_get_register(p->mp_mod_gmf, GMF_DEBUG_LANE_MARKER);
	p->mp_debug_lane_marker_compensation =
		nthw_register_get_field(p->mp_debug_lane_marker,
			GMF_DEBUG_LANE_MARKER_COMPENSATION);

	p->mp_stat_sticky = nthw_module_get_register(p->mp_mod_gmf, GMF_STAT_STICKY);
	p->mp_stat_sticky_data_underflowed =
		nthw_register_get_field(p->mp_stat_sticky, GMF_STAT_STICKY_DATA_UNDERFLOWED);
	p->mp_stat_sticky_ifg_adjusted =
		nthw_register_get_field(p->mp_stat_sticky, GMF_STAT_STICKY_IFG_ADJUSTED);

	p->mn_param_gmf_ifg_speed_mul =
		nthw_fpga_get_product_param(p_fpga, NT_GMF_IFG_SPEED_MUL, 1);
	p->mn_param_gmf_ifg_speed_div =
		nthw_fpga_get_product_param(p_fpga, NT_GMF_IFG_SPEED_DIV, 1);

	p->m_administrative_block = false;

	p->mp_stat_next_pkt = nthw_module_query_register(p->mp_mod_gmf, GMF_STAT_NEXT_PKT);

	if (p->mp_stat_next_pkt) {
		p->mp_stat_next_pkt_ns =
			nthw_register_query_field(p->mp_stat_next_pkt, GMF_STAT_NEXT_PKT_NS);

	} else {
		p->mp_stat_next_pkt_ns = NULL;
	}

	p->mp_stat_max_delayed_pkt =
		nthw_module_query_register(p->mp_mod_gmf, GMF_STAT_MAX_DELAYED_PKT);

	if (p->mp_stat_max_delayed_pkt) {
		p->mp_stat_max_delayed_pkt_ns =
			nthw_register_query_field(p->mp_stat_max_delayed_pkt,
				GMF_STAT_MAX_DELAYED_PKT_NS);

	} else {
		p->mp_stat_max_delayed_pkt_ns = NULL;
	}

	p->mp_ctrl_ifg_tx_now_always =
		nthw_register_query_field(p->mp_ctrl, GMF_CTRL_IFG_TX_NOW_ALWAYS);
	p->mp_ctrl_ifg_tx_on_ts_always =
		nthw_register_query_field(p->mp_ctrl, GMF_CTRL_IFG_TX_ON_TS_ALWAYS);

	p->mp_ctrl_ifg_tx_on_ts_adjust_on_set_clock =
		nthw_register_query_field(p->mp_ctrl, GMF_CTRL_IFG_TX_ON_TS_ADJUST_ON_SET_CLOCK);

	p->mp_ifg_clock_delta_adjust =
		nthw_module_query_register(p->mp_mod_gmf, GMF_IFG_SET_CLOCK_DELTA_ADJUST);

	if (p->mp_ifg_clock_delta_adjust) {
		p->mp_ifg_clock_delta_adjust_delta =
			nthw_register_query_field(p->mp_ifg_clock_delta_adjust,
				GMF_IFG_SET_CLOCK_DELTA_ADJUST_DELTA);

	} else {
		p->mp_ifg_clock_delta_adjust_delta = NULL;
	}

	p->mp_ts_inject = nthw_module_query_register(p->mp_mod_gmf, GMF_TS_INJECT);

	if (p->mp_ts_inject) {
		p->mp_ts_inject_offset =
			nthw_register_query_field(p->mp_ts_inject, GMF_TS_INJECT_OFFSET);
		p->mp_ts_inject_pos =
			nthw_register_query_field(p->mp_ts_inject, GMF_TS_INJECT_POS);

	} else {
		p->mp_ts_inject_offset = NULL;
		p->mp_ts_inject_pos = NULL;
	}

	return 0;
}

void nthw_gmf_set_enable(nthw_gmf_t *p, bool enable)
{
	if (!p->m_administrative_block)
		nthw_field_set_val_flush32(p->mp_ctrl_enable, enable ? 1 : 0);
}

void nthw_gmf_set_enable_tsi(nthw_gmf_t *p, bool enable, int tsi_dynamic_offset,
	int tsi_static_offset, bool tsi_always)
{
	if (!p->m_administrative_block) {
		nthw_field_update_register(p->mp_ctrl_enable);

		if (p->mp_ctrl_ts_inject_always) {
			/*
			 * Do not force timestamp Inject- let the TBH control this now
			 * Later we could consider an ini-setting for controlling this
			 */
			nthw_field_set_val_flush32(p->mp_ctrl_ts_inject_always,
				tsi_always ? 1 : 0);
		}

		if (p->mp_ctrl_fcs_always) {
			/*
			 * Do not force FSC calculation - let the TBH control this
			 * Later we could consider an ini-setting for controlling this
			 */
			nthw_field_set_val_flush32(p->mp_ctrl_fcs_always, 0);
		}

		if (p->mp_ts_inject) {
			nthw_register_update(p->mp_ts_inject);

			if (p->mp_ts_inject_pos) {
				nthw_field_set_val_flush32(p->mp_ts_inject_pos,
					(uint32_t)tsi_dynamic_offset);
			}

			if (p->mp_ts_inject_offset) {
				nthw_field_set_val_flush32(p->mp_ts_inject_offset,
					(uint32_t)tsi_static_offset);
			}
		}

		nthw_field_set_val_flush32(p->mp_ctrl_enable, enable ? 1 : 0);
	}
}
