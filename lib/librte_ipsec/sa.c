/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_ipsec.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_errno.h>

#include "sa.h"
#include "ipsec_sqn.h"

/* some helper structures */
struct crypto_xform {
	struct rte_crypto_auth_xform *auth;
	struct rte_crypto_cipher_xform *cipher;
	struct rte_crypto_aead_xform *aead;
};

/*
 * helper routine, fills internal crypto_xform structure.
 */
static int
fill_crypto_xform(struct crypto_xform *xform, uint64_t type,
	const struct rte_ipsec_sa_prm *prm)
{
	struct rte_crypto_sym_xform *xf, *xfn;

	memset(xform, 0, sizeof(*xform));

	xf = prm->crypto_xform;
	if (xf == NULL)
		return -EINVAL;

	xfn = xf->next;

	/* for AEAD just one xform required */
	if (xf->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (xfn != NULL)
			return -EINVAL;
		xform->aead = &xf->aead;
	/*
	 * CIPHER+AUTH xforms are expected in strict order,
	 * depending on SA direction:
	 * inbound: AUTH+CIPHER
	 * outbound: CIPHER+AUTH
	 */
	} else if ((type & RTE_IPSEC_SATP_DIR_MASK) == RTE_IPSEC_SATP_DIR_IB) {

		/* wrong order or no cipher */
		if (xfn == NULL || xf->type != RTE_CRYPTO_SYM_XFORM_AUTH ||
				xfn->type != RTE_CRYPTO_SYM_XFORM_CIPHER)
			return -EINVAL;

		xform->auth = &xf->auth;
		xform->cipher = &xfn->cipher;

	} else {

		/* wrong order or no auth */
		if (xfn == NULL || xf->type != RTE_CRYPTO_SYM_XFORM_CIPHER ||
				xfn->type != RTE_CRYPTO_SYM_XFORM_AUTH)
			return -EINVAL;

		xform->cipher = &xf->cipher;
		xform->auth = &xfn->auth;
	}

	return 0;
}

uint64_t __rte_experimental
rte_ipsec_sa_type(const struct rte_ipsec_sa *sa)
{
	return sa->type;
}

static int32_t
ipsec_sa_size(uint32_t wsz, uint64_t type, uint32_t *nb_bucket)
{
	uint32_t n, sz;

	n = 0;
	if (wsz != 0 && (type & RTE_IPSEC_SATP_DIR_MASK) ==
			RTE_IPSEC_SATP_DIR_IB)
		n = replay_num_bucket(wsz);

	if (n > WINDOW_BUCKET_MAX)
		return -EINVAL;

	*nb_bucket = n;

	sz = rsn_size(n);
	sz += sizeof(struct rte_ipsec_sa);
	return sz;
}

void __rte_experimental
rte_ipsec_sa_fini(struct rte_ipsec_sa *sa)
{
	memset(sa, 0, sa->size);
}

static int
fill_sa_type(const struct rte_ipsec_sa_prm *prm, uint64_t *type)
{
	uint64_t tp;

	tp = 0;

	if (prm->ipsec_xform.proto == RTE_SECURITY_IPSEC_SA_PROTO_AH)
		tp |= RTE_IPSEC_SATP_PROTO_AH;
	else if (prm->ipsec_xform.proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP)
		tp |= RTE_IPSEC_SATP_PROTO_ESP;
	else
		return -EINVAL;

	if (prm->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		tp |= RTE_IPSEC_SATP_DIR_OB;
	else if (prm->ipsec_xform.direction ==
			RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		tp |= RTE_IPSEC_SATP_DIR_IB;
	else
		return -EINVAL;

	if (prm->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (prm->ipsec_xform.tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4)
			tp |= RTE_IPSEC_SATP_MODE_TUNLV4;
		else if (prm->ipsec_xform.tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV6)
			tp |= RTE_IPSEC_SATP_MODE_TUNLV6;
		else
			return -EINVAL;

		if (prm->tun.next_proto == IPPROTO_IPIP)
			tp |= RTE_IPSEC_SATP_IPV4;
		else if (prm->tun.next_proto == IPPROTO_IPV6)
			tp |= RTE_IPSEC_SATP_IPV6;
		else
			return -EINVAL;
	} else if (prm->ipsec_xform.mode ==
			RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT) {
		tp |= RTE_IPSEC_SATP_MODE_TRANS;
		if (prm->trs.proto == IPPROTO_IPIP)
			tp |= RTE_IPSEC_SATP_IPV4;
		else if (prm->trs.proto == IPPROTO_IPV6)
			tp |= RTE_IPSEC_SATP_IPV6;
		else
			return -EINVAL;
	} else
		return -EINVAL;

	*type = tp;
	return 0;
}

static void
esp_inb_init(struct rte_ipsec_sa *sa)
{
	/* these params may differ with new algorithms support */
	sa->ctp.auth.offset = 0;
	sa->ctp.auth.length = sa->icv_len - sa->sqh_len;
	sa->ctp.cipher.offset = sizeof(struct esp_hdr) + sa->iv_len;
	sa->ctp.cipher.length = sa->icv_len + sa->ctp.cipher.offset;
}

static void
esp_inb_tun_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm)
{
	sa->proto = prm->tun.next_proto;
	esp_inb_init(sa);
}

static void
esp_outb_init(struct rte_ipsec_sa *sa, uint32_t hlen)
{
	sa->sqn.outb = 1;

	/* these params may differ with new algorithms support */
	sa->ctp.auth.offset = hlen;
	sa->ctp.auth.length = sizeof(struct esp_hdr) + sa->iv_len + sa->sqh_len;
	if (sa->aad_len != 0) {
		sa->ctp.cipher.offset = hlen + sizeof(struct esp_hdr) +
			sa->iv_len;
		sa->ctp.cipher.length = 0;
	} else {
		sa->ctp.cipher.offset = sa->hdr_len + sizeof(struct esp_hdr);
		sa->ctp.cipher.length = sa->iv_len;
	}
}

static void
esp_outb_tun_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm)
{
	sa->proto = prm->tun.next_proto;
	sa->hdr_len = prm->tun.hdr_len;
	sa->hdr_l3_off = prm->tun.hdr_l3_off;
	memcpy(sa->hdr, prm->tun.hdr, sa->hdr_len);

	esp_outb_init(sa, sa->hdr_len);
}

static int
esp_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	const struct crypto_xform *cxf)
{
	static const uint64_t msk = RTE_IPSEC_SATP_DIR_MASK |
				RTE_IPSEC_SATP_MODE_MASK;

	if (cxf->aead != NULL) {
		/* RFC 4106 */
		if (cxf->aead->algo != RTE_CRYPTO_AEAD_AES_GCM)
			return -EINVAL;
		sa->icv_len = cxf->aead->digest_length;
		sa->iv_ofs = cxf->aead->iv.offset;
		sa->iv_len = sizeof(uint64_t);
		sa->pad_align = IPSEC_PAD_AES_GCM;
	} else {
		sa->icv_len = cxf->auth->digest_length;
		sa->iv_ofs = cxf->cipher->iv.offset;
		sa->sqh_len = IS_ESN(sa) ? sizeof(uint32_t) : 0;
		if (cxf->cipher->algo == RTE_CRYPTO_CIPHER_NULL) {
			sa->pad_align = IPSEC_PAD_NULL;
			sa->iv_len = 0;
		} else if (cxf->cipher->algo == RTE_CRYPTO_CIPHER_AES_CBC) {
			sa->pad_align = IPSEC_PAD_AES_CBC;
			sa->iv_len = IPSEC_MAX_IV_SIZE;
		} else
			return -EINVAL;
	}

	sa->udata = prm->userdata;
	sa->spi = rte_cpu_to_be_32(prm->ipsec_xform.spi);
	sa->salt = prm->ipsec_xform.salt;

	switch (sa->type & msk) {
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV6):
		esp_inb_tun_init(sa, prm);
		break;
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TRANS):
		esp_inb_init(sa);
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6):
		esp_outb_tun_init(sa, prm);
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS):
		esp_outb_init(sa, 0);
		break;
	}

	return 0;
}

int __rte_experimental
rte_ipsec_sa_size(const struct rte_ipsec_sa_prm *prm)
{
	uint64_t type;
	uint32_t nb;
	int32_t rc;

	if (prm == NULL)
		return -EINVAL;

	/* determine SA type */
	rc = fill_sa_type(prm, &type);
	if (rc != 0)
		return rc;

	/* determine required size */
	return ipsec_sa_size(prm->replay_win_sz, type, &nb);
}

int __rte_experimental
rte_ipsec_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	uint32_t size)
{
	int32_t rc, sz;
	uint32_t nb;
	uint64_t type;
	struct crypto_xform cxf;

	if (sa == NULL || prm == NULL)
		return -EINVAL;

	/* determine SA type */
	rc = fill_sa_type(prm, &type);
	if (rc != 0)
		return rc;

	/* determine required size */
	sz = ipsec_sa_size(prm->replay_win_sz, type, &nb);
	if (sz < 0)
		return sz;
	else if (size < (uint32_t)sz)
		return -ENOSPC;

	/* only esp is supported right now */
	if (prm->ipsec_xform.proto != RTE_SECURITY_IPSEC_SA_PROTO_ESP)
		return -EINVAL;

	if (prm->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL &&
			prm->tun.hdr_len > sizeof(sa->hdr))
		return -EINVAL;

	rc = fill_crypto_xform(&cxf, type, prm);
	if (rc != 0)
		return rc;

	/* initialize SA */

	memset(sa, 0, sz);
	sa->type = type;
	sa->size = sz;

	/* check for ESN flag */
	sa->sqn_mask = (prm->ipsec_xform.options.esn == 0) ?
		UINT32_MAX : UINT64_MAX;

	rc = esp_sa_init(sa, prm, &cxf);
	if (rc != 0)
		rte_ipsec_sa_fini(sa);

	/* fill replay window related fields */
	if (nb != 0) {
		sa->replay.win_sz = prm->replay_win_sz;
		sa->replay.nb_bucket = nb;
		sa->replay.bucket_index_mask = sa->replay.nb_bucket - 1;
		sa->sqn.inb = (struct replay_sqn *)(sa + 1);
	}

	return sz;
}

int
ipsec_sa_pkt_func_select(const struct rte_ipsec_session *ss,
	const struct rte_ipsec_sa *sa, struct rte_ipsec_sa_pkt_func *pf)
{
	int32_t rc;

	RTE_SET_USED(sa);

	rc = 0;
	pf[0] = (struct rte_ipsec_sa_pkt_func) { 0 };

	switch (ss->type) {
	default:
		rc = -ENOTSUP;
	}

	return rc;
}
