/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright(c) 2010-2014 Intel Corporation.
 */

#ifndef _KNI_FIFO_H_
#define _KNI_FIFO_H_

#include <rte_kni_common.h>

/* Skip some memory barriers on Linux < 3.14 */
#ifndef smp_load_acquire
#define smp_load_acquire(a) (*(a))
#endif
#ifndef smp_store_release
#define smp_store_release(a, b) *(a) = (b)
#endif

/**
 * Adds num elements into the fifo. Return the number actually written
 */
static inline uint32_t
kni_fifo_put(struct rte_kni_fifo *fifo, void **data, uint32_t num)
{
	uint32_t i = 0;
	uint32_t fifo_write = fifo->write;
	uint32_t fifo_read = smp_load_acquire(&fifo->read);
	uint32_t new_write = fifo_write;

	for (i = 0; i < num; i++) {
		new_write = (new_write + 1) & (fifo->len - 1);

		if (new_write == fifo_read)
			break;
		fifo->buffer[fifo_write] = data[i];
		fifo_write = new_write;
	}
	smp_store_release(&fifo->write, fifo_write);

	return i;
}

/**
 * Get up to num elements from the FIFO. Return the number actually read
 */
static inline uint32_t
kni_fifo_get(struct rte_kni_fifo *fifo, void **data, uint32_t num)
{
	uint32_t i = 0;
	uint32_t new_read = fifo->read;
	uint32_t fifo_write = smp_load_acquire(&fifo->write);

	for (i = 0; i < num; i++) {
		if (new_read == fifo_write)
			break;

		data[i] = fifo->buffer[new_read];
		new_read = (new_read + 1) & (fifo->len - 1);
	}
	smp_store_release(&fifo->read, new_read);

	return i;
}

/**
 * Get the num of elements in the fifo
 */
static inline uint32_t
kni_fifo_count(struct rte_kni_fifo *fifo)
{
	uint32_t fifo_write = smp_load_acquire(&fifo->write);
	uint32_t fifo_read = smp_load_acquire(&fifo->read);
	return (fifo->len + fifo_write - fifo_read) & (fifo->len - 1);
}

/**
 * Get the num of available elements in the fifo
 */
static inline uint32_t
kni_fifo_free_count(struct rte_kni_fifo *fifo)
{
	uint32_t fifo_write = smp_load_acquire(&fifo->write);
	uint32_t fifo_read = smp_load_acquire(&fifo->read);
	return (fifo_read - fifo_write - 1) & (fifo->len - 1);
}

#endif /* _KNI_FIFO_H_ */
