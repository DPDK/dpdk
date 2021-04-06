/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static void
tim_lf_irq(void *param)
{
	uintptr_t base = (uintptr_t)param;
	uint64_t intr;
	uint8_t ring;

	ring = (base >> 12) & 0xFF;

	intr = plt_read64(base + TIM_LF_NRSPERR_INT);
	plt_err("TIM RING %d TIM_LF_NRSPERR_INT=0x%" PRIx64 "", ring, intr);
	intr = plt_read64(base + TIM_LF_RAS_INT);
	plt_err("TIM RING %d TIM_LF_RAS_INT=0x%" PRIx64 "", ring, intr);

	/* Clear interrupt */
	plt_write64(intr, base + TIM_LF_NRSPERR_INT);
	plt_write64(intr, base + TIM_LF_RAS_INT);
}

static int
tim_lf_register_irq(uintptr_t base, struct plt_intr_handle *handle,
		    uint16_t msix_offset)
{
	unsigned int vec;
	int rc;

	vec = msix_offset + TIM_LF_INT_VEC_NRSPERR_INT;

	/* Clear err interrupt */
	plt_write64(~0ull, base + TIM_LF_NRSPERR_INT);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, tim_lf_irq, (void *)base, vec);
	/* Enable hw interrupt */
	plt_write64(~0ull, base + TIM_LF_NRSPERR_INT_ENA_W1S);

	vec = msix_offset + TIM_LF_INT_VEC_RAS_INT;

	/* Clear err interrupt */
	plt_write64(~0ull, base + TIM_LF_RAS_INT);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, tim_lf_irq, (void *)base, vec);
	/* Enable hw interrupt */
	plt_write64(~0ull, base + TIM_LF_RAS_INT_ENA_W1S);

	return rc;
}

int
tim_register_irq_priv(struct roc_tim *roc_tim, struct plt_intr_handle *handle,
		      uint8_t ring_id, uint16_t msix_offset)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	uintptr_t base;

	if (msix_offset == MSIX_VECTOR_INVALID) {
		plt_err("Invalid MSIX offset for TIM LF %d", ring_id);
		return TIM_ERR_PARAM;
	}

	base = dev->bar2 + (RVU_BLOCK_ADDR_TIM << 20 | ring_id << 12);
	return tim_lf_register_irq(base, handle, msix_offset);
}

static void
tim_lf_unregister_irq(uintptr_t base, struct plt_intr_handle *handle,
		      uint16_t msix_offset)
{
	unsigned int vec;

	vec = msix_offset + TIM_LF_INT_VEC_NRSPERR_INT;

	/* Clear err interrupt */
	plt_write64(~0ull, base + TIM_LF_NRSPERR_INT_ENA_W1C);
	dev_irq_unregister(handle, tim_lf_irq, (void *)base, vec);

	vec = msix_offset + TIM_LF_INT_VEC_RAS_INT;

	/* Clear err interrupt */
	plt_write64(~0ull, base + TIM_LF_RAS_INT_ENA_W1C);
	dev_irq_unregister(handle, tim_lf_irq, (void *)base, vec);
}

void
tim_unregister_irq_priv(struct roc_tim *roc_tim, struct plt_intr_handle *handle,
			uint8_t ring_id, uint16_t msix_offset)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_tim->roc_sso)->dev;
	uintptr_t base;

	if (msix_offset == MSIX_VECTOR_INVALID) {
		plt_err("Invalid MSIX offset for TIM LF %d", ring_id);
		return;
	}

	base = dev->bar2 + (RVU_BLOCK_ADDR_TIM << 20 | ring_id << 12);
	tim_lf_unregister_irq(base, handle, msix_offset);
}
