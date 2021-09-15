/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _TRIE_H_
#define _TRIE_H_

/**
 * @file
 * RTE IPv6 Longest Prefix Match (LPM)
 */
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

/* @internal Total number of tbl24 entries. */
#define TRIE_TBL24_NUM_ENT	(1 << 24)
/* Maximum depth value possible for IPv6 LPM. */
#define TRIE_MAX_DEPTH		128
/* @internal Number of entries in a tbl8 group. */
#define TRIE_TBL8_GRP_NUM_ENT	256ULL
/* @internal Total number of tbl8 groups in the tbl8. */
#define TRIE_TBL8_NUM_GROUPS	65536
/* @internal bitmask with valid and valid_group fields set */
#define TRIE_EXT_ENT		1

#define BITMAP_SLAB_BIT_SIZE_LOG2	6
#define BITMAP_SLAB_BIT_SIZE		(1ULL << BITMAP_SLAB_BIT_SIZE_LOG2)
#define BITMAP_SLAB_BITMASK		(BITMAP_SLAB_BIT_SIZE - 1)

struct rte_trie_tbl {
	uint32_t	number_tbl8s;	/**< Total number of tbl8s */
	uint32_t	rsvd_tbl8s;	/**< Number of reserved tbl8s */
	uint32_t	cur_tbl8s;	/**< Current cumber of tbl8s */
	uint64_t	def_nh;		/**< Default next hop */
	enum rte_fib_trie_nh_sz	nh_sz;	/**< Size of nexthop entry */
	uint64_t	*tbl8;		/**< tbl8 table. */
	uint32_t	*tbl8_pool;	/**< bitmap containing free tbl8 idxes*/
	uint32_t	tbl8_pool_pos;
	/* tbl24 table. */
	__extension__ uint64_t	tbl24[0] __rte_cache_aligned;
};

static inline uint32_t
get_tbl24_idx(const uint8_t *ip)
{
	return ip[0] << 16|ip[1] << 8|ip[2];
}

static inline void *
get_tbl24_p(struct rte_trie_tbl *dp, const uint8_t *ip, uint8_t nh_sz)
{
	uint32_t tbl24_idx;

	tbl24_idx = get_tbl24_idx(ip);
	return (void *)&((uint8_t *)dp->tbl24)[tbl24_idx << nh_sz];
}

static inline uint8_t
bits_in_nh(uint8_t nh_sz)
{
	return 8 * (1 << nh_sz);
}

static inline uint64_t
get_max_nh(uint8_t nh_sz)
{
	return ((1ULL << (bits_in_nh(nh_sz) - 1)) - 1);
}

static inline uint64_t
lookup_msk(uint8_t nh_sz)
{
	return ((1ULL << ((1 << (nh_sz + 3)) - 1)) << 1) - 1;
}

static inline uint8_t
get_psd_idx(uint32_t val, uint8_t nh_sz)
{
	return val & ((1 << (3 - nh_sz)) - 1);
}

static inline uint32_t
get_tbl_pos(uint32_t val, uint8_t nh_sz)
{
	return val >> (3 - nh_sz);
}

static inline uint64_t
get_tbl_val_by_idx(uint64_t *tbl, uint32_t idx, uint8_t nh_sz)
{
	return ((tbl[get_tbl_pos(idx, nh_sz)] >> (get_psd_idx(idx, nh_sz) *
		bits_in_nh(nh_sz))) & lookup_msk(nh_sz));
}

static inline void *
get_tbl_p_by_idx(uint64_t *tbl, uint64_t idx, uint8_t nh_sz)
{
	return (uint8_t *)tbl + (idx << nh_sz);
}

static inline int
is_entry_extended(uint64_t ent)
{
	return (ent & TRIE_EXT_ENT) == TRIE_EXT_ENT;
}

#define LOOKUP_FUNC(suffix, type, nh_sz)				\
static inline void rte_trie_lookup_bulk_##suffix(void *p,		\
	uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],				\
	uint64_t *next_hops, const unsigned int n)			\
{									\
	struct rte_trie_tbl *dp = (struct rte_trie_tbl *)p;		\
	uint64_t tmp;							\
	uint32_t i, j;							\
									\
	for (i = 0; i < n; i++) {					\
		tmp = ((type *)dp->tbl24)[get_tbl24_idx(&ips[i][0])];	\
		j = 3;							\
		while (is_entry_extended(tmp)) {			\
			tmp = ((type *)dp->tbl8)[ips[i][j++] +		\
				((tmp >> 1) * TRIE_TBL8_GRP_NUM_ENT)];	\
		}							\
		next_hops[i] = tmp >> 1;				\
	}								\
}
LOOKUP_FUNC(2b, uint16_t, 1)
LOOKUP_FUNC(4b, uint32_t, 2)
LOOKUP_FUNC(8b, uint64_t, 3)

void *
trie_create(const char *name, int socket_id, struct rte_fib6_conf *conf);

void
trie_free(void *p);

rte_fib6_lookup_fn_t
trie_get_lookup_fn(void *p, enum rte_fib6_lookup_type type);

int
trie_modify(struct rte_fib6 *fib, const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t depth, uint64_t next_hop, int op);

#endif /* _TRIE_H_ */
