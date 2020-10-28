/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_eventdev.h>
#include <rte_pause.h>
#include <rte_service.h>
#include <rte_service_component.h>
#include <rte_bus_vdev.h>

#include "sw_evdev.h"

#define MAX_PORTS 16
#define MAX_QIDS 16
#define NUM_PACKETS (1<<18)
#define DEQUEUE_DEPTH 128

static int evdev;

struct test {
	struct rte_mempool *mbuf_pool;
	uint8_t port[MAX_PORTS];
	uint8_t qid[MAX_QIDS];
	int nb_qids;
	uint32_t service_id;
};

typedef uint8_t counter_dynfield_t;
static int counter_dynfield_offset = -1;

static inline counter_dynfield_t *
counter_field(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, \
			counter_dynfield_offset, counter_dynfield_t *);
}

static struct rte_event release_ev;

static inline struct rte_mbuf *
rte_gen_arp(int portid, struct rte_mempool *mp)
{
	/*
	 * len = 14 + 46
	 * ARP, Request who-has 10.0.0.1 tell 10.0.0.2, length 46
	 */
	static const uint8_t arp_request[] = {
		/*0x0000:*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xec, 0xa8,
		0x6b, 0xfd, 0x02, 0x29, 0x08, 0x06, 0x00, 0x01,
		/*0x0010:*/ 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0xec, 0xa8,
		0x6b, 0xfd, 0x02, 0x29, 0x0a, 0x00, 0x00, 0x01,
		/*0x0020:*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00,
		0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		/*0x0030:*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	struct rte_mbuf *m;
	int pkt_len = sizeof(arp_request) - 1;

	m = rte_pktmbuf_alloc(mp);
	if (!m)
		return 0;

	memcpy((void *)((uintptr_t)m->buf_addr + m->data_off),
		arp_request, pkt_len);
	rte_pktmbuf_pkt_len(m) = pkt_len;
	rte_pktmbuf_data_len(m) = pkt_len;

	RTE_SET_USED(portid);

	return m;
}

static void
xstats_print(void)
{
	const uint32_t XSTATS_MAX = 1024;
	uint32_t i;
	uint32_t ids[XSTATS_MAX];
	uint64_t values[XSTATS_MAX];
	struct rte_event_dev_xstats_name xstats_names[XSTATS_MAX];

	for (i = 0; i < XSTATS_MAX; i++)
		ids[i] = i;

	/* Device names / values */
	int ret = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE, 0,
					xstats_names, ids, XSTATS_MAX);
	if (ret < 0) {
		printf("%d: xstats names get() returned error\n",
			__LINE__);
		return;
	}
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, ids, values, ret);
	if (ret > (signed int)XSTATS_MAX)
		printf("%s %d: more xstats available than space\n",
				__func__, __LINE__);
	for (i = 0; (signed int)i < ret; i++) {
		printf("%d : %s : %"PRIu64"\n",
				i, xstats_names[i].name, values[i]);
	}

	/* Port names / values */
	ret = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT, 0,
					xstats_names, ids, XSTATS_MAX);
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT, 1,
					ids, values, ret);
	if (ret > (signed int)XSTATS_MAX)
		printf("%s %d: more xstats available than space\n",
				__func__, __LINE__);
	for (i = 0; (signed int)i < ret; i++) {
		printf("%d : %s : %"PRIu64"\n",
				i, xstats_names[i].name, values[i]);
	}

	/* Queue names / values */
	ret = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE, 0,
					xstats_names, ids, XSTATS_MAX);
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					1, ids, values, ret);
	if (ret > (signed int)XSTATS_MAX)
		printf("%s %d: more xstats available than space\n",
				__func__, __LINE__);
	for (i = 0; (signed int)i < ret; i++) {
		printf("%d : %s : %"PRIu64"\n",
				i, xstats_names[i].name, values[i]);
	}
}

/* initialization and config */
static inline int
init(struct test *t, int nb_queues, int nb_ports)
{
	struct rte_event_dev_config config = {
			.nb_event_queues = nb_queues,
			.nb_event_ports = nb_ports,
			.nb_event_queue_flows = 1024,
			.nb_events_limit = 4096,
			.nb_event_port_dequeue_depth = DEQUEUE_DEPTH,
			.nb_event_port_enqueue_depth = 128,
	};
	int ret;

	void *temp = t->mbuf_pool; /* save and restore mbuf pool */

	memset(t, 0, sizeof(*t));
	t->mbuf_pool = temp;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0)
		printf("%d: Error configuring device\n", __LINE__);
	return ret;
};

static inline int
create_ports(struct test *t, int num_ports)
{
	int i;
	static const struct rte_event_port_conf conf = {
			.new_event_threshold = 1024,
			.dequeue_depth = 32,
			.enqueue_depth = 64,
	};
	if (num_ports > MAX_PORTS)
		return -1;

	for (i = 0; i < num_ports; i++) {
		if (rte_event_port_setup(evdev, i, &conf) < 0) {
			printf("Error setting up port %d\n", i);
			return -1;
		}
		t->port[i] = i;
	}

	return 0;
}

static inline int
create_lb_qids(struct test *t, int num_qids, uint32_t flags)
{
	int i;

	/* Q creation */
	const struct rte_event_queue_conf conf = {
			.schedule_type = flags,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
	};

	for (i = t->nb_qids; i < t->nb_qids + num_qids; i++) {
		if (rte_event_queue_setup(evdev, i, &conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
		t->qid[i] = i;
	}
	t->nb_qids += num_qids;
	if (t->nb_qids > MAX_QIDS)
		return -1;

	return 0;
}

static inline int
create_atomic_qids(struct test *t, int num_qids)
{
	return create_lb_qids(t, num_qids, RTE_SCHED_TYPE_ATOMIC);
}

static inline int
create_ordered_qids(struct test *t, int num_qids)
{
	return create_lb_qids(t, num_qids, RTE_SCHED_TYPE_ORDERED);
}


static inline int
create_unordered_qids(struct test *t, int num_qids)
{
	return create_lb_qids(t, num_qids, RTE_SCHED_TYPE_PARALLEL);
}

static inline int
create_directed_qids(struct test *t, int num_qids, const uint8_t ports[])
{
	int i;

	/* Q creation */
	static const struct rte_event_queue_conf conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK,
	};

	for (i = t->nb_qids; i < t->nb_qids + num_qids; i++) {
		if (rte_event_queue_setup(evdev, i, &conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
		t->qid[i] = i;

		if (rte_event_port_link(evdev, ports[i - t->nb_qids],
				&t->qid[i], NULL, 1) != 1) {
			printf("%d: error creating link for qid %d\n",
					__LINE__, i);
			return -1;
		}
	}
	t->nb_qids += num_qids;
	if (t->nb_qids > MAX_QIDS)
		return -1;

	return 0;
}

/* destruction */
static inline int
cleanup(struct test *t __rte_unused)
{
	rte_event_dev_stop(evdev);
	rte_event_dev_close(evdev);
	return 0;
};

struct test_event_dev_stats {
	uint64_t rx_pkts;       /**< Total packets received */
	uint64_t rx_dropped;    /**< Total packets dropped (Eg Invalid QID) */
	uint64_t tx_pkts;       /**< Total packets transmitted */

	/** Packets received on this port */
	uint64_t port_rx_pkts[MAX_PORTS];
	/** Packets dropped on this port */
	uint64_t port_rx_dropped[MAX_PORTS];
	/** Packets inflight on this port */
	uint64_t port_inflight[MAX_PORTS];
	/** Packets transmitted on this port */
	uint64_t port_tx_pkts[MAX_PORTS];
	/** Packets received on this qid */
	uint64_t qid_rx_pkts[MAX_QIDS];
	/** Packets dropped on this qid */
	uint64_t qid_rx_dropped[MAX_QIDS];
	/** Packets transmitted on this qid */
	uint64_t qid_tx_pkts[MAX_QIDS];
};

static inline int
test_event_dev_stats_get(int dev_id, struct test_event_dev_stats *stats)
{
	static uint32_t i;
	static uint32_t total_ids[3]; /* rx, tx and drop */
	static uint32_t port_rx_pkts_ids[MAX_PORTS];
	static uint32_t port_rx_dropped_ids[MAX_PORTS];
	static uint32_t port_inflight_ids[MAX_PORTS];
	static uint32_t port_tx_pkts_ids[MAX_PORTS];
	static uint32_t qid_rx_pkts_ids[MAX_QIDS];
	static uint32_t qid_rx_dropped_ids[MAX_QIDS];
	static uint32_t qid_tx_pkts_ids[MAX_QIDS];


	stats->rx_pkts = rte_event_dev_xstats_by_name_get(dev_id,
			"dev_rx", &total_ids[0]);
	stats->rx_dropped = rte_event_dev_xstats_by_name_get(dev_id,
			"dev_drop", &total_ids[1]);
	stats->tx_pkts = rte_event_dev_xstats_by_name_get(dev_id,
			"dev_tx", &total_ids[2]);
	for (i = 0; i < MAX_PORTS; i++) {
		char name[32];
		snprintf(name, sizeof(name), "port_%u_rx", i);
		stats->port_rx_pkts[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &port_rx_pkts_ids[i]);
		snprintf(name, sizeof(name), "port_%u_drop", i);
		stats->port_rx_dropped[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &port_rx_dropped_ids[i]);
		snprintf(name, sizeof(name), "port_%u_inflight", i);
		stats->port_inflight[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &port_inflight_ids[i]);
		snprintf(name, sizeof(name), "port_%u_tx", i);
		stats->port_tx_pkts[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &port_tx_pkts_ids[i]);
	}
	for (i = 0; i < MAX_QIDS; i++) {
		char name[32];
		snprintf(name, sizeof(name), "qid_%u_rx", i);
		stats->qid_rx_pkts[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &qid_rx_pkts_ids[i]);
		snprintf(name, sizeof(name), "qid_%u_drop", i);
		stats->qid_rx_dropped[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &qid_rx_dropped_ids[i]);
		snprintf(name, sizeof(name), "qid_%u_tx", i);
		stats->qid_tx_pkts[i] = rte_event_dev_xstats_by_name_get(
				dev_id, name, &qid_tx_pkts_ids[i]);
	}

	return 0;
}

/* run_prio_packet_test
 * This performs a basic packet priority check on the test instance passed in.
 * It is factored out of the main priority tests as the same tests must be
 * performed to ensure prioritization of each type of QID.
 *
 * Requirements:
 *  - An initialized test structure, including mempool
 *  - t->port[0] is initialized for both Enq / Deq of packets to the QID
 *  - t->qid[0] is the QID to be tested
 *  - if LB QID, the CQ must be mapped to the QID.
 */
static int
run_prio_packet_test(struct test *t)
{
	int err;
	const uint32_t MAGIC_SEQN[] = {4711, 1234};
	const uint32_t PRIORITY[] = {
		RTE_EVENT_DEV_PRIORITY_NORMAL,
		RTE_EVENT_DEV_PRIORITY_HIGHEST
	};
	unsigned int i;
	for (i = 0; i < RTE_DIM(MAGIC_SEQN); i++) {
		/* generate pkt and enqueue */
		struct rte_event ev;
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}
		*rte_event_pmd_selftest_seqn(arp) = MAGIC_SEQN[i];

		ev = (struct rte_event){
			.priority = PRIORITY[i],
			.op = RTE_EVENT_OP_NEW,
			.queue_id = t->qid[0],
			.mbuf = arp
		};
		err = rte_event_enqueue_burst(evdev, t->port[0], &ev, 1);
		if (err != 1) {
			printf("%d: error failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	struct test_event_dev_stats stats;
	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: error failed to get stats\n", __LINE__);
		return -1;
	}

	if (stats.port_rx_pkts[t->port[0]] != 2) {
		printf("%d: error stats incorrect for directed port\n",
				__LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}

	struct rte_event ev, ev2;
	uint32_t deq_pkts;
	deq_pkts = rte_event_dequeue_burst(evdev, t->port[0], &ev, 1, 0);
	if (deq_pkts != 1) {
		printf("%d: error failed to deq\n", __LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	if (*rte_event_pmd_selftest_seqn(ev.mbuf) != MAGIC_SEQN[1]) {
		printf("%d: first packet out not highest priority\n",
				__LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	rte_pktmbuf_free(ev.mbuf);

	deq_pkts = rte_event_dequeue_burst(evdev, t->port[0], &ev2, 1, 0);
	if (deq_pkts != 1) {
		printf("%d: error failed to deq\n", __LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	if (*rte_event_pmd_selftest_seqn(ev2.mbuf) != MAGIC_SEQN[0]) {
		printf("%d: second packet out not lower priority\n",
				__LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	rte_pktmbuf_free(ev2.mbuf);

	cleanup(t);
	return 0;
}

static int
test_single_directed_packet(struct test *t)
{
	const int rx_enq = 0;
	const int wrk_enq = 2;
	int err;

	/* Create instance with 3 directed QIDs going to 3 ports */
	if (init(t, 3, 3) < 0 ||
			create_ports(t, 3) < 0 ||
			create_directed_qids(t, 3, t->port) < 0)
		return -1;

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/************** FORWARD ****************/
	struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
	struct rte_event ev = {
			.op = RTE_EVENT_OP_NEW,
			.queue_id = wrk_enq,
			.mbuf = arp,
	};

	if (!arp) {
		printf("%d: gen of pkt failed\n", __LINE__);
		return -1;
	}

	const uint32_t MAGIC_SEQN = 4711;
	*rte_event_pmd_selftest_seqn(arp) = MAGIC_SEQN;

	/* generate pkt and enqueue */
	err = rte_event_enqueue_burst(evdev, rx_enq, &ev, 1);
	if (err != 1) {
		printf("%d: error failed to enqueue\n", __LINE__);
		return -1;
	}

	/* Run schedule() as dir packets may need to be re-ordered */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	struct test_event_dev_stats stats;
	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: error failed to get stats\n", __LINE__);
		return -1;
	}

	if (stats.port_rx_pkts[rx_enq] != 1) {
		printf("%d: error stats incorrect for directed port\n",
				__LINE__);
		return -1;
	}

	uint32_t deq_pkts;
	deq_pkts = rte_event_dequeue_burst(evdev, wrk_enq, &ev, 1, 0);
	if (deq_pkts != 1) {
		printf("%d: error failed to deq\n", __LINE__);
		return -1;
	}

	err = test_event_dev_stats_get(evdev, &stats);
	if (stats.port_rx_pkts[wrk_enq] != 0 &&
			stats.port_rx_pkts[wrk_enq] != 1) {
		printf("%d: error directed stats post-dequeue\n", __LINE__);
		return -1;
	}

	if (*rte_event_pmd_selftest_seqn(ev.mbuf) != MAGIC_SEQN) {
		printf("%d: error magic sequence number not dequeued\n",
				__LINE__);
		return -1;
	}

	rte_pktmbuf_free(ev.mbuf);
	cleanup(t);
	return 0;
}

static int
test_directed_forward_credits(struct test *t)
{
	uint32_t i;
	int32_t err;

	if (init(t, 1, 1) < 0 ||
			create_ports(t, 1) < 0 ||
			create_directed_qids(t, 1, t->port) < 0)
		return -1;

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	struct rte_event ev = {
			.op = RTE_EVENT_OP_NEW,
			.queue_id = 0,
	};

	for (i = 0; i < 1000; i++) {
		err = rte_event_enqueue_burst(evdev, 0, &ev, 1);
		if (err != 1) {
			printf("%d: error failed to enqueue\n", __LINE__);
			return -1;
		}
		rte_service_run_iter_on_app_lcore(t->service_id, 1);

		uint32_t deq_pkts;
		deq_pkts = rte_event_dequeue_burst(evdev, 0, &ev, 1, 0);
		if (deq_pkts != 1) {
			printf("%d: error failed to deq\n", __LINE__);
			return -1;
		}

		/* re-write event to be a forward, and continue looping it */
		ev.op = RTE_EVENT_OP_FORWARD;
	}

	cleanup(t);
	return 0;
}


static int
test_priority_directed(struct test *t)
{
	if (init(t, 1, 1) < 0 ||
			create_ports(t, 1) < 0 ||
			create_directed_qids(t, 1, t->port) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	return run_prio_packet_test(t);
}

static int
test_priority_atomic(struct test *t)
{
	if (init(t, 1, 1) < 0 ||
			create_ports(t, 1) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* map the QID */
	if (rte_event_port_link(evdev, t->port[0], &t->qid[0], NULL, 1) != 1) {
		printf("%d: error mapping qid to port\n", __LINE__);
		return -1;
	}
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	return run_prio_packet_test(t);
}

static int
test_priority_ordered(struct test *t)
{
	if (init(t, 1, 1) < 0 ||
			create_ports(t, 1) < 0 ||
			create_ordered_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* map the QID */
	if (rte_event_port_link(evdev, t->port[0], &t->qid[0], NULL, 1) != 1) {
		printf("%d: error mapping qid to port\n", __LINE__);
		return -1;
	}
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	return run_prio_packet_test(t);
}

static int
test_priority_unordered(struct test *t)
{
	if (init(t, 1, 1) < 0 ||
			create_ports(t, 1) < 0 ||
			create_unordered_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* map the QID */
	if (rte_event_port_link(evdev, t->port[0], &t->qid[0], NULL, 1) != 1) {
		printf("%d: error mapping qid to port\n", __LINE__);
		return -1;
	}
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	return run_prio_packet_test(t);
}

static int
burst_packets(struct test *t)
{
	/************** CONFIG ****************/
	uint32_t i;
	int err;
	int ret;

	/* Create instance with 2 ports and 2 queues */
	if (init(t, 2, 2) < 0 ||
			create_ports(t, 2) < 0 ||
			create_atomic_qids(t, 2) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	ret = rte_event_port_link(evdev, t->port[0], &t->qid[0], NULL, 1);
	if (ret != 1) {
		printf("%d: error mapping lb qid0\n", __LINE__);
		return -1;
	}
	ret = rte_event_port_link(evdev, t->port[1], &t->qid[1], NULL, 1);
	if (ret != 1) {
		printf("%d: error mapping lb qid1\n", __LINE__);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/************** FORWARD ****************/
	const uint32_t rx_port = 0;
	const uint32_t NUM_PKTS = 2;

	for (i = 0; i < NUM_PKTS; i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: error generating pkt\n", __LINE__);
			return -1;
		}

		struct rte_event ev = {
				.op = RTE_EVENT_OP_NEW,
				.queue_id = i % 2,
				.flow_id = i % 3,
				.mbuf = arp,
		};
		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_port], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* Check stats for all NUM_PKTS arrived to sched core */
	struct test_event_dev_stats stats;

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		return -1;
	}
	if (stats.rx_pkts != NUM_PKTS || stats.tx_pkts != NUM_PKTS) {
		printf("%d: Sched core didn't receive all %d pkts\n",
				__LINE__, NUM_PKTS);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}

	uint32_t deq_pkts;
	int p;

	deq_pkts = 0;
	/******** DEQ QID 1 *******/
	do {
		struct rte_event ev;
		p = rte_event_dequeue_burst(evdev, t->port[0], &ev, 1, 0);
		deq_pkts += p;
		rte_pktmbuf_free(ev.mbuf);
	} while (p);

	if (deq_pkts != NUM_PKTS/2) {
		printf("%d: Half of NUM_PKTS didn't arrive at port 1\n",
				__LINE__);
		return -1;
	}

	/******** DEQ QID 2 *******/
	deq_pkts = 0;
	do {
		struct rte_event ev;
		p = rte_event_dequeue_burst(evdev, t->port[1], &ev, 1, 0);
		deq_pkts += p;
		rte_pktmbuf_free(ev.mbuf);
	} while (p);
	if (deq_pkts != NUM_PKTS/2) {
		printf("%d: Half of NUM_PKTS didn't arrive at port 2\n",
				__LINE__);
		return -1;
	}

	cleanup(t);
	return 0;
}

static int
abuse_inflights(struct test *t)
{
	const int rx_enq = 0;
	const int wrk_enq = 2;
	int err;

	/* Create instance with 4 ports */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	err = rte_event_port_link(evdev, t->port[wrk_enq], NULL, NULL, 0);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/* Enqueue op only */
	err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &release_ev, 1);
	if (err != 1) {
		printf("%d: Failed to enqueue\n", __LINE__);
		return -1;
	}

	/* schedule */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	struct test_event_dev_stats stats;

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		return -1;
	}

	if (stats.rx_pkts != 0 ||
			stats.tx_pkts != 0 ||
			stats.port_inflight[wrk_enq] != 0) {
		printf("%d: Sched core didn't handle pkt as expected\n",
				__LINE__);
		return -1;
	}

	cleanup(t);
	return 0;
}

static int
xstats_tests(struct test *t)
{
	const int wrk_enq = 2;
	int err;

	/* Create instance with 4 ports */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	err = rte_event_port_link(evdev, t->port[wrk_enq], NULL, NULL, 0);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	const uint32_t XSTATS_MAX = 1024;

	uint32_t i;
	uint32_t ids[XSTATS_MAX];
	uint64_t values[XSTATS_MAX];
	struct rte_event_dev_xstats_name xstats_names[XSTATS_MAX];

	for (i = 0; i < XSTATS_MAX; i++)
		ids[i] = i;

	/* Device names / values */
	int ret = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, xstats_names, ids, XSTATS_MAX);
	if (ret != 6) {
		printf("%d: expected 6 stats, got return %d\n", __LINE__, ret);
		return -1;
	}
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, ids, values, ret);
	if (ret != 6) {
		printf("%d: expected 6 stats, got return %d\n", __LINE__, ret);
		return -1;
	}

	/* Port names / values */
	ret = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT, 0,
					xstats_names, ids, XSTATS_MAX);
	if (ret != 21) {
		printf("%d: expected 21 stats, got return %d\n", __LINE__, ret);
		return -1;
	}
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT, 0,
					ids, values, ret);
	if (ret != 21) {
		printf("%d: expected 21 stats, got return %d\n", __LINE__, ret);
		return -1;
	}

	/* Queue names / values */
	ret = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					0, xstats_names, ids, XSTATS_MAX);
	if (ret != 16) {
		printf("%d: expected 16 stats, got return %d\n", __LINE__, ret);
		return -1;
	}

	/* NEGATIVE TEST: with wrong queue passed, 0 stats should be returned */
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					1, ids, values, ret);
	if (ret != -EINVAL) {
		printf("%d: expected 0 stats, got return %d\n", __LINE__, ret);
		return -1;
	}

	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					0, ids, values, ret);
	if (ret != 16) {
		printf("%d: expected 16 stats, got return %d\n", __LINE__, ret);
		return -1;
	}

	/* enqueue packets to check values */
	for (i = 0; i < 3; i++) {
		struct rte_event ev;
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}
		ev.queue_id = t->qid[i];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = arp;
		ev.flow_id = 7;
		*rte_event_pmd_selftest_seqn(arp) = i;

		int err = rte_event_enqueue_burst(evdev, t->port[0], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* Device names / values */
	int num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE, 0,
					xstats_names, ids, XSTATS_MAX);
	if (num_stats < 0)
		goto fail;
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, ids, values, num_stats);
	static const uint64_t expected[] = {3, 3, 0, 1, 0, 0};
	for (i = 0; (signed int)i < ret; i++) {
		if (expected[i] != values[i]) {
			printf(
				"%d Error xstat %d (id %d) %s : %"PRIu64
				", expect %"PRIu64"\n",
				__LINE__, i, ids[i], xstats_names[i].name,
				values[i], expected[i]);
			goto fail;
		}
	}

	ret = rte_event_dev_xstats_reset(evdev, RTE_EVENT_DEV_XSTATS_DEVICE,
					0, NULL, 0);

	/* ensure reset statistics are zero-ed */
	static const uint64_t expected_zero[] = {0, 0, 0, 0, 0, 0};
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, ids, values, num_stats);
	for (i = 0; (signed int)i < ret; i++) {
		if (expected_zero[i] != values[i]) {
			printf(
				"%d Error, xstat %d (id %d) %s : %"PRIu64
				", expect %"PRIu64"\n",
				__LINE__, i, ids[i], xstats_names[i].name,
				values[i], expected_zero[i]);
			goto fail;
		}
	}

	/* port reset checks */
	num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT, 0,
					xstats_names, ids, XSTATS_MAX);
	if (num_stats < 0)
		goto fail;
	ret = rte_event_dev_xstats_get(evdev, RTE_EVENT_DEV_XSTATS_PORT,
					0, ids, values, num_stats);

	static const uint64_t port_expected[] = {
		3 /* rx */,
		0 /* tx */,
		0 /* drop */,
		0 /* inflights */,
		0 /* avg pkt cycles */,
		29 /* credits */,
		0 /* rx ring used */,
		4096 /* rx ring free */,
		0 /* cq ring used */,
		32 /* cq ring free */,
		0 /* dequeue calls */,
		/* 10 dequeue burst buckets */
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	};
	if (ret != RTE_DIM(port_expected)) {
		printf(
			"%s %d: wrong number of port stats (%d), expected %zu\n",
			__func__, __LINE__, ret, RTE_DIM(port_expected));
	}

	for (i = 0; (signed int)i < ret; i++) {
		if (port_expected[i] != values[i]) {
			printf(
				"%s : %d: Error stat %s is %"PRIu64
				", expected %"PRIu64"\n",
				__func__, __LINE__, xstats_names[i].name,
				values[i], port_expected[i]);
			goto fail;
		}
	}

	ret = rte_event_dev_xstats_reset(evdev, RTE_EVENT_DEV_XSTATS_PORT,
					0, NULL, 0);

	/* ensure reset statistics are zero-ed */
	static const uint64_t port_expected_zero[] = {
		0 /* rx */,
		0 /* tx */,
		0 /* drop */,
		0 /* inflights */,
		0 /* avg pkt cycles */,
		29 /* credits */,
		0 /* rx ring used */,
		4096 /* rx ring free */,
		0 /* cq ring used */,
		32 /* cq ring free */,
		0 /* dequeue calls */,
		/* 10 dequeue burst buckets */
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	};
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT,
					0, ids, values, num_stats);
	for (i = 0; (signed int)i < ret; i++) {
		if (port_expected_zero[i] != values[i]) {
			printf(
				"%d, Error, xstat %d (id %d) %s : %"PRIu64
				", expect %"PRIu64"\n",
				__LINE__, i, ids[i], xstats_names[i].name,
				values[i], port_expected_zero[i]);
			goto fail;
		}
	}

	/* QUEUE STATS TESTS */
	num_stats = rte_event_dev_xstats_names_get(evdev,
						RTE_EVENT_DEV_XSTATS_QUEUE, 0,
						xstats_names, ids, XSTATS_MAX);
	ret = rte_event_dev_xstats_get(evdev, RTE_EVENT_DEV_XSTATS_QUEUE,
					0, ids, values, num_stats);
	if (ret < 0) {
		printf("xstats get returned %d\n", ret);
		goto fail;
	}
	if ((unsigned int)ret > XSTATS_MAX)
		printf("%s %d: more xstats available than space\n",
				__func__, __LINE__);

	static const uint64_t queue_expected[] = {
		3 /* rx */,
		3 /* tx */,
		0 /* drop */,
		3 /* inflights */,
		0, 0, 0, 0, /* iq 0, 1, 2, 3 used */
		/* QID-to-Port: pinned_flows, packets */
		0, 0,
		0, 0,
		1, 3,
		0, 0,
	};
	for (i = 0; (signed int)i < ret; i++) {
		if (queue_expected[i] != values[i]) {
			printf(
				"%d, Error, xstat %d (id %d) %s : %"PRIu64
				", expect %"PRIu64"\n",
				__LINE__, i, ids[i], xstats_names[i].name,
				values[i], queue_expected[i]);
			goto fail;
		}
	}

	/* Reset the queue stats here */
	ret = rte_event_dev_xstats_reset(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE, 0,
					NULL,
					0);

	/* Verify that the resetable stats are reset, and others are not */
	static const uint64_t queue_expected_zero[] = {
		0 /* rx */,
		0 /* tx */,
		0 /* drop */,
		3 /* inflight */,
		0, 0, 0, 0, /* 4 iq used */
		/* QID-to-Port: pinned_flows, packets */
		0, 0,
		0, 0,
		1, 0,
		0, 0,
	};

	ret = rte_event_dev_xstats_get(evdev, RTE_EVENT_DEV_XSTATS_QUEUE, 0,
					ids, values, num_stats);
	int fails = 0;
	for (i = 0; (signed int)i < ret; i++) {
		if (queue_expected_zero[i] != values[i]) {
			printf(
				"%d, Error, xstat %d (id %d) %s : %"PRIu64
				", expect %"PRIu64"\n",
				__LINE__, i, ids[i], xstats_names[i].name,
				values[i], queue_expected_zero[i]);
			fails++;
		}
	}
	if (fails) {
		printf("%d : %d of values were not as expected above\n",
				__LINE__, fails);
		goto fail;
	}

	cleanup(t);
	return 0;

fail:
	rte_event_dev_dump(0, stdout);
	cleanup(t);
	return -1;
}


static int
xstats_id_abuse_tests(struct test *t)
{
	int err;
	const uint32_t XSTATS_MAX = 1024;
	const uint32_t link_port = 2;

	uint32_t ids[XSTATS_MAX];
	struct rte_event_dev_xstats_name xstats_names[XSTATS_MAX];

	/* Create instance with 4 ports */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		goto fail;
	}

	err = rte_event_port_link(evdev, t->port[link_port], NULL, NULL, 0);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		goto fail;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto fail;
	}

	/* no test for device, as it ignores the port/q number */
	int num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT,
					UINT8_MAX-1, xstats_names, ids,
					XSTATS_MAX);
	if (num_stats != 0) {
		printf("%d: expected %d stats, got return %d\n", __LINE__,
				0, num_stats);
		goto fail;
	}

	num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					UINT8_MAX-1, xstats_names, ids,
					XSTATS_MAX);
	if (num_stats != 0) {
		printf("%d: expected %d stats, got return %d\n", __LINE__,
				0, num_stats);
		goto fail;
	}

	cleanup(t);
	return 0;
fail:
	cleanup(t);
	return -1;
}

static int
port_reconfig_credits(struct test *t)
{
	if (init(t, 1, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	uint32_t i;
	const uint32_t NUM_ITERS = 32;
	for (i = 0; i < NUM_ITERS; i++) {
		const struct rte_event_queue_conf conf = {
			.schedule_type = RTE_SCHED_TYPE_ATOMIC,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
		};
		if (rte_event_queue_setup(evdev, 0, &conf) < 0) {
			printf("%d: error creating qid\n", __LINE__);
			return -1;
		}
		t->qid[0] = 0;

		static const struct rte_event_port_conf port_conf = {
				.new_event_threshold = 128,
				.dequeue_depth = 32,
				.enqueue_depth = 64,
		};
		if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
			printf("%d Error setting up port\n", __LINE__);
			return -1;
		}

		int links = rte_event_port_link(evdev, 0, NULL, NULL, 0);
		if (links != 1) {
			printf("%d: error mapping lb qid\n", __LINE__);
			goto fail;
		}

		if (rte_event_dev_start(evdev) < 0) {
			printf("%d: Error with start call\n", __LINE__);
			goto fail;
		}

		const uint32_t NPKTS = 1;
		uint32_t j;
		for (j = 0; j < NPKTS; j++) {
			struct rte_event ev;
			struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
			if (!arp) {
				printf("%d: gen of pkt failed\n", __LINE__);
				goto fail;
			}
			ev.queue_id = t->qid[0];
			ev.op = RTE_EVENT_OP_NEW;
			ev.mbuf = arp;
			int err = rte_event_enqueue_burst(evdev, 0, &ev, 1);
			if (err != 1) {
				printf("%d: Failed to enqueue\n", __LINE__);
				rte_event_dev_dump(0, stdout);
				goto fail;
			}
		}

		rte_service_run_iter_on_app_lcore(t->service_id, 1);

		struct rte_event ev[NPKTS];
		int deq = rte_event_dequeue_burst(evdev, t->port[0], ev,
							NPKTS, 0);
		if (deq != 1)
			printf("%d error; no packet dequeued\n", __LINE__);

		/* let cleanup below stop the device on last iter */
		if (i != NUM_ITERS-1)
			rte_event_dev_stop(evdev);
	}

	cleanup(t);
	return 0;
fail:
	cleanup(t);
	return -1;
}

static int
port_single_lb_reconfig(struct test *t)
{
	if (init(t, 2, 2) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		goto fail;
	}

	static const struct rte_event_queue_conf conf_lb_atomic = {
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.schedule_type = RTE_SCHED_TYPE_ATOMIC,
		.nb_atomic_flows = 1024,
		.nb_atomic_order_sequences = 1024,
	};
	if (rte_event_queue_setup(evdev, 0, &conf_lb_atomic) < 0) {
		printf("%d: error creating qid\n", __LINE__);
		goto fail;
	}

	static const struct rte_event_queue_conf conf_single_link = {
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK,
	};
	if (rte_event_queue_setup(evdev, 1, &conf_single_link) < 0) {
		printf("%d: error creating qid\n", __LINE__);
		goto fail;
	}

	struct rte_event_port_conf port_conf = {
		.new_event_threshold = 128,
		.dequeue_depth = 32,
		.enqueue_depth = 64,
	};
	if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
		printf("%d Error setting up port\n", __LINE__);
		goto fail;
	}
	if (rte_event_port_setup(evdev, 1, &port_conf) < 0) {
		printf("%d Error setting up port\n", __LINE__);
		goto fail;
	}

	/* link port to lb queue */
	uint8_t queue_id = 0;
	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: error creating link for qid\n", __LINE__);
		goto fail;
	}

	int ret = rte_event_port_unlink(evdev, 0, &queue_id, 1);
	if (ret != 1) {
		printf("%d: Error unlinking lb port\n", __LINE__);
		goto fail;
	}

	queue_id = 1;
	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: error creating link for qid\n", __LINE__);
		goto fail;
	}

	queue_id = 0;
	int err = rte_event_port_link(evdev, 1, &queue_id, NULL, 1);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		goto fail;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto fail;
	}

	cleanup(t);
	return 0;
fail:
	cleanup(t);
	return -1;
}

static int
xstats_brute_force(struct test *t)
{
	uint32_t i;
	const uint32_t XSTATS_MAX = 1024;
	uint32_t ids[XSTATS_MAX];
	uint64_t values[XSTATS_MAX];
	struct rte_event_dev_xstats_name xstats_names[XSTATS_MAX];


	/* Create instance with 4 ports */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	int err = rte_event_port_link(evdev, t->port[0], NULL, NULL, 0);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		goto fail;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto fail;
	}

	for (i = 0; i < XSTATS_MAX; i++)
		ids[i] = i;

	for (i = 0; i < 3; i++) {
		uint32_t mode = RTE_EVENT_DEV_XSTATS_DEVICE + i;
		uint32_t j;
		for (j = 0; j < UINT8_MAX; j++) {
			rte_event_dev_xstats_names_get(evdev, mode,
				j, xstats_names, ids, XSTATS_MAX);

			rte_event_dev_xstats_get(evdev, mode, j, ids,
						 values, XSTATS_MAX);
		}
	}

	cleanup(t);
	return 0;
fail:
	cleanup(t);
	return -1;
}

static int
xstats_id_reset_tests(struct test *t)
{
	const int wrk_enq = 2;
	int err;

	/* Create instance with 4 ports */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	err = rte_event_port_link(evdev, t->port[wrk_enq], NULL, NULL, 0);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		goto fail;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto fail;
	}

#define XSTATS_MAX 1024
	int ret;
	uint32_t i;
	uint32_t ids[XSTATS_MAX];
	uint64_t values[XSTATS_MAX];
	struct rte_event_dev_xstats_name xstats_names[XSTATS_MAX];

	for (i = 0; i < XSTATS_MAX; i++)
		ids[i] = i;

#define NUM_DEV_STATS 6
	/* Device names / values */
	int num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, xstats_names, ids, XSTATS_MAX);
	if (num_stats != NUM_DEV_STATS) {
		printf("%d: expected %d stats, got return %d\n", __LINE__,
				NUM_DEV_STATS, num_stats);
		goto fail;
	}
	ret = rte_event_dev_xstats_get(evdev,
					RTE_EVENT_DEV_XSTATS_DEVICE,
					0, ids, values, num_stats);
	if (ret != NUM_DEV_STATS) {
		printf("%d: expected %d stats, got return %d\n", __LINE__,
				NUM_DEV_STATS, ret);
		goto fail;
	}

#define NPKTS 7
	for (i = 0; i < NPKTS; i++) {
		struct rte_event ev;
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			goto fail;
		}
		ev.queue_id = t->qid[i];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = arp;
		*rte_event_pmd_selftest_seqn(arp) = i;

		int err = rte_event_enqueue_burst(evdev, t->port[0], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			goto fail;
		}
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	static const char * const dev_names[] = {
		"dev_rx", "dev_tx", "dev_drop", "dev_sched_calls",
		"dev_sched_no_iq_enq", "dev_sched_no_cq_enq",
	};
	uint64_t dev_expected[] = {NPKTS, NPKTS, 0, 1, 0, 0};
	for (i = 0; (int)i < ret; i++) {
		unsigned int id;
		uint64_t val = rte_event_dev_xstats_by_name_get(evdev,
								dev_names[i],
								&id);
		if (id != i) {
			printf("%d: %s id incorrect, expected %d got %d\n",
					__LINE__, dev_names[i], i, id);
			goto fail;
		}
		if (val != dev_expected[i]) {
			printf("%d: %s value incorrect, expected %"
				PRIu64" got %d\n", __LINE__, dev_names[i],
				dev_expected[i], id);
			goto fail;
		}
		/* reset to zero */
		int reset_ret = rte_event_dev_xstats_reset(evdev,
						RTE_EVENT_DEV_XSTATS_DEVICE, 0,
						&id,
						1);
		if (reset_ret) {
			printf("%d: failed to reset successfully\n", __LINE__);
			goto fail;
		}
		dev_expected[i] = 0;
		/* check value again */
		val = rte_event_dev_xstats_by_name_get(evdev, dev_names[i], 0);
		if (val != dev_expected[i]) {
			printf("%d: %s value incorrect, expected %"PRIu64
				" got %"PRIu64"\n", __LINE__, dev_names[i],
				dev_expected[i], val);
			goto fail;
		}
	};

/* 48 is stat offset from start of the devices whole xstats.
 * This WILL break every time we add a statistic to a port
 * or the device, but there is no other way to test
 */
#define PORT_OFF 48
/* num stats for the tested port. CQ size adds more stats to a port */
#define NUM_PORT_STATS 21
/* the port to test. */
#define PORT 2
	num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_PORT, PORT,
					xstats_names, ids, XSTATS_MAX);
	if (num_stats != NUM_PORT_STATS) {
		printf("%d: expected %d stats, got return %d\n",
			__LINE__, NUM_PORT_STATS, num_stats);
		goto fail;
	}
	ret = rte_event_dev_xstats_get(evdev, RTE_EVENT_DEV_XSTATS_PORT, PORT,
					ids, values, num_stats);

	if (ret != NUM_PORT_STATS) {
		printf("%d: expected %d stats, got return %d\n",
				__LINE__, NUM_PORT_STATS, ret);
		goto fail;
	}
	static const char * const port_names[] = {
		"port_2_rx",
		"port_2_tx",
		"port_2_drop",
		"port_2_inflight",
		"port_2_avg_pkt_cycles",
		"port_2_credits",
		"port_2_rx_ring_used",
		"port_2_rx_ring_free",
		"port_2_cq_ring_used",
		"port_2_cq_ring_free",
		"port_2_dequeue_calls",
		"port_2_dequeues_returning_0",
		"port_2_dequeues_returning_1-4",
		"port_2_dequeues_returning_5-8",
		"port_2_dequeues_returning_9-12",
		"port_2_dequeues_returning_13-16",
		"port_2_dequeues_returning_17-20",
		"port_2_dequeues_returning_21-24",
		"port_2_dequeues_returning_25-28",
		"port_2_dequeues_returning_29-32",
		"port_2_dequeues_returning_33-36",
	};
	uint64_t port_expected[] = {
		0, /* rx */
		NPKTS, /* tx */
		0, /* drop */
		NPKTS, /* inflight */
		0, /* avg pkt cycles */
		0, /* credits */
		0, /* rx ring used */
		4096, /* rx ring free */
		NPKTS,  /* cq ring used */
		25, /* cq ring free */
		0, /* dequeue zero calls */
		0, 0, 0, 0, 0, /* 10 dequeue buckets */
		0, 0, 0, 0, 0,
	};
	uint64_t port_expected_zero[] = {
		0, /* rx */
		0, /* tx */
		0, /* drop */
		NPKTS, /* inflight */
		0, /* avg pkt cycles */
		0, /* credits */
		0, /* rx ring used */
		4096, /* rx ring free */
		NPKTS,  /* cq ring used */
		25, /* cq ring free */
		0, /* dequeue zero calls */
		0, 0, 0, 0, 0, /* 10 dequeue buckets */
		0, 0, 0, 0, 0,
	};
	if (RTE_DIM(port_expected) != NUM_PORT_STATS ||
			RTE_DIM(port_names) != NUM_PORT_STATS) {
		printf("%d: port array of wrong size\n", __LINE__);
		goto fail;
	}

	int failed = 0;
	for (i = 0; (int)i < ret; i++) {
		unsigned int id;
		uint64_t val = rte_event_dev_xstats_by_name_get(evdev,
								port_names[i],
								&id);
		if (id != i + PORT_OFF) {
			printf("%d: %s id incorrect, expected %d got %d\n",
					__LINE__, port_names[i], i+PORT_OFF,
					id);
			failed = 1;
		}
		if (val != port_expected[i]) {
			printf("%d: %s value incorrect, expected %"PRIu64
				" got %d\n", __LINE__, port_names[i],
				port_expected[i], id);
			failed = 1;
		}
		/* reset to zero */
		int reset_ret = rte_event_dev_xstats_reset(evdev,
						RTE_EVENT_DEV_XSTATS_PORT, PORT,
						&id,
						1);
		if (reset_ret) {
			printf("%d: failed to reset successfully\n", __LINE__);
			failed = 1;
		}
		/* check value again */
		val = rte_event_dev_xstats_by_name_get(evdev, port_names[i], 0);
		if (val != port_expected_zero[i]) {
			printf("%d: %s value incorrect, expected %"PRIu64
				" got %"PRIu64"\n", __LINE__, port_names[i],
				port_expected_zero[i], val);
			failed = 1;
		}
	};
	if (failed)
		goto fail;

/* num queue stats */
#define NUM_Q_STATS 16
/* queue offset from start of the devices whole xstats.
 * This will break every time we add a statistic to a device/port/queue
 */
#define QUEUE_OFF 90
	const uint32_t queue = 0;
	num_stats = rte_event_dev_xstats_names_get(evdev,
					RTE_EVENT_DEV_XSTATS_QUEUE, queue,
					xstats_names, ids, XSTATS_MAX);
	if (num_stats != NUM_Q_STATS) {
		printf("%d: expected %d stats, got return %d\n",
			__LINE__, NUM_Q_STATS, num_stats);
		goto fail;
	}
	ret = rte_event_dev_xstats_get(evdev, RTE_EVENT_DEV_XSTATS_QUEUE,
					queue, ids, values, num_stats);
	if (ret != NUM_Q_STATS) {
		printf("%d: expected 21 stats, got return %d\n", __LINE__, ret);
		goto fail;
	}
	static const char * const queue_names[] = {
		"qid_0_rx",
		"qid_0_tx",
		"qid_0_drop",
		"qid_0_inflight",
		"qid_0_iq_0_used",
		"qid_0_iq_1_used",
		"qid_0_iq_2_used",
		"qid_0_iq_3_used",
		"qid_0_port_0_pinned_flows",
		"qid_0_port_0_packets",
		"qid_0_port_1_pinned_flows",
		"qid_0_port_1_packets",
		"qid_0_port_2_pinned_flows",
		"qid_0_port_2_packets",
		"qid_0_port_3_pinned_flows",
		"qid_0_port_3_packets",
	};
	uint64_t queue_expected[] = {
		7, /* rx */
		7, /* tx */
		0, /* drop */
		7, /* inflight */
		0, /* iq 0 used */
		0, /* iq 1 used */
		0, /* iq 2 used */
		0, /* iq 3 used */
		/* QID-to-Port: pinned_flows, packets */
		0, 0,
		0, 0,
		1, 7,
		0, 0,
	};
	uint64_t queue_expected_zero[] = {
		0, /* rx */
		0, /* tx */
		0, /* drop */
		7, /* inflight */
		0, /* iq 0 used */
		0, /* iq 1 used */
		0, /* iq 2 used */
		0, /* iq 3 used */
		/* QID-to-Port: pinned_flows, packets */
		0, 0,
		0, 0,
		1, 0,
		0, 0,
	};
	if (RTE_DIM(queue_expected) != NUM_Q_STATS ||
			RTE_DIM(queue_expected_zero) != NUM_Q_STATS ||
			RTE_DIM(queue_names) != NUM_Q_STATS) {
		printf("%d : queue array of wrong size\n", __LINE__);
		goto fail;
	}

	failed = 0;
	for (i = 0; (int)i < ret; i++) {
		unsigned int id;
		uint64_t val = rte_event_dev_xstats_by_name_get(evdev,
								queue_names[i],
								&id);
		if (id != i + QUEUE_OFF) {
			printf("%d: %s id incorrect, expected %d got %d\n",
					__LINE__, queue_names[i], i+QUEUE_OFF,
					id);
			failed = 1;
		}
		if (val != queue_expected[i]) {
			printf("%d: %d: %s value , expected %"PRIu64
				" got %"PRIu64"\n", i, __LINE__,
				queue_names[i], queue_expected[i], val);
			failed = 1;
		}
		/* reset to zero */
		int reset_ret = rte_event_dev_xstats_reset(evdev,
						RTE_EVENT_DEV_XSTATS_QUEUE,
						queue, &id, 1);
		if (reset_ret) {
			printf("%d: failed to reset successfully\n", __LINE__);
			failed = 1;
		}
		/* check value again */
		val = rte_event_dev_xstats_by_name_get(evdev, queue_names[i],
							0);
		if (val != queue_expected_zero[i]) {
			printf("%d: %s value incorrect, expected %"PRIu64
				" got %"PRIu64"\n", __LINE__, queue_names[i],
				queue_expected_zero[i], val);
			failed = 1;
		}
	};

	if (failed)
		goto fail;

	cleanup(t);
	return 0;
fail:
	cleanup(t);
	return -1;
}

static int
ordered_reconfigure(struct test *t)
{
	if (init(t, 1, 1) < 0 ||
			create_ports(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	const struct rte_event_queue_conf conf = {
			.schedule_type = RTE_SCHED_TYPE_ORDERED,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
	};

	if (rte_event_queue_setup(evdev, 0, &conf) < 0) {
		printf("%d: error creating qid\n", __LINE__);
		goto failed;
	}

	if (rte_event_queue_setup(evdev, 0, &conf) < 0) {
		printf("%d: error creating qid, for 2nd time\n", __LINE__);
		goto failed;
	}

	rte_event_port_link(evdev, t->port[0], NULL, NULL, 0);
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	cleanup(t);
	return 0;
failed:
	cleanup(t);
	return -1;
}

static int
qid_priorities(struct test *t)
{
	/* Test works by having a CQ with enough empty space for all packets,
	 * and enqueueing 3 packets to 3 QIDs. They must return based on the
	 * priority of the QID, not the ingress order, to pass the test
	 */
	unsigned int i;
	/* Create instance with 1 ports, and 3 qids */
	if (init(t, 3, 1) < 0 ||
			create_ports(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	for (i = 0; i < 3; i++) {
		/* Create QID */
		const struct rte_event_queue_conf conf = {
			.schedule_type = RTE_SCHED_TYPE_ATOMIC,
			/* increase priority (0 == highest), as we go */
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL - i,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
		};

		if (rte_event_queue_setup(evdev, i, &conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
		t->qid[i] = i;
	}
	t->nb_qids = i;
	/* map all QIDs to port */
	rte_event_port_link(evdev, t->port[0], NULL, NULL, 0);

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/* enqueue 3 packets, setting seqn and QID to check priority */
	for (i = 0; i < 3; i++) {
		struct rte_event ev;
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}
		ev.queue_id = t->qid[i];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = arp;
		*rte_event_pmd_selftest_seqn(arp) = i;

		int err = rte_event_enqueue_burst(evdev, t->port[0], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* dequeue packets, verify priority was upheld */
	struct rte_event ev[32];
	uint32_t deq_pkts =
		rte_event_dequeue_burst(evdev, t->port[0], ev, 32, 0);
	if (deq_pkts != 3) {
		printf("%d: failed to deq packets\n", __LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	for (i = 0; i < 3; i++) {
		if (*rte_event_pmd_selftest_seqn(ev[i].mbuf) != 2-i) {
			printf(
				"%d: qid priority test: seqn %d incorrectly prioritized\n",
					__LINE__, i);
		}
	}

	cleanup(t);
	return 0;
}

static int
unlink_in_progress(struct test *t)
{
	/* Test unlinking API, in particular that when an unlink request has
	 * not yet been seen by the scheduler thread, that the
	 * unlink_in_progress() function returns the number of unlinks.
	 */
	unsigned int i;
	/* Create instance with 1 ports, and 3 qids */
	if (init(t, 3, 1) < 0 ||
			create_ports(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	for (i = 0; i < 3; i++) {
		/* Create QID */
		const struct rte_event_queue_conf conf = {
			.schedule_type = RTE_SCHED_TYPE_ATOMIC,
			/* increase priority (0 == highest), as we go */
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL - i,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
		};

		if (rte_event_queue_setup(evdev, i, &conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
		t->qid[i] = i;
	}
	t->nb_qids = i;
	/* map all QIDs to port */
	rte_event_port_link(evdev, t->port[0], NULL, NULL, 0);

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/* unlink all ports to have outstanding unlink requests */
	int ret = rte_event_port_unlink(evdev, t->port[0], NULL, 0);
	if (ret < 0) {
		printf("%d: Failed to unlink queues\n", __LINE__);
		return -1;
	}

	/* get active unlinks here, expect 3 */
	int unlinks_in_progress =
		rte_event_port_unlinks_in_progress(evdev, t->port[0]);
	if (unlinks_in_progress != 3) {
		printf("%d: Expected num unlinks in progress == 3, got %d\n",
				__LINE__, unlinks_in_progress);
		return -1;
	}

	/* run scheduler service on this thread to ack the unlinks */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* active unlinks expected as 0 as scheduler thread has acked */
	unlinks_in_progress =
		rte_event_port_unlinks_in_progress(evdev, t->port[0]);
	if (unlinks_in_progress != 0) {
		printf("%d: Expected num unlinks in progress == 0, got %d\n",
				__LINE__, unlinks_in_progress);
	}

	cleanup(t);
	return 0;
}

static int
load_balancing(struct test *t)
{
	const int rx_enq = 0;
	int err;
	uint32_t i;

	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	for (i = 0; i < 3; i++) {
		/* map port 1 - 3 inclusive */
		if (rte_event_port_link(evdev, t->port[i+1], &t->qid[0],
				NULL, 1) != 1) {
			printf("%d: error mapping qid to port %d\n",
					__LINE__, i);
			return -1;
		}
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/************** FORWARD ****************/
	/*
	 * Create a set of flows that test the load-balancing operation of the
	 * implementation. Fill CQ 0 and 1 with flows 0 and 1, and test
	 * with a new flow, which should be sent to the 3rd mapped CQ
	 */
	static uint32_t flows[] = {0, 1, 1, 0, 0, 2, 2, 0, 2};

	for (i = 0; i < RTE_DIM(flows); i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}

		struct rte_event ev = {
				.op = RTE_EVENT_OP_NEW,
				.queue_id = t->qid[0],
				.flow_id = flows[i],
				.mbuf = arp,
		};
		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	struct test_event_dev_stats stats;
	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		return -1;
	}

	if (stats.port_inflight[1] != 4) {
		printf("%d:%s: port 1 inflight not correct\n", __LINE__,
				__func__);
		return -1;
	}
	if (stats.port_inflight[2] != 2) {
		printf("%d:%s: port 2 inflight not correct\n", __LINE__,
				__func__);
		return -1;
	}
	if (stats.port_inflight[3] != 3) {
		printf("%d:%s: port 3 inflight not correct\n", __LINE__,
				__func__);
		return -1;
	}

	cleanup(t);
	return 0;
}

static int
load_balancing_history(struct test *t)
{
	struct test_event_dev_stats stats = {0};
	const int rx_enq = 0;
	int err;
	uint32_t i;

	/* Create instance with 1 atomic QID going to 3 ports + 1 prod port */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0)
		return -1;

	/* CQ mapping to QID */
	if (rte_event_port_link(evdev, t->port[1], &t->qid[0], NULL, 1) != 1) {
		printf("%d: error mapping port 1 qid\n", __LINE__);
		return -1;
	}
	if (rte_event_port_link(evdev, t->port[2], &t->qid[0], NULL, 1) != 1) {
		printf("%d: error mapping port 2 qid\n", __LINE__);
		return -1;
	}
	if (rte_event_port_link(evdev, t->port[3], &t->qid[0], NULL, 1) != 1) {
		printf("%d: error mapping port 3 qid\n", __LINE__);
		return -1;
	}
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/*
	 * Create a set of flows that test the load-balancing operation of the
	 * implementation. Fill CQ 0, 1 and 2 with flows 0, 1 and 2, drop
	 * the packet from CQ 0, send in a new set of flows. Ensure that:
	 *  1. The new flow 3 gets into the empty CQ0
	 *  2. packets for existing flow gets added into CQ1
	 *  3. Next flow 0 pkt is now onto CQ2, since CQ0 and CQ1 now contain
	 *     more outstanding pkts
	 *
	 *  This test makes sure that when a flow ends (i.e. all packets
	 *  have been completed for that flow), that the flow can be moved
	 *  to a different CQ when new packets come in for that flow.
	 */
	static uint32_t flows1[] = {0, 1, 1, 2};

	for (i = 0; i < RTE_DIM(flows1); i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		struct rte_event ev = {
				.flow_id = flows1[i],
				.op = RTE_EVENT_OP_NEW,
				.queue_id = t->qid[0],
				.event_type = RTE_EVENT_TYPE_CPU,
				.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
				.mbuf = arp
		};

		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}
		arp->hash.rss = flows1[i];
		err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	/* call the scheduler */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* Dequeue the flow 0 packet from port 1, so that we can then drop */
	struct rte_event ev;
	if (!rte_event_dequeue_burst(evdev, t->port[1], &ev, 1, 0)) {
		printf("%d: failed to dequeue\n", __LINE__);
		return -1;
	}
	if (ev.mbuf->hash.rss != flows1[0]) {
		printf("%d: unexpected flow received\n", __LINE__);
		return -1;
	}

	/* drop the flow 0 packet from port 1 */
	rte_event_enqueue_burst(evdev, t->port[1], &release_ev, 1);

	/* call the scheduler */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/*
	 * Set up the next set of flows, first a new flow to fill up
	 * CQ 0, so that the next flow 0 packet should go to CQ2
	 */
	static uint32_t flows2[] = { 3, 3, 3, 1, 1, 0 };

	for (i = 0; i < RTE_DIM(flows2); i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		struct rte_event ev = {
				.flow_id = flows2[i],
				.op = RTE_EVENT_OP_NEW,
				.queue_id = t->qid[0],
				.event_type = RTE_EVENT_TYPE_CPU,
				.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
				.mbuf = arp
		};

		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}
		arp->hash.rss = flows2[i];

		err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	/* schedule */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d:failed to get stats\n", __LINE__);
		return -1;
	}

	/*
	 * Now check the resulting inflights on each port.
	 */
	if (stats.port_inflight[1] != 3) {
		printf("%d:%s: port 1 inflight not correct\n", __LINE__,
				__func__);
		printf("Inflights, ports 1, 2, 3: %u, %u, %u\n",
				(unsigned int)stats.port_inflight[1],
				(unsigned int)stats.port_inflight[2],
				(unsigned int)stats.port_inflight[3]);
		return -1;
	}
	if (stats.port_inflight[2] != 4) {
		printf("%d:%s: port 2 inflight not correct\n", __LINE__,
				__func__);
		printf("Inflights, ports 1, 2, 3: %u, %u, %u\n",
				(unsigned int)stats.port_inflight[1],
				(unsigned int)stats.port_inflight[2],
				(unsigned int)stats.port_inflight[3]);
		return -1;
	}
	if (stats.port_inflight[3] != 2) {
		printf("%d:%s: port 3 inflight not correct\n", __LINE__,
				__func__);
		printf("Inflights, ports 1, 2, 3: %u, %u, %u\n",
				(unsigned int)stats.port_inflight[1],
				(unsigned int)stats.port_inflight[2],
				(unsigned int)stats.port_inflight[3]);
		return -1;
	}

	for (i = 1; i <= 3; i++) {
		struct rte_event ev;
		while (rte_event_dequeue_burst(evdev, i, &ev, 1, 0))
			rte_event_enqueue_burst(evdev, i, &release_ev, 1);
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	cleanup(t);
	return 0;
}

static int
invalid_qid(struct test *t)
{
	struct test_event_dev_stats stats;
	const int rx_enq = 0;
	int err;
	uint32_t i;

	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	for (i = 0; i < 4; i++) {
		err = rte_event_port_link(evdev, t->port[i], &t->qid[0],
				NULL, 1);
		if (err != 1) {
			printf("%d: error mapping port 1 qid\n", __LINE__);
			return -1;
		}
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/*
	 * Send in a packet with an invalid qid to the scheduler.
	 * We should see the packed enqueued OK, but the inflights for
	 * that packet should not be incremented, and the rx_dropped
	 * should be incremented.
	 */
	static uint32_t flows1[] = {20};

	for (i = 0; i < RTE_DIM(flows1); i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}

		struct rte_event ev = {
				.op = RTE_EVENT_OP_NEW,
				.queue_id = t->qid[0] + flows1[i],
				.flow_id = i,
				.mbuf = arp,
		};
		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	/* call the scheduler */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		return -1;
	}

	/*
	 * Now check the resulting inflights on the port, and the rx_dropped.
	 */
	if (stats.port_inflight[0] != 0) {
		printf("%d:%s: port 1 inflight count not correct\n", __LINE__,
				__func__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	if (stats.port_rx_dropped[0] != 1) {
		printf("%d:%s: port 1 drops\n", __LINE__, __func__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}
	/* each packet drop should only be counted in one place - port or dev */
	if (stats.rx_dropped != 0) {
		printf("%d:%s: port 1 dropped count not correct\n", __LINE__,
				__func__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}

	cleanup(t);
	return 0;
}

static int
single_packet(struct test *t)
{
	const uint32_t MAGIC_SEQN = 7321;
	struct rte_event ev;
	struct test_event_dev_stats stats;
	const int rx_enq = 0;
	const int wrk_enq = 2;
	int err;

	/* Create instance with 4 ports */
	if (init(t, 1, 4) < 0 ||
			create_ports(t, 4) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	err = rte_event_port_link(evdev, t->port[wrk_enq], NULL, NULL, 0);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/************** Gen pkt and enqueue ****************/
	struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);
	if (!arp) {
		printf("%d: gen of pkt failed\n", __LINE__);
		return -1;
	}

	ev.op = RTE_EVENT_OP_NEW;
	ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	ev.mbuf = arp;
	ev.queue_id = 0;
	ev.flow_id = 3;
	*rte_event_pmd_selftest_seqn(arp) = MAGIC_SEQN;

	err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
	if (err != 1) {
		printf("%d: Failed to enqueue\n", __LINE__);
		return -1;
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		return -1;
	}

	if (stats.rx_pkts != 1 ||
			stats.tx_pkts != 1 ||
			stats.port_inflight[wrk_enq] != 1) {
		printf("%d: Sched core didn't handle pkt as expected\n",
				__LINE__);
		rte_event_dev_dump(evdev, stdout);
		return -1;
	}

	uint32_t deq_pkts;

	deq_pkts = rte_event_dequeue_burst(evdev, t->port[wrk_enq], &ev, 1, 0);
	if (deq_pkts < 1) {
		printf("%d: Failed to deq\n", __LINE__);
		return -1;
	}

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		return -1;
	}

	err = test_event_dev_stats_get(evdev, &stats);
	if (*rte_event_pmd_selftest_seqn(ev.mbuf) != MAGIC_SEQN) {
		printf("%d: magic sequence number not dequeued\n", __LINE__);
		return -1;
	}

	rte_pktmbuf_free(ev.mbuf);
	err = rte_event_enqueue_burst(evdev, t->port[wrk_enq], &release_ev, 1);
	if (err != 1) {
		printf("%d: Failed to enqueue\n", __LINE__);
		return -1;
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (stats.port_inflight[wrk_enq] != 0) {
		printf("%d: port inflight not correct\n", __LINE__);
		return -1;
	}

	cleanup(t);
	return 0;
}

static int
inflight_counts(struct test *t)
{
	struct rte_event ev;
	struct test_event_dev_stats stats;
	const int rx_enq = 0;
	const int p1 = 1;
	const int p2 = 2;
	int err;
	int i;

	/* Create instance with 4 ports */
	if (init(t, 2, 3) < 0 ||
			create_ports(t, 3) < 0 ||
			create_atomic_qids(t, 2) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* CQ mapping to QID */
	err = rte_event_port_link(evdev, t->port[p1], &t->qid[0], NULL, 1);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}
	err = rte_event_port_link(evdev, t->port[p2], &t->qid[1], NULL, 1);
	if (err != 1) {
		printf("%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/************** FORWARD ****************/
#define QID1_NUM 5
	for (i = 0; i < QID1_NUM; i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);

		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			goto err;
		}

		ev.queue_id =  t->qid[0];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = arp;
		err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			goto err;
		}
	}
#define QID2_NUM 3
	for (i = 0; i < QID2_NUM; i++) {
		struct rte_mbuf *arp = rte_gen_arp(0, t->mbuf_pool);

		if (!arp) {
			printf("%d: gen of pkt failed\n", __LINE__);
			goto err;
		}
		ev.queue_id =  t->qid[1];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = arp;
		err = rte_event_enqueue_burst(evdev, t->port[rx_enq], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			goto err;
		}
	}

	/* schedule */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (err) {
		printf("%d: failed to get stats\n", __LINE__);
		goto err;
	}

	if (stats.rx_pkts != QID1_NUM + QID2_NUM ||
			stats.tx_pkts != QID1_NUM + QID2_NUM) {
		printf("%d: Sched core didn't handle pkt as expected\n",
				__LINE__);
		goto err;
	}

	if (stats.port_inflight[p1] != QID1_NUM) {
		printf("%d: %s port 1 inflight not correct\n", __LINE__,
				__func__);
		goto err;
	}
	if (stats.port_inflight[p2] != QID2_NUM) {
		printf("%d: %s port 2 inflight not correct\n", __LINE__,
				__func__);
		goto err;
	}

	/************** DEQUEUE INFLIGHT COUNT CHECKS  ****************/
	/* port 1 */
	struct rte_event events[QID1_NUM + QID2_NUM];
	uint32_t deq_pkts = rte_event_dequeue_burst(evdev, t->port[p1], events,
			RTE_DIM(events), 0);

	if (deq_pkts != QID1_NUM) {
		printf("%d: Port 1: DEQUEUE inflight failed\n", __LINE__);
		goto err;
	}
	err = test_event_dev_stats_get(evdev, &stats);
	if (stats.port_inflight[p1] != QID1_NUM) {
		printf("%d: port 1 inflight decrement after DEQ != 0\n",
				__LINE__);
		goto err;
	}
	for (i = 0; i < QID1_NUM; i++) {
		err = rte_event_enqueue_burst(evdev, t->port[p1], &release_ev,
				1);
		if (err != 1) {
			printf("%d: %s rte enqueue of inf release failed\n",
				__LINE__, __func__);
			goto err;
		}
	}

	/*
	 * As the scheduler core decrements inflights, it needs to run to
	 * process packets to act on the drop messages
	 */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (stats.port_inflight[p1] != 0) {
		printf("%d: port 1 inflight NON NULL after DROP\n", __LINE__);
		goto err;
	}

	/* port2 */
	deq_pkts = rte_event_dequeue_burst(evdev, t->port[p2], events,
			RTE_DIM(events), 0);
	if (deq_pkts != QID2_NUM) {
		printf("%d: Port 2: DEQUEUE inflight failed\n", __LINE__);
		goto err;
	}
	err = test_event_dev_stats_get(evdev, &stats);
	if (stats.port_inflight[p2] != QID2_NUM) {
		printf("%d: port 1 inflight decrement after DEQ != 0\n",
				__LINE__);
		goto err;
	}
	for (i = 0; i < QID2_NUM; i++) {
		err = rte_event_enqueue_burst(evdev, t->port[p2], &release_ev,
				1);
		if (err != 1) {
			printf("%d: %s rte enqueue of inf release failed\n",
				__LINE__, __func__);
			goto err;
		}
	}

	/*
	 * As the scheduler core decrements inflights, it needs to run to
	 * process packets to act on the drop messages
	 */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	err = test_event_dev_stats_get(evdev, &stats);
	if (stats.port_inflight[p2] != 0) {
		printf("%d: port 2 inflight NON NULL after DROP\n", __LINE__);
		goto err;
	}
	cleanup(t);
	return 0;

err:
	rte_event_dev_dump(evdev, stdout);
	cleanup(t);
	return -1;
}

static int
parallel_basic(struct test *t, int check_order)
{
	const uint8_t rx_port = 0;
	const uint8_t w1_port = 1;
	const uint8_t w3_port = 3;
	const uint8_t tx_port = 4;
	int err;
	int i;
	uint32_t deq_pkts, j;
	struct rte_mbuf *mbufs[3];
	struct rte_mbuf *mbufs_out[3] = { 0 };
	const uint32_t MAGIC_SEQN = 1234;

	/* Create instance with 4 ports */
	if (init(t, 2, tx_port + 1) < 0 ||
			create_ports(t, tx_port + 1) < 0 ||
			(check_order ?  create_ordered_qids(t, 1) :
				create_unordered_qids(t, 1)) < 0 ||
			create_directed_qids(t, 1, &tx_port)) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/*
	 * CQ mapping to QID
	 * We need three ports, all mapped to the same ordered qid0. Then we'll
	 * take a packet out to each port, re-enqueue in reverse order,
	 * then make sure the reordering has taken place properly when we
	 * dequeue from the tx_port.
	 *
	 * Simplified test setup diagram:
	 *
	 * rx_port        w1_port
	 *        \     /         \
	 *         qid0 - w2_port - qid1
	 *              \         /     \
	 *                w3_port        tx_port
	 */
	/* CQ mapping to QID for LB ports (directed mapped on create) */
	for (i = w1_port; i <= w3_port; i++) {
		err = rte_event_port_link(evdev, t->port[i], &t->qid[0], NULL,
				1);
		if (err != 1) {
			printf("%d: error mapping lb qid\n", __LINE__);
			cleanup(t);
			return -1;
		}
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	/* Enqueue 3 packets to the rx port */
	for (i = 0; i < 3; i++) {
		struct rte_event ev;
		mbufs[i] = rte_gen_arp(0, t->mbuf_pool);
		if (!mbufs[i]) {
			printf("%d: gen of pkt failed\n", __LINE__);
			return -1;
		}

		ev.queue_id = t->qid[0];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = mbufs[i];
		*rte_event_pmd_selftest_seqn(mbufs[i]) = MAGIC_SEQN + i;

		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_port], &ev, 1);
		if (err != 1) {
			printf("%d: Failed to enqueue pkt %u, retval = %u\n",
					__LINE__, i, err);
			return -1;
		}
	}

	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* use extra slot to make logic in loops easier */
	struct rte_event deq_ev[w3_port + 1];

	/* Dequeue the 3 packets, one from each worker port */
	for (i = w1_port; i <= w3_port; i++) {
		deq_pkts = rte_event_dequeue_burst(evdev, t->port[i],
				&deq_ev[i], 1, 0);
		if (deq_pkts != 1) {
			printf("%d: Failed to deq\n", __LINE__);
			rte_event_dev_dump(evdev, stdout);
			return -1;
		}
	}

	/* Enqueue each packet in reverse order, flushing after each one */
	for (i = w3_port; i >= w1_port; i--) {

		deq_ev[i].op = RTE_EVENT_OP_FORWARD;
		deq_ev[i].queue_id = t->qid[1];
		err = rte_event_enqueue_burst(evdev, t->port[i], &deq_ev[i], 1);
		if (err != 1) {
			printf("%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* dequeue from the tx ports, we should get 3 packets */
	deq_pkts = rte_event_dequeue_burst(evdev, t->port[tx_port], deq_ev,
			3, 0);

	/* Check to see if we've got all 3 packets */
	if (deq_pkts != 3) {
		printf("%d: expected 3 pkts at tx port got %d from port %d\n",
			__LINE__, deq_pkts, tx_port);
		rte_event_dev_dump(evdev, stdout);
		return 1;
	}

	/* Check to see if the sequence numbers are in expected order */
	if (check_order) {
		for (j = 0 ; j < deq_pkts ; j++) {
			if (*rte_event_pmd_selftest_seqn(deq_ev[j].mbuf) !=
					MAGIC_SEQN + j) {
				printf("%d: Incorrect sequence number(%d) from port %d\n",
					__LINE__,
					*rte_event_pmd_selftest_seqn(mbufs_out[j]),
					tx_port);
				return -1;
			}
		}
	}

	/* Destroy the instance */
	cleanup(t);
	return 0;
}

static int
ordered_basic(struct test *t)
{
	return parallel_basic(t, 1);
}

static int
unordered_basic(struct test *t)
{
	return parallel_basic(t, 0);
}

static int
holb(struct test *t) /* test to check we avoid basic head-of-line blocking */
{
	const struct rte_event new_ev = {
			.op = RTE_EVENT_OP_NEW
			/* all other fields zero */
	};
	struct rte_event ev = new_ev;
	unsigned int rx_port = 0; /* port we get the first flow on */
	char rx_port_used_stat[64];
	char rx_port_free_stat[64];
	char other_port_used_stat[64];

	if (init(t, 1, 2) < 0 ||
			create_ports(t, 2) < 0 ||
			create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}
	int nb_links = rte_event_port_link(evdev, t->port[1], NULL, NULL, 0);
	if (rte_event_port_link(evdev, t->port[0], NULL, NULL, 0) != 1 ||
			nb_links != 1) {
		printf("%d: Error links queue to ports\n", __LINE__);
		goto err;
	}
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto err;
	}

	/* send one packet and see where it goes, port 0 or 1 */
	if (rte_event_enqueue_burst(evdev, t->port[0], &ev, 1) != 1) {
		printf("%d: Error doing first enqueue\n", __LINE__);
		goto err;
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	if (rte_event_dev_xstats_by_name_get(evdev, "port_0_cq_ring_used", NULL)
			!= 1)
		rx_port = 1;

	snprintf(rx_port_used_stat, sizeof(rx_port_used_stat),
			"port_%u_cq_ring_used", rx_port);
	snprintf(rx_port_free_stat, sizeof(rx_port_free_stat),
			"port_%u_cq_ring_free", rx_port);
	snprintf(other_port_used_stat, sizeof(other_port_used_stat),
			"port_%u_cq_ring_used", rx_port ^ 1);
	if (rte_event_dev_xstats_by_name_get(evdev, rx_port_used_stat, NULL)
			!= 1) {
		printf("%d: Error, first event not scheduled\n", __LINE__);
		goto err;
	}

	/* now fill up the rx port's queue with one flow to cause HOLB */
	do {
		ev = new_ev;
		if (rte_event_enqueue_burst(evdev, t->port[0], &ev, 1) != 1) {
			printf("%d: Error with enqueue\n", __LINE__);
			goto err;
		}
		rte_service_run_iter_on_app_lcore(t->service_id, 1);
	} while (rte_event_dev_xstats_by_name_get(evdev,
				rx_port_free_stat, NULL) != 0);

	/* one more packet, which needs to stay in IQ - i.e. HOLB */
	ev = new_ev;
	if (rte_event_enqueue_burst(evdev, t->port[0], &ev, 1) != 1) {
		printf("%d: Error with enqueue\n", __LINE__);
		goto err;
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	/* check that the other port still has an empty CQ */
	if (rte_event_dev_xstats_by_name_get(evdev, other_port_used_stat, NULL)
			!= 0) {
		printf("%d: Error, second port CQ is not empty\n", __LINE__);
		goto err;
	}
	/* check IQ now has one packet */
	if (rte_event_dev_xstats_by_name_get(evdev, "qid_0_iq_0_used", NULL)
			!= 1) {
		printf("%d: Error, QID does not have exactly 1 packet\n",
			__LINE__);
		goto err;
	}

	/* send another flow, which should pass the other IQ entry */
	ev = new_ev;
	ev.flow_id = 1;
	if (rte_event_enqueue_burst(evdev, t->port[0], &ev, 1) != 1) {
		printf("%d: Error with enqueue\n", __LINE__);
		goto err;
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	if (rte_event_dev_xstats_by_name_get(evdev, other_port_used_stat, NULL)
			!= 1) {
		printf("%d: Error, second flow did not pass out first\n",
			__LINE__);
		goto err;
	}

	if (rte_event_dev_xstats_by_name_get(evdev, "qid_0_iq_0_used", NULL)
			!= 1) {
		printf("%d: Error, QID does not have exactly 1 packet\n",
			__LINE__);
		goto err;
	}
	cleanup(t);
	return 0;
err:
	rte_event_dev_dump(evdev, stdout);
	cleanup(t);
	return -1;
}

static void
flush(uint8_t dev_id __rte_unused, struct rte_event event, void *arg)
{
	*((uint8_t *) arg) += (event.u64 == 0xCA11BACC) ? 1 : 0;
}

static int
dev_stop_flush(struct test *t) /* test to check we can properly flush events */
{
	const struct rte_event new_ev = {
		.op = RTE_EVENT_OP_NEW,
		.u64 = 0xCA11BACC,
		.queue_id = 0
	};
	struct rte_event ev = new_ev;
	uint8_t count = 0;
	int i;

	if (init(t, 1, 1) < 0 ||
	    create_ports(t, 1) < 0 ||
	    create_atomic_qids(t, 1) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* Link the queue so *_start() doesn't error out */
	if (rte_event_port_link(evdev, t->port[0], NULL, NULL, 0) != 1) {
		printf("%d: Error linking queue to port\n", __LINE__);
		goto err;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto err;
	}

	for (i = 0; i < DEQUEUE_DEPTH + 1; i++) {
		if (rte_event_enqueue_burst(evdev, t->port[0], &ev, 1) != 1) {
			printf("%d: Error enqueuing events\n", __LINE__);
			goto err;
		}
	}

	/* Schedule the events from the port to the IQ. At least one event
	 * should be remaining in the queue.
	 */
	rte_service_run_iter_on_app_lcore(t->service_id, 1);

	if (rte_event_dev_stop_flush_callback_register(evdev, flush, &count)) {
		printf("%d: Error installing the flush callback\n", __LINE__);
		goto err;
	}

	cleanup(t);

	if (count == 0) {
		printf("%d: Error executing the flush callback\n", __LINE__);
		goto err;
	}

	if (rte_event_dev_stop_flush_callback_register(evdev, NULL, NULL)) {
		printf("%d: Error uninstalling the flush callback\n", __LINE__);
		goto err;
	}

	return 0;
err:
	rte_event_dev_dump(evdev, stdout);
	cleanup(t);
	return -1;
}

static int
worker_loopback_worker_fn(void *arg)
{
	struct test *t = arg;
	uint8_t port = t->port[1];
	int count = 0;
	int enqd;

	/*
	 * Takes packets from the input port and then loops them back through
	 * the Eventdev. Each packet gets looped through QIDs 0-8, 16 times
	 * so each packet goes through 8*16 = 128 times.
	 */
	printf("%d: \tWorker function started\n", __LINE__);
	while (count < NUM_PACKETS) {
#define BURST_SIZE 32
		struct rte_event ev[BURST_SIZE];
		uint16_t i, nb_rx = rte_event_dequeue_burst(evdev, port, ev,
				BURST_SIZE, 0);
		if (nb_rx == 0) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			ev[i].queue_id++;
			if (ev[i].queue_id != 8) {
				ev[i].op = RTE_EVENT_OP_FORWARD;
				enqd = rte_event_enqueue_burst(evdev, port,
						&ev[i], 1);
				if (enqd != 1) {
					printf("%d: Can't enqueue FWD!!\n",
							__LINE__);
					return -1;
				}
				continue;
			}

			ev[i].queue_id = 0;
			(*counter_field(ev[i].mbuf))++;
			if (*counter_field(ev[i].mbuf) != 16) {
				ev[i].op = RTE_EVENT_OP_FORWARD;
				enqd = rte_event_enqueue_burst(evdev, port,
						&ev[i], 1);
				if (enqd != 1) {
					printf("%d: Can't enqueue FWD!!\n",
							__LINE__);
					return -1;
				}
				continue;
			}
			/* we have hit 16 iterations through system - drop */
			rte_pktmbuf_free(ev[i].mbuf);
			count++;
			ev[i].op = RTE_EVENT_OP_RELEASE;
			enqd = rte_event_enqueue_burst(evdev, port, &ev[i], 1);
			if (enqd != 1) {
				printf("%d drop enqueue failed\n", __LINE__);
				return -1;
			}
		}
	}

	return 0;
}

static int
worker_loopback_producer_fn(void *arg)
{
	struct test *t = arg;
	uint8_t port = t->port[0];
	uint64_t count = 0;

	printf("%d: \tProducer function started\n", __LINE__);
	while (count < NUM_PACKETS) {
		struct rte_mbuf *m = 0;
		do {
			m = rte_pktmbuf_alloc(t->mbuf_pool);
		} while (m == NULL);

		*counter_field(m) = 0;

		struct rte_event ev = {
				.op = RTE_EVENT_OP_NEW,
				.queue_id = t->qid[0],
				.flow_id = (uintptr_t)m & 0xFFFF,
				.mbuf = m,
		};

		if (rte_event_enqueue_burst(evdev, port, &ev, 1) != 1) {
			while (rte_event_enqueue_burst(evdev, port, &ev, 1) !=
					1)
				rte_pause();
		}

		count++;
	}

	return 0;
}

static int
worker_loopback(struct test *t, uint8_t disable_implicit_release)
{
	/* use a single producer core, and a worker core to see what happens
	 * if the worker loops packets back multiple times
	 */
	struct test_event_dev_stats stats;
	uint64_t print_cycles = 0, cycles = 0;
	uint64_t tx_pkts = 0;
	int err;
	int w_lcore, p_lcore;

	static const struct rte_mbuf_dynfield counter_dynfield_desc = {
		.name = "rte_event_sw_dynfield_selftest_counter",
		.size = sizeof(counter_dynfield_t),
		.align = __alignof__(counter_dynfield_t),
	};
	counter_dynfield_offset =
		rte_mbuf_dynfield_register(&counter_dynfield_desc);
	if (counter_dynfield_offset < 0) {
		printf("Error registering mbuf field\n");
		return -rte_errno;
	}

	if (init(t, 8, 2) < 0 ||
			create_atomic_qids(t, 8) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	/* RX with low max events */
	static struct rte_event_port_conf conf = {
			.dequeue_depth = 32,
			.enqueue_depth = 64,
	};
	/* beware: this cannot be initialized in the static above as it would
	 * only be initialized once - and this needs to be set for multiple runs
	 */
	conf.new_event_threshold = 512;
	conf.event_port_cfg = disable_implicit_release ?
		RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL : 0;

	if (rte_event_port_setup(evdev, 0, &conf) < 0) {
		printf("Error setting up RX port\n");
		return -1;
	}
	t->port[0] = 0;
	/* TX with higher max events */
	conf.new_event_threshold = 4096;
	if (rte_event_port_setup(evdev, 1, &conf) < 0) {
		printf("Error setting up TX port\n");
		return -1;
	}
	t->port[1] = 1;

	/* CQ mapping to QID */
	err = rte_event_port_link(evdev, t->port[1], NULL, NULL, 0);
	if (err != 8) { /* should have mapped all queues*/
		printf("%d: error mapping port 2 to all qids\n", __LINE__);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		return -1;
	}

	p_lcore = rte_get_next_lcore(
			/* start core */ -1,
			/* skip main */ 1,
			/* wrap */ 0);
	w_lcore = rte_get_next_lcore(p_lcore, 1, 0);

	rte_eal_remote_launch(worker_loopback_producer_fn, t, p_lcore);
	rte_eal_remote_launch(worker_loopback_worker_fn, t, w_lcore);

	print_cycles = cycles = rte_get_timer_cycles();
	while (rte_eal_get_lcore_state(p_lcore) != FINISHED ||
			rte_eal_get_lcore_state(w_lcore) != FINISHED) {

		rte_service_run_iter_on_app_lcore(t->service_id, 1);

		uint64_t new_cycles = rte_get_timer_cycles();

		if (new_cycles - print_cycles > rte_get_timer_hz()) {
			test_event_dev_stats_get(evdev, &stats);
			printf(
				"%d: \tSched Rx = %"PRIu64", Tx = %"PRIu64"\n",
				__LINE__, stats.rx_pkts, stats.tx_pkts);

			print_cycles = new_cycles;
		}
		if (new_cycles - cycles > rte_get_timer_hz() * 3) {
			test_event_dev_stats_get(evdev, &stats);
			if (stats.tx_pkts == tx_pkts) {
				rte_event_dev_dump(evdev, stdout);
				printf("Dumping xstats:\n");
				xstats_print();
				printf(
					"%d: No schedules for seconds, deadlock\n",
					__LINE__);
				return -1;
			}
			tx_pkts = stats.tx_pkts;
			cycles = new_cycles;
		}
	}
	rte_service_run_iter_on_app_lcore(t->service_id, 1);
	/* ensure all completions are flushed */

	rte_eal_mp_wait_lcore();

	cleanup(t);
	return 0;
}

static struct rte_mempool *eventdev_func_mempool;

int
test_sw_eventdev(void)
{
	struct test *t;
	int ret;

	t = malloc(sizeof(struct test));
	if (t == NULL)
		return -1;
	/* manually initialize the op, older gcc's complain on static
	 * initialization of struct elements that are a bitfield.
	 */
	release_ev.op = RTE_EVENT_OP_RELEASE;

	const char *eventdev_name = "event_sw";
	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		printf("%d: Eventdev %s not found - creating.\n",
				__LINE__, eventdev_name);
		if (rte_vdev_init(eventdev_name, NULL) < 0) {
			printf("Error creating eventdev\n");
			goto test_fail;
		}
		evdev = rte_event_dev_get_dev_id(eventdev_name);
		if (evdev < 0) {
			printf("Error finding newly created eventdev\n");
			goto test_fail;
		}
	}

	if (rte_event_dev_service_id_get(evdev, &t->service_id) < 0) {
		printf("Failed to get service ID for software event dev\n");
		goto test_fail;
	}

	rte_service_runstate_set(t->service_id, 1);
	rte_service_set_runstate_mapped_check(t->service_id, 0);

	/* Only create mbuf pool once, reuse for each test run */
	if (!eventdev_func_mempool) {
		eventdev_func_mempool = rte_pktmbuf_pool_create(
				"EVENTDEV_SW_SA_MBUF_POOL",
				(1<<12), /* 4k buffers */
				32 /*MBUF_CACHE_SIZE*/,
				0,
				512, /* use very small mbufs */
				rte_socket_id());
		if (!eventdev_func_mempool) {
			printf("ERROR creating mempool\n");
			goto test_fail;
		}
	}
	t->mbuf_pool = eventdev_func_mempool;
	printf("*** Running Single Directed Packet test...\n");
	ret = test_single_directed_packet(t);
	if (ret != 0) {
		printf("ERROR - Single Directed Packet test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Directed Forward Credit test...\n");
	ret = test_directed_forward_credits(t);
	if (ret != 0) {
		printf("ERROR - Directed Forward Credit test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Single Load Balanced Packet test...\n");
	ret = single_packet(t);
	if (ret != 0) {
		printf("ERROR - Single Packet test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Unordered Basic test...\n");
	ret = unordered_basic(t);
	if (ret != 0) {
		printf("ERROR -  Unordered Basic test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Ordered Basic test...\n");
	ret = ordered_basic(t);
	if (ret != 0) {
		printf("ERROR -  Ordered Basic test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Burst Packets test...\n");
	ret = burst_packets(t);
	if (ret != 0) {
		printf("ERROR - Burst Packets test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Load Balancing test...\n");
	ret = load_balancing(t);
	if (ret != 0) {
		printf("ERROR - Load Balancing test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Prioritized Directed test...\n");
	ret = test_priority_directed(t);
	if (ret != 0) {
		printf("ERROR - Prioritized Directed test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Prioritized Atomic test...\n");
	ret = test_priority_atomic(t);
	if (ret != 0) {
		printf("ERROR - Prioritized Atomic test FAILED.\n");
		goto test_fail;
	}

	printf("*** Running Prioritized Ordered test...\n");
	ret = test_priority_ordered(t);
	if (ret != 0) {
		printf("ERROR - Prioritized Ordered test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Prioritized Unordered test...\n");
	ret = test_priority_unordered(t);
	if (ret != 0) {
		printf("ERROR - Prioritized Unordered test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Invalid QID test...\n");
	ret = invalid_qid(t);
	if (ret != 0) {
		printf("ERROR - Invalid QID test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Load Balancing History test...\n");
	ret = load_balancing_history(t);
	if (ret != 0) {
		printf("ERROR - Load Balancing History test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Inflight Count test...\n");
	ret = inflight_counts(t);
	if (ret != 0) {
		printf("ERROR - Inflight Count test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Abuse Inflights test...\n");
	ret = abuse_inflights(t);
	if (ret != 0) {
		printf("ERROR - Abuse Inflights test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running XStats test...\n");
	ret = xstats_tests(t);
	if (ret != 0) {
		printf("ERROR - XStats test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running XStats ID Reset test...\n");
	ret = xstats_id_reset_tests(t);
	if (ret != 0) {
		printf("ERROR - XStats ID Reset test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running XStats Brute Force test...\n");
	ret = xstats_brute_force(t);
	if (ret != 0) {
		printf("ERROR - XStats Brute Force test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running XStats ID Abuse test...\n");
	ret = xstats_id_abuse_tests(t);
	if (ret != 0) {
		printf("ERROR - XStats ID Abuse test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running QID Priority test...\n");
	ret = qid_priorities(t);
	if (ret != 0) {
		printf("ERROR - QID Priority test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Unlink-in-progress test...\n");
	ret = unlink_in_progress(t);
	if (ret != 0) {
		printf("ERROR - Unlink in progress test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Ordered Reconfigure test...\n");
	ret = ordered_reconfigure(t);
	if (ret != 0) {
		printf("ERROR - Ordered Reconfigure test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Port LB Single Reconfig test...\n");
	ret = port_single_lb_reconfig(t);
	if (ret != 0) {
		printf("ERROR - Port LB Single Reconfig test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Port Reconfig Credits test...\n");
	ret = port_reconfig_credits(t);
	if (ret != 0) {
		printf("ERROR - Port Reconfig Credits Reset test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Head-of-line-blocking test...\n");
	ret = holb(t);
	if (ret != 0) {
		printf("ERROR - Head-of-line-blocking test FAILED.\n");
		goto test_fail;
	}
	printf("*** Running Stop Flush test...\n");
	ret = dev_stop_flush(t);
	if (ret != 0) {
		printf("ERROR - Stop Flush test FAILED.\n");
		goto test_fail;
	}
	if (rte_lcore_count() >= 3) {
		printf("*** Running Worker loopback test...\n");
		ret = worker_loopback(t, 0);
		if (ret != 0) {
			printf("ERROR - Worker loopback test FAILED.\n");
			return ret;
		}

		printf("*** Running Worker loopback test (implicit release disabled)...\n");
		ret = worker_loopback(t, 1);
		if (ret != 0) {
			printf("ERROR - Worker loopback test FAILED.\n");
			goto test_fail;
		}
	} else {
		printf("### Not enough cores for worker loopback tests.\n");
		printf("### Need at least 3 cores for the tests.\n");
	}

	/*
	 * Free test instance, leaving mempool initialized, and a pointer to it
	 * in static eventdev_func_mempool, as it is re-used on re-runs
	 */
	free(t);

	printf("SW Eventdev Selftest Successful.\n");
	return 0;
test_fail:
	free(t);
	printf("SW Eventdev Selftest Failed.\n");
	return -1;
}
