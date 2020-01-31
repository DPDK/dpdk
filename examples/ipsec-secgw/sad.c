/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_errno.h>
#include <rte_malloc.h>

#include "ipsec.h"
#include "sad.h"

RTE_DEFINE_PER_LCORE(struct ipsec_sad_cache, sad_cache) = {
	.v4 = NULL,
	.v6 = NULL,
	.mask = 0,
};

int
ipsec_sad_add(struct ipsec_sad *sad, struct ipsec_sa *sa)
{
	int ret;
	void *tmp = NULL;
	union rte_ipsec_sad_key key = { {0} };
	const union rte_ipsec_sad_key *lookup_key[1];

	/* spi field is common for ipv4 and ipv6 key types */
	key.v4.spi = rte_cpu_to_be_32(sa->spi);
	lookup_key[0] = &key;
	switch (WITHOUT_TRANSPORT_VERSION(sa->flags)) {
	case IP4_TUNNEL:
		rte_ipsec_sad_lookup(sad->sad_v4, lookup_key, &tmp, 1);
		if (tmp != NULL)
			return -EEXIST;

		ret = rte_ipsec_sad_add(sad->sad_v4, &key,
			RTE_IPSEC_SAD_SPI_ONLY, sa);
		if (ret != 0)
			return ret;
		break;
	case IP6_TUNNEL:
		rte_ipsec_sad_lookup(sad->sad_v6, lookup_key, &tmp, 1);
		if (tmp != NULL)
			return -EEXIST;

		ret = rte_ipsec_sad_add(sad->sad_v6, &key,
			RTE_IPSEC_SAD_SPI_ONLY, sa);
		if (ret != 0)
			return ret;
		break;
	case TRANSPORT:
		if (sp4_spi_present(sa->spi, 1, NULL, NULL) >= 0) {
			rte_ipsec_sad_lookup(sad->sad_v4, lookup_key, &tmp, 1);
			if (tmp != NULL)
				return -EEXIST;

			ret = rte_ipsec_sad_add(sad->sad_v4, &key,
				RTE_IPSEC_SAD_SPI_ONLY, sa);
			if (ret != 0)
				return ret;
		}
		if (sp6_spi_present(sa->spi, 1, NULL, NULL) >= 0) {
			rte_ipsec_sad_lookup(sad->sad_v6, lookup_key, &tmp, 1);
			if (tmp != NULL)
				return -EEXIST;

			ret = rte_ipsec_sad_add(sad->sad_v6, &key,
				RTE_IPSEC_SAD_SPI_ONLY, sa);
			if (ret != 0)
				return ret;
		}
	}

	return 0;
}

/*
 * Init per lcore SAD cache.
 * Must be called by every processing lcore.
 */
int
ipsec_sad_lcore_cache_init(uint32_t nb_cache_ent)
{
	uint32_t cache_elem;
	size_t cache_mem_sz;
	struct ipsec_sad_cache *cache;

	cache = &RTE_PER_LCORE(sad_cache);

	cache_elem = rte_align32pow2(nb_cache_ent);
	cache_mem_sz = sizeof(struct ipsec_sa *) * cache_elem;

	if (cache_mem_sz != 0) {
		cache->v4 = rte_zmalloc_socket(NULL, cache_mem_sz,
			RTE_CACHE_LINE_SIZE, rte_socket_id());
		if (cache->v4 == NULL)
			return -rte_errno;

		cache->v6 = rte_zmalloc_socket(NULL, cache_mem_sz,
			RTE_CACHE_LINE_SIZE, rte_socket_id());
		if (cache->v6 == NULL)
			return -rte_errno;

		cache->mask = cache_elem - 1;
	}

	return 0;
}

int
ipsec_sad_create(const char *name, struct ipsec_sad *sad,
	int socket_id, struct ipsec_sa_cnt *sa_cnt)
{
	int ret;
	struct rte_ipsec_sad_conf sad_conf;
	char sad_name[RTE_IPSEC_SAD_NAMESIZE];

	if ((name == NULL) || (sad == NULL) || (sa_cnt == NULL))
		return -EINVAL;

	ret = snprintf(sad_name, RTE_IPSEC_SAD_NAMESIZE, "%s_v4", name);
	if (ret < 0 || ret >= RTE_IPSEC_SAD_NAMESIZE)
		return -ENAMETOOLONG;

	sad_conf.socket_id = socket_id;
	sad_conf.flags = 0;
	/* Make SAD have extra 25% of required number of entries */
	sad_conf.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = sa_cnt->nb_v4 * 5 / 4;
	sad_conf.max_sa[RTE_IPSEC_SAD_SPI_DIP] = 0;
	sad_conf.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = 0;

	if (sa_cnt->nb_v4 != 0) {
		sad->sad_v4 = rte_ipsec_sad_create(sad_name, &sad_conf);
		if (sad->sad_v4 == NULL)
			return -rte_errno;
	}

	ret = snprintf(sad_name, RTE_IPSEC_SAD_NAMESIZE, "%s_v6", name);
	if (ret < 0 || ret >= RTE_IPSEC_SAD_NAMESIZE)
		return -ENAMETOOLONG;
	sad_conf.flags = RTE_IPSEC_SAD_FLAG_IPV6;
	sad_conf.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = sa_cnt->nb_v6 * 5 / 4;

	if (sa_cnt->nb_v6 != 0) {
		sad->sad_v6 = rte_ipsec_sad_create(name, &sad_conf);
		if (sad->sad_v6 == NULL)
			return -rte_errno;
	}

	return 0;
}
