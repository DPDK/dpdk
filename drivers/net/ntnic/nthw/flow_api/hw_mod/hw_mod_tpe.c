/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>

#include "hw_mod_backend.h"

#define _MOD_ "TPE"
#define _VER_ be->tpe.ver

bool nthw_mod_tpe_present(struct flow_api_backend_s *be)
{
	return be->iface->get_tpe_present(be->be_dev);
}

int nthw_mod_tpe_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_tpe_version(be->be_dev);
	NT_LOG(DBG, FILTER, _MOD_ " MODULE VERSION %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	nb = be->iface->get_nb_tpe_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tpe_categories);
		return COUNT_ERROR;
	}

	be->tpe.nb_rcp_categories = (uint32_t)nb;

	nb = be->iface->get_nb_tpe_ifr_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tpe_ifr_categories);
		return COUNT_ERROR;
	}

	be->tpe.nb_ifr_categories = (uint32_t)nb;

	nb = be->iface->get_nb_tx_cpy_writers(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tx_cpy_writers);
		return COUNT_ERROR;
	}

	be->tpe.nb_cpy_writers = (uint32_t)nb;

	nb = be->iface->get_nb_tx_rpl_depth(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tx_rpl_depth);
		return COUNT_ERROR;
	}

	be->tpe.nb_rpl_depth = (uint32_t)nb;

	nb = be->iface->get_nb_tx_rpl_ext_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(tx_rpl_ext_categories);
		return COUNT_ERROR;
	}

	be->tpe.nb_rpl_ext_categories = (uint32_t)nb;

	switch (_VER_) {
	case 3:
		if (!nthw_callocate_mod((struct common_func_s *)&be->tpe, 11, &be->tpe.v3.rpp_rcp,
				be->tpe.nb_rcp_categories, sizeof(struct tpe_v1_rpp_v0_rcp_s),
				&be->tpe.v3.rpp_ifr_rcp, be->tpe.nb_ifr_categories,
				sizeof(struct tpe_v2_rpp_v1_ifr_rcp_s), &be->tpe.v3.ifr_rcp,
				be->tpe.nb_ifr_categories, sizeof(struct tpe_v2_ifr_v1_rcp_s),
				&be->tpe.v3.ifr_counters, be->tpe.nb_ifr_categories,
				sizeof(struct tpe_v2_ifr_v1_counters_s),

				&be->tpe.v3.ins_rcp, be->tpe.nb_rcp_categories,
				sizeof(struct tpe_v1_ins_v1_rcp_s),

				&be->tpe.v3.rpl_rcp, be->tpe.nb_rcp_categories,
				sizeof(struct tpe_v3_rpl_v4_rcp_s), &be->tpe.v3.rpl_ext,
				be->tpe.nb_rpl_ext_categories,
				sizeof(struct tpe_v1_rpl_v2_ext_s), &be->tpe.v3.rpl_rpl,
				be->tpe.nb_rpl_depth, sizeof(struct tpe_v1_rpl_v2_rpl_s),

				&be->tpe.v3.cpy_rcp,
				be->tpe.nb_cpy_writers * be->tpe.nb_rcp_categories,
				sizeof(struct tpe_v1_cpy_v1_rcp_s),

				&be->tpe.v3.hfu_rcp, be->tpe.nb_rcp_categories,
				sizeof(struct tpe_v1_hfu_v1_rcp_s),

				&be->tpe.v3.csu_rcp, be->tpe.nb_rcp_categories,
				sizeof(struct tpe_v1_csu_v0_rcp_s)))
			return -1;

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void nthw_mod_tpe_free(struct flow_api_backend_s *be)
{
	if (be->tpe.base) {
		free(be->tpe.base);
		be->tpe.base = NULL;
	}
}

int nthw_mod_tpe_reset(struct flow_api_backend_s *be)
{
	int err = 0;

	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->tpe));

	NT_LOG(DBG, FILTER, "INIT TPE");
	err |= nthw_mod_tpe_rpp_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_ins_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_rpl_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_rpl_ext_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_rpl_rpl_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_cpy_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_hfu_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_csu_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_rpp_ifr_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_ifr_rcp_flush(be, 0, ALL_ENTRIES);
	err |= nthw_mod_tpe_ifr_counters_update(be, 0, ALL_ENTRIES);

	return err;
}

/*
 * RPP_IFR_RCP
 */

int nthw_mod_tpe_rpp_ifr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_ifr_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_ifr_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpp_ifr_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_rpp_ifr_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_ifr_categories)
		return INDEX_TOO_LARGE;

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_IFR_RCP_IPV4_EN:
			GET_SET(be->tpe.v3.rpp_ifr_rcp[index].ipv4_en, value);
			break;

		case HW_TPE_IFR_RCP_IPV4_DF_DROP:
			GET_SET(be->tpe.v3.rpp_ifr_rcp[index].ipv4_df_drop, value);
			break;

		case HW_TPE_IFR_RCP_IPV6_EN:
			GET_SET(be->tpe.v3.rpp_ifr_rcp[index].ipv6_en, value);
			break;

		case HW_TPE_IFR_RCP_IPV6_DROP:
			GET_SET(be->tpe.v3.rpp_ifr_rcp[index].ipv6_drop, value);
			break;

		case HW_TPE_IFR_RCP_MTU:
			GET_SET(be->tpe.v3.rpp_ifr_rcp[index].mtu, value);
			break;

		default:
			return UNSUP_FIELD;
		}

		break;

	default:
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_rpp_ifr_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_rpp_ifr_rcp_mod(be, field, index, &value, 0);
}

/*
 * RPP_RCP
 */

int nthw_mod_tpe_rpp_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpp_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_rpp_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.rpp_rcp[index], (uint8_t)*value,
				sizeof(struct tpe_v1_rpp_v0_rcp_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.rpp_rcp, struct tpe_v1_rpp_v0_rcp_s, index,
				*value, be->tpe.nb_rcp_categories);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.rpp_rcp, struct tpe_v1_rpp_v0_rcp_s, index,
				*value);
			break;

		case HW_TPE_RPP_RCP_EXP:
			GET_SET(be->tpe.v3.rpp_rcp[index].exp, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_rpp_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_rpp_rcp_mod(be, field, index, &value, 0);
}

/*
 * IFR_RCP
 */

int nthw_mod_tpe_ifr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_ifr_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_ifr_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_ifr_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_ifr_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_ifr_categories)
		return INDEX_TOO_LARGE;

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_IFR_RCP_IPV4_EN:
			GET_SET(be->tpe.v3.ifr_rcp[index].ipv4_en, value);
			break;

		case HW_TPE_IFR_RCP_IPV4_DF_DROP:
			GET_SET(be->tpe.v3.ifr_rcp[index].ipv4_df_drop, value);
			break;

		case HW_TPE_IFR_RCP_IPV6_EN:
			GET_SET(be->tpe.v3.ifr_rcp[index].ipv6_en, value);
			break;

		case HW_TPE_IFR_RCP_IPV6_DROP:
			GET_SET(be->tpe.v3.ifr_rcp[index].ipv6_drop, value);
			break;

		case HW_TPE_IFR_RCP_MTU:
			GET_SET(be->tpe.v3.ifr_rcp[index].mtu, value);
			break;

		default:
			return UNSUP_FIELD;
		}

		break;

	default:
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_ifr_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_ifr_rcp_mod(be, field, index, &value, 0);
}

/*
 * IFR_COUNTER
 */

int nthw_mod_tpe_ifr_counters_update(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_ifr_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_ifr_categories)
		return INDEX_TOO_LARGE;

	return be->iface->tpe_ifr_counters_update(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_ifr_counters_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_ifr_categories)
		return INDEX_TOO_LARGE;

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_IFR_COUNTERS_DROP:
			GET_SET(be->tpe.v3.ifr_counters[index].drop, value);
			break;

		default:
			return UNSUP_FIELD;
		}

		break;

	default:
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_ifr_counters_get(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t *value)
{
	return hw_mod_tpe_ifr_counters_mod(be, field, index, value, 1);
}

/*
 * INS_RCP
 */

int nthw_mod_tpe_ins_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_ins_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_ins_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.ins_rcp[index], (uint8_t)*value,
				sizeof(struct tpe_v1_ins_v1_rcp_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.ins_rcp, struct tpe_v1_ins_v1_rcp_s, index,
				*value, be->tpe.nb_rcp_categories);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.ins_rcp, struct tpe_v1_ins_v1_rcp_s, index,
				*value);
			break;

		case HW_TPE_INS_RCP_DYN:
			GET_SET(be->tpe.v3.ins_rcp[index].dyn, value);
			break;

		case HW_TPE_INS_RCP_OFS:
			GET_SET(be->tpe.v3.ins_rcp[index].ofs, value);
			break;

		case HW_TPE_INS_RCP_LEN:
			GET_SET(be->tpe.v3.ins_rcp[index].len, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_ins_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_ins_rcp_mod(be, field, index, &value, 0);
}

/*
 * RPL_RCP
 */

int nthw_mod_tpe_rpl_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpl_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_rpl_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.rpl_rcp[index], (uint8_t)*value,
				sizeof(struct tpe_v3_rpl_v4_rcp_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.rpl_rcp, struct tpe_v3_rpl_v4_rcp_s, index,
				*value, be->tpe.nb_rcp_categories);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.rpl_rcp, struct tpe_v3_rpl_v4_rcp_s, index,
				*value);
			break;

		case HW_TPE_RPL_RCP_DYN:
			GET_SET(be->tpe.v3.rpl_rcp[index].dyn, value);
			break;

		case HW_TPE_RPL_RCP_OFS:
			GET_SET(be->tpe.v3.rpl_rcp[index].ofs, value);
			break;

		case HW_TPE_RPL_RCP_LEN:
			GET_SET(be->tpe.v3.rpl_rcp[index].len, value);
			break;

		case HW_TPE_RPL_RCP_RPL_PTR:
			GET_SET(be->tpe.v3.rpl_rcp[index].rpl_ptr, value);
			break;

		case HW_TPE_RPL_RCP_EXT_PRIO:
			GET_SET(be->tpe.v3.rpl_rcp[index].ext_prio, value);
			break;

		case HW_TPE_RPL_RCP_ETH_TYPE_WR:
			GET_SET(be->tpe.v3.rpl_rcp[index].eth_type_wr, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_rpl_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_rpl_rcp_mod(be, field, index, &value, 0);
}

/*
 * RPL_EXT
 */

int nthw_mod_tpe_rpl_ext_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rpl_ext_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rpl_ext_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpl_ext_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_rpl_ext_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rpl_ext_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.rpl_ext[index], (uint8_t)*value,
				sizeof(struct tpe_v1_rpl_v2_ext_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rpl_ext_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.rpl_ext, struct tpe_v1_rpl_v2_ext_s, index,
				*value, be->tpe.nb_rpl_ext_categories);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rpl_ext_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.rpl_ext, struct tpe_v1_rpl_v2_ext_s, index,
				*value);
			break;

		case HW_TPE_RPL_EXT_RPL_PTR:
			GET_SET(be->tpe.v3.rpl_ext[index].rpl_ptr, value);
			break;

		case HW_TPE_RPL_EXT_META_RPL_LEN:
			GET_SET(be->tpe.v3.rpl_ext[index].meta_rpl_len, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_rpl_ext_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_rpl_ext_mod(be, field, index, &value, 0);
}

/*
 * RPL_RPL
 */

int nthw_mod_tpe_rpl_rpl_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rpl_depth;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rpl_depth) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpl_rpl_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_rpl_rpl_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rpl_depth) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.rpl_rpl[index], (uint8_t)*value,
				sizeof(struct tpe_v1_rpl_v2_rpl_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rpl_depth) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.rpl_rpl, struct tpe_v1_rpl_v2_rpl_s, index,
				*value, be->tpe.nb_rpl_depth);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rpl_depth) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.rpl_rpl, struct tpe_v1_rpl_v2_rpl_s, index,
				*value);
			break;

		case HW_TPE_RPL_RPL_VALUE:
			if (get)
				memcpy(value, be->tpe.v3.rpl_rpl[index].value,
					sizeof(uint32_t) * 4);

			else
				memcpy(be->tpe.v3.rpl_rpl[index].value, value,
					sizeof(uint32_t) * 4);

			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_rpl_rpl_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t *value)
{
	return hw_mod_tpe_rpl_rpl_mod(be, field, index, value, 0);
}

/*
 * CPY_RCP
 */

int nthw_mod_tpe_cpy_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	const uint32_t cpy_size = be->tpe.nb_cpy_writers * be->tpe.nb_rcp_categories;

	if (count == ALL_ENTRIES)
		count = cpy_size;

	if ((unsigned int)(start_idx + count) > cpy_size) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_cpy_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_cpy_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	const uint32_t cpy_size = be->tpe.nb_cpy_writers * be->tpe.nb_rcp_categories;

	if (index >= cpy_size) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.cpy_rcp[index], (uint8_t)*value,
				sizeof(struct tpe_v1_cpy_v1_rcp_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= cpy_size) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.cpy_rcp, struct tpe_v1_cpy_v1_rcp_s, index,
				*value, cpy_size);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= cpy_size) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.cpy_rcp, struct tpe_v1_cpy_v1_rcp_s, index,
				*value);
			break;

		case HW_TPE_CPY_RCP_READER_SELECT:
			GET_SET(be->tpe.v3.cpy_rcp[index].reader_select, value);
			break;

		case HW_TPE_CPY_RCP_DYN:
			GET_SET(be->tpe.v3.cpy_rcp[index].dyn, value);
			break;

		case HW_TPE_CPY_RCP_OFS:
			GET_SET(be->tpe.v3.cpy_rcp[index].ofs, value);
			break;

		case HW_TPE_CPY_RCP_LEN:
			GET_SET(be->tpe.v3.cpy_rcp[index].len, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_cpy_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_cpy_rcp_mod(be, field, index, &value, 0);
}

/*
 * HFU_RCP
 */

int nthw_mod_tpe_hfu_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_hfu_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_hfu_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.hfu_rcp[index], (uint8_t)*value,
				sizeof(struct tpe_v1_hfu_v1_rcp_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.hfu_rcp, struct tpe_v1_hfu_v1_rcp_s, index,
				*value, be->tpe.nb_rcp_categories);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.hfu_rcp, struct tpe_v1_hfu_v1_rcp_s, index,
				*value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_WR:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_wr, value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_OUTER_L4_LEN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_outer_l4_len, value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_POS_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_pos_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_POS_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_pos_ofs, value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_ADD_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_add_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_ADD_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_add_ofs, value);
			break;

		case HW_TPE_HFU_RCP_LEN_A_SUB_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_a_sub_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_B_WR:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_b_wr, value);
			break;

		case HW_TPE_HFU_RCP_LEN_B_POS_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_b_pos_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_B_POS_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_b_pos_ofs, value);
			break;

		case HW_TPE_HFU_RCP_LEN_B_ADD_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_b_add_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_B_ADD_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_b_add_ofs, value);
			break;

		case HW_TPE_HFU_RCP_LEN_B_SUB_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_b_sub_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_C_WR:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_c_wr, value);
			break;

		case HW_TPE_HFU_RCP_LEN_C_POS_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_c_pos_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_C_POS_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_c_pos_ofs, value);
			break;

		case HW_TPE_HFU_RCP_LEN_C_ADD_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_c_add_dyn, value);
			break;

		case HW_TPE_HFU_RCP_LEN_C_ADD_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_c_add_ofs, value);
			break;

		case HW_TPE_HFU_RCP_LEN_C_SUB_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].len_c_sub_dyn, value);
			break;

		case HW_TPE_HFU_RCP_TTL_WR:
			GET_SET(be->tpe.v3.hfu_rcp[index].ttl_wr, value);
			break;

		case HW_TPE_HFU_RCP_TTL_POS_DYN:
			GET_SET(be->tpe.v3.hfu_rcp[index].ttl_pos_dyn, value);
			break;

		case HW_TPE_HFU_RCP_TTL_POS_OFS:
			GET_SET(be->tpe.v3.hfu_rcp[index].ttl_pos_ofs, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_hfu_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_hfu_rcp_mod(be, field, index, &value, 0);
}

/*
 * CSU_RCP
 */

int nthw_mod_tpe_csu_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_csu_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

static int hw_mod_tpe_csu_rcp_mod(struct flow_api_backend_s *be, enum hw_tpe_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 3:
		switch (field) {
		case HW_TPE_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->tpe.v3.csu_rcp[index], (uint8_t)*value,
				sizeof(struct tpe_v1_csu_v0_rcp_s));
			break;

		case HW_TPE_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->tpe.v3.csu_rcp, struct tpe_v1_csu_v0_rcp_s, index,
				*value, be->tpe.nb_rcp_categories);
			break;

		case HW_TPE_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->tpe.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->tpe.v3.csu_rcp, struct tpe_v1_csu_v0_rcp_s, index,
				*value);
			break;

		case HW_TPE_CSU_RCP_OUTER_L3_CMD:
			GET_SET(be->tpe.v3.csu_rcp[index].ol3_cmd, value);
			break;

		case HW_TPE_CSU_RCP_OUTER_L4_CMD:
			GET_SET(be->tpe.v3.csu_rcp[index].ol4_cmd, value);
			break;

		case HW_TPE_CSU_RCP_INNER_L3_CMD:
			GET_SET(be->tpe.v3.csu_rcp[index].il3_cmd, value);
			break;

		case HW_TPE_CSU_RCP_INNER_L4_CMD:
			GET_SET(be->tpe.v3.csu_rcp[index].il4_cmd, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_tpe_csu_rcp_set(struct flow_api_backend_s *be, enum hw_tpe_e field, int index,
	uint32_t value)
{
	return hw_mod_tpe_csu_rcp_mod(be, field, index, &value, 0);
}
