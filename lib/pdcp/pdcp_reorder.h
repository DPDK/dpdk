/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_REORDER_H
#define PDCP_REORDER_H

#include <rte_reorder.h>

struct pdcp_reorder {
	struct rte_reorder_buffer *buf;
	bool is_active;
};

int pdcp_reorder_create(struct pdcp_reorder *reorder, size_t nb_elem, void *mem, size_t mem_size);

/* NOTE: replace with `rte_reorder_memory_footprint_get` after DPDK 23.07 */
#define SIZE_OF_REORDER_BUFFER (4 * RTE_CACHE_LINE_SIZE)
static inline size_t
pdcp_reorder_memory_footprint_get(size_t nb_elem)
{
	return SIZE_OF_REORDER_BUFFER + (2 * nb_elem * sizeof(struct rte_mbuf *));
}

static inline uint32_t
pdcp_reorder_get_sequential(struct pdcp_reorder *reorder, struct rte_mbuf **mbufs,
		uint32_t max_mbufs)
{
	return rte_reorder_drain(reorder->buf, mbufs, max_mbufs);
}

static inline uint32_t
pdcp_reorder_up_to_get(struct pdcp_reorder *reorder, struct rte_mbuf **mbufs,
		       uint32_t max_mbufs, uint32_t seqn)
{
	return rte_reorder_drain_up_to_seqn(reorder->buf, mbufs, max_mbufs, seqn);
}

static inline void
pdcp_reorder_start(struct pdcp_reorder *reorder, uint32_t min_seqn)
{
	int ret;

	reorder->is_active = true;

	ret = rte_reorder_min_seqn_set(reorder->buf, min_seqn);
	RTE_VERIFY(ret == 0);
}

static inline void
pdcp_reorder_stop(struct pdcp_reorder *reorder)
{
	reorder->is_active = false;
}

static inline void
pdcp_reorder_insert(struct pdcp_reorder *reorder, struct rte_mbuf *mbuf,
		    rte_reorder_seqn_t pkt_count)
{
	int ret;

	*rte_reorder_seqn(mbuf) = pkt_count;

	ret = rte_reorder_insert(reorder->buf, mbuf);
	RTE_VERIFY(ret == 0);
}

#endif /* PDCP_REORDER_H */
