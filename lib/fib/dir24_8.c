/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_debug.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_vect.h>

#include <rte_rib.h>
#include <rte_fib.h>
#include "dir24_8.h"

#ifdef CC_DIR24_8_AVX512_SUPPORT

#include "dir24_8_avx512.h"

#endif /* CC_DIR24_8_AVX512_SUPPORT */

#define DIR24_8_NAMESIZE	64

#define ROUNDUP(x, y)	 RTE_ALIGN_CEIL(x, (1 << (32 - y)))

static inline rte_fib_lookup_fn_t
get_scalar_fn(enum rte_fib_dir24_8_nh_sz nh_sz)
{
	switch (nh_sz) {
	case RTE_FIB_DIR24_8_1B:
		return dir24_8_lookup_bulk_1b;
	case RTE_FIB_DIR24_8_2B:
		return dir24_8_lookup_bulk_2b;
	case RTE_FIB_DIR24_8_4B:
		return dir24_8_lookup_bulk_4b;
	case RTE_FIB_DIR24_8_8B:
		return dir24_8_lookup_bulk_8b;
	default:
		return NULL;
	}
}

static inline rte_fib_lookup_fn_t
get_scalar_fn_inlined(enum rte_fib_dir24_8_nh_sz nh_sz)
{
	switch (nh_sz) {
	case RTE_FIB_DIR24_8_1B:
		return dir24_8_lookup_bulk_0;
	case RTE_FIB_DIR24_8_2B:
		return dir24_8_lookup_bulk_1;
	case RTE_FIB_DIR24_8_4B:
		return dir24_8_lookup_bulk_2;
	case RTE_FIB_DIR24_8_8B:
		return dir24_8_lookup_bulk_3;
	default:
		return NULL;
	}
}

static inline rte_fib_lookup_fn_t
get_vector_fn(enum rte_fib_dir24_8_nh_sz nh_sz)
{
#ifdef CC_DIR24_8_AVX512_SUPPORT
	if ((rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) <= 0) ||
			(rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_512))
		return NULL;

	switch (nh_sz) {
	case RTE_FIB_DIR24_8_1B:
		return rte_dir24_8_vec_lookup_bulk_1b;
	case RTE_FIB_DIR24_8_2B:
		return rte_dir24_8_vec_lookup_bulk_2b;
	case RTE_FIB_DIR24_8_4B:
		return rte_dir24_8_vec_lookup_bulk_4b;
	case RTE_FIB_DIR24_8_8B:
		return rte_dir24_8_vec_lookup_bulk_8b;
	default:
		return NULL;
	}
#else
	RTE_SET_USED(nh_sz);
#endif
	return NULL;
}

rte_fib_lookup_fn_t
dir24_8_get_lookup_fn(void *p, enum rte_fib_lookup_type type)
{
	enum rte_fib_dir24_8_nh_sz nh_sz;
	rte_fib_lookup_fn_t ret_fn;
	struct dir24_8_tbl *dp = p;

	if (dp == NULL)
		return NULL;

	nh_sz = dp->nh_sz;

	switch (type) {
	case RTE_FIB_LOOKUP_DIR24_8_SCALAR_MACRO:
		return get_scalar_fn(nh_sz);
	case RTE_FIB_LOOKUP_DIR24_8_SCALAR_INLINE:
		return get_scalar_fn_inlined(nh_sz);
	case RTE_FIB_LOOKUP_DIR24_8_SCALAR_UNI:
		return dir24_8_lookup_bulk_uni;
	case RTE_FIB_LOOKUP_DIR24_8_VECTOR_AVX512:
		return get_vector_fn(nh_sz);
	case RTE_FIB_LOOKUP_DEFAULT:
		ret_fn = get_vector_fn(nh_sz);
		return (ret_fn != NULL) ? ret_fn : get_scalar_fn(nh_sz);
	default:
		return NULL;
	}

	return NULL;
}

static void
write_to_fib(void *ptr, uint64_t val, enum rte_fib_dir24_8_nh_sz size, int n)
{
	int i;
	uint8_t *ptr8 = (uint8_t *)ptr;
	uint16_t *ptr16 = (uint16_t *)ptr;
	uint32_t *ptr32 = (uint32_t *)ptr;
	uint64_t *ptr64 = (uint64_t *)ptr;

	switch (size) {
	case RTE_FIB_DIR24_8_1B:
		for (i = 0; i < n; i++)
			ptr8[i] = (uint8_t)val;
		break;
	case RTE_FIB_DIR24_8_2B:
		for (i = 0; i < n; i++)
			ptr16[i] = (uint16_t)val;
		break;
	case RTE_FIB_DIR24_8_4B:
		for (i = 0; i < n; i++)
			ptr32[i] = (uint32_t)val;
		break;
	case RTE_FIB_DIR24_8_8B:
		for (i = 0; i < n; i++)
			ptr64[i] = (uint64_t)val;
		break;
	}
}

static int
tbl8_get_idx(struct dir24_8_tbl *dp)
{
	uint32_t i;
	int bit_idx;

	for (i = 0; (i < (dp->number_tbl8s >> BITMAP_SLAB_BIT_SIZE_LOG2)) &&
			(dp->tbl8_idxes[i] == UINT64_MAX); i++)
		;
	if (i < (dp->number_tbl8s >> BITMAP_SLAB_BIT_SIZE_LOG2)) {
		bit_idx = __builtin_ctzll(~dp->tbl8_idxes[i]);
		dp->tbl8_idxes[i] |= (1ULL << bit_idx);
		return (i << BITMAP_SLAB_BIT_SIZE_LOG2) + bit_idx;
	}
	return -ENOSPC;
}

static inline void
tbl8_free_idx(struct dir24_8_tbl *dp, int idx)
{
	dp->tbl8_idxes[idx >> BITMAP_SLAB_BIT_SIZE_LOG2] &=
		~(1ULL << (idx & BITMAP_SLAB_BITMASK));
}

static int
tbl8_alloc(struct dir24_8_tbl *dp, uint64_t nh)
{
	int64_t	tbl8_idx;
	uint8_t	*tbl8_ptr;

	tbl8_idx = tbl8_get_idx(dp);
	if (tbl8_idx < 0)
		return tbl8_idx;
	tbl8_ptr = (uint8_t *)dp->tbl8 +
		((tbl8_idx * DIR24_8_TBL8_GRP_NUM_ENT) <<
		dp->nh_sz);
	/*Init tbl8 entries with nexthop from tbl24*/
	write_to_fib((void *)tbl8_ptr, nh|
		DIR24_8_EXT_ENT, dp->nh_sz,
		DIR24_8_TBL8_GRP_NUM_ENT);
	dp->cur_tbl8s++;
	return tbl8_idx;
}

static void
tbl8_recycle(struct dir24_8_tbl *dp, uint32_t ip, uint64_t tbl8_idx)
{
	uint32_t i;
	uint64_t nh;
	uint8_t *ptr8;
	uint16_t *ptr16;
	uint32_t *ptr32;
	uint64_t *ptr64;

	switch (dp->nh_sz) {
	case RTE_FIB_DIR24_8_1B:
		ptr8 = &((uint8_t *)dp->tbl8)[tbl8_idx *
				DIR24_8_TBL8_GRP_NUM_ENT];
		nh = *ptr8;
		for (i = 1; i < DIR24_8_TBL8_GRP_NUM_ENT; i++) {
			if (nh != ptr8[i])
				return;
		}
		((uint8_t *)dp->tbl24)[ip >> 8] =
			nh & ~DIR24_8_EXT_ENT;
		for (i = 0; i < DIR24_8_TBL8_GRP_NUM_ENT; i++)
			ptr8[i] = 0;
		break;
	case RTE_FIB_DIR24_8_2B:
		ptr16 = &((uint16_t *)dp->tbl8)[tbl8_idx *
				DIR24_8_TBL8_GRP_NUM_ENT];
		nh = *ptr16;
		for (i = 1; i < DIR24_8_TBL8_GRP_NUM_ENT; i++) {
			if (nh != ptr16[i])
				return;
		}
		((uint16_t *)dp->tbl24)[ip >> 8] =
			nh & ~DIR24_8_EXT_ENT;
		for (i = 0; i < DIR24_8_TBL8_GRP_NUM_ENT; i++)
			ptr16[i] = 0;
		break;
	case RTE_FIB_DIR24_8_4B:
		ptr32 = &((uint32_t *)dp->tbl8)[tbl8_idx *
				DIR24_8_TBL8_GRP_NUM_ENT];
		nh = *ptr32;
		for (i = 1; i < DIR24_8_TBL8_GRP_NUM_ENT; i++) {
			if (nh != ptr32[i])
				return;
		}
		((uint32_t *)dp->tbl24)[ip >> 8] =
			nh & ~DIR24_8_EXT_ENT;
		for (i = 0; i < DIR24_8_TBL8_GRP_NUM_ENT; i++)
			ptr32[i] = 0;
		break;
	case RTE_FIB_DIR24_8_8B:
		ptr64 = &((uint64_t *)dp->tbl8)[tbl8_idx *
				DIR24_8_TBL8_GRP_NUM_ENT];
		nh = *ptr64;
		for (i = 1; i < DIR24_8_TBL8_GRP_NUM_ENT; i++) {
			if (nh != ptr64[i])
				return;
		}
		((uint64_t *)dp->tbl24)[ip >> 8] =
			nh & ~DIR24_8_EXT_ENT;
		for (i = 0; i < DIR24_8_TBL8_GRP_NUM_ENT; i++)
			ptr64[i] = 0;
		break;
	}
	tbl8_free_idx(dp, tbl8_idx);
	dp->cur_tbl8s--;
}

static int
install_to_fib(struct dir24_8_tbl *dp, uint32_t ledge, uint32_t redge,
	uint64_t next_hop)
{
	uint64_t	tbl24_tmp;
	int	tbl8_idx;
	int tmp_tbl8_idx;
	uint8_t	*tbl8_ptr;
	uint32_t len;

	len = ((ledge == 0) && (redge == 0)) ? 1 << 24 :
		((redge & DIR24_8_TBL24_MASK) - ROUNDUP(ledge, 24)) >> 8;

	if (((ledge >> 8) != (redge >> 8)) || (len == 1 << 24)) {
		if ((ROUNDUP(ledge, 24) - ledge) != 0) {
			tbl24_tmp = get_tbl24(dp, ledge, dp->nh_sz);
			if ((tbl24_tmp & DIR24_8_EXT_ENT) !=
					DIR24_8_EXT_ENT) {
				/**
				 * Make sure there is space for two TBL8.
				 * This is necessary when installing range that
				 * needs tbl8 for ledge and redge.
				 */
				tbl8_idx = tbl8_alloc(dp, tbl24_tmp);
				tmp_tbl8_idx = tbl8_get_idx(dp);
				if (tbl8_idx < 0)
					return -ENOSPC;
				else if (tmp_tbl8_idx < 0) {
					tbl8_free_idx(dp, tbl8_idx);
					return -ENOSPC;
				}
				tbl8_free_idx(dp, tmp_tbl8_idx);
				/*update dir24 entry with tbl8 index*/
				write_to_fib(get_tbl24_p(dp, ledge,
					dp->nh_sz), (tbl8_idx << 1)|
					DIR24_8_EXT_ENT,
					dp->nh_sz, 1);
			} else
				tbl8_idx = tbl24_tmp >> 1;
			tbl8_ptr = (uint8_t *)dp->tbl8 +
				(((tbl8_idx * DIR24_8_TBL8_GRP_NUM_ENT) +
				(ledge & ~DIR24_8_TBL24_MASK)) <<
				dp->nh_sz);
			/*update tbl8 with new next hop*/
			write_to_fib((void *)tbl8_ptr, (next_hop << 1)|
				DIR24_8_EXT_ENT,
				dp->nh_sz, ROUNDUP(ledge, 24) - ledge);
			tbl8_recycle(dp, ledge, tbl8_idx);
		}
		write_to_fib(get_tbl24_p(dp, ROUNDUP(ledge, 24), dp->nh_sz),
			next_hop << 1, dp->nh_sz, len);
		if (redge & ~DIR24_8_TBL24_MASK) {
			tbl24_tmp = get_tbl24(dp, redge, dp->nh_sz);
			if ((tbl24_tmp & DIR24_8_EXT_ENT) !=
					DIR24_8_EXT_ENT) {
				tbl8_idx = tbl8_alloc(dp, tbl24_tmp);
				if (tbl8_idx < 0)
					return -ENOSPC;
				/*update dir24 entry with tbl8 index*/
				write_to_fib(get_tbl24_p(dp, redge,
					dp->nh_sz), (tbl8_idx << 1)|
					DIR24_8_EXT_ENT,
					dp->nh_sz, 1);
			} else
				tbl8_idx = tbl24_tmp >> 1;
			tbl8_ptr = (uint8_t *)dp->tbl8 +
				((tbl8_idx * DIR24_8_TBL8_GRP_NUM_ENT) <<
				dp->nh_sz);
			/*update tbl8 with new next hop*/
			write_to_fib((void *)tbl8_ptr, (next_hop << 1)|
				DIR24_8_EXT_ENT,
				dp->nh_sz, redge & ~DIR24_8_TBL24_MASK);
			tbl8_recycle(dp, redge, tbl8_idx);
		}
	} else if ((redge - ledge) != 0) {
		tbl24_tmp = get_tbl24(dp, ledge, dp->nh_sz);
		if ((tbl24_tmp & DIR24_8_EXT_ENT) !=
				DIR24_8_EXT_ENT) {
			tbl8_idx = tbl8_alloc(dp, tbl24_tmp);
			if (tbl8_idx < 0)
				return -ENOSPC;
			/*update dir24 entry with tbl8 index*/
			write_to_fib(get_tbl24_p(dp, ledge, dp->nh_sz),
				(tbl8_idx << 1)|
				DIR24_8_EXT_ENT,
				dp->nh_sz, 1);
		} else
			tbl8_idx = tbl24_tmp >> 1;
		tbl8_ptr = (uint8_t *)dp->tbl8 +
			(((tbl8_idx * DIR24_8_TBL8_GRP_NUM_ENT) +
			(ledge & ~DIR24_8_TBL24_MASK)) <<
			dp->nh_sz);
		/*update tbl8 with new next hop*/
		write_to_fib((void *)tbl8_ptr, (next_hop << 1)|
			DIR24_8_EXT_ENT,
			dp->nh_sz, redge - ledge);
		tbl8_recycle(dp, ledge, tbl8_idx);
	}
	return 0;
}

static int
modify_fib(struct dir24_8_tbl *dp, struct rte_rib *rib, uint32_t ip,
	uint8_t depth, uint64_t next_hop)
{
	struct rte_rib_node *tmp = NULL;
	uint32_t ledge, redge, tmp_ip;
	int ret;
	uint8_t tmp_depth;

	ledge = ip;
	do {
		tmp = rte_rib_get_nxt(rib, ip, depth, tmp,
			RTE_RIB_GET_NXT_COVER);
		if (tmp != NULL) {
			rte_rib_get_depth(tmp, &tmp_depth);
			if (tmp_depth == depth)
				continue;
			rte_rib_get_ip(tmp, &tmp_ip);
			redge = tmp_ip & rte_rib_depth_to_mask(tmp_depth);
			if (ledge == redge) {
				ledge = redge +
					(uint32_t)(1ULL << (32 - tmp_depth));
				continue;
			}
			ret = install_to_fib(dp, ledge, redge,
				next_hop);
			if (ret != 0)
				return ret;
			ledge = redge +
				(uint32_t)(1ULL << (32 - tmp_depth));
		} else {
			redge = ip + (uint32_t)(1ULL << (32 - depth));
			if (ledge == redge && ledge != 0)
				break;
			ret = install_to_fib(dp, ledge, redge,
				next_hop);
			if (ret != 0)
				return ret;
		}
	} while (tmp);

	return 0;
}

int
dir24_8_modify(struct rte_fib *fib, uint32_t ip, uint8_t depth,
	uint64_t next_hop, int op)
{
	struct dir24_8_tbl *dp;
	struct rte_rib *rib;
	struct rte_rib_node *tmp = NULL;
	struct rte_rib_node *node;
	struct rte_rib_node *parent;
	int ret = 0;
	uint64_t par_nh, node_nh;

	if ((fib == NULL) || (depth > RTE_FIB_MAXDEPTH))
		return -EINVAL;

	dp = rte_fib_get_dp(fib);
	rib = rte_fib_get_rib(fib);
	RTE_ASSERT((dp != NULL) && (rib != NULL));

	if (next_hop > get_max_nh(dp->nh_sz))
		return -EINVAL;

	ip &= rte_rib_depth_to_mask(depth);

	node = rte_rib_lookup_exact(rib, ip, depth);
	switch (op) {
	case RTE_FIB_ADD:
		if (node != NULL) {
			rte_rib_get_nh(node, &node_nh);
			if (node_nh == next_hop)
				return 0;
			ret = modify_fib(dp, rib, ip, depth, next_hop);
			if (ret == 0)
				rte_rib_set_nh(node, next_hop);
			return 0;
		}
		if (depth > 24) {
			tmp = rte_rib_get_nxt(rib, ip, 24, NULL,
				RTE_RIB_GET_NXT_COVER);
			if ((tmp == NULL) &&
				(dp->rsvd_tbl8s >= dp->number_tbl8s))
				return -ENOSPC;

		}
		node = rte_rib_insert(rib, ip, depth);
		if (node == NULL)
			return -rte_errno;
		rte_rib_set_nh(node, next_hop);
		parent = rte_rib_lookup_parent(node);
		if (parent != NULL) {
			rte_rib_get_nh(parent, &par_nh);
			if (par_nh == next_hop)
				return 0;
		}
		ret = modify_fib(dp, rib, ip, depth, next_hop);
		if (ret != 0) {
			rte_rib_remove(rib, ip, depth);
			return ret;
		}
		if ((depth > 24) && (tmp == NULL))
			dp->rsvd_tbl8s++;
		return 0;
	case RTE_FIB_DEL:
		if (node == NULL)
			return -ENOENT;

		parent = rte_rib_lookup_parent(node);
		if (parent != NULL) {
			rte_rib_get_nh(parent, &par_nh);
			rte_rib_get_nh(node, &node_nh);
			if (par_nh != node_nh)
				ret = modify_fib(dp, rib, ip, depth, par_nh);
		} else
			ret = modify_fib(dp, rib, ip, depth, dp->def_nh);
		if (ret == 0) {
			rte_rib_remove(rib, ip, depth);
			if (depth > 24) {
				tmp = rte_rib_get_nxt(rib, ip, 24, NULL,
					RTE_RIB_GET_NXT_COVER);
				if (tmp == NULL)
					dp->rsvd_tbl8s--;
			}
		}
		return ret;
	default:
		break;
	}
	return -EINVAL;
}

void *
dir24_8_create(const char *name, int socket_id, struct rte_fib_conf *fib_conf)
{
	char mem_name[DIR24_8_NAMESIZE];
	struct dir24_8_tbl *dp;
	uint64_t	def_nh;
	uint32_t	num_tbl8;
	enum rte_fib_dir24_8_nh_sz	nh_sz;

	if ((name == NULL) || (fib_conf == NULL) ||
			(fib_conf->dir24_8.nh_sz < RTE_FIB_DIR24_8_1B) ||
			(fib_conf->dir24_8.nh_sz > RTE_FIB_DIR24_8_8B) ||
			(fib_conf->dir24_8.num_tbl8 >
			get_max_nh(fib_conf->dir24_8.nh_sz)) ||
			(fib_conf->dir24_8.num_tbl8 == 0) ||
			(fib_conf->default_nh >
			get_max_nh(fib_conf->dir24_8.nh_sz))) {
		rte_errno = EINVAL;
		return NULL;
	}

	def_nh = fib_conf->default_nh;
	nh_sz = fib_conf->dir24_8.nh_sz;
	num_tbl8 = RTE_ALIGN_CEIL(fib_conf->dir24_8.num_tbl8,
			BITMAP_SLAB_BIT_SIZE);

	snprintf(mem_name, sizeof(mem_name), "DP_%s", name);
	dp = rte_zmalloc_socket(name, sizeof(struct dir24_8_tbl) +
		DIR24_8_TBL24_NUM_ENT * (1 << nh_sz), RTE_CACHE_LINE_SIZE,
		socket_id);
	if (dp == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	/* Init table with default value */
	write_to_fib(dp->tbl24, (def_nh << 1), nh_sz, 1 << 24);

	snprintf(mem_name, sizeof(mem_name), "TBL8_%p", dp);
	uint64_t tbl8_sz = DIR24_8_TBL8_GRP_NUM_ENT * (1ULL << nh_sz) *
			(num_tbl8 + 1);
	dp->tbl8 = rte_zmalloc_socket(mem_name, tbl8_sz,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (dp->tbl8 == NULL) {
		rte_errno = ENOMEM;
		rte_free(dp);
		return NULL;
	}
	dp->def_nh = def_nh;
	dp->nh_sz = nh_sz;
	dp->number_tbl8s = num_tbl8;

	snprintf(mem_name, sizeof(mem_name), "TBL8_idxes_%p", dp);
	dp->tbl8_idxes = rte_zmalloc_socket(mem_name,
			RTE_ALIGN_CEIL(dp->number_tbl8s, 64) >> 3,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (dp->tbl8_idxes == NULL) {
		rte_errno = ENOMEM;
		rte_free(dp->tbl8);
		rte_free(dp);
		return NULL;
	}

	return dp;
}

void
dir24_8_free(void *p)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;

	rte_free(dp->tbl8_idxes);
	rte_free(dp->tbl8);
	rte_free(dp);
}
