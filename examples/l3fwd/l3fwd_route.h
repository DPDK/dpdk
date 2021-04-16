/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

struct ipv4_l3fwd_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

struct ipv6_l3fwd_route {
	uint8_t ip[16];
	uint8_t depth;
	uint8_t if_out;
};

extern const struct ipv4_l3fwd_route ipv4_l3fwd_route_array[8];

extern const struct ipv6_l3fwd_route ipv6_l3fwd_route_array[8];
