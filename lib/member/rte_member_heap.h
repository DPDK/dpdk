/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 * Copyright(c) 2020, Alan Liu <zaoxingliu@gmail.com>
 */

#ifndef RTE_MEMBER_HEAP_H
#define RTE_MEMBER_HEAP_H

#include "member.h"
#include <rte_ring_elem.h>
#include "rte_member.h"

#define LCHILD(x) (2 * x + 1)
#define RCHILD(x) (2 * x + 2)
#define PARENT(x) ((x - 1) / 2)

#define HASH_BKT_SIZE 16
#define HASH_HP_MULTI 4
#define HASH_RESIZE_MULTI 2

struct hash_bkt {
	uint16_t sig[HASH_BKT_SIZE];
	uint16_t idx[HASH_BKT_SIZE];
};

struct hash {
	uint16_t bkt_cnt;
	uint16_t num_item;
	uint32_t seed;
	struct hash_bkt buckets[0];
};

struct node {
	void *key;
	uint64_t count;
};

struct minheap {
	uint32_t key_len;
	uint32_t size;
	uint32_t socket;
	struct hash *hashtable;
	struct node *elem;
};

static int
hash_table_insert(const void *key, int value, int key_len, struct hash *table)
{
	uint32_t hash = MEMBER_HASH_FUNC(key, key_len, table->seed);
	uint16_t idx = hash % table->bkt_cnt;
	uint16_t sig = hash >> 16;
	int i;

	for (i = 0; i < HASH_BKT_SIZE; i++) {
		if (table->buckets[idx].idx[i] == 0) {
			table->buckets[idx].idx[i] = value;
			table->buckets[idx].sig[i] = sig;
			table->num_item++;
			return 0;
		}
	}

	return -ENOMEM;
}

static int
hash_table_update(const void *key, int old_value, int value, int key_len, struct hash *table)
{
	uint32_t hash = MEMBER_HASH_FUNC(key, key_len, table->seed);
	uint16_t idx = hash % table->bkt_cnt;
	uint16_t sig = hash >> 16;
	int i;

	for (i = 0; i < HASH_BKT_SIZE; i++) {
		if (table->buckets[idx].sig[i] == sig && table->buckets[idx].idx[i] == old_value) {
			table->buckets[idx].idx[i] = value;
			return 0;
		}
	}

	return -1;
}

static int
hash_table_del(const void *key, uint16_t value, int key_len, struct hash *table)
{
	uint32_t hash = MEMBER_HASH_FUNC(key, key_len, table->seed);
	uint16_t idx = hash % table->bkt_cnt;
	uint16_t sig = hash >> 16;
	int i;

	for (i = 0; i < HASH_BKT_SIZE; i++) {
		if (table->buckets[idx].sig[i] == sig && table->buckets[idx].idx[i] == value) {
			table->buckets[idx].idx[i] = 0;
			table->num_item--;
			return 0;
		}
	}

	return -1;
}

static int
hash_table_lookup(const void *key, int key_len, struct minheap *hp)
{
	struct hash *table = hp->hashtable;
	uint32_t hash = MEMBER_HASH_FUNC(key, key_len, table->seed);
	uint16_t idx = hash % table->bkt_cnt;
	uint16_t sig = hash >> 16;
	int i;

	for (i = 0; i < HASH_BKT_SIZE; i++) {
		if (table->buckets[idx].sig[i] == sig && table->buckets[idx].idx[i] != 0) {
			uint32_t hp_idx = table->buckets[idx].idx[i] - 1;

			if (memcmp(hp->elem[hp_idx].key, key, hp->key_len) == 0)
				return hp_idx;
		}
	}

	return -ENOENT; /* key doesn't exist */
}

static int
resize_hash_table(struct minheap *hp)
{
	uint32_t i;
	uint32_t new_bkt_cnt;

	while (1) {
		new_bkt_cnt = hp->hashtable->bkt_cnt * HASH_RESIZE_MULTI;

		MEMBER_LOG(ERR, "Sketch Minheap HT load factor is [%f]",
			hp->hashtable->num_item / ((float)hp->hashtable->bkt_cnt * HASH_BKT_SIZE));
		MEMBER_LOG(ERR, "Sketch Minheap HT resize happen!");
		rte_free(hp->hashtable);
		hp->hashtable = rte_zmalloc_socket(NULL, sizeof(struct hash) +
						new_bkt_cnt * sizeof(struct hash_bkt),
						RTE_CACHE_LINE_SIZE, hp->socket);

		if (hp->hashtable == NULL) {
			MEMBER_LOG(ERR, "Sketch Minheap HT allocation failed");
			return -ENOMEM;
		}

		hp->hashtable->bkt_cnt = new_bkt_cnt;

		for (i = 0; i < hp->size; ++i) {
			if (hash_table_insert(hp->elem[i].key,
				i + 1, hp->key_len, hp->hashtable) < 0) {
				MEMBER_LOG(ERR,
					"Sketch Minheap HT resize insert fail!");
				break;
			}
		}
		if (i == hp->size)
			break;
	}

	return 0;
}

/* find the item in the given minheap */
static int
rte_member_minheap_find(struct minheap *hp, const void *key)
{
	int idx = hash_table_lookup(key, hp->key_len, hp);
	return idx;
}

static int
rte_member_minheap_init(struct minheap *heap, int size,
			uint32_t socket, uint32_t seed)
{
	heap->elem = rte_zmalloc_socket(NULL, sizeof(struct node) * size,
				RTE_CACHE_LINE_SIZE, socket);
	if (heap->elem == NULL) {
		MEMBER_LOG(ERR, "Sketch Minheap elem allocation failed");
		return -ENOMEM;
	}

	uint32_t hash_bkt_cnt = rte_align32pow2(size * HASH_HP_MULTI) / HASH_BKT_SIZE;

	if (hash_bkt_cnt == 0)
		hash_bkt_cnt = 1;

	heap->hashtable = rte_zmalloc_socket(NULL, sizeof(struct hash) +
					hash_bkt_cnt * sizeof(struct hash_bkt),
					RTE_CACHE_LINE_SIZE, socket);

	if (heap->hashtable == NULL) {
		MEMBER_LOG(ERR, "Sketch Minheap HT allocation failed");
		rte_free(heap->elem);
		return -ENOMEM;
	}

	heap->hashtable->seed = seed;
	heap->hashtable->bkt_cnt = hash_bkt_cnt;
	heap->socket = socket;

	return 0;
}

/* swap the minheap nodes */
static __rte_always_inline void
rte_member_heap_swap(struct node *n1, struct node *n2)
{
	struct node temp = *n1;
	*n1 = *n2;
	*n2 = temp;
}

/* heapify function */
static void
rte_member_heapify(struct minheap *hp, uint32_t idx, bool update_hash)
{
	uint32_t smallest;

	if (LCHILD(idx) < hp->size &&
			hp->elem[LCHILD(idx)].count < hp->elem[idx].count)
		smallest = LCHILD(idx);
	else
		smallest = idx;

	if (RCHILD(idx) < hp->size &&
			hp->elem[RCHILD(idx)].count < hp->elem[smallest].count)
		smallest = RCHILD(idx);

	if (smallest != idx) {
		rte_member_heap_swap(&(hp->elem[idx]), &(hp->elem[smallest]));

		if (update_hash) {
			if (hash_table_update(hp->elem[smallest].key, idx + 1, smallest + 1,
					hp->key_len, hp->hashtable) < 0) {
				MEMBER_LOG(ERR, "Minheap Hash Table update failed");
				return;
			}

			if (hash_table_update(hp->elem[idx].key, smallest + 1, idx + 1,
					hp->key_len, hp->hashtable) < 0) {
				MEMBER_LOG(ERR, "Minheap Hash Table update failed");
				return;
			}
		}
		rte_member_heapify(hp, smallest, update_hash);
	}
}

/* insert a node into the minheap */
static int
rte_member_minheap_insert_node(struct minheap *hp, const void *key,
			       int counter, void *key_slot,
			       struct rte_ring *free_key_slot)
{
	struct node nd;
	uint32_t slot_id;

	if (rte_ring_sc_dequeue_elem(free_key_slot, &slot_id, sizeof(uint32_t)) != 0) {
		MEMBER_LOG(ERR, "Minheap get empty keyslot failed");
		return -1;
	}

	nd.count = counter;
	nd.key = RTE_PTR_ADD(key_slot, slot_id * hp->key_len);

	memcpy(nd.key, key, hp->key_len);

	uint32_t i = (hp->size)++;

	while (i && nd.count < hp->elem[PARENT(i)].count) {
		hp->elem[i] = hp->elem[PARENT(i)];
		if (hash_table_update(hp->elem[i].key, PARENT(i) + 1, i + 1,
				hp->key_len, hp->hashtable) < 0) {
			MEMBER_LOG(ERR, "Minheap Hash Table update failed");
			return -1;
		}
		i = PARENT(i);
	}
	hp->elem[i] = nd;

	if (hash_table_insert(key, i + 1, hp->key_len, hp->hashtable) < 0) {
		if (resize_hash_table(hp) < 0) {
			MEMBER_LOG(ERR, "Minheap Hash Table resize failed");
			return -1;
		}
	}

	return 0;
}

/* delete a key from the minheap */
static int
rte_member_minheap_delete_node(struct minheap *hp, const void *key,
			       void *key_slot, struct rte_ring *free_key_slot)
{
	int idx = rte_member_minheap_find(hp, key);
	uint32_t offset = RTE_PTR_DIFF(hp->elem[idx].key, key_slot) / hp->key_len;

	if (hash_table_del(key, idx + 1, hp->key_len, hp->hashtable) < 0) {
		MEMBER_LOG(ERR, "Minheap Hash Table delete failed");
		return -1;
	}

	rte_ring_sp_enqueue_elem(free_key_slot, &offset, sizeof(uint32_t));

	if (idx == (int)(hp->size - 1)) {
		hp->size--;
		return 0;
	}

	hp->elem[idx] = hp->elem[hp->size - 1];

	if (hash_table_update(hp->elem[idx].key, hp->size, idx + 1,
				hp->key_len, hp->hashtable) < 0) {
		MEMBER_LOG(ERR, "Minheap Hash Table update failed");
		return -1;
	}
	hp->size--;
	rte_member_heapify(hp, idx, true);

	return 0;
}

/* replace a min node with a new key. */
static int
rte_member_minheap_replace_node(struct minheap *hp,
				const void *new_key,
				int new_counter)
{
	struct node nd;
	void *recycle_key = NULL;

	recycle_key = hp->elem[0].key;

	if (hash_table_del(recycle_key, 1, hp->key_len, hp->hashtable) < 0) {
		MEMBER_LOG(ERR, "Minheap Hash Table delete failed");
		return -1;
	}

	hp->elem[0] = hp->elem[hp->size - 1];

	if (hash_table_update(hp->elem[0].key, hp->size, 1,
				hp->key_len, hp->hashtable) < 0) {
		MEMBER_LOG(ERR, "Minheap Hash Table update failed");
		return -1;
	}
	hp->size--;

	rte_member_heapify(hp, 0, true);

	nd.count = new_counter;
	nd.key = recycle_key;

	memcpy(nd.key, new_key, hp->key_len);

	uint32_t i = (hp->size)++;

	while (i && nd.count < hp->elem[PARENT(i)].count) {
		hp->elem[i] = hp->elem[PARENT(i)];
		if (hash_table_update(hp->elem[i].key, PARENT(i) + 1, i + 1,
				hp->key_len, hp->hashtable) < 0) {
			MEMBER_LOG(ERR, "Minheap Hash Table update failed");
			return -1;
		}
		i = PARENT(i);
	}

	hp->elem[i] = nd;

	if (hash_table_insert(new_key, i + 1, hp->key_len, hp->hashtable) < 0) {
		MEMBER_LOG(ERR, "Minheap Hash Table replace insert failed");
		if (resize_hash_table(hp) < 0) {
			MEMBER_LOG(ERR, "Minheap Hash Table replace resize failed");
			return -1;
		}
	}

	return 0;
}

/* sort the heap into a descending array */
static void
rte_member_heapsort(struct minheap *hp, struct node *result_array)
{
	struct minheap new_hp;

	/* build a new heap for using the given array */
	new_hp.size = hp->size;
	new_hp.key_len = hp->key_len;
	new_hp.elem = result_array;
	memcpy(result_array, hp->elem, hp->size * sizeof(struct node));

	/* sort the new heap */
	while (new_hp.size > 1) {
		rte_member_heap_swap(&(new_hp.elem[0]), &(new_hp.elem[new_hp.size - 1]));
		new_hp.size--;
		rte_member_heapify(&new_hp, 0, false);
	}
}

static void
rte_member_minheap_free(struct minheap *hp)
{
	if (hp == NULL)
		return;

	rte_free(hp->elem);
	rte_free(hp->hashtable);
}

static void
rte_member_minheap_reset(struct minheap *hp)
{
	if (hp == NULL)
		return;

	memset(hp->elem, 0, sizeof(struct node) * hp->size);
	hp->size = 0;

	memset((char *)hp->hashtable + sizeof(struct hash), 0,
			hp->hashtable->bkt_cnt * sizeof(struct hash_bkt));
	hp->hashtable->num_item = 0;
}

#endif /* RTE_MEMBER_HEAP_H */
