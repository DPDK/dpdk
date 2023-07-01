/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
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
#include <rte_alarm.h>
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
#include <rte_string_fns.h>
#include <rte_pdump.h>

#define CMD_LINE_OPT_PDUMP "pdump"
#define CMD_LINE_OPT_PDUMP_NUM 256
#define CMD_LINE_OPT_MULTI "multi"
#define CMD_LINE_OPT_MULTI_NUM 257
#define PDUMP_PORT_ARG "port"
#define PDUMP_PCI_ARG "device_id"
#define PDUMP_QUEUE_ARG "queue"
#define PDUMP_DIR_ARG "dir"
#define PDUMP_RX_DEV_ARG "rx-dev"
#define PDUMP_TX_DEV_ARG "tx-dev"
#define PDUMP_RING_SIZE_ARG "ring-size"
#define PDUMP_MSIZE_ARG "mbuf-size"
#define PDUMP_NUM_MBUFS_ARG "total-num-mbufs"

#define VDEV_NAME_FMT "net_pcap_%s_%d"
#define VDEV_PCAP_ARGS_FMT "tx_pcap=%s"
#define VDEV_IFACE_ARGS_FMT "tx_iface=%s"
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
/* Maximum delay for exiting after primary process. */
#define MONITOR_INTERVAL (500 * 1000)

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

static const char * const valid_pdump_arguments[] = {
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
	uint16_t port;
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
	uint16_t rx_vdev_id;
	uint16_t tx_vdev_id;
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

static int num_tuples;
static struct rte_eth_conf port_conf_default;
static volatile uint8_t quit_signal;
static uint8_t multiple_core_capture;

/**< display usage */
static void
pdump_usage(const char *prgname)
{
	printf("usage: %s [EAL options] --"
			" --["CMD_LINE_OPT_MULTI"]\n"
			" --"CMD_LINE_OPT_PDUMP" "
			"'(port=<port id> | device_id=<pci id or vdev name>),"
			"(queue=<queue_id>),"
			"(rx-dev=<iface or pcap file> |"
			" tx-dev=<iface or pcap file>,"
			"[ring-size=<ring size>default:16384],"
			"[mbuf-size=<mbuf data size>default:2176],"
			"[total-num-mbufs=<number of mbufs>default:65535]'\n",
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
		strlcpy(pt->rx_dev, value, sizeof(pt->rx_dev));
		/* identify the tx stream type for pcap vdev */
		if (if_nametoindex(pt->rx_dev))
			pt->rx_vdev_stream_type = IFACE;
	} else if (!strcmp(key, PDUMP_TX_DEV_ARG)) {
		strlcpy(pt->tx_dev, value, sizeof(pt->tx_dev));
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
		pt->port = (uint16_t) v.val;
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
		{CMD_LINE_OPT_PDUMP, 1, 0, CMD_LINE_OPT_PDUMP_NUM},
		{CMD_LINE_OPT_MULTI, 0, 0, CMD_LINE_OPT_MULTI_NUM},
		{NULL, 0, 0, 0}
	};

	if (argc == 1)
		pdump_usage(prgname);

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, " ",
			long_option, &option_index)) != EOF) {
		switch (opt) {
		case CMD_LINE_OPT_PDUMP_NUM:
			ret = parse_pdump(optarg);
			if (ret) {
				pdump_usage(prgname);
				return -1;
			}
			break;
		case CMD_LINE_OPT_MULTI_NUM:
			multiple_core_capture = 1;
			break;
		default:
			pdump_usage(prgname);
			return -1;
		}
	}

	return 0;
}

static void
monitor_primary(void *arg __rte_unused)
{
	if (quit_signal)
		return;

	if (rte_eal_primary_proc_alive(NULL)) {
		rte_eal_alarm_set(MONITOR_INTERVAL, monitor_primary, NULL);
		return;
	}

	printf("Primary process is no longer active, exiting...\n");
	quit_signal = 1;
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
pdump_rxtx(struct rte_ring *ring, uint16_t vdev_id, struct pdump_stats *stats)
{
	/* write input packets of port to vdev for pdump */
	struct rte_mbuf *rxtx_bufs[BURST_SIZE];

	/* first dequeue packets from ring of primary process */
	const uint16_t nb_in_deq = rte_ring_dequeue_burst(ring,
			(void *)rxtx_bufs, BURST_SIZE, NULL);
	stats->dequeue_pkts += nb_in_deq;

	if (nb_in_deq) {
		/* then sent on vdev */
		uint16_t nb_in_txd = rte_eth_tx_burst(
				vdev_id,
				0, rxtx_bufs, nb_in_deq);
		stats->tx_pkts += nb_in_txd;

		if (unlikely(nb_in_txd < nb_in_deq)) {
			unsigned int drops = nb_in_deq - nb_in_txd;

			rte_pktmbuf_free_bulk(&rxtx_bufs[nb_in_txd], drops);
			stats->freed_pkts += drops;
		}
	}
}

static void
free_ring_data(struct rte_ring *ring, uint16_t vdev_id,
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

		free(pt->device_id);

		/* free the rings */
		rte_ring_free(pt->rx_ring);
		rte_ring_free(pt->tx_ring);
		rte_mempool_free(pt->mp);
	}
}

static void
cleanup_pdump_resources(void)
{
	int i;
	struct pdump_tuples *pt;
	char name[RTE_ETH_NAME_MAX_LEN];

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

		/* Remove the vdev(s) created */
		if (pt->dir & RTE_PDUMP_FLAG_RX) {
			rte_eth_dev_get_name_by_port(pt->rx_vdev_id, name);
			rte_eal_hotplug_remove("vdev", name);
		}

		if (pt->single_pdump_dev)
			continue;

		if (pt->dir & RTE_PDUMP_FLAG_TX) {
			rte_eth_dev_get_name_by_port(pt->tx_vdev_id, name);
			rte_eal_hotplug_remove("vdev", name);
		}

	}
	cleanup_rings();
}

static void
disable_primary_monitor(void)
{
	int ret;

	/*
	 * Cancel monitoring of primary process.
	 * There will be no error if no alarm is set
	 * (in case primary process kill was detected earlier).
	 */
	ret = rte_eal_alarm_cancel(monitor_primary, NULL);
	if (ret < 0)
		printf("Fail to disable monitor:%d\n", ret);
}

static void
signal_handler(int sig_num)
{
	if (sig_num == SIGINT) {
		quit_signal = 1;
	}
}

static inline int
configure_vdev(uint16_t port_id)
{
	struct rte_ether_addr addr;
	const uint16_t rxRings = 0, txRings = 1;
	int ret;
	uint16_t q;

	if (!rte_eth_dev_is_valid_port(port_id))
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

	ret = rte_eth_macaddr_get(port_id, &addr);
	if (ret != 0)
		rte_exit(EXIT_FAILURE, "macaddr get failed\n");

	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			port_id, RTE_ETHER_ADDR_BYTES(&addr));

	ret = rte_eth_promiscuous_enable(port_id);
	if (ret != 0) {
		rte_exit(EXIT_FAILURE,
			 "promiscuous mode enable failed: %s\n",
			 rte_strerror(-ret));
		return ret;
	}

	return 0;
}

static void
create_mp_ring_vdev(void)
{
	int i;
	uint16_t portid;
	struct pdump_tuples *pt = NULL;
	struct rte_mempool *mbuf_pool = NULL;
	char vdev_name[SIZE];
	char vdev_args[SIZE];
	char ring_name[SIZE];
	char mempool_name[SIZE];

	for (i = 0; i < num_tuples; i++) {
		pt = &pdump_t[i];
		snprintf(mempool_name, SIZE, MP_NAME, i);
		mbuf_pool = rte_mempool_lookup(mempool_name);
		if (mbuf_pool == NULL) {
			/* create mempool */
			mbuf_pool = rte_pktmbuf_pool_create_by_ops(mempool_name,
					pt->total_num_mbufs,
					MBUF_POOL_CACHE_SIZE, 0,
					pt->mbuf_data_size,
					rte_socket_id(), "ring_mp_mc");
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
			snprintf(vdev_name, sizeof(vdev_name),
				 VDEV_NAME_FMT, RX_STR, i);
			(pt->rx_vdev_stream_type == IFACE) ?
			snprintf(vdev_args, sizeof(vdev_args),
				 VDEV_IFACE_ARGS_FMT, pt->rx_dev) :
			snprintf(vdev_args, sizeof(vdev_args),
				 VDEV_PCAP_ARGS_FMT, pt->rx_dev);
			if (rte_eal_hotplug_add("vdev", vdev_name,
						vdev_args) < 0) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"vdev creation failed:%s:%d\n",
					__func__, __LINE__);
			}
			if (rte_eth_dev_get_port_by_name(vdev_name,
							 &portid) != 0) {
				rte_eal_hotplug_remove("vdev", vdev_name);
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"cannot find added vdev %s:%s:%d\n",
					vdev_name, __func__, __LINE__);
			}
			pt->rx_vdev_id = portid;

			/* configure vdev */
			configure_vdev(pt->rx_vdev_id);

			if (pt->single_pdump_dev)
				pt->tx_vdev_id = portid;
			else {
				snprintf(vdev_name, sizeof(vdev_name),
					 VDEV_NAME_FMT, TX_STR, i);
				(pt->rx_vdev_stream_type == IFACE) ?
				snprintf(vdev_args, sizeof(vdev_args),
					 VDEV_IFACE_ARGS_FMT, pt->tx_dev) :
				snprintf(vdev_args, sizeof(vdev_args),
					 VDEV_PCAP_ARGS_FMT, pt->tx_dev);
				if (rte_eal_hotplug_add("vdev", vdev_name,
							vdev_args) < 0) {
					cleanup_rings();
					rte_exit(EXIT_FAILURE,
						"vdev creation failed:"
						"%s:%d\n", __func__, __LINE__);
				}
				if (rte_eth_dev_get_port_by_name(vdev_name,
						&portid) != 0) {
					rte_eal_hotplug_remove("vdev",
							       vdev_name);
					cleanup_rings();
					rte_exit(EXIT_FAILURE,
						"cannot find added vdev %s:%s:%d\n",
						vdev_name, __func__, __LINE__);
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

			snprintf(vdev_name, sizeof(vdev_name),
				 VDEV_NAME_FMT, RX_STR, i);
			(pt->rx_vdev_stream_type == IFACE) ?
			snprintf(vdev_args, sizeof(vdev_args),
				 VDEV_IFACE_ARGS_FMT, pt->rx_dev) :
			snprintf(vdev_args, sizeof(vdev_args),
				 VDEV_PCAP_ARGS_FMT, pt->rx_dev);
			if (rte_eal_hotplug_add("vdev", vdev_name,
						vdev_args) < 0) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"vdev creation failed:%s:%d\n",
					__func__, __LINE__);
			}
			if (rte_eth_dev_get_port_by_name(vdev_name,
							 &portid) != 0) {
				rte_eal_hotplug_remove("vdev", vdev_name);
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"cannot find added vdev %s:%s:%d\n",
					vdev_name, __func__, __LINE__);
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

			snprintf(vdev_name, sizeof(vdev_name),
				 VDEV_NAME_FMT, TX_STR, i);
			(pt->tx_vdev_stream_type == IFACE) ?
			snprintf(vdev_args, sizeof(vdev_args),
				 VDEV_IFACE_ARGS_FMT, pt->tx_dev) :
			snprintf(vdev_args, sizeof(vdev_args),
				 VDEV_PCAP_ARGS_FMT, pt->tx_dev);
			if (rte_eal_hotplug_add("vdev", vdev_name,
						vdev_args) < 0) {
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"vdev creation failed\n");
			}
			if (rte_eth_dev_get_port_by_name(vdev_name,
							 &portid) != 0) {
				rte_eal_hotplug_remove("vdev", vdev_name);
				cleanup_rings();
				rte_exit(EXIT_FAILURE,
					"cannot find added vdev %s:%s:%d\n",
					vdev_name, __func__, __LINE__);
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
pdump_packets(struct pdump_tuples *pt)
{
	if (pt->dir & RTE_PDUMP_FLAG_RX)
		pdump_rxtx(pt->rx_ring, pt->rx_vdev_id, &pt->stats);
	if (pt->dir & RTE_PDUMP_FLAG_TX)
		pdump_rxtx(pt->tx_ring, pt->tx_vdev_id, &pt->stats);
}

static int
dump_packets_core(void *arg)
{
	struct pdump_tuples *pt = (struct pdump_tuples *) arg;

	printf(" core (%u); port %u device (%s) queue %u\n",
			rte_lcore_id(), pt->port, pt->device_id, pt->queue);
	fflush(stdout);

	while (!quit_signal)
		pdump_packets(pt);

	return 0;
}

static unsigned int
get_next_core(unsigned int lcore)
{
	lcore = rte_get_next_lcore(lcore, 1, 0);
	if (lcore == RTE_MAX_LCORE)
		rte_exit(EXIT_FAILURE,
				"Max core limit %u reached for packet capture", lcore);
	return lcore;
}

static inline void
dump_packets(void)
{
	int i;
	unsigned int lcore_id = 0;

	if (num_tuples == 0)
		rte_exit(EXIT_FAILURE, "No device specified for capture\n");

	if (!multiple_core_capture) {
		printf(" core (%u), capture for (%d) tuples\n",
				rte_lcore_id(), num_tuples);

		for (i = 0; i < num_tuples; i++)
			printf(" - port %u device (%s) queue %u\n",
				pdump_t[i].port,
				pdump_t[i].device_id,
				pdump_t[i].queue);

		while (!quit_signal) {
			for (i = 0; i < num_tuples; i++)
				pdump_packets(&pdump_t[i]);
		}

		return;
	}

	/* check if there enough core */
	if ((uint32_t)num_tuples >= rte_lcore_count()) {
		printf("Insufficient cores to run parallel!\n");
		return;
	}

	lcore_id = get_next_core(lcore_id);

	for (i = 0; i < num_tuples; i++) {
		rte_eal_remote_launch(dump_packets_core,
				&pdump_t[i], lcore_id);
		lcore_id = get_next_core(lcore_id);

		if (rte_eal_wait_lcore(lcore_id) < 0)
			rte_exit(EXIT_FAILURE, "failed to wait\n");
	}

	/* main core */
	while (!quit_signal)
		;
}

static void
enable_primary_monitor(void)
{
	int ret;

	/* Once primary exits, so will pdump. */
	ret = rte_eal_alarm_set(MONITOR_INTERVAL, monitor_primary, NULL);
	if (ret < 0)
		printf("Fail to enable monitor:%d\n", ret);
}

int
main(int argc, char **argv)
{
	int diag;
	int ret;
	int i;

	char n_flag[] = "-n4";
	char mp_flag[] = "--proc-type=secondary";
	char *argp[argc + 2];

	/* catch ctrl-c so we can print on exit */
	signal(SIGINT, signal_handler);

	argp[0] = argv[0];
	argp[1] = n_flag;
	argp[2] = mp_flag;

	for (i = 1; i < argc; i++)
		argp[i + 2] = argv[i];

	argc += 2;

	diag = rte_eal_init(argc, argp);
	if (diag < 0)
		rte_panic("Cannot init EAL\n");

	if (rte_eth_dev_count_avail() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	argc -= diag;
	argv += (diag - 2);

	/* parse app arguments */
	if (argc > 1) {
		ret = launch_args_parse(argc, argv, argp[0]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Invalid argument\n");
	}

	/* create mempool, ring and vdevs info */
	create_mp_ring_vdev();
	enable_pdump();
	enable_primary_monitor();
	dump_packets();

	disable_primary_monitor();
	cleanup_pdump_resources();
	/* dump debug stats */
	print_pdump_stats();

	ret = rte_eal_cleanup();
	if (ret)
		printf("Error from rte_eal_cleanup(), %d\n", ret);

	return 0;
}
