/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTNIC_STAT_H_
#define NTNIC_STAT_H_

#include "common_adapter_defs.h"
#include "nthw_rmc.h"
#include "nthw_fpga_model.h"

#define NT_MAX_COLOR_FLOW_STATS 0x400

struct nthw_stat {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_stat;
	int mn_instance;

	int mn_stat_layout_version;

	bool mb_has_tx_stats;

	int m_nb_phy_ports;
	int m_nb_nim_ports;

	int m_nb_rx_ports;
	int m_nb_tx_ports;

	int m_nb_rx_host_buffers;
	int m_nb_tx_host_buffers;

	int m_dbs_present;

	int m_rx_port_replicate;

	int m_nb_color_counters;

	int m_nb_rx_hb_counters;
	int m_nb_tx_hb_counters;

	int m_nb_rx_port_counters;
	int m_nb_tx_port_counters;

	int m_nb_counters;

	int m_nb_rpp_per_ps;

	nthw_field_t *mp_fld_dma_ena;
	nthw_field_t *mp_fld_cnt_clear;

	nthw_field_t *mp_fld_tx_disable;

	nthw_field_t *mp_fld_cnt_freeze;

	nthw_field_t *mp_fld_stat_toggle_missed;

	nthw_field_t *mp_fld_dma_lsb;
	nthw_field_t *mp_fld_dma_msb;

	nthw_field_t *mp_fld_load_bin;
	nthw_field_t *mp_fld_load_bps_rx0;
	nthw_field_t *mp_fld_load_bps_rx1;
	nthw_field_t *mp_fld_load_bps_tx0;
	nthw_field_t *mp_fld_load_bps_tx1;
	nthw_field_t *mp_fld_load_pps_rx0;
	nthw_field_t *mp_fld_load_pps_rx1;
	nthw_field_t *mp_fld_load_pps_tx0;
	nthw_field_t *mp_fld_load_pps_tx1;

	uint64_t m_stat_dma_physical;
	uint32_t *mp_stat_dma_virtual;

	uint64_t *mp_timestamp;
};

typedef struct nthw_stat nthw_stat_t;
typedef struct nthw_stat nthw_stat;

struct color_counters {
	uint64_t color_packets;
	uint64_t color_bytes;
	uint8_t tcp_flags;
};

struct host_buffer_counters {
};

struct port_load_counters {
	uint64_t rx_pps_max;
	uint64_t tx_pps_max;
	uint64_t rx_bps_max;
	uint64_t tx_bps_max;
};

struct port_counters_v2 {
};

struct flm_counters_v1 {
};

struct nt4ga_stat_s {
	nthw_stat_t *mp_nthw_stat;
	nthw_rmc_t *mp_nthw_rmc;
	struct nt_dma_s *p_stat_dma;
	uint32_t *p_stat_dma_virtual;
	uint32_t n_stat_size;

	uint64_t last_timestamp;

	int mn_rx_host_buffers;
	int mn_tx_host_buffers;

	int mn_rx_ports;
	int mn_tx_ports;

	struct color_counters *mp_stat_structs_color;
	/* For calculating increments between stats polls */
	struct color_counters a_stat_structs_color_base[NT_MAX_COLOR_FLOW_STATS];

	/* Port counters for inline */
	struct {
		struct port_counters_v2 *mp_stat_structs_port_rx;
		struct port_counters_v2 *mp_stat_structs_port_tx;
	} cap;

	struct host_buffer_counters *mp_stat_structs_hb;
	struct port_load_counters *mp_port_load;

	/* Rx/Tx totals: */
	uint64_t n_totals_reset_timestamp;	/* timestamp for last totals reset */

	uint64_t a_port_rx_octets_total[NUM_ADAPTER_PORTS_MAX];
	/* Base is for calculating increments between statistics reads */
	uint64_t a_port_rx_octets_base[NUM_ADAPTER_PORTS_MAX];

	uint64_t a_port_rx_packets_total[NUM_ADAPTER_PORTS_MAX];
	uint64_t a_port_rx_packets_base[NUM_ADAPTER_PORTS_MAX];

	uint64_t a_port_rx_drops_total[NUM_ADAPTER_PORTS_MAX];
	uint64_t a_port_rx_drops_base[NUM_ADAPTER_PORTS_MAX];

	uint64_t a_port_tx_octets_total[NUM_ADAPTER_PORTS_MAX];
	uint64_t a_port_tx_octets_base[NUM_ADAPTER_PORTS_MAX];

	uint64_t a_port_tx_packets_base[NUM_ADAPTER_PORTS_MAX];
	uint64_t a_port_tx_packets_total[NUM_ADAPTER_PORTS_MAX];
};

typedef struct nt4ga_stat_s nt4ga_stat_t;

nthw_stat_t *nthw_stat_new(void);
int nthw_stat_init(nthw_stat_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_stat_delete(nthw_stat_t *p);

int nthw_stat_set_dma_address(nthw_stat_t *p, uint64_t stat_dma_physical,
	uint32_t *p_stat_dma_virtual);
int nthw_stat_trigger(nthw_stat_t *p);

#endif  /* NTNIC_STAT_H_ */
