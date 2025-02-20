/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_PHY_TILE_H__
#define __NTHW_PHY_TILE_H__

#include "nthw_fpga_model.h"

struct nt_phy_tile {
	nthw_fpga_t *mp_fpga;

	int mn_phy_tile_instance;

	int mn_fpga_version;
	int mn_fpga_revision;

	nthw_register_t *mp_reg_port_xcvr_base[2][4];
	nthw_field_t *mp_fld_port_xcvr_base_ptr[2][4];
	nthw_field_t *mp_fld_port_xcvr_base_busy[2][4];
	nthw_field_t *mp_fld_port_xcvr_base_cmd[2][4];

	nthw_field_t *mp_fld_port_xcvr_data_data[2][4];

	nthw_register_t *mp_reg_port_eth_base[2];
	nthw_field_t *mp_fld_port_eth_base_ptr[2];
	nthw_field_t *mp_fld_port_eth_base_busy[2];
	nthw_field_t *mp_fld_port_eth_base_cmd[2];

	nthw_field_t *mp_fld_port_eth_data_data[2];

	nthw_register_t *mp_reg_link_summary[2];
	nthw_field_t *mp_fld_link_summary_nt_phy_link_state[2];
	nthw_field_t *mp_fld_link_summary_ll_nt_phy_link_state[2];
	nthw_field_t *mp_fld_link_summary_link_down_cnt[2];
	nthw_field_t *mp_fld_link_summary_lh_received_local_fault[2];
	nthw_field_t *mp_fld_link_summary_lh_remote_fault[2];

	nthw_field_t *mp_fld_port_status_rx_hi_ber[2];
	nthw_field_t *mp_fld_port_status_rx_am_lock[2];
	nthw_field_t *mp_fld_port_status_tx_reset_ackn[2];
	nthw_field_t *mp_fld_port_status_rx_reset_ackn[2];

	nthw_field_t *mp_fld_port_config_reset[2];
	nthw_field_t *mp_fld_port_config_rx_reset[2];
	nthw_field_t *mp_fld_port_config_tx_reset[2];

	nthw_field_t *mp_fld_port_comp_rx_compensation[2];

	nthw_register_t *mp_reg_dyn_reconfig_base;
	nthw_field_t *mp_fld_dyn_reconfig_base_ptr;
	nthw_field_t *mp_fld_dyn_reconfig_base_busy;
	nthw_field_t *mp_fld_dyn_reconfig_base_cmd;

	nthw_register_t *mp_reg_dyn_reconfig_data;
	nthw_field_t *mp_fld_dyn_reconfig_data_data;

	nthw_field_t *mp_fld_scratch_data;

	nthw_register_t *mp_reg_dr_cfg_status;
	nthw_field_t *mp_fld_dr_cfg_status_curr_profile_id;
	nthw_field_t *mp_fld_dr_cfg_status_in_progress;
	nthw_field_t *mp_fld_dr_cfg_status_error;
};

typedef struct nt_phy_tile nthw_phy_tile_t;
typedef struct nt_phy_tile nt_phy_tile;

void nthw_phy_tile_set_tx_pol_inv(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, bool invert);
void nthw_phy_tile_set_rx_pol_inv(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, bool invert);
void nthw_phy_tile_set_host_loopback(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	bool enable);
void nthw_phy_tile_set_tx_equalization(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t pre_tap2, uint32_t main_tap, uint32_t pre_tap1,
	uint32_t post_tap1);
void nthw_phy_tile_set_tx_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset);
void nthw_phy_tile_set_rx_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset);

uint32_t nthw_phy_tile_read_eth(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t address);
void nthw_phy_tile_write_eth(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t address, uint32_t data);
bool nthw_phy_tile_configure_fec(nthw_phy_tile_t *p, uint8_t intf_no, bool enable);
uint32_t nthw_phy_tile_read_xcvr(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t address);
void nthw_phy_tile_write_xcvr(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, uint32_t address,
	uint32_t data);

#endif	/* __NTHW_PHY_TILE_H__ */
