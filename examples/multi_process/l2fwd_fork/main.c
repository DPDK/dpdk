/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "flib.h"

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1
#define MBUF_NAME	"mbuf_pool_%d"
#define MBUF_DATA_SIZE	RTE_MBUF_DEFAULT_BUF_SIZE
#define NB_MBUF   8192
#define RING_MASTER_NAME	"l2fwd_ring_m2s_"
#define RING_SLAVE_NAME		"l2fwd_ring_s2m_"
#define MAX_NAME_LEN	32
/* RECREATE flag indicate needs initialize resource and launch slave_core again */
#define SLAVE_RECREATE_FLAG 0x1
/* RESTART flag indicate needs restart port and send START command again */
#define SLAVE_RESTART_FLAG 0x2
#define INVALID_MAPPING_ID	((unsigned)LCORE_ID_ANY)
/* Maximum message buffer per slave */
#define NB_CORE_MSGBUF	32
enum l2fwd_cmd{
	CMD_START,
	CMD_STOP,
};

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int l2fwd_rx_queue_per_lcore = 1;

struct mbuf_table {
	unsigned len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

struct lcore_resource_struct {
	int enabled;	/* Only set in case this lcore involved into packet forwarding */
	int flags; 	    /* Set only slave need to restart or recreate */
	unsigned lcore_id;  /*  lcore ID */
	unsigned pair_id; 	/* dependency lcore ID on port */
	char ring_name[2][MAX_NAME_LEN];
	/* ring[0] for master send cmd, slave read */
	/* ring[1] for slave send ack, master read */
	struct rte_ring *ring[2];
	int port_num;					/* Total port numbers */
	uint8_t port[RTE_MAX_ETHPORTS]; /* Port id for that lcore to receive packets */
}__attribute__((packed)) __rte_cache_aligned;

static struct lcore_resource_struct lcore_resource[RTE_MAX_LCORE];
static struct rte_mempool *message_pool;
static rte_spinlock_t res_lock = RTE_SPINLOCK_INITIALIZER;
/* use floating processes */
static int float_proc = 0;
/* Save original cpu affinity */
struct cpu_aff_arg{
	cpu_set_t set;
	size_t size;
}cpu_aff;

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static struct rte_mempool * l2fwd_pktmbuf_pool[RTE_MAX_ETHPORTS];

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics *port_statistics;
/**
 * pointer to lcore ID mapping array, used to return lcore id in case slave
 * process exited unexpectedly, use only floating process option applied
 **/
unsigned *mapping_id;

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000; /* default period is 10 seconds */

static int l2fwd_launch_one_lcore(void *dummy);

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
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

static int
clear_cpu_affinity(void)
{
	int s;

	s = sched_setaffinity(0, cpu_aff.size, &cpu_aff.set);
	if (s != 0) {
		printf("sched_setaffinity failed:%s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int
get_cpu_affinity(void)
{
	int s;

	cpu_aff.size = sizeof(cpu_set_t);
	CPU_ZERO(&cpu_aff.set);

	s = sched_getaffinity(0, cpu_aff.size, &cpu_aff.set);
	if (s != 0) {
		printf("sched_getaffinity failed:%s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * This fnciton demonstrates the approach to create ring in first instance
 * or re-attach an existed ring in later instance.
 **/
static struct rte_ring *
create_ring(const char *name, unsigned count,
					int socket_id,unsigned flags)
{
	struct rte_ring *ring;

	if (name == NULL)
		return NULL;

	/* If already create, just attached it */
	if (likely((ring = rte_ring_lookup(name)) != NULL))
		return ring;

	/* First call it, create one */
	return rte_ring_create(name, count, socket_id, flags);
}

/* Malloc with rte_malloc on structures that shared by master and slave */
static int
l2fwd_malloc_shared_struct(void)
{
	port_statistics = rte_zmalloc("port_stat",
						sizeof(struct l2fwd_port_statistics) * RTE_MAX_ETHPORTS,
						0);
	if (port_statistics == NULL)
		return -1;

	/* allocate  mapping_id array */
	if (float_proc) {
		int i;
		mapping_id = rte_malloc("mapping_id", sizeof(unsigned) * RTE_MAX_LCORE,
								0);

		if (mapping_id == NULL)
			return -1;

		for (i = 0 ;i < RTE_MAX_LCORE; i++)
			mapping_id[i] = INVALID_MAPPING_ID;
	}
	return 0;
}

/* Create ring which used for communicate among master and slave */
static int
create_ms_ring(unsigned slaveid)
{
	unsigned flag = RING_F_SP_ENQ | RING_F_SC_DEQ;
	struct lcore_resource_struct *res = &lcore_resource[slaveid];
	unsigned socketid = rte_socket_id();

	/* Always assume create ring on master socket_id */
	/* Default only create a ring size 32 */
	snprintf(res->ring_name[0], MAX_NAME_LEN, "%s%u",
			RING_MASTER_NAME, slaveid);
	if ((res->ring[0] = create_ring(res->ring_name[0], NB_CORE_MSGBUF,
				socketid, flag)) == NULL) {
		printf("Create m2s ring %s failed\n", res->ring_name[0]);
		return -1;
	}

	snprintf(res->ring_name[1], MAX_NAME_LEN, "%s%u",
			RING_SLAVE_NAME, slaveid);
	if ((res->ring[1] = create_ring(res->ring_name[1], NB_CORE_MSGBUF,
		socketid, flag)) == NULL) {
		printf("Create s2m ring %s failed\n", res->ring_name[1]);
		return -1;
	}

	return 0;
}

/* send command to pair in paired master and slave ring */
static inline int
sendcmd(unsigned slaveid, enum l2fwd_cmd cmd, int is_master)
{
	struct lcore_resource_struct *res = &lcore_resource[slaveid];
	void *msg;
	int fd = !is_master;

	/* Only check master, it must be enabled and running if it is slave */
	if (is_master && !res->enabled)
		return -1;

	if (res->ring[fd] == NULL)
		return -1;

	if (rte_mempool_get(message_pool, &msg) < 0) {
		printf("Error to get message buffer\n");
		return -1;
	}

	*(enum l2fwd_cmd *)msg = cmd;

	if (rte_ring_enqueue(res->ring[fd], msg) != 0) {
		printf("Enqueue error\n");
		rte_mempool_put(message_pool, msg);
		return -1;
	}

	return 0;
}

/* Get command from pair in paired master and slave ring */
static inline int
getcmd(unsigned slaveid, enum l2fwd_cmd *cmd, int is_master)
{
	struct lcore_resource_struct *res = &lcore_resource[slaveid];
	void *msg;
	int fd = !!is_master;
	int ret;
	/* Only check master, it must be enabled and running if it is slave */
	if (is_master && (!res->enabled))
		return -1;

	if (res->ring[fd] == NULL)
		return -1;

	ret = rte_ring_dequeue(res->ring[fd], &msg);

	if (ret == 0) {
		*cmd = *(enum l2fwd_cmd *)msg;
		rte_mempool_put(message_pool, msg);
	}
	return ret;
}

/* Master send command to slave and wait until ack received or error met */
static int
master_sendcmd_with_ack(unsigned slaveid, enum l2fwd_cmd cmd)
{
	enum l2fwd_cmd ack_cmd;
	int ret = -1;

	if (sendcmd(slaveid, cmd, 1) != 0)
		rte_exit(EXIT_FAILURE, "Failed to send message\n");

	/* Get ack */
	while (1) {
		ret = getcmd(slaveid, &ack_cmd, 1);
		if (ret == 0 && cmd == ack_cmd)
			break;

		/* If slave not running yet, return an error */
		if (flib_query_slave_status(slaveid) != ST_RUN) {
			ret = -ENOENT;
			break;
		}
	}

	return ret;
}

/* restart all port that assigned to that slave lcore */
static int
reset_slave_all_ports(unsigned slaveid)
{
	struct lcore_resource_struct *slave = &lcore_resource[slaveid];
	int i, ret = 0;

	/* stop/start port */
	for (i = 0; i < slave->port_num; i++) {
		char buf_name[RTE_MEMPOOL_NAMESIZE];
		struct rte_mempool *pool;
		printf("Stop port :%d\n", slave->port[i]);
		rte_eth_dev_stop(slave->port[i]);
		snprintf(buf_name, RTE_MEMPOOL_NAMESIZE, MBUF_NAME, slave->port[i]);
		pool = rte_mempool_lookup(buf_name);
		if (pool)
			printf("Port %d mempool free object is %u(%u)\n", slave->port[i],
				rte_mempool_avail_count(pool),
				(unsigned int)NB_MBUF);
		else
			printf("Can't find mempool %s\n", buf_name);

		printf("Start port :%d\n", slave->port[i]);
		ret = rte_eth_dev_start(slave->port[i]);
		if (ret != 0)
			break;
	}
	return ret;
}

static int
reset_shared_structures(unsigned slaveid)
{
	int ret;
	/* Only port are shared resource here */
	ret = reset_slave_all_ports(slaveid);

	return ret;
}

/**
 * Call this function to re-create resource that needed for slave process that
 * exited in last instance
 **/
static int
init_slave_res(unsigned slaveid)
{
	struct lcore_resource_struct *slave = &lcore_resource[slaveid];
	enum l2fwd_cmd cmd;

	if (!slave->enabled) {
		printf("Something wrong with lcore=%u enabled=%d\n",slaveid,
			slave->enabled);
		return -1;
	}

	/* Initialize ring */
	if (create_ms_ring(slaveid) != 0)
		rte_exit(EXIT_FAILURE, "failed to create ring for slave %u\n",
				slaveid);

	/* drain un-read buffer if have */
	while (getcmd(slaveid, &cmd, 1) == 0);
	while (getcmd(slaveid, &cmd, 0) == 0);

	return 0;
}

static int
recreate_one_slave(unsigned slaveid)
{
	int ret = 0;
	/* Re-initialize resource for stalled slave */
	if ((ret = init_slave_res(slaveid)) != 0) {
		printf("Init slave=%u failed\n", slaveid);
		return ret;
	}

	if ((ret = flib_remote_launch(l2fwd_launch_one_lcore, NULL, slaveid))
		!= 0)
		printf("Launch slave %u failed\n", slaveid);

	return ret;
}

/**
 * remapping resource belong to slave_id to new lcore that gets from flib_assign_lcore_id(),
 * used only floating process option applied.
 *
 * @param slaveid
 *   original lcore_id that apply for remapping
 */
static void
remapping_slave_resource(unsigned slaveid, unsigned map_id)
{

	/* remapping lcore_resource */
	memcpy(&lcore_resource[map_id], &lcore_resource[slaveid],
			sizeof(struct lcore_resource_struct));

	/* remapping lcore_queue_conf */
	memcpy(&lcore_queue_conf[map_id], &lcore_queue_conf[slaveid],
			sizeof(struct lcore_queue_conf));
}

static int
reset_pair(unsigned slaveid, unsigned pairid)
{
	int ret;
	if ((ret = reset_shared_structures(slaveid)) != 0)
		goto back;

	if((ret = reset_shared_structures(pairid)) != 0)
		goto back;

	if (float_proc) {
		unsigned map_id = mapping_id[slaveid];

		if (map_id != INVALID_MAPPING_ID) {
			printf("%u return mapping id %u\n", slaveid, map_id);
			flib_free_lcore_id(map_id);
			mapping_id[slaveid] = INVALID_MAPPING_ID;
		}

		map_id = mapping_id[pairid];
		if (map_id != INVALID_MAPPING_ID) {
			printf("%u return mapping id %u\n", pairid, map_id);
			flib_free_lcore_id(map_id);
			mapping_id[pairid] = INVALID_MAPPING_ID;
		}
	}

	if((ret = recreate_one_slave(slaveid)) != 0)
		goto back;

	ret = recreate_one_slave(pairid);

back:
	return ret;
}

static void
slave_exit_cb(unsigned slaveid, __attribute__((unused))int stat)
{
	struct lcore_resource_struct *slave = &lcore_resource[slaveid];

	printf("Get slave %u leave info\n", slaveid);
	if (!slave->enabled) {
		printf("Lcore=%u not registered for it's exit\n", slaveid);
		return;
	}
	rte_spinlock_lock(&res_lock);

	/* Change the state and wait master to start them */
	slave->flags = SLAVE_RECREATE_FLAG;

	rte_spinlock_unlock(&res_lock);
}

static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
{
	struct ether_hdr *eth;
	void *tmp;
	unsigned dst_port;
	int sent;
	struct rte_eth_dev_tx_buffer *buffer;

	dst_port = l2fwd_dst_ports[portid];
	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);

	/* src addr */
	ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

	buffer = tx_buffer[dst_port];
	sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
	if (sent)
		port_statistics[dst_port].tx += sent;
}

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	int sent;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;

	prev_tsc = 0;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {
		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (1) {
		enum l2fwd_cmd cmd;
		cur_tsc = rte_rdtsc();

		if (unlikely(getcmd(lcore_id, &cmd, 0) == 0)) {
			sendcmd(lcore_id, cmd, 0);

			/* If get stop command, stop forwarding and exit */
			if (cmd == CMD_STOP) {
				return;
			}
		}

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_rx_port; i++) {

				portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
				buffer = tx_buffer[portid];

				sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
				if (sent)
					port_statistics[portid].tx += sent;

			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {

			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			port_statistics[portid].rx += nb_rx;

			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				l2fwd_simple_forward(m, portid);
			}
		}
	}
}

static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	unsigned lcore_id = rte_lcore_id();

	if (float_proc) {
		unsigned flcore_id;

		/* Change it to floating process, also change it's lcore_id */
		clear_cpu_affinity();
		RTE_PER_LCORE(_lcore_id) = 0;
		/* Get a lcore_id */
		if (flib_assign_lcore_id() < 0 ) {
			printf("flib_assign_lcore_id failed\n");
			return -1;
		}
		flcore_id = rte_lcore_id();
		/* Set mapping id, so master can return it after slave exited */
		mapping_id[lcore_id] = flcore_id;
		printf("Org lcore_id = %u, cur lcore_id = %u\n",
				lcore_id, flcore_id);
		remapping_slave_resource(lcore_id, flcore_id);
	}

	l2fwd_main_loop();

	/* return lcore_id before return */
	if (float_proc) {
		flib_free_lcore_id(rte_lcore_id());
		mapping_id[lcore_id] = INVALID_MAPPING_ID;
	}
	return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK -s COREMASK [-q NQ] -f\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
	       "  -f use floating process which won't bind to any core to run\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
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
l2fwd_parse_nqueue(const char *q_arg)
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
l2fwd_parse_timer_period(const char *q_arg)
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

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};
	int has_pmask = 0;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:T:f",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			has_pmask = 1;
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* use floating process */
		case 'f':
			float_proc = 1;
			break;

		/* long options */
		case 0:
			l2fwd_usage(prgname);
			return -1;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	if (!has_pmask) {
		l2fwd_usage(prgname);
		return -1;
	}
	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
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

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int ret;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	unsigned i;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;

	/* Save cpu_affinity first, restore it in case it's floating process option */
	if (get_cpu_affinity() != 0)
		rte_exit(EXIT_FAILURE, "get_cpu_affinity error\n");

	/* Also tries to set cpu affinity to detect whether  it will fail in child process */
	if(clear_cpu_affinity() != 0)
		rte_exit(EXIT_FAILURE, "clear_cpu_affinity error\n");

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	/*flib init */
	if (flib_init() != 0)
		rte_exit(EXIT_FAILURE, "flib init error");

	/**
	  * Allocated structures that slave lcore would change. For those that slaves are
	  * read only, needn't use malloc to share and global or static variables is ok since
	  * slave inherit all the knowledge that master initialized.
	  **/
	if (l2fwd_malloc_shared_struct() != 0)
		rte_exit(EXIT_FAILURE, "malloc mem failed\n");

	/* Initialize lcore_resource structures */
	memset(lcore_resource, 0, sizeof(lcore_resource));
	for (i = 0; i < RTE_MAX_LCORE; i++)
		lcore_resource[i].lcore_id = i;

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* create the mbuf pool */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		char buf_name[RTE_MEMPOOL_NAMESIZE];
		snprintf(buf_name, RTE_MEMPOOL_NAMESIZE, MBUF_NAME, portid);
		l2fwd_pktmbuf_pool[portid] =
			rte_pktmbuf_pool_create(buf_name, NB_MBUF, 32,
				0, MBUF_DATA_SIZE, rte_socket_id());
		if (l2fwd_pktmbuf_pool[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

		printf("Create mbuf %s\n", buf_name);
	}

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		if (nb_ports_in_mask % 2) {
			l2fwd_dst_ports[portid] = last_port;
			l2fwd_dst_ports[last_port] = portid;
		}
		else
			last_port = portid;

		nb_ports_in_mask++;

		rte_eth_dev_info_get(portid, &dev_info);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		struct lcore_resource_struct *res;
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		/* skip master lcore */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
			   rte_get_master_lcore() == rx_lcore_id ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {

			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;

		/* Save the port resource info into lcore_resource strucutres */
		res = &lcore_resource[rx_lcore_id];
		res->enabled = 1;
		res->port[res->port_num++] = portid;

		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_dev_adjust_nb_rx_tx_desc: err=%d, port=%u\n",
				 ret, (unsigned) portid);

		rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);

		/* Initialize TX buffers */
		tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(portid));
		if (tx_buffer[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
					(unsigned) portid);

		rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,
				&port_statistics[portid].dropped);
		if (ret < 0)
				rte_exit(EXIT_FAILURE, "Cannot set error callback for "
						"tx buffer on port %u\n", (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		//memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	/* Record pair lcore */
	/**
	 * Since l2fwd example would create pair between different neighbour port, that's
	 * port 0 receive and forward to port 1, the same to port 1, these 2 ports will have
	 * dependency. If one port stopped working (killed, for example), the port need to
	 * be stopped/started again. During the time, another port need to wait until stop/start
	 * procedure completed. So, record the pair relationship for those lcores working
	 * on ports.
	 **/
	for (portid = 0; portid < nb_ports; portid++) {
		uint32_t pair_port;
		unsigned lcore = 0, pair_lcore = 0;
		unsigned j, find_lcore, find_pair_lcore;
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Find pair ports' lcores */
		find_lcore = find_pair_lcore = 0;
		pair_port = l2fwd_dst_ports[portid];
		for (i = 0; i < RTE_MAX_LCORE; i++) {
			if (!rte_lcore_is_enabled(i))
				continue;
			for (j = 0; j < lcore_queue_conf[i].n_rx_port;j++) {
				if (lcore_queue_conf[i].rx_port_list[j] == portid) {
					lcore = i;
					find_lcore = 1;
					break;
				}
				if (lcore_queue_conf[i].rx_port_list[j] == pair_port) {
					pair_lcore = i;
					find_pair_lcore = 1;
					break;
				}
			}
			if (find_lcore && find_pair_lcore)
				break;
		}
		if (!find_lcore || !find_pair_lcore)
			rte_exit(EXIT_FAILURE, "Not find port=%d pair\n", portid);

		printf("lcore %u and %u paired\n", lcore, pair_lcore);
		lcore_resource[lcore].pair_id = pair_lcore;
		lcore_resource[pair_lcore].pair_id = lcore;
	}

	/* Create message buffer for all master and slave */
	message_pool = rte_mempool_create("ms_msg_pool",
			   NB_CORE_MSGBUF * RTE_MAX_LCORE,
			   sizeof(enum l2fwd_cmd), NB_CORE_MSGBUF / 2,
			   0,
			   rte_pktmbuf_pool_init, NULL,
			   rte_pktmbuf_init, NULL,
			   rte_socket_id(), 0);

	if (message_pool == NULL)
		rte_exit(EXIT_FAILURE, "Create msg mempool failed\n");

	/* Create ring for each master and slave pair, also register cb when slave leaves */
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		/**
		 * Only create ring and register slave_exit cb in case that core involved into
		 * packet forwarding
		 **/
		if (lcore_resource[i].enabled) {
			/* Create ring for master and slave communication */
			ret = create_ms_ring(i);
			if (ret != 0)
				rte_exit(EXIT_FAILURE, "Create ring for lcore=%u failed",
				i);

			if (flib_register_slave_exit_notify(i,
				slave_exit_cb) != 0)
				rte_exit(EXIT_FAILURE,
						"Register master_trace_slave_exit failed");
		}
	}

	/* launch per-lcore init on every lcore except master */
	flib_mp_remote_launch(l2fwd_launch_one_lcore, NULL, SKIP_MASTER);

	/* print statistics 10 second */
	prev_tsc = cur_tsc = rte_rdtsc();
	timer_tsc = 0;
	while (1) {
		sleep(1);
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;
		/* if timer is enabled */
		if (timer_period > 0) {

			/* advance the timer */
			timer_tsc += diff_tsc;

			/* if timer has reached its timeout */
			if (unlikely(timer_tsc >= (uint64_t) timer_period)) {

				print_stats();
				/* reset the timer */
				timer_tsc = 0;
			}
		}

		prev_tsc = cur_tsc;

		/* Check any slave need restart or recreate */
		rte_spinlock_lock(&res_lock);
		for (i = 0; i < RTE_MAX_LCORE; i++) {
			struct lcore_resource_struct *res  = &lcore_resource[i];
			struct lcore_resource_struct *pair = &lcore_resource[res->pair_id];

			/* If find slave exited, try to reset pair */
			if (res->enabled && res->flags && pair->enabled) {
				if (!pair->flags) {
					master_sendcmd_with_ack(pair->lcore_id, CMD_STOP);
					rte_spinlock_unlock(&res_lock);
					sleep(1);
					rte_spinlock_lock(&res_lock);
					if (pair->flags)
						continue;
				}
				if (reset_pair(res->lcore_id, pair->lcore_id) != 0)
					rte_exit(EXIT_FAILURE, "failed to reset slave");
				res->flags  = 0;
				pair->flags = 0;
			}
		}
		rte_spinlock_unlock(&res_lock);
	}

}
