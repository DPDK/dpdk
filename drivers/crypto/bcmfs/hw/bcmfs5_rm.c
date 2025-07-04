/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#include <unistd.h>

#include <rte_bitmap.h>

#include "bcmfs_qp.h"
#include "bcmfs_logs.h"
#include "bcmfs_dev_msg.h"
#include "bcmfs_device.h"
#include "bcmfs_hw_defs.h"
#include "bcmfs_rm_common.h"

/* Ring version */
#define RING_VER_MAGIC					0x76303032

/* Per-Ring register offsets */
#define RING_VER					0x000
#define RING_BD_START_ADDRESS_LSB			0x004
#define RING_BD_READ_PTR				0x008
#define RING_BD_WRITE_PTR				0x00c
#define RING_BD_READ_PTR_DDR_LS				0x010
#define RING_BD_READ_PTR_DDR_MS				0x014
#define RING_CMPL_START_ADDR_LSB			0x018
#define RING_CMPL_WRITE_PTR				0x01c
#define RING_NUM_REQ_RECV_LS				0x020
#define RING_NUM_REQ_RECV_MS				0x024
#define RING_NUM_REQ_TRANS_LS				0x028
#define RING_NUM_REQ_TRANS_MS				0x02c
#define RING_NUM_REQ_OUTSTAND				0x030
#define RING_CONTROL					0x034
#define RING_FLUSH_DONE					0x038
#define RING_MSI_ADDR_LS				0x03c
#define RING_MSI_ADDR_MS				0x040
#define RING_MSI_CONTROL				0x048
#define RING_BD_READ_PTR_DDR_CONTROL			0x04c
#define RING_MSI_DATA_VALUE				0x064
#define RING_BD_START_ADDRESS_MSB			0x078
#define RING_CMPL_START_ADDR_MSB			0x07c
#define RING_DOORBELL_BD_WRITE_COUNT			0x074

/* Register RING_BD_START_ADDR fields */
#define BD_LAST_UPDATE_HW_SHIFT				28
#define BD_LAST_UPDATE_HW_MASK				0x1
#define BD_START_ADDR_VALUE(pa)				\
	((uint32_t)((((uint64_t)(pa)) >> RING_BD_ALIGN_ORDER) & 0x0fffffff))
#define BD_START_ADDR_DECODE(val)			\
	((uint64_t)((val) & 0x0fffffff) << RING_BD_ALIGN_ORDER)

/* Register RING_CMPL_START_ADDR fields */
#define CMPL_START_ADDR_VALUE(pa)			\
	((uint32_t)((((uint64_t)(pa)) >> RING_CMPL_ALIGN_ORDER) & 0x07ffffff))

/* Register RING_CONTROL fields */
#define CONTROL_MASK_DISABLE_CONTROL			12
#define CONTROL_FLUSH_SHIFT				5
#define CONTROL_ACTIVE_SHIFT				4
#define CONTROL_RATE_ADAPT_MASK				0xf
#define CONTROL_RATE_DYNAMIC				0x0
#define CONTROL_RATE_FAST				0x8
#define CONTROL_RATE_MEDIUM				0x9
#define CONTROL_RATE_SLOW				0xa
#define CONTROL_RATE_IDLE				0xb

/* Register RING_FLUSH_DONE fields */
#define FLUSH_DONE_MASK					0x1

/* Register RING_MSI_CONTROL fields */
#define MSI_TIMER_VAL_SHIFT				16
#define MSI_TIMER_VAL_MASK				0xffff
#define MSI_ENABLE_SHIFT				15
#define MSI_ENABLE_MASK					0x1
#define MSI_COUNT_SHIFT					0
#define MSI_COUNT_MASK					0x3ff

/* Register RING_BD_READ_PTR_DDR_CONTROL fields */
#define BD_READ_PTR_DDR_TIMER_VAL_SHIFT			16
#define BD_READ_PTR_DDR_TIMER_VAL_MASK			0xffff
#define BD_READ_PTR_DDR_ENABLE_SHIFT			15
#define BD_READ_PTR_DDR_ENABLE_MASK			0x1

/* General descriptor format */
#define DESC_TYPE_SHIFT					60
#define DESC_TYPE_MASK					0xf
#define DESC_PAYLOAD_SHIFT				0
#define DESC_PAYLOAD_MASK				0x0fffffffffffffff

/* Null descriptor format  */
#define NULL_TYPE					0
#define NULL_TOGGLE_SHIFT				59
#define NULL_TOGGLE_MASK				0x1

/* Header descriptor format */
#define HEADER_TYPE					1
#define HEADER_TOGGLE_SHIFT				59
#define HEADER_TOGGLE_MASK				0x1
#define HEADER_ENDPKT_SHIFT				57
#define HEADER_ENDPKT_MASK				0x1
#define HEADER_STARTPKT_SHIFT				56
#define HEADER_STARTPKT_MASK				0x1
#define HEADER_BDCOUNT_SHIFT				36
#define HEADER_BDCOUNT_MASK				0x1f
#define HEADER_BDCOUNT_MAX				HEADER_BDCOUNT_MASK
#define HEADER_FLAGS_SHIFT				16
#define HEADER_FLAGS_MASK				0xffff
#define HEADER_OPAQUE_SHIFT				0
#define HEADER_OPAQUE_MASK				0xffff

/* Source (SRC) descriptor format */

#define SRC_TYPE					2
#define SRC_LENGTH_SHIFT				44
#define SRC_LENGTH_MASK					0xffff
#define SRC_ADDR_SHIFT					0
#define SRC_ADDR_MASK					0x00000fffffffffff

/* Destination (DST) descriptor format */
#define DST_TYPE					3
#define DST_LENGTH_SHIFT				44
#define DST_LENGTH_MASK					0xffff
#define DST_ADDR_SHIFT					0
#define DST_ADDR_MASK					0x00000fffffffffff

/* Next pointer (NPTR) descriptor format */
#define NPTR_TYPE					5
#define NPTR_TOGGLE_SHIFT				59
#define NPTR_TOGGLE_MASK				0x1
#define NPTR_ADDR_SHIFT					0
#define NPTR_ADDR_MASK					0x00000fffffffffff

/* Mega source (MSRC) descriptor format */
#define MSRC_TYPE					6
#define MSRC_LENGTH_SHIFT				44
#define MSRC_LENGTH_MASK				0xffff
#define MSRC_ADDR_SHIFT					0
#define MSRC_ADDR_MASK					0x00000fffffffffff

/* Mega destination (MDST) descriptor format */
#define MDST_TYPE					7
#define MDST_LENGTH_SHIFT				44
#define MDST_LENGTH_MASK				0xffff
#define MDST_ADDR_SHIFT					0
#define MDST_ADDR_MASK					0x00000fffffffffff

static uint8_t
bcmfs5_is_next_table_desc(void *desc_ptr)
{
	uint64_t desc = rm_read_desc(desc_ptr);
	uint32_t type = FS_DESC_DEC(desc, DESC_TYPE_SHIFT, DESC_TYPE_MASK);

	return (type == NPTR_TYPE) ? true : false;
}

static uint64_t
bcmfs5_next_table_desc(uint64_t next_addr)
{
	return (rm_build_desc(NPTR_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK) |
		rm_build_desc(next_addr, NPTR_ADDR_SHIFT, NPTR_ADDR_MASK));
}

static uint64_t
bcmfs5_null_desc(void)
{
	return rm_build_desc(NULL_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK);
}

static uint64_t
bcmfs5_header_desc(uint32_t startpkt, uint32_t endpkt,
				       uint32_t bdcount, uint32_t flags,
				       uint32_t opaque)
{
	return (rm_build_desc(HEADER_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK) |
		rm_build_desc(startpkt, HEADER_STARTPKT_SHIFT,
			      HEADER_STARTPKT_MASK) |
		rm_build_desc(endpkt, HEADER_ENDPKT_SHIFT, HEADER_ENDPKT_MASK) |
		rm_build_desc(bdcount, HEADER_BDCOUNT_SHIFT, HEADER_BDCOUNT_MASK) |
		rm_build_desc(flags, HEADER_FLAGS_SHIFT, HEADER_FLAGS_MASK) |
		rm_build_desc(opaque, HEADER_OPAQUE_SHIFT, HEADER_OPAQUE_MASK));
}

static int
bcmfs5_enqueue_desc(uint32_t nhpos, uint32_t nhcnt,
		    uint32_t reqid, uint64_t desc,
		    void **desc_ptr, void *start_desc,
		    void *end_desc)
{
	uint64_t d;
	uint32_t nhavail, _startpkt, _endpkt, _bdcount;
	int is_nxt_page = 0;

	/*
	 * Each request or packet start with a HEADER descriptor followed
	 * by one or more non-HEADER descriptors (SRC, SRCT, MSRC, DST,
	 * DSTT, MDST, IMM, and IMMT). The number of non-HEADER descriptors
	 * following a HEADER descriptor is represented by BDCOUNT field
	 * of HEADER descriptor. The max value of BDCOUNT field is 31 which
	 * means we can only have 31 non-HEADER descriptors following one
	 * HEADER descriptor.
	 *
	 * In general use, number of non-HEADER descriptors can easily go
	 * beyond 31. To tackle this situation, we have packet (or request)
	 * extension bits (STARTPKT and ENDPKT) in the HEADER descriptor.
	 *
	 * To use packet extension, the first HEADER descriptor of request
	 * (or packet) will have STARTPKT=1 and ENDPKT=0. The intermediate
	 * HEADER descriptors will have STARTPKT=0 and ENDPKT=0. The last
	 * HEADER descriptor will have STARTPKT=0 and ENDPKT=1.
	 */

	if ((nhpos % HEADER_BDCOUNT_MAX == 0) && (nhcnt - nhpos)) {
		/* Prepare the header descriptor */
		nhavail = (nhcnt - nhpos);
		_startpkt = (nhpos == 0) ? 0x1 : 0x0;
		_endpkt = (nhavail <= HEADER_BDCOUNT_MAX) ? 0x1 : 0x0;
		_bdcount = (nhavail <= HEADER_BDCOUNT_MAX) ?
				nhavail : HEADER_BDCOUNT_MAX;
		if (nhavail <= HEADER_BDCOUNT_MAX)
			_bdcount = nhavail;
		else
			_bdcount = HEADER_BDCOUNT_MAX;
		d = bcmfs5_header_desc(_startpkt, _endpkt,
				       _bdcount, 0x0, reqid);

		/* Write header descriptor */
		rm_write_desc(*desc_ptr, d);

		/* Point to next descriptor */
		*desc_ptr = (uint8_t *)*desc_ptr + sizeof(desc);
		if (*desc_ptr == end_desc)
			*desc_ptr = start_desc;

		/* Skip next pointer descriptors */
		while (bcmfs5_is_next_table_desc(*desc_ptr)) {
			is_nxt_page = 1;
			*desc_ptr = (uint8_t *)*desc_ptr + sizeof(desc);
			if (*desc_ptr == end_desc)
				*desc_ptr = start_desc;
		}
	}

	/* Write desired descriptor */
	rm_write_desc(*desc_ptr, desc);

	/* Point to next descriptor */
	*desc_ptr = (uint8_t *)*desc_ptr + sizeof(desc);
	if (*desc_ptr == end_desc)
		*desc_ptr = start_desc;

	/* Skip next pointer descriptors */
	while (bcmfs5_is_next_table_desc(*desc_ptr)) {
		is_nxt_page = 1;
		*desc_ptr = (uint8_t *)*desc_ptr + sizeof(desc);
		if (*desc_ptr == end_desc)
			*desc_ptr = start_desc;
	}

	return is_nxt_page;
}

static uint64_t
bcmfs5_src_desc(uint64_t addr, unsigned int len)
{
	return (rm_build_desc(SRC_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK) |
		rm_build_desc(len, SRC_LENGTH_SHIFT, SRC_LENGTH_MASK) |
		rm_build_desc(addr, SRC_ADDR_SHIFT, SRC_ADDR_MASK));
}

static uint64_t
bcmfs5_msrc_desc(uint64_t addr, unsigned int len_div_16)
{
	return (rm_build_desc(MSRC_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK) |
		rm_build_desc(len_div_16, MSRC_LENGTH_SHIFT, MSRC_LENGTH_MASK) |
		rm_build_desc(addr, MSRC_ADDR_SHIFT, MSRC_ADDR_MASK));
}

static uint64_t
bcmfs5_dst_desc(uint64_t addr, unsigned int len)
{
	return (rm_build_desc(DST_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK) |
		rm_build_desc(len, DST_LENGTH_SHIFT, DST_LENGTH_MASK) |
		rm_build_desc(addr, DST_ADDR_SHIFT, DST_ADDR_MASK));
}

static uint64_t
bcmfs5_mdst_desc(uint64_t addr, unsigned int len_div_16)
{
	return (rm_build_desc(MDST_TYPE, DESC_TYPE_SHIFT, DESC_TYPE_MASK) |
		rm_build_desc(len_div_16, MDST_LENGTH_SHIFT, MDST_LENGTH_MASK) |
		rm_build_desc(addr, MDST_ADDR_SHIFT, MDST_ADDR_MASK));
}

static bool
bcmfs5_sanity_check(struct bcmfs_qp_message *msg)
{
	unsigned int i = 0;

	if (msg == NULL)
		return false;

	for (i = 0; i <  msg->srcs_count; i++) {
		if (msg->srcs_len[i] & 0xf) {
			if (msg->srcs_len[i] > SRC_LENGTH_MASK)
				return false;
		} else {
			if (msg->srcs_len[i] > (MSRC_LENGTH_MASK * 16))
				return false;
		}
	}
	for (i = 0; i <  msg->dsts_count; i++) {
		if (msg->dsts_len[i] & 0xf) {
			if (msg->dsts_len[i] > DST_LENGTH_MASK)
				return false;
		} else {
			if (msg->dsts_len[i] > (MDST_LENGTH_MASK * 16))
				return false;
		}
	}

	return true;
}

static void *
bcmfs5_enqueue_msg(struct bcmfs_queue *txq,
		   struct bcmfs_qp_message *msg,
		   uint32_t reqid, void *desc_ptr,
		   void *start_desc, void *end_desc)
{
	uint64_t d;
	unsigned int src, dst;
	uint32_t nhpos = 0;
	int nxt_page = 0;
	uint32_t nhcnt = msg->srcs_count + msg->dsts_count;

	if (desc_ptr == NULL || start_desc == NULL || end_desc == NULL)
		return NULL;

	if (desc_ptr < start_desc || end_desc <= desc_ptr)
		return NULL;

	for (src = 0; src < msg->srcs_count; src++) {
		if (msg->srcs_len[src] & 0xf)
			d = bcmfs5_src_desc(msg->srcs_addr[src],
					    msg->srcs_len[src]);
		else
			d = bcmfs5_msrc_desc(msg->srcs_addr[src],
					     msg->srcs_len[src] / 16);

		nxt_page = bcmfs5_enqueue_desc(nhpos, nhcnt, reqid,
					       d, &desc_ptr, start_desc,
					       end_desc);
		if (nxt_page)
			txq->descs_inflight++;
		nhpos++;
	}

	for (dst = 0; dst < msg->dsts_count; dst++) {
		if (msg->dsts_len[dst] & 0xf)
			d = bcmfs5_dst_desc(msg->dsts_addr[dst],
					    msg->dsts_len[dst]);
		else
			d = bcmfs5_mdst_desc(msg->dsts_addr[dst],
					     msg->dsts_len[dst] / 16);

		nxt_page = bcmfs5_enqueue_desc(nhpos, nhcnt, reqid,
					       d, &desc_ptr, start_desc,
					       end_desc);
		if (nxt_page)
			txq->descs_inflight++;
		nhpos++;
	}

	txq->descs_inflight += nhcnt + 1;

	return desc_ptr;
}

static int
bcmfs5_enqueue_single_request_qp(struct bcmfs_qp *qp, void *op)
{
	void *next;
	int reqid;
	int ret = 0;
	uint64_t slab = 0;
	uint32_t pos = 0;
	uint8_t exit_cleanup = false;
	struct bcmfs_queue *txq = &qp->tx_q;
	struct bcmfs_qp_message *msg = (struct bcmfs_qp_message *)op;

	/* Do sanity check on message */
	if (!bcmfs5_sanity_check(msg)) {
		BCMFS_DP_LOG(ERR, "Invalid msg on queue %d", qp->qpair_id);
		return -EIO;
	}

	/* Scan from the beginning */
	__rte_bitmap_scan_init(qp->ctx_bmp);
	/* Scan bitmap to get the free pool */
	ret = rte_bitmap_scan(qp->ctx_bmp, &pos, &slab);
	if (ret == 0) {
		BCMFS_DP_LOG(ERR, "BD memory exhausted");
		return -ERANGE;
	}

	reqid = pos + rte_ctz64(slab);
	rte_bitmap_clear(qp->ctx_bmp, reqid);
	qp->ctx_pool[reqid] = (unsigned long)msg;

	/* Write descriptors to ring */
	next = bcmfs5_enqueue_msg(txq, msg, reqid,
				  (uint8_t *)txq->base_addr + txq->tx_write_ptr,
				  txq->base_addr,
				  (uint8_t *)txq->base_addr + txq->queue_size);
	if (next == NULL) {
		BCMFS_DP_LOG(ERR, "Enqueue for desc failed on queue %d",
			     qp->qpair_id);
		ret = -EINVAL;
		exit_cleanup = true;
		goto exit;
	}

	/* Save ring BD write offset */
	txq->tx_write_ptr = (uint32_t)((uint8_t *)next -
				       (uint8_t *)txq->base_addr);

	qp->nb_pending_requests++;

	return 0;

exit:
	/* Cleanup if we failed */
	if (exit_cleanup)
		rte_bitmap_set(qp->ctx_bmp, reqid);

	return ret;
}

static void bcmfs5_write_doorbell(struct bcmfs_qp *qp)
{
	struct bcmfs_queue *txq = &qp->tx_q;

	/* sync in before ringing the door-bell */
	rte_wmb();

	FS_MMIO_WRITE32(txq->descs_inflight,
			(uint8_t *)qp->ioreg + RING_DOORBELL_BD_WRITE_COUNT);

	/* reset the count */
	txq->descs_inflight = 0;
}

static uint16_t
bcmfs5_dequeue_qp(struct bcmfs_qp *qp, void **ops, uint16_t budget)
{
	int err;
	uint16_t reqid;
	uint64_t desc;
	uint16_t count = 0;
	unsigned long context = 0;
	struct bcmfs_queue *hwq = &qp->cmpl_q;
	uint32_t cmpl_read_offset, cmpl_write_offset;

	/*
	 * Check whether budget is valid, else set the budget to maximum
	 * so that all the available completions will be processed.
	 */
	if (budget > qp->nb_pending_requests)
		budget =  qp->nb_pending_requests;

	/*
	 * Get current completion read and write offset
	 *
	 * Note: We should read completion write pointer at least once
	 * after we get a MSI interrupt because HW maintains internal
	 * MSI status which will allow next MSI interrupt only after
	 * completion write pointer is read.
	 */
	cmpl_write_offset = FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_CMPL_WRITE_PTR);
	cmpl_write_offset *= FS_RING_DESC_SIZE;
	cmpl_read_offset = hwq->cmpl_read_ptr;

	/* read the ring cmpl write ptr before cmpl read offset */
	rte_io_rmb();

	/* For each completed request notify mailbox clients */
	reqid = 0;
	while ((cmpl_read_offset != cmpl_write_offset) && (budget > 0)) {
		/* Dequeue next completion descriptor */
		desc = *((uint64_t *)((uint8_t *)hwq->base_addr +
				      cmpl_read_offset));

		/* Next read offset */
		cmpl_read_offset += FS_RING_DESC_SIZE;
		if (cmpl_read_offset == FS_RING_CMPL_SIZE)
			cmpl_read_offset = 0;

		/* Decode error from completion descriptor */
		err = rm_cmpl_desc_to_error(desc);
		if (err < 0)
			BCMFS_DP_LOG(ERR, "error desc rcvd");

		/* Determine request id from completion descriptor */
		reqid = rm_cmpl_desc_to_reqid(desc);

		/* Retrieve context */
		context = qp->ctx_pool[reqid];
		if (context == 0)
			BCMFS_DP_LOG(ERR, "HW error detected");

		/* Release reqid for recycling */
		qp->ctx_pool[reqid] = 0;
		rte_bitmap_set(qp->ctx_bmp, reqid);

		*ops = (void *)context;

		/* Increment number of completions processed */
		count++;
		budget--;
		ops++;
	}

	hwq->cmpl_read_ptr = cmpl_read_offset;

	qp->nb_pending_requests -= count;

	return count;
}

static int
bcmfs5_start_qp(struct bcmfs_qp *qp)
{
	uint32_t val, off;
	uint64_t d, next_addr, msi;
	int timeout;
	uint32_t bd_high, bd_low, cmpl_high, cmpl_low;
	struct bcmfs_queue *tx_queue = &qp->tx_q;
	struct bcmfs_queue *cmpl_queue = &qp->cmpl_q;

	/* Disable/deactivate ring */
	FS_MMIO_WRITE32(0x0, (uint8_t *)qp->ioreg + RING_CONTROL);

	/* Configure next table pointer entries in BD memory */
	for (off = 0; off < tx_queue->queue_size; off += FS_RING_DESC_SIZE) {
		next_addr = off + FS_RING_DESC_SIZE;
		if (next_addr == tx_queue->queue_size)
			next_addr = 0;
		next_addr += (uint64_t)tx_queue->base_phys_addr;
		if (FS_RING_BD_ALIGN_CHECK(next_addr))
			d = bcmfs5_next_table_desc(next_addr);
		else
			d = bcmfs5_null_desc();
		rm_write_desc((uint8_t *)tx_queue->base_addr + off, d);
	}

	/*
	 * If user interrupt the test in between the run(Ctrl+C), then all
	 * subsequent test run will fail because sw cmpl_read_offset and hw
	 * cmpl_write_offset will be pointing at different completion BD. To
	 * handle this we should flush all the rings in the startup instead
	 * of shutdown function.
	 * Ring flush will reset hw cmpl_write_offset.
	 */

	/* Set ring flush state */
	timeout = 1000;
	FS_MMIO_WRITE32(BIT(CONTROL_FLUSH_SHIFT),
			(uint8_t *)qp->ioreg + RING_CONTROL);
	do {
		/*
		 * If previous test is stopped in between the run, then
		 * sw has to read cmpl_write_offset else DME/AE will be not
		 * come out of flush state.
		 */
		FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_CMPL_WRITE_PTR);

		if (FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_FLUSH_DONE) &
				   FLUSH_DONE_MASK)
			break;
		usleep(1000);
	} while (--timeout);
	if (!timeout) {
		BCMFS_DP_LOG(ERR, "Ring flush timeout hw-queue %d",
			     qp->qpair_id);
	}

	/* Clear ring flush state */
	timeout = 1000;
	FS_MMIO_WRITE32(0x0, (uint8_t *)qp->ioreg + RING_CONTROL);
	do {
		if (!(FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_FLUSH_DONE) &
				     FLUSH_DONE_MASK))
			break;
		usleep(1000);
	} while (--timeout);
	if (!timeout) {
		BCMFS_DP_LOG(ERR, "Ring clear flush timeout hw-queue %d",
			     qp->qpair_id);
	}

	/* Program BD start address */
	bd_low = lower_32_bits(tx_queue->base_phys_addr);
	bd_high = upper_32_bits(tx_queue->base_phys_addr);
	FS_MMIO_WRITE32(bd_low, (uint8_t *)qp->ioreg +
				RING_BD_START_ADDRESS_LSB);
	FS_MMIO_WRITE32(bd_high, (uint8_t *)qp->ioreg +
				 RING_BD_START_ADDRESS_MSB);

	tx_queue->tx_write_ptr = 0;

	for (off = 0; off < FS_RING_CMPL_SIZE; off += FS_RING_DESC_SIZE)
		rm_write_desc((uint8_t *)cmpl_queue->base_addr + off, 0x0);

	/* Completion read pointer will be same as HW write pointer */
	cmpl_queue->cmpl_read_ptr = FS_MMIO_READ32((uint8_t *)qp->ioreg +
						   RING_CMPL_WRITE_PTR);
	/* Program completion start address */
	cmpl_low = lower_32_bits(cmpl_queue->base_phys_addr);
	cmpl_high = upper_32_bits(cmpl_queue->base_phys_addr);
	FS_MMIO_WRITE32(cmpl_low, (uint8_t *)qp->ioreg +
				RING_CMPL_START_ADDR_LSB);
	FS_MMIO_WRITE32(cmpl_high, (uint8_t *)qp->ioreg +
				RING_CMPL_START_ADDR_MSB);

	cmpl_queue->cmpl_read_ptr *= FS_RING_DESC_SIZE;

	/* Read ring Tx, Rx, and Outstanding counts to clear */
	FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_NUM_REQ_RECV_LS);
	FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_NUM_REQ_RECV_MS);
	FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_NUM_REQ_TRANS_LS);
	FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_NUM_REQ_TRANS_MS);
	FS_MMIO_READ32((uint8_t *)qp->ioreg + RING_NUM_REQ_OUTSTAND);

	/* Configure per-Ring MSI registers with dummy location */
	msi = cmpl_queue->base_phys_addr + (1024 * FS_RING_DESC_SIZE);
	FS_MMIO_WRITE32((msi & 0xFFFFFFFF),
			(uint8_t *)qp->ioreg + RING_MSI_ADDR_LS);
	FS_MMIO_WRITE32(((msi >> 32) & 0xFFFFFFFF),
			(uint8_t *)qp->ioreg + RING_MSI_ADDR_MS);
	FS_MMIO_WRITE32(qp->qpair_id, (uint8_t *)qp->ioreg +
				      RING_MSI_DATA_VALUE);

	/* Configure RING_MSI_CONTROL */
	val = 0;
	val |= (MSI_TIMER_VAL_MASK << MSI_TIMER_VAL_SHIFT);
	val |= BIT(MSI_ENABLE_SHIFT);
	val |= (0x1 & MSI_COUNT_MASK) << MSI_COUNT_SHIFT;
	FS_MMIO_WRITE32(val, (uint8_t *)qp->ioreg + RING_MSI_CONTROL);

	/* Enable/activate ring */
	val = BIT(CONTROL_ACTIVE_SHIFT);
	FS_MMIO_WRITE32(val, (uint8_t *)qp->ioreg + RING_CONTROL);

	return 0;
}

static void
bcmfs5_shutdown_qp(struct bcmfs_qp *qp)
{
	/* Disable/deactivate ring */
	FS_MMIO_WRITE32(0x0, (uint8_t *)qp->ioreg + RING_CONTROL);
}

struct bcmfs_hw_queue_pair_ops bcmfs5_qp_ops = {
	.name = "fs5",
	.enq_one_req = bcmfs5_enqueue_single_request_qp,
	.ring_db = bcmfs5_write_doorbell,
	.dequeue = bcmfs5_dequeue_qp,
	.startq = bcmfs5_start_qp,
	.stopq = bcmfs5_shutdown_qp,
};

RTE_INIT(bcmfs5_register_qp_ops)
{
	bcmfs_hw_queue_pair_register_ops(&bcmfs5_qp_ops);
}
