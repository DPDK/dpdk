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

	/* Check the readiness of PF/VF */
	rc = otx2_send_ready_msg(dev->mbox, &dev->pf_func);
	if (rc)
		goto mbox_fini;

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
	struct otx2_dev *dev = otx2_dev;
	struct otx2_idev_cfg *idev;
	struct otx2_mbox *mbox;

	/* Clear references to this pci dev */
	idev = otx2_intra_dev_get_cfg();
	if (idev->npa_lf && idev->npa_lf->pci_dev == pci_dev)
		idev->npa_lf = NULL;

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
}
