/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 */

#include "bpf_value_set.h"

#include <rte_debug.h>

/* Helper interval operations and checks.  */

/* One of many possible full intervals. */
static const struct value_set_interval canonical_full_interval = {
	.first = 0,
	.last = UINT64_MAX,
};

/* Translate ("shift") interval by `offset`. */
static void
interval_translate(struct value_set_interval *interval, uint64_t offset)
{
	interval->first += offset;
	interval->last += offset;
}

/* Return true if the interval includes all possible values. */
static bool
interval_is_full(struct value_set_interval interval)
{
	return interval.last + 1 == interval.first;
}

/* Return true if the interval includes `value`. */
static bool
interval_contains(struct value_set_interval interval, uint64_t value)
{
	return value - interval.first <= interval.last - interval.first;
}

/* Return true if the interval `lhs` includes all values from `rhs`. */
static bool
interval_covers(struct value_set_interval lhs, struct value_set_interval rhs)
{
	const uint64_t offset = -lhs.first;
	interval_translate(&lhs, offset);
	interval_translate(&rhs, offset);
	RTE_ASSERT(lhs.first == 0);

	return lhs.last == UINT64_MAX ||
		(lhs.last >= rhs.last && rhs.last >= rhs.first);
}

/* Return true if the interval includes step from UINT64_MAX to 0. */
static bool
interval_crosses_zero(struct value_set_interval interval)
{
	return interval.last < interval.first;
}

/* Return number of elements in a non-full elements, 0 for full interval. */
static uint64_t
interval_size(struct value_set_interval interval)
{
	return interval.last - interval.first + 1;
}

/* Return true if two intervals represent same sets of values. */
static bool
intervals_equal(struct value_set_interval lhs, struct value_set_interval rhs)
{
	return (interval_is_full(lhs) && interval_is_full(rhs)) ||
		(lhs.first == rhs.first && lhs.last == rhs.last);
}

/* Return true if two intervals have common elements. */
static bool
intervals_intersect(struct value_set_interval lhs, struct value_set_interval rhs)
{
	return interval_contains(lhs, rhs.first) || interval_contains(rhs, lhs.first);
}

/* Return true if `rhs.first` follows `lhs.last` with some gap. Does not check other ends! */
static bool
intervals_follow_with_gap(struct value_set_interval lhs, struct value_set_interval rhs)
{
	return lhs.last != UINT64_MAX && rhs.first > lhs.last + 1;
}

/* Return true if `(l - o) < (r - o)` for all `(o in origin, l in lhs, r in rhs)`. */
static bool
intervals_based_less(struct value_set_interval origin, struct value_set_interval lhs,
	struct value_set_interval rhs)
{
	/* Translate all intervals for the origin to start at 0. */
	const uint64_t offset = -origin.first;
	interval_translate(&origin, offset);
	interval_translate(&lhs, offset);
	interval_translate(&rhs, offset);
	RTE_ASSERT(origin.first == 0);

	return origin.last <= lhs.first &&
		lhs.first <= lhs.last &&
		lhs.last < rhs.first &&
		rhs.first <= rhs.last;
}

/* Return true if `(l - o) <= (r - o)` for all `(o in origin, l in lhs, r in rhs)`. */
static bool
intervals_based_less_or_equal(struct value_set_interval origin, struct value_set_interval lhs,
	struct value_set_interval rhs)
{
	/* Translate all intervals for the origin to start at 0. */
	const uint64_t offset = -origin.first;
	interval_translate(&origin, offset);
	interval_translate(&lhs, offset);
	interval_translate(&rhs, offset);
	RTE_ASSERT(origin.first == 0);

	/* Special cases. */
	if (origin.last == 0 && lhs.first == 0 && lhs.last == 0)
		return true;
	if (origin.last == 0 && rhs.first == UINT64_MAX && rhs.last == UINT64_MAX)
		return true;
	if (lhs.first == lhs.last && lhs.last == rhs.first && rhs.first == rhs.last)
		return true;

	return origin.last <= lhs.first &&
		lhs.first <= lhs.last &&
		lhs.last <= rhs.first &&
		rhs.first <= rhs.last;
}

/* Append interval rhs to list of intervals in lhs. */
static void
value_set_append(struct value_set *lhs, struct value_set_interval rhs)
{
	RTE_VERIFY(lhs->nb_interval < VALUE_SET_NB_INTERVAL_MAX);
	RTE_VERIFY(lhs->nb_interval == 0 ||
		intervals_follow_with_gap(lhs->interval[lhs->nb_interval - 1], rhs));
	lhs->interval[lhs->nb_interval++] = rhs;
}

/*
 * Helper operations on noncyclic value set and intervals.
 * Noncyclic means no interval crosses zero,
 * but in return last value set interval may touch first.
 */

static struct value_set
noncyclic_value_set_union_interval(const struct value_set *lhs, const struct value_set_interval rhs)
{
	struct value_set result = {};
	uint32_t index = 0;

	RTE_ASSERT(lhs->nb_interval == 0 ||
		!interval_crosses_zero(lhs->interval[lhs->nb_interval - 1]));
	RTE_ASSERT(!interval_crosses_zero(rhs));

	/* Append to result all lhs intervals preceding rhs. */
	for (; index != lhs->nb_interval; ++index) {
		const struct value_set_interval lhs_interval = lhs->interval[index];
		if (!intervals_follow_with_gap(lhs_interval, rhs))
			break;

		value_set_append(&result, lhs_interval);
	}

	/* Appendinterval joined from rhs and all lhs intervals intersecting or touching it. */
	struct value_set_interval joint_interval = rhs;
	for (; index != lhs->nb_interval; ++index) {
		const struct value_set_interval lhs_interval = lhs->interval[index];
		if (intervals_follow_with_gap(rhs, lhs_interval))
			break;

		joint_interval.first = RTE_MIN(joint_interval.first, lhs_interval.first);
		joint_interval.last = RTE_MAX(joint_interval.last, lhs_interval.last);
	}
	value_set_append(&result, joint_interval);

	/* Append to result all lhs intervals following rhs. */
	for (; index != lhs->nb_interval; ++index)
		value_set_append(&result, lhs->interval[index]);

	return result;
}

/* Make "normal" maximal disjoint interval value set out of noncyclic one. */
static struct value_set
value_set_from_noncyclic(const struct value_set *set)
{
	struct value_set result = {};
	uint32_t index = 0;

	if (set->nb_interval <= 1)
		return *set;

	struct value_set_interval last_interval = set->interval[set->nb_interval - 1];
	if (last_interval.last == UINT64_MAX && set->interval[0].first == 0) {
		/* Join first interval with the last one instead of copying it. */
		last_interval.last = set->interval[0].last;
		++index;
	}

	for (; index != set->nb_interval - 1; ++index)
		value_set_append(&result, set->interval[index]);

	value_set_append(&result, last_interval);

	return result;
}

/* Make lhs a union of lhs and rhs. */
static void
value_set_union_interval(struct value_set *lhs, const struct value_set_interval rhs)
{
	struct value_set temp;

	if (value_set_is_empty(lhs)) {
		value_set_append(lhs, rhs);
		return;
	}

	struct value_set_interval *const last_interval = &lhs->interval[lhs->nb_interval - 1];
	const bool last_interval_crossed_zero = interval_crosses_zero(*last_interval);
	const uint64_t wrapping_last = last_interval->last;

	if (last_interval_crossed_zero)
		/* Make value set noncyclic by removing crossing part of last interval. */
		last_interval->last = UINT64_MAX;

	if (interval_crosses_zero(rhs)) {
		/* Add parts before and after zero separately. */
		temp = noncyclic_value_set_union_interval(lhs,
			(struct value_set_interval){
				.first = rhs.first,
				.last = UINT64_MAX,
			});
		temp = noncyclic_value_set_union_interval(lhs,
			(struct value_set_interval){
				.first = 0,
				.last = rhs.last,
			});
	} else
		temp = noncyclic_value_set_union_interval(lhs, rhs);

	if (last_interval_crossed_zero)
		/* Restore previously removed part. */
		temp = noncyclic_value_set_union_interval(&temp,
			(struct value_set_interval){
				.first = 0,
				.last = wrapping_last,
			});

	*lhs = value_set_from_noncyclic(&temp);
}

/* Set `lhs` to the set of possible sums between values from `lhs` and `rhs`. */
static void
value_set_add_interval(struct value_set *lhs, struct value_set_interval rhs)
{
	const struct value_set temp = *lhs;
	lhs->nb_interval = 0;

	for (uint32_t index = 0; index != temp.nb_interval; ++index) {
		const struct value_set_interval interval = temp.interval[index];
		if (interval_is_full(rhs) || interval_is_full(interval) ||
				interval_size(interval) > UINT64_MAX - interval_size(rhs)) {
			value_set_append(lhs, canonical_full_interval);
			return;
		}
	}

	for (uint32_t index = 0; index != temp.nb_interval; ++index)
		value_set_union_interval(lhs, (struct value_set_interval){
			/* Checked sizes above, so these interval expansions won't overflow. */
			.first = temp.interval[index].first + rhs.first,
			.last = temp.interval[index].last + rhs.last,
		});
}

struct value_set
value_set_singleton(uint64_t value)
{
	return value_set_contiguous(value, value);
}

struct value_set
value_set_contiguous(uint64_t first, uint64_t last)
{
	return (struct value_set){
		.nb_interval = 1,
		.interval = {
			{ .first = first, .last = last },
		},
	};
}

struct value_set
value_set_from_pair(uint64_t first1, uint64_t last1, uint64_t first2, uint64_t last2)
{
	struct value_set result = {};

	if (first1 - first2 <= last2 - first2)
		/* Interval 1 starts within interval 2. */
		value_set_union_interval(&result, (struct value_set_interval){
				.first = first1,
				.last = first1 + RTE_MIN(last1 - first1, last2 - first1),
			});

	if (first2 - first1 <= last1 - first1)
		/* Interval 2 starts within interval 1. */
		value_set_union_interval(&result, (struct value_set_interval){
				.first = first2,
				.last = first2 + RTE_MIN(last2 - first2, last1 - first2),
			});

	return result;
}

bool
value_set_is_empty(const struct value_set *set)
{
	return set->nb_interval == 0;
}

bool
value_set_is_singleton(const struct value_set *set)
{
	return set->nb_interval == 1 && interval_size(set->interval[0]) == 1;
}

bool
value_sets_equal(const struct value_set *lhs, const struct value_set *rhs)
{
	if (lhs->nb_interval != rhs->nb_interval)
		return false;

	for (uint32_t index = 0; index != lhs->nb_interval; ++index)
		if (!intervals_equal(lhs->interval[index], rhs->interval[index]))
			return false;

	return true;
}

bool
value_sets_intersect(const struct value_set *lhs, const struct value_set *rhs)
{
	for (uint32_t lhs_index = 0; lhs_index != lhs->nb_interval; ++lhs_index)
		for (uint32_t rhs_index = 0; rhs_index != rhs->nb_interval; ++rhs_index)
			if (intervals_intersect(lhs->interval[lhs_index], rhs->interval[rhs_index]))
				return true;

	return false;
}

bool
value_set_is_covered_by_contiguous(const struct value_set *lhs, uint64_t first, uint64_t last)
{
	const struct value_set_interval rhs = { .first = first, .last = last };
	for (uint32_t lhs_index = 0; lhs_index != lhs->nb_interval; ++lhs_index)
		if (!interval_covers(rhs, lhs->interval[lhs_index]))
			return false;

	return true;
}

bool
value_sets_based_less(const struct value_set *origin, const struct value_set *lhs,
	const struct value_set *rhs)
{
	for (uint32_t origin_index = 0; origin_index != origin->nb_interval; ++origin_index)
		for (uint32_t lhs_index = 0; lhs_index != lhs->nb_interval; ++lhs_index)
			for (uint32_t rhs_index = 0; rhs_index != rhs->nb_interval; ++rhs_index)
				if (!intervals_based_less(origin->interval[origin_index],
						lhs->interval[lhs_index], rhs->interval[rhs_index]))
					return false;
	return true;
}

bool
value_sets_based_less_or_equal(const struct value_set *origin, const struct value_set *lhs,
	const struct value_set *rhs)
{
	for (uint32_t origin_index = 0; origin_index != origin->nb_interval; ++origin_index)
		for (uint32_t lhs_index = 0; lhs_index != lhs->nb_interval; ++lhs_index)
			for (uint32_t rhs_index = 0; rhs_index != rhs->nb_interval; ++rhs_index)
				if (!intervals_based_less_or_equal(origin->interval[origin_index],
						lhs->interval[lhs_index], rhs->interval[rhs_index]))
					return false;
	return true;
}

void
value_set_translate(struct value_set *set, uint64_t offset)
{
	for (uint32_t index = 0; index != set->nb_interval; ++index)
		interval_translate(&set->interval[index], offset);
}

void
value_set_add_contiguous(struct value_set *lhs, uint64_t first, uint64_t last)
{
	value_set_add_interval(lhs, (struct value_set_interval){ .first = first, .last = last });
}
