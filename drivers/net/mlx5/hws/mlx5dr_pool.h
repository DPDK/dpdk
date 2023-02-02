/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_POOL_H_
#define MLX5DR_POOL_H_

enum mlx5dr_pool_type {
	MLX5DR_POOL_TYPE_STE,
	MLX5DR_POOL_TYPE_STC,
};

#define MLX5DR_POOL_STC_LOG_SZ 15

#define MLX5DR_POOL_RESOURCE_ARR_SZ 100

struct mlx5dr_pool_chunk {
	uint32_t resource_idx;
	/* Internal offset, relative to base index */
	int      offset;
	int      order;
};

struct mlx5dr_pool_resource {
	struct mlx5dr_pool *pool;
	struct mlx5dr_devx_obj *devx_obj;
	uint32_t base_id;
	uint32_t range;
};

enum mlx5dr_pool_flags {
	/* Only a one resource in that pool */
	MLX5DR_POOL_FLAGS_ONE_RESOURCE = 1 << 0,
	MLX5DR_POOL_FLAGS_RELEASE_FREE_RESOURCE = 1 << 1,
	/* No sharing resources between chunks */
	MLX5DR_POOL_FLAGS_RESOURCE_PER_CHUNK = 1 << 2,
	/* All objects are in the same size */
	MLX5DR_POOL_FLAGS_FIXED_SIZE_OBJECTS = 1 << 3,
	/* Manged by buddy allocator */
	MLX5DR_POOL_FLAGS_BUDDY_MANAGED = 1 << 4,
	/* Allocate pool_type memory on pool creation */
	MLX5DR_POOL_FLAGS_ALLOC_MEM_ON_CREATE = 1 << 5,

	/* These values should be used by the caller */
	MLX5DR_POOL_FLAGS_FOR_STC_POOL =
		MLX5DR_POOL_FLAGS_ONE_RESOURCE |
		MLX5DR_POOL_FLAGS_FIXED_SIZE_OBJECTS,
	MLX5DR_POOL_FLAGS_FOR_MATCHER_STE_POOL =
		MLX5DR_POOL_FLAGS_RELEASE_FREE_RESOURCE |
		MLX5DR_POOL_FLAGS_RESOURCE_PER_CHUNK,
	MLX5DR_POOL_FLAGS_FOR_STE_ACTION_POOL =
		MLX5DR_POOL_FLAGS_ONE_RESOURCE |
		MLX5DR_POOL_FLAGS_BUDDY_MANAGED |
		MLX5DR_POOL_FLAGS_ALLOC_MEM_ON_CREATE,
};

enum mlx5dr_pool_optimize {
	MLX5DR_POOL_OPTIMIZE_NONE = 0x0,
	MLX5DR_POOL_OPTIMIZE_ORIG = 0x1,
	MLX5DR_POOL_OPTIMIZE_MIRROR = 0x2,
};

struct mlx5dr_pool_attr {
	enum mlx5dr_pool_type pool_type;
	enum mlx5dr_table_type table_type;
	enum mlx5dr_pool_flags flags;
	enum mlx5dr_pool_optimize opt_type;
	/* Allocation size once memory is depleted */
	size_t alloc_log_sz;
};

enum mlx5dr_db_type {
	/* Uses for allocating chunk of big memory, each element has its own resource in the FW*/
	MLX5DR_POOL_DB_TYPE_GENERAL_SIZE,
	/* One resource only, all the elements are with same one size */
	MLX5DR_POOL_DB_TYPE_ONE_SIZE_RESOURCE,
	/* Many resources, the memory allocated with buddy mechanism */
	MLX5DR_POOL_DB_TYPE_BUDDY,
};

struct mlx5dr_buddy_manager {
	struct mlx5dr_buddy_mem *buddies[MLX5DR_POOL_RESOURCE_ARR_SZ];
};

struct mlx5dr_pool_elements {
	uint32_t num_of_elements;
	struct rte_bitmap *bitmap;
	bool is_full;
};

struct mlx5dr_element_manager {
	struct mlx5dr_pool_elements *elements[MLX5DR_POOL_RESOURCE_ARR_SZ];
};

struct mlx5dr_pool_db {
	enum mlx5dr_db_type type;
	union {
		struct mlx5dr_element_manager *element_manager;
		struct mlx5dr_buddy_manager *buddy_manager;
	};
};

typedef int (*mlx5dr_pool_db_get_chunk)(struct mlx5dr_pool *pool,
					struct mlx5dr_pool_chunk *chunk);
typedef void (*mlx5dr_pool_db_put_chunk)(struct mlx5dr_pool *pool,
					 struct mlx5dr_pool_chunk *chunk);
typedef void (*mlx5dr_pool_unint_db)(struct mlx5dr_pool *pool);

struct mlx5dr_pool {
	struct mlx5dr_context *ctx;
	enum mlx5dr_pool_type type;
	enum mlx5dr_pool_flags flags;
	pthread_spinlock_t lock;
	size_t alloc_log_sz;
	enum mlx5dr_table_type tbl_type;
	enum mlx5dr_pool_optimize opt_type;
	struct mlx5dr_pool_resource *resource[MLX5DR_POOL_RESOURCE_ARR_SZ];
	struct mlx5dr_pool_resource *mirror_resource[MLX5DR_POOL_RESOURCE_ARR_SZ];
	/* DB */
	struct mlx5dr_pool_db db;
	/* Functions */
	mlx5dr_pool_unint_db p_db_uninit;
	mlx5dr_pool_db_get_chunk p_get_chunk;
	mlx5dr_pool_db_put_chunk p_put_chunk;
};

struct mlx5dr_pool *
mlx5dr_pool_create(struct mlx5dr_context *ctx,
		   struct mlx5dr_pool_attr *pool_attr);

int mlx5dr_pool_destroy(struct mlx5dr_pool *pool);

int mlx5dr_pool_chunk_alloc(struct mlx5dr_pool *pool,
			    struct mlx5dr_pool_chunk *chunk);

void mlx5dr_pool_chunk_free(struct mlx5dr_pool *pool,
			    struct mlx5dr_pool_chunk *chunk);

static inline struct mlx5dr_devx_obj *
mlx5dr_pool_chunk_get_base_devx_obj(struct mlx5dr_pool *pool,
				    struct mlx5dr_pool_chunk *chunk)
{
	return pool->resource[chunk->resource_idx]->devx_obj;
}

static inline struct mlx5dr_devx_obj *
mlx5dr_pool_chunk_get_base_devx_obj_mirror(struct mlx5dr_pool *pool,
					   struct mlx5dr_pool_chunk *chunk)
{
	return pool->mirror_resource[chunk->resource_idx]->devx_obj;
}
#endif /* MLX5DR_POOL_H_ */
