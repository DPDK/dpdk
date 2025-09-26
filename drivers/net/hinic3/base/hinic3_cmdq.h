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

enum cmdq_scmd_type { CMDQ_SET_ARM_CMD = 2 };

enum cmdq_wqe_type { WQE_LCMD_TYPE = 0, WQE_SCMD_TYPE = 1 };

enum ctrl_sect_len { CTRL_SECT_LEN = 1, CTRL_DIRECT_SECT_LEN = 2 };

enum bufdesc_len { BUFDESC_LCMD_LEN = 2, BUFDESC_SCMD_LEN = 3 };

enum data_format { DATA_SGE = 0};

enum completion_format { COMPLETE_DIRECT = 0, COMPLETE_SGE = 1 };

enum completion_request { CEQ_SET = 1 };

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
	HINIC3_ACK_TYPE_CMDQ		= 0,
	HINIC3_ACK_TYPE_SHARE_CQN	= 1,
	HINIC3_ACK_TYPE_APP_CQN		= 2,

	HINIC3_MOD_ACK_MAX		= 15
};

/* Cmdq wqe ctrls. */
struct hinic3_cmdq_header {
	uint32_t header_info;
	uint32_t saved_data;
};

struct hinic3_scmd_bufdesc {
	uint32_t buf_len;
	uint32_t rsvd;
	uint8_t data[HINIC3_SCMD_DATA_LEN];
};

struct hinic3_lcmd_bufdesc {
	struct hinic3_sge sge;
	uint32_t rsvd1;
	uint64_t saved_async_buf;
	uint64_t rsvd3;
};

struct hinic3_cmdq_db {
	uint32_t db_head;
	uint32_t db_info;
};

struct hinic3_status {
	uint32_t status_info;
};

struct hinic3_ctrl {
	uint32_t ctrl_info;
};

struct hinic3_sge_resp {
	struct hinic3_sge sge;
	uint32_t rsvd;
};

struct hinic3_cmdq_completion {
	/* HW format. */
	union {
		struct hinic3_sge_resp sge_resp;
		uint64_t direct_resp;
	};
};

struct hinic3_cmdq_wqe_scmd {
	struct hinic3_cmdq_header header;
	uint64_t rsvd;
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
	uint64_t curr_wqe_page_pfn;
	uint64_t wq_block_pfn;
};

struct hinic3_cmd_cmdq_ctxt {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_idx;
	uint8_t cmdq_id;
	uint8_t rsvd1[5];

	struct hinic3_cmdq_ctxt_info ctxt_info;
};

enum hinic3_cmdq_status {
	HINIC3_CMDQ_ENABLE = RTE_BIT32(0),
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
	uint8_t *db_base;

	rte_spinlock_t cmdq_lock;

	struct hinic3_cmdq_ctxt_info cmdq_ctxt;

	struct hinic3_cmdq_cmd_info *cmd_infos;
};

struct hinic3_cmdqs {
	struct hinic3_hwdev *hwdev;
	uint8_t *cmdqs_db_base;

	struct rte_mempool *cmd_buf_pool;

	struct hinic3_wq *saved_wqs;

	struct hinic3_cmdq cmdq[HINIC3_MAX_CMDQ_TYPES];

	uint32_t status;
};

struct hinic3_cmd_buf {
	void *buf;
	uint64_t dma_addr;
	struct rte_mbuf *mbuf;
	uint16_t size;
};

int hinic3_reinit_cmdq_ctxts(struct hinic3_hwdev *hwdev);

bool hinic3_cmdq_idle(struct hinic3_cmdq *cmdq);

struct hinic3_cmd_buf *hinic3_alloc_cmd_buf(struct hinic3_hwdev *hwdev);

void hinic3_free_cmd_buf(struct hinic3_cmd_buf *cmd_buf);

/*
 * PF/VF sends cmd to ucode by cmdq, and return 0 if success.
 * timeout=0, use default timeout.
 */
int hinic3_cmdq_direct_resp(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod, uint8_t cmd,
			    struct hinic3_cmd_buf *buf_in, uint64_t *out_param, uint32_t timeout);

int hinic3_cmdq_detail_resp(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod, uint8_t cmd,
		 struct hinic3_cmd_buf *buf_in, struct hinic3_cmd_buf *buf_out, uint32_t timeout);

int hinic3_init_cmdqs(struct hinic3_hwdev *hwdev);

void hinic3_free_cmdqs(struct hinic3_hwdev *hwdev);

#endif /* _HINIC3_CMDQ_H_ */
