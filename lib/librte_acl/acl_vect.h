/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ACL_VECT_H_
#define _RTE_ACL_VECT_H_

/**
 * @file
 *
 * RTE ACL SSE/AVX related header.
 */

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Takes 2 SIMD registers containing N transitions eachi (tr0, tr1).
 * Shuffles it into different representation:
 * lo - contains low 32 bits of given N transitions.
 * hi - contains high 32 bits of given N transitions.
 */
#define	ACL_TR_HILO(P, TC, tr0, tr1, lo, hi)                        do { \
	lo = (typeof(lo))_##P##_shuffle_ps((TC)(tr0), (TC)(tr1), 0x88);  \
	hi = (typeof(hi))_##P##_shuffle_ps((TC)(tr0), (TC)(tr1), 0xdd);  \
} while (0)


/*
 * Calculate the address of the next transition for
 * all types of nodes. Note that only DFA nodes and range
 * nodes actually transition to another node. Match
 * nodes not supposed to be encountered here.
 * For quad range nodes:
 * Calculate number of range boundaries that are less than the
 * input value. Range boundaries for each node are in signed 8 bit,
 * ordered from -128 to 127.
 * This is effectively a popcnt of bytes that are greater than the
 * input byte.
 * Single nodes are processed in the same ways as quad range nodes.
*/
#define ACL_TR_CALC_ADDR(P, S,					\
	addr, index_mask, next_input, shuffle_input,		\
	ones_16, range_base, tr_lo, tr_hi)               do {	\
								\
	typeof(addr) in, node_type, r, t;			\
	typeof(addr) dfa_msk, dfa_ofs, quad_ofs;		\
								\
	t = _##P##_xor_si##S(index_mask, index_mask);		\
	in = _##P##_shuffle_epi8(next_input, shuffle_input);	\
								\
	/* Calc node type and node addr */			\
	node_type = _##P##_andnot_si##S(index_mask, tr_lo);	\
	addr = _##P##_and_si##S(index_mask, tr_lo);		\
								\
	/* mask for DFA type(0) nodes */			\
	dfa_msk = _##P##_cmpeq_epi32(node_type, t);		\
								\
	/* DFA calculations. */					\
	r = _##P##_srli_epi32(in, 30);				\
	r = _##P##_add_epi8(r, range_base);			\
	t = _##P##_srli_epi32(in, 24);				\
	r = _##P##_shuffle_epi8(tr_hi, r);			\
								\
	dfa_ofs = _##P##_sub_epi32(t, r);			\
								\
	/* QUAD/SINGLE caluclations. */				\
	t = _##P##_cmpgt_epi8(in, tr_hi);			\
	t = _##P##_sign_epi8(t, t);				\
	t = _##P##_maddubs_epi16(t, t);				\
	quad_ofs = _##P##_madd_epi16(t, ones_16);		\
								\
	/* blend DFA and QUAD/SINGLE. */			\
	t = _##P##_blendv_epi8(quad_ofs, dfa_ofs, dfa_msk);	\
								\
	/* calculate address for next transitions. */		\
	addr = _##P##_add_epi32(addr, t);			\
} while (0)


#ifdef __cplusplus
}
#endif

#endif /* _RTE_ACL_VECT_H_ */
