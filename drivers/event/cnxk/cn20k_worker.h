/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN20K_WORKER_H__
#define __CN20K_WORKER_H__

#include <rte_eventdev.h>

#include "cnxk_worker.h"
#include "cn20k_eventdev.h"

/* CN20K Fastpath functions. */
uint16_t __rte_hot cn20k_sso_hws_enq_burst(void *port, const struct rte_event ev[],
					   uint16_t nb_events);
uint16_t __rte_hot cn20k_sso_hws_enq_new_burst(void *port, const struct rte_event ev[],
					       uint16_t nb_events);
uint16_t __rte_hot cn20k_sso_hws_enq_fwd_burst(void *port, const struct rte_event ev[],
					       uint16_t nb_events);

#endif
