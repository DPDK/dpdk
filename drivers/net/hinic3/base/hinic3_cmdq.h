/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_CMDQ_H_
#define _HINIC3_CMDQ_H_

#include "hinic3_mgmt.h"
#include "hinic3_wq.h"

#define HINIC3_SCMD_DATA_LEN 16

/* Pmd driver uses 64, kernel l2nic uses 4096. */
#define HINIC3_CMDQ_DEPTH 64

#define HINIC3_CMDQ_BUF_SIZE 2048U

#define HINIC3_CEQ_ID_CMDQ 0

enum cmdq_scmd_type {
	CMDQ_SET_ARM_CMD = 2,
};

enum cmdq_wqe_type { WQE_LCMD_TYPE, WQE_SCMD_TYPE };

enum ctrl_sect_len { CTRL_SECT_LEN = 1, CTRL_DIRECT_SECT_LEN = 2 };

enum bufdesc_len { BUFDESC_LCMD_LEN = 2, BUFDESC_SCMD_LEN = 3 };

enum data_format {
	DATA_SGE,
};

enum completion_format { COMPLETE_DIRECT, COMPLETE_SGE };

enum completion_request {
	CEQ_SET = 1,
};

enum cmdq_cmd_type { SYNC_CMD_DIRECT_RESP, SYNC_CMD_SGE_RESP, ASYNC_CMD };

enum hinic3_cmdq_type {
	HINIC3_CMDQ_SYNC,
	HINIC3_CMDQ_ASYNC,
	HINIC3_MAX_CMDQ_TYPES
};

enum hinic3_db_src_type {
	HINIC3_DB_SRC_CMDQ_TYPE,
	HINIC3_DB_SRC_L2NIC_SQ_TYPE
};

enum hinic3_cmdq_db_type { HINIC3_DB_SQ_RQ_TYPE, HINIC3_DB_CMDQ_TYPE };

/* Cmdq ack type. */
enum hinic3_ack_type {
	HINIC3_ACK_TYPE_CMDQ,
	HINIC3_ACK_TYPE_SHARE_CQN,
	HINIC3_ACK_TYPE_APP_CQN,

	HINIC3_MOD_ACK_MAX = 15
};

/* Cmdq wqe ctrls. */
struct hinic3_cmdq_header {
	u32 header_info;
	u32 saved_data;
};

struct hinic3_scmd_bufdesc {
	u32 buf_len;
	u32 rsvd;
	u8 data[HINIC3_SCMD_DATA_LEN];
};

struct hinic3_lcmd_bufdesc {
	struct hinic3_sge sge;
	u32 rsvd1;
	u64 saved_async_buf;
	u64 rsvd3;
};

struct hinic3_cmdq_db {
	u32 db_head;
	u32 db_info;
};

struct hinic3_status {
	u32 status_info;
};

struct hinic3_ctrl {
	u32 ctrl_info;
};

struct hinic3_sge_resp {
	struct hinic3_sge sge;
	u32 rsvd;
};

struct hinic3_cmdq_completion {
	/* HW format. */
	union {
		struct hinic3_sge_resp sge_resp;
		u64 direct_resp;
	};
};

struct hinic3_cmdq_wqe_scmd {
	struct hinic3_cmdq_header header;
	u64 rsvd;
	struct hinic3_status status;
	struct hinic3_ctrl ctrl;
	struct hinic3_cmdq_completion completion;
	struct hinic3_scmd_bufdesc buf_desc;
};

struct hinic3_cmdq_wqe_lcmd {
	struct hinic3_cmdq_header header;
	struct hinic3_status status;
	struct hinic3_ctrl ctrl;
	struct hinic3_cmdq_completion completion;
	struct hinic3_lcmd_bufdesc buf_desc;
};

struct hinic3_cmdq_inline_wqe {
	struct hinic3_cmdq_wqe_scmd wqe_scmd;
};

struct hinic3_cmdq_wqe {
	/* HW format. */
	union {
		struct hinic3_cmdq_inline_wqe inline_wqe;
		struct hinic3_cmdq_wqe_lcmd wqe_lcmd;
	};
};

struct hinic3_cmdq_ctxt_info {
	u64 curr_wqe_page_pfn;
	u64 wq_block_pfn;
};

struct hinic3_cmd_cmdq_ctxt {
	u8 status;
	u8 version;
	u8 rsvd0[6];

	u16 func_idx;
	u8 cmdq_id;
	u8 rsvd1[5];

	struct hinic3_cmdq_ctxt_info ctxt_info;
};

enum hinic3_cmdq_status {
	HINIC3_CMDQ_ENABLE = BIT(0),
};

enum hinic3_cmdq_cmd_type {
	HINIC3_CMD_TYPE_NONE,
	HINIC3_CMD_TYPE_SET_ARM,
	HINIC3_CMD_TYPE_DIRECT_RESP,
	HINIC3_CMD_TYPE_SGE_RESP
};

struct hinic3_cmdq_cmd_info {
	enum hinic3_cmdq_cmd_type cmd_type;
};

struct hinic3_cmdq {
	struct hinic3_wq *wq;

	enum hinic3_cmdq_type cmdq_type;
	int wrapped;

	int *errcode;
	u8 *db_base;

	rte_spinlock_t cmdq_lock;

	struct hinic3_cmdq_ctxt_info cmdq_ctxt;

	struct hinic3_cmdq_cmd_info *cmd_infos;
};

struct hinic3_cmdqs {
	struct hinic3_hwdev *hwdev;
	u8 *cmdqs_db_base;

	struct rte_mempool *cmd_buf_pool;

	struct hinic3_wq *saved_wqs;

	struct hinic3_cmdq cmdq[HINIC3_MAX_CMDQ_TYPES];

	u32 status;
};

struct hinic3_cmd_buf {
	void *buf;
	uint64_t dma_addr;
	struct rte_mbuf *mbuf;
	u16 size;
};

int hinic3_reinit_cmdq_ctxts(struct hinic3_hwdev *hwdev);

bool hinic3_cmdq_idle(struct hinic3_cmdq *cmdq);

struct hinic3_cmd_buf *hinic3_alloc_cmd_buf(void *hwdev);

void hinic3_free_cmd_buf(struct hinic3_cmd_buf *cmd_buf);

/*
 * PF/VF sends cmd to ucode by cmdq, and return 0 if success.
 * timeout=0, use default timeout.
 */
int hinic3_cmdq_direct_resp(void *hwdev, enum hinic3_mod_type mod, u8 cmd,
			    struct hinic3_cmd_buf *buf_in, u64 *out_param,
			    u32 timeout);

int hinic3_cmdq_detail_resp(void *hwdev, enum hinic3_mod_type mod, u8 cmd,
			    struct hinic3_cmd_buf *buf_in,
			    struct hinic3_cmd_buf *buf_out, u32 timeout);

int hinic3_cmdqs_init(struct hinic3_hwdev *hwdev);

void hinic3_cmdqs_free(struct hinic3_hwdev *hwdev);

#endif /* _HINIC3_CMDQ_H_ */
