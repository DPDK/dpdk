/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_EVDEV_H__
#define __OTX2_EVDEV_H__

#include <rte_eventdev.h>

#include "otx2_common.h"

#define EVENTDEV_NAME_OCTEONTX2_PMD otx2_eventdev

#define sso_func_trace otx2_sso_dbg

#define OTX2_SSO_MAX_VHGRP                  RTE_EVENT_MAX_QUEUES_PER_DEV
#define OTX2_SSO_MAX_VHWS                   (UINT8_MAX)

struct otx2_sso_evdev {
};

/* Init and Fini API's */
int otx2_sso_init(struct rte_eventdev *event_dev);
int otx2_sso_fini(struct rte_eventdev *event_dev);

#endif /* __OTX2_EVDEV_H__ */
