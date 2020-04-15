/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#ifndef _IGC_ETHDEV_H_
#define _IGC_ETHDEV_H_

#include <rte_ethdev.h>

#include "base/igc_osdep.h"
#include "base/igc_hw.h"
#include "base/igc_i225.h"
#include "base/igc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IGC_QUEUE_PAIRS_NUM		4

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct igc_adapter {
	struct igc_hw		hw;
};

#define IGC_DEV_PRIVATE(_dev)	((_dev)->data->dev_private)

#define IGC_DEV_PRIVATE_HW(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->hw)

#ifdef __cplusplus
}
#endif

#endif /* _IGC_ETHDEV_H_ */
