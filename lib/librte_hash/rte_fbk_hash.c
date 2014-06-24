/**
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

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <sys/queue.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_log.h>
#include <rte_spinlock.h>

#include "rte_fbk_hash.h"

TAILQ_HEAD(rte_fbk_hash_list, rte_fbk_hash_table);

/**
 * Performs a lookup for an existing hash table, and returns a pointer to
 * the table if found.
 *
 * @param name
 *   Name of the hash table to find
 *
 * @return
 *   pointer to hash table structure or NULL on error.
 */
struct rte_fbk_hash_table *
rte_fbk_hash_find_existing(const char *name)
{
	struct rte_fbk_hash_table *h;
	struct rte_fbk_hash_list *fbk_hash_list;

	/* check that we have an initialised tail queue */
	if ((fbk_hash_list =
	     RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_FBK_HASH, rte_fbk_hash_list)) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);
	TAILQ_FOREACH(h, fbk_hash_list, next) {
		if (strncmp(name, h->name, RTE_FBK_HASH_NAMESIZE) == 0)
			break;
	}
	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);
	if (h == NULL)
		rte_errno = ENOENT;
	return h;
}

/**
 * Create a new hash table for use with four byte keys.
 *
 * @param params
 *   Parameters used in creation of hash table.
 *
 * @return
 *   Pointer to hash table structure that is used in future hash table
 *   operations, or NULL on error.
 */
struct rte_fbk_hash_table *
rte_fbk_hash_create(const struct rte_fbk_hash_params *params)
{
	struct rte_fbk_hash_table *ht = NULL;
	char hash_name[RTE_FBK_HASH_NAMESIZE];
	const uint32_t mem_size =
			sizeof(*ht) + (sizeof(ht->t[0]) * params->entries);
	uint32_t i;
	struct rte_fbk_hash_list *fbk_hash_list;

	/* check that we have an initialised tail queue */
	if ((fbk_hash_list =
	     RTE_TAILQ_LOOKUP_BY_IDX(RTE_TAILQ_FBK_HASH, rte_fbk_hash_list)) == NULL) {
		rte_errno = E_RTE_NO_TAILQ;
		return NULL;
	}

	/* Error checking of parameters. */
	if ((!rte_is_power_of_2(params->entries)) ||
			(!rte_is_power_of_2(params->entries_per_bucket)) ||
			(params->entries == 0) ||
			(params->entries_per_bucket == 0) ||
			(params->entries_per_bucket > params->entries) ||
			(params->entries > RTE_FBK_HASH_ENTRIES_MAX) ||
			(params->entries_per_bucket > RTE_FBK_HASH_ENTRIES_PER_BUCKET_MAX)){
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(hash_name, sizeof(hash_name), "FBK_%s", params->name);

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* guarantee there's no existing */
	TAILQ_FOREACH(ht, fbk_hash_list, next) {
		if (strncmp(params->name, ht->name, RTE_FBK_HASH_NAMESIZE) == 0)
			break;
	}
	if (ht != NULL)
		goto exit;

	/* Allocate memory for table. */
	ht = (struct rte_fbk_hash_table *)rte_malloc_socket(hash_name, mem_size,
			0, params->socket_id);
	if (ht == NULL)
		goto exit;

	memset(ht, 0, mem_size);

	/* Set up hash table context. */
	snprintf(ht->name, sizeof(ht->name), "%s", params->name);
	ht->entries = params->entries;
	ht->entries_per_bucket = params->entries_per_bucket;
	ht->used_entries = 0;
	ht->bucket_mask = (params->entries / params->entries_per_bucket) - 1;
	for (ht->bucket_shift = 0, i = 1;
	    (params->entries_per_bucket & i) == 0;
	    ht->bucket_shift++, i <<= 1)
		; /* empty loop body */

	if (params->hash_func != NULL) {
		ht->hash_func = params->hash_func;
		ht->init_val = params->init_val;
	}
	else {
		ht->hash_func = RTE_FBK_HASH_FUNC_DEFAULT;
		ht->init_val = RTE_FBK_HASH_INIT_VAL_DEFAULT;
	}

	TAILQ_INSERT_TAIL(fbk_hash_list, ht, next);

exit:
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	return ht;
}

/**
 * Free all memory used by a hash table.
 *
 * @param ht
 *   Hash table to deallocate.
 */
void
rte_fbk_hash_free(struct rte_fbk_hash_table *ht)
{
	if (ht == NULL)
		return;

	RTE_EAL_TAILQ_REMOVE(RTE_TAILQ_FBK_HASH, rte_fbk_hash_list, ht);
	rte_free(ht);
}

