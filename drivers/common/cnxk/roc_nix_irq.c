/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static void
nix_err_intr_enb_dis(struct nix *nix, bool enb)
{
	/* Enable all nix lf error irqs except RQ_DISABLED and CQ_DISABLED */
	if (enb)
		plt_write64(~(BIT_ULL(11) | BIT_ULL(24)),
			    nix->base + NIX_LF_ERR_INT_ENA_W1S);
	else
		plt_write64(~0ull, nix->base + NIX_LF_ERR_INT_ENA_W1C);
}

static void
nix_ras_intr_enb_dis(struct nix *nix, bool enb)
{
	if (enb)
		plt_write64(~0ull, nix->base + NIX_LF_RAS_ENA_W1S);
	else
		plt_write64(~0ull, nix->base + NIX_LF_RAS_ENA_W1C);
}

void
roc_nix_rx_queue_intr_enable(struct roc_nix *roc_nix, uint16_t rx_queue_id)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	/* Enable CINT interrupt */
	plt_write64(BIT_ULL(0), nix->base + NIX_LF_CINTX_ENA_W1S(rx_queue_id));
}

void
roc_nix_rx_queue_intr_disable(struct roc_nix *roc_nix, uint16_t rx_queue_id)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	/* Clear and disable CINT interrupt */
	plt_write64(BIT_ULL(0), nix->base + NIX_LF_CINTX_ENA_W1C(rx_queue_id));
}

void
roc_nix_err_intr_ena_dis(struct roc_nix *roc_nix, bool enb)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix_err_intr_enb_dis(nix, enb);
}

void
roc_nix_ras_intr_ena_dis(struct roc_nix *roc_nix, bool enb)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix_ras_intr_enb_dis(nix, enb);
}

static void
nix_lf_err_irq(void *param)
{
	struct nix *nix = (struct nix *)param;
	struct dev *dev = &nix->dev;
	uint64_t intr;

	intr = plt_read64(nix->base + NIX_LF_ERR_INT);
	if (intr == 0)
		return;

	plt_err("Err_irq=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Clear interrupt */
	plt_write64(intr, nix->base + NIX_LF_ERR_INT);
	/* Dump registers to std out */
	roc_nix_lf_reg_dump(nix_priv_to_roc_nix(nix), NULL);
	roc_nix_queues_ctx_dump(nix_priv_to_roc_nix(nix), NULL);
}

static int
nix_lf_register_err_irq(struct nix *nix)
{
	struct plt_intr_handle *handle = nix->pci_dev->intr_handle;
	int rc, vec;

	vec = nix->msixoff + NIX_LF_INT_VEC_ERR_INT;
	/* Clear err interrupt */
	nix_err_intr_enb_dis(nix, false);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, nix_lf_err_irq, nix, vec);
	/* Enable all dev interrupt except for RQ_DISABLED */
	nix_err_intr_enb_dis(nix, true);

	return rc;
}

static void
nix_lf_unregister_err_irq(struct nix *nix)
{
	struct plt_intr_handle *handle = nix->pci_dev->intr_handle;
	int vec;

	vec = nix->msixoff + NIX_LF_INT_VEC_ERR_INT;
	/* Clear err interrupt */
	nix_err_intr_enb_dis(nix, false);
	dev_irq_unregister(handle, nix_lf_err_irq, nix, vec);
}

static void
nix_lf_ras_irq(void *param)
{
	struct nix *nix = (struct nix *)param;
	struct dev *dev = &nix->dev;
	uint64_t intr;

	intr = plt_read64(nix->base + NIX_LF_RAS);
	if (intr == 0)
		return;

	plt_err("Ras_intr=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);
	/* Clear interrupt */
	plt_write64(intr, nix->base + NIX_LF_RAS);

	/* Dump registers to std out */
	roc_nix_lf_reg_dump(nix_priv_to_roc_nix(nix), NULL);
	roc_nix_queues_ctx_dump(nix_priv_to_roc_nix(nix), NULL);
}

static int
nix_lf_register_ras_irq(struct nix *nix)
{
	struct plt_intr_handle *handle = nix->pci_dev->intr_handle;
	int rc, vec;

	vec = nix->msixoff + NIX_LF_INT_VEC_POISON;
	/* Clear err interrupt */
	nix_ras_intr_enb_dis(nix, false);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, nix_lf_ras_irq, nix, vec);
	/* Enable dev interrupt */
	nix_ras_intr_enb_dis(nix, true);

	return rc;
}

static void
nix_lf_unregister_ras_irq(struct nix *nix)
{
	struct plt_intr_handle *handle = nix->pci_dev->intr_handle;
	int vec;

	vec = nix->msixoff + NIX_LF_INT_VEC_POISON;
	/* Clear err interrupt */
	nix_ras_intr_enb_dis(nix, false);
	dev_irq_unregister(handle, nix_lf_ras_irq, nix, vec);
}

static inline uint8_t
nix_lf_q_irq_get_and_clear(struct nix *nix, uint16_t q, uint32_t off,
			   uint64_t mask)
{
	uint64_t reg, wdata;
	uint8_t qint;

	wdata = (uint64_t)q << 44;
	reg = roc_atomic64_add_nosync(wdata, (int64_t *)(nix->base + off));

	if (reg & BIT_ULL(42) /* OP_ERR */) {
		plt_err("Failed execute irq get off=0x%x", off);
		return 0;
	}
	qint = reg & 0xff;
	wdata &= mask;
	plt_write64(wdata | qint, nix->base + off);

	return qint;
}

static inline uint8_t
nix_lf_rq_irq_get_and_clear(struct nix *nix, uint16_t rq)
{
	return nix_lf_q_irq_get_and_clear(nix, rq, NIX_LF_RQ_OP_INT, ~0xff00);
}

static inline uint8_t
nix_lf_cq_irq_get_and_clear(struct nix *nix, uint16_t cq)
{
	return nix_lf_q_irq_get_and_clear(nix, cq, NIX_LF_CQ_OP_INT, ~0xff00);
}

static inline uint8_t
nix_lf_sq_irq_get_and_clear(struct nix *nix, uint16_t sq)
{
	return nix_lf_q_irq_get_and_clear(nix, sq, NIX_LF_SQ_OP_INT, ~0x1ff00);
}

static inline bool
nix_lf_is_sqb_null(struct dev *dev, int q)
{
	bool is_sqb_null = false;
	volatile void *ctx;
	int rc;

	rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_SQ, q, &ctx);
	if (rc) {
		plt_err("Failed to get sq context");
	} else {
		is_sqb_null =
			roc_model_is_cn9k() ?
				(((__io struct nix_sq_ctx_s *)ctx)->next_sqb ==
				 0) :
				(((__io struct nix_cn10k_sq_ctx_s *)ctx)
					 ->next_sqb == 0);
	}

	return is_sqb_null;
}

static inline uint8_t
nix_lf_sq_debug_reg(struct nix *nix, uint32_t off)
{
	uint8_t err = 0;
	uint64_t reg;

	reg = plt_read64(nix->base + off);
	if (reg & BIT_ULL(44)) {
		err = reg & 0xff;
		/* Clear valid bit */
		plt_write64(BIT_ULL(44), nix->base + off);
	}

	return err;
}

static void
nix_lf_cq_irq(void *param)
{
	struct nix_qint *cint = (struct nix_qint *)param;
	struct nix *nix = cint->nix;

	/* Clear interrupt */
	plt_write64(BIT_ULL(0), nix->base + NIX_LF_CINTX_INT(cint->qintx));
}

static void
nix_lf_q_irq(void *param)
{
	struct nix_qint *qint = (struct nix_qint *)param;
	uint8_t irq, qintx = qint->qintx;
	int q, cq, rq, sq, intr_cb = 0;
	struct nix *nix = qint->nix;
	struct dev *dev = &nix->dev;
	uint64_t intr;
	uint8_t rc;

	intr = plt_read64(nix->base + NIX_LF_QINTX_INT(qintx));
	if (intr == 0)
		return;

	plt_err("Queue_intr=0x%" PRIx64 " qintx=%d pf=%d, vf=%d", intr, qintx,
		dev->pf, dev->vf);

	/* Handle RQ interrupts */
	for (q = 0; q < nix->nb_rx_queues; q++) {
		rq = q % nix->qints;
		irq = nix_lf_rq_irq_get_and_clear(nix, rq);

		if (irq & BIT_ULL(NIX_RQINT_DROP))
			plt_err("RQ=%d NIX_RQINT_DROP", rq);

		if (irq & BIT_ULL(NIX_RQINT_RED))
			plt_err("RQ=%d NIX_RQINT_RED", rq);
	}

	/* Handle CQ interrupts */
	for (q = 0; q < nix->nb_rx_queues; q++) {
		cq = q % nix->qints;
		irq = nix_lf_cq_irq_get_and_clear(nix, cq);

		if (irq & BIT_ULL(NIX_CQERRINT_DOOR_ERR))
			plt_err("CQ=%d NIX_CQERRINT_DOOR_ERR", cq);

		if (irq & BIT_ULL(NIX_CQERRINT_WR_FULL))
			plt_err("CQ=%d NIX_CQERRINT_WR_FULL", cq);

		if (irq & BIT_ULL(NIX_CQERRINT_CQE_FAULT))
			plt_err("CQ=%d NIX_CQERRINT_CQE_FAULT", cq);

		if (irq & BIT_ULL(NIX_CQERRINT_CPT_DROP))
			plt_err("CQ=%d NIX_CQERRINT_CPT_DROP", cq);
	}

	/* Handle SQ interrupts */
	for (q = 0; q < nix->nb_tx_queues; q++) {
		sq = q % nix->qints;
		irq = nix_lf_sq_irq_get_and_clear(nix, sq);

		/* Detect LMT store error */
		rc = nix_lf_sq_debug_reg(nix, NIX_LF_SQ_OP_ERR_DBG);
		if (rc)
			plt_err("SQ=%d NIX_SQINT_LMT_ERR, errcode %x", sq, rc);

		/* Detect Meta-descriptor enqueue error */
		rc = nix_lf_sq_debug_reg(nix, NIX_LF_MNQ_ERR_DBG);
		if (rc) {
			plt_err("SQ=%d NIX_SQINT_MNQ_ERR, errcode %x", sq, rc);
			intr_cb = 1;
		}

		/* Detect Send error */
		rc = nix_lf_sq_debug_reg(nix, NIX_LF_SEND_ERR_DBG);
		if (rc)
			plt_err("SQ=%d NIX_SQINT_SEND_ERR, errcode %x", sq, rc);

		/* Detect SQB fault, read SQ context to check SQB NULL case */
		if (irq & BIT_ULL(NIX_SQINT_SQB_ALLOC_FAIL) ||
		    nix_lf_is_sqb_null(dev, q))
			plt_err("SQ=%d NIX_SQINT_SQB_ALLOC_FAIL", sq);
	}

	/* Clear interrupt */
	plt_write64(intr, nix->base + NIX_LF_QINTX_INT(qintx));

	/* Dump registers to std out */
	roc_nix_lf_reg_dump(nix_priv_to_roc_nix(nix), NULL);
	roc_nix_queues_ctx_dump(nix_priv_to_roc_nix(nix), NULL);

	/* Call reset callback */
	if (intr_cb && dev->ops->q_err_cb)
		dev->ops->q_err_cb(nix_priv_to_roc_nix(nix), NULL);
}

int
roc_nix_register_queue_irqs(struct roc_nix *roc_nix)
{
	int vec, q, sqs, rqs, qs, rc = 0;
	struct plt_intr_handle *handle;
	struct nix *nix;

	nix = roc_nix_to_nix_priv(roc_nix);
	handle = nix->pci_dev->intr_handle;

	/* Figure out max qintx required */
	rqs = PLT_MIN(nix->qints, nix->nb_rx_queues);
	sqs = PLT_MIN(nix->qints, nix->nb_tx_queues);
	qs = PLT_MAX(rqs, sqs);

	nix->configured_qints = qs;

	nix->qints_mem =
		plt_zmalloc(nix->configured_qints * sizeof(struct nix_qint), 0);
	if (nix->qints_mem == NULL)
		return -ENOMEM;

	for (q = 0; q < qs; q++) {
		vec = nix->msixoff + NIX_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		plt_write64(0, nix->base + NIX_LF_QINTX_CNT(q));

		/* Clear interrupt */
		plt_write64(~0ull, nix->base + NIX_LF_QINTX_ENA_W1C(q));

		nix->qints_mem[q].nix = nix;
		nix->qints_mem[q].qintx = q;

		/* Sync qints_mem update */
		plt_wmb();

		/* Register queue irq vector */
		rc = dev_irq_register(handle, nix_lf_q_irq, &nix->qints_mem[q],
				      vec);
		if (rc)
			break;

		plt_write64(0, nix->base + NIX_LF_QINTX_CNT(q));
		plt_write64(0, nix->base + NIX_LF_QINTX_INT(q));
		/* Enable QINT interrupt */
		plt_write64(~0ull, nix->base + NIX_LF_QINTX_ENA_W1S(q));
	}

	return rc;
}

void
roc_nix_unregister_queue_irqs(struct roc_nix *roc_nix)
{
	struct plt_intr_handle *handle;
	struct nix *nix;
	int vec, q;

	nix = roc_nix_to_nix_priv(roc_nix);
	handle = nix->pci_dev->intr_handle;

	for (q = 0; q < nix->configured_qints; q++) {
		vec = nix->msixoff + NIX_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		plt_write64(0, nix->base + NIX_LF_QINTX_CNT(q));
		plt_write64(0, nix->base + NIX_LF_QINTX_INT(q));

		/* Clear interrupt */
		plt_write64(~0ull, nix->base + NIX_LF_QINTX_ENA_W1C(q));

		/* Unregister queue irq vector */
		dev_irq_unregister(handle, nix_lf_q_irq, &nix->qints_mem[q],
				   vec);
	}
	nix->configured_qints = 0;

	plt_free(nix->qints_mem);
	nix->qints_mem = NULL;
}

int
roc_nix_register_cq_irqs(struct roc_nix *roc_nix)
{
	struct plt_intr_handle *handle;
	uint8_t rc = 0, vec, q;
	struct nix *nix;

	nix = roc_nix_to_nix_priv(roc_nix);
	handle = nix->pci_dev->intr_handle;

	nix->configured_cints = PLT_MIN(nix->cints, nix->nb_rx_queues);

	nix->cints_mem =
		plt_zmalloc(nix->configured_cints * sizeof(struct nix_qint), 0);
	if (nix->cints_mem == NULL)
		return -ENOMEM;

	for (q = 0; q < nix->configured_cints; q++) {
		vec = nix->msixoff + NIX_LF_INT_VEC_CINT_START + q;

		/* Clear CINT CNT */
		plt_write64(0, nix->base + NIX_LF_CINTX_CNT(q));

		/* Clear interrupt */
		plt_write64(BIT_ULL(0), nix->base + NIX_LF_CINTX_ENA_W1C(q));

		nix->cints_mem[q].nix = nix;
		nix->cints_mem[q].qintx = q;

		/* Sync cints_mem update */
		plt_wmb();

		/* Register queue irq vector */
		rc = dev_irq_register(handle, nix_lf_cq_irq, &nix->cints_mem[q],
				      vec);
		if (rc) {
			plt_err("Fail to register CQ irq, rc=%d", rc);
			return rc;
		}

		rc = plt_intr_vec_list_alloc(handle, "cnxk",
					     nix->configured_cints);
		if (rc) {
			plt_err("Fail to allocate intr vec list, rc=%d",
				rc);
			return rc;
		}
		/* VFIO vector zero is reserved for misc interrupt so
		 * doing required adjustment. (b13bfab4cd)
		 */
		if (plt_intr_vec_list_index_set(handle, q,
						PLT_INTR_VEC_RXTX_OFFSET + vec))
			return -1;

		/* Configure CQE interrupt coalescing parameters */
		plt_write64(((CQ_CQE_THRESH_DEFAULT) |
			     (CQ_CQE_THRESH_DEFAULT << 32) |
			     (CQ_TIMER_THRESH_DEFAULT << 48)),
			    nix->base + NIX_LF_CINTX_WAIT((q)));

		/* Keeping the CQ interrupt disabled as the rx interrupt
		 * feature needs to be enabled/disabled on demand.
		 */
	}

	return rc;
}

void
roc_nix_unregister_cq_irqs(struct roc_nix *roc_nix)
{
	struct plt_intr_handle *handle;
	struct nix *nix;
	int vec, q;

	nix = roc_nix_to_nix_priv(roc_nix);
	handle = nix->pci_dev->intr_handle;

	for (q = 0; q < nix->configured_cints; q++) {
		vec = nix->msixoff + NIX_LF_INT_VEC_CINT_START + q;

		/* Clear CINT CNT */
		plt_write64(0, nix->base + NIX_LF_CINTX_CNT(q));

		/* Clear interrupt */
		plt_write64(BIT_ULL(0), nix->base + NIX_LF_CINTX_ENA_W1C(q));

		/* Unregister queue irq vector */
		dev_irq_unregister(handle, nix_lf_cq_irq, &nix->cints_mem[q],
				   vec);
	}

	plt_intr_vec_list_free(handle);
	plt_free(nix->cints_mem);
}

int
nix_register_irqs(struct nix *nix)
{
	int rc;

	if (nix->msixoff == MSIX_VECTOR_INVALID) {
		plt_err("Invalid NIXLF MSIX vector offset vector: 0x%x",
			nix->msixoff);
		return NIX_ERR_PARAM;
	}

	/* Register lf err interrupt */
	rc = nix_lf_register_err_irq(nix);
	/* Register RAS interrupt */
	rc |= nix_lf_register_ras_irq(nix);

	return rc;
}

void
nix_unregister_irqs(struct nix *nix)
{
	nix_lf_unregister_err_irq(nix);
	nix_lf_unregister_ras_irq(nix);
}
