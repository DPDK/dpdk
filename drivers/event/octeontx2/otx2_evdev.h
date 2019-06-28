/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_EVDEV_H__
#define __OTX2_EVDEV_H__

#include <rte_eventdev.h>

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_mempool.h"

#define EVENTDEV_NAME_OCTEONTX2_PMD otx2_eventdev

#define sso_func_trace otx2_sso_dbg

#define OTX2_SSO_MAX_VHGRP                  RTE_EVENT_MAX_QUEUES_PER_DEV
#define OTX2_SSO_MAX_VHWS                   (UINT8_MAX)

#define USEC2NSEC(__us)                 ((__us) * 1E3)

struct otx2_sso_evdev {
	OTX2_DEV; /* Base class */
	uint8_t max_event_queues;
	uint8_t max_event_ports;
	uint8_t is_timeout_deq;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint32_t deq_tmo_ns;
	uint32_t min_dequeue_timeout_ns;
	uint32_t max_dequeue_timeout_ns;
	int32_t max_num_events;
} __rte_cache_aligned;

static inline struct otx2_sso_evdev *
sso_pmd_priv(const struct rte_eventdev *event_dev)
{
	return event_dev->data->dev_private;
}

/* Init and Fini API's */
int otx2_sso_init(struct rte_eventdev *event_dev);
int otx2_sso_fini(struct rte_eventdev *event_dev);

#endif /* __OTX2_EVDEV_H__ */
