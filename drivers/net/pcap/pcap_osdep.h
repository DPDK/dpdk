/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Dmitry Kozlyuk
 */

#ifndef _RTE_PCAP_OSDEP_
#define _RTE_PCAP_OSDEP_

#include <rte_ether.h>

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, eth_pcap_logtype, \
		"%s(): " fmt "\n", __func__, ##args)
extern int eth_pcap_logtype;

int osdep_iface_index_get(const char *name);
int osdep_iface_mac_get(const char *name, struct rte_ether_addr *mac);

#endif
