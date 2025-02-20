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

static void phy_rx_path_rst(adapter_info_t *drv, int port, bool reset)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	NT_LOG(DBG, NTNIC, "Port %d: %s", port, reset ? "assert" : "deassert");
	nthw_phy_tile_set_rx_reset(p, port, reset);
}

static void phy_tx_path_rst(adapter_info_t *drv, int port, bool reset)
{
	nthw_phy_tile_t *p = drv->fpga_info.mp_nthw_agx.p_phy_tile;
	NT_LOG(DBG, NTNIC, "Port %d: %s", port, reset ? "assert" : "deassert");
	nthw_phy_tile_set_tx_reset(p, port, reset);
}

static void phy_reset_rx(adapter_info_t *drv, int port)
{
	phy_rx_path_rst(drv, port, true);
	nt_os_wait_usec(10000);	/* 10ms */
	phy_rx_path_rst(drv, port, false);
	nt_os_wait_usec(10000);	/* 10ms */
}

static void phy_reset_tx(adapter_info_t *drv, int port)
{
	phy_tx_path_rst(drv, port, true);
	nt_os_wait_usec(10000);	/* 10ms */
	phy_tx_path_rst(drv, port, false);
	nt_os_wait_usec(10000);	/* 10ms */
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
	nt_os_wait_usec(10000);	/* 10ms - arbitrary choice */
}

/*
 * Link state machine
 */
static void *_common_ptp_nim_state_machine(void *data)
{
	adapter_info_t *drv = (adapter_info_t *)data;
	fpga_info_t *fpga_info = &drv->fpga_info;
	nt4ga_link_t *link_info = &drv->nt4ga_link;
	nthw_fpga_t *fpga = fpga_info->mp_fpga;
	const int adapter_no = drv->adapter_no;
	const int nb_ports = fpga_info->n_phy_ports;
	uint32_t last_lpbk_mode[NUM_ADAPTER_PORTS_MAX];
	/* link_state_t new_link_state; */

	link_state_t *link_state = link_info->link_state;

	if (!fpga) {
		NT_LOG(ERR, NTNIC, "%s: fpga is NULL", drv->mp_adapter_id_str);
		goto NT4GA_LINK_100G_MON_EXIT;
	}

	assert(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);

	monitor_task_is_running[adapter_no] = 1;
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

	if (monitor_task_is_running[adapter_no])
		NT_LOG(DBG, NTNIC, "%s: link state machine running...", drv->mp_adapter_id_str);

	while (monitor_task_is_running[adapter_no]) {
		int i;

		for (i = 0; i < nb_ports; i++) {
			const bool is_port_disabled = link_info->port_action[i].port_disable;
			const bool was_port_disabled = link_state[i].link_disabled;
			const bool disable_port = is_port_disabled && !was_port_disabled;
			const bool enable_port = !is_port_disabled && was_port_disabled;

			if (!monitor_task_is_running[adapter_no])
				break;

			/*
			 * Has the administrative port state changed?
			 */
			assert(!(disable_port && enable_port));

			if (enable_port) {
				link_state[i].link_disabled = false;
				NT_LOG(DBG, NTNIC, "%s: Port %i is enabled",
					drv->mp_port_id_str[i], i);
			}

			if (is_port_disabled)
				continue;

			if (link_info->port_action[i].port_lpbk_mode != last_lpbk_mode[i]) {
				set_loopback(drv,
					i,
					link_info->port_action[i].port_lpbk_mode,
					last_lpbk_mode[i]);

				if (link_info->port_action[i].port_lpbk_mode == 1)
					link_state[i].link_up = true;

				last_lpbk_mode[i] = link_info->port_action[i].port_lpbk_mode;
				continue;
			}

			link_state[i].link_disabled = is_port_disabled;

			if (!link_state[i].nim_present) {
				if (!link_state[i].lh_nim_absent) {
					NT_LOG(INF, NTNIC, "%s: NIM module removed",
						drv->mp_port_id_str[i]);
					link_state[i].link_up = false;
					link_state[i].lh_nim_absent = true;

				} else {
					NT_LOG(DBG, NTNIC, "%s: No NIM module, skip",
						drv->mp_port_id_str[i]);
				}

				continue;
			}
		}

		if (monitor_task_is_running[adapter_no])
			nt_os_wait_usec(5 * 100000U);	/*  5 x 0.1s = 0.5s */
	}

NT4GA_LINK_100G_MON_EXIT:
	NT_LOG(DBG, NTNIC, "%s: Stopped NT4GA 100 Gbps link monitoring thread.",
		drv->mp_adapter_id_str);
	return NULL;
}

static uint32_t nt4ga_agx_link_100g_mon(void *data)
{
	(void)_common_ptp_nim_state_machine(data);

	return 0;
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
			NT_LOG(ERR, NTNIC, "%s: Failed to initialize RPF module (%u)",
				p_adapter_info->mp_adapter_id_str, res);
			return res;
		}

		res = nthw_gfg_init(&gfg_mod[adapter_no], fpga, 0 /* Only one instance */);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "%s: Failed to initialize GFG module (%u)",
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

		if (res == 0) {
			p_adapter_info->nt4ga_link.speed_capa = NT_LINK_SPEED_100G;
			p_adapter_info->nt4ga_link.variables_initialized = true;
		}
	}

	/*
	 * Create state-machine thread
	 */
	if (res == 0) {
		if (!monitor_task_is_running[adapter_no]) {
			res = rte_thread_create(&monitor_tasks[adapter_no], NULL,
					nt4ga_agx_link_100g_mon, p_adapter_info);
		}
	}

	return res;
}
