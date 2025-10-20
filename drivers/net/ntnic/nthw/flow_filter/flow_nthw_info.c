/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "nt_util.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_info.h"

static inline unsigned int clamp_one(unsigned int val)
{
	return val > 1 ? 1 : val;
}

struct info_nthw *nthw_info_new(void)
{
	struct info_nthw *p = malloc(sizeof(struct info_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_info_delete(struct info_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_info_init(struct info_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;

	unsigned int km_present = clamp_one(nthw_fpga_get_product_param(p_fpga, NT_KM_PRESENT, 0));
	unsigned int kcc_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_CAT_KCC_PRESENT, 0));
	unsigned int roa_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_ROA_PRESENT, 0));
	unsigned int dbs_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_DBS_PRESENT, 0));
	unsigned int flm_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_FLM_PRESENT, 0));
	unsigned int hst_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_HST_PRESENT, 0));

	/* Modules for Tx Packet Edit function */
	unsigned int hfu_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_HFU_PRESENT, 0));
	unsigned int tx_cpy_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_TX_CPY_PRESENT, 0));
	unsigned int tx_ins_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_TX_INS_PRESENT, 0));
	unsigned int tx_rpl_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_TX_RPL_PRESENT, 0));
	unsigned int csu_present =
		clamp_one(nthw_fpga_get_product_param(p_fpga, NT_CSU_PRESENT, 0));
	unsigned int tpe_present =
		(hfu_present && tx_cpy_present && tx_ins_present && tx_rpl_present && csu_present)
		? 1
		: 0;

	p->n_phy_ports = nthw_fpga_get_product_param(p_fpga, NT_PHY_PORTS, 0);
	p->n_rx_ports = nthw_fpga_get_product_param(p_fpga, NT_RX_PORTS, 0);
	p->n_ltx_avail = nthw_fpga_get_product_param(p_fpga, NT_LR_PRESENT, 0);
	p->nb_cat_func = nthw_fpga_get_product_param(p_fpga, NT_CAT_FUNCS, 0);
	p->nb_categories = nthw_fpga_get_product_param(p_fpga, NT_CATEGORIES, 0);
	p->nb_queues = nthw_fpga_get_product_param(p_fpga, NT_QUEUES, 0);
	p->nb_flow_types = nthw_fpga_get_product_param(p_fpga, NT_KM_FLOW_TYPES, 0) *
		clamp_one(km_present + flm_present);
	p->nb_pm_ext = nthw_fpga_get_product_param(p_fpga, NT_CAT_N_EXT, 0);
	p->nb_len = nthw_fpga_get_product_param(p_fpga, NT_CAT_N_LEN, 0);
	p->nb_kcc_size = nthw_fpga_get_product_param(p_fpga, NT_CAT_KCC_SIZE, 0) * kcc_present;
	p->nb_kcc_banks = nthw_fpga_get_product_param(p_fpga, NT_CAT_KCC_BANKS, 0) * kcc_present;
	p->nb_km_categories =
		nthw_fpga_get_product_param(p_fpga, NT_KM_CATEGORIES, 0) * km_present;
	p->nb_km_cam_banks = nthw_fpga_get_product_param(p_fpga, NT_KM_CAM_BANKS, 0) * km_present;
	p->nb_km_cam_record_words =
		nthw_fpga_get_product_param(p_fpga, NT_KM_CAM_REC_WORDS, 0) * km_present;
	p->nb_km_cam_records =
		nthw_fpga_get_product_param(p_fpga, NT_KM_CAM_RECORDS, 0) * km_present;
	p->nb_km_tcam_banks =
		nthw_fpga_get_product_param(p_fpga, NT_KM_TCAM_BANKS, 0) * km_present;
	p->nb_km_tcam_bank_width =
		nthw_fpga_get_product_param(p_fpga, NT_KM_TCAM_BANK_WIDTH, 0) * km_present;
	p->nb_flm_categories =
		nthw_fpga_get_product_param(p_fpga, NT_FLM_CATEGORIES, 0) * flm_present;
	p->nb_flm_size_mb = nthw_fpga_get_product_param(p_fpga, NT_FLM_SIZE_MB, 0);
	p->nb_flm_entry_size = nthw_fpga_get_product_param(p_fpga, NT_FLM_ENTRY_SIZE, 0);
	p->nb_flm_variant = nthw_fpga_get_product_param(p_fpga, NT_FLM_VARIANT, 0);
	p->nb_flm_prios = nthw_fpga_get_product_param(p_fpga, NT_FLM_PRIOS, 0) * flm_present;
	p->nb_flm_pst_profiles =
		nthw_fpga_get_product_param(p_fpga, NT_FLM_PST_PROFILES, 0) * flm_present;
	p->nb_flm_scrub_profiles =
		nthw_fpga_get_product_param(p_fpga, NT_FLM_SCRUB_PROFILES, 0) * flm_present;
	p->nb_flm_load_aps_max = nthw_fpga_get_product_param(p_fpga, NT_FLM_LOAD_APS_MAX, 0);
	p->nb_hst_categories =
		nthw_fpga_get_product_param(p_fpga, NT_HST_CATEGORIES, 0) * hst_present;
	p->nb_qsl_categories = nthw_fpga_get_product_param(p_fpga, NT_QSL_CATEGORIES, 0);
	p->nb_qsl_qst_entries = nthw_fpga_get_product_param(p_fpga, NT_QSL_QST_SIZE, 0);
	p->nb_pdb_categories = nthw_fpga_get_product_param(p_fpga, NT_PDB_CATEGORIES, 0);
	p->nb_roa_categories =
		nthw_fpga_get_product_param(p_fpga, NT_ROA_CATEGORIES, 0) * roa_present;
	p->nb_dbs_categories = min(nthw_fpga_get_product_param(p_fpga, NT_DBS_RX_QUEUES, 0),
			nthw_fpga_get_product_param(p_fpga, NT_DBS_TX_QUEUES, 0)) *
		dbs_present;
	p->nb_cat_km_if_cnt =
		nthw_fpga_get_product_param(p_fpga, NT_CAT_KM_IF_CNT, km_present + flm_present);
	p->m_cat_km_if_m0 = nthw_fpga_get_product_param(p_fpga, NT_CAT_KM_IF_M0, -1);
	p->m_cat_km_if_m1 = nthw_fpga_get_product_param(p_fpga, NT_CAT_KM_IF_M1, -1);
	p->nb_tpe_categories =
		nthw_fpga_get_product_param(p_fpga, NT_TPE_CATEGORIES, 0) * tpe_present;
	p->nb_tx_cpy_writers =
		nthw_fpga_get_product_param(p_fpga, NT_TX_CPY_WRITERS, 0) * tpe_present;
	p->nb_tx_cpy_mask_mem =
		nthw_fpga_get_product_param(p_fpga, NT_CPY_MASK_MEM, 0) * tpe_present;
	p->nb_tx_rpl_depth = nthw_fpga_get_product_param(p_fpga, NT_TX_RPL_DEPTH, 0) * tpe_present;
	p->nb_tx_rpl_ext_categories =
		nthw_fpga_get_product_param(p_fpga, NT_TX_RPL_EXT_CATEGORIES, 0) * tpe_present;
	p->nb_tpe_ifr_categories = nthw_fpga_get_product_param(p_fpga, NT_TX_MTU_PROFILE_IFR, 0);
	p->nb_rpp_per_ps = nthw_fpga_get_product_param(p_fpga, NT_RPP_PER_PS, 0);
	p->nb_hsh_categories = nthw_fpga_get_product_param(p_fpga, NT_HSH_CATEGORIES, 0);
	p->nb_hsh_toeplitz = nthw_fpga_get_product_param(p_fpga, NT_HSH_TOEPLITZ, 0);
	return 0;
}

unsigned int nthw_info_get_nb_phy_ports(const struct info_nthw *p)
{
	return p->n_phy_ports;
}

unsigned int nthw_info_get_nb_rx_ports(const struct info_nthw *p)
{
	return p->n_rx_ports;
}

unsigned int nthw_info_get_ltx_avail(const struct info_nthw *p)
{
	return p->n_ltx_avail;
}

unsigned int nthw_info_get_nb_categories(const struct info_nthw *p)
{
	return p->nb_categories;
}

unsigned int nthw_info_get_kcc_size(const struct info_nthw *p)
{
	return p->nb_kcc_size;
}

unsigned int nthw_info_get_kcc_banks(const struct info_nthw *p)
{
	return p->nb_kcc_banks;
}

unsigned int nthw_info_get_nb_queues(const struct info_nthw *p)
{
	return p->nb_queues;
}

unsigned int nthw_info_get_nb_cat_funcs(const struct info_nthw *p)
{
	return p->nb_cat_func;
}

unsigned int nthw_info_get_nb_km_flow_types(const struct info_nthw *p)
{
	return p->nb_flow_types;
}

unsigned int nthw_info_get_nb_pm_ext(const struct info_nthw *p)
{
	return p->nb_pm_ext;
}

unsigned int nthw_info_get_nb_len(const struct info_nthw *p)
{
	return p->nb_len;
}

unsigned int nthw_info_get_nb_km_categories(const struct info_nthw *p)
{
	return p->nb_km_categories;
}

unsigned int nthw_info_get_nb_km_cam_banks(const struct info_nthw *p)
{
	return p->nb_km_cam_banks;
}

unsigned int nthw_info_get_nb_km_cam_record_words(const struct info_nthw *p)
{
	return p->nb_km_cam_record_words;
}

unsigned int nthw_info_get_nb_km_cam_records(const struct info_nthw *p)
{
	return p->nb_km_cam_records;
}

unsigned int nthw_info_get_nb_km_tcam_banks(const struct info_nthw *p)
{
	return p->nb_km_tcam_banks;
}

unsigned int nthw_info_get_nb_km_tcam_bank_width(const struct info_nthw *p)
{
	return p->nb_km_tcam_bank_width;
}

unsigned int nthw_info_get_nb_flm_categories(const struct info_nthw *p)
{
	return p->nb_flm_categories;
}

unsigned int nthw_info_get_nb_flm_size_mb(const struct info_nthw *p)
{
	return p->nb_flm_size_mb;
}

unsigned int nthw_info_get_nb_flm_entry_size(const struct info_nthw *p)
{
	return p->nb_flm_entry_size;
}

unsigned int nthw_info_get_nb_flm_variant(const struct info_nthw *p)
{
	return p->nb_flm_variant;
}

unsigned int nthw_info_get_nb_flm_prios(const struct info_nthw *p)
{
	return p->nb_flm_prios;
}

unsigned int nthw_info_get_nb_flm_pst_profiles(const struct info_nthw *p)
{
	return p->nb_flm_pst_profiles;
}

unsigned int nthw_info_get_nb_flm_scrub_profiles(const struct info_nthw *p)
{
	return p->nb_flm_scrub_profiles;
}

unsigned int nthw_info_get_nb_flm_load_aps_max(const struct info_nthw *p)
{
	return p->nb_flm_load_aps_max;
}

unsigned int nthw_info_get_nb_qsl_categories(const struct info_nthw *p)
{
	return p->nb_qsl_categories;
}

unsigned int nthw_info_get_nb_qsl_qst_entries(const struct info_nthw *p)
{
	return p->nb_qsl_qst_entries;
}

unsigned int nthw_info_get_nb_pdb_categories(const struct info_nthw *p)
{
	return p->nb_pdb_categories;
}

unsigned int nthw_info_get_nb_roa_categories(const struct info_nthw *p)
{
	return p->nb_roa_categories;
}

unsigned int nthw_info_get_nb_cat_km_if_cnt(const struct info_nthw *p)
{
	return p->nb_cat_km_if_cnt;
}

unsigned int nthw_info_get_nb_cat_km_if_m0(const struct info_nthw *p)
{
	return p->m_cat_km_if_m0;
}

unsigned int nthw_info_get_nb_cat_km_if_m1(const struct info_nthw *p)
{
	return p->m_cat_km_if_m1;
}

unsigned int nthw_info_get_nb_tpe_categories(const struct info_nthw *p)
{
	return p->nb_tpe_categories;
}

unsigned int nthw_info_get_nb_tx_cpy_writers(const struct info_nthw *p)
{
	return p->nb_tx_cpy_writers;
}

unsigned int nthw_info_get_nb_tx_cpy_mask_mem(const struct info_nthw *p)
{
	return p->nb_tx_cpy_mask_mem;
}

unsigned int nthw_info_get_nb_tx_rpl_depth(const struct info_nthw *p)
{
	return p->nb_tx_rpl_depth;
}

unsigned int nthw_info_get_nb_tx_rpl_ext_categories(const struct info_nthw *p)
{
	return p->nb_tx_rpl_ext_categories;
}

unsigned int nthw_info_get_nb_tpe_ifr_categories(const struct info_nthw *p)
{
	return p->nb_tpe_ifr_categories;
}

unsigned int nthw_info_get_nb_rpp_per_ps(const struct info_nthw *p)
{
	return p->nb_rpp_per_ps;
}

unsigned int nthw_info_get_nb_hsh_categories(const struct info_nthw *p)
{
	return p->nb_hsh_categories;
}

unsigned int nthw_info_get_nb_hsh_toeplitz(const struct info_nthw *p)
{
	return p->nb_hsh_toeplitz;
}
