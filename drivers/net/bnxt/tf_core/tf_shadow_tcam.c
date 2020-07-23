/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include "tf_common.h"
#include "tf_util.h"
#include "tfp.h"
#include "tf_shadow_tcam.h"
#include "tf_hash.h"

/**
 * The implementation includes 3 tables per tcam table type.
 * - hash table
 *   - sized so that a minimum of 4 slots per shadow entry are available to
 *   minimize the likelihood of collisions.
 * - shadow key table
 *   - sized to the number of entries requested and is directly indexed
 *   - the index is zero based and is the tcam index - the base address
 *   - the key and mask are stored in the key table.
 *   - The stored key is the AND of the key/mask in order to eliminate the need
 *   to compare both the key and mask.
 * - shadow result table
 *   - the result table is stored separately since it only needs to be accessed
 *   when the key matches.
 *   - the result has a back pointer to the hash table via the hb handle.  The
 *   hb handle is a 32 bit represention of the hash with a valid bit, bucket
 *   element index, and the hash index.  It is necessary to store the hb handle
 *   with the result since subsequent removes only provide the tcam index.
 *
 * - Max entries is limited in the current implementation since bit 15 is the
 *   valid bit in the hash table.
 * - A 16bit hash is calculated and masked based on the number of entries
 * - 64b wide bucket is used and broken into 4x16bit elements.
 *   This decision is based on quicker bucket scanning to determine if any
 *   elements are in use.
 * - bit 15 of each bucket element is the valid, this is done to prevent having
 *   to read the larger key/result data for determining VALID.  It also aids
 *   in the more efficient scanning of the bucket for slot usage.
 */

/*
 * The maximum number of shadow entries supported.  The value also doubles as
 * the maximum number of hash buckets.  There are only 15 bits of data per
 * bucket to point to the shadow tables.
 */
#define TF_SHADOW_TCAM_ENTRIES_MAX (1 << 15)

/* The number of elements(BE) per hash bucket (HB) */
#define TF_SHADOW_TCAM_HB_NUM_ELEM (4)
#define TF_SHADOW_TCAM_BE_VALID (1 << 15)
#define TF_SHADOW_TCAM_BE_IS_VALID(be) (((be) & TF_SHADOW_TCAM_BE_VALID) != 0)

/**
 * The hash bucket handle is 32b
 * - bit 31, the Valid bit
 * - bit 29-30, the element
 * - bits 0-15, the hash idx (is masked based on the allocated size)
 */
#define TF_SHADOW_TCAM_HB_HANDLE_IS_VALID(hndl) (((hndl) & (1 << 31)) != 0)
#define TF_SHADOW_TCAM_HB_HANDLE_CREATE(idx, be) ((1 << 31) | \
						  ((be) << 29) | (idx))

#define TF_SHADOW_TCAM_HB_HANDLE_BE_GET(hdl) (((hdl) >> 29) & \
					      (TF_SHADOW_TCAM_HB_NUM_ELEM - 1))

#define TF_SHADOW_TCAM_HB_HANDLE_HASH_GET(ctxt, hdl)((hdl) & \
						     (ctxt)->hash_ctxt.hid_mask)

/**
 * The idx provided by the caller is within a region, so currently the base is
 * either added or subtracted from the idx to ensure it can be used as a
 * compressed index
 */

/* Convert the tcam index to a shadow index */
#define TF_SHADOW_TCAM_IDX_TO_SHIDX(ctxt, idx) ((idx) - \
						(ctxt)->shadow_ctxt.base_addr)

/* Convert the shadow index to a tcam index */
#define TF_SHADOW_TCAM_SHIDX_TO_IDX(ctxt, idx) ((idx) + \
						(ctxt)->shadow_ctxt.base_addr)

/* Simple helper masks for clearing en element from the bucket */
#define TF_SHADOW_TCAM_BE0_MASK_CLEAR(hb) ((hb) & 0xffffffffffff0000ull)
#define TF_SHADOW_TCAM_BE1_MASK_CLEAR(hb) ((hb) & 0xffffffff0000ffffull)
#define TF_SHADOW_TCAM_BE2_MASK_CLEAR(hb) ((hb) & 0xffff0000ffffffffull)
#define TF_SHADOW_TCAM_BE3_MASK_CLEAR(hb) ((hb) & 0x0000ffffffffffffull)

/**
 * This should be coming from external, but for now it is assumed that no key
 * is greater than 1K bits and no result is bigger than 128 bits.  This makes
 * allocation of the hash table easier without having to allocate on the fly.
 */
#define TF_SHADOW_TCAM_MAX_KEY_SZ 128
#define TF_SHADOW_TCAM_MAX_RESULT_SZ 16

/*
 * Local only defines for the internal data.
 */

/**
 * tf_shadow_tcam_shadow_key_entry is the key/mask entry of the key table.
 * The key stored in the table is the masked version of the key.  This is done
 * to eliminate the need of comparing both the key and mask.
 */
struct tf_shadow_tcam_shadow_key_entry {
	uint8_t key[TF_SHADOW_TCAM_MAX_KEY_SZ];
	uint8_t mask[TF_SHADOW_TCAM_MAX_KEY_SZ];
};

/**
 * tf_shadow_tcam_shadow_result_entry is the result table entry.
 * The result table writes are broken into two phases:
 * - The search phase, which stores the hb_handle and key size and
 * - The set phase, which writes the result, refcnt, and result size
 */
struct tf_shadow_tcam_shadow_result_entry {
	uint8_t result[TF_SHADOW_TCAM_MAX_RESULT_SZ];
	uint16_t result_size;
	uint16_t key_size;
	uint32_t refcnt;
	uint32_t hb_handle;
};

/**
 * tf_shadow_tcam_shadow_ctxt holds all information for accessing the key and
 * result tables.
 */
struct tf_shadow_tcam_shadow_ctxt {
	struct tf_shadow_tcam_shadow_key_entry *sh_key_tbl;
	struct tf_shadow_tcam_shadow_result_entry *sh_res_tbl;
	uint32_t base_addr;
	uint16_t num_entries;
	uint16_t alloc_idx;
};

/**
 * tf_shadow_tcam_hash_ctxt holds all information related to accessing the hash
 * table.
 */
struct tf_shadow_tcam_hash_ctxt {
	uint64_t *hashtbl;
	uint16_t hid_mask;
	uint16_t hash_entries;
};

/**
 * tf_shadow_tcam_ctxt holds the hash and shadow tables for the current shadow
 * tcam db.  This structure is per tcam table type as each tcam table has it's
 * own shadow and hash table.
 */
struct tf_shadow_tcam_ctxt {
	struct tf_shadow_tcam_shadow_ctxt shadow_ctxt;
	struct tf_shadow_tcam_hash_ctxt hash_ctxt;
};

/**
 * tf_shadow_tcam_db is the allocated db structure returned as an opaque
 * void * pointer to the caller during create db.  It holds the pointers for
 * each tcam associated with the db.
 */
struct tf_shadow_tcam_db {
	/* Each context holds the shadow and hash table information */
	struct tf_shadow_tcam_ctxt *ctxt[TF_TCAM_TBL_TYPE_MAX];
};

/**
 * Returns the number of entries in the contexts shadow table.
 */
static inline uint16_t
tf_shadow_tcam_sh_num_entries_get(struct tf_shadow_tcam_ctxt *ctxt)
{
	return ctxt->shadow_ctxt.num_entries;
}

/**
 * Compare the give key with the key in the shadow table.
 *
 * Returns 0 if the keys match
 */
static int
tf_shadow_tcam_key_cmp(struct tf_shadow_tcam_ctxt *ctxt,
		       uint8_t *key,
		       uint8_t *mask,
		       uint16_t sh_idx,
		       uint16_t size)
{
	if (size != ctxt->shadow_ctxt.sh_res_tbl[sh_idx].key_size ||
	    sh_idx >= tf_shadow_tcam_sh_num_entries_get(ctxt) || !key || !mask)
		return -1;

	return memcmp(key, ctxt->shadow_ctxt.sh_key_tbl[sh_idx].key, size);
}

/**
 * Copies the shadow result to the result.
 *
 * Returns 0 on failure
 */
static void *
tf_shadow_tcam_res_cpy(struct tf_shadow_tcam_ctxt *ctxt,
		       uint8_t *result,
		       uint16_t sh_idx,
		       uint16_t size)
{
	if (sh_idx >= tf_shadow_tcam_sh_num_entries_get(ctxt) || !result)
		return 0;

	if (ctxt->shadow_ctxt.sh_res_tbl[sh_idx].result_size != size)
		return 0;

	return memcpy(result,
		      ctxt->shadow_ctxt.sh_res_tbl[sh_idx].result,
		      size);
}

/**
 * Using a software based CRC function for now, but will look into using hw
 * assisted in the future.
 */
static uint32_t
tf_shadow_tcam_crc32_calc(uint8_t *key, uint32_t len)
{
	return tf_hash_calc_crc32(key, len);
}

/**
 * Free the memory associated with the context.
 */
static void
tf_shadow_tcam_ctxt_delete(struct tf_shadow_tcam_ctxt *ctxt)
{
	if (!ctxt)
		return;

	tfp_free(ctxt->hash_ctxt.hashtbl);
	tfp_free(ctxt->shadow_ctxt.sh_key_tbl);
	tfp_free(ctxt->shadow_ctxt.sh_res_tbl);
}

/**
 * The TF Shadow TCAM context is per TCAM and holds all information relating to
 * managing the shadow and search capability.  This routine allocated data that
 * needs to be deallocated by the tf_shadow_tcam_ctxt_delete prior when deleting
 * the shadow db.
 */
static int
tf_shadow_tcam_ctxt_create(struct tf_shadow_tcam_ctxt *ctxt,
			   uint16_t num_entries,
			   uint16_t base_addr)
{
	struct tfp_calloc_parms cparms;
	uint16_t hash_size = 1;
	uint16_t hash_mask;
	int rc;

	/* Hash table is a power of two that holds the number of entries */
	if (num_entries > TF_SHADOW_TCAM_ENTRIES_MAX) {
		TFP_DRV_LOG(ERR, "Too many entries for shadow %d > %d\n",
			    num_entries,
			    TF_SHADOW_TCAM_ENTRIES_MAX);
		return -ENOMEM;
	}

	while (hash_size < num_entries)
		hash_size = hash_size << 1;

	hash_mask = hash_size - 1;

	/* Allocate the hash table */
	cparms.nitems = hash_size;
	cparms.size = sizeof(uint64_t);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		goto error;
	ctxt->hash_ctxt.hashtbl = cparms.mem_va;
	ctxt->hash_ctxt.hid_mask = hash_mask;
	ctxt->hash_ctxt.hash_entries = hash_size;

	/* allocate the shadow tables */
	/* allocate the shadow key table */
	cparms.nitems = num_entries;
	cparms.size = sizeof(struct tf_shadow_tcam_shadow_key_entry);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		goto error;
	ctxt->shadow_ctxt.sh_key_tbl = cparms.mem_va;

	/* allocate the shadow result table */
	cparms.nitems = num_entries;
	cparms.size = sizeof(struct tf_shadow_tcam_shadow_result_entry);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		goto error;
	ctxt->shadow_ctxt.sh_res_tbl = cparms.mem_va;

	ctxt->shadow_ctxt.num_entries = num_entries;
	ctxt->shadow_ctxt.base_addr = base_addr;

	return 0;
error:
	tf_shadow_tcam_ctxt_delete(ctxt);

	return -ENOMEM;
}

/**
 * Get a shadow TCAM context given the db and the TCAM type
 */
static struct tf_shadow_tcam_ctxt *
tf_shadow_tcam_ctxt_get(struct tf_shadow_tcam_db *shadow_db,
			enum tf_tcam_tbl_type type)
{
	if (type >= TF_TCAM_TBL_TYPE_MAX ||
	    !shadow_db ||
	    !shadow_db->ctxt[type])
		return NULL;

	return shadow_db->ctxt[type];
}

/**
 * Sets the hash entry into the table given the TCAM context, hash bucket
 * handle, and shadow index.
 */
static inline int
tf_shadow_tcam_set_hash_entry(struct tf_shadow_tcam_ctxt *ctxt,
			      uint32_t hb_handle,
			      uint16_t sh_idx)
{
	uint16_t hid = TF_SHADOW_TCAM_HB_HANDLE_HASH_GET(ctxt, hb_handle);
	uint16_t be = TF_SHADOW_TCAM_HB_HANDLE_BE_GET(hb_handle);
	uint64_t entry = sh_idx | TF_SHADOW_TCAM_BE_VALID;

	if (hid >= ctxt->hash_ctxt.hash_entries)
		return -EINVAL;

	ctxt->hash_ctxt.hashtbl[hid] |= entry << (be * 16);
	return 0;
}

/**
 * Clears the hash entry given the TCAM context and hash bucket handle.
 */
static inline void
tf_shadow_tcam_clear_hash_entry(struct tf_shadow_tcam_ctxt *ctxt,
				uint32_t hb_handle)
{
	uint16_t hid, be;
	uint64_t *bucket;

	if (!TF_SHADOW_TCAM_HB_HANDLE_IS_VALID(hb_handle))
		return;

	hid = TF_SHADOW_TCAM_HB_HANDLE_HASH_GET(ctxt, hb_handle);
	be = TF_SHADOW_TCAM_HB_HANDLE_BE_GET(hb_handle);
	bucket = &ctxt->hash_ctxt.hashtbl[hid];

	switch (be) {
	case 0:
		*bucket = TF_SHADOW_TCAM_BE0_MASK_CLEAR(*bucket);
		break;
	case 1:
		*bucket = TF_SHADOW_TCAM_BE1_MASK_CLEAR(*bucket);
		break;
	case 2:
		*bucket = TF_SHADOW_TCAM_BE2_MASK_CLEAR(*bucket);
		break;
	case 3:
		*bucket = TF_SHADOW_TCAM_BE2_MASK_CLEAR(*bucket);
		break;
	default:
		/*
		 * Since the BE_GET masks non-inclusive bits, this will not
		 * happen.
		 */
		break;
	}
}

/**
 * Clears the shadow key and result entries given the TCAM context and
 * shadow index.
 */
static void
tf_shadow_tcam_clear_sh_entry(struct tf_shadow_tcam_ctxt *ctxt,
			      uint16_t sh_idx)
{
	struct tf_shadow_tcam_shadow_key_entry *sk_entry;
	struct tf_shadow_tcam_shadow_result_entry *sr_entry;

	if (sh_idx >= tf_shadow_tcam_sh_num_entries_get(ctxt))
		return;

	sk_entry = &ctxt->shadow_ctxt.sh_key_tbl[sh_idx];
	sr_entry = &ctxt->shadow_ctxt.sh_res_tbl[sh_idx];

	/*
	 * memset key/result to zero for now, possibly leave the data alone
	 * in the future and rely on the valid bit in the hash table.
	 */
	memset(sk_entry, 0, sizeof(struct tf_shadow_tcam_shadow_key_entry));
	memset(sr_entry, 0, sizeof(struct tf_shadow_tcam_shadow_result_entry));
}

/**
 * Binds the allocated tcam index with the hash and shadow tables.
 * The entry will be incomplete until the set has happened with the result
 * data.
 */
int
tf_shadow_tcam_bind_index(struct tf_shadow_tcam_bind_index_parms *parms)
{
	int rc;
	int i;
	uint16_t idx, klen;
	struct tf_shadow_tcam_ctxt *ctxt;
	struct tf_shadow_tcam_db *shadow_db;
	struct tf_shadow_tcam_shadow_key_entry *sk_entry;
	struct tf_shadow_tcam_shadow_result_entry *sr_entry;
	uint8_t tkey[TF_SHADOW_TCAM_MAX_KEY_SZ];

	if (!parms || !TF_SHADOW_TCAM_HB_HANDLE_IS_VALID(parms->hb_handle) ||
	    !parms->key || !parms->mask) {
		TFP_DRV_LOG(ERR, "Invalid parms\n");
		return -EINVAL;
	}

	shadow_db = (struct tf_shadow_tcam_db *)parms->shadow_db;
	ctxt = tf_shadow_tcam_ctxt_get(shadow_db, parms->type);
	if (!ctxt) {
		TFP_DRV_LOG(DEBUG, "%s no ctxt for table\n",
			    tf_tcam_tbl_2_str(parms->type));
		return -EINVAL;
	}

	memset(tkey, 0, sizeof(tkey));
	idx = TF_SHADOW_TCAM_IDX_TO_SHIDX(ctxt, parms->idx);
	klen = parms->key_size;
	if (idx >= tf_shadow_tcam_sh_num_entries_get(ctxt) ||
	    klen > TF_SHADOW_TCAM_MAX_KEY_SZ) {
		TFP_DRV_LOG(ERR, "%s:%s Invalid len (%d) > %d || oob idx %d\n",
			    tf_dir_2_str(parms->dir),
			    tf_tcam_tbl_2_str(parms->type),
			    klen,
			    TF_SHADOW_TCAM_MAX_KEY_SZ, idx);

		return -EINVAL;
	}

	rc = tf_shadow_tcam_set_hash_entry(ctxt, parms->hb_handle, idx);
	if (rc)
		return -EINVAL;

	sk_entry = &ctxt->shadow_ctxt.sh_key_tbl[idx];
	sr_entry = &ctxt->shadow_ctxt.sh_res_tbl[idx];

	/*
	 * Write the masked key to the table for more efficient comparisons
	 * later.
	 */
	for (i = 0; i < klen; i++)
		tkey[i] = parms->key[i] & parms->mask[i];

	memcpy(sk_entry->key, tkey, klen);
	memcpy(sk_entry->mask, parms->mask, klen);

	/* Write the result table */
	sr_entry->key_size = parms->key_size;
	sr_entry->hb_handle = parms->hb_handle;
	sr_entry->refcnt = 1;

	return 0;
}

/**
 * Deletes hash/shadow information if no more references.
 *
 * Returns 0 - The caller should delete the tcam entry in hardware.
 * Returns non-zero - The number of references to the entry
 */
int
tf_shadow_tcam_remove(struct tf_shadow_tcam_remove_parms *parms)
{
	uint16_t idx;
	uint32_t hb_handle;
	struct tf_shadow_tcam_ctxt *ctxt;
	struct tf_shadow_tcam_db *shadow_db;
	struct tf_tcam_free_parms *fparms;
	struct tf_shadow_tcam_shadow_result_entry *sr_entry;

	if (!parms || !parms->fparms) {
		TFP_DRV_LOG(ERR, "Invalid parms\n");
		return -EINVAL;
	}

	fparms = parms->fparms;

	/*
	 * Initialize the reference count to zero.  It will only be changed if
	 * non-zero.
	 */
	fparms->ref_cnt = 0;

	shadow_db = (struct tf_shadow_tcam_db *)parms->shadow_db;
	ctxt = tf_shadow_tcam_ctxt_get(shadow_db, fparms->type);
	if (!ctxt) {
		TFP_DRV_LOG(DEBUG, "%s no ctxt for table\n",
			    tf_tcam_tbl_2_str(fparms->type));
		return 0;
	}

	idx = TF_SHADOW_TCAM_IDX_TO_SHIDX(ctxt, fparms->idx);
	if (idx >= tf_shadow_tcam_sh_num_entries_get(ctxt)) {
		TFP_DRV_LOG(DEBUG, "%s %d >= %d\n",
			    tf_tcam_tbl_2_str(fparms->type),
			    fparms->idx,
			    tf_shadow_tcam_sh_num_entries_get(ctxt));
		return 0;
	}

	sr_entry = &ctxt->shadow_ctxt.sh_res_tbl[idx];
	if (sr_entry->refcnt <= 1) {
		hb_handle = sr_entry->hb_handle;
		tf_shadow_tcam_clear_hash_entry(ctxt, hb_handle);
		tf_shadow_tcam_clear_sh_entry(ctxt, idx);
	} else {
		sr_entry->refcnt--;
		fparms->ref_cnt = sr_entry->refcnt;
	}

	return 0;
}

int
tf_shadow_tcam_search(struct tf_shadow_tcam_search_parms *parms)
{
	uint16_t len;
	uint8_t rcopy;
	uint64_t bucket;
	uint32_t i, hid32;
	struct tf_shadow_tcam_ctxt *ctxt;
	struct tf_shadow_tcam_db *shadow_db;
	uint16_t hid16, hb_idx, hid_mask, shtbl_idx, shtbl_key, be_valid;
	struct tf_tcam_alloc_search_parms *sparms;
	uint8_t tkey[TF_SHADOW_TCAM_MAX_KEY_SZ];
	uint32_t be_avail = TF_SHADOW_TCAM_HB_NUM_ELEM;

	if (!parms || !parms->sparms) {
		TFP_DRV_LOG(ERR, "tcam search with invalid parms\n");
		return -EINVAL;
	}

	memset(tkey, 0, sizeof(tkey));
	sparms = parms->sparms;

	/* Initialize return values to invalid */
	sparms->hit = 0;
	sparms->search_status = REJECT;
	parms->hb_handle = 0;
	sparms->ref_cnt = 0;
	/* see if caller wanted the result */
	rcopy = sparms->result && sparms->result_size;

	shadow_db = (struct tf_shadow_tcam_db *)parms->shadow_db;
	ctxt = tf_shadow_tcam_ctxt_get(shadow_db, sparms->type);
	if (!ctxt) {
		TFP_DRV_LOG(ERR, "%s Unable to get tcam mgr context\n",
			    tf_tcam_tbl_2_str(sparms->type));
		return -EINVAL;
	}

	hid_mask = ctxt->hash_ctxt.hid_mask;

	len = sparms->key_size;

	if (len > TF_SHADOW_TCAM_MAX_KEY_SZ ||
	    !sparms->key || !sparms->mask || !len) {
		TFP_DRV_LOG(ERR, "%s:%s Invalid parms %d : %p : %p\n",
			    tf_dir_2_str(sparms->dir),
			    tf_tcam_tbl_2_str(sparms->type),
			    len,
			    sparms->key,
			    sparms->mask);
		return -EINVAL;
	}

	/* Combine the key and mask */
	for (i = 0; i < len; i++)
		tkey[i] = sparms->key[i] & sparms->mask[i];

	/*
	 * Calculate the crc32
	 * Fold it to create a 16b value
	 * Reduce it to fit the table
	 */
	hid32 = tf_shadow_tcam_crc32_calc(tkey, len);
	hid16 = (uint16_t)(((hid32 >> 16) & 0xffff) ^ (hid32 & 0xffff));
	hb_idx = hid16 & hid_mask;

	bucket = ctxt->hash_ctxt.hashtbl[hb_idx];

	if (!bucket) {
		/* empty bucket means a miss and available entry */
		sparms->search_status = MISS;
		parms->hb_handle = TF_SHADOW_TCAM_HB_HANDLE_CREATE(hb_idx, 0);
		sparms->idx = 0;
		return 0;
	}

	/* Set the avail to max so we can detect when there is an avail entry */
	be_avail = TF_SHADOW_TCAM_HB_NUM_ELEM;
	for (i = 0; i < TF_SHADOW_TCAM_HB_NUM_ELEM; i++) {
		shtbl_idx = (uint16_t)((bucket >> (i * 16)) & 0xffff);
		be_valid = TF_SHADOW_TCAM_BE_IS_VALID(shtbl_idx);
		if (!be_valid) {
			/* The element is avail, keep going */
			be_avail = i;
			continue;
		}
		/* There is a valid entry, compare it */
		shtbl_key = shtbl_idx & ~TF_SHADOW_TCAM_BE_VALID;
		if (!tf_shadow_tcam_key_cmp(ctxt,
					    sparms->key,
					    sparms->mask,
					    shtbl_key,
					    sparms->key_size)) {
			/*
			 * It matches, increment the ref count if the caller
			 * requested allocation and return the info
			 */
			if (sparms->alloc)
				ctxt->shadow_ctxt.sh_res_tbl[shtbl_key].refcnt =
			ctxt->shadow_ctxt.sh_res_tbl[shtbl_key].refcnt + 1;

			sparms->hit = 1;
			sparms->search_status = HIT;
			parms->hb_handle =
				TF_SHADOW_TCAM_HB_HANDLE_CREATE(hb_idx, i);
			sparms->idx = TF_SHADOW_TCAM_SHIDX_TO_IDX(ctxt,
								  shtbl_key);
			sparms->ref_cnt =
				ctxt->shadow_ctxt.sh_res_tbl[shtbl_key].refcnt;

			/* copy the result, if caller wanted it. */
			if (rcopy &&
			    !tf_shadow_tcam_res_cpy(ctxt,
						    sparms->result,
						    shtbl_key,
						    sparms->result_size)) {
				/*
				 * Should never get here, possible memory
				 * corruption or something unexpected.
				 */
				TFP_DRV_LOG(ERR, "Error copying result\n");
				return -EINVAL;
			}

			return 0;
		}
	}

	/* No hits, return avail entry if exists */
	if (be_avail < TF_SHADOW_TCAM_HB_NUM_ELEM) {
		parms->hb_handle =
			TF_SHADOW_TCAM_HB_HANDLE_CREATE(hb_idx, be_avail);
		sparms->search_status = MISS;
		sparms->hit = 0;
		sparms->idx = 0;
	} else {
		sparms->search_status = REJECT;
	}

	return 0;
}

int
tf_shadow_tcam_insert(struct tf_shadow_tcam_insert_parms *parms)
{
	uint16_t idx;
	struct tf_shadow_tcam_ctxt *ctxt;
	struct tf_tcam_set_parms *sparms;
	struct tf_shadow_tcam_db *shadow_db;
	struct tf_shadow_tcam_shadow_result_entry *sr_entry;

	if (!parms || !parms->sparms) {
		TFP_DRV_LOG(ERR, "Null parms\n");
		return -EINVAL;
	}

	sparms = parms->sparms;
	if (!sparms->result || !sparms->result_size) {
		TFP_DRV_LOG(ERR, "%s:%s No result to set.\n",
			    tf_dir_2_str(sparms->dir),
			    tf_tcam_tbl_2_str(sparms->type));
		return -EINVAL;
	}

	shadow_db = (struct tf_shadow_tcam_db *)parms->shadow_db;
	ctxt = tf_shadow_tcam_ctxt_get(shadow_db, sparms->type);
	if (!ctxt) {
		/* We aren't tracking this table, so return success */
		TFP_DRV_LOG(DEBUG, "%s Unable to get tcam mgr context\n",
			    tf_tcam_tbl_2_str(sparms->type));
		return 0;
	}

	idx = TF_SHADOW_TCAM_IDX_TO_SHIDX(ctxt, sparms->idx);
	if (idx >= tf_shadow_tcam_sh_num_entries_get(ctxt)) {
		TFP_DRV_LOG(ERR, "%s:%s Invalid idx(0x%x)\n",
			    tf_dir_2_str(sparms->dir),
			    tf_tcam_tbl_2_str(sparms->type),
			    sparms->idx);
		return -EINVAL;
	}

	/* Write the result table, the key/hash has been written already */
	sr_entry = &ctxt->shadow_ctxt.sh_res_tbl[idx];

	/*
	 * If the handle is not valid, the bind was never called.  We aren't
	 * tracking this entry.
	 */
	if (!TF_SHADOW_TCAM_HB_HANDLE_IS_VALID(sr_entry->hb_handle))
		return 0;

	if (sparms->result_size > TF_SHADOW_TCAM_MAX_RESULT_SZ) {
		TFP_DRV_LOG(ERR, "%s:%s Result length %d > %d\n",
			    tf_dir_2_str(sparms->dir),
			    tf_tcam_tbl_2_str(sparms->type),
			    sparms->result_size,
			    TF_SHADOW_TCAM_MAX_RESULT_SZ);
		return -EINVAL;
	}

	memcpy(sr_entry->result, sparms->result, sparms->result_size);
	sr_entry->result_size = sparms->result_size;

	return 0;
}

int
tf_shadow_tcam_free_db(struct tf_shadow_tcam_free_db_parms *parms)
{
	struct tf_shadow_tcam_db *shadow_db;
	int i;

	TF_CHECK_PARMS1(parms);

	shadow_db = (struct tf_shadow_tcam_db *)parms->shadow_db;
	if (!shadow_db) {
		TFP_DRV_LOG(DEBUG, "Shadow db is NULL cannot be freed\n");
		return -EINVAL;
	}

	for (i = 0; i < TF_TCAM_TBL_TYPE_MAX; i++) {
		if (shadow_db->ctxt[i]) {
			tf_shadow_tcam_ctxt_delete(shadow_db->ctxt[i]);
			tfp_free(shadow_db->ctxt[i]);
		}
	}

	tfp_free(shadow_db);

	return 0;
}

/**
 * Allocate the TCAM resources for search and allocate
 *
 */
int tf_shadow_tcam_create_db(struct tf_shadow_tcam_create_db_parms *parms)
{
	int rc;
	int i;
	uint16_t base;
	struct tfp_calloc_parms cparms;
	struct tf_shadow_tcam_db *shadow_db = NULL;

	TF_CHECK_PARMS1(parms);

	/* Build the shadow DB per the request */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_shadow_tcam_db);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc)
		return rc;
	shadow_db = (void *)cparms.mem_va;

	for (i = 0; i < TF_TCAM_TBL_TYPE_MAX; i++) {
		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (!parms->cfg->alloc_cnt[i]) {
			shadow_db->ctxt[i] = NULL;
			continue;
		}

		cparms.nitems = 1;
		cparms.size = sizeof(struct tf_shadow_tcam_ctxt);
		cparms.alignment = 0;
		rc = tfp_calloc(&cparms);
		if (rc)
			goto error;

		shadow_db->ctxt[i] = cparms.mem_va;
		base = parms->cfg->base_addr[i];
		rc = tf_shadow_tcam_ctxt_create(shadow_db->ctxt[i],
						parms->cfg->alloc_cnt[i],
						base);
		if (rc)
			goto error;
	}

	*parms->shadow_db = (void *)shadow_db;

	TFP_DRV_LOG(INFO,
		    "TF SHADOW TCAM - initialized\n");

	return 0;
error:
	for (i = 0; i < TF_TCAM_TBL_TYPE_MAX; i++) {
		if (shadow_db->ctxt[i]) {
			tf_shadow_tcam_ctxt_delete(shadow_db->ctxt[i]);
			tfp_free(shadow_db->ctxt[i]);
		}
	}

	tfp_free(shadow_db);

	return -ENOMEM;
}
