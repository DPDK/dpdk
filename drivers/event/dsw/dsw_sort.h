/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#ifndef _DSW_SORT_
#define _DSW_SORT_

#include <string.h>

#include <rte_common.h>

#define DSW_ARY_ELEM_PTR(_ary, _idx, _elem_size)	\
	RTE_PTR_ADD(_ary, (_idx) * (_elem_size))

#define DSW_ARY_ELEM_SWAP(_ary, _a_idx, _b_idx, _elem_size)		\
	do {								\
		char tmp[_elem_size];					\
		void *_a_ptr = DSW_ARY_ELEM_PTR(_ary, _a_idx, _elem_size); \
		void *_b_ptr = DSW_ARY_ELEM_PTR(_ary, _b_idx, _elem_size); \
		memcpy(tmp, _a_ptr, _elem_size);			\
		memcpy(_a_ptr, _b_ptr, _elem_size);			\
		memcpy(_b_ptr, tmp, _elem_size);			\
	} while (0)

static inline void
dsw_insertion_sort(void *ary, uint16_t len, uint16_t elem_size,
		   int (*cmp_fn)(const void *, const void *))
{
	uint16_t i;

	for (i = 1; i < len; i++) {
		uint16_t j;
		for (j = i; j > 0 &&
			     cmp_fn(DSW_ARY_ELEM_PTR(ary, j-1, elem_size),
				    DSW_ARY_ELEM_PTR(ary, j, elem_size)) > 0;
		     j--)
			DSW_ARY_ELEM_SWAP(ary, j, j-1, elem_size);
	}
}

static inline void
dsw_stable_sort(void *ary, uint16_t len, uint16_t elem_size,
		int (*cmp_fn)(const void *, const void *))
{
	dsw_insertion_sort(ary, len, elem_size, cmp_fn);
}

#endif
