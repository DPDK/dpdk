/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Ericsson AB
 */

#ifndef RTE_LCORE_VAR_H
#define RTE_LCORE_VAR_H

/**
 * @file
 *
 * Lcore variables
 *
 * This API provides a mechanism to create and access per-lcore id
 * variables in a space- and cycle-efficient manner.
 *
 * Please refer to the lcore variables' programmer's guide
 * for an overview of this API and its implementation.
 *
 * EXPERIMENTAL: this API may change, or be removed, without prior notice.
 */

#include <stddef.h>
#include <stdalign.h>

#include <rte_common.h>
#include <rte_config.h>
#include <rte_debug.h>
#include <rte_lcore.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Given the lcore variable type, produces the type of the lcore variable handle.
 */
#define RTE_LCORE_VAR_HANDLE_TYPE(type)		\
	type *

/**
 * Define an lcore variable handle.
 *
 * This macro defines a variable which is used as a handle
 * to access the various instances of a per-lcore id variable.
 *
 * This macro clarifies that the declaration is an lcore handle,
 * not a regular pointer.
 *
 * Add @b static as a prefix in case the lcore variable
 * is only to be accessed from a particular translation unit.
 */
#define RTE_LCORE_VAR_HANDLE(type, name)	\
	RTE_LCORE_VAR_HANDLE_TYPE(type) name

/**
 * Allocate space for an lcore variable, and initialize its handle.
 *
 * The values of the lcore variable are initialized to zero.
 */
#define RTE_LCORE_VAR_ALLOC_SIZE_ALIGN(handle, size, align)	\
	handle = rte_lcore_var_alloc(size, align)

/**
 * Allocate space for an lcore variable, and initialize its handle,
 * with values aligned for any type of object.
 *
 * The values of the lcore variable are initialized to zero.
 */
#define RTE_LCORE_VAR_ALLOC_SIZE(handle, size)	\
	RTE_LCORE_VAR_ALLOC_SIZE_ALIGN(handle, size, 0)

/**
 * Allocate space for an lcore variable of the size and alignment requirements
 * suggested by the handle pointer type, and initialize its handle.
 *
 * The values of the lcore variable are initialized to zero.
 */
#define RTE_LCORE_VAR_ALLOC(handle)					\
	RTE_LCORE_VAR_ALLOC_SIZE_ALIGN(handle, sizeof(*(handle)),	\
				       alignof(typeof(*(handle))))

/**
 * Allocate an explicitly-sized, explicitly-aligned lcore variable
 * by means of a @ref RTE_INIT constructor.
 *
 * The values of the lcore variable are initialized to zero.
 */
#define RTE_LCORE_VAR_INIT_SIZE_ALIGN(name, size, align)		\
	RTE_INIT(rte_lcore_var_init_ ## name)				\
	{								\
		RTE_LCORE_VAR_ALLOC_SIZE_ALIGN(name, size, align);	\
	}

/**
 * Allocate an explicitly-sized lcore variable
 * by means of a @ref RTE_INIT constructor.
 *
 * The values of the lcore variable are initialized to zero.
 */
#define RTE_LCORE_VAR_INIT_SIZE(name, size)		\
	RTE_LCORE_VAR_INIT_SIZE_ALIGN(name, size, 0)

/**
 * Allocate an lcore variable by means of a @ref RTE_INIT constructor.
 *
 * The values of the lcore variable are initialized to zero.
 */
#define RTE_LCORE_VAR_INIT(name)					\
	RTE_INIT(rte_lcore_var_init_ ## name)				\
	{								\
		RTE_LCORE_VAR_ALLOC(name);				\
	}

/**
 * Get void pointer to lcore variable instance with the specified lcore id.
 *
 * @param lcore_id
 *   The lcore id specifying which of the @c RTE_MAX_LCORE value
 *   instances should be accessed. The lcore id need not be valid
 *   (e.g., may be @ref LCORE_ID_ANY), but in such a case,
 *   the pointer is also not valid (and thus should not be dereferenced).
 * @param handle
 *   The lcore variable handle.
 */
/* access function 8< */
static inline void *
rte_lcore_var_lcore(unsigned int lcore_id, void *handle)
{
	RTE_ASSERT(handle != NULL);
	return RTE_PTR_ADD(handle, lcore_id * RTE_MAX_LCORE_VAR);
}
/* >8 end of access function */

/**
 * Get pointer to lcore variable instance with the specified lcore id.
 *
 * @param lcore_id
 *   The lcore id specifying which of the @c RTE_MAX_LCORE value
 *   instances should be accessed. The lcore id need not be valid
 *   (e.g., may be @ref LCORE_ID_ANY), but in such a case,
 *   the pointer is also not valid (and thus should not be dereferenced).
 * @param handle
 *   The lcore variable handle.
 */
#define RTE_LCORE_VAR_LCORE(lcore_id, handle)			\
	((typeof(handle))rte_lcore_var_lcore(lcore_id, handle))

/**
 * Get pointer to lcore variable instance of the current thread.
 *
 * May only be used by EAL threads and registered non-EAL threads.
 */
#define RTE_LCORE_VAR(handle)				\
	RTE_LCORE_VAR_LCORE(rte_lcore_id(), handle)

/**
 * Iterate over each lcore id's value for an lcore variable.
 *
 * @param lcore_id
 *   An <code>unsigned int</code> variable successively set to the
 *   lcore id of every valid lcore id (up to @c RTE_MAX_LCORE).
 * @param value
 *   A pointer variable successively set to point to lcore variable
 *   value instance of the current lcore id being processed.
 * @param handle
 *   The lcore variable handle.
 */
#define RTE_LCORE_VAR_FOREACH(lcore_id, value, handle)			\
	for ((lcore_id) =						\
		     (((value) = RTE_LCORE_VAR_LCORE(0, handle)), 0); \
	     (lcore_id) < RTE_MAX_LCORE;				\
	     (lcore_id)++, (value) = RTE_LCORE_VAR_LCORE(lcore_id, \
							       handle))

/**
 * Allocate space in the per-lcore id buffers for an lcore variable.
 *
 * The pointer returned is only an opaque identifier of the variable.
 * To get an actual pointer to a particular instance of the variable,
 * use @ref RTE_LCORE_VAR or @ref RTE_LCORE_VAR_LCORE.
 *
 * The lcore variable values' memory is set to zero.
 *
 * The allocation is always successful,
 * barring a fatal exhaustion of the per-lcore id buffer space.
 *
 * rte_lcore_var_alloc() is not multi-thread safe.
 *
 * The allocated memory cannot be freed.
 *
 * @param size
 *   The size (in bytes) of the variable's per-lcore id value. Must be > 0.
 * @param align
 *   If 0, the values will be suitably aligned for any kind of type
 *   (i.e., alignof(max_align_t)). Otherwise, the values will be aligned
 *   on a multiple of *align*, which must be a power of 2
 *   and equal or less than @c RTE_CACHE_LINE_SIZE.
 * @return
 *   The variable's handle, stored in a void pointer value.
 *   The value is always non-NULL.
 */
__rte_experimental
void *
rte_lcore_var_alloc(size_t size, size_t align)
	__rte_alloc_size(1) __rte_alloc_align(2);

#ifdef __cplusplus
}
#endif

#endif /* RTE_LCORE_VAR_H */
