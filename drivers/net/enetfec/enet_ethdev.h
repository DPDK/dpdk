/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#ifndef __ENETFEC_ETHDEV_H__
#define __ENETFEC_ETHDEV_H__

/*
 * ENETFEC can support 1 rx and tx queue..
 */

#define ENETFEC_MAX_Q		1

struct enetfec_private {
	struct rte_eth_dev	*dev;
};

#endif /*__ENETFEC_ETHDEV_H__*/
