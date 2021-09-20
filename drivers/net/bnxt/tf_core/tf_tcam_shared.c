/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <rte_common.h>

#include "tf_tcam_shared.h"
#include "tf_tcam.h"
#include "tf_common.h"
#include "tf_util.h"
#include "tf_rm.h"
#include "tf_device.h"
#include "tfp.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "bitalloc.h"
#include "tf_core.h"

/** Shared WC TCAM pool identifiers
 */
enum tf_tcam_shared_wc_pool_id {
	TF_TCAM_SHARED_WC_POOL_HI  = 0,
	TF_TCAM_SHARED_WC_POOL_LO  = 1,
	TF_TCAM_SHARED_WC_POOL_MAX = 2
};

/** Get string representation of a WC TCAM shared pool id
 */
static const char *
tf_pool_2_str(enum tf_tcam_shared_wc_pool_id id)
{
	switch (id) {
	case TF_TCAM_SHARED_WC_POOL_HI:
		return "TCAM_SHARED_WC_POOL_HI";
	case TF_TCAM_SHARED_WC_POOL_LO:
		return "TCAM_SHARED_WC_POOL_LO";
	default:
		return "Invalid TCAM_SHARED_WC_POOL";
	}
}

/** The WC TCAM shared pool datastructure
 */
struct tf_tcam_shared_wc_pool {
	/** Start and stride data */
	struct tf_resource_info info;
	/** bitalloc pool */
	struct bitalloc *pool;
};

struct tf_tcam_shared_wc_pools {
	struct tf_tcam_shared_wc_pool db[TF_DIR_MAX][TF_TCAM_SHARED_WC_POOL_MAX];
};

/** The WC TCAM shared pool declarations
 */
/* struct tf_tcam_shared_wc_pool tcam_shared_wc[TF_DIR_MAX][TF_TCAM_SHARED_WC_POOL_MAX]; */

static int
tf_tcam_shared_create_db(struct tf_tcam_shared_wc_pools **db)
{
	struct tfp_calloc_parms cparms;
	int rc = 0;

	cparms.nitems = 1;
	cparms.alignment = 0;
	cparms.size = sizeof(struct tf_tcam_shared_wc_pools);
	rc = tfp_calloc(&cparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "TCAM shared db allocation failed (%s)\n",
			    strerror(-rc));
		return rc;
	}
	*db = cparms.mem_va;

	return rc;
}

/** Create a WC TCAM shared pool
 */
static int
tf_tcam_shared_create_wc_pool(int dir,
			      enum tf_tcam_shared_wc_pool_id id,
			      int start,
			      int stride,
			      struct tf_tcam_shared_wc_pools *tcam_shared_wc)
{
	int rc = 0;
	bool free = true;
	struct tfp_calloc_parms cparms;
	uint32_t pool_size;

	/* Create pool */
	pool_size = (BITALLOC_SIZEOF(stride) / sizeof(struct bitalloc));
	cparms.nitems = pool_size;
	cparms.alignment = 0;
	cparms.size = sizeof(struct bitalloc);
	rc = tfp_calloc(&cparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: pool memory alloc failed %s:%s\n",
			    tf_dir_2_str(dir), tf_pool_2_str(id),
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc->db[dir][id].pool = (struct bitalloc *)cparms.mem_va;

	rc = ba_init(tcam_shared_wc->db[dir][id].pool,
		     stride,
		     free);

	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: pool bitalloc failed %s\n",
			    tf_dir_2_str(dir), tf_pool_2_str(id));
		return rc;
	}

	tcam_shared_wc->db[dir][id].info.start = start;
	tcam_shared_wc->db[dir][id].info.stride = stride;

	return rc;
}
/** Free a WC TCAM shared pool
 */
static int
tf_tcam_shared_free_wc_pool(int dir,
			    enum tf_tcam_shared_wc_pool_id id,
			    struct tf_tcam_shared_wc_pools *tcam_shared_wc)
{
	int rc = 0;
	TF_CHECK_PARMS1(tcam_shared_wc);

	tcam_shared_wc->db[dir][id].info.start = 0;
	tcam_shared_wc->db[dir][id].info.stride = 0;

	if (tcam_shared_wc->db[dir][id].pool)
		tfp_free((void *)tcam_shared_wc->db[dir][id].pool);
	return rc;
}

/** Get the number of WC TCAM slices allocated during 1 allocation/free
 */
static int
tf_tcam_shared_get_slices(struct tf *tfp,
			  struct tf_dev_info *dev,
			  uint16_t *num_slices)
{
	int rc;

	if (dev->ops->tf_dev_get_tcam_slice_info == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "Operation not supported, rc:%s\n", strerror(-rc));
		return rc;
	}
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  TF_TCAM_TBL_TYPE_WC_TCAM,
						  0,
						  num_slices);
	return rc;
}

static bool
tf_tcam_db_valid(struct tf *tfp,
			enum tf_dir dir)
{
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	int rc;

	TF_CHECK_PARMS1(tfp);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc)
		return false;

	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	if (tcam_db->tcam_db[dir])
		return true;

	return false;
}

static int
tf_tcam_shared_get_rm_info(struct tf *tfp,
			   enum tf_dir dir,
			   uint16_t *hcapi_type,
			   struct tf_rm_alloc_info *info)
{
	int rc;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_rm_get_hcapi_parms hparms;

	TF_CHECK_PARMS3(tfp, hcapi_type, info);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		TFP_DRV_LOG(INFO,
			    "Tcam_db is not initialized, rc:%s\n",
			    strerror(-rc));
		return 0;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Convert TF type to HCAPI RM type */
	memset(&hparms, 0, sizeof(hparms));
	hparms.rm_db = tcam_db->tcam_db[dir];
	hparms.subtype = TF_TCAM_TBL_TYPE_WC_TCAM;
	hparms.hcapi_type = hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Get RM hcapi type failed %s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}

	memset(info, 0, sizeof(struct tf_rm_alloc_info));
	ainfo.rm_db = tcam_db->tcam_db[dir];
	ainfo.subtype = TF_TCAM_TBL_TYPE_WC_TCAM;
	ainfo.info = info;

	rc = tf_rm_get_info(&ainfo);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM rm info get failed %s\n",
			    tf_dir_2_str(dir),
			    strerror(-rc));
		return rc;
	}
	return rc;
}

/**
 * tf_tcam_shared_bind
 */
int
tf_tcam_shared_bind(struct tf *tfp,
		    struct tf_tcam_cfg_parms *parms)
{
	int rc, dir;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_rm_alloc_info info;
	uint16_t start, stride;
	uint16_t num_slices;
	uint16_t hcapi_type;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Perform normal bind
	 */
	rc = tf_tcam_bind(tfp, parms);
	if (rc)
		return rc;

	/* After the normal TCAM bind, if this is a shared session
	 * create all required databases for the WC_HI and WC_LO pools
	 */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Session access failure: %s\n", strerror(-rc));
		return rc;
	}
	if (tf_session_is_shared_session(tfs)) {
		/* Retrieve the device information */
		rc = tf_session_get_device(tfs, &dev);
		if (rc)
			return rc;

		tf_tcam_shared_create_db(&tcam_shared_wc);


		/* If there are WC TCAM entries, create 2 pools each with 1/2
		 * the total number of entries
		 */
		for (dir = 0; dir < TF_DIR_MAX; dir++) {
			if (!tf_tcam_db_valid(tfp, dir))
				continue;

			rc = tf_tcam_shared_get_rm_info(tfp,
							dir,
							&hcapi_type,
							&info);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "%s: TCAM rm info get failed\n",
					    tf_dir_2_str(dir));
				goto done;
			}

			start = info.entry.start;
			stride = info.entry.stride / 2;

			tf_tcam_shared_create_wc_pool(dir,
						      TF_TCAM_SHARED_WC_POOL_HI,
						      start,
						      stride,
						      tcam_shared_wc);

			start += stride;
			tf_tcam_shared_create_wc_pool(dir,
						      TF_TCAM_SHARED_WC_POOL_LO,
						      start,
						      stride,
						      tcam_shared_wc);

			tf_session_set_tcam_shared_db(tfp, (void *)tcam_shared_wc);
		}

		rc = tf_tcam_shared_get_slices(tfp,
					       dev,
					       &num_slices);
		if (rc)
			return rc;

		if (num_slices > 1) {
			TFP_DRV_LOG(ERR,
				    "Only single slice supported\n");
			return -EOPNOTSUPP;
		}
	}
done:
	return rc;
}
/**
 * tf_tcam_shared_unbind
 */
int
tf_tcam_shared_unbind(struct tf *tfp)
{
	int rc, dir;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	void *tcam_shared_db_ptr = NULL;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;
	enum tf_tcam_shared_wc_pool_id pool_id;
	struct tf_tcam_free_parms parms;
	struct bitalloc *pool;
	uint16_t start;
	int log_idx, phy_idx;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;
	int i, pool_cnt;

	TF_CHECK_PARMS1(tfp);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If not the shared session, call the normal
	 * tcam unbind and exit
	 */
	if (!tf_session_is_shared_session(tfs)) {
		rc = tf_tcam_unbind(tfp);
		return rc;
	}

	/* We must be a shared session, get the database
	 */
	rc = tf_session_get_tcam_shared_db(tfp,
					   (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	tcam_shared_wc =
		(struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;


	/* Get the device
	 */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;


	/* If there are WC TCAM entries allocated, free them
	 */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/* If the database is invalid, skip
		 */
		if (!tf_tcam_db_valid(tfp, dir))
			continue;

		rc = tf_tcam_shared_get_rm_info(tfp,
						dir,
						&hcapi_type,
						&info);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: TCAM shared rm info get failed\n",
				    tf_dir_2_str(dir));
			return rc;
		}

		for (pool_id = TF_TCAM_SHARED_WC_POOL_HI;
		     pool_id < TF_TCAM_SHARED_WC_POOL_MAX;
		     pool_id++) {
			pool = tcam_shared_wc->db[dir][pool_id].pool;
			start = tcam_shared_wc->db[dir][pool_id].info.start;
			pool_cnt = ba_inuse_count(pool);

			if (pool_cnt) {
				TFP_DRV_LOG(INFO,
					    "%s: %s: %d residuals found, freeing\n",
					    tf_dir_2_str(dir),
					    tf_pool_2_str(pool_id),
					    pool_cnt);
			}

			log_idx = 0;

			for (i = 0; i < pool_cnt; i++) {
				log_idx = ba_find_next_inuse(pool, log_idx);

				if (log_idx < 0) {
					TFP_DRV_LOG(ERR,
						    "Expected a found %s entry %d\n",
						    tf_pool_2_str(pool_id),
						    i);
					/* attempt normal unbind
					 */
					goto done;
				}
				phy_idx = start + log_idx;

				parms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
				parms.hcapi_type = hcapi_type;
				parms.idx = phy_idx;
				parms.dir = dir;
				rc = tf_msg_tcam_entry_free(tfp, dev, &parms);
				if (rc) {
					/* Log error */
					TFP_DRV_LOG(ERR,
						    "%s: %s: %d free failed, rc:%s\n",
						    tf_dir_2_str(parms.dir),
						    tf_tcam_tbl_2_str(parms.type),
						    phy_idx,
						    strerror(-rc));
					return rc;
				}
			}
			/* Free the pool once all the entries
			 * have been cleared
			 */
			tf_tcam_shared_free_wc_pool(dir,
						    pool_id,
						    tcam_shared_wc);
		}
	}
done:
	rc = tf_tcam_unbind(tfp);
	return rc;
}

/**
 * tf_tcam_shared_alloc
 */
int
tf_tcam_shared_alloc(struct tf *tfp,
		     struct tf_tcam_alloc_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int log_idx;
	struct bitalloc *pool;
	enum tf_tcam_shared_wc_pool_id id;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;
	void *tcam_shared_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we aren't the shared session or the type is
	 * not one of the special WC TCAM types, call the normal
	 * allocation.
	 */
	if (!tf_session_is_shared_session(tfs) ||
	    (parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW)) {
		/* Perform normal alloc
		 */
		rc = tf_tcam_alloc(tfp, parms);
		return rc;
	}

	if (!tf_tcam_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	rc = tf_session_get_tcam_shared_db(tfp, (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc = (struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;

	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	pool = tcam_shared_wc->db[parms->dir][id].pool;

	/*
	 * priority  0: allocate from top of the tcam i.e. high
	 * priority !0: allocate index from bottom i.e lowest
	 */
	if (parms->priority)
		log_idx = ba_alloc_reverse(pool);
	else
		log_idx = ba_alloc(pool);
	if (log_idx == BA_FAIL) {
		TFP_DRV_LOG(ERR,
			    "%s: Allocation failed, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    strerror(ENOMEM));
		return -ENOMEM;
	}
	parms->idx = log_idx;
	return 0;
}

int
tf_tcam_shared_free(struct tf *tfp,
		    struct tf_tcam_free_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int allocated = 0;
	uint16_t start;
	int phy_idx;
	struct bitalloc *pool;
	enum tf_tcam_shared_wc_pool_id id;
	struct tf_tcam_free_parms nparms;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;
	void *tcam_shared_db_ptr = NULL;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we aren't the shared session or the type is
	 * not one of the special WC TCAM types, call the normal
	 * allocation.
	 */
	if (!tf_session_is_shared_session(tfs) ||
	    (parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW)) {
		/* Perform normal free
		 */
		rc = tf_tcam_free(tfp, parms);
		return rc;
	}

	if (!tf_tcam_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	rc = tf_session_get_tcam_shared_db(tfp, (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc = (struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;


	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_tcam_shared_get_rm_info(tfp,
					parms->dir,
					&hcapi_type,
					&info);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM rm info get failed\n",
			    tf_dir_2_str(parms->dir));
		return rc;
	}

	pool = tcam_shared_wc->db[parms->dir][id].pool;
	start = tcam_shared_wc->db[parms->dir][id].info.start;

	phy_idx = parms->idx + start;
	allocated = ba_inuse(pool, parms->idx);

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry already free, type:%d, idx:%d\n",
			    tf_dir_2_str(parms->dir), parms->type, parms->idx);
		return -EINVAL;
	}

	rc = ba_free(pool, parms->idx);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Free failed, type:%s, idx:%d\n",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->type),
			    parms->idx);
		return rc;
	}

	/* Override HI/LO type with parent WC TCAM type */
	nparms = *parms;
	nparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	nparms.hcapi_type = hcapi_type;
	nparms.idx = phy_idx;

	rc = tf_msg_tcam_entry_free(tfp, dev, &nparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: log%d free failed, rc:%s\n",
			    tf_dir_2_str(nparms.dir),
			    tf_tcam_tbl_2_str(nparms.type),
			    phy_idx,
			    strerror(-rc));
		return rc;
	}
	return 0;
}

int
tf_tcam_shared_set(struct tf *tfp __rte_unused,
		   struct tf_tcam_set_parms *parms __rte_unused)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int allocated = 0;
	int phy_idx, log_idx;
	struct tf_tcam_set_parms nparms;
	struct bitalloc *pool;
	uint16_t start;
	enum tf_tcam_shared_wc_pool_id id;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;
	void *tcam_shared_db_ptr = NULL;


	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we aren't the shared session or one of our
	 * special types
	 */
	if (!tf_session_is_shared_session(tfs) ||
	    (parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW)) {
		/* Perform normal set and exit
		 */
		rc = tf_tcam_set(tfp, parms);
		return rc;
	}

	if (!tf_tcam_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;

	rc = tf_session_get_tcam_shared_db(tfp, (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc = (struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;

	pool = tcam_shared_wc->db[parms->dir][id].pool;
	start = tcam_shared_wc->db[parms->dir][id].info.start;

	log_idx = parms->idx;
	phy_idx = parms->idx + start;
	allocated = ba_inuse(pool, parms->idx);

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry is not allocated, type:%d, logid:%d\n",
			    tf_dir_2_str(parms->dir), parms->type, log_idx);
		return -EINVAL;
	}

	rc = tf_tcam_shared_get_rm_info(tfp,
					parms->dir,
					&hcapi_type,
					&info);
	if (rc)
		return rc;

	/* Override HI/LO type with parent WC TCAM type */
	nparms.hcapi_type = hcapi_type;
	nparms.dir = parms->dir;
	nparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	nparms.idx = phy_idx;
	nparms.key = parms->key;
	nparms.mask = parms->mask;
	nparms.key_size = parms->key_size;
	nparms.result = parms->result;
	nparms.result_size = parms->result_size;

	rc = tf_msg_tcam_entry_set(tfp, dev, &nparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: phy entry %d set failed, rc:%s",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(nparms.type),
			    phy_idx,
			    strerror(-rc));
		return rc;
	}
	return 0;
}

int
tf_tcam_shared_get(struct tf *tfp __rte_unused,
		   struct tf_tcam_get_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int allocated = 0;
	int phy_idx, log_idx;
	struct tf_tcam_get_parms nparms;
	struct bitalloc *pool;
	uint16_t start;
	enum tf_tcam_shared_wc_pool_id id;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;
	void *tcam_shared_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we aren't the shared session or one of our
	 * special types
	 */
	if (!tf_session_is_shared_session(tfs) ||
	    (parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW)) {
		/* Perform normal get and exit
		 */
		rc = tf_tcam_get(tfp, parms);
		return rc;
	}

	if (!tf_tcam_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;
	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;


	rc = tf_session_get_tcam_shared_db(tfp, (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc = (struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;

	pool = tcam_shared_wc->db[parms->dir][id].pool;
	start = tcam_shared_wc->db[parms->dir][id].info.start;

	log_idx = parms->idx;
	phy_idx = parms->idx + start;
	allocated = ba_inuse(pool, parms->idx);

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry is not allocated, type:%d, logid:%d\n",
			    tf_dir_2_str(parms->dir), parms->type, log_idx);
		return -EINVAL;
	}

	rc = tf_tcam_shared_get_rm_info(tfp,
					parms->dir,
					&hcapi_type,
					&info);
	if (rc)
		return rc;

	/* Override HI/LO type with parent WC TCAM type */
	nparms = *parms;
	nparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	nparms.hcapi_type = hcapi_type;
	nparms.idx = phy_idx;

	rc = tf_msg_tcam_entry_get(tfp, dev, &nparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: Entry %d set failed, rc:%s",
			    tf_dir_2_str(nparms.dir),
			    tf_tcam_tbl_2_str(nparms.type),
			    nparms.idx,
			    strerror(-rc));
		return rc;
	}
	return 0;
}

/* Normally, device specific code wouldn't reside here, it belongs
 * in a separate device specific function in tf_device_pxx.c.
 * But this code is placed here as it is not a long term solution
 * and we would like to have this code centrally located for easy
 * removal
 */
#define TF_TCAM_SHARED_KEY_SLICE_SZ_BYTES_P4 12
#define TF_TCAM_SHARED_REMAP_SZ_BYTES_P4 4
#define TF_TCAM_SHARED_KEY_SLICE_SZ_BYTES_P58 24
#define TF_TCAM_SHARED_REMAP_SZ_BYTES_P58 8

/* Temporary builder defines pulled in here and adjusted
 * for max WC TCAM values
 */
union tf_tmp_field_obj {
	uint32_t words[(TF_TCAM_SHARED_REMAP_SZ_BYTES_P58 + 3) / 4];
	uint8_t bytes[TF_TCAM_SHARED_REMAP_SZ_BYTES_P58];
};

union tf_tmp_key {
	uint32_t words[(TF_TCAM_SHARED_KEY_SLICE_SZ_BYTES_P58 + 3) / 4];
	uint8_t bytes[TF_TCAM_SHARED_KEY_SLICE_SZ_BYTES_P58];
};

/** p58 has an enable bit, p4 does not
 */
#define TF_TCAM_SHARED_ENTRY_ENABLE 0x8

/** Move a WC TCAM entry from the high offset to the same low offset
 */
static int
tf_tcam_shared_move_entry(struct tf *tfp,
			  struct tf_dev_info *dev,
			  uint16_t hcapi_type,
			  enum tf_dir dir,
			  int sphy_idx,
			  int dphy_idx,
			  int key_sz_bytes,
			  int remap_sz_bytes,
			  bool set_enable_bit)
{
	int rc = 0;
	struct tf_tcam_get_parms gparms;
	struct tf_tcam_set_parms sparms;
	struct tf_tcam_free_parms fparms;
	union tf_tmp_key tcam_key_obj;
	union tf_tmp_key tcam_key_msk_obj;
	union tf_tmp_field_obj tcam_remap_obj;

	memset(&tcam_key_obj, 0, sizeof(tcam_key_obj));
	memset(&tcam_key_msk_obj, 0, sizeof(tcam_key_msk_obj));
	memset(&tcam_remap_obj, 0, sizeof(tcam_remap_obj));
	memset(&gparms, 0, sizeof(gparms));

	gparms.hcapi_type = hcapi_type;
	gparms.dir = dir;
	gparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	gparms.idx = sphy_idx;
	gparms.key = (uint8_t *)&tcam_key_obj;
	gparms.key_size = key_sz_bytes;
	gparms.mask = (uint8_t *)&tcam_key_msk_obj;
	gparms.result = (uint8_t *)&tcam_remap_obj;
	gparms.result_size = remap_sz_bytes;

	rc = tf_msg_tcam_entry_get(tfp, dev, &gparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: phyid(%d) get failed, rc:%s\n",
			    tf_tcam_tbl_2_str(gparms.type),
			    tf_dir_2_str(dir),
			    gparms.idx,
			    strerror(-rc));
		return rc;
	}

	if (set_enable_bit)
		tcam_key_obj.bytes[0] |= TF_TCAM_SHARED_ENTRY_ENABLE;

	/* Override HI/LO type with parent WC TCAM type */
	sparms.hcapi_type = hcapi_type;
	sparms.dir = dir;
	sparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	sparms.idx = dphy_idx;
	sparms.key = gparms.key;
	sparms.mask = gparms.mask;
	sparms.key_size = key_sz_bytes;
	sparms.result = gparms.result;
	sparms.result_size = remap_sz_bytes;

	rc = tf_msg_tcam_entry_set(tfp, dev, &sparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s phyid(%d/0x%x) set failed, rc:%s\n",
			    tf_tcam_tbl_2_str(sparms.type),
			    tf_dir_2_str(dir),
			    sparms.idx,
			    sparms.idx,
			    strerror(-rc));
		return rc;
	}

	/* Override HI/LO type with parent WC TCAM type */
	fparms.dir = dir;
	fparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	fparms.hcapi_type = hcapi_type;
	fparms.idx = sphy_idx;

	rc = tf_msg_tcam_entry_free(tfp, dev, &fparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "%s: %s: phyid(%d/0x%x) free failed, rc:%s\n",
			    tf_dir_2_str(dir),
			    tf_tcam_tbl_2_str(fparms.type),
			    sphy_idx,
			    sphy_idx,
			    strerror(-rc));
		return rc;
	}
	return rc;
}

/** Move all shared WC TCAM entries from the high pool into the low pool
 *  and clear out the high pool entries.
 */
static
int tf_tcam_shared_move(struct tf *tfp,
			struct tf_move_tcam_shared_entries_parms *parms,
			int key_sz_bytes,
			int remap_sz_bytes,
			bool set_enable_bit)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int log_idx;
	struct bitalloc *hi_pool, *lo_pool;
	uint16_t hi_start, lo_start;
	enum tf_tcam_shared_wc_pool_id hi_id, lo_id;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;
	int hi_cnt, i;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;
	void *tcam_shared_db_ptr = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we aren't the shared session or one of our
	 * special types
	 */
	if (!tf_session_is_shared_session(tfs) ||
	    (parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW)) {
		TFP_DRV_LOG(ERR,
			    "%s: Session must be shared with HI/LO type\n",
			    tf_dir_2_str(parms->dir));
		return -EOPNOTSUPP;
	}

	if (!tf_tcam_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		/* TODO print amazing error */
		return rc;
	}

	rc = tf_tcam_shared_get_rm_info(tfp,
					parms->dir,
					&hcapi_type,
					&info);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM rm info get failed\n",
			    tf_dir_2_str(parms->dir));
		return rc;
	}

	rc = tf_session_get_tcam_shared_db(tfp, (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc = (struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;

	hi_id = TF_TCAM_SHARED_WC_POOL_HI;
	hi_pool = tcam_shared_wc->db[parms->dir][hi_id].pool;
	hi_start = tcam_shared_wc->db[parms->dir][hi_id].info.start;

	lo_id = TF_TCAM_SHARED_WC_POOL_LO;
	lo_pool = tcam_shared_wc->db[parms->dir][lo_id].pool;
	lo_start = tcam_shared_wc->db[parms->dir][lo_id].info.start;

	if (hi_pool == NULL || lo_pool == NULL)
		return -ENOMEM;

	/* Get the total count of in use entries in the high pool
	 */
	hi_cnt = ba_inuse_count(hi_pool);

	/* Copy each valid entry to the same low pool logical offset
	 */
	log_idx = 0;

	for (i = 0; i < hi_cnt; i++) {
		/* Find next free index starting from where we left off
		 */
		log_idx = ba_find_next_inuse(hi_pool, log_idx);
		if (log_idx < 0) {
			TFP_DRV_LOG(ERR,
				    "Expected a found %s entry %d\n",
				    tf_pool_2_str(hi_id),
				    i);
			goto done;
		}
		/* The user should have never allocated from the low
		 * pool because the move only happens when switching
		 * from the high to the low pool
		 */
		if (ba_alloc_index(lo_pool, log_idx) < 0) {
			TFP_DRV_LOG(ERR,
				    "Warning %s index %d already allocated\n",
				    tf_pool_2_str(lo_id),
				    i);

			/* Since already allocated, continue with move
			 */
		}

		rc = tf_tcam_shared_move_entry(tfp, dev,
					       hcapi_type,
					       parms->dir,
					       hi_start + log_idx,
					       lo_start + log_idx,
					       key_sz_bytes,
					       remap_sz_bytes,
					       set_enable_bit);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Move error %s to %s index %d\n",
				    tf_dir_2_str(parms->dir),
				    tf_pool_2_str(hi_id),
				    tf_pool_2_str(lo_id),
				    i);
			goto done;
		}
		ba_free(hi_pool, log_idx);
	}
done:
	return rc;
}

int
tf_tcam_shared_move_p4(struct tf *tfp,
		       struct tf_move_tcam_shared_entries_parms *parms)
{
	int rc = 0;
	rc = tf_tcam_shared_move(tfp,
				 parms,
				 TF_TCAM_SHARED_KEY_SLICE_SZ_BYTES_P4,
				 TF_TCAM_SHARED_REMAP_SZ_BYTES_P4,
				 false); /* no enable bit */
	return rc;
}


int
tf_tcam_shared_move_p58(struct tf *tfp,
			struct tf_move_tcam_shared_entries_parms *parms)
{
	int rc = 0;
	rc = tf_tcam_shared_move(tfp,
				 parms,
				 TF_TCAM_SHARED_KEY_SLICE_SZ_BYTES_P58,
				 TF_TCAM_SHARED_REMAP_SZ_BYTES_P58,
				 true); /* set enable bit */
	return rc;
}

int
tf_tcam_shared_clear(struct tf *tfp,
		     struct tf_clear_tcam_shared_entries_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	uint16_t start;
	int phy_idx;
	enum tf_tcam_shared_wc_pool_id id;
	struct tf_tcam_free_parms nparms;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;
	void *tcam_shared_db_ptr = NULL;
	struct tf_tcam_shared_wc_pools *tcam_shared_wc;
	int i, cnt;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	if (!tf_session_is_shared_session(tfs) ||
	    (parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_HIGH &&
	     parms->tcam_tbl_type != TF_TCAM_TBL_TYPE_WC_TCAM_LOW))
		return -EOPNOTSUPP;

	if (!tf_tcam_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	rc = tf_session_get_tcam_shared_db(tfp, (void *)&tcam_shared_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tcam_shared_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tcam_shared_wc = (struct tf_tcam_shared_wc_pools *)tcam_shared_db_ptr;


	if (parms->tcam_tbl_type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;


	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_tcam_shared_get_rm_info(tfp,
					parms->dir,
					&hcapi_type,
					&info);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM rm info get failed\n",
			    tf_dir_2_str(parms->dir));
		return rc;
	}

	start = tcam_shared_wc->db[parms->dir][id].info.start;
	cnt = tcam_shared_wc->db[parms->dir][id].info.stride;

	/* Override HI/LO type with parent WC TCAM type */
	nparms.dir = parms->dir;
	nparms.type = TF_TCAM_TBL_TYPE_WC_TCAM;
	nparms.hcapi_type = hcapi_type;

	for (i = 0; i < cnt; i++) {
		phy_idx = start + i;
		nparms.idx = phy_idx;

		/* Clear entry */
		rc = tf_msg_tcam_entry_free(tfp, dev, &nparms);
		if (rc) {
			/* Log error */
			TFP_DRV_LOG(ERR,
				    "%s: %s: log%d free failed, rc:%s\n",
				    tf_dir_2_str(nparms.dir),
				    tf_tcam_tbl_2_str(nparms.type),
				    phy_idx,
				    strerror(-rc));
			return rc;
		}
	}

	TFP_DRV_LOG(DEBUG,
		    "%s: TCAM shared clear pool(%s)\n",
		    tf_dir_2_str(nparms.dir),
		    tf_pool_2_str(id));
	return 0;
}
