/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_string_fns.h>
#include "compat_netmap.h"


#define BUF_SIZE	RTE_MBUF_DEFAULT_DATAROOM
#define MBUF_DATA_SIZE	(BUF_SIZE + RTE_PKTMBUF_HEADROOM)

#define MBUF_PER_POOL	8192

struct rte_eth_conf eth_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.ignore_offload_bitfield = 1,
		.offloads = DEV_RX_OFFLOAD_CRC_STRIP,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

#define	MAX_QUEUE_NUM	1
#define	RX_QUEUE_NUM	1
#define	TX_QUEUE_NUM	1

#define	MAX_DESC_NUM	0x400
#define	RX_DESC_NUM	0x100
#define	TX_DESC_NUM	0x200

#define	RX_SYNC_NUM	0x20
#define	TX_SYNC_NUM	0x20

struct rte_netmap_port_conf port_conf = {
	.eth_conf = &eth_conf,
	.socket_id = SOCKET_ID_ANY,
	.nr_tx_rings = TX_QUEUE_NUM,
	.nr_rx_rings = RX_QUEUE_NUM,
	.nr_tx_slots = TX_DESC_NUM,
	.nr_rx_slots = RX_DESC_NUM,
	.tx_burst = TX_SYNC_NUM,
	.rx_burst = RX_SYNC_NUM,
};

struct rte_netmap_conf netmap_conf = {
	.socket_id = SOCKET_ID_ANY,
	.max_bufsz = BUF_SIZE,
	.max_rings = MAX_QUEUE_NUM,
	.max_slots = MAX_DESC_NUM,
};

static int stop = 0;

#define	MAX_PORT_NUM	2

struct netmap_port {
	int fd;
	struct netmap_if *nmif;
	struct netmap_ring *rx_ring;
	struct netmap_ring *tx_ring;
	const char *str;
	uint8_t id;
};

static struct {
	uint32_t num;
	struct netmap_port p[MAX_PORT_NUM];
	void *mem;
} ports;

static void
usage(const char *prgname)
{
	fprintf(stderr, "Usage: %s [EAL args] -- [OPTION]...\n"
		"-h, --help   \t Show this help message and exit\n"
		"-i INTERFACE_A   \t Interface (DPDK port number) to use\n"
		"[ -i INTERFACE_B   \t Interface (DPDK port number) to use ]\n",
		prgname);
}

static uint8_t
parse_portid(const char *portid_str)
{
	char *end;
	unsigned id;

	id = strtoul(portid_str, &end, 10);

	if (end == portid_str || *end != '\0' || id > RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE, "Invalid port number\n");

	return (uint8_t) id;
}

static int
parse_args(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "hi:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			rte_exit(EXIT_SUCCESS, "exiting...");
			break;
		case 'i':
			if (ports.num >= RTE_DIM(ports.p)) {
				usage(argv[0]);
				rte_exit(EXIT_FAILURE, "configs with %u "
					"ports are not supported\n",
					ports.num + 1);

			}

			ports.p[ports.num].str = optarg;
			ports.p[ports.num].id = parse_portid(optarg);
			ports.num++;
			break;
		default:
			usage(argv[0]);
			rte_exit(EXIT_FAILURE, "invalid option: %c\n", opt);
		}
	}

	return 0;
}

static void sigint_handler(__rte_unused int sig)
{
	stop = 1;
	signal(SIGINT, SIG_DFL);
}

static void move(int n, struct netmap_ring *rx, struct netmap_ring *tx)
{
	uint32_t tmp;

	while (n-- > 0) {
		tmp = tx->slot[tx->cur].buf_idx;

		tx->slot[tx->cur].buf_idx = rx->slot[rx->cur].buf_idx;
		tx->slot[tx->cur].len     = rx->slot[rx->cur].len;
		tx->slot[tx->cur].flags  |= NS_BUF_CHANGED;
		tx->cur = NETMAP_RING_NEXT(tx, tx->cur);
		tx->avail--;

		rx->slot[rx->cur].buf_idx = tmp;
		rx->slot[rx->cur].flags  |= NS_BUF_CHANGED;
		rx->cur = NETMAP_RING_NEXT(rx, rx->cur);
		rx->avail--;
	}
}

static int
netmap_port_open(uint32_t idx)
{
	int err;
	struct netmap_port *port;
	struct nmreq req;

	port = ports.p + idx;

	port->fd = rte_netmap_open("/dev/netmap", O_RDWR);

	snprintf(req.nr_name, sizeof(req.nr_name), "%s", port->str);
	req.nr_version = NETMAP_API;
	req.nr_ringid = 0;

	err = rte_netmap_ioctl(port->fd, NIOCGINFO, &req);
	if (err) {
		printf("[E] NIOCGINFO ioctl failed (error %d)\n", err);
		return err;
	}

	snprintf(req.nr_name, sizeof(req.nr_name), "%s", port->str);
	req.nr_version = NETMAP_API;
	req.nr_ringid = 0;

	err = rte_netmap_ioctl(port->fd, NIOCREGIF, &req);
	if (err) {
		printf("[E] NIOCREGIF ioctl failed (error %d)\n", err);
		return err;
	}

	/* mmap only once. */
	if (ports.mem == NULL)
		ports.mem = rte_netmap_mmap(NULL, req.nr_memsize,
			PROT_WRITE | PROT_READ, MAP_PRIVATE, port->fd, 0);

	if (ports.mem == MAP_FAILED) {
		printf("[E] NETMAP mmap failed for fd: %d)\n", port->fd);
		return -ENOMEM;
	}

	port->nmif = NETMAP_IF(ports.mem, req.nr_offset);

	port->tx_ring = NETMAP_TXRING(port->nmif, 0);
	port->rx_ring = NETMAP_RXRING(port->nmif, 0);

	return 0;
}


int main(int argc, char *argv[])
{
	int err, ret;
	uint32_t i, pmsk;
	struct nmreq req;
	struct pollfd pollfd[MAX_PORT_NUM];
	struct rte_mempool *pool;
	struct netmap_ring *rx_ring, *tx_ring;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");

	argc -= ret;
	argv += ret;

	parse_args(argc, argv);

	if (ports.num == 0)
		rte_exit(EXIT_FAILURE, "no ports specified\n");

	if (rte_eth_dev_count_avail() < 1)
		rte_exit(EXIT_FAILURE, "Not enough ethernet ports available\n");

	pool = rte_pktmbuf_pool_create("mbuf_pool", MBUF_PER_POOL, 32, 0,
		MBUF_DATA_SIZE, rte_socket_id());
	if (pool == NULL)
		rte_exit(EXIT_FAILURE, "Couldn't create mempool\n");

	netmap_conf.socket_id = rte_socket_id();
	err = rte_netmap_init(&netmap_conf);

	if (err < 0)
		rte_exit(EXIT_FAILURE,
			"Couldn't initialize librte_compat_netmap\n");
	else
		printf("librte_compat_netmap initialized\n");

	port_conf.pool = pool;
	port_conf.socket_id = rte_socket_id();

	for (i = 0; i != ports.num; i++) {

		err = rte_netmap_init_port(ports.p[i].id, &port_conf);
		if (err < 0)
			rte_exit(EXIT_FAILURE, "Couldn't setup port %hhu\n",
				ports.p[i].id);

		rte_eth_promiscuous_enable(ports.p[i].id);
	}

	for (i = 0; i != ports.num; i++) {

		err = netmap_port_open(i);
		if (err) {
			rte_exit(EXIT_FAILURE, "Couldn't set port %hhu "
				"under NETMAP control\n",
				ports.p[i].id);
		}
		else
			printf("Port %hhu now in Netmap mode\n", ports.p[i].id);
	}

	memset(pollfd, 0, sizeof(pollfd));

	for (i = 0; i != ports.num; i++) {
		pollfd[i].fd = ports.p[i].fd;
		pollfd[i].events = POLLIN | POLLOUT;
	}

	signal(SIGINT, sigint_handler);

	pmsk = ports.num - 1;

	printf("Bridge up and running!\n");

	while (!stop) {
		uint32_t n_pkts;

		pollfd[0].revents = 0;
		pollfd[1].revents = 0;

		ret = rte_netmap_poll(pollfd, ports.num, 0);
		if (ret < 0) {
	   		stop = 1;
	    		printf("[E] poll returned with error %d\n", ret);
		}

		if (((pollfd[0].revents | pollfd[1].revents) & POLLERR) != 0) {
			printf("POLLERR!\n");
		}

		if ((pollfd[0].revents & POLLIN) != 0 &&
				(pollfd[pmsk].revents & POLLOUT) != 0) {

			rx_ring = ports.p[0].rx_ring;
			tx_ring = ports.p[pmsk].tx_ring;

			n_pkts = RTE_MIN(rx_ring->avail, tx_ring->avail);
			move(n_pkts, rx_ring, tx_ring);
		}

		if (pmsk != 0 && (pollfd[pmsk].revents & POLLIN) != 0 &&
				(pollfd[0].revents & POLLOUT) != 0) {

			rx_ring = ports.p[pmsk].rx_ring;
			tx_ring = ports.p[0].tx_ring;

			n_pkts = RTE_MIN(rx_ring->avail, tx_ring->avail);
			move(n_pkts, rx_ring, tx_ring);
		}
	}

	printf("Bridge stopped!\n");

	for (i = 0; i != ports.num; i++) {
		err = rte_netmap_ioctl(ports.p[i].fd, NIOCUNREGIF, &req);
		if (err) {
			printf("[E] NIOCUNREGIF ioctl failed (error %d)\n",
				err);
		}
		else
			printf("Port %hhu unregistered from Netmap mode\n", ports.p[i].id);

		rte_netmap_close(ports.p[i].fd);
	}
	return 0;
}
