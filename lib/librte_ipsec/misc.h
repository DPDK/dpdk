/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _MISC_H_
#define _MISC_H_

/**
 * @file misc.h
 * Contains miscellaneous functions/structures/macros used internally
 * by ipsec library.
 */

/*
 * Move bad (unprocessed) mbufs beyond the good (processed) ones.
 * bad_idx[] contains the indexes of bad mbufs inside the mb[].
 */
static inline void
move_bad_mbufs(struct rte_mbuf *mb[], const uint32_t bad_idx[], uint32_t nb_mb,
	uint32_t nb_bad)
{
	uint32_t i, j, k;
	struct rte_mbuf *drb[nb_bad];

	j = 0;
	k = 0;

	/* copy bad ones into a temp place */
	for (i = 0; i != nb_mb; i++) {
		if (j != nb_bad && i == bad_idx[j])
			drb[j++] = mb[i];
		else
			mb[k++] = mb[i];
	}

	/* copy bad ones after the good ones */
	for (i = 0; i != nb_bad; i++)
		mb[k + i] = drb[i];
}

#endif /* _MISC_H_ */
