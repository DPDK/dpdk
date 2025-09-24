/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "flow_id_table.h"
#include "rte_spinlock.h"

#define NTNIC_ARRAY_BITS 14
#define NTNIC_ARRAY_SIZE (1 << NTNIC_ARRAY_BITS)
#define NTNIC_ARRAY_MASK (NTNIC_ARRAY_SIZE - 1)
#define NTNIC_MAX_ID (NTNIC_ARRAY_SIZE * NTNIC_ARRAY_SIZE)
#define NTNIC_MAX_ID_MASK (NTNIC_MAX_ID - 1)
#define NTNIC_MIN_FREE 1000

struct ntnic_id_table_element {
	union flm_handles handle;
	uint8_t caller_id;
	uint8_t type;
};

struct ntnic_id_table_data {
	struct ntnic_id_table_element *arrays[NTNIC_ARRAY_SIZE];
	rte_spinlock_t mtx;

	uint32_t next_id;

	uint32_t free_head;
	uint32_t free_tail;
	uint32_t free_count;
};

static inline struct ntnic_id_table_element *
ntnic_id_table_array_find_element(struct ntnic_id_table_data *handle, uint32_t id)
{
	uint32_t idx_d1 = id & NTNIC_ARRAY_MASK;
	uint32_t idx_d2 = (id >> NTNIC_ARRAY_BITS) & NTNIC_ARRAY_MASK;

	if (handle->arrays[idx_d2] == NULL) {
		handle->arrays[idx_d2] =
			calloc(NTNIC_ARRAY_SIZE, sizeof(struct ntnic_id_table_element));
	}

	return &handle->arrays[idx_d2][idx_d1];
}

static inline uint32_t ntnic_id_table_array_pop_free_id(struct ntnic_id_table_data *handle)
{
	uint32_t id = 0;

	if (handle->free_count > NTNIC_MIN_FREE) {
		struct ntnic_id_table_element *element =
			ntnic_id_table_array_find_element(handle, handle->free_tail);
		id = handle->free_tail;

		handle->free_tail = element->handle.idx & NTNIC_MAX_ID_MASK;
		handle->free_count -= 1;
	}

	return id;
}

void *nthw_id_table_create(void)
{
	struct ntnic_id_table_data *handle = calloc(1, sizeof(struct ntnic_id_table_data));

	rte_spinlock_init(&handle->mtx);
	handle->next_id = 1;

	return handle;
}

void nthw_id_table_destroy(void *id_table)
{
	struct ntnic_id_table_data *handle = id_table;

	for (uint32_t i = 0; i < NTNIC_ARRAY_SIZE; ++i)
		free(handle->arrays[i]);

	free(id_table);
}

uint32_t nthw_id_table_get_id(void *id_table, union flm_handles flm_h, uint8_t caller_id,
	uint8_t type)
{
	struct ntnic_id_table_data *handle = id_table;

	rte_spinlock_lock(&handle->mtx);

	uint32_t new_id = ntnic_id_table_array_pop_free_id(handle);

	if (new_id == 0)
		new_id = handle->next_id++;

	struct ntnic_id_table_element *element = ntnic_id_table_array_find_element(handle, new_id);
	element->caller_id = caller_id;
	element->type = type;
	element->handle = flm_h;

	rte_spinlock_unlock(&handle->mtx);

	return new_id;
}

void nthw_id_table_free_id(void *id_table, uint32_t id)
{
	struct ntnic_id_table_data *handle = id_table;

	rte_spinlock_lock(&handle->mtx);

	struct ntnic_id_table_element *current_element =
		ntnic_id_table_array_find_element(handle, id);
	if (current_element)
		memset(current_element, 0, sizeof(struct ntnic_id_table_element));

	struct ntnic_id_table_element *element =
		ntnic_id_table_array_find_element(handle, handle->free_head);
	element->handle.idx = id;
	handle->free_head = id;
	handle->free_count += 1;

	if (handle->free_tail == 0)
		handle->free_tail = handle->free_head;

	rte_spinlock_unlock(&handle->mtx);
}

void nthw_id_table_find(void *id_table, uint32_t id, union flm_handles *flm_h, uint8_t *caller_id,
	uint8_t *type)
{
	struct ntnic_id_table_data *handle = id_table;

	rte_spinlock_lock(&handle->mtx);

	struct ntnic_id_table_element *element = ntnic_id_table_array_find_element(handle, id);

	*caller_id = element->caller_id;
	*type = element->type;
	*flm_h = element->handle;

	rte_spinlock_unlock(&handle->mtx);
}
