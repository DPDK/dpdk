/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_regexdev_hw_access.h"
#include "otx2_regexdev_mbox.h"

static void
ree_lf_err_intr_handler(void *param)
{
	uintptr_t base = (uintptr_t)param;
	uint8_t lf_id;
	uint64_t intr;

	lf_id = (base >> 12) & 0xFF;

	intr = otx2_read64(base + OTX2_REE_LF_MISC_INT);
	if (intr == 0)
		return;

	otx2_ree_dbg("LF %d MISC_INT: 0x%" PRIx64 "", lf_id, intr);

	/* Clear interrupt */
	otx2_write64(intr, base + OTX2_REE_LF_MISC_INT);
}

static void
ree_lf_err_intr_unregister(const struct rte_regexdev *dev, uint16_t msix_off,
			   uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;

	/* Disable error interrupts */
	otx2_write64(~0ull, base + OTX2_REE_LF_MISC_INT_ENA_W1C);

	otx2_unregister_irq(handle, ree_lf_err_intr_handler, (void *)base,
			    msix_off);
}

void
otx2_ree_err_intr_unregister(const struct rte_regexdev *dev)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	uintptr_t base;
	uint32_t i;

	for (i = 0; i < vf->nb_queues; i++) {
		base = OTX2_REE_LF_BAR2(vf, i);
		ree_lf_err_intr_unregister(dev, vf->lf_msixoff[i], base);
	}

	vf->err_intr_registered = 0;
}

static int
ree_lf_err_intr_register(const struct rte_regexdev *dev, uint16_t msix_off,
			 uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	int ret;

	/* Disable error interrupts */
	otx2_write64(~0ull, base + OTX2_REE_LF_MISC_INT_ENA_W1C);

	/* Register error interrupt handler */
	ret = otx2_register_irq(handle, ree_lf_err_intr_handler, (void *)base,
				msix_off);
	if (ret)
		return ret;

	/* Enable error interrupts */
	otx2_write64(~0ull, base + OTX2_REE_LF_MISC_INT_ENA_W1S);

	return 0;
}

int
otx2_ree_err_intr_register(const struct rte_regexdev *dev)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	uint32_t i, j, ret;
	uintptr_t base;

	for (i = 0; i < vf->nb_queues; i++) {
		if (vf->lf_msixoff[i] == MSIX_VECTOR_INVALID) {
			otx2_err("Invalid REE LF MSI-X offset: 0x%x",
				    vf->lf_msixoff[i]);
			return -EINVAL;
		}
	}

	for (i = 0; i < vf->nb_queues; i++) {
		base = OTX2_REE_LF_BAR2(vf, i);
		ret = ree_lf_err_intr_register(dev, vf->lf_msixoff[i], base);
		if (ret)
			goto intr_unregister;
	}

	vf->err_intr_registered = 1;
	return 0;

intr_unregister:
	/* Unregister the ones already registered */
	for (j = 0; j < i; j++) {
		base = OTX2_REE_LF_BAR2(vf, j);
		ree_lf_err_intr_unregister(dev, vf->lf_msixoff[j], base);
	}
	return ret;
}

int
otx2_ree_iq_enable(const struct rte_regexdev *dev, const struct otx2_ree_qp *qp,
		   uint8_t pri, uint32_t size_div2)
{
	union otx2_ree_lf_sbuf_addr base;
	union otx2_ree_lf_ena lf_ena;

	/* Set instruction queue size and priority */
	otx2_ree_config_lf(dev, qp->id, pri, size_div2);

	/* Set instruction queue base address */
	/* Should be written after SBUF_CTL and before LF_ENA */

	base.u = otx2_read64(qp->base + OTX2_REE_LF_SBUF_ADDR);
	base.s.ptr = qp->iq_dma_addr >> 7;
	otx2_write64(base.u, qp->base + OTX2_REE_LF_SBUF_ADDR);

	/* Enable instruction queue */

	lf_ena.u = otx2_read64(qp->base + OTX2_REE_LF_ENA);
	lf_ena.s.ena = 1;
	otx2_write64(lf_ena.u, qp->base + OTX2_REE_LF_ENA);

	return 0;
}

void
otx2_ree_iq_disable(struct otx2_ree_qp *qp)
{
	union otx2_ree_lf_ena lf_ena;

	/* Stop instruction execution */
	lf_ena.u = otx2_read64(qp->base + OTX2_REE_LF_ENA);
	lf_ena.s.ena = 0x0;
	otx2_write64(lf_ena.u, qp->base + OTX2_REE_LF_ENA);
}

int
otx2_ree_max_matches_get(const struct rte_regexdev *dev, uint8_t *max_matches)
{
	union otx2_ree_af_reexm_max_match reexm_max_match;
	int ret;

	ret = otx2_ree_af_reg_read(dev, REE_AF_REEXM_MAX_MATCH,
				   &reexm_max_match.u);
	if (ret)
		return ret;

	*max_matches = reexm_max_match.s.max;
	return 0;
}
