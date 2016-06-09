/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Routing Table (RT)
 */
#include <sys/types.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_errno.h>
#include <rte_ip.h>

#include "ipsec.h"

#define RT_IPV4_MAX_RULES	1024
#define RT_IPV6_MAX_RULES	1024

struct ip4_route {
	uint32_t ip;
	uint8_t depth;
	uint8_t if_out;
};

struct ip6_route {
	uint8_t ip[16];
	uint8_t depth;
	uint8_t if_out;
};

static struct ip4_route rt_ip4_ep0[] = {
	/* Outbound */
	/* Tunnels */
	{ IPv4(172, 16, 2, 5), 32, 0 },
	{ IPv4(172, 16, 2, 6), 32, 1 },
	/* Transport */
	{ IPv4(192, 168, 175, 0), 24, 0 },
	{ IPv4(192, 168, 176, 0), 24, 1 },
	/* Bypass */
	{ IPv4(192, 168, 240, 0), 24, 0 },
	{ IPv4(192, 168, 241, 0), 24, 1 },

	/* Inbound */
	/* Tunnels */
	{ IPv4(192, 168, 115, 0), 24, 2 },
	{ IPv4(192, 168, 116, 0), 24, 3 },
	{ IPv4(192, 168, 65, 0), 24, 2 },
	{ IPv4(192, 168, 66, 0), 24, 3 },
	/* Transport */
	{ IPv4(192, 168, 185, 0), 24, 2 },
	{ IPv4(192, 168, 186, 0), 24, 3 },
	/* NULL */
	{ IPv4(192, 168, 210, 0), 24, 2 },
	{ IPv4(192, 168, 211, 0), 24, 3 },
	/* Bypass */
	{ IPv4(192, 168, 245, 0), 24, 2 },
	{ IPv4(192, 168, 246, 0), 24, 3 },
};

static struct ip6_route rt_ip6_ep0[] = {
	/* Outbound */
	/* Tunnels */
	{ { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
		  0x22, 0x22, 0x22, 0x22, 0x22, 0x55, 0x55 }, 116, 0 },
	{ { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
		  0x22, 0x22, 0x22, 0x22, 0x22, 0x66, 0x66 }, 116, 1 },
	/* Transport */
	{ { 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 116, 0 },
	{ { 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11,
		  0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00 }, 116, 1 },
	/* Inbound */
	/* Tunnels */
	{ { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa,
		  0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00 }, 116, 2 },
	{ { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbb,
		  0xbb, 0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00 }, 116, 3 },
	{ { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55,
		  0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00 }, 116, 2 },
	{ { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66,
		  0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00 }, 116, 3 },
	/* Transport */
	{ { 0xff, 0xff, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 116, 2 },
	{ { 0xff, 0xff, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11,
		  0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00 }, 116, 3 },
};

static struct ip4_route rt_ip4_ep1[] = {
	/* Outbound */
	/* Tunnels */
	{ IPv4(172, 16, 1, 5), 32, 0 },
	{ IPv4(172, 16, 1, 6), 32, 1 },
	/* Transport */
	{ IPv4(192, 168, 185, 0), 24, 0 },
	{ IPv4(192, 168, 186, 0), 24, 1 },
	/* Bypass */
	{ IPv4(192, 168, 245, 0), 24, 0 },
	{ IPv4(192, 168, 246, 0), 24, 1 },

	/* Inbound */
	/* Tunnels */
	{ IPv4(192, 168, 105, 0), 24, 2 },
	{ IPv4(192, 168, 106, 0), 24, 3 },
	{ IPv4(192, 168, 55, 0), 24, 2 },
	{ IPv4(192, 168, 56, 0), 24, 3 },
	/* Transport */
	{ IPv4(192, 168, 175, 0), 24, 2 },
	{ IPv4(192, 168, 176, 0), 24, 3 },
	/* NULL */
	{ IPv4(192, 168, 200, 0), 24, 2 },
	{ IPv4(192, 168, 201, 0), 24, 3 },
	/* Bypass */
	{ IPv4(192, 168, 240, 0), 24, 2 },
	{ IPv4(192, 168, 241, 0), 24, 3 },
};

static struct ip6_route rt_ip6_ep1[] = {
	/* Outbound */
	/* Tunnels */
	{ { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		  0x11, 0x11, 0x11, 0x11, 0x11, 0x55, 0x55 }, 116, 0 },
	{ { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		  0x11, 0x11, 0x11, 0x11, 0x11, 0x66, 0x66 }, 116, 1 },
	/* Transport */
	{ { 0xff, 0xff, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 116, 0 },
	{ { 0xff, 0xff, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11,
		  0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00 }, 116, 1 },

	/* Inbound */
	/* Tunnels */
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa,
		  0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00 }, 116, 2 },
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbb,
		  0xbb, 0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00 }, 116, 3 },
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55,
		  0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00 }, 116, 2 },
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66,
		  0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00 }, 116, 3 },
	/* Transport */
	{ { 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 116, 2 },
	{ { 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11,
		  0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00 }, 116, 3 },
};

void
rt_init(struct socket_ctx *ctx, int32_t socket_id, uint32_t ep)
{
	char name[PATH_MAX];
	uint32_t i;
	int32_t ret;
	struct rte_lpm *lpm;
	struct rte_lpm6 *lpm6;
	struct ip4_route *rt;
	struct ip6_route *rt6;
	char a, b, c, d;
	uint32_t nb_routes, nb_routes6;
	struct rte_lpm_config conf = { 0 };
	struct rte_lpm6_config conf6 = { 0 };

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->rt_ip4 != NULL)
		rte_exit(EXIT_FAILURE, "IPv4 Routing Table for socket %u "
			"already initialized\n", socket_id);

	if (ctx->rt_ip6 != NULL)
		rte_exit(EXIT_FAILURE, "IPv6 Routing Table for socket %u "
			"already initialized\n", socket_id);

	printf("Creating IPv4 Routing Table (RT) context with %u max routes\n",
			RT_IPV4_MAX_RULES);

	if (ep == 0) {
		rt = rt_ip4_ep0;
		nb_routes = RTE_DIM(rt_ip4_ep0);
		rt6 = rt_ip6_ep0;
		nb_routes6 = RTE_DIM(rt_ip6_ep0);
	} else if (ep == 1) {
		rt = rt_ip4_ep1;
		nb_routes = RTE_DIM(rt_ip4_ep1);
		rt6 = rt_ip6_ep1;
		nb_routes6 = RTE_DIM(rt_ip6_ep1);
	} else
		rte_exit(EXIT_FAILURE, "Invalid EP value %u. Only 0 or 1 "
			"supported.\n", ep);

	/* create the LPM table */
	snprintf(name, sizeof(name), "%s_%u", "rt_ip4", socket_id);
	conf.max_rules = RT_IPV4_MAX_RULES;
	conf.number_tbl8s = RTE_LPM_TBL8_NUM_ENTRIES;
	lpm = rte_lpm_create(name, socket_id, &conf);
	if (lpm == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create %s LPM table "
			"on socket %d\n", name, socket_id);

	/* populate the LPM table */
	for (i = 0; i < nb_routes; i++) {
		ret = rte_lpm_add(lpm, rt[i].ip, rt[i].depth, rt[i].if_out);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Fail to add entry num %u to %s "
				"LPM table on socket %d\n", i, name, socket_id);

		uint32_t_to_char(rt[i].ip, &a, &b, &c, &d);
		printf("LPM: Adding route %hhu.%hhu.%hhu.%hhu/%hhu (%hhu)\n",
				a, b, c, d, rt[i].depth, rt[i].if_out);
	}

	snprintf(name, sizeof(name), "%s_%u", "rt_ip6", socket_id);
	conf6.max_rules = RT_IPV6_MAX_RULES;
	conf6.number_tbl8s = RTE_LPM_TBL8_NUM_ENTRIES;
	lpm6 = rte_lpm6_create(name, socket_id, &conf6);
	if (lpm6 == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create %s LPM table "
			"on socket %d\n", name, socket_id);

	/* populate the LPM table */
	for (i = 0; i < nb_routes6; i++) {
		ret = rte_lpm6_add(lpm6, rt6[i].ip, rt6[i].depth,
				rt6[i].if_out);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Fail to add entry num %u to %s "
				"LPM table on socket %d\n", i, name, socket_id);

		printf("LPM6: Adding route "
			" %hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx/%hhx (%hhx)\n",
			(uint16_t)((rt6[i].ip[0] << 8) | rt6[i].ip[1]),
			(uint16_t)((rt6[i].ip[2] << 8) | rt6[i].ip[3]),
			(uint16_t)((rt6[i].ip[4] << 8) | rt6[i].ip[5]),
			(uint16_t)((rt6[i].ip[6] << 8) | rt6[i].ip[7]),
			(uint16_t)((rt6[i].ip[8] << 8) | rt6[i].ip[9]),
			(uint16_t)((rt6[i].ip[10] << 8) | rt6[i].ip[11]),
			(uint16_t)((rt6[i].ip[12] << 8) | rt6[i].ip[13]),
			(uint16_t)((rt6[i].ip[14] << 8) | rt6[i].ip[15]),
			rt6[i].depth, rt6[i].if_out);
	}

	ctx->rt_ip4 = (struct rt_ctx *)lpm;
	ctx->rt_ip6 = (struct rt_ctx *)lpm6;
}
