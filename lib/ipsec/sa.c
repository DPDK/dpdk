/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2020 Intel Corporation
 */

#include <rte_ipsec.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_errno.h>

#include "sa.h"
#include "ipsec_sqn.h"
#include "crypto.h"
#include "misc.h"

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

	/* GMAC has only auth */
	} else if (xf->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			xf->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		if (xfn != NULL)
			return -EINVAL;
		xform->auth = &xf->auth;
		xform->cipher = &xfn->cipher;

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

uint64_t
rte_ipsec_sa_type(const struct rte_ipsec_sa *sa)
{
	return sa->type;
}

/**
 * Based on number of buckets calculated required size for the
 * structure that holds replay window and sequence number (RSN) information.
 */
static size_t
rsn_size(uint32_t nb_bucket)
{
	size_t sz;
	struct replay_sqn *rsn;

	sz = sizeof(*rsn) + nb_bucket * sizeof(rsn->window[0]);
	sz = RTE_ALIGN_CEIL(sz, RTE_CACHE_LINE_SIZE);
	return sz;
}

/*
 * for given size, calculate required number of buckets.
 */
static uint32_t
replay_num_bucket(uint32_t wsz)
{
	uint32_t nb;

	nb = rte_align32pow2(RTE_ALIGN_MUL_CEIL(wsz, WINDOW_BUCKET_SIZE) /
		WINDOW_BUCKET_SIZE);
	nb = RTE_MAX(nb, (uint32_t)WINDOW_BUCKET_MIN);

	return nb;
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
		 * so make sure we have at least 64 window when ESN is enabled.
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

void
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

	/* check for UDP encapsulation flag */
	if (prm->ipsec_xform.options.udp_encap == 1)
		tp |= RTE_IPSEC_SATP_NATT_ENABLE;

	/* check for ESN flag */
	if (prm->ipsec_xform.options.esn == 0)
		tp |= RTE_IPSEC_SATP_ESN_DISABLE;
	else
		tp |= RTE_IPSEC_SATP_ESN_ENABLE;

	/* check for ECN flag */
	if (prm->ipsec_xform.options.ecn == 0)
		tp |= RTE_IPSEC_SATP_ECN_DISABLE;
	else
		tp |= RTE_IPSEC_SATP_ECN_ENABLE;

	/* check for DSCP flag */
	if (prm->ipsec_xform.options.copy_dscp == 0)
		tp |= RTE_IPSEC_SATP_DSCP_DISABLE;
	else
		tp |= RTE_IPSEC_SATP_DSCP_ENABLE;

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
	sa->ctp.cipher.offset = sizeof(struct rte_esp_hdr) + sa->iv_len;
	sa->ctp.cipher.length = sa->icv_len + sa->ctp.cipher.offset;

	/*
	 * for AEAD algorithms we can assume that
	 * auth and cipher offsets would be equal.
	 */
	switch (sa->algo_type) {
	case ALGO_TYPE_AES_GCM:
	case ALGO_TYPE_AES_CCM:
	case ALGO_TYPE_CHACHA20_POLY1305:
		sa->ctp.auth.raw = sa->ctp.cipher.raw;
		break;
	default:
		sa->ctp.auth.offset = 0;
		sa->ctp.auth.length = sa->icv_len - sa->sqh_len;
		sa->cofs.ofs.cipher.tail = sa->sqh_len;
		break;
	}

	sa->cofs.ofs.cipher.head = sa->ctp.cipher.offset - sa->ctp.auth.offset;
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
esp_outb_init(struct rte_ipsec_sa *sa, uint32_t hlen, uint64_t sqn)
{
	uint8_t algo_type;

	sa->sqn.outb = sqn > 1 ? sqn : 1;

	algo_type = sa->algo_type;

	/*
	 * Setup auth and cipher length and offset.
	 * these params may differ with new algorithms support
	 */

	switch (algo_type) {
	case ALGO_TYPE_AES_GCM:
	case ALGO_TYPE_AES_CCM:
	case ALGO_TYPE_CHACHA20_POLY1305:
	case ALGO_TYPE_AES_CTR:
	case ALGO_TYPE_NULL:
		sa->ctp.cipher.offset = hlen + sizeof(struct rte_esp_hdr) +
			sa->iv_len;
		sa->ctp.cipher.length = 0;
		break;
	case ALGO_TYPE_AES_CBC:
	case ALGO_TYPE_3DES_CBC:
		sa->ctp.cipher.offset = hlen + sizeof(struct rte_esp_hdr);
		sa->ctp.cipher.length = sa->iv_len;
		break;
	case ALGO_TYPE_AES_GMAC:
		sa->ctp.cipher.offset = 0;
		sa->ctp.cipher.length = 0;
		break;
	}

	/*
	 * for AEAD algorithms we can assume that
	 * auth and cipher offsets would be equal.
	 */
	switch (algo_type) {
	case ALGO_TYPE_AES_GCM:
	case ALGO_TYPE_AES_CCM:
	case ALGO_TYPE_CHACHA20_POLY1305:
		sa->ctp.auth.raw = sa->ctp.cipher.raw;
		break;
	default:
		sa->ctp.auth.offset = hlen;
		sa->ctp.auth.length = sizeof(struct rte_esp_hdr) +
			sa->iv_len + sa->sqh_len;
		break;
	}

	sa->cofs.ofs.cipher.head = sa->ctp.cipher.offset - sa->ctp.auth.offset;
	sa->cofs.ofs.cipher.tail = (sa->ctp.auth.offset + sa->ctp.auth.length) -
			(sa->ctp.cipher.offset + sa->ctp.cipher.length);
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

	memcpy(sa->hdr, prm->tun.hdr, prm->tun.hdr_len);

	/* insert UDP header if UDP encapsulation is enabled */
	if (sa->type & RTE_IPSEC_SATP_NATT_ENABLE) {
		struct rte_udp_hdr *udph = (struct rte_udp_hdr *)
				&sa->hdr[prm->tun.hdr_len];
		sa->hdr_len += sizeof(struct rte_udp_hdr);
		udph->src_port = rte_cpu_to_be_16(prm->ipsec_xform.udp.sport);
		udph->dst_port = rte_cpu_to_be_16(prm->ipsec_xform.udp.dport);
		udph->dgram_cksum = 0;
	}

	/* update l2_len and l3_len fields for outbound mbuf */
	sa->tx_offload.val = rte_mbuf_tx_offload(sa->hdr_l3_off,
		prm->tun.hdr_len - sa->hdr_l3_off, 0, 0, 0, 0, 0);

	esp_outb_init(sa, sa->hdr_len, prm->ipsec_xform.esn.value);
}

/*
 * helper function, init SA structure.
 */
static int
esp_sa_init(struct rte_ipsec_sa *sa, const struct rte_ipsec_sa_prm *prm,
	const struct crypto_xform *cxf)
{
	static const uint64_t msk = RTE_IPSEC_SATP_DIR_MASK |
				RTE_IPSEC_SATP_MODE_MASK |
				RTE_IPSEC_SATP_NATT_MASK;

	if (prm->ipsec_xform.options.ecn)
		sa->tos_mask |= RTE_IPV4_HDR_ECN_MASK;

	if (prm->ipsec_xform.options.copy_dscp)
		sa->tos_mask |= RTE_IPV4_HDR_DSCP_MASK;

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
		case RTE_CRYPTO_AEAD_AES_CCM:
			/* RFC 4309 */
			sa->aad_len = sizeof(struct aead_ccm_aad);
			sa->icv_len = cxf->aead->digest_length;
			sa->iv_ofs = cxf->aead->iv.offset;
			sa->iv_len = sizeof(uint64_t);
			sa->pad_align = IPSEC_PAD_AES_CCM;
			sa->algo_type = ALGO_TYPE_AES_CCM;
			break;
		case RTE_CRYPTO_AEAD_CHACHA20_POLY1305:
			/* RFC 7634 & 8439*/
			sa->aad_len = sizeof(struct aead_chacha20_poly1305_aad);
			sa->icv_len = cxf->aead->digest_length;
			sa->iv_ofs = cxf->aead->iv.offset;
			sa->iv_len = sizeof(uint64_t);
			sa->pad_align = IPSEC_PAD_CHACHA20_POLY1305;
			sa->algo_type = ALGO_TYPE_CHACHA20_POLY1305;
			break;
		default:
			return -EINVAL;
		}
	} else if (cxf->auth->algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		/* RFC 4543 */
		/* AES-GMAC is a special case of auth that needs IV */
		sa->pad_align = IPSEC_PAD_AES_GMAC;
		sa->iv_len = sizeof(uint64_t);
		sa->icv_len = cxf->auth->digest_length;
		sa->iv_ofs = cxf->auth->iv.offset;
		sa->algo_type = ALGO_TYPE_AES_GMAC;

	} else {
		sa->icv_len = cxf->auth->digest_length;
		sa->iv_ofs = cxf->cipher->iv.offset;

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

	sa->sqh_len = IS_ESN(sa) ? sizeof(uint32_t) : 0;
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
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4 |
			RTE_IPSEC_SATP_NATT_ENABLE):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6 |
			RTE_IPSEC_SATP_NATT_ENABLE):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6):
		esp_outb_tun_init(sa, prm);
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS |
			RTE_IPSEC_SATP_NATT_ENABLE):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS):
		esp_outb_init(sa, 0, prm->ipsec_xform.esn.value);
		break;
	}

	return 0;
}

/*
 * helper function, init SA replay structure.
 */
static void
fill_sa_replay(struct rte_ipsec_sa *sa, uint32_t wnd_sz, uint32_t nb_bucket,
	uint64_t sqn)
{
	sa->replay.win_sz = wnd_sz;
	sa->replay.nb_bucket = nb_bucket;
	sa->replay.bucket_index_mask = nb_bucket - 1;
	sa->sqn.inb.rsn[0] = (struct replay_sqn *)(sa + 1);
	sa->sqn.inb.rsn[0]->sqn = sqn;
	if ((sa->type & RTE_IPSEC_SATP_SQN_MASK) == RTE_IPSEC_SATP_SQN_ATOM) {
		sa->sqn.inb.rsn[1] = (struct replay_sqn *)
			((uintptr_t)sa->sqn.inb.rsn[0] + rsn_size(nb_bucket));
		sa->sqn.inb.rsn[1]->sqn = sqn;
	}
}

int
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
	wsz = prm->ipsec_xform.replay_win_sz;
	return ipsec_sa_size(type, &wsz, &nb);
}

int
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
	wsz = prm->ipsec_xform.replay_win_sz;
	sz = ipsec_sa_size(type, &wsz, &nb);
	if (sz < 0)
		return sz;
	else if (size < (uint32_t)sz)
		return -ENOSPC;

	/* only esp is supported right now */
	if (prm->ipsec_xform.proto != RTE_SECURITY_IPSEC_SA_PROTO_ESP)
		return -EINVAL;

	if (prm->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		uint32_t hlen = prm->tun.hdr_len;
		if (sa->type & RTE_IPSEC_SATP_NATT_ENABLE)
			hlen += sizeof(struct rte_udp_hdr);
		if (hlen > sizeof(sa->hdr))
			return -EINVAL;
	}

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
		fill_sa_replay(sa, wsz, nb, prm->ipsec_xform.esn.value);

	return sz;
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
 * simplest pkt process routine:
 * all actual processing is already done by HW/PMD,
 * just check mbuf ol_flags.
 * used for:
 * - inbound for RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL
 * - inbound/outbound for RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL
 * - outbound for RTE_SECURITY_ACTION_TYPE_NONE when ESN is disabled
 */
uint16_t
pkt_flag_process(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num)
{
	uint32_t i, k, bytes;
	uint32_t dr[num];

	RTE_SET_USED(ss);

	k = 0;
	bytes = 0;
	for (i = 0; i != num; i++) {
		if ((mb[i]->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED) == 0) {
			k++;
			bytes += mb[i]->pkt_len;
		}
		else
			dr[i - k] = i;
	}

	ss->sa->statistics.count += k;
	ss->sa->statistics.bytes += bytes;

	/* handle unprocessed mbufs */
	if (k != num) {
		rte_errno = EBADMSG;
		if (k != 0)
			move_bad_mbufs(mb, dr, num, num - k);
	}

	return k;
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
		pf->prepare.async = esp_inb_pkt_prepare;
		pf->process = esp_inb_tun_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->prepare.async = esp_inb_pkt_prepare;
		pf->process = esp_inb_trs_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->prepare.async = esp_outb_tun_prepare;
		pf->process = (sa->sqh_len != 0) ?
			esp_outb_sqh_process : pkt_flag_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->prepare.async = esp_outb_trs_prepare;
		pf->process = (sa->sqh_len != 0) ?
			esp_outb_sqh_process : pkt_flag_process;
		break;
	default:
		rc = -ENOTSUP;
	}

	return rc;
}

static int
cpu_crypto_pkt_func_select(const struct rte_ipsec_sa *sa,
		struct rte_ipsec_sa_pkt_func *pf)
{
	int32_t rc;

	static const uint64_t msk = RTE_IPSEC_SATP_DIR_MASK |
			RTE_IPSEC_SATP_MODE_MASK;

	rc = 0;
	switch (sa->type & msk) {
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->prepare.sync = cpu_inb_pkt_prepare;
		pf->process = esp_inb_tun_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->prepare.sync = cpu_inb_pkt_prepare;
		pf->process = esp_inb_trs_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV4):
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TUNLV6):
		pf->prepare.sync = cpu_outb_tun_pkt_prepare;
		pf->process = (sa->sqh_len != 0) ?
			esp_outb_sqh_process : pkt_flag_process;
		break;
	case (RTE_IPSEC_SATP_DIR_OB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->prepare.sync = cpu_outb_trs_pkt_prepare;
		pf->process = (sa->sqh_len != 0) ?
			esp_outb_sqh_process : pkt_flag_process;
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
		pf->process = inline_inb_tun_pkt_process;
		break;
	case (RTE_IPSEC_SATP_DIR_IB | RTE_IPSEC_SATP_MODE_TRANS):
		pf->process = inline_inb_trs_pkt_process;
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
	pf[0] = (struct rte_ipsec_sa_pkt_func) { {NULL}, NULL };

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
			pf->process = inline_proto_outb_pkt_process;
		break;
	case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
		pf->prepare.async = lksd_proto_prepare;
		pf->process = pkt_flag_process;
		break;
	case RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO:
		rc = cpu_crypto_pkt_func_select(sa, pf);
		break;
	default:
		rc = -ENOTSUP;
	}

	return rc;
}
