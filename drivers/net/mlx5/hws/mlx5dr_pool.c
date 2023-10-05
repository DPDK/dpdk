/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_bitmap.h>
#include <rte_malloc.h>
#include "mlx5dr_buddy.h"
#include "mlx5dr_internal.h"

static void mlx5dr_pool_free_one_resource(struct mlx5dr_pool_resource *resource)
{
	mlx5dr_cmd_destroy_obj(resource->devx_obj);

	simple_free(resource);
}

static void mlx5dr_pool_resource_free(struct mlx5dr_pool *pool,
				      int resource_idx)
{
	mlx5dr_pool_free_one_resource(pool->resource[resource_idx]);
	pool->resource[resource_idx] = NULL;

	if (pool->tbl_type == MLX5DR_TABLE_TYPE_FDB) {
		mlx5dr_pool_free_one_resource(pool->mirror_resource[resource_idx]);
		pool->mirror_resource[resource_idx] = NULL;
	}
}

static struct mlx5dr_pool_resource *
mlx5dr_pool_create_one_resource(struct mlx5dr_pool *pool, uint32_t log_range,
				uint32_t fw_ft_type)
{
	struct mlx5dr_cmd_ste_create_attr ste_attr;
	struct mlx5dr_cmd_stc_create_attr stc_attr;
	struct mlx5dr_pool_resource *resource;
	struct mlx5dr_devx_obj *devx_obj;

	resource = simple_malloc(sizeof(*resource));
	if (!resource) {
		rte_errno = ENOMEM;
		return NULL;
	}

	switch (pool->type) {
	case MLX5DR_POOL_TYPE_STE:
		ste_attr.log_obj_range = log_range;
		ste_attr.table_type = fw_ft_type;
		devx_obj = mlx5dr_cmd_ste_create(pool->ctx->ibv_ctx, &ste_attr);
		break;
	case MLX5DR_POOL_TYPE_STC:
		stc_attr.log_obj_range = log_range;
		stc_attr.table_type = fw_ft_type;
		devx_obj = mlx5dr_cmd_stc_create(pool->ctx->ibv_ctx, &stc_attr);
		break;
	default:
		assert(0);
		break;
	}

	if (!devx_obj) {
		DR_LOG(ERR, "Failed to allocate resource objects");
		goto free_resource;
	}

	resource->pool = pool;
	resource->devx_obj = devx_obj;
	resource->range = 1 << log_range;
	resource->base_id = devx_obj->id;

	return resource;

free_resource:
	simple_free(resource);
	return NULL;
}

static int
mlx5dr_pool_resource_alloc(struct mlx5dr_pool *pool, uint32_t log_range, int idx)
{
	struct mlx5dr_pool_resource *resource;
	uint32_t fw_ft_type, opt_log_range;

	fw_ft_type = mlx5dr_table_get_res_fw_ft_type(pool->tbl_type, false);
	opt_log_range = pool->opt_type == MLX5DR_POOL_OPTIMIZE_ORIG ? 0 : log_range;
	resource = mlx5dr_pool_create_one_resource(pool, opt_log_range, fw_ft_type);
	if (!resource) {
		DR_LOG(ERR, "Failed allocating resource");
		return rte_errno;
	}
	pool->resource[idx] = resource;

	if (pool->tbl_type == MLX5DR_TABLE_TYPE_FDB) {
		struct mlx5dr_pool_resource *mir_resource;

		fw_ft_type = mlx5dr_table_get_res_fw_ft_type(pool->tbl_type, true);
		opt_log_range = pool->opt_type == MLX5DR_POOL_OPTIMIZE_MIRROR ? 0 : log_range;
		mir_resource = mlx5dr_pool_create_one_resource(pool, opt_log_range, fw_ft_type);
		if (!mir_resource) {
			DR_LOG(ERR, "Failed allocating mirrored resource");
			mlx5dr_pool_free_one_resource(resource);
			pool->resource[idx] = NULL;
			return rte_errno;
		}
		pool->mirror_resource[idx] = mir_resource;
	}

	return 0;
}

static int mlx5dr_pool_bitmap_get_free_slot(struct rte_bitmap *bitmap, uint32_t *iidx)
{
	uint64_t slab = 0;

	__rte_bitmap_scan_init(bitmap);

	if (!rte_bitmap_scan(bitmap, iidx, &slab))
		return ENOMEM;

	*iidx += rte_ctz64(slab);

	rte_bitmap_clear(bitmap, *iidx);

	return 0;
}

static struct rte_bitmap *mlx5dr_pool_create_and_init_bitmap(uint32_t log_range)
{
	struct rte_bitmap *cur_bmp;
	uint32_t bmp_size;
	void *mem;

	bmp_size = rte_bitmap_get_memory_footprint(1 << log_range);
	mem = rte_zmalloc("create_stc_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (!mem) {
		DR_LOG(ERR, "No mem for bitmap");
		rte_errno = ENOMEM;
		return NULL;
	}

	cur_bmp = rte_bitmap_init_with_all_set(1 << log_range, mem, bmp_size);
	if (!cur_bmp) {
		rte_free(mem);
		DR_LOG(ERR, "Failed to initialize stc bitmap.");
		rte_errno = ENOMEM;
		return NULL;
	}

	return cur_bmp;
}

static void mlx5dr_pool_buddy_db_put_chunk(struct mlx5dr_pool *pool,
				      struct mlx5dr_pool_chunk *chunk)
{
	struct mlx5dr_buddy_mem *buddy;

	buddy = pool->db.buddy_manager->buddies[chunk->resource_idx];
	if (!buddy) {
		assert(false);
		DR_LOG(ERR, "No such buddy (%d)", chunk->resource_idx);
		return;
	}

	mlx5dr_buddy_free_mem(buddy, chunk->offset, chunk->order);
}

static struct mlx5dr_buddy_mem *
mlx5dr_pool_buddy_get_next_buddy(struct mlx5dr_pool *pool, int idx,
				 uint32_t order, bool *is_new_buddy)
{
	static struct mlx5dr_buddy_mem *buddy;
	uint32_t new_buddy_size;

	buddy = pool->db.buddy_manager->buddies[idx];
	if (buddy)
		return buddy;

	new_buddy_size = RTE_MAX(pool->alloc_log_sz, order);
	*is_new_buddy = true;
	buddy = mlx5dr_buddy_create(new_buddy_size);
	if (!buddy) {
		DR_LOG(ERR, "Failed to create buddy order: %d index: %d",
		       new_buddy_size, idx);
		return NULL;
	}

	if (mlx5dr_pool_resource_alloc(pool, new_buddy_size, idx) != 0) {
		DR_LOG(ERR, "Failed to create resource type: %d: size %d index: %d",
			pool->type, new_buddy_size, idx);
		mlx5dr_buddy_cleanup(buddy);
		return NULL;
	}

	pool->db.buddy_manager->buddies[idx] = buddy;

	return buddy;
}

static int mlx5dr_pool_buddy_get_mem_chunk(struct mlx5dr_pool *pool,
					   int order,
					   uint32_t *buddy_idx,
					   int *seg)
{
	struct mlx5dr_buddy_mem *buddy;
	bool new_mem = false;
	int err = 0;
	int i;

	*seg = -1;

	/* Find the next free place from the buddy array */
	while (*seg == -1) {
		for (i = 0; i < MLX5DR_POOL_RESOURCE_ARR_SZ; i++) {
			buddy = mlx5dr_pool_buddy_get_next_buddy(pool, i,
								 order,
								 &new_mem);
			if (!buddy) {
				err = rte_errno;
				goto out;
			}

			*seg = mlx5dr_buddy_alloc_mem(buddy, order);
			if (*seg != -1)
				goto found;

			if (pool->flags & MLX5DR_POOL_FLAGS_ONE_RESOURCE) {
				DR_LOG(ERR, "Fail to allocate seg for one resource pool");
				err = rte_errno;
				goto out;
			}

			if (new_mem) {
				/* We have new memory pool, should be place for us */
				assert(false);
				DR_LOG(ERR, "No memory for order: %d with buddy no: %d",
					order, i);
				rte_errno = ENOMEM;
				err = ENOMEM;
				goto out;
			}
		}
	}

found:
	*buddy_idx = i;
out:
	return err;
}

static int mlx5dr_pool_buddy_db_get_chunk(struct mlx5dr_pool *pool,
				     struct mlx5dr_pool_chunk *chunk)
{
	int ret = 0;

	/* Go over the buddies and find next free slot */
	ret = mlx5dr_pool_buddy_get_mem_chunk(pool, chunk->order,
					      &chunk->resource_idx,
					      &chunk->offset);
	if (ret)
		DR_LOG(ERR, "Failed to get free slot for chunk with order: %d",
			chunk->order);

	return ret;
}

static void mlx5dr_pool_buddy_db_uninit(struct mlx5dr_pool *pool)
{
	struct mlx5dr_buddy_mem *buddy;
	int i;

	for (i = 0; i < MLX5DR_POOL_RESOURCE_ARR_SZ; i++) {
		buddy = pool->db.buddy_manager->buddies[i];
		if (buddy) {
			mlx5dr_buddy_cleanup(buddy);
			simple_free(buddy);
			pool->db.buddy_manager->buddies[i] = NULL;
		}
	}

	simple_free(pool->db.buddy_manager);
}

static int mlx5dr_pool_buddy_db_init(struct mlx5dr_pool *pool, uint32_t log_range)
{
	pool->db.buddy_manager = simple_calloc(1, sizeof(*pool->db.buddy_manager));
	if (!pool->db.buddy_manager) {
		DR_LOG(ERR, "No mem for buddy_manager with log_range: %d", log_range);
		rte_errno = ENOMEM;
		return rte_errno;
	}

	if (pool->flags & MLX5DR_POOL_FLAGS_ALLOC_MEM_ON_CREATE) {
		bool new_buddy;

		if (!mlx5dr_pool_buddy_get_next_buddy(pool, 0, log_range, &new_buddy)) {
			DR_LOG(ERR, "Failed allocating memory on create log_sz: %d", log_range);
			simple_free(pool->db.buddy_manager);
			return rte_errno;
		}
	}

	pool->p_db_uninit = &mlx5dr_pool_buddy_db_uninit;
	pool->p_get_chunk = &mlx5dr_pool_buddy_db_get_chunk;
	pool->p_put_chunk = &mlx5dr_pool_buddy_db_put_chunk;

	return 0;
}

static int mlx5dr_pool_create_resource_on_index(struct mlx5dr_pool *pool,
						uint32_t alloc_size, int idx)
{
	if (mlx5dr_pool_resource_alloc(pool, alloc_size, idx) != 0) {
		DR_LOG(ERR, "Failed to create resource type: %d: size %d index: %d",
			pool->type, alloc_size, idx);
		return rte_errno;
	}

	return 0;
}

static struct mlx5dr_pool_elements *
mlx5dr_pool_element_create_new_elem(struct mlx5dr_pool *pool, uint32_t order, int idx)
{
	struct mlx5dr_pool_elements *elem;
	uint32_t alloc_size;

	alloc_size = pool->alloc_log_sz;

	elem = simple_calloc(1, sizeof(*elem));
	if (!elem) {
		DR_LOG(ERR, "Failed to create elem order: %d index: %d",
		       order, idx);
		rte_errno = ENOMEM;
		return NULL;
	}
	/*sharing the same resource, also means that all the elements are with size 1*/
	if ((pool->flags & MLX5DR_POOL_FLAGS_FIXED_SIZE_OBJECTS) &&
	    !(pool->flags & MLX5DR_POOL_FLAGS_RESOURCE_PER_CHUNK)) {
		 /* Currently all chunks in size 1 */
		elem->bitmap =  mlx5dr_pool_create_and_init_bitmap(alloc_size - order);
		if (!elem->bitmap) {
			DR_LOG(ERR, "Failed to create bitmap type: %d: size %d index: %d",
			       pool->type, alloc_size, idx);
			goto free_elem;
		}
	}

	if (mlx5dr_pool_create_resource_on_index(pool, alloc_size, idx)) {
		DR_LOG(ERR, "Failed to create resource type: %d: size %d index: %d",
			pool->type, alloc_size, idx);
		goto free_db;
	}

	pool->db.element_manager->elements[idx] = elem;

	return elem;

free_db:
	rte_free(elem->bitmap);
free_elem:
	simple_free(elem);
	return NULL;
}

static int mlx5dr_pool_element_find_seg(struct mlx5dr_pool_elements *elem, int *seg)
{
	if (mlx5dr_pool_bitmap_get_free_slot(elem->bitmap, (uint32_t *)seg)) {
		elem->is_full = true;
		return ENOMEM;
	}
	return 0;
}

static int
mlx5dr_pool_onesize_element_get_mem_chunk(struct mlx5dr_pool *pool, uint32_t order,
					  uint32_t *idx, int *seg)
{
	struct mlx5dr_pool_elements *elem;

	elem = pool->db.element_manager->elements[0];
	if (!elem)
		elem = mlx5dr_pool_element_create_new_elem(pool, order, 0);
	if (!elem)
		goto err_no_elem;

	*idx = 0;

	if (mlx5dr_pool_element_find_seg(elem, seg) != 0) {
		DR_LOG(ERR, "No more resources (last request order: %d)", order);
		rte_errno = ENOMEM;
		return ENOMEM;
	}

	elem->num_of_elements++;
	return 0;

err_no_elem:
	DR_LOG(ERR, "Failed to allocate element for order: %d", order);
	return ENOMEM;
}

static int
mlx5dr_pool_general_element_get_mem_chunk(struct mlx5dr_pool *pool, uint32_t order,
					  uint32_t *idx, int *seg)
{
	int ret;
	int i;

	for (i = 0; i < MLX5DR_POOL_RESOURCE_ARR_SZ; i++) {
		if (!pool->resource[i]) {
			ret = mlx5dr_pool_create_resource_on_index(pool, order, i);
			if (ret)
				goto err_no_res;
			*idx = i;
			*seg = 0; /* One memory slot in that element */
			return 0;
		}
	}

	rte_errno = ENOMEM;
	DR_LOG(ERR, "No more resources (last request order: %d)", order);
	return ENOMEM;

err_no_res:
	DR_LOG(ERR, "Failed to allocate element for order: %d", order);
	return ENOMEM;
}

static int mlx5dr_pool_general_element_db_get_chunk(struct mlx5dr_pool *pool,
						    struct mlx5dr_pool_chunk *chunk)
{
	int ret;

	/* Go over all memory elements and find/allocate free slot */
	ret = mlx5dr_pool_general_element_get_mem_chunk(pool, chunk->order,
							&chunk->resource_idx,
							&chunk->offset);
	if (ret)
		DR_LOG(ERR, "Failed to get free slot for chunk with order: %d",
			chunk->order);

	return ret;
}

static void mlx5dr_pool_general_element_db_put_chunk(struct mlx5dr_pool *pool,
						     struct mlx5dr_pool_chunk *chunk)
{
	assert(pool->resource[chunk->resource_idx]);

	if (pool->flags & MLX5DR_POOL_FLAGS_RELEASE_FREE_RESOURCE)
		mlx5dr_pool_resource_free(pool, chunk->resource_idx);
}

static void mlx5dr_pool_general_element_db_uninit(struct mlx5dr_pool *pool)
{
	(void)pool;
}

/* This memory management works as the following:
 * - At start doesn't allocate no mem at all.
 * - When new request for chunk arrived:
 *	allocate resource and give it.
 * - When free that chunk:
 *	the resource is freed.
 */
static int mlx5dr_pool_general_element_db_init(struct mlx5dr_pool *pool)
{
	pool->p_db_uninit = &mlx5dr_pool_general_element_db_uninit;
	pool->p_get_chunk = &mlx5dr_pool_general_element_db_get_chunk;
	pool->p_put_chunk = &mlx5dr_pool_general_element_db_put_chunk;

	return 0;
}

static void mlx5dr_onesize_element_db_destroy_element(struct mlx5dr_pool *pool,
						      struct mlx5dr_pool_elements *elem,
						      struct mlx5dr_pool_chunk *chunk)
{
	assert(pool->resource[chunk->resource_idx]);

	mlx5dr_pool_resource_free(pool, chunk->resource_idx);

	simple_free(elem);
	pool->db.element_manager->elements[chunk->resource_idx] = NULL;
}

static void mlx5dr_onesize_element_db_put_chunk(struct mlx5dr_pool *pool,
						struct mlx5dr_pool_chunk *chunk)
{
	struct mlx5dr_pool_elements *elem;

	assert(chunk->resource_idx == 0);

	elem = pool->db.element_manager->elements[chunk->resource_idx];
	if (!elem) {
		assert(false);
		DR_LOG(ERR, "No such element (%d)", chunk->resource_idx);
		return;
	}

	rte_bitmap_set(elem->bitmap, chunk->offset);
	elem->is_full = false;
	elem->num_of_elements--;

	if (pool->flags & MLX5DR_POOL_FLAGS_RELEASE_FREE_RESOURCE &&
	   !elem->num_of_elements)
		mlx5dr_onesize_element_db_destroy_element(pool, elem, chunk);
}

static int mlx5dr_onesize_element_db_get_chunk(struct mlx5dr_pool *pool,
					       struct mlx5dr_pool_chunk *chunk)
{
	int ret = 0;

	/* Go over all memory elements and find/allocate free slot */
	ret = mlx5dr_pool_onesize_element_get_mem_chunk(pool, chunk->order,
							&chunk->resource_idx,
							&chunk->offset);
	if (ret)
		DR_LOG(ERR, "Failed to get free slot for chunk with order: %d",
			chunk->order);

	return ret;
}

static void mlx5dr_onesize_element_db_uninit(struct mlx5dr_pool *pool)
{
	struct mlx5dr_pool_elements *elem;
	int i;

	for (i = 0; i < MLX5DR_POOL_RESOURCE_ARR_SZ; i++) {
		elem = pool->db.element_manager->elements[i];
		if (elem) {
			rte_free(elem->bitmap);
			simple_free(elem);
			pool->db.element_manager->elements[i] = NULL;
		}
	}
	simple_free(pool->db.element_manager);
}

/* This memory management works as the following:
 * - At start doesn't allocate no mem at all.
 * - When new request for chunk arrived:
 *  aloocate the first and only slot of memory/resource
 *  when it ended return error.
 */
static int mlx5dr_pool_onesize_element_db_init(struct mlx5dr_pool *pool)
{
	pool->db.element_manager = simple_calloc(1, sizeof(*pool->db.element_manager));
	if (!pool->db.element_manager) {
		DR_LOG(ERR, "No mem for general elemnt_manager");
		rte_errno = ENOMEM;
		return rte_errno;
	}

	pool->p_db_uninit = &mlx5dr_onesize_element_db_uninit;
	pool->p_get_chunk = &mlx5dr_onesize_element_db_get_chunk;
	pool->p_put_chunk = &mlx5dr_onesize_element_db_put_chunk;

	return 0;
}

static int mlx5dr_pool_db_init(struct mlx5dr_pool *pool,
			       enum mlx5dr_db_type db_type)
{
	int ret;

	if (db_type == MLX5DR_POOL_DB_TYPE_GENERAL_SIZE)
		ret = mlx5dr_pool_general_element_db_init(pool);
	else if (db_type == MLX5DR_POOL_DB_TYPE_ONE_SIZE_RESOURCE)
		ret = mlx5dr_pool_onesize_element_db_init(pool);
	else
		ret = mlx5dr_pool_buddy_db_init(pool, pool->alloc_log_sz);

	if (ret) {
		DR_LOG(ERR, "Failed to init general db : %d (ret: %d)", db_type, ret);
		return ret;
	}

	return 0;
}

static void mlx5dr_pool_db_unint(struct mlx5dr_pool *pool)
{
	pool->p_db_uninit(pool);
}

int
mlx5dr_pool_chunk_alloc(struct mlx5dr_pool *pool,
			struct mlx5dr_pool_chunk *chunk)
{
	int ret;

	pthread_spin_lock(&pool->lock);
	ret = pool->p_get_chunk(pool, chunk);
	pthread_spin_unlock(&pool->lock);

	return ret;
}

void mlx5dr_pool_chunk_free(struct mlx5dr_pool *pool,
			    struct mlx5dr_pool_chunk *chunk)
{
	pthread_spin_lock(&pool->lock);
	pool->p_put_chunk(pool, chunk);
	pthread_spin_unlock(&pool->lock);
}

struct mlx5dr_pool *
mlx5dr_pool_create(struct mlx5dr_context *ctx, struct mlx5dr_pool_attr *pool_attr)
{
	enum mlx5dr_db_type res_db_type;
	struct mlx5dr_pool *pool;

	pool = simple_calloc(1, sizeof(*pool));
	if (!pool)
		return NULL;

	pool->ctx = ctx;
	pool->type = pool_attr->pool_type;
	pool->alloc_log_sz = pool_attr->alloc_log_sz;
	pool->flags = pool_attr->flags;
	pool->tbl_type = pool_attr->table_type;
	pool->opt_type = pool_attr->opt_type;

	pthread_spin_init(&pool->lock, PTHREAD_PROCESS_PRIVATE);

	/* Support general db */
	if (pool->flags == (MLX5DR_POOL_FLAGS_RELEASE_FREE_RESOURCE |
			    MLX5DR_POOL_FLAGS_RESOURCE_PER_CHUNK))
		res_db_type = MLX5DR_POOL_DB_TYPE_GENERAL_SIZE;
	else if (pool->flags == (MLX5DR_POOL_FLAGS_ONE_RESOURCE |
				 MLX5DR_POOL_FLAGS_FIXED_SIZE_OBJECTS))
		res_db_type = MLX5DR_POOL_DB_TYPE_ONE_SIZE_RESOURCE;
	else
		res_db_type = MLX5DR_POOL_DB_TYPE_BUDDY;

	pool->alloc_log_sz = pool_attr->alloc_log_sz;

	if (mlx5dr_pool_db_init(pool, res_db_type))
		goto free_pool;

	return pool;

free_pool:
	pthread_spin_destroy(&pool->lock);
	simple_free(pool);
	return NULL;
}

int mlx5dr_pool_destroy(struct mlx5dr_pool *pool)
{
	int i;

	for (i = 0; i < MLX5DR_POOL_RESOURCE_ARR_SZ; i++)
		if (pool->resource[i])
			mlx5dr_pool_resource_free(pool, i);

	mlx5dr_pool_db_unint(pool);

	pthread_spin_destroy(&pool->lock);
	simple_free(pool);
	return 0;
}
