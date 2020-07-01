/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2019 NXP
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include "rte_dpaa2_mempool.h"

#include "fslmc_vfio.h"
#include <fslmc_logs.h>
#include <mc/fsl_dpbp.h>
#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>
#include "dpaa2_hw_mempool.h"
#include "dpaa2_hw_mempool_logs.h"

#include <dpaax_iova_table.h>

struct dpaa2_bp_info *rte_dpaa2_bpid_info;
static struct dpaa2_bp_list *h_bp_list;

static int
rte_hw_mbuf_create_pool(struct rte_mempool *mp)
{
	struct dpaa2_bp_list *bp_list;
	struct dpaa2_dpbp_dev *avail_dpbp;
	struct dpaa2_bp_info *bp_info;
	struct dpbp_attr dpbp_attr;
	uint32_t bpid;
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
				"Failed to allocate IO portal, tid: %d\n",
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
	return 0;
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

static void
rte_dpaa2_mbuf_release(struct rte_mempool *pool __rte_unused,
			void * const *obj_table,
			uint32_t bpid,
			uint32_t meta_data_size,
			int count)
{
	struct qbman_release_desc releasedesc;
	struct qbman_swp *swp;
	int ret;
	int i, n, retry_count;
	uint64_t bufs[DPAA2_MBUF_MAX_ACQ_REL];

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret != 0) {
			DPAA2_MEMPOOL_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return;
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
	for (i = 0; i < n ; i++) {
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
		bufs[i] = (uint64_t)rte_mempool_virt2iova(obj_table[i])
				+ meta_data_size;
#else
		bufs[i] = (uint64_t)obj_table[i] + meta_data_size;
#endif
	}

	/* feed them to bman */
	retry_count = 0;
	while ((ret = qbman_swp_release(swp, &releasedesc, bufs, n)) ==
			-EBUSY) {
		retry_count++;
		if (retry_count > DPAA2_MAX_TX_RETRY_COUNT) {
			DPAA2_MEMPOOL_ERR("bman release retry exceeded, low fbpr?");
			return;
		}
	}

aligned:
	/* if there are more buffers to free */
	while (n < count) {
		/* convert mbuf to buffers */
		for (i = 0; i < DPAA2_MBUF_MAX_ACQ_REL; i++) {
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
			bufs[i] = (uint64_t)
				  rte_mempool_virt2iova(obj_table[n + i])
				  + meta_data_size;
#else
			bufs[i] = (uint64_t)obj_table[n + i] + meta_data_size;
#endif
		}

		retry_count = 0;
		while ((ret = qbman_swp_release(swp, &releasedesc, bufs,
					DPAA2_MBUF_MAX_ACQ_REL)) == -EBUSY) {
			retry_count++;
			if (retry_count > DPAA2_MAX_TX_RETRY_COUNT) {
				DPAA2_MEMPOOL_ERR("bman release retry exceeded, low fbpr?");
				return;
			}
		}
		n += DPAA2_MBUF_MAX_ACQ_REL;
	}
}

uint16_t
rte_dpaa2_mbuf_pool_bpid(struct rte_mempool *mp)
{
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(mp);
	if (!(bp_info->bp_list)) {
		RTE_LOG(ERR, PMD, "DPAA2 buffer pool not configured\n");
		return -ENOMEM;
	}

	return bp_info->bpid;
}

struct rte_mbuf *
rte_dpaa2_mbuf_from_buf_addr(struct rte_mempool *mp, void *buf_addr)
{
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(mp);
	if (!(bp_info->bp_list)) {
		RTE_LOG(ERR, PMD, "DPAA2 buffer pool not configured\n");
		return NULL;
	}

	return (struct rte_mbuf *)((uint8_t *)buf_addr -
			bp_info->meta_data_size);
}

int
rte_dpaa2_mbuf_alloc_bulk(struct rte_mempool *pool,
			  void **obj_table, unsigned int count)
{
#ifdef RTE_LIBRTE_DPAA2_DEBUG_DRIVER
	static int alloc;
#endif
	struct qbman_swp *swp;
	uint16_t bpid;
	size_t bufs[DPAA2_MBUF_MAX_ACQ_REL];
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
		if (ret != 0) {
			DPAA2_MEMPOOL_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return ret;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		if ((count - n) > DPAA2_MBUF_MAX_ACQ_REL) {
			ret = qbman_swp_acquire(swp, bpid, (void *)bufs,
						DPAA2_MBUF_MAX_ACQ_REL);
		} else {
			ret = qbman_swp_acquire(swp, bpid, (void *)bufs,
						count - n);
		}
		/* In case of less than requested number of buffers available
		 * in pool, qbman_swp_acquire returns 0
		 */
		if (ret <= 0) {
			DPAA2_MEMPOOL_DP_DEBUG(
				"Buffer acquire failed with err code: %d", ret);
			/* The API expect the exact number of requested bufs */
			/* Releasing all buffers allocated */
			rte_dpaa2_mbuf_release(pool, obj_table, bpid,
					   bp_info->meta_data_size, n);
			return -ENOBUFS;
		}
		/* assigning mbuf from the acquired objects */
		for (i = 0; (i < ret) && bufs[i]; i++) {
			DPAA2_MODIFY_IOVA_TO_VADDR(bufs[i], size_t);
			obj_table[n] = (struct rte_mbuf *)
				       (bufs[i] - bp_info->meta_data_size);
			DPAA2_MEMPOOL_DP_DEBUG(
				   "Acquired %p address %p from BMAN\n",
				   (void *)bufs[i], (void *)obj_table[n]);
			n++;
		}
	}

#ifdef RTE_LIBRTE_DPAA2_DEBUG_DRIVER
	alloc += n;
	DPAA2_MEMPOOL_DP_DEBUG("Total = %d , req = %d done = %d\n",
			       alloc, count, n);
#endif
	return 0;
}

static int
rte_hw_mbuf_free_bulk(struct rte_mempool *pool,
		  void * const *obj_table, unsigned int n)
{
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(pool);
	if (!(bp_info->bp_list)) {
		DPAA2_MEMPOOL_ERR("DPAA2 buffer pool not configured");
		return -ENOENT;
	}
	rte_dpaa2_mbuf_release(pool, obj_table, bp_info->bpid,
			   bp_info->meta_data_size, n);

	return 0;
}

static unsigned int
rte_hw_mbuf_get_count(const struct rte_mempool *mp)
{
	int ret;
	unsigned int num_of_bufs = 0;
	struct dpaa2_bp_info *bp_info;
	struct dpaa2_dpbp_dev *dpbp_node;

	if (!mp || !mp->pool_data) {
		DPAA2_MEMPOOL_ERR("Invalid mempool provided");
		return 0;
	}

	bp_info = (struct dpaa2_bp_info *)mp->pool_data;
	dpbp_node = bp_info->bp_list->buf_pool.dpbp_node;

	ret = dpbp_get_num_free_bufs(&dpbp_node->dpbp, CMD_PRI_LOW,
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
	/* The memsegment list exists incase the memory is not external.
	 * So, DMA-Map is required only when memory is provided by user,
	 * i.e. External.
	 */
	msl = rte_mem_virt2memseg_list(vaddr);

	if (!msl) {
		DPAA2_MEMPOOL_DEBUG("Memsegment is External.\n");
		rte_fslmc_vfio_mem_dmamap((size_t)vaddr,
				(size_t)paddr, (size_t)len);
	}
	/* Insert entry into the PA->VA Table */
	dpaax_iova_table_update(paddr, vaddr, len);

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

MEMPOOL_REGISTER_OPS(dpaa2_mpool_ops);

RTE_LOG_REGISTER(dpaa2_logtype_mempool, mempool.dpaa2, NOTICE);
