/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_ipsec.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_errno.h>
#include <rte_cryptodev.h>

#include "sa.h"
#include "ipsec_sqn.h"
#include "crypto.h"
#include "iph.h"
#include "pad.h"

#define MBUF_MAX_L2_LEN		RTE_LEN2MASK(RTE_MBUF_L2_LEN_BITS, uint64_t)
#define MBUF_MAX_L3_LEN		RTE_LEN2MASK(RTE_MBUF_L3_LEN_BITS, uint64_t)

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
ipsec_sa_size(uint64_t type, uint32_t *wnd_sz, uint32_t *nb_bucket)
{
	uint32_t n, sz, wsz;

	wsz = *wnd_sz;
	n = 0;

	if ((type & RTE_IPSEC_SATP_DIR_MASK) == RTE_IPSEC_SATP_DIR_IB) {

		/*
		 * RFC 4303 recommends 64 as minimum window size.
		 * there is no point to use ESN mode without SQN window,
		 * so make sure we have at least 64 window when ESN is enalbed.
		 */
		wsz = ((type & RTE_IPSEC_SATP_ESN_MASK) ==
			RTE_IPSEC_SATP_ESN_DISABLE) ?
			wsz : RTE_MAX(wsz, (uint32_t)WINDOW_BUCKET_SIZE);
		if (wsz != 0)
			n = replay_num_bucket(wsz);
	}

	if (n > WINDOW_BUCKET_MAX)
		return -EINVAL;

	*wnd_sz = wsz;
	*nb_bucket = n;

	sz = rsn_size(n);
	if ((type & RTE_IPSEC_SATP_SQN_MASK) == RTE_IPSEC_SATP_SQN_ATOM)
		sz *= REPLAY_SQN_NUM;

	sz += sizeof(struct rte_ipsec_sa);
	return sz;
}

void __rte_experimental
rte_ipsec_sa_fini(struct rte_ipsec_sa *sa)
{
	memset(sa, 0, sa->size);
}

/*
 * Determine expected SA type based on input parameters.
 */
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

	/* check for ESN flag */
	if (prm->ipsec_xform.options.esn == 0)
		tp |= RTE_IPSEC_SATP_ESN_DISABLE;
	else
		tp |= RTE_IPSEC_SATP_ESN_ENABLE;

	/* interpret flags */
	if (prm->flags & RTE_IPSEC_SAFLAG_SQN_ATOM)
		tp |= RTE_IPSEC_SATP_SQN_ATOM;
	else
		tp |= RTE_IPSEC_SATP_SQN_RAW;

	*type = tp;
	return 0;
}

/*
 * Init ESP inbound specific things.
 */
static void
esp_inb_init(struct rte_ipsec_sa *sa)
{
	/* these params may differ with new algorithms support */
	sa->ctp.auth.offset = 0;
	sa->ctp.auth.length = sa->icv_len - sa->sqh_len;
	sa->ctp.cipher.offset = sizeof(struct esp_hdr) + sa->iv_len;
	sa->ctp.cipher.length = sa->icv_len + sa->ctp.cipher.offset;
}

/*
 * Init ESP inbound tunnel specific things.
 */
static void
esp_inb_tun_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm)
{
	sa->proto = prm->tun.next_proto;
	esp_inb_init(sa);
}

/*
 * Init ESP outbound specific things.
 */
static void
esp_outb_init(struct rte_ipsec_sa *sa, uint32_t hlen)
{
	uint8_t algo_type;

	sa->sqn.outb.raw = 1;

	/* these params may differ with new algorithms support */
	sa->ctp.auth.offset = hlen;
	sa->ctp.auth.length = sizeof(struct esp_hdr) + sa->iv_len + sa->sqh_len;

	algo_type = sa->algo_type;

	switch (algo_type) {
	case ALGO_TYPE_AES_GCM:
	case ALGO_TYPE_AES_CTR:
	case ALGO_TYPE_NULL:
		sa->ctp.cipher.offset = hlen + sizeof(struct esp_hdr) +
			sa->iv_len;
		sa->ctp.cipher.length = 0;
		break;
	case ALGO_TYPE_AES_CBC:
	case ALGO_TYPE_3DES_CBC:
		sa->ctp.cipher.offset = sa->hdr_len + sizeof(struct esp_hdr);
		sa->ctp.cipher.length = sa->iv_len;
		break;
	}
}

/*
 * Init ESP outbound tunnel specific things.
 */
static void
esp_outb_tun_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm)
{
	sa->proto = prm->tun.next_proto;
	sa->hdr_len = prm->tun.hdr_len;
	sa->hdr_l3_off = prm->tun.hdr_l3_off;

	/* update l2_len and l3_len fields for outbound mbuf */
	sa->tx_offload.val = rte_mbuf_tx_offload(sa->hdr_l3_off,
		sa->hdr_len - sa->hdr_l3_off, 0, 0, 0, 0, 0);

	memcpy(sa->hdr, prm->tun.hdr, sa->hdr_len);

	esp_outb_init(sa, sa->hdr_len);
}

/*
 * helper function, init SA structure.
 */
static int
esp_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	const struct crypto_xform *cxf)
{
	static const uint64_t msk = RTE_IPSEC_SATP_DIR_MASK |
				RTE_IPSEC_SATP_MODE_MASK;

	if (cxf->aead != NULL) {
		switch (cxf->aead->algo) {
		case RTE_CRYPTO_AEAD_AES_GCM:
			/* RFC 4106 */
			sa->aad_len = sizeof(struct aead_gcm_aad);
			sa->icv_len = cxf->aead->digest_length;
			sa->iv_ofs = cxf->aead->iv.offset;
			sa->iv_len = sizeof(uint64_t);
			sa->pad_align = IPSEC_PAD_AES_GCM;
			sa->algo_type = ALGO_TYPE_AES_GCM;
			break;
		default:
			return -EINVAL;
		}
	} else {
		sa->icv_len = cxf->auth->digest_length;
		sa->iv_ofs = cxf->cipher->iv.offset;
		sa->sqh_len = IS_ESN(sa) ? sizeof(uint32_t) : 0;

		switch (cxf->cipher->algo) {
		case RTE_CRYPTO_CIPHER_NULL:
			sa->pad_align = IPSEC_PAD_NULL;
			sa->iv_len = 0;
			sa->algo_type = ALGO_TYPE_NULL;
			break;

		case RTE_CRYPTO_CIPHER_AES_CBC:
			sa->pad_align = IPSEC_PAD_AES_CBC;
			sa->iv_len = IPSEC_MAX_IV_SIZE;
			sa->algo_type = ALGO_TYPE_AES_CBC;
			break;

		case RTE_CRYPTO_CIPHER_AES_CTR:
			/* RFC 3686 */
			sa->pad_align = IPSEC_PAD_AES_CTR;
			sa->iv_len = IPSEC_AES_CTR_IV_SIZE;
			sa->algo_type = ALGO_TYPE_AES_CTR;
			break;

		case RTE_CRYPTO_CIPHER_3DES_CBC:
			/* RFC 1851 */
			sa->pad_align = IPSEC_PAD_3DES_CBC;
			sa->iv_len = IPSEC_3DES_IV_SIZE;
			sa->algo_type = ALGO_TYPE_3DES_CBC;
			break;

		default:
			return -EINVAL;
		}
	}

	sa->udata = prm->userdata;
	sa->spi = rte_cpu_to_be_32(prm->ipsec_xform.spi);
	sa->salt = prm->ipsec_xform.salt;

	/* preserve all values except l2_len and l3_len */
	sa->tx_offload.msk =
		~rte_mbuf_tx_offload(MBUF_MAX_L2_LEN, MBUF_MAX_L3_LEN,
				0, 0, 0, 0, 0);

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

/*
 * helper function, init SA replay structure.
 */
static void
fill_sa_replay(struct rte_ipsec_sa *sa, uint32_t wnd_sz, uint32_t nb_bucket)
{
	sa->replay.win_sz = wnd_sz;
	sa->replay.nb_bucket = nb_bucket;
	sa->replay.bucket_index_mask = nb_bucket - 1;
	sa->sqn.inb.rsn[0] = (struct replay_sqn *)(sa + 1);
	if ((sa->type & RTE_IPSEC_SATP_SQN_MASK) == RTE_IPSEC_SATP_SQN_ATOM)
		sa->sqn.inb.rsn[1] = (struct replay_sqn *)
			((uintptr_t)sa->sqn.inb.rsn[0] + rsn_size(nb_bucket));
}

int __rte_experimental
rte_ipsec_sa_size(const struct rte_ipsec_sa_prm *prm)
{
	uint64_t type;
	uint32_t nb, wsz;
	int32_t rc;

	if (prm == NULL)
		return -EINVAL;

	/* determine SA type */
	rc = fill_sa_type(prm, &type);
	if (rc != 0)
		return rc;

	/* determine required size */
	wsz = prm->replay_win_sz;
	return ipsec_sa_size(type, &wsz, &nb);
}

int __rte_experimental
rte_ipsec_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	uint32_t size)
{
	int32_t rc, sz;
	uint32_t nb, wsz;
	uint64_t type;
	struct crypto_xform cxf;

	if (sa == NULL || prm == NULL)
		return -EINVAL;

	/* determine SA type */
	rc = fill_sa_type(prm, &type);
	if (rc != 0)
		return rc;

	/* determine required size */
	wsz = prm->replay_win_sz;
	sz = ipsec_sa_size(type, &wsz, &nb);
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
	if (nb != 0)
		fill_sa_replay(sa, wsz, nb);

	return sz;
}

static inline void
mbuf_bulk_copy(struct rte_mbuf *dst[], struct rte_mbuf * const src[],
	uint32_t num)
{
	uint32_t i;

	for (i = 0; i != num; i++)
		dst[i] = src[i];
}

/*
 * setup crypto ops for LOOKASIDE_NONE (pure crypto) type of devices.
 */
static inline void
lksd_none_cop_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], struct rte_crypto_op *cop[], uint16_t num)
{
	uint32_t i;
	struct rte_crypto_sym_op *sop;

	for (i = 0; i != num; i++) {
		sop = cop[i]->sym;
		cop[i]->type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
		cop[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		cop[i]->sess_type = RTE_CRYPTO_OP_WITH_SESSION;
		sop->m_src = mb[i];
		__rte_crypto_sym_op_attach_sym_session(sop, ss->crypto.ses);
	}
}

/*
 * setup crypto op and crypto sym op for ESP outbound packet.
 */
static inline void
esp_outb_cop_prepare(struct rte_crypto_op *cop,
	const struct rte_ipsec_sa *sa, const uint64_t ivp[IPSEC_MAX_IV_QWORD],
	const union sym_op_data *icv, uint32_t hlen, uint32_t plen)
{
	struct rte_crypto_sym_op *sop;
	struct aead_gcm_iv *gcm;
	struct aesctr_cnt_blk *ctr;
	uint8_t algo_type = sa->algo_type;

	/* fill sym op fields */
	sop = cop->sym;

	switch (algo_type) {
	case ALGO_TYPE_AES_CBC:
		/* Cipher-Auth (AES-CBC *) case */
	case ALGO_TYPE_3DES_CBC:
		/* Cipher-Auth (3DES-CBC *) case */
	case ALGO_TYPE_NULL:
		/* NULL case */
		sop->cipher.data.offset = sa->ctp.cipher.offset + hlen;
		sop->cipher.data.length = sa->ctp.cipher.length + plen;
		sop->auth.data.offset = sa->ctp.auth.offset + hlen;
		sop->auth.data.length = sa->ctp.auth.length + plen;
		sop->auth.digest.data = icv->va;
		sop->auth.digest.phys_addr = icv->pa;
		break;
	case ALGO_TYPE_AES_GCM:
		/* AEAD (AES_GCM) case */
		sop->aead.data.offset = sa->ctp.cipher.offset + hlen;
		sop->aead.data.length = sa->ctp.cipher.length + plen;
		sop->aead.digest.data = icv->va;
		sop->aead.digest.phys_addr = icv->pa;
		sop->aead.aad.data = icv->va + sa->icv_len;
		sop->aead.aad.phys_addr = icv->pa + sa->icv_len;

		/* fill AAD IV (located inside crypto op) */
		gcm = rte_crypto_op_ctod_offset(cop, struct aead_gcm_iv *,
			sa->iv_ofs);
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CTR:
		/* Cipher-Auth (AES-CTR *) case */
		sop->cipher.data.offset = sa->ctp.cipher.offset + hlen;
		sop->cipher.data.length = sa->ctp.cipher.length + plen;
		sop->auth.data.offset = sa->ctp.auth.offset + hlen;
		sop->auth.data.length = sa->ctp.auth.length + plen;
		sop->auth.digest.data = icv->va;
		sop->auth.digest.phys_addr = icv->pa;

		ctr = rte_crypto_op_ctod_offset(cop, struct aesctr_cnt_blk *,
			sa->iv_ofs);
		aes_ctr_cnt_blk_fill(ctr, ivp[0], sa->salt);
		break;
	default:
		break;
	}
}

/*
 * setup/update packet data and metadata for ESP outbound tunnel case.
 */
static inline int32_t
esp_outb_tun_pkt_prepare(struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const uint64_t ivp[IPSEC_MAX_IV_QWORD], struct rte_mbuf *mb,
	union sym_op_data *icv)
{
	uint32_t clen, hlen, l2len, pdlen, pdofs, plen, tlen;
	struct rte_mbuf *ml;
	struct esp_hdr *esph;
	struct esp_tail *espt;
	char *ph, *pt;
	uint64_t *iv;

	/* calculate extra header space required */
	hlen = sa->hdr_len + sa->iv_len + sizeof(*esph);

	/* size of ipsec protected data */
	l2len = mb->l2_len;
	plen = mb->pkt_len - l2len;

	/* number of bytes to encrypt */
	clen = plen + sizeof(*espt);
	clen = RTE_ALIGN_CEIL(clen, sa->pad_align);

	/* pad length + esp tail */
	pdlen = clen - plen;
	tlen = pdlen + sa->icv_len;

	/* do append and prepend */
	ml = rte_pktmbuf_lastseg(mb);
	if (tlen + sa->sqh_len + sa->aad_len > rte_pktmbuf_tailroom(ml))
		return -ENOSPC;

	/* prepend header */
	ph = rte_pktmbuf_prepend(mb, hlen - l2len);
	if (ph == NULL)
		return -ENOSPC;

	/* append tail */
	pdofs = ml->data_len;
	ml->data_len += tlen;
	mb->pkt_len += tlen;
	pt = rte_pktmbuf_mtod_offset(ml, typeof(pt), pdofs);

	/* update pkt l2/l3 len */
	mb->tx_offload = (mb->tx_offload & sa->tx_offload.msk) |
		sa->tx_offload.val;

	/* copy tunnel pkt header */
	rte_memcpy(ph, sa->hdr, sa->hdr_len);

	/* update original and new ip header fields */
	update_tun_l3hdr(sa, ph + sa->hdr_l3_off, mb->pkt_len, sa->hdr_l3_off,
			sqn_low16(sqc));

	/* update spi, seqn and iv */
	esph = (struct esp_hdr *)(ph + sa->hdr_len);
	iv = (uint64_t *)(esph + 1);
	copy_iv(iv, ivp, sa->iv_len);

	esph->spi = sa->spi;
	esph->seq = sqn_low32(sqc);

	/* offset for ICV */
	pdofs += pdlen + sa->sqh_len;

	/* pad length */
	pdlen -= sizeof(*espt);

	/* copy padding data */
	rte_memcpy(pt, esp_pad_bytes, pdlen);

	/* update esp trailer */
	espt = (struct esp_tail *)(pt + pdlen);
	espt->pad_len = pdlen;
	espt->next_proto = sa->proto;

	icv->va = rte_pktmbuf_mtod_offset(ml, void *, pdofs);
	icv->pa = rte_pktmbuf_iova_offset(ml, pdofs);

	return clen;
}

/*
 * for pure cryptodev (lookaside none) depending on SA settings,
 * we might have to write some extra data to the packet.
 */
static inline void
outb_pkt_xprepare(const struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const union sym_op_data *icv)
{
	uint32_t *psqh;
	struct aead_gcm_aad *aad;
	uint8_t algo_type = sa->algo_type;

	/* insert SQN.hi between ESP trailer and ICV */
	if (sa->sqh_len != 0) {
		psqh = (uint32_t *)(icv->va - sa->sqh_len);
		psqh[0] = sqn_hi32(sqc);
	}

	/*
	 * fill IV and AAD fields, if any (aad fields are placed after icv),
	 * right now we support only one AEAD algorithm: AES-GCM .
	 */
	if (algo_type == ALGO_TYPE_AES_GCM) {
		aad = (struct aead_gcm_aad *)(icv->va + sa->icv_len);
		aead_gcm_aad_fill(aad, sa->spi, sqc, IS_ESN(sa));
	}
}

/*
 * setup/update packets and crypto ops for ESP outbound tunnel case.
 */
static uint16_t
outb_tun_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, n;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	n = num;
	sqn = esn_outb_update_sqn(sa, &n);
	if (n != num)
		rte_errno = EOVERFLOW;

	k = 0;
	for (i = 0; i != n; i++) {

		sqc = rte_cpu_to_be_64(sqn + i);
		gen_iv(iv, sqc);

		/* try to update the packet itself */
		rc = esp_outb_tun_pkt_prepare(sa, sqc, iv, mb[i], &icv);

		/* success, setup crypto op */
		if (rc >= 0) {
			mb[k] = mb[i];
			outb_pkt_xprepare(sa, sqc, &icv);
			esp_outb_cop_prepare(cop[k], sa, iv, &icv, 0, rc);
			k++;
		/* failure, put packet into the death-row */
		} else {
			dr[i - k] = mb[i];
			rte_errno = -rc;
		}
	}

	/* update cops */
	lksd_none_cop_prepare(ss, mb, cop, k);

	 /* copy not prepared mbufs beyond good ones */
	if (k != n && k != 0)
		mbuf_bulk_copy(mb + k, dr, n - k);

	return k;
}

/*
 * setup/update packet data and metadata for ESP outbound transport case.
 */
static inline int32_t
esp_outb_trs_pkt_prepare(struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const uint64_t ivp[IPSEC_MAX_IV_QWORD], struct rte_mbuf *mb,
	uint32_t l2len, uint32_t l3len, union sym_op_data *icv)
{
	uint8_t np;
	uint32_t clen, hlen, pdlen, pdofs, plen, tlen, uhlen;
	struct rte_mbuf *ml;
	struct esp_hdr *esph;
	struct esp_tail *espt;
	char *ph, *pt;
	uint64_t *iv;

	uhlen = l2len + l3len;
	plen = mb->pkt_len - uhlen;

	/* calculate extra header space required */
	hlen = sa->iv_len + sizeof(*esph);

	/* number of bytes to encrypt */
	clen = plen + sizeof(*espt);
	clen = RTE_ALIGN_CEIL(clen, sa->pad_align);

	/* pad length + esp tail */
	pdlen = clen - plen;
	tlen = pdlen + sa->icv_len;

	/* do append and insert */
	ml = rte_pktmbuf_lastseg(mb);
	if (tlen + sa->sqh_len + sa->aad_len > rte_pktmbuf_tailroom(ml))
		return -ENOSPC;

	/* prepend space for ESP header */
	ph = rte_pktmbuf_prepend(mb, hlen);
	if (ph == NULL)
		return -ENOSPC;

	/* append tail */
	pdofs = ml->data_len;
	ml->data_len += tlen;
	mb->pkt_len += tlen;
	pt = rte_pktmbuf_mtod_offset(ml, typeof(pt), pdofs);

	/* shift L2/L3 headers */
	insert_esph(ph, ph + hlen, uhlen);

	/* update ip  header fields */
	np = update_trs_l3hdr(sa, ph + l2len, mb->pkt_len, l2len, l3len,
			IPPROTO_ESP);

	/* update spi, seqn and iv */
	esph = (struct esp_hdr *)(ph + uhlen);
	iv = (uint64_t *)(esph + 1);
	copy_iv(iv, ivp, sa->iv_len);

	esph->spi = sa->spi;
	esph->seq = sqn_low32(sqc);

	/* offset for ICV */
	pdofs += pdlen + sa->sqh_len;

	/* pad length */
	pdlen -= sizeof(*espt);

	/* copy padding data */
	rte_memcpy(pt, esp_pad_bytes, pdlen);

	/* update esp trailer */
	espt = (struct esp_tail *)(pt + pdlen);
	espt->pad_len = pdlen;
	espt->next_proto = np;

	icv->va = rte_pktmbuf_mtod_offset(ml, void *, pdofs);
	icv->pa = rte_pktmbuf_iova_offset(ml, pdofs);

	return clen;
}

/*
 * setup/update packets and crypto ops for ESP outbound transport case.
 */
static uint16_t
outb_trs_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, n, l2, l3;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	n = num;
	sqn = esn_outb_update_sqn(sa, &n);
	if (n != num)
		rte_errno = EOVERFLOW;

	k = 0;
	for (i = 0; i != n; i++) {

		l2 = mb[i]->l2_len;
		l3 = mb[i]->l3_len;

		sqc = rte_cpu_to_be_64(sqn + i);
		gen_iv(iv, sqc);

		/* try to update the packet itself */
		rc = esp_outb_trs_pkt_prepare(sa, sqc, iv, mb[i],
				l2, l3, &icv);

		/* success, setup crypto op */
		if (rc >= 0) {
			mb[k] = mb[i];
			outb_pkt_xprepare(sa, sqc, &icv);
			esp_outb_cop_prepare(cop[k], sa, iv, &icv, l2 + l3, rc);
			k++;
		/* failure, put packet into the death-row */
		} else {
			dr[i - k] = mb[i];
			rte_errno = -rc;
		}
	}

	/* update cops */
	lksd_none_cop_prepare(ss, mb, cop, k);

	/* copy not prepared mbufs beyond good ones */
	if (k != n && k != 0)
		mbuf_bulk_copy(mb + k, dr, n - k);

	return k;
}

/*
 * setup crypto op and crypto sym op for ESP inbound tunnel packet.
 */
static inline int32_t
esp_inb_tun_cop_prepare(struct rte_crypto_op *cop,
	const struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
	const union sym_op_data *icv, uint32_t pofs, uint32_t plen)
{
	struct rte_crypto_sym_op *sop;
	struct aead_gcm_iv *gcm;
	struct aesctr_cnt_blk *ctr;
	uint64_t *ivc, *ivp;
	uint32_t clen;
	uint8_t algo_type = sa->algo_type;

	clen = plen - sa->ctp.cipher.length;
	if ((int32_t)clen < 0 || (clen & (sa->pad_align - 1)) != 0)
		return -EINVAL;

	/* fill sym op fields */
	sop = cop->sym;

	switch (algo_type) {
	case ALGO_TYPE_AES_GCM:
		sop->aead.data.offset = pofs + sa->ctp.cipher.offset;
		sop->aead.data.length = clen;
		sop->aead.digest.data = icv->va;
		sop->aead.digest.phys_addr = icv->pa;
		sop->aead.aad.data = icv->va + sa->icv_len;
		sop->aead.aad.phys_addr = icv->pa + sa->icv_len;

		/* fill AAD IV (located inside crypto op) */
		gcm = rte_crypto_op_ctod_offset(cop, struct aead_gcm_iv *,
			sa->iv_ofs);
		ivp = rte_pktmbuf_mtod_offset(mb, uint64_t *,
			pofs + sizeof(struct esp_hdr));
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CBC:
	case ALGO_TYPE_3DES_CBC:
		sop->cipher.data.offset = pofs + sa->ctp.cipher.offset;
		sop->cipher.data.length = clen;
		sop->auth.data.offset = pofs + sa->ctp.auth.offset;
		sop->auth.data.length = plen - sa->ctp.auth.length;
		sop->auth.digest.data = icv->va;
		sop->auth.digest.phys_addr = icv->pa;

		/* copy iv from the input packet to the cop */
		ivc = rte_crypto_op_ctod_offset(cop, uint64_t *, sa->iv_ofs);
		ivp = rte_pktmbuf_mtod_offset(mb, uint64_t *,
			pofs + sizeof(struct esp_hdr));
		copy_iv(ivc, ivp, sa->iv_len);
		break;
	case ALGO_TYPE_AES_CTR:
		sop->cipher.data.offset = pofs + sa->ctp.cipher.offset;
		sop->cipher.data.length = clen;
		sop->auth.data.offset = pofs + sa->ctp.auth.offset;
		sop->auth.data.length = plen - sa->ctp.auth.length;
		sop->auth.digest.data = icv->va;
		sop->auth.digest.phys_addr = icv->pa;

		/* copy iv from the input packet to the cop */
		ctr = rte_crypto_op_ctod_offset(cop, struct aesctr_cnt_blk *,
			sa->iv_ofs);
		ivp = rte_pktmbuf_mtod_offset(mb, uint64_t *,
			pofs + sizeof(struct esp_hdr));
		aes_ctr_cnt_blk_fill(ctr, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_NULL:
		sop->cipher.data.offset = pofs + sa->ctp.cipher.offset;
		sop->cipher.data.length = clen;
		sop->auth.data.offset = pofs + sa->ctp.auth.offset;
		sop->auth.data.length = plen - sa->ctp.auth.length;
		sop->auth.digest.data = icv->va;
		sop->auth.digest.phys_addr = icv->pa;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * for pure cryptodev (lookaside none) depending on SA settings,
 * we might have to write some extra data to the packet.
 */
static inline void
inb_pkt_xprepare(const struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const union sym_op_data *icv)
{
	struct aead_gcm_aad *aad;

	/* insert SQN.hi between ESP trailer and ICV */
	if (sa->sqh_len != 0)
		insert_sqh(sqn_hi32(sqc), icv->va, sa->icv_len);

	/*
	 * fill AAD fields, if any (aad fields are placed after icv),
	 * right now we support only one AEAD algorithm: AES-GCM.
	 */
	if (sa->aad_len != 0) {
		aad = (struct aead_gcm_aad *)(icv->va + sa->icv_len);
		aead_gcm_aad_fill(aad, sa->spi, sqc, IS_ESN(sa));
	}
}

/*
 * setup/update packet data and metadata for ESP inbound tunnel case.
 */
static inline int32_t
esp_inb_tun_pkt_prepare(const struct rte_ipsec_sa *sa,
	const struct replay_sqn *rsn, struct rte_mbuf *mb,
	uint32_t hlen, union sym_op_data *icv)
{
	int32_t rc;
	uint64_t sqn;
	uint32_t icv_ofs, plen;
	struct rte_mbuf *ml;
	struct esp_hdr *esph;

	esph = rte_pktmbuf_mtod_offset(mb, struct esp_hdr *, hlen);

	/*
	 * retrieve and reconstruct SQN, then check it, then
	 * convert it back into network byte order.
	 */
	sqn = rte_be_to_cpu_32(esph->seq);
	if (IS_ESN(sa))
		sqn = reconstruct_esn(rsn->sqn, sqn, sa->replay.win_sz);

	rc = esn_inb_check_sqn(rsn, sa, sqn);
	if (rc != 0)
		return rc;

	sqn = rte_cpu_to_be_64(sqn);

	/* start packet manipulation */
	plen = mb->pkt_len;
	plen = plen - hlen;

	ml = rte_pktmbuf_lastseg(mb);
	icv_ofs = ml->data_len - sa->icv_len + sa->sqh_len;

	/* we have to allocate space for AAD somewhere,
	 * right now - just use free trailing space at the last segment.
	 * Would probably be more convenient to reserve space for AAD
	 * inside rte_crypto_op itself
	 * (again for IV space is already reserved inside cop).
	 */
	if (sa->aad_len + sa->sqh_len > rte_pktmbuf_tailroom(ml))
		return -ENOSPC;

	icv->va = rte_pktmbuf_mtod_offset(ml, void *, icv_ofs);
	icv->pa = rte_pktmbuf_iova_offset(ml, icv_ofs);

	inb_pkt_xprepare(sa, sqn, icv);
	return plen;
}

/*
 * setup/update packets and crypto ops for ESP inbound case.
 */
static uint16_t
inb_pkt_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, hl;
	struct rte_ipsec_sa *sa;
	struct replay_sqn *rsn;
	union sym_op_data icv;
	struct rte_mbuf *dr[num];

	sa = ss->sa;
	rsn = rsn_acquire(sa);

	k = 0;
	for (i = 0; i != num; i++) {

		hl = mb[i]->l2_len + mb[i]->l3_len;
		rc = esp_inb_tun_pkt_prepare(sa, rsn, mb[i], hl, &icv);
		if (rc >= 0)
			rc = esp_inb_tun_cop_prepare(cop[k], sa, mb[i], &icv,
				hl, rc);

		if (rc == 0)
			mb[k++] = mb[i];
		else {
			dr[i - k] = mb[i];
			rte_errno = -rc;
		}
	}

	rsn_release(sa, rsn);

	/* update cops */
	lksd_none_cop_prepare(ss, mb, cop, k);

	/* copy not prepared mbufs beyond good ones */
	if (k != num && k != 0)
		mbuf_bulk_copy(mb + k, dr, num - k);

	return k;
}

/*
 *  setup crypto ops for LOOKASIDE_PROTO type of devices.
 */
static inline void
lksd_proto_cop_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], struct rte_crypto_op *cop[], uint16_t num)
{
	uint32_t i;
	struct rte_crypto_sym_op *sop;

	for (i = 0; i != num; i++) {
		sop = cop[i]->sym;
		cop[i]->type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
		cop[i]->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		cop[i]->sess_type = RTE_CRYPTO_OP_SECURITY_SESSION;
		sop->m_src = mb[i];
		__rte_security_attach_session(sop, ss->security.ses);
	}
}

/*
 *  setup packets and crypto ops for LOOKASIDE_PROTO type of devices.
 *  Note that for LOOKASIDE_PROTO all packet modifications will be
 *  performed by PMD/HW.
 *  SW has only to prepare crypto op.
 */
static uint16_t
lksd_proto_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], struct rte_crypto_op *cop[], uint16_t num)
{
	lksd_proto_cop_prepare(ss, mb, cop, num);
	return num;
}

/*
 * process ESP inbound tunnel packet.
 */
static inline int
esp_inb_tun_single_pkt_process(struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
	uint32_t *sqn)
{
	uint32_t hlen, icv_len, tlen;
	struct esp_hdr *esph;
	struct esp_tail *espt;
	struct rte_mbuf *ml;
	char *pd;

	if (mb->ol_flags & PKT_RX_SEC_OFFLOAD_FAILED)
		return -EBADMSG;

	icv_len = sa->icv_len;

	ml = rte_pktmbuf_lastseg(mb);
	espt = rte_pktmbuf_mtod_offset(ml, struct esp_tail *,
		ml->data_len - icv_len - sizeof(*espt));

	/*
	 * check padding and next proto.
	 * return an error if something is wrong.
	 */
	pd = (char *)espt - espt->pad_len;
	if (espt->next_proto != sa->proto ||
			memcmp(pd, esp_pad_bytes, espt->pad_len))
		return -EINVAL;

	/* cut of ICV, ESP tail and padding bytes */
	tlen = icv_len + sizeof(*espt) + espt->pad_len;
	ml->data_len -= tlen;
	mb->pkt_len -= tlen;

	/* cut of L2/L3 headers, ESP header and IV */
	hlen = mb->l2_len + mb->l3_len;
	esph = rte_pktmbuf_mtod_offset(mb, struct esp_hdr *, hlen);
	rte_pktmbuf_adj(mb, hlen + sa->ctp.cipher.offset);

	/* retrieve SQN for later check */
	*sqn = rte_be_to_cpu_32(esph->seq);

	/* reset mbuf metatdata: L2/L3 len, packet type */
	mb->packet_type = RTE_PTYPE_UNKNOWN;
	mb->tx_offload = (mb->tx_offload & sa->tx_offload.msk) |
		sa->tx_offload.val;

	/* clear the PKT_RX_SEC_OFFLOAD flag if set */
	mb->ol_flags &= ~(mb->ol_flags & PKT_RX_SEC_OFFLOAD);
	return 0;
}

/*
 * process ESP inbound transport packet.
 */
static inline int
esp_inb_trs_single_pkt_process(struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
	uint32_t *sqn)
{
	uint32_t hlen, icv_len, l2len, l3len, tlen;
	struct esp_hdr *esph;
	struct esp_tail *espt;
	struct rte_mbuf *ml;
	char *np, *op, *pd;

	if (mb->ol_flags & PKT_RX_SEC_OFFLOAD_FAILED)
		return -EBADMSG;

	icv_len = sa->icv_len;

	ml = rte_pktmbuf_lastseg(mb);
	espt = rte_pktmbuf_mtod_offset(ml, struct esp_tail *,
		ml->data_len - icv_len - sizeof(*espt));

	/* check padding, return an error if something is wrong. */
	pd = (char *)espt - espt->pad_len;
	if (memcmp(pd, esp_pad_bytes, espt->pad_len))
		return -EINVAL;

	/* cut of ICV, ESP tail and padding bytes */
	tlen = icv_len + sizeof(*espt) + espt->pad_len;
	ml->data_len -= tlen;
	mb->pkt_len -= tlen;

	/* retrieve SQN for later check */
	l2len = mb->l2_len;
	l3len = mb->l3_len;
	hlen = l2len + l3len;
	op = rte_pktmbuf_mtod(mb, char *);
	esph = (struct esp_hdr *)(op + hlen);
	*sqn = rte_be_to_cpu_32(esph->seq);

	/* cut off ESP header and IV, update L3 header */
	np = rte_pktmbuf_adj(mb, sa->ctp.cipher.offset);
	remove_esph(np, op, hlen);
	update_trs_l3hdr(sa, np + l2len, mb->pkt_len, l2len, l3len,
			espt->next_proto);

	/* reset mbuf packet type */
	mb->packet_type &= (RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);

	/* clear the PKT_RX_SEC_OFFLOAD flag if set */
	mb->ol_flags &= ~(mb->ol_flags & PKT_RX_SEC_OFFLOAD);
	return 0;
}

/*
 * for group of ESP inbound packets perform SQN check and update.
 */
static inline uint16_t
esp_inb_rsn_update(struct rte_ipsec_sa *sa, const uint32_t sqn[],
	struct rte_mbuf *mb[], struct rte_mbuf *dr[], uint16_t num)
{
	uint32_t i, k;
	struct replay_sqn *rsn;

	rsn = rsn_update_start(sa);

	k = 0;
	for (i = 0; i != num; i++) {
		if (esn_inb_update_sqn(rsn, sa, sqn[i]) == 0)
			mb[k++] = mb[i];
		else
			dr[i - k] = mb[i];
	}

	rsn_update_finish(sa, rsn);
	return k;
}

/*
 * process group of ESP inbound tunnel packets.
 */
static uint16_t
inb_tun_pkt_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num)
{
	uint32_t i, k;
	struct rte_ipsec_sa *sa;
	uint32_t sqn[num];
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	/* process packets, extract seq numbers */

	k = 0;
	for (i = 0; i != num; i++) {
		/* good packet */
		if (esp_inb_tun_single_pkt_process(sa, mb[i], sqn + k) == 0)
			mb[k++] = mb[i];
		/* bad packet, will drop from furhter processing */
		else
			dr[i - k] = mb[i];
	}

	/* update seq # and replay winow */
	k = esp_inb_rsn_update(sa, sqn, mb, dr + i - k, k);

	/* handle unprocessed mbufs */
	if (k != num) {
		rte_errno = EBADMSG;
		if (k != 0)
			mbuf_bulk_copy(mb + k, dr, num - k);
	}

	return k;
}

/*
 * process group of ESP inbound transport packets.
 */
static uint16_t
inb_trs_pkt_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num)
{
	uint32_t i, k;
	uint32_t sqn[num];
	struct rte_ipsec_sa *sa;
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	/* process packets, extract seq numbers */

	k = 0;
	for (i = 0; i != num; i++) {
		/* good packet */
		if (esp_inb_trs_single_pkt_process(sa, mb[i], sqn + k) == 0)
			mb[k++] = mb[i];
		/* bad packet, will drop from furhter processing */
		else
			dr[i - k] = mb[i];
	}

	/* update seq # and replay winow */
	k = esp_inb_rsn_update(sa, sqn, mb, dr + i - k, k);

	/* handle unprocessed mbufs */
	if (k != num) {
		rte_errno = EBADMSG;
		if (k != 0)
			mbuf_bulk_copy(mb + k, dr, num - k);
	}

	return k;
}

/*
 * process outbound packets for SA with ESN support,
 * for algorithms that require SQN.hibits to be implictly included
 * into digest computation.
 * In that case we have to move ICV bytes back to their proper place.
 */
static uint16_t
outb_sqh_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num)
{
	uint32_t i, k, icv_len, *icv;
	struct rte_mbuf *ml;
	struct rte_ipsec_sa *sa;
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	k = 0;
	icv_len = sa->icv_len;

	for (i = 0; i != num; i++) {
		if ((mb[i]->ol_flags & PKT_RX_SEC_OFFLOAD_FAILED) == 0) {
			ml = rte_pktmbuf_lastseg(mb[i]);
			icv = rte_pktmbuf_mtod_offset(ml, void *,
				ml->data_len - icv_len);
			remove_sqh(icv, icv_len);
			mb[k++] = mb[i];
		} else
			dr[i - k] = mb[i];
	}

	/* handle unprocessed mbufs */
	if (k != num) {
		rte_errno = EBADMSG;
		if (k != 0)
			mbuf_bulk_copy(mb + k, dr, num - k);
	}

	return k;
}

/*
 * simplest pkt process routine:
 * all actual processing is already done by HW/PMD,
 * just check mbuf ol_flags.
 * used for:
 * - inbound for RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL
 * - inbound/outbound for RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL
 * - outbound for RTE_SECURITY_ACTION_TYPE_NONE when ESN is disabled
 */
static uint16_t
pkt_flag_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num)
{
	uint32_t i, k;
	struct rte_mbuf *dr[num];

	RTE_SET_USED(ss);

	k = 0;
	for (i = 0; i != num; i++) {
		if ((mb[i]->ol_flags & PKT_RX_SEC_OFFLOAD_FAILED) == 0)
			mb[k++] = mb[i];
		else
			dr[i - k] = mb[i];
	}

	/* handle unprocessed mbufs */
	if (k != num) {
		rte_errno = EBADMSG;
		if (k != 0)
			mbuf_bulk_copy(mb + k, dr, num - k);
	}

	return k;
}

/*
 * prepare packets for inline ipsec processing:
 * set ol_flags and attach metadata.
 */
static inline void
inline_outb_mbuf_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	uint32_t i, ol_flags;

	ol_flags = ss->security.ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA;
	for (i = 0; i != num; i++) {

		mb[i]->ol_flags |= PKT_TX_SEC_OFFLOAD;
		if (ol_flags != 0)
			rte_security_set_pkt_metadata(ss->security.ctx,
				ss->security.ses, mb[i], NULL);
	}
}

/*
 * process group of ESP outbound tunnel packets destined for
 * INLINE_CRYPTO type of device.
 */
static uint16_t
inline_outb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, n;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	n = num;
	sqn = esn_outb_update_sqn(sa, &n);
	if (n != num)
		rte_errno = EOVERFLOW;

	k = 0;
	for (i = 0; i != n; i++) {

		sqc = rte_cpu_to_be_64(sqn + i);
		gen_iv(iv, sqc);

		/* try to update the packet itself */
		rc = esp_outb_tun_pkt_prepare(sa, sqc, iv, mb[i], &icv);

		/* success, update mbuf fields */
		if (rc >= 0)
			mb[k++] = mb[i];
		/* failure, put packet into the death-row */
		else {
			dr[i - k] = mb[i];
			rte_errno = -rc;
		}
	}

	inline_outb_mbuf_prepare(ss, mb, k);

	/* copy not processed mbufs beyond good ones */
	if (k != n && k != 0)
		mbuf_bulk_copy(mb + k, dr, n - k);

	return k;
}

/*
 * process group of ESP outbound transport packets destined for
 * INLINE_CRYPTO type of device.
 */
static uint16_t
inline_outb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, n, l2, l3;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	struct rte_mbuf *dr[num];

	sa = ss->sa;

	n = num;
	sqn = esn_outb_update_sqn(sa, &n);
	if (n != num)
		rte_errno = EOVERFLOW;

	k = 0;
	for (i = 0; i != n; i++) {

		l2 = mb[i]->l2_len;
		l3 = mb[i]->l3_len;

		sqc = rte_cpu_to_be_64(sqn + i);
		gen_iv(iv, sqc);

		/* try to update the packet itself */
		rc = esp_outb_trs_pkt_prepare(sa, sqc, iv, mb[i],
				l2, l3, &icv);

		/* success, update mbuf fields */
		if (rc >= 0)
			mb[k++] = mb[i];
		/* failure, put packet into the death-row */
		else {
			dr[i - k] = mb[i];
			rte_errno = -rc;
		}
	}

	inline_outb_mbuf_prepare(ss, mb, k);

	/* copy not processed mbufs beyond good ones */
	if (k != n && k != 0)
		mbuf_bulk_copy(mb + k, dr, n - k);

	return k;
}

/*
 * outbound for RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
 * actual processing is done by HW/PMD, just set flags and metadata.
 */
static uint16_t
outb_inline_proto_process(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num)
{
	inline_outb_mbuf_prepare(ss, mb, num);
	return num;
}

/*
 * Select packet processing function for session on LOOKASIDE_NONE
 * type of device.
 */
static int
lksd_none_pkt_func_select(const struct rte_ipsec_sa *sa,
		struct rte_ipsec_sa_pkt_func *pf)
{
	int32_t rc;

	static const uint64_t msk = RTE_IPSEC_SATP_DIR_MASK |
			RTE_IPSEC_SATP_MODE_MASK;

	rc = 0;
	switch (sa->type & msk) {
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->prepare = inb_pkt_prepare;
		pf->process = inb_tun_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->prepare = inb_pkt_prepare;
		pf->process = inb_trs_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->prepare = outb_tun_prepare;
		pf->process = (sa->sqh_len != 0) ?
			outb_sqh_process : pkt_flag_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->prepare = outb_trs_prepare;
		pf->process = (sa->sqh_len != 0) ?
			outb_sqh_process : pkt_flag_process;
		break;
	default:
		rc = -ENOTSUP;
	}

	return rc;
}

/*
 * Select packet processing function for session on INLINE_CRYPTO
 * type of device.
 */
static int
inline_crypto_pkt_func_select(const struct rte_ipsec_sa *sa,
		struct rte_ipsec_sa_pkt_func *pf)
{
	int32_t rc;

	static const uint64_t msk = RTE_IPSEC_SATP_DIR_MASK |
			RTE_IPSEC_SATP_MODE_MASK;

	rc = 0;
	switch (sa->type & msk) {
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->process = inb_tun_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->process = inb_trs_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->process = inline_outb_tun_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->process = inline_outb_trs_pkt_process;
		break;
	default:
		rc = -ENOTSUP;
	}

	return rc;
}

/*
 * Select packet processing function for given session based on SA parameters
 * and type of associated with the session device.
 */
int
ipsec_sa_pkt_func_select(const struct rte_ipsec_session *ss,
	const struct rte_ipsec_sa *sa, struct rte_ipsec_sa_pkt_func *pf)
{
	int32_t rc;

	rc = 0;
	pf[0] = (struct rte_ipsec_sa_pkt_func) { 0 };

	switch (ss->type) {
	case RTE_SECURITY_ACTION_TYPE_NONE:
		rc = lksd_none_pkt_func_select(sa, pf);
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
		rc = inline_crypto_pkt_func_select(sa, pf);
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
		if ((sa->type & RTE_IPSEC_SATP_DIR_MASK) ==
				RTE_IPSEC_SATP_DIR_IB)
			pf->process = pkt_flag_process;
		else
			pf->process = outb_inline_proto_process;
		break;
	case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
		pf->prepare = lksd_proto_prepare;
		pf->process = pkt_flag_process;
		break;
	default:
		rc = -ENOTSUP;
	}

	return rc;
}
