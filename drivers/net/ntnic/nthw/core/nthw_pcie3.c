/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_pcie3.h"

#define NTHW_TG_REF_FREQ (250000000ULL)

nthw_pcie3_t *nthw_pcie3_new(void)
{
	nthw_pcie3_t *p = malloc(sizeof(nthw_pcie3_t));

	if (p)
		memset(p, 0, sizeof(nthw_pcie3_t));

	return p;
}

void nthw_pcie3_delete(nthw_pcie3_t *p)
{
	free(p);
}

int nthw_pcie3_init(nthw_pcie3_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_PCIE3, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: PCIE3 %d: no such instance",
			p_fpga->p_fpga_info->mp_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_pcie3 = mod;

	/* PCIe3 */
	p->mp_reg_stat_ctrl = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STAT_CTRL);
	p->mp_fld_stat_ctrl_ena =
		nthw_register_get_field(p->mp_reg_stat_ctrl, PCIE3_STAT_CTRL_STAT_ENA);
	p->mp_fld_stat_ctrl_req =
		nthw_register_get_field(p->mp_reg_stat_ctrl, PCIE3_STAT_CTRL_STAT_REQ);

	p->mp_reg_stat_rx = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STAT_RX);
	p->mp_fld_stat_rx_counter =
		nthw_register_get_field(p->mp_reg_stat_rx, PCIE3_STAT_RX_COUNTER);

	p->mp_reg_stat_tx = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STAT_TX);
	p->mp_fld_stat_tx_counter =
		nthw_register_get_field(p->mp_reg_stat_tx, PCIE3_STAT_TX_COUNTER);

	p->mp_reg_stat_ref_clk = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STAT_REFCLK);
	p->mp_fld_stat_ref_clk_ref_clk =
		nthw_register_get_field(p->mp_reg_stat_ref_clk, PCIE3_STAT_REFCLK_REFCLK250);

	p->mp_reg_stat_rq_rdy = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STAT_RQ_RDY);
	p->mp_fld_stat_rq_rdy_counter =
		nthw_register_get_field(p->mp_reg_stat_rq_rdy, PCIE3_STAT_RQ_RDY_COUNTER);

	p->mp_reg_stat_rq_vld = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STAT_RQ_VLD);
	p->mp_fld_stat_rq_vld_counter =
		nthw_register_get_field(p->mp_reg_stat_rq_vld, PCIE3_STAT_RQ_VLD_COUNTER);

	p->mp_reg_status0 = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_STATUS0);
	p->mp_fld_status0_tags_in_use =
		nthw_register_get_field(p->mp_reg_status0, PCIE3_STATUS0_TAGS_IN_USE);

	p->mp_reg_rp_to_ep_err = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_RP_TO_EP_ERR);
	p->mp_fld_rp_to_ep_err_cor =
		nthw_register_get_field(p->mp_reg_rp_to_ep_err, PCIE3_RP_TO_EP_ERR_ERR_COR);
	p->mp_fld_rp_to_ep_err_non_fatal =
		nthw_register_get_field(p->mp_reg_rp_to_ep_err, PCIE3_RP_TO_EP_ERR_ERR_NONFATAL);
	p->mp_fld_rp_to_ep_err_fatal =
		nthw_register_get_field(p->mp_reg_rp_to_ep_err, PCIE3_RP_TO_EP_ERR_ERR_FATAL);

	p->mp_reg_ep_to_rp_err = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_EP_TO_RP_ERR);
	p->mp_fld_ep_to_rp_err_cor =
		nthw_register_get_field(p->mp_reg_ep_to_rp_err, PCIE3_EP_TO_RP_ERR_ERR_COR);
	p->mp_fld_ep_to_rp_err_non_fatal =
		nthw_register_get_field(p->mp_reg_ep_to_rp_err, PCIE3_EP_TO_RP_ERR_ERR_NONFATAL);
	p->mp_fld_ep_to_rp_err_fatal =
		nthw_register_get_field(p->mp_reg_ep_to_rp_err, PCIE3_EP_TO_RP_ERR_ERR_FATAL);

	p->mp_reg_sample_time = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_SAMPLE_TIME);
	p->mp_fld_sample_time =
		nthw_register_get_field(p->mp_reg_sample_time, PCIE3_SAMPLE_TIME_SAMPLE_TIME);

	p->mp_reg_pci_end_point = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_PCI_ENDPOINT);
	p->mp_fld_pci_end_point_if_id =
		nthw_register_get_field(p->mp_reg_pci_end_point, PCIE3_PCI_ENDPOINT_IF_ID);
	p->mp_fld_pci_end_point_send_msg =
		nthw_register_get_field(p->mp_reg_pci_end_point, PCIE3_PCI_ENDPOINT_SEND_MSG);
	p->mp_fld_pci_end_point_get_msg =
		nthw_register_get_field(p->mp_reg_pci_end_point, PCIE3_PCI_ENDPOINT_GET_MSG);
	p->mp_fld_pci_end_point_dmaep0_allow_mask =
		nthw_register_get_field(p->mp_reg_pci_end_point,
			PCIE3_PCI_ENDPOINT_DMA_EP0_ALLOW_MASK);
	p->mp_fld_pci_end_point_dmaep1_allow_mask =
		nthw_register_get_field(p->mp_reg_pci_end_point,
			PCIE3_PCI_ENDPOINT_DMA_EP1_ALLOW_MASK);

	if (p->mp_reg_pci_end_point)
		nthw_register_update(p->mp_reg_pci_end_point);

	p->mp_reg_pci_test0 = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_PCI_TEST0);
	p->mp_fld_pci_test0 = nthw_register_get_field(p->mp_reg_pci_test0, PCIE3_PCI_TEST0_DATA);

	if (p->mp_reg_pci_test0)
		nthw_register_update(p->mp_reg_pci_test0);

	p->mp_reg_pci_test1 = nthw_module_get_register(p->mp_mod_pcie3, PCIE3_PCI_TEST1);
	p->mp_fld_pci_test1 = nthw_register_get_field(p->mp_reg_pci_test1, PCIE3_PCI_TEST1_DATA);

	if (p->mp_reg_pci_test1)
		nthw_register_update(p->mp_reg_pci_test1);

	p->mp_reg_pci_e3_mark_adr_lsb =
		nthw_module_get_register(p->mp_mod_pcie3, PCIE3_MARKADR_LSB);
	p->mp_fld_pci_e3_mark_adr_lsb_adr =
		nthw_register_get_field(p->mp_reg_pci_e3_mark_adr_lsb, PCIE3_MARKADR_LSB_ADR);

	if (p->mp_reg_pci_e3_mark_adr_lsb)
		nthw_register_update(p->mp_reg_pci_e3_mark_adr_lsb);

	p->mp_reg_pci_e3_mark_adr_msb =
		nthw_module_get_register(p->mp_mod_pcie3, PCIE3_MARKADR_MSB);
	p->mp_fld_pci_e3_mark_adr_msb_adr =
		nthw_register_get_field(p->mp_reg_pci_e3_mark_adr_msb, PCIE3_MARKADR_MSB_ADR);

	if (p->mp_reg_pci_e3_mark_adr_msb)
		nthw_register_update(p->mp_reg_pci_e3_mark_adr_msb);

	/* Initial setup - disable markerscheme and bifurcation */
	if (p->mp_fld_pci_end_point_dmaep0_allow_mask)
		nthw_field_clr_flush(p->mp_fld_pci_end_point_dmaep0_allow_mask);

	if (p->mp_fld_pci_end_point_dmaep1_allow_mask)
		nthw_field_clr_flush(p->mp_fld_pci_end_point_dmaep1_allow_mask);

	if (p->mp_fld_pci_e3_mark_adr_lsb_adr)
		nthw_field_set_val_flush32(p->mp_fld_pci_e3_mark_adr_lsb_adr, 0UL);

	if (p->mp_fld_pci_e3_mark_adr_msb_adr)
		nthw_field_set_val_flush32(p->mp_fld_pci_e3_mark_adr_msb_adr, 0UL);

	if (p->mp_fld_pci_end_point_dmaep0_allow_mask)
		nthw_field_set_flush(p->mp_fld_pci_end_point_dmaep0_allow_mask);

	if (p->mp_fld_pci_end_point_dmaep1_allow_mask)
		nthw_field_clr_flush(p->mp_fld_pci_end_point_dmaep1_allow_mask);

	return 0;
};

int nthw_pcie3_trigger_sample_time(nthw_pcie3_t *p)
{
	nthw_field_set_val_flush32(p->mp_fld_sample_time, 0xfee1dead);

	return 0;
}

static int nthw_pcie3_stat_req_enable(nthw_pcie3_t *p)
{
	nthw_field_set_all(p->mp_fld_stat_ctrl_ena);
	nthw_field_set_all(p->mp_fld_stat_ctrl_req);
	nthw_field_flush_register(p->mp_fld_stat_ctrl_req);
	return 0;
}

static int nthw_pcie3_stat_req_disable(nthw_pcie3_t *p)
{
	nthw_field_clr_all(p->mp_fld_stat_ctrl_ena);
	nthw_field_set_all(p->mp_fld_stat_ctrl_req);
	nthw_field_flush_register(p->mp_fld_stat_ctrl_req);
	return 0;
}

static int nthw_pcie3_get_stat(nthw_pcie3_t *p, uint32_t *p_rx_cnt, uint32_t *p_tx_cnt,
	uint32_t *p_ref_clk_cnt, uint32_t *p_tg_unit_size, uint32_t *p_tg_ref_freq,
	uint32_t *p_tag_use_cnt, uint32_t *p_rq_rdy_cnt, uint32_t *p_rq_vld_cnt)
{
	*p_rx_cnt = nthw_field_get_updated(p->mp_fld_stat_rx_counter);
	*p_tx_cnt = nthw_field_get_updated(p->mp_fld_stat_tx_counter);

	*p_ref_clk_cnt = nthw_field_get_updated(p->mp_fld_stat_ref_clk_ref_clk);

	*p_tg_unit_size = NTHW_TG_CNT_SIZE;
	*p_tg_ref_freq = NTHW_TG_REF_FREQ;

	*p_tag_use_cnt = nthw_field_get_updated(p->mp_fld_status0_tags_in_use);

	*p_rq_rdy_cnt = nthw_field_get_updated(p->mp_fld_stat_rq_rdy_counter);
	*p_rq_vld_cnt = nthw_field_get_updated(p->mp_fld_stat_rq_vld_counter);

	return 0;
}

static int nthw_pcie3_get_stat_rate(nthw_pcie3_t *p, uint64_t *p_pci_rx_rate,
	uint64_t *p_pci_tx_rate,
	uint64_t *p_ref_clk_cnt, uint64_t *p_tag_use_cnt,
	uint64_t *p_pci_nt_bus_util, uint64_t *p_pci_xil_bus_util)
{
	uint32_t rx_cnt, tx_cnt, ref_clk_cnt;
	uint32_t tg_unit_size, tg_ref_freq;
	uint32_t tag_use_cnt, rq_rdy_cnt, rq_vld_cnt;

	nthw_pcie3_get_stat(p, &rx_cnt, &tx_cnt, &ref_clk_cnt, &tg_unit_size, &tg_ref_freq,
		&tag_use_cnt, &rq_rdy_cnt, &rq_vld_cnt);

	if (ref_clk_cnt) {
		uint64_t nt_bus_util, xil_bus_util;
		uint64_t rx_rate, tx_rate;

		rx_rate = ((uint64_t)rx_cnt * tg_unit_size * tg_ref_freq) / (uint64_t)ref_clk_cnt;
		*p_pci_rx_rate = rx_rate;

		tx_rate = ((uint64_t)tx_cnt * tg_unit_size * tg_ref_freq) / (uint64_t)ref_clk_cnt;
		*p_pci_tx_rate = tx_rate;

		*p_ref_clk_cnt = ref_clk_cnt;

		*p_tag_use_cnt = tag_use_cnt;

		nt_bus_util = ((uint64_t)rq_vld_cnt * 1000000ULL) / (uint64_t)ref_clk_cnt;
		*p_pci_nt_bus_util = nt_bus_util;
		xil_bus_util = ((uint64_t)rq_rdy_cnt * 1000000ULL) / (uint64_t)ref_clk_cnt;
		*p_pci_xil_bus_util = xil_bus_util;

	} else {
		*p_ref_clk_cnt = 0;
		*p_pci_nt_bus_util = 0;
		*p_pci_xil_bus_util = 0;
	}

	return 0;
}

static int nthw_pcie3_end_point_counters_sample_post(nthw_pcie3_t *p,
	struct nthw_hif_end_point_counters *epc)
{
	NT_LOG_DBGX(DBG, NTHW);
	RTE_ASSERT(epc);
	nthw_pcie3_get_stat_rate(p, &epc->cur_tx, &epc->cur_rx, &epc->n_ref_clk_cnt,
		&epc->n_tags_in_use, &epc->cur_pci_nt_util,
		&epc->cur_pci_xil_util);
	return 0;
}
