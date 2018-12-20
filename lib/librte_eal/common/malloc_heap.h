/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef MALLOC_HEAP_H_
#define MALLOC_HEAP_H_

#include <stdbool.h>

#include <rte_malloc.h>
#include <rte_malloc_heap.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline unsigned
malloc_get_numa_socket(void)
{
	unsigned socket_id = rte_socket_id();

	if (socket_id == (unsigned)SOCKET_ID_ANY)
		return 0;

	return socket_id;
}

void *
malloc_heap_alloc(const char *type, size_t size, int socket, unsigned int flags,
		size_t align, size_t bound, bool contig);

void *
malloc_heap_alloc_biggest(const char *type, int socket, unsigned int flags,
		size_t align, bool contig);

int
malloc_heap_create(struct malloc_heap *heap, const char *heap_name);

int
malloc_heap_destroy(struct malloc_heap *heap);

struct rte_memseg_list *
malloc_heap_create_external_seg(void *va_addr, rte_iova_t iova_addrs[],
		unsigned int n_pages, size_t page_sz, const char *seg_name,
		unsigned int socket_id);

struct rte_memseg_list *
malloc_heap_find_external_seg(void *va_addr, size_t len);

int
malloc_heap_destroy_external_seg(struct rte_memseg_list *msl);

int
malloc_heap_add_external_memory(struct malloc_heap *heap,
		struct rte_memseg_list *msl);

int
malloc_heap_remove_external_memory(struct malloc_heap *heap, void *va_addr,
		size_t len);

int
malloc_heap_free(struct malloc_elem *elem);

int
malloc_heap_resize(struct malloc_elem *elem, size_t size);

int
malloc_heap_get_stats(struct malloc_heap *heap,
		struct rte_malloc_socket_stats *socket_stats);

void
malloc_heap_dump(struct malloc_heap *heap, FILE *f);

int
malloc_socket_to_heap_id(unsigned int socket_id);

int
rte_eal_malloc_heap_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MALLOC_HEAP_H_ */
