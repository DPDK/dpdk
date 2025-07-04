/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_IPSEC_H__
#define __CN9K_IPSEC_H__

#include <rte_security_driver.h>

#include "cnxk_ipsec.h"
#include "cnxk_security.h"
#include "cnxk_security_ar.h"

struct __rte_aligned(8) cn9k_ipsec_sa {
	union {
		/** Inbound SA */
		struct roc_ie_on_inb_sa in_sa;
		/** Outbound SA */
		struct roc_ie_on_outb_sa out_sa;
	};
};

#define SEC_SESS_SIZE sizeof(struct rte_security_session)

struct __rte_cache_aligned cn9k_sec_session {
	uint8_t rte_sess[SEC_SESS_SIZE];

	/** PMD private space */

	/** ESN */
	union {
		uint64_t esn;
		struct {
			uint32_t seq_lo;
			uint32_t seq_hi;
		};
	};
	/** IPsec SA direction */
	uint8_t is_outbound;
	/* ESN enable flag */
	uint8_t esn_en;
	/** Pre-populated CPT inst words */
	struct cnxk_cpt_inst_tmpl inst;
	/** Response length calculation data */
	struct cnxk_ipsec_outb_rlens rlens;
	/** Anti replay window size */
	uint32_t replay_win_sz;
	/** Cipher IV offset in bytes */
	uint16_t cipher_iv_off;
	/** Cipher IV length in bytes */
	uint8_t cipher_iv_len;
	/** Outbound custom header length */
	uint8_t custom_hdr_len;
	/** Anti replay */
	struct cnxk_on_ipsec_ar ar;
	/** Queue pair */
	struct cnxk_cpt_qp *qp;

	struct cn9k_ipsec_sa sa;
};

void cn9k_sec_ops_override(void);

#endif /* __CN9K_IPSEC_H__ */
