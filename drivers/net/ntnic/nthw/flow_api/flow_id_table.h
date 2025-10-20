/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _FLOW_ID_TABLE_H_
#define _FLOW_ID_TABLE_H_

#include <stdint.h>

union flm_handles {
	uint64_t idx;
	void *p;
};

void *nthw_id_table_create(void);
void nthw_id_table_destroy(void *id_table);

uint32_t nthw_id_table_get_id(void *id_table, union flm_handles flm_h, uint8_t caller_id,
	uint8_t type);
void nthw_id_table_free_id(void *id_table, uint32_t id);

void nthw_id_table_find(void *id_table, uint32_t id, union flm_handles *flm_h, uint8_t *caller_id,
	uint8_t *type);

#endif	/* FLOW_ID_TABLE_H_ */
