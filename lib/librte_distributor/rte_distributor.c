/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <sys/queue.h>
#include <string.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_cycles.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_eal_memconfig.h>
#include <rte_pause.h>
#include <rte_tailq.h>

#include "rte_distributor.h"
#include "rte_distributor_single.h"
#include "distributor_private.h"

TAILQ_HEAD(rte_dist_burst_list, rte_distributor);

static struct rte_tailq_elem rte_dist_burst_tailq = {
	.name = "RTE_DIST_BURST",
};
EAL_REGISTER_TAILQ(rte_dist_burst_tailq)

/**** APIs called by workers ****/

/**** Burst Packet APIs called by workers ****/

void
rte_distributor_request_pkt(struct rte_distributor *d,
		unsigned int worker_id, struct rte_mbuf **oldpkt,
		unsigned int count)
{
	struct rte_distributor_buffer *buf = &(d->bufs[worker_id]);
	unsigned int i;

	volatile int64_t *retptr64;

	if (unlikely(d->alg_type == RTE_DIST_ALG_SINGLE)) {
		rte_distributor_request_pkt_single(d->d_single,
			worker_id, count ? oldpkt[0] : NULL);
		return;
	}

	retptr64 = &(buf->retptr64[0]);
	/* Spin while handshake bits are set (scheduler clears it).
	 * Sync with worker on GET_BUF flag.
	 */
	while (unlikely(__atomic_load_n(retptr64, __ATOMIC_ACQUIRE)
			& (RTE_DISTRIB_GET_BUF | RTE_DISTRIB_RETURN_BUF))) {
		rte_pause();
		uint64_t t = rte_rdtsc()+100;

		while (rte_rdtsc() < t)
			rte_pause();
	}

	/*
	 * OK, if we've got here, then the scheduler has just cleared the
	 * handshake bits. Populate the retptrs with returning packets.
	 */

	for (i = count; i < RTE_DIST_BURST_SIZE; i++)
		buf->retptr64[i] = 0;

	/* Set VALID_BUF bit for each packet returned */
	for (i = count; i-- > 0; )
		buf->retptr64[i] =
			(((int64_t)(uintptr_t)(oldpkt[i])) <<
			RTE_DISTRIB_FLAG_BITS) | RTE_DISTRIB_VALID_BUF;

	/*
	 * Finally, set the GET_BUF  to signal to distributor that cache
	 * line is ready for processing
	 * Sync with distributor to release retptrs
	 */
	__atomic_store_n(retptr64, *retptr64 | RTE_DISTRIB_GET_BUF,
			__ATOMIC_RELEASE);
}

int
rte_distributor_poll_pkt(struct rte_distributor *d,
		unsigned int worker_id, struct rte_mbuf **pkts)
{
	struct rte_distributor_buffer *buf = &d->bufs[worker_id];
	uint64_t ret;
	int count = 0;
	unsigned int i;

	if (unlikely(d->alg_type == RTE_DIST_ALG_SINGLE)) {
		pkts[0] = rte_distributor_poll_pkt_single(d->d_single,
			worker_id);
		return (pkts[0]) ? 1 : 0;
	}

	/* If any of below bits is set, return.
	 * GET_BUF is set when distributor hasn't sent any packets yet
	 * RETURN_BUF is set when distributor must retrieve in-flight packets
	 * Sync with distributor to acquire bufptrs
	 */
	if (__atomic_load_n(&(buf->bufptr64[0]), __ATOMIC_ACQUIRE)
		& (RTE_DISTRIB_GET_BUF | RTE_DISTRIB_RETURN_BUF))
		return -1;

	/* since bufptr64 is signed, this should be an arithmetic shift */
	for (i = 0; i < RTE_DIST_BURST_SIZE; i++) {
		if (likely(buf->bufptr64[i] & RTE_DISTRIB_VALID_BUF)) {
			ret = buf->bufptr64[i] >> RTE_DISTRIB_FLAG_BITS;
			pkts[count++] = (struct rte_mbuf *)((uintptr_t)(ret));
		}
	}

	/*
	 * so now we've got the contents of the cacheline into an array of
	 * mbuf pointers, so toggle the bit so scheduler can start working
	 * on the next cacheline while we're working.
	 * Sync with distributor on GET_BUF flag. Release bufptrs.
	 */
	__atomic_store_n(&(buf->bufptr64[0]),
		buf->bufptr64[0] | RTE_DISTRIB_GET_BUF, __ATOMIC_RELEASE);

	return count;
}

int
rte_distributor_get_pkt(struct rte_distributor *d,
		unsigned int worker_id, struct rte_mbuf **pkts,
		struct rte_mbuf **oldpkt, unsigned int return_count)
{
	int count;

	if (unlikely(d->alg_type == RTE_DIST_ALG_SINGLE)) {
		if (return_count <= 1) {
			pkts[0] = rte_distributor_get_pkt_single(d->d_single,
				worker_id, return_count ? oldpkt[0] : NULL);
			return (pkts[0]) ? 1 : 0;
		} else
			return -EINVAL;
	}

	rte_distributor_request_pkt(d, worker_id, oldpkt, return_count);

	count = rte_distributor_poll_pkt(d, worker_id, pkts);
	while (count == -1) {
		uint64_t t = rte_rdtsc() + 100;

		while (rte_rdtsc() < t)
			rte_pause();

		count = rte_distributor_poll_pkt(d, worker_id, pkts);
	}
	return count;
}

int
rte_distributor_return_pkt(struct rte_distributor *d,
		unsigned int worker_id, struct rte_mbuf **oldpkt, int num)
{
	struct rte_distributor_buffer *buf = &d->bufs[worker_id];
	unsigned int i;

	if (unlikely(d->alg_type == RTE_DIST_ALG_SINGLE)) {
		if (num == 1)
			return rte_distributor_return_pkt_single(d->d_single,
				worker_id, oldpkt[0]);
		else if (num == 0)
			return rte_distributor_return_pkt_single(d->d_single,
				worker_id, NULL);
		else
			return -EINVAL;
	}

	/* Spin while handshake bits are set (scheduler clears it).
	 * Sync with worker on GET_BUF flag.
	 */
	while (unlikely(__atomic_load_n(&(buf->retptr64[0]), __ATOMIC_RELAXED)
			& (RTE_DISTRIB_GET_BUF | RTE_DISTRIB_RETURN_BUF))) {
		rte_pause();
		uint64_t t = rte_rdtsc()+100;

		while (rte_rdtsc() < t)
			rte_pause();
	}

	/* Sync with distributor to acquire retptrs */
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	for (i = 0; i < RTE_DIST_BURST_SIZE; i++)
		/* Switch off the return bit first */
		buf->retptr64[i] = 0;

	for (i = num; i-- > 0; )
		buf->retptr64[i] = (((int64_t)(uintptr_t)oldpkt[i]) <<
			RTE_DISTRIB_FLAG_BITS) | RTE_DISTRIB_VALID_BUF;

	/* Use RETURN_BUF on bufptr64 to notify distributor that
	 * we won't read any mbufs from there even if GET_BUF is set.
	 * This allows distributor to retrieve in-flight already sent packets.
	 */
	__atomic_or_fetch(&(buf->bufptr64[0]), RTE_DISTRIB_RETURN_BUF,
		__ATOMIC_ACQ_REL);

	/* set the RETURN_BUF on retptr64 even if we got no returns.
	 * Sync with distributor on RETURN_BUF flag. Release retptrs.
	 * Notify distributor that we don't request more packets any more.
	 */
	__atomic_store_n(&(buf->retptr64[0]),
		buf->retptr64[0] | RTE_DISTRIB_RETURN_BUF, __ATOMIC_RELEASE);

	return 0;
}

/**** APIs called on distributor core ***/

/* stores a packet returned from a worker inside the returns array */
static inline void
store_return(uintptr_t oldbuf, struct rte_distributor *d,
		unsigned int *ret_start, unsigned int *ret_count)
{
	if (!oldbuf)
		return;
	/* store returns in a circular buffer */
	d->returns.mbufs[(*ret_start + *ret_count) & RTE_DISTRIB_RETURNS_MASK]
			= (void *)oldbuf;
	*ret_start += (*ret_count == RTE_DISTRIB_RETURNS_MASK);
	*ret_count += (*ret_count != RTE_DISTRIB_RETURNS_MASK);
}

/*
 * Match then flow_ids (tags) of the incoming packets to the flow_ids
 * of the inflight packets (both inflight on the workers and in each worker
 * backlog). This will then allow us to pin those packets to the relevant
 * workers to give us our atomic flow pinning.
 */
void
find_match_scalar(struct rte_distributor *d,
			uint16_t *data_ptr,
			uint16_t *output_ptr)
{
	struct rte_distributor_backlog *bl;
	uint16_t i, j, w;

	/*
	 * Function overview:
	 * 1. Loop through all worker ID's
	 * 2. Compare the current inflights to the incoming tags
	 * 3. Compare the current backlog to the incoming tags
	 * 4. Add any matches to the output
	 */

	for (j = 0 ; j < RTE_DIST_BURST_SIZE; j++)
		output_ptr[j] = 0;

	for (i = 0; i < d->num_workers; i++) {
		bl = &d->backlog[i];

		for (j = 0; j < RTE_DIST_BURST_SIZE ; j++)
			for (w = 0; w < RTE_DIST_BURST_SIZE; w++)
				if (d->in_flight_tags[i][w] == data_ptr[j]) {
					output_ptr[j] = i+1;
					break;
				}
		for (j = 0; j < RTE_DIST_BURST_SIZE; j++)
			for (w = 0; w < RTE_DIST_BURST_SIZE; w++)
				if (bl->tags[w] == data_ptr[j]) {
					output_ptr[j] = i+1;
					break;
				}
	}

	/*
	 * At this stage, the output contains 8 16-bit values, with
	 * each non-zero value containing the worker ID on which the
	 * corresponding flow is pinned to.
	 */
}

/*
 * When worker called rte_distributor_return_pkt()
 * and passed RTE_DISTRIB_RETURN_BUF handshake through retptr64,
 * distributor must retrieve both inflight and backlog packets assigned
 * to the worker and reprocess them to another worker.
 */
static void
handle_worker_shutdown(struct rte_distributor *d, unsigned int wkr)
{
	struct rte_distributor_buffer *buf = &(d->bufs[wkr]);
	/* double BURST size for storing both inflights and backlog */
	struct rte_mbuf *pkts[RTE_DIST_BURST_SIZE * 2];
	unsigned int pkts_count = 0;
	unsigned int i;

	/* If GET_BUF is cleared there are in-flight packets sent
	 * to worker which does not require new packets.
	 * They must be retrieved and assigned to another worker.
	 */
	if (!(__atomic_load_n(&(buf->bufptr64[0]), __ATOMIC_ACQUIRE)
		& RTE_DISTRIB_GET_BUF))
		for (i = 0; i < RTE_DIST_BURST_SIZE; i++)
			if (buf->bufptr64[i] & RTE_DISTRIB_VALID_BUF)
				pkts[pkts_count++] = (void *)((uintptr_t)
					(buf->bufptr64[i]
						>> RTE_DISTRIB_FLAG_BITS));

	/* Make following operations on handshake flags on bufptr64:
	 * - set GET_BUF to indicate that distributor can overwrite buffer
	 *     with new packets if worker will make a new request.
	 * - clear RETURN_BUF to unlock reads on worker side.
	 */
	__atomic_store_n(&(buf->bufptr64[0]), RTE_DISTRIB_GET_BUF,
		__ATOMIC_RELEASE);

	/* Collect backlog packets from worker */
	for (i = 0; i < d->backlog[wkr].count; i++)
		pkts[pkts_count++] = (void *)((uintptr_t)
			(d->backlog[wkr].pkts[i] >> RTE_DISTRIB_FLAG_BITS));

	d->backlog[wkr].count = 0;

	/* Clear both inflight and backlog tags */
	for (i = 0; i < RTE_DIST_BURST_SIZE; i++) {
		d->in_flight_tags[wkr][i] = 0;
		d->backlog[wkr].tags[i] = 0;
	}

	/* Recursive call */
	if (pkts_count > 0)
		rte_distributor_process(d, pkts, pkts_count);
}


/*
 * When the handshake bits indicate that there are packets coming
 * back from the worker, this function is called to copy and store
 * the valid returned pointers (store_return).
 */
static unsigned int
handle_returns(struct rte_distributor *d, unsigned int wkr)
{
	struct rte_distributor_buffer *buf = &(d->bufs[wkr]);
	uintptr_t oldbuf;
	unsigned int ret_start = d->returns.start,
			ret_count = d->returns.count;
	unsigned int count = 0;
	unsigned int i;

	/* Sync on GET_BUF flag. Acquire retptrs. */
	if (__atomic_load_n(&(buf->retptr64[0]), __ATOMIC_ACQUIRE)
		& (RTE_DISTRIB_GET_BUF | RTE_DISTRIB_RETURN_BUF)) {
		for (i = 0; i < RTE_DIST_BURST_SIZE; i++) {
			if (buf->retptr64[i] & RTE_DISTRIB_VALID_BUF) {
				oldbuf = ((uintptr_t)(buf->retptr64[i] >>
					RTE_DISTRIB_FLAG_BITS));
				/* store returns in a circular buffer */
				store_return(oldbuf, d, &ret_start, &ret_count);
				count++;
				buf->retptr64[i] &= ~RTE_DISTRIB_VALID_BUF;
			}
		}
		d->returns.start = ret_start;
		d->returns.count = ret_count;

		/* If worker requested packets with GET_BUF, set it to active
		 * otherwise (RETURN_BUF), set it to not active.
		 */
		d->activesum -= d->active[wkr];
		d->active[wkr] = !!(buf->retptr64[0] & RTE_DISTRIB_GET_BUF);
		d->activesum += d->active[wkr];

		/* If worker returned packets without requesting new ones,
		 * handle all in-flights and backlog packets assigned to it.
		 */
		if (unlikely(buf->retptr64[0] & RTE_DISTRIB_RETURN_BUF))
			handle_worker_shutdown(d, wkr);

		/* Clear for the worker to populate with more returns.
		 * Sync with distributor on GET_BUF flag. Release retptrs.
		 */
		__atomic_store_n(&(buf->retptr64[0]), 0, __ATOMIC_RELEASE);
	}
	return count;
}

/*
 * This function releases a burst (cache line) to a worker.
 * It is called from the process function when a cacheline is
 * full to make room for more packets for that worker, or when
 * all packets have been assigned to bursts and need to be flushed
 * to the workers.
 * It also needs to wait for any outstanding packets from the worker
 * before sending out new packets.
 */
static unsigned int
release(struct rte_distributor *d, unsigned int wkr)
{
	struct rte_distributor_buffer *buf = &(d->bufs[wkr]);
	unsigned int i;

	handle_returns(d, wkr);
	if (unlikely(!d->active[wkr]))
		return 0;

	/* Sync with worker on GET_BUF flag */
	while (!(__atomic_load_n(&(d->bufs[wkr].bufptr64[0]), __ATOMIC_ACQUIRE)
		& RTE_DISTRIB_GET_BUF)) {
		handle_returns(d, wkr);
		if (unlikely(!d->active[wkr]))
			return 0;
		rte_pause();
	}

	buf->count = 0;

	for (i = 0; i < d->backlog[wkr].count; i++) {
		d->bufs[wkr].bufptr64[i] = d->backlog[wkr].pkts[i] |
				RTE_DISTRIB_GET_BUF | RTE_DISTRIB_VALID_BUF;
		d->in_flight_tags[wkr][i] = d->backlog[wkr].tags[i];
	}
	buf->count = i;
	for ( ; i < RTE_DIST_BURST_SIZE ; i++) {
		buf->bufptr64[i] = RTE_DISTRIB_GET_BUF;
		d->in_flight_tags[wkr][i] = 0;
	}

	d->backlog[wkr].count = 0;

	/* Clear the GET bit.
	 * Sync with worker on GET_BUF flag. Release bufptrs.
	 */
	__atomic_store_n(&(buf->bufptr64[0]),
		buf->bufptr64[0] & ~RTE_DISTRIB_GET_BUF, __ATOMIC_RELEASE);
	return  buf->count;

}


/* process a set of packets to distribute them to workers */
int
rte_distributor_process(struct rte_distributor *d,
		struct rte_mbuf **mbufs, unsigned int num_mbufs)
{
	unsigned int next_idx = 0;
	static unsigned int wkr;
	struct rte_mbuf *next_mb = NULL;
	int64_t next_value = 0;
	uint16_t new_tag = 0;
	uint16_t flows[RTE_DIST_BURST_SIZE] __rte_cache_aligned;
	unsigned int i, j, w, wid, matching_required;

	if (d->alg_type == RTE_DIST_ALG_SINGLE) {
		/* Call the old API */
		return rte_distributor_process_single(d->d_single,
			mbufs, num_mbufs);
	}

	for (wid = 0 ; wid < d->num_workers; wid++)
		handle_returns(d, wid);

	if (unlikely(num_mbufs == 0)) {
		/* Flush out all non-full cache-lines to workers. */
		for (wid = 0 ; wid < d->num_workers; wid++) {
			/* Sync with worker on GET_BUF flag. */
			if (__atomic_load_n(&(d->bufs[wid].bufptr64[0]),
				__ATOMIC_ACQUIRE) & RTE_DISTRIB_GET_BUF) {
				d->bufs[wid].count = 0;
				release(d, wid);
				handle_returns(d, wid);
			}
		}
		return 0;
	}

	if (unlikely(!d->activesum))
		return 0;

	while (next_idx < num_mbufs) {
		uint16_t matches[RTE_DIST_BURST_SIZE];
		unsigned int pkts;

		if ((num_mbufs - next_idx) < RTE_DIST_BURST_SIZE)
			pkts = num_mbufs - next_idx;
		else
			pkts = RTE_DIST_BURST_SIZE;

		for (i = 0; i < pkts; i++) {
			if (mbufs[next_idx + i]) {
				/* flows have to be non-zero */
				flows[i] = mbufs[next_idx + i]->hash.usr | 1;
			} else
				flows[i] = 0;
		}
		for (; i < RTE_DIST_BURST_SIZE; i++)
			flows[i] = 0;

		matching_required = 1;

		for (j = 0; j < pkts; j++) {
			if (unlikely(!d->activesum))
				return next_idx;

			if (unlikely(matching_required)) {
				switch (d->dist_match_fn) {
				case RTE_DIST_MATCH_VECTOR:
					find_match_vec(d, &flows[0],
						&matches[0]);
					break;
				default:
					find_match_scalar(d, &flows[0],
						&matches[0]);
				}
				matching_required = 0;
			}
		/*
		 * Matches array now contain the intended worker ID (+1) of
		 * the incoming packets. Any zeroes need to be assigned
		 * workers.
		 */

			next_mb = mbufs[next_idx++];
			next_value = (((int64_t)(uintptr_t)next_mb) <<
					RTE_DISTRIB_FLAG_BITS);
			/*
			 * User is advocated to set tag value for each
			 * mbuf before calling rte_distributor_process.
			 * User defined tags are used to identify flows,
			 * or sessions.
			 */
			/* flows MUST be non-zero */
			new_tag = (uint16_t)(next_mb->hash.usr) | 1;

			/*
			 * Uncommenting the next line will cause the find_match
			 * function to be optimized out, making this function
			 * do parallel (non-atomic) distribution
			 */
			/* matches[j] = 0; */

			if (matches[j] && d->active[matches[j]-1]) {
				struct rte_distributor_backlog *bl =
						&d->backlog[matches[j]-1];
				if (unlikely(bl->count ==
						RTE_DIST_BURST_SIZE)) {
					release(d, matches[j]-1);
					if (!d->active[matches[j]-1]) {
						j--;
						next_idx--;
						matching_required = 1;
						continue;
					}
				}

				/* Add to worker that already has flow */
				unsigned int idx = bl->count++;

				bl->tags[idx] = new_tag;
				bl->pkts[idx] = next_value;

			} else {
				struct rte_distributor_backlog *bl;

				while (unlikely(!d->active[wkr]))
					wkr = (wkr + 1) % d->num_workers;
				bl = &d->backlog[wkr];

				if (unlikely(bl->count ==
						RTE_DIST_BURST_SIZE)) {
					release(d, wkr);
					if (!d->active[wkr]) {
						j--;
						next_idx--;
						matching_required = 1;
						continue;
					}
				}

				/* Add to current worker worker */
				unsigned int idx = bl->count++;

				bl->tags[idx] = new_tag;
				bl->pkts[idx] = next_value;
				/*
				 * Now that we've just added an unpinned flow
				 * to a worker, we need to ensure that all
				 * other packets with that same flow will go
				 * to the same worker in this burst.
				 */
				for (w = j; w < pkts; w++)
					if (flows[w] == new_tag)
						matches[w] = wkr+1;
			}
		}
		wkr = (wkr + 1) % d->num_workers;
	}

	/* Flush out all non-full cache-lines to workers. */
	for (wid = 0 ; wid < d->num_workers; wid++)
		/* Sync with worker on GET_BUF flag. */
		if ((__atomic_load_n(&(d->bufs[wid].bufptr64[0]),
			__ATOMIC_ACQUIRE) & RTE_DISTRIB_GET_BUF)) {
			d->bufs[wid].count = 0;
			release(d, wid);
		}

	return num_mbufs;
}

/* return to the caller, packets returned from workers */
int
rte_distributor_returned_pkts(struct rte_distributor *d,
		struct rte_mbuf **mbufs, unsigned int max_mbufs)
{
	struct rte_distributor_returned_pkts *returns = &d->returns;
	unsigned int retval = (max_mbufs < returns->count) ?
			max_mbufs : returns->count;
	unsigned int i;

	if (d->alg_type == RTE_DIST_ALG_SINGLE) {
		/* Call the old API */
		return rte_distributor_returned_pkts_single(d->d_single,
				mbufs, max_mbufs);
	}

	for (i = 0; i < retval; i++) {
		unsigned int idx = (returns->start + i) &
				RTE_DISTRIB_RETURNS_MASK;

		mbufs[i] = returns->mbufs[idx];
	}
	returns->start += i;
	returns->count -= i;

	return retval;
}

/*
 * Return the number of packets in-flight in a distributor, i.e. packets
 * being worked on or queued up in a backlog.
 */
static inline unsigned int
total_outstanding(const struct rte_distributor *d)
{
	unsigned int wkr, total_outstanding = 0;

	for (wkr = 0; wkr < d->num_workers; wkr++)
		total_outstanding += d->backlog[wkr].count + d->bufs[wkr].count;

	return total_outstanding;
}

/*
 * Flush the distributor, so that there are no outstanding packets in flight or
 * queued up.
 */
int
rte_distributor_flush(struct rte_distributor *d)
{
	unsigned int flushed;
	unsigned int wkr;

	if (d->alg_type == RTE_DIST_ALG_SINGLE) {
		/* Call the old API */
		return rte_distributor_flush_single(d->d_single);
	}

	flushed = total_outstanding(d);

	while (total_outstanding(d) > 0)
		rte_distributor_process(d, NULL, 0);

	/* wait 10ms to allow all worker drain the pkts */
	rte_delay_us(10000);

	/*
	 * Send empty burst to all workers to allow them to exit
	 * gracefully, should they need to.
	 */
	rte_distributor_process(d, NULL, 0);

	for (wkr = 0; wkr < d->num_workers; wkr++)
		handle_returns(d, wkr);

	return flushed;
}

/* clears the internal returns array in the distributor */
void
rte_distributor_clear_returns(struct rte_distributor *d)
{
	unsigned int wkr;

	if (d->alg_type == RTE_DIST_ALG_SINGLE) {
		/* Call the old API */
		rte_distributor_clear_returns_single(d->d_single);
		return;
	}

	/* throw away returns, so workers can exit */
	for (wkr = 0; wkr < d->num_workers; wkr++)
		/* Sync with worker. Release retptrs. */
		__atomic_store_n(&(d->bufs[wkr].retptr64[0]), 0,
				__ATOMIC_RELEASE);
}

/* creates a distributor instance */
struct rte_distributor *
rte_distributor_create(const char *name,
		unsigned int socket_id,
		unsigned int num_workers,
		unsigned int alg_type)
{
	struct rte_distributor *d;
	struct rte_dist_burst_list *dist_burst_list;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	unsigned int i;

	/* TODO Reorganise function properly around RTE_DIST_ALG_SINGLE/BURST */

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(*d) & RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((RTE_DISTRIB_MAX_WORKERS & 7) != 0);

	if (name == NULL || num_workers >=
		(unsigned int)RTE_MIN(RTE_DISTRIB_MAX_WORKERS, RTE_MAX_LCORE)) {
		rte_errno = EINVAL;
		return NULL;
	}

	if (alg_type == RTE_DIST_ALG_SINGLE) {
		d = malloc(sizeof(struct rte_distributor));
		if (d == NULL) {
			rte_errno = ENOMEM;
			return NULL;
		}
		d->d_single = rte_distributor_create_single(name,
				socket_id, num_workers);
		if (d->d_single == NULL) {
			free(d);
			/* rte_errno will have been set */
			return NULL;
		}
		d->alg_type = alg_type;
		return d;
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
	d->alg_type = alg_type;

	d->dist_match_fn = RTE_DIST_MATCH_SCALAR;
#if defined(RTE_ARCH_X86)
	d->dist_match_fn = RTE_DIST_MATCH_VECTOR;
#endif

	/*
	 * Set up the backlog tags so they're pointing at the second cache
	 * line for performance during flow matching
	 */
	for (i = 0 ; i < num_workers ; i++)
		d->backlog[i].tags = &d->in_flight_tags[i][RTE_DIST_BURST_SIZE];

	memset(d->active, 0, sizeof(d->active));
	d->activesum = 0;

	dist_burst_list = RTE_TAILQ_CAST(rte_dist_burst_tailq.head,
					  rte_dist_burst_list);


	rte_mcfg_tailq_write_lock();
	TAILQ_INSERT_TAIL(dist_burst_list, d, next);
	rte_mcfg_tailq_write_unlock();

	return d;
}
