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

#include "sw_evdev.h"
#include "iq_ring.h"
#include "event_ring.h"

enum xstats_type {
	/* common stats */
	rx,
	tx,
	dropped,
	inflight,
	calls,
	credits,
	/* device instance specific */
	no_iq_enq,
	no_cq_enq,
	/* port_specific */
	rx_used,
	rx_free,
	tx_used,
	tx_free,
	pkt_cycles,
	poll_return, /* for zero-count and used also for port bucket loop */
	/* qid_specific */
	iq_size,
	iq_used,
	/* qid port mapping specific */
	pinned,
	pkts, /* note: qid-to-port pkts */
};

typedef uint64_t (*xstats_fn)(const struct sw_evdev *dev,
		uint16_t obj_idx, /* port or queue id */
		enum xstats_type stat, int extra_arg);

struct sw_xstats_entry {
	struct rte_event_dev_xstats_name name;
	xstats_fn fn;
	uint16_t obj_idx;
	enum xstats_type stat;
	enum rte_event_dev_xstats_mode mode;
	int extra_arg;
	uint8_t reset_allowed; /* when set, this value can be reset */
	uint64_t reset_value; /* an offset to be taken away to emulate resets */
};

static uint64_t
get_dev_stat(const struct sw_evdev *sw, uint16_t obj_idx __rte_unused,
		enum xstats_type type, int extra_arg __rte_unused)
{
	switch (type) {
	case rx: return sw->stats.rx_pkts;
	case tx: return sw->stats.tx_pkts;
	case dropped: return sw->stats.rx_dropped;
	case calls: return sw->sched_called;
	case no_iq_enq: return sw->sched_no_iq_enqueues;
	case no_cq_enq: return sw->sched_no_cq_enqueues;
	default: return -1;
	}
}

static uint64_t
get_port_stat(const struct sw_evdev *sw, uint16_t obj_idx,
		enum xstats_type type, int extra_arg __rte_unused)
{
	const struct sw_port *p = &sw->ports[obj_idx];

	switch (type) {
	case rx: return p->stats.rx_pkts;
	case tx: return p->stats.tx_pkts;
	case dropped: return p->stats.rx_dropped;
	case inflight: return p->inflights;
	case pkt_cycles: return p->avg_pkt_ticks;
	case calls: return p->total_polls;
	case credits: return p->inflight_credits;
	case poll_return: return p->zero_polls;
	case rx_used: return qe_ring_count(p->rx_worker_ring);
	case rx_free: return qe_ring_free_count(p->rx_worker_ring);
	case tx_used: return qe_ring_count(p->cq_worker_ring);
	case tx_free: return qe_ring_free_count(p->cq_worker_ring);
	default: return -1;
	}
}

static uint64_t
get_port_bucket_stat(const struct sw_evdev *sw, uint16_t obj_idx,
		enum xstats_type type, int extra_arg)
{
	const struct sw_port *p = &sw->ports[obj_idx];

	switch (type) {
	case poll_return: return p->poll_buckets[extra_arg];
	default: return -1;
	}
}

static uint64_t
get_qid_stat(const struct sw_evdev *sw, uint16_t obj_idx,
		enum xstats_type type, int extra_arg __rte_unused)
{
	const struct sw_qid *qid = &sw->qids[obj_idx];

	switch (type) {
	case rx: return qid->stats.rx_pkts;
	case tx: return qid->stats.tx_pkts;
	case dropped: return qid->stats.rx_dropped;
	case inflight:
		do {
			uint64_t infl = 0;
			unsigned int i;
			for (i = 0; i < RTE_DIM(qid->fids); i++)
				infl += qid->fids[i].pcount;
			return infl;
		} while (0);
		break;
	case iq_size: return RTE_DIM(qid->iq[0]->ring);
	default: return -1;
	}
}

static uint64_t
get_qid_iq_stat(const struct sw_evdev *sw, uint16_t obj_idx,
		enum xstats_type type, int extra_arg)
{
	const struct sw_qid *qid = &sw->qids[obj_idx];
	const int iq_idx = extra_arg;

	switch (type) {
	case iq_used: return iq_ring_count(qid->iq[iq_idx]);
	default: return -1;
	}
}

static uint64_t
get_qid_port_stat(const struct sw_evdev *sw, uint16_t obj_idx,
		enum xstats_type type, int extra_arg)
{
	const struct sw_qid *qid = &sw->qids[obj_idx];
	uint16_t port = extra_arg;

	switch (type) {
	case pinned:
		do {
			uint64_t pin = 0;
			unsigned int i;
			for (i = 0; i < RTE_DIM(qid->fids); i++)
				if (qid->fids[i].cq == port)
					pin++;
			return pin;
		} while (0);
		break;
	case pkts:
		return qid->to_port[port];
	default: return -1;
	}
}

int
sw_xstats_init(struct sw_evdev *sw)
{
	/*
	 * define the stats names and types. Used to build up the device
	 * xstats array
	 * There are multiple set of stats:
	 *   - device-level,
	 *   - per-port,
	 *   - per-port-dequeue-burst-sizes
	 *   - per-qid,
	 *   - per-iq
	 *   - per-port-per-qid
	 *
	 * For each of these sets, we have three parallel arrays, one for the
	 * names, the other for the stat type parameter to be passed in the fn
	 * call to get that stat. The third array allows resetting or not.
	 * All these arrays must be kept in sync
	 */
	static const char * const dev_stats[] = { "rx", "tx", "drop",
			"sched_calls", "sched_no_iq_enq", "sched_no_cq_enq",
	};
	static const enum xstats_type dev_types[] = { rx, tx, dropped,
			calls, no_iq_enq, no_cq_enq,
	};
	/* all device stats are allowed to be reset */

	static const char * const port_stats[] = {"rx", "tx", "drop",
			"inflight", "avg_pkt_cycles", "credits",
			"rx_ring_used", "rx_ring_free",
			"cq_ring_used", "cq_ring_free",
			"dequeue_calls", "dequeues_returning_0",
	};
	static const enum xstats_type port_types[] = { rx, tx, dropped,
			inflight, pkt_cycles, credits,
			rx_used, rx_free, tx_used, tx_free,
			calls, poll_return,
	};
	static const uint8_t port_reset_allowed[] = {1, 1, 1,
			0, 1, 0,
			0, 0, 0, 0,
			1, 1,
	};

	static const char * const port_bucket_stats[] = {
			"dequeues_returning" };
	static const enum xstats_type port_bucket_types[] = { poll_return };
	/* all bucket dequeues are allowed to be reset, handled in loop below */

	static const char * const qid_stats[] = {"rx", "tx", "drop",
			"inflight", "iq_size"
	};
	static const enum xstats_type qid_types[] = { rx, tx, dropped,
			inflight, iq_size
	};
	static const uint8_t qid_reset_allowed[] = {1, 1, 1,
			0, 0
	};

	static const char * const qid_iq_stats[] = { "used" };
	static const enum xstats_type qid_iq_types[] = { iq_used };
	/* reset allowed */

	static const char * const qid_port_stats[] = { "pinned_flows",
		"packets"
	};
	static const enum xstats_type qid_port_types[] = { pinned, pkts };
	static const uint8_t qid_port_reset_allowed[] = {0, 1};
	/* reset allowed */
	/* ---- end of stat definitions ---- */

	/* check sizes, since a missed comma can lead to strings being
	 * joined by the compiler.
	 */
	RTE_BUILD_BUG_ON(RTE_DIM(dev_stats) != RTE_DIM(dev_types));
	RTE_BUILD_BUG_ON(RTE_DIM(port_stats) != RTE_DIM(port_types));
	RTE_BUILD_BUG_ON(RTE_DIM(qid_stats) != RTE_DIM(qid_types));
	RTE_BUILD_BUG_ON(RTE_DIM(qid_iq_stats) != RTE_DIM(qid_iq_types));
	RTE_BUILD_BUG_ON(RTE_DIM(qid_port_stats) != RTE_DIM(qid_port_types));
	RTE_BUILD_BUG_ON(RTE_DIM(port_bucket_stats) !=
			RTE_DIM(port_bucket_types));

	RTE_BUILD_BUG_ON(RTE_DIM(port_stats) != RTE_DIM(port_reset_allowed));
	RTE_BUILD_BUG_ON(RTE_DIM(qid_stats) != RTE_DIM(qid_reset_allowed));

	/* other vars */
	const uint32_t cons_bkt_shift =
		(MAX_SW_CONS_Q_DEPTH >> SW_DEQ_STAT_BUCKET_SHIFT);
	const unsigned int count = RTE_DIM(dev_stats) +
			sw->port_count * RTE_DIM(port_stats) +
			sw->port_count * RTE_DIM(port_bucket_stats) *
				(cons_bkt_shift + 1) +
			sw->qid_count * RTE_DIM(qid_stats) +
			sw->qid_count * SW_IQS_MAX * RTE_DIM(qid_iq_stats) +
			sw->qid_count * sw->port_count *
				RTE_DIM(qid_port_stats);
	unsigned int i, port, qid, iq, bkt, stat = 0;

	sw->xstats = rte_zmalloc_socket(NULL, sizeof(sw->xstats[0]) * count, 0,
			sw->data->socket_id);
	if (sw->xstats == NULL)
		return -ENOMEM;

#define sname sw->xstats[stat].name.name
	for (i = 0; i < RTE_DIM(dev_stats); i++, stat++) {
		sw->xstats[stat] = (struct sw_xstats_entry){
			.fn = get_dev_stat,
			.stat = dev_types[i],
			.mode = RTE_EVENT_DEV_XSTATS_DEVICE,
			.reset_allowed = 1,
		};
		snprintf(sname, sizeof(sname), "dev_%s", dev_stats[i]);
	}
	sw->xstats_count_mode_dev = stat;

	for (port = 0; port < sw->port_count; port++) {
		sw->xstats_offset_for_port[port] = stat;

		uint32_t count_offset = stat;

		for (i = 0; i < RTE_DIM(port_stats); i++, stat++) {
			sw->xstats[stat] = (struct sw_xstats_entry){
				.fn = get_port_stat,
				.obj_idx = port,
				.stat = port_types[i],
				.mode = RTE_EVENT_DEV_XSTATS_PORT,
				.reset_allowed = port_reset_allowed[i],
			};
			snprintf(sname, sizeof(sname), "port_%u_%s",
					port, port_stats[i]);
		}

		for (bkt = 0; bkt < (sw->ports[port].cq_worker_ring->size >>
				SW_DEQ_STAT_BUCKET_SHIFT) + 1; bkt++) {
			for (i = 0; i < RTE_DIM(port_bucket_stats); i++) {
				sw->xstats[stat] = (struct sw_xstats_entry){
					.fn = get_port_bucket_stat,
					.obj_idx = port,
					.stat = port_bucket_types[i],
					.mode = RTE_EVENT_DEV_XSTATS_PORT,
					.extra_arg = bkt,
					.reset_allowed = 1,
				};
				snprintf(sname, sizeof(sname),
					"port_%u_%s_%u-%u",
					port, port_bucket_stats[i],
					(bkt << SW_DEQ_STAT_BUCKET_SHIFT) + 1,
					(bkt + 1) << SW_DEQ_STAT_BUCKET_SHIFT);
				stat++;
			}
		}

		sw->xstats_count_per_port[port] = stat - count_offset;
	}

	sw->xstats_count_mode_port = stat - sw->xstats_count_mode_dev;

	for (qid = 0; qid < sw->qid_count; qid++) {
		uint32_t count_offset = stat;
		sw->xstats_offset_for_qid[qid] = stat;

		for (i = 0; i < RTE_DIM(qid_stats); i++, stat++) {
			sw->xstats[stat] = (struct sw_xstats_entry){
				.fn = get_qid_stat,
				.obj_idx = qid,
				.stat = qid_types[i],
				.mode = RTE_EVENT_DEV_XSTATS_QUEUE,
				.reset_allowed = qid_reset_allowed[i],
			};
			snprintf(sname, sizeof(sname), "qid_%u_%s",
					qid, qid_stats[i]);
		}
		for (iq = 0; iq < SW_IQS_MAX; iq++)
			for (i = 0; i < RTE_DIM(qid_iq_stats); i++, stat++) {
				sw->xstats[stat] = (struct sw_xstats_entry){
					.fn = get_qid_iq_stat,
					.obj_idx = qid,
					.stat = qid_iq_types[i],
					.mode = RTE_EVENT_DEV_XSTATS_QUEUE,
					.extra_arg = iq,
					.reset_allowed = 0,
				};
				snprintf(sname, sizeof(sname),
						"qid_%u_iq_%u_%s",
						qid, iq,
						qid_iq_stats[i]);
			}

		for (port = 0; port < sw->port_count; port++)
			for (i = 0; i < RTE_DIM(qid_port_stats); i++, stat++) {
				sw->xstats[stat] = (struct sw_xstats_entry){
					.fn = get_qid_port_stat,
					.obj_idx = qid,
					.stat = qid_port_types[i],
					.mode = RTE_EVENT_DEV_XSTATS_QUEUE,
					.extra_arg = port,
					.reset_allowed =
						qid_port_reset_allowed[i],
				};
				snprintf(sname, sizeof(sname),
						"qid_%u_port_%u_%s",
						qid, port,
						qid_port_stats[i]);
			}

		sw->xstats_count_per_qid[qid] = stat - count_offset;
	}

	sw->xstats_count_mode_queue = stat -
		(sw->xstats_count_mode_dev + sw->xstats_count_mode_port);
#undef sname

	sw->xstats_count = stat;

	return stat;
}

int
sw_xstats_uninit(struct sw_evdev *sw)
{
	rte_free(sw->xstats);
	sw->xstats_count = 0;
	return 0;
}

int
sw_xstats_get_names(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstats_names,
		unsigned int *ids, unsigned int size)
{
	const struct sw_evdev *sw = sw_pmd_priv_const(dev);
	unsigned int i;
	unsigned int xidx = 0;
	RTE_SET_USED(mode);
	RTE_SET_USED(queue_port_id);

	uint32_t xstats_mode_count = 0;
	uint32_t start_offset = 0;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		xstats_mode_count = sw->xstats_count_mode_dev;
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)sw->port_count)
			break;
		xstats_mode_count = sw->xstats_count_per_port[queue_port_id];
		start_offset = sw->xstats_offset_for_port[queue_port_id];
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)sw->qid_count)
			break;
		xstats_mode_count = sw->xstats_count_per_qid[queue_port_id];
		start_offset = sw->xstats_offset_for_qid[queue_port_id];
		break;
	default:
		SW_LOG_ERR("Invalid mode received in sw_xstats_get_names()\n");
		return -EINVAL;
	};

	if (xstats_mode_count > size || !ids || !xstats_names)
		return xstats_mode_count;

	for (i = 0; i < sw->xstats_count && xidx < size; i++) {
		if (sw->xstats[i].mode != mode)
			continue;

		if (mode != RTE_EVENT_DEV_XSTATS_DEVICE &&
				queue_port_id != sw->xstats[i].obj_idx)
			continue;

		xstats_names[xidx] = sw->xstats[i].name;
		if (ids)
			ids[xidx] = start_offset + xidx;
		xidx++;
	}
	return xidx;
}

static int
sw_xstats_update(struct sw_evdev *sw, enum rte_event_dev_xstats_mode mode,
		uint8_t queue_port_id, const unsigned int ids[],
		uint64_t values[], unsigned int n, const uint32_t reset,
		const uint32_t ret_if_n_lt_nstats)
{
	unsigned int i;
	unsigned int xidx = 0;
	RTE_SET_USED(mode);
	RTE_SET_USED(queue_port_id);

	uint32_t xstats_mode_count = 0;

	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		xstats_mode_count = sw->xstats_count_mode_dev;
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id >= (signed int)sw->port_count)
			goto invalid_value;
		xstats_mode_count = sw->xstats_count_per_port[queue_port_id];
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id >= (signed int)sw->qid_count)
			goto invalid_value;
		xstats_mode_count = sw->xstats_count_per_qid[queue_port_id];
		break;
	default:
		SW_LOG_ERR("Invalid mode received in sw_xstats_get()\n");
		goto invalid_value;
	};

	/* this function can check num stats and return them (xstats_get() style
	 * behaviour) or ignore n for reset() of a single stat style behaviour.
	 */
	if (ret_if_n_lt_nstats && xstats_mode_count > n)
		return xstats_mode_count;

	for (i = 0; i < n && xidx < xstats_mode_count; i++) {
		struct sw_xstats_entry *xs = &sw->xstats[ids[i]];
		if (ids[i] > sw->xstats_count || xs->mode != mode)
			continue;

		if (mode != RTE_EVENT_DEV_XSTATS_DEVICE &&
				queue_port_id != xs->obj_idx)
			continue;

		uint64_t val = xs->fn(sw, xs->obj_idx, xs->stat, xs->extra_arg)
					- xs->reset_value;

		if (values)
			values[xidx] = val;

		if (xs->reset_allowed && reset)
			xs->reset_value = val;

		xidx++;
	}

	return xidx;
invalid_value:
	return -EINVAL;
}

int
sw_xstats_get(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		const unsigned int ids[], uint64_t values[], unsigned int n)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	const uint32_t reset = 0;
	const uint32_t ret_n_lt_stats = 1;
	return sw_xstats_update(sw, mode, queue_port_id, ids, values, n,
				reset, ret_n_lt_stats);
}

uint64_t
sw_xstats_get_by_name(const struct rte_eventdev *dev,
		const char *name, unsigned int *id)
{
	const struct sw_evdev *sw = sw_pmd_priv_const(dev);
	unsigned int i;

	for (i = 0; i < sw->xstats_count; i++) {
		struct sw_xstats_entry *xs = &sw->xstats[i];
		if (strncmp(xs->name.name, name,
				RTE_EVENT_DEV_XSTATS_NAME_SIZE) == 0){
			if (id != NULL)
				*id = i;
			return xs->fn(sw, xs->obj_idx, xs->stat, xs->extra_arg)
					- xs->reset_value;
		}
	}
	if (id != NULL)
		*id = (uint32_t)-1;
	return (uint64_t)-1;
}

static void
sw_xstats_reset_range(struct sw_evdev *sw, uint32_t start, uint32_t num)
{
	uint32_t i;
	for (i = start; i < start + num; i++) {
		struct sw_xstats_entry *xs = &sw->xstats[i];
		if (!xs->reset_allowed)
			continue;

		uint64_t val = xs->fn(sw, xs->obj_idx, xs->stat, xs->extra_arg)
					- xs->reset_value;
		xs->reset_value = val;
	}
}

static int
sw_xstats_reset_queue(struct sw_evdev *sw, uint8_t queue_id,
		const uint32_t ids[], uint32_t nb_ids)
{
	const uint32_t reset = 1;
	const uint32_t ret_n_lt_stats = 0;
	if (ids) {
		uint32_t nb_reset = sw_xstats_update(sw,
					RTE_EVENT_DEV_XSTATS_QUEUE,
					queue_id, ids, NULL, nb_ids,
					reset, ret_n_lt_stats);
		return nb_reset == nb_ids ? 0 : -EINVAL;
	}

	if (ids == NULL)
		sw_xstats_reset_range(sw, sw->xstats_offset_for_qid[queue_id],
				      sw->xstats_count_per_qid[queue_id]);

	return 0;
}

static int
sw_xstats_reset_port(struct sw_evdev *sw, uint8_t port_id,
		const uint32_t ids[], uint32_t nb_ids)
{
	const uint32_t reset = 1;
	const uint32_t ret_n_lt_stats = 0;
	int offset = sw->xstats_offset_for_port[port_id];
	int nb_stat = sw->xstats_count_per_port[port_id];

	if (ids) {
		uint32_t nb_reset = sw_xstats_update(sw,
					RTE_EVENT_DEV_XSTATS_PORT, port_id,
					ids, NULL, nb_ids,
					reset, ret_n_lt_stats);
		return nb_reset == nb_ids ? 0 : -EINVAL;
	}

	sw_xstats_reset_range(sw, offset, nb_stat);
	return 0;
}

static int
sw_xstats_reset_dev(struct sw_evdev *sw, const uint32_t ids[], uint32_t nb_ids)
{
	uint32_t i;
	if (ids) {
		for (i = 0; i < nb_ids; i++) {
			uint32_t id = ids[i];
			if (id >= sw->xstats_count_mode_dev)
				return -EINVAL;
			sw_xstats_reset_range(sw, id, 1);
		}
	} else {
		for (i = 0; i < sw->xstats_count_mode_dev; i++)
			sw_xstats_reset_range(sw, i, 1);
	}

	return 0;
}

int
sw_xstats_reset(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id,
		const uint32_t ids[],
		uint32_t nb_ids)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	uint32_t i, err;

	/* handle -1 for queue_port_id here, looping over all ports/queues */
	switch (mode) {
	case RTE_EVENT_DEV_XSTATS_DEVICE:
		sw_xstats_reset_dev(sw, ids, nb_ids);
		break;
	case RTE_EVENT_DEV_XSTATS_PORT:
		if (queue_port_id == -1) {
			for (i = 0; i < sw->port_count; i++) {
				err = sw_xstats_reset_port(sw, i, ids, nb_ids);
				if (err)
					return -EINVAL;
			}
		} else if (queue_port_id < (int16_t)sw->port_count)
			sw_xstats_reset_port(sw, queue_port_id, ids, nb_ids);
		break;
	case RTE_EVENT_DEV_XSTATS_QUEUE:
		if (queue_port_id == -1) {
			for (i = 0; i < sw->qid_count; i++) {
				err = sw_xstats_reset_queue(sw, i, ids, nb_ids);
				if (err)
					return -EINVAL;
			}
		} else if (queue_port_id < (int16_t)sw->qid_count)
			sw_xstats_reset_queue(sw, queue_port_id, ids, nb_ids);
		break;
	};

	return 0;
}
