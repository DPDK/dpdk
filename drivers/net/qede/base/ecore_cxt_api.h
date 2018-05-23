/*
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_CXT_API_H__
#define __ECORE_CXT_API_H__

struct ecore_hwfn;

struct ecore_cxt_info {
	void			*p_cxt;
	u32			iid;
	enum protocol_type	type;
};

#define MAX_TID_BLOCKS			512
struct ecore_tid_mem {
	u32 tid_size;
	u32 num_tids_per_block;
	u32 waste;
	u8 *blocks[MAX_TID_BLOCKS]; /* 4K */
};

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

#endif
