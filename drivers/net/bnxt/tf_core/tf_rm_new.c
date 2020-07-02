/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>

#include <cfa_resource_types.h>

#include "tf_rm_new.h"
#include "tf_util.h"
#include "tf_session.h"
#include "tf_device.h"
#include "tfp.h"
#include "tf_msg.h"

/**
 * Generic RM Element data type that an RM DB is build upon.
 */
struct tf_rm_element {
	/**
	 * RM Element configuration type. If Private then the
	 * hcapi_type can be ignored. If Null then the element is not
	 * valid for the device.
	 */
	enum tf_rm_elem_cfg_type cfg_type;

	/**
	 * HCAPI RM Type for the element.
	 */
	uint16_t hcapi_type;

	/**
	 * HCAPI RM allocated range information for the element.
	 */
	struct tf_rm_alloc_info alloc;

	/**
	 * Bit allocator pool for the element. Pool size is controlled
	 * by the struct tf_session_resources at time of session creation.
	 * Null indicates that the element is not used for the device.
	 */
	struct bitalloc *pool;
};

/**
 * TF RM DB definition
 */
struct tf_rm_new_db {
	/**
	 * Number of elements in the DB
	 */
	uint16_t num_entries;

	/**
	 * Direction this DB controls.
	 */
	enum tf_dir dir;

	/**
	 * The DB consists of an array of elements
	 */
	struct tf_rm_element *db;
};


/**
 * Resource Manager Adjust of base index definitions.
 */
enum tf_rm_adjust_type {
	TF_RM_ADJUST_ADD_BASE, /**< Adds base to the index */
	TF_RM_ADJUST_RM_BASE   /**< Removes base from the index */
};

/**
 * Adjust an index according to the allocation information.
 *
 * All resources are controlled in a 0 based pool. Some resources, by
 * design, are not 0 based, i.e. Full Action Records (SRAM) thus they
 * need to be adjusted before they are handed out.
 *
 * [in] db
 *   Pointer to the db, used for the lookup
 *
 * [in] action
 *   Adjust action
 *
 * [in] db_index
 *   DB index for the element type
 *
 * [in] index
 *   Index to convert
 *
 * [out] adj_index
 *   Adjusted index
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP - Operation not supported
 */
static int
tf_rm_adjust_index(struct tf_rm_element *db,
		   enum tf_rm_adjust_type action,
		   uint32_t db_index,
		   uint32_t index,
		   uint32_t *adj_index)
{
	int rc = 0;
	uint32_t base_index;

	base_index = db[db_index].alloc.entry.start;

	switch (action) {
	case TF_RM_ADJUST_RM_BASE:
		*adj_index = index - base_index;
		break;
	case TF_RM_ADJUST_ADD_BASE:
		*adj_index = index + base_index;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return rc;
}

int
tf_rm_create_db(struct tf *tfp,
		struct tf_rm_create_db_parms *parms)
{
	int rc;
	int i;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	uint16_t max_types;
	struct tfp_calloc_parms cparms;
	struct tf_rm_resc_req_entry *query;
	enum tf_rm_resc_resv_strategy resv_strategy;
	struct tf_rm_resc_req_entry *req;
	struct tf_rm_resc_entry *resv;
	struct tf_rm_new_db *rm_db;
	struct tf_rm_element *db;
	uint32_t pool_size;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	/* Need device max number of elements for the RM QCAPS */
	rc = dev->ops->tf_dev_get_max_types(tfp, &max_types);
	if (rc)
		return rc;

	cparms.nitems = max_types;
	cparms.size = sizeof(struct tf_rm_resc_req_entry);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;

	query = (struct tf_rm_resc_req_entry *)cparms.mem_va;

	/* Get Firmware Capabilities */
	rc = tf_msg_session_resc_qcaps(tfp,
				       parms->dir,
				       max_types,
				       query,
				       &resv_strategy);
	if (rc)
		return rc;

	/* Process capabilities against db requirements */

	/* Alloc request, alignment already set */
	cparms.nitems = parms->num_elements;
	cparms.size = sizeof(struct tf_rm_resc_req_entry);
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	req = (struct tf_rm_resc_req_entry *)cparms.mem_va;

	/* Alloc reservation, alignment and nitems already set */
	cparms.size = sizeof(struct tf_rm_resc_entry);
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	resv = (struct tf_rm_resc_entry *)cparms.mem_va;

	/* Build the request */
	for (i = 0; i < parms->num_elements; i++) {
		/* Skip any non HCAPI cfg elements */
		if (parms->cfg[i].cfg_type == TF_RM_ELEM_CFG_HCAPI) {
			req[i].type = parms->cfg[i].hcapi_type;
			/* Check that we can get the full amount allocated */
			if (parms->alloc_num[i] <=
			    query[parms->cfg[i].hcapi_type].max) {
				req[i].min = parms->alloc_num[i];
				req[i].max = parms->alloc_num[i];
			} else {
				TFP_DRV_LOG(ERR,
					    "%s: Resource failure, type:%d\n",
					    tf_dir_2_str(parms->dir),
					    parms->cfg[i].hcapi_type);
				TFP_DRV_LOG(ERR,
					"req:%d, avail:%d\n",
					parms->alloc_num[i],
					query[parms->cfg[i].hcapi_type].max);
				return -EINVAL;
			}
		} else {
			/* Skip the element */
			req[i].type = CFA_RESOURCE_TYPE_INVALID;
		}
	}

	rc = tf_msg_session_resc_alloc(tfp,
				       parms->dir,
				       parms->num_elements,
				       req,
				       resv);
	if (rc)
		return rc;

	/* Build the RM DB per the request */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_rm_new_db);
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	rm_db = (void *)cparms.mem_va;

	/* Build the DB within RM DB */
	cparms.nitems = parms->num_elements;
	cparms.size = sizeof(struct tf_rm_element);
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	rm_db->db = (struct tf_rm_element *)cparms.mem_va;

	db = rm_db->db;
	for (i = 0; i < parms->num_elements; i++) {
		/* If allocation failed for a single entry the DB
		 * creation is considered a failure.
		 */
		if (parms->alloc_num[i] != resv[i].stride) {
			TFP_DRV_LOG(ERR,
				    "%s: Alloc failed, type:%d\n",
				    tf_dir_2_str(parms->dir),
				    i);
			TFP_DRV_LOG(ERR,
				    "req:%d, alloc:%d\n",
				    parms->alloc_num[i],
				    resv[i].stride);
			goto fail;
		}

		db[i].cfg_type = parms->cfg[i].cfg_type;
		db[i].hcapi_type = parms->cfg[i].hcapi_type;
		db[i].alloc.entry.start = resv[i].start;
		db[i].alloc.entry.stride = resv[i].stride;

		/* Create pool */
		pool_size = (BITALLOC_SIZEOF(resv[i].stride) /
			     sizeof(struct bitalloc));
		/* Alloc request, alignment already set */
		cparms.nitems = pool_size;
		cparms.size = sizeof(struct bitalloc);
		rc = tfp_calloc(&cparms);
		if (rc)
			return rc;
		db[i].pool = (struct bitalloc *)cparms.mem_va;
	}

	rm_db->num_entries = i;
	rm_db->dir = parms->dir;
	parms->rm_db = (void *)rm_db;

	tfp_free((void *)req);
	tfp_free((void *)resv);

	return 0;

 fail:
	tfp_free((void *)req);
	tfp_free((void *)resv);
	tfp_free((void *)db->pool);
	tfp_free((void *)db);
	tfp_free((void *)rm_db);
	parms->rm_db = NULL;

	return -EINVAL;
}

int
tf_rm_free_db(struct tf *tfp __rte_unused,
	      struct tf_rm_free_db_parms *parms)
{
	int rc = 0;
	int i;
	struct tf_rm_new_db *rm_db;

	/* Traverse the DB and clear each pool.
	 * NOTE:
	 *   Firmware is not cleared. It will be cleared on close only.
	 */
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	for (i = 0; i < rm_db->num_entries; i++)
		tfp_free((void *)rm_db->db->pool);

	tfp_free((void *)parms->rm_db);

	return rc;
}

int
tf_rm_allocate(struct tf_rm_allocate_parms *parms)
{
	int rc = 0;
	int id;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	if (parms == NULL || parms->rm_db == NULL)
		return -EINVAL;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	id = ba_alloc(rm_db->db[parms->db_index].pool);
	if (id == BA_FAIL) {
		TFP_DRV_LOG(ERR,
			    "%s: Allocation failed, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    strerror(-rc));
		return -ENOMEM;
	}

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_ADD_BASE,
				parms->db_index,
				id,
				parms->index);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Alloc adjust of base index failed, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    strerror(-rc));
		return -1;
	}

	return rc;
}

int
tf_rm_free(struct tf_rm_free_parms *parms)
{
	int rc = 0;
	uint32_t adj_index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	if (parms == NULL || parms->rm_db == NULL)
		return -EINVAL;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_RM_BASE,
				parms->db_index,
				parms->index,
				&adj_index);
	if (rc)
		return rc;

	rc = ba_free(rm_db->db[parms->db_index].pool, adj_index);
	/* No logging direction matters and that is not available here */
	if (rc)
		return rc;

	return rc;
}

int
tf_rm_is_allocated(struct tf_rm_is_allocated_parms *parms)
{
	int rc = 0;
	uint32_t adj_index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	if (parms == NULL || parms->rm_db == NULL)
		return -EINVAL;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_RM_BASE,
				parms->db_index,
				parms->index,
				&adj_index);
	if (rc)
		return rc;

	*parms->allocated = ba_inuse(rm_db->db[parms->db_index].pool,
				     adj_index);

	return rc;
}

int
tf_rm_get_info(struct tf_rm_get_alloc_info_parms *parms)
{
	int rc = 0;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	if (parms == NULL || parms->rm_db == NULL)
		return -EINVAL;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	parms->info = &rm_db->db[parms->db_index].alloc;

	return rc;
}

int
tf_rm_get_hcapi_type(struct tf_rm_get_hcapi_parms *parms)
{
	int rc = 0;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	if (parms == NULL || parms->rm_db == NULL)
		return -EINVAL;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	*parms->hcapi_type = rm_db->db[parms->db_index].hcapi_type;

	return rc;
}
