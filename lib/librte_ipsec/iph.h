/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _IPH_H_
#define _IPH_H_

#include <rte_ip.h>

/**
 * @file iph.h
 * Contains functions/structures/macros to manipulate IPv4/IPv6 headers
 * used internally by ipsec library.
 */

/*
 * Move preceding (L3) headers down to remove ESP header and IV.
 */
static inline void
remove_esph(char *np, char *op, uint32_t hlen)
{
	uint32_t i;

	for (i = hlen; i-- != 0; np[i] = op[i])
		;
}

/*
 * Move preceding (L3) headers up to free space for ESP header and IV.
 */
static inline void
insert_esph(char *np, char *op, uint32_t hlen)
{
	uint32_t i;

	for (i = 0; i != hlen; i++)
		np[i] = op[i];
}

/* update original ip header fields for transport case */
static inline int
update_trs_l3hdr(const struct rte_ipsec_sa *sa, void *p, uint32_t plen,
		uint32_t l2len, uint32_t l3len, uint8_t proto)
{
	int32_t rc;

	/* IPv4 */
	if ((sa->type & RTE_IPSEC_SATP_IPV_MASK) == RTE_IPSEC_SATP_IPV4) {
		struct rte_ipv4_hdr *v4h;

		v4h = p;
		rc = v4h->next_proto_id;
		v4h->next_proto_id = proto;
		v4h->total_length = rte_cpu_to_be_16(plen - l2len);
	/* IPv6 */
	} else {
		struct rte_ipv6_hdr *v6h;
		uint8_t *p_nh;

		v6h = p;

		/* basic IPv6 header with no extensions */
		if (l3len == sizeof(struct rte_ipv6_hdr))
			p_nh = &v6h->proto;

		/* IPv6 with extensions */
		else {
			size_t ext_len;
			int nh;
			uint8_t *pd, *plimit;

			/* locate last extension within l3len bytes */
			pd = (uint8_t *)p;
			plimit = pd + l3len;
			ext_len = sizeof(struct rte_ipv6_hdr);
			nh = v6h->proto;
			while (pd + ext_len < plimit) {
				pd += ext_len;
				nh = rte_ipv6_get_next_ext(pd, nh, &ext_len);
				if (unlikely(nh < 0))
					return -EINVAL;
			}

			/* invalid l3len - extension exceeds header length */
			if (unlikely(pd + ext_len != plimit))
				return -EINVAL;

			/* save last extension offset */
			p_nh = pd;
		}

		/* update header type; return original value */
		rc = *p_nh;
		*p_nh = proto;

		/* fix packet length */
		v6h->payload_len = rte_cpu_to_be_16(plen - l2len -
				sizeof(*v6h));
	}

	return rc;
}

/*
 * Inline functions to get and set ipv6 packet header traffic class (TC) field.
 */
static inline uint8_t
get_ipv6_tc(rte_be32_t vtc_flow)
{
	uint32_t v;

	v = rte_be_to_cpu_32(vtc_flow);
	return v >> RTE_IPV6_HDR_TC_SHIFT;
}

static inline rte_be32_t
set_ipv6_tc(rte_be32_t vtc_flow, uint32_t tos)
{
	uint32_t v;

	v = rte_cpu_to_be_32(tos << RTE_IPV6_HDR_TC_SHIFT);
	vtc_flow &= ~rte_cpu_to_be_32(RTE_IPV6_HDR_TC_MASK);

	return (v | vtc_flow);
}

/**
 * Update type-of-service/traffic-class field of outbound tunnel packet.
 *
 * @param ref_h: reference header, for outbound it is inner header, otherwise
 *   outer header.
 * @param update_h: header to be updated tos/tc field, for outbound it is outer
 *   header, otherwise inner header.
 * @param tos_mask: type-of-service mask stored in sa.
 * @param is_outh_ipv4: 1 if outer header is ipv4, 0 if it is ipv6.
 * @param is_inner_ipv4: 1 if inner header is ipv4, 0 if it is ipv6.
 */
static inline void
update_outb_tun_tos(const void *ref_h, void *update_h, uint32_t tos_mask,
		uint8_t is_outh_ipv4, uint8_t is_inh_ipv4)
{
	uint8_t idx = ((is_outh_ipv4 << 1) | is_inh_ipv4);
	struct rte_ipv4_hdr *v4out_h;
	struct rte_ipv6_hdr *v6out_h;
	uint32_t itp, otp;

	switch (idx) {
	case 0: /*outh ipv6, inh ipv6 */
		v6out_h = update_h;
		otp = get_ipv6_tc(v6out_h->vtc_flow) & ~tos_mask;
		itp = get_ipv6_tc(((const struct rte_ipv6_hdr *)ref_h)->
				vtc_flow) & tos_mask;
		v6out_h->vtc_flow = set_ipv6_tc(v6out_h->vtc_flow, otp | itp);
		break;
	case 1: /*outh ipv6, inh ipv4 */
		v6out_h = update_h;
		otp = get_ipv6_tc(v6out_h->vtc_flow) & ~tos_mask;
		itp = ((const struct rte_ipv4_hdr *)ref_h)->type_of_service &
				tos_mask;
		v6out_h->vtc_flow = set_ipv6_tc(v6out_h->vtc_flow, otp | itp);
		break;
	case 2: /*outh ipv4, inh ipv6 */
		v4out_h = update_h;
		otp = v4out_h->type_of_service & ~tos_mask;
		itp = get_ipv6_tc(((const struct rte_ipv6_hdr *)ref_h)->
				vtc_flow) & tos_mask;
		v4out_h->type_of_service = (otp | itp);
		break;
	case 3: /* outh ipv4, inh ipv4 */
		v4out_h = update_h;
		otp = v4out_h->type_of_service & ~tos_mask;
		itp = ((const struct rte_ipv4_hdr *)ref_h)->type_of_service &
				tos_mask;
		v4out_h->type_of_service = (otp | itp);
		break;
	}
}

/**
 * Update type-of-service/traffic-class field of inbound tunnel packet.
 *
 * @param ref_h: reference header, for outbound it is inner header, otherwise
 *   outer header.
 * @param update_h: header to be updated tos/tc field, for outbound it is outer
 *   header, otherwise inner header.
 * @param is_outh_ipv4: 1 if outer header is ipv4, 0 if it is ipv6.
 * @param is_inner_ipv4: 1 if inner header is ipv4, 0 if it is ipv6.
 */
static inline void
update_inb_tun_tos(const void *ref_h, void *update_h,
		uint8_t is_outh_ipv4, uint8_t is_inh_ipv4)
{
	uint8_t idx = ((is_outh_ipv4 << 1) | is_inh_ipv4);
	struct rte_ipv4_hdr *v4in_h;
	struct rte_ipv6_hdr *v6in_h;
	uint8_t ecn_v4out, ecn_v4in;
	uint32_t ecn_v6out, ecn_v6in;

	switch (idx) {
	case 0: /* outh ipv6, inh ipv6 */
		v6in_h = update_h;
		ecn_v6out = ((const struct rte_ipv6_hdr *)ref_h)->vtc_flow &
				rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_MASK);
		ecn_v6in = v6in_h->vtc_flow &
				rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_MASK);
		if ((ecn_v6out == rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_CE)) &&
				(ecn_v6in != 0))
			v6in_h->vtc_flow |=
					rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_CE);
		break;
	case 1: /* outh ipv6, inh ipv4 */
		v4in_h = update_h;
		ecn_v6out = ((const struct rte_ipv6_hdr *)ref_h)->vtc_flow &
				rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_MASK);
		ecn_v4in = v4in_h->type_of_service & RTE_IPV4_HDR_ECN_MASK;
		if ((ecn_v6out == rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_CE)) &&
				(ecn_v4in != 0))
			v4in_h->type_of_service |= RTE_IPV4_HDR_ECN_CE;
		break;
	case 2: /* outh ipv4, inh ipv6 */
		v6in_h = update_h;
		ecn_v4out = ((const struct rte_ipv4_hdr *)ref_h)->
				type_of_service & RTE_IPV4_HDR_ECN_MASK;
		ecn_v6in = v6in_h->vtc_flow &
				rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_MASK);
		if (ecn_v4out == RTE_IPV4_HDR_ECN_CE && ecn_v6in != 0)
			v6in_h->vtc_flow |=
					rte_cpu_to_be_32(RTE_IPV6_HDR_ECN_CE);
		break;
	case 3: /* outh ipv4, inh ipv4 */
		v4in_h = update_h;
		ecn_v4out = ((const struct rte_ipv4_hdr *)ref_h)->
				type_of_service & RTE_IPV4_HDR_ECN_MASK;
		ecn_v4in = v4in_h->type_of_service & RTE_IPV4_HDR_ECN_MASK;
		if (ecn_v4out == RTE_IPV4_HDR_ECN_CE && ecn_v4in != 0)
			v4in_h->type_of_service |= RTE_IPV4_HDR_ECN_CE;
		break;
	}
}

/* update original and new ip header fields for tunnel case */
static inline void
update_tun_outb_l3hdr(const struct rte_ipsec_sa *sa, void *outh,
		const void *inh, uint32_t plen, uint32_t l2len, rte_be16_t pid)
{
	struct rte_ipv4_hdr *v4h;
	struct rte_ipv6_hdr *v6h;
	uint8_t is_outh_ipv4;

	if (sa->type & RTE_IPSEC_SATP_MODE_TUNLV4) {
		is_outh_ipv4 = 1;
		v4h = outh;
		v4h->packet_id = pid;
		v4h->total_length = rte_cpu_to_be_16(plen - l2len);
	} else {
		is_outh_ipv4 = 0;
		v6h = outh;
		v6h->payload_len = rte_cpu_to_be_16(plen - l2len -
				sizeof(*v6h));
	}

	if (sa->type & TUN_HDR_MSK)
		update_outb_tun_tos(inh, outh, sa->tos_mask, is_outh_ipv4,
				((sa->type & RTE_IPSEC_SATP_IPV_MASK) ==
					RTE_IPSEC_SATP_IPV4));
}

static inline void
update_tun_inb_l3hdr(const struct rte_ipsec_sa *sa, const void *outh,
		void *inh)
{
	if (sa->type & TUN_HDR_MSK)
		update_inb_tun_tos(outh, inh,
				((sa->type & RTE_IPSEC_SATP_MODE_TUNLV4) != 0),
				((sa->type & RTE_IPSEC_SATP_IPV_MASK) ==
						RTE_IPSEC_SATP_IPV4));
}

#endif /* _IPH_H_ */
