/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>

#define MAX_NUM_STAGES 8
#define BATCH_SIZE 16
#define MAX_NUM_CORE 64

struct prod_data {
	uint8_t dev_id;
	uint8_t port_id;
	int32_t qid;
	unsigned int num_nic_ports;
} __rte_cache_aligned;

struct cons_data {
	uint8_t dev_id;
	uint8_t port_id;
} __rte_cache_aligned;

static struct prod_data prod_data;
static struct cons_data cons_data;

struct worker_data {
	uint8_t dev_id;
	uint8_t port_id;
} __rte_cache_aligned;

struct fastpath_data {
	volatile int done;
	uint32_t rx_lock;
	uint32_t tx_lock;
	uint32_t sched_lock;
	bool rx_single;
	bool tx_single;
	bool sched_single;
	unsigned int rx_core[MAX_NUM_CORE];
	unsigned int tx_core[MAX_NUM_CORE];
	unsigned int sched_core[MAX_NUM_CORE];
	unsigned int worker_core[MAX_NUM_CORE];
	struct rte_eth_dev_tx_buffer *tx_buf[RTE_MAX_ETHPORTS];
};

static struct fastpath_data *fdata;

struct config_data {
	unsigned int active_cores;
	unsigned int num_workers;
	int64_t num_packets;
	unsigned int num_fids;
	int queue_type;
	int worker_cycles;
	int enable_queue_priorities;
	int quiet;
	int dump_dev;
	int dump_dev_signal;
	unsigned int num_stages;
	unsigned int worker_cq_depth;
	int16_t next_qid[MAX_NUM_STAGES+2];
	int16_t qid[MAX_NUM_STAGES];
};

static struct config_data cdata = {
	.num_packets = (1L << 25), /* do ~32M packets */
	.num_fids = 512,
	.queue_type = RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY,
	.next_qid = {-1},
	.qid = {-1},
	.num_stages = 1,
	.worker_cq_depth = 16
};

static bool
core_in_use(unsigned int lcore_id) {
	return (fdata->rx_core[lcore_id] || fdata->sched_core[lcore_id] ||
		fdata->tx_core[lcore_id] || fdata->worker_core[lcore_id]);
}

static void
eth_tx_buffer_retry(struct rte_mbuf **pkts, uint16_t unsent,
			void *userdata)
{
	int port_id = (uintptr_t) userdata;
	unsigned int _sent = 0;

	do {
		/* Note: hard-coded TX queue */
		_sent += rte_eth_tx_burst(port_id, 0, &pkts[_sent],
					  unsent - _sent);
	} while (_sent != unsent);
}

static int
consumer(void)
{
	const uint64_t freq_khz = rte_get_timer_hz() / 1000;
	struct rte_event packets[BATCH_SIZE];

	static uint64_t received;
	static uint64_t last_pkts;
	static uint64_t last_time;
	static uint64_t start_time;
	unsigned int i, j;
	uint8_t dev_id = cons_data.dev_id;
	uint8_t port_id = cons_data.port_id;

	uint16_t n = rte_event_dequeue_burst(dev_id, port_id,
			packets, RTE_DIM(packets), 0);

	if (n == 0) {
		for (j = 0; j < rte_eth_dev_count(); j++)
			rte_eth_tx_buffer_flush(j, 0, fdata->tx_buf[j]);
		return 0;
	}
	if (start_time == 0)
		last_time = start_time = rte_get_timer_cycles();

	received += n;
	for (i = 0; i < n; i++) {
		uint8_t outport = packets[i].mbuf->port;
		rte_eth_tx_buffer(outport, 0, fdata->tx_buf[outport],
				packets[i].mbuf);
	}

	/* Print out mpps every 1<22 packets */
	if (!cdata.quiet && received >= last_pkts + (1<<22)) {
		const uint64_t now = rte_get_timer_cycles();
		const uint64_t total_ms = (now - start_time) / freq_khz;
		const uint64_t delta_ms = (now - last_time) / freq_khz;
		uint64_t delta_pkts = received - last_pkts;

		printf("# consumer RX=%"PRIu64", time %"PRIu64 "ms, "
			"avg %.3f mpps [current %.3f mpps]\n",
				received,
				total_ms,
				received / (total_ms * 1000.0),
				delta_pkts / (delta_ms * 1000.0));
		last_pkts = received;
		last_time = now;
	}

	cdata.num_packets -= n;
	if (cdata.num_packets <= 0)
		fdata->done = 1;

	return 0;
}

static int
producer(void)
{
	static uint8_t eth_port;
	struct rte_mbuf *mbufs[BATCH_SIZE+2];
	struct rte_event ev[BATCH_SIZE+2];
	uint32_t i, num_ports = prod_data.num_nic_ports;
	int32_t qid = prod_data.qid;
	uint8_t dev_id = prod_data.dev_id;
	uint8_t port_id = prod_data.port_id;
	uint32_t prio_idx = 0;

	const uint16_t nb_rx = rte_eth_rx_burst(eth_port, 0, mbufs, BATCH_SIZE);
	if (++eth_port == num_ports)
		eth_port = 0;
	if (nb_rx == 0) {
		rte_pause();
		return 0;
	}

	for (i = 0; i < nb_rx; i++) {
		ev[i].flow_id = mbufs[i]->hash.rss;
		ev[i].op = RTE_EVENT_OP_NEW;
		ev[i].sched_type = cdata.queue_type;
		ev[i].queue_id = qid;
		ev[i].event_type = RTE_EVENT_TYPE_ETHDEV;
		ev[i].sub_event_type = 0;
		ev[i].priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
		ev[i].mbuf = mbufs[i];
		RTE_SET_USED(prio_idx);
	}

	const int nb_tx = rte_event_enqueue_burst(dev_id, port_id, ev, nb_rx);
	if (nb_tx != nb_rx) {
		for (i = nb_tx; i < nb_rx; i++)
			rte_pktmbuf_free(mbufs[i]);
	}

	return 0;
}

static inline void
schedule_devices(uint8_t dev_id, unsigned int lcore_id)
{
	if (fdata->rx_core[lcore_id] && (fdata->rx_single ||
	    rte_atomic32_cmpset(&(fdata->rx_lock), 0, 1))) {
		producer();
		rte_atomic32_clear((rte_atomic32_t *)&(fdata->rx_lock));
	}

	if (fdata->sched_core[lcore_id] && (fdata->sched_single ||
	    rte_atomic32_cmpset(&(fdata->sched_lock), 0, 1))) {
		rte_event_schedule(dev_id);
		if (cdata.dump_dev_signal) {
			rte_event_dev_dump(0, stdout);
			cdata.dump_dev_signal = 0;
		}
		rte_atomic32_clear((rte_atomic32_t *)&(fdata->sched_lock));
	}

	if (fdata->tx_core[lcore_id] && (fdata->tx_single ||
	    rte_atomic32_cmpset(&(fdata->tx_lock), 0, 1))) {
		consumer();
		rte_atomic32_clear((rte_atomic32_t *)&(fdata->tx_lock));
	}
}

static inline void
work(struct rte_mbuf *m)
{
	struct ether_hdr *eth;
	struct ether_addr addr;

	/* change mac addresses on packet (to use mbuf data) */
	/*
	 * FIXME Swap mac address properly and also handle the
	 * case for both odd and even number of stages that the
	 * addresses end up the same at the end of the pipeline
	 */
	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_addr_copy(&eth->d_addr, &addr);
	ether_addr_copy(&addr, &eth->d_addr);

	/* do a number of cycles of work per packet */
	volatile uint64_t start_tsc = rte_rdtsc();
	while (rte_rdtsc() < start_tsc + cdata.worker_cycles)
		rte_pause();
}

static int
worker(void *arg)
{
	struct rte_event events[BATCH_SIZE];

	struct worker_data *data = (struct worker_data *)arg;
	uint8_t dev_id = data->dev_id;
	uint8_t port_id = data->port_id;
	size_t sent = 0, received = 0;
	unsigned int lcore_id = rte_lcore_id();

	while (!fdata->done) {
		uint16_t i;

		schedule_devices(dev_id, lcore_id);

		if (!fdata->worker_core[lcore_id]) {
			rte_pause();
			continue;
		}

		const uint16_t nb_rx = rte_event_dequeue_burst(dev_id, port_id,
				events, RTE_DIM(events), 0);

		if (nb_rx == 0) {
			rte_pause();
			continue;
		}
		received += nb_rx;

		for (i = 0; i < nb_rx; i++) {

			/* The first worker stage does classification */
			if (events[i].queue_id == cdata.qid[0])
				events[i].flow_id = events[i].mbuf->hash.rss
							% cdata.num_fids;

			events[i].queue_id = cdata.next_qid[events[i].queue_id];
			events[i].op = RTE_EVENT_OP_FORWARD;
			events[i].sched_type = cdata.queue_type;

			work(events[i].mbuf);
		}
		uint16_t nb_tx = rte_event_enqueue_burst(dev_id, port_id,
				events, nb_rx);
		while (nb_tx < nb_rx && !fdata->done)
			nb_tx += rte_event_enqueue_burst(dev_id, port_id,
							events + nb_tx,
							nb_rx - nb_tx);
		sent += nb_tx;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu TX=%zu\n",
				rte_lcore_id(), received, sent);

	return 0;
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
				mask |= (1UL << idx);
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
	{"queue-priority", no_argument, 0, 'P'},
	{"parallel", no_argument, 0, 'p'},
	{"ordered", no_argument, 0, 'o'},
	{"quiet", no_argument, 0, 'q'},
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
		c = getopt_long(argc, argv, "r:t:e:c:w:n:f:s:poPqDW:",
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
			cdata.queue_type = RTE_EVENT_QUEUE_CFG_ORDERED_ONLY;
			break;
		case 'p':
			cdata.queue_type = RTE_EVENT_QUEUE_CFG_PARALLEL_ONLY;
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
		default:
			usage();
		}
	}

	if (worker_lcore_mask == 0 || rx_lcore_mask == 0 ||
	    sched_lcore_mask == 0 || tx_lcore_mask == 0) {
		printf("Core part of pipeline was not assigned any cores. "
			"This will stall the pipeline, please check core masks "
			"(use -h for details on setting core masks):\n"
			"\trx: %"PRIu64"\n\ttx: %"PRIu64"\n\tsched: %"PRIu64
			"\n\tworkers: %"PRIu64"\n",
			rx_lcore_mask, tx_lcore_mask, sched_lcore_mask,
			worker_lcore_mask);
		rte_exit(-1, "Fix core masks\n");
	}
	if (cdata.num_stages == 0 || cdata.num_stages > MAX_NUM_STAGES)
		usage();

	for (i = 0; i < MAX_NUM_CORE; i++) {
		fdata->rx_core[i] = !!(rx_lcore_mask & (1UL << i));
		fdata->tx_core[i] = !!(tx_lcore_mask & (1UL << i));
		fdata->sched_core[i] = !!(sched_lcore_mask & (1UL << i));
		fdata->worker_core[i] = !!(worker_lcore_mask & (1UL << i));

		if (fdata->worker_core[i])
			cdata.num_workers++;
		if (core_in_use(i))
			cdata.active_cores++;
	}
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = ETHER_MAX_LEN
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_hf = ETH_RSS_IP |
					  ETH_RSS_TCP |
					  ETH_RSS_UDP,
			}
		}
	};
	const uint16_t rx_rings = 1, tx_rings = 1;
	const uint16_t rx_ring_size = 512, tx_ring_size = 512;
	struct rte_eth_conf port_conf = port_conf_default;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned int)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

static int
init_ports(unsigned int num_ports)
{
	uint8_t portid;
	unsigned int i;

	struct rte_mempool *mp = rte_pktmbuf_pool_create("packet_pool",
			/* mbufs */ 16384 * num_ports,
			/* cache_size */ 512,
			/* priv_size*/ 0,
			/* data_room_size */ RTE_MBUF_DEFAULT_BUF_SIZE,
			rte_socket_id());

	for (portid = 0; portid < num_ports; portid++)
		if (port_init(portid, mp) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
					portid);

	for (i = 0; i < num_ports; i++) {
		void *userdata = (void *)(uintptr_t) i;
		fdata->tx_buf[i] =
			rte_malloc(NULL, RTE_ETH_TX_BUFFER_SIZE(32), 0);
		if (fdata->tx_buf[i] == NULL)
			rte_panic("Out of memory\n");
		rte_eth_tx_buffer_init(fdata->tx_buf[i], 32);
		rte_eth_tx_buffer_set_err_callback(fdata->tx_buf[i],
						   eth_tx_buffer_retry,
						   userdata);
	}

	return 0;
}

struct port_link {
	uint8_t queue_id;
	uint8_t priority;
};

static int
setup_eventdev(struct prod_data *prod_data,
		struct cons_data *cons_data,
		struct worker_data *worker_data)
{
	const uint8_t dev_id = 0;
	/* +1 stages is for a SINGLE_LINK TX stage */
	const uint8_t nb_queues = cdata.num_stages + 1;
	/* + 2 is one port for producer and one for consumer */
	const uint8_t nb_ports = cdata.num_workers + 2;
	struct rte_event_dev_config config = {
			.nb_event_queues = nb_queues,
			.nb_event_ports = nb_ports,
			.nb_events_limit  = 4096,
			.nb_event_queue_flows = 1024,
			.nb_event_port_dequeue_depth = 128,
			.nb_event_port_enqueue_depth = 128,
	};
	struct rte_event_port_conf wkr_p_conf = {
			.dequeue_depth = cdata.worker_cq_depth,
			.enqueue_depth = 64,
			.new_event_threshold = 4096,
	};
	struct rte_event_queue_conf wkr_q_conf = {
			.event_queue_cfg = cdata.queue_type,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
	};
	struct rte_event_port_conf tx_p_conf = {
			.dequeue_depth = 128,
			.enqueue_depth = 128,
			.new_event_threshold = 4096,
	};
	const struct rte_event_queue_conf tx_q_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_HIGHEST,
			.event_queue_cfg =
					RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY |
					RTE_EVENT_QUEUE_CFG_SINGLE_LINK,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
	};

	struct port_link worker_queues[MAX_NUM_STAGES];
	struct port_link tx_queue;
	unsigned int i;

	int ret, ndev = rte_event_dev_count();
	if (ndev < 1) {
		printf("%d: No Eventdev Devices Found\n", __LINE__);
		return -1;
	}

	struct rte_event_dev_info dev_info;
	ret = rte_event_dev_info_get(dev_id, &dev_info);
	printf("\tEventdev %d: %s\n", dev_id, dev_info.driver_name);

	if (dev_info.max_event_port_dequeue_depth <
			config.nb_event_port_dequeue_depth)
		config.nb_event_port_dequeue_depth =
				dev_info.max_event_port_dequeue_depth;
	if (dev_info.max_event_port_enqueue_depth <
			config.nb_event_port_enqueue_depth)
		config.nb_event_port_enqueue_depth =
				dev_info.max_event_port_enqueue_depth;

	ret = rte_event_dev_configure(dev_id, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	/* Q creation - one load balanced per pipeline stage*/
	printf("  Stages:\n");
	for (i = 0; i < cdata.num_stages; i++) {
		if (rte_event_queue_setup(dev_id, i, &wkr_q_conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
		cdata.qid[i] = i;
		cdata.next_qid[i] = i+1;
		worker_queues[i].queue_id = i;
		if (cdata.enable_queue_priorities) {
			/* calculate priority stepping for each stage, leaving
			 * headroom of 1 for the SINGLE_LINK TX below
			 */
			const uint32_t prio_delta =
				(RTE_EVENT_DEV_PRIORITY_LOWEST-1) /  nb_queues;

			/* higher priority for queues closer to tx */
			wkr_q_conf.priority =
				RTE_EVENT_DEV_PRIORITY_LOWEST - prio_delta * i;
		}

		const char *type_str = "Atomic";
		switch (wkr_q_conf.event_queue_cfg) {
		case RTE_EVENT_QUEUE_CFG_ORDERED_ONLY:
			type_str = "Ordered";
			break;
		case RTE_EVENT_QUEUE_CFG_PARALLEL_ONLY:
			type_str = "Parallel";
			break;
		}
		printf("\tStage %d, Type %s\tPriority = %d\n", i, type_str,
				wkr_q_conf.priority);
	}
	printf("\n");

	/* final queue for sending to TX core */
	if (rte_event_queue_setup(dev_id, i, &tx_q_conf) < 0) {
		printf("%d: error creating qid %d\n", __LINE__, i);
		return -1;
	}
	tx_queue.queue_id = i;
	tx_queue.priority = RTE_EVENT_DEV_PRIORITY_HIGHEST;

	if (wkr_p_conf.dequeue_depth > config.nb_event_port_dequeue_depth)
		wkr_p_conf.dequeue_depth = config.nb_event_port_dequeue_depth;
	if (wkr_p_conf.enqueue_depth > config.nb_event_port_enqueue_depth)
		wkr_p_conf.enqueue_depth = config.nb_event_port_enqueue_depth;

	/* set up one port per worker, linking to all stage queues */
	for (i = 0; i < cdata.num_workers; i++) {
		struct worker_data *w = &worker_data[i];
		w->dev_id = dev_id;
		if (rte_event_port_setup(dev_id, i, &wkr_p_conf) < 0) {
			printf("Error setting up port %d\n", i);
			return -1;
		}

		uint32_t s;
		for (s = 0; s < cdata.num_stages; s++) {
			if (rte_event_port_link(dev_id, i,
						&worker_queues[s].queue_id,
						&worker_queues[s].priority,
						1) != 1) {
				printf("%d: error creating link for port %d\n",
						__LINE__, i);
				return -1;
			}
		}
		w->port_id = i;
	}

	if (tx_p_conf.dequeue_depth > config.nb_event_port_dequeue_depth)
		tx_p_conf.dequeue_depth = config.nb_event_port_dequeue_depth;
	if (tx_p_conf.enqueue_depth > config.nb_event_port_enqueue_depth)
		tx_p_conf.enqueue_depth = config.nb_event_port_enqueue_depth;

	/* port for consumer, linked to TX queue */
	if (rte_event_port_setup(dev_id, i, &tx_p_conf) < 0) {
		printf("Error setting up port %d\n", i);
		return -1;
	}
	if (rte_event_port_link(dev_id, i, &tx_queue.queue_id,
				&tx_queue.priority, 1) != 1) {
		printf("%d: error creating link for port %d\n",
				__LINE__, i);
		return -1;
	}
	/* port for producer, no links */
	struct rte_event_port_conf rx_p_conf = {
			.dequeue_depth = 8,
			.enqueue_depth = 8,
			.new_event_threshold = 1200,
	};

	if (rx_p_conf.dequeue_depth > config.nb_event_port_dequeue_depth)
		rx_p_conf.dequeue_depth = config.nb_event_port_dequeue_depth;
	if (rx_p_conf.enqueue_depth > config.nb_event_port_enqueue_depth)
		rx_p_conf.enqueue_depth = config.nb_event_port_enqueue_depth;

	if (rte_event_port_setup(dev_id, i + 1, &rx_p_conf) < 0) {
		printf("Error setting up port %d\n", i);
		return -1;
	}

	*prod_data = (struct prod_data){.dev_id = dev_id,
					.port_id = i + 1,
					.qid = cdata.qid[0] };
	*cons_data = (struct cons_data){.dev_id = dev_id,
					.port_id = i };

	if (rte_event_dev_start(dev_id) < 0) {
		printf("Error starting eventdev\n");
		return -1;
	}

	return dev_id;
}

static void
signal_handler(int signum)
{
	if (fdata->done)
		rte_exit(1, "Exiting on signal %d\n", signum);
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
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
	unsigned int num_ports;
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

	num_ports = rte_eth_dev_count();
	if (num_ports == 0)
		rte_panic("No ethernet ports found\n");

	const unsigned int cores_needed = cdata.active_cores;

	if (!cdata.quiet) {
		printf("  Config:\n");
		printf("\tports: %u\n", num_ports);
		printf("\tworkers: %u\n", cdata.num_workers);
		printf("\tpackets: %"PRIi64"\n", cdata.num_packets);
		printf("\tQueue-prio: %u\n", cdata.enable_queue_priorities);
		if (cdata.queue_type == RTE_EVENT_QUEUE_CFG_ORDERED_ONLY)
			printf("\tqid0 type: ordered\n");
		if (cdata.queue_type == RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY)
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

	worker_data = rte_calloc(0, cdata.num_workers,
			sizeof(worker_data[0]), 0);
	if (worker_data == NULL)
		rte_panic("rte_calloc failed\n");

	int dev_id = setup_eventdev(&prod_data, &cons_data, worker_data);
	if (dev_id < 0)
		rte_exit(EXIT_FAILURE, "Error setting up eventdev\n");

	prod_data.num_nic_ports = num_ports;
	init_ports(num_ports);

	int worker_idx = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (lcore_id >= MAX_NUM_CORE)
			break;

		if (!fdata->rx_core[lcore_id] &&
			!fdata->worker_core[lcore_id] &&
			!fdata->tx_core[lcore_id] &&
			!fdata->sched_core[lcore_id])
			continue;

		if (fdata->rx_core[lcore_id])
			printf(
				"[%s()] lcore %d executing NIC Rx, and using eventdev port %u\n",
				__func__, lcore_id, prod_data.port_id);

		if (fdata->tx_core[lcore_id])
			printf(
				"[%s()] lcore %d executing NIC Tx, and using eventdev port %u\n",
				__func__, lcore_id, cons_data.port_id);

		if (fdata->sched_core[lcore_id])
			printf("[%s()] lcore %d executing scheduler\n",
					__func__, lcore_id);

		if (fdata->worker_core[lcore_id])
			printf(
				"[%s()] lcore %d executing worker, using eventdev port %u\n",
				__func__, lcore_id,
				worker_data[worker_idx].port_id);

		err = rte_eal_remote_launch(worker, &worker_data[worker_idx],
					    lcore_id);
		if (err) {
			rte_panic("Failed to launch worker on core %d\n",
					lcore_id);
			continue;
		}
		if (fdata->worker_core[lcore_id])
			worker_idx++;
	}

	lcore_id = rte_lcore_id();

	if (core_in_use(lcore_id))
		worker(&worker_data[worker_idx++]);

	rte_eal_mp_wait_lcore();

	if (cdata.dump_dev)
		rte_event_dev_dump(dev_id, stdout);

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

	return 0;
}
