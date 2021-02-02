/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_TSO_H
#define _SFC_TSO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Standard TSO header length */
#define SFC_TSOH_STD_LEN	256

/** The number of TSO option descriptors that precede the packet descriptors */
#define SFC_EF10_TSO_OPT_DESCS_NUM	2

/**
 * The number of DMA descriptors for TSO header that may or may not precede the
 * packet's payload descriptors
 */
#define SFC_EF10_TSO_HDR_DESCS_NUM	1

static inline uint16_t
sfc_tso_ip4_get_ipid(const uint8_t *pkt_hdrp, size_t ip_hdr_off)
{
	const struct rte_ipv4_hdr *ip_hdrp;
	uint16_t ipid;

	ip_hdrp = (const struct rte_ipv4_hdr *)(pkt_hdrp + ip_hdr_off);
	rte_memcpy(&ipid, &ip_hdrp->packet_id, sizeof(ipid));

	return rte_be_to_cpu_16(ipid);
}

static inline void
sfc_tso_outer_udp_fix_len(const struct rte_mbuf *m, uint8_t *tsoh)
{
	rte_be16_t len = rte_cpu_to_be_16(m->l2_len + m->l3_len + m->l4_len +
					  m->tso_segsz);

	rte_memcpy(tsoh + m->outer_l2_len + m->outer_l3_len +
		   offsetof(struct rte_udp_hdr, dgram_len),
		   &len, sizeof(len));
}

static inline void
sfc_tso_innermost_ip_fix_len(const struct rte_mbuf *m, uint8_t *tsoh,
			     size_t iph_ofst)
{
	size_t ip_payload_len = m->l4_len + m->tso_segsz;
	size_t field_ofst;
	rte_be16_t len;

	if (m->ol_flags & PKT_TX_IPV4) {
		field_ofst = offsetof(struct rte_ipv4_hdr, total_length);
		len = rte_cpu_to_be_16(m->l3_len + ip_payload_len);
	} else {
		field_ofst = offsetof(struct rte_ipv6_hdr, payload_len);
		len = rte_cpu_to_be_16(ip_payload_len);
	}

	rte_memcpy(tsoh + iph_ofst + field_ofst, &len, sizeof(len));
}

unsigned int sfc_tso_prepare_header(uint8_t *tsoh, size_t header_len,
				    struct rte_mbuf **in_seg, size_t *in_off);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_TSO_H */
