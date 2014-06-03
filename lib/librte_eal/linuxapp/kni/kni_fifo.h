/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#ifndef _KNI_FIFO_H_
#define _KNI_FIFO_H_

#include <exec-env/rte_kni_common.h>

/**
 * Adds num elements into the fifo. Return the number actually written
 */
static inline unsigned
kni_fifo_put(struct rte_kni_fifo *fifo, void **data, unsigned num)
{
	unsigned i = 0;
	unsigned fifo_write = fifo->write;
	unsigned fifo_read = fifo->read;
	unsigned new_write = fifo_write;

	for (i = 0; i < num; i++) {
		new_write = (new_write + 1) & (fifo->len - 1);

		if (new_write == fifo_read)
			break;
		fifo->buffer[fifo_write] = data[i];
		fifo_write = new_write;
	}
	fifo->write = fifo_write;

	return i;
}

/**
 * Get up to num elements from the fifo. Return the number actully read
 */
static inline unsigned
kni_fifo_get(struct rte_kni_fifo *fifo, void **data, unsigned num)
{
	unsigned i = 0;
	unsigned new_read = fifo->read;
	unsigned fifo_write = fifo->write;

	for (i = 0; i < num; i++) {
		if (new_read == fifo_write)
			break;

		data[i] = fifo->buffer[new_read];
		new_read = (new_read + 1) & (fifo->len - 1);
	}
	fifo->read = new_read;

	return i;
}

/**
 * Get the num of elements in the fifo
 */
static inline unsigned
kni_fifo_count(struct rte_kni_fifo *fifo)
{
	return (fifo->len + fifo->write - fifo->read) & ( fifo->len - 1);
}

/**
 * Get the num of available elements in the fifo
 */
static inline unsigned
kni_fifo_free_count(struct rte_kni_fifo *fifo)
{
	return (fifo->read - fifo->write - 1) & (fifo->len - 1);
}

#ifdef RTE_KNI_VHOST
/**
 * Initializes the kni fifo structure
 */
static inline void
kni_fifo_init(struct rte_kni_fifo *fifo, unsigned size)
{
	fifo->write = 0;
	fifo->read = 0;
	fifo->len = size;
	fifo->elem_size = sizeof(void *);
}
#endif

#endif /* _KNI_FIFO_H_ */
