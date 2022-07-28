/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */
#include <bus_pci_driver.h>

#include "hinic_compat.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_wq.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_cmdq.h"
#include "hinic_pmd_cfg.h"
#include "hinic_pmd_niccfg.h"
#include "hinic_pmd_nicio.h"

#define WQ_PREFETCH_MAX			6
#define WQ_PREFETCH_MIN			1
#define WQ_PREFETCH_THRESHOLD		256

#define DEFAULT_RX_BUF_SIZE		((u16)0xB)

enum {
	RECYCLE_MODE_NIC = 0x0,
	RECYCLE_MODE_DPDK = 0x1,
};

/* Queue buffer related define */
enum hinic_rx_buf_size {
	HINIC_RX_BUF_SIZE_32B = 0x20,
	HINIC_RX_BUF_SIZE_64B = 0x40,
	HINIC_RX_BUF_SIZE_96B = 0x60,
	HINIC_RX_BUF_SIZE_128B = 0x80,
	HINIC_RX_BUF_SIZE_192B = 0xC0,
	HINIC_RX_BUF_SIZE_256B = 0x100,
	HINIC_RX_BUF_SIZE_384B = 0x180,
	HINIC_RX_BUF_SIZE_512B = 0x200,
	HINIC_RX_BUF_SIZE_768B = 0x300,
	HINIC_RX_BUF_SIZE_1K = 0x400,
	HINIC_RX_BUF_SIZE_1_5K = 0x600,
	HINIC_RX_BUF_SIZE_2K = 0x800,
	HINIC_RX_BUF_SIZE_3K = 0xC00,
	HINIC_RX_BUF_SIZE_4K = 0x1000,
	HINIC_RX_BUF_SIZE_8K = 0x2000,
	HINIC_RX_BUF_SIZE_16K = 0x4000,
};

const u32 hinic_hw_rx_buf_size[] = {
	HINIC_RX_BUF_SIZE_32B,
	HINIC_RX_BUF_SIZE_64B,
	HINIC_RX_BUF_SIZE_96B,
	HINIC_RX_BUF_SIZE_128B,
	HINIC_RX_BUF_SIZE_192B,
	HINIC_RX_BUF_SIZE_256B,
	HINIC_RX_BUF_SIZE_384B,
	HINIC_RX_BUF_SIZE_512B,
	HINIC_RX_BUF_SIZE_768B,
	HINIC_RX_BUF_SIZE_1K,
	HINIC_RX_BUF_SIZE_1_5K,
	HINIC_RX_BUF_SIZE_2K,
	HINIC_RX_BUF_SIZE_3K,
	HINIC_RX_BUF_SIZE_4K,
	HINIC_RX_BUF_SIZE_8K,
	HINIC_RX_BUF_SIZE_16K,
};

struct hinic_qp_ctxt_header {
	u16	num_queues;
	u16	queue_type;
	u32	addr_offset;
};

struct hinic_sq_ctxt {
	u32	ceq_attr;

	u32	ci_owner;

	u32	wq_pfn_hi;
	u32	wq_pfn_lo;

	u32	pref_cache;
	u32	pref_owner;
	u32	pref_wq_pfn_hi_ci;
	u32	pref_wq_pfn_lo;

	u32	rsvd8;
	u32	rsvd9;

	u32	wq_block_pfn_hi;
	u32	wq_block_pfn_lo;
};

struct hinic_rq_ctxt {
	u32	ceq_attr;

	u32	pi_intr_attr;

	u32	wq_pfn_hi_ci;
	u32	wq_pfn_lo;

	u32	pref_cache;
	u32	pref_owner;

	u32	pref_wq_pfn_hi_ci;
	u32	pref_wq_pfn_lo;

	u32	pi_paddr_hi;
	u32	pi_paddr_lo;

	u32	wq_block_pfn_hi;
	u32	wq_block_pfn_lo;
};

struct hinic_sq_ctxt_block {
	struct hinic_qp_ctxt_header	cmdq_hdr;
	struct hinic_sq_ctxt		sq_ctxt[HINIC_Q_CTXT_MAX];
};

struct hinic_rq_ctxt_block {
	struct hinic_qp_ctxt_header	cmdq_hdr;
	struct hinic_rq_ctxt		rq_ctxt[HINIC_Q_CTXT_MAX];
};

struct hinic_clean_queue_ctxt {
	struct hinic_qp_ctxt_header	cmdq_hdr;
	u32				ctxt_size;
};


static void
hinic_qp_prepare_cmdq_header(struct hinic_qp_ctxt_header *qp_ctxt_hdr,
			     enum hinic_qp_ctxt_type ctxt_type,
			     u16 num_queues, u16 max_queues, u16 q_id)
{
	qp_ctxt_hdr->queue_type = ctxt_type;
	qp_ctxt_hdr->num_queues = num_queues;

	if (ctxt_type == HINIC_QP_CTXT_TYPE_SQ)
		qp_ctxt_hdr->addr_offset =
				SQ_CTXT_OFFSET(max_queues, max_queues, q_id);
	else
		qp_ctxt_hdr->addr_offset =
				RQ_CTXT_OFFSET(max_queues, max_queues, q_id);

	qp_ctxt_hdr->addr_offset = SIZE_16BYTES(qp_ctxt_hdr->addr_offset);

	hinic_cpu_to_be32(qp_ctxt_hdr, sizeof(*qp_ctxt_hdr));
}

static void hinic_sq_prepare_ctxt(struct hinic_sq *sq, u16 global_qpn,
			   struct hinic_sq_ctxt *sq_ctxt)
{
	struct hinic_wq *wq = sq->wq;
	u64 wq_page_addr;
	u64 wq_page_pfn, wq_block_pfn;
	u32 wq_page_pfn_hi, wq_page_pfn_lo;
	u32 wq_block_pfn_hi, wq_block_pfn_lo;
	u16 pi_start, ci_start;

	ci_start = (u16)(wq->cons_idx);
	pi_start = (u16)(wq->prod_idx);

	/* read the first page from the HW table */
	wq_page_addr = wq->queue_buf_paddr;

	wq_page_pfn = WQ_PAGE_PFN(wq_page_addr);
	wq_page_pfn_hi = upper_32_bits(wq_page_pfn);
	wq_page_pfn_lo = lower_32_bits(wq_page_pfn);

	wq_block_pfn = WQ_BLOCK_PFN(wq_page_addr);
	wq_block_pfn_hi = upper_32_bits(wq_block_pfn);
	wq_block_pfn_lo = lower_32_bits(wq_block_pfn);

	/* must config as ceq disabled */
	sq_ctxt->ceq_attr = SQ_CTXT_CEQ_ATTR_SET(global_qpn, GLOBAL_SQ_ID) |
				SQ_CTXT_CEQ_ATTR_SET(0, ARM) |
				SQ_CTXT_CEQ_ATTR_SET(0, CEQ_ID) |
				SQ_CTXT_CEQ_ATTR_SET(0, EN);

	sq_ctxt->ci_owner = SQ_CTXT_CI_SET(ci_start, IDX) |
				SQ_CTXT_CI_SET(1, OWNER);

	sq_ctxt->wq_pfn_hi =
			SQ_CTXT_WQ_PAGE_SET(wq_page_pfn_hi, HI_PFN) |
			SQ_CTXT_WQ_PAGE_SET(pi_start, PI);

	sq_ctxt->wq_pfn_lo = wq_page_pfn_lo;

	sq_ctxt->pref_cache =
		SQ_CTXT_PREF_SET(WQ_PREFETCH_MIN, CACHE_MIN) |
		SQ_CTXT_PREF_SET(WQ_PREFETCH_MAX, CACHE_MAX) |
		SQ_CTXT_PREF_SET(WQ_PREFETCH_THRESHOLD, CACHE_THRESHOLD);

	sq_ctxt->pref_owner = 1;

	sq_ctxt->pref_wq_pfn_hi_ci =
		SQ_CTXT_PREF_SET(ci_start, CI) |
		SQ_CTXT_PREF_SET(wq_page_pfn_hi, WQ_PFN_HI);

	sq_ctxt->pref_wq_pfn_lo = wq_page_pfn_lo;

	sq_ctxt->wq_block_pfn_hi =
		SQ_CTXT_WQ_BLOCK_SET(wq_block_pfn_hi, PFN_HI);

	sq_ctxt->wq_block_pfn_lo = wq_block_pfn_lo;

	hinic_cpu_to_be32(sq_ctxt, sizeof(*sq_ctxt));
}

static void hinic_rq_prepare_ctxt(struct hinic_rq *rq,
			struct hinic_rq_ctxt *rq_ctxt)
{
	struct hinic_wq *wq = rq->wq;
	u64 wq_page_addr;
	u64 wq_page_pfn, wq_block_pfn;
	u32 wq_page_pfn_hi, wq_page_pfn_lo;
	u32 wq_block_pfn_hi, wq_block_pfn_lo;
	u16 pi_start, ci_start;

	ci_start = (u16)(wq->cons_idx);
	pi_start = (u16)(wq->prod_idx);

	/* read the first page from the HW table */
	wq_page_addr = wq->queue_buf_paddr;

	wq_page_pfn = WQ_PAGE_PFN(wq_page_addr);
	wq_page_pfn_hi = upper_32_bits(wq_page_pfn);
	wq_page_pfn_lo = lower_32_bits(wq_page_pfn);

	wq_block_pfn = WQ_BLOCK_PFN(wq_page_addr);
	wq_block_pfn_hi = upper_32_bits(wq_block_pfn);
	wq_block_pfn_lo = lower_32_bits(wq_block_pfn);

	/* config as ceq disable, but must set msix state disable */
	rq_ctxt->ceq_attr = RQ_CTXT_CEQ_ATTR_SET(0, EN) |
			    RQ_CTXT_CEQ_ATTR_SET(1, OWNER);

	rq_ctxt->pi_intr_attr = RQ_CTXT_PI_SET(pi_start, IDX) |
				RQ_CTXT_PI_SET(rq->msix_entry_idx, INTR) |
				RQ_CTXT_PI_SET(0, CEQ_ARM);

	rq_ctxt->wq_pfn_hi_ci = RQ_CTXT_WQ_PAGE_SET(wq_page_pfn_hi, HI_PFN) |
				RQ_CTXT_WQ_PAGE_SET(ci_start, CI);

	rq_ctxt->wq_pfn_lo = wq_page_pfn_lo;

	rq_ctxt->pref_cache =
		RQ_CTXT_PREF_SET(WQ_PREFETCH_MIN, CACHE_MIN) |
		RQ_CTXT_PREF_SET(WQ_PREFETCH_MAX, CACHE_MAX) |
		RQ_CTXT_PREF_SET(WQ_PREFETCH_THRESHOLD, CACHE_THRESHOLD);

	rq_ctxt->pref_owner = 1;

	rq_ctxt->pref_wq_pfn_hi_ci =
		RQ_CTXT_PREF_SET(wq_page_pfn_hi, WQ_PFN_HI) |
		RQ_CTXT_PREF_SET(ci_start, CI);

	rq_ctxt->pref_wq_pfn_lo = wq_page_pfn_lo;

	rq_ctxt->pi_paddr_hi = upper_32_bits(rq->pi_dma_addr);
	rq_ctxt->pi_paddr_lo = lower_32_bits(rq->pi_dma_addr);

	rq_ctxt->wq_block_pfn_hi =
		RQ_CTXT_WQ_BLOCK_SET(wq_block_pfn_hi, PFN_HI);

	rq_ctxt->wq_block_pfn_lo = wq_block_pfn_lo;

	hinic_cpu_to_be32(rq_ctxt, sizeof(*rq_ctxt));
}

static int init_sq_ctxts(struct hinic_nic_io *nic_io)
{
	struct hinic_hwdev *hwdev = nic_io->hwdev;
	struct hinic_sq_ctxt_block *sq_ctxt_block;
	struct hinic_sq_ctxt *sq_ctxt;
	struct hinic_cmd_buf *cmd_buf;
	struct hinic_qp *qp;
	u64 out_param = EIO;
	u16 q_id, curr_id, global_qpn, max_ctxts, i;
	int err = 0;

	cmd_buf = hinic_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd buf");
		return -ENOMEM;
	}

	q_id = 0;
	/* sq and rq number may not equal */
	while (q_id < nic_io->num_sqs) {
		sq_ctxt_block = cmd_buf->buf;
		sq_ctxt = sq_ctxt_block->sq_ctxt;

		max_ctxts = (nic_io->num_sqs - q_id) > HINIC_Q_CTXT_MAX ?
				HINIC_Q_CTXT_MAX : (nic_io->num_sqs - q_id);

		hinic_qp_prepare_cmdq_header(&sq_ctxt_block->cmdq_hdr,
					     HINIC_QP_CTXT_TYPE_SQ, max_ctxts,
					     nic_io->max_qps, q_id);

		for (i = 0; i < max_ctxts; i++) {
			curr_id = q_id + i;
			qp = &nic_io->qps[curr_id];
			global_qpn = nic_io->global_qpn + curr_id;

			hinic_sq_prepare_ctxt(&qp->sq, global_qpn, &sq_ctxt[i]);
		}

		cmd_buf->size = SQ_CTXT_SIZE(max_ctxts);

		err = hinic_cmdq_direct_resp(hwdev, HINIC_ACK_TYPE_CMDQ,
					     HINIC_MOD_L2NIC,
					     HINIC_UCODE_CMD_MDY_QUEUE_CONTEXT,
					     cmd_buf, &out_param, 0);
		if (err || out_param != 0) {
			PMD_DRV_LOG(ERR, "Failed to set SQ ctxts, err: %d",
				err);
			err = -EIO;
			break;
		}

		q_id += max_ctxts;
	}

	hinic_free_cmd_buf(hwdev, cmd_buf);

	return err;
}

static int init_rq_ctxts(struct hinic_nic_io *nic_io)
{
	struct hinic_hwdev *hwdev = nic_io->hwdev;
	struct hinic_rq_ctxt_block *rq_ctxt_block;
	struct hinic_rq_ctxt *rq_ctxt;
	struct hinic_cmd_buf *cmd_buf;
	struct hinic_qp *qp;
	u64 out_param = 0;
	u16 q_id, curr_id, max_ctxts, i;
	int err = 0;

	cmd_buf = hinic_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd buf");
		return -ENOMEM;
	}

	q_id = 0;
	/* sq and rq number may not equal */
	while (q_id < nic_io->num_rqs) {
		rq_ctxt_block = cmd_buf->buf;
		rq_ctxt = rq_ctxt_block->rq_ctxt;

		max_ctxts = (nic_io->num_rqs - q_id) > HINIC_Q_CTXT_MAX ?
				HINIC_Q_CTXT_MAX : (nic_io->num_rqs - q_id);

		hinic_qp_prepare_cmdq_header(&rq_ctxt_block->cmdq_hdr,
					     HINIC_QP_CTXT_TYPE_RQ, max_ctxts,
					     nic_io->max_qps, q_id);

		for (i = 0; i < max_ctxts; i++) {
			curr_id = q_id + i;
			qp = &nic_io->qps[curr_id];

			hinic_rq_prepare_ctxt(&qp->rq, &rq_ctxt[i]);
		}

		cmd_buf->size = RQ_CTXT_SIZE(max_ctxts);

		err = hinic_cmdq_direct_resp(hwdev, HINIC_ACK_TYPE_CMDQ,
					     HINIC_MOD_L2NIC,
					     HINIC_UCODE_CMD_MDY_QUEUE_CONTEXT,
					     cmd_buf, &out_param, 0);
		if ((err) || out_param != 0) {
			PMD_DRV_LOG(ERR, "Failed to set RQ ctxts");
			err = -EIO;
			break;
		}

		q_id += max_ctxts;
	}

	hinic_free_cmd_buf(hwdev, cmd_buf);

	return err;
}

static int init_qp_ctxts(struct hinic_nic_io *nic_io)
{
	return (init_sq_ctxts(nic_io) || init_rq_ctxts(nic_io));
}

static int clean_queue_offload_ctxt(struct hinic_nic_io *nic_io,
				    enum hinic_qp_ctxt_type ctxt_type)
{
	struct hinic_hwdev *hwdev = nic_io->hwdev;
	struct hinic_clean_queue_ctxt *ctxt_block;
	struct hinic_cmd_buf *cmd_buf;
	u64 out_param = 0;
	int err;

	cmd_buf = hinic_alloc_cmd_buf(hwdev);
	if (!cmd_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate cmd buf");
		return -ENOMEM;
	}

	ctxt_block = cmd_buf->buf;
	ctxt_block->cmdq_hdr.num_queues = nic_io->max_qps;
	ctxt_block->cmdq_hdr.queue_type = ctxt_type;
	ctxt_block->cmdq_hdr.addr_offset = 0;

	/* TSO/LRO ctxt size: 0x0:0B; 0x1:160B; 0x2:200B; 0x3:240B */
	ctxt_block->ctxt_size = 0x3;

	hinic_cpu_to_be32(ctxt_block, sizeof(*ctxt_block));

	cmd_buf->size = sizeof(*ctxt_block);

	err = hinic_cmdq_direct_resp(hwdev, HINIC_ACK_TYPE_CMDQ,
				     HINIC_MOD_L2NIC,
				     HINIC_UCODE_CMD_CLEAN_QUEUE_CONTEXT,
				     cmd_buf, &out_param, 0);

	if ((err) || (out_param)) {
		PMD_DRV_LOG(ERR, "Failed to clean queue offload ctxts");
		err = -EIO;
	}

	hinic_free_cmd_buf(hwdev, cmd_buf);

	return err;
}

static int clean_qp_offload_ctxt(struct hinic_nic_io *nic_io)
{
	/* clean LRO/TSO context space */
	return (clean_queue_offload_ctxt(nic_io, HINIC_QP_CTXT_TYPE_SQ) ||
		clean_queue_offload_ctxt(nic_io, HINIC_QP_CTXT_TYPE_RQ));
}

/**
 * get_hw_rx_buf_size - translate rx_buf_size into hw_rx_buf_size
 * @rx_buf_sz: receive buffer size
 * @return
 *   hw rx buffer size
 */
static u16 get_hw_rx_buf_size(u32 rx_buf_sz)
{
	u16 num_hw_types = sizeof(hinic_hw_rx_buf_size)
			   / sizeof(hinic_hw_rx_buf_size[0]);
	u16 i;

	for (i = 0; i < num_hw_types; i++) {
		if (hinic_hw_rx_buf_size[i] == rx_buf_sz)
			return i;
	}

	PMD_DRV_LOG(ERR, "Hw can't support rx buf size of %u", rx_buf_sz);

	return DEFAULT_RX_BUF_SIZE;	/* default 2K */
}

/**
 * hinic_set_root_ctxt - init root context in NIC
 * @hwdev: the hardware interface of a nic device
 * @rq_depth: the depth of receive queue
 * @sq_depth: the depth of transmit queue
 * @rx_buf_sz: receive buffer size from app
 * Return: 0 on success, negative error value otherwise.
 */
static int
hinic_set_root_ctxt(void *hwdev, u16 rq_depth, u16 sq_depth, int rx_buf_sz)
{
	struct hinic_root_ctxt root_ctxt;
	u16 out_size = sizeof(root_ctxt);
	int err;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	root_ctxt.func_idx = hinic_global_func_id(hwdev);
	root_ctxt.ppf_idx = hinic_ppf_idx(hwdev);
	root_ctxt.set_cmdq_depth = 0;
	root_ctxt.cmdq_depth = 0;
	root_ctxt.lro_en = 1;
	root_ctxt.rq_depth  = (u16)ilog2(rq_depth);
	root_ctxt.rx_buf_sz = get_hw_rx_buf_size(rx_buf_sz);
	root_ctxt.sq_depth  = (u16)ilog2(sq_depth);

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_VAT_SET,
				     &root_ctxt, sizeof(root_ctxt),
				     &root_ctxt, &out_size, 0);
	if (err || !out_size || root_ctxt.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Set root context failed, err: %d, status: 0x%x, out_size: 0x%x",
			err, root_ctxt.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * hinic_clean_root_ctxt - clean root context table in NIC
 * @hwdev: the hardware interface of a nic device
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
static int hinic_clean_root_ctxt(void *hwdev)
{
	struct hinic_root_ctxt root_ctxt;
	u16 out_size = sizeof(root_ctxt);
	int err;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	root_ctxt.func_idx = hinic_global_func_id(hwdev);
	root_ctxt.ppf_idx = hinic_ppf_idx(hwdev);
	root_ctxt.set_cmdq_depth = 0;
	root_ctxt.cmdq_depth = 0;
	root_ctxt.lro_en = 0;
	root_ctxt.rq_depth  = 0;
	root_ctxt.rx_buf_sz = 0;
	root_ctxt.sq_depth  = 0;

	err = hinic_msg_to_mgmt_sync(hwdev, HINIC_MOD_COMM,
				     HINIC_MGMT_CMD_VAT_SET,
				     &root_ctxt, sizeof(root_ctxt),
				     &root_ctxt, &out_size, 0);
	if (err || !out_size || root_ctxt.mgmt_msg_head.status) {
		PMD_DRV_LOG(ERR,
			"Clean root context failed, err: %d, status: 0x%x, out_size: 0x%x",
			err, root_ctxt.mgmt_msg_head.status, out_size);
		return -EIO;
	}

	return 0;
}

/* init qps ctxt and set sq ci attr and arm all sq and set vat page_size */
int hinic_init_qp_ctxts(struct hinic_hwdev *hwdev)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_sq_attr sq_attr;
	u16 q_id;
	int err, rx_buf_sz;

	/* set vat page size to max queue depth page_size */
	err = hinic_set_pagesize(hwdev, HINIC_PAGE_SIZE_DPDK);
	if (err != HINIC_OK) {
		PMD_DRV_LOG(ERR, "Set vat page size: %d failed, rc: %d",
			HINIC_PAGE_SIZE_DPDK, err);
		return err;
	}

	if (hwdev->cmdqs->status & HINIC_CMDQ_SET_FAIL) {
		err = hinic_reinit_cmdq_ctxts(hwdev);
		if (err) {
			PMD_DRV_LOG(ERR, "Reinit cmdq context failed when dev start, err: %d",
				err);
			return err;
		}
	}

	err = init_qp_ctxts(nic_io);
	if (err) {
		PMD_DRV_LOG(ERR, "Init QP ctxts failed, rc: %d", err);
		return err;
	}

	/* clean LRO/TSO context space */
	err = clean_qp_offload_ctxt(nic_io);
	if (err) {
		PMD_DRV_LOG(ERR, "Clean qp offload ctxts failed, rc: %d", err);
		return err;
	}

	rx_buf_sz = nic_io->rq_buf_size;

	/* update rx buf size to function table */
	err = hinic_set_rx_vhd_mode(hwdev, HINIC_VHD_TYPE_0B, rx_buf_sz);
	if (err) {
		PMD_DRV_LOG(ERR, "Set rx vhd mode failed, rc: %d", err);
		return err;
	}

	err = hinic_set_root_ctxt(hwdev, nic_io->rq_depth,
				  nic_io->sq_depth, rx_buf_sz);
	if (err) {
		PMD_DRV_LOG(ERR, "Set root context failed, rc: %d", err);
		return err;
	}

	for (q_id = 0; q_id < nic_io->num_sqs; q_id++) {
		sq_attr.ci_dma_base =
			HINIC_CI_PADDR(nic_io->ci_dma_base, q_id) >> 2;
		/* performance: sq ci update threshold as 8 */
		sq_attr.pending_limit = 1;
		sq_attr.coalescing_time = 1;
		sq_attr.intr_en = 0;
		sq_attr.l2nic_sqn = q_id;
		sq_attr.dma_attr_off = 0;
		err = hinic_set_ci_table(hwdev, q_id, &sq_attr);
		if (err) {
			PMD_DRV_LOG(ERR, "Set ci table failed, rc: %d", err);
			goto set_cons_idx_table_err;
		}
	}

	return 0;

set_cons_idx_table_err:
	(void)hinic_clean_root_ctxt(hwdev);
	return err;
}

void hinic_free_qp_ctxts(struct hinic_hwdev *hwdev)
{
	int err;

	err = hinic_clean_root_ctxt(hwdev);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to clean root ctxt");
}

static int hinic_init_nic_hwdev(struct hinic_hwdev *hwdev)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	u16 global_qpn, rx_buf_sz;
	int err;

	err = hinic_get_base_qpn(hwdev, &global_qpn);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get base qpn");
		goto err_init_nic_hwdev;
	}

	nic_io->global_qpn = global_qpn;
	rx_buf_sz = HINIC_IS_VF(hwdev) ? RX_BUF_LEN_1_5K : RX_BUF_LEN_16K;
	err = hinic_init_function_table(hwdev, rx_buf_sz);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init function table");
		goto err_init_nic_hwdev;
	}

	err = hinic_vf_func_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init nic mbox");
		goto err_init_nic_hwdev;
	}

	err = hinic_set_fast_recycle_mode(hwdev, RECYCLE_MODE_DPDK);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to set fast recycle mode");
		goto err_init_nic_hwdev;
	}

	return 0;

err_init_nic_hwdev:
	return err;
}

static void hinic_free_nic_hwdev(struct hinic_hwdev *hwdev)
{
	hinic_vf_func_free(hwdev);
}

int hinic_rx_tx_flush(struct hinic_hwdev *hwdev)
{
	return hinic_func_rx_tx_flush(hwdev);
}

int hinic_get_sq_free_wqebbs(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_wq *wq = &nic_io->sq_wq[q_id];

	return (wq->delta) - 1;
}

int hinic_get_rq_free_wqebbs(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_wq *wq = &nic_io->rq_wq[q_id];

	return (wq->delta) - 1;
}

u16 hinic_get_sq_local_ci(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_wq *wq = &nic_io->sq_wq[q_id];

	return (wq->cons_idx) & wq->mask;
}

void hinic_return_sq_wqe(struct hinic_hwdev *hwdev, u16 q_id,
			 int num_wqebbs, u16 owner)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_sq *sq = &nic_io->qps[q_id].sq;

	if (owner != sq->owner)
		sq->owner = owner;

	sq->wq->delta += num_wqebbs;
	sq->wq->prod_idx -= num_wqebbs;
}

void hinic_update_sq_local_ci(struct hinic_hwdev *hwdev,
			      u16 q_id, int wqebb_cnt)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_sq *sq = &nic_io->qps[q_id].sq;

	hinic_put_wqe(sq->wq, wqebb_cnt);
}

void *hinic_get_rq_wqe(struct hinic_hwdev *hwdev, u16 q_id, u16 *pi)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_rq *rq = &nic_io->qps[q_id].rq;

	return hinic_get_wqe(rq->wq, 1, pi);
}

void hinic_return_rq_wqe(struct hinic_hwdev *hwdev, u16 q_id, int num_wqebbs)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_rq *rq = &nic_io->qps[q_id].rq;

	rq->wq->delta += num_wqebbs;
	rq->wq->prod_idx -= num_wqebbs;
}

u16 hinic_get_rq_local_ci(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_wq *wq = &nic_io->rq_wq[q_id];

	return (wq->cons_idx) & wq->mask;
}

void hinic_update_rq_local_ci(struct hinic_hwdev *hwdev, u16 q_id, int wqe_cnt)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_rq *rq = &nic_io->qps[q_id].rq;

	hinic_put_wqe(rq->wq, wqe_cnt);
}

static int hinic_alloc_nicio(struct hinic_hwdev *hwdev)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct rte_pci_device *pdev = hwdev->pcidev_hdl;
	u16 max_qps, num_qp;
	int err;

	max_qps = hinic_func_max_qnum(hwdev);

	nic_io->max_qps = max_qps;
	nic_io->num_qps = max_qps;
	num_qp = max_qps;

	nic_io->qps = kzalloc_aligned(num_qp * sizeof(*nic_io->qps),
				      GFP_KERNEL);
	if (!nic_io->qps) {
		PMD_DRV_LOG(ERR, "Failed to allocate qps");
		err = -ENOMEM;
		goto alloc_qps_err;
	}

	nic_io->ci_vaddr_base = dma_zalloc_coherent(hwdev,
				    CI_TABLE_SIZE(num_qp, HINIC_PAGE_SIZE),
				    &nic_io->ci_dma_base,
				    pdev->device.numa_node);
	if (!nic_io->ci_vaddr_base) {
		PMD_DRV_LOG(ERR, "Failed to allocate ci area");
		err = -ENOMEM;
		goto ci_base_err;
	}

	nic_io->sq_wq = kzalloc_aligned(num_qp * sizeof(*nic_io->sq_wq),
					GFP_KERNEL);
	if (!nic_io->sq_wq) {
		PMD_DRV_LOG(ERR, "Failed to allocate sq wq array");
		err = -ENOMEM;
		goto sq_wq_err;
	}

	nic_io->rq_wq = kzalloc_aligned(num_qp * sizeof(*nic_io->rq_wq),
					GFP_KERNEL);
	if (!nic_io->rq_wq) {
		PMD_DRV_LOG(ERR, "Failed to allocate rq wq array");
		err = -ENOMEM;
		goto rq_wq_err;
	}

	return HINIC_OK;

rq_wq_err:
	kfree(nic_io->sq_wq);

sq_wq_err:
	dma_free_coherent(hwdev, CI_TABLE_SIZE(num_qp, HINIC_PAGE_SIZE),
			  nic_io->ci_vaddr_base, nic_io->ci_dma_base);

ci_base_err:
	kfree(nic_io->qps);

alloc_qps_err:
	return err;
}

static void hinic_free_nicio(struct hinic_hwdev *hwdev)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;

	/* nic_io->rq_wq */
	kfree(nic_io->rq_wq);

	/* nic_io->sq_wq */
	kfree(nic_io->sq_wq);

	/* nic_io->ci_vaddr_base */
	dma_free_coherent(hwdev,
			  CI_TABLE_SIZE(nic_io->max_qps, HINIC_PAGE_SIZE),
			  nic_io->ci_vaddr_base, nic_io->ci_dma_base);

	/* nic_io->qps */
	kfree(nic_io->qps);
}

/* alloc nic hwdev and init function table */
int hinic_init_nicio(struct hinic_hwdev *hwdev)
{
	int rc;

	hwdev->nic_io = rte_zmalloc("hinic_nicio", sizeof(*hwdev->nic_io),
				      RTE_CACHE_LINE_SIZE);
	if (!hwdev->nic_io) {
		PMD_DRV_LOG(ERR, "Allocate nic_io failed, dev_name: %s",
			    hwdev->pcidev_hdl->name);
		return -ENOMEM;
	}
	hwdev->nic_io->hwdev = hwdev;

	/* alloc root working queue set */
	rc = hinic_alloc_nicio(hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Allocate nic_io failed, dev_name: %s",
			    hwdev->pcidev_hdl->name);
		goto allc_nicio_fail;
	}

	rc = hinic_init_nic_hwdev(hwdev);
	if (rc) {
		PMD_DRV_LOG(ERR, "Initialize hwdev failed, dev_name: %s",
			    hwdev->pcidev_hdl->name);
		goto init_nic_hwdev_fail;
	}

	return 0;

init_nic_hwdev_fail:
	hinic_free_nicio(hwdev);

allc_nicio_fail:
	rte_free(hwdev->nic_io);
	return rc;
}

void hinic_deinit_nicio(struct hinic_hwdev *hwdev)
{
	hinic_free_nicio(hwdev);

	hinic_free_nic_hwdev(hwdev);

	rte_free(hwdev->nic_io);
	hwdev->nic_io = NULL;
}

/**
 * hinic_convert_rx_buf_size - convert rx buffer size to hw size
 * @rx_buf_sz: receive buffer size of mbuf
 * @match_sz: receive buffer size of hardware
 * @return
 *   0 on success,
 *   negative error value otherwise.
 */
int hinic_convert_rx_buf_size(u32 rx_buf_sz, u32 *match_sz)
{
	u32 i, num_hw_types, best_match_sz;

	if (unlikely(!match_sz || rx_buf_sz < HINIC_RX_BUF_SIZE_32B))
		return -EINVAL;

	if (rx_buf_sz >= HINIC_RX_BUF_SIZE_16K) {
		best_match_sz =  HINIC_RX_BUF_SIZE_16K;
		goto size_matched;
	}

	num_hw_types = sizeof(hinic_hw_rx_buf_size) /
		sizeof(hinic_hw_rx_buf_size[0]);
	best_match_sz = hinic_hw_rx_buf_size[0];
	for (i = 0; i < num_hw_types; i++) {
		if (rx_buf_sz == hinic_hw_rx_buf_size[i]) {
			best_match_sz = hinic_hw_rx_buf_size[i];
			break;
		} else if (rx_buf_sz < hinic_hw_rx_buf_size[i]) {
			break;
		}
		best_match_sz = hinic_hw_rx_buf_size[i];
	}

size_matched:
	*match_sz = best_match_sz;

	return 0;
}
