/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_HASH_H_
#define _RTE_HASH_H_

/**
 * @file
 *
 * RTE Hash Table
 */

#include <stdint.h>
#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of hash table that can be created. */
#define RTE_HASH_ENTRIES_MAX			(1 << 26)

/** Maximum bucket size that can be created. */
#define RTE_HASH_BUCKET_ENTRIES_MAX		16

/** Maximum length of key that can be used. */
#define RTE_HASH_KEY_LENGTH_MAX			64

/** Max number of keys that can be searched for using rte_hash_lookup_multi. */
#define RTE_HASH_LOOKUP_BULK_MAX		16
#define RTE_HASH_LOOKUP_MULTI_MAX		RTE_HASH_LOOKUP_BULK_MAX

/** Max number of characters in hash name.*/
#define RTE_HASH_NAMESIZE			32

/** Signature of key that is stored internally. */
typedef uint32_t hash_sig_t;

/** Type of function that can be used for calculating the hash value. */
typedef uint32_t (*rte_hash_function)(const void *key, uint32_t key_len,
				      uint32_t init_val);

/**
 * Parameters used when creating the hash table. The total table entries and
 * bucket entries must be a power of 2.
 */
struct rte_hash_parameters {
	const char *name;		/**< Name of the hash. */
	uint32_t entries;		/**< Total hash table entries. */
	uint32_t bucket_entries;	/**< Bucket entries. */
	uint32_t key_len;		/**< Length of hash key. */
	rte_hash_function hash_func;	/**< Function used to calculate hash. */
	uint32_t hash_func_init_val;	/**< Init value used by hash_func. */
	int socket_id;			/**< NUMA Socket ID for memory. */
};

/** A hash table structure. */
struct rte_hash {
	TAILQ_ENTRY(rte_hash) next;/**< Next in list. */

	char name[RTE_HASH_NAMESIZE];	/**< Name of the hash. */
	uint32_t entries;		/**< Total table entries. */
	uint32_t bucket_entries;	/**< Bucket entries. */
	uint32_t key_len;		/**< Length of hash key. */
	rte_hash_function hash_func;	/**< Function used to calculate hash. */
	uint32_t hash_func_init_val;	/**< Init value used by hash_func. */
	uint32_t num_buckets;		/**< Number of buckets in table. */
	uint32_t bucket_bitmask;	/**< Bitmask for getting bucket index
							from hash signature. */
	hash_sig_t sig_msb;	/**< MSB is always set in valid signatures. */
	uint8_t *sig_tbl;	/**< Flat array of hash signature buckets. */
	uint32_t sig_tbl_bucket_size;	/**< Signature buckets may be padded for
					   alignment reasons, and this is the
					   bucket size used by sig_tbl. */
	uint8_t *key_tbl;	/**< Flat array of key value buckets. */
	uint32_t key_tbl_key_size;	/**< Keys may be padded for alignment
					   reasons, and this is the key size
					   used	by key_tbl. */
};

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
 *    - E_RTE_NO_TAILQ - no tailq list could be got for the hash table list
 *    - ENOENT - missing entry
 *    - EINVAL - invalid parameter passed to function
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_hash *
rte_hash_create(const struct rte_hash_parameters *params);


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
 * Add a key to an existing hash table. This operation is not multi-thread safe
 * and should only be called from one thread.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @param sig
 *   Hash value to add to the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key.
 */
int32_t
rte_hash_add_key_with_hash(const struct rte_hash *h,
				const void *key, hash_sig_t sig);

/**
 * Remove a key from an existing hash table. This operation is not multi-thread
 * safe and should only be called from one thread.
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
 * Remove a key from an existing hash table. This operation is not multi-thread
 * safe and should only be called from one thread.
 *
 * @param h
 *   Hash table to remove the key from.
 * @param key
 *   Key to remove from the hash table.
 * @param sig
 *   Hash value to remove from the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t
rte_hash_del_key_with_hash(const struct rte_hash *h,
				const void *key, hash_sig_t sig);


/**
 * Find a key in the hash table. This operation is multi-thread safe.
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
 * Find a key in the hash table. This operation is multi-thread safe.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param sig
 *   Hash value to find.
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
 * Calc a hash value by key. This operation is not multi-process safe.
 *
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @return
 *   - hash value
 */
static inline hash_sig_t
rte_hash_hash(const struct rte_hash *h, const void *key)
{
	/* calc hash result by key */
	return h->hash_func(key, h->key_len, h->hash_func_init_val);
}

#define rte_hash_lookup_multi rte_hash_lookup_bulk
/**
 * Find multiple keys in the hash table. This operation is multi-thread safe.
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
#ifdef __cplusplus
}
#endif

#endif /* _RTE_HASH_H_ */
