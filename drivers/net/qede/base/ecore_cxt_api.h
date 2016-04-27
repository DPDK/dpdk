/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_CXT_API_H__
#define __ECORE_CXT_API_H__

struct ecore_hwfn;

struct ecore_cxt_info {
	void *p_cxt;
	u32 iid;
	enum protocol_type type;
};

#define MAX_TID_BLOCKS			512
struct ecore_tid_mem {
	u32 tid_size;
	u32 num_tids_per_block;
	u32 waste;
	u8 *blocks[MAX_TID_BLOCKS];	/* 4K */
};

static OSAL_INLINE void *get_task_mem(struct ecore_tid_mem *info, u32 tid)
{
	/* note: waste is superfluous */
	return (void *)(info->blocks[tid / info->num_tids_per_block] +
			(tid % info->num_tids_per_block) * info->tid_size);

	/* more elaborate alternative with no modulo
	 * u32 mask = info->tid_size * info->num_tids_per_block +
	 *            info->waste - 1;
	 * u32 index = tid / info->num_tids_per_block;
	 * u32 offset = tid * info->tid_size + index * info->waste;
	 * return (void *)(blocks[index] + (offset & mask));
	 */
}

/**
* @brief ecore_cxt_acquire - Acquire a new cid of a specific protocol type
*
* @param p_hwfn
* @param type
* @param p_cid
*
* @return enum _ecore_status_t
*/
enum _ecore_status_t ecore_cxt_acquire_cid(struct ecore_hwfn *p_hwfn,
					   enum protocol_type type,
					   u32 *p_cid);

/**
* @brief ecoreo_cid_get_cxt_info - Returns the context info for a specific cid
*
*
* @param p_hwfn
* @param p_info in/out
*
* @return enum _ecore_status_t
*/
enum _ecore_status_t ecore_cxt_get_cid_info(struct ecore_hwfn *p_hwfn,
					    struct ecore_cxt_info *p_info);

/**
* @brief ecore_cxt_get_tid_mem_info
*
* @param p_hwfn
* @param p_info
*
* @return enum _ecore_status_t
*/
enum _ecore_status_t ecore_cxt_get_tid_mem_info(struct ecore_hwfn *p_hwfn,
						struct ecore_tid_mem *p_info);

#endif
