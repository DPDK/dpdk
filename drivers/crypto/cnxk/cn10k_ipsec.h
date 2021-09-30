/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN10K_IPSEC_H__
#define __CN10K_IPSEC_H__

#include <rte_security.h>

#include "cnxk_ipsec.h"

#define CN10K_IPSEC_SA_CTX_HDR_SIZE 1

struct cn10k_ipsec_sa {
	union {
		/** Inbound SA */
		struct roc_ot_ipsec_inb_sa in_sa;
		/** Outbound SA */
		struct roc_ot_ipsec_outb_sa out_sa;
	};
	/** Pre-populated CPT inst words */
	struct cnxk_cpt_inst_tmpl inst;
	uint16_t max_extended_len;
	uint16_t iv_offset;
	uint8_t iv_length;
	bool ip_csum_enable;
};

struct cn10k_sec_session {
	struct cn10k_ipsec_sa sa;
} __rte_cache_aligned;

void cn10k_sec_ops_override(void);

#endif /* __CN10K_IPSEC_H__ */
