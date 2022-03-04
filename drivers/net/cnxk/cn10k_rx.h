/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_RX_H__
#define __CN10K_RX_H__

#include <rte_ether.h>
#include <rte_vect.h>

#include <cnxk_ethdev.h>

#define NIX_RX_OFFLOAD_NONE	     (0)
#define NIX_RX_OFFLOAD_RSS_F	     BIT(0)
#define NIX_RX_OFFLOAD_PTYPE_F	     BIT(1)
#define NIX_RX_OFFLOAD_CHECKSUM_F    BIT(2)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F BIT(3)
#define NIX_RX_OFFLOAD_TSTAMP_F	     BIT(4)
#define NIX_RX_OFFLOAD_VLAN_STRIP_F  BIT(5)
#define NIX_RX_OFFLOAD_SECURITY_F    BIT(6)
#define NIX_RX_OFFLOAD_MAX	     (NIX_RX_OFFLOAD_SECURITY_F << 1)

/* Flags to control cqe_to_mbuf conversion function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_RX_REAS_F	   BIT(12)
#define NIX_RX_VWQE_F	   BIT(13)
#define NIX_RX_MULTI_SEG_F BIT(14)
#define CPT_RX_WQE_F	   BIT(15)

#define CNXK_NIX_CQ_ENTRY_SZ 128
#define NIX_DESCS_PER_LOOP   4
#define CQE_CAST(x)	     ((struct nix_cqe_hdr_s *)(x))
#define CQE_SZ(x)	     ((x) * CNXK_NIX_CQ_ENTRY_SZ)

#define CQE_PTR_OFF(b, i, o, f)                                                \
	(((f) & NIX_RX_VWQE_F) ?                                               \
		       (uint64_t *)(((uintptr_t)((uint64_t *)(b))[i]) + (o)) : \
		       (uint64_t *)(((uintptr_t)(b)) + CQE_SZ(i) + (o)))
#define CQE_PTR_DIFF(b, i, o, f)                                               \
	(((f) & NIX_RX_VWQE_F) ?                                               \
		 (uint64_t *)(((uintptr_t)((uint64_t *)(b))[i]) - (o)) :       \
		       (uint64_t *)(((uintptr_t)(b)) + CQE_SZ(i) - (o)))

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
static inline void
nix_mbuf_validate_next(struct rte_mbuf *m)
{
	if (m->nb_segs == 1 && m->next) {
		rte_panic("mbuf->next[%p] valid when mbuf->nb_segs is %d",
			m->next, m->nb_segs);
	}
}
#else
static inline void
nix_mbuf_validate_next(struct rte_mbuf *m)
{
	RTE_SET_USED(m);
}
#endif

#define NIX_RX_SEC_REASSEMBLY_F \
	(NIX_RX_REAS_F | NIX_RX_OFFLOAD_SECURITY_F)

static inline rte_eth_ip_reassembly_dynfield_t *
cnxk_ip_reassembly_dynfield(struct rte_mbuf *mbuf,
		int ip_reassembly_dynfield_offset)
{
	return RTE_MBUF_DYNFIELD(mbuf, ip_reassembly_dynfield_offset,
				 rte_eth_ip_reassembly_dynfield_t *);
}

union mbuf_initializer {
	struct {
		uint16_t data_off;
		uint16_t refcnt;
		uint16_t nb_segs;
		uint16_t port;
	} fields;
	uint64_t value;
};

static __rte_always_inline uint64_t
nix_clear_data_off(uint64_t oldval)
{
	union mbuf_initializer mbuf_init = {.value = oldval};

	mbuf_init.fields.data_off = 0;
	return mbuf_init.value;
}

static __rte_always_inline struct rte_mbuf *
nix_get_mbuf_from_cqe(void *cq, const uint64_t data_off)
{
	rte_iova_t buff;

	/* Skip CQE, NIX_RX_PARSE_S and SG HDR(9 DWORDs) and peek buff addr */
	buff = *((rte_iova_t *)((uint64_t *)cq + 9));
	return (struct rte_mbuf *)(buff - data_off);
}

static __rte_always_inline void
nix_sec_flush_meta_burst(uint16_t lmt_id, uint64_t data, uint16_t lnum,
			 uintptr_t aura_handle)
{
	uint64_t pa;

	/* Prepare PA and Data */
	pa = roc_npa_aura_handle_to_base(aura_handle) + NPA_LF_AURA_BATCH_FREE0;
	pa |= ((data & 0x7) << 4);

	data >>= 3;
	data <<= 19;
	data |= (uint64_t)lmt_id;
	data |= (uint64_t)(lnum - 1) << 12;

	roc_lmt_submit_steorl(data, pa);
}

static __rte_always_inline void
nix_sec_flush_meta(uintptr_t laddr, uint16_t lmt_id, uint8_t loff,
		   uintptr_t aura_handle)
{
	uint64_t pa;

	/* laddr is pointing to first pointer */
	laddr -= 8;

	/* Trigger free either on lmtline full or different aura handle */
	pa = roc_npa_aura_handle_to_base(aura_handle) + NPA_LF_AURA_BATCH_FREE0;

	/* Update aura handle */
	*(uint64_t *)laddr = (((uint64_t)(loff & 0x1) << 32) |
			      roc_npa_aura_handle_to_aura(aura_handle));

	pa |= ((uint64_t)(loff >> 1) << 4);
	roc_lmt_submit_steorl(lmt_id, pa);
}

static struct rte_mbuf *
nix_sec_attach_frags(const struct cpt_parse_hdr_s *hdr,
		     struct cn10k_inb_priv_data *inb_priv,
		     const uint64_t mbuf_init)
{
	struct rte_mbuf *head, *mbuf, *mbuf_prev;
	uint32_t offset = hdr->w2.fi_offset;
	union nix_rx_parse_u *frag_rx;
	struct cpt_frag_info_s *finfo;
	uint64_t *frag_ptr = NULL;
	uint64_t ol_flags;
	uint16_t frag_size;
	uint16_t rlen;
	uint64_t *wqe;
	int off;

	off = inb_priv->reass_dynfield_off;
	ol_flags = BIT_ULL(inb_priv->reass_dynflag_bit);
	ol_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD;

	/* offset of 0 implies 256B, otherwise it implies offset*8B */
	offset = (((offset - 1) & 0x1f) + 1) * 8;
	finfo = RTE_PTR_ADD(hdr, offset + hdr->w2.fi_pad);

	/* Frag-0: */
	wqe = (uint64_t *)(rte_be_to_cpu_64(hdr->wqe_ptr));
	rlen = ((*(wqe + 10)) >> 16) & 0xFFFF;

	frag_rx = (union nix_rx_parse_u *)(wqe + 1);
	frag_size = rlen + frag_rx->lcptr - frag_rx->laptr;
	frag_rx->pkt_lenm1 = frag_size - 1;

	mbuf = (struct rte_mbuf *)((uintptr_t)wqe - sizeof(struct rte_mbuf));
	*(uint64_t *)(&mbuf->rearm_data) = mbuf_init;
	mbuf->data_len = frag_size;
	mbuf->pkt_len = frag_size;
	mbuf->ol_flags = ol_flags;
	mbuf->next = NULL;
	head = mbuf;
	mbuf_prev = mbuf;
	/* Update dynamic field with userdata */
	*rte_security_dynfield(mbuf) = (uint64_t)inb_priv->userdata;

	cnxk_ip_reassembly_dynfield(head, off)->nb_frags = hdr->w0.num_frags - 1;
	cnxk_ip_reassembly_dynfield(head, off)->next_frag = NULL;

	/* Frag-1: */
	if (hdr->w0.num_frags > 1) {
		wqe = (uint64_t *)(rte_be_to_cpu_64(hdr->frag1_wqe_ptr));
		rlen = ((*(wqe + 10)) >> 16) & 0xFFFF;

		frag_rx = (union nix_rx_parse_u *)(wqe + 1);
		frag_size = rlen + frag_rx->lcptr - frag_rx->laptr;
		frag_rx->pkt_lenm1 = frag_size - 1;

		mbuf = (struct rte_mbuf *)((uintptr_t)wqe -
				sizeof(struct rte_mbuf));
		*(uint64_t *)(&mbuf->rearm_data) = mbuf_init;
		mbuf->data_len = frag_size;
		mbuf->pkt_len = frag_size;
		mbuf->ol_flags = ol_flags;
		mbuf->next = NULL;

		/* Update dynamic field with userdata */
		*rte_security_dynfield(mbuf) = (uint64_t)inb_priv->userdata;

		cnxk_ip_reassembly_dynfield(mbuf, off)->nb_frags =
			hdr->w0.num_frags - 2;
		cnxk_ip_reassembly_dynfield(mbuf, off)->next_frag = NULL;
		cnxk_ip_reassembly_dynfield(mbuf_prev, off)->next_frag = mbuf;
		mbuf_prev = mbuf;
	}

	/* Frag-2: */
	if (hdr->w0.num_frags > 2) {
		frag_ptr = (uint64_t *)(finfo + 1);
		wqe = (uint64_t *)(rte_be_to_cpu_64(*frag_ptr));
		rlen = ((*(wqe + 10)) >> 16) & 0xFFFF;

		frag_rx = (union nix_rx_parse_u *)(wqe + 1);
		frag_size = rlen + frag_rx->lcptr - frag_rx->laptr;
		frag_rx->pkt_lenm1 = frag_size - 1;

		mbuf = (struct rte_mbuf *)((uintptr_t)wqe -
				sizeof(struct rte_mbuf));
		*(uint64_t *)(&mbuf->rearm_data) = mbuf_init;
		mbuf->data_len = frag_size;
		mbuf->pkt_len = frag_size;
		mbuf->ol_flags = ol_flags;
		mbuf->next = NULL;

		/* Update dynamic field with userdata */
		*rte_security_dynfield(mbuf) = (uint64_t)inb_priv->userdata;

		cnxk_ip_reassembly_dynfield(mbuf, off)->nb_frags =
			hdr->w0.num_frags - 3;
		cnxk_ip_reassembly_dynfield(mbuf, off)->next_frag = NULL;
		cnxk_ip_reassembly_dynfield(mbuf_prev, off)->next_frag = mbuf;
		mbuf_prev = mbuf;
	}

	/* Frag-3: */
	if (hdr->w0.num_frags > 3) {
		wqe = (uint64_t *)(rte_be_to_cpu_64(*(frag_ptr + 1)));
		rlen = ((*(wqe + 10)) >> 16) & 0xFFFF;

		frag_rx = (union nix_rx_parse_u *)(wqe + 1);
		frag_size = rlen + frag_rx->lcptr - frag_rx->laptr;
		frag_rx->pkt_lenm1 = frag_size - 1;

		mbuf = (struct rte_mbuf *)((uintptr_t)wqe -
				sizeof(struct rte_mbuf));
		*(uint64_t *)(&mbuf->rearm_data) = mbuf_init;
		mbuf->data_len = frag_size;
		mbuf->pkt_len = frag_size;
		mbuf->ol_flags = ol_flags;
		mbuf->next = NULL;

		/* Update dynamic field with userdata */
		*rte_security_dynfield(mbuf) = (uint64_t)inb_priv->userdata;

		cnxk_ip_reassembly_dynfield(mbuf, off)->nb_frags =
			hdr->w0.num_frags - 4;
		cnxk_ip_reassembly_dynfield(mbuf, off)->next_frag = NULL;
		cnxk_ip_reassembly_dynfield(mbuf_prev, off)->next_frag = mbuf;
	}
	return head;
}

static struct rte_mbuf *
nix_sec_reassemble_frags(const struct cpt_parse_hdr_s *hdr, uint64_t cq_w1,
			uint64_t cq_w5, uint64_t mbuf_init)
{
	uint32_t fragx_sum, pkt_hdr_len, l3_hdr_size;
	uint32_t offset = hdr->w2.fi_offset;
	union nix_rx_parse_u *inner_rx;
	uint16_t rlen, data_off, b_off;
	union nix_rx_parse_u *frag_rx;
	struct cpt_frag_info_s *finfo;
	struct rte_mbuf *head, *mbuf;
	uint64_t *frag_ptr = NULL;
	rte_iova_t *inner_iova;
	uint16_t frag_size;
	uint64_t *wqe;

	/* Base data offset */
	b_off = mbuf_init & 0xFFFFUL;
	mbuf_init &= ~0xFFFFUL;

	/* offset of 0 implies 256B, otherwise it implies offset*8B */
	offset = (((offset - 1) & 0x1f) + 1) * 8;
	finfo = RTE_PTR_ADD(hdr, offset + hdr->w2.fi_pad);

	/* Frag-0: */
	wqe = (uint64_t *)rte_be_to_cpu_64(hdr->wqe_ptr);
	inner_rx = (union nix_rx_parse_u *)(wqe + 1);
	inner_iova = (rte_iova_t *)*(wqe + 9);

	/* Update only the upper 28-bits from meta pkt parse info */
	*((uint64_t *)inner_rx) = ((*((uint64_t *)inner_rx) & ((1ULL << 36) - 1)) |
				(cq_w1 & ~((1ULL << 36) - 1)));

	rlen = ((*(wqe + 10)) >> 16) & 0xFFFF;
	frag_size = rlen + ((cq_w5 >> 16) & 0xFF) - (cq_w5 & 0xFF);
	fragx_sum = rte_be_to_cpu_16(finfo->w1.frag_size0);
	pkt_hdr_len = frag_size - fragx_sum;

	mbuf = (struct rte_mbuf *)((uintptr_t)wqe - sizeof(struct rte_mbuf));
	*(uint64_t *)(&mbuf->rearm_data) = mbuf_init | b_off;
	mbuf->data_len = frag_size;
	head = mbuf;

	if (inner_rx->lctype == NPC_LT_LC_IP) {
		struct rte_ipv4_hdr *hdr = (struct rte_ipv4_hdr *)
				RTE_PTR_ADD(inner_iova, inner_rx->lcptr);

		l3_hdr_size = (hdr->version_ihl & 0xf) << 2;
	} else {
		struct rte_ipv6_hdr *hdr = (struct rte_ipv6_hdr *)
				RTE_PTR_ADD(inner_iova, inner_rx->lcptr);
		size_t ext_len = sizeof(struct rte_ipv6_hdr);
		uint8_t *nxt_hdr = (uint8_t *)hdr;
		int nh = hdr->proto;

		l3_hdr_size = 0;
		while (nh != -EINVAL) {
			nxt_hdr += ext_len;
			l3_hdr_size += ext_len;
			nh = rte_ipv6_get_next_ext(nxt_hdr, nh, &ext_len);
		}
	}

	/* Frag-1: */
	wqe = (uint64_t *)(rte_be_to_cpu_64(hdr->frag1_wqe_ptr));
	frag_size = rte_be_to_cpu_16(finfo->w1.frag_size1);
	frag_rx = (union nix_rx_parse_u *)(wqe + 1);

	mbuf->next = (struct rte_mbuf *)((uintptr_t)wqe - sizeof(struct rte_mbuf));
	mbuf = mbuf->next;
	data_off = b_off + frag_rx->lcptr + l3_hdr_size;
	*(uint64_t *)(&mbuf->rearm_data) = mbuf_init | data_off;
	mbuf->data_len = frag_size;
	fragx_sum += frag_size;

	/* Frag-2: */
	if (hdr->w0.num_frags > 2) {
		frag_ptr = (uint64_t *)(finfo + 1);
		wqe = (uint64_t *)(rte_be_to_cpu_64(*frag_ptr));
		frag_size = rte_be_to_cpu_16(finfo->w1.frag_size2);
		frag_rx = (union nix_rx_parse_u *)(wqe + 1);

		mbuf->next = (struct rte_mbuf *)((uintptr_t)wqe - sizeof(struct rte_mbuf));
		mbuf = mbuf->next;
		data_off = b_off + frag_rx->lcptr + l3_hdr_size;
		*(uint64_t *)(&mbuf->rearm_data) = mbuf_init | data_off;
		mbuf->data_len = frag_size;
		fragx_sum += frag_size;
	}

	/* Frag-3: */
	if (hdr->w0.num_frags > 3) {
		wqe = (uint64_t *)(rte_be_to_cpu_64(*(frag_ptr + 1)));
		frag_size = rte_be_to_cpu_16(finfo->w1.frag_size3);
		frag_rx = (union nix_rx_parse_u *)(wqe + 1);

		mbuf->next = (struct rte_mbuf *)((uintptr_t)wqe - sizeof(struct rte_mbuf));
		mbuf = mbuf->next;
		data_off = b_off + frag_rx->lcptr + l3_hdr_size;
		*(uint64_t *)(&mbuf->rearm_data) = mbuf_init | data_off;
		mbuf->data_len = frag_size;
		fragx_sum += frag_size;
	}

	if (inner_rx->lctype == NPC_LT_LC_IP) {
		struct rte_ipv4_hdr *hdr = (struct rte_ipv4_hdr *)
				RTE_PTR_ADD(inner_iova, inner_rx->lcptr);

		hdr->fragment_offset = 0;
		hdr->total_length = rte_cpu_to_be_16(fragx_sum + l3_hdr_size);
		hdr->hdr_checksum = 0;
		hdr->hdr_checksum = rte_ipv4_cksum(hdr);

		inner_rx->pkt_lenm1 = pkt_hdr_len + fragx_sum - 1;
	} else {
		/* Remove the frag header by moving header 8 bytes forward */
		struct rte_ipv6_hdr *hdr = (struct rte_ipv6_hdr *)
				RTE_PTR_ADD(inner_iova, inner_rx->lcptr);

		hdr->payload_len = rte_cpu_to_be_16(fragx_sum + l3_hdr_size -
					8 - sizeof(struct rte_ipv6_hdr));

		rte_memcpy(rte_pktmbuf_mtod_offset(head, void *, 8),
			   rte_pktmbuf_mtod(head, void *),
			   inner_rx->lcptr + sizeof(struct rte_ipv6_hdr));

		inner_rx->pkt_lenm1 = pkt_hdr_len + fragx_sum - 8 - 1;
		head->data_len -= 8;
		head->data_off += 8;
	}
	mbuf->next = NULL;
	head->pkt_len = inner_rx->pkt_lenm1 + 1;
	head->nb_segs = hdr->w0.num_frags;

	return head;
}

static __rte_always_inline struct rte_mbuf *
nix_sec_meta_to_mbuf_sc(uint64_t cq_w1, uint64_t cq_w5, const uint64_t sa_base,
			uintptr_t laddr, uint8_t *loff, struct rte_mbuf *mbuf,
			uint16_t data_off, const uint16_t flags,
			const uint64_t mbuf_init)
{
	const void *__p = (void *)((uintptr_t)mbuf + (uint16_t)data_off);
	const struct cpt_parse_hdr_s *hdr = (const struct cpt_parse_hdr_s *)__p;
	struct cn10k_inb_priv_data *inb_priv;
	struct rte_mbuf *inner = NULL;
	uint64_t res_w1;
	uint32_t sa_idx;
	uint16_t uc_cc;
	uint32_t len;
	void *inb_sa;
	uint64_t w0;

	if ((flags & NIX_RX_REAS_F) && (cq_w1 & BIT(11))) {
		/* Get SPI from CPT_PARSE_S's cookie(already swapped) */
		w0 = hdr->w0.u64;
		sa_idx = w0 >> 32;

		inb_sa = roc_nix_inl_ot_ipsec_inb_sa(sa_base, sa_idx);
		inb_priv = roc_nix_inl_ot_ipsec_inb_sa_sw_rsvd(inb_sa);

		if (!hdr->w0.num_frags) {
			/* No Reassembly or inbound error */
			inner = (struct rte_mbuf *)
				(rte_be_to_cpu_64(hdr->wqe_ptr) -
				 sizeof(struct rte_mbuf));

			/* Update dynamic field with userdata */
			*rte_security_dynfield(inner) =
				(uint64_t)inb_priv->userdata;

			/* CPT result(struct cpt_cn10k_res_s) is at
			 * after first IOVA in meta
			 */
			res_w1 = *((uint64_t *)(&inner[1]) + 10);
			uc_cc = res_w1 & 0xFF;

			/* Calculate inner packet length */
			len = ((res_w1 >> 16) & 0xFFFF) + hdr->w2.il3_off -
				sizeof(struct cpt_parse_hdr_s) - (w0 & 0x7);
			inner->pkt_len = len;
			inner->data_len = len;
			*(uint64_t *)(&inner->rearm_data) = mbuf_init;

			inner->ol_flags = ((uc_cc == CPT_COMP_WARN) ?
					   RTE_MBUF_F_RX_SEC_OFFLOAD :
					   (RTE_MBUF_F_RX_SEC_OFFLOAD |
					    RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED));
		} else if (!(hdr->w0.err_sum) && !(hdr->w0.reas_sts)) {
			/* Reassembly success */
			inner = nix_sec_reassemble_frags(hdr, cq_w1, cq_w5,
							 mbuf_init);

			/* Update dynamic field with userdata */
			*rte_security_dynfield(inner) =
				(uint64_t)inb_priv->userdata;

			/* Assume success */
			inner->ol_flags = RTE_MBUF_F_RX_SEC_OFFLOAD;
		} else {
			/* Reassembly failure */
			inner = nix_sec_attach_frags(hdr, inb_priv, mbuf_init);
		}

		/* Store meta in lmtline to free
		 * Assume all meta's from same aura.
		 */
		*(uint64_t *)(laddr + (*loff << 3)) = (uint64_t)mbuf;
		*loff = *loff + 1;

		return inner;
	} else if (cq_w1 & BIT(11)) {
		inner = (struct rte_mbuf *)(rte_be_to_cpu_64(hdr->wqe_ptr) -
					    sizeof(struct rte_mbuf));

		/* Get SPI from CPT_PARSE_S's cookie(already swapped) */
		w0 = hdr->w0.u64;
		sa_idx = w0 >> 32;

		inb_sa = roc_nix_inl_ot_ipsec_inb_sa(sa_base, sa_idx);
		inb_priv = roc_nix_inl_ot_ipsec_inb_sa_sw_rsvd(inb_sa);

		/* Update dynamic field with userdata */
		*rte_security_dynfield(inner) = (uint64_t)inb_priv->userdata;

		/* Update l2 hdr length first */

		/* CPT result(struct cpt_cn10k_res_s) is at
		 * after first IOVA in meta
		 */
		res_w1 = *((uint64_t *)(&inner[1]) + 10);
		uc_cc = res_w1 & 0xFF;

		/* Calculate inner packet length */
		len = ((res_w1 >> 16) & 0xFFFF) + hdr->w2.il3_off -
			sizeof(struct cpt_parse_hdr_s) - (w0 & 0x7);
		inner->pkt_len = len;
		inner->data_len = len;
		*(uint64_t *)(&inner->rearm_data) = mbuf_init;

		inner->ol_flags = ((uc_cc == CPT_COMP_WARN) ?
				   RTE_MBUF_F_RX_SEC_OFFLOAD :
				   (RTE_MBUF_F_RX_SEC_OFFLOAD |
				    RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED));

		/* Store meta in lmtline to free
		 * Assume all meta's from same aura.
		 */
		*(uint64_t *)(laddr + (*loff << 3)) = (uint64_t)mbuf;
		*loff = *loff + 1;

		/* Mark meta mbuf as put */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 0);

		/* Mark inner mbuf as get */
		RTE_MEMPOOL_CHECK_COOKIES(inner->pool, (void **)&inner, 1, 1);

		return inner;
	}

	return mbuf;
}

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline struct rte_mbuf *
nix_sec_meta_to_mbuf(uint64_t cq_w1, uint64_t cq_w5, uintptr_t sa_base,
		     uintptr_t laddr, uint8_t *loff, struct rte_mbuf *mbuf,
		     uint16_t data_off, uint8x16_t *rx_desc_field1,
		     uint64_t *ol_flags, const uint16_t flags,
		     uint64x2_t *rearm)
{
	const void *__p = (void *)((uintptr_t)mbuf + (uint16_t)data_off);
	const struct cpt_parse_hdr_s *hdr = (const struct cpt_parse_hdr_s *)__p;
	uint64_t mbuf_init = vgetq_lane_u64(*rearm, 0);
	struct cn10k_inb_priv_data *inb_priv;
	struct rte_mbuf *inner;
	uint64_t *sg, res_w1;
	uint32_t sa_idx;
	void *inb_sa;
	uint16_t len;
	uint64_t w0;

	if ((flags & NIX_RX_REAS_F) && (cq_w1 & BIT(11))) {
		w0 = hdr->w0.u64;
		sa_idx = w0 >> 32;

		/* Get SPI from CPT_PARSE_S's cookie(already swapped) */
		w0 = hdr->w0.u64;
		sa_idx = w0 >> 32;

		inb_sa = roc_nix_inl_ot_ipsec_inb_sa(sa_base, sa_idx);
		inb_priv = roc_nix_inl_ot_ipsec_inb_sa_sw_rsvd(inb_sa);

		/* Clear checksum flags */
		*ol_flags &= ~(RTE_MBUF_F_RX_L4_CKSUM_MASK |
			       RTE_MBUF_F_RX_IP_CKSUM_MASK);

		if (!hdr->w0.num_frags) {
			/* No Reassembly or inbound error */
			inner = (struct rte_mbuf *)
				(rte_be_to_cpu_64(hdr->wqe_ptr) -
				 sizeof(struct rte_mbuf));
			/* Update dynamic field with userdata */
			*rte_security_dynfield(inner) =
				(uint64_t)inb_priv->userdata;

			/* CPT result(struct cpt_cn10k_res_s) is at
			 * after first IOVA in meta
			 */
			sg = (uint64_t *)(inner + 1);
			res_w1 = sg[10];

			/* Clear checksum flags and update security flag */
			*ol_flags &= ~(RTE_MBUF_F_RX_L4_CKSUM_MASK |
				       RTE_MBUF_F_RX_IP_CKSUM_MASK);
			*ol_flags |=
				(((res_w1 & 0xFF) == CPT_COMP_WARN) ?
				 RTE_MBUF_F_RX_SEC_OFFLOAD :
				 (RTE_MBUF_F_RX_SEC_OFFLOAD |
				  RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED));
			/* Calculate inner packet length */
			len = ((res_w1 >> 16) & 0xFFFF) +
				hdr->w2.il3_off -
				sizeof(struct cpt_parse_hdr_s) -
				(w0 & 0x7);
			/* Update pkt_len and data_len */
			*rx_desc_field1 =
				vsetq_lane_u16(len, *rx_desc_field1, 2);
			*rx_desc_field1 =
				vsetq_lane_u16(len, *rx_desc_field1, 4);

		} else if (!(hdr->w0.err_sum) && !(hdr->w0.reas_sts)) {
			/* Reassembly success */
			inner = nix_sec_reassemble_frags(hdr, cq_w1, cq_w5,
							 mbuf_init);
			sg = (uint64_t *)(inner + 1);
			res_w1 = sg[10];

			/* Update dynamic field with userdata */
			*rte_security_dynfield(inner) =
				(uint64_t)inb_priv->userdata;

			/* Assume success */
			*ol_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD;

			/* Update pkt_len and data_len */
			*rx_desc_field1 = vsetq_lane_u16(inner->pkt_len,
							 *rx_desc_field1, 2);
			*rx_desc_field1 = vsetq_lane_u16(inner->data_len,
							 *rx_desc_field1, 4);

			/* Data offset might be updated */
			mbuf_init = *(uint64_t *)(&inner->rearm_data);
			*rearm = vsetq_lane_u64(mbuf_init, *rearm, 0);
		} else {
			/* Reassembly failure */
			inner = nix_sec_attach_frags(hdr, inb_priv, mbuf_init);
			*ol_flags |= inner->ol_flags;

			/* Update pkt_len and data_len */
			*rx_desc_field1 = vsetq_lane_u16(inner->pkt_len,
							 *rx_desc_field1, 2);
			*rx_desc_field1 = vsetq_lane_u16(inner->data_len,
							 *rx_desc_field1, 4);
		}

		/* Store meta in lmtline to free
		 * Assume all meta's from same aura.
		 */
		*(uint64_t *)(laddr + (*loff << 3)) = (uint64_t)mbuf;
		*loff = *loff + 1;

		/* Return inner mbuf */
		return inner;

	} else if (cq_w1 & BIT(11)) {
		inner = (struct rte_mbuf *)(rte_be_to_cpu_64(hdr->wqe_ptr) -
					    sizeof(struct rte_mbuf));
		/* Get SPI from CPT_PARSE_S's cookie(already swapped) */
		w0 = hdr->w0.u64;
		sa_idx = w0 >> 32;

		inb_sa = roc_nix_inl_ot_ipsec_inb_sa(sa_base, sa_idx);
		inb_priv = roc_nix_inl_ot_ipsec_inb_sa_sw_rsvd(inb_sa);

		/* Update dynamic field with userdata */
		*rte_security_dynfield(inner) = (uint64_t)inb_priv->userdata;

		/* CPT result(struct cpt_cn10k_res_s) is at
		 * after first IOVA in meta
		 */
		sg = (uint64_t *)(inner + 1);
		res_w1 = sg[10];

		/* Clear checksum flags and update security flag */
		*ol_flags &= ~(RTE_MBUF_F_RX_L4_CKSUM_MASK | RTE_MBUF_F_RX_IP_CKSUM_MASK);
		*ol_flags |= (((res_w1 & 0xFF) == CPT_COMP_WARN) ?
			      RTE_MBUF_F_RX_SEC_OFFLOAD :
			      (RTE_MBUF_F_RX_SEC_OFFLOAD | RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED));
		/* Calculate inner packet length */
		len = ((res_w1 >> 16) & 0xFFFF) + hdr->w2.il3_off -
			sizeof(struct cpt_parse_hdr_s) - (w0 & 0x7);
		/* Update pkt_len and data_len */
		*rx_desc_field1 = vsetq_lane_u16(len, *rx_desc_field1, 2);
		*rx_desc_field1 = vsetq_lane_u16(len, *rx_desc_field1, 4);

		/* Store meta in lmtline to free
		 * Assume all meta's from same aura.
		 */
		*(uint64_t *)(laddr + (*loff << 3)) = (uint64_t)mbuf;
		*loff = *loff + 1;

		/* Mark meta mbuf as put */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 0);

		/* Mark inner mbuf as get */
		RTE_MEMPOOL_CHECK_COOKIES(inner->pool, (void **)&inner, 1, 1);

		/* Return inner mbuf */
		return inner;
	}

	/* Return same mbuf as it is not a decrypted pkt */
	return mbuf;
}
#endif

static __rte_always_inline uint32_t
nix_ptype_get(const void *const lookup_mem, const uint64_t in)
{
	const uint16_t *const ptype = lookup_mem;
	const uint16_t lh_lg_lf = (in & 0xFFF0000000000000) >> 52;
	const uint16_t tu_l2 = ptype[(in & 0x000FFFF000000000) >> 36];
	const uint16_t il4_tu = ptype[PTYPE_NON_TUNNEL_ARRAY_SZ + lh_lg_lf];

	return (il4_tu << PTYPE_NON_TUNNEL_WIDTH) | tu_l2;
}

static __rte_always_inline uint32_t
nix_rx_olflags_get(const void *const lookup_mem, const uint64_t in)
{
	const uint32_t *const ol_flags =
		(const uint32_t *)((const uint8_t *)lookup_mem +
				   PTYPE_ARRAY_SZ);

	return ol_flags[(in & 0xfff00000) >> 20];
}

static inline uint64_t
nix_update_match_id(const uint16_t match_id, uint64_t ol_flags,
		    struct rte_mbuf *mbuf)
{
	/* There is no separate bit to check match_id
	 * is valid or not? and no flag to identify it is an
	 * RTE_FLOW_ACTION_TYPE_FLAG vs RTE_FLOW_ACTION_TYPE_MARK
	 * action. The former case addressed through 0 being invalid
	 * value and inc/dec match_id pair when MARK is activated.
	 * The later case addressed through defining
	 * CNXK_FLOW_MARK_DEFAULT as value for
	 * RTE_FLOW_ACTION_TYPE_MARK.
	 * This would translate to not use
	 * CNXK_FLOW_ACTION_FLAG_DEFAULT - 1 and
	 * CNXK_FLOW_ACTION_FLAG_DEFAULT for match_id.
	 * i.e valid mark_id's are from
	 * 0 to CNXK_FLOW_ACTION_FLAG_DEFAULT - 2
	 */
	if (likely(match_id)) {
		ol_flags |= RTE_MBUF_F_RX_FDIR;
		if (match_id != CNXK_FLOW_ACTION_FLAG_DEFAULT) {
			ol_flags |= RTE_MBUF_F_RX_FDIR_ID;
			mbuf->hash.fdir.hi = match_id - 1;
		}
	}

	return ol_flags;
}

static __rte_always_inline void
nix_cqe_xtract_mseg(const union nix_rx_parse_u *rx, struct rte_mbuf *mbuf,
		    uint64_t rearm, const uint16_t flags)
{
	const rte_iova_t *iova_list;
	struct rte_mbuf *head;
	const rte_iova_t *eol;
	uint8_t nb_segs;
	uint64_t sg;

	sg = *(const uint64_t *)(rx + 1);
	nb_segs = (sg >> 48) & 0x3;

	if (nb_segs == 1 && !(flags & NIX_RX_SEC_REASSEMBLY_F)) {
		mbuf->next = NULL;
		return;
	}

	mbuf->pkt_len = (rx->pkt_lenm1 + 1) - (flags & NIX_RX_OFFLOAD_TSTAMP_F ?
					       CNXK_NIX_TIMESYNC_RX_OFFSET : 0);
	mbuf->data_len = (sg & 0xFFFF) - (flags & NIX_RX_OFFLOAD_TSTAMP_F ?
					  CNXK_NIX_TIMESYNC_RX_OFFSET : 0);
	mbuf->nb_segs = nb_segs;
	sg = sg >> 16;

	eol = ((const rte_iova_t *)(rx + 1) + ((rx->desc_sizem1 + 1) << 1));
	/* Skip SG_S and first IOVA*/
	iova_list = ((const rte_iova_t *)(rx + 1)) + 2;
	nb_segs--;

	rearm = rearm & ~0xFFFF;

	head = mbuf;
	while (nb_segs) {
		mbuf->next = ((struct rte_mbuf *)*iova_list) - 1;
		mbuf = mbuf->next;

		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		mbuf->data_len = sg & 0xFFFF;
		sg = sg >> 16;
		*(uint64_t *)(&mbuf->rearm_data) = rearm;
		nb_segs--;
		iova_list++;

		if (!nb_segs && (iova_list + 1 < eol)) {
			sg = *(const uint64_t *)(iova_list);
			nb_segs = (sg >> 48) & 0x3;
			head->nb_segs += nb_segs;
			iova_list = (const rte_iova_t *)(iova_list + 1);
		}
	}
	mbuf->next = NULL;
}

static __rte_always_inline void
cn10k_nix_cqe_to_mbuf(const struct nix_cqe_hdr_s *cq, const uint32_t tag,
		      struct rte_mbuf *mbuf, const void *lookup_mem,
		      const uint64_t val, const uint16_t flag)
{
	const union nix_rx_parse_u *rx =
		(const union nix_rx_parse_u *)((const uint64_t *)cq + 1);
	const uint64_t w1 = *(const uint64_t *)rx;
	uint16_t len = rx->pkt_lenm1 + 1;
	uint64_t ol_flags = 0;

	if (flag & NIX_RX_OFFLOAD_PTYPE_F)
		mbuf->packet_type = nix_ptype_get(lookup_mem, w1);
	else
		mbuf->packet_type = 0;

	if (flag & NIX_RX_OFFLOAD_RSS_F) {
		mbuf->hash.rss = tag;
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}

	/* Skip rx ol flags extraction for Security packets */
	if ((!(flag & NIX_RX_SEC_REASSEMBLY_F) || !(w1 & BIT(11))) &&
			flag & NIX_RX_OFFLOAD_CHECKSUM_F)
		ol_flags |= nix_rx_olflags_get(lookup_mem, w1);

	if (flag & NIX_RX_OFFLOAD_VLAN_STRIP_F) {
		if (rx->vtag0_gone) {
			ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
			mbuf->vlan_tci = rx->vtag0_tci;
		}
		if (rx->vtag1_gone) {
			ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
			mbuf->vlan_tci_outer = rx->vtag1_tci;
		}
	}

	if (flag & NIX_RX_OFFLOAD_MARK_UPDATE_F)
		ol_flags = nix_update_match_id(rx->match_id, ol_flags, mbuf);

	/* Packet data length and ol flags is already updated for sec */
	if (flag & NIX_RX_SEC_REASSEMBLY_F && w1 & BIT_ULL(11)) {
		mbuf->ol_flags |= ol_flags;
	} else {
		mbuf->ol_flags = ol_flags;
		mbuf->pkt_len = len;
		mbuf->data_len = len;
		*(uint64_t *)(&mbuf->rearm_data) = val;
	}

	if (flag & NIX_RX_MULTI_SEG_F)
		/*
		 * For multi segment packets, mbuf length correction according
		 * to Rx timestamp length will be handled later during
		 * timestamp data process.
		 * Hence, flag argument is not required.
		 */
		nix_cqe_xtract_mseg(rx, mbuf, val, 0);
}

static inline uint16_t
nix_rx_nb_pkts(struct cn10k_eth_rxq *rxq, const uint64_t wdata,
	       const uint16_t pkts, const uint32_t qmask)
{
	uint32_t available = rxq->available;

	/* Update the available count if cached value is not enough */
	if (unlikely(available < pkts)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = roc_atomic64_add_sync(wdata, rxq->cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) ||
		    reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
			return 0;

		tail = reg & 0xFFFFF;
		head = (reg >> 20) & 0xFFFFF;
		if (tail < head)
			available = tail - head + qmask + 1;
		else
			available = tail - head;

		rxq->available = available;
	}

	return RTE_MIN(pkts, available);
}

static __rte_always_inline void
cn10k_nix_mbuf_to_tstamp(struct rte_mbuf *mbuf,
			struct cnxk_timesync_info *tstamp,
			const uint8_t ts_enable, uint64_t *tstamp_ptr)
{
	if (ts_enable) {
		mbuf->pkt_len -= CNXK_NIX_TIMESYNC_RX_OFFSET;
		mbuf->data_len -= CNXK_NIX_TIMESYNC_RX_OFFSET;

		/* Reading the rx timestamp inserted by CGX, viz at
		 * starting of the packet data.
		 */
		*tstamp_ptr = ((*tstamp_ptr >> 32) * NSEC_PER_SEC) +
			(*tstamp_ptr & 0xFFFFFFFFUL);
		*cnxk_nix_timestamp_dynfield(mbuf, tstamp) =
			rte_be_to_cpu_64(*tstamp_ptr);
		/* RTE_MBUF_F_RX_IEEE1588_TMST flag needs to be set only in case
		 * PTP packets are received.
		 */
		if (mbuf->packet_type == RTE_PTYPE_L2_ETHER_TIMESYNC) {
			tstamp->rx_tstamp =
				*cnxk_nix_timestamp_dynfield(mbuf, tstamp);
			tstamp->rx_ready = 1;
			mbuf->ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP |
				RTE_MBUF_F_RX_IEEE1588_TMST |
				tstamp->rx_tstamp_dynflag;
		}
	}
}

static __rte_always_inline uint16_t
cn10k_nix_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts,
		    const uint16_t flags)
{
	struct cn10k_eth_rxq *rxq = rx_queue;
	const uint64_t mbuf_init = rxq->mbuf_initializer;
	const void *lookup_mem = rxq->lookup_mem;
	const uint64_t data_off = rxq->data_off;
	const uintptr_t desc = rxq->desc;
	const uint64_t wdata = rxq->wdata;
	const uint32_t qmask = rxq->qmask;
	uint64_t lbase = rxq->lmt_base;
	uint16_t packets = 0, nb_pkts;
	uint8_t loff = 0, lnum = 0;
	uint32_t head = rxq->head;
	struct nix_cqe_hdr_s *cq;
	struct rte_mbuf *mbuf;
	uint64_t aura_handle;
	uint64_t sa_base;
	uint16_t lmt_id;
	uint64_t laddr;

	nb_pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);

	if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
		aura_handle = rxq->aura_handle;
		sa_base = rxq->sa_base;
		sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);
		ROC_LMT_BASE_ID_GET(lbase, lmt_id);
		laddr = lbase;
		laddr += 8;
	}

	while (packets < nb_pkts) {
		/* Prefetch N desc ahead */
		rte_prefetch_non_temporal(
			(void *)(desc + (CQE_SZ((head + 2) & qmask))));
		cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));

		mbuf = nix_get_mbuf_from_cqe(cq, data_off);

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf->pool, (void **)&mbuf, 1, 1);

		/* Translate meta to mbuf */
		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			const uint64_t cq_w1 = *((const uint64_t *)cq + 1);
			const uint64_t cq_w5 = *((const uint64_t *)cq + 5);

			mbuf = nix_sec_meta_to_mbuf_sc(cq_w1, cq_w5, sa_base, laddr,
						       &loff, mbuf, data_off,
						       flags, mbuf_init);
		}

		cn10k_nix_cqe_to_mbuf(cq, cq->tag, mbuf, lookup_mem, mbuf_init,
				      flags);
		cn10k_nix_mbuf_to_tstamp(mbuf, rxq->tstamp,
					(flags & NIX_RX_OFFLOAD_TSTAMP_F),
					(uint64_t *)((uint8_t *)mbuf
								+ data_off));
		rx_pkts[packets++] = mbuf;
		roc_prefetch_store_keep(mbuf);
		head++;
		head &= qmask;

		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			/* Flush when we don't have space for 4 meta */
			if ((15 - loff) < 1) {
				nix_sec_flush_meta(laddr, lmt_id + lnum, loff,
						   aura_handle);
				lnum++;
				lnum &= BIT_ULL(ROC_LMT_LINES_PER_CORE_LOG2) -
					1;
				/* First pointer starts at 8B offset */
				laddr = (uintptr_t)LMT_OFF(lbase, lnum, 8);
				loff = 0;
			}
		}
	}

	rxq->head = head;
	rxq->available -= nb_pkts;

	/* Free all the CQs that we've processed */
	plt_write64((wdata | nb_pkts), rxq->cq_door);

	/* Free remaining meta buffers if any */
	if (flags & NIX_RX_OFFLOAD_SECURITY_F && loff) {
		nix_sec_flush_meta(laddr, lmt_id + lnum, loff, aura_handle);
		plt_io_wmb();
	}

	return nb_pkts;
}

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline uint64_t
nix_vlan_update(const uint64_t w2, uint64_t ol_flags, uint8x16_t *f)
{
	if (w2 & BIT_ULL(21) /* vtag0_gone */) {
		ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		*f = vsetq_lane_u16((uint16_t)(w2 >> 32), *f, 5);
	}

	return ol_flags;
}

static __rte_always_inline uint64_t
nix_qinq_update(const uint64_t w2, uint64_t ol_flags, struct rte_mbuf *mbuf)
{
	if (w2 & BIT_ULL(23) /* vtag1_gone */) {
		ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
		mbuf->vlan_tci_outer = (uint16_t)(w2 >> 48);
	}

	return ol_flags;
}

static __rte_always_inline uint16_t
cn10k_nix_recv_pkts_vector(void *args, struct rte_mbuf **mbufs, uint16_t pkts,
			   const uint16_t flags, void *lookup_mem,
			   struct cnxk_timesync_info *tstamp,
			   uintptr_t lmt_base)
{
	struct cn10k_eth_rxq *rxq = args;
	const uint64_t mbuf_initializer = (flags & NIX_RX_VWQE_F) ?
							*(uint64_t *)args :
							rxq->mbuf_initializer;
	const uint64x2_t data_off = flags & NIX_RX_VWQE_F ?
					vdupq_n_u64(RTE_PKTMBUF_HEADROOM) :
					vdupq_n_u64(rxq->data_off);
	const uint32_t qmask = flags & NIX_RX_VWQE_F ? 0 : rxq->qmask;
	const uint64_t wdata = flags & NIX_RX_VWQE_F ? 0 : rxq->wdata;
	const uintptr_t desc = flags & NIX_RX_VWQE_F ? 0 : rxq->desc;
	uint64x2_t cq0_w8, cq1_w8, cq2_w8, cq3_w8, mbuf01, mbuf23;
	uint64_t ol_flags0, ol_flags1, ol_flags2, ol_flags3;
	uint64x2_t rearm0 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm1 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm2 = vdupq_n_u64(mbuf_initializer);
	uint64x2_t rearm3 = vdupq_n_u64(mbuf_initializer);
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3;
	uint64_t aura_handle, lbase, laddr;
	uint8_t loff = 0, lnum = 0, shft = 0;
	uint8x16_t f0, f1, f2, f3;
	uint16_t lmt_id, d_off;
	uint16_t packets = 0;
	uint16_t pkts_left;
	uintptr_t sa_base;
	uint32_t head;
	uintptr_t cq0;

	if (!(flags & NIX_RX_VWQE_F)) {
		lookup_mem = rxq->lookup_mem;
		head = rxq->head;

		pkts = nix_rx_nb_pkts(rxq, wdata, pkts, qmask);
		pkts_left = pkts & (NIX_DESCS_PER_LOOP - 1);
		/* Packets has to be floor-aligned to NIX_DESCS_PER_LOOP */
		pkts = RTE_ALIGN_FLOOR(pkts, NIX_DESCS_PER_LOOP);
		if (flags & NIX_RX_OFFLOAD_TSTAMP_F)
			tstamp = rxq->tstamp;
	} else {
		RTE_SET_USED(head);
	}

	if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
		if (flags & NIX_RX_VWQE_F) {
			uint16_t port;

			mbuf0 = (struct rte_mbuf *)((uintptr_t)mbufs[0] -
						    sizeof(struct rte_mbuf));
			/* Pick first mbuf's aura handle assuming all
			 * mbufs are from a vec and are from same RQ.
			 */
			aura_handle = mbuf0->pool->pool_id;
			/* Calculate offset from mbuf to actual data area */
			d_off = ((uintptr_t)mbuf0->buf_addr - (uintptr_t)mbuf0);
			d_off += (mbuf_initializer & 0xFFFF);

			/* Get SA Base from lookup tbl using port_id */
			port = mbuf_initializer >> 48;
			sa_base = cnxk_nix_sa_base_get(port, lookup_mem);

			lbase = lmt_base;
		} else {
			aura_handle = rxq->aura_handle;
			d_off = rxq->data_off;
			sa_base = rxq->sa_base;
			lbase = rxq->lmt_base;
		}
		sa_base &= ~(ROC_NIX_INL_SA_BASE_ALIGN - 1);
		ROC_LMT_BASE_ID_GET(lbase, lmt_id);
		lnum = 0;
		laddr = lbase;
		laddr += 8;
	}

	while (packets < pkts) {
		if (!(flags & NIX_RX_VWQE_F)) {
			/* Exit loop if head is about to wrap and become
			 * unaligned.
			 */
			if (((head + NIX_DESCS_PER_LOOP - 1) & qmask) <
			    NIX_DESCS_PER_LOOP) {
				pkts_left += (pkts - packets);
				break;
			}

			cq0 = desc + CQE_SZ(head);
		} else {
			cq0 = (uintptr_t)&mbufs[packets];
		}

		if (flags & NIX_RX_VWQE_F) {
			if (pkts - packets > 4) {
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0,
					4, 0, flags));
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0,
					5, 0, flags));
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0,
					6, 0, flags));
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0,
					7, 0, flags));

				if (likely(pkts - packets > 8)) {
					rte_prefetch1(CQE_PTR_OFF(cq0,
						8, 0, flags));
					rte_prefetch1(CQE_PTR_OFF(cq0,
						9, 0, flags));
					rte_prefetch1(CQE_PTR_OFF(cq0,
						10, 0, flags));
					rte_prefetch1(CQE_PTR_OFF(cq0,
						11, 0, flags));
					if (pkts - packets > 12) {
						rte_prefetch1(CQE_PTR_OFF(cq0,
							12, 0, flags));
						rte_prefetch1(CQE_PTR_OFF(cq0,
							13, 0, flags));
						rte_prefetch1(CQE_PTR_OFF(cq0,
							14, 0, flags));
						rte_prefetch1(CQE_PTR_OFF(cq0,
							15, 0, flags));
					}
				}

				rte_prefetch0(CQE_PTR_DIFF(cq0,
					4, RTE_PKTMBUF_HEADROOM, flags));
				rte_prefetch0(CQE_PTR_DIFF(cq0,
					5, RTE_PKTMBUF_HEADROOM, flags));
				rte_prefetch0(CQE_PTR_DIFF(cq0,
					6, RTE_PKTMBUF_HEADROOM, flags));
				rte_prefetch0(CQE_PTR_DIFF(cq0,
					7, RTE_PKTMBUF_HEADROOM, flags));

				if (likely(pkts - packets > 8)) {
					rte_prefetch0(CQE_PTR_DIFF(cq0,
						8, RTE_PKTMBUF_HEADROOM, flags));
					rte_prefetch0(CQE_PTR_DIFF(cq0,
						9, RTE_PKTMBUF_HEADROOM, flags));
					rte_prefetch0(CQE_PTR_DIFF(cq0,
						10, RTE_PKTMBUF_HEADROOM, flags));
					rte_prefetch0(CQE_PTR_DIFF(cq0,
						11, RTE_PKTMBUF_HEADROOM, flags));
				}
			}
		} else {
			if (pkts - packets > 4) {
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0, 4, 64, flags));
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0, 5, 64, flags));
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0, 6, 64, flags));
				rte_prefetch_non_temporal(CQE_PTR_OFF(cq0, 7, 64, flags));
			}
		}

		if (!(flags & NIX_RX_VWQE_F)) {
			/* Get NIX_RX_SG_S for size and buffer pointer */
			cq0_w8 = vld1q_u64(CQE_PTR_OFF(cq0, 0, 64, flags));
			cq1_w8 = vld1q_u64(CQE_PTR_OFF(cq0, 1, 64, flags));
			cq2_w8 = vld1q_u64(CQE_PTR_OFF(cq0, 2, 64, flags));
			cq3_w8 = vld1q_u64(CQE_PTR_OFF(cq0, 3, 64, flags));

			/* Extract mbuf from NIX_RX_SG_S */
			mbuf01 = vzip2q_u64(cq0_w8, cq1_w8);
			mbuf23 = vzip2q_u64(cq2_w8, cq3_w8);
			mbuf01 = vqsubq_u64(mbuf01, data_off);
			mbuf23 = vqsubq_u64(mbuf23, data_off);
		} else {
			mbuf01 =
				vsubq_u64(vld1q_u64((uint64_t *)cq0), data_off);
			mbuf23 = vsubq_u64(vld1q_u64((uint64_t *)(cq0 + 16)),
					   data_off);
		}

		/* Move mbufs to scalar registers for future use */
		mbuf0 = (struct rte_mbuf *)vgetq_lane_u64(mbuf01, 0);
		mbuf1 = (struct rte_mbuf *)vgetq_lane_u64(mbuf01, 1);
		mbuf2 = (struct rte_mbuf *)vgetq_lane_u64(mbuf23, 0);
		mbuf3 = (struct rte_mbuf *)vgetq_lane_u64(mbuf23, 1);

		if (!(flags & NIX_RX_VWQE_F)) {
			/* Mask to get packet len from NIX_RX_SG_S */
			const uint8x16_t shuf_msk = {
				0xFF, 0xFF, /* pkt_type set as unknown */
				0xFF, 0xFF, /* pkt_type set as unknown */
				0,    1,    /* octet 1~0, low 16 bits pkt_len */
				0xFF, 0xFF, /* skip high 16it pkt_len, zero out */
				0,    1,    /* octet 1~0, 16 bits data_len */
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

			/* Form the rx_descriptor_fields1 with pkt_len and data_len */
			f0 = vqtbl1q_u8(cq0_w8, shuf_msk);
			f1 = vqtbl1q_u8(cq1_w8, shuf_msk);
			f2 = vqtbl1q_u8(cq2_w8, shuf_msk);
			f3 = vqtbl1q_u8(cq3_w8, shuf_msk);
		}
		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			/* Prefetch probable CPT parse header area */
			rte_prefetch_non_temporal(RTE_PTR_ADD(mbuf0, d_off));
			rte_prefetch_non_temporal(RTE_PTR_ADD(mbuf1, d_off));
			rte_prefetch_non_temporal(RTE_PTR_ADD(mbuf2, d_off));
			rte_prefetch_non_temporal(RTE_PTR_ADD(mbuf3, d_off));
		}

		/* Load CQE word0 and word 1 */
		const uint64_t cq0_w0 = *CQE_PTR_OFF(cq0, 0, 0, flags);
		const uint64_t cq0_w1 = *CQE_PTR_OFF(cq0, 0, 8, flags);
		const uint64_t cq0_w2 = *CQE_PTR_OFF(cq0, 0, 16, flags);
		const uint64_t cq1_w0 = *CQE_PTR_OFF(cq0, 1, 0, flags);
		const uint64_t cq1_w1 = *CQE_PTR_OFF(cq0, 1, 8, flags);
		const uint64_t cq1_w2 = *CQE_PTR_OFF(cq0, 1, 16, flags);
		const uint64_t cq2_w0 = *CQE_PTR_OFF(cq0, 2, 0, flags);
		const uint64_t cq2_w1 = *CQE_PTR_OFF(cq0, 2, 8, flags);
		const uint64_t cq2_w2 = *CQE_PTR_OFF(cq0, 2, 16, flags);
		const uint64_t cq3_w0 = *CQE_PTR_OFF(cq0, 3, 0, flags);
		const uint64_t cq3_w1 = *CQE_PTR_OFF(cq0, 3, 8, flags);
		const uint64_t cq3_w2 = *CQE_PTR_OFF(cq0, 3, 16, flags);

		if (flags & NIX_RX_VWQE_F) {
			uint16_t psize0, psize1, psize2, psize3;

			psize0 = (cq0_w2 & 0xFFFF) + 1;
			psize1 = (cq1_w2 & 0xFFFF) + 1;
			psize2 = (cq2_w2 & 0xFFFF) + 1;
			psize3 = (cq3_w2 & 0xFFFF) + 1;

			f0 = vdupq_n_u64(0);
			f1 = vdupq_n_u64(0);
			f2 = vdupq_n_u64(0);
			f3 = vdupq_n_u64(0);

			f0 = vsetq_lane_u16(psize0, f0, 2);
			f0 = vsetq_lane_u16(psize0, f0, 4);

			f1 = vsetq_lane_u16(psize1, f1, 2);
			f1 = vsetq_lane_u16(psize1, f1, 4);

			f2 = vsetq_lane_u16(psize2, f2, 2);
			f2 = vsetq_lane_u16(psize2, f2, 4);

			f3 = vsetq_lane_u16(psize3, f3, 2);
			f3 = vsetq_lane_u16(psize3, f3, 4);
		}

		if (flags & NIX_RX_OFFLOAD_RSS_F) {
			/* Fill rss in the rx_descriptor_fields1 */
			f0 = vsetq_lane_u32(cq0_w0, f0, 3);
			f1 = vsetq_lane_u32(cq1_w0, f1, 3);
			f2 = vsetq_lane_u32(cq2_w0, f2, 3);
			f3 = vsetq_lane_u32(cq3_w0, f3, 3);
			ol_flags0 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags1 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags2 = RTE_MBUF_F_RX_RSS_HASH;
			ol_flags3 = RTE_MBUF_F_RX_RSS_HASH;
		} else {
			ol_flags0 = 0;
			ol_flags1 = 0;
			ol_flags2 = 0;
			ol_flags3 = 0;
		}

		if (flags & NIX_RX_OFFLOAD_PTYPE_F) {
			/* Fill packet_type in the rx_descriptor_fields1 */
			f0 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq0_w1),
					    f0, 0);
			f1 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq1_w1),
					    f1, 0);
			f2 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq2_w1),
					    f2, 0);
			f3 = vsetq_lane_u32(nix_ptype_get(lookup_mem, cq3_w1),
					    f3, 0);
		}

		if (flags & NIX_RX_OFFLOAD_CHECKSUM_F) {
			ol_flags0 |= nix_rx_olflags_get(lookup_mem, cq0_w1);
			ol_flags1 |= nix_rx_olflags_get(lookup_mem, cq1_w1);
			ol_flags2 |= nix_rx_olflags_get(lookup_mem, cq2_w1);
			ol_flags3 |= nix_rx_olflags_get(lookup_mem, cq3_w1);
		}

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf0->pool, (void **)&mbuf0, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf1->pool, (void **)&mbuf1, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf2->pool, (void **)&mbuf2, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf3->pool, (void **)&mbuf3, 1, 1);

		/* Translate meta to mbuf */
		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			uint64_t cq0_w5 = *(uint64_t *)(cq0 + CQE_SZ(0) + 40);
			uint64_t cq1_w5 = *(uint64_t *)(cq0 + CQE_SZ(1) + 40);
			uint64_t cq2_w5 = *(uint64_t *)(cq0 + CQE_SZ(2) + 40);
			uint64_t cq3_w5 = *(uint64_t *)(cq0 + CQE_SZ(3) + 40);

			/* Initialize rearm data when reassembly is enabled as
			 * data offset might change.
			 */
			if (flags & NIX_RX_REAS_F) {
				rearm0 = vdupq_n_u64(mbuf_initializer);
				rearm1 = vdupq_n_u64(mbuf_initializer);
				rearm2 = vdupq_n_u64(mbuf_initializer);
				rearm3 = vdupq_n_u64(mbuf_initializer);
			}

			/* Checksum ol_flags will be cleared if mbuf is meta */
			mbuf0 = nix_sec_meta_to_mbuf(cq0_w1, cq0_w5, sa_base, laddr,
						     &loff, mbuf0, d_off, &f0,
						     &ol_flags0, flags, &rearm0);
			mbuf01 = vsetq_lane_u64((uint64_t)mbuf0, mbuf01, 0);

			mbuf1 = nix_sec_meta_to_mbuf(cq1_w1, cq1_w5, sa_base, laddr,
						     &loff, mbuf1, d_off, &f1,
						     &ol_flags1, flags, &rearm1);
			mbuf01 = vsetq_lane_u64((uint64_t)mbuf1, mbuf01, 1);

			mbuf2 = nix_sec_meta_to_mbuf(cq2_w1, cq2_w5, sa_base, laddr,
						     &loff, mbuf2, d_off, &f2,
						     &ol_flags2, flags, &rearm2);
			mbuf23 = vsetq_lane_u64((uint64_t)mbuf2, mbuf23, 0);

			mbuf3 = nix_sec_meta_to_mbuf(cq3_w1, cq3_w5, sa_base, laddr,
						     &loff, mbuf3, d_off, &f3,
						     &ol_flags3, flags, &rearm3);
			mbuf23 = vsetq_lane_u64((uint64_t)mbuf3, mbuf23, 1);
		}

		if (flags & NIX_RX_OFFLOAD_VLAN_STRIP_F) {

			ol_flags0 = nix_vlan_update(cq0_w2, ol_flags0, &f0);
			ol_flags1 = nix_vlan_update(cq1_w2, ol_flags1, &f1);
			ol_flags2 = nix_vlan_update(cq2_w2, ol_flags2, &f2);
			ol_flags3 = nix_vlan_update(cq3_w2, ol_flags3, &f3);

			ol_flags0 = nix_qinq_update(cq0_w2, ol_flags0, mbuf0);
			ol_flags1 = nix_qinq_update(cq1_w2, ol_flags1, mbuf1);
			ol_flags2 = nix_qinq_update(cq2_w2, ol_flags2, mbuf2);
			ol_flags3 = nix_qinq_update(cq3_w2, ol_flags3, mbuf3);
		}

		if (flags & NIX_RX_OFFLOAD_MARK_UPDATE_F) {
			ol_flags0 = nix_update_match_id(
				*(uint16_t *)CQE_PTR_OFF(cq0, 0, 38, flags),
				ol_flags0, mbuf0);
			ol_flags1 = nix_update_match_id(
				*(uint16_t *)CQE_PTR_OFF(cq0, 1, 38, flags),
				ol_flags1, mbuf1);
			ol_flags2 = nix_update_match_id(
				*(uint16_t *)CQE_PTR_OFF(cq0, 2, 38, flags),
				ol_flags2, mbuf2);
			ol_flags3 = nix_update_match_id(
				*(uint16_t *)CQE_PTR_OFF(cq0, 3, 38, flags),
				ol_flags3, mbuf3);
		}

		if (flags & NIX_RX_OFFLOAD_TSTAMP_F) {
			const uint16x8_t len_off = {
				0,			     /* ptype   0:15 */
				0,			     /* ptype  16:32 */
				CNXK_NIX_TIMESYNC_RX_OFFSET, /* pktlen  0:15*/
				0,			     /* pktlen 16:32 */
				CNXK_NIX_TIMESYNC_RX_OFFSET, /* datalen 0:15 */
				0,
				0,
				0};
			const uint32x4_t ptype = {RTE_PTYPE_L2_ETHER_TIMESYNC,
						  RTE_PTYPE_L2_ETHER_TIMESYNC,
						  RTE_PTYPE_L2_ETHER_TIMESYNC,
						  RTE_PTYPE_L2_ETHER_TIMESYNC};
			const uint64_t ts_olf = RTE_MBUF_F_RX_IEEE1588_PTP |
						RTE_MBUF_F_RX_IEEE1588_TMST |
						tstamp->rx_tstamp_dynflag;
			const uint32x4_t and_mask = {0x1, 0x2, 0x4, 0x8};
			uint64x2_t ts01, ts23, mask;
			uint64_t ts[4];
			uint8_t res;

			/* Subtract timesync length from total pkt length. */
			f0 = vsubq_u16(f0, len_off);
			f1 = vsubq_u16(f1, len_off);
			f2 = vsubq_u16(f2, len_off);
			f3 = vsubq_u16(f3, len_off);

			/* Get the address of actual timestamp. */
			ts01 = vaddq_u64(mbuf01, data_off);
			ts23 = vaddq_u64(mbuf23, data_off);
			/* Load timestamp from address. */
			ts01 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts01,
									  0),
					      ts01, 0);
			ts01 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts01,
									  1),
					      ts01, 1);
			ts23 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts23,
									  0),
					      ts23, 0);
			ts23 = vsetq_lane_u64(*(uint64_t *)vgetq_lane_u64(ts23,
									  1),
					      ts23, 1);
			/* Convert from be to cpu byteorder. */
			ts01 = vrev64q_u8(ts01);
			ts23 = vrev64q_u8(ts23);
			/* Store timestamp into scalar for later use. */
			ts[0] = vgetq_lane_u64(ts01, 0);
			ts[1] = vgetq_lane_u64(ts01, 1);
			ts[2] = vgetq_lane_u64(ts23, 0);
			ts[3] = vgetq_lane_u64(ts23, 1);

			/* Store timestamp into dynfield. */
			*cnxk_nix_timestamp_dynfield(mbuf0, tstamp) = ts[0];
			*cnxk_nix_timestamp_dynfield(mbuf1, tstamp) = ts[1];
			*cnxk_nix_timestamp_dynfield(mbuf2, tstamp) = ts[2];
			*cnxk_nix_timestamp_dynfield(mbuf3, tstamp) = ts[3];

			/* Generate ptype mask to filter L2 ether timesync */
			mask = vdupq_n_u32(vgetq_lane_u32(f0, 0));
			mask = vsetq_lane_u32(vgetq_lane_u32(f1, 0), mask, 1);
			mask = vsetq_lane_u32(vgetq_lane_u32(f2, 0), mask, 2);
			mask = vsetq_lane_u32(vgetq_lane_u32(f3, 0), mask, 3);

			/* Match against L2 ether timesync. */
			mask = vceqq_u32(mask, ptype);
			/* Convert from vector from scalar mask */
			res = vaddvq_u32(vandq_u32(mask, and_mask));
			res &= 0xF;

			if (res) {
				/* Fill in the ol_flags for any packets that
				 * matched.
				 */
				ol_flags0 |= ((res & 0x1) ? ts_olf : 0);
				ol_flags1 |= ((res & 0x2) ? ts_olf : 0);
				ol_flags2 |= ((res & 0x4) ? ts_olf : 0);
				ol_flags3 |= ((res & 0x8) ? ts_olf : 0);

				/* Update Rxq timestamp with the latest
				 * timestamp.
				 */
				tstamp->rx_ready = 1;
				tstamp->rx_tstamp = ts[31 - __builtin_clz(res)];
			}
		}

		/* Form rearm_data with ol_flags */
		rearm0 = vsetq_lane_u64(ol_flags0, rearm0, 1);
		rearm1 = vsetq_lane_u64(ol_flags1, rearm1, 1);
		rearm2 = vsetq_lane_u64(ol_flags2, rearm2, 1);
		rearm3 = vsetq_lane_u64(ol_flags3, rearm3, 1);

		/* Update rx_descriptor_fields1 */
		vst1q_u64((uint64_t *)mbuf0->rx_descriptor_fields1, f0);
		vst1q_u64((uint64_t *)mbuf1->rx_descriptor_fields1, f1);
		vst1q_u64((uint64_t *)mbuf2->rx_descriptor_fields1, f2);
		vst1q_u64((uint64_t *)mbuf3->rx_descriptor_fields1, f3);

		/* Update rearm_data */
		vst1q_u64((uint64_t *)mbuf0->rearm_data, rearm0);
		vst1q_u64((uint64_t *)mbuf1->rearm_data, rearm1);
		vst1q_u64((uint64_t *)mbuf2->rearm_data, rearm2);
		vst1q_u64((uint64_t *)mbuf3->rearm_data, rearm3);

		/* Store the mbufs to rx_pkts */
		vst1q_u64((uint64_t *)&mbufs[packets], mbuf01);
		vst1q_u64((uint64_t *)&mbufs[packets + 2], mbuf23);

		if (flags & NIX_RX_MULTI_SEG_F) {
			/* Multi segment is enable build mseg list for
			 * individual mbufs in scalar mode.
			 */
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
					    (CQE_PTR_OFF(cq0, 0, 8, flags)),
					    mbuf0, mbuf_initializer, flags);
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
					    (CQE_PTR_OFF(cq0, 1, 8, flags)),
					    mbuf1, mbuf_initializer, flags);
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
					    (CQE_PTR_OFF(cq0, 2, 8, flags)),
					    mbuf2, mbuf_initializer, flags);
			nix_cqe_xtract_mseg((union nix_rx_parse_u *)
					    (CQE_PTR_OFF(cq0, 3, 8, flags)),
					    mbuf3, mbuf_initializer, flags);
		}

		/* Mark mempool obj as "get" as it is alloc'ed by NIX */
		RTE_MEMPOOL_CHECK_COOKIES(mbuf0->pool, (void **)&mbuf0, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf1->pool, (void **)&mbuf1, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf2->pool, (void **)&mbuf2, 1, 1);
		RTE_MEMPOOL_CHECK_COOKIES(mbuf3->pool, (void **)&mbuf3, 1, 1);

		nix_mbuf_validate_next(mbuf0);
		nix_mbuf_validate_next(mbuf1);
		nix_mbuf_validate_next(mbuf2);
		nix_mbuf_validate_next(mbuf3);

		packets += NIX_DESCS_PER_LOOP;

		if (!(flags & NIX_RX_VWQE_F)) {
			/* Advance head pointer and packets */
			head += NIX_DESCS_PER_LOOP;
			head &= qmask;
		}

		if (flags & NIX_RX_OFFLOAD_SECURITY_F) {
			/* Check if lmtline border is crossed and adjust lnum */
			if (loff > 15) {
				/* Update aura handle */
				*(uint64_t *)(laddr - 8) =
					(((uint64_t)(15 & 0x1) << 32) |
				    roc_npa_aura_handle_to_aura(aura_handle));
				loff = loff - 15;
				shft += 3;

				lnum++;
				laddr = (uintptr_t)LMT_OFF(lbase, lnum, 8);
				/* Pick the pointer from 16th index and put it
				 * at end of this new line.
				 */
				*(uint64_t *)(laddr + (loff << 3) - 8) =
					*(uint64_t *)(laddr - 8);
			}

			/* Flush it when we are in 16th line and might
			 * overflow it
			 */
			if (lnum >= 15 && loff >= 12) {
				/* 16 LMT Line size m1 */
				uint64_t data = BIT_ULL(48) - 1;

				/* Update aura handle */
				*(uint64_t *)(laddr - 8) =
					(((uint64_t)(loff & 0x1) << 32) |
				    roc_npa_aura_handle_to_aura(aura_handle));

				data = (data & ~(0x7UL << shft)) |
				       (((uint64_t)loff >> 1) << shft);

				/* Send up to 16 lmt lines of pointers */
				nix_sec_flush_meta_burst(lmt_id, data, lnum + 1,
							 aura_handle);
				rte_io_wmb();
				lnum = 0;
				loff = 0;
				shft = 0;
				/* First pointer starts at 8B offset */
				laddr = (uintptr_t)LMT_OFF(lbase, lnum, 8);
			}
		}
	}

	if (flags & NIX_RX_OFFLOAD_SECURITY_F && loff) {
		/* 16 LMT Line size m1 */
		uint64_t data = BIT_ULL(48) - 1;

		/* Update aura handle */
		*(uint64_t *)(laddr - 8) =
			(((uint64_t)(loff & 0x1) << 32) |
			 roc_npa_aura_handle_to_aura(aura_handle));

		data = (data & ~(0x7UL << shft)) |
		       (((uint64_t)loff >> 1) << shft);

		/* Send up to 16 lmt lines of pointers */
		nix_sec_flush_meta_burst(lmt_id, data, lnum + 1, aura_handle);
		if (flags & NIX_RX_VWQE_F)
			plt_io_wmb();
	}

	if (flags & NIX_RX_VWQE_F)
		return packets;

	rxq->head = head;
	rxq->available -= packets;

	rte_io_wmb();
	/* Free all the CQs that we've processed */
	plt_write64((rxq->wdata | packets), rxq->cq_door);

	if (unlikely(pkts_left))
		packets += cn10k_nix_recv_pkts(args, &mbufs[packets], pkts_left,
					       flags);

	return packets;
}

#else

static inline uint16_t
cn10k_nix_recv_pkts_vector(void *args, struct rte_mbuf **mbufs, uint16_t pkts,
			   const uint16_t flags, void *lookup_mem,
			   struct cnxk_timesync_info *tstamp,
			   uintptr_t lmt_base)
{
	RTE_SET_USED(args);
	RTE_SET_USED(mbufs);
	RTE_SET_USED(pkts);
	RTE_SET_USED(flags);
	RTE_SET_USED(lookup_mem);
	RTE_SET_USED(tstamp);
	RTE_SET_USED(lmt_base);

	return 0;
}

#endif


#define RSS_F	  NIX_RX_OFFLOAD_RSS_F
#define PTYPE_F	  NIX_RX_OFFLOAD_PTYPE_F
#define CKSUM_F	  NIX_RX_OFFLOAD_CHECKSUM_F
#define MARK_F	  NIX_RX_OFFLOAD_MARK_UPDATE_F
#define TS_F      NIX_RX_OFFLOAD_TSTAMP_F
#define RX_VLAN_F NIX_RX_OFFLOAD_VLAN_STRIP_F
#define R_SEC_F   NIX_RX_OFFLOAD_SECURITY_F

/* [R_SEC_F] [RX_VLAN_F] [TS] [MARK] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES_0_15                                             \
	R(no_offload, NIX_RX_OFFLOAD_NONE)                                     \
	R(rss, RSS_F)                                                          \
	R(ptype, PTYPE_F)                                                      \
	R(ptype_rss, PTYPE_F | RSS_F)                                          \
	R(cksum, CKSUM_F)                                                      \
	R(cksum_rss, CKSUM_F | RSS_F)                                          \
	R(cksum_ptype, CKSUM_F | PTYPE_F)                                      \
	R(cksum_ptype_rss, CKSUM_F | PTYPE_F | RSS_F)                          \
	R(mark, MARK_F)                                                        \
	R(mark_rss, MARK_F | RSS_F)                                            \
	R(mark_ptype, MARK_F | PTYPE_F)                                        \
	R(mark_ptype_rss, MARK_F | PTYPE_F | RSS_F)                            \
	R(mark_cksum, MARK_F | CKSUM_F)                                        \
	R(mark_cksum_rss, MARK_F | CKSUM_F | RSS_F)                            \
	R(mark_cksum_ptype, MARK_F | CKSUM_F | PTYPE_F)                        \
	R(mark_cksum_ptype_rss, MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_16_31                                            \
	R(ts, TS_F)                                                            \
	R(ts_rss, TS_F | RSS_F)                                                \
	R(ts_ptype, TS_F | PTYPE_F)                                            \
	R(ts_ptype_rss, TS_F | PTYPE_F | RSS_F)                                \
	R(ts_cksum, TS_F | CKSUM_F)                                            \
	R(ts_cksum_rss, TS_F | CKSUM_F | RSS_F)                                \
	R(ts_cksum_ptype, TS_F | CKSUM_F | PTYPE_F)                            \
	R(ts_cksum_ptype_rss, TS_F | CKSUM_F | PTYPE_F | RSS_F)                \
	R(ts_mark, TS_F | MARK_F)                                              \
	R(ts_mark_rss, TS_F | MARK_F | RSS_F)                                  \
	R(ts_mark_ptype, TS_F | MARK_F | PTYPE_F)                              \
	R(ts_mark_ptype_rss, TS_F | MARK_F | PTYPE_F | RSS_F)                  \
	R(ts_mark_cksum, TS_F | MARK_F | CKSUM_F)                              \
	R(ts_mark_cksum_rss, TS_F | MARK_F | CKSUM_F | RSS_F)                  \
	R(ts_mark_cksum_ptype, TS_F | MARK_F | CKSUM_F | PTYPE_F)              \
	R(ts_mark_cksum_ptype_rss, TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_32_47                                            \
	R(vlan, RX_VLAN_F)                                                     \
	R(vlan_rss, RX_VLAN_F | RSS_F)                                         \
	R(vlan_ptype, RX_VLAN_F | PTYPE_F)                                     \
	R(vlan_ptype_rss, RX_VLAN_F | PTYPE_F | RSS_F)                         \
	R(vlan_cksum, RX_VLAN_F | CKSUM_F)                                     \
	R(vlan_cksum_rss, RX_VLAN_F | CKSUM_F | RSS_F)                         \
	R(vlan_cksum_ptype, RX_VLAN_F | CKSUM_F | PTYPE_F)                     \
	R(vlan_cksum_ptype_rss, RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)         \
	R(vlan_mark, RX_VLAN_F | MARK_F)                                       \
	R(vlan_mark_rss, RX_VLAN_F | MARK_F | RSS_F)                           \
	R(vlan_mark_ptype, RX_VLAN_F | MARK_F | PTYPE_F)                       \
	R(vlan_mark_ptype_rss, RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)           \
	R(vlan_mark_cksum, RX_VLAN_F | MARK_F | CKSUM_F)                       \
	R(vlan_mark_cksum_rss, RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)           \
	R(vlan_mark_cksum_ptype, RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)       \
	R(vlan_mark_cksum_ptype_rss,                                           \
	  RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_48_63                                            \
	R(vlan_ts, RX_VLAN_F | TS_F)                                           \
	R(vlan_ts_rss, RX_VLAN_F | TS_F | RSS_F)                               \
	R(vlan_ts_ptype, RX_VLAN_F | TS_F | PTYPE_F)                           \
	R(vlan_ts_ptype_rss, RX_VLAN_F | TS_F | PTYPE_F | RSS_F)               \
	R(vlan_ts_cksum, RX_VLAN_F | TS_F | CKSUM_F)                           \
	R(vlan_ts_cksum_rss, RX_VLAN_F | TS_F | CKSUM_F | RSS_F)               \
	R(vlan_ts_cksum_ptype, RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)           \
	R(vlan_ts_cksum_ptype_rss,                                             \
	  RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)                        \
	R(vlan_ts_mark, RX_VLAN_F | TS_F | MARK_F)                             \
	R(vlan_ts_mark_rss, RX_VLAN_F | TS_F | MARK_F | RSS_F)                 \
	R(vlan_ts_mark_ptype, RX_VLAN_F | TS_F | MARK_F | PTYPE_F)             \
	R(vlan_ts_mark_ptype_rss, RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F) \
	R(vlan_ts_mark_cksum, RX_VLAN_F | TS_F | MARK_F | CKSUM_F)             \
	R(vlan_ts_mark_cksum_rss, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F) \
	R(vlan_ts_mark_cksum_ptype,                                            \
	  RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                       \
	R(vlan_ts_mark_cksum_ptype_rss,                                        \
	  RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_64_79                                            \
	R(sec, R_SEC_F)                                                        \
	R(sec_rss, R_SEC_F | RSS_F)                                            \
	R(sec_ptype, R_SEC_F | PTYPE_F)                                        \
	R(sec_ptype_rss, R_SEC_F | PTYPE_F | RSS_F)                            \
	R(sec_cksum, R_SEC_F | CKSUM_F)                                        \
	R(sec_cksum_rss, R_SEC_F | CKSUM_F | RSS_F)                            \
	R(sec_cksum_ptype, R_SEC_F | CKSUM_F | PTYPE_F)                        \
	R(sec_cksum_ptype_rss, R_SEC_F | CKSUM_F | PTYPE_F | RSS_F)            \
	R(sec_mark, R_SEC_F | MARK_F)                                          \
	R(sec_mark_rss, R_SEC_F | MARK_F | RSS_F)                              \
	R(sec_mark_ptype, R_SEC_F | MARK_F | PTYPE_F)                          \
	R(sec_mark_ptype_rss, R_SEC_F | MARK_F | PTYPE_F | RSS_F)              \
	R(sec_mark_cksum, R_SEC_F | MARK_F | CKSUM_F)                          \
	R(sec_mark_cksum_rss, R_SEC_F | MARK_F | CKSUM_F | RSS_F)              \
	R(sec_mark_cksum_ptype, R_SEC_F | MARK_F | CKSUM_F | PTYPE_F)          \
	R(sec_mark_cksum_ptype_rss,                                            \
	  R_SEC_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_80_95                                            \
	R(sec_ts, R_SEC_F | TS_F)                                              \
	R(sec_ts_rss, R_SEC_F | TS_F | RSS_F)                                  \
	R(sec_ts_ptype, R_SEC_F | TS_F | PTYPE_F)                              \
	R(sec_ts_ptype_rss, R_SEC_F | TS_F | PTYPE_F | RSS_F)                  \
	R(sec_ts_cksum, R_SEC_F | TS_F | CKSUM_F)                              \
	R(sec_ts_cksum_rss, R_SEC_F | TS_F | CKSUM_F | RSS_F)                  \
	R(sec_ts_cksum_ptype, R_SEC_F | TS_F | CKSUM_F | PTYPE_F)              \
	R(sec_ts_cksum_ptype_rss, R_SEC_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)  \
	R(sec_ts_mark, R_SEC_F | TS_F | MARK_F)                                \
	R(sec_ts_mark_rss, R_SEC_F | TS_F | MARK_F | RSS_F)                    \
	R(sec_ts_mark_ptype, R_SEC_F | TS_F | MARK_F | PTYPE_F)                \
	R(sec_ts_mark_ptype_rss, R_SEC_F | TS_F | MARK_F | PTYPE_F | RSS_F)    \
	R(sec_ts_mark_cksum, R_SEC_F | TS_F | MARK_F | CKSUM_F)                \
	R(sec_ts_mark_cksum_rss, R_SEC_F | TS_F | MARK_F | CKSUM_F | RSS_F)    \
	R(sec_ts_mark_cksum_ptype,                                             \
	  R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                         \
	R(sec_ts_mark_cksum_ptype_rss,                                         \
	  R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_96_111                                           \
	R(sec_vlan, R_SEC_F | RX_VLAN_F)                                       \
	R(sec_vlan_rss, R_SEC_F | RX_VLAN_F | RSS_F)                           \
	R(sec_vlan_ptype, R_SEC_F | RX_VLAN_F | PTYPE_F)                       \
	R(sec_vlan_ptype_rss, R_SEC_F | RX_VLAN_F | PTYPE_F | RSS_F)           \
	R(sec_vlan_cksum, R_SEC_F | RX_VLAN_F | CKSUM_F)                       \
	R(sec_vlan_cksum_rss, R_SEC_F | RX_VLAN_F | CKSUM_F | RSS_F)           \
	R(sec_vlan_cksum_ptype, R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F)       \
	R(sec_vlan_cksum_ptype_rss,                                            \
	  R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)                     \
	R(sec_vlan_mark, R_SEC_F | RX_VLAN_F | MARK_F)                         \
	R(sec_vlan_mark_rss, R_SEC_F | RX_VLAN_F | MARK_F | RSS_F)             \
	R(sec_vlan_mark_ptype, R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F)         \
	R(sec_vlan_mark_ptype_rss,                                             \
	  R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)                      \
	R(sec_vlan_mark_cksum, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F)         \
	R(sec_vlan_mark_cksum_rss,                                             \
	  R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)                      \
	R(sec_vlan_mark_cksum_ptype,                                           \
	  R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)                    \
	R(sec_vlan_mark_cksum_ptype_rss,                                       \
	  R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_112_127                                          \
	R(sec_vlan_ts, R_SEC_F | RX_VLAN_F | TS_F)                             \
	R(sec_vlan_ts_rss, R_SEC_F | RX_VLAN_F | TS_F | RSS_F)                 \
	R(sec_vlan_ts_ptype, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F)             \
	R(sec_vlan_ts_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F | RSS_F) \
	R(sec_vlan_ts_cksum, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F)             \
	R(sec_vlan_ts_cksum_rss, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | RSS_F) \
	R(sec_vlan_ts_cksum_ptype,                                             \
	  R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)                      \
	R(sec_vlan_ts_cksum_ptype_rss,                                         \
	  R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)              \
	R(sec_vlan_ts_mark, R_SEC_F | RX_VLAN_F | TS_F | MARK_F)               \
	R(sec_vlan_ts_mark_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | RSS_F)   \
	R(sec_vlan_ts_mark_ptype,                                              \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F)                       \
	R(sec_vlan_ts_mark_ptype_rss,                                          \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)               \
	R(sec_vlan_ts_mark_cksum,                                              \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F)                       \
	R(sec_vlan_ts_mark_cksum_rss,                                          \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)               \
	R(sec_vlan_ts_mark_cksum_ptype,                                        \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)             \
	R(sec_vlan_ts_mark_cksum_ptype_rss,                                    \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)


#define NIX_RX_FASTPATH_MODES                                                  \
	NIX_RX_FASTPATH_MODES_0_15                                             \
	NIX_RX_FASTPATH_MODES_16_31                                            \
	NIX_RX_FASTPATH_MODES_32_47                                            \
	NIX_RX_FASTPATH_MODES_48_63                                            \
	NIX_RX_FASTPATH_MODES_64_79                                            \
	NIX_RX_FASTPATH_MODES_80_95                                            \
	NIX_RX_FASTPATH_MODES_96_111                                           \
	NIX_RX_FASTPATH_MODES_112_127                                          \

#define R(name, flags)                                                         \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_##name(          \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_mseg_##name(     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_vec_##name(      \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_vec_mseg_##name( \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_reas_##name(     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_reas_mseg_##name(\
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_reas_vec_##name( \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);     \
	uint16_t __rte_noinline __rte_hot cn10k_nix_recv_pkts_reas_vec_mseg_##name( \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);

NIX_RX_FASTPATH_MODES
#undef R

#define NIX_RX_RECV(fn, flags)                                                 \
	uint16_t __rte_noinline __rte_hot fn(                                  \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)      \
	{                                                                      \
		return cn10k_nix_recv_pkts(rx_queue, rx_pkts, pkts, (flags));  \
	}

#define NIX_RX_RECV_MSEG(fn, flags) NIX_RX_RECV(fn, flags | NIX_RX_MULTI_SEG_F)

#define NIX_RX_RECV_VEC(fn, flags)                                             \
	uint16_t __rte_noinline __rte_hot fn(                                  \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)      \
	{                                                                      \
		return cn10k_nix_recv_pkts_vector(rx_queue, rx_pkts, pkts,     \
						  (flags), NULL, NULL, 0);     \
	}

#define NIX_RX_RECV_VEC_MSEG(fn, flags)                                        \
	NIX_RX_RECV_VEC(fn, flags | NIX_RX_MULTI_SEG_F)

#endif /* __CN10K_RX_H__ */
