/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_ip.h>
#include <rte_security.h>
#include <rte_security_driver.h>

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_ipsec.h"
#include "cnxk_security.h"
#include "cn9k_ipsec.h"

#include "roc_api.h"

static inline int
cn9k_cpt_enq_sa_write(struct cn9k_ipsec_sa *sa, struct cnxk_cpt_qp *qp,
		      uint8_t opcode, size_t ctx_len)
{
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	uint64_t lmtline = qp->lmtline.lmt_base;
	uint64_t io_addr = qp->lmtline.io_addr;
	uint64_t lmt_status, time_out;
	struct cpt_cn9k_res_s *res;
	struct cpt_inst_s inst;
	uint64_t *mdata;
	int ret = 0;

	if (unlikely(rte_mempool_get(qp->meta_info.pool, (void **)&mdata) < 0))
		return -ENOMEM;

	res = (struct cpt_cn9k_res_s *)RTE_PTR_ALIGN(mdata, 16);
	res->compcode = CPT_COMP_NOT_DONE;

	inst.w4.s.opcode_major = opcode;
	inst.w4.s.opcode_minor = ctx_len >> 3;
	inst.w4.s.param1 = 0;
	inst.w4.s.param2 = 0;
	inst.w4.s.dlen = ctx_len;
	inst.dptr = rte_mempool_virt2iova(sa);
	inst.rptr = 0;
	inst.w7.s.cptr = rte_mempool_virt2iova(sa);
	inst.w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];

	inst.w0.u64 = 0;
	inst.w2.u64 = 0;
	inst.w3.u64 = 0;
	inst.res_addr = rte_mempool_virt2iova(res);

	rte_io_wmb();

	do {
		/* Copy CPT command to LMTLINE */
		roc_lmt_mov((void *)lmtline, &inst, 2);
		lmt_status = roc_lmt_submit_ldeor(io_addr);
	} while (lmt_status == 0);

	time_out = rte_get_timer_cycles() +
		   DEFAULT_COMMAND_TIMEOUT * rte_get_timer_hz();

	while (res->compcode == CPT_COMP_NOT_DONE) {
		if (rte_get_timer_cycles() > time_out) {
			rte_mempool_put(qp->meta_info.pool, mdata);
			plt_err("Request timed out");
			return -ETIMEDOUT;
		}
		rte_io_rmb();
	}

	if (unlikely(res->compcode != CPT_COMP_GOOD)) {
		ret = res->compcode;
		switch (ret) {
		case CPT_COMP_INSTERR:
			plt_err("Request failed with instruction error");
			break;
		case CPT_COMP_FAULT:
			plt_err("Request failed with DMA fault");
			break;
		case CPT_COMP_HWERR:
			plt_err("Request failed with hardware error");
			break;
		default:
			plt_err("Request failed with unknown hardware "
				"completion code : 0x%x",
				ret);
		}
		ret = -EINVAL;
		goto mempool_put;
	}

	if (unlikely(res->uc_compcode != ROC_IE_ON_UCC_SUCCESS)) {
		ret = res->uc_compcode;
		switch (ret) {
		case ROC_IE_ON_AUTH_UNSUPPORTED:
			plt_err("Invalid auth type");
			break;
		case ROC_IE_ON_ENCRYPT_UNSUPPORTED:
			plt_err("Invalid encrypt type");
			break;
		default:
			plt_err("Request failed with unknown microcode "
				"completion code : 0x%x",
				ret);
		}
		ret = -ENOTSUP;
	}

mempool_put:
	rte_mempool_put(qp->meta_info.pool, mdata);
	return ret;
}

static inline int
ipsec_sa_ctl_set(struct rte_security_ipsec_xform *ipsec,
		 struct rte_crypto_sym_xform *crypto_xform,
		 struct roc_ie_on_sa_ctl *ctl)
{
	struct rte_crypto_sym_xform *cipher_xform, *auth_xform;
	int aes_key_len;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		ctl->direction = ROC_IE_SA_DIR_OUTBOUND;
		cipher_xform = crypto_xform;
		auth_xform = crypto_xform->next;
	} else if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		ctl->direction = ROC_IE_SA_DIR_INBOUND;
		auth_xform = crypto_xform;
		cipher_xform = crypto_xform->next;
	} else {
		return -EINVAL;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4)
			ctl->outer_ip_ver = ROC_IE_SA_IP_VERSION_4;
		else if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV6)
			ctl->outer_ip_ver = ROC_IE_SA_IP_VERSION_6;
		else
			return -EINVAL;
	}

	ctl->inner_ip_ver = ctl->outer_ip_ver;

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT)
		ctl->ipsec_mode = ROC_IE_SA_MODE_TRANSPORT;
	else if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
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
		if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
			ctl->enc_type = ROC_IE_ON_SA_ENC_AES_GCM;
			aes_key_len = crypto_xform->aead.key.length;
		} else {
			return -ENOTSUP;
		}
	} else if (cipher_xform->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
		ctl->enc_type = ROC_IE_ON_SA_ENC_AES_CBC;
		aes_key_len = cipher_xform->cipher.key.length;
	} else {
		return -ENOTSUP;
	}

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
		return -EINVAL;
	}

	if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		switch (auth_xform->auth.algo) {
		case RTE_CRYPTO_AUTH_NULL:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_NULL;
			break;
		case RTE_CRYPTO_AUTH_MD5_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_MD5;
			break;
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_SHA1;
			break;
		case RTE_CRYPTO_AUTH_SHA224_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_SHA2_224;
			break;
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_SHA2_256;
			break;
		case RTE_CRYPTO_AUTH_SHA384_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_SHA2_384;
			break;
		case RTE_CRYPTO_AUTH_SHA512_HMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_SHA2_512;
			break;
		case RTE_CRYPTO_AUTH_AES_GMAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_AES_GMAC;
			break;
		case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
			ctl->auth_type = ROC_IE_ON_SA_AUTH_AES_XCBC_128;
			break;
		default:
			return -ENOTSUP;
		}
	}

	if (ipsec->options.esn)
		ctl->esn_en = 1;

	if (ipsec->options.udp_encap == 1)
		ctl->encap_type = ROC_IE_ON_SA_ENCAP_UDP;

	ctl->spi = rte_cpu_to_be_32(ipsec->spi);

	rte_io_wmb();

	ctl->valid = 1;

	return 0;
}

static inline int
fill_ipsec_common_sa(struct rte_security_ipsec_xform *ipsec,
		     struct rte_crypto_sym_xform *crypto_xform,
		     struct roc_ie_on_common_sa *common_sa)
{
	struct rte_crypto_sym_xform *cipher_xform;
	const uint8_t *cipher_key;
	int cipher_key_len = 0;
	int ret;

	if (ipsec->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		cipher_xform = crypto_xform->next;
	else
		cipher_xform = crypto_xform;

	ret = ipsec_sa_ctl_set(ipsec, crypto_xform, &common_sa->ctl);
	if (ret)
		return ret;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM)
			memcpy(common_sa->iv.gcm.nonce, &ipsec->salt, 4);
		cipher_key = crypto_xform->aead.key.data;
		cipher_key_len = crypto_xform->aead.key.length;
	} else {
		cipher_key = cipher_xform->cipher.key.data;
		cipher_key_len = cipher_xform->cipher.key.length;
	}

	if (cipher_key_len != 0)
		memcpy(common_sa->cipher_key, cipher_key, cipher_key_len);
	else
		return -EINVAL;

	return 0;
}

static int
cn9k_ipsec_outb_sa_create(struct cnxk_cpt_qp *qp,
			  struct rte_security_ipsec_xform *ipsec,
			  struct rte_crypto_sym_xform *crypto_xform,
			  struct rte_security_session *sec_sess)
{
	struct rte_crypto_sym_xform *auth_xform = crypto_xform->next;
	struct roc_ie_on_ip_template *template = NULL;
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	struct cnxk_cpt_inst_tmpl *inst_tmpl;
	struct roc_ie_on_outb_sa *out_sa;
	struct cn9k_sec_session *sess;
	struct roc_ie_on_sa_ctl *ctl;
	struct cn9k_ipsec_sa *sa;
	struct rte_ipv6_hdr *ip6;
	struct rte_ipv4_hdr *ip4;
	const uint8_t *auth_key;
	union cpt_inst_w4 w4;
	union cpt_inst_w7 w7;
	int auth_key_len = 0;
	size_t ctx_len;
	int ret;

	sess = get_sec_session_private_data(sec_sess);
	sa = &sess->sa;
	out_sa = &sa->out_sa;
	ctl = &out_sa->common_sa.ctl;

	memset(sa, 0, sizeof(struct cn9k_ipsec_sa));

	/* Initialize lookaside IPsec private data */
	sa->dir = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	/* Start ip id from 1 */
	sa->ip_id = 1;
	sa->seq_lo = 1;
	sa->seq_hi = 0;

	ret = fill_ipsec_common_sa(ipsec, crypto_xform, &out_sa->common_sa);
	if (ret)
		return ret;

	ret = cnxk_ipsec_outb_rlens_get(&sa->rlens, ipsec, crypto_xform);
	if (ret)
		return ret;

	if (ctl->enc_type == ROC_IE_ON_SA_ENC_AES_GCM ||
	    ctl->auth_type == ROC_IE_ON_SA_AUTH_NULL) {
		template = &out_sa->aes_gcm.template;
		ctx_len = offsetof(struct roc_ie_on_outb_sa, aes_gcm.template);
	} else if (ctl->auth_type == ROC_IE_ON_SA_AUTH_SHA1) {
		template = &out_sa->sha1.template;
		ctx_len = offsetof(struct roc_ie_on_outb_sa, sha1.template);
	} else if (ctl->auth_type == ROC_IE_ON_SA_AUTH_SHA2_256) {
		template = &out_sa->sha2.template;
		ctx_len = offsetof(struct roc_ie_on_outb_sa, sha2.template);
	} else {
		return -EINVAL;
	}

	ip4 = (struct rte_ipv4_hdr *)&template->ip4.ipv4_hdr;
	if (ipsec->options.udp_encap) {
		ip4->next_proto_id = IPPROTO_UDP;
		template->ip4.udp_src = rte_be_to_cpu_16(4500);
		template->ip4.udp_dst = rte_be_to_cpu_16(4500);
	} else {
		ip4->next_proto_id = IPPROTO_ESP;
	}

	if (ipsec->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (ipsec->tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			ctx_len += sizeof(template->ip4);

			ip4->version_ihl = RTE_IPV4_VHL_DEF;
			ip4->time_to_live = ipsec->tunnel.ipv4.ttl;
			ip4->type_of_service |= (ipsec->tunnel.ipv4.dscp << 2);
			if (ipsec->tunnel.ipv4.df)
				ip4->fragment_offset = BIT(14);
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
				template->ip6.udp_src = rte_be_to_cpu_16(4500);
				template->ip6.udp_dst = rte_be_to_cpu_16(4500);
			} else {
				ip6->proto = (ipsec->proto ==
					      RTE_SECURITY_IPSEC_SA_PROTO_ESP) ?
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
			ip6->hop_limits = ipsec->tunnel.ipv6.hlimit;
			memcpy(&ip6->src_addr, &ipsec->tunnel.ipv6.src_addr,
			       sizeof(struct in6_addr));
			memcpy(&ip6->dst_addr, &ipsec->tunnel.ipv6.dst_addr,
			       sizeof(struct in6_addr));
		}
	} else
		ctx_len += sizeof(template->ip4);

	ctx_len += RTE_ALIGN_CEIL(ctx_len, 8);

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		sa->cipher_iv_off = crypto_xform->aead.iv.offset;
		sa->cipher_iv_len = crypto_xform->aead.iv.length;
	} else {
		sa->cipher_iv_off = crypto_xform->cipher.iv.offset;
		sa->cipher_iv_len = crypto_xform->cipher.iv.length;

		auth_key = auth_xform->auth.key.data;
		auth_key_len = auth_xform->auth.key.length;

		if (auth_xform->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC)
			memcpy(out_sa->sha1.hmac_key, auth_key, auth_key_len);
		else if (auth_xform->auth.algo == RTE_CRYPTO_AUTH_SHA256_HMAC)
			memcpy(out_sa->sha2.hmac_key, auth_key, auth_key_len);
	}

	inst_tmpl = &sa->inst;

	w4.u64 = 0;
	w4.s.opcode_major = ROC_IE_ON_MAJOR_OP_PROCESS_OUTBOUND_IPSEC;
	w4.s.opcode_minor = ctx_len >> 3;
	w4.s.param1 = BIT(9);
	w4.s.param1 |= ROC_IE_ON_PER_PKT_IV;
	inst_tmpl->w4 = w4.u64;

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];
	w7.s.cptr = rte_mempool_virt2iova(out_sa);
	inst_tmpl->w7 = w7.u64;

	return cn9k_cpt_enq_sa_write(
		sa, qp, ROC_IE_ON_MAJOR_OP_WRITE_IPSEC_OUTBOUND, ctx_len);
}

static int
cn9k_ipsec_inb_sa_create(struct cnxk_cpt_qp *qp,
			 struct rte_security_ipsec_xform *ipsec,
			 struct rte_crypto_sym_xform *crypto_xform,
			 struct rte_security_session *sec_sess)
{
	struct rte_crypto_sym_xform *auth_xform = crypto_xform;
	struct roc_cpt *roc_cpt = qp->lf.roc_cpt;
	struct cnxk_cpt_inst_tmpl *inst_tmpl;
	struct roc_ie_on_inb_sa *in_sa;
	struct cn9k_sec_session *sess;
	struct cn9k_ipsec_sa *sa;
	const uint8_t *auth_key;
	union cpt_inst_w4 w4;
	union cpt_inst_w7 w7;
	int auth_key_len = 0;
	size_t ctx_len = 0;
	int ret;

	sess = get_sec_session_private_data(sec_sess);
	sa = &sess->sa;
	in_sa = &sa->in_sa;

	memset(sa, 0, sizeof(struct cn9k_ipsec_sa));

	sa->dir = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	sa->replay_win_sz = ipsec->replay_win_sz;

	ret = fill_ipsec_common_sa(ipsec, crypto_xform, &in_sa->common_sa);
	if (ret)
		return ret;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD ||
	    auth_xform->auth.algo == RTE_CRYPTO_AUTH_NULL) {
		ctx_len = offsetof(struct roc_ie_on_inb_sa,
				   sha1_or_gcm.hmac_key[0]);
	} else {
		auth_key = auth_xform->auth.key.data;
		auth_key_len = auth_xform->auth.key.length;

		if (auth_xform->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC) {
			memcpy(in_sa->sha1_or_gcm.hmac_key, auth_key,
			       auth_key_len);
			ctx_len = offsetof(struct roc_ie_on_inb_sa,
					   sha1_or_gcm.selector);
		} else if (auth_xform->auth.algo ==
			   RTE_CRYPTO_AUTH_SHA256_HMAC) {
			memcpy(in_sa->sha2.hmac_key, auth_key, auth_key_len);
			ctx_len = offsetof(struct roc_ie_on_inb_sa,
					   sha2.selector);
		}
	}

	inst_tmpl = &sa->inst;

	w4.u64 = 0;
	w4.s.opcode_major = ROC_IE_ON_MAJOR_OP_PROCESS_INBOUND_IPSEC;
	w4.s.opcode_minor = ctx_len >> 3;
	w4.s.param2 = BIT(12);
	inst_tmpl->w4 = w4.u64;

	w7.u64 = 0;
	w7.s.egrp = roc_cpt->eng_grp[CPT_ENG_TYPE_IE];
	w7.s.cptr = rte_mempool_virt2iova(in_sa);
	inst_tmpl->w7 = w7.u64;

	if (sa->replay_win_sz) {
		if (sa->replay_win_sz > CNXK_ON_AR_WIN_SIZE_MAX) {
			plt_err("Replay window size:%u is not supported",
				sa->replay_win_sz);
			return -ENOTSUP;
		}

		/* Set window bottom to 1, base and top to size of window */
		sa->ar.winb = 1;
		sa->ar.wint = sa->replay_win_sz;
		sa->ar.base = sa->replay_win_sz;

		in_sa->common_sa.esn_low = 0;
		in_sa->common_sa.esn_hi = 0;
	}

	return cn9k_cpt_enq_sa_write(
		sa, qp, ROC_IE_ON_MAJOR_OP_WRITE_IPSEC_INBOUND, ctx_len);
}

static inline int
cn9k_ipsec_xform_verify(struct rte_security_ipsec_xform *ipsec)
{
	if (ipsec->life.bytes_hard_limit != 0 ||
	    ipsec->life.bytes_soft_limit != 0 ||
	    ipsec->life.packets_hard_limit != 0 ||
	    ipsec->life.packets_soft_limit != 0)
		return -ENOTSUP;

	return 0;
}

static int
cn9k_ipsec_session_create(void *dev,
			  struct rte_security_ipsec_xform *ipsec_xform,
			  struct rte_crypto_sym_xform *crypto_xform,
			  struct rte_security_session *sess)
{
	struct rte_cryptodev *crypto_dev = dev;
	struct cnxk_cpt_qp *qp;
	int ret;

	qp = crypto_dev->data->queue_pairs[0];
	if (qp == NULL) {
		plt_err("CPT queue pairs need to be setup for creating security"
			" session");
		return -EPERM;
	}

	ret = cnxk_ipsec_xform_verify(ipsec_xform, crypto_xform);
	if (ret)
		return ret;

	ret = cn9k_ipsec_xform_verify(ipsec_xform);
	if (ret)
		return ret;

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		return cn9k_ipsec_inb_sa_create(qp, ipsec_xform, crypto_xform,
						sess);
	else
		return cn9k_ipsec_outb_sa_create(qp, ipsec_xform, crypto_xform,
						 sess);
}

static int
cn9k_sec_session_create(void *device, struct rte_security_session_conf *conf,
			struct rte_security_session *sess,
			struct rte_mempool *mempool)
{
	struct cn9k_sec_session *priv;
	int ret;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL)
		return -EINVAL;

	if (rte_mempool_get(mempool, (void **)&priv)) {
		plt_err("Could not allocate security session private data");
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(*priv));

	set_sec_session_private_data(sess, priv);

	if (conf->protocol != RTE_SECURITY_PROTOCOL_IPSEC) {
		ret = -ENOTSUP;
		goto mempool_put;
	}

	ret = cn9k_ipsec_session_create(device, &conf->ipsec,
					conf->crypto_xform, sess);
	if (ret)
		goto mempool_put;

	return 0;

mempool_put:
	rte_mempool_put(mempool, priv);
	set_sec_session_private_data(sess, NULL);
	return ret;
}

static int
cn9k_sec_session_destroy(void *device __rte_unused,
			 struct rte_security_session *sess)
{
	struct roc_ie_on_outb_sa *out_sa;
	struct cn9k_sec_session *priv;
	struct rte_mempool *sess_mp;
	struct roc_ie_on_sa_ctl *ctl;
	struct cn9k_ipsec_sa *sa;

	priv = get_sec_session_private_data(sess);
	if (priv == NULL)
		return 0;

	sa = &priv->sa;
	out_sa = &sa->out_sa;

	ctl = &out_sa->common_sa.ctl;
	ctl->valid = 0;

	rte_io_wmb();

	sess_mp = rte_mempool_from_obj(priv);

	memset(priv, 0, sizeof(*priv));

	set_sec_session_private_data(sess, NULL);
	rte_mempool_put(sess_mp, priv);

	return 0;
}

static unsigned int
cn9k_sec_session_get_size(void *device __rte_unused)
{
	return sizeof(struct cn9k_sec_session);
}

/* Update platform specific security ops */
void
cn9k_sec_ops_override(void)
{
	/* Update platform specific ops */
	cnxk_sec_ops.session_create = cn9k_sec_session_create;
	cnxk_sec_ops.session_destroy = cn9k_sec_session_destroy;
	cnxk_sec_ops.session_get_size = cn9k_sec_session_get_size;
}
