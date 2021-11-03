/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_debug.h>

#include <cfa_resource_types.h>

#include "tf_rm.h"
#include "tf_common.h"
#include "tf_util.h"
#include "tf_session.h"
#include "tf_device.h"
#include "tfp.h"
#include "tf_msg.h"

/* Logging defines */
#define TF_RM_DEBUG  0

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
	 * Resource slices.  How many slices will fit in the
	 * resource pool chunk size.
	 */
	uint8_t slices;

	/**
	 * HCAPI RM allocated range information for the element.
	 */
	struct tf_rm_alloc_info alloc;

	/**
	 * If cfg_type == HCAPI_BA_CHILD, this field indicates
	 * the parent module subtype for look up into the parent pool.
	 * An example subtype is TF_TBL_TYPE_FULL_ACT_RECORD which is a
	 * module subtype of TF_MODULE_TYPE_TABLE.
	 */
	uint16_t parent_subtype;

	/**
	 * Bit allocator pool for the element. Pool size is controlled
	 * by the struct tf_session_resources at time of session creation.
	 * Null indicates that the pool is not used for the element.
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
	 * Module type, used for logging purposes.
	 */
	enum tf_module_type module;

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
tf_rm_count_hcapi_reservations(enum tf_dir dir,
			       enum tf_module_type module,
			       struct tf_rm_element_cfg *cfg,
			       uint16_t *reservations,
			       uint16_t count,
			       uint16_t *valid_count)
{
	int i;
	uint16_t cnt = 0;

	for (i = 0; i < count; i++) {
		if (cfg[i].cfg_type != TF_RM_ELEM_CFG_NULL &&
		    reservations[i] > 0)
			cnt++;

		/* Only log msg if a type is attempted reserved and
		 * not supported. We ignore EM module as its using a
		 * split configuration array thus it would fail for
		 * this type of check.
		 */
		if (module != TF_MODULE_TYPE_EM &&
		    cfg[i].cfg_type == TF_RM_ELEM_CFG_NULL &&
		    reservations[i] > 0) {
			TFP_DRV_LOG(ERR,
				"%s, %s, %s allocation of %d not supported\n",
				tf_module_2_str(module),
				tf_dir_2_str(dir),
				tf_module_subtype_2_str(module, i),
				reservations[i]);
		}
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
 * [in] subtype
 *   TF module subtype used as an index into the database.
 *   An example subtype is TF_TBL_TYPE_FULL_ACT_RECORD which is a
 *   module subtype of TF_MODULE_TYPE_TABLE.
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
		   uint32_t subtype,
		   uint32_t index,
		   uint32_t *adj_index)
{
	int rc = 0;
	uint32_t base_index;

	base_index = db[subtype].alloc.entry.start;

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

/**
 * Logs an array of found residual entries to the console.
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * [in] module
 *   Type of Device Module
 *
 * [in] count
 *   Number of entries in the residual array
 *
 * [in] residuals
 *   Pointer to an array of residual entries. Array is index same as
 *   the DB in which this function is used. Each entry holds residual
 *   value for that entry.
 */
#if (TF_RM_DEBUG == 1)
static void
tf_rm_log_residuals(enum tf_dir dir,
		    enum tf_module_type module,
		    uint16_t count,
		    uint16_t *residuals)
{
	int i;

	/* Walk the residual array and log the types that wasn't
	 * cleaned up to the console.
	 */
	for (i = 0; i < count; i++) {
		if (residuals[i] != 0)
			TFP_DRV_LOG(INFO,
				"%s, %s was not cleaned up, %d outstanding\n",
				tf_dir_2_str(dir),
				tf_module_subtype_2_str(module, i),
				residuals[i]);
	}
}
#endif /* TF_RM_DEBUG == 1 */
/**
 * Performs a check of the passed in DB for any lingering elements. If
 * a resource type was found to not have been cleaned up by the caller
 * then its residual values are recorded, logged and passed back in an
 * allocate reservation array that the caller can pass to the FW for
 * cleanup.
 *
 * [in] db
 *   Pointer to the db, used for the lookup
 *
 * [out] resv_size
 *   Pointer to the reservation size of the generated reservation
 *   array.
 *
 * [in/out] resv
 *   Pointer Pointer to a reservation array. The reservation array is
 *   allocated after the residual scan and holds any found residual
 *   entries. Thus it can be smaller than the DB that the check was
 *   performed on. Array must be freed by the caller.
 *
 * [out] residuals_present
 *   Pointer to a bool flag indicating if residual was present in the
 *   DB
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP - Operation not supported
 */
static int
tf_rm_check_residuals(struct tf_rm_new_db *rm_db,
		      uint16_t *resv_size,
		      struct tf_rm_resc_entry **resv,
		      bool *residuals_present)
{
	int rc;
	int i;
	int f;
	uint16_t count;
	uint16_t found;
	uint16_t *residuals = NULL;
	uint16_t hcapi_type;
	struct tf_rm_get_inuse_count_parms iparms;
	struct tf_rm_get_alloc_info_parms aparms;
	struct tf_rm_get_hcapi_parms hparms;
	struct tf_rm_alloc_info info;
	struct tfp_calloc_parms cparms;
	struct tf_rm_resc_entry *local_resv = NULL;

	/* Create array to hold the entries that have residuals */
	cparms.nitems = rm_db->num_entries;
	cparms.size = sizeof(uint16_t);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;

	residuals = (uint16_t *)cparms.mem_va;

	/* Traverse the DB and collect any residual elements */
	iparms.rm_db = rm_db;
	iparms.count = &count;
	for (i = 0, found = 0; i < rm_db->num_entries; i++) {
		iparms.subtype = i;
		rc = tf_rm_get_inuse_count(&iparms);
		/* Not a device supported entry, just skip */
		if (rc == -ENOTSUP)
			continue;
		if (rc)
			goto cleanup_residuals;

		if (count) {
			found++;
			residuals[i] = count;
			*residuals_present = true;
		}
	}

	if (*residuals_present) {
		/* Populate a reduced resv array with only the entries
		 * that have residuals.
		 */
		cparms.nitems = found;
		cparms.size = sizeof(struct tf_rm_resc_entry);
		cparms.alignment = 0;
		rc = tfp_calloc(&cparms);
		if (rc)
			return rc;

		local_resv = (struct tf_rm_resc_entry *)cparms.mem_va;

		aparms.rm_db = rm_db;
		hparms.rm_db = rm_db;
		hparms.hcapi_type = &hcapi_type;
		for (i = 0, f = 0; i < rm_db->num_entries; i++) {
			if (residuals[i] == 0)
				continue;
			aparms.subtype = i;
			aparms.info = &info;
			rc = tf_rm_get_info(&aparms);
			if (rc)
				goto cleanup_all;

			hparms.subtype = i;
			rc = tf_rm_get_hcapi_type(&hparms);
			if (rc)
				goto cleanup_all;

			local_resv[f].type = hcapi_type;
			local_resv[f].start = info.entry.start;
			local_resv[f].stride = info.entry.stride;
			f++;
		}
		*resv_size = found;
	}

#if (TF_RM_DEBUG == 1)
	tf_rm_log_residuals(rm_db->dir,
			    rm_db->module,
			    rm_db->num_entries,
			    residuals);
#endif
	tfp_free((void *)residuals);
	*resv = local_resv;

	return 0;

 cleanup_all:
	tfp_free((void *)local_resv);
	*resv = NULL;
 cleanup_residuals:
	tfp_free((void *)residuals);

	return rc;
}

/**
 * Some resources do not have a 1:1 mapping between the Truflow type and the cfa
 * resource type (HCAPI RM).  These resources have multiple Truflow types which
 * map to a single HCAPI RM type.  In order to support this, one Truflow type
 * sharing the HCAPI resources is designated the parent.  All other Truflow
 * types associated with that HCAPI RM type are designated the children.
 *
 * This function updates the resource counts of any HCAPI_BA_PARENT with the
 * counts of the HCAPI_BA_CHILDREN.  These are read from the alloc_cnt and
 * written back to the req_cnt.
 *
 * [in] cfg
 *   Pointer to an array of module specific Truflow type indexed RM cfg items
 *
 * [in] alloc_cnt
 *   Pointer to the tf_open_session() configured array of module specific
 *   Truflow type indexed requested counts.
 *
 * [in/out] req_cnt
 *   Pointer to the location to put the updated resource counts.
 *
 * Returns:
 *     0          - Success
 *     -          - Failure if negative
 */
static int
tf_rm_update_parent_reservations(struct tf *tfp,
				 struct tf_dev_info *dev,
				 struct tf_rm_element_cfg *cfg,
				 uint16_t *alloc_cnt,
				 uint16_t num_elements,
				 uint16_t *req_cnt,
				 bool shared_session)
{
	int parent, child;
	const char *type_str;

	/* Search through all the elements */
	for (parent = 0; parent < num_elements; parent++) {
		uint16_t combined_cnt = 0;

		/* If I am a parent */
		if (cfg[parent].cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
			uint8_t p_slices = 1;

			/* Shared session doesn't support slices */
			if (!shared_session)
				p_slices = cfg[parent].slices;

			RTE_ASSERT(p_slices);

			combined_cnt = alloc_cnt[parent] / p_slices;

			if (alloc_cnt[parent] % p_slices)
				combined_cnt++;

			if (alloc_cnt[parent]) {
				dev->ops->tf_dev_get_resource_str(tfp,
							 cfg[parent].hcapi_type,
							 &type_str);
#if (TF_RM_DEBUG == 1)
				printf("%s:%s cnt(%d) slices(%d)\n",
				       type_str, tf_tbl_type_2_str(parent),
				       alloc_cnt[parent], p_slices);
#endif /* (TF_RM_DEBUG == 1) */
			}

			/* Search again through all the elements */
			for (child = 0; child < num_elements; child++) {
				/* If this is one of my children */
				if (cfg[child].cfg_type ==
				    TF_RM_ELEM_CFG_HCAPI_BA_CHILD &&
				    cfg[child].parent_subtype == parent &&
				    alloc_cnt[child]) {
					uint8_t c_slices = 1;
					uint16_t cnt = 0;

					if (!shared_session)
						c_slices = cfg[child].slices;

					RTE_ASSERT(c_slices);

					dev->ops->tf_dev_get_resource_str(tfp,
							  cfg[child].hcapi_type,
							   &type_str);
#if (TF_RM_DEBUG == 1)
					printf("%s:%s cnt(%d) slices(%d)\n",
					       type_str,
					       tf_tbl_type_2_str(child),
					       alloc_cnt[child],
					       c_slices);
#endif /* (TF_RM_DEBUG == 1) */
					/* Increment the parents combined count
					 * with each child's count adjusted for
					 * number of slices per RM alloc item.
					 */
					cnt = alloc_cnt[child] / c_slices;

					if (alloc_cnt[child] % c_slices)
						cnt++;

					combined_cnt += cnt;
					/* Clear the requested child count */
					req_cnt[child] = 0;
				}
			}
			/* Save the parent count to be requested */
			req_cnt[parent] = combined_cnt;
#if (TF_RM_DEBUG == 1)
			printf("%s calculated total:%d\n\n",
			       type_str, req_cnt[parent]);
#endif /* (TF_RM_DEBUG == 1) */
		}
	}
	return 0;
}

int
tf_rm_create_db(struct tf *tfp,
		struct tf_rm_create_db_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int i, j;
	uint16_t max_types, hcapi_items, *req_cnt;
	struct tfp_calloc_parms cparms;
	struct tf_rm_resc_req_entry *query;
	enum tf_rm_resc_resv_strategy resv_strategy;
	struct tf_rm_resc_req_entry *req;
	struct tf_rm_resc_entry *resv;
	struct tf_rm_new_db *rm_db;
	struct tf_rm_element *db;
	uint32_t pool_size;
	bool shared_session = 0;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	/* Need device max number of elements for the RM QCAPS */
	rc = dev->ops->tf_dev_get_max_types(tfp, &max_types);

	/* Allocate memory for RM QCAPS request */
	cparms.nitems = max_types;
	cparms.size = sizeof(struct tf_rm_resc_req_entry);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;

	query = (struct tf_rm_resc_req_entry *)cparms.mem_va;

	/* Get Firmware Capabilities */
	rc = tf_msg_session_resc_qcaps(tfp,
				       dev,
				       parms->dir,
				       max_types,
				       query,
				       &resv_strategy,
				       NULL);
	if (rc)
		return rc;

	/* Copy requested counts (alloc_cnt) from tf_open_session() to local
	 * copy (req_cnt) so that it can be updated if required.
	 */

	cparms.nitems = parms->num_elements;
	cparms.size = sizeof(uint16_t);
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;

	req_cnt = (uint16_t *)cparms.mem_va;

	tfp_memcpy(req_cnt, parms->alloc_cnt,
		   parms->num_elements * sizeof(uint16_t));

	shared_session = tf_session_is_shared_session(tfs);

	/* Update the req_cnt based upon the element configuration
	 */
	tf_rm_update_parent_reservations(tfp, dev, parms->cfg,
					 parms->alloc_cnt,
					 parms->num_elements,
					 req_cnt,
					 shared_session);

	/* Process capabilities against DB requirements. However, as a
	 * DB can hold elements that are not HCAPI we can reduce the
	 * req msg content by removing those out of the request yet
	 * the DB holds them all as to give a fast lookup. We can also
	 * remove entries where there are no request for elements.
	 */
	tf_rm_count_hcapi_reservations(parms->dir,
				       parms->module,
				       parms->cfg,
				       req_cnt,
				       parms->num_elements,
				       &hcapi_items);

	if (hcapi_items == 0) {
#if (TF_RM_DEBUG == 1)
		TFP_DRV_LOG(INFO,
			"%s: module: %s Empty RM DB create request\n",
			tf_dir_2_str(parms->dir),
			tf_module_2_str(parms->module));
#endif
		parms->rm_db = NULL;
		return -ENOMEM;
	}

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
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		uint16_t hcapi_type = cfg->hcapi_type;

		/* Only perform reservation for requested entries
		 */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children in the request */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {

			/* Verify that we can get the full amount per qcaps.
			 */
			if (req_cnt[i] <= query[hcapi_type].max) {
				req[j].type = hcapi_type;
				req[j].min = req_cnt[i];
				req[j].max = req_cnt[i];
				j++;
			} else {
				const char *type_str;

				dev->ops->tf_dev_get_resource_str(tfp,
							      hcapi_type,
							      &type_str);
				TFP_DRV_LOG(ERR,
					"Failure, %s:%d:%s req:%d avail:%d\n",
					tf_dir_2_str(parms->dir),
					hcapi_type, type_str,
					req_cnt[i],
					query[hcapi_type].max);
				return -EINVAL;
			}
		}
	}

	/* Allocate all resources for the module type
	 */
	rc = tf_msg_session_resc_alloc(tfp,
				       dev,
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
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		const char *type_str;

		dev->ops->tf_dev_get_resource_str(tfp,
						  cfg->hcapi_type,
						  &type_str);

		db[i].cfg_type = cfg->cfg_type;
		db[i].hcapi_type = cfg->hcapi_type;
		db[i].slices = cfg->slices;

		/* Save the parent subtype for later use to find the pool
		 */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
			db[i].parent_subtype = cfg->parent_subtype;

		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children or invalid
		 */
		if (cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI &&
		    cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
		    cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT)
			continue;

		/* If the element had requested an allocation and that
		 * allocation was a success (full amount) then
		 * allocate the pool.
		 */
		if (req_cnt[i] == resv[j].stride) {
			db[i].alloc.entry.start = resv[j].start;
			db[i].alloc.entry.stride = resv[j].stride;

			/* Only allocate BA pool if a BA type not a child */
			if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
			    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
				/* Create pool */
				pool_size = (BITALLOC_SIZEOF(resv[j].stride) /
					     sizeof(struct bitalloc));
				/* Alloc request, alignment already set */
				cparms.nitems = pool_size;
				cparms.size = sizeof(struct bitalloc);
				rc = tfp_calloc(&cparms);
				if (rc) {
					TFP_DRV_LOG(ERR,
					 "%s: Pool alloc failed, type:%d:%s\n",
					 tf_dir_2_str(parms->dir),
					 cfg->hcapi_type, type_str);
					goto fail;
				}
				db[i].pool = (struct bitalloc *)cparms.mem_va;

				rc = ba_init(db[i].pool,
					     resv[j].stride,
					     !tf_session_is_shared_session(tfs));
				if (rc) {
					TFP_DRV_LOG(ERR,
					  "%s: Pool init failed, type:%d:%s\n",
					  tf_dir_2_str(parms->dir),
					  cfg->hcapi_type, type_str);
					goto fail;
				}
			}
			j++;
		} else {
			/* Bail out as we want what we requested for
			 * all elements, not any less.
			 */
			TFP_DRV_LOG(ERR,
				    "%s: Alloc failed %d:%s req:%d, alloc:%d\n",
				    tf_dir_2_str(parms->dir), cfg->hcapi_type,
				    type_str, req_cnt[i], resv[j].stride);
			goto fail;
		}
	}

	rm_db->num_entries = parms->num_elements;
	rm_db->dir = parms->dir;
	rm_db->module = parms->module;
	*parms->rm_db = (void *)rm_db;

#if (TF_RM_DEBUG == 1)

	printf("%s: module:%s\n",
	       tf_dir_2_str(parms->dir),
	       tf_module_2_str(parms->module));
#endif /* (TF_RM_DEBUG == 1) */

	tfp_free((void *)req);
	tfp_free((void *)resv);
	tfp_free((void *)req_cnt);
	return 0;

 fail:
	tfp_free((void *)req);
	tfp_free((void *)resv);
	tfp_free((void *)db->pool);
	tfp_free((void *)db);
	tfp_free((void *)rm_db);
	tfp_free((void *)req_cnt);
	parms->rm_db = NULL;

	return -EINVAL;
}

int
tf_rm_create_db_no_reservation(struct tf *tfp,
			       struct tf_rm_create_db_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int i, j;
	uint16_t hcapi_items, *req_cnt;
	struct tfp_calloc_parms cparms;
	struct tf_rm_resc_req_entry *req;
	struct tf_rm_resc_entry *resv;
	struct tf_rm_new_db *rm_db;
	struct tf_rm_element *db;
	uint32_t pool_size;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	/* Copy requested counts (alloc_cnt) from tf_open_session() to local
	 * copy (req_cnt) so that it can be updated if required.
	 */

	cparms.nitems = parms->num_elements;
	cparms.size = sizeof(uint16_t);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;

	req_cnt = (uint16_t *)cparms.mem_va;

	tfp_memcpy(req_cnt, parms->alloc_cnt,
		   parms->num_elements * sizeof(uint16_t));

	/* Process capabilities against DB requirements. However, as a
	 * DB can hold elements that are not HCAPI we can reduce the
	 * req msg content by removing those out of the request yet
	 * the DB holds them all as to give a fast lookup. We can also
	 * remove entries where there are no request for elements.
	 */
	tf_rm_count_hcapi_reservations(parms->dir,
				       parms->module,
				       parms->cfg,
				       req_cnt,
				       parms->num_elements,
				       &hcapi_items);

	if (hcapi_items == 0) {
		TFP_DRV_LOG(ERR,
			"%s: module:%s Empty RM DB create request\n",
			tf_dir_2_str(parms->dir),
			tf_module_2_str(parms->module));

		parms->rm_db = NULL;
		return -ENOMEM;
	}

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
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		uint16_t hcapi_type = cfg->hcapi_type;

		/* Only perform reservation for requested entries
		 */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children in the request */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
			req[j].type = hcapi_type;
			req[j].min = req_cnt[i];
			req[j].max = req_cnt[i];
			j++;
		}
	}

	/* Get all resources info for the module type
	 */
	rc = tf_msg_session_resc_info(tfp,
				      dev,
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
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		const char *type_str;

		dev->ops->tf_dev_get_resource_str(tfp,
						  cfg->hcapi_type,
						  &type_str);

		db[i].cfg_type = cfg->cfg_type;
		db[i].hcapi_type = cfg->hcapi_type;

		/* Save the parent subtype for later use to find the pool
		 */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
			db[i].parent_subtype = cfg->parent_subtype;

		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children or invalid
		 */
		if (cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI &&
		    cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
		    cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT)
			continue;

		/* If the element had requested an allocation and that
		 * allocation was a success (full amount) then
		 * allocate the pool.
		 */
		if (req_cnt[i] == resv[j].stride) {
			db[i].alloc.entry.start = resv[j].start;
			db[i].alloc.entry.stride = resv[j].stride;

			/* Only allocate BA pool if a BA type not a child */
			if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
			    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
				/* Create pool */
				pool_size = (BITALLOC_SIZEOF(resv[j].stride) /
					     sizeof(struct bitalloc));
				/* Alloc request, alignment already set */
				cparms.nitems = pool_size;
				cparms.size = sizeof(struct bitalloc);
				rc = tfp_calloc(&cparms);
				if (rc) {
					TFP_DRV_LOG(ERR,
					 "%s: Pool alloc failed, type:%d:%s\n",
					 tf_dir_2_str(parms->dir),
					 cfg->hcapi_type, type_str);
					goto fail;
				}
				db[i].pool = (struct bitalloc *)cparms.mem_va;

				rc = ba_init(db[i].pool,
					     resv[j].stride,
					     !tf_session_is_shared_session(tfs));
				if (rc) {
					TFP_DRV_LOG(ERR,
					  "%s: Pool init failed, type:%d:%s\n",
					  tf_dir_2_str(parms->dir),
					  cfg->hcapi_type, type_str);
					goto fail;
				}
			}
			j++;
		} else {
			/* Bail out as we want what we requested for
			 * all elements, not any less.
			 */
			TFP_DRV_LOG(ERR,
				    "%s: Alloc failed %d:%s req:%d, alloc:%d\n",
				    tf_dir_2_str(parms->dir), cfg->hcapi_type,
				    type_str, req_cnt[i], resv[j].stride);
			goto fail;
		}
	}

	rm_db->num_entries = parms->num_elements;
	rm_db->dir = parms->dir;
	rm_db->module = parms->module;
	*parms->rm_db = (void *)rm_db;

#if (TF_RM_DEBUG == 1)

	printf("%s: module:%s\n",
	       tf_dir_2_str(parms->dir),
	       tf_module_2_str(parms->module));
#endif /* (TF_RM_DEBUG == 1) */

	tfp_free((void *)req);
	tfp_free((void *)resv);
	tfp_free((void *)req_cnt);
	return 0;

 fail:
	tfp_free((void *)req);
	tfp_free((void *)resv);
	tfp_free((void *)db->pool);
	tfp_free((void *)db);
	tfp_free((void *)rm_db);
	tfp_free((void *)req_cnt);
	parms->rm_db = NULL;

	return -EINVAL;
}
int
tf_rm_free_db(struct tf *tfp,
	      struct tf_rm_free_db_parms *parms)
{
	int rc;
	int i;
	uint16_t resv_size = 0;
	struct tf_rm_new_db *rm_db;
	struct tf_rm_resc_entry *resv;
	bool residuals_found = false;

	TF_CHECK_PARMS2(parms, parms->rm_db);

	/* Device unbind happens when the TF Session is closed and the
	 * session ref count is 0. Device unbind will cleanup each of
	 * its support modules, i.e. Identifier, thus we're ending up
	 * here to close the DB.
	 *
	 * On TF Session close it is assumed that the session has already
	 * cleaned up all its resources, individually, while
	 * destroying its flows.
	 *
	 * To assist in the 'cleanup checking' the DB is checked for any
	 * remaining elements and logged if found to be the case.
	 *
	 * Any such elements will need to be 'cleared' ahead of
	 * returning the resources to the HCAPI RM.
	 *
	 * RM will signal FW to flush the DB resources. FW will
	 * perform the invalidation. TF Session close will return the
	 * previous allocated elements to the RM and then close the
	 * HCAPI RM registration. That then saves several 'free' msgs
	 * from being required.
	 */

	rm_db = (struct tf_rm_new_db *)parms->rm_db;

	/* Check for residuals that the client didn't clean up */
	rc = tf_rm_check_residuals(rm_db,
				   &resv_size,
				   &resv,
				   &residuals_found);
	if (rc)
		return rc;

	/* Invalidate any residuals followed by a DB traversal for
	 * pool cleanup.
	 */
	if (residuals_found) {
		rc = tf_msg_session_resc_flush(tfp,
					       parms->dir,
					       resv_size,
					       resv);
		tfp_free((void *)resv);
		/* On failure we still have to cleanup so we can only
		 * log that FW failed.
		 */
		if (rc)
			TFP_DRV_LOG(ERR,
				    "%s: Internal Flush error, module:%s\n",
				    tf_dir_2_str(parms->dir),
				    tf_module_2_str(rm_db->module));
	}

	/* No need to check for configuration type, even if we do not
	 * have a BA pool we just delete on a null ptr, no harm
	 */
	for (i = 0; i < rm_db->num_entries; i++)
		tfp_free((void *)rm_db->db[i].pool);

	tfp_free((void *)parms->rm_db);

	return rc;
}
/**
 * Get the bit allocator pool associated with the subtype and the db
 *
 * [in] rm_db
 *   Pointer to the DB
 *
 * [in] subtype
 *   Module subtype used to index into the module specific database.
 *   An example subtype is TF_TBL_TYPE_FULL_ACT_RECORD which is a
 *   module subtype of TF_MODULE_TYPE_TABLE.
 *
 * [in/out] pool
 *   Pointer to the bit allocator pool used
 *
 * [in/out] new_subtype
 *   Pointer to the subtype of the actual pool used
 * Returns:
 *     0          - Success
 *   - ENOTSUP    - Operation not supported
 */
static int
tf_rm_get_pool(struct tf_rm_new_db *rm_db,
	       uint16_t subtype,
	       struct bitalloc **pool,
	       uint16_t *new_subtype)
{
	int rc = 0;
	uint16_t tmp_subtype = subtype;

	/* If we are a child, get the parent table index */
	if (rm_db->db[subtype].cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		tmp_subtype = rm_db->db[subtype].parent_subtype;

	*pool = rm_db->db[tmp_subtype].pool;

	/* Bail out if the pool is not valid, should never happen */
	if (rm_db->db[tmp_subtype].pool == NULL) {
		rc = -ENOTSUP;
		TFP_DRV_LOG(ERR,
			    "%s: Invalid pool for this type:%d, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    tmp_subtype,
			    strerror(-rc));
		return rc;
	}
	*new_subtype = tmp_subtype;
	return rc;
}

int
tf_rm_allocate(struct tf_rm_allocate_parms *parms)
{
	int rc;
	int id;
	uint32_t index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;
	struct bitalloc *pool;
	uint16_t subtype;

	TF_CHECK_PARMS2(parms, parms->rm_db);

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -ENOTSUP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;
	/*
	 * priority  0: allocate from top of the tcam i.e. high
	 * priority !0: allocate index from bottom i.e lowest
	 */
	if (parms->priority)
		id = ba_alloc_reverse(pool);
	else
		id = ba_alloc(pool);
	if (id == BA_FAIL) {
		rc = -ENOMEM;
		TFP_DRV_LOG(ERR,
			    "%s: Allocation failed, rc:%s\n",
			    tf_dir_2_str(rm_db->dir),
			    strerror(-rc));
		return rc;
	}

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_ADD_BASE,
				subtype,
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
	if (parms->base_index)
		*parms->base_index = id;

	return rc;
}

int
tf_rm_free(struct tf_rm_free_parms *parms)
{
	int rc;
	uint32_t adj_index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;
	struct bitalloc *pool;
	uint16_t subtype;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -ENOTSUP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_RM_BASE,
				subtype,
				parms->index,
				&adj_index);
	if (rc)
		return rc;

	rc = ba_free(pool, adj_index);
	/* No logging direction matters and that is not available here */
	if (rc)
		return rc;

	return rc;
}

int
tf_rm_is_allocated(struct tf_rm_is_allocated_parms *parms)
{
	int rc;
	uint32_t adj_index;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;
	struct bitalloc *pool;
	uint16_t subtype;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -ENOTSUP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_RM_BASE,
				subtype,
				parms->index,
				&adj_index);
	if (rc)
		return rc;

	if (parms->base_index)
		*parms->base_index = adj_index;
	*parms->allocated = ba_inuse(pool, adj_index);

	return rc;
}

int
tf_rm_get_info(struct tf_rm_get_alloc_info_parms *parms)
{
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by HCAPI */
	if (cfg_type == TF_RM_ELEM_CFG_NULL)
		return -ENOTSUP;

	memcpy(parms->info,
	       &rm_db->db[parms->subtype].alloc,
	       sizeof(struct tf_rm_alloc_info));

	return 0;
}

int
tf_rm_get_all_info(struct tf_rm_get_alloc_info_parms *parms, int size)
{
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_alloc_info *info = parms->info;
	int i;

	TF_CHECK_PARMS1(parms);

	/* No rm info available for this module type
	 */
	if (!parms->rm_db)
		return -ENOMEM;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	for (i = 0; i < size; i++) {
		cfg_type = rm_db->db[i].cfg_type;

		/* Bail out if not controlled by HCAPI */
		if (cfg_type == TF_RM_ELEM_CFG_NULL) {
			info++;
			continue;
		}

		memcpy(info,
		       &rm_db->db[i].alloc,
		       sizeof(struct tf_rm_alloc_info));
		info++;
	}

	return 0;
}

int
tf_rm_get_hcapi_type(struct tf_rm_get_hcapi_parms *parms)
{
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by HCAPI */
	if (cfg_type == TF_RM_ELEM_CFG_NULL)
		return -ENOTSUP;

	*parms->hcapi_type = rm_db->db[parms->subtype].hcapi_type;

	return 0;
}
int
tf_rm_get_slices(struct tf_rm_get_slices_parms *parms)
{
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by HCAPI */
	if (cfg_type == TF_RM_ELEM_CFG_NULL)
		return -ENOTSUP;

	*parms->slices = rm_db->db[parms->subtype].slices;

	return 0;
}

int
tf_rm_get_inuse_count(struct tf_rm_get_inuse_count_parms *parms)
{
	int rc = 0;
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not a BA pool */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -ENOTSUP;

	/* Bail silently (no logging), if the pool is not valid there
	 * was no elements allocated for it.
	 */
	if (rm_db->db[parms->subtype].pool == NULL) {
		*parms->count = 0;
		return 0;
	}

	*parms->count = ba_inuse_count(rm_db->db[parms->subtype].pool);

	return rc;
}
/* Only used for table bulk get at this time
 */
int
tf_rm_check_indexes_in_range(struct tf_rm_check_indexes_in_range_parms *parms)
{
	struct tf_rm_new_db *rm_db;
	enum tf_rm_elem_cfg_type cfg_type;
	uint32_t base_index;
	uint32_t stride;
	int rc = 0;
	struct bitalloc *pool;
	uint16_t subtype;

	TF_CHECK_PARMS2(parms, parms->rm_db);
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	TF_CHECK_PARMS1(rm_db->db);

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not a BA pool */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -ENOTSUP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;

	base_index = rm_db->db[subtype].alloc.entry.start;
	stride = rm_db->db[subtype].alloc.entry.stride;

	if (parms->starting_index < base_index ||
	    parms->starting_index + parms->num_entries > base_index + stride)
		return -EINVAL;

	return rc;
}
