/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_IPSEC_PO_H__
#define __OTX2_IPSEC_PO_H__

#include <rte_crypto_sym.h>
#include <rte_ip.h>
#include <rte_security.h>

union otx2_ipsec_po_bit_perfect_iv {
	uint8_t aes_iv[16];
	uint8_t des_iv[8];
	struct {
		uint8_t nonce[4];
		uint8_t iv[8];
		uint8_t counter[4];
	} gcm;
};

struct otx2_ipsec_po_traffic_selector {
	rte_be16_t src_port[2];
	rte_be16_t dst_port[2];
	RTE_STD_C11
	union {
		struct {
			rte_be32_t src_addr[2];
			rte_be32_t dst_addr[2];
		} ipv4;
		struct {
			uint8_t src_addr[32];
			uint8_t dst_addr[32];
		} ipv6;
	};
};

struct otx2_ipsec_po_sa_ctl {
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

struct otx2_ipsec_po_in_sa {
	/* w0 */
	struct otx2_ipsec_po_sa_ctl ctl;

	/* w1-w4 */
	uint8_t cipher_key[32];

	/* w5-w6 */
	union otx2_ipsec_po_bit_perfect_iv iv;

	/* w7 */
	uint32_t esn_hi;
	uint32_t esn_low;

	/* w8 */
	uint8_t udp_encap[8];

	/* w9-w23 */
	struct {
		uint8_t hmac_key[48];
		struct otx2_ipsec_po_traffic_selector selector;
	} aes_gcm;
};

struct otx2_ipsec_po_ip_template {
	RTE_STD_C11
	union {
		uint8_t raw[252];
		struct rte_ipv4_hdr ipv4_hdr;
		struct rte_ipv6_hdr ipv6_hdr;
	};
};

struct otx2_ipsec_po_out_sa {
	/* w0 */
	struct otx2_ipsec_po_sa_ctl ctl;

	/* w1-w4 */
	uint8_t cipher_key[32];

	/* w5-w6 */
	union otx2_ipsec_po_bit_perfect_iv iv;

	/* w7 */
	uint32_t esn_hi;
	uint32_t esn_low;

	/* w8-w39 */
	struct otx2_ipsec_po_ip_template template;
	uint16_t udp_src;
	uint16_t udp_dst;
};

#endif /* __OTX2_IPSEC_PO_H__ */
