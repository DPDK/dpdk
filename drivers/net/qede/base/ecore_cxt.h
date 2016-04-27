/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef _ECORE_CID_
#define _ECORE_CID_

#include "ecore_hsi_common.h"
#include "ecore_proto_if.h"
#include "ecore_cxt_api.h"

enum ecore_cxt_elem_type {
	ECORE_ELEM_CXT,
	ECORE_ELEM_TASK
};

u32 ecore_cxt_get_proto_cid_count(struct ecore_hwfn *p_hwfn,
				  enum protocol_type type, u32 *vf_cid);

u32 ecore_cxt_get_proto_cid_start(struct ecore_hwfn *p_hwfn,
				  enum protocol_type type);

/**
 * @brief ecore_cxt_qm_iids - fills the cid/tid counts for the QM configuration
 *
 * @param p_hwfn
 * @param iids [out], a structure holding all the counters
 */
void ecore_cxt_qm_iids(struct ecore_hwfn *p_hwfn, struct ecore_qm_iids *iids);

/**
 * @brief ecore_cxt_set_pf_params - Set the PF params for cxt init
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_cxt_set_pf_params(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_set_proto_cid_count - Set the max cids per protocol for cxt
 *        init
 *
 * @param p_hwfn
 * @param type
 * @param cid_cnt - number of pf cids
 * @param vf_cid_cnt - number of vf cids
 */
void ecore_cxt_set_proto_cid_count(struct ecore_hwfn *p_hwfn,
				   enum protocol_type type,
				   u32 cid_cnt, u32 vf_cid_cnt);
/**
 * @brief ecore_cxt_cfg_ilt_compute - compute ILT init parameters
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_cxt_cfg_ilt_compute(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_mngr_alloc - Allocate and init the context manager struct
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_cxt_mngr_alloc(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_mngr_free
 *
 * @param p_hwfn
 */
void ecore_cxt_mngr_free(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_tables_alloc - Allocate ILT shadow, Searcher T2, acquired
 *        map
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_cxt_tables_alloc(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_mngr_setup - Reset the acquired CIDs
 *
 * @param p_hwfn
 */
void ecore_cxt_mngr_setup(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_hw_init_common - Initailze ILT and DQ, common phase, per
 *        path.
 *
 * @param p_hwfn
 */
void ecore_cxt_hw_init_common(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_hw_init_pf - Initailze ILT and DQ, PF phase, per path.
 *
 * @param p_hwfn
 */
void ecore_cxt_hw_init_pf(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_qm_init_pf - Initailze the QM PF phase, per path
 *
 * @param p_hwfn
 */
void ecore_qm_init_pf(struct ecore_hwfn *p_hwfn);

 /**
 * @brief Reconfigures QM pf on the fly
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_qm_reconf(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt);

/**
* @brief ecore_cxt_release - Release a cid
*
* @param p_hwfn
* @param cid
*/
void ecore_cxt_release_cid(struct ecore_hwfn *p_hwfn, u32 cid);

/**
 * @brief ecore_cxt_free_proto_ilt - function frees ilt pages
 *        associated with the protocol passed.
 *
 * @param p_hwfn
 * @param proto
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_cxt_free_proto_ilt(struct ecore_hwfn *p_hwfn,
					      enum protocol_type proto);

#define ECORE_CTX_WORKING_MEM 0
#define ECORE_CTX_FL_MEM 1
enum _ecore_status_t ecore_cxt_get_task_ctx(struct ecore_hwfn *p_hwfn,
					    u32 tid,
					    u8 ctx_type, void **task_ctx);

#endif /* _ECORE_CID_ */
