/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */


#include "hw_mod_backend.h"
#include "flow_api_engine.h"

#include "flow_api_hw_db_inline.h"
#include "rte_common.h"

/******************************************************************************/
/* Handle                                                                     */
/******************************************************************************/

struct hw_db_inline_resource_db {
	/* Actions */
	struct hw_db_inline_resource_db_cot {
		struct hw_db_inline_cot_data data;
		int ref;
	} *cot;

	uint32_t nb_cot;

	/* Hardware */

	struct hw_db_inline_resource_db_cfn {
		uint64_t priority;
		int cfn_hw;
		int ref;
	} *cfn;
};

int hw_db_inline_create(struct flow_nic_dev *ndev, void **db_handle)
{
	/* Note: calloc is required for functionality in the hw_db_inline_destroy() */
	struct hw_db_inline_resource_db *db = calloc(1, sizeof(struct hw_db_inline_resource_db));

	if (db == NULL)
		return -1;

	db->nb_cot = ndev->be.cat.nb_cat_funcs;
	db->cot = calloc(db->nb_cot, sizeof(struct hw_db_inline_resource_db_cot));

	if (db->cot == NULL) {
		hw_db_inline_destroy(db);
		return -1;
	}

	*db_handle = db;
	return 0;
}

void hw_db_inline_destroy(void *db_handle)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	free(db->cot);

	free(db->cfn);

	free(db);
}

void hw_db_inline_deref_idxs(struct flow_nic_dev *ndev, void *db_handle, struct hw_db_idx *idxs,
	uint32_t size)
{
	for (uint32_t i = 0; i < size; ++i) {
		switch (idxs[i].type) {
		case HW_DB_IDX_TYPE_NONE:
			break;

		case HW_DB_IDX_TYPE_COT:
			hw_db_inline_cot_deref(ndev, db_handle, *(struct hw_db_cot_idx *)&idxs[i]);
			break;

		default:
			break;
		}
	}
}

/******************************************************************************/
/* COT                                                                        */
/******************************************************************************/

static int hw_db_inline_cot_compare(const struct hw_db_inline_cot_data *data1,
	const struct hw_db_inline_cot_data *data2)
{
	return data1->matcher_color_contrib == data2->matcher_color_contrib &&
		data1->frag_rcp == data2->frag_rcp;
}

struct hw_db_cot_idx hw_db_inline_cot_add(struct flow_nic_dev *ndev, void *db_handle,
	const struct hw_db_inline_cot_data *data)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;
	struct hw_db_cot_idx idx = { .raw = 0 };
	int found = 0;

	idx.type = HW_DB_IDX_TYPE_COT;

	for (uint32_t i = 1; i < db->nb_cot; ++i) {
		int ref = db->cot[i].ref;

		if (ref > 0 && hw_db_inline_cot_compare(data, &db->cot[i].data)) {
			idx.ids = i;
			hw_db_inline_cot_ref(ndev, db, idx);
			return idx;
		}

		if (!found && ref <= 0) {
			found = 1;
			idx.ids = i;
		}
	}

	if (!found) {
		idx.error = 1;
		return idx;
	}

	db->cot[idx.ids].ref = 1;
	memcpy(&db->cot[idx.ids].data, data, sizeof(struct hw_db_inline_cot_data));

	return idx;
}

void hw_db_inline_cot_ref(struct flow_nic_dev *ndev __rte_unused, void *db_handle,
	struct hw_db_cot_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (!idx.error)
		db->cot[idx.ids].ref += 1;
}

void hw_db_inline_cot_deref(struct flow_nic_dev *ndev __rte_unused, void *db_handle,
	struct hw_db_cot_idx idx)
{
	struct hw_db_inline_resource_db *db = (struct hw_db_inline_resource_db *)db_handle;

	if (idx.error)
		return;

	db->cot[idx.ids].ref -= 1;

	if (db->cot[idx.ids].ref <= 0) {
		memset(&db->cot[idx.ids].data, 0x0, sizeof(struct hw_db_inline_cot_data));
		db->cot[idx.ids].ref = 0;
	}
}
