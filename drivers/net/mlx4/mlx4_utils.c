/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Utility functions used by the mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memory.h>

#include "mlx4_utils.h"

/**
 * Make a file descriptor non-blocking.
 *
 * @param fd
 *   File descriptor to alter.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_fd_set_non_blocking(int fd)
{
	int ret = fcntl(fd, F_GETFL);

	if (ret != -1 && !fcntl(fd, F_SETFL, ret | O_NONBLOCK))
		return 0;
	assert(errno);
	rte_errno = errno;
	return -rte_errno;
}

/**
 * Internal helper to allocate memory once for several disparate objects.
 *
 * The most restrictive alignment constraint for standard objects is assumed
 * to be sizeof(double) and is used as a default value.
 *
 * C11 code would include stdalign.h and use alignof(max_align_t) however
 * we'll stick with C99 for the time being.
 */
static inline size_t
mlx4_mallocv_inline(const char *type, const struct mlx4_malloc_vec *vec,
		    unsigned int cnt, int zero, int socket)
{
	unsigned int i;
	size_t size;
	size_t least;
	uint8_t *data = NULL;
	int fill = !vec[0].addr;

fill:
	size = 0;
	least = 0;
	for (i = 0; i < cnt; ++i) {
		size_t align = (uintptr_t)vec[i].align;

		if (!align) {
			align = sizeof(double);
		} else if (!rte_is_power_of_2(align)) {
			rte_errno = EINVAL;
			goto error;
		}
		if (least < align)
			least = align;
		align = RTE_ALIGN_CEIL(size, align);
		size = align + vec[i].size;
		if (fill && vec[i].addr)
			*vec[i].addr = data + align;
	}
	if (fill)
		return size;
	if (!zero)
		data = rte_malloc_socket(type, size, least, socket);
	else
		data = rte_zmalloc_socket(type, size, least, socket);
	if (data) {
		fill = 1;
		goto fill;
	}
	rte_errno = ENOMEM;
error:
	for (i = 0; i != cnt; ++i)
		if (vec[i].addr)
			*vec[i].addr = NULL;
	return 0;
}

/**
 * Allocate memory once for several disparate objects.
 *
 * This function adds iovec-like semantics (e.g. readv()) to rte_malloc().
 * Memory is allocated once for several contiguous objects of nonuniform
 * sizes and alignment constraints.
 *
 * Each entry of @p vec describes the size, alignment constraint and
 * provides a buffer address where the resulting object pointer must be
 * stored.
 *
 * The buffer of the first entry is guaranteed to point to the beginning of
 * the allocated region and is safe to use with rte_free().
 *
 * NULL buffers are silently ignored.
 *
 * Providing a NULL buffer in the first entry prevents this function from
 * allocating any memory but has otherwise no effect on its behavior. In
 * this case, the contents of remaining non-NULL buffers are updated with
 * addresses relative to zero (i.e. offsets that would have been used during
 * the allocation).
 *
 * @param[in] type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param[in, out] vec
 *   Description of objects to allocate memory for.
 * @param cnt
 *   Number of entries in @p vec.
 *
 * @return
 *   Size in bytes of the allocated region including any padding. In case of
 *   error, rte_errno is set, 0 is returned and NULL is stored in the
 *   non-NULL buffers pointed by @p vec.
 *
 * @see struct mlx4_malloc_vec
 * @see rte_malloc()
 */
size_t
mlx4_mallocv(const char *type, const struct mlx4_malloc_vec *vec,
	     unsigned int cnt)
{
	return mlx4_mallocv_inline(type, vec, cnt, 0, SOCKET_ID_ANY);
}

/**
 * Combines the semantics of mlx4_mallocv() with those of rte_zmalloc().
 *
 * @see mlx4_mallocv()
 * @see rte_zmalloc()
 */
size_t
mlx4_zmallocv(const char *type, const struct mlx4_malloc_vec *vec,
	      unsigned int cnt)
{
	return mlx4_mallocv_inline(type, vec, cnt, 1, SOCKET_ID_ANY);
}

/**
 * Socket-aware version of mlx4_mallocv().
 *
 * This function takes one additional parameter.
 *
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this
 *   function will behave the same as mlx4_mallocv().
 *
 * @see mlx4_mallocv()
 * @see rte_malloc_socket()
 */
size_t
mlx4_mallocv_socket(const char *type, const struct mlx4_malloc_vec *vec,
		    unsigned int cnt, int socket)
{
	return mlx4_mallocv_inline(type, vec, cnt, 0, socket);
}

/**
 * Combines the semantics of mlx4_mallocv_socket() with those of
 * mlx4_zmalloc_socket().
 *
 * @see mlx4_mallocv_socket()
 * @see rte_zmalloc_socket()
 */
size_t
mlx4_zmallocv_socket(const char *type, const struct mlx4_malloc_vec *vec,
		     unsigned int cnt, int socket)
{
	return mlx4_mallocv_inline(type, vec, cnt, 1, socket);
}
