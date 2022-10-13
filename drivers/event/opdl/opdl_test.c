/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_eventdev.h>
#include <bus_vdev_driver.h>
#include <rte_pause.h>

#include "opdl_evdev.h"
#include "opdl_log.h"


#define MAX_PORTS 16
#define MAX_QIDS 16
#define NUM_PACKETS (1<<18)
#define NUM_EVENTS 256
#define BURST_SIZE 32



static int evdev;

struct test {
	struct rte_mempool *mbuf_pool;
	uint8_t port[MAX_PORTS];
	uint8_t qid[MAX_QIDS];
	int nb_qids;
};

static struct rte_mempool *eventdev_func_mempool;

static __rte_always_inline struct rte_mbuf *
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

/* initialization and config */
static __rte_always_inline int
init(struct test *t, int nb_queues, int nb_ports)
{
	struct rte_event_dev_config config = {
			.nb_event_queues = nb_queues,
			.nb_event_ports = nb_ports,
			.nb_event_queue_flows = 1024,
			.nb_events_limit = 4096,
			.nb_event_port_dequeue_depth = 128,
			.nb_event_port_enqueue_depth = 128,
	};
	int ret;

	void *temp = t->mbuf_pool; /* save and restore mbuf pool */

	memset(t, 0, sizeof(*t));
	t->mbuf_pool = temp;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0)
		PMD_DRV_LOG(ERR, "%d: Error configuring device\n", __LINE__);
	return ret;
};

static __rte_always_inline int
create_ports(struct test *t, int num_ports)
{
	int i;
	static const struct rte_event_port_conf conf = {
			.new_event_threshold = 1024,
			.dequeue_depth = 32,
			.enqueue_depth = 32,
	};
	if (num_ports > MAX_PORTS)
		return -1;

	for (i = 0; i < num_ports; i++) {
		if (rte_event_port_setup(evdev, i, &conf) < 0) {
			PMD_DRV_LOG(ERR, "Error setting up port %d\n", i);
			return -1;
		}
		t->port[i] = i;
	}

	return 0;
};

static __rte_always_inline int
create_queues_type(struct test *t, int num_qids, enum queue_type flags)
{
	int i;
	uint8_t type;

	switch (flags) {
	case OPDL_Q_TYPE_ORDERED:
		type = RTE_SCHED_TYPE_ORDERED;
		break;
	case OPDL_Q_TYPE_ATOMIC:
		type = RTE_SCHED_TYPE_ATOMIC;
		break;
	default:
		type = 0;
	}

	/* Q creation */
	const struct rte_event_queue_conf conf = {
		.event_queue_cfg =
		(flags == OPDL_Q_TYPE_SINGLE_LINK ?
		 RTE_EVENT_QUEUE_CFG_SINGLE_LINK : 0),
		.schedule_type = type,
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.nb_atomic_flows = 1024,
		.nb_atomic_order_sequences = 1024,
	};

	for (i = t->nb_qids ; i < t->nb_qids + num_qids; i++) {
		if (rte_event_queue_setup(evdev, i, &conf) < 0) {
			PMD_DRV_LOG(ERR, "%d: error creating qid %d\n ",
					__LINE__, i);
			return -1;
		}
		t->qid[i] = i;
	}

	t->nb_qids += num_qids;

	if (t->nb_qids > MAX_QIDS)
		return -1;

	return 0;
}


/* destruction */
static __rte_always_inline int
cleanup(struct test *t __rte_unused)
{
	rte_event_dev_stop(evdev);
	rte_event_dev_close(evdev);
	PMD_DRV_LOG(ERR, "clean up for test done\n");
	return 0;
};

static int
ordered_basic(struct test *t)
{
	const uint8_t rx_port = 0;
	const uint8_t w1_port = 1;
	const uint8_t w3_port = 3;
	const uint8_t tx_port = 4;
	int err;
	uint32_t i;
	uint32_t deq_pkts;
	struct rte_mbuf *mbufs[3];

	const uint32_t MAGIC_SEQN = 1234;

	/* Create instance with 5 ports */
	if (init(t, 2, tx_port+1) < 0 ||
	    create_ports(t, tx_port+1) < 0 ||
	    create_queues_type(t, 2, OPDL_Q_TYPE_ORDERED)) {
		PMD_DRV_LOG(ERR, "%d: Error initializing device\n", __LINE__);
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
			PMD_DRV_LOG(ERR, "%d: error mapping lb qid\n",
					__LINE__);
			cleanup(t);
			return -1;
		}
	}

	err = rte_event_port_link(evdev, t->port[tx_port], &t->qid[1], NULL,
			1);
	if (err != 1) {
		PMD_DRV_LOG(ERR, "%d: error mapping TX  qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		PMD_DRV_LOG(ERR, "%d: Error with start call\n", __LINE__);
		return -1;
	}
	/* Enqueue 3 packets to the rx port */
	for (i = 0; i < 3; i++) {
		struct rte_event ev;
		mbufs[i] = rte_gen_arp(0, t->mbuf_pool);
		if (!mbufs[i]) {
			PMD_DRV_LOG(ERR, "%d: gen of pkt failed\n", __LINE__);
			return -1;
		}

		ev.queue_id = t->qid[0];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = mbufs[i];
		*rte_event_pmd_selftest_seqn(mbufs[i]) = MAGIC_SEQN + i;

		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_port], &ev, 1);
		if (err != 1) {
			PMD_DRV_LOG(ERR, "%d: Failed to enqueue pkt %u, retval = %u\n",
					__LINE__, i, err);
			return -1;
		}
	}

	/* use extra slot to make logic in loops easier */
	struct rte_event deq_ev[w3_port + 1];

	uint32_t  seq  = 0;

	/* Dequeue the 3 packets, one from each worker port */
	for (i = w1_port; i <= w3_port; i++) {
		deq_pkts = rte_event_dequeue_burst(evdev, t->port[i],
				&deq_ev[i], 1, 0);
		if (deq_pkts != 1) {
			PMD_DRV_LOG(ERR, "%d: Failed to deq\n", __LINE__);
			rte_event_dev_dump(evdev, stdout);
			return -1;
		}
		seq = *rte_event_pmd_selftest_seqn(deq_ev[i].mbuf)  - MAGIC_SEQN;

		if (seq != (i-1)) {
			PMD_DRV_LOG(ERR, " seq test failed ! eq is %d , "
					"port number is %u\n", seq, i);
			return -1;
		}
	}

	/* Enqueue each packet in reverse order, flushing after each one */
	for (i = w3_port; i >= w1_port; i--) {

		deq_ev[i].op = RTE_EVENT_OP_FORWARD;
		deq_ev[i].queue_id = t->qid[1];
		err = rte_event_enqueue_burst(evdev, t->port[i], &deq_ev[i], 1);
		if (err != 1) {
			PMD_DRV_LOG(ERR, "%d: Failed to enqueue\n", __LINE__);
			return -1;
		}
	}

	/* dequeue from the tx ports, we should get 3 packets */
	deq_pkts = rte_event_dequeue_burst(evdev, t->port[tx_port], deq_ev,
			3, 0);

	/* Check to see if we've got all 3 packets */
	if (deq_pkts != 3) {
		PMD_DRV_LOG(ERR, "%d: expected 3 pkts at tx port got %d from port %d\n",
			__LINE__, deq_pkts, tx_port);
		rte_event_dev_dump(evdev, stdout);
		return 1;
	}

	/* Destroy the instance */
	cleanup(t);

	return 0;
}


static int
atomic_basic(struct test *t)
{
	const uint8_t rx_port = 0;
	const uint8_t w1_port = 1;
	const uint8_t w3_port = 3;
	const uint8_t tx_port = 4;
	int err;
	int i;
	uint32_t deq_pkts;
	struct rte_mbuf *mbufs[3];
	const uint32_t MAGIC_SEQN = 1234;

	/* Create instance with 5 ports */
	if (init(t, 2, tx_port+1) < 0 ||
	    create_ports(t, tx_port+1) < 0 ||
	    create_queues_type(t, 2, OPDL_Q_TYPE_ATOMIC)) {
		PMD_DRV_LOG(ERR, "%d: Error initializing device\n", __LINE__);
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
	/* CQ mapping to QID for Atomic  ports (directed mapped on create) */
	for (i = w1_port; i <= w3_port; i++) {
		err = rte_event_port_link(evdev, t->port[i], &t->qid[0], NULL,
				1);
		if (err != 1) {
			PMD_DRV_LOG(ERR, "%d: error mapping lb qid\n",
					__LINE__);
			cleanup(t);
			return -1;
		}
	}

	err = rte_event_port_link(evdev, t->port[tx_port], &t->qid[1], NULL,
			1);
	if (err != 1) {
		PMD_DRV_LOG(ERR, "%d: error mapping TX  qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) < 0) {
		PMD_DRV_LOG(ERR, "%d: Error with start call\n", __LINE__);
		return -1;
	}

	/* Enqueue 3 packets to the rx port */
	for (i = 0; i < 3; i++) {
		struct rte_event ev;
		mbufs[i] = rte_gen_arp(0, t->mbuf_pool);
		if (!mbufs[i]) {
			PMD_DRV_LOG(ERR, "%d: gen of pkt failed\n", __LINE__);
			return -1;
		}

		ev.queue_id = t->qid[0];
		ev.op = RTE_EVENT_OP_NEW;
		ev.flow_id = 1;
		ev.mbuf = mbufs[i];
		*rte_event_pmd_selftest_seqn(mbufs[i]) = MAGIC_SEQN + i;

		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_port], &ev, 1);
		if (err != 1) {
			PMD_DRV_LOG(ERR, "%d: Failed to enqueue pkt %u, retval = %u\n",
					__LINE__, i, err);
			return -1;
		}
	}

	/* use extra slot to make logic in loops easier */
	struct rte_event deq_ev[w3_port + 1];

	/* Dequeue the 3 packets, one from each worker port */
	for (i = w1_port; i <= w3_port; i++) {

		deq_pkts = rte_event_dequeue_burst(evdev, t->port[i],
				deq_ev, 3, 0);

		if (t->port[i] != 2) {
			if (deq_pkts != 0) {
				PMD_DRV_LOG(ERR, "%d: deq none zero !\n",
						__LINE__);
				rte_event_dev_dump(evdev, stdout);
				return -1;
			}
		} else {

			if (deq_pkts != 3) {
				PMD_DRV_LOG(ERR, "%d: deq not eqal to 3 %u !\n",
						__LINE__, deq_pkts);
				rte_event_dev_dump(evdev, stdout);
				return -1;
			}

			int j;
			for (j = 0; j < 3; j++) {
				deq_ev[j].op = RTE_EVENT_OP_FORWARD;
				deq_ev[j].queue_id = t->qid[1];
			}

			err = rte_event_enqueue_burst(evdev, t->port[i],
					deq_ev, 3);

			if (err != 3) {
				PMD_DRV_LOG(ERR, "port %d: Failed to enqueue pkt %u, "
						"retval = %u\n",
						t->port[i], 3, err);
				return -1;
			}

		}

	}


	/* dequeue from the tx ports, we should get 3 packets */
	deq_pkts = rte_event_dequeue_burst(evdev, t->port[tx_port], deq_ev,
			3, 0);

	/* Check to see if we've got all 3 packets */
	if (deq_pkts != 3) {
		PMD_DRV_LOG(ERR, "%d: expected 3 pkts at tx port got %d from port %d\n",
			__LINE__, deq_pkts, tx_port);
		rte_event_dev_dump(evdev, stdout);
		return 1;
	}

	cleanup(t);

	return 0;
}
static __rte_always_inline int
check_qid_stats(uint64_t id[], int index)
{

	if (index == 0) {
		if (id[0] != 3 || id[1] != 3
				|| id[2] != 3)
			return -1;
	} else if (index == 1) {
		if (id[0] != 5 || id[1] != 5
				|| id[2] != 2)
			return -1;
	} else if (index == 2) {
		if (id[0] != 3 || id[1] != 1
				|| id[2] != 1)
			return -1;
	}

	return 0;
}


static int
check_statistics(void)
{
	int num_ports = 3; /* Hard-coded for this app */
	int i;

	for (i = 0; i < num_ports; i++) {
		int num_stats, num_stats_returned;

		num_stats = rte_event_dev_xstats_names_get(0,
				RTE_EVENT_DEV_XSTATS_PORT,
				i,
				NULL,
				NULL,
				0);
		if (num_stats > 0) {

			uint64_t id[num_stats];
			struct rte_event_dev_xstats_name names[num_stats];
			uint64_t values[num_stats];

			num_stats_returned = rte_event_dev_xstats_names_get(0,
					RTE_EVENT_DEV_XSTATS_PORT,
					i,
					names,
					id,
					num_stats);

			if (num_stats == num_stats_returned) {
				num_stats_returned = rte_event_dev_xstats_get(0,
						RTE_EVENT_DEV_XSTATS_PORT,
						i,
						id,
						values,
						num_stats);

				if (num_stats == num_stats_returned) {
					int err;

					err = check_qid_stats(id, i);

					if (err)
						return err;

				} else {
					return -1;
				}
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	}
	return 0;
}

#define OLD_NUM_PACKETS 3
#define NEW_NUM_PACKETS 2
static int
single_link_w_stats(struct test *t)
{
	const uint8_t rx_port = 0;
	const uint8_t w1_port = 1;
	const uint8_t tx_port = 2;
	int err;
	int i;
	uint32_t deq_pkts;
	struct rte_mbuf *mbufs[3];
	RTE_SET_USED(mbufs);

	/* Create instance with 3 ports */
	if (init(t, 2, tx_port + 1) < 0 ||
	    create_ports(t, 3) < 0 || /* 0,1,2 */
	    create_queues_type(t, 1, OPDL_Q_TYPE_SINGLE_LINK) < 0 ||
	    create_queues_type(t, 1, OPDL_Q_TYPE_ORDERED) < 0) {
		PMD_DRV_LOG(ERR, "%d: Error initializing device\n", __LINE__);
		return -1;
	}


	/*
	 *
	 * Simplified test setup diagram:
	 *
	 * rx_port(0)
	 *           \
	 *            qid0 - w1_port(1) - qid1
	 *                                    \
	 *                                     tx_port(2)
	 */

	err = rte_event_port_link(evdev, t->port[1], &t->qid[0], NULL,
				  1);
	if (err != 1) {
		PMD_DRV_LOG(ERR, "%d: error linking port:[%u] to queue:[%u]\n",
		       __LINE__,
		       t->port[1],
		       t->qid[0]);
		cleanup(t);
		return -1;
	}

	err = rte_event_port_link(evdev, t->port[2], &t->qid[1], NULL,
				  1);
	if (err != 1) {
		PMD_DRV_LOG(ERR, "%d: error linking port:[%u] to queue:[%u]\n",
		       __LINE__,
		       t->port[2],
		       t->qid[1]);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) != 0) {
		PMD_DRV_LOG(ERR, "%d: failed to start device\n", __LINE__);
		cleanup(t);
		return -1;
	}

	/*
	 * Enqueue 3 packets to the rx port
	 */
	for (i = 0; i < 3; i++) {
		struct rte_event ev;
		mbufs[i] = rte_gen_arp(0, t->mbuf_pool);
		if (!mbufs[i]) {
			PMD_DRV_LOG(ERR, "%d: gen of pkt failed\n", __LINE__);
			return -1;
		}

		ev.queue_id = t->qid[0];
		ev.op = RTE_EVENT_OP_NEW;
		ev.mbuf = mbufs[i];
		*rte_event_pmd_selftest_seqn(mbufs[i]) = 1234 + i;

		/* generate pkt and enqueue */
		err = rte_event_enqueue_burst(evdev, t->port[rx_port], &ev, 1);
		if (err != 1) {
			PMD_DRV_LOG(ERR, "%d: Failed to enqueue pkt %u, retval = %u\n",
			       __LINE__,
			       t->port[rx_port],
			       err);
			return -1;
		}
	}

	/* Dequeue the 3 packets, from SINGLE_LINK worker port */
	struct rte_event deq_ev[3];

	deq_pkts = rte_event_dequeue_burst(evdev,
					   t->port[w1_port],
					   deq_ev, 3, 0);

	if (deq_pkts != 3) {
		PMD_DRV_LOG(ERR, "%d: deq not 3 !\n", __LINE__);
		cleanup(t);
		return -1;
	}

	/* Just enqueue 2 onto new ring */
	for (i = 0; i < NEW_NUM_PACKETS; i++)
		deq_ev[i].queue_id = t->qid[1];

	deq_pkts = rte_event_enqueue_burst(evdev,
					   t->port[w1_port],
					   deq_ev,
					   NEW_NUM_PACKETS);

	if (deq_pkts != 2) {
		PMD_DRV_LOG(ERR, "%d: enq not 2 but %u!\n", __LINE__, deq_pkts);
		cleanup(t);
		return -1;
	}

	/* dequeue from the tx ports, we should get 2 packets */
	deq_pkts = rte_event_dequeue_burst(evdev,
					   t->port[tx_port],
					   deq_ev,
					   3,
					   0);

	/* Check to see if we've got all 2 packets */
	if (deq_pkts != 2) {
		PMD_DRV_LOG(ERR, "%d: expected 2 pkts at tx port got %d from port %d\n",
			__LINE__, deq_pkts, tx_port);
		cleanup(t);
		return -1;
	}

	if (!check_statistics()) {
		PMD_DRV_LOG(ERR, "xstats check failed");
		cleanup(t);
		return -1;
	}

	cleanup(t);

	return 0;
}

static int
single_link(struct test *t)
{
	const uint8_t tx_port = 2;
	int err;
	struct rte_mbuf *mbufs[3];
	RTE_SET_USED(mbufs);

	/* Create instance with 5 ports */
	if (init(t, 2, tx_port+1) < 0 ||
	    create_ports(t, 3) < 0 || /* 0,1,2 */
	    create_queues_type(t, 1, OPDL_Q_TYPE_SINGLE_LINK) < 0 ||
	    create_queues_type(t, 1, OPDL_Q_TYPE_ORDERED) < 0) {
		PMD_DRV_LOG(ERR, "%d: Error initializing device\n", __LINE__);
		return -1;
	}


	/*
	 *
	 * Simplified test setup diagram:
	 *
	 * rx_port(0)
	 *           \
	 *            qid0 - w1_port(1) - qid1
	 *                                    \
	 *                                     tx_port(2)
	 */

	err = rte_event_port_link(evdev, t->port[1], &t->qid[0], NULL,
				  1);
	if (err != 1) {
		PMD_DRV_LOG(ERR, "%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	err = rte_event_port_link(evdev, t->port[2], &t->qid[0], NULL,
				  1);
	if (err != 1) {
		PMD_DRV_LOG(ERR, "%d: error mapping lb qid\n", __LINE__);
		cleanup(t);
		return -1;
	}

	if (rte_event_dev_start(evdev) == 0) {
		PMD_DRV_LOG(ERR, "%d: start DIDN'T FAIL with more than 1 "
				"SINGLE_LINK PORT\n", __LINE__);
		cleanup(t);
		return -1;
	}

	cleanup(t);

	return 0;
}


static __rte_always_inline void
populate_event_burst(struct rte_event ev[],
		     uint8_t qid,
		     uint16_t num_events)
{
	uint16_t i;
	for (i = 0; i < num_events; i++) {
		ev[i].flow_id = 1;
		ev[i].op = RTE_EVENT_OP_NEW;
		ev[i].sched_type = RTE_SCHED_TYPE_ORDERED;
		ev[i].queue_id = qid;
		ev[i].event_type = RTE_EVENT_TYPE_ETHDEV;
		ev[i].sub_event_type = 0;
		ev[i].priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
		ev[i].mbuf = (struct rte_mbuf *)0xdead0000;
	}
}

#define NUM_QUEUES 3
#define BATCH_SIZE 32

static int
qid_basic(struct test *t)
{
	int err = 0;

	uint8_t q_id = 0;
	uint8_t p_id = 0;

	uint32_t num_events;
	uint32_t i;

	struct rte_event ev[BATCH_SIZE];

	/* Create instance with 4 ports */
	if (init(t, NUM_QUEUES, NUM_QUEUES+1) < 0 ||
	    create_ports(t, NUM_QUEUES+1) < 0 ||
	    create_queues_type(t, NUM_QUEUES, OPDL_Q_TYPE_ORDERED)) {
		PMD_DRV_LOG(ERR, "%d: Error initializing device\n", __LINE__);
		return -1;
	}

	for (i = 0; i < NUM_QUEUES; i++) {
		int nb_linked;
		q_id = i;

		nb_linked = rte_event_port_link(evdev,
				i+1, /* port = q_id + 1*/
				&q_id,
				NULL,
				1);

		if (nb_linked != 1) {

			PMD_DRV_LOG(ERR, "%s:%d: error mapping port:%u to queue:%u\n",
					__FILE__,
					__LINE__,
					i + 1,
					q_id);

			err = -1;
			break;
		}

	}


	/* Try and link to the same port again */
	if (!err) {
		uint8_t t_qid = 0;
		if (rte_event_port_link(evdev,
					1,
					&t_qid,
					NULL,
					1) > 0) {
			PMD_DRV_LOG(ERR, "%s:%d: Second call to port link on same port DID NOT fail\n",
					__FILE__,
					__LINE__);
			err = -1;
		}

		uint32_t test_num_events;

		if (!err) {
			test_num_events = rte_event_dequeue_burst(evdev,
					p_id,
					ev,
					BATCH_SIZE,
					0);
			if (test_num_events != 0) {
				PMD_DRV_LOG(ERR, "%s:%d: Error dequeuing 0 packets from port %u on stopped device\n",
						__FILE__,
						__LINE__,
						p_id);
				err = -1;
			}
		}

		if (!err) {
			test_num_events = rte_event_enqueue_burst(evdev,
					p_id,
					ev,
					BATCH_SIZE);
			if (test_num_events != 0) {
				PMD_DRV_LOG(ERR, "%s:%d: Error enqueuing 0 packets to port %u on stopped device\n",
						__FILE__,
						__LINE__,
						p_id);
				err = -1;
			}
		}
	}


	/* Start the device */
	if (!err) {
		if (rte_event_dev_start(evdev) < 0) {
			PMD_DRV_LOG(ERR, "%s:%d: Error with start call\n",
					__FILE__,
					__LINE__);
			err = -1;
		}
	}


	/* Check we can't do any more links now that device is started.*/
	if (!err) {
		uint8_t t_qid = 0;
		if (rte_event_port_link(evdev,
					1,
					&t_qid,
					NULL,
					1) > 0) {
			PMD_DRV_LOG(ERR, "%s:%d: Call to port link on started device DID NOT fail\n",
					__FILE__,
					__LINE__);
			err = -1;
		}
	}

	if (!err) {

		q_id = 0;

		populate_event_burst(ev,
				q_id,
				BATCH_SIZE);

		num_events = rte_event_enqueue_burst(evdev,
				p_id,
				ev,
				BATCH_SIZE);
		if (num_events != BATCH_SIZE) {
			PMD_DRV_LOG(ERR, "%s:%d: Error enqueuing rx packets\n",
					__FILE__,
					__LINE__);
			err = -1;
		}
	}

	if (!err) {
		while (++p_id < NUM_QUEUES) {

			num_events = rte_event_dequeue_burst(evdev,
					p_id,
					ev,
					BATCH_SIZE,
					0);

			if (num_events != BATCH_SIZE) {
				PMD_DRV_LOG(ERR, "%s:%d: Error dequeuing packets from port %u\n",
						__FILE__,
						__LINE__,
						p_id);
				err = -1;
				break;
			}

			if (ev[0].queue_id != q_id) {
				PMD_DRV_LOG(ERR, "%s:%d: Error event portid[%u] q_id:[%u] does not match expected:[%u]\n",
						__FILE__,
						__LINE__,
						p_id,
						ev[0].queue_id,
						q_id);
				err = -1;
				break;
			}

			populate_event_burst(ev,
					++q_id,
					BATCH_SIZE);

			num_events = rte_event_enqueue_burst(evdev,
					p_id,
					ev,
					BATCH_SIZE);
			if (num_events != BATCH_SIZE) {
				PMD_DRV_LOG(ERR, "%s:%d: Error enqueuing packets from port:%u to queue:%u\n",
						__FILE__,
						__LINE__,
						p_id,
						q_id);
				err = -1;
				break;
			}
		}
	}

	if (!err) {
		num_events = rte_event_dequeue_burst(evdev,
				p_id,
				ev,
				BATCH_SIZE,
				0);
		if (num_events != BATCH_SIZE) {
			PMD_DRV_LOG(ERR, "%s:%d: Error dequeuing packets from tx port %u\n",
					__FILE__,
					__LINE__,
					p_id);
			err = -1;
		}
	}

	cleanup(t);

	return err;
}



int
opdl_selftest(void)
{
	struct test *t = malloc(sizeof(struct test));
	int ret;

	const char *eventdev_name = "event_opdl0";

	evdev = rte_event_dev_get_dev_id(eventdev_name);

	if (evdev < 0) {
		PMD_DRV_LOG(ERR, "%d: Eventdev %s not found - creating.\n",
				__LINE__, eventdev_name);
		/* turn on stats by default */
		if (rte_vdev_init(eventdev_name, "do_validation=1") < 0) {
			PMD_DRV_LOG(ERR, "Error creating eventdev\n");
			free(t);
			return -1;
		}
		evdev = rte_event_dev_get_dev_id(eventdev_name);
		if (evdev < 0) {
			PMD_DRV_LOG(ERR, "Error finding newly created eventdev\n");
			free(t);
			return -1;
		}
	}

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
			PMD_DRV_LOG(ERR, "ERROR creating mempool\n");
			free(t);
			return -1;
		}
	}
	t->mbuf_pool = eventdev_func_mempool;

	PMD_DRV_LOG(ERR, "*** Running Ordered Basic test...\n");
	ret = ordered_basic(t);

	PMD_DRV_LOG(ERR, "*** Running Atomic Basic test...\n");
	ret = atomic_basic(t);


	PMD_DRV_LOG(ERR, "*** Running QID  Basic test...\n");
	ret = qid_basic(t);

	PMD_DRV_LOG(ERR, "*** Running SINGLE LINK failure test...\n");
	ret = single_link(t);

	PMD_DRV_LOG(ERR, "*** Running SINGLE LINK w stats test...\n");
	ret = single_link_w_stats(t);

	/*
	 * Free test instance, free  mempool
	 */
	rte_mempool_free(t->mbuf_pool);
	free(t);

	if (ret != 0)
		return ret;
	return 0;

}
