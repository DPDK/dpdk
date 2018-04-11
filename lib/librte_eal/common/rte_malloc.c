/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_spinlock.h>

#include <rte_malloc.h>
#include "malloc_elem.h"
#include "malloc_heap.h"


/* Free the memory space back to heap */
void rte_free(void *addr)
{
	if (addr == NULL) return;
	if (malloc_heap_free(malloc_elem_from_data(addr)) < 0)
		RTE_LOG(ERR, EAL, "Error: Invalid memory\n");
}

/*
 * Allocate memory on specified heap.
 */
void *
rte_malloc_socket(const char *type, size_t size, unsigned int align,
		int socket_arg)
{
	/* return NULL if size is 0 or alignment is not power-of-2 */
	if (size == 0 || (align && !rte_is_power_of_2(align)))
		return NULL;

	if (!rte_eal_has_hugepages())
		socket_arg = SOCKET_ID_ANY;

	/* Check socket parameter */
	if (socket_arg >= RTE_MAX_NUMA_NODES)
		return NULL;

	return malloc_heap_alloc(type, size, socket_arg, 0,
			align == 0 ? 1 : align, 0, false);
}

/*
 * Allocate memory on default heap.
 */
void *
rte_malloc(const char *type, size_t size, unsigned align)
{
	return rte_malloc_socket(type, size, align, SOCKET_ID_ANY);
}

/*
 * Allocate zero'd memory on specified heap.
 */
void *
rte_zmalloc_socket(const char *type, size_t size, unsigned align, int socket)
{
	return rte_malloc_socket(type, size, align, socket);
}

/*
 * Allocate zero'd memory on default heap.
 */
void *
rte_zmalloc(const char *type, size_t size, unsigned align)
{
	return rte_zmalloc_socket(type, size, align, SOCKET_ID_ANY);
}

/*
 * Allocate zero'd memory on specified heap.
 */
void *
rte_calloc_socket(const char *type, size_t num, size_t size, unsigned align, int socket)
{
	return rte_zmalloc_socket(type, num * size, align, socket);
}

/*
 * Allocate zero'd memory on default heap.
 */
void *
rte_calloc(const char *type, size_t num, size_t size, unsigned align)
{
	return rte_zmalloc(type, num * size, align);
}

/*
 * Resize allocated memory.
 */
void *
rte_realloc(void *ptr, size_t size, unsigned align)
{
	if (ptr == NULL)
		return rte_malloc(NULL, size, align);

	struct malloc_elem *elem = malloc_elem_from_data(ptr);
	if (elem == NULL) {
		RTE_LOG(ERR, EAL, "Error: memory corruption detected\n");
		return NULL;
	}

	size = RTE_CACHE_LINE_ROUNDUP(size), align = RTE_CACHE_LINE_ROUNDUP(align);
	/* check alignment matches first, and if ok, see if we can resize block */
	if (RTE_PTR_ALIGN(ptr,align) == ptr &&
			malloc_heap_resize(elem, size) == 0)
		return ptr;

	/* either alignment is off, or we have no room to expand,
	 * so move data. */
	void *new_ptr = rte_malloc(NULL, size, align);
	if (new_ptr == NULL)
		return NULL;
	const unsigned old_size = elem->size - MALLOC_ELEM_OVERHEAD;
	rte_memcpy(new_ptr, ptr, old_size < size ? old_size : size);
	rte_free(ptr);

	return new_ptr;
}

int
rte_malloc_validate(const void *ptr, size_t *size)
{
	const struct malloc_elem *elem = malloc_elem_from_data(ptr);
	if (!malloc_elem_cookies_ok(elem))
		return -1;
	if (size != NULL)
		*size = elem->size - elem->pad - MALLOC_ELEM_OVERHEAD;
	return 0;
}

/*
 * Function to retrieve data for heap on given socket
 */
int
rte_malloc_get_socket_stats(int socket,
		struct rte_malloc_socket_stats *socket_stats)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;

	if (socket >= RTE_MAX_NUMA_NODES || socket < 0)
		return -1;

	return malloc_heap_get_stats(&mcfg->malloc_heaps[socket], socket_stats);
}

/*
 * Function to dump contents of all heaps
 */
void __rte_experimental
rte_malloc_dump_heaps(FILE *f)
{
	struct rte_mem_config *mcfg = rte_eal_get_configuration()->mem_config;
	unsigned int idx;

	for (idx = 0; idx < rte_socket_count(); idx++) {
		unsigned int socket = rte_socket_id_by_idx(idx);
		fprintf(f, "Heap on socket %i:\n", socket);
		malloc_heap_dump(&mcfg->malloc_heaps[socket], f);
	}

}

/*
 * Print stats on memory type. If type is NULL, info on all types is printed
 */
void
rte_malloc_dump_stats(FILE *f, __rte_unused const char *type)
{
	unsigned int socket;
	struct rte_malloc_socket_stats sock_stats;
	/* Iterate through all initialised heaps */
	for (socket=0; socket< RTE_MAX_NUMA_NODES; socket++) {
		if ((rte_malloc_get_socket_stats(socket, &sock_stats) < 0))
			continue;

		fprintf(f, "Socket:%u\n", socket);
		fprintf(f, "\tHeap_size:%zu,\n", sock_stats.heap_totalsz_bytes);
		fprintf(f, "\tFree_size:%zu,\n", sock_stats.heap_freesz_bytes);
		fprintf(f, "\tAlloc_size:%zu,\n", sock_stats.heap_allocsz_bytes);
		fprintf(f, "\tGreatest_free_size:%zu,\n",
				sock_stats.greatest_free_size);
		fprintf(f, "\tAlloc_count:%u,\n",sock_stats.alloc_count);
		fprintf(f, "\tFree_count:%u,\n", sock_stats.free_count);
	}
	return;
}

/*
 * TODO: Set limit to memory that can be allocated to memory type
 */
int
rte_malloc_set_limit(__rte_unused const char *type,
		__rte_unused size_t max)
{
	return 0;
}

/*
 * Return the IO address of a virtual address obtained through rte_malloc
 */
rte_iova_t
rte_malloc_virt2iova(const void *addr)
{
	const struct rte_memseg *ms;
	struct malloc_elem *elem = malloc_elem_from_data(addr);

	if (elem == NULL)
		return RTE_BAD_IOVA;

	if (rte_eal_iova_mode() == RTE_IOVA_VA)
		return (uintptr_t) addr;

	ms = rte_mem_virt2memseg(addr, elem->msl);
	if (ms == NULL)
		return RTE_BAD_IOVA;

	if (ms->iova == RTE_BAD_IOVA)
		return RTE_BAD_IOVA;

	return ms->iova + RTE_PTR_DIFF(addr, ms->addr);
}
