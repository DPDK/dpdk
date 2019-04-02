/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _IPSEC_SQN_H_
#define _IPSEC_SQN_H_

#define WINDOW_BUCKET_BITS		6 /* uint64_t */
#define WINDOW_BUCKET_SIZE		(1 << WINDOW_BUCKET_BITS)
#define WINDOW_BIT_LOC_MASK		(WINDOW_BUCKET_SIZE - 1)

/* minimum number of bucket, power of 2*/
#define WINDOW_BUCKET_MIN		2
#define WINDOW_BUCKET_MAX		(INT16_MAX + 1)

#define IS_ESN(sa)	((sa)->sqn_mask == UINT64_MAX)

#define	SQN_ATOMIC(sa)	((sa)->type & RTE_IPSEC_SATP_SQN_ATOM)

/*
 * gets SQN.hi32 bits, SQN supposed to be in network byte order.
 */
static inline rte_be32_t
sqn_hi32(rte_be64_t sqn)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	return (sqn >> 32);
#else
	return sqn;
#endif
}

/*
 * gets SQN.low32 bits, SQN supposed to be in network byte order.
 */
static inline rte_be32_t
sqn_low32(rte_be64_t sqn)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	return sqn;
#else
	return (sqn >> 32);
#endif
}

/*
 * gets SQN.low16 bits, SQN supposed to be in network byte order.
 */
static inline rte_be16_t
sqn_low16(rte_be64_t sqn)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	return sqn;
#else
	return (sqn >> 48);
#endif
}

/*
 * According to RFC4303 A2.1, determine the high-order bit of sequence number.
 * use 32bit arithmetic inside, return uint64_t.
 */
static inline uint64_t
reconstruct_esn(uint64_t t, uint32_t sqn, uint32_t w)
{
	uint32_t th, tl, bl;

	tl = t;
	th = t >> 32;
	bl = tl - w + 1;

	/* case A: window is within one sequence number subspace */
	if (tl >= (w - 1))
		th += (sqn < bl);
	/* case B: window spans two sequence number subspaces */
	else if (th != 0)
		th -= (sqn >= bl);

	/* return constructed sequence with proper high-order bits */
	return (uint64_t)th << 32 | sqn;
}

/**
 * Perform the replay checking.
 *
 * struct rte_ipsec_sa contains the window and window related parameters,
 * such as the window size, bitmask, and the last acknowledged sequence number.
 *
 * Based on RFC 6479.
 * Blocks are 64 bits unsigned integers
 */
static inline int32_t
esn_inb_check_sqn(const struct replay_sqn *rsn, const struct rte_ipsec_sa *sa,
	uint64_t sqn)
{
	uint32_t bit, bucket;

	/* replay not enabled */
	if (sa->replay.win_sz == 0)
		return 0;

	/* seq is larger than lastseq */
	if (sqn > rsn->sqn)
		return 0;

	/* seq is outside window */
	if (sqn == 0 || sqn + sa->replay.win_sz < rsn->sqn)
		return -EINVAL;

	/* seq is inside the window */
	bit = sqn & WINDOW_BIT_LOC_MASK;
	bucket = (sqn >> WINDOW_BUCKET_BITS) & sa->replay.bucket_index_mask;

	/* already seen packet */
	if (rsn->window[bucket] & ((uint64_t)1 << bit))
		return -EINVAL;

	return 0;
}

/**
 * For outbound SA perform the sequence number update.
 */
static inline uint64_t
esn_outb_update_sqn(struct rte_ipsec_sa *sa, uint32_t *num)
{
	uint64_t n, s, sqn;

	n = *num;
	if (SQN_ATOMIC(sa))
		sqn = (uint64_t)rte_atomic64_add_return(&sa->sqn.outb.atom, n);
	else {
		sqn = sa->sqn.outb.raw + n;
		sa->sqn.outb.raw = sqn;
	}

	/* overflow */
	if (sqn > sa->sqn_mask) {
		s = sqn - sa->sqn_mask;
		*num = (s < n) ?  n - s : 0;
	}

	return sqn - n;
}

/**
 * For inbound SA perform the sequence number and replay window update.
 */
static inline int32_t
esn_inb_update_sqn(struct replay_sqn *rsn, const struct rte_ipsec_sa *sa,
	uint64_t sqn)
{
	uint32_t bit, bucket, last_bucket, new_bucket, diff, i;

	/* handle ESN */
	if (IS_ESN(sa))
		sqn = reconstruct_esn(rsn->sqn, sqn, sa->replay.win_sz);

	/* seq is outside window*/
	if (sqn == 0 || sqn + sa->replay.win_sz < rsn->sqn)
		return -EINVAL;

	/* update the bit */
	bucket = (sqn >> WINDOW_BUCKET_BITS);

	/* check if the seq is within the range */
	if (sqn > rsn->sqn) {
		last_bucket = rsn->sqn >> WINDOW_BUCKET_BITS;
		diff = bucket - last_bucket;
		/* seq is way after the range of WINDOW_SIZE */
		if (diff > sa->replay.nb_bucket)
			diff = sa->replay.nb_bucket;

		for (i = 0; i != diff; i++) {
			new_bucket = (i + last_bucket + 1) &
				sa->replay.bucket_index_mask;
			rsn->window[new_bucket] = 0;
		}
		rsn->sqn = sqn;
	}

	bucket &= sa->replay.bucket_index_mask;
	bit = (uint64_t)1 << (sqn & WINDOW_BIT_LOC_MASK);

	/* already seen packet */
	if (rsn->window[bucket] & bit)
		return -EINVAL;

	rsn->window[bucket] |= bit;
	return 0;
}

/**
 * To achieve ability to do multiple readers single writer for
 * SA replay window information and sequence number (RSN)
 * basic RCU schema is used:
 * SA have 2 copies of RSN (one for readers, another for writers).
 * Each RSN contains a rwlock that has to be grabbed (for read/write)
 * to avoid races between readers and writer.
 * Writer is responsible to make a copy or reader RSN, update it
 * and mark newly updated RSN as readers one.
 * That approach is intended to minimize contention and cache sharing
 * between writer and readers.
 */

/**
 * Copy replay window and SQN.
 */
static inline void
rsn_copy(const struct rte_ipsec_sa *sa, uint32_t dst, uint32_t src)
{
	uint32_t i, n;
	struct replay_sqn *d;
	const struct replay_sqn *s;

	d = sa->sqn.inb.rsn[dst];
	s = sa->sqn.inb.rsn[src];

	n = sa->replay.nb_bucket;

	d->sqn = s->sqn;
	for (i = 0; i != n; i++)
		d->window[i] = s->window[i];
}

/**
 * Get RSN for read-only access.
 */
static inline struct replay_sqn *
rsn_acquire(struct rte_ipsec_sa *sa)
{
	uint32_t n;
	struct replay_sqn *rsn;

	n = sa->sqn.inb.rdidx;
	rsn = sa->sqn.inb.rsn[n];

	if (!SQN_ATOMIC(sa))
		return rsn;

	/* check there are no writers */
	while (rte_rwlock_read_trylock(&rsn->rwl) < 0) {
		rte_pause();
		n = sa->sqn.inb.rdidx;
		rsn = sa->sqn.inb.rsn[n];
		rte_compiler_barrier();
	}

	return rsn;
}

/**
 * Release read-only access for RSN.
 */
static inline void
rsn_release(struct rte_ipsec_sa *sa, struct replay_sqn *rsn)
{
	if (SQN_ATOMIC(sa))
		rte_rwlock_read_unlock(&rsn->rwl);
}

/**
 * Start RSN update.
 */
static inline struct replay_sqn *
rsn_update_start(struct rte_ipsec_sa *sa)
{
	uint32_t k, n;
	struct replay_sqn *rsn;

	n = sa->sqn.inb.wridx;

	/* no active writers */
	RTE_ASSERT(n == sa->sqn.inb.rdidx);

	if (!SQN_ATOMIC(sa))
		return sa->sqn.inb.rsn[n];

	k = REPLAY_SQN_NEXT(n);
	sa->sqn.inb.wridx = k;

	rsn = sa->sqn.inb.rsn[k];
	rte_rwlock_write_lock(&rsn->rwl);
	rsn_copy(sa, k, n);

	return rsn;
}

/**
 * Finish RSN update.
 */
static inline void
rsn_update_finish(struct rte_ipsec_sa *sa, struct replay_sqn *rsn)
{
	uint32_t n;

	if (!SQN_ATOMIC(sa))
		return;

	n = sa->sqn.inb.wridx;
	RTE_ASSERT(n != sa->sqn.inb.rdidx);
	RTE_ASSERT(rsn == sa->sqn.inb.rsn[n]);

	rte_rwlock_write_unlock(&rsn->rwl);
	sa->sqn.inb.rdidx = n;
}


#endif /* _IPSEC_SQN_H_ */
