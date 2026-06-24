/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#ifndef _BPF_VALUE_SET_H_
#define _BPF_VALUE_SET_H_

/**
 * @file value_set.h
 *
 * Value set operations for BPF validate debug.
 *
 * This is not a general use library, only minimal set of operations is provided
 * that are necessary for implementing validate debug interface.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VALUE_SET_NB_INTERVAL_MAX 3

/*
 * Cyclic interval on uint64_t.
 *
 * Cyclic means value of `last` might be numerically smaller than `first`,
 * that is the interval may cross from UINT64_MAX to 0.
 *
 * Contains element `first` and all elements that can be obtained from it by
 * adding 1 until the result reaches `last`, which is included.
 * There is thus multiple representations of the full set and no representation
 * of the empty set.
 *
 * When `first` and `last` are accepted separately as function arguments, the
 * term _contiguous_ is being used. It means that values of `first` and `last`
 * are used to create a contiguous set composed of a single cyclic interval
 * defined by these points.
 */
struct value_set_interval {
	uint64_t first;
	uint64_t last;
};

/*
 * Set of values represented as an ordered sequence of maximal disjoint cyclic intervals.
 *
 * Condition `maximal disjoint` means intervals do not intersect or touch each other.
 *
 * The sequence is ordered by member `first`. Only last interval may thus cross zero.
 */
struct value_set {
	uint32_t nb_interval;
	struct value_set_interval interval[VALUE_SET_NB_INTERVAL_MAX];
};

/* Empty value set. */
static const struct value_set value_set_empty = {
	.nb_interval = 0,
};

/* Full (including every possible value) value set. */
static const struct value_set value_set_full = {
	.nb_interval = 1,
	.interval = {
		{ .first = 0, .last = UINT64_MAX },
	},
};

/* Return set containing only `value`. */
struct value_set
value_set_singleton(uint64_t value);

/* Return set of all values between and including `first` and `last` (AKA first..last). */
struct value_set
value_set_contiguous(uint64_t first, uint64_t last);

/* Return set of all values belonging to _both_ first1..last1 and first2..last. */
struct value_set
value_set_from_pair(uint64_t first1, uint64_t last1, uint64_t first2, uint64_t last2);

/* Return true if the set is empty. */
bool
value_set_is_empty(const struct value_set *set);

/* Return true if the set only contains one element. */
bool
value_set_is_singleton(const struct value_set *set);

/* Return true if lhs and rhs represent the same set. */
bool
value_sets_equal(const struct value_set *lhs, const struct value_set *rhs);

/* Return true if sets intersect (contain common elements). */
bool
value_sets_intersect(const struct value_set *lhs, const struct value_set *rhs);

/* Return true if all elements in lhs belong to interval first..last */
bool
value_set_is_covered_by_contiguous(const struct value_set *lhs, uint64_t first, uint64_t last);

/* Return true if `(l - o) < (r - o)` for all `(o in origin, l in lhs, r in rhs)`. */
bool
value_sets_based_less(const struct value_set *origin, const struct value_set *lhs,
	const struct value_set *rhs);

/* Return true if `(l - o) <= (r - o)` for all `(o in origin, l in lhs, r in rhs)`. */
bool
value_sets_based_less_or_equal(const struct value_set *origin, const struct value_set *lhs,
	const struct value_set *rhs);

/* Translate ("shift") all set elements by `offset`. */
void
value_set_translate(struct value_set *lhs, uint64_t rhs);

/* Set `lhs` to the set of possible sums between values from `lhs` and `rhs`. */
void
value_set_add_contiguous(struct value_set *lhs, uint64_t first, uint64_t last);

#ifdef __cplusplus
}
#endif

#endif /* _BPF_VALUE_SET_H */
