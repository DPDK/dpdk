/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017,2019,2023-2025 NXP
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

#include <eal_export.h>
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

#define FMAN_ERRATA_BOUNDARY ((uint64_t)4096)
#define FMAN_ERRATA_BOUNDARY_MASK (~(FMAN_ERRATA_BOUNDARY - 1))

/* List of all the memseg information locally maintained in dpaa driver. This
 * is to optimize the PA_to_VA searches until a better mechanism (algo) is
 * available.
 */
RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa_memsegs)
struct dpaa_memseg_list rte_dpaa_memsegs
	= TAILQ_HEAD_INITIALIZER(rte_dpaa_memsegs);

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa_bpid_info)
struct dpaa_bp_info *rte_dpaa_bpid_info;

RTE_LOG_REGISTER_DEFAULT(dpaa_logtype_mempool, NOTICE);
#define RTE_LOGTYPE_DPAA_MEMPOOL dpaa_logtype_mempool

static int
dpaa_mbuf_create_pool(struct rte_mempool *mp)
{
	struct bman_pool *bp;
	struct bm_buffer bufs[8];
	struct dpaa_bp_info *bp_info;
	uint8_t bpid;
	int num_bufs = 0, ret = 0;
	uint16_t elem_max_size;
	struct bman_pool_params params = {
		.flags = BMAN_POOL_FLAG_DYNAMIC_BPID
	};
	unsigned int lcore_id;
	struct rte_mempool_cache *cache;

	MEMPOOL_INIT_FUNC_TRACE();

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_MEMPOOL_ERR(
				"rte_dpaa_portal_init failed with ret: %d",
				 ret);
			return -1;
		}
	}
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

	if (rte_dpaa_bpid_info == NULL) {
		rte_dpaa_bpid_info = (struct dpaa_bp_info *)rte_zmalloc(NULL,
				sizeof(struct dpaa_bp_info) * DPAA_MAX_BPOOLS,
				RTE_CACHE_LINE_SIZE);
		if (rte_dpaa_bpid_info == NULL) {
			bman_free_pool(bp);
			return -ENOMEM;
		}
	}

	elem_max_size = rte_pktmbuf_data_room_size(mp);

	rte_dpaa_bpid_info[bpid].mp = mp;
	rte_dpaa_bpid_info[bpid].bpid = bpid;
	rte_dpaa_bpid_info[bpid].size = elem_max_size;
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
	/* Update per core mempool cache threshold to optimal value which is
	 * number of buffers that can be released to HW buffer pool in
	 * a single API call.
	 */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		cache = &mp->local_cache[lcore_id];
		DPAA_MEMPOOL_DEBUG("lCore %d: cache->flushthresh %d -> %d",
			lcore_id, cache->flushthresh,
			(uint32_t)(cache->size + DPAA_MBUF_MAX_ACQ_REL));
		if (cache->flushthresh)
			cache->flushthresh = cache->size + DPAA_MBUF_MAX_ACQ_REL;
	}

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
		bp_info->bp = NULL;
		mp->pool_data = NULL;
	}
}

static int
dpaa_mbuf_free_bulk(struct rte_mempool *pool,
		    void *const *obj_table,
		    unsigned int count)
{
	struct dpaa_bp_info *bp_info = DPAA_MEMPOOL_TO_POOL_INFO(pool);
	int ret;
	uint32_t n = 0, i, left;
	uint64_t phys[DPAA_MBUF_MAX_ACQ_REL];

	DPAA_MEMPOOL_DPDEBUG("Request to free %d buffers in bpid = %d",
			     count, bp_info->bpid);

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_MEMPOOL_ERR("rte_dpaa_portal_init failed with ret: %d",
					 ret);
			return ret;
		}
	}

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		if ((count - n) > DPAA_MBUF_MAX_ACQ_REL)
			left = DPAA_MBUF_MAX_ACQ_REL;
		else
			left = count - n;

		for (i = 0; i < left; i++) {
			phys[i] = rte_mempool_virt2iova(obj_table[n]);
			phys[i] += bp_info->meta_data_size;
			n++;
		}
release_again:
		ret = bman_release_fast(bp_info->bp, phys, left);
		if (unlikely(ret))
			goto release_again;
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
	uint64_t bufs[DPAA_MBUF_MAX_ACQ_REL];
	struct dpaa_bp_info *bp_info;
	uint8_t *bufaddr;
	int i, ret;
	unsigned int n = 0;

	bp_info = DPAA_MEMPOOL_TO_POOL_INFO(pool);

	DPAA_MEMPOOL_DPDEBUG("Request to alloc %d buffers in bpid = %d",
			     count, bp_info->bpid);

	if (unlikely(count >= (RTE_MEMPOOL_CACHE_MAX_SIZE * 2))) {
		DPAA_MEMPOOL_ERR("Unable to allocate requested (%u) buffers",
				 count);
		return -EINVAL;
	}

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		ret = rte_dpaa_portal_init((void *)0);
		if (ret) {
			DPAA_MEMPOOL_ERR("rte_dpaa_portal_init failed with ret: %d",
					 ret);
			return ret;
		}
	}

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		if ((count - n) > DPAA_MBUF_MAX_ACQ_REL) {
			ret = bman_acquire_fast(bp_info->bp, bufs,
				DPAA_MBUF_MAX_ACQ_REL);
		} else {
			ret = bman_acquire_fast(bp_info->bp, bufs,
				count - n);
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
		for (i = 0; (i < ret) && bufs[i]; i++) {
			/* TODO-errata - observed that bufs may be null
			 * i.e. first buffer is valid, remaining 6 buffers
			 * may be null.
			 */
			bufaddr = DPAA_MEMPOOL_PTOV(bp_info, bufs[i]);
			m[n] = (void *)(bufaddr - bp_info->meta_data_size);
			DPAA_MEMPOOL_DPDEBUG("Vaddr(%p), mbuf(%p) from BMAN",
				bufaddr, m[n]);
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
		DPAA_MEMPOOL_ERR("Invalid mempool provided");
		return 0;
	}

	bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);

	return bman_query_free_buffers(bp_info->bp);
}

static int
dpaa_check_obj_bounds(char *obj, size_t pg_sz, size_t elt_sz)
{
	if (!pg_sz || elt_sz > pg_sz)
		return true;

	if (RTE_PTR_ALIGN(obj, pg_sz) !=
		RTE_PTR_ALIGN(obj + elt_sz - 1, pg_sz))
		return false;
	return true;
}

static void
dpaa_adjust_obj_bounds(char *va, size_t *offset,
	size_t pg_sz, size_t total, uint32_t flags)
{
	size_t off = *offset;

	if (dpaa_check_obj_bounds(va + off, pg_sz, total) == false) {
		off += RTE_PTR_ALIGN_CEIL(va + off, pg_sz) - (va + off);
		if (flags & RTE_MEMPOOL_POPULATE_F_ALIGN_OBJ)
			off += total - ((((size_t)va + off - 1) % total) + 1);
	}

	*offset = off;
}

static int
dpaa_mbuf_ls1043a_errata_obj_adjust(uint8_t **pobj,
	uint32_t header_size, size_t *poff, size_t data_room)
{
	uint8_t *obj = *pobj;
	size_t off = *poff, buf_addr, end;

	if (RTE_PKTMBUF_HEADROOM % FMAN_ERRATA_BUF_START_ALIGN) {
		DPAA_MEMPOOL_ERR("RTE_PKTMBUF_HEADROOM(%d) NOT aligned to %d",
			RTE_PKTMBUF_HEADROOM,
			FMAN_ERRATA_BUF_START_ALIGN);
		return -1;
	}
	if (header_size % FMAN_ERRATA_BUF_START_ALIGN) {
		DPAA_MEMPOOL_ERR("Header size(%d) NOT aligned to %d",
			header_size,
			FMAN_ERRATA_BUF_START_ALIGN);
		return -1;
	}

	/** All FMAN DMA start addresses (for example, BMAN buffer
	 * address, FD[address] + FD[offset]) are 16B aligned.
	 */
	buf_addr = (size_t)obj + header_size;
	while (!rte_is_aligned((void *)buf_addr,
		FMAN_ERRATA_BUF_START_ALIGN)) {
		off++;
		obj++;
		buf_addr = (size_t)obj + header_size;
	}

	/** Frame buffers must not span a 4KB address boundary,
	 * unless the frame start address is 256 byte aligned.
	 */
	end = buf_addr + data_room;
	if (((buf_addr + RTE_PKTMBUF_HEADROOM) &
		FMAN_ERRATA_BOUNDARY_MASK) ==
		(end & FMAN_ERRATA_BOUNDARY_MASK))
		goto quit;

	while (!rte_is_aligned((void *)(buf_addr + RTE_PKTMBUF_HEADROOM),
		FMAN_ERRATA_4K_SPAN_ADDR_ALIGN)) {
		off++;
		obj++;
		buf_addr = (size_t)obj + header_size;
	}
quit:
	*pobj = obj;
	*poff = off;

	return 0;
}

static int
dpaa_mbuf_op_pop_helper(struct rte_mempool *mp, uint32_t flags,
	uint32_t max_objs, void *vaddr, rte_iova_t iova,
	size_t len, struct dpaa_bp_info *bp_info,
	rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	char *va = vaddr;
	size_t total_elt_sz, pg_sz, off;
	uint32_t i;
	void *obj;
	int ret;
	uint16_t data_room = rte_pktmbuf_data_room_size(mp);

	ret = rte_mempool_get_page_size(mp, &pg_sz);
	if (ret < 0)
		return ret;

	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	if (flags & RTE_MEMPOOL_POPULATE_F_ALIGN_OBJ)
		off = total_elt_sz - (((uintptr_t)(va - 1) % total_elt_sz) + 1);
	else
		off = 0;
	for (i = 0; i < max_objs; i++) {
		/* avoid objects to cross page boundaries */
		dpaa_adjust_obj_bounds(va, &off, pg_sz, total_elt_sz, flags);
		if (off + total_elt_sz > len)
			break;

		off += mp->header_size;
		obj = va + off;
		if (dpaa_soc_ver() == SVR_LS1043A_FAMILY) {
			dpaa_mbuf_ls1043a_errata_obj_adjust((uint8_t **)&obj,
				bp_info->meta_data_size, &off, data_room);
		}
		obj_cb(mp, obj_cb_arg, obj,
			(iova == RTE_BAD_IOVA) ? RTE_BAD_IOVA : (iova + off));
		rte_mempool_ops_enqueue_bulk(mp, &obj, 1);
		off += mp->elt_size + mp->trailer_size;
	}

	return i;
}

static int
dpaa_populate(struct rte_mempool *mp, unsigned int max_objs,
	      void *vaddr, rte_iova_t paddr, size_t len,
	      rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	struct dpaa_bp_info *bp_info;
	unsigned int total_elt_sz;
	struct dpaa_memseg *ms;

	if (!mp || !mp->pool_data) {
		DPAA_MEMPOOL_ERR("Invalid mempool provided");
		if (dpaa_soc_ver() == SVR_LS1043A_FAMILY) {
			/** populate must be successful for LS1043A*/
			return -EINVAL;
		}
		return 0;
	}

	/* Update the PA-VA Table */
	dpaax_iova_table_update(paddr, vaddr, len);

	bp_info = DPAA_MEMPOOL_TO_POOL_INFO(mp);
	total_elt_sz = mp->header_size + mp->elt_size + mp->trailer_size;

	DPAA_MEMPOOL_DPDEBUG("Req size %" PRIx64 " vs Available %u\n",
			   (uint64_t)len, total_elt_sz * mp->size);

	/* Detect pool area has sufficient space for elements in this memzone */
	if (len >= total_elt_sz * mp->size)
		bp_info->flags |= DPAA_MPOOL_SINGLE_SEGMENT;

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

	return dpaa_mbuf_op_pop_helper(mp, 0, max_objs, vaddr, paddr,
			len, bp_info, obj_cb, obj_cb_arg);
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

RTE_MEMPOOL_REGISTER_OPS(dpaa_mpool_ops);
