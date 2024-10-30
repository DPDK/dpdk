/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "flow_id_table.h"

#define NTNIC_ARRAY_BITS 14
#define NTNIC_ARRAY_SIZE (1 << NTNIC_ARRAY_BITS)

struct ntnic_id_table_element {
	union flm_handles handle;
	uint8_t caller_id;
	uint8_t type;
};

struct ntnic_id_table_data {
	struct ntnic_id_table_element *arrays[NTNIC_ARRAY_SIZE];
	pthread_mutex_t mtx;

	uint32_t next_id;

	uint32_t free_head;
	uint32_t free_tail;
	uint32_t free_count;
};

void *ntnic_id_table_create(void)
{
	struct ntnic_id_table_data *handle = calloc(1, sizeof(struct ntnic_id_table_data));

	pthread_mutex_init(&handle->mtx, NULL);
	handle->next_id = 1;

	return handle;
}

void ntnic_id_table_destroy(void *id_table)
{
	struct ntnic_id_table_data *handle = id_table;

	for (uint32_t i = 0; i < NTNIC_ARRAY_SIZE; ++i)
		free(handle->arrays[i]);

	pthread_mutex_destroy(&handle->mtx);

	free(id_table);
}
