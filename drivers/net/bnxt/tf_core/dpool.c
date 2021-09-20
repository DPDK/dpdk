/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include "tfp.h"
#include "dpool.h"

int dpool_init(struct dpool *dpool,
	       uint32_t start_index,
	       uint32_t size,
	       uint8_t max_alloc_size,
	       void *user_data,
	       int (*move_callback)(void *, uint64_t, uint32_t))
{
	uint32_t i;
	int rc;
	struct tfp_calloc_parms parms;

	parms.nitems = size;
	parms.size = sizeof(struct dpool_entry);
	parms.alignment = 0;

	rc = tfp_calloc(&parms);

	if (rc)
		return rc;

	dpool->entry = parms.mem_va;
	dpool->start_index = start_index;
	dpool->size = size;
	dpool->max_alloc_size = max_alloc_size;
	dpool->user_data = user_data;
	dpool->move_callback = move_callback;
	/*
	 * Init entries
	 */
	for (i = 0; i < size; i++) {
		dpool->entry[i].flags = 0;
		dpool->entry[i].index = start_index;
		dpool->entry[i].entry_data = 0UL;
		start_index++;
	}

	return 0;
}

static int dpool_move(struct dpool *dpool,
		      uint32_t dst_index,
		      uint32_t src_index)
{
	uint32_t size;
	uint32_t i;
	if (DP_IS_FREE(dpool->entry[dst_index].flags)) {
		size = DP_FLAGS_SIZE(dpool->entry[src_index].flags);

		dpool->entry[dst_index].flags = dpool->entry[src_index].flags;
		dpool->entry[dst_index].entry_data = dpool->entry[src_index].entry_data;

		if (dpool->move_callback != NULL) {
			dpool->move_callback(dpool->user_data,
					     dpool->entry[src_index].entry_data,
					     dst_index + dpool->start_index);
		}

		dpool->entry[src_index].flags = 0;
		dpool->entry[src_index].entry_data = 0UL;

		for (i = 1; i < size; i++) {
			dpool->entry[dst_index + i].flags = size;
			dpool->entry[src_index + i].flags = 0;
		}
	} else {
		return -1;
	}

	return 0;
}

int dpool_defrag(struct dpool *dpool,
		 uint32_t entry_size,
		 uint8_t defrag)
{
	struct dpool_free_list *free_list;
	struct dpool_adj_list *adj_list;
	struct tfp_calloc_parms parms;
	uint32_t count;
	uint32_t index;
	uint32_t used;
	uint32_t i;
	uint32_t size;
	uint32_t largest_free_index = 0;
	uint32_t largest_free_size;
	uint32_t max;
	uint32_t max_index;
	uint32_t max_size = 0;
	int rc;

	parms.nitems = 1;
	parms.size = sizeof(struct dpool_free_list);
	parms.alignment = 0;

	rc = tfp_calloc(&parms);

	if (rc)
		return rc;

	free_list = (struct dpool_free_list *)parms.mem_va;
	if (free_list == NULL) {
		TFP_DRV_LOG(ERR, "dpool free list allocation failed\n");
		return -ENOMEM;
	}

	parms.nitems = 1;
	parms.size = sizeof(struct dpool_adj_list);
	parms.alignment = 0;

	rc = tfp_calloc(&parms);

	if (rc)
		return rc;

	adj_list = (struct dpool_adj_list *)parms.mem_va;
	if (adj_list == NULL) {
		TFP_DRV_LOG(ERR, "dpool adjacent list allocation failed\n");
		return -ENOMEM;
	}

	while (1) {
		/*
		 * Create list of free entries
		 */
		free_list->size = 0;
		largest_free_size = 0;
		largest_free_index = 0;
		count = 0;
		index = 0;

		for (i = 0; i < dpool->size; i++) {
			if (DP_IS_FREE(dpool->entry[i].flags)) {
				if (count == 0)
					index = i;
				count++;
			} else if (count > 0) {
				free_list->entry[free_list->size].index = index;
				free_list->entry[free_list->size].size = count;

				if (count > largest_free_size) {
					largest_free_index = free_list->size;
					largest_free_size = count;
				}

				free_list->size++;
				count = 0;
			}
		}

		if (free_list->size == 0)
			largest_free_size = count;

		/*
		 * If using defrag to fit and there's a large enough
		 * space then we are done.
		 */
		if (defrag == DP_DEFRAG_TO_FIT &&
		    largest_free_size >= entry_size)
			goto done;

		/*
		 * Create list of entries adjacent to free entries
		 */
		count = 0;
		adj_list->size = 0;
		used = 0;

		for (i = 0; i < dpool->size; ) {
			if (DP_IS_USED(dpool->entry[i].flags)) {
				used++;

				if (count > 0) {
					adj_list->entry[adj_list->size].index = i;
					adj_list->entry[adj_list->size].size =
						DP_FLAGS_SIZE(dpool->entry[i].flags);
					adj_list->entry[adj_list->size].left = count;

					if (adj_list->size > 0 && used == 1)
						adj_list->entry[adj_list->size - 1].right = count;

					adj_list->size++;
				}

				count = 0;
				i += DP_FLAGS_SIZE(dpool->entry[i].flags);
			} else {
				used = 0;
				count++;
				i++;
			}
		}

		/*
		 * Using the size of the largest free space available
		 * select the adjacency list entry of that size with
		 * the largest left + right + size count. If there
		 * are no entries of that size then decrement the size
		 * and try again.
		 */
		max = 0;
		max_index = 0;
		max_size = 0;

		for (size = largest_free_size; size > 0; size--) {
			for (i = 0; i < adj_list->size; i++) {
				if (adj_list->entry[i].size == size &&
				    ((size +
				      adj_list->entry[i].left +
				      adj_list->entry[i].right) > max)) {
					max = size +
						adj_list->entry[i].left +
						adj_list->entry[i].right;
					max_size = size;
					max_index = adj_list->entry[i].index;
				}
			}

			if (max)
				break;
		}

		/*
		 * If the max entry is smaller than the largest_free_size
		 * find the first entry in the free list that it cn fit in to.
		 */
		if (max_size < largest_free_size) {
			for (i = 0; i < free_list->size; i++) {
				if (free_list->entry[i].size >= max_size) {
					largest_free_index = i;
					break;
				}
			}
		}

		/*
		 * If we have a contender then move it to the new spot.
		 */
		if (max) {
			rc = dpool_move(dpool,
					free_list->entry[largest_free_index].index,
					max_index);
			if (rc) {
				tfp_free(free_list);
				tfp_free(adj_list);
				return rc;
			}
		} else {
			break;
		}
	}

done:
	tfp_free(free_list);
	tfp_free(adj_list);
	return largest_free_size;
}

uint32_t dpool_alloc(struct dpool *dpool,
		     uint32_t size,
		     uint8_t defrag)
{
	uint32_t i;
	uint32_t j;
	uint32_t count = 0;
	uint32_t first_entry_index;
	int rc;

	if (size > dpool->max_alloc_size || size == 0)
		return DP_INVALID_INDEX;

	/*
	 * Defrag requires EM move support.
	 */
	if (defrag != DP_DEFRAG_NONE &&
	    dpool->move_callback == NULL)
		return DP_INVALID_INDEX;

	while (1) {
		/*
		 * find <size> consecutive free entries
		 */
		for (i = 0; i < dpool->size; i++) {
			if (DP_IS_FREE(dpool->entry[i].flags)) {
				if (count == 0)
					first_entry_index = i;

				count++;

				if (count == size) {
					for (j = 0; j < size; j++) {
						dpool->entry[j + first_entry_index].flags = size;
						if (j == 0)
							dpool->entry[j + first_entry_index].flags |=
								DP_FLAGS_START;
					}

					dpool->entry[i].entry_data = 0UL;
					return (first_entry_index + dpool->start_index);
				}
			} else {
				count = 0;
			}
		}

		/*
		 * If defragging then do it to it
		 */
		if (defrag != DP_DEFRAG_NONE) {
			rc = dpool_defrag(dpool, size, defrag);

			if (rc < 0)
				return DP_INVALID_INDEX;
		} else {
			break;
		}

		/*
		 * If the defrag created enough space then try the
		 * alloc again else quit.
		 */
		if ((uint32_t)rc < size)
			break;
	}

	return DP_INVALID_INDEX;
}

int dpool_free(struct dpool *dpool,
	       uint32_t index)
{
	uint32_t i;
	int start = (index - dpool->start_index);
	uint32_t size;

	if (start < 0)
		return -1;

	if (DP_IS_START(dpool->entry[start].flags)) {
		size = DP_FLAGS_SIZE(dpool->entry[start].flags);
		if (size > dpool->max_alloc_size || size == 0)
			return -1;

		for (i = start; i < (start + size); i++)
			dpool->entry[i].flags = 0;

		return 0;
	}

	return -1;
}

void dpool_free_all(struct dpool *dpool)
{
	uint32_t i;

	for (i = 0; i < dpool->size; i++)
		dpool_free(dpool, dpool->entry[i].index);
}

int dpool_set_entry_data(struct dpool *dpool,
			 uint32_t index,
			 uint64_t entry_data)
{
	int start = (index - dpool->start_index);

	if (start < 0)
		return -1;

	if (DP_IS_START(dpool->entry[start].flags)) {
		dpool->entry[start].entry_data = entry_data;
		return 0;
	}

	return -1;
}
