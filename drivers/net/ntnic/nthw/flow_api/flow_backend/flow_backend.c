/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>

#include "flow_nthw_info.h"
#include "flow_nthw_ifr.h"
#include "flow_nthw_cat.h"
#include "flow_nthw_csu.h"
#include "flow_nthw_km.h"
#include "flow_nthw_flm.h"
#include "flow_nthw_hfu.h"
#include "flow_nthw_hsh.h"
#include "flow_nthw_qsl.h"
#include "flow_nthw_slc_lr.h"
#include "flow_nthw_pdb.h"
#include "flow_nthw_rpp_lr.h"
#include "flow_nthw_tx_cpy.h"
#include "flow_nthw_tx_ins.h"
#include "flow_nthw_tx_rpl.h"
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
	struct km_nthw *p_km_nthw;
	struct flm_nthw *p_flm_nthw;
	struct hsh_nthw *p_hsh_nthw;
	struct qsl_nthw *p_qsl_nthw;
	struct slc_lr_nthw *p_slc_lr_nthw;
	struct pdb_nthw *p_pdb_nthw;
	struct hfu_nthw *p_hfu_nthw;    /* TPE module */
	struct rpp_lr_nthw *p_rpp_lr_nthw;      /* TPE module */
	struct tx_cpy_nthw *p_tx_cpy_nthw;      /* TPE module */
	struct tx_ins_nthw *p_tx_ins_nthw;      /* TPE module */
	struct tx_rpl_nthw *p_tx_rpl_nthw;      /* TPE module */
	struct csu_nthw *p_csu_nthw;    /* TPE module */
	struct ifr_nthw *p_ifr_nthw;    /* TPE module */
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

const struct flow_api_backend_ops *nthw_bin_flow_backend_init(nthw_fpga_t *p_fpga, void **be_dev);
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
 * KM
 */

static bool km_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_km_nthw != NULL;
}

static uint32_t km_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_km_nthw->m_km) << 16) |
			(nthw_module_get_minor_version(be->p_km_nthw->m_km) & 0xffff));
}

static int km_rcp_flush(void *be_dev, const struct km_func_s *km, int category, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;

	CHECK_DEBUG_ON(be, km, be->p_km_nthw);

	if (km->ver == 7) {
		km_nthw_rcp_cnt(be->p_km_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			km_nthw_rcp_select(be->p_km_nthw, category + i);
			km_nthw_rcp_qw0_dyn(be->p_km_nthw, km->v7.rcp[category + i].qw0_dyn);
			km_nthw_rcp_qw0_ofs(be->p_km_nthw, km->v7.rcp[category + i].qw0_ofs);
			km_nthw_rcp_qw0_sel_a(be->p_km_nthw, km->v7.rcp[category + i].qw0_sel_a);
			km_nthw_rcp_qw0_sel_b(be->p_km_nthw, km->v7.rcp[category + i].qw0_sel_b);
			km_nthw_rcp_qw4_dyn(be->p_km_nthw, km->v7.rcp[category + i].qw4_dyn);
			km_nthw_rcp_qw4_ofs(be->p_km_nthw, km->v7.rcp[category + i].qw4_ofs);
			km_nthw_rcp_qw4_sel_a(be->p_km_nthw, km->v7.rcp[category + i].qw4_sel_a);
			km_nthw_rcp_qw4_sel_b(be->p_km_nthw, km->v7.rcp[category + i].qw4_sel_b);
			km_nthw_rcp_dw8_dyn(be->p_km_nthw, km->v7.rcp[category + i].dw8_dyn);
			km_nthw_rcp_dw8_ofs(be->p_km_nthw, km->v7.rcp[category + i].dw8_ofs);
			km_nthw_rcp_dw8_sel_a(be->p_km_nthw, km->v7.rcp[category + i].dw8_sel_a);
			km_nthw_rcp_dw8_sel_b(be->p_km_nthw, km->v7.rcp[category + i].dw8_sel_b);
			km_nthw_rcp_dw10_dyn(be->p_km_nthw, km->v7.rcp[category + i].dw10_dyn);
			km_nthw_rcp_dw10_ofs(be->p_km_nthw, km->v7.rcp[category + i].dw10_ofs);
			km_nthw_rcp_dw10_sel_a(be->p_km_nthw, km->v7.rcp[category + i].dw10_sel_a);
			km_nthw_rcp_dw10_sel_b(be->p_km_nthw, km->v7.rcp[category + i].dw10_sel_b);
			km_nthw_rcp_swx_cch(be->p_km_nthw, km->v7.rcp[category + i].swx_cch);
			km_nthw_rcp_swx_sel_a(be->p_km_nthw, km->v7.rcp[category + i].swx_sel_a);
			km_nthw_rcp_swx_sel_b(be->p_km_nthw, km->v7.rcp[category + i].swx_sel_b);
			km_nthw_rcp_mask_da(be->p_km_nthw, km->v7.rcp[category + i].mask_d_a);
			km_nthw_rcp_mask_b(be->p_km_nthw, km->v7.rcp[category + i].mask_b);
			km_nthw_rcp_dual(be->p_km_nthw, km->v7.rcp[category + i].dual);
			km_nthw_rcp_paired(be->p_km_nthw, km->v7.rcp[category + i].paired);
			km_nthw_rcp_el_a(be->p_km_nthw, km->v7.rcp[category + i].el_a);
			km_nthw_rcp_el_b(be->p_km_nthw, km->v7.rcp[category + i].el_b);
			km_nthw_rcp_info_a(be->p_km_nthw, km->v7.rcp[category + i].info_a);
			km_nthw_rcp_info_b(be->p_km_nthw, km->v7.rcp[category + i].info_b);
			km_nthw_rcp_ftm_a(be->p_km_nthw, km->v7.rcp[category + i].ftm_a);
			km_nthw_rcp_ftm_b(be->p_km_nthw, km->v7.rcp[category + i].ftm_b);
			km_nthw_rcp_bank_a(be->p_km_nthw, km->v7.rcp[category + i].bank_a);
			km_nthw_rcp_bank_b(be->p_km_nthw, km->v7.rcp[category + i].bank_b);
			km_nthw_rcp_kl_a(be->p_km_nthw, km->v7.rcp[category + i].kl_a);
			km_nthw_rcp_kl_b(be->p_km_nthw, km->v7.rcp[category + i].kl_b);
			km_nthw_rcp_keyway_a(be->p_km_nthw, km->v7.rcp[category + i].keyway_a);
			km_nthw_rcp_keyway_b(be->p_km_nthw, km->v7.rcp[category + i].keyway_b);
			km_nthw_rcp_synergy_mode(be->p_km_nthw,
				km->v7.rcp[category + i].synergy_mode);
			km_nthw_rcp_dw0_b_dyn(be->p_km_nthw, km->v7.rcp[category + i].dw0_b_dyn);
			km_nthw_rcp_dw0_b_ofs(be->p_km_nthw, km->v7.rcp[category + i].dw0_b_ofs);
			km_nthw_rcp_dw2_b_dyn(be->p_km_nthw, km->v7.rcp[category + i].dw2_b_dyn);
			km_nthw_rcp_dw2_b_ofs(be->p_km_nthw, km->v7.rcp[category + i].dw2_b_ofs);
			km_nthw_rcp_sw4_b_dyn(be->p_km_nthw, km->v7.rcp[category + i].sw4_b_dyn);
			km_nthw_rcp_sw4_b_ofs(be->p_km_nthw, km->v7.rcp[category + i].sw4_b_ofs);
			km_nthw_rcp_sw5_b_dyn(be->p_km_nthw, km->v7.rcp[category + i].sw5_b_dyn);
			km_nthw_rcp_sw5_b_ofs(be->p_km_nthw, km->v7.rcp[category + i].sw5_b_ofs);
			km_nthw_rcp_flush(be->p_km_nthw);
		}
	}

	CHECK_DEBUG_OFF(km, be->p_km_nthw);
	return 0;
}

static int km_cam_flush(void *be_dev, const struct km_func_s *km, int bank, int record, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, km, be->p_km_nthw);

	if (km->ver == 7) {
		km_nthw_cam_cnt(be->p_km_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			km_nthw_cam_select(be->p_km_nthw, (bank << 11) + record + i);
			km_nthw_cam_w0(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].w0);
			km_nthw_cam_w1(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].w1);
			km_nthw_cam_w2(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].w2);
			km_nthw_cam_w3(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].w3);
			km_nthw_cam_w4(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].w4);
			km_nthw_cam_w5(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].w5);
			km_nthw_cam_ft0(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].ft0);
			km_nthw_cam_ft1(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].ft1);
			km_nthw_cam_ft2(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].ft2);
			km_nthw_cam_ft3(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].ft3);
			km_nthw_cam_ft4(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].ft4);
			km_nthw_cam_ft5(be->p_km_nthw, km->v7.cam[(bank << 11) + record + i].ft5);
			km_nthw_cam_flush(be->p_km_nthw);
		}
	}

	CHECK_DEBUG_OFF(km, be->p_km_nthw);
	return 0;
}

static int km_tcam_flush(void *be_dev, const struct km_func_s *km, int bank, int byte, int value,
	int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, km, be->p_km_nthw);

	if (km->ver == 7) {
		int start_idx = bank * 4 * 256 + byte * 256 + value;
		km_nthw_tcam_cnt(be->p_km_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			if (km->v7.tcam[start_idx + i].dirty) {
				km_nthw_tcam_select(be->p_km_nthw, start_idx + i);
				km_nthw_tcam_t(be->p_km_nthw, km->v7.tcam[start_idx + i].t);
				km_nthw_tcam_flush(be->p_km_nthw);
				km->v7.tcam[start_idx + i].dirty = 0;
			}
		}
	}

	CHECK_DEBUG_OFF(km, be->p_km_nthw);
	return 0;
}

/*
 * bank is the TCAM bank, index is the index within the bank (0..71)
 */
static int km_tci_flush(void *be_dev, const struct km_func_s *km, int bank, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, km, be->p_km_nthw);

	if (km->ver == 7) {
		/* TCAM bank width in version 3 = 72 */
		km_nthw_tci_cnt(be->p_km_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			km_nthw_tci_select(be->p_km_nthw, bank * 72 + index + i);
			km_nthw_tci_color(be->p_km_nthw, km->v7.tci[bank * 72 + index + i].color);
			km_nthw_tci_ft(be->p_km_nthw, km->v7.tci[bank * 72 + index + i].ft);
			km_nthw_tci_flush(be->p_km_nthw);
		}
	}

	CHECK_DEBUG_OFF(km, be->p_km_nthw);
	return 0;
}

/*
 * bank is the TCAM bank, index is the index within the bank (0..71)
 */
static int km_tcq_flush(void *be_dev, const struct km_func_s *km, int bank, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, km, be->p_km_nthw);

	if (km->ver == 7) {
		/* TCAM bank width in version 3 = 72 */
		km_nthw_tcq_cnt(be->p_km_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			/* adr = lover 4 bits = bank, upper 7 bits = index */
			km_nthw_tcq_select(be->p_km_nthw, bank + (index << 4) + i);
			km_nthw_tcq_bank_mask(be->p_km_nthw,
				km->v7.tcq[bank + (index << 4) + i].bank_mask);
			km_nthw_tcq_qual(be->p_km_nthw, km->v7.tcq[bank + (index << 4) + i].qual);
			km_nthw_tcq_flush(be->p_km_nthw);
		}
	}

	CHECK_DEBUG_OFF(km, be->p_km_nthw);
	return 0;
}

/*
 * FLM
 */

static bool flm_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_flm_nthw != NULL;
}

static uint32_t flm_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_flm_nthw->m_flm) << 16) |
			(nthw_module_get_minor_version(be->p_flm_nthw->m_flm) & 0xffff));
}

static int flm_control_flush(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_control_enable(be->p_flm_nthw, flm->v25.control->enable);
		flm_nthw_control_init(be->p_flm_nthw, flm->v25.control->init);
		flm_nthw_control_lds(be->p_flm_nthw, flm->v25.control->lds);
		flm_nthw_control_lfs(be->p_flm_nthw, flm->v25.control->lfs);
		flm_nthw_control_lis(be->p_flm_nthw, flm->v25.control->lis);
		flm_nthw_control_uds(be->p_flm_nthw, flm->v25.control->uds);
		flm_nthw_control_uis(be->p_flm_nthw, flm->v25.control->uis);
		flm_nthw_control_rds(be->p_flm_nthw, flm->v25.control->rds);
		flm_nthw_control_ris(be->p_flm_nthw, flm->v25.control->ris);
		flm_nthw_control_pds(be->p_flm_nthw, flm->v25.control->pds);
		flm_nthw_control_pis(be->p_flm_nthw, flm->v25.control->pis);
		flm_nthw_control_crcwr(be->p_flm_nthw, flm->v25.control->crcwr);
		flm_nthw_control_crcrd(be->p_flm_nthw, flm->v25.control->crcrd);
		flm_nthw_control_rbl(be->p_flm_nthw, flm->v25.control->rbl);
		flm_nthw_control_eab(be->p_flm_nthw, flm->v25.control->eab);
		flm_nthw_control_split_sdram_usage(be->p_flm_nthw,
			flm->v25.control->split_sdram_usage);
		flm_nthw_control_flush(be->p_flm_nthw);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_status_flush(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		/* CALIBDONE, INITDONE, IDLE, and EFT_BP is read only */
		flm_nthw_status_critical(be->p_flm_nthw, &flm->v25.status->critical, 0);
		flm_nthw_status_panic(be->p_flm_nthw, &flm->v25.status->panic, 0);
		flm_nthw_status_crcerr(be->p_flm_nthw, &flm->v25.status->crcerr, 0);
		flm_nthw_status_cache_buf_crit(be->p_flm_nthw,
			&flm->v25.status->cache_buf_critical, 0);
		flm_nthw_status_flush(be->p_flm_nthw);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_status_update(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_status_update(be->p_flm_nthw);
		flm_nthw_status_calib_success(be->p_flm_nthw, &flm->v25.status->calib_success, 1);
		flm_nthw_status_calib_fail(be->p_flm_nthw, &flm->v25.status->calib_fail, 1);
		flm_nthw_status_initdone(be->p_flm_nthw, &flm->v25.status->initdone, 1);
		flm_nthw_status_idle(be->p_flm_nthw, &flm->v25.status->idle, 1);
		flm_nthw_status_critical(be->p_flm_nthw, &flm->v25.status->critical, 1);
		flm_nthw_status_panic(be->p_flm_nthw, &flm->v25.status->panic, 1);
		flm_nthw_status_crcerr(be->p_flm_nthw, &flm->v25.status->crcerr, 1);
		flm_nthw_status_eft_bp(be->p_flm_nthw, &flm->v25.status->eft_bp, 1);
		flm_nthw_status_cache_buf_crit(be->p_flm_nthw,
			&flm->v25.status->cache_buf_critical, 1);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_scan_flush(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_scan_i(be->p_flm_nthw, flm->v25.scan->i);
		flm_nthw_scan_flush(be->p_flm_nthw);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_load_bin_flush(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_load_bin(be->p_flm_nthw, flm->v25.load_bin->bin);
		flm_nthw_load_bin_flush(be->p_flm_nthw);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_prio_flush(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_prio_limit0(be->p_flm_nthw, flm->v25.prio->limit0);
		flm_nthw_prio_ft0(be->p_flm_nthw, flm->v25.prio->ft0);
		flm_nthw_prio_limit1(be->p_flm_nthw, flm->v25.prio->limit1);
		flm_nthw_prio_ft1(be->p_flm_nthw, flm->v25.prio->ft1);
		flm_nthw_prio_limit2(be->p_flm_nthw, flm->v25.prio->limit2);
		flm_nthw_prio_ft2(be->p_flm_nthw, flm->v25.prio->ft2);
		flm_nthw_prio_limit3(be->p_flm_nthw, flm->v25.prio->limit3);
		flm_nthw_prio_ft3(be->p_flm_nthw, flm->v25.prio->ft3);
		flm_nthw_prio_flush(be->p_flm_nthw);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_pst_flush(void *be_dev, const struct flm_func_s *flm, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_pst_cnt(be->p_flm_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			flm_nthw_pst_select(be->p_flm_nthw, index + i);
			flm_nthw_pst_bp(be->p_flm_nthw, flm->v25.pst[index + i].bp);
			flm_nthw_pst_pp(be->p_flm_nthw, flm->v25.pst[index + i].pp);
			flm_nthw_pst_tp(be->p_flm_nthw, flm->v25.pst[index + i].tp);
			flm_nthw_pst_flush(be->p_flm_nthw);
		}
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_rcp_flush(void *be_dev, const struct flm_func_s *flm, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_rcp_cnt(be->p_flm_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			flm_nthw_rcp_select(be->p_flm_nthw, index + i);
			flm_nthw_rcp_lookup(be->p_flm_nthw, flm->v25.rcp[index + i].lookup);
			flm_nthw_rcp_qw0_dyn(be->p_flm_nthw, flm->v25.rcp[index + i].qw0_dyn);
			flm_nthw_rcp_qw0_ofs(be->p_flm_nthw, flm->v25.rcp[index + i].qw0_ofs);
			flm_nthw_rcp_qw0_sel(be->p_flm_nthw, flm->v25.rcp[index + i].qw0_sel);
			flm_nthw_rcp_qw4_dyn(be->p_flm_nthw, flm->v25.rcp[index + i].qw4_dyn);
			flm_nthw_rcp_qw4_ofs(be->p_flm_nthw, flm->v25.rcp[index + i].qw4_ofs);
			flm_nthw_rcp_sw8_dyn(be->p_flm_nthw, flm->v25.rcp[index + i].sw8_dyn);
			flm_nthw_rcp_sw8_ofs(be->p_flm_nthw, flm->v25.rcp[index + i].sw8_ofs);
			flm_nthw_rcp_sw8_sel(be->p_flm_nthw, flm->v25.rcp[index + i].sw8_sel);
			flm_nthw_rcp_sw9_dyn(be->p_flm_nthw, flm->v25.rcp[index + i].sw9_dyn);
			flm_nthw_rcp_sw9_ofs(be->p_flm_nthw, flm->v25.rcp[index + i].sw9_ofs);
			flm_nthw_rcp_mask(be->p_flm_nthw, flm->v25.rcp[index + i].mask);
			flm_nthw_rcp_kid(be->p_flm_nthw, flm->v25.rcp[index + i].kid);
			flm_nthw_rcp_opn(be->p_flm_nthw, flm->v25.rcp[index + i].opn);
			flm_nthw_rcp_ipn(be->p_flm_nthw, flm->v25.rcp[index + i].ipn);
			flm_nthw_rcp_byt_dyn(be->p_flm_nthw, flm->v25.rcp[index + i].byt_dyn);
			flm_nthw_rcp_byt_ofs(be->p_flm_nthw, flm->v25.rcp[index + i].byt_ofs);
			flm_nthw_rcp_txplm(be->p_flm_nthw, flm->v25.rcp[index + i].txplm);
			flm_nthw_rcp_auto_ipv4_mask(be->p_flm_nthw,
				flm->v25.rcp[index + i].auto_ipv4_mask);
			flm_nthw_rcp_flush(be->p_flm_nthw);
		}
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_scrub_flush(void *be_dev, const struct flm_func_s *flm, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_scrub_cnt(be->p_flm_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			flm_nthw_scrub_select(be->p_flm_nthw, index + i);
			flm_nthw_scrub_t(be->p_flm_nthw, flm->v25.scrub[index + i].t);
			flm_nthw_scrub_r(be->p_flm_nthw, flm->v25.scrub[index + i].r);
			flm_nthw_scrub_del(be->p_flm_nthw, flm->v25.scrub[index + i].del);
			flm_nthw_scrub_inf(be->p_flm_nthw, flm->v25.scrub[index + i].inf);
			flm_nthw_scrub_flush(be->p_flm_nthw);
		}
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_buf_ctrl_update(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_buf_ctrl_update(be->p_flm_nthw,
			&flm->v25.buf_ctrl->lrn_free,
			&flm->v25.buf_ctrl->inf_avail,
			&flm->v25.buf_ctrl->sta_avail);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_stat_update(void *be_dev, const struct flm_func_s *flm)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	if (flm->ver >= 25) {
		flm_nthw_stat_lrn_done_update(be->p_flm_nthw);
		flm_nthw_stat_lrn_ignore_update(be->p_flm_nthw);
		flm_nthw_stat_lrn_fail_update(be->p_flm_nthw);
		flm_nthw_stat_unl_done_update(be->p_flm_nthw);
		flm_nthw_stat_unl_ignore_update(be->p_flm_nthw);
		flm_nthw_stat_rel_done_update(be->p_flm_nthw);
		flm_nthw_stat_rel_ignore_update(be->p_flm_nthw);
		flm_nthw_stat_aul_done_update(be->p_flm_nthw);
		flm_nthw_stat_aul_ignore_update(be->p_flm_nthw);
		flm_nthw_stat_aul_fail_update(be->p_flm_nthw);
		flm_nthw_stat_tul_done_update(be->p_flm_nthw);
		flm_nthw_stat_flows_update(be->p_flm_nthw);
		flm_nthw_load_lps_update(be->p_flm_nthw);
		flm_nthw_load_aps_update(be->p_flm_nthw);

		flm_nthw_stat_lrn_done_cnt(be->p_flm_nthw, &flm->v25.lrn_done->cnt, 1);
		flm_nthw_stat_lrn_ignore_cnt(be->p_flm_nthw, &flm->v25.lrn_ignore->cnt, 1);
		flm_nthw_stat_lrn_fail_cnt(be->p_flm_nthw, &flm->v25.lrn_fail->cnt, 1);
		flm_nthw_stat_unl_done_cnt(be->p_flm_nthw, &flm->v25.unl_done->cnt, 1);
		flm_nthw_stat_unl_ignore_cnt(be->p_flm_nthw, &flm->v25.unl_ignore->cnt, 1);
		flm_nthw_stat_rel_done_cnt(be->p_flm_nthw, &flm->v25.rel_done->cnt, 1);
		flm_nthw_stat_rel_ignore_cnt(be->p_flm_nthw, &flm->v25.rel_ignore->cnt, 1);
		flm_nthw_stat_aul_done_cnt(be->p_flm_nthw, &flm->v25.aul_done->cnt, 1);
		flm_nthw_stat_aul_ignore_cnt(be->p_flm_nthw, &flm->v25.aul_ignore->cnt, 1);
		flm_nthw_stat_aul_fail_cnt(be->p_flm_nthw, &flm->v25.aul_fail->cnt, 1);
		flm_nthw_stat_tul_done_cnt(be->p_flm_nthw, &flm->v25.tul_done->cnt, 1);
		flm_nthw_stat_flows_cnt(be->p_flm_nthw, &flm->v25.flows->cnt, 1);

		flm_nthw_stat_prb_done_update(be->p_flm_nthw);
		flm_nthw_stat_prb_ignore_update(be->p_flm_nthw);
		flm_nthw_stat_prb_done_cnt(be->p_flm_nthw, &flm->v25.prb_done->cnt, 1);
		flm_nthw_stat_prb_ignore_cnt(be->p_flm_nthw, &flm->v25.prb_ignore->cnt, 1);

		flm_nthw_load_lps_cnt(be->p_flm_nthw, &flm->v25.load_lps->lps, 1);
		flm_nthw_load_aps_cnt(be->p_flm_nthw, &flm->v25.load_aps->aps, 1);
	}

	if (flm->ver >= 25) {
		flm_nthw_stat_sta_done_update(be->p_flm_nthw);
		flm_nthw_stat_inf_done_update(be->p_flm_nthw);
		flm_nthw_stat_inf_skip_update(be->p_flm_nthw);
		flm_nthw_stat_pck_hit_update(be->p_flm_nthw);
		flm_nthw_stat_pck_miss_update(be->p_flm_nthw);
		flm_nthw_stat_pck_unh_update(be->p_flm_nthw);
		flm_nthw_stat_pck_dis_update(be->p_flm_nthw);
		flm_nthw_stat_csh_hit_update(be->p_flm_nthw);
		flm_nthw_stat_csh_miss_update(be->p_flm_nthw);
		flm_nthw_stat_csh_unh_update(be->p_flm_nthw);
		flm_nthw_stat_cuc_start_update(be->p_flm_nthw);
		flm_nthw_stat_cuc_move_update(be->p_flm_nthw);

		flm_nthw_stat_sta_done_cnt(be->p_flm_nthw, &flm->v25.sta_done->cnt, 1);
		flm_nthw_stat_inf_done_cnt(be->p_flm_nthw, &flm->v25.inf_done->cnt, 1);
		flm_nthw_stat_inf_skip_cnt(be->p_flm_nthw, &flm->v25.inf_skip->cnt, 1);
		flm_nthw_stat_pck_hit_cnt(be->p_flm_nthw, &flm->v25.pck_hit->cnt, 1);
		flm_nthw_stat_pck_miss_cnt(be->p_flm_nthw, &flm->v25.pck_miss->cnt, 1);
		flm_nthw_stat_pck_unh_cnt(be->p_flm_nthw, &flm->v25.pck_unh->cnt, 1);
		flm_nthw_stat_pck_dis_cnt(be->p_flm_nthw, &flm->v25.pck_dis->cnt, 1);
		flm_nthw_stat_csh_hit_cnt(be->p_flm_nthw, &flm->v25.csh_hit->cnt, 1);
		flm_nthw_stat_csh_miss_cnt(be->p_flm_nthw, &flm->v25.csh_miss->cnt, 1);
		flm_nthw_stat_csh_unh_cnt(be->p_flm_nthw, &flm->v25.csh_unh->cnt, 1);
		flm_nthw_stat_cuc_start_cnt(be->p_flm_nthw, &flm->v25.cuc_start->cnt, 1);
		flm_nthw_stat_cuc_move_cnt(be->p_flm_nthw, &flm->v25.cuc_move->cnt, 1);
	}

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return 0;
}

static int flm_lrn_data_flush(void *be_dev, const struct flm_func_s *flm, const uint32_t *lrn_data,
	uint32_t records, uint32_t *handled_records,
	uint32_t words_per_record, uint32_t *inf_word_cnt,
	uint32_t *sta_word_cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	int ret = flm_nthw_lrn_data_flush(be->p_flm_nthw, lrn_data, records, words_per_record,
			handled_records, &flm->v25.buf_ctrl->lrn_free,
			&flm->v25.buf_ctrl->inf_avail,
			&flm->v25.buf_ctrl->sta_avail);

	*inf_word_cnt = flm->v25.buf_ctrl->inf_avail;
	*sta_word_cnt = flm->v25.buf_ctrl->sta_avail;

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return ret;
}

static int flm_inf_sta_data_update(void *be_dev, const struct flm_func_s *flm, uint32_t *inf_data,
	uint32_t inf_size, uint32_t *inf_word_cnt, uint32_t *sta_data,
	uint32_t sta_size, uint32_t *sta_word_cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, flm, be->p_flm_nthw);

	int ret = flm_nthw_inf_sta_data_update(be->p_flm_nthw, inf_data, inf_size, sta_data,
			sta_size, &flm->v25.buf_ctrl->lrn_free,
			&flm->v25.buf_ctrl->inf_avail,
			&flm->v25.buf_ctrl->sta_avail);

	*inf_word_cnt = flm->v25.buf_ctrl->inf_avail;
	*sta_word_cnt = flm->v25.buf_ctrl->sta_avail;

	CHECK_DEBUG_OFF(flm, be->p_flm_nthw);
	return ret;
}

/*
 * HSH
 */

static bool hsh_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_hsh_nthw != NULL;
}

static uint32_t hsh_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_hsh_nthw->m_hsh) << 16) |
			(nthw_module_get_minor_version(be->p_hsh_nthw->m_hsh) & 0xffff));
}

static int hsh_rcp_flush(void *be_dev, const struct hsh_func_s *hsh, int category, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, hsh, be->p_hsh_nthw);

	if (hsh->ver == 5) {
		hsh_nthw_rcp_cnt(be->p_hsh_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			hsh_nthw_rcp_select(be->p_hsh_nthw, category + i);
			hsh_nthw_rcp_load_dist_type(be->p_hsh_nthw,
				hsh->v5.rcp[category + i].load_dist_type);
			hsh_nthw_rcp_mac_port_mask(be->p_hsh_nthw,
				hsh->v5.rcp[category + i].mac_port_mask);
			hsh_nthw_rcp_sort(be->p_hsh_nthw, hsh->v5.rcp[category + i].sort);
			hsh_nthw_rcp_qw0_pe(be->p_hsh_nthw, hsh->v5.rcp[category + i].qw0_pe);
			hsh_nthw_rcp_qw0_ofs(be->p_hsh_nthw, hsh->v5.rcp[category + i].qw0_ofs);
			hsh_nthw_rcp_qw4_pe(be->p_hsh_nthw, hsh->v5.rcp[category + i].qw4_pe);
			hsh_nthw_rcp_qw4_ofs(be->p_hsh_nthw, hsh->v5.rcp[category + i].qw4_ofs);
			hsh_nthw_rcp_w8_pe(be->p_hsh_nthw, hsh->v5.rcp[category + i].w8_pe);
			hsh_nthw_rcp_w8_ofs(be->p_hsh_nthw, hsh->v5.rcp[category + i].w8_ofs);
			hsh_nthw_rcp_w8_sort(be->p_hsh_nthw, hsh->v5.rcp[category + i].w8_sort);
			hsh_nthw_rcp_w9_pe(be->p_hsh_nthw, hsh->v5.rcp[category + i].w9_pe);
			hsh_nthw_rcp_w9_ofs(be->p_hsh_nthw, hsh->v5.rcp[category + i].w9_ofs);
			hsh_nthw_rcp_w9_sort(be->p_hsh_nthw, hsh->v5.rcp[category + i].w9_sort);
			hsh_nthw_rcp_w9_p(be->p_hsh_nthw, hsh->v5.rcp[category + i].w9_p);
			hsh_nthw_rcp_p_mask(be->p_hsh_nthw, hsh->v5.rcp[category + i].p_mask);
			hsh_nthw_rcp_word_mask(be->p_hsh_nthw,
				hsh->v5.rcp[category + i].word_mask);
			hsh_nthw_rcp_seed(be->p_hsh_nthw, hsh->v5.rcp[category + i].seed);
			hsh_nthw_rcp_tnl_p(be->p_hsh_nthw, hsh->v5.rcp[category + i].tnl_p);
			hsh_nthw_rcp_hsh_valid(be->p_hsh_nthw,
				hsh->v5.rcp[category + i].hsh_valid);
			hsh_nthw_rcp_hsh_type(be->p_hsh_nthw, hsh->v5.rcp[category + i].hsh_type);
			hsh_nthw_rcp_toeplitz(be->p_hsh_nthw, hsh->v5.rcp[category + i].toeplitz);
			hsh_nthw_rcp_k(be->p_hsh_nthw, hsh->v5.rcp[category + i].k);
			hsh_nthw_rcp_auto_ipv4_mask(be->p_hsh_nthw,
				hsh->v5.rcp[category + i].auto_ipv4_mask);
			hsh_nthw_rcp_flush(be->p_hsh_nthw);
		}
	}

	CHECK_DEBUG_OFF(hsh, be->p_hsh_nthw);
	return 0;
}

/*
 * QSL
 */

static bool qsl_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_qsl_nthw != NULL;
}

static uint32_t qsl_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_qsl_nthw->m_qsl) << 16) |
			(nthw_module_get_minor_version(be->p_qsl_nthw->m_qsl) & 0xffff));
}

static int qsl_rcp_flush(void *be_dev, const struct qsl_func_s *qsl, int category, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, qsl, be->p_qsl_nthw);

	if (qsl->ver == 7) {
		qsl_nthw_rcp_cnt(be->p_qsl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			qsl_nthw_rcp_select(be->p_qsl_nthw, category + i);
			qsl_nthw_rcp_discard(be->p_qsl_nthw, qsl->v7.rcp[category + i].discard);
			qsl_nthw_rcp_drop(be->p_qsl_nthw, qsl->v7.rcp[category + i].drop);
			qsl_nthw_rcp_tbl_lo(be->p_qsl_nthw, qsl->v7.rcp[category + i].tbl_lo);
			qsl_nthw_rcp_tbl_hi(be->p_qsl_nthw, qsl->v7.rcp[category + i].tbl_hi);
			qsl_nthw_rcp_tbl_idx(be->p_qsl_nthw, qsl->v7.rcp[category + i].tbl_idx);
			qsl_nthw_rcp_tbl_msk(be->p_qsl_nthw, qsl->v7.rcp[category + i].tbl_msk);
			qsl_nthw_rcp_lr(be->p_qsl_nthw, qsl->v7.rcp[category + i].lr);
			qsl_nthw_rcp_tsa(be->p_qsl_nthw, qsl->v7.rcp[category + i].tsa);
			qsl_nthw_rcp_vli(be->p_qsl_nthw, qsl->v7.rcp[category + i].vli);
			qsl_nthw_rcp_flush(be->p_qsl_nthw);
		}
	}

	CHECK_DEBUG_OFF(qsl, be->p_qsl_nthw);
	return 0;
}

static int qsl_qst_flush(void *be_dev, const struct qsl_func_s *qsl, int entry, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, qsl, be->p_qsl_nthw);

	if (qsl->ver == 7) {
		qsl_nthw_qst_cnt(be->p_qsl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			qsl_nthw_qst_select(be->p_qsl_nthw, entry + i);
			qsl_nthw_qst_queue(be->p_qsl_nthw, qsl->v7.qst[entry + i].queue);
			qsl_nthw_qst_en(be->p_qsl_nthw, qsl->v7.qst[entry + i].en);

			qsl_nthw_qst_tx_port(be->p_qsl_nthw, qsl->v7.qst[entry + i].tx_port);
			qsl_nthw_qst_lre(be->p_qsl_nthw, qsl->v7.qst[entry + i].lre);
			qsl_nthw_qst_tci(be->p_qsl_nthw, qsl->v7.qst[entry + i].tci);
			qsl_nthw_qst_ven(be->p_qsl_nthw, qsl->v7.qst[entry + i].ven);
			qsl_nthw_qst_flush(be->p_qsl_nthw);
		}
	}

	CHECK_DEBUG_OFF(qsl, be->p_qsl_nthw);
	return 0;
}

static int qsl_qen_flush(void *be_dev, const struct qsl_func_s *qsl, int entry, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, qsl, be->p_qsl_nthw);

	if (qsl->ver == 7) {
		qsl_nthw_qen_cnt(be->p_qsl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			qsl_nthw_qen_select(be->p_qsl_nthw, entry + i);
			qsl_nthw_qen_en(be->p_qsl_nthw, qsl->v7.qen[entry + i].en);
			qsl_nthw_qen_flush(be->p_qsl_nthw);
		}
	}

	CHECK_DEBUG_OFF(qsl, be->p_qsl_nthw);
	return 0;
}

static int qsl_unmq_flush(void *be_dev, const struct qsl_func_s *qsl, int entry, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, qsl, be->p_qsl_nthw);

	if (qsl->ver == 7) {
		qsl_nthw_unmq_cnt(be->p_qsl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			qsl_nthw_unmq_select(be->p_qsl_nthw, entry + i);
			qsl_nthw_unmq_dest_queue(be->p_qsl_nthw,
				qsl->v7.unmq[entry + i].dest_queue);
			qsl_nthw_unmq_en(be->p_qsl_nthw, qsl->v7.unmq[entry + i].en);
			qsl_nthw_unmq_flush(be->p_qsl_nthw);
		}
	}

	CHECK_DEBUG_OFF(qsl, be->p_qsl_nthw);
	return 0;
}

/*
 * SLC LR
 */

static bool slc_lr_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_slc_lr_nthw != NULL;
}

static uint32_t slc_lr_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_slc_lr_nthw->m_slc_lr) << 16) |
			(nthw_module_get_minor_version(be->p_slc_lr_nthw->m_slc_lr) & 0xffff));
}

static int slc_lr_rcp_flush(void *be_dev, const struct slc_lr_func_s *slc_lr, int category,
	int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, slc_lr, be->p_slc_lr_nthw);

	if (slc_lr->ver == 2) {
		slc_lr_nthw_rcp_cnt(be->p_slc_lr_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			slc_lr_nthw_rcp_select(be->p_slc_lr_nthw, category + i);
			slc_lr_nthw_rcp_head_slc_en(be->p_slc_lr_nthw,
				slc_lr->v2.rcp[category + i].head_slc_en);
			slc_lr_nthw_rcp_head_dyn(be->p_slc_lr_nthw,
				slc_lr->v2.rcp[category + i].head_dyn);
			slc_lr_nthw_rcp_head_ofs(be->p_slc_lr_nthw,
				slc_lr->v2.rcp[category + i].head_ofs);
			slc_lr_nthw_rcp_tail_slc_en(be->p_slc_lr_nthw,
				slc_lr->v2.rcp[category + i].tail_slc_en);
			slc_lr_nthw_rcp_tail_dyn(be->p_slc_lr_nthw,
				slc_lr->v2.rcp[category + i].tail_dyn);
			slc_lr_nthw_rcp_tail_ofs(be->p_slc_lr_nthw,
				slc_lr->v2.rcp[category + i].tail_ofs);
			slc_lr_nthw_rcp_pcap(be->p_slc_lr_nthw, slc_lr->v2.rcp[category + i].pcap);
			slc_lr_nthw_rcp_flush(be->p_slc_lr_nthw);
		}
	}

	CHECK_DEBUG_OFF(slc_lr, be->p_slc_lr_nthw);
	return 0;
}

/*
 * PDB
 */

static bool pdb_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_pdb_nthw != NULL;
}

static uint32_t pdb_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return (uint32_t)((nthw_module_get_major_version(be->p_pdb_nthw->m_pdb) << 16) |
			(nthw_module_get_minor_version(be->p_pdb_nthw->m_pdb) & 0xffff));
}

static int pdb_rcp_flush(void *be_dev, const struct pdb_func_s *pdb, int category, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, pdb, be->p_pdb_nthw);

	if (pdb->ver == 9) {
		pdb_nthw_rcp_cnt(be->p_pdb_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			pdb_nthw_rcp_select(be->p_pdb_nthw, category + i);
			pdb_nthw_rcp_descriptor(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].descriptor);
			pdb_nthw_rcp_desc_len(be->p_pdb_nthw, pdb->v9.rcp[category + i].desc_len);
			pdb_nthw_rcp_tx_port(be->p_pdb_nthw, pdb->v9.rcp[category + i].tx_port);
			pdb_nthw_rcp_tx_ignore(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].tx_ignore);
			pdb_nthw_rcp_tx_now(be->p_pdb_nthw, pdb->v9.rcp[category + i].tx_now);
			pdb_nthw_rcp_crc_overwrite(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].crc_overwrite);
			pdb_nthw_rcp_align(be->p_pdb_nthw, pdb->v9.rcp[category + i].align);
			pdb_nthw_rcp_ofs0_dyn(be->p_pdb_nthw, pdb->v9.rcp[category + i].ofs0_dyn);
			pdb_nthw_rcp_ofs0_rel(be->p_pdb_nthw, pdb->v9.rcp[category + i].ofs0_rel);
			pdb_nthw_rcp_ofs1_dyn(be->p_pdb_nthw, pdb->v9.rcp[category + i].ofs1_dyn);
			pdb_nthw_rcp_ofs1_rel(be->p_pdb_nthw, pdb->v9.rcp[category + i].ofs1_rel);
			pdb_nthw_rcp_ofs2_dyn(be->p_pdb_nthw, pdb->v9.rcp[category + i].ofs2_dyn);
			pdb_nthw_rcp_ofs2_rel(be->p_pdb_nthw, pdb->v9.rcp[category + i].ofs2_rel);
			pdb_nthw_rcp_ip_prot_tnl(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].ip_prot_tnl);
			pdb_nthw_rcp_ppc_hsh(be->p_pdb_nthw, pdb->v9.rcp[category + i].ppc_hsh);
			pdb_nthw_rcp_duplicate_en(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].duplicate_en);
			pdb_nthw_rcp_duplicate_bit(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].duplicate_bit);
			pdb_nthw_rcp_duplicate_bit(be->p_pdb_nthw,
				pdb->v9.rcp[category + i].pcap_keep_fcs);
			pdb_nthw_rcp_flush(be->p_pdb_nthw);
		}
	}

	CHECK_DEBUG_OFF(pdb, be->p_pdb_nthw);
	return 0;
}

static int pdb_config_flush(void *be_dev, const struct pdb_func_s *pdb)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, pdb, be->p_pdb_nthw);

	if (pdb->ver == 9) {
		pdb_nthw_config_ts_format(be->p_pdb_nthw, pdb->v9.config->ts_format);
		pdb_nthw_config_port_ofs(be->p_pdb_nthw, pdb->v9.config->port_ofs);
		pdb_nthw_config_flush(be->p_pdb_nthw);
	}

	CHECK_DEBUG_OFF(pdb, be->p_pdb_nthw);
	return 0;
}

/*
 * TPE
 */

static bool tpe_get_present(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return be->p_csu_nthw != NULL && be->p_hfu_nthw != NULL && be->p_rpp_lr_nthw != NULL &&
		be->p_ifr_nthw != NULL && be->p_tx_cpy_nthw != NULL && be->p_tx_ins_nthw != NULL &&
		be->p_tx_rpl_nthw != NULL;
}

static uint32_t tpe_get_version(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;

	const uint32_t csu_version =
		(uint32_t)((nthw_module_get_major_version(be->p_csu_nthw->m_csu) << 16) |
			(nthw_module_get_minor_version(be->p_csu_nthw->m_csu) & 0xffff));

	const uint32_t hfu_version =
		(uint32_t)((nthw_module_get_major_version(be->p_hfu_nthw->m_hfu) << 16) |
			(nthw_module_get_minor_version(be->p_hfu_nthw->m_hfu) & 0xffff));

	const uint32_t rpp_lr_version =
		(uint32_t)((nthw_module_get_major_version(be->p_rpp_lr_nthw->m_rpp_lr) << 16) |
			(nthw_module_get_minor_version(be->p_rpp_lr_nthw->m_rpp_lr) & 0xffff));

	const uint32_t tx_cpy_version =
		(uint32_t)((nthw_module_get_major_version(be->p_tx_cpy_nthw->m_tx_cpy) << 16) |
			(nthw_module_get_minor_version(be->p_tx_cpy_nthw->m_tx_cpy) & 0xffff));

	const uint32_t tx_ins_version =
		(uint32_t)((nthw_module_get_major_version(be->p_tx_ins_nthw->m_tx_ins) << 16) |
			(nthw_module_get_minor_version(be->p_tx_ins_nthw->m_tx_ins) & 0xffff));

	const uint32_t tx_rpl_version =
		(uint32_t)((nthw_module_get_major_version(be->p_tx_rpl_nthw->m_tx_rpl) << 16) |
			(nthw_module_get_minor_version(be->p_tx_rpl_nthw->m_tx_rpl) & 0xffff));

	/*
	 * we have to support 9563-55-28 and 9563-55-30
	 * so check for INS ver 0.1 and RPL ver 0.2 or for INS ver 0.2 and RPL ver 0.4
	 */
	if (csu_version == 0 && hfu_version == 2 && rpp_lr_version >= 1 && tx_cpy_version == 2 &&
		((tx_ins_version == 1 && tx_rpl_version == 2) ||
			(tx_ins_version == 2 && tx_rpl_version == 4))) {
		return 3;
	}

	if (csu_version == 0 && hfu_version == 2 && rpp_lr_version >= 1 && tx_cpy_version == 4 &&
		((tx_ins_version == 1 && tx_rpl_version == 2) ||
			(tx_ins_version == 2 && tx_rpl_version == 4))) {
		return 3;
	}

	RTE_ASSERT(false);
	return 0;
}

static int tpe_rpp_rcp_flush(void *be_dev, const struct tpe_func_s *rpp_lr, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, rpp_lr, be->p_rpp_lr_nthw);

	if (rpp_lr->ver >= 1) {
		rpp_lr_nthw_rcp_cnt(be->p_rpp_lr_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			rpp_lr_nthw_rcp_select(be->p_rpp_lr_nthw, index + i);
			rpp_lr_nthw_rcp_exp(be->p_rpp_lr_nthw, rpp_lr->v3.rpp_rcp[index + i].exp);
			rpp_lr_nthw_rcp_flush(be->p_rpp_lr_nthw);
		}
	}

	CHECK_DEBUG_OFF(rpp_lr, be->p_rpp_lr_nthw);
	return 0;
}

static int tpe_rpp_ifr_rcp_flush(void *be_dev, const struct tpe_func_s *rpp_lr, int index, int cnt)
{
	int res = 0;
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, rpp_lr, be->p_rpp_lr_nthw);

	if (rpp_lr->ver >= 2) {
		rpp_lr_nthw_ifr_rcp_cnt(be->p_rpp_lr_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			rpp_lr_nthw_ifr_rcp_select(be->p_rpp_lr_nthw, index + i);
			rpp_lr_nthw_ifr_rcp_ipv4_en(be->p_rpp_lr_nthw,
				rpp_lr->v3.rpp_ifr_rcp[index + i].ipv4_en);
			rpp_lr_nthw_ifr_rcp_ipv4_df_drop(be->p_rpp_lr_nthw,
				rpp_lr->v3.rpp_ifr_rcp[index + i]
				.ipv4_df_drop);
			rpp_lr_nthw_ifr_rcp_ipv6_en(be->p_rpp_lr_nthw,
				rpp_lr->v3.rpp_ifr_rcp[index + i].ipv6_en);
			rpp_lr_nthw_ifr_rcp_ipv6_drop(be->p_rpp_lr_nthw,
				rpp_lr->v3.rpp_ifr_rcp[index + i].ipv6_drop);
			rpp_lr_nthw_ifr_rcp_mtu(be->p_rpp_lr_nthw,
				rpp_lr->v3.rpp_ifr_rcp[index + i].mtu);
			rpp_lr_nthw_ifr_rcp_flush(be->p_rpp_lr_nthw);
		}

	} else {
		res = -1;
	}

	CHECK_DEBUG_OFF(rpp_lr, be->p_rpp_lr_nthw);
	return res;
}

static int tpe_ifr_rcp_flush(void *be_dev, const struct tpe_func_s *ifr, int index, int cnt)
{
	int res = 0;
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, ifr, be->p_ifr_nthw);

	if (ifr->ver >= 2) {
		ifr_nthw_rcp_cnt(be->p_ifr_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			ifr_nthw_rcp_select(be->p_ifr_nthw, index + i);
			ifr_nthw_rcp_ipv4_en(be->p_ifr_nthw, ifr->v3.ifr_rcp[index + i].ipv4_en);
			ifr_nthw_rcp_ipv4_df_drop(be->p_ifr_nthw,
				ifr->v3.ifr_rcp[index + i].ipv4_df_drop);
			ifr_nthw_rcp_ipv6_en(be->p_ifr_nthw, ifr->v3.ifr_rcp[index + i].ipv6_en);
			ifr_nthw_rcp_ipv6_drop(be->p_ifr_nthw,
				ifr->v3.ifr_rcp[index + i].ipv6_drop);
			ifr_nthw_rcp_mtu(be->p_ifr_nthw, ifr->v3.ifr_rcp[index + i].mtu);
			ifr_nthw_rcp_flush(be->p_ifr_nthw);
		}

	} else {
		res = -1;
	}

	CHECK_DEBUG_OFF(ifr, be->p_ifr_nthw);
	return res;
}

static int tpe_ifr_counters_update(void *be_dev, const struct tpe_func_s *ifr, int index, int cnt)
{
	int res = 0;
	int i = 0;
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, ifr, be->p_ifr_nthw);

	if (ifr->ver >= 2) {
		ifr_nthw_counters_cnt(be->p_ifr_nthw, 1);

		for (i = 0; i < cnt; i++) {
			ifr_nthw_counters_select(be->p_ifr_nthw, index + i);
			ifr_nthw_counters_update(be->p_ifr_nthw);
			ifr_nthw_counters_drop(be->p_ifr_nthw,
				&ifr->v3.ifr_counters[index + i].drop, 1);
		}

	} else {
		res = -1;
	}

	CHECK_DEBUG_OFF(ifr, be->p_ifr_nthw);
	return res;
}

static int tpe_ins_rcp_flush(void *be_dev, const struct tpe_func_s *tx_ins, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, tx_ins, be->p_tx_ins_nthw);

	if (tx_ins->ver >= 1) {
		tx_ins_nthw_rcp_cnt(be->p_tx_ins_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			tx_ins_nthw_rcp_select(be->p_tx_ins_nthw, index + i);
			tx_ins_nthw_rcp_dyn(be->p_tx_ins_nthw, tx_ins->v3.ins_rcp[index + i].dyn);
			tx_ins_nthw_rcp_ofs(be->p_tx_ins_nthw, tx_ins->v3.ins_rcp[index + i].ofs);
			tx_ins_nthw_rcp_len(be->p_tx_ins_nthw, tx_ins->v3.ins_rcp[index + i].len);
			tx_ins_nthw_rcp_flush(be->p_tx_ins_nthw);
		}
	}

	CHECK_DEBUG_OFF(tx_ins, be->p_tx_ins_nthw);
	return 0;
}

static int tpe_rpl_rcp_flush(void *be_dev, const struct tpe_func_s *tx_rpl, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, tx_rpl, be->p_tx_rpl_nthw);

	if (tx_rpl->ver >= 1) {
		tx_rpl_nthw_rcp_cnt(be->p_tx_rpl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			tx_rpl_nthw_rcp_select(be->p_tx_rpl_nthw, index + i);
			tx_rpl_nthw_rcp_dyn(be->p_tx_rpl_nthw, tx_rpl->v3.rpl_rcp[index + i].dyn);
			tx_rpl_nthw_rcp_ofs(be->p_tx_rpl_nthw, tx_rpl->v3.rpl_rcp[index + i].ofs);
			tx_rpl_nthw_rcp_len(be->p_tx_rpl_nthw, tx_rpl->v3.rpl_rcp[index + i].len);
			tx_rpl_nthw_rcp_rpl_ptr(be->p_tx_rpl_nthw,
				tx_rpl->v3.rpl_rcp[index + i].rpl_ptr);
			tx_rpl_nthw_rcp_ext_prio(be->p_tx_rpl_nthw,
				tx_rpl->v3.rpl_rcp[index + i].ext_prio);

			if (tx_rpl->ver >= 3) {
				tx_rpl_nthw_rcp_eth_type_wr(be->p_tx_rpl_nthw,
					tx_rpl->v3.rpl_rcp[index + i]
					.eth_type_wr);
			}

			tx_rpl_nthw_rcp_flush(be->p_tx_rpl_nthw);
		}
	}

	CHECK_DEBUG_OFF(tx_rpl, be->p_tx_rpl_nthw);
	return 0;
}

static int tpe_rpl_ext_flush(void *be_dev, const struct tpe_func_s *tx_rpl, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, tx_rpl, be->p_tx_rpl_nthw);

	if (tx_rpl->ver >= 1) {
		tx_rpl_nthw_ext_cnt(be->p_tx_rpl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			tx_rpl_nthw_ext_select(be->p_tx_rpl_nthw, index + i);
			tx_rpl_nthw_ext_rpl_ptr(be->p_tx_rpl_nthw,
				tx_rpl->v3.rpl_ext[index + i].rpl_ptr);
			tx_rpl_nthw_ext_flush(be->p_tx_rpl_nthw);
		}
	}

	CHECK_DEBUG_OFF(tx_rpl, be->p_tx_rpl_nthw);
	return 0;
}

static int tpe_rpl_rpl_flush(void *be_dev, const struct tpe_func_s *tx_rpl, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, tx_rpl, be->p_tx_rpl_nthw);

	if (tx_rpl->ver >= 1) {
		tx_rpl_nthw_rpl_cnt(be->p_tx_rpl_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			tx_rpl_nthw_rpl_select(be->p_tx_rpl_nthw, index + i);
			tx_rpl_nthw_rpl_value(be->p_tx_rpl_nthw,
				tx_rpl->v3.rpl_rpl[index + i].value);
			tx_rpl_nthw_rpl_flush(be->p_tx_rpl_nthw);
		}
	}

	CHECK_DEBUG_OFF(tx_rpl, be->p_tx_rpl_nthw);
	return 0;
}

static int tpe_cpy_rcp_flush(void *be_dev, const struct tpe_func_s *tx_cpy, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	unsigned int wr_index = -1;

	CHECK_DEBUG_ON(be, tx_cpy, be->p_tx_cpy_nthw);

	if (tx_cpy->ver >= 1) {
		for (int i = 0; i < cnt; i++) {
			if (wr_index != (index + i) / tx_cpy->nb_rcp_categories) {
				wr_index = (index + i) / tx_cpy->nb_rcp_categories;
				tx_cpy_nthw_writer_cnt(be->p_tx_cpy_nthw, wr_index, 1);
			}

			tx_cpy_nthw_writer_select(be->p_tx_cpy_nthw, wr_index,
				(index + i) % tx_cpy->nb_rcp_categories);
			tx_cpy_nthw_writer_reader_select(be->p_tx_cpy_nthw, wr_index,
				tx_cpy->v3.cpy_rcp[index + i]
				.reader_select);
			tx_cpy_nthw_writer_dyn(be->p_tx_cpy_nthw, wr_index,
				tx_cpy->v3.cpy_rcp[index + i].dyn);
			tx_cpy_nthw_writer_ofs(be->p_tx_cpy_nthw, wr_index,
				tx_cpy->v3.cpy_rcp[index + i].ofs);
			tx_cpy_nthw_writer_len(be->p_tx_cpy_nthw, wr_index,
				tx_cpy->v3.cpy_rcp[index + i].len);
			tx_cpy_nthw_writer_flush(be->p_tx_cpy_nthw, wr_index);
		}
	}

	CHECK_DEBUG_OFF(tx_cpy, be->p_tx_cpy_nthw);
	return 0;
}

static int tpe_hfu_rcp_flush(void *be_dev, const struct tpe_func_s *hfu, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, hfu, be->p_hfu_nthw);

	if (hfu->ver >= 1) {
		hfu_nthw_rcp_cnt(be->p_hfu_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			hfu_nthw_rcp_select(be->p_hfu_nthw, index + i);
			hfu_nthw_rcp_len_a_wr(be->p_hfu_nthw, hfu->v3.hfu_rcp[index + i].len_a_wr);
			hfu_nthw_rcp_len_a_ol4len(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_a_outer_l4_len);
			hfu_nthw_rcp_len_a_pos_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_a_pos_dyn);
			hfu_nthw_rcp_len_a_pos_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_a_pos_ofs);
			hfu_nthw_rcp_len_a_add_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_a_add_dyn);
			hfu_nthw_rcp_len_a_add_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_a_add_ofs);
			hfu_nthw_rcp_len_a_sub_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_a_sub_dyn);
			hfu_nthw_rcp_len_b_wr(be->p_hfu_nthw, hfu->v3.hfu_rcp[index + i].len_b_wr);
			hfu_nthw_rcp_len_b_pos_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_b_pos_dyn);
			hfu_nthw_rcp_len_b_pos_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_b_pos_ofs);
			hfu_nthw_rcp_len_b_add_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_b_add_dyn);
			hfu_nthw_rcp_len_b_add_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_b_add_ofs);
			hfu_nthw_rcp_len_b_sub_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_b_sub_dyn);
			hfu_nthw_rcp_len_c_wr(be->p_hfu_nthw, hfu->v3.hfu_rcp[index + i].len_c_wr);
			hfu_nthw_rcp_len_c_pos_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_c_pos_dyn);
			hfu_nthw_rcp_len_c_pos_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_c_pos_ofs);
			hfu_nthw_rcp_len_c_add_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_c_add_dyn);
			hfu_nthw_rcp_len_c_add_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_c_add_ofs);
			hfu_nthw_rcp_len_c_sub_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].len_c_sub_dyn);
			hfu_nthw_rcp_ttl_wr(be->p_hfu_nthw, hfu->v3.hfu_rcp[index + i].ttl_wr);
			hfu_nthw_rcp_ttl_pos_dyn(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].ttl_pos_dyn);
			hfu_nthw_rcp_ttl_pos_ofs(be->p_hfu_nthw,
				hfu->v3.hfu_rcp[index + i].ttl_pos_ofs);
			hfu_nthw_rcp_flush(be->p_hfu_nthw);
		}
	}

	CHECK_DEBUG_OFF(hfu, be->p_hfu_nthw);
	return 0;
}

static int tpe_csu_rcp_flush(void *be_dev, const struct tpe_func_s *csu, int index, int cnt)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	CHECK_DEBUG_ON(be, csu, be->p_csu_nthw);

	if (csu->ver >= 1) {
		csu_nthw_rcp_cnt(be->p_csu_nthw, 1);

		for (int i = 0; i < cnt; i++) {
			csu_nthw_rcp_select(be->p_csu_nthw, index + i);
			csu_nthw_rcp_outer_l3_cmd(be->p_csu_nthw,
				csu->v3.csu_rcp[index + i].ol3_cmd);
			csu_nthw_rcp_outer_l4_cmd(be->p_csu_nthw,
				csu->v3.csu_rcp[index + i].ol4_cmd);
			csu_nthw_rcp_inner_l3_cmd(be->p_csu_nthw,
				csu->v3.csu_rcp[index + i].il3_cmd);
			csu_nthw_rcp_inner_l4_cmd(be->p_csu_nthw,
				csu->v3.csu_rcp[index + i].il4_cmd);
			csu_nthw_rcp_flush(be->p_csu_nthw);
		}
	}

	CHECK_DEBUG_OFF(csu, be->p_csu_nthw);
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

	km_get_present,
	km_get_version,
	km_rcp_flush,
	km_cam_flush,
	km_tcam_flush,
	km_tci_flush,
	km_tcq_flush,

	flm_get_present,
	flm_get_version,
	flm_control_flush,
	flm_status_flush,
	flm_status_update,
	flm_scan_flush,
	flm_load_bin_flush,
	flm_prio_flush,
	flm_pst_flush,
	flm_rcp_flush,
	flm_scrub_flush,
	flm_buf_ctrl_update,
	flm_stat_update,
	flm_lrn_data_flush,
	flm_inf_sta_data_update,

	hsh_get_present,
	hsh_get_version,
	hsh_rcp_flush,

	qsl_get_present,
	qsl_get_version,
	qsl_rcp_flush,
	qsl_qst_flush,
	qsl_qen_flush,
	qsl_unmq_flush,

	slc_lr_get_present,
	slc_lr_get_version,
	slc_lr_rcp_flush,

	pdb_get_present,
	pdb_get_version,
	pdb_rcp_flush,
	pdb_config_flush,

	tpe_get_present,
	tpe_get_version,
	tpe_rpp_rcp_flush,
	tpe_rpp_ifr_rcp_flush,
	tpe_ifr_rcp_flush,
	tpe_ifr_counters_update,
	tpe_ins_rcp_flush,
	tpe_rpl_rcp_flush,
	tpe_rpl_ext_flush,
	tpe_rpl_rpl_flush,
	tpe_cpy_rcp_flush,
	tpe_hfu_rcp_flush,
	tpe_csu_rcp_flush,
};

const struct flow_api_backend_ops *nthw_bin_flow_backend_init(nthw_fpga_t *p_fpga, void **dev)
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

	/* Init nthw KM */
	if (km_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct km_nthw *pkmnthw = km_nthw_new();
		km_nthw_init(pkmnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_km_nthw = pkmnthw;

	} else {
		be_devs[physical_adapter_no].p_km_nthw = NULL;
	}

	/* Init nthw FLM */
	if (flm_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct flm_nthw *pflmnthw = flm_nthw_new();
		flm_nthw_init(pflmnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_flm_nthw = pflmnthw;

	} else {
		be_devs[physical_adapter_no].p_flm_nthw = NULL;
	}

	/* Init nthw IFR */
	if (ifr_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct ifr_nthw *ifrnthw = ifr_nthw_new();
		ifr_nthw_init(ifrnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_ifr_nthw = ifrnthw;

	} else {
		be_devs[physical_adapter_no].p_ifr_nthw = NULL;
	}

	/* Init nthw HSH */
	if (hsh_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct hsh_nthw *phshnthw = hsh_nthw_new();
		hsh_nthw_init(phshnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_hsh_nthw = phshnthw;

	} else {
		be_devs[physical_adapter_no].p_hsh_nthw = NULL;
	}

	/* Init nthw QSL */
	if (qsl_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct qsl_nthw *pqslnthw = qsl_nthw_new();
		qsl_nthw_init(pqslnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_qsl_nthw = pqslnthw;

	} else {
		be_devs[physical_adapter_no].p_qsl_nthw = NULL;
	}

	/* Init nthw SLC LR */
	if (slc_lr_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct slc_lr_nthw *pslclrnthw = slc_lr_nthw_new();
		slc_lr_nthw_init(pslclrnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_slc_lr_nthw = pslclrnthw;

	} else {
		be_devs[physical_adapter_no].p_slc_lr_nthw = NULL;
	}

	/* Init nthw PDB */
	if (pdb_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct pdb_nthw *ppdbnthw = pdb_nthw_new();
		pdb_nthw_init(ppdbnthw, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_pdb_nthw = ppdbnthw;

	} else {
		be_devs[physical_adapter_no].p_pdb_nthw = NULL;
	}

	/* Init nthw HFU */
	if (hfu_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct hfu_nthw *ptr = hfu_nthw_new();
		hfu_nthw_init(ptr, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_hfu_nthw = ptr;

	} else {
		be_devs[physical_adapter_no].p_hfu_nthw = NULL;
	}

	/* Init nthw RPP_LR */
	if (rpp_lr_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct rpp_lr_nthw *ptr = rpp_lr_nthw_new();
		rpp_lr_nthw_init(ptr, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_rpp_lr_nthw = ptr;

	} else {
		be_devs[physical_adapter_no].p_rpp_lr_nthw = NULL;
	}

	/* Init nthw TX_CPY */
	if (tx_cpy_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct tx_cpy_nthw *ptr = tx_cpy_nthw_new();
		tx_cpy_nthw_init(ptr, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_tx_cpy_nthw = ptr;

	} else {
		be_devs[physical_adapter_no].p_tx_cpy_nthw = NULL;
	}

	/* Init nthw CSU */
	if (csu_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct csu_nthw *ptr = csu_nthw_new();
		csu_nthw_init(ptr, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_csu_nthw = ptr;

	} else {
		be_devs[physical_adapter_no].p_csu_nthw = NULL;
	}

	/* Init nthw TX_INS */
	if (tx_ins_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct tx_ins_nthw *ptr = tx_ins_nthw_new();
		tx_ins_nthw_init(ptr, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_tx_ins_nthw = ptr;

	} else {
		be_devs[physical_adapter_no].p_tx_ins_nthw = NULL;
	}

	/* Init nthw TX_RPL */
	if (tx_rpl_nthw_init(NULL, p_fpga, physical_adapter_no) == 0) {
		struct tx_rpl_nthw *ptr = tx_rpl_nthw_new();
		tx_rpl_nthw_init(ptr, p_fpga, physical_adapter_no);
		be_devs[physical_adapter_no].p_tx_rpl_nthw = ptr;

	} else {
		be_devs[physical_adapter_no].p_tx_rpl_nthw = NULL;
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
	km_nthw_delete(be_dev->p_km_nthw);
	flm_nthw_delete(be_dev->p_flm_nthw);
	hsh_nthw_delete(be_dev->p_hsh_nthw);
	qsl_nthw_delete(be_dev->p_qsl_nthw);
	slc_lr_nthw_delete(be_dev->p_slc_lr_nthw);
	pdb_nthw_delete(be_dev->p_pdb_nthw);
	csu_nthw_delete(be_dev->p_csu_nthw);
	hfu_nthw_delete(be_dev->p_hfu_nthw);
	rpp_lr_nthw_delete(be_dev->p_rpp_lr_nthw);
	tx_cpy_nthw_delete(be_dev->p_tx_cpy_nthw);
	tx_ins_nthw_delete(be_dev->p_tx_ins_nthw);
	tx_rpl_nthw_delete(be_dev->p_tx_rpl_nthw);
}

static const struct flow_backend_ops ops = {
	.nthw_bin_flow_backend_init = nthw_bin_flow_backend_init,
	.bin_flow_backend_done = bin_flow_backend_done,
};

void nthw_flow_backend_init(void)
{
	register_flow_backend_ops(&ops);
}
