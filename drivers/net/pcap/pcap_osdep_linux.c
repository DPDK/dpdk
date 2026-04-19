/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <rte_string_fns.h>

#include "pcap_osdep.h"

int
osdep_iface_index_get(const char *name)
{
	return if_nametoindex(name);
}

int
osdep_iface_mac_get(const char *if_name, struct rte_ether_addr *mac)
{
	struct ifreq ifr;
	int if_fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (if_fd == -1)
		return -1;

	rte_strscpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(if_fd, SIOCGIFHWADDR, &ifr)) {
		close(if_fd);
		return -1;
	}

	memcpy(mac->addr_bytes, ifr.ifr_hwaddr.sa_data, RTE_ETHER_ADDR_LEN);

	close(if_fd);
	return 0;
}

int
osdep_iface_link_status(const char *if_name)
{
	struct ifreq ifr;
	int fd, status = 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		return -1;

	rte_strscpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {
		/*
		 * IFF_UP means administratively up.
		 * IFF_RUNNING means operationally up (carrier detected).
		 * Both must be set for link to be considered up.
		 */
		if ((ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING))
			status = 1;
	} else {
		status = -1;
	}

	close(fd);
	return status;
}
