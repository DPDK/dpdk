/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_MEMPOOL_H_
#define _RTE_MEMPOOL_H_

/**
 * @file
 * RTE Mempool.
 *
 * A memory pool is an allocator of fixed-size object. It is
 * identified by its name, and uses a ring to store free objects. It
 * provides some other optional services, like a per-core object
 * cache, and an alignment helper to ensure that objects are padded
 * to spread them equally on all RAM channels, ranks, and so on.
 *
 * Objects owned by a mempool should never be added in another
 * mempool. When an object is freed using rte_mempool_put() or
 * equivalent, the object data is not modified; the user can save some
 * meta-data in the object data and retrieve them when allocating a
 * new object.
 *
 * Note: the mempool implementation is not preemptable. A lcore must
 * not be interrupted by another task that uses the same mempool
 * (because it uses a ring which is not preemptable). Also, mempool
 * functions must not be used outside the DPDK environment: for
 * example, in linuxapp environment, a thread that is not created by
 * the EAL must not use mempools. This is due to the per-lcore cache
 * that won't work as rte_lcore_id() will not return a correct value.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_debug.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_MEMPOOL_HEADER_COOKIE1  0xbadbadbadadd2e55ULL /**< Header cookie. */
#define RTE_MEMPOOL_HEADER_COOKIE2  0xf2eef2eedadd2e55ULL /**< Header cookie. */
#define RTE_MEMPOOL_TRAILER_COOKIE  0xadd2e55badbadbadULL /**< Trailer cookie.*/

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
/**
 * A structure that stores the mempool statistics (per-lcore).
 */
struct rte_mempool_debug_stats {
	uint64_t put_bulk;         /**< Number of puts. */
	uint64_t put_objs;         /**< Number of objects successfully put. */
	uint64_t get_success_bulk; /**< Successful allocation number. */
	uint64_t get_success_objs; /**< Objects successfully allocated. */
	uint64_t get_fail_bulk;    /**< Failed allocation number. */
	uint64_t get_fail_objs;    /**< Objects that failed to be allocated. */
} __rte_cache_aligned;
#endif

#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
/**
 * A structure that stores a per-core object cache.
 */
struct rte_mempool_cache {
	unsigned len; /**< Cache len */
	/*
	 * Cache is allocated to this size to allow it to overflow in certain
	 * cases to avoid needless emptying of cache.
	 */
	void *objs[RTE_MEMPOOL_CACHE_MAX_SIZE * 3]; /**< Cache objects */
} __rte_cache_aligned;
#endif /* RTE_MEMPOOL_CACHE_MAX_SIZE > 0 */

struct rte_mempool_objsz {
	uint32_t elt_size;     /**< Size of an element. */
	uint32_t header_size;  /**< Size of header (before elt). */
	uint32_t trailer_size; /**< Size of trailer (after elt). */
	uint32_t total_size;
	/**< Total size of an object (header + elt + trailer). */
};

#define RTE_MEMPOOL_NAMESIZE 32 /**< Maximum length of a memory pool. */
#define RTE_MEMPOOL_MZ_PREFIX "MP_"

/* "MP_<name>" */
#define	RTE_MEMPOOL_MZ_FORMAT	RTE_MEMPOOL_MZ_PREFIX "%s"

#ifdef RTE_LIBRTE_XEN_DOM0

/* "<name>_MP_elt" */
#define	RTE_MEMPOOL_OBJ_NAME	"%s_" RTE_MEMPOOL_MZ_PREFIX "elt"

#else

#define	RTE_MEMPOOL_OBJ_NAME	RTE_MEMPOOL_MZ_FORMAT

#endif /* RTE_LIBRTE_XEN_DOM0 */

#define	MEMPOOL_PG_SHIFT_MAX	(sizeof(uintptr_t) * CHAR_BIT - 1)

/** Mempool over one chunk of physically continuous memory */
#define	MEMPOOL_PG_NUM_DEFAULT	1

/**
 * The RTE mempool structure.
 */
struct rte_mempool {
	TAILQ_ENTRY(rte_mempool) next;   /**< Next in list. */

	char name[RTE_MEMPOOL_NAMESIZE]; /**< Name of mempool. */
	struct rte_ring *ring;           /**< Ring to store objects. */
	phys_addr_t phys_addr;           /**< Phys. addr. of mempool struct. */
	int flags;                       /**< Flags of the mempool. */
	uint32_t size;                   /**< Size of the mempool. */
	uint32_t cache_size;             /**< Size of per-lcore local cache. */
	uint32_t cache_flushthresh;
	/**< Threshold before we flush excess elements. */

	uint32_t elt_size;               /**< Size of an element. */
	uint32_t header_size;            /**< Size of header (before elt). */
	uint32_t trailer_size;           /**< Size of trailer (after elt). */

	unsigned private_data_size;      /**< Size of private data. */

#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	/** Per-lcore local cache. */
	struct rte_mempool_cache local_cache[RTE_MAX_LCORE];
#endif

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	/** Per-lcore statistics. */
	struct rte_mempool_debug_stats stats[RTE_MAX_LCORE];
#endif

	/* Address translation support, starts from next cache line. */

	/** Number of elements in the elt_pa array. */
	uint32_t    pg_num __rte_cache_aligned;
	uint32_t    pg_shift;     /**< LOG2 of the physical pages. */
	uintptr_t   pg_mask;      /**< physical page mask value. */
	uintptr_t   elt_va_start;
	/**< Virtual address of the first mempool object. */
	uintptr_t   elt_va_end;
	/**< Virtual address of the <size + 1> mempool object. */
	phys_addr_t elt_pa[MEMPOOL_PG_NUM_DEFAULT];
	/**< Array of physical pages addresses for the mempool objects buffer. */

}  __rte_cache_aligned;

#define MEMPOOL_F_NO_SPREAD      0x0001 /**< Do not spread in memory. */
#define MEMPOOL_F_NO_CACHE_ALIGN 0x0002 /**< Do not align objs on cache lines.*/
#define MEMPOOL_F_SP_PUT         0x0004 /**< Default put is "single-producer".*/
#define MEMPOOL_F_SC_GET         0x0008 /**< Default get is "single-consumer".*/

/**
 * @internal When debug is enabled, store some statistics.
 * @param mp
 *   Pointer to the memory pool.
 * @param name
 *   Name of the statistics field to increment in the memory pool.
 * @param n
 *   Number to add to the object-oriented statistics.
 */
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
#define __MEMPOOL_STAT_ADD(mp, name, n) do {			\
		unsigned __lcore_id = rte_lcore_id();		\
		mp->stats[__lcore_id].name##_objs += n;		\
		mp->stats[__lcore_id].name##_bulk += 1;		\
	} while(0)
#else
#define __MEMPOOL_STAT_ADD(mp, name, n) do {} while(0)
#endif

/**
 * Calculates size of the mempool header.
 * @param mp
 *   Pointer to the memory pool.
 * @param pgn
 *   Number of page used to store mempool objects.
 */
#define	MEMPOOL_HEADER_SIZE(mp, pgn)	(sizeof(*(mp)) + \
	RTE_ALIGN_CEIL(((pgn) - RTE_DIM((mp)->elt_pa)) * \
	sizeof ((mp)->elt_pa[0]), CACHE_LINE_SIZE))

/**
 * Returns TRUE if whole mempool is allocated in one contiguous block of memory.
 */
#define	MEMPOOL_IS_CONTIG(mp)                      \
	((mp)->pg_num == MEMPOOL_PG_NUM_DEFAULT && \
	(mp)->phys_addr == (mp)->elt_pa[0])

/**
 * @internal Get a pointer to a mempool pointer in the object header.
 * @param obj
 *   Pointer to object.
 * @return
 *   The pointer to the mempool from which the object was allocated.
 */
static inline struct rte_mempool **__mempool_from_obj(void *obj)
{
	struct rte_mempool **mpp;
	unsigned off;

	off = sizeof(struct rte_mempool *);
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	off += sizeof(uint64_t);
#endif
	mpp = (struct rte_mempool **)((char *)obj - off);
	return mpp;
}

/**
 * Return a pointer to the mempool owning this object.
 *
 * @param obj
 *   An object that is owned by a pool. If this is not the case,
 *   the behavior is undefined.
 * @return
 *   A pointer to the mempool structure.
 */
static inline const struct rte_mempool *rte_mempool_from_obj(void *obj)
{
	struct rte_mempool * const *mpp;
	mpp = __mempool_from_obj(obj);
	return *mpp;
}

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
/* get header cookie value */
static inline uint64_t __mempool_read_header_cookie(const void *obj)
{
	return *(const uint64_t *)((const char *)obj - sizeof(uint64_t));
}

/* get trailer cookie value */
static inline uint64_t __mempool_read_trailer_cookie(void *obj)
{
	struct rte_mempool **mpp = __mempool_from_obj(obj);
	return *(uint64_t *)((char *)obj + (*mpp)->elt_size);
}

/* write header cookie value */
static inline void __mempool_write_header_cookie(void *obj, int free)
{
	uint64_t *cookie_p;
	cookie_p = (uint64_t *)((char *)obj - sizeof(uint64_t));
	if (free == 0)
		*cookie_p = RTE_MEMPOOL_HEADER_COOKIE1;
	else
		*cookie_p = RTE_MEMPOOL_HEADER_COOKIE2;

}

/* write trailer cookie value */
static inline void __mempool_write_trailer_cookie(void *obj)
{
	uint64_t *cookie_p;
	struct rte_mempool **mpp = __mempool_from_obj(obj);
	cookie_p = (uint64_t *)((char *)obj + (*mpp)->elt_size);
	*cookie_p = RTE_MEMPOOL_TRAILER_COOKIE;
}
#endif /* RTE_LIBRTE_MEMPOOL_DEBUG */

/**
 * @internal Check and update cookies or panic.
 *
 * @param mp
 *   Pointer to the memory pool.
 * @param obj_table_const
 *   Pointer to a table of void * pointers (objects).
 * @param n
 *   Index of object in object table.
 * @param free
 *   - 0: object is supposed to be allocated, mark it as free
 *   - 1: object is supposed to be free, mark it as allocated
 *   - 2: just check that cookie is valid (free or allocated)
 */
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
#ifndef __INTEL_COMPILER
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
static inline void __mempool_check_cookies(const struct rte_mempool *mp,
					   void * const *obj_table_const,
					   unsigned n, int free)
{
	uint64_t cookie;
	void *tmp;
	void *obj;
	void **obj_table;

	/* Force to drop the "const" attribute. This is done only when
	 * DEBUG is enabled */
	tmp = (void *) obj_table_const;
	obj_table = (void **) tmp;

	while (n--) {
		obj = obj_table[n];

		if (rte_mempool_from_obj(obj) != mp)
			rte_panic("MEMPOOL: object is owned by another "
				  "mempool\n");

		cookie = __mempool_read_header_cookie(obj);

		if (free == 0) {
			if (cookie != RTE_MEMPOOL_HEADER_COOKIE1) {
				rte_log_set_history(0);
				RTE_LOG(CRIT, MEMPOOL,
					"obj=%p, mempool=%p, cookie=%"PRIx64"\n",
					obj, mp, cookie);
				rte_panic("MEMPOOL: bad header cookie (put)\n");
			}
			__mempool_write_header_cookie(obj, 1);
		}
		else if (free == 1) {
			if (cookie != RTE_MEMPOOL_HEADER_COOKIE2) {
				rte_log_set_history(0);
				RTE_LOG(CRIT, MEMPOOL,
					"obj=%p, mempool=%p, cookie=%"PRIx64"\n",
					obj, mp, cookie);
				rte_panic("MEMPOOL: bad header cookie (get)\n");
			}
			__mempool_write_header_cookie(obj, 0);
		}
		else if (free == 2) {
			if (cookie != RTE_MEMPOOL_HEADER_COOKIE1 &&
			    cookie != RTE_MEMPOOL_HEADER_COOKIE2) {
				rte_log_set_history(0);
				RTE_LOG(CRIT, MEMPOOL,
					"obj=%p, mempool=%p, cookie=%"PRIx64"\n",
					obj, mp, cookie);
				rte_panic("MEMPOOL: bad header cookie (audit)\n");
			}
		}
		cookie = __mempool_read_trailer_cookie(obj);
		if (cookie != RTE_MEMPOOL_TRAILER_COOKIE) {
			rte_log_set_history(0);
			RTE_LOG(CRIT, MEMPOOL,
				"obj=%p, mempool=%p, cookie=%"PRIx64"\n",
				obj, mp, cookie);
			rte_panic("MEMPOOL: bad trailer cookie\n");
		}
	}
}
#ifndef __INTEL_COMPILER
#pragma GCC pop_options
#endif
#else
#define __mempool_check_cookies(mp, obj_table_const, n, free) do {} while(0)
#endif /* RTE_LIBRTE_MEMPOOL_DEBUG */

/**
 * An mempool's object iterator callback function.
 */
typedef void (*rte_mempool_obj_iter_t)(void * /*obj_iter_arg*/,
	void * /*obj_start*/,
	void * /*obj_end*/,
	uint32_t /*obj_index */);

/*
 * Iterates across objects of the given size and alignment in the
 * provided chunk of memory. The given memory buffer can consist of
 * disjoint physical pages.
 * For each object calls the provided callback (if any).
 * Used to populate mempool, walk through all elements of the mempool,
 * estimate how many elements of the given size could be created in the given
 * memory buffer.
 * @param vaddr
 *   Virtual address of the memory buffer.
 * @param elt_num
 *   Maximum number of objects to iterate through.
 * @param elt_sz
 *   Size of each object.
 * @param paddr
 *   Array of phyiscall addresses of the pages that comprises given memory
 *   buffer.
 * @param pg_num
 *   Number of elements in the paddr array.
 * @param pg_shift
 *   LOG2 of the physical pages size.
 * @param obj_iter
 *   Object iterator callback function (could be NULL).
 * @param obj_iter_arg
 *   User defined Prameter for the object iterator callback function.
 *
 * @return
 *   Number of objects iterated through.
 */

uint32_t rte_mempool_obj_iter(void *vaddr,
	uint32_t elt_num, size_t elt_sz, size_t align,
	const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift,
	rte_mempool_obj_iter_t obj_iter, void *obj_iter_arg);

/**
 * An object constructor callback function for mempool.
 *
 * Arguments are the mempool, the opaque pointer given by the user in
 * rte_mempool_create(), the pointer to the element and the index of
 * the element in the pool.
 */
typedef void (rte_mempool_obj_ctor_t)(struct rte_mempool *, void *,
				      void *, unsigned);

/**
 * A mempool constructor callback function.
 *
 * Arguments are the mempool and the opaque pointer given by the user in
 * rte_mempool_create().
 */
typedef void (rte_mempool_ctor_t)(struct rte_mempool *, void *);

/**
 * Creates a new mempool named *name* in memory.
 *
 * This function uses ``memzone_reserve()`` to allocate memory. The
 * pool contains n elements of elt_size. Its size is set to n.
 * All elements of the mempool are allocated together with the mempool header,
 * in one physically continuous chunk of memory.
 *
 * @param name
 *   The name of the mempool.
 * @param n
 *   The number of elements in the mempool. The optimum size (in terms of
 *   memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param elt_size
 *   The size of each element.
 * @param cache_size
 *   If cache_size is non-zero, the rte_mempool library will try to
 *   limit the accesses to the common lockless pool, by maintaining a
 *   per-lcore object cache. This argument must be lower or equal to
 *   CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE. It is advised to choose
 *   cache_size to have "n modulo cache_size == 0": if this is
 *   not the case, some elements will always stay in the pool and will
 *   never be used. The access to the per-lcore table is of course
 *   faster than the multi-producer/consumer pool. The cache can be
 *   disabled if the cache_size argument is set to 0; it can be useful to
 *   avoid losing objects in cache. Note that even if not used, the
 *   memory space for cache is always reserved in a mempool structure,
 *   except if CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE is set to 0.
 * @param private_data_size
 *   The size of the private data appended after the mempool
 *   structure. This is useful for storing some private data after the
 *   mempool structure, as is done for rte_mbuf_pool for example.
 * @param mp_init
 *   A function pointer that is called for initialization of the pool,
 *   before object initialization. The user can initialize the private
 *   data in this function if needed. This parameter can be NULL if
 *   not needed.
 * @param mp_init_arg
 *   An opaque pointer to data that can be used in the mempool
 *   constructor function.
 * @param obj_init
 *   A function pointer that is called for each object at
 *   initialization of the pool. The user can set some meta data in
 *   objects if needed. This parameter can be NULL if not needed.
 *   The obj_init() function takes the mempool pointer, the init_arg,
 *   the object pointer and the object number as parameters.
 * @param obj_init_arg
 *   An opaque pointer to data that can be used as an argument for
 *   each call to the object constructor function.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in the case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The *flags* arguments is an OR of following flags:
 *   - MEMPOOL_F_NO_SPREAD: By default, objects addresses are spread
 *     between channels in RAM: the pool allocator will add padding
 *     between objects depending on the hardware configuration. See
 *     Memory alignment constraints for details. If this flag is set,
 *     the allocator will just align them to a cache line.
 *   - MEMPOOL_F_NO_CACHE_ALIGN: By default, the returned objects are
 *     cache-aligned. This flag removes this constraint, and no
 *     padding will be present between objects. This flag implies
 *     MEMPOOL_F_NO_SPREAD.
 *   - MEMPOOL_F_SP_PUT: If this flag is set, the default behavior
 *     when using rte_mempool_put() or rte_mempool_put_bulk() is
 *     "single-producer". Otherwise, it is "multi-producers".
 *   - MEMPOOL_F_SC_GET: If this flag is set, the default behavior
 *     when using rte_mempool_get() or rte_mempool_get_bulk() is
 *     "single-consumer". Otherwise, it is "multi-consumers".
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - E_RTE_NO_TAILQ - no tailq list could be got for the ring or mempool list
 *    - EINVAL - cache size provided is too large
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_mempool *
rte_mempool_create(const char *name, unsigned n, unsigned elt_size,
		   unsigned cache_size, unsigned private_data_size,
		   rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		   rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		   int socket_id, unsigned flags);

/**
 * Creates a new mempool named *name* in memory.
 *
 * This function uses ``memzone_reserve()`` to allocate memory. The
 * pool contains n elements of elt_size. Its size is set to n.
 * Depending on the input parameters, mempool elements can be either allocated
 * together with the mempool header, or an externally provided memory buffer
 * could be used to store mempool objects. In later case, that external
 * memory buffer can consist of set of disjoint phyiscal pages.
 *
 * @param name
 *   The name of the mempool.
 * @param n
 *   The number of elements in the mempool. The optimum size (in terms of
 *   memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param elt_size
 *   The size of each element.
 * @param cache_size
 *   If cache_size is non-zero, the rte_mempool library will try to
 *   limit the accesses to the common lockless pool, by maintaining a
 *   per-lcore object cache. This argument must be lower or equal to
 *   CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE. It is advised to choose
 *   cache_size to have "n modulo cache_size == 0": if this is
 *   not the case, some elements will always stay in the pool and will
 *   never be used. The access to the per-lcore table is of course
 *   faster than the multi-producer/consumer pool. The cache can be
 *   disabled if the cache_size argument is set to 0; it can be useful to
 *   avoid losing objects in cache. Note that even if not used, the
 *   memory space for cache is always reserved in a mempool structure,
 *   except if CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE is set to 0.
 * @param private_data_size
 *   The size of the private data appended after the mempool
 *   structure. This is useful for storing some private data after the
 *   mempool structure, as is done for rte_mbuf_pool for example.
 * @param mp_init
 *   A function pointer that is called for initialization of the pool,
 *   before object initialization. The user can initialize the private
 *   data in this function if needed. This parameter can be NULL if
 *   not needed.
 * @param mp_init_arg
 *   An opaque pointer to data that can be used in the mempool
 *   constructor function.
 * @param obj_init
 *   A function pointer that is called for each object at
 *   initialization of the pool. The user can set some meta data in
 *   objects if needed. This parameter can be NULL if not needed.
 *   The obj_init() function takes the mempool pointer, the init_arg,
 *   the object pointer and the object number as parameters.
 * @param obj_init_arg
 *   An opaque pointer to data that can be used as an argument for
 *   each call to the object constructor function.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in the case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The *flags* arguments is an OR of following flags:
 *   - MEMPOOL_F_NO_SPREAD: By default, objects addresses are spread
 *     between channels in RAM: the pool allocator will add padding
 *     between objects depending on the hardware configuration. See
 *     Memory alignment constraints for details. If this flag is set,
 *     the allocator will just align them to a cache line.
 *   - MEMPOOL_F_NO_CACHE_ALIGN: By default, the returned objects are
 *     cache-aligned. This flag removes this constraint, and no
 *     padding will be present between objects. This flag implies
 *     MEMPOOL_F_NO_SPREAD.
 *   - MEMPOOL_F_SP_PUT: If this flag is set, the default behavior
 *     when using rte_mempool_put() or rte_mempool_put_bulk() is
 *     "single-producer". Otherwise, it is "multi-producers".
 *   - MEMPOOL_F_SC_GET: If this flag is set, the default behavior
 *     when using rte_mempool_get() or rte_mempool_get_bulk() is
 *     "single-consumer". Otherwise, it is "multi-consumers".
 * @param vaddr
 *   Virtual address of the externally allocated memory buffer.
 *   Will be used to store mempool objects.
 * @param paddr
 *   Array of phyiscall addresses of the pages that comprises given memory
 *   buffer.
 * @param pg_num
 *   Number of elements in the paddr array.
 * @param pg_shift
 *   LOG2 of the physical pages size.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - E_RTE_NO_TAILQ - no tailq list could be got for the ring or mempool list
 *    - EINVAL - cache size provided is too large
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_mempool *
rte_mempool_xmem_create(const char *name, unsigned n, unsigned elt_size,
		unsigned cache_size, unsigned private_data_size,
		rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		int socket_id, unsigned flags, void *vaddr,
		const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift);

#ifdef RTE_LIBRTE_XEN_DOM0
/**
 * Creates a new mempool named *name* in memory on Xen Dom0.
 *
 * This function uses ``rte_mempool_xmem_create()`` to allocate memory. The
 * pool contains n elements of elt_size. Its size is set to n.
 * All elements of the mempool are allocated together with the mempool header,
 * and memory buffer can consist of set of disjoint phyiscal pages.
 *
 * @param name
 *   The name of the mempool.
 * @param n
 *   The number of elements in the mempool. The optimum size (in terms of
 *   memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param elt_size
 *   The size of each element.
 * @param cache_size
 *   If cache_size is non-zero, the rte_mempool library will try to
 *   limit the accesses to the common lockless pool, by maintaining a
 *   per-lcore object cache. This argument must be lower or equal to
 *   CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE. It is advised to choose
 *   cache_size to have "n modulo cache_size == 0": if this is
 *   not the case, some elements will always stay in the pool and will
 *   never be used. The access to the per-lcore table is of course
 *   faster than the multi-producer/consumer pool. The cache can be
 *   disabled if the cache_size argument is set to 0; it can be useful to
 *   avoid losing objects in cache. Note that even if not used, the
 *   memory space for cache is always reserved in a mempool structure,
 *   except if CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE is set to 0.
 * @param private_data_size
 *   The size of the private data appended after the mempool
 *   structure. This is useful for storing some private data after the
 *   mempool structure, as is done for rte_mbuf_pool for example.
 * @param mp_init
 *   A function pointer that is called for initialization of the pool,
 *   before object initialization. The user can initialize the private
 *   data in this function if needed. This parameter can be NULL if
 *   not needed.
 * @param mp_init_arg
 *   An opaque pointer to data that can be used in the mempool
 *   constructor function.
 * @param obj_init
 *   A function pointer that is called for each object at
 *   initialization of the pool. The user can set some meta data in
 *   objects if needed. This parameter can be NULL if not needed.
 *   The obj_init() function takes the mempool pointer, the init_arg,
 *   the object pointer and the object number as parameters.
 * @param obj_init_arg
 *   An opaque pointer to data that can be used as an argument for
 *   each call to the object constructor function.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in the case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The *flags* arguments is an OR of following flags:
 *   - MEMPOOL_F_NO_SPREAD: By default, objects addresses are spread
 *     between channels in RAM: the pool allocator will add padding
 *     between objects depending on the hardware configuration. See
 *     Memory alignment constraints for details. If this flag is set,
 *     the allocator will just align them to a cache line.
 *   - MEMPOOL_F_NO_CACHE_ALIGN: By default, the returned objects are
 *     cache-aligned. This flag removes this constraint, and no
 *     padding will be present between objects. This flag implies
 *     MEMPOOL_F_NO_SPREAD.
 *   - MEMPOOL_F_SP_PUT: If this flag is set, the default behavior
 *     when using rte_mempool_put() or rte_mempool_put_bulk() is
 *     "single-producer". Otherwise, it is "multi-producers".
 *   - MEMPOOL_F_SC_GET: If this flag is set, the default behavior
 *     when using rte_mempool_get() or rte_mempool_get_bulk() is
 *     "single-consumer". Otherwise, it is "multi-consumers".
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - E_RTE_NO_TAILQ - no tailq list could be got for the ring or mempool list
 *    - EINVAL - cache size provided is too large
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_mempool *
rte_dom0_mempool_create(const char *name, unsigned n, unsigned elt_size,
		unsigned cache_size, unsigned private_data_size,
		rte_mempool_ctor_t *mp_init, void *mp_init_arg,
		rte_mempool_obj_ctor_t *obj_init, void *obj_init_arg,
		int socket_id, unsigned flags);
#endif

/**
 * Dump the status of the mempool to the console.
 *
 * @param f
 *   A pointer to a file for output
 * @param mp
 *   A pointer to the mempool structure.
 */
void rte_mempool_dump(FILE *f, const struct rte_mempool *mp);

/**
 * @internal Put several objects back in the mempool; used internally.
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to store back in the mempool, must be strictly
 *   positive.
 * @param is_mp
 *   Mono-producer (0) or multi-producers (1).
 */
static inline void __attribute__((always_inline))
__mempool_put_bulk(struct rte_mempool *mp, void * const *obj_table,
		    unsigned n, int is_mp)
{
#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	struct rte_mempool_cache *cache;
	uint32_t index;
	void **cache_objs;
	unsigned lcore_id = rte_lcore_id();
	uint32_t cache_size = mp->cache_size;
	uint32_t flushthresh = mp->cache_flushthresh;
#endif /* RTE_MEMPOOL_CACHE_MAX_SIZE > 0 */

	/* increment stat now, adding in mempool always success */
	__MEMPOOL_STAT_ADD(mp, put, n);

#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	/* cache is not enabled or single producer */
	if (unlikely(cache_size == 0 || is_mp == 0))
		goto ring_enqueue;

	/* Go straight to ring if put would overflow mem allocated for cache */
	if (unlikely(n > RTE_MEMPOOL_CACHE_MAX_SIZE))
		goto ring_enqueue;

	cache = &mp->local_cache[lcore_id];
	cache_objs = &cache->objs[cache->len];

	/*
	 * The cache follows the following algorithm
	 *   1. Add the objects to the cache
	 *   2. Anything greater than the cache min value (if it crosses the
	 *   cache flush threshold) is flushed to the ring.
	 */

	/* Add elements back into the cache */
	for (index = 0; index < n; ++index, obj_table++)
		cache_objs[index] = *obj_table;

	cache->len += n;

	if (cache->len >= flushthresh) {
		rte_ring_mp_enqueue_bulk(mp->ring, &cache->objs[cache_size],
				cache->len - cache_size);
		cache->len = cache_size;
	}

	return;

ring_enqueue:
#endif /* RTE_MEMPOOL_CACHE_MAX_SIZE > 0 */

	/* push remaining objects in ring */
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	if (is_mp) {
		if (rte_ring_mp_enqueue_bulk(mp->ring, obj_table, n) < 0)
			rte_panic("cannot put objects in mempool\n");
	}
	else {
		if (rte_ring_sp_enqueue_bulk(mp->ring, obj_table, n) < 0)
			rte_panic("cannot put objects in mempool\n");
	}
#else
	if (is_mp)
		rte_ring_mp_enqueue_bulk(mp->ring, obj_table, n);
	else
		rte_ring_sp_enqueue_bulk(mp->ring, obj_table, n);
#endif
}


/**
 * Put several objects back in the mempool (multi-producers safe).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the mempool from the obj_table.
 */
static inline void __attribute__((always_inline))
rte_mempool_mp_put_bulk(struct rte_mempool *mp, void * const *obj_table,
			unsigned n)
{
	__mempool_check_cookies(mp, obj_table, n, 0);
	__mempool_put_bulk(mp, obj_table, n, 1);
}

/**
 * Put several objects back in the mempool (NOT multi-producers safe).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the mempool from obj_table.
 */
static inline void
rte_mempool_sp_put_bulk(struct rte_mempool *mp, void * const *obj_table,
			unsigned n)
{
	__mempool_check_cookies(mp, obj_table, n, 0);
	__mempool_put_bulk(mp, obj_table, n, 0);
}

/**
 * Put several objects back in the mempool.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * mempool creation time (see flags).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the mempool from obj_table.
 */
static inline void __attribute__((always_inline))
rte_mempool_put_bulk(struct rte_mempool *mp, void * const *obj_table,
		     unsigned n)
{
	__mempool_check_cookies(mp, obj_table, n, 0);
	__mempool_put_bulk(mp, obj_table, n, !(mp->flags & MEMPOOL_F_SP_PUT));
}

/**
 * Put one object in the mempool (multi-producers safe).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj
 *   A pointer to the object to be added.
 */
static inline void __attribute__((always_inline))
rte_mempool_mp_put(struct rte_mempool *mp, void *obj)
{
	rte_mempool_mp_put_bulk(mp, &obj, 1);
}

/**
 * Put one object back in the mempool (NOT multi-producers safe).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj
 *   A pointer to the object to be added.
 */
static inline void __attribute__((always_inline))
rte_mempool_sp_put(struct rte_mempool *mp, void *obj)
{
	rte_mempool_sp_put_bulk(mp, &obj, 1);
}

/**
 * Put one object back in the mempool.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * mempool creation time (see flags).
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj
 *   A pointer to the object to be added.
 */
static inline void __attribute__((always_inline))
rte_mempool_put(struct rte_mempool *mp, void *obj)
{
	rte_mempool_put_bulk(mp, &obj, 1);
}

/**
 * @internal Get several objects from the mempool; used internally.
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to get, must be strictly positive.
 * @param is_mc
 *   Mono-consumer (0) or multi-consumers (1).
 * @return
 *   - >=0: Success; number of objects supplied.
 *   - <0: Error; code of ring dequeue function.
 */
static inline int __attribute__((always_inline))
__mempool_get_bulk(struct rte_mempool *mp, void **obj_table,
		   unsigned n, int is_mc)
{
	int ret;
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	unsigned n_orig = n;
#endif
#if RTE_MEMPOOL_CACHE_MAX_SIZE > 0
	struct rte_mempool_cache *cache;
	uint32_t index, len;
	void **cache_objs;
	unsigned lcore_id = rte_lcore_id();
	uint32_t cache_size = mp->cache_size;

	/* cache is not enabled or single consumer */
	if (unlikely(cache_size == 0 || is_mc == 0 || n >= cache_size))
		goto ring_dequeue;

	cache = &mp->local_cache[lcore_id];
	cache_objs = cache->objs;

	/* Can this be satisfied from the cache? */
	if (cache->len < n) {
		/* No. Backfill the cache first, and then fill from it */
		uint32_t req = n + (cache_size - cache->len);

		/* How many do we require i.e. number to fill the cache + the request */
		ret = rte_ring_mc_dequeue_bulk(mp->ring, &cache->objs[cache->len], req);
		if (unlikely(ret < 0)) {
			/*
			 * In the offchance that we are buffer constrained,
			 * where we are not able to allocate cache + n, go to
			 * the ring directly. If that fails, we are truly out of
			 * buffers.
			 */
			goto ring_dequeue;
		}

		cache->len += req;
	}

	/* Now fill in the response ... */
	for (index = 0, len = cache->len - 1; index < n; ++index, len--, obj_table++)
		*obj_table = cache_objs[len];

	cache->len -= n;

	__MEMPOOL_STAT_ADD(mp, get_success, n_orig);

	return 0;

ring_dequeue:
#endif /* RTE_MEMPOOL_CACHE_MAX_SIZE > 0 */

	/* get remaining objects from ring */
	if (is_mc)
		ret = rte_ring_mc_dequeue_bulk(mp->ring, obj_table, n);
	else
		ret = rte_ring_sc_dequeue_bulk(mp->ring, obj_table, n);

	if (ret < 0)
		__MEMPOOL_STAT_ADD(mp, get_fail, n_orig);
	else
		__MEMPOOL_STAT_ADD(mp, get_success, n_orig);

	return ret;
}

/**
 * Get several objects from the mempool (multi-consumers safe).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to get from mempool to obj_table.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static inline int __attribute__((always_inline))
rte_mempool_mc_get_bulk(struct rte_mempool *mp, void **obj_table, unsigned n)
{
	int ret;
	ret = __mempool_get_bulk(mp, obj_table, n, 1);
	if (ret == 0)
		__mempool_check_cookies(mp, obj_table, n, 1);
	return ret;
}

/**
 * Get several objects from the mempool (NOT multi-consumers safe).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to get from the mempool to obj_table.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is
 *     retrieved.
 */
static inline int __attribute__((always_inline))
rte_mempool_sc_get_bulk(struct rte_mempool *mp, void **obj_table, unsigned n)
{
	int ret;
	ret = __mempool_get_bulk(mp, obj_table, n, 0);
	if (ret == 0)
		__mempool_check_cookies(mp, obj_table, n, 1);
	return ret;
}

/**
 * Get several objects from the mempool.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * mempool creation time (see flags).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to get from the mempool to obj_table.
 * @return
 *   - 0: Success; objects taken
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static inline int __attribute__((always_inline))
rte_mempool_get_bulk(struct rte_mempool *mp, void **obj_table, unsigned n)
{
	int ret;
	ret = __mempool_get_bulk(mp, obj_table, n,
				 !(mp->flags & MEMPOOL_F_SC_GET));
	if (ret == 0)
		__mempool_check_cookies(mp, obj_table, n, 1);
	return ret;
}

/**
 * Get one object from the mempool (multi-consumers safe).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static inline int __attribute__((always_inline))
rte_mempool_mc_get(struct rte_mempool *mp, void **obj_p)
{
	return rte_mempool_mc_get_bulk(mp, obj_p, 1);
}

/**
 * Get one object from the mempool (NOT multi-consumers safe).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static inline int __attribute__((always_inline))
rte_mempool_sc_get(struct rte_mempool *mp, void **obj_p)
{
	return rte_mempool_sc_get_bulk(mp, obj_p, 1);
}

/**
 * Get one object from the mempool.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behavior that was specified at
 * mempool creation (see flags).
 *
 * If cache is enabled, objects will be retrieved first from cache,
 * subsequently from the common pool. Note that it can return -ENOENT when
 * the local cache and common pool are empty, even if cache from other
 * lcores are full.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects taken.
 *   - -ENOENT: Not enough entries in the mempool; no object is retrieved.
 */
static inline int __attribute__((always_inline))
rte_mempool_get(struct rte_mempool *mp, void **obj_p)
{
	return rte_mempool_get_bulk(mp, obj_p, 1);
}

/**
 * Return the number of entries in the mempool.
 *
 * When cache is enabled, this function has to browse the length of
 * all lcores, so it should not be used in a data path, but only for
 * debug purposes.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @return
 *   The number of entries in the mempool.
 */
unsigned rte_mempool_count(const struct rte_mempool *mp);

/**
 * Return the number of free entries in the mempool ring.
 * i.e. how many entries can be freed back to the mempool.
 *
 * NOTE: This corresponds to the number of elements *allocated* from the
 * memory pool, not the number of elements in the pool itself. To count
 * the number elements currently available in the pool, use "rte_mempool_count"
 *
 * When cache is enabled, this function has to browse the length of
 * all lcores, so it should not be used in a data path, but only for
 * debug purposes.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @return
 *   The number of free entries in the mempool.
 */
static inline unsigned
rte_mempool_free_count(const struct rte_mempool *mp)
{
	return mp->size - rte_mempool_count(mp);
}

/**
 * Test if the mempool is full.
 *
 * When cache is enabled, this function has to browse the length of all
 * lcores, so it should not be used in a data path, but only for debug
 * purposes.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @return
 *   - 1: The mempool is full.
 *   - 0: The mempool is not full.
 */
static inline int
rte_mempool_full(const struct rte_mempool *mp)
{
	return !!(rte_mempool_count(mp) == mp->size);
}

/**
 * Test if the mempool is empty.
 *
 * When cache is enabled, this function has to browse the length of all
 * lcores, so it should not be used in a data path, but only for debug
 * purposes.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @return
 *   - 1: The mempool is empty.
 *   - 0: The mempool is not empty.
 */
static inline int
rte_mempool_empty(const struct rte_mempool *mp)
{
	return !!(rte_mempool_count(mp) == 0);
}

/**
 * Return the physical address of elt, which is an element of the pool mp.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @param elt
 *   A pointer (virtual address) to the element of the pool.
 * @return
 *   The physical address of the elt element.
 */
static inline phys_addr_t
rte_mempool_virt2phy(const struct rte_mempool *mp, const void *elt)
{
	if (rte_eal_has_hugepages()) {
		uintptr_t off;

		off = (const char *)elt - (const char *)mp->elt_va_start;
		return (mp->elt_pa[off >> mp->pg_shift] + (off & mp->pg_mask));
	} else {
		/*
		 * If huge pages are disabled, we cannot assume the
		 * memory region to be physically contiguous.
		 * Lookup for each element.
		 */
		return rte_mem_virt2phy(elt);
	}
}

/**
 * Check the consistency of mempool objects.
 *
 * Verify the coherency of fields in the mempool structure. Also check
 * that the cookies of mempool objects (even the ones that are not
 * present in pool) have a correct value. If not, a panic will occur.
 *
 * @param mp
 *   A pointer to the mempool structure.
 */
void rte_mempool_audit(const struct rte_mempool *mp);

/**
 * Return a pointer to the private data in an mempool structure.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @return
 *   A pointer to the private data.
 */
static inline void *rte_mempool_get_priv(struct rte_mempool *mp)
{
	return (char *)mp + MEMPOOL_HEADER_SIZE(mp, mp->pg_num);
}

/**
 * Dump the status of all mempools on the console
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_mempool_list_dump(FILE *f);

/**
 * Search a mempool from its name
 *
 * @param name
 *   The name of the mempool.
 * @return
 *   The pointer to the mempool matching the name, or NULL if not found.
 *   NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - ENOENT - required entry not available to return.
 *
 */
struct rte_mempool *rte_mempool_lookup(const char *name);

/**
 * Given a desired size of the mempool element and mempool flags,
 * caluclates header, trailer, body and total sizes of the mempool object.
 * @param elt_size
 *   The size of each element.
 * @param flags
 *   The flags used for the mempool creation.
 *   Consult rte_mempool_create() for more information about possible values.
 *   The size of each element.
 * @return
 *   Total size of the mempool object.
 */
uint32_t rte_mempool_calc_obj_size(uint32_t elt_size, uint32_t flags,
	struct rte_mempool_objsz *sz);

/**
 * Calculate maximum amount of memory required to store given number of objects.
 * Assumes that the memory buffer will be aligned at page boundary.
 * Note, that if object size is bigger then page size, then it assumes that
 * we have a subsets of physically continuous  pages big enough to store
 * at least one object.
 * @param elt_num
 *   Number of elements.
 * @param elt_sz
 *   The size of each element.
 * @param pg_shift
 *   LOG2 of the physical pages size.
 * @return
 *   Required memory size aligned at page boundary.
 */
size_t rte_mempool_xmem_size(uint32_t elt_num, size_t elt_sz,
	uint32_t pg_shift);

/**
 * Calculate how much memory would be actually required with the given
 * memory footprint to store required number of objects.
 * @param vaddr
 *   Virtual address of the externally allocated memory buffer.
 *   Will be used to store mempool objects.
 * @param elt_num
 *   Number of elements.
 * @param elt_sz
 *   The size of each element.
 * @param paddr
 *   Array of phyiscall addresses of the pages that comprises given memory
 *   buffer.
 * @param pg_num
 *   Number of elements in the paddr array.
 * @param pg_shift
 *   LOG2 of the physical pages size.
 * @return
 *   Number of bytes needed to store given number of objects,
 *   aligned to the given page size.
 *   If provided memory buffer is not big enough:
 *   (-1) * actual number of elemnts that can be stored in that buffer.
 */
ssize_t rte_mempool_xmem_usage(void *vaddr, uint32_t elt_num, size_t elt_sz,
	const phys_addr_t paddr[], uint32_t pg_num, uint32_t pg_shift);

/**
 * Walk list of all memory pools
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 */
void rte_mempool_walk(void (*func)(const struct rte_mempool *, void *arg),
		      void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMPOOL_H_ */
