/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <inttypes.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "rte_reorder.h"

TAILQ_HEAD(rte_reorder_list, rte_tailq_entry);

static struct rte_tailq_elem rte_reorder_tailq = {
	.name = "RTE_REORDER",
};
EAL_REGISTER_TAILQ(rte_reorder_tailq)

#define NO_FLAGS 0
#define RTE_REORDER_PREFIX "RO_"
#define RTE_REORDER_NAMESIZE 32

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_REORDER	RTE_LOGTYPE_USER1

/* A generic circular buffer */
struct cir_buffer {
	unsigned int size;   /**< Number of entries that can be stored */
	unsigned int mask;   /**< [buffer_size - 1]: used for wrap-around */
	unsigned int head;   /**< insertion point in buffer */
	unsigned int tail;   /**< extraction point in buffer */
	struct rte_mbuf **entries;
} __rte_cache_aligned;

/* The reorder buffer data structure itself */
struct rte_reorder_buffer {
	char name[RTE_REORDER_NAMESIZE];
	uint32_t min_seqn;  /**< Lowest seq. number that can be in the buffer */
	unsigned int memsize; /**< memory area size of reorder buffer */
	struct cir_buffer ready_buf; /**< temp buffer for dequeued entries */
	struct cir_buffer order_buf; /**< buffer used to reorder entries */
	int is_initialized;
} __rte_cache_aligned;

static void
rte_reorder_free_mbufs(struct rte_reorder_buffer *b);

struct rte_reorder_buffer *
rte_reorder_init(struct rte_reorder_buffer *b, unsigned int bufsize,
		const char *name, unsigned int size)
{
	const unsigned int min_bufsize = sizeof(*b) +
					(2 * size * sizeof(struct rte_mbuf *));

	if (b == NULL) {
		RTE_LOG(ERR, REORDER, "Invalid reorder buffer parameter:"
					" NULL\n");
		rte_errno = EINVAL;
		return NULL;
	}
	if (!rte_is_power_of_2(size)) {
		RTE_LOG(ERR, REORDER, "Invalid reorder buffer size"
				" - Not a power of 2\n");
		rte_errno = EINVAL;
		return NULL;
	}
	if (name == NULL) {
		RTE_LOG(ERR, REORDER, "Invalid reorder buffer name ptr:"
					" NULL\n");
		rte_errno = EINVAL;
		return NULL;
	}
	if (bufsize < min_bufsize) {
		RTE_LOG(ERR, REORDER, "Invalid reorder buffer memory size: %u, "
			"minimum required: %u\n", bufsize, min_bufsize);
		rte_errno = EINVAL;
		return NULL;
	}

	memset(b, 0, bufsize);
	strlcpy(b->name, name, sizeof(b->name));
	b->memsize = bufsize;
	b->order_buf.size = b->ready_buf.size = size;
	b->order_buf.mask = b->ready_buf.mask = size - 1;
	b->ready_buf.entries = (void *)&b[1];
	b->order_buf.entries = RTE_PTR_ADD(&b[1],
			size * sizeof(b->ready_buf.entries[0]));

	return b;
}

struct rte_reorder_buffer*
rte_reorder_create(const char *name, unsigned socket_id, unsigned int size)
{
	struct rte_reorder_buffer *b = NULL;
	struct rte_tailq_entry *te;
	struct rte_reorder_list *reorder_list;
	const unsigned int bufsize = sizeof(struct rte_reorder_buffer) +
					(2 * size * sizeof(struct rte_mbuf *));

	reorder_list = RTE_TAILQ_CAST(rte_reorder_tailq.head, rte_reorder_list);

	/* Check user arguments. */
	if (!rte_is_power_of_2(size)) {
		RTE_LOG(ERR, REORDER, "Invalid reorder buffer size"
				" - Not a power of 2\n");
		rte_errno = EINVAL;
		return NULL;
	}
	if (name == NULL) {
		RTE_LOG(ERR, REORDER, "Invalid reorder buffer name ptr:"
					" NULL\n");
		rte_errno = EINVAL;
		return NULL;
	}

	rte_mcfg_tailq_write_lock();

	/* guarantee there's no existing */
	TAILQ_FOREACH(te, reorder_list, next) {
		b = (struct rte_reorder_buffer *) te->data;
		if (strncmp(name, b->name, RTE_REORDER_NAMESIZE) == 0)
			break;
	}
	if (te != NULL)
		goto exit;

	/* allocate tailq entry */
	te = rte_zmalloc("REORDER_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, REORDER, "Failed to allocate tailq entry\n");
		rte_errno = ENOMEM;
		b = NULL;
		goto exit;
	}

	/* Allocate memory to store the reorder buffer structure. */
	b = rte_zmalloc_socket("REORDER_BUFFER", bufsize, 0, socket_id);
	if (b == NULL) {
		RTE_LOG(ERR, REORDER, "Memzone allocation failed\n");
		rte_errno = ENOMEM;
		rte_free(te);
	} else {
		rte_reorder_init(b, bufsize, name, size);
		te->data = (void *)b;
		TAILQ_INSERT_TAIL(reorder_list, te, next);
	}

exit:
	rte_mcfg_tailq_write_unlock();
	return b;
}

void
rte_reorder_reset(struct rte_reorder_buffer *b)
{
	char name[RTE_REORDER_NAMESIZE];

	rte_reorder_free_mbufs(b);
	strlcpy(name, b->name, sizeof(name));
	/* No error checking as current values should be valid */
	rte_reorder_init(b, b->memsize, name, b->order_buf.size);
}

static void
rte_reorder_free_mbufs(struct rte_reorder_buffer *b)
{
	unsigned i;

	/* Free up the mbufs of order buffer & ready buffer */
	for (i = 0; i < b->order_buf.size; i++) {
		if (b->order_buf.entries[i])
			rte_pktmbuf_free(b->order_buf.entries[i]);
		if (b->ready_buf.entries[i])
			rte_pktmbuf_free(b->ready_buf.entries[i]);
	}
}

void
rte_reorder_free(struct rte_reorder_buffer *b)
{
	struct rte_reorder_list *reorder_list;
	struct rte_tailq_entry *te;

	/* Check user arguments. */
	if (b == NULL)
		return;

	reorder_list = RTE_TAILQ_CAST(rte_reorder_tailq.head, rte_reorder_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, reorder_list, next) {
		if (te->data == (void *) b)
			break;
	}
	if (te == NULL) {
		rte_mcfg_tailq_write_unlock();
		return;
	}

	TAILQ_REMOVE(reorder_list, te, next);

	rte_mcfg_tailq_write_unlock();

	rte_reorder_free_mbufs(b);

	rte_free(b);
	rte_free(te);
}

struct rte_reorder_buffer *
rte_reorder_find_existing(const char *name)
{
	struct rte_reorder_buffer *b = NULL;
	struct rte_tailq_entry *te;
	struct rte_reorder_list *reorder_list;

	if (name == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	reorder_list = RTE_TAILQ_CAST(rte_reorder_tailq.head, rte_reorder_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, reorder_list, next) {
		b = (struct rte_reorder_buffer *) te->data;
		if (strncmp(name, b->name, RTE_REORDER_NAMESIZE) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return b;
}

static unsigned
rte_reorder_fill_overflow(struct rte_reorder_buffer *b, unsigned n)
{
	/*
	 * 1. Move all ready entries that fit to the ready_buf
	 * 2. check if we meet the minimum needed (n).
	 * 3. If not, then skip any gaps and keep moving.
	 * 4. If at any point the ready buffer is full, stop
	 * 5. Return the number of positions the order_buf head has moved
	 */

	struct cir_buffer *order_buf = &b->order_buf,
			*ready_buf = &b->ready_buf;

	unsigned int order_head_adv = 0;

	/*
	 * move at least n packets to ready buffer, assuming ready buffer
	 * has room for those packets.
	 */
	while (order_head_adv < n &&
			((ready_buf->head + 1) & ready_buf->mask) != ready_buf->tail) {

		/* if we are blocked waiting on a packet, skip it */
		if (order_buf->entries[order_buf->head] == NULL) {
			order_buf->head = (order_buf->head + 1) & order_buf->mask;
			order_head_adv++;
		}

		/* Move all ready entries that fit to the ready_buf */
		while (order_buf->entries[order_buf->head] != NULL) {
			ready_buf->entries[ready_buf->head] =
					order_buf->entries[order_buf->head];

			order_buf->entries[order_buf->head] = NULL;
			order_head_adv++;

			order_buf->head = (order_buf->head + 1) & order_buf->mask;

			if (((ready_buf->head + 1) & ready_buf->mask) == ready_buf->tail)
				break;

			ready_buf->head = (ready_buf->head + 1) & ready_buf->mask;
		}
	}

	b->min_seqn += order_head_adv;
	/* Return the number of positions the order_buf head has moved */
	return order_head_adv;
}

int
rte_reorder_insert(struct rte_reorder_buffer *b, struct rte_mbuf *mbuf)
{
	uint32_t offset, position;
	struct cir_buffer *order_buf;

	if (b == NULL || mbuf == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	order_buf = &b->order_buf;
	if (!b->is_initialized) {
		b->min_seqn = mbuf->seqn;
		b->is_initialized = 1;
	}

	/*
	 * calculate the offset from the head pointer we need to go.
	 * The subtraction takes care of the sequence number wrapping.
	 * For example (using 16-bit for brevity):
	 *	min_seqn  = 0xFFFD
	 *	mbuf_seqn = 0x0010
	 *	offset    = 0x0010 - 0xFFFD = 0x13
	 */
	offset = mbuf->seqn - b->min_seqn;

	/*
	 * action to take depends on offset.
	 * offset < buffer->size: the mbuf fits within the current window of
	 *    sequence numbers we can reorder. EXPECTED CASE.
	 * offset > buffer->size: the mbuf is outside the current window. There
	 *    are a number of cases to consider:
	 *    1. The packet sequence is just outside the window, then we need
	 *       to see about shifting the head pointer and taking any ready
	 *       to return packets out of the ring. If there was a delayed
	 *       or dropped packet preventing drains from shifting the window
	 *       this case will skip over the dropped packet instead, and any
	 *       packets dequeued here will be returned on the next drain call.
	 *    2. The packet sequence number is vastly outside our window, taken
	 *       here as having offset greater than twice the buffer size. In
	 *       this case, the packet is probably an old or late packet that
	 *       was previously skipped, so just enqueue the packet for
	 *       immediate return on the next drain call, or else return error.
	 */
	if (offset < b->order_buf.size) {
		position = (order_buf->head + offset) & order_buf->mask;
		order_buf->entries[position] = mbuf;
	} else if (offset < 2 * b->order_buf.size) {
		if (rte_reorder_fill_overflow(b, offset + 1 - order_buf->size)
				< (offset + 1 - order_buf->size)) {
			/* Put in handling for enqueue straight to output */
			rte_errno = ENOSPC;
			return -1;
		}
		offset = mbuf->seqn - b->min_seqn;
		position = (order_buf->head + offset) & order_buf->mask;
		order_buf->entries[position] = mbuf;
	} else {
		/* Put in handling for enqueue straight to output */
		rte_errno = ERANGE;
		return -1;
	}
	return 0;
}

unsigned int
rte_reorder_drain(struct rte_reorder_buffer *b, struct rte_mbuf **mbufs,
		unsigned max_mbufs)
{
	unsigned int drain_cnt = 0;

	struct cir_buffer *order_buf = &b->order_buf,
			*ready_buf = &b->ready_buf;

	/* Try to fetch requested number of mbufs from ready buffer */
	while ((drain_cnt < max_mbufs) && (ready_buf->tail != ready_buf->head)) {
		mbufs[drain_cnt++] = ready_buf->entries[ready_buf->tail];
		ready_buf->tail = (ready_buf->tail + 1) & ready_buf->mask;
	}

	/*
	 * If requested number of buffers not fetched from ready buffer, fetch
	 * remaining buffers from order buffer
	 */
	while ((drain_cnt < max_mbufs) &&
			(order_buf->entries[order_buf->head] != NULL)) {
		mbufs[drain_cnt++] = order_buf->entries[order_buf->head];
		order_buf->entries[order_buf->head] = NULL;
		b->min_seqn++;
		order_buf->head = (order_buf->head + 1) & order_buf->mask;
	}

	return drain_cnt;
}
