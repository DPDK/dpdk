/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _SCHED_H_
#define _SCHED_H_

/**
 * This file is added to support the common code in eal_common_thread.c
 * as Microsoft libc does not contain sched.h. This may be removed
 * in future releases.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef CPU_SETSIZE
#define CPU_SETSIZE RTE_MAX_LCORE
#endif

#define _BITS_PER_SET (sizeof(long long) * 8)
#define _BIT_SET_MASK (_BITS_PER_SET - 1)

#define _NUM_SETS(b) (((b) + _BIT_SET_MASK) / _BITS_PER_SET)
#define _WHICH_SET(b) ((b) / _BITS_PER_SET)
#define _WHICH_BIT(b) ((b) & (_BITS_PER_SET - 1))

typedef struct _rte_cpuset_s {
	long long _bits[_NUM_SETS(CPU_SETSIZE)];
} rte_cpuset_t;
#define RTE_HAS_CPUSET

#define CPU_SET(b, s) ((s)->_bits[_WHICH_SET(b)] |= (1LL << _WHICH_BIT(b)))

#define CPU_ZERO(s)							\
	do {								\
		unsigned int _i;					\
									\
		for (_i = 0; _i < _NUM_SETS(CPU_SETSIZE); _i++)		\
			(s)->_bits[_i] = 0LL;				\
	} while (0)

#define CPU_ISSET(b, s) (((s)->_bits[_WHICH_SET(b)] & \
	(1LL << _WHICH_BIT(b))) != 0LL)

static inline int
count_cpu(rte_cpuset_t *s)
{
	unsigned int _i;
	int count = 0;

	for (_i = 0; _i < CPU_SETSIZE; _i++)
		if (CPU_ISSET(_i, s) != 0LL)
			count++;
	return count;
}
#define CPU_COUNT(s) count_cpu(s)

#define CPU_AND(dst, src1, src2) \
do { \
	unsigned int _i; \
	\
	for (_i = 0; _i < _NUM_SETS(CPU_SETSIZE); _i++) \
		(dst)->_bits[_i] = (src1)->_bits[_i] & (src2)->_bits[_i]; \
} while (0)

#define CPU_OR(dst, src1, src2) \
do { \
	unsigned int _i; \
	\
	for (_i = 0; _i < _NUM_SETS(CPU_SETSIZE); _i++) \
		(dst)->_bits[_i] = (src1)->_bits[_i] | (src2)->_bits[_i]; \
} while (0)

#define CPU_FILL(s) \
do { \
	unsigned int _i; \
	for (_i = 0; _i < _NUM_SETS(CPU_SETSIZE); _i++) \
		(s)->_bits[_i] = -1LL; \
} while (0)

#define CPU_NOT(dst, src) \
do { \
	unsigned int _i; \
	for (_i = 0; _i < _NUM_SETS(CPU_SETSIZE); _i++) \
		(dst)->_bits[_i] = (src)->_bits[_i] ^ -1LL; \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* _SCHED_H_ */
