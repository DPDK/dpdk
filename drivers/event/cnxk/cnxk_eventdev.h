/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CNXK_EVENTDEV_H__
#define __CNXK_EVENTDEV_H__

#include <rte_pci.h>

#include <eventdev_pmd_pci.h>

#include "roc_api.h"

#define USEC2NSEC(__us) ((__us)*1E3)

#define CNXK_SSO_MZ_NAME "cnxk_evdev_mz"

struct cnxk_sso_evdev {
	struct roc_sso sso;
	uint8_t is_timeout_deq;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint32_t min_dequeue_timeout_ns;
	uint32_t max_dequeue_timeout_ns;
	int32_t max_num_events;
} __rte_cache_aligned;

static inline struct cnxk_sso_evdev *
cnxk_sso_pmd_priv(const struct rte_eventdev *event_dev)
{
	return event_dev->data->dev_private;
}

/* Common ops API. */
int cnxk_sso_init(struct rte_eventdev *event_dev);
int cnxk_sso_fini(struct rte_eventdev *event_dev);
int cnxk_sso_remove(struct rte_pci_device *pci_dev);

#endif /* __CNXK_EVENTDEV_H__ */
