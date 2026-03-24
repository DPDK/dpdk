/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_HTN_CMDQ_H_
#define _HINIC3_HTN_CMDQ_H_

#include "hinic3_nic_io.h"

struct hinic3_htn_qp_ctxt_header {
	uint32_t rsvd[2];
	uint16_t num_queues;
	uint16_t queue_type;
	uint16_t start_qid;
	uint16_t dest_func_id;
};

struct hinic3_htn_clean_queue_ctxt {
	struct hinic3_htn_qp_ctxt_header cmdq_hdr;
};

struct hinic3_htn_qp_ctxt_block {
	struct hinic3_htn_qp_ctxt_header cmdq_hdr;
	union {
		struct hinic3_sq_ctxt  sq_ctxt[HINIC3_Q_CTXT_MAX];
		struct hinic3_rq_ctxt  rq_ctxt[HINIC3_Q_CTXT_MAX];
	};
};

struct hinic3_rss_cmd_header {
	uint32_t rsv[3];
	uint16_t rsv1;
	uint16_t dest_func_id;
};

/* NIC HTN CMD */
enum hinic3_htn_cmd {
	HINIC3_HTN_CMD_SQ_RQ_CONTEXT_MULTI_ST = 0x20,
	HINIC3_HTN_CMD_SQ_RQ_CONTEXT_MULTI_LD,
	HINIC3_HTN_CMD_TSO_LRO_SPACE_CLEAN,
	HINIC3_HTN_CMD_SVLAN_MODIFY,
	HINIC3_HTN_CMD_SET_RSS_INDIR_TABLE,
	HINIC3_HTN_CMD_GET_RSS_INDIR_TABLE
};

struct hinic3_htn_vlan_ctx {
	uint32_t rsv[2];
	uint16_t vlan_tag;
	uint8_t vlan_sel;
	uint8_t vlan_mode;
	uint16_t start_qid;
	uint16_t dest_func_id;
};

/**
 * Get cmdq ops hardware tile NIC(htn) supported.
 *
 * @return
 * Pointer to ops.
 */
struct hinic3_nic_cmdq_ops *hinic3_nic_cmdq_get_htn_ops(void);

#endif /* _HINIC3_HTN_CMDQ_H_ */
