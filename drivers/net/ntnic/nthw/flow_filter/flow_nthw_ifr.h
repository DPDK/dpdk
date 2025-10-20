/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_IFR_H__
#define __FLOW_NTHW_IFR_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct ifr_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_ifr;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;

	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_ipv4_en;
	nthw_field_t *mp_rcp_data_ipv6_en;
	nthw_field_t *mp_rcp_data_mtu;
	nthw_field_t *mp_rcp_data_ipv4_df_drop;
	nthw_field_t *mp_rcp_data_ipv6_drop;

	nthw_register_t *mp_df_buf_ctrl;
	nthw_field_t *mp_df_buf_ctrl_available;
	nthw_field_t *mp_df_buf_ctrl_mtu_profile;

	nthw_register_t *mp_df_buf_data;
	nthw_field_t *mp_df_buf_data_fifo_dat;

	nthw_register_t *mp_counters_ctrl;
	nthw_field_t *mp_counters_addr;
	nthw_field_t *mp_counters_cnt;

	nthw_register_t *mp_counters_data;
	nthw_field_t *mp_counters_drop;
};

struct ifr_nthw *nthw_ifr_new(void);
int nthw_ifr_init(struct ifr_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int ifr_nthw_setup(struct ifr_nthw *p, int n_idx, int n_idx_cnt);
void nthw_ifr_set_debug_mode(struct ifr_nthw *p, unsigned int n_debug_mode);

/* IFR RCP */
void nthw_ifr_rcp_select(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_cnt(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_ipv4_en(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_ipv4_df_drop(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_ipv6_en(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_ipv6_drop(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_mtu(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_rcp_flush(const struct ifr_nthw *p);

/* IFR Counters */
void nthw_ifr_counters_select(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_counters_cnt(const struct ifr_nthw *p, uint32_t val);
void nthw_ifr_counters_drop(const struct ifr_nthw *p, uint32_t *val, int get);
void nthw_ifr_counters_update(const struct ifr_nthw *p);

#endif	/* __FLOW_NTHW_IFR_H__ */
