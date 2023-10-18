/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <bus_vdev_driver.h>
#include <rte_kvargs.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <rte_event_ring.h>
#include <rte_service_component.h>

#include "sw_evdev.h"
#include "iq_chunk.h"
#include "event_ring.h"

#define EVENTDEV_NAME_SW_PMD event_sw
#define NUMA_NODE_ARG "numa_node"
#define SCHED_QUANTA_ARG "sched_quanta"
#define CREDIT_QUANTA_ARG "credit_quanta"
#define MIN_BURST_SIZE_ARG "min_burst"
#define DEQ_BURST_SIZE_ARG "deq_burst"
#define REFIL_ONCE_ARG "refill_once"

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
		unsigned int j;

		/* check for qid map overflow */
		if (q->cq_num_mapped_cqs >= RTE_DIM(q->cq_map)) {
			rte_errno = EDQUOT;
			break;
		}

		if (p->is_directed && p->num_qids_mapped > 0) {
			rte_errno = EDQUOT;
			break;
		}

		for (j = 0; j < q->cq_num_mapped_cqs; j++) {
			if (q->cq_map[j] == p->id)
				break;
		}

		/* check if port is already linked */
		if (j < q->cq_num_mapped_cqs)
			continue;

		if (q->type == SW_SCHED_TYPE_DIRECT) {
			/* check directed qids only map to one port */
			if (p->num_qids_mapped > 0) {
				rte_errno = EDQUOT;
				break;
			}
			/* check port only takes a directed flow */
			if (num > 1) {
				rte_errno = EDQUOT;
				break;
			}

			p->is_directed = 1;
			p->num_qids_mapped = 1;
		} else if (q->type == RTE_SCHED_TYPE_ORDERED) {
			p->num_ordered_qids++;
			p->num_qids_mapped++;
		} else if (q->type == RTE_SCHED_TYPE_ATOMIC ||
				q->type == RTE_SCHED_TYPE_PARALLEL) {
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

	p->unlinks_in_progress += unlinked;
	rte_smp_mb();

	return unlinked;
}

static int
sw_port_unlinks_in_progress(struct rte_eventdev *dev, void *port)
{
	RTE_SET_USED(dev);
	struct sw_port *p = port;
	return p->unlinks_in_progress;
}

static int
sw_port_setup(struct rte_eventdev *dev, uint8_t port_id,
		const struct rte_event_port_conf *conf)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	struct sw_port *p = &sw->ports[port_id];
	char buf[RTE_RING_NAMESIZE];
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

	/* check to see if rings exists - port_setup() can be called multiple
	 * times legally (assuming device is stopped). If ring exists, free it
	 * to so it gets re-created with the correct size
	 */
	snprintf(buf, sizeof(buf), "sw%d_p%u_%s", dev->data->dev_id,
			port_id, "rx_worker_ring");
	struct rte_event_ring *existing_ring = rte_event_ring_lookup(buf);
	rte_event_ring_free(existing_ring);

	p->rx_worker_ring = rte_event_ring_create(buf, MAX_SW_PROD_Q_DEPTH,
			dev->data->socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
	if (p->rx_worker_ring == NULL) {
		SW_LOG_ERR("Error creating RX worker ring for port %d\n",
				port_id);
		return -1;
	}

	p->inflight_max = conf->new_event_threshold;
	p->implicit_release = !(conf->event_port_cfg &
				RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL);

	/* check if ring exists, same as rx_worker above */
	snprintf(buf, sizeof(buf), "sw%d_p%u, %s", dev->data->dev_id,
			port_id, "cq_worker_ring");
	existing_ring = rte_event_ring_lookup(buf);
	rte_event_ring_free(existing_ring);

	p->cq_worker_ring = rte_event_ring_create(buf, conf->dequeue_depth,
			dev->data->socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
	if (p->cq_worker_ring == NULL) {
		rte_event_ring_free(p->rx_worker_ring);
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

	rte_event_ring_free(p->rx_worker_ring);
	rte_event_ring_free(p->cq_worker_ring);
	memset(p, 0, sizeof(*p));
}

static int32_t
qid_init(struct sw_evdev *sw, unsigned int idx, int type,
		const struct rte_event_queue_conf *queue_conf)
{
	unsigned int i;
	int dev_id = sw->data->dev_id;
	int socket_id = sw->data->socket_id;
	char buf[IQ_ROB_NAMESIZE];
	struct sw_qid *qid = &sw->qids[idx];

	/* Initialize the FID structures to no pinning (-1), and zero packets */
	const struct sw_fid_t fid = {.cq = -1, .pcount = 0};
	for (i = 0; i < RTE_DIM(qid->fids); i++)
		qid->fids[i] = fid;

	qid->id = idx;
	qid->type = type;
	qid->priority = queue_conf->priority;

	if (qid->type == RTE_SCHED_TYPE_ORDERED) {
		uint32_t window_size;

		/* rte_ring and window_size_mask require window_size to
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

		qid->reorder_buffer_freelist = rob_ring_create(window_size,
				socket_id);
		if (!qid->reorder_buffer_freelist) {
			SW_LOG_DBG("freelist ring create failed");
			goto cleanup;
		}

		/* Populate the freelist with reorder buffer entries. Enqueue
		 * 'window_size - 1' entries because the rte_ring holds only
		 * that many.
		 */
		for (i = 0; i < window_size - 1; i++) {
			if (rob_ring_enqueue(qid->reorder_buffer_freelist,
						&qid->reorder_buffer[i]) != 1)
				goto cleanup;
		}

		qid->reorder_buffer_index = 0;
		qid->cq_next_tx = 0;
	}

	qid->initialized = 1;

	return 0;

cleanup:
	if (qid->reorder_buffer) {
		rte_free(qid->reorder_buffer);
		qid->reorder_buffer = NULL;
	}

	if (qid->reorder_buffer_freelist) {
		rob_ring_free(qid->reorder_buffer_freelist);
		qid->reorder_buffer_freelist = NULL;
	}

	return -EINVAL;
}

static void
sw_queue_release(struct rte_eventdev *dev, uint8_t id)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	struct sw_qid *qid = &sw->qids[id];

	if (qid->type == RTE_SCHED_TYPE_ORDERED) {
		rte_free(qid->reorder_buffer);
		rob_ring_free(qid->reorder_buffer_freelist);
	}
	memset(qid, 0, sizeof(*qid));
}

static int
sw_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
		const struct rte_event_queue_conf *conf)
{
	int type;

	type = conf->schedule_type;

	if (RTE_EVENT_QUEUE_CFG_SINGLE_LINK & conf->event_queue_cfg) {
		type = SW_SCHED_TYPE_DIRECT;
	} else if (RTE_EVENT_QUEUE_CFG_ALL_TYPES
			& conf->event_queue_cfg) {
		SW_LOG_ERR("QUEUE_CFG_ALL_TYPES not supported\n");
		return -ENOTSUP;
	}

	struct sw_evdev *sw = sw_pmd_priv(dev);

	if (sw->qids[queue_id].initialized)
		sw_queue_release(dev, queue_id);

	return qid_init(sw, queue_id, type, conf);
}

static void
sw_init_qid_iqs(struct sw_evdev *sw)
{
	int i, j;

	/* Initialize the IQ memory of all configured qids */
	for (i = 0; i < RTE_EVENT_MAX_QUEUES_PER_DEV; i++) {
		struct sw_qid *qid = &sw->qids[i];

		if (!qid->initialized)
			continue;

		for (j = 0; j < SW_IQS_MAX; j++)
			iq_init(sw, &qid->iq[j]);
	}
}

static int
sw_qids_empty(struct sw_evdev *sw)
{
	unsigned int i, j;

	for (i = 0; i < sw->qid_count; i++) {
		for (j = 0; j < SW_IQS_MAX; j++) {
			if (iq_count(&sw->qids[i].iq[j]))
				return 0;
		}
	}

	return 1;
}

static int
sw_ports_empty(struct sw_evdev *sw)
{
	unsigned int i;

	for (i = 0; i < sw->port_count; i++) {
		if ((rte_event_ring_count(sw->ports[i].rx_worker_ring)) ||
		     rte_event_ring_count(sw->ports[i].cq_worker_ring))
			return 0;
	}

	return 1;
}

static void
sw_drain_ports(struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	eventdev_stop_flush_t flush;
	unsigned int i;
	uint8_t dev_id;
	void *arg;

	flush = dev->dev_ops->dev_stop_flush;
	dev_id = dev->data->dev_id;
	arg = dev->data->dev_stop_flush_arg;

	for (i = 0; i < sw->port_count; i++) {
		struct rte_event ev;

		while (rte_event_dequeue_burst(dev_id, i, &ev, 1, 0)) {
			if (flush)
				flush(dev_id, ev, arg);

			ev.op = RTE_EVENT_OP_RELEASE;
			rte_event_enqueue_burst(dev_id, i, &ev, 1);
		}
	}
}

static void
sw_drain_queue(struct rte_eventdev *dev, struct sw_iq *iq)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	eventdev_stop_flush_t flush;
	uint8_t dev_id;
	void *arg;

	flush = dev->dev_ops->dev_stop_flush;
	dev_id = dev->data->dev_id;
	arg = dev->data->dev_stop_flush_arg;

	while (iq_count(iq) > 0) {
		struct rte_event ev;

		iq_dequeue_burst(sw, iq, &ev, 1);

		if (flush)
			flush(dev_id, ev, arg);
	}
}

static void
sw_drain_queues(struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	unsigned int i, j;

	for (i = 0; i < sw->qid_count; i++) {
		for (j = 0; j < SW_IQS_MAX; j++)
			sw_drain_queue(dev, &sw->qids[i].iq[j]);
	}
}

static void
sw_clean_qid_iqs(struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	int i, j;

	/* Release the IQ memory of all configured qids */
	for (i = 0; i < RTE_EVENT_MAX_QUEUES_PER_DEV; i++) {
		struct sw_qid *qid = &sw->qids[i];

		for (j = 0; j < SW_IQS_MAX; j++) {
			if (!qid->iq[j].head)
				continue;
			iq_free_chunk_list(sw, qid->iq[j].head);
			qid->iq[j].head = NULL;
		}
	}
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
		.schedule_type = RTE_SCHED_TYPE_ATOMIC,
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
	port_conf->event_port_cfg = 0;
}

static int
sw_dev_configure(const struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	const struct rte_eventdev_data *data = dev->data;
	const struct rte_event_dev_config *conf = &data->dev_conf;
	int num_chunks, i;

	sw->qid_count = conf->nb_event_queues;
	sw->port_count = conf->nb_event_ports;
	sw->nb_events_limit = conf->nb_events_limit;
	rte_atomic32_set(&sw->inflights, 0);

	/* Number of chunks sized for worst-case spread of events across IQs */
	num_chunks = ((SW_INFLIGHT_EVENTS_TOTAL/SW_EVS_PER_Q_CHUNK)+1) +
			sw->qid_count*SW_IQS_MAX*2;

	/* If this is a reconfiguration, free the previous IQ allocation. All
	 * IQ chunk references were cleaned out of the QIDs in sw_stop(), and
	 * will be reinitialized in sw_start().
	 */
	rte_free(sw->chunks);

	sw->chunks = rte_malloc_socket(NULL,
				       sizeof(struct sw_queue_chunk) *
				       num_chunks,
				       0,
				       sw->data->socket_id);
	if (!sw->chunks)
		return -ENOMEM;

	sw->chunk_list_head = NULL;
	for (i = 0; i < num_chunks; i++)
		iq_free_chunk(sw, &sw->chunks[i]);

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		return -ENOTSUP;

	return 0;
}

struct rte_eth_dev;

static int
sw_eth_rx_adapter_caps_get(const struct rte_eventdev *dev,
			const struct rte_eth_dev *eth_dev,
			uint32_t *caps)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);
	*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	return 0;
}

static int
sw_timer_adapter_caps_get(const struct rte_eventdev *dev, uint64_t flags,
			  uint32_t *caps,
			  const struct event_timer_adapter_ops **ops)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(flags);
	*caps = RTE_EVENT_TIMER_ADAPTER_SW_CAP;

	/* Use default SW ops */
	*ops = NULL;

	return 0;
}

static int
sw_crypto_adapter_caps_get(const struct rte_eventdev *dev,
			   const struct rte_cryptodev *cdev,
			   uint32_t *caps)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(cdev);
	*caps = RTE_EVENT_CRYPTO_ADAPTER_SW_CAP;
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
			.event_dev_cap = (
				RTE_EVENT_DEV_CAP_QUEUE_QOS |
				RTE_EVENT_DEV_CAP_BURST_MODE |
				RTE_EVENT_DEV_CAP_EVENT_QOS |
				RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE|
				RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
				RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
				RTE_EVENT_DEV_CAP_NONSEQ_MODE |
				RTE_EVENT_DEV_CAP_CARRY_FLOW_ID |
				RTE_EVENT_DEV_CAP_MAINTENANCE_FREE),
			.max_profiles_per_port = 1,
	};

	*info = evdev_sw_info;
}

static void
sw_dump(struct rte_eventdev *dev, FILE *f)
{
	const struct sw_evdev *sw = sw_pmd_priv(dev);

	static const char * const q_type_strings[] = {
			"Ordered", "Atomic", "Parallel", "Directed"
	};
	uint32_t i;
	fprintf(f, "EventDev %s: ports %d, qids %d\n",
		dev->data->name, sw->port_count, sw->qid_count);

	fprintf(f, "\trx   %"PRIu64"\n\tdrop %"PRIu64"\n\ttx   %"PRIu64"\n",
		sw->stats.rx_pkts, sw->stats.rx_dropped, sw->stats.tx_pkts);
	fprintf(f, "\tsched calls: %"PRIu64"\n", sw->sched_called);
	fprintf(f, "\tsched cq/qid call: %"PRIu64"\n", sw->sched_cq_qid_called);
	fprintf(f, "\tsched no IQ enq: %"PRIu64"\n", sw->sched_no_iq_enqueues);
	fprintf(f, "\tsched no CQ enq: %"PRIu64"\n", sw->sched_no_cq_enqueues);
	uint32_t inflights = rte_atomic32_read(&sw->inflights);
	uint32_t credits = sw->nb_events_limit - inflights;
	fprintf(f, "\tinflight %d, credits: %d\n", inflights, credits);

#define COL_RED "\x1b[31m"
#define COL_RESET "\x1b[0m"

	for (i = 0; i < sw->port_count; i++) {
		int max, j;
		const struct sw_port *p = &sw->ports[i];
		if (!p->initialized) {
			fprintf(f, "  %sPort %d not initialized.%s\n",
				COL_RED, i, COL_RESET);
			continue;
		}
		fprintf(f, "  Port %d %s\n", i,
			p->is_directed ? " (SingleCons)" : "");
		fprintf(f, "\trx   %"PRIu64"\tdrop %"PRIu64"\ttx   %"PRIu64
			"\t%sinflight %d%s\n", sw->ports[i].stats.rx_pkts,
			sw->ports[i].stats.rx_dropped,
			sw->ports[i].stats.tx_pkts,
			(p->inflights == p->inflight_max) ?
				COL_RED : COL_RESET,
			sw->ports[i].inflights, COL_RESET);

		fprintf(f, "\tMax New: %u"
			"\tAvg cycles PP: %"PRIu64"\tCredits: %u\n",
			sw->ports[i].inflight_max,
			sw->ports[i].avg_pkt_ticks,
			sw->ports[i].inflight_credits);
		fprintf(f, "\tReceive burst distribution:\n");
		float zp_percent = p->zero_polls * 100.0 / p->total_polls;
		fprintf(f, zp_percent < 10 ? "\t\t0:%.02f%% " : "\t\t0:%.0f%% ",
				zp_percent);
		for (max = (int)RTE_DIM(p->poll_buckets); max-- > 0;)
			if (p->poll_buckets[max] != 0)
				break;
		for (j = 0; j <= max; j++) {
			if (p->poll_buckets[j] != 0) {
				float poll_pc = p->poll_buckets[j] * 100.0 /
					p->total_polls;
				fprintf(f, "%u-%u:%.02f%% ",
					((j << SW_DEQ_STAT_BUCKET_SHIFT) + 1),
					((j+1) << SW_DEQ_STAT_BUCKET_SHIFT),
					poll_pc);
			}
		}
		fprintf(f, "\n");

		if (p->rx_worker_ring) {
			uint64_t used = rte_event_ring_count(p->rx_worker_ring);
			uint64_t space = rte_event_ring_free_count(
					p->rx_worker_ring);
			const char *col = (space == 0) ? COL_RED : COL_RESET;
			fprintf(f, "\t%srx ring used: %4"PRIu64"\tfree: %4"
					PRIu64 COL_RESET"\n", col, used, space);
		} else
			fprintf(f, "\trx ring not initialized.\n");

		if (p->cq_worker_ring) {
			uint64_t used = rte_event_ring_count(p->cq_worker_ring);
			uint64_t space = rte_event_ring_free_count(
					p->cq_worker_ring);
			const char *col = (space == 0) ? COL_RED : COL_RESET;
			fprintf(f, "\t%scq ring used: %4"PRIu64"\tfree: %4"
					PRIu64 COL_RESET"\n", col, used, space);
		} else
			fprintf(f, "\tcq ring not initialized.\n");
	}

	for (i = 0; i < sw->qid_count; i++) {
		const struct sw_qid *qid = &sw->qids[i];
		if (!qid->initialized) {
			fprintf(f, "  %sQueue %d not initialized.%s\n",
				COL_RED, i, COL_RESET);
			continue;
		}
		int affinities_per_port[SW_PORTS_MAX] = {0};

		fprintf(f, "  Queue %d (%s)\n", i, q_type_strings[qid->type]);
		fprintf(f, "\trx   %"PRIu64"\tdrop %"PRIu64"\ttx   %"PRIu64"\n",
			qid->stats.rx_pkts, qid->stats.rx_dropped,
			qid->stats.tx_pkts);
		if (qid->type == RTE_SCHED_TYPE_ORDERED) {
			struct rob_ring *rob_buf_free =
				qid->reorder_buffer_freelist;
			if (rob_buf_free)
				fprintf(f, "\tReorder entries in use: %u\n",
					rob_ring_free_count(rob_buf_free));
			else
				fprintf(f,
					"\tReorder buffer not initialized\n");
		}

		uint32_t flow;
		for (flow = 0; flow < RTE_DIM(qid->fids); flow++)
			if (qid->fids[flow].cq != -1) {
				affinities_per_port[qid->fids[flow].cq]++;
			}

		uint32_t port;
		fprintf(f, "\tPer Port Stats:\n");
		for (port = 0; port < sw->port_count; port++) {
			fprintf(f, "\t  Port %d: Pkts: %"PRIu64, port,
					qid->to_port[port]);
			fprintf(f, "\tFlows: %d\n", affinities_per_port[port]);
		}

		uint32_t iq;
		uint32_t iq_printed = 0;
		for (iq = 0; iq < SW_IQS_MAX; iq++) {
			if (!qid->iq[iq].head) {
				fprintf(f, "\tiq %d is not initialized.\n", iq);
				iq_printed = 1;
				continue;
			}
			uint32_t used = iq_count(&qid->iq[iq]);
			const char *col = COL_RESET;
			if (used > 0) {
				fprintf(f, "\t%siq %d: Used %d"
					COL_RESET"\n", col, iq, used);
				iq_printed = 1;
			}
		}
		if (iq_printed == 0)
			fprintf(f, "\t-- iqs empty --\n");
	}
}

static int
sw_start(struct rte_eventdev *dev)
{
	unsigned int i, j;
	struct sw_evdev *sw = sw_pmd_priv(dev);

	rte_service_component_runstate_set(sw->service_id, 1);

	/* check a service core is mapped to this service */
	if (!rte_service_runstate_get(sw->service_id)) {
		SW_LOG_ERR("Warning: No Service core enabled on service %s\n",
				sw->service_name);
		return -ENOENT;
	}

	/* check all ports are set up */
	for (i = 0; i < sw->port_count; i++)
		if (sw->ports[i].rx_worker_ring == NULL) {
			SW_LOG_ERR("Port %d not configured\n", i);
			return -ESTALE;
		}

	/* check all queues are configured and mapped to ports*/
	for (i = 0; i < sw->qid_count; i++)
		if (!sw->qids[i].initialized ||
		    sw->qids[i].cq_num_mapped_cqs == 0) {
			SW_LOG_ERR("Queue %d not configured\n", i);
			return -ENOLINK;
		}

	/* build up our prioritized array of qids */
	/* We don't use qsort here, as if all/multiple entries have the same
	 * priority, the result is non-deterministic. From "man 3 qsort":
	 * "If two members compare as equal, their order in the sorted
	 * array is undefined."
	 */
	uint32_t qidx = 0;
	for (j = 0; j <= RTE_EVENT_DEV_PRIORITY_LOWEST; j++) {
		for (i = 0; i < sw->qid_count; i++) {
			if (sw->qids[i].priority == j) {
				sw->qids_prioritized[qidx] = &sw->qids[i];
				qidx++;
			}
		}
	}

	sw_init_qid_iqs(sw);

	if (sw_xstats_init(sw) < 0)
		return -EINVAL;

	rte_smp_wmb();
	sw->started = 1;

	return 0;
}

static void
sw_stop(struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	int32_t runstate;

	/* Stop the scheduler if it's running */
	runstate = rte_service_runstate_get(sw->service_id);
	if (runstate == 1)
		rte_service_runstate_set(sw->service_id, 0);

	while (rte_service_may_be_active(sw->service_id))
		rte_pause();

	/* Flush all events out of the device */
	while (!(sw_qids_empty(sw) && sw_ports_empty(sw))) {
		sw_event_schedule(dev);
		sw_drain_ports(dev);
		sw_drain_queues(dev);
	}

	sw_clean_qid_iqs(dev);
	sw_xstats_uninit(sw);
	sw->started = 0;
	rte_smp_wmb();

	if (runstate == 1)
		rte_service_runstate_set(sw->service_id, 1);
}

static int
sw_close(struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	uint32_t i;

	for (i = 0; i < sw->qid_count; i++)
		sw_queue_release(dev, i);
	sw->qid_count = 0;

	for (i = 0; i < sw->port_count; i++)
		sw_port_release(&sw->ports[i]);
	sw->port_count = 0;

	memset(&sw->stats, 0, sizeof(sw->stats));
	sw->sched_called = 0;
	sw->sched_no_iq_enqueues = 0;
	sw->sched_no_cq_enqueues = 0;
	sw->sched_cq_qid_called = 0;

	return 0;
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
set_deq_burst_sz(const char *key __rte_unused, const char *value, void *opaque)
{
	int *deq_burst_sz = opaque;
	*deq_burst_sz = atoi(value);
	if (*deq_burst_sz < 0 || *deq_burst_sz > SCHED_DEQUEUE_MAX_BURST_SIZE)
		return -1;
	return 0;
}

static int
set_min_burst_sz(const char *key __rte_unused, const char *value, void *opaque)
{
	int *min_burst_sz = opaque;
	*min_burst_sz = atoi(value);
	if (*min_burst_sz < 0 || *min_burst_sz > SCHED_DEQUEUE_MAX_BURST_SIZE)
		return -1;
	return 0;
}

static int
set_refill_once(const char *key __rte_unused, const char *value, void *opaque)
{
	int *refill_once_per_call = opaque;
	*refill_once_per_call = atoi(value);
	if (*refill_once_per_call < 0 || *refill_once_per_call > 1)
		return -1;
	return 0;
}

static int32_t sw_sched_service_func(void *args)
{
	struct rte_eventdev *dev = args;
	return sw_event_schedule(dev);
}

static int
sw_probe(struct rte_vdev_device *vdev)
{
	static struct eventdev_ops evdev_sw_ops = {
			.dev_configure = sw_dev_configure,
			.dev_infos_get = sw_info_get,
			.dev_close = sw_close,
			.dev_start = sw_start,
			.dev_stop = sw_stop,
			.dump = sw_dump,

			.queue_def_conf = sw_queue_def_conf,
			.queue_setup = sw_queue_setup,
			.queue_release = sw_queue_release,
			.port_def_conf = sw_port_def_conf,
			.port_setup = sw_port_setup,
			.port_release = sw_port_release,
			.port_link = sw_port_link,
			.port_unlink = sw_port_unlink,
			.port_unlinks_in_progress = sw_port_unlinks_in_progress,

			.eth_rx_adapter_caps_get = sw_eth_rx_adapter_caps_get,

			.timer_adapter_caps_get = sw_timer_adapter_caps_get,

			.crypto_adapter_caps_get = sw_crypto_adapter_caps_get,

			.xstats_get = sw_xstats_get,
			.xstats_get_names = sw_xstats_get_names,
			.xstats_get_by_name = sw_xstats_get_by_name,
			.xstats_reset = sw_xstats_reset,

			.dev_selftest = test_sw_eventdev,
	};

	static const char *const args[] = {
		NUMA_NODE_ARG,
		SCHED_QUANTA_ARG,
		CREDIT_QUANTA_ARG,
		MIN_BURST_SIZE_ARG,
		DEQ_BURST_SIZE_ARG,
		REFIL_ONCE_ARG,
		NULL
	};
	const char *name;
	const char *params;
	struct rte_eventdev *dev;
	struct sw_evdev *sw;
	int socket_id = rte_socket_id();
	int sched_quanta  = SW_DEFAULT_SCHED_QUANTA;
	int credit_quanta = SW_DEFAULT_CREDIT_QUANTA;
	int min_burst_size = 1;
	int deq_burst_size = SCHED_DEQUEUE_DEFAULT_BURST_SIZE;
	int refill_once = 0;

	name = rte_vdev_device_name(vdev);
	params = rte_vdev_device_args(vdev);
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

			ret = rte_kvargs_process(kvlist, MIN_BURST_SIZE_ARG,
					set_min_burst_sz, &min_burst_size);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing minimum burst size parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DEQ_BURST_SIZE_ARG,
					set_deq_burst_sz, &deq_burst_size);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing dequeue burst size parameter",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, REFIL_ONCE_ARG,
					set_refill_once, &refill_once);
			if (ret != 0) {
				SW_LOG_ERR(
					"%s: Error parsing refill once per call switch",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			rte_kvargs_free(kvlist);
		}
	}

	SW_LOG_INFO(
			"Creating eventdev sw device %s, numa_node=%d, "
			"sched_quanta=%d, credit_quanta=%d "
			"min_burst=%d, deq_burst=%d, refill_once=%d\n",
			name, socket_id, sched_quanta, credit_quanta,
			min_burst_size, deq_burst_size, refill_once);

	dev = rte_event_pmd_vdev_init(name,
			sizeof(struct sw_evdev), socket_id, vdev);
	if (dev == NULL) {
		SW_LOG_ERR("eventdev vdev init() failed");
		return -EFAULT;
	}
	dev->dev_ops = &evdev_sw_ops;
	dev->enqueue = sw_event_enqueue;
	dev->enqueue_burst = sw_event_enqueue_burst;
	dev->enqueue_new_burst = sw_event_enqueue_burst;
	dev->enqueue_forward_burst = sw_event_enqueue_burst;
	dev->dequeue = sw_event_dequeue;
	dev->dequeue_burst = sw_event_dequeue_burst;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	sw = dev->data->dev_private;
	sw->data = dev->data;

	/* copy values passed from vdev command line to instance */
	sw->credit_update_quanta = credit_quanta;
	sw->sched_quanta = sched_quanta;
	sw->sched_min_burst_size = min_burst_size;
	sw->sched_deq_burst_size = deq_burst_size;
	sw->refill_once_per_iter = refill_once;

	/* register service with EAL */
	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	snprintf(service.name, sizeof(service.name), "%s_service", name);
	snprintf(sw->service_name, sizeof(sw->service_name), "%s_service",
			name);
	service.socket_id = socket_id;
	service.callback = sw_sched_service_func;
	service.callback_userdata = (void *)dev;

	int32_t ret = rte_service_component_register(&service, &sw->service_id);
	if (ret) {
		SW_LOG_ERR("service register() failed");
		return -ENOEXEC;
	}

	dev->data->service_inited = 1;
	dev->data->service_id = sw->service_id;

	event_dev_probing_finish(dev);

	return 0;
}

static int
sw_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
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
		SCHED_QUANTA_ARG "=<int>" CREDIT_QUANTA_ARG "=<int>"
		MIN_BURST_SIZE_ARG "=<int>" DEQ_BURST_SIZE_ARG "=<int>"
		REFIL_ONCE_ARG "=<int>");
RTE_LOG_REGISTER_DEFAULT(eventdev_sw_log_level, NOTICE);
