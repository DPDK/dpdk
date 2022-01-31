/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_SECURITY_AR_H__
#define __CNXK_SECURITY_AR_H__

#include <rte_mbuf.h>

#include "cnxk_security.h"

#define CNXK_ON_AR_WIN_SIZE_MAX 1024

/* u64 array size to fit anti replay window bits */
#define AR_WIN_ARR_SZ                                                          \
	(PLT_ALIGN_CEIL(CNXK_ON_AR_WIN_SIZE_MAX + 1, BITS_PER_LONG_LONG) /     \
	 BITS_PER_LONG_LONG)

#define WORD_SHIFT 6
#define WORD_SIZE  (1 << WORD_SHIFT)
#define WORD_MASK  (WORD_SIZE - 1)

#define IPSEC_ANTI_REPLAY_FAILED (-1)

struct cnxk_on_ipsec_ar {
	rte_spinlock_t lock;
	uint32_t winb;
	uint32_t wint;
	uint64_t base;			/**< base of the anti-replay window */
	uint64_t window[AR_WIN_ARR_SZ]; /**< anti-replay window */
};

static inline uint32_t
cnxk_on_anti_replay_get_seqh(uint32_t winsz, uint32_t seql, uint32_t esn_hi,
			     uint32_t esn_low)
{
	uint32_t win_low = esn_low - winsz + 1;

	if (esn_low > winsz - 1) {
		/* Window is in one sequence number subspace */
		if (seql > win_low)
			return esn_hi;
		else
			return esn_hi + 1;
	} else {
		/* Window is split across two sequence number subspaces */
		if (seql > win_low)
			return esn_hi - 1;
		else
			return esn_hi;
	}
}

static inline int
cnxk_on_anti_replay_check(uint64_t seq, struct cnxk_on_ipsec_ar *ar,
			  uint32_t winsz)
{
	uint64_t ex_winsz = winsz + WORD_SIZE;
	uint64_t *window = &ar->window[0];
	uint64_t seqword, shiftwords;
	uint64_t base = ar->base;
	uint32_t winb = ar->winb;
	uint32_t wint = ar->wint;
	uint64_t winwords;
	uint64_t bit_pos;
	uint64_t shift;
	uint64_t *wptr;
	uint64_t tmp;

	winwords = ex_winsz >> WORD_SHIFT;
	if (winsz > 64)
		goto slow_shift;
	/* Check if the seq is the biggest one yet */
	if (likely(seq > base)) {
		shift = seq - base;
		if (shift < winsz) { /* In window */
			/*
			 * If more than 64-bit anti-replay window,
			 * use slow shift routine
			 */
			wptr = window + (shift >> WORD_SHIFT);
			*wptr <<= shift;
			*wptr |= 1ull;
		} else {
			/* No special handling of window size > 64 */
			wptr = window + ((winsz - 1) >> WORD_SHIFT);
			/*
			 * Zero out the whole window (especially for
			 * bigger than 64b window) till the last 64b word
			 * as the incoming sequence number minus
			 * base sequence is more than the window size.
			 */
			while (window != wptr)
				*window++ = 0ull;
			/*
			 * Set the last bit (of the window) to 1
			 * as that corresponds to the base sequence number.
			 * Now any incoming sequence number which is
			 * (base - window size - 1) will pass anti-replay check
			 */
			*wptr = 1ull;
		}
		/*
		 * Set the base to incoming sequence number as
		 * that is the biggest sequence number seen yet
		 */
		ar->base = seq;
		return 0;
	}

	bit_pos = base - seq;

	/* If seq falls behind the window, return failure */
	if (bit_pos >= winsz)
		return IPSEC_ANTI_REPLAY_FAILED;

	/* seq is within anti-replay window */
	wptr = window + ((winsz - bit_pos - 1) >> WORD_SHIFT);
	bit_pos &= WORD_MASK;

	/* Check if this is a replayed packet */
	if (*wptr & ((1ull) << bit_pos))
		return IPSEC_ANTI_REPLAY_FAILED;

	/* mark as seen */
	*wptr |= ((1ull) << bit_pos);
	return 0;

slow_shift:
	if (likely(seq > base)) {
		uint32_t i;

		shift = seq - base;
		if (unlikely(shift >= winsz)) {
			/*
			 * shift is bigger than the window,
			 * so just zero out everything
			 */
			for (i = 0; i < winwords; i++)
				window[i] = 0;
winupdate:
			/* Find out the word */
			seqword = ((seq - 1) % ex_winsz) >> WORD_SHIFT;

			/* Find out the bit in the word */
			bit_pos = (seq - 1) & WORD_MASK;

			/*
			 * Set the bit corresponding to sequence number
			 * in window to mark it as received
			 */
			window[seqword] |= (1ull << (63 - bit_pos));

			/* wint and winb range from 1 to ex_winsz */
			ar->wint = ((wint + shift - 1) % ex_winsz) + 1;
			ar->winb = ((winb + shift - 1) % ex_winsz) + 1;

			ar->base = seq;
			return 0;
		}

		/*
		 * New sequence number is bigger than the base but
		 * it's not bigger than base + window size
		 */

		shiftwords = ((wint + shift - 1) >> WORD_SHIFT) -
			     ((wint - 1) >> WORD_SHIFT);
		if (unlikely(shiftwords)) {
			tmp = (wint + WORD_SIZE - 1) / WORD_SIZE;
			for (i = 0; i < shiftwords; i++) {
				tmp %= winwords;
				window[tmp++] = 0;
			}
		}

		goto winupdate;
	}

	/* Sequence number is before the window */
	if (unlikely((seq + winsz) <= base))
		return IPSEC_ANTI_REPLAY_FAILED;

	/* Sequence number is within the window */

	/* Find out the word */
	seqword = ((seq - 1) % ex_winsz) >> WORD_SHIFT;

	/* Find out the bit in the word */
	bit_pos = (seq - 1) & WORD_MASK;

	/* Check if this is a replayed packet */
	if (window[seqword] & (1ull << (63 - bit_pos)))
		return IPSEC_ANTI_REPLAY_FAILED;

	/*
	 * Set the bit corresponding to sequence number
	 * in window to mark it as received
	 */
	window[seqword] |= (1ull << (63 - bit_pos));

	return 0;
}

#endif /* __CNXK_SECURITY_AR_H__ */
