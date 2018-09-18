/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#ifndef _DSW_EVDEV_H_
#define _DSW_EVDEV_H_

#include <rte_eventdev.h>

#define DSW_PMD_NAME RTE_STR(event_dsw)

struct dsw_evdev {
	struct rte_eventdev_data *data;
};

#endif
