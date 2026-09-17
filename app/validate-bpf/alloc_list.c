/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "internal.h"
#include <stdlib.h>


/* Needs to be a power of two. */
#define START_CAPACITY (1u << 3)

size_t
alloc_list_append(struct alloc_list *alloc_list, void *ptr)
{
	if (alloc_list->count == 0) {
		RTE_ASSERT(alloc_list->ptrs == NULL);
		alloc_list->ptrs = malloc(
			sizeof(alloc_list->ptrs[0]) * START_CAPACITY);
		RTE_VERIFY(alloc_list->ptrs != NULL);
	} else if (alloc_list->count >= START_CAPACITY &&
			/* Power of two detection */
			(alloc_list->count & (alloc_list->count - 1)) == 0) {
		/*
		 * We allocate in powers of two, and current count is one of
		 * these powers, so need to reallocate to larger capacity.
		 */
		RTE_ASSERT(alloc_list->ptrs != NULL);
		const size_t new_capacity = alloc_list->count * 2;
		alloc_list->ptrs = realloc(alloc_list->ptrs,
			sizeof(alloc_list->ptrs[0]) * new_capacity);
		RTE_VERIFY(alloc_list->ptrs != NULL);
	}
	alloc_list->ptrs[alloc_list->count] = ptr;
	return alloc_list->count++;
}

void alloc_list_replace(struct alloc_list *alloc_list, size_t index, void *ptr)
{
	RTE_ASSERT(index < alloc_list->count);
	alloc_list->ptrs[index] = ptr;
}

void alloc_list_free_all(struct alloc_list *alloc_list)
{
	/* Copy and clear fields first in case alloc_list itself gets freed. */
	size_t count = alloc_list->count;
	void ** const ptrs = alloc_list->ptrs;
	*alloc_list = (struct alloc_list){};

	while (count != 0)
		free(ptrs[--count]);
	free(ptrs);
}
