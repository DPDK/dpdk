/* SPDX-License-Identifier: (BSD-3-Clause OR LGPL-2.1)
 * Copyright(c) 2010-2013 Intel Corporation.
 * Copyright(c) 2013-2017 Wind River Systems, Inc.
 */

#ifndef _RTE_AVP_FIFO_H_
#define _RTE_AVP_FIFO_H_

#include "rte_avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL__
/* Write memory barrier for kernel compiles */
#define AVP_WMB() smp_wmb()
/* Read memory barrier for kernel compiles */
#define AVP_RMB() smp_rmb()
#else
/* Write memory barrier for userspace compiles */
#define AVP_WMB() rte_wmb()
/* Read memory barrier for userspace compiles */
#define AVP_RMB() rte_rmb()
#endif

#ifndef __KERNEL__
#include <rte_debug.h>

/**
 * Initializes the avp fifo structure
 */
static inline void
avp_fifo_init(struct rte_avp_fifo *fifo, unsigned int size)
{
	/* Ensure size is power of 2 */
	if (size & (size - 1))
		rte_panic("AVP fifo size must be power of 2\n");

	fifo->write = 0;
	fifo->read = 0;
	fifo->len = size;
	fifo->elem_size = sizeof(void *);
}
#endif

/**
 * Adds num elements into the fifo. Return the number actually written
 */
static inline unsigned
avp_fifo_put(struct rte_avp_fifo *fifo, void **data, unsigned int num)
{
	unsigned int i = 0;
	unsigned int fifo_write = fifo->write;
	unsigned int fifo_read = fifo->read;
	unsigned int new_write = fifo_write;

	for (i = 0; i < num; i++) {
		new_write = (new_write + 1) & (fifo->len - 1);

		if (new_write == fifo_read)
			break;
		fifo->buffer[fifo_write] = data[i];
		fifo_write = new_write;
	}
	AVP_WMB();
	fifo->write = fifo_write;
	return i;
}

/**
 * Get up to num elements from the fifo. Return the number actually read
 */
static inline unsigned int
avp_fifo_get(struct rte_avp_fifo *fifo, void **data, unsigned int num)
{
	unsigned int i = 0;
	unsigned int new_read = fifo->read;
	unsigned int fifo_write = fifo->write;

	if (new_read == fifo_write)
		return 0; /* empty */

	for (i = 0; i < num; i++) {
		if (new_read == fifo_write)
			break;

		data[i] = fifo->buffer[new_read];
		new_read = (new_read + 1) & (fifo->len - 1);
	}
	AVP_RMB();
	fifo->read = new_read;
	return i;
}

/**
 * Get the num of elements in the fifo
 */
static inline unsigned int
avp_fifo_count(struct rte_avp_fifo *fifo)
{
	return (fifo->len + fifo->write - fifo->read) & (fifo->len - 1);
}

/**
 * Get the num of available elements in the fifo
 */
static inline unsigned int
avp_fifo_free_count(struct rte_avp_fifo *fifo)
{
	return (fifo->read - fifo->write - 1) & (fifo->len - 1);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_AVP_FIFO_H_ */
