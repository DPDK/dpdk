/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"
#include "i2c_nim.h"
#include "nt4ga_adapter.h"

#include <string.h>
#include "ntnic_mod_reg.h"
#include "nim_defines.h"
#include "nthw_gfg.h"
#include "nthw_phy_tile.h"
#include "nt_service.h"

typedef enum {
	LOOPBACK_HOST_NONE,
	LOOPBACK_HOST
} loopback_host_t;

typedef enum {
	LOOPBACK_LINE_NONE,
	LOOPBACK_LINE
} loopback_line_t;

static int nt4ga_agx_link_100g_ports_init(struct adapter_info_s *p_adapter_info,
	nthw_fpga_t *fpga);

/*
 * Init AGX 100G link ops variables
 */
static struct link_ops_s link_agx_100g_ops = {
	.link_init = nt4ga_agx_link_100g_ports_init,
};

void link_agx_100g_init(void)
{
	register_agx_100g_link_ops(&link_agx_100g_ops);
}

/*
 * Phy handling
 */

static void phy_get_link_state(adapter_info_t *drv,
	int port,
	nt_link_state_p p_curr_link_state,
	nt_link_state_p p_latched_links_state,
	uint32_t *p_local_fault,
	uint32_t *p_remote_fault)
{
	uint32_t status = 0;
	uint32_t status_latch = 0;

	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;

	nthw_phy_tile_get_link_summary(p,
		&status,
		&status_latch,
		p_local_fault,
		p_remote_fault,
		port);

	if (status == 0x0)
		*p_curr_link_state = NT_LINK_STATE_DOWN;

	else
		*p_curr_link_state = NT_LINK_STATE_UP;

	if (status_latch == 0x0)
		*p_latched_links_state = NT_LINK_STATE_DOWN;

	else
		*p_latched_links_state = NT_LINK_STATE_UP;
}

static void phy_rx_path_rst(adapter_info_t *drv, int port, bool reset)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	NT_LOG(DBG, NTNIC, "Port %d: %s", port, reset ? "RTE_ASSERT" : "deassert");
	nthw_phy_tile_set_rx_reset(p, port, reset);
}

static void phy_tx_path_rst(adapter_info_t *drv, int port, bool reset)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	NT_LOG(DBG, NTNIC, "Port %d: %s", port, reset ? "RTE_ASSERT" : "deassert");
	nthw_phy_tile_set_tx_reset(p, port, reset);
}

static void phy_reset_rx(adapter_info_t *drv, int port)
{
	phy_rx_path_rst(drv, port, true);
	nthw_os_wait_usec(10000);	/* 10ms */
	phy_rx_path_rst(drv, port, false);
	nthw_os_wait_usec(10000);	/* 10ms */
}

static void phy_reset_tx(adapter_info_t *drv, int port)
{
	phy_tx_path_rst(drv, port, true);
	nthw_os_wait_usec(10000);	/* 10ms */
	phy_tx_path_rst(drv, port, false);
	nthw_os_wait_usec(10000);	/* 10ms */
}

static int phy_set_host_loopback(adapter_info_t *drv, int port, loopback_host_t loopback)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;

	switch (loopback) {
	case LOOPBACK_HOST_NONE:
		for (uint8_t lane = 0; lane < 4; lane++)
			nthw_phy_tile_set_host_loopback(p, port, lane, false);

		break;

	case LOOPBACK_HOST:
		for (uint8_t lane = 0; lane < 4; lane++)
			nthw_phy_tile_set_host_loopback(p, port, lane, true);

		break;

	default:
		NT_LOG(ERR, NTNIC, "Port %d: Unhandled loopback value (%d)", port, loopback);
		return -1;
	}

	return 0;
}

static int phy_set_line_loopback(adapter_info_t *drv, int port, loopback_line_t loopback)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	uint32_t addr;

	switch (loopback) {
	case LOOPBACK_LINE_NONE:
		for (uint8_t lane = 0; lane < 4; lane++) {
			uint8_t quad_lane;

			for (uint8_t fgt = 0; fgt < 4; fgt++) {
				uint8_t quad_and_lane_no =
					(uint8_t)nthw_phy_tile_read_xcvr(p, port, lane, 0xFFFFC);
				quad_lane = quad_and_lane_no & 0x3;	/* bits[1:0] */

				if (quad_lane == lane)
					break;
			}

			nthw_phy_tile_write_xcvr(p, port, 0, 0x41830u + (0x8000u * quad_lane),
				0x03);

			addr = nthw_phy_tile_read_xcvr(p, 0, 0, 0x41768u + (0x8000u * quad_lane)) |
				(1u << 24u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41768u + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, 0, 0, 0x41414u + (0x8000u * quad_lane)) &
				~(1u << 29u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41414u + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, 0, 0, 0x4141Cu + (0x8000u * quad_lane)) &
				~(1u << 30u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x4141Cu + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, 0, 0, 0x41418u + (0x8000u * quad_lane)) &
				~(1u << 31u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41418u + (0x8000u * quad_lane),
				addr);
		}

		break;

	case LOOPBACK_LINE:
		for (uint8_t lane = 0; lane < 4; lane++) {
			uint8_t quad_lane;

			for (uint8_t fgt = 0; fgt < 4; fgt++) {
				uint8_t quad_and_lane_no =
					(uint8_t)nthw_phy_tile_read_xcvr(p, port, lane, 0xFFFFC);
				quad_lane = quad_and_lane_no & 0x3;	/* bits[1:0] */

				if (quad_lane == lane)
					break;
			}

			addr = nthw_phy_tile_read_xcvr(p, port, 0,
					0x41830u + (0x8000u * quad_lane)) &
				0u;
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41830u + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, port, 0,
					0x41768u + (0x8000u * quad_lane)) &
				~(1u << 24u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41768u + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, port, 0,
					0x41414u + (0x8000u * quad_lane)) |
				(1u << 29u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41414u + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, port, 0,
					0x4141Cu + (0x8000u * quad_lane)) |
				(1u << 30u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x4141Cu + (0x8000u * quad_lane),
				addr);

			addr = nthw_phy_tile_read_xcvr(p, port, 0,
					0x41418u + (0x8000u * quad_lane)) |
				(1u << 31u);
			nthw_phy_tile_write_xcvr(p, port, 0, 0x41418u + (0x8000u * quad_lane),
				addr);
		}

		break;

	default:
		NT_LOG(ERR, NTNIC, "Port %d: Unhandled loopback value (%d)", port, loopback);
		return -1;
	}

	return 0;
}

/*
 * Nim handling
 */

static void nim_set_reset(struct nim_i2c_ctx *ctx, uint8_t nim_idx, bool reset)
{
	nthw_pcal6416a_t *p = ctx->hwagx.p_io_nim;

	if (nim_idx == 0)
		nthw_pcal6416a_write(p, 0, reset ? 0 : 1);

	else if (nim_idx == 1)
		nthw_pcal6416a_write(p, 4, reset ? 0 : 1);
}

static bool nim_is_present(nim_i2c_ctx_p ctx, uint8_t nim_idx)
{
	RTE_ASSERT(nim_idx < NUM_ADAPTER_PORTS_MAX);

	nthw_pcal6416a_t *p = ctx->hwagx.p_io_nim;
	uint8_t data = 0;

	if (nim_idx == 0)
		nthw_pcal6416a_read(p, 3, &data);

	else if (nim_idx == 1)
		nthw_pcal6416a_read(p, 7, &data);

	return data == 0;
}

static void set_nim_low_power(nim_i2c_ctx_p ctx, uint8_t nim_idx, bool low_power)
{
	nthw_pcal6416a_t *p = ctx->hwagx.p_io_nim;

	if (nim_idx == 0) {
		/* De-asserting LP mode pin 1 */
		nthw_pcal6416a_write(p, 1, low_power ? 1 : 0);

	} else if (nim_idx == 1) {
		/* De-asserting LP mode pin 5 */
		nthw_pcal6416a_write(p, 5, low_power ? 1 : 0);
	}
}

/*
 * Link handling
 */

static void adjust_maturing_delay(adapter_info_t *drv, int port)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	nthw_rpf_t *p_rpf = drv->fpga_info.mp_nthw_agx.p_rpf;
	/*
	 * Find the maximum of the absolute values of the RX componensation for all
	 * ports (the RX componensation may not be set for some of the other ports).
	 */
	const int16_t unset_rx_compensation = -1;	/* 0xffff */
	int16_t max_comp = 0;

	for (int i = 0; i < drv->fpga_info.n_phy_ports; i++) {
		int16_t comp = (int16_t)nthw_phy_tile_get_timestamp_comp_rx(p, i);

		if (comp != unset_rx_compensation && abs(comp) > abs(max_comp))
			max_comp = comp;
	}

	int delay = nthw_rpf_get_maturing_delay(p_rpf) - max_comp;

	/*
	 * For SOF time-stamping account for jumbo frame.
	 * Frame size = 80000 b. divided by Gb link speed = processing time in ns.
	 */
	if (!nthw_rpf_get_ts_at_eof(p_rpf)) {
		uint32_t jumbo_frame_processing_time_ns = 80000U / 100U;
		delay -= (int)jumbo_frame_processing_time_ns;
	}

	const unsigned int delay_bit_width = 19;/* 19 bits maturing delay */
	const int min_delay = -(1 << (delay_bit_width - 1));
	const int max_delay = 0;

	if (delay >= min_delay && delay <= max_delay) {
		nthw_rpf_set_maturing_delay(p_rpf, delay);

	} else {
		NT_LOG(WRN, NTNIC,
			"Port %i: Cannot set the RPF adjusted maturing delay to %i because "
			"that value is outside the legal range [%i:%i]",
			port, delay, min_delay, max_delay);
	}
}

static void set_link_state(adapter_info_t *drv, nim_i2c_ctx_p ctx, link_state_t *state, int port)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	/*
	 * 100G: 4 LEDs per port
	 */

	bool led_on = state->nim_present && state->link_state == NT_LINK_STATE_UP;
	uint8_t led_pos = (uint8_t)(port * ctx->lane_count);

	for (uint8_t i = 0; i < ctx->lane_count; i++) {
		nthw_pca9532_set_led_on(drv->fpga_info.mp_nthw_agx.p_pca9532_led, led_pos + 1,
			led_on);
	}

	nthw_phy_tile_set_timestamp_comp_rx(p, port, 0);

	if (ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena) {
		/* Updated after mail from JKH 2023-02-07 */
		nthw_phy_tile_set_timestamp_comp_rx(p, port, 0x9E);
		/* TODO Hermosa Awaiting comp values to use */

	} else {
		/* TODO Hermosa Awaiting comp values to use */
		nthw_phy_tile_set_timestamp_comp_rx(p, port, 0);
	}

	adjust_maturing_delay(drv, port);	/* MUST be called after timestampCompRx */
}

static void get_link_state(adapter_info_t *drv, nim_i2c_ctx_p ctx, link_state_t *state, int port)
{
	uint32_t local_fault;
	uint32_t remote_fault;
	nt_link_state_t curr_link_state;

	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;

	/* Save the current state before reading the new */
	curr_link_state = state->link_state;

	phy_get_link_state(drv, port, &state->link_state, &state->link_state_latched, &local_fault,
		&remote_fault);

	if (curr_link_state != state->link_state)
		NT_LOG(DBG, NTNIC, "Port %i: Faults(Local = %" PRIu32 ", Remote = %" PRIu32 ")",
			port, local_fault, remote_fault);

	state->nim_present = nim_is_present(ctx, port);

	if (!state->nim_present)
		return;	/* No nim so no need to do anything */

	state->link_up = state->link_state == NT_LINK_STATE_UP ? true : false;

	if (state->link_state == NT_LINK_STATE_UP)
		return;	/* The link is up so no need to do anything else */

	if (remote_fault == 0) {
		phy_reset_rx(drv, port);
		NT_LOG(DBG, NTNIC, "Port %i: resetRx due to local fault.", port);
		return;
	}

	/* In case of too many errors perform a reset */
	if (nthw_phy_tile_get_rx_hi_ber(p, port)) {
		NT_LOG(INF, NTNIC, "Port %i: HiBer", port);
		phy_reset_rx(drv, port);
		return;
	}

	/* If FEC is not enabled then no reason to look at FEC state */
	if (!ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena ||
		nthw_phy_tile_read_fec_enabled_by_scratch(p, port)) {
		return;
	}

	if (!nthw_phy_tile_get_rx_am_lock(p, port))
		phy_reset_rx(drv, port);
}

/*
 * Utility functions
 */

static int swap_tx_rx_polarity(adapter_info_t *drv, int port, bool swap)
{
	/*
	 * Mapping according to schematics
	 * I: Inversion, N: Non-inversion
	 *
	 *  Port 0: PMA0/FGT0
	 *  FPGA QSFP Tx Rx
	 *  ---------------
	 *  Q3C0 Ch4  I  I
	 *  Q3C1 Ch2  I  I
	 *  Q3C2 Ch1  I  N
	 *  Q3C3 Ch3  I  I
	 *
	 *  Port 1: PMA1/FGT1
	 *  FPGA QSFP Tx Rx
	 *  ---------------
	 *  Q2C0 Ch4  I  I
	 *  Q2C1 Ch2  I  I
	 *  Q2C2 Ch1  N  I
	 *  Q2C3 Ch3  N  N
	 */

	bool rx_polarity_swap[2][4] = { { false, true, true, true }, { true, false, true, true } };
	bool tx_polarity_swap[2][4] = { { false, false, true, true }, { true, true, true, true } };

	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;

	if (p) {
		for (uint8_t lane = 0; lane < 4; lane++) {
			if (swap) {
				nthw_phy_tile_set_tx_pol_inv(p, port, lane,
					tx_polarity_swap[port][lane]);
				nthw_phy_tile_set_rx_pol_inv(p, port, lane,
					rx_polarity_swap[port][lane]);

			} else {
				nthw_phy_tile_set_tx_pol_inv(p, port, lane, false);
				nthw_phy_tile_set_rx_pol_inv(p, port, lane, false);
			}
		}

		return 0;
	}

	return -1;
}

static void
set_loopback(struct adapter_info_s *p_adapter_info, int port, uint32_t mode, uint32_t last_mode)
{
	switch (mode) {
	case 1:
		NT_LOG(INF, NTNIC, "%s: Applying host loopback",
			p_adapter_info->mp_port_id_str[port]);
		phy_set_host_loopback(p_adapter_info, port, LOOPBACK_HOST);
		swap_tx_rx_polarity(p_adapter_info, port, false);
		break;

	case 2:
		NT_LOG(INF, NTNIC, "%s: Applying line loopback",
			p_adapter_info->mp_port_id_str[port]);
		phy_set_line_loopback(p_adapter_info, port, LOOPBACK_LINE);
		break;

	default:
		switch (last_mode) {
		case 1:
			NT_LOG(INF, NTNIC, "%s: Removing host loopback",
				p_adapter_info->mp_port_id_str[port]);
			phy_set_host_loopback(p_adapter_info, port, LOOPBACK_HOST_NONE);
			swap_tx_rx_polarity(p_adapter_info, port, true);
			break;

		case 2:
			NT_LOG(INF, NTNIC, "%s: Removing line loopback",
				p_adapter_info->mp_port_id_str[port]);
			phy_set_line_loopback(p_adapter_info, port, LOOPBACK_LINE_NONE);
			break;

		default:
			/* do nothing */
			break;
		}

		break;
	}

	/* After changing the loopback the system must be properly reset */
	phy_reset_rx(p_adapter_info, port);
	phy_reset_tx(p_adapter_info, port);
	nthw_os_wait_usec(10000);	/* 10ms - arbitrary choice */
}

static void port_disable(adapter_info_t *drv, int port)
{
	nim_i2c_ctx_t *nim_ctx = &drv->nt4ga_link.u.nim_ctx[port];
	phy_reset_rx(drv, port);
	phy_reset_tx(drv, port);
	set_nim_low_power(nim_ctx, port, true);
}

/*
 * Initialize NIM, Code based on nt400d1x.cpp: MyPort::createNim()
 */

static int create_nim(adapter_info_t *drv, int port, bool enable)
{
	int res = 0;
	const uint8_t valid_nim_id = NT_NIM_QSFP28;
	sfp_nim_state_t nim;
	nt4ga_link_t *link_info = &drv->nt4ga_link;

	RTE_ASSERT(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	RTE_ASSERT(link_info->variables_initialized);

	nim_i2c_ctx_t *nim_ctx = &link_info->u.nim_ctx[port];

	if (!enable) {
		phy_reset_rx(drv, port);
		phy_reset_tx(drv, port);
	}

	/*
	 * Check NIM is present before doing reset.
	 */

	if (!nim_is_present(nim_ctx, port)) {
		NT_LOG(DBG, NTNIC, "%s: NIM module is no longer absent!",
			drv->mp_port_id_str[port]);
		return -1;
	}

	/*
	 * Reset NIM
	 */

	NT_LOG(DBG, NTNIC, "%s: Performing NIM reset", drv->mp_port_id_str[port]);
	nim_set_reset(nim_ctx, (uint8_t)port, true);
	nthw_os_wait_usec(100000);/*  pause 0.1s */
	nim_set_reset(nim_ctx, (uint8_t)port, false);

	/*
	 * Wait a little after a module has been inserted before trying to access I2C
	 * data, otherwise the module will not respond correctly.
	 */
	nthw_os_wait_usec(1000000);	/* pause 1.0s */

	res = nthw_construct_and_preinit_nim(nim_ctx, NULL);

	if (res)
		return res;

	res = nthw_nim_state_build(nim_ctx, &nim);

	if (res)
		return res;

	/* Set FEC to be enabled by default */
	nim_ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena = true;

	NT_LOG(DBG, NTHW, "%s: NIM id = %u (%s), br = %u, vendor = '%s', pn = '%s', sn='%s'",
		drv->mp_port_id_str[port], nim_ctx->nim_id, nthw_nim_id_to_text(nim_ctx->nim_id),
		nim.br, nim_ctx->vendor_name, nim_ctx->prod_no, nim_ctx->serial_no);

	/*
	 * Does the driver support the NIM module type?
	 */
	if (nim_ctx->nim_id != valid_nim_id) {
		NT_LOG(ERR, NTHW, "%s: The driver does not support the NIM module type %s",
			drv->mp_port_id_str[port], nthw_nim_id_to_text(nim_ctx->nim_id));
		NT_LOG(DBG, NTHW, "%s: The driver supports the NIM module type %s",
			drv->mp_port_id_str[port], nthw_nim_id_to_text(valid_nim_id));
		return -1;
	}

	if (enable) {
		NT_LOG(DBG, NTNIC, "%s: De-asserting low power", drv->mp_port_id_str[port]);
		set_nim_low_power(nim_ctx, port, false);

	} else {
		NT_LOG(DBG, NTNIC, "%s: Asserting low power", drv->mp_port_id_str[port]);
		set_nim_low_power(nim_ctx, port, true);
	}

	return res;
}

static int nim_ready_100_gb(adapter_info_t *p_info, int port)
{
	nt4ga_link_t *link_info = &p_info->nt4ga_link;
	nim_i2c_ctx_t *nim_ctx = &link_info->u.nim_ctx[port];
	nthw_phy_tile_t *p_phy_tile = p_info->fpga_info.mp_nthw_agx.p_phy_tile;

	bool disable_fec_by_port_type = (nim_ctx->port_type == NT_PORT_TYPE_QSFP28_LR4);
	bool enable_fec = nim_ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena &&
		!disable_fec_by_port_type;

	NT_LOG(DBG, NTNIC, "Port %s: DisableFec = %d", p_info->mp_port_id_str[port], !enable_fec);
	/*
	 * FecAutonegotiation at 100G is not supported for Napatech adapters and therefore
	 * not relevant at this speed
	 */

	/* Adjust NIM power level */
	if (nim_ctx->pwr_level_req > 4) {
		nthw_qsfp28_set_high_power(nim_ctx);
		nim_ctx->pwr_level_cur = nim_ctx->pwr_level_req;
	}

	/* enable_fec is what the end result should be, now find out if it's possible */
	if (enable_fec) {
		if (nthw_qsfp28_set_fec_enable(nim_ctx, true, false)) {
			/* Prefer NIM media FEC since the NIM is assumed to know the right FEC */
			NT_LOG(DBG, NTNIC, "Port %s: NIM media FEC enabled",
				p_info->mp_port_id_str[port]);

			/* The NIM has been set to FEC on the media side so turn FPGA FEC off */
			if (nthw_phy_tile_configure_fec(p_phy_tile, port, true)) {
				NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC disabled",
					p_info->mp_port_id_str[port]);

			} else {
				NT_LOG(DBG, NTNIC,
					"Port %s: FPGA does not support disabling of FEC",
					p_info->mp_port_id_str[port]);
				return 1;
			}

		} else if (nthw_qsfp28_set_fec_enable(nim_ctx, false, false)) {
			/* The NIM does not support FEC at all so turn FPGA FEC on instead */
			/* This is relevant to SR4 modules */
			NT_LOG(DBG, NTNIC, "Port %s: No NIM FEC", p_info->mp_port_id_str[port]);
			nthw_phy_tile_configure_fec(p_phy_tile, port, false);
			NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC enabled",
				p_info->mp_port_id_str[port]);

		} else if (nthw_qsfp28_set_fec_enable(nim_ctx, true, true)) {
			/* This probably not a likely scenario */
			nthw_phy_tile_configure_fec(p_phy_tile, port, false);
			NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC enabled",
				p_info->mp_port_id_str[port]);
			NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC terminated, NIM media FEC enabled",
				p_info->mp_port_id_str[port]);

		} else {
			NT_LOG(ERR, NTNIC, "Port %s: Could not enable FEC",
				p_info->mp_port_id_str[port]);
			return 1;
		}

	} else if (nthw_qsfp28_set_fec_enable(nim_ctx, false, false)) {
		/* The NIM does not support FEC at all - this is relevant to LR4 modules */
		NT_LOG(DBG, NTNIC, "Port %s: No NIM FEC", p_info->mp_port_id_str[port]);

		if (nthw_phy_tile_configure_fec(p_phy_tile, port, true)) {
			NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC disabled",
				p_info->mp_port_id_str[port]);

		} else {
			NT_LOG(ERR, NTNIC, "Port %s: FPGA does not support disabling of FEC",
				p_info->mp_port_id_str[port]);
			return 1;
		}

	} else if (nthw_qsfp28_set_fec_enable(nim_ctx, false, true)) {
		nthw_phy_tile_configure_fec(p_phy_tile, port, false);
		/* This probably not a likely scenario */
		NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC enabled", p_info->mp_port_id_str[port]);
		NT_LOG(DBG, NTNIC, "Port %s: FPGA FEC terminated, NIM media FEC disabled",
			p_info->mp_port_id_str[port]);

	} else {
		NT_LOG(ERR, NTNIC, "Port %s: Could not disable FEC",
			p_info->mp_port_id_str[port]);
		return 1;
	}

	/* setTxEqualization(uint8_t intf_no, uint8_t lane, uint32_t pre_tap2,
	 * uint32_t main_tap, uint32_t pre_tap1, uint32_t post_tap1)
	 */
	nthw_phy_tile_set_tx_equalization(p_phy_tile, port, 0, 0, 44, 2, 9);
	nthw_phy_tile_set_tx_equalization(p_phy_tile, port, 1, 0, 44, 2, 9);
	nthw_phy_tile_set_tx_equalization(p_phy_tile, port, 2, 0, 44, 2, 9);
	nthw_phy_tile_set_tx_equalization(p_phy_tile, port, 3, 0, 44, 2, 9);

	/*
	 * Perform a full reset. If the RX is in reset from the start this sequence will
	 * take it out of reset and this is necessary in order to read block/lane lock
	 * in getLinkState()
	 */
	phy_reset_rx(p_info, port);
	return 0;
}

/*
 * Initialize one 100 Gbps port.
 */
static int _port_init(adapter_info_t *p_info, nthw_fpga_t *fpga, int port)
{
	uint8_t adapter_no = p_info->adapter_no;
	int res;

	nt4ga_link_t *link_info = &p_info->nt4ga_link;
	nthw_gfg_t *p_gfg = &link_info->u.var_a100g.gfg[adapter_no];
	nthw_phy_tile_t *p_phy_tile = p_info->fpga_info.mp_nthw_agx.p_phy_tile;
	nthw_rpf_t *p_rpf = p_info->fpga_info.mp_nthw_agx.p_rpf;

	RTE_ASSERT(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	RTE_ASSERT(link_info->variables_initialized);

	link_info->link_info[port].link_speed = NT_LINK_SPEED_100G;
	link_info->link_info[port].link_duplex = NT_LINK_DUPLEX_FULL;
	link_info->link_info[port].link_auto_neg = NT_LINK_AUTONEG_OFF;
	link_info->speed_capa |= NT_LINK_SPEED_100G;

	nthw_gfg_stop(p_gfg, port);

	for (uint8_t lane = 0; lane < 4; lane++)
		nthw_phy_tile_set_host_loopback(p_phy_tile, port, lane, false);

	swap_tx_rx_polarity(p_info, port, true);
	nthw_rpf_set_ts_at_eof(p_rpf, true);

	NT_LOG(DBG, NTNIC, "%s: Setting up port %d", p_info->mp_port_id_str[port], port);

	phy_reset_rx(p_info, port);

	if (nthw_gmf_init(NULL, fpga, port) == 0) {
		nthw_gmf_t gmf;

		if (nthw_gmf_init(&gmf, fpga, port) == 0)
			nthw_gmf_set_enable_tsi(&gmf, true, 0, 0, false);
	}

	nthw_rpf_unblock(p_rpf);

	res = create_nim(p_info, port, true);

	if (res) {
		NT_LOG(WRN, NTNIC, "%s: NIM initialization failed",
			p_info->mp_port_id_str[port]);
		return res;
	}

	NT_LOG(DBG, NTNIC, "%s: NIM initialized", p_info->mp_port_id_str[port]);

	res = nim_ready_100_gb(p_info, port);

	if (res) {
		NT_LOG(WRN, NTNIC, "%s: NIM failed to get ready", p_info->mp_port_id_str[port]);
		return res;
	}

	phy_reset_rx(p_info, port);
	return res;
}

/*
 * Link state machine
 */
static int _common_ptp_nim_state_machine(void *data)
{
	static adapter_info_t *drv;
	static nt4ga_link_t *link_info;
	static nthw_fpga_t *fpga;
	static int nb_ports;
	static link_state_t *link_state;
	static nim_i2c_ctx_t *nim_ctx;
	static uint32_t last_lpbk_mode[NUM_ADAPTER_PORTS_MAX];

	struct nt_service *adapter_mon_srv = nthw_service_get_info(RTE_NTNIC_SERVICE_ADAPTER_MON);
	RTE_ASSERT(adapter_mon_srv != NULL);

	if (!NT_SERVICE_GET_STATE(adapter_mon_srv)) {
		drv = (adapter_info_t *)data;
		RTE_ASSERT(drv != NULL);

		fpga_info_t *fpga_info = &drv->fpga_info;
		link_info = &drv->nt4ga_link;
		fpga = fpga_info->mp_fpga;
		int adapter_no = drv->adapter_no;

		nb_ports = fpga_info->n_phy_ports;
		link_state = link_info->link_state;
		nim_ctx = link_info->u.var_a100g.nim_ctx;

		if (!fpga) {
			NT_LOG(ERR, NTNIC, "%s: fpga is NULL", drv->mp_adapter_id_str);
			return -1;
		}

		RTE_ASSERT(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);

		memset(last_lpbk_mode, 0, sizeof(last_lpbk_mode));

		/* Initialize link state */
		for (int i = 0; i < nb_ports; i++) {
			link_state[i].link_disabled = true;
			link_state[i].nim_present = false;
			link_state[i].lh_nim_absent = true;
			link_state[i].link_up = false;
			link_state[i].link_state = NT_LINK_STATE_UNKNOWN;
			link_state[i].link_state_latched = NT_LINK_STATE_UNKNOWN;
		}

		NT_LOG(INF, NTNIC, "Adapter monitor service started on lcore %i", rte_lcore_id());
		adapter_mon_srv->lcore = rte_lcore_id();
		NT_SERVICE_SET_STATE(adapter_mon_srv, true);
		return 0;
	}

	int i;
	static bool reported_link[NUM_ADAPTER_PORTS_MAX] = { false };

	for (i = 0; i < nb_ports; i++) {
		const bool is_port_disabled = link_info->port_action[i].port_disable;
		const bool was_port_disabled = link_state[i].link_disabled;
		const bool disable_port = is_port_disabled && !was_port_disabled;
		const bool enable_port = !is_port_disabled && was_port_disabled;

		if (!rte_service_runstate_get(adapter_mon_srv->id))
			break;

		/*
		 * Has the administrative port state changed?
		 */
		RTE_ASSERT(!(disable_port && enable_port));

		if (disable_port) {
			memset(&link_state[i], 0, sizeof(link_state[i]));
			link_state[i].link_disabled = true;
			link_state[i].lh_nim_absent = true;
			reported_link[i] = false;
			port_disable(drv, i);
			NT_LOG(INF, NTNIC, "%s: Port %i is disabled",
				drv->mp_port_id_str[i], i);
			continue;
		}

		if (enable_port) {
			link_state[i].link_disabled = false;
			NT_LOG(DBG, NTNIC, "%s: Port %i is enabled",
				drv->mp_port_id_str[i], i);
		}

		if (is_port_disabled)
			continue;

		if (link_info->port_action[i].port_lpbk_mode != last_lpbk_mode[i]) {
			/* Loopback mode has changed. Do something */
			if (!nim_is_present(&nim_ctx[i], i)) {
				/*
				 * If there is no Nim present, we need to initialize the
				 * port  anyway
				 */
				_port_init(drv, fpga, i);
			}

			set_loopback(drv,
				i,
				link_info->port_action[i].port_lpbk_mode,
				last_lpbk_mode[i]);

			if (link_info->port_action[i].port_lpbk_mode == 1)
				link_state[i].link_up = true;

			last_lpbk_mode[i] = link_info->port_action[i].port_lpbk_mode;
			continue;
		}

		get_link_state(drv, nim_ctx, &link_state[i], i);
		link_state[i].link_disabled = is_port_disabled;

		if (!link_state[i].nim_present) {
			if (!link_state[i].lh_nim_absent) {
				NT_LOG(INF, NTNIC, "%s: NIM module removed",
					drv->mp_port_id_str[i]);
				reported_link[i] = false;
				link_state[i].link_up = false;
				link_state[i].lh_nim_absent = true;

			} else {
				NT_LOG(DBG, NTNIC, "%s: No NIM module, skip",
					drv->mp_port_id_str[i]);
			}

			continue;
		}

		/*
		 * NIM module is present
		 */
		if (link_state[i].lh_nim_absent && link_state[i].nim_present) {
			sfp_nim_state_t new_state;
			NT_LOG(INF, NTNIC, "%s: NIM module inserted",
				drv->mp_port_id_str[i]);

			if (_port_init(drv, fpga, i)) {
				NT_LOG(ERR, NTNIC,
					"%s: Failed to initialize NIM module",
					drv->mp_port_id_str[i]);
				continue;
			}

			if (nthw_nim_state_build(&nim_ctx[i], &new_state)) {
				NT_LOG(ERR, NTNIC, "%s: Cannot read basic NIM data",
					drv->mp_port_id_str[i]);
				continue;
			}

			RTE_ASSERT(new_state.br); /* Cannot be zero if NIM is present */
			NT_LOG(DBG, NTNIC,
				"%s: NIM id = %u (%s), br = %u, vendor = '%s', pn = '%s', sn='%s'",
				drv->mp_port_id_str[i], nim_ctx[i].nim_id,
				nthw_nim_id_to_text(nim_ctx[i].nim_id),
				(unsigned int)new_state.br, nim_ctx[i].vendor_name,
				nim_ctx[i].prod_no, nim_ctx[i].serial_no);
			link_state[i].lh_nim_absent = false;
			NT_LOG(DBG, NTNIC, "%s: NIM module initialized",
				drv->mp_port_id_str[i]);
			continue;
		}

		if (reported_link[i] != link_state[i].link_up) {
			NT_LOG(INF, NTNIC, "%s: link is %s", drv->mp_port_id_str[i],
				(link_state[i].link_up ? "up" : "down"));
			reported_link[i] = link_state[i].link_up;
			set_link_state(drv, nim_ctx, &link_state[i], i);
		}
	}

	if (rte_service_runstate_get(adapter_mon_srv->id))
		nthw_os_wait_usec(5 * 100000U);	/*  5 x 0.1s = 0.5s */

	return 0;
}

static int nt4ga_agx_link_100g_mon(void *data)
{
	return _common_ptp_nim_state_machine(data);
}

/*
 * Initialize all ports
 * The driver calls this function during initialization (of the driver).
 */
int nt4ga_agx_link_100g_ports_init(struct adapter_info_s *p_adapter_info, nthw_fpga_t *fpga)
{
	fpga_info_t *fpga_info = &p_adapter_info->fpga_info;
	nt4ga_link_t *nt4ga_link = &p_adapter_info->nt4ga_link;
	const int adapter_no = p_adapter_info->adapter_no;
	const int nb_ports = fpga_info->n_phy_ports;
	int res = 0;
	int i;

	NT_LOG(DBG, NTNIC, "%s: Initializing ports", p_adapter_info->mp_adapter_id_str);

	if (!nt4ga_link->variables_initialized) {
		nthw_gfg_t *gfg_mod = p_adapter_info->nt4ga_link.u.var_a100g.gfg;
		nim_i2c_ctx_t *nim_ctx = p_adapter_info->nt4ga_link.u.var_a100g.nim_ctx;
		nthw_agx_t *p_nthw_agx = &p_adapter_info->fpga_info.mp_nthw_agx;

		p_nthw_agx->p_rpf = nthw_rpf_new();
		res = nthw_rpf_init(p_nthw_agx->p_rpf, fpga, adapter_no);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "%s: Failed to initialize RPF module (%i)",
				p_adapter_info->mp_adapter_id_str, res);
			return res;
		}

		res = nthw_gfg_init(&gfg_mod[adapter_no], fpga, 0 /* Only one instance */);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "%s: Failed to initialize GFG module (%i)",
				p_adapter_info->mp_adapter_id_str, res);
			return res;
		}

		for (i = 0; i < nb_ports; i++) {
			/* 2 + adapter port number */
			const uint8_t instance = (uint8_t)(2U + i);
			nim_agx_setup(&nim_ctx[i], p_nthw_agx->p_io_nim, p_nthw_agx->p_i2cm,
				p_nthw_agx->p_pca9849);
			nim_ctx[i].hwagx.mux_channel = i;
			nim_ctx[i].instance = instance;	/* not used */
			nim_ctx[i].devaddr = 0;	/* not used */
			nim_ctx[i].regaddr = 0;	/* not used */
			nim_ctx[i].type = I2C_HWAGX;

			nthw_gfg_stop(&gfg_mod[adapter_no], i);

			for (uint8_t lane = 0; lane < 4; lane++) {
				nthw_phy_tile_set_host_loopback(p_nthw_agx->p_phy_tile, i, lane,
					false);
			}

			swap_tx_rx_polarity(p_adapter_info, i, true);
		}

		nthw_rpf_set_ts_at_eof(p_nthw_agx->p_rpf, true);


		p_adapter_info->nt4ga_link.speed_capa = NT_LINK_SPEED_100G;
		p_adapter_info->nt4ga_link.variables_initialized = true;
	}

	/*
	 * Create state-machine service
	 */

	if (res == 0) {
		struct rte_service_spec adapter_monitor_service = {
						.name = "ntnic-adapter_agx-monitor",
						.callback = nt4ga_agx_link_100g_mon,
						.socket_id = SOCKET_ID_ANY,
						.capabilities = RTE_SERVICE_CAP_MT_SAFE,
						.callback_userdata = p_adapter_info,
		};

		res = nthw_service_add(&adapter_monitor_service, RTE_NTNIC_SERVICE_ADAPTER_MON);
		if (res) {
			NT_LOG(ERR, NTNIC, "%s: Failed to create adapter monitor service",
				p_adapter_info->mp_adapter_id_str);
			return res;
		}
	}

	return res;
}
