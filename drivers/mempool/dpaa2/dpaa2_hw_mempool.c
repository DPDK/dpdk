/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2019,2022-2025 NXP
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <eal_export.h>
#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <dev_driver.h>
#include "rte_dpaa2_mempool.h"

#include <bus_fslmc_driver.h>
#include <fslmc_logs.h>
#include <mc/fsl_dpbp.h>
#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>
#include "dpaa2_hw_mempool.h"
#include "dpaa2_hw_mempool_logs.h"

#include <dpaax_iova_table.h>

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_bpid_info)
struct dpaa2_bp_info *rte_dpaa2_bpid_info;
static struct dpaa2_bp_list *h_bp_list;

static int16_t s_dpaa2_pool_ops_idx = RTE_MEMPOOL_MAX_OPS_IDX;

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_mpool_get_ops_idx)
int rte_dpaa2_mpool_get_ops_idx(void)
{
	return s_dpaa2_pool_ops_idx;
}

static int
rte_hw_mbuf_create_pool(struct rte_mempool *mp)
{
	struct dpaa2_bp_list *bp_list;
	struct dpaa2_dpbp_dev *avail_dpbp;
	struct dpaa2_bp_info *bp_info;
	struct dpbp_attr dpbp_attr;
	uint32_t bpid;
	unsigned int lcore_id;
	struct rte_mempool_cache *cache;
	int ret;

	avail_dpbp = dpaa2_alloc_dpbp_dev();

	if (rte_dpaa2_bpid_info == NULL) {
		rte_dpaa2_bpid_info = (struct dpaa2_bp_info *)rte_malloc(NULL,
				      sizeof(struct dpaa2_bp_info) * MAX_BPID,
				      RTE_CACHE_LINE_SIZE);
		if (rte_dpaa2_bpid_info == NULL)
			return -ENOMEM;
		memset(rte_dpaa2_bpid_info, 0,
		       sizeof(struct dpaa2_bp_info) * MAX_BPID);
	}

	if (!avail_dpbp) {
		DPAA2_MEMPOOL_ERR("DPAA2 pool not available!");
		return -ENOENT;
	}

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_MEMPOOL_ERR(
				"Failed to allocate IO portal, tid: %d",
				rte_gettid());
			goto err1;
		}
	}

	ret = dpbp_enable(&avail_dpbp->dpbp, CMD_PRI_LOW, avail_dpbp->token);
	if (ret != 0) {
		DPAA2_MEMPOOL_ERR("Resource enable failure with err code: %d",
				  ret);
		goto err1;
	}

	ret = dpbp_get_attributes(&avail_dpbp->dpbp, CMD_PRI_LOW,
				  avail_dpbp->token, &dpbp_attr);
	if (ret != 0) {
		DPAA2_MEMPOOL_ERR("Resource read failure with err code: %d",
				  ret);
		goto err2;
	}

	bp_info = rte_malloc(NULL,
			     sizeof(struct dpaa2_bp_info),
			     RTE_CACHE_LINE_SIZE);
	if (!bp_info) {
		DPAA2_MEMPOOL_ERR("Unable to allocate buffer pool memory");
		ret = -ENOMEM;
		goto err2;
	}

	/* Allocate the bp_list which will be added into global_bp_list */
	bp_list = rte_malloc(NULL, sizeof(struct dpaa2_bp_list),
			     RTE_CACHE_LINE_SIZE);
	if (!bp_list) {
		DPAA2_MEMPOOL_ERR("Unable to allocate buffer pool memory");
		ret = -ENOMEM;
		goto err3;
	}

	/* Set parameters of buffer pool list */
	bp_list->buf_pool.num_bufs = mp->size;
	bp_list->buf_pool.size = mp->elt_size
			- sizeof(struct rte_mbuf) - rte_pktmbuf_priv_size(mp);
	bp_list->buf_pool.bpid = dpbp_attr.bpid;
	bp_list->buf_pool.h_bpool_mem = NULL;
	bp_list->buf_pool.dpbp_node = avail_dpbp;
	/* Identification for our offloaded pool_data structure */
	bp_list->dpaa2_ops_index = mp->ops_index;
	if (s_dpaa2_pool_ops_idx == RTE_MEMPOOL_MAX_OPS_IDX) {
		s_dpaa2_pool_ops_idx = mp->ops_index;
	} else if (s_dpaa2_pool_ops_idx != mp->ops_index) {
		DPAA2_MEMPOOL_ERR("Only single ops index only");
		ret = -EINVAL;
		goto err4;
	}

	bp_list->next = h_bp_list;
	bp_list->mp = mp;

	bpid = dpbp_attr.bpid;

	rte_dpaa2_bpid_info[bpid].meta_data_size = sizeof(struct rte_mbuf)
				+ rte_pktmbuf_priv_size(mp);
	rte_dpaa2_bpid_info[bpid].bp_list = bp_list;
	rte_dpaa2_bpid_info[bpid].bpid = bpid;

	rte_memcpy(bp_info, (void *)&rte_dpaa2_bpid_info[bpid],
		   sizeof(struct dpaa2_bp_info));
	mp->pool_data = (void *)bp_info;

	DPAA2_MEMPOOL_DEBUG("BP List created for bpid =%d", dpbp_attr.bpid);

	h_bp_list = bp_list;
	/* Update per core mempool cache threshold to optimal value which is
	 * number of buffers that can be released to HW buffer pool in
	 * a single API call.
	 */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		cache = &mp->local_cache[lcore_id];
		DPAA2_MEMPOOL_DEBUG("lCore %d: cache->flushthresh %d -> %d",
			lcore_id, cache->flushthresh,
			(uint32_t)(cache->size + DPAA2_MBUF_MAX_ACQ_REL));
		if (cache->flushthresh)
			cache->flushthresh = cache->size + DPAA2_MBUF_MAX_ACQ_REL;
	}

	return 0;
err4:
	rte_free(bp_list);
err3:
	rte_free(bp_info);
err2:
	dpbp_disable(&avail_dpbp->dpbp, CMD_PRI_LOW, avail_dpbp->token);
err1:
	dpaa2_free_dpbp_dev(avail_dpbp);

	return ret;
}

static void
rte_hw_mbuf_free_pool(struct rte_mempool *mp)
{
	struct dpaa2_bp_info *bpinfo;
	struct dpaa2_bp_list *bp;
	struct dpaa2_dpbp_dev *dpbp_node;

	if (!mp->pool_data) {
		DPAA2_MEMPOOL_ERR("Not a valid dpaa2 buffer pool");
		return;
	}

	bpinfo = (struct dpaa2_bp_info *)mp->pool_data;
	bp = bpinfo->bp_list;
	dpbp_node = bp->buf_pool.dpbp_node;

	dpbp_disable(&(dpbp_node->dpbp), CMD_PRI_LOW, dpbp_node->token);

	if (h_bp_list == bp) {
		h_bp_list = h_bp_list->next;
	} else { /* if it is not the first node */
		struct dpaa2_bp_list *prev = h_bp_list, *temp;
		temp = h_bp_list->next;
		while (temp) {
			if (temp == bp) {
				prev->next = temp->next;
				rte_free(bp);
				break;
			}
			prev = temp;
			temp = temp->next;
		}
	}

	rte_free(mp->pool_data);
	dpaa2_free_dpbp_dev(dpbp_node);
}

static inline int
dpaa2_bman_multi_release(uint64_t *bufs, int num,
	struct qbman_swp *swp,
	const struct qbman_release_desc *releasedesc)
{
	int retry_count = 0, ret;

release_again:
	ret = qbman_swp_release(swp, releasedesc, bufs, num);
	if (unlikely(ret == -EBUSY)) {
		retry_count++;
		if (retry_count <= DPAA2_MAX_TX_RETRY_COUNT)
			goto release_again;

		DPAA2_MEMPOOL_ERR("bman release retry exceeded, low fbpr?");
		return ret;
	}
	if (unlikely(ret)) {
		DPAA2_MEMPOOL_ERR("bman release failed(err=%d)", ret);
		return ret;
	}

	return 0;
}

static int
dpaa2_mbuf_release(void * const *obj_table, uint32_t bpid,
	uint32_t meta_data_size, int count)
{
	struct qbman_release_desc releasedesc;
	struct qbman_swp *swp;
	int ret;
	int i, n;
	uint64_t bufs[DPAA2_MBUF_MAX_ACQ_REL];

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_MEMPOOL_ERR("affine portal err: %d, tid: %d",
				ret, rte_gettid());
			return -EIO;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	/* Create a release descriptor required for releasing
	 * buffers into QBMAN
	 */
	qbman_release_desc_clear(&releasedesc);
	qbman_release_desc_set_bpid(&releasedesc, bpid);

	n = count % DPAA2_MBUF_MAX_ACQ_REL;
	if (unlikely(!n))
		goto aligned;

	/* convert mbuf to buffers for the remainder */
	if (likely(rte_eal_iova_mode() == RTE_IOVA_VA)) {
		for (i = 0; i < n ; i++) {
			bufs[i] = DPAA2_VAMODE_VADDR_TO_IOVA(obj_table[i]) +
				meta_data_size;
		}
	} else {
		for (i = 0; i < n ; i++) {
			bufs[i] = DPAA2_PAMODE_VADDR_TO_IOVA(obj_table[i]) +
				meta_data_size;
		}
	}

	/* feed them to bman */
	ret = dpaa2_bman_multi_release(bufs, n, swp, &releasedesc);
	if (unlikely(ret))
		return 0;

aligned:
	/* if there are more buffers to free */
	if (unlikely(rte_eal_iova_mode() != RTE_IOVA_VA))
		goto iova_pa_release;

	while (n < count) {
		/* convert mbuf to buffers */
		for (i = 0; i < DPAA2_MBUF_MAX_ACQ_REL; i++) {
			bufs[i] = DPAA2_VAMODE_VADDR_TO_IOVA(obj_table[n + i]) +
				meta_data_size;
		}

		ret = dpaa2_bman_multi_release(bufs,
				DPAA2_MBUF_MAX_ACQ_REL, swp, &releasedesc);
		if (unlikely(ret))
			return n;

		n += DPAA2_MBUF_MAX_ACQ_REL;
	}

	return count;

iova_pa_release:
	while (n < count) {
		/* convert mbuf to buffers */
		for (i = 0; i < DPAA2_MBUF_MAX_ACQ_REL; i++) {
			bufs[i] = DPAA2_PAMODE_VADDR_TO_IOVA(obj_table[n + i]) +
				meta_data_size;
		}

		ret = dpaa2_bman_multi_release(bufs,
				DPAA2_MBUF_MAX_ACQ_REL, swp, &releasedesc);
		if (unlikely(ret))
			return n;

		n += DPAA2_MBUF_MAX_ACQ_REL;
	}

	return count;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_bpid_info_init)
int rte_dpaa2_bpid_info_init(struct rte_mempool *mp)
{
	struct dpaa2_bp_info *bp_info = mempool_to_bpinfo(mp);
	uint32_t bpid = bp_info->bpid;

	if (!rte_dpaa2_bpid_info) {
		rte_dpaa2_bpid_info = (struct dpaa2_bp_info *)rte_malloc(NULL,
				      sizeof(struct dpaa2_bp_info) * MAX_BPID,
				      RTE_CACHE_LINE_SIZE);
		if (rte_dpaa2_bpid_info == NULL)
			return -ENOMEM;
		memset(rte_dpaa2_bpid_info, 0,
		       sizeof(struct dpaa2_bp_info) * MAX_BPID);
	}

	rte_dpaa2_bpid_info[bpid].meta_data_size = sizeof(struct rte_mbuf)
				+ rte_pktmbuf_priv_size(mp);
	rte_dpaa2_bpid_info[bpid].bp_list = bp_info->bp_list;
	rte_dpaa2_bpid_info[bpid].bpid = bpid;

	return 0;
}

RTE_EXPORT_SYMBOL(rte_dpaa2_mbuf_pool_bpid)
uint16_t
rte_dpaa2_mbuf_pool_bpid(struct rte_mempool *mp)
{
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(mp);
	if (!(bp_info->bp_list)) {
		DPAA2_MEMPOOL_ERR("DPAA2 buffer pool not configured");
		return -ENOMEM;
	}

	return bp_info->bpid;
}

RTE_EXPORT_SYMBOL(rte_dpaa2_mbuf_from_buf_addr)
struct rte_mbuf *
rte_dpaa2_mbuf_from_buf_addr(struct rte_mempool *mp, void *buf_addr)
{
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(mp);
	if (!(bp_info->bp_list)) {
		DPAA2_MEMPOOL_ERR("DPAA2 buffer pool not configured");
		return NULL;
	}

	return (struct rte_mbuf *)((uint8_t *)buf_addr -
			bp_info->meta_data_size);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_mbuf_alloc_bulk)
int
rte_dpaa2_mbuf_alloc_bulk(struct rte_mempool *pool,
			  void **obj_table, unsigned int count)
{
#ifdef RTE_LIBRTE_DPAA2_DEBUG_DRIVER
	static int alloc;
#endif
	struct qbman_swp *swp;
	uint16_t bpid;
	uint64_t bufs[DPAA2_MBUF_MAX_ACQ_REL];
	int i, ret;
	unsigned int n = 0;
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(pool);

	if (!(bp_info->bp_list)) {
		DPAA2_MEMPOOL_ERR("DPAA2 buffer pool not configured");
		return -ENOENT;
	}

	bpid = bp_info->bpid;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_MEMPOOL_ERR("affine portal err: %d, tid: %d",
				ret, rte_gettid());
			return ret;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	if (unlikely(rte_eal_iova_mode() != RTE_IOVA_VA))
		goto iova_pa_acquire;

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		ret = qbman_swp_acquire(swp, bpid, bufs,
			(count - n) > DPAA2_MBUF_MAX_ACQ_REL ?
			DPAA2_MBUF_MAX_ACQ_REL : (count - n));
		if (unlikely(ret <= 0))
			goto acquire_failed;

		/* assigning mbuf from the acquired objects */
		for (i = 0; i < ret; i++) {
			DPAA2_VAMODE_MODIFY_IOVA_TO_VADDR(bufs[i],
				size_t);
			obj_table[n] = (void *)(uintptr_t)(bufs[i] -
				bp_info->meta_data_size);
			n++;
		}
	}
	goto acquire_success;

iova_pa_acquire:

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		ret = qbman_swp_acquire(swp, bpid, bufs,
			(count - n) > DPAA2_MBUF_MAX_ACQ_REL ?
			DPAA2_MBUF_MAX_ACQ_REL : (count - n));
		if (unlikely(ret <= 0))
			goto acquire_failed;

		/* assigning mbuf from the acquired objects */
		for (i = 0; i < ret; i++) {
			DPAA2_PAMODE_MODIFY_IOVA_TO_VADDR(bufs[i],
				size_t);
			obj_table[n] = (void *)(uintptr_t)(bufs[i] -
				bp_info->meta_data_size);
			n++;
		}
	}

acquire_success:
#ifdef RTE_LIBRTE_DPAA2_DEBUG_DRIVER
	alloc += n;
	DPAA2_MEMPOOL_DP_DEBUG("Total = %d , req = %d done = %d",
		alloc, count, n);
#endif
	return 0;

acquire_failed:
	DPAA2_MEMPOOL_DP_DEBUG("Buffer acquire err: %d", ret);
	/* The API expect the exact number of requested bufs */
	/* Releasing all buffers allocated */
	ret = dpaa2_mbuf_release(obj_table, bpid,
			bp_info->meta_data_size, n);
	if (ret != (int)n) {
		DPAA2_MEMPOOL_ERR("%s: expect to free %d!= %d",
			__func__, n, ret);
	}
	return -ENOBUFS;
}

static int
rte_hw_mbuf_free_bulk(struct rte_mempool *pool,
		  void * const *obj_table, unsigned int n)
{
	struct dpaa2_bp_info *bp_info;
	int ret;

	bp_info = mempool_to_bpinfo(pool);
	if (!(bp_info->bp_list)) {
		DPAA2_MEMPOOL_ERR("DPAA2 buffer pool not configured");
		return -ENOENT;
	}
	ret = dpaa2_mbuf_release(obj_table, bp_info->bpid,
			bp_info->meta_data_size, n);
	if (unlikely(ret != (int)n)) {
		DPAA2_MEMPOOL_ERR("%s: expect to free %d!= %d",
			__func__, n, ret);

		return -EIO;
	}

	return 0;
}

static unsigned int
rte_hw_mbuf_get_count(const struct rte_mempool *mp)
{
	int ret;
	unsigned int num_of_bufs = 0;
	struct dpaa2_bp_info *bp_info;
	struct dpaa2_dpbp_dev *dpbp_node;
	struct fsl_mc_io mc_io;

	if (!mp || !mp->pool_data) {
		DPAA2_MEMPOOL_ERR("Invalid mempool provided");
		return 0;
	}

	bp_info = (struct dpaa2_bp_info *)mp->pool_data;
	dpbp_node = bp_info->bp_list->buf_pool.dpbp_node;

	/* In case as secondary process access stats, MCP portal in priv-hw may
	 * have primary process address. Need the secondary process based MCP
	 * portal address for this object.
	 */
	mc_io.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	ret = dpbp_get_num_free_bufs(&mc_io, CMD_PRI_LOW,
				     dpbp_node->token, &num_of_bufs);
	if (ret) {
		DPAA2_MEMPOOL_ERR("Unable to obtain free buf count (err=%d)",
				  ret);
		return 0;
	}

	DPAA2_MEMPOOL_DP_DEBUG("Free bufs = %u\n", num_of_bufs);

	return num_of_bufs;
}

static int
dpaa2_populate(struct rte_mempool *mp, unsigned int max_objs,
	      void *vaddr, rte_iova_t paddr, size_t len,
	      rte_mempool_populate_obj_cb_t *obj_cb, void *obj_cb_arg)
{
	struct rte_memseg_list *msl;
	int ret;
	/* The memsegment list exists incase the memory is not external.
	 * So, DMA-Map is required only when memory is provided by user,
	 * i.e. External.
	 */
	msl = rte_mem_virt2memseg_list(vaddr);

	if (!msl) {
		DPAA2_MEMPOOL_DEBUG("Memsegment is External.");
		ret = rte_fslmc_vfio_mem_dmamap((size_t)vaddr, paddr, len);
		if (ret)
			return ret;
	}

	return rte_mempool_op_populate_helper(mp, 0, max_objs, vaddr, paddr,
					       len, obj_cb, obj_cb_arg);
}

static const struct rte_mempool_ops dpaa2_mpool_ops = {
	.name = DPAA2_MEMPOOL_OPS_NAME,
	.alloc = rte_hw_mbuf_create_pool,
	.free = rte_hw_mbuf_free_pool,
	.enqueue = rte_hw_mbuf_free_bulk,
	.dequeue = rte_dpaa2_mbuf_alloc_bulk,
	.get_count = rte_hw_mbuf_get_count,
	.populate = dpaa2_populate,
};

RTE_MEMPOOL_REGISTER_OPS(dpaa2_mpool_ops);

RTE_LOG_REGISTER_DEFAULT(dpaa2_logtype_mempool, NOTICE);
