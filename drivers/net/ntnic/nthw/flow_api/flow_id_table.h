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

void *ntnic_id_table_create(void);
void ntnic_id_table_destroy(void *id_table);

#endif	/* FLOW_ID_TABLE_H_ */
