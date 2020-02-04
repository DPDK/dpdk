/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_IPSEC_FP_H__
#define __OTX2_IPSEC_FP_H__

struct otx2_ipsec_fp_sa_ctl {
	rte_be32_t spi          : 32;
	uint64_t exp_proto_inter_frag : 8;
	uint64_t rsvd_42_40   : 3;
	uint64_t esn_en       : 1;
	uint64_t rsvd_45_44   : 2;
	uint64_t encap_type   : 2;
	uint64_t enc_type     : 3;
	uint64_t rsvd_48      : 1;
	uint64_t auth_type    : 4;
	uint64_t valid        : 1;
	uint64_t direction    : 1;
	uint64_t outer_ip_ver : 1;
	uint64_t inner_ip_ver : 1;
	uint64_t ipsec_mode   : 1;
	uint64_t ipsec_proto  : 1;
	uint64_t aes_key_len  : 2;
};

struct otx2_ipsec_fp_in_sa {
	/* w0 */
	struct otx2_ipsec_fp_sa_ctl ctl;

	/* w1 */
	uint8_t nonce[4]; /* Only for AES-GCM */
	uint32_t unused;

	/* w2 */
	uint32_t esn_low;
	uint32_t esn_hi;

	/* w3-w6 */
	uint8_t cipher_key[32];

	/* w7-w12 */
	uint8_t hmac_key[48];

	RTE_STD_C11
	union {
		void *userdata;
		uint64_t udata64;
	};

	uint64_t reserved1;
	uint64_t reserved2;
};

#endif /* __OTX2_IPSEC_FP_H__ */
