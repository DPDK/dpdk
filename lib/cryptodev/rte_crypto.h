/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _RTE_CRYPTO_H_
#define _RTE_CRYPTO_H_

/**
 * @file rte_crypto.h
 *
 * RTE Cryptography Common Definitions
 *
 */

#ifdef __cplusplus
extern "C" {
#endif


#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_common.h>

#include "rte_crypto_sym.h"
#include "rte_crypto_asym.h"

/** Crypto operation types */
enum rte_crypto_op_type {
	RTE_CRYPTO_OP_TYPE_UNDEFINED,
	/**< Undefined operation type */
	RTE_CRYPTO_OP_TYPE_SYMMETRIC,
	/**< Symmetric operation */
	RTE_CRYPTO_OP_TYPE_ASYMMETRIC
	/**< Asymmetric operation */
};

/** Status of crypto operation */
enum rte_crypto_op_status {
	RTE_CRYPTO_OP_STATUS_SUCCESS,
	/**< Operation completed successfully */
	RTE_CRYPTO_OP_STATUS_NOT_PROCESSED,
	/**< Operation has not yet been processed by a crypto device */
	RTE_CRYPTO_OP_STATUS_AUTH_FAILED,
	/**< Authentication verification failed */
	RTE_CRYPTO_OP_STATUS_INVALID_SESSION,
	/**<
	 * Symmetric operation failed due to invalid session arguments, or if
	 * in session-less mode, failed to allocate private operation material.
	 */
	RTE_CRYPTO_OP_STATUS_INVALID_ARGS,
	/**< Operation failed due to invalid arguments in request */
	RTE_CRYPTO_OP_STATUS_ERROR,
	/**< Error handling operation */
};

/**
 * Crypto operation session type. This is used to specify whether a crypto
 * operation has session structure attached for immutable parameters or if all
 * operation information is included in the operation data structure.
 */
enum rte_crypto_op_sess_type {
	RTE_CRYPTO_OP_WITH_SESSION,	/**< Session based crypto operation */
	RTE_CRYPTO_OP_SESSIONLESS,	/**< Session-less crypto operation */
	RTE_CRYPTO_OP_SECURITY_SESSION	/**< Security session crypto operation */
};

/**
 * Cryptographic Operation.
 *
 * This structure contains data relating to performing cryptographic
 * operations. This operation structure is used to contain any operation which
 * is supported by the cryptodev API, PMDs should check the type parameter to
 * verify that the operation is a support function of the device. Crypto
 * operations are enqueued and dequeued in crypto PMDs using the
 * rte_cryptodev_enqueue_burst() / rte_cryptodev_dequeue_burst() .
 */
struct rte_crypto_op {
	__extension__
	union {
		uint64_t raw;
		__extension__
		struct {
			uint8_t type;
			/**< operation type */
			uint8_t status;
			/**<
			 * operation status - this is reset to
			 * RTE_CRYPTO_OP_STATUS_NOT_PROCESSED on allocation
			 * from mempool and will be set to
			 * RTE_CRYPTO_OP_STATUS_SUCCESS after crypto operation
			 * is successfully processed by a crypto PMD
			 */
			uint8_t sess_type;
			/**< operation session type */
			uint8_t reserved[3];
			/**< Reserved bytes to fill 64 bits for
			 * future additions
			 */
			uint16_t private_data_offset;
			/**< Offset to indicate start of private data (if any).
			 * The offset is counted from the start of the
			 * rte_crypto_op including IV.
			 * The private data may be used by the application
			 * to store information which should remain untouched
			 * in the library/driver
			 */
		};
	};
	struct rte_mempool *mempool;
	/**< crypto operation mempool which operation is allocated from */

	rte_iova_t phys_addr;
	/**< physical address of crypto operation */

	__extension__
	union {
		struct rte_crypto_sym_op sym[0];
		/**< Symmetric operation parameters */

		struct rte_crypto_asym_op asym[0];
		/**< Asymmetric operation parameters */

	}; /**< operation specific parameters */
};

/**
 * Reset the fields of a crypto operation to their default values.
 *
 * @param	op	The crypto operation to be reset.
 * @param	type	The crypto operation type.
 */
static inline void
__rte_crypto_op_reset(struct rte_crypto_op *op, enum rte_crypto_op_type type)
{
	op->type = type;
	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	op->sess_type = RTE_CRYPTO_OP_SESSIONLESS;

	switch (type) {
	case RTE_CRYPTO_OP_TYPE_SYMMETRIC:
		__rte_crypto_sym_op_reset(op->sym);
		break;
	case RTE_CRYPTO_OP_TYPE_ASYMMETRIC:
		memset(op->asym, 0, sizeof(struct rte_crypto_asym_op));
	break;
	case RTE_CRYPTO_OP_TYPE_UNDEFINED:
	default:
		break;
	}
}

/**
 * Private data structure belonging to a crypto symmetric operation pool.
 */
struct rte_crypto_op_pool_private {
	enum rte_crypto_op_type type;
	/**< Crypto op pool type operation. */
	uint16_t priv_size;
	/**< Size of private area in each crypto operation. */
};


/**
 * Returns the size of private data allocated with each rte_crypto_op object by
 * the mempool
 *
 * @param	mempool	rte_crypto_op mempool
 *
 * @return	private data size
 */
static inline uint16_t
__rte_crypto_op_get_priv_data_size(struct rte_mempool *mempool)
{
	struct rte_crypto_op_pool_private *priv =
		(struct rte_crypto_op_pool_private *) rte_mempool_get_priv(mempool);

	return priv->priv_size;
}


/**
 * Creates a crypto operation pool
 *
 * @param	name		pool name
 * @param	type		crypto operation type, use
 *				RTE_CRYPTO_OP_TYPE_UNDEFINED for a pool which
 *				supports all operation types
 * @param	nb_elts		number of elements in pool
 * @param	cache_size	Number of elements to cache on lcore, see
 *				*rte_mempool_create* for further details about
 *				cache size
 * @param	priv_size	Size of private data to allocate with each
 *				operation
 * @param	socket_id	Socket to allocate memory on
 *
 * @return
 *  - On success pointer to mempool
 *  - On failure NULL
 */
extern struct rte_mempool *
rte_crypto_op_pool_create(const char *name, enum rte_crypto_op_type type,
		unsigned nb_elts, unsigned cache_size, uint16_t priv_size,
		int socket_id);

/**
 * Bulk allocate raw element from mempool and return as crypto operations
 *
 * @param	mempool		crypto operation mempool.
 * @param	type		crypto operation type.
 * @param	ops		Array to place allocated crypto operations
 * @param	nb_ops		Number of crypto operations to allocate
 *
 * @returns
 * - On success returns  number of ops allocated
 */
static inline int
__rte_crypto_op_raw_bulk_alloc(struct rte_mempool *mempool,
		enum rte_crypto_op_type type,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_crypto_op_pool_private *priv;

	priv = (struct rte_crypto_op_pool_private *) rte_mempool_get_priv(mempool);
	if (unlikely(priv->type != type &&
			priv->type != RTE_CRYPTO_OP_TYPE_UNDEFINED))
		return -EINVAL;

	if (rte_mempool_get_bulk(mempool, (void **)ops, nb_ops) == 0)
		return nb_ops;

	return 0;
}

/**
 * Allocate a crypto operation from a mempool with default parameters set
 *
 * @param	mempool	crypto operation mempool
 * @param	type	operation type to allocate
 *
 * @returns
 * - On success returns a valid rte_crypto_op structure
 * - On failure returns NULL
 */
static inline struct rte_crypto_op *
rte_crypto_op_alloc(struct rte_mempool *mempool, enum rte_crypto_op_type type)
{
	struct rte_crypto_op *op = NULL;
	int retval;

	retval = __rte_crypto_op_raw_bulk_alloc(mempool, type, &op, 1);
	if (unlikely(retval != 1))
		return NULL;

	__rte_crypto_op_reset(op, type);

	return op;
}


/**
 * Bulk allocate crypto operations from a mempool with default parameters set
 *
 * @param	mempool	crypto operation mempool
 * @param	type	operation type to allocate
 * @param	ops	Array to place allocated crypto operations
 * @param	nb_ops	Number of crypto operations to allocate
 *
 * @returns
 * - nb_ops if the number of operations requested were allocated.
 * - 0 if the requested number of ops are not available.
 *   None are allocated in this case.
 */

static inline unsigned
rte_crypto_op_bulk_alloc(struct rte_mempool *mempool,
		enum rte_crypto_op_type type,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	int i;

	if (unlikely(__rte_crypto_op_raw_bulk_alloc(mempool, type, ops, nb_ops)
			!= nb_ops))
		return 0;

	for (i = 0; i < nb_ops; i++)
		__rte_crypto_op_reset(ops[i], type);

	return nb_ops;
}



/**
 * Returns a pointer to the private data of a crypto operation if
 * that operation has enough capacity for requested size.
 *
 * @param	op	crypto operation.
 * @param	size	size of space requested in private data.
 *
 * @returns
 * - if sufficient space available returns pointer to start of private data
 * - if insufficient space returns NULL
 */
static inline void *
__rte_crypto_op_get_priv_data(struct rte_crypto_op *op, uint32_t size)
{
	uint32_t priv_size;

	if (likely(op->mempool != NULL)) {
		priv_size = __rte_crypto_op_get_priv_data_size(op->mempool);

		if (likely(priv_size >= size)) {
			if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC)
				return (void *)((uint8_t *)(op + 1) +
					sizeof(struct rte_crypto_sym_op));
			if (op->type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC)
				return (void *)((uint8_t *)(op + 1) +
					sizeof(struct rte_crypto_asym_op));
		}
	}

	return NULL;
}

/**
 * free crypto operation structure
 * If operation has been allocate from a rte_mempool, then the operation will
 * be returned to the mempool.
 *
 * @param	op	symmetric crypto operation
 */
static inline void
rte_crypto_op_free(struct rte_crypto_op *op)
{
	if (op != NULL && op->mempool != NULL)
		rte_mempool_put(op->mempool, op);
}

/**
 * Allocate a symmetric crypto operation in the private data of an mbuf.
 *
 * @param	m	mbuf which is associated with the crypto operation, the
 *			operation will be allocated in the private data of that
 *			mbuf.
 *
 * @returns
 * - On success returns a pointer to the crypto operation.
 * - On failure returns NULL.
 */
static inline struct rte_crypto_op *
rte_crypto_sym_op_alloc_from_mbuf_priv_data(struct rte_mbuf *m)
{
	if (unlikely(m == NULL))
		return NULL;

	/*
	 * check that the mbuf's private data size is sufficient to contain a
	 * crypto operation
	 */
	if (unlikely(m->priv_size < (sizeof(struct rte_crypto_op) +
			sizeof(struct rte_crypto_sym_op))))
		return NULL;

	/* private data starts immediately after the mbuf header in the mbuf. */
	struct rte_crypto_op *op = (struct rte_crypto_op *)(m + 1);

	__rte_crypto_op_reset(op, RTE_CRYPTO_OP_TYPE_SYMMETRIC);

	op->mempool = NULL;
	op->sym->m_src = m;

	return op;
}

/**
 * Allocate space for symmetric crypto xforms in the private data space of the
 * crypto operation. This also defaults the crypto xform type and configures
 * the chaining of the xforms in the crypto operation
 *
 * @return
 * - On success returns pointer to first crypto xform in crypto operations chain
 * - On failure returns NULL
 */
static inline struct rte_crypto_sym_xform *
rte_crypto_op_sym_xforms_alloc(struct rte_crypto_op *op, uint8_t nb_xforms)
{
	void *priv_data;
	uint32_t size;

	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_SYMMETRIC))
		return NULL;

	size = sizeof(struct rte_crypto_sym_xform) * nb_xforms;

	priv_data = __rte_crypto_op_get_priv_data(op, size);
	if (priv_data == NULL)
		return NULL;

	return __rte_crypto_sym_op_sym_xforms_alloc(op->sym, priv_data,
			nb_xforms);
}


/**
 * Attach a session to a crypto operation
 *
 * @param	op	crypto operation, must be of type symmetric
 * @param	sess	cryptodev session
 */
static inline int
rte_crypto_op_attach_sym_session(struct rte_crypto_op *op,
		struct rte_cryptodev_sym_session *sess)
{
	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_SYMMETRIC))
		return -1;

	op->sess_type = RTE_CRYPTO_OP_WITH_SESSION;

	return __rte_crypto_sym_op_attach_sym_session(op->sym, sess);
}

/**
 * Attach a asymmetric session to a crypto operation
 *
 * @param	op	crypto operation, must be of type asymmetric
 * @param	sess	cryptodev session
 */
static inline int
rte_crypto_op_attach_asym_session(struct rte_crypto_op *op,
		struct rte_cryptodev_asym_session *sess)
{
	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_ASYMMETRIC))
		return -1;

	op->sess_type = RTE_CRYPTO_OP_WITH_SESSION;
	op->asym->session = sess;
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTO_H_ */
