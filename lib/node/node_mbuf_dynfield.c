/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <rte_memzone.h>
#include <rte_node_mbuf_dynfield.h>
#include <node_private.h>
#include <eal_export.h>
#include <rte_errno.h>

#define NODE_MBUF_DYNFIELD_MEMZONE_NAME "__rte_node_mbuf_dynfield"

struct node_mbuf_dynfield_mz {
	int dynfield_offset;
};

static const struct rte_mbuf_dynfield node_mbuf_dynfield_desc = {
	.name = "rte_node_mbuf_dynfield",
	.size = sizeof(rte_node_mbuf_dynfield_t),
	.align = alignof(rte_node_mbuf_dynfield_t),
};

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_node_mbuf_dynfield_register, 25.07);
int rte_node_mbuf_dynfield_register(void)
{
	struct node_mbuf_dynfield_mz *f = NULL;
	const struct rte_memzone *mz = NULL;
	int dyn_offset;

	RTE_BUILD_BUG_ON(sizeof(rte_node_mbuf_dynfield_t) < RTE_NODE_MBUF_DYNFIELD_SIZE);
	RTE_BUILD_BUG_ON(sizeof(rte_node_mbuf_overload_fields_t) <
			 RTE_NODE_MBUF_OVERLOADABLE_FIELDS_SIZE);

	mz = rte_memzone_lookup(NODE_MBUF_DYNFIELD_MEMZONE_NAME);

	if (!mz) {
		mz = rte_memzone_reserve_aligned(NODE_MBUF_DYNFIELD_MEMZONE_NAME,
						 sizeof(struct node_mbuf_dynfield_mz),
						 SOCKET_ID_ANY, 0,
						 RTE_CACHE_LINE_SIZE);
		if (!mz) {
			node_err("node_mbuf_dyn", "rte_memzone_reserve_aligned failed");
			rte_errno = ENOMEM;
			return -1;
		}
		dyn_offset = rte_mbuf_dynfield_register(&node_mbuf_dynfield_desc);
		if (dyn_offset < 0) {
			node_err("node_mbuf_dyn", "rte_mbuf_dynfield_register failed");
			return dyn_offset;
		}
		f = (struct node_mbuf_dynfield_mz *)mz->addr;
		f->dynfield_offset = dyn_offset;

		node_dbg("node_mbuf_dyn", "memzone: %s of size: %zu at offset : %d",
			 mz->name, mz->len, f->dynfield_offset);
	} else {
		f = (struct node_mbuf_dynfield_mz *)mz->addr;
	}
	return f->dynfield_offset;
}
