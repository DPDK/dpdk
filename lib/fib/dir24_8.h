/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _DIR24_8_H_
#define _DIR24_8_H_

#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

/**
 * @file
 * DIR24_8 algorithm
 */

#define DIR24_8_TBL24_NUM_ENT		(1 << 24)
#define DIR24_8_TBL8_GRP_NUM_ENT	256U
#define DIR24_8_EXT_ENT			1
#define DIR24_8_TBL24_MASK		0xffffff00

#define BITMAP_SLAB_BIT_SIZE_LOG2	6
#define BITMAP_SLAB_BIT_SIZE		(1 << BITMAP_SLAB_BIT_SIZE_LOG2)
#define BITMAP_SLAB_BITMASK		(BITMAP_SLAB_BIT_SIZE - 1)

struct dir24_8_tbl {
	uint32_t	number_tbl8s;	/**< Total number of tbl8s */
	uint32_t	rsvd_tbl8s;	/**< Number of reserved tbl8s */
	uint32_t	cur_tbl8s;	/**< Current number of tbl8s */
	enum rte_fib_dir24_8_nh_sz	nh_sz;	/**< Size of nexthop entry */
	uint64_t	def_nh;		/**< Default next hop */
	uint64_t	*tbl8;		/**< tbl8 table. */
	uint64_t	*tbl8_idxes;	/**< bitmap containing free tbl8 idxes*/
	/* tbl24 table. */
	__extension__ uint64_t	tbl24[0] __rte_cache_aligned;
};

static inline void *
get_tbl24_p(struct dir24_8_tbl *dp, uint32_t ip, uint8_t nh_sz)
{
	return (void *)&((uint8_t *)dp->tbl24)[(ip &
		DIR24_8_TBL24_MASK) >> (8 - nh_sz)];
}

static inline  uint8_t
bits_in_nh(uint8_t nh_sz)
{
	return 8 * (1 << nh_sz);
}

static inline uint64_t
get_max_nh(uint8_t nh_sz)
{
	return ((1ULL << (bits_in_nh(nh_sz) - 1)) - 1);
}

static  inline uint32_t
get_tbl24_idx(uint32_t ip)
{
	return ip >> 8;
}

static  inline uint32_t
get_tbl8_idx(uint32_t res, uint32_t ip)
{
	return (res >> 1) * DIR24_8_TBL8_GRP_NUM_ENT + (uint8_t)ip;
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
get_tbl_idx(uint32_t val, uint8_t nh_sz)
{
	return val >> (3 - nh_sz);
}

static inline uint64_t
get_tbl24(struct dir24_8_tbl *dp, uint32_t ip, uint8_t nh_sz)
{
	return ((dp->tbl24[get_tbl_idx(get_tbl24_idx(ip), nh_sz)] >>
		(get_psd_idx(get_tbl24_idx(ip), nh_sz) *
		bits_in_nh(nh_sz))) & lookup_msk(nh_sz));
}

static inline uint64_t
get_tbl8(struct dir24_8_tbl *dp, uint32_t res, uint32_t ip, uint8_t nh_sz)
{
	return ((dp->tbl8[get_tbl_idx(get_tbl8_idx(res, ip), nh_sz)] >>
		(get_psd_idx(get_tbl8_idx(res, ip), nh_sz) *
		bits_in_nh(nh_sz))) & lookup_msk(nh_sz));
}

static inline int
is_entry_extended(uint64_t ent)
{
	return (ent & DIR24_8_EXT_ENT) == DIR24_8_EXT_ENT;
}

#define LOOKUP_FUNC(suffix, type, bulk_prefetch, nh_sz)			\
static inline void dir24_8_lookup_bulk_##suffix(void *p, const uint32_t *ips, \
	uint64_t *next_hops, const unsigned int n)			\
{									\
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;		\
	uint64_t tmp;							\
	uint32_t i;							\
	uint32_t prefetch_offset =					\
		RTE_MIN((unsigned int)bulk_prefetch, n);		\
									\
	for (i = 0; i < prefetch_offset; i++)				\
		rte_prefetch0(get_tbl24_p(dp, ips[i], nh_sz));		\
	for (i = 0; i < (n - prefetch_offset); i++) {			\
		rte_prefetch0(get_tbl24_p(dp,				\
			ips[i + prefetch_offset], nh_sz));		\
		tmp = ((type *)dp->tbl24)[ips[i] >> 8];			\
		if (unlikely(is_entry_extended(tmp)))			\
			tmp = ((type *)dp->tbl8)[(uint8_t)ips[i] +	\
				((tmp >> 1) * DIR24_8_TBL8_GRP_NUM_ENT)]; \
		next_hops[i] = tmp >> 1;				\
	}								\
	for (; i < n; i++) {						\
		tmp = ((type *)dp->tbl24)[ips[i] >> 8];			\
		if (unlikely(is_entry_extended(tmp)))			\
			tmp = ((type *)dp->tbl8)[(uint8_t)ips[i] +	\
				((tmp >> 1) * DIR24_8_TBL8_GRP_NUM_ENT)]; \
		next_hops[i] = tmp >> 1;				\
	}								\
}									\

LOOKUP_FUNC(1b, uint8_t, 5, 0)
LOOKUP_FUNC(2b, uint16_t, 6, 1)
LOOKUP_FUNC(4b, uint32_t, 15, 2)
LOOKUP_FUNC(8b, uint64_t, 12, 3)

static inline void
dir24_8_lookup_bulk(struct dir24_8_tbl *dp, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n, uint8_t nh_sz)
{
	uint64_t tmp;
	uint32_t i;
	uint32_t prefetch_offset = RTE_MIN(15U, n);

	for (i = 0; i < prefetch_offset; i++)
		rte_prefetch0(get_tbl24_p(dp, ips[i], nh_sz));
	for (i = 0; i < (n - prefetch_offset); i++) {
		rte_prefetch0(get_tbl24_p(dp, ips[i + prefetch_offset],
			nh_sz));
		tmp = get_tbl24(dp, ips[i], nh_sz);
		if (unlikely(is_entry_extended(tmp)))
			tmp = get_tbl8(dp, tmp, ips[i], nh_sz);

		next_hops[i] = tmp >> 1;
	}
	for (; i < n; i++) {
		tmp = get_tbl24(dp, ips[i], nh_sz);
		if (unlikely(is_entry_extended(tmp)))
			tmp = get_tbl8(dp, tmp, ips[i], nh_sz);

		next_hops[i] = tmp >> 1;
	}
}

static inline void
dir24_8_lookup_bulk_0(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;

	dir24_8_lookup_bulk(dp, ips, next_hops, n, 0);
}

static inline void
dir24_8_lookup_bulk_1(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;

	dir24_8_lookup_bulk(dp, ips, next_hops, n, 1);
}

static inline void
dir24_8_lookup_bulk_2(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;

	dir24_8_lookup_bulk(dp, ips, next_hops, n, 2);
}

static inline void
dir24_8_lookup_bulk_3(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;

	dir24_8_lookup_bulk(dp, ips, next_hops, n, 3);
}

static inline void
dir24_8_lookup_bulk_uni(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;
	uint64_t tmp;
	uint32_t i;
	uint32_t prefetch_offset = RTE_MIN(15U, n);
	uint8_t nh_sz = dp->nh_sz;

	for (i = 0; i < prefetch_offset; i++)
		rte_prefetch0(get_tbl24_p(dp, ips[i], nh_sz));
	for (i = 0; i < (n - prefetch_offset); i++) {
		rte_prefetch0(get_tbl24_p(dp, ips[i + prefetch_offset],
			nh_sz));
		tmp = get_tbl24(dp, ips[i], nh_sz);
		if (unlikely(is_entry_extended(tmp)))
			tmp = get_tbl8(dp, tmp, ips[i], nh_sz);

		next_hops[i] = tmp >> 1;
	}
	for (; i < n; i++) {
		tmp = get_tbl24(dp, ips[i], nh_sz);
		if (unlikely(is_entry_extended(tmp)))
			tmp = get_tbl8(dp, tmp, ips[i], nh_sz);

		next_hops[i] = tmp >> 1;
	}
}

void *
dir24_8_create(const char *name, int socket_id, struct rte_fib_conf *conf);

void
dir24_8_free(void *p);

rte_fib_lookup_fn_t
dir24_8_get_lookup_fn(void *p, enum rte_fib_lookup_type type);

int
dir24_8_modify(struct rte_fib *fib, uint32_t ip, uint8_t depth,
	uint64_t next_hop, int op);

#endif /* _DIR24_8_H_ */
