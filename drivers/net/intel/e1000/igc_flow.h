/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#ifndef _IGC_FLOW_H_
#define _IGC_FLOW_H_

#include <rte_flow_driver.h>
#include "igc_ethdev.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct rte_flow_ops igc_flow_ops;

void igc_flow_init(struct rte_eth_dev *dev);
int igc_flow_flush(struct rte_eth_dev *dev,
		__rte_unused struct rte_flow_error *error);

#ifdef __cplusplus
}
#endif

#endif /* _IGC_FLOW_H_ */
