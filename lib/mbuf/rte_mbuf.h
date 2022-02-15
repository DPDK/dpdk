/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright 2014 6WIND S.A.
 */

#ifndef _RTE_MBUF_H_
#define _RTE_MBUF_H_

/**
 * @file
 * RTE Mbuf
 *
 * The mbuf library provides the ability to create and destroy buffers
 * that may be used by the RTE application to store message
 * buffers. The message buffers are stored in a mempool, using the
 * RTE mempool library.
 *
 * The preferred way to create a mbuf pool is to use
 * rte_pktmbuf_pool_create(). However, in some situations, an
 * application may want to have more control (ex: populate the pool with
 * specific memory), in this case it is possible to use functions from
 * rte_mempool. See how rte_pktmbuf_pool_create() is implemented for
 * details.
 *
 * This library provides an API to allocate/free packet mbufs, which are
 * used to carry network packets.
 *
 * To understand the concepts of packet buffers or mbufs, you
 * should read "TCP/IP Illustrated, Volume 2: The Implementation,
 * Addison-Wesley, 1995, ISBN 0-201-63354-X from Richard Stevens"
 * http://www.kohala.com/start/tcpipiv2.html
 */

#include <stdint.h>
#include <rte_compat.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_mbuf_ptype.h>
#include <rte_mbuf_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the name of a RX offload flag
 *
 * @param mask
 *   The mask describing the flag.
 * @return
 *   The name of this flag, or NULL if it's not a valid RX flag.
 */
const char *rte_get_rx_ol_flag_name(uint64_t mask);

/**
 * Dump the list of RX offload flags in a buffer
 *
 * @param mask
 *   The mask describing the RX flags.
 * @param buf
 *   The output buffer.
 * @param buflen
 *   The length of the buffer.
 * @return
 *   0 on success, (-1) on error.
 */
int rte_get_rx_ol_flag_list(uint64_t mask, char *buf, size_t buflen);

/**
 * Get the name of a TX offload flag
 *
 * @param mask
 *   The mask describing the flag. Usually only one bit must be set.
 *   Several bits can be given if they belong to the same mask.
 *   Ex: RTE_MBUF_F_TX_L4_MASK.
 * @return
 *   The name of this flag, or NULL if it's not a valid TX flag.
 */
const char *rte_get_tx_ol_flag_name(uint64_t mask);

/**
 * Dump the list of TX offload flags in a buffer
 *
 * @param mask
 *   The mask describing the TX flags.
 * @param buf
 *   The output buffer.
 * @param buflen
 *   The length of the buffer.
 * @return
 *   0 on success, (-1) on error.
 */
int rte_get_tx_ol_flag_list(uint64_t mask, char *buf, size_t buflen);

/**
 * Prefetch the first part of the mbuf
 *
 * The first 64 bytes of the mbuf corresponds to fields that are used early
 * in the receive path. If the cache line of the architecture is higher than
 * 64B, the second part will also be prefetched.
 *
 * @param m
 *   The pointer to the mbuf.
 */
static inline void
rte_mbuf_prefetch_part1(struct rte_mbuf *m)
{
	rte_prefetch0(&m->cacheline0);
}

/**
 * Prefetch the second part of the mbuf
 *
 * The next 64 bytes of the mbuf corresponds to fields that are used in the
 * transmit path. If the cache line of the architecture is higher than 64B,
 * this function does nothing as it is expected that the full mbuf is
 * already in cache.
 *
 * @param m
 *   The pointer to the mbuf.
 */
static inline void
rte_mbuf_prefetch_part2(struct rte_mbuf *m)
{
#if RTE_CACHE_LINE_SIZE == 64
	rte_prefetch0(&m->cacheline1);
#else
	RTE_SET_USED(m);
#endif
}


static inline uint16_t rte_pktmbuf_priv_size(struct rte_mempool *mp);

/**
 * Return the IO address of the beginning of the mbuf data
 *
 * @param mb
 *   The pointer to the mbuf.
 * @return
 *   The IO address of the beginning of the mbuf data
 */
static inline rte_iova_t
rte_mbuf_data_iova(const struct rte_mbuf *mb)
{
	return mb->buf_iova + mb->data_off;
}

/**
 * Return the default IO address of the beginning of the mbuf data
 *
 * This function is used by drivers in their receive function, as it
 * returns the location where data should be written by the NIC, taking
 * the default headroom in account.
 *
 * @param mb
 *   The pointer to the mbuf.
 * @return
 *   The IO address of the beginning of the mbuf data
 */
static inline rte_iova_t
rte_mbuf_data_iova_default(const struct rte_mbuf *mb)
{
	return mb->buf_iova + RTE_PKTMBUF_HEADROOM;
}

/**
 * Return the mbuf owning the data buffer address of an indirect mbuf.
 *
 * @param mi
 *   The pointer to the indirect mbuf.
 * @return
 *   The address of the direct mbuf corresponding to buffer_addr.
 */
static inline struct rte_mbuf *
rte_mbuf_from_indirect(struct rte_mbuf *mi)
{
	return (struct rte_mbuf *)RTE_PTR_SUB(mi->buf_addr, sizeof(*mi) + mi->priv_size);
}

/**
 * Return address of buffer embedded in the given mbuf.
 *
 * The return value shall be same as mb->buf_addr if the mbuf is already
 * initialized and direct. However, this API is useful if mempool of the
 * mbuf is already known because it doesn't need to access mbuf contents in
 * order to get the mempool pointer.
 *
 * @param mb
 *   The pointer to the mbuf.
 * @param mp
 *   The pointer to the mempool of the mbuf.
 * @return
 *   The pointer of the mbuf buffer.
 */
static inline char *
rte_mbuf_buf_addr(struct rte_mbuf *mb, struct rte_mempool *mp)
{
	return (char *)mb + sizeof(*mb) + rte_pktmbuf_priv_size(mp);
}

/**
 * Return the default address of the beginning of the mbuf data.
 *
 * @param mb
 *   The pointer to the mbuf.
 * @return
 *   The pointer of the beginning of the mbuf data.
 */
static inline char *
rte_mbuf_data_addr_default(struct rte_mbuf *mb)
{
	return rte_mbuf_buf_addr(mb, mb->pool) + RTE_PKTMBUF_HEADROOM;
}

/**
 * Return address of buffer embedded in the given mbuf.
 *
 * @note: Accessing mempool pointer of a mbuf is expensive because the
 * pointer is stored in the 2nd cache line of mbuf. If mempool is known, it
 * is better not to reference the mempool pointer in mbuf but calling
 * rte_mbuf_buf_addr() would be more efficient.
 *
 * @param md
 *   The pointer to the mbuf.
 * @return
 *   The address of the data buffer owned by the mbuf.
 */
static inline char *
rte_mbuf_to_baddr(struct rte_mbuf *md)
{
	return rte_mbuf_buf_addr(md, md->pool);
}

/**
 * Return the starting address of the private data area embedded in
 * the given mbuf.
 *
 * Note that no check is made to ensure that a private data area
 * actually exists in the supplied mbuf.
 *
 * @param m
 *   The pointer to the mbuf.
 * @return
 *   The starting address of the private data area of the given mbuf.
 */
static inline void *
rte_mbuf_to_priv(struct rte_mbuf *m)
{
	return RTE_PTR_ADD(m, sizeof(struct rte_mbuf));
}

/**
 * Private data in case of pktmbuf pool.
 *
 * A structure that contains some pktmbuf_pool-specific data that are
 * appended after the mempool structure (in private data).
 */
struct rte_pktmbuf_pool_private {
	uint16_t mbuf_data_room_size; /**< Size of data space in each mbuf. */
	uint16_t mbuf_priv_size;      /**< Size of private area in each mbuf. */
	uint32_t flags; /**< reserved for future use. */
};

/**
 * Return the flags from private data in an mempool structure.
 *
 * @param mp
 *   A pointer to the mempool structure.
 * @return
 *   The flags from the private data structure.
 */
static inline uint32_t
rte_pktmbuf_priv_flags(struct rte_mempool *mp)
{
	struct rte_pktmbuf_pool_private *mbp_priv;

	mbp_priv = (struct rte_pktmbuf_pool_private *)rte_mempool_get_priv(mp);
	return mbp_priv->flags;
}

/**
 * When set, pktmbuf mempool will hold only mbufs with pinned external
 * buffer. The external buffer will be attached to the mbuf at the
 * memory pool creation and will never be detached by the mbuf free calls.
 * mbuf should not contain any room for data after the mbuf structure.
 */
#define RTE_PKTMBUF_POOL_F_PINNED_EXT_BUF (1 << 0)

/**
 * Returns non zero if given mbuf has a pinned external buffer, or zero
 * otherwise. The pinned external buffer is allocated at pool creation
 * time and should not be freed on mbuf freeing.
 *
 * External buffer is a user-provided anonymous buffer.
 */
#define RTE_MBUF_HAS_PINNED_EXTBUF(mb) \
	(rte_pktmbuf_priv_flags(mb->pool) & RTE_PKTMBUF_POOL_F_PINNED_EXT_BUF)

#ifdef RTE_LIBRTE_MBUF_DEBUG

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, is_h) rte_mbuf_sanity_check(m, is_h)

#else /*  RTE_LIBRTE_MBUF_DEBUG */

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, is_h) do { } while (0)

#endif /*  RTE_LIBRTE_MBUF_DEBUG */

#ifdef RTE_MBUF_REFCNT_ATOMIC

/**
 * Reads the value of an mbuf's refcnt.
 * @param m
 *   Mbuf to read
 * @return
 *   Reference count number.
 */
static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return __atomic_load_n(&m->refcnt, __ATOMIC_RELAXED);
}

/**
 * Sets an mbuf's refcnt to a defined value.
 * @param m
 *   Mbuf to update
 * @param new_value
 *   Value set
 */
static inline void
rte_mbuf_refcnt_set(struct rte_mbuf *m, uint16_t new_value)
{
	__atomic_store_n(&m->refcnt, new_value, __ATOMIC_RELAXED);
}

/* internal */
static inline uint16_t
__rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	return __atomic_add_fetch(&m->refcnt, (uint16_t)value,
				 __ATOMIC_ACQ_REL);
}

/**
 * Adds given value to an mbuf's refcnt and returns its new value.
 * @param m
 *   Mbuf to update
 * @param value
 *   Value to add/subtract
 * @return
 *   Updated value
 */
static inline uint16_t
rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	/*
	 * The atomic_add is an expensive operation, so we don't want to
	 * call it in the case where we know we are the unique holder of
	 * this mbuf (i.e. ref_cnt == 1). Otherwise, an atomic
	 * operation has to be used because concurrent accesses on the
	 * reference counter can occur.
	 */
	if (likely(rte_mbuf_refcnt_read(m) == 1)) {
		++value;
		rte_mbuf_refcnt_set(m, (uint16_t)value);
		return (uint16_t)value;
	}

	return __rte_mbuf_refcnt_update(m, value);
}

#else /* ! RTE_MBUF_REFCNT_ATOMIC */

/* internal */
static inline uint16_t
__rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	m->refcnt = (uint16_t)(m->refcnt + value);
	return m->refcnt;
}

/**
 * Adds given value to an mbuf's refcnt and returns its new value.
 */
static inline uint16_t
rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	return __rte_mbuf_refcnt_update(m, value);
}

/**
 * Reads the value of an mbuf's refcnt.
 */
static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return m->refcnt;
}

/**
 * Sets an mbuf's refcnt to the defined value.
 */
static inline void
rte_mbuf_refcnt_set(struct rte_mbuf *m, uint16_t new_value)
{
	m->refcnt = new_value;
}

#endif /* RTE_MBUF_REFCNT_ATOMIC */

/**
 * Reads the refcnt of an external buffer.
 *
 * @param shinfo
 *   Shared data of the external buffer.
 * @return
 *   Reference count number.
 */
static inline uint16_t
rte_mbuf_ext_refcnt_read(const struct rte_mbuf_ext_shared_info *shinfo)
{
	return __atomic_load_n(&shinfo->refcnt, __ATOMIC_RELAXED);
}

/**
 * Set refcnt of an external buffer.
 *
 * @param shinfo
 *   Shared data of the external buffer.
 * @param new_value
 *   Value set
 */
static inline void
rte_mbuf_ext_refcnt_set(struct rte_mbuf_ext_shared_info *shinfo,
	uint16_t new_value)
{
	__atomic_store_n(&shinfo->refcnt, new_value, __ATOMIC_RELAXED);
}

/**
 * Add given value to refcnt of an external buffer and return its new
 * value.
 *
 * @param shinfo
 *   Shared data of the external buffer.
 * @param value
 *   Value to add/subtract
 * @return
 *   Updated value
 */
static inline uint16_t
rte_mbuf_ext_refcnt_update(struct rte_mbuf_ext_shared_info *shinfo,
	int16_t value)
{
	if (likely(rte_mbuf_ext_refcnt_read(shinfo) == 1)) {
		++value;
		rte_mbuf_ext_refcnt_set(shinfo, (uint16_t)value);
		return (uint16_t)value;
	}

	return __atomic_add_fetch(&shinfo->refcnt, (uint16_t)value,
				 __ATOMIC_ACQ_REL);
}

/** Mbuf prefetch */
#define RTE_MBUF_PREFETCH_TO_FREE(m) do {       \
	if ((m) != NULL)                        \
		rte_prefetch0(m);               \
} while (0)


/**
 * Sanity checks on an mbuf.
 *
 * Check the consistency of the given mbuf. The function will cause a
 * panic if corruption is detected.
 *
 * @param m
 *   The mbuf to be checked.
 * @param is_header
 *   True if the mbuf is a packet header, false if it is a sub-segment
 *   of a packet (in this case, some fields like nb_segs are not checked)
 */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, int is_header);

/**
 * Sanity checks on a mbuf.
 *
 * Almost like rte_mbuf_sanity_check(), but this function gives the reason
 * if corruption is detected rather than panic.
 *
 * @param m
 *   The mbuf to be checked.
 * @param is_header
 *   True if the mbuf is a packet header, false if it is a sub-segment
 *   of a packet (in this case, some fields like nb_segs are not checked)
 * @param reason
 *   A reference to a string pointer where to store the reason why a mbuf is
 *   considered invalid.
 * @return
 *   - 0 if no issue has been found, reason is left untouched.
 *   - -1 if a problem is detected, reason then points to a string describing
 *     the reason why the mbuf is deemed invalid.
 */
int rte_mbuf_check(const struct rte_mbuf *m, int is_header,
		   const char **reason);

/**
 * Sanity checks on a reinitialized mbuf in debug mode.
 *
 * Check the consistency of the given reinitialized mbuf.
 * The function will cause a panic if corruption is detected.
 *
 * Check that the mbuf is properly reinitialized (refcnt=1, next=NULL,
 * nb_segs=1), as done by rte_pktmbuf_prefree_seg().
 *
 * @param m
 *   The mbuf to be checked.
 */
static __rte_always_inline void
__rte_mbuf_raw_sanity_check(__rte_unused const struct rte_mbuf *m)
{
	RTE_ASSERT(rte_mbuf_refcnt_read(m) == 1);
	RTE_ASSERT(m->next == NULL);
	RTE_ASSERT(m->nb_segs == 1);
	__rte_mbuf_sanity_check(m, 0);
}

/** For backwards compatibility. */
#define MBUF_RAW_ALLOC_CHECK(m) __rte_mbuf_raw_sanity_check(m)

/**
 * Allocate an uninitialized mbuf from mempool *mp*.
 *
 * This function can be used by PMDs (especially in RX functions) to
 * allocate an uninitialized mbuf. The driver is responsible of
 * initializing all the required fields. See rte_pktmbuf_reset().
 * For standard needs, prefer rte_pktmbuf_alloc().
 *
 * The caller can expect that the following fields of the mbuf structure
 * are initialized: buf_addr, buf_iova, buf_len, refcnt=1, nb_segs=1,
 * next=NULL, pool, priv_size. The other fields must be initialized
 * by the caller.
 *
 * @param mp
 *   The mempool from which mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
static inline struct rte_mbuf *rte_mbuf_raw_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	if (rte_mempool_get(mp, (void **)&m) < 0)
		return NULL;
	__rte_mbuf_raw_sanity_check(m);
	return m;
}

/**
 * Put mbuf back into its original mempool.
 *
 * The caller must ensure that the mbuf is direct and properly
 * reinitialized (refcnt=1, next=NULL, nb_segs=1), as done by
 * rte_pktmbuf_prefree_seg().
 *
 * This function should be used with care, when optimization is
 * required. For standard needs, prefer rte_pktmbuf_free() or
 * rte_pktmbuf_free_seg().
 *
 * @param m
 *   The mbuf to be freed.
 */
static __rte_always_inline void
rte_mbuf_raw_free(struct rte_mbuf *m)
{
	RTE_ASSERT(!RTE_MBUF_CLONED(m) &&
		  (!RTE_MBUF_HAS_EXTBUF(m) || RTE_MBUF_HAS_PINNED_EXTBUF(m)));
	__rte_mbuf_raw_sanity_check(m);
	rte_mempool_put(m->pool, m);
}

/**
 * The packet mbuf constructor.
 *
 * This function initializes some fields in the mbuf structure that are
 * not modified by the user once created (origin pool, buffer start
 * address, and so on). This function is given as a callback function to
 * rte_mempool_obj_iter() or rte_mempool_create() at pool creation time.
 *
 * This function expects that the mempool private area was previously
 * initialized with rte_pktmbuf_pool_init().
 *
 * @param mp
 *   The mempool from which mbufs originate.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer is the opaque argument passed to
 *   rte_mempool_obj_iter() or rte_mempool_create().
 * @param m
 *   The mbuf to initialize.
 * @param i
 *   The index of the mbuf in the pool table.
 */
void rte_pktmbuf_init(struct rte_mempool *mp, void *opaque_arg,
		      void *m, unsigned i);

/**
 * A packet mbuf pool constructor.
 *
 * This function initializes the mempool private data in the case of a
 * pktmbuf pool. This private data is needed by the driver. The
 * function must be called on the mempool before it is used, or it
 * can be given as a callback function to rte_mempool_create() at
 * pool creation. It can be extended by the user, for example, to
 * provide another packet size.
 *
 * The mempool private area size must be at least equal to
 * sizeof(struct rte_pktmbuf_pool_private).
 *
 * @param mp
 *   The mempool from which mbufs originate.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer is the opaque argument passed to
 *   rte_mempool_create().
 */
void rte_pktmbuf_pool_init(struct rte_mempool *mp, void *opaque_arg);

/**
 * Create a mbuf pool.
 *
 * This function creates and initializes a packet mbuf pool. It is
 * a wrapper to rte_mempool functions.
 *
 * @param name
 *   The name of the mbuf pool.
 * @param n
 *   The number of elements in the mbuf pool. The optimum size (in terms
 *   of memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param cache_size
 *   Size of the per-core object cache. See rte_mempool_create() for
 *   details.
 * @param priv_size
 *   Size of application private are between the rte_mbuf structure
 *   and the data buffer. This value must be aligned to RTE_MBUF_PRIV_ALIGN.
 * @param data_room_size
 *   Size of data buffer in each mbuf, including RTE_PKTMBUF_HEADROOM.
 * @param socket_id
 *   The socket identifier where the memory should be allocated. The
 *   value can be *SOCKET_ID_ANY* if there is no NUMA constraint for the
 *   reserved zone.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - cache size provided is too large, or priv_size is not aligned.
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_mempool *
rte_pktmbuf_pool_create(const char *name, unsigned n,
	unsigned cache_size, uint16_t priv_size, uint16_t data_room_size,
	int socket_id);

/**
 * Create a mbuf pool with a given mempool ops name
 *
 * This function creates and initializes a packet mbuf pool. It is
 * a wrapper to rte_mempool functions.
 *
 * @param name
 *   The name of the mbuf pool.
 * @param n
 *   The number of elements in the mbuf pool. The optimum size (in terms
 *   of memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param cache_size
 *   Size of the per-core object cache. See rte_mempool_create() for
 *   details.
 * @param priv_size
 *   Size of application private are between the rte_mbuf structure
 *   and the data buffer. This value must be aligned to RTE_MBUF_PRIV_ALIGN.
 * @param data_room_size
 *   Size of data buffer in each mbuf, including RTE_PKTMBUF_HEADROOM.
 * @param socket_id
 *   The socket identifier where the memory should be allocated. The
 *   value can be *SOCKET_ID_ANY* if there is no NUMA constraint for the
 *   reserved zone.
 * @param ops_name
 *   The mempool ops name to be used for this mempool instead of
 *   default mempool. The value can be *NULL* to use default mempool.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - cache size provided is too large, or priv_size is not aligned.
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_mempool *
rte_pktmbuf_pool_create_by_ops(const char *name, unsigned int n,
	unsigned int cache_size, uint16_t priv_size, uint16_t data_room_size,
	int socket_id, const char *ops_name);

/** A structure that describes the pinned external buffer segment. */
struct rte_pktmbuf_extmem {
	void *buf_ptr;		/**< The virtual address of data buffer. */
	rte_iova_t buf_iova;	/**< The IO address of the data buffer. */
	size_t buf_len;		/**< External buffer length in bytes. */
	uint16_t elt_size;	/**< mbuf element size in bytes. */
};

/**
 * Create a mbuf pool with external pinned data buffers.
 *
 * This function creates and initializes a packet mbuf pool that contains
 * only mbufs with external buffer. It is a wrapper to rte_mempool functions.
 *
 * @param name
 *   The name of the mbuf pool.
 * @param n
 *   The number of elements in the mbuf pool. The optimum size (in terms
 *   of memory usage) for a mempool is when n is a power of two minus one:
 *   n = (2^q - 1).
 * @param cache_size
 *   Size of the per-core object cache. See rte_mempool_create() for
 *   details.
 * @param priv_size
 *   Size of application private are between the rte_mbuf structure
 *   and the data buffer. This value must be aligned to RTE_MBUF_PRIV_ALIGN.
 * @param data_room_size
 *   Size of data buffer in each mbuf, including RTE_PKTMBUF_HEADROOM.
 * @param socket_id
 *   The socket identifier where the memory should be allocated. The
 *   value can be *SOCKET_ID_ANY* if there is no NUMA constraint for the
 *   reserved zone.
 * @param ext_mem
 *   Pointer to the array of structures describing the external memory
 *   for data buffers. It is caller responsibility to register this memory
 *   with rte_extmem_register() (if needed), map this memory to appropriate
 *   physical device, etc.
 * @param ext_num
 *   Number of elements in the ext_mem array.
 * @return
 *   The pointer to the new allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - cache size provided is too large, or priv_size is not aligned.
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
__rte_experimental
struct rte_mempool *
rte_pktmbuf_pool_create_extbuf(const char *name, unsigned int n,
	unsigned int cache_size, uint16_t priv_size,
	uint16_t data_room_size, int socket_id,
	const struct rte_pktmbuf_extmem *ext_mem,
	unsigned int ext_num);

/**
 * Get the data room size of mbufs stored in a pktmbuf_pool
 *
 * The data room size is the amount of data that can be stored in a
 * mbuf including the headroom (RTE_PKTMBUF_HEADROOM).
 *
 * @param mp
 *   The packet mbuf pool.
 * @return
 *   The data room size of mbufs stored in this mempool.
 */
static inline uint16_t
rte_pktmbuf_data_room_size(struct rte_mempool *mp)
{
	struct rte_pktmbuf_pool_private *mbp_priv;

	mbp_priv = (struct rte_pktmbuf_pool_private *)rte_mempool_get_priv(mp);
	return mbp_priv->mbuf_data_room_size;
}

/**
 * Get the application private size of mbufs stored in a pktmbuf_pool
 *
 * The private size of mbuf is a zone located between the rte_mbuf
 * structure and the data buffer where an application can store data
 * associated to a packet.
 *
 * @param mp
 *   The packet mbuf pool.
 * @return
 *   The private size of mbufs stored in this mempool.
 */
static inline uint16_t
rte_pktmbuf_priv_size(struct rte_mempool *mp)
{
	struct rte_pktmbuf_pool_private *mbp_priv;

	mbp_priv = (struct rte_pktmbuf_pool_private *)rte_mempool_get_priv(mp);
	return mbp_priv->mbuf_priv_size;
}

/**
 * Reset the data_off field of a packet mbuf to its default value.
 *
 * The given mbuf must have only one segment, which should be empty.
 *
 * @param m
 *   The packet mbuf's data_off field has to be reset.
 */
static inline void rte_pktmbuf_reset_headroom(struct rte_mbuf *m)
{
	m->data_off = (uint16_t)RTE_MIN((uint16_t)RTE_PKTMBUF_HEADROOM,
					(uint16_t)m->buf_len);
}

/**
 * Reset the fields of a packet mbuf to their default values.
 *
 * The given mbuf must have only one segment.
 *
 * @param m
 *   The packet mbuf to be reset.
 */
static inline void rte_pktmbuf_reset(struct rte_mbuf *m)
{
	m->next = NULL;
	m->pkt_len = 0;
	m->tx_offload = 0;
	m->vlan_tci = 0;
	m->vlan_tci_outer = 0;
	m->nb_segs = 1;
	m->port = RTE_MBUF_PORT_INVALID;

	m->ol_flags &= RTE_MBUF_F_EXTERNAL;
	m->packet_type = 0;
	rte_pktmbuf_reset_headroom(m);

	m->data_len = 0;
	__rte_mbuf_sanity_check(m, 1);
}

/**
 * Allocate a new mbuf from a mempool.
 *
 * This new mbuf contains one segment, which has a length of 0. The pointer
 * to data is initialized to have some bytes of headroom in the buffer
 * (if buffer size allows).
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
static inline struct rte_mbuf *rte_pktmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;
	if ((m = rte_mbuf_raw_alloc(mp)) != NULL)
		rte_pktmbuf_reset(m);
	return m;
}

/**
 * Allocate a bulk of mbufs, initialize refcnt and reset the fields to default
 * values.
 *
 *  @param pool
 *    The mempool from which mbufs are allocated.
 *  @param mbufs
 *    Array of pointers to mbufs
 *  @param count
 *    Array size
 *  @return
 *   - 0: Success
 *   - -ENOENT: Not enough entries in the mempool; no mbufs are retrieved.
 */
static inline int rte_pktmbuf_alloc_bulk(struct rte_mempool *pool,
	 struct rte_mbuf **mbufs, unsigned count)
{
	unsigned idx = 0;
	int rc;

	rc = rte_mempool_get_bulk(pool, (void **)mbufs, count);
	if (unlikely(rc))
		return rc;

	/* To understand duff's device on loop unwinding optimization, see
	 * https://en.wikipedia.org/wiki/Duff's_device.
	 * Here while() loop is used rather than do() while{} to avoid extra
	 * check if count is zero.
	 */
	switch (count % 4) {
	case 0:
		while (idx != count) {
			__rte_mbuf_raw_sanity_check(mbufs[idx]);
			rte_pktmbuf_reset(mbufs[idx]);
			idx++;
			/* fall-through */
	case 3:
			__rte_mbuf_raw_sanity_check(mbufs[idx]);
			rte_pktmbuf_reset(mbufs[idx]);
			idx++;
			/* fall-through */
	case 2:
			__rte_mbuf_raw_sanity_check(mbufs[idx]);
			rte_pktmbuf_reset(mbufs[idx]);
			idx++;
			/* fall-through */
	case 1:
			__rte_mbuf_raw_sanity_check(mbufs[idx]);
			rte_pktmbuf_reset(mbufs[idx]);
			idx++;
			/* fall-through */
		}
	}
	return 0;
}

/**
 * Initialize shared data at the end of an external buffer before attaching
 * to a mbuf by ``rte_pktmbuf_attach_extbuf()``. This is not a mandatory
 * initialization but a helper function to simply spare a few bytes at the
 * end of the buffer for shared data. If shared data is allocated
 * separately, this should not be called but application has to properly
 * initialize the shared data according to its need.
 *
 * Free callback and its argument is saved and the refcnt is set to 1.
 *
 * @warning
 * The value of buf_len will be reduced to RTE_PTR_DIFF(shinfo, buf_addr)
 * after this initialization. This shall be used for
 * ``rte_pktmbuf_attach_extbuf()``
 *
 * @param buf_addr
 *   The pointer to the external buffer.
 * @param [in,out] buf_len
 *   The pointer to length of the external buffer. Input value must be
 *   larger than the size of ``struct rte_mbuf_ext_shared_info`` and
 *   padding for alignment. If not enough, this function will return NULL.
 *   Adjusted buffer length will be returned through this pointer.
 * @param free_cb
 *   Free callback function to call when the external buffer needs to be
 *   freed.
 * @param fcb_opaque
 *   Argument for the free callback function.
 *
 * @return
 *   A pointer to the initialized shared data on success, return NULL
 *   otherwise.
 */
static inline struct rte_mbuf_ext_shared_info *
rte_pktmbuf_ext_shinfo_init_helper(void *buf_addr, uint16_t *buf_len,
	rte_mbuf_extbuf_free_callback_t free_cb, void *fcb_opaque)
{
	struct rte_mbuf_ext_shared_info *shinfo;
	void *buf_end = RTE_PTR_ADD(buf_addr, *buf_len);
	void *addr;

	addr = RTE_PTR_ALIGN_FLOOR(RTE_PTR_SUB(buf_end, sizeof(*shinfo)),
				   sizeof(uintptr_t));
	if (addr <= buf_addr)
		return NULL;

	shinfo = (struct rte_mbuf_ext_shared_info *)addr;
	shinfo->free_cb = free_cb;
	shinfo->fcb_opaque = fcb_opaque;
	rte_mbuf_ext_refcnt_set(shinfo, 1);

	*buf_len = (uint16_t)RTE_PTR_DIFF(shinfo, buf_addr);
	return shinfo;
}

/**
 * Attach an external buffer to a mbuf.
 *
 * User-managed anonymous buffer can be attached to an mbuf. When attaching
 * it, corresponding free callback function and its argument should be
 * provided via shinfo. This callback function will be called once all the
 * mbufs are detached from the buffer (refcnt becomes zero).
 *
 * The headroom length of the attaching mbuf will be set to zero and this
 * can be properly adjusted after attachment. For example, ``rte_pktmbuf_adj()``
 * or ``rte_pktmbuf_reset_headroom()`` might be used.
 *
 * Similarly, the packet length is initialized to 0. If the buffer contains
 * data, the user has to adjust ``data_len`` and the ``pkt_len`` field of
 * the mbuf accordingly.
 *
 * More mbufs can be attached to the same external buffer by
 * ``rte_pktmbuf_attach()`` once the external buffer has been attached by
 * this API.
 *
 * Detachment can be done by either ``rte_pktmbuf_detach_extbuf()`` or
 * ``rte_pktmbuf_detach()``.
 *
 * Memory for shared data must be provided and user must initialize all of
 * the content properly, especially free callback and refcnt. The pointer
 * of shared data will be stored in m->shinfo.
 * ``rte_pktmbuf_ext_shinfo_init_helper`` can help to simply spare a few
 * bytes at the end of buffer for the shared data, store free callback and
 * its argument and set the refcnt to 1. The following is an example:
 *
 *   struct rte_mbuf_ext_shared_info *shinfo =
 *          rte_pktmbuf_ext_shinfo_init_helper(buf_addr, &buf_len,
 *                                             free_cb, fcb_arg);
 *   rte_pktmbuf_attach_extbuf(m, buf_addr, buf_iova, buf_len, shinfo);
 *   rte_pktmbuf_reset_headroom(m);
 *   rte_pktmbuf_adj(m, data_len);
 *
 * Attaching an external buffer is quite similar to mbuf indirection in
 * replacing buffer addresses and length of a mbuf, but a few differences:
 * - When an indirect mbuf is attached, refcnt of the direct mbuf would be
 *   2 as long as the direct mbuf itself isn't freed after the attachment.
 *   In such cases, the buffer area of a direct mbuf must be read-only. But
 *   external buffer has its own refcnt and it starts from 1. Unless
 *   multiple mbufs are attached to a mbuf having an external buffer, the
 *   external buffer is writable.
 * - There's no need to allocate buffer from a mempool. Any buffer can be
 *   attached with appropriate free callback and its IO address.
 * - Smaller metadata is required to maintain shared data such as refcnt.
 *
 * @param m
 *   The pointer to the mbuf.
 * @param buf_addr
 *   The pointer to the external buffer.
 * @param buf_iova
 *   IO address of the external buffer.
 * @param buf_len
 *   The size of the external buffer.
 * @param shinfo
 *   User-provided memory for shared data of the external buffer.
 */
static inline void
rte_pktmbuf_attach_extbuf(struct rte_mbuf *m, void *buf_addr,
	rte_iova_t buf_iova, uint16_t buf_len,
	struct rte_mbuf_ext_shared_info *shinfo)
{
	/* mbuf should not be read-only */
	RTE_ASSERT(RTE_MBUF_DIRECT(m) && rte_mbuf_refcnt_read(m) == 1);
	RTE_ASSERT(shinfo->free_cb != NULL);

	m->buf_addr = buf_addr;
	m->buf_iova = buf_iova;
	m->buf_len = buf_len;

	m->data_len = 0;
	m->data_off = 0;

	m->ol_flags |= RTE_MBUF_F_EXTERNAL;
	m->shinfo = shinfo;
}

/**
 * Detach the external buffer attached to a mbuf, same as
 * ``rte_pktmbuf_detach()``
 *
 * @param m
 *   The mbuf having external buffer.
 */
#define rte_pktmbuf_detach_extbuf(m) rte_pktmbuf_detach(m)

/**
 * Copy dynamic fields from msrc to mdst.
 *
 * @param mdst
 *   The destination mbuf.
 * @param msrc
 *   The source mbuf.
 */
static inline void
rte_mbuf_dynfield_copy(struct rte_mbuf *mdst, const struct rte_mbuf *msrc)
{
	memcpy(&mdst->dynfield1, msrc->dynfield1, sizeof(mdst->dynfield1));
}

/* internal */
static inline void
__rte_pktmbuf_copy_hdr(struct rte_mbuf *mdst, const struct rte_mbuf *msrc)
{
	mdst->port = msrc->port;
	mdst->vlan_tci = msrc->vlan_tci;
	mdst->vlan_tci_outer = msrc->vlan_tci_outer;
	mdst->tx_offload = msrc->tx_offload;
	mdst->hash = msrc->hash;
	mdst->packet_type = msrc->packet_type;
	rte_mbuf_dynfield_copy(mdst, msrc);
}

/**
 * Attach packet mbuf to another packet mbuf.
 *
 * If the mbuf we are attaching to isn't a direct buffer and is attached to
 * an external buffer, the mbuf being attached will be attached to the
 * external buffer instead of mbuf indirection.
 *
 * Otherwise, the mbuf will be indirectly attached. After attachment we
 * refer the mbuf we attached as 'indirect', while mbuf we attached to as
 * 'direct'.  The direct mbuf's reference counter is incremented.
 *
 * Right now, not supported:
 *  - attachment for already indirect mbuf (e.g. - mi has to be direct).
 *  - mbuf we trying to attach (mi) is used by someone else
 *    e.g. it's reference counter is greater then 1.
 *
 * @param mi
 *   The indirect packet mbuf.
 * @param m
 *   The packet mbuf we're attaching to.
 */
static inline void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *m)
{
	RTE_ASSERT(RTE_MBUF_DIRECT(mi) &&
	    rte_mbuf_refcnt_read(mi) == 1);

	if (RTE_MBUF_HAS_EXTBUF(m)) {
		rte_mbuf_ext_refcnt_update(m->shinfo, 1);
		mi->ol_flags = m->ol_flags;
		mi->shinfo = m->shinfo;
	} else {
		/* if m is not direct, get the mbuf that embeds the data */
		rte_mbuf_refcnt_update(rte_mbuf_from_indirect(m), 1);
		mi->priv_size = m->priv_size;
		mi->ol_flags = m->ol_flags | RTE_MBUF_F_INDIRECT;
	}

	__rte_pktmbuf_copy_hdr(mi, m);

	mi->data_off = m->data_off;
	mi->data_len = m->data_len;
	mi->buf_iova = m->buf_iova;
	mi->buf_addr = m->buf_addr;
	mi->buf_len = m->buf_len;

	mi->next = NULL;
	mi->pkt_len = mi->data_len;
	mi->nb_segs = 1;

	__rte_mbuf_sanity_check(mi, 1);
	__rte_mbuf_sanity_check(m, 0);
}

/**
 * @internal used by rte_pktmbuf_detach().
 *
 * Decrement the reference counter of the external buffer. When the
 * reference counter becomes 0, the buffer is freed by pre-registered
 * callback.
 */
static inline void
__rte_pktmbuf_free_extbuf(struct rte_mbuf *m)
{
	RTE_ASSERT(RTE_MBUF_HAS_EXTBUF(m));
	RTE_ASSERT(m->shinfo != NULL);

	if (rte_mbuf_ext_refcnt_update(m->shinfo, -1) == 0)
		m->shinfo->free_cb(m->buf_addr, m->shinfo->fcb_opaque);
}

/**
 * @internal used by rte_pktmbuf_detach().
 *
 * Decrement the direct mbuf's reference counter. When the reference
 * counter becomes 0, the direct mbuf is freed.
 */
static inline void
__rte_pktmbuf_free_direct(struct rte_mbuf *m)
{
	struct rte_mbuf *md;

	RTE_ASSERT(RTE_MBUF_CLONED(m));

	md = rte_mbuf_from_indirect(m);

	if (rte_mbuf_refcnt_update(md, -1) == 0) {
		md->next = NULL;
		md->nb_segs = 1;
		rte_mbuf_refcnt_set(md, 1);
		rte_mbuf_raw_free(md);
	}
}

/**
 * Detach a packet mbuf from external buffer or direct buffer.
 *
 *  - decrement refcnt and free the external/direct buffer if refcnt
 *    becomes zero.
 *  - restore original mbuf address and length values.
 *  - reset pktmbuf data and data_len to their default values.
 *
 * All other fields of the given packet mbuf will be left intact.
 *
 * If the packet mbuf was allocated from the pool with pinned
 * external buffers the rte_pktmbuf_detach does nothing with the
 * mbuf of this kind, because the pinned buffers are not supposed
 * to be detached.
 *
 * @param m
 *   The indirect attached packet mbuf.
 */
static inline void rte_pktmbuf_detach(struct rte_mbuf *m)
{
	struct rte_mempool *mp = m->pool;
	uint32_t mbuf_size, buf_len;
	uint16_t priv_size;

	if (RTE_MBUF_HAS_EXTBUF(m)) {
		/*
		 * The mbuf has the external attached buffer,
		 * we should check the type of the memory pool where
		 * the mbuf was allocated from to detect the pinned
		 * external buffer.
		 */
		uint32_t flags = rte_pktmbuf_priv_flags(mp);

		if (flags & RTE_PKTMBUF_POOL_F_PINNED_EXT_BUF) {
			/*
			 * The pinned external buffer should not be
			 * detached from its backing mbuf, just exit.
			 */
			return;
		}
		__rte_pktmbuf_free_extbuf(m);
	} else {
		__rte_pktmbuf_free_direct(m);
	}
	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = (uint32_t)(sizeof(struct rte_mbuf) + priv_size);
	buf_len = rte_pktmbuf_data_room_size(mp);

	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
	m->buf_len = (uint16_t)buf_len;
	rte_pktmbuf_reset_headroom(m);
	m->data_len = 0;
	m->ol_flags = 0;
}

/**
 * @internal Handle the packet mbufs with attached pinned external buffer
 * on the mbuf freeing:
 *
 *  - return zero if reference counter in shinfo is one. It means there is
 *  no more reference to this pinned buffer and mbuf can be returned to
 *  the pool
 *
 *  - otherwise (if reference counter is not one), decrement reference
 *  counter and return non-zero value to prevent freeing the backing mbuf.
 *
 * Returns non zero if mbuf should not be freed.
 */
static inline int __rte_pktmbuf_pinned_extbuf_decref(struct rte_mbuf *m)
{
	struct rte_mbuf_ext_shared_info *shinfo;

	/* Clear flags, mbuf is being freed. */
	m->ol_flags = RTE_MBUF_F_EXTERNAL;
	shinfo = m->shinfo;

	/* Optimize for performance - do not dec/reinit */
	if (likely(rte_mbuf_ext_refcnt_read(shinfo) == 1))
		return 0;

	/*
	 * Direct usage of add primitive to avoid
	 * duplication of comparing with one.
	 */
	if (likely(__atomic_add_fetch(&shinfo->refcnt, (uint16_t)-1,
				     __ATOMIC_ACQ_REL)))
		return 1;

	/* Reinitialize counter before mbuf freeing. */
	rte_mbuf_ext_refcnt_set(shinfo, 1);
	return 0;
}

/**
 * Decrease reference counter and unlink a mbuf segment
 *
 * This function does the same than a free, except that it does not
 * return the segment to its pool.
 * It decreases the reference counter, and if it reaches 0, it is
 * detached from its parent for an indirect mbuf.
 *
 * @param m
 *   The mbuf to be unlinked
 * @return
 *   - (m) if it is the last reference. It can be recycled or freed.
 *   - (NULL) if the mbuf still has remaining references on it.
 */
static __rte_always_inline struct rte_mbuf *
rte_pktmbuf_prefree_seg(struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 0);

	if (likely(rte_mbuf_refcnt_read(m) == 1)) {

		if (!RTE_MBUF_DIRECT(m)) {
			rte_pktmbuf_detach(m);
			if (RTE_MBUF_HAS_EXTBUF(m) &&
			    RTE_MBUF_HAS_PINNED_EXTBUF(m) &&
			    __rte_pktmbuf_pinned_extbuf_decref(m))
				return NULL;
		}

		if (m->next != NULL)
			m->next = NULL;
		if (m->nb_segs != 1)
			m->nb_segs = 1;

		return m;

	} else if (__rte_mbuf_refcnt_update(m, -1) == 0) {

		if (!RTE_MBUF_DIRECT(m)) {
			rte_pktmbuf_detach(m);
			if (RTE_MBUF_HAS_EXTBUF(m) &&
			    RTE_MBUF_HAS_PINNED_EXTBUF(m) &&
			    __rte_pktmbuf_pinned_extbuf_decref(m))
				return NULL;
		}

		if (m->next != NULL)
			m->next = NULL;
		if (m->nb_segs != 1)
			m->nb_segs = 1;
		rte_mbuf_refcnt_set(m, 1);

		return m;
	}
	return NULL;
}

/**
 * Free a segment of a packet mbuf into its original mempool.
 *
 * Free an mbuf, without parsing other segments in case of chained
 * buffers.
 *
 * @param m
 *   The packet mbuf segment to be freed.
 */
static __rte_always_inline void
rte_pktmbuf_free_seg(struct rte_mbuf *m)
{
	m = rte_pktmbuf_prefree_seg(m);
	if (likely(m != NULL))
		rte_mbuf_raw_free(m);
}

/**
 * Free a packet mbuf back into its original mempool.
 *
 * Free an mbuf, and all its segments in case of chained buffers. Each
 * segment is added back into its original mempool.
 *
 * @param m
 *   The packet mbuf to be freed. If NULL, the function does nothing.
 */
static inline void rte_pktmbuf_free(struct rte_mbuf *m)
{
	struct rte_mbuf *m_next;

	if (m != NULL)
		__rte_mbuf_sanity_check(m, 1);

	while (m != NULL) {
		m_next = m->next;
		rte_pktmbuf_free_seg(m);
		m = m_next;
	}
}

/**
 * Free a bulk of packet mbufs back into their original mempools.
 *
 * Free a bulk of mbufs, and all their segments in case of chained buffers.
 * Each segment is added back into its original mempool.
 *
 *  @param mbufs
 *    Array of pointers to packet mbufs.
 *    The array may contain NULL pointers.
 *  @param count
 *    Array size.
 */
void rte_pktmbuf_free_bulk(struct rte_mbuf **mbufs, unsigned int count);

/**
 * Create a "clone" of the given packet mbuf.
 *
 * Walks through all segments of the given packet mbuf, and for each of them:
 *  - Creates a new packet mbuf from the given pool.
 *  - Attaches newly created mbuf to the segment.
 * Then updates pkt_len and nb_segs of the "clone" packet mbuf to match values
 * from the original packet mbuf.
 *
 * @param md
 *   The packet mbuf to be cloned.
 * @param mp
 *   The mempool from which the "clone" mbufs are allocated.
 * @return
 *   - The pointer to the new "clone" mbuf on success.
 *   - NULL if allocation fails.
 */
struct rte_mbuf *
rte_pktmbuf_clone(struct rte_mbuf *md, struct rte_mempool *mp);

/**
 * Create a full copy of a given packet mbuf.
 *
 * Copies all the data from a given packet mbuf to a newly allocated
 * set of mbufs. The private data are is not copied.
 *
 * @param m
 *   The packet mbuf to be copied.
 * @param mp
 *   The mempool from which the "clone" mbufs are allocated.
 * @param offset
 *   The number of bytes to skip before copying.
 *   If the mbuf does not have that many bytes, it is an error
 *   and NULL is returned.
 * @param length
 *   The upper limit on bytes to copy.  Passing UINT32_MAX
 *   means all data (after offset).
 * @return
 *   - The pointer to the new "clone" mbuf on success.
 *   - NULL if allocation fails.
 */
struct rte_mbuf *
rte_pktmbuf_copy(const struct rte_mbuf *m, struct rte_mempool *mp,
		 uint32_t offset, uint32_t length);

/**
 * Adds given value to the refcnt of all packet mbuf segments.
 *
 * Walks through all segments of given packet mbuf and for each of them
 * invokes rte_mbuf_refcnt_update().
 *
 * @param m
 *   The packet mbuf whose refcnt to be updated.
 * @param v
 *   The value to add to the mbuf's segments refcnt.
 */
static inline void rte_pktmbuf_refcnt_update(struct rte_mbuf *m, int16_t v)
{
	__rte_mbuf_sanity_check(m, 1);

	do {
		rte_mbuf_refcnt_update(m, v);
	} while ((m = m->next) != NULL);
}

/**
 * Get the headroom in a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the headroom.
 */
static inline uint16_t rte_pktmbuf_headroom(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 0);
	return m->data_off;
}

/**
 * Get the tailroom of a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the tailroom.
 */
static inline uint16_t rte_pktmbuf_tailroom(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 0);
	return (uint16_t)(m->buf_len - rte_pktmbuf_headroom(m) -
			  m->data_len);
}

/**
 * Get the last segment of the packet.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The last segment of the given mbuf.
 */
static inline struct rte_mbuf *rte_pktmbuf_lastseg(struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	while (m->next != NULL)
		m = m->next;
	return m;
}

/**
 * A macro that returns the length of the packet.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_pkt_len(m) ((m)->pkt_len)

/**
 * A macro that returns the length of the segment.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_data_len(m) ((m)->data_len)

/**
 * Prepend len bytes to an mbuf data area.
 *
 * Returns a pointer to the new
 * data start address. If there is not enough headroom in the first
 * segment, the function will return NULL, without modifying the mbuf.
 *
 * @param m
 *   The pkt mbuf.
 * @param len
 *   The amount of data to prepend (in bytes).
 * @return
 *   A pointer to the start of the newly prepended data, or
 *   NULL if there is not enough headroom space in the first segment
 */
static inline char *rte_pktmbuf_prepend(struct rte_mbuf *m,
					uint16_t len)
{
	__rte_mbuf_sanity_check(m, 1);

	if (unlikely(len > rte_pktmbuf_headroom(m)))
		return NULL;

	/* NB: elaborating the subtraction like this instead of using
	 *     -= allows us to ensure the result type is uint16_t
	 *     avoiding compiler warnings on gcc 8.1 at least */
	m->data_off = (uint16_t)(m->data_off - len);
	m->data_len = (uint16_t)(m->data_len + len);
	m->pkt_len  = (m->pkt_len + len);

	return (char *)m->buf_addr + m->data_off;
}

/**
 * Append len bytes to an mbuf.
 *
 * Append len bytes to an mbuf and return a pointer to the start address
 * of the added data. If there is not enough tailroom in the last
 * segment, the function will return NULL, without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to append (in bytes).
 * @return
 *   A pointer to the start of the newly appended data, or
 *   NULL if there is not enough tailroom space in the last segment
 */
static inline char *rte_pktmbuf_append(struct rte_mbuf *m, uint16_t len)
{
	void *tail;
	struct rte_mbuf *m_last;

	__rte_mbuf_sanity_check(m, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > rte_pktmbuf_tailroom(m_last)))
		return NULL;

	tail = (char *)m_last->buf_addr + m_last->data_off + m_last->data_len;
	m_last->data_len = (uint16_t)(m_last->data_len + len);
	m->pkt_len  = (m->pkt_len + len);
	return (char*) tail;
}

/**
 * Remove len bytes at the beginning of an mbuf.
 *
 * Returns a pointer to the start address of the new data area. If the
 * length is greater than the length of the first segment, then the
 * function will fail and return NULL, without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to remove (in bytes).
 * @return
 *   A pointer to the new start of the data.
 */
static inline char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len)
{
	__rte_mbuf_sanity_check(m, 1);

	if (unlikely(len > m->data_len))
		return NULL;

	/* NB: elaborating the addition like this instead of using
	 *     += allows us to ensure the result type is uint16_t
	 *     avoiding compiler warnings on gcc 8.1 at least */
	m->data_len = (uint16_t)(m->data_len - len);
	m->data_off = (uint16_t)(m->data_off + len);
	m->pkt_len  = (m->pkt_len - len);
	return (char *)m->buf_addr + m->data_off;
}

/**
 * Remove len bytes of data at the end of the mbuf.
 *
 * If the length is greater than the length of the last segment, the
 * function will fail and return -1 without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to remove (in bytes).
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static inline int rte_pktmbuf_trim(struct rte_mbuf *m, uint16_t len)
{
	struct rte_mbuf *m_last;

	__rte_mbuf_sanity_check(m, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > m_last->data_len))
		return -1;

	m_last->data_len = (uint16_t)(m_last->data_len - len);
	m->pkt_len  = (m->pkt_len - len);
	return 0;
}

/**
 * Test if mbuf data is contiguous.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 1, if all data is contiguous (one segment).
 *   - 0, if there is several segments.
 */
static inline int rte_pktmbuf_is_contiguous(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return m->nb_segs == 1;
}

/**
 * @internal used by rte_pktmbuf_read().
 */
const void *__rte_pktmbuf_read(const struct rte_mbuf *m, uint32_t off,
	uint32_t len, void *buf);

/**
 * Read len data bytes in a mbuf at specified offset.
 *
 * If the data is contiguous, return the pointer in the mbuf data, else
 * copy the data in the buffer provided by the user and return its
 * pointer.
 *
 * @param m
 *   The pointer to the mbuf.
 * @param off
 *   The offset of the data in the mbuf.
 * @param len
 *   The amount of bytes to read.
 * @param buf
 *   The buffer where data is copied if it is not contiguous in mbuf
 *   data. Its length should be at least equal to the len parameter.
 * @return
 *   The pointer to the data, either in the mbuf if it is contiguous,
 *   or in the user buffer. If mbuf is too small, NULL is returned.
 */
static inline const void *rte_pktmbuf_read(const struct rte_mbuf *m,
	uint32_t off, uint32_t len, void *buf)
{
	if (likely(off + len <= rte_pktmbuf_data_len(m)))
		return rte_pktmbuf_mtod_offset(m, char *, off);
	else
		return __rte_pktmbuf_read(m, off, len, buf);
}

/**
 * Chain an mbuf to another, thereby creating a segmented packet.
 *
 * Note: The implementation will do a linear walk over the segments to find
 * the tail entry. For cases when there are many segments, it's better to
 * chain the entries manually.
 *
 * @param head
 *   The head of the mbuf chain (the first packet)
 * @param tail
 *   The mbuf to put last in the chain
 *
 * @return
 *   - 0, on success.
 *   - -EOVERFLOW, if the chain segment limit exceeded
 */
static inline int rte_pktmbuf_chain(struct rte_mbuf *head, struct rte_mbuf *tail)
{
	struct rte_mbuf *cur_tail;

	/* Check for number-of-segments-overflow */
	if (head->nb_segs + tail->nb_segs > RTE_MBUF_MAX_NB_SEGS)
		return -EOVERFLOW;

	/* Chain 'tail' onto the old tail */
	cur_tail = rte_pktmbuf_lastseg(head);
	cur_tail->next = tail;

	/* accumulate number of segments and total length.
	 * NB: elaborating the addition like this instead of using
	 *     -= allows us to ensure the result type is uint16_t
	 *     avoiding compiler warnings on gcc 8.1 at least */
	head->nb_segs = (uint16_t)(head->nb_segs + tail->nb_segs);
	head->pkt_len += tail->pkt_len;

	/* pkt_len is only set in the head */
	tail->pkt_len = tail->data_len;

	return 0;
}

/**
 * For given input values generate raw tx_offload value.
 * Note that it is caller responsibility to make sure that input parameters
 * don't exceed maximum bit-field values.
 * @param il2
 *   l2_len value.
 * @param il3
 *   l3_len value.
 * @param il4
 *   l4_len value.
 * @param tso
 *   tso_segsz value.
 * @param ol3
 *   outer_l3_len value.
 * @param ol2
 *   outer_l2_len value.
 * @param unused
 *   unused value.
 * @return
 *   raw tx_offload value.
 */
static __rte_always_inline uint64_t
rte_mbuf_tx_offload(uint64_t il2, uint64_t il3, uint64_t il4, uint64_t tso,
	uint64_t ol3, uint64_t ol2, uint64_t unused)
{
	return il2 << RTE_MBUF_L2_LEN_OFS |
		il3 << RTE_MBUF_L3_LEN_OFS |
		il4 << RTE_MBUF_L4_LEN_OFS |
		tso << RTE_MBUF_TSO_SEGSZ_OFS |
		ol3 << RTE_MBUF_OUTL3_LEN_OFS |
		ol2 << RTE_MBUF_OUTL2_LEN_OFS |
		unused << RTE_MBUF_TXOFLD_UNUSED_OFS;
}

/**
 * Validate general requirements for Tx offload in mbuf.
 *
 * This function checks correctness and completeness of Tx offload settings.
 *
 * @param m
 *   The packet mbuf to be validated.
 * @return
 *   0 if packet is valid
 */
static inline int
rte_validate_tx_offload(const struct rte_mbuf *m)
{
	uint64_t ol_flags = m->ol_flags;

	/* Does packet set any of available offloads? */
	if (!(ol_flags & RTE_MBUF_F_TX_OFFLOAD_MASK))
		return 0;

	/* IP checksum can be counted only for IPv4 packet */
	if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM) && (ol_flags & RTE_MBUF_F_TX_IPV6))
		return -EINVAL;

	/* IP type not set when required */
	if (ol_flags & (RTE_MBUF_F_TX_L4_MASK | RTE_MBUF_F_TX_TCP_SEG))
		if (!(ol_flags & (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IPV6)))
			return -EINVAL;

	/* Check requirements for TSO packet */
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		if ((m->tso_segsz == 0) ||
				((ol_flags & RTE_MBUF_F_TX_IPV4) &&
				 !(ol_flags & RTE_MBUF_F_TX_IP_CKSUM)))
			return -EINVAL;

	/* RTE_MBUF_F_TX_OUTER_IP_CKSUM set for non outer IPv4 packet. */
	if ((ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) &&
			!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV4))
		return -EINVAL;

	return 0;
}

/**
 * @internal used by rte_pktmbuf_linearize().
 */
int __rte_pktmbuf_linearize(struct rte_mbuf *mbuf);

/**
 * Linearize data in mbuf.
 *
 * This function moves the mbuf data in the first segment if there is enough
 * tailroom. The subsequent segments are unchained and freed.
 *
 * @param mbuf
 *   mbuf to linearize
 * @return
 *   - 0, on success
 *   - -1, on error
 */
static inline int
rte_pktmbuf_linearize(struct rte_mbuf *mbuf)
{
	if (rte_pktmbuf_is_contiguous(mbuf))
		return 0;
	return __rte_pktmbuf_linearize(mbuf);
}

/**
 * Dump an mbuf structure to a file.
 *
 * Dump all fields for the given packet mbuf and all its associated
 * segments (in the case of a chained buffer).
 *
 * @param f
 *   A pointer to a file for output
 * @param m
 *   The packet mbuf.
 * @param dump_len
 *   If dump_len != 0, also dump the "dump_len" first data bytes of
 *   the packet.
 */
void rte_pktmbuf_dump(FILE *f, const struct rte_mbuf *m, unsigned dump_len);

/**
 * Get the value of mbuf sched queue_id field.
 */
static inline uint32_t
rte_mbuf_sched_queue_get(const struct rte_mbuf *m)
{
	return m->hash.sched.queue_id;
}

/**
 * Get the value of mbuf sched traffic_class field.
 */
static inline uint8_t
rte_mbuf_sched_traffic_class_get(const struct rte_mbuf *m)
{
	return m->hash.sched.traffic_class;
}

/**
 * Get the value of mbuf sched color field.
 */
static inline uint8_t
rte_mbuf_sched_color_get(const struct rte_mbuf *m)
{
	return m->hash.sched.color;
}

/**
 * Get the values of mbuf sched queue_id, traffic_class and color.
 *
 * @param m
 *   Mbuf to read
 * @param queue_id
 *  Returns the queue id
 * @param traffic_class
 *  Returns the traffic class id
 * @param color
 *  Returns the colour id
 */
static inline void
rte_mbuf_sched_get(const struct rte_mbuf *m, uint32_t *queue_id,
			uint8_t *traffic_class,
			uint8_t *color)
{
	struct rte_mbuf_sched sched = m->hash.sched;

	*queue_id = sched.queue_id;
	*traffic_class = sched.traffic_class;
	*color = sched.color;
}

/**
 * Set the mbuf sched queue_id to the defined value.
 */
static inline void
rte_mbuf_sched_queue_set(struct rte_mbuf *m, uint32_t queue_id)
{
	m->hash.sched.queue_id = queue_id;
}

/**
 * Set the mbuf sched traffic_class id to the defined value.
 */
static inline void
rte_mbuf_sched_traffic_class_set(struct rte_mbuf *m, uint8_t traffic_class)
{
	m->hash.sched.traffic_class = traffic_class;
}

/**
 * Set the mbuf sched color id to the defined value.
 */
static inline void
rte_mbuf_sched_color_set(struct rte_mbuf *m, uint8_t color)
{
	m->hash.sched.color = color;
}

/**
 * Set the mbuf sched queue_id, traffic_class and color.
 *
 * @param m
 *   Mbuf to set
 * @param queue_id
 *  Queue id value to be set
 * @param traffic_class
 *  Traffic class id value to be set
 * @param color
 *  Color id to be set
 */
static inline void
rte_mbuf_sched_set(struct rte_mbuf *m, uint32_t queue_id,
			uint8_t traffic_class,
			uint8_t color)
{
	m->hash.sched = (struct rte_mbuf_sched){
				.queue_id = queue_id,
				.traffic_class = traffic_class,
				.color = color,
				.reserved = 0,
			};
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_H_ */
