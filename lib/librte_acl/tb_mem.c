/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tb_mem.h"

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
		RTE_LOG(ERR, MALLOC, "%s(%zu)\n failed, currently allocated "
			"by pool: %zu bytes\n", __func__, sz, pool->alloc);
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
