/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include <rte_errno.h>
#include <rte_cfgfile.h>
#include <rte_string_fns.h>

#include "app.h"

/**
 * Default config values
 **/

static struct app_params app_params_default = {
	.config_file = "./config/ip_pipeline.cfg",
	.log_level = APP_LOG_LEVEL_HIGH,

	.eal_params = {
		.channels = 4,
	},
};

static const struct app_mempool_params mempool_params_default = {
	.parsed = 0,
	.buffer_size = 2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM,
	.pool_size = 32 * 1024,
	.cache_size = 256,
	.cpu_socket_id = 0,
};

static const struct app_link_params link_params_default = {
	.parsed = 0,
	.pmd_id = 0,
	.arp_q = 0,
	.tcp_syn_local_q = 0,
	.ip_local_q = 0,
	.tcp_local_q = 0,
	.udp_local_q = 0,
	.sctp_local_q = 0,
	.state = 0,
	.ip = 0,
	.depth = 0,
	.mac_addr = 0,

	.conf = {
		.link_speed = 0,
		.link_duplex = 0,
		.rxmode = {
			.mq_mode = ETH_MQ_RX_NONE,

			.header_split   = 0, /* Header split */
			.hw_ip_checksum = 0, /* IP checksum offload */
			.hw_vlan_filter = 0, /* VLAN filtering */
			.hw_vlan_strip  = 0, /* VLAN strip */
			.hw_vlan_extend = 0, /* Extended VLAN */
			.jumbo_frame    = 0, /* Jumbo frame support */
			.hw_strip_crc   = 0, /* CRC strip by HW */
			.enable_scatter = 0, /* Scattered packets RX handler */

			.max_rx_pkt_len = 9000, /* Jumbo frame max packet len */
			.split_hdr_size = 0, /* Header split buffer size */
		},
		.txmode = {
			.mq_mode = ETH_MQ_TX_NONE,
		},
		.lpbk_mode = 0,
	},

	.promisc = 1,
};

static const struct app_pktq_hwq_in_params default_hwq_in_params = {
	.parsed = 0,
	.mempool_id = 0,
	.size = 128,
	.burst = 32,

	.conf = {
		.rx_thresh = {
				.pthresh = 8,
				.hthresh = 8,
				.wthresh = 4,
		},
		.rx_free_thresh = 64,
		.rx_drop_en = 0,
		.rx_deferred_start = 0,
	}
};

static const struct app_pktq_hwq_out_params default_hwq_out_params = {
	.parsed = 0,
	.size = 512,
	.burst = 32,
	.dropless = 0,
	.n_retries = 0,

	.conf = {
		.tx_thresh = {
			.pthresh = 36,
			.hthresh = 0,
			.wthresh = 0,
		},
		.tx_rs_thresh = 0,
		.tx_free_thresh = 0,
		.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS |
			ETH_TXQ_FLAGS_NOOFFLOADS,
		.tx_deferred_start = 0,
	}
};

static const struct app_pktq_swq_params default_swq_params = {
	.parsed = 0,
	.size = 256,
	.burst_read = 32,
	.burst_write = 32,
	.dropless = 0,
	.n_retries = 0,
	.cpu_socket_id = 0,
};

struct app_pktq_tm_params default_tm_params = {
	.parsed = 0,
	.file_name = "./config/tm_profile.cfg",
	.burst_read = 64,
	.burst_write = 32,
};

struct app_pktq_source_params default_source_params = {
	.parsed = 0,
	.mempool_id = 0,
	.burst = 32,
};

struct app_pktq_sink_params default_sink_params = {
	.parsed = 0,
};

struct app_msgq_params default_msgq_params = {
	.parsed = 0,
	.size = 64,
	.cpu_socket_id = 0,
};

struct app_pipeline_params default_pipeline_params = {
	.parsed = 0,
	.socket_id = 0,
	.core_id = 0,
	.hyper_th_id = 0,
	.n_pktq_in = 0,
	.n_pktq_out = 0,
	.n_msgq_in = 0,
	.n_msgq_out = 0,
	.timer_period = 1,
	.n_args = 0,
};

static const char app_usage[] =
	"Usage: %s [-f CONFIG_FILE] [-s SCRIPT_FILE] -p PORT_MASK "
	"[-l LOG_LEVEL]\n"
	"\n"
	"Arguments:\n"
	"\t-f CONFIG_FILE: Default config file is %s\n"
	"\t-p PORT_MASK: Mask of NIC port IDs in hexadecimal format\n"
	"\t-s SCRIPT_FILE: No CLI script file is run when not specified\n"
	"\t-l LOG_LEVEL: 0 = NONE, 1 = HIGH PRIO (default), 2 = LOW PRIO\n"
	"\n";

static void
app_print_usage(char *prgname)
{
	rte_exit(0, app_usage, prgname, app_params_default.config_file);
}

#define skip_white_spaces(pos)			\
({						\
	__typeof__(pos) _p = (pos);		\
	for ( ; isspace(*_p); _p++);		\
	_p;					\
})

#define PARSER_IMPLICIT_PARAM_ADD_CHECK(result, section_name)		\
do {									\
	APP_CHECK((result != -EINVAL),					\
		"CFG: [%s] name too long", section_name);		\
	APP_CHECK(result != -ENOMEM,					\
		"CFG: [%s] too much sections", section_name);		\
	APP_CHECK(result >= 0,						\
		"CFG: [%s] Unknown error while adding '%s'",		\
		section_name, section_name);				\
} while (0)

#define PARSER_PARAM_ADD_CHECK(result, params_array, section_name)	\
do {									\
	APP_CHECK((result != -EINVAL),					\
		"CFG: [%s] name too long", section_name);		\
	APP_CHECK((result != -ENOMEM),					\
		"CFG: [%s] too much sections", section_name);		\
	APP_CHECK(((result >= 0) && (params_array)[result].parsed == 0),\
		"CFG: [%s] duplicate section", section_name);		\
	APP_CHECK((result >= 0),					\
		"CFG: [%s] Unknown error while adding '%s'",		\
		section_name, section_name);				\
} while (0)

static int
parser_read_arg_bool(const char *p)
{
	p = skip_white_spaces(p);
	int result = -EINVAL;

	if (((p[0] == 'y') && (p[1] == 'e') && (p[2] == 's')) ||
		((p[0] == 'Y') && (p[1] == 'E') && (p[2] == 'S'))) {
		p += 3;
		result = 1;
	}

	if (((p[0] == 'o') && (p[1] == 'n')) ||
		((p[0] == 'O') && (p[1] == 'N'))) {
		p += 2;
		result = 1;
	}

	if (((p[0] == 'n') && (p[1] == 'o')) ||
		((p[0] == 'N') && (p[1] == 'O'))) {
		p += 2;
		result = 0;
	}

	if (((p[0] == 'o') && (p[1] == 'f') && (p[2] == 'f')) ||
		((p[0] == 'O') && (p[1] == 'F') && (p[2] == 'F'))) {
		p += 3;
		result = 0;
	}

	p = skip_white_spaces(p);

	if (p[0] != '\0')
		return -EINVAL;

	return result;
}

#define PARSE_ERROR(exp, section, entry)				\
APP_CHECK(exp, "Parse error in section \"%s\": entry \"%s\"\n", section, entry)

#define PARSE_ERROR_MALLOC(exp)						\
APP_CHECK(exp, "Parse error: no free memory\n")

#define PARSE_ERROR_SECTION(exp, section)				\
APP_CHECK(exp, "Parse error in section \"%s\"", section)

#define PARSE_ERROR_SECTION_NO_ENTRIES(exp, section)			\
APP_CHECK(exp, "Parse error in section \"%s\": no entries\n", section)

#define PARSE_WARNING_IGNORED(exp, section, entry)			\
do									\
if (!(exp))								\
	fprintf(stderr, "Parse warning in section \"%s\": "		\
		"entry \"%s\" is ignored\n", section, entry);		\
while (0)

#define PARSE_ERROR_INVALID(exp, section, entry)			\
APP_CHECK(exp, "Parse error in section \"%s\": unrecognized entry \"%s\"\n",\
	section, entry)

#define PARSE_ERROR_DUPLICATE(exp, section, entry)			\
APP_CHECK(exp, "Parse error in section \"%s\": duplicate entry \"%s\"\n",\
	section, entry)

static int
parser_read_uint64(uint64_t *value, const char *p)
{
	char *next;
	uint64_t val;

	p = skip_white_spaces(p);
	if (!isdigit(*p))
		return -EINVAL;

	val = strtoul(p, &next, 10);
	if (p == next)
		return -EINVAL;

	p = next;
	switch (*p) {
	case 'T':
		val *= 1024ULL;
		/* fall trought */
	case 'G':
		val *= 1024ULL;
		/* fall trought */
	case 'M':
		val *= 1024ULL;
		/* fall trought */
	case 'k':
	case 'K':
		val *= 1024ULL;
		p++;
		break;
	}

	p = skip_white_spaces(p);
	if (*p != '\0')
		return -EINVAL;

	*value = val;
	return 0;
}

static int
parser_read_uint32(uint32_t *value, const char *p)
{
	uint64_t val = 0;
	int ret = parser_read_uint64(&val, p);

	if (ret < 0)
		return ret;
	else if (val > UINT32_MAX)
		return -ERANGE;

	*value = val;
	return 0;
}

static int
parse_pipeline_core(uint32_t *socket,
	uint32_t *core,
	uint32_t *ht,
	const char *entry)
{
	size_t num_len;
	char num[8];

	uint32_t s = 0, c = 0, h = 0, val;
	uint8_t s_parsed = 0, c_parsed = 0, h_parsed = 0;
	const char *next = skip_white_spaces(entry);
	char type;

	/* Expect <CORE> or [sX][cY][h]. At least one parameter is required. */
	while (*next != '\0') {
		/* If everything parsed nothing should left */
		if (s_parsed && c_parsed && h_parsed)
			return -EINVAL;

		type = *next;
		switch (type) {
		case 's':
		case 'S':
			if (s_parsed || c_parsed || h_parsed)
				return -EINVAL;
			s_parsed = 1;
			next++;
			break;
		case 'c':
		case 'C':
			if (c_parsed || h_parsed)
				return -EINVAL;
			c_parsed = 1;
			next++;
			break;
		case 'h':
		case 'H':
			if (h_parsed)
				return -EINVAL;
			h_parsed = 1;
			next++;
			break;
		default:
			/* If it start from digit it must be only core id. */
			if (!isdigit(*next) || s_parsed || c_parsed || h_parsed)
				return -EINVAL;

			type = 'C';
		}

		for (num_len = 0; *next != '\0'; next++, num_len++) {
			if (num_len == RTE_DIM(num))
				return -EINVAL;

			if (!isdigit(*next))
				break;

			num[num_len] = *next;
		}

		if (num_len == 0 && type != 'h' && type != 'H')
			return -EINVAL;

		if (num_len != 0 && (type == 'h' || type == 'H'))
			return -EINVAL;

		num[num_len] = '\0';
		val = strtol(num, NULL, 10);

		h = 0;
		switch (type) {
		case 's':
		case 'S':
			s = val;
			break;
		case 'c':
		case 'C':
			c = val;
			break;
		case 'h':
		case 'H':
			h = 1;
			break;
		}
	}

	*socket = s;
	*core = c;
	*ht = h;
	return 0;
}

static size_t
skip_digits(const char *src)
{
	size_t i;

	for (i = 0; isdigit(src[i]); i++);

	return i;
}

static int
validate_name(const char *name, const char *prefix, int num)
{
	size_t i, j;

	for (i = 0; (name[i] != '\0') && (prefix[i] != '\0'); i++) {
		if (name[i] != prefix[i])
			return -1;
	}

	if (prefix[i] != '\0')
		return -1;

	if (!num) {
		if (name[i] != '\0')
			return -1;
		else
			return 0;
	}

	if (num == 2) {
		j = skip_digits(&name[i]);
		i += j;
		if ((j == 0) || (name[i] != '.'))
			return -1;
		i++;
	}

	if (num == 1) {
		j = skip_digits(&name[i]);
		i += j;
		if ((j == 0) || (name[i] != '\0'))
			return -1;
	}

	return 0;
}

static void
parse_eal(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_eal_params *p = &app->eal_params;
	struct rte_cfgfile_entry *entries;
	int n_entries, i;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *entry = &entries[i];

		/* coremask */
		if (strcmp(entry->name, "c") == 0) {
			PARSE_WARNING_IGNORED(0, section_name, entry->name);
			continue;
		}

		/* corelist */
		if (strcmp(entry->name, "l") == 0) {
			PARSE_WARNING_IGNORED(0, section_name, entry->name);
			continue;
		}

		/* coremap */
		if (strcmp(entry->name, "lcores") == 0) {
			PARSE_ERROR_DUPLICATE((p->coremap == NULL),
				section_name,
				entry->name);
			p->coremap = strdup(entry->value);
			continue;
		}

		/* master_lcore */
		if (strcmp(entry->name, "master_lcore") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((p->master_lcore_present == 0),
				section_name,
				entry->name);
			p->master_lcore_present = 1;

			status = parser_read_uint32(&p->master_lcore,
				entry->value);
			PARSE_ERROR((status == 0), section_name, entry->name);
			continue;
		}

		/* channels */
		if (strcmp(entry->name, "n") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((p->channels_present == 0),
				section_name,
				entry->name);
			p->channels_present = 1;

			status = parser_read_uint32(&p->channels, entry->value);
			PARSE_ERROR((status == 0), section_name, entry->name);
			continue;
		}

		/* memory */
		if (strcmp(entry->name, "m") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((p->memory_present == 0),
				section_name,
				entry->name);
			p->memory_present = 1;

			status = parser_read_uint32(&p->memory, entry->value);
			PARSE_ERROR((status == 0), section_name, entry->name);
			continue;
		}

		/* ranks */
		if (strcmp(entry->name, "r") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((p->ranks_present == 0),
				section_name,
				entry->name);
			p->ranks_present = 1;

			status = parser_read_uint32(&p->ranks, entry->value);
			PARSE_ERROR((status == 0), section_name, entry->name);
			continue;
		}

		/* pci_blacklist */
		if ((strcmp(entry->name, "pci_blacklist") == 0) ||
			(strcmp(entry->name, "b") == 0)) {
			PARSE_ERROR_DUPLICATE((p->pci_blacklist == NULL),
				section_name,
				entry->name);
			p->pci_blacklist = strdup(entry->value);
			continue;
		}

		/* pci_whitelist */
		if ((strcmp(entry->name, "pci_whitelist") == 0) ||
			(strcmp(entry->name, "w") == 0)) {
			PARSE_ERROR_DUPLICATE((p->pci_whitelist == NULL),
				section_name,
				entry->name);
			p->pci_whitelist = strdup(entry->value);
			continue;
		}

		/* vdev */
		if (strcmp(entry->name, "vdev") == 0) {
			PARSE_ERROR_DUPLICATE((p->vdev == NULL),
				section_name,
				entry->name);
			p->vdev = strdup(entry->value);
			continue;
		}

		/* vmware_tsc_map */
		if (strcmp(entry->name, "vmware_tsc_map") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->vmware_tsc_map_present == 0),
				section_name,
				entry->name);
			p->vmware_tsc_map_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->vmware_tsc_map = val;
			continue;
		}

		/* proc_type */
		if (strcmp(entry->name, "proc_type") == 0) {
			PARSE_ERROR_DUPLICATE((p->proc_type == NULL),
				section_name,
				entry->name);
			p->proc_type = strdup(entry->value);
			continue;
		}

		/* syslog */
		if (strcmp(entry->name, "syslog") == 0) {
			PARSE_ERROR_DUPLICATE((p->syslog == NULL),
				section_name,
				entry->name);
			p->syslog = strdup(entry->value);
			continue;
		}

		/* log_level */
		if (strcmp(entry->name, "log_level") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((p->log_level_present == 0),
				section_name,
				entry->name);
			p->log_level_present = 1;

			status = parser_read_uint32(&p->log_level,
				entry->value);
			PARSE_ERROR((status == 0), section_name, entry->name);
			continue;
		}

		/* version */
		if (strcmp(entry->name, "v") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->version_present == 0),
				section_name,
				entry->name);
			p->version_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->version = val;
			continue;
		}

		/* help */
		if ((strcmp(entry->name, "help") == 0) ||
			(strcmp(entry->name, "h") == 0)) {
			int val;

			PARSE_ERROR_DUPLICATE((p->help_present == 0),
				section_name,
				entry->name);
			p->help_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->help = val;
			continue;
		}

		/* no_huge */
		if (strcmp(entry->name, "no_huge") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->no_huge_present == 0),
				section_name,
				entry->name);
			p->no_huge_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->no_huge = val;
			continue;
		}

		/* no_pci */
		if (strcmp(entry->name, "no_pci") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->no_pci_present == 0),
				section_name,
				entry->name);
			p->no_pci_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->no_pci = val;
			continue;
		}

		/* no_hpet */
		if (strcmp(entry->name, "no_hpet") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->no_hpet_present == 0),
				section_name,
				entry->name);
			p->no_hpet_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->no_hpet = val;
			continue;
		}

		/* no_shconf */
		if (strcmp(entry->name, "no_shconf") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->no_shconf_present == 0),
				section_name,
				entry->name);
			p->no_shconf_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->no_shconf = val;
			continue;
		}

		/* add_driver */
		if (strcmp(entry->name, "d") == 0) {
			PARSE_ERROR_DUPLICATE((p->add_driver == NULL),
				section_name,
				entry->name);
			p->add_driver = strdup(entry->value);
			continue;
		}

		/* socket_mem */
		if (strcmp(entry->name, "socket_mem") == 0) {
			PARSE_ERROR_DUPLICATE((p->socket_mem == NULL),
				section_name,
				entry->name);
			p->socket_mem = strdup(entry->value);
			continue;
		}

		/* huge_dir */
		if (strcmp(entry->name, "huge_dir") == 0) {
			PARSE_ERROR_DUPLICATE((p->huge_dir == NULL),
				section_name,
				entry->name);
			p->huge_dir = strdup(entry->value);
			continue;
		}

		/* file_prefix */
		if (strcmp(entry->name, "file_prefix") == 0) {
			PARSE_ERROR_DUPLICATE((p->file_prefix == NULL),
				section_name,
				entry->name);
			p->file_prefix = strdup(entry->value);
			continue;
		}

		/* base_virtaddr */
		if (strcmp(entry->name, "base_virtaddr") == 0) {
			PARSE_ERROR_DUPLICATE((p->base_virtaddr == NULL),
				section_name,
				entry->name);
			p->base_virtaddr = strdup(entry->value);
			continue;
		}

		/* create_uio_dev */
		if (strcmp(entry->name, "create_uio_dev") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->create_uio_dev_present == 0),
				section_name,
				entry->name);
			p->create_uio_dev_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->create_uio_dev = val;
			continue;
		}

		/* vfio_intr */
		if (strcmp(entry->name, "vfio_intr") == 0) {
			PARSE_ERROR_DUPLICATE((p->vfio_intr == NULL),
				section_name,
				entry->name);
			p->vfio_intr = strdup(entry->value);
			continue;
		}

		/* xen_dom0 */
		if (strcmp(entry->name, "xen_dom0") == 0) {
			int val;

			PARSE_ERROR_DUPLICATE((p->xen_dom0_present == 0),
				section_name,
				entry->name);
			p->xen_dom0_present = 1;

			val = parser_read_arg_bool(entry->value);
			PARSE_ERROR((val >= 0), section_name, entry->name);
			p->xen_dom0 = val;
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, entry->name);
	}

	free(entries);
}

static int
parse_pipeline_pktq_in(struct app_params *app,
	struct app_pipeline_params *p,
	const char *value)
{
	const char *next = value;
	char *end;
	char name[APP_PARAM_NAME_SIZE];
	size_t name_len;

	while (*next != '\0') {
		enum app_pktq_in_type type;
		int id;

		end = strchr(next, ' ');
		if (!end)
			name_len = strlen(next);
		else
			name_len = end - next;

		if (name_len == 0 || name_len == sizeof(name))
			return -EINVAL;

		strncpy(name, next, name_len);
		name[name_len] = '\0';
		next += name_len;
		if (*next != '\0')
			next++;

		if (validate_name(name, "RXQ", 2) == 0) {
			type = APP_PKTQ_IN_HWQ;
			id = APP_PARAM_ADD(app->hwq_in_params, name);
		} else if (validate_name(name, "SWQ", 1) == 0) {
			type = APP_PKTQ_IN_SWQ;
			id = APP_PARAM_ADD(app->swq_params, name);
		} else if (validate_name(name, "TM", 1) == 0) {
			type = APP_PKTQ_IN_TM;
			id = APP_PARAM_ADD(app->tm_params, name);
		} else if (validate_name(name, "SOURCE", 1) == 0) {
			type = APP_PKTQ_IN_SOURCE;
			id = APP_PARAM_ADD(app->source_params, name);
		} else
			return -EINVAL;

		if (id < 0)
			return id;

		p->pktq_in[p->n_pktq_in].type = type;
		p->pktq_in[p->n_pktq_in].id = (uint32_t) id;
		p->n_pktq_in++;
	}

	return 0;
}

static int
parse_pipeline_pktq_out(struct app_params *app,
	struct app_pipeline_params *p,
	const char *value)
{
	const char *next = value;
	char *end;
	char name[APP_PARAM_NAME_SIZE];
	size_t name_len;

	while (*next != '\0') {
		enum app_pktq_out_type type;
		int id;

		end = strchr(next, ' ');
		if (!end)
			name_len = strlen(next);
		else
			name_len = end - next;

		if (name_len == 0 || name_len == sizeof(name))
			return -EINVAL;

		strncpy(name, next, name_len);
		name[name_len] = '\0';
		next += name_len;
		if (*next != '\0')
			next++;

		if (validate_name(name, "TXQ", 2) == 0) {
			type = APP_PKTQ_OUT_HWQ;
			id = APP_PARAM_ADD(app->hwq_out_params, name);
		} else if (validate_name(name, "SWQ", 1) == 0) {
			type = APP_PKTQ_OUT_SWQ;
			id = APP_PARAM_ADD(app->swq_params, name);
		} else if (validate_name(name, "TM", 1) == 0) {
			type = APP_PKTQ_OUT_TM;
			id = APP_PARAM_ADD(app->tm_params, name);
		} else if (validate_name(name, "SINK", 1) == 0) {
			type = APP_PKTQ_OUT_SINK;
			id = APP_PARAM_ADD(app->sink_params, name);
		} else
			return -EINVAL;

		if (id < 0)
			return id;

		p->pktq_out[p->n_pktq_out].type = type;
		p->pktq_out[p->n_pktq_out].id = id;
		p->n_pktq_out++;
	}

	return 0;
}

static int
parse_pipeline_msgq_in(struct app_params *app,
	struct app_pipeline_params *p,
	const char *value)
{
	const char *next = value;
	char *end;
	char name[APP_PARAM_NAME_SIZE];
	size_t name_len;
	ssize_t idx;

	while (*next != '\0') {
		end = strchr(next, ' ');
		if (!end)
			name_len = strlen(next);
		else
			name_len = end - next;

		if (name_len == 0 || name_len == sizeof(name))
			return -EINVAL;

		strncpy(name, next, name_len);
		name[name_len] = '\0';
		next += name_len;
		if (*next != '\0')
			next++;

		if (validate_name(name, "MSGQ", 1) != 0)
			return -EINVAL;

		idx = APP_PARAM_ADD(app->msgq_params, name);
		if (idx < 0)
			return idx;

		p->msgq_in[p->n_msgq_in] = idx;
		p->n_msgq_in++;
	}

	return 0;
}

static int
parse_pipeline_msgq_out(struct app_params *app,
	struct app_pipeline_params *p,
	const char *value)
{
	const char *next = value;
	char *end;
	char name[APP_PARAM_NAME_SIZE];
	size_t name_len;
	ssize_t idx;

	while (*next != '\0') {
		end = strchr(next, ' ');
		if (!end)
			name_len = strlen(next);
		else
			name_len = end - next;

		if (name_len == 0 || name_len == sizeof(name))
			return -EINVAL;

		strncpy(name, next, name_len);
		name[name_len] = '\0';
		next += name_len;
		if (*next != '\0')
			next++;

		if (validate_name(name, "MSGQ", 1) != 0)
			return -EINVAL;

		idx = APP_PARAM_ADD(app->msgq_params, name);
		if (idx < 0)
			return idx;

		p->msgq_out[p->n_msgq_out] = idx;
		p->n_msgq_out++;
	}

	return 0;
}


static void
parse_pipeline(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	char name[CFG_NAME_LEN];
	struct app_pipeline_params *param;
	struct rte_cfgfile_entry *entries;
	ssize_t param_idx;
	int n_entries, ret, i;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->pipeline_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->pipeline_params, section_name);

	param = &app->pipeline_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "type") == 0) {
			ret = snprintf(param->type,
				RTE_DIM(param->type),
				"%s",
				ent->value);
			if ((ret > 0) && (ret < (int)RTE_DIM(param->type)))
				ret = 0;
			else
				ret = -EINVAL;
		} else if (strcmp(ent->name, "core") == 0)
			ret = parse_pipeline_core(&param->socket_id,
				&param->core_id,
				&param->hyper_th_id,
				ent->value);
		else if (strcmp(ent->name, "pktq_in") == 0)
			ret = parse_pipeline_pktq_in(app, param, ent->value);
		else if (strcmp(ent->name, "pktq_out") == 0)
			ret = parse_pipeline_pktq_out(app, param, ent->value);
		else if (strcmp(ent->name, "msgq_in") == 0)
			ret = parse_pipeline_msgq_in(app, param, ent->value);
		else if (strcmp(ent->name, "msgq_out") == 0)
			ret = parse_pipeline_msgq_out(app, param, ent->value);
		else if (strcmp(ent->name, "timer_period") == 0)
			ret = parser_read_uint32(&param->timer_period,
				ent->value);
		else {
			param->args_name[param->n_args] = strdup(ent->name);
			param->args_value[param->n_args] = strdup(ent->value);

			APP_CHECK((param->args_name[param->n_args] != NULL) &&
				(param->args_value[param->n_args] != NULL),
				"CFG: [%s] out of memory",
				section_name);

			param->n_args++;
			ret = 0;
		}

		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	snprintf(name, sizeof(name), "MSGQ-REQ-%s", section_name);
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	PARSER_IMPLICIT_PARAM_ADD_CHECK(param_idx, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;
	param->msgq_in[param->n_msgq_in++] = param_idx;

	snprintf(name, sizeof(name), "MSGQ-RSP-%s", section_name);
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	PARSER_IMPLICIT_PARAM_ADD_CHECK(param_idx, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;
	param->msgq_out[param->n_msgq_out++] = param_idx;

	snprintf(name, sizeof(name), "MSGQ-REQ-CORE-s%" PRIu32 "c%" PRIu32 "%s",
		param->socket_id,
		param->core_id,
		(param->hyper_th_id) ? "h" : "");
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	PARSER_IMPLICIT_PARAM_ADD_CHECK(param_idx, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;

	snprintf(name, sizeof(name), "MSGQ-RSP-CORE-s%" PRIu32 "c%" PRIu32 "%s",
		param->socket_id,
		param->core_id,
		(param->hyper_th_id) ? "h" : "");
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	PARSER_IMPLICIT_PARAM_ADD_CHECK(param_idx, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;

	free(entries);
}

static void
parse_mempool(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_mempool_params *param;
	struct rte_cfgfile_entry *entries;
	ssize_t param_idx;
	int n_entries, ret, i;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->mempool_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->mempool_params, section_name);

	param = &app->mempool_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "buffer_size") == 0)
			ret = parser_read_uint32(&param->buffer_size,
				ent->value);
		else if (strcmp(ent->name, "pool_size") == 0)
			ret = parser_read_uint32(&param->pool_size,
				ent->value);
		else if (strcmp(ent->name, "cache_size") == 0)
			ret = parser_read_uint32(&param->cache_size,
				ent->value);
		else if (strcmp(ent->name, "cpu") == 0)
			ret = parser_read_uint32(&param->cpu_socket_id,
				ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_link(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_link_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->link_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->link_params, section_name);

	param = &app->link_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "arp_q") == 0)
			ret = parser_read_uint32(&param->arp_q,
				ent->value);
		else if (strcmp(ent->name, "tcp_syn_q") == 0)
			ret = parser_read_uint32(&param->tcp_syn_local_q,
				ent->value);
		else if (strcmp(ent->name, "ip_local_q") == 0)
			ret = parser_read_uint32(&param->ip_local_q,
				ent->value);
		else if (strcmp(ent->name, "tcp_local_q") == 0)
			ret = parser_read_uint32(&param->tcp_local_q,
				ent->value);
		else if (strcmp(ent->name, "udp_local_q") == 0)
			ret = parser_read_uint32(&param->udp_local_q,
				ent->value);
		else if (strcmp(ent->name, "sctp_local_q") == 0)
			ret = parser_read_uint32(&param->sctp_local_q,
				ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_rxq(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_hwq_in_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->hwq_in_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->hwq_in_params, section_name);

	param = &app->hwq_in_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "mempool") == 0) {
			int status = validate_name(ent->value, "MEMPOOL", 1);
			ssize_t idx;

			APP_CHECK((status == 0),
				"CFG: [%s] entry '%s': invalid mempool\n",
				section_name,
				ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			PARSER_IMPLICIT_PARAM_ADD_CHECK(idx, section_name);
			param->mempool_id = idx;
			ret = 0;
		} else if (strcmp(ent->name, "size") == 0)
			ret = parser_read_uint32(&param->size,
				ent->value);
		else if (strcmp(ent->name, "burst") == 0)
			ret = parser_read_uint32(&param->burst,
				ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_txq(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_hwq_out_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->hwq_out_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->hwq_out_params, section_name);

	param = &app->hwq_out_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "size") == 0)
			ret = parser_read_uint32(&param->size, ent->value);
		else if (strcmp(ent->name, "burst") == 0)
			ret = parser_read_uint32(&param->burst, ent->value);
		else if (strcmp(ent->name, "dropless") == 0) {
			ret = parser_read_arg_bool(ent->value);
			if (ret >= 0) {
				param->dropless = ret;
				ret = 0;
			}
		}

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_swq(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_swq_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->swq_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->swq_params, section_name);

	param = &app->swq_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "size") == 0)
			ret = parser_read_uint32(&param->size,
				ent->value);
		else if (strcmp(ent->name, "burst_read") == 0)
			ret = parser_read_uint32(&param->burst_read,
				ent->value);
		else if (strcmp(ent->name, "burst_write") == 0)
			ret = parser_read_uint32(&param->burst_write,
				ent->value);
		else if (strcmp(ent->name, "dropless") == 0) {
			ret = parser_read_arg_bool(ent->value);
			if (ret >= 0) {
				param->dropless = ret;
				ret = 0;
			}
		} else if (strcmp(ent->name, "n_retries") == 0)
			ret = parser_read_uint64(&param->n_retries,
				ent->value);
		else if (strcmp(ent->name, "cpu") == 0)
			ret = parser_read_uint32(&param->cpu_socket_id,
				ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_tm(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_tm_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->tm_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->tm_params, section_name);

	param = &app->tm_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "cfg") == 0) {
			param->file_name = strdup(ent->value);
			if (param->file_name == NULL)
				ret = -EINVAL;

			ret = 0;
		} else if (strcmp(ent->name, "burst_read") == 0)
			ret = parser_read_uint32(&param->burst_read,
				ent->value);
		else if (strcmp(ent->name, "burst_write") == 0)
			ret = parser_read_uint32(&param->burst_write,
				ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret != -EBADF,
			"CFG: [%s] entry '%s': TM cfg parse error '%s'\n",
			section_name,
			ent->name,
			ent->value);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_source(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_source_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->source_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->source_params, section_name);

	param = &app->source_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "mempool") == 0) {
			int status = validate_name(ent->value, "MEMPOOL", 1);
			ssize_t idx;

			APP_CHECK((status == 0),
				"CFG: [%s] entry '%s': invalid mempool\n",
					section_name,
					ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			PARSER_IMPLICIT_PARAM_ADD_CHECK(idx, section_name);
			param->mempool_id = idx;
			ret = 0;
		} else if (strcmp(ent->name, "burst") == 0)
			ret = parser_read_uint32(&param->burst, ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_msgq_req_pipeline(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_msgq_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->msgq_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->msgq_params, section_name);

	param = &app->msgq_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "size") == 0)
			ret = parser_read_uint32(&param->size, ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_msgq_rsp_pipeline(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_msgq_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->msgq_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->msgq_params, section_name);

	param = &app->msgq_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "size") == 0)
			ret = parser_read_uint32(&param->size, ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

static void
parse_msgq(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_msgq_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, ret, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->msgq_params, section_name);
	PARSER_PARAM_ADD_CHECK(param_idx, app->msgq_params, section_name);

	param = &app->msgq_params[param_idx];
	param->parsed = 1;

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		ret = -ESRCH;
		if (strcmp(ent->name, "size") == 0)
			ret = parser_read_uint32(&param->size,
				ent->value);
		else if (strcmp(ent->name, "cpu") == 0)
			ret = parser_read_uint32(&param->cpu_socket_id,
				ent->value);

		APP_CHECK(ret != -ESRCH,
			"CFG: [%s] entry '%s': unknown entry\n",
			section_name,
			ent->name);
		APP_CHECK(ret == 0,
			"CFG: [%s] entry '%s': Invalid value '%s'\n",
			section_name,
			ent->name,
			ent->value);
	}

	free(entries);
}

typedef void (*config_section_load)(struct app_params *p,
	const char *section_name,
	struct rte_cfgfile *cfg);

struct config_section {
	const char prefix[CFG_NAME_LEN];
	int numbers;
	config_section_load load;
};

static const struct config_section cfg_file_scheme[] = {
	{"EAL", 0, parse_eal},
	{"PIPELINE", 1, parse_pipeline},
	{"MEMPOOL", 1, parse_mempool},
	{"LINK", 1, parse_link},
	{"RXQ", 2, parse_rxq},
	{"TXQ", 2, parse_txq},
	{"SWQ", 1, parse_swq},
	{"TM", 1, parse_tm},
	{"SOURCE", 1, parse_source},
	{"MSGQ-REQ-PIPELINE", 1, parse_msgq_req_pipeline},
	{"MSGQ-RSP-PIPELINE", 1, parse_msgq_rsp_pipeline},
	{"MSGQ", 1, parse_msgq},
};

static void
create_implicit_mempools(struct app_params *app)
{
	ssize_t idx;

	idx = APP_PARAM_ADD(app->mempool_params, "MEMPOOL0");
	PARSER_IMPLICIT_PARAM_ADD_CHECK(idx, "start-up");
}

static void
parse_port_mask(struct app_params *app, uint64_t port_mask)
{
	uint32_t pmd_id, link_id;

	link_id = 0;
	for (pmd_id = 0; pmd_id < RTE_MAX_ETHPORTS; pmd_id++) {
		char name[APP_PARAM_NAME_SIZE];
		ssize_t idx;

		if ((port_mask & (1LLU << pmd_id)) == 0)
			continue;

		snprintf(name, sizeof(name), "LINK%" PRIu32, link_id);
		idx = APP_PARAM_ADD(app->link_params, name);
		PARSER_IMPLICIT_PARAM_ADD_CHECK(idx, name);

		app->link_params[idx].pmd_id = pmd_id;
		link_id++;
	}
}

int
app_config_parse(struct app_params *app, const char *file_name)
{
	char config_file_out[APP_FILE_NAME_SIZE];
	struct rte_cfgfile *cfg;
	char **section_names;
	int i, j, sect_count;

	/* Implicit mempools */
	create_implicit_mempools(app);

	/* Port mask */
	parse_port_mask(app, app->port_mask);

	/* Load application configuration file */
	cfg = rte_cfgfile_load(file_name, 0);
	APP_CHECK(cfg != NULL, "Unable to load config file %s", file_name);

	sect_count = rte_cfgfile_num_sections(cfg, NULL, 0);
	section_names = malloc(sect_count * sizeof(char *));
	for (i = 0; i < sect_count; i++)
		section_names[i] = malloc(CFG_NAME_LEN);

	rte_cfgfile_sections(cfg, section_names, sect_count);

	for (i = 0; i < sect_count; i++) {
		const struct config_section *sch_s;
		int len, cfg_name_len;

		cfg_name_len = strlen(section_names[i]);

		/* Find section type */
		for (j = 0; j < (int)RTE_DIM(cfg_file_scheme); j++) {
			sch_s = &cfg_file_scheme[j];
			len = strlen(sch_s->prefix);

			if (cfg_name_len < len)
				continue;

			/* After section name we expect only '\0' or digit or
			 * digit dot digit, so protect against false matching,
			 * for example: "ABC" should match section name
			 * "ABC0.0", but it should not match section_name
			 * "ABCDEF".
			 */
			if ((section_names[i][len] != '\0') &&
				!isdigit(section_names[i][len]))
				continue;

			if (strncmp(sch_s->prefix, section_names[i], len) == 0)
				break;
		}

		APP_CHECK(j < (int)RTE_DIM(cfg_file_scheme),
			"Unknown section %s",
			section_names[i]);

		APP_CHECK(validate_name(section_names[i],
			sch_s->prefix,
			sch_s->numbers) == 0,
			"Invalid section name '%s'",
			section_names[i]);

		sch_s->load(app, section_names[i], cfg);
	}

	for (i = 0; i < sect_count; i++)
		free(section_names[i]);

	free(section_names);

	rte_cfgfile_close(cfg);

	APP_PARAM_COUNT(app->mempool_params, app->n_mempools);
	APP_PARAM_COUNT(app->link_params, app->n_links);
	APP_PARAM_COUNT(app->hwq_in_params, app->n_pktq_hwq_in);
	APP_PARAM_COUNT(app->hwq_out_params, app->n_pktq_hwq_out);
	APP_PARAM_COUNT(app->swq_params, app->n_pktq_swq);
	APP_PARAM_COUNT(app->tm_params, app->n_pktq_tm);
	APP_PARAM_COUNT(app->source_params, app->n_pktq_source);
	APP_PARAM_COUNT(app->sink_params, app->n_pktq_sink);
	APP_PARAM_COUNT(app->msgq_params, app->n_msgq);
	APP_PARAM_COUNT(app->pipeline_params, app->n_pipelines);

	/* Save configuration to output file */
	snprintf(config_file_out,
		APP_FILE_NAME_SIZE,
		"%s.out",
		app->config_file);
	app_config_save(app, config_file_out);

	/* Load TM configuration files */
	app_config_parse_tm(app);

	return 0;
}

static void
save_eal_params(struct app_params *app, FILE *f)
{
	struct app_eal_params *p = &app->eal_params;

	fprintf(f, "[EAL]\n");

	if (p->coremap)
		fprintf(f, "%s = %s\n", "lcores", p->coremap);

	if (p->master_lcore_present)
		fprintf(f, "%s = %" PRIu32 "\n",
			"master_lcore", p->master_lcore);

	fprintf(f, "%s = %" PRIu32 "\n", "n", p->channels);

	if (p->memory_present)
		fprintf(f, "%s = %" PRIu32 "\n", "m", p->memory);

	if (p->ranks_present)
		fprintf(f, "%s = %" PRIu32 "\n", "r", p->ranks);

	if (p->pci_blacklist)
		fprintf(f, "%s = %s\n", "pci_blacklist", p->pci_blacklist);

	if (p->pci_whitelist)
		fprintf(f, "%s = %s\n", "pci_whitelist", p->pci_whitelist);

	if (p->vdev)
		fprintf(f, "%s = %s\n", "vdev", p->vdev);

	if (p->vmware_tsc_map_present)
		fprintf(f, "%s = %s\n", "vmware_tsc_map",
			(p->vmware_tsc_map) ? "yes" : "no");

	if (p->proc_type)
		fprintf(f, "%s = %s\n", "proc_type", p->proc_type);

	if (p->syslog)
		fprintf(f, "%s = %s\n", "syslog", p->syslog);

	if (p->log_level_present)
		fprintf(f, "%s = %" PRIu32 "\n", "log_level", p->log_level);

	if (p->version_present)
		fprintf(f, "%s = %s\n",	"v", (p->version) ? "yes" : "no");

	if (p->help_present)
		fprintf(f, "%s = %s\n",	"help", (p->help) ? "yes" : "no");

	if (p->no_huge_present)
		fprintf(f, "%s = %s\n",	"no_huge", (p->no_huge) ? "yes" : "no");

	if (p->no_pci_present)
		fprintf(f, "%s = %s\n",	"no_pci", (p->no_pci) ? "yes" : "no");

	if (p->no_hpet_present)
		fprintf(f, "%s = %s\n",	"no_hpet", (p->no_hpet) ? "yes" : "no");

	if (p->no_shconf_present)
		fprintf(f, "%s = %s\n", "no_shconf",
			(p->no_shconf) ? "yes" : "no");

	if (p->add_driver)
		fprintf(f, "%s = %s\n",	"d", p->add_driver);

	if (p->socket_mem)
		fprintf(f, "%s = %s\n",	"socket_mem", p->socket_mem);

	if (p->huge_dir)
		fprintf(f, "%s = %s\n", "huge_dir", p->huge_dir);

	if (p->file_prefix)
		fprintf(f, "%s = %s\n", "file_prefix", p->file_prefix);

	if (p->base_virtaddr)
		fprintf(f, "%s = %s\n",	"base_virtaddr", p->base_virtaddr);

	if (p->create_uio_dev_present)
		fprintf(f, "%s = %s\n", "create_uio_dev",
			(p->create_uio_dev) ? "yes" : "no");

	if (p->vfio_intr)
		fprintf(f, "%s = %s\n", "vfio_intr", p->vfio_intr);

	if (p->xen_dom0_present)
		fprintf(f, "%s = %s\n", "xen_dom0",
			(p->xen_dom0) ? "yes" : "no");

	fputc('\n', f);
}

static void
save_mempool_params(struct app_params *app, FILE *f)
{
	struct app_mempool_params *p;
	size_t i, count;

	count = RTE_DIM(app->mempool_params);
	for (i = 0; i < count; i++) {
		p = &app->mempool_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %" PRIu32 "\n", "buffer_size", p->buffer_size);
		fprintf(f, "%s = %" PRIu32 "\n", "pool_size", p->pool_size);
		fprintf(f, "%s = %" PRIu32 "\n", "cache_size", p->cache_size);
		fprintf(f, "%s = %" PRIu32 "\n", "cpu", p->cpu_socket_id);

		fputc('\n', f);
	}
}

static void
save_links_params(struct app_params *app, FILE *f)
{
	struct app_link_params *p;
	size_t i, count;

	count = RTE_DIM(app->link_params);
	for (i = 0; i < count; i++) {
		p = &app->link_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "; %s = %" PRIu32 "\n", "pmd_id", p->pmd_id);
		fprintf(f, "%s = %" PRIu32 "\n", "arp_q", p->arp_q);
		fprintf(f, "%s = %" PRIu32 "\n", "tcp_syn_local_q",
			p->tcp_syn_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "ip_local_q", p->ip_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "tcp_local_q", p->tcp_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "udp_local_q", p->udp_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "sctp_local_q",
			p->sctp_local_q);

		fputc('\n', f);
	}
}

static void
save_rxq_params(struct app_params *app, FILE *f)
{
	struct app_pktq_hwq_in_params *p;
	size_t i, count;

	count = RTE_DIM(app->hwq_in_params);
	for (i = 0; i < count; i++) {
		p = &app->hwq_in_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %s\n",
			"mempool",
			app->mempool_params[p->mempool_id].name);
		fprintf(f, "%s = %" PRIu32 "\n", "size", p->size);
		fprintf(f, "%s = %" PRIu32 "\n", "burst", p->burst);

		fputc('\n', f);
	}
}

static void
save_txq_params(struct app_params *app, FILE *f)
{
	struct app_pktq_hwq_out_params *p;
	size_t i, count;

	count = RTE_DIM(app->hwq_out_params);
	for (i = 0; i < count; i++) {
		p = &app->hwq_out_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %" PRIu32 "\n", "size", p->size);
		fprintf(f, "%s = %" PRIu32 "\n", "burst", p->burst);
		fprintf(f, "%s = %s\n",
			"dropless",
			p->dropless ? "yes" : "no");

		fputc('\n', f);
	}
}

static void
save_swq_params(struct app_params *app, FILE *f)
{
	struct app_pktq_swq_params *p;
	size_t i, count;

	count = RTE_DIM(app->swq_params);
	for (i = 0; i < count; i++) {
		p = &app->swq_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %" PRIu32 "\n", "size", p->size);
		fprintf(f, "%s = %" PRIu32 "\n", "burst_read", p->burst_read);
		fprintf(f, "%s = %" PRIu32 "\n", "burst_write", p->burst_write);
		fprintf(f, "%s = %s\n", "dropless", p->dropless ? "yes" : "no");
		fprintf(f, "%s = %" PRIu64 "\n", "n_retries", p->n_retries);
		fprintf(f, "%s = %" PRIu32 "\n", "cpu", p->cpu_socket_id);

		fputc('\n', f);
	}
}

static void
save_tm_params(struct app_params *app, FILE *f)
{
	struct app_pktq_tm_params *p;
	size_t i, count;

	count = RTE_DIM(app->tm_params);
	for (i = 0; i < count; i++) {
		p = &app->tm_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %s\n", "cfg", p->file_name);
		fprintf(f, "%s = %" PRIu32 "\n", "burst_read", p->burst_read);
		fprintf(f, "%s = %" PRIu32 "\n", "burst_write", p->burst_write);

		fputc('\n', f);
	}
}

static void
save_source_params(struct app_params *app, FILE *f)
{
	struct app_pktq_source_params *p;
	size_t i, count;

	count = RTE_DIM(app->source_params);
	for (i = 0; i < count; i++) {
		p = &app->source_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %s\n",
			"mempool",
			app->mempool_params[p->mempool_id].name);
		fprintf(f, "%s = %" PRIu32 "\n", "burst", p->burst);
		fputc('\n', f);
	}
}

static void
save_msgq_params(struct app_params *app, FILE *f)
{
	struct app_msgq_params *p;
	size_t i, count;

	count = RTE_DIM(app->msgq_params);
	for (i = 0; i < count; i++) {
		p = &app->msgq_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %" PRIu32 "\n", "size", p->size);
		fprintf(f, "%s = %" PRIu32 "\n", "cpu", p->cpu_socket_id);

		fputc('\n', f);
	}
}

static void
save_pipeline_params(struct app_params *app, FILE *f)
{
	size_t i, count;

	count = RTE_DIM(app->pipeline_params);
	for (i = 0; i < count; i++) {
		struct app_pipeline_params *p = &app->pipeline_params[i];

		if (!APP_PARAM_VALID(p))
			continue;

		/* section name */
		fprintf(f, "[%s]\n", p->name);

		/* type */
		fprintf(f, "type = %s\n", p->type);

		/* core */
		fprintf(f, "core = s%" PRIu32 "c%" PRIu32 "%s\n",
			p->socket_id,
			p->core_id,
			(p->hyper_th_id) ? "h" : "");

		/* pktq_in */
		if (p->n_pktq_in) {
			uint32_t j;

			fprintf(f, "pktq_in =");
			for (j = 0; j < p->n_pktq_in; j++) {
				struct app_pktq_in_params *pp = &p->pktq_in[j];
				char *name;

				switch (pp->type) {
				case APP_PKTQ_IN_HWQ:
					name = app->hwq_in_params[pp->id].name;
					break;
				case APP_PKTQ_IN_SWQ:
					name = app->swq_params[pp->id].name;
					break;
				case APP_PKTQ_IN_TM:
					name = app->tm_params[pp->id].name;
					break;
				case APP_PKTQ_IN_SOURCE:
					name = app->source_params[pp->id].name;
					break;
				default:
					APP_CHECK(0, "Error\n");
				}

				fprintf(f, " %s", name);
			}
			fprintf(f, "\n");
		}

		/* pktq_in */
		if (p->n_pktq_out) {
			uint32_t j;

			fprintf(f, "pktq_out =");
			for (j = 0; j < p->n_pktq_out; j++) {
				struct app_pktq_out_params *pp =
					&p->pktq_out[j];
				char *name;

				switch (pp->type) {
				case APP_PKTQ_OUT_HWQ:
					name = app->hwq_out_params[pp->id].name;
					break;
				case APP_PKTQ_OUT_SWQ:
					name = app->swq_params[pp->id].name;
					break;
				case APP_PKTQ_OUT_TM:
					name = app->tm_params[pp->id].name;
					break;
				case APP_PKTQ_OUT_SINK:
					name = app->sink_params[pp->id].name;
					break;
				default:
					APP_CHECK(0, "Error\n");
				}

				fprintf(f, " %s", name);
			}
			fprintf(f, "\n");
		}

		/* msgq_in */
		if (p->n_msgq_in) {
			uint32_t j;

			fprintf(f, "msgq_in =");
			for (j = 0; j < p->n_msgq_in; j++) {
				uint32_t id = p->msgq_in[j];
				char *name = app->msgq_params[id].name;

				fprintf(f, " %s", name);
			}
			fprintf(f, "\n");
		}

		/* msgq_out */
		if (p->n_msgq_in) {
			uint32_t j;

			fprintf(f, "msgq_out =");
			for (j = 0; j < p->n_msgq_out; j++) {
				uint32_t id = p->msgq_out[j];
				char *name = app->msgq_params[id].name;

				fprintf(f, " %s", name);
			}
			fprintf(f, "\n");
		}

		/* timer_period */
		fprintf(f, "timer_period = %" PRIu32 "\n", p->timer_period);

		/* args */
		if (p->n_args) {
			uint32_t j;

			for (j = 0; j < p->n_args; j++)
				fprintf(f, "%s = %s\n", p->args_name[j],
					p->args_value[j]);
		}

		fprintf(f, "\n");
	}
}

void
app_config_save(struct app_params *app, const char *file_name)
{
	FILE *file;
	char *name, *dir_name;
	int status;

	name = strdup(file_name);
	dir_name = dirname(name);
	status = access(dir_name, W_OK);
	APP_CHECK((status == 0),
		"Need write access to directory \"%s\" to save configuration\n",
		dir_name);

	file = fopen(file_name, "w");
	APP_CHECK((file != NULL),
		"Failed to save configuration to file \"%s\"", file_name);

	save_eal_params(app, file);
	save_pipeline_params(app, file);
	save_mempool_params(app, file);
	save_links_params(app, file);
	save_rxq_params(app, file);
	save_txq_params(app, file);
	save_swq_params(app, file);
	save_tm_params(app, file);
	save_source_params(app, file);
	save_msgq_params(app, file);

	fclose(file);
	free(name);
}

int
app_config_init(struct app_params *app)
{
	size_t i;

	memcpy(app, &app_params_default, sizeof(struct app_params));

	for (i = 0; i < RTE_DIM(app->mempool_params); i++)
		memcpy(&app->mempool_params[i],
			&mempool_params_default,
			sizeof(struct app_mempool_params));

	for (i = 0; i < RTE_DIM(app->link_params); i++)
		memcpy(&app->link_params[i],
			&link_params_default,
			sizeof(struct app_link_params));

	for (i = 0; i < RTE_DIM(app->hwq_in_params); i++)
		memcpy(&app->hwq_in_params[i],
			&default_hwq_in_params,
			sizeof(default_hwq_in_params));

	for (i = 0; i < RTE_DIM(app->hwq_out_params); i++)
		memcpy(&app->hwq_out_params[i],
			&default_hwq_out_params,
			sizeof(default_hwq_out_params));

	for (i = 0; i < RTE_DIM(app->swq_params); i++)
		memcpy(&app->swq_params[i],
			&default_swq_params,
			sizeof(default_swq_params));

	for (i = 0; i < RTE_DIM(app->tm_params); i++)
		memcpy(&app->tm_params[i],
			&default_tm_params,
			sizeof(default_tm_params));

	for (i = 0; i < RTE_DIM(app->source_params); i++)
		memcpy(&app->source_params[i],
			&default_source_params,
			sizeof(default_source_params));

	for (i = 0; i < RTE_DIM(app->sink_params); i++)
		memcpy(&app->sink_params[i],
			&default_sink_params,
			sizeof(default_sink_params));

	for (i = 0; i < RTE_DIM(app->msgq_params); i++)
		memcpy(&app->msgq_params[i],
			&default_msgq_params,
			sizeof(default_msgq_params));

	for (i = 0; i < RTE_DIM(app->pipeline_params); i++)
		memcpy(&app->pipeline_params[i],
			&default_pipeline_params,
			sizeof(default_pipeline_params));

	return 0;
}

int
app_config_args(struct app_params *app, int argc, char **argv)
{
	int opt;
	int option_index, f_present, s_present, p_present, l_present;
	int scaned = 0;

	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	/* Copy application name */
	strncpy(app->app_name, argv[0], APP_APPNAME_SIZE - 1);

	f_present = 0;
	s_present = 0;
	p_present = 0;
	l_present = 0;

	while ((opt = getopt_long(argc, argv, "f:s:p:l:", lgopts,
			&option_index)) != EOF)
		switch (opt) {
		case 'f':
			if (f_present)
				rte_panic("Error: Config file is provided "
					"more than once\n");
			f_present = 1;

			if (!strlen(optarg))
				rte_panic("Error: Config file name is null\n");

			app->config_file = strdup(optarg);
			if (app->config_file == NULL)
				rte_panic("Error: Memory allocation failure\n");

			break;

		case 's':
			if (s_present)
				rte_panic("Error: Script file is provided "
					"more than once\n");
			s_present = 1;

			if (!strlen(optarg))
				rte_panic("Error: Script file name is null\n");

			app->script_file = strdup(optarg);
			if (app->script_file == NULL)
				rte_panic("Error: Memory allocation failure\n");

			break;

		case 'p':
			if (p_present)
				rte_panic("Error: PORT_MASK is provided "
					"more than once\n");
			p_present = 1;

			if ((sscanf(optarg, "%" SCNx64 "%n", &app->port_mask,
				&scaned) != 1) ||
				((size_t) scaned != strlen(optarg)))
				rte_panic("Error: PORT_MASK is not "
					"a hexadecimal integer\n");

			if (app->port_mask == 0)
				rte_panic("Error: PORT_MASK is null\n");

			break;

		case 'l':
			if (l_present)
				rte_panic("Error: LOG_LEVEL is provided "
					"more than once\n");
			l_present = 1;

			if ((sscanf(optarg, "%" SCNu32 "%n", &app->log_level,
				&scaned) != 1) ||
				((size_t) scaned != strlen(optarg)) ||
				(app->log_level >= APP_LOG_LEVELS))
				rte_panic("Error: LOG_LEVEL invalid value\n");

			break;

		default:
			app_print_usage(argv[0]);
		}

	optind = 0; /* reset getopt lib */

	/* Check that mandatory args have been provided */
	if (!p_present)
		rte_panic("Error: PORT_MASK is not provided\n");

	return 0;
}
