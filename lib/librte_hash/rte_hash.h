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

/** Signature of key that is stored internally. */
typedef uint32_t hash_sig_t;

/** Type of function that can be used for calculating the hash value. */
typedef uint32_t (*rte_hash_function)(const void *key, uint32_t key_len,
				      uint32_t init_val);

/** Type of function used to compare the hash key. */
typedef int (*rte_hash_cmp_eq_t)(const void *key1, const void *key2, size_t key_len);

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
 * @param h
 *   Hash table to free
 */
void
rte_hash_free(struct rte_hash *h);

/**
 * Reset all hash structure, by zeroing all entries
 * @param h
 *   Hash table to reset
 */
void
rte_hash_reset(struct rte_hash *h);

/**
 * Add a key-value pair to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread.
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
 * and should only be called from one thread.
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
 * and should only be called from one thread.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key.
 */
int32_t
rte_hash_add_key(const struct rte_hash *h, const void *key);

/**
 * Add a key to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread.
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
 *     array of user data. This value is unique for this key.
 */
int32_t
rte_hash_add_key_with_hash(const struct rte_hash *h, const void *key, hash_sig_t sig);

/**
 * Remove a key from an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread.
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
 * and should only be called from one thread.
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
 * This operation is multi-thread safe.
 *
 * @param h
 *   Hash table to get the key from.
 * @param position
 *   Position returned when the key was inserted.
 * @param key
 *   Output containing a pointer to the key
 * @return
 *   - 0 if retrieved successfully
 *   - EINVAL if the parameters are invalid.
 *   - ENOENT if no valid key is found in the given position.
 */
int
rte_hash_get_key_with_position(const struct rte_hash *h, const int32_t position,
			       void **key);

/**
 * Find a key-value pair in the hash table.
 * This operation is multi-thread safe.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param data
 *   Output with pointer to data returned from the hash table.
 * @return
 *   0 if successful lookup
 *   - EINVAL if the parameters are invalid.
 *   - ENOENT if the key is not found.
 */
int
rte_hash_lookup_data(const struct rte_hash *h, const void *key, void **data);

/**
 * Find a key-value pair with a pre-computed hash value
 * to an existing hash table.
 * This operation is multi-thread safe.
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
 *   0 if successful lookup
 *   - EINVAL if the parameters are invalid.
 *   - ENOENT if the key is not found.
 */
int
rte_hash_lookup_with_hash_data(const struct rte_hash *h, const void *key,
					hash_sig_t sig, void **data);

/**
 * Find a key in the hash table.
 * This operation is multi-thread safe.
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
 * This operation is multi-thread safe.
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
 * This operation is not multi-thread safe.
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
 * This operation is multi-thread safe.
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
 * Find multiple keys in the hash table.
 * This operation is multi-thread safe.
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
#ifdef __cplusplus
}
#endif

#endif /* _RTE_HASH_H_ */
