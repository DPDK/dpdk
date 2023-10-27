/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2023 Intel Corporation
 */

#ifndef _SIMPLE_MP_COMMANDS_H_
#define _SIMPLE_MP_COMMANDS_H_

#include "commands.h"

extern struct rte_ring *send_ring;
extern struct rte_mempool *message_pool;
extern volatile int quit;

#endif /* _SIMPLE_MP_COMMANDS_H_ */
