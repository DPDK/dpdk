/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_REORDER_H_
#define _RTE_REORDER_H_

/**
 * @file
 * RTE reorder
 *
 * Reorder library is a component which is designed to
 * provide ordering of out of ordered packets based on
 * sequence number present in mbuf.
 *
 */

#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_reorder_buffer;

/**
 * Create a new reorder buffer instance
 *
 * Allocate memory and initialize a new reorder buffer in that
 * memory, returning the reorder buffer pointer to the user
 *
 * @param name
 *   The name to be given to the reorder buffer instance.
 * @param socket_id
 *   The NUMA node on which the memory for the reorder buffer
 *   instance is to be reserved.
 * @param size
 *   Max number of elements that can be stored in the reorder buffer
 * @return
 *   The initialized reorder buffer instance, or NULL on error
 *   On error case, rte_errno will be set appropriately:
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 *    - EINVAL - invalid parameters
 */
struct rte_reorder_buffer *
rte_reorder_create(const char *name, unsigned socket_id, unsigned int size);

/**
 * Initializes given reorder buffer instance
 *
 * @param b
 *   Reorder buffer instance to initialize
 * @param bufsize
 *   Size of the reorder buffer
 * @param name
 *   The name to be given to the reorder buffer
 * @param size
 *   Number of elements that can be stored in reorder buffer
 * @return
 *   The initialized reorder buffer instance, or NULL on error
 *   On error case, rte_errno will be set appropriately:
 *    - EINVAL - invalid parameters
 */
struct rte_reorder_buffer *
rte_reorder_init(struct rte_reorder_buffer *b, unsigned int bufsize,
		const char *name, unsigned int size);

/**
 * Find an existing reorder buffer instance
 * and return a pointer to it.
 *
 * @param name
 *   Name of the reorder buffer instance as passed to rte_reorder_create()
 * @return
 *   Pointer to reorder buffer instance or NULL if object not found with rte_errno
 *   set appropriately. Possible rte_errno values include:
 *    - ENOENT - required entry not available to return.
 *    reorder instance list
 */
struct rte_reorder_buffer *
rte_reorder_find_existing(const char *name);

/**
 * Reset the given reorder buffer instance with initial values.
 *
 * @param b
 *   Reorder buffer instance which has to be reset
 */
void
rte_reorder_reset(struct rte_reorder_buffer *b);

/**
 * Free reorder buffer instance.
 *
 * @param b
 *   reorder buffer instance
 * @return
 *   None
 */
void
rte_reorder_free(struct rte_reorder_buffer *b);

/**
 * Insert given mbuf in reorder buffer in its correct position
 *
 * The given mbuf is to be reordered relative to other mbufs in the system.
 * The mbuf must contain a sequence number which is then used to place
 * the buffer in the correct position in the reorder buffer. Reordered
 * packets can later be taken from the buffer using the rte_reorder_drain()
 * API.
 *
 * @param b
 *   Reorder buffer where the mbuf has to be inserted.
 * @param mbuf
 *   mbuf of packet that needs to be inserted in reorder buffer.
 * @return
 *   0 on success
 *   -1 on error
 *   On error case, rte_errno will be set appropriately:
 *    - ENOSPC - Cannot move existing mbufs from reorder buffer to accommodate
 *      early mbuf, but it can be accommodated by performing drain and then insert.
 *    - ERANGE - Too early or late mbuf which is vastly out of range of expected
 *      window should be ignored without any handling.
 */
int
rte_reorder_insert(struct rte_reorder_buffer *b, struct rte_mbuf *mbuf);

/**
 * Fetch reordered buffers
 *
 * Returns a set of in-order buffers from the reorder buffer structure. Gaps
 * may be present in the sequence numbers of the mbuf if packets have been
 * delayed too long before reaching the reorder window, or have been previously
 * dropped by the system.
 *
 * @param b
 *   Reorder buffer instance from which packets are to be drained
 * @param mbufs
 *   array of mbufs where reordered packets will be inserted from reorder buffer
 * @param max_mbufs
 *   the number of elements in the mbufs array.
 * @return
 *   number of mbuf pointers written to mbufs. 0 <= N < max_mbufs.
 */
unsigned int
rte_reorder_drain(struct rte_reorder_buffer *b, struct rte_mbuf **mbufs,
		unsigned max_mbufs);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_REORDER_H_ */
