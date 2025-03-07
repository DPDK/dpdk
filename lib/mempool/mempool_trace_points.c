/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <eal_export.h>
#include <rte_trace_point_register.h>

#include "mempool_trace.h"

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_ops_dequeue_bulk, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_ops_dequeue_bulk,
	lib.mempool.ops.deq.bulk)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_ops_dequeue_contig_blocks, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_ops_dequeue_contig_blocks,
	lib.mempool.ops.deq.contig)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_ops_enqueue_bulk, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_ops_enqueue_bulk,
	lib.mempool.ops.enq.bulk)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_generic_put, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_generic_put,
	lib.mempool.generic.put)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_put_bulk, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_put_bulk,
	lib.mempool.put.bulk)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_generic_get, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_generic_get,
	lib.mempool.generic.get)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_get_bulk, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_get_bulk,
	lib.mempool.get.bulk)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_get_contig_blocks, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_get_contig_blocks,
	lib.mempool.get.blocks)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_create,
	lib.mempool.create)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_create_empty,
	lib.mempool.create.empty)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_free,
	lib.mempool.free)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_populate_iova,
	lib.mempool.populate.iova)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_populate_virt,
	lib.mempool.populate.virt)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_populate_default,
	lib.mempool.populate.default)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_populate_anon,
	lib.mempool.populate.anon)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_cache_create,
	lib.mempool.cache_create)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_cache_free,
	lib.mempool.cache.free)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_default_cache, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_default_cache,
	lib.mempool.default.cache)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_get_page_size,
	lib.mempool.get.page.size)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_mempool_trace_cache_flush, 20.05)
RTE_TRACE_POINT_REGISTER(rte_mempool_trace_cache_flush,
	lib.mempool.cache.flush)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_ops_populate,
	lib.mempool.ops.populate)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_ops_alloc,
	lib.mempool.ops.alloc)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_ops_free,
	lib.mempool.ops.free)

RTE_TRACE_POINT_REGISTER(rte_mempool_trace_set_ops_byname,
	lib.mempool.set.ops.byname)
