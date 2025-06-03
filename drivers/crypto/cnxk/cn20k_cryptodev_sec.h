/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef __CN20K_CRYPTODEV_SEC_H__
#define __CN20K_CRYPTODEV_SEC_H__

#include <rte_common.h>
#include <rte_security.h>

#include "roc_constants.h"
#include "roc_cpt.h"

#include "cn20k_ipsec.h"
#include "cn20k_tls.h"

#define SEC_SESS_SIZE sizeof(struct rte_security_session)

struct cn20k_tls_opt {
	uint16_t pad_shift : 3;
	uint16_t enable_padding : 1;
	uint16_t tail_fetch_len : 2;
	uint16_t tls_ver : 2;
	uint16_t is_write : 1;
	uint16_t mac_len : 7;
};

void cn20k_sec_ops_override(void);

struct __rte_aligned(ROC_ALIGN) cn20k_sec_session {
	uint8_t rte_sess[SEC_SESS_SIZE];

	/** PMD private space */
	alignas(RTE_CACHE_LINE_MIN_SIZE)

	/** Pre-populated CPT inst words */
	struct cnxk_cpt_inst_tmpl inst;
	uint16_t max_extended_len;
	uint16_t iv_offset;
	uint8_t proto;
	uint8_t iv_length;
	union {
		uint16_t u16;
		struct cn20k_tls_opt tls_opt;
		struct {
			uint8_t ip_csum;
			uint8_t is_outbound : 1;
		} ipsec;
	};
	/** Queue pair */
	struct cnxk_cpt_qp *qp;
	/** Userdata to be set for Rx inject */
	void *userdata;

	/**
	 * End of SW mutable area
	 */
	union {
		struct cn20k_ipsec_sa sa;
		struct cn20k_tls_record tls_rec;
	};
};

#endif /* __CN20K_CRYPTODEV_SEC_H__ */
