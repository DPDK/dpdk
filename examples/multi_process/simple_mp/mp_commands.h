/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _SIMPLE_MP_COMMANDS_H_
#define _SIMPLE_MP_COMMANDS_H_

extern struct rte_ring *send_ring;
extern struct rte_mempool *message_pool;
extern volatile int quit;

extern cmdline_parse_ctx_t simple_mp_ctx[];

#endif /* _SIMPLE_MP_COMMANDS_H_ */
