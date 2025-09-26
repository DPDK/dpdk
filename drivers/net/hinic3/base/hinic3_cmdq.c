/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_cmd.h"
#include "hinic3_cmdq.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mgmt.h"
#include "hinic3_wq.h"
#include "hinic3_nic_cfg.h"

#define CMDQ_CMD_TIMEOUT 5000 /**< Millisecond. */

#define UPPER_8_BITS(data) (((data) >> 8) & 0xFF)
#define LOWER_8_BITS(data) ((data) & 0xFF)

#define CMDQ_DB_INFO_HI_PROD_IDX_SHIFT 0
#define CMDQ_DB_INFO_HI_PROD_IDX_MASK  0xFFU

#define CMDQ_DB_INFO_SET(val, member)                  \
	((((uint32_t)(val)) & CMDQ_DB_INFO_##member##_MASK) \
	 << CMDQ_DB_INFO_##member##_SHIFT)
#define CMDQ_DB_INFO_UPPER_32(val) ((uint64_t)(val) << 32)

#define CMDQ_DB_HEAD_QUEUE_TYPE_SHIFT 23
#define CMDQ_DB_HEAD_CMDQ_TYPE_SHIFT  24
#define CMDQ_DB_HEAD_SRC_TYPE_SHIFT   27
#define CMDQ_DB_HEAD_QUEUE_TYPE_MASK  0x1U
#define CMDQ_DB_HEAD_CMDQ_TYPE_MASK   0x7U
#define CMDQ_DB_HEAD_SRC_TYPE_MASK    0x1FU
#define CMDQ_DB_HEAD_SET(val, member)                  \
	((((uint32_t)(val)) & CMDQ_DB_HEAD_##member##_MASK) \
	 << CMDQ_DB_HEAD_##member##_SHIFT)

#define CMDQ_CTRL_PI_SHIFT	    0
#define CMDQ_CTRL_CMD_SHIFT	    16
#define CMDQ_CTRL_MOD_SHIFT	    24
#define CMDQ_CTRL_ACK_TYPE_SHIFT    29
#define CMDQ_CTRL_HW_BUSY_BIT_SHIFT 31

#define CMDQ_CTRL_PI_MASK	   0xFFFFU
#define CMDQ_CTRL_CMD_MASK	   0xFFU
#define CMDQ_CTRL_MOD_MASK	   0x1FU
#define CMDQ_CTRL_ACK_TYPE_MASK	   0x3U
#define CMDQ_CTRL_HW_BUSY_BIT_MASK 0x1U

#define CMDQ_CTRL_SET(val, member) \
	(((uint32_t)(val) & CMDQ_CTRL_##member##_MASK) << CMDQ_CTRL_##member##_SHIFT)

#define CMDQ_CTRL_GET(val, member) \
	(((val) >> CMDQ_CTRL_##member##_SHIFT) & CMDQ_CTRL_##member##_MASK)

#define CMDQ_WQE_HEADER_BUFDESC_LEN_SHIFT	0
#define CMDQ_WQE_HEADER_COMPLETE_FMT_SHIFT	15
#define CMDQ_WQE_HEADER_DATA_FMT_SHIFT		22
#define CMDQ_WQE_HEADER_COMPLETE_REQ_SHIFT	23
#define CMDQ_WQE_HEADER_COMPLETE_SECT_LEN_SHIFT 27
#define CMDQ_WQE_HEADER_CTRL_LEN_SHIFT		29
#define CMDQ_WQE_HEADER_HW_BUSY_BIT_SHIFT	31

#define CMDQ_WQE_HEADER_BUFDESC_LEN_MASK       0xFFU
#define CMDQ_WQE_HEADER_COMPLETE_FMT_MASK      0x1U
#define CMDQ_WQE_HEADER_DATA_FMT_MASK	       0x1U
#define CMDQ_WQE_HEADER_COMPLETE_REQ_MASK      0x1U
#define CMDQ_WQE_HEADER_COMPLETE_SECT_LEN_MASK 0x3U
#define CMDQ_WQE_HEADER_CTRL_LEN_MASK	       0x3U
#define CMDQ_WQE_HEADER_HW_BUSY_BIT_MASK       0x1U

#define CMDQ_WQE_HEADER_SET(val, member)                \
	(((uint32_t)(val) & CMDQ_WQE_HEADER_##member##_MASK) \
	 << CMDQ_WQE_HEADER_##member##_SHIFT)

#define CMDQ_WQE_HEADER_GET(val, member)               \
	(((val) >> CMDQ_WQE_HEADER_##member##_SHIFT) & \
	 CMDQ_WQE_HEADER_##member##_MASK)

#define CMDQ_CTXT_CURR_WQE_PAGE_PFN_SHIFT 0
#define CMDQ_CTXT_EQ_ID_SHIFT		  53
#define CMDQ_CTXT_CEQ_ARM_SHIFT		  61
#define CMDQ_CTXT_CEQ_EN_SHIFT		  62
#define CMDQ_CTXT_HW_BUSY_BIT_SHIFT	  63

#define CMDQ_CTXT_CURR_WQE_PAGE_PFN_MASK 0xFFFFFFFFFFFFF
#define CMDQ_CTXT_EQ_ID_MASK		 0xFF
#define CMDQ_CTXT_CEQ_ARM_MASK		 0x1
#define CMDQ_CTXT_CEQ_EN_MASK		 0x1
#define CMDQ_CTXT_HW_BUSY_BIT_MASK	 0x1

#define CMDQ_CTXT_PAGE_INFO_SET(val, member) \
	(((uint64_t)(val) & CMDQ_CTXT_##member##_MASK) << CMDQ_CTXT_##member##_SHIFT)

#define CMDQ_CTXT_WQ_BLOCK_PFN_SHIFT 0
#define CMDQ_CTXT_CI_SHIFT	     52

#define CMDQ_CTXT_WQ_BLOCK_PFN_MASK 0xFFFFFFFFFFFFF
#define CMDQ_CTXT_CI_MASK	    0xFFF

#define CMDQ_CTXT_BLOCK_INFO_SET(val, member) \
	(((uint64_t)(val) & CMDQ_CTXT_##member##_MASK) << CMDQ_CTXT_##member##_SHIFT)

#define SAVED_DATA_ARM_SHIFT 31

#define SAVED_DATA_ARM_MASK 0x1U

#define SAVED_DATA_SET(val, member) \
	(((val) & SAVED_DATA_##member##_MASK) << SAVED_DATA_##member##_SHIFT)

#define SAVED_DATA_CLEAR(val, member) \
	((val) & (~(SAVED_DATA_##member##_MASK << SAVED_DATA_##member##_SHIFT)))

#define WQE_ERRCODE_VAL_SHIFT 0

#define WQE_ERRCODE_VAL_MASK 0x7FFFFFFF

#define WQE_ERRCODE_GET(val, member) \
	(((val) >> WQE_ERRCODE_##member##_SHIFT) & WQE_ERRCODE_##member##_MASK)

#define WQE_COMPLETED(ctrl_info) CMDQ_CTRL_GET(ctrl_info, HW_BUSY_BIT)

#define WQE_HEADER(wqe) ((struct hinic3_cmdq_header *)(wqe))

#define CMDQ_DB_PI_OFF(pi) (LOWER_8_BITS(pi) << 3)

#define CMDQ_DB_ADDR(db_base, pi) ((db_base) + CMDQ_DB_PI_OFF(pi))

#define CMDQ_PFN(addr, page_size) ((addr) >> (rte_log2_u32(page_size)))

#define FIRST_DATA_TO_WRITE_LAST sizeof(uint64_t)

#define WQE_LCMD_SIZE 64
#define WQE_SCMD_SIZE 64

#define COMPLETE_LEN 3

#define CMDQ_WQEBB_SIZE	 64
#define CMDQ_WQEBB_SHIFT 6

#define CMDQ_WQE_SIZE 64

#define HINIC3_CMDQ_WQ_BUF_SIZE 4096

#define WQE_NUM_WQEBBS(wqe_size, wq)                                 \
	({                                                           \
		typeof(wq) __wq = (wq);                              \
		(uint16_t)(RTE_ALIGN((uint32_t)(wqe_size), __wq->wqebb_size) / \
		      __wq->wqebb_size);                             \
	})

#define cmdq_to_cmdqs(cmdq)                                                   \
	({                                                                    \
		typeof(cmdq) __cmdq = (cmdq);                                 \
		container_of(__cmdq - __cmdq->cmdq_type, struct hinic3_cmdqs, \
			     __cmdq[0]);                                      \
	})

#define WAIT_CMDQ_ENABLE_TIMEOUT 300

static int hinic3_cmdq_poll_msg(struct hinic3_cmdq *cmdq, uint32_t timeout);

bool
hinic3_cmdq_idle(struct hinic3_cmdq *cmdq)
{
	struct hinic3_wq *wq = cmdq->wq;

	return rte_atomic_load_explicit(&wq->delta, rte_memory_order_seq_cst) ==
	       wq->q_depth;
}

struct hinic3_cmd_buf *
hinic3_alloc_cmd_buf(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmdqs *cmdqs = hwdev->cmdqs;
	struct hinic3_cmd_buf *cmd_buf;

	cmd_buf = rte_zmalloc(NULL, sizeof(*cmd_buf), 0);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Allocate cmd buffer failed");
		return NULL;
	}

	cmd_buf->mbuf = rte_pktmbuf_alloc(cmdqs->cmd_buf_pool);
	if (!cmd_buf->mbuf) {
		PMD_DRV_LOG(ERR, "Allocate cmd from the pool failed");
		goto alloc_pci_buf_err;
	}

	cmd_buf->dma_addr = rte_mbuf_data_iova(cmd_buf->mbuf);
	cmd_buf->buf = rte_pktmbuf_mtod(cmd_buf->mbuf, void *);

	return cmd_buf;

alloc_pci_buf_err:
	rte_free(cmd_buf);
	return NULL;
}

void
hinic3_free_cmd_buf(struct hinic3_cmd_buf *cmd_buf)
{
	rte_pktmbuf_free(cmd_buf->mbuf);
	rte_free(cmd_buf);
}

static uint32_t
cmdq_wqe_size(enum cmdq_wqe_type wqe_type)
{
	uint32_t wqe_size = 0;

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

static uint32_t
cmdq_get_wqe_size(enum bufdesc_len len)
{
	uint32_t wqe_size = 0;

	switch (len) {
	case BUFDESC_LCMD_LEN:
		wqe_size = WQE_LCMD_SIZE;
		break;
	case BUFDESC_SCMD_LEN:
		wqe_size = WQE_SCMD_SIZE;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid bufdesc_len");
		break;
	}

	return wqe_size;
}

static void
cmdq_set_completion(struct hinic3_cmdq_completion *complete,
		    struct hinic3_cmd_buf *buf_out)
{
	struct hinic3_sge_resp *sge_resp = &complete->sge_resp;

	hinic3_set_sge(&sge_resp->sge, buf_out->dma_addr, HINIC3_CMDQ_BUF_SIZE);
}

static void
cmdq_set_lcmd_bufdesc(struct hinic3_cmdq_wqe_lcmd *wqe,
		      struct hinic3_cmd_buf *buf_in)
{
	hinic3_set_sge(&wqe->buf_desc.sge, buf_in->dma_addr, buf_in->size);
}

static void
cmdq_set_db(struct hinic3_cmdq *cmdq, enum hinic3_cmdq_type cmdq_type,
	    uint16_t prod_idx)
{
	uint64_t db = CMDQ_DB_INFO_SET(UPPER_8_BITS(prod_idx), HI_PROD_IDX);

	db = CMDQ_DB_INFO_UPPER_32(db) |
	     CMDQ_DB_HEAD_SET(HINIC3_DB_CMDQ_TYPE, QUEUE_TYPE) |
	     CMDQ_DB_HEAD_SET(cmdq_type, CMDQ_TYPE) |
	     CMDQ_DB_HEAD_SET(HINIC3_DB_SRC_CMDQ_TYPE, SRC_TYPE);

	/**< Write all before the doorbell. */
	rte_atomic_thread_fence(rte_memory_order_release);
	/* Hardware will do endianness converting. */
	rte_write64(db, CMDQ_DB_ADDR(cmdq->db_base, prod_idx));
}

static void
cmdq_wqe_fill(void *dst, void *src)
{
	memcpy((void *)((uint8_t *)dst + FIRST_DATA_TO_WRITE_LAST),
	       (void *)((uint8_t *)src + FIRST_DATA_TO_WRITE_LAST),
	       CMDQ_WQE_SIZE - FIRST_DATA_TO_WRITE_LAST);

	/* The first 8 bytes should be written last. */
	rte_atomic_thread_fence(rte_memory_order_release);

	*(uint64_t *)dst = *(uint64_t *)src;
}

static void
cmdq_prepare_wqe_ctrl(struct hinic3_cmdq_wqe *wqe, int wrapped,
		      enum hinic3_mod_type mod, uint8_t cmd, uint16_t prod_idx,
		      enum completion_format complete_format,
		      enum data_format local_data_format,
		      enum bufdesc_len buf_len)
{
	struct hinic3_ctrl *ctrl = NULL;
	enum ctrl_sect_len ctrl_len;
	struct hinic3_cmdq_wqe_lcmd *wqe_lcmd = NULL;
	struct hinic3_cmdq_wqe_scmd *wqe_scmd = NULL;
	uint32_t saved_data = WQE_HEADER(wqe)->saved_data;

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

	ctrl->ctrl_info = CMDQ_CTRL_SET(prod_idx, PI) |
			  CMDQ_CTRL_SET(cmd, CMD) | CMDQ_CTRL_SET(mod, MOD) |
			  CMDQ_CTRL_SET(HINIC3_ACK_TYPE_CMDQ, ACK_TYPE);

	WQE_HEADER(wqe)->header_info =
		CMDQ_WQE_HEADER_SET(buf_len, BUFDESC_LEN) |
		CMDQ_WQE_HEADER_SET(complete_format, COMPLETE_FMT) |
		CMDQ_WQE_HEADER_SET(local_data_format, DATA_FMT) |
		CMDQ_WQE_HEADER_SET(CEQ_SET, COMPLETE_REQ) |
		CMDQ_WQE_HEADER_SET(COMPLETE_LEN, COMPLETE_SECT_LEN) |
		CMDQ_WQE_HEADER_SET(ctrl_len, CTRL_LEN) |
		CMDQ_WQE_HEADER_SET((uint32_t)wrapped, HW_BUSY_BIT);

	saved_data &= SAVED_DATA_CLEAR(saved_data, ARM);
	if (cmd == CMDQ_SET_ARM_CMD && mod == HINIC3_MOD_COMM)
		WQE_HEADER(wqe)->saved_data = saved_data |
					      SAVED_DATA_SET(1, ARM);
	else
		WQE_HEADER(wqe)->saved_data = saved_data;
}

static void
cmdq_set_lcmd_wqe(struct hinic3_cmdq_wqe *wqe, enum cmdq_cmd_type cmd_type,
		  struct hinic3_cmd_buf *buf_in, struct hinic3_cmd_buf *buf_out,
		  int wrapped, enum hinic3_mod_type mod, uint8_t cmd, uint16_t prod_idx)
{
	struct hinic3_cmdq_wqe_lcmd *wqe_lcmd = &wqe->wqe_lcmd;
	enum completion_format complete_format = COMPLETE_DIRECT;

	switch (cmd_type) {
	case SYNC_CMD_DIRECT_RESP:
		complete_format = COMPLETE_DIRECT;
		wqe_lcmd->completion.direct_resp = 0;
		break;
	case SYNC_CMD_SGE_RESP:
		if (buf_out) {
			complete_format = COMPLETE_SGE;
			cmdq_set_completion(&wqe_lcmd->completion, buf_out);
		}
		break;
	case ASYNC_CMD:
		complete_format = COMPLETE_DIRECT;
		wqe_lcmd->completion.direct_resp = 0;
		wqe_lcmd->buf_desc.saved_async_buf = (uint64_t)(buf_in);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid cmdq_cmd_type");
		break;
	}

	cmdq_prepare_wqe_ctrl(wqe, wrapped, mod, cmd, prod_idx, complete_format,
			      DATA_SGE, BUFDESC_LCMD_LEN);

	cmdq_set_lcmd_bufdesc(wqe_lcmd, buf_in);
}

/**
 * Prepare necessary context for command queue, send a synchronous command with
 * a direct response to hardware. It waits for completion of command by polling
 * command queue for a response.
 *
 * @param[in] cmdq
 * The command queue object that represents the queue to send the command to.
 * @param[in] mod
 * The module type that the command belongs to.
 * @param[in] cmd
 * The command to be executed.
 * @param[in] buf_in
 * The input buffer containing the command parameters.
 * @param[out] out_param
 * A pointer to the location where the response data will be stored, if
 * available.
 * @param[in] timeout
 * The timeout value (ms) to wait for the command completion. If zero, a default
 * timeout will be used.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -EBUSY: The command queue is busy.
 * - -ETIMEDOUT: The command did not complete within the specified timeout.
 */
static int
cmdq_sync_cmd_direct_resp(struct hinic3_cmdq *cmdq, enum hinic3_mod_type mod,
			  uint8_t cmd, struct hinic3_cmd_buf *buf_in,
			  uint64_t *out_param, uint32_t timeout)
{
	struct hinic3_cmdq_wqe wqe;
	struct hinic3_wq *wq = cmdq->wq;
	struct hinic3_cmdq_wqe *curr_wqe = NULL;
	struct hinic3_cmdq_wqe_lcmd *wqe_lcmd = NULL;
	uint16_t curr_prod_idx, next_prod_idx, num_wqebbs;
	uint32_t timeo, wqe_size;
	int wrapped, err;

	wqe_size = cmdq_wqe_size(WQE_LCMD_TYPE);
	num_wqebbs = WQE_NUM_WQEBBS(wqe_size, wq);

	/* ensure thread safety and maintain wrapped and doorbell index correct. */
	rte_spinlock_lock(&cmdq->cmdq_lock);

	curr_wqe = hinic3_get_wqe(cmdq->wq, num_wqebbs, &curr_prod_idx);
	if (curr_wqe == NULL) {
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

	cmdq_set_lcmd_wqe(&wqe, SYNC_CMD_DIRECT_RESP, buf_in, NULL, wrapped,
			  mod, cmd, curr_prod_idx);


	hinic3_cpu_to_hw(&wqe, wqe_size);

	/* Cmdq wqe is not shadow, therefore wqe will be written to wq. */
	cmdq_wqe_fill(curr_wqe, &wqe);

	cmdq->cmd_infos[curr_prod_idx].cmd_type = HINIC3_CMD_TYPE_DIRECT_RESP;

	cmdq_set_db(cmdq, HINIC3_CMDQ_SYNC, next_prod_idx);

	timeo = timeout ? timeout : CMDQ_CMD_TIMEOUT;
	err = hinic3_cmdq_poll_msg(cmdq, timeo);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq poll msg ack failed, prod idx: 0x%x",
			    curr_prod_idx);
		err = -ETIMEDOUT;
		goto cmdq_unlock;
	}

	rte_smp_rmb(); /*Ensure all cmdq return messages are completed*/

	if (out_param) {
		wqe_lcmd = &curr_wqe->wqe_lcmd;
		*out_param = rte_cpu_to_be_64(wqe_lcmd->completion.direct_resp);
	}

	if (cmdq->errcode[curr_prod_idx])
		err = cmdq->errcode[curr_prod_idx];

cmdq_unlock:
	rte_spinlock_unlock(&cmdq->cmdq_lock);

	return err;
}

/**
 * Send a synchronous command with detailed response and wait for the
 * completion.
 *
 * @param[in] cmdq
 * The command queue object representing the queue to send the command to.
 * @param[in] mod
 * The module type that the command belongs to.
 * @param[in] cmd
 * The command to be executed.
 * @param[in] buf_in
 * The input buffer containing the parameters for the command.
 * @param[out] buf_out
 * The output buffer where the detailed response from the hardware will be
 * stored.
 * @param[in] timeout
 * The timeout value (ms) to wait for the command completion. If zero, a default
 * timeout will be used.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -EBUSY: The command queue is busy.
 * - -ETIMEDOUT: The command did not complete within the specified timeout.
 */
static int
cmdq_sync_cmd_detail_resp(struct hinic3_cmdq *cmdq, enum hinic3_mod_type mod,
			  uint8_t cmd, struct hinic3_cmd_buf *buf_in,
			  struct hinic3_cmd_buf *buf_out, uint32_t timeout)
{
	struct hinic3_cmdq_wqe wqe;
	struct hinic3_wq *wq = cmdq->wq;
	struct hinic3_cmdq_wqe *curr_wqe = NULL;
	uint16_t curr_prod_idx, next_prod_idx, num_wqebbs;
	uint32_t timeo, wqe_size;
	int wrapped, err;

	wqe_size = cmdq_wqe_size(WQE_LCMD_TYPE);
	num_wqebbs = WQE_NUM_WQEBBS(wqe_size, wq);

	/* ensure thread safety and maintain wrapped and doorbell index correct. */
	rte_spinlock_lock(&cmdq->cmdq_lock);

	curr_wqe = hinic3_get_wqe(cmdq->wq, num_wqebbs, &curr_prod_idx);
	if (curr_wqe == NULL) {
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

	cmdq_set_lcmd_wqe(&wqe, SYNC_CMD_SGE_RESP, buf_in, buf_out, wrapped,
			  mod, cmd, curr_prod_idx);

	hinic3_cpu_to_hw(&wqe, wqe_size);

	/* Cmdq wqe is not shadow, therefore wqe will be written to wq. */
	cmdq_wqe_fill(curr_wqe, &wqe);

	cmdq->cmd_infos[curr_prod_idx].cmd_type = HINIC3_CMD_TYPE_SGE_RESP;

	cmdq_set_db(cmdq, cmdq->cmdq_type, next_prod_idx);

	timeo = timeout ? timeout : CMDQ_CMD_TIMEOUT;
	err = hinic3_cmdq_poll_msg(cmdq, timeo);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq poll msg ack failed, prod idx: 0x%x",
			    curr_prod_idx);
		err = -ETIMEDOUT;
		goto cmdq_unlock;
	}

	rte_smp_rmb(); /*Ensure all cmdq return messages are completed*/

	if (cmdq->errcode[curr_prod_idx])
		err = cmdq->errcode[curr_prod_idx];

cmdq_unlock:
	rte_spinlock_unlock(&cmdq->cmdq_lock);

	return err;
}

static int
cmdq_params_valid(struct hinic3_hwdev *hwdev, struct hinic3_cmd_buf *buf_in)
{
	if (!buf_in || !hwdev) {
		PMD_DRV_LOG(ERR, "Invalid CMDQ buffer or hwdev is NULL");
		return -EINVAL;
	}

	if (buf_in->size == 0 || buf_in->size > HINIC3_CMDQ_BUF_SIZE) {
		PMD_DRV_LOG(ERR, "Invalid CMDQ buffer size: 0x%x",
			    buf_in->size);
		return -EINVAL;
	}

	return 0;
}

static int
wait_cmdqs_enable(struct hinic3_cmdqs *cmdqs)
{
	uint64_t end;

	end = cycles + msecs_to_cycles(WAIT_CMDQ_ENABLE_TIMEOUT);
	do {
		if (cmdqs->status & HINIC3_CMDQ_ENABLE)
			return 0;
	} while (time_before(cycles, end));

	return -EBUSY;
}

int
hinic3_cmdq_direct_resp(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod, uint8_t cmd,
			struct hinic3_cmd_buf *buf_in, uint64_t *out_param, uint32_t timeout)
{
	struct hinic3_cmdqs *cmdqs = hwdev->cmdqs;
	int err;

	err = cmdq_params_valid(hwdev, buf_in);
	if (err) {
		PMD_DRV_LOG(ERR, "Invalid cmdq parameters");
		return err;
	}

	err = wait_cmdqs_enable(cmdqs);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq is disabled");
		return err;
	}

	return cmdq_sync_cmd_direct_resp(&cmdqs->cmdq[HINIC3_CMDQ_SYNC], mod,
					 cmd, buf_in, out_param, timeout);
}

int
hinic3_cmdq_detail_resp(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod, uint8_t cmd,
		struct hinic3_cmd_buf *buf_in, struct hinic3_cmd_buf *buf_out, uint32_t timeout)
{
	struct hinic3_cmdqs *cmdqs = hwdev->cmdqs;
	int err;

	err = cmdq_params_valid(hwdev, buf_in);
	if (err) {
		PMD_DRV_LOG(ERR, "Invalid cmdq parameters");
		return err;
	}

	err = wait_cmdqs_enable(cmdqs);
	if (err) {
		PMD_DRV_LOG(ERR, "Cmdq is disabled");
		return err;
	}

	return cmdq_sync_cmd_detail_resp(&cmdqs->cmdq[HINIC3_CMDQ_SYNC], mod,
					 cmd, buf_in, buf_out, timeout);
}

static void
cmdq_update_errcode(struct hinic3_cmdq *cmdq, uint16_t prod_idx, int errcode)
{
	cmdq->errcode[prod_idx] = errcode;
}

static void
clear_wqe_complete_bit(struct hinic3_cmdq *cmdq, struct hinic3_cmdq_wqe *wqe)
{
	struct hinic3_ctrl *ctrl = NULL;
	uint32_t header_info = hinic3_hw_cpu32(WQE_HEADER(wqe)->header_info);
	int buf_len = CMDQ_WQE_HEADER_GET(header_info, BUFDESC_LEN);
	uint32_t wqe_size = cmdq_get_wqe_size(buf_len);
	uint16_t num_wqebbs;

	if (wqe_size == WQE_LCMD_SIZE)
		ctrl = &wqe->wqe_lcmd.ctrl;
	else
		ctrl = &wqe->inline_wqe.wqe_scmd.ctrl;

	/* Clear HW busy bit. */
	ctrl->ctrl_info = 0;

	rte_atomic_thread_fence(rte_memory_order_release); /**< Verify wqe is cleared. */

	num_wqebbs = WQE_NUM_WQEBBS(wqe_size, cmdq->wq);
	hinic3_put_wqe(cmdq->wq, num_wqebbs);
}

static void
cmdq_init_queue_ctxt(struct hinic3_cmdq *cmdq,
		     struct hinic3_cmdq_ctxt_info *ctxt_info)
{
	struct hinic3_wq *wq = cmdq->wq;
	uint64_t wq_first_page_paddr, pfn;

	uint16_t start_ci = (uint16_t)(wq->cons_idx);

	/* The data in the HW is in Big Endian Format. */
	wq_first_page_paddr = wq->queue_buf_paddr;

	pfn = CMDQ_PFN(wq_first_page_paddr, RTE_PGSIZE_4K);
	ctxt_info->curr_wqe_page_pfn =
		CMDQ_CTXT_PAGE_INFO_SET(1, HW_BUSY_BIT) |
		CMDQ_CTXT_PAGE_INFO_SET(0, CEQ_EN) |
		CMDQ_CTXT_PAGE_INFO_SET(0, CEQ_ARM) |
		CMDQ_CTXT_PAGE_INFO_SET(HINIC3_CEQ_ID_CMDQ, EQ_ID) |
		CMDQ_CTXT_PAGE_INFO_SET(pfn, CURR_WQE_PAGE_PFN);

	ctxt_info->wq_block_pfn = CMDQ_CTXT_BLOCK_INFO_SET(start_ci, CI) |
				  CMDQ_CTXT_BLOCK_INFO_SET(pfn, WQ_BLOCK_PFN);
}

static int
init_cmdq(struct hinic3_cmdq *cmdq, struct hinic3_hwdev *hwdev,
	  struct hinic3_wq *wq, enum hinic3_cmdq_type q_type)
{
	int err = 0;
	size_t errcode_size;
	size_t cmd_infos_size;

	cmdq->wq = wq;
	cmdq->cmdq_type = q_type;
	cmdq->wrapped = 1;

	rte_spinlock_init(&cmdq->cmdq_lock);

	errcode_size = wq->q_depth * sizeof(*cmdq->errcode);
	cmdq->errcode = rte_zmalloc(NULL, errcode_size, 0);
	if (!cmdq->errcode) {
		PMD_DRV_LOG(ERR, "Allocate errcode for cmdq failed");
		return -ENOMEM;
	}

	cmd_infos_size = wq->q_depth * sizeof(*cmdq->cmd_infos);
	cmdq->cmd_infos = rte_zmalloc(NULL, cmd_infos_size, 0);
	if (!cmdq->cmd_infos) {
		PMD_DRV_LOG(ERR, "Allocate cmd info for cmdq failed");
		err = -ENOMEM;
		goto cmd_infos_err;
	}

	cmdq->db_base = hwdev->cmdqs->cmdqs_db_base;

	return 0;

cmd_infos_err:
	rte_free(cmdq->errcode);

	return err;
}

static void
free_cmdq(struct hinic3_cmdq *cmdq)
{
	rte_free(cmdq->cmd_infos);
	rte_free(cmdq->errcode);
}

static int
hinic3_set_cmdq_ctxts(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmdqs *cmdqs = hwdev->cmdqs;
	struct hinic3_cmd_cmdq_ctxt cmdq_ctxt;
	enum hinic3_cmdq_type cmdq_type;
	uint16_t out_size = sizeof(cmdq_ctxt);
	int err;

	for (cmdq_type = HINIC3_CMDQ_SYNC; cmdq_type < HINIC3_MAX_CMDQ_TYPES; cmdq_type++) {
		memset(&cmdq_ctxt, 0, sizeof(cmdq_ctxt));
		cmdq_ctxt.ctxt_info = cmdqs->cmdq[cmdq_type].cmdq_ctxt;
		cmdq_ctxt.func_idx = hinic3_global_func_id(hwdev);
		cmdq_ctxt.cmdq_id = cmdq_type;

		err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
					      HINIC3_MGMT_CMD_SET_CMDQ_CTXT,
					      &cmdq_ctxt, sizeof(cmdq_ctxt),
					      &cmdq_ctxt, &out_size);

		if (err || !out_size || cmdq_ctxt.status) {
			PMD_DRV_LOG(ERR,
				    "Set cmdq ctxt failed, err: %d, status: 0x%x, out_size: 0x%x",
				    err, cmdq_ctxt.status, out_size);
			return -EFAULT;
		}
	}

	cmdqs->status |= HINIC3_CMDQ_ENABLE;

	return 0;
}

int
hinic3_reinit_cmdq_ctxts(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmdqs *cmdqs = hwdev->cmdqs;
	enum hinic3_cmdq_type cmdq_type;

	for (cmdq_type = HINIC3_CMDQ_SYNC; cmdq_type < HINIC3_MAX_CMDQ_TYPES; cmdq_type++) {
		cmdqs->cmdq[cmdq_type].wrapped = 1;
		hinic3_wq_wqe_pg_clear(cmdqs->cmdq[cmdq_type].wq);
	}

	return hinic3_set_cmdq_ctxts(hwdev);
}

static int
hinic3_set_cmdqs(struct hinic3_hwdev *hwdev, struct hinic3_cmdqs *cmdqs)
{
	void *db_base = NULL;
	enum hinic3_cmdq_type type, cmdq_type;
	int err;

	err = hinic3_alloc_db_addr(hwdev, &db_base, HINIC3_DB_TYPE_CMDQ);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate doorbell address");
		goto alloc_db_err;
	}

	cmdqs->cmdqs_db_base = (uint8_t *)db_base;

	for (cmdq_type = HINIC3_CMDQ_SYNC; cmdq_type < HINIC3_MAX_CMDQ_TYPES; cmdq_type++) {
		err = init_cmdq(&cmdqs->cmdq[cmdq_type], hwdev,
				&cmdqs->saved_wqs[cmdq_type], cmdq_type);
		if (err) {
			PMD_DRV_LOG(ERR, "Initialize cmdq failed");
			goto init_cmdq_err;
		}

		cmdq_init_queue_ctxt(&cmdqs->cmdq[cmdq_type],
				     &cmdqs->cmdq[cmdq_type].cmdq_ctxt);
	}

	err = hinic3_set_cmdq_ctxts(hwdev);
	if (err)
		goto init_cmdq_err;

	return 0;

init_cmdq_err:
	for (type = HINIC3_CMDQ_SYNC; type < cmdq_type; type++)
		free_cmdq(&cmdqs->cmdq[type]);

alloc_db_err:
	hinic3_cmdq_free(cmdqs->saved_wqs, HINIC3_MAX_CMDQ_TYPES);
	return -ENOMEM;
}

int
hinic3_init_cmdqs(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmdqs *cmdqs = NULL;
	size_t saved_wqs_size;
	char cmdq_pool_name[RTE_MEMPOOL_NAMESIZE];
	int err;

	cmdqs = rte_zmalloc(NULL, sizeof(*cmdqs), 0);
	if (!cmdqs)
		return -ENOMEM;

	hwdev->cmdqs = cmdqs;
	cmdqs->hwdev = hwdev;

	saved_wqs_size = HINIC3_MAX_CMDQ_TYPES * sizeof(struct hinic3_wq);
	cmdqs->saved_wqs = rte_zmalloc(NULL, saved_wqs_size, 0);
	if (!cmdqs->saved_wqs) {
		PMD_DRV_LOG(ERR, "Allocate saved wqs failed");
		err = -ENOMEM;
		goto alloc_wqs_err;
	}

	memset(cmdq_pool_name, 0, RTE_MEMPOOL_NAMESIZE);
	snprintf(cmdq_pool_name, sizeof(cmdq_pool_name), "hinic3_cmdq_%u",
		       hwdev->port_id);

	cmdqs->cmd_buf_pool = rte_pktmbuf_pool_create(cmdq_pool_name,
		HINIC3_CMDQ_DEPTH * HINIC3_MAX_CMDQ_TYPES, 0, 0,
		HINIC3_CMDQ_BUF_SIZE, (int)rte_socket_id());
	if (!cmdqs->cmd_buf_pool) {
		PMD_DRV_LOG(ERR, "Create cmdq buffer pool failed");
		err = -ENOMEM;
		goto pool_create_err;
	}

	err = hinic3_cmdq_alloc(cmdqs->saved_wqs, hwdev, HINIC3_MAX_CMDQ_TYPES,
				HINIC3_CMDQ_WQ_BUF_SIZE, CMDQ_WQEBB_SHIFT,
				HINIC3_CMDQ_DEPTH);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate cmdq failed");
		goto cmdq_alloc_err;
	}

	err = hinic3_set_cmdqs(hwdev, cmdqs);
	if (err) {
		PMD_DRV_LOG(ERR, "set_cmdqs failed");
		goto cmdq_alloc_err;
	}
	return 0;

cmdq_alloc_err:
	rte_mempool_free(cmdqs->cmd_buf_pool);

pool_create_err:
	rte_free(cmdqs->saved_wqs);

alloc_wqs_err:
	rte_free(cmdqs);

	return err;
}

void
hinic3_free_cmdqs(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmdqs *cmdqs = hwdev->cmdqs;
	enum hinic3_cmdq_type cmdq_type = HINIC3_CMDQ_SYNC;

	cmdqs->status &= ~HINIC3_CMDQ_ENABLE;

	for (; cmdq_type < HINIC3_MAX_CMDQ_TYPES; cmdq_type++)
		free_cmdq(&cmdqs->cmdq[cmdq_type]);

	hinic3_cmdq_free(cmdqs->saved_wqs, HINIC3_MAX_CMDQ_TYPES);
	rte_mempool_free(cmdqs->cmd_buf_pool);
	rte_free(cmdqs->saved_wqs);
	rte_free(cmdqs);
}

static int
hinic3_cmdq_poll_msg(struct hinic3_cmdq *cmdq, uint32_t timeout)
{
	struct hinic3_cmdq_wqe *wqe = NULL;
	struct hinic3_cmdq_wqe_lcmd *wqe_lcmd = NULL;
	struct hinic3_ctrl *ctrl = NULL;
	struct hinic3_cmdq_cmd_info *cmd_info = NULL;
	uint32_t status_info, ctrl_info;
	uint16_t ci;
	int errcode;
	uint64_t end;
	int done = 0;
	int err = 0;

	wqe = hinic3_read_wqe(cmdq->wq, 1, &ci);
	if (!wqe) {
		PMD_DRV_LOG(ERR, "No outstanding cmdq msg");
		return -EINVAL;
	}

	cmd_info = &cmdq->cmd_infos[ci];
	if (cmd_info->cmd_type == HINIC3_CMD_TYPE_NONE) {
		PMD_DRV_LOG(ERR,
			    "Cmdq msg has not been filled and send to hw, or get TMO msg ack. cmdq ci: %u",
			    ci);
		return -EINVAL;
	}

	/* Only arm bit is using scmd wqe, the wqe is lcmd. */
	wqe_lcmd = &wqe->wqe_lcmd;
	ctrl = &wqe_lcmd->ctrl;
	end = cycles + msecs_to_cycles(timeout);
	do {
		ctrl_info = hinic3_hw_cpu32((ctrl)->ctrl_info);
		if (WQE_COMPLETED(ctrl_info)) {
			done = 1;
			break;
		}

		rte_delay_us(1);
	} while (time_before(cycles, end));

	if (done) {
		status_info = hinic3_hw_cpu32(wqe_lcmd->status.status_info);
		errcode = WQE_ERRCODE_GET(status_info, VAL);
		cmdq_update_errcode(cmdq, ci, errcode);
		clear_wqe_complete_bit(cmdq, wqe);
		err = 0;
	} else {
		PMD_DRV_LOG(ERR, "Poll cmdq msg time out, ci: %u", ci);
		err = -ETIMEDOUT;
	}

	/* Set this cmd invalid. */
	cmd_info->cmd_type = HINIC3_CMD_TYPE_NONE;

	return err;
}
