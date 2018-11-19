/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include "rte_comp.h"
#include "rte_compressdev.h"
#include "rte_compressdev_internal.h"

const char * __rte_experimental
rte_comp_get_feature_name(uint64_t flag)
{
	switch (flag) {
	case RTE_COMP_FF_STATEFUL_COMPRESSION:
		return "STATEFUL_COMPRESSION";
	case RTE_COMP_FF_STATEFUL_DECOMPRESSION:
		return "STATEFUL_DECOMPRESSION";
	case RTE_COMP_FF_OOP_SGL_IN_SGL_OUT:
		return "OOP_SGL_IN_SGL_OUT";
	case RTE_COMP_FF_OOP_SGL_IN_LB_OUT:
		return "OOP_SGL_IN_LB_OUT";
	case RTE_COMP_FF_OOP_LB_IN_SGL_OUT:
		return "OOP_LB_IN_SGL_OUT";
	case RTE_COMP_FF_MULTI_PKT_CHECKSUM:
		return "MULTI_PKT_CHECKSUM";
	case RTE_COMP_FF_ADLER32_CHECKSUM:
		return "ADLER32_CHECKSUM";
	case RTE_COMP_FF_CRC32_CHECKSUM:
		return "CRC32_CHECKSUM";
	case RTE_COMP_FF_CRC32_ADLER32_CHECKSUM:
		return "CRC32_ADLER32_CHECKSUM";
	case RTE_COMP_FF_NONCOMPRESSED_BLOCKS:
		return "NONCOMPRESSED_BLOCKS";
	case RTE_COMP_FF_SHA1_HASH:
		return "SHA1_HASH";
	case RTE_COMP_FF_SHA2_SHA256_HASH:
		return "SHA2_SHA256_HASH";
	case RTE_COMP_FF_SHAREABLE_PRIV_XFORM:
		return "SHAREABLE_PRIV_XFORM";
	case RTE_COMP_FF_HUFFMAN_FIXED:
		return "HUFFMAN_FIXED";
	case RTE_COMP_FF_HUFFMAN_DYNAMIC:
		return "HUFFMAN_DYNAMIC";
	default:
		return NULL;
	}
}

/**
 * Reset the fields of an operation to their default values.
 *
 * @note The private data associated with the operation is not zeroed.
 *
 * @param op
 *   The operation to be reset
 */
static inline void
rte_comp_op_reset(struct rte_comp_op *op)
{
	struct rte_mempool *tmp_mp = op->mempool;
	rte_iova_t tmp_iova_addr = op->iova_addr;

	memset(op, 0, sizeof(struct rte_comp_op));
	op->status = RTE_COMP_OP_STATUS_NOT_PROCESSED;
	op->iova_addr = tmp_iova_addr;
	op->mempool = tmp_mp;
}

/**
 * Private data structure belonging to an operation pool.
 */
struct rte_comp_op_pool_private {
	uint16_t user_size;
	/**< Size of private user data with each operation. */
};

/**
 * Bulk allocate raw element from mempool and return as comp operations
 *
 * @param mempool
 *   Compress operation mempool
 * @param ops
 *   Array to place allocated operations
 * @param nb_ops
 *   Number of operations to allocate
 * @return
 *   - nb_ops: Success, the nb_ops requested was allocated
 *   - 0: Not enough entries in the mempool; no ops are retrieved.
 */
static inline int
rte_comp_op_raw_bulk_alloc(struct rte_mempool *mempool,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	if (rte_mempool_get_bulk(mempool, (void **)ops, nb_ops) == 0)
		return nb_ops;

	return 0;
}

/** Initialise rte_comp_op mempool element */
static void
rte_comp_op_init(struct rte_mempool *mempool,
		__rte_unused void *opaque_arg,
		void *_op_data,
		__rte_unused unsigned int i)
{
	struct rte_comp_op *op = _op_data;

	memset(_op_data, 0, mempool->elt_size);

	op->status = RTE_COMP_OP_STATUS_NOT_PROCESSED;
	op->iova_addr = rte_mem_virt2iova(_op_data);
	op->mempool = mempool;
}

struct rte_mempool * __rte_experimental
rte_comp_op_pool_create(const char *name,
		unsigned int nb_elts, unsigned int cache_size,
		uint16_t user_size, int socket_id)
{
	struct rte_comp_op_pool_private *priv;

	unsigned int elt_size = sizeof(struct rte_comp_op) + user_size;

	/* lookup mempool in case already allocated */
	struct rte_mempool *mp = rte_mempool_lookup(name);

	if (mp != NULL) {
		priv = (struct rte_comp_op_pool_private *)
				rte_mempool_get_priv(mp);

		if (mp->elt_size != elt_size ||
				mp->cache_size < cache_size ||
				mp->size < nb_elts ||
				priv->user_size <  user_size) {
			mp = NULL;
			COMPRESSDEV_LOG(ERR,
		"Mempool %s already exists but with incompatible parameters",
					name);
			return NULL;
		}
		return mp;
	}

	mp = rte_mempool_create(
			name,
			nb_elts,
			elt_size,
			cache_size,
			sizeof(struct rte_comp_op_pool_private),
			NULL,
			NULL,
			rte_comp_op_init,
			NULL,
			socket_id,
			0);

	if (mp == NULL) {
		COMPRESSDEV_LOG(ERR, "Failed to create mempool %s", name);
		return NULL;
	}

	priv = (struct rte_comp_op_pool_private *)
			rte_mempool_get_priv(mp);

	priv->user_size = user_size;

	return mp;
}

struct rte_comp_op * __rte_experimental
rte_comp_op_alloc(struct rte_mempool *mempool)
{
	struct rte_comp_op *op = NULL;
	int retval;

	retval = rte_comp_op_raw_bulk_alloc(mempool, &op, 1);
	if (unlikely(retval != 1))
		return NULL;

	rte_comp_op_reset(op);

	return op;
}

int __rte_experimental
rte_comp_op_bulk_alloc(struct rte_mempool *mempool,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	int retval;
	uint16_t i;

	retval = rte_comp_op_raw_bulk_alloc(mempool, ops, nb_ops);
	if (unlikely(retval != nb_ops))
		return 0;

	for (i = 0; i < nb_ops; i++)
		rte_comp_op_reset(ops[i]);

	return nb_ops;
}

/**
 * free operation structure
 * If operation has been allocate from a rte_mempool, then the operation will
 * be returned to the mempool.
 *
 * @param op
 *   Compress operation
 */
void __rte_experimental
rte_comp_op_free(struct rte_comp_op *op)
{
	if (op != NULL && op->mempool != NULL)
		rte_mempool_put(op->mempool, op);
}

void __rte_experimental
rte_comp_op_bulk_free(struct rte_comp_op **ops, uint16_t nb_ops)
{
	uint16_t i;

	for (i = 0; i < nb_ops; i++) {
		if (ops[i] != NULL && ops[i]->mempool != NULL)
			rte_mempool_put(ops[i]->mempool, ops[i]);
		ops[i] = NULL;
	}
}
