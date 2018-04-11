/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef EAL_MEMALLOC_H
#define EAL_MEMALLOC_H

#include <stdbool.h>

#include <rte_memory.h>

/*
 * Allocate segment of specified page size.
 */
struct rte_memseg *
eal_memalloc_alloc_seg(size_t page_sz, int socket);

/*
 * Allocate `n_segs` segments.
 *
 * Note: `ms` can be NULL.
 *
 * Note: it is possible to request best-effort allocation by setting `exact` to
 * `false`, in which case allocator will return however many pages it managed to
 * allocate successfully.
 */
int
eal_memalloc_alloc_seg_bulk(struct rte_memseg **ms, int n_segs, size_t page_sz,
		int socket, bool exact);

#endif /* EAL_MEMALLOC_H */
