/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

/*
 * This application is a simple Layer 2 PTP v2 client. It shows delta values
 * which are used to synchronize the PHC clock. if the "-T 1" parameter is
 * passed to the application the Linux kernel clock is also synchronized.
 */

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ptp.h>
#include <limits.h>
#include <sys/time.h>
#include <getopt.h>
#include <signal.h>

static volatile bool force_quit;

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS            8191
#define MBUF_CACHE_SIZE       250

#define NSEC_PER_SEC        1000000000L
#define KERNEL_TIME_ADJUST_LIMIT  20000

struct rte_mempool *mbuf_pool;
uint32_t ptp_enabled_port_mask;
uint8_t ptp_enabled_port_nb;
static uint8_t ptp_enabled_ports[RTE_MAX_ETHPORTS];

static const struct rte_ether_addr ether_multicast = {
	.addr_bytes = {0x01, 0x1b, 0x19, 0x0, 0x0, 0x0}
};

/* Structs used for PTP handling - using library definitions from rte_ptp.h */
struct __rte_packed_begin sync_msg {
	struct rte_ptp_hdr       hdr;
	struct rte_ptp_timestamp origin_tstamp;
} __rte_packed_end;

struct __rte_packed_begin follow_up_msg {
	struct rte_ptp_hdr       hdr;
	struct rte_ptp_timestamp precise_origin_tstamp;
	uint8_t             suffix[];
} __rte_packed_end;

struct __rte_packed_begin delay_req_msg {
	struct rte_ptp_hdr       hdr;
	struct rte_ptp_timestamp origin_tstamp;
} __rte_packed_end;

struct __rte_packed_begin delay_resp_msg {
	struct rte_ptp_hdr       hdr;
	struct rte_ptp_timestamp rx_tstamp;
	struct rte_ptp_port_id   req_port_id;
	uint8_t             suffix[];
} __rte_packed_end;

struct __rte_packed_begin ptp_message {
	union __rte_packed_begin {
		struct rte_ptp_hdr         header;
		struct sync_msg      sync;
		struct delay_req_msg delay_req;
		struct follow_up_msg follow_up;
		struct delay_resp_msg delay_resp;
	} __rte_packed_end;
} __rte_packed_end;

struct ptpv2_time_receiver_ordinary {
	struct rte_mbuf *m;
	struct timespec tstamp1;
	struct timespec tstamp2;
	struct timespec tstamp3;
	struct timespec tstamp4;
	uint8_t client_clock_id[8];
	uint8_t transmitter_clock_id[8];
	struct timeval new_adj;
	int64_t delta;
	uint16_t portid;
	uint16_t seqID_SYNC;
	uint16_t seqID_FOLLOWUP;
	uint8_t ptpset;
	uint8_t kernel_time_set;
	uint16_t current_ptp_port;
};

static struct ptpv2_time_receiver_ordinary ptp_data;

static inline uint64_t timespec64_to_ns(const struct timespec *ts)
{
	return ((uint64_t) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

static struct timeval
ns_to_timeval(int64_t nsec)
{
	struct timespec t_spec = {0, 0};
	struct timeval t_eval = {0, 0};
	int32_t rem;

	if (nsec == 0)
		return t_eval;
	rem = nsec % NSEC_PER_SEC;
	t_spec.tv_sec = nsec / NSEC_PER_SEC;

	if (rem < 0) {
		t_spec.tv_sec--;
		rem += NSEC_PER_SEC;
	}

	t_spec.tv_nsec = rem;
	t_eval.tv_sec = t_spec.tv_sec;
	t_eval.tv_usec = t_spec.tv_nsec / 1000;

	return t_eval;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mp)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf port_conf;
	const uint16_t rx_rings = 1;
	const uint16_t tx_rings = 1;
	int retval;
	uint16_t q;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	memset(&port_conf, 0, sizeof(struct rte_eth_conf));

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));

		return retval;
	}

	if (dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	/* Force full Tx path in the driver, required for IEEE1588 */
	port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		struct rte_eth_rxconf *rxconf;

		rxconf = &dev_info.default_rxconf;
		rxconf->offloads = port_conf.rxmode.offloads;

		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), rxconf, mp);

		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		struct rte_eth_txconf *txconf;

		txconf = &dev_info.default_txconf;
		txconf->offloads = port_conf.txmode.offloads;

		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Enable timesync timestamping for the Ethernet device */
	retval = rte_eth_timesync_enable(port);
	if (retval < 0) {
		printf("Timesync enable failed: %d\n", retval);
		return retval;
	}

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0) {
		printf("Promiscuous mode enable failed: %s\n",
			rte_strerror(-retval));
		return retval;
	}

	return 0;
}

static void
print_clock_info(struct ptpv2_time_receiver_ordinary *ptp)
{
	int64_t nsec;
	struct timespec net_time, sys_time;

	printf("time transmitter clock id: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ptp->transmitter_clock_id[0],
		ptp->transmitter_clock_id[1],
		ptp->transmitter_clock_id[2],
		ptp->transmitter_clock_id[3],
		ptp->transmitter_clock_id[4],
		ptp->transmitter_clock_id[5],
		ptp->transmitter_clock_id[6],
		ptp->transmitter_clock_id[7]);

	printf("\nT2 - time receiver clock.  %lds %ldns",
			(ptp->tstamp2.tv_sec),
			(ptp->tstamp2.tv_nsec));

	printf("\nT1 - time transmitter clock.  %lds %ldns ",
			ptp->tstamp1.tv_sec,
			(ptp->tstamp1.tv_nsec));

	printf("\nT3 - time receiver clock.  %lds %ldns",
			ptp->tstamp3.tv_sec,
			(ptp->tstamp3.tv_nsec));

	printf("\nT4 - time transmitter clock.  %lds %ldns\n",
			ptp->tstamp4.tv_sec,
			(ptp->tstamp4.tv_nsec));

	printf("\nDelta between transmitter and receiver clocks:%"PRId64"ns\n",
		ptp->delta);

	clock_gettime(CLOCK_REALTIME, &sys_time);
	rte_eth_timesync_read_time(ptp->current_ptp_port,
					&net_time);

	time_t ts = net_time.tv_sec;

	printf("\n\nComparison between Linux kernel Time and PTP:");

	printf("\nCurrent PTP Time: %.24s %.9ld ns",
		ctime(&ts), net_time.tv_nsec);

	nsec = (int64_t)timespec64_to_ns(&net_time) -
		(int64_t)timespec64_to_ns(&sys_time);
	ptp->new_adj = ns_to_timeval(nsec);

	gettimeofday(&ptp->new_adj, NULL);

	time_t tp = ptp->new_adj.tv_sec;

	printf("\nCurrent SYS Time: %.24s %.6ld ns",
		ctime(&tp), ptp->new_adj.tv_usec);

	printf("\nDelta between PTP and Linux Kernel time:%"PRId64"ns\n",
		nsec);

	printf("[Ctrl+C to quit]\n");

	/* Clear screen and put cursor in column 1, row 1 */
	printf("\033[2J\033[1;1H");
}

static int64_t
delta_eval(struct ptpv2_time_receiver_ordinary *ptp)
{
	int64_t delta;
	uint64_t t1 = 0;
	uint64_t t2 = 0;
	uint64_t t3 = 0;
	uint64_t t4 = 0;

	t1 = timespec64_to_ns(&ptp->tstamp1);
	t2 = timespec64_to_ns(&ptp->tstamp2);
	t3 = timespec64_to_ns(&ptp->tstamp3);
	t4 = timespec64_to_ns(&ptp->tstamp4);

	delta = -((int64_t)((t2 - t1) - (t4 - t3))) / 2;

	return delta;
}

/*
 * Parse the PTP SYNC message.
 */
static void
parse_sync(struct ptpv2_time_receiver_ordinary *ptp, uint16_t rx_tstamp_idx)
{
	struct rte_ptp_hdr *ptp_hdr;

	ptp_hdr = rte_pktmbuf_mtod_offset(ptp->m, struct rte_ptp_hdr *,
					  sizeof(struct rte_ether_hdr));
	ptp->seqID_SYNC = rte_be_to_cpu_16(ptp_hdr->sequence_id);

	if (ptp->ptpset == 0) {
		memcpy(ptp->transmitter_clock_id,
		       ptp_hdr->source_port_id.clock_id, 8);
		ptp->ptpset = 1;
	}

	if (memcmp(ptp->transmitter_clock_id,
			ptp_hdr->source_port_id.clock_id,
			8) == 0) {

		if (ptp->ptpset == 1)
			rte_eth_timesync_read_rx_timestamp(ptp->portid,
					&ptp->tstamp2, rx_tstamp_idx);
	}

}

/*
 * Parse the PTP FOLLOWUP message and send DELAY_REQ to the main clock.
 */
static void
parse_fup(struct ptpv2_time_receiver_ordinary *ptp)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ether_addr eth_addr;
	struct rte_ptp_hdr *ptp_hdr;
	struct ptp_message *ptp_msg;
	struct delay_req_msg *req_msg;
	struct rte_mbuf *created_pkt;
	struct rte_ptp_timestamp *origin_tstamp;
	struct rte_ether_addr eth_multicast = ether_multicast;
	size_t pkt_size;
	int wait_us;
	struct rte_mbuf *m = ptp->m;
	int ret;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	ptp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ptp_hdr *,
					  sizeof(struct rte_ether_hdr));
	if (memcmp(ptp->transmitter_clock_id,
			ptp_hdr->source_port_id.clock_id,
			8) != 0)
		return;

	ptp->seqID_FOLLOWUP = rte_be_to_cpu_16(ptp_hdr->sequence_id);
	ptp_msg = rte_pktmbuf_mtod_offset(m, struct ptp_message *,
					  sizeof(struct rte_ether_hdr));

	origin_tstamp = &ptp_msg->follow_up.precise_origin_tstamp;
	ptp->tstamp1.tv_nsec = rte_be_to_cpu_32(origin_tstamp->nanoseconds);
	ptp->tstamp1.tv_sec =
		((uint64_t)rte_be_to_cpu_32(origin_tstamp->seconds_lo)) |
		(((uint64_t)rte_be_to_cpu_16(origin_tstamp->seconds_hi)) << 32);

	if (ptp->seqID_FOLLOWUP == ptp->seqID_SYNC) {
		ret = rte_eth_macaddr_get(ptp->portid, &eth_addr);
		if (ret != 0) {
			printf("\nCore %u: port %u failed to get MAC address: %s\n",
				rte_lcore_id(), ptp->portid,
				rte_strerror(-ret));
			return;
		}

		created_pkt = rte_pktmbuf_alloc(mbuf_pool);
		pkt_size = sizeof(struct rte_ether_hdr) +
			sizeof(struct delay_req_msg);

		if (rte_pktmbuf_append(created_pkt, pkt_size) == NULL) {
			rte_pktmbuf_free(created_pkt);
			return;
		}
		created_pkt->data_len = pkt_size;
		created_pkt->pkt_len = pkt_size;
		eth_hdr = rte_pktmbuf_mtod(created_pkt, struct rte_ether_hdr *);
		rte_ether_addr_copy(&eth_addr, &eth_hdr->src_addr);

		/* Set multicast address 01-1B-19-00-00-00. */
		rte_ether_addr_copy(&eth_multicast, &eth_hdr->dst_addr);

		eth_hdr->ether_type = htons(RTE_ETHER_TYPE_1588);
		req_msg = rte_pktmbuf_mtod_offset(created_pkt,
			struct delay_req_msg *, sizeof(struct
			rte_ether_hdr));

		req_msg->hdr.sequence_id = htons(ptp->seqID_SYNC);
		req_msg->hdr.msg_type = RTE_PTP_MSGTYPE_DELAY_REQ;
		req_msg->hdr.version = 2;
		req_msg->hdr.msg_length =
			htons(sizeof(struct delay_req_msg));
		req_msg->hdr.domain_number = ptp_hdr->domain_number;

		/* Set up clock id. */
		ptp->client_clock_id[0] = eth_hdr->src_addr.addr_bytes[0];
		ptp->client_clock_id[1] = eth_hdr->src_addr.addr_bytes[1];
		ptp->client_clock_id[2] = eth_hdr->src_addr.addr_bytes[2];
		ptp->client_clock_id[3] = 0xFF;
		ptp->client_clock_id[4] = 0xFE;
		ptp->client_clock_id[5] = eth_hdr->src_addr.addr_bytes[3];
		ptp->client_clock_id[6] = eth_hdr->src_addr.addr_bytes[4];
		ptp->client_clock_id[7] = eth_hdr->src_addr.addr_bytes[5];

		memcpy(req_msg->hdr.source_port_id.clock_id,
		       ptp->client_clock_id, 8);

		/* Enable flag for hardware timestamping. */
		created_pkt->ol_flags |= RTE_MBUF_F_TX_IEEE1588_TMST;

		/*Read value from NIC to prevent latching with old value. */
		rte_eth_timesync_read_tx_timestamp(ptp->portid,
				&ptp->tstamp3);

		/* Transmit the packet. */
		rte_eth_tx_burst(ptp->portid, 0, &created_pkt, 1);

		wait_us = 0;
		ptp->tstamp3.tv_nsec = 0;
		ptp->tstamp3.tv_sec = 0;

		/* Wait at least 1 us to read TX timestamp. */
		while ((rte_eth_timesync_read_tx_timestamp(ptp->portid,
				&ptp->tstamp3) < 0) && (wait_us < 1000)) {
			rte_delay_us(1);
			wait_us++;
		}
	}
}

/*
 * Update the kernel time with the difference between it and the current NIC
 * time.
 */
static inline void
update_kernel_time(void)
{
	int64_t nsec;
	struct timespec net_time, sys_time;

	clock_gettime(CLOCK_REALTIME, &sys_time);
	rte_eth_timesync_read_time(ptp_data.current_ptp_port, &net_time);

	nsec = (int64_t)timespec64_to_ns(&net_time) -
	       (int64_t)timespec64_to_ns(&sys_time);

	ptp_data.new_adj = ns_to_timeval(nsec);

	/*
	 * If difference between kernel time and system time in NIC is too big
	 * (more than +/- 20 microseconds), use clock_settime to set directly
	 * the kernel time, as adjtime is better for small adjustments (takes
	 * longer to adjust the time).
	 */

	if (nsec > KERNEL_TIME_ADJUST_LIMIT || nsec < -KERNEL_TIME_ADJUST_LIMIT)
		clock_settime(CLOCK_REALTIME, &net_time);
	else
		adjtime(&ptp_data.new_adj, 0);


}

/*
 * Parse the DELAY_RESP message.
 */
static void
parse_drsp(struct ptpv2_time_receiver_ordinary *ptp)
{
	struct rte_mbuf *m = ptp->m;
	struct ptp_message *ptp_msg;
	struct rte_ptp_timestamp *rx_tstamp;
	uint16_t seq_id;

	ptp_msg = rte_pktmbuf_mtod_offset(m, struct ptp_message *,
					  sizeof(struct rte_ether_hdr));
	seq_id = rte_be_to_cpu_16(ptp_msg->delay_resp.hdr.sequence_id);
	if (memcmp(ptp->client_clock_id,
		   ptp_msg->delay_resp.req_port_id.clock_id,
		   8) == 0) {
		if (seq_id == ptp->seqID_FOLLOWUP) {
			rx_tstamp = &ptp_msg->delay_resp.rx_tstamp;
			ptp->tstamp4.tv_nsec = rte_be_to_cpu_32(rx_tstamp->nanoseconds);
			ptp->tstamp4.tv_sec =
				((uint64_t)rte_be_to_cpu_32(rx_tstamp->seconds_lo)) |
				(((uint64_t)rte_be_to_cpu_16(rx_tstamp->seconds_hi)) << 32);

			/* Evaluate the delta for adjustment. */
			ptp->delta = delta_eval(ptp);

			rte_eth_timesync_adjust_time(ptp->portid,
							ptp->delta);

			ptp->current_ptp_port = ptp->portid;

			/* Update kernel time if enabled in app parameters. */
			if (ptp->kernel_time_set == 1)
				update_kernel_time();



		}
	}
}

/* This function processes PTP packets, implementing time receiver PTP IEEE1588 L2
 * functionality.
 */

/* Parse ptp frames. 8< */
static void
parse_ptp_frames(uint16_t portid, struct rte_mbuf *m) {
	struct rte_ptp_hdr *ptp_hdr;
	struct rte_ether_hdr *eth_hdr;
	uint16_t eth_type;
	uint8_t msg_type;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (eth_type == RTE_ETHER_TYPE_1588) {
		ptp_data.m = m;
		ptp_data.portid = portid;
		ptp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ptp_hdr *,
						  sizeof(struct rte_ether_hdr));

		msg_type = ptp_hdr->msg_type;
		switch (msg_type) {
		case RTE_PTP_MSGTYPE_SYNC:
			parse_sync(&ptp_data, m->timesync);
			break;
		case RTE_PTP_MSGTYPE_FU:
			parse_fup(&ptp_data);
			break;
		case RTE_PTP_MSGTYPE_DELAY_RESP:
			parse_drsp(&ptp_data);
			print_clock_info(&ptp_data);
			break;
		default:
			break;
		}
	}
}
/* >8 End of function processes PTP packets. */

/*
 * The lcore main. This is the main thread that does the work, reading from an
 * input port and writing to an output port.
 */
static void
lcore_main(void)
{
	uint16_t portid;
	unsigned nb_rx;
	struct rte_mbuf *m;

	printf("\nCore %u Waiting for SYNC packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Run until the application is quit or killed. */

	while (!force_quit) {
		/* Read packet from RX queues. 8< */
		for (portid = 0; portid < ptp_enabled_port_nb; portid++) {

			portid = ptp_enabled_ports[portid];
			nb_rx = rte_eth_rx_burst(portid, 0, &m, 1);

			if (likely(nb_rx == 0))
				continue;

			/* Packet is parsed to determine which type. 8< */
			if (m->ol_flags & RTE_MBUF_F_RX_IEEE1588_PTP)
				parse_ptp_frames(portid, m);
			/* >8 End of packet is parsed to determine which type. */

			rte_pktmbuf_free(m);
		}
		/* >8 End of read packets from RX queues. */
	}
}

static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK -T VALUE\n"
		" -T VALUE: 0 - Disable, 1 - Enable Linux Clock"
		" Synchronization (0 default)\n"
		" -p PORTMASK: hexadecimal bitmask of ports to configure\n",
		prgname);
}

static int
ptp_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* Parse the hexadecimal string. */
	pm = strtoul(portmask, &end, 16);

	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static int
parse_ptp_kernel(const char *param)
{
	char *end = NULL;
	unsigned long pm;

	/* Parse the hexadecimal string. */
	pm = strtoul(param, &end, 16);

	if ((param[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (pm == 0)
		return 0;

	return 1;
}

/* Parse the commandline arguments. */
static int
ptp_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = { {NULL, 0, 0, 0} };

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:T:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {

		/* Portmask. */
		case 'p':
			ptp_enabled_port_mask = ptp_parse_portmask(optarg);
			if (ptp_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;
		/* Time synchronization. */
		case 'T':
			ret = parse_ptp_kernel(optarg);
			if (ret < 0) {
				print_usage(prgname);
				return -1;
			}

			ptp_data.kernel_time_set = ret;
			break;

		default:
			print_usage(prgname);
			return -1;
		}
	}

	argv[optind-1] = prgname;

	optind = 1; /* Reset getopt lib. */

	return 0;
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM)
		force_quit = true;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	unsigned nb_ports;

	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). 8< */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	/* >8 End of initialization of EAL. */

	memset(&ptp_data, 0, sizeof(struct ptpv2_time_receiver_ordinary));

	/* Parse specific arguments. 8< */
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = ptp_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with PTP initialization\n");
	/* >8 End of parsing specific arguments. */

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();

	/* Creates a new mempool in memory to hold the mbufs. 8< */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	/* >8 End of a new mempool in memory to hold the mbufs. */

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. 8< */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((ptp_enabled_port_mask & (1 << portid)) != 0) {
			if (port_init(portid, mbuf_pool) == 0) {
				ptp_enabled_ports[ptp_enabled_port_nb] = portid;
				ptp_enabled_port_nb++;
			} else {
				rte_exit(EXIT_FAILURE,
					 "Cannot init port %"PRIu16"\n",
					 portid);
			}
		} else
			printf("Skipping disabled port %u\n", portid);
	}
	/* >8 End of initialization of all ports. */

	if (ptp_enabled_port_nb == 0) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled."
			" Please set portmask.\n");
	}

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the main core only. */
	lcore_main();

	RTE_ETH_FOREACH_DEV(portid) {
		if ((ptp_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Disable timesync timestamping for the Ethernet device */
		rte_eth_timesync_disable(portid);

		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%d, port=%d\n", ret, portid);

		rte_eth_dev_close(portid);
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
