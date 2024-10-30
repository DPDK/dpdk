/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nt_util.h"
#include "nthw_drv.h"
#include "nthw_fpga.h"
#include "nthw_fpga_param_defs.h"
#include "nt4ga_adapter.h"
#include "ntnic_nim.h"
#include "flow_filter.h"
#include "ntnic_stat.h"
#include "ntnic_mod_reg.h"

#define DEFAULT_MAX_BPS_SPEED 100e9

static int nt4ga_stat_init(struct adapter_info_s *p_adapter_info)
{
	const char *const p_adapter_id_str = p_adapter_info->mp_adapter_id_str;
	fpga_info_t *fpga_info = &p_adapter_info->fpga_info;
	nthw_fpga_t *p_fpga = fpga_info->mp_fpga;
	nt4ga_stat_t *p_nt4ga_stat = &p_adapter_info->nt4ga_stat;

	if (p_nt4ga_stat) {
		memset(p_nt4ga_stat, 0, sizeof(nt4ga_stat_t));

	} else {
		NT_LOG_DBGX(ERR, NTNIC, "%s: ERROR", p_adapter_id_str);
		return -1;
	}

	{
		nthw_stat_t *p_nthw_stat = nthw_stat_new();

		if (!p_nthw_stat) {
			NT_LOG_DBGX(ERR, NTNIC, "%s: ERROR", p_adapter_id_str);
			return -1;
		}

		if (nthw_rmc_init(NULL, p_fpga, 0) == 0) {
			nthw_rmc_t *p_nthw_rmc = nthw_rmc_new();

			if (!p_nthw_rmc) {
				nthw_stat_delete(p_nthw_stat);
				NT_LOG(ERR, NTNIC, "%s: ERROR rmc allocation", p_adapter_id_str);
				return -1;
			}

			nthw_rmc_init(p_nthw_rmc, p_fpga, 0);
			p_nt4ga_stat->mp_nthw_rmc = p_nthw_rmc;

		} else {
			p_nt4ga_stat->mp_nthw_rmc = NULL;
		}

		if (nthw_rpf_init(NULL, p_fpga, p_adapter_info->adapter_no) == 0) {
			nthw_rpf_t *p_nthw_rpf = nthw_rpf_new();

			if (!p_nthw_rpf) {
				nthw_stat_delete(p_nthw_stat);
				NT_LOG_DBGX(ERR, NTNIC, "%s: ERROR", p_adapter_id_str);
				return -1;
			}

			nthw_rpf_init(p_nthw_rpf, p_fpga, p_adapter_info->adapter_no);
			p_nt4ga_stat->mp_nthw_rpf = p_nthw_rpf;

		} else {
			p_nt4ga_stat->mp_nthw_rpf = NULL;
		}

		p_nt4ga_stat->mp_nthw_stat = p_nthw_stat;
		nthw_stat_init(p_nthw_stat, p_fpga, 0);

		p_nt4ga_stat->mn_rx_host_buffers = p_nthw_stat->m_nb_rx_host_buffers;
		p_nt4ga_stat->mn_tx_host_buffers = p_nthw_stat->m_nb_tx_host_buffers;

		p_nt4ga_stat->mn_rx_ports = p_nthw_stat->m_nb_rx_ports;
		p_nt4ga_stat->mn_tx_ports = p_nthw_stat->m_nb_tx_ports;
	}

	return 0;
}

static int nt4ga_stat_setup(struct adapter_info_s *p_adapter_info)
{
	const int n_physical_adapter_no = p_adapter_info->adapter_no;
	(void)n_physical_adapter_no;
	nt4ga_stat_t *p_nt4ga_stat = &p_adapter_info->nt4ga_stat;
	nthw_stat_t *p_nthw_stat = p_nt4ga_stat->mp_nthw_stat;

	if (p_nt4ga_stat->mp_nthw_rmc)
		nthw_rmc_block(p_nt4ga_stat->mp_nthw_rmc);

	if (p_nt4ga_stat->mp_nthw_rpf)
		nthw_rpf_block(p_nt4ga_stat->mp_nthw_rpf);

	/* Allocate and map memory for fpga statistics */
	{
		uint32_t n_stat_size = (uint32_t)(p_nthw_stat->m_nb_counters * sizeof(uint32_t) +
				sizeof(p_nthw_stat->mp_timestamp));
		struct nt_dma_s *p_dma;
		int numa_node = p_adapter_info->fpga_info.numa_node;

		/* FPGA needs a 16K alignment on Statistics */
		p_dma = nt_dma_alloc(n_stat_size, 0x4000, numa_node);

		if (!p_dma) {
			NT_LOG_DBGX(ERR, NTNIC, "p_dma alloc failed");
			return -1;
		}

		NT_LOG_DBGX(DBG, NTNIC, "%x @%d %" PRIx64 " %" PRIx64, n_stat_size, numa_node,
			p_dma->addr, p_dma->iova);

		NT_LOG(DBG, NTNIC,
			"DMA: Physical adapter %02d, PA = 0x%016" PRIX64 " DMA = 0x%016" PRIX64
			" size = 0x%" PRIX32 "",
			n_physical_adapter_no, p_dma->iova, p_dma->addr, n_stat_size);

		p_nt4ga_stat->p_stat_dma_virtual = (uint32_t *)p_dma->addr;
		p_nt4ga_stat->n_stat_size = n_stat_size;
		p_nt4ga_stat->p_stat_dma = p_dma;

		memset(p_nt4ga_stat->p_stat_dma_virtual, 0xaa, n_stat_size);
		nthw_stat_set_dma_address(p_nthw_stat, p_dma->iova,
			p_nt4ga_stat->p_stat_dma_virtual);
	}

	if (p_nt4ga_stat->mp_nthw_rmc)
		nthw_rmc_unblock(p_nt4ga_stat->mp_nthw_rmc, false);

	if (p_nt4ga_stat->mp_nthw_rpf)
		nthw_rpf_unblock(p_nt4ga_stat->mp_nthw_rpf);

	p_nt4ga_stat->mp_stat_structs_color =
		calloc(p_nthw_stat->m_nb_color_counters, sizeof(struct color_counters));

	if (!p_nt4ga_stat->mp_stat_structs_color) {
		NT_LOG_DBGX(ERR, GENERAL, "Cannot allocate mem.");
		return -1;
	}

	p_nt4ga_stat->mp_stat_structs_hb =
		calloc(p_nt4ga_stat->mn_rx_host_buffers + p_nt4ga_stat->mn_tx_host_buffers,
			sizeof(struct host_buffer_counters));

	if (!p_nt4ga_stat->mp_stat_structs_hb) {
		NT_LOG_DBGX(ERR, GENERAL, "Cannot allocate mem.");
		return -1;
	}

	p_nt4ga_stat->cap.mp_stat_structs_port_rx =
		calloc(NUM_ADAPTER_PORTS_MAX, sizeof(struct port_counters_v2));

	if (!p_nt4ga_stat->cap.mp_stat_structs_port_rx) {
		NT_LOG_DBGX(ERR, GENERAL, "Cannot allocate mem.");
		return -1;
	}

	p_nt4ga_stat->cap.mp_stat_structs_port_tx =
		calloc(NUM_ADAPTER_PORTS_MAX, sizeof(struct port_counters_v2));

	if (!p_nt4ga_stat->cap.mp_stat_structs_port_tx) {
		NT_LOG_DBGX(ERR, GENERAL, "Cannot allocate mem.");
		return -1;
	}

	p_nt4ga_stat->mp_port_load =
		calloc(NUM_ADAPTER_PORTS_MAX, sizeof(struct port_load_counters));

	if (!p_nt4ga_stat->mp_port_load) {
		NT_LOG_DBGX(ERR, GENERAL, "Cannot allocate mem.");
		return -1;
	}

#ifdef NIM_TRIGGER
	uint64_t max_bps_speed = nt_get_max_link_speed(p_adapter_info->nt4ga_link.speed_capa);

	if (max_bps_speed == 0)
		max_bps_speed = DEFAULT_MAX_BPS_SPEED;

#else
	uint64_t max_bps_speed = DEFAULT_MAX_BPS_SPEED;
	NT_LOG(ERR, NTNIC, "NIM module not included");
#endif

	for (int p = 0; p < NUM_ADAPTER_PORTS_MAX; p++) {
		p_nt4ga_stat->mp_port_load[p].rx_bps_max = max_bps_speed;
		p_nt4ga_stat->mp_port_load[p].tx_bps_max = max_bps_speed;
		p_nt4ga_stat->mp_port_load[p].rx_pps_max = max_bps_speed / (8 * (20 + 64));
		p_nt4ga_stat->mp_port_load[p].tx_pps_max = max_bps_speed / (8 * (20 + 64));
	}

	memset(p_nt4ga_stat->a_stat_structs_color_base, 0,
		sizeof(struct color_counters) * NT_MAX_COLOR_FLOW_STATS);
	p_nt4ga_stat->last_timestamp = 0;

	nthw_stat_trigger(p_nthw_stat);

	return 0;
}

static struct nt4ga_stat_ops ops = {
	.nt4ga_stat_init = nt4ga_stat_init,
	.nt4ga_stat_setup = nt4ga_stat_setup,
};

void nt4ga_stat_ops_init(void)
{
	NT_LOG_DBGX(DBG, NTNIC, "Stat module was initialized");
	register_nt4ga_stat_ops(&ops);
}
