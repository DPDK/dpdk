/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <rte_malloc.h>

#include "bnxt.h"
#include "tfc.h"
#include "tfc_cpm.h"

/*
 * Per pool entry
 */
struct cpm_pool_entry {
	bool valid;
	struct tfc_cmm *cmm;
	uint32_t used_count;
	bool all_used;
	struct cpm_pool_use *pool_use;
};

/*
 * Pool use list entry
 */
struct cpm_pool_use {
	uint16_t pool_id;
	struct cpm_pool_use *prev;
	struct cpm_pool_use *next;
};

/*
 * tfc_cpm
 *
 * This is the main CPM data struct
 */
struct tfc_cpm {
	struct cpm_pool_entry *pools;
	uint16_t available_pool_id; /* pool with highest use count, i.e. most used entries */
	bool pool_valid; /* pool has free entries */
	uint32_t pool_size; /* number of entries in each pool */
	uint32_t max_pools; /* maximum number of pools */
	uint32_t next_index; /* search index */
	struct cpm_pool_use *pool_use_list; /* Ordered list of pool usage */
};

#define CPM_DEBUG 0

#if (CPM_DEBUG == 1)
static void show_list(char *str, struct tfc_cpm *cpm)
{
	struct cpm_pool_use *pu = cpm->pool_use_list;

	PMD_DRV_LOG_LINE("%s - ", str);

	while (pu != NULL) {
		PMD_DRV_LOG_LINE("PU(%p) id:%d(u:%d au:%d) p:0x%p n:0x%p",
				 pu,
				 pu->pool_id,
				 cpm->pools[pu->pool_id].used_count,
				 cpm->pools[pu->pool_id].all_used,
				 pu->prev,
				 pu->next);

		pu = pu->next;
	}
}
#endif

static int cpm_insert_pool_id(struct tfc_cpm *cpm, uint16_t pool_id)
{
	struct cpm_pool_entry *pool = &cpm->pools[pool_id];
	struct cpm_pool_use *pool_use = cpm->pool_use_list;
	struct cpm_pool_use *new_pool_use;
	struct cpm_pool_use *prev = NULL;

	if (!pool->valid) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x is invalid", pool_id);
		return -EINVAL;
	}

	/* Find where in insert new entry */
	while (pool_use != NULL) {
		if (cpm->pools[pool_use->pool_id].valid &&
			cpm->pools[pool_use->pool_id].used_count >
			pool->used_count) {
			pool_use = pool_use->next;
			prev =	pool_use;
		} else {
			break;
		}
	}

	/* Alloc new entry */
	new_pool_use = rte_zmalloc("tf", sizeof(struct cpm_pool_use), 0);
	new_pool_use->pool_id = pool_id;
	new_pool_use->prev = NULL;
	new_pool_use->next = NULL;
	pool->pool_use = new_pool_use;

	if (pool_use == NULL) { /* Empty list */
		cpm->pool_use_list = new_pool_use;
	} else if (prev == NULL) { /* Start of list */
		cpm->pool_use_list = new_pool_use;
		new_pool_use->next = pool_use;
		pool_use->prev = new_pool_use;
	} else { /* Within list */
		prev->next = new_pool_use;
		new_pool_use->next = pool_use;
		new_pool_use->prev = prev;
	}

	cpm->available_pool_id = cpm->pool_use_list->pool_id;
	cpm->pool_valid = true;
#if (CPM_DEBUG == 1)
	show_list("Insert", cpm);
#endif
	return 0;
}

static int cpm_sort_pool_id(struct tfc_cpm *cpm, uint16_t pool_id)
{
	struct cpm_pool_entry *pool = &cpm->pools[pool_id];
	struct cpm_pool_use *pool_use = pool->pool_use;
	struct cpm_pool_use *prev, *next;

	/*
	 * Does entry need to move up, down or stay where it is?
	 *
	 * The list is ordered by:
	 *	  Head:	 - Most used, but not full
	 *			 - ....next most used but not full
	 *			 - least used
	 *	  Tail:	 - All used
	 */
	while (1) {
		if (pool_use->prev != NULL &&
			cpm->pools[pool_use->prev->pool_id].valid &&
			!pool->all_used &&
			(cpm->pools[pool_use->prev->pool_id].all_used ||
			 cpm->pools[pool_use->prev->pool_id].used_count <
			 pool->used_count)) {
			/* Move up */
			prev = pool_use->prev;
			pool_use->prev->next = pool_use->next;
			if (pool_use->next != NULL) /* May be at the end of the list */
				pool_use->next->prev = pool_use->prev;
			pool_use->next = pool_use->prev;

			if (pool_use->prev->prev != NULL) {
				pool_use->prev->prev->next = pool_use;
				pool_use->prev = pool_use->prev->prev;
			} else {
				/* Moved to head of the list */
				pool_use->prev->prev = pool_use;
				pool_use->prev = NULL;
				cpm->pool_use_list = pool_use;
			}

			prev->prev = pool_use;
		} else if (pool_use->next != NULL &&
				   cpm->pools[pool_use->next->pool_id].valid &&
				   (pool->all_used ||
					(!cpm->pools[pool_use->next->pool_id].all_used &&
					 (cpm->pools[pool_use->next->pool_id].used_count >
					  pool->used_count)))) {
			/* Move down */
			next = pool_use->next;
			pool_use->next->prev = pool_use->prev;
			if (pool_use->prev != NULL) /* May be at the start of the list */
				pool_use->prev->next = pool_use->next;
			else
				cpm->pool_use_list = pool_use->next;

			pool_use->prev = pool_use->next;

			if (pool_use->next->next != NULL) {
				pool_use->next->next->prev = pool_use;
				pool_use->next = pool_use->next->next;
			} else {
				/* Moved to end of the list */
				pool_use->next->next = pool_use;
				pool_use->next = NULL;
			}

			next->next = pool_use;
		} else {
			/* Nothing to do */
			break;
		}
#if (CPM_DEBUG == 1)
		show_list("Sort", cpm);
#endif
	}

	if (cpm->pools[cpm->pool_use_list->pool_id].all_used) {
		cpm->available_pool_id = TFC_CPM_INVALID_POOL_ID;
		cpm->pool_valid = false;
	} else {
		cpm->available_pool_id = cpm->pool_use_list->pool_id;
		cpm->pool_valid = true;
	}

	return 0;
}

int tfc_cpm_open(struct tfc_cpm **cpm, uint32_t max_pools)
{
	/* Allocate CPM struct */
	*cpm = rte_zmalloc("tf", sizeof(struct tfc_cpm), 0);
	if (*cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "cpm alloc error %s", strerror(ENOMEM));
		*cpm = NULL;
		return -ENOMEM;
	}

	/* Allocate CPM pools array */
	(*cpm)->pools = rte_zmalloc("tf", sizeof(struct cpm_pool_entry) * max_pools, 0);
	if ((*cpm)->pools == NULL) {
		PMD_DRV_LOG_LINE(ERR, "pools alloc error %s", strerror(ENOMEM));
		rte_free(*cpm);
		*cpm = NULL;

		return -ENOMEM;
	}

	/* Init pool entries by setting all fields to zero */
	memset((*cpm)->pools, 0, sizeof(struct cpm_pool_entry) * max_pools);

	/* Init remaining CPM fields */
	(*cpm)->pool_valid = false;
	(*cpm)->available_pool_id = 0;
	(*cpm)->max_pools = max_pools;
	(*cpm)->pool_use_list = NULL;

	return	0;
}

int tfc_cpm_close(struct tfc_cpm *cpm)
{
	struct cpm_pool_use *current;
	struct cpm_pool_use *next;

	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	/* Free pool_use_list */
	current = cpm->pool_use_list;
	while (current != NULL) {
		next = current->next;
		rte_free(current);
		current = next;
	}

	/* Free pools */
	rte_free(cpm->pools);

	/* Free CPM */
	rte_free(cpm);

	return 0;
}

int tfc_cpm_set_pool_size(struct tfc_cpm *cpm, uint32_t pool_sz_in_records)
{
	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	cpm->pool_size = pool_sz_in_records;
	return 0;
}

int tfc_cpm_get_pool_size(struct tfc_cpm *cpm, uint32_t *pool_sz_in_records)
{
	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	*pool_sz_in_records = cpm->pool_size;
	return 0;
}

int tfc_cpm_set_cmm_inst(struct tfc_cpm *cpm, uint16_t pool_id, struct tfc_cmm *cmm)
{
	struct cpm_pool_entry *pool;

	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	if (pool_id >= cpm->max_pools) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x > max 0x%x", pool_id, cpm->max_pools);
		return -EINVAL;
	}
	pool = &cpm->pools[pool_id];

	if (pool->valid && cmm != NULL) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x is already in use", pool_id);
		return -EINVAL;
	}

	pool->cmm = cmm;
	pool->used_count = 0;
	pool->all_used = false;
	pool->pool_use = NULL;

	if (cmm == NULL) {
		pool->valid = false;
	} else {
		pool->valid = true;
		cpm_insert_pool_id(cpm, pool_id);
	}

	return 0;
}

int tfc_cpm_get_cmm_inst(struct tfc_cpm *cpm, uint16_t pool_id, struct tfc_cmm **cmm)
{
	struct cpm_pool_entry *pool;

	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	if (pool_id >= cpm->max_pools) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x > max 0x%x", pool_id, cpm->max_pools);
		return -EINVAL;
	}

	pool = &cpm->pools[pool_id];

	if (!pool->valid) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x is not valid", pool_id);
		return -EINVAL;
	}

	*cmm = pool->cmm;
	return 0;
}

int tfc_cpm_get_avail_pool(struct tfc_cpm *cpm, uint16_t *pool_id)
{
	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	if (!cpm->pool_valid)
		return -EINVAL;

	*pool_id = cpm->available_pool_id;

	return 0;
}

int tfc_cpm_set_usage(struct tfc_cpm *cpm, uint16_t pool_id, uint32_t used_count, bool all_used)
{
	struct cpm_pool_entry *pool;

	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	if (pool_id >= cpm->max_pools) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x > max 0x%x", pool_id, cpm->max_pools);
		return -EINVAL;
	}
	pool = &cpm->pools[pool_id];

	if (!pool->valid) {
		PMD_DRV_LOG_LINE(ERR, "Pool ID:0x%x is invalid", pool_id);
		return -EINVAL;
	}

	if (used_count > cpm->pool_size) {
		PMD_DRV_LOG_LINE(ERR,
				 "Number of entries(%d) exceeds pool_size(%d)",
				 used_count, cpm->pool_size);
		return -EINVAL;
	}

	pool->all_used = all_used;
	pool->used_count = used_count;

	/* Update ordered list of pool_ids */
	cpm_sort_pool_id(cpm, pool_id);

	return 0;
}
int tfc_cpm_srchm_by_configured_pool(struct tfc_cpm *cpm, enum cfa_srch_mode srch_mode,
				    uint16_t *pool_id, struct tfc_cmm **cmm)
{
	uint32_t i;

	if (cpm == NULL) {
		PMD_DRV_LOG_LINE(ERR, "CPM is NULL");
		return -EINVAL;
	}

	if (!pool_id) {
		PMD_DRV_LOG_LINE(ERR, "pool_id ptr is NULL");
		return -EINVAL;
	}

	if (!cmm) {
		PMD_DRV_LOG_LINE(ERR, "cmm ptr is NULL");
		return -EINVAL;
	}
	*pool_id = TFC_CPM_INVALID_POOL_ID;
	*cmm = NULL;

	if (srch_mode == CFA_SRCH_MODE_FIRST)
		cpm->next_index = 0;

	for (i = cpm->next_index; i < cpm->max_pools; i++) {
		if (cpm->pools[i].cmm) {
			*pool_id = i;
			*cmm = cpm->pools[i].cmm;
			cpm->next_index = i + 1;
			return 0;
		}
	}
	cpm->next_index = cpm->max_pools;
	return -ENOENT;
}
