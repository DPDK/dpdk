/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>

#include "flow_nthw_info.h"
#include "flow_nthw_cat.h"
#include "ntnic_mod_reg.h"
#include "nthw_fpga_model.h"
#include "hw_mod_backend.h"

/*
 * Binary Flow API backend implementation into ntservice driver
 *
 * General note on this backend implementation:
 * Maybe use shadow class to combine multiple writes. However, this backend is only for dev/testing
 */

static struct backend_dev_s {
	uint8_t adapter_no;
	enum debug_mode_e dmode;
	struct info_nthw *p_info_nthw;
	struct cat_nthw *p_cat_nthw;
} be_devs[MAX_PHYS_ADAPTERS];

#define CHECK_DEBUG_ON(be, mod, inst)                                                             \
	int __debug__ = 0;                                                                        \
	if (((be)->dmode & FLOW_BACKEND_DEBUG_MODE_WRITE) || (mod)->debug)                        \
		do {                                                                              \
			mod##_nthw_set_debug_mode((inst), 0xFF);                                  \
			__debug__ = 1;                                                            \
	} while (0)

#define CHECK_DEBUG_OFF(mod, inst)                                                                \
	do {                                                                                      \
		if (__debug__)                                                                    \
			mod##_nthw_set_debug_mode((inst), 0);                                     \
	} while (0)

const struct flow_api_backend_ops *bin_flow_backend_init(nthw_fpga_t *p_fpga, void **be_dev);
static void bin_flow_backend_done(void *be_dev);

static int set_debug_mode(void *be_dev, enum debug_mode_e mode)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	be->dmode = mode;
	return 0;
}

/*
 * INFO
 */

static int get_nb_phy_ports(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_phy_ports(be->p_info_nthw);
}

static int get_nb_rx_ports(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_rx_ports(be->p_info_nthw);
}

static int get_ltx_avail(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_ltx_avail(be->p_info_nthw);
}

static int get_nb_cat_funcs(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_funcs(be->p_info_nthw);
}

static int get_nb_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_categories(be->p_info_nthw);
}

static int get_nb_cat_km_if_cnt(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_km_if_cnt(be->p_info_nthw);
}

static int get_nb_cat_km_if_m0(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_km_if_m0(be->p_info_nthw);
}

static int get_nb_cat_km_if_m1(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_km_if_m1(be->p_info_nthw);
}

static int get_nb_queues(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_queues(be->p_info_nthw);
}

static int get_nb_km_flow_types(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_flow_types(be->p_info_nthw);
}

static int get_nb_pm_ext(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_pm_ext(be->p_info_nthw);
}

static int get_nb_len(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_len(be->p_info_nthw);
}

static int get_kcc_size(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_kcc_size(be->p_info_nthw);
}

static int get_kcc_banks(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_kcc_banks(be->p_info_nthw);
}

static int get_nb_km_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_categories(be->p_info_nthw);
}

static int get_nb_km_cam_banks(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_cam_banks(be->p_info_nthw);
}

static int get_nb_km_cam_record_words(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_cam_record_words(be->p_info_nthw);
}

static int get_nb_km_cam_records(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_cam_records(be->p_info_nthw);
}

static int get_nb_km_tcam_banks(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_tcam_banks(be->p_info_nthw);
}

static int get_nb_km_tcam_bank_width(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_tcam_bank_width(be->p_info_nthw);
}

static int get_nb_flm_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_categories(be->p_info_nthw);
}

static int get_nb_flm_size_mb(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_size_mb(be->p_info_nthw);
}

static int get_nb_flm_entry_size(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_entry_size(be->p_info_nthw);
}

static int get_nb_flm_variant(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_variant(be->p_info_nthw);
}

static int get_nb_flm_prios(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_prios(be->p_info_nthw);
}

static int get_nb_flm_pst_profiles(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_pst_profiles(be->p_info_nthw);
}

static int get_nb_flm_scrub_profiles(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_scrub_profiles(be->p_info_nthw);
}

static int get_nb_flm_load_aps_max(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_load_aps_max(be->p_info_nthw);
}

static int get_nb_qsl_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_qsl_categories(be->p_info_nthw);
}

static int get_nb_qsl_qst_entries(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_qsl_qst_entries(be->p_info_nthw);
}

static int get_nb_pdb_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_pdb_categories(be->p_info_nthw);
}

static int get_nb_roa_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_roa_categories(be->p_info_nthw);
}

static int get_nb_tpe_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tpe_categories(be->p_info_nthw);
}

static int get_nb_tx_cpy_writers(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_cpy_writers(be->p_info_nthw);
}

static int get_nb_tx_cpy_mask_mem(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_cpy_mask_mem(be->p_info_nthw);
}

static int get_nb_tx_rpl_depth(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_rpl_depth(be->p_info_nthw);
}

static int get_nb_tx_rpl_ext_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_rpl_ext_categories(be->p_info_nthw);
}

static int get_nb_tpe_ifr_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tpe_ifr_categories(be->p_info_nthw);
}

static int get_nb_rpp_per_ps(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_rpp_per_ps(be->p_info_nthw);
}

static int get_nb_hsh_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_hsh_categories(be->p_info_nthw);
}

static int get_nb_hsh_toeplitz(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_hsh_toeplitz(be->p_info_nthw);
}

/*
 * CAT
 */

static bool cat_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_cat_nthw != NULL;
}

static uint32_t cat_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_cat_nthw->m_cat) << 16) |
			(nthw_module_get_minor_version(be->p_cat_nthw->m_cat) & 0xffff));
}

static int cat_cfn_flush(void *be_dev, const struct cat_func_s *cat, int cat_func, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;

	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18) {
		cat_nthw_cfn_cnt(be->p_cat_nthw, 1U);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_cfn_select(be->p_cat_nthw, cat_func);
			cat_nthw_cfn_enable(be->p_cat_nthw, cat->v18.cfn[cat_func].enable);
			cat_nthw_cfn_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].inv);
			cat_nthw_cfn_ptc_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_inv);
			cat_nthw_cfn_ptc_isl(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_isl);
			cat_nthw_cfn_ptc_cfp(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_cfp);
			cat_nthw_cfn_ptc_mac(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_mac);
			cat_nthw_cfn_ptc_l2(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_l2);
			cat_nthw_cfn_ptc_vn_tag(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_vntag);
			cat_nthw_cfn_ptc_vlan(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_vlan);
			cat_nthw_cfn_ptc_mpls(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_mpls);
			cat_nthw_cfn_ptc_l3(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_l3);
			cat_nthw_cfn_ptc_frag(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_frag);
			cat_nthw_cfn_ptc_ip_prot(be->p_cat_nthw,
				cat->v18.cfn[cat_func].ptc_ip_prot);
			cat_nthw_cfn_ptc_l4(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_l4);
			cat_nthw_cfn_ptc_tunnel(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_tunnel);
			cat_nthw_cfn_ptc_tnl_l2(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_tnl_l2);
			cat_nthw_cfn_ptc_tnl_vlan(be->p_cat_nthw,
				cat->v18.cfn[cat_func].ptc_tnl_vlan);
			cat_nthw_cfn_ptc_tnl_mpls(be->p_cat_nthw,
				cat->v18.cfn[cat_func].ptc_tnl_mpls);
			cat_nthw_cfn_ptc_tnl_l3(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_tnl_l3);
			cat_nthw_cfn_ptc_tnl_frag(be->p_cat_nthw,
				cat->v18.cfn[cat_func].ptc_tnl_frag);
			cat_nthw_cfn_ptc_tnl_ip_prot(be->p_cat_nthw,
				cat->v18.cfn[cat_func].ptc_tnl_ip_prot);
			cat_nthw_cfn_ptc_tnl_l4(be->p_cat_nthw, cat->v18.cfn[cat_func].ptc_tnl_l4);

			cat_nthw_cfn_err_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].err_inv);
			cat_nthw_cfn_err_cv(be->p_cat_nthw, cat->v18.cfn[cat_func].err_cv);
			cat_nthw_cfn_err_fcs(be->p_cat_nthw, cat->v18.cfn[cat_func].err_fcs);
			cat_nthw_cfn_err_trunc(be->p_cat_nthw, cat->v18.cfn[cat_func].err_trunc);
			cat_nthw_cfn_err_l3_cs(be->p_cat_nthw, cat->v18.cfn[cat_func].err_l3_cs);
			cat_nthw_cfn_err_l4_cs(be->p_cat_nthw, cat->v18.cfn[cat_func].err_l4_cs);

			cat_nthw_cfn_mac_port(be->p_cat_nthw, cat->v18.cfn[cat_func].mac_port);

			cat_nthw_cfn_pm_cmp(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_cmp);
			cat_nthw_cfn_pm_dct(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_dct);
			cat_nthw_cfn_pm_ext_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_ext_inv);
			cat_nthw_cfn_pm_cmb(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_cmb);
			cat_nthw_cfn_pm_and_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_and_inv);
			cat_nthw_cfn_pm_or_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_or_inv);
			cat_nthw_cfn_pm_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].pm_inv);

			cat_nthw_cfn_lc(be->p_cat_nthw, cat->v18.cfn[cat_func].lc);
			cat_nthw_cfn_lc_inv(be->p_cat_nthw, cat->v18.cfn[cat_func].lc_inv);
			cat_nthw_cfn_km0_or(be->p_cat_nthw, cat->v18.cfn[cat_func].km_or);
			cat_nthw_cfn_flush(be->p_cat_nthw);
			cat_func++;
		}

	} else if (cat->ver == 21) {
		cat_nthw_cfn_cnt(be->p_cat_nthw, 1U);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_cfn_select(be->p_cat_nthw, cat_func);
			cat_nthw_cfn_enable(be->p_cat_nthw, cat->v21.cfn[cat_func].enable);
			cat_nthw_cfn_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].inv);
			cat_nthw_cfn_ptc_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_inv);
			cat_nthw_cfn_ptc_isl(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_isl);
			cat_nthw_cfn_ptc_cfp(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_cfp);
			cat_nthw_cfn_ptc_mac(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_mac);
			cat_nthw_cfn_ptc_l2(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_l2);
			cat_nthw_cfn_ptc_vn_tag(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_vntag);
			cat_nthw_cfn_ptc_vlan(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_vlan);
			cat_nthw_cfn_ptc_mpls(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_mpls);
			cat_nthw_cfn_ptc_l3(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_l3);
			cat_nthw_cfn_ptc_frag(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_frag);
			cat_nthw_cfn_ptc_ip_prot(be->p_cat_nthw,
				cat->v21.cfn[cat_func].ptc_ip_prot);
			cat_nthw_cfn_ptc_l4(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_l4);
			cat_nthw_cfn_ptc_tunnel(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_tunnel);
			cat_nthw_cfn_ptc_tnl_l2(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_tnl_l2);
			cat_nthw_cfn_ptc_tnl_vlan(be->p_cat_nthw,
				cat->v21.cfn[cat_func].ptc_tnl_vlan);
			cat_nthw_cfn_ptc_tnl_mpls(be->p_cat_nthw,
				cat->v21.cfn[cat_func].ptc_tnl_mpls);
			cat_nthw_cfn_ptc_tnl_l3(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_tnl_l3);
			cat_nthw_cfn_ptc_tnl_frag(be->p_cat_nthw,
				cat->v21.cfn[cat_func].ptc_tnl_frag);
			cat_nthw_cfn_ptc_tnl_ip_prot(be->p_cat_nthw,
				cat->v21.cfn[cat_func].ptc_tnl_ip_prot);
			cat_nthw_cfn_ptc_tnl_l4(be->p_cat_nthw, cat->v21.cfn[cat_func].ptc_tnl_l4);

			cat_nthw_cfn_err_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].err_inv);
			cat_nthw_cfn_err_cv(be->p_cat_nthw, cat->v21.cfn[cat_func].err_cv);
			cat_nthw_cfn_err_fcs(be->p_cat_nthw, cat->v21.cfn[cat_func].err_fcs);
			cat_nthw_cfn_err_trunc(be->p_cat_nthw, cat->v21.cfn[cat_func].err_trunc);
			cat_nthw_cfn_err_l3_cs(be->p_cat_nthw, cat->v21.cfn[cat_func].err_l3_cs);
			cat_nthw_cfn_err_l4_cs(be->p_cat_nthw, cat->v21.cfn[cat_func].err_l4_cs);
			cat_nthw_cfn_err_tnl_l3_cs(be->p_cat_nthw,
				cat->v21.cfn[cat_func].err_tnl_l3_cs);
			cat_nthw_cfn_err_tnl_l4_cs(be->p_cat_nthw,
				cat->v21.cfn[cat_func].err_tnl_l4_cs);
			cat_nthw_cfn_err_ttl_exp(be->p_cat_nthw,
				cat->v21.cfn[cat_func].err_ttl_exp);
			cat_nthw_cfn_err_tnl_ttl_exp(be->p_cat_nthw,
				cat->v21.cfn[cat_func].err_tnl_ttl_exp);

			cat_nthw_cfn_mac_port(be->p_cat_nthw, cat->v21.cfn[cat_func].mac_port);

			cat_nthw_cfn_pm_cmp(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_cmp);
			cat_nthw_cfn_pm_dct(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_dct);
			cat_nthw_cfn_pm_ext_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_ext_inv);
			cat_nthw_cfn_pm_cmb(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_cmb);
			cat_nthw_cfn_pm_and_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_and_inv);
			cat_nthw_cfn_pm_or_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_or_inv);
			cat_nthw_cfn_pm_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].pm_inv);

			cat_nthw_cfn_lc(be->p_cat_nthw, cat->v21.cfn[cat_func].lc);
			cat_nthw_cfn_lc_inv(be->p_cat_nthw, cat->v21.cfn[cat_func].lc_inv);
			cat_nthw_cfn_km0_or(be->p_cat_nthw, cat->v21.cfn[cat_func].km0_or);

			if (be->p_cat_nthw->m_km_if_cnt > 1)
				cat_nthw_cfn_km1_or(be->p_cat_nthw, cat->v21.cfn[cat_func].km1_or);

			cat_nthw_cfn_flush(be->p_cat_nthw);
			cat_func++;
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_kce_flush(void *be_dev, const struct cat_func_s *cat, int km_if_idx, int index,
	int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18) {
		cat_nthw_kce_cnt(be->p_cat_nthw, 0, 1U);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_kce_select(be->p_cat_nthw, 0, index + i);
			cat_nthw_kce_enable(be->p_cat_nthw, 0, cat->v18.kce[index + i].enable_bm);
			cat_nthw_kce_flush(be->p_cat_nthw, 0);
		}

	} else if (cat->ver == 21) {
		cat_nthw_kce_cnt(be->p_cat_nthw, km_if_idx, 1U);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_kce_select(be->p_cat_nthw, km_if_idx, index + i);
			cat_nthw_kce_enable(be->p_cat_nthw, km_if_idx,
				cat->v21.kce[index + i].enable_bm[km_if_idx]);
			cat_nthw_kce_flush(be->p_cat_nthw, km_if_idx);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_kcs_flush(void *be_dev, const struct cat_func_s *cat, int km_if_idx, int cat_func,
	int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18) {
		cat_nthw_kcs_cnt(be->p_cat_nthw, 0, 1U);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_kcs_select(be->p_cat_nthw, 0, cat_func);
			cat_nthw_kcs_category(be->p_cat_nthw, 0, cat->v18.kcs[cat_func].category);
			cat_nthw_kcs_flush(be->p_cat_nthw, 0);
			cat_func++;
		}

	} else if (cat->ver == 21) {
		cat_nthw_kcs_cnt(be->p_cat_nthw, km_if_idx, 1U);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_kcs_select(be->p_cat_nthw, km_if_idx, cat_func);
			cat_nthw_kcs_category(be->p_cat_nthw, km_if_idx,
				cat->v21.kcs[cat_func].category[km_if_idx]);
			cat_nthw_kcs_flush(be->p_cat_nthw, km_if_idx);
			cat_func++;
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_fte_flush(void *be_dev, const struct cat_func_s *cat, int km_if_idx, int index,
	int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18) {
		cat_nthw_fte_cnt(be->p_cat_nthw, 0, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_fte_select(be->p_cat_nthw, 0, index + i);
			cat_nthw_fte_enable(be->p_cat_nthw, 0, cat->v18.fte[index + i].enable_bm);
			cat_nthw_fte_flush(be->p_cat_nthw, 0);
		}

	} else if (cat->ver == 21) {
		cat_nthw_fte_cnt(be->p_cat_nthw, km_if_idx, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_fte_select(be->p_cat_nthw, km_if_idx, index + i);
			cat_nthw_fte_enable(be->p_cat_nthw, km_if_idx,
				cat->v21.fte[index + i].enable_bm[km_if_idx]);
			cat_nthw_fte_flush(be->p_cat_nthw, km_if_idx);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_cte_flush(void *be_dev, const struct cat_func_s *cat, int cat_func, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_cte_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_cte_select(be->p_cat_nthw, cat_func);
			cat_nthw_cte_enable_col(be->p_cat_nthw, cat->v18.cte[cat_func].b.col);
			cat_nthw_cte_enable_cor(be->p_cat_nthw, cat->v18.cte[cat_func].b.cor);
			cat_nthw_cte_enable_hsh(be->p_cat_nthw, cat->v18.cte[cat_func].b.hsh);
			cat_nthw_cte_enable_qsl(be->p_cat_nthw, cat->v18.cte[cat_func].b.qsl);
			cat_nthw_cte_enable_ipf(be->p_cat_nthw, cat->v18.cte[cat_func].b.ipf);
			cat_nthw_cte_enable_slc(be->p_cat_nthw, cat->v18.cte[cat_func].b.slc);
			cat_nthw_cte_enable_pdb(be->p_cat_nthw, cat->v18.cte[cat_func].b.pdb);
			cat_nthw_cte_enable_msk(be->p_cat_nthw, cat->v18.cte[cat_func].b.msk);
			cat_nthw_cte_enable_hst(be->p_cat_nthw, cat->v18.cte[cat_func].b.hst);
			cat_nthw_cte_enable_epp(be->p_cat_nthw, cat->v18.cte[cat_func].b.epp);
			cat_nthw_cte_enable_tpe(be->p_cat_nthw, cat->v18.cte[cat_func].b.tpe);

			cat_nthw_cte_flush(be->p_cat_nthw);
			cat_func++;
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_cts_flush(void *be_dev, const struct cat_func_s *cat, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_cts_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_cts_select(be->p_cat_nthw, index + i);
			cat_nthw_cts_cat_a(be->p_cat_nthw, cat->v18.cts[index + i].cat_a);
			cat_nthw_cts_cat_b(be->p_cat_nthw, cat->v18.cts[index + i].cat_b);
			cat_nthw_cts_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_cot_flush(void *be_dev, const struct cat_func_s *cat, int cat_func, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_cot_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_cot_select(be->p_cat_nthw, cat_func + i);
			cat_nthw_cot_color(be->p_cat_nthw, cat->v18.cot[cat_func + i].color);
			cat_nthw_cot_km(be->p_cat_nthw, cat->v18.cot[cat_func + i].km);
			cat_nthw_cot_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_cct_flush(void *be_dev, const struct cat_func_s *cat, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_cct_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_cct_select(be->p_cat_nthw, index + i);
			cat_nthw_cct_color(be->p_cat_nthw, cat->v18.cct[index + i].color);
			cat_nthw_cct_km(be->p_cat_nthw, cat->v18.cct[index + i].km);
			cat_nthw_cct_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_exo_flush(void *be_dev, const struct cat_func_s *cat, int ext_index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_exo_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_exo_select(be->p_cat_nthw, ext_index + i);
			cat_nthw_exo_dyn(be->p_cat_nthw, cat->v18.exo[ext_index + i].dyn);
			cat_nthw_exo_ofs(be->p_cat_nthw, cat->v18.exo[ext_index + i].ofs);
			cat_nthw_exo_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_rck_flush(void *be_dev, const struct cat_func_s *cat, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_rck_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_rck_select(be->p_cat_nthw, index + i);
			cat_nthw_rck_data(be->p_cat_nthw, cat->v18.rck[index + i].rck_data);
			cat_nthw_rck_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_len_flush(void *be_dev, const struct cat_func_s *cat, int len_index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_len_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_len_select(be->p_cat_nthw, len_index + i);
			cat_nthw_len_lower(be->p_cat_nthw, cat->v18.len[len_index + i].lower);
			cat_nthw_len_upper(be->p_cat_nthw, cat->v18.len[len_index + i].upper);
			cat_nthw_len_dyn1(be->p_cat_nthw, cat->v18.len[len_index + i].dyn1);
			cat_nthw_len_dyn2(be->p_cat_nthw, cat->v18.len[len_index + i].dyn2);
			cat_nthw_len_inv(be->p_cat_nthw, cat->v18.len[len_index + i].inv);
			cat_nthw_len_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

static int cat_kcc_flush(void *be_dev, const struct cat_func_s *cat, int len_index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, cat, be->p_cat_nthw);

	if (cat->ver == 18 || cat->ver == 21) {
		cat_nthw_kcc_cnt(be->p_cat_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			cat_nthw_kcc_select(be->p_cat_nthw, len_index + i);
			cat_nthw_kcc_key(be->p_cat_nthw, cat->v18.kcc_cam[len_index + i].key);
			cat_nthw_kcc_category(be->p_cat_nthw,
				cat->v18.kcc_cam[len_index + i].category);
			cat_nthw_kcc_id(be->p_cat_nthw, cat->v18.kcc_cam[len_index + i].id);
			cat_nthw_kcc_flush(be->p_cat_nthw);
		}
	}

	CHECK_DEBUG_OFF(cat, be->p_cat_nthw);
	return 0;
}

/*
 * DBS
 */

static int alloc_rx_queue(void *be_dev, int queue_id)
{
	(void)be_dev;
	(void)queue_id;
	NT_LOG(ERR, FILTER, "ERROR alloc Rx queue");
	return -1;
}

static int free_rx_queue(void *be_dev, int hw_queue)
{
	(void)be_dev;
	(void)hw_queue;
	NT_LOG(ERR, FILTER, "ERROR free Rx queue");
	return 0;
}

const struct flow_api_backend_ops flow_be_iface = {
	1,

	set_debug_mode,
	get_nb_phy_ports,
	get_nb_rx_ports,
	get_ltx_avail,
	get_nb_cat_funcs,
	get_nb_categories,
	get_nb_cat_km_if_cnt,
	get_nb_cat_km_if_m0,
	get_nb_cat_km_if_m1,
	get_nb_queues,
	get_nb_km_flow_types,
	get_nb_pm_ext,
	get_nb_len,
	get_kcc_size,
	get_kcc_banks,
	get_nb_km_categories,
	get_nb_km_cam_banks,
	get_nb_km_cam_record_words,
	get_nb_km_cam_records,
	get_nb_km_tcam_banks,
	get_nb_km_tcam_bank_width,
	get_nb_flm_categories,
	get_nb_flm_size_mb,
	get_nb_flm_entry_size,
	get_nb_flm_variant,
	get_nb_flm_prios,
	get_nb_flm_pst_profiles,
	get_nb_flm_scrub_profiles,
	get_nb_flm_load_aps_max,
	get_nb_qsl_categories,
	get_nb_qsl_qst_entries,
	get_nb_pdb_categories,
	get_nb_roa_categories,
	get_nb_tpe_categories,
	get_nb_tx_cpy_writers,
	get_nb_tx_cpy_mask_mem,
	get_nb_tx_rpl_depth,
	get_nb_tx_rpl_ext_categories,
	get_nb_tpe_ifr_categories,
	get_nb_rpp_per_ps,
	get_nb_hsh_categories,
	get_nb_hsh_toeplitz,

	alloc_rx_queue,
	free_rx_queue,

	cat_get_present,
	cat_get_version,
	cat_cfn_flush,

	cat_kce_flush,
	cat_kcs_flush,
	cat_fte_flush,

	cat_cte_flush,
	cat_cts_flush,
	cat_cot_flush,
	cat_cct_flush,
	cat_exo_flush,
	cat_rck_flush,
	cat_len_flush,
	cat_kcc_flush,
};

const struct flow_api_backend_ops *bin_flow_backend_init(nthw_fpga_t *p_fpga, void **dev)
{
	uint8_t physical_adapter_no = (uint8_t)p_fpga->p_fpga_info->adapter_no;

	struct info_nthw *pinfonthw = info_nthw_new();
	info_nthw_init(pinfonthw, p_fpga, physical_adapter_no);
	be_devs[physical_adapter_no].p_info_nthw = pinfonthw;

	/* Init nthw CAT */
	if (cat_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct cat_nthw *pcatnthw = cat_nthw_new();
		cat_nthw_init(pcatnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_cat_nthw = pcatnthw;

	} else {
		be_devs[physical_adapter_no].p_cat_nthw = NULL;
	}

	be_devs[physical_adapter_no].adapter_no = physical_adapter_no;
	*dev = (void *)&be_devs[physical_adapter_no];

	return &flow_be_iface;
}

static void bin_flow_backend_done(void *dev)
{
	struct backend_dev_s *be_dev = (struct backend_dev_s *)dev;
	info_nthw_delete(be_dev->p_info_nthw);
	cat_nthw_delete(be_dev->p_cat_nthw);
}

static const struct flow_backend_ops ops = {
	.bin_flow_backend_init = bin_flow_backend_init,
	.bin_flow_backend_done = bin_flow_backend_done,
};

void flow_backend_init(void)
{
	register_flow_backend_ops(&ops);
}
