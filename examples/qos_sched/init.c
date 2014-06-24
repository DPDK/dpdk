/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <memory.h>

#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_sched.h>
#include <rte_cycles.h>
#include <rte_string_fns.h>

#include "main.h"
#include "cfg_file.h"

uint32_t app_numa_mask = 0;
static uint32_t app_inited_port_mask = 0;

int app_pipe_to_profile[MAX_SCHED_SUBPORTS][MAX_SCHED_PIPES];

#define MAX_NAME_LEN 32

struct ring_conf ring_conf = {
	.rx_size   = APP_RX_DESC_DEFAULT,
	.ring_size = APP_RING_SIZE,
	.tx_size   = APP_TX_DESC_DEFAULT,
};

struct burst_conf burst_conf = {
	.rx_burst    = MAX_PKT_RX_BURST,
	.ring_burst  = PKT_ENQUEUE,
	.qos_dequeue = PKT_DEQUEUE,
	.tx_burst    = MAX_PKT_TX_BURST,
};

struct ring_thresh rx_thresh = {
	.pthresh = RX_PTHRESH,
	.hthresh = RX_HTHRESH,
	.wthresh = RX_WTHRESH,
};

struct ring_thresh tx_thresh = {
	.pthresh = TX_PTHRESH,
	.hthresh = TX_HTHRESH,
	.wthresh = TX_WTHRESH,
};

uint32_t nb_pfc;
const char *cfg_profile = NULL;
int mp_size = NB_MBUF;
struct flow_conf qos_conf[MAX_DATA_STREAMS];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_DCB_NONE,
	},
};

static int
app_init_port(uint8_t portid, struct rte_mempool *mp)
{
	int ret;
	struct rte_eth_link link;
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_txconf tx_conf;

	/* check if port already initialized (multistream configuration) */
	if (app_inited_port_mask & (1u << portid))
		return 0;

	rx_conf.rx_thresh.pthresh = rx_thresh.pthresh;
	rx_conf.rx_thresh.hthresh = rx_thresh.hthresh;
	rx_conf.rx_thresh.wthresh = rx_thresh.wthresh;
	rx_conf.rx_free_thresh = 32;
	rx_conf.rx_drop_en = 0;

	tx_conf.tx_thresh.pthresh = tx_thresh.pthresh;
	tx_conf.tx_thresh.hthresh = tx_thresh.hthresh;
	tx_conf.tx_thresh.wthresh = tx_thresh.wthresh;
	tx_conf.tx_free_thresh = 0;
	tx_conf.tx_rs_thresh = 0;
	tx_conf.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOOFFLOADS;

	/* init port */
	RTE_LOG(INFO, APP, "Initializing port %"PRIu8"... ", portid);
	fflush(stdout);
	ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure device: "
				"err=%d, port=%"PRIu8"\n", ret, portid);

	/* init one RX queue */
	fflush(stdout);
	ret = rte_eth_rx_queue_setup(portid, 0, (uint16_t)ring_conf.rx_size,
		rte_eth_dev_socket_id(portid), &rx_conf, mp);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
				"err=%d, port=%"PRIu8"\n", ret, portid);

	/* init one TX queue */
	fflush(stdout);
	ret = rte_eth_tx_queue_setup(portid, 0,
		(uint16_t)ring_conf.tx_size, rte_eth_dev_socket_id(portid), &tx_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d, "
				"port=%"PRIu8" queue=%d\n", ret, portid, 0);

	/* Start device */
	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_pmd_port_start: "
				"err=%d, port=%"PRIu8"\n", ret, portid);

	printf("done: ");

	/* get link status */
	rte_eth_link_get(portid, &link);
	if (link.link_status) {
		printf(" Link Up - speed %u Mbps - %s\n",
			(uint32_t) link.link_speed,
			(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
			("full-duplex") : ("half-duplex\n"));
	} else {
		printf(" Link Down\n");
	}
	rte_eth_promiscuous_enable(portid);

	/* mark port as initialized */
	app_inited_port_mask |= 1u << portid;

	return 0;
}

static struct rte_sched_subport_params subport_params[MAX_SCHED_SUBPORTS] = {
	{
		.tb_rate = 1250000000,
		.tb_size = 1000000,

		.tc_rate = {1250000000, 1250000000, 1250000000, 1250000000},
		.tc_period = 10,
	},
};

static struct rte_sched_pipe_params pipe_profiles[RTE_SCHED_PIPE_PROFILES_PER_PORT] = {
	{ /* Profile #0 */
		.tb_rate = 305175,
		.tb_size = 1000000,

		.tc_rate = {305175, 305175, 305175, 305175},
		.tc_period = 40,
#ifdef RTE_SCHED_SUBPORT_TC_OV
		.tc_ov_weight = 1,
#endif

		.wrr_weights = {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1},
	},
};

struct rte_sched_port_params port_params = {
	.name = "port_scheduler_0",
	.socket = 0, /* computed */
	.rate = 0, /* computed */
	.mtu = 6 + 6 + 4 + 4 + 2 + 1500,
	.frame_overhead = RTE_SCHED_FRAME_OVERHEAD_DEFAULT,
	.n_subports_per_port = 1,
	.n_pipes_per_subport = 4096,
	.qsize = {64, 64, 64, 64},
	.pipe_profiles = pipe_profiles,
	.n_pipe_profiles = sizeof(pipe_profiles) / sizeof(struct rte_sched_pipe_params),

#ifdef RTE_SCHED_RED
	.red_params = {
		/* Traffic Class 0 Colors Green / Yellow / Red */
		[0][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[0][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[0][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 1 - Colors Green / Yellow / Red */
		[1][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[1][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[1][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 2 - Colors Green / Yellow / Red */
		[2][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[2][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[2][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},

		/* Traffic Class 3 - Colors Green / Yellow / Red */
		[3][0] = {.min_th = 48, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[3][1] = {.min_th = 40, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9},
		[3][2] = {.min_th = 32, .max_th = 64, .maxp_inv = 10, .wq_log2 = 9}
	}
#endif /* RTE_SCHED_RED */
};

static struct rte_sched_port *
app_init_sched_port(uint32_t portid, uint32_t socketid)
{
	static char port_name[32]; /* static as referenced from global port_params*/
	struct rte_eth_link link;
	struct rte_sched_port *port = NULL;
	uint32_t pipe, subport;
	int err;

	rte_eth_link_get((uint8_t)portid, &link);

	port_params.socket = socketid;
	port_params.rate = (uint64_t) link.link_speed * 1000 * 1000 / 8;
	snprintf(port_name, sizeof(port_name), "port_%d", portid);
	port_params.name = port_name;

	port = rte_sched_port_config(&port_params);
	if (port == NULL){
		rte_exit(EXIT_FAILURE, "Unable to config sched port\n");
	}

	for (subport = 0; subport < port_params.n_subports_per_port; subport ++) {
		err = rte_sched_subport_config(port, subport, &subport_params[subport]);
		if (err) {
			rte_exit(EXIT_FAILURE, "Unable to config sched subport %u, err=%d\n",
					subport, err);
		}

		for (pipe = 0; pipe < port_params.n_pipes_per_subport; pipe ++) {
			if (app_pipe_to_profile[subport][pipe] != -1) {
				err = rte_sched_pipe_config(port, subport, pipe,
						app_pipe_to_profile[subport][pipe]);
				if (err) {
					rte_exit(EXIT_FAILURE, "Unable to config sched pipe %u "
							"for profile %d, err=%d\n", pipe,
							app_pipe_to_profile[subport][pipe], err);
				}
			}
		}
	}

	return port;
}

static int
app_load_cfg_profile(const char *profile)
{
	if (profile == NULL)
		return 0;

	struct cfg_file *cfg_file = cfg_load(profile, 0);
	if (cfg_file == NULL)
		rte_exit(EXIT_FAILURE, "Cannot load configuration profile %s\n", profile);

	cfg_load_port(cfg_file, &port_params);
	cfg_load_subport(cfg_file, subport_params);
	cfg_load_pipe(cfg_file, pipe_profiles);

	cfg_close(cfg_file);

	return 0;
}

int app_init(void)
{
	uint32_t i;
	char ring_name[MAX_NAME_LEN];
	char pool_name[MAX_NAME_LEN];

	if (rte_eal_pci_probe() < 0)
		rte_exit(EXIT_FAILURE, "Cannot probe PCI\n");

	if (rte_eth_dev_count() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet port - bye\n");

	/* load configuration profile */
	if (app_load_cfg_profile(cfg_profile) != 0)
		rte_exit(EXIT_FAILURE, "Invalid configuration profile\n");

	/* Initialize each active flow */
	for(i = 0; i < nb_pfc; i++) {
		uint32_t socket = rte_lcore_to_socket_id(qos_conf[i].rx_core);
		struct rte_ring *ring;

		snprintf(ring_name, MAX_NAME_LEN, "ring-%u-%u", i, qos_conf[i].rx_core);
		ring = rte_ring_lookup(ring_name);
		if (ring == NULL)
			qos_conf[i].rx_ring = rte_ring_create(ring_name, ring_conf.ring_size,
			 	socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
		else
			qos_conf[i].rx_ring = ring;

		snprintf(ring_name, MAX_NAME_LEN, "ring-%u-%u", i, qos_conf[i].tx_core);
		ring = rte_ring_lookup(ring_name);
		if (ring == NULL)
			qos_conf[i].tx_ring = rte_ring_create(ring_name, ring_conf.ring_size,
				socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
		else
			qos_conf[i].tx_ring = ring;


		/* create the mbuf pools for each RX Port */
		snprintf(pool_name, MAX_NAME_LEN, "mbuf_pool%u", i);
		qos_conf[i].mbuf_pool = rte_mempool_create(pool_name, mp_size, MBUF_SIZE,
						burst_conf.rx_burst * 4,
						sizeof(struct rte_pktmbuf_pool_private),
						rte_pktmbuf_pool_init, NULL,
						rte_pktmbuf_init, NULL,
						rte_eth_dev_socket_id(qos_conf[i].rx_port),
						0);
		if (qos_conf[i].mbuf_pool == NULL)
			rte_exit(EXIT_FAILURE, "Cannot init mbuf pool for socket %u\n", i);

		app_init_port(qos_conf[i].rx_port, qos_conf[i].mbuf_pool);
		app_init_port(qos_conf[i].tx_port, qos_conf[i].mbuf_pool);

		qos_conf[i].sched_port = app_init_sched_port(qos_conf[i].tx_port, socket);
	}

	RTE_LOG(INFO, APP, "time stamp clock running at %" PRIu64 " Hz\n",
			 rte_get_timer_hz());

	RTE_LOG(INFO, APP, "Ring sizes: NIC RX = %u, Mempool = %d SW queue = %u,"
			 "NIC TX = %u\n", ring_conf.rx_size, mp_size, ring_conf.ring_size,
			 ring_conf.tx_size);

	RTE_LOG(INFO, APP, "Burst sizes: RX read = %hu, RX write = %hu,\n"
						  "             Worker read/QoS enqueue = %hu,\n"
						  "             QoS dequeue = %hu, Worker write = %hu\n",
		burst_conf.rx_burst, burst_conf.ring_burst, burst_conf.ring_burst,
		burst_conf.qos_dequeue, burst_conf.tx_burst);

	RTE_LOG(INFO, APP, "NIC thresholds RX (p = %hhu, h = %hhu, w = %hhu),"
				 "TX (p = %hhu, h = %hhu, w = %hhu)\n",
		rx_thresh.pthresh, rx_thresh.hthresh, rx_thresh.wthresh,
		tx_thresh.pthresh, tx_thresh.hthresh, tx_thresh.wthresh);

	return 0;
}
