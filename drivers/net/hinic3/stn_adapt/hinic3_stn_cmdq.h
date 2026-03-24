/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_STN_CMDQ_H_
#define _HINIC3_STN_CMDQ_H_

#include "hinic3_nic_io.h"

struct hinic3_qp_ctxt_header {
	uint16_t num_queues;
	uint16_t queue_type;
	uint16_t start_qid;
	uint16_t rsvd;
};

struct hinic3_clean_queue_ctxt {
	struct hinic3_qp_ctxt_header cmdq_hdr;
	uint32_t rsvd;
};

struct hinic3_qp_ctxt_block {
	struct hinic3_qp_ctxt_header   cmdq_hdr;
	union {
		struct hinic3_sq_ctxt  sq_ctxt[HINIC3_Q_CTXT_MAX];
		struct hinic3_rq_ctxt  rq_ctxt[HINIC3_Q_CTXT_MAX];
	};
};

struct hinic3_vlan_ctx {
	uint32_t func_id;
	uint32_t qid; /* if qid = 0xFFFF, config for all queues */
	uint32_t vlan_id;
	uint32_t vlan_mode;
	uint32_t vlan_sel;
};

/**
 * Get cmdq ops software tile NIC(stn) supported.
 *
 * @return
 * Pointer to ops.
 */
struct hinic3_nic_cmdq_ops *hinic3_nic_cmdq_get_stn_ops(void);

#endif /* _HINIC3_STN_CMDQ_H_ */
