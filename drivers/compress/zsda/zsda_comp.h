/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_COMP_H_
#define _ZSDA_COMP_H_

#include "zsda_comp_pmd.h"

struct __rte_packed_begin compress_cfg {
} __rte_packed_end;

struct __rte_packed_begin zsda_wqe_comp {
	uint8_t valid;
	uint8_t op_code;
	uint16_t sid;
	uint8_t resv[3];
	uint8_t rx_sgl_type : 4;
	uint8_t tx_sgl_type : 4;
	uint64_t rx_addr;
	uint32_t rx_length;
	uint64_t tx_addr;
	uint32_t tx_length;
	struct compress_cfg cfg;
} __rte_packed_end;

int zsda_comp_match(const void *op_in);
int zsda_decomp_match(const void *op_in);

int zsda_comp_request_build(void *op_in, const struct zsda_queue *queue,
		       void **op_cookies, const uint16_t new_tail);

int zsda_decomp_request_build(void *op_in, const struct zsda_queue *queue,
			 void **op_cookies, const uint16_t new_tail);

#endif /* _ZSDA_COMP_H_ */
