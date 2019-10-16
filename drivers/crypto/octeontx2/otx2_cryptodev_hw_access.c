/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include "otx2_common.h"
#include "otx2_cryptodev.h"
#include "otx2_cryptodev_hw_access.h"

#include "cpt_pmd_logs.h"

static void
otx2_cpt_lf_err_intr_handler(void *param)
{
	uintptr_t base = (uintptr_t)param;
	uint8_t lf_id;
	uint64_t intr;

	lf_id = (base >> 12) & 0xFF;

	intr = otx2_read64(base + OTX2_CPT_LF_MISC_INT);
	if (intr == 0)
		return;

	CPT_LOG_ERR("LF %d MISC_INT: 0x%" PRIx64 "", lf_id, intr);

	/* Clear interrupt */
	otx2_write64(intr, base + OTX2_CPT_LF_MISC_INT);
}

static void
otx2_cpt_lf_err_intr_unregister(const struct rte_cryptodev *dev,
				uint16_t msix_off, uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;

	/* Disable error interrupts */
	otx2_write64(~0ull, base + OTX2_CPT_LF_MISC_INT_ENA_W1C);

	otx2_unregister_irq(handle, otx2_cpt_lf_err_intr_handler, (void *)base,
			    msix_off);
}

void
otx2_cpt_err_intr_unregister(const struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	uintptr_t base;
	uint32_t i;

	for (i = 0; i < vf->nb_queues; i++) {
		base = OTX2_CPT_LF_BAR2(vf, i);
		otx2_cpt_lf_err_intr_unregister(dev, vf->lf_msixoff[i], base);
	}

	vf->err_intr_registered = 0;
}

static int
otx2_cpt_lf_err_intr_register(const struct rte_cryptodev *dev,
			     uint16_t msix_off, uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	int ret;

	/* Disable error interrupts */
	otx2_write64(~0ull, base + OTX2_CPT_LF_MISC_INT_ENA_W1C);

	/* Register error interrupt handler */
	ret = otx2_register_irq(handle, otx2_cpt_lf_err_intr_handler,
				(void *)base, msix_off);
	if (ret)
		return ret;

	/* Enable error interrupts */
	otx2_write64(~0ull, base + OTX2_CPT_LF_MISC_INT_ENA_W1S);

	return 0;
}

int
otx2_cpt_err_intr_register(const struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	uint32_t i, j, ret;
	uintptr_t base;

	for (i = 0; i < vf->nb_queues; i++) {
		if (vf->lf_msixoff[i] == MSIX_VECTOR_INVALID) {
			CPT_LOG_ERR("Invalid CPT LF MSI-X offset: 0x%x",
				    vf->lf_msixoff[i]);
			return -EINVAL;
		}
	}

	for (i = 0; i < vf->nb_queues; i++) {
		base = OTX2_CPT_LF_BAR2(vf, i);
		ret = otx2_cpt_lf_err_intr_register(dev, vf->lf_msixoff[i],
						   base);
		if (ret)
			goto intr_unregister;
	}

	vf->err_intr_registered = 1;
	return 0;

intr_unregister:
	/* Unregister the ones already registered */
	for (j = 0; j < i; j++) {
		base = OTX2_CPT_LF_BAR2(vf, j);
		otx2_cpt_lf_err_intr_unregister(dev, vf->lf_msixoff[j], base);
	}

	/*
	 * Failed to register error interrupt. Not returning error as this would
	 * prevent application from enabling larger number of devs.
	 *
	 * This failure is a known issue because otx2_dev_init() initializes
	 * interrupts based on static values from ATF, and the actual number
	 * of interrupts needed (which is based on LFs) can be determined only
	 * after otx2_dev_init() sets up interrupts which includes mbox
	 * interrupts.
	 */
	return 0;
}
