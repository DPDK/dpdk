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
#include <sys/wait.h>

#include <rte_errno.h>
#include <rte_cfgfile.h>
#include <rte_string_fns.h>

#include "app.h"
#include "parser.h"

/**
 * Default config values
 **/

static struct app_params app_params_default = {
	.config_file = "./config/ip_pipeline.cfg",
	.log_level = APP_LOG_LEVEL_HIGH,
	.port_mask = 0,

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
	.tcp_syn_q = 0,
	.ip_local_q = 0,
	.tcp_local_q = 0,
	.udp_local_q = 0,
	.sctp_local_q = 0,
	.rss_qs = {0},
	.n_rss_qs = 0,
	.rss_proto_ipv4 = ETH_RSS_IPV4,
	.rss_proto_ipv6 = ETH_RSS_IPV6,
	.rss_proto_l2 = 0,
	.state = 0,
	.ip = 0,
	.depth = 0,
	.mac_addr = 0,
	.pci_bdf = {0},

	.conf = {
		.link_speeds = 0,
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
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_key_len = 40,
				.rss_hf = 0,
			},
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
	.ipv4_frag = 0,
	.ipv6_frag = 0,
	.ipv4_ras = 0,
	.ipv6_ras = 0,
	.mtu = 0,
	.metadata_size = 0,
	.mempool_direct_id = 0,
	.mempool_indirect_id = 0,
};

struct app_pktq_tm_params default_tm_params = {
	.parsed = 0,
	.file_name = "./config/tm_profile.cfg",
	.burst_read = 24,
	.burst_write = 32,
};

struct app_pktq_tap_params default_tap_params = {
	.parsed = 0,
	.burst_read = 32,
	.burst_write = 32,
	.dropless = 0,
	.n_retries = 0,
	.mempool_id = 0,
};

struct app_pktq_kni_params default_kni_params = {
	.parsed = 0,
	.socket_id = 0,
	.core_id = 0,
	.hyper_th_id = 0,
	.force_bind = 0,

	.mempool_id = 0,
	.burst_read = 32,
	.burst_write = 32,
	.dropless = 0,
	.n_retries = 0,
};

struct app_pktq_source_params default_source_params = {
	.parsed = 0,
	.mempool_id = 0,
	.burst = 32,
	.file_name = "./config/packets.pcap",
	.n_bytes_per_pkt = 0,
};

struct app_pktq_sink_params default_sink_params = {
	.parsed = 0,
	.file_name = NULL,
	.n_pkts_to_dump = 0,
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
	"Usage: %s [-f CONFIG_FILE] [-s SCRIPT_FILE] [-p PORT_MASK] "
	"[-l LOG_LEVEL] [--preproc PREPROCESSOR] [--preproc-args ARGS]\n"
	"\n"
	"Arguments:\n"
	"\t-f CONFIG_FILE: Default config file is %s\n"
	"\t-p PORT_MASK: Mask of NIC port IDs in hex format (generated from "
		"config file when not provided)\n"
	"\t-s SCRIPT_FILE: No CLI script file is run when not specified\n"
	"\t-l LOG_LEVEL: 0 = NONE, 1 = HIGH PRIO (default), 2 = LOW PRIO\n"
	"\t--preproc PREPROCESSOR: Configuration file pre-processor\n"
	"\t--preproc-args ARGS: Arguments to be passed to pre-processor\n"
	"\n";

static void
app_print_usage(char *prgname)
{
	rte_exit(0, app_usage, prgname, app_params_default.config_file);
}

#define APP_PARAM_ADD(set, key)						\
({									\
	ssize_t pos = APP_PARAM_FIND(set, key);				\
	ssize_t size = RTE_DIM(set);					\
									\
	if (pos < 0) {							\
		for (pos = 0; pos < size; pos++) {			\
			if (!APP_PARAM_VALID(&((set)[pos])))		\
				break;					\
		}							\
									\
		APP_CHECK((pos < size),					\
			"Parse error: size of %s is limited to %u elements",\
			#set, (uint32_t) size);				\
									\
		(set)[pos].name = strdup(key);				\
		APP_CHECK(((set)[pos].name),				\
			"Parse error: no free memory");			\
	}								\
	pos;								\
})

#define APP_PARAM_ADD_LINK_FOR_RXQ(app, rxq_name)			\
({									\
	char link_name[APP_PARAM_NAME_SIZE];				\
	ssize_t link_param_pos;						\
	uint32_t link_id, queue_id;				\
									\
	sscanf((rxq_name), "RXQ%" SCNu32 ".%" SCNu32, &link_id, &queue_id);\
	sprintf(link_name, "LINK%" PRIu32, link_id);			\
	link_param_pos = APP_PARAM_ADD((app)->link_params, link_name);	\
	link_param_pos;							\
})

#define APP_PARAM_ADD_LINK_FOR_TXQ(app, txq_name)			\
({									\
	char link_name[APP_PARAM_NAME_SIZE];				\
	ssize_t link_param_pos;						\
	uint32_t link_id, queue_id;					\
									\
	sscanf((txq_name), "TXQ%" SCNu32 ".%" SCNu32, &link_id, &queue_id);\
	sprintf(link_name, "LINK%" PRIu32, link_id);			\
	link_param_pos = APP_PARAM_ADD((app)->link_params, link_name);	\
	link_param_pos;							\
})

#define APP_PARAM_ADD_LINK_FOR_TM(app, tm_name)				\
({									\
	char link_name[APP_PARAM_NAME_SIZE];				\
	ssize_t link_param_pos;						\
	uint32_t link_id;						\
									\
	sscanf((tm_name), "TM%" SCNu32, &link_id);			\
	sprintf(link_name, "LINK%" PRIu32, link_id);			\
	link_param_pos = APP_PARAM_ADD((app)->link_params, link_name);	\
	link_param_pos;							\
})

#define APP_PARAM_ADD_LINK_FOR_KNI(app, kni_name)			\
({									\
	char link_name[APP_PARAM_NAME_SIZE];				\
	ssize_t link_param_pos;						\
	uint32_t link_id;						\
									\
	sscanf((kni_name), "KNI%" SCNu32, &link_id);		\
	sprintf(link_name, "LINK%" PRIu32, link_id);			\
	link_param_pos = APP_PARAM_ADD((app)->link_params, link_name);	\
	link_param_pos;							\
})

#define PARSE_CHECK_DUPLICATE_SECTION(obj)				\
do {									\
	APP_CHECK(((obj)->parsed == 0),					\
		"Parse error: duplicate \"%s\" section", (obj)->name);	\
	(obj)->parsed++;					\
} while (0)

#define PARSE_CHECK_DUPLICATE_SECTION_EAL(obj)				\
do {									\
	APP_CHECK(((obj)->parsed == 0),					\
		"Parse error: duplicate \"%s\" section", "EAL");	\
	(obj)->parsed++;					\
} while (0)

#define PARSE_ERROR(exp, section, entry)				\
APP_CHECK(exp, "Parse error in section \"%s\": entry \"%s\"", section, entry)

#define PARSE_ERROR_MESSAGE(exp, section, entry, message)		\
APP_CHECK(exp, "Parse error in section \"%s\", entry \"%s\": %s",	\
	section, entry, message)

#define PARSE_ERROR_NO_ELEMENTS(exp, section, entry)			\
APP_CHECK(exp, "Parse error in section \"%s\", entry \"%s\": "		\
	"no elements detected",						\
	section, entry)

#define PARSE_ERROR_TOO_MANY_ELEMENTS(exp, section, entry, max)		\
APP_CHECK(exp, "Parse error in section \"%s\", entry \"%s\": "		\
	"maximum number of elements allowed is %u",			\
	section, entry, max)

#define PARSE_ERROR_INVALID_ELEMENT(exp, section, entry, value)		\
APP_CHECK(exp, "Parse error in section \"%s\", entry \"%s\": "		\
	"Invalid element value \"%s\"",					\
	section, entry, value)

#define PARSE_ERROR_MALLOC(exp)						\
APP_CHECK(exp, "Parse error: no free memory")

#define PARSE_ERROR_SECTION(exp, section)				\
APP_CHECK(exp, "Parse error in section \"%s\"", section)

#define PARSE_ERROR_SECTION_NO_ENTRIES(exp, section)			\
APP_CHECK(exp, "Parse error in section \"%s\": no entries", section)

#define PARSE_WARNING_IGNORED(exp, section, entry)			\
do									\
if (!(exp))								\
	fprintf(stderr, "Parse warning in section \"%s\": "		\
		"entry \"%s\" is ignored", section, entry);		\
while (0)

#define PARSE_ERROR_INVALID(exp, section, entry)			\
APP_CHECK(exp, "Parse error in section \"%s\": unrecognized entry \"%s\"",\
	section, entry)

#define PARSE_ERROR_DUPLICATE(exp, section, entry)			\
APP_CHECK(exp, "Parse error in section \"%s\": duplicate entry \"%s\"",	\
	section, entry)

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

	PARSE_CHECK_DUPLICATE_SECTION_EAL(p);

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
			uint32_t i;

			for (i = 0; i < APP_MAX_LINKS; i++) {
				if (p->pci_blacklist[i])
					continue;

				p->pci_blacklist[i] =
					strdup(entry->value);
				PARSE_ERROR_MALLOC(p->pci_blacklist[i]);

				break;
			}

			PARSE_ERROR_MESSAGE((i < APP_MAX_LINKS),
				section_name, entry->name,
				"too many elements");
			continue;
		}

		/* pci_whitelist */
		if ((strcmp(entry->name, "pci_whitelist") == 0) ||
			(strcmp(entry->name, "w") == 0)) {
			uint32_t i;

			PARSE_ERROR_MESSAGE((app->port_mask != 0),
				section_name, entry->name, "entry to be "
				"generated by the application (port_mask "
				"not provided)");

			for (i = 0; i < APP_MAX_LINKS; i++) {
				if (p->pci_whitelist[i])
					continue;

				p->pci_whitelist[i] = strdup(entry->value);
				PARSE_ERROR_MALLOC(p->pci_whitelist[i]);

				break;
			}

			PARSE_ERROR_MESSAGE((i < APP_MAX_LINKS),
				section_name, entry->name,
				"too many elements");
			continue;
		}

		/* vdev */
		if (strcmp(entry->name, "vdev") == 0) {
			uint32_t i;

			for (i = 0; i < APP_MAX_LINKS; i++) {
				if (p->vdev[i])
					continue;

				p->vdev[i] = strdup(entry->value);
				PARSE_ERROR_MALLOC(p->vdev[i]);

				break;
			}

			PARSE_ERROR_MESSAGE((i < APP_MAX_LINKS),
				section_name, entry->name,
				"too many elements");
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

static void
parse_pipeline_pktq_in(struct app_params *app,
	struct app_pipeline_params *p,
	char *value)
{
	p->n_pktq_in = 0;

	while (1) {
		enum app_pktq_in_type type;
		int id;
		char *name = strtok_r(value, PARSE_DELIMITER, &value);

		if (name == NULL)
			break;

		PARSE_ERROR_TOO_MANY_ELEMENTS(
			(p->n_pktq_in < RTE_DIM(p->pktq_in)),
			p->name, "pktq_in", (uint32_t)RTE_DIM(p->pktq_in));

		if (validate_name(name, "RXQ", 2) == 0) {
			type = APP_PKTQ_IN_HWQ;
			id = APP_PARAM_ADD(app->hwq_in_params, name);
			APP_PARAM_ADD_LINK_FOR_RXQ(app, name);
		} else if (validate_name(name, "SWQ", 1) == 0) {
			type = APP_PKTQ_IN_SWQ;
			id = APP_PARAM_ADD(app->swq_params, name);
		} else if (validate_name(name, "TM", 1) == 0) {
			type = APP_PKTQ_IN_TM;
			id = APP_PARAM_ADD(app->tm_params, name);
			APP_PARAM_ADD_LINK_FOR_TM(app, name);
		} else if (validate_name(name, "TAP", 1) == 0) {
			type = APP_PKTQ_IN_TAP;
			id = APP_PARAM_ADD(app->tap_params, name);
		} else if (validate_name(name, "KNI", 1) == 0) {
			type = APP_PKTQ_IN_KNI;
			id = APP_PARAM_ADD(app->kni_params, name);
			APP_PARAM_ADD_LINK_FOR_KNI(app, name);
		} else if (validate_name(name, "SOURCE", 1) == 0) {
			type = APP_PKTQ_IN_SOURCE;
			id = APP_PARAM_ADD(app->source_params, name);
		} else
			PARSE_ERROR_INVALID_ELEMENT(0,
				p->name, "pktq_in", name);

		p->pktq_in[p->n_pktq_in].type = type;
		p->pktq_in[p->n_pktq_in].id = (uint32_t) id;
		p->n_pktq_in++;
	}

	PARSE_ERROR_NO_ELEMENTS((p->n_pktq_in > 0), p->name, "pktq_in");
}

static void
parse_pipeline_pktq_out(struct app_params *app,
	struct app_pipeline_params *p,
	char *value)
{
	p->n_pktq_out = 0;

	while (1) {
		enum app_pktq_out_type type;
		int id;
		char *name = strtok_r(value, PARSE_DELIMITER, &value);

		if (name == NULL)
			break;

		PARSE_ERROR_TOO_MANY_ELEMENTS(
			(p->n_pktq_out < RTE_DIM(p->pktq_out)),
			p->name, "pktq_out", (uint32_t)RTE_DIM(p->pktq_out));

		if (validate_name(name, "TXQ", 2) == 0) {
			type = APP_PKTQ_OUT_HWQ;
			id = APP_PARAM_ADD(app->hwq_out_params, name);
			APP_PARAM_ADD_LINK_FOR_TXQ(app, name);
		} else if (validate_name(name, "SWQ", 1) == 0) {
			type = APP_PKTQ_OUT_SWQ;
			id = APP_PARAM_ADD(app->swq_params, name);
		} else if (validate_name(name, "TM", 1) == 0) {
			type = APP_PKTQ_OUT_TM;
			id = APP_PARAM_ADD(app->tm_params, name);
			APP_PARAM_ADD_LINK_FOR_TM(app, name);
		} else if (validate_name(name, "TAP", 1) == 0) {
			type = APP_PKTQ_OUT_TAP;
			id = APP_PARAM_ADD(app->tap_params, name);
		} else if (validate_name(name, "KNI", 1) == 0) {
			type = APP_PKTQ_OUT_KNI;
			id = APP_PARAM_ADD(app->kni_params, name);
			APP_PARAM_ADD_LINK_FOR_KNI(app, name);
		} else if (validate_name(name, "SINK", 1) == 0) {
			type = APP_PKTQ_OUT_SINK;
			id = APP_PARAM_ADD(app->sink_params, name);
		} else
			PARSE_ERROR_INVALID_ELEMENT(0,
				p->name, "pktq_out", name);

		p->pktq_out[p->n_pktq_out].type = type;
		p->pktq_out[p->n_pktq_out].id = id;
		p->n_pktq_out++;
	}

	PARSE_ERROR_NO_ELEMENTS((p->n_pktq_out > 0), p->name, "pktq_out");
}

static void
parse_pipeline_msgq_in(struct app_params *app,
	struct app_pipeline_params *p,
	char *value)
{
	p->n_msgq_in = 0;

	while (1) {
		int idx;
		char *name = strtok_r(value, PARSE_DELIMITER, &value);

		if (name == NULL)
			break;

		PARSE_ERROR_TOO_MANY_ELEMENTS(
			(p->n_msgq_in < RTE_DIM(p->msgq_in)),
			p->name, "msgq_in", (uint32_t)(RTE_DIM(p->msgq_in)));

		PARSE_ERROR_INVALID_ELEMENT(
			(validate_name(name, "MSGQ", 1) == 0),
			p->name, "msgq_in", name);

		idx = APP_PARAM_ADD(app->msgq_params, name);
		p->msgq_in[p->n_msgq_in] = idx;
		p->n_msgq_in++;
	}

	PARSE_ERROR_NO_ELEMENTS((p->n_msgq_in > 0), p->name, "msgq_in");
}

static void
parse_pipeline_msgq_out(struct app_params *app,
	struct app_pipeline_params *p,
	char *value)
{
	p->n_msgq_out = 0;

	while (1) {
		int idx;
		char *name = strtok_r(value, PARSE_DELIMITER, &value);

		if (name == NULL)
			break;

		PARSE_ERROR_TOO_MANY_ELEMENTS(
			(p->n_msgq_out < RTE_DIM(p->msgq_out)),
			p->name, "msgq_out", (uint32_t)RTE_DIM(p->msgq_out));

		PARSE_ERROR_INVALID_ELEMENT(
			(validate_name(name, "MSGQ", 1) == 0),
			p->name, "msgq_out", name);

		idx = APP_PARAM_ADD(app->msgq_params, name);
		p->msgq_out[p->n_msgq_out] = idx;
		p->n_msgq_out++;
	}

	PARSE_ERROR_NO_ELEMENTS((p->n_msgq_out > 0), p->name, "msgq_out");
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
	int n_entries, i;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->pipeline_params, section_name);
	param = &app->pipeline_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "type") == 0) {
			int w_size = snprintf(param->type, RTE_DIM(param->type),
					"%s", ent->value);

			PARSE_ERROR(((w_size > 0) &&
				(w_size < (int)RTE_DIM(param->type))),
				section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "core") == 0) {
			int status = parse_pipeline_core(
				&param->socket_id, &param->core_id,
				&param->hyper_th_id, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "pktq_in") == 0) {
			parse_pipeline_pktq_in(app, param, ent->value);

			continue;
		}

		if (strcmp(ent->name, "pktq_out") == 0) {
			parse_pipeline_pktq_out(app, param, ent->value);

			continue;
		}

		if (strcmp(ent->name, "msgq_in") == 0) {
			parse_pipeline_msgq_in(app, param, ent->value);

			continue;
		}

		if (strcmp(ent->name, "msgq_out") == 0) {
			parse_pipeline_msgq_out(app, param, ent->value);

			continue;
		}

		if (strcmp(ent->name, "timer_period") == 0) {
			int status = parser_read_uint32(
				&param->timer_period,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* pipeline type specific items */
		APP_CHECK((param->n_args < APP_MAX_PIPELINE_ARGS),
			"Parse error in section \"%s\": too many "
			"pipeline specified parameters", section_name);

		param->args_name[param->n_args] = strdup(ent->name);
		param->args_value[param->n_args] = strdup(ent->value);

		APP_CHECK((param->args_name[param->n_args] != NULL) &&
			(param->args_value[param->n_args] != NULL),
			"Parse error: no free memory");

		param->n_args++;
	}

	snprintf(name, sizeof(name), "MSGQ-REQ-%s", section_name);
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;
	param->msgq_in[param->n_msgq_in++] = param_idx;

	snprintf(name, sizeof(name), "MSGQ-RSP-%s", section_name);
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;
	param->msgq_out[param->n_msgq_out++] = param_idx;

	snprintf(name, sizeof(name), "MSGQ-REQ-CORE-s%" PRIu32 "c%" PRIu32 "%s",
		param->socket_id,
		param->core_id,
		(param->hyper_th_id) ? "h" : "");
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
	app->msgq_params[param_idx].cpu_socket_id = param->socket_id;

	snprintf(name, sizeof(name), "MSGQ-RSP-CORE-s%" PRIu32 "c%" PRIu32 "%s",
		param->socket_id,
		param->core_id,
		(param->hyper_th_id) ? "h" : "");
	param_idx = APP_PARAM_ADD(app->msgq_params, name);
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
	int n_entries, i;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->mempool_params, section_name);
	param = &app->mempool_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "buffer_size") == 0) {
			int status = parser_read_uint32(
				&param->buffer_size, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "pool_size") == 0) {
			int status = parser_read_uint32(
				&param->pool_size, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "cache_size") == 0) {
			int status = parser_read_uint32(
				&param->cache_size, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "cpu") == 0) {
			int status = parser_read_uint32(
				&param->cpu_socket_id, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
	}

	free(entries);
}

static int
parse_link_rss_qs(struct app_link_params *p,
	char *value)
{
	p->n_rss_qs = 0;

	while (1) {
		char *token = strtok_r(value, PARSE_DELIMITER, &value);

		if (token == NULL)
			break;

		if (p->n_rss_qs == RTE_DIM(p->rss_qs))
			return -ENOMEM;

		if (parser_read_uint32(&p->rss_qs[p->n_rss_qs++], token))
			return -EINVAL;
	}

	return 0;
}

static int
parse_link_rss_proto_ipv4(struct app_link_params *p,
	char *value)
{
	uint64_t mask = 0;

	while (1) {
		char *token = strtok_r(value, PARSE_DELIMITER, &value);

		if (token == NULL)
			break;

		if (strcmp(token, "IP") == 0) {
			mask |= ETH_RSS_IPV4;
			continue;
		}
		if (strcmp(token, "FRAG") == 0) {
			mask |= ETH_RSS_FRAG_IPV4;
			continue;
		}
		if (strcmp(token, "TCP") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV4_TCP;
			continue;
		}
		if (strcmp(token, "UDP") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV4_UDP;
			continue;
		}
		if (strcmp(token, "SCTP") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV4_SCTP;
			continue;
		}
		if (strcmp(token, "OTHER") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV4_OTHER;
			continue;
		}
		return -EINVAL;
	}

	p->rss_proto_ipv4 = mask;
	return 0;
}

static int
parse_link_rss_proto_ipv6(struct app_link_params *p,
	char *value)
{
	uint64_t mask = 0;

	while (1) {
		char *token = strtok_r(value, PARSE_DELIMITER, &value);

		if (token == NULL)
			break;

		if (strcmp(token, "IP") == 0) {
			mask |= ETH_RSS_IPV6;
			continue;
		}
		if (strcmp(token, "FRAG") == 0) {
			mask |= ETH_RSS_FRAG_IPV6;
			continue;
		}
		if (strcmp(token, "TCP") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV6_TCP;
			continue;
		}
		if (strcmp(token, "UDP") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV6_UDP;
			continue;
		}
		if (strcmp(token, "SCTP") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV6_SCTP;
			continue;
		}
		if (strcmp(token, "OTHER") == 0) {
			mask |= ETH_RSS_NONFRAG_IPV6_OTHER;
			continue;
		}
		if (strcmp(token, "IP_EX") == 0) {
			mask |= ETH_RSS_IPV6_EX;
			continue;
		}
		if (strcmp(token, "TCP_EX") == 0) {
			mask |= ETH_RSS_IPV6_TCP_EX;
			continue;
		}
		if (strcmp(token, "UDP_EX") == 0) {
			mask |= ETH_RSS_IPV6_UDP_EX;
			continue;
		}
		return -EINVAL;
	}

	p->rss_proto_ipv6 = mask;
	return 0;
}

static int
parse_link_rss_proto_l2(struct app_link_params *p,
	char *value)
{
	uint64_t mask = 0;

	while (1) {
		char *token = strtok_r(value, PARSE_DELIMITER, &value);

		if (token == NULL)
			break;

		if (strcmp(token, "L2") == 0) {
			mask |= ETH_RSS_L2_PAYLOAD;
			continue;
		}
		return -EINVAL;
	}

	p->rss_proto_l2 = mask;
	return 0;
}

static void
parse_link(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_link_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, i;
	int rss_qs_present = 0;
	int rss_proto_ipv4_present = 0;
	int rss_proto_ipv6_present = 0;
	int rss_proto_l2_present = 0;
	int pci_bdf_present = 0;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->link_params, section_name);
	param = &app->link_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "promisc") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->promisc = status;
			continue;
		}

		if (strcmp(ent->name, "arp_q") == 0) {
			int status = parser_read_uint32(&param->arp_q,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "tcp_syn_q") == 0) {
			int status = parser_read_uint32(
				&param->tcp_syn_q, ent->value);

			PARSE_ERROR((status == 0), section_name, ent->name);
			continue;
		}

		if (strcmp(ent->name, "ip_local_q") == 0) {
			int status = parser_read_uint32(
				&param->ip_local_q, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "tcp_local_q") == 0) {
			int status = parser_read_uint32(
				&param->tcp_local_q, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "udp_local_q") == 0) {
			int status = parser_read_uint32(
				&param->udp_local_q, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "sctp_local_q") == 0) {
			int status = parser_read_uint32(
				&param->sctp_local_q, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "rss_qs") == 0) {
			int status = parse_link_rss_qs(param, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			rss_qs_present = 1;
			continue;
		}

		if (strcmp(ent->name, "rss_proto_ipv4") == 0) {
			int status =
				parse_link_rss_proto_ipv4(param, ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			rss_proto_ipv4_present = 1;
			continue;
		}

		if (strcmp(ent->name, "rss_proto_ipv6") == 0) {
			int status =
				parse_link_rss_proto_ipv6(param, ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			rss_proto_ipv6_present = 1;
			continue;
		}

		if (strcmp(ent->name, "rss_proto_l2") == 0) {
			int status = parse_link_rss_proto_l2(param, ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			rss_proto_l2_present = 1;
			continue;
		}

		if (strcmp(ent->name, "pci_bdf") == 0) {
			PARSE_ERROR_DUPLICATE((pci_bdf_present == 0),
				section_name, ent->name);

			snprintf(param->pci_bdf, APP_LINK_PCI_BDF_SIZE,
				"%s", ent->value);
			pci_bdf_present = 1;
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
	}

	/* Check for mandatory fields */
	if (app->port_mask)
		PARSE_ERROR_MESSAGE((pci_bdf_present == 0),
			section_name, "pci_bdf",
			"entry not allowed (port_mask is provided)");
	else
		PARSE_ERROR_MESSAGE((pci_bdf_present),
			section_name, "pci_bdf",
			"this entry is mandatory (port_mask is not "
			"provided)");

	if (rss_proto_ipv4_present)
		PARSE_ERROR_MESSAGE((rss_qs_present),
			section_name, "rss_proto_ipv4",
			"entry not allowed (rss_qs entry is not provided)");
	if (rss_proto_ipv6_present)
		PARSE_ERROR_MESSAGE((rss_qs_present),
			section_name, "rss_proto_ipv6",
			"entry not allowed (rss_qs entry is not provided)");
	if (rss_proto_l2_present)
		PARSE_ERROR_MESSAGE((rss_qs_present),
			section_name, "rss_proto_l2",
			"entry not allowed (rss_qs entry is not provided)");
	if (rss_proto_ipv4_present |
		rss_proto_ipv6_present |
		rss_proto_l2_present){
		if (rss_proto_ipv4_present == 0)
			param->rss_proto_ipv4 = 0;
		if (rss_proto_ipv6_present == 0)
			param->rss_proto_ipv6 = 0;
		if (rss_proto_l2_present == 0)
			param->rss_proto_l2 = 0;
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
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->hwq_in_params, section_name);
	param = &app->hwq_in_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	APP_PARAM_ADD_LINK_FOR_RXQ(app, section_name);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "mempool") == 0) {
			int status = validate_name(ent->value,
				"MEMPOOL", 1);
			ssize_t idx;

			PARSE_ERROR((status == 0), section_name,
				ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			param->mempool_id = idx;
			continue;
		}

		if (strcmp(ent->name, "size") == 0) {
			int status = parser_read_uint32(&param->size,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst") == 0) {
			int status = parser_read_uint32(&param->burst,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->hwq_out_params, section_name);
	param = &app->hwq_out_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	APP_PARAM_ADD_LINK_FOR_TXQ(app, section_name);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "size") == 0) {
			int status = parser_read_uint32(&param->size,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst") == 0) {
			int status = parser_read_uint32(&param->burst,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "dropless") == 0) {
			int status = parser_read_arg_bool(ent->value);


			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->dropless = status;
			continue;
		}

		if (strcmp(ent->name, "n_retries") == 0) {
			int status = parser_read_uint64(&param->n_retries,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	int n_entries, i;
	uint32_t mtu_present = 0;
	uint32_t metadata_size_present = 0;
	uint32_t mempool_direct_present = 0;
	uint32_t mempool_indirect_present = 0;

	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->swq_params, section_name);
	param = &app->swq_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "size") == 0) {
			int status = parser_read_uint32(&param->size,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst_read") == 0) {
			int status = parser_read_uint32(&
				param->burst_read, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst_write") == 0) {
			int status = parser_read_uint32(
				&param->burst_write, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "dropless") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->dropless = status;
			continue;
		}

		if (strcmp(ent->name, "n_retries") == 0) {
			int status = parser_read_uint64(&param->n_retries,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "cpu") == 0) {
			int status = parser_read_uint32(
				&param->cpu_socket_id, ent->value);

			PARSE_ERROR((status == 0), section_name, ent->name);
			continue;
		}

		if (strcmp(ent->name, "ipv4_frag") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);

			param->ipv4_frag = status;
			if (param->mtu == 0)
				param->mtu = 1500;

			continue;
		}

		if (strcmp(ent->name, "ipv6_frag") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->ipv6_frag = status;
			if (param->mtu == 0)
				param->mtu = 1320;
			continue;
		}

		if (strcmp(ent->name, "ipv4_ras") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->ipv4_ras = status;
			continue;
		}

		if (strcmp(ent->name, "ipv6_ras") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->ipv6_ras = status;
			continue;
		}

		if (strcmp(ent->name, "mtu") == 0) {
			int status = parser_read_uint32(&param->mtu,
					ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			mtu_present = 1;
			continue;
		}

		if (strcmp(ent->name, "metadata_size") == 0) {
			int status = parser_read_uint32(
				&param->metadata_size, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			metadata_size_present = 1;
			continue;
		}

		if (strcmp(ent->name, "mempool_direct") == 0) {
			int status = validate_name(ent->value,
				"MEMPOOL", 1);
			ssize_t idx;

			PARSE_ERROR((status == 0), section_name,
				ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			param->mempool_direct_id = idx;

			mempool_direct_present = 1;
			continue;
		}

		if (strcmp(ent->name, "mempool_indirect") == 0) {
			int status = validate_name(ent->value,
				"MEMPOOL", 1);
			ssize_t idx;

			PARSE_ERROR((status == 0), section_name,
				ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			param->mempool_indirect_id = idx;

			mempool_indirect_present = 1;
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
	}

	APP_CHECK(((mtu_present == 0) ||
		((param->ipv4_frag == 1) || (param->ipv6_frag == 1))),
		"Parse error in section \"%s\": IPv4/IPv6 fragmentation "
		"is off, therefore entry \"mtu\" is not allowed",
		section_name);

	APP_CHECK(((metadata_size_present == 0) ||
		((param->ipv4_frag == 1) || (param->ipv6_frag == 1))),
		"Parse error in section \"%s\": IPv4/IPv6 fragmentation "
		"is off, therefore entry \"metadata_size\" is "
		"not allowed", section_name);

	APP_CHECK(((mempool_direct_present == 0) ||
		((param->ipv4_frag == 1) || (param->ipv6_frag == 1))),
		"Parse error in section \"%s\": IPv4/IPv6 fragmentation "
		"is off, therefore entry \"mempool_direct\" is "
		"not allowed", section_name);

	APP_CHECK(((mempool_indirect_present == 0) ||
		((param->ipv4_frag == 1) || (param->ipv6_frag == 1))),
		"Parse error in section \"%s\": IPv4/IPv6 fragmentation "
		"is off, therefore entry \"mempool_indirect\" is "
		"not allowed", section_name);

	free(entries);
}

static void
parse_tm(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_tm_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->tm_params, section_name);
	param = &app->tm_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	APP_PARAM_ADD_LINK_FOR_TM(app, section_name);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "cfg") == 0) {
			param->file_name = strdup(ent->value);
			PARSE_ERROR_MALLOC(param->file_name != NULL);
			continue;
		}

		if (strcmp(ent->name, "burst_read") == 0) {
			int status = parser_read_uint32(
				&param->burst_read, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst_write") == 0) {
			int status = parser_read_uint32(
				&param->burst_write, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
	}

	free(entries);
}

static void
parse_tap(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_tap_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->tap_params, section_name);
	param = &app->tap_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "burst_read") == 0) {
			int status = parser_read_uint32(
				&param->burst_read, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst_write") == 0) {
			int status = parser_read_uint32(
				&param->burst_write, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "dropless") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
				ent->name);
			param->dropless = status;
			continue;
		}

		if (strcmp(ent->name, "n_retries") == 0) {
			int status = parser_read_uint64(&param->n_retries,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "mempool") == 0) {
			int status = validate_name(ent->value,
				"MEMPOOL", 1);
			ssize_t idx;

			PARSE_ERROR((status == 0), section_name,
				ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			param->mempool_id = idx;

			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
	}

	free(entries);
}

static void
parse_kni(struct app_params *app,
		  const char *section_name,
		  struct rte_cfgfile *cfg)
{
	struct app_pktq_kni_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->kni_params, section_name);
	param = &app->kni_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	APP_PARAM_ADD_LINK_FOR_KNI(app, section_name);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "core") == 0) {
			int status = parse_pipeline_core(
					&param->socket_id,
					&param->core_id,
					&param->hyper_th_id,
					ent->value);

			PARSE_ERROR((status == 0), section_name,
						ent->name);
			param->force_bind = 1;
			continue;
		}

		if (strcmp(ent->name, "mempool") == 0) {
			int status = validate_name(ent->value,
				"MEMPOOL", 1);
			ssize_t idx;

			PARSE_ERROR((status == 0), section_name,
						ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			param->mempool_id = idx;
			continue;
		}

		if (strcmp(ent->name, "burst_read") == 0) {
			int status = parser_read_uint32(&param->burst_read,
						ent->value);

			PARSE_ERROR((status == 0), section_name,
						ent->name);
			continue;
		}

		if (strcmp(ent->name, "burst_write") == 0) {
			int status = parser_read_uint32(&param->burst_write,
						ent->value);

			PARSE_ERROR((status == 0), section_name,
						ent->name);
			continue;
		}

		if (strcmp(ent->name, "dropless") == 0) {
			int status = parser_read_arg_bool(ent->value);

			PARSE_ERROR((status != -EINVAL), section_name,
						ent->name);
			param->dropless = status;
			continue;
		}

		if (strcmp(ent->name, "n_retries") == 0) {
			int status = parser_read_uint64(&param->n_retries,
						ent->value);

			PARSE_ERROR((status == 0), section_name,
						ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	int n_entries, i;
	ssize_t param_idx;
	uint32_t pcap_file_present = 0;
	uint32_t pcap_size_present = 0;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->source_params, section_name);
	param = &app->source_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "mempool") == 0) {
			int status = validate_name(ent->value,
				"MEMPOOL", 1);
			ssize_t idx;

			PARSE_ERROR((status == 0), section_name,
				ent->name);

			idx = APP_PARAM_ADD(app->mempool_params, ent->value);
			param->mempool_id = idx;
			continue;
		}

		if (strcmp(ent->name, "burst") == 0) {
			int status = parser_read_uint32(&param->burst,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "pcap_file_rd") == 0) {
			PARSE_ERROR_DUPLICATE((pcap_file_present == 0),
				section_name, ent->name);

			param->file_name = strdup(ent->value);

			PARSE_ERROR_MALLOC(param->file_name != NULL);
			pcap_file_present = 1;

			continue;
		}

		if (strcmp(ent->name, "pcap_bytes_rd_per_pkt") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((pcap_size_present == 0),
				section_name, ent->name);

			status = parser_read_uint32(
				&param->n_bytes_per_pkt, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			pcap_size_present = 1;

			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
	}

	free(entries);
}

static void
parse_sink(struct app_params *app,
	const char *section_name,
	struct rte_cfgfile *cfg)
{
	struct app_pktq_sink_params *param;
	struct rte_cfgfile_entry *entries;
	int n_entries, i;
	ssize_t param_idx;
	uint32_t pcap_file_present = 0;
	uint32_t pcap_n_pkt_present = 0;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->sink_params, section_name);
	param = &app->sink_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "pcap_file_wr") == 0) {
			PARSE_ERROR_DUPLICATE((pcap_file_present == 0),
				section_name, ent->name);

			param->file_name = strdup(ent->value);

			PARSE_ERROR_MALLOC((param->file_name != NULL));

			continue;
		}

		if (strcmp(ent->name, "pcap_n_pkt_wr") == 0) {
			int status;

			PARSE_ERROR_DUPLICATE((pcap_n_pkt_present == 0),
				section_name, ent->name);

			status = parser_read_uint32(
				&param->n_pkts_to_dump, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);

			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->msgq_params, section_name);
	param = &app->msgq_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "size") == 0) {
			int status = parser_read_uint32(&param->size,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->msgq_params, section_name);
	param = &app->msgq_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "size") == 0) {
			int status = parser_read_uint32(&param->size,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	int n_entries, i;
	ssize_t param_idx;

	n_entries = rte_cfgfile_section_num_entries(cfg, section_name);
	PARSE_ERROR_SECTION_NO_ENTRIES((n_entries > 0), section_name);

	entries = malloc(n_entries * sizeof(struct rte_cfgfile_entry));
	PARSE_ERROR_MALLOC(entries != NULL);

	rte_cfgfile_section_entries(cfg, section_name, entries, n_entries);

	param_idx = APP_PARAM_ADD(app->msgq_params, section_name);
	param = &app->msgq_params[param_idx];
	PARSE_CHECK_DUPLICATE_SECTION(param);

	for (i = 0; i < n_entries; i++) {
		struct rte_cfgfile_entry *ent = &entries[i];

		if (strcmp(ent->name, "size") == 0) {
			int status = parser_read_uint32(&param->size,
				ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		if (strcmp(ent->name, "cpu") == 0) {
			int status = parser_read_uint32(
				&param->cpu_socket_id, ent->value);

			PARSE_ERROR((status == 0), section_name,
				ent->name);
			continue;
		}

		/* unrecognized */
		PARSE_ERROR_INVALID(0, section_name, ent->name);
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
	{"TAP", 1, parse_tap},
	{"KNI", 1, parse_kni},
	{"SOURCE", 1, parse_source},
	{"SINK", 1, parse_sink},
	{"MSGQ-REQ-PIPELINE", 1, parse_msgq_req_pipeline},
	{"MSGQ-RSP-PIPELINE", 1, parse_msgq_rsp_pipeline},
	{"MSGQ", 1, parse_msgq},
};

static void
create_implicit_mempools(struct app_params *app)
{
	APP_PARAM_ADD(app->mempool_params, "MEMPOOL0");
}

static void
create_implicit_links_from_port_mask(struct app_params *app,
	uint64_t port_mask)
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

		app->link_params[idx].pmd_id = pmd_id;
		link_id++;
	}
}

static void
assign_link_pmd_id_from_pci_bdf(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *link = &app->link_params[i];

		APP_CHECK((strlen(link->pci_bdf)),
			"Parse error: %s pci_bdf is not configured "
			"(port_mask is not provided)",
			link->name);

		link->pmd_id = i;
	}
}

int
app_config_parse(struct app_params *app, const char *file_name)
{
	struct rte_cfgfile *cfg;
	char **section_names;
	int i, j, sect_count;

	/* Implicit mempools */
	create_implicit_mempools(app);

	/* Port mask */
	if (app->port_mask)
		create_implicit_links_from_port_mask(app, app->port_mask);

	/* Load application configuration file */
	cfg = rte_cfgfile_load(file_name, 0);
	APP_CHECK((cfg != NULL), "Parse error: Unable to load config "
		"file %s", file_name);

	sect_count = rte_cfgfile_num_sections(cfg, NULL, 0);
	APP_CHECK((sect_count > 0), "Parse error: number of sections "
		"in file \"%s\" return %d", file_name,
		sect_count);

	section_names = malloc(sect_count * sizeof(char *));
	PARSE_ERROR_MALLOC(section_names != NULL);

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
			"Parse error: unknown section %s",
			section_names[i]);

		APP_CHECK(validate_name(section_names[i],
			sch_s->prefix,
			sch_s->numbers) == 0,
			"Parse error: invalid section name \"%s\"",
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
	APP_PARAM_COUNT(app->tap_params, app->n_pktq_tap);
	APP_PARAM_COUNT(app->kni_params, app->n_pktq_kni);
	APP_PARAM_COUNT(app->source_params, app->n_pktq_source);
	APP_PARAM_COUNT(app->sink_params, app->n_pktq_sink);
	APP_PARAM_COUNT(app->msgq_params, app->n_msgq);
	APP_PARAM_COUNT(app->pipeline_params, app->n_pipelines);

	if (app->port_mask == 0)
		assign_link_pmd_id_from_pci_bdf(app);

	/* Save configuration to output file */
	app_config_save(app, app->output_file);

	/* Load TM configuration files */
	app_config_parse_tm(app);

	return 0;
}

static void
save_eal_params(struct app_params *app, FILE *f)
{
	struct app_eal_params *p = &app->eal_params;
	uint32_t i;

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

	for (i = 0; i < APP_MAX_LINKS; i++) {
		if (p->pci_blacklist[i] == NULL)
			break;

		fprintf(f, "%s = %s\n", "pci_blacklist",
			p->pci_blacklist[i]);
	}

	for (i = 0; i < APP_MAX_LINKS; i++) {
		if (p->pci_whitelist[i] == NULL)
			break;

		fprintf(f, "%s = %s\n", "pci_whitelist",
			p->pci_whitelist[i]);
	}

	for (i = 0; i < APP_MAX_LINKS; i++) {
		if (p->vdev[i] == NULL)
			break;

		fprintf(f, "%s = %s\n", "vdev",
			p->vdev[i]);
	}

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
		fprintf(f, "%s = %s\n", "promisc", p->promisc ? "yes" : "no");
		fprintf(f, "%s = %" PRIu32 "\n", "arp_q", p->arp_q);
		fprintf(f, "%s = %" PRIu32 "\n", "tcp_syn_q",
			p->tcp_syn_q);
		fprintf(f, "%s = %" PRIu32 "\n", "ip_local_q", p->ip_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "tcp_local_q", p->tcp_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "udp_local_q", p->udp_local_q);
		fprintf(f, "%s = %" PRIu32 "\n", "sctp_local_q",
			p->sctp_local_q);

		if (p->n_rss_qs) {
			uint32_t j;

			/* rss_qs */
			fprintf(f, "rss_qs = ");
			for (j = 0; j < p->n_rss_qs; j++)
				fprintf(f, "%" PRIu32 " ",	p->rss_qs[j]);
			fputc('\n', f);

			/* rss_proto_ipv4 */
			if (p->rss_proto_ipv4) {
				fprintf(f, "rss_proto_ipv4 = ");
				if (p->rss_proto_ipv4 & ETH_RSS_IPV4)
					fprintf(f, "IP ");
				if (p->rss_proto_ipv4 & ETH_RSS_FRAG_IPV4)
					fprintf(f, "FRAG ");
				if (p->rss_proto_ipv4 &
					ETH_RSS_NONFRAG_IPV4_TCP)
					fprintf(f, "TCP ");
				if (p->rss_proto_ipv4 &
					ETH_RSS_NONFRAG_IPV4_UDP)
					fprintf(f, "UDP ");
				if (p->rss_proto_ipv4 &
					ETH_RSS_NONFRAG_IPV4_SCTP)
					fprintf(f, "SCTP ");
				if (p->rss_proto_ipv4 &
					ETH_RSS_NONFRAG_IPV4_OTHER)
					fprintf(f, "OTHER ");
				fprintf(f, "\n");
			} else
				fprintf(f, "; rss_proto_ipv4 = <NONE>\n");

			/* rss_proto_ipv6 */
			if (p->rss_proto_ipv6) {
				fprintf(f, "rss_proto_ipv6 = ");
				if (p->rss_proto_ipv6 & ETH_RSS_IPV6)
					fprintf(f, "IP ");
				if (p->rss_proto_ipv6 & ETH_RSS_FRAG_IPV6)
					fprintf(f, "FRAG ");
				if (p->rss_proto_ipv6 &
					ETH_RSS_NONFRAG_IPV6_TCP)
					fprintf(f, "TCP ");
				if (p->rss_proto_ipv6 &
					ETH_RSS_NONFRAG_IPV6_UDP)
					fprintf(f, "UDP ");
				if (p->rss_proto_ipv6 &
					ETH_RSS_NONFRAG_IPV6_SCTP)
					fprintf(f, "SCTP ");
				if (p->rss_proto_ipv6 &
					ETH_RSS_NONFRAG_IPV6_OTHER)
					fprintf(f, "OTHER ");
				if (p->rss_proto_ipv6 & ETH_RSS_IPV6_EX)
					fprintf(f, "IP_EX ");
				if (p->rss_proto_ipv6 &
					ETH_RSS_IPV6_TCP_EX)
					fprintf(f, "TCP_EX ");
				if (p->rss_proto_ipv6 &
					ETH_RSS_IPV6_UDP_EX)
					fprintf(f, "UDP_EX ");
				fprintf(f, "\n");
			} else
				fprintf(f, "; rss_proto_ipv6 = <NONE>\n");

			/* rss_proto_l2 */
			if (p->rss_proto_l2) {
				fprintf(f, "rss_proto_l2 = ");
				if (p->rss_proto_l2 & ETH_RSS_L2_PAYLOAD)
					fprintf(f, "L2 ");
				fprintf(f, "\n");
			} else
				fprintf(f, "; rss_proto_l2 = <NONE>\n");
		} else {
			fprintf(f, "; rss_qs = <NONE>\n");
			fprintf(f, "; rss_proto_ipv4 = <NONE>\n");
			fprintf(f, "; rss_proto_ipv6 = <NONE>\n");
			fprintf(f, "; rss_proto_l2 = <NONE>\n");
		}

		if (strlen(p->pci_bdf))
			fprintf(f, "%s = %s\n", "pci_bdf", p->pci_bdf);

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
		fprintf(f, "%s = %" PRIu64 "\n", "n_retries", p->n_retries);

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
		fprintf(f, "%s = %s\n", "ipv4_frag", p->ipv4_frag ? "yes" : "no");
		fprintf(f, "%s = %s\n", "ipv6_frag", p->ipv6_frag ? "yes" : "no");
		fprintf(f, "%s = %s\n", "ipv4_ras", p->ipv4_ras ? "yes" : "no");
		fprintf(f, "%s = %s\n", "ipv6_ras", p->ipv6_ras ? "yes" : "no");
		if ((p->ipv4_frag == 1) || (p->ipv6_frag == 1)) {
			fprintf(f, "%s = %" PRIu32 "\n", "mtu", p->mtu);
			fprintf(f, "%s = %" PRIu32 "\n", "metadata_size", p->metadata_size);
			fprintf(f, "%s = %s\n",
				"mempool_direct",
				app->mempool_params[p->mempool_direct_id].name);
			fprintf(f, "%s = %s\n",
				"mempool_indirect",
				app->mempool_params[p->mempool_indirect_id].name);
		}

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
save_tap_params(struct app_params *app, FILE *f)
{
	struct app_pktq_tap_params *p;
	size_t i, count;

	count = RTE_DIM(app->tap_params);
	for (i = 0; i < count; i++) {
		p = &app->tap_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %" PRIu32 "\n", "burst_read", p->burst_read);
		fprintf(f, "%s = %" PRIu32 "\n", "burst_write", p->burst_write);
		fprintf(f, "%s = %s\n", "dropless", p->dropless ? "yes" : "no");
		fprintf(f, "%s = %" PRIu64 "\n", "n_retries", p->n_retries);
		fprintf(f, "%s = %s\n", "mempool",
			app->mempool_params[p->mempool_id].name);

		fputc('\n', f);
	}
}

static void
save_kni_params(struct app_params *app, FILE *f)
{
	struct app_pktq_kni_params *p;
	size_t i, count;

	count = RTE_DIM(app->kni_params);
	for (i = 0; i < count; i++) {
		p = &app->kni_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		/* section name */
		fprintf(f, "[%s]\n", p->name);

		/* core */
		if (p->force_bind) {
			fprintf(f, "; force_bind = 1\n");
			fprintf(f, "core = s%" PRIu32 "c%" PRIu32 "%s\n",
					p->socket_id,
					p->core_id,
					(p->hyper_th_id) ? "h" : "");
		} else
			fprintf(f, "; force_bind = 0\n");

		/* mempool */
		fprintf(f, "%s = %s\n", "mempool",
				app->mempool_params[p->mempool_id].name);

		/* burst_read */
		fprintf(f, "%s = %" PRIu32 "\n", "burst_read", p->burst_read);

		/* burst_write */
		fprintf(f, "%s = %" PRIu32 "\n", "burst_write", p->burst_write);

		/* dropless */
		fprintf(f, "%s = %s\n",
				"dropless",
				p->dropless ? "yes" : "no");

		/* n_retries */
		fprintf(f, "%s = %" PRIu64 "\n", "n_retries", p->n_retries);

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
		fprintf(f, "%s = %s\n", "pcap_file_rd", p->file_name);
		fprintf(f, "%s = %" PRIu32 "\n", "pcap_bytes_rd_per_pkt",
			p->n_bytes_per_pkt);
		fputc('\n', f);
	}
}

static void
save_sink_params(struct app_params *app, FILE *f)
{
	struct app_pktq_sink_params *p;
	size_t i, count;

	count = RTE_DIM(app->sink_params);
	for (i = 0; i < count; i++) {
		p = &app->sink_params[i];
		if (!APP_PARAM_VALID(p))
			continue;

		fprintf(f, "[%s]\n", p->name);
		fprintf(f, "%s = %s\n", "pcap_file_wr", p->file_name);
		fprintf(f, "%s = %" PRIu32 "\n",
				"pcap_n_pkt_wr", p->n_pkts_to_dump);
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
				case APP_PKTQ_IN_TAP:
					name = app->tap_params[pp->id].name;
					break;
				case APP_PKTQ_IN_KNI:
					name = app->kni_params[pp->id].name;
					break;
				case APP_PKTQ_IN_SOURCE:
					name = app->source_params[pp->id].name;
					break;
				default:
					APP_CHECK(0, "System error "
						"occurred while saving "
						"parameter to file");
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
				case APP_PKTQ_OUT_TAP:
					name = app->tap_params[pp->id].name;
					break;
				case APP_PKTQ_OUT_KNI:
					name = app->kni_params[pp->id].name;
					break;
				case APP_PKTQ_OUT_SINK:
					name = app->sink_params[pp->id].name;
					break;
				default:
					APP_CHECK(0, "System error "
						"occurred while saving "
						"parameter to file");
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
		if (p->n_msgq_out) {
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
		"Error: need write access privilege to directory "
		"\"%s\" to save configuration\n", dir_name);

	file = fopen(file_name, "w");
	APP_CHECK((file != NULL),
		"Error: failed to save configuration to file \"%s\"",
		file_name);

	save_eal_params(app, file);
	save_pipeline_params(app, file);
	save_mempool_params(app, file);
	save_links_params(app, file);
	save_rxq_params(app, file);
	save_txq_params(app, file);
	save_swq_params(app, file);
	save_tm_params(app, file);
	save_tap_params(app, file);
	save_kni_params(app, file);
	save_source_params(app, file);
	save_sink_params(app, file);
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

	for (i = 0; i < RTE_DIM(app->tap_params); i++)
		memcpy(&app->tap_params[i],
			&default_tap_params,
			sizeof(default_tap_params));

	for (i = 0; i < RTE_DIM(app->kni_params); i++)
		memcpy(&app->kni_params[i],
			   &default_kni_params,
			   sizeof(default_kni_params));

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

static char *
filenamedup(const char *filename, const char *suffix)
{
	char *s = malloc(strlen(filename) + strlen(suffix) + 1);

	if (!s)
		return NULL;

	sprintf(s, "%s%s", filename, suffix);
	return s;
}

int
app_config_args(struct app_params *app, int argc, char **argv)
{
	const char *optname;
	int opt, option_index;
	int f_present, s_present, p_present, l_present;
	int preproc_present, preproc_params_present;
	int scaned = 0;

	static struct option lgopts[] = {
		{ "preproc", 1, 0, 0 },
		{ "preproc-args", 1, 0, 0 },
		{ NULL,  0, 0, 0 }
	};

	/* Copy application name */
	strncpy(app->app_name, argv[0], APP_APPNAME_SIZE - 1);

	f_present = 0;
	s_present = 0;
	p_present = 0;
	l_present = 0;
	preproc_present = 0;
	preproc_params_present = 0;

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

		case 0:
			optname = lgopts[option_index].name;

			if (strcmp(optname, "preproc") == 0) {
				if (preproc_present)
					rte_panic("Error: Preprocessor argument "
						"is provided more than once\n");
				preproc_present = 1;

				app->preproc = strdup(optarg);
				break;
			}

			if (strcmp(optname, "preproc-args") == 0) {
				if (preproc_params_present)
					rte_panic("Error: Preprocessor args "
						"are provided more than once\n");
				preproc_params_present = 1;

				app->preproc_args = strdup(optarg);
				break;
			}

			app_print_usage(argv[0]);
			break;

		default:
			app_print_usage(argv[0]);
		}

	optind = 1; /* reset getopt lib */

	/* Check dependencies between args */
	if (preproc_params_present && (preproc_present == 0))
		rte_panic("Error: Preprocessor args specified while "
			"preprocessor is not defined\n");

	app->parser_file = preproc_present ?
		filenamedup(app->config_file, ".preproc") :
		strdup(app->config_file);
	app->output_file = filenamedup(app->config_file, ".out");

	return 0;
}

int
app_config_preproc(struct app_params *app)
{
	char buffer[256];
	int status;

	if (app->preproc == NULL)
		return 0;

	status = access(app->config_file, F_OK | R_OK);
	APP_CHECK((status == 0), "Error: Unable to open file %s",
		app->config_file);

	snprintf(buffer, sizeof(buffer), "%s %s %s > %s",
		app->preproc,
		app->preproc_args ? app->preproc_args : "",
		app->config_file,
		app->parser_file);

	status = system(buffer);
	APP_CHECK((WIFEXITED(status) && (WEXITSTATUS(status) == 0)),
		"Error occurred while pre-processing file \"%s\"\n",
		app->config_file);

	return status;
}
