/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static void
sso_hwgrp_irq(void *param)
{
	struct sso_rsrc *rsrc = param;
	uint64_t intr;

	intr = plt_read64(rsrc->base + SSO_LF_GGRP_INT);
	if (intr == 0)
		return;

	plt_err("GGRP %d GGRP_INT=0x%" PRIx64 "", rsrc->rsrc_id, intr);

	/* Clear interrupt */
	plt_write64(intr, rsrc->base + SSO_LF_GGRP_INT);
}

static int
sso_hwgrp_register_irq(struct plt_intr_handle *handle, uint16_t ggrp_msixoff,
		       struct sso_rsrc *rsrc)
{
	int rc, vec;

	vec = ggrp_msixoff + SSO_LF_INT_VEC_GRP;

	/* Clear err interrupt */
	plt_write64(~0ull, rsrc->base + SSO_LF_GGRP_INT_ENA_W1C);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, sso_hwgrp_irq, (void *)rsrc, vec);
	/* Enable hw interrupt */
	plt_write64(~0ull, rsrc->base + SSO_LF_GGRP_INT_ENA_W1S);

	return rc;
}

static void
sso_hws_irq(void *param)
{
	struct sso_rsrc *rsrc = param;
	uint64_t intr;

	intr = plt_read64(rsrc->base + SSOW_LF_GWS_INT);
	if (intr == 0)
		return;

	plt_err("GWS %d GWS_INT=0x%" PRIx64 "", rsrc->rsrc_id, intr);

	/* Clear interrupt */
	plt_write64(intr, rsrc->base + SSOW_LF_GWS_INT);
}

static int
sso_hws_register_irq(struct plt_intr_handle *handle, uint16_t hws_msixoff,
		     struct sso_rsrc *rsrc)
{
	int rc, vec;

	vec = hws_msixoff + SSOW_LF_INT_VEC_IOP;

	/* Clear err interrupt */
	plt_write64(~0ull, rsrc->base + SSOW_LF_GWS_INT_ENA_W1C);
	/* Set used interrupt vectors */
	rc = dev_irq_register(handle, sso_hws_irq, (void *)rsrc, vec);
	/* Enable hw interrupt */
	plt_write64(~0ull, rsrc->base + SSOW_LF_GWS_INT_ENA_W1S);

	return rc;
}

int
sso_register_irqs_priv(struct roc_sso *roc_sso, struct plt_intr_handle *handle,
		       uint16_t nb_hws, uint16_t nb_hwgrp)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	struct dev *dev = &sso->dev;
	int i, rc = SSO_ERR_PARAM;

	for (i = 0; i < nb_hws; i++) {
		if (sso->hws_msix_offset[i] == MSIX_VECTOR_INVALID) {
			plt_err("Invalid SSO HWS MSIX offset[%d] vector 0x%x",
				i, sso->hws_msix_offset[i]);
			goto fail;
		}
	}

	for (i = 0; i < nb_hwgrp; i++) {
		if (sso->hwgrp_msix_offset[i] == MSIX_VECTOR_INVALID) {
			plt_err("Invalid SSO HWGRP MSIX offset[%d] vector 0x%x",
				i, sso->hwgrp_msix_offset[i]);
			goto fail;
		}
	}

	for (i = 0; i < nb_hws; i++) {
		uintptr_t base =
			dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | i << 12);

		sso->hws_rsrc[i].rsrc_id = i;
		sso->hws_rsrc[i].base = base;
		rc = sso_hws_register_irq(handle, sso->hws_msix_offset[i],
					  &sso->hws_rsrc[i]);
	}

	for (i = 0; i < nb_hwgrp; i++) {
		uintptr_t base =
			dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 | i << 12);

		sso->hwgrp_rsrc[i].rsrc_id = i;
		sso->hwgrp_rsrc[i].base = base;
		rc = sso_hwgrp_register_irq(handle, sso->hwgrp_msix_offset[i],
					    &sso->hwgrp_rsrc[i]);
	}
fail:
	return rc;
}

static void
sso_hwgrp_unregister_irq(struct plt_intr_handle *handle, uint16_t ggrp_msixoff,
			 struct sso_rsrc *rsrc)
{
	int vec;

	vec = ggrp_msixoff + SSO_LF_INT_VEC_GRP;

	/* Clear err interrupt */
	plt_write64(~0ull, rsrc->base + SSO_LF_GGRP_INT_ENA_W1C);
	dev_irq_unregister(handle, sso_hwgrp_irq, (void *)rsrc, vec);
}

static void
sso_hws_unregister_irq(struct plt_intr_handle *handle, uint16_t gws_msixoff,
		       struct sso_rsrc *rsrc)
{
	int vec;

	vec = gws_msixoff + SSOW_LF_INT_VEC_IOP;

	/* Clear err interrupt */
	plt_write64(~0ull, rsrc->base + SSOW_LF_GWS_INT_ENA_W1C);
	dev_irq_unregister(handle, sso_hws_irq, (void *)rsrc, vec);
}

void
sso_unregister_irqs_priv(struct roc_sso *roc_sso,
			 struct plt_intr_handle *handle, uint16_t nb_hws,
			 uint16_t nb_hwgrp)
{
	struct sso *sso = roc_sso_to_sso_priv(roc_sso);
	int i;

	for (i = 0; i < nb_hwgrp; i++)
		sso_hwgrp_unregister_irq(handle, sso->hwgrp_msix_offset[i],
					 &sso->hwgrp_rsrc[i]);

	for (i = 0; i < nb_hws; i++)
		sso_hws_unregister_irq(handle, sso->hws_msix_offset[i],
				       &sso->hws_rsrc[i]);
}
