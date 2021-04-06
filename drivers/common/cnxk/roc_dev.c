/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_priv.h"

/* PCI Extended capability ID */
#define ROC_PCI_EXT_CAP_ID_SRIOV 0x10 /* SRIOV cap */

/* Single Root I/O Virtualization */
#define ROC_PCI_SRIOV_TOTAL_VF 0x0e /* Total VFs */

static void
process_msgs(struct dev *dev, struct mbox *mbox)
{
	struct mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int msgs_acked = 0;
	int offset;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + PLT_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		msgs_acked++;
		plt_base_dbg("Message 0x%x (%s) pf:%d/vf:%d", msg->id,
			     mbox_id2name(msg->id), dev_get_pf(msg->pcifunc),
			     dev_get_vf(msg->pcifunc));

		switch (msg->id) {
			/* Add message id's that are handled here */
		case MBOX_MSG_READY:
			/* Get our identity */
			dev->pf_func = msg->pcifunc;
			break;

		default:
			if (msg->rc)
				plt_err("Message (%s) response has err=%d",
					mbox_id2name(msg->id), msg->rc);
			break;
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}

	mbox_reset(mbox, 0);
	/* Update acked if someone is waiting a message */
	mdev->msgs_acked = msgs_acked;
	plt_wmb();
}

static int
mbox_process_msgs_up(struct dev *dev, struct mbox_msghdr *req)
{
	/* Check if valid, if not reply with a invalid msg */
	if (req->sig != MBOX_REQ_SIG)
		return -EIO;

	switch (req->id) {
	default:
		reply_invalid_msg(&dev->mbox_up, 0, 0, req->id);
		break;
	}

	return -ENODEV;
}

static void
process_msgs_up(struct dev *dev, struct mbox *mbox)
{
	struct mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int i, err, offset;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + PLT_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		plt_base_dbg("Message 0x%x (%s) pf:%d/vf:%d", msg->id,
			     mbox_id2name(msg->id), dev_get_pf(msg->pcifunc),
			     dev_get_vf(msg->pcifunc));
		err = mbox_process_msgs_up(dev, msg);
		if (err)
			plt_err("Error %d handling 0x%x (%s)", err, msg->id,
				mbox_id2name(msg->id));
		offset = mbox->rx_start + msg->next_msgoff;
	}
	/* Send mbox responses */
	if (mdev->num_msgs) {
		plt_base_dbg("Reply num_msgs:%d", mdev->num_msgs);
		mbox_msg_send(mbox, 0);
	}
}

static void
roc_af_pf_mbox_irq(void *param)
{
	struct dev *dev = param;
	uint64_t intr;

	intr = plt_read64(dev->bar2 + RVU_PF_INT);
	if (intr == 0)
		plt_base_dbg("Proceeding to check mbox UP messages if any");

	plt_write64(intr, dev->bar2 + RVU_PF_INT);
	plt_base_dbg("Irq 0x%" PRIx64 "(pf:%d)", intr, dev->pf);

	/* First process all configuration messages */
	process_msgs(dev, dev->mbox);

	/* Process Uplink messages */
	process_msgs_up(dev, &dev->mbox_up);
}

static int
mbox_register_pf_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = &pci_dev->intr_handle;
	int rc;

	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* MBOX interrupt AF <-> PF */
	rc = dev_irq_register(intr_handle, roc_af_pf_mbox_irq, dev,
			      RVU_PF_INT_VEC_AFPF_MBOX);
	if (rc) {
		plt_err("Fail to register AF<->PF mbox irq");
		return rc;
	}

	plt_write64(~0ull, dev->bar2 + RVU_PF_INT);
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	return rc;
}

static int
mbox_register_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	return mbox_register_pf_irq(pci_dev, dev);
}

static void
mbox_unregister_pf_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = &pci_dev->intr_handle;

	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* MBOX interrupt AF <-> PF */
	dev_irq_unregister(intr_handle, roc_af_pf_mbox_irq, dev,
			   RVU_PF_INT_VEC_AFPF_MBOX);
}

static void
mbox_unregister_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	mbox_unregister_pf_irq(pci_dev, dev);
}

static uint16_t
dev_pf_total_vfs(struct plt_pci_device *pci_dev)
{
	uint16_t total_vfs = 0;
	int sriov_pos, rc;

	sriov_pos =
		plt_pci_find_ext_capability(pci_dev, ROC_PCI_EXT_CAP_ID_SRIOV);
	if (sriov_pos <= 0) {
		plt_warn("Unable to find SRIOV cap, rc=%d", sriov_pos);
		return 0;
	}

	rc = plt_pci_read_config(pci_dev, &total_vfs, 2,
				 sriov_pos + ROC_PCI_SRIOV_TOTAL_VF);
	if (rc < 0) {
		plt_warn("Unable to read SRIOV cap, rc=%d", rc);
		return 0;
	}

	return total_vfs;
}

static int
dev_setup_shared_lmt_region(struct mbox *mbox)
{
	struct lmtst_tbl_setup_req *req;

	req = mbox_alloc_msg_lmtst_tbl_setup(mbox);
	req->pcifunc = idev_lmt_pffunc_get();

	return mbox_process(mbox);
}

static int
dev_lmt_setup(struct plt_pci_device *pci_dev, struct dev *dev)
{
	uint64_t bar4_mbox_sz = MBOX_SIZE;
	struct idev_cfg *idev;
	int rc;

	if (roc_model_is_cn9k()) {
		dev->lmt_base = dev->bar2 + (RVU_BLOCK_ADDR_LMT << 20);
		return 0;
	}

	/* [CN10K, .) */

	/* Set common lmt region from second pf_func onwards. */
	if (!dev->disable_shared_lmt && idev_lmt_pffunc_get() &&
	    dev->pf_func != idev_lmt_pffunc_get()) {
		rc = dev_setup_shared_lmt_region(dev->mbox);
		if (!rc) {
			dev->lmt_base = roc_idev_lmt_base_addr_get();
			return rc;
		}
		plt_err("Failed to setup shared lmt region, pf_func %d err %d "
			"Using respective LMT region per pf func",
			dev->pf_func, rc);
	}

	/* PF BAR4 should always be sufficient enough to
	 * hold PF-AF MBOX + PF-VF MBOX + LMT lines.
	 */
	if (pci_dev->mem_resource[4].len <
	    (bar4_mbox_sz + (RVU_LMT_LINE_MAX * RVU_LMT_SZ))) {
		plt_err("Not enough bar4 space for lmt lines and mbox");
		return -EFAULT;
	}

	/* LMT base is just after total VF MBOX area */
	bar4_mbox_sz += (MBOX_SIZE * dev_pf_total_vfs(pci_dev));
	dev->lmt_base = dev->bar4 + bar4_mbox_sz;

	/* Base LMT address should be chosen from only those pci funcs which
	 * participate in LMT shared mode.
	 */
	if (!dev->disable_shared_lmt) {
		idev = idev_get_cfg();
		if (!__atomic_load_n(&idev->lmt_pf_func, __ATOMIC_ACQUIRE)) {
			idev->lmt_base_addr = dev->lmt_base;
			idev->lmt_pf_func = dev->pf_func;
			idev->num_lmtlines = RVU_LMT_LINE_MAX;
		}
	}

	return 0;
}

int
dev_init(struct dev *dev, struct plt_pci_device *pci_dev)
{
	int direction, up_direction, rc;
	uintptr_t bar2, bar4, mbox;
	uint64_t intr_offset;

	bar2 = (uintptr_t)pci_dev->mem_resource[2].addr;
	bar4 = (uintptr_t)pci_dev->mem_resource[4].addr;
	if (bar2 == 0 || bar4 == 0) {
		plt_err("Failed to get PCI bars");
		rc = -ENODEV;
		goto error;
	}

	/* Trigger fault on bar2 and bar4 regions
	 * to avoid BUG_ON in remap_pfn_range()
	 * in latest kernel.
	 */
	*(volatile uint64_t *)bar2;
	*(volatile uint64_t *)bar4;

	/* Check ROC model supported */
	if (roc_model->flag == 0) {
		rc = UTIL_ERR_INVALID_MODEL;
		goto error;
	}

	dev->bar2 = bar2;
	dev->bar4 = bar4;

	mbox = bar4;
	direction = MBOX_DIR_PFAF;
	up_direction = MBOX_DIR_PFAF_UP;
	intr_offset = RVU_PF_INT;

	/* Initialize the local mbox */
	rc = mbox_init(&dev->mbox_local, mbox, bar2, direction, 1, intr_offset);
	if (rc)
		goto error;
	dev->mbox = &dev->mbox_local;

	rc = mbox_init(&dev->mbox_up, mbox, bar2, up_direction, 1, intr_offset);
	if (rc)
		goto mbox_fini;

	/* Register mbox interrupts */
	rc = mbox_register_irq(pci_dev, dev);
	if (rc)
		goto mbox_fini;

	/* Check the readiness of PF/VF */
	rc = send_ready_msg(dev->mbox, &dev->pf_func);
	if (rc)
		goto mbox_unregister;

	dev->pf = dev_get_pf(dev->pf_func);

	dev->mbox_active = 1;

	/* Setup LMT line base */
	rc = dev_lmt_setup(pci_dev, dev);
	if (rc)
		goto iounmap;

	return rc;
iounmap:
mbox_unregister:
	mbox_unregister_irq(pci_dev, dev);
mbox_fini:
	mbox_fini(dev->mbox);
	mbox_fini(&dev->mbox_up);
error:
	return rc;
}

int
dev_fini(struct dev *dev, struct plt_pci_device *pci_dev)
{
	struct plt_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct mbox *mbox;

	mbox_unregister_irq(pci_dev, dev);

	/* Release PF - AF */
	mbox = dev->mbox;
	mbox_fini(mbox);
	mbox = &dev->mbox_up;
	mbox_fini(mbox);
	dev->mbox_active = 0;

	/* Disable MSIX vectors */
	dev_irqs_disable(intr_handle);
	return 0;
}
