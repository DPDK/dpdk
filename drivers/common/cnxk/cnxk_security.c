/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_udp.h>

#include "cnxk_security.h"

#include "roc_api.h"

static void
ipsec_hmac_opad_ipad_gen(struct rte_crypto_sym_xform *auth_xform,
			 uint8_t *hmac_opad_ipad)
{
	const uint8_t *key = auth_xform->auth.key.data;
	uint32_t length = auth_xform->auth.key.length;
	uint8_t opad[128] = {[0 ... 127] = 0x5c};
	uint8_t ipad[128] = {[0 ... 127] = 0x36};
	uint32_t i;

	/* HMAC OPAD and IPAD */
	for (i = 0; i < 127 && i < length; i++) {
		opad[i] = opad[i] ^ key[i];
		ipad[i] = ipad[i] ^ key[i];
	}

	/* Precompute hash of HMAC OPAD and IPAD to avoid
	 * per packet computation
	 */
	switch (auth_xform->auth.algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		roc_hash_sha1_gen(opad, (uint32_t *)&hmac_opad_ipad[0]);
		roc_hash_sha1_gen(ipad, (uint32_t *)&hmac_opad_ipad[24]);
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		roc_hash_sha256_gen(opad, (uint32_t *)&hmac_opad_ipad[0]);
		roc_hash_sha256_gen(ipad, (uint32_t *)&hmac_opad_ipad[64]);
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		roc_hash_sha512_gen(opad, (uint64_t *)&hmac_opad_ipad[0], 384);
		roc_hash_sha512_gen(ipad, (uint64_t *)&hmac_opad_ipad[64], 384);
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		roc_hash_sha512_gen(opad, (uint64_t *)&hmac_opad_ipad[0], 512);
		roc_hash_sha512_gen(ipad, (uint64_t *)&hmac_opad_ipad[64], 512);
		break;
	default:
		break;
	}
}

static int
ot_ipsec_sa_common_param_fill(union roc_ot_ipsec_sa_word2 *w2,
			      uint8_t *cipher_key, uint8_t *salt_key,
			      uint8_t *hmac_opad_ipad,
			      struct rte_security_ipsec_xform *ipsec_xfrm,
			      struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_crypto_sym_xform *auth_xfrm, *cipher_xfrm;
	const uint8_t *key;
	uint32_t *tmp_salt;
	uint64_t *tmp_key;
	int length, i;

	/* Set direction */
	switch (ipsec_xfrm->direction) {
	case RTE_SECURITY_IPSEC_SA_DIR_INGRESS:
		w2->s.dir = ROC_IE_SA_DIR_INBOUND;
		auth_xfrm = crypto_xfrm;
		cipher_xfrm = crypto_xfrm->next;
		break;
	case RTE_SECURITY_IPSEC_SA_DIR_EGRESS:
		w2->s.dir = ROC_IE_SA_DIR_OUTBOUND;
		cipher_xfrm = crypto_xfrm;
		auth_xfrm = crypto_xfrm->next;
		break;
	default:
		return -EINVAL;
	}

	/* Set protocol - ESP vs AH */
	switch (ipsec_xfrm->proto) {
	case RTE_SECURITY_IPSEC_SA_PROTO_ESP:
		w2->s.protocol = ROC_IE_SA_PROTOCOL_ESP;
		break;
	case RTE_SECURITY_IPSEC_SA_PROTO_AH:
		w2->s.protocol = ROC_IE_SA_PROTOCOL_AH;
		break;
	default:
		return -EINVAL;
	}

	/* Set mode - transport vs tunnel */
	switch (ipsec_xfrm->mode) {
	case RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT:
		w2->s.mode = ROC_IE_SA_MODE_TRANSPORT;
		break;
	case RTE_SECURITY_IPSEC_SA_MODE_TUNNEL:
		w2->s.mode = ROC_IE_SA_MODE_TUNNEL;
		break;
	default:
		return -EINVAL;
	}

	/* Set encryption algorithm */
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		key = crypto_xfrm->aead.key.data;
		length = crypto_xfrm->aead.key.length;

		switch (crypto_xfrm->aead.algo) {
		case RTE_CRYPTO_AEAD_AES_GCM:
			w2->s.enc_type = ROC_IE_OT_SA_ENC_AES_GCM;
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_NULL;
			memcpy(salt_key, &ipsec_xfrm->salt, 4);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		default:
			return -ENOTSUP;
		}
	} else {
		switch (cipher_xfrm->cipher.algo) {
		case RTE_CRYPTO_CIPHER_NULL:
			w2->s.enc_type = ROC_IE_OT_SA_ENC_NULL;
			break;
		case RTE_CRYPTO_CIPHER_AES_CBC:
			w2->s.enc_type = ROC_IE_OT_SA_ENC_AES_CBC;
			break;
		case RTE_CRYPTO_CIPHER_AES_CTR:
			w2->s.enc_type = ROC_IE_OT_SA_ENC_AES_CTR;
			break;
		default:
			return -ENOTSUP;
		}

		switch (auth_xfrm->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			w2->s.auth_type = ROC_IE_OT_SA_AUTH_AES_XCBC_128;
			break;
		default:
			return -ENOTSUP;
		}

		if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
			const uint8_t *auth_key = auth_xfrm->auth.key.data;
			roc_aes_xcbc_key_derive(auth_key, hmac_opad_ipad);
		} else {
			ipsec_hmac_opad_ipad_gen(auth_xfrm, hmac_opad_ipad);
		}

		tmp_key = (uint64_t *)hmac_opad_ipad;
		for (i = 0;
		     i < (int)(ROC_CTX_MAX_OPAD_IPAD_LEN / sizeof(uint64_t));
		     i++)
			tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);

		key = cipher_xfrm->cipher.key.data;
		length = cipher_xfrm->cipher.key.length;
	}

	/* Set encapsulation type */
	if (ipsec_xfrm->options.udp_encap)
		w2->s.encap_type = ROC_IE_OT_SA_ENCAP_UDP;

	w2->s.spi = ipsec_xfrm->spi;

	/* Copy encryption key */
	memcpy(cipher_key, key, length);
	tmp_key = (uint64_t *)cipher_key;
	for (i = 0; i < (int)(ROC_CTX_MAX_CKEY_LEN / sizeof(uint64_t)); i++)
		tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);

	/* Set AES key length */
	if (w2->s.enc_type == ROC_IE_OT_SA_ENC_AES_CBC ||
	    w2->s.enc_type == ROC_IE_OT_SA_ENC_AES_CCM ||
	    w2->s.enc_type == ROC_IE_OT_SA_ENC_AES_CTR ||
	    w2->s.enc_type == ROC_IE_OT_SA_ENC_AES_GCM ||
	    w2->s.auth_type == ROC_IE_OT_SA_AUTH_AES_GMAC) {
		switch (length) {
		case ROC_CPT_AES128_KEY_LEN:
			w2->s.aes_key_len = ROC_IE_SA_AES_KEY_LEN_128;
			break;
		case ROC_CPT_AES192_KEY_LEN:
			w2->s.aes_key_len = ROC_IE_SA_AES_KEY_LEN_192;
			break;
		case ROC_CPT_AES256_KEY_LEN:
			w2->s.aes_key_len = ROC_IE_SA_AES_KEY_LEN_256;
			break;
		default:
			plt_err("Invalid AES key length");
			return -EINVAL;
		}
	}

	if (ipsec_xfrm->life.packets_soft_limit != 0 ||
	    ipsec_xfrm->life.packets_hard_limit != 0) {
		if (ipsec_xfrm->life.bytes_soft_limit != 0 ||
		    ipsec_xfrm->life.bytes_hard_limit != 0) {
			plt_err("Expiry tracking with both packets & bytes is not supported");
			return -EINVAL;
		}
		w2->s.life_unit = ROC_IE_OT_SA_LIFE_UNIT_PKTS;
	}

	if (ipsec_xfrm->life.bytes_soft_limit != 0 ||
	    ipsec_xfrm->life.bytes_hard_limit != 0) {
		if (ipsec_xfrm->life.packets_soft_limit != 0 ||
		    ipsec_xfrm->life.packets_hard_limit != 0) {
			plt_err("Expiry tracking with both packets & bytes is not supported");
			return -EINVAL;
		}
		w2->s.life_unit = ROC_IE_OT_SA_LIFE_UNIT_OCTETS;
	}

	return 0;
}

static size_t
ot_ipsec_inb_ctx_size(struct roc_ot_ipsec_inb_sa *sa)
{
	size_t size;

	/* Variable based on Anti-replay Window */
	size = offsetof(struct roc_ot_ipsec_inb_sa, ctx) +
	       offsetof(struct roc_ot_ipsec_inb_ctx_update_reg, ar_winbits);

	if (sa->w0.s.ar_win)
		size += (1 << (sa->w0.s.ar_win - 1)) * sizeof(uint64_t);

	return size;
}

static int
ot_ipsec_inb_tunnel_hdr_fill(struct roc_ot_ipsec_inb_sa *sa,
			     struct rte_security_ipsec_xform *ipsec_xfrm)
{
	struct rte_security_ipsec_tunnel_param *tunnel;

	if (ipsec_xfrm->mode != RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		return 0;

	if (ipsec_xfrm->options.tunnel_hdr_verify == 0)
		return 0;

	tunnel = &ipsec_xfrm->tunnel;

	switch (tunnel->type) {
	case RTE_SECURITY_IPSEC_TUNNEL_IPV4:
		sa->w2.s.outer_ip_ver = ROC_IE_SA_IP_VERSION_4;
		memcpy(&sa->outer_hdr.ipv4.src_addr, &tunnel->ipv4.src_ip,
		       sizeof(struct in_addr));
		memcpy(&sa->outer_hdr.ipv4.dst_addr, &tunnel->ipv4.dst_ip,
		       sizeof(struct in_addr));

		/* IP Source and Dest are in LE/CPU endian */
		sa->outer_hdr.ipv4.src_addr =
			rte_be_to_cpu_32(sa->outer_hdr.ipv4.src_addr);
		sa->outer_hdr.ipv4.dst_addr =
			rte_be_to_cpu_32(sa->outer_hdr.ipv4.dst_addr);

		break;
	case RTE_SECURITY_IPSEC_TUNNEL_IPV6:
		sa->w2.s.outer_ip_ver = ROC_IE_SA_IP_VERSION_6;
		memcpy(&sa->outer_hdr.ipv6.src_addr, &tunnel->ipv6.src_addr,
		       sizeof(struct in6_addr));
		memcpy(&sa->outer_hdr.ipv6.dst_addr, &tunnel->ipv6.dst_addr,
		       sizeof(struct in6_addr));

		break;
	default:
		return -EINVAL;
	}

	switch (ipsec_xfrm->options.tunnel_hdr_verify) {
	case RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR:
		sa->w2.s.ip_hdr_verify = ROC_IE_OT_SA_IP_HDR_VERIFY_DST_ADDR;
		break;
	case RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR:
		sa->w2.s.ip_hdr_verify =
			ROC_IE_OT_SA_IP_HDR_VERIFY_SRC_DST_ADDR;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

int
cnxk_ot_ipsec_inb_sa_fill(struct roc_ot_ipsec_inb_sa *sa,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm,
			  bool is_inline)
{
	union roc_ot_ipsec_sa_word2 w2;
	uint32_t replay_win_sz;
	size_t offset;
	int rc;

	/* Initialize the SA */
	roc_ot_ipsec_inb_sa_init(sa, is_inline);

	w2.u64 = 0;
	rc = ot_ipsec_sa_common_param_fill(&w2, sa->cipher_key, sa->w8.s.salt,
					   sa->hmac_opad_ipad, ipsec_xfrm,
					   crypto_xfrm);
	if (rc)
		return rc;

	/* Updata common word2 data */
	sa->w2.u64 = w2.u64;

	/* Only support power-of-two window sizes supported */
	replay_win_sz = ipsec_xfrm->replay_win_sz;
	if (replay_win_sz) {
		if (!rte_is_power_of_2(replay_win_sz) ||
		    replay_win_sz > ROC_AR_WIN_SIZE_MAX)
			return -ENOTSUP;

		sa->w0.s.ar_win = rte_log2_u32(replay_win_sz) - 5;
	}

	rc = ot_ipsec_inb_tunnel_hdr_fill(sa, ipsec_xfrm);
	if (rc)
		return rc;

	/* Default options for pkt_out and pkt_fmt are with
	 * second pass meta and no defrag.
	 */
	sa->w0.s.pkt_format = ROC_IE_OT_SA_PKT_FMT_META;
	sa->w0.s.pkt_output = ROC_IE_OT_SA_PKT_OUTPUT_NO_FRAG;
	sa->w0.s.pkind = ROC_IE_OT_CPT_PKIND;

	if (ipsec_xfrm->options.ip_reassembly_en)
		sa->w0.s.pkt_output = ROC_IE_OT_SA_PKT_OUTPUT_HW_BASED_DEFRAG;

	/* ESN */
	sa->w2.s.esn_en = !!ipsec_xfrm->options.esn;
	if (ipsec_xfrm->options.udp_encap) {
		sa->w10.s.udp_src_port = 4500;
		sa->w10.s.udp_dst_port = 4500;
	}

	if (ipsec_xfrm->options.udp_ports_verify)
		sa->w2.s.udp_ports_verify = 1;

	offset = offsetof(struct roc_ot_ipsec_inb_sa, ctx);
	/* Word offset for HW managed SA field */
	sa->w0.s.hw_ctx_off = offset / 8;
	/* Context push size for inbound spans up to hw_ctx including
	 * ar_base field, in 8b units
	 */
	sa->w0.s.ctx_push_size = sa->w0.s.hw_ctx_off + 1;
	/* Entire context size in 128B units */
	sa->w0.s.ctx_size =
		(PLT_ALIGN_CEIL(ot_ipsec_inb_ctx_size(sa), ROC_CTX_UNIT_128B) /
		 ROC_CTX_UNIT_128B) -
		1;

	/**
	 * CPT MC triggers expiry when counter value changes from 2 to 1. To
	 * mitigate this behaviour add 1 to the life counter values provided.
	 */

	if (ipsec_xfrm->life.bytes_soft_limit) {
		sa->ctx.soft_life = ipsec_xfrm->life.bytes_soft_limit + 1;
		sa->w0.s.soft_life_dec = 1;
	}

	if (ipsec_xfrm->life.packets_soft_limit) {
		sa->ctx.soft_life = ipsec_xfrm->life.packets_soft_limit + 1;
		sa->w0.s.soft_life_dec = 1;
	}

	if (ipsec_xfrm->life.bytes_hard_limit) {
		sa->ctx.hard_life = ipsec_xfrm->life.bytes_hard_limit + 1;
		sa->w0.s.hard_life_dec = 1;
	}

	if (ipsec_xfrm->life.packets_hard_limit) {
		sa->ctx.hard_life = ipsec_xfrm->life.packets_hard_limit + 1;
		sa->w0.s.hard_life_dec = 1;
	}

	rte_wmb();

	/* Enable SA */
	sa->w2.s.valid = 1;
	return 0;
}

int
cnxk_ot_ipsec_outb_sa_fill(struct roc_ot_ipsec_outb_sa *sa,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_security_ipsec_tunnel_param *tunnel = &ipsec_xfrm->tunnel;
	union roc_ot_ipsec_sa_word2 w2;
	size_t offset;
	int rc;

	/* Initialize the SA */
	roc_ot_ipsec_outb_sa_init(sa);

	w2.u64 = 0;
	rc = ot_ipsec_sa_common_param_fill(&w2, sa->cipher_key, sa->iv.s.salt,
					   sa->hmac_opad_ipad, ipsec_xfrm,
					   crypto_xfrm);
	if (rc)
		return rc;

	/* Update common word2 data */
	sa->w2.u64 = w2.u64;

	if (ipsec_xfrm->mode != RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		goto skip_tunnel_info;

	/* Tunnel header info */
	switch (tunnel->type) {
	case RTE_SECURITY_IPSEC_TUNNEL_IPV4:
		sa->w2.s.outer_ip_ver = ROC_IE_SA_IP_VERSION_4;
		memcpy(&sa->outer_hdr.ipv4.src_addr, &tunnel->ipv4.src_ip,
		       sizeof(struct in_addr));
		memcpy(&sa->outer_hdr.ipv4.dst_addr, &tunnel->ipv4.dst_ip,
		       sizeof(struct in_addr));

		/* IP Source and Dest seems to be in LE/CPU endian */
		sa->outer_hdr.ipv4.src_addr =
			rte_be_to_cpu_32(sa->outer_hdr.ipv4.src_addr);
		sa->outer_hdr.ipv4.dst_addr =
			rte_be_to_cpu_32(sa->outer_hdr.ipv4.dst_addr);

		/* Outer header DF bit source */
		if (!ipsec_xfrm->options.copy_df) {
			sa->w2.s.ipv4_df_src_or_ipv6_flw_lbl_src =
				ROC_IE_OT_SA_COPY_FROM_SA;
			sa->w10.s.ipv4_df_or_ipv6_flw_lbl = tunnel->ipv4.df;
		} else {
			sa->w2.s.ipv4_df_src_or_ipv6_flw_lbl_src =
				ROC_IE_OT_SA_COPY_FROM_INNER_IP_HDR;
		}

		/* Outer header DSCP source */
		if (!ipsec_xfrm->options.copy_dscp) {
			sa->w2.s.dscp_src = ROC_IE_OT_SA_COPY_FROM_SA;
			sa->w10.s.dscp = tunnel->ipv4.dscp;
		} else {
			sa->w2.s.dscp_src = ROC_IE_OT_SA_COPY_FROM_INNER_IP_HDR;
		}
		break;
	case RTE_SECURITY_IPSEC_TUNNEL_IPV6:
		sa->w2.s.outer_ip_ver = ROC_IE_SA_IP_VERSION_6;
		memcpy(&sa->outer_hdr.ipv6.src_addr, &tunnel->ipv6.src_addr,
		       sizeof(struct in6_addr));
		memcpy(&sa->outer_hdr.ipv6.dst_addr, &tunnel->ipv6.dst_addr,
		       sizeof(struct in6_addr));

		/* Outer header flow label source */
		if (!ipsec_xfrm->options.copy_flabel) {
			sa->w2.s.ipv4_df_src_or_ipv6_flw_lbl_src =
				ROC_IE_OT_SA_COPY_FROM_SA;

			sa->w10.s.ipv4_df_or_ipv6_flw_lbl = tunnel->ipv6.flabel;
		} else {
			sa->w2.s.ipv4_df_src_or_ipv6_flw_lbl_src =
				ROC_IE_OT_SA_COPY_FROM_INNER_IP_HDR;
		}

		/* Outer header DSCP source */
		if (!ipsec_xfrm->options.copy_dscp) {
			sa->w2.s.dscp_src = ROC_IE_OT_SA_COPY_FROM_SA;
			sa->w10.s.dscp = tunnel->ipv6.dscp;
		} else {
			sa->w2.s.dscp_src = ROC_IE_OT_SA_COPY_FROM_INNER_IP_HDR;
		}
		break;
	default:
		return -EINVAL;
	}

skip_tunnel_info:
	/* ESN */
	sa->w0.s.esn_en = !!ipsec_xfrm->options.esn;

	if (ipsec_xfrm->esn.value)
		sa->ctx.esn_val = ipsec_xfrm->esn.value - 1;

	if (ipsec_xfrm->options.udp_encap) {
		sa->w10.s.udp_src_port = 4500;
		sa->w10.s.udp_dst_port = 4500;
	}

	offset = offsetof(struct roc_ot_ipsec_outb_sa, ctx);
	/* Word offset for HW managed SA field */
	sa->w0.s.hw_ctx_off = offset / 8;

	/* Context push size is up to err ctl in HW ctx */
	sa->w0.s.ctx_push_size = sa->w0.s.hw_ctx_off + 1;

	/* Entire context size in 128B units */
	offset = sizeof(struct roc_ot_ipsec_outb_sa);
	sa->w0.s.ctx_size = (PLT_ALIGN_CEIL(offset, ROC_CTX_UNIT_128B) /
			     ROC_CTX_UNIT_128B) -
			    1;

	/* IPID gen */
	sa->w2.s.ipid_gen = 1;

	/**
	 * CPT MC triggers expiry when counter value changes from 2 to 1. To
	 * mitigate this behaviour add 1 to the life counter values provided.
	 */

	if (ipsec_xfrm->life.bytes_soft_limit) {
		sa->ctx.soft_life = ipsec_xfrm->life.bytes_soft_limit + 1;
		sa->w0.s.soft_life_dec = 1;
	}

	if (ipsec_xfrm->life.packets_soft_limit) {
		sa->ctx.soft_life = ipsec_xfrm->life.packets_soft_limit + 1;
		sa->w0.s.soft_life_dec = 1;
	}

	if (ipsec_xfrm->life.bytes_hard_limit) {
		sa->ctx.hard_life = ipsec_xfrm->life.bytes_hard_limit + 1;
		sa->w0.s.hard_life_dec = 1;
	}

	if (ipsec_xfrm->life.packets_hard_limit) {
		sa->ctx.hard_life = ipsec_xfrm->life.packets_hard_limit + 1;
		sa->w0.s.hard_life_dec = 1;
	}

	/* There are two words of CPT_CTX_HW_S for ucode to skip */
	sa->w0.s.ctx_hdr_size = 1;
	sa->w0.s.aop_valid = 1;

	rte_wmb();

	/* Enable SA */
	sa->w2.s.valid = 1;
	return 0;
}

bool
cnxk_ot_ipsec_inb_sa_valid(struct roc_ot_ipsec_inb_sa *sa)
{
	return !!sa->w2.s.valid;
}

bool
cnxk_ot_ipsec_outb_sa_valid(struct roc_ot_ipsec_outb_sa *sa)
{
	return !!sa->w2.s.valid;
}

static inline int
ipsec_xfrm_verify(struct rte_security_ipsec_xform *ipsec_xfrm,
		  struct rte_crypto_sym_xform *crypto_xfrm)
{
	if (crypto_xfrm->next == NULL)
		return -EINVAL;

	if (ipsec_xfrm->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		if (crypto_xfrm->type != RTE_CRYPTO_SYM_XFORM_AUTH ||
		    crypto_xfrm->next->type != RTE_CRYPTO_SYM_XFORM_CIPHER)
			return -EINVAL;
	} else {
		if (crypto_xfrm->type != RTE_CRYPTO_SYM_XFORM_CIPHER ||
		    crypto_xfrm->next->type != RTE_CRYPTO_SYM_XFORM_AUTH)
			return -EINVAL;
	}

	return 0;
}

static int
onf_ipsec_sa_common_param_fill(struct roc_ie_onf_sa_ctl *ctl, uint8_t *salt,
			       uint8_t *cipher_key, uint8_t *hmac_opad_ipad,
			       struct rte_security_ipsec_xform *ipsec_xfrm,
			       struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_crypto_sym_xform *auth_xfrm, *cipher_xfrm;
	int rc, length, auth_key_len;
	const uint8_t *key = NULL;

	/* Set direction */
	switch (ipsec_xfrm->direction) {
	case RTE_SECURITY_IPSEC_SA_DIR_INGRESS:
		ctl->direction = ROC_IE_SA_DIR_INBOUND;
		auth_xfrm = crypto_xfrm;
		cipher_xfrm = crypto_xfrm->next;
		break;
	case RTE_SECURITY_IPSEC_SA_DIR_EGRESS:
		ctl->direction = ROC_IE_SA_DIR_OUTBOUND;
		cipher_xfrm = crypto_xfrm;
		auth_xfrm = crypto_xfrm->next;
		break;
	default:
		return -EINVAL;
	}

	/* Set protocol - ESP vs AH */
	switch (ipsec_xfrm->proto) {
	case RTE_SECURITY_IPSEC_SA_PROTO_ESP:
		ctl->ipsec_proto = ROC_IE_SA_PROTOCOL_ESP;
		break;
	case RTE_SECURITY_IPSEC_SA_PROTO_AH:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}

	/* Set mode - transport vs tunnel */
	switch (ipsec_xfrm->mode) {
	case RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT:
		ctl->ipsec_mode = ROC_IE_SA_MODE_TRANSPORT;
		break;
	case RTE_SECURITY_IPSEC_SA_MODE_TUNNEL:
		ctl->ipsec_mode = ROC_IE_SA_MODE_TUNNEL;
		break;
	default:
		return -EINVAL;
	}

	/* Set encryption algorithm */
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		length = crypto_xfrm->aead.key.length;

		switch (crypto_xfrm->aead.algo) {
		case RTE_CRYPTO_AEAD_AES_GCM:
			ctl->enc_type = ROC_IE_ON_SA_ENC_AES_GCM;
			ctl->auth_type = ROC_IE_ON_SA_AUTH_NULL;
			memcpy(salt, &ipsec_xfrm->salt, 4);
			key = crypto_xfrm->aead.key.data;
			break;
		default:
			return -ENOTSUP;
		}

	} else {
		rc = ipsec_xfrm_verify(ipsec_xfrm, crypto_xfrm);
		if (rc)
			return rc;

		switch (cipher_xfrm->cipher.algo) {
		case RTE_CRYPTO_CIPHER_AES_CBC:
			ctl->enc_type = ROC_IE_ON_SA_ENC_AES_CBC;
			break;
		case RTE_CRYPTO_CIPHER_AES_CTR:
			ctl->enc_type = ROC_IE_ON_SA_ENC_AES_CTR;
			break;
		default:
			return -ENOTSUP;
		}

		switch (auth_xfrm->auth.algo) {
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_SHA1;
			break;
		default:
			return -ENOTSUP;
		}
		auth_key_len = auth_xfrm->auth.key.length;
		if (auth_key_len < 20 || auth_key_len > 64)
			return -ENOTSUP;

		key = cipher_xfrm->cipher.key.data;
		length = cipher_xfrm->cipher.key.length;

		ipsec_hmac_opad_ipad_gen(auth_xfrm, hmac_opad_ipad);
	}

	switch (length) {
	case ROC_CPT_AES128_KEY_LEN:
		ctl->aes_key_len = ROC_IE_SA_AES_KEY_LEN_128;
		break;
	case ROC_CPT_AES192_KEY_LEN:
		ctl->aes_key_len = ROC_IE_SA_AES_KEY_LEN_192;
		break;
	case ROC_CPT_AES256_KEY_LEN:
		ctl->aes_key_len = ROC_IE_SA_AES_KEY_LEN_256;
		break;
	default:
		return -EINVAL;
	}

	memcpy(cipher_key, key, length);

	if (ipsec_xfrm->options.esn)
		ctl->esn_en = 1;

	ctl->spi = rte_cpu_to_be_32(ipsec_xfrm->spi);
	return 0;
}

int
cnxk_onf_ipsec_inb_sa_fill(struct roc_onf_ipsec_inb_sa *sa,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct roc_ie_onf_sa_ctl *ctl = &sa->ctl;
	int rc;

	rc = onf_ipsec_sa_common_param_fill(ctl, sa->nonce, sa->cipher_key,
					    sa->hmac_key, ipsec_xfrm,
					    crypto_xfrm);
	if (rc)
		return rc;

	rte_wmb();

	/* Enable SA */
	ctl->valid = 1;
	return 0;
}

int
cnxk_onf_ipsec_outb_sa_fill(struct roc_onf_ipsec_outb_sa *sa,
			    struct rte_security_ipsec_xform *ipsec_xfrm,
			    struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_security_ipsec_tunnel_param *tunnel = &ipsec_xfrm->tunnel;
	struct roc_ie_onf_sa_ctl *ctl = &sa->ctl;
	int rc;

	/* Fill common params */
	rc = onf_ipsec_sa_common_param_fill(ctl, sa->nonce, sa->cipher_key,
					    sa->hmac_key, ipsec_xfrm,
					    crypto_xfrm);
	if (rc)
		return rc;

	if (ipsec_xfrm->mode != RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		goto skip_tunnel_info;

	/* Tunnel header info */
	switch (tunnel->type) {
	case RTE_SECURITY_IPSEC_TUNNEL_IPV4:
		memcpy(&sa->ip_src, &tunnel->ipv4.src_ip,
		       sizeof(struct in_addr));
		memcpy(&sa->ip_dst, &tunnel->ipv4.dst_ip,
		       sizeof(struct in_addr));
		break;
	case RTE_SECURITY_IPSEC_TUNNEL_IPV6:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}

	/* Update udp encap ports */
	if (ipsec_xfrm->options.udp_encap == 1) {
		sa->udp_src = 4500;
		sa->udp_dst = 4500;
	}

skip_tunnel_info:
	rte_wmb();

	/* Enable SA */
	ctl->valid = 1;
	return 0;
}

bool
cnxk_onf_ipsec_inb_sa_valid(struct roc_onf_ipsec_inb_sa *sa)
{
	return !!sa->ctl.valid;
}

bool
cnxk_onf_ipsec_outb_sa_valid(struct roc_onf_ipsec_outb_sa *sa)
{
	return !!sa->ctl.valid;
}

uint8_t
cnxk_ipsec_ivlen_get(enum rte_crypto_cipher_algorithm c_algo,
		     enum rte_crypto_auth_algorithm a_algo,
		     enum rte_crypto_aead_algorithm aead_algo)
{
	uint8_t ivlen = 0;

	if (aead_algo == RTE_CRYPTO_AEAD_AES_GCM)
		ivlen = 8;

	switch (c_algo) {
	case RTE_CRYPTO_CIPHER_AES_CTR:
		ivlen = 8;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		ivlen = ROC_CPT_DES_BLOCK_LENGTH;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		ivlen = ROC_CPT_AES_BLOCK_LENGTH;
		break;
	default:
		break;
	}

	switch (a_algo) {
	case RTE_CRYPTO_AUTH_AES_GMAC:
		ivlen = 8;
		break;
	default:
		break;
	}

	return ivlen;
}

uint8_t
cnxk_ipsec_icvlen_get(enum rte_crypto_cipher_algorithm c_algo,
		      enum rte_crypto_auth_algorithm a_algo,
		      enum rte_crypto_aead_algorithm aead_algo)
{
	uint8_t icv = 0;

	(void)c_algo;

	switch (a_algo) {
	case RTE_CRYPTO_AUTH_NULL:
		icv = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		icv = 12;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_AES_GMAC:
		icv = 16;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		icv = 24;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		icv = 32;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		icv = 12;
		break;
	default:
		break;
	}

	switch (aead_algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		icv = 16;
		break;
	default:
		break;
	}

	return icv;
}

uint8_t
cnxk_ipsec_outb_roundup_byte(enum rte_crypto_cipher_algorithm c_algo,
			     enum rte_crypto_aead_algorithm aead_algo)
{
	uint8_t roundup_byte = 4;

	if (aead_algo == RTE_CRYPTO_AEAD_AES_GCM)
		return roundup_byte;

	switch (c_algo) {
	case RTE_CRYPTO_CIPHER_AES_CTR:
		roundup_byte = 4;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		roundup_byte = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		roundup_byte = 8;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		roundup_byte = 4;
		break;
	default:
		break;
	}

	return roundup_byte;
}

int
cnxk_ipsec_outb_rlens_get(struct cnxk_ipsec_outb_rlens *rlens,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_security_ipsec_tunnel_param *tunnel = &ipsec_xfrm->tunnel;
	enum rte_crypto_cipher_algorithm c_algo = RTE_CRYPTO_CIPHER_NULL;
	enum rte_crypto_auth_algorithm a_algo = RTE_CRYPTO_AUTH_NULL;
	enum rte_crypto_aead_algorithm aead_algo = 0;
	uint16_t partial_len = 0;
	uint8_t roundup_byte = 0;
	int8_t roundup_len = 0;

	memset(rlens, 0, sizeof(struct cnxk_ipsec_outb_rlens));

	/* Get Cipher and Auth algo */
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		aead_algo = crypto_xfrm->aead.algo;
	} else {
		if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			c_algo = crypto_xfrm->cipher.algo;
		else
			a_algo = crypto_xfrm->auth.algo;

		if (crypto_xfrm->next) {
			if (crypto_xfrm->next->type ==
			    RTE_CRYPTO_SYM_XFORM_CIPHER)
				c_algo = crypto_xfrm->next->cipher.algo;
			else
				a_algo = crypto_xfrm->next->auth.algo;
		}
	}

	if (ipsec_xfrm->proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP) {
		partial_len = ROC_CPT_ESP_HDR_LEN;
		roundup_len = ROC_CPT_ESP_TRL_LEN;
	} else {
		partial_len = ROC_CPT_AH_HDR_LEN;
	}

	if (ipsec_xfrm->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (tunnel->type == RTE_SECURITY_IPSEC_TUNNEL_IPV4)
			partial_len += ROC_CPT_TUNNEL_IPV4_HDR_LEN;
		else
			partial_len += ROC_CPT_TUNNEL_IPV6_HDR_LEN;
	}

	partial_len += cnxk_ipsec_ivlen_get(c_algo, a_algo, aead_algo);
	partial_len += cnxk_ipsec_icvlen_get(c_algo, a_algo, aead_algo);
	roundup_byte = cnxk_ipsec_outb_roundup_byte(c_algo, aead_algo);

	if (ipsec_xfrm->options.udp_encap)
		partial_len += sizeof(struct rte_udp_hdr);

	rlens->partial_len = partial_len;
	rlens->roundup_len = roundup_len;
	rlens->roundup_byte = roundup_byte;
	rlens->max_extended_len = partial_len + roundup_len + roundup_byte;
	return 0;
}
