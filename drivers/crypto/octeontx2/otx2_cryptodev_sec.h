/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_CRYPTODEV_SEC_H__
#define __OTX2_CRYPTODEV_SEC_H__

#include <rte_cryptodev.h>

#include "otx2_ipsec_po.h"

struct otx2_sec_session_ipsec_lp {
	RTE_STD_C11
	union {
		/* Inbound SA */
		struct otx2_ipsec_po_in_sa in_sa;
		/* Outbound SA */
		struct otx2_ipsec_po_out_sa out_sa;
	};

	uint64_t cpt_inst_w7;
	union {
		uint64_t ucmd_w0;
		struct {
			uint16_t ucmd_dlen;
			uint16_t ucmd_param2;
			uint16_t ucmd_param1;
			uint16_t ucmd_opcode;
		};
	};

	uint8_t partial_len;
	uint8_t roundup_len;
	uint8_t roundup_byte;
	uint16_t ip_id;
	union {
		uint64_t esn;
		struct {
			uint32_t seq_lo;
			uint32_t seq_hi;
		};
	};

	/** Context length in 8-byte words */
	size_t ctx_len;
	/** Auth IV offset in bytes */
	uint16_t auth_iv_offset;
	/** IV offset in bytes */
	uint16_t iv_offset;
	/** AAD length */
	uint16_t aad_length;
	/** MAC len in bytes */
	uint8_t mac_len;
	/** IV length in bytes */
	uint8_t iv_length;
	/** Auth IV length in bytes */
	uint8_t auth_iv_length;
	/** IPsec tunnel type */
	enum rte_security_ipsec_tunnel_type tunnel_type;
};

int otx2_crypto_sec_ctx_create(struct rte_cryptodev *crypto_dev);

void otx2_crypto_sec_ctx_destroy(struct rte_cryptodev *crypto_dev);

#endif /* __OTX2_CRYPTODEV_SEC_H__ */
