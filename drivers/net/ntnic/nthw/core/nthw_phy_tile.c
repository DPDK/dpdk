/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_phy_tile.h"
#include "nthw_helper.h"

static const uint32_t link_addr = 0x9003C;
static const uint32_t phy_addr = 0x90040;
static const uint32_t tx_eq_addr = 0x47830;
/* CPI option flags */
static const uint32_t cpi_set = 0x2000;	/* 1 << 13 */
static const uint32_t cpi_in_reset = 0x4000;	/* 1 << 14 */
static const uint32_t bit_cpi_assert = 0x8000;	/* 1 << 15 */
static const uint32_t dyn_rcfg_dr_trigger_reg;
static const uint32_t dyn_rcfg_dr_next_profile_0_reg = 0x4;
static const uint32_t dyn_rcfg_dr_next_profile_1_reg = 0x8;
static const uint32_t dyn_rcfg_dr_next_profile_2_reg = 0xc;
/*
 * Base Addresses for Avalon MM Secondary Components
 * static const uint32_t ADDR_AVMM_DR_CTRL_INT                  =  0x10035000 << 0;
 * static const uint32_t ADDR_GDR_P0_XCVR_RECONFIG_INT          =  0x00800000 << 0;
 * static const uint32_t ADDR_GDR_P1_XCVR_RECONFIG_INT          =  0x00900000 << 0;
 * static const uint32_t ADDR_GDR_P2_XCVR_RECONFIG_INT          =  0x00A00000 << 0;
 * static const uint32_t ADDR_GDR_P3_XCVR_RECONFIG_INT          =  0x00B00000 << 0;
 * static const uint32_t ADDR_GDR_P0_ETH_RECONFIG_INT           =  0x00000000 << 0;
 * static const uint32_t ADDR_GDR_P1_ETH_RECONFIG_INT           =  0x00010000 << 0;
 * static const uint32_t ADDR_GDR_P2_ETH_RECONFIG_INT           =  0x00020000 << 0;
 * static const uint32_t ADDR_GDR_P3_ETH_RECONFIG_INT           =  0x00030000 << 0;
 * static const uint32_t ADDR_GDR_PKT_CLIENT_CONFIG_INT         =  0x000100000 << 0;
 * static const uint32_t ADDR_GDR_P0_25G_PKT_CLIENT_CONFIG_INT  =  0x000200000 << 0;
 * static const uint32_t ADDR_GDR_P0_50G_PKT_CLIENT_CONFIG_INT  =  0x000300000 << 0;
 *
 * ETH AVMM USER SPACE Registers Address Map
 */
static const uint32_t eth_soft_csr1 = 0x200;
static const uint32_t eth_soft_csr2 = 0x204;
static const uint32_t eth_soft_csr3 = 0x208;

nthw_phy_tile_t *nthw_phy_tile_new(void)
{
	nthw_phy_tile_t *p = malloc(sizeof(nthw_phy_tile_t));

	if (p)
		memset(p, 0, sizeof(nthw_phy_tile_t));

	return p;
}

int nthw_phy_tile_init(nthw_phy_tile_t *p, nthw_fpga_t *p_fpga, int mn_phy_tile_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_PHY_TILE, mn_phy_tile_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: PHY_TILE %d: no such instance", p_adapter_id_str,
			mn_phy_tile_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_phy_tile_instance = mn_phy_tile_instance;

	p->m_mod_phy_tile = mod;

	p->mn_fpga_revision = p_fpga->mn_fpga_revision;
	p->mn_fpga_version = p_fpga->mn_fpga_version;

	switch (p_fpga->mn_product_id) {
	case 9569:
	case 9574:
		p->mac_pcs_mode = MAC_PCS_MODE_2X100;
		break;

	default:
		NT_LOG_DBG(DBG, NTHW, "unknown product ID: %i", p_fpga->mn_product_id);
		break;
	}

	/* Port 0 Base 0..3 */
	p->mp_reg_port_xcvr_base[0][0] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_0_BASE);
	p->mp_fld_port_xcvr_base_ptr[0][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][0],
			PHY_TILE_PORT_0_XCVR_0_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[0][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][0],
			PHY_TILE_PORT_0_XCVR_0_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[0][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][0],
			PHY_TILE_PORT_0_XCVR_0_BASE_CMD);

	p->mp_reg_port_xcvr_base[0][1] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_1_BASE);
	p->mp_fld_port_xcvr_base_ptr[0][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][1],
			PHY_TILE_PORT_0_XCVR_1_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[0][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][1],
			PHY_TILE_PORT_0_XCVR_1_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[0][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][1],
			PHY_TILE_PORT_0_XCVR_1_BASE_CMD);

	p->mp_reg_port_xcvr_base[0][2] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_2_BASE);
	p->mp_fld_port_xcvr_base_ptr[0][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][2],
			PHY_TILE_PORT_0_XCVR_2_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[0][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][2],
			PHY_TILE_PORT_0_XCVR_2_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[0][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][2],
			PHY_TILE_PORT_0_XCVR_2_BASE_CMD);

	p->mp_reg_port_xcvr_base[0][3] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_3_BASE);
	p->mp_fld_port_xcvr_base_ptr[0][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][3],
			PHY_TILE_PORT_0_XCVR_3_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[0][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][3],
			PHY_TILE_PORT_0_XCVR_3_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[0][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[0][3],
			PHY_TILE_PORT_0_XCVR_3_BASE_CMD);

	/* Port 0 Data 0..3 */
	p->mp_reg_port_xcvr_data[0][0] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_0_DATA);
	p->mp_fld_port_xcvr_data_data[0][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[0][0],
			PHY_TILE_PORT_0_XCVR_0_DATA_DATA);

	p->mp_reg_port_xcvr_data[0][1] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_1_DATA);
	p->mp_fld_port_xcvr_data_data[0][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[0][1],
			PHY_TILE_PORT_0_XCVR_1_DATA_DATA);

	p->mp_reg_port_xcvr_data[0][2] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_2_DATA);
	p->mp_fld_port_xcvr_data_data[0][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[0][2],
			PHY_TILE_PORT_0_XCVR_2_DATA_DATA);

	p->mp_reg_port_xcvr_data[0][3] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_XCVR_3_DATA);
	p->mp_fld_port_xcvr_data_data[0][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[0][3],
			PHY_TILE_PORT_0_XCVR_3_DATA_DATA);

	/* Port 1 Base 0..3 */
	p->mp_reg_port_xcvr_base[1][0] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_0_BASE);
	p->mp_fld_port_xcvr_base_ptr[1][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][0],
			PHY_TILE_PORT_1_XCVR_0_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[1][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][0],
			PHY_TILE_PORT_1_XCVR_0_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[1][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][0],
			PHY_TILE_PORT_1_XCVR_0_BASE_CMD);

	p->mp_reg_port_xcvr_base[1][1] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_1_BASE);
	p->mp_fld_port_xcvr_base_ptr[1][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][1],
			PHY_TILE_PORT_1_XCVR_1_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[1][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][1],
			PHY_TILE_PORT_1_XCVR_1_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[1][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][1],
			PHY_TILE_PORT_1_XCVR_1_BASE_CMD);

	p->mp_reg_port_xcvr_base[1][2] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_2_BASE);
	p->mp_fld_port_xcvr_base_ptr[1][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][2],
			PHY_TILE_PORT_1_XCVR_2_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[1][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][2],
			PHY_TILE_PORT_1_XCVR_2_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[1][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][2],
			PHY_TILE_PORT_1_XCVR_2_BASE_CMD);

	p->mp_reg_port_xcvr_base[1][3] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_3_BASE);
	p->mp_fld_port_xcvr_base_ptr[1][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][3],
			PHY_TILE_PORT_1_XCVR_3_BASE_PTR);
	p->mp_fld_port_xcvr_base_busy[1][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][3],
			PHY_TILE_PORT_1_XCVR_3_BASE_BUSY);
	p->mp_fld_port_xcvr_base_cmd[1][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_base[1][3],
			PHY_TILE_PORT_1_XCVR_3_BASE_CMD);

	/* Port 1 Data 0..3 */
	p->mp_reg_port_xcvr_data[1][0] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_0_DATA);
	p->mp_fld_port_xcvr_data_data[1][0] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[1][0],
			PHY_TILE_PORT_1_XCVR_0_DATA_DATA);

	p->mp_reg_port_xcvr_data[1][1] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_1_DATA);
	p->mp_fld_port_xcvr_data_data[1][1] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[1][1],
			PHY_TILE_PORT_1_XCVR_1_DATA_DATA);

	p->mp_reg_port_xcvr_data[1][2] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_2_DATA);
	p->mp_fld_port_xcvr_data_data[1][2] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[1][2],
			PHY_TILE_PORT_1_XCVR_2_DATA_DATA);

	p->mp_reg_port_xcvr_data[1][3] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_XCVR_3_DATA);
	p->mp_fld_port_xcvr_data_data[1][3] =
		nthw_register_get_field(p->mp_reg_port_xcvr_data[1][3],
			PHY_TILE_PORT_1_XCVR_3_DATA_DATA);

	if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 3)) {
		if (p->mac_pcs_mode == MAC_PCS_MODE_2X100 ||
			p->mac_pcs_mode == MAC_PCS_MODE_8x10_25) {
			/* DYN_RECONFIG */
			p->mp_reg_dyn_reconfig_base =
				nthw_module_get_register(p->m_mod_phy_tile,
					PHY_TILE_DYN_RECONFIG_BASE);
			p->mp_fld_dyn_reconfig_base_ptr =
				nthw_register_get_field(p->mp_reg_dyn_reconfig_base,
					PHY_TILE_DYN_RECONFIG_BASE_PTR);
			p->mp_fld_dyn_reconfig_base_busy =
				nthw_register_get_field(p->mp_reg_dyn_reconfig_base,
					PHY_TILE_DYN_RECONFIG_BASE_BUSY);
			p->mp_fld_dyn_reconfig_base_cmd =
				nthw_register_get_field(p->mp_reg_dyn_reconfig_base,
					PHY_TILE_DYN_RECONFIG_BASE_CMD);

			p->mp_reg_dyn_reconfig_data =
				nthw_module_get_register(p->m_mod_phy_tile,
					PHY_TILE_DYN_RECONFIG_DATA);
			p->mp_fld_dyn_reconfig_data_data =
				nthw_register_get_field(p->mp_reg_dyn_reconfig_data,
					PHY_TILE_DYN_RECONFIG_DATA_DATA);
		}

		/* Port 0 Eth Base */
		p->mp_reg_port_eth_base[0] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_ETH_0_BASE);
		p->mp_fld_port_eth_base_ptr[0] =
			nthw_register_get_field(p->mp_reg_port_eth_base[0],
				PHY_TILE_PORT_0_ETH_0_BASE_PTR);
		p->mp_fld_port_eth_base_busy[0] =
			nthw_register_get_field(p->mp_reg_port_eth_base[0],
				PHY_TILE_PORT_0_ETH_0_BASE_BUSY);
		p->mp_fld_port_eth_base_cmd[0] =
			nthw_register_get_field(p->mp_reg_port_eth_base[0],
				PHY_TILE_PORT_0_ETH_0_BASE_CMD);

		/* Port 1 Eth Base */
		p->mp_reg_port_eth_base[1] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_ETH_0_BASE);
		p->mp_fld_port_eth_base_ptr[1] =
			nthw_register_get_field(p->mp_reg_port_eth_base[1],
				PHY_TILE_PORT_1_ETH_0_BASE_PTR);
		p->mp_fld_port_eth_base_busy[1] =
			nthw_register_get_field(p->mp_reg_port_eth_base[1],
				PHY_TILE_PORT_1_ETH_0_BASE_BUSY);
		p->mp_fld_port_eth_base_cmd[1] =
			nthw_register_get_field(p->mp_reg_port_eth_base[1],
				PHY_TILE_PORT_1_ETH_0_BASE_CMD);

		/* Port 0 Eth Data */
		p->mp_reg_port_eth_data[0] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_0_ETH_0_DATA);
		p->mp_fld_port_eth_data_data[0] =
			nthw_register_get_field(p->mp_reg_port_eth_data[0],
				PHY_TILE_PORT_0_ETH_0_DATA_DATA);

		/* Port 1 Eth Data 0..3 */
		p->mp_reg_port_eth_data[1] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_1_ETH_0_DATA);
		p->mp_fld_port_eth_data_data[1] =
			nthw_register_get_field(p->mp_reg_port_eth_data[1],
				PHY_TILE_PORT_1_ETH_0_DATA_DATA);
	}

	p->mp_reg_link_summary[0] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_LINK_SUMMARY_0);
	p->mp_fld_link_summary_nt_phy_link_state[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_NT_PHY_LINK_STATE);
	p->mp_fld_link_summary_ll_nt_phy_link_state[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LL_PHY_LINK_STATE);
	p->mp_fld_link_summary_link_down_cnt[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LINK_DOWN_CNT);
	p->mp_fld_link_summary_ll_rx_block_lock[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LL_RX_BLOCK_LOCK);
	p->mp_fld_link_summary_ll_rx_am_lock[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LL_RX_AM_LOCK);
	p->mp_fld_link_summary_lh_rx_high_bit_error_rate[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LH_RX_HIGH_BIT_ERROR_RATE);
	p->mp_fld_link_summary_lh_received_local_fault[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LH_RECEIVED_LOCAL_FAULT);
	p->mp_fld_link_summary_lh_remote_fault[0] =
		nthw_register_get_field(p->mp_reg_link_summary[0],
			PHY_TILE_LINK_SUMMARY_0_LH_REMOTE_FAULT);

	p->mp_reg_link_summary[1] =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_LINK_SUMMARY_1);
	p->mp_fld_link_summary_nt_phy_link_state[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_NT_PHY_LINK_STATE);
	p->mp_fld_link_summary_ll_nt_phy_link_state[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LL_PHY_LINK_STATE);
	p->mp_fld_link_summary_link_down_cnt[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LINK_DOWN_CNT);
	p->mp_fld_link_summary_ll_rx_block_lock[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LL_RX_BLOCK_LOCK);
	p->mp_fld_link_summary_ll_rx_am_lock[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LL_RX_AM_LOCK);
	p->mp_fld_link_summary_lh_rx_high_bit_error_rate[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LH_RX_HIGH_BIT_ERROR_RATE);
	p->mp_fld_link_summary_lh_received_local_fault[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LH_RECEIVED_LOCAL_FAULT);
	p->mp_fld_link_summary_lh_remote_fault[1] =
		nthw_register_get_field(p->mp_reg_link_summary[1],
			PHY_TILE_LINK_SUMMARY_1_LH_REMOTE_FAULT);

	if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 4)) {
		p->mp_reg_port_status[0] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_STATUS_0);
		p->mp_fld_port_status_rx_pcs_fully_aligned[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RX_PCS_FULLY_ALIGNED);
		p->mp_fld_port_status_rx_hi_ber[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RX_HI_BER);
		p->mp_fld_port_status_rx_remote_fault[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RX_REMOTE_FAULT);
		p->mp_fld_port_status_rx_local_fault[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RX_LOCAL_FAULT);
		p->mp_fld_port_status_rx_am_lock[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RX_AM_LOCK);
		p->mp_fld_port_status_reset_ackn[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RESET_ACK_N);
		p->mp_fld_port_status_tx_lanes_stable[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_TX_LANES_STABLE);
		p->mp_fld_port_status_tx_pll_locked[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_TX_PLL_LOCKED);
		p->mp_fld_port_status_sys_pll_locked[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_SYS_PLL_LOCKED);
		p->mp_fld_port_status_tx_reset_ackn[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_TX_RESET_ACK_N);
		p->mp_fld_port_status_rx_reset_ackn[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_0_RX_RESET_ACK_N);

		p->mp_reg_port_status[1] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_STATUS_1);
		p->mp_fld_port_status_rx_pcs_fully_aligned[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RX_PCS_FULLY_ALIGNED);
		p->mp_fld_port_status_rx_hi_ber[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RX_HI_BER);
		p->mp_fld_port_status_rx_remote_fault[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RX_REMOTE_FAULT);
		p->mp_fld_port_status_rx_local_fault[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RX_LOCAL_FAULT);
		p->mp_fld_port_status_rx_am_lock[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RX_AM_LOCK);
		p->mp_fld_port_status_reset_ackn[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RESET_ACK_N);
		p->mp_fld_port_status_tx_lanes_stable[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_TX_LANES_STABLE);
		p->mp_fld_port_status_tx_pll_locked[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_TX_PLL_LOCKED);
		p->mp_fld_port_status_sys_pll_locked[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_SYS_PLL_LOCKED);
		p->mp_fld_port_status_tx_reset_ackn[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_TX_RESET_ACK_N);
		p->mp_fld_port_status_rx_reset_ackn[1] =
			nthw_register_get_field(p->mp_reg_port_status[1],
				PHY_TILE_PORT_STATUS_1_RX_RESET_ACK_N);

	} else {
		p->mp_reg_port_status[0] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_STATUS);
		p->mp_fld_port_status_rx_pcs_fully_aligned[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_PCS_FULLY_ALIGNED_0);
		p->mp_fld_port_status_rx_hi_ber[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_HI_BER_0);
		p->mp_fld_port_status_rx_remote_fault[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_REMOTE_FAULT_0);
		p->mp_fld_port_status_rx_local_fault[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_LOCAL_FAULT_0);
		p->mp_fld_port_status_rx_am_lock[0] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_AM_LOCK_0);

		p->mp_fld_port_status_rx_pcs_fully_aligned[1] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_PCS_FULLY_ALIGNED_1);
		p->mp_fld_port_status_rx_hi_ber[1] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_HI_BER_1);
		p->mp_fld_port_status_rx_remote_fault[1] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_REMOTE_FAULT_1);
		p->mp_fld_port_status_rx_local_fault[1] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_LOCAL_FAULT_1);
		p->mp_fld_port_status_rx_am_lock[1] =
			nthw_register_get_field(p->mp_reg_port_status[0],
				PHY_TILE_PORT_STATUS_RX_AM_LOCK_1);

		p->mp_fld_port_status_reset_ackn[0] = NULL;
		p->mp_fld_port_status_tx_lanes_stable[0] = NULL;
		p->mp_fld_port_status_tx_pll_locked[0] = NULL;
		p->mp_fld_port_status_sys_pll_locked[0] = NULL;
		p->mp_fld_port_status_tx_reset_ackn[0] = NULL;
		p->mp_fld_port_status_rx_reset_ackn[0] = NULL;

		p->mp_fld_port_status_reset_ackn[1] = NULL;
		p->mp_fld_port_status_tx_lanes_stable[1] = NULL;
		p->mp_fld_port_status_tx_pll_locked[1] = NULL;
		p->mp_fld_port_status_sys_pll_locked[1] = NULL;
		p->mp_fld_port_status_tx_reset_ackn[1] = NULL;
		p->mp_fld_port_status_rx_reset_ackn[1] = NULL;
	}

	if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 4)) {
		p->mp_reg_port_config[0] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_CONFIG_0);
		p->mp_fld_port_config_reset[0] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_0_RST);
		p->mp_fld_port_config_rx_reset[0] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_0_RX_RST);
		p->mp_fld_port_config_tx_reset[0] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_0_TX_RST);
		p->mp_fld_port_config_nt_linkup_latency[0] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_0_NT_LINKUP_LATENCY);
		p->mp_fld_port_config_nt_force_linkdown[0] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_0_NT_FORCE_LINK_DOWN);

		if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 10)) {
			p->mp_fld_port_config_nt_auto_force_linkdown[0] =
				nthw_register_query_field(p->mp_reg_port_config[0],
					PHY_TILE_PORT_CONFIG_0_NT_AUTO_FORCE_LINK_DOWN);
		}

		p->mp_reg_port_config[1] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_CONFIG_1);
		p->mp_fld_port_config_reset[1] =
			nthw_register_query_field(p->mp_reg_port_config[1],
				PHY_TILE_PORT_CONFIG_1_RST);
		p->mp_fld_port_config_rx_reset[1] =
			nthw_register_query_field(p->mp_reg_port_config[1],
				PHY_TILE_PORT_CONFIG_1_RX_RST);
		p->mp_fld_port_config_tx_reset[1] =
			nthw_register_query_field(p->mp_reg_port_config[1],
				PHY_TILE_PORT_CONFIG_1_TX_RST);
		p->mp_fld_port_config_nt_linkup_latency[1] =
			nthw_register_query_field(p->mp_reg_port_config[1],
				PHY_TILE_PORT_CONFIG_1_NT_LINKUP_LATENCY);
		p->mp_fld_port_config_nt_force_linkdown[1] =
			nthw_register_query_field(p->mp_reg_port_config[1],
				PHY_TILE_PORT_CONFIG_1_NT_FORCE_LINK_DOWN);

		if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 10)) {
			p->mp_fld_port_config_nt_auto_force_linkdown[1] =
				nthw_register_query_field(p->mp_reg_port_config[1],
					PHY_TILE_PORT_CONFIG_1_NT_AUTO_FORCE_LINK_DOWN);
		}

	} else {
		p->mp_reg_port_config[0] =
			nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_CONFIG);
		p->mp_fld_port_config_dyn_reset =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_DYN_RESET);
		p->mp_fld_port_config_reset[0] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_RESET_0);
		p->mp_fld_port_config_rx_reset[0] =
			nthw_register_get_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_RX_RESET_0);
		p->mp_fld_port_config_tx_reset[0] =
			nthw_register_get_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_TX_RESET_0);
		p->mp_fld_port_config_reset[1] =
			nthw_register_query_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_RESET_1);
		p->mp_fld_port_config_rx_reset[1] =
			nthw_register_get_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_RX_RESET_1);
		p->mp_fld_port_config_tx_reset[1] =
			nthw_register_get_field(p->mp_reg_port_config[0],
				PHY_TILE_PORT_CONFIG_TX_RESET_1);
	}

	p->mp_reg_port_comp[0] = nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_COMP_0);
	p->mp_fld_port_comp_rx_compensation[0] =
		nthw_register_get_field(p->mp_reg_port_comp[0],
			PHY_TILE_PORT_COMP_0_RX_COMPENSATION);
	p->mp_fld_port_comp_tx_compensation[0] =
		nthw_register_get_field(p->mp_reg_port_comp[0],
			PHY_TILE_PORT_COMP_0_TX_COMPENSATION);
	p->mp_reg_port_comp[1] = nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_PORT_COMP_1);
	p->mp_fld_port_comp_rx_compensation[1] =
		nthw_register_get_field(p->mp_reg_port_comp[1],
			PHY_TILE_PORT_COMP_1_RX_COMPENSATION);
	p->mp_fld_port_comp_tx_compensation[1] =
		nthw_register_get_field(p->mp_reg_port_comp[1],
			PHY_TILE_PORT_COMP_1_TX_COMPENSATION);

	p->mp_reg_scratch = nthw_module_query_register(p->m_mod_phy_tile, PHY_TILE_SCRATCH);

	if (p->mp_reg_scratch) {
		p->mp_fld_scratch_data =
			nthw_register_query_field(p->mp_reg_scratch, PHY_TILE_SCRATCH_DATA);
	}

	if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 4)) {
		p->mp_reg_dr_cfg = nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_DR_CFG);
		p->mp_fld_port_config_dyn_reset =
			nthw_register_get_field(p->mp_reg_dr_cfg, PHY_TILE_DR_CFG_DYN_RST);

		if (!p->mp_reg_scratch) {
			p->mp_fld_scratch_data =
				nthw_register_get_field(p->mp_reg_dr_cfg, PHY_TILE_DR_CFG_SCRATCH);
		}

		p->mp_fld_reg_dr_cfg_features =
			nthw_register_get_field(p->mp_reg_dr_cfg, PHY_TILE_DR_CFG_FEATURES);

		if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 10)) {
			p->mp_fld_reg_dr_cfg_tx_flush_level =
				nthw_register_get_field(p->mp_reg_dr_cfg,
					PHY_TILE_DR_CFG_TX_FLUSH_LEVEL);
		}
	}

	p->mp_reg_dr_cfg_status =
		nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_DR_CFG_STATUS);
	p->mp_fld_dr_cfg_status_curr_profile_id =
		nthw_register_get_field(p->mp_reg_dr_cfg_status,
			PHY_TILE_DR_CFG_STATUS_CURR_PROFILE_ID);
	p->mp_fld_dr_cfg_status_in_progress =
		nthw_register_get_field(p->mp_reg_dr_cfg_status,
			PHY_TILE_DR_CFG_STATUS_IN_PROGRESS);
	p->mp_fld_dr_cfg_status_error =
		nthw_register_get_field(p->mp_reg_dr_cfg_status, PHY_TILE_DR_CFG_STATUS_ERROR);

	if (nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 7)) {
		p->mp_reg_sys_pll = nthw_module_get_register(p->m_mod_phy_tile, PHY_TILE_SYS_PLL);
		p->mp_fld_sys_pll_set_rdy =
			nthw_register_get_field(p->mp_reg_sys_pll, PHY_TILE_SYS_PLL_SET_RDY);
		p->mp_fld_sys_pll_get_rdy =
			nthw_register_get_field(p->mp_reg_sys_pll, PHY_TILE_SYS_PLL_GET_RDY);
		p->mp_fld_sys_pll_system_pll_lock =
			nthw_register_get_field(p->mp_reg_sys_pll,
				PHY_TILE_SYS_PLL_SYSTEMPLL_LOCK);
		p->mp_fld_sys_pll_en_ref_clk_fgt =
			nthw_register_get_field(p->mp_reg_sys_pll, PHY_TILE_SYS_PLL_EN_REFCLK_FGT);
		p->mp_fld_sys_pll_disable_ref_clk_monitor =
			nthw_register_get_field(p->mp_reg_sys_pll,
				PHY_TILE_SYS_PLL_DISABLE_REFCLK_MONITOR);
		p->mp_fld_sys_pll_ref_clk_fgt_enabled =
			nthw_register_get_field(p->mp_reg_sys_pll,
				PHY_TILE_SYS_PLL_REFCLK_FGT_ENABLED);
		p->mp_fld_sys_pll_forward_rst =
			nthw_register_get_field(p->mp_reg_sys_pll, PHY_TILE_SYS_PLL_FORWARD_RST);
		p->mp_fld_sys_pll_force_rst =
			nthw_register_get_field(p->mp_reg_sys_pll, PHY_TILE_SYS_PLL_FORCE_RST);

	} else {
		p->mp_fld_sys_pll_set_rdy = NULL;
		p->mp_fld_sys_pll_get_rdy = NULL;
		p->mp_fld_sys_pll_system_pll_lock = NULL;
		p->mp_fld_sys_pll_en_ref_clk_fgt = NULL;
		p->mp_fld_sys_pll_disable_ref_clk_monitor = NULL;
		p->mp_fld_sys_pll_ref_clk_fgt_enabled = NULL;
		p->mp_fld_sys_pll_forward_rst = NULL;
		p->mp_fld_sys_pll_force_rst = NULL;
	}

	return 0;
}

bool nthw_phy_tile_use_phy_tile_pll_check(nthw_phy_tile_t *p)
{
	return nthw_module_is_version_newer(p->m_mod_phy_tile, 0, 8);
}

uint8_t nthw_phy_tile_get_no_intfs(nthw_phy_tile_t *p)
{
	switch (p->mac_pcs_mode) {
	case MAC_PCS_MODE_2X100:
		return 2;

	default:
		return 0;
	}
}

static void nthw_phy_tile_set_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset)
{
	/* Reset is active low */
	nthw_field_get_updated(p->mp_fld_port_config_reset[intf_no]);

	if (reset)
		nthw_field_clr_flush(p->mp_fld_port_config_reset[intf_no]);

	else
		nthw_field_set_flush(p->mp_fld_port_config_reset[intf_no]);
}

void nthw_phy_tile_set_rx_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset)
{
	/* Reset is active low */
	nthw_field_get_updated(p->mp_fld_port_config_rx_reset[intf_no]);

	if (reset) {
		nthw_field_clr_flush(p->mp_fld_port_config_rx_reset[intf_no]);

		/* Wait for ack */
		if (p->mp_fld_port_status_rx_reset_ackn[intf_no]) {
			int32_t count = 1000;

			do {
				nt_os_wait_usec(1000);	/* 1ms */
			} while (nthw_field_get_updated(p->mp_fld_port_status_rx_reset_ackn
				[intf_no]) && (--count > 0));

			if (count <= 0) {
				NT_LOG(ERR, NTHW, "intf_no %u: Time-out waiting for RxResetAck",
					intf_no);
			}
		}

	} else {
		nthw_field_set_flush(p->mp_fld_port_config_rx_reset[intf_no]);
	}
}

void nthw_phy_tile_set_tx_reset(nthw_phy_tile_t *p, uint8_t intf_no, bool reset)
{
	/* Reset is active low */
	nthw_field_get_updated(p->mp_fld_port_config_tx_reset[intf_no]);

	if (reset) {
		nthw_field_clr_flush(p->mp_fld_port_config_tx_reset[intf_no]);

		/* Wait for ack */
		if (p->mp_fld_port_status_tx_reset_ackn[intf_no]) {
			int32_t count = 1000;

			do {
				nt_os_wait_usec(1000);	/* 1ms */
			} while (nthw_field_get_updated(p->mp_fld_port_status_tx_reset_ackn
				[intf_no]) && (--count > 0));

			if (count <= 0) {
				NT_LOG(ERR, NTHW, "intf_no %u: Time-out waiting for TxResetAck",
					intf_no);
			}
		}

	} else {
		nthw_field_set_flush(p->mp_fld_port_config_tx_reset[intf_no]);
	}
}

uint32_t nthw_phy_tile_read_xcvr(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t address)
{
	nthw_register_update(p->mp_reg_port_xcvr_base[intf_no][lane]);
	nthw_field_set_val32(p->mp_fld_port_xcvr_base_cmd[intf_no][lane], 0);
	nthw_field_set_val_flush32(p->mp_fld_port_xcvr_base_ptr[intf_no][lane], address);

	while (nthw_field_get_updated(p->mp_fld_port_xcvr_base_busy[intf_no][lane]) == 1)
		nt_os_wait_usec(100);

	return nthw_field_get_updated(p->mp_fld_port_xcvr_data_data[intf_no][lane]);
}

void nthw_phy_tile_write_xcvr(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, uint32_t address,
	uint32_t data)
{
	nthw_field_set_val_flush32(p->mp_fld_port_xcvr_data_data[intf_no][lane], data);
	nthw_register_update(p->mp_reg_port_xcvr_base[intf_no][lane]);
	nthw_field_set_val32(p->mp_fld_port_xcvr_base_ptr[intf_no][lane], address);
	nthw_field_set_val_flush32(p->mp_fld_port_xcvr_base_cmd[intf_no][lane], 1);

	while (nthw_field_get_updated(p->mp_fld_port_xcvr_base_busy[intf_no][lane]) == 1)
		/* NT_LOG(INF, NTHW, "busy"); */
		nt_os_wait_usec(100);
}

static uint32_t nthw_phy_tile_read_dyn_reconfig(nthw_phy_tile_t *p, uint32_t address)
{
	nthw_register_update(p->mp_reg_dyn_reconfig_base);
	nthw_field_set_val32(p->mp_fld_dyn_reconfig_base_cmd, 0);
	nthw_field_set_val_flush32(p->mp_fld_dyn_reconfig_base_ptr, address);

	while (nthw_field_get_updated(p->mp_fld_dyn_reconfig_base_busy) == 1)
		nt_os_wait_usec(100);

	uint32_t value = nthw_field_get_updated(p->mp_fld_dyn_reconfig_data_data);
	/*
	 * NT_LOG(INF, NTHW, "nthw_phy_tile_read_dyn_reconfig address %0x value,
	 * %0x", address, value);
	 */
	return value;
	/* return nthw_field_get_updated(p->mp_fld_dyn_reconfig_data_data); */
}

static void nthw_phy_tile_write_dyn_reconfig(nthw_phy_tile_t *p, uint32_t address, uint32_t data)
{
	nthw_register_update(p->mp_reg_dyn_reconfig_data);
	nthw_field_set_val_flush32(p->mp_fld_dyn_reconfig_data_data, data);

	nthw_register_update(p->mp_reg_dyn_reconfig_base);
	nthw_field_set_val32(p->mp_fld_dyn_reconfig_base_ptr, address);
	nthw_field_set_val_flush32(p->mp_fld_dyn_reconfig_base_cmd, 1);

	while (nthw_field_get_updated(p->mp_fld_dyn_reconfig_base_busy) == 1)
		/* NT_LOG(INF, NTHW, "busy"); */
		nt_os_wait_usec(100);
}

static uint32_t nthw_phy_tile_cpi_request(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t data, uint8_t op_code, uint32_t cpi_assert,
	uint32_t cpi_set_get)
{
	uint32_t cpi_cmd;
	uint32_t value = 0;
	uint32_t xcvr_instance = lane;
	uint32_t lane_offset = 0;
	uint32_t quad_lane = 0;

	/* Find quad lane */
	quad_lane = nthw_phy_tile_read_xcvr(p, intf_no, lane, 0xFFFFC) & 0x3;

	lane_offset = (uint32_t)(lane << 20);

	cpi_cmd = (data << 16) | cpi_assert | cpi_set_get | (quad_lane << 8) | op_code;

	nthw_phy_tile_write_xcvr(p, intf_no, lane, link_addr + lane_offset, cpi_cmd);

	nt_os_wait_usec(10000);

	for (int i = 20; i >= 0; i--) {
		data = nthw_phy_tile_read_xcvr(p, intf_no, lane, phy_addr + lane_offset);

		value =
			nthw_field_get_updated(p->mp_fld_port_xcvr_data_data
				[intf_no][xcvr_instance]);

		if (((value & bit_cpi_assert) == cpi_assert) && ((value & cpi_in_reset) == 0))
			break;

		nt_os_wait_usec(10000);

		if (i == 0)
			NT_LOG(ERR, NTHW, "Time out");
	}

	return value;
}

static void nthw_phy_tile_write_attribute(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint8_t op_code, uint8_t data)
{
	/*
	 * p->cpi_set_get = 0x2000 #bit13: (1: set, 0: get)
	 * cpi_assert = 0x0000 #bit15: (1: assert, 0: deassert)
	 */
	nthw_phy_tile_cpi_request(p, intf_no, lane, data, op_code, bit_cpi_assert, cpi_set);
	nthw_phy_tile_cpi_request(p, intf_no, lane, data, op_code, 0x0000, cpi_set);
}

void nthw_phy_tile_set_tx_pol_inv(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, bool invert)
{
	nthw_phy_tile_write_attribute(p, intf_no, lane, 0x65, invert ? 1 : 0);
	/*
	 * NT_LOG(INF, NTHW, "setTxPolInv intf_no %d, lane %d, inv %d ", intf_no, lane, invert);
	 * nthw_phy_tile_get_tx_pol_inv(p, intf_no, lane);
	 */
}

void nthw_phy_tile_set_rx_pol_inv(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane, bool invert)
{
	nthw_phy_tile_write_attribute(p, intf_no, lane, 0x66, invert ? 1 : 0);
	/*
	 * NT_LOG(INF, NTHW, "setRxPolInv intf_no %d, lane %d, inv %d ", intf_no, lane, invert);
	 * nthw_phy_tile_get_rx_pol_inv(p, intf_no, lane);
	 */
}

void nthw_phy_tile_set_host_loopback(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	bool enable)
{
	nthw_phy_tile_set_rx_reset(p, intf_no, true);
	nthw_phy_tile_write_attribute(p, intf_no, lane, 0x40, enable ? 0x06 : 0);
	nthw_phy_tile_set_rx_reset(p, intf_no, false);
	/*
	 * NT_LOG(INF, NTHW, "setLoopback intf_no %d, lane %d, enable %d ",
	 * intf_no, lane, enable);
	 */
}

void nthw_phy_tile_set_tx_equalization(nthw_phy_tile_t *p, uint8_t intf_no, uint8_t lane,
	uint32_t pre_tap2, uint32_t main_tap, uint32_t pre_tap1,
	uint32_t post_tap1)
{
	uint32_t data = (pre_tap2 << 16) + (main_tap << 10) + (pre_tap1 << 5) + (post_tap1);
	NT_LOG(DBG, NTHW,
		"intf_no %u: setTxEq lane %u, pre_tap2 %u, main_tap %u, pre_tap1 %u, post_tap1 %u data %x",
		intf_no, lane, pre_tap2, main_tap, pre_tap1, post_tap1, data);
	nthw_phy_tile_write_xcvr(p, intf_no, lane, tx_eq_addr + 0x8000U * lane, data);
}

void nthw_phy_tile_get_link_summary(nthw_phy_tile_t *p, uint32_t *p_nt_phy_link_state,
	uint32_t *p_ll_nt_phy_link_state, uint32_t *p_lh_local_fault,
	uint32_t *p_lh_remote_fault, uint8_t index)
{
	nthw_register_update(p->mp_reg_link_summary[index]);

	if (p_nt_phy_link_state) {
		*p_nt_phy_link_state =
			nthw_field_get_val32(p->mp_fld_link_summary_nt_phy_link_state[index]);
	}

	if (p_ll_nt_phy_link_state) {
		*p_ll_nt_phy_link_state =
			nthw_field_get_val32(p->mp_fld_link_summary_ll_nt_phy_link_state[index]);
	}

	if (p_lh_local_fault) {
		*p_lh_local_fault =
			nthw_field_get_val32(p->mp_fld_link_summary_lh_received_local_fault[index]);
	}

	if (p_lh_remote_fault) {
		*p_lh_remote_fault =
			nthw_field_get_val32(p->mp_fld_link_summary_lh_remote_fault[index]);
	}
}

bool nthw_phy_tile_read_fec_enabled_by_scratch(nthw_phy_tile_t *p, uint8_t intf_no)
{
	bool fec_enabled = false;

	if (p->mp_fld_scratch_data) {
		fec_enabled =
			!((nthw_field_get_updated(p->mp_fld_scratch_data) >> (intf_no)) & 0x1);
	}

	/*
	 * log(INF,"intf_no: %d FEC state read from inverted port specificscratch register
	 * value %u ", intf_no,fec_enabled);
	 */
	return fec_enabled;
}

static void nthw_phy_tile_write_fec_enabled_by_scratch(nthw_phy_tile_t *p, uint8_t intf_no,
	bool fec_enabled)
{
	const uint8_t mask = intf_no == 0U ? 0xf0U : 0xfU;
	const uint8_t status_other_port =
		(uint8_t)(nthw_field_get_updated(p->mp_fld_scratch_data) & mask);
	const uint8_t disablebit = (uint8_t)(1U << (4 * intf_no));
	const uint8_t val =
		fec_enabled ? status_other_port : (uint8_t)(status_other_port | disablebit);
	/*  NT_LOG(INF, NTHW, "intf_no: %u write ScratchFEC value %u", intf_no, val); */
	nthw_field_set_val_flush32(p->mp_fld_scratch_data, val);
}

bool nthw_phy_tile_get_rx_hi_ber(nthw_phy_tile_t *p, uint8_t intf_no)
{
	return nthw_field_get_updated(p->mp_fld_port_status_rx_hi_ber[intf_no]);
}

bool nthw_phy_tile_get_rx_am_lock(nthw_phy_tile_t *p, uint8_t intf_no)
{
	return nthw_field_get_updated(p->mp_fld_port_status_rx_am_lock[intf_no]);
}

void nthw_phy_tile_set_timestamp_comp_rx(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t value)
{
	nthw_field_get_updated(p->mp_fld_port_comp_rx_compensation[intf_no]);
	nthw_field_set_val_flush32(p->mp_fld_port_comp_rx_compensation[intf_no], value);
}

uint32_t nthw_phy_tile_get_timestamp_comp_rx(nthw_phy_tile_t *p, uint8_t intf_no)
{
	nthw_field_get_updated(p->mp_fld_port_comp_rx_compensation[intf_no]);
	return nthw_field_get_val32(p->mp_fld_port_comp_rx_compensation[intf_no]);
}

static uint32_t nthw_phy_tile_read_eth(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t address)
{
	nthw_register_update(p->mp_reg_port_eth_base[intf_no]);
	nthw_field_set_val32(p->mp_fld_port_eth_base_cmd[intf_no], 0);
	nthw_field_set_val_flush32(p->mp_fld_port_eth_base_ptr[intf_no], address);

	while (nthw_field_get_updated(p->mp_fld_port_eth_base_busy[intf_no]) == 1)
		nt_os_wait_usec(100);

	return nthw_field_get_updated(p->mp_fld_port_eth_data_data[intf_no]);
}

static void nthw_phy_tile_write_eth(nthw_phy_tile_t *p, uint8_t intf_no,
	uint32_t address, uint32_t data)
{
	nthw_field_set_val_flush32(p->mp_fld_port_eth_data_data[intf_no], data);
	nthw_register_update(p->mp_reg_port_eth_base[intf_no]);
	nthw_field_set_val32(p->mp_fld_port_eth_base_ptr[intf_no], address);
	nthw_field_set_val_flush32(p->mp_fld_port_eth_base_cmd[intf_no], 1);

	while (nthw_field_get_updated(p->mp_fld_port_eth_base_busy[intf_no]) == 1)
		/* NT_LOG(INF, NTHW, "busy"); */
		nt_os_wait_usec(100);
}

bool nthw_phy_tile_configure_fec(nthw_phy_tile_t *p, uint8_t intf_no, bool enable)
{
	/*
	 * FPGA must have the Dynamic Reconfig register for FEC configuration to work.
	 * Previous versions of the FPGA enable FEC statically.
	 */
	if (!p->mp_reg_dyn_reconfig_base)
		return false;

	/*
	 * NT_LOG(INF, NTHW, "void nthw_phy_tile_configure_fec(intf_no %u, enable %u)",
	 * intf_no, enable);
	 */

	/* Available DR profiles related to FEC */
	/* Used as end symbol in profile sequence fed to NIOS processor - not really a profile. */
	const uint32_t neutral_dr_profile = 0;

	/* DR Fec profiles port 0*/
	const uint32_t intf_no0_with_fec_dr_profile = 1;
	const uint32_t intf_no0_no_fec_dr_profile = 2;

	/* DR Fec profiles port 1 */
	const uint32_t intf_no1_with_fec_dr_profile = 3;
	const uint32_t intf_no1_no_fec_dr_profile = 4;

	const uint32_t fec_profile_none = 0x00;
	const uint32_t fec_profile_cl91 = 0x02;

	uint32_t original_dr_profile_id = 0x00;
	uint32_t destination_dr_profile_id = 0x00;
	uint32_t final_fec_profile = 0x00;

	if (intf_no == 0) {
		original_dr_profile_id =
			enable ? intf_no0_no_fec_dr_profile : intf_no0_with_fec_dr_profile;
		destination_dr_profile_id =
			enable ? intf_no0_with_fec_dr_profile : intf_no0_no_fec_dr_profile;

	} else if (intf_no == 1) {
		original_dr_profile_id =
			enable ? intf_no1_no_fec_dr_profile : intf_no1_with_fec_dr_profile;
		destination_dr_profile_id =
			enable ? intf_no1_with_fec_dr_profile : intf_no1_no_fec_dr_profile;
	}

	final_fec_profile = enable ? fec_profile_cl91 : fec_profile_none;

	/*
	 * These steps are copied from a code example provided by Intel: ftile_eth_dr_test.tcl
	 * Step 6: Program DUT soft CSR
	 * The CSR registers are reset by the FPGA reset sequence so
	 * they need to be set every time the driver is restarted.
	 * This is why we perform step 6 first before step 1.
	 */
	uint32_t current_fec_profile = nthw_phy_tile_read_eth(p, intf_no, eth_soft_csr2);

	if (current_fec_profile != final_fec_profile) {
		nthw_phy_tile_set_reset(p, intf_no, true);
		nthw_phy_tile_set_tx_reset(p, intf_no, true);
		NT_LOG(DBG, NTHW, "intf_no %u: Program DUT soft CSR", intf_no);
		/* Select profile */
		nthw_phy_tile_write_eth(p, intf_no, eth_soft_csr1, 0x00);

		/* Select FEC-Mode */
		nthw_phy_tile_write_eth(p, intf_no, eth_soft_csr2,
			(0 << 9) + (0 << 6) + (0 << 3) + (final_fec_profile << 0));

		nt_os_wait_usec(10000);
		nthw_phy_tile_set_reset(p, intf_no, false);
		nthw_phy_tile_set_tx_reset(p, intf_no, false);

		NT_LOG(DBG, NTHW, "intf_no %u: Register eth_soft_csr1 profile_sel: 0x%2.2x",
			intf_no, nthw_phy_tile_read_eth(p, intf_no, eth_soft_csr1));
		NT_LOG(DBG, NTHW, "intf_no %u: Register eth_soft_csr2 fec_mode:    0x%2.2x",
			intf_no, nthw_phy_tile_read_eth(p, intf_no, eth_soft_csr2));
		NT_LOG(DBG, NTHW, "intf_no %u: Register eth_soft_csr3 sel:         0x%2.2x",
			intf_no, nthw_phy_tile_read_eth(p, intf_no, eth_soft_csr3));
	}

	/*
	 * The FEC profile applied with Dynamic Reconfiguration is persistent through
	 * a driver restart. Additionally, the operation is not idempotent, meaning that
	 * the operations must only be performed when actual reconfiguration is necessary.
	 * We use a persistent scratch register provided by the FPGA to store the FEC
	 * state since we have not been able to get any of the methods suggested
	 * by Intel to function properly.
	 */
	if (nthw_phy_tile_read_fec_enabled_by_scratch(p, intf_no) == enable)
		return true;

	/* Step 1: Wait for DR NIOS to be ready */
	NT_LOG(DBG, NTHW, "intf_no %u: Step 1 Wait for DR NIOS", intf_no);

	while ((nthw_phy_tile_read_dyn_reconfig(p, dyn_rcfg_dr_trigger_reg) & 0x02) != 0x02)
		nt_os_wait_usec(10000);

	/* Step 2: Triggering Reconfiguration */
	NT_LOG(DBG, NTHW, "intf_no %u: Step 2: Triggering Reconfiguration", intf_no);

	nthw_phy_tile_set_reset(p, intf_no, true);
	nthw_phy_tile_set_rx_reset(p, intf_no, true);
	nthw_phy_tile_set_tx_reset(p, intf_no, true);
	nt_os_wait_usec(10000);

	/* Disable original profile */
	nthw_phy_tile_write_dyn_reconfig(p, dyn_rcfg_dr_next_profile_0_reg,
		(1U << 18) + (0U << 15) + original_dr_profile_id);
	nt_os_wait_usec(10000);
	NT_LOG(DBG, NTHW, "intf_no %u: dyn_rcfg_dr_next_profile_0_reg: %#010x", intf_no,
		nthw_phy_tile_read_dyn_reconfig(p, dyn_rcfg_dr_next_profile_0_reg));

	nthw_phy_tile_write_dyn_reconfig(p, dyn_rcfg_dr_next_profile_1_reg,
		(1U << 15) + destination_dr_profile_id + (0U << 31) +
		(neutral_dr_profile << 16));
	/*
	 * Enable profile 2 and terminate dyn reconfig by
	 * setting next profile to 0 and deactivate it
	 */
	nt_os_wait_usec(10000);
	NT_LOG(DBG, NTHW, "intf_no %u: dyn_rcfg_dr_next_profile_1_reg: %#010x", intf_no,
		nthw_phy_tile_read_dyn_reconfig(p, dyn_rcfg_dr_next_profile_1_reg));

	/*
	 * We do not know if this neutral profile is necessary.
	 * It is done in the example design and INTEL has not
	 * answered our question if it is necessary or not.
	 */
	nthw_phy_tile_write_dyn_reconfig(p, dyn_rcfg_dr_next_profile_2_reg,
		(0U << 15) + neutral_dr_profile + (0U << 31) +
		(neutral_dr_profile << 16));
	nt_os_wait_usec(10000);
	NT_LOG(DBG, NTHW, "intf_no %u: dyn_rcfg_dr_next_profile_2_reg: %#010x", intf_no,
		nthw_phy_tile_read_dyn_reconfig(p, dyn_rcfg_dr_next_profile_2_reg));

	nt_os_wait_usec(10000);

	/* Step 3: Trigger DR interrupt */
	NT_LOG(DBG, NTHW, "intf_no %u: Step 3: Trigger DR interrupt", intf_no);
	nthw_phy_tile_write_dyn_reconfig(p, dyn_rcfg_dr_trigger_reg, 0x00000001);

	nt_os_wait_usec(1000000);

	/* Step 4: Wait for interrupt Acknowledge */
	NT_LOG(DBG, NTHW, "intf_no %u: Step 4: Wait for interrupt Acknowledge", intf_no);

	while ((nthw_phy_tile_read_dyn_reconfig(p, dyn_rcfg_dr_trigger_reg) & 0x01) != 0x00)
		nt_os_wait_usec(10000);

	nt_os_wait_usec(10000);

	/* Step 5: Wait Until DR config is done */
	NT_LOG(DBG, NTHW, "intf_no %u: Step 5: Wait Until DR config is done", intf_no);

	while ((nthw_phy_tile_read_dyn_reconfig(p, dyn_rcfg_dr_trigger_reg) & 0x02) != 0x02)
		nt_os_wait_usec(10000);

	nt_os_wait_usec(1000000);

	/* Write Fec status to scratch register */
	nthw_phy_tile_write_fec_enabled_by_scratch(p, intf_no, enable);

	nt_os_wait_usec(1000000);

	nthw_phy_tile_set_reset(p, intf_no, false);
	nthw_phy_tile_set_tx_reset(p, intf_no, false);
	nthw_phy_tile_set_rx_reset(p, intf_no, false);

	nthw_register_update(p->mp_reg_dr_cfg_status);
	NT_LOG(DBG, NTHW, "intf_no %u: DrCfgStatusCurrPofileId: %u", intf_no,
		nthw_field_get_val32(p->mp_fld_dr_cfg_status_curr_profile_id));
	NT_LOG(DBG, NTHW, "intf_no %u: DrCfgStatusInProgress:  %u", intf_no,
		nthw_field_get_val32(p->mp_fld_dr_cfg_status_in_progress));
	NT_LOG(DBG, NTHW, "intf_no %u: DrCfgStatusError: %u", intf_no,
		nthw_field_get_val32(p->mp_fld_dr_cfg_status_error));

	return true;
}

uint32_t nthw_phy_tile_get_port_status_reset_ack(nthw_phy_tile_t *p, uint8_t intf_no)
{
	if (p->mp_fld_port_status_reset_ackn[intf_no]) {
		return nthw_field_get_updated(p->mp_fld_port_status_reset_ackn[intf_no]) == 0
			? 1
			: 0;    /* Inverted */
	}

	return 1;
}

uint32_t nthw_phy_tile_get_port_status_tx_lanes_stable(nthw_phy_tile_t *p, uint8_t intf_no)
{
	if (p->mp_fld_port_status_tx_lanes_stable[intf_no])
		return nthw_field_get_updated(p->mp_fld_port_status_tx_lanes_stable[intf_no]);

	return 1;
}

void nthw_phy_tile_set_sys_pll_set_rdy(nthw_phy_tile_t *p, uint32_t value)
{
	if (p->mp_fld_sys_pll_set_rdy) {
		nthw_field_get_updated(p->mp_fld_sys_pll_set_rdy);
		nthw_field_set_val_flush32(p->mp_fld_sys_pll_set_rdy, value);
	}
}

uint32_t nthw_phy_tile_get_sys_pll_get_rdy(nthw_phy_tile_t *p)
{
	if (p->mp_fld_sys_pll_get_rdy)
		return nthw_field_get_updated(p->mp_fld_sys_pll_get_rdy);

	return 0;
}

uint32_t nthw_phy_tile_get_sys_pll_system_pll_lock(nthw_phy_tile_t *p)
{
	if (p->mp_fld_sys_pll_system_pll_lock)
		return nthw_field_get_updated(p->mp_fld_sys_pll_system_pll_lock);

	return 0;
}

void nthw_phy_tile_set_sys_pll_en_ref_clk_fgt(nthw_phy_tile_t *p, uint32_t value)
{
	if (p->mp_fld_sys_pll_en_ref_clk_fgt) {
		nthw_field_get_updated(p->mp_fld_sys_pll_en_ref_clk_fgt);
		nthw_field_set_val_flush32(p->mp_fld_sys_pll_en_ref_clk_fgt, value);
	}
}

uint32_t nthw_phy_tile_get_sys_pll_ref_clk_fgt_enabled(nthw_phy_tile_t *p)
{
	if (p->mp_fld_sys_pll_ref_clk_fgt_enabled)
		return nthw_field_get_updated(p->mp_fld_sys_pll_ref_clk_fgt_enabled);

	return 0;
}

void nthw_phy_tile_set_sys_pll_forward_rst(nthw_phy_tile_t *p, uint32_t value)
{
	if (p->mp_fld_sys_pll_forward_rst) {
		nthw_field_get_updated(p->mp_fld_sys_pll_forward_rst);
		nthw_field_set_val_flush32(p->mp_fld_sys_pll_forward_rst, value);
	}
}

void nthw_phy_tile_set_sys_pll_force_rst(nthw_phy_tile_t *p, uint32_t value)
{
	if (p->mp_fld_sys_pll_force_rst) {
		nthw_field_get_updated(p->mp_fld_sys_pll_force_rst);
		nthw_field_set_val_flush32(p->mp_fld_sys_pll_force_rst, value);
	}
}

void nthw_phy_tile_set_port_config_rst(nthw_phy_tile_t *p, uint8_t intf_no, uint32_t value)
{
	if (p->mp_fld_port_config_reset[intf_no]) {
		nthw_field_get_updated(p->mp_fld_port_config_reset[intf_no]);
		nthw_field_set_val_flush32(p->mp_fld_port_config_reset[intf_no], value);
	}
}
