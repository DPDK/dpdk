/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>

#include "pipeline_common.h"

struct fastpath_data *fdata;

struct config_data cdata = {
	.num_packets = (1L << 25), /* do ~32M packets */
	.num_fids = 512,
	.queue_type = RTE_SCHED_TYPE_ATOMIC,
	.next_qid = {-1},
	.qid = {-1},
	.num_stages = 1,
	.worker_cq_depth = 16
};

static void
dump_core_info(unsigned int lcore_id, struct worker_data *data,
		unsigned int worker_idx)
{
	if (fdata->rx_core[lcore_id])
		printf(
			"[%s()] lcore %d executing NIC Rx\n",
			__func__, lcore_id);

	if (fdata->tx_core[lcore_id])
		printf(
			"[%s()] lcore %d executing NIC Tx\n",
			__func__, lcore_id);

	if (fdata->sched_core[lcore_id])
		printf(
			"[%s()] lcore %d executing scheduler\n",
			__func__, lcore_id);

	if (fdata->worker_core[lcore_id])
		printf(
			"[%s()] lcore %d executing worker, using eventdev port %u\n",
			__func__, lcore_id,
			data[worker_idx].port_id);
}

static bool
core_in_use(unsigned int lcore_id) {
	return (fdata->rx_core[lcore_id] || fdata->sched_core[lcore_id] ||
		fdata->tx_core[lcore_id] || fdata->worker_core[lcore_id]);
}

/*
 * Parse the coremask given as argument (hexadecimal string) and fill
 * the global configuration (core role and core count) with the parsed
 * value.
 */
static int xdigit2val(unsigned char c)
{
	int val;

	if (isdigit(c))
		val = c - '0';
	else if (isupper(c))
		val = c - 'A' + 10;
	else
		val = c - 'a' + 10;
	return val;
}

static uint64_t
parse_coremask(const char *coremask)
{
	int i, j, idx = 0;
	unsigned int count = 0;
	char c;
	int val;
	uint64_t mask = 0;
	const int32_t BITS_HEX = 4;

	if (coremask == NULL)
		return -1;
	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;
	if (coremask[0] == '0' && ((coremask[1] == 'x')
		|| (coremask[1] == 'X')))
		coremask += 2;
	i = strlen(coremask);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;
	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < MAX_NUM_CORE; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			return -1;
		}
		val = xdigit2val(c);
		for (j = 0; j < BITS_HEX && idx < MAX_NUM_CORE; j++, idx++) {
			if ((1 << j) & val) {
				mask |= (1ULL << idx);
				count++;
			}
		}
	}
	for (; i >= 0; i--)
		if (coremask[i] != '0')
			return -1;
	if (count == 0)
		return -1;
	return mask;
}

static struct option long_options[] = {
	{"workers", required_argument, 0, 'w'},
	{"packets", required_argument, 0, 'n'},
	{"atomic-flows", required_argument, 0, 'f'},
	{"num_stages", required_argument, 0, 's'},
	{"rx-mask", required_argument, 0, 'r'},
	{"tx-mask", required_argument, 0, 't'},
	{"sched-mask", required_argument, 0, 'e'},
	{"cq-depth", required_argument, 0, 'c'},
	{"work-cycles", required_argument, 0, 'W'},
	{"mempool-size", required_argument, 0, 'm'},
	{"queue-priority", no_argument, 0, 'P'},
	{"parallel", no_argument, 0, 'p'},
	{"ordered", no_argument, 0, 'o'},
	{"quiet", no_argument, 0, 'q'},
	{"use-atq", no_argument, 0, 'a'},
	{"dump", no_argument, 0, 'D'},
	{0, 0, 0, 0}
};

static void
usage(void)
{
	const char *usage_str =
		"  Usage: eventdev_demo [options]\n"
		"  Options:\n"
		"  -n, --packets=N              Send N packets (default ~32M), 0 implies no limit\n"
		"  -f, --atomic-flows=N         Use N random flows from 1 to N (default 16)\n"
		"  -s, --num_stages=N           Use N atomic stages (default 1)\n"
		"  -r, --rx-mask=core mask      Run NIC rx on CPUs in core mask\n"
		"  -w, --worker-mask=core mask  Run worker on CPUs in core mask\n"
		"  -t, --tx-mask=core mask      Run NIC tx on CPUs in core mask\n"
		"  -e  --sched-mask=core mask   Run scheduler on CPUs in core mask\n"
		"  -c  --cq-depth=N             Worker CQ depth (default 16)\n"
		"  -W  --work-cycles=N          Worker cycles (default 0)\n"
		"  -P  --queue-priority         Enable scheduler queue prioritization\n"
		"  -o, --ordered                Use ordered scheduling\n"
		"  -p, --parallel               Use parallel scheduling\n"
		"  -q, --quiet                  Minimize printed output\n"
		"  -a, --use-atq                Use all type queues\n"
		"  -m, --mempool-size=N         Dictate the mempool size\n"
		"  -D, --dump                   Print detailed statistics before exit"
		"\n";
	fprintf(stderr, "%s", usage_str);
	exit(1);
}

static void
parse_app_args(int argc, char **argv)
{
	/* Parse cli options*/
	int option_index;
	int c;
	opterr = 0;
	uint64_t rx_lcore_mask = 0;
	uint64_t tx_lcore_mask = 0;
	uint64_t sched_lcore_mask = 0;
	uint64_t worker_lcore_mask = 0;
	int i;

	for (;;) {
		c = getopt_long(argc, argv, "r:t:e:c:w:n:f:s:m:paoPqDW:",
				long_options, &option_index);
		if (c == -1)
			break;

		int popcnt = 0;
		switch (c) {
		case 'n':
			cdata.num_packets = (int64_t)atol(optarg);
			if (cdata.num_packets == 0)
				cdata.num_packets = INT64_MAX;
			break;
		case 'f':
			cdata.num_fids = (unsigned int)atoi(optarg);
			break;
		case 's':
			cdata.num_stages = (unsigned int)atoi(optarg);
			break;
		case 'c':
			cdata.worker_cq_depth = (unsigned int)atoi(optarg);
			break;
		case 'W':
			cdata.worker_cycles = (unsigned int)atoi(optarg);
			break;
		case 'P':
			cdata.enable_queue_priorities = 1;
			break;
		case 'o':
			cdata.queue_type = RTE_SCHED_TYPE_ORDERED;
			break;
		case 'p':
			cdata.queue_type = RTE_SCHED_TYPE_PARALLEL;
			break;
		case 'a':
			cdata.all_type_queues = 1;
			break;
		case 'q':
			cdata.quiet = 1;
			break;
		case 'D':
			cdata.dump_dev = 1;
			break;
		case 'w':
			worker_lcore_mask = parse_coremask(optarg);
			break;
		case 'r':
			rx_lcore_mask = parse_coremask(optarg);
			popcnt = __builtin_popcountll(rx_lcore_mask);
			fdata->rx_single = (popcnt == 1);
			break;
		case 't':
			tx_lcore_mask = parse_coremask(optarg);
			popcnt = __builtin_popcountll(tx_lcore_mask);
			fdata->tx_single = (popcnt == 1);
			break;
		case 'e':
			sched_lcore_mask = parse_coremask(optarg);
			popcnt = __builtin_popcountll(sched_lcore_mask);
			fdata->sched_single = (popcnt == 1);
			break;
		case 'm':
			cdata.num_mbuf = (uint64_t)atol(optarg);
			break;
		default:
			usage();
		}
	}

	cdata.worker_lcore_mask = worker_lcore_mask;
	cdata.sched_lcore_mask = sched_lcore_mask;
	cdata.rx_lcore_mask = rx_lcore_mask;
	cdata.tx_lcore_mask = tx_lcore_mask;

	if (cdata.num_stages == 0 || cdata.num_stages > MAX_NUM_STAGES)
		usage();

	for (i = 0; i < MAX_NUM_CORE; i++) {
		fdata->rx_core[i] = !!(rx_lcore_mask & (1ULL << i));
		fdata->tx_core[i] = !!(tx_lcore_mask & (1ULL << i));
		fdata->sched_core[i] = !!(sched_lcore_mask & (1ULL << i));
		fdata->worker_core[i] = !!(worker_lcore_mask & (1ULL << i));

		if (fdata->worker_core[i])
			cdata.num_workers++;
		if (core_in_use(i)) {
			if (!rte_lcore_is_enabled(i)) {
				printf("lcore %d is not enabled in lcore list\n",
					i);
				rte_exit(EXIT_FAILURE,
					"check lcore params failed\n");
			}
			cdata.active_cores++;
		}
	}
}

static void
do_capability_setup(uint8_t eventdev_id)
{
	int ret;
	uint16_t i;
	uint8_t generic_pipeline = 0;
	uint8_t burst = 0;

	RTE_ETH_FOREACH_DEV(i) {
		uint32_t caps = 0;

		ret = rte_event_eth_tx_adapter_caps_get(eventdev_id, i, &caps);
		if (ret)
			rte_exit(EXIT_FAILURE,
				"Invalid capability for Tx adptr port %d\n", i);
		generic_pipeline |= !(caps &
				RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT);
	}

	struct rte_event_dev_info eventdev_info;
	memset(&eventdev_info, 0, sizeof(struct rte_event_dev_info));

	rte_event_dev_info_get(eventdev_id, &eventdev_info);
	burst = eventdev_info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE ? 1 :
		0;

	if (generic_pipeline)
		set_worker_generic_setup_data(&fdata->cap, burst);
	else
		set_worker_tx_enq_setup_data(&fdata->cap, burst);
}

static void
signal_handler(int signum)
{
	static uint8_t once;

	if (fdata->done)
		rte_exit(1, "Exiting on signal %d\n", signum);
	if ((signum == SIGINT || signum == SIGTERM) && !once) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		if (cdata.dump_dev)
			rte_event_dev_dump(0, stdout);
		once = 1;
		fdata->done = 1;
	}
	if (signum == SIGTSTP)
		rte_event_dev_dump(0, stdout);
}

static inline uint64_t
port_stat(int dev_id, int32_t p)
{
	char statname[64];
	snprintf(statname, sizeof(statname), "port_%u_rx", p);
	return rte_event_dev_xstats_by_name_get(dev_id, statname, NULL);
}

int
main(int argc, char **argv)
{
	struct worker_data *worker_data;
	uint16_t num_ports;
	uint16_t portid;
	int lcore_id;
	int err;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGTSTP, signal_handler);

	err = rte_eal_init(argc, argv);
	if (err < 0)
		rte_panic("Invalid EAL arguments\n");

	argc -= err;
	argv += err;

	fdata = rte_malloc(NULL, sizeof(struct fastpath_data), 0);
	if (fdata == NULL)
		rte_panic("Out of memory\n");

	/* Parse cli options*/
	parse_app_args(argc, argv);

	num_ports = rte_eth_dev_count_avail();
	if (num_ports == 0)
		rte_panic("No ethernet ports found\n");

	const unsigned int cores_needed = cdata.active_cores;

	if (!cdata.quiet) {
		printf("  Config:\n");
		printf("\tports: %u\n", num_ports);
		printf("\tworkers: %u\n", cdata.num_workers);
		printf("\tpackets: %"PRIi64"\n", cdata.num_packets);
		printf("\tQueue-prio: %u\n", cdata.enable_queue_priorities);
		if (cdata.queue_type == RTE_SCHED_TYPE_ORDERED)
			printf("\tqid0 type: ordered\n");
		if (cdata.queue_type == RTE_SCHED_TYPE_ATOMIC)
			printf("\tqid0 type: atomic\n");
		printf("\tCores available: %u\n", rte_lcore_count());
		printf("\tCores used: %u\n", cores_needed);
	}

	if (rte_lcore_count() < cores_needed)
		rte_panic("Too few cores (%d < %d)\n", rte_lcore_count(),
				cores_needed);

	const unsigned int ndevs = rte_event_dev_count();
	if (ndevs == 0)
		rte_panic("No dev_id devs found. Pasl in a --vdev eventdev.\n");
	if (ndevs > 1)
		fprintf(stderr, "Warning: More than one eventdev, using idx 0");


	do_capability_setup(0);
	fdata->cap.check_opt();

	worker_data = rte_calloc(0, cdata.num_workers,
			sizeof(worker_data[0]), 0);
	if (worker_data == NULL)
		rte_panic("rte_calloc failed\n");

	int dev_id = fdata->cap.evdev_setup(worker_data);
	if (dev_id < 0)
		rte_exit(EXIT_FAILURE, "Error setting up eventdev\n");

	fdata->cap.adptr_setup(num_ports);

	/* Start the Ethernet port. */
	RTE_ETH_FOREACH_DEV(portid) {
		err = rte_eth_dev_start(portid);
		if (err < 0)
			rte_exit(EXIT_FAILURE, "Error starting ethdev %d\n",
					portid);
	}

	int worker_idx = 0;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (lcore_id >= MAX_NUM_CORE)
			break;

		if (!fdata->rx_core[lcore_id] &&
			!fdata->worker_core[lcore_id] &&
			!fdata->tx_core[lcore_id] &&
			!fdata->sched_core[lcore_id])
			continue;

		dump_core_info(lcore_id, worker_data, worker_idx);

		err = rte_eal_remote_launch(fdata->cap.worker,
				&worker_data[worker_idx], lcore_id);
		if (err) {
			rte_panic("Failed to launch worker on core %d\n",
					lcore_id);
			continue;
		}
		if (fdata->worker_core[lcore_id])
			worker_idx++;
	}

	lcore_id = rte_lcore_id();

	if (core_in_use(lcore_id)) {
		dump_core_info(lcore_id, worker_data, worker_idx);
		fdata->cap.worker(&worker_data[worker_idx]);

		if (fdata->worker_core[lcore_id])
			worker_idx++;
	}

	rte_eal_mp_wait_lcore();

	if (!cdata.quiet && (port_stat(dev_id, worker_data[0].port_id) !=
			(uint64_t)-ENOTSUP)) {
		printf("\nPort Workload distribution:\n");
		uint32_t i;
		uint64_t tot_pkts = 0;
		uint64_t pkts_per_wkr[RTE_MAX_LCORE] = {0};
		for (i = 0; i < cdata.num_workers; i++) {
			pkts_per_wkr[i] =
				port_stat(dev_id, worker_data[i].port_id);
			tot_pkts += pkts_per_wkr[i];
		}
		for (i = 0; i < cdata.num_workers; i++) {
			float pc = pkts_per_wkr[i]  * 100 /
				((float)tot_pkts);
			printf("worker %i :\t%.1f %% (%"PRIu64" pkts)\n",
					i, pc, pkts_per_wkr[i]);
		}

	}

	RTE_ETH_FOREACH_DEV(portid) {
		rte_event_eth_rx_adapter_stop(portid);
		rte_event_eth_tx_adapter_stop(portid);
		if (rte_eth_dev_stop(portid) < 0)
			printf("Failed to stop port %u", portid);
		rte_eth_dev_close(portid);
	}

	rte_event_dev_stop(0);
	rte_event_dev_close(0);

	rte_eal_cleanup();

	return 0;
}
