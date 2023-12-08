/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "tb_mem.h"
#include "acl_log.h"

/*
 *  Memory management routines for temporary memory.
 *  That memory is used only during build phase and is released after
 *  build is finished.
 *  Note, that tb_pool/tb_alloc() are not supposed to return NULL.
 *  Instead, in the case of failure to allocate memory,
 *  it would do siglongjmp(pool->fail).
 *  It is responsibility of the caller to save the proper context/environment,
 *  in the pool->fail before calling tb_alloc() for the given pool first time.
 */

static struct tb_mem_block *
tb_pool(struct tb_mem_pool *pool, size_t sz)
{
	struct tb_mem_block *block;
	uint8_t *ptr;
	size_t size;

	size = sz + pool->alignment - 1;
	block = calloc(1, size + sizeof(*pool->block));
	if (block == NULL) {
		ACL_LOG(ERR, "%s(%zu) failed, currently allocated by pool: %zu bytes",
			__func__, sz, pool->alloc);
		siglongjmp(pool->fail, -ENOMEM);
		return NULL;
	}

	block->pool = pool;

	block->next = pool->block;
	pool->block = block;

	pool->alloc += size;

	ptr = (uint8_t *)(block + 1);
	block->mem = RTE_PTR_ALIGN_CEIL(ptr, pool->alignment);
	block->size = size - (block->mem - ptr);

	return block;
}

void *
tb_alloc(struct tb_mem_pool *pool, size_t size)
{
	struct tb_mem_block *block;
	void *ptr;
	size_t new_sz;

	size = RTE_ALIGN_CEIL(size, pool->alignment);

	block = pool->block;
	if (block == NULL || block->size < size) {
		new_sz = (size > pool->min_alloc) ? size : pool->min_alloc;
		block = tb_pool(pool, new_sz);
	}
	ptr = block->mem;
	block->size -= size;
	block->mem += size;
	return ptr;
}

void
tb_free_pool(struct tb_mem_pool *pool)
{
	struct tb_mem_block *next, *block;

	for (block = pool->block; block != NULL; block = next) {
		next = block->next;
		free(block);
	}
	pool->block = NULL;
	pool->alloc = 0;
}
