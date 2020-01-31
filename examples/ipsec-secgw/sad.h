/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef __SAD_H__
#define __SAD_H__

#include <rte_ipsec_sad.h>

struct ipsec_sad {
	struct rte_ipsec_sad *sad_v4;
	struct rte_ipsec_sad *sad_v6;
};

int ipsec_sad_create(const char *name, struct ipsec_sad *sad,
	int socket_id, struct ipsec_sa_cnt *sa_cnt);

int ipsec_sad_add(struct ipsec_sad *sad, struct ipsec_sa *sa);

static inline void
sad_lookup(const struct ipsec_sad *sad, struct rte_mbuf *pkts[],
	void *sa[], uint16_t nb_pkts)
{
	uint32_t i;
	uint32_t nb_v4 = 0, nb_v6 = 0;
	struct rte_esp_hdr *esp;
	struct rte_ipv4_hdr *ipv4;
	struct rte_ipv6_hdr *ipv6;
	struct rte_ipsec_sadv4_key	v4[nb_pkts];
	struct rte_ipsec_sadv6_key	v6[nb_pkts];
	int v4_idxes[nb_pkts];
	int v6_idxes[nb_pkts];
	const union rte_ipsec_sad_key	*keys_v4[nb_pkts];
	const union rte_ipsec_sad_key	*keys_v6[nb_pkts];
	void *v4_res[nb_pkts];
	void *v6_res[nb_pkts];

	/* split received packets by address family into two arrays */
	for (i = 0; i < nb_pkts; i++) {
		ipv4 = rte_pktmbuf_mtod(pkts[i], struct rte_ipv4_hdr *);
		esp = rte_pktmbuf_mtod_offset(pkts[i], struct rte_esp_hdr *,
				pkts[i]->l3_len);
		if ((ipv4->version_ihl >> 4) == IPVERSION) {
			v4[nb_v4].spi = esp->spi;
			v4[nb_v4].dip = ipv4->dst_addr;
			v4[nb_v4].sip = ipv4->src_addr;
			keys_v4[nb_v4] = (const union rte_ipsec_sad_key *)
						&v4[nb_v4];
			v4_idxes[nb_v4++] = i;
		} else {
			ipv6 = rte_pktmbuf_mtod(pkts[i], struct rte_ipv6_hdr *);
			v6[nb_v6].spi = esp->spi;
			memcpy(v6[nb_v6].dip, ipv6->dst_addr,
					sizeof(ipv6->dst_addr));
			memcpy(v6[nb_v6].sip, ipv6->src_addr,
					sizeof(ipv6->src_addr));
			keys_v6[nb_v6] = (const union rte_ipsec_sad_key *)
						&v6[nb_v6];
			v6_idxes[nb_v6++] = i;
		}
	}

	if (nb_v4 != 0)
		rte_ipsec_sad_lookup(sad->sad_v4, keys_v4, v4_res, nb_v4);
	if (nb_v6 != 0)
		rte_ipsec_sad_lookup(sad->sad_v6, keys_v6, v6_res, nb_v6);

	for (i = 0; i < nb_v4; i++)
		sa[v4_idxes[i]] = v4_res[i];

	for (i = 0; i < nb_v6; i++)
		sa[v6_idxes[i]] = v6_res[i];
}

#endif /* __SAD_H__ */
