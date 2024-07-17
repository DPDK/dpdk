/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <string.h>	/* memcmp, memset */

#include "nt_util.h"
#include "ntlog.h"
#include "i2c_nim.h"
#include "ntnic_mod_reg.h"

/*
 * Initialize NIM, Code based on nt200e3_2_ptp.cpp: MyPort::createNim()
 */
static int _create_nim(adapter_info_t *drv, int port)
{
	int res = 0;
	const uint8_t valid_nim_id = 17U;
	nim_i2c_ctx_t *nim_ctx;
	sfp_nim_state_t nim;
	nt4ga_link_t *link_info = &drv->nt4ga_link;

	assert(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	assert(link_info->variables_initialized);

	nim_ctx = &link_info->u.var100g.nim_ctx[port];

	/*
	 * Wait a little after a module has been inserted before trying to access I2C
	 * data, otherwise the module will not respond correctly.
	 */
	nt_os_wait_usec(1000000);	/* pause 1.0s */

	res = construct_and_preinit_nim(nim_ctx, NULL);

	if (res)
		return res;

	res = nim_state_build(nim_ctx, &nim);

	if (res)
		return res;

	NT_LOG(DBG, NTHW, "%s: NIM id = %u (%s), br = %u, vendor = '%s', pn = '%s', sn='%s'\n",
		drv->mp_port_id_str[port], nim_ctx->nim_id, nim_id_to_text(nim_ctx->nim_id), nim.br,
		nim_ctx->vendor_name, nim_ctx->prod_no, nim_ctx->serial_no);

	/*
	 * Does the driver support the NIM module type?
	 */
	if (nim_ctx->nim_id != valid_nim_id) {
		NT_LOG(ERR, NTHW, "%s: The driver does not support the NIM module type %s\n",
			drv->mp_port_id_str[port], nim_id_to_text(nim_ctx->nim_id));
		NT_LOG(DBG, NTHW, "%s: The driver supports the NIM module type %s\n",
			drv->mp_port_id_str[port], nim_id_to_text(valid_nim_id));
		return -1;
	}

	return res;
}

/*
 * Initialize one 100 Gbps port.
 * The function shall not assume anything about the state of the adapter
 * and/or port.
 */
static int _port_init(adapter_info_t *drv, int port)
{
	int res;
	nt4ga_link_t *link_info = &drv->nt4ga_link;

	assert(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	assert(link_info->variables_initialized);

	/*
	 * Phase 1. Pre-state machine (`port init` functions)
	 * 1.1) nt4ga_adapter::port_init()
	 */

	/* No adapter set-up here, only state variables */

	/* 1.2) MyPort::init() */
	link_info->link_info[port].link_speed = NT_LINK_SPEED_100G;
	link_info->link_info[port].link_duplex = NT_LINK_DUPLEX_FULL;
	link_info->link_info[port].link_auto_neg = NT_LINK_AUTONEG_OFF;
	link_info->speed_capa |= NT_LINK_SPEED_100G;

	/* Phase 3. Link state machine steps */

	/* 3.1) Create NIM, ::createNim() */
	res = _create_nim(drv, port);

	if (res) {
		NT_LOG(WRN, NTNIC, "%s: NIM initialization failed\n", drv->mp_port_id_str[port]);
		return res;
	}

	NT_LOG(DBG, NTNIC, "%s: NIM initialized\n", drv->mp_port_id_str[port]);

	return res;
}

/*
 * State machine shared between kernel and userland
 */
static int _common_ptp_nim_state_machine(void *data)
{
	adapter_info_t *drv = (adapter_info_t *)data;
	fpga_info_t *fpga_info = &drv->fpga_info;
	nt4ga_link_t *link_info = &drv->nt4ga_link;
	nthw_fpga_t *fpga = fpga_info->mp_fpga;
	const int adapter_no = drv->adapter_no;
	const int nb_ports = fpga_info->n_phy_ports;
	uint32_t last_lpbk_mode[NUM_ADAPTER_PORTS_MAX];

	link_state_t *link_state;

	if (!fpga) {
		NT_LOG(ERR, NTNIC, "%s: fpga is NULL\n", drv->mp_adapter_id_str);
		goto NT4GA_LINK_100G_MON_EXIT;
	}

	assert(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);
	link_state = link_info->link_state;

	monitor_task_is_running[adapter_no] = 1;
	memset(last_lpbk_mode, 0, sizeof(last_lpbk_mode));

	if (monitor_task_is_running[adapter_no])
		NT_LOG(DBG, NTNIC, "%s: link state machine running...\n", drv->mp_adapter_id_str);

	while (monitor_task_is_running[adapter_no]) {
		int i;

		for (i = 0; i < nb_ports; i++) {
			const bool is_port_disabled = link_info->port_action[i].port_disable;
			const bool was_port_disabled = link_state[i].link_disabled;
			const bool disable_port = is_port_disabled && !was_port_disabled;
			const bool enable_port = !is_port_disabled && was_port_disabled;

			if (!monitor_task_is_running[adapter_no])	/* stop quickly */
				break;

			/* Has the administrative port state changed? */
			assert(!(disable_port && enable_port));

			if (disable_port) {
				memset(&link_state[i], 0, sizeof(link_state[i]));
				link_info->link_info[i].link_speed = NT_LINK_SPEED_UNKNOWN;
				link_state[i].link_disabled = true;
				/* Turn off laser and LED, etc. */
				(void)_create_nim(drv, i);
				NT_LOG(DBG, NTNIC, "%s: Port %i is disabled\n",
					drv->mp_port_id_str[i], i);
				continue;
			}

			if (enable_port) {
				link_state[i].link_disabled = false;
				NT_LOG(DBG, NTNIC, "%s: Port %i is enabled\n",
					drv->mp_port_id_str[i], i);
			}

			if (is_port_disabled)
				continue;

			if (link_info->port_action[i].port_lpbk_mode != last_lpbk_mode[i]) {
				/* Loopback mode has changed. Do something */
				_port_init(drv, i);

				NT_LOG(INF, NTNIC, "%s: Loopback mode changed=%u\n",
					drv->mp_port_id_str[i],
					link_info->port_action[i].port_lpbk_mode);

				if (link_info->port_action[i].port_lpbk_mode == 1)
					link_state[i].link_up = true;

				last_lpbk_mode[i] = link_info->port_action[i].port_lpbk_mode;
				continue;
			}

		}	/* end-for */

		if (monitor_task_is_running[adapter_no])
			nt_os_wait_usec(5 * 100000U);	/* 5 x 0.1s = 0.5s */
	}

NT4GA_LINK_100G_MON_EXIT:

	NT_LOG(DBG, NTNIC, "%s: Stopped NT4GA 100 Gbps link monitoring thread.\n",
		drv->mp_adapter_id_str);

	return 0;
}

/*
 * Userland NIM state machine
 */
static uint32_t nt4ga_link_100g_mon(void *data)
{
	(void)_common_ptp_nim_state_machine(data);

	return 0;
}

/*
 * Initialize all ports
 * The driver calls this function during initialization (of the driver).
 */
static int nt4ga_link_100g_ports_init(struct adapter_info_s *p_adapter_info, nthw_fpga_t *fpga)
{
	fpga_info_t *fpga_info = &p_adapter_info->fpga_info;
	const int adapter_no = p_adapter_info->adapter_no;
	const int nb_ports = fpga_info->n_phy_ports;
	int res = 0;

	NT_LOG(DBG, NTNIC, "%s: Initializing ports\n", p_adapter_info->mp_adapter_id_str);

	/*
	 * Initialize global variables
	 */
	assert(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);

	if (res == 0 && !p_adapter_info->nt4ga_link.variables_initialized) {
		nim_i2c_ctx_t *nim_ctx = p_adapter_info->nt4ga_link.u.var100g.nim_ctx;
		int i;

		for (i = 0; i < nb_ports; i++) {
			/* 2 + adapter port number */
			const uint8_t instance = (uint8_t)(2U + i);

			res = nthw_iic_init(&nim_ctx[i].hwiic, fpga, instance, 8);

			if (res != 0)
				break;

			nim_ctx[i].instance = instance;
			nim_ctx[i].devaddr = 0x50;	/* 0xA0 / 2 */
			nim_ctx[i].regaddr = 0U;
			nim_ctx[i].type = I2C_HWIIC;
		}

		if (res == 0) {
			p_adapter_info->nt4ga_link.speed_capa = NT_LINK_SPEED_100G;
			p_adapter_info->nt4ga_link.variables_initialized = true;
		}
	}

	/* Create state-machine thread */
	if (res == 0) {
		if (!monitor_task_is_running[adapter_no]) {
			res = rte_thread_create(&monitor_tasks[adapter_no], NULL,
					nt4ga_link_100g_mon, p_adapter_info);
		}
	}

	return res;
}

/*
 * Init 100G link ops variables
 */
static struct link_ops_s link_100g_ops = {
	.link_init = nt4ga_link_100g_ports_init,
};

void link_100g_init(void)
{
	register_100g_link_ops(&link_100g_ops);
}
