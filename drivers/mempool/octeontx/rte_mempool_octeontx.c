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

static ssize_t
octeontx_fpavf_calc_mem_size(const struct rte_mempool *mp,
			     uint32_t obj_num, uint32_t pg_shift,
			     size_t *min_chunk_size, size_t *align)
{
	ssize_t mem_size;

	/*
	 * Simply need space for one more object to be able to
	 * fulfil alignment requirements.
	 */
	mem_size = rte_mempool_op_calc_mem_size_default(mp, obj_num + 1,
							pg_shift,
							min_chunk_size, align);
	if (mem_size >= 0) {
		/*
		 * Memory area which contains objects must be physically
		 * contiguous.
		 */
		*min_chunk_size = mem_size;
	}

	return mem_size;
}

static int
octeontx_fpavf_populate(struct rte_mempool *mp, unsigned int max_objs,
			void *vaddr, rte_iova_t iova, size_t len,
			rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	size_t total_elt_sz;
	size_t off;
	uint8_t gpool;
	uintptr_t pool_bar;
	int ret;

	if (iova == RTE_BAD_IOVA)
		return -EINVAL;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	/* align object start address to a multiple of total_elt_sz */
	off = total_elt_sz - ((uintptr_t)vaddr % total_elt_sz);

	if (len < off)
		return -EINVAL;

	vaddr = (char *)vaddr + off;
	iova += off;
	len -= off;

	gpool = octeontx_fpa_bufpool_gpool(mp->pool_id);
	pool_bar = mp->pool_id & ~(uint64_t)FPA_GPOOL_MASK;

	ret = octeontx_fpavf_pool_set_range(pool_bar, len, vaddr, gpool);
	if (ret < 0)
		return ret;

	return rte_mempool_op_populate_default(mp, max_objs, vaddr, iova, len,
					       obj_cb, obj_cb_arg);
}

static struct rte_mempool_ops octeontx_fpavf_ops = {
	.name = "octeontx_fpavf",
	.alloc = octeontx_fpavf_alloc,
	.free = octeontx_fpavf_free,
	.enqueue = octeontx_fpavf_enqueue,
	.dequeue = octeontx_fpavf_dequeue,
	.get_count = octeontx_fpavf_get_count,
	.calc_mem_size = octeontx_fpavf_calc_mem_size,
	.populate = octeontx_fpavf_populate,
};

MEMPOOL_REGISTER_OPS(octeontx_fpavf_ops);
