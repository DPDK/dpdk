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
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_string_fns.h>
#include <rte_metrics.h>
#include <rte_cycles.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security.h>
#endif
#include <rte_cryptodev.h>
#include <rte_tm.h>
#include <rte_hexdump.h>

/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64
#define MAX_STRING_LEN 256

#define STATS_BDR_FMT "========================================"
#define STATS_BDR_STR(w, s) printf("%.*s%s%.*s\n", w, \
	STATS_BDR_FMT, s, w, STATS_BDR_FMT)

/**< mask of enabled ports */
static unsigned long enabled_port_mask;
/**< Enable stats. */
static uint32_t enable_stats;
/**< Enable xstats. */
static uint32_t enable_xstats;
/**< Enable collectd format*/
static uint32_t enable_collectd_format;
/**< FD to send collectd format messages to STDOUT*/
static int stdout_fd;
/**< Host id process is running on */
static char host_id[MAX_LONG_OPT_SZ];
/**< Enable metrics. */
static uint32_t enable_metrics;
/**< Enable stats reset. */
static uint32_t reset_stats;
/**< Enable xstats reset. */
static uint32_t reset_xstats;
/**< Enable memory info. */
static uint32_t mem_info;
/**< Enable displaying xstat name. */
static uint32_t enable_xstats_name;
static char *xstats_name;

/**< Enable xstats by ids. */
#define MAX_NB_XSTATS_IDS 1024
static uint32_t nb_xstats_ids;
static uint64_t xstats_ids[MAX_NB_XSTATS_IDS];

/* show border */
static char bdr_str[MAX_STRING_LEN];

/**< Enable show port. */
static uint32_t enable_shw_port;
/**< Enable show tm. */
static uint32_t enable_shw_tm;
/**< Enable show crypto. */
static uint32_t enable_shw_crypto;
/**< Enable show ring. */
static uint32_t enable_shw_ring;
static char *ring_name;
/**< Enable show mempool. */
static uint32_t enable_shw_mempool;
static char *mempool_name;
/**< Enable iter mempool. */
static uint32_t enable_iter_mempool;
static char *mempool_iter_name;

/**< display usage */
static void
proc_info_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK\n"
		"  -m to display DPDK memory zones, segments and TAILQ information\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to retrieve stats for\n"
		"  --stats: to display port statistics, enabled by default\n"
		"  --xstats: to display extended port statistics, disabled by "
			"default\n"
		"  --metrics: to display derived metrics of the ports, disabled by "
			"default\n"
		"  --xstats-name NAME: to display single xstat id by NAME\n"
		"  --xstats-ids IDLIST: to display xstat values by id. "
			"The argument is comma-separated list of xstat ids to print out.\n"
		"  --stats-reset: to reset port statistics\n"
		"  --xstats-reset: to reset port extended statistics\n"
		"  --collectd-format: to print statistics to STDOUT in expected by collectd format\n"
		"  --host-id STRING: host id used to identify the system process is running on\n"
		"  --show-port: to display ports information\n"
		"  --show-tm: to display traffic manager information for ports\n"
		"  --show-crypto: to display crypto information\n"
		"  --show-ring[=name]: to display ring information\n"
		"  --show-mempool[=name]: to display mempool information\n"
		"  --iter-mempool=name: iterate mempool elements to display content\n",
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
		{"metrics", 0, NULL, 0},
		{"xstats-reset", 0, NULL, 0},
		{"xstats-name", required_argument, NULL, 1},
		{"collectd-format", 0, NULL, 0},
		{"xstats-ids", 1, NULL, 1},
		{"host-id", 0, NULL, 0},
		{"show-port", 0, NULL, 0},
		{"show-tm", 0, NULL, 0},
		{"show-crypto", 0, NULL, 0},
		{"show-ring", optional_argument, NULL, 0},
		{"show-mempool", optional_argument, NULL, 0},
		{"iter-mempool", required_argument, NULL, 0},
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
			else if (!strncmp(long_option[option_index].name,
					"metrics",
					MAX_LONG_OPT_SZ))
				enable_metrics = 1;
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

	metrics = rte_malloc("proc_info_metrics",
		sizeof(struct rte_metric_value) * len, 0);
	if (metrics == NULL) {
		printf("Cannot allocate memory for metrics\n");
		return;
	}

	names =  rte_malloc(NULL, sizeof(struct rte_metric_name) * len, 0);
	if (names == NULL) {
		printf("Cannot allocate memory for metrcis names\n");
		rte_free(metrics);
		return;
	}

	if (len != rte_metrics_get_names(names, len)) {
		printf("Cannot get metrics names\n");
		rte_free(metrics);
		rte_free(names);
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
		rte_free(metrics);
		rte_free(names);
		return;
	}

	int i;
	for (i = 0; i < len; i++)
		printf("%s: %"PRIu64"\n", names[i].name, metrics[i].value);

	printf("%s############################\n", nic_stats_border);
	rte_free(metrics);
	rte_free(names);
}

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
		uint64_t offload_flag = 1ULL << __builtin_ctzll(offloads);
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
		       dev_info.driver_name, dev_info.device->name,
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
		if (ret == 0 && fc_conf.mode != RTE_FC_NONE)  {
			printf("\t  -- flow control mode %s%s high %u low %u pause %u%s%s\n",
			       fc_conf.mode == RTE_FC_RX_PAUSE ? "rx " :
			       fc_conf.mode == RTE_FC_TX_PAUSE ? "tx " :
			       fc_conf.mode == RTE_FC_FULL ? "full" : "???",
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

			printf("\n");
		}

		for (j = 0; j < dev_info.nb_tx_queues; j++) {
			struct rte_eth_txq_info queue_info;

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
			printf("\t  -- indetical: non leaf %u leaf %u\n",
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
		       dev_info.device->numa_node,
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
				"\t  -- Single Consmer Dequeue (%u)\n",
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
				"\t  -- No IOVA config (%c)\n",
				ptr->name,
				ptr->socket_id,
				(flags & MEMPOOL_F_NO_SPREAD) ? 'y' : 'n',
				(flags & MEMPOOL_F_NO_CACHE_ALIGN) ? 'y' : 'n',
				(flags & MEMPOOL_F_SP_PUT) ? 'y' : 'n',
				(flags & MEMPOOL_F_SC_GET) ? 'y' : 'n',
				(flags & MEMPOOL_F_POOL_CREATED) ? 'y' : 'n',
				(flags & MEMPOOL_F_NO_IOVA_CONTIG) ? 'y' : 'n');
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

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* If no port mask was specified, then show non-owned ports */
	if (enabled_port_mask == 0) {
		RTE_ETH_FOREACH_DEV(i)
			enabled_port_mask = 1ul << i;
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
		else if (enable_metrics)
			metrics_display(i);

	}

	/* print port independent stats */
	if (enable_metrics)
		metrics_display(RTE_METRICS_GLOBAL);

	/* show information for PMD */
	if (enable_shw_port)
		show_port();
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

	RTE_ETH_FOREACH_DEV(i)
		rte_eth_dev_close(i);

	ret = rte_eal_cleanup();
	if (ret)
		printf("Error from rte_eal_cleanup(), %d\n", ret);

	return 0;
}
