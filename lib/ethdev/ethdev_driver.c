/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "ethdev_driver.h"

uint16_t
rte_eth_pkt_burst_dummy(void *queue __rte_unused,
		struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_pkts __rte_unused)
{
	return 0;
}
