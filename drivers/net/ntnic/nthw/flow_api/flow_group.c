/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>

#include "flow_api_engine.h"

#define OWNER_ID_COUNT 256
#define PORT_COUNT 8

struct group_lookup_entry_s {
	uint64_t ref_counter;
	uint32_t *reverse_lookup;
};

struct group_handle_s {
	uint32_t group_count;

	uint32_t *translation_table;

	struct group_lookup_entry_s *lookup_entries;
};

int flow_group_handle_create(void **handle, uint32_t group_count)
{
	struct group_handle_s *group_handle;

	*handle = calloc(1, sizeof(struct group_handle_s));
	group_handle = *handle;

	group_handle->group_count = group_count;
	group_handle->translation_table =
		calloc((uint32_t)(group_count * PORT_COUNT * OWNER_ID_COUNT), sizeof(uint32_t));
	group_handle->lookup_entries = calloc(group_count, sizeof(struct group_lookup_entry_s));

	return *handle != NULL ? 0 : -1;
}

int flow_group_handle_destroy(void **handle)
{
	if (*handle) {
		struct group_handle_s *group_handle = (struct group_handle_s *)*handle;

		free(group_handle->translation_table);
		free(group_handle->lookup_entries);

		free(*handle);
		*handle = NULL;
	}

	return 0;
}
