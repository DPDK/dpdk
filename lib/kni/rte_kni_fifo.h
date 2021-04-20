/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */



/**
 * @internal when c11 memory model enabled use c11 atomic memory barrier.
 * when under non c11 memory model use rte_smp_* memory barrier.
 *
 * @param src
 *   Pointer to the source data.
 * @param dst
 *   Pointer to the destination data.
 * @param value
 *   Data value.
 */
#ifdef RTE_USE_C11_MEM_MODEL
#define __KNI_LOAD_ACQUIRE(src) ({                         \
		__atomic_load_n((src), __ATOMIC_ACQUIRE);           \
	})
#define __KNI_STORE_RELEASE(dst, value) do {               \
		__atomic_store_n((dst), value, __ATOMIC_RELEASE);   \
	} while(0)
#else
#define __KNI_LOAD_ACQUIRE(src) ({                         \
		typeof (*(src)) val = *(src);                       \
		rte_smp_rmb();                                      \
		val;                                                \
	})
#define __KNI_STORE_RELEASE(dst, value) do {               \
		*(dst) = value;                                     \
		rte_smp_wmb();                                      \
	} while(0)
#endif

/**
 * Initializes the kni fifo structure
 */
static void
kni_fifo_init(struct rte_kni_fifo *fifo, unsigned size)
{
	/* Ensure size is power of 2 */
	if (size & (size - 1))
		rte_panic("KNI fifo size must be power of 2\n");

	fifo->write = 0;
	fifo->read = 0;
	fifo->len = size;
	fifo->elem_size = sizeof(void *);
}

/**
 * Adds num elements into the fifo. Return the number actually written
 */
static inline unsigned
kni_fifo_put(struct rte_kni_fifo *fifo, void **data, unsigned num)
{
	unsigned i = 0;
	unsigned fifo_write = fifo->write;
	unsigned new_write = fifo_write;
	unsigned fifo_read = __KNI_LOAD_ACQUIRE(&fifo->read);

	for (i = 0; i < num; i++) {
		new_write = (new_write + 1) & (fifo->len - 1);

		if (new_write == fifo_read)
			break;
		fifo->buffer[fifo_write] = data[i];
		fifo_write = new_write;
	}
	__KNI_STORE_RELEASE(&fifo->write, fifo_write);
	return i;
}

/**
 * Get up to num elements from the fifo. Return the number actually read
 */
static inline unsigned
kni_fifo_get(struct rte_kni_fifo *fifo, void **data, unsigned num)
{
	unsigned i = 0;
	unsigned new_read = fifo->read;
	unsigned fifo_write = __KNI_LOAD_ACQUIRE(&fifo->write);

	for (i = 0; i < num; i++) {
		if (new_read == fifo_write)
			break;

		data[i] = fifo->buffer[new_read];
		new_read = (new_read + 1) & (fifo->len - 1);
	}
	__KNI_STORE_RELEASE(&fifo->read, new_read);
	return i;
}

/**
 * Get the num of elements in the fifo
 */
static inline uint32_t
kni_fifo_count(struct rte_kni_fifo *fifo)
{
	unsigned fifo_write = __KNI_LOAD_ACQUIRE(&fifo->write);
	unsigned fifo_read = __KNI_LOAD_ACQUIRE(&fifo->read);
	return (fifo->len + fifo_write - fifo_read) & (fifo->len - 1);
}

/**
 * Get the num of available elements in the fifo
 */
static inline uint32_t
kni_fifo_free_count(struct rte_kni_fifo *fifo)
{
	uint32_t fifo_write = __KNI_LOAD_ACQUIRE(&fifo->write);
	uint32_t fifo_read = __KNI_LOAD_ACQUIRE(&fifo->read);
	return (fifo_read - fifo_write - 1) & (fifo->len - 1);
}
