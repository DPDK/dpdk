/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _RTE_HASH_H_
#define _RTE_HASH_H_

/**
 * @file
 *
 * RTE Hash Table
 */

#include <stdint.h>
#include <stddef.h>

#include <rte_compat.h>
#include <rte_rcu_qsbr.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of hash table that can be created. */
#define RTE_HASH_ENTRIES_MAX			(1 << 30)

/** Maximum number of characters in hash name.*/
#define RTE_HASH_NAMESIZE			32

/** Maximum number of keys that can be searched for using rte_hash_lookup_bulk. */
#define RTE_HASH_LOOKUP_BULK_MAX		64
#define RTE_HASH_LOOKUP_MULTI_MAX		RTE_HASH_LOOKUP_BULK_MAX

/** Enable Hardware transactional memory support. */
#define RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT	0x01

/** Default behavior of insertion, single writer/multi writer */
#define RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD 0x02

/** Flag to support reader writer concurrency */
#define RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY 0x04

/** Flag to indicate the extendable bucket table feature should be used */
#define RTE_HASH_EXTRA_FLAGS_EXT_TABLE 0x08

/** Flag to disable freeing of key index on hash delete.
 * Refer to rte_hash_del_xxx APIs for more details.
 * This is enabled by default when RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF
 * is enabled. However, if internal RCU is enabled, freeing of internal
 * memory/index is done on delete
 */
#define RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL 0x10

/** Flag to support lock free reader writer concurrency. Both single writer
 * and multi writer use cases are supported.
 */
#define RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF 0x20

/**
 * The type of hash value of a key.
 * It should be a value of at least 32bit with fully random pattern.
 */
typedef uint32_t hash_sig_t;

/** Type of function that can be used for calculating the hash value. */
typedef uint32_t (*rte_hash_function)(const void *key, uint32_t key_len,
				      uint32_t init_val);

/** Type of function used to compare the hash key. */
typedef int (*rte_hash_cmp_eq_t)(const void *key1, const void *key2, size_t key_len);

/**
 * Type of function used to free data stored in the key.
 * Required when using internal RCU to allow application to free key-data once
 * the key is returned to the ring of free key-slots.
 */
typedef void (*rte_hash_free_key_data)(void *p, void *key_data);

/**
 * Parameters used when creating the hash table.
 */
struct rte_hash_parameters {
	const char *name;		/**< Name of the hash. */
	uint32_t entries;		/**< Total hash table entries. */
	uint32_t reserved;		/**< Unused field. Should be set to 0 */
	uint32_t key_len;		/**< Length of hash key. */
	rte_hash_function hash_func;	/**< Primary Hash function used to calculate hash. */
	uint32_t hash_func_init_val;	/**< Init value used by hash_func. */
	int socket_id;			/**< NUMA Socket ID for memory. */
	uint8_t extra_flag;		/**< Indicate if additional parameters are present. */
};

/** RCU reclamation modes */
enum rte_hash_qsbr_mode {
	/** Create defer queue for reclaim. */
	RTE_HASH_QSBR_MODE_DQ = 0,
	/** Use blocking mode reclaim. No defer queue created. */
	RTE_HASH_QSBR_MODE_SYNC
};

/** HASH RCU QSBR configuration structure. */
struct rte_hash_rcu_config {
	struct rte_rcu_qsbr *v;		/**< RCU QSBR variable. */
	enum rte_hash_qsbr_mode mode;
	/**< Mode of RCU QSBR. RTE_HASH_QSBR_MODE_xxx
	 * '0' for default: create defer queue for reclaim.
	 */
	uint32_t dq_size;
	/**< RCU defer queue size.
	 * default: total hash table entries.
	 */
	uint32_t trigger_reclaim_limit;	/**< Threshold to trigger auto reclaim. */
	uint32_t max_reclaim_size;
	/**< Max entries to reclaim in one go.
	 * default: RTE_HASH_RCU_DQ_RECLAIM_MAX.
	 */
	void *key_data_ptr;
	/**< Pointer passed to the free function. Typically, this is the
	 * pointer to the data structure to which the resource to free
	 * (key-data) belongs. This can be NULL.
	 */
	rte_hash_free_key_data free_key_data_func;
	/**< Function to call to free the resource (key-data). */
};

/** @internal A hash table structure. */
struct rte_hash;

/**
 * Create a new hash table.
 *
 * @param params
 *   Parameters used to create and initialise the hash table.
 * @return
 *   Pointer to hash table structure that is used in future hash table
 *   operations, or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - ENOENT - missing entry
 *    - EINVAL - invalid parameter passed to function
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_hash *
rte_hash_create(const struct rte_hash_parameters *params);

/**
 * Set a new hash compare function other than the default one.
 *
 * @note Function pointer does not work with multi-process, so do not use it
 * in multi-process mode.
 *
 * @param h
 *   Hash table for which the function is to be changed
 * @param func
 *   New compare function
 */
void rte_hash_set_cmp_func(struct rte_hash *h, rte_hash_cmp_eq_t func);

/**
 * Find an existing hash table object and return a pointer to it.
 *
 * @param name
 *   Name of the hash table as passed to rte_hash_create()
 * @return
 *   Pointer to hash table or NULL if object not found
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - ENOENT - value not available for return
 */
struct rte_hash *
rte_hash_find_existing(const char *name);

/**
 * De-allocate all memory used by hash table.
 *
 * @param h
 *   Hash table to free, if NULL, the function does nothing.
 *
 */
void
rte_hash_free(struct rte_hash *h);

/**
 * Reset all hash structure, by zeroing all entries.
 * When RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF is enabled,
 * it is application's responsibility to make sure that
 * none of the readers are referencing the hash table
 * while calling this API.
 *
 * @param h
 *   Hash table to reset
 */
void
rte_hash_reset(struct rte_hash *h);

/**
 * Return the number of keys in the hash table
 * @param h
 *  Hash table to query from
 * @return
 *   - -EINVAL if parameters are invalid
 *   - A value indicating how many keys were inserted in the table.
 */
int32_t
rte_hash_count(const struct rte_hash *h);

/**
 * Return the maximum key value ID that could possibly be returned by
 * rte_hash_add_key function.
 *
 * @param h
 *  Hash table to query from
 * @return
 *   - -EINVAL if parameters are invalid
 *   - A value indicating the max key ID of key slots present in the table.
 */
int32_t
rte_hash_max_key_id(const struct rte_hash *h);

/**
 * Add a key-value pair to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * Thread safety can be enabled by setting flag during
 * table creation.
 * If the key exists already in the table, this API updates its value
 * with 'data' passed in this API. It is the responsibility of
 * the application to manage any memory associated with the old value.
 * The readers might still be using the old value even after this API
 * has returned.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @param data
 *   Data to add to the hash table.
 * @return
 *   - 0 if added successfully
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 */
int
rte_hash_add_key_data(const struct rte_hash *h, const void *key, void *data);

/**
 * Add a key-value pair with a pre-computed hash value
 * to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * Thread safety can be enabled by setting flag during
 * table creation.
 * If the key exists already in the table, this API updates its value
 * with 'data' passed in this API. It is the responsibility of
 * the application to manage any memory associated with the old value.
 * The readers might still be using the old value even after this API
 * has returned.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @param sig
 *   Precomputed hash value for 'key'
 * @param data
 *   Data to add to the hash table.
 * @return
 *   - 0 if added successfully
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 */
int32_t
rte_hash_add_key_with_hash_data(const struct rte_hash *h, const void *key,
						hash_sig_t sig, void *data);

/**
 * Add a key to an existing hash table. This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * Thread safety can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key. This
 *     unique key id may be larger than the user specified entry count
 *     when RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD flag is set.
 */
int32_t
rte_hash_add_key(const struct rte_hash *h, const void *key);

/**
 * Add a key to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * Thread safety can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @param sig
 *   Precomputed hash value for 'key'.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key. This
 *     unique key ID may be larger than the user specified entry count
 *     when RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD flag is set.
 */
int32_t
rte_hash_add_key_with_hash(const struct rte_hash *h, const void *key, hash_sig_t sig);

/**
 * Remove a key from an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * Thread safety can be enabled by setting flag during
 * table creation.
 * If RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL or
 * RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF is enabled and
 * internal RCU is NOT enabled,
 * the key index returned by rte_hash_add_key_xxx APIs will not be
 * freed by this API. rte_hash_free_key_with_position API must be called
 * additionally to free the index associated with the key.
 * rte_hash_free_key_with_position API should be called after all
 * the readers have stopped referencing the entry corresponding to
 * this key. RCU mechanisms could be used to determine such a state.
 *
 * @param h
 *   Hash table to remove the key from.
 * @param key
 *   Key to remove from the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t
rte_hash_del_key(const struct rte_hash *h, const void *key);

/**
 * Remove a key from an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread by default.
 * Thread safety can be enabled by setting flag during
 * table creation.
 * If RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL or
 * RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF is enabled and
 * internal RCU is NOT enabled,
 * the key index returned by rte_hash_add_key_xxx APIs will not be
 * freed by this API. rte_hash_free_key_with_position API must be called
 * additionally to free the index associated with the key.
 * rte_hash_free_key_with_position API should be called after all
 * the readers have stopped referencing the entry corresponding to
 * this key. RCU mechanisms could be used to determine such a state.
 *
 * @param h
 *   Hash table to remove the key from.
 * @param key
 *   Key to remove from the hash table.
 * @param sig
 *   Precomputed hash value for 'key'.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t
rte_hash_del_key_with_hash(const struct rte_hash *h, const void *key, hash_sig_t sig);

/**
 * Find a key in the hash table given the position.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to get the key from.
 * @param position
 *   Position returned when the key was inserted.
 * @param key
 *   Output containing a pointer to the key
 * @return
 *   - 0 if retrieved successfully
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if no valid key is found in the given position.
 */
int
rte_hash_get_key_with_position(const struct rte_hash *h, const int32_t position,
			       void **key);

/**
 * Free a hash key in the hash table given the position
 * of the key. This operation is not multi-thread safe and should
 * only be called from one thread by default. Thread safety
 * can be enabled by setting flag during table creation.
 * If RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL or
 * RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF is enabled and
 * internal RCU is NOT enabled,
 * the key index returned by rte_hash_del_key_xxx APIs must be freed
 * using this API. This API should be called after all the readers
 * have stopped referencing the entry corresponding to this key.
 * RCU mechanisms could be used to determine such a state.
 * This API does not validate if the key is already freed.
 *
 * @param h
 *   Hash table to free the key from.
 * @param position
 *   Position returned when the key was deleted.
 * @return
 *   - 0 if freed successfully
 *   - -EINVAL if the parameters are invalid.
 */
int
rte_hash_free_key_with_position(const struct rte_hash *h,
				const int32_t position);

/**
 * Find a key-value pair in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param data
 *   Output with pointer to data returned from the hash table.
 * @return
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 */
int
rte_hash_lookup_data(const struct rte_hash *h, const void *key, void **data);

/**
 * Find a key-value pair with a pre-computed hash value
 * to an existing hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param sig
 *   Precomputed hash value for 'key'
 * @param data
 *   Output with pointer to data returned from the hash table.
 * @return
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 */
int
rte_hash_lookup_with_hash_data(const struct rte_hash *h, const void *key,
					hash_sig_t sig, void **data);

/**
 * Find a key in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t
rte_hash_lookup(const struct rte_hash *h, const void *key);

/**
 * Find a key in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param sig
 *   Precomputed hash value for 'key'.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t
rte_hash_lookup_with_hash(const struct rte_hash *h,
				const void *key, hash_sig_t sig);

/**
 * Calc a hash value by key.
 * This operation is not multi-process safe.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @return
 *   - hash value
 */
hash_sig_t
rte_hash_hash(const struct rte_hash *h, const void *key);

/**
 * Find multiple keys in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param keys
 *   A pointer to a list of keys to look for.
 * @param num_keys
 *   How many keys are in the keys list (less than RTE_HASH_LOOKUP_BULK_MAX).
 * @param hit_mask
 *   Output containing a bitmask with all successful lookups.
 * @param data
 *   Output containing array of data returned from all the successful lookups.
 * @return
 *   -EINVAL if there's an error, otherwise number of successful lookups.
 */
int
rte_hash_lookup_bulk_data(const struct rte_hash *h, const void **keys,
		      uint32_t num_keys, uint64_t *hit_mask, void *data[]);

/**
 * Find multiple keys in the hash table with precomputed hash value array.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param keys
 *   A pointer to a list of keys to look for.
 * @param sig
 *   A pointer to a list of precomputed hash values for keys.
 * @param num_keys
 *   How many keys are in the keys list (less than RTE_HASH_LOOKUP_BULK_MAX).
 * @param positions
 *   Output containing a list of values, corresponding to the list of keys that
 *   can be used by the caller as an offset into an array of user data. These
 *   values are unique for each key, and are the same values that were returned
 *   when each key was added. If a key in the list was not found, then -ENOENT
 *   will be the value.
 * @return
 *   -EINVAL if there's an error, otherwise 0.
 */
int
rte_hash_lookup_with_hash_bulk(const struct rte_hash *h, const void **keys,
		hash_sig_t *sig, uint32_t num_keys, int32_t *positions);

/**
 * Find multiple keys in the hash table with precomputed hash value array.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param keys
 *   A pointer to a list of keys to look for.
 * @param sig
 *   A pointer to a list of precomputed hash values for keys.
 * @param num_keys
 *   How many keys are in the keys list (less than RTE_HASH_LOOKUP_BULK_MAX).
 * @param hit_mask
 *   Output containing a bitmask with all successful lookups.
 * @param data
 *   Output containing array of data returned from all the successful lookups.
 * @return
 *   -EINVAL if there's an error, otherwise number of successful lookups.
 */
int
rte_hash_lookup_with_hash_bulk_data(const struct rte_hash *h,
		const void **keys, hash_sig_t *sig,
		uint32_t num_keys, uint64_t *hit_mask, void *data[]);

/**
 * Find multiple keys in the hash table.
 * This operation is multi-thread safe with regarding to other lookup threads.
 * Read-write concurrency can be enabled by setting flag during
 * table creation.
 *
 * @param h
 *   Hash table to look in.
 * @param keys
 *   A pointer to a list of keys to look for.
 * @param num_keys
 *   How many keys are in the keys list (less than RTE_HASH_LOOKUP_BULK_MAX).
 * @param positions
 *   Output containing a list of values, corresponding to the list of keys that
 *   can be used by the caller as an offset into an array of user data. These
 *   values are unique for each key, and are the same values that were returned
 *   when each key was added. If a key in the list was not found, then -ENOENT
 *   will be the value.
 * @return
 *   -EINVAL if there's an error, otherwise 0.
 */
int
rte_hash_lookup_bulk(const struct rte_hash *h, const void **keys,
		      uint32_t num_keys, int32_t *positions);

/**
 * Iterate through the hash table, returning key-value pairs.
 *
 * @param h
 *   Hash table to iterate
 * @param key
 *   Output containing the key where current iterator
 *   was pointing at
 * @param data
 *   Output containing the data associated with key.
 *   Returns NULL if data was not stored.
 * @param next
 *   Pointer to iterator. Should be 0 to start iterating the hash table.
 *   Iterator is incremented after each call of this function.
 * @return
 *   Position where key was stored, if successful.
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if end of the hash table.
 */
int32_t
rte_hash_iterate(const struct rte_hash *h, const void **key, void **data, uint32_t *next);

/**
 * Associate RCU QSBR variable with a Hash object.
 * This API should be called to enable the integrated RCU QSBR support and
 * should be called immediately after creating the Hash object.
 *
 * @param h
 *   the hash object to add RCU QSBR
 * @param cfg
 *   RCU QSBR configuration
 * @return
 *   On success - 0
 *   On error - 1 with error code set in rte_errno.
 *   Possible rte_errno codes are:
 *   - EINVAL - invalid pointer
 *   - EEXIST - already added QSBR
 *   - ENOMEM - memory allocation failure
 */
int rte_hash_rcu_qsbr_add(struct rte_hash *h, struct rte_hash_rcu_config *cfg);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_HASH_H_ */
