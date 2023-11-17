/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation.
 * All rights reserved.
 */

#include <ctype.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_common.h>
#include <rte_timer.h>
#include <rte_service_component.h>
#include <rte_telemetry.h>
#include <rte_reciprocal.h>

#include "event_timer_adapter_pmd.h"
#include "eventdev_pmd.h"
#include "rte_event_timer_adapter.h"
#include "rte_eventdev.h"
#include "eventdev_trace.h"

#define DATA_MZ_NAME_MAX_LEN 64
#define DATA_MZ_NAME_FORMAT "rte_event_timer_adapter_data_%d"

RTE_LOG_REGISTER_SUFFIX(evtim_logtype, adapter.timer, NOTICE);
RTE_LOG_REGISTER_SUFFIX(evtim_buffer_logtype, adapter.timer, NOTICE);
RTE_LOG_REGISTER_SUFFIX(evtim_svc_logtype, adapter.timer.svc, NOTICE);

static struct rte_event_timer_adapter *adapters;

static const struct event_timer_adapter_ops swtim_ops;

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

static inline enum rte_timer_type
get_timer_type(const struct rte_event_timer_adapter *adapter)
{
	return (adapter->data->conf.flags &
			RTE_EVENT_TIMER_ADAPTER_F_PERIODIC) ?
			PERIODICAL : SINGLE;
}

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
	if (conf_arg != NULL)
		port_conf = conf_arg;
	else {
		port_conf = &def_port_conf;
		ret = rte_event_port_default_conf_get(dev_id, (port_id - 1),
						      port_conf);
		if (ret < 0)
			return ret;
	}

	dev_conf.nb_event_ports += 1;
	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_SINGLE_LINK)
		dev_conf.nb_single_link_event_port_queues += 1;

	ret = rte_event_dev_configure(dev_id, &dev_conf);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to configure event dev %u", dev_id);
		if (started)
			if (rte_event_dev_start(dev_id))
				return -EIO;

		return ret;
	}

	ret = rte_event_port_setup(dev_id, port_id, port_conf);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to setup event port %u on event dev %u",
			      port_id, dev_id);
		return ret;
	}

	*event_port_id = port_id;

	if (started)
		ret = rte_event_dev_start(dev_id);

	return ret;
}

struct rte_event_timer_adapter *
rte_event_timer_adapter_create(const struct rte_event_timer_adapter_conf *conf)
{
	return rte_event_timer_adapter_create_ext(conf, default_port_conf_cb,
						  NULL);
}

struct rte_event_timer_adapter *
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

	if (adapters == NULL) {
		adapters = rte_zmalloc("Eventdev",
				       sizeof(struct rte_event_timer_adapter) *
					       RTE_EVENT_TIMER_ADAPTER_NUM_MAX,
				       RTE_CACHE_LINE_SIZE);
		if (adapters == NULL) {
			rte_errno = ENOMEM;
			return NULL;
		}
	}

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
	if (dev->dev_ops->timer_adapter_caps_get) {
		ret = dev->dev_ops->timer_adapter_caps_get(dev,
				adapter->data->conf.flags,
				&adapter->data->caps, &adapter->ops);
		if (ret < 0) {
			rte_errno = -ret;
			goto free_memzone;
		}
	}

	if (!(adapter->data->caps &
	      RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT)) {
		FUNC_PTR_OR_NULL_RET_WITH_ERRNO(conf_cb, EINVAL);
		ret = conf_cb(adapter->data->id, adapter->data->event_dev_id,
			      &adapter->data->event_port_id, conf_arg);
		if (ret < 0) {
			rte_errno = -ret;
			goto free_memzone;
		}
	}

	/* If eventdev PMD did not provide ops, use default software
	 * implementation.
	 */
	if (adapter->ops == NULL)
		adapter->ops = &swtim_ops;

	/* Allow driver to do some setup */
	FUNC_PTR_OR_NULL_RET_WITH_ERRNO(adapter->ops->init, ENOTSUP);
	ret = adapter->ops->init(adapter);
	if (ret < 0) {
		rte_errno = -ret;
		goto free_memzone;
	}

	/* Set fast-path function pointers */
	adapter->arm_burst = adapter->ops->arm_burst;
	adapter->arm_tmo_tick_burst = adapter->ops->arm_tmo_tick_burst;
	adapter->cancel_burst = adapter->ops->cancel_burst;

	adapter->allocated = 1;

	rte_eventdev_trace_timer_adapter_create(adapter_id, adapter, conf,
		conf_cb);
	return adapter;

free_memzone:
	rte_memzone_free(adapter->data->mz);
	return NULL;
}

int
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

	rte_eventdev_trace_timer_adapter_get_info(adapter, adapter_info);

	return 0;
}

int
rte_event_timer_adapter_start(const struct rte_event_timer_adapter *adapter)
{
	int ret;

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->start, -EINVAL);

	if (adapter->data->started) {
		EVTIM_LOG_ERR("event timer adapter %"PRIu8" already started",
			      adapter->data->id);
		return -EALREADY;
	}

	ret = adapter->ops->start(adapter);
	if (ret < 0)
		return ret;

	adapter->data->started = 1;
	rte_eventdev_trace_timer_adapter_start(adapter);
	return 0;
}

int
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
	rte_eventdev_trace_timer_adapter_stop(adapter);
	return 0;
}

struct rte_event_timer_adapter *
rte_event_timer_adapter_lookup(uint16_t adapter_id)
{
	char name[DATA_MZ_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	struct rte_event_timer_adapter_data *data;
	struct rte_event_timer_adapter *adapter;
	int ret;
	struct rte_eventdev *dev;

	if (adapters == NULL) {
		adapters = rte_zmalloc("Eventdev",
				       sizeof(struct rte_event_timer_adapter) *
					       RTE_EVENT_TIMER_ADAPTER_NUM_MAX,
				       RTE_CACHE_LINE_SIZE);
		if (adapters == NULL) {
			rte_errno = ENOMEM;
			return NULL;
		}
	}

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
	if (dev->dev_ops->timer_adapter_caps_get) {
		ret = dev->dev_ops->timer_adapter_caps_get(dev,
				adapter->data->conf.flags,
				&adapter->data->caps, &adapter->ops);
		if (ret < 0) {
			rte_errno = EINVAL;
			return NULL;
		}
	}

	/* If eventdev PMD did not provide ops, use default software
	 * implementation.
	 */
	if (adapter->ops == NULL)
		adapter->ops = &swtim_ops;

	/* Set fast-path function pointers */
	adapter->arm_burst = adapter->ops->arm_burst;
	adapter->arm_tmo_tick_burst = adapter->ops->arm_tmo_tick_burst;
	adapter->cancel_burst = adapter->ops->cancel_burst;

	adapter->allocated = 1;

	rte_eventdev_trace_timer_adapter_lookup(adapter_id, adapter);

	return adapter;
}

int
rte_event_timer_adapter_free(struct rte_event_timer_adapter *adapter)
{
	int i, ret;

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

	ret = 0;
	for (i = 0; i < RTE_EVENT_TIMER_ADAPTER_NUM_MAX; i++)
		if (adapters[i].allocated)
			ret = adapters[i].allocated;

	if (!ret) {
		rte_free(adapters);
		adapters = NULL;
	}

	rte_eventdev_trace_timer_adapter_free(adapter);
	return 0;
}

int
rte_event_timer_adapter_service_id_get(struct rte_event_timer_adapter *adapter,
				       uint32_t *service_id)
{
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);

	if (service_id == NULL)
		return -EINVAL;

	if (adapter->data->service_inited && service_id != NULL)
		*service_id = adapter->data->service_id;

	rte_eventdev_trace_timer_adapter_service_id_get(adapter, *service_id);

	return adapter->data->service_inited ? 0 : -ESRCH;
}

int
rte_event_timer_adapter_stats_get(struct rte_event_timer_adapter *adapter,
				  struct rte_event_timer_adapter_stats *stats)
{
	rte_eventdev_trace_timer_adapter_stats_get(adapter, stats);

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->stats_get, -EINVAL);
	if (stats == NULL)
		return -EINVAL;

	return adapter->ops->stats_get(adapter, stats);
}

int
rte_event_timer_adapter_stats_reset(struct rte_event_timer_adapter *adapter)
{
	rte_eventdev_trace_timer_adapter_stats_reset(adapter);

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->stats_reset, -EINVAL);
	return adapter->ops->stats_reset(adapter);
}

int
rte_event_timer_remaining_ticks_get(
			const struct rte_event_timer_adapter *adapter,
			const struct rte_event_timer *evtim,
			uint64_t *ticks_remaining)
{
	rte_eventdev_trace_timer_remaining_ticks_get(adapter, evtim, ticks_remaining);

	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->ops->remaining_ticks_get, -ENOTSUP);

	if (ticks_remaining == NULL)
		return -EINVAL;

	return adapter->ops->remaining_ticks_get(adapter, evtim,
						 ticks_remaining);
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

#define EXP_TIM_BUF_SZ 128

struct event_buffer {
	size_t head;
	size_t tail;
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
	size_t head_idx;
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
	struct rte_event *events = bufp->events;
	size_t head_idx, tail_idx;
	uint16_t n = 0;

	/* Instead of modulus, bitwise AND with mask to get index. */
	head_idx = bufp->head & EVENT_BUFFER_MASK;
	tail_idx = bufp->tail & EVENT_BUFFER_MASK;

	RTE_ASSERT(head_idx < EVENT_BUFFER_SZ && tail_idx < EVENT_BUFFER_SZ);

	/* Determine the largest contiguous run we can attempt to enqueue to the
	 * event device.
	 */
	if (head_idx > tail_idx)
		n = head_idx - tail_idx;
	else if (head_idx < tail_idx)
		n = EVENT_BUFFER_SZ - tail_idx;
	else if (event_buffer_full(bufp))
		n = EVENT_BUFFER_SZ - tail_idx;
	else {
		*nb_events_flushed = 0;
		return;
	}

	n = RTE_MIN(EVENT_BUFFER_BATCHSZ, n);
	*nb_events_inv = 0;

	*nb_events_flushed = rte_event_enqueue_burst(dev_id, port_id,
						     &events[tail_idx], n);
	if (*nb_events_flushed != n) {
		if (rte_errno == EINVAL) {
			EVTIM_LOG_ERR("failed to enqueue invalid event - "
				      "dropping it");
			(*nb_events_inv)++;
		} else if (rte_errno == ENOSPC)
			rte_pause();
	}

	if (*nb_events_flushed > 0)
		EVTIM_BUF_LOG_DBG("enqueued %"PRIu16" timer events to event "
				  "device", *nb_events_flushed);

	bufp->tail = bufp->tail + *nb_events_flushed + *nb_events_inv;
}

/*
 * Software event timer adapter implementation
 */
struct swtim {
	/* Identifier of service executing timer management logic. */
	uint32_t service_id;
	/* The cycle count at which the adapter should next tick */
	uint64_t next_tick_cycles;
	/* The tick resolution used by adapter instance. May have been
	 * adjusted from what user requested
	 */
	uint64_t timer_tick_ns;
	/* Maximum timeout in nanoseconds allowed by adapter instance. */
	uint64_t max_tmo_ns;
	/* Buffered timer expiry events to be enqueued to an event device. */
	struct event_buffer buffer;
	/* Statistics */
	struct rte_event_timer_adapter_stats stats;
	/* Mempool of timer objects */
	struct rte_mempool *tim_pool;
	/* Back pointer for convenience */
	struct rte_event_timer_adapter *adapter;
	/* Identifier of timer data instance */
	uint32_t timer_data_id;
	/* Track which cores have actually armed a timer */
	struct {
		RTE_ATOMIC(uint16_t) v;
	} __rte_cache_aligned in_use[RTE_MAX_LCORE];
	/* Track which cores' timer lists should be polled */
	RTE_ATOMIC(unsigned int) poll_lcores[RTE_MAX_LCORE];
	/* The number of lists that should be polled */
	RTE_ATOMIC(int) n_poll_lcores;
	/* Timers which have expired and can be returned to a mempool */
	struct rte_timer *expired_timers[EXP_TIM_BUF_SZ];
	/* The number of timers that can be returned to a mempool */
	size_t n_expired_timers;
};

static inline struct swtim *
swtim_pmd_priv(const struct rte_event_timer_adapter *adapter)
{
	return adapter->data->adapter_priv;
}

static void
swtim_callback(struct rte_timer *tim)
{
	struct rte_event_timer *evtim = tim->arg;
	struct rte_event_timer_adapter *adapter;
	unsigned int lcore = rte_lcore_id();
	struct swtim *sw;
	uint16_t nb_evs_flushed = 0;
	uint16_t nb_evs_invalid = 0;
	uint64_t opaque;
	int ret;
	int n_lcores;
	enum rte_timer_type type;

	opaque = evtim->impl_opaque[1];
	adapter = (struct rte_event_timer_adapter *)(uintptr_t)opaque;
	sw = swtim_pmd_priv(adapter);
	type = get_timer_type(adapter);

	if (unlikely(sw->in_use[lcore].v == 0)) {
		sw->in_use[lcore].v = 1;
		n_lcores = rte_atomic_fetch_add_explicit(&sw->n_poll_lcores, 1,
					     rte_memory_order_relaxed);
		rte_atomic_store_explicit(&sw->poll_lcores[n_lcores], lcore,
				rte_memory_order_relaxed);
	}

	ret = event_buffer_add(&sw->buffer, &evtim->ev);
	if (ret < 0) {
		if (type == SINGLE) {
			/* If event buffer is full, put timer back in list with
			 * immediate expiry value, so that we process it again
			 * on the next iteration.
			 */
			ret = rte_timer_alt_reset(sw->timer_data_id, tim, 0,
						SINGLE,	lcore, NULL, evtim);
			if (ret < 0) {
				EVTIM_LOG_DBG("event buffer full, failed to "
						"reset timer with immediate "
						"expiry value");
			} else {
				sw->stats.evtim_retry_count++;
				EVTIM_LOG_DBG("event buffer full, resetting "
						"rte_timer with immediate "
						"expiry value");
			}
		} else {
			sw->stats.evtim_drop_count++;
		}

	} else {
		EVTIM_BUF_LOG_DBG("buffered an event timer expiry event");

		/* Empty the buffer here, if necessary, to free older expired
		 * timers only
		 */
		if (unlikely(sw->n_expired_timers == EXP_TIM_BUF_SZ)) {
			rte_mempool_put_bulk(sw->tim_pool,
					     (void **)sw->expired_timers,
					     sw->n_expired_timers);
			sw->n_expired_timers = 0;
		}

		/* Don't free rte_timer for a periodic event timer until
		 * it is cancelled
		 */
		if (type == SINGLE)
			sw->expired_timers[sw->n_expired_timers++] = tim;
		sw->stats.evtim_exp_count++;

		if (type == SINGLE)
			rte_atomic_store_explicit(&evtim->state, RTE_EVENT_TIMER_NOT_ARMED,
				rte_memory_order_release);
	}

	if (event_buffer_batch_ready(&sw->buffer)) {
		event_buffer_flush(&sw->buffer,
				   adapter->data->event_dev_id,
				   adapter->data->event_port_id,
				   &nb_evs_flushed,
				   &nb_evs_invalid);

		sw->stats.ev_enq_count += nb_evs_flushed;
		sw->stats.ev_inv_count += nb_evs_invalid;
	}
}

static __rte_always_inline int
get_timeout_cycles(struct rte_event_timer *evtim,
		   const struct rte_event_timer_adapter *adapter,
		   uint64_t *timeout_cycles)
{
	static struct rte_reciprocal_u64 nsecpersec_inverse;
	static uint64_t timer_hz;
	uint64_t rem_cycles, secs_cycles = 0;
	uint64_t secs, timeout_nsecs;
	uint64_t nsecpersec;
	struct swtim *sw;

	sw = swtim_pmd_priv(adapter);
	nsecpersec = (uint64_t)NSECPERSEC;

	timeout_nsecs = evtim->timeout_ticks * sw->timer_tick_ns;
	if (timeout_nsecs > sw->max_tmo_ns)
		return -1;
	if (timeout_nsecs < sw->timer_tick_ns)
		return -2;

	/* Set these values in the first invocation */
	if (!timer_hz) {
		timer_hz = rte_get_timer_hz();
		nsecpersec_inverse = rte_reciprocal_value_u64(nsecpersec);
	}

	/* If timeout_nsecs > nsecpersec, decrease timeout_nsecs by the number
	 * of whole seconds it contains and convert that value to a number
	 * of cycles. This keeps timeout_nsecs in the interval [0..nsecpersec)
	 * in order to avoid overflow when we later multiply by timer_hz.
	 */
	if (timeout_nsecs > nsecpersec) {
		secs = rte_reciprocal_divide_u64(timeout_nsecs,
						 &nsecpersec_inverse);
		secs_cycles = secs * timer_hz;
		timeout_nsecs -= secs * nsecpersec;
	}

	rem_cycles = rte_reciprocal_divide_u64(timeout_nsecs * timer_hz,
					       &nsecpersec_inverse);

	*timeout_cycles = secs_cycles + rem_cycles;

	return 0;
}

/* This function returns true if one or more (adapter) ticks have occurred since
 * the last time it was called.
 */
static inline bool
swtim_did_tick(struct swtim *sw)
{
	uint64_t cycles_per_adapter_tick, start_cycles;
	uint64_t *next_tick_cyclesp;

	next_tick_cyclesp = &sw->next_tick_cycles;
	cycles_per_adapter_tick = sw->timer_tick_ns *
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

	if ((ret == 0 && evtim->ev.sched_type == sched_type) ||
	    ret == -EOVERFLOW)
		return 0;

	return -1;
}

static int
swtim_service_func(void *arg)
{
	struct rte_event_timer_adapter *adapter = arg;
	struct swtim *sw = swtim_pmd_priv(adapter);
	uint16_t nb_evs_flushed = 0;
	uint16_t nb_evs_invalid = 0;
	const uint64_t prior_enq_count = sw->stats.ev_enq_count;

	if (swtim_did_tick(sw)) {
		rte_timer_alt_manage(sw->timer_data_id,
				     (unsigned int *)(uintptr_t)sw->poll_lcores,
				     sw->n_poll_lcores,
				     swtim_callback);

		/* Return expired timer objects back to mempool */
		rte_mempool_put_bulk(sw->tim_pool, (void **)sw->expired_timers,
				     sw->n_expired_timers);
		sw->n_expired_timers = 0;

		sw->stats.adapter_tick_count++;
	}

	event_buffer_flush(&sw->buffer,
			   adapter->data->event_dev_id,
			   adapter->data->event_port_id,
			   &nb_evs_flushed,
			   &nb_evs_invalid);

	sw->stats.ev_enq_count += nb_evs_flushed;
	sw->stats.ev_inv_count += nb_evs_invalid;

	rte_event_maintain(adapter->data->event_dev_id,
			   adapter->data->event_port_id, 0);

	return prior_enq_count == sw->stats.ev_enq_count ? -EAGAIN : 0;
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

	for (i = 0;; i++) {
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

static int
swtim_init(struct rte_event_timer_adapter *adapter)
{
	int i, ret;
	struct swtim *sw;
	unsigned int flags;
	struct rte_service_spec service;

	/* Allocate storage for private data area */
#define SWTIM_NAMESIZE 32
	char swtim_name[SWTIM_NAMESIZE];
	snprintf(swtim_name, SWTIM_NAMESIZE, "swtim_%"PRIu8,
			adapter->data->id);
	sw = rte_zmalloc_socket(swtim_name, sizeof(*sw), RTE_CACHE_LINE_SIZE,
			adapter->data->socket_id);
	if (sw == NULL) {
		EVTIM_LOG_ERR("failed to allocate space for private data");
		rte_errno = ENOMEM;
		return -1;
	}

	/* Connect storage to adapter instance */
	adapter->data->adapter_priv = sw;
	sw->adapter = adapter;

	sw->timer_tick_ns = adapter->data->conf.timer_tick_ns;
	sw->max_tmo_ns = adapter->data->conf.max_tmo_ns;

	/* Create a timer pool */
	char pool_name[SWTIM_NAMESIZE];
	snprintf(pool_name, SWTIM_NAMESIZE, "swtim_pool_%"PRIu8,
		 adapter->data->id);
	/* Optimal mempool size is a power of 2 minus one */
	uint64_t nb_timers = rte_align64pow2(adapter->data->conf.nb_timers);
	int pool_size = nb_timers - 1;
	int cache_size = compute_msg_mempool_cache_size(
				adapter->data->conf.nb_timers, nb_timers);
	flags = 0; /* pool is multi-producer, multi-consumer */
	sw->tim_pool = rte_mempool_create(pool_name, pool_size,
			sizeof(struct rte_timer), cache_size, 0, NULL, NULL,
			NULL, NULL, adapter->data->socket_id, flags);
	if (sw->tim_pool == NULL) {
		EVTIM_LOG_ERR("failed to create timer object mempool");
		rte_errno = ENOMEM;
		goto free_alloc;
	}

	/* Initialize the variables that track in-use timer lists */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		sw->in_use[i].v = 0;

	/* Initialize the timer subsystem and allocate timer data instance */
	ret = rte_timer_subsystem_init();
	if (ret < 0) {
		if (ret != -EALREADY) {
			EVTIM_LOG_ERR("failed to initialize timer subsystem");
			rte_errno = -ret;
			goto free_mempool;
		}
	}

	ret = rte_timer_data_alloc(&sw->timer_data_id);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to allocate timer data instance");
		rte_errno = -ret;
		goto free_mempool;
	}

	/* Initialize timer event buffer */
	event_buffer_init(&sw->buffer);

	sw->adapter = adapter;

	/* Register a service component to run adapter logic */
	memset(&service, 0, sizeof(service));
	snprintf(service.name, RTE_SERVICE_NAME_MAX,
		 "swtim_svc_%"PRIu8, adapter->data->id);
	service.socket_id = adapter->data->socket_id;
	service.callback = swtim_service_func;
	service.callback_userdata = adapter;
	service.capabilities &= ~(RTE_SERVICE_CAP_MT_SAFE);
	ret = rte_service_component_register(&service, &sw->service_id);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to register service %s with id %"PRIu32
			      ": err = %d", service.name, sw->service_id,
			      ret);

		rte_errno = ENOSPC;
		goto free_mempool;
	}

	EVTIM_LOG_DBG("registered service %s with id %"PRIu32, service.name,
		      sw->service_id);

	adapter->data->service_id = sw->service_id;
	adapter->data->service_inited = 1;

	return 0;
free_mempool:
	rte_mempool_free(sw->tim_pool);
free_alloc:
	rte_free(sw);
	return -1;
}

static void
swtim_free_tim(struct rte_timer *tim, void *arg)
{
	struct swtim *sw = arg;

	rte_mempool_put(sw->tim_pool, tim);
}

/* Traverse the list of outstanding timers and put them back in the mempool
 * before freeing the adapter to avoid leaking the memory.
 */
static int
swtim_uninit(struct rte_event_timer_adapter *adapter)
{
	int ret;
	struct swtim *sw = swtim_pmd_priv(adapter);

	/* Free outstanding timers */
	rte_timer_stop_all(sw->timer_data_id,
			   (unsigned int *)(uintptr_t)sw->poll_lcores,
			   sw->n_poll_lcores,
			   swtim_free_tim,
			   sw);

	ret = rte_timer_data_dealloc(sw->timer_data_id);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to deallocate timer data instance");
		return ret;
	}

	ret = rte_service_component_unregister(sw->service_id);
	if (ret < 0) {
		EVTIM_LOG_ERR("failed to unregister service component");
		return ret;
	}

	rte_mempool_free(sw->tim_pool);
	rte_free(sw);
	adapter->data->adapter_priv = NULL;

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
swtim_start(const struct rte_event_timer_adapter *adapter)
{
	int mapped_count;
	struct swtim *sw = swtim_pmd_priv(adapter);

	/* Mapping the service to more than one service core can introduce
	 * delays while one thread is waiting to acquire a lock, so only allow
	 * one core to be mapped to the service.
	 *
	 * Note: the service could be modified such that it spreads cores to
	 * poll over multiple service instances.
	 */
	mapped_count = get_mapped_count_for_service(sw->service_id);

	if (mapped_count != 1)
		return mapped_count < 1 ? -ENOENT : -ENOTSUP;

	return rte_service_component_runstate_set(sw->service_id, 1);
}

static int
swtim_stop(const struct rte_event_timer_adapter *adapter)
{
	int ret;
	struct swtim *sw = swtim_pmd_priv(adapter);

	ret = rte_service_component_runstate_set(sw->service_id, 0);
	if (ret < 0)
		return ret;

	/* Wait for the service to complete its final iteration */
	while (rte_service_may_be_active(sw->service_id))
		rte_pause();

	return 0;
}

static void
swtim_get_info(const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_info *adapter_info)
{
	struct swtim *sw = swtim_pmd_priv(adapter);
	adapter_info->min_resolution_ns = sw->timer_tick_ns;
	adapter_info->max_tmo_ns = sw->max_tmo_ns;
}

static int
swtim_stats_get(const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_stats *stats)
{
	struct swtim *sw = swtim_pmd_priv(adapter);
	*stats = sw->stats; /* structure copy */
	return 0;
}

static int
swtim_stats_reset(const struct rte_event_timer_adapter *adapter)
{
	struct swtim *sw = swtim_pmd_priv(adapter);
	memset(&sw->stats, 0, sizeof(sw->stats));
	return 0;
}

static int
swtim_remaining_ticks_get(const struct rte_event_timer_adapter *adapter,
			  const struct rte_event_timer *evtim,
			  uint64_t *ticks_remaining)
{
	uint64_t nsecs_per_adapter_tick, opaque, cycles_remaining;
	enum rte_event_timer_state n_state;
	double nsecs_per_cycle;
	struct rte_timer *tim;
	uint64_t cur_cycles;

	/* Check that timer is armed */
	n_state = rte_atomic_load_explicit(&evtim->state, rte_memory_order_acquire);
	if (n_state != RTE_EVENT_TIMER_ARMED)
		return -EINVAL;

	opaque = evtim->impl_opaque[0];
	tim = (struct rte_timer *)(uintptr_t)opaque;

	cur_cycles = rte_get_timer_cycles();
	if (cur_cycles > tim->expire) {
		*ticks_remaining = 0;
		return 0;
	}

	cycles_remaining = tim->expire - cur_cycles;
	nsecs_per_cycle = (double)NSECPERSEC / rte_get_timer_hz();
	nsecs_per_adapter_tick = adapter->data->conf.timer_tick_ns;

	*ticks_remaining = (uint64_t)ceil((cycles_remaining * nsecs_per_cycle) /
					  nsecs_per_adapter_tick);

	return 0;
}

static uint16_t
__swtim_arm_burst(const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer **evtims,
		uint16_t nb_evtims)
{
	int i, ret;
	struct swtim *sw = swtim_pmd_priv(adapter);
	uint32_t lcore_id = rte_lcore_id();
	struct rte_timer *tim, *tims[nb_evtims];
	uint64_t cycles;
	int n_lcores;
	/* Timer list for this lcore is not in use. */
	uint16_t exp_state = 0;
	enum rte_event_timer_state n_state;
	enum rte_timer_type type = SINGLE;

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	/* Check that the service is running. */
	if (rte_service_runstate_get(adapter->data->service_id) != 1) {
		rte_errno = EINVAL;
		return 0;
	}
#endif

	/* Adjust lcore_id if non-EAL thread. Arbitrarily pick the timer list of
	 * the highest lcore to insert such timers into
	 */
	if (lcore_id == LCORE_ID_ANY)
		lcore_id = RTE_MAX_LCORE - 1;

	/* If this is the first time we're arming an event timer on this lcore,
	 * mark this lcore as "in use"; this will cause the service
	 * function to process the timer list that corresponds to this lcore.
	 * The atomic compare-and-swap operation can prevent the race condition
	 * on in_use flag between multiple non-EAL threads.
	 */
	if (unlikely(rte_atomic_compare_exchange_strong_explicit(&sw->in_use[lcore_id].v,
			&exp_state, 1,
			rte_memory_order_relaxed, rte_memory_order_relaxed))) {
		EVTIM_LOG_DBG("Adding lcore id = %u to list of lcores to poll",
			      lcore_id);
		n_lcores = rte_atomic_fetch_add_explicit(&sw->n_poll_lcores, 1,
					     rte_memory_order_relaxed);
		rte_atomic_store_explicit(&sw->poll_lcores[n_lcores], lcore_id,
				rte_memory_order_relaxed);
	}

	ret = rte_mempool_get_bulk(sw->tim_pool, (void **)tims,
				   nb_evtims);
	if (ret < 0) {
		rte_errno = ENOSPC;
		return 0;
	}

	/* update timer type for periodic adapter */
	type = get_timer_type(adapter);

	for (i = 0; i < nb_evtims; i++) {
		n_state = rte_atomic_load_explicit(&evtims[i]->state, rte_memory_order_acquire);
		if (n_state == RTE_EVENT_TIMER_ARMED) {
			rte_errno = EALREADY;
			break;
		} else if (!(n_state == RTE_EVENT_TIMER_NOT_ARMED ||
			     n_state == RTE_EVENT_TIMER_CANCELED)) {
			rte_errno = EINVAL;
			break;
		}

		if (unlikely(check_destination_event_queue(evtims[i],
							   adapter) < 0)) {
			rte_atomic_store_explicit(&evtims[i]->state,
					RTE_EVENT_TIMER_ERROR,
					rte_memory_order_relaxed);
			rte_errno = EINVAL;
			break;
		}

		tim = tims[i];
		rte_timer_init(tim);

		evtims[i]->impl_opaque[0] = (uintptr_t)tim;
		evtims[i]->impl_opaque[1] = (uintptr_t)adapter;

		ret = get_timeout_cycles(evtims[i], adapter, &cycles);
		if (unlikely(ret == -1)) {
			rte_atomic_store_explicit(&evtims[i]->state,
					RTE_EVENT_TIMER_ERROR_TOOLATE,
					rte_memory_order_relaxed);
			rte_errno = EINVAL;
			break;
		} else if (unlikely(ret == -2)) {
			rte_atomic_store_explicit(&evtims[i]->state,
					RTE_EVENT_TIMER_ERROR_TOOEARLY,
					rte_memory_order_relaxed);
			rte_errno = EINVAL;
			break;
		}

		ret = rte_timer_alt_reset(sw->timer_data_id, tim, cycles,
					  type, lcore_id, NULL, evtims[i]);
		if (ret < 0) {
			/* tim was in RUNNING or CONFIG state */
			rte_atomic_store_explicit(&evtims[i]->state,
					RTE_EVENT_TIMER_ERROR,
					rte_memory_order_release);
			break;
		}

		EVTIM_LOG_DBG("armed an event timer");
		/* RELEASE ordering guarantees the adapter specific value
		 * changes observed before the update of state.
		 */
		rte_atomic_store_explicit(&evtims[i]->state, RTE_EVENT_TIMER_ARMED,
				rte_memory_order_release);
	}

	if (i < nb_evtims)
		rte_mempool_put_bulk(sw->tim_pool,
				     (void **)&tims[i], nb_evtims - i);

	return i;
}

static uint16_t
swtim_arm_burst(const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer **evtims,
		uint16_t nb_evtims)
{
	return __swtim_arm_burst(adapter, evtims, nb_evtims);
}

static uint16_t
swtim_cancel_burst(const struct rte_event_timer_adapter *adapter,
		   struct rte_event_timer **evtims,
		   uint16_t nb_evtims)
{
	int i, ret;
	struct rte_timer *timp;
	uint64_t opaque;
	struct swtim *sw = swtim_pmd_priv(adapter);
	enum rte_event_timer_state n_state;

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	/* Check that the service is running. */
	if (rte_service_runstate_get(adapter->data->service_id) != 1) {
		rte_errno = EINVAL;
		return 0;
	}
#endif

	for (i = 0; i < nb_evtims; i++) {
		/* Don't modify the event timer state in these cases */
		/* ACQUIRE ordering guarantees the access of implementation
		 * specific opaque data under the correct state.
		 */
		n_state = rte_atomic_load_explicit(&evtims[i]->state, rte_memory_order_acquire);
		if (n_state == RTE_EVENT_TIMER_CANCELED) {
			rte_errno = EALREADY;
			break;
		} else if (n_state != RTE_EVENT_TIMER_ARMED) {
			rte_errno = EINVAL;
			break;
		}

		opaque = evtims[i]->impl_opaque[0];
		timp = (struct rte_timer *)(uintptr_t)opaque;
		RTE_ASSERT(timp != NULL);

		ret = rte_timer_alt_stop(sw->timer_data_id, timp);
		if (ret < 0) {
			/* Timer is running or being configured */
			rte_errno = EAGAIN;
			break;
		}

		rte_mempool_put(sw->tim_pool, (void **)timp);

		/* The RELEASE ordering here pairs with atomic ordering
		 * to make sure the state update data observed between
		 * threads.
		 */
		rte_atomic_store_explicit(&evtims[i]->state, RTE_EVENT_TIMER_CANCELED,
				rte_memory_order_release);
	}

	return i;
}

static uint16_t
swtim_arm_tmo_tick_burst(const struct rte_event_timer_adapter *adapter,
			 struct rte_event_timer **evtims,
			 uint64_t timeout_ticks,
			 uint16_t nb_evtims)
{
	int i;

	for (i = 0; i < nb_evtims; i++)
		evtims[i]->timeout_ticks = timeout_ticks;

	return __swtim_arm_burst(adapter, evtims, nb_evtims);
}

static const struct event_timer_adapter_ops swtim_ops = {
	.init = swtim_init,
	.uninit = swtim_uninit,
	.start = swtim_start,
	.stop = swtim_stop,
	.get_info = swtim_get_info,
	.stats_get = swtim_stats_get,
	.stats_reset = swtim_stats_reset,
	.arm_burst = swtim_arm_burst,
	.arm_tmo_tick_burst = swtim_arm_tmo_tick_burst,
	.cancel_burst = swtim_cancel_burst,
	.remaining_ticks_get = swtim_remaining_ticks_get,
};

static int
handle_ta_info(const char *cmd __rte_unused, const char *params,
		struct rte_tel_data *d)
{
	struct rte_event_timer_adapter_info adapter_info;
	struct rte_event_timer_adapter *adapter;
	uint16_t adapter_id;
	int ret;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	adapter_id = atoi(params);

	if (adapter_id >= RTE_EVENT_TIMER_ADAPTER_NUM_MAX) {
		EVTIM_LOG_ERR("Invalid timer adapter id %u", adapter_id);
		return -EINVAL;
	}

	adapter = &adapters[adapter_id];

	ret = rte_event_timer_adapter_get_info(adapter, &adapter_info);
	if (ret < 0) {
		EVTIM_LOG_ERR("Failed to get info for timer adapter id %u", adapter_id);
		return ret;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint(d, "timer_adapter_id", adapter_id);
	rte_tel_data_add_dict_uint(d, "min_resolution_ns",
				   adapter_info.min_resolution_ns);
	rte_tel_data_add_dict_uint(d, "max_tmo_ns", adapter_info.max_tmo_ns);
	rte_tel_data_add_dict_uint(d, "event_dev_id",
				   adapter_info.conf.event_dev_id);
	rte_tel_data_add_dict_uint(d, "socket_id",
				   adapter_info.conf.socket_id);
	rte_tel_data_add_dict_uint(d, "clk_src", adapter_info.conf.clk_src);
	rte_tel_data_add_dict_uint(d, "timer_tick_ns",
				   adapter_info.conf.timer_tick_ns);
	rte_tel_data_add_dict_uint(d, "nb_timers",
				   adapter_info.conf.nb_timers);
	rte_tel_data_add_dict_uint(d, "flags", adapter_info.conf.flags);

	return 0;
}

static int
handle_ta_stats(const char *cmd __rte_unused, const char *params,
		struct rte_tel_data *d)
{
	struct rte_event_timer_adapter_stats stats;
	struct rte_event_timer_adapter *adapter;
	uint16_t adapter_id;
	int ret;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	adapter_id = atoi(params);

	if (adapter_id >= RTE_EVENT_TIMER_ADAPTER_NUM_MAX) {
		EVTIM_LOG_ERR("Invalid timer adapter id %u", adapter_id);
		return -EINVAL;
	}

	adapter = &adapters[adapter_id];

	ret = rte_event_timer_adapter_stats_get(adapter, &stats);
	if (ret < 0) {
		EVTIM_LOG_ERR("Failed to get stats for timer adapter id %u", adapter_id);
		return ret;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint(d, "timer_adapter_id", adapter_id);
	rte_tel_data_add_dict_uint(d, "evtim_exp_count",
				   stats.evtim_exp_count);
	rte_tel_data_add_dict_uint(d, "ev_enq_count", stats.ev_enq_count);
	rte_tel_data_add_dict_uint(d, "ev_inv_count", stats.ev_inv_count);
	rte_tel_data_add_dict_uint(d, "evtim_retry_count",
				   stats.evtim_retry_count);
	rte_tel_data_add_dict_uint(d, "adapter_tick_count",
				   stats.adapter_tick_count);

	return 0;
}

RTE_INIT(ta_init_telemetry)
{
	rte_telemetry_register_cmd("/eventdev/ta_info",
		handle_ta_info,
		"Returns Timer adapter info. Parameter: Timer adapter id");

	rte_telemetry_register_cmd("/eventdev/ta_stats",
		handle_ta_stats,
		"Returns Timer adapter stats. Parameter: Timer adapter id");
}
