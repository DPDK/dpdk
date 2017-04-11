/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright (c) 2016 NXP. All rights reserved.
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
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
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

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_ethdev.h>

#include <fslmc_logs.h>
#include <mc/fsl_dpbp.h>
#include <portal/dpaa2_hw_pvt.h>
#include <portal/dpaa2_hw_dpio.h>
#include "dpaa2_hw_mempool.h"

struct dpaa2_bp_info rte_dpaa2_bpid_info[MAX_BPID];
static struct dpaa2_bp_list *h_bp_list;

static int
rte_hw_mbuf_create_pool(struct rte_mempool *mp)
{
	struct dpaa2_bp_list *bp_list;
	struct dpaa2_dpbp_dev *avail_dpbp;
	struct dpbp_attr dpbp_attr;
	uint32_t bpid;
	int ret, p_ret;

	avail_dpbp = dpaa2_alloc_dpbp_dev();

	if (!avail_dpbp) {
		PMD_DRV_LOG(ERR, "DPAA2 resources not available");
		return -ENOENT;
	}

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			RTE_LOG(ERR, PMD, "Failure in affining portal\n");
			return ret;
		}
	}

	ret = dpbp_enable(&avail_dpbp->dpbp, CMD_PRI_LOW, avail_dpbp->token);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Resource enable failure with"
			" err code: %d\n", ret);
		return ret;
	}

	ret = dpbp_get_attributes(&avail_dpbp->dpbp, CMD_PRI_LOW,
				  avail_dpbp->token, &dpbp_attr);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Resource read failure with"
			     " err code: %d\n", ret);
		p_ret = ret;
		ret = dpbp_disable(&avail_dpbp->dpbp, CMD_PRI_LOW,
				   avail_dpbp->token);
		return p_ret;
	}

	/* Allocate the bp_list which will be added into global_bp_list */
	bp_list = rte_malloc(NULL, sizeof(struct dpaa2_bp_list),
			     RTE_CACHE_LINE_SIZE);
	if (!bp_list) {
		PMD_INIT_LOG(ERR, "No heap memory available");
		return -ENOMEM;
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

	mp->pool_data = (void *)&rte_dpaa2_bpid_info[bpid];

	PMD_INIT_LOG(DEBUG, "BP List created for bpid =%d", dpbp_attr.bpid);

	h_bp_list = bp_list;
	return 0;
}

static void
rte_hw_mbuf_free_pool(struct rte_mempool *mp)
{
	struct dpaa2_bp_info *bpinfo;
	struct dpaa2_bp_list *bp;
	struct dpaa2_dpbp_dev *dpbp_node;

	if (!mp->pool_data) {
		PMD_DRV_LOG(ERR, "Not a valid dpaa22 pool");
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
				free(bp);
				break;
			}
			prev = temp;
			temp = temp->next;
		}
	}

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
	int i, n;
	uint64_t bufs[DPAA2_MBUF_MAX_ACQ_REL];

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret != 0) {
			RTE_LOG(ERR, PMD, "Failed to allocate IO portal");
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
		bufs[i] = (uint64_t)rte_mempool_virt2phy(pool, obj_table[i])
				+ meta_data_size;
#else
		bufs[i] = (uint64_t)obj_table[i] + meta_data_size;
#endif
	}

	/* feed them to bman */
	do {
		ret = qbman_swp_release(swp, &releasedesc, bufs, n);
	} while (ret == -EBUSY);

aligned:
	/* if there are more buffers to free */
	while (n < count) {
		/* convert mbuf to buffers */
		for (i = 0; i < DPAA2_MBUF_MAX_ACQ_REL; i++) {
#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
			bufs[i] = (uint64_t)
				  rte_mempool_virt2phy(pool, obj_table[n + i])
				  + meta_data_size;
#else
			bufs[i] = (uint64_t)obj_table[n + i] + meta_data_size;
#endif
		}

		do {
			ret = qbman_swp_release(swp, &releasedesc, bufs,
						DPAA2_MBUF_MAX_ACQ_REL);
		} while (ret == -EBUSY);
		n += DPAA2_MBUF_MAX_ACQ_REL;
	}
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
	uint64_t bufs[DPAA2_MBUF_MAX_ACQ_REL];
	int i, ret;
	unsigned int n = 0;
	struct dpaa2_bp_info *bp_info;

	bp_info = mempool_to_bpinfo(pool);

	if (!(bp_info->bp_list)) {
		RTE_LOG(ERR, PMD, "DPAA2 buffer pool not configured\n");
		return -ENOENT;
	}

	bpid = bp_info->bpid;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret != 0) {
			RTE_LOG(ERR, PMD, "Failed to allocate IO portal");
			return ret;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	while (n < count) {
		/* Acquire is all-or-nothing, so we drain in 7s,
		 * then the remainder.
		 */
		if ((count - n) > DPAA2_MBUF_MAX_ACQ_REL) {
			ret = qbman_swp_acquire(swp, bpid, bufs,
						DPAA2_MBUF_MAX_ACQ_REL);
		} else {
			ret = qbman_swp_acquire(swp, bpid, bufs,
						count - n);
		}
		/* In case of less than requested number of buffers available
		 * in pool, qbman_swp_acquire returns 0
		 */
		if (ret <= 0) {
			PMD_TX_LOG(ERR, "Buffer acquire failed with"
				   " err code: %d", ret);
			/* The API expect the exact number of requested bufs */
			/* Releasing all buffers allocated */
			rte_dpaa2_mbuf_release(pool, obj_table, bpid,
					   bp_info->meta_data_size, n);
			return ret;
		}
		/* assigning mbuf from the acquired objects */
		for (i = 0; (i < ret) && bufs[i]; i++) {
			DPAA2_MODIFY_IOVA_TO_VADDR(bufs[i], uint64_t);
			obj_table[n] = (struct rte_mbuf *)
				       (bufs[i] - bp_info->meta_data_size);
			PMD_TX_LOG(DEBUG, "Acquired %p address %p from BMAN",
				   (void *)bufs[i], (void *)obj_table[n]);
			n++;
		}
	}

#ifdef RTE_LIBRTE_DPAA2_DEBUG_DRIVER
	alloc += n;
	PMD_TX_LOG(DEBUG, "Total = %d , req = %d done = %d",
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
		RTE_LOG(ERR, PMD, "DPAA2 buffer pool not configured");
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
		RTE_LOG(ERR, PMD, "Invalid mempool provided");
		return 0;
	}

	bp_info = (struct dpaa2_bp_info *)mp->pool_data;
	dpbp_node = bp_info->bp_list->buf_pool.dpbp_node;

	ret = dpbp_get_num_free_bufs(&dpbp_node->dpbp, CMD_PRI_LOW,
				     dpbp_node->token, &num_of_bufs);
	if (ret) {
		RTE_LOG(ERR, PMD, "Unable to obtain free buf count (err=%d)",
			ret);
		return 0;
	}

	RTE_LOG(DEBUG, PMD, "Free bufs = %u", num_of_bufs);

	return num_of_bufs;
}

struct rte_mempool_ops dpaa2_mpool_ops = {
	.name = "dpaa2",
	.alloc = rte_hw_mbuf_create_pool,
	.free = rte_hw_mbuf_free_pool,
	.enqueue = rte_hw_mbuf_free_bulk,
	.dequeue = rte_dpaa2_mbuf_alloc_bulk,
	.get_count = rte_hw_mbuf_get_count,
};

MEMPOOL_REGISTER_OPS(dpaa2_mpool_ops);
