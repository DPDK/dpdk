/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "l2fwd_event.h"
#include "l2fwd_poll.h"

/* display usage */
static void
l2fwd_event_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
	       "  -T PERIOD: statistics will be refreshed each PERIOD seconds "
	       "		(0 to disable, 10 default, 86400 maximum)\n"
	       "  --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default)\n"
	       "      When enabled:\n"
	       "       - The source MAC address is replaced by the TX port MAC address\n"
	       "       - The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID\n"
	       "  --mode: Packet transfer mode for I/O, poll or eventdev\n"
	       "          Default mode = eventdev\n"
	       "  --eventq-sched: Event queue schedule type, ordered, atomic or parallel.\n"
	       "                  Default: atomic\n"
	       "                  Valid only if --mode=eventdev\n\n",
	       prgname);
}

static int
l2fwd_event_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int
l2fwd_event_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_event_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

static void
l2fwd_event_parse_mode(const char *optarg,
		       struct l2fwd_resources *rsrc)
{
	if (!strncmp(optarg, "poll", 4))
		rsrc->event_mode = false;
	else if (!strncmp(optarg, "eventdev", 8))
		rsrc->event_mode = true;
}

static void
l2fwd_event_parse_eventq_sched(const char *optarg,
			       struct l2fwd_resources *rsrc)
{
	if (!strncmp(optarg, "ordered", 7))
		rsrc->sched_type = RTE_SCHED_TYPE_ORDERED;
	else if (!strncmp(optarg, "atomic", 6))
		rsrc->sched_type = RTE_SCHED_TYPE_ATOMIC;
	else if (!strncmp(optarg, "parallel", 8))
		rsrc->sched_type = RTE_SCHED_TYPE_PARALLEL;
}

static const char short_options[] =
	"p:"  /* portmask */
	"q:"  /* number of queues */
	"T:"  /* timer period */
	;

#define CMD_LINE_OPT_MAC_UPDATING "mac-updating"
#define CMD_LINE_OPT_NO_MAC_UPDATING "no-mac-updating"
#define CMD_LINE_OPT_MODE "mode"
#define CMD_LINE_OPT_EVENTQ_SCHED "eventq-sched"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options
	 */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_MODE_NUM,
	CMD_LINE_OPT_EVENTQ_SCHED_NUM,
};

/* Parse the argument given in the command line of the application */
static int
l2fwd_event_parse_args(int argc, char **argv,
		struct l2fwd_resources *rsrc)
{
	int mac_updating = 1;
	struct option lgopts[] = {
		{ CMD_LINE_OPT_MAC_UPDATING, no_argument, &mac_updating, 1},
		{ CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, &mac_updating, 0},
		{ CMD_LINE_OPT_MODE, required_argument, NULL,
							CMD_LINE_OPT_MODE_NUM},
		{ CMD_LINE_OPT_EVENTQ_SCHED, required_argument, NULL,
						CMD_LINE_OPT_EVENTQ_SCHED_NUM},
		{NULL, 0, 0, 0}
	};
	int opt, ret, timer_secs;
	char *prgname = argv[0];
	char **argvopt;
	int option_index;

	argvopt = argv;
	while ((opt = getopt_long(argc, argvopt, short_options,
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			rsrc->enabled_port_mask =
					l2fwd_event_parse_portmask(optarg);
			if (rsrc->enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_event_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			rsrc->rx_queue_per_lcore =
					l2fwd_event_parse_nqueue(optarg);
			if (rsrc->rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_event_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_secs = l2fwd_event_parse_timer_period(optarg);
			if (timer_secs < 0) {
				printf("invalid timer period\n");
				l2fwd_event_usage(prgname);
				return -1;
			}
			rsrc->timer_period = timer_secs;
			/* convert to number of cycles */
			rsrc->timer_period *= rte_get_timer_hz();
			break;

		case CMD_LINE_OPT_MODE_NUM:
			l2fwd_event_parse_mode(optarg, rsrc);
			break;

		case CMD_LINE_OPT_EVENTQ_SCHED_NUM:
			l2fwd_event_parse_eventq_sched(optarg, rsrc);
			break;

		/* long options */
		case 0:
			break;

		default:
			l2fwd_event_usage(prgname);
			return -1;
		}
	}

	rsrc->mac_updating = mac_updating;

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static int
l2fwd_launch_one_lcore(void *args)
{
	struct l2fwd_resources *rsrc = args;
	struct l2fwd_poll_resources *poll_rsrc = rsrc->poll_rsrc;

	poll_rsrc->poll_main_loop(rsrc);

	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(struct l2fwd_resources *rsrc,
			    uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t port_id;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status...");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (rsrc->force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(port_id) {
			if (rsrc->force_quit)
				return;
			if ((port_mask & (1 << port_id)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(port_id, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up. Speed %u Mbps - %s\n",
						port_id, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n", port_id);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

/* Print out statistics on packets dropped */
static void
print_stats(struct l2fwd_resources *rsrc)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	uint32_t port_id;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = {27, '[', '2', 'J', '\0' };
	const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
		/* skip disabled ports */
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   port_id,
			   rsrc->port_stats[port_id].tx,
			   rsrc->port_stats[port_id].rx,
			   rsrc->port_stats[port_id].dropped);

		total_packets_dropped +=
					rsrc->port_stats[port_id].dropped;
		total_packets_tx += rsrc->port_stats[port_id].tx;
		total_packets_rx += rsrc->port_stats[port_id].rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\n====================================================\n");
}

static void
l2fwd_event_print_stats(struct l2fwd_resources *rsrc)
{
	uint64_t prev_tsc = 0, diff_tsc, cur_tsc, timer_tsc = 0;
	const uint64_t timer_period = rsrc->timer_period;

	while (!rsrc->force_quit) {
		/* if timer is enabled */
		if (timer_period > 0) {
			cur_tsc = rte_rdtsc();
			diff_tsc = cur_tsc - prev_tsc;

			/* advance the timer */
			timer_tsc += diff_tsc;

			/* if timer has reached its timeout */
			if (unlikely(timer_tsc >= timer_period)) {
				print_stats(rsrc);
				/* reset the timer */
				timer_tsc = 0;
			}
			prev_tsc = cur_tsc;
		}
	}
}


static void
signal_handler(int signum)
{
	struct l2fwd_resources *rsrc = l2fwd_get_rsrc();
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		rsrc->force_quit = true;
	}
}

int
main(int argc, char **argv)
{
	struct l2fwd_resources *rsrc;
	uint16_t nb_ports_available = 0;
	uint32_t nb_ports_in_mask = 0;
	uint16_t port_id, last_port;
	uint32_t nb_mbufs;
	uint16_t nb_ports;
	int ret;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	rsrc = l2fwd_get_rsrc();

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_event_parse_args(argc, argv, rsrc);
	if (ret < 0)
		rte_panic("Invalid L2FWD arguments\n");

	printf("MAC updating %s\n", rsrc->mac_updating ? "enabled" :
			"disabled");

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_panic("No Ethernet ports - bye\n");

	/* check port mask to possible port mask */
	if (rsrc->enabled_port_mask & ~((1 << nb_ports) - 1))
		rte_panic("Invalid portmask; possible (0x%x)\n",
			(1 << nb_ports) - 1);

	/* reset l2fwd_dst_ports */
	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
		rsrc->dst_ports[port_id] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	RTE_ETH_FOREACH_DEV(port_id) {
		/* skip ports that are not enabled */
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0)
			continue;

		if (nb_ports_in_mask % 2) {
			rsrc->dst_ports[port_id] = last_port;
			rsrc->dst_ports[last_port] = port_id;
		} else {
			last_port = port_id;
		}

		nb_ports_in_mask++;
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		rsrc->dst_ports[last_port] = last_port;
	}

	nb_mbufs = RTE_MAX(nb_ports * (RTE_TEST_RX_DESC_DEFAULT +
				       RTE_TEST_TX_DESC_DEFAULT +
				       MAX_PKT_BURST + rte_lcore_count() *
				       MEMPOOL_CACHE_SIZE), 8192U);

	/* create the mbuf pool */
	rsrc->pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool",
			nb_mbufs, MEMPOOL_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (rsrc->pktmbuf_pool == NULL)
		rte_panic("Cannot init mbuf pool\n");

	nb_ports_available = l2fwd_event_init_ports(rsrc);
	if (!nb_ports_available)
		rte_panic("All available ports are disabled. Please set portmask.\n");

	/* Configure eventdev parameters if required */
	if (rsrc->event_mode)
		l2fwd_event_resource_setup(rsrc);
	else
		l2fwd_poll_resource_setup(rsrc);

	/* initialize port stats */
	memset(&rsrc->port_stats, 0,
					sizeof(struct l2fwd_port_statistics));

	/* All settings are done. Now enable eth devices */
	RTE_ETH_FOREACH_DEV(port_id) {
		/* skip ports that are not enabled */
		if ((rsrc->enabled_port_mask &
					(1 << port_id)) == 0)
			continue;

		ret = rte_eth_dev_start(port_id);
		if (ret < 0)
			rte_panic("rte_eth_dev_start:err=%d, port=%u\n", ret,
				  port_id);
	}

	check_all_ports_link_status(rsrc, rsrc->enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, rsrc,
				 SKIP_MASTER);
	l2fwd_event_print_stats(rsrc);
	rte_eal_mp_wait_lcore();

	RTE_ETH_FOREACH_DEV(port_id) {
		if ((rsrc->enabled_port_mask &
						(1 << port_id)) == 0)
			continue;
		printf("Closing port %d...", port_id);
		rte_eth_dev_stop(port_id);
		rte_eth_dev_close(port_id);
		printf(" Done\n");
	}
	printf("Bye...\n");

	return 0;
}
