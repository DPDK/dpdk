/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2020 Intel Corporation
 */

#include <rte_ipsec.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_errno.h>
#include <rte_cryptodev.h>

#include "sa.h"
#include "ipsec_sqn.h"
#include "crypto.h"
#include "iph.h"
#include "misc.h"
#include "pad.h"

typedef int32_t (*esp_outb_prepare_t)(struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const uint64_t ivp[IPSEC_MAX_IV_QWORD], struct rte_mbuf *mb,
	union sym_op_data *icv, uint8_t sqh_len, uint8_t tso);

/*
 * helper function to fill crypto_sym op for cipher+auth algorithms.
 * used by outb_cop_prepare(), see below.
 */
static inline void
sop_ciph_auth_prepare(struct rte_crypto_sym_op *sop,
	const struct rte_ipsec_sa *sa, const union sym_op_data *icv,
	uint32_t pofs, uint32_t plen)
{
	sop->cipher.data.offset = sa->ctp.cipher.offset + pofs;
	sop->cipher.data.length = sa->ctp.cipher.length + plen;
	sop->auth.data.offset = sa->ctp.auth.offset + pofs;
	sop->auth.data.length = sa->ctp.auth.length + plen;
	sop->auth.digest.data = icv->va;
	sop->auth.digest.phys_addr = icv->pa;
}

/*
 * helper function to fill crypto_sym op for cipher+auth algorithms.
 * used by outb_cop_prepare(), see below.
 */
static inline void
sop_aead_prepare(struct rte_crypto_sym_op *sop,
	const struct rte_ipsec_sa *sa, const union sym_op_data *icv,
	uint32_t pofs, uint32_t plen)
{
	sop->aead.data.offset = sa->ctp.cipher.offset + pofs;
	sop->aead.data.length = sa->ctp.cipher.length + plen;
	sop->aead.digest.data = icv->va;
	sop->aead.digest.phys_addr = icv->pa;
	sop->aead.aad.data = icv->va + sa->icv_len;
	sop->aead.aad.phys_addr = icv->pa + sa->icv_len;
}

/*
 * setup crypto op and crypto sym op for ESP outbound packet.
 */
static inline void
outb_cop_prepare(struct rte_crypto_op *cop,
	const struct rte_ipsec_sa *sa, const uint64_t ivp[IPSEC_MAX_IV_QWORD],
	const union sym_op_data *icv, uint32_t hlen, uint32_t plen)
{
	struct rte_crypto_sym_op *sop;
	struct aead_gcm_iv *gcm;
	struct aead_ccm_iv *ccm;
	struct aead_chacha20_poly1305_iv *chacha20_poly1305;
	struct aesctr_cnt_blk *ctr;
	uint32_t algo;

	algo = sa->algo_type;

	/* fill sym op fields */
	sop = cop->sym;

	switch (algo) {
	case ALGO_TYPE_AES_CBC:
		/* Cipher-Auth (AES-CBC *) case */
	case ALGO_TYPE_3DES_CBC:
		/* Cipher-Auth (3DES-CBC *) case */
	case ALGO_TYPE_NULL:
		/* NULL case */
		sop_ciph_auth_prepare(sop, sa, icv, hlen, plen);
		break;
	case ALGO_TYPE_AES_GMAC:
		/* GMAC case */
		sop_ciph_auth_prepare(sop, sa, icv, hlen, plen);

		/* fill AAD IV (located inside crypto op) */
		gcm = rte_crypto_op_ctod_offset(cop, struct aead_gcm_iv *,
			sa->iv_ofs);
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_GCM:
		/* AEAD (AES_GCM) case */
		sop_aead_prepare(sop, sa, icv, hlen, plen);

		/* fill AAD IV (located inside crypto op) */
		gcm = rte_crypto_op_ctod_offset(cop, struct aead_gcm_iv *,
			sa->iv_ofs);
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CCM:
		/* AEAD (AES_CCM) case */
		sop_aead_prepare(sop, sa, icv, hlen, plen);

		/* fill AAD IV (located inside crypto op) */
		ccm = rte_crypto_op_ctod_offset(cop, struct aead_ccm_iv *,
			sa->iv_ofs);
		aead_ccm_iv_fill(ccm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_CHACHA20_POLY1305:
		/* AEAD (CHACHA20_POLY) case */
		sop_aead_prepare(sop, sa, icv, hlen, plen);

		/* fill AAD IV (located inside crypto op) */
		chacha20_poly1305 = rte_crypto_op_ctod_offset(cop,
			struct aead_chacha20_poly1305_iv *,
			sa->iv_ofs);
		aead_chacha20_poly1305_iv_fill(chacha20_poly1305,
					       ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CTR:
		/* Cipher-Auth (AES-CTR *) case */
		sop_ciph_auth_prepare(sop, sa, icv, hlen, plen);

		/* fill CTR block (located inside crypto op) */
		ctr = rte_crypto_op_ctod_offset(cop, struct aesctr_cnt_blk *,
			sa->iv_ofs);
		aes_ctr_cnt_blk_fill(ctr, ivp[0], sa->salt);
		break;
	}
}

/*
 * setup/update packet data and metadata for ESP outbound tunnel case.
 */
static inline int32_t
outb_tun_pkt_prepare(struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const uint64_t ivp[IPSEC_MAX_IV_QWORD], struct rte_mbuf *mb,
	union sym_op_data *icv, uint8_t sqh_len, uint8_t tso)
{
	uint32_t clen, hlen, l2len, pdlen, pdofs, plen, tlen;
	struct rte_mbuf *ml;
	struct rte_esp_hdr *esph;
	struct rte_esp_tail *espt;
	char *ph, *pt;
	uint64_t *iv;

	/* calculate extra header space required */
	hlen = sa->hdr_len + sa->iv_len + sizeof(*esph);

	/* size of ipsec protected data */
	l2len = mb->l2_len;
	plen = mb->pkt_len - l2len;

	/* number of bytes to encrypt */
	clen = plen + sizeof(*espt);

	if (!tso) {
		clen = RTE_ALIGN_CEIL(clen, sa->pad_align);
		/* pad length + esp tail */
		pdlen = clen - plen;
		tlen = pdlen + sa->icv_len + sqh_len;
	} else {
		/* We don't need to pad/align packet or append ICV length
		 * when using TSO offload
		 */
		pdlen = clen - plen;
		tlen = pdlen + sqh_len;
	}

	/* do append and prepend */
	ml = rte_pktmbuf_lastseg(mb);
	if (tlen + sa->aad_len > rte_pktmbuf_tailroom(ml))
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

	/* if UDP encap is enabled update the dgram_len */
	if (sa->type & RTE_IPSEC_SATP_NATT_ENABLE) {
		struct rte_udp_hdr *udph = (struct rte_udp_hdr *)
			(ph + sa->hdr_len - sizeof(struct rte_udp_hdr));
		udph->dgram_len = rte_cpu_to_be_16(mb->pkt_len - sqh_len -
				sa->hdr_l3_off - sa->hdr_len);
	}

	/* update original and new ip header fields */
	update_tun_outb_l3hdr(sa, ph + sa->hdr_l3_off, ph + hlen,
			mb->pkt_len - sqh_len, sa->hdr_l3_off, sqn_low16(sqc));

	/* update spi, seqn and iv */
	esph = (struct rte_esp_hdr *)(ph + sa->hdr_len);
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
	espt = (struct rte_esp_tail *)(pt + pdlen);
	espt->pad_len = pdlen;
	espt->next_proto = sa->proto;

	/* set icv va/pa value(s) */
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
	struct aead_gcm_aad *gaad;
	struct aead_ccm_aad *caad;
	struct aead_chacha20_poly1305_aad *chacha20_poly1305_aad;

	/* insert SQN.hi between ESP trailer and ICV */
	if (sa->sqh_len != 0) {
		psqh = (uint32_t *)(icv->va - sa->sqh_len);
		psqh[0] = sqn_hi32(sqc);
	}

	/*
	 * fill IV and AAD fields, if any (aad fields are placed after icv),
	 * right now we support only one AEAD algorithm: AES-GCM .
	 */
	switch (sa->algo_type) {
	case ALGO_TYPE_AES_GCM:
	if (sa->aad_len != 0) {
		gaad = (struct aead_gcm_aad *)(icv->va + sa->icv_len);
		aead_gcm_aad_fill(gaad, sa->spi, sqc, IS_ESN(sa));
	}
		break;
	case ALGO_TYPE_AES_CCM:
	if (sa->aad_len != 0) {
		caad = (struct aead_ccm_aad *)(icv->va + sa->icv_len);
		aead_ccm_aad_fill(caad, sa->spi, sqc, IS_ESN(sa));
	}
		break;
	case ALGO_TYPE_CHACHA20_POLY1305:
	if (sa->aad_len != 0) {
		chacha20_poly1305_aad =	(struct aead_chacha20_poly1305_aad *)
			(icv->va + sa->icv_len);
		aead_chacha20_poly1305_aad_fill(chacha20_poly1305_aad,
			sa->spi, sqc, IS_ESN(sa));
	}
		break;
	default:
		break;
	}
}

/*
 * setup/update packets and crypto ops for ESP outbound tunnel case.
 */
uint16_t
esp_outb_tun_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, n;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	struct rte_cryptodev_sym_session *cs;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	uint32_t dr[num];

	sa = ss->sa;
	cs = ss->crypto.ses;

	n = num;
	sqn = esn_outb_update_sqn(sa, &n);
	if (n != num)
		rte_errno = EOVERFLOW;

	k = 0;
	for (i = 0; i != n; i++) {

		sqc = rte_cpu_to_be_64(sqn + i);
		gen_iv(iv, sqc);

		/* try to update the packet itself */
		rc = outb_tun_pkt_prepare(sa, sqc, iv, mb[i], &icv,
					  sa->sqh_len, 0);
		/* success, setup crypto op */
		if (rc >= 0) {
			outb_pkt_xprepare(sa, sqc, &icv);
			lksd_none_cop_prepare(cop[k], cs, mb[i]);
			outb_cop_prepare(cop[k], sa, iv, &icv, 0, rc);
			k++;
		/* failure, put packet into the death-row */
		} else {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	 /* copy not prepared mbufs beyond good ones */
	if (k != n && k != 0)
		move_bad_mbufs(mb, dr, n, n - k);

	return k;
}

/*
 * setup/update packet data and metadata for ESP outbound transport case.
 */
static inline int32_t
outb_trs_pkt_prepare(struct rte_ipsec_sa *sa, rte_be64_t sqc,
	const uint64_t ivp[IPSEC_MAX_IV_QWORD], struct rte_mbuf *mb,
	union sym_op_data *icv, uint8_t sqh_len, uint8_t tso)
{
	uint8_t np;
	uint32_t clen, hlen, pdlen, pdofs, plen, tlen, uhlen;
	struct rte_mbuf *ml;
	struct rte_esp_hdr *esph;
	struct rte_esp_tail *espt;
	char *ph, *pt;
	uint64_t *iv;
	uint32_t l2len, l3len;

	l2len = mb->l2_len;
	l3len = mb->l3_len;

	uhlen = l2len + l3len;
	plen = mb->pkt_len - uhlen;

	/* calculate extra header space required */
	hlen = sa->iv_len + sizeof(*esph);

	/* number of bytes to encrypt */
	clen = plen + sizeof(*espt);

	if (!tso) {
		clen = RTE_ALIGN_CEIL(clen, sa->pad_align);
		/* pad length + esp tail */
		pdlen = clen - plen;
		tlen = pdlen + sa->icv_len + sqh_len;
	} else {
		/* We don't need to pad/align packet or append ICV length
		 * when using TSO offload
		 */
		pdlen = clen - plen;
		tlen = pdlen + sqh_len;
	}

	/* do append and insert */
	ml = rte_pktmbuf_lastseg(mb);
	if (tlen + sa->aad_len > rte_pktmbuf_tailroom(ml))
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
	np = update_trs_l3hdr(sa, ph + l2len, mb->pkt_len - sqh_len, l2len,
			l3len, IPPROTO_ESP);

	/* update spi, seqn and iv */
	esph = (struct rte_esp_hdr *)(ph + uhlen);
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
	espt = (struct rte_esp_tail *)(pt + pdlen);
	espt->pad_len = pdlen;
	espt->next_proto = np;

	/* set icv va/pa value(s) */
	icv->va = rte_pktmbuf_mtod_offset(ml, void *, pdofs);
	icv->pa = rte_pktmbuf_iova_offset(ml, pdofs);

	return clen;
}

/*
 * setup/update packets and crypto ops for ESP outbound transport case.
 */
uint16_t
esp_outb_trs_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, n, l2, l3;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	struct rte_cryptodev_sym_session *cs;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	uint32_t dr[num];

	sa = ss->sa;
	cs = ss->crypto.ses;

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
		rc = outb_trs_pkt_prepare(sa, sqc, iv, mb[i], &icv,
				  sa->sqh_len, 0);
		/* success, setup crypto op */
		if (rc >= 0) {
			outb_pkt_xprepare(sa, sqc, &icv);
			lksd_none_cop_prepare(cop[k], cs, mb[i]);
			outb_cop_prepare(cop[k], sa, iv, &icv, l2 + l3, rc);
			k++;
		/* failure, put packet into the death-row */
		} else {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	/* copy not prepared mbufs beyond good ones */
	if (k != n && k != 0)
		move_bad_mbufs(mb, dr, n, n - k);

	return k;
}


static inline uint32_t
outb_cpu_crypto_prepare(const struct rte_ipsec_sa *sa, uint32_t *pofs,
	uint32_t plen, void *iv)
{
	uint64_t *ivp = iv;
	struct aead_gcm_iv *gcm;
	struct aead_ccm_iv *ccm;
	struct aead_chacha20_poly1305_iv *chacha20_poly1305;
	struct aesctr_cnt_blk *ctr;
	uint32_t clen;

	switch (sa->algo_type) {
	case ALGO_TYPE_AES_GCM:
		gcm = iv;
		aead_gcm_iv_fill(gcm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CCM:
		ccm = iv;
		aead_ccm_iv_fill(ccm, ivp[0], sa->salt);
		break;
	case ALGO_TYPE_CHACHA20_POLY1305:
		chacha20_poly1305 = iv;
		aead_chacha20_poly1305_iv_fill(chacha20_poly1305,
					       ivp[0], sa->salt);
		break;
	case ALGO_TYPE_AES_CTR:
		ctr = iv;
		aes_ctr_cnt_blk_fill(ctr, ivp[0], sa->salt);
		break;
	}

	*pofs += sa->ctp.auth.offset;
	clen = plen + sa->ctp.auth.length;
	return clen;
}

static uint16_t
cpu_outb_pkt_prepare(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num,
		esp_outb_prepare_t prepare, uint32_t cofs_mask)
{
	int32_t rc;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	uint32_t i, k, n;
	uint32_t l2, l3;
	union sym_op_data icv;
	struct rte_crypto_va_iova_ptr iv[num];
	struct rte_crypto_va_iova_ptr aad[num];
	struct rte_crypto_va_iova_ptr dgst[num];
	uint32_t dr[num];
	uint32_t l4ofs[num];
	uint32_t clen[num];
	uint64_t ivbuf[num][IPSEC_MAX_IV_QWORD];

	sa = ss->sa;

	n = num;
	sqn = esn_outb_update_sqn(sa, &n);
	if (n != num)
		rte_errno = EOVERFLOW;

	for (i = 0, k = 0; i != n; i++) {

		l2 = mb[i]->l2_len;
		l3 = mb[i]->l3_len;

		/* calculate ESP header offset */
		l4ofs[k] = (l2 + l3) & cofs_mask;

		sqc = rte_cpu_to_be_64(sqn + i);
		gen_iv(ivbuf[k], sqc);

		/* try to update the packet itself */
		rc = prepare(sa, sqc, ivbuf[k], mb[i], &icv, sa->sqh_len, 0);

		/* success, proceed with preparations */
		if (rc >= 0) {

			outb_pkt_xprepare(sa, sqc, &icv);

			/* get encrypted data offset and length */
			clen[k] = outb_cpu_crypto_prepare(sa, l4ofs + k, rc,
				ivbuf[k]);

			/* fill iv, digest and aad */
			iv[k].va = ivbuf[k];
			aad[k].va = icv.va + sa->icv_len;
			dgst[k++].va = icv.va;
		} else {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	/* copy not prepared mbufs beyond good ones */
	if (k != n && k != 0)
		move_bad_mbufs(mb, dr, n, n - k);

	/* convert mbufs to iovecs and do actual crypto/auth processing */
	if (k != 0)
		cpu_crypto_bulk(ss, sa->cofs, mb, iv, aad, dgst,
			l4ofs, clen, k);
	return k;
}

uint16_t
cpu_outb_tun_pkt_prepare(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num)
{
	return cpu_outb_pkt_prepare(ss, mb, num, outb_tun_pkt_prepare, 0);
}

uint16_t
cpu_outb_trs_pkt_prepare(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num)
{
	return cpu_outb_pkt_prepare(ss, mb, num, outb_trs_pkt_prepare,
		UINT32_MAX);
}

/*
 * process outbound packets for SA with ESN support,
 * for algorithms that require SQN.hibits to be implicitly included
 * into digest computation.
 * In that case we have to move ICV bytes back to their proper place.
 */
uint16_t
esp_outb_sqh_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num)
{
	uint32_t i, k, icv_len, *icv, bytes;
	struct rte_mbuf *ml;
	struct rte_ipsec_sa *sa;
	uint32_t dr[num];

	sa = ss->sa;

	k = 0;
	icv_len = sa->icv_len;
	bytes = 0;

	for (i = 0; i != num; i++) {
		if ((mb[i]->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED) == 0) {
			ml = rte_pktmbuf_lastseg(mb[i]);
			/* remove high-order 32 bits of esn from packet len */
			mb[i]->pkt_len -= sa->sqh_len;
			ml->data_len -= sa->sqh_len;
			icv = rte_pktmbuf_mtod_offset(ml, void *,
				ml->data_len - icv_len);
			remove_sqh(icv, icv_len);
			bytes += mb[i]->pkt_len;
			k++;
		} else
			dr[i - k] = i;
	}
	sa->statistics.count += k;
	sa->statistics.bytes += bytes;

	/* handle unprocessed mbufs */
	if (k != num) {
		rte_errno = EBADMSG;
		if (k != 0)
			move_bad_mbufs(mb, dr, num, num - k);
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
	uint32_t i, ol_flags, bytes;

	ol_flags = ss->security.ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA;
	bytes = 0;
	for (i = 0; i != num; i++) {

		mb[i]->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;
		bytes += mb[i]->pkt_len;
		if (ol_flags != 0)
			rte_security_set_pkt_metadata(ss->security.ctx,
				ss->security.ses, mb[i], NULL);
	}
	ss->sa->statistics.count += num;
	ss->sa->statistics.bytes += bytes;
}


static inline int
esn_outb_nb_segments(struct rte_mbuf *m)
{
	if  (m->ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) {
		uint16_t pkt_l3len = m->pkt_len - m->l2_len;
		uint16_t segments =
			(m->tso_segsz > 0 && pkt_l3len > m->tso_segsz) ?
			(pkt_l3len + m->tso_segsz - 1) / m->tso_segsz : 1;
		return segments;
	}
	return 1; /* no TSO */
}

/* Compute how many packets can be sent before overflow occurs */
static inline uint16_t
esn_outb_nb_valid_packets(uint16_t num, uint32_t n_sqn, uint16_t nb_segs[])
{
	uint16_t i;
	uint32_t seg_cnt = 0;
	for (i = 0; i < num && seg_cnt < n_sqn; i++)
		seg_cnt += nb_segs[i];
	return i - 1;
}

/*
 * process group of ESP outbound tunnel packets destined for
 * INLINE_CRYPTO type of device.
 */
uint16_t
inline_outb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, nb_segs_total, n_sqn;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	uint32_t dr[num];
	uint16_t nb_segs[num];

	sa = ss->sa;
	nb_segs_total = 0;
	/* Calculate number of segments */
	for (i = 0; i != num; i++) {
		nb_segs[i] = esn_outb_nb_segments(mb[i]);
		nb_segs_total += nb_segs[i];
	}

	n_sqn = nb_segs_total;
	sqn = esn_outb_update_sqn(sa, &n_sqn);
	if (n_sqn != nb_segs_total) {
		rte_errno = EOVERFLOW;
		/* if there are segmented packets find out how many can be
		 * sent until overflow occurs
		 */
		if (nb_segs_total > num) /* there is at least 1 */
			num = esn_outb_nb_valid_packets(num, n_sqn, nb_segs);
		else
			num = n_sqn; /* no segmented packets */
	}

	k = 0;
	for (i = 0; i != num; i++) {

		sqc = rte_cpu_to_be_64(sqn);
		gen_iv(iv, sqc);
		sqn += nb_segs[i];

		/* try to update the packet itself */
		rc = outb_tun_pkt_prepare(sa, sqc, iv, mb[i], &icv, 0,
			(mb[i]->ol_flags &
			(RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) != 0);

		k += (rc >= 0);

		/* failure, put packet into the death-row */
		if (rc < 0) {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	/* copy not processed mbufs beyond good ones */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	inline_outb_mbuf_prepare(ss, mb, k);
	return k;
}

/*
 * process group of ESP outbound transport packets destined for
 * INLINE_CRYPTO type of device.
 */
uint16_t
inline_outb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	int32_t rc;
	uint32_t i, k, nb_segs_total, n_sqn;
	uint64_t sqn;
	rte_be64_t sqc;
	struct rte_ipsec_sa *sa;
	union sym_op_data icv;
	uint64_t iv[IPSEC_MAX_IV_QWORD];
	uint32_t dr[num];
	uint16_t nb_segs[num];

	sa = ss->sa;
	nb_segs_total = 0;
	/* Calculate number of segments */
	for (i = 0; i != num; i++) {
		nb_segs[i] = esn_outb_nb_segments(mb[i]);
		nb_segs_total += nb_segs[i];
	}

	n_sqn = nb_segs_total;
	sqn = esn_outb_update_sqn(sa, &n_sqn);
	if (n_sqn != nb_segs_total) {
		rte_errno = EOVERFLOW;
		/* if there are segmented packets find out how many can be
		 * sent until overflow occurs
		 */
		if (nb_segs_total > num) /* there is at least 1 */
			num = esn_outb_nb_valid_packets(num, n_sqn, nb_segs);
		else
			num = n_sqn; /* no segmented packets */
	}

	k = 0;
	for (i = 0; i != num; i++) {

		sqc = rte_cpu_to_be_64(sqn);
		gen_iv(iv, sqc);
		sqn += nb_segs[i];

		/* try to update the packet itself */
		rc = outb_trs_pkt_prepare(sa, sqc, iv, mb[i], &icv, 0,
			(mb[i]->ol_flags &
			(RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) != 0);

		k += (rc >= 0);

		/* failure, put packet into the death-row */
		if (rc < 0) {
			dr[i - k] = i;
			rte_errno = -rc;
		}
	}

	/* copy not processed mbufs beyond good ones */
	if (k != num && k != 0)
		move_bad_mbufs(mb, dr, num, num - k);

	inline_outb_mbuf_prepare(ss, mb, k);
	return k;
}

/*
 * outbound for RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
 * actual processing is done by HW/PMD, just set flags and metadata.
 */
uint16_t
inline_proto_outb_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num)
{
	inline_outb_mbuf_prepare(ss, mb, num);
	return num;
}
