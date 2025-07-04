/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef __CNXK_VECTOR_ADPTR_H__
#define __CNXK_VECTOR_ADPTR_H__

#include <rte_event_vector_adapter.h>
#include <rte_eventdev.h>

struct cnxk_event_vector_adapter {
	uint8_t tt;
	uint32_t agq;  /**< Aggregation queue ID */
	uint64_t base; /**< Base address of the adapter */
};

int cnxk_vector_caps_get(const struct rte_eventdev *evdev, uint32_t *caps,
			 const struct event_vector_adapter_ops **ops);
int cnxk_vector_info_get(const struct rte_eventdev *evdev,
			 struct rte_event_vector_adapter_info *info);

#endif /* __CNXK_VECTOR_ADPTR_H__ */
