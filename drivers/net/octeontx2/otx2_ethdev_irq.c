/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_bus_pci.h>
#include <rte_malloc.h>

#include "otx2_ethdev.h"

static void
nix_lf_err_irq(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t intr;

	intr = otx2_read64(dev->base + NIX_LF_ERR_INT);
	if (intr == 0)
		return;

	otx2_err("Err_intr=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Clear interrupt */
	otx2_write64(intr, dev->base + NIX_LF_ERR_INT);

	/* Dump registers to std out */
	otx2_nix_reg_dump(dev, NULL);
	otx2_nix_queues_ctx_dump(eth_dev);
}

static int
nix_lf_register_err_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	otx2_nix_err_intr_enb_dis(eth_dev, false);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, nix_lf_err_irq, eth_dev, vec);
	/* Enable all dev interrupt except for RQ_DISABLED */
	otx2_nix_err_intr_enb_dis(eth_dev, true);

	return rc;
}

static void
nix_lf_unregister_err_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	otx2_nix_err_intr_enb_dis(eth_dev, false);
	otx2_unregister_irq(handle, nix_lf_err_irq, eth_dev, vec);
}

static void
nix_lf_ras_irq(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t intr;

	intr = otx2_read64(dev->base + NIX_LF_RAS);
	if (intr == 0)
		return;

	otx2_err("Ras_intr=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Clear interrupt */
	otx2_write64(intr, dev->base + NIX_LF_RAS);

	/* Dump registers to std out */
	otx2_nix_reg_dump(dev, NULL);
	otx2_nix_queues_ctx_dump(eth_dev);
}

static int
nix_lf_register_ras_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	otx2_nix_ras_intr_enb_dis(eth_dev, false);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, nix_lf_ras_irq, eth_dev, vec);
	/* Enable dev interrupt */
	otx2_nix_ras_intr_enb_dis(eth_dev, true);

	return rc;
}

static void
nix_lf_unregister_ras_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	otx2_nix_ras_intr_enb_dis(eth_dev, false);
	otx2_unregister_irq(handle, nix_lf_ras_irq, eth_dev, vec);
}

static inline uint8_t
nix_lf_q_irq_get_and_clear(struct otx2_eth_dev *dev, uint16_t q,
			   uint32_t off, uint64_t mask)
{
	uint64_t reg, wdata;
	uint8_t qint;

	wdata = (uint64_t)q << 44;
	reg = otx2_atomic64_add_nosync(wdata, (int64_t *)(dev->base + off));

	if (reg & BIT_ULL(42) /* OP_ERR */) {
		otx2_err("Failed execute irq get off=0x%x", off);
		return 0;
	}

	qint = reg & 0xff;
	wdata &= mask;
	otx2_write64(wdata | qint, dev->base + off);

	return qint;
}

static inline uint8_t
nix_lf_rq_irq_get_and_clear(struct otx2_eth_dev *dev, uint16_t rq)
{
	return nix_lf_q_irq_get_and_clear(dev, rq, NIX_LF_RQ_OP_INT, ~0xff00);
}

static inline uint8_t
nix_lf_cq_irq_get_and_clear(struct otx2_eth_dev *dev, uint16_t cq)
{
	return nix_lf_q_irq_get_and_clear(dev, cq, NIX_LF_CQ_OP_INT, ~0xff00);
}

static inline uint8_t
nix_lf_sq_irq_get_and_clear(struct otx2_eth_dev *dev, uint16_t sq)
{
	return nix_lf_q_irq_get_and_clear(dev, sq, NIX_LF_SQ_OP_INT, ~0x1ff00);
}

static inline void
nix_lf_sq_debug_reg(struct otx2_eth_dev *dev, uint32_t off)
{
	uint64_t reg;

	reg = otx2_read64(dev->base + off);
	if (reg & BIT_ULL(44))
		otx2_err("SQ=%d err_code=0x%x",
			 (int)((reg >> 8) & 0xfffff), (uint8_t)(reg & 0xff));
}

static void
nix_lf_cq_irq(void *param)
{
	struct otx2_qint *cint = (struct otx2_qint *)param;
	struct rte_eth_dev *eth_dev = cint->eth_dev;
	struct otx2_eth_dev *dev;

	dev = otx2_eth_pmd_priv(eth_dev);
	/* Clear interrupt */
	otx2_write64(BIT_ULL(0), dev->base + NIX_LF_CINTX_INT(cint->qintx));
}

static void
nix_lf_q_irq(void *param)
{
	struct otx2_qint *qint = (struct otx2_qint *)param;
	struct rte_eth_dev *eth_dev = qint->eth_dev;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint8_t irq, qintx = qint->qintx;
	int q, cq, rq, sq;
	uint64_t intr;

	intr = otx2_read64(dev->base + NIX_LF_QINTX_INT(qintx));
	if (intr == 0)
		return;

	otx2_err("Queue_intr=0x%" PRIx64 " qintx=%d pf=%d, vf=%d",
		 intr, qintx, dev->pf, dev->vf);

	/* Handle RQ interrupts */
	for (q = 0; q < eth_dev->data->nb_rx_queues; q++) {
		rq = q % dev->qints;
		irq = nix_lf_rq_irq_get_and_clear(dev, rq);

		if (irq & BIT_ULL(NIX_RQINT_DROP))
			otx2_err("RQ=%d NIX_RQINT_DROP", rq);

		if (irq & BIT_ULL(NIX_RQINT_RED))
			otx2_err("RQ=%d NIX_RQINT_RED",	rq);
	}

	/* Handle CQ interrupts */
	for (q = 0; q < eth_dev->data->nb_rx_queues; q++) {
		cq = q % dev->qints;
		irq = nix_lf_cq_irq_get_and_clear(dev, cq);

		if (irq & BIT_ULL(NIX_CQERRINT_DOOR_ERR))
			otx2_err("CQ=%d NIX_CQERRINT_DOOR_ERR", cq);

		if (irq & BIT_ULL(NIX_CQERRINT_WR_FULL))
			otx2_err("CQ=%d NIX_CQERRINT_WR_FULL", cq);

		if (irq & BIT_ULL(NIX_CQERRINT_CQE_FAULT))
			otx2_err("CQ=%d NIX_CQERRINT_CQE_FAULT", cq);
	}

	/* Handle SQ interrupts */
	for (q = 0; q < eth_dev->data->nb_tx_queues; q++) {
		sq = q % dev->qints;
		irq = nix_lf_sq_irq_get_and_clear(dev, sq);

		if (irq & BIT_ULL(NIX_SQINT_LMT_ERR)) {
			otx2_err("SQ=%d NIX_SQINT_LMT_ERR", sq);
			nix_lf_sq_debug_reg(dev, NIX_LF_SQ_OP_ERR_DBG);
		}
		if (irq & BIT_ULL(NIX_SQINT_MNQ_ERR)) {
			otx2_err("SQ=%d NIX_SQINT_MNQ_ERR", sq);
			nix_lf_sq_debug_reg(dev, NIX_LF_MNQ_ERR_DBG);
		}
		if (irq & BIT_ULL(NIX_SQINT_SEND_ERR)) {
			otx2_err("SQ=%d NIX_SQINT_SEND_ERR", sq);
			nix_lf_sq_debug_reg(dev, NIX_LF_SEND_ERR_DBG);
		}
		if (irq & BIT_ULL(NIX_SQINT_SQB_ALLOC_FAIL)) {
			otx2_err("SQ=%d NIX_SQINT_SQB_ALLOC_FAIL", sq);
			nix_lf_sq_debug_reg(dev, NIX_LF_SEND_ERR_DBG);
		}
	}

	/* Clear interrupt */
	otx2_write64(intr, dev->base + NIX_LF_QINTX_INT(qintx));

	/* Dump registers to std out */
	otx2_nix_reg_dump(dev, NULL);
	otx2_nix_queues_ctx_dump(eth_dev);
}

int
oxt2_nix_register_queue_irqs(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec, q, sqs, rqs, qs, rc = 0;

	/* Figure out max qintx required */
	rqs = RTE_MIN(dev->qints, eth_dev->data->nb_rx_queues);
	sqs = RTE_MIN(dev->qints, eth_dev->data->nb_tx_queues);
	qs  = RTE_MAX(rqs, sqs);

	dev->configured_qints = qs;

	for (q = 0; q < qs; q++) {
		vec = dev->nix_msixoff + NIX_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		otx2_write64(0, dev->base + NIX_LF_QINTX_CNT(q));

		/* Clear interrupt */
		otx2_write64(~0ull, dev->base + NIX_LF_QINTX_ENA_W1C(q));

		dev->qints_mem[q].eth_dev = eth_dev;
		dev->qints_mem[q].qintx = q;

		/* Sync qints_mem update */
		rte_smp_wmb();

		/* Register queue irq vector */
		rc = otx2_register_irq(handle, nix_lf_q_irq,
				       &dev->qints_mem[q], vec);
		if (rc)
			break;

		otx2_write64(0, dev->base + NIX_LF_QINTX_CNT(q));
		otx2_write64(0, dev->base + NIX_LF_QINTX_INT(q));
		/* Enable QINT interrupt */
		otx2_write64(~0ull, dev->base + NIX_LF_QINTX_ENA_W1S(q));
	}

	return rc;
}

void
oxt2_nix_unregister_queue_irqs(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec, q;

	for (q = 0; q < dev->configured_qints; q++) {
		vec = dev->nix_msixoff + NIX_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		otx2_write64(0, dev->base + NIX_LF_QINTX_CNT(q));
		otx2_write64(0, dev->base + NIX_LF_QINTX_INT(q));

		/* Clear interrupt */
		otx2_write64(~0ull, dev->base + NIX_LF_QINTX_ENA_W1C(q));

		/* Unregister queue irq vector */
		otx2_unregister_irq(handle, nix_lf_q_irq,
				    &dev->qints_mem[q], vec);
	}
}

int
oxt2_nix_register_cq_irqs(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint8_t rc = 0, vec, q;

	dev->configured_cints = RTE_MIN(dev->cints,
					eth_dev->data->nb_rx_queues);

	for (q = 0; q < dev->configured_cints; q++) {
		vec = dev->nix_msixoff + NIX_LF_INT_VEC_CINT_START + q;

		/* Clear CINT CNT */
		otx2_write64(0, dev->base + NIX_LF_CINTX_CNT(q));

		/* Clear interrupt */
		otx2_write64(BIT_ULL(0), dev->base + NIX_LF_CINTX_ENA_W1C(q));

		dev->cints_mem[q].eth_dev = eth_dev;
		dev->cints_mem[q].qintx = q;

		/* Sync cints_mem update */
		rte_smp_wmb();

		/* Register queue irq vector */
		rc = otx2_register_irq(handle, nix_lf_cq_irq,
				       &dev->cints_mem[q], vec);
		if (rc) {
			otx2_err("Fail to register CQ irq, rc=%d", rc);
			return rc;
		}

		if (!handle->intr_vec) {
			handle->intr_vec = rte_zmalloc("intr_vec",
					    dev->configured_cints *
					    sizeof(int), 0);
			if (!handle->intr_vec) {
				otx2_err("Failed to allocate %d rx intr_vec",
					 dev->configured_cints);
				return -ENOMEM;
			}
		}
		/* VFIO vector zero is resereved for misc interrupt so
		 * doing required adjustment. (b13bfab4cd)
		 */
		handle->intr_vec[q] = RTE_INTR_VEC_RXTX_OFFSET + vec;

		/* Configure CQE interrupt coalescing parameters */
		otx2_write64(((CQ_CQE_THRESH_DEFAULT) |
			      (CQ_CQE_THRESH_DEFAULT << 32) |
			      (CQ_TIMER_THRESH_DEFAULT << 48)),
			     dev->base + NIX_LF_CINTX_WAIT((q)));

		/* Keeping the CQ interrupt disabled as the rx interrupt
		 * feature needs to be enabled/disabled on demand.
		 */
	}

	return rc;
}

void
oxt2_nix_unregister_cq_irqs(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec, q;

	for (q = 0; q < dev->configured_cints; q++) {
		vec = dev->nix_msixoff + NIX_LF_INT_VEC_CINT_START + q;

		/* Clear CINT CNT */
		otx2_write64(0, dev->base + NIX_LF_CINTX_CNT(q));

		/* Clear interrupt */
		otx2_write64(BIT_ULL(0), dev->base + NIX_LF_CINTX_ENA_W1C(q));

		/* Unregister queue irq vector */
		otx2_unregister_irq(handle, nix_lf_cq_irq,
				    &dev->cints_mem[q], vec);
	}
}

int
otx2_nix_register_irqs(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc;

	if (dev->nix_msixoff == MSIX_VECTOR_INVALID) {
		otx2_err("Invalid NIXLF MSIX vector offset vector: 0x%x",
			 dev->nix_msixoff);
		return -EINVAL;
	}

	/* Register lf err interrupt */
	rc = nix_lf_register_err_irq(eth_dev);
	/* Register RAS interrupt */
	rc |= nix_lf_register_ras_irq(eth_dev);

	return rc;
}

void
otx2_nix_unregister_irqs(struct rte_eth_dev *eth_dev)
{
	nix_lf_unregister_err_irq(eth_dev);
	nix_lf_unregister_ras_irq(eth_dev);
}

int
otx2_nix_rx_queue_intr_enable(struct rte_eth_dev *eth_dev,
			      uint16_t rx_queue_id)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* Enable CINT interrupt */
	otx2_write64(BIT_ULL(0), dev->base +
		     NIX_LF_CINTX_ENA_W1S(rx_queue_id));

	return 0;
}

int
otx2_nix_rx_queue_intr_disable(struct rte_eth_dev *eth_dev,
			       uint16_t rx_queue_id)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* Clear and disable CINT interrupt */
	otx2_write64(BIT_ULL(0), dev->base +
		     NIX_LF_CINTX_ENA_W1C(rx_queue_id));

	return 0;
}

void
otx2_nix_err_intr_enb_dis(struct rte_eth_dev *eth_dev, bool enb)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* Enable all nix lf error interrupts except
	 * RQ_DISABLED and CQ_DISABLED.
	 */
	if (enb)
		otx2_write64(~(BIT_ULL(11) | BIT_ULL(24)),
			     dev->base + NIX_LF_ERR_INT_ENA_W1S);
	else
		otx2_write64(~0ull, dev->base + NIX_LF_ERR_INT_ENA_W1C);
}

void
otx2_nix_ras_intr_enb_dis(struct rte_eth_dev *eth_dev, bool enb)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	if (enb)
		otx2_write64(~0ull, dev->base + NIX_LF_RAS_ENA_W1S);
	else
		otx2_write64(~0ull, dev->base + NIX_LF_RAS_ENA_W1C);
}
