/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#ifndef _MRVL_FLOW_H_
#define _MRVL_FLOW_H_

#include "mrvl_ethdev.h"

void mrvl_flow_init(struct rte_eth_dev *dev);
void mrvl_flow_deinit(struct rte_eth_dev *dev);

#endif /* _MRVL_FLOW_H_ */
