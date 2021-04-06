/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static void
npa_err_irq(void *param)
{
	struct npa_lf *lf = (struct npa_lf *)param;
	uint64_t intr;

	intr = plt_read64(lf->base + NPA_LF_ERR_INT);
	if (intr == 0)
		return;

	plt_err("Err_intr=0x%" PRIx64 "", intr);

	/* Clear interrupt */
	plt_write64(intr, lf->base + NPA_LF_ERR_INT);
}

static int
npa_register_err_irq(struct npa_lf *lf)
{
	struct plt_intr_handle *handle = lf->intr_handle;
	int rc, vec;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	plt_write64(~0ull, lf->base + NPA_LF_ERR_INT_ENA_W1C);
	/* Register err interrupt vector */
	rc = dev_irq_register(handle, npa_err_irq, lf, vec);

	/* Enable hw interrupt */
	plt_write64(~0ull, lf->base + NPA_LF_ERR_INT_ENA_W1S);

	return rc;
}

static void
npa_unregister_err_irq(struct npa_lf *lf)
{
	struct plt_intr_handle *handle = lf->intr_handle;
	int vec;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	plt_write64(~0ull, lf->base + NPA_LF_ERR_INT_ENA_W1C);
	dev_irq_unregister(handle, npa_err_irq, lf, vec);
}

static void
npa_ras_irq(void *param)
{
	struct npa_lf *lf = (struct npa_lf *)param;
	uint64_t intr;

	intr = plt_read64(lf->base + NPA_LF_RAS);
	if (intr == 0)
		return;

	plt_err("Ras_intr=0x%" PRIx64 "", intr);

	/* Clear interrupt */
	plt_write64(intr, lf->base + NPA_LF_RAS);
}

static int
npa_register_ras_irq(struct npa_lf *lf)
{
	struct plt_intr_handle *handle = lf->intr_handle;
	int rc, vec;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	plt_write64(~0ull, lf->base + NPA_LF_RAS_ENA_W1C);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, npa_ras_irq, lf, vec);
	/* Enable hw interrupt */
	plt_write64(~0ull, lf->base + NPA_LF_RAS_ENA_W1S);

	return rc;
}

static void
npa_unregister_ras_irq(struct npa_lf *lf)
{
	int vec;
	struct plt_intr_handle *handle = lf->intr_handle;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	plt_write64(~0ull, lf->base + NPA_LF_RAS_ENA_W1C);
	dev_irq_unregister(handle, npa_ras_irq, lf, vec);
}

static inline uint8_t
npa_q_irq_get_and_clear(struct npa_lf *lf, uint32_t q, uint32_t off,
			uint64_t mask)
{
	uint64_t reg, wdata;
	uint8_t qint;

	wdata = (uint64_t)q << 44;
	reg = roc_atomic64_add_nosync(wdata, (int64_t *)(lf->base + off));

	if (reg & BIT_ULL(42) /* OP_ERR */) {
		plt_err("Failed execute irq get off=0x%x", off);
		return 0;
	}

	qint = reg & 0xff;
	wdata &= mask;
	plt_write64(wdata | qint, lf->base + off);

	return qint;
}

static inline uint8_t
npa_pool_irq_get_and_clear(struct npa_lf *lf, uint32_t p)
{
	return npa_q_irq_get_and_clear(lf, p, NPA_LF_POOL_OP_INT, ~0xff00);
}

static inline uint8_t
npa_aura_irq_get_and_clear(struct npa_lf *lf, uint32_t a)
{
	return npa_q_irq_get_and_clear(lf, a, NPA_LF_AURA_OP_INT, ~0xff00);
}

static void
npa_q_irq(void *param)
{
	struct npa_qint *qint = (struct npa_qint *)param;
	struct npa_lf *lf = qint->lf;
	uint8_t irq, qintx = qint->qintx;
	uint32_t q, pool, aura;
	uint64_t intr;

	intr = plt_read64(lf->base + NPA_LF_QINTX_INT(qintx));
	if (intr == 0)
		return;

	plt_err("queue_intr=0x%" PRIx64 " qintx=%d", intr, qintx);

	/* Handle pool queue interrupts */
	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip disabled POOL */
		if (plt_bitmap_get(lf->npa_bmp, q))
			continue;

		pool = q % lf->qints;
		irq = npa_pool_irq_get_and_clear(lf, pool);

		if (irq & BIT_ULL(NPA_POOL_ERR_INT_OVFLS))
			plt_err("Pool=%d NPA_POOL_ERR_INT_OVFLS", pool);

		if (irq & BIT_ULL(NPA_POOL_ERR_INT_RANGE))
			plt_err("Pool=%d NPA_POOL_ERR_INT_RANGE", pool);

		if (irq & BIT_ULL(NPA_POOL_ERR_INT_PERR))
			plt_err("Pool=%d NPA_POOL_ERR_INT_PERR", pool);
	}

	/* Handle aura queue interrupts */
	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip disabled AURA */
		if (plt_bitmap_get(lf->npa_bmp, q))
			continue;

		aura = q % lf->qints;
		irq = npa_aura_irq_get_and_clear(lf, aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_AURA_ADD_OVER))
			plt_err("Aura=%d NPA_AURA_ERR_INT_ADD_OVER", aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_AURA_ADD_UNDER))
			plt_err("Aura=%d NPA_AURA_ERR_INT_ADD_UNDER", aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_AURA_FREE_UNDER))
			plt_err("Aura=%d NPA_AURA_ERR_INT_FREE_UNDER", aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_POOL_DIS))
			plt_err("Aura=%d NPA_AURA_ERR_POOL_DIS", aura);
	}

	/* Clear interrupt */
	plt_write64(intr, lf->base + NPA_LF_QINTX_INT(qintx));
	roc_npa_ctx_dump();
}

static int
npa_register_queue_irqs(struct npa_lf *lf)
{
	struct plt_intr_handle *handle = lf->intr_handle;
	int vec, q, qs, rc = 0;

	/* Figure out max qintx required */
	qs = PLT_MIN(lf->qints, lf->nr_pools);

	for (q = 0; q < qs; q++) {
		vec = lf->npa_msixoff + NPA_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		plt_write64(0, lf->base + NPA_LF_QINTX_CNT(q));

		/* Clear interrupt */
		plt_write64(~0ull, lf->base + NPA_LF_QINTX_ENA_W1C(q));

		struct npa_qint *qintmem = lf->npa_qint_mem;

		qintmem += q;

		qintmem->lf = lf;
		qintmem->qintx = q;

		/* Sync qints_mem update */
		plt_wmb();

		/* Register queue irq vector */
		rc = dev_irq_register(handle, npa_q_irq, qintmem, vec);
		if (rc)
			break;

		plt_write64(0, lf->base + NPA_LF_QINTX_CNT(q));
		plt_write64(0, lf->base + NPA_LF_QINTX_INT(q));
		/* Enable QINT interrupt */
		plt_write64(~0ull, lf->base + NPA_LF_QINTX_ENA_W1S(q));
	}

	return rc;
}

static void
npa_unregister_queue_irqs(struct npa_lf *lf)
{
	struct plt_intr_handle *handle = lf->intr_handle;
	int vec, q, qs;

	/* Figure out max qintx required */
	qs = PLT_MIN(lf->qints, lf->nr_pools);

	for (q = 0; q < qs; q++) {
		vec = lf->npa_msixoff + NPA_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		plt_write64(0, lf->base + NPA_LF_QINTX_CNT(q));
		plt_write64(0, lf->base + NPA_LF_QINTX_INT(q));

		/* Clear interrupt */
		plt_write64(~0ull, lf->base + NPA_LF_QINTX_ENA_W1C(q));

		struct npa_qint *qintmem = lf->npa_qint_mem;

		qintmem += q;

		/* Unregister queue irq vector */
		dev_irq_unregister(handle, npa_q_irq, qintmem, vec);

		qintmem->lf = NULL;
		qintmem->qintx = 0;
	}
}

int
npa_register_irqs(struct npa_lf *lf)
{
	int rc;

	if (lf->npa_msixoff == MSIX_VECTOR_INVALID) {
		plt_err("Invalid NPALF MSIX vector offset vector: 0x%x",
			lf->npa_msixoff);
		return NPA_ERR_PARAM;
	}

	/* Register lf err interrupt */
	rc = npa_register_err_irq(lf);
	/* Register RAS interrupt */
	rc |= npa_register_ras_irq(lf);
	/* Register queue interrupts */
	rc |= npa_register_queue_irqs(lf);

	return rc;
}

void
npa_unregister_irqs(struct npa_lf *lf)
{
	npa_unregister_err_irq(lf);
	npa_unregister_ras_irq(lf);
	npa_unregister_queue_irqs(lf);
}
