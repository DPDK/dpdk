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

struct tf;

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

/** The WC TCAM shared pool declarations
 * TODO: add tcam_shared_wc_db
 */
struct tf_tcam_shared_wc_pool tcam_shared_wc[TF_DIR_MAX][TF_TCAM_SHARED_WC_POOL_MAX];

/** Create a WC TCAM shared pool
 */
static int
tf_tcam_shared_create_wc_pool(int dir,
			      enum tf_tcam_shared_wc_pool_id id,
			      int start,
			      int stride)
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
	tcam_shared_wc[dir][id].pool = (struct bitalloc *)cparms.mem_va;

	rc = ba_init(tcam_shared_wc[dir][id].pool,
		     stride,
		     free);

	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: pool bitalloc failed %s\n",
			    tf_dir_2_str(dir), tf_pool_2_str(id));
		return rc;
	}

	tcam_shared_wc[dir][id].info.start = start;
	tcam_shared_wc[dir][id].info.stride = stride;
	return rc;
}
/** Free a WC TCAM shared pool
 */
static void
tf_tcam_shared_free_wc_pool(int dir,
			    enum tf_tcam_shared_wc_pool_id id)
{
	tcam_shared_wc[dir][id].info.start = 0;
	tcam_shared_wc[dir][id].info.stride = 0;

	if (tcam_shared_wc[dir][id].pool)
		tfp_free((void *)tcam_shared_wc[dir][id].pool);
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
tf_tcam_shared_db_valid(struct tf *tfp,
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

		rc = tf_tcam_shared_get_slices(tfp,
					       dev,
					       &num_slices);
		if (rc)
			return rc;

		/* If there are WC TCAM entries, create 2 pools each with 1/2
		 * the total number of entries
		 */
		for (dir = 0; dir < TF_DIR_MAX; dir++) {
			if (!tf_tcam_shared_db_valid(tfp, dir))
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
						stride);

			start += stride;
			tf_tcam_shared_create_wc_pool(dir,
						TF_TCAM_SHARED_WC_POOL_LO,
						start,
						stride);
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
	struct tf_session *tfs;

	TF_CHECK_PARMS1(tfp);

	/* Perform normal unbind, this will write all the
	 * allocated TCAM entries in the shared session.
	 */
	rc = tf_tcam_unbind(tfp);
	if (rc)
		return rc;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* If we are the shared session
	 */
	if (tf_session_is_shared_session(tfs)) {
		/* If there are WC TCAM entries allocated, free them
		 */
		for (dir = 0; dir < TF_DIR_MAX; dir++) {
			tf_tcam_shared_free_wc_pool(dir,
						    TF_TCAM_SHARED_WC_POOL_HI);
			tf_tcam_shared_free_wc_pool(dir,
						    TF_TCAM_SHARED_WC_POOL_LO);
		}
	}
	return 0;
}
/**
 * tf_tcam_shared_alloc
 */
int
tf_tcam_shared_alloc(struct tf *tfp,
		     struct tf_tcam_alloc_parms *parms)
{
	int rc, i;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	int log_idx;
	struct bitalloc *pool;
	enum tf_tcam_shared_wc_pool_id id;
	uint16_t num_slices;

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

	if (!tf_tcam_shared_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_tcam_shared_get_slices(tfp, dev, &num_slices);
	if (rc)
		return rc;

	pool = tcam_shared_wc[parms->dir][id].pool;

	for (i = 0; i < num_slices; i++) {
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
		/* return the index without the start of each row */
		if (i == 0)
			parms->idx = log_idx;
	}
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
	int i;
	uint16_t start;
	int phy_idx;
	struct bitalloc *pool;
	enum tf_tcam_shared_wc_pool_id id;
	struct tf_tcam_free_parms nparms;
	uint16_t num_slices;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;

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

	if (!tf_tcam_shared_db_valid(tfp, parms->dir)) {
		TFP_DRV_LOG(ERR,
			    "%s: tcam shared pool doesn't exist\n",
			    tf_dir_2_str(parms->dir));
		return -ENOMEM;
	}

	if (parms->type == TF_TCAM_TBL_TYPE_WC_TCAM_HIGH)
		id = TF_TCAM_SHARED_WC_POOL_HI;
	else
		id = TF_TCAM_SHARED_WC_POOL_LO;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_tcam_shared_get_slices(tfp, dev, &num_slices);
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

	pool = tcam_shared_wc[parms->dir][id].pool;
	start = tcam_shared_wc[parms->dir][id].info.start;

	if (parms->idx % num_slices) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM reserved resource is not multiple of %d\n",
			    tf_dir_2_str(parms->dir), num_slices);
		return -EINVAL;
	}

	phy_idx = parms->idx + start;
	allocated = ba_inuse(pool, parms->idx);

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry already free, type:%d, idx:%d\n",
			    tf_dir_2_str(parms->dir), parms->type, parms->idx);
		return -EINVAL;
	}

	for (i = 0; i < num_slices; i++) {
		rc = ba_free(pool, parms->idx + i);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: Free failed, type:%s, idx:%d\n",
				    tf_dir_2_str(parms->dir),
				    tf_tcam_tbl_2_str(parms->type),
				    parms->idx);
			return rc;
		}
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
	uint16_t num_slices;
	struct tf_tcam_set_parms nparms;
	struct bitalloc *pool;
	uint16_t start;
	enum tf_tcam_shared_wc_pool_id id;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;

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

	if (!tf_tcam_shared_db_valid(tfp, parms->dir)) {
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

	pool = tcam_shared_wc[parms->dir][id].pool;
	start = tcam_shared_wc[parms->dir][id].info.start;

	log_idx = parms->idx;
	phy_idx = parms->idx + start;
	allocated = ba_inuse(pool, parms->idx);

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry is not allocated, type:%d, logid:%d\n",
			    tf_dir_2_str(parms->dir), parms->type, log_idx);
		return -EINVAL;
	}
	rc = tf_tcam_shared_get_slices(tfp, dev, &num_slices);
	if (rc)
		return rc;

	if (parms->idx % num_slices) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM reserved resource is not multiple of %d\n",
			    tf_dir_2_str(parms->dir), num_slices);
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
	uint16_t num_slices;
	struct tf_tcam_get_parms nparms;
	struct bitalloc *pool;
	uint16_t start;
	enum tf_tcam_shared_wc_pool_id id;
	uint16_t hcapi_type;
	struct tf_rm_alloc_info info;

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

	if (!tf_tcam_shared_db_valid(tfp, parms->dir)) {
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

	pool = tcam_shared_wc[parms->dir][id].pool;
	start = tcam_shared_wc[parms->dir][id].info.start;

	rc = tf_tcam_shared_get_slices(tfp, dev, &num_slices);
	if (rc)
		return rc;

	if (parms->idx % num_slices) {
		TFP_DRV_LOG(ERR,
			    "%s: TCAM reserved resource is not multiple of %d\n",
			    tf_dir_2_str(parms->dir), num_slices);
		return -EINVAL;
	}
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
