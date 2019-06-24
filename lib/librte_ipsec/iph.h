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

/* update original and new ip header fields for tunnel case */
static inline void
update_tun_l3hdr(const struct rte_ipsec_sa *sa, void *p, uint32_t plen,
		uint32_t l2len, rte_be16_t pid)
{
	struct rte_ipv4_hdr *v4h;
	struct rte_ipv6_hdr *v6h;

	if (sa->type & RTE_IPSEC_SATP_MODE_TUNLV4) {
		v4h = p;
		v4h->packet_id = pid;
		v4h->total_length = rte_cpu_to_be_16(plen - l2len);
	} else {
		v6h = p;
		v6h->payload_len = rte_cpu_to_be_16(plen - l2len -
				sizeof(*v6h));
	}
}

#endif /* _IPH_H_ */
