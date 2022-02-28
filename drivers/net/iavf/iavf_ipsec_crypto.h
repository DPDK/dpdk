/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _IAVF_IPSEC_CRYPTO_H_
#define _IAVF_IPSEC_CRYPTO_H_

#include <rte_security.h>

#include "iavf.h"



struct iavf_tx_ipsec_desc {
	union {
		struct {
			__le64 qw0;
			__le64 qw1;
		};
		struct {
			__le16 l4payload_length;
			__le32 esn;
			__le16 trailer_length;
			u8 type:4;
			u8 rsv:1;
			u8 udp:1;
			u8 ivlen:2;
			u8 next_header;
			__le16 ipv6_ext_hdr_length;
			__le32 said;
		} __rte_packed;
	};
} __rte_packed;

#define IAVF_IPSEC_TX_DESC_QW0_L4PAYLEN_SHIFT    0
#define IAVF_IPSEC_TX_DESC_QW0_L4PAYLEN_MASK     (0x3FFFULL << \
			IAVF_IPSEC_TX_DESC_QW0_L4PAYLEN_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW0_IPSECESN_SHIFT    16
#define IAVF_IPSEC_TX_DESC_QW0_IPSECESN_MASK     (0xFFFFFFFFULL << \
			IAVF_IPSEC_TX_DESC_QW0_IPSECESN_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW0_TRAILERLEN_SHIFT  48
#define IAVF_IPSEC_TX_DESC_QW0_TRAILERLEN_MASK   (0x3FULL << \
			IAVF_IPSEC_TX_DESC_QW0_TRAILERLEN_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW1_UDP_SHIFT         5
#define IAVF_IPSEC_TX_DESC_QW1_UDP_MASK          (0x1ULL << \
			IAVF_IPSEC_TX_DESC_QW1_UDP_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW1_IVLEN_SHIFT       6
#define IAVF_IPSEC_TX_DESC_QW1_IVLEN_MASK        (0x3ULL << \
			IAVF_IPSEC_TX_DESC_QW1_IVLEN_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW1_IPSECNH_SHIFT     8
#define IAVF_IPSEC_TX_DESC_QW1_IPSECNH_MASK      (0xFFULL << \
			IAVF_IPSEC_TX_DESC_QW1_IPSECNH_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW1_EXTLEN_SHIFT      16
#define IAVF_IPSEC_TX_DESC_QW1_EXTLEN_MASK       (0xFFULL << \
			IAVF_IPSEC_TX_DESC_QW1_EXTLEN_SHIFT)

#define IAVF_IPSEC_TX_DESC_QW1_IPSECSA_SHIFT     32
#define IAVF_IPSEC_TX_DESC_QW1_IPSECSA_MASK      (0xFFFFFULL << \
			IAVF_IPSEC_TX_DESC_QW1_IPSECSA_SHIFT)

/* Initialization Vector Length type */
enum iavf_ipsec_iv_len {
	IAVF_IPSEC_IV_LEN_NONE,		/* No IV */
	IAVF_IPSEC_IV_LEN_DW,		/* 4B IV */
	IAVF_IPSEC_IV_LEN_DDW,		/* 8B IV */
	IAVF_IPSEC_IV_LEN_QDW,		/* 16B IV */
};


/* IPsec Crypto Packet Metadata offload flags */
#define IAVF_IPSEC_CRYPTO_OL_FLAGS_IS_TUN		(0x1 << 0)
#define IAVF_IPSEC_CRYPTO_OL_FLAGS_ESN			(0x1 << 1)
#define IAVF_IPSEC_CRYPTO_OL_FLAGS_IPV6_EXT_HDRS	(0x1 << 2)
#define IAVF_IPSEC_CRYPTO_OL_FLAGS_NATT			(0x1 << 3)

/**
 * Packet metadata data structure used to hold parameters required by the iAVF
 * transmit data path. Parameters set for session by calling
 * rte_security_set_pkt_metadata() API.
 */
struct iavf_ipsec_crypto_pkt_metadata {
	uint32_t sa_idx;                /* SA hardware index (20b/4B) */

	uint8_t ol_flags;		/* flags (1B) */
	uint8_t len_iv;			/* IV length (2b/1B) */
	uint8_t ctx_desc_ipsec_params;	/* IPsec params for ctx desc (7b/1B) */
	uint8_t esp_trailer_len;	/* ESP trailer length (6b/1B) */

	uint16_t l4_payload_len;	/* L4 payload length */
	uint8_t ipv6_ext_hdrs_len;	/* IPv6 extender headers len (5b/1B) */
	uint8_t next_proto;		/* Next Protocol (8b/1B) */

	uint32_t esn;		        /* Extended Sequence Number (32b/4B) */
} __rte_packed;

/**
 * Inline IPsec Crypto offload is supported
 */
int
iavf_ipsec_crypto_supported(struct iavf_adapter *adapter);

/**
 * Create security context
 */
int iavf_security_ctx_create(struct iavf_adapter *adapter);

/**
 * Create security context
 */
int iavf_security_init(struct iavf_adapter *adapter);

/**
 * Set security capabilities
 */
int iavf_ipsec_crypto_set_security_capabililites(struct iavf_security_ctx
		*iavf_sctx, struct virtchnl_ipsec_cap *virtchl_capabilities);


int iavf_security_get_pkt_md_offset(struct iavf_adapter *adapter);

/**
 * Destroy security context
 */
int iavf_security_ctx_destroy(struct iavf_adapter *adapterv);

/**
 * Verify that the inline IPsec Crypto action is valid for this device
 */
uint32_t
iavf_ipsec_crypto_action_valid(struct rte_eth_dev *ethdev,
	const struct rte_security_session *session, uint32_t spi);

/**
 * Add inbound security policy rule to hardware
 */
int
iavf_ipsec_crypto_inbound_security_policy_add(struct iavf_adapter *adapter,
	uint32_t esp_spi,
	uint8_t is_v4,
	rte_be32_t v4_dst_addr,
	uint8_t *v6_dst_addr,
	uint8_t drop,
	bool is_udp,
	uint16_t udp_port);

/**
 * Delete inbound security policy rule from hardware
 */
int
iavf_ipsec_crypto_security_policy_delete(struct iavf_adapter *adapter,
	uint8_t is_v4, uint32_t flow_id);

int
iavf_security_get_pkt_md_offset(struct iavf_adapter *adapter);

#endif /* _IAVF_IPSEC_CRYPTO_H_ */
