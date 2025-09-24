/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_QSL_H__
#define __FLOW_NTHW_QSL_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct qsl_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_qsl;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;
	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_discard;
	nthw_field_t *mp_rcp_data_drop;
	nthw_field_t *mp_rcp_data_tbl_lo;
	nthw_field_t *mp_rcp_data_tbl_hi;
	nthw_field_t *mp_rcp_data_tbl_idx;
	nthw_field_t *mp_rcp_data_tbl_msk;
	nthw_field_t *mp_rcp_data_cao;
	nthw_field_t *mp_rcp_data_lr;
	nthw_field_t *mp_rcp_data_tsa;
	nthw_field_t *mp_rcp_data_vli;

	nthw_register_t *mp_ltx_ctrl;
	nthw_field_t *mp_ltx_addr;
	nthw_field_t *mp_ltx_cnt;
	nthw_register_t *mp_ltx_data;
	nthw_field_t *mp_ltx_data_lr;
	nthw_field_t *mp_ltx_data_tx_port;
	nthw_field_t *mp_ltx_data_tsa;

	nthw_register_t *mp_qst_ctrl;
	nthw_field_t *mp_qst_addr;
	nthw_field_t *mp_qst_cnt;
	nthw_register_t *mp_qst_data;
	nthw_field_t *mp_qst_data_queue;
	nthw_field_t *mp_qst_data_en;
	nthw_field_t *mp_qst_data_tx_port;
	nthw_field_t *mp_qst_data_lre;
	nthw_field_t *mp_qst_data_tci;
	nthw_field_t *mp_qst_data_ven;

	nthw_register_t *mp_qen_ctrl;
	nthw_field_t *mp_qen_addr;
	nthw_field_t *mp_qen_cnt;
	nthw_register_t *mp_qen_data;
	nthw_field_t *mp_qen_data_en;

	nthw_register_t *mp_unmq_ctrl;
	nthw_field_t *mp_unmq_addr;
	nthw_field_t *mp_unmq_cnt;
	nthw_register_t *mp_unmq_data;
	nthw_field_t *mp_unmq_data_dest_queue;
	nthw_field_t *mp_unmq_data_en;
};

typedef struct qsl_nthw qsl_nthw_t;

struct qsl_nthw *nthw_qsl_new(void);
void nthw_qsl_delete(struct qsl_nthw *p);
int nthw_qsl_init(struct qsl_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

void nthw_qsl_set_debug_mode(struct qsl_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_qsl_rcp_select(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_cnt(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_discard(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_drop(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_tbl_lo(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_tbl_hi(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_tbl_idx(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_tbl_msk(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_lr(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_tsa(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_vli(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_rcp_flush(const struct qsl_nthw *p);

/* QST */
void nthw_qsl_qst_select(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_cnt(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_queue(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_en(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_tx_port(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_lre(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_tci(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_ven(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qst_flush(const struct qsl_nthw *p);

/* QEN */
void nthw_qsl_qen_select(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qen_cnt(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qen_en(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_qen_flush(const struct qsl_nthw *p);

/* UNMQ */
void nthw_qsl_unmq_select(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_unmq_cnt(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_unmq_dest_queue(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_unmq_en(const struct qsl_nthw *p, uint32_t val);
void nthw_qsl_unmq_flush(const struct qsl_nthw *p);

#endif	/* __FLOW_NTHW_QSL_H__ */
