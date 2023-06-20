/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 * Copyright(c) 2020, Alan Liu <zaoxingliu@gmail.com>
 */

#include <math.h>
#include <string.h>

#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_random.h>
#include <rte_prefetch.h>
#include <rte_ring_elem.h>

#include "rte_member.h"
#include "rte_member_sketch.h"
#include "rte_member_heap.h"

#ifdef CC_AVX512_SUPPORT
#include "rte_member_sketch_avx512.h"
#endif /* CC_AVX512_SUPPORT */

struct sketch_runtime {
	uint64_t pkt_cnt;
	uint32_t until_next;
	int converged;
	struct minheap heap;
	struct node *report_array;
	void *key_slots;
	struct rte_ring *free_key_slots;
} __rte_cache_aligned;

/*
 * Geometric sampling to calculate how many packets needs to be
 * skipped until next update. This method can mitigate the CPU
 * overheads compared with coin-toss sampling.
 */
static uint32_t
draw_geometric(const struct rte_member_setsum *ss)
{
	double rand = 1;

	if (ss->sample_rate == 1)
		return 1;

	while (rand == 1 || rand == 0)
		rand = (double) rte_rand() / (double)(RTE_RAND_MAX);

	return (uint32_t)ceil(log(1 - rand) / log(1 - ss->sample_rate));
}

static void
isort(uint64_t *array, int n)
{
	int i;

	for (i = 1; i < n; i++) {
		uint64_t t = array[i];
		int j;

		for (j = i - 1; j >= 0; j--) {
			if (t < array[j])
				array[j + 1] = array[j];
			else
				break;
		}
		array[j + 1] = t;
	}
}

static __rte_always_inline void
swap(uint64_t *a, uint64_t *b)
{
	uint64_t tmp = *a;
	*a = *b;
	*b = tmp;
}

static uint64_t
medianof5(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e)
{
	if (a > b)
		swap(&a, &b);
	if (c > d)
		swap(&c, &d);
	if (a > c) {
		if (d > e)
			swap(&c, &e);
		else {
			swap(&c, &d);
			swap(&d, &e);
		}
	} else {
		if (b > e)
			swap(&a, &e);
		else {
			swap(&a, &b);
			swap(&b, &e);
		}
	}

	if (a > c)
		return a > d ? d : a;
	else
		return b > c ? c : b;
}

int
rte_member_create_sketch(struct rte_member_setsum *ss,
			 const struct rte_member_parameters *params,
			 struct rte_ring *ring)
{
	struct sketch_runtime *runtime;
	uint32_t num_col;
	uint32_t i;

	if (params->sample_rate == 0 || params->sample_rate > 1) {
		rte_errno = EINVAL;
		RTE_MEMBER_LOG(ERR,
			"Membership Sketch created with invalid parameters\n");
		return -EINVAL;
	}

	if (params->extra_flag & RTE_MEMBER_SKETCH_COUNT_BYTE)
		ss->count_byte = 1;

#ifdef RTE_ARCH_X86
	if (ss->count_byte == 1 &&
		rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512 &&
		rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
		rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512IFMA) == 1) {
#ifdef CC_AVX512_SUPPORT
		ss->use_avx512 = true;
#else
		ss->use_avx512 = false;
#endif
	}

	if (ss->use_avx512 == true) {
#ifdef CC_AVX512_SUPPORT
		ss->num_row = NUM_ROW_VEC;
		RTE_MEMBER_LOG(NOTICE,
			"Membership Sketch AVX512 update/lookup/delete ops is selected\n");
		ss->sketch_update = sketch_update_avx512;
		ss->sketch_lookup = sketch_lookup_avx512;
		ss->sketch_delete = sketch_delete_avx512;
#endif
	} else
#endif
	{
		ss->num_row = NUM_ROW_SCALAR;
		RTE_MEMBER_LOG(NOTICE,
			"Membership Sketch SCALAR update/lookup/delete ops is selected\n");
		ss->sketch_update = sketch_update_scalar;
		ss->sketch_lookup = sketch_lookup_scalar;
		ss->sketch_delete = sketch_delete_scalar;
	}

	ss->socket_id = params->socket_id;

	if (ss->count_byte == 0)
		num_col = 4.0 / params->error_rate / params->sample_rate;
#ifdef RTE_ARCH_X86
	else if (ss->use_avx512 == true)
		num_col = rte_align32pow2(4.0 / params->error_rate);
#endif
	else
		num_col = 4.0 / params->error_rate;

	ss->table = rte_zmalloc_socket(NULL,
			sizeof(uint64_t) * num_col * ss->num_row,
			RTE_CACHE_LINE_SIZE, ss->socket_id);
	if (ss->table == NULL) {
		RTE_MEMBER_LOG(ERR, "Sketch Table memory allocation failed\n");
		return -ENOMEM;
	}

	ss->hash_seeds = rte_zmalloc_socket(NULL, sizeof(uint64_t) * ss->num_row,
			RTE_CACHE_LINE_SIZE, ss->socket_id);
	if (ss->hash_seeds == NULL) {
		RTE_MEMBER_LOG(ERR, "Sketch Hashseeds memory allocation failed\n");
		return -ENOMEM;
	}

	ss->runtime_var = rte_zmalloc_socket(NULL, sizeof(struct sketch_runtime),
					RTE_CACHE_LINE_SIZE, ss->socket_id);
	if (ss->runtime_var == NULL) {
		RTE_MEMBER_LOG(ERR, "Sketch Runtime memory allocation failed\n");
		rte_free(ss);
		return -ENOMEM;
	}
	runtime = ss->runtime_var;

	ss->num_col = num_col;
	ss->sample_rate = params->sample_rate;
	ss->prim_hash_seed = params->prim_hash_seed;
	ss->sec_hash_seed = params->sec_hash_seed;
	ss->error_rate = params->error_rate;
	ss->topk = params->top_k;
	ss->key_len = params->key_len;
	runtime->heap.key_len = ss->key_len;

	runtime->key_slots = rte_zmalloc_socket(NULL, ss->key_len * ss->topk,
					RTE_CACHE_LINE_SIZE, ss->socket_id);
	if (runtime->key_slots == NULL) {
		RTE_MEMBER_LOG(ERR, "Sketch Key Slots allocation failed\n");
		goto error;
	}

	runtime->free_key_slots = ring;
	for (i = 0; i < ss->topk; i++)
		rte_ring_sp_enqueue_elem(runtime->free_key_slots,
					&i, sizeof(uint32_t));

	if (rte_member_minheap_init(&(runtime->heap), params->top_k,
			ss->socket_id, params->prim_hash_seed) < 0) {
		RTE_MEMBER_LOG(ERR, "Sketch Minheap allocation failed\n");
		goto error_runtime;
	}

	runtime->report_array = rte_zmalloc_socket(NULL, sizeof(struct node) * ss->topk,
					RTE_CACHE_LINE_SIZE, ss->socket_id);
	if (runtime->report_array == NULL) {
		RTE_MEMBER_LOG(ERR, "Sketch Runtime Report Array allocation failed\n");
		goto error_runtime;
	}

	for (i = 0; i < ss->num_row; i++)
		ss->hash_seeds[i] = rte_rand();

	if (params->extra_flag & RTE_MEMBER_SKETCH_ALWAYS_BOUNDED)
		ss->always_bounded = 1;

	if (ss->always_bounded) {
		double delta = 1.0 / (pow(2, ss->num_row));

		ss->converge_thresh = 10 * pow(ss->error_rate, -2.0) * sqrt(log(1 / delta));
	}

	RTE_MEMBER_LOG(DEBUG, "Sketch created, "
		"the total memory required is %u Bytes\n",  ss->num_col * ss->num_row * 8);

	return 0;

error_runtime:
	rte_member_minheap_free(&runtime->heap);
	rte_ring_free(runtime->free_key_slots);
	rte_free(runtime->key_slots);
error:
	rte_free(runtime);
	rte_free(ss);

	return -ENOMEM;
}

uint64_t
sketch_lookup_scalar(const struct rte_member_setsum *ss, const void *key)
{
	uint64_t *count_array = ss->table;
	uint32_t col[ss->num_row];
	uint64_t count_row[ss->num_row];
	uint32_t cur_row;
	uint64_t count;

	for (cur_row = 0; cur_row < ss->num_row; cur_row++) {
		col[cur_row] = MEMBER_HASH_FUNC(key, ss->key_len,
			ss->hash_seeds[cur_row]) % ss->num_col;

		rte_prefetch0(&count_array[cur_row * ss->num_col + col[cur_row]]);
	}

	/* if sample rate is 1, it is a regular count-min, we report the min */
	if (ss->sample_rate == 1 || ss->count_byte == 1)
		return count_min(ss, col);

	memset(count_row, 0, sizeof(uint64_t) * ss->num_row);

	/* otherwise we report the median number */
	for (cur_row = 0; cur_row < ss->num_row; cur_row++)
		count_row[cur_row] = count_array[cur_row * ss->num_col + col[cur_row]];

	if (ss->num_row == 5)
		return medianof5(count_row[0], count_row[1],
				count_row[2], count_row[3], count_row[4]);

	isort(count_row, ss->num_row);

	if (ss->num_row % 2 == 0) {
		count = (count_row[ss->num_row / 2] + count_row[ss->num_row / 2 - 1]) / 2;
		return count;
	}
	/* ss->num_row % 2 != 0 */
	count = count_row[ss->num_row / 2];

	return count;
}

void
sketch_delete_scalar(const struct rte_member_setsum *ss, const void *key)
{
	uint32_t col[ss->num_row];
	uint64_t *count_array = ss->table;
	uint32_t cur_row;

	for (cur_row = 0; cur_row < ss->num_row; cur_row++) {
		col[cur_row] = MEMBER_HASH_FUNC(key, ss->key_len,
			ss->hash_seeds[cur_row]) % ss->num_col;

		/* set corresponding counter to 0 */
		count_array[cur_row * ss->num_col + col[cur_row]] = 0;
	}
}

int
rte_member_query_sketch(const struct rte_member_setsum *ss,
			const void *key,
			uint64_t *output)
{
	uint64_t count = ss->sketch_lookup(ss, key);
	*output = count;

	return 0;
}

void
rte_member_update_heap(const struct rte_member_setsum *ss)
{
	uint32_t i;
	struct sketch_runtime *runtime_var = ss->runtime_var;

	for (i = 0; i < runtime_var->heap.size; i++) {
		uint64_t count = ss->sketch_lookup(ss, runtime_var->heap.elem[i].key);

		runtime_var->heap.elem[i].count = count;
	}
}

int
rte_member_report_heavyhitter_sketch(const struct rte_member_setsum *setsum,
				     void **key,
				     uint64_t *count)
{
	uint32_t i;
	struct sketch_runtime *runtime_var = setsum->runtime_var;

	rte_member_update_heap(setsum);
	rte_member_heapsort(&(runtime_var->heap), runtime_var->report_array);

	for (i = 0; i < runtime_var->heap.size; i++) {
		key[i] = runtime_var->report_array[i].key;
		count[i] = runtime_var->report_array[i].count;
	}

	return runtime_var->heap.size;
}

int
rte_member_lookup_sketch(const struct rte_member_setsum *ss,
			 const void *key, member_set_t *set_id)
{
	uint64_t count = ss->sketch_lookup(ss, key);
	struct sketch_runtime *runtime_var = ss->runtime_var;

	if (runtime_var->heap.size > 0 && count >= runtime_var->heap.elem[0].count)
		*set_id = 1;
	else
		*set_id = 0;

	if (count == 0)
		return 0;
	else
		return 1;
}

static void
should_converge(const struct rte_member_setsum *ss)
{
	struct sketch_runtime *runtime_var = ss->runtime_var;

	/* For count min sketch - L1 norm */
	if (runtime_var->pkt_cnt > ss->converge_thresh) {
		runtime_var->converged = 1;
		RTE_MEMBER_LOG(DEBUG, "Sketch converged, begin sampling "
					"from key count %"PRIu64"\n",
					runtime_var->pkt_cnt);
	}
}

static void
sketch_update_row(const struct rte_member_setsum *ss, const void *key,
		  uint32_t count, uint32_t cur_row)
{
	uint64_t *count_array = ss->table;
	uint32_t col = MEMBER_HASH_FUNC(key, ss->key_len,
			ss->hash_seeds[cur_row]) % ss->num_col;

	/* sketch counter update */
	count_array[cur_row * ss->num_col + col] +=
			ceil(count / (ss->sample_rate));
}

void
sketch_update_scalar(const struct rte_member_setsum *ss,
		     const void *key,
		     uint32_t count)
{
	uint64_t *count_array = ss->table;
	uint32_t col;
	uint32_t cur_row;

	for (cur_row = 0; cur_row < ss->num_row; cur_row++) {
		col = MEMBER_HASH_FUNC(key, ss->key_len,
				ss->hash_seeds[cur_row]) % ss->num_col;
		count_array[cur_row * ss->num_col + col] += count;
	}
}

static void
heap_update(const struct rte_member_setsum *ss, const void *key)
{
	struct sketch_runtime *runtime_var = ss->runtime_var;
	uint64_t key_cnt = 0;
	int found;

	/* We also update the heap for this key */
	key_cnt = ss->sketch_lookup(ss, key);
	if (key_cnt > runtime_var->heap.elem[0].count) {
		found = rte_member_minheap_find(&runtime_var->heap, key);
		/* the key is found in the top-k heap */
		if (found >= 0) {
			if (runtime_var->heap.elem[found].count < key_cnt)
				rte_member_heapify(&runtime_var->heap, found, true);

			runtime_var->heap.elem[found].count = key_cnt;
		} else if (runtime_var->heap.size < ss->topk) {
			rte_member_minheap_insert_node(&runtime_var->heap, key,
				key_cnt, runtime_var->key_slots, runtime_var->free_key_slots);
		} else {
			rte_member_minheap_replace_node(&runtime_var->heap, key, key_cnt);
		}
	} else if (runtime_var->heap.size < ss->topk) {
		found = rte_member_minheap_find(&runtime_var->heap, key);
		if (found >= 0) {
			if (runtime_var->heap.elem[found].count < key_cnt)
				rte_member_heapify(&runtime_var->heap, found, true);

			runtime_var->heap.elem[found].count = key_cnt;
		} else
			rte_member_minheap_insert_node(&runtime_var->heap, key,
				key_cnt, runtime_var->key_slots, runtime_var->free_key_slots);
	}
}

/*
 * Add a single packet into the sketch.
 * Sketch value is meatured by packet numbers in this mode.
 */
int
rte_member_add_sketch(const struct rte_member_setsum *ss,
		      const void *key,
		      __rte_unused member_set_t set_id)
{
	uint32_t cur_row;
	struct sketch_runtime *runtime_var = ss->runtime_var;
	uint32_t *until_next = &(runtime_var->until_next);

	/*
	 * If sketch is measured by byte count,
	 * the rte_member_add_sketch_byte_count routine should be used.
	 */
	if (ss->count_byte == 1) {
		RTE_MEMBER_LOG(ERR, "Sketch is Byte Mode, "
			"should use rte_member_add_byte_count()!\n");
		return -EINVAL;
	}

	if (ss->sample_rate == 1) {
		ss->sketch_update(ss, key, 1);
		heap_update(ss, key);
		return 0;
	}

	/* convergence stage if it's needed */
	if (ss->always_bounded && !runtime_var->converged) {
		ss->sketch_update(ss, key, 1);

		if (!((++runtime_var->pkt_cnt) & (INTERVAL - 1)))
			should_converge(ss);

		heap_update(ss, key);
		return 0;
	}

	/* should we skip this packet */
	if (*until_next >= ss->num_row) {
		*until_next -= ss->num_row;
		return 0;
	}
	cur_row = *until_next;
	do {
		sketch_update_row(ss, key, 1, cur_row);
		*until_next = draw_geometric(ss);
		if (cur_row + *until_next >= ss->num_row)
			break;
		cur_row += *until_next;
	} while (1);

	*until_next -= (ss->num_row - cur_row);

	heap_update(ss, key);

	return 0;
}

/*
 * Add the byte count of the packet into the sketch.
 * Sketch value is meatured by byte count numbers in this mode.
 */
int
rte_member_add_sketch_byte_count(const struct rte_member_setsum *ss,
				 const void *key,
				 uint32_t byte_count)
{
	struct sketch_runtime *runtime_var = ss->runtime_var;
	uint32_t *until_next = &(runtime_var->until_next);

	/* should not call this API if not in count byte mode */
	if (ss->count_byte == 0) {
		RTE_MEMBER_LOG(ERR, "Sketch is Pkt Mode, "
			"should use rte_member_add()!\n");
		return -EINVAL;
	}

	/* there's specific optimization for the sketch update */
	ss->sketch_update(ss, key, byte_count);

	if (*until_next != 0) {
		*until_next = *until_next - 1;
		return 0;
	}

	*until_next = draw_geometric(ss) - 1;

	heap_update(ss, key);

	return 0;
}

int
rte_member_delete_sketch(const struct rte_member_setsum *ss,
			 const void *key)
{
	struct sketch_runtime *runtime_var = ss->runtime_var;
	int found;

	found = rte_member_minheap_find(&runtime_var->heap, key);
	if (found < 0)
		return -1;

	ss->sketch_delete(ss, key);

	return rte_member_minheap_delete_node
		(&runtime_var->heap, key, runtime_var->key_slots, runtime_var->free_key_slots);
}

void
rte_member_free_sketch(struct rte_member_setsum *ss)
{
	struct sketch_runtime *runtime_var = ss->runtime_var;

	rte_free(ss->table);
	rte_member_minheap_free(&runtime_var->heap);
	rte_free(runtime_var->key_slots);
	rte_ring_free(runtime_var->free_key_slots);
	rte_free(runtime_var);
}

void
rte_member_reset_sketch(const struct rte_member_setsum *ss)
{
	struct sketch_runtime *runtime_var = ss->runtime_var;
	uint64_t *sketch = ss->table;
	uint32_t i;

	memset(sketch, 0, sizeof(uint64_t) * ss->num_col * ss->num_row);
	rte_member_minheap_reset(&runtime_var->heap);
	rte_ring_reset(runtime_var->free_key_slots);

	for (i = 0; i < ss->topk; i++)
		rte_ring_sp_enqueue_elem(runtime_var->free_key_slots, &i, sizeof(uint32_t));
}
