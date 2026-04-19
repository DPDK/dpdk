/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Dmitry Kozlyuk
 */

#ifndef _RTE_PCAP_OSDEP_
#define _RTE_PCAP_OSDEP_

#include <rte_ether.h>
#include <rte_log.h>

#define PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ETH_PCAP, "%s(): ", __func__, __VA_ARGS__)
extern int eth_pcap_logtype;
#define RTE_LOGTYPE_ETH_PCAP eth_pcap_logtype

#ifdef RTE_ETHDEV_DEBUG_RX
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ETH_PCAP, "%s() rx: ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ETH_PCAP, "%s() tx: ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

int osdep_iface_index_get(const char *name);
int osdep_iface_mac_get(const char *name, struct rte_ether_addr *mac);

#endif
