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
#include "misc.h"
#include "pad.h"

/*
 * setup crypto op and crypto sym op for ESP inbound tunnel packet.
 */
static inline void
inb_cop_prepare(struct rte_crypto_op *cop,
	const struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
	const union sym_op_data *icv, uint32_t pofs, uint32_t plen)
{
	struct rte_crypto_sym_op *sop;
	struct aead_gcm_iv *gcm;
	struct aesctr_cnt_blk *ctr;
	uint64_t *ivc, *ivp;
	uint32_t algo;

	algo = sa->algo_type;

	/* fill sym op fields */
	sop = cop->sym;

	switch (algo) {
	case ALGO_TYPE_AES_GCM:
		sop->aead.data.offset = pofs + sa->ctp.cipher.offset;
		sop->aead.data.length = plen - sa->ctp.cipher.length;
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
		sop->cipher.data.length = plen - sa->ctp.cipher.length;
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
		sop->cipher.data.length = plen - sa->ctp.cipher.length;
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
		sop->cipher.data.length = plen - sa->ctp.cipher.length;
		sop->auth.data.offset = pofs + sa->ctp.auth.offset;
		sop->auth.data.length = plen - sa->ctp.auth.length;
		sop->auth.digest.data = icv->va;
		sop->auth.digest.phys_addr = icv->pa;
		break;
	}
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
inb_pkt_prepare(const struct rte_ipsec_sa *sa, const struct replay_sqn *rsn,
	struct rte_mbuf *mb, uint32_t hlen, union sym_op_data *icv)
{
	int32_t rc;
	uint64_t sqn;
	uint32_t clen, icv_ofs, plen;
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

	/* check that packet has a valid length */
	clen = plen - sa->ctp.cipher.length;
	if ((int32_t)clen < 0 || (clen & (sa->pad_align - 1)) != 0)
		return -EBADMSG;

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
uint16_t
esp_inb_pkt_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, hl;
	struct rte_ipsec_sa *sa;
	struct rte_cryptodev_sym_session *cs;
	struct replay_sqn *rsn;
	union sym_op_data icv;
	uint32_t dr[num];

	sa = ss->sa;
	cs = ss->crypto.ses;
	rsn = rsn_acquire(sa);

	k = 0;
	for (i = 0; i != num; i++) {

		hl = mb[i]->l2_len + mb[i]->l3_len;
		rc = inb_pkt_prepare(sa, rsn, mb[i], hl, &icv);
		if (rc >= 0) {
			lksd_none_cop_prepare(cop[k], cs, mb[i]);
			inb_cop_prepare(cop[k], sa, mb[i], &icv, hl, rc);
			k++;
		} else
			dr[i - k] = i;
	}

	rsn_release(sa, rsn);

	/* copy not prepared mbufs beyond good ones */
	if (k != num && k != 0) {
		move_bad_mbufs(mb, dr, num, num - k);
		rte_errno = EBADMSG;
	}

	return k;
}

/*
 * process ESP inbound tunnel packet.
 */
static inline int
inb_tun_single_pkt_process(struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
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
inb_trs_single_pkt_process(struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
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
	uint32_t dr[], uint16_t num)
{
	uint32_t i, k;
	struct replay_sqn *rsn;

	rsn = rsn_update_start(sa);

	k = 0;
	for (i = 0; i != num; i++) {
		if (esn_inb_update_sqn(rsn, sa, sqn[i]) == 0)
			k++;
		else
			dr[i - k] = i;
	}

	rsn_update_finish(sa, rsn);
	return k;
}

/*
 * process group of ESP inbound tunnel packets.
 */
uint16_t
esp_inb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	uint32_t i, k, n;
	struct rte_ipsec_sa *sa;
	uint32_t sqn[num];
	uint32_t dr[num];

	sa = ss->sa;

	/* process packets, extract seq numbers */

	k = 0;
	for (i = 0; i != num; i++) {
		/* good packet */
		if (inb_tun_single_pkt_process(sa, mb[i], sqn + k) == 0)
			k++;
		/* bad packet, will drop from furhter processing */
		else
			dr[i - k] = i;
	}

	/* handle unprocessed mbufs */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	/* update SQN and replay winow */
	n = esp_inb_rsn_update(sa, sqn, dr, k);

	/* handle mbufs with wrong SQN */
	if (n != k && n != 0)
		move_bad_mbufs(mb, dr, k, k - n);

	if (n != num)
		rte_errno = EBADMSG;

	return n;
}

/*
 * process group of ESP inbound transport packets.
 */
uint16_t
esp_inb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	uint32_t i, k, n;
	uint32_t sqn[num];
	struct rte_ipsec_sa *sa;
	uint32_t dr[num];

	sa = ss->sa;

	/* process packets, extract seq numbers */

	k = 0;
	for (i = 0; i != num; i++) {
		/* good packet */
		if (inb_trs_single_pkt_process(sa, mb[i], sqn + k) == 0)
			k++;
		/* bad packet, will drop from furhter processing */
		else
			dr[i - k] = i;
	}

	/* handle unprocessed mbufs */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	/* update SQN and replay winow */
	n = esp_inb_rsn_update(sa, sqn, dr, k);

	/* handle mbufs with wrong SQN */
	if (n != k && n != 0)
		move_bad_mbufs(mb, dr, k, k - n);

	if (n != num)
		rte_errno = EBADMSG;

	return n;
}
