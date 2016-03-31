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
#include <rte_errno.h>

#include "ipsec.h"

#define RT_IPV4_MAX_RULES         64

struct ipv4_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

/* In the default routing table we have:
 * ep0 protected ports 0 and 1, and unprotected ports 2 and 3.
 */
static struct ipv4_route rt_ipv4_ep0[] = {
	{ IPv4(172, 16, 2, 5), 32, 0 },
	{ IPv4(172, 16, 2, 6), 32, 0 },
	{ IPv4(172, 16, 2, 7), 32, 1 },
	{ IPv4(172, 16, 2, 8), 32, 1 },

	{ IPv4(192, 168, 115, 0), 24, 2 },
	{ IPv4(192, 168, 116, 0), 24, 2 },
	{ IPv4(192, 168, 117, 0), 24, 3 },
	{ IPv4(192, 168, 118, 0), 24, 3 },

	{ IPv4(192, 168, 210, 0), 24, 2 },

	{ IPv4(192, 168, 240, 0), 24, 2 },
	{ IPv4(192, 168, 250, 0), 24, 0 }
};

/* In the default routing table we have:
 * ep1 protected ports 0 and 1, and unprotected ports 2 and 3.
 */
static struct ipv4_route rt_ipv4_ep1[] = {
	{ IPv4(172, 16, 1, 5), 32, 2 },
	{ IPv4(172, 16, 1, 6), 32, 2 },
	{ IPv4(172, 16, 1, 7), 32, 3 },
	{ IPv4(172, 16, 1, 8), 32, 3 },

	{ IPv4(192, 168, 105, 0), 24, 0 },
	{ IPv4(192, 168, 106, 0), 24, 0 },
	{ IPv4(192, 168, 107, 0), 24, 1 },
	{ IPv4(192, 168, 108, 0), 24, 1 },

	{ IPv4(192, 168, 200, 0), 24, 0 },

	{ IPv4(192, 168, 240, 0), 24, 2 },
	{ IPv4(192, 168, 250, 0), 24, 0 }
};

void
rt_init(struct socket_ctx *ctx, int socket_id, unsigned ep)
{
	char name[PATH_MAX];
	unsigned i;
	int ret;
	struct rte_lpm *lpm;
	struct ipv4_route *rt;
	char a, b, c, d;
	unsigned nb_routes;
	struct rte_lpm_config conf = { 0 };

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->rt_ipv4 != NULL)
		rte_exit(EXIT_FAILURE, "Routing Table for socket %u already "
			"initialized\n", socket_id);

	printf("Creating Routing Table (RT) context with %u max routes\n",
			RT_IPV4_MAX_RULES);

	if (ep == 0) {
		rt = rt_ipv4_ep0;
		nb_routes = RTE_DIM(rt_ipv4_ep0);
	} else if (ep == 1) {
		rt = rt_ipv4_ep1;
		nb_routes = RTE_DIM(rt_ipv4_ep1);
	} else
		rte_exit(EXIT_FAILURE, "Invalid EP value %u. Only 0 or 1 "
			"supported.\n", ep);

	/* create the LPM table */
	snprintf(name, sizeof(name), "%s_%u", "rt_ipv4", socket_id);
	conf.max_rules = RT_IPV4_MAX_RULES;
	conf.number_tbl8s = RTE_LPM_TBL8_NUM_ENTRIES;
	lpm = rte_lpm_create(name, socket_id, &conf);
	if (lpm == NULL)
		rte_exit(EXIT_FAILURE, "Unable to create LPM table "
			"on socket %d\n", socket_id);

	/* populate the LPM table */
	for (i = 0; i < nb_routes; i++) {
		ret = rte_lpm_add(lpm, rt[i].ip, rt[i].depth, rt[i].if_out);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry num %u to "
				"LPM table on socket %d\n", i, socket_id);

		uint32_t_to_char(rt[i].ip, &a, &b, &c, &d);
		printf("LPM: Adding route %hhu.%hhu.%hhu.%hhu/%hhu (%hhu)\n",
				a, b, c, d, rt[i].depth, rt[i].if_out);
	}

	ctx->rt_ipv4 = (struct rt_ctx *)lpm;
}
