/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
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
	int rc = 0;

	object_size = mp->elt_size + mp->header_size + mp->trailer_size;

	pool = octeontx_fpa_bufpool_create(object_size, memseg_count,
						OCTEONTX_FPAVF_BUF_OFFSET,
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

static int
octeontx_fpavf_get_capabilities(const struct rte_mempool *mp,
				unsigned int *flags)
{
	RTE_SET_USED(mp);
	*flags |= (MEMPOOL_F_CAPA_PHYS_CONTIG |
			MEMPOOL_F_CAPA_BLK_ALIGNED_OBJECTS);
	return 0;
}

static int
octeontx_fpavf_register_memory_area(const struct rte_mempool *mp,
				    char *vaddr, rte_iova_t paddr, size_t len)
{
	RTE_SET_USED(paddr);
	uint8_t gpool;
	uintptr_t pool_bar;

	gpool = octeontx_fpa_bufpool_gpool(mp->pool_id);
	pool_bar = mp->pool_id & ~(uint64_t)FPA_GPOOL_MASK;

	return octeontx_fpavf_pool_set_range(pool_bar, len, vaddr, gpool);
}

static struct rte_mempool_ops octeontx_fpavf_ops = {
	.name = "octeontx_fpavf",
	.alloc = octeontx_fpavf_alloc,
	.free = octeontx_fpavf_free,
	.enqueue = octeontx_fpavf_enqueue,
	.dequeue = octeontx_fpavf_dequeue,
	.get_count = octeontx_fpavf_get_count,
	.get_capabilities = octeontx_fpavf_get_capabilities,
	.register_memory_area = octeontx_fpavf_register_memory_area,
};

MEMPOOL_REGISTER_OPS(octeontx_fpavf_ops);
