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
/* CPI option flags */
static const uint32_t cpi_set = 0x2000;	/* 1 << 13 */
static const uint32_t cpi_in_reset = 0x4000;	/* 1 << 14 */
static const uint32_t bit_cpi_assert = 0x8000;	/* 1 << 15 */
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

	xcvr_instance = lane;
	lane_offset = (uint32_t)(lane << 20);

	cpi_cmd = (data << 16) | cpi_assert | cpi_set_get | (quad_lane << 8) | op_code;

	nthw_phy_tile_write_xcvr(p, intf_no, lane, link_addr + lane_offset, cpi_cmd);

	nt_os_wait_usec(10000);

	for (int i = 20; i > 0; i--) {
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
