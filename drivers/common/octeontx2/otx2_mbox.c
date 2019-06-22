/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_atomic.h>
#include <rte_cycles.h>

#include "otx2_mbox.h"

#define RVU_AF_AFPF_MBOX0	(0x02000)
#define RVU_AF_AFPF_MBOX1	(0x02008)

#define RVU_PF_PFAF_MBOX0	(0xC00)
#define RVU_PF_PFAF_MBOX1	(0xC08)

#define RVU_PF_VFX_PFVF_MBOX0	(0x0000)
#define RVU_PF_VFX_PFVF_MBOX1	(0x0008)

#define	RVU_VF_VFPF_MBOX0	(0x0000)
#define	RVU_VF_VFPF_MBOX1	(0x0008)

void
otx2_mbox_fini(struct otx2_mbox *mbox)
{
	mbox->reg_base = 0;
	mbox->hwbase = 0;
	free(mbox->dev);
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
otx2_mbox_init(struct otx2_mbox *mbox, uintptr_t hwbase,
	       uintptr_t reg_base, int direction, int ndevs)
{
	struct otx2_mbox_dev *mdev;
	int devid;

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

	mbox->dev = malloc(ndevs * sizeof(struct otx2_mbox_dev));
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
