/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "QSL"
#define _VER_ be->qsl.ver

#define QSL_QEN_ENTRIES 32
#define QSL_QNMQ_ENTRIES 256

bool nthw_mod_qsl_present(struct flow_api_backend_s *be)
{
	return be->iface->get_qsl_present(be->be_dev);
}

int nthw_mod_qsl_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_qsl_version(be->be_dev);
	NT_LOG(DBG, FILTER, "QSL MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	nb = be->iface->get_nb_qsl_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(qsl_categories);
		return COUNT_ERROR;
	}
	be->qsl.nb_rcp_categories = (uint32_t)nb;

	nb = be->iface->get_nb_qsl_qst_entries(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(qsl_qst_entries);
		return COUNT_ERROR;
	}

	be->qsl.nb_qst_entries = (uint32_t)nb;

	switch (_VER_) {
	case 7:
		if (!nthw_callocate_mod((struct common_func_s *)&be->qsl, 4, &be->qsl.v7.rcp,
				be->qsl.nb_rcp_categories, sizeof(struct qsl_v7_rcp_s),
				&be->qsl.v7.qst, be->qsl.nb_qst_entries,
				sizeof(struct qsl_v7_qst_s), &be->qsl.v7.qen, QSL_QEN_ENTRIES,
				sizeof(struct qsl_v7_qen_s), &be->qsl.v7.unmq, QSL_QNMQ_ENTRIES,
				sizeof(struct qsl_v7_unmq_s)))
			return -1;

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void nthw_mod_qsl_free(struct flow_api_backend_s *be)
{
	if (be->qsl.base) {
		free(be->qsl.base);
		be->qsl.base = NULL;
	}
}

int nthw_mod_qsl_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->qsl));

	NT_LOG(DBG, FILTER, "INIT QSL RCP");
	nthw_mod_qsl_rcp_flush(be, 0, ALL_ENTRIES);

	NT_LOG(DBG, FILTER, "INIT QSL QST");
	nthw_mod_qsl_qst_flush(be, 0, ALL_ENTRIES);

	NT_LOG(DBG, FILTER, "INIT QSL QEN");
	nthw_mod_qsl_qen_flush(be, 0, ALL_ENTRIES);

	NT_LOG(DBG, FILTER, "INIT QSL UNMQ");
	be->iface->qsl_unmq_flush(be->be_dev, &be->qsl, 0, 256);

	return 0;
}

int nthw_mod_qsl_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->qsl.nb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->qsl.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}
	return be->iface->qsl_rcp_flush(be->be_dev, &be->qsl, start_idx, count);
}

static int hw_mod_qsl_rcp_mod(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t *value, int get)
{
	if (index >= be->qsl.nb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 7:
		switch (field) {
		case HW_QSL_RCP_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->qsl.v7.rcp[index], (uint8_t)*value,
				sizeof(struct qsl_v7_rcp_s));
			break;

		case HW_QSL_RCP_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->qsl.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->qsl.v7.rcp, struct qsl_v7_rcp_s, index, *value,
				be->qsl.nb_rcp_categories);
			break;

		case HW_QSL_RCP_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->qsl.nb_rcp_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->qsl.v7.rcp, struct qsl_v7_rcp_s, index, *value);
			break;

		case HW_QSL_RCP_DISCARD:
			GET_SET(be->qsl.v7.rcp[index].discard, value);
			break;

		case HW_QSL_RCP_DROP:
			GET_SET(be->qsl.v7.rcp[index].drop, value);
			break;

		case HW_QSL_RCP_TBL_LO:
			GET_SET(be->qsl.v7.rcp[index].tbl_lo, value);
			break;

		case HW_QSL_RCP_TBL_HI:
			GET_SET(be->qsl.v7.rcp[index].tbl_hi, value);
			break;

		case HW_QSL_RCP_TBL_IDX:
			GET_SET(be->qsl.v7.rcp[index].tbl_idx, value);
			break;

		case HW_QSL_RCP_TBL_MSK:
			GET_SET(be->qsl.v7.rcp[index].tbl_msk, value);
			break;

		case HW_QSL_RCP_LR:
			GET_SET(be->qsl.v7.rcp[index].lr, value);
			break;

		case HW_QSL_RCP_TSA:
			GET_SET(be->qsl.v7.rcp[index].tsa, value);
			break;

		case HW_QSL_RCP_VLI:
			GET_SET(be->qsl.v7.rcp[index].vli, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_qsl_rcp_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value)
{
	return hw_mod_qsl_rcp_mod(be, field, index, &value, 0);
}

int nthw_mod_qsl_qst_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->qsl.nb_qst_entries;

	if ((unsigned int)(start_idx + count) > be->qsl.nb_qst_entries) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->qsl_qst_flush(be->be_dev, &be->qsl, start_idx, count);
}

static int hw_mod_qsl_qst_mod(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t *value, int get)
{
	if (index >= be->qsl.nb_qst_entries) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 7:
		switch (field) {
		case HW_QSL_QST_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->qsl.v7.qst[index], (uint8_t)*value,
				sizeof(struct qsl_v7_qst_s));
			break;

		case HW_QSL_QST_QUEUE:
			GET_SET(be->qsl.v7.qst[index].queue, value);
			break;

		case HW_QSL_QST_EN:
			GET_SET(be->qsl.v7.qst[index].en, value);
			break;

		case HW_QSL_QST_TX_PORT:
			GET_SET(be->qsl.v7.qst[index].tx_port, value);
			break;

		case HW_QSL_QST_LRE:
			GET_SET(be->qsl.v7.qst[index].lre, value);
			break;

		case HW_QSL_QST_TCI:
			GET_SET(be->qsl.v7.qst[index].tci, value);
			break;

		case HW_QSL_QST_VEN:
			GET_SET(be->qsl.v7.qst[index].ven, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_qsl_qst_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value)
{
	return hw_mod_qsl_qst_mod(be, field, index, &value, 0);
}

int nthw_mod_qsl_qen_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = QSL_QEN_ENTRIES;

	if ((start_idx + count) > QSL_QEN_ENTRIES) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->qsl_qen_flush(be->be_dev, &be->qsl, start_idx, count);
}

static int hw_mod_qsl_qen_mod(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t *value, int get)
{
	if (index >= QSL_QEN_ENTRIES) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 7:
		switch (field) {
		case HW_QSL_QEN_EN:
			GET_SET(be->qsl.v7.qen[index].en, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_qsl_qen_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value)
{
	return hw_mod_qsl_qen_mod(be, field, index, &value, 0);
}

int nthw_mod_qsl_qen_get(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t *value)
{
	return hw_mod_qsl_qen_mod(be, field, index, value, 1);
}

int nthw_mod_qsl_unmq_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = QSL_QNMQ_ENTRIES;

	if ((start_idx + count) > QSL_QNMQ_ENTRIES) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->qsl_unmq_flush(be->be_dev, &be->qsl, start_idx, count);
}

static int hw_mod_qsl_unmq_mod(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t *value, int get)
{
	if (index >= QSL_QNMQ_ENTRIES) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 7:
		switch (field) {
		case HW_QSL_UNMQ_DEST_QUEUE:
			GET_SET(be->qsl.v7.unmq[index].dest_queue, value);
			break;

		case HW_QSL_UNMQ_EN:
			GET_SET(be->qsl.v7.unmq[index].en, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	/* end case 7 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_qsl_unmq_set(struct flow_api_backend_s *be, enum hw_qsl_e field, uint32_t index,
	uint32_t value)
{
	return hw_mod_qsl_unmq_mod(be, field, index, &value, 0);
}
