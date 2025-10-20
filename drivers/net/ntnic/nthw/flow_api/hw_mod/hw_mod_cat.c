/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "CAT"
#define _VER_ be->cat.ver

static int hw_mod_cat_kce_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int km_if_id, int start_idx, int count);
static int hw_mod_cat_kcs_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int km_if_id, int start_idx, int count);
static int hw_mod_cat_fte_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int km_if_id, int start_idx, int count);

bool nthw_mod_cat_present(struct flow_api_backend_s *be)
{
	return be->iface->get_cat_present(be->be_dev);
}

int nthw_mod_cat_alloc(struct flow_api_backend_s *be)
{
	_VER_ = be->iface->get_cat_version(be->be_dev);
	NT_LOG(DBG, FILTER, "CAT MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	int nb = be->iface->get_nb_cat_funcs(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.nb_cat_funcs = (uint32_t)nb;

	nb = be->iface->get_nb_km_flow_types(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.nb_flow_types = (uint32_t)nb;

	nb = be->iface->get_nb_pm_ext(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.nb_pm_ext = (uint32_t)nb;

	nb = be->iface->get_nb_len(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.nb_len = (uint32_t)nb;

	nb = be->iface->get_kcc_size(be->be_dev);

	if (nb < 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.kcc_size = (uint32_t)nb;

	nb = be->iface->get_kcc_banks(be->be_dev);

	if (nb < 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.kcc_banks = (uint32_t)nb;

	nb = be->iface->get_nb_cat_km_if_cnt(be->be_dev);

	if (nb < 0) {
		COUNT_ERROR_LOG(cat_funcs);
		return COUNT_ERROR;
	}

	be->cat.km_if_count = (uint32_t)nb;

	int idx = be->iface->get_nb_cat_km_if_m0(be->be_dev);
	be->cat.km_if_m0 = idx;

	idx = be->iface->get_nb_cat_km_if_m1(be->be_dev);
	be->cat.km_if_m1 = idx;

	if (be->cat.kcc_banks)
		be->cat.kcc_records = be->cat.kcc_size / be->cat.kcc_banks;

	else
		be->cat.kcc_records = 0;

	be->cat.kcc_id_bit_size = 10;

	switch (_VER_) {
	case 18:
		be->cat.cts_num = 11;

		if (!nthw_callocate_mod((struct common_func_s *)&be->cat, 12, &be->cat.v18.cfn,
				be->cat.nb_cat_funcs, sizeof(struct cat_v18_cfn_s),
				&be->cat.v18.kce, (be->cat.nb_cat_funcs / 8),
				sizeof(struct cat_v18_kce_s), &be->cat.v18.kcs,
				be->cat.nb_cat_funcs, sizeof(struct cat_v18_kcs_s),
				&be->cat.v18.fte,
				(be->cat.nb_cat_funcs / 8) * be->cat.nb_flow_types * 2,
				sizeof(struct cat_v18_fte_s),

				&be->cat.v18.cte, be->cat.nb_cat_funcs,
				sizeof(struct cat_v18_cte_s), &be->cat.v18.cts,
				be->cat.nb_cat_funcs * ((be->cat.cts_num + 1) / 2),
				sizeof(struct cat_v18_cts_s), &be->cat.v18.cot,
				be->max_categories, sizeof(struct cat_v18_cot_s),

				&be->cat.v18.cct, be->max_categories * 4,
				sizeof(struct cat_v18_cct_s), &be->cat.v18.exo,
				be->cat.nb_pm_ext, sizeof(struct cat_v18_exo_s),
				&be->cat.v18.rck, be->cat.nb_pm_ext * 64,
				sizeof(struct cat_v18_rck_s), &be->cat.v18.len, be->cat.nb_len,
				sizeof(struct cat_v18_len_s), &be->cat.v18.kcc_cam,
				be->cat.kcc_size, sizeof(struct cat_v18_kcc_s)))
			return -1;

		break;

	/* end case 18 */
	case 21:
		be->cat.cts_num = 11;

		if (!nthw_callocate_mod((struct common_func_s *)&be->cat, 12, &be->cat.v21.cfn,
				be->cat.nb_cat_funcs, sizeof(struct cat_v21_cfn_s),
				&be->cat.v21.kce, (be->cat.nb_cat_funcs / 8),
				sizeof(struct cat_v21_kce_s), &be->cat.v21.kcs,
				be->cat.nb_cat_funcs, sizeof(struct cat_v21_kcs_s),
				&be->cat.v21.fte,
				(be->cat.nb_cat_funcs / 8) * be->cat.nb_flow_types * 4,
				sizeof(struct cat_v21_fte_s),

				&be->cat.v21.cte, be->cat.nb_cat_funcs,
				sizeof(struct cat_v18_cte_s), &be->cat.v21.cts,
				be->cat.nb_cat_funcs * ((be->cat.cts_num + 1) / 2),
				sizeof(struct cat_v18_cts_s), &be->cat.v21.cot,
				be->max_categories, sizeof(struct cat_v18_cot_s),

				&be->cat.v21.cct, be->max_categories * 4,
				sizeof(struct cat_v18_cct_s), &be->cat.v21.exo,
				be->cat.nb_pm_ext, sizeof(struct cat_v18_exo_s),
				&be->cat.v21.rck, be->cat.nb_pm_ext * 64,
				sizeof(struct cat_v18_rck_s), &be->cat.v21.len, be->cat.nb_len,
				sizeof(struct cat_v18_len_s), &be->cat.v21.kcc_cam,
				be->cat.kcc_size, sizeof(struct cat_v18_kcc_s)))
			return -1;

		break;

	/* end case 21 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void nthw_mod_cat_free(struct flow_api_backend_s *be)
{
	if (be->cat.base) {
		free(be->cat.base);
		be->cat.base = NULL;
	}
}

static int cfn_reset(struct flow_api_backend_s *be, int i)
{
	int err = nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PRESET_ALL, i, 0, 0);
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_ISL, i, 0,
		0xffffffff);	/* accept both ISL or not ISL */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_CFP, i, 0,
		0xffffffff);	/* accept both CFP or not CFP */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_MAC, i, 0, 0xffffffff);	/* accept all MACs */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_L2, i, 0, 0xffffffff);	/* accept all L2 prot */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_VNTAG, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_VLAN, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_MPLS, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_L3, i, 0, 0xffffffff);	/* accept all L3 prot */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_FRAG, i, 0, 0xffffffff); /* accept all fragments */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_IP_PROT, i, 0,
		0xffffffff);	/* IP prot check disabled */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_L4, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TUNNEL, i, 0, 0xffffffff);/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_L2, i, 0, 0xffffffff);/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_VLAN, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_MPLS, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_L3, i, 0, 0xffffffff);/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_FRAG, i, 0, 0xffffffff);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_IP_PROT, i, 0,
		0xffffffff);	/* inner IP prot check disabled */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PTC_TNL_L4, i, 0, 0xffffffff);/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_CV, i, 0, 3);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_FCS, i, 0, 3);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_TRUNC, i, 0,
		0xffffffff);	/* accept all truncations */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_L3_CS, i, 0, 3);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_L4_CS, i, 0, 3);	/* accept all */
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_PM_OR_INV, i, 0, 1);
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_LC_INV, i, 0, 1);
	nthw_mod_cat_cfn_set(be, HW_CAT_CFN_KM0_OR, i, 0, 0xffffffff);	/* or all */

	if (_VER_ >= 21) {
		nthw_mod_cat_cfn_set(be, HW_CAT_CFN_KM1_OR, i, 0, 0xffffffff);	/* or all */
		nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_TNL_L3_CS, i, 0, 0xffffffff);	/* or all */
		nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_TNL_L4_CS, i, 0, 0xffffffff);	/* or all */
		nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_TTL_EXP, i, 0, 0xffffffff);	/* or all */
		nthw_mod_cat_cfn_set(be, HW_CAT_CFN_ERR_TNL_TTL_EXP, i, 0, 0xffffffff);	/* or all */
	}

	return err;
}

int nthw_mod_cat_cfn_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	switch (count) {
	case ALL_ENTRIES:
		if (start_idx != 0) {
			INDEX_TOO_LARGE_LOG;
			return INDEX_TOO_LARGE;
		}

		return be->iface->cat_cfn_flush(be->be_dev, &be->cat, start_idx,
				be->cat.nb_cat_funcs);

	default:
		if ((unsigned int)(start_idx + count) > be->cat.nb_cat_funcs) {
			INDEX_TOO_LARGE_LOG;
			return INDEX_TOO_LARGE;
		}

		return be->iface->cat_cfn_flush(be->be_dev, &be->cat, start_idx, count);
	}
}

static int hw_mod_cat_cfn_mod(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	int word_off, uint32_t *value, int get)
{
	if ((unsigned int)index >= be->cat.nb_cat_funcs) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 18:
		switch (field) {
		case HW_CAT_CFN_SET_ALL_DEFAULTS:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}
			return cfn_reset(be, index);

		case HW_CAT_CFN_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->cat.v18.cfn[index], (uint8_t)*value,
				sizeof(struct cat_v18_cfn_s));
			break;

		case HW_CAT_CFN_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}
			if ((unsigned int)word_off >= be->cat.nb_cat_funcs) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->cat.v18.cfn, struct cat_v18_cfn_s, index, word_off);
			break;

		case HW_CAT_CFN_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if ((unsigned int)word_off >= be->cat.nb_cat_funcs) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->cat.v18.cfn, struct cat_v18_cfn_s, index, word_off,
				be->cat.nb_cat_funcs);
			break;

		case HW_CAT_CFN_ENABLE:
			GET_SET(be->cat.v18.cfn[index].enable, value);
			break;

		case HW_CAT_CFN_INV:
			GET_SET(be->cat.v18.cfn[index].inv, value);
			break;

		case HW_CAT_CFN_PTC_INV:
			GET_SET(be->cat.v18.cfn[index].ptc_inv, value);
			break;

		case HW_CAT_CFN_PTC_ISL:
			GET_SET(be->cat.v18.cfn[index].ptc_isl, value);
			break;

		case HW_CAT_CFN_PTC_CFP:
			GET_SET(be->cat.v18.cfn[index].ptc_cfp, value);
			break;

		case HW_CAT_CFN_PTC_MAC:
			GET_SET(be->cat.v18.cfn[index].ptc_mac, value);
			break;

		case HW_CAT_CFN_PTC_L2:
			GET_SET(be->cat.v18.cfn[index].ptc_l2, value);
			break;

		case HW_CAT_CFN_PTC_VNTAG:
			GET_SET(be->cat.v18.cfn[index].ptc_vntag, value);
			break;

		case HW_CAT_CFN_PTC_VLAN:
			GET_SET(be->cat.v18.cfn[index].ptc_vlan, value);
			break;

		case HW_CAT_CFN_PTC_MPLS:
			GET_SET(be->cat.v18.cfn[index].ptc_mpls, value);
			break;

		case HW_CAT_CFN_PTC_L3:
			GET_SET(be->cat.v18.cfn[index].ptc_l3, value);
			break;

		case HW_CAT_CFN_PTC_FRAG:
			GET_SET(be->cat.v18.cfn[index].ptc_frag, value);
			break;

		case HW_CAT_CFN_PTC_IP_PROT:
			GET_SET(be->cat.v18.cfn[index].ptc_ip_prot, value);
			break;

		case HW_CAT_CFN_PTC_L4:
			GET_SET(be->cat.v18.cfn[index].ptc_l4, value);
			break;

		case HW_CAT_CFN_PTC_TUNNEL:
			GET_SET(be->cat.v18.cfn[index].ptc_tunnel, value);
			break;

		case HW_CAT_CFN_PTC_TNL_L2:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_l2, value);
			break;

		case HW_CAT_CFN_PTC_TNL_VLAN:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_vlan, value);
			break;

		case HW_CAT_CFN_PTC_TNL_MPLS:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_mpls, value);
			break;

		case HW_CAT_CFN_PTC_TNL_L3:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_l3, value);
			break;

		case HW_CAT_CFN_PTC_TNL_FRAG:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_frag, value);
			break;

		case HW_CAT_CFN_PTC_TNL_IP_PROT:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_ip_prot, value);
			break;

		case HW_CAT_CFN_PTC_TNL_L4:
			GET_SET(be->cat.v18.cfn[index].ptc_tnl_l4, value);
			break;

		case HW_CAT_CFN_ERR_INV:
			GET_SET(be->cat.v18.cfn[index].err_inv, value);
			break;

		case HW_CAT_CFN_ERR_CV:
			GET_SET(be->cat.v18.cfn[index].err_cv, value);
			break;

		case HW_CAT_CFN_ERR_FCS:
			GET_SET(be->cat.v18.cfn[index].err_fcs, value);
			break;

		case HW_CAT_CFN_ERR_TRUNC:
			GET_SET(be->cat.v18.cfn[index].err_trunc, value);
			break;

		case HW_CAT_CFN_ERR_L3_CS:
			GET_SET(be->cat.v18.cfn[index].err_l3_cs, value);
			break;

		case HW_CAT_CFN_ERR_L4_CS:
			GET_SET(be->cat.v18.cfn[index].err_l4_cs, value);
			break;

		case HW_CAT_CFN_MAC_PORT:
			GET_SET(be->cat.v18.cfn[index].mac_port, value);
			break;

		case HW_CAT_CFN_PM_CMP:
			if (word_off > 1) {
				WORD_OFF_TOO_LARGE_LOG;
				return WORD_OFF_TOO_LARGE;
			}

			GET_SET(be->cat.v18.cfn[index].pm_cmp[word_off], value);
			break;

		case HW_CAT_CFN_PM_DCT:
			GET_SET(be->cat.v18.cfn[index].pm_dct, value);
			break;

		case HW_CAT_CFN_PM_EXT_INV:
			GET_SET(be->cat.v18.cfn[index].pm_ext_inv, value);
			break;

		case HW_CAT_CFN_PM_CMB:
			GET_SET(be->cat.v18.cfn[index].pm_cmb, value);
			break;

		case HW_CAT_CFN_PM_AND_INV:
			GET_SET(be->cat.v18.cfn[index].pm_and_inv, value);
			break;

		case HW_CAT_CFN_PM_OR_INV:
			GET_SET(be->cat.v18.cfn[index].pm_or_inv, value);
			break;

		case HW_CAT_CFN_PM_INV:
			GET_SET(be->cat.v18.cfn[index].pm_inv, value);
			break;

		case HW_CAT_CFN_LC:
			GET_SET(be->cat.v18.cfn[index].lc, value);
			break;

		case HW_CAT_CFN_LC_INV:
			GET_SET(be->cat.v18.cfn[index].lc_inv, value);
			break;

		case HW_CAT_CFN_KM0_OR:
			GET_SET(be->cat.v18.cfn[index].km_or, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18 */
	case 21:
		switch (field) {
		case HW_CAT_CFN_SET_ALL_DEFAULTS:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			return cfn_reset(be, index);

		case HW_CAT_CFN_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->cat.v21.cfn[index], (uint8_t)*value,
				sizeof(struct cat_v21_cfn_s));
			break;

		case HW_CAT_CFN_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if ((unsigned int)word_off >= be->cat.nb_cat_funcs) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->cat.v21.cfn, struct cat_v21_cfn_s, index, word_off);
			break;

		case HW_CAT_CFN_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if ((unsigned int)word_off >= be->cat.nb_cat_funcs) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->cat.v21.cfn, struct cat_v21_cfn_s, index, word_off,
				be->cat.nb_cat_funcs);
			break;

		case HW_CAT_CFN_COPY_FROM:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memcpy(&be->cat.v21.cfn[index], &be->cat.v21.cfn[*value],
				sizeof(struct cat_v21_cfn_s));
			break;

		case HW_CAT_CFN_ENABLE:
			GET_SET(be->cat.v21.cfn[index].enable, value);
			break;

		case HW_CAT_CFN_INV:
			GET_SET(be->cat.v21.cfn[index].inv, value);
			break;

		case HW_CAT_CFN_PTC_INV:
			GET_SET(be->cat.v21.cfn[index].ptc_inv, value);
			break;

		case HW_CAT_CFN_PTC_ISL:
			GET_SET(be->cat.v21.cfn[index].ptc_isl, value);
			break;

		case HW_CAT_CFN_PTC_CFP:
			GET_SET(be->cat.v21.cfn[index].ptc_cfp, value);
			break;

		case HW_CAT_CFN_PTC_MAC:
			GET_SET(be->cat.v21.cfn[index].ptc_mac, value);
			break;

		case HW_CAT_CFN_PTC_L2:
			GET_SET(be->cat.v21.cfn[index].ptc_l2, value);
			break;

		case HW_CAT_CFN_PTC_VNTAG:
			GET_SET(be->cat.v21.cfn[index].ptc_vntag, value);
			break;

		case HW_CAT_CFN_PTC_VLAN:
			GET_SET(be->cat.v21.cfn[index].ptc_vlan, value);
			break;

		case HW_CAT_CFN_PTC_MPLS:
			GET_SET(be->cat.v21.cfn[index].ptc_mpls, value);
			break;

		case HW_CAT_CFN_PTC_L3:
			GET_SET(be->cat.v21.cfn[index].ptc_l3, value);
			break;

		case HW_CAT_CFN_PTC_FRAG:
			GET_SET(be->cat.v21.cfn[index].ptc_frag, value);
			break;

		case HW_CAT_CFN_PTC_IP_PROT:
			GET_SET(be->cat.v21.cfn[index].ptc_ip_prot, value);
			break;

		case HW_CAT_CFN_PTC_L4:
			GET_SET(be->cat.v21.cfn[index].ptc_l4, value);
			break;

		case HW_CAT_CFN_PTC_TUNNEL:
			GET_SET(be->cat.v21.cfn[index].ptc_tunnel, value);
			break;

		case HW_CAT_CFN_PTC_TNL_L2:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_l2, value);
			break;

		case HW_CAT_CFN_PTC_TNL_VLAN:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_vlan, value);
			break;

		case HW_CAT_CFN_PTC_TNL_MPLS:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_mpls, value);
			break;

		case HW_CAT_CFN_PTC_TNL_L3:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_l3, value);
			break;

		case HW_CAT_CFN_PTC_TNL_FRAG:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_frag, value);
			break;

		case HW_CAT_CFN_PTC_TNL_IP_PROT:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_ip_prot, value);
			break;

		case HW_CAT_CFN_PTC_TNL_L4:
			GET_SET(be->cat.v21.cfn[index].ptc_tnl_l4, value);
			break;

		case HW_CAT_CFN_ERR_INV:
			GET_SET(be->cat.v21.cfn[index].err_inv, value);
			break;

		case HW_CAT_CFN_ERR_CV:
			GET_SET(be->cat.v21.cfn[index].err_cv, value);
			break;

		case HW_CAT_CFN_ERR_FCS:
			GET_SET(be->cat.v21.cfn[index].err_fcs, value);
			break;

		case HW_CAT_CFN_ERR_TRUNC:
			GET_SET(be->cat.v21.cfn[index].err_trunc, value);
			break;

		case HW_CAT_CFN_ERR_L3_CS:
			GET_SET(be->cat.v21.cfn[index].err_l3_cs, value);
			break;

		case HW_CAT_CFN_ERR_L4_CS:
			GET_SET(be->cat.v21.cfn[index].err_l4_cs, value);
			break;

		case HW_CAT_CFN_ERR_TNL_L3_CS:
			GET_SET(be->cat.v21.cfn[index].err_tnl_l3_cs, value);
			break;

		case HW_CAT_CFN_ERR_TNL_L4_CS:
			GET_SET(be->cat.v21.cfn[index].err_tnl_l4_cs, value);
			break;

		case HW_CAT_CFN_ERR_TTL_EXP:
			GET_SET(be->cat.v21.cfn[index].err_ttl_exp, value);
			break;

		case HW_CAT_CFN_ERR_TNL_TTL_EXP:
			GET_SET(be->cat.v21.cfn[index].err_tnl_ttl_exp, value);
			break;

		case HW_CAT_CFN_MAC_PORT:
			GET_SET(be->cat.v21.cfn[index].mac_port, value);
			break;

		case HW_CAT_CFN_PM_CMP:
			if (word_off > 1) {
				WORD_OFF_TOO_LARGE_LOG;
				return WORD_OFF_TOO_LARGE;
			}

			GET_SET(be->cat.v21.cfn[index].pm_cmp[word_off], value);
			break;

		case HW_CAT_CFN_PM_DCT:
			GET_SET(be->cat.v21.cfn[index].pm_dct, value);
			break;

		case HW_CAT_CFN_PM_EXT_INV:
			GET_SET(be->cat.v21.cfn[index].pm_ext_inv, value);
			break;

		case HW_CAT_CFN_PM_CMB:
			GET_SET(be->cat.v21.cfn[index].pm_cmb, value);
			break;

		case HW_CAT_CFN_PM_AND_INV:
			GET_SET(be->cat.v21.cfn[index].pm_and_inv, value);
			break;

		case HW_CAT_CFN_PM_OR_INV:
			GET_SET(be->cat.v21.cfn[index].pm_or_inv, value);
			break;

		case HW_CAT_CFN_PM_INV:
			GET_SET(be->cat.v21.cfn[index].pm_inv, value);
			break;

		case HW_CAT_CFN_LC:
			GET_SET(be->cat.v21.cfn[index].lc, value);
			break;

		case HW_CAT_CFN_LC_INV:
			GET_SET(be->cat.v21.cfn[index].lc_inv, value);
			break;

		case HW_CAT_CFN_KM0_OR:
			GET_SET(be->cat.v21.cfn[index].km0_or, value);
			break;

		case HW_CAT_CFN_KM1_OR:
			GET_SET(be->cat.v21.cfn[index].km1_or, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_cfn_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	int index, int word_off, uint32_t value)
{
	return hw_mod_cat_cfn_mod(be, field, index, word_off, &value, 0);
}

static inline int find_km_flm_module_interface_index(struct flow_api_backend_s *be,
	enum km_flm_if_select_e if_num, int km_if_id)
{
	int km_if_idx;

	if (_VER_ == 18) {
		km_if_idx = 0;

	} else {
		if (if_num == KM_FLM_IF_SECOND) {
			if (be->cat.km_if_m1 == km_if_id) {
				km_if_idx = 1;
			} else {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}
		} else if (be->cat.km_if_m0 == km_if_id) {
			km_if_idx = 0;
		} else if (be->cat.km_if_m1 == km_if_id) {
			km_if_idx = 1;
		} else {
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}
	}

	return km_if_idx;
}

/*
 * KCE
 */

static int hw_mod_cat_kce_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int km_if_id, int start_idx, int count)
{
	/* writes 8 bits - one for each cfn - at a time */
	if (count == ALL_ENTRIES)
		count = be->cat.nb_cat_funcs / 8;

	if ((unsigned int)(start_idx + count) > (be->cat.nb_cat_funcs / 8)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	/* find KM module */
	int km_if_idx = find_km_flm_module_interface_index(be, if_num, km_if_id);

	if (km_if_idx < 0)
		return km_if_idx;

	return be->iface->cat_kce_flush(be->be_dev, &be->cat, km_if_idx, start_idx, count);
}

int nthw_mod_cat_kce_km_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count)
{
	return hw_mod_cat_kce_flush(be, if_num, 0, start_idx, count);
}

int nthw_mod_cat_kce_flm_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count)
{
	return hw_mod_cat_kce_flush(be, if_num, 1, start_idx, count);
}

static int hw_mod_cat_kce_mod(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int km_if_id, int index,
	uint32_t *value, int get)
{
	if ((unsigned int)index >= (be->cat.nb_cat_funcs / 8)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	/* find KM module */
	int km_if_idx = find_km_flm_module_interface_index(be, if_num, km_if_id);

	if (km_if_idx < 0)
		return km_if_idx;

	switch (_VER_) {
	case 18:
		switch (field) {
		case HW_CAT_KCE_ENABLE_BM:
			GET_SET(be->cat.v18.kce[index].enable_bm, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18 */
	case 21:
		switch (field) {
		case HW_CAT_KCE_ENABLE_BM:
			GET_SET(be->cat.v21.kce[index].enable_bm[km_if_idx], value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_kce_km_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value)
{
	return hw_mod_cat_kce_mod(be, field, if_num, 0, index, &value, 0);
}

int nthw_mod_cat_kce_km_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value)
{
	return hw_mod_cat_kce_mod(be, field, if_num, 0, index, value, 1);
}

int nthw_mod_cat_kce_flm_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value)
{
	return hw_mod_cat_kce_mod(be, field, if_num, 1, index, &value, 0);
}

int nthw_mod_cat_kce_flm_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value)
{
	return hw_mod_cat_kce_mod(be, field, if_num, 1, index, value, 1);
}

/*
 * KCS
 */
static int hw_mod_cat_kcs_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int km_if_id, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.nb_cat_funcs;

	if ((unsigned int)(start_idx + count) > be->cat.nb_cat_funcs) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	/* find KM module */
	int km_if_idx = find_km_flm_module_interface_index(be, if_num, km_if_id);

	if (km_if_idx < 0)
		return km_if_idx;

	return be->iface->cat_kcs_flush(be->be_dev, &be->cat, km_if_idx, start_idx, count);
}

int nthw_mod_cat_kcs_km_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count)
{
	return hw_mod_cat_kcs_flush(be, if_num, 0, start_idx, count);
}

int nthw_mod_cat_kcs_flm_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count)
{
	return hw_mod_cat_kcs_flush(be, if_num, 1, start_idx, count);
}

static int hw_mod_cat_kcs_mod(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int km_if_id, int index,
	uint32_t *value, int get)
{
	if ((unsigned int)index >= be->cat.nb_cat_funcs) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	/* find KM module */
	int km_if_idx = find_km_flm_module_interface_index(be, if_num, km_if_id);

	if (km_if_idx < 0)
		return km_if_idx;

	switch (_VER_) {
	case 18:
		switch (field) {
		case HW_CAT_KCS_CATEGORY:
			GET_SET(be->cat.v18.kcs[index].category, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18 */
	case 21:
		switch (field) {
		case HW_CAT_KCS_CATEGORY:
			GET_SET(be->cat.v21.kcs[index].category[km_if_idx], value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_kcs_km_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value)
{
	return hw_mod_cat_kcs_mod(be, field, if_num, 0, index, &value, 0);
}

int nthw_mod_cat_kcs_km_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value)
{
	return hw_mod_cat_kcs_mod(be, field, if_num, 0, index, value, 1);
}

int nthw_mod_cat_kcs_flm_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value)
{
	return hw_mod_cat_kcs_mod(be, field, if_num, 1, index, &value, 0);
}

int nthw_mod_cat_kcs_flm_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value)
{
	return hw_mod_cat_kcs_mod(be, field, if_num, 1, index, value, 1);
}

/*
 * FTE
 */
static int hw_mod_cat_fte_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int km_if_id, int start_idx, int count)
{
	const uint32_t key_cnt = (_VER_ >= 20) ? 4 : 2;

	if (count == ALL_ENTRIES)
		count = be->cat.nb_cat_funcs / 8 * be->cat.nb_flow_types * key_cnt;

	if ((unsigned int)(start_idx + count) >
		(be->cat.nb_cat_funcs / 8 * be->cat.nb_flow_types * key_cnt)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	/* find KM module */
	int km_if_idx = find_km_flm_module_interface_index(be, if_num, km_if_id);

	if (km_if_idx < 0)
		return km_if_idx;

	return be->iface->cat_fte_flush(be->be_dev, &be->cat, km_if_idx, start_idx, count);
}

int nthw_mod_cat_fte_km_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count)
{
	return hw_mod_cat_fte_flush(be, if_num, 0, start_idx, count);
}

int nthw_mod_cat_fte_flm_flush(struct flow_api_backend_s *be, enum km_flm_if_select_e if_num,
	int start_idx, int count)
{
	return hw_mod_cat_fte_flush(be, if_num, 1, start_idx, count);
}

static int hw_mod_cat_fte_mod(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int km_if_id, int index,
	uint32_t *value, int get)
{
	const uint32_t key_cnt = (_VER_ >= 20) ? 4 : 2;

	if ((unsigned int)index >= (be->cat.nb_cat_funcs / 8 * be->cat.nb_flow_types * key_cnt)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	/* find KM module */
	int km_if_idx = find_km_flm_module_interface_index(be, if_num, km_if_id);

	if (km_if_idx < 0)
		return km_if_idx;

	switch (_VER_) {
	case 18:
		switch (field) {
		case HW_CAT_FTE_ENABLE_BM:
			GET_SET(be->cat.v18.fte[index].enable_bm, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18 */
	case 21:
		switch (field) {
		case HW_CAT_FTE_ENABLE_BM:
			GET_SET(be->cat.v21.fte[index].enable_bm[km_if_idx], value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_fte_km_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value)
{
	return hw_mod_cat_fte_mod(be, field, if_num, 0, index, &value, 0);
}

int nthw_mod_cat_fte_km_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value)
{
	return hw_mod_cat_fte_mod(be, field, if_num, 0, index, value, 1);
}

int nthw_mod_cat_fte_flm_set(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t value)
{
	return hw_mod_cat_fte_mod(be, field, if_num, 1, index, &value, 0);
}

int nthw_mod_cat_fte_flm_get(struct flow_api_backend_s *be, enum hw_cat_e field,
	enum km_flm_if_select_e if_num, int index, uint32_t *value)
{
	return hw_mod_cat_fte_mod(be, field, if_num, 1, index, value, 1);
}

int nthw_mod_cat_cte_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.nb_cat_funcs;

	if ((unsigned int)(start_idx + count) > be->cat.nb_cat_funcs) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_cte_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_cte_mod(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value, int get)
{
	if ((unsigned int)index >= be->cat.nb_cat_funcs) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 18:
	case 21:
		switch (field) {
		case HW_CAT_CTE_ENABLE_BM:
			GET_SET(be->cat.v18.cte[index].enable_bm, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18/21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_cte_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t value)
{
	return hw_mod_cat_cte_mod(be, field, index, &value, 0);
}

int nthw_mod_cat_cte_get(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value)
{
	return hw_mod_cat_cte_mod(be, field, index, value, 1);
}

int nthw_mod_cat_cts_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	int addr_size = (_VER_ < 15) ? 8 : ((be->cat.cts_num + 1) / 2);

	if (count == ALL_ENTRIES)
		count = be->cat.nb_cat_funcs * addr_size;

	if ((unsigned int)(start_idx + count) > (be->cat.nb_cat_funcs * addr_size)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_cts_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_cts_mod(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value, int get)
{
	int addr_size = (be->cat.cts_num + 1) / 2;

	if ((unsigned int)index >= (be->cat.nb_cat_funcs * addr_size)) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 18:
	case 21:
		switch (field) {
		case HW_CAT_CTS_CAT_A:
			GET_SET(be->cat.v18.cts[index].cat_a, value);
			break;

		case HW_CAT_CTS_CAT_B:
			GET_SET(be->cat.v18.cts[index].cat_b, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18/21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_cts_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t value)
{
	return hw_mod_cat_cts_mod(be, field, index, &value, 0);
}

int nthw_mod_cat_cts_get(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value)
{
	return hw_mod_cat_cts_mod(be, field, index, value, 1);
}

int nthw_mod_cat_cot_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->max_categories;

	if ((unsigned int)(start_idx + count) > be->max_categories)  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_cot_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_cot_mod(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t *value, int get)
{
	if ((unsigned int)index >= be->max_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 18:
	case 21:
		switch (field) {
		case HW_CAT_COT_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->cat.v18.cot[index], (uint8_t)*value,
				sizeof(struct cat_v18_cot_s));
			break;

		case HW_CAT_COT_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->max_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->cat.v18.cot, struct cat_v18_cot_s, index, *value);
			break;

		case HW_CAT_COT_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->max_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->cat.v18.cot, struct cat_v18_cot_s, index, *value,
				be->max_categories);
			break;

		case HW_CAT_COT_COPY_FROM:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memcpy(&be->cat.v18.cot[index], &be->cat.v18.cot[*value],
				sizeof(struct cat_v18_cot_s));
			break;

		case HW_CAT_COT_COLOR:
			GET_SET(be->cat.v18.cot[index].color, value);
			break;

		case HW_CAT_COT_KM:
			GET_SET(be->cat.v18.cot[index].km, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 18/21 */

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_cat_cot_set(struct flow_api_backend_s *be, enum hw_cat_e field, int index,
	uint32_t value)
{
	return hw_mod_cat_cot_mod(be, field, index, &value, 0);
}

static int hw_mod_cat_cct_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.nb_cat_funcs * 4;

	if ((unsigned int)(start_idx + count) > be->cat.nb_cat_funcs * 4)  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_cct_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_kcc_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.kcc_size;

	if ((unsigned int)(start_idx + count) > be->cat.kcc_size)  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_kcc_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_exo_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.nb_pm_ext;

	if ((unsigned int)(start_idx + count) > be->cat.nb_pm_ext)  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_exo_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_rck_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.nb_pm_ext * 64;

	if ((unsigned int)(start_idx + count) > (be->cat.nb_pm_ext * 64))  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_rck_flush(be->be_dev, &be->cat, start_idx, count);
}

static int hw_mod_cat_len_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->cat.nb_len;

	if ((unsigned int)(start_idx + count) > be->cat.nb_len) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->cat_len_flush(be->be_dev, &be->cat, start_idx, count);
}

int nthw_mod_cat_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->cat));

	NT_LOG(DBG, FILTER, "INIT CAT CFN");

	if (nthw_mod_cat_cfn_flush(be, 0, ALL_ENTRIES))
		return -1;

	if (_VER_ <= 18) {
		NT_LOG(DBG, FILTER, "INIT CAT KCE");

		if (hw_mod_cat_kce_flush(be, KM_FLM_IF_FIRST, 0, 0, ALL_ENTRIES))
			return -1;

		NT_LOG(DBG, FILTER, "INIT CAT KCS");

		if (hw_mod_cat_kcs_flush(be, KM_FLM_IF_FIRST, 0, 0, ALL_ENTRIES))
			return -1;

		NT_LOG(DBG, FILTER, "INIT CAT FTE");

		if (hw_mod_cat_fte_flush(be, KM_FLM_IF_FIRST, 0, 0, ALL_ENTRIES))
			return -1;

	} else {
		NT_LOG(DBG, FILTER, "INIT CAT KCE 0");

		if (hw_mod_cat_kce_flush(be, KM_FLM_IF_FIRST, be->cat.km_if_m0, 0, ALL_ENTRIES))
			return -1;

		NT_LOG(DBG, FILTER, "INIT CAT KCS 0");

		if (hw_mod_cat_kcs_flush(be, KM_FLM_IF_FIRST, be->cat.km_if_m0, 0, ALL_ENTRIES))
			return -1;

		NT_LOG(DBG, FILTER, "INIT CAT FTE 0");

		if (hw_mod_cat_fte_flush(be, KM_FLM_IF_FIRST, be->cat.km_if_m0, 0, ALL_ENTRIES))
			return -1;

		if (be->cat.km_if_count > 1) {
			NT_LOG(DBG, FILTER, "INIT CAT KCE 1");

			if (hw_mod_cat_kce_flush(be, KM_FLM_IF_SECOND, be->cat.km_if_m1, 0,
					ALL_ENTRIES))
				return -1;

			NT_LOG(DBG, FILTER, "INIT CAT KCS 1");

			if (hw_mod_cat_kcs_flush(be, KM_FLM_IF_SECOND, be->cat.km_if_m1, 0,
					ALL_ENTRIES))
				return -1;

			NT_LOG(DBG, FILTER, "INIT CAT FTE 1");

			if (hw_mod_cat_fte_flush(be, KM_FLM_IF_SECOND, be->cat.km_if_m1, 0,
					ALL_ENTRIES))
				return -1;
		}
	}

	NT_LOG(DBG, FILTER, "INIT CAT CTE");

	if (nthw_mod_cat_cte_flush(be, 0, ALL_ENTRIES))
		return -1;

	NT_LOG(DBG, FILTER, "INIT CAT CTS");

	if (nthw_mod_cat_cts_flush(be, 0, ALL_ENTRIES))
		return -1;

	NT_LOG(DBG, FILTER, "INIT CAT COT");

	if (nthw_mod_cat_cot_flush(be, 0, ALL_ENTRIES))
		return -1;

	NT_LOG(DBG, FILTER, "INIT CAT CCT");

	if (hw_mod_cat_cct_flush(be, 0, ALL_ENTRIES))
		return -1;

	NT_LOG(DBG, FILTER, "INIT CAT EXO");

	if (hw_mod_cat_exo_flush(be, 0, ALL_ENTRIES))
		return -1;

	NT_LOG(DBG, FILTER, "INIT CAT RCK");

	if (hw_mod_cat_rck_flush(be, 0, ALL_ENTRIES))
		return -1;

	NT_LOG(DBG, FILTER, "INIT CAT LEN");

	if (hw_mod_cat_len_flush(be, 0, ALL_ENTRIES))
		return -1;

	if (be->cat.kcc_size) {
		NT_LOG(DBG, FILTER, "INIT CAT KCC");

		if (hw_mod_cat_kcc_flush(be, 0, ALL_ENTRIES))
			return -1;
	}

	return 0;
}
