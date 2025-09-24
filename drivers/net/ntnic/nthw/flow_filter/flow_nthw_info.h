/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_INFO_H__
#define __FLOW_NTHW_INFO_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct info_nthw;

struct info_nthw *nthw_info_new(void);
void nthw_info_delete(struct info_nthw *p);
int nthw_info_init(struct info_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

unsigned int nthw_info_get_nb_phy_ports(const struct info_nthw *p);
unsigned int nthw_info_get_nb_rx_ports(const struct info_nthw *p);
unsigned int nthw_info_get_ltx_avail(const struct info_nthw *p);

unsigned int nthw_info_get_nb_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_queues(const struct info_nthw *p);
unsigned int nthw_info_get_nb_cat_funcs(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_flow_types(const struct info_nthw *p);
unsigned int nthw_info_get_nb_pm_ext(const struct info_nthw *p);
unsigned int nthw_info_get_nb_len(const struct info_nthw *p);
unsigned int nthw_info_get_kcc_size(const struct info_nthw *p);
unsigned int nthw_info_get_kcc_banks(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_cam_banks(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_cam_record_words(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_cam_records(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_tcam_banks(const struct info_nthw *p);
unsigned int nthw_info_get_nb_km_tcam_bank_width(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_size_mb(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_entry_size(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_variant(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_prios(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_pst_profiles(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_scrub_profiles(const struct info_nthw *p);
unsigned int nthw_info_get_nb_flm_load_aps_max(const struct info_nthw *p);
unsigned int nthw_info_get_nb_qsl_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_qsl_qst_entries(const struct info_nthw *p);
unsigned int nthw_info_get_nb_pdb_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_roa_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_cat_km_if_cnt(const struct info_nthw *p);
unsigned int nthw_info_get_nb_cat_km_if_m0(const struct info_nthw *p);
unsigned int nthw_info_get_nb_cat_km_if_m1(const struct info_nthw *p);
unsigned int nthw_info_get_nb_tpe_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_tx_cpy_writers(const struct info_nthw *p);
unsigned int nthw_info_get_nb_tx_cpy_mask_mem(const struct info_nthw *p);
unsigned int nthw_info_get_nb_tx_rpl_depth(const struct info_nthw *p);
unsigned int nthw_info_get_nb_tx_rpl_ext_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_tpe_ifr_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_rpp_per_ps(const struct info_nthw *p);
unsigned int nthw_info_get_nb_hsh_categories(const struct info_nthw *p);
unsigned int nthw_info_get_nb_hsh_toeplitz(const struct info_nthw *p);

struct info_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;
	unsigned int n_phy_ports;
	unsigned int n_rx_ports;
	unsigned int n_ltx_avail;
	unsigned int nb_cat_func;
	unsigned int nb_categories;
	unsigned int nb_queues;
	unsigned int nb_flow_types;
	unsigned int nb_pm_ext;
	unsigned int nb_len;
	unsigned int nb_kcc_size;
	unsigned int nb_kcc_banks;
	unsigned int nb_km_categories;
	unsigned int nb_km_cam_banks;
	unsigned int nb_km_cam_record_words;
	unsigned int nb_km_cam_records;
	unsigned int nb_km_tcam_banks;
	unsigned int nb_km_tcam_bank_width;
	unsigned int nb_flm_categories;
	unsigned int nb_flm_size_mb;
	unsigned int nb_flm_entry_size;
	unsigned int nb_flm_variant;
	unsigned int nb_flm_prios;
	unsigned int nb_flm_pst_profiles;
	unsigned int nb_flm_scrub_profiles;
	unsigned int nb_flm_load_aps_max;
	unsigned int nb_hst_categories;
	unsigned int nb_qsl_categories;
	unsigned int nb_qsl_qst_entries;
	unsigned int nb_pdb_categories;
	unsigned int nb_roa_categories;
	unsigned int nb_dbs_categories;
	unsigned int nb_cat_km_if_cnt;
	unsigned int m_cat_km_if_m0;
	unsigned int m_cat_km_if_m1;
	unsigned int nb_tpe_categories;
	unsigned int nb_tx_cpy_writers;
	unsigned int nb_tx_cpy_mask_mem;
	unsigned int nb_tx_rpl_depth;
	unsigned int nb_tx_rpl_ext_categories;
	unsigned int nb_tpe_ifr_categories;
	unsigned int nb_rpp_per_ps;
	unsigned int nb_hsh_categories;
	unsigned int nb_hsh_toeplitz;
};

#endif  /* __FLOW_NTHW_INFO_H__ */
