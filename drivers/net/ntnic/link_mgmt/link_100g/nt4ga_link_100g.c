/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <string.h>	/* memcmp, memset */

#include "nt_util.h"
#include "ntlog.h"
#include "i2c_nim.h"
#include "ntnic_mod_reg.h"
#include "nt_service.h"

/*
 * Swap tx/rx polarity
 */
static int _swap_tx_rx_polarity(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs, int port, bool swap)
{
	const bool tx_polarity_swap[2][4] = { { true, true, false, false },
		{ false, true, false, false }
	};
	const bool rx_polarity_swap[2][4] = { { false, true, true, true },
		{ false, true, true, false }
	};
	uint8_t lane;

	(void)drv;

	for (lane = 0U; lane < 4U; lane++) {
		if (swap) {
			nthw_mac_pcs_swap_gty_tx_polarity(mac_pcs, lane,
				tx_polarity_swap[port][lane]);
			nthw_mac_pcs_swap_gty_rx_polarity(mac_pcs, lane,
				rx_polarity_swap[port][lane]);

		} else {
			nthw_mac_pcs_swap_gty_tx_polarity(mac_pcs, lane, false);
			nthw_mac_pcs_swap_gty_rx_polarity(mac_pcs, lane, false);
		}
	}

	return 0;
}

/*
 * Reset RX
 */
static int _reset_rx(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs)
{
	(void)drv;

	nthw_mac_pcs_rx_path_rst(mac_pcs, true);
	nt_os_wait_usec(10000);	/* 10ms */
	nthw_mac_pcs_rx_path_rst(mac_pcs, false);
	nt_os_wait_usec(10000);	/* 10ms */

	return 0;
}

static void _set_loopback(struct adapter_info_s *p_adapter_info,
	nthw_mac_pcs_t *mac_pcs,
	int intf_no,
	uint32_t mode,
	uint32_t last_mode)
{
	bool swap_polerity = true;

	switch (mode) {
	case 1:
		NT_LOG(INF, NTNIC, "%s: Applying host loopback",
			p_adapter_info->mp_port_id_str[intf_no]);
		nthw_mac_pcs_set_fec(mac_pcs, true);
		nthw_mac_pcs_set_host_loopback(mac_pcs, true);
		swap_polerity = false;
		break;

	case 2:
		NT_LOG(INF, NTNIC, "%s: Applying line loopback",
			p_adapter_info->mp_port_id_str[intf_no]);
		nthw_mac_pcs_set_line_loopback(mac_pcs, true);
		break;

	default:
		switch (last_mode) {
		case 1:
			NT_LOG(INF, NTNIC, "%s: Removing host loopback",
				p_adapter_info->mp_port_id_str[intf_no]);
			nthw_mac_pcs_set_host_loopback(mac_pcs, false);
			break;

		case 2:
			NT_LOG(INF, NTNIC, "%s: Removing line loopback",
				p_adapter_info->mp_port_id_str[intf_no]);
			nthw_mac_pcs_set_line_loopback(mac_pcs, false);
			break;

		default:
			/* Do nothing */
			break;
		}

		break;
	}

	if (p_adapter_info->fpga_info.nthw_hw_info.hw_id == 2 ||
		p_adapter_info->hw_info.n_nthw_adapter_id == NT_HW_ADAPTER_ID_NT200A02) {
		(void)_swap_tx_rx_polarity(p_adapter_info, mac_pcs, intf_no, swap_polerity);
	}

	/* After changing the loopback the system must be properly reset */
	_reset_rx(p_adapter_info, mac_pcs);

	nt_os_wait_usec(10000);	/* 10ms - arbitrary choice */

	if (!nthw_mac_pcs_is_rx_path_rst(mac_pcs)) {
		nthw_mac_pcs_reset_bip_counters(mac_pcs);

		if (!nthw_mac_pcs_get_fec_bypass(mac_pcs))
			nthw_mac_pcs_reset_fec_counters(mac_pcs);
	}
}

/*
 * Function to retrieve the current state of a link (for one port)
 */
static int _link_state_build(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs,
	nthw_gpio_phy_t *gpio_phy, int port, link_state_t *state,
	bool is_port_disabled)
{
	uint32_t abs;
	uint32_t phy_link_state;
	uint32_t lh_abs;
	uint32_t ll_phy_link_state;
	uint32_t link_down_cnt;
	uint32_t nim_interr;
	uint32_t lh_local_fault;
	uint32_t lh_remote_fault;
	uint32_t lh_internal_local_fault;
	uint32_t lh_received_local_fault;

	RTE_ASSERT(state);

	memset(state, 0, sizeof(*state));
	state->link_disabled = is_port_disabled;
	nthw_mac_pcs_get_link_summary(mac_pcs, &abs, &phy_link_state, &lh_abs, &ll_phy_link_state,
		&link_down_cnt, &nim_interr, &lh_local_fault,
		&lh_remote_fault, &lh_internal_local_fault,
		&lh_received_local_fault);

	RTE_ASSERT(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	state->nim_present = nthw_gpio_phy_is_module_present(gpio_phy, (uint8_t)port);
	state->lh_nim_absent = !state->nim_present;
	state->link_up = phy_link_state ? true : false;

	{
		static char lsbuf[NUM_ADAPTER_MAX][NUM_ADAPTER_PORTS_MAX][256];
		char buf[255];
		const int adapter_no = drv->adapter_no;
		snprintf(buf, sizeof(buf),
			"%s: Port = %d: abs = %u, phy_link_state = %u, lh_abs = %u, "
			"ll_phy_link_state = %u, "
			"link_down_cnt = %u, nim_interr = %u, lh_local_fault = %u, lh_remote_fault = "
			"%u, lh_internal_local_fault = %u, lh_received_local_fault = %u",
			drv->mp_adapter_id_str, mac_pcs->mn_instance, abs, phy_link_state, lh_abs,
			ll_phy_link_state, link_down_cnt, nim_interr, lh_local_fault,
			lh_remote_fault, lh_internal_local_fault, lh_received_local_fault);

		if (strcmp(lsbuf[adapter_no][port], buf) != 0) {
			snprintf(lsbuf[adapter_no][port], sizeof(lsbuf[adapter_no][port]), "%s",
				buf);
			lsbuf[adapter_no][port][sizeof(lsbuf[adapter_no][port]) - 1U] = '\0';
			NT_LOG(DBG, NTNIC, "%s", lsbuf[adapter_no][port]);
		}
	}
	return 0;
}

/*
 * Check whether a NIM module is present
 */
static bool _nim_is_present(nthw_gpio_phy_t *gpio_phy, uint8_t if_no)
{
	RTE_ASSERT(if_no < NUM_ADAPTER_PORTS_MAX);

	return nthw_gpio_phy_is_module_present(gpio_phy, if_no);
}

/*
 * Enable RX
 */
static int _enable_rx(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs)
{
	(void)drv;	/* unused */
	nthw_mac_pcs_set_rx_enable(mac_pcs, true);
	return 0;
}

/*
 * Enable TX
 */
static int _enable_tx(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs)
{
	(void)drv;	/* unused */
	nthw_mac_pcs_set_tx_enable(mac_pcs, true);
	nthw_mac_pcs_set_tx_sel_host(mac_pcs, true);
	return 0;
}

/*
 * Disable RX
 */
static int _disable_rx(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs)
{
	(void)drv;	/* unused */
	nthw_mac_pcs_set_rx_enable(mac_pcs, false);
	return 0;
}

/*
 * Disable TX
 */
static int _disable_tx(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs)
{
	(void)drv;	/* unused */
	nthw_mac_pcs_set_tx_enable(mac_pcs, false);
	nthw_mac_pcs_set_tx_sel_host(mac_pcs, false);
	return 0;
}

/*
 * Check link once NIM is installed and link can be expected.
 */
static int check_link_state(adapter_info_t *drv, nthw_mac_pcs_t *mac_pcs)
{
	bool rst_required;
	bool ber;
	bool fec_all_locked;

	rst_required = nthw_mac_pcs_reset_required(mac_pcs);

	ber = nthw_mac_pcs_get_hi_ber(mac_pcs);

	fec_all_locked = nthw_mac_pcs_get_fec_stat_all_am_locked(mac_pcs);

	if (rst_required || ber || !fec_all_locked)
		_reset_rx(drv, mac_pcs);

	return 0;
}

/*
 * Initialize NIM, Code based on nt200e3_2_ptp.cpp: MyPort::createNim()
 */
static int _create_nim(adapter_info_t *drv, int port, bool enable)
{
	int res = 0;
	const uint8_t valid_nim_id = 17U;
	nthw_gpio_phy_t *gpio_phy;
	nim_i2c_ctx_t *nim_ctx;
	sfp_nim_state_t nim;
	nt4ga_link_t *link_info = &drv->nt4ga_link;

	RTE_ASSERT(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	RTE_ASSERT(link_info->variables_initialized);

	nthw_mac_pcs_t *mac_pcs = &link_info->u.var100g.mac_pcs100g[port];
	gpio_phy = &link_info->u.var100g.gpio_phy[port];
	nim_ctx = &link_info->u.var100g.nim_ctx[port];

	/*
	 * Check NIM is present before doing GPIO PHY reset.
	 */
	if (!_nim_is_present(gpio_phy, (uint8_t)port)) {
		NT_LOG(INF, NTNIC, "%s: NIM module is absent", drv->mp_port_id_str[port]);
		return 0;
	}

	if (!enable) {
		_disable_rx(drv, mac_pcs);
		_disable_tx(drv, mac_pcs);
		_reset_rx(drv, mac_pcs);
	}

	/*
	 * Perform PHY reset.
	 */
	NT_LOG(DBG, NTNIC, "%s: Performing NIM reset", drv->mp_port_id_str[port]);
	nthw_gpio_phy_set_reset(gpio_phy, (uint8_t)port, true);
	nt_os_wait_usec(100000);/* pause 0.1s */
	nthw_gpio_phy_set_reset(gpio_phy, (uint8_t)port, false);

	/*
	 * Wait a little after a module has been inserted before trying to access I2C
	 * data, otherwise the module will not respond correctly.
	 */
	nt_os_wait_usec(1000000);	/* pause 1.0s */

	if (!_nim_is_present(gpio_phy, (uint8_t)port)) {
		NT_LOG(DBG, NTNIC, "%s: NIM module is no longer absent!",
			drv->mp_port_id_str[port]);
		return -1;
	}

	if (!_nim_is_present(gpio_phy, (uint8_t)port)) {
		NT_LOG(DBG, NTNIC, "%s: NIM module is no longer absent!",
			drv->mp_port_id_str[port]);
		return -1;
	}

	res = nthw_construct_and_preinit_nim(nim_ctx, NULL);

	if (res)
		return res;

	res = nthw_nim_state_build(nim_ctx, &nim);

	if (res)
		return res;

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
		nthw_gpio_phy_set_low_power(gpio_phy, (uint8_t)port, false);

	} else {
		NT_LOG(DBG, NTNIC, "%s: Asserting low power", drv->mp_port_id_str[port]);
		nthw_gpio_phy_set_low_power(gpio_phy, (uint8_t)port, true);
	}

	return res;
}

/*
 * Initialize one 100 Gbps port.
 * The function shall not assume anything about the state of the adapter
 * and/or port.
 */
static int _port_init(adapter_info_t *drv, nthw_fpga_t *fpga, int port)
{
	int adapter_id;
	int hw_id;
	int res;
	nt4ga_link_t *link_info = &drv->nt4ga_link;

	nthw_mac_pcs_t *mac_pcs;

	RTE_ASSERT(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	RTE_ASSERT(link_info->variables_initialized);

	if (fpga && fpga->p_fpga_info) {
		adapter_id = fpga->p_fpga_info->n_nthw_adapter_id;
		hw_id = fpga->p_fpga_info->nthw_hw_info.hw_id;

	} else {
		adapter_id = -1;
		hw_id = -1;
	}

	mac_pcs = &link_info->u.var100g.mac_pcs100g[port];

	/*
	 * Phase 1. Pre-state machine (`port init` functions)
	 * 1.1) nt4ga_adapter::nthw_port_init()
	 */

	/* No adapter set-up here, only state variables */

	/* 1.2) MyPort::init() */
	link_info->link_info[port].link_speed = NT_LINK_SPEED_100G;
	link_info->link_info[port].link_duplex = NT_LINK_DUPLEX_FULL;
	link_info->link_info[port].link_auto_neg = NT_LINK_AUTONEG_OFF;
	link_info->speed_capa |= NT_LINK_SPEED_100G;
	nthw_mac_pcs_set_led_mode(mac_pcs, NTHW_MAC_PCS_LED_AUTO);
	nthw_mac_pcs_set_receiver_equalization_mode(mac_pcs, nthw_mac_pcs_receiver_mode_lpm);

	/*
	 * NT200A01 build 2 HW and NT200A02 that require GTY polarity swap
	 * if (adapter is `NT200A01 build 2 HW or NT200A02`)
	 */
	if (adapter_id == NT_HW_ADAPTER_ID_NT200A02 || hw_id == 2)
		(void)_swap_tx_rx_polarity(drv, mac_pcs, port, true);

	nthw_mac_pcs_set_ts_eop(mac_pcs, true);	/* end-of-frame timestamping */

	/* Work in ABSOLUTE timing mode, don't set IFG mode. */

	/* Phase 2. Pre-state machine (`setup` functions) */

	/* 2.1) nt200a0x.cpp:Myport::setup() */
	NT_LOG(DBG, NTNIC, "%s: Setting up port %d", drv->mp_port_id_str[port], port);

	NT_LOG(DBG, NTNIC, "%s: Port %d: PHY TX enable", drv->mp_port_id_str[port], port);
	_enable_tx(drv, mac_pcs);
	_reset_rx(drv, mac_pcs);

	/* 2.2) Nt4gaPort::setup() */
	if (fpga && nthw_gmf_init(NULL, fpga, port) == 0) {
		nthw_gmf_t gmf;

		if (nthw_gmf_init(&gmf, fpga, port) == 0)
			nthw_gmf_set_enable(&gmf, true);
	}

	/* Phase 3. Link state machine steps */

	/* 3.1) Create NIM, ::createNim() */
	res = _create_nim(drv, port, true);

	if (res) {
		NT_LOG(WRN, NTNIC, "%s: NIM initialization failed", drv->mp_port_id_str[port]);
		return res;
	}

	NT_LOG(DBG, NTNIC, "%s: NIM initialized", drv->mp_port_id_str[port]);

	/* 3.2) MyPort::nimReady() */

	/* 3.3) MyPort::nimReady100Gb() */

	/* Setting FEC resets the lane counter in one half of the GMF */
	nthw_mac_pcs_set_fec(mac_pcs, true);
	NT_LOG(DBG, NTNIC, "%s: Port %d: HOST FEC enabled", drv->mp_port_id_str[port], port);

	if (adapter_id == NT_HW_ADAPTER_ID_NT200A02 || hw_id == 2) {
		const uint8_t pre = 5;
		const uint8_t diff = 25;
		const uint8_t post = 12;

		uint8_t lane = 0;

		for (lane = 0; lane < 4; lane++)
			nthw_mac_pcs_set_gty_tx_tuning(mac_pcs, lane, pre, diff, post);

	} else {
		NT_LOG(ERR, NTNIC, "Unhandled AdapterId/HwId: %02x_hwid%d", adapter_id, hw_id);
		RTE_ASSERT(0);
	}

	_reset_rx(drv, mac_pcs);

	/*
	 * 3.4) MyPort::setLinkState()
	 *
	 * Compensation = 1640 - dly
	 * CMAC-core dly 188 ns
	 * FEC no correction 87 ns
	 * FEC active correction 211
	 */
	if (nthw_mac_pcs_get_fec_valid(mac_pcs))
		nthw_mac_pcs_set_timestamp_comp_rx(mac_pcs, (1640 - 188 - 211));

	else
		nthw_mac_pcs_set_timestamp_comp_rx(mac_pcs, (1640 - 188 - 87));

	/* 3.5) uint32_t MyPort::macConfig(nt_link_state_t link_state) */
	_enable_rx(drv, mac_pcs);

	nthw_mac_pcs_set_host_loopback(mac_pcs, false);

	return res;
}

/*
 * State machine shared between kernel and userland
 */
static int _common_ptp_nim_state_machine(void *data)
{
	static adapter_info_t *drv;
	static nt4ga_link_t *link_info;
	static nthw_fpga_t *fpga;
	static int nb_ports;
	static nim_i2c_ctx_t *nim_ctx;
	static link_state_t *link_state;
	static nthw_mac_pcs_t *mac_pcs;
	static nthw_gpio_phy_t *gpio_phy;
	static uint32_t last_lpbk_mode[NUM_ADAPTER_PORTS_MAX];

	struct nt_service *adapter_mon_srv = nthw_service_get_info(RTE_NTNIC_SERVICE_ADAPTER_MON);
	RTE_ASSERT(adapter_mon_srv != NULL);

	if (!NT_SERVICE_GET_STATE(adapter_mon_srv)) {
		drv = (adapter_info_t *)data;
		RTE_ASSERT(drv != NULL);

		fpga_info_t *fpga_info = &drv->fpga_info;
		int adapter_no = drv->adapter_no;

		link_info = &drv->nt4ga_link;
		fpga = fpga_info->mp_fpga;
		nb_ports = fpga_info->n_phy_ports;

		if (!fpga) {
			NT_LOG(ERR, NTNIC, "%s: fpga is NULL", drv->mp_adapter_id_str);
			return -1;
		}

		RTE_ASSERT(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);
		nim_ctx = link_info->u.var100g.nim_ctx;
		link_state = link_info->link_state;
		mac_pcs = link_info->u.var100g.mac_pcs100g;
		gpio_phy = link_info->u.var100g.gpio_phy;

		memset(last_lpbk_mode, 0, sizeof(last_lpbk_mode));

		NT_LOG(DBG, NTNIC, "%s: link state machine running...", drv->mp_adapter_id_str);

		NT_LOG(INF, NTNIC, "Adapter monitor service started on lcore %i", rte_lcore_id());
		adapter_mon_srv->lcore = rte_lcore_id();
		NT_SERVICE_SET_STATE(adapter_mon_srv, true);
		return 0;
	}

	int i;
	static bool reported_link[NUM_ADAPTER_PORTS_MAX] = { false };

	for (i = 0; i < nb_ports; i++) {
		link_state_t new_link_state;
		const bool is_port_disabled = link_info->port_action[i].port_disable;
		const bool was_port_disabled = link_state[i].link_disabled;
		const bool disable_port = is_port_disabled && !was_port_disabled;
		const bool enable_port = !is_port_disabled && was_port_disabled;

		if (!rte_service_runstate_get(adapter_mon_srv->id))	/* stop quickly */
			break;

		/* Has the administrative port state changed? */
		RTE_ASSERT(!(disable_port && enable_port));

		if (disable_port) {
			memset(&link_state[i], 0, sizeof(link_state[i]));
			link_info->link_info[i].link_speed = NT_LINK_SPEED_UNKNOWN;
			link_state[i].link_disabled = true;
			reported_link[i] = false;
			/* Turn off laser and LED, etc. */
			(void)_create_nim(drv, i, false);
			NT_LOG(DBG, NTNIC, "%s: Port %i is disabled",
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
			if (!_nim_is_present(&gpio_phy[i], (uint8_t)i)) {
				/*
				 * If there is no Nim present, we need to initialize the
				 * port anyway
				 */
				_port_init(drv, fpga, i);
			}

			NT_LOG(INF, NTNIC, "%s: Loopback mode changed=%u",
				drv->mp_port_id_str[i],
				link_info->port_action[i].port_lpbk_mode);
			_set_loopback(drv,
				&mac_pcs[i],
				i,
				link_info->port_action[i].port_lpbk_mode,
				last_lpbk_mode[i]);

			if (link_info->port_action[i].port_lpbk_mode == 1)
				link_state[i].link_up = true;

			last_lpbk_mode[i] = link_info->port_action[i].port_lpbk_mode;
			continue;
		}

		(void)_link_state_build(drv, &mac_pcs[i], &gpio_phy[i], i, &new_link_state,
			is_port_disabled);

		if (!new_link_state.nim_present) {
			if (link_state[i].nim_present) {
				NT_LOG(INF, NTNIC, "%s: NIM module removed",
					drv->mp_port_id_str[i]);
			}

			link_state[i] = new_link_state;
			continue;
		}

		/* NIM module is present */
		if (new_link_state.lh_nim_absent || !link_state[i].nim_present) {
			sfp_nim_state_t new_state;

			NT_LOG(DBG, NTNIC, "%s: NIM module inserted",
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
				drv->mp_port_id_str[i], nim_ctx->nim_id,
				nthw_nim_id_to_text(nim_ctx->nim_id),
				(unsigned int)new_state.br, nim_ctx->vendor_name,
				nim_ctx->prod_no, nim_ctx->serial_no);

			(void)_link_state_build(drv, &mac_pcs[i], &gpio_phy[i], i,
				&link_state[i], is_port_disabled);

			NT_LOG(DBG, NTNIC, "%s: NIM module initialized",
				drv->mp_port_id_str[i]);
			continue;
		}

		if (reported_link[i] != new_link_state.link_up) {
			NT_LOG(INF, NTNIC, "%s: link is %s", drv->mp_port_id_str[i],
				(new_link_state.link_up ? "up" : "down"));
			link_info->link_info[i].link_speed =
				(new_link_state.link_up ? NT_LINK_SPEED_100G
					: NT_LINK_SPEED_UNKNOWN);
			link_state[i].link_up = new_link_state.link_up;
			reported_link[i] = new_link_state.link_up;
		}

		check_link_state(drv, &mac_pcs[i]);
	}	/* end-for */

	if (rte_service_runstate_get(adapter_mon_srv->id))
		nt_os_wait_usec(5 * 100000U);	/* 5 x 0.1s = 0.5s */

	return 0;
}

/*
 * Userland NIM state machine
 */
static int nt4ga_link_100g_mon(void *data)
{
	return _common_ptp_nim_state_machine(data);
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

	NT_LOG(DBG, NTNIC, "%s: Initializing ports", p_adapter_info->mp_adapter_id_str);

	/*
	 * Initialize global variables
	 */
	RTE_ASSERT(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);

	if (!p_adapter_info->nt4ga_link.variables_initialized) {
		nthw_mac_pcs_t *mac_pcs = p_adapter_info->nt4ga_link.u.var100g.mac_pcs100g;
		nim_i2c_ctx_t *nim_ctx = p_adapter_info->nt4ga_link.u.var100g.nim_ctx;
		nthw_gpio_phy_t *gpio_phy = p_adapter_info->nt4ga_link.u.var100g.gpio_phy;
		int i;

		for (i = 0; i < nb_ports; i++) {
			/* 2 + adapter port number */
			const uint8_t instance = (uint8_t)(2U + i);
			res = nthw_mac_pcs_init(&mac_pcs[i], fpga, i /* int n_instance */);

			if (res != 0)
				break;

			res = nthw_iic_init(&nim_ctx[i].hwiic, fpga, instance, 8);

			if (res != 0)
				break;

			nim_ctx[i].instance = instance;
			nim_ctx[i].devaddr = 0x50;	/* 0xA0 / 2 */
			nim_ctx[i].regaddr = 0U;
			nim_ctx[i].type = I2C_HWIIC;

			res = nthw_gpio_phy_init(&gpio_phy[i], fpga, 0 /* Only one instance */);

			if (res != 0)
				break;
		}

		if (res == 0) {
			p_adapter_info->nt4ga_link.speed_capa = NT_LINK_SPEED_100G;
			p_adapter_info->nt4ga_link.variables_initialized = true;
		}
	}

	/* Create state-machine service */
	if (res == 0) {
		struct rte_service_spec adapter_monitor_service = {
			.name = "ntnic-adapter-monitor",
			.callback = nt4ga_link_100g_mon,
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

/*
 * Init 100G link ops variables
 */
static struct link_ops_s link_100g_ops = {
	.link_init = nt4ga_link_100g_ports_init,
};

void nthw_link_100g_init(void)
{
	register_100g_link_ops(&link_100g_ops);
}
