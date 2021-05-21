/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

/*
 * WARNING: It is not recommended to include this file directly.
 * Please include "acl_run_avx512x*.h" instead.
 * To make this file to generate proper code an includer has to
 * define several macros, refer to "acl_run_avx512x*.h" for more details.
 */

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
static __rte_always_inline _T_simd
_F_(calc_addr)(_T_simd index_mask, _T_simd next_input, _T_simd shuffle_input,
	_T_simd four_32, _T_simd range_base, _T_simd tr_lo, _T_simd tr_hi)
{
	__mmask64 qm;
	_T_mask dfa_msk;
	_T_simd addr, in, node_type, r, t;
	_T_simd dfa_ofs, quad_ofs;

	t = _M_SI_(xor)(index_mask, index_mask);
	in = _M_I_(shuffle_epi8)(next_input, shuffle_input);

	/* Calc node type and node addr */
	node_type = _M_SI_(andnot)(index_mask, tr_lo);
	addr = _M_SI_(and)(index_mask, tr_lo);

	/* mask for DFA type(0) nodes */
	dfa_msk = _M_I_(cmpeq_epi32_mask)(node_type, t);

	/* DFA calculations. */
	r = _M_I_(srli_epi32)(in, 30);
	r = _M_I_(add_epi8)(r, range_base);
	t = _M_I_(srli_epi32)(in, 24);
	r = _M_I_(shuffle_epi8)(tr_hi, r);

	dfa_ofs = _M_I_(sub_epi32)(t, r);

	/* QUAD/SINGLE calculations. */
	qm = _M_I_(cmpgt_epi8_mask)(in, tr_hi);
	t = _M_I_(maskz_set1_epi8)(qm, (uint8_t)UINT8_MAX);
	t = _M_I_(lzcnt_epi32)(t);
	t = _M_I_(srli_epi32)(t, 3);
	quad_ofs = _M_I_(sub_epi32)(four_32, t);

	/* blend DFA and QUAD/SINGLE. */
	t = _M_I_(mask_mov_epi32)(quad_ofs, dfa_msk, dfa_ofs);

	/* calculate address for next transitions. */
	addr = _M_I_(add_epi32)(addr, t);
	return addr;
}

/*
 * Process _N_ transitions in parallel.
 * tr_lo contains low 32 bits for _N_ transition.
 * tr_hi contains high 32 bits for _N_ transition.
 * next_input contains up to 4 input bytes for _N_ flows.
 */
static __rte_always_inline _T_simd
_F_(trans)(_T_simd next_input, const uint64_t *trans, _T_simd *tr_lo,
	_T_simd *tr_hi)
{
	const int32_t *tr;
	_T_simd addr;

	tr = (const int32_t *)(uintptr_t)trans;

	/* Calculate the address (array index) for all _N_ transitions. */
	addr = _F_(calc_addr)(_SV_(index_mask), next_input, _SV_(shuffle_input),
		_SV_(four_32), _SV_(range_base), *tr_lo, *tr_hi);

	/* load lower 32 bits of _N_ transactions at once. */
	*tr_lo = _M_GI_(i32gather_epi32, addr, tr, sizeof(trans[0]));

	next_input = _M_I_(srli_epi32)(next_input, CHAR_BIT);

	/* load high 32 bits of _N_ transactions at once. */
	*tr_hi = _M_GI_(i32gather_epi32, addr, (tr + 1), sizeof(trans[0]));

	return next_input;
}

/*
 * Execute first transition for up to _N_ flows in parallel.
 * next_input should contain one input byte for up to _N_ flows.
 * msk - mask of active flows.
 * tr_lo contains low 32 bits for up to _N_ transitions.
 * tr_hi contains high 32 bits for up to _N_ transitions.
 */
static __rte_always_inline void
_F_(first_trans)(const struct acl_flow_avx512 *flow, _T_simd next_input,
	_T_mask msk, _T_simd *tr_lo, _T_simd *tr_hi)
{
	const int32_t *tr;
	_T_simd addr, root;

	tr = (const int32_t *)(uintptr_t)flow->trans;

	addr = _M_I_(set1_epi32)(UINT8_MAX);
	root = _M_I_(set1_epi32)(flow->root_index);

	addr = _M_SI_(and)(next_input, addr);
	addr = _M_I_(add_epi32)(root, addr);

	/* load lower 32 bits of _N_ transactions at once. */
	*tr_lo = _M_MGI_(mask_i32gather_epi32)(*tr_lo, msk, addr, tr,
		sizeof(flow->trans[0]));

	/* load high 32 bits of _N_ transactions at once. */
	*tr_hi = _M_MGI_(mask_i32gather_epi32)(*tr_hi, msk, addr, (tr + 1),
		sizeof(flow->trans[0]));
}

/*
 * Load and return next 4 input bytes for up to _N_ flows in parallel.
 * pdata - 8x2 pointers to flow input data
 * mask - mask of active flows.
 * di - data indexes for these _N_ flows.
 */
static inline _T_simd
_F_(get_next_bytes)(const struct acl_flow_avx512 *flow, _T_simd pdata[2],
	uint32_t msk, _T_simd *di, uint32_t bnum)
{
	const int32_t *div;
	uint32_t m[2];
	_T_simd one, zero, t, p[2];

	div = (const int32_t *)flow->data_index;

	one = _M_I_(set1_epi32)(1);
	zero = _M_SI_(xor)(one, one);

	/* load data offsets for given indexes */
	t = _M_MGI_(mask_i32gather_epi32)(zero, msk, *di, div, sizeof(div[0]));

	/* increment data indexes */
	*di = _M_I_(mask_add_epi32)(*di, msk, *di, one);

	/*
	 * unsigned expand 32-bit indexes to 64-bit
	 * (for later pointer arithmetic), i.e:
	 * for (i = 0; i != _N_; i++)
	 *   p[i/8].u64[i%8] = (uint64_t)t.u32[i];
	 */
	p[0] = _M_I_(maskz_permutexvar_epi32)(_SC_(pmidx_msk), _SV_(pmidx[0]),
			t);
	p[1] = _M_I_(maskz_permutexvar_epi32)(_SC_(pmidx_msk), _SV_(pmidx[1]),
			t);

	p[0] = _M_I_(add_epi64)(p[0], pdata[0]);
	p[1] = _M_I_(add_epi64)(p[1], pdata[1]);

	/* load input byte(s), either one or four */

	m[0] = msk & _SIMD_PTR_MSK_;
	m[1] = msk >> _SIMD_PTR_NUM_;

	return _F_(gather_bytes)(zero, p, m, bnum);
}

/*
 * Start up to _N_ new flows.
 * num - number of flows to start
 * msk - mask of new flows.
 * pdata - pointers to flow input data
 * idx - match indexed for given flows
 * di - data indexes for these flows.
 */
static inline void
_F_(start_flow)(struct acl_flow_avx512 *flow, uint32_t num, uint32_t msk,
	_T_simd pdata[2], _T_simd *idx, _T_simd *di)
{
	uint32_t n, m[2], nm[2];
	_T_simd ni, nd[2];

	/* split mask into two - one for each pdata[] */
	m[0] = msk & _SIMD_PTR_MSK_;
	m[1] = msk >> _SIMD_PTR_NUM_;

	/* calculate masks for new flows */
	n = __builtin_popcount(m[0]);
	nm[0] = (1 << n) - 1;
	nm[1] = (1 << (num - n)) - 1;

	/* load input data pointers for new flows */
	nd[0] = _M_I_(maskz_loadu_epi64)(nm[0],
			flow->idata + flow->num_packets);
	nd[1] = _M_I_(maskz_loadu_epi64)(nm[1],
			flow->idata + flow->num_packets + n);

	/* calculate match indexes of new flows */
	ni = _M_I_(set1_epi32)(flow->num_packets);
	ni = _M_I_(add_epi32)(ni, _SV_(idx_add));

	/* merge new and existing flows data */
	pdata[0] = _M_I_(mask_expand_epi64)(pdata[0], m[0], nd[0]);
	pdata[1] = _M_I_(mask_expand_epi64)(pdata[1], m[1], nd[1]);

	/* update match and data indexes */
	*idx = _M_I_(mask_expand_epi32)(*idx, msk, ni);
	*di = _M_I_(maskz_mov_epi32)(msk ^ _SIMD_MASK_MAX_, *di);

	flow->num_packets += num;
}

/*
 * Process found matches for up to _N_ flows.
 * fmsk - mask of active flows
 * rmsk - mask of found matches
 * pdata - pointers to flow input data
 * di - data indexes for these flows
 * idx - match indexed for given flows
 * tr_lo contains low 32 bits for up to _N_ transitions.
 * tr_hi contains high 32 bits for up to _N_ transitions.
 */
static inline uint32_t
_F_(match_process)(struct acl_flow_avx512 *flow, uint32_t *fmsk,
	uint32_t *rmsk, _T_simd pdata[2], _T_simd *di, _T_simd *idx,
	_T_simd *tr_lo, _T_simd *tr_hi)
{
	uint32_t n;
	_T_simd res;

	if (rmsk[0] == 0)
		return 0;

	/* extract match indexes */
	res = _M_SI_(and)(tr_lo[0], _SV_(index_mask));

	/* mask  matched transitions to nop */
	tr_lo[0] = _M_I_(mask_mov_epi32)(tr_lo[0], rmsk[0], _SV_(trlo_idle));
	tr_hi[0] = _M_I_(mask_mov_epi32)(tr_hi[0], rmsk[0], _SV_(trhi_idle));

	/* save found match indexes */
	_M_I_(mask_i32scatter_epi32)((void *)flow->matches, rmsk[0], idx[0],
			res, sizeof(flow->matches[0]));

	/* update masks and start new flows for matches */
	n = update_flow_mask(flow, fmsk, rmsk);
	_F_(start_flow)(flow, n, rmsk[0], pdata, idx, di);

	return n;
}

/*
 * Test for matches ut to (2 * _N_) flows at once,
 * if matches exist - process them and start new flows.
 */
static inline void
_F_(match_check_process)(struct acl_flow_avx512 *flow, uint32_t fm[2],
	_T_simd pdata[4], _T_simd di[2], _T_simd idx[2], _T_simd inp[2],
	_T_simd tr_lo[2], _T_simd tr_hi[2])
{
	uint32_t n[2];
	uint32_t rm[2];

	/* check for matches */
	rm[0] = _M_I_(test_epi32_mask)(tr_lo[0], _SV_(match_mask));
	rm[1] = _M_I_(test_epi32_mask)(tr_lo[1], _SV_(match_mask));

	/* till unprocessed matches exist */
	while ((rm[0] | rm[1]) != 0) {

		/* process matches and start new flows */
		n[0] = _F_(match_process)(flow, &fm[0], &rm[0], &pdata[0],
			&di[0], &idx[0], &tr_lo[0], &tr_hi[0]);
		n[1] = _F_(match_process)(flow, &fm[1], &rm[1], &pdata[2],
			&di[1], &idx[1], &tr_lo[1], &tr_hi[1]);

		/* execute first transition for new flows, if any */

		if (n[0] != 0) {
			inp[0] = _F_(get_next_bytes)(flow, &pdata[0],
					rm[0], &di[0], flow->first_load_sz);
			_F_(first_trans)(flow, inp[0], rm[0], &tr_lo[0],
					&tr_hi[0]);
			rm[0] = _M_I_(test_epi32_mask)(tr_lo[0],
					_SV_(match_mask));
		}

		if (n[1] != 0) {
			inp[1] = _F_(get_next_bytes)(flow, &pdata[2],
					rm[1], &di[1], flow->first_load_sz);
			_F_(first_trans)(flow, inp[1], rm[1], &tr_lo[1],
					&tr_hi[1]);
			rm[1] = _M_I_(test_epi32_mask)(tr_lo[1],
					_SV_(match_mask));
		}
	}
}

static inline void
_F_(reset_flow_vars)(_T_simd di[2], _T_simd idx[2], _T_simd pdata[4],
	_T_simd tr_lo[2], _T_simd tr_hi[2])
{
	di[0] = _M_SI_(setzero)();
	di[1] = _M_SI_(setzero)();

	idx[0] = _M_SI_(setzero)();
	idx[1] = _M_SI_(setzero)();

	pdata[0] = _M_SI_(setzero)();
	pdata[1] = _M_SI_(setzero)();
	pdata[2] = _M_SI_(setzero)();
	pdata[3] = _M_SI_(setzero)();

	tr_lo[0] = _M_SI_(setzero)();
	tr_lo[1] = _M_SI_(setzero)();

	tr_hi[0] = _M_SI_(setzero)();
	tr_hi[1] = _M_SI_(setzero)();
}

/*
 * Perform search for up to (2 * _N_) flows in parallel.
 * Use two sets of metadata, each serves _N_ flows max.
 */
static inline void
_F_(search_trie)(struct acl_flow_avx512 *flow)
{
	uint32_t fm[2];
	_T_simd di[2], idx[2], in[2], pdata[4], tr_lo[2], tr_hi[2];

	_F_(reset_flow_vars)(di, idx, pdata, tr_lo, tr_hi);

	/* first 1B load */
	_F_(start_flow)(flow, _SIMD_MASK_BIT_, _SIMD_MASK_MAX_,
			&pdata[0], &idx[0], &di[0]);
	_F_(start_flow)(flow, _SIMD_MASK_BIT_, _SIMD_MASK_MAX_,
			&pdata[2], &idx[1], &di[1]);

	in[0] = _F_(get_next_bytes)(flow, &pdata[0], _SIMD_MASK_MAX_, &di[0],
			flow->first_load_sz);
	in[1] = _F_(get_next_bytes)(flow, &pdata[2], _SIMD_MASK_MAX_, &di[1],
			flow->first_load_sz);

	_F_(first_trans)(flow, in[0], _SIMD_MASK_MAX_, &tr_lo[0], &tr_hi[0]);
	_F_(first_trans)(flow, in[1], _SIMD_MASK_MAX_, &tr_lo[1], &tr_hi[1]);

	fm[0] = _SIMD_MASK_MAX_;
	fm[1] = _SIMD_MASK_MAX_;

	/* match check */
	_F_(match_check_process)(flow, fm, pdata, di, idx, in, tr_lo, tr_hi);

	while ((fm[0] | fm[1]) != 0) {

		/* load next 4B */

		in[0] = _F_(get_next_bytes)(flow, &pdata[0], fm[0],
				&di[0], sizeof(uint32_t));
		in[1] = _F_(get_next_bytes)(flow, &pdata[2], fm[1],
				&di[1], sizeof(uint32_t));

		/* main 4B loop */

		in[0] = _F_(trans)(in[0], flow->trans, &tr_lo[0], &tr_hi[0]);
		in[1] = _F_(trans)(in[1], flow->trans, &tr_lo[1], &tr_hi[1]);

		in[0] = _F_(trans)(in[0], flow->trans, &tr_lo[0], &tr_hi[0]);
		in[1] = _F_(trans)(in[1], flow->trans, &tr_lo[1], &tr_hi[1]);

		in[0] = _F_(trans)(in[0], flow->trans, &tr_lo[0], &tr_hi[0]);
		in[1] = _F_(trans)(in[1], flow->trans, &tr_lo[1], &tr_hi[1]);

		in[0] = _F_(trans)(in[0], flow->trans, &tr_lo[0], &tr_hi[0]);
		in[1] = _F_(trans)(in[1], flow->trans, &tr_lo[1], &tr_hi[1]);

		/* check for matches */
		_F_(match_check_process)(flow, fm, pdata, di, idx, in,
			tr_lo, tr_hi);
	}
}

/*
 * resolve match index to actual result/priority offset.
 */
static inline _T_simd
_F_(resolve_match_idx)(_T_simd mi)
{
	RTE_BUILD_BUG_ON(sizeof(struct rte_acl_match_results) !=
		1 << (ACL_MATCH_LOG + 2));
	return _M_I_(slli_epi32)(mi, ACL_MATCH_LOG);
}

/*
 * Resolve multiple matches for the same flow based on priority.
 */
static inline _T_simd
_F_(resolve_pri)(const int32_t res[], const int32_t pri[],
	const uint32_t match[], _T_mask msk, uint32_t nb_trie,
	uint32_t nb_skip)
{
	uint32_t i;
	const uint32_t *pm;
	_T_mask m;
	_T_simd cp, cr, np, nr, mch;

	const _T_simd zero = _M_I_(set1_epi32)(0);

	/* get match indexes */
	mch = _M_I_(maskz_loadu_epi32)(msk, match);
	mch = _F_(resolve_match_idx)(mch);

	/* read result and priority values for first trie */
	cr = _M_MGI_(mask_i32gather_epi32)(zero, msk, mch, res, sizeof(res[0]));
	cp = _M_MGI_(mask_i32gather_epi32)(zero, msk, mch, pri, sizeof(pri[0]));

	/*
	 * read result and priority values for next tries and select one
	 * with highest priority.
	 */
	for (i = 1, pm = match + nb_skip; i != nb_trie;
			i++, pm += nb_skip) {

		mch = _M_I_(maskz_loadu_epi32)(msk, pm);
		mch = _F_(resolve_match_idx)(mch);

		nr = _M_MGI_(mask_i32gather_epi32)(zero, msk, mch, res,
				sizeof(res[0]));
		np = _M_MGI_(mask_i32gather_epi32)(zero, msk, mch, pri,
				sizeof(pri[0]));

		m = _M_I_(cmpgt_epi32_mask)(cp, np);
		cr = _M_I_(mask_mov_epi32)(nr, m, cr);
		cp = _M_I_(mask_mov_epi32)(np, m, cp);
	}

	return cr;
}

/*
 * Resolve num (<= _N_) matches for single category
 */
static inline void
_F_(resolve_sc)(uint32_t result[], const int32_t res[],
	const int32_t pri[], const uint32_t match[], uint32_t nb_pkt,
	uint32_t nb_trie, uint32_t nb_skip)
{
	_T_mask msk;
	_T_simd cr;

	msk = (1 << nb_pkt) - 1;
	cr = _F_(resolve_pri)(res, pri, match, msk, nb_trie, nb_skip);
	_M_I_(mask_storeu_epi32)(result, msk, cr);
}

/*
 * Resolve matches for single category
 */
static inline void
_F_(resolve_single_cat)(uint32_t result[],
	const struct rte_acl_match_results pr[], const uint32_t match[],
	uint32_t nb_pkt, uint32_t nb_trie)
{
	uint32_t j, k, n;
	const int32_t *res, *pri;
	_T_simd cr[2];

	res = (const int32_t *)pr->results;
	pri = pr->priority;

	for (k = 0; k != (nb_pkt & ~_SIMD_FLOW_MSK_); k += _SIMD_FLOW_NUM_) {

		j = k + _SIMD_MASK_BIT_;

		cr[0] = _F_(resolve_pri)(res, pri, match + k, _SIMD_MASK_MAX_,
				nb_trie, nb_pkt);
		cr[1] = _F_(resolve_pri)(res, pri, match + j, _SIMD_MASK_MAX_,
				nb_trie, nb_pkt);

		_M_SI_(storeu)((void *)(result + k), cr[0]);
		_M_SI_(storeu)((void *)(result + j), cr[1]);
	}

	n = nb_pkt - k;
	if (n != 0) {
		if (n > _SIMD_MASK_BIT_) {
			_F_(resolve_sc)(result + k, res, pri, match + k,
				_SIMD_MASK_BIT_, nb_trie, nb_pkt);
			k += _SIMD_MASK_BIT_;
			n -= _SIMD_MASK_BIT_;
		}
		_F_(resolve_sc)(result + k, res, pri, match + k, n,
				nb_trie, nb_pkt);
	}
}
