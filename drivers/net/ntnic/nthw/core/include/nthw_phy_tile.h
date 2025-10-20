/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_PHY_TILE_H__
#define __NTHW_PHY_TILE_H__

#include "nthw_fpga_model.h"

enum mac_pcs_mode_e {
	MAC_PCS_MODE_8x10_25,
	MAC_PCS_MODE_2X40,
	MAC_PCS_MODE_2X100
};

struct nt_phy_tile {
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_mod_phy_tile;

	int mn_phy_tile_instance;

	int mn_fpga_version;
	int mn_fpga_revision;

	enum mac_pcs_mode_e mac_pcs_mode;

	nthw_register_t *mp_reg_port_xcvr_base[2][4];
	nthw_field_t *mp_fld_port_xcvr_base_ptr[2][4];
	nthw_field_t *mp_fld_port_xcvr_base_busy[2][4];
	nthw_field_t *mp_fld_port_xcvr_base_cmd[2][4];

	nthw_register_t *mp_reg_port_xcvr_data[2][4];
	nthw_field_t *mp_fld_port_xcvr_data_data[2][4];

	nthw_register_t *mp_reg_port_eth_base[2];
	nthw_field_t *mp_fld_port_eth_base_ptr[2];
	nthw_field_t *mp_fld_port_eth_base_busy[2];
	nthw_field_t *mp_fld_port_eth_base_cmd[2];

	nthw_register_t *mp_reg_port_eth_data[2];
	nthw_field_t *mp_fld_port_eth_data_data[2];

	nthw_register_t *mp_reg_link_summary[2];
	nthw_field_t *mp_fld_link_summary_nt_phy_link_state[2];
	nthw_field_t *mp_fld_link_summary_ll_nt_phy_link_state[2];
	nthw_field_t *mp_fld_link_summary_link_down_cnt[2];
	nthw_field_t *mp_fld_link_summary_ll_rx_block_lock[2];
	nthw_field_t *mp_fld_link_summary_ll_rx_am_lock[2];
	nthw_field_t *mp_fld_link_summary_lh_rx_high_bit_error_rate[2];
	nthw_field_t *mp_fld_link_summary_lh_received_local_fault[2];
	nthw_field_t *mp_fld_link_summary_lh_remote_fault[2];

	nthw_register_t *mp_reg_port_status[2];
	nthw_field_t *mp_fld_port_status_rx_pcs_fully_aligned[2];
	nthw_field_t *mp_fld_port_status_rx_hi_ber[2];
	nthw_field_t *mp_fld_port_status_rx_remote_fault[2];
	nthw_field_t *mp_fld_port_status_rx_local_fault[2];
	nthw_field_t *mp_fld_port_status_rx_am_lock[2];
	nthw_field_t *mp_fld_port_status_reset_ackn[2];
	nthw_field_t *mp_fld_port_status_tx_lanes_stable[2];
	nthw_field_t *mp_fld_port_status_tx_pll_locked[2];
	nthw_field_t *mp_fld_port_status_sys_pll_locked[2];
	nthw_field_t *mp_fld_port_status_tx_reset_ackn[2];
	nthw_field_t *mp_fld_port_status_rx_reset_ackn[2];

	nthw_register_t *mp_reg_port_config[2];
	nthw_field_t *mp_fld_port_config_dyn_reset;
	nthw_field_t *mp_fld_port_config_reset[2];
	nthw_field_t *mp_fld_port_config_rx_reset[2];
	nthw_field_t *mp_fld_port_config_tx_reset[2];
	nthw_field_t *mp_fld_port_config_nt_linkup_latency[2];
	nthw_field_t *mp_fld_port_config_nt_force_linkdown[2];
	nthw_field_t *mp_fld_port_config_nt_auto_force_linkdown[2];

	nthw_register_t *mp_reg_port_comp[2];
	nthw_field_t *mp_fld_port_comp_rx_compensation[2];
	nthw_field_t *mp_fld_port_comp_tx_compensation[2];

	nthw_register_t *mp_reg_dyn_reconfig_base;
	nthw_field_t *mp_fld_dyn_reconfig_base_ptr;
	nthw_field_t *mp_fld_dyn_reconfig_base_busy;
	nthw_field_t *mp_fld_dyn_reconfig_base_cmd;

	nthw_register_t *mp_reg_dyn_reconfig_data;
	nthw_field_t *mp_fld_dyn_reconfig_data_data;

	nthw_register_t *mp_reg_scratch;
	nthw_field_t *mp_fld_scratch_data;

	nthw_register_t *mp_reg_dr_cfg;
	nthw_field_t *mp_fld_reg_dr_cfg_features;
	nthw_field_t *mp_fld_reg_dr_cfg_tx_flush_level;

	nthw_register_t *mp_reg_dr_cfg_status;
	nthw_field_t *mp_fld_dr_cfg_status_curr_profile_id;
	nthw_field_t *mp_fld_dr_cfg_status_in_progress;
	nthw_field_t *mp_fld_dr_cfg_status_error;

	nthw_register_t *mp_reg_sys_pll;
	nthw_field_t *mp_fld_sys_pll_set_rdy;
	nthw_field_t *mp_fld_sys_pll_get_rdy;
	nthw_field_t *mp_fld_sys_pll_system_pll_lock;
	nthw_field_t *mp_fld_sys_pll_en_ref_clk_fgt;
	nthw_field_t *mp_fld_sys_pll_disable_ref_clk_monitor;
	nthw_field_t *mp_fld_sys_pll_ref_clk_fgt_enabled;
	nthw_field_t *mp_fld_sys_pll_forward_rst;
	nthw_field_t *mp_fld_sys_pll_force_rst;
};

typedef struct nt_phy_tile nthw_phy_tile_t;
typedef struct nt_phy_tile nt_phy_tile;

nthw_phy_tile_t *nthw_phy_tile_new(void);
int nthw_phy_tile_init(nthw_phy_tile_t *p, nthw_fpga_t *p_fpga, int mn_phy_tile_instance);
void nthw_phy_tile_set_tx_pol_inv(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, bool invert);
void nthw_phy_tile_set_rx_pol_inv(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, bool invert);
void nthw_phy_tile_set_host_loopback(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	bool enable);
void nthw_phy_tile_set_tx_equalization(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t pre_tap2, uint32_t main_tap, uint32_t pre_tap1,
	uint32_t post_tap1);
void nthw_phy_tile_get_link_summary(nthw_phy_tile_t *p, uint32_t *p_nt_phy_link_state,
	uint32_t *p_ll_nt_phy_link_state, uint32_t *p_lh_local_fault,
	uint32_t *p_lh_remote_fault, uint8_t index);
void nthw_phy_tile_set_tx_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset);
void nthw_phy_tile_set_rx_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset);
bool nthw_phy_tile_read_fec_enabled_by_scratch(nthw_phy_tile_t *p, uint8_t intf_no);

bool nthw_phy_tile_get_rx_hi_ber(nthw_phy_tile_t *p, uint8_t intf_no);
bool nthw_phy_tile_get_rx_am_lock(nthw_phy_tile_t *p, uint8_t intf_no);
void nthw_phy_tile_set_timestamp_comp_rx(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t value);
uint32_t nthw_phy_tile_get_timestamp_comp_rx(nthw_phy_tile_t *p, uint8_t intf_no);

bool nthw_phy_tile_configure_fec(nthw_phy_tile_t *p, uint8_t intf_no, bool enable);
uint32_t nthw_phy_tile_read_xcvr(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t address);
void nthw_phy_tile_write_xcvr(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, uint32_t address,
	uint32_t data);
uint32_t nthw_phy_tile_get_port_status_reset_ack(nthw_phy_tile_t *p, uint8_t intf_no);
uint32_t nthw_phy_tile_get_port_status_tx_lanes_stable(nthw_phy_tile_t *p, uint8_t intf_no);
void nthw_phy_tile_set_sys_pll_set_rdy(nthw_phy_tile_t *p, uint32_t value);
uint32_t nthw_phy_tile_get_sys_pll_get_rdy(nthw_phy_tile_t *p);
uint32_t nthw_phy_tile_get_sys_pll_system_pll_lock(nthw_phy_tile_t *p);
void nthw_phy_tile_set_sys_pll_en_ref_clk_fgt(nthw_phy_tile_t *p, uint32_t value);
uint32_t nthw_phy_tile_get_sys_pll_ref_clk_fgt_enabled(nthw_phy_tile_t *p);
void nthw_phy_tile_set_sys_pll_forward_rst(nthw_phy_tile_t *p, uint32_t value);
void nthw_phy_tile_set_sys_pll_force_rst(nthw_phy_tile_t *p, uint32_t value);
uint8_t nthw_phy_tile_get_no_intfs(nthw_phy_tile_t *p);
void nthw_phy_tile_set_port_config_rst(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t value);
bool nthw_phy_tile_use_phy_tile_pll_check(nthw_phy_tile_t *p);

#endif	/* __NTHW_PHY_TILE_H__ */
