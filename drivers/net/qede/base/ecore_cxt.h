/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef _ECORE_CID_
#define _ECORE_CID_

#include "ecore_hsi_common.h"
#include "ecore_proto_if.h"
#include "ecore_cxt_api.h"

/* Tasks segments definitions  */
#define ECORE_CXT_ISCSI_TID_SEG			PROTOCOLID_ISCSI	/* 0 */
#define ECORE_CXT_FCOE_TID_SEG			PROTOCOLID_FCOE		/* 1 */
#define ECORE_CXT_ROCE_TID_SEG			PROTOCOLID_ROCE		/* 2 */

enum ecore_cxt_elem_type {
	ECORE_ELEM_CXT,
	ECORE_ELEM_SRQ,
	ECORE_ELEM_TASK
};

enum ilt_clients {
	ILT_CLI_CDUC,
	ILT_CLI_CDUT,
	ILT_CLI_QM,
	ILT_CLI_TM,
	ILT_CLI_SRC,
	ILT_CLI_TSDM,
	ILT_CLI_RGFS,
	ILT_CLI_TGFS,
	MAX_ILT_CLIENTS
};

u32 ecore_cxt_get_proto_cid_count(struct ecore_hwfn *p_hwfn,
				  enum protocol_type type,
				  u32 *vf_cid);

u32 ecore_cxt_get_proto_tid_count(struct ecore_hwfn *p_hwfn,
				  enum protocol_type type);

u32 ecore_cxt_get_proto_cid_start(struct ecore_hwfn *p_hwfn,
				  enum protocol_type type);
u32 ecore_cxt_get_srq_count(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_cxt_set_pf_params - Set the PF params for cxt init
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_cxt_set_pf_params(struct ecore_hwfn *p_hwfn);

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
 * @param p_ptt
 */
void ecore_cxt_hw_init_pf(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt);

/**
 * @brief ecore_qm_init_pf - Initailze the QM PF phase, per path
 *
 * @param p_hwfn
 * @param p_ptt
 * @param is_pf_loading
 */
void ecore_qm_init_pf(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt,
		      bool is_pf_loading);

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

#define ECORE_CXT_PF_CID (0xff)

/**
 * @brief ecore_cxt_release - Release a cid
 *
 * @param p_hwfn
 * @param cid
 */
void ecore_cxt_release_cid(struct ecore_hwfn *p_hwfn, u32 cid);

/**
 * @brief ecore_cxt_release - Release a cid belonging to a vf-queue
 *
 * @param p_hwfn
 * @param cid
 * @param vfid - engine relative index. ECORE_CXT_PF_CID if belongs to PF
 */
void _ecore_cxt_release_cid(struct ecore_hwfn *p_hwfn,
			    u32 cid, u8 vfid);

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
 * @brief _ecore_cxt_acquire - Acquire a new cid of a specific protocol type
 *                             for a vf-queue
 *
 * @param p_hwfn
 * @param type
 * @param p_cid
 * @param vfid - engine relative index. ECORE_CXT_PF_CID if belongs to PF
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t _ecore_cxt_acquire_cid(struct ecore_hwfn *p_hwfn,
					    enum protocol_type type,
					    u32 *p_cid, u8 vfid);

/**
 * @brief ecore_cxt_get_tid_mem_info - function checks if the
 *        page containing the iid in the ilt is already
 *        allocated, if it is not it allocates the page.
 *
 * @param p_hwfn
 * @param elem_type
 * @param iid
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_cxt_dynamic_ilt_alloc(struct ecore_hwfn *p_hwfn,
			    enum ecore_cxt_elem_type elem_type,
			    u32 iid);

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

/* Max number of connection types in HW (DQ/CDU etc.) */
#define MAX_CONN_TYPES		PROTOCOLID_COMMON
#define NUM_TASK_TYPES		2
#define NUM_TASK_PF_SEGMENTS	4
#define NUM_TASK_VF_SEGMENTS	1

/* PF per protocol configuration object */
#define TASK_SEGMENTS   (NUM_TASK_PF_SEGMENTS + NUM_TASK_VF_SEGMENTS)
#define TASK_SEGMENT_VF (NUM_TASK_PF_SEGMENTS)

struct ecore_tid_seg {
	u32 count;
	u8 type;
	bool has_fl_mem;
};

struct ecore_conn_type_cfg {
	u32 cid_count;
	u32 cids_per_vf;
	struct ecore_tid_seg tid_seg[TASK_SEGMENTS];
};

/* ILT Client configuration,
 * Per connection type (protocol) resources (cids, tis, vf cids etc.)
 * 1 - for connection context (CDUC) and for each task context we need two
 * values, for regular task context and for force load memory
 */
#define ILT_CLI_PF_BLOCKS	(1 + NUM_TASK_PF_SEGMENTS * 2)
#define ILT_CLI_VF_BLOCKS	(1 + NUM_TASK_VF_SEGMENTS * 2)
#define CDUC_BLK		(0)
#define SRQ_BLK			(0)
#define CDUT_SEG_BLK(n)		(1 + (u8)(n))
#define CDUT_FL_SEG_BLK(n, X)	(1 + (n) + NUM_TASK_##X##_SEGMENTS)

struct ilt_cfg_pair {
	u32 reg;
	u32 val;
};

struct ecore_ilt_cli_blk {
	u32 total_size;		/* 0 means not active */
	u32 real_size_in_page;
	u32 start_line;
	u32 dynamic_line_offset;
	u32 dynamic_line_cnt;
};

struct ecore_ilt_client_cfg {
	bool active;

	/* ILT boundaries */
	struct ilt_cfg_pair first;
	struct ilt_cfg_pair last;
	struct ilt_cfg_pair p_size;

	/* ILT client blocks for PF */
	struct ecore_ilt_cli_blk pf_blks[ILT_CLI_PF_BLOCKS];
	u32 pf_total_lines;

	/* ILT client blocks for VFs */
	struct ecore_ilt_cli_blk vf_blks[ILT_CLI_VF_BLOCKS];
	u32 vf_total_lines;
};

#define MAP_WORD_SIZE		sizeof(unsigned long)
#define BITS_PER_MAP_WORD	(MAP_WORD_SIZE * 8)

struct ecore_cid_acquired_map {
	u32 start_cid;
	u32 max_count;
	u32 *cid_map;
};

struct ecore_src_t2 {
	struct phys_mem_desc	*dma_mem;
	u32			num_pages;
	u64			first_free;
	u64			last_free;
};

struct ecore_cxt_mngr {
	/* Per protocol configuration */
	struct ecore_conn_type_cfg	conn_cfg[MAX_CONN_TYPES];

	/* computed ILT structure */
	struct ecore_ilt_client_cfg	clients[MAX_ILT_CLIENTS];

	/* Task type sizes */
	u32				task_type_size[NUM_TASK_TYPES];

	/* total number of VFs for this hwfn -
	 * ALL VFs are symmetric in terms of HW resources
	 */
	u32				vf_count;
	u32				first_vf_in_pf;

	/* Acquired CIDs */
	struct ecore_cid_acquired_map acquired[MAX_CONN_TYPES];
	struct ecore_cid_acquired_map *acquired_vf[MAX_CONN_TYPES];

	/* ILT  shadow table */
	struct phys_mem_desc		*ilt_shadow;
	u32				ilt_shadow_size;
	u32				pf_start_line;

	/* Mutex for a dynamic ILT allocation */
	osal_mutex_t mutex;

	/* SRC T2 */
	struct ecore_src_t2		src_t2;

	/* The infrastructure originally was very generic and context/task
	 * oriented - per connection-type we would set how many of those
	 * are needed, and later when determining how much memory we're
	 * needing for a given block we'd iterate over all the relevant
	 * connection-types.
	 * But since then we've had some additional resources, some of which
	 * require memory which is independent of the general context/task
	 * scheme. We add those here explicitly per-feature.
	 */

	/* total number of SRQ's for this hwfn */
	u32				srq_count;

	/* Maximal number of L2 steering filters */
	u32				arfs_count;

	/* TODO - VF arfs filters ? */

	u8				task_type_id;
	u16				task_ctx_size;
	u16				conn_ctx_size;
};

u16 ecore_get_cdut_num_pf_init_pages(struct ecore_hwfn *p_hwfn);
u16 ecore_get_cdut_num_vf_init_pages(struct ecore_hwfn *p_hwfn);
u16 ecore_get_cdut_num_pf_work_pages(struct ecore_hwfn *p_hwfn);
u16 ecore_get_cdut_num_vf_work_pages(struct ecore_hwfn *p_hwfn);
#endif /* _ECORE_CID_ */
