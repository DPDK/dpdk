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

/* For situations where err0 are reported but the results are correct */
#define DECOMP_RIGHT_ERR0_0 0xC710
#define DECOMP_RIGHT_ERR0_1 0xC727
#define DECOMP_RIGHT_ERR0_2 0xC729
#define CQE_ERR0_RIGHT(value)                                                  \
	(value == DECOMP_RIGHT_ERR0_0 || value == DECOMP_RIGHT_ERR0_1 ||       \
	 value == DECOMP_RIGHT_ERR0_2)

int zsda_comp_match(const void *op_in);
int zsda_decomp_match(const void *op_in);

int zsda_comp_wqe_build(void *op_in, const struct zsda_queue *queue,
		       void **op_cookies, const uint16_t new_tail);

int zsda_decomp_request_build(void *op_in, const struct zsda_queue *queue,
			 void **op_cookies, const uint16_t new_tail);
int zsda_comp_callback(void *cookie_in, struct zsda_cqe *cqe);

#endif /* _ZSDA_COMP_H_ */
