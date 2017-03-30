/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#include <string.h>

#include <rte_vdev.h>
#include <rte_memzone.h>
#include <rte_kvargs.h>
#include <rte_ring.h>
#include <rte_errno.h>

#include "sw_evdev.h"
#include "iq_ring.h"
#include "event_ring.h"

#define EVENTDEV_NAME_SW_PMD event_sw
#define NUMA_NODE_ARG "numa_node"
#define SCHED_QUANTA_ARG "sched_quanta"
#define CREDIT_QUANTA_ARG "credit_quanta"

static void
sw_info_get(struct rte_eventdev *dev, struct rte_event_dev_info *info);

static int
sw_port_link(struct rte_eventdev *dev, void *port, const uint8_t queues[],
		const uint8_t priorities[], uint16_t num)
{
	struct sw_port *p = port;
	struct sw_evdev *sw = sw_pmd_priv(dev);
	int i;

	RTE_SET_USED(priorities);
	for (i = 0; i < num; i++) {
		struct sw_qid *q = &sw->qids[queues[i]];

		/* check for qid map overflow */
		if (q->cq_num_mapped_cqs >= RTE_DIM(q->cq_map)) {
			rte_errno = -EDQUOT;
			break;
		}

		if (p->is_directed && p->num_qids_mapped > 0) {
			rte_errno = -EDQUOT;
			break;
		}

		if (q->type == SW_SCHED_TYPE_DIRECT) {
			/* check directed qids only map to one port */
			if (p->num_qids_mapped > 0) {
				rte_errno = -EDQUOT;
				break;
			}
			/* check port only takes a directed flow */
			if (num > 1) {
				rte_errno = -EDQUOT;
				break;
			}

			p->is_directed = 1;
			p->num_qids_mapped = 1;
		} else if (q->type == RTE_SCHED_TYPE_ORDERED) {
			p->num_ordered_qids++;
			p->num_qids_mapped++;
		} else if (q->type == RTE_SCHED_TYPE_ATOMIC) {
			p->num_qids_mapped++;
		}

		q->cq_map[q->cq_num_mapped_cqs] = p->id;
		rte_smp_wmb();
		q->cq_num_mapped_cqs++;
	}
	return i;
}

static int
sw_port_unlink(struct rte_eventdev *dev, void *port, uint8_t queues[],
		uint16_t nb_unlinks)
{
	struct sw_port *p = port;
	struct sw_evdev *sw = sw_pmd_priv(dev);
	unsigned int i, j;

	int unlinked = 0;
	for (i = 0; i < nb_unlinks; i++) {
		struct sw_qid *q = &sw->qids[queues[i]];
		for (j = 0; j < q->cq_num_mapped_cqs; j++) {
			if (q->cq_map[j] == p->id) {
				q->cq_map[j] =
					q->cq_map[q->cq_num_mapped_cqs - 1];
				rte_smp_wmb();
				q->cq_num_mapped_cqs--;
				unlinked++;

				p->num_qids_mapped--;

				if (q->type == RTE_SCHED_TYPE_ORDERED)
					p->num_ordered_qids--;

				continue;
			}
		}
	}
	return unlinked;
}

static int
sw_port_setup(struct rte_eventdev *dev, uint8_t port_id,
		const struct rte_event_port_conf *conf)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	struct sw_port *p = &sw->ports[port_id];
	char buf[QE_RING_NAMESIZE];
	unsigned int i;

	struct rte_event_dev_info info;
	sw_info_get(dev, &info);

	/* detect re-configuring and return credits to instance if needed */
	if (p->initialized) {
		/* taking credits from pool is done one quanta at a time, and
		 * credits may be spend (counted in p->inflights) or still
		 * available in the port (p->inflight_credits). We must return
		 * the sum to no leak credits
		 */
		int possible_inflights = p->inflight_credits + p->inflights;
		rte_atomic32_sub(&sw->inflights, possible_inflights);
	}

	*p = (struct sw_port){0}; /* zero entire structure */
	p->id = port_id;
	p->sw = sw;

	snprintf(buf, sizeof(buf), "sw%d_%s", dev->data->dev_id,
			"rx_worker_ring");
	p->rx_worker_ring = qe_ring_create(buf, MAX_SW_PROD_Q_DEPTH,
			dev->data->socket_id);
	if (p->rx_worker_ring == NULL) {
		SW_LOG_ERR("Error creating RX worker ring for port %d\n",
				port_id);
		return -1;
	}

	p->inflight_max = conf->new_event_threshold;

	snprintf(buf, sizeof(buf), "sw%d_%s", dev->data->dev_id,
			"cq_worker_ring");
	p->cq_worker_ring = qe_ring_create(buf, conf->dequeue_depth,
			dev->data->socket_id);
	if (p->cq_worker_ring == NULL) {
		qe_ring_destroy(p->rx_worker_ring);
		SW_LOG_ERR("Error creating CQ worker ring for port %d\n",
				port_id);
		return -1;
	}
	sw->cq_ring_space[port_id] = conf->dequeue_depth;

	/* set hist list contents to empty */
	for (i = 0; i < SW_PORT_HIST_LIST; i++) {
		p->hist_list[i].fid = -1;
		p->hist_list[i].qid = -1;
	}
	dev->data->ports[port_id] = p;

	rte_smp_wmb();
	p->initialized = 1;
	return 0;
}

static void
sw_port_release(void *port)
{
	struct sw_port *p = (void *)port;
	if (p == NULL)
		return;

	qe_ring_destroy(p->rx_worker_ring);
	qe_ring_destroy(p->cq_worker_ring);
	memset(p, 0, sizeof(*p));
}

static int32_t
qid_init(struct sw_evdev *sw, unsigned int idx, int type,
		const struct rte_event_queue_conf *queue_conf)
{
	unsigned int i;
	int dev_id = sw->data->dev_id;
	int socket_id = sw->data->socket_id;
	char buf[IQ_RING_NAMESIZE];
	struct sw_qid *qid = &sw->qids[idx];

	for (i = 0; i < SW_IQS_MAX; i++) {
		snprintf(buf, sizeof(buf), "q_%u_iq_%d", idx, i);
		qid->iq[i] = iq_ring_create(buf, socket_id);
		if (!qid->iq[i]) {
			SW_LOG_DBG("ring create failed");
			goto cleanup;
		}
	}

	/* Initialize the FID structures to no pinning (-1), and zero packets */
	const struct sw_fid_t fid = {.cq = -1, .pcount = 0};
	for (i = 0; i < RTE_DIM(qid->fids); i++)
		qid->fids[i] = fid;

	qid->id = idx;
	qid->type = type;
	qid->priority = queue_conf->priority;

	if (qid->type == RTE_SCHED_TYPE_ORDERED) {
		char ring_name[RTE_RING_NAMESIZE];
		uint32_t window_size;

		/* rte_ring and window_size_mask require require window_size to
		 * be a power-of-2.
		 */
		window_size = rte_align32pow2(
				queue_conf->nb_atomic_order_sequences);

		qid->window_size = window_size - 1;

		if (!window_size) {
			SW_LOG_DBG(
				"invalid reorder_window_size for ordered queue\n"
				);
			goto cleanup;
		}

		snprintf(buf, sizeof(buf), "sw%d_iq_%d_rob", dev_id, i);
		qid->reorder_buffer = rte_zmalloc_socket(buf,
				window_size * sizeof(qid->reorder_buffer[0]),
				0, socket_id);
		if (!qid->reorder_buffer) {
			SW_LOG_DBG("reorder_buffer malloc failed\n");
			goto cleanup;
		}

		memset(&qid->reorder_buffer[0],
		       0,
		       window_size * sizeof(qid->reorder_buffer[0]));

		snprintf(ring_name, sizeof(ring_name), "sw%d_q%d_freelist",
				dev_id, idx);

		/* lookup the ring, and if it already exists, free it */
		struct rte_ring *cleanup = rte_ring_lookup(ring_name);
		if (cleanup)
			rte_ring_free(cleanup);

		qid->reorder_buffer_freelist = rte_ring_create(ring_name,
				window_size,
				socket_id,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (!qid->reorder_buffer_freelist) {
			SW_LOG_DBG("freelist ring create failed");
			goto cleanup;
		}

		/* Populate the freelist with reorder buffer entries. Enqueue
		 * 'window_size - 1' entries because the rte_ring holds only
		 * that many.
		 */
		for (i = 0; i < window_size - 1; i++) {
			if (rte_ring_sp_enqueue(qid->reorder_buffer_freelist,
						&qid->reorder_buffer[i]) < 0)
				goto cleanup;
		}

		qid->reorder_buffer_index = 0;
		qid->cq_next_tx = 0;
	}

	qid->initialized = 1;

	return 0;

cleanup:
	for (i = 0; i < SW_IQS_MAX; i++) {
		if (qid->iq[i])
			iq_ring_destroy(qid->iq[i]);
	}

	if (qid->reorder_buffer) {
		rte_free(qid->reorder_buffer);
		qid->reorder_buffer = NULL;
	}

	if (qid->reorder_buffer_freelist) {
		rte_ring_free(qid->reorder_buffer_freelist);
		qid->reorder_buffer_freelist = NULL;
	}

	return -EINVAL;
}

static int
sw_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
		const struct rte_event_queue_conf *conf)
{
	int type;

	/* SINGLE_LINK can be OR-ed with other types, so handle first */
	if (RTE_EVENT_QUEUE_CFG_SINGLE_LINK & conf->event_queue_cfg) {
		type = SW_SCHED_TYPE_DIRECT;
	} else {
		switch (conf->event_queue_cfg) {
		case RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY:
			type = RTE_SCHED_TYPE_ATOMIC;
			break;
		case RTE_EVENT_QUEUE_CFG_ORDERED_ONLY:
			type = RTE_SCHED_TYPE_ORDERED;
			break;
		case RTE_EVENT_QUEUE_CFG_PARALLEL_ONLY:
			type = RTE_SCHED_TYPE_PARALLEL;
			break;
		case RTE_EVENT_QUEUE_CFG_ALL_TYPES:
			SW_LOG_ERR("QUEUE_CFG_ALL_TYPES not supported\n");
			return -ENOTSUP;
		default:
			SW_LOG_ERR("Unknown queue type %d requested\n",
				   conf->event_queue_cfg);
			return -EINVAL;
		}
	}

	struct sw_evdev *sw = sw_pmd_priv(dev);
	return qid_init(sw, queue_id, type, conf);
}

static void
sw_queue_release(struct rte_eventdev *dev, uint8_t id)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	struct sw_qid *qid = &sw->qids[id];
	uint32_t i;

	for (i = 0; i < SW_IQS_MAX; i++)
		iq_ring_destroy(qid->iq[i]);

	if (qid->type == RTE_SCHED_TYPE_ORDERED) {
		rte_free(qid->reorder_buffer);
		rte_ring_free(qid->reorder_buffer_freelist);
	}
	memset(qid, 0, sizeof(*qid));
}

static void
sw_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
				 struct rte_event_queue_conf *conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	static const struct rte_event_queue_conf default_conf = {
		.nb_atomic_flows = 4096,
		.nb_atomic_order_sequences = 1,
		.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY,
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
	};

	*conf = default_conf;
}

static void
sw_port_def_conf(struct rte_eventdev *dev, uint8_t port_id,
		 struct rte_event_port_conf *port_conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(port_id);

	port_conf->new_event_threshold = 1024;
	port_conf->dequeue_depth = 16;
	port_conf->enqueue_depth = 16;
}

static int
sw_dev_configure(const struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	const struct rte_eventdev_data *data = dev->data;
	const struct rte_event_dev_config *conf = &data->dev_conf;

	sw->qid_count = conf->nb_event_queues;
	sw->port_count = conf->nb_event_ports;
	sw->nb_events_limit = conf->nb_events_limit;
	rte_atomic32_set(&sw->inflights, 0);

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		return -ENOTSUP;

	return 0;
}

static void
sw_info_get(struct rte_eventdev *dev, struct rte_event_dev_info *info)
{
	RTE_SET_USED(dev);

	static const struct rte_event_dev_info evdev_sw_info = {
			.driver_name = SW_PMD_NAME,
			.max_event_queues = RTE_EVENT_MAX_QUEUES_PER_DEV,
			.max_event_queue_flows = SW_QID_NUM_FIDS,
			.max_event_queue_priority_levels = SW_Q_PRIORITY_MAX,
			.max_event_priority_levels = SW_IQS_MAX,
			.max_event_ports = SW_PORTS_MAX,
			.max_event_port_dequeue_depth = MAX_SW_CONS_Q_DEPTH,
			.max_event_port_enqueue_depth = MAX_SW_PROD_Q_DEPTH,
			.max_num_events = SW_INFLIGHT_EVENTS_TOTAL,
			.event_dev_cap = (RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_EVENT_QOS),
	};

	*info = evdev_sw_info;
}

static int
assign_numa_node(const char *key __rte_unused, const char *value, void *opaque)
{
	int *socket_id = opaque;
	*socket_id = atoi(value);
	if (*socket_id >= RTE_MAX_NUMA_NODES)
		return -1;
	return 0;
}

static int
set_sched_quanta(const char *key __rte_unused, const char *value, void *opaque)
{
	int *quanta = opaque;
	*quanta = atoi(value);
	if (*quanta < 0 || *quanta >= 4096)
		return -1;
	return 0;
}

static int
set_credit_quanta(const char *key __rte_unused, const char *value, void *opaque)
{
	int *credit = opaque;
	*credit = atoi(value);
	if (*credit < 0 || *credit >= 128)
		return -1;
	return 0;
}

static int
sw_probe(const char *name, const char *params)
{
	static const struct rte_eventdev_ops evdev_sw_ops = {
			.dev_configure = sw_dev_configure,
			.dev_infos_get = sw_info_get,

			.queue_def_conf = sw_queue_def_conf,
			.queue_setup = sw_queue_setup,
			.queue_release = sw_queue_release,
			.port_def_conf = sw_port_def_conf,
			.port_setup = sw_port_setup,
			.port_release = sw_port_release,
			.port_link = sw_port_link,
			.port_unlink = sw_port_unlink,
	};

	static const char *const args[] = {
		NUMA_NODE_ARG,
		SCHED_QUANTA_ARG,
		CREDIT_QUANTA_ARG,
		NULL
	};
	struct rte_eventdev *dev;
	struct sw_evdev *sw;
	int socket_id = rte_socket_id();
	int sched_quanta  = SW_DEFAULT_SCHED_QUANTA;
	int credit_quanta = SW_DEFAULT_CREDIT_QUANTA;

	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (!kvlist) {
			SW_LOG_INFO(
				"Ignoring unsupported parameters when creating device '%s'\n",
				name);
		} else {
			int ret = rte_kvargs_process(kvlist, NUMA_NODE_ARG,
					assign_numa_node, &socket_id);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing numa node parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, SCHED_QUANTA_ARG,
					set_sched_quanta, &sched_quanta);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing sched quanta parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, CREDIT_QUANTA_ARG,
					set_credit_quanta, &credit_quanta);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing credit quanta parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			rte_kvargs_free(kvlist);
		}
	}

	SW_LOG_INFO(
			"Creating eventdev sw device %s, numa_node=%d, sched_quanta=%d, credit_quanta=%d\n",
			name, socket_id, sched_quanta, credit_quanta);

	dev = rte_event_pmd_vdev_init(name,
			sizeof(struct sw_evdev), socket_id);
	if (dev == NULL) {
		SW_LOG_ERR("eventdev vdev init() failed");
		return -EFAULT;
	}
	dev->dev_ops = &evdev_sw_ops;
	dev->enqueue = sw_event_enqueue;
	dev->enqueue_burst = sw_event_enqueue_burst;
	dev->dequeue = sw_event_dequeue;
	dev->dequeue_burst = sw_event_dequeue_burst;
	dev->schedule = sw_event_schedule;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	sw = dev->data->dev_private;
	sw->data = dev->data;

	/* copy values passed from vdev command line to instance */
	sw->credit_update_quanta = credit_quanta;
	sw->sched_quanta = sched_quanta;

	return 0;
}

static int
sw_remove(const char *name)
{
	if (name == NULL)
		return -EINVAL;

	SW_LOG_INFO("Closing eventdev sw device %s\n", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver evdev_sw_pmd_drv = {
	.probe = sw_probe,
	.remove = sw_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_SW_PMD, evdev_sw_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(event_sw, NUMA_NODE_ARG "=<int> "
		SCHED_QUANTA_ARG "=<int>" CREDIT_QUANTA_ARG "=<int>");
