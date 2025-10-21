/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_H_
#define _TFC_H_

#include <stdint.h>
#include <stdbool.h>
#include "cfa_resources.h"
#include "cfa_types.h"

/**
 * @file
 *
 * @brief TFC (Truflow Core v3) API Header File
 *
 * @page TFCV3 Truflow Core v3
 *
 * These pages describe the APIs for Truflow Core v3.
 *
 * - @subpage Init
 * - @subpage Resources
 * - @subpage Session
 * - @subpage GID
 * - @subpage Identifiers
 * - @subpage GIM
 * - @subpage TCAM
 * - @subpage TBM
 * - @subpage EM
 * - @subpage ACTCFA
 * - @subpage TFCOV3
 */

/********** BEGIN Truflow Core v3 DEFINITIONS **********/
/* @cond temporary */
#define TFC_V3_RESOURCE_API_ENABLE 0
/* @endcond */

/**
 * TFC handle
 */
struct tfc {
	/**
	 * Pointer to the private tfc object
	 */
	void *tfo;
	/**
	 * the pointer to the parent bp struct
	 */
	void *bp;
};

/* API Guidance:
 *
 * 1.  If more than 5-6 parameters, please define structures
 *
 * 2.  Design structures that can be used with multiple APIs
 *
 * 3.  If items in structures are not to be used, these must
 *     be documented in the API header IN DETAIL.
 *
 * 4.  Use defines in cfa_types.h where possible. These are shared
 *     firmware types to avoid duplication. These types do
 *     not represent the HWRM interface and may need to be mapped
 *     to HWRM definitions.
 *
 * 5.  Resource types and subtypes are defined in cfa_resources.h
 */

/********** BEGIN API FUNCTION PROTOTYPES/PARAMETERS **********/
/**
 * @page Init Initialization and De-initialization APIs
 *
 * @ref tfc_open
 *
 * @ref tfc_close
 */
/**
 * Allocate the TFC state for this DPDK port/function. The TF
 * object memory is allocated during this API call.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *
 * @note This API will initialize only the software state.
 */
int tfc_open(struct tfc *tfcp);

/**
 * De-allocate the TFC state for this DPDK port/function. The TF
 * object memory is deallocated during this API call.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *
 * @note This API will reset only the software state.
 */
int tfc_close(struct tfc *tfcp);

/**
 * @page Resources
 *
 * @ref tfc_resource_types_query
 */

/**
 * The maximum number of foreseeable resource types.
 * Use cfa_resource_types enum internally.
 */
#define TFC_MAX_RESOURCE_TYPES 32

/**
 * Supported resource information
 */
struct tfc_resources {
	/** Resource subtype mask of valid resource types */
	uint32_t rtypes_mask;
	/** Maximum resource type number */
	uint8_t max_rtype;
	/** Array indexed by resource type indicating valid subtypes */
	uint32_t rsubtypes_mask[TFC_MAX_RESOURCE_TYPES];
};

/**
 * Get all supported CFA resource types for the device
 *
 * This API goes to the firmware to query all supported resource
 * types and subtypes supported.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in,out] resources
 *   Pointer to a structure containing information about the supported CFA device
 *   resources.
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_resource_types_query(struct tfc *tfcp, struct tfc_resources *resources);

/**
 * @page Session
 *
 * @ref tfc_session_id_alloc
 *
 * @ref tfc_session_fid_add
 *
 * @ref tfc_session_fid_rem
 */

/**
 * Allocate a TFC session
 *
 * This API goes to the firmware to allocate a TFC session id and associate a
 * forwarding function with the session.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
- * @param[in] fid
- *   Function id to associated with the session
- *
 * @param[out] sid
 *   Pointer to the where the session id will be returned
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_id_alloc(struct tfc *tfcp, uint16_t fid, uint16_t *sid);

/**
 * Associate a forwarding function with an existing TFC session
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function id to associated with the session
 *
 * @param[in] sid
 *   The session id to associate with
 *
 * @param[in,out] fid_cnt
 *   The number of forwarding functions currently associated with the session
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_fid_add(struct tfc *tfcp, uint16_t fid, uint16_t sid,
			uint16_t *fid_cnt);
/**
 * Disassociate a forwarding function from an existing TFC session
 *
 * Once the last function has been removed from the session in the firmware
 * the session is freed and all associated resources freed.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function id to associated with the session
 *
 * @param[in,out] fid_cnt
 *   The number of forwarding functions currently associated with the session
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_fid_rem(struct tfc *tfcp, uint16_t fid, uint16_t *fid_cnt);

/**
 * @page GID Global Identifier
 *
 * @ref tfc_global_id_alloc
 */

/** Domain id range
 */
enum tfc_domain_id {
	TFC_DOMAIN_ID_INVALID = 0,
	TFC_DOMAIN_ID_1,
	TFC_DOMAIN_ID_2,
	TFC_DOMAIN_ID_3,
	TFC_DOMAIN_ID_4,
	TFC_DOMAIN_ID_MAX = TFC_DOMAIN_ID_4
};

/** Global id request definition
 */
struct tfc_global_id_req {
	enum cfa_resource_type rtype; /**< Resource type */
	uint8_t rsubtype; /**< Resource subtype */
	enum cfa_dir dir; /**< Direction */
	uint8_t *context_id;
	uint16_t context_len;
	uint16_t resource_id;
};

/** Global id resource definition
 */
struct tfc_global_id {
	enum cfa_resource_type rtype; /**< Resource type */
	uint8_t rsubtype; /**< Resource subtype */
	enum cfa_dir dir; /**< Direction */
	uint16_t id; /**< Resource id */
};

/**
 * Allocate global TFC resources
 *
 * Some resources are not owned by a single session.  They are "global" in that
 * they will be in use as long as any associated session exists.  Once all
 * sessions/functions have been removed, all associated global ids are freed.
 * There are currently up to 4 global id domain sets.
 *
 * TODO: REDUCE PARAMETERS WHEN IMPLEMENTING API
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] glb_id_req
 *   The list of global id requests
 *
 * @param[out] glb_id_rsp
 *   The number of items in the response buffer
 *
 * @param[in,out] first
 *   This is the first domain request for the indicated domain id.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_global_id_alloc(struct tfc *tfcp, uint16_t fid,
			const struct tfc_global_id_req *glb_id_req,
			struct tfc_global_id *glb_id_rsp,
			bool *first);

/**
 * Free global Identifier TFC resources
 *
 * Some resources are not owned by a single session.  They are "global" in that
 * they will be in use as long as any associated session exists.  Once all
 * sessions/functions have been removed, all associated global ids are freed.
 * There are currently up to 4 global id domain sets.
 *
 * TODO: REDUCE PARAMETERS WHEN IMPLEMENTING API
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] glb_id_req
 *   The list of global id requests
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_global_id_free(struct tfc *tfcp, uint16_t fid,
		       const struct tfc_global_id_req *glb_id_req);

/**
 * @page Identifiers
 *
 * @ref tfc_identifier_alloc
 *
 * @ref tfc_identifier_free
 */

/**
 * Identifier resource structure
 */
struct tfc_identifier_info {
	enum cfa_resource_subtype_ident rsubtype; /**< resource subtype */
	enum cfa_dir dir; /**< direction rx/tx */
	uint16_t id; /**< alloc/free index */
};

/**
 * allocate a TFC Identifier
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tt
 *   Track type - either track by session or by function
 *
 * @param[in, out] ident_info
 *   All the information related to the requested identifier (subtype/dir) and
 *   the returned identifier id.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_identifier_alloc(struct tfc *tfcp, uint16_t fid,
			 enum cfa_track_type tt,
			 struct tfc_identifier_info *ident_info);

/**
 * free a TFC Identifier
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] ident_info
 *   All the information related to the requested identifier (subtype/dir) and
 *   the identifier id to free.
 *
 * @returns success or failure code.
 */
int tfc_identifier_free(struct tfc *tfcp, uint16_t fid,
			const struct tfc_identifier_info *ident_info);

/**
 * @page GIM Index Table
 *
 * @ref tfc_idx_tbl_alloc
 *
 * @ref tfc_idx_tbl_alloc_set
 *
 * @ref tfc_idx_tbl_set
 *
 * @ref tfc_idx_tbl_get
 *
 * @ref tfc_idx_tbl_free
 */

/**
 * Index table resource structure
 */
struct tfc_idx_tbl_info {
	enum cfa_resource_subtype_idx_tbl rsubtype; /**< resource subtype */
	enum cfa_dir dir; /**< direction rx/tx */
	uint16_t id; /**< alloc/free index */
	enum cfa_resource_blktype_idx_tbl blktype; /**< block type */
};

/**
 * allocate a TFC index table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tt
 *   Track type - either track by session or by function
 *
 * @param[in,out] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   and the returned id.
 *
 * @returns
 *	 0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_alloc(struct tfc *tfcp, uint16_t fid,
		      enum cfa_track_type tt,
		      struct tfc_idx_tbl_info *tbl_info);

/**
 * allocate and set a TFC index table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tt
 *   Track type - either track by session or by function
 *
 * @param[in,out] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   and the returned id.
 *
 * @param[in] data
 *   Pointer to the data to write to the entry.	The data is aligned correctly
 *   in the buffer for writing to the hardware.
 *
 * @param[in] data_sz_in_bytes
 *   The size of the entry in bytes for Thor2.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_alloc_set(struct tfc *tfcp, uint16_t fid,
			  enum cfa_track_type tt,
			  struct tfc_idx_tbl_info *tbl_info,
			  const uint32_t *data, uint8_t data_sz_in_bytes);

/**
 * Set a TFC index table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   including the id.
 *
 * @param[in] data
 *   Pointer to the data to write to the entry.	The data is aligned correctly
 *   in the buffer for writing to the hardware.
 *
 * @param[in] data_sz_in_bytes
 *   The size of the entry in device sized bytes for Thor2.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_set(struct tfc *tfcp, uint16_t fid,
		    const struct tfc_idx_tbl_info *tbl_info,
		    const uint32_t *data, uint8_t data_sz_in_bytes);

/**
 * Get a TFC index table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   including the id.
 *
 * @param[in, out] data
 *   Pointer to the data to read from the entry.
 *
 * @param[in,out] data_sz_in_bytes
 *   The size of the entry in device sized bytes for Thor2. Input is the
 *   size of the buffer, output is the actual size.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_get(struct tfc *tfcp, uint16_t fid,
		    const struct tfc_idx_tbl_info *tbl_info,
		    uint32_t *data, uint8_t *data_sz_in_bytes);
/**
 * Free a TFC index table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   and the returned id.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_free(struct tfc *tfcp, uint16_t fid,
		     const struct tfc_idx_tbl_info *tbl_info);

/**
 * @page TCAM
 *
 * @ref tfc_tcam_alloc
 *
 * @ref tfc_tcam_alloc_set
 *
 * @ref tfc_tcam_set
 *
 * @ref tfc_tcam_get
 *
 * @ref tfc_tcam_free
 */
/**
 * Tcam table info structure
 */
struct tfc_tcam_info {
	enum cfa_resource_subtype_tcam rsubtype; /**< resource subtype */
	enum cfa_dir dir; /**< direction rx/tx */
	uint16_t id; /**< alloc/free index */
};

/**
 * Tcam table resource structure
 */
struct tfc_tcam_data {
	uint8_t *key; /**< tcam key */
	uint8_t *mask; /**< tcam mask */
	uint8_t *remap; /**< remap */
	uint8_t key_sz_in_bytes; /**< key size in bytes */
	uint8_t remap_sz_in_bytes; /**< remap size in bytes */

};

/**
 * allocate a TFC TCAM entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tt
 *   Track type - either track by session or by function
 *
 * @param[in] priority
 *   Priority - the priority of the tcam entry
 *
 * @param[in,out] tcam_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   and the returned id.
 *
 * @param[in] key_sz_in_bytes
 *   The size of the entry in bytes for Thor2.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_alloc(struct tfc *tfcp, uint16_t fid,
		   enum cfa_track_type tt, uint16_t priority,
		   uint8_t key_sz_in_bytes,
		   struct tfc_tcam_info *tcam_info);

/**
 * allocate and set a TFC TCAM entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tt
 *   Track type - either track by session or by function
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] priority
 *   Priority - the priority of the tcam entry
 *
 * @param[in,out] tcam_info
 *   All the information related to the requested TCAM table entry (subtype/dir)
 *   and the returned id.
 *
 * @param[in] tcam_data
 *  Pointer to the tcam data, including tcam, mask, and remap,  to write to
 *  the entry.  The data is aligned in the buffer for writing to the hardware.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_alloc_set(struct tfc *tfcp, uint16_t fid,
		       enum cfa_track_type tt, uint16_t priority,
		       struct tfc_tcam_info *tcam_info,
		       const struct tfc_tcam_data *tcam_data);

/**
 * Set a TFC TCAM entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in, out] tcam_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   including the id.
 *
 * @param[in] tcam_data
 *  Pointer to the tcam data, including tcam, mask, and remap,  to write to
 *  the entry.  The data is aligned in the buffer for writing to the hardware.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_set(struct tfc *tfcp, uint16_t fid,
		 const struct tfc_tcam_info *tcam_info,
		 const struct tfc_tcam_data *tcam_data);

/**
 * Get a TFC TCAM entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tcam_info
 *   All the information related to the requested TCAM entry (subtype/dir)
 *   including the id.
 *
 * @param[in, out] tcam_data
 *  Pointer to the tcam data, including tcam, mask, and remap,  to read from
 *  the entry.  The data is aligned in the buffer for writing to the hardware.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_get(struct tfc *tfcp, uint16_t fid,
		 const struct tfc_tcam_info *tcam_info,
		 struct tfc_tcam_data *tcam_data);
/**
 * Free a TFC TCAM entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tcam_info
 *   All the information related to the requested tcam entry (subtype/dir)
 *   and the id to be freed.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_free(struct tfc *tfcp, uint16_t fid,
		  const struct tfc_tcam_info *tcam_info);

/**
 * Update the TFC TCAM entry priority
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tt
 *   Track type - either track by session or by function
 *
 * @param[in] tcam_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   including the id.
 *
 * @param[in] priority
 *  The priority of the tcam entry to be updated with.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_priority_update(struct tfc *tfcp, uint16_t fid,
			     enum cfa_track_type tt,
			     const struct tfc_tcam_info *tcam_info,
			     uint16_t priority);

/**
 * @page TBM Table Scope
 *
 * @ref tfc_tbl_scope_qcaps
 *
 * @ref tfc_tbl_scope_size_query
 *
 * @ref tfc_tbl_scope_id_alloc
 *
 * @ref tfc_tbl_scope_mem_alloc
 *
 * @ref tfc_tbl_scope_mem_free
 *
 * @ref tfc_tbl_scope_cpm_alloc
 *
 * @ref tfc_tbl_scope_cpm_free
 *
 * @ref tfc_tbl_scope_fid_add
 *
 * @ref tfc_tbl_scope_fid_rem
 *
 * @ref tfc_tbl_scope_config_state_get
 *
 * tfc_tbl_scope_pfid_query (FUTURE shared table scope)
 *
 * tfc_tbl_scope_config_get (FUTURE shared table scope)
 *
 */

/**
 * tfc_tbl_scope_bucket_factor indicates a multiplier factor for determining the
 * static and dynamic buckets counts.  The larger the factor, the more buckets
 * will be allocated.
 *
 * This is necessary because flows will not hash so as to perfectly fill all the
 * buckets.  It is necessary to add some allowance for not fully populated
 * buckets.
 */
enum tfc_tbl_scope_bucket_factor {
	TFC_TBL_SCOPE_BUCKET_FACTOR_1 = 1,
	TFC_TBL_SCOPE_BUCKET_FACTOR_2 = 2,
	TFC_TBL_SCOPE_BUCKET_FACTOR_4 = 4,
	TFC_TBL_SCOPE_BUCKET_FACTOR_8 = 8,
	TFC_TBL_SCOPE_BUCKET_FACTOR_16 = 16,
	TFC_TBL_SCOPE_BUCKET_FACTOR_32 = 32,
	TFC_TBL_SCOPE_BUCKET_FACTOR_64 = 64,
	TFC_TBL_SCOPE_BUCKET_FACTOR_MAX = TFC_TBL_SCOPE_BUCKET_FACTOR_64
};

/**
 * tfc_tbl_scope_size_query_parms contains the parameters for the
 * tfc_tbl_scope_size_query API.
 */
struct tfc_tbl_scope_size_query_parms {
	/** Scope is one of non-shared, shared-app or global.
	 * If a shared-app or global table scope, dynamic buckets are disabled.
	 * this combined with the multiplier affects the calculation for static
	 * buckets in this function.
	 */
	enum cfa_scope_type scope_type;
	/**
	 * [in] Direction indexed array indicating the number of flows.  Must be
	 * at least as large as the number entries that the buckets can point
	 * to.
	 */
	uint32_t flow_cnt[CFA_DIR_MAX];
	/**
	 * [in] tfc_tbl_scope_bucket_factor indicates a multiplier factor for
	 * determining the static and dynamic buckets counts.  The larger the
	 * factor, the more buckets will be allocated.
	 */
	enum tfc_tbl_scope_bucket_factor factor;
	/**
	 * [in] The number of pools each region of the table scope will be
	 * divided into.
	 */
	uint32_t max_pools;
	/**
	 * [in] Direction indexed array indicating the key size.
	 */
	uint16_t key_sz_in_bytes[CFA_DIR_MAX];
	/**
	 * [in] Direction indexed array indicating the action record size.  Must
	 * be a multiple of 32B lines on Thor2.
	 */
	uint16_t act_rec_sz_in_bytes[CFA_DIR_MAX];
	/**
	 * [out] Direction indexed array indicating the EM static bucket count
	 * expressed as: log2(static_bucket_count). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be returned.
	 */
	uint8_t static_bucket_cnt_exp[CFA_DIR_MAX];
	/**
	 * [out] Direction indexed array indicating the EM dynamic bucket count.
	 */
	uint32_t dynamic_bucket_cnt[CFA_DIR_MAX];
	/**
	 * [out] The number of minimum sized lkup records per direction.  In
	 * this usage, records are the minimum lookup memory allocation unit in
	 * a table scope.  This value is the total memory required for buckets
	 * and entries.
	 *
	 * Note: The EAS variously refers to these as words or cache-lines.
	 *
	 * For example, on Thor2 where each bucket consumes one record, if the
	 * key size is such that the LREC and key use 2 records, then the
	 * lkup_rec_cnt = the number of buckets + (2 * the number of flows).
	 */
	uint32_t lkup_rec_cnt[CFA_DIR_MAX];
	/**
	 * [out] The number of minimum sized action records per direction.
	 * Similar to the lkup_rec_cnt, records are the minimum action memory
	 * allocation unit in a table scope.
	 */
	uint32_t act_rec_cnt[CFA_DIR_MAX];
	/**
	 * [out] Direction indexed array indicating the size of each individual
	 * lookup record pool expressed as: log2(max_records/max_pools). For
	 * example if 1024 records and 2 pools 1024/2=512=2^9, so the value 9
	 * would be entered.
	 */
	uint8_t lkup_pool_sz_exp[CFA_DIR_MAX];
	/**
	 * [out] Direction indexed array indicating the size of each individual
	 * action record pool expressed as: log2(max_records/max_pools). For
	 * example if 1024 records and 2 pools 1024/2=512=2^9, so the value 9
	 * would be entered.
	 */
	uint8_t act_pool_sz_exp[CFA_DIR_MAX];
	/**
	 * [out] Direction indexed array indicating the offset in records from
	 * the start of the memory after the static buckets where the first
	 * lrec pool begins.
	 */
	uint32_t lkup_rec_start_offset[CFA_DIR_MAX];
};

/**
 * tfc_tbl_scope_mem_alloc_parms contains the parameters for allocating memory
 * to be used by a table scope.
 */
struct tfc_tbl_scope_mem_alloc_parms {
	/** Scope is one of non-shared, shared-app or global.
	 * If a shared-app or global table scope, dynamic buckets are disabled.
	 * this combined with the multiplier affects the calculation for static
	 * buckets in this function.
	 */
	enum cfa_scope_type scope_type;
	/**
	 * [in] If a shared table scope, indicate whether this is the first
	 * if, the first, the table scope memory will be allocated.  Otherwise
	 * only the details of the configuration will be stored internally
	 * for use - i.e. act_rec_cnt/lkup_rec_cnt/lkup_rec_start_offset.
	 */
	bool first;
	/**
	 * [in] Direction indexed array indicating the EM static bucket count
	 * expressed as: log2(static_bucket_count). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be entered.
	 */
	uint8_t static_bucket_cnt_exp[CFA_DIR_MAX];
	/**
	 * [in] Direction indexed array indicating the EM dynamic bucket count.
	 */
	uint8_t dynamic_bucket_cnt[CFA_DIR_MAX];
	/**
	 * [in] The number of minimum sized lkup records per direction.	In this
	 * usage, records are the minimum lookup memory allocation unit in a
	 * table scope.	 This value is the total memory required for buckets and
	 * entries.
	 */
	uint32_t lkup_rec_cnt[CFA_DIR_MAX];
	/**
	 * [in] The number of minimum sized action records per direction.
	 * Similar to the lkup_rec_cnt, records are the minimum action memory
	 * allocation unit in a table scope.
	 */
	uint32_t act_rec_cnt[CFA_DIR_MAX];
	/**
	 * [in] The page size used for allocation. If running in the kernel
	 * driver, this may be as small as 1KB.	 For huge pages this may be more
	 * commonly 2MB.  Supported values include 4K, 8K, 64K, 2M, 8M and 1GB.
	 */
	uint32_t pbl_page_sz_in_bytes;
	/**
	 * [in] Indicates local application vs remote application table scope. A
	 * table scope can be created on a PF for it's own use or for use by
	 * other children.  These may or may not be shared table scopes.  Set
	 * local to false if calling API on behalf of a remote client VF.
	 * (alternatively, we could pass in the remote fid or the local fid).
	 */
	bool local;
	/**
	 * [in] The maximum number of pools supported.
	 */
	uint8_t max_pools;
	/**
	 * [in] Direction indexed array indicating the action table pool size
	 * expressed as: log2(act_pool_sz). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be entered.
	 */
	uint8_t act_pool_sz_exp[CFA_DIR_MAX];
	/**
	 * [in] Direction indexed array indicating the lookup table pool size
	 * expressed as: log2(lkup_pool_sz). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be entered.
	 */
	uint8_t lkup_pool_sz_exp[CFA_DIR_MAX];
	/**
	 * [in] Lookup table record start offset.  Offset in 32B records after
	 * the static buckets where the lookup records and dynamic bucket memory
	 * will begin.
	 */
	uint32_t lkup_rec_start_offset[CFA_DIR_MAX];
};


/**
 * tfc_tbl_scope_qcaps_parms contains the parameters for determining
 * the table scope capabilities
 */
struct tfc_tbl_scope_qcaps_parms {
	/**
	 * [out] if true, the device supports a table scope.
	 */
	bool tbl_scope_cap;
	/**
	 * [out] if true, the device supports a global table scope.
	 */
	bool global_cap;
	/**
	 * [out] if true, the device supports locked regions.
	 */
	bool locked_cap;
	/**
	 * [out] the maximum number of static buckets supported.
	 */
	uint8_t max_lkup_static_bucket_exp;
	/**
	 * [out] The maximum number of minimum sized lkup records supported.
	 */
	uint32_t max_lkup_rec_cnt;
	/**
	 * [out] The maximum number of  minimum sized action records supported.
	 */
	uint32_t max_act_rec_cnt;
};

/**
 * Determine whether table scopes are supported in the hardware.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in,out] parms
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_qcaps(struct tfc *tfcp,
			struct tfc_tbl_scope_qcaps_parms *parms);

/**
 * Determine table scope sizing
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in,out] parms
 *   The parameters used by this function.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_size_query(struct tfc *tfcp,
			     struct tfc_tbl_scope_size_query_parms *parms);

/**
 * Allocate a table scope
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] scope_type
 *   non-shared, shared-app or global
 *
 * @param[in] app_type
 *   The application type, TF or AFM
 *
 * @param[out] tsid
 *   The allocated table scope ID.
 *
 * @param[in,out] first
 *   True if the caller is the creator of this table scope.
 *   If not shared, first is always set. (optional)
 *
 * @returns
 *	 0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_id_alloc(struct tfc *tfcp, enum cfa_scope_type scope_type,
			   enum cfa_app_type app_type, uint8_t *tsid,
			   bool *first);

/**
 * Allocate memory for a table scope
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in] fid
 *   Function id requesting the memory allocation
 *
 * @param[in] parms
 *   Memory allocation parameters
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_mem_alloc(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			    struct tfc_tbl_scope_mem_alloc_parms *parms);

/**
 * Free memory for a table scope
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function id for memory to free from the table scope. Set to INVALID_FID
 *   by default. Populated when VF2PF mem_free message received from a VF
 *   for a shared table scope.
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in] fid_cnt
 *   Used for global scope cleanup.  If a fid remains, do not delete scope
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_mem_free(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			   uint16_t fid_cnt);

/**
 * tfc_tbl_scope_cpm_alloc_parms contains the parameters for allocating a
 * CPM instance to be used by a table scope.
 */
struct tfc_tbl_scope_cpm_alloc_parms {
	/**
	 * [in] Direction indexed array indicating the maximum number of lookup
	 * contiguous records.
	 */
	uint8_t lkup_max_contig_rec[CFA_DIR_MAX];
	/**
	 * [in] Direction indexed array indicating the maximum number of action
	 * contiguous records.
	 */
	uint8_t act_max_contig_rec[CFA_DIR_MAX];
	/**
	 * [in] The maximum number of pools supported by the table scope.
	 */
	uint16_t max_pools;
};
/**
 * Allocate CFA Pool Manager (CPM) Instance
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] cpm_parms
 *   Pointer to the CFA Pool Manager parameters
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_cpm_alloc(struct tfc *tfcp, uint8_t tsid,
			    struct tfc_tbl_scope_cpm_alloc_parms *cpm_parms);

/**
 * Free CPM Instance
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_cpm_free(struct tfc *tfcp, uint8_t tsid);

/**
 * Associate a forwarding function with an existing table scope
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function id to associated with the table scope
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in,out] fid_cnt
 *   The number of forwarding functions currently associated with the table scope
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_fid_add(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			  uint16_t *fid_cnt);

/**
 * Disassociate a forwarding function from an existing TFC table scope
 *
 * Once the last function has been removed from the session in the firmware
 * the session is freed and all associated resources freed.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function id to associated with the table scope
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in,out] fid_cnt
 *   The number of forwarding functions currently associated with the session
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_fid_rem(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			  uint16_t *fid_cnt);

/**
 * Pool allocation
 *
 * Allocate a pool ID and set it's size
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function id allocating the pool
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in] region
 *   Pool region identifier
 *
 * @param[in] dir
 *   Direction
 *
 * @param[out] pool_sz_exp
 *   Pool size exponent
 *
 * @param[out] pool_id
 *   Used to return the allocated pool ID.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_pool_alloc(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			     enum cfa_region_type region, enum cfa_dir dir,
			     uint8_t *pool_sz_exp, uint16_t *pool_id);

/**
 * Pool free
 *
 * Free a pool ID
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Function freeing the pool
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in] region
 *   Pool region identifier
 *
 * @param[in] dir
 *   Direction
 *
 * @param[in] pool_id
 *   Used to return the allocated pool ID.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_pool_free(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			    enum cfa_region_type region, enum cfa_dir dir,
			    uint16_t pool_id);

/**
 * Get configured state
 *
 * This API is intended for DPDK applications where a single table scope is shared
 * across one or more DPDK instances. When one instance succeeds to allocate and
 * configure a table scope, it then sets the config for that table scope; while
 * other sessions polling and waiting for the shared table scope to be configured.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[out] configured
 *   Used to return the allocated pool ID.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_config_state_get(struct tfc *tfcp, uint8_t tsid, bool *configured);

/**
 * Table scope function reset
 *
 * Delete resources and EM entries associated with fid.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   Table scope identifier
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_func_reset(struct tfc *tfcp, uint16_t fid);

/**
 * @page EM Exact Match
 *
 * @ref tfc_em_insert
 *
 * @ref tfc_em_delete
 */

/**
 * tfc_em_insert_parms contains the parameters for an EM insert.
 *
 */
struct tfc_em_insert_parms {
	/**
	 * [in] Entry direction.
	 */
	enum cfa_dir dir;
	/**
	 * [in] Pointer to the combined lkup record and key data to be written.
	 */
	uint8_t *lkup_key_data;
	/**
	 * [in] The size of the entry to write in 32b words.
	 */
	uint16_t lkup_key_sz_words;
	/**
	 * [in] Thor only - The key data to be used to calculate the hash.
	 */
	const uint8_t *key_data;
	/**
	 * [in] Thor only - Size of key in bits.
	 */
	uint16_t key_sz_bits;
	/**
	 * [out] Will contain the entry flow handle a unique identifier.
	 */
	uint64_t *flow_handle;
	/**
	 * [in/out] Batch mode data
	 */
	struct tfc_mpc_batch_info_t *batch_info;
};

/**
 * Start MPC batching
 *
 * @param[in/out] batch_info
 *   Contains batch processing info
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_mpc_batch_start(struct tfc_mpc_batch_info_t *batch_info);

/**
 * Ends MPC batching and returns the accumulated results
 *
 * @param[in/out] batch_info
 *   Contains batch processing info
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_mpc_batch_end(struct tfc *tfcp,
		      struct tfc_mpc_batch_info_t *batch_info);

/**
 * Checks to see if batching is active and other MPCs have been sent
 *
 * @param[in/out] batch_info
 *   Contains batch processing info
 *
 * @returns
 *   True is started and MPCs have been sent else False.
 */
bool tfc_mpc_batch_started(struct tfc_mpc_batch_info_t *batch_info);

/**
 * Insert an EM Entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope id
 *
 * @param[in,out] parms
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_em_insert(struct tfc *tfcp, uint8_t tsid,
		  struct tfc_em_insert_parms *parms);


/**
 * tfc_em_delete_parms Contains args required to delete an EM Entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] dir
 *   Direction (CFA_DIR_RX or CFA_DIR_TX)
 *
 * @param[in,out] flow_handle
 *   The flow handle returned to be used for flow deletion.
 *
 */
struct tfc_em_delete_parms {
	/**
	 * [in] Entry direction.
	 */
	enum cfa_dir dir;
	/**
	 * [in] Flow handle of flow to delete
	 */
	uint64_t flow_handle;
	/**
	 * [in/out] Batch mode data
	 */
	struct tfc_mpc_batch_info_t *batch_info;
};

/**
 * Delete an EM Entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in,out] parms
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_em_delete(struct tfc *tfcp, struct tfc_em_delete_parms *parms);


/**
 * @page ACTCFA Action CFA Memory Management
 *
 * @ref tfc_act_alloc
 *
 * @ref tfc_act_set
 *
 * @ref tfc_act_get
 *
 * @ref tfc_act_free
 */
/**
 * CMM resource structure
 */
struct tfc_cmm_info {
	enum cfa_resource_subtype_cmm rsubtype; /**< resource subtype */
	enum cfa_dir dir; /**< direction rx/tx */
	uint64_t act_handle; /**< alloc/free handle */
};

/**
 * CMM resource clear structure
 */
struct tfc_cmm_clr {
	bool clr; /**< flag for clear */
	uint16_t offset_in_byte; /**< field offset in byte */
	uint16_t sz_in_byte; /**<  field size in byte */
};

/**
 * Allocate an action CMM Resource
 *
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in,out] cmm_info
 *   Pointer to cmm info
 *
 * @param[in] num_contig_rec
 *   Num contiguous records required.  Record size is 8B for Thor/32B for Thor2.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_act_alloc(struct tfc *tfcp, uint8_t tsid,
		  struct tfc_cmm_info *cmm_info, uint16_t num_contig_rec);

/**
 * Set an action CMM resource
 *
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in/out] batch_info
 *   Batch mode data
 *
 * @param[in] cmm_info
 *   Pointer to cmm info.
 *
 * @param[in] data
 *   Data to be written.
 *
 * @param[in] data_sz_words
 *   Data buffer size in words. In 8B increments.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_act_set(struct tfc *tfcp,
		struct tfc_mpc_batch_info_t *batch_info,
		const struct tfc_cmm_info *cmm_info,
		const uint8_t *data, uint16_t data_sz_words);

/**
 * Get an action CMM resource
 *
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in/out] batch_info
 *   Batch mode data
 *
 * @param[in] cmm_info
 *   Pointer to cmm info
 *
 * @param[in] cmm_clr
 *   Pointer to cmm clr
 *
 * @param[in,out] host_address
 *   Data read. Must be word aligned, i.e. [1:0] must be 0. The address
 *   must be the ret_mem_virt2iova() version of the virt address.
 *
 * @param[in,out] data_sz_words
 *   Data buffer size in words.	Size could be 8/16/24/32/64B
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_act_get(struct tfc *tfcp,
		struct tfc_mpc_batch_info_t *batch_info,
		const struct tfc_cmm_info *cmm_info,
		struct tfc_cmm_clr *clr,
		uint64_t *host_address,
		uint16_t *data_sz_words);

/**
 * Free a CMM Resource
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] cmm_info
 *   CMM info
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_act_free(struct tfc *tfcp,
		 const struct tfc_cmm_info *cmm_info);

/**
 * @page IF Table
 *
 * @ref tfc_if_tbl_set
 *
 * @ref tfc_if_tbl_get
 */

/**
 * IF table resource structure
 */
struct tfc_if_tbl_info {
	enum cfa_resource_subtype_if_tbl rsubtype; /**< resource subtype */
	enum cfa_dir dir; /**< direction rx/tx */
	uint16_t id; /**< index */
};

/**
 * Set a TFC if table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   including the id.
 *
 * @param[in] data
 *   Pointer to the data to write to the entry.	The data is aligned correctly
 *   in the buffer for writing to the hardware.
 *
 * @param[in] data_sz_in_bytes
 *   The size of the entry in device sized bytes for Thor2.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_if_tbl_set(struct tfc *tfcp, uint16_t fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   const uint8_t *data, uint8_t data_sz_in_bytes);

/**
 * Get a TFC if table entry
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] tbl_info
 *   All the information related to the requested index table entry (subtype/dir)
 *   including the id.
 *
 * @param[in, out] data
 *   Pointer to the data to read from the entry.
 *
 * @param[in,out] data_sz_in_bytes
 *   The size of the entry in device sized bytes for Thor2. Input is the
 *   size of the buffer, output is the actual size.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_if_tbl_get(struct tfc *tfcp, uint16_t fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   uint8_t *data, uint8_t *data_sz_in_bytes);

/**
 * Get a TFC hot upgrade status and application instance count
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] app_inst_id
 *   the applicatoin instance id.
 *
 * @param[out] app_inst_cnt
 *   Pointer to the application instance count.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int
tfc_hot_up_app_inst_count(struct tfc *tfcp, uint16_t fid,
			  uint8_t app_inst_id, uint8_t *app_inst_cnt);

/**
 * Allocate and initialize the TFC hot upgrade state
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] app_inst_id
 *   the application instance id.
 *
 * @param[in] app_inst_cnt
 *   the application instance count.
 *
 * @param[out] session
 *   the session count.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int
tfc_hot_up_app_inst_alloc(struct tfc *tfcp, uint16_t fid,
			  uint8_t app_inst_id, uint8_t app_inst_cnt,
			  uint8_t *session);
/**
 * Free the TFC hot upgrade state
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] app_inst_id
 *   the application instance id.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int
tfc_hot_up_app_inst_free(struct tfc *tfcp, uint16_t fid, uint8_t app_inst_id);

/**
 * Set the TFC hot upgrade state to primary
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   FID - Function ID to be used
 *
 * @param[in] app_inst_id
 *   the application instance id.
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int
tfc_hot_up_app_inst_set(struct tfc *tfcp, uint16_t fid, uint8_t app_inst_id);

#endif /* _TFC_H_ */
