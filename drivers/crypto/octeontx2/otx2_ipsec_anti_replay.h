/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_IPSEC_ANTI_REPLAY_H__
#define __OTX2_IPSEC_ANTI_REPLAY_H__

#include <rte_mbuf.h>

#include "otx2_ipsec_fp.h"

#define WORD_SHIFT	6
#define WORD_SIZE	(1 << WORD_SHIFT)
#define WORD_MASK	(WORD_SIZE - 1)

#define IPSEC_ANTI_REPLAY_FAILED	(-1)

static inline int
anti_replay_check(struct otx2_ipsec_replay *replay, uint64_t seq,
			uint64_t winsz)
{
	uint64_t *window = &replay->window[0];
	uint64_t ex_winsz = winsz + WORD_SIZE;
	uint64_t winwords = ex_winsz >> WORD_SHIFT;
	uint64_t base = replay->base;
	uint32_t winb = replay->winb;
	uint32_t wint = replay->wint;
	uint64_t seqword, shiftwords;
	uint64_t bit_pos;
	uint64_t shift;
	uint64_t *wptr;
	uint64_t tmp;

	if (winsz > 64)
		goto slow_shift;
	/* Check if the seq is the biggest one yet */
	if (likely(seq > base)) {
		shift = seq - base;
		if (shift < winsz) {  /* In window */
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
		replay->base = seq;
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
			replay->wint = ((wint + shift - 1) % ex_winsz) + 1;
			replay->winb = ((winb + shift - 1) % ex_winsz) + 1;

			replay->base = seq;
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

static inline int
cpt_ipsec_ip_antireplay_check(struct otx2_ipsec_fp_in_sa *sa, void *l3_ptr)
{
	struct otx2_ipsec_fp_res_hdr *hdr = l3_ptr;
	uint64_t seq_in_sa;
	uint32_t seqh = 0;
	uint32_t seql;
	uint64_t seq;
	uint8_t esn;
	int ret;

	esn = sa->ctl.esn_en;
	seql = rte_be_to_cpu_32(hdr->seq_no_lo);

	if (!esn)
		seq = (uint64_t)seql;
	else {
		seqh = rte_be_to_cpu_32(hdr->seq_no_hi);
		seq = ((uint64_t)seqh << 32) | seql;
	}

	if (unlikely(seq == 0))
		return IPSEC_ANTI_REPLAY_FAILED;

	rte_spinlock_lock(&sa->replay->lock);
	ret = anti_replay_check(sa->replay, seq, sa->replay_win_sz);
	if (esn && (ret == 0)) {
		seq_in_sa = ((uint64_t)rte_be_to_cpu_32(sa->esn_hi) << 32) |
				rte_be_to_cpu_32(sa->esn_low);
		if (seq > seq_in_sa) {
			sa->esn_low = rte_cpu_to_be_32(seql);
			sa->esn_hi = rte_cpu_to_be_32(seqh);
		}
	}
	rte_spinlock_unlock(&sa->replay->lock);

	return ret;
}

static inline uint32_t
anti_replay_get_seqh(uint32_t winsz, uint32_t seql,
			uint32_t esn_hi, uint32_t esn_low)
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
#endif /* __OTX2_IPSEC_ANTI_REPLAY_H__ */
