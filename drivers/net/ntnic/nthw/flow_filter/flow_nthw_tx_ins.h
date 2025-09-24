/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_TX_INS_H__
#define __FLOW_NTHW_TX_INS_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct tx_ins_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_tx_ins;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;

	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_dyn;
	nthw_field_t *mp_rcp_data_ofs;
	nthw_field_t *mp_rcp_data_len;
};

struct tx_ins_nthw *nthw_tx_ins_new(void);
void nthw_tx_ins_delete(struct tx_ins_nthw *p);
int nthw_tx_ins_init(struct tx_ins_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int tx_ins_nthw_setup(struct tx_ins_nthw *p, int n_idx, int n_idx_cnt);
void nthw_tx_ins_set_debug_mode(struct tx_ins_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_tx_ins_rcp_select(const struct tx_ins_nthw *p, uint32_t val);
void nthw_tx_ins_rcp_cnt(const struct tx_ins_nthw *p, uint32_t val);
void nthw_tx_ins_rcp_dyn(const struct tx_ins_nthw *p, uint32_t val);
void nthw_tx_ins_rcp_ofs(const struct tx_ins_nthw *p, uint32_t val);
void nthw_tx_ins_rcp_len(const struct tx_ins_nthw *p, uint32_t val);
void nthw_tx_ins_rcp_flush(const struct tx_ins_nthw *p);

#endif	/* __FLOW_NTHW_TX_INS_H__ */
