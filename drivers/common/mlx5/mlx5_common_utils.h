/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_UTILS_H_
#define RTE_PMD_MLX5_COMMON_UTILS_H_

#include "mlx5_common.h"

/************************ mlx5 list *****************************/

/** Maximum size of string for naming. */
#define MLX5_NAME_SIZE			32

struct mlx5_list;

/**
 * Structure of the entry in the mlx5 list, user should define its own struct
 * that contains this in order to store the data.
 */
struct mlx5_list_entry {
	LIST_ENTRY(mlx5_list_entry) next; /* Entry pointers in the list. */
	uint32_t ref_cnt; /* 0 means, entry is invalid. */
	uint32_t lcore_idx;
	struct mlx5_list_entry *gentry;
};

struct mlx5_list_cache {
	LIST_HEAD(mlx5_list_head, mlx5_list_entry) h;
	uint32_t inv_cnt; /* Invalid entries counter. */
} __rte_cache_aligned;

/**
 * Type of callback function for entry removal.
 *
 * @param list
 *   The mlx5 list.
 * @param entry
 *   The entry in the list.
 */
typedef void (*mlx5_list_remove_cb)(struct mlx5_list *list,
				     struct mlx5_list_entry *entry);

/**
 * Type of function for user defined matching.
 *
 * @param list
 *   The mlx5 list.
 * @param entry
 *   The entry in the list.
 * @param ctx
 *   The pointer to new entry context.
 *
 * @return
 *   0 if matching, non-zero number otherwise.
 */
typedef int (*mlx5_list_match_cb)(struct mlx5_list *list,
				   struct mlx5_list_entry *entry, void *ctx);

typedef struct mlx5_list_entry *(*mlx5_list_clone_cb)
				 (struct mlx5_list *list,
				  struct mlx5_list_entry *entry, void *ctx);

typedef void (*mlx5_list_clone_free_cb)(struct mlx5_list *list,
					 struct mlx5_list_entry *entry);

/**
 * Type of function for user defined mlx5 list entry creation.
 *
 * @param list
 *   The mlx5 list.
 * @param entry
 *   The new allocated entry, NULL if list entry size unspecified,
 *   New entry has to be allocated in callback and return.
 * @param ctx
 *   The pointer to new entry context.
 *
 * @return
 *   Pointer of entry on success, NULL otherwise.
 */
typedef struct mlx5_list_entry *(*mlx5_list_create_cb)
				 (struct mlx5_list *list,
				  struct mlx5_list_entry *entry,
				  void *ctx);

/**
 * Linked mlx5 list structure.
 *
 * Entry in mlx5 list could be reused if entry already exists,
 * reference count will increase and the existing entry returns.
 *
 * When destroy an entry from list, decrease reference count and only
 * destroy when no further reference.
 *
 * Linked list is designed for limited number of entries,
 * read mostly, less modification.
 *
 * For huge amount of entries, please consider hash list.
 *
 */
struct mlx5_list {
	char name[MLX5_NAME_SIZE]; /**< Name of the mlx5 list. */
	void *ctx; /* user objects target to callback. */
	bool lcores_share; /* Whether to share objects between the lcores. */
	mlx5_list_create_cb cb_create; /**< entry create callback. */
	mlx5_list_match_cb cb_match; /**< entry match callback. */
	mlx5_list_remove_cb cb_remove; /**< entry remove callback. */
	mlx5_list_clone_cb cb_clone; /**< entry clone callback. */
	mlx5_list_clone_free_cb cb_clone_free;
	struct mlx5_list_cache cache[RTE_MAX_LCORE + 1];
	/* Lcore cache, last index is the global cache. */
	volatile uint32_t gen_cnt; /* List modification may update it. */
	volatile uint32_t count; /* number of entries in list. */
	rte_rwlock_t lock; /* read/write lock. */
};

/**
 * Create a mlx5 list.
 *
 * For actions in SW-steering is only memory and  can be allowed
 * to create duplicate objects, the lists don't need to check if
 * there are existing same objects in other sub local lists,
 * search the object only in local list will be more efficient.
 *
 * @param list
 *   Pointer to the hast list table.
 * @param name
 *   Name of the mlx5 list.
 * @param ctx
 *   Pointer to the list context data.
 * @param lcores_share
 *   Whether to share objects between the lcores.
 * @param cb_create
 *   Callback function for entry create.
 * @param cb_match
 *   Callback function for entry match.
 * @param cb_remove
 *   Callback function for entry remove.
 * @return
 *   List pointer on success, otherwise NULL.
 */
__rte_internal
struct mlx5_list *mlx5_list_create(const char *name, void *ctx,
				   bool lcores_share,
				   mlx5_list_create_cb cb_create,
				   mlx5_list_match_cb cb_match,
				   mlx5_list_remove_cb cb_remove,
				   mlx5_list_clone_cb cb_clone,
				   mlx5_list_clone_free_cb cb_clone_free);

/**
 * Search an entry matching the key.
 *
 * Result returned might be destroyed by other thread, must use
 * this function only in main thread.
 *
 * @param list
 *   Pointer to the mlx5 list.
 * @param ctx
 *   Common context parameter used by entry callback function.
 *
 * @return
 *   Pointer of the list entry if found, NULL otherwise.
 */
__rte_internal
struct mlx5_list_entry *mlx5_list_lookup(struct mlx5_list *list,
					   void *ctx);

/**
 * Reuse or create an entry to the mlx5 list.
 *
 * @param list
 *   Pointer to the hast list table.
 * @param ctx
 *   Common context parameter used by callback function.
 *
 * @return
 *   registered entry on success, NULL otherwise
 */
__rte_internal
struct mlx5_list_entry *mlx5_list_register(struct mlx5_list *list,
					     void *ctx);

/**
 * Remove an entry from the mlx5 list.
 *
 * User should guarantee the validity of the entry.
 *
 * @param list
 *   Pointer to the hast list.
 * @param entry
 *   Entry to be removed from the mlx5 list table.
 * @return
 *   0 on entry removed, 1 on entry still referenced.
 */
__rte_internal
int mlx5_list_unregister(struct mlx5_list *list,
			  struct mlx5_list_entry *entry);

/**
 * Destroy the mlx5 list.
 *
 * @param list
 *   Pointer to the mlx5 list.
 */
__rte_internal
void mlx5_list_destroy(struct mlx5_list *list);

/**
 * Get entry number from the mlx5 list.
 *
 * @param list
 *   Pointer to the hast list.
 * @return
 *   mlx5 list entry number.
 */
__rte_internal
uint32_t
mlx5_list_get_entry_num(struct mlx5_list *list);

/************************ Hash list *****************************/

#define MLX5_HLIST_DIRECT_KEY 0x0001 /* Use the key directly as hash index. */
#define MLX5_HLIST_WRITE_MOST 0x0002 /* List mostly used for append new. */

/** Maximum size of string for naming the hlist table. */
#define MLX5_HLIST_NAMESIZE			32

struct mlx5_hlist;

/**
 * Structure of the entry in the hash list, user should define its own struct
 * that contains this in order to store the data. The 'key' is 64-bits right
 * now and its user's responsibility to guarantee there is no collision.
 */
struct mlx5_hlist_entry {
	LIST_ENTRY(mlx5_hlist_entry) next; /* entry pointers in the list. */
	uint32_t idx; /* Bucket index the entry belongs to. */
	uint32_t ref_cnt; /* Reference count. */
};

/** Structure for hash head. */
LIST_HEAD(mlx5_hlist_head, mlx5_hlist_entry);

/**
 * Type of callback function for entry removal.
 *
 * @param list
 *   The hash list.
 * @param entry
 *   The entry in the list.
 */
typedef void (*mlx5_hlist_remove_cb)(struct mlx5_hlist *list,
				     struct mlx5_hlist_entry *entry);

/**
 * Type of function for user defined matching.
 *
 * @param list
 *   The hash list.
 * @param entry
 *   The entry in the list.
 * @param key
 *   The new entry key.
 * @param ctx
 *   The pointer to new entry context.
 *
 * @return
 *   0 if matching, non-zero number otherwise.
 */
typedef int (*mlx5_hlist_match_cb)(struct mlx5_hlist *list,
				   struct mlx5_hlist_entry *entry,
				   uint64_t key, void *ctx);

/**
 * Type of function for user defined hash list entry creation.
 *
 * @param list
 *   The hash list.
 * @param key
 *   The key of the new entry.
 * @param ctx
 *   The pointer to new entry context.
 *
 * @return
 *   Pointer to allocated entry on success, NULL otherwise.
 */
typedef struct mlx5_hlist_entry *(*mlx5_hlist_create_cb)
				  (struct mlx5_hlist *list,
				   uint64_t key, void *ctx);

/* Hash list bucket head. */
struct mlx5_hlist_bucket {
	struct mlx5_hlist_head head; /* List head. */
	rte_rwlock_t lock; /* Bucket lock. */
	uint32_t gen_cnt; /* List modification will update generation count. */
} __rte_cache_aligned;

/**
 * Hash list table structure
 *
 * Entry in hash list could be reused if entry already exists, reference
 * count will increase and the existing entry returns.
 *
 * When destroy an entry from list, decrease reference count and only
 * destroy when no further reference.
 */
struct mlx5_hlist {
	char name[MLX5_HLIST_NAMESIZE]; /**< Name of the hash list. */
	/**< number of heads, need to be power of 2. */
	uint32_t table_sz;
	uint32_t entry_sz; /**< Size of entry, used to allocate entry. */
	/**< mask to get the index of the list heads. */
	uint32_t mask;
	bool direct_key; /* Use the new entry key directly as hash index. */
	bool write_most; /* List mostly used for append new or destroy. */
	void *ctx;
	mlx5_hlist_create_cb cb_create; /**< entry create callback. */
	mlx5_hlist_match_cb cb_match; /**< entry match callback. */
	mlx5_hlist_remove_cb cb_remove; /**< entry remove callback. */
	struct mlx5_hlist_bucket buckets[] __rte_cache_aligned;
	/**< list bucket arrays. */
};

/**
 * Create a hash list table, the user can specify the list heads array size
 * of the table, now the size should be a power of 2 in order to get better
 * distribution for the entries. Each entry is a part of the whole data element
 * and the caller should be responsible for the data element's allocation and
 * cleanup / free. Key of each entry will be calculated with CRC in order to
 * generate a little fairer distribution.
 *
 * @param name
 *   Name of the hash list(optional).
 * @param size
 *   Heads array size of the hash list.
 * @param entry_size
 *   Entry size to allocate if cb_create not specified.
 * @param flags
 *   The hash list attribute flags.
 * @param cb_create
 *   Callback function for entry create.
 * @param cb_match
 *   Callback function for entry match.
 * @param cb_destroy
 *   Callback function for entry destroy.
 * @return
 *   Pointer of the hash list table created, NULL on failure.
 */
__rte_internal
struct mlx5_hlist *mlx5_hlist_create(const char *name, uint32_t size,
				     uint32_t entry_size, uint32_t flags,
				     mlx5_hlist_create_cb cb_create,
				     mlx5_hlist_match_cb cb_match,
				     mlx5_hlist_remove_cb cb_destroy);

/**
 * Search an entry matching the key.
 *
 * Result returned might be destroyed by other thread, must use
 * this function only in main thread.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param key
 *   Key for the searching entry.
 * @param ctx
 *   Common context parameter used by entry callback function.
 *
 * @return
 *   Pointer of the hlist entry if found, NULL otherwise.
 */
__rte_internal
struct mlx5_hlist_entry *mlx5_hlist_lookup(struct mlx5_hlist *h, uint64_t key,
					   void *ctx);

/**
 * Insert an entry to the hash list table, the entry is only part of whole data
 * element and a 64B key is used for matching. User should construct the key or
 * give a calculated hash signature and guarantee there is no collision.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param entry
 *   Entry to be inserted into the hash list table.
 * @param ctx
 *   Common context parameter used by callback function.
 *
 * @return
 *   registered entry on success, NULL otherwise
 */
__rte_internal
struct mlx5_hlist_entry *mlx5_hlist_register(struct mlx5_hlist *h, uint64_t key,
					     void *ctx);

/**
 * Remove an entry from the hash list table. User should guarantee the validity
 * of the entry.
 *
 * @param h
 *   Pointer to the hast list table. (not used)
 * @param entry
 *   Entry to be removed from the hash list table.
 * @return
 *   0 on entry removed, 1 on entry still referenced.
 */
__rte_internal
int mlx5_hlist_unregister(struct mlx5_hlist *h, struct mlx5_hlist_entry *entry);

/**
 * Destroy the hash list table, all the entries already inserted into the lists
 * will be handled by the callback function provided by the user (including
 * free if needed) before the table is freed.
 *
 * @param h
 *   Pointer to the hast list table.
 */
__rte_internal
void mlx5_hlist_destroy(struct mlx5_hlist *h);

#endif /* RTE_PMD_MLX5_COMMON_UTILS_H_ */
