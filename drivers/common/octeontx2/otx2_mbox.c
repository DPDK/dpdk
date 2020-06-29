/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "otx2_mbox.h"
#include "otx2_dev.h"

#define RVU_AF_AFPF_MBOX0	(0x02000)
#define RVU_AF_AFPF_MBOX1	(0x02008)

#define RVU_PF_PFAF_MBOX0	(0xC00)
#define RVU_PF_PFAF_MBOX1	(0xC08)

#define RVU_PF_VFX_PFVF_MBOX0	(0x0000)
#define RVU_PF_VFX_PFVF_MBOX1	(0x0008)

#define	RVU_VF_VFPF_MBOX0	(0x0000)
#define	RVU_VF_VFPF_MBOX1	(0x0008)

static inline uint16_t
msgs_offset(void)
{
	return RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
}

void
otx2_mbox_fini(struct otx2_mbox *mbox)
{
	mbox->reg_base = 0;
	mbox->hwbase = 0;
	rte_free(mbox->dev);
	mbox->dev = NULL;
}

void
otx2_mbox_reset(struct otx2_mbox *mbox, int devid)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_hdr *tx_hdr =
		(struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->tx_start);
	struct mbox_hdr *rx_hdr =
		(struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);

	rte_spinlock_lock(&mdev->mbox_lock);
	mdev->msg_size = 0;
	mdev->rsp_size = 0;
	tx_hdr->msg_size = 0;
	tx_hdr->num_msgs = 0;
	rx_hdr->msg_size = 0;
	rx_hdr->num_msgs = 0;
	rte_spinlock_unlock(&mdev->mbox_lock);
}

int
otx2_mbox_init(struct otx2_mbox *mbox, uintptr_t hwbase, uintptr_t reg_base,
	       int direction, int ndevs, uint64_t intr_offset)
{
	struct otx2_mbox_dev *mdev;
	int devid;

	mbox->intr_offset = intr_offset;
	mbox->reg_base = reg_base;
	mbox->hwbase = hwbase;

	switch (direction) {
	case MBOX_DIR_AFPF:
	case MBOX_DIR_PFVF:
		mbox->tx_start = MBOX_DOWN_TX_START;
		mbox->rx_start = MBOX_DOWN_RX_START;
		mbox->tx_size  = MBOX_DOWN_TX_SIZE;
		mbox->rx_size  = MBOX_DOWN_RX_SIZE;
		break;
	case MBOX_DIR_PFAF:
	case MBOX_DIR_VFPF:
		mbox->tx_start = MBOX_DOWN_RX_START;
		mbox->rx_start = MBOX_DOWN_TX_START;
		mbox->tx_size  = MBOX_DOWN_RX_SIZE;
		mbox->rx_size  = MBOX_DOWN_TX_SIZE;
		break;
	case MBOX_DIR_AFPF_UP:
	case MBOX_DIR_PFVF_UP:
		mbox->tx_start = MBOX_UP_TX_START;
		mbox->rx_start = MBOX_UP_RX_START;
		mbox->tx_size  = MBOX_UP_TX_SIZE;
		mbox->rx_size  = MBOX_UP_RX_SIZE;
		break;
	case MBOX_DIR_PFAF_UP:
	case MBOX_DIR_VFPF_UP:
		mbox->tx_start = MBOX_UP_RX_START;
		mbox->rx_start = MBOX_UP_TX_START;
		mbox->tx_size  = MBOX_UP_RX_SIZE;
		mbox->rx_size  = MBOX_UP_TX_SIZE;
		break;
	default:
		return -ENODEV;
	}

	switch (direction) {
	case MBOX_DIR_AFPF:
	case MBOX_DIR_AFPF_UP:
		mbox->trigger = RVU_AF_AFPF_MBOX0;
		mbox->tr_shift = 4;
		break;
	case MBOX_DIR_PFAF:
	case MBOX_DIR_PFAF_UP:
		mbox->trigger = RVU_PF_PFAF_MBOX1;
		mbox->tr_shift = 0;
		break;
	case MBOX_DIR_PFVF:
	case MBOX_DIR_PFVF_UP:
		mbox->trigger = RVU_PF_VFX_PFVF_MBOX0;
		mbox->tr_shift = 12;
		break;
	case MBOX_DIR_VFPF:
	case MBOX_DIR_VFPF_UP:
		mbox->trigger = RVU_VF_VFPF_MBOX1;
		mbox->tr_shift = 0;
		break;
	default:
		return -ENODEV;
	}

	mbox->dev = rte_zmalloc("mbox dev",
				ndevs * sizeof(struct otx2_mbox_dev),
				OTX2_ALIGN);
	if (!mbox->dev) {
		otx2_mbox_fini(mbox);
		return -ENOMEM;
	}
	mbox->ndevs = ndevs;
	for (devid = 0; devid < ndevs; devid++) {
		mdev = &mbox->dev[devid];
		mdev->mbase = (void *)(mbox->hwbase + (devid * MBOX_SIZE));
		rte_spinlock_init(&mdev->mbox_lock);
		/* Init header to reset value */
		otx2_mbox_reset(mbox, devid);
	}

	return 0;
}

/**
 * @internal
 * Allocate a message response
 */
struct mbox_msghdr *
otx2_mbox_alloc_msg_rsp(struct otx2_mbox *mbox, int devid, int size,
			int size_rsp)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_msghdr *msghdr = NULL;

	rte_spinlock_lock(&mdev->mbox_lock);
	size = RTE_ALIGN(size, MBOX_MSG_ALIGN);
	size_rsp = RTE_ALIGN(size_rsp, MBOX_MSG_ALIGN);
	/* Check if there is space in mailbox */
	if ((mdev->msg_size + size) > mbox->tx_size - msgs_offset())
		goto exit;
	if ((mdev->rsp_size + size_rsp) > mbox->rx_size - msgs_offset())
		goto exit;
	if (mdev->msg_size == 0)
		mdev->num_msgs = 0;
	mdev->num_msgs++;

	msghdr = (struct mbox_msghdr *)(((uintptr_t)mdev->mbase +
			mbox->tx_start + msgs_offset() + mdev->msg_size));

	/* Clear the whole msg region */
	otx2_mbox_memset(msghdr, 0, sizeof(*msghdr) + size);
	/* Init message header with reset values */
	msghdr->ver = OTX2_MBOX_VERSION;
	mdev->msg_size += size;
	mdev->rsp_size += size_rsp;
	msghdr->next_msgoff = mdev->msg_size + msgs_offset();
exit:
	rte_spinlock_unlock(&mdev->mbox_lock);

	return msghdr;
}

/**
 * @internal
 * Send a mailbox message
 */
void
otx2_mbox_msg_send(struct otx2_mbox *mbox, int devid)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_hdr *tx_hdr =
		(struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->tx_start);
	struct mbox_hdr *rx_hdr =
		(struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);

	/* Reset header for next messages */
	tx_hdr->msg_size = mdev->msg_size;
	mdev->msg_size = 0;
	mdev->rsp_size = 0;
	mdev->msgs_acked = 0;

	/* num_msgs != 0 signals to the peer that the buffer has a number of
	 * messages. So this should be written after copying txmem
	 */
	tx_hdr->num_msgs = mdev->num_msgs;
	rx_hdr->num_msgs = 0;

	/* Sync mbox data into memory */
	rte_wmb();

	/* The interrupt should be fired after num_msgs is written
	 * to the shared memory
	 */
	rte_write64(1, (volatile void *)(mbox->reg_base +
		(mbox->trigger | (devid << mbox->tr_shift))));
}

/**
 * @internal
 * Wait and get mailbox response
 */
int
otx2_mbox_get_rsp(struct otx2_mbox *mbox, int devid, void **msg)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_msghdr *msghdr;
	uint64_t offset;
	int rc;

	rc = otx2_mbox_wait_for_rsp(mbox, devid);
	if (rc != 1)
		return -EIO;

	rte_rmb();

	offset = mbox->rx_start +
		RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msghdr = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
	if (msg != NULL)
		*msg = msghdr;

	return msghdr->rc;
}

/**
 * Polling for given wait time to get mailbox response
 */
static int
mbox_poll(struct otx2_mbox *mbox, uint32_t wait)
{
	uint32_t timeout = 0, sleep = 1;
	uint32_t wait_us = wait * 1000;
	uint64_t rsp_reg = 0;
	uintptr_t reg_addr;

	reg_addr = mbox->reg_base + mbox->intr_offset;
	do {
		rsp_reg = otx2_read64(reg_addr);

		if (timeout >= wait_us)
			return -ETIMEDOUT;

		rte_delay_us(sleep);
		timeout += sleep;
	} while (!rsp_reg);

	rte_smp_rmb();

	/* Clear interrupt */
	otx2_write64(rsp_reg, reg_addr);

	/* Reset mbox */
	otx2_mbox_reset(mbox, 0);

	return 0;
}

/**
 * @internal
 * Wait and get mailbox response with timeout
 */
int
otx2_mbox_get_rsp_tmo(struct otx2_mbox *mbox, int devid, void **msg,
		      uint32_t tmo)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_msghdr *msghdr;
	uint64_t offset;
	int rc;

	rc = otx2_mbox_wait_for_rsp_tmo(mbox, devid, tmo);
	if (rc != 1)
		return -EIO;

	rte_rmb();

	offset = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msghdr = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
	if (msg != NULL)
		*msg = msghdr;

	return msghdr->rc;
}

static int
mbox_wait(struct otx2_mbox *mbox, int devid, uint32_t rst_timo)
{
	volatile struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	uint32_t timeout = 0, sleep = 1;

	rst_timo  = rst_timo * 1000; /* Milli seconds to micro seconds */
	while (mdev->num_msgs > mdev->msgs_acked) {
		rte_delay_us(sleep);
		timeout += sleep;
		if (timeout >= rst_timo) {
			struct mbox_hdr *tx_hdr =
				(struct mbox_hdr *)((uintptr_t)mdev->mbase +
							mbox->tx_start);
			struct mbox_hdr *rx_hdr =
				(struct mbox_hdr *)((uintptr_t)mdev->mbase +
							mbox->rx_start);

			otx2_err("MBOX[devid: %d] message wait timeout %d, "
				 "num_msgs: %d, msgs_acked: %d "
				 "(tx/rx num_msgs: %d/%d), msg_size: %d, "
				 "rsp_size: %d",
				 devid, timeout, mdev->num_msgs,
				 mdev->msgs_acked, tx_hdr->num_msgs,
				 rx_hdr->num_msgs, mdev->msg_size,
				 mdev->rsp_size);

			return -EIO;
		}
		rte_rmb();
	}
	return 0;
}

int
otx2_mbox_wait_for_rsp_tmo(struct otx2_mbox *mbox, int devid, uint32_t tmo)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	int rc = 0;

	/* Sync with mbox region */
	rte_rmb();

	if (mbox->trigger == RVU_PF_VFX_PFVF_MBOX1 ||
		mbox->trigger == RVU_PF_VFX_PFVF_MBOX0) {
		/* In case of VF, Wait a bit more to account round trip delay */
		tmo = tmo * 2;
	}

	/* Wait message */
	if (rte_thread_is_intr())
		rc = mbox_poll(mbox, tmo);
	else
		rc = mbox_wait(mbox, devid, tmo);

	if (!rc)
		rc = mdev->num_msgs;

	return rc;
}

/**
 * @internal
 * Wait for the mailbox response
 */
int
otx2_mbox_wait_for_rsp(struct otx2_mbox *mbox, int devid)
{
	return otx2_mbox_wait_for_rsp_tmo(mbox, devid, MBOX_RSP_TIMEOUT);
}

int
otx2_mbox_get_availmem(struct otx2_mbox *mbox, int devid)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	int avail;

	rte_spinlock_lock(&mdev->mbox_lock);
	avail = mbox->tx_size - mdev->msg_size - msgs_offset();
	rte_spinlock_unlock(&mdev->mbox_lock);

	return avail;
}

int
otx2_send_ready_msg(struct otx2_mbox *mbox, uint16_t *pcifunc)
{
	struct ready_msg_rsp *rsp;
	int rc;

	otx2_mbox_alloc_msg_ready(mbox);

	otx2_mbox_msg_send(mbox, 0);
	rc = otx2_mbox_get_rsp(mbox, 0, (void *)&rsp);
	if (rc)
		return rc;

	if (rsp->hdr.ver != OTX2_MBOX_VERSION) {
		otx2_err("Incompatible MBox versions(AF: 0x%04x DPDK: 0x%04x)",
			  rsp->hdr.ver, OTX2_MBOX_VERSION);
		return -EPIPE;
	}

	if (pcifunc)
		*pcifunc = rsp->hdr.pcifunc;

	return 0;
}

int
otx2_reply_invalid_msg(struct otx2_mbox *mbox, int devid, uint16_t pcifunc,
		       uint16_t id)
{
	struct msg_rsp *rsp;

	rsp = (struct msg_rsp *)otx2_mbox_alloc_msg(mbox, devid, sizeof(*rsp));
	if (!rsp)
		return -ENOMEM;
	rsp->hdr.id = id;
	rsp->hdr.sig = OTX2_MBOX_RSP_SIG;
	rsp->hdr.rc = MBOX_MSG_INVALID;
	rsp->hdr.pcifunc = pcifunc;

	return 0;
}

/**
 * @internal
 * Convert mail box ID to name
 */
const char *otx2_mbox_id2name(uint16_t id)
{
	switch (id) {
#define M(_name, _id, _1, _2, _3) case _id: return # _name;
	MBOX_MESSAGES
	MBOX_UP_CGX_MESSAGES
#undef M
	default :
		return "INVALID ID";
	}
}

int otx2_mbox_id2size(uint16_t id)
{
	switch (id) {
#define M(_1, _id, _2, _req_type, _3) case _id: return sizeof(struct _req_type);
	MBOX_MESSAGES
	MBOX_UP_CGX_MESSAGES
#undef M
	default :
		return 0;
	}
}
