/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "ntnic_stat.h"

#include <malloc.h>

nthw_stat_t *nthw_stat_new(void)
{
	nthw_stat_t *p = malloc(sizeof(nthw_stat_t));

	if (p)
		memset(p, 0, sizeof(nthw_stat_t));

	return p;
}

void nthw_stat_delete(nthw_stat_t *p)
{
	free(p);
}

int nthw_stat_init(nthw_stat_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	uint64_t n_module_version_packed64 = -1;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_STA, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: STAT %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_stat = mod;

	n_module_version_packed64 = nthw_module_get_version_packed64(p->mp_mod_stat);
	NT_LOG(DBG, NTHW, "%s: STAT %d: version=0x%08lX", p_adapter_id_str, p->mn_instance,
		n_module_version_packed64);

	{
		nthw_register_t *p_reg;
		/* STA_CFG register */
		p_reg = nthw_module_get_register(p->mp_mod_stat, STA_CFG);
		p->mp_fld_dma_ena = nthw_register_get_field(p_reg, STA_CFG_DMA_ENA);
		p->mp_fld_cnt_clear = nthw_register_get_field(p_reg, STA_CFG_CNT_CLEAR);

		/* CFG: fields NOT available from v. 3 */
		p->mp_fld_tx_disable = nthw_register_query_field(p_reg, STA_CFG_TX_DISABLE);
		p->mp_fld_cnt_freeze = nthw_register_query_field(p_reg, STA_CFG_CNT_FRZ);

		/* STA_STATUS register */
		p_reg = nthw_module_get_register(p->mp_mod_stat, STA_STATUS);
		p->mp_fld_stat_toggle_missed =
			nthw_register_get_field(p_reg, STA_STATUS_STAT_TOGGLE_MISSED);

		/* HOST_ADR registers */
		p_reg = nthw_module_get_register(p->mp_mod_stat, STA_HOST_ADR_LSB);
		p->mp_fld_dma_lsb = nthw_register_get_field(p_reg, STA_HOST_ADR_LSB_LSB);

		p_reg = nthw_module_get_register(p->mp_mod_stat, STA_HOST_ADR_MSB);
		p->mp_fld_dma_msb = nthw_register_get_field(p_reg, STA_HOST_ADR_MSB_MSB);

		/* Binning cycles */
		p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_BIN);

		if (p_reg) {
			p->mp_fld_load_bin = nthw_register_get_field(p_reg, STA_LOAD_BIN_BIN);

			/* Bandwidth load for RX port 0 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_BPS_RX_0);

			if (p_reg) {
				p->mp_fld_load_bps_rx0 =
					nthw_register_get_field(p_reg, STA_LOAD_BPS_RX_0_BPS);

			} else {
				p->mp_fld_load_bps_rx0 = NULL;
			}

			/* Bandwidth load for RX port 1 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_BPS_RX_1);

			if (p_reg) {
				p->mp_fld_load_bps_rx1 =
					nthw_register_get_field(p_reg, STA_LOAD_BPS_RX_1_BPS);

			} else {
				p->mp_fld_load_bps_rx1 = NULL;
			}

			/* Bandwidth load for TX port 0 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_BPS_TX_0);

			if (p_reg) {
				p->mp_fld_load_bps_tx0 =
					nthw_register_get_field(p_reg, STA_LOAD_BPS_TX_0_BPS);

			} else {
				p->mp_fld_load_bps_tx0 = NULL;
			}

			/* Bandwidth load for TX port 1 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_BPS_TX_1);

			if (p_reg) {
				p->mp_fld_load_bps_tx1 =
					nthw_register_get_field(p_reg, STA_LOAD_BPS_TX_1_BPS);

			} else {
				p->mp_fld_load_bps_tx1 = NULL;
			}

			/* Packet load for RX port 0 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_PPS_RX_0);

			if (p_reg) {
				p->mp_fld_load_pps_rx0 =
					nthw_register_get_field(p_reg, STA_LOAD_PPS_RX_0_PPS);

			} else {
				p->mp_fld_load_pps_rx0 = NULL;
			}

			/* Packet load for RX port 1 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_PPS_RX_1);

			if (p_reg) {
				p->mp_fld_load_pps_rx1 =
					nthw_register_get_field(p_reg, STA_LOAD_PPS_RX_1_PPS);

			} else {
				p->mp_fld_load_pps_rx1 = NULL;
			}

			/* Packet load for TX port 0 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_PPS_TX_0);

			if (p_reg) {
				p->mp_fld_load_pps_tx0 =
					nthw_register_get_field(p_reg, STA_LOAD_PPS_TX_0_PPS);

			} else {
				p->mp_fld_load_pps_tx0 = NULL;
			}

			/* Packet load for TX port 1 */
			p_reg = nthw_module_query_register(p->mp_mod_stat, STA_LOAD_PPS_TX_1);

			if (p_reg) {
				p->mp_fld_load_pps_tx1 =
					nthw_register_get_field(p_reg, STA_LOAD_PPS_TX_1_PPS);

			} else {
				p->mp_fld_load_pps_tx1 = NULL;
			}

		} else {
			p->mp_fld_load_bin = NULL;
			p->mp_fld_load_bps_rx0 = NULL;
			p->mp_fld_load_bps_rx1 = NULL;
			p->mp_fld_load_bps_tx0 = NULL;
			p->mp_fld_load_bps_tx1 = NULL;
			p->mp_fld_load_pps_rx0 = NULL;
			p->mp_fld_load_pps_rx1 = NULL;
			p->mp_fld_load_pps_tx0 = NULL;
			p->mp_fld_load_pps_tx1 = NULL;
		}
	}

	/* Params */
	p->m_nb_nim_ports = nthw_fpga_get_product_param(p_fpga, NT_NIMS, 0);
	p->m_nb_phy_ports = nthw_fpga_get_product_param(p_fpga, NT_PHY_PORTS, 0);

	/* VSWITCH */
	p->m_nb_rx_ports = nthw_fpga_get_product_param(p_fpga, NT_STA_RX_PORTS, -1);

	if (p->m_nb_rx_ports == -1) {
		/* non-VSWITCH */
		p->m_nb_rx_ports = nthw_fpga_get_product_param(p_fpga, NT_RX_PORTS, -1);

		if (p->m_nb_rx_ports == -1) {
			/* non-VSWITCH */
			p->m_nb_rx_ports = nthw_fpga_get_product_param(p_fpga, NT_PORTS, 0);
		}
	}

	p->m_nb_rpp_per_ps = nthw_fpga_get_product_param(p_fpga, NT_RPP_PER_PS, 0);

	p->m_nb_tx_ports = nthw_fpga_get_product_param(p_fpga, NT_TX_PORTS, 0);
	p->m_rx_port_replicate = nthw_fpga_get_product_param(p_fpga, NT_RX_PORT_REPLICATE, 0);

	/* VSWITCH */
	p->m_nb_color_counters = nthw_fpga_get_product_param(p_fpga, NT_STA_COLORS, 64) * 2;

	if (p->m_nb_color_counters == 0) {
		/* non-VSWITCH */
		p->m_nb_color_counters = nthw_fpga_get_product_param(p_fpga, NT_CAT_FUNCS, 0) * 2;
	}

	p->m_nb_rx_host_buffers = nthw_fpga_get_product_param(p_fpga, NT_QUEUES, 0);
	p->m_nb_tx_host_buffers = p->m_nb_rx_host_buffers;

	p->m_dbs_present = nthw_fpga_get_product_param(p_fpga, NT_DBS_PRESENT, 0);

	p->m_nb_rx_hb_counters = (p->m_nb_rx_host_buffers * (6 + 2 *
				(n_module_version_packed64 >= VERSION_PACKED64(0, 6) ?
					p->m_dbs_present : 0)));

	p->m_nb_tx_hb_counters = 0;

	p->m_nb_rx_port_counters = 42 +
		2 * (n_module_version_packed64 >= VERSION_PACKED64(0, 6) ? p->m_dbs_present : 0);
	p->m_nb_tx_port_counters = 0;

	p->m_nb_counters =
		p->m_nb_color_counters + p->m_nb_rx_hb_counters + p->m_nb_tx_hb_counters;

	p->mn_stat_layout_version = 0;

	if (n_module_version_packed64 >= VERSION_PACKED64(0, 9)) {
		p->mn_stat_layout_version = 7;

	} else if (n_module_version_packed64 >= VERSION_PACKED64(0, 8)) {
		p->mn_stat_layout_version = 6;

	} else if (n_module_version_packed64 >= VERSION_PACKED64(0, 6)) {
		p->mn_stat_layout_version = 5;

	} else if (n_module_version_packed64 >= VERSION_PACKED64(0, 4)) {
		p->mn_stat_layout_version = 4;

	} else if (n_module_version_packed64 >= VERSION_PACKED64(0, 3)) {
		p->mn_stat_layout_version = 3;

	} else if (n_module_version_packed64 >= VERSION_PACKED64(0, 2)) {
		p->mn_stat_layout_version = 2;

	} else if (n_module_version_packed64 > VERSION_PACKED64(0, 0)) {
		p->mn_stat_layout_version = 1;

	} else {
		p->mn_stat_layout_version = 0;
		NT_LOG(ERR, NTHW, "%s: unknown module_version 0x%08lX layout=%d",
			p_adapter_id_str, n_module_version_packed64, p->mn_stat_layout_version);
	}

	assert(p->mn_stat_layout_version);

	/* STA module 0.2+ adds IPF counters per port (Rx feature) */
	if (n_module_version_packed64 >= VERSION_PACKED64(0, 2))
		p->m_nb_rx_port_counters += 6;

	/* STA module 0.3+ adds TX stats */
	if (n_module_version_packed64 >= VERSION_PACKED64(0, 3) || p->m_nb_tx_ports >= 1)
		p->mb_has_tx_stats = true;

	/* STA module 0.3+ adds TX stat counters */
	if (n_module_version_packed64 >= VERSION_PACKED64(0, 3))
		p->m_nb_tx_port_counters += 22;

	/* STA module 0.4+ adds TX drop event counter */
	if (n_module_version_packed64 >= VERSION_PACKED64(0, 4))
		p->m_nb_tx_port_counters += 1;	/* TX drop event counter */

	/*
	 * STA module 0.6+ adds pkt filter drop octets+pkts, retransmit and
	 * duplicate counters
	 */
	if (n_module_version_packed64 >= VERSION_PACKED64(0, 6)) {
		p->m_nb_rx_port_counters += 4;
		p->m_nb_tx_port_counters += 1;
	}

	p->m_nb_counters += (p->m_nb_rx_ports * p->m_nb_rx_port_counters);

	if (p->mb_has_tx_stats)
		p->m_nb_counters += (p->m_nb_tx_ports * p->m_nb_tx_port_counters);

	/* Output params (debug) */
	NT_LOG(DBG, NTHW, "%s: nims=%d rxports=%d txports=%d rxrepl=%d colors=%d queues=%d",
		p_adapter_id_str, p->m_nb_nim_ports, p->m_nb_rx_ports, p->m_nb_tx_ports,
		p->m_rx_port_replicate, p->m_nb_color_counters, p->m_nb_rx_host_buffers);
	NT_LOG(DBG, NTHW, "%s: hbs=%d hbcounters=%d rxcounters=%d txcounters=%d",
		p_adapter_id_str, p->m_nb_rx_host_buffers, p->m_nb_rx_hb_counters,
		p->m_nb_rx_port_counters, p->m_nb_tx_port_counters);
	NT_LOG(DBG, NTHW, "%s: layout=%d", p_adapter_id_str, p->mn_stat_layout_version);
	NT_LOG(DBG, NTHW, "%s: counters=%d (0x%X)", p_adapter_id_str, p->m_nb_counters,
		p->m_nb_counters);

	/* Init */
	if (p->mp_fld_tx_disable)
		nthw_field_set_flush(p->mp_fld_tx_disable);

	nthw_field_update_register(p->mp_fld_cnt_clear);
	nthw_field_set_flush(p->mp_fld_cnt_clear);
	nthw_field_clr_flush(p->mp_fld_cnt_clear);

	nthw_field_update_register(p->mp_fld_stat_toggle_missed);
	nthw_field_set_flush(p->mp_fld_stat_toggle_missed);

	nthw_field_update_register(p->mp_fld_dma_ena);
	nthw_field_clr_flush(p->mp_fld_dma_ena);
	nthw_field_update_register(p->mp_fld_dma_ena);

	/* Set the sliding windows size for port load */
	if (p->mp_fld_load_bin) {
		uint32_t rpp = nthw_fpga_get_product_param(p_fpga, NT_RPP_PER_PS, 0);
		if (rpp == 0) {
			NT_LOG(ERR, NTHW, "RPP has wrong value"); /* Avoid divide by 0 */
			return -1;
		}
		uint32_t bin =
			(uint32_t)(((PORT_LOAD_WINDOWS_SIZE * 1000000000000ULL) / (32ULL * rpp)) -
				1ULL);
		nthw_field_set_val_flush32(p->mp_fld_load_bin, bin);
	}

	return 0;
}

int nthw_stat_set_dma_address(nthw_stat_t *p, uint64_t stat_dma_physical,
	uint32_t *p_stat_dma_virtual)
{
	assert(p_stat_dma_virtual);
	p->mp_timestamp = NULL;

	p->m_stat_dma_physical = stat_dma_physical;
	p->mp_stat_dma_virtual = p_stat_dma_virtual;

	memset(p->mp_stat_dma_virtual, 0, (p->m_nb_counters * sizeof(uint32_t)));

	nthw_field_set_val_flush32(p->mp_fld_dma_msb,
		(uint32_t)((p->m_stat_dma_physical >> 32) & 0xffffffff));
	nthw_field_set_val_flush32(p->mp_fld_dma_lsb,
		(uint32_t)(p->m_stat_dma_physical & 0xffffffff));

	p->mp_timestamp = (uint64_t *)(p->mp_stat_dma_virtual + p->m_nb_counters);
	NT_LOG(DBG, NTHW,
		"stat_dma_physical=%" PRIX64 " p_stat_dma_virtual=%" PRIX64
		" mp_timestamp=%" PRIX64 "", p->m_stat_dma_physical,
		(uint64_t)p->mp_stat_dma_virtual, (uint64_t)p->mp_timestamp);
	*p->mp_timestamp = (uint64_t)(int64_t)-1;
	return 0;
}

int nthw_stat_trigger(nthw_stat_t *p)
{
	int n_toggle_miss = nthw_field_get_updated(p->mp_fld_stat_toggle_missed);

	if (n_toggle_miss)
		nthw_field_set_flush(p->mp_fld_stat_toggle_missed);

	if (p->mp_timestamp)
		*p->mp_timestamp = -1;	/* Clear old ts */

	nthw_field_update_register(p->mp_fld_dma_ena);
	nthw_field_set_flush(p->mp_fld_dma_ena);

	return 0;
}

int nthw_stat_get_load_bps_rx(nthw_stat_t *p, uint8_t port, uint32_t *val)
{
	switch (port) {
	case 0:
		if (p->mp_fld_load_bps_rx0) {
			*val = nthw_field_get_updated(p->mp_fld_load_bps_rx0);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	case 1:
		if (p->mp_fld_load_bps_rx1) {
			*val = nthw_field_get_updated(p->mp_fld_load_bps_rx1);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	default:
		return -1;
	}
}

int nthw_stat_get_load_bps_tx(nthw_stat_t *p, uint8_t port, uint32_t *val)
{
	switch (port) {
	case 0:
		if (p->mp_fld_load_bps_tx0) {
			*val = nthw_field_get_updated(p->mp_fld_load_bps_tx0);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	case 1:
		if (p->mp_fld_load_bps_tx1) {
			*val = nthw_field_get_updated(p->mp_fld_load_bps_tx1);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	default:
		return -1;
	}
}

int nthw_stat_get_load_pps_rx(nthw_stat_t *p, uint8_t port, uint32_t *val)
{
	switch (port) {
	case 0:
		if (p->mp_fld_load_pps_rx0) {
			*val = nthw_field_get_updated(p->mp_fld_load_pps_rx0);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	case 1:
		if (p->mp_fld_load_pps_rx1) {
			*val = nthw_field_get_updated(p->mp_fld_load_pps_rx1);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	default:
		return -1;
	}
}

int nthw_stat_get_load_pps_tx(nthw_stat_t *p, uint8_t port, uint32_t *val)
{
	switch (port) {
	case 0:
		if (p->mp_fld_load_pps_tx0) {
			*val = nthw_field_get_updated(p->mp_fld_load_pps_tx0);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	case 1:
		if (p->mp_fld_load_pps_tx1) {
			*val = nthw_field_get_updated(p->mp_fld_load_pps_tx1);
			return 0;

		} else {
			*val = 0;
			return -1;
		}

		break;

	default:
		return -1;
	}
}
