/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_qsl.h"

void nthw_qsl_set_debug_mode(struct qsl_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_qsl, n_debug_mode);
}

struct qsl_nthw *nthw_qsl_new(void)
{
	struct qsl_nthw *p = malloc(sizeof(struct qsl_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_qsl_delete(struct qsl_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_qsl_init(struct qsl_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_QSL, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: QSL %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_qsl = p_mod;

	/* RCP */
	p->mp_rcp_ctrl = nthw_module_get_register(p->m_qsl, QSL_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, QSL_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, QSL_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_qsl, QSL_RCP_DATA);
	p->mp_rcp_data_discard = nthw_register_get_field(p->mp_rcp_data, QSL_RCP_DATA_DISCARD);
	p->mp_rcp_data_drop = nthw_register_get_field(p->mp_rcp_data, QSL_RCP_DATA_DROP);
	p->mp_rcp_data_tbl_lo = nthw_register_get_field(p->mp_rcp_data, QSL_RCP_DATA_TBL_LO);
	p->mp_rcp_data_tbl_hi = nthw_register_get_field(p->mp_rcp_data, QSL_RCP_DATA_TBL_HI);
	p->mp_rcp_data_tbl_idx = nthw_register_get_field(p->mp_rcp_data, QSL_RCP_DATA_TBL_IDX);
	p->mp_rcp_data_tbl_msk = nthw_register_get_field(p->mp_rcp_data, QSL_RCP_DATA_TBL_MSK);
	p->mp_rcp_data_cao = nthw_register_query_field(p->mp_rcp_data, QSL_RCP_DATA_CAO);
	p->mp_rcp_data_lr = nthw_register_query_field(p->mp_rcp_data, QSL_RCP_DATA_LR);
	p->mp_rcp_data_tsa = nthw_register_query_field(p->mp_rcp_data, QSL_RCP_DATA_TSA);
	p->mp_rcp_data_vli = nthw_register_query_field(p->mp_rcp_data, QSL_RCP_DATA_VLI);

	/* QST */
	p->mp_qst_ctrl = nthw_module_get_register(p->m_qsl, QSL_QST_CTRL);
	p->mp_qst_addr = nthw_register_get_field(p->mp_qst_ctrl, QSL_QST_CTRL_ADR);
	p->mp_qst_cnt = nthw_register_get_field(p->mp_qst_ctrl, QSL_QST_CTRL_CNT);
	p->mp_qst_data = nthw_module_get_register(p->m_qsl, QSL_QST_DATA);
	p->mp_qst_data_queue = nthw_register_get_field(p->mp_qst_data, QSL_QST_DATA_QUEUE);
	p->mp_qst_data_en = nthw_register_query_field(p->mp_qst_data, QSL_QST_DATA_EN);
	p->mp_qst_data_tx_port = nthw_register_query_field(p->mp_qst_data, QSL_QST_DATA_TX_PORT);
	p->mp_qst_data_lre = nthw_register_query_field(p->mp_qst_data, QSL_QST_DATA_LRE);
	p->mp_qst_data_tci = nthw_register_query_field(p->mp_qst_data, QSL_QST_DATA_TCI);
	p->mp_qst_data_ven = nthw_register_query_field(p->mp_qst_data, QSL_QST_DATA_VEN);
	/* QEN */
	p->mp_qen_ctrl = nthw_module_get_register(p->m_qsl, QSL_QEN_CTRL);
	p->mp_qen_addr = nthw_register_get_field(p->mp_qen_ctrl, QSL_QEN_CTRL_ADR);
	p->mp_qen_cnt = nthw_register_get_field(p->mp_qen_ctrl, QSL_QEN_CTRL_CNT);
	p->mp_qen_data = nthw_module_get_register(p->m_qsl, QSL_QEN_DATA);
	p->mp_qen_data_en = nthw_register_get_field(p->mp_qen_data, QSL_QEN_DATA_EN);
	/* UNMQ */
	p->mp_unmq_ctrl = nthw_module_get_register(p->m_qsl, QSL_UNMQ_CTRL);
	p->mp_unmq_addr = nthw_register_get_field(p->mp_unmq_ctrl, QSL_UNMQ_CTRL_ADR);
	p->mp_unmq_cnt = nthw_register_get_field(p->mp_unmq_ctrl, QSL_UNMQ_CTRL_CNT);
	p->mp_unmq_data = nthw_module_get_register(p->m_qsl, QSL_UNMQ_DATA);
	p->mp_unmq_data_dest_queue =
		nthw_register_get_field(p->mp_unmq_data, QSL_UNMQ_DATA_DEST_QUEUE);
	p->mp_unmq_data_en = nthw_register_get_field(p->mp_unmq_data, QSL_UNMQ_DATA_EN);

	if (!p->mp_qst_data_en) {
		/* changed name from EN to QEN in v0.7 */
		p->mp_qst_data_en = nthw_register_get_field(p->mp_qst_data, QSL_QST_DATA_QEN);
	}

	/* LTX - not there anymore from v0.7+ */
	p->mp_ltx_ctrl = nthw_module_query_register(p->m_qsl, QSL_LTX_CTRL);

	if (p->mp_ltx_ctrl) {
		p->mp_ltx_addr = nthw_register_get_field(p->mp_ltx_ctrl, QSL_LTX_CTRL_ADR);
		p->mp_ltx_cnt = nthw_register_get_field(p->mp_ltx_ctrl, QSL_LTX_CTRL_CNT);

	} else {
		p->mp_ltx_addr = NULL;
		p->mp_ltx_cnt = NULL;
	}

	p->mp_ltx_data = nthw_module_query_register(p->m_qsl, QSL_LTX_DATA);

	if (p->mp_ltx_data) {
		p->mp_ltx_data_lr = nthw_register_get_field(p->mp_ltx_data, QSL_LTX_DATA_LR);
		p->mp_ltx_data_tx_port =
			nthw_register_get_field(p->mp_ltx_data, QSL_LTX_DATA_TX_PORT);
		p->mp_ltx_data_tsa = nthw_register_get_field(p->mp_ltx_data, QSL_LTX_DATA_TSA);

	} else {
		p->mp_ltx_data_lr = NULL;
		p->mp_ltx_data_tx_port = NULL;
		p->mp_ltx_data_tsa = NULL;
	}

	return 0;
}

/* RCP */
void nthw_qsl_rcp_select(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_addr, val);
};

void nthw_qsl_rcp_cnt(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_cnt, val);
}

void nthw_qsl_rcp_discard(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_discard, val);
}

void nthw_qsl_rcp_drop(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_drop, val);
}

void nthw_qsl_rcp_tbl_lo(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tbl_lo, val);
}
void nthw_qsl_rcp_tbl_hi(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tbl_hi, val);
}
void nthw_qsl_rcp_tbl_idx(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tbl_idx, val);
}

void nthw_qsl_rcp_tbl_msk(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tbl_msk, val);
}

void nthw_qsl_rcp_lr(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_rcp_data_lr)
		nthw_field_set_val32(p->mp_rcp_data_lr, val);
}

void nthw_qsl_rcp_tsa(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_rcp_data_tsa)
		nthw_field_set_val32(p->mp_rcp_data_tsa, val);
}

void nthw_qsl_rcp_vli(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_rcp_data_vli)
		nthw_field_set_val32(p->mp_rcp_data_vli, val);
}

void nthw_qsl_rcp_flush(const struct qsl_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}

/* QST */
void nthw_qsl_qst_select(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qst_addr, val);
}

void nthw_qsl_qst_cnt(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qst_cnt, val);
}

void nthw_qsl_qst_queue(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qst_data_queue, val);
}

void nthw_qsl_qst_en(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qst_data_en, val);
}

void nthw_qsl_qst_tx_port(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_qst_data_tx_port)
		nthw_field_set_val32(p->mp_qst_data_tx_port, val);
}

void nthw_qsl_qst_lre(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_qst_data_lre)
		nthw_field_set_val32(p->mp_qst_data_lre, val);
}

void nthw_qsl_qst_tci(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_qst_data_tci)
		nthw_field_set_val32(p->mp_qst_data_tci, val);
}

void nthw_qsl_qst_ven(const struct qsl_nthw *p, uint32_t val)
{
	if (p->mp_qst_data_ven)
		nthw_field_set_val32(p->mp_qst_data_ven, val);
}

void nthw_qsl_qst_flush(const struct qsl_nthw *p)
{
	nthw_register_flush(p->mp_qst_ctrl, 1);
	nthw_register_flush(p->mp_qst_data, 1);
}

/* QEN */
void nthw_qsl_qen_select(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qen_addr, val);
}

void nthw_qsl_qen_cnt(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qen_cnt, val);
}

void nthw_qsl_qen_en(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_qen_data_en, val);
}

void nthw_qsl_qen_flush(const struct qsl_nthw *p)
{
	nthw_register_flush(p->mp_qen_ctrl, 1);
	nthw_register_flush(p->mp_qen_data, 1);
}

/* UNMQ */
void nthw_qsl_unmq_select(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_unmq_addr, val);
}

void nthw_qsl_unmq_cnt(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_unmq_cnt, val);
}

void nthw_qsl_unmq_dest_queue(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_unmq_data_dest_queue, val);
}

void nthw_qsl_unmq_en(const struct qsl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_unmq_data_en, val);
}

void nthw_qsl_unmq_flush(const struct qsl_nthw *p)
{
	nthw_register_flush(p->mp_unmq_ctrl, 1);
	nthw_register_flush(p->mp_unmq_data, 1);
}
