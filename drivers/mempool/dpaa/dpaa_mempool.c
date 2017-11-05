/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
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
 *     * Neither the name of NXP nor the names of its
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

	DPAA_MEMPOOL_DEBUG("Free 0x%lx to bpid: %d", addr, bp_info->bpid);

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

	ret = rte_dpaa_portal_init((void *)0);
	if (ret) {
		DPAA_MEMPOOL_ERR("rte_dpaa_portal_init failed with ret: %d",
				 ret);
		return 0;
	}

	while (i < n) {
		dpaa_buf_free(bp_info,
			      (uint64_t)rte_mempool_virt2iova(obj_table[i]) +
			      bp_info->meta_data_size);
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

	ret = rte_dpaa_portal_init((void *)0);
	if (ret) {
		DPAA_MEMPOOL_ERR("rte_dpaa_portal_init failed with ret: %d",
				 ret);
		return -1;
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
			bufaddr = (void *)rte_dpaa_mem_ptov(bufs[i].addr);
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

struct rte_mempool_ops dpaa_mpool_ops = {
	.name = "dpaa",
	.alloc = dpaa_mbuf_create_pool,
	.free = dpaa_mbuf_free_pool,
	.enqueue = dpaa_mbuf_free_bulk,
	.dequeue = dpaa_mbuf_alloc_bulk,
	.get_count = dpaa_mbuf_get_count,
};

MEMPOOL_REGISTER_OPS(dpaa_mpool_ops);
