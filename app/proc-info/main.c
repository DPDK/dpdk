/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <strings.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_branch_prediction.h>
#include <rte_string_fns.h>
#ifdef RTE_LIB_METRICS
#include <rte_metrics.h>
#endif
#include <rte_cycles.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security.h>
#endif
#include <rte_cryptodev.h>
#include <rte_tm.h>
#include <rte_hexdump.h>
#include <rte_version.h>
#include <rte_eventdev.h>

/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64
#define MAX_STRING_LEN 256

#define ETHDEV_FWVERS_LEN 32
#define RTE_RETA_CONF_GROUP_NUM 32
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define EEPROM_DUMP_CHUNKSIZE 1024

#define STATS_BDR_FMT "========================================"
#define STATS_BDR_STR(w, s) printf("%.*s%s%.*s\n", w, \
	STATS_BDR_FMT, s, w, STATS_BDR_FMT)

/* mask of enabled ports */
static unsigned long enabled_port_mask;
/* Enable stats. */
static uint32_t enable_stats;
/* Enable xstats. */
static uint32_t enable_xstats;
/* Enable collectd format */
static uint32_t enable_collectd_format;
/* FD to send collectd format messages to STDOUT */
static int stdout_fd;
/* Host id process is running on */
static char host_id[MAX_LONG_OPT_SZ];
#ifdef RTE_LIB_METRICS
/* Enable metrics. */
static uint32_t enable_metrics;
#endif
/* Enable stats reset. */
static uint32_t reset_stats;
/* Enable xstats reset. */
static uint32_t reset_xstats;
/* Enable memory info. */
static uint32_t mem_info;
/* Enable displaying xstat name. */
static uint32_t enable_xstats_name;
static char *xstats_name;

/* Enable xstats by ids. */
#define MAX_NB_XSTATS_IDS 1024
static uint32_t nb_xstats_ids;
static uint64_t xstats_ids[MAX_NB_XSTATS_IDS];

/* show border */
static char bdr_str[MAX_STRING_LEN];

/* Enable show port. */
static uint32_t enable_shw_port;
/* Enable show port private info. */
static uint32_t enable_shw_port_priv;
/* Enable show tm. */
static uint32_t enable_shw_tm;
/* Enable show crypto. */
static uint32_t enable_shw_crypto;
/* Enable show ring. */
static uint32_t enable_shw_ring;
static char *ring_name;
/* Enable show mempool. */
static uint32_t enable_shw_mempool;
static char *mempool_name;
/* Enable iter mempool. */
static uint32_t enable_iter_mempool;
static char *mempool_iter_name;
/* Enable dump regs. */
static uint32_t enable_dump_regs;
static char *dump_regs_file_prefix;
/* Enable show DPDK version. */
static uint32_t enable_shw_version;
/* Enable show ethdev firmware version. */
static uint32_t enable_shw_fw_version;
/* Enable show RSS reta. */
static uint32_t enable_shw_rss_reta;
/* Enable show module eeprom information. */
static uint32_t enable_shw_module_eeprom;

/* Enable dump Rx/Tx descriptor. */
static uint32_t enable_shw_rx_desc_dump;
static uint32_t enable_shw_tx_desc_dump;

/* Note: Port_queue_id in xstats APIs is 8 bits, so we have a maximum of
 * 256 ports and queues for event_Dev
 */
#define MAX_PORTS_QUEUES 256

struct eventdev_params {
	uint16_t ports[MAX_PORTS_QUEUES];
	uint16_t queues[MAX_PORTS_QUEUES];
	uint16_t num_queues;
	uint16_t num_ports;
	uint8_t shw_all_queues:1,
		shw_all_ports:1,
		dump_xstats:1,
		reset_xstats:1,
		shw_device_xstats:1;
};

static struct eventdev_params eventdev_var[RTE_EVENT_MAX_DEVS];

#define DESC_PARAM_NUM 3

struct desc_param {
	uint16_t queue_id; /* A queue identifier on this port. */
	uint16_t offset;   /* The offset of the descriptor starting from tail. */
	uint16_t num;      /* The number of the descriptors to dump. */
};

static struct desc_param rx_desc_param;
static struct desc_param tx_desc_param;

/* display usage */
static void
proc_info_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK\n"
		"  -m to display DPDK memory zones, segments and TAILQ information\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to retrieve stats for\n"
		"  --stats: to display port statistics, enabled by default\n"
		"  --xstats: to display extended port statistics, disabled by "
			"default\n"
#ifdef RTE_LIB_METRICS
		"  --metrics: to display derived metrics of the ports, disabled by "
			"default\n"
#endif
		"  --xstats-name NAME: to display single xstat id by NAME\n"
		"  --xstats-ids IDLIST: to display xstat values by id. "
			"The argument is comma-separated list of xstat ids to print out.\n"
		"  --stats-reset: to reset port statistics\n"
		"  --xstats-reset: to reset port extended statistics\n"
		"  --collectd-format: to print statistics to STDOUT in expected by collectd format\n"
		"  --host-id STRING: host id used to identify the system process is running on\n"
		"  --show-port: to display ports information\n"
		"  --show-port-private: to display ports private information\n"
		"  --show-tm: to display traffic manager information for ports\n"
		"  --show-crypto: to display crypto information\n"
		"  --show-ring[=name]: to display ring information\n"
		"  --show-mempool[=name]: to display mempool information\n"
		"  --version: to display DPDK version\n"
		"  --firmware-version: to display ethdev firmware version\n"
		"  --show-rss-reta: to display ports redirection table\n"
		"  --show-module-eeprom: to display ports module eeprom information\n"
		"  --show-rx-descriptor queue_id:offset:num to display ports Rx descriptor information. "
			"queue_id: A Rx queue identifier on this port. "
			"offset: The offset of the descriptor starting from tail. "
			"num: The number of the descriptors to dump.\n"
		"  --show-tx-descriptor queue_id:offset:num to display ports Tx descriptor information. "
			"queue_id: A Tx queue identifier on this port. "
			"offset: The offset of the descriptor starting from tail. "
			"num: The number of the descriptors to dump.\n"
		"  --iter-mempool=name: iterate mempool elements to display content\n"
		"  --dump-regs=file-prefix: dump registers to file with the file-prefix\n"
		"  --show-edev-queue-xstats=queue_num:evdev_id or *:evdev_id to get queue xstats for specified queue or all queues;\n"
		"  --show-edev-port-xstats=port_num:evdev_id or *:evdev_id to get queue xstats for specified port or all ports;\n"
		"  --edev-dump-xstats=evdev_id to dump all event_dev xstats for specified eventdev device;\n"
		"  --edev-reset-xstats=evdev_id to reset event_dev xstats after reading;\n"
		"  --show-edev-device-xstats=evdev_id to get event_dev device xstats for specified eventdev device;\n",
		prgname);
}

/*
 * Parse the portmask provided at run time.
 */
static int
parse_portmask(const char *portmask)
{
	char *end = NULL;

	errno = 0;

	/* parse hexadecimal string */
	enabled_port_mask = strtoul(portmask, &end, 16);
	if (portmask[0] == '\0' || end == NULL || *end != '\0' || errno != 0) {
		fprintf(stderr, "Invalid portmask '%s'\n", portmask);
		return -1;
	}

	return 0;
}

/*
 * Parse ids value list into array
 */
static int
parse_xstats_ids(char *list, uint64_t *ids, int limit) {
	int length;
	char *token;
	char *ctx = NULL;
	char *endptr;

	length = 0;
	token = strtok_r(list, ",", &ctx);
	while (token != NULL) {
		ids[length] = strtoull(token, &endptr, 10);
		if (*endptr != '\0')
			return -EINVAL;

		length++;
		if (length >= limit)
			return -E2BIG;

		token = strtok_r(NULL, ",", &ctx);
	}

	return length;
}

static int
parse_descriptor_param(char *list, struct desc_param *desc)
{
	int ret;

	ret = sscanf(list, "%hu:%hu:%hu", &desc->queue_id, &desc->offset,
		     &desc->num);
	if (ret != DESC_PARAM_NUM)
		return -EINVAL;

	return 0;
}

static int
parse_eventdev_id(const char *str)
{
	unsigned long evdev_id;
	char *endp;

	evdev_id = strtoul(str, &endp, 0);

	if (*str == '\0' || *endp != '\0' || evdev_id >= rte_event_dev_count()) {
		fprintf(stderr, "Invalid eventdev id: %s\n", str);
		return -1;
	}

	return evdev_id;
}

static int
parse_eventdev_dump_xstats_params(const char *list)
{
	int evdev_id = parse_eventdev_id(list);

	if (evdev_id < 0)
		return -EINVAL;

	eventdev_var[evdev_id].dump_xstats = 1;

	return 0;
}

static int
parse_eventdev_reset_xstats_params(const char *list)
{
	int evdev_id = parse_eventdev_id(list);

	if (evdev_id < 0)
		return -EINVAL;

	eventdev_var[evdev_id].reset_xstats = 1;

	return 0;
}

static int
parse_eventdev_device_xstats_params(const char *list)
{
	int evdev_id = parse_eventdev_id(list);

	if (evdev_id < 0)
		return -EINVAL;

	eventdev_var[evdev_id].shw_device_xstats = 1;

	return 0;
}

static int
parse_eventdev_queue_xstats_params(const char *list)
{
	uint16_t queue_id;
	uint16_t evdev_id;

	if (sscanf(list, "*:%hu", &evdev_id) == 1) {
		if (evdev_id >= rte_event_dev_count()) {
			printf("Invalid eventdev id: %hu\n", evdev_id);
			return -EINVAL;
		}
		eventdev_var[evdev_id].shw_all_queues = 1;
	} else if (sscanf(list, "%hu:%hu", &queue_id, &evdev_id) == 2) {
		if (evdev_id >= rte_event_dev_count()) {
			printf("Invalid eventdev id: %hu\n", evdev_id);
			return -EINVAL;
		}

		if (queue_id >= MAX_PORTS_QUEUES) {
			printf("Invalid queue_id: %hu\n", queue_id);
			return -EINVAL;
		}

		eventdev_var[evdev_id].queues[eventdev_var[evdev_id].num_queues] = queue_id;
		eventdev_var[evdev_id].num_queues++;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
parse_eventdev_port_xstats_params(const char *list)
{
	uint16_t port_id;
	uint16_t evdev_id;

	if (sscanf(list, "*:%hu", &evdev_id) == 1) {
		if (evdev_id >= rte_event_dev_count()) {
			printf("Invalid eventdev id: %hu\n", evdev_id);
			return -EINVAL;
		}
		eventdev_var[evdev_id].shw_all_ports = 1;
	} else if (sscanf(list, "%hu:%hu", &port_id, &evdev_id) == 2) {
		if (evdev_id >= rte_event_dev_count()) {
			printf("Invalid eventdev id: %hu\n", evdev_id);
			return -EINVAL;
		}

		if (port_id >= MAX_PORTS_QUEUES) {
			printf("Invalid port_id: %hu\n", port_id);
			return -EINVAL;
		}

		eventdev_var[evdev_id].ports[eventdev_var[evdev_id].num_ports] = port_id;
		eventdev_var[evdev_id].num_ports++;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
proc_info_preparse_args(int argc, char **argv)
{
	char *prgname = argv[0];
	int i;

	for (i = 0; i < argc; i++) {
		/* Print stats or xstats to STDOUT in collectd format */
		if (!strncmp(argv[i], "--collectd-format", MAX_LONG_OPT_SZ)) {
			enable_collectd_format = 1;
			stdout_fd = dup(STDOUT_FILENO);
			close(STDOUT_FILENO);
		}
		if (!strncmp(argv[i], "--host-id", MAX_LONG_OPT_SZ)) {
			if ((i + 1) == argc) {
				printf("Invalid host id or not specified\n");
				proc_info_usage(prgname);
				return -1;
			}
			strlcpy(host_id, argv[i + 1], sizeof(host_id));
		}
	}

	if (!strlen(host_id)) {
		int err = gethostname(host_id, MAX_LONG_OPT_SZ-1);

		if (err)
			strlcpy(host_id, "unknown", sizeof(host_id));
	}

	return 0;
}

/* Parse the argument given in the command line of the application */
static int
proc_info_parse_args(int argc, char **argv)
{
	int opt;
	int option_index;
	char *prgname = argv[0];
	static struct option long_option[] = {
		{"stats", 0, NULL, 0},
		{"stats-reset", 0, NULL, 0},
		{"xstats", 0, NULL, 0},
#ifdef RTE_LIB_METRICS
		{"metrics", 0, NULL, 0},
#endif
		{"xstats-reset", 0, NULL, 0},
		{"xstats-name", required_argument, NULL, 1},
		{"collectd-format", 0, NULL, 0},
		{"xstats-ids", 1, NULL, 1},
		{"host-id", 0, NULL, 0},
		{"show-port", 0, NULL, 0},
		{"show-port-private", 0, NULL, 0},
		{"show-tm", 0, NULL, 0},
		{"show-crypto", 0, NULL, 0},
		{"show-ring", optional_argument, NULL, 0},
		{"show-mempool", optional_argument, NULL, 0},
		{"iter-mempool", required_argument, NULL, 0},
		{"dump-regs", required_argument, NULL, 0},
		{"version", 0, NULL, 0},
		{"firmware-version", 0, NULL, 0},
		{"show-rss-reta", 0, NULL, 0},
		{"show-module-eeprom", 0, NULL, 0},
		{"show-rx-descriptor", required_argument, NULL, 1},
		{"show-tx-descriptor", required_argument, NULL, 1},
		{"show-edev-queue-xstats", required_argument, NULL, 0},
		{"show-edev-port-xstats", required_argument, NULL, 0},
		{"edev-dump-xstats", required_argument, NULL, 0},
		{"edev-reset-xstats", required_argument, NULL, 0},
		{"show-edev-device-xstats", required_argument, NULL, 0},
		{NULL, 0, 0, 0}
	};

	if (argc == 1)
		proc_info_usage(prgname);

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, "p:m",
			long_option, &option_index)) != EOF) {
		switch (opt) {
		/* portmask */
		case 'p':
			if (parse_portmask(optarg) < 0) {
				proc_info_usage(prgname);
				return -1;
			}
			break;
		case 'm':
			mem_info = 1;
			break;
		case 0:
			/* Print stats */
			if (!strncmp(long_option[option_index].name, "stats",
					MAX_LONG_OPT_SZ))
				enable_stats = 1;
			/* Print xstats */
			else if (!strncmp(long_option[option_index].name, "xstats",
					MAX_LONG_OPT_SZ))
				enable_xstats = 1;
#ifdef RTE_LIB_METRICS
			else if (!strncmp(long_option[option_index].name,
					"metrics",
					MAX_LONG_OPT_SZ))
				enable_metrics = 1;
#endif
			/* Reset stats */
			if (!strncmp(long_option[option_index].name, "stats-reset",
					MAX_LONG_OPT_SZ))
				reset_stats = 1;
			/* Reset xstats */
			else if (!strncmp(long_option[option_index].name, "xstats-reset",
					MAX_LONG_OPT_SZ))
				reset_xstats = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-port", MAX_LONG_OPT_SZ))
				enable_shw_port = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-port-private", MAX_LONG_OPT_SZ))
				enable_shw_port_priv = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-tm", MAX_LONG_OPT_SZ))
				enable_shw_tm = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-crypto", MAX_LONG_OPT_SZ))
				enable_shw_crypto = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-ring", MAX_LONG_OPT_SZ)) {
				enable_shw_ring = 1;
				ring_name = optarg;
			} else if (!strncmp(long_option[option_index].name,
					"show-mempool", MAX_LONG_OPT_SZ)) {
				enable_shw_mempool = 1;
				mempool_name = optarg;
			} else if (!strncmp(long_option[option_index].name,
					"iter-mempool", MAX_LONG_OPT_SZ)) {
				enable_iter_mempool = 1;
				mempool_iter_name = optarg;
			} else if (!strncmp(long_option[option_index].name,
					"dump-regs", MAX_LONG_OPT_SZ)) {
				enable_dump_regs = 1;
				dump_regs_file_prefix = optarg;
			} else if (!strncmp(long_option[option_index].name,
					"version", MAX_LONG_OPT_SZ))
				enable_shw_version = 1;
			else if (!strncmp(long_option[option_index].name,
					"firmware-version", MAX_LONG_OPT_SZ))
				enable_shw_fw_version = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-rss-reta", MAX_LONG_OPT_SZ))
				enable_shw_rss_reta = 1;
			else if (!strncmp(long_option[option_index].name,
					"show-module-eeprom", MAX_LONG_OPT_SZ))
				enable_shw_module_eeprom = 1;
			else if (!strncmp(long_option[option_index].name,
					"edev-dump-xstats", MAX_LONG_OPT_SZ)) {
				int ret = parse_eventdev_dump_xstats_params(optarg);
				if (ret < 0) {
					fprintf(stderr, "Error parsing eventdev dump xstats params: %s\n",
						strerror(-ret));
					return -1;
				}
			} else if (!strncmp(long_option[option_index].name,
					"edev-reset-xstats", MAX_LONG_OPT_SZ)) {
				int ret = parse_eventdev_reset_xstats_params(optarg);
				if (ret < 0) {
					fprintf(stderr, "Error parsing eventdev reset xstats params: %s\n",
						strerror(-ret));
					return -1;
				}
			} else if (!strncmp(long_option[option_index].name,
					"show-edev-device-xstats", MAX_LONG_OPT_SZ)) {
				int ret = parse_eventdev_device_xstats_params(optarg);
				if (ret < 0) {
					fprintf(stderr, "Error parsing eventdev reset xstats params: %s\n",
						strerror(-ret));
					return -1;
				}
			} else if (!strncmp(long_option[option_index].name,
					"show-edev-queue-xstats", MAX_LONG_OPT_SZ)) {
				int ret = parse_eventdev_queue_xstats_params(optarg);
				if (ret < 0) {
					fprintf(stderr, "Error parsing eventdev queue xstats params: %s\n",
						strerror(-ret));
					return -1;
				}
			} else if (!strncmp(long_option[option_index].name,
					"show-edev-port-xstats", MAX_LONG_OPT_SZ)) {
				int ret = parse_eventdev_port_xstats_params(optarg);
				if (ret < 0) {
					fprintf(stderr, "Error parsing eventdev port xstats params: %s\n",
						strerror(-ret));
					return -1;
				}
			}
			break;
		case 1:
			/* Print xstat single value given by name*/
			if (!strncmp(long_option[option_index].name,
					"xstats-name", MAX_LONG_OPT_SZ)) {
				enable_xstats_name = 1;
				xstats_name = optarg;
				printf("name:%s:%s\n",
						long_option[option_index].name,
						optarg);
			} else if (!strncmp(long_option[option_index].name,
					"xstats-ids",
					MAX_LONG_OPT_SZ))	{
				int ret = parse_xstats_ids(optarg,
						xstats_ids, MAX_NB_XSTATS_IDS);
				if (ret <= 0) {
					printf("xstats-id list parse error.\n");
					return -1;
				}
				nb_xstats_ids = ret;
			} else if (!strncmp(long_option[option_index].name,
				"show-rx-descriptor", MAX_LONG_OPT_SZ)) {
				int ret = parse_descriptor_param(optarg,
							&rx_desc_param);
				if (ret < 0) {
					fprintf(stderr, "Error parsing Rx descriptor param: %s\n",
						strerror(-ret));
					return -1;
				}
				enable_shw_rx_desc_dump = 1;
			} else if (!strncmp(long_option[option_index].name,
				"show-tx-descriptor", MAX_LONG_OPT_SZ)) {
				int ret = parse_descriptor_param(optarg,
							&tx_desc_param);
				if (ret < 0) {
					fprintf(stderr, "Error parsing Tx descriptor param: %s\n",
						strerror(-ret));
					return -1;
				}
				enable_shw_tx_desc_dump = 1;
			}
			break;
		default:
			proc_info_usage(prgname);
			return -1;
		}
	}
	return 0;
}

static void
meminfo_display(void)
{
	printf("----------- MEMORY_SEGMENTS -----------\n");
	rte_dump_physmem_layout(stdout);
	printf("--------- END_MEMORY_SEGMENTS ---------\n");

	printf("------------ MEMORY_ZONES -------------\n");
	rte_memzone_dump(stdout);
	printf("---------- END_MEMORY_ZONES -----------\n");

	printf("------------- TAIL_QUEUES -------------\n");
	rte_dump_tailq(stdout);
	printf("---------- END_TAIL_QUEUES ------------\n");
}

static void
nic_stats_display(uint16_t port_id)
{
	struct rte_eth_stats stats;
	uint8_t i;

	static const char *nic_stats_border = "########################";

	rte_eth_stats_get(port_id, &stats);
	printf("\n  %s NIC statistics for port %-2d %s\n",
		   nic_stats_border, port_id, nic_stats_border);

	printf("  RX-packets: %-10"PRIu64"  RX-errors:  %-10"PRIu64
	       "  RX-bytes:  %-10"PRIu64"\n", stats.ipackets, stats.ierrors,
	       stats.ibytes);
	printf("  RX-nombuf:  %-10"PRIu64"\n", stats.rx_nombuf);
	printf("  TX-packets: %-10"PRIu64"  TX-errors:  %-10"PRIu64
	       "  TX-bytes:  %-10"PRIu64"\n", stats.opackets, stats.oerrors,
	       stats.obytes);

	printf("\n");
	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
		printf("  Stats reg %2d RX-packets: %-10"PRIu64
		       "  RX-errors: %-10"PRIu64
		       "  RX-bytes: %-10"PRIu64"\n",
		       i, stats.q_ipackets[i], stats.q_errors[i], stats.q_ibytes[i]);
	}

	printf("\n");
	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
		printf("  Stats reg %2d TX-packets: %-10"PRIu64
		       "  TX-bytes: %-10"PRIu64"\n",
		       i, stats.q_opackets[i], stats.q_obytes[i]);
	}

	printf("  %s############################%s\n",
		   nic_stats_border, nic_stats_border);
}

static void
nic_stats_clear(uint16_t port_id)
{
	printf("\n Clearing NIC stats for port %d\n", port_id);
	rte_eth_stats_reset(port_id);
	printf("\n  NIC statistics for port %d cleared\n", port_id);
}

static void collectd_resolve_cnt_type(char *cnt_type, size_t cnt_type_len,
				      const char *cnt_name) {
	char *type_end = strrchr(cnt_name, '_');

	if ((type_end != NULL) &&
	    (strncmp(cnt_name, "rx_", strlen("rx_")) == 0)) {
		if (strncmp(type_end, "_errors", strlen("_errors")) == 0)
			strlcpy(cnt_type, "if_rx_errors", cnt_type_len);
		else if (strncmp(type_end, "_dropped", strlen("_dropped")) == 0)
			strlcpy(cnt_type, "if_rx_dropped", cnt_type_len);
		else if (strncmp(type_end, "_bytes", strlen("_bytes")) == 0)
			strlcpy(cnt_type, "if_rx_octets", cnt_type_len);
		else if (strncmp(type_end, "_packets", strlen("_packets")) == 0)
			strlcpy(cnt_type, "if_rx_packets", cnt_type_len);
		else if (strncmp(type_end, "_placement",
				 strlen("_placement")) == 0)
			strlcpy(cnt_type, "if_rx_errors", cnt_type_len);
		else if (strncmp(type_end, "_buff", strlen("_buff")) == 0)
			strlcpy(cnt_type, "if_rx_errors", cnt_type_len);
		else
			/* Does not fit obvious type: use a more generic one */
			strlcpy(cnt_type, "derive", cnt_type_len);
	} else if ((type_end != NULL) &&
		(strncmp(cnt_name, "tx_", strlen("tx_"))) == 0) {
		if (strncmp(type_end, "_errors", strlen("_errors")) == 0)
			strlcpy(cnt_type, "if_tx_errors", cnt_type_len);
		else if (strncmp(type_end, "_dropped", strlen("_dropped")) == 0)
			strlcpy(cnt_type, "if_tx_dropped", cnt_type_len);
		else if (strncmp(type_end, "_bytes", strlen("_bytes")) == 0)
			strlcpy(cnt_type, "if_tx_octets", cnt_type_len);
		else if (strncmp(type_end, "_packets", strlen("_packets")) == 0)
			strlcpy(cnt_type, "if_tx_packets", cnt_type_len);
		else
			/* Does not fit obvious type: use a more generic one */
			strlcpy(cnt_type, "derive", cnt_type_len);
	} else if ((type_end != NULL) &&
		   (strncmp(cnt_name, "flow_", strlen("flow_"))) == 0) {
		if (strncmp(type_end, "_filters", strlen("_filters")) == 0)
			strlcpy(cnt_type, "filter_result", cnt_type_len);
		else if (strncmp(type_end, "_errors", strlen("_errors")) == 0)
			strlcpy(cnt_type, "errors", cnt_type_len);
	} else if ((type_end != NULL) &&
		   (strncmp(cnt_name, "mac_", strlen("mac_"))) == 0) {
		if (strncmp(type_end, "_errors", strlen("_errors")) == 0)
			strlcpy(cnt_type, "errors", cnt_type_len);
	} else {
		/* Does not fit obvious type, or strrchr error: */
		/* use a more generic type */
		strlcpy(cnt_type, "derive", cnt_type_len);
	}
}

static void
nic_xstats_by_name_display(uint16_t port_id, char *name)
{
	uint64_t id;

	printf("###### NIC statistics for port %-2d, statistic name '%s':\n",
			   port_id, name);

	if (rte_eth_xstats_get_id_by_name(port_id, name, &id) == 0)
		printf("%s: %"PRIu64"\n", name, id);
	else
		printf("Statistic not found...\n");

}

static void
nic_xstats_by_ids_display(uint16_t port_id, uint64_t *ids, int len)
{
	struct rte_eth_xstat_name *xstats_names;
	uint64_t *values;
	int ret, i;
	static const char *nic_stats_border = "########################";

	values = malloc(sizeof(*values) * len);
	if (values == NULL) {
		printf("Cannot allocate memory for xstats\n");
		return;
	}

	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * len);
	if (xstats_names == NULL) {
		printf("Cannot allocate memory for xstat names\n");
		free(values);
		return;
	}

	if (len != rte_eth_xstats_get_names_by_id(
			port_id, xstats_names, len, ids)) {
		printf("Cannot get xstat names\n");
		goto err;
	}

	printf("###### NIC extended statistics for port %-2d #########\n",
			   port_id);
	printf("%s############################\n", nic_stats_border);
	ret = rte_eth_xstats_get_by_id(port_id, ids, values, len);
	if (ret < 0 || ret > len) {
		printf("Cannot get xstats\n");
		goto err;
	}

	for (i = 0; i < len; i++)
		printf("%s: %"PRIu64"\n",
			xstats_names[i].name,
			values[i]);

	printf("%s############################\n", nic_stats_border);
err:
	free(values);
	free(xstats_names);
}

static void
nic_xstats_display(uint16_t port_id)
{
	struct rte_eth_xstat_name *xstats_names;
	uint64_t *values;
	int len, ret, i;
	static const char *nic_stats_border = "########################";

	len = rte_eth_xstats_get_names_by_id(port_id, NULL, 0, NULL);
	if (len < 0) {
		printf("Cannot get xstats count\n");
		return;
	}
	values = malloc(sizeof(*values) * len);
	if (values == NULL) {
		printf("Cannot allocate memory for xstats\n");
		return;
	}

	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * len);
	if (xstats_names == NULL) {
		printf("Cannot allocate memory for xstat names\n");
		free(values);
		return;
	}
	if (len != rte_eth_xstats_get_names_by_id(
			port_id, xstats_names, len, NULL)) {
		printf("Cannot get xstat names\n");
		goto err;
	}

	printf("###### NIC extended statistics for port %-2d #########\n",
			   port_id);
	printf("%s############################\n",
			   nic_stats_border);
	ret = rte_eth_xstats_get_by_id(port_id, NULL, values, len);
	if (ret < 0 || ret > len) {
		printf("Cannot get xstats\n");
		goto err;
	}

	for (i = 0; i < len; i++) {
		if (enable_collectd_format) {
			char counter_type[MAX_STRING_LEN];
			char buf[MAX_STRING_LEN];
			size_t n;

			collectd_resolve_cnt_type(counter_type,
						  sizeof(counter_type),
						  xstats_names[i].name);
			n = snprintf(buf, MAX_STRING_LEN,
				"PUTVAL %s/dpdkstat-port.%u/%s-%s N:%"
				PRIu64"\n", host_id, port_id, counter_type,
				xstats_names[i].name, values[i]);
			if (n > sizeof(buf) - 1)
				n = sizeof(buf) - 1;
			ret = write(stdout_fd, buf, n);
			if (ret < 0)
				goto err;
		} else {
			printf("%s: %"PRIu64"\n", xstats_names[i].name,
					values[i]);
		}
	}

	printf("%s############################\n",
			   nic_stats_border);
err:
	free(values);
	free(xstats_names);
}

static void
nic_xstats_clear(uint16_t port_id)
{
	int ret;

	printf("\n Clearing NIC xstats for port %d\n", port_id);
	ret = rte_eth_xstats_reset(port_id);
	if (ret != 0) {
		printf("\n Error clearing xstats for port %d: %s\n", port_id,
		       strerror(-ret));
		return;
	}

	printf("\n  NIC extended statistics for port %d cleared\n", port_id);
}

#ifdef RTE_LIB_METRICS
static void
metrics_display(int port_id)
{
	struct rte_metric_value *metrics;
	struct rte_metric_name *names;
	int len, ret;
	static const char *nic_stats_border = "########################";

	len = rte_metrics_get_names(NULL, 0);
	if (len < 0) {
		printf("Cannot get metrics count\n");
		return;
	}
	if (len == 0) {
		printf("No metrics to display (none have been registered)\n");
		return;
	}

	metrics = malloc(sizeof(struct rte_metric_value) * len);
	if (metrics == NULL) {
		printf("Cannot allocate memory for metrics\n");
		return;
	}

	names = malloc(sizeof(struct rte_metric_name) * len);
	if (names == NULL) {
		printf("Cannot allocate memory for metrics names\n");
		free(metrics);
		return;
	}

	if (len != rte_metrics_get_names(names, len)) {
		printf("Cannot get metrics names\n");
		free(metrics);
		free(names);
		return;
	}

	if (port_id == RTE_METRICS_GLOBAL)
		printf("###### Non port specific metrics  #########\n");
	else
		printf("###### metrics for port %-2d #########\n", port_id);
	printf("%s############################\n", nic_stats_border);
	ret = rte_metrics_get_values(port_id, metrics, len);
	if (ret < 0 || ret > len) {
		printf("Cannot get metrics values\n");
		free(metrics);
		free(names);
		return;
	}

	int i;
	for (i = 0; i < len; i++)
		printf("%s: %"PRIu64"\n", names[i].name, metrics[i].value);

	printf("%s############################\n", nic_stats_border);
	free(metrics);
	free(names);
}
#endif

static void
show_security_context(uint16_t portid, bool inline_offload)
{
	void *p_ctx;
	const struct rte_security_capability *s_cap;

	if (inline_offload)
		p_ctx = rte_eth_dev_get_sec_ctx(portid);
	else
		p_ctx = rte_cryptodev_get_sec_ctx(portid);

	if (p_ctx == NULL)
		return;

	printf("  - crypto context\n");
	printf("\t  -- security context - %p\n", p_ctx);
	printf("\t  -- size %u\n",
	       rte_security_session_get_size(p_ctx));

	s_cap = rte_security_capabilities_get(p_ctx);
	if (s_cap) {
		printf("\t  -- action (0x%x), protocol (0x%x),"
		       " offload flags (0x%x)\n",
		       s_cap->action,
		       s_cap->protocol,
		       s_cap->ol_flags);
		printf("\t  -- capabilities - oper type %x\n",
		       s_cap->crypto_capabilities->op);
	}
}

static void
show_offloads(uint64_t offloads,
	      const char *(show_offload)(uint64_t))
{
	printf(" offloads :");
	while (offloads != 0) {
		uint64_t offload_flag = 1ULL << rte_ctz64(offloads);
		printf(" %s", show_offload(offload_flag));
		offloads &= ~offload_flag;
	}
}

static void
show_port(void)
{
	int i, ret, j, k;

	snprintf(bdr_str, MAX_STRING_LEN, " show - Port PMD ");
	STATS_BDR_STR(10, bdr_str);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		uint16_t mtu = 0;
		struct rte_eth_link link;
		struct rte_eth_dev_info dev_info;
		struct rte_eth_rss_conf rss_conf;
		char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];
		struct rte_eth_fc_conf fc_conf;
		struct rte_ether_addr mac;
		struct rte_eth_dev_owner owner;

		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << i)) == 0)
			continue;

		/* Skip if port is unused */
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		memset(&rss_conf, 0, sizeof(rss_conf));

		snprintf(bdr_str, MAX_STRING_LEN, " Port %u ", i);
		STATS_BDR_STR(5, bdr_str);
		printf("  - generic config\n");

		ret = rte_eth_dev_info_get(i, &dev_info);
		if (ret != 0) {
			printf("Error during getting device info: %s\n",
				strerror(-ret));
			return;
		}

		printf("\t  -- driver %s device %s socket %d\n",
		       dev_info.driver_name, rte_dev_name(dev_info.device),
		       rte_eth_dev_socket_id(i));

		ret = rte_eth_dev_owner_get(i, &owner);
		if (ret == 0 && owner.id != RTE_ETH_DEV_NO_OWNER)
			printf("\t --  owner %#"PRIx64":%s\n",
			       owner.id, owner.name);

		ret = rte_eth_link_get(i, &link);
		if (ret < 0) {
			printf("Link get failed (port %u): %s\n",
			       i, rte_strerror(-ret));
		} else {
			rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text),
					&link);
			printf("\t%s\n", link_status_text);
		}

		ret = rte_eth_dev_flow_ctrl_get(i, &fc_conf);
		if (ret == 0 && fc_conf.mode != RTE_ETH_FC_NONE)  {
			printf("\t  -- flow control mode %s%s high %u low %u pause %u%s%s\n",
			       fc_conf.mode == RTE_ETH_FC_RX_PAUSE ? "rx " :
			       fc_conf.mode == RTE_ETH_FC_TX_PAUSE ? "tx " :
			       fc_conf.mode == RTE_ETH_FC_FULL ? "full" : "???",
			       fc_conf.autoneg ? " auto" : "",
			       fc_conf.high_water,
			       fc_conf.low_water,
			       fc_conf.pause_time,
			       fc_conf.send_xon ? " xon" : "",
			       fc_conf.mac_ctrl_frame_fwd ? " mac_ctrl" : "");
		}

		ret = rte_eth_macaddr_get(i, &mac);
		if (ret == 0) {
			char ebuf[RTE_ETHER_ADDR_FMT_SIZE];

			rte_ether_format_addr(ebuf, sizeof(ebuf), &mac);
			printf("\t  -- mac %s\n", ebuf);
		}

		ret = rte_eth_promiscuous_get(i);
		if (ret >= 0)
			printf("\t  -- promiscuous mode %s\n",
			       ret > 0 ? "enabled" : "disabled");

		ret = rte_eth_allmulticast_get(i);
		if (ret >= 0)
			printf("\t  -- all multicast mode %s\n",
			       ret > 0 ? "enabled" : "disabled");

		ret = rte_eth_dev_get_mtu(i, &mtu);
		if (ret == 0)
			printf("\t  -- mtu (%d)\n", mtu);

		for (j = 0; j < dev_info.nb_rx_queues; j++) {
			struct rte_eth_rxq_info queue_info;
			struct rte_eth_burst_mode mode;
			int count;

			ret = rte_eth_rx_queue_info_get(i, j, &queue_info);
			if (ret != 0)
				break;

			if (j == 0)
				printf("  - rx queue\n");

			printf("\t  -- %d descriptors ", j);
			count = rte_eth_rx_queue_count(i, j);
			if (count >= 0)
				printf("%d/", count);
			printf("%u", queue_info.nb_desc);

			if (queue_info.scattered_rx)
				printf(" scattered");

			if (queue_info.conf.rx_drop_en)
				printf(" drop_en");

			if (queue_info.conf.rx_deferred_start)
				printf(" deferred_start");

			if (queue_info.rx_buf_size != 0)
				printf(" rx buffer size %u",
				       queue_info.rx_buf_size);

			printf(" mempool %s socket %d",
			       queue_info.mp->name,
			       queue_info.mp->socket_id);

			if (queue_info.conf.offloads != 0)
				show_offloads(queue_info.conf.offloads, rte_eth_dev_rx_offload_name);

			if (rte_eth_rx_burst_mode_get(i, j, &mode) == 0)
				printf(" burst mode : %s%s",
				       mode.info,
				       mode.flags & RTE_ETH_BURST_FLAG_PER_QUEUE ?
						" (per queue)" : "");

			printf("\n");
		}

		for (j = 0; j < dev_info.nb_tx_queues; j++) {
			struct rte_eth_txq_info queue_info;
			struct rte_eth_burst_mode mode;

			ret = rte_eth_tx_queue_info_get(i, j, &queue_info);
			if (ret != 0)
				break;

			if (j == 0)
				printf("  - tx queue\n");

			printf("\t  -- %d descriptors %d",
			       j, queue_info.nb_desc);

			printf(" thresh %u/%u",
			       queue_info.conf.tx_rs_thresh,
			       queue_info.conf.tx_free_thresh);

			if (queue_info.conf.tx_deferred_start)
				printf(" deferred_start");

			if (queue_info.conf.offloads != 0)
				show_offloads(queue_info.conf.offloads, rte_eth_dev_tx_offload_name);

			if (rte_eth_tx_burst_mode_get(i, j, &mode) == 0)
				printf(" burst mode : %s%s",
				       mode.info,
				       mode.flags & RTE_ETH_BURST_FLAG_PER_QUEUE ?
						" (per queue)" : "");

			printf("\n");
		}

		ret = rte_eth_dev_rss_hash_conf_get(i, &rss_conf);
		if (ret == 0) {
			if (rss_conf.rss_key) {
				printf("  - RSS\n");
				printf("\t  -- RSS len %u key (hex):",
						rss_conf.rss_key_len);
				for (k = 0; k < rss_conf.rss_key_len; k++)
					printf(" %x", rss_conf.rss_key[k]);
				printf("\t  -- hf 0x%"PRIx64"\n",
						rss_conf.rss_hf);
			}
		}

#ifdef RTE_LIB_SECURITY
		show_security_context(i, true);
#endif
	}
}

static void
show_port_private_info(void)
{
	int i;

	snprintf(bdr_str, MAX_STRING_LEN, " Dump - Ports private information");
	STATS_BDR_STR(10, bdr_str);

	RTE_ETH_FOREACH_DEV(i) {
		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << i)) == 0)
			continue;

		snprintf(bdr_str, MAX_STRING_LEN, " Port %u ", i);
		STATS_BDR_STR(5, bdr_str);
		rte_eth_dev_priv_dump(i, stdout);
	}
}

static void
display_nodecap_info(int is_leaf, struct rte_tm_node_capabilities *cap)
{
	if (cap == NULL)
		return;

	if (!is_leaf) {
		printf("\t  -- nonleaf sched max:\n"
			"\t\t  + children (%u)\n"
			"\t\t  + sp priorities (%u)\n"
			"\t\t  + wfq children per group (%u)\n"
			"\t\t  + wfq groups (%u)\n"
			"\t\t  + wfq weight (%u)\n",
			cap->nonleaf.sched_n_children_max,
			cap->nonleaf.sched_sp_n_priorities_max,
			cap->nonleaf.sched_wfq_n_children_per_group_max,
			cap->nonleaf.sched_wfq_n_groups_max,
			cap->nonleaf.sched_wfq_weight_max);
	} else {
		printf("\t  -- leaf cman support:\n"
			"\t\t  + wred pkt mode (%d)\n"
			"\t\t  + wred byte mode (%d)\n"
			"\t\t  + head drop (%d)\n"
			"\t\t  + wred context private (%d)\n"
			"\t\t  + wred context shared (%u)\n",
			cap->leaf.cman_wred_packet_mode_supported,
			cap->leaf.cman_wred_byte_mode_supported,
			cap->leaf.cman_head_drop_supported,
			cap->leaf.cman_wred_context_private_supported,
			cap->leaf.cman_wred_context_shared_n_max);
	}
}

static void
display_levelcap_info(int is_leaf, struct rte_tm_level_capabilities *cap)
{
	if (cap == NULL)
		return;

	if (!is_leaf) {
		printf("\t  -- shaper private: (%d) dual rate (%d)\n",
			cap->nonleaf.shaper_private_supported,
			cap->nonleaf.shaper_private_dual_rate_supported);
		printf("\t  -- shaper share: (%u)\n",
			cap->nonleaf.shaper_shared_n_max);
		printf("\t  -- non leaf sched MAX:\n"
			"\t\t  + children (%u)\n"
			"\t\t  + sp (%u)\n"
			"\t\t  + wfq children per group (%u)\n"
			"\t\t  + wfq groups (%u)\n"
			"\t\t  + wfq weight (%u)\n",
			cap->nonleaf.sched_n_children_max,
			cap->nonleaf.sched_sp_n_priorities_max,
			cap->nonleaf.sched_wfq_n_children_per_group_max,
			cap->nonleaf.sched_wfq_n_groups_max,
			cap->nonleaf.sched_wfq_weight_max);
	} else {
		printf("\t  -- shaper private: (%d) dual rate (%d)\n",
			cap->leaf.shaper_private_supported,
			cap->leaf.shaper_private_dual_rate_supported);
		printf("\t  -- shaper share: (%u)\n",
			cap->leaf.shaper_shared_n_max);
		printf("  -- leaf cman support:\n"
			"\t\t  + wred pkt mode (%d)\n"
			"\t\t  + wred byte mode (%d)\n"
			"\t\t  + head drop (%d)\n"
			"\t\t  + wred context private (%d)\n"
			"\t\t  + wred context shared (%u)\n",
			cap->leaf.cman_wred_packet_mode_supported,
			cap->leaf.cman_wred_byte_mode_supported,
			cap->leaf.cman_head_drop_supported,
			cap->leaf.cman_wred_context_private_supported,
			cap->leaf.cman_wred_context_shared_n_max);
	}
}

static void
show_tm(void)
{
	int ret = 0, check_for_leaf = 0, is_leaf = 0;
	unsigned int j, k;
	uint16_t i = 0;

	snprintf(bdr_str, MAX_STRING_LEN, " show - TM PMD ");
	STATS_BDR_STR(10, bdr_str);

	RTE_ETH_FOREACH_DEV(i) {
		struct rte_eth_dev_info dev_info;
		struct rte_tm_capabilities cap;
		struct rte_tm_error error;
		struct rte_tm_node_capabilities capnode;
		struct rte_tm_level_capabilities caplevel;
		uint32_t n_leaf_nodes = 0;

		memset(&cap, 0, sizeof(cap));
		memset(&error, 0, sizeof(error));

		ret = rte_eth_dev_info_get(i, &dev_info);
		if (ret != 0) {
			printf("Error during getting device (port %u) info: %s\n",
				i, strerror(-ret));
			return;
		}

		printf("  - Generic for port (%u)\n"
			"\t  -- driver name %s\n"
			"\t  -- max vf (%u)\n"
			"\t  -- max tx queues (%u)\n"
			"\t  -- number of tx queues (%u)\n",
			i,
			dev_info.driver_name,
			dev_info.max_vfs,
			dev_info.max_tx_queues,
			dev_info.nb_tx_queues);

		ret = rte_tm_capabilities_get(i, &cap, &error);
		if (ret)
			continue;

		printf("  - MAX: nodes (%u) levels (%u) children (%u)\n",
			cap.n_nodes_max,
			cap.n_levels_max,
			cap.sched_n_children_max);

		printf("  - identical nodes: non leaf (%d) leaf (%d)\n",
			cap.non_leaf_nodes_identical,
			cap.leaf_nodes_identical);

		printf("  - Shaper MAX:\n"
			"\t  -- total (%u)\n"
			"\t  -- private (%u) private dual (%d)\n"
			"\t  -- shared (%u) shared dual (%u)\n",
			cap.shaper_n_max,
			cap.shaper_private_n_max,
			cap.shaper_private_dual_rate_n_max,
			cap.shaper_shared_n_max,
			cap.shaper_shared_dual_rate_n_max);

		printf("  - mark support:\n");
		printf("\t  -- vlan dei: GREEN (%d) YELLOW (%d) RED (%d)\n",
			cap.mark_vlan_dei_supported[RTE_COLOR_GREEN],
			cap.mark_vlan_dei_supported[RTE_COLOR_YELLOW],
			cap.mark_vlan_dei_supported[RTE_COLOR_RED]);
		printf("\t  -- ip ecn tcp: GREEN (%d) YELLOW (%d) RED (%d)\n",
			cap.mark_ip_ecn_tcp_supported[RTE_COLOR_GREEN],
			cap.mark_ip_ecn_tcp_supported[RTE_COLOR_YELLOW],
			cap.mark_ip_ecn_tcp_supported[RTE_COLOR_RED]);
		printf("\t  -- ip ecn sctp: GREEN (%d) YELLOW (%d) RED (%d)\n",
			cap.mark_ip_ecn_sctp_supported[RTE_COLOR_GREEN],
			cap.mark_ip_ecn_sctp_supported[RTE_COLOR_YELLOW],
			cap.mark_ip_ecn_sctp_supported[RTE_COLOR_RED]);
		printf("\t  -- ip dscp: GREEN (%d) YELLOW (%d) RED (%d)\n",
			cap.mark_ip_dscp_supported[RTE_COLOR_GREEN],
			cap.mark_ip_dscp_supported[RTE_COLOR_YELLOW],
			cap.mark_ip_dscp_supported[RTE_COLOR_RED]);

		printf("  - mask stats (0x%"PRIx64")"
			" dynamic update (0x%"PRIx64")\n",
			cap.stats_mask,
			cap.dynamic_update_mask);

		printf("  - sched MAX:\n"
			"\t  -- total (%u)\n"
			"\t  -- sp levels (%u)\n"
			"\t  -- wfq children per group (%u)\n"
			"\t  -- wfq groups (%u)\n"
			"\t  -- wfq weight (%u)\n",
			cap.sched_sp_n_priorities_max,
			cap.sched_sp_n_priorities_max,
			cap.sched_wfq_n_children_per_group_max,
			cap.sched_wfq_n_groups_max,
			cap.sched_wfq_weight_max);

		printf("  - CMAN support:\n"
			"\t  -- WRED mode: pkt (%d) byte (%d)\n"
			"\t  -- head drop (%d)\n",
			cap.cman_wred_packet_mode_supported,
			cap.cman_wred_byte_mode_supported,
			cap.cman_head_drop_supported);
		printf("\t  -- MAX WRED CONTEXT:"
			" total (%u) private (%u) shared (%u)\n",
			cap.cman_wred_context_n_max,
			cap.cman_wred_context_private_n_max,
			cap.cman_wred_context_shared_n_max);

		for (j = 0; j < cap.n_nodes_max; j++) {
			memset(&capnode, 0, sizeof(capnode));
			ret = rte_tm_node_capabilities_get(i, j,
					&capnode, &error);
			if (ret)
				continue;

			check_for_leaf = 1;

			printf("  NODE %u\n", j);
			printf("\t  - shaper private: (%d) dual rate (%d)\n",
				capnode.shaper_private_supported,
				capnode.shaper_private_dual_rate_supported);
			printf("\t  - shaper shared max: (%u)\n",
				capnode.shaper_shared_n_max);
			printf("\t  - stats mask %"PRIx64"\n",
				capnode.stats_mask);

			ret = rte_tm_node_type_get(i, j, &is_leaf, &error);
			if (ret)
				continue;

			display_nodecap_info(is_leaf, &capnode);
		}

		for (j = 0; j < cap.n_levels_max; j++) {
			memset(&caplevel, 0, sizeof(caplevel));
			ret = rte_tm_level_capabilities_get(i, j,
					&caplevel, &error);
			if (ret)
				continue;

			printf("  - Level %u\n", j);
			printf("\t  -- node MAX: %u non leaf %u leaf %u\n",
				caplevel.n_nodes_max,
				caplevel.n_nodes_nonleaf_max,
				caplevel.n_nodes_leaf_max);
			printf("\t  -- identical: non leaf %u leaf %u\n",
				caplevel.non_leaf_nodes_identical,
				caplevel.leaf_nodes_identical);

			for (k = 0; k < caplevel.n_nodes_max; k++) {
				ret = rte_tm_node_type_get(i, k,
					&is_leaf, &error);
				if (ret)
					continue;

				display_levelcap_info(is_leaf, &caplevel);
			}
		}

		if (check_for_leaf) {
			ret = rte_tm_get_number_of_leaf_nodes(i,
					&n_leaf_nodes, &error);
			if (ret == 0)
				printf("  - leaf nodes (%u)\n", n_leaf_nodes);
		}

		for (j = 0; j < n_leaf_nodes; j++) {
			struct rte_tm_node_stats stats;
			memset(&stats, 0, sizeof(stats));

			ret = rte_tm_node_stats_read(i, j,
					&stats, &cap.stats_mask, 0, &error);
			if (ret)
				continue;

			printf("  - STATS for node (%u)\n", j);
			printf("  -- pkts (%"PRIu64") bytes (%"PRIu64")\n",
				stats.n_pkts, stats.n_bytes);

			ret = rte_tm_node_type_get(i, j, &is_leaf, &error);
			if (ret || (!is_leaf))
				continue;

			printf("  -- leaf queued:"
				" pkts (%"PRIu64") bytes (%"PRIu64")\n",
				stats.leaf.n_pkts_queued,
				stats.leaf.n_bytes_queued);
			printf("  - dropped:\n"
				"\t  -- GREEN:"
				" pkts (%"PRIu64") bytes (%"PRIu64")\n"
				"\t  -- YELLOW:"
				" pkts (%"PRIu64") bytes (%"PRIu64")\n"
				"\t  -- RED:"
				" pkts (%"PRIu64") bytes (%"PRIu64")\n",
				stats.leaf.n_pkts_dropped[RTE_COLOR_GREEN],
				stats.leaf.n_bytes_dropped[RTE_COLOR_GREEN],
				stats.leaf.n_pkts_dropped[RTE_COLOR_YELLOW],
				stats.leaf.n_bytes_dropped[RTE_COLOR_YELLOW],
				stats.leaf.n_pkts_dropped[RTE_COLOR_RED],
				stats.leaf.n_bytes_dropped[RTE_COLOR_RED]);
		}
	}
}

static void
display_crypto_feature_info(uint64_t x)
{
	if (x == 0)
		return;

	printf("\t  -- feature flags\n");
	printf("\t\t  + symmetric (%c), asymmetric (%c)\n"
		"\t\t  + symmetric operation chaining (%c)\n",
		(x & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING) ? 'y' : 'n');
	printf("\t\t  + CPU: SSE (%c), AVX (%c), AVX2 (%c), AVX512 (%c)\n",
		(x & RTE_CRYPTODEV_FF_CPU_SSE) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_CPU_AVX) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_CPU_AVX2) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_CPU_AVX512) ? 'y' : 'n');
	printf("\t\t  + AESNI: CPU (%c), HW (%c)\n",
		(x & RTE_CRYPTODEV_FF_CPU_AESNI) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_HW_ACCELERATED) ? 'y' : 'n');
	printf("\t\t  + SECURITY OFFLOAD (%c)\n",
		(x & RTE_CRYPTODEV_FF_SECURITY) ? 'y' : 'n');
	printf("\t\t  + ARM: NEON (%c), CE (%c)\n",
		(x & RTE_CRYPTODEV_FF_CPU_NEON) ? 'y' : 'n',
		(x & RTE_CRYPTODEV_FF_CPU_ARM_CE) ? 'y' : 'n');
	printf("\t  -- buffer offload\n");
	printf("\t\t  + IN_PLACE_SGL (%c)\n",
		(x & RTE_CRYPTODEV_FF_IN_PLACE_SGL) ? 'y' : 'n');
	printf("\t\t  + OOP_SGL_IN_SGL_OUT (%c)\n",
		(x & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT) ? 'y' : 'n');
	printf("\t\t  + OOP_SGL_IN_LB_OUT (%c)\n",
		(x & RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT) ? 'y' : 'n');
	printf("\t\t  + OOP_LB_IN_SGL_OUT (%c)\n",
		(x & RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT) ? 'y' : 'n');
	printf("\t\t  + OOP_LB_IN_LB_OUT (%c)\n",
		(x & RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT) ? 'y' : 'n');
}

static void
show_crypto(void)
{
	uint8_t crypto_dev_count = rte_cryptodev_count(), i;

	snprintf(bdr_str, MAX_STRING_LEN, " show - CRYPTO PMD ");
	STATS_BDR_STR(10, bdr_str);

	for (i = 0; i < crypto_dev_count; i++) {
		struct rte_cryptodev_info dev_info;
		struct rte_cryptodev_stats stats;

		rte_cryptodev_info_get(i, &dev_info);

		printf("  - device (%u)\n", i);
		printf("\t  -- name (%s)\n"
		       "\t  -- driver (%s)\n"
		       "\t  -- id (%u) on socket (%d)\n"
		       "\t  -- queue pairs (%d)\n",
		       rte_cryptodev_name_get(i),
		       dev_info.driver_name,
		       dev_info.driver_id,
		       rte_dev_numa_node(dev_info.device),
		       rte_cryptodev_queue_pair_count(i));

		display_crypto_feature_info(dev_info.feature_flags);

		if (rte_cryptodev_stats_get(i, &stats) == 0) {
			printf("\t  -- stats\n");
			printf("\t\t  + enqueue count (%"PRIu64")"
			       " error (%"PRIu64")\n",
			       stats.enqueued_count,
			       stats.enqueue_err_count);
			printf("\t\t  + dequeue count (%"PRIu64")"
			       " error (%"PRIu64")\n",
			       stats.dequeued_count,
			       stats.dequeue_err_count);
		}

#ifdef RTE_LIB_SECURITY
		show_security_context(i, false);
#endif
	}
}

static void
show_ring(char *name)
{
	snprintf(bdr_str, MAX_STRING_LEN, " show - RING ");
	STATS_BDR_STR(10, bdr_str);

	if (name != NULL) {
		struct rte_ring *ptr = rte_ring_lookup(name);
		if (ptr != NULL) {
			printf("  - Name (%s) on socket (%d)\n"
				"  - flags:\n"
				"\t  -- Single Producer Enqueue (%u)\n"
				"\t  -- Single Consumer Dequeue (%u)\n",
				ptr->name,
				ptr->memzone->socket_id,
				ptr->flags & RING_F_SP_ENQ,
				ptr->flags & RING_F_SC_DEQ);
			printf("  - size (%u) mask (0x%x) capacity (%u)\n",
				ptr->size,
				ptr->mask,
				ptr->capacity);
			printf("  - count (%u) free count (%u)\n",
				rte_ring_count(ptr),
				rte_ring_free_count(ptr));
			printf("  - full (%d) empty (%d)\n",
				rte_ring_full(ptr),
				rte_ring_empty(ptr));

			STATS_BDR_STR(50, "");
			return;
		}
	}

	rte_ring_list_dump(stdout);
}

static void
show_mempool(char *name)
{
	snprintf(bdr_str, MAX_STRING_LEN, " show - MEMPOOL ");
	STATS_BDR_STR(10, bdr_str);

	if (name != NULL) {
		struct rte_mempool *ptr = rte_mempool_lookup(name);
		if (ptr != NULL) {
			struct rte_mempool_ops *ops;
			uint64_t flags = ptr->flags;

			ops = rte_mempool_get_ops(ptr->ops_index);
			printf("  - Name: %s on socket %d\n"
				"  - flags:\n"
				"\t  -- No spread (%c)\n"
				"\t  -- No cache align (%c)\n"
				"\t  -- SP put (%c), SC get (%c)\n"
				"\t  -- Pool created (%c)\n"
				"\t  -- No IOVA config (%c)\n"
				"\t  -- Not used for IO (%c)\n",
				ptr->name,
				ptr->socket_id,
				(flags & RTE_MEMPOOL_F_NO_SPREAD) ? 'y' : 'n',
				(flags & RTE_MEMPOOL_F_NO_CACHE_ALIGN) ? 'y' : 'n',
				(flags & RTE_MEMPOOL_F_SP_PUT) ? 'y' : 'n',
				(flags & RTE_MEMPOOL_F_SC_GET) ? 'y' : 'n',
				(flags & RTE_MEMPOOL_F_POOL_CREATED) ? 'y' : 'n',
				(flags & RTE_MEMPOOL_F_NO_IOVA_CONTIG) ? 'y' : 'n',
				(flags & RTE_MEMPOOL_F_NON_IO) ? 'y' : 'n');
			printf("  - Size %u Cache %u element %u\n"
				"  - header %u trailer %u\n"
				"  - private data size %u\n",
				ptr->size,
				ptr->cache_size,
				ptr->elt_size,
				ptr->header_size,
				ptr->trailer_size,
				ptr->private_data_size);
			printf("  - memezone - socket %d\n",
				ptr->mz->socket_id);
			printf("  - Count: avail (%u), in use (%u)\n",
				rte_mempool_avail_count(ptr),
				rte_mempool_in_use_count(ptr));
			printf("  - ops_index %d ops_name %s\n",
				ptr->ops_index, ops ? ops->name : "NA");

			return;
		}
	}

	rte_mempool_list_dump(stdout);
}

static void
mempool_itr_obj(struct rte_mempool *mp, void *opaque,
		void *obj, unsigned int obj_idx)
{
	printf("  - obj_idx %u opaque %p obj %p\n",
			obj_idx, opaque, obj);

	if (obj)
		rte_hexdump(stdout, " Obj Content",
				obj, (mp->elt_size > 256)?256:mp->elt_size);
}

static void
iter_mempool(char *name)
{
	snprintf(bdr_str, MAX_STRING_LEN, " iter - MEMPOOL ");
	STATS_BDR_STR(10, bdr_str);

	if (name != NULL) {
		struct rte_mempool *ptr = rte_mempool_lookup(name);
		if (ptr != NULL) {
			/* iterate each object */
			uint32_t ret = rte_mempool_obj_iter(ptr,
					mempool_itr_obj, NULL);
			printf("\n  - iterated %u objects\n", ret);
			return;
		}
	}
}

static void
dump_regs(char *file_prefix)
{
#define MAX_FILE_NAME_SZ (MAX_LONG_OPT_SZ + 10)
	char file_name[MAX_FILE_NAME_SZ];
	struct rte_dev_reg_info reg_info;
	struct rte_eth_dev_info dev_info;
	unsigned char *buf_data;
	size_t buf_size;
	FILE *fp_regs;
	uint16_t i;
	int ret;

	snprintf(bdr_str, MAX_STRING_LEN, " dump - Port REG");
	STATS_BDR_STR(10, bdr_str);

	RTE_ETH_FOREACH_DEV(i) {
		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << i)) == 0)
			continue;

		snprintf(bdr_str, MAX_STRING_LEN, " Port (%u)", i);
		STATS_BDR_STR(5, bdr_str);

		ret = rte_eth_dev_info_get(i, &dev_info);
		if (ret) {
			printf("Error getting device info: %d\n", ret);
			continue;
		}

		memset(&reg_info, 0, sizeof(reg_info));
		ret = rte_eth_dev_get_reg_info(i, &reg_info);
		if (ret) {
			printf("Error getting device reg info: %d\n", ret);
			continue;
		}

		buf_size = reg_info.length * reg_info.width;
		buf_data = malloc(buf_size);
		if (buf_data == NULL) {
			printf("Error allocating %zu bytes buffer\n", buf_size);
			continue;
		}

		reg_info.data = buf_data;
		reg_info.length = 0;
		ret = rte_eth_dev_get_reg_info(i, &reg_info);
		if (ret) {
			printf("Error getting regs from device: %d\n", ret);
			free(buf_data);
			continue;
		}

		snprintf(file_name, MAX_FILE_NAME_SZ, "%s-port%u",
				file_prefix, i);
		fp_regs = fopen(file_name, "wb");
		if (fp_regs == NULL) {
			printf("Error during opening '%s' for writing: %s\n",
					file_name, strerror(errno));
		} else {
			size_t nr_written;

			nr_written = fwrite(buf_data, 1, buf_size, fp_regs);
			if (nr_written != buf_size)
				printf("Error during writing %s: %s\n",
						file_prefix, strerror(errno));
			else
				printf("Device (%s) regs dumped successfully, "
					"driver:%s version:0X%08X\n",
					rte_dev_name(dev_info.device),
					dev_info.driver_name, reg_info.version);

			fclose(fp_regs);
		}

		free(buf_data);
	}
}

static void
show_version(void)
{
	snprintf(bdr_str, MAX_STRING_LEN, " show - DPDK version ");
	STATS_BDR_STR(10, bdr_str);
	printf("DPDK version: %s\n", rte_version());
}

static void
show_firmware_version(void)
{
	char fw_version[ETHDEV_FWVERS_LEN];
	uint16_t i;

	snprintf(bdr_str, MAX_STRING_LEN, " show - firmware version ");
	STATS_BDR_STR(10, bdr_str);

	RTE_ETH_FOREACH_DEV(i) {
		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << i)) == 0)
			continue;

		if (rte_eth_dev_fw_version_get(i, fw_version,
					       ETHDEV_FWVERS_LEN) == 0)
			printf("Ethdev port %u firmware version: %s\n", i,
				fw_version);
		else
			printf("Ethdev port %u firmware version: %s\n", i,
				"not available");
	}
}

static void
show_port_rss_reta_info(void)
{
	struct rte_eth_rss_reta_entry64 reta_conf[RTE_RETA_CONF_GROUP_NUM + 1];
	struct rte_eth_dev_info dev_info;
	uint16_t i, idx, shift;
	uint16_t num;
	uint16_t id;
	int ret;

	RTE_ETH_FOREACH_DEV(id) {
		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << id)) == 0)
			continue;

		snprintf(bdr_str, MAX_STRING_LEN, " Port %u ", id);
		STATS_BDR_STR(5, bdr_str);

		ret = rte_eth_dev_info_get(id, &dev_info);
		if (ret != 0) {
			fprintf(stderr, "Error getting device info: %s\n",
				strerror(-ret));
			return;
		}

		num = DIV_ROUND_UP(dev_info.reta_size, RTE_ETH_RETA_GROUP_SIZE);
		memset(reta_conf, 0, sizeof(reta_conf));
		for (i = 0; i < num; i++)
			reta_conf[i].mask = ~0ULL;

		ret = rte_eth_dev_rss_reta_query(id, reta_conf, dev_info.reta_size);
		if (ret != 0) {
			fprintf(stderr, "Error getting RSS RETA info: %s\n",
				strerror(-ret));
			return;
		}

		for (i = 0; i < dev_info.reta_size; i++) {
			idx = i / RTE_ETH_RETA_GROUP_SIZE;
			shift = i % RTE_ETH_RETA_GROUP_SIZE;
			printf("RSS RETA configuration: hash index=%u, queue=%u\n",
				i, reta_conf[idx].reta[shift]);
		}
	}
}

static void
show_module_eeprom_info(void)
{
	unsigned char bytes_eeprom[EEPROM_DUMP_CHUNKSIZE];
	struct rte_eth_dev_module_info module_info;
	struct rte_dev_eeprom_info eeprom_info;
	uint16_t i;
	int ret;

	RTE_ETH_FOREACH_DEV(i) {
		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << i)) == 0)
			continue;

		snprintf(bdr_str, MAX_STRING_LEN, " Port %u ", i);
		STATS_BDR_STR(5, bdr_str);

		ret = rte_eth_dev_get_module_info(i, &module_info);
		if (ret != 0) {
			fprintf(stderr, "Module EEPROM information read error: %s\n",
				strerror(-ret));
			return;
		}

		eeprom_info.offset = 0;
		eeprom_info.length = module_info.eeprom_len;
		eeprom_info.data = bytes_eeprom;

		ret = rte_eth_dev_get_module_eeprom(i, &eeprom_info);
		if (ret != 0) {
			fprintf(stderr, "Module EEPROM read error: %s\n",
				strerror(-ret));
			return;
		}

		rte_hexdump(stdout, "hexdump", eeprom_info.data,
			    eeprom_info.length);
		printf("Finish -- Port: %u MODULE EEPROM length: %d bytes\n",
		       i, eeprom_info.length);
	}
}

static void
nic_rx_descriptor_display(uint16_t port_id, struct desc_param *desc)
{
	uint16_t queue_id = desc->queue_id;
	uint16_t offset = desc->offset;
	uint16_t num = desc->num;
	int ret;

	snprintf(bdr_str, MAX_STRING_LEN, " show - Rx descriptor ");
	STATS_BDR_STR(10, bdr_str);

	printf("Dump ethdev Rx descriptor for port %u, queue %u, offset %u, num %u\n",
		port_id, queue_id, offset, num);

	ret = rte_eth_rx_descriptor_dump(port_id, queue_id, offset, num,
					 stdout);
	if (ret < 0)
		fprintf(stderr, "Error dumping ethdev Rx descriptor: %s\n",
			strerror(-ret));
}

static void
nic_tx_descriptor_display(uint16_t port_id, struct desc_param *desc)
{
	uint16_t queue_id = desc->queue_id;
	uint16_t offset = desc->offset;
	uint16_t num = desc->num;
	int ret;

	snprintf(bdr_str, MAX_STRING_LEN, " show - Tx descriptor ");
	STATS_BDR_STR(10, bdr_str);

	printf("Dump ethdev Tx descriptor for port %u, queue %u, offset %u, num %u\n",
		port_id, queue_id, offset, num);

	ret = rte_eth_tx_descriptor_dump(port_id, queue_id, offset, num,
					 stdout);
	if (ret < 0)
		fprintf(stderr, "Error dumping ethdev Tx descriptor: %s\n",
			strerror(-ret));
}

static void
xstats_display(uint8_t dev_id,
	  enum rte_event_dev_xstats_mode mode,
	  uint8_t queue_port_id)
{
	int ret;
	struct rte_event_dev_xstats_name *xstats_names;
	uint64_t *ids;
	uint64_t *values;
	int size;
	int i;

	size = rte_event_dev_xstats_names_get(dev_id,
					     mode,
					     queue_port_id,
					     NULL, /* names */
					     NULL, /* ids */
					     0);   /* num */

	if (size < 0)
		rte_panic("rte_event_dev_xstats_names_get err %d\n", size);

	if (size == 0) {
		printf(
		"No stats available for this item, mode=%d, queue_port_id=%d\n",
			mode, queue_port_id);
		return;
	}

	/* Get memory to hold stat names, IDs, and values */
	xstats_names = malloc(sizeof(struct rte_event_dev_xstats_name) * (unsigned int)size);
	ids = malloc(sizeof(unsigned int) * size);

	if (!xstats_names || !ids)
		rte_panic("unable to alloc memory for stats retrieval\n");

	ret = rte_event_dev_xstats_names_get(dev_id, mode, queue_port_id,
					     xstats_names, ids,
					     (unsigned int)size);
	if (ret != size)
		rte_panic("rte_event_dev_xstats_names_get err %d\n", ret);

	values = malloc(sizeof(uint64_t) * size);
	if (!values)
		rte_panic("unable to alloc memory for stats retrieval\n");

	ret = rte_event_dev_xstats_get(dev_id, mode, queue_port_id,
					    ids, values, size);

	if (ret != size)
		rte_panic("rte_event_dev_xstats_get err %d\n", ret);

	for (i = 0; i < size; i++) {
		printf("id %"PRIu64"  %s = %"PRIu64"\n",
			ids[i], &xstats_names[i].name[0], values[i]);
	}

	free(values);
	free(xstats_names);
	free(ids);

}

static void
xstats_reset(uint8_t dev_id,
	  enum rte_event_dev_xstats_mode mode,
	  uint8_t queue_port_id)
{
	int ret;
	struct rte_event_dev_xstats_name *xstats_names;
	uint64_t *ids;
	int size;

	size = rte_event_dev_xstats_names_get(dev_id,
					     mode,
					     queue_port_id,
					     NULL, /* names */
					     NULL, /* ids */
					     0);   /* num */

	if (size < 0)
		rte_panic("rte_event_dev_xstats_names_get err %d\n", size);

	if (size == 0) {
		printf(
		"No stats available for this item, mode=%d, queue_port_id=%d\n",
			mode, queue_port_id);
		return;
	}

	/* Get memory to hold stat names, IDs, and values */
	xstats_names = malloc(sizeof(struct rte_event_dev_xstats_name) * (unsigned int)size);
	ids = malloc(sizeof(unsigned int) * size);

	if (!xstats_names || !ids)
		rte_panic("unable to alloc memory for stats retrieval\n");

	ret = rte_event_dev_xstats_names_get(dev_id, mode, queue_port_id,
					     xstats_names, ids,
					     (unsigned int)size);
	if (ret != size)
		rte_panic("rte_event_dev_xstats_names_get err %d\n", ret);

	rte_event_dev_xstats_reset(dev_id, mode, queue_port_id,
					   ids, size);

	free(xstats_names);
	free(ids);

}

static unsigned int
eventdev_xstats(void)
{
	unsigned int count = 0;
	int i, j;

	for (i = 0; i < rte_event_dev_count(); i++) {

		if (eventdev_var[i].dump_xstats) {
			++count;
			int ret = rte_event_dev_dump(i, stdout);

			if (ret)
				rte_panic("dump failed with err=%d\n", ret);
		}

		if (eventdev_var[i].shw_device_xstats == 1) {
			++count;
			xstats_display(i, RTE_EVENT_DEV_XSTATS_DEVICE, 0);

			if (eventdev_var[i].reset_xstats == 1)
				xstats_reset(i, RTE_EVENT_DEV_XSTATS_DEVICE, 0);
		}

		if (eventdev_var[i].shw_all_ports == 1) {
			++count;
			for (j = 0; j < MAX_PORTS_QUEUES; j++) {
				xstats_display(i, RTE_EVENT_DEV_XSTATS_PORT, j);

				if (eventdev_var[i].reset_xstats == 1)
					xstats_reset(i, RTE_EVENT_DEV_XSTATS_PORT, j);
			}
		} else {
			if (eventdev_var[i].num_ports > 0)
				++count;
			for (j = 0; j < eventdev_var[i].num_ports; j++) {
				xstats_display(i, RTE_EVENT_DEV_XSTATS_PORT,
					eventdev_var[i].ports[j]);

				if (eventdev_var[i].reset_xstats == 1)
					xstats_reset(i, RTE_EVENT_DEV_XSTATS_PORT,
							eventdev_var[i].ports[j]);
			}
		}

		if (eventdev_var[i].shw_all_queues == 1) {
			++count;
			for (j = 0; j < MAX_PORTS_QUEUES; j++) {
				xstats_display(i, RTE_EVENT_DEV_XSTATS_QUEUE, j);

				if (eventdev_var[i].reset_xstats == 1)
					xstats_reset(i, RTE_EVENT_DEV_XSTATS_QUEUE, j);
			}
		} else {
			if (eventdev_var[i].num_queues > 0)
				++count;
			for (j = 0; j < eventdev_var[i].num_queues; j++) {
				xstats_display(i, RTE_EVENT_DEV_XSTATS_QUEUE,
						eventdev_var[i].queues[j]);

				if (eventdev_var[i].reset_xstats == 1)
					xstats_reset(i, RTE_EVENT_DEV_XSTATS_QUEUE,
							eventdev_var[i].queues[j]);
			}
		}
	}

	return count;
}

int
main(int argc, char **argv)
{
	int ret;
	int i;
	char c_flag[] = "-c1";
	char n_flag[] = "-n4";
	char mp_flag[] = "--proc-type=secondary";
	char log_flag[] = "--log-level=6";
	char *argp[argc + 4];
	uint16_t nb_ports;

	/* preparse app arguments */
	ret = proc_info_preparse_args(argc, argv);
	if (ret < 0) {
		printf("Failed to parse arguments\n");
		return -1;
	}

	argp[0] = argv[0];
	argp[1] = c_flag;
	argp[2] = n_flag;
	argp[3] = mp_flag;
	argp[4] = log_flag;

	for (i = 1; i < argc; i++)
		argp[i + 4] = argv[i];

	argc += 4;

	ret = rte_eal_init(argc, argp);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	argc -= ret;
	argv += ret - 4;

	if (!rte_eal_primary_proc_alive(NULL))
		rte_exit(EXIT_FAILURE, "No primary DPDK process is running.\n");

	/* parse app arguments */
	ret = proc_info_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid argument\n");

	if (mem_info) {
		meminfo_display();
		return 0;
	}

	if (eventdev_xstats() > 0)
		return 0;

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* If no port mask was specified, then show all non-owned ports */
	if (enabled_port_mask == 0) {
		RTE_ETH_FOREACH_DEV(i)
			enabled_port_mask |= 1ul << i;
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		/* Skip if port is not in mask */
		if ((enabled_port_mask & (1ul << i)) == 0)
			continue;

		/* Skip if port is unused */
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		if (enable_stats)
			nic_stats_display(i);
		else if (enable_xstats)
			nic_xstats_display(i);
		else if (reset_stats)
			nic_stats_clear(i);
		else if (reset_xstats)
			nic_xstats_clear(i);
		else if (enable_xstats_name)
			nic_xstats_by_name_display(i, xstats_name);
		else if (nb_xstats_ids > 0)
			nic_xstats_by_ids_display(i, xstats_ids,
						  nb_xstats_ids);
#ifdef RTE_LIB_METRICS
		else if (enable_metrics)
			metrics_display(i);
#endif

		if (enable_shw_rx_desc_dump)
			nic_rx_descriptor_display(i, &rx_desc_param);
		if (enable_shw_tx_desc_dump)
			nic_tx_descriptor_display(i, &tx_desc_param);
	}

#ifdef RTE_LIB_METRICS
	/* print port independent stats */
	if (enable_metrics)
		metrics_display(RTE_METRICS_GLOBAL);
#endif

	/* show information for PMD */
	if (enable_shw_port)
		show_port();
	if (enable_shw_port_priv)
		show_port_private_info();
	if (enable_shw_tm)
		show_tm();
	if (enable_shw_crypto)
		show_crypto();
	if (enable_shw_ring)
		show_ring(ring_name);
	if (enable_shw_mempool)
		show_mempool(mempool_name);
	if (enable_iter_mempool)
		iter_mempool(mempool_iter_name);
	if (enable_dump_regs)
		dump_regs(dump_regs_file_prefix);
	if (enable_shw_version)
		show_version();
	if (enable_shw_fw_version)
		show_firmware_version();
	if (enable_shw_rss_reta)
		show_port_rss_reta_info();
	if (enable_shw_module_eeprom)
		show_module_eeprom_info();

	RTE_ETH_FOREACH_DEV(i)
		rte_eth_dev_close(i);

	ret = rte_eal_cleanup();
	if (ret)
		printf("Error from rte_eal_cleanup(), %d\n", ret);

	return 0;
}
