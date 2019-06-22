/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_memcpy.h>

#include "otx2_dev.h"
#include "otx2_mbox.h"

#define RVU_MAX_VF		64 /* RVU_PF_VFPF_MBOX_INT(0..1) */
#define RVU_MAX_INT_RETRY	3

/* PF/VF message handling timer */
#define VF_PF_MBOX_TIMER_MS	(20 * 1000)

static void *
mbox_mem_map(off_t off, size_t size)
{
	void *va = MAP_FAILED;
	int mem_fd;

	if (size <= 0)
		goto error;

	mem_fd = open("/dev/mem", O_RDWR);
	if (mem_fd < 0)
		goto error;

	va = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, off);
	close(mem_fd);

	if (va == MAP_FAILED)
		otx2_err("Failed to mmap sz=0x%zx, fd=%d, off=%jd",
			 size, mem_fd, (intmax_t)off);
error:
	return va;
}

static void
mbox_mem_unmap(void *va, size_t size)
{
	if (va)
		munmap(va, size);
}

static void
otx2_process_msgs(struct otx2_dev *dev, struct otx2_mbox *mbox)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int msgs_acked = 0;
	int offset;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + RTE_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		msgs_acked++;
		otx2_base_dbg("Message 0x%x (%s) pf:%d/vf:%d",
			      msg->id, otx2_mbox_id2name(msg->id),
			      otx2_get_pf(msg->pcifunc),
			      otx2_get_vf(msg->pcifunc));

		switch (msg->id) {
			/* Add message id's that are handled here */
		case MBOX_MSG_READY:
			/* Get our identity */
			dev->pf_func = msg->pcifunc;
			break;

		default:
			if (msg->rc)
				otx2_err("Message (%s) response has err=%d",
					 otx2_mbox_id2name(msg->id), msg->rc);
			break;
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}

	otx2_mbox_reset(mbox, 0);
	/* Update acked if someone is waiting a message */
	mdev->msgs_acked = msgs_acked;
	rte_wmb();
}

static void
otx2_af_pf_mbox_irq(void *param)
{
	struct otx2_dev *dev = param;
	uint64_t intr;

	intr = otx2_read64(dev->bar2 + RVU_PF_INT);
	if (intr == 0)
		return;

	otx2_write64(intr, dev->bar2 + RVU_PF_INT);

	otx2_base_dbg("Irq 0x%" PRIx64 "(pf:%d,vf:%d)", intr, dev->pf, dev->vf);
	if (intr)
		/* First process all configuration messages */
		otx2_process_msgs(dev, dev->mbox);
}

static int
mbox_register_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	int rc;

	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	dev->timer_set = 0;

	/* MBOX interrupt AF <-> PF */
	rc = otx2_register_irq(intr_handle, otx2_af_pf_mbox_irq,
			       dev, RVU_PF_INT_VEC_AFPF_MBOX);
	if (rc) {
		otx2_err("Fail to register AF<->PF mbox irq");
		return rc;
	}

	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT);
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	return rc;
}

static void
mbox_unregister_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	dev->timer_set = 0;

	/* MBOX interrupt AF <-> PF */
	otx2_unregister_irq(intr_handle, otx2_af_pf_mbox_irq, dev,
			    RVU_PF_INT_VEC_AFPF_MBOX);
}

static void
otx2_update_pass_hwcap(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	RTE_SET_USED(pci_dev);

	/* Update this logic when we have A1 */
	dev->hwcap |= OTX2_HWCAP_F_A0;
}

static void
otx2_update_vf_hwcap(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	dev->hwcap = 0;

	switch (pci_dev->id.device_id) {
	case PCI_DEVID_OCTEONTX2_RVU_PF:
		break;
	case PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_VF:
	case PCI_DEVID_OCTEONTX2_RVU_NPA_VF:
	case PCI_DEVID_OCTEONTX2_RVU_CPT_VF:
	case PCI_DEVID_OCTEONTX2_RVU_AF_VF:
	case PCI_DEVID_OCTEONTX2_RVU_VF:
		dev->hwcap |= OTX2_HWCAP_F_VF;
		break;
	}
}

/**
 * @internal
 * Initialize the otx2 device
 */
int
otx2_dev_init(struct rte_pci_device *pci_dev, void *otx2_dev)
{
	int up_direction = MBOX_DIR_PFAF_UP;
	int rc, direction = MBOX_DIR_PFAF;
	struct otx2_dev *dev = otx2_dev;
	uintptr_t bar2, bar4;
	uint64_t bar4_addr;
	void *hwbase;

	bar2 = (uintptr_t)pci_dev->mem_resource[2].addr;
	bar4 = (uintptr_t)pci_dev->mem_resource[4].addr;

	if (bar2 == 0 || bar4 == 0) {
		otx2_err("Failed to get pci bars");
		rc = -ENODEV;
		goto error;
	}

	dev->node = pci_dev->device.numa_node;
	dev->maxvf = pci_dev->max_vfs;
	dev->bar2 = bar2;
	dev->bar4 = bar4;

	otx2_update_vf_hwcap(pci_dev, dev);
	otx2_update_pass_hwcap(pci_dev, dev);

	if (otx2_dev_is_vf(dev)) {
		direction = MBOX_DIR_VFPF;
		up_direction = MBOX_DIR_VFPF_UP;
	}

	/* Initialize the local mbox */
	rc = otx2_mbox_init(&dev->mbox_local, bar4, bar2, direction, 1);
	if (rc)
		goto error;
	dev->mbox = &dev->mbox_local;

	rc = otx2_mbox_init(&dev->mbox_up, bar4, bar2, up_direction, 1);
	if (rc)
		goto error;

	/* Register mbox interrupts */
	rc = mbox_register_irq(pci_dev, dev);
	if (rc)
		goto mbox_fini;

	/* Check the readiness of PF/VF */
	rc = otx2_send_ready_msg(dev->mbox, &dev->pf_func);
	if (rc)
		goto mbox_unregister;

	dev->pf = otx2_get_pf(dev->pf_func);
	dev->vf = otx2_get_vf(dev->pf_func);
	memset(&dev->active_vfs, 0, sizeof(dev->active_vfs));

	/* Found VF devices in a PF device */
	if (pci_dev->max_vfs > 0) {

		/* Remap mbox area for all vf's */
		bar4_addr = otx2_read64(bar2 + RVU_PF_VF_BAR4_ADDR);
		if (bar4_addr == 0) {
			rc = -ENODEV;
			goto mbox_fini;
		}

		hwbase = mbox_mem_map(bar4_addr, MBOX_SIZE * pci_dev->max_vfs);
		if (hwbase == MAP_FAILED) {
			rc = -ENOMEM;
			goto mbox_fini;
		}
		/* Init mbox object */
		rc = otx2_mbox_init(&dev->mbox_vfpf, (uintptr_t)hwbase,
				    bar2, MBOX_DIR_PFVF, pci_dev->max_vfs);
		if (rc)
			goto iounmap;

		/* PF -> VF UP messages */
		rc = otx2_mbox_init(&dev->mbox_vfpf_up, (uintptr_t)hwbase,
				    bar2, MBOX_DIR_PFVF_UP, pci_dev->max_vfs);
		if (rc)
			goto mbox_fini;
	}

	dev->mbox_active = 1;
	return rc;

iounmap:
	mbox_mem_unmap(hwbase, MBOX_SIZE * pci_dev->max_vfs);
mbox_unregister:
	mbox_unregister_irq(pci_dev, dev);
mbox_fini:
	otx2_mbox_fini(dev->mbox);
	otx2_mbox_fini(&dev->mbox_up);
error:
	return rc;
}

/**
 * @internal
 * Finalize the otx2 device
 */
void
otx2_dev_fini(struct rte_pci_device *pci_dev, void *otx2_dev)
{
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct otx2_dev *dev = otx2_dev;
	struct otx2_idev_cfg *idev;
	struct otx2_mbox *mbox;

	/* Clear references to this pci dev */
	idev = otx2_intra_dev_get_cfg();
	if (idev->npa_lf && idev->npa_lf->pci_dev == pci_dev)
		idev->npa_lf = NULL;

	mbox_unregister_irq(pci_dev, dev);

	/* Release PF - VF */
	mbox = &dev->mbox_vfpf;
	if (mbox->hwbase && mbox->dev)
		mbox_mem_unmap((void *)mbox->hwbase,
			       MBOX_SIZE * pci_dev->max_vfs);
	otx2_mbox_fini(mbox);
	mbox = &dev->mbox_vfpf_up;
	otx2_mbox_fini(mbox);

	/* Release PF - AF */
	mbox = dev->mbox;
	otx2_mbox_fini(mbox);
	mbox = &dev->mbox_up;
	otx2_mbox_fini(mbox);
	dev->mbox_active = 0;

	/* Disable MSIX vectors */
	otx2_disable_irqs(intr_handle);
}
