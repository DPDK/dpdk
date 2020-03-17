/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_CMDQ_H_
#define _HINIC_PMD_CMDQ_H_

#define HINIC_DB_OFF			0x00000800

#define HINIC_SCMD_DATA_LEN		16

/* hiovs pmd use 64, kernel l2nic use 4096 */
#define	HINIC_CMDQ_DEPTH		64

#define	HINIC_CMDQ_BUF_SIZE		2048U
#define HINIC_CMDQ_BUF_HW_RSVD		8
#define HINIC_CMDQ_MAX_DATA_SIZE	(HINIC_CMDQ_BUF_SIZE	\
					 - HINIC_CMDQ_BUF_HW_RSVD)

#define	HINIC_CEQ_ID_CMDQ		0

enum cmdq_scmd_type {
	CMDQ_SET_ARM_CMD = 2,
};

enum cmdq_wqe_type {
	WQE_LCMD_TYPE,
	WQE_SCMD_TYPE,
};

enum ctrl_sect_len {
	CTRL_SECT_LEN = 1,
	CTRL_DIRECT_SECT_LEN = 2,
};

enum bufdesc_len {
	BUFDESC_LCMD_LEN = 2,
	BUFDESC_SCMD_LEN = 3,
};

enum data_format {
	DATA_SGE,
};

enum completion_format {
	COMPLETE_DIRECT,
	COMPLETE_SGE,
};

enum completion_request {
	CEQ_SET = 1,
};

enum cmdq_cmd_type {
	SYNC_CMD_DIRECT_RESP,
	SYNC_CMD_SGE_RESP,
	ASYNC_CMD,
};

enum hinic_cmdq_type {
	HINIC_CMDQ_SYNC,
	HINIC_CMDQ_ASYNC,
	HINIC_MAX_CMDQ_TYPES,
};

enum hinic_db_src_type {
	HINIC_DB_SRC_CMDQ_TYPE,
	HINIC_DB_SRC_L2NIC_SQ_TYPE,
};

enum hinic_cmdq_db_type {
	HINIC_DB_SQ_RQ_TYPE,
	HINIC_DB_CMDQ_TYPE,
};

/* CMDQ WQE CTRLS */
struct hinic_cmdq_header {
	u32	header_info;
	u32	saved_data;
};

struct hinic_scmd_bufdesc {
	u32	buf_len;
	u32	rsvd;
	u8	data[HINIC_SCMD_DATA_LEN];
};

struct hinic_lcmd_bufdesc {
	struct hinic_sge	sge;
	u32			rsvd1;
	u64			saved_async_buf;
	u64			rsvd3;
};

struct hinic_cmdq_db {
	u32	db_info;
	u32	rsvd;
};

struct hinic_status {
	u32 status_info;
};

struct hinic_ctrl {
	u32 ctrl_info;
};

struct hinic_sge_resp {
	struct hinic_sge sge;
	u32		rsvd;
};

struct hinic_cmdq_completion {
	/* HW Format */
	union {
		struct hinic_sge_resp	sge_resp;
		u64			direct_resp;
	};
};

struct hinic_cmdq_wqe_scmd {
	struct hinic_cmdq_header	header;
	struct hinic_cmdq_db		db;
	struct hinic_status		status;
	struct hinic_ctrl		ctrl;
	struct hinic_cmdq_completion	completion;
	struct hinic_scmd_bufdesc	buf_desc;
};

struct hinic_cmdq_wqe_lcmd {
	struct hinic_cmdq_header	header;
	struct hinic_status		status;
	struct hinic_ctrl		ctrl;
	struct hinic_cmdq_completion	completion;
	struct hinic_lcmd_bufdesc	buf_desc;
};

struct hinic_cmdq_inline_wqe {
	struct hinic_cmdq_wqe_scmd	wqe_scmd;
};

struct hinic_cmdq_wqe {
	/* HW Format */
	union{
		struct hinic_cmdq_inline_wqe	inline_wqe;
		struct hinic_cmdq_wqe_lcmd	wqe_lcmd;
	};
};

struct hinic_cmdq_ctxt_info {
	u64	curr_wqe_page_pfn;
	u64	wq_block_pfn;
};

/* New interface */
struct hinic_cmdq_ctxt {
	u8	status;
	u8	version;
	u8	resp_aeq_num;
	u8	rsvd0[5];

	u16	func_idx;
	u8	cmdq_id;
	u8	ppf_idx;

	u8	rsvd1[4];

	struct hinic_cmdq_ctxt_info ctxt_info;
};

enum hinic_cmdq_status {
	HINIC_CMDQ_ENABLE = BIT(0),
	HINIC_CMDQ_SET_FAIL = BIT(1)
};

enum hinic_cmdq_cmd_type {
	HINIC_CMD_TYPE_NONE,
	HINIC_CMD_TYPE_SET_ARM,
	HINIC_CMD_TYPE_NORMAL,
};

struct hinic_cmdq_cmd_info {
	enum hinic_cmdq_cmd_type cmd_type;
};

struct hinic_cmdq {
	struct hinic_wq			*wq;

	enum hinic_cmdq_type		cmdq_type;
	int				wrapped;

	hinic_spinlock_t		cmdq_lock;

	int				*errcode;

	/* doorbell area */
	u8 __iomem			*db_base;

	struct hinic_cmdq_ctxt		cmdq_ctxt;

	struct hinic_cmdq_cmd_info	*cmd_infos;
};

struct hinic_cmdqs {
	struct hinic_hwdev		*hwdev;

	struct pci_pool			*cmd_buf_pool;

	struct hinic_wq			*saved_wqs;

	struct hinic_cmdq		cmdq[HINIC_MAX_CMDQ_TYPES];

	u32				status;
};

struct hinic_cmd_buf {
	void		*buf;
	dma_addr_t	dma_addr;
	struct rte_mbuf *mbuf;
	u16		size;
};

int hinic_reinit_cmdq_ctxts(struct hinic_hwdev *hwdev);

bool hinic_cmdq_idle(struct hinic_cmdq *cmdq);

struct hinic_cmd_buf *hinic_alloc_cmd_buf(void *hwdev);

void hinic_free_cmd_buf(void *hwdev, struct hinic_cmd_buf *cmd_buf);

/* PF/VF send cmd to ucode by cmdq, and return if success.
 * timeout=0, use default timeout.
 */
int hinic_cmdq_direct_resp(void *hwdev, enum hinic_ack_type ack_type,
			   enum hinic_mod_type mod, u8 cmd,
			   struct hinic_cmd_buf *buf_in,
			   u64 *out_param, u32 timeout);

int hinic_comm_cmdqs_init(struct hinic_hwdev *hwdev);

void hinic_comm_cmdqs_free(struct hinic_hwdev *hwdev);

#endif /* _HINIC_PMD_CMDQ_H_ */
