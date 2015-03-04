/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_string_fns.h>
#include <rte_cfgfile.h>

#include "main.h"

struct app_params app;

static const char usage[] =
	"Usage: %s EAL_OPTIONS-- -p PORT_MASK [-f CONFIG_FILE]\n";

void
app_print_usage(char *prgname)
{
	printf(usage, prgname);
}

const char *
app_core_type_id_to_string(enum app_core_type id)
{
	switch (id) {
	case APP_CORE_NONE: return "NONE";
	case APP_CORE_MASTER: return "MASTER";
	case APP_CORE_RX: return "RX";
	case APP_CORE_TX: return "TX";
	case APP_CORE_PT: return "PT";
	case APP_CORE_FC: return "FC";
	case APP_CORE_FW: return "FW";
	case APP_CORE_RT: return "RT";
	case APP_CORE_TM: return "TM";
	case APP_CORE_IPV4_FRAG: return "IPV4_FRAG";
	case APP_CORE_IPV4_RAS: return "IPV4_RAS";
	default: return NULL;
	}
}

int
app_core_type_string_to_id(const char *string, enum app_core_type *id)
{
	if (strcmp(string, "NONE") == 0) {
		*id = APP_CORE_NONE;
		return 0;
	}
	if (strcmp(string, "MASTER") == 0) {
		*id = APP_CORE_MASTER;
		return 0;
	}
	if (strcmp(string, "RX") == 0) {
		*id = APP_CORE_RX;
		return 0;
	}
	if (strcmp(string, "TX") == 0) {
		*id = APP_CORE_TX;
		return 0;
	}
	if (strcmp(string, "PT") == 0) {
		*id = APP_CORE_PT;
		return 0;
	}
	if (strcmp(string, "FC") == 0) {
		*id = APP_CORE_FC;
		return 0;
	}
	if (strcmp(string, "FW") == 0) {
		*id = APP_CORE_FW;
		return 0;
	}
	if (strcmp(string, "RT") == 0) {
		*id = APP_CORE_RT;
		return 0;
	}
	if (strcmp(string, "TM") == 0) {
		*id = APP_CORE_TM;
		return 0;
	}
	if (strcmp(string, "IPV4_FRAG") == 0) {
		*id = APP_CORE_IPV4_FRAG;
		return 0;
	}
	if (strcmp(string, "IPV4_RAS") == 0) {
		*id = APP_CORE_IPV4_RAS;
		return 0;
	}

	return -1;
}

static uint64_t
app_get_core_mask(void)
{
	uint64_t core_mask = 0;
	uint32_t i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (rte_lcore_is_enabled(i) == 0)
			continue;

		core_mask |= 1LLU << i;
	}

	return core_mask;
}

static int
app_install_coremask(uint64_t core_mask)
{
	uint32_t n_cores, i;

	for (n_cores = 0, i = 0; i < RTE_MAX_LCORE; i++)
		if (app.cores[i].core_type != APP_CORE_NONE)
			n_cores++;

	if (n_cores != app.n_cores) {
		rte_panic("Number of cores in COREMASK should be %u instead "
			"of %u\n", n_cores, app.n_cores);
		return -1;
	}

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		uint32_t core_id;

		if (app.cores[i].core_type == APP_CORE_NONE)
			continue;

		core_id = __builtin_ctzll(core_mask);
		core_mask &= ~(1LLU << core_id);

		app.cores[i].core_id = core_id;
	}

	return 0;
}
static int
app_install_cfgfile(const char *file_name)
{
	struct rte_cfgfile *file;
	uint32_t n_cores, i;

	memset(app.cores, 0, sizeof(app.cores));

	if (file_name[0] == '\0')
		return -1;

	file = rte_cfgfile_load(file_name, 0);
	if (file == NULL) {
		rte_panic("Config file %s not found\n", file_name);
		return -1;
	}

	n_cores = (uint32_t) rte_cfgfile_num_sections(file, "core",
		strnlen("core", 5));
	if (n_cores < app.n_cores) {
		rte_panic("Config file parse error: not enough cores specified "
			"(%u cores missing)\n", app.n_cores - n_cores);
		return -1;
	}
	if (n_cores > app.n_cores) {
		rte_panic("Config file parse error: too many cores specified "
			"(%u cores too many)\n", n_cores - app.n_cores);
		return -1;
	}

	for (i = 0; i < n_cores; i++) {
		struct app_core_params *p = &app.cores[i];
		char section_name[16];
		const char *entry;
		uint32_t j;

		/* [core X] */
		snprintf(section_name, sizeof(section_name), "core %u", i);
		if (!rte_cfgfile_has_section(file, section_name)) {
			rte_panic("Config file parse error: core IDs are not "
				"sequential (core %u missing)\n", i);
			return -1;
		}

		/* type */
		entry = rte_cfgfile_get_entry(file, section_name, "type");
		if (!entry) {
			rte_panic("Config file parse error: core %u type not "
				"defined\n", i);
			return -1;
		}
		if ((app_core_type_string_to_id(entry, &p->core_type) != 0) ||
		    (p->core_type == APP_CORE_NONE)) {
			rte_panic("Config file parse error: core %u type "
				"error\n", i);
			return -1;
		}

		/* queues in */
		entry = rte_cfgfile_get_entry(file, section_name, "queues in");
		if (!entry) {
			rte_panic("Config file parse error: core %u queues in "
				"not defined\n", i);
			return -1;
		}

		for (j = 0; (j < APP_MAX_SWQ_PER_CORE) && (entry != NULL);
			j++) {
			char *next;

			p->swq_in[j] =  (uint32_t) strtol(entry, &next, 10);
			if (next == entry)
				break;
			entry = next;
		}

		if ((j != APP_MAX_SWQ_PER_CORE) || (*entry != '\0')) {
			rte_panic("Config file parse error: core %u queues in "
				"error\n", i);
			return -1;
		}

		/* queues out */
		entry = rte_cfgfile_get_entry(file, section_name, "queues out");
		if (!entry) {
			rte_panic("Config file parse error: core %u queues out "
				"not defined\n", i);
			return -1;
		}

		for (j = 0; (j < APP_MAX_SWQ_PER_CORE) && (entry != NULL);
			j++) {
			char *next;

			p->swq_out[j] =  (uint32_t) strtol(entry, &next, 10);
			if (next == entry)
				break;
			entry = next;
		}
		if ((j != APP_MAX_SWQ_PER_CORE) || (*entry != '\0')) {
			rte_panic("Config file parse error: core %u queues out "
				"error\n", i);
			return -1;
		}
	}

	rte_cfgfile_close(file);

	return 0;
}

void app_cores_config_print(void)
{
	uint32_t i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct app_core_params *p = &app.cores[i];
		uint32_t j;

		if (app.cores[i].core_type == APP_CORE_NONE)
			continue;

		printf("---> core %u: id = %u type = %6s [", i, p->core_id,
			app_core_type_id_to_string(p->core_type));
		for (j = 0; j < APP_MAX_SWQ_PER_CORE; j++)
			printf("%2d ", (int) p->swq_in[j]);

		printf("] [");
		for (j = 0; j < APP_MAX_SWQ_PER_CORE; j++)
			printf("%2d ", (int) p->swq_out[j]);

		printf("]\n");
	}
}

static int
app_install_port_mask(const char *arg)
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

int
app_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};
	uint64_t core_mask = app_get_core_mask();

	app.n_cores = __builtin_popcountll(core_mask);

	argvopt = argv;
	while ((opt = getopt_long(argc, argvopt, "p:f:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case 'p':
			if (app_install_port_mask(optarg) != 0)
				rte_panic("PORT_MASK should specify a number "
					"of ports that is power of 2 less or "
					"equal to %u\n", APP_MAX_PORTS);
			break;

		case 'f':
			app_install_cfgfile(optarg);
			break;

		default:
			return -1;
		}
	}

	app_install_coremask(core_mask);

	app_cores_config_print();

	if (optind >= 0)
		argv[optind - 1] = prgname;

	ret = optind - 1;
	optind = 0; /* reset getopt lib */

	return ret;
}
