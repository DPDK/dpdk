/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN10K_IPSEC_H__
#define __CN10K_IPSEC_H__

#include <rte_security.h>

#include "roc_api.h"

#include "cnxk_ipsec.h"

typedef void *CN10K_SA_CONTEXT_MARKER[0];

struct cn10k_ipsec_sa {
	/** Pre-populated CPT inst words */
	struct cnxk_cpt_inst_tmpl inst;
	uint16_t max_extended_len;
	uint16_t iv_offset;
	uint8_t iv_length;
	bool is_outbound;

	/**
	 * End of SW mutable area
	 */
	CN10K_SA_CONTEXT_MARKER sw_area_end __rte_aligned(ROC_ALIGN);

	union {
		/** Inbound SA */
		struct roc_ot_ipsec_inb_sa in_sa;
		/** Outbound SA */
		struct roc_ot_ipsec_outb_sa out_sa;
	};
} __rte_aligned(ROC_ALIGN);

struct cn10k_sec_session {
	struct cn10k_ipsec_sa sa;
} __rte_aligned(ROC_ALIGN);

void cn10k_sec_ops_override(void);

#endif /* __CN10K_IPSEC_H__ */
