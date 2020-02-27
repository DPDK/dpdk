/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef __SAD_H__
#define __SAD_H__

#include <rte_ipsec_sad.h>

#define SA_CACHE_SZ	128
#define SPI2IDX(spi, mask)	((spi) & (mask))

struct ipsec_sad_cache {
	struct ipsec_sa **v4;
	struct ipsec_sa **v6;
	uint32_t mask;
};

RTE_DECLARE_PER_LCORE(struct ipsec_sad_cache, sad_cache);

int ipsec_sad_create(const char *name, struct ipsec_sad *sad,
	int socket_id, struct ipsec_sa_cnt *sa_cnt);

int ipsec_sad_add(struct ipsec_sad *sad, struct ipsec_sa *sa);

int ipsec_sad_lcore_cache_init(uint32_t nb_cache_ent);

static inline int
cmp_sa_key(struct ipsec_sa *sa, int is_v4, struct rte_ipv4_hdr *ipv4,
	struct rte_ipv6_hdr *ipv6)
{
	int sa_type = WITHOUT_TRANSPORT_VERSION(sa->flags);
	if ((sa_type == TRANSPORT) ||
			/* IPv4 check */
			(is_v4 && (sa_type == IP4_TUNNEL) &&
			(sa->src.ip.ip4 == ipv4->src_addr) &&
			(sa->dst.ip.ip4 == ipv4->dst_addr)) ||
			/* IPv6 check */
			(!is_v4 && (sa_type == IP6_TUNNEL) &&
			(!memcmp(sa->src.ip.ip6.ip6, ipv6->src_addr, 16)) &&
			(!memcmp(sa->dst.ip.ip6.ip6, ipv6->dst_addr, 16))))
		return 1;

	return 0;
}

static inline void
sa_cache_update(struct ipsec_sa **sa_cache, struct ipsec_sa *sa, uint32_t mask)
{
	uint32_t cache_idx;

	/* SAD cache is disabled */
	if (mask == 0)
		return;

	cache_idx = SPI2IDX(sa->spi, mask);
	sa_cache[cache_idx] = sa;
}

static inline void
sad_lookup(struct ipsec_sad *sad, struct rte_mbuf *pkts[],
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
	uint32_t spi, cache_idx;
	struct ipsec_sad_cache *cache;
	struct ipsec_sa *cached_sa;
	int is_ipv4;

	cache  = &RTE_PER_LCORE(sad_cache);

	/* split received packets by address family into two arrays */
	for (i = 0; i < nb_pkts; i++) {
		ipv4 = rte_pktmbuf_mtod(pkts[i], struct rte_ipv4_hdr *);
		ipv6 = rte_pktmbuf_mtod(pkts[i], struct rte_ipv6_hdr *);
		esp = rte_pktmbuf_mtod_offset(pkts[i], struct rte_esp_hdr *,
				pkts[i]->l3_len);

		is_ipv4 = pkts[i]->packet_type & RTE_PTYPE_L3_IPV4;
		spi = rte_be_to_cpu_32(esp->spi);
		cache_idx = SPI2IDX(spi, cache->mask);

		if (is_ipv4) {
			cached_sa = (cache->mask != 0) ?
				cache->v4[cache_idx] : NULL;
			/* check SAD cache entry */
			if ((cached_sa != NULL) && (cached_sa->spi == spi)) {
				if (cmp_sa_key(cached_sa, 1, ipv4, ipv6)) {
					/* cache hit */
					sa[i] = cached_sa;
					continue;
				}
			}
			/*
			 * cache miss
			 * preparing sad key to proceed with sad lookup
			 */
			v4[nb_v4].spi = esp->spi;
			v4[nb_v4].dip = ipv4->dst_addr;
			v4[nb_v4].sip = ipv4->src_addr;
			keys_v4[nb_v4] = (const union rte_ipsec_sad_key *)
						&v4[nb_v4];
			v4_idxes[nb_v4++] = i;
		} else {
			cached_sa = (cache->mask != 0) ?
				cache->v6[cache_idx] : NULL;
			if ((cached_sa != NULL) && (cached_sa->spi == spi)) {
				if (cmp_sa_key(cached_sa, 0, ipv4, ipv6)) {
					sa[i] = cached_sa;
					continue;
				}
			}
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

	for (i = 0; i < nb_v4; i++) {
		ipv4 = rte_pktmbuf_mtod(pkts[v4_idxes[i]],
			struct rte_ipv4_hdr *);
		if ((v4_res[i] != NULL) &&
				(cmp_sa_key(v4_res[i], 1, ipv4, NULL))) {
			sa[v4_idxes[i]] = v4_res[i];
			sa_cache_update(cache->v4, (struct ipsec_sa *)v4_res[i],
				cache->mask);
		} else
			sa[v4_idxes[i]] = NULL;
	}
	for (i = 0; i < nb_v6; i++) {
		ipv6 = rte_pktmbuf_mtod(pkts[v6_idxes[i]],
			struct rte_ipv6_hdr *);
		if ((v6_res[i] != NULL) &&
				(cmp_sa_key(v6_res[i], 0, NULL, ipv6))) {
			sa[v6_idxes[i]] = v6_res[i];
			sa_cache_update(cache->v6, (struct ipsec_sa *)v6_res[i],
				cache->mask);
		} else
			sa[v6_idxes[i]] = NULL;
	}
}

#endif /* __SAD_H__ */
