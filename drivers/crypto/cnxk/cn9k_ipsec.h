/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN9K_IPSEC_H__
#define __CN9K_IPSEC_H__

#include "cnxk_ipsec.h"
#include "cnxk_security.h"
#include "cnxk_security_ar.h"

struct cn9k_ipsec_sa {
	union {
		/** Inbound SA */
		struct roc_ie_on_inb_sa in_sa;
		/** Outbound SA */
		struct roc_ie_on_outb_sa out_sa;
	};
	/** IPsec SA direction */
	enum rte_security_ipsec_sa_direction dir;
	/** Pre-populated CPT inst words */
	struct cnxk_cpt_inst_tmpl inst;
	/** Cipher IV offset in bytes */
	uint16_t cipher_iv_off;
	/** Cipher IV length in bytes */
	uint8_t cipher_iv_len;
	/** Response length calculation data */
	struct cnxk_ipsec_outb_rlens rlens;
	/** Outbound IP-ID */
	uint16_t ip_id;
	/** ESN */
	union {
		uint64_t esn;
		struct {
			uint32_t seq_lo;
			uint32_t seq_hi;
		};
	};
	/** Anti replay */
	struct cnxk_on_ipsec_ar ar;
	/** Anti replay window size */
	uint32_t replay_win_sz;
};

struct cn9k_sec_session {
	struct cn9k_ipsec_sa sa;
} __rte_cache_aligned;

void cn9k_sec_ops_override(void);

#endif /* __CN9K_IPSEC_H__ */
