/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_wq.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_mbox.h"
#include "hinic_pmd_cmdq.h"

#define CMDQ_CMD_TIMEOUT				5000 /* millisecond */

#define UPPER_8_BITS(data)				(((data) >> 8) & 0xFF)
#define LOWER_8_BITS(data)				((data) & 0xFF)

#define CMDQ_DB_INFO_HI_PROD_IDX_SHIFT			0
#define CMDQ_DB_INFO_QUEUE_TYPE_SHIFT			23
#define CMDQ_DB_INFO_CMDQ_TYPE_SHIFT			24
#define CMDQ_DB_INFO_SRC_TYPE_SHIFT			27

#define CMDQ_DB_INFO_HI_PROD_IDX_MASK			0xFFU
#define CMDQ_DB_INFO_QUEUE_TYPE_MASK			0x1U
#define CMDQ_DB_INFO_CMDQ_TYPE_MASK			0x7U
#define CMDQ_DB_INFO_SRC_TYPE_MASK			0x1FU

#define CMDQ_DB_INFO_SET(val, member)		\
	(((val) & CMDQ_DB_INFO_##member##_MASK) <<	\
		CMDQ_DB_INFO_##member##_SHIFT)

#define CMDQ_CTRL_PI_SHIFT				0
#define CMDQ_CTRL_CMD_SHIFT				16
#define CMDQ_CTRL_MOD_SHIFT				24
#define CMDQ_CTRL_ACK_TYPE_SHIFT			29
#define CMDQ_CTRL_HW_BUSY_BIT_SHIFT			31

#define CMDQ_CTRL_PI_MASK				0xFFFFU
#define CMDQ_CTRL_CMD_MASK				0xFFU
#define CMDQ_CTRL_MOD_MASK				0x1FU
#define CMDQ_CTRL_ACK_TYPE_MASK				0x3U
#define CMDQ_CTRL_HW_BUSY_BIT_MASK			0x1U

#define CMDQ_CTRL_SET(val, member)		\
	(((val) & CMDQ_CTRL_##member##_MASK) << CMDQ_CTRL_##member##_SHIFT)

#define CMDQ_CTRL_GET(val, member)		\
	(((val) >> CMDQ_CTRL_##member##_SHIFT) & CMDQ_CTRL_##member##_MASK)

#define CMDQ_WQE_HEADER_BUFDESC_LEN_SHIFT		0
#define CMDQ_WQE_HEADER_COMPLETE_FMT_SHIFT		15
#define CMDQ_WQE_HEADER_DATA_FMT_SHIFT			22
#define CMDQ_WQE_HEADER_COMPLETE_REQ_SHIFT		23
#define CMDQ_WQE_HEADER_COMPLETE_SECT_LEN_SHIFT		27
#define CMDQ_WQE_HEADER_CTRL_LEN_SHIFT			29
#define CMDQ_WQE_HEADER_HW_BUSY_BIT_SHIFT		31

#define CMDQ_WQE_HEADER_BUFDESC_LEN_MASK		0xFFU
#define CMDQ_WQE_HEADER_COMPLETE_FMT_MASK		0x1U
#define CMDQ_WQE_HEADER_DATA_FMT_MASK			0x1U
#define CMDQ_WQE_HEADER_COMPLETE_REQ_MASK		0x1U
#define CMDQ_WQE_HEADER_COMPLETE_SECT_LEN_MASK		0x3U
#define CMDQ_WQE_HEADER_CTRL_LEN_MASK			0x3U
#define CMDQ_WQE_HEADER_HW_BUSY_BIT_MASK		0x1U

#define CMDQ_WQE_HEADER_SET(val, member)	\
	(((val) & CMDQ_WQE_HEADER_##member##_MASK) <<	\
		CMDQ_WQE_HEADER_##member##_SHIFT)

#define CMDQ_WQE_HEADER_GET(val, member)	\
	(((val) >> CMDQ_WQE_HEADER_##member##_SHIFT) &	\
		CMDQ_WQE_HEADER_##member##_MASK)

#define CMDQ_CTXT_CURR_WQE_PAGE_PFN_SHIFT		0
#define CMDQ_CTXT_EQ_ID_SHIFT				56
#define CMDQ_CTXT_CEQ_ARM_SHIFT				61
#define CMDQ_CTXT_CEQ_EN_SHIFT				62
#define CMDQ_CTXT_HW_BUSY_BIT_SHIFT			63

#define CMDQ_CTXT_CURR_WQE_PAGE_PFN_MASK		0xFFFFFFFFFFFFF
#define CMDQ_CTXT_EQ_ID_MASK				0x1F
#define CMDQ_CTXT_CEQ_ARM_MASK				0x1
#define CMDQ_CTXT_CEQ_EN_MASK				0x1
#define CMDQ_CTXT_HW_BUSY_BIT_MASK			0x1

#define CMDQ_CTXT_PAGE_INFO_SET(val, member)		\
	(((u64)(val) & CMDQ_CTXT_##member##_MASK) << CMDQ_CTXT_##member##_SHIFT)

#define CMDQ_CTXT_PAGE_INFO_CLEAR(val, member)		\
	((val) & (~((u64)CMDQ_CTXT_##member##_MASK <<	\
		CMDQ_CTXT_##member##_SHIFT)))

#define CMDQ_CTXT_WQ_BLOCK_PFN_SHIFT			0
#define CMDQ_CTXT_CI_SHIFT				52

#define CMDQ_CTXT_WQ_BLOCK_PFN_MASK			0xFFFFFFFFFFFFF
#define CMDQ_CTXT_CI_MASK				0xFFF

#define CMDQ_CTXT_BLOCK_INFO_SET(val, member)		\
	(((u64)(val) & CMDQ_CTXT_##member##_MASK) << CMDQ_CTXT_##member##_SHIFT)

#define SAVED_DATA_ARM_SHIFT			31

#define SAVED_DATA_ARM_MASK			0x1U

#define SAVED_DATA_SET(val, member)		\
	(((val) & SAVED_DATA_##member##_MASK) << SAVED_DATA_##member##_SHIFT)

#define SAVED_DATA_CLEAR(val, member)		\
	((val) & (~(SAVED_DATA_##member##_MASK << SAVED_DATA_##member##_SHIFT)))

#define WQE_ERRCODE_VAL_SHIFT			20

#define WQE_ERRCODE_VAL_MASK			0xF

#define WQE_ERRCODE_GET(val, member)		\
	(((val) >> WQE_ERRCODE_##member##_SHIFT) & WQE_ERRCODE_##member##_MASK)

#define WQE_COMPLETED(ctrl_info)	CMDQ_CTRL_GET(ctrl_info, HW_BUSY_BIT)

#define WQE_HEADER(wqe)		((struct hinic_cmdq_header *)(wqe))

#define CMDQ_DB_PI_OFF(pi)		(((u16)LOWER_8_BITS(pi)) << 3)

#define CMDQ_DB_ADDR(db_base, pi)	\
	(((u8 *)(db_base) + HINIC_DB_OFF) + CMDQ_DB_PI_OFF(pi))

#define CMDQ_PFN(addr, page_size)	((addr) >> (ilog2(page_size)))

#define FIRST_DATA_TO_WRITE_LAST	sizeof(u64)

#define WQE_LCMD_SIZE		64
#define WQE_SCMD_SIZE		64

#define COMPLETE_LEN		3

#define CMDQ_WQEBB_SIZE		64
#define CMDQ_WQEBB_SHIFT	6

#define CMDQ_WQE_SIZE		64

#define HINIC_CMDQ_WQ_BUF_SIZE	4096

#define WQE_NUM_WQEBBS(wqe_size, wq)	\
	((u16)(ALIGN((u32)(wqe_size), (wq)->wqebb_size) / (wq)->wqebb_size))

#define cmdq_to_cmdqs(cmdq)	container_of((cmdq) - (cmdq)->cmdq_type, \
				struct hinic_cmdqs, cmdq[0])

#define WAIT_CMDQ_ENABLE_TIMEOUT	300


static void cmdq_init_queue_ctxt(struct hinic_cmdq *cmdq,
				 struct hinic_cmdq_ctxt *cmdq_ctxt);
static void hinic_cmdqs_free(struct hinic_hwdev *hwdev);

bool hinic_cmdq_idle(struct hinic_cmdq *cmdq)
{
	struct hinic_wq *wq = cmdq->wq;

	return ((wq->delta) == wq->q_depth ? true : false);
}

struct hinic_cmd_buf *hinic_alloc_cmd_buf(void *hwdev)
{
	struct hinic_cmdqs *cmdqs = ((struct hinic_hwdev *)hwdev)->cmdqs;
	struct hinic_cmd_buf *cmd_buf;

	cmd_buf = kzalloc(sizeof(*cmd_buf), GFP_KERNEL);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buffer failed");
		return NULL;
	}

	cmd_buf->buf = pci_pool_alloc(cmdqs->cmd_buf_pool, &cmd_buf->dma_addr);
	if (!cmd_buf->buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd from the pool failed");
		goto alloc_pci_buf_err;
	}

	return cmd_buf;

alloc_pci_buf_err:
	kfree(cmd_buf);
	return NULL;
}

void hinic_free_cmd_buf(void *hwdev, struct hinic_cmd_buf *cmd_buf)
{
	struct hinic_cmdqs *cmdqs = ((struct hinic_hwdev *)hwdev)->cmdqs;

	pci_pool_free(cmdqs->cmd_buf_pool, cmd_buf->buf, cmd_buf->dma_addr);
	kfree(cmd_buf);
}

static u32 cmdq_wqe_size(enum cmdq_wqe_type wqe_type)
{
	u32 wqe_size = 0;

	switch (wqe_type) {
	case WQE_LCMD_TYPE:
		wqe_size = WQE_LCMD_SIZE;
		break;
	case WQE_SCMD_TYPE:
		wqe_size = WQE_SCMD_SIZE;
		break;
	}

	return wqe_size;
}

static int cmdq_get_wqe_size(enum bufdesc_len len)
{
	int wqe_size = 0;

	switch (len) {
	case BUFDESC_LCMD_LEN:
		wqe_size = WQE_LCMD_SIZE;
		break;
	case BUFDESC_SCMD_LEN:
		wqe_size = WQE_SCMD_SIZE;
		break;
	}

	return wqe_size;
}

static void cmdq_set_completion(struct hinic_cmdq_completion *complete,
					struct hinic_cmd_buf *buf_out)
{
	struct hinic_sge_resp *sge_resp = &complete->sge_resp;

	hinic_set_sge(&sge_resp->sge, buf_out->dma_addr,
		      HINIC_CMDQ_BUF_SIZE);
}

static void cmdq_set_lcmd_bufdesc(struct hinic_cmdq_wqe_lcmd *wqe,
					struct hinic_cmd_buf *buf_in)
{
	hinic_set_sge(&wqe->buf_desc.sge, buf_in->dma_addr, buf_in->size);
}

static void cmdq_fill_db(struct hinic_cmdq_db *db,
			enum hinic_cmdq_type cmdq_type, u16 prod_idx)
{
	db->db_info = CMDQ_DB_INFO_SET(UPPER_8_BITS(prod_idx), HI_PROD_IDX) |
			CMDQ_DB_INFO_SET(HINIC_DB_CMDQ_TYPE, QUEUE_TYPE) |
			CMDQ_DB_INFO_SET(cmdq_type, CMDQ_TYPE)		|
			CMDQ_DB_INFO_SET(HINIC_DB_SRC_CMDQ_TYPE, SRC_TYPE);
}

static void cmdq_set_db(struct hinic_cmdq *cmdq,
			enum hinic_cmdq_type cmdq_type, u16 prod_idx)
{
	struct hinic_cmdq_db db;

	cmdq_fill_db(&db, cmdq_type, prod_idx);

	/* The data that is written to HW should be in Big Endian Format */
	db.db_info = cpu_to_be32(db.db_info);

	rte_wmb();	/* write all before the doorbell */

	writel(db.db_info, CMDQ_DB_ADDR(cmdq->db_base, prod_idx));
}

static void cmdq_wqe_fill(void *dst, void *src)
{
	memcpy((u8 *)dst + FIRST_DATA_TO_WRITE_LAST,
	       (u8 *)src + FIRST_DATA_TO_WRITE_LAST,
	       CMDQ_WQE_SIZE - FIRST_DATA_TO_WRITE_LAST);

	rte_wmb();/* The first 8 bytes should be written last */

	*(u64 *)dst = *(u64 *)src;
}

static void cmdq_prepare_wqe_ctrl(struct hinic_cmdq_wqe *wqe, int wrapped,
				  enum hinic_ack_type ack_type,
				  enum hinic_mod_type mod, u8 cmd, u16 prod_idx,
				  enum completion_format complete_format,
				  enum data_format local_data_format,
				  enum bufdesc_len buf_len)
{
	struct hinic_ctrl *ctrl;
	enum ctrl_sect_len ctrl_len;
	struct hinic_cmdq_wqe_lcmd *wqe_lcmd;
	struct hinic_cmdq_wqe_scmd *wqe_scmd;
	u32 saved_data = WQE_HEADER(wqe)->saved_data;

	if (local_data_format == DATA_SGE) {
		wqe_lcmd = &wqe->wqe_lcmd;

		wqe_lcmd->status.status_info = 0;
		ctrl = &wqe_lcmd->ctrl;
		ctrl_len = CTRL_SECT_LEN;
	} else {
		wqe_scmd = &wqe->inline_wqe.wqe_scmd;

		wqe_scmd->status.status_info = 0;
		ctrl = &wqe_scmd->ctrl;
		ctrl_len = CTRL_DIRECT_SECT_LEN;
	}

	ctrl->ctrl_info = CMDQ_CTRL_SET(prod_idx, PI)		|
			CMDQ_CTRL_SET(cmd, CMD)			|
			CMDQ_CTRL_SET(mod, MOD)			|
			CMDQ_CTRL_SET(ack_type, ACK_TYPE);

	WQE_HEADER(wqe)->header_info =
		CMDQ_WQE_HEADER_SET(buf_len, BUFDESC_LEN) |
		CMDQ_WQE_HEADER_SET(complete_format, COMPLETE_FMT) |
		CMDQ_WQE_HEADER_SET(local_data_format, DATA_FMT)	|
		CMDQ_WQE_HEADER_SET(CEQ_SET, COMPLETE_REQ)	|
		CMDQ_WQE_HEADER_SET(COMPLETE_LEN, COMPLETE_SECT_LEN) |
		CMDQ_WQE_HEADER_SET(ctrl_len, CTRL_LEN)		|
		CMDQ_WQE_HEADER_SET((u32)wrapped, HW_BUSY_BIT);

	if (cmd == CMDQ_SET_ARM_CMD && mod == HINIC_MOD_COMM) {
		saved_data &= SAVED_DATA_CLEAR(saved_data, ARM);
		WQE_HEADER(wqe)->saved_data = saved_data |
						SAVED_DATA_SET(1, ARM);
	} else {
		saved_data &= SAVED_DATA_CLEAR(saved_data, ARM);
		WQE_HEADER(wqe)->saved_data = saved_data;
	}
}

static void cmdq_set_lcmd_wqe(struct hinic_cmdq_wqe *wqe,
			      enum cmdq_cmd_type cmd_type,
			      struct hinic_cmd_buf *buf_in,
			      struct hinic_cmd_buf *buf_out, int wrapped,
			      enum hinic_ack_type ack_type,
			      enum hinic_mod_type mod, u8 cmd, u16 prod_idx)
{
	struct hinic_cmdq_wqe_lcmd *wqe_lcmd = &wqe->wqe_lcmd;
	enum completion_format complete_format = COMPLETE_DIRECT;

	switch (cmd_type) {
	case SYNC_CMD_SGE_RESP:
		if (buf_out) {
			complete_format = COMPLETE_SGE;
			cmdq_set_completion(&wqe_lcmd->completion, buf_out);
		}
		break;
	case SYNC_CMD_DIRECT_RESP:
		complete_format = COMPLETE_DIRECT;
		wqe_lcmd->completion.direct_resp = 0;
		break;
	case ASYNC_CMD:
		complete_format = COMPLETE_DIRECT;
		wqe_lcmd->completion.direct_resp = 0;

		wqe_lcmd->buf_desc.saved_async_buf = (u64)(buf_in);
		break;
	}

	cmdq_prepare_wqe_ctrl(wqe, wrapped, ack_type, mod, cmd,
			      prod_idx, complete_format, DATA_SGE,
			      BUFDESC_LCMD_LEN);

	cmdq_set_lcmd_bufdesc(wqe_lcmd, buf_in);
}

static int cmdq_params_valid(struct hinic_cmd_buf *buf_in)
{
	if (buf_in->size > HINIC_CMDQ_MAX_DATA_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid CMDQ buffer size");
		return -EINVAL;
	}

	return 0;
}

static int wait_cmdqs_enable(struct hinic_cmdqs *cmdqs)
{
	unsigned long end;

	end = jiffies + msecs_to_jiffies(WAIT_CMDQ_ENABLE_TIMEOUT);
	do {
		if (cmdqs->status & HINIC_CMDQ_ENABLE)
			return 0;

	} while (time_before(jiffies, end));

	return -EBUSY;
}

static void cmdq_update_errcode(struct hinic_cmdq *cmdq, u16 prod_idx,
				int errcode)
{
	cmdq->errcode[prod_idx] = errcode;
}

static void clear_wqe_complete_bit(struct hinic_cmdq *cmdq,
				   struct hinic_cmdq_wqe *wqe)
{
	struct hinic_cmdq_wqe_lcmd *wqe_lcmd;
	struct hinic_cmdq_inline_wqe *inline_wqe;
	struct hinic_cmdq_wqe_scmd *wqe_scmd;
	struct hinic_ctrl *ctrl;
	u32 header_info = be32_to_cpu(WQE_HEADER(wqe)->header_info);
	int buf_len = CMDQ_WQE_HEADER_GET(header_info, BUFDESC_LEN);
	int wqe_size = cmdq_get_wqe_size(buf_len);
	u16 num_wqebbs;

	if (wqe_size == WQE_LCMD_SIZE) {
		wqe_lcmd = &wqe->wqe_lcmd;
		ctrl = &wqe_lcmd->ctrl;
	} else {
		inline_wqe = &wqe->inline_wqe;
		wqe_scmd = &inline_wqe->wqe_scmd;
		ctrl = &wqe_scmd->ctrl;
	}

	/* clear HW busy bit */
	ctrl->ctrl_info = 0;

	rte_wmb();	/* verify wqe is clear */

	num_wqebbs = WQE_NUM_WQEBBS(wqe_size, cmdq->wq);
	hinic_put_wqe(cmdq->wq, num_wqebbs);
}

static int hinic_set_cmdq_ctxts(struct hinic_hwdev *hwdev)
{
	struct hinic_cmdqs *cmdqs = hwdev->cmdqs;
	struct hinic_cmdq_ctxt *cmdq_ctxt;
	enum hinic_cmdq_type cmdq_type;
	u16 in_size;
	int err;

	cmdq_type = HINIC_CMDQ_SYNC;
	for (; cmdq_type < HINIC_MAX_CMDQ_TYPES; cmdq_type++) {
		cmdq_ctxt = &cmdqs->cmdq[cmdq_type].cmdq_ctxt;
		cmdq_ctxt->resp_aeq_num = HINIC_AEQ1;
		in_size = sizeof(*cmdq_ctxt);
		err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
					     HINIC_MGMT_CMD_CMDQ_CTXT_SET,
					     cmdq_ctxt, in_size, NULL,
					     NULL, 0);
		if (err) {
			if (err == HINIC_MBOX_PF_BUSY_ACTIVE_FW ||
				err == HINIC_DEV_BUSY_ACTIVE_FW) {
				cmdqs->status |= HINIC_CMDQ_SET_FAIL;
				PMD_DRV_LOG(ERR, "PF or VF fw is hot active");
			}
			PMD_DRV_LOG(ERR, "Set cmdq ctxt failed, err: %d", err);
			return -EFAULT;
		}
	}

	cmdqs->status &= ~HINIC_CMDQ_SET_FAIL;
	cmdqs->status |= HINIC_CMDQ_ENABLE;

	return 0;
}

void hinic_comm_cmdqs_free(struct hinic_hwdev *hwdev)
{
	hinic_cmdqs_free(hwdev);
}

int hinic_reinit_cmdq_ctxts(struct hinic_hwdev *hwdev)
{
	struct hinic_cmdqs *cmdqs = hwdev->cmdqs;
	enum hinic_cmdq_type cmdq_type;

	cmdq_type = HINIC_CMDQ_SYNC;
	for (; cmdq_type < HINIC_MAX_CMDQ_TYPES; cmdq_type++) {
		cmdqs->cmdq[cmdq_type].wrapped = 1;
		hinic_wq_wqe_pg_clear(cmdqs->cmdq[cmdq_type].wq);
	}

	return hinic_set_cmdq_ctxts(hwdev);
}

static int init_cmdq(struct hinic_cmdq *cmdq, struct hinic_hwdev *hwdev,
		     struct hinic_wq *wq, enum hinic_cmdq_type q_type)
{
	void __iomem *db_base;
	int err = 0;
	size_t errcode_size;
	size_t cmd_infos_size;

	cmdq->wq = wq;
	cmdq->cmdq_type = q_type;
	cmdq->wrapped = 1;

	spin_lock_init(&cmdq->cmdq_lock);

	errcode_size = wq->q_depth * sizeof(*cmdq->errcode);
	cmdq->errcode = kzalloc(errcode_size, GFP_KERNEL);
	if (!cmdq->errcode) {
		PMD_DRV_LOG(ERR, "Allocate errcode for cmdq failed");
		spin_lock_deinit(&cmdq->cmdq_lock);
		return -ENOMEM;
	}

	cmd_infos_size = wq->q_depth * sizeof(*cmdq->cmd_infos);
	cmdq->cmd_infos = kzalloc(cmd_infos_size, GFP_KERNEL);
	if (!cmdq->cmd_infos) {
		PMD_DRV_LOG(ERR, "Allocate errcode for cmdq failed");
		err = -ENOMEM;
		goto cmd_infos_err;
	}

	err = hinic_alloc_db_addr(hwdev, &db_base);
	if (err)
		goto alloc_db_err;

	cmdq->db_base = (u8 *)db_base;
	return 0;

alloc_db_err:
	kfree(cmdq->cmd_infos);

cmd_infos_err:
	kfree(cmdq->errcode);
	spin_lock_deinit(&cmdq->cmdq_lock);

	return err;
}

static void free_cmdq(struct hinic_hwdev *hwdev, struct hinic_cmdq *cmdq)
{
	hinic_free_db_addr(hwdev, cmdq->db_base);
	kfree(cmdq->cmd_infos);
	kfree(cmdq->errcode);
	spin_lock_deinit(&cmdq->cmdq_lock);
}

static int hinic_cmdqs_init(struct hinic_hwdev *hwdev)
{
	struct hinic_cmdqs *cmdqs;
	struct hinic_cmdq_ctxt *cmdq_ctxt;
	enum hinic_cmdq_type type, cmdq_type;
	size_t saved_wqs_size;
	int err;

	cmdqs = kzalloc(sizeof(*cmdqs), GFP_KERNEL);
	if (!cmdqs)
		return -ENOMEM;

	hwdev->cmdqs = cmdqs;
	cmdqs->hwdev = hwdev;

	saved_wqs_size = HINIC_MAX_CMDQ_TYPES * sizeof(struct hinic_wq);
	cmdqs->saved_wqs = kzalloc(saved_wqs_size, GFP_KERNEL);
	if (!cmdqs->saved_wqs) {
		PMD_DRV_LOG(ERR, "Allocate saved wqs failed");
		err = -ENOMEM;
		goto alloc_wqs_err;
	}

	cmdqs->cmd_buf_pool = dma_pool_create("hinic_cmdq", hwdev,
					      HINIC_CMDQ_BUF_SIZE,
					      HINIC_CMDQ_BUF_SIZE, 0ULL);
	if (!cmdqs->cmd_buf_pool) {
		PMD_DRV_LOG(ERR, "Create cmdq buffer pool failed");
		err = -ENOMEM;
		goto pool_create_err;
	}

	err = hinic_cmdq_alloc(cmdqs->saved_wqs, hwdev,
			       HINIC_MAX_CMDQ_TYPES, HINIC_CMDQ_WQ_BUF_SIZE,
			       CMDQ_WQEBB_SHIFT, HINIC_CMDQ_DEPTH);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate cmdq failed");
		goto cmdq_alloc_err;
	}

	cmdq_type = HINIC_CMDQ_SYNC;
	for (; cmdq_type < HINIC_MAX_CMDQ_TYPES; cmdq_type++) {
		err = init_cmdq(&cmdqs->cmdq[cmdq_type], hwdev,
				&cmdqs->saved_wqs[cmdq_type], cmdq_type);
		if (err) {
			PMD_DRV_LOG(ERR, "Initialize cmdq failed");
			goto init_cmdq_err;
		}

		cmdq_ctxt = &cmdqs->cmdq[cmdq_type].cmdq_ctxt;
		cmdq_init_queue_ctxt(&cmdqs->cmdq[cmdq_type], cmdq_ctxt);
	}

	err = hinic_set_cmdq_ctxts(hwdev);
	if (err)
		goto init_cmdq_err;

	return 0;

init_cmdq_err:
	type = HINIC_CMDQ_SYNC;
	for ( ; type < cmdq_type; type++)
		free_cmdq(hwdev, &cmdqs->cmdq[type]);

	hinic_cmdq_free(hwdev, cmdqs->saved_wqs, HINIC_MAX_CMDQ_TYPES);

cmdq_alloc_err:
	dma_pool_destroy(cmdqs->cmd_buf_pool);

pool_create_err:
	kfree(cmdqs->saved_wqs);

alloc_wqs_err:
	kfree(cmdqs);

	return err;
}

static void hinic_cmdqs_free(struct hinic_hwdev *hwdev)
{
	struct hinic_cmdqs *cmdqs = hwdev->cmdqs;
	enum hinic_cmdq_type cmdq_type = HINIC_CMDQ_SYNC;

	cmdqs->status &= ~HINIC_CMDQ_ENABLE;

	for ( ; cmdq_type < HINIC_MAX_CMDQ_TYPES; cmdq_type++)
		free_cmdq(cmdqs->hwdev, &cmdqs->cmdq[cmdq_type]);

	hinic_cmdq_free(hwdev, cmdqs->saved_wqs,
			HINIC_MAX_CMDQ_TYPES);

	dma_pool_destroy(cmdqs->cmd_buf_pool);

	kfree(cmdqs->saved_wqs);

	kfree(cmdqs);
}

static int hinic_set_cmdq_depth(struct hinic_hwdev *hwdev, u16 cmdq_depth)
{
	struct hinic_root_ctxt root_ctxt;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	root_ctxt.func_idx = hinic_global_func_id(hwdev);
	root_ctxt.ppf_idx = hinic_ppf_idx(hwdev);
	root_ctxt.set_cmdq_depth = 1;
	root_ctxt.cmdq_depth = (u8)ilog2(cmdq_depth);
	return hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				      HINIC_MGMT_CMD_VAT_SET,
				      &root_ctxt, sizeof(root_ctxt),
				      NULL, NULL, 0);
}

int hinic_comm_cmdqs_init(struct hinic_hwdev *hwdev)
{
	int err;

	err = hinic_cmdqs_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init cmd queues failed");
		return err;
	}

	err = hinic_set_cmdq_depth(hwdev, HINIC_CMDQ_DEPTH);
	if (err) {
		PMD_DRV_LOG(ERR, "Set cmdq depth failed");
		goto set_cmdq_depth_err;
	}

	return 0;

set_cmdq_depth_err:
	hinic_cmdqs_free(hwdev);

	return err;
}

static void cmdq_init_queue_ctxt(struct hinic_cmdq *cmdq,
				 struct hinic_cmdq_ctxt *cmdq_ctxt)
{
	struct hinic_cmdqs *cmdqs = (struct hinic_cmdqs *)cmdq_to_cmdqs(cmdq);
	struct hinic_hwdev *hwdev = cmdqs->hwdev;
	struct hinic_wq *wq = cmdq->wq;
	struct hinic_cmdq_ctxt_info *ctxt_info = &cmdq_ctxt->ctxt_info;
	u64 wq_first_page_paddr, pfn;

	u16 start_ci = (u16)(wq->cons_idx);

	/* The data in the HW is in Big Endian Format */
	wq_first_page_paddr = wq->queue_buf_paddr;

	pfn = CMDQ_PFN(wq_first_page_paddr, HINIC_PAGE_SIZE);
	ctxt_info->curr_wqe_page_pfn =
		CMDQ_CTXT_PAGE_INFO_SET(1, HW_BUSY_BIT) |
		CMDQ_CTXT_PAGE_INFO_SET(1, CEQ_EN)	|
		CMDQ_CTXT_PAGE_INFO_SET(0, CEQ_ARM)	|
		CMDQ_CTXT_PAGE_INFO_SET(HINIC_CEQ_ID_CMDQ, EQ_ID) |
		CMDQ_CTXT_PAGE_INFO_SET(pfn, CURR_WQE_PAGE_PFN);

	ctxt_info->wq_block_pfn = CMDQ_CTXT_BLOCK_INFO_SET(start_ci, CI) |
				CMDQ_CTXT_BLOCK_INFO_SET(pfn, WQ_BLOCK_PFN);
	cmdq_ctxt->func_idx = HINIC_HWIF_GLOBAL_IDX(hwdev->hwif);
	cmdq_ctxt->ppf_idx  = HINIC_HWIF_PPF_IDX(hwdev->hwif);
	cmdq_ctxt->cmdq_id  = cmdq->cmdq_type;
}

static int hinic_cmdq_poll_msg(struct hinic_cmdq *cmdq, u32 timeout)
{
	struct hinic_cmdq_wqe *wqe;
	struct hinic_cmdq_wqe_lcmd *wqe_lcmd;
	struct hinic_ctrl *ctrl;
	struct hinic_cmdq_cmd_info *cmd_info;
	u32 status_info, ctrl_info;
	u16 ci;
	int errcode;
	unsigned long end;
	int done = 0;
	int rc = 0;

	wqe = hinic_read_wqe(cmdq->wq, 1, &ci);
	if (wqe == NULL) {
		PMD_DRV_LOG(ERR, "No outstanding cmdq msg");
		return -EINVAL;
	}

	cmd_info = &cmdq->cmd_infos[ci];
	/* this cmd has not been filled and send to hw, or get TMO msg ack*/
	if (cmd_info->cmd_type == HINIC_CMD_TYPE_NONE) {
		PMD_DRV_LOG(ERR, "Cmdq msg has not been filled and send to hw, or get TMO msg ack. cmdq ci: %u",
			    ci);
		return -EINVAL;
	}

	/* only arm bit is using scmd wqe, the wqe is lcmd */
	wqe_lcmd = &wqe->wqe_lcmd;
	ctrl = &wqe_lcmd->ctrl;
	end = jiffies + msecs_to_jiffies(timeout);
	do {
		ctrl_info = be32_to_cpu((ctrl)->ctrl_info);
		if (WQE_COMPLETED(ctrl_info)) {
			done = 1;
			break;
		}

		rte_delay_ms(1);
	} while (time_before(jiffies, end));

	if (done) {
		status_info = be32_to_cpu(wqe_lcmd->status.status_info);
		errcode = WQE_ERRCODE_GET(status_info, VAL);
		cmdq_update_errcode(cmdq, ci, errcode);
		clear_wqe_complete_bit(cmdq, wqe);
		rc = 0;
	} else {
		PMD_DRV_LOG(ERR, "Poll cmdq msg time out, ci: %u", ci);
		rc = -ETIMEDOUT;
	}

	/* set this cmd invalid */
	cmd_info->cmd_type = HINIC_CMD_TYPE_NONE;

	return rc;
}

static int cmdq_sync_cmd_direct_resp(struct hinic_cmdq *cmdq,
				     enum hinic_ack_type ack_type,
				     enum hinic_mod_type mod, u8 cmd,
				     struct hinic_cmd_buf *buf_in,
				     u64 *out_param, u32 timeout)
{
	struct hinic_wq *wq = cmdq->wq;
	struct hinic_cmdq_wqe *curr_wqe, wqe;
	struct hinic_cmdq_wqe_lcmd *wqe_lcmd;
	u16 curr_prod_idx, next_prod_idx, num_wqebbs;
	int wrapped;
	u32 timeo, wqe_size;
	int err;

	wqe_size = cmdq_wqe_size(WQE_LCMD_TYPE);
	num_wqebbs = WQE_NUM_WQEBBS(wqe_size, wq);

	/* Keep wrapped and doorbell index correct. */
	spin_lock(&cmdq->cmdq_lock);

	curr_wqe = hinic_get_wqe(cmdq->wq, num_wqebbs, &curr_prod_idx);
	if (!curr_wqe) {
		err = -EBUSY;
		goto cmdq_unlock;
	}

	memset(&wqe, 0, sizeof(wqe));
	wrapped = cmdq->wrapped;

	next_prod_idx = curr_prod_idx + num_wqebbs;
	if (next_prod_idx >= wq->q_depth) {
		cmdq->wrapped = !cmdq->wrapped;
		next_prod_idx -= wq->q_depth;
	}

	cmdq_set_lcmd_wqe(&wqe, SYNC_CMD_DIRECT_RESP, buf_in, NULL,
			  wrapped, ack_type, mod, cmd, curr_prod_idx);

	/* The data that is written to HW should be in Big Endian Format */
	hinic_cpu_to_be32(&wqe, wqe_size);

	/* CMDQ WQE is not shadow, therefore wqe will be written to wq */
	cmdq_wqe_fill(curr_wqe, &wqe);

	cmdq->cmd_infos[curr_prod_idx].cmd_type = HINIC_CMD_TYPE_NORMAL;

	cmdq_set_db(cmdq, HINIC_CMDQ_SYNC, next_prod_idx);

	timeo = msecs_to_jiffies(timeout ? timeout : CMDQ_CMD_TIMEOUT);
	err = hinic_cmdq_poll_msg(cmdq, timeo);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq poll msg ack failed, prod idx: 0x%x",
			curr_prod_idx);
		err = -ETIMEDOUT;
		goto cmdq_unlock;
	}

	rte_smp_rmb();	/* read error code after completion */

	if (out_param) {
		wqe_lcmd = &curr_wqe->wqe_lcmd;
		*out_param = cpu_to_be64(wqe_lcmd->completion.direct_resp);
	}

	if (cmdq->errcode[curr_prod_idx] > 1) {
		err = cmdq->errcode[curr_prod_idx];
		goto cmdq_unlock;
	}

cmdq_unlock:
	spin_unlock(&cmdq->cmdq_lock);

	return err;
}

int hinic_cmdq_direct_resp(void *hwdev, enum hinic_ack_type ack_type,
			   enum hinic_mod_type mod, u8 cmd,
			   struct hinic_cmd_buf *buf_in,
			   u64 *out_param, u32 timeout)
{
	struct hinic_cmdqs *cmdqs = ((struct hinic_hwdev *)hwdev)->cmdqs;
	int err = cmdq_params_valid(buf_in);

	if (err) {
		PMD_DRV_LOG(ERR, "Invalid CMDQ parameters");
		return err;
	}

	err = wait_cmdqs_enable(cmdqs);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq is disable");
		return err;
	}

	return cmdq_sync_cmd_direct_resp(&cmdqs->cmdq[HINIC_CMDQ_SYNC],
					 ack_type, mod, cmd, buf_in,
					 out_param, timeout);
}
