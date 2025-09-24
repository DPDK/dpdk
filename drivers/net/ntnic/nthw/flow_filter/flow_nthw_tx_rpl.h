/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_TX_RPL_H__
#define __FLOW_NTHW_TX_RPL_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct tx_rpl_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_tx_rpl;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_ctrl_addr;
	nthw_field_t *mp_rcp_ctrl_cnt;

	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_dyn;
	nthw_field_t *mp_rcp_data_ofs;
	nthw_field_t *mp_rcp_data_len;
	nthw_field_t *mp_rcp_data_rpl_ptr;
	nthw_field_t *mp_rcp_data_ext_prio;
	nthw_field_t *mp_rcp_data_eth_type_wr;

	nthw_register_t *mp_ext_ctrl;
	nthw_field_t *mp_ext_ctrl_addr;
	nthw_field_t *mp_ext_ctrl_cnt;

	nthw_register_t *mp_ext_data;
	nthw_field_t *mp_ext_data_rpl_ptr;

	nthw_register_t *mp_rpl_ctrl;
	nthw_field_t *mp_rpl_ctrl_addr;
	nthw_field_t *mp_rpl_ctrl_cnt;

	nthw_register_t *mp_rpl_data;
	nthw_field_t *mp_rpl_data_value;
};

struct tx_rpl_nthw *nthw_tx_rpl_new(void);
void nthw_tx_rpl_delete(struct tx_rpl_nthw *p);
int nthw_tx_rpl_init(struct tx_rpl_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int tx_rpl_nthw_setup(struct tx_rpl_nthw *p, int n_idx, int n_idx_cnt);
void nthw_tx_rpl_set_debug_mode(struct tx_rpl_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_tx_rpl_rcp_select(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_cnt(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_dyn(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_ofs(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_len(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_rpl_ptr(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_ext_prio(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_eth_type_wr(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rcp_flush(const struct tx_rpl_nthw *p);

void nthw_tx_rpl_ext_select(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_ext_cnt(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_ext_rpl_ptr(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_ext_flush(const struct tx_rpl_nthw *p);

void nthw_tx_rpl_rpl_select(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rpl_cnt(const struct tx_rpl_nthw *p, uint32_t val);
void nthw_tx_rpl_rpl_value(const struct tx_rpl_nthw *p, const uint32_t *val);
void nthw_tx_rpl_rpl_flush(const struct tx_rpl_nthw *p);

#endif	/* __FLOW_NTHW_TX_RPL_H__ */
