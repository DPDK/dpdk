/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */
#include <rte_cryptodev.h>

#include "otx2_common.h"
#include "otx2_cryptodev.h"
#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_cryptodev_ops.h"
#include "otx2_dev.h"

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
	struct rte_intr_handle *handle = pci_dev->intr_handle;

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
		base = OTX2_CPT_LF_BAR2(vf, vf->lf_blkaddr[i], i);
		otx2_cpt_lf_err_intr_unregister(dev, vf->lf_msixoff[i], base);
	}

	vf->err_intr_registered = 0;
}

static int
otx2_cpt_lf_err_intr_register(const struct rte_cryptodev *dev,
			     uint16_t msix_off, uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	struct rte_intr_handle *handle = pci_dev->intr_handle;
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
		base = OTX2_CPT_LF_BAR2(vf, vf->lf_blkaddr[i], i);
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
		base = OTX2_CPT_LF_BAR2(vf, vf->lf_blkaddr[j], j);
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

int
otx2_cpt_iq_enable(const struct rte_cryptodev *dev,
		   const struct otx2_cpt_qp *qp, uint8_t grp_mask, uint8_t pri,
		   uint32_t size_div40)
{
	union otx2_cpt_af_lf_ctl af_lf_ctl;
	union otx2_cpt_lf_inprog inprog;
	union otx2_cpt_lf_q_base base;
	union otx2_cpt_lf_q_size size;
	union otx2_cpt_lf_ctl lf_ctl;
	int ret;

	/* Set engine group mask and priority */

	ret = otx2_cpt_af_reg_read(dev, OTX2_CPT_AF_LF_CTL(qp->id),
				   qp->blkaddr, &af_lf_ctl.u);
	if (ret)
		return ret;
	af_lf_ctl.s.grp = grp_mask;
	af_lf_ctl.s.pri = pri ? 1 : 0;
	ret = otx2_cpt_af_reg_write(dev, OTX2_CPT_AF_LF_CTL(qp->id),
				    qp->blkaddr, af_lf_ctl.u);
	if (ret)
		return ret;

	/* Set instruction queue base address */

	base.u = otx2_read64(qp->base + OTX2_CPT_LF_Q_BASE);
	base.s.fault = 0;
	base.s.stopped = 0;
	base.s.addr = qp->iq_dma_addr >> 7;
	otx2_write64(base.u, qp->base + OTX2_CPT_LF_Q_BASE);

	/* Set instruction queue size */

	size.u = otx2_read64(qp->base + OTX2_CPT_LF_Q_SIZE);
	size.s.size_div40 = size_div40;
	otx2_write64(size.u, qp->base + OTX2_CPT_LF_Q_SIZE);

	/* Enable instruction queue */

	lf_ctl.u = otx2_read64(qp->base + OTX2_CPT_LF_CTL);
	lf_ctl.s.ena = 1;
	otx2_write64(lf_ctl.u, qp->base + OTX2_CPT_LF_CTL);

	/* Start instruction execution */

	inprog.u = otx2_read64(qp->base + OTX2_CPT_LF_INPROG);
	inprog.s.eena = 1;
	otx2_write64(inprog.u, qp->base + OTX2_CPT_LF_INPROG);

	return 0;
}

void
otx2_cpt_iq_disable(struct otx2_cpt_qp *qp)
{
	union otx2_cpt_lf_q_grp_ptr grp_ptr;
	union otx2_cpt_lf_inprog inprog;
	union otx2_cpt_lf_ctl ctl;
	int cnt;

	/* Stop instruction execution */
	inprog.u = otx2_read64(qp->base + OTX2_CPT_LF_INPROG);
	inprog.s.eena = 0x0;
	otx2_write64(inprog.u, qp->base + OTX2_CPT_LF_INPROG);

	/* Disable instructions enqueuing */
	ctl.u = otx2_read64(qp->base + OTX2_CPT_LF_CTL);
	ctl.s.ena = 0;
	otx2_write64(ctl.u, qp->base + OTX2_CPT_LF_CTL);

	/* Wait for instruction queue to become empty */
	cnt = 0;
	do {
		inprog.u = otx2_read64(qp->base + OTX2_CPT_LF_INPROG);
		if (inprog.s.grb_partial)
			cnt = 0;
		else
			cnt++;
		grp_ptr.u = otx2_read64(qp->base + OTX2_CPT_LF_Q_GRP_PTR);
	} while ((cnt < 10) && (grp_ptr.s.nq_ptr != grp_ptr.s.dq_ptr));

	cnt = 0;
	do {
		inprog.u = otx2_read64(qp->base + OTX2_CPT_LF_INPROG);
		if ((inprog.s.inflight == 0) &&
		    (inprog.s.gwb_cnt < 40) &&
		    ((inprog.s.grb_cnt == 0) || (inprog.s.grb_cnt == 40)))
			cnt++;
		else
			cnt = 0;
	} while (cnt < 10);
}
