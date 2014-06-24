/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#include <stdio.h>
#include <sys/queue.h>
#include <string.h>
#include <rte_mbuf.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_tailq.h>
#include <rte_eal_memconfig.h>
#include "rte_distributor.h"

#define NO_FLAGS 0
#define RTE_DISTRIB_PREFIX "DT_"

/* we will use the bottom four bits of pointer for flags, shifting out
 * the top four bits to make room (since a 64-bit pointer actually only uses
 * 48 bits). An arithmetic-right-shift will then appropriately restore the
 * original pointer value with proper sign extension into the top bits. */
#define RTE_DISTRIB_FLAG_BITS 4
#define RTE_DISTRIB_FLAGS_MASK (0x0F)
#define RTE_DISTRIB_NO_BUF 0       /**< empty flags: no buffer requested */
#define RTE_DISTRIB_GET_BUF (1)    /**< worker requests a buffer, returns old */
#define RTE_DISTRIB_RETURN_BUF (2) /**< worker returns a buffer, no request */

#define RTE_DISTRIB_BACKLOG_SIZE 8
#define RTE_DISTRIB_BACKLOG_MASK (RTE_DISTRIB_BACKLOG_SIZE - 1)

#define RTE_DISTRIB_MAX_RETURNS 128
#define RTE_DISTRIB_RETURNS_MASK (RTE_DISTRIB_MAX_RETURNS - 1)

/**
 * Buffer structure used to pass the pointer data between cores. This is cache
 * line aligned, but to improve performance and prevent adjacent cache-line
 * prefetches of buffers for other workers, e.g. when worker 1's buffer is on
 * the next cache line to worker 0, we pad this out to three cache lines.
 * Only 64-bits of the memory is actually used though.
 */
union rte_distributor_buffer {
	volatile int64_t bufptr64;
	char pad[CACHE_LINE_SIZE*3];
} __rte_cache_aligned;

struct rte_distributor_backlog {
	unsigned start;
	unsigned count;
	int64_t pkts[RTE_DISTRIB_BACKLOG_SIZE];
};

struct rte_distributor_returned_pkts {
	unsigned start;
	unsigned count;
	struct rte_mbuf *mbufs[RTE_DISTRIB_MAX_RETURNS];
};

struct rte_distributor {
	TAILQ_ENTRY(rte_distributor) next;    /**< Next in list. */

	char name[RTE_DISTRIBUTOR_NAMESIZE];  /**< Name of the ring. */
	unsigned num_workers;                 /**< Number of workers polling */

	uint32_t in_flight_tags[RTE_MAX_LCORE];
	struct rte_distributor_backlog backlog[RTE_MAX_LCORE];

	union rte_distributor_buffer bufs[RTE_MAX_LCORE];

	struct rte_distributor_returned_pkts returns;
};

TAILQ_HEAD(rte_distributor_list, rte_distributor);

/**** APIs called by workers ****/

void
rte_distributor_request_pkt(struct rte_distributor *d,
		unsigned worker_id, struct rte_mbuf *oldpkt)
{
	union rte_distributor_buffer *buf = &d->bufs[worker_id];
	int64_t req = (((int64_t)(uintptr_t)oldpkt) << RTE_DISTRIB_FLAG_BITS)
			| RTE_DISTRIB_GET_BUF;
	while (unlikely(buf->bufptr64 & RTE_DISTRIB_FLAGS_MASK))
		rte_pause();
	buf->bufptr64 = req;
}

struct rte_mbuf *
rte_distributor_poll_pkt(struct rte_distributor *d,
		unsigned worker_id)
{
	union rte_distributor_buffer *buf = &d->bufs[worker_id];
	if (buf->bufptr64 & RTE_DISTRIB_GET_BUF)
		return NULL;

	/* since bufptr64 is signed, this should be an arithmetic shift */
	int64_t ret = buf->bufptr64 >> RTE_DISTRIB_FLAG_BITS;
	return (struct rte_mbuf *)((uintptr_t)ret);
}

struct rte_mbuf *
rte_distributor_get_pkt(struct rte_distributor *d,
		unsigned worker_id, struct rte_mbuf *oldpkt)
{
	struct rte_mbuf *ret;
	rte_distributor_request_pkt(d, worker_id, oldpkt);
	while ((ret = rte_distributor_poll_pkt(d, worker_id)) == NULL)
		rte_pause();
	return ret;
}

int
rte_distributor_return_pkt(struct rte_distributor *d,
		unsigned worker_id, struct rte_mbuf *oldpkt)
{
	union rte_distributor_buffer *buf = &d->bufs[worker_id];
	uint64_t req = (((int64_t)(uintptr_t)oldpkt) << RTE_DISTRIB_FLAG_BITS)
			| RTE_DISTRIB_RETURN_BUF;
	buf->bufptr64 = req;
	return 0;
}

/**** APIs called on distributor core ***/

/* as name suggests, adds a packet to the backlog for a particular worker */
static int
add_to_backlog(struct rte_distributor_backlog *bl, int64_t item)
{
	if (bl->count == RTE_DISTRIB_BACKLOG_SIZE)
		return -1;

	bl->pkts[(bl->start + bl->count++) & (RTE_DISTRIB_BACKLOG_MASK)]
			= item;
	return 0;
}

/* takes the next packet for a worker off the backlog */
static int64_t
backlog_pop(struct rte_distributor_backlog *bl)
{
	bl->count--;
	return bl->pkts[bl->start++ & RTE_DISTRIB_BACKLOG_MASK];
}

/* stores a packet returned from a worker inside the returns array */
static inline void
store_return(uintptr_t oldbuf, struct rte_distributor *d,
		unsigned *ret_start, unsigned *ret_count)
{
	/* store returns in a circular buffer - code is branch-free */
	d->returns.mbufs[(*ret_start + *ret_count) & RTE_DISTRIB_RETURNS_MASK]
			= (void *)oldbuf;
	*ret_start += (*ret_count == RTE_DISTRIB_RETURNS_MASK) & !!(oldbuf);
	*ret_count += (*ret_count != RTE_DISTRIB_RETURNS_MASK) & !!(oldbuf);
}

static inline void
handle_worker_shutdown(struct rte_distributor *d, unsigned wkr)
{
	d->in_flight_tags[wkr] = 0;
	d->bufs[wkr].bufptr64 = 0;
	if (unlikely(d->backlog[wkr].count != 0)) {
		/* On return of a packet, we need to move the
		 * queued packets for this core elsewhere.
		 * Easiest solution is to set things up for
		 * a recursive call. That will cause those
		 * packets to be queued up for the next free
		 * core, i.e. it will return as soon as a
		 * core becomes free to accept the first
		 * packet, as subsequent ones will be added to
		 * the backlog for that core.
		 */
		struct rte_mbuf *pkts[RTE_DISTRIB_BACKLOG_SIZE];
		unsigned i;
		struct rte_distributor_backlog *bl = &d->backlog[wkr];

		for (i = 0; i < bl->count; i++) {
			unsigned idx = (bl->start + i) &
					RTE_DISTRIB_BACKLOG_MASK;
			pkts[i] = (void *)((uintptr_t)(bl->pkts[idx] >>
					RTE_DISTRIB_FLAG_BITS));
		}
		/* recursive call */
		rte_distributor_process(d, pkts, i);
		bl->count = bl->start = 0;
	}
}

/* this function is called when process() fn is called without any new
 * packets. It goes through all the workers and clears any returned packets
 * to do a partial flush.
 */
static int
process_returns(struct rte_distributor *d)
{
	unsigned wkr;
	unsigned flushed = 0;
	unsigned ret_start = d->returns.start,
			ret_count = d->returns.count;

	for (wkr = 0; wkr < d->num_workers; wkr++) {

		const int64_t data = d->bufs[wkr].bufptr64;
		uintptr_t oldbuf = 0;

		if (data & RTE_DISTRIB_GET_BUF) {
			flushed++;
			if (d->backlog[wkr].count)
				d->bufs[wkr].bufptr64 =
						backlog_pop(&d->backlog[wkr]);
			else {
				d->bufs[wkr].bufptr64 = RTE_DISTRIB_GET_BUF;
				d->in_flight_tags[wkr] = 0;
			}
			oldbuf = data >> RTE_DISTRIB_FLAG_BITS;
		} else if (data & RTE_DISTRIB_RETURN_BUF) {
			handle_worker_shutdown(d, wkr);
			oldbuf = data >> RTE_DISTRIB_FLAG_BITS;
		}

		store_return(oldbuf, d, &ret_start, &ret_count);
	}

	d->returns.start = ret_start;
	d->returns.count = ret_count;

	return flushed;
}

/* process a set of packets to distribute them to workers */
int
rte_distributor_process(struct rte_distributor *d,
		struct rte_mbuf **mbufs, unsigned num_mbufs)
{
	unsigned next_idx = 0;
	unsigned wkr = 0;
	struct rte_mbuf *next_mb = NULL;
	int64_t next_value = 0;
	uint32_t new_tag = 0;
	unsigned ret_start = d->returns.start,
			ret_count = d->returns.count;

	if (unlikely(num_mbufs == 0))
		return process_returns(d);

	while (next_idx < num_mbufs || next_mb != NULL) {

		int64_t data = d->bufs[wkr].bufptr64;
		uintptr_t oldbuf = 0;

		if (!next_mb) {
			next_mb = mbufs[next_idx++];
			next_value = (((int64_t)(uintptr_t)next_mb)
					<< RTE_DISTRIB_FLAG_BITS);
			new_tag = (next_mb->pkt.hash.rss | 1);

			uint32_t match = 0;
			unsigned i;
			for (i = 0; i < d->num_workers; i++)
				match |= (!(d->in_flight_tags[i] ^ new_tag)
					<< i);

			if (match) {
				next_mb = NULL;
				unsigned worker = __builtin_ctz(match);
				if (add_to_backlog(&d->backlog[worker],
						next_value) < 0)
					next_idx--;
			}
		}

		if ((data & RTE_DISTRIB_GET_BUF) &&
				(d->backlog[wkr].count || next_mb)) {

			if (d->backlog[wkr].count)
				d->bufs[wkr].bufptr64 =
						backlog_pop(&d->backlog[wkr]);

			else {
				d->bufs[wkr].bufptr64 = next_value;
				d->in_flight_tags[wkr] = new_tag;
				next_mb = NULL;
			}
			oldbuf = data >> RTE_DISTRIB_FLAG_BITS;
		} else if (data & RTE_DISTRIB_RETURN_BUF) {
			handle_worker_shutdown(d, wkr);
			oldbuf = data >> RTE_DISTRIB_FLAG_BITS;
		}

		/* store returns in a circular buffer */
		store_return(oldbuf, d, &ret_start, &ret_count);

		if (++wkr == d->num_workers)
			wkr = 0;
	}
	/* to finish, check all workers for backlog and schedule work for them
	 * if they are ready */
	for (wkr = 0; wkr < d->num_workers; wkr++)
		if (d->backlog[wkr].count &&
				(d->bufs[wkr].bufptr64 & RTE_DISTRIB_GET_BUF)) {

			int64_t oldbuf = d->bufs[wkr].bufptr64 >>
					RTE_DISTRIB_FLAG_BITS;
			store_return(oldbuf, d, &ret_start, &ret_count);

			d->bufs[wkr].bufptr64 = backlog_pop(&d->backlog[wkr]);
		}

	d->returns.start = ret_start;
	d->returns.count = ret_count;
	return num_mbufs;
}

/* return to the caller, packets returned from workers */
int
rte_distributor_returned_pkts(struct rte_distributor *d,
		struct rte_mbuf **mbufs, unsigned max_mbufs)
{
	struct rte_distributor_returned_pkts *returns = &d->returns;
	unsigned retval = (max_mbufs < returns->count) ?
			max_mbufs : returns->count;
	unsigned i;

	for (i = 0; i < retval; i++) {
		unsigned idx = (returns->start + i) & RTE_DISTRIB_RETURNS_MASK;
		mbufs[i] = returns->mbufs[idx];
	}
	returns->start += i;
	returns->count -= i;

	return retval;
}

/* return the number of packets in-flight in a distributor, i.e. packets
 * being workered on or queued up in a backlog. */
static inline unsigned
total_outstanding(const struct rte_distributor *d)
{
	unsigned wkr, total_outstanding = 0;

	for (wkr = 0; wkr < d->num_workers; wkr++)
		total_outstanding += d->backlog[wkr].count +
				!!(d->in_flight_tags[wkr]);
	return total_outstanding;
}

/* flush the distributor, so that there are no outstanding packets in flight or
 * queued up. */
int
rte_distributor_flush(struct rte_distributor *d)
{
	const unsigned flushed = total_outstanding(d);

	while (total_outstanding(d) > 0)
		rte_distributor_process(d, NULL, 0);

	return flushed;
}

/* clears the internal returns array in the distributor */
void
rte_distributor_clear_returns(struct rte_distributor *d)
{
	d->returns.start = d->returns.count = 0;
#ifndef __OPTIMIZE__
	memset(d->returns.mbufs, 0, sizeof(d->returns.mbufs));
#endif
}

/* creates a distributor instance */
struct rte_distributor *
rte_distributor_create(const char *name,
		unsigned socket_id,
		unsigned num_workers)
{
	struct rte_distributor *d;
	struct rte_distributor_list *distributor_list;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(*d) & CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((RTE_MAX_LCORE & 7) != 0);

	if (name == NULL || num_workers >= RTE_MAX_LCORE) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* check that we have an initialised tail queue */
	distributor_list = RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_DISTRIBUTOR,
				rte_distributor_list);
	if (distributor_list == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

	snprintf(mz_name, sizeof(mz_name), RTE_DISTRIB_PREFIX"%s", name);
	mz = rte_memzone_reserve(mz_name, sizeof(*d), socket_id, NO_FLAGS);
	if (mz == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	d = mz->addr;
	snprintf(d->name, sizeof(d->name), "%s", name);
	d->num_workers = num_workers;

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_INSERT_TAIL(distributor_list, d, next);
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	return d;
}
