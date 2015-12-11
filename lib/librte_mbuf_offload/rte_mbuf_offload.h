/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

#ifndef _RTE_MBUF_OFFLOAD_H_
#define _RTE_MBUF_OFFLOAD_H_

/**
 * @file
 * RTE mbuf offload
 *
 * The rte_mbuf_offload library provides the ability to specify a device generic
 * off-load operation independent of the current Rx/Tx Ethernet offloads
 * supported within the rte_mbuf structure, and add supports for multiple
 * off-load operations and offload device types.
 *
 * The rte_mbuf_offload specifies the particular off-load operation type, such
 * as a crypto operation, and provides a container for the operations
 * parameter's inside the op union. These parameters are then used by the
 * device which supports that operation to perform the specified offload.
 *
 * This library provides an API to create pre-allocated mempool of offload
 * operations, with supporting allocate and free functions. It also provides
 * APIs for attaching an offload to a mbuf, as well as an API to retrieve a
 * specified offload type from an mbuf offload chain.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#include <rte_mbuf.h>
#include <rte_crypto.h>


/** packet mbuf offload operation types */
enum rte_mbuf_ol_op_type {
	RTE_PKTMBUF_OL_NOT_SPECIFIED = 0,
	/**< Off-load not specified */
	RTE_PKTMBUF_OL_CRYPTO
	/**< Crypto offload operation */
};

/**
 * Generic packet mbuf offload
 * This is used to specify a offload operation to be performed on a rte_mbuf.
 * Multiple offload operations can be chained to the same mbuf, but only a
 * single offload operation of a particular type can be in the chain
 */
struct rte_mbuf_offload {
	struct rte_mbuf_offload *next;	/**< next offload in chain */
	struct rte_mbuf *m;		/**< mbuf offload is attached to */
	struct rte_mempool *mp;		/**< mempool offload allocated from */

	enum rte_mbuf_ol_op_type type;	/**< offload type */
	union {
		struct rte_crypto_op crypto;	/**< Crypto operation */
	} op;
};

/**< private data structure belonging to packet mbug offload mempool */
struct rte_pktmbuf_offload_pool_private {
	uint16_t offload_priv_size;
	/**< Size of private area in each mbuf_offload. */
};


/**
 * Creates a mempool of rte_mbuf_offload objects
 *
 * EXPERIMENTAL: this API file may change without prior notice
 *
 * @param	name		mempool name
 * @param	size		number of objects in mempool
 * @param	cache_size	cache size of objects for each core
 * @param	priv_size	size of private data to be allocated with each
 *				rte_mbuf_offload object
 * @param	socket_id	Socket on which to allocate mempool objects
 *
 * @return
 * - On success returns a valid mempool of rte_mbuf_offload objects
 * - On failure return NULL
 */
extern struct rte_mempool *
rte_pktmbuf_offload_pool_create(const char *name, unsigned size,
		unsigned cache_size, uint16_t priv_size, int socket_id);


/**
 * Returns private data size allocated with each rte_mbuf_offload object by
 * the mempool
 *
 * @param	mpool	rte_mbuf_offload mempool
 *
 * @return	private data size
 */
static inline uint16_t
__rte_pktmbuf_offload_priv_size(struct rte_mempool *mpool)
{
	struct rte_pktmbuf_offload_pool_private *priv =
			rte_mempool_get_priv(mpool);

	return priv->offload_priv_size;
}

/**
 * Get specified off-load operation type from mbuf.
 *
 * @param	m		packet mbuf.
 * @param	type		offload operation type requested.
 *
 * @return
 * - On success retruns rte_mbuf_offload pointer
 * - On failure returns NULL
 *
 */
static inline struct rte_mbuf_offload *
rte_pktmbuf_offload_get(struct rte_mbuf *m, enum rte_mbuf_ol_op_type type)
{
	struct rte_mbuf_offload *ol;

	for (ol = m->offload_ops; ol != NULL; ol = ol->next)
		if (ol->type == type)
			return ol;

	return ol;
}

/**
 * Attach a rte_mbuf_offload to a mbuf. We only support a single offload of any
 * one type in our chain of offloads.
 *
 * @param	m	packet mbuf.
 * @param	ol	rte_mbuf_offload strucutre to be attached
 *
 * @returns
 * - On success returns the pointer to the offload we just added
 * - On failure returns NULL
 */
static inline struct rte_mbuf_offload *
rte_pktmbuf_offload_attach(struct rte_mbuf *m, struct rte_mbuf_offload *ol)
{
	struct rte_mbuf_offload **ol_last;

	for (ol_last = &m->offload_ops;	ol_last[0] != NULL;
			ol_last = &ol_last[0]->next)
		if (ol_last[0]->type == ol->type)
			return NULL;

	ol_last[0] = ol;
	ol_last[0]->m = m;
	ol_last[0]->next = NULL;

	return ol_last[0];
}


/** Rearms rte_mbuf_offload default parameters */
static inline void
__rte_pktmbuf_offload_reset(struct rte_mbuf_offload *ol,
		enum rte_mbuf_ol_op_type type)
{
	ol->m = NULL;
	ol->type = type;

	switch (type) {
	case RTE_PKTMBUF_OL_CRYPTO:
		__rte_crypto_op_reset(&ol->op.crypto); break;
	default:
		break;
	}
}

/** Allocate rte_mbuf_offload from mempool */
static inline struct rte_mbuf_offload *
__rte_pktmbuf_offload_raw_alloc(struct rte_mempool *mp)
{
	void *buf = NULL;

	if (rte_mempool_get(mp, &buf) < 0)
		return NULL;

	return (struct rte_mbuf_offload *)buf;
}

/**
 * Allocate a rte_mbuf_offload with a specified operation type from
 * rte_mbuf_offload mempool
 *
 * @param	mpool		rte_mbuf_offload mempool
 * @param	type		offload operation type
 *
 * @returns
 * - On success returns a valid rte_mbuf_offload structure
 * - On failure returns NULL
 */
static inline struct rte_mbuf_offload *
rte_pktmbuf_offload_alloc(struct rte_mempool *mpool,
		enum rte_mbuf_ol_op_type type)
{
	struct rte_mbuf_offload *ol = __rte_pktmbuf_offload_raw_alloc(mpool);

	if (ol != NULL)
		__rte_pktmbuf_offload_reset(ol, type);

	return ol;
}

/**
 * free rte_mbuf_offload structure
 */
static inline void
rte_pktmbuf_offload_free(struct rte_mbuf_offload *ol)
{
	if (ol != NULL && ol->mp != NULL)
		rte_mempool_put(ol->mp, ol);
}

/**
 * Checks if the private data of a rte_mbuf_offload has enough capacity for
 * requested size
 *
 * @returns
 * - if sufficient space available returns pointer to start of private data
 * - if insufficient space returns NULL
 */
static inline void *
__rte_pktmbuf_offload_check_priv_data_size(struct rte_mbuf_offload *ol,
		uint16_t size)
{
	uint16_t priv_size;

	if (likely(ol->mp != NULL)) {
		priv_size = __rte_pktmbuf_offload_priv_size(ol->mp);

		if (likely(priv_size >= size))
			return (void *)(ol + 1);
	}
	return NULL;
}

/**
 * Allocate space for crypto xforms in the private data space of the
 * rte_mbuf_offload. This also defaults the crypto xform type and configures
 * the chaining of the xform in the crypto operation
 *
 * @return
 * - On success returns pointer to first crypto xform in crypto operations chain
 * - On failure returns NULL
 */
static inline struct rte_crypto_xform *
rte_pktmbuf_offload_alloc_crypto_xforms(struct rte_mbuf_offload *ol,
		unsigned nb_xforms)
{
	struct rte_crypto_xform *xform;
	void *priv_data;
	uint16_t size;

	size = sizeof(struct rte_crypto_xform) * nb_xforms;
	priv_data = __rte_pktmbuf_offload_check_priv_data_size(ol, size);

	if (priv_data == NULL)
		return NULL;

	ol->op.crypto.xform = xform = (struct rte_crypto_xform *)priv_data;

	do {
		xform->type = RTE_CRYPTO_XFORM_NOT_SPECIFIED;
		xform = xform->next = --nb_xforms > 0 ? xform + 1 : NULL;
	} while (xform);

	return ol->op.crypto.xform;
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_OFFLOAD_H_ */
