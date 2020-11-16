/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_eventdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "dlb_priv.h"
#include "rte_pmd_dlb.h"

#define MAX_PORTS 32
#define MAX_QIDS 32
#define DEFAULT_NUM_SEQ_NUMS 32

static struct rte_mempool *eventdev_func_mempool;
static int evdev;

struct test {
	struct rte_mempool *mbuf_pool;
	int nb_qids;
};

/* initialization and config */
static inline int
init(struct test *t, int nb_queues, int nb_ports)
{
	struct rte_event_dev_config config = {0};
	struct rte_event_dev_info info;
	int ret;

	memset(t, 0, sizeof(*t));

	t->mbuf_pool = eventdev_func_mempool;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = nb_queues;
	config.nb_event_ports = nb_ports;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0)
		printf("%d: Error configuring device\n", __LINE__);

	return ret;
}

static inline int
create_ports(int num_ports)
{
	int i;

	if (num_ports > MAX_PORTS)
		return -1;

	for (i = 0; i < num_ports; i++) {
		struct rte_event_port_conf conf;

		if (rte_event_port_default_conf_get(evdev, i, &conf)) {
			printf("%d: Error querying default port conf\n",
			       __LINE__);
			return -1;
		}

		if (rte_event_port_setup(evdev, i, &conf) < 0) {
			printf("%d: Error setting up port %d\n", __LINE__, i);
			return -1;
		}
	}

	return 0;
}

static inline int
create_lb_qids(struct test *t, int num_qids, uint32_t flags)
{
	int i;

	for (i = t->nb_qids; i < t->nb_qids + num_qids; i++) {
		struct rte_event_queue_conf conf;

		if (rte_event_queue_default_conf_get(evdev, i, &conf)) {
			printf("%d: Error querying default queue conf\n",
			       __LINE__);
			return -1;
		}

		conf.schedule_type = flags;

		if (conf.schedule_type == RTE_SCHED_TYPE_PARALLEL)
			conf.nb_atomic_order_sequences = 0;
		else
			conf.nb_atomic_order_sequences = DEFAULT_NUM_SEQ_NUMS;

		if (rte_event_queue_setup(evdev, i, &conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
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

/* destruction */
static inline int
cleanup(void)
{
	rte_event_dev_stop(evdev);
	return rte_event_dev_close(evdev);
};

static inline int
enqueue_timeout(uint8_t port_id, struct rte_event *ev, uint64_t tmo_us)
{
	const uint64_t start = rte_get_timer_cycles();
	const uint64_t ticks = (tmo_us * rte_get_timer_hz()) / 1E6;

	while ((rte_get_timer_cycles() - start) < ticks) {
		if (rte_event_enqueue_burst(evdev, port_id, ev, 1) == 1)
			return 0;

		if (rte_errno != -ENOSPC)
			return -1;
	}

	return -1;
}

static void
flush(uint8_t id __rte_unused, struct rte_event event, void *arg __rte_unused)
{
	rte_pktmbuf_free(event.mbuf);
}

static int
test_stop_flush(struct test *t) /* test to check we can properly flush events */
{
	struct rte_event ev;
	uint32_t dequeue_depth;
	unsigned int i, count;
	uint8_t queue_id;

	ev.op = RTE_EVENT_OP_NEW;

	if (init(t, 2, 1) < 0 ||
	    create_ports(1) < 0 ||
	    create_atomic_qids(t, 2) < 0) {
		printf("%d: Error initializing device\n", __LINE__);
		return -1;
	}

	if (rte_event_port_link(evdev, 0, NULL, NULL, 0) != 2) {
		printf("%d: Error linking queues to the port\n", __LINE__);
		goto err;
	}

	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: Error with start call\n", __LINE__);
		goto err;
	}

	/* Unlink queue 1 so the PMD's stop callback has to cleanup an unlinked
	 * queue.
	 */
	queue_id = 1;

	if (rte_event_port_unlink(evdev, 0, &queue_id, 1) != 1) {
		printf("%d: Error unlinking queue 1 from port\n", __LINE__);
		goto err;
	}

	if (t->mbuf_pool)
		count = rte_mempool_avail_count(t->mbuf_pool);
	else {
		printf("%d: mbuf_pool is NULL\n", __LINE__);
		goto err;
	}

	if (rte_event_port_attr_get(evdev,
				    0,
				    RTE_EVENT_PORT_ATTR_DEQ_DEPTH,
				    &dequeue_depth)) {
		printf("%d: Error retrieveing dequeue depth\n", __LINE__);
		goto err;
	}

	/* Send QEs to queue 0 */
	for (i = 0; i < dequeue_depth + 1; i++) {
		ev.mbuf = rte_pktmbuf_alloc(t->mbuf_pool);
		ev.queue_id = 0;
		ev.sched_type = RTE_SCHED_TYPE_ATOMIC;

		if (enqueue_timeout(0, &ev, 1000)) {
			printf("%d: Error enqueuing events\n", __LINE__);
			goto err;
		}
	}

	/* Send QEs to queue 1 */
	for (i = 0; i < dequeue_depth + 1; i++) {
		ev.mbuf = rte_pktmbuf_alloc(t->mbuf_pool);
		ev.queue_id = 1;
		ev.sched_type = RTE_SCHED_TYPE_ATOMIC;

		if (enqueue_timeout(0, &ev, 1000)) {
			printf("%d: Error enqueuing events\n", __LINE__);
			goto err;
		}
	}

	/* Now the DLB is scheduling events from the port to the IQ, and at
	 * least one event should be remaining in each queue.
	 */

	if (rte_event_dev_stop_flush_callback_register(evdev, flush, NULL)) {
		printf("%d: Error installing the flush callback\n", __LINE__);
		goto err;
	}

	cleanup();

	if (count != rte_mempool_avail_count(t->mbuf_pool)) {
		printf("%d: Error executing the flush callback\n", __LINE__);
		goto err;
	}

	if (rte_event_dev_stop_flush_callback_register(evdev, NULL, NULL)) {
		printf("%d: Error uninstalling the flush callback\n", __LINE__);
		goto err;
	}

	return 0;
err:
	cleanup();
	return -1;
}

static int
test_single_link(void)
{
	struct rte_event_dev_config config = {0};
	struct rte_event_queue_conf queue_conf;
	struct rte_event_port_conf port_conf;
	struct rte_event_dev_info info;
	uint8_t queue_id;
	int ret;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = 2;
	config.nb_event_ports = 2;
	config.nb_single_link_event_port_queues = 1;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	/* Create a directed port */
	if (rte_event_port_default_conf_get(evdev, 0, &port_conf)) {
		printf("%d: Error querying default port conf\n", __LINE__);
		goto err;
	}

	port_conf.event_port_cfg = RTE_EVENT_PORT_CFG_SINGLE_LINK;

	if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
		printf("%d: port 0 setup expected to succeed\n", __LINE__);
		goto err;
	}

	/* Attempt to create another directed port */
	if (rte_event_port_setup(evdev, 1, &port_conf) == 0) {
		printf("%d: port 1 setup expected to fail\n", __LINE__);
		goto err;
	}

	port_conf.event_port_cfg = 0;

	/* Create a load-balanced port */
	if (rte_event_port_setup(evdev, 1, &port_conf) < 0) {
		printf("%d: port 1 setup expected to succeed\n", __LINE__);
		goto err;
	}

	/* Create a directed queue */
	if (rte_event_queue_default_conf_get(evdev, 0, &queue_conf)) {
		printf("%d: Error querying default queue conf\n", __LINE__);
		goto err;
	}

	queue_conf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	if (rte_event_queue_setup(evdev, 0, &queue_conf) < 0) {
		printf("%d: queue 0 setup expected to succeed\n", __LINE__);
		goto err;
	}

	/* Attempt to create another directed queue */
	if (rte_event_queue_setup(evdev, 1, &queue_conf) == 0) {
		printf("%d: queue 1 setup expected to fail\n", __LINE__);
		goto err;
	}

	/* Create a load-balanced queue */
	queue_conf.event_queue_cfg = 0;

	if (rte_event_queue_setup(evdev, 1, &queue_conf) < 0) {
		printf("%d: queue 1 setup expected to succeed\n", __LINE__);
		goto err;
	}

	/* Attempt to link directed and load-balanced resources */
	queue_id = 1;
	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) == 1) {
		printf("%d: port 0 link expected to fail\n", __LINE__);
		goto err;
	}

	queue_id = 0;
	if (rte_event_port_link(evdev, 1, &queue_id, NULL, 1) == 1) {
		printf("%d: port 1 link expected to fail\n", __LINE__);
		goto err;
	}

	/* Link ports to queues */
	queue_id = 0;
	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: port 0 link expected to succeed\n", __LINE__);
		goto err;
	}

	queue_id = 1;
	if (rte_event_port_link(evdev, 1, &queue_id, NULL, 1) != 1) {
		printf("%d: port 1 link expected to succeed\n", __LINE__);
		goto err;
	}

	return rte_event_dev_close(evdev);

err:
	rte_event_dev_close(evdev);
	return -1;
}

#define NUM_LDB_PORTS 64
#define NUM_LDB_QUEUES 128

static int
test_info_get(void)
{
	struct rte_event_dev_config config = {0};
	struct rte_event_dev_info info;
	int ret;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	if (info.max_event_ports != NUM_LDB_PORTS) {
		printf("%d: Got %u ports, expected %u\n",
		       __LINE__, info.max_event_ports, NUM_LDB_PORTS);
		goto err;
	}

	if (info.max_event_queues != NUM_LDB_QUEUES) {
		printf("%d: Got %u queues, expected %u\n",
		       __LINE__, info.max_event_queues, NUM_LDB_QUEUES);
		goto err;
	}

	config.nb_event_ports = info.max_event_ports;
	config.nb_event_queues = NUM_LDB_QUEUES + info.max_event_ports / 2;
	config.nb_single_link_event_port_queues = info.max_event_ports / 2;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		goto err;
	}

	/* The DLB PMD only reports load-balanced ports and queues in its
	 * info_get function. Confirm that these values don't include the
	 * directed port or queue counts.
	 */

	if (info.max_event_ports != NUM_LDB_PORTS) {
		printf("%d: Got %u ports, expected %u\n",
		       __LINE__, info.max_event_ports, NUM_LDB_PORTS);
		goto err;
	}

	if (info.max_event_queues != NUM_LDB_QUEUES) {
		printf("%d: Got %u queues, expected %u\n",
		       __LINE__, info.max_event_queues, NUM_LDB_QUEUES);
		goto err;
	}

	ret = rte_event_dev_close(evdev);
	if (ret) {
		printf("rte_event_dev_close err %d\n", ret);
		goto err;
	}

	return 0;

err:
	rte_event_dev_close(evdev);
	return -1;
}

static int
test_reconfiguration_link(void)
{
	struct rte_event_dev_config config = {0};
	struct rte_event_queue_conf queue_conf;
	struct rte_event_port_conf port_conf;
	struct rte_event_dev_info info;
	uint8_t queue_id;
	int ret, i;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = 2;
	config.nb_event_ports = 2;
	config.nb_single_link_event_port_queues = 0;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	/* Configure the device with 2 LDB ports and 2 LDB queues */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	/* Configure the ports and queues */
	if (rte_event_port_default_conf_get(evdev, 0, &port_conf)) {
		printf("%d: Error querying default port conf\n", __LINE__);
		goto err;
	}

	for (i = 0; i < 2; i++) {
		if (rte_event_port_setup(evdev, i, &port_conf) < 0) {
			printf("%d: port %d setup expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	if (rte_event_queue_default_conf_get(evdev, 0, &queue_conf)) {
		printf("%d: Error querying default queue conf\n", __LINE__);
		goto err;
	}

	for (i = 0; i < 2; i++) {
		if (rte_event_queue_setup(evdev, i, &queue_conf) < 0) {
			printf("%d: queue %d setup expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	/* Link P0->Q0 and P1->Q1 */
	for (i = 0; i < 2; i++) {
		queue_id = i;

		if (rte_event_port_link(evdev, i, &queue_id, NULL, 1) != 1) {
			printf("%d: port %d link expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Stop the device */
	rte_event_dev_stop(evdev);

	/* Reconfigure device */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error re-configuring device\n", __LINE__);
		return -1;
	}

	/* Configure P1 and Q1, leave P0 and Q0 to be configured by the PMD. */
	if (rte_event_port_setup(evdev, 1, &port_conf) < 0) {
		printf("%d: port 1 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_queue_setup(evdev, 1, &queue_conf) < 0) {
		printf("%d: queue 1 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Link P0->Q0 and Q1 */
	for (i = 0; i < 2; i++) {
		queue_id = i;

		if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
			printf("%d: P0->Q%d link expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	/* Link P1->Q0 and Q1 */
	for (i = 0; i < 2; i++) {
		queue_id = i;

		if (rte_event_port_link(evdev, 1, &queue_id, NULL, 1) != 1) {
			printf("%d: P1->Q%d link expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Stop the device */
	rte_event_dev_stop(evdev);

	/* Configure device with 2 DIR ports and 2 DIR queues */
	config.nb_single_link_event_port_queues = 2;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	/* Configure the ports and queues */
	port_conf.event_port_cfg = RTE_EVENT_PORT_CFG_SINGLE_LINK;

	for (i = 0; i < 2; i++) {
		if (rte_event_port_setup(evdev, i, &port_conf) < 0) {
			printf("%d: port %d setup expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	queue_conf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	for (i = 0; i < 2; i++) {
		if (rte_event_queue_setup(evdev, i, &queue_conf) < 0) {
			printf("%d: queue %d setup expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	/* Link P0->Q0 and P1->Q1 */
	for (i = 0; i < 2; i++) {
		queue_id = i;

		if (rte_event_port_link(evdev, i, &queue_id, NULL, 1) != 1) {
			printf("%d: port %d link expected to succeed\n",
			       __LINE__, i);
			goto err;
		}
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Stop the device */
	rte_event_dev_stop(evdev);

	/* Reconfigure device */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error re-configuring device\n", __LINE__);
		return -1;
	}

	/* Configure P1 and Q0, leave P0 and Q1 to be configured by the PMD. */
	if (rte_event_port_setup(evdev, 1, &port_conf) < 0) {
		printf("%d: port 1 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_queue_setup(evdev, 0, &queue_conf) < 0) {
		printf("%d: queue 1 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Link P0->Q1 */
	queue_id = 1;

	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: P0->Q%d link expected to succeed\n",
		       __LINE__, i);
		goto err;
	}

	/* Link P1->Q0 */
	queue_id = 0;

	if (rte_event_port_link(evdev, 1, &queue_id, NULL, 1) != 1) {
		printf("%d: P1->Q%d link expected to succeed\n",
		       __LINE__, i);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	rte_event_dev_stop(evdev);

	config.nb_event_queues = 5;
	config.nb_event_ports = 5;
	config.nb_single_link_event_port_queues = 1;

	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error re-configuring device\n", __LINE__);
		return -1;
	}

	for (i = 0; i < config.nb_event_queues - 1; i++) {
		port_conf.event_port_cfg = 0;
		queue_conf.event_queue_cfg = 0;

		if (rte_event_port_setup(evdev, i, &port_conf) < 0) {
			printf("%d: port %d setup expected to succeed\n",
			       __LINE__, i);
			goto err;
		}

		if (rte_event_queue_setup(evdev, i, &queue_conf) < 0) {
			printf("%d: queue %d setup expected to succeed\n",
			       __LINE__, i);
			goto err;
		}

		queue_id = i;

		if (rte_event_port_link(evdev, i, &queue_id, NULL, 1) != 1) {
			printf("%d: P%d->Q%d link expected to succeed\n",
			       __LINE__, i, i);
			goto err;
		}
	}

	port_conf.event_port_cfg = RTE_EVENT_PORT_CFG_SINGLE_LINK;
	queue_conf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	if (rte_event_port_setup(evdev, i, &port_conf) < 0) {
		printf("%d: port %d setup expected to succeed\n",
		       __LINE__, i);
		goto err;
	}

	if (rte_event_queue_setup(evdev, i, &queue_conf) < 0) {
		printf("%d: queue %d setup expected to succeed\n",
		       __LINE__, i);
		goto err;
	}

	queue_id = i;

	if (rte_event_port_link(evdev, i, &queue_id, NULL, 1) != 1) {
		printf("%d: P%d->Q%d link expected to succeed\n",
		       __LINE__, i, i);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Stop the device */
	rte_event_dev_stop(evdev);

	config.nb_event_ports += 1;

	/* Reconfigure device with 1 more load-balanced port */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error re-configuring device\n", __LINE__);
		return -1;
	}

	port_conf.event_port_cfg = 0;

	/* Configure the new port */
	if (rte_event_port_setup(evdev, config.nb_event_ports - 1,
				 &port_conf) < 0) {
		printf("%d: port 1 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	cleanup();
	return 0;

err:
	cleanup();
	return -1;
}

static int
test_load_balanced_traffic(void)
{
	uint64_t timeout;
	struct rte_event_dev_config config = {0};
	struct rte_event_queue_conf queue_conf;
	struct rte_event_port_conf port_conf;
	struct rte_event_dev_info info;
	struct rte_event ev;
	uint8_t queue_id;
	int ret;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = 1;
	config.nb_event_ports = 1;
	config.nb_single_link_event_port_queues = 0;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	/* Configure the device with 1 LDB port and queue */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	/* Configure the ports and queues */
	if (rte_event_port_default_conf_get(evdev, 0, &port_conf)) {
		printf("%d: Error querying default port conf\n", __LINE__);
		goto err;
	}

	if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
		printf("%d: port 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_queue_default_conf_get(evdev, 0, &queue_conf)) {
		printf("%d: Error querying default queue conf\n", __LINE__);
		goto err;
	}

	if (rte_event_queue_setup(evdev, 0, &queue_conf) < 0) {
		printf("%d: queue 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Link P0->Q0 */
	queue_id = 0;

	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: port 0 link expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Enqueue 1 NEW event */
	ev.op = RTE_EVENT_OP_NEW;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.queue_id = 0;
	ev.priority = 0;
	ev.u64 = 0;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: NEW enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Dequeue and enqueue 1 FORWARD event */
	timeout = 0xFFFFFFFFF;
	if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
		printf("%d: event dequeue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	ev.op = RTE_EVENT_OP_FORWARD;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: NEW enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Dequeue and enqueue 1 RELEASE operation */
	if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
		printf("%d: event dequeue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	ev.op = RTE_EVENT_OP_RELEASE;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: NEW enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	cleanup();
	return 0;

err:
	cleanup();
	return -1;
}

static int
test_directed_traffic(void)
{
	uint64_t timeout;
	struct rte_event_dev_config config = {0};
	struct rte_event_queue_conf queue_conf;
	struct rte_event_port_conf port_conf;
	struct rte_event_dev_info info;
	struct rte_event ev;
	uint8_t queue_id;
	int ret;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = 1;
	config.nb_event_ports = 1;
	config.nb_single_link_event_port_queues = 1;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	/* Configure the device with 1 DIR port and queue */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	/* Configure the ports and queues */
	if (rte_event_port_default_conf_get(evdev, 0, &port_conf)) {
		printf("%d: Error querying default port conf\n", __LINE__);
		goto err;
	}

	port_conf.event_port_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
		printf("%d: port 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_queue_default_conf_get(evdev, 0, &queue_conf)) {
		printf("%d: Error querying default queue conf\n", __LINE__);
		goto err;
	}

	queue_conf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_SINGLE_LINK;

	if (rte_event_queue_setup(evdev, 0, &queue_conf) < 0) {
		printf("%d: queue 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Link P0->Q0 */
	queue_id = 0;

	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: port 0 link expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Enqueue 1 NEW event */
	ev.op = RTE_EVENT_OP_NEW;
	ev.queue_id = 0;
	ev.priority = 0;
	ev.u64 = 0;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: NEW enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Dequeue and enqueue 1 FORWARD event */
	timeout = 0xFFFFFFFFF;
	if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
		printf("%d: event dequeue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (ev.queue_id != 0) {
		printf("%d: invalid dequeued event queue ID (%d)\n",
		       __LINE__, ev.queue_id);
		goto err;
	}

	ev.op = RTE_EVENT_OP_FORWARD;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: NEW enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Dequeue and enqueue 1 RELEASE operation */
	if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
		printf("%d: event dequeue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	ev.op = RTE_EVENT_OP_RELEASE;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: NEW enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	cleanup();
	return 0;

err:
	cleanup();
	return -1;
}

static int
test_deferred_sched(void)
{
	uint64_t timeout;
	struct rte_event_dev_config config = {0};
	struct rte_event_queue_conf queue_conf;
	struct rte_event_port_conf port_conf;
	struct rte_event_dev_info info;
	const int num_events = 128;
	struct rte_event ev;
	uint8_t queue_id;
	int ret, i;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = 1;
	config.nb_event_ports = 2;
	config.nb_single_link_event_port_queues = 0;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	/* Configure the device with 2 LDB ports and 1 queue */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	ret = rte_pmd_dlb_set_token_pop_mode(evdev, 0, DEFERRED_POP);
	if (ret < 0) {
		printf("%d: Error setting deferred scheduling\n", __LINE__);
		goto err;
	}

	ret = rte_pmd_dlb_set_token_pop_mode(evdev, 1, DEFERRED_POP);
	if (ret < 0) {
		printf("%d: Error setting deferred scheduling\n", __LINE__);
		goto err;
	}

	/* Configure the ports and queues */
	if (rte_event_port_default_conf_get(evdev, 0, &port_conf)) {
		printf("%d: Error querying default port conf\n", __LINE__);
		goto err;
	}

	port_conf.dequeue_depth = 1;

	if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
		printf("%d: port 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_port_setup(evdev, 1, &port_conf) < 0) {
		printf("%d: port 1 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_queue_default_conf_get(evdev, 0, &queue_conf)) {
		printf("%d: Error querying default queue conf\n", __LINE__);
		goto err;
	}

	queue_conf.schedule_type = RTE_SCHED_TYPE_PARALLEL;
	queue_conf.nb_atomic_order_sequences = 0;

	if (rte_event_queue_setup(evdev, 0, &queue_conf) < 0) {
		printf("%d: queue 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Link P0->Q0 and P1->Q0 */
	queue_id = 0;

	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: port 0 link expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_port_link(evdev, 1, &queue_id, NULL, 1) != 1) {
		printf("%d: port 1 link expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	/* Enqueue 128 NEW events */
	ev.op = RTE_EVENT_OP_NEW;
	ev.sched_type = RTE_SCHED_TYPE_PARALLEL;
	ev.queue_id = 0;
	ev.priority = 0;
	ev.u64 = 0;

	for (i = 0; i < num_events; i++) {
		if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
			printf("%d: NEW enqueue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	/* Dequeue two events from port 0 (dequeue_depth * 2 due to the
	 * reserved token scheme)
	 */
	timeout = 0xFFFFFFFFF;
	if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
		printf("%d: event dequeue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
		printf("%d: event dequeue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Dequeue (and release) all other events from port 1. Deferred
	 * scheduling ensures no other events are scheduled to port 0 without a
	 * subsequent rte_event_dequeue_burst() call.
	 */
	for (i = 0; i < num_events - 2; i++) {
		if (rte_event_dequeue_burst(evdev, 1, &ev, 1, timeout) != 1) {
			printf("%d: event dequeue expected to succeed\n",
			       __LINE__);
			goto err;
		}

		ev.op = RTE_EVENT_OP_RELEASE;

		if (rte_event_enqueue_burst(evdev, 1, &ev, 1) != 1) {
			printf("%d: RELEASE enqueue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	cleanup();
	return 0;

err:
	cleanup();
	return -1;
}

static int
test_delayed_pop(void)
{
	uint64_t timeout;
	struct rte_event_dev_config config = {0};
	struct rte_event_queue_conf queue_conf;
	struct rte_event_port_conf port_conf;
	struct rte_event_dev_info info;
	int ret, i, num_events;
	struct rte_event ev;
	uint8_t queue_id;

	if (rte_event_dev_info_get(evdev, &info)) {
		printf("%d: Error querying device info\n", __LINE__);
		return -1;
	}

	config.nb_event_queues = 1;
	config.nb_event_ports = 1;
	config.nb_single_link_event_port_queues = 0;
	config.nb_event_queue_flows = info.max_event_queue_flows;
	config.nb_events_limit = info.max_num_events;
	config.nb_event_port_dequeue_depth = info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth = info.max_event_port_enqueue_depth;
	config.dequeue_timeout_ns = info.max_dequeue_timeout_ns;
	config.event_dev_cfg = RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT;

	/* Configure the device with 1 LDB port and queue */
	ret = rte_event_dev_configure(evdev, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	ret = rte_pmd_dlb_set_token_pop_mode(evdev, 0, DELAYED_POP);
	if (ret < 0) {
		printf("%d: Error setting deferred scheduling\n", __LINE__);
		goto err;
	}

	/* Configure the ports and queues */
	if (rte_event_port_default_conf_get(evdev, 0, &port_conf)) {
		printf("%d: Error querying default port conf\n", __LINE__);
		goto err;
	}

	port_conf.dequeue_depth = 16;
	port_conf.event_port_cfg = RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL;

	if (rte_event_port_setup(evdev, 0, &port_conf) < 0) {
		printf("%d: port 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	if (rte_event_queue_default_conf_get(evdev, 0, &queue_conf)) {
		printf("%d: Error querying default queue conf\n", __LINE__);
		goto err;
	}

	if (rte_event_queue_setup(evdev, 0, &queue_conf) < 0) {
		printf("%d: queue 0 setup expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Link P0->Q0 */
	queue_id = 0;

	if (rte_event_port_link(evdev, 0, &queue_id, NULL, 1) != 1) {
		printf("%d: port 0 link expected to succeed\n",
		       __LINE__);
		goto err;
	}

	/* Start the device */
	if (rte_event_dev_start(evdev) < 0) {
		printf("%d: device start failed\n", __LINE__);
		goto err;
	}

	num_events = 2 * port_conf.dequeue_depth;

	/* Enqueue 2 * dequeue_depth NEW events. Due to the PMD's reserved
	 * token scheme, the port will initially behave as though its
	 * dequeue_depth is twice the requested size.
	 */
	ev.op = RTE_EVENT_OP_NEW;
	ev.sched_type = RTE_SCHED_TYPE_PARALLEL;
	ev.queue_id = 0;
	ev.priority = 0;
	ev.u64 = 0;

	for (i = 0; i < num_events; i++) {
		if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
			printf("%d: NEW enqueue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	/* Flush these events out of the CQ */
	timeout = 0xFFFFFFFFF;

	for (i = 0; i < num_events; i++) {
		if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
			printf("%d: event dequeue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	ev.op = RTE_EVENT_OP_RELEASE;

	for (i = 0; i < num_events; i++) {
		if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
			printf("%d: RELEASE enqueue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	/* Enqueue 2 * dequeue_depth NEW events again */
	ev.op = RTE_EVENT_OP_NEW;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.queue_id = 0;
	ev.priority = 0;
	ev.u64 = 0;

	for (i = 0; i < num_events; i++) {
		if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
			printf("%d: NEW enqueue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	/* Dequeue dequeue_depth events but only release dequeue_depth - 1.
	 * Delayed pop won't perform the pop and no more events will be
	 * scheduled.
	 */
	for (i = 0; i < port_conf.dequeue_depth; i++) {
		if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
			printf("%d: event dequeue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	ev.op = RTE_EVENT_OP_RELEASE;

	for (i = 0; i < port_conf.dequeue_depth - 1; i++) {
		if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
			printf("%d: RELEASE enqueue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	timeout = 0x10000;

	ret = rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout);
	if (ret != 0) {
		printf("%d: event dequeue expected to fail (ret = %d)\n",
		       __LINE__, ret);
		goto err;
	}

	/* Release one more event. This will trigger the token pop, and
	 * another batch of events will be scheduled to the device.
	 */
	ev.op = RTE_EVENT_OP_RELEASE;

	if (rte_event_enqueue_burst(evdev, 0, &ev, 1) != 1) {
		printf("%d: RELEASE enqueue expected to succeed\n",
		       __LINE__);
		goto err;
	}

	timeout = 0xFFFFFFFFF;

	for (i = 0; i < port_conf.dequeue_depth; i++) {
		if (rte_event_dequeue_burst(evdev, 0, &ev, 1, timeout) != 1) {
			printf("%d: event dequeue expected to succeed\n",
			       __LINE__);
			goto err;
		}
	}

	cleanup();
	return 0;

err:
	cleanup();
	return -1;
}

static int
do_selftest(void)
{
	struct test t;
	int ret;

	/* Only create mbuf pool once, reuse for each test run */
	if (!eventdev_func_mempool) {
		eventdev_func_mempool =
			rte_pktmbuf_pool_create("EVENTDEV_DLB_SA_MBUF_POOL",
						(1 << 12), /* 4k buffers */
						32 /*MBUF_CACHE_SIZE*/,
						0,
						512, /* use very small mbufs */
						rte_socket_id());
		if (!eventdev_func_mempool) {
			printf("ERROR creating mempool\n");
			goto test_fail;
		}
	}
	t.mbuf_pool = eventdev_func_mempool;

	printf("*** Running Stop Flush test...\n");
	ret = test_stop_flush(&t);
	if (ret != 0) {
		printf("ERROR - Stop Flush test FAILED.\n");
		return ret;
	}

	printf("*** Running Single Link test...\n");
	ret = test_single_link();
	if (ret != 0) {
		printf("ERROR - Single Link test FAILED.\n");

		goto test_fail;
	}

	printf("*** Running Info Get test...\n");
	ret = test_info_get();
	if (ret != 0) {
		printf("ERROR - Stop Flush test FAILED.\n");
		return ret;
	}

	printf("*** Running Reconfiguration Link test...\n");
	ret = test_reconfiguration_link();
	if (ret != 0) {
		printf("ERROR - Reconfiguration Link test FAILED.\n");

		goto test_fail;
	}

	printf("*** Running Load-Balanced Traffic test...\n");
	ret = test_load_balanced_traffic();
	if (ret != 0) {
		printf("ERROR - Load-Balanced Traffic test FAILED.\n");

		goto test_fail;
	}

	printf("*** Running Directed Traffic test...\n");
	ret = test_directed_traffic();
	if (ret != 0) {
		printf("ERROR - Directed Traffic test FAILED.\n");

		goto test_fail;
	}

	printf("*** Running Deferred Scheduling test...\n");
	ret = test_deferred_sched();
	if (ret != 0) {
		printf("ERROR - Deferred Scheduling test FAILED.\n");

		goto test_fail;
	}

	printf("*** Running Delayed Pop test...\n");
	ret = test_delayed_pop();
	if (ret != 0) {
		printf("ERROR - Delayed Pop test FAILED.\n");

		goto test_fail;
	}

	return 0;

test_fail:
	return -1;
}

int
test_dlb_eventdev(void)
{
	const char *dlb_eventdev_name = "dlb_event";
	uint8_t num_evdevs = rte_event_dev_count();
	int i, ret = 0;
	int found = 0, skipped = 0, passed = 0, failed = 0;
	struct rte_event_dev_info info;

	for (i = 0; found + skipped < num_evdevs && i < RTE_EVENT_MAX_DEVS;
	     i++) {
		ret = rte_event_dev_info_get(i, &info);
		if (ret < 0)
			continue;

		/* skip non-dlb event devices */
		if (strncmp(info.driver_name, dlb_eventdev_name,
			    sizeof(*info.driver_name)) != 0) {
			skipped++;
			continue;
		}

		evdev = rte_event_dev_get_dev_id(info.driver_name);
		if (evdev < 0) {
			printf("Could not get dev_id for eventdev with name %s, i=%d\n",
			       info.driver_name, i);
			skipped++;
			continue;
		}
		found++;
		printf("Running selftest on eventdev %s\n", info.driver_name);
		ret = do_selftest();
		if (ret == 0) {
			passed++;
			printf("Selftest passed for eventdev %s\n",
			       info.driver_name);
		} else {
			failed++;
			printf("Selftest failed for eventdev %s, err=%d\n",
			       info.driver_name, ret);
		}
	}

	printf("Ran selftest on %d eventdevs, %d skipped, %d passed, %d failed\n",
	       found, skipped, passed, failed);
	return ret;
}
