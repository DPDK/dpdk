/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PDCP_PROCESS_H
#define PDCP_PROCESS_H

#include <rte_mbuf_dyn.h>
#include <rte_pdcp.h>

typedef uint32_t rte_pdcp_dynfield_t;

extern int rte_pdcp_dynfield_offset;

static inline rte_pdcp_dynfield_t *
pdcp_dynfield(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, rte_pdcp_dynfield_offset, rte_pdcp_dynfield_t *);
}

int
pdcp_process_func_set(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf);

#endif /* PDCP_PROCESS_H */
