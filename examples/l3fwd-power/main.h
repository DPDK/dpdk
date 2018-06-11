/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_


#define MAX_LCORE_PARAMS 1024
struct lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

extern struct lcore_params *lcore_params;
extern uint16_t nb_lcore_params;
extern struct lcore_params lcore_params_array[];

#endif /* _MAIN_H_ */
