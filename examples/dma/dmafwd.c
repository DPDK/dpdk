/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Intel Corporation
 */

#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_dmadev.h>

/* size of ring used for software copying between rx and tx. */
#define RTE_LOGTYPE_DMA RTE_LOGTYPE_USER1
#define MAX_PKT_BURST 32
#define MEMPOOL_CACHE_SIZE 512
#define MIN_POOL_SIZE 65536U
#define CMD_LINE_OPT_MAC_UPDATING "mac-updating"
#define CMD_LINE_OPT_NO_MAC_UPDATING "no-mac-updating"
#define CMD_LINE_OPT_PORTMASK "portmask"
#define CMD_LINE_OPT_NB_QUEUE "nb-queue"
#define CMD_LINE_OPT_COPY_TYPE "copy-type"
#define CMD_LINE_OPT_RING_SIZE "ring-size"
#define CMD_LINE_OPT_BATCH_SIZE "dma-batch-size"
#define CMD_LINE_OPT_FRAME_SIZE "max-frame-size"
#define CMD_LINE_OPT_STATS_INTERVAL "stats-interval"

/* configurable number of RX/TX ring descriptors */
#define RX_DEFAULT_RINGSIZE 1024
#define TX_DEFAULT_RINGSIZE 1024

/* max number of RX queues per port */
#define MAX_RX_QUEUES_COUNT 8

struct rxtx_port_config {
	/* common config */
	uint16_t rxtx_port;
	uint16_t nb_queues;
	/* for software copy mode */
	struct rte_ring *rx_to_tx_ring;
	/* for dmadev HW copy mode */
	uint16_t dmadev_ids[MAX_RX_QUEUES_COUNT];
};

/* Configuring ports and number of assigned lcores in struct. 8< */
struct rxtx_transmission_config {
	struct rxtx_port_config ports[RTE_MAX_ETHPORTS];
	uint16_t nb_ports;
	uint16_t nb_lcores;
};
/* >8 End of configuration of ports and number of assigned lcores. */

/* per-port statistics struct */
struct dma_port_statistics {
	uint64_t rx[RTE_MAX_ETHPORTS];
	uint64_t tx[RTE_MAX_ETHPORTS];
	uint64_t tx_dropped[RTE_MAX_ETHPORTS];
	uint64_t copy_dropped[RTE_MAX_ETHPORTS];
};
struct dma_port_statistics port_statistics;
struct total_statistics {
	uint64_t total_packets_dropped;
	uint64_t total_packets_tx;
	uint64_t total_packets_rx;
	uint64_t total_submitted;
	uint64_t total_completed;
	uint64_t total_failed;
};

typedef enum copy_mode_t {
#define COPY_MODE_SW "sw"
	COPY_MODE_SW_NUM,
#define COPY_MODE_DMA "hw"
	COPY_MODE_DMA_NUM,
	COPY_MODE_INVALID_NUM,
	COPY_MODE_SIZE_NUM = COPY_MODE_INVALID_NUM
} copy_mode_t;

/* mask of enabled ports */
static uint32_t dma_enabled_port_mask;

/* number of RX queues per port */
static uint16_t nb_queues = 1;

/* MAC updating enabled by default. */
static int mac_updating = 1;

/* hardware copy mode enabled by default. */
static copy_mode_t copy_mode = COPY_MODE_DMA_NUM;

/* size of descriptor ring for hardware copy mode or
 * rte_ring for software copy mode
 */
static unsigned short ring_size = 2048;

/* interval, in seconds, between stats prints */
static unsigned short stats_interval = 1;
/* global mbuf arrays for tracking DMA bufs */
#define MBUF_RING_SIZE	2048
#define MBUF_RING_MASK	(MBUF_RING_SIZE - 1)
struct dma_bufs {
	struct rte_mbuf *bufs[MBUF_RING_SIZE];
	struct rte_mbuf *copies[MBUF_RING_SIZE];
	uint16_t sent;
};
static struct dma_bufs dma_bufs[RTE_DMADEV_DEFAULT_MAX];

/* global transmission config */
struct rxtx_transmission_config cfg;

/* configurable number of RX/TX ring descriptors */
static uint16_t nb_rxd = RX_DEFAULT_RINGSIZE;
static uint16_t nb_txd = TX_DEFAULT_RINGSIZE;

static volatile bool force_quit;

static uint32_t dma_batch_sz = MAX_PKT_BURST;
static uint32_t max_frame_size = RTE_ETHER_MAX_LEN;

/* ethernet addresses of ports */
static struct rte_ether_addr dma_ports_eth_addr[RTE_MAX_ETHPORTS];

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
struct rte_mempool *dma_pktmbuf_pool;

/* Print out statistics for one port. */
static void
print_port_stats(uint16_t port_id)
{
	printf("\nStatistics for port %u ------------------------------"
		"\nPackets sent: %34"PRIu64
		"\nPackets received: %30"PRIu64
		"\nPackets dropped on tx: %25"PRIu64
		"\nPackets dropped on copy: %23"PRIu64,
		port_id,
		port_statistics.tx[port_id],
		port_statistics.rx[port_id],
		port_statistics.tx_dropped[port_id],
		port_statistics.copy_dropped[port_id]);
}

/* Print out statistics for one dmadev device. */
static void
print_dmadev_stats(uint32_t dev_id, struct rte_dma_stats stats)
{
	printf("\nDMA channel %u", dev_id);
	printf("\n\t Total submitted ops: %"PRIu64"", stats.submitted);
	printf("\n\t Total completed ops: %"PRIu64"", stats.completed);
	printf("\n\t Total failed ops: %"PRIu64"", stats.errors);
}

static void
print_total_stats(struct total_statistics *ts)
{
	printf("\nAggregate statistics ==============================="
		"\nTotal packets Tx: %22"PRIu64" [pkt/s]"
		"\nTotal packets Rx: %22"PRIu64" [pkt/s]"
		"\nTotal packets dropped: %17"PRIu64" [pkt/s]",
		ts->total_packets_tx / stats_interval,
		ts->total_packets_rx / stats_interval,
		ts->total_packets_dropped / stats_interval);

	if (copy_mode == COPY_MODE_DMA_NUM) {
		printf("\nTotal submitted ops: %19"PRIu64" [ops/s]"
			"\nTotal completed ops: %19"PRIu64" [ops/s]"
			"\nTotal failed ops: %22"PRIu64" [ops/s]",
			ts->total_submitted / stats_interval,
			ts->total_completed / stats_interval,
			ts->total_failed / stats_interval);
	}

	printf("\n====================================================\n");
}

/* Print out statistics on packets dropped. */
static void
print_stats(char *prgname)
{
	struct total_statistics ts, delta_ts;
	struct rte_dma_stats stats = {0};
	uint32_t i, port_id, dev_id;
	char status_string[255]; /* to print at the top of the output */
	int status_strlen;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	status_strlen = snprintf(status_string, sizeof(status_string),
		"%s, ", prgname);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Worker Threads = %d, ",
		rte_lcore_count() > 2 ? 2 : 1);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Copy Mode = %s,\n", copy_mode == COPY_MODE_SW_NUM ?
		COPY_MODE_SW : COPY_MODE_DMA);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Updating MAC = %s, ", mac_updating ?
		"enabled" : "disabled");
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Rx Queues = %d, ", nb_queues);
	status_strlen += snprintf(status_string + status_strlen,
		sizeof(status_string) - status_strlen,
		"Ring Size = %d", ring_size);

	memset(&ts, 0, sizeof(struct total_statistics));

	while (!force_quit) {
		/* Sleep for "stats_interval" seconds each round - init sleep allows reading
		 * messages from app startup.
		 */
		sleep(stats_interval);

		/* Clear screen and move to top left */
		printf("%s%s", clr, topLeft);

		memset(&delta_ts, 0, sizeof(struct total_statistics));

		printf("%s\n", status_string);

		for (i = 0; i < cfg.nb_ports; i++) {
			port_id = cfg.ports[i].rxtx_port;
			print_port_stats(port_id);

			delta_ts.total_packets_dropped +=
				port_statistics.tx_dropped[port_id]
				+ port_statistics.copy_dropped[port_id];
			delta_ts.total_packets_tx +=
				port_statistics.tx[port_id];
			delta_ts.total_packets_rx +=
				port_statistics.rx[port_id];

			if (copy_mode == COPY_MODE_DMA_NUM) {
				uint32_t j;

				for (j = 0; j < cfg.ports[i].nb_queues; j++) {
					dev_id = cfg.ports[i].dmadev_ids[j];
					rte_dma_stats_get(dev_id, 0, &stats);
					print_dmadev_stats(dev_id, stats);

					delta_ts.total_submitted += stats.submitted;
					delta_ts.total_completed += stats.completed;
					delta_ts.total_failed += stats.errors;
				}
			}
		}

		delta_ts.total_packets_tx -= ts.total_packets_tx;
		delta_ts.total_packets_rx -= ts.total_packets_rx;
		delta_ts.total_packets_dropped -= ts.total_packets_dropped;
		delta_ts.total_submitted -= ts.total_submitted;
		delta_ts.total_completed -= ts.total_completed;
		delta_ts.total_failed -= ts.total_failed;

		printf("\n");
		print_total_stats(&delta_ts);

		fflush(stdout);

		ts.total_packets_tx += delta_ts.total_packets_tx;
		ts.total_packets_rx += delta_ts.total_packets_rx;
		ts.total_packets_dropped += delta_ts.total_packets_dropped;
		ts.total_submitted += delta_ts.total_submitted;
		ts.total_completed += delta_ts.total_completed;
		ts.total_failed += delta_ts.total_failed;
	}
}

static void
update_mac_addrs(struct rte_mbuf *m, uint32_t dest_portid)
{
	struct rte_ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* 02:00:00:00:00:xx - overwriting 2 bytes of source address but
	 * it's acceptable cause it gets overwritten by rte_ether_addr_copy
	 */
	tmp = &eth->dst_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);

	/* src addr */
	rte_ether_addr_copy(&dma_ports_eth_addr[dest_portid], &eth->src_addr);
}

/* Perform packet copy there is a user-defined function. 8< */
static inline void
pktmbuf_metadata_copy(const struct rte_mbuf *src, struct rte_mbuf *dst)
{
	dst->data_off = src->data_off;
	memcpy(&dst->rx_descriptor_fields1, &src->rx_descriptor_fields1,
		offsetof(struct rte_mbuf, buf_len) -
		offsetof(struct rte_mbuf, rx_descriptor_fields1));
}

/* Copy packet data */
static inline void
pktmbuf_sw_copy(struct rte_mbuf *src, struct rte_mbuf *dst)
{
	rte_memcpy(rte_pktmbuf_mtod(dst, char *),
		rte_pktmbuf_mtod(src, char *), src->data_len);
}
/* >8 End of perform packet copy there is a user-defined function. */

static uint32_t
dma_enqueue_packets(struct rte_mbuf *pkts[], struct rte_mbuf *pkts_copy[],
	uint32_t nb_rx, uint16_t dev_id)
{
	struct dma_bufs *dma = &dma_bufs[dev_id];
	int ret;
	uint32_t i;

	for (i = 0; i < nb_rx; i++) {
		/* Perform data copy */
		ret = rte_dma_copy(dev_id, 0,
			rte_pktmbuf_iova(pkts[i]),
			rte_pktmbuf_iova(pkts_copy[i]),
			rte_pktmbuf_data_len(pkts[i]), 0);

		if (ret < 0)
			break;

		dma->bufs[ret & MBUF_RING_MASK] = pkts[i];
		dma->copies[ret & MBUF_RING_MASK] = pkts_copy[i];
	}

	ret = i;
	return ret;
}

static inline uint32_t
dma_enqueue(struct rte_mbuf *pkts[], struct rte_mbuf *pkts_copy[],
		uint32_t num, uint32_t step, uint16_t dev_id)
{
	uint32_t i, k, m, n;

	k = 0;
	for (i = 0; i < num; i += m) {

		m = RTE_MIN(step, num - i);
		n = dma_enqueue_packets(pkts + i, pkts_copy + i, m, dev_id);
		k += n;
		if (n > 0)
			rte_dma_submit(dev_id, 0);

		/* don't try to enqueue more if HW queue is full */
		if (n != m)
			break;
	}

	return k;
}

static inline uint32_t
dma_dequeue(struct rte_mbuf *src[], struct rte_mbuf *dst[], uint32_t num,
	uint16_t dev_id)
{
	struct dma_bufs *dma = &dma_bufs[dev_id];
	uint16_t nb_dq, filled;
	/* Dequeue the mbufs from DMA device. Since all memory
	 * is DPDK pinned memory and therefore all addresses should
	 * be valid, we don't check for copy errors
	 */
	nb_dq = rte_dma_completed(dev_id, 0, num, NULL, NULL);

	/* Return early if no work to do */
	if (unlikely(nb_dq == 0))
		return nb_dq;

	/* Populate pkts_copy with the copies bufs from dma->copies for tx */
	for (filled = 0; filled < nb_dq; filled++) {
		src[filled] = dma->bufs[(dma->sent + filled) & MBUF_RING_MASK];
		dst[filled] = dma->copies[(dma->sent + filled) & MBUF_RING_MASK];
	}
	dma->sent += nb_dq;

	return filled;

}

/* Receive packets on one port and enqueue to dmadev or rte_ring. 8< */
static void
dma_rx_port(struct rxtx_port_config *rx_config)
{
	int32_t ret;
	uint32_t nb_rx, nb_enq, i, j;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst_copy[MAX_PKT_BURST];

	for (i = 0; i < rx_config->nb_queues; i++) {

		nb_rx = rte_eth_rx_burst(rx_config->rxtx_port, i,
			pkts_burst, MAX_PKT_BURST);

		if (nb_rx == 0)
			continue;

		port_statistics.rx[rx_config->rxtx_port] += nb_rx;

		ret = rte_mempool_get_bulk(dma_pktmbuf_pool,
			(void *)pkts_burst_copy, nb_rx);

		if (unlikely(ret < 0))
			rte_exit(EXIT_FAILURE,
				"Unable to allocate memory.\n");

		for (j = 0; j < nb_rx; j++)
			pktmbuf_metadata_copy(pkts_burst[j],
				pkts_burst_copy[j]);

		if (copy_mode == COPY_MODE_DMA_NUM) {
			/* enqueue packets for  hardware copy */
			nb_enq = dma_enqueue(pkts_burst, pkts_burst_copy,
				nb_rx, dma_batch_sz, rx_config->dmadev_ids[i]);

			/* free any not enqueued packets. */
			rte_mempool_put_bulk(dma_pktmbuf_pool,
				(void *)&pkts_burst[nb_enq],
				nb_rx - nb_enq);
			rte_mempool_put_bulk(dma_pktmbuf_pool,
				(void *)&pkts_burst_copy[nb_enq],
				nb_rx - nb_enq);

			port_statistics.copy_dropped[rx_config->rxtx_port] +=
				(nb_rx - nb_enq);

			/* get completed copies */
			nb_rx = dma_dequeue(pkts_burst, pkts_burst_copy,
				MAX_PKT_BURST, rx_config->dmadev_ids[i]);
		} else {
			/* Perform packet software copy, free source packets */
			for (j = 0; j < nb_rx; j++)
				pktmbuf_sw_copy(pkts_burst[j],
					pkts_burst_copy[j]);
		}

		rte_mempool_put_bulk(dma_pktmbuf_pool,
			(void *)pkts_burst, nb_rx);

		nb_enq = rte_ring_enqueue_burst(rx_config->rx_to_tx_ring,
			(void *)pkts_burst_copy, nb_rx, NULL);

		/* Free any not enqueued packets. */
		rte_mempool_put_bulk(dma_pktmbuf_pool,
			(void *)&pkts_burst_copy[nb_enq],
			nb_rx - nb_enq);

		port_statistics.copy_dropped[rx_config->rxtx_port] +=
			(nb_rx - nb_enq);
	}
}
/* >8 End of receive packets on one port and enqueue to dmadev or rte_ring. */

/* Transmit packets from dmadev/rte_ring for one port. 8< */
static void
dma_tx_port(struct rxtx_port_config *tx_config)
{
	uint32_t i, j, nb_dq, nb_tx;
	struct rte_mbuf *mbufs[MAX_PKT_BURST];

	for (i = 0; i < tx_config->nb_queues; i++) {

		/* Dequeue the mbufs from rx_to_tx_ring. */
		nb_dq = rte_ring_dequeue_burst(tx_config->rx_to_tx_ring,
				(void *)mbufs, MAX_PKT_BURST, NULL);
		if (nb_dq == 0)
			continue;

		/* Update macs if enabled */
		if (mac_updating) {
			for (j = 0; j < nb_dq; j++)
				update_mac_addrs(mbufs[j],
					tx_config->rxtx_port);
		}

		nb_tx = rte_eth_tx_burst(tx_config->rxtx_port, 0,
				(void *)mbufs, nb_dq);

		port_statistics.tx[tx_config->rxtx_port] += nb_tx;

		/* Free any unsent packets. */
		if (unlikely(nb_tx < nb_dq))
			rte_mempool_put_bulk(dma_pktmbuf_pool,
			(void *)&mbufs[nb_tx], nb_dq - nb_tx);
	}
}
/* >8 End of transmitting packets from dmadev. */

/* Main rx processing loop for dmadev. */
static void
rx_main_loop(void)
{
	uint16_t i;
	uint16_t nb_ports = cfg.nb_ports;

	RTE_LOG(INFO, DMA, "Entering main rx loop for copy on lcore %u\n",
		rte_lcore_id());

	while (!force_quit)
		for (i = 0; i < nb_ports; i++)
			dma_rx_port(&cfg.ports[i]);
}

/* Main tx processing loop for hardware copy. */
static void
tx_main_loop(void)
{
	uint16_t i;
	uint16_t nb_ports = cfg.nb_ports;

	RTE_LOG(INFO, DMA, "Entering main tx loop for copy on lcore %u\n",
		rte_lcore_id());

	while (!force_quit)
		for (i = 0; i < nb_ports; i++)
			dma_tx_port(&cfg.ports[i]);
}

/* Main rx and tx loop if only one worker lcore available */
static void
rxtx_main_loop(void)
{
	uint16_t i;
	uint16_t nb_ports = cfg.nb_ports;

	RTE_LOG(INFO, DMA, "Entering main rx and tx loop for copy on"
		" lcore %u\n", rte_lcore_id());

	while (!force_quit)
		for (i = 0; i < nb_ports; i++) {
			dma_rx_port(&cfg.ports[i]);
			dma_tx_port(&cfg.ports[i]);
		}
}

/* Start processing for each lcore. 8< */
static void start_forwarding_cores(void)
{
	uint32_t lcore_id = rte_lcore_id();

	RTE_LOG(INFO, DMA, "Entering %s on lcore %u\n",
		__func__, rte_lcore_id());

	if (cfg.nb_lcores == 1) {
		lcore_id = rte_get_next_lcore(lcore_id, true, true);
		rte_eal_remote_launch((lcore_function_t *)rxtx_main_loop,
			NULL, lcore_id);
	} else if (cfg.nb_lcores > 1) {
		lcore_id = rte_get_next_lcore(lcore_id, true, true);
		rte_eal_remote_launch((lcore_function_t *)rx_main_loop,
			NULL, lcore_id);

		lcore_id = rte_get_next_lcore(lcore_id, true, true);
		rte_eal_remote_launch((lcore_function_t *)tx_main_loop, NULL,
			lcore_id);
	}
}
/* >8 End of starting to process for each lcore. */

/* Display usage */
static void
dma_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
		"  -b --dma-batch-size: number of requests per DMA batch\n"
		"  -f --max-frame-size: max frame size\n"
		"  -p --portmask: hexadecimal bitmask of ports to configure\n"
		"  -q NQ: number of RX queues per port (default is 1)\n"
		"  --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default)\n"
		"      When enabled:\n"
		"       - The source MAC address is replaced by the TX port MAC address\n"
		"       - The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID\n"
		"  -c --copy-type CT: type of copy: sw|hw\n"
		"  -s --ring-size RS: size of dmadev descriptor ring for hardware copy mode or rte_ring for software copy mode\n"
		"  -i --stats-interval SI: interval, in seconds, between stats prints (default is 1)\n",
			prgname);
}

static int
dma_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* Parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static copy_mode_t
dma_parse_copy_mode(const char *copy_mode)
{
	if (strcmp(copy_mode, COPY_MODE_SW) == 0)
		return COPY_MODE_SW_NUM;
	else if (strcmp(copy_mode, COPY_MODE_DMA) == 0)
		return COPY_MODE_DMA_NUM;

	return COPY_MODE_INVALID_NUM;
}

/* Parse the argument given in the command line of the application */
static int
dma_parse_args(int argc, char **argv, unsigned int nb_ports)
{
	static const char short_options[] =
		"b:"  /* dma batch size */
		"c:"  /* copy type (sw|hw) */
		"f:"  /* max frame size */
		"p:"  /* portmask */
		"q:"  /* number of RX queues per port */
		"s:"  /* ring size */
		"i:"  /* interval, in seconds, between stats prints */
		;

	static const struct option lgopts[] = {
		{CMD_LINE_OPT_MAC_UPDATING, no_argument, &mac_updating, 1},
		{CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, &mac_updating, 0},
		{CMD_LINE_OPT_PORTMASK, required_argument, NULL, 'p'},
		{CMD_LINE_OPT_NB_QUEUE, required_argument, NULL, 'q'},
		{CMD_LINE_OPT_COPY_TYPE, required_argument, NULL, 'c'},
		{CMD_LINE_OPT_RING_SIZE, required_argument, NULL, 's'},
		{CMD_LINE_OPT_BATCH_SIZE, required_argument, NULL, 'b'},
		{CMD_LINE_OPT_FRAME_SIZE, required_argument, NULL, 'f'},
		{CMD_LINE_OPT_STATS_INTERVAL, required_argument, NULL, 'i'},
		{NULL, 0, 0, 0}
	};

	const unsigned int default_port_mask = (1 << nb_ports) - 1;
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	dma_enabled_port_mask = default_port_mask;
	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, short_options,
			lgopts, &option_index)) != EOF) {

		switch (opt) {
		case 'b':
			dma_batch_sz = atoi(optarg);
			if (dma_batch_sz > MAX_PKT_BURST) {
				printf("Invalid dma batch size, %s.\n", optarg);
				dma_usage(prgname);
				return -1;
			}
			break;
		case 'f':
			max_frame_size = atoi(optarg);
			if (max_frame_size > RTE_ETHER_MAX_JUMBO_FRAME_LEN) {
				printf("Invalid max frame size, %s.\n", optarg);
				dma_usage(prgname);
				return -1;
			}
			break;

		/* portmask */
		case 'p':
			dma_enabled_port_mask = dma_parse_portmask(optarg);
			if (dma_enabled_port_mask & ~default_port_mask ||
					dma_enabled_port_mask <= 0) {
				printf("Invalid portmask, %s, suggest 0x%x\n",
						optarg, default_port_mask);
				dma_usage(prgname);
				return -1;
			}
			break;

		case 'q':
			nb_queues = atoi(optarg);
			if (nb_queues == 0 || nb_queues > MAX_RX_QUEUES_COUNT) {
				printf("Invalid RX queues number %s. Max %u\n",
					optarg, MAX_RX_QUEUES_COUNT);
				dma_usage(prgname);
				return -1;
			}
			break;

		case 'c':
			copy_mode = dma_parse_copy_mode(optarg);
			if (copy_mode == COPY_MODE_INVALID_NUM) {
				printf("Invalid copy type. Use: sw, hw\n");
				dma_usage(prgname);
				return -1;
			}
			break;

		case 's':
			ring_size = atoi(optarg);
			if (ring_size == 0) {
				printf("Invalid ring size, %s.\n", optarg);
				dma_usage(prgname);
				return -1;
			}
			/* ring_size must be less-than or equal to MBUF_RING_SIZE
			 * to avoid overwriting bufs
			 */
			if (ring_size > MBUF_RING_SIZE) {
				printf("Max ring_size is %d, setting ring_size to max",
						MBUF_RING_SIZE);
				ring_size = MBUF_RING_SIZE;
			}
			break;

		case 'i':
			stats_interval = atoi(optarg);
			if (stats_interval == 0) {
				printf("Invalid stats interval, setting to 1\n");
				stats_interval = 1;	/* set to default */
			}
			break;

		/* long options */
		case 0:
			break;

		default:
			dma_usage(prgname);
			return -1;
		}
	}

	printf("MAC updating %s\n", mac_updating ? "enabled" : "disabled");
	if (optind >= 0)
		argv[optind - 1] = prgname;

	ret = optind - 1;
	optind = 1; /* reset getopt lib */
	return ret;
}

/* check link status, return true if at least one port is up */
static int
check_link_status(uint32_t port_mask)
{
	uint16_t portid;
	struct rte_eth_link link;
	int ret, link_status = 0;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status\n");
	RTE_ETH_FOREACH_DEV(portid) {
		if ((port_mask & (1 << portid)) == 0)
			continue;

		memset(&link, 0, sizeof(link));
		ret = rte_eth_link_get(portid, &link);
		if (ret < 0) {
			printf("Port %u link get failed: err=%d\n",
					portid, ret);
			continue;
		}

		/* Print link status */
		rte_eth_link_to_str(link_status_text,
			sizeof(link_status_text), &link);
		printf("Port %d %s\n", portid, link_status_text);

		if (link.link_status)
			link_status = 1;
	}
	return link_status;
}

/* Configuration of device. 8< */
static void
configure_dmadev_queue(uint32_t dev_id)
{
	struct rte_dma_info info;
	struct rte_dma_conf dev_config = { .nb_vchans = 1 };
	struct rte_dma_vchan_conf qconf = {
		.direction = RTE_DMA_DIR_MEM_TO_MEM,
		.nb_desc = ring_size
	};
	uint16_t vchan = 0;

	if (rte_dma_configure(dev_id, &dev_config) != 0)
		rte_exit(EXIT_FAILURE, "Error with rte_dma_configure()\n");

	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) != 0) {
		printf("Error with queue configuration\n");
		rte_panic();
	}
	rte_dma_info_get(dev_id, &info);
	if (info.nb_vchans != 1) {
		printf("Error, no configured queues reported on device id %u\n", dev_id);
		rte_panic();
	}
	if (rte_dma_start(dev_id) != 0)
		rte_exit(EXIT_FAILURE, "Error with rte_dma_start()\n");
}
/* >8 End of configuration of device. */

/* Using dmadev API functions. 8< */
static void
assign_dmadevs(void)
{
	uint16_t nb_dmadev = 0;
	int16_t dev_id = rte_dma_next_dev(0);
	uint32_t i, j;

	for (i = 0; i < cfg.nb_ports; i++) {
		for (j = 0; j < cfg.ports[i].nb_queues; j++) {
			if (dev_id == -1)
				goto end;

			cfg.ports[i].dmadev_ids[j] = dev_id;
			configure_dmadev_queue(cfg.ports[i].dmadev_ids[j]);
			dev_id = rte_dma_next_dev(dev_id + 1);
			++nb_dmadev;
		}
	}
end:
	if (nb_dmadev < cfg.nb_ports * cfg.ports[0].nb_queues)
		rte_exit(EXIT_FAILURE,
			"Not enough dmadevs (%u) for all queues (%u).\n",
			nb_dmadev, cfg.nb_ports * cfg.ports[0].nb_queues);
	RTE_LOG(INFO, DMA, "Number of used dmadevs: %u.\n", nb_dmadev);
}
/* >8 End of using dmadev API functions. */

/* Assign ring structures for packet exchanging. 8< */
static void
assign_rings(void)
{
	uint32_t i;

	for (i = 0; i < cfg.nb_ports; i++) {
		char ring_name[RTE_RING_NAMESIZE];

		snprintf(ring_name, sizeof(ring_name), "rx_to_tx_ring_%u", i);
		/* Create ring for inter core communication */
		cfg.ports[i].rx_to_tx_ring = rte_ring_create(
			ring_name, ring_size,
			rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (cfg.ports[i].rx_to_tx_ring == NULL)
			rte_exit(EXIT_FAILURE, "Ring create failed: %s\n",
				rte_strerror(rte_errno));
	}
}
/* >8 End of assigning ring structures for packet exchanging. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline void
port_init(uint16_t portid, struct rte_mempool *mbuf_pool, uint16_t nb_queues)
{
	/* Configuring port to use RSS for multiple RX queues. 8< */
	static const struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = RTE_ETH_MQ_RX_RSS,
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = RTE_ETH_RSS_PROTO_MASK,
			}
		}
	};
	/* >8 End of configuring port to use RSS for multiple RX queues. */

	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_conf local_port_conf = port_conf;
	struct rte_eth_dev_info dev_info;
	int ret, i;

	if (max_frame_size > local_port_conf.rxmode.mtu)
		local_port_conf.rxmode.mtu = max_frame_size;

	/* Skip ports that are not enabled */
	if ((dma_enabled_port_mask & (1 << portid)) == 0) {
		printf("Skipping disabled port %u\n", portid);
		return;
	}

	/* Init port */
	printf("Initializing port %u... ", portid);
	fflush(stdout);
	ret = rte_eth_dev_info_get(portid, &dev_info);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot get device info: %s, port=%u\n",
			rte_strerror(-ret), portid);

	local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
		dev_info.flow_type_rss_offloads;
	ret = rte_eth_dev_configure(portid, nb_queues, 1, &local_port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device:"
			" err=%d, port=%u\n", ret, portid);

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
			&nb_txd);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"Cannot adjust number of descriptors: err=%d, port=%u\n",
			ret, portid);

	rte_eth_macaddr_get(portid, &dma_ports_eth_addr[portid]);

	/* Init RX queues */
	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = local_port_conf.rxmode.offloads;
	for (i = 0; i < nb_queues; i++) {
		ret = rte_eth_rx_queue_setup(portid, i, nb_rxd,
			rte_eth_dev_socket_id(portid), &rxq_conf,
			mbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_rx_queue_setup:err=%d,port=%u, queue_id=%u\n",
				ret, portid, i);
	}

	/* Init one TX queue on each port */
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
			rte_eth_dev_socket_id(portid),
			&txq_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_tx_queue_setup:err=%d,port=%u\n",
			ret, portid);

	/* Initialize TX buffers */
	tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
			rte_eth_dev_socket_id(portid));
	if (tx_buffer[portid] == NULL)
		rte_exit(EXIT_FAILURE,
			"Cannot allocate buffer for tx on port %u\n",
			portid);

	rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
		rte_eth_tx_buffer_count_callback,
		&port_statistics.tx_dropped[portid]);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
			portid);

	/* Start device. 8< */
	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, portid);
	/* >8 End of starting device. */

	/* RX port is set in promiscuous mode. 8< */
	rte_eth_promiscuous_enable(portid);
	/* >8 End of RX port is set in promiscuous mode. */

	printf("Port %u, MAC address: " RTE_ETHER_ADDR_PRT_FMT "\n\n",
			portid,
			RTE_ETHER_ADDR_BYTES(&dma_ports_eth_addr[portid]));

	cfg.ports[cfg.nb_ports].rxtx_port = portid;
	cfg.ports[cfg.nb_ports++].nb_queues = nb_queues;
}

/* Get a device dump for each device being used by the application */
static void
dmadev_dump(void)
{
	uint32_t i, j;

	if (copy_mode != COPY_MODE_DMA_NUM)
		return;

	for (i = 0; i < cfg.nb_ports; i++)
		for (j = 0; j < cfg.ports[i].nb_queues; j++)
			rte_dma_dump(cfg.ports[i].dmadev_ids[j], stdout);
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
			signum);
		force_quit = true;
	} else if (signum == SIGUSR1) {
		dmadev_dump();
	}
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t nb_ports, portid;
	uint32_t i;
	unsigned int nb_mbufs;
	size_t sz;

	/* Init EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	/* >8 End of init EAL. */
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGUSR1, signal_handler);

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* Parse application arguments (after the EAL ones) */
	ret = dma_parse_args(argc, argv, nb_ports);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid DMA arguments\n");

	/* Allocates mempool to hold the mbufs. 8< */
	nb_mbufs = RTE_MAX(nb_ports * (nb_queues * (nb_rxd + nb_txd +
		4 * MAX_PKT_BURST + ring_size) + ring_size +
		rte_lcore_count() * MEMPOOL_CACHE_SIZE),
		MIN_POOL_SIZE);

	/* Create the mbuf pool */
	sz = max_frame_size + RTE_PKTMBUF_HEADROOM;
	sz = RTE_MAX(sz, (size_t)RTE_MBUF_DEFAULT_BUF_SIZE);
	dma_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, sz, rte_socket_id());
	if (dma_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	/* >8 End of allocates mempool to hold the mbufs. */

	/* Initialize each port. 8< */
	cfg.nb_ports = 0;
	RTE_ETH_FOREACH_DEV(portid)
		port_init(portid, dma_pktmbuf_pool, nb_queues);
	/* >8 End of initializing each port. */

	/* Initialize port xstats */
	memset(&port_statistics, 0, sizeof(port_statistics));

	/* Assigning each port resources. 8< */
	while (!check_link_status(dma_enabled_port_mask) && !force_quit)
		sleep(1);

	/* Check if there is enough lcores for all ports. */
	cfg.nb_lcores = rte_lcore_count() - 1;
	if (cfg.nb_lcores < 1)
		rte_exit(EXIT_FAILURE,
			"There should be at least one worker lcore.\n");

	if (copy_mode == COPY_MODE_DMA_NUM)
		assign_dmadevs();

	assign_rings();
	/* >8 End of assigning each port resources. */

	start_forwarding_cores();
	/* main core prints stats while other cores forward */
	print_stats(argv[0]);

	/* force_quit is true when we get here */
	rte_eal_mp_wait_lcore();

	uint32_t j;
	for (i = 0; i < cfg.nb_ports; i++) {
		printf("Closing port %d\n", cfg.ports[i].rxtx_port);
		ret = rte_eth_dev_stop(cfg.ports[i].rxtx_port);
		if (ret != 0)
			RTE_LOG(ERR, DMA, "rte_eth_dev_stop: err=%s, port=%u\n",
				rte_strerror(-ret), cfg.ports[i].rxtx_port);

		rte_eth_dev_close(cfg.ports[i].rxtx_port);
		if (copy_mode == COPY_MODE_DMA_NUM) {
			for (j = 0; j < cfg.ports[i].nb_queues; j++) {
				printf("Stopping dmadev %d\n",
					cfg.ports[i].dmadev_ids[j]);
				rte_dma_stop(cfg.ports[i].dmadev_ids[j]);
			}
		} else /* copy_mode == COPY_MODE_SW_NUM */
			rte_ring_free(cfg.ports[i].rx_to_tx_ring);
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	printf("Bye...\n");
	return 0;
}
