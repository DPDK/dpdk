/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_bitmap.h>
#include <rte_malloc.h>
#include "mlx5dr_internal.h"
#include "mlx5dr_buddy.h"

static struct rte_bitmap *bitmap_alloc0(int s)
{
	struct rte_bitmap *bitmap;
	uint32_t bmp_size;
	void *mem;

	bmp_size = rte_bitmap_get_memory_footprint(s);
	mem = rte_zmalloc("create_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (!mem) {
		DR_LOG(ERR, "No mem for bitmap");
		rte_errno = ENOMEM;
		return NULL;
	}

	bitmap = rte_bitmap_init(s, mem, bmp_size);
	if (!bitmap) {
		DR_LOG(ERR, "%s Failed to initialize bitmap", __func__);
		rte_errno = EINVAL;
		goto err_mem_alloc;
	}

	return bitmap;

err_mem_alloc:
	rte_free(mem);
	return NULL;
}

static void bitmap_set_bit(struct rte_bitmap *bmp, uint32_t pos)
{
	rte_bitmap_set(bmp, pos);
}

static void bitmap_clear_bit(struct rte_bitmap *bmp, uint32_t pos)
{
	rte_bitmap_clear(bmp, pos);
}

static bool bitmap_test_bit(struct rte_bitmap *bmp, unsigned long n)
{
	return !!rte_bitmap_get(bmp, n);
}

static unsigned long bitmap_ffs(struct rte_bitmap *bmap,
				uint64_t n, unsigned long m)
{
	uint64_t out_slab = 0;
	uint32_t pos = 0; /* Compilation warn */

	__rte_bitmap_scan_init(bmap);
	if (!rte_bitmap_scan(bmap, &pos, &out_slab)) {
		DR_LOG(ERR, "Failed to get slab from bitmap.");
		return m;
	}
	pos = pos + rte_ctz64(out_slab);

	if (pos < n) {
		DR_LOG(ERR, "Unexpected bit (%d < %"PRIx64") from bitmap", pos, n);
		return m;
	}
	return pos;
}

static unsigned long mlx5dr_buddy_find_first_bit(struct rte_bitmap *addr,
						 uint32_t size)
{
	return bitmap_ffs(addr, 0, size);
}

static int mlx5dr_buddy_init(struct mlx5dr_buddy_mem *buddy, uint32_t max_order)
{
	int i, s;

	buddy->max_order = max_order;

	buddy->bits = simple_calloc(buddy->max_order + 1, sizeof(long *));
	if (!buddy->bits) {
		rte_errno = ENOMEM;
		return -1;
	}

	buddy->num_free = simple_calloc(buddy->max_order + 1, sizeof(*buddy->num_free));
	if (!buddy->num_free) {
		rte_errno = ENOMEM;
		goto err_out_free_bits;
	}

	for (i = 0; i <= (int)buddy->max_order; ++i) {
		s = 1 << (buddy->max_order - i);
		buddy->bits[i] = bitmap_alloc0(s);
		if (!buddy->bits[i])
			goto err_out_free_num_free;
	}

	bitmap_set_bit(buddy->bits[buddy->max_order], 0);

	buddy->num_free[buddy->max_order] = 1;

	return 0;

err_out_free_num_free:
	for (i = 0; i <= (int)buddy->max_order; ++i)
		rte_free(buddy->bits[i]);

	simple_free(buddy->num_free);

err_out_free_bits:
	simple_free(buddy->bits);
	return -1;
}

struct mlx5dr_buddy_mem *mlx5dr_buddy_create(uint32_t max_order)
{
	struct mlx5dr_buddy_mem *buddy;

	buddy = simple_calloc(1, sizeof(*buddy));
	if (!buddy) {
		rte_errno = ENOMEM;
		return NULL;
	}

	if (mlx5dr_buddy_init(buddy, max_order))
		goto free_buddy;

	return buddy;

free_buddy:
	simple_free(buddy);
	return NULL;
}

void mlx5dr_buddy_cleanup(struct mlx5dr_buddy_mem *buddy)
{
	int i;

	for (i = 0; i <= (int)buddy->max_order; ++i)
		rte_free(buddy->bits[i]);

	simple_free(buddy->num_free);
	simple_free(buddy->bits);
}

int mlx5dr_buddy_alloc_mem(struct mlx5dr_buddy_mem *buddy, int order)
{
	int seg;
	int o, m;

	for (o = order; o <= (int)buddy->max_order; ++o)
		if (buddy->num_free[o]) {
			m = 1 << (buddy->max_order - o);
			seg = mlx5dr_buddy_find_first_bit(buddy->bits[o], m);
			if (m <= seg)
				return -1;

			goto found;
		}

	return -1;

found:
	bitmap_clear_bit(buddy->bits[o], seg);
	--buddy->num_free[o];

	while (o > order) {
		--o;
		seg <<= 1;
		bitmap_set_bit(buddy->bits[o], seg ^ 1);
		++buddy->num_free[o];
	}

	seg <<= order;

	return seg;
}

void mlx5dr_buddy_free_mem(struct mlx5dr_buddy_mem *buddy, uint32_t seg, int order)
{
	seg >>= order;

	while (bitmap_test_bit(buddy->bits[order], seg ^ 1)) {
		bitmap_clear_bit(buddy->bits[order], seg ^ 1);
		--buddy->num_free[order];
		seg >>= 1;
		++order;
	}

	bitmap_set_bit(buddy->bits[order], seg);

	++buddy->num_free[order];
}

