/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <eal_export.h>
#include <rte_udp.h>

#include "roc_api.h"

#include "cnxk_security.h"

static int
ot_ipsec_sa_common_param_fill(union roc_ot_ipsec_sa_word2 *w2, uint8_t *cipher_key,
			      uint8_t *salt_key, uint8_t *hmac_opad_ipad,
			      struct rte_security_ipsec_xform *ipsec_xfrm,
			      struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_crypto_sym_xform *auth_xfrm, *cipher_xfrm;
	const uint8_t *key = NULL;
	uint8_t ccm_flag = 0;
	uint32_t *tmp_salt;
	uint64_t *tmp_key;
	int i, length = 0;

	/* Set direction */
	if (ipsec_xfrm->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		w2->s.dir = ROC_IE_SA_DIR_OUTBOUND;
	else
		w2->s.dir = ROC_IE_SA_DIR_INBOUND;

	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xfrm = crypto_xfrm;
		cipher_xfrm = crypto_xfrm->next;
	} else {
		cipher_xfrm = crypto_xfrm;
		auth_xfrm = crypto_xfrm->next;
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
			w2->s.enc_type = ROC_IE_SA_ENC_AES_GCM;
			w2->s.auth_type = ROC_IE_SA_AUTH_NULL;
			memcpy(salt_key, &ipsec_xfrm->salt, 4);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		case RTE_CRYPTO_AEAD_AES_CCM:
			w2->s.enc_type = ROC_IE_SA_ENC_AES_CCM;
			w2->s.auth_type = ROC_IE_SA_AUTH_NULL;
			ccm_flag = 0x07 & ~ROC_CPT_AES_CCM_CTR_LEN;
			*salt_key = ccm_flag;
			memcpy(PLT_PTR_ADD(salt_key, 1), &ipsec_xfrm->salt, 3);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		default:
			return -ENOTSUP;
		}
	} else {
		if (cipher_xfrm != NULL) {
			switch (cipher_xfrm->cipher.algo) {
			case RTE_CRYPTO_CIPHER_NULL:
				w2->s.enc_type = ROC_IE_SA_ENC_NULL;
				break;
			case RTE_CRYPTO_CIPHER_AES_CBC:
				w2->s.enc_type = ROC_IE_SA_ENC_AES_CBC;
				break;
			case RTE_CRYPTO_CIPHER_AES_CTR:
				w2->s.enc_type = ROC_IE_SA_ENC_AES_CTR;
				memcpy(salt_key, &ipsec_xfrm->salt, 4);
				tmp_salt = (uint32_t *)salt_key;
				*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
				break;
			case RTE_CRYPTO_CIPHER_3DES_CBC:
				w2->s.enc_type = ROC_IE_SA_ENC_3DES_CBC;
				break;
			default:
				return -ENOTSUP;
			}

			key = cipher_xfrm->cipher.key.data;
			length = cipher_xfrm->cipher.key.length;
		}

		switch (auth_xfrm->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			if (w2->s.dir == ROC_IE_SA_DIR_INBOUND && ipsec_xfrm->replay_win_sz) {
				plt_err("anti-replay can't be supported with integrity service disabled");
				return -EINVAL;
			}
			w2->s.auth_type = ROC_IE_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_AES_XCBC_128;
			break;
		case RTE_CRYPTO_AUTH_AES_GMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_AES_GMAC;
			key = auth_xfrm->auth.key.data;
			length = auth_xfrm->auth.key.length;
			memcpy(salt_key, &ipsec_xfrm->salt, 4);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		default:
			return -ENOTSUP;
		}

		if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
			const uint8_t *auth_key = auth_xfrm->auth.key.data;
			roc_aes_xcbc_key_derive(auth_key, hmac_opad_ipad);
		} else {
			roc_se_hmac_opad_ipad_gen(w2->s.auth_type, auth_xfrm->auth.key.data,
						  auth_xfrm->auth.key.length, &hmac_opad_ipad[0],
						  ROC_SE_IPSEC);
		}

		tmp_key = (uint64_t *)hmac_opad_ipad;
		for (i = 0;
		     i < (int)(ROC_CTX_MAX_OPAD_IPAD_LEN / sizeof(uint64_t));
		     i++)
			tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);

	}

	/* Set encapsulation type */
	if (ipsec_xfrm->options.udp_encap)
		w2->s.encap_type = ROC_IE_OT_SA_ENCAP_UDP;

	w2->s.spi = ipsec_xfrm->spi;

	if (key != NULL && length != 0) {
		/* Copy encryption key */
		memcpy(cipher_key, key, length);
		tmp_key = (uint64_t *)cipher_key;
		for (i = 0; i < (int)(ROC_CTX_MAX_CKEY_LEN / sizeof(uint64_t)); i++)
			tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);
	}

	/* Set AES key length */
	if (w2->s.enc_type == ROC_IE_SA_ENC_AES_CBC ||
	    w2->s.enc_type == ROC_IE_SA_ENC_AES_CTR ||
	    w2->s.enc_type == ROC_IE_SA_ENC_AES_GCM ||
	    w2->s.enc_type == ROC_IE_SA_ENC_AES_CCM ||
	    w2->s.auth_type == ROC_IE_SA_AUTH_AES_GMAC) {
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

static void
ot_ipsec_update_ipv6_addr_endianness(uint64_t *addr)
{
	*addr = rte_be_to_cpu_64(*addr);
	addr++;
	*addr = rte_be_to_cpu_64(*addr);
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
		       sizeof(sa->outer_hdr.ipv6.src_addr));
		memcpy(&sa->outer_hdr.ipv6.dst_addr, &tunnel->ipv6.dst_addr,
		       sizeof(sa->outer_hdr.ipv6.dst_addr));

		/* IP Source and Dest are in LE/CPU endian */
		ot_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.src_addr);
		ot_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.dst_addr);

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

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ot_ipsec_inb_sa_fill)
int
cnxk_ot_ipsec_inb_sa_fill(struct roc_ot_ipsec_inb_sa *sa,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen)
{
	uint16_t sport = 4500, dport = 4500;
	union roc_ot_ipsec_sa_word2 w2;
	uint32_t replay_win_sz;
	size_t offset;
	int rc;

	/* Initialize the SA */
	roc_ot_ipsec_inb_sa_init(sa);

	w2.u64 = sa->w2.u64;
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
		if (ipsec_xfrm->udp.sport)
			sport = ipsec_xfrm->udp.sport;

		if (ipsec_xfrm->udp.dport)
			dport = ipsec_xfrm->udp.dport;

		sa->w10.s.udp_src_port = sport;
		sa->w10.s.udp_dst_port = dport;
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

	if (sa->w0.s.ctx_size < ctx_ilen)
		sa->w0.s.ctx_size = ctx_ilen;

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

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ot_ipsec_outb_sa_fill)
int
cnxk_ot_ipsec_outb_sa_fill(struct roc_ot_ipsec_outb_sa *sa,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen)
{
	struct rte_security_ipsec_tunnel_param *tunnel = &ipsec_xfrm->tunnel;
	uint16_t sport = 4500, dport = 4500;
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
		       sizeof(sa->outer_hdr.ipv6.src_addr));
		memcpy(&sa->outer_hdr.ipv6.dst_addr, &tunnel->ipv6.dst_addr,
		       sizeof(sa->outer_hdr.ipv6.dst_addr));

		/* IP Source and Dest are in LE/CPU endian */
		ot_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.src_addr);
		ot_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.dst_addr);

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
		if (ipsec_xfrm->udp.sport)
			sport = ipsec_xfrm->udp.sport;

		if (ipsec_xfrm->udp.dport)
			dport = ipsec_xfrm->udp.dport;

		sa->w10.s.udp_src_port = sport;
		sa->w10.s.udp_dst_port = dport;
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

	if (sa->w0.s.ctx_size < ctx_ilen)
		sa->w0.s.ctx_size = ctx_ilen;

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

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ot_ipsec_inb_sa_valid)
bool
cnxk_ot_ipsec_inb_sa_valid(struct roc_ot_ipsec_inb_sa *sa)
{
	return !!sa->w2.s.valid;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ot_ipsec_outb_sa_valid)
bool
cnxk_ot_ipsec_outb_sa_valid(struct roc_ot_ipsec_outb_sa *sa)
{
	return !!sa->w2.s.valid;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ipsec_ivlen_get)
uint8_t
cnxk_ipsec_ivlen_get(enum rte_crypto_cipher_algorithm c_algo,
		     enum rte_crypto_auth_algorithm a_algo,
		     enum rte_crypto_aead_algorithm aead_algo)
{
	uint8_t ivlen = 0;

	if ((aead_algo == RTE_CRYPTO_AEAD_AES_GCM) || (aead_algo == RTE_CRYPTO_AEAD_AES_CCM))
		ivlen = 8;

	switch (c_algo) {
	case RTE_CRYPTO_CIPHER_AES_CTR:
		ivlen = 8;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
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

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ipsec_icvlen_get)
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
	case RTE_CRYPTO_AUTH_MD5_HMAC:
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
	case RTE_CRYPTO_AEAD_AES_CCM:
		icv = 16;
		break;
	default:
		break;
	}

	return icv;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ipsec_outb_roundup_byte)
uint8_t
cnxk_ipsec_outb_roundup_byte(enum rte_crypto_cipher_algorithm c_algo,
			     enum rte_crypto_aead_algorithm aead_algo)
{
	uint8_t roundup_byte = 4;

	if ((aead_algo == RTE_CRYPTO_AEAD_AES_GCM) || (aead_algo == RTE_CRYPTO_AEAD_AES_CCM))
		return roundup_byte;

	switch (c_algo) {
	case RTE_CRYPTO_CIPHER_AES_CTR:
		roundup_byte = 4;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		roundup_byte = 16;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
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

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ipsec_outb_rlens_get)
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

static inline int
on_ipsec_sa_ctl_set(struct rte_security_ipsec_xform *ipsec,
		    struct rte_crypto_sym_xform *crypto_xform,
		    struct roc_ie_on_sa_ctl *ctl)
{
	struct rte_crypto_sym_xform *cipher_xform, *auth_xform;
	int aes_key_len = 0;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xform = crypto_xform;
		cipher_xform = crypto_xform->next;
	} else {
		cipher_xform = crypto_xform;
		auth_xform = crypto_xform->next;
	}

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		ctl->direction = ROC_IE_SA_DIR_OUTBOUND;
	else
		ctl->direction = ROC_IE_SA_DIR_INBOUND;

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4)
			ctl->outer_ip_ver = ROC_IE_SA_IP_VERSION_4;
		else if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV6)
			ctl->outer_ip_ver = ROC_IE_SA_IP_VERSION_6;
		else
			return -EINVAL;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT) {
		ctl->ipsec_mode = ROC_IE_SA_MODE_TRANSPORT;
		ctl->outer_ip_ver = ROC_IE_SA_IP_VERSION_4;
	} else if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
		ctl->ipsec_mode = ROC_IE_SA_MODE_TUNNEL;
	else
		return -EINVAL;

	if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_AH)
		ctl->ipsec_proto = ROC_IE_SA_PROTOCOL_AH;
	else if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP)
		ctl->ipsec_proto = ROC_IE_SA_PROTOCOL_ESP;
	else
		return -EINVAL;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		switch (crypto_xform->aead.algo) {
		case RTE_CRYPTO_AEAD_AES_GCM:
			ctl->enc_type = ROC_IE_SA_ENC_AES_GCM;
			aes_key_len = crypto_xform->aead.key.length;
			break;
		case RTE_CRYPTO_AEAD_AES_CCM:
			ctl->enc_type = ROC_IE_SA_ENC_AES_CCM;
			aes_key_len = crypto_xform->aead.key.length;
			break;
		default:
			plt_err("Unsupported AEAD algorithm");
			return -ENOTSUP;
		}
	} else {
		if (cipher_xform != NULL) {
			switch (cipher_xform->cipher.algo) {
			case RTE_CRYPTO_CIPHER_NULL:
				ctl->enc_type = ROC_IE_SA_ENC_NULL;
				break;
			case RTE_CRYPTO_CIPHER_DES_CBC:
				ctl->enc_type = ROC_IE_SA_ENC_DES_CBC;
				break;
			case RTE_CRYPTO_CIPHER_3DES_CBC:
				ctl->enc_type = ROC_IE_SA_ENC_3DES_CBC;
				break;
			case RTE_CRYPTO_CIPHER_AES_CBC:
				ctl->enc_type = ROC_IE_SA_ENC_AES_CBC;
				aes_key_len = cipher_xform->cipher.key.length;
				break;
			case RTE_CRYPTO_CIPHER_AES_CTR:
				ctl->enc_type = ROC_IE_SA_ENC_AES_CTR;
				aes_key_len = cipher_xform->cipher.key.length;
				break;
			default:
				plt_err("Unsupported cipher algorithm");
				return -ENOTSUP;
			}
		}

		switch (auth_xform->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			ctl->auth_type = ROC_IE_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_MD5_HMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_MD5;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA224_HMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_SHA2_224;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_GMAC:
			ctl->auth_type = ROC_IE_SA_AUTH_AES_GMAC;
			aes_key_len = auth_xform->auth.key.length;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			ctl->auth_type = ROC_IE_SA_AUTH_AES_XCBC_128;
			break;
		default:
			plt_err("Unsupported auth algorithm");
			return -ENOTSUP;
		}
	}

	/* Set AES key length */
	if (ctl->enc_type == ROC_IE_SA_ENC_AES_CBC ||
	    ctl->enc_type == ROC_IE_SA_ENC_AES_CTR ||
	    ctl->enc_type == ROC_IE_SA_ENC_AES_GCM ||
	    ctl->enc_type == ROC_IE_SA_ENC_AES_CCM ||
	    ctl->auth_type == ROC_IE_SA_AUTH_AES_GMAC) {
		switch (aes_key_len) {
		case 16:
			ctl->aes_key_len = ROC_IE_SA_AES_KEY_LEN_128;
			break;
		case 24:
			ctl->aes_key_len = ROC_IE_SA_AES_KEY_LEN_192;
			break;
		case 32:
			ctl->aes_key_len = ROC_IE_SA_AES_KEY_LEN_256;
			break;
		default:
			plt_err("Invalid AES key length");
			return -EINVAL;
		}
	}

	if (ipsec->options.esn)
		ctl->esn_en = 1;

	if (ipsec->options.udp_encap == 1)
		ctl->encap_type = ROC_IE_ON_SA_ENCAP_UDP;

	ctl->copy_df = ipsec->options.copy_df;

	ctl->spi = rte_cpu_to_be_32(ipsec->spi);

	rte_io_wmb();

	ctl->valid = 1;

	return 0;
}

static inline int
on_fill_ipsec_common_sa(struct rte_security_ipsec_xform *ipsec,
			struct rte_crypto_sym_xform *crypto_xform,
			struct roc_ie_on_common_sa *common_sa)
{
	struct rte_crypto_sym_xform *cipher_xform, *auth_xform;
	const uint8_t *cipher_key;
	int cipher_key_len = 0;
	uint8_t ccm_flag = 0;
	int ret;

	ret = on_ipsec_sa_ctl_set(ipsec, crypto_xform, &common_sa->ctl);
	if (ret)
		return ret;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xform = crypto_xform;
		cipher_xform = crypto_xform->next;
	} else {
		cipher_xform = crypto_xform;
		auth_xform = crypto_xform->next;
	}

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM)
			memcpy(common_sa->iv.gcm.nonce, &ipsec->salt, 4);
		else if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM) {
			ccm_flag = 0x07 & ~ROC_CPT_AES_CCM_CTR_LEN;
			*common_sa->iv.gcm.nonce = ccm_flag;
			memcpy(PLT_PTR_ADD(common_sa->iv.gcm.nonce, 1), &ipsec->salt, 3);
		}
		cipher_key = crypto_xform->aead.key.data;
		cipher_key_len = crypto_xform->aead.key.length;
	} else {
		if (cipher_xform) {
			if (cipher_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CTR)
				memcpy(common_sa->iv.gcm.nonce, &ipsec->salt, 4);
			cipher_key = cipher_xform->cipher.key.data;
			cipher_key_len = cipher_xform->cipher.key.length;
		}

		if (auth_xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
			memcpy(common_sa->iv.gcm.nonce, &ipsec->salt, 4);
			cipher_key = auth_xform->auth.key.data;
			cipher_key_len = auth_xform->auth.key.length;
		}
	}

	if (cipher_key_len != 0)
		memcpy(common_sa->cipher_key, cipher_key, cipher_key_len);

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_on_ipsec_outb_sa_create)
int
cnxk_on_ipsec_outb_sa_create(struct rte_security_ipsec_xform *ipsec,
			     struct rte_crypto_sym_xform *crypto_xform,
			     struct roc_ie_on_outb_sa *out_sa)
{
	struct roc_ie_on_ip_template *template = NULL;
	struct rte_crypto_sym_xform *auth_xform;
	struct roc_ie_on_sa_ctl *ctl;
	struct rte_ipv6_hdr *ip6;
	struct rte_ipv4_hdr *ip4;
	uint16_t sport, dport;
	size_t ctx_len;
	int ret;

	ctl = &out_sa->common_sa.ctl;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
		auth_xform = crypto_xform;
	else
		auth_xform = crypto_xform->next;

	ret = on_fill_ipsec_common_sa(ipsec, crypto_xform, &out_sa->common_sa);
	if (ret)
		return ret;

	if (ctl->enc_type == ROC_IE_SA_ENC_AES_GCM || ctl->enc_type == ROC_IE_SA_ENC_AES_CCM ||
	    ctl->auth_type == ROC_IE_SA_AUTH_NULL || ctl->auth_type == ROC_IE_SA_AUTH_AES_GMAC) {
		template = &out_sa->aes_gcm.template;
		ctx_len = offsetof(struct roc_ie_on_outb_sa, aes_gcm.template);
	} else {
		switch (ctl->auth_type) {
		case ROC_IE_SA_AUTH_MD5:
		case ROC_IE_SA_AUTH_SHA1:
			template = &out_sa->sha1.template;
			ctx_len = offsetof(struct roc_ie_on_outb_sa, sha1.template);
			break;
		case ROC_IE_SA_AUTH_SHA2_256:
		case ROC_IE_SA_AUTH_SHA2_384:
		case ROC_IE_SA_AUTH_SHA2_512:
			template = &out_sa->sha2.template;
			ctx_len = offsetof(struct roc_ie_on_outb_sa, sha2.template);
			break;
		case ROC_IE_SA_AUTH_AES_XCBC_128:
			template = &out_sa->aes_xcbc.template;
			ctx_len = offsetof(struct roc_ie_on_outb_sa, aes_xcbc.template);
			break;
		default:
			plt_err("Unsupported auth algorithm");
			return -EINVAL;
		}
	}

	ip4 = (struct rte_ipv4_hdr *)&template->ip4.ipv4_hdr;

	sport = 4500;
	dport = 4500;

	/* If custom port values are provided, Overwrite default port values. */
	if (ipsec->options.udp_encap) {

		if (ipsec->udp.sport)
			sport = ipsec->udp.sport;

		if (ipsec->udp.dport)
			dport = ipsec->udp.dport;

		ip4->next_proto_id = IPPROTO_UDP;
		template->ip4.udp_src = rte_be_to_cpu_16(sport);
		template->ip4.udp_dst = rte_be_to_cpu_16(dport);
	} else {
		if (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_AH)
			ip4->next_proto_id = IPPROTO_AH;
		else
			ip4->next_proto_id = IPPROTO_ESP;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			uint16_t frag_off = 0;

			ctx_len += sizeof(template->ip4);

			ip4->version_ihl = RTE_IPV4_VHL_DEF;
			ip4->time_to_live = ipsec->tunnel.ipv4.ttl ?
						    ipsec->tunnel.ipv4.ttl :
						    0x40;
			ip4->type_of_service |= (ipsec->tunnel.ipv4.dscp << 2);
			if (ipsec->tunnel.ipv4.df)
				frag_off |= RTE_IPV4_HDR_DF_FLAG;
			ip4->fragment_offset = rte_cpu_to_be_16(frag_off);

			memcpy(&ip4->src_addr, &ipsec->tunnel.ipv4.src_ip,
			       sizeof(struct in_addr));
			memcpy(&ip4->dst_addr, &ipsec->tunnel.ipv4.dst_ip,
			       sizeof(struct in_addr));
		} else if (ipsec->tunnel.type ==
			   RTE_SECURITY_IPSEC_TUNNEL_IPV6) {
			ctx_len += sizeof(template->ip6);

			ip6 = (struct rte_ipv6_hdr *)&template->ip6.ipv6_hdr;
			if (ipsec->options.udp_encap) {
				ip6->proto = IPPROTO_UDP;
				template->ip6.udp_src = rte_be_to_cpu_16(sport);
				template->ip6.udp_dst = rte_be_to_cpu_16(dport);
			} else {
				ip6->proto = (ipsec->proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP) ?
							      IPPROTO_ESP :
							      IPPROTO_AH;
			}
			ip6->vtc_flow =
				rte_cpu_to_be_32(0x60000000 |
						 ((ipsec->tunnel.ipv6.dscp
						   << RTE_IPV6_HDR_TC_SHIFT) &
						  RTE_IPV6_HDR_TC_MASK) |
						 ((ipsec->tunnel.ipv6.flabel
						   << RTE_IPV6_HDR_FL_SHIFT) &
						  RTE_IPV6_HDR_FL_MASK));
			ip6->hop_limits = ipsec->tunnel.ipv6.hlimit ?
						  ipsec->tunnel.ipv6.hlimit :
						  0x40;
			ip6->src_addr = ipsec->tunnel.ipv6.src_addr;
			ip6->dst_addr = ipsec->tunnel.ipv6.dst_addr;
		}
	} else
		ctx_len += sizeof(template->ip4);

	ctx_len = RTE_ALIGN_CEIL(ctx_len, 8);

	if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		uint8_t *hmac_opad_ipad = (uint8_t *)&out_sa->sha2;

		if (auth_xform->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
			const uint8_t *auth_key = auth_xform->auth.key.data;

			roc_aes_xcbc_key_derive(auth_key, hmac_opad_ipad);
		} else if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_NULL) {
			roc_se_hmac_opad_ipad_gen(
				out_sa->common_sa.ctl.auth_type, auth_xform->auth.key.data,
				auth_xform->auth.key.length, &hmac_opad_ipad[0], ROC_SE_IPSEC);
		}
	}

	return ctx_len;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_on_ipsec_inb_sa_create)
int
cnxk_on_ipsec_inb_sa_create(struct rte_security_ipsec_xform *ipsec,
			    struct rte_crypto_sym_xform *crypto_xform,
			    struct roc_ie_on_inb_sa *in_sa)
{
	struct rte_crypto_sym_xform *auth_xform = crypto_xform;
	const uint8_t *auth_key;
	int auth_key_len = 0;
	size_t ctx_len = 0;
	int ret;

	ret = on_fill_ipsec_common_sa(ipsec, crypto_xform, &in_sa->common_sa);
	if (ret)
		return ret;

	if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AEAD &&
	    crypto_xform->auth.algo == RTE_CRYPTO_AUTH_NULL && ipsec->replay_win_sz) {
		plt_err("anti-replay can't be supported with integrity service disabled");
		return -EINVAL;
	}
	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD ||
	    auth_xform->auth.algo == RTE_CRYPTO_AUTH_NULL ||
	    auth_xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		ctx_len = offsetof(struct roc_ie_on_inb_sa, sha1_or_gcm.hmac_key[0]);
	} else {
		uint8_t *hmac_opad_ipad = (uint8_t *)&in_sa->sha2;
		auth_key = auth_xform->auth.key.data;
		auth_key_len = auth_xform->auth.key.length;

		switch (auth_xform->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			break;
		case RTE_CRYPTO_AUTH_MD5_HMAC:
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			memcpy(in_sa->sha1_or_gcm.hmac_key, auth_key,
			       auth_key_len);
			ctx_len = offsetof(struct roc_ie_on_inb_sa,
					   sha1_or_gcm.selector);
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			memcpy(in_sa->sha2.hmac_key, auth_key, auth_key_len);
			ctx_len = offsetof(struct roc_ie_on_inb_sa,
					   sha2.selector);
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			memcpy(in_sa->aes_xcbc.key, auth_key, auth_key_len);
			ctx_len = offsetof(struct roc_ie_on_inb_sa,
					   aes_xcbc.selector);
			break;
		default:
			plt_err("Unsupported auth algorithm %u", auth_xform->auth.algo);
			return -ENOTSUP;
		}
		if (auth_xform->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
			const uint8_t *auth_key = auth_xform->auth.key.data;

			roc_aes_xcbc_key_derive(auth_key, hmac_opad_ipad);
		} else if (auth_xform->auth.algo != RTE_CRYPTO_AUTH_NULL) {
			roc_se_hmac_opad_ipad_gen(
				in_sa->common_sa.ctl.auth_type, auth_xform->auth.key.data,
				auth_xform->auth.key.length, &hmac_opad_ipad[0], ROC_SE_IPSEC);
		}
	}

	return ctx_len;
}

static int
ow_ipsec_sa_common_param_fill(union roc_ow_ipsec_sa_word2 *w2, uint8_t *cipher_key,
			      uint8_t *salt_key, uint8_t *hmac_opad_ipad,
			      struct rte_security_ipsec_xform *ipsec_xfrm,
			      struct rte_crypto_sym_xform *crypto_xfrm)
{
	struct rte_crypto_sym_xform *auth_xfrm, *cipher_xfrm;
	const uint8_t *key = NULL;
	uint8_t ccm_flag = 0;
	uint32_t *tmp_salt;
	uint64_t *tmp_key;
	int i, length = 0;

	/* Set direction */
	if (ipsec_xfrm->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		w2->s.dir = ROC_IE_SA_DIR_OUTBOUND;
	else
		w2->s.dir = ROC_IE_SA_DIR_INBOUND;

	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xfrm = crypto_xfrm;
		cipher_xfrm = crypto_xfrm->next;
	} else {
		cipher_xfrm = crypto_xfrm;
		auth_xfrm = crypto_xfrm->next;
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
			w2->s.enc_type = ROC_IE_SA_ENC_AES_GCM;
			w2->s.auth_type = ROC_IE_SA_AUTH_NULL;
			memcpy(salt_key, &ipsec_xfrm->salt, 4);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		case RTE_CRYPTO_AEAD_AES_CCM:
			w2->s.enc_type = ROC_IE_SA_ENC_AES_CCM;
			w2->s.auth_type = ROC_IE_SA_AUTH_NULL;
			ccm_flag = 0x07 & ~ROC_CPT_AES_CCM_CTR_LEN;
			*salt_key = ccm_flag;
			memcpy(PLT_PTR_ADD(salt_key, 1), &ipsec_xfrm->salt, 3);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		default:
			return -ENOTSUP;
		}
	} else {
		if (cipher_xfrm != NULL) {
			switch (cipher_xfrm->cipher.algo) {
			case RTE_CRYPTO_CIPHER_NULL:
				w2->s.enc_type = ROC_IE_SA_ENC_NULL;
				break;
			case RTE_CRYPTO_CIPHER_AES_CBC:
				w2->s.enc_type = ROC_IE_SA_ENC_AES_CBC;
				break;
			case RTE_CRYPTO_CIPHER_AES_CTR:
				w2->s.enc_type = ROC_IE_SA_ENC_AES_CTR;
				memcpy(salt_key, &ipsec_xfrm->salt, 4);
				tmp_salt = (uint32_t *)salt_key;
				*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
				break;
			case RTE_CRYPTO_CIPHER_3DES_CBC:
				w2->s.enc_type = ROC_IE_SA_ENC_3DES_CBC;
				break;
			default:
				return -ENOTSUP;
			}

			key = cipher_xfrm->cipher.key.data;
			length = cipher_xfrm->cipher.key.length;
		}

		switch (auth_xfrm->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			if (w2->s.dir == ROC_IE_SA_DIR_INBOUND && ipsec_xfrm->replay_win_sz) {
				plt_err("anti-replay can't be supported with integrity service disabled");
				return -EINVAL;
			}
			w2->s.auth_type = ROC_IE_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_AES_XCBC_128;
			break;
		case RTE_CRYPTO_AUTH_AES_GMAC:
			w2->s.auth_type = ROC_IE_SA_AUTH_AES_GMAC;
			key = auth_xfrm->auth.key.data;
			length = auth_xfrm->auth.key.length;
			memcpy(salt_key, &ipsec_xfrm->salt, 4);
			tmp_salt = (uint32_t *)salt_key;
			*tmp_salt = rte_be_to_cpu_32(*tmp_salt);
			break;
		default:
			return -ENOTSUP;
		}

		if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_AES_XCBC_MAC) {
			const uint8_t *auth_key = auth_xfrm->auth.key.data;
			roc_aes_xcbc_key_derive(auth_key, hmac_opad_ipad);
		} else {
			roc_se_hmac_opad_ipad_gen(w2->s.auth_type, auth_xfrm->auth.key.data,
						  auth_xfrm->auth.key.length, &hmac_opad_ipad[0],
						  ROC_SE_IPSEC);
		}

		tmp_key = (uint64_t *)hmac_opad_ipad;
		for (i = 0; i < (int)(ROC_CTX_MAX_OPAD_IPAD_LEN / sizeof(uint64_t)); i++)
			tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);
	}

	/* Set encapsulation type */
	if (ipsec_xfrm->options.udp_encap)
		w2->s.encap_type = ROC_IE_OT_SA_ENCAP_UDP;

	w2->s.spi = ipsec_xfrm->spi;

	if (key != NULL && length != 0) {
		/* Copy encryption key */
		memcpy(cipher_key, key, length);
		tmp_key = (uint64_t *)cipher_key;
		for (i = 0; i < (int)(ROC_CTX_MAX_CKEY_LEN / sizeof(uint64_t)); i++)
			tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);
	}

	/* Set AES key length */
	if (w2->s.enc_type == ROC_IE_SA_ENC_AES_CBC || w2->s.enc_type == ROC_IE_SA_ENC_AES_CCM ||
	    w2->s.enc_type == ROC_IE_SA_ENC_AES_CTR || w2->s.enc_type == ROC_IE_SA_ENC_AES_GCM ||
	    w2->s.enc_type == ROC_IE_SA_ENC_AES_CCM || w2->s.auth_type == ROC_IE_SA_AUTH_AES_GMAC) {
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

	if (ipsec_xfrm->life.packets_soft_limit != 0 || ipsec_xfrm->life.packets_hard_limit != 0) {
		if (ipsec_xfrm->life.bytes_soft_limit != 0 ||
		    ipsec_xfrm->life.bytes_hard_limit != 0) {
			plt_err("Expiry tracking with both packets & bytes is not supported");
			return -EINVAL;
		}
		w2->s.life_unit = ROC_IE_OT_SA_LIFE_UNIT_PKTS;
	}

	if (ipsec_xfrm->life.bytes_soft_limit != 0 || ipsec_xfrm->life.bytes_hard_limit != 0) {
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
ow_ipsec_inb_ctx_size(struct roc_ow_ipsec_inb_sa *sa)
{
	size_t size;

	/* Variable based on Anti-replay Window */
	size = offsetof(struct roc_ow_ipsec_inb_sa, ctx) +
	       offsetof(struct roc_ow_ipsec_inb_ctx_update_reg, ar_winbits);

	if (sa->w0.s.ar_win)
		size += (1 << (sa->w0.s.ar_win - 1)) * sizeof(uint64_t);

	return size;
}

static void
ow_ipsec_update_ipv6_addr_endianness(uint64_t *addr)
{
	*addr = rte_be_to_cpu_64(*addr);
	addr++;
	*addr = rte_be_to_cpu_64(*addr);
}

static int
ow_ipsec_inb_tunnel_hdr_fill(struct roc_ow_ipsec_inb_sa *sa,
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
		memcpy(&sa->outer_hdr.ipv4.src_addr, &tunnel->ipv4.src_ip, sizeof(struct in_addr));
		memcpy(&sa->outer_hdr.ipv4.dst_addr, &tunnel->ipv4.dst_ip, sizeof(struct in_addr));

		/* IP Source and Dest are in LE/CPU endian */
		sa->outer_hdr.ipv4.src_addr = rte_be_to_cpu_32(sa->outer_hdr.ipv4.src_addr);
		sa->outer_hdr.ipv4.dst_addr = rte_be_to_cpu_32(sa->outer_hdr.ipv4.dst_addr);

		break;
	case RTE_SECURITY_IPSEC_TUNNEL_IPV6:
		sa->w2.s.outer_ip_ver = ROC_IE_SA_IP_VERSION_6;
		memcpy(&sa->outer_hdr.ipv6.src_addr, &tunnel->ipv6.src_addr,
		       sizeof(struct in6_addr));
		memcpy(&sa->outer_hdr.ipv6.dst_addr, &tunnel->ipv6.dst_addr,
		       sizeof(struct in6_addr));

		/* IP Source and Dest are in LE/CPU endian */
		ow_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.src_addr);
		ow_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.dst_addr);

		break;
	default:
		return -EINVAL;
	}

	switch (ipsec_xfrm->options.tunnel_hdr_verify) {
	case RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR:
		sa->w2.s.ip_hdr_verify = ROC_IE_OT_SA_IP_HDR_VERIFY_DST_ADDR;
		break;
	case RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR:
		sa->w2.s.ip_hdr_verify = ROC_IE_OT_SA_IP_HDR_VERIFY_SRC_DST_ADDR;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ow_ipsec_inb_sa_fill)
int
cnxk_ow_ipsec_inb_sa_fill(struct roc_ow_ipsec_inb_sa *sa,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen)
{
	uint16_t sport = 4500, dport = 4500;
	union roc_ow_ipsec_sa_word2 w2;
	uint32_t replay_win_sz;
	size_t offset;
	int rc;

	/* Initialize the SA */
	roc_ow_ipsec_inb_sa_init(sa);

	w2.u64 = 0;
	rc = ow_ipsec_sa_common_param_fill(&w2, sa->cipher_key, sa->w8.s.salt, sa->hmac_opad_ipad,
					   ipsec_xfrm, crypto_xfrm);
	if (rc)
		return rc;

	/* Updata common word2 data */
	sa->w2.u64 = w2.u64;

	/* Only support power-of-two window sizes supported */
	replay_win_sz = ipsec_xfrm->replay_win_sz;
	if (replay_win_sz) {
		if (!rte_is_power_of_2(replay_win_sz) || replay_win_sz > ROC_AR_WIN_SIZE_MAX)
			return -ENOTSUP;

		sa->w0.s.ar_win = rte_log2_u32(replay_win_sz) - 5;
	}

	rc = ow_ipsec_inb_tunnel_hdr_fill(sa, ipsec_xfrm);
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
		if (ipsec_xfrm->udp.sport)
			sport = ipsec_xfrm->udp.sport;

		if (ipsec_xfrm->udp.dport)
			dport = ipsec_xfrm->udp.dport;

		sa->w10.s.udp_src_port = sport;
		sa->w10.s.udp_dst_port = dport;
	}

	if (ipsec_xfrm->options.udp_ports_verify)
		sa->w2.s.udp_ports_verify = 1;

	offset = offsetof(struct roc_ow_ipsec_inb_sa, ctx);
	/* Word offset for HW managed SA field */
	sa->w0.s.hw_ctx_off = offset / 8;
	/* Context push size for inbound spans up to hw_ctx including
	 * ar_base field, in 8b units
	 */
	sa->w0.s.ctx_push_size = sa->w0.s.hw_ctx_off + 1;
	/* Entire context size in 128B units */
	sa->w0.s.ctx_size =
		(PLT_ALIGN_CEIL(ow_ipsec_inb_ctx_size(sa), ROC_CTX_UNIT_128B) / ROC_CTX_UNIT_128B) -
		1;

	if (sa->w0.s.ctx_size < ctx_ilen)
		sa->w0.s.ctx_size = ctx_ilen;

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

RTE_EXPORT_INTERNAL_SYMBOL(cnxk_ow_ipsec_outb_sa_fill)
int
cnxk_ow_ipsec_outb_sa_fill(struct roc_ow_ipsec_outb_sa *sa,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen)
{
	struct rte_security_ipsec_tunnel_param *tunnel = &ipsec_xfrm->tunnel;
	uint16_t sport = 4500, dport = 4500;
	union roc_ow_ipsec_sa_word2 w2;
	size_t offset;
	int rc;

	/* Initialize the SA */
	roc_ow_ipsec_outb_sa_init(sa);

	w2.u64 = 0;
	rc = ow_ipsec_sa_common_param_fill(&w2, sa->cipher_key, sa->iv.s.salt, sa->hmac_opad_ipad,
					   ipsec_xfrm, crypto_xfrm);
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
		memcpy(&sa->outer_hdr.ipv4.src_addr, &tunnel->ipv4.src_ip, sizeof(struct in_addr));
		memcpy(&sa->outer_hdr.ipv4.dst_addr, &tunnel->ipv4.dst_ip, sizeof(struct in_addr));

		/* IP Source and Dest seems to be in LE/CPU endian */
		sa->outer_hdr.ipv4.src_addr = rte_be_to_cpu_32(sa->outer_hdr.ipv4.src_addr);
		sa->outer_hdr.ipv4.dst_addr = rte_be_to_cpu_32(sa->outer_hdr.ipv4.dst_addr);

		/* Outer header DF bit source */
		if (!ipsec_xfrm->options.copy_df) {
			sa->w2.s.ipv4_df_src_or_ipv6_flw_lbl_src = ROC_IE_OT_SA_COPY_FROM_SA;
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

		/* IP Source and Dest are in LE/CPU endian */
		ow_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.src_addr);
		ow_ipsec_update_ipv6_addr_endianness((uint64_t *)&sa->outer_hdr.ipv6.dst_addr);

		/* Outer header flow label source */
		if (!ipsec_xfrm->options.copy_flabel) {
			sa->w2.s.ipv4_df_src_or_ipv6_flw_lbl_src = ROC_IE_OT_SA_COPY_FROM_SA;

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
		if (ipsec_xfrm->udp.sport)
			sport = ipsec_xfrm->udp.sport;

		if (ipsec_xfrm->udp.dport)
			dport = ipsec_xfrm->udp.dport;

		sa->w10.s.udp_src_port = sport;
		sa->w10.s.udp_dst_port = dport;
	}

	offset = offsetof(struct roc_ow_ipsec_outb_sa, ctx);
	/* Word offset for HW managed SA field */
	sa->w0.s.hw_ctx_off = offset / 8;

	/* Context push size is up to err ctl in HW ctx */
	sa->w0.s.ctx_push_size = sa->w0.s.hw_ctx_off + 1;

	/* Entire context size in 128B units */
	offset = sizeof(struct roc_ow_ipsec_outb_sa);
	sa->w0.s.ctx_size = (PLT_ALIGN_CEIL(offset, ROC_CTX_UNIT_128B) / ROC_CTX_UNIT_128B) - 1;

	/* IPID gen */
	sa->w2.s.ipid_gen = 1;

	if (sa->w0.s.ctx_size < ctx_ilen)
		sa->w0.s.ctx_size = ctx_ilen;

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
