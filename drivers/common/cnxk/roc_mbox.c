/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "roc_api.h"
#include "roc_priv.h"

/* RCLK, SCLK in MHz */
uint16_t dev_rclk_freq;
uint16_t dev_sclk_freq;

static inline uint16_t
msgs_offset(void)
{
	return PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
}

void
mbox_fini(struct mbox *mbox)
{
	mbox->reg_base = 0;
	mbox->hwbase = 0;
	plt_free(mbox->dev);
	mbox->dev = NULL;
}

void
mbox_reset(struct mbox *mbox, int devid)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_hdr *tx_hdr =
		(struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->tx_start);
	struct mbox_hdr *rx_hdr =
		(struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);

	mdev->msg_size = 0;
	mdev->rsp_size = 0;
	tx_hdr->msg_size = 0;
	tx_hdr->num_msgs = 0;
	rx_hdr->msg_size = 0;
	rx_hdr->num_msgs = 0;
}

int
mbox_init(struct mbox *mbox, uintptr_t hwbase, uintptr_t reg_base,
	  int direction, int ndevs, uint64_t intr_offset)
{
	struct mbox_dev *mdev;
	char *var, *var_to;
	int devid;

	mbox->intr_offset = intr_offset;
	mbox->reg_base = reg_base;
	mbox->hwbase = hwbase;

	switch (direction) {
	case MBOX_DIR_AFPF:
	case MBOX_DIR_PFVF:
		mbox->tx_start = MBOX_DOWN_TX_START;
		mbox->rx_start = MBOX_DOWN_RX_START;
		mbox->tx_size = MBOX_DOWN_TX_SIZE;
		mbox->rx_size = MBOX_DOWN_RX_SIZE;
		break;
	case MBOX_DIR_PFAF:
	case MBOX_DIR_VFPF:
		mbox->tx_start = MBOX_DOWN_RX_START;
		mbox->rx_start = MBOX_DOWN_TX_START;
		mbox->tx_size = MBOX_DOWN_RX_SIZE;
		mbox->rx_size = MBOX_DOWN_TX_SIZE;
		break;
	case MBOX_DIR_AFPF_UP:
	case MBOX_DIR_PFVF_UP:
		mbox->tx_start = MBOX_UP_TX_START;
		mbox->rx_start = MBOX_UP_RX_START;
		mbox->tx_size = MBOX_UP_TX_SIZE;
		mbox->rx_size = MBOX_UP_RX_SIZE;
		break;
	case MBOX_DIR_PFAF_UP:
	case MBOX_DIR_VFPF_UP:
		mbox->tx_start = MBOX_UP_RX_START;
		mbox->rx_start = MBOX_UP_TX_START;
		mbox->tx_size = MBOX_UP_RX_SIZE;
		mbox->rx_size = MBOX_UP_TX_SIZE;
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

	mbox->dev = plt_zmalloc(ndevs * sizeof(struct mbox_dev), ROC_ALIGN);
	if (!mbox->dev) {
		mbox_fini(mbox);
		return -ENOMEM;
	}
	mbox->ndevs = ndevs;
	for (devid = 0; devid < ndevs; devid++) {
		mdev = &mbox->dev[devid];
		mdev->mbase = (void *)(mbox->hwbase + (devid * MBOX_SIZE));
		plt_spinlock_init(&mdev->mbox_lock);
		/* Init header to reset value */
		mbox_reset(mbox, devid);
	}

	var = getenv("ROC_CN10K_MBOX_TIMEOUT");
	var_to = getenv("ROC_MBOX_TIMEOUT");

	if (var)
		mbox->rsp_tmo = atoi(var);
	else if (var_to)
		mbox->rsp_tmo = atoi(var_to);
	else
		mbox->rsp_tmo = MBOX_RSP_TIMEOUT;

	return 0;
}

/**
 * @internal
 * Allocate a message response
 */
struct mbox_msghdr *
mbox_alloc_msg_rsp(struct mbox *mbox, int devid, int size, int size_rsp)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_msghdr *msghdr = NULL;

	size = PLT_ALIGN(size, MBOX_MSG_ALIGN);
	size_rsp = PLT_ALIGN(size_rsp, MBOX_MSG_ALIGN);
	/* Check if there is space in mailbox */
	if ((mdev->msg_size + size) > mbox->tx_size - msgs_offset())
		goto exit;
	if ((mdev->rsp_size + size_rsp) > mbox->rx_size - msgs_offset())
		goto exit;
	if (mdev->msg_size == 0)
		mdev->num_msgs = 0;
	mdev->num_msgs++;

	msghdr = (struct mbox_msghdr *)(((uintptr_t)mdev->mbase +
					 mbox->tx_start + msgs_offset() +
					 mdev->msg_size));

	/* Clear the whole msg region */
	mbox_memset(msghdr, 0, sizeof(*msghdr) + size);
	/* Init message header with reset values */
	msghdr->ver = MBOX_VERSION;
	mdev->msg_size += size;
	mdev->rsp_size += size_rsp;
	msghdr->next_msgoff = mdev->msg_size + msgs_offset();
exit:

	return msghdr;
}

/**
 * @internal
 * Synchronization between UP and DOWN messages
 */
bool
mbox_wait_for_zero(struct mbox *mbox, int devid)
{
	uint64_t data;

	data = plt_read64((volatile void *)(mbox->reg_base +
				(mbox->trigger | (devid << mbox->tr_shift))));

	/* If data is non-zero wait for ~1ms and return to caller
	 * whether data has changed to zero or not after the wait.
	 */
	if (data)
		usleep(1000);
	else
		return true;

	data = plt_read64((volatile void *)(mbox->reg_base +
				(mbox->trigger | (devid << mbox->tr_shift))));
	return data == 0;
}

static void
mbox_msg_send_data(struct mbox *mbox, int devid, uint8_t data)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_hdr *tx_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->tx_start);
	struct mbox_hdr *rx_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	uint64_t intr_val;

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
	plt_wmb();

	/* Check for any pending interrupt */
	intr_val = plt_read64(
		(volatile void *)(mbox->reg_base + (mbox->trigger | (devid << mbox->tr_shift))));

	intr_val |= (uint64_t)data;
	/* The interrupt should be fired after num_msgs is written
	 * to the shared memory
	 */
	plt_write64(intr_val, (volatile void *)(mbox->reg_base +
						(mbox->trigger | (devid << mbox->tr_shift))));
}

/**
 * @internal
 * Send a mailbox message
 */
void
mbox_msg_send(struct mbox *mbox, int devid)
{
	mbox_msg_send_data(mbox, devid, MBOX_DOWN_MSG);
}

/**
 * @internal
 * Send an UP mailbox message
 */
void
mbox_msg_send_up(struct mbox *mbox, int devid)
{
	mbox_msg_send_data(mbox, devid, MBOX_UP_MSG);
}

/**
 * @internal
 * Wait and get mailbox response
 */
int
mbox_get_rsp(struct mbox *mbox, int devid, void **msg)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_msghdr *msghdr;
	uint64_t offset;
	int rc;

	rc = mbox_wait_for_rsp(mbox, devid);
	if (rc < 0)
		return -EIO;

	plt_rmb();

	offset = mbox->rx_start +
		 PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msghdr = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
	if (msg != NULL)
		*msg = msghdr;

	return msghdr->rc;
}

/**
 * Polling for given wait time to get mailbox response
 */
static int
mbox_poll(struct mbox *mbox, uint32_t wait)
{
	uint32_t timeout = 0, sleep = 1;
	uint32_t wait_us = wait * 1000;
	uint64_t rsp_reg = 0;
	uintptr_t reg_addr;

	reg_addr = mbox->reg_base + mbox->intr_offset;
	do {
		rsp_reg = plt_read64(reg_addr);

		if (timeout >= wait_us)
			return -ETIMEDOUT;

		plt_delay_us(sleep);
		timeout += sleep;
	} while (!rsp_reg);

	plt_rmb();

	/* Clear interrupt */
	plt_write64(rsp_reg, reg_addr);

	/* Reset mbox */
	mbox_reset(mbox, 0);

	return 0;
}

/**
 * @internal
 * Wait and get mailbox response with timeout
 */
int
mbox_get_rsp_tmo(struct mbox *mbox, int devid, void **msg, uint32_t tmo)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	struct mbox_msghdr *msghdr;
	uint64_t offset;
	int rc;

	rc = mbox_wait_for_rsp_tmo(mbox, devid, tmo);
	if (rc != 1)
		return -EIO;

	plt_rmb();

	offset = mbox->rx_start +
		 PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msghdr = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
	if (msg != NULL)
		*msg = msghdr;

	return msghdr->rc;
}

static int
mbox_wait(struct mbox *mbox, int devid, uint32_t rst_timo)
{
	volatile struct mbox_dev *mdev = &mbox->dev[devid];
	uint32_t timeout = 0, sleep = 1;

	rst_timo = rst_timo * 1000; /* Milli seconds to micro seconds */

	/* Waiting for mdev->msgs_acked tp become equal to mdev->num_msgs,
	 * mdev->msgs_acked are incremented at process_msgs() in interrupt
	 * thread context.
	 */
	while (mdev->num_msgs > mdev->msgs_acked) {
		plt_delay_us(sleep);
		timeout += sleep;
		if (timeout >= rst_timo) {
			struct mbox_hdr *tx_hdr =
				(struct mbox_hdr *)((uintptr_t)mdev->mbase +
						    mbox->tx_start);
			struct mbox_hdr *rx_hdr =
				(struct mbox_hdr *)((uintptr_t)mdev->mbase +
						    mbox->rx_start);

			plt_err("MBOX[devid: %d] message wait timeout %d, "
				"num_msgs: %d, msgs_acked: %d "
				"(tx/rx num_msgs: %d/%d), msg_size: %d, "
				"rsp_size: %d",
				devid, timeout, mdev->num_msgs,
				mdev->msgs_acked, tx_hdr->num_msgs,
				rx_hdr->num_msgs, mdev->msg_size,
				mdev->rsp_size);

			return -EIO;
		}
		plt_rmb();
	}
	return 0;
}

int
mbox_wait_for_rsp_tmo(struct mbox *mbox, int devid, uint32_t tmo)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	int rc = 0;

	/* Sync with mbox region */
	plt_rmb();

	if (mbox->trigger == RVU_PF_VFX_PFVF_MBOX1 ||
	    mbox->trigger == RVU_PF_VFX_PFVF_MBOX0) {
		/* In case of VF, Wait a bit more to account round trip delay */
		tmo = tmo * 2;
	}

	/* Wait message */
	if (plt_thread_is_intr())
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
mbox_wait_for_rsp(struct mbox *mbox, int devid)
{
	return mbox_wait_for_rsp_tmo(mbox, devid, mbox->rsp_tmo);
}

int
mbox_get_availmem(struct mbox *mbox, int devid)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	int avail;

	plt_spinlock_lock(&mdev->mbox_lock);
	avail = mbox->tx_size - mdev->msg_size - msgs_offset();
	plt_spinlock_unlock(&mdev->mbox_lock);

	return avail;
}

int
send_ready_msg(struct mbox *mbox, uint16_t *pcifunc)
{
	struct ready_msg_rsp *rsp;
	int rc;

	mbox_alloc_msg_ready(mbox_get(mbox));

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		mbox_put(mbox);
		return rc;
	}
	mbox_put(mbox);

	if (rsp->hdr.ver != MBOX_VERSION) {
		plt_err("Incompatible MBox versions(AF: 0x%04x Client: 0x%04x)",
			rsp->hdr.ver, MBOX_VERSION);
		return -EPIPE;
	}

	if (pcifunc)
		*pcifunc = rsp->hdr.pcifunc;

	/* Save rclk & sclk freq */
	if (!dev_rclk_freq || !dev_sclk_freq) {
		dev_rclk_freq = rsp->rclk_freq;
		dev_sclk_freq = rsp->sclk_freq;
	}
	return 0;
}

int
reply_invalid_msg(struct mbox *mbox, int devid, uint16_t pcifunc, uint16_t id)
{
	struct msg_rsp *rsp;

	rsp = (struct msg_rsp *)mbox_alloc_msg(mbox, devid, sizeof(*rsp));
	if (!rsp)
		return -ENOMEM;
	rsp->hdr.id = id;
	rsp->hdr.sig = MBOX_RSP_SIG;
	rsp->hdr.rc = MBOX_MSG_INVALID;
	rsp->hdr.pcifunc = pcifunc;

	return 0;
}

/**
 * @internal
 * Convert mail box ID to name
 */
const char *
mbox_id2name(uint16_t id)
{
	switch (id) {
	default:
		return "INVALID ID";
#define M(_name, _id, _1, _2, _3)                                              \
	case _id:                                                              \
		return #_name;
		MBOX_MESSAGES
		MBOX_UP_CGX_MESSAGES
#undef M
	}
}

int
mbox_id2size(uint16_t id)
{
	switch (id) {
	default:
		return 0;
#define M(_1, _id, _2, _req_type, _3)                                          \
	case _id:                                                              \
		return sizeof(struct _req_type);
		MBOX_MESSAGES
		MBOX_UP_CGX_MESSAGES
#undef M
	}
}
