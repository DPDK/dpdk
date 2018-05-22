/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation.
 * All rights reserved.
 */

#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_common.h>
#include <rte_timer.h>
#include <rte_service_component.h>
#include <rte_cycles.h>

#include "rte_eventdev.h"
#include "rte_eventdev_pmd.h"
#include "rte_event_timer_adapter.h"
#include "rte_event_timer_adapter_pmd.h"

#define DATA_MZ_NAME_MAX_LEN 64
#define DATA_MZ_NAME_FORMAT "rte_event_timer_adapter_data_%d"

static int evtim_logtype;
static int evtim_svc_logtype;
static int evtim_buffer_logtype;

static struct rte_event_timer_adapter adapters[RTE_EVENT_TIMER_ADAPTER_NUM_MAX];

static const struct rte_event_timer_adapter_ops sw_event_adapter_timer_ops;

#define EVTIM_LOG(level, logtype, ...) \
	rte_log(RTE_LOG_ ## level, logtype, \
		RTE_FMT("EVTIMER: %s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) \
			"\n", __func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#define EVTIM_LOG_ERR(...) EVTIM_LOG(ERR, evtim_logtype, __VA_ARGS__)

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
#define EVTIM_LOG_DBG(...) \
	EVTIM_LOG(DEBUG, evtim_logtype, __VA_ARGS__)
#define EVTIM_BUF_LOG_DBG(...) \
	EVTIM_LOG(DEBUG, evtim_buffer_logtype, __VA_ARGS__)
#define EVTIM_SVC_LOG_DBG(...) \
	EVTIM_LOG(DEBUG, evtim_svc_logtype, __VA_ARGS__)
#else
#define EVTIM_LOG_DBG(...) (void)0
#define EVTIM_BUF_LOG_DBG(...) (void)0
#define EVTIM_SVC_LOG_DBG(...) (void)0
#endif

static int
default_port_conf_cb(uint16_t id, uint8_t event_dev_id, uint8_t *event_port_id,
		     void *conf_arg)
{
	struct rte_event_timer_adapter *adapter;
	struct rte_eventdev *dev;
	struct rte_event_dev_config dev_conf;
	struct rte_event_port_conf *port_conf, def_port_conf = {0};
	int started;
	uint8_t port_id;
	uint8_t dev_id;
	int ret;

	RTE_SET_USED(event_dev_id);

	adapter = &adapters[id];
	dev = &rte_eventdevs[adapter->data->event_dev_id];
	dev_id = dev->data->dev_id;
	dev_conf = dev->data->dev_conf;

	started = dev->data->dev_started;
	if (started)
		rte_event_dev_stop(dev_id);

	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports += 1;
	ret = rte_event_dev_configure(dev_id, &dev_conf);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to configure event dev %u\n", dev_id);
		if (started)
			if (rte_event_dev_start(dev_id))
				return -EIO;

		return ret;
	}

	if (conf_arg != NULL)
		port_conf = conf_arg;
	else {
		port_conf = &def_port_conf;
		ret = rte_event_port_default_conf_get(dev_id, port_id,
						      port_conf);
		if (ret < 0)
			return ret;
	}

	ret = rte_event_port_setup(dev_id, port_id, port_conf);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to setup event port %u on event dev %u\n",
			      port_id, dev_id);
		return ret;
	}

	*event_port_id = port_id;

	if (started)
		ret = rte_event_dev_start(dev_id);

	return ret;
}

struct rte_event_timer_adapter * __rte_experimental
rte_event_timer_adapter_create(const struct rte_event_timer_adapter_conf *conf)
{
	return rte_event_timer_adapter_create_ext(conf, default_port_conf_cb,
						  NULL);
}

struct rte_event_timer_adapter * __rte_experimental
rte_event_timer_adapter_create_ext(
		const struct rte_event_timer_adapter_conf *conf,
		rte_event_timer_adapter_port_conf_cb_t conf_cb,
		void *conf_arg)
{
	uint16_t adapter_id;
	struct rte_event_timer_adapter *adapter;
	const struct rte_memzone *mz;
	char mz_name[DATA_MZ_NAME_MAX_LEN];
	int n, ret;
	struct rte_eventdev *dev;

	if (conf == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Check eventdev ID */
	if (!rte_event_pmd_is_valid_dev(conf->event_dev_id)) {
		rte_errno = EINVAL;
		return NULL;
	}
	dev = &rte_eventdevs[conf->event_dev_id];

	adapter_id = conf->timer_adapter_id;

	/* Check that adapter_id is in range */
	if (adapter_id >= RTE_EVENT_TIMER_ADAPTER_NUM_MAX) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Check adapter ID not already allocated */
	adapter = &adapters[adapter_id];
	if (adapter->allocated) {
		rte_errno = EEXIST;
		return NULL;
	}

	/* Create shared data area. */
	n = snprintf(mz_name, sizeof(mz_name), DATA_MZ_NAME_FORMAT, adapter_id);
	if (n >= (int)sizeof(mz_name)) {
		rte_errno = EINVAL;
		return NULL;
	}
	mz = rte_memzone_reserve(mz_name,
				 sizeof(struct rte_event_timer_adapter_data),
				 conf->socket_id, 0);
	if (mz == NULL)
		/* rte_errno set by rte_memzone_reserve */
		return NULL;

	adapter->data = mz->addr;
	memset(adapter->data, 0, sizeof(struct rte_event_timer_adapter_data));

	adapter->data->mz = mz;
	adapter->data->event_dev_id = conf->event_dev_id;
	adapter->data->id = adapter_id;
	adapter->data->socket_id = conf->socket_id;
	adapter->data->conf = *conf;  /* copy conf structure */

	/* Query eventdev PMD for timer adapter capabilities and ops */
	ret = dev->dev_ops->timer_adapter_caps_get(dev,
						   adapter->data->conf.flags,
						   &adapter->data->caps,
						   &adapter->ops);
	if (ret < 0) {
		rte_errno = ret;
		goto free_memzone;
	}

	if (!(adapter->data->caps &
	      RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT)) {
		FUNC_PTR_OR_NULL_RET_WITH_ERRNO(conf_cb, -EINVAL);
		ret = conf_cb(adapter->data->id, adapter->data->event_dev_id,
			      &adapter->data->event_port_id, conf_arg);
		if (ret < 0) {
			rte_errno = ret;
			goto free_memzone;
		}
	}

	/* If eventdev PMD did not provide ops, use default software
	 * implementation.
	 */
	if (adapter->ops == NULL)
		adapter->ops = &sw_event_adapter_timer_ops;

	/* Allow driver to do some setup */
	FUNC_PTR_OR_NULL_RET_WITH_ERRNO(adapter->ops->init, -ENOTSUP);
	ret = adapter->ops->init(adapter);
	if (ret < 0) {
		rte_errno = ret;
		goto free_memzone;
	}

	/* Set fast-path function pointers */
	adapter->arm_burst = adapter->ops->arm_burst;
	adapter->arm_tmo_tick_burst = adapter->ops->arm_tmo_tick_burst;
	adapter->cancel_burst = adapter->ops->cancel_burst;

	adapter->allocated = 1;

	return adapter;

free_memzone:
	rte_memzone_free(adapter->data->mz);
	return NULL;
}

int __rte_experimental
rte_event_timer_adapter_get_info(const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_info *adapter_info)
{
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);

	if (adapter->ops->get_info)
		/* let driver set values it knows */
		adapter->ops->get_info(adapter, adapter_info);

	/* Set common values */
	adapter_info->conf = adapter->data->conf;
	adapter_info->event_dev_port_id = adapter->data->event_port_id;
	adapter_info->caps = adapter->data->caps;

	return 0;
}

int __rte_experimental
rte_event_timer_adapter_start(const struct rte_event_timer_adapter *adapter)
{
	int ret;

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->start, -EINVAL);

	ret = adapter->ops->start(adapter);
	if (ret < 0)
		return ret;

	adapter->data->started = 1;

	return 0;
}

int __rte_experimental
rte_event_timer_adapter_stop(const struct rte_event_timer_adapter *adapter)
{
	int ret;

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->stop, -EINVAL);

	if (adapter->data->started == 0) {
		EVTIM_LOG_ERR("event timer adapter %"PRIu8" already stopped",
			      adapter->data->id);
		return 0;
	}

	ret = adapter->ops->stop(adapter);
	if (ret < 0)
		return ret;

	adapter->data->started = 0;

	return 0;
}

struct rte_event_timer_adapter * __rte_experimental
rte_event_timer_adapter_lookup(uint16_t adapter_id)
{
	char name[DATA_MZ_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	struct rte_event_timer_adapter_data *data;
	struct rte_event_timer_adapter *adapter;
	int ret;
	struct rte_eventdev *dev;

	if (adapters[adapter_id].allocated)
		return &adapters[adapter_id]; /* Adapter is already loaded */

	snprintf(name, DATA_MZ_NAME_MAX_LEN, DATA_MZ_NAME_FORMAT, adapter_id);
	mz = rte_memzone_lookup(name);
	if (mz == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	data = mz->addr;

	adapter = &adapters[data->id];
	adapter->data = data;

	dev = &rte_eventdevs[adapter->data->event_dev_id];

	/* Query eventdev PMD for timer adapter capabilities and ops */
	ret = dev->dev_ops->timer_adapter_caps_get(dev,
						   adapter->data->conf.flags,
						   &adapter->data->caps,
						   &adapter->ops);
	if (ret < 0) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* If eventdev PMD did not provide ops, use default software
	 * implementation.
	 */
	if (adapter->ops == NULL)
		adapter->ops = &sw_event_adapter_timer_ops;

	/* Set fast-path function pointers */
	adapter->arm_burst = adapter->ops->arm_burst;
	adapter->arm_tmo_tick_burst = adapter->ops->arm_tmo_tick_burst;
	adapter->cancel_burst = adapter->ops->cancel_burst;

	adapter->allocated = 1;

	return adapter;
}

int __rte_experimental
rte_event_timer_adapter_free(struct rte_event_timer_adapter *adapter)
{
	int ret;

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->uninit, -EINVAL);

	if (adapter->data->started == 1) {
		EVTIM_LOG_ERR("event timer adapter %"PRIu8" must be stopped "
			      "before freeing", adapter->data->id);
		return -EBUSY;
	}

	/* free impl priv data */
	ret = adapter->ops->uninit(adapter);
	if (ret < 0)
		return ret;

	/* free shared data area */
	ret = rte_memzone_free(adapter->data->mz);
	if (ret < 0)
		return ret;

	adapter->data = NULL;
	adapter->allocated = 0;

	return 0;
}

int __rte_experimental
rte_event_timer_adapter_service_id_get(struct rte_event_timer_adapter *adapter,
				       uint32_t *service_id)
{
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);

	if (adapter->data->service_inited && service_id != NULL)
		*service_id = adapter->data->service_id;

	return adapter->data->service_inited ? 0 : -ESRCH;
}

int __rte_experimental
rte_event_timer_adapter_stats_get(struct rte_event_timer_adapter *adapter,
				  struct rte_event_timer_adapter_stats *stats)
{
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->stats_get, -EINVAL);
	if (stats == NULL)
		return -EINVAL;

	return adapter->ops->stats_get(adapter, stats);
}

int __rte_experimental
rte_event_timer_adapter_stats_reset(struct rte_event_timer_adapter *adapter)
{
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->stats_reset, -EINVAL);
	return adapter->ops->stats_reset(adapter);
}

/*
 * Software event timer adapter buffer helper functions
 */

#define NSECPERSEC 1E9

/* Optimizations used to index into the buffer require that the buffer size
 * be a power of 2.
 */
#define EVENT_BUFFER_SZ 4096
#define EVENT_BUFFER_BATCHSZ 32
#define EVENT_BUFFER_MASK (EVENT_BUFFER_SZ - 1)

struct event_buffer {
	uint16_t head;
	uint16_t tail;
	struct rte_event events[EVENT_BUFFER_SZ];
} __rte_cache_aligned;

static inline bool
event_buffer_full(struct event_buffer *bufp)
{
	return (bufp->head - bufp->tail) == EVENT_BUFFER_SZ;
}

static inline bool
event_buffer_batch_ready(struct event_buffer *bufp)
{
	return (bufp->head - bufp->tail) >= EVENT_BUFFER_BATCHSZ;
}

static void
event_buffer_init(struct event_buffer *bufp)
{
	bufp->head = bufp->tail = 0;
	memset(&bufp->events, 0, sizeof(struct rte_event) * EVENT_BUFFER_SZ);
}

static int
event_buffer_add(struct event_buffer *bufp, struct rte_event *eventp)
{
	uint16_t head_idx;
	struct rte_event *buf_eventp;

	if (event_buffer_full(bufp))
		return -1;

	/* Instead of modulus, bitwise AND with mask to get head_idx. */
	head_idx = bufp->head & EVENT_BUFFER_MASK;
	buf_eventp = &bufp->events[head_idx];
	rte_memcpy(buf_eventp, eventp, sizeof(struct rte_event));

	/* Wrap automatically when overflow occurs. */
	bufp->head++;

	return 0;
}

static void
event_buffer_flush(struct event_buffer *bufp, uint8_t dev_id, uint8_t port_id,
		   uint16_t *nb_events_flushed,
		   uint16_t *nb_events_inv)
{
	uint16_t head_idx, tail_idx, n = 0;
	struct rte_event *events = bufp->events;

	/* Instead of modulus, bitwise AND with mask to get index. */
	head_idx = bufp->head & EVENT_BUFFER_MASK;
	tail_idx = bufp->tail & EVENT_BUFFER_MASK;

	/* Determine the largest contigous run we can attempt to enqueue to the
	 * event device.
	 */
	if (head_idx > tail_idx)
		n = head_idx - tail_idx;
	else if (head_idx < tail_idx)
		n = EVENT_BUFFER_SZ - tail_idx;
	else {
		*nb_events_flushed = 0;
		return;
	}

	*nb_events_inv = 0;
	*nb_events_flushed = rte_event_enqueue_burst(dev_id, port_id,
						     &events[tail_idx], n);
	if (*nb_events_flushed != n && rte_errno == -EINVAL) {
		EVTIM_LOG_ERR("failed to enqueue invalid event - dropping it");
		(*nb_events_inv)++;
	}

	bufp->tail = bufp->tail + *nb_events_flushed + *nb_events_inv;
}

/*
 * Software event timer adapter implementation
 */

struct rte_event_timer_adapter_sw_data {
	/* List of messages for outstanding timers */
	TAILQ_HEAD(, msg) msgs_tailq_head;
	/* Lock to guard tailq and armed count */
	rte_spinlock_t msgs_tailq_sl;
	/* Identifier of service executing timer management logic. */
	uint32_t service_id;
	/* The cycle count at which the adapter should next tick */
	uint64_t next_tick_cycles;
	/* Incremented as the service moves through phases of an iteration */
	volatile int service_phase;
	/* The tick resolution used by adapter instance. May have been
	 * adjusted from what user requested
	 */
	uint64_t timer_tick_ns;
	/* Maximum timeout in nanoseconds allowed by adapter instance. */
	uint64_t max_tmo_ns;
	/* Ring containing messages to arm or cancel event timers */
	struct rte_ring *msg_ring;
	/* Mempool containing msg objects */
	struct rte_mempool *msg_pool;
	/* Buffered timer expiry events to be enqueued to an event device. */
	struct event_buffer buffer;
	/* Statistics */
	struct rte_event_timer_adapter_stats stats;
	/* The number of threads currently adding to the message ring */
	rte_atomic16_t message_producer_count;
};

enum msg_type {MSG_TYPE_ARM, MSG_TYPE_CANCEL};

struct msg {
	enum msg_type type;
	struct rte_event_timer *evtim;
	struct rte_timer tim;
	TAILQ_ENTRY(msg) msgs;
};

static void
sw_event_timer_cb(struct rte_timer *tim, void *arg)
{
	int ret;
	uint16_t nb_evs_flushed = 0;
	uint16_t nb_evs_invalid = 0;
	uint64_t opaque;
	struct rte_event_timer *evtim;
	struct rte_event_timer_adapter *adapter;
	struct rte_event_timer_adapter_sw_data *sw_data;

	evtim = arg;
	opaque = evtim->impl_opaque[1];
	adapter = (struct rte_event_timer_adapter *)(uintptr_t)opaque;
	sw_data = adapter->data->adapter_priv;

	ret = event_buffer_add(&sw_data->buffer, &evtim->ev);
	if (ret < 0) {
		/* If event buffer is full, put timer back in list with
		 * immediate expiry value, so that we process it again on the
		 * next iteration.
		 */
		rte_timer_reset_sync(tim, 0, SINGLE, rte_lcore_id(),
				     sw_event_timer_cb, evtim);

		sw_data->stats.evtim_retry_count++;
		EVTIM_LOG_DBG("event buffer full, resetting rte_timer with "
			      "immediate expiry value");
	} else {
		struct msg *m = container_of(tim, struct msg, tim);
		TAILQ_REMOVE(&sw_data->msgs_tailq_head, m, msgs);
		EVTIM_BUF_LOG_DBG("buffered an event timer expiry event");
		evtim->state = RTE_EVENT_TIMER_NOT_ARMED;

		/* Free the msg object containing the rte_timer now that
		 * we've buffered its event successfully.
		 */
		rte_mempool_put(sw_data->msg_pool, m);

		/* Bump the count when we successfully add an expiry event to
		 * the buffer.
		 */
		sw_data->stats.evtim_exp_count++;
	}

	if (event_buffer_batch_ready(&sw_data->buffer)) {
		event_buffer_flush(&sw_data->buffer,
				   adapter->data->event_dev_id,
				   adapter->data->event_port_id,
				   &nb_evs_flushed,
				   &nb_evs_invalid);

		sw_data->stats.ev_enq_count += nb_evs_flushed;
		sw_data->stats.ev_inv_count += nb_evs_invalid;
	}
}

static __rte_always_inline uint64_t
get_timeout_cycles(struct rte_event_timer *evtim,
		   struct rte_event_timer_adapter *adapter)
{
	uint64_t timeout_ns;
	struct rte_event_timer_adapter_sw_data *sw_data;

	sw_data = adapter->data->adapter_priv;
	timeout_ns = evtim->timeout_ticks * sw_data->timer_tick_ns;
	return timeout_ns * rte_get_timer_hz() / NSECPERSEC;

}

/* This function returns true if one or more (adapter) ticks have occurred since
 * the last time it was called.
 */
static inline bool
adapter_did_tick(struct rte_event_timer_adapter *adapter)
{
	uint64_t cycles_per_adapter_tick, start_cycles;
	uint64_t *next_tick_cyclesp;
	struct rte_event_timer_adapter_sw_data *sw_data;

	sw_data = adapter->data->adapter_priv;
	next_tick_cyclesp = &sw_data->next_tick_cycles;

	cycles_per_adapter_tick = sw_data->timer_tick_ns *
			(rte_get_timer_hz() / NSECPERSEC);

	start_cycles = rte_get_timer_cycles();

	/* Note: initially, *next_tick_cyclesp == 0, so the clause below will
	 * execute, and set things going.
	 */

	if (start_cycles >= *next_tick_cyclesp) {
		/* Snap the current cycle count to the preceding adapter tick
		 * boundary.
		 */
		start_cycles -= start_cycles % cycles_per_adapter_tick;

		*next_tick_cyclesp = start_cycles + cycles_per_adapter_tick;

		return true;
	}

	return false;
}

/* Check that event timer timeout value is in range */
static __rte_always_inline int
check_timeout(struct rte_event_timer *evtim,
	      const struct rte_event_timer_adapter *adapter)
{
	uint64_t tmo_nsec;
	struct rte_event_timer_adapter_sw_data *sw_data;

	sw_data = adapter->data->adapter_priv;
	tmo_nsec = evtim->timeout_ticks * sw_data->timer_tick_ns;

	if (tmo_nsec > sw_data->max_tmo_ns)
		return -1;

	if (tmo_nsec < sw_data->timer_tick_ns)
		return -2;

	return 0;
}

/* Check that event timer event queue sched type matches destination event queue
 * sched type
 */
static __rte_always_inline int
check_destination_event_queue(struct rte_event_timer *evtim,
			      const struct rte_event_timer_adapter *adapter)
{
	int ret;
	uint32_t sched_type;

	ret = rte_event_queue_attr_get(adapter->data->event_dev_id,
				       evtim->ev.queue_id,
				       RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE,
				       &sched_type);

	if ((ret < 0 && ret != -EOVERFLOW) ||
	    evtim->ev.sched_type != sched_type)
		return -1;

	return 0;
}

#define NB_OBJS 32
static int
sw_event_timer_adapter_service_func(void *arg)
{
	int i, num_msgs;
	uint64_t cycles, opaque;
	uint16_t nb_evs_flushed = 0;
	uint16_t nb_evs_invalid = 0;
	struct rte_event_timer_adapter *adapter;
	struct rte_event_timer_adapter_sw_data *sw_data;
	struct rte_event_timer *evtim = NULL;
	struct rte_timer *tim = NULL;
	struct msg *msg, *msgs[NB_OBJS];

	adapter = arg;
	sw_data = adapter->data->adapter_priv;

	sw_data->service_phase = 1;
	rte_smp_wmb();

	while (rte_atomic16_read(&sw_data->message_producer_count) > 0 ||
	       !rte_ring_empty(sw_data->msg_ring)) {

		num_msgs = rte_ring_dequeue_burst(sw_data->msg_ring,
						  (void **)msgs, NB_OBJS, NULL);

		for (i = 0; i < num_msgs; i++) {
			int ret = 0;

			RTE_SET_USED(ret);

			msg = msgs[i];
			evtim = msg->evtim;

			switch (msg->type) {
			case MSG_TYPE_ARM:
				EVTIM_SVC_LOG_DBG("dequeued ARM message from "
						  "ring");
				tim = &msg->tim;
				rte_timer_init(tim);
				cycles = get_timeout_cycles(evtim,
							    adapter);
				ret = rte_timer_reset(tim, cycles, SINGLE,
						      rte_lcore_id(),
						      sw_event_timer_cb,
						      evtim);
				RTE_ASSERT(ret == 0);

				evtim->impl_opaque[0] = (uintptr_t)tim;
				evtim->impl_opaque[1] = (uintptr_t)adapter;

				TAILQ_INSERT_TAIL(&sw_data->msgs_tailq_head,
						  msg,
						  msgs);
				break;
			case MSG_TYPE_CANCEL:
				EVTIM_SVC_LOG_DBG("dequeued CANCEL message "
						  "from ring");
				opaque = evtim->impl_opaque[0];
				tim = (struct rte_timer *)(uintptr_t)opaque;
				RTE_ASSERT(tim != NULL);

				ret = rte_timer_stop(tim);
				RTE_ASSERT(ret == 0);

				/* Free the msg object for the original arm
				 * request.
				 */
				struct msg *m;
				m = container_of(tim, struct msg, tim);
				TAILQ_REMOVE(&sw_data->msgs_tailq_head, m,
					     msgs);
				rte_mempool_put(sw_data->msg_pool, m);

				/* Free the msg object for the current msg */
				rte_mempool_put(sw_data->msg_pool, msg);

				evtim->impl_opaque[0] = 0;
				evtim->impl_opaque[1] = 0;

				break;
			}
		}
	}

	sw_data->service_phase = 2;
	rte_smp_wmb();

	if (adapter_did_tick(adapter)) {
		rte_timer_manage();

		event_buffer_flush(&sw_data->buffer,
				   adapter->data->event_dev_id,
				   adapter->data->event_port_id,
				   &nb_evs_flushed, &nb_evs_invalid);

		sw_data->stats.ev_enq_count += nb_evs_flushed;
		sw_data->stats.ev_inv_count += nb_evs_invalid;
		sw_data->stats.adapter_tick_count++;
	}

	sw_data->service_phase = 0;
	rte_smp_wmb();

	return 0;
}

/* The adapter initialization function rounds the mempool size up to the next
 * power of 2, so we can take the difference between that value and what the
 * user requested, and use the space for caches.  This avoids a scenario where a
 * user can't arm the number of timers the adapter was configured with because
 * mempool objects have been lost to caches.
 *
 * nb_actual should always be a power of 2, so we can iterate over the powers
 * of 2 to see what the largest cache size we can use is.
 */
static int
compute_msg_mempool_cache_size(uint64_t nb_requested, uint64_t nb_actual)
{
	int i;
	int size;
	int cache_size = 0;

	for (i = 0; ; i++) {
		size = 1 << i;

		if (RTE_MAX_LCORE * size < (int)(nb_actual - nb_requested) &&
		    size < RTE_MEMPOOL_CACHE_MAX_SIZE &&
		    size <= nb_actual / 1.5)
			cache_size = size;
		else
			break;
	}

	return cache_size;
}

#define SW_MIN_INTERVAL 1E5

static int
sw_event_timer_adapter_init(struct rte_event_timer_adapter *adapter)
{
	int ret;
	struct rte_event_timer_adapter_sw_data *sw_data;
	uint64_t nb_timers;
	unsigned int flags;
	struct rte_service_spec service;
	static bool timer_subsystem_inited; // static initialized to false

	/* Allocate storage for SW implementation data */
	char priv_data_name[RTE_RING_NAMESIZE];
	snprintf(priv_data_name, RTE_RING_NAMESIZE, "sw_evtim_adap_priv_%"PRIu8,
		 adapter->data->id);
	adapter->data->adapter_priv = rte_zmalloc_socket(
				priv_data_name,
				sizeof(struct rte_event_timer_adapter_sw_data),
				RTE_CACHE_LINE_SIZE,
				adapter->data->socket_id);
	if (adapter->data->adapter_priv == NULL) {
		EVTIM_LOG_ERR("failed to allocate space for private data");
		rte_errno = ENOMEM;
		return -1;
	}

	if (adapter->data->conf.timer_tick_ns < SW_MIN_INTERVAL) {
		EVTIM_LOG_ERR("failed to create adapter with requested tick "
			      "interval");
		rte_errno = EINVAL;
		return -1;
	}

	sw_data = adapter->data->adapter_priv;

	sw_data->timer_tick_ns = adapter->data->conf.timer_tick_ns;
	sw_data->max_tmo_ns = adapter->data->conf.max_tmo_ns;

	TAILQ_INIT(&sw_data->msgs_tailq_head);
	rte_spinlock_init(&sw_data->msgs_tailq_sl);
	rte_atomic16_init(&sw_data->message_producer_count);

	/* Rings require power of 2, so round up to next such value */
	nb_timers = rte_align64pow2(adapter->data->conf.nb_timers);

	char msg_ring_name[RTE_RING_NAMESIZE];
	snprintf(msg_ring_name, RTE_RING_NAMESIZE,
		 "sw_evtim_adap_msg_ring_%"PRIu8, adapter->data->id);
	flags = adapter->data->conf.flags & RTE_EVENT_TIMER_ADAPTER_F_SP_PUT ?
		RING_F_SP_ENQ | RING_F_SC_DEQ :
		RING_F_SC_DEQ;
	sw_data->msg_ring = rte_ring_create(msg_ring_name, nb_timers,
					    adapter->data->socket_id, flags);
	if (sw_data->msg_ring == NULL) {
		EVTIM_LOG_ERR("failed to create message ring");
		rte_errno = ENOMEM;
		goto free_priv_data;
	}

	char pool_name[RTE_RING_NAMESIZE];
	snprintf(pool_name, RTE_RING_NAMESIZE, "sw_evtim_adap_msg_pool_%"PRIu8,
		 adapter->data->id);

	/* Both the arming/canceling thread and the service thread will do puts
	 * to the mempool, but if the SP_PUT flag is enabled, we can specify
	 * single-consumer get for the mempool.
	 */
	flags = adapter->data->conf.flags & RTE_EVENT_TIMER_ADAPTER_F_SP_PUT ?
		MEMPOOL_F_SC_GET : 0;

	/* The usable size of a ring is count - 1, so subtract one here to
	 * make the counts agree.
	 */
	int pool_size = nb_timers - 1;
	int cache_size = compute_msg_mempool_cache_size(
				adapter->data->conf.nb_timers, nb_timers);
	sw_data->msg_pool = rte_mempool_create(pool_name, pool_size,
					       sizeof(struct msg), cache_size,
					       0, NULL, NULL, NULL, NULL,
					       adapter->data->socket_id, flags);
	if (sw_data->msg_pool == NULL) {
		EVTIM_LOG_ERR("failed to create message object mempool");
		rte_errno = ENOMEM;
		goto free_msg_ring;
	}

	event_buffer_init(&sw_data->buffer);

	/* Register a service component to run adapter logic */
	memset(&service, 0, sizeof(service));
	snprintf(service.name, RTE_SERVICE_NAME_MAX,
		 "sw_evimer_adap_svc_%"PRIu8, adapter->data->id);
	service.socket_id = adapter->data->socket_id;
	service.callback = sw_event_timer_adapter_service_func;
	service.callback_userdata = adapter;
	service.capabilities &= ~(RTE_SERVICE_CAP_MT_SAFE);
	ret = rte_service_component_register(&service, &sw_data->service_id);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to register service %s with id %"PRIu32
			      ": err = %d", service.name, sw_data->service_id,
			      ret);

		rte_errno = ENOSPC;
		goto free_msg_pool;
	}

	EVTIM_LOG_DBG("registered service %s with id %"PRIu32, service.name,
		      sw_data->service_id);

	adapter->data->service_id = sw_data->service_id;
	adapter->data->service_inited = 1;

	if (!timer_subsystem_inited) {
		rte_timer_subsystem_init();
		timer_subsystem_inited = true;
	}

	return 0;

free_msg_pool:
	rte_mempool_free(sw_data->msg_pool);
free_msg_ring:
	rte_ring_free(sw_data->msg_ring);
free_priv_data:
	rte_free(sw_data);
	return -1;
}

static int
sw_event_timer_adapter_uninit(struct rte_event_timer_adapter *adapter)
{
	int ret;
	struct msg *m1, *m2;
	struct rte_event_timer_adapter_sw_data *sw_data =
						adapter->data->adapter_priv;

	rte_spinlock_lock(&sw_data->msgs_tailq_sl);

	/* Cancel outstanding rte_timers and free msg objects */
	m1 = TAILQ_FIRST(&sw_data->msgs_tailq_head);
	while (m1 != NULL) {
		EVTIM_LOG_DBG("freeing outstanding timer");
		m2 = TAILQ_NEXT(m1, msgs);

		rte_timer_stop_sync(&m1->tim);
		rte_mempool_put(sw_data->msg_pool, m1);

		m1 = m2;
	}

	rte_spinlock_unlock(&sw_data->msgs_tailq_sl);

	ret = rte_service_component_unregister(sw_data->service_id);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to unregister service component");
		return ret;
	}

	rte_ring_free(sw_data->msg_ring);
	rte_mempool_free(sw_data->msg_pool);
	rte_free(adapter->data->adapter_priv);

	return 0;
}

static inline int32_t
get_mapped_count_for_service(uint32_t service_id)
{
	int32_t core_count, i, mapped_count = 0;
	uint32_t lcore_arr[RTE_MAX_LCORE];

	core_count = rte_service_lcore_list(lcore_arr, RTE_MAX_LCORE);

	for (i = 0; i < core_count; i++)
		if (rte_service_map_lcore_get(service_id, lcore_arr[i]) == 1)
			mapped_count++;

	return mapped_count;
}

static int
sw_event_timer_adapter_start(const struct rte_event_timer_adapter *adapter)
{
	int mapped_count;
	struct rte_event_timer_adapter_sw_data *sw_data;

	sw_data = adapter->data->adapter_priv;

	/* Mapping the service to more than one service core can introduce
	 * delays while one thread is waiting to acquire a lock, so only allow
	 * one core to be mapped to the service.
	 */
	mapped_count = get_mapped_count_for_service(sw_data->service_id);

	if (mapped_count == 1)
		return rte_service_component_runstate_set(sw_data->service_id,
							  1);

	return mapped_count < 1 ? -ENOENT : -ENOTSUP;
}

static int
sw_event_timer_adapter_stop(const struct rte_event_timer_adapter *adapter)
{
	int ret;
	struct rte_event_timer_adapter_sw_data *sw_data =
						adapter->data->adapter_priv;

	ret = rte_service_component_runstate_set(sw_data->service_id, 0);
	if (ret < 0)
		return ret;

	/* Wait for the service to complete its final iteration before
	 * stopping.
	 */
	while (sw_data->service_phase != 0)
		rte_pause();

	rte_smp_rmb();

	return 0;
}

static void
sw_event_timer_adapter_get_info(const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_info *adapter_info)
{
	struct rte_event_timer_adapter_sw_data *sw_data;
	sw_data = adapter->data->adapter_priv;

	adapter_info->min_resolution_ns = sw_data->timer_tick_ns;
	adapter_info->max_tmo_ns = sw_data->max_tmo_ns;
}

static int
sw_event_timer_adapter_stats_get(const struct rte_event_timer_adapter *adapter,
				 struct rte_event_timer_adapter_stats *stats)
{
	struct rte_event_timer_adapter_sw_data *sw_data;
	sw_data = adapter->data->adapter_priv;
	*stats = sw_data->stats;
	return 0;
}

static int
sw_event_timer_adapter_stats_reset(
				const struct rte_event_timer_adapter *adapter)
{
	struct rte_event_timer_adapter_sw_data *sw_data;
	sw_data = adapter->data->adapter_priv;
	memset(&sw_data->stats, 0, sizeof(sw_data->stats));
	return 0;
}

static __rte_always_inline uint16_t
__sw_event_timer_arm_burst(const struct rte_event_timer_adapter *adapter,
			  struct rte_event_timer **evtims,
			  uint16_t nb_evtims)
{
	uint16_t i;
	int ret;
	struct rte_event_timer_adapter_sw_data *sw_data;
	struct msg *msgs[nb_evtims];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	/* Check that the service is running. */
	if (rte_service_runstate_get(adapter->data->service_id) != 1) {
		rte_errno = EINVAL;
		return 0;
	}
#endif

	sw_data = adapter->data->adapter_priv;

	ret = rte_mempool_get_bulk(sw_data->msg_pool, (void **)msgs, nb_evtims);
	if (ret < 0) {
		rte_errno = ENOSPC;
		return 0;
	}

	/* Let the service know we're producing messages for it to process */
	rte_atomic16_inc(&sw_data->message_producer_count);

	/* If the service is managing timers, wait for it to finish */
	while (sw_data->service_phase == 2)
		rte_pause();

	rte_smp_rmb();

	for (i = 0; i < nb_evtims; i++) {
		/* Don't modify the event timer state in these cases */
		if (evtims[i]->state == RTE_EVENT_TIMER_ARMED) {
			rte_errno = EALREADY;
			break;
		} else if (!(evtims[i]->state == RTE_EVENT_TIMER_NOT_ARMED ||
		    evtims[i]->state == RTE_EVENT_TIMER_CANCELED)) {
			rte_errno = EINVAL;
			break;
		}

		ret = check_timeout(evtims[i], adapter);
		if (ret == -1) {
			evtims[i]->state = RTE_EVENT_TIMER_ERROR_TOOLATE;
			rte_errno = EINVAL;
			break;
		}
		if (ret == -2) {
			evtims[i]->state = RTE_EVENT_TIMER_ERROR_TOOEARLY;
			rte_errno = EINVAL;
			break;
		}

		if (check_destination_event_queue(evtims[i], adapter) < 0) {
			evtims[i]->state = RTE_EVENT_TIMER_ERROR;
			rte_errno = EINVAL;
			break;
		}

		/* Checks passed, set up a message to enqueue */
		msgs[i]->type = MSG_TYPE_ARM;
		msgs[i]->evtim = evtims[i];

		/* Set the payload pointer if not set. */
		if (evtims[i]->ev.event_ptr == NULL)
			evtims[i]->ev.event_ptr = evtims[i];

		/* msg objects that get enqueued successfully will be freed
		 * either by a future cancel operation or by the timer
		 * expiration callback.
		 */
		if (rte_ring_enqueue(sw_data->msg_ring, msgs[i]) < 0) {
			rte_errno = ENOSPC;
			break;
		}

		EVTIM_LOG_DBG("enqueued ARM message to ring");

		evtims[i]->state = RTE_EVENT_TIMER_ARMED;
	}

	/* Let the service know we're done producing messages */
	rte_atomic16_dec(&sw_data->message_producer_count);

	if (i < nb_evtims)
		rte_mempool_put_bulk(sw_data->msg_pool, (void **)&msgs[i],
				     nb_evtims - i);

	return i;
}

static uint16_t
sw_event_timer_arm_burst(const struct rte_event_timer_adapter *adapter,
			 struct rte_event_timer **evtims,
			 uint16_t nb_evtims)
{
	return __sw_event_timer_arm_burst(adapter, evtims, nb_evtims);
}

static uint16_t
sw_event_timer_cancel_burst(const struct rte_event_timer_adapter *adapter,
			    struct rte_event_timer **evtims,
			    uint16_t nb_evtims)
{
	uint16_t i;
	int ret;
	struct rte_event_timer_adapter_sw_data *sw_data;
	struct msg *msgs[nb_evtims];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	/* Check that the service is running. */
	if (rte_service_runstate_get(adapter->data->service_id) != 1) {
		rte_errno = EINVAL;
		return 0;
	}
#endif

	sw_data = adapter->data->adapter_priv;

	ret = rte_mempool_get_bulk(sw_data->msg_pool, (void **)msgs, nb_evtims);
	if (ret < 0) {
		rte_errno = ENOSPC;
		return 0;
	}

	/* Let the service know we're producing messages for it to process */
	rte_atomic16_inc(&sw_data->message_producer_count);

	/* If the service could be modifying event timer states, wait */
	while (sw_data->service_phase == 2)
		rte_pause();

	rte_smp_rmb();

	for (i = 0; i < nb_evtims; i++) {
		/* Don't modify the event timer state in these cases */
		if (evtims[i]->state == RTE_EVENT_TIMER_CANCELED) {
			rte_errno = EALREADY;
			break;
		} else if (evtims[i]->state != RTE_EVENT_TIMER_ARMED) {
			rte_errno = EINVAL;
			break;
		}

		msgs[i]->type = MSG_TYPE_CANCEL;
		msgs[i]->evtim = evtims[i];

		if (rte_ring_enqueue(sw_data->msg_ring, msgs[i]) < 0) {
			rte_errno = ENOSPC;
			break;
		}

		EVTIM_LOG_DBG("enqueued CANCEL message to ring");

		evtims[i]->state = RTE_EVENT_TIMER_CANCELED;
	}

	/* Let the service know we're done producing messages */
	rte_atomic16_dec(&sw_data->message_producer_count);

	if (i < nb_evtims)
		rte_mempool_put_bulk(sw_data->msg_pool, (void **)&msgs[i],
				     nb_evtims - i);

	return i;
}

static uint16_t
sw_event_timer_arm_tmo_tick_burst(const struct rte_event_timer_adapter *adapter,
				  struct rte_event_timer **evtims,
				  uint64_t timeout_ticks,
				  uint16_t nb_evtims)
{
	int i;

	for (i = 0; i < nb_evtims; i++)
		evtims[i]->timeout_ticks = timeout_ticks;

	return __sw_event_timer_arm_burst(adapter, evtims, nb_evtims);
}

static const struct rte_event_timer_adapter_ops sw_event_adapter_timer_ops = {
	.init = sw_event_timer_adapter_init,
	.uninit = sw_event_timer_adapter_uninit,
	.start = sw_event_timer_adapter_start,
	.stop = sw_event_timer_adapter_stop,
	.get_info = sw_event_timer_adapter_get_info,
	.stats_get = sw_event_timer_adapter_stats_get,
	.stats_reset = sw_event_timer_adapter_stats_reset,
	.arm_burst = sw_event_timer_arm_burst,
	.arm_tmo_tick_burst = sw_event_timer_arm_tmo_tick_burst,
	.cancel_burst = sw_event_timer_cancel_burst,
};

RTE_INIT(event_timer_adapter_init_log);
static void
event_timer_adapter_init_log(void)
{
	evtim_logtype = rte_log_register("lib.eventdev.adapter.timer");
	if (evtim_logtype >= 0)
		rte_log_set_level(evtim_logtype, RTE_LOG_NOTICE);

	evtim_buffer_logtype = rte_log_register("lib.eventdev.adapter.timer."
						"buffer");
	if (evtim_buffer_logtype >= 0)
		rte_log_set_level(evtim_buffer_logtype, RTE_LOG_NOTICE);

	evtim_svc_logtype = rte_log_register("lib.eventdev.adapter.timer.svc");
	if (evtim_svc_logtype >= 0)
		rte_log_set_level(evtim_svc_logtype, RTE_LOG_NOTICE);
}
