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
#include "parser.h"

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

struct ip4_route rt_ip4[RT_IPV4_MAX_RULES];
uint32_t nb_rt_ip4;

struct ip6_route rt_ip6[RT_IPV4_MAX_RULES];
uint32_t nb_rt_ip6;

void
parse_rt_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	uint32_t ti;
	uint32_t *n_rts = NULL;
	struct ip4_route *route_ipv4 = NULL;
	struct ip6_route *route_ipv6 = NULL;

	if (strcmp(tokens[0], "ipv4") == 0) {
		n_rts = &nb_rt_ip4;
		route_ipv4 = &rt_ip4[*n_rts];

		APP_CHECK(*n_rts <= RT_IPV4_MAX_RULES - 1, status,
			"too many rt rules, abort insertion\n");
		if (status->status < 0)
			return;

	} else if (strcmp(tokens[0], "ipv6") == 0) {
		n_rts = &nb_rt_ip6;
		route_ipv6 = &rt_ip6[*n_rts];

		APP_CHECK(*n_rts <= RT_IPV6_MAX_RULES - 1, status,
			"too many rt rules, abort insertion\n");
		if (status->status < 0)
			return;
	} else {
		APP_CHECK(0, status, "unrecognized input \"%s\"",
			tokens[0]);
		return;
	}

	for (ti = 1; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "dst") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (route_ipv4 != NULL) {
				struct in_addr ip;
				uint32_t depth = 0;

				APP_CHECK(parse_ipv4_addr(tokens[ti],
					&ip, &depth) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv4 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				route_ipv4->ip = rte_bswap32(
					(uint32_t)ip.s_addr);
				route_ipv4->depth = (uint8_t)depth;
			} else {
				struct in6_addr ip;
				uint32_t depth;

				APP_CHECK(parse_ipv6_addr(tokens[ti],
					&ip, &depth) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv6 address",
					tokens[ti]);
				if (status->status < 0)
					return;
				memcpy(route_ipv6->ip, ip.s6_addr, 16);
				route_ipv6->depth = (uint8_t)depth;
			}
		}

		if (strcmp(tokens[ti], "port") == 0) {
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;
			if (route_ipv4 != NULL)
				route_ipv4->if_out = atoi(tokens[ti]);
			else
				route_ipv6->if_out = atoi(tokens[ti]);
		}
	}

	*n_rts = *n_rts + 1;
}

void
rt_init(struct socket_ctx *ctx, int32_t socket_id)
{
	char name[PATH_MAX];
	uint32_t i;
	int32_t ret;
	struct rte_lpm *lpm;
	struct rte_lpm6 *lpm6;
	char a, b, c, d;
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

	if (nb_rt_ip4 == 0 && nb_rt_ip6 == 0)
		RTE_LOG(WARNING, IPSEC, "No Routing rule specified\n");

	printf("Creating IPv4 Routing Table (RT) context with %u max routes\n",
			RT_IPV4_MAX_RULES);

	/* create the LPM table */
	snprintf(name, sizeof(name), "%s_%u", "rt_ip4", socket_id);
	conf.max_rules = RT_IPV4_MAX_RULES;
	conf.number_tbl8s = RTE_LPM_TBL8_NUM_ENTRIES;
	lpm = rte_lpm_create(name, socket_id, &conf);
	if (lpm == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create %s LPM table "
			"on socket %d\n", name, socket_id);

	/* populate the LPM table */
	for (i = 0; i < nb_rt_ip4; i++) {
		ret = rte_lpm_add(lpm, rt_ip4[i].ip, rt_ip4[i].depth,
			rt_ip4[i].if_out);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Fail to add entry num %u to %s "
				"LPM table on socket %d\n", i, name, socket_id);

		uint32_t_to_char(rt_ip4[i].ip, &a, &b, &c, &d);
		printf("LPM: Adding route %hhu.%hhu.%hhu.%hhu/%hhu (%hhu)\n",
				a, b, c, d, rt_ip4[i].depth,
				rt_ip4[i].if_out);
	}

	snprintf(name, sizeof(name), "%s_%u", "rt_ip6", socket_id);
	conf6.max_rules = RT_IPV6_MAX_RULES;
	conf6.number_tbl8s = RTE_LPM_TBL8_NUM_ENTRIES;
	lpm6 = rte_lpm6_create(name, socket_id, &conf6);
	if (lpm6 == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create %s LPM table "
			"on socket %d\n", name, socket_id);

	/* populate the LPM table */
	for (i = 0; i < nb_rt_ip6; i++) {
		ret = rte_lpm6_add(lpm6, rt_ip6[i].ip, rt_ip6[i].depth,
				rt_ip6[i].if_out);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Fail to add entry num %u to %s "
				"LPM table on socket %d\n", i, name, socket_id);

		printf("LPM6: Adding route "
			" %hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx/%hhx (%hhx)\n",
			(uint16_t)((rt_ip6[i].ip[0] << 8) | rt_ip6[i].ip[1]),
			(uint16_t)((rt_ip6[i].ip[2] << 8) | rt_ip6[i].ip[3]),
			(uint16_t)((rt_ip6[i].ip[4] << 8) | rt_ip6[i].ip[5]),
			(uint16_t)((rt_ip6[i].ip[6] << 8) | rt_ip6[i].ip[7]),
			(uint16_t)((rt_ip6[i].ip[8] << 8) | rt_ip6[i].ip[9]),
			(uint16_t)((rt_ip6[i].ip[10] << 8) | rt_ip6[i].ip[11]),
			(uint16_t)((rt_ip6[i].ip[12] << 8) | rt_ip6[i].ip[13]),
			(uint16_t)((rt_ip6[i].ip[14] << 8) | rt_ip6[i].ip[15]),
			rt_ip6[i].depth, rt_ip6[i].if_out);
	}

	ctx->rt_ip4 = (struct rt_ctx *)lpm;
	ctx->rt_ip6 = (struct rt_ctx *)lpm6;
}
