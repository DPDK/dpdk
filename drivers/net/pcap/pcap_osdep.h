/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Dmitry Kozlyuk
 */

#ifndef _RTE_PCAP_OSDEP_
#define _RTE_PCAP_OSDEP_

#include <rte_ether.h>

int osdep_iface_index_get(const char *name);
int osdep_iface_mac_get(const char *name, struct rte_ether_addr *mac);

#endif
