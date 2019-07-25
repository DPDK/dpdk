/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_common.h>
#include <rte_bus_pci.h>

#include "otx2_common.h"
#include "otx2_irq.h"
#include "otx2_mempool.h"

static void
npa_lf_err_irq(void *param)
{
	struct otx2_npa_lf *lf = (struct otx2_npa_lf *)param;
	uint64_t intr;

	intr = otx2_read64(lf->base + NPA_LF_ERR_INT);
	if (intr == 0)
		return;

	otx2_err("Err_intr=0x%" PRIx64 "", intr);

	/* Clear interrupt */
	otx2_write64(intr, lf->base + NPA_LF_ERR_INT);
}

static int
npa_lf_register_err_irq(struct otx2_npa_lf *lf)
{
	struct rte_intr_handle *handle = lf->intr_handle;
	int rc, vec;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	otx2_write64(~0ull, lf->base + NPA_LF_ERR_INT_ENA_W1C);
	/* Register err interrupt vector */
	rc = otx2_register_irq(handle, npa_lf_err_irq, lf, vec);

	/* Enable hw interrupt */
	otx2_write64(~0ull, lf->base + NPA_LF_ERR_INT_ENA_W1S);

	return rc;
}

static void
npa_lf_unregister_err_irq(struct otx2_npa_lf *lf)
{
	struct rte_intr_handle *handle = lf->intr_handle;
	int vec;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	otx2_write64(~0ull, lf->base + NPA_LF_ERR_INT_ENA_W1C);
	otx2_unregister_irq(handle, npa_lf_err_irq, lf, vec);
}

static void
npa_lf_ras_irq(void *param)
{
	struct otx2_npa_lf *lf = (struct otx2_npa_lf *)param;
	uint64_t intr;

	intr = otx2_read64(lf->base + NPA_LF_RAS);
	if (intr == 0)
		return;

	otx2_err("Ras_intr=0x%" PRIx64 "", intr);

	/* Clear interrupt */
	otx2_write64(intr, lf->base + NPA_LF_RAS);
}

static int
npa_lf_register_ras_irq(struct otx2_npa_lf *lf)
{
	struct rte_intr_handle *handle = lf->intr_handle;
	int rc, vec;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	otx2_write64(~0ull, lf->base + NPA_LF_RAS_ENA_W1C);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, npa_lf_ras_irq, lf, vec);
	/* Enable hw interrupt */
	otx2_write64(~0ull, lf->base + NPA_LF_RAS_ENA_W1S);

	return rc;
}

static void
npa_lf_unregister_ras_irq(struct otx2_npa_lf *lf)
{
	int vec;
	struct rte_intr_handle *handle = lf->intr_handle;

	vec = lf->npa_msixoff + NPA_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	otx2_write64(~0ull, lf->base + NPA_LF_RAS_ENA_W1C);
	otx2_unregister_irq(handle, npa_lf_ras_irq, lf, vec);
}

static inline uint8_t
npa_lf_q_irq_get_and_clear(struct otx2_npa_lf *lf, uint32_t q,
			uint32_t off, uint64_t mask)
{
	uint64_t reg, wdata;
	uint8_t qint;

	wdata = (uint64_t)q << 44;
	reg = otx2_atomic64_add_nosync(wdata, (int64_t *)(lf->base + off));

	if (reg & BIT_ULL(42) /* OP_ERR */) {
		otx2_err("Failed execute irq get off=0x%x", off);
		return 0;
	}

	qint = reg & 0xff;
	wdata &= mask;
	otx2_write64(wdata | qint, lf->base + off);

	return qint;
}

static inline uint8_t
npa_lf_pool_irq_get_and_clear(struct otx2_npa_lf *lf, uint32_t p)
{
	return npa_lf_q_irq_get_and_clear(lf, p, NPA_LF_POOL_OP_INT, ~0xff00);
}

static inline uint8_t
npa_lf_aura_irq_get_and_clear(struct otx2_npa_lf *lf, uint32_t a)
{
	return npa_lf_q_irq_get_and_clear(lf, a, NPA_LF_AURA_OP_INT, ~0xff00);
}

static void
npa_lf_q_irq(void *param)
{
	struct otx2_npa_qint *qint = (struct otx2_npa_qint *)param;
	struct otx2_npa_lf *lf = qint->lf;
	uint8_t irq, qintx = qint->qintx;
	uint32_t q, pool, aura;
	uint64_t intr;

	intr = otx2_read64(lf->base + NPA_LF_QINTX_INT(qintx));
	if (intr == 0)
		return;

	otx2_err("queue_intr=0x%" PRIx64 " qintx=%d", intr, qintx);

	/* Handle pool queue interrupts */
	for (q = 0; q < lf->nr_pools; q++) {
		/* Skip disabled POOL */
		if (rte_bitmap_get(lf->npa_bmp, q))
			continue;

		pool = q % lf->qints;
		irq = npa_lf_pool_irq_get_and_clear(lf, pool);

		if (irq & BIT_ULL(NPA_POOL_ERR_INT_OVFLS))
			otx2_err("Pool=%d NPA_POOL_ERR_INT_OVFLS", pool);

		if (irq & BIT_ULL(NPA_POOL_ERR_INT_RANGE))
			otx2_err("Pool=%d NPA_POOL_ERR_INT_RANGE", pool);

		if (irq & BIT_ULL(NPA_POOL_ERR_INT_PERR))
			otx2_err("Pool=%d NPA_POOL_ERR_INT_PERR", pool);
	}

	/* Handle aura queue interrupts */
	for (q = 0; q < lf->nr_pools; q++) {

		/* Skip disabled AURA */
		if (rte_bitmap_get(lf->npa_bmp, q))
			continue;

		aura = q % lf->qints;
		irq = npa_lf_aura_irq_get_and_clear(lf, aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_AURA_ADD_OVER))
			otx2_err("Aura=%d NPA_AURA_ERR_INT_ADD_OVER", aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_AURA_ADD_UNDER))
			otx2_err("Aura=%d NPA_AURA_ERR_INT_ADD_UNDER", aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_AURA_FREE_UNDER))
			otx2_err("Aura=%d NPA_AURA_ERR_INT_FREE_UNDER", aura);

		if (irq & BIT_ULL(NPA_AURA_ERR_INT_POOL_DIS))
			otx2_err("Aura=%d NPA_AURA_ERR_POOL_DIS", aura);
	}

	/* Clear interrupt */
	otx2_write64(intr, lf->base + NPA_LF_QINTX_INT(qintx));
	otx2_mempool_ctx_dump(lf);
}

static int
npa_lf_register_queue_irqs(struct otx2_npa_lf *lf)
{
	struct rte_intr_handle *handle = lf->intr_handle;
	int vec, q, qs, rc = 0;

	/* Figure out max qintx required */
	qs = RTE_MIN(lf->qints, lf->nr_pools);

	for (q = 0; q < qs; q++) {
		vec = lf->npa_msixoff + NPA_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		otx2_write64(0, lf->base + NPA_LF_QINTX_CNT(q));

		/* Clear interrupt */
		otx2_write64(~0ull, lf->base + NPA_LF_QINTX_ENA_W1C(q));

		struct otx2_npa_qint *qintmem = lf->npa_qint_mem;
		qintmem += q;

		qintmem->lf = lf;
		qintmem->qintx = q;

		/* Sync qints_mem update */
		rte_smp_wmb();

		/* Register queue irq vector */
		rc = otx2_register_irq(handle, npa_lf_q_irq, qintmem, vec);
		if (rc)
			break;

		otx2_write64(0, lf->base + NPA_LF_QINTX_CNT(q));
		otx2_write64(0, lf->base + NPA_LF_QINTX_INT(q));
		/* Enable QINT interrupt */
		otx2_write64(~0ull, lf->base + NPA_LF_QINTX_ENA_W1S(q));
	}

	return rc;
}

static void
npa_lf_unregister_queue_irqs(struct otx2_npa_lf *lf)
{
	struct rte_intr_handle *handle = lf->intr_handle;
	int vec, q, qs;

	/* Figure out max qintx required */
	qs = RTE_MIN(lf->qints, lf->nr_pools);

	for (q = 0; q < qs; q++) {
		vec = lf->npa_msixoff + NPA_LF_INT_VEC_QINT_START + q;

		/* Clear QINT CNT */
		otx2_write64(0, lf->base + NPA_LF_QINTX_CNT(q));
		otx2_write64(0, lf->base + NPA_LF_QINTX_INT(q));

		/* Clear interrupt */
		otx2_write64(~0ull, lf->base + NPA_LF_QINTX_ENA_W1C(q));

		struct otx2_npa_qint *qintmem = lf->npa_qint_mem;
		qintmem += q;

		/* Unregister queue irq vector */
		otx2_unregister_irq(handle, npa_lf_q_irq, qintmem, vec);

		qintmem->lf = NULL;
		qintmem->qintx = 0;
	}
}

int
otx2_npa_register_irqs(struct otx2_npa_lf *lf)
{
	int rc;

	if (lf->npa_msixoff == MSIX_VECTOR_INVALID) {
		otx2_err("Invalid NPALF MSIX vector offset vector: 0x%x",
			lf->npa_msixoff);
		return -EINVAL;
	}

	/* Register lf err interrupt */
	rc = npa_lf_register_err_irq(lf);
	/* Register RAS interrupt */
	rc |= npa_lf_register_ras_irq(lf);
	/* Register queue interrupts */
	rc |= npa_lf_register_queue_irqs(lf);

	return rc;
}

void
otx2_npa_unregister_irqs(struct otx2_npa_lf *lf)
{
	npa_lf_unregister_err_irq(lf);
	npa_lf_unregister_ras_irq(lf);
	npa_lf_unregister_queue_irqs(lf);
}
