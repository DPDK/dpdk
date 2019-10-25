/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "test.h"

#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_sched.h>


#define SUBPORT         0
#define PIPE            1
#define TC              2
#define QUEUE           0

static struct rte_sched_pipe_params pipe_profile[] = {
	{ /* Profile #0 */
		.tb_rate = 305175,
		.tb_size = 1000000,

		.tc_rate = {305175, 305175, 305175, 305175, 305175, 305175,
			305175, 305175, 305175, 305175, 305175, 305175, 305175},
		.tc_period = 40,
		.tc_ov_weight = 1,

		.wrr_weights = {1, 1, 1, 1},
	},
};

static struct rte_sched_subport_params subport_param[] = {
	{
		.tb_rate = 1250000000,
		.tb_size = 1000000,

		.tc_rate = {1250000000, 1250000000, 1250000000, 1250000000,
			1250000000, 1250000000, 1250000000, 1250000000, 1250000000,
			1250000000, 1250000000, 1250000000, 1250000000},
		.tc_period = 10,
		.n_pipes_per_subport_enabled = 1024,
		.qsize = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32},
		.pipe_profiles = pipe_profile,
		.n_pipe_profiles = 1,
		.n_max_pipe_profiles = 1,
	},
};

static struct rte_sched_port_params port_param = {
	.socket = 0, /* computed */
	.rate = 0, /* computed */
	.mtu = 1522,
	.frame_overhead = RTE_SCHED_FRAME_OVERHEAD_DEFAULT,
	.n_subports_per_port = 1,
	.n_pipes_per_subport = 1024,
};

#define NB_MBUF          32
#define MBUF_DATA_SZ     (2048 + RTE_PKTMBUF_HEADROOM)
#define MEMPOOL_CACHE_SZ 0
#define SOCKET           0


static struct rte_mempool *
create_mempool(void)
{
	struct rte_mempool * mp;

	mp = rte_mempool_lookup("test_sched");
	if (!mp)
		mp = rte_pktmbuf_pool_create("test_sched", NB_MBUF,
			MEMPOOL_CACHE_SZ, 0, MBUF_DATA_SZ, SOCKET);

	return mp;
}

static void
prepare_pkt(struct rte_sched_port *port, struct rte_mbuf *mbuf)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_vlan_hdr *vlan1, *vlan2;
	struct rte_ipv4_hdr *ip_hdr;

	/* Simulate a classifier */
	eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
	vlan1 = (struct rte_vlan_hdr *)(&eth_hdr->ether_type);
	vlan2 = (struct rte_vlan_hdr *)(
		(uintptr_t)&eth_hdr->ether_type + sizeof(struct rte_vlan_hdr));
	eth_hdr = (struct rte_ether_hdr *)(
		(uintptr_t)&eth_hdr->ether_type +
		2 * sizeof(struct rte_vlan_hdr));
	ip_hdr = (struct rte_ipv4_hdr *)(
		(uintptr_t)eth_hdr + sizeof(eth_hdr->ether_type));

	vlan1->vlan_tci = rte_cpu_to_be_16(SUBPORT);
	vlan2->vlan_tci = rte_cpu_to_be_16(PIPE);
	eth_hdr->ether_type =  rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	ip_hdr->dst_addr = RTE_IPV4(0,0,TC,QUEUE);


	rte_sched_port_pkt_write(port, mbuf, SUBPORT, PIPE, TC, QUEUE,
					RTE_COLOR_YELLOW);

	/* 64 byte packet */
	mbuf->pkt_len  = 60;
	mbuf->data_len = 60;
}


/**
 * test main entrance for library sched
 */
static int
test_sched(void)
{
	struct rte_mempool *mp = NULL;
	struct rte_sched_port *port = NULL;
	uint32_t pipe;
	struct rte_mbuf *in_mbufs[10];
	struct rte_mbuf *out_mbufs[10];
	int i;

	int err;

	mp = create_mempool();
	TEST_ASSERT_NOT_NULL(mp, "Error creating mempool\n");

	port_param.socket = 0;
	port_param.rate = (uint64_t) 10000 * 1000 * 1000 / 8;

	port = rte_sched_port_config(&port_param);
	TEST_ASSERT_NOT_NULL(port, "Error config sched port\n");

	err = rte_sched_subport_config(port, SUBPORT, subport_param);
	TEST_ASSERT_SUCCESS(err, "Error config sched, err=%d\n", err);

	for (pipe = 0; pipe < subport_param[0].n_pipes_per_subport_enabled; pipe++) {
		err = rte_sched_pipe_config(port, SUBPORT, pipe, 0);
		TEST_ASSERT_SUCCESS(err, "Error config sched pipe %u, err=%d\n", pipe, err);
	}

	for (i = 0; i < 10; i++) {
		in_mbufs[i] = rte_pktmbuf_alloc(mp);
		TEST_ASSERT_NOT_NULL(in_mbufs[i], "Packet allocation failed\n");
		prepare_pkt(port, in_mbufs[i]);
	}


	err = rte_sched_port_enqueue(port, in_mbufs, 10);
	TEST_ASSERT_EQUAL(err, 10, "Wrong enqueue, err=%d\n", err);

	err = rte_sched_port_dequeue(port, out_mbufs, 10);
	TEST_ASSERT_EQUAL(err, 10, "Wrong dequeue, err=%d\n", err);

	for (i = 0; i < 10; i++) {
		enum rte_color color;
		uint32_t subport, traffic_class, queue;

		color = rte_sched_port_pkt_read_color(out_mbufs[i]);
		TEST_ASSERT_EQUAL(color, RTE_COLOR_YELLOW, "Wrong color\n");

		rte_sched_port_pkt_read_tree_path(port, out_mbufs[i],
				&subport, &pipe, &traffic_class, &queue);

		TEST_ASSERT_EQUAL(subport, SUBPORT, "Wrong subport\n");
		TEST_ASSERT_EQUAL(pipe, PIPE, "Wrong pipe\n");
		TEST_ASSERT_EQUAL(traffic_class, TC, "Wrong traffic_class\n");
		TEST_ASSERT_EQUAL(queue, QUEUE, "Wrong queue\n");

	}


	struct rte_sched_subport_stats subport_stats;
	uint32_t tc_ov;
	rte_sched_subport_read_stats(port, SUBPORT, &subport_stats, &tc_ov);
#if 0
	TEST_ASSERT_EQUAL(subport_stats.n_pkts_tc[TC-1], 10, "Wrong subport stats\n");
#endif
	struct rte_sched_queue_stats queue_stats;
	uint16_t qlen;
	rte_sched_queue_read_stats(port, QUEUE, &queue_stats, &qlen);
#if 0
	TEST_ASSERT_EQUAL(queue_stats.n_pkts, 10, "Wrong queue stats\n");
#endif

	rte_sched_port_free(port);

	return 0;
}

REGISTER_TEST_COMMAND(sched_autotest, test_sched);
