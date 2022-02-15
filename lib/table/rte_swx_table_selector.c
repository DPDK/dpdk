/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <rte_common.h>

#include "rte_swx_table_selector.h"

#ifndef RTE_SWX_TABLE_SELECTOR_HUGE_PAGES_DISABLE

#include <rte_malloc.h>

static void *
env_calloc(size_t size, size_t alignment, int numa_node)
{
	return rte_zmalloc_socket(NULL, size, alignment, numa_node);
}

static void
env_free(void *start, size_t size __rte_unused)
{
	rte_free(start);
}

#else

#include <numa.h>

static void *
env_calloc(size_t size, size_t alignment __rte_unused, int numa_node)
{
	void *start;

	if (numa_available() == -1)
		return NULL;

	start = numa_alloc_onnode(size, numa_node);
	if (!start)
		return NULL;

	memset(start, 0, size);
	return start;
}

static void
env_free(void *start, size_t size)
{
	if ((numa_available() == -1) || !start)
		return;

	numa_free(start, size);
}

#endif

#if defined(RTE_ARCH_X86_64)

#include <x86intrin.h>

#define crc32_u64(crc, v) _mm_crc32_u64(crc, v)

#else

static inline uint64_t
crc32_u64_generic(uint64_t crc, uint64_t value)
{
	int i;

	crc = (crc & 0xFFFFFFFFLLU) ^ value;
	for (i = 63; i >= 0; i--) {
		uint64_t mask;

		mask = -(crc & 1LLU);
		crc = (crc >> 1LLU) ^ (0x82F63B78LLU & mask);
	}

	return crc;
}

#define crc32_u64(crc, v) crc32_u64_generic(crc, v)

#endif

/* Key size needs to be one of: 8, 16, 32 or 64. */
static inline uint32_t
hash(void *key, void *key_mask, uint32_t key_size, uint32_t seed)
{
	uint64_t *k = key;
	uint64_t *m = key_mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	switch (key_size) {
	case 8:
		crc0 = crc32_u64(seed, k[0] & m[0]);
		return crc0;

	case 16:
		k0 = k[0] & m[0];

		crc0 = crc32_u64(k0, seed);
		crc1 = crc32_u64(k0 >> 32, k[1] & m[1]);

		crc0 ^= crc1;

		return crc0;

	case 32:
		k0 = k[0] & m[0];
		k2 = k[2] & m[2];

		crc0 = crc32_u64(k0, seed);
		crc1 = crc32_u64(k0 >> 32, k[1] & m[1]);

		crc2 = crc32_u64(k2, k[3] & m[3]);
		crc3 = k2 >> 32;

		crc0 = crc32_u64(crc0, crc1);
		crc1 = crc32_u64(crc2, crc3);

		crc0 ^= crc1;

		return crc0;

	case 64:
		k0 = k[0] & m[0];
		k2 = k[2] & m[2];
		k5 = k[5] & m[5];

		crc0 = crc32_u64(k0, seed);
		crc1 = crc32_u64(k0 >> 32, k[1] & m[1]);

		crc2 = crc32_u64(k2, k[3] & m[3]);
		crc3 = crc32_u64(k2 >> 32, k[4] & m[4]);

		crc4 = crc32_u64(k5, k[6] & m[6]);
		crc5 = crc32_u64(k5 >> 32, k[7] & m[7]);

		crc0 = crc32_u64(crc0, (crc1 << 32) ^ crc2);
		crc1 = crc32_u64(crc3, (crc4 << 32) ^ crc5);

		crc0 ^= crc1;

		return crc0;

	default:
		crc0 = 0;
		return crc0;
	}
}

struct group_member_info {
	uint32_t member_id;
	uint32_t member_weight;
	uint32_t member_weight_normalized;
	uint32_t count;
};

struct table {
	/* Input parameters */
	struct rte_swx_table_selector_params params;

	/* Internal. */
	uint32_t *group_table;
	uint64_t group_table_size;
	struct group_member_info *members;
	uint32_t n_members_per_group_max_log2;
};

uint64_t
rte_swx_table_selector_footprint_get(uint32_t n_groups_max, uint32_t n_members_per_group_max)
{
	uint64_t group_table_size, members_size;

	group_table_size = n_groups_max * n_members_per_group_max * sizeof(uint32_t);

	members_size = n_members_per_group_max * sizeof(struct group_member_info);

	return sizeof(struct table) + group_table_size + members_size;
}

void
rte_swx_table_selector_free(void *table)
{
	struct table *t = table;

	if (!t)
		return;

	free(t->members);

	env_free(t->group_table, t->group_table_size);

	free(t->params.selector_mask);

	free(t);
}

static int
table_create_check(struct rte_swx_table_selector_params *params)
{
	if (!params)
		return -1;

	if (!params->selector_size ||
	    (params->selector_size > 64) ||
	    !params->n_groups_max ||
	    (params->n_groups_max > 1U << 31) ||
	    !params->n_members_per_group_max ||
	    (params->n_members_per_group_max > 1U << 31))
		return -EINVAL;

	return 0;
}

static int
table_params_copy(struct table *t, struct rte_swx_table_selector_params *params)
{
	uint32_t selector_size, i;

	selector_size = rte_align32pow2(params->selector_size);
	if (selector_size < 8)
		selector_size = 8;

	memcpy(&t->params, params, sizeof(struct rte_swx_table_selector_params));
	t->params.selector_size = selector_size;
	t->params.selector_mask = NULL;
	t->params.n_groups_max = rte_align32pow2(params->n_groups_max);
	t->params.n_members_per_group_max = rte_align32pow2(params->n_members_per_group_max);

	for (i = 0; i < 32; i++)
		if (params->n_members_per_group_max == 1U << i)
			t->n_members_per_group_max_log2 = i;

	/* t->params.selector_mask */
	t->params.selector_mask = calloc(selector_size, sizeof(uint8_t));
	if (!t->params.selector_mask)
		goto error;

	if (params->selector_mask)
		memcpy(t->params.selector_mask, params->selector_mask, params->selector_size);
	else
		memset(t->params.selector_mask, 0xFF, params->selector_size);

	return 0;

error:
	free(t->params.selector_mask);
	t->params.selector_mask = NULL;

	return -ENOMEM;
}

static int
group_set(struct table *t,
	  uint32_t group_id,
	  struct rte_swx_table_selector_group *group);

void *
rte_swx_table_selector_create(struct rte_swx_table_selector_params *params,
			      struct rte_swx_table_selector_group **groups,
			      int numa_node)
{
	struct table *t = NULL;
	uint32_t group_size, i;
	int status;

	/* Check input arguments. */
	status = table_create_check(params);
	if (status)
		goto error;

	/* Table object. */
	t = calloc(1, sizeof(struct table));
	if (!t)
		goto error;

	/* Parameter copy. */
	status = table_params_copy(t, params);
	if (status)
		goto error;

	/* Group. */
	group_size = params->n_members_per_group_max * sizeof(uint32_t);
	t->group_table_size = params->n_groups_max * group_size;

	t->group_table = env_calloc(t->group_table_size, RTE_CACHE_LINE_SIZE, numa_node);
	if (!t->group_table)
		goto error;

	t->members = calloc(params->n_members_per_group_max, sizeof(struct group_member_info));
	if (!t->members)
		goto error;

	if (groups)
		for (i = 0; i < params->n_groups_max; i++)
			if (groups[i]) {
				status = group_set(t, i, groups[i]);
				if (status)
					goto error;
			}

	return t;

error:
	rte_swx_table_selector_free(t);
	return NULL;
}


static int
group_check(struct table *t, struct rte_swx_table_selector_group *group)
{
	struct rte_swx_table_selector_member *elem;
	uint32_t n_members = 0;

	if (!group)
		return 0;

	TAILQ_FOREACH(elem, &group->members, node) {
		struct rte_swx_table_selector_member *e;
		uint32_t n = 0;

		/* Check group size. */
		if (n_members >= t->params.n_members_per_group_max)
			return -ENOSPC;

		/* Check attributes of the current group member. */
		if (elem->member_id >= t->params.n_members_per_group_max ||
		    !elem->member_weight)
			return -ENOSPC;

		/* Check against duplicate member IDs. */
		TAILQ_FOREACH(e, &group->members, node)
			if (e->member_id == elem->member_id)
				n++;

		if (n != 1)
			return -EINVAL;

		/* Update group size. */
		n_members++;
	}

	return 0;
}

static uint32_t
members_read(struct group_member_info *members,
	     struct rte_swx_table_selector_group *group)
{
	struct rte_swx_table_selector_member *elem;
	uint32_t n_members = 0;

	if (!group)
		return 0;

	TAILQ_FOREACH(elem, &group->members, node) {
		struct group_member_info *m = &members[n_members];

		memset(m, 0, sizeof(struct group_member_info));

		m->member_id = elem->member_id;
		m->member_weight = elem->member_weight;
		m->member_weight_normalized = elem->member_weight;

		n_members++;
	}

	return n_members;
}

static uint32_t
members_min_weight_find(struct group_member_info *members, uint32_t n_members)
{
	uint32_t min = UINT32_MAX, i;

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];

		if (m->member_weight < min)
			min = m->member_weight;
	}

	return min;
}

static uint32_t
members_weight_divisor_check(struct group_member_info *members,
			     uint32_t n_members,
			     uint32_t divisor)
{
	uint32_t i;

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];

		if (m->member_weight_normalized % divisor)
			return 0; /* FALSE. */
	}

	return 1; /* TRUE. */
}

static void
members_weight_divisor_apply(struct group_member_info *members,
			     uint32_t n_members,
			     uint32_t divisor)
{
	uint32_t i;

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];

		m->member_weight_normalized /= divisor;
	}
}

static uint32_t
members_weight_sum(struct group_member_info *members, uint32_t n_members)
{
	uint32_t result = 0, i;

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];

		result += m->member_weight_normalized;
	}

	return result;
}

static void
members_weight_scale(struct group_member_info *members,
		     uint32_t n_members,
		     uint32_t n_members_per_group_max,
		     uint32_t weight_sum)
{
	uint32_t multiplier, remainder, i;

	multiplier = n_members_per_group_max / weight_sum;
	remainder = n_members_per_group_max % weight_sum;

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];

		m->count = m->member_weight_normalized * multiplier;
	}

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];
		uint32_t min;

		min = m->member_weight_normalized;
		if (remainder < m->member_weight_normalized)
			min = remainder;

		m->count += min;
		remainder -= min;
		if (!remainder)
			break;
	}
}

static void
members_write(struct group_member_info *members,
	      uint32_t n_members,
	      uint32_t *group_table)
{
	uint32_t pos = 0, i;

	for (i = 0; i < n_members; i++) {
		struct group_member_info *m = &members[i];
		uint32_t j;

		for (j = 0; j < m->count; j++)
			group_table[pos++] = m->member_id;
	}
}

static int
group_set(struct table *t,
	  uint32_t group_id,
	  struct rte_swx_table_selector_group *group)
{
	uint32_t *gt = &t->group_table[group_id * t->params.n_members_per_group_max];
	struct group_member_info *members = t->members;
	uint32_t n_members, weight_min, weight_sum, divisor;
	int status = 0;

	/* Check input arguments. */
	if (group_id >= t->params.n_groups_max)
		return -EINVAL;

	status = group_check(t, group);
	if (status)
		return status;

	/* Read group members. */
	n_members = members_read(members, group);

	if (!n_members) {
		memset(gt, 0, t->params.n_members_per_group_max * sizeof(uint32_t));

		return 0;
	}

	/* Normalize weights. */
	weight_min = members_min_weight_find(members, n_members);

	for (divisor = 2; divisor <= weight_min; divisor++)
		if (members_weight_divisor_check(members, n_members, divisor))
			members_weight_divisor_apply(members, n_members, divisor);

	/* Scale weights. */
	weight_sum = members_weight_sum(members, n_members);
	if (weight_sum > t->params.n_members_per_group_max)
		return -ENOSPC;

	members_weight_scale(members, n_members, t->params.n_members_per_group_max, weight_sum);

	/* Write group members to the group table. */
	members_write(members, n_members, gt);

	return 0;
}

int
rte_swx_table_selector_group_set(void *table,
				 uint32_t group_id,
				 struct rte_swx_table_selector_group *group)
{
	struct table *t = table;

	return group_set(t, group_id, group);
}

struct mailbox {

};

uint64_t
rte_swx_table_selector_mailbox_size_get(void)
{
	return sizeof(struct mailbox);
}

int
rte_swx_table_selector_select(void *table,
			      void *mailbox __rte_unused,
			      uint8_t **group_id_buffer,
			      uint8_t **selector_buffer,
			      uint8_t **member_id_buffer)
{
	struct table *t = table;
	uint32_t *group_id_ptr, *member_id_ptr, group_id, member_id, selector, group_member_index;

	group_id_ptr = (uint32_t *)&(*group_id_buffer)[t->params.group_id_offset];

	member_id_ptr = (uint32_t *)&(*member_id_buffer)[t->params.member_id_offset];

	group_id = *group_id_ptr & (t->params.n_groups_max - 1);

	selector = hash(&(*selector_buffer)[t->params.selector_offset],
			t->params.selector_mask,
			t->params.selector_size,
			0);

	group_member_index = selector & (t->params.n_members_per_group_max - 1);

	member_id = t->group_table[(group_id << t->n_members_per_group_max_log2) +
				   group_member_index];

	*member_id_ptr = member_id;

	return 1;
}
