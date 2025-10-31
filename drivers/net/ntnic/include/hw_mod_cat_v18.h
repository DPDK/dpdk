/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_CAT_V18_H_
#define _HW_MOD_CAT_V18_H_

#include <stdint.h>

struct cat_v18_cfn_s {
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
	uint32_t km_or;
};

struct cat_v18_kce_s {
	uint32_t enable_bm;
};

struct cat_v18_kcs_s {
	uint32_t category;
};

struct cat_v18_fte_s {
	uint32_t enable_bm;
};

struct cat_v18_cte_s {
	union {
		uint32_t enable_bm;
		struct {
			uint32_t col : 1;
			uint32_t cor : 1;
			uint32_t hsh : 1;
			uint32_t qsl : 1;
			uint32_t ipf : 1;
			uint32_t slc : 1;
			uint32_t pdb : 1;
			uint32_t msk : 1;
			uint32_t hst : 1;
			uint32_t epp : 1;
			uint32_t tpe : 1;
		} b;
	};
};

struct cat_v18_cts_s {
	uint32_t cat_a;
	uint32_t cat_b;
};

struct cat_v18_cot_s {
	uint32_t color;
	uint32_t km;
};

struct cat_v18_cct_s {
	uint32_t color;
	uint32_t km;
};

struct cat_v18_exo_s {
	uint32_t dyn;
	int32_t ofs;
};

struct cat_v18_rck_s {
	uint32_t rck_data;
};

struct cat_v18_len_s {
	uint32_t lower;
	uint32_t upper;
	uint32_t dyn1;
	uint32_t dyn2;
	uint32_t inv;
};

struct cat_v18_kcc_s {
	uint32_t key[2];
	uint32_t category;
	uint32_t id;
};

struct hw_mod_cat_v18_s {
	struct cat_v18_cfn_s *cfn;
	struct cat_v18_kce_s *kce;
	struct cat_v18_kcs_s *kcs;
	struct cat_v18_fte_s *fte;
	struct cat_v18_cte_s *cte;
	struct cat_v18_cts_s *cts;
	struct cat_v18_cot_s *cot;
	struct cat_v18_cct_s *cct;
	struct cat_v18_exo_s *exo;
	struct cat_v18_rck_s *rck;
	struct cat_v18_len_s *len;
	struct cat_v18_kcc_s *kcc_cam;
};

#endif	/* _HW_MOD_CAT_V18_H_ */
