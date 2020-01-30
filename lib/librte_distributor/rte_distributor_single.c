/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <sys/queue.h>
#include <string.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_eal_memconfig.h>
#include <rte_pause.h>
#include <rte_tailq.h>

#include "rte_distributor_single.h"
#include "distributor_private.h"

TAILQ_HEAD(rte_distributor_list, rte_distributor_single);

static struct rte_tailq_elem rte_distributor_tailq = {
	.name = "RTE_DISTRIBUTOR",
};
EAL_REGISTER_TAILQ(rte_distributor_tailq)

/**** APIs called by workers ****/

void
rte_distributor_request_pkt_single(struct rte_distributor_single *d,
		unsigned worker_id, struct rte_mbuf *oldpkt)
{
	union rte_distributor_buffer_single *buf = &d->bufs[worker_id];
	int64_t req = (((int64_t)(uintptr_t)oldpkt) << RTE_DISTRIB_FLAG_BITS)
			| RTE_DISTRIB_GET_BUF;
	while (unlikely(__atomic_load_n(&buf->bufptr64, __ATOMIC_RELAXED)
			& RTE_DISTRIB_FLAGS_MASK))
		rte_pause();

	/* Sync with distributor on GET_BUF flag. */
	__atomic_store_n(&(buf->bufptr64), req, __ATOMIC_RELEASE);
}

struct rte_mbuf *
rte_distributor_poll_pkt_single(struct rte_distributor_single *d,
		unsigned worker_id)
{
	union rte_distributor_buffer_single *buf = &d->bufs[worker_id];
	/* Sync with distributor. Acquire bufptr64. */
	if (__atomic_load_n(&buf->bufptr64, __ATOMIC_ACQUIRE)
		& RTE_DISTRIB_GET_BUF)
		return NULL;

	/* since bufptr64 is signed, this should be an arithmetic shift */
	int64_t ret = buf->bufptr64 >> RTE_DISTRIB_FLAG_BITS;
	return (struct rte_mbuf *)((uintptr_t)ret);
}

struct rte_mbuf *
rte_distributor_get_pkt_single(struct rte_distributor_single *d,
		unsigned worker_id, struct rte_mbuf *oldpkt)
{
	struct rte_mbuf *ret;
	rte_distributor_request_pkt_single(d, worker_id, oldpkt);
	while ((ret = rte_distributor_poll_pkt_single(d, worker_id)) == NULL)
		rte_pause();
	return ret;
}

int
rte_distributor_return_pkt_single(struct rte_distributor_single *d,
		unsigned worker_id, struct rte_mbuf *oldpkt)
{
	union rte_distributor_buffer_single *buf = &d->bufs[worker_id];
	uint64_t req = (((int64_t)(uintptr_t)oldpkt) << RTE_DISTRIB_FLAG_BITS)
			| RTE_DISTRIB_RETURN_BUF;
	/* Sync with distributor on RETURN_BUF flag. */
	__atomic_store_n(&(buf->bufptr64), req, __ATOMIC_RELEASE);
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
store_return(uintptr_t oldbuf, struct rte_distributor_single *d,
		unsigned *ret_start, unsigned *ret_count)
{
	/* store returns in a circular buffer - code is branch-free */
	d->returns.mbufs[(*ret_start + *ret_count) & RTE_DISTRIB_RETURNS_MASK]
			= (void *)oldbuf;
	*ret_start += (*ret_count == RTE_DISTRIB_RETURNS_MASK) & !!(oldbuf);
	*ret_count += (*ret_count != RTE_DISTRIB_RETURNS_MASK) & !!(oldbuf);
}

static inline void
handle_worker_shutdown(struct rte_distributor_single *d, unsigned int wkr)
{
	d->in_flight_tags[wkr] = 0;
	d->in_flight_bitmask &= ~(1UL << wkr);
	/* Sync with worker. Release bufptr64. */
	__atomic_store_n(&(d->bufs[wkr].bufptr64), 0, __ATOMIC_RELEASE);
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
		/* recursive call.
		 * Note that the tags were set before first level call
		 * to rte_distributor_process.
		 */
		rte_distributor_process_single(d, pkts, i);
		bl->count = bl->start = 0;
	}
}

/* this function is called when process() fn is called without any new
 * packets. It goes through all the workers and clears any returned packets
 * to do a partial flush.
 */
static int
process_returns(struct rte_distributor_single *d)
{
	unsigned wkr;
	unsigned flushed = 0;
	unsigned ret_start = d->returns.start,
			ret_count = d->returns.count;

	for (wkr = 0; wkr < d->num_workers; wkr++) {
		uintptr_t oldbuf = 0;
		/* Sync with worker. Acquire bufptr64. */
		const int64_t data = __atomic_load_n(&(d->bufs[wkr].bufptr64),
							__ATOMIC_ACQUIRE);

		if (data & RTE_DISTRIB_GET_BUF) {
			flushed++;
			if (d->backlog[wkr].count)
				/* Sync with worker. Release bufptr64. */
				__atomic_store_n(&(d->bufs[wkr].bufptr64),
					backlog_pop(&d->backlog[wkr]),
					__ATOMIC_RELEASE);
			else {
				/* Sync with worker on GET_BUF flag. */
				__atomic_store_n(&(d->bufs[wkr].bufptr64),
					RTE_DISTRIB_GET_BUF,
					__ATOMIC_RELEASE);
				d->in_flight_tags[wkr] = 0;
				d->in_flight_bitmask &= ~(1UL << wkr);
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
rte_distributor_process_single(struct rte_distributor_single *d,
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
		uintptr_t oldbuf = 0;
		/* Sync with worker. Acquire bufptr64. */
		int64_t data = __atomic_load_n(&(d->bufs[wkr].bufptr64),
						__ATOMIC_ACQUIRE);

		if (!next_mb) {
			next_mb = mbufs[next_idx++];
			next_value = (((int64_t)(uintptr_t)next_mb)
					<< RTE_DISTRIB_FLAG_BITS);
			/*
			 * User is advocated to set tag value for each
			 * mbuf before calling rte_distributor_process.
			 * User defined tags are used to identify flows,
			 * or sessions.
			 */
			new_tag = next_mb->hash.usr;

			/*
			 * Note that if RTE_DISTRIB_MAX_WORKERS is larger than 64
			 * then the size of match has to be expanded.
			 */
			uint64_t match = 0;
			unsigned i;
			/*
			 * to scan for a match use "xor" and "not" to get a 0/1
			 * value, then use shifting to merge to single "match"
			 * variable, where a one-bit indicates a match for the
			 * worker given by the bit-position
			 */
			for (i = 0; i < d->num_workers; i++)
				match |= (!(d->in_flight_tags[i] ^ new_tag)
					<< i);

			/* Only turned-on bits are considered as match */
			match &= d->in_flight_bitmask;

			if (match) {
				next_mb = NULL;
				unsigned worker = __builtin_ctzl(match);
				if (add_to_backlog(&d->backlog[worker],
						next_value) < 0)
					next_idx--;
			}
		}

		if ((data & RTE_DISTRIB_GET_BUF) &&
				(d->backlog[wkr].count || next_mb)) {

			if (d->backlog[wkr].count)
				/* Sync with worker. Release bufptr64. */
				__atomic_store_n(&(d->bufs[wkr].bufptr64),
						backlog_pop(&d->backlog[wkr]),
						__ATOMIC_RELEASE);

			else {
				/* Sync with worker. Release bufptr64.  */
				__atomic_store_n(&(d->bufs[wkr].bufptr64),
						next_value,
						__ATOMIC_RELEASE);
				d->in_flight_tags[wkr] = new_tag;
				d->in_flight_bitmask |= (1UL << wkr);
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
				/* Sync with worker. Acquire bufptr64. */
				(__atomic_load_n(&(d->bufs[wkr].bufptr64),
				__ATOMIC_ACQUIRE) & RTE_DISTRIB_GET_BUF)) {

			int64_t oldbuf = d->bufs[wkr].bufptr64 >>
					RTE_DISTRIB_FLAG_BITS;

			store_return(oldbuf, d, &ret_start, &ret_count);

			/* Sync with worker. Release bufptr64. */
			__atomic_store_n(&(d->bufs[wkr].bufptr64),
				backlog_pop(&d->backlog[wkr]),
				__ATOMIC_RELEASE);
		}

	d->returns.start = ret_start;
	d->returns.count = ret_count;
	return num_mbufs;
}

/* return to the caller, packets returned from workers */
int
rte_distributor_returned_pkts_single(struct rte_distributor_single *d,
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
 * being worked on or queued up in a backlog.
 */
static inline unsigned
total_outstanding(const struct rte_distributor_single *d)
{
	unsigned wkr, total_outstanding;

	total_outstanding = __builtin_popcountl(d->in_flight_bitmask);

	for (wkr = 0; wkr < d->num_workers; wkr++)
		total_outstanding += d->backlog[wkr].count;

	return total_outstanding;
}

/* flush the distributor, so that there are no outstanding packets in flight or
 * queued up. */
int
rte_distributor_flush_single(struct rte_distributor_single *d)
{
	const unsigned flushed = total_outstanding(d);

	while (total_outstanding(d) > 0)
		rte_distributor_process_single(d, NULL, 0);

	return flushed;
}

/* clears the internal returns array in the distributor */
void
rte_distributor_clear_returns_single(struct rte_distributor_single *d)
{
	d->returns.start = d->returns.count = 0;
#ifndef __OPTIMIZE__
	memset(d->returns.mbufs, 0, sizeof(d->returns.mbufs));
#endif
}

/* creates a distributor instance */
struct rte_distributor_single *
rte_distributor_create_single(const char *name,
		unsigned socket_id,
		unsigned num_workers)
{
	struct rte_distributor_single *d;
	struct rte_distributor_list *distributor_list;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(*d) & RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((RTE_DISTRIB_MAX_WORKERS & 7) != 0);
	RTE_BUILD_BUG_ON(RTE_DISTRIB_MAX_WORKERS >
				sizeof(d->in_flight_bitmask) * CHAR_BIT);

	if (name == NULL || num_workers >= RTE_DISTRIB_MAX_WORKERS) {
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(mz_name, sizeof(mz_name), RTE_DISTRIB_PREFIX"%s", name);
	mz = rte_memzone_reserve(mz_name, sizeof(*d), socket_id, NO_FLAGS);
	if (mz == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	d = mz->addr;
	strlcpy(d->name, name, sizeof(d->name));
	d->num_workers = num_workers;

	distributor_list = RTE_TAILQ_CAST(rte_distributor_tailq.head,
					  rte_distributor_list);

	rte_mcfg_tailq_write_lock();
	TAILQ_INSERT_TAIL(distributor_list, d, next);
	rte_mcfg_tailq_write_unlock();

	return d;
}
