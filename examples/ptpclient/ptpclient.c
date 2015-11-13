/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

/*
 * This application is a simple Layer 2 PTP v2 client. It shows delta values
 * which are used to synchronize the PHC clock. if the "-T 1" parameter is
 * passed to the application the Linux kernel clock is also synchronized.
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <limits.h>
#include <sys/time.h>
#include <getopt.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS            8191
#define MBUF_CACHE_SIZE       250

/* Values for the PTP messageType field. */
#define SYNC                  0x0
#define DELAY_REQ             0x1
#define PDELAY_REQ            0x2
#define PDELAY_RESP           0x3
#define FOLLOW_UP             0x8
#define DELAY_RESP            0x9
#define PDELAY_RESP_FOLLOW_UP 0xA
#define ANNOUNCE              0xB
#define SIGNALING             0xC
#define MANAGEMENT            0xD

#define NSEC_PER_SEC        1000000000L
#define KERNEL_TIME_ADJUST_LIMIT  20000
#define PTP_PROTOCOL             0x88F7

struct rte_mempool *mbuf_pool;
uint32_t ptp_enabled_port_mask;
uint8_t ptp_enabled_port_nb;
static uint8_t ptp_enabled_ports[RTE_MAX_ETHPORTS];

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

static const struct ether_addr ether_multicast = {
	.addr_bytes = {0x01, 0x1b, 0x19, 0x0, 0x0, 0x0}
};

/* Structs used for PTP handling. */
struct tstamp {
	uint16_t   sec_msb;
	uint32_t   sec_lsb;
	uint32_t   ns;
}  __attribute__((packed));

struct clock_id {
	uint8_t id[8];
};

struct port_id {
	struct clock_id        clock_id;
	uint16_t               port_number;
}  __attribute__((packed));

struct ptp_header {
	uint8_t              msg_type;
	uint8_t              ver;
	uint16_t             message_length;
	uint8_t              domain_number;
	uint8_t              reserved1;
	uint8_t              flag_field[2];
	int64_t              correction;
	uint32_t             reserved2;
	struct port_id       source_port_id;
	uint16_t             seq_id;
	uint8_t              control;
	int8_t               log_message_interval;
} __attribute__((packed));

struct sync_msg {
	struct ptp_header   hdr;
	struct tstamp       origin_tstamp;
} __attribute__((packed));

struct follow_up_msg {
	struct ptp_header   hdr;
	struct tstamp       precise_origin_tstamp;
	uint8_t             suffix[0];
} __attribute__((packed));

struct delay_req_msg {
	struct ptp_header   hdr;
	struct tstamp       origin_tstamp;
} __attribute__((packed));

struct delay_resp_msg {
	struct ptp_header    hdr;
	struct tstamp        rx_tstamp;
	struct port_id       req_port_id;
	uint8_t              suffix[0];
} __attribute__((packed));

struct ptp_message {
	union {
		struct ptp_header          header;
		struct sync_msg            sync;
		struct delay_req_msg       delay_req;
		struct follow_up_msg       follow_up;
		struct delay_resp_msg      delay_resp;
	} __attribute__((packed));
};

struct ptpv2_data_slave_ordinary {
	struct rte_mbuf *m;
	struct timespec tstamp1;
	struct timespec tstamp2;
	struct timespec tstamp3;
	struct timespec tstamp4;
	struct clock_id client_clock_id;
	struct clock_id master_clock_id;
	struct timeval new_adj;
	int64_t delta;
	uint8_t portid;
	uint16_t seqID_SYNC;
	uint16_t seqID_FOLLOWUP;
	uint8_t ptpset;
	uint8_t kernel_time_set;
	uint8_t current_ptp_port;
};

static struct ptpv2_data_slave_ordinary ptp_data;

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
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1;
	const uint16_t tx_rings = 1;
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
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);

		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		/* Setup txq_flags */
		struct rte_eth_txconf *txconf;

		rte_eth_dev_info_get(q, &dev_info);
		txconf = &dev_info.default_txconf;
		txconf->txq_flags = 0;

		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Enable timesync timestamping for the Ethernet device */
	rte_eth_timesync_enable(port);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

static void
print_clock_info(struct ptpv2_data_slave_ordinary *ptp_data)
{
	int64_t nsec;
	struct timespec net_time, sys_time;

	printf("Master Clock id: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ptp_data->master_clock_id.id[0],
		ptp_data->master_clock_id.id[1],
		ptp_data->master_clock_id.id[2],
		ptp_data->master_clock_id.id[3],
		ptp_data->master_clock_id.id[4],
		ptp_data->master_clock_id.id[5],
		ptp_data->master_clock_id.id[6],
		ptp_data->master_clock_id.id[7]);

	printf("\nT2 - Slave  Clock.  %lds %ldns",
			(ptp_data->tstamp2.tv_sec),
			(ptp_data->tstamp2.tv_nsec));

	printf("\nT1 - Master Clock.  %lds %ldns ",
			ptp_data->tstamp1.tv_sec,
			(ptp_data->tstamp1.tv_nsec));

	printf("\nT3 - Slave  Clock.  %lds %ldns",
			ptp_data->tstamp3.tv_sec,
			(ptp_data->tstamp3.tv_nsec));

	printf("\nT4 - Master Clock.  %lds %ldns ",
			ptp_data->tstamp4.tv_sec,
			(ptp_data->tstamp4.tv_nsec));

	printf("\nDelta between master and slave clocks:%"PRId64"ns\n",
			ptp_data->delta);

	clock_gettime(CLOCK_REALTIME, &sys_time);
	rte_eth_timesync_read_time(ptp_data->current_ptp_port, &net_time);

	time_t ts = net_time.tv_sec;

	printf("\n\nComparison between Linux kernel Time and PTP:");

	printf("\nCurrent PTP Time: %.24s %.9ld ns",
			ctime(&ts), net_time.tv_nsec);

	nsec = (int64_t)timespec64_to_ns(&net_time) -
			(int64_t)timespec64_to_ns(&sys_time);
	ptp_data->new_adj = ns_to_timeval(nsec);

	gettimeofday(&ptp_data->new_adj, NULL);

	time_t tp = ptp_data->new_adj.tv_sec;

	printf("\nCurrent SYS Time: %.24s %.6ld ns",
				ctime(&tp), ptp_data->new_adj.tv_usec);

	printf("\nDelta between PTP and Linux Kernel time:%"PRId64"ns\n",
				nsec);

	printf("[Ctrl+C to quit]\n");

	/* Clear screen and put cursor in column 1, row 1 */
	printf("\033[2J\033[1;1H");
}

static int64_t
delta_eval(struct ptpv2_data_slave_ordinary *ptp_data)
{
	int64_t delta;
	uint64_t t1 = 0;
	uint64_t t2 = 0;
	uint64_t t3 = 0;
	uint64_t t4 = 0;

	t1 = timespec64_to_ns(&ptp_data->tstamp1);
	t2 = timespec64_to_ns(&ptp_data->tstamp2);
	t3 = timespec64_to_ns(&ptp_data->tstamp3);
	t4 = timespec64_to_ns(&ptp_data->tstamp4);

	delta = -((int64_t)((t2 - t1) - (t4 - t3))) / 2;

	return delta;
}

/*
 * Parse the PTP SYNC message.
 */
static void
parse_sync(struct ptpv2_data_slave_ordinary *ptp_data, uint16_t rx_tstamp_idx)
{
	struct ptp_header *ptp_hdr;

	ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(ptp_data->m, char *)
			+ sizeof(struct ether_hdr));
	ptp_data->seqID_SYNC = rte_be_to_cpu_16(ptp_hdr->seq_id);

	if (ptp_data->ptpset == 0) {
		rte_memcpy(&ptp_data->master_clock_id,
				&ptp_hdr->source_port_id.clock_id,
				sizeof(struct clock_id));
		ptp_data->ptpset = 1;
	}

	if (memcmp(&ptp_hdr->source_port_id.clock_id,
			&ptp_hdr->source_port_id.clock_id,
			sizeof(struct clock_id)) == 0) {

		if (ptp_data->ptpset == 1)
			rte_eth_timesync_read_rx_timestamp(ptp_data->portid,
					&ptp_data->tstamp2, rx_tstamp_idx);
	}

}

/*
 * Parse the PTP FOLLOWUP message and send DELAY_REQ to the master clock.
 */
static void
parse_fup(struct ptpv2_data_slave_ordinary *ptp_data)
{
	struct ether_hdr *eth_hdr;
	struct ptp_header *ptp_hdr;
	struct clock_id *client_clkid;
	struct ptp_message *ptp_msg;
	struct rte_mbuf *created_pkt;
	struct tstamp *origin_tstamp;
	struct ether_addr eth_multicast = ether_multicast;
	size_t pkt_size;
	int wait_us;
	struct rte_mbuf *m = ptp_data->m;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(m, char *)
			+ sizeof(struct ether_hdr));
	if (memcmp(&ptp_data->master_clock_id,
			&ptp_hdr->source_port_id.clock_id,
			sizeof(struct clock_id)) != 0)
		return;

	ptp_data->seqID_FOLLOWUP = rte_be_to_cpu_16(ptp_hdr->seq_id);
	ptp_msg = (struct ptp_message *) (rte_pktmbuf_mtod(m, char *) +
					  sizeof(struct ether_hdr));

	origin_tstamp = &ptp_msg->follow_up.precise_origin_tstamp;
	ptp_data->tstamp1.tv_nsec = ntohl(origin_tstamp->ns);
	ptp_data->tstamp1.tv_sec =
		((uint64_t)ntohl(origin_tstamp->sec_lsb)) |
		(((uint64_t)ntohs(origin_tstamp->sec_msb)) << 32);

	if (ptp_data->seqID_FOLLOWUP == ptp_data->seqID_SYNC) {

		created_pkt = rte_pktmbuf_alloc(mbuf_pool);
		pkt_size = sizeof(struct ether_hdr) +
			sizeof(struct ptp_message);
		created_pkt->data_len = pkt_size;
		created_pkt->pkt_len = pkt_size;
		eth_hdr = rte_pktmbuf_mtod(created_pkt, struct ether_hdr *);
		rte_eth_macaddr_get(ptp_data->portid, &eth_hdr->s_addr);

		/* Set multicast address 01-1B-19-00-00-00. */
		ether_addr_copy(&eth_multicast, &eth_hdr->d_addr);

		eth_hdr->ether_type = htons(PTP_PROTOCOL);
		ptp_msg = (struct ptp_message *)
			(rte_pktmbuf_mtod(created_pkt, char *) +
			sizeof(struct ether_hdr));

		ptp_msg->delay_req.hdr.seq_id = htons(ptp_data->seqID_SYNC);
		ptp_msg->delay_req.hdr.msg_type = DELAY_REQ;
		ptp_msg->delay_req.hdr.ver = 2;
		ptp_msg->delay_req.hdr.control = 1;
		ptp_msg->delay_req.hdr.log_message_interval = 127;

		/* Set up clock id. */
		client_clkid =
			&ptp_msg->delay_req.hdr.source_port_id.clock_id;

		client_clkid->id[0] = eth_hdr->s_addr.addr_bytes[0];
		client_clkid->id[1] = eth_hdr->s_addr.addr_bytes[1];
		client_clkid->id[2] = eth_hdr->s_addr.addr_bytes[2];
		client_clkid->id[3] = 0xFF;
		client_clkid->id[4] = 0xFE;
		client_clkid->id[5] = eth_hdr->s_addr.addr_bytes[3];
		client_clkid->id[6] = eth_hdr->s_addr.addr_bytes[4];
		client_clkid->id[7] = eth_hdr->s_addr.addr_bytes[5];

		rte_memcpy(&ptp_data->client_clock_id,
			   client_clkid,
			   sizeof(struct clock_id));

		/* Enable flag for hardware timestamping. */
		created_pkt->ol_flags |= PKT_TX_IEEE1588_TMST;

		/*Read value from NIC to prevent latching with old value. */
		rte_eth_timesync_read_tx_timestamp(ptp_data->portid,
				&ptp_data->tstamp3);

		/* Transmit the packet. */
		rte_eth_tx_burst(ptp_data->portid, 0, &created_pkt, 1);

		wait_us = 0;
		ptp_data->tstamp3.tv_nsec = 0;
		ptp_data->tstamp3.tv_sec = 0;

		/* Wait at least 1 us to read TX timestamp. */
		while ((rte_eth_timesync_read_tx_timestamp(ptp_data->portid,
				&ptp_data->tstamp3) < 0) && (wait_us < 1000)) {
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
parse_drsp(struct ptpv2_data_slave_ordinary *ptp_data)
{
	struct rte_mbuf *m = ptp_data->m;
	struct ptp_message *ptp_msg;
	struct tstamp *rx_tstamp;
	uint16_t seq_id;

	ptp_msg = (struct ptp_message *) (rte_pktmbuf_mtod(m, char *) +
					sizeof(struct ether_hdr));
	seq_id = rte_be_to_cpu_16(ptp_msg->delay_resp.hdr.seq_id);
	if (memcmp(&ptp_data->client_clock_id,
		   &ptp_msg->delay_resp.req_port_id.clock_id,
		   sizeof(struct clock_id)) == 0) {
		if (seq_id == ptp_data->seqID_FOLLOWUP) {
			rx_tstamp = &ptp_msg->delay_resp.rx_tstamp;
			ptp_data->tstamp4.tv_nsec = ntohl(rx_tstamp->ns);
			ptp_data->tstamp4.tv_sec =
				((uint64_t)ntohl(rx_tstamp->sec_lsb)) |
				(((uint64_t)ntohs(rx_tstamp->sec_msb)) << 32);

			/* Evaluate the delta for adjustment. */
			ptp_data->delta = delta_eval(ptp_data);

			rte_eth_timesync_adjust_time(ptp_data->portid,
						     ptp_data->delta);

			ptp_data->current_ptp_port = ptp_data->portid;

			/* Update kernel time if enabled in app parameters. */
			if (ptp_data->kernel_time_set == 1)
				update_kernel_time();



		}
	}
}

/* This function processes PTP packets, implementing slave PTP IEEE1588 L2
 * functionality.
 */
static void
parse_ptp_frames(uint8_t portid, struct rte_mbuf *m) {
	struct ptp_header *ptp_hdr;
	struct ether_hdr *eth_hdr;
	uint16_t eth_type;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (eth_type == PTP_PROTOCOL) {
		ptp_data.m = m;
		ptp_data.portid = portid;
		ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(m, char *)
					+ sizeof(struct ether_hdr));

		switch (ptp_hdr->msg_type) {
		case SYNC:
			parse_sync(&ptp_data, m->timesync);
			break;
		case FOLLOW_UP:
			parse_fup(&ptp_data);
			break;
		case DELAY_RESP:
			parse_drsp(&ptp_data);
			print_clock_info(&ptp_data);
			break;
		default:
			break;
		}
	}
}

/*
 * The lcore main. This is the main thread that does the work, reading from an
 * input port and writing to an output port.
 */
static __attribute__((noreturn)) void
lcore_main(void)
{
	uint8_t portid;
	unsigned nb_rx;
	struct rte_mbuf *m;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	printf("\nCore %u Waiting for SYNC packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Run until the application is quit or killed. */

	while (1) {
		/* Read packet from RX queues. */
		for (portid = 0; portid < ptp_enabled_port_nb; portid++) {

			portid = ptp_enabled_ports[portid];
			nb_rx = rte_eth_rx_burst(portid, 0, &m, 1);

			if (likely(nb_rx == 0))
				continue;

			if (m->ol_flags & PKT_RX_IEEE1588_PTP)
				parse_ptp_frames(portid, m);

			rte_pktmbuf_free(m);
		}
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
		return -1;

	if (pm == 0)
		return -1;

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

	optind = 0; /* Reset getopt lib. */

	return 0;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	unsigned nb_ports;

	uint8_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	memset(&ptp_data, '\0', sizeof(struct ptpv2_data_slave_ordinary));

	argc -= ret;
	argv += ret;

	ret = ptp_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with PTP initialization\n");

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count();

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	for (portid = 0; portid < nb_ports; portid++) {
		if ((ptp_enabled_port_mask & (1 << portid)) != 0) {
			if (port_init(portid, mbuf_pool) == 0) {
				ptp_enabled_ports[ptp_enabled_port_nb] = portid;
				ptp_enabled_port_nb++;
			} else {
				rte_exit(EXIT_FAILURE,
					 "Cannot init port %"PRIu8 "\n",
					 portid);
			}
		} else
			printf("Skipping disabled port %u\n", portid);
	}

	if (ptp_enabled_port_nb == 0) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled."
			" Please set portmask.\n");
	}

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main();

	return 0;
}
