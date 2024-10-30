/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */


#include "flow_api_hw_db_inline.h"

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
