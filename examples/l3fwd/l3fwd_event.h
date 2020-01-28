/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L3FWD_EVENTDEV_H__
#define __L3FWD_EVENTDEV_H__

#include <rte_common.h>
#include <rte_eventdev.h>
#include <rte_spinlock.h>

#include "l3fwd.h"

struct l3fwd_event_resources {
	uint8_t sched_type;
	uint8_t enabled;
	uint8_t eth_rx_queues;
};

struct l3fwd_event_resources *l3fwd_get_eventdev_rsrc(void);
void l3fwd_event_resource_setup(void);

#endif /* __L3FWD_EVENTDEV_H__ */
