/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static void
sso_hws_dump(uintptr_t base, FILE *f)
{
	fprintf(f, "SSOW_LF_GWS Base addr   0x%" PRIx64 "\n", (uint64_t)base);
	fprintf(f, "SSOW_LF_GWS_LINKS       0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_LINKS));
	fprintf(f, "SSOW_LF_GWS_PENDWQP     0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_PENDWQP));
	fprintf(f, "SSOW_LF_GWS_PENDSTATE   0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_PENDSTATE));
	fprintf(f, "SSOW_LF_GWS_NW_TIM      0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_NW_TIM));
	fprintf(f, "SSOW_LF_GWS_TAG         0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_TAG));
	fprintf(f, "SSOW_LF_GWS_WQP         0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_TAG));
	fprintf(f, "SSOW_LF_GWS_SWTP        0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_SWTP));
	fprintf(f, "SSOW_LF_GWS_PENDTAG     0x%" PRIx64 "\n",
		plt_read64(base + SSOW_LF_GWS_PENDTAG));
}

static void
sso_hwgrp_dump(uintptr_t base, FILE *f)
{
	fprintf(f, "SSO_LF_GGRP Base addr   0x%" PRIx64 "\n", (uint64_t)base);
	fprintf(f, "SSO_LF_GGRP_QCTL        0x%" PRIx64 "\n",
		plt_read64(base + SSO_LF_GGRP_QCTL));
	fprintf(f, "SSO_LF_GGRP_XAQ_CNT     0x%" PRIx64 "\n",
		plt_read64(base + SSO_LF_GGRP_XAQ_CNT));
	fprintf(f, "SSO_LF_GGRP_INT_THR     0x%" PRIx64 "\n",
		plt_read64(base + SSO_LF_GGRP_INT_THR));
	fprintf(f, "SSO_LF_GGRP_INT_CNT     0x%" PRIX64 "\n",
		plt_read64(base + SSO_LF_GGRP_INT_CNT));
	fprintf(f, "SSO_LF_GGRP_AQ_CNT      0x%" PRIX64 "\n",
		plt_read64(base + SSO_LF_GGRP_AQ_CNT));
	fprintf(f, "SSO_LF_GGRP_AQ_THR      0x%" PRIX64 "\n",
		plt_read64(base + SSO_LF_GGRP_AQ_THR));
	fprintf(f, "SSO_LF_GGRP_MISC_CNT    0x%" PRIx64 "\n",
		plt_read64(base + SSO_LF_GGRP_MISC_CNT));
}

void
roc_sso_dump(struct roc_sso *roc_sso, uint8_t nb_hws, uint16_t hwgrp, FILE *f)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	uintptr_t base;
	int i;

	/* Dump SSOW registers */
	for (i = 0; i < nb_hws; i++) {
		base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | i << 12);
		sso_hws_dump(base, f);
	}

	/* Dump SSO registers */
	for (i = 0; i < hwgrp; i++) {
		base = dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 | i << 12);
		sso_hwgrp_dump(base, f);
	}
}
