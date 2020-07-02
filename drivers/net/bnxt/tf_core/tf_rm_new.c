/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>

#include <cfa_resource_types.h>

#include "tf_rm_new.h"
#include "tf_common.h"
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
 * Adjust an index according to the allocation information.
 *
 * All resources are controlled in a 0 based pool. Some resources, by
 * design, are not 0 based, i.e. Full Action Records (SRAM) thus they
 * need to be adjusted before they are handed out.
 *
 * [in] cfg
 *   Pointer to the DB configuration
 *
 * [in] reservations
 *   Pointer to the allocation values associated with the module
 *
 * [in] count
 *   Number of DB configuration elements
 *
 * [out] valid_count
 *   Number of HCAPI entries with a reservation value greater than 0
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP - Operation not supported
 */
static void
tf_rm_count_hcapi_reservations(struct tf_rm_element_cfg *cfg,
			       uint16_t *reservations,
			       uint16_t count,
			       uint16_t *valid_count)
{
	int i;
	uint16_t cnt = 0;

	for (i = 0; i < count; i++) {
		if (cfg[i].cfg_type == TF_RM_ELEM_CFG_HCAPI &&
		    reservations[i] > 0)
			cnt++;
	}

	*valid_count = cnt;
}

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
	int j;
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
	uint16_t hcapi_items;

	TF_CHECK_PARMS2(tfp, parms);

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

	/* Process capabilities against DB requirements. However, as a
	 * DB can hold elements that are not HCAPI we can reduce the
	 * req msg content by removing those out of the request yet
	 * the DB holds them all as to give a fast lookup. We can also
	 * remove entries where there are no request for elements.
	 */
	tf_rm_count_hcapi_reservations(parms->cfg,
				       parms->alloc_cnt,
				       parms->num_elements,
				       &hcapi_items);

	/* Alloc request, alignment already set */
	cparms.nitems = (size_t)hcapi_items;
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
	for (i = 0, j = 0; i < parms->num_elements; i++) {
		/* Skip any non HCAPI cfg elements */
		if (parms->cfg[i].cfg_type == TF_RM_ELEM_CFG_HCAPI) {
			/* Only perform reservation for entries that
			 * has been requested
			 */
			if (parms->alloc_cnt[i] == 0)
				continue;

			/* Verify that we can get the full amount
			 * allocated per the qcaps availability.
			 */
			if (parms->alloc_cnt[i] <=
			    query[parms->cfg[i].hcapi_type].max) {
				req[j].type = parms->cfg[i].hcapi_type;
				req[j].min = parms->alloc_cnt[i];
				req[j].max = parms->alloc_cnt[i];
				j++;
			} else {
				TFP_DRV_LOG(ERR,
					    "%s: Resource failure, type:%d\n",
					    tf_dir_2_str(parms->dir),
					    parms->cfg[i].hcapi_type);
				TFP_DRV_LOG(ERR,
					"req:%d, avail:%d\n",
					parms->alloc_cnt[i],
					query[parms->cfg[i].hcapi_type].max);
				return -EINVAL;
			}
		}
	}

	rc = tf_msg_session_resc_alloc(tfp,
				       parms->dir,
				       hcapi_items,
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
	for (i = 0, j = 0; i < parms->num_elements; i++) {
		db[i].cfg_type = parms->cfg[i].cfg_type;
		db[i].hcapi_type = parms->cfg[i].hcapi_type;

		/* Skip any non HCAPI types as we didn't include them
		 * in the reservation request.
		 */
		if (parms->cfg[i].cfg_type != TF_RM_ELEM_CFG_HCAPI)
			continue;

		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (parms->alloc_cnt[i] == 0)
			continue;

		/* If the element had requested an allocation and that
		 * allocation was a success (full amount) then
		 * allocate the pool.
		 */
		if (parms->alloc_cnt[i] == resv[j].stride) {
			db[i].alloc.entry.start = resv[j].start;
			db[i].alloc.entry.stride = resv[j].stride;

			/* Create pool */
			pool_size = (BITALLOC_SIZEOF(resv[j].stride) /
				     sizeof(struct bitalloc));
			/* Alloc request, alignment already set */
			cparms.nitems = pool_size;
			cparms.size = sizeof(struct bitalloc);
			rc = tfp_calloc(&cparms);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "%s: Pool alloc failed, type:%d\n",
					    tf_dir_2_str(parms->dir),
					    db[i].cfg_type);
				goto fail;
			}
			db[i].pool = (struct bitalloc *)cparms.mem_va;

			rc = ba_init(db[i].pool, resv[j].stride);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "%s: Pool init failed, type:%d\n",
					    tf_dir_2_str(parms->dir),
					    db[i].cfg_type);
				goto fail;
			}
			j++;
		} else {
			/* Bail out as we want what we requested for
			 * all elements, not any less.
			 */
			TFP_DRV_LOG(ERR,
				    "%s: Alloc failed, type:%d\n",
				    tf_dir_2_str(parms->dir),
				    db[i].cfg_type);
			TFP_DRV_LOG(ERR,
				    "req:%d, alloc:%d\n",
				    parms->alloc_cnt[i],
				    resv[j].stride);
			goto fail;
		}
	}

	rm_db->num_entries = i;
	rm_db->dir = parms->dir;
	*parms->rm_db = (void *)rm_db;

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

	TF_CHECK_PARMS1(parms);

	/* Traverse the DB and clear each pool.
	 * NOTE:
	 *   Firmware is not cleared. It will be cleared on close only.
	 */
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	for (i = 0; i < rm_db->num_entries; i++)
		tfp_free((void *)rm_db->db[i].pool);

	tfp_free((void *)parms->rm_db);

	return rc;
}

int
tf_rm_allocate(struct tf_rm_allocate_parms *parms)
{
	int rc = 0;
	int id;
	uint32_t index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	TF_CHECK_PARMS2(parms, parms->rm_db);

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	/* Bail out if the pool is not valid, should never happen */
	if (rm_db->db[parms->db_index].pool == NULL) {
		rc = -ENOTSUP;
		TFP_DRV_LOG(ERR,
			    "%s: Invalid pool for this type:%d, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    parms->db_index,
			    strerror(-rc));
		return rc;
	}

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
				&index);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Alloc adjust of base index failed, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    strerror(-rc));
		return -EINVAL;
	}

	*parms->index = index;

	return rc;
}

int
tf_rm_free(struct tf_rm_free_parms *parms)
{
	int rc = 0;
	uint32_t adj_index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	TF_CHECK_PARMS2(parms, parms->rm_db);

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	/* Bail out if the pool is not valid, should never happen */
	if (rm_db->db[parms->db_index].pool == NULL) {
		rc = -ENOTSUP;
		TFP_DRV_LOG(ERR,
			    "%s: Invalid pool for this type:%d, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    parms->db_index,
			    strerror(-rc));
		return rc;
	}

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

	TF_CHECK_PARMS2(parms, parms->rm_db);

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	/* Bail out if the pool is not valid, should never happen */
	if (rm_db->db[parms->db_index].pool == NULL) {
		rc = -ENOTSUP;
		TFP_DRV_LOG(ERR,
			    "%s: Invalid pool for this type:%d, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    parms->db_index,
			    strerror(-rc));
		return rc;
	}

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

	TF_CHECK_PARMS2(parms, parms->rm_db);

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

	TF_CHECK_PARMS2(parms, parms->rm_db);

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	cfg_type = rm_db->db[parms->db_index].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI &&
	    cfg_type != TF_RM_ELEM_CFG_PRIVATE)
		return -ENOTSUP;

	*parms->hcapi_type = rm_db->db[parms->db_index].hcapi_type;

	return rc;
}
