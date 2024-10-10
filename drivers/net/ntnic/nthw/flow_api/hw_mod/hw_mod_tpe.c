/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>

#include "hw_mod_backend.h"

#define _MOD_ "TPE"
#define _VER_ be->tpe.ver

bool hw_mod_tpe_present(struct flow_api_backend_s *be)
{
	return be->iface->get_tpe_present(be->be_dev);
}

int hw_mod_tpe_alloc(struct flow_api_backend_s *be)
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
		if (!callocate_mod((struct common_func_s *)&be->tpe, 10, &be->tpe.v3.rpp_rcp,
				be->tpe.nb_rcp_categories, sizeof(struct tpe_v1_rpp_v0_rcp_s),
				&be->tpe.v3.rpp_ifr_rcp, be->tpe.nb_ifr_categories,
				sizeof(struct tpe_v2_rpp_v1_ifr_rcp_s), &be->tpe.v3.ifr_rcp,
				be->tpe.nb_ifr_categories, sizeof(struct tpe_v2_ifr_v1_rcp_s),

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

void hw_mod_tpe_free(struct flow_api_backend_s *be)
{
	if (be->tpe.base) {
		free(be->tpe.base);
		be->tpe.base = NULL;
	}
}

int hw_mod_tpe_reset(struct flow_api_backend_s *be)
{
	int err = 0;

	/* Zero entire cache area */
	zero_module_cache((struct common_func_s *)(&be->tpe));

	NT_LOG(DBG, FILTER, "INIT TPE");
	err |= hw_mod_tpe_rpp_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_ins_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_rpl_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_rpl_ext_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_rpl_rpl_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_cpy_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_hfu_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_csu_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_rpp_ifr_rcp_flush(be, 0, ALL_ENTRIES);
	err |= hw_mod_tpe_ifr_rcp_flush(be, 0, ALL_ENTRIES);

	return err;
}

/*
 * RPP_IFR_RCP
 */

int hw_mod_tpe_rpp_ifr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_ifr_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_ifr_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpp_ifr_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * RPP_RCP
 */

int hw_mod_tpe_rpp_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpp_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * IFR_RCP
 */

int hw_mod_tpe_ifr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_ifr_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_ifr_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_ifr_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * INS_RCP
 */

int hw_mod_tpe_ins_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_ins_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * RPL_RCP
 */

int hw_mod_tpe_rpl_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpl_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * RPL_EXT
 */

int hw_mod_tpe_rpl_ext_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rpl_ext_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rpl_ext_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpl_ext_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * RPL_RPL
 */

int hw_mod_tpe_rpl_rpl_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rpl_depth;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rpl_depth) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_rpl_rpl_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * CPY_RCP
 */

int hw_mod_tpe_cpy_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
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

/*
 * HFU_RCP
 */

int hw_mod_tpe_hfu_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_hfu_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}

/*
 * CSU_RCP
 */

int hw_mod_tpe_csu_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->tpe.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->tpe.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->tpe_csu_rcp_flush(be->be_dev, &be->tpe, start_idx, count);
}
