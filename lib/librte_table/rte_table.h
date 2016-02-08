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

#ifndef __INCLUDE_RTE_TABLE_H__
#define __INCLUDE_RTE_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table
 *
 * This tool is part of the DPDK Packet Framework tool suite and provides
 * a standard interface to implement different types of lookup tables for data
 * plane processing.
 *
 * Virtually any search algorithm that can uniquely associate data to a lookup
 * key can be fitted under this lookup table abstraction. For the flow table
 * use-case, the lookup key is an n-tuple of packet fields that uniquely
 * identifies a traffic flow, while data represents actions and action
 * meta-data associated with the same traffic flow.
 *
 ***/

#include <stdint.h>
#include <rte_port.h>

struct rte_mbuf;

/** Lookup table statistics */
struct rte_table_stats {
	uint64_t n_pkts_in;
	uint64_t n_pkts_lookup_miss;
};

/**
 * Lookup table create
 *
 * @param params
 *   Parameters for lookup table creation. The underlying data structure is
 *   different for each lookup table type.
 * @param socket_id
 *   CPU socket ID (e.g. for memory allocation purpose)
 * @param entry_size
 *   Data size of each lookup table entry (measured in bytes)
 * @return
 *   Handle to lookup table instance
 */
typedef void* (*rte_table_op_create)(void *params, int socket_id,
	uint32_t entry_size);

/**
 * Lookup table free
 *
 * @param table
 *   Handle to lookup table instance
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_table_op_free)(void *table);

/**
 * Lookup table entry add
 *
 * @param table
 *   Handle to lookup table instance
 * @param key
 *   Lookup key
 * @param entry
 *   Data to be associated with the current key. This parameter has to point to
 *   a valid memory buffer where the first entry_size bytes (table create
 *   parameter) are populated with the data.
 * @param key_found
 *   After successful invocation, *key_found is set to a value different than 0
 *   if the current key is already present in the table and to 0 if not. This
 *   pointer has to be set to a valid memory location before the table entry add
 *   function is called.
 * @param entry_ptr
 *   After successful invocation, *entry_ptr stores the handle to the table
 *   entry containing the data associated with the current key. This handle can
 *   be used to perform further read-write accesses to this entry. This handle
 *   is valid until the key is deleted from the table or the same key is
 *   re-added to the table, typically to associate it with different data. This
 *   pointer has to be set to a valid memory location before the function is
 *   called.
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_table_op_entry_add)(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr);

/**
 * Lookup table entry delete
 *
 * @param table
 *   Handle to lookup table instance
 * @param key
 *   Lookup key
 * @param key_found
 *   After successful invocation, *key_found is set to a value different than 0
 *   if the current key was present in the table before the delete operation
 *   was performed and to 0 if not. This pointer has to be set to a valid
 *   memory location before the table entry delete function is called.
 * @param entry
 *   After successful invocation, if the key is found in the table (*key found
 *   is different than 0 after function call is completed) and entry points to
 *   a valid buffer (entry is set to a value different than NULL before the
 *   function is called), then the first entry_size bytes (table create
 *   parameter) in *entry store a copy of table entry that contained the data
 *   associated with the current key before the key was deleted.
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_table_op_entry_delete)(
	void *table,
	void *key,
	int *key_found,
	void *entry);

/**
 * Lookup table entry add bulk
 *
 * @param table
 *   Handle to lookup table instance
 * @param key
 *   Array containing lookup keys
 * @param entries
 *   Array containing data to be associated with each key. Every item in the
 *   array has to point to a valid memory buffer where the first entry_size
 *   bytes (table create parameter) are populated with the data.
 * @param n_keys
 *   Number of keys to add
 * @param key_found
 *   After successful invocation, key_found for every item in the array is set
 *   to a value different than 0 if the current key is already present in the
 *   table and to 0 if not. This pointer has to be set to a valid memory
 *   location before the table entry add function is called.
 * @param entries_ptr
 *   After successful invocation, array *entries_ptr stores the handle to the
 *   table entry containing the data associated with every key. This handle can
 *   be used to perform further read-write accesses to this entry. This handle
 *   is valid until the key is deleted from the table or the same key is
 *   re-added to the table, typically to associate it with different data. This
 *   pointer has to be set to a valid memory location before the function is
 *   called.
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_table_op_entry_add_bulk)(
	void *table,
	void **keys,
	void **entries,
	uint32_t n_keys,
	int *key_found,
	void **entries_ptr);

/**
 * Lookup table entry delete bulk
 *
 * @param table
 *   Handle to lookup table instance
 * @param key
 *   Array containing lookup keys
 * @param n_keys
 *   Number of keys to delete
 * @param key_found
 *   After successful invocation, key_found for every item in the array is set
 *   to a value different than 0if the current key was present in the table
 *   before the delete operation was performed and to 0 if not. This pointer
 *   has to be set to a valid memory location before the table entry delete
 *   function is called.
 * @param entries
 *   If entries pointer is NULL, this pointer is ignored for every entry found.
 *   Else, after successful invocation, if specific key is found in the table
 *   (key_found is different than 0 for this item after function call is
 *   completed) and item of entry array points to a valid buffer (entry is set
 *   to a value different than NULL before the function is called), then the
 *   first entry_size bytes (table create parameter) in *entry store a copy of
 *   table entry that contained the data associated with the current key before
 *   the key was deleted.
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_table_op_entry_delete_bulk)(
	void *table,
	void **keys,
	uint32_t n_keys,
	int *key_found,
	void **entries);

/**
 * Lookup table lookup
 *
 * @param table
 *   Handle to lookup table instance
 * @param pkts
 *   Burst of input packets specified as array of up to 64 pointers to struct
 *   rte_mbuf
 * @param pkts_mask
 *   64-bit bitmask specifying which packets in the input burst are valid. When
 *   pkts_mask bit n is set, then element n of pkts array is pointing to a
 *   valid packet. Otherwise, element n of pkts array does not point to a valid
 *   packet, therefore it will not be accessed.
 * @param lookup_hit_mask
 *   Once the table lookup operation is completed, this 64-bit bitmask
 *   specifies which of the valid packets in the input burst resulted in lookup
 *   hit. For each valid input packet (pkts_mask bit n is set), the following
 *   are true on lookup hit: lookup_hit_mask bit n is set, element n of entries
 *   array is valid and it points to the lookup table entry that was hit. For
 *   each valid input packet (pkts_mask bit n is set), the following are true
 *   on lookup miss: lookup_hit_mask bit n is not set and element n of entries
 *   array is not valid.
 * @param entries
 *   Once the table lookup operation is completed, this array provides the
 *   lookup table entries that were hit, as described above. It is required
 *   that this array is always pre-allocated by the caller of this function
 *   with exactly 64 elements. The implementation is allowed to speculatively
 *   modify the elements of this array, so elements marked as invalid in
 *   lookup_hit_mask once the table lookup operation is completed might have
 *   been modified by this function.
 * @return
 *   0 on success, error code otherwise
 */
typedef int (*rte_table_op_lookup)(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries);

/**
 * Lookup table stats read
 *
 * @param table
 *   Handle to lookup table instance
 * @param stats
 *   Handle to table stats struct to copy data
 * @param clear
 *   Flag indicating that stats should be cleared after read
 *
 * @return
 *   Error code or 0 on success.
 */
typedef int (*rte_table_op_stats_read)(
	void *table,
	struct rte_table_stats *stats,
	int clear);

/** Lookup table interface defining the lookup table operation */
struct rte_table_ops {
	rte_table_op_create f_create;                 /**< Create */
	rte_table_op_free f_free;                     /**< Free */
	rte_table_op_entry_add f_add;                 /**< Entry add */
	rte_table_op_entry_delete f_delete;           /**< Entry delete */
	rte_table_op_entry_add_bulk f_add_bulk;       /**< Add entry bulk */
	rte_table_op_entry_delete_bulk f_delete_bulk; /**< Delete entry bulk */
	rte_table_op_lookup f_lookup;                 /**< Lookup */
	rte_table_op_stats_read f_stats;              /**< Stats */
};

#ifdef __cplusplus
}
#endif

#endif
