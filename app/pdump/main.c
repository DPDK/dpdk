/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <net/if.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_errno.h>
#include <rte_dev.h>
#include <rte_kvargs.h>
#include <rte_mempool.h>
#include <rte_ring.h>
#include <rte_pdump.h>

#define CMD_LINE_OPT_PDUMP "pdump"
#define PDUMP_PORT_ARG "port"
#define PDUMP_PCI_ARG "device_id"
#define PDUMP_QUEUE_ARG "queue"
#define PDUMP_DIR_ARG "dir"
#define PDUMP_RX_DEV_ARG "rx-dev"
#define PDUMP_TX_DEV_ARG "tx-dev"
#define PDUMP_RING_SIZE_ARG "ring-size"
#define PDUMP_MSIZE_ARG "mbuf-size"
#define PDUMP_NUM_MBUFS_ARG "total-num-mbufs"
#define CMD_LINE_OPT_SER_SOCK_PATH "server-socket-path"
#define CMD_LINE_OPT_CLI_SOCK_PATH "client-socket-path"

#define VDEV_PCAP "net_pcap_%s_%d,tx_pcap=%s"
#define VDEV_IFACE "net_pcap_%s_%d,tx_iface=%s"
#define TX_STREAM_SIZE 64

#define MP_NAME "pdump_pool_%d"

#define RX_RING "rx_ring_%d"
#define TX_RING "tx_ring_%d"

#define RX_STR "rx"
#define TX_STR "tx"

/* Maximum long option length for option parsing. */
#define APP_ARG_TCPDUMP_MAX_TUPLES 54
#define MBUF_POOL_CACHE_SIZE 250
#define TX_DESC_PER_QUEUE 512
#define RX_DESC_PER_QUEUE 128
#define MBUFS_PER_POOL 65535
#define MAX_LONG_OPT_SZ 64
#define RING_SIZE 16384
#define SIZE 256
#define BURST_SIZE 32
#define NUM_VDEVS 2

#define RTE_RING_SZ_MASK  (unsigned)(0x0fffffff) /**< Ring size mask */
/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

enum pdump_en_dis {
	DISABLE = 1,
	ENABLE = 2
};

enum pcap_stream {
	IFACE = 1,
	PCAP = 2
};

enum pdump_by {
	PORT_ID = 1,
	DEVICE_ID = 2
};

const char *valid_pdump_arguments[] = {
	PDUMP_PORT_ARG,
	PDUMP_PCI_ARG,
	PDUMP_QUEUE_ARG,
	PDUMP_DIR_ARG,
	PDUMP_RX_DEV_ARG,
	PDUMP_TX_DEV_ARG,
	PDUMP_RING_SIZE_ARG,
	PDUMP_MSIZE_ARG,
	PDUMP_NUM_MBUFS_ARG,
	NULL
};

struct pdump_stats {
	uint64_t dequeue_pkts;
	uint64_t tx_pkts;
	uint64_t freed_pkts;
};

struct pdump_tuples {
	/* cli params */
	uint8_t port;
	char *device_id;
	uint16_t queue;
	char rx_dev[TX_STREAM_SIZE];
	char tx_dev[TX_STREAM_SIZE];
	uint32_t ring_size;
	uint16_t mbuf_data_size;
	uint32_t total_num_mbufs;

	/* params for library API call */
	uint32_t dir;
	struct rte_mempool *mp;
	struct rte_ring *rx_ring;
	struct rte_ring *tx_ring;

	/* params for packet dumping */
	enum pdump_by dump_by_type;
	int rx_vdev_id;
	int tx_vdev_id;
	enum pcap_stream rx_vdev_stream_type;
	enum pcap_stream tx_vdev_stream_type;
	bool single_pdump_dev;

	/* stats */
	struct pdump_stats stats;
} __rte_cache_aligned;
static struct pdump_tuples pdump_t[APP_ARG_TCPDUMP_MAX_TUPLES];

struct parse_val {
	uint64_t min;
	uint64_t max;
	uint64_t val;
};

int num_tuples;
static struct rte_eth_conf port_conf_default;
volatile uint8_t quit_signal;
static char server_socket_path[PATH_MAX];
static char client_socket_path[PATH_MAX];

/**< display usage */
static void
pdump_usage(const char *prgname)
{
	printf("usage: %s [EAL options] -- --pdump "
			"'(port=<port id> | device_id=<pci id or vdev name>),"
			"(queue=<queue_id>),"
			"(rx-dev=<iface or pcap file> |"
			" tx-dev=<iface or pcap file>,"
			"[ring-size=<ring size>default:16384],"
			"[mbuf-size=<mbuf data size>default:2176],"
			"[total-num-mbufs=<number of mbufs>default:65535]'\n"
			"[--server-socket-path=<server socket dir>"
				"default:/var/run/.dpdk/ (or) ~/.dpdk/]\n"
			"[--client-socket-path=<client socket dir>"
				"default:/var/run/.dpdk/ (or) ~/.dpdk/]\n",
			prgname);
}

static int
parse_device_id(const char *key __rte_unused, const char *value,
		void *extra_args)
{
	struct pdump_tuples *pt = extra_args;

	pt->device_id = strdup(value);
	pt->dump_by_type = DEVICE_ID;

	return 0;
}

static int
parse_queue(const char *key __rte_unused, const char *value, void *extra_args)
{
	unsigned long n;
	struct pdump_tuples *pt = extra_args;

	if (!strcmp(value, "*"))
		pt->queue = RTE_PDUMP_ALL_QUEUES;
	else {
		n = strtoul(value, NULL, 10);
		pt->queue = (uint16_t) n;
	}
	return 0;
}

static int
parse_rxtxdev(const char *key, const char *value, void *extra_args)
{

	struct pdump_tuples *pt = extra_args;

	if (!strcmp(key, PDUMP_RX_DEV_ARG)) {
		snprintf(pt->rx_dev, sizeof(pt->rx_dev), "%s", value);
		/* identify the tx stream type for pcap vdev */
		if (if_nametoindex(pt->rx_dev))
			pt->rx_vdev_stream_type = IFACE;
	} else if (!strcmp(key, PDUMP_TX_DEV_ARG)) {
		snprintf(pt->tx_dev, sizeof(pt->tx_dev), "%s", value);
		/* identify the tx stream type for pcap vdev */
		if (if_nametoindex(pt->tx_dev))
			pt->tx_vdev_stream_type = IFACE;
	}

	return 0;
}

static int
parse_uint_value(const char *key, const char *value, void *extra_args)
{
	struct parse_val *v;
	unsigned long t;
	char *end;
	int ret = 0;

	errno = 0;
	v = extra_args;
	t = strtoul(value, &end, 10);

	if (errno != 0 || end[0] != 0 || t < v->min || t > v->max) {
		printf("invalid value:\"%s\" for key:\"%s\", "
			"value must be >= %"PRIu64" and <= %"PRIu64"\n",
			value, key, v->min, v->max);
		ret = -EINVAL;
	}
	if (!strcmp(key, PDUMP_RING_SIZE_ARG) && !POWEROF2(t)) {
		printf("invalid value:\"%s\" for key:\"%s\", "
			"value must be power of 2\n", value, key);
		ret = -EINVAL;
	}

	if (ret != 0)
		return ret;

	v->val = t;
	return 0;
}

static int
parse_pdump(const char *optarg)
{
	struct rte_kvargs *kvlist;
	int ret = 0, cnt1, cnt2;
	struct pdump_tuples *pt;
	struct parse_val v = {0};

	pt = &pdump_t[num_tuples];

	/* initial check for invalid arguments */
	kvlist = rte_kvargs_parse(optarg, valid_pdump_arguments);
	if (kvlist == NULL) {
		printf("--pdump=\"%s\": invalid argument passed\n", optarg);
		return -1;
	}

	/* port/device_id parsing and validation */
	cnt1 = rte_kvargs_count(kvlist, PDUMP_PORT_ARG);
	cnt2 = rte_kvargs_count(kvlist, PDUMP_PCI_ARG);
	if (!((cnt1 == 1 && cnt2 == 0) || (cnt1 == 0 && cnt2 == 1))) {
		printf("--pdump=\"%s\": must have either port or "
			"device_id argument\n", optarg);
		ret = -1;
		goto free_kvlist;
	} else if (cnt1 == 1) {
		v.min = 0;
		v.max = RTE_MAX_ETHPORTS-1;
		ret = rte_kvargs_process(kvlist, PDUMP_PORT_ARG,
				&parse_uint_value, &v);
		if (ret < 0)
			goto free_kvlist;
		pt->port = (uint8_t) v.val;
		pt->dump_by_type = PORT_ID;
	} else if (cnt2 == 1) {
		ret = rte_kvargs_process(kvlist, PDUMP_PCI_ARG,
				&parse_device_id, pt);
		if (ret < 0)
			goto free_kvlist;
	}

	/* queue parsing and validation */
	cnt1 = rte_kvargs_count(kvlist, PDUMP_QUEUE_ARG);
	if (cnt1 != 1) {
		printf("--pdump=\"%s\": must have queue argument\n", optarg);
		ret = -1;
		goto free_kvlist;
	}
	ret = rte_kvargs_process(kvlist, PDUMP_QUEUE_ARG, &parse_queue, pt);
	if (ret < 0)
		goto free_kvlist;

	/* rx-dev and tx-dev parsing and validation */
	cnt1 = rte_kvargs_count(kvlist, PDUMP_RX_DEV_ARG);
	cnt2 = rte_kvargs_count(kvlist, PDUMP_TX_DEV_ARG);
	if (cnt1 == 0 && cnt2 == 0) {
		printf("--pdump=\"%s\": must have either rx-dev or "
			"tx-dev argument\n", optarg);
		ret = -1;
		goto free_kvlist;
	} else if (cnt1 == 1 && cnt2 == 1) {
		ret = rte_kvargs_process(kvlist, PDUMP_RX_DEV_ARG,
					&parse_rxtxdev, pt);
		if (ret < 0)
			goto free_kvlist;
		ret = rte_kvargs_process(kvlist, PDUMP_TX_DEV_ARG,
					&parse_rxtxdev, pt);
		if (ret < 0)
			goto free_kvlist;
		/* if captured packets has to send to the same vdev */
		if (!strcmp(pt->rx_dev, pt->tx_dev))
			pt->single_pdump_dev = true;
		pt->dir = RTE_PDUMP_FLAG_RXTX;
	} else if (cnt1 == 1) {
		ret = rte_kvargs_process(kvlist, PDUMP_RX_DEV_ARG,
					&parse_rxtxdev, pt);
		if (ret < 0)
			goto free_kvlist;
		pt->dir = RTE_PDUMP_FLAG_RX;
	} else if (cnt2 == 1) {
		ret = rte_kvargs_process(kvlist, PDUMP_TX_DEV_ARG,
					&parse_rxtxdev, pt);
		if (ret < 0)
			goto free_kvlist;
		pt->dir = RTE_PDUMP_FLAG_TX;
	}

	/* optional */
	/* ring_size parsing and validation */
	cnt1 = rte_kvargs_count(kvlist, PDUMP_RING_SIZE_ARG);
	if (cnt1 == 1) {
		v.min = 2;
		v.max = RTE_RING_SZ_MASK-1;
		ret = rte_kvargs_process(kvlist, PDUMP_RING_SIZE_ARG,
						&parse_uint_value, &v);
		if (ret < 0)
			goto free_kvlist;
		pt->ring_size = (uint32_t) v.val;
	} else
		pt->ring_size = RING_SIZE;

	/* mbuf_data_size parsing and validation */
	cnt1 = rte_kvargs_count(kvlist, PDUMP_MSIZE_ARG);
	if (cnt1 == 1) {
		v.min = 1;
		v.max = UINT16_MAX;
		ret = rte_kvargs_process(kvlist, PDUMP_MSIZE_ARG,
						&parse_uint_value, &v);
		if (ret < 0)
			goto free_kvlist;
		pt->mbuf_data_size = (uint16_t) v.val;
	} else
		pt->mbuf_data_size = RTE_MBUF_DEFAULT_BUF_SIZE;

	/* total_num_mbufs parsing and validation */
	cnt1 = rte_kvargs_count(kvlist, PDUMP_NUM_MBUFS_ARG);
	if (cnt1 == 1) {
		v.min = 1025;
		v.max = UINT16_MAX;
		ret = rte_kvargs_process(kvlist, PDUMP_NUM_MBUFS_ARG,
						&parse_uint_value, &v);
		if (ret < 0)
			goto free_kvlist;
		pt->total_num_mbufs = (uint16_t) v.val;
	} else
		pt->total_num_mbufs = MBUFS_PER_POOL;

	num_tuples++;

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

/* Parse the argument given in the command line of the application */
static int
launch_args_parse(int argc, char **argv, char *prgname)
{
	int opt, ret;
	int option_index;
	static struct option long_option[] = {
		{"pdump", 1, 0, 0},
		{"server-socket-path", 1, 0, 0},
		{"client-socket-path", 1, 0, 0},
		{NULL, 0, 0, 0}
	};

	if (argc == 1)
		pdump_usage(prgname);

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, " ",
			long_option, &option_index)) != EOF) {
		switch (opt) {
		case 0:
			if (!strncmp(long_option[option_index].name,
					CMD_LINE_OPT_PDUMP,
					sizeof(CMD_LINE_OPT_PDUMP))) {
				ret = parse_pdump(optarg);
				if (ret) {
					pdump_usage(prgname);
					return -1;
				}
			}

			if (!strncmp(long_option[option_index].name,
					CMD_LINE_OPT_SER_SOCK_PATH,
					sizeof(CMD_LINE_OPT_SER_SOCK_PATH))) {
				snprintf(server_socket_path,
					sizeof(server_socket_path), "%s",
					optarg);
			}

			if (!strncmp(long_option[option_index].name,
					CMD_LINE_OPT_CLI_SOCK_PATH,
					sizeof(CMD_LINE_OPT_CLI_SOCK_PATH))) {
				snprintf(client_socket_path,
					sizeof(client_socket_path), "%s",
					optarg);
			}

			break;
		default:
			pdump_usage(prgname);
			return -1;
		}
	}

	return 0;
}

static void
print_pdump_stats(void)
{
	int i;
	struct pdump_tuples *pt;

	for (i = 0; i < num_tuples; i++) {
		printf("##### PDUMP DEBUG STATS #####\n");
		pt = &pdump_t[i];
		printf(" -packets dequeued:			%"PRIu64"\n",
							pt->stats.dequeue_pkts);
		printf(" -packets transmitted to vdev:		%"PRIu64"\n",
							pt->stats.tx_pkts);
		printf(" -packets freed:			%"PRIu64"\n",
							pt->stats.freed_pkts);
	}
}

static inline void
disable_pdump(struct pdump_tuples *pt)
{
	if (pt->dump_by_type == DEVICE_ID)
		rte_pdump_disable_by_deviceid(pt->device_id, pt->queue,
						pt->dir);
	else if (pt->dump_by_type == PORT_ID)
		rte_pdump_disable(pt->port, pt->queue, pt->dir);
}

static inline void
pdump_rxtx(struct rte_ring *ring, uint8_t vdev_id, struct pdump_stats *stats)
{
	/* write input packets of port to vdev for pdump */
	struct rte_mbuf *rxtx_bufs[BURST_SIZE];

	/* first dequeue packets from ring of primary process */
	const uint16_t nb_in_deq = rte_ring_dequeue_burst(ring,
			(void *)rxtx_bufs, BURST_SIZE);
	stats->dequeue_pkts += nb_in_deq;

	if (nb_in_deq) {
		/* then sent on vdev */
		uint16_t nb_in_txd = rte_eth_tx_burst(
				vdev_id,
				0, rxtx_bufs, nb_in_deq);
		stats->tx_pkts += nb_in_txd;

		if (unlikely(nb_in_txd < nb_in_deq)) {
			do {
				rte_pktmbuf_free(rxtx_bufs[nb_in_txd]);
				stats->freed_pkts++;
			} while (++nb_in_txd < nb_in_deq);
		}
	}
}

static void
free_ring_data(struct rte_ring *ring, uint8_t vdev_id,
		struct pdump_stats *stats)
{
	while (rte_ring_count(ring))
		pdump_rxtx(ring, vdev_id, stats);
}

static void
cleanup_rings(void)
{
	int i;
	struct pdump_tuples *pt;

	for (i = 0; i < num_tuples; i++) {
		pt = &pdump_t[i];

		if (pt->device_id)
			free(pt->device_id);

		/* free the rings */
		if (pt->rx_ring)
			rte_ring_free(pt->rx_ring);
		if (pt->tx_ring)
			rte_ring_free(pt->tx_ring);
	}
}

static void
cleanup_pdump_resources(void)
{
	int i;
	struct pdump_tuples *pt;

	/* disable pdump and free the pdump_tuple resources */
	for (i = 0; i < num_tuples; i++) {
		pt = &pdump_t[i];

		/* remove callbacks */
		disable_pdump(pt);

		/*
		* transmit rest of the enqueued packets of the rings on to
		* the vdev, in order to release mbufs to the mepool.
		**/
		if (pt->dir & RTE_PDUMP_FLAG_RX)
			free_ring_data(pt->rx_ring, pt->rx_vdev_id, &pt->stats);
		if (pt->dir & RTE_PDUMP_FLAG_TX)
			free_ring_data(pt->tx_ring, pt->tx_vdev_id, &pt->stats);
	}
	cleanup_rings();
}

static void
signal_handler(int sig_num)
{
	if (sig_num == SIGINT) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				sig_num);
		quit_signal = 1;
	}
}

static inline int
configure_vdev(uint8_t port_id)
{
	struct ether_addr addr;
	const uint16_t rxRings = 0, txRings = 1;
	const uint8_t nb_ports = rte_eth_dev_count();
	int ret;
	uint16_t q;

	if (port_id > nb_ports)
		return -1;

	ret = rte_eth_dev_configure(port_id, rxRings, txRings,
					&port_conf_default);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "dev config failed\n");

	 for (q = 0; q < txRings; q++) {
		ret = rte_eth_tx_queue_setup(port_id, q, TX_DESC_PER_QUEUE,
				rte_eth_dev_socket_id(port_id), NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "queue setup failed\n");
	}

	ret = rte_eth_dev_start(port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "dev start failed\n");

	rte_eth_macaddr_get(port_id, &addr);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port_id,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	rte_eth_promiscuous_enable(port_id);

	return 0;
}

static void
create_mp_ring_vdev(void)
{
	int i;
	uint8_t portid;
	struct pdump_tuples *pt = NULL;
	struct rte_mempool *mbuf_pool = NULL;
	char vdev_args[SIZE];
	char ring_name[SIZE];
	char mempool_name[SIZE];

	for (i = 0; i < num_tuples; i++) {
		pt = &pdump_t[i];
		snprintf(mempool_name, SIZE, MP_NAME, i);
		mbuf_pool = rte_mempool_lookup(mempool_name);
		if (mbuf_pool == NULL) {
			/* create mempool */
			mbuf_pool = rte_pktmbuf_pool_create(mempool_name,
					pt->total_num_mbufs,
					MBUF_POOL_CACHE_SIZE, 0,
					pt->mbuf_data_size,
					rte_socket_id());
			if (mbuf_pool == NULL) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"Mempool creation failed: %s\n",
					rte_strerror(rte_errno));
			}
		}
		pt->mp = mbuf_pool;

		if (pt->dir == RTE_PDUMP_FLAG_RXTX) {
			/* if captured packets has to send to the same vdev */
			/* create rx_ring */
			snprintf(ring_name, SIZE, RX_RING, i);
			pt->rx_ring = rte_ring_create(ring_name, pt->ring_size,
					rte_socket_id(), 0);
			if (pt->rx_ring == NULL) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE, "%s:%s:%d\n",
						rte_strerror(rte_errno),
						__func__, __LINE__);
			}

			/* create tx_ring */
			snprintf(ring_name, SIZE, TX_RING, i);
			pt->tx_ring = rte_ring_create(ring_name, pt->ring_size,
					rte_socket_id(), 0);
			if (pt->tx_ring == NULL) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE, "%s:%s:%d\n",
						rte_strerror(rte_errno),
						__func__, __LINE__);
			}

			/* create vdevs */
			(pt->rx_vdev_stream_type == IFACE) ?
			snprintf(vdev_args, SIZE, VDEV_IFACE, RX_STR, i,
			pt->rx_dev) :
			snprintf(vdev_args, SIZE, VDEV_PCAP, RX_STR, i,
			pt->rx_dev);
			if (rte_eth_dev_attach(vdev_args, &portid) < 0) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"vdev creation failed:%s:%d\n",
					__func__, __LINE__);
			}
			pt->rx_vdev_id = portid;

			/* configure vdev */
			configure_vdev(pt->rx_vdev_id);

			if (pt->single_pdump_dev)
				pt->tx_vdev_id = portid;
			else {
				(pt->tx_vdev_stream_type == IFACE) ?
				snprintf(vdev_args, SIZE, VDEV_IFACE, TX_STR, i,
				pt->tx_dev) :
				snprintf(vdev_args, SIZE, VDEV_PCAP, TX_STR, i,
				pt->tx_dev);
				if (rte_eth_dev_attach(vdev_args,
							&portid) < 0) {
					cleanup_rings();
					rte_exit(EXIT_FAILURE,
						"vdev creation failed:"
						"%s:%d\n", __func__, __LINE__);
				}
				pt->tx_vdev_id = portid;

				/* configure vdev */
				configure_vdev(pt->tx_vdev_id);
			}
		} else if (pt->dir == RTE_PDUMP_FLAG_RX) {

			/* create rx_ring */
			snprintf(ring_name, SIZE, RX_RING, i);
			pt->rx_ring = rte_ring_create(ring_name, pt->ring_size,
					rte_socket_id(), 0);
			if (pt->rx_ring == NULL) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE, "%s\n",
					rte_strerror(rte_errno));
			}

			(pt->rx_vdev_stream_type == IFACE) ?
			snprintf(vdev_args, SIZE, VDEV_IFACE, RX_STR, i,
				pt->rx_dev) :
			snprintf(vdev_args, SIZE, VDEV_PCAP, RX_STR, i,
				pt->rx_dev);
			if (rte_eth_dev_attach(vdev_args, &portid) < 0) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"vdev creation failed:%s:%d\n",
					__func__, __LINE__);
			}
			pt->rx_vdev_id = portid;
			/* configure vdev */
			configure_vdev(pt->rx_vdev_id);
		} else if (pt->dir == RTE_PDUMP_FLAG_TX) {

			/* create tx_ring */
			snprintf(ring_name, SIZE, TX_RING, i);
			pt->tx_ring = rte_ring_create(ring_name, pt->ring_size,
					rte_socket_id(), 0);
			if (pt->tx_ring == NULL) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE, "%s\n",
					rte_strerror(rte_errno));
			}

			(pt->tx_vdev_stream_type == IFACE) ?
			snprintf(vdev_args, SIZE, VDEV_IFACE, TX_STR, i,
				pt->tx_dev) :
			snprintf(vdev_args, SIZE, VDEV_PCAP, TX_STR, i,
				pt->tx_dev);
			if (rte_eth_dev_attach(vdev_args, &portid) < 0) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"vdev creation failed\n");
			}
			pt->tx_vdev_id = portid;

			/* configure vdev */
			configure_vdev(pt->tx_vdev_id);
		}
	}
}

static void
enable_pdump(void)
{
	int i;
	struct pdump_tuples *pt;
	int ret = 0, ret1 = 0;

	if (server_socket_path[0] != 0)
		ret = rte_pdump_set_socket_dir(server_socket_path,
				RTE_PDUMP_SOCKET_SERVER);
	if (ret == 0 && client_socket_path[0] != 0) {
		ret = rte_pdump_set_socket_dir(client_socket_path,
				RTE_PDUMP_SOCKET_CLIENT);
	}
	if (ret < 0) {
		cleanup_pdump_resources();
		rte_exit(EXIT_FAILURE,
				"failed to set socket paths of server:%s, "
				"client:%s\n",
				server_socket_path,
				client_socket_path);
	}

	for (i = 0; i < num_tuples; i++) {
		pt = &pdump_t[i];
		if (pt->dir == RTE_PDUMP_FLAG_RXTX) {
			if (pt->dump_by_type == DEVICE_ID) {
				ret = rte_pdump_enable_by_deviceid(
						pt->device_id,
						pt->queue,
						RTE_PDUMP_FLAG_RX,
						pt->rx_ring,
						pt->mp, NULL);
				ret1 = rte_pdump_enable_by_deviceid(
						pt->device_id,
						pt->queue,
						RTE_PDUMP_FLAG_TX,
						pt->tx_ring,
						pt->mp, NULL);
			} else if (pt->dump_by_type == PORT_ID) {
				ret = rte_pdump_enable(pt->port, pt->queue,
						RTE_PDUMP_FLAG_RX,
						pt->rx_ring, pt->mp, NULL);
				ret1 = rte_pdump_enable(pt->port, pt->queue,
						RTE_PDUMP_FLAG_TX,
						pt->tx_ring, pt->mp, NULL);
			}
		} else if (pt->dir == RTE_PDUMP_FLAG_RX) {
			if (pt->dump_by_type == DEVICE_ID)
				ret = rte_pdump_enable_by_deviceid(
						pt->device_id,
						pt->queue,
						pt->dir, pt->rx_ring,
						pt->mp, NULL);
			else if (pt->dump_by_type == PORT_ID)
				ret = rte_pdump_enable(pt->port, pt->queue,
						pt->dir,
						pt->rx_ring, pt->mp, NULL);
		} else if (pt->dir == RTE_PDUMP_FLAG_TX) {
			if (pt->dump_by_type == DEVICE_ID)
				ret = rte_pdump_enable_by_deviceid(
						pt->device_id,
						pt->queue,
						pt->dir,
						pt->tx_ring, pt->mp, NULL);
			else if (pt->dump_by_type == PORT_ID)
				ret = rte_pdump_enable(pt->port, pt->queue,
						pt->dir,
						pt->tx_ring, pt->mp, NULL);
		}
		if (ret < 0 || ret1 < 0) {
			cleanup_pdump_resources();
			rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));
		}
	}
}

static inline void
dump_packets(void)
{
	int i;
	struct pdump_tuples *pt;

	while (!quit_signal) {
		for (i = 0; i < num_tuples; i++) {
			pt = &pdump_t[i];
			if (pt->dir & RTE_PDUMP_FLAG_RX)
				pdump_rxtx(pt->rx_ring, pt->rx_vdev_id,
					&pt->stats);
			if (pt->dir & RTE_PDUMP_FLAG_TX)
				pdump_rxtx(pt->tx_ring, pt->tx_vdev_id,
					&pt->stats);
		}
	}
}

int
main(int argc, char **argv)
{
	int diag;
	int ret;
	int i;

	char c_flag[] = "-c1";
	char n_flag[] = "-n4";
	char mp_flag[] = "--proc-type=secondary";
	char *argp[argc + 3];

	/* catch ctrl-c so we can print on exit */
	signal(SIGINT, signal_handler);

	argp[0] = argv[0];
	argp[1] = c_flag;
	argp[2] = n_flag;
	argp[3] = mp_flag;

	for (i = 1; i < argc; i++)
		argp[i + 3] = argv[i];

	argc += 3;

	diag = rte_eal_init(argc, argp);
	if (diag < 0)
		rte_panic("Cannot init EAL\n");

	argc -= diag;
	argv += (diag - 3);

	/* parse app arguments */
	if (argc > 1) {
		ret = launch_args_parse(argc, argv, argp[0]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Invalid argument\n");
	}

	/* create mempool, ring and vdevs info */
	create_mp_ring_vdev();
	enable_pdump();
	dump_packets();

	cleanup_pdump_resources();
	/* dump debug stats */
	print_pdump_stats();

	return 0;
}
