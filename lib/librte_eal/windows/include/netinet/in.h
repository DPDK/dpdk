/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef _IN_H_
#define _IN_H_

#include <stdint.h>
#include <sys/socket.h>

#define IPPROTO_IP         0
#define IPPROTO_HOPOPTS    0
#define IPPROTO_ICMP       1
#define IPPROTO_IPIP       4
#define IPPROTO_TCP        6
#define IPPROTO_UDP       17
#define IPPROTO_IPV6      41
#define IPPROTO_ROUTING   43
#define IPPROTO_FRAGMENT  44
#define IPPROTO_GRE       47
#define IPPROTO_ESP       50
#define IPPROTO_AH        51
#define IPPROTO_ICMPV6    58
#define IPPROTO_NONE      59
#define IPPROTO_DSTOPTS   60
#define IPPROTO_SCTP     132

#define INET6_ADDRSTRLEN 46

struct in_addr {
	uint32_t s_addr;
};

struct in6_addr {
	uint8_t s6_addr[16];
};

#endif
