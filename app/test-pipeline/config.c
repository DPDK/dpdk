/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_string_fns.h>

#include "main.h"

struct app_params app;

static const char usage[] = "\n";

void
app_print_usage(void)
{
	printf(usage);
}

static int
app_parse_port_mask(const char *arg)
{
	char *end = NULL;
	uint64_t port_mask;
	uint32_t i;

	if (arg[0] == '\0')
		return -1;

	port_mask = strtoul(arg, &end, 16);
	if ((end == NULL) || (*end != '\0'))
		return -2;

	if (port_mask == 0)
		return -3;

	app.n_ports = 0;
	for (i = 0; i < 64; i++) {
		if ((port_mask & (1LLU << i)) == 0)
			continue;

		if (app.n_ports >= APP_MAX_PORTS)
			return -4;

		app.ports[app.n_ports] = i;
		app.n_ports++;
	}

	if (!rte_is_power_of_2(app.n_ports))
		return -5;

	return 0;
}

struct {
	const char *name;
	uint32_t value;
} app_args_table[] = {
	{"none", e_APP_PIPELINE_NONE},
	{"stub", e_APP_PIPELINE_STUB},
	{"hash-8-ext", e_APP_PIPELINE_HASH_KEY8_EXT},
	{"hash-8-lru", e_APP_PIPELINE_HASH_KEY8_LRU},
	{"hash-16-ext", e_APP_PIPELINE_HASH_KEY16_EXT},
	{"hash-16-lru", e_APP_PIPELINE_HASH_KEY16_LRU},
	{"hash-32-ext", e_APP_PIPELINE_HASH_KEY32_EXT},
	{"hash-32-lru", e_APP_PIPELINE_HASH_KEY32_LRU},
	{"hash-spec-8-ext", e_APP_PIPELINE_HASH_SPEC_KEY8_EXT},
	{"hash-spec-8-lru", e_APP_PIPELINE_HASH_SPEC_KEY8_LRU},
	{"hash-spec-16-ext", e_APP_PIPELINE_HASH_SPEC_KEY16_EXT},
	{"hash-spec-16-lru", e_APP_PIPELINE_HASH_SPEC_KEY16_LRU},
	{"hash-spec-32-ext", e_APP_PIPELINE_HASH_SPEC_KEY32_EXT},
	{"hash-spec-32-lru", e_APP_PIPELINE_HASH_SPEC_KEY32_LRU},
	{"acl", e_APP_PIPELINE_ACL},
	{"lpm", e_APP_PIPELINE_LPM},
	{"lpm-ipv6", e_APP_PIPELINE_LPM_IPV6},
	{"hash-cuckoo-8", e_APP_PIPELINE_HASH_CUCKOO_KEY8},
	{"hash-cuckoo-16", e_APP_PIPELINE_HASH_CUCKOO_KEY16},
	{"hash-cuckoo-32", e_APP_PIPELINE_HASH_CUCKOO_KEY32},
	{"hash-cuckoo-48", e_APP_PIPELINE_HASH_CUCKOO_KEY48},
	{"hash-cuckoo-64", e_APP_PIPELINE_HASH_CUCKOO_KEY64},
	{"hash-cuckoo-80", e_APP_PIPELINE_HASH_CUCKOO_KEY80},
	{"hash-cuckoo-96", e_APP_PIPELINE_HASH_CUCKOO_KEY96},
	{"hash-cuckoo-112", e_APP_PIPELINE_HASH_CUCKOO_KEY112},
	{"hash-cuckoo-128", e_APP_PIPELINE_HASH_CUCKOO_KEY128},
};

int
app_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{"none", 0, 0, 0},
		{"stub", 0, 0, 0},
		{"hash-8-ext", 0, 0, 0},
		{"hash-8-lru", 0, 0, 0},
		{"hash-16-ext", 0, 0, 0},
		{"hash-16-lru", 0, 0, 0},
		{"hash-32-ext", 0, 0, 0},
		{"hash-32-lru", 0, 0, 0},
		{"hash-spec-8-ext", 0, 0, 0},
		{"hash-spec-8-lru", 0, 0, 0},
		{"hash-spec-16-ext", 0, 0, 0},
		{"hash-spec-16-lru", 0, 0, 0},
		{"hash-spec-32-ext", 0, 0, 0},
		{"hash-spec-32-lru", 0, 0, 0},
		{"acl", 0, 0, 0},
		{"lpm", 0, 0, 0},
		{"lpm-ipv6", 0, 0, 0},
		{"hash-cuckoo-8", 0, 0, 0},
		{"hash-cuckoo-16", 0, 0, 0},
		{"hash-cuckoo-32", 0, 0, 0},
		{"hash-cuckoo-48", 0, 0, 0},
		{"hash-cuckoo-64", 0, 0, 0},
		{"hash-cuckoo-80", 0, 0, 0},
		{"hash-cuckoo-96", 0, 0, 0},
		{"hash-cuckoo-112", 0, 0, 0},
		{"hash-cuckoo-128", 0, 0, 0},
		{NULL, 0, 0, 0}
	};
	uint32_t lcores[3], n_lcores, lcore_id, pipeline_type_provided;

	/* EAL args */
	n_lcores = 0;
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (n_lcores >= 3) {
			RTE_LOG(ERR, USER1, "Number of cores must be 3\n");
			app_print_usage();
			return -1;
		}

		lcores[n_lcores] = lcore_id;
		n_lcores++;
	}

	if (n_lcores != 3) {
		RTE_LOG(ERR, USER1, "Number of cores must be 3\n");
		app_print_usage();
		return -1;
	}

	app.core_rx = lcores[0];
	app.core_worker = lcores[1];
	app.core_tx = lcores[2];

	/* Non-EAL args */
	argvopt = argv;

	app.pipeline_type = e_APP_PIPELINE_HASH_KEY16_LRU;
	pipeline_type_provided = 0;

	while ((opt = getopt_long(argc, argvopt, "p:",
			lgopts, &option_index)) != EOF) {
		switch (opt) {
		case 'p':
			if (app_parse_port_mask(optarg) < 0) {
				app_print_usage();
				return -1;
			}
			break;

		case 0: /* long options */
			if (!pipeline_type_provided) {
				uint32_t i;

				for (i = 0; i < e_APP_PIPELINES; i++) {
					if (!strcmp(lgopts[option_index].name,
						app_args_table[i].name)) {
						app.pipeline_type =
							app_args_table[i].value;
						pipeline_type_provided = 1;
						break;
					}
				}

				break;
			}

			app_print_usage();
			return -1;

		default:
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind - 1] = prgname;

	ret = optind - 1;
	optind = 0; /* reset getopt lib */
	return ret;
}
