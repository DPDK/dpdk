/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#include <net/if.h>
#include <net/if_dl.h>
#include <sys/sysctl.h>

#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "pcap_osdep.h"

int
osdep_iface_index_get(const char *name)
{
	return if_nametoindex(name);
}

int
osdep_iface_mac_get(const char *if_name, struct rte_ether_addr *mac)
{
	struct if_msghdr *ifm;
	struct sockaddr_dl *sdl;
	int mib[6];
	size_t len = 0;
	char *buf;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_nametoindex(if_name);

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
		return -1;

	if (len == 0)
		return -1;

	buf = rte_malloc(NULL, len, 0);
	if (!buf)
		return -1;

	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		rte_free(buf);
		return -1;
	}
	ifm = (struct if_msghdr *)buf;
	sdl = (struct sockaddr_dl *)(ifm + 1);

	rte_memcpy(mac->addr_bytes, LLADDR(sdl), RTE_ETHER_ADDR_LEN);

	rte_free(buf);
	return 0;
}
