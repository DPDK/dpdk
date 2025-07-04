/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_HASHER_H_
#define _FLOW_HASHER_H_

#include <stdint.h>

struct hasher_s {
	int banks;
	int cam_records_bw;
	uint32_t cam_records_bw_mask;
	int cam_bw;
};

int nthw_init_hasher(struct hasher_s *hsh, int _banks, int nb_records);
uint32_t nthw_gethash(struct hasher_s *hsh, const uint32_t key[16], int *result);

#endif	/* _FLOW_HASHER_H_ */
