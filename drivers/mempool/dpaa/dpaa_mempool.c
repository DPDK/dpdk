/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

/* System headers */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_ring.h>

#include <dpaa_mempool.h>
#include <dpaax_iova_table.h>

/* List of all the memseg information locally maintained in dpaa driver. This
 * is to optimize the PA_to_VA searches until a better mechanism (algo) is
 * available.
 */
struct dpaa_memseg_list rte_dpaa_memsegs
	= TAILQ_HEAD_INITIALIZER(rte_dpaa_memsegs);

struct dpaa_bp_info rte_dpaa_bpid_info[DPAA_MAX_BPOOLS];

static int
dpaa_mbuf_create_pool(struct rte_mempool *mp)
{
	struct bman_pool *bp;
	struct bm_buffer bufs[8];
	struct dpaa_bp_info *bp_info;
	uint8_t bpid;
	int num_bufs = 0, ret = 0;
	struct bman_pool_params params = {
		.flags = BMAN_POOL_FLAG_DYNAMIC_BPID
	};

	MEMPOOL_INIT_FUNC_TRACE();

	bp = bman_new_pool(&params);
	if (!bp) {
		DPAA_MEMPOOL_ERR("bman_new_pool() failed");
		return -ENODEV;
	}
	bpid = bman_get_params(bp)->bpid;

	/* Drain the pool of anything already in it. */
	do {
		/* Acquire is all-or-nothing, so we drain in 8s,
		 * then in 1s for the remainder.
		 */
		if (ret != 1)
			ret = bman_acquire(bp, bufs, 8, 0);
		if (ret < 8)
			ret = bman_acquire(bp, bufs, 1, 0);
		if (ret > 0)
			num_bufs += ret;
	} while (ret > 0);
	if (num_bufs)
		DPAA_MEMPOOL_WARN("drained %u bufs from BPID %d",
				  num_bufs, bpid);

	rte_dpaa_bpid_info[bpid].mp = mp;
	rte_dpaa_bpid_info[bpid].bpid = bpid;
	rte_dpaa_bpid_info[bpid].size = mp->elt_size;
	rte_dpaa_bpid_info[bpid].bp = bp;
	rte_dpaa_bpid_info[bpid].meta_data_size =
		sizeof(struct rte_mbuf) + rte_pktmbuf_priv_size(mp);
	rte_dpaa_bpid_info[bpid].dpaa_ops_index = mp->ops_index;
	rte_dpaa_bpid_info[bpid].ptov_off = 0;
	rte_dpaa_bpid_info[bpid].flags = 0;

	bp_info = rte_malloc(NULL,
			     sizeof(struct dpaa_bp_info),
			     RTE_CACHE_LINE_SIZE);
	if (!bp_info) {
		DPAA_MEMPOOL_WARN("Memory allocation failed for bp_info");
		bman_free_pool(bp);
		return -ENOMEM;
	}

	rte_memcpy(bp_info, (void *)&rte_dpaa_bpid_info[bpid],
		   sizeof(struct dpaa_bp_info));
	mp->pool_data = (void *)bp_info;

	DPAA_MEMPOOL_INFO("BMAN pool created for bpid =%d", bpid);
	return 0;
}

static void
dpaa_mbuf_free_pool(struct rte_mempool *mp)
{
	struct dpaa_bp_info *bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);

	MEMPOOL_INIT_FUNC_TRACE();

	if (bp_info) {
		bman_free_pool(bp_info->bp);
		DPAA_MEMPOOL_INFO("BMAN pool freed for bpid =%d",
				  bp_info->bpid);
		rte_free(mp->pool_data);
		mp->pool_data = NULL;
	}
}

static void
dpaa_buf_free(struct dpaa_bp_info *bp_info, uint64_t addr)
{
	struct bm_buffer buf;
	int ret;

	DPAA_MEMPOOL_DPDEBUG("Free 0x%" PRIx64 " to bpid: %d",
			   addr, bp_info->bpid);

	bm_buffer_set64(&buf, addr);
retry:
	ret = bman_release(bp_info->bp, &buf, 1, 0);
	if (ret) {
		DPAA_MEMPOOL_DEBUG("BMAN busy. Retrying...");
		cpu_spin(CPU_SPIN_BACKOFF_CYCLES);
		goto retry;
	}
}

static int
dpaa_mbuf_free_bulk(struct rte_mempool *pool,
		    void *const *obj_table,
		    unsigned int n)
{
	struct dpaa_bp_info *bp_info = DPAA_MEMPOOL_TO_POOL_INFO(pool);
	int ret;
	unsigned int i = 0;

	DPAA_MEMPOOL_DPDEBUG("Request to free %d buffers in bpid = %d",
			     n, bp_info->bpid);

	if (unlikely(!RTE_PER_LCORE(dpaa_io))) {
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_MEMPOOL_ERR("rte_dpaa_portal_init failed with ret: %d",
					 ret);
			return 0;
		}
	}

	while (i < n) {
		uint64_t phy = rte_mempool_virt2iova(obj_table[i]);

		if (unlikely(!bp_info->ptov_off)) {
			/* buffers are from single mem segment */
			if (bp_info->flags & DPAA_MPOOL_SINGLE_SEGMENT) {
				bp_info->ptov_off = (size_t)obj_table[i] - phy;
				rte_dpaa_bpid_info[bp_info->bpid].ptov_off
						= bp_info->ptov_off;
			}
		}

		dpaa_buf_free(bp_info,
			      (uint64_t)phy + bp_info->meta_data_size);
		i = i + 1;
	}

	DPAA_MEMPOOL_DPDEBUG("freed %d buffers in bpid =%d",
			     n, bp_info->bpid);

	return 0;
}

static int
dpaa_mbuf_alloc_bulk(struct rte_mempool *pool,
		     void **obj_table,
		     unsigned int count)
{
	struct rte_mbuf **m = (struct rte_mbuf **)obj_table;
	struct bm_buffer bufs[DPAA_MBUF_MAX_ACQ_REL];
	struct dpaa_bp_info *bp_info;
	void *bufaddr;
	int i, ret;
	unsigned int n = 0;

	bp_info = DPAA_MEMPOOL_TO_POOL_INFO(pool);

	DPAA_MEMPOOL_DPDEBUG("Request to alloc %d buffers in bpid = %d",
			     count, bp_info->bpid);

	if (unlikely(count >= (RTE_MEMPOOL_CACHE_MAX_SIZE * 2))) {
		DPAA_MEMPOOL_ERR("Unable to allocate requested (%u) buffers",
				 count);
		return -1;
	}

	if (unlikely(!RTE_PER_LCORE(dpaa_io))) {
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_MEMPOOL_ERR("rte_dpaa_portal_init failed with ret: %d",
					 ret);
			return -1;
		}
	}

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		if ((count - n) > DPAA_MBUF_MAX_ACQ_REL) {
			ret = bman_acquire(bp_info->bp, bufs,
					   DPAA_MBUF_MAX_ACQ_REL, 0);
		} else {
			ret = bman_acquire(bp_info->bp, bufs, count - n, 0);
		}
		/* In case of less than requested number of buffers available
		 * in pool, qbman_swp_acquire returns 0
		 */
		if (ret <= 0) {
			DPAA_MEMPOOL_DPDEBUG("Buffer acquire failed (%d)",
					     ret);
			/* The API expect the exact number of requested
			 * buffers. Releasing all buffers allocated
			 */
			dpaa_mbuf_free_bulk(pool, obj_table, n);
			return -ENOBUFS;
		}
		/* assigning mbuf from the acquired objects */
		for (i = 0; (i < ret) && bufs[i].addr; i++) {
			/* TODO-errata - objerved that bufs may be null
			 * i.e. first buffer is valid, remaining 6 buffers
			 * may be null.
			 */
			bufaddr = DPAA_MEMPOOL_PTOV(bp_info, bufs[i].addr);
			m[n] = (struct rte_mbuf *)((char *)bufaddr
						- bp_info->meta_data_size);
			DPAA_MEMPOOL_DPDEBUG("Paddr (%p), FD (%p) from BMAN",
					     (void *)bufaddr, (void *)m[n]);
			n++;
		}
	}

	DPAA_MEMPOOL_DPDEBUG("Allocated %d buffers from bpid=%d",
			     n, bp_info->bpid);
	return 0;
}

static unsigned int
dpaa_mbuf_get_count(const struct rte_mempool *mp)
{
	struct dpaa_bp_info *bp_info;

	MEMPOOL_INIT_FUNC_TRACE();

	if (!mp || !mp->pool_data) {
		DPAA_MEMPOOL_ERR("Invalid mempool provided\n");
		return 0;
	}

	bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);

	return bman_query_free_buffers(bp_info->bp);
}

static int
dpaa_populate(struct rte_mempool *mp, unsigned int max_objs,
	      void *vaddr, rte_iova_t paddr, size_t len,
	      rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	struct dpaa_bp_info *bp_info;
	unsigned int total_elt_sz;

	MEMPOOL_INIT_FUNC_TRACE();

	if (!mp || !mp->pool_data) {
		DPAA_MEMPOOL_ERR("Invalid mempool provided\n");
		return 0;
	}

	/* Update the PA-VA Table */
	dpaax_iova_table_update(paddr, vaddr, len);

	bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);
	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	DPAA_MEMPOOL_DEBUG("Req size %" PRIx64 " vs Available %u\n",
			   (uint64_t)len, total_elt_sz * mp->size);

	/* Detect pool area has sufficient space for elements in this memzone */
	if (len >= total_elt_sz * mp->size)
		bp_info->flags |= DPAA_MPOOL_SINGLE_SEGMENT;
	struct dpaa_memseg *ms;

	/* For each memory chunk pinned to the Mempool, a linked list of the
	 * contained memsegs is created for searching when PA to VA
	 * conversion is required.
	 */
	ms = rte_zmalloc(NULL, sizeof(struct dpaa_memseg), 0);
	if (!ms) {
		DPAA_MEMPOOL_ERR("Unable to allocate internal memory.");
		DPAA_MEMPOOL_WARN("Fast Physical to Virtual Addr translation would not be available.");
		/* If the element is not added, it would only lead to failure
		 * in searching for the element and the logic would Fallback
		 * to traditional DPDK memseg traversal code. So, this is not
		 * a blocking error - but, error would be printed on screen.
		 */
		return 0;
	}

	ms->vaddr = vaddr;
	ms->iova = paddr;
	ms->len = len;
	/* Head insertions are generally faster than tail insertions as the
	 * buffers pinned are picked from rear end.
	 */
	TAILQ_INSERT_HEAD(&rte_dpaa_memsegs, ms, next);

	return rte_mempool_op_populate_default(mp, max_objs, vaddr, paddr, len,
					       obj_cb, obj_cb_arg);
}

static const struct rte_mempool_ops dpaa_mpool_ops = {
	.name = DPAA_MEMPOOL_OPS_NAME,
	.alloc = dpaa_mbuf_create_pool,
	.free = dpaa_mbuf_free_pool,
	.enqueue = dpaa_mbuf_free_bulk,
	.dequeue = dpaa_mbuf_alloc_bulk,
	.get_count = dpaa_mbuf_get_count,
	.populate = dpaa_populate,
};

MEMPOOL_REGISTER_OPS(dpaa_mpool_ops);
