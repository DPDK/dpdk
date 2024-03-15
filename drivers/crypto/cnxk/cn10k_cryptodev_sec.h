/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN10K_CRYPTODEV_SEC_H__
#define __CN10K_CRYPTODEV_SEC_H__

#include <rte_security.h>

#include "roc_constants.h"
#include "roc_cpt.h"

#include "cn10k_ipsec.h"
#include "cn10k_tls.h"

#define SEC_SESS_SIZE sizeof(struct rte_security_session)

struct cn10k_sec_session {
	uint8_t rte_sess[SEC_SESS_SIZE];

	/** PMD private space */

	enum rte_security_session_protocol proto;
	/** Pre-populated CPT inst words */
	struct cnxk_cpt_inst_tmpl inst;
	uint16_t max_extended_len;
	uint16_t iv_offset;
	uint8_t iv_length;
	union {
		struct {
			uint8_t ip_csum;
			bool is_outbound;
		} ipsec;
		struct {
			uint8_t enable_padding : 1;
			uint8_t tail_fetch_len : 2;
			uint8_t rvsd : 5;
			bool is_write;
		} tls;
	};
	/** Queue pair */
	struct cnxk_cpt_qp *qp;
	/** Userdata to be set for Rx inject */
	void *userdata;

	/**
	 * End of SW mutable area
	 */
	union {
		struct cn10k_ipsec_sa sa;
		struct cn10k_tls_record tls_rec;
	};
} __rte_aligned(ROC_ALIGN);

static inline uint64_t
cpt_inst_w7_get(struct roc_cpt *roc_cpt, void *cptr)
{
	union cpt_inst_w7 w7;

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];
	w7.s.ctx_val = 1;
	w7.s.cptr = (uint64_t)cptr;
	rte_mb();

	return w7.u64;
}

void cn10k_sec_ops_override(void);

#endif /* __CN10K_CRYPTODEV_SEC_H__ */
