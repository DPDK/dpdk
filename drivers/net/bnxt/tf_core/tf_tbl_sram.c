/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <rte_common.h>

#include "tf_tbl.h"
#include "tf_tbl_sram.h"
#include "tf_sram_mgr.h"
#include "tf_common.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tfp.h"
#include "tf_session.h"
#include "tf_device.h"
#include "cfa_resource_types.h"

#define DBG_SRAM 0

#define TF_TBL_PTR_TO_RM(new_idx, idx, base, shift) {		\
		*(new_idx) = (((idx) >> (shift)) - (base));	\
}

/**
 * tf_sram_tbl_get_info_parms parameter definition
 */
struct tf_tbl_sram_get_info_parms {
	/**
	 * [in] table RM database
	 */
	void *rm_db;
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] table_type
	 *
	 *  the TF index table type
	 */
	enum tf_tbl_type tbl_type;
	/**
	 * [out] bank
	 *
	 *  The SRAM bank associated with the type
	 */
	enum tf_sram_bank_id bank_id;
	/**
	 * [out] slice_size
	 *
	 *  the slice size for the indicated table type
	 */
	enum tf_sram_slice_size slice_size;
};

/**
 * Translate HCAPI type to SRAM Manager bank
 */
const uint16_t tf_tbl_sram_hcapi_2_bank[CFA_RESOURCE_TYPE_P58_LAST] = {
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_0] = TF_SRAM_BANK_ID_0,
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_1] = TF_SRAM_BANK_ID_1,
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_2] = TF_SRAM_BANK_ID_2,
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_3] = TF_SRAM_BANK_ID_3
};

#define TF_TBL_SRAM_SLICES_MAX  \
	(TF_SRAM_MGR_BLOCK_SZ_BYTES / TF_SRAM_MGR_MIN_SLICE_BYTES)
/**
 * Translate HCAPI type to SRAM Manager bank
 */
const uint8_t tf_tbl_sram_slices_2_size[TF_TBL_SRAM_SLICES_MAX + 1] = {
	[0] = TF_SRAM_SLICE_SIZE_128B,	/* if 0 slices assume 1 128B block */
	[1] = TF_SRAM_SLICE_SIZE_128B,	/* 1 slice  per 128B block */
	[2] = TF_SRAM_SLICE_SIZE_64B,	/* 2 slice  per 128B block */
	[4] = TF_SRAM_SLICE_SIZE_32B,	/* 4 slices per 128B block */
	[8] = TF_SRAM_SLICE_SIZE_16B,	/* 8 slices per 128B block */
	[16] = TF_SRAM_SLICE_SIZE_8B	/* 16 slices per 128B block */
};

/**
 * Get SRAM Table Information for a given index table type
 *
 *
 * [in] sram_handle
 *   Pointer to SRAM handle
 *
 * [in] parms
 *   Pointer to the SRAM get info parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
static int tf_tbl_sram_get_info(struct tf_tbl_sram_get_info_parms *parms)
{
	int rc = 0;
	uint16_t hcapi_type;
	uint16_t slices;
	struct tf_rm_get_hcapi_parms hparms;
	struct tf_rm_get_slices_parms sparms;

	hparms.rm_db = parms->rm_db;
	hparms.subtype = parms->tbl_type;
	hparms.hcapi_type = &hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get hcapi_type %s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->tbl_type),
			    strerror(-rc));
		return rc;
	}
	parms->bank_id = tf_tbl_sram_hcapi_2_bank[hcapi_type];

	sparms.rm_db = parms->rm_db;
	sparms.subtype = parms->tbl_type;
	sparms.slices = &slices;

	rc = tf_rm_get_slices(&sparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get slice cnt %s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->tbl_type),
			    strerror(-rc));
		return rc;
	}
	if (slices)
		parms->slice_size = tf_tbl_sram_slices_2_size[slices];

	return rc;
}

int
tf_tbl_sram_bind(struct tf *tfp __rte_unused)
{
	int rc = 0;
	void *sram_handle = NULL;

	TF_CHECK_PARMS1(tfp);

	rc = tf_sram_mgr_bind(&sram_handle);

	tf_session_set_sram_db(tfp, sram_handle);

	TFP_DRV_LOG(INFO,
		    "SRAM Table - initialized\n");

	return rc;
}

int
tf_tbl_sram_unbind(struct tf *tfp __rte_unused)
{
	int rc = 0;
	void *sram_handle = NULL;

	TF_CHECK_PARMS1(tfp);

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get sram_handle from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	if (sram_handle)
		rc = tf_sram_mgr_unbind(sram_handle);

	TFP_DRV_LOG(INFO,
		    "SRAM Table - deinitialized\n");
	return rc;
}

int
tf_tbl_sram_alloc(struct tf *tfp,
		  struct tf_tbl_alloc_parms *parms)
{
	int rc;
	uint16_t idx;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_sram_mgr_alloc_parms aparms = { 0 };
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	void *sram_handle = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get tbl_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get sram_handle from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);

	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get SRAM info %s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type));
		return rc;
	}

	aparms.dir = parms->dir;
	aparms.bank_id = iparms.bank_id;
	aparms.slice_size = iparms.slice_size;
	aparms.sram_offset = &idx;
	aparms.tbl_type = parms->type;
	aparms.rm_db = tbl_db->tbl_db[parms->dir];

	rc = tf_sram_mgr_alloc(sram_handle, &aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to allocate SRAM table:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type));
		return rc;
	}
	*parms->idx = idx;

#if (DBG_SRAM == 1)
	{
		struct tf_sram_mgr_dump_parms dparms;

		dparms.dir = parms->dir;
		dparms.bank_id = iparms.bank_id;
		dparms.slice_size = iparms.slice_size;

		rc = tf_sram_mgr_dump(sram_handle, &dparms);
	}
#endif

	return rc;
}

int
tf_tbl_sram_free(struct tf *tfp __rte_unused,
		 struct tf_tbl_free_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_sram_mgr_free_parms fparms = { 0 };
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	bool allocated = false;
	void *sram_handle = NULL;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get sram_handle from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get table info:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type));
		return rc;
	}

#if (DBG_SRAM == 1)
	{
		struct tf_sram_mgr_dump_parms dparms;

		printf("%s: %s: %s\n", tf_dir_2_str(parms->dir),
		       tf_sram_slice_2_str(iparms.slice_size),
		       tf_sram_bank_2_str(iparms.bank_id));

		dparms.dir = parms->dir;
		dparms.bank_id = iparms.bank_id;
		dparms.slice_size = iparms.slice_size;

		rc = tf_sram_mgr_dump(sram_handle, &dparms);
	}
#endif

	aparms.sram_offset = parms->idx;
	aparms.slice_size = iparms.slice_size;
	aparms.bank_id = iparms.bank_id;
	aparms.dir = parms->dir;
	aparms.is_allocated = &allocated;

	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		TFP_DRV_LOG(ERR,
			    "%s: Free of invalid entry:%s idx(0x%x):(%s)\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    parms->idx,
			    strerror(-rc));
		rc = -ENOMEM;
		return rc;
	}

	fparms.rm_db = tbl_db->tbl_db[parms->dir];
	fparms.tbl_type = parms->type;
	fparms.sram_offset = parms->idx;
	fparms.slice_size = iparms.slice_size;
	fparms.bank_id = iparms.bank_id;
	fparms.dir = parms->dir;
#if (STATS_CLEAR_ON_READ_SUPPORT == 0)
	fparms.tfp = tfp;
#endif
	rc = tf_sram_mgr_free(sram_handle, &fparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to free entry:%s idx(0x%x)\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    parms->idx);
		return rc;
	}

#if (DBG_SRAM == 1)
	{
		struct tf_sram_mgr_dump_parms dparms;

		printf("%s: %s: %s\n", tf_dir_2_str(parms->dir),
		       tf_sram_slice_2_str(iparms.slice_size),
		       tf_sram_bank_2_str(iparms.bank_id));

		dparms.dir = parms->dir;
		dparms.bank_id = iparms.bank_id;
		dparms.slice_size = iparms.slice_size;

		rc = tf_sram_mgr_dump(sram_handle, &dparms);
	}
#endif
	return rc;
}

int
tf_tbl_sram_set(struct tf *tfp,
		struct tf_tbl_set_parms *parms)
{
	int rc;
	bool allocated = 0;
	int rallocated = 0;
	uint16_t hcapi_type;
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	struct tf_rm_is_allocated_parms raparms = { 0 };
	void *sram_handle = NULL;
	uint16_t base = 0, shift = 0;

	TF_CHECK_PARMS3(tfp, parms, parms->data);

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get sram_handle from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get table info:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type));
		return rc;
	}

	if (tf_session_is_shared_session(tfs)) {
		/* Only get table info if required for the device */
		if (dev->ops->tf_dev_get_tbl_info) {
			rc = dev->ops->tf_dev_get_tbl_info(tfp,
							   tbl_db->tbl_db[parms->dir],
							   parms->type,
							   &base,
							   &shift);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "%s: Failed to get table info:%d\n",
					    tf_dir_2_str(parms->dir),
					    parms->type);
				return rc;
			}
		}
		TF_TBL_PTR_TO_RM(&raparms.index, parms->idx, base, shift);

		raparms.rm_db = tbl_db->tbl_db[parms->dir];
		raparms.subtype = parms->type;
		raparms.allocated = &rallocated;
		rc = tf_rm_is_allocated(&raparms);
		if (rc)
			return rc;

		if (rallocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
			TFP_DRV_LOG(ERR,
			   "%s, Invalid or not allocated index, type:%s, idx:0x%x\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type),
			   parms->idx);
			return -EINVAL;
		}
	} else {
		aparms.sram_offset = parms->idx;
		aparms.slice_size = iparms.slice_size;
		aparms.bank_id = iparms.bank_id;
		aparms.dir = parms->dir;
		aparms.is_allocated = &allocated;
		rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
		if (rc || !allocated) {
			TFP_DRV_LOG(ERR,
				    "%s: Entry not allocated:%s idx(0x%x):(%s)\n",
				    tf_dir_2_str(parms->dir),
				    tf_tbl_type_2_str(parms->type),
				    parms->idx,
				    strerror(-rc));
			rc = -ENOMEM;
			return rc;
		}
	}
	/* Set the entry */
	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Failed type lookup, type:%s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    strerror(-rc));
		return rc;
	}

	rc = tf_msg_set_tbl_entry(tfp,
				  parms->dir,
				  hcapi_type,
				  parms->data_sz_in_bytes,
				  parms->data,
				  parms->idx);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Set failed, type:%s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    strerror(-rc));
		return rc;
	}
	return rc;
}

int
tf_tbl_sram_get(struct tf *tfp,
		struct tf_tbl_get_parms *parms)
{
	int rc;
	uint16_t hcapi_type;
	bool allocated = 0;
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	void *sram_handle = NULL;
	bool clear_on_read = false;

	TF_CHECK_PARMS3(tfp, parms, parms->data);

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get sram_handle from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get table info:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type));
		return rc;
	}

	aparms.sram_offset = parms->idx;
	aparms.slice_size = iparms.slice_size;
	aparms.bank_id = iparms.bank_id;
	aparms.dir = parms->dir;
	aparms.is_allocated = &allocated;

	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry not allocated:%s idx(0x%x):(%s)\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    parms->idx,
			    strerror(-rc));
		rc = -ENOMEM;
		return rc;
	}

	/* Get the entry */
	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Failed type lookup, type:%s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    strerror(-rc));
		return rc;
	}
	if (parms->type == TF_TBL_TYPE_ACT_STATS_64)
		clear_on_read = true;

	/* Get the entry */
	rc = tf_msg_get_tbl_entry(tfp,
				  parms->dir,
				  hcapi_type,
				  parms->data_sz_in_bytes,
				  parms->data,
				  parms->idx,
				  clear_on_read);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Get failed, type:%s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    strerror(-rc));
		return rc;
	}
	return rc;
}

int
tf_tbl_sram_bulk_get(struct tf *tfp,
		     struct tf_tbl_get_bulk_parms *parms)
{
	int rc;
	uint16_t hcapi_type;
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	uint16_t idx;
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	bool allocated = false;
	void *sram_handle = NULL;
	bool clear_on_read = false;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get em_ext_db from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to get sram_handle from session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s: Failed to get table info:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type));
		return rc;
	}

	/* Validate the start offset and the end offset is allocated
	 * This API is only used for statistics.  8 Byte entry allocation
	 * is used to verify
	 */
	aparms.sram_offset = parms->starting_idx;
	aparms.slice_size = iparms.slice_size;
	aparms.bank_id = iparms.bank_id;
	aparms.dir = parms->dir;
	aparms.is_allocated = &allocated;
	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry not allocated:%s starting_idx(%d):(%s)\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    parms->starting_idx,
			    strerror(-rc));
		rc = -ENOMEM;
		return rc;
	}
	idx = parms->starting_idx + parms->num_entries - 1;
	aparms.sram_offset = idx;
	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		TFP_DRV_LOG(ERR,
			    "%s: Entry not allocated:%s last_idx(0x%x):(%s)\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    idx,
			    strerror(-rc));
		rc = -ENOMEM;
		return rc;
	}

	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Failed type lookup, type:%s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    strerror(-rc));
		return rc;
	}

	if (parms->type == TF_TBL_TYPE_ACT_STATS_64)
		clear_on_read = true;

	/* Get the entries */
	rc = tf_msg_bulk_get_tbl_entry(tfp,
				       parms->dir,
				       hcapi_type,
				       parms->starting_idx,
				       parms->num_entries,
				       parms->entry_sz_in_bytes,
				       parms->physical_mem_addr,
				       clear_on_read);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "%s, Bulk get failed, type:%s, rc:%s\n",
			    tf_dir_2_str(parms->dir),
			    tf_tbl_type_2_str(parms->type),
			    strerror(-rc));
	}
	return rc;
}
