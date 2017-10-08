/*
 *   BSD LICENSE
 *
 *   Copyright (C) 2017 Cavium Inc. All rights reserved.
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
#include <stdio.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "octeontx_fpavf.h"

static int
octeontx_fpavf_alloc(struct rte_mempool *mp)
{
	uintptr_t pool;
	uint32_t memseg_count = mp->size;
	uint32_t object_size;
	uintptr_t va_start;
	int rc = 0;

	/* virtual hugepage mapped addr */
	va_start = ~(uint64_t)0;

	object_size = mp->elt_size + mp->header_size + mp->trailer_size;

	pool = octeontx_fpa_bufpool_create(object_size, memseg_count,
						OCTEONTX_FPAVF_BUF_OFFSET,
						(char **)&va_start,
						mp->socket_id);
	rc = octeontx_fpa_bufpool_block_size(pool);
	if (rc < 0)
		goto _end;

	if ((uint32_t)rc != object_size)
		fpavf_log_err("buffer size mismatch: %d instead of %u\n",
				rc, object_size);

	fpavf_log_info("Pool created %p with .. ", (void *)pool);
	fpavf_log_info("obj_sz %d, cnt %d\n", object_size, memseg_count);

	/* assign pool handle to mempool */
	mp->pool_id = (uint64_t)pool;

	return 0;

_end:
	return rc;
}

static void
octeontx_fpavf_free(struct rte_mempool *mp)
{
	uintptr_t pool;

	pool = (uintptr_t)mp->pool_id;

	octeontx_fpa_bufpool_destroy(pool, mp->socket_id);
}

static __rte_always_inline void *
octeontx_fpa_bufpool_alloc(uintptr_t handle)
{
	return (void *)(uintptr_t)fpavf_read64((void *)(handle +
						FPA_VF_VHAURA_OP_ALLOC(0)));
}

static __rte_always_inline void
octeontx_fpa_bufpool_free(uintptr_t handle, void *buf)
{
	uint64_t free_addr = FPA_VF_FREE_ADDRS_S(FPA_VF_VHAURA_OP_FREE(0),
						 0 /* DWB */, 1 /* FABS */);

	fpavf_write64((uintptr_t)buf, (void *)(uintptr_t)(handle + free_addr));
}

static int
octeontx_fpavf_enqueue(struct rte_mempool *mp, void * const *obj_table,
			unsigned int n)
{
	uintptr_t pool;
	unsigned int index;

	pool = (uintptr_t)mp->pool_id;
	/* Get pool bar address from handle */
	pool &= ~(uint64_t)FPA_GPOOL_MASK;
	for (index = 0; index < n; index++, obj_table++)
		octeontx_fpa_bufpool_free(pool, *obj_table);

	return 0;
}

static int
octeontx_fpavf_dequeue(struct rte_mempool *mp, void **obj_table,
			unsigned int n)
{
	unsigned int index;
	uintptr_t pool;
	void *obj;

	pool = (uintptr_t)mp->pool_id;
	/* Get pool bar address from handle */
	pool &= ~(uint64_t)FPA_GPOOL_MASK;
	for (index = 0; index < n; index++, obj_table++) {
		obj = octeontx_fpa_bufpool_alloc(pool);
		if (obj == NULL) {
			/*
			 * Failed to allocate the requested number of objects
			 * from the pool. Current pool implementation requires
			 * completing the entire request or returning error
			 * otherwise.
			 * Free already allocated buffers to the pool.
			 */
			for (; index > 0; index--) {
				obj_table--;
				octeontx_fpa_bufpool_free(pool, *obj_table);
			}
			return -ENOMEM;
		}
		*obj_table = obj;
	}

	return 0;
}

static unsigned int
octeontx_fpavf_get_count(const struct rte_mempool *mp)
{
	uintptr_t pool;

	pool = (uintptr_t)mp->pool_id;

	return octeontx_fpa_bufpool_free_count(pool);
}

static struct rte_mempool_ops octeontx_fpavf_ops = {
	.name = "octeontx_fpavf",
	.alloc = octeontx_fpavf_alloc,
	.free = octeontx_fpavf_free,
	.enqueue = octeontx_fpavf_enqueue,
	.dequeue = octeontx_fpavf_dequeue,
	.get_count = octeontx_fpavf_get_count,
	.get_capabilities = NULL,
	.register_memory_area = NULL,
};

MEMPOOL_REGISTER_OPS(octeontx_fpavf_ops);
