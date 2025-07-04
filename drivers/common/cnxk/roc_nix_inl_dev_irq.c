/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define WORK_LIMIT 1000

static void
nix_inl_sso_work_cb(struct nix_inl_dev *inl_dev)
{
	uintptr_t getwrk_op = inl_dev->ssow_base + SSOW_LF_GWS_OP_GET_WORK0;
	uintptr_t tag_wqe_op = inl_dev->ssow_base + SSOW_LF_GWS_WQE0;
	uint32_t wdata = BIT(16) | 1;
	union {
		__uint128_t get_work;
		uint64_t u64[2];
	} gw;
	uint16_t cnt = 0;
	uint64_t work;

again:
	/* Try to do get work */
	gw.get_work = wdata;
	plt_write64(gw.u64[0], getwrk_op);
	do {
		roc_load_pair(gw.u64[0], gw.u64[1], tag_wqe_op);
	} while (gw.u64[0] & BIT_ULL(63));

	work = gw.u64[1];
	/* Do we have any work? */
	if (work) {
		if (inl_dev->work_cb)
			inl_dev->work_cb(gw.u64, inl_dev->cb_args, false);
		else
			plt_warn("Undelivered inl dev work gw0: %p gw1: %p",
				 (void *)gw.u64[0], (void *)gw.u64[1]);
		cnt++;
		if (cnt < WORK_LIMIT)
			goto again;
	}

	inl_dev->sso_work_cnt += cnt;
	plt_atomic_thread_fence(__ATOMIC_ACQ_REL);
}

static int
nix_inl_nix_reg_dump(struct nix_inl_dev *inl_dev)
{
	uintptr_t nix_base = inl_dev->nix_base;

	/* General registers */
	nix_lf_gen_reg_dump(nix_base, NULL);

	/* Rx, Tx stat registers */
	nix_lf_stat_reg_dump(nix_base, NULL, inl_dev->lf_tx_stats,
			     inl_dev->lf_rx_stats);

	/* Intr registers */
	nix_lf_int_reg_dump(nix_base, NULL, inl_dev->qints, inl_dev->cints);

	return 0;
}

static void
nix_inl_sso_hwgrp_irq(void *param)
{
	struct nix_inl_dev *inl_dev = (struct nix_inl_dev *)param;
	uintptr_t sso_base = inl_dev->sso_base;
	uint64_t intr;

	intr = plt_read64(sso_base + SSO_LF_GGRP_INT);
	if (intr == 0)
		return;

	/* Check for work executable interrupt */
	if (intr & BIT(1))
		nix_inl_sso_work_cb(inl_dev);

	if (intr & ~BIT(1))
		plt_err("GGRP 0 GGRP_INT=0x%" PRIx64 "", intr);

	/* Clear interrupt */
	plt_write64(intr, sso_base + SSO_LF_GGRP_INT);
}

static void
nix_inl_sso_hws_irq(void *param)
{
	struct nix_inl_dev *inl_dev = (struct nix_inl_dev *)param;
	uintptr_t ssow_base = inl_dev->ssow_base;
	uint64_t intr;

	intr = plt_read64(ssow_base + SSOW_LF_GWS_INT);
	if (intr == 0)
		return;

	plt_err("GWS 0 GWS_INT=0x%" PRIx64 "", intr);

	/* Clear interrupt */
	plt_write64(intr, ssow_base + SSOW_LF_GWS_INT);
}

int
nix_inl_sso_register_irqs(struct nix_inl_dev *inl_dev)
{
	struct plt_intr_handle *handle = inl_dev->pci_dev->intr_handle;
	uintptr_t ssow_base = inl_dev->ssow_base;
	uintptr_t sso_base = inl_dev->sso_base;
	uint16_t sso_msixoff, ssow_msixoff;
	int rc;

	ssow_msixoff = inl_dev->ssow_msixoff;
	sso_msixoff = inl_dev->sso_msixoff;
	if (sso_msixoff == MSIX_VECTOR_INVALID ||
	    ssow_msixoff == MSIX_VECTOR_INVALID) {
		plt_err("Invalid SSO/SSOW MSIX offsets (0x%x, 0x%x)",
			sso_msixoff, ssow_msixoff);
		return -EINVAL;
	}

	/*
	 * Setup SSOW interrupt
	 */

	/* Clear SSOW interrupt enable */
	plt_write64(~0ull, ssow_base + SSOW_LF_GWS_INT_ENA_W1C);
	/* Register interrupt with vfio */
	rc = dev_irq_register(handle, nix_inl_sso_hws_irq, inl_dev,
			      ssow_msixoff + SSOW_LF_INT_VEC_IOP);
	/* Set SSOW interrupt enable */
	plt_write64(~0ull, ssow_base + SSOW_LF_GWS_INT_ENA_W1S);

	/*
	 * Setup SSO/HWGRP interrupt
	 */

	/* Clear SSO interrupt enable */
	plt_write64(~0ull, sso_base + SSO_LF_GGRP_INT_ENA_W1C);
	/* Register IRQ */
	rc |= dev_irq_register(handle, nix_inl_sso_hwgrp_irq, (void *)inl_dev,
			       sso_msixoff + SSO_LF_INT_VEC_GRP);
	/* Enable hw interrupt */
	plt_write64(~0ull, sso_base + SSO_LF_GGRP_INT_ENA_W1S);

	/* Setup threshold for work exec interrupt to 100us timeout
	 * based on time counter.
	 */
	plt_write64(BIT_ULL(63) | 10ULL << 48, sso_base + SSO_LF_GGRP_INT_THR);

	return rc;
}

void
nix_inl_sso_unregister_irqs(struct nix_inl_dev *inl_dev)
{
	struct plt_intr_handle *handle = inl_dev->pci_dev->intr_handle;
	uintptr_t ssow_base = inl_dev->ssow_base;
	uintptr_t sso_base = inl_dev->sso_base;
	uint16_t sso_msixoff, ssow_msixoff;

	ssow_msixoff = inl_dev->ssow_msixoff;
	sso_msixoff = inl_dev->sso_msixoff;

	/* Clear SSOW interrupt enable */
	plt_write64(~0ull, ssow_base + SSOW_LF_GWS_INT_ENA_W1C);
	/* Clear SSO/HWGRP interrupt enable */
	plt_write64(~0ull, sso_base + SSO_LF_GGRP_INT_ENA_W1C);
	/* Clear SSO threshold */
	plt_write64(0, sso_base + SSO_LF_GGRP_INT_THR);

	/* Unregister IRQ */
	dev_irq_unregister(handle, nix_inl_sso_hws_irq, (void *)inl_dev,
			   ssow_msixoff + SSOW_LF_INT_VEC_IOP);
	dev_irq_unregister(handle, nix_inl_sso_hwgrp_irq, (void *)inl_dev,
			   sso_msixoff + SSO_LF_INT_VEC_GRP);
}

static void
nix_inl_nix_q_irq(void *param)
{
	struct nix_inl_qint *qints_mem = (struct nix_inl_qint *)param;
	struct nix_inl_dev *inl_dev = qints_mem->inl_dev;
	uintptr_t nix_base = inl_dev->nix_base;
	struct dev *dev = &inl_dev->dev;
	uint16_t qint = qints_mem->qint;
	volatile void *ctx;
	uint64_t reg, intr;
	uint64_t wdata;
	uint8_t irq;
	int rc, q;

	intr = plt_read64(nix_base + NIX_LF_QINTX_INT(qint));
	if (intr == 0)
		return;

	plt_err("Queue_intr=0x%" PRIx64 " qintx 0 pf=%d, vf=%d", intr, dev->pf,
		dev->vf);

	/* Handle RQ interrupts */
	for (q = 0; q < inl_dev->nb_rqs; q++) {
		/* Get and clear RQ interrupts */
		wdata = (uint64_t)q << 44;
		reg = roc_atomic64_add_nosync(wdata,
					      (int64_t *)(nix_base + NIX_LF_RQ_OP_INT));
		if (reg & BIT_ULL(42) /* OP_ERR */) {
			plt_err("Failed to get rq_int");
			return;
		}
		irq = reg & 0xff;
		plt_write64(wdata | irq, nix_base + NIX_LF_RQ_OP_INT);

		if (irq & BIT_ULL(NIX_RQINT_DROP))
			plt_err("RQ=0 NIX_RQINT_DROP");

		if (irq & BIT_ULL(NIX_RQINT_RED))
			plt_err("RQ=0 NIX_RQINT_RED");
	}

	/* Clear interrupt */
	plt_write64(intr, nix_base + NIX_LF_QINTX_INT(qint));

	/* Dump registers to std out */
	nix_inl_nix_reg_dump(inl_dev);

	/* Dump RQs */
	for (q = 0; q < inl_dev->nb_rqs; q++) {
		rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_RQ, q, &ctx);
		if (rc) {
			plt_err("Failed to get rq %d context, rc=%d", q, rc);
			continue;
		}
		nix_lf_rq_dump(ctx, NULL);
	}
}

static void
nix_inl_nix_ras_irq(void *param)
{
	struct nix_inl_dev *inl_dev = (struct nix_inl_dev *)param;
	uintptr_t nix_base = inl_dev->nix_base;
	struct dev *dev = &inl_dev->dev;
	volatile void *ctx;
	uint64_t intr;
	int rc, q;

	intr = plt_read64(nix_base + NIX_LF_RAS);
	if (intr == 0)
		return;

	plt_err("Ras_intr=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);
	/* Clear interrupt */
	plt_write64(intr, nix_base + NIX_LF_RAS);

	/* Dump registers to std out */
	nix_inl_nix_reg_dump(inl_dev);

	/* Dump RQs */
	for (q = 0; q < inl_dev->nb_rqs; q++) {
		rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_RQ, q, &ctx);
		if (rc) {
			plt_err("Failed to get rq %d context, rc=%d", q, rc);
			continue;
		}
		nix_lf_rq_dump(ctx, NULL);
	}
}

static void
nix_inl_nix_err_irq(void *param)
{
	struct nix_inl_dev *inl_dev = (struct nix_inl_dev *)param;
	uintptr_t nix_base = inl_dev->nix_base;
	struct dev *dev = &inl_dev->dev;
	volatile void *ctx;
	uint64_t intr;
	int rc, q;

	intr = plt_read64(nix_base + NIX_LF_ERR_INT);
	if (intr == 0)
		return;

	plt_err("Err_irq=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Clear interrupt */
	plt_write64(intr, nix_base + NIX_LF_ERR_INT);

	/* Dump registers to std out */
	nix_inl_nix_reg_dump(inl_dev);

	/* Dump RQs */
	for (q = 0; q < inl_dev->nb_rqs; q++) {
		rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_RQ, q, &ctx);
		if (rc) {
			plt_err("Failed to get rq %d context, rc=%d", q, rc);
			continue;
		}
		nix_lf_rq_dump(ctx, NULL);
	}
}

int
nix_inl_nix_register_irqs(struct nix_inl_dev *inl_dev)
{
	struct plt_intr_handle *handle = inl_dev->pci_dev->intr_handle;
	uintptr_t nix_base = inl_dev->nix_base;
	struct nix_inl_qint *qints_mem;
	int rc, q, ret = 0;
	uint16_t msixoff;
	int qints;

	msixoff = inl_dev->nix_msixoff;
	if (msixoff == MSIX_VECTOR_INVALID) {
		plt_err("Invalid NIXLF MSIX vector offset: 0x%x", msixoff);
		return -EINVAL;
	}

	/* Disable err interrupts */
	plt_write64(~0ull, nix_base + NIX_LF_ERR_INT_ENA_W1C);
	/* DIsable RAS interrupts */
	plt_write64(~0ull, nix_base + NIX_LF_RAS_ENA_W1C);

	/* Register err irq */
	rc = dev_irq_register(handle, nix_inl_nix_err_irq, inl_dev,
			      msixoff + NIX_LF_INT_VEC_ERR_INT);
	rc |= dev_irq_register(handle, nix_inl_nix_ras_irq, inl_dev,
			       msixoff + NIX_LF_INT_VEC_POISON);

	/* Enable all nix lf error irqs except RQ_DISABLED and CQ_DISABLED */
	plt_write64(~(BIT_ULL(11) | BIT_ULL(24)),
		    nix_base + NIX_LF_ERR_INT_ENA_W1S);
	/* Enable RAS interrupts */
	plt_write64(~0ull, nix_base + NIX_LF_RAS_ENA_W1S);

	/* Setup queue irq for RQ's */
	qints = PLT_MIN(inl_dev->nb_rqs, inl_dev->qints);
	qints_mem = plt_zmalloc(sizeof(struct nix_inl_qint) * qints, 0);
	if (!qints_mem) {
		plt_err("Failed to allocate memory for %u qints", qints);
		return -ENOMEM;
	}

	inl_dev->configured_qints = qints;
	inl_dev->qints_mem = qints_mem;

	for (q = 0; q < qints; q++) {
		/* Clear QINT CNT, interrupt */
		plt_write64(0, nix_base + NIX_LF_QINTX_CNT(q));
		plt_write64(~0ull, nix_base + NIX_LF_QINTX_ENA_W1C(q));

		/* Register queue irq vector */
		ret = dev_irq_register(handle, nix_inl_nix_q_irq, &qints_mem[q],
				       msixoff + NIX_LF_INT_VEC_QINT_START + q);
		if (ret)
			break;

		plt_write64(0, nix_base + NIX_LF_QINTX_CNT(q));
		plt_write64(0, nix_base + NIX_LF_QINTX_INT(q));
		/* Enable QINT interrupt */
		plt_write64(~0ull, nix_base + NIX_LF_QINTX_ENA_W1S(q));

		qints_mem[q].inl_dev = inl_dev;
		qints_mem[q].qint = q;
	}

	rc |= ret;
	return rc;
}

void
nix_inl_nix_unregister_irqs(struct nix_inl_dev *inl_dev)
{
	struct plt_intr_handle *handle = inl_dev->pci_dev->intr_handle;
	struct nix_inl_qint *qints_mem = inl_dev->qints_mem;
	uintptr_t nix_base = inl_dev->nix_base;
	uint16_t msixoff;
	int q;

	msixoff = inl_dev->nix_msixoff;
	/* Disable err interrupts */
	plt_write64(~0ull, nix_base + NIX_LF_ERR_INT_ENA_W1C);
	/* DIsable RAS interrupts */
	plt_write64(~0ull, nix_base + NIX_LF_RAS_ENA_W1C);

	dev_irq_unregister(handle, nix_inl_nix_err_irq, inl_dev,
			   msixoff + NIX_LF_INT_VEC_ERR_INT);
	dev_irq_unregister(handle, nix_inl_nix_ras_irq, inl_dev,
			   msixoff + NIX_LF_INT_VEC_POISON);

	for (q = 0; q < inl_dev->configured_qints; q++) {
		/* Clear QINT CNT */
		plt_write64(0, nix_base + NIX_LF_QINTX_CNT(q));
		plt_write64(0, nix_base + NIX_LF_QINTX_INT(q));

		/* Disable QINT interrupt */
		plt_write64(~0ull, nix_base + NIX_LF_QINTX_ENA_W1C(q));

		/* Unregister queue irq vector */
		dev_irq_unregister(handle, nix_inl_nix_q_irq, &qints_mem[q],
				   msixoff + NIX_LF_INT_VEC_QINT_START + q);
	}

	plt_free(inl_dev->qints_mem);
	inl_dev->qints_mem = NULL;
}
