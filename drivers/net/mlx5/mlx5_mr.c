/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   Copyright 2016 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_mempool.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"

struct mlx5_check_mempool_data {
	int ret;
	char *start;
	char *end;
};

/* Called by mlx5_check_mempool() when iterating the memory chunks. */
static void
mlx5_check_mempool_cb(struct rte_mempool *mp,
		      void *opaque, struct rte_mempool_memhdr *memhdr,
		      unsigned int mem_idx)
{
	struct mlx5_check_mempool_data *data = opaque;

	(void)mp;
	(void)mem_idx;

	/* It already failed, skip the next chunks. */
	if (data->ret != 0)
		return;
	/* It is the first chunk. */
	if (data->start == NULL && data->end == NULL) {
		data->start = memhdr->addr;
		data->end = data->start + memhdr->len;
		return;
	}
	if (data->end == memhdr->addr) {
		data->end += memhdr->len;
		return;
	}
	if (data->start == (char *)memhdr->addr + memhdr->len) {
		data->start -= memhdr->len;
		return;
	}
	/* Error, mempool is not virtually contiguous. */
	data->ret = -1;
}

/**
 * Check if a mempool can be used: it must be virtually contiguous.
 *
 * @param[in] mp
 *   Pointer to memory pool.
 * @param[out] start
 *   Pointer to the start address of the mempool virtual memory area
 * @param[out] end
 *   Pointer to the end address of the mempool virtual memory area
 *
 * @return
 *   0 on success (mempool is virtually contiguous), -1 on error.
 */
static int mlx5_check_mempool(struct rte_mempool *mp, uintptr_t *start,
	uintptr_t *end)
{
	struct mlx5_check_mempool_data data;

	memset(&data, 0, sizeof(data));
	rte_mempool_mem_iter(mp, mlx5_check_mempool_cb, &data);
	*start = (uintptr_t)data.start;
	*end = (uintptr_t)data.end;

	return data.ret;
}

/**
 * Register mempool as a memory region.
 *
 * @param pd
 *   Pointer to protection domain.
 * @param mp
 *   Pointer to memory pool.
 *
 * @return
 *   Memory region pointer, NULL in case of error.
 */
struct ibv_mr *
mlx5_mp2mr(struct ibv_pd *pd, struct rte_mempool *mp)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	uintptr_t start;
	uintptr_t end;
	unsigned int i;

	if (mlx5_check_mempool(mp, &start, &end) != 0) {
		ERROR("mempool %p: not virtually contiguous",
		      (void *)mp);
		return NULL;
	}

	DEBUG("mempool %p area start=%p end=%p size=%zu",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	/* Round start and end to page boundary if found in memory segments. */
	for (i = 0; (i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL); ++i) {
		uintptr_t addr = (uintptr_t)ms[i].addr;
		size_t len = ms[i].len;
		unsigned int align = ms[i].hugepage_sz;

		if ((start > addr) && (start < addr + len))
			start = RTE_ALIGN_FLOOR(start, align);
		if ((end > addr) && (end < addr + len))
			end = RTE_ALIGN_CEIL(end, align);
	}
	DEBUG("mempool %p using start=%p end=%p size=%zu for MR",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	return ibv_reg_mr(pd,
			  (void *)start,
			  end - start,
			  IBV_ACCESS_LOCAL_WRITE);
}

/**
 * Register a Memory Region (MR) <-> Memory Pool (MP) association in
 * txq->mp2mr[]. If mp2mr[] is full, remove an entry first.
 *
 * This function should only be called by txq_mp2mr().
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 * @param idx
 *   Index of the next available entry.
 *
 * @return
 *   mr->lkey on success, (uint32_t)-1 on failure.
 */
uint32_t
txq_mp2mr_reg(struct txq *txq, struct rte_mempool *mp, unsigned int idx)
{
	struct txq_ctrl *txq_ctrl = container_of(txq, struct txq_ctrl, txq);
	struct ibv_mr *mr;

	/* Add a new entry, register MR first. */
	DEBUG("%p: discovered new memory pool \"%s\" (%p)",
	      (void *)txq_ctrl, mp->name, (void *)mp);
	mr = mlx5_mp2mr(txq_ctrl->priv->pd, mp);
	if (unlikely(mr == NULL)) {
		DEBUG("%p: unable to configure MR, ibv_reg_mr() failed.",
		      (void *)txq_ctrl);
		return (uint32_t)-1;
	}
	if (unlikely(idx == RTE_DIM(txq_ctrl->txq.mp2mr))) {
		/* Table is full, remove oldest entry. */
		DEBUG("%p: MR <-> MP table full, dropping oldest entry.",
		      (void *)txq_ctrl);
		--idx;
		claim_zero(ibv_dereg_mr(txq_ctrl->txq.mp2mr[0].mr));
		memmove(&txq_ctrl->txq.mp2mr[0], &txq_ctrl->txq.mp2mr[1],
			(sizeof(txq_ctrl->txq.mp2mr) -
			 sizeof(txq_ctrl->txq.mp2mr[0])));
	}
	/* Store the new entry. */
	txq_ctrl->txq.mp2mr[idx].mp = mp;
	txq_ctrl->txq.mp2mr[idx].mr = mr;
	txq_ctrl->txq.mp2mr[idx].lkey = htonl(mr->lkey);
	DEBUG("%p: new MR lkey for MP \"%s\" (%p): 0x%08" PRIu32,
	      (void *)txq_ctrl, mp->name, (void *)mp,
	      txq_ctrl->txq.mp2mr[idx].lkey);
	return txq_ctrl->txq.mp2mr[idx].lkey;
}

struct txq_mp2mr_mbuf_check_data {
	int ret;
};

/**
 * Callback function for rte_mempool_obj_iter() to check whether a given
 * mempool object looks like a mbuf.
 *
 * @param[in] mp
 *   The mempool pointer
 * @param[in] arg
 *   Context data (struct txq_mp2mr_mbuf_check_data). Contains the
 *   return value.
 * @param[in] obj
 *   Object address.
 * @param index
 *   Object index, unused.
 */
static void
txq_mp2mr_mbuf_check(struct rte_mempool *mp, void *arg, void *obj,
	uint32_t index __rte_unused)
{
	struct txq_mp2mr_mbuf_check_data *data = arg;
	struct rte_mbuf *buf = obj;

	/*
	 * Check whether mbuf structure fits element size and whether mempool
	 * pointer is valid.
	 */
	if (sizeof(*buf) > mp->elt_size || buf->pool != mp)
		data->ret = -1;
}

/**
 * Iterator function for rte_mempool_walk() to register existing mempools and
 * fill the MP to MR cache of a TX queue.
 *
 * @param[in] mp
 *   Memory Pool to register.
 * @param *arg
 *   Pointer to TX queue structure.
 */
void
txq_mp2mr_iter(struct rte_mempool *mp, void *arg)
{
	struct txq_ctrl *txq_ctrl = arg;
	struct txq_mp2mr_mbuf_check_data data = {
		.ret = 0,
	};
	unsigned int i;

	/* Register mempool only if the first element looks like a mbuf. */
	if (rte_mempool_obj_iter(mp, txq_mp2mr_mbuf_check, &data) == 0 ||
			data.ret == -1)
		return;
	for (i = 0; (i != RTE_DIM(txq_ctrl->txq.mp2mr)); ++i) {
		if (unlikely(txq_ctrl->txq.mp2mr[i].mp == NULL)) {
			/* Unknown MP, add a new MR for it. */
			break;
		}
		if (txq_ctrl->txq.mp2mr[i].mp == mp)
			return;
	}
	txq_mp2mr_reg(&txq_ctrl->txq, mp, i);
}
