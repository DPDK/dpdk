/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_CAT_V21_H_
#define _HW_MOD_CAT_V21_H_

#include <stdint.h>

#include "hw_mod_cat_v18.h"

struct cat_v21_cfn_s {
	uint32_t enable;
	uint32_t inv;
	/* protocol checks */
	uint32_t ptc_inv;
	uint32_t ptc_isl;
	uint32_t ptc_cfp;
	uint32_t ptc_mac;
	uint32_t ptc_l2;
	uint32_t ptc_vntag;
	uint32_t ptc_vlan;
	uint32_t ptc_mpls;
	uint32_t ptc_l3;
	uint32_t ptc_frag;
	uint32_t ptc_ip_prot;
	uint32_t ptc_l4;
	uint32_t ptc_tunnel;
	uint32_t ptc_tnl_l2;
	uint32_t ptc_tnl_vlan;
	uint32_t ptc_tnl_mpls;
	uint32_t ptc_tnl_l3;
	uint32_t ptc_tnl_frag;
	uint32_t ptc_tnl_ip_prot;
	uint32_t ptc_tnl_l4;
	/* error checks */
	uint32_t err_inv;
	uint32_t err_cv;
	uint32_t err_fcs;
	uint32_t err_trunc;
	uint32_t err_l3_cs;
	uint32_t err_l4_cs;
	uint32_t err_tnl_l3_cs;
	uint32_t err_tnl_l4_cs;
	uint32_t err_ttl_exp;
	uint32_t err_tnl_ttl_exp;
	/* in port */
	uint32_t mac_port;
	/* pattern matcher */
	uint32_t pm_cmp[2];
	uint32_t pm_dct;
	uint32_t pm_ext_inv;
	uint32_t pm_cmb;
	uint32_t pm_and_inv;
	uint32_t pm_or_inv;
	uint32_t pm_inv;
	uint32_t lc;
	uint32_t lc_inv;
	uint32_t km0_or;
	uint32_t km1_or;
};

struct cat_v21_kce_s {
	uint32_t enable_bm[2];
};

struct cat_v21_kcs_s {
	uint32_t category[2];
};

struct cat_v21_fte_s {
	uint32_t enable_bm[2];
};

struct hw_mod_cat_v21_s {
	struct cat_v21_cfn_s *cfn;
	struct cat_v21_kce_s *kce;
	struct cat_v21_kcs_s *kcs;
	struct cat_v21_fte_s *fte;
	struct cat_v18_cte_s *cte;
	struct cat_v18_cts_s *cts;
	struct cat_v18_cot_s *cot;
	struct cat_v18_cct_s *cct;
	struct cat_v18_exo_s *exo;
	struct cat_v18_rck_s *rck;
	struct cat_v18_len_s *len;
	struct cat_v18_kcc_s *kcc_cam;
};

#endif	/* _HW_MOD_CAT_V21_H_ */
