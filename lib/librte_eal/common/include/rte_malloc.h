/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_MALLOC_H_
#define _RTE_MALLOC_H_

/**
 * @file
 * RTE Malloc. This library provides methods for dynamically allocating memory
 * from hugepages.
 */

#include <stdio.h>
#include <stddef.h>
#include <rte_compat.h>
#include <rte_memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Structure to hold heap statistics obtained from rte_malloc_get_socket_stats function.
 */
struct rte_malloc_socket_stats {
	size_t heap_totalsz_bytes; /**< Total bytes on heap */
	size_t heap_freesz_bytes;  /**< Total free bytes on heap */
	size_t greatest_free_size; /**< Size in bytes of largest free block */
	unsigned free_count;       /**< Number of free elements on heap */
	unsigned alloc_count;      /**< Number of allocated elements on heap */
	size_t heap_allocsz_bytes; /**< Total allocated bytes on heap */
};

/**
 * This function allocates memory from the huge-page area of memory. The memory
 * is not cleared. In NUMA systems, the memory allocated resides on the same
 * NUMA socket as the core that calls this function.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
void *
rte_malloc(const char *type, size_t size, unsigned align);

/**
 * Allocate zero'ed memory from the heap.
 *
 * Equivalent to rte_malloc() except that the memory zone is
 * initialised with zeros. In NUMA systems, the memory allocated resides on the
 * same NUMA socket as the core that calls this function.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
void *
rte_zmalloc(const char *type, size_t size, unsigned align);

/**
 * Replacement function for calloc(), using huge-page memory. Memory area is
 * initialised with zeros. In NUMA systems, the memory allocated resides on the
 * same NUMA socket as the core that calls this function.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param num
 *   Number of elements to be allocated.
 * @param size
 *   Size (in bytes) of a single element.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
void *
rte_calloc(const char *type, size_t num, size_t size, unsigned align);

/**
 * Replacement function for realloc(), using huge-page memory. Reserved area
 * memory is resized, preserving contents. In NUMA systems, the new area
 * resides on the same NUMA socket as the old area.
 *
 * @param ptr
 *   Pointer to already allocated memory
 * @param size
 *   Size (in bytes) of new area. If this is 0, memory is freed.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the reallocated memory.
 */
void *
rte_realloc(void *ptr, size_t size, unsigned align);

/**
 * This function allocates memory from the huge-page area of memory. The memory
 * is not cleared.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this function
 *   will behave the same as rte_malloc().
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
void *
rte_malloc_socket(const char *type, size_t size, unsigned align, int socket);

/**
 * Allocate zero'ed memory from the heap.
 *
 * Equivalent to rte_malloc() except that the memory zone is
 * initialised with zeros.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this function
 *   will behave the same as rte_zmalloc().
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
void *
rte_zmalloc_socket(const char *type, size_t size, unsigned align, int socket);

/**
 * Replacement function for calloc(), using huge-page memory. Memory area is
 * initialised with zeros.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param num
 *   Number of elements to be allocated.
 * @param size
 *   Size (in bytes) of a single element.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this function
 *   will behave the same as rte_calloc().
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
void *
rte_calloc_socket(const char *type, size_t num, size_t size, unsigned align, int socket);

/**
 * Frees the memory space pointed to by the provided pointer.
 *
 * This pointer must have been returned by a previous call to
 * rte_malloc(), rte_zmalloc(), rte_calloc() or rte_realloc(). The behaviour of
 * rte_free() is undefined if the pointer does not match this requirement.
 *
 * If the pointer is NULL, the function does nothing.
 *
 * @param ptr
 *   The pointer to memory to be freed.
 */
void
rte_free(void *ptr);

/**
 * If malloc debug is enabled, check a memory block for header
 * and trailer markers to indicate that all is well with the block.
 * If size is non-null, also return the size of the block.
 *
 * @param ptr
 *   pointer to the start of a data block, must have been returned
 *   by a previous call to rte_malloc(), rte_zmalloc(), rte_calloc()
 *   or rte_realloc()
 * @param size
 *   if non-null, and memory block pointer is valid, returns the size
 *   of the memory block
 * @return
 *   -1 on error, invalid pointer passed or header and trailer markers
 *   are missing or corrupted
 *   0 on success
 */
int
rte_malloc_validate(const void *ptr, size_t *size);

/**
 * Get heap statistics for the specified heap.
 *
 * @param socket
 *   An unsigned integer specifying the socket to get heap statistics for
 * @param socket_stats
 *   A structure which provides memory to store statistics
 * @return
 *   Null on error
 *   Pointer to structure storing statistics on success
 */
int
rte_malloc_get_socket_stats(int socket,
		struct rte_malloc_socket_stats *socket_stats);

/**
 * Dump statistics.
 *
 * Dump for the specified type to a file. If the type argument is
 * NULL, all memory types will be dumped.
 *
 * @param f
 *   A pointer to a file for output
 * @param type
 *   A string identifying the type of objects to dump, or NULL
 *   to dump all objects.
 */
void
rte_malloc_dump_stats(FILE *f, const char *type);

/**
 * Dump contents of all malloc heaps to a file.
 *
 * @param f
 *   A pointer to a file for output
 */
void __rte_experimental
rte_malloc_dump_heaps(FILE *f);

/**
 * Set the maximum amount of allocated memory for this type.
 *
 * This is not yet implemented
 *
 * @param type
 *   A string identifying the type of allocated objects.
 * @param max
 *   The maximum amount of allocated bytes for this type.
 * @return
 *   - 0: Success.
 *   - (-1): Error.
 */
int
rte_malloc_set_limit(const char *type, size_t max);

/**
 * Return the IO address of a virtual address obtained through
 * rte_malloc
 *
 * @param addr
 *   Address obtained from a previous rte_malloc call
 * @return
 *   RTE_BAD_IOVA on error
 *   otherwise return an address suitable for IO
 */
rte_iova_t
rte_malloc_virt2iova(const void *addr);

__rte_deprecated
static inline phys_addr_t
rte_malloc_virt2phy(const void *addr)
{
	return rte_malloc_virt2iova(addr);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MALLOC_H_ */
