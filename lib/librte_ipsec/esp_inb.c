/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2020 Intel Corporation
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

typedef uint16_t (*esp_inb_process_t)(const struct rte_ipsec_sa *sa,
	struct rte_mbuf *mb[], uint32_t sqn[], uint32_t dr[], uint16_t num,
	uint8_t sqh_len);

/*
 * helper function to fill crypto_sym op for cipher+auth algorithms.
 * used by inb_cop_prepare(), see below.
 */
static inline void
sop_ciph_auth_prepare(struct rte_crypto_sym_op *sop,
	const struct rte_ipsec_sa *sa, const union sym_op_data *icv,
	uint32_t pofs, uint32_t plen)
{
	sop->cipher.data.offset = pofs + sa->ctp.cipher.offset;
	sop->cipher.data.length = plen - sa->ctp.cipher.length;
	sop->auth.data.offset = pofs + sa->ctp.auth.offset;
	sop->auth.data.length = plen - sa->ctp.auth.length;
	sop->auth.digest.data = icv->va;
	sop->auth.digest.phys_addr = icv->pa;
}

/*
 * helper function to fill crypto_sym op for aead algorithms
 * used by inb_cop_prepare(), see below.
 */
static inline void
sop_aead_prepare(struct rte_crypto_sym_op *sop,
	const struct rte_ipsec_sa *sa, const union sym_op_data *icv,
	uint32_t pofs, uint32_t plen)
{
	sop->aead.data.offset = pofs + sa->ctp.cipher.offset;
	sop->aead.data.length = plen - sa->ctp.cipher.length;
	sop->aead.digest.data = icv->va;
	sop->aead.digest.phys_addr = icv->pa;
	sop->aead.aad.data = icv->va + sa->icv_len;
	sop->aead.aad.phys_addr = icv->pa + sa->icv_len;
}

/*
 * setup crypto op and crypto sym op for ESP inbound packet.
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
	ivp = rte_pktmbuf_mtod_offset(mb, uint64_t *,
		pofs + sizeof(struct rte_esp_hdr));

	/* fill sym op fields */
	sop = cop->sym;

	switch (algo) {
	case ALGO_TYPE_AES_GCM:
		sop_aead_prepare(sop, sa, icv, pofs, plen);

		/* fill AAD IV (located inside crypto op) */
		gcm = rte_crypto_op_ctod_offset(cop, struct aead_gcm_iv *,
			sa->iv_ofs);
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CBC:
	case ALGO_TYPE_3DES_CBC:
		sop_ciph_auth_prepare(sop, sa, icv, pofs, plen);

		/* copy iv from the input packet to the cop */
		ivc = rte_crypto_op_ctod_offset(cop, uint64_t *, sa->iv_ofs);
		copy_iv(ivc, ivp, sa->iv_len);
		break;
	case ALGO_TYPE_AES_CTR:
		sop_ciph_auth_prepare(sop, sa, icv, pofs, plen);

		/* fill CTR block (located inside crypto op) */
		ctr = rte_crypto_op_ctod_offset(cop, struct aesctr_cnt_blk *,
			sa->iv_ofs);
		aes_ctr_cnt_blk_fill(ctr, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_NULL:
		sop_ciph_auth_prepare(sop, sa, icv, pofs, plen);
		break;
	}
}

static inline uint32_t
inb_cpu_crypto_prepare(const struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
	uint32_t *pofs, uint32_t plen, void *iv)
{
	struct aead_gcm_iv *gcm;
	struct aesctr_cnt_blk *ctr;
	uint64_t *ivp;
	uint32_t clen;

	ivp = rte_pktmbuf_mtod_offset(mb, uint64_t *,
		*pofs + sizeof(struct rte_esp_hdr));
	clen = 0;

	switch (sa->algo_type) {
	case ALGO_TYPE_AES_GCM:
		gcm = (struct aead_gcm_iv *)iv;
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CBC:
	case ALGO_TYPE_3DES_CBC:
		copy_iv(iv, ivp, sa->iv_len);
		break;
	case ALGO_TYPE_AES_CTR:
		ctr = (struct aesctr_cnt_blk *)iv;
		aes_ctr_cnt_blk_fill(ctr, ivp[0], sa->salt);
		break;
	}

	*pofs += sa->ctp.auth.offset;
	clen = plen - sa->ctp.auth.length;
	return clen;
}

/*
 * Helper function for prepare() to deal with situation when
 * ICV is spread by two segments. Tries to move ICV completely into the
 * last segment.
 */
static struct rte_mbuf *
move_icv(struct rte_mbuf *ml, uint32_t ofs)
{
	uint32_t n;
	struct rte_mbuf *ms;
	const void *prev;
	void *new;

	ms = ml->next;
	n = ml->data_len - ofs;

	prev = rte_pktmbuf_mtod_offset(ml, const void *, ofs);
	new = rte_pktmbuf_prepend(ms, n);
	if (new == NULL)
		return NULL;

	/* move n ICV bytes from ml into ms */
	rte_memcpy(new, prev, n);
	ml->data_len -= n;

	return ms;
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

static inline int
inb_get_sqn(const struct rte_ipsec_sa *sa, const struct replay_sqn *rsn,
	struct rte_mbuf *mb, uint32_t hlen, rte_be64_t *sqc)
{
	int32_t rc;
	uint64_t sqn;
	struct rte_esp_hdr *esph;

	esph = rte_pktmbuf_mtod_offset(mb, struct rte_esp_hdr *, hlen);

	/*
	 * retrieve and reconstruct SQN, then check it, then
	 * convert it back into network byte order.
	 */
	sqn = rte_be_to_cpu_32(esph->seq);
	if (IS_ESN(sa))
		sqn = reconstruct_esn(rsn->sqn, sqn, sa->replay.win_sz);
	*sqc = rte_cpu_to_be_64(sqn);

	/* check IPsec window */
	rc = esn_inb_check_sqn(rsn, sa, sqn);

	return rc;
}

/* prepare packet for upcoming processing */
static inline int32_t
inb_prepare(const struct rte_ipsec_sa *sa, struct rte_mbuf *mb,
	uint32_t hlen, union sym_op_data *icv)
{
	uint32_t clen, icv_len, icv_ofs, plen;
	struct rte_mbuf *ml;

	/* start packet manipulation */
	plen = mb->pkt_len;
	plen = plen - hlen;

	/* check that packet has a valid length */
	clen = plen - sa->ctp.cipher.length;
	if ((int32_t)clen < 0 || (clen & (sa->pad_align - 1)) != 0)
		return -EBADMSG;

	/* find ICV location */
	icv_len = sa->icv_len;
	icv_ofs = mb->pkt_len - icv_len;

	ml = mbuf_get_seg_ofs(mb, &icv_ofs);

	/*
	 * if ICV is spread by two segments, then try to
	 * move ICV completely into the last segment.
	 */
	if (ml->data_len < icv_ofs + icv_len) {

		ml = move_icv(ml, icv_ofs);
		if (ml == NULL)
			return -ENOSPC;

		/* new ICV location */
		icv_ofs = 0;
	}

	icv_ofs += sa->sqh_len;

	/*
	 * we have to allocate space for AAD somewhere,
	 * right now - just use free trailing space at the last segment.
	 * Would probably be more convenient to reserve space for AAD
	 * inside rte_crypto_op itself
	 * (again for IV space is already reserved inside cop).
	 */
	if (sa->aad_len + sa->sqh_len > rte_pktmbuf_tailroom(ml))
		return -ENOSPC;

	icv->va = rte_pktmbuf_mtod_offset(ml, void *, icv_ofs);
	icv->pa = rte_pktmbuf_iova_offset(ml, icv_ofs);

	/*
	 * if esn is used then high-order 32 bits are also used in ICV
	 * calculation but are not transmitted, update packet length
	 * to be consistent with auth data length and offset, this will
	 * be subtracted from packet length in post crypto processing
	 */
	mb->pkt_len += sa->sqh_len;
	ml->data_len += sa->sqh_len;

	return plen;
}

static inline int32_t
inb_pkt_prepare(const struct rte_ipsec_sa *sa, const struct replay_sqn *rsn,
	struct rte_mbuf *mb, uint32_t hlen, union sym_op_data *icv)
{
	int rc;
	rte_be64_t sqn;

	rc = inb_get_sqn(sa, rsn, mb, hlen, &sqn);
	if (rc != 0)
		return rc;

	rc = inb_prepare(sa, mb, hlen, icv);
	if (rc < 0)
		return rc;

	inb_pkt_xprepare(sa, sqn, icv);
	return rc;
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
		} else {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	rsn_release(sa, rsn);

	/* copy not prepared mbufs beyond good ones */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	return k;
}

/*
 * Start with processing inbound packet.
 * This is common part for both tunnel and transport mode.
 * Extract information that will be needed later from mbuf metadata and
 * actual packet data:
 * - mbuf for packet's last segment
 * - length of the L2/L3 headers
 * - esp tail structure
 */
static inline void
process_step1(struct rte_mbuf *mb, uint32_t tlen, struct rte_mbuf **ml,
	struct rte_esp_tail *espt, uint32_t *hlen, uint32_t *tofs)
{
	const struct rte_esp_tail *pt;
	uint32_t ofs;

	ofs = mb->pkt_len - tlen;
	hlen[0] = mb->l2_len + mb->l3_len;
	ml[0] = mbuf_get_seg_ofs(mb, &ofs);
	pt = rte_pktmbuf_mtod_offset(ml[0], const struct rte_esp_tail *, ofs);
	tofs[0] = ofs;
	espt[0] = pt[0];
}

/*
 * Helper function to check pad bytes values.
 * Note that pad bytes can be spread across multiple segments.
 */
static inline int
check_pad_bytes(struct rte_mbuf *mb, uint32_t ofs, uint32_t len)
{
	const uint8_t *pd;
	uint32_t k, n;

	for (n = 0; n != len; n += k, mb = mb->next) {
		k = mb->data_len - ofs;
		k = RTE_MIN(k, len - n);
		pd = rte_pktmbuf_mtod_offset(mb, const uint8_t *, ofs);
		if (memcmp(pd, esp_pad_bytes + n, k) != 0)
			break;
		ofs = 0;
	}

	return len - n;
}

/*
 * packet checks for transport mode:
 * - no reported IPsec related failures in ol_flags
 * - tail and header lengths are valid
 * - padding bytes are valid
 * apart from checks, function also updates tail offset (and segment)
 * by taking into account pad length.
 */
static inline int32_t
trs_process_check(struct rte_mbuf *mb, struct rte_mbuf **ml,
	uint32_t *tofs, struct rte_esp_tail espt, uint32_t hlen, uint32_t tlen)
{
	if ((mb->ol_flags & PKT_RX_SEC_OFFLOAD_FAILED) != 0 ||
			tlen + hlen > mb->pkt_len)
		return -EBADMSG;

	/* padding bytes are spread over multiple segments */
	if (tofs[0] < espt.pad_len) {
		tofs[0] = mb->pkt_len - tlen;
		ml[0] = mbuf_get_seg_ofs(mb, tofs);
	} else
		tofs[0] -= espt.pad_len;

	return check_pad_bytes(ml[0], tofs[0], espt.pad_len);
}

/*
 * packet checks for tunnel mode:
 * - same as for transport mode
 * - esp tail next proto contains expected for that SA value
 */
static inline int32_t
tun_process_check(struct rte_mbuf *mb, struct rte_mbuf **ml,
	uint32_t *tofs, struct rte_esp_tail espt, uint32_t hlen, uint32_t tlen,
	uint8_t proto)
{
	return (trs_process_check(mb, ml, tofs, espt, hlen, tlen) ||
		espt.next_proto != proto);
}

/*
 * step two for tunnel mode:
 * - read SQN value (for future use)
 * - cut of ICV, ESP tail and padding bytes
 * - cut of ESP header and IV, also if needed - L2/L3 headers
 *   (controlled by *adj* value)
 */
static inline void *
tun_process_step2(struct rte_mbuf *mb, struct rte_mbuf *ml, uint32_t hlen,
	uint32_t adj, uint32_t tofs, uint32_t tlen, uint32_t *sqn)
{
	const struct rte_esp_hdr *ph;

	/* read SQN value */
	ph = rte_pktmbuf_mtod_offset(mb, const struct rte_esp_hdr *, hlen);
	sqn[0] = ph->seq;

	/* cut of ICV, ESP tail and padding bytes */
	mbuf_cut_seg_ofs(mb, ml, tofs, tlen);

	/* cut of L2/L3 headers, ESP header and IV */
	return rte_pktmbuf_adj(mb, adj);
}

/*
 * step two for transport mode:
 * - read SQN value (for future use)
 * - cut of ICV, ESP tail and padding bytes
 * - cut of ESP header and IV
 * - move L2/L3 header to fill the gap after ESP header removal
 */
static inline void *
trs_process_step2(struct rte_mbuf *mb, struct rte_mbuf *ml, uint32_t hlen,
	uint32_t adj, uint32_t tofs, uint32_t tlen, uint32_t *sqn)
{
	char *np, *op;

	/* get start of the packet before modifications */
	op = rte_pktmbuf_mtod(mb, char *);

	/* cut off ESP header and IV */
	np = tun_process_step2(mb, ml, hlen, adj, tofs, tlen, sqn);

	/* move header bytes to fill the gap after ESP header removal */
	remove_esph(np, op, hlen);
	return np;
}

/*
 * step three for transport mode:
 * update mbuf metadata:
 * - packet_type
 * - ol_flags
 */
static inline void
trs_process_step3(struct rte_mbuf *mb)
{
	/* reset mbuf packet type */
	mb->packet_type &= (RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK);

	/* clear the PKT_RX_SEC_OFFLOAD flag if set */
	mb->ol_flags &= ~PKT_RX_SEC_OFFLOAD;
}

/*
 * step three for tunnel mode:
 * update mbuf metadata:
 * - packet_type
 * - ol_flags
 * - tx_offload
 */
static inline void
tun_process_step3(struct rte_mbuf *mb, uint64_t txof_msk, uint64_t txof_val)
{
	/* reset mbuf metadata: L2/L3 len, packet type */
	mb->packet_type = RTE_PTYPE_UNKNOWN;
	mb->tx_offload = (mb->tx_offload & txof_msk) | txof_val;

	/* clear the PKT_RX_SEC_OFFLOAD flag if set */
	mb->ol_flags &= ~PKT_RX_SEC_OFFLOAD;
}

/*
 * *process* function for tunnel packets
 */
static inline uint16_t
tun_process(const struct rte_ipsec_sa *sa, struct rte_mbuf *mb[],
	    uint32_t sqn[], uint32_t dr[], uint16_t num, uint8_t sqh_len)
{
	uint32_t adj, i, k, tl;
	uint32_t hl[num], to[num];
	struct rte_esp_tail espt[num];
	struct rte_mbuf *ml[num];
	const void *outh;
	void *inh;

	/*
	 * remove icv, esp trailer and high-order
	 * 32 bits of esn from packet length
	 */
	const uint32_t tlen = sa->icv_len + sizeof(espt[0]) + sqh_len;
	const uint32_t cofs = sa->ctp.cipher.offset;

	/*
	 * to minimize stalls due to load latency,
	 * read mbufs metadata and esp tail first.
	 */
	for (i = 0; i != num; i++)
		process_step1(mb[i], tlen, &ml[i], &espt[i], &hl[i], &to[i]);

	k = 0;
	for (i = 0; i != num; i++) {

		adj = hl[i] + cofs;
		tl = tlen + espt[i].pad_len;

		/* check that packet is valid */
		if (tun_process_check(mb[i], &ml[i], &to[i], espt[i], adj, tl,
					sa->proto) == 0) {

			outh = rte_pktmbuf_mtod_offset(mb[i], uint8_t *,
					mb[i]->l2_len);

			/* modify packet's layout */
			inh = tun_process_step2(mb[i], ml[i], hl[i], adj,
					to[i], tl, sqn + k);

			/* update inner ip header */
			update_tun_inb_l3hdr(sa, outh, inh);

			/* update mbuf's metadata */
			tun_process_step3(mb[i], sa->tx_offload.msk,
				sa->tx_offload.val);
			k++;
		} else
			dr[i - k] = i;
	}

	return k;
}

/*
 * *process* function for tunnel packets
 */
static inline uint16_t
trs_process(const struct rte_ipsec_sa *sa, struct rte_mbuf *mb[],
	uint32_t sqn[], uint32_t dr[], uint16_t num, uint8_t sqh_len)
{
	char *np;
	uint32_t i, k, l2, tl;
	uint32_t hl[num], to[num];
	struct rte_esp_tail espt[num];
	struct rte_mbuf *ml[num];

	/*
	 * remove icv, esp trailer and high-order
	 * 32 bits of esn from packet length
	 */
	const uint32_t tlen = sa->icv_len + sizeof(espt[0]) + sqh_len;
	const uint32_t cofs = sa->ctp.cipher.offset;

	/*
	 * to minimize stalls due to load latency,
	 * read mbufs metadata and esp tail first.
	 */
	for (i = 0; i != num; i++)
		process_step1(mb[i], tlen, &ml[i], &espt[i], &hl[i], &to[i]);

	k = 0;
	for (i = 0; i != num; i++) {

		tl = tlen + espt[i].pad_len;
		l2 = mb[i]->l2_len;

		/* check that packet is valid */
		if (trs_process_check(mb[i], &ml[i], &to[i], espt[i],
				hl[i] + cofs, tl) == 0) {

			/* modify packet's layout */
			np = trs_process_step2(mb[i], ml[i], hl[i], cofs,
				to[i], tl, sqn + k);
			update_trs_l3hdr(sa, np + l2, mb[i]->pkt_len,
				l2, hl[i] - l2, espt[i].next_proto);

			/* update mbuf's metadata */
			trs_process_step3(mb[i]);
			k++;
		} else
			dr[i - k] = i;
	}

	return k;
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

	/* replay not enabled */
	if (sa->replay.win_sz == 0)
		return num;

	rsn = rsn_update_start(sa);

	k = 0;
	for (i = 0; i != num; i++) {
		if (esn_inb_update_sqn(rsn, sa, rte_be_to_cpu_32(sqn[i])) == 0)
			k++;
		else
			dr[i - k] = i;
	}

	rsn_update_finish(sa, rsn);
	return k;
}

/*
 * process group of ESP inbound packets.
 */
static inline uint16_t
esp_inb_pkt_process(struct rte_ipsec_sa *sa, struct rte_mbuf *mb[],
	uint16_t num, uint8_t sqh_len, esp_inb_process_t process)
{
	uint32_t k, n;
	uint32_t sqn[num];
	uint32_t dr[num];

	/* process packets, extract seq numbers */
	k = process(sa, mb, sqn, dr, num, sqh_len);

	/* handle unprocessed mbufs */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	/* update SQN and replay window */
	n = esp_inb_rsn_update(sa, sqn, dr, k);

	/* handle mbufs with wrong SQN */
	if (n != k && n != 0)
		move_bad_mbufs(mb, dr, k, k - n);

	if (n != num)
		rte_errno = EBADMSG;

	return n;
}

/*
 * Prepare (plus actual crypto/auth) routine for inbound CPU-CRYPTO
 * (synchronous mode).
 */
uint16_t
cpu_inb_pkt_prepare(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k;
	struct rte_ipsec_sa *sa;
	struct replay_sqn *rsn;
	union sym_op_data icv;
	struct rte_crypto_va_iova_ptr iv[num];
	struct rte_crypto_va_iova_ptr aad[num];
	struct rte_crypto_va_iova_ptr dgst[num];
	uint32_t dr[num];
	uint32_t l4ofs[num];
	uint32_t clen[num];
	uint64_t ivbuf[num][IPSEC_MAX_IV_QWORD];

	sa = ss->sa;

	/* grab rsn lock */
	rsn = rsn_acquire(sa);

	/* do preparation for all packets */
	for (i = 0, k = 0; i != num; i++) {

		/* calculate ESP header offset */
		l4ofs[k] = mb[i]->l2_len + mb[i]->l3_len;

		/* prepare ESP packet for processing */
		rc = inb_pkt_prepare(sa, rsn, mb[i], l4ofs[k], &icv);
		if (rc >= 0) {
			/* get encrypted data offset and length */
			clen[k] = inb_cpu_crypto_prepare(sa, mb[i],
				l4ofs + k, rc, ivbuf[k]);

			/* fill iv, digest and aad */
			iv[k].va = ivbuf[k];
			aad[k].va = icv.va + sa->icv_len;
			dgst[k++].va = icv.va;
		} else {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	/* release rsn lock */
	rsn_release(sa, rsn);

	/* copy not prepared mbufs beyond good ones */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	/* convert mbufs to iovecs and do actual crypto/auth processing */
	if (k != 0)
		cpu_crypto_bulk(ss, sa->cofs, mb, iv, aad, dgst,
			l4ofs, clen, k);
	return k;
}

/*
 * process group of ESP inbound tunnel packets.
 */
uint16_t
esp_inb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	struct rte_ipsec_sa *sa = ss->sa;

	return esp_inb_pkt_process(sa, mb, num, sa->sqh_len, tun_process);
}

uint16_t
inline_inb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	return esp_inb_pkt_process(ss->sa, mb, num, 0, tun_process);
}

/*
 * process group of ESP inbound transport packets.
 */
uint16_t
esp_inb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	struct rte_ipsec_sa *sa = ss->sa;

	return esp_inb_pkt_process(sa, mb, num, sa->sqh_len, trs_process);
}

uint16_t
inline_inb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	return esp_inb_pkt_process(ss->sa, mb, num, 0, trs_process);
}
