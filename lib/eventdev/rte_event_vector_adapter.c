/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell International Ltd.
 * All rights reserved.
 */

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mcslock.h>
#include <rte_service_component.h>
#include <rte_tailq.h>

#include <eal_export.h>

#include "event_vector_adapter_pmd.h"
#include "eventdev_pmd.h"
#include "rte_event_vector_adapter.h"

#define ADAPTER_ID(dev_id, queue_id, adapter_id)                                                   \
	((uint32_t)dev_id << 16 | (uint32_t)queue_id << 8 | (uint32_t)adapter_id)
#define DEV_ID_FROM_ADAPTER_ID(adapter_id)     ((adapter_id >> 16) & 0xFF)
#define QUEUE_ID_FROM_ADAPTER_ID(adapter_id)   ((adapter_id >> 8) & 0xFF)
#define ADAPTER_ID_FROM_ADAPTER_ID(adapter_id) (adapter_id & 0xFF)

#define MZ_NAME_MAX_LEN	    64
#define DATA_MZ_NAME_FORMAT "vector_adapter_data_%d_%d_%d"
#define MAX_VECTOR_SIZE	    1024
#define MIN_VECTOR_SIZE	    1
#define MAX_VECTOR_NS	    1E9
#define MIN_VECTOR_NS	    1E5
#define SERVICE_RING_SZ	    1024
#define SERVICE_DEQ_SZ	    16
#define SERVICE_PEND_LIST   32

RTE_LOG_REGISTER_SUFFIX(ev_vector_logtype, adapter.vector, NOTICE);
#define RTE_LOGTYPE_EVVEC ev_vector_logtype

struct rte_event_vector_adapter *adapters[RTE_EVENT_MAX_DEVS][RTE_EVENT_MAX_QUEUES_PER_DEV];

#define EVVEC_LOG(level, logtype, ...)                                                             \
	RTE_LOG_LINE_PREFIX(level, logtype,                                                        \
			    "EVVEC: %s() line %u: ", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)
#define EVVEC_LOG_ERR(...) EVVEC_LOG(ERR, EVVEC, __VA_ARGS__)

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
#define EVVEC_LOG_DBG(...) EVVEC_LOG(DEBUG, EVVEC, __VA_ARGS__)
#else
#define EVVEC_LOG_DBG(...) /* No debug logging */
#endif

#define PTR_VALID_OR_ERR_RET(ptr, retval)                                                          \
	do {                                                                                       \
		if (ptr == NULL) {                                                                 \
			rte_errno = EINVAL;                                                        \
			return retval;                                                             \
		}                                                                                  \
	} while (0)

static const struct event_vector_adapter_ops sw_ops;
static const struct rte_event_vector_adapter_info sw_info;

static int
validate_conf(const struct rte_event_vector_adapter_conf *conf,
	      struct rte_event_vector_adapter_info *info)
{
	int rc = -EINVAL;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(conf->event_dev_id, rc);

	if (conf->vector_sz < info->min_vector_sz || conf->vector_sz > info->max_vector_sz) {
		EVVEC_LOG_DBG("invalid vector size %u, should be between %u and %u",
			      conf->vector_sz, info->min_vector_sz, info->max_vector_sz);
		return rc;
	}

	if (conf->vector_timeout_ns < info->min_vector_timeout_ns ||
	    conf->vector_timeout_ns > info->max_vector_timeout_ns) {
		EVVEC_LOG_DBG("invalid vector timeout %" PRIu64 ", should be between %" PRIu64
			      " and %" PRIu64,
			      conf->vector_timeout_ns, info->min_vector_timeout_ns,
			      info->max_vector_timeout_ns);
		return rc;
	}

	if (conf->vector_mp == NULL) {
		EVVEC_LOG_DBG("invalid mempool for vector adapter");
		return rc;
	}

	if (info->log2_sz && rte_is_power_of_2(conf->vector_sz) != 0) {
		EVVEC_LOG_DBG("invalid vector size %u, should be a power of 2", conf->vector_sz);
		return rc;
	}

	return 0;
}

static int
default_port_conf_cb(uint8_t event_dev_id, uint8_t *event_port_id, void *conf_arg)
{
	struct rte_event_port_conf *port_conf, def_port_conf = {0};
	struct rte_event_dev_config dev_conf;
	struct rte_eventdev *dev;
	uint8_t port_id;
	uint8_t dev_id;
	int started;
	int ret;

	dev = &rte_eventdevs[event_dev_id];
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
		ret = rte_event_port_default_conf_get(dev_id, (port_id - 1), port_conf);
		if (ret < 0)
			return ret;
	}

	dev_conf.nb_event_ports += 1;
	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_SINGLE_LINK)
		dev_conf.nb_single_link_event_port_queues += 1;

	ret = rte_event_dev_configure(dev_id, &dev_conf);
	if (ret < 0) {
		EVVEC_LOG_ERR("failed to configure event dev %u", dev_id);
		if (started)
			if (rte_event_dev_start(dev_id))
				return -EIO;

		return ret;
	}

	ret = rte_event_port_setup(dev_id, port_id, port_conf);
	if (ret < 0) {
		EVVEC_LOG_ERR("failed to setup event port %u on event dev %u", port_id, dev_id);
		return ret;
	}

	*event_port_id = port_id;

	if (started)
		ret = rte_event_dev_start(dev_id);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_create, 25.07)
struct rte_event_vector_adapter *
rte_event_vector_adapter_create(const struct rte_event_vector_adapter_conf *conf)
{
	return rte_event_vector_adapter_create_ext(conf, default_port_conf_cb, NULL);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_create_ext, 25.07)
struct rte_event_vector_adapter *
rte_event_vector_adapter_create_ext(const struct rte_event_vector_adapter_conf *conf,
				    rte_event_vector_adapter_port_conf_cb_t conf_cb, void *conf_arg)
{
	struct rte_event_vector_adapter *adapter = NULL;
	struct rte_event_vector_adapter_info info;
	char mz_name[MZ_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	struct rte_eventdev *dev;
	uint32_t caps;
	int i, n, rc;

	PTR_VALID_OR_ERR_RET(conf, NULL);

	if (adapters[conf->event_dev_id][conf->ev.queue_id] == NULL) {
		adapters[conf->event_dev_id][conf->ev.queue_id] =
			rte_zmalloc("rte_event_vector_adapter",
				    sizeof(struct rte_event_vector_adapter) *
					    RTE_EVENT_VECTOR_ADAPTER_MAX_INSTANCE_PER_QUEUE,
				    RTE_CACHE_LINE_SIZE);
		if (adapters[conf->event_dev_id][conf->ev.queue_id] == NULL) {
			EVVEC_LOG_DBG("failed to allocate memory for vector adapters");
			rte_errno = ENOMEM;
			return NULL;
		}
	}

	for (i = 0; i < RTE_EVENT_VECTOR_ADAPTER_MAX_INSTANCE_PER_QUEUE; i++) {
		if (adapters[conf->event_dev_id][conf->ev.queue_id][i].used == false) {
			adapter = &adapters[conf->event_dev_id][conf->ev.queue_id][i];
			adapter->adapter_id = ADAPTER_ID(conf->event_dev_id, conf->ev.queue_id, i);
			adapter->used = true;
			break;
		}
		EVVEC_LOG_DBG("adapter %u is already in use", i);
		rte_errno = EEXIST;
		return NULL;
	}

	if (adapter == NULL) {
		EVVEC_LOG_DBG("no available vector adapters");
		rte_errno = ENODEV;
		return NULL;
	}

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(conf->event_dev_id, NULL);

	dev = &rte_eventdevs[conf->event_dev_id];
	if (dev->dev_ops->vector_adapter_caps_get != NULL &&
	    dev->dev_ops->vector_adapter_info_get != NULL) {
		rc = dev->dev_ops->vector_adapter_caps_get(dev, &caps, &adapter->ops);
		if (rc < 0) {
			EVVEC_LOG_DBG("failed to get vector adapter capabilities rc = %d", rc);
			rte_errno = ENOTSUP;
			goto error;
		}

		rc = dev->dev_ops->vector_adapter_info_get(dev, &info);
		if (rc < 0) {
			adapter->ops = NULL;
			EVVEC_LOG_DBG("failed to get vector adapter info rc = %d", rc);
			rte_errno = ENOTSUP;
			goto error;
		}
	}

	if (conf->ev.sched_type != dev->data->queues_cfg[conf->ev.queue_id].schedule_type &&
	    !(dev->data->event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES)) {
		EVVEC_LOG_DBG("invalid event schedule type, eventdev doesn't support all types");
		rte_errno = EINVAL;
		goto error;
	}

	if (adapter->ops == NULL) {
		adapter->ops = &sw_ops;
		info = sw_info;
	}

	rc = validate_conf(conf, &info);
	if (rc < 0) {
		adapter->ops = NULL;
		rte_errno = EINVAL;
		goto error;
	}

	n = snprintf(mz_name, MZ_NAME_MAX_LEN, DATA_MZ_NAME_FORMAT, conf->event_dev_id,
		     conf->ev.queue_id, adapter->adapter_id);
	if (n >= (int)sizeof(mz_name)) {
		adapter->ops = NULL;
		EVVEC_LOG_DBG("failed to create memzone name");
		rte_errno = EINVAL;
		goto error;
	}
	mz = rte_memzone_reserve(mz_name, sizeof(struct rte_event_vector_adapter_data),
				 conf->socket_id, 0);
	if (mz == NULL) {
		adapter->ops = NULL;
		EVVEC_LOG_DBG("failed to reserve memzone for vector adapter");
		rte_errno = ENOMEM;
		goto error;
	}

	adapter->data = mz->addr;
	memset(adapter->data, 0, sizeof(struct rte_event_vector_adapter_data));

	adapter->data->mz = mz;
	adapter->data->event_dev_id = conf->event_dev_id;
	adapter->data->id = adapter->adapter_id;
	adapter->data->socket_id = conf->socket_id;
	adapter->data->conf = *conf;

	if (!(caps & RTE_EVENT_VECTOR_ADAPTER_CAP_INTERNAL_PORT)) {
		if (conf_cb == NULL) {
			EVVEC_LOG_DBG("port config callback is NULL");
			rte_errno = EINVAL;
			goto error;
		}

		rc = conf_cb(conf->event_dev_id, &adapter->data->event_port_id, conf_arg);
		if (rc < 0) {
			EVVEC_LOG_DBG("failed to create port for vector adapter");
			rte_errno = EINVAL;
			goto error;
		}
	}

	FUNC_PTR_OR_ERR_RET(adapter->ops->create, NULL);

	rc = adapter->ops->create(adapter);
	if (rc < 0) {
		adapter->ops = NULL;
		EVVEC_LOG_DBG("failed to create vector adapter");
		rte_errno = EINVAL;
		goto error;
	}

	adapter->enqueue = adapter->ops->enqueue;

	return adapter;

error:
	adapter->used = false;
	return NULL;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_lookup, 25.07)
struct rte_event_vector_adapter *
rte_event_vector_adapter_lookup(uint32_t adapter_id)
{
	uint8_t adapter_idx = ADAPTER_ID_FROM_ADAPTER_ID(adapter_id);
	uint8_t queue_id = QUEUE_ID_FROM_ADAPTER_ID(adapter_id);
	uint8_t dev_id = DEV_ID_FROM_ADAPTER_ID(adapter_id);
	struct rte_event_vector_adapter *adapter;
	const struct rte_memzone *mz;
	char name[MZ_NAME_MAX_LEN];
	struct rte_eventdev *dev;
	int rc;

	if (dev_id >= RTE_EVENT_MAX_DEVS || queue_id >= RTE_EVENT_MAX_QUEUES_PER_DEV ||
	    adapter_idx >= RTE_EVENT_VECTOR_ADAPTER_MAX_INSTANCE_PER_QUEUE) {
		EVVEC_LOG_ERR("invalid adapter id %u", adapter_id);
		rte_errno = EINVAL;
		return NULL;
	}

	if (adapters[dev_id][queue_id] == NULL) {
		adapters[dev_id][queue_id] =
			rte_zmalloc("rte_event_vector_adapter",
				    sizeof(struct rte_event_vector_adapter) *
					    RTE_EVENT_VECTOR_ADAPTER_MAX_INSTANCE_PER_QUEUE,
				    RTE_CACHE_LINE_SIZE);
		if (adapters[dev_id][queue_id] == NULL) {
			EVVEC_LOG_DBG("failed to allocate memory for vector adapters");
			rte_errno = ENOMEM;
			return NULL;
		}
	}

	if (adapters[dev_id][queue_id][adapter_idx].used == true)
		return &adapters[dev_id][queue_id][adapter_idx];

	adapter = &adapters[dev_id][queue_id][adapter_idx];

	snprintf(name, MZ_NAME_MAX_LEN, DATA_MZ_NAME_FORMAT, dev_id, queue_id, adapter_idx);
	mz = rte_memzone_lookup(name);
	if (mz == NULL) {
		EVVEC_LOG_DBG("failed to lookup memzone for vector adapter");
		rte_errno = ENOENT;
		return NULL;
	}

	adapter->data = mz->addr;
	dev = &rte_eventdevs[dev_id];

	if (dev->dev_ops->vector_adapter_caps_get != NULL) {
		rc = dev->dev_ops->vector_adapter_caps_get(dev, &adapter->data->caps,
							   &adapter->ops);
		if (rc < 0) {
			EVVEC_LOG_DBG("failed to get vector adapter capabilities");
			rte_errno = ENOTSUP;
			return NULL;
		}
	}
	if (adapter->ops == NULL)
		adapter->ops = &sw_ops;

	adapter->enqueue = adapter->ops->enqueue;
	adapter->adapter_id = adapter_id;
	adapter->used = true;

	return adapter;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_service_id_get, 25.07)
int
rte_event_vector_adapter_service_id_get(struct rte_event_vector_adapter *adapter,
					uint32_t *service_id)
{
	PTR_VALID_OR_ERR_RET(adapter, -EINVAL);

	if (adapter->data->service_inited && service_id != NULL)
		*service_id = adapter->data->unified_service_id;

	return adapter->data->service_inited ? 0 : -ESRCH;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_destroy, 25.07)
int
rte_event_vector_adapter_destroy(struct rte_event_vector_adapter *adapter)
{
	int rc;

	PTR_VALID_OR_ERR_RET(adapter, -EINVAL);
	if (adapter->used == false) {
		EVVEC_LOG_ERR("event vector adapter is not allocated");
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(adapter->ops->destroy, -ENOTSUP);

	rc = adapter->ops->destroy(adapter);
	if (rc < 0) {
		EVVEC_LOG_DBG("failed to destroy vector adapter");
		return rc;
	}

	rte_memzone_free(adapter->data->mz);
	adapter->ops = NULL;
	adapter->enqueue = dummy_vector_adapter_enqueue;
	adapter->data = NULL;
	adapter->used = false;

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_info_get, 25.07)
int
rte_event_vector_adapter_info_get(uint8_t event_dev_id, struct rte_event_vector_adapter_info *info)
{
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(event_dev_id, -EINVAL);
	PTR_VALID_OR_ERR_RET(info, -EINVAL);

	struct rte_eventdev *dev = &rte_eventdevs[event_dev_id];
	if (dev->dev_ops->vector_adapter_info_get != NULL)
		return dev->dev_ops->vector_adapter_info_get(dev, info);

	*info = sw_info;
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_conf_get, 25.07)
int
rte_event_vector_adapter_conf_get(struct rte_event_vector_adapter *adapter,
				  struct rte_event_vector_adapter_conf *conf)
{
	PTR_VALID_OR_ERR_RET(adapter, -EINVAL);
	PTR_VALID_OR_ERR_RET(conf, -EINVAL);

	*conf = adapter->data->conf;
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_remaining, 25.07)
uint8_t
rte_event_vector_adapter_remaining(uint8_t event_dev_id, uint8_t event_queue_id)
{
	uint8_t remaining = 0;
	int i;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(event_dev_id, 0);

	if (event_queue_id >= RTE_EVENT_MAX_QUEUES_PER_DEV)
		return 0;

	for (i = 0; i < RTE_EVENT_VECTOR_ADAPTER_MAX_INSTANCE_PER_QUEUE; i++) {
		if (adapters[event_dev_id][event_queue_id][i].used == false)
			remaining++;
	}

	return remaining;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_stats_get, 25.07)
int
rte_event_vector_adapter_stats_get(struct rte_event_vector_adapter *adapter,
				   struct rte_event_vector_adapter_stats *stats)
{
	PTR_VALID_OR_ERR_RET(adapter, -EINVAL);
	PTR_VALID_OR_ERR_RET(stats, -EINVAL);

	FUNC_PTR_OR_ERR_RET(adapter->ops->stats_get, -ENOTSUP);

	adapter->ops->stats_get(adapter, stats);

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_event_vector_adapter_stats_reset, 25.07)
int
rte_event_vector_adapter_stats_reset(struct rte_event_vector_adapter *adapter)
{
	PTR_VALID_OR_ERR_RET(adapter, -EINVAL);

	FUNC_PTR_OR_ERR_RET(adapter->ops->stats_reset, -ENOTSUP);

	adapter->ops->stats_reset(adapter);

	return 0;
}

/* Software vector adapter implementation. */

struct sw_vector_adapter_service_data;
struct sw_vector_adapter_data {
	uint8_t dev_id;
	uint8_t port_id;
	uint16_t vector_sz;
	uint64_t timestamp;
	uint64_t event_meta;
	uint64_t vector_tmo_ticks;
	uint64_t fallback_event_meta;
	struct rte_mempool *vector_mp;
	struct rte_event_vector *vector;
	rte_spinlock_t lock;
	struct rte_event_vector_adapter *adapter;
	struct rte_event_vector_adapter_stats stats;
	struct sw_vector_adapter_service_data *service_data;
	RTE_TAILQ_ENTRY(sw_vector_adapter_data) next;
};

struct sw_vector_adapter_service_data {
	uint8_t pend_list;
	uint32_t service_id;
	struct rte_ring *ring;
	struct sw_vector_adapter_data *pend[SERVICE_PEND_LIST];
};

static inline struct sw_vector_adapter_data *
sw_vector_adapter_priv(const struct rte_event_vector_adapter *adapter)
{
	return adapter->data->adapter_priv;
}

static int
sw_vector_adapter_flush(struct sw_vector_adapter_data *sw)
{
	struct rte_event ev;

	if (sw->vector == NULL)
		return -ENOBUFS;

	ev.event = sw->event_meta;
	ev.vec = sw->vector;
	if (rte_event_enqueue_burst(sw->dev_id, sw->port_id, &ev, 1) != 1)
		return -ENOSPC;

	sw->vector = NULL;
	sw->timestamp = 0;
	return 0;
}

static void
sw_vector_adapter_process_pend_list(struct sw_vector_adapter_service_data *service_data)
{
	struct sw_vector_adapter_data *sw;
	int i;

	if (service_data->pend_list == 0)
		return;
	for (i = 0; i < SERVICE_PEND_LIST; i++) {
		if (service_data->pend_list == 0)
			break;
		if (service_data->pend[i] == NULL)
			continue;

		sw = service_data->pend[i];
		if (sw->vector == NULL) {
			service_data->pend[i] = NULL;
			service_data->pend_list--;
			continue;
		}

		rte_spinlock_lock(&sw->lock);
		if (rte_get_tsc_cycles() - sw->timestamp >= sw->vector_tmo_ticks) {
			if (sw_vector_adapter_flush(sw) != -ENOSPC) {
				service_data->pend[i] = NULL;
				service_data->pend_list--;
			}
		}
		rte_spinlock_unlock(&sw->lock);
	}
}

static void
sw_vector_adapter_add_to_pend_list(struct sw_vector_adapter_service_data *service_data,
				   struct sw_vector_adapter_data *sw)
{
	int i, pos = SERVICE_PEND_LIST;

	if (service_data->pend_list >= SERVICE_PEND_LIST) {
		EVVEC_LOG_ERR("pend list is full");
		return;
	}
	for (i = 0; i < SERVICE_PEND_LIST; i++) {
		if (service_data->pend[i] == sw)
			return;
		if (service_data->pend[i] == NULL)
			pos = i;
	}
	if (pos == SERVICE_PEND_LIST)
		return;
	service_data->pend[pos] = sw;
	service_data->pend_list++;
}

static int
sw_vector_adapter_service_func(void *arg)
{
	struct sw_vector_adapter_service_data *service_data = arg;
	struct sw_vector_adapter_data *sw[SERVICE_DEQ_SZ];
	int n, i;

	sw_vector_adapter_process_pend_list(service_data);
	/* Dequeue the adapter list and flush the vectors */
	n = rte_ring_dequeue_burst(service_data->ring, (void **)&sw, SERVICE_DEQ_SZ, NULL);
	for (i = 0; i < n; i++) {

		if (sw[i]->vector == NULL)
			continue;

		if (rte_get_tsc_cycles() - sw[i]->timestamp < sw[i]->vector_tmo_ticks) {
			sw_vector_adapter_add_to_pend_list(service_data, sw[i]);
		} else {
			if (!rte_spinlock_trylock(&sw[i]->lock)) {
				sw_vector_adapter_add_to_pend_list(service_data, sw[i]);
				continue;
			}
			if (sw_vector_adapter_flush(sw[i]) == -ENOSPC)
				sw_vector_adapter_add_to_pend_list(service_data, sw[i]);
			else
				sw[i]->stats.vectors_timedout++;
			rte_spinlock_unlock(&sw[i]->lock);
		}
	}

	return 0;
}

static int
sw_vector_adapter_service_init(struct sw_vector_adapter_data *sw)
{
#define SW_VECTOR_ADAPTER_SERVICE_FMT "sw_vector_adapter_service"
	struct sw_vector_adapter_service_data *service_data;
	struct rte_service_spec service;
	const struct rte_memzone *mz;
	struct rte_ring *ring;
	int ret;

	mz = rte_memzone_lookup(SW_VECTOR_ADAPTER_SERVICE_FMT);
	if (mz == NULL) {
		mz = rte_memzone_reserve(SW_VECTOR_ADAPTER_SERVICE_FMT,
					 sizeof(struct sw_vector_adapter_service_data),
					 sw->adapter->data->socket_id, 0);
		if (mz == NULL) {
			EVVEC_LOG_DBG("failed to reserve memzone for service");
			return -ENOMEM;
		}
		service_data = (struct sw_vector_adapter_service_data *)mz->addr;
		memset(service_data, 0, sizeof(*service_data));

		ring = rte_ring_create(SW_VECTOR_ADAPTER_SERVICE_FMT, SERVICE_RING_SZ,
				       sw->adapter->data->socket_id, 0);
		if (ring == NULL) {
			EVVEC_LOG_ERR("failed to create ring for service");
			rte_memzone_free(mz);
			return -ENOMEM;
		}
		service_data->ring = ring;

		memset(&service, 0, sizeof(service));
		snprintf(service.name, RTE_SERVICE_NAME_MAX, "%s", SW_VECTOR_ADAPTER_SERVICE_FMT);
		service.callback = sw_vector_adapter_service_func;
		service.callback_userdata = service_data;
		service.socket_id = sw->adapter->data->socket_id;

		ret = rte_service_component_register(&service, &service_data->service_id);
		if (ret < 0) {
			EVVEC_LOG_ERR("failed to register service %s with id %" PRIu32 ": err = %d",
				      service.name, service_data->service_id, ret);
			return -ENOTSUP;
		}
		ret = rte_service_component_runstate_set(service_data->service_id, 1);
		if (ret < 0) {
			EVVEC_LOG_ERR("failed to set service runstate with id %" PRIu32
				      ": err = %d",
				      service_data->service_id, ret);
			return -ENOTSUP;
		}
	}
	service_data = (struct sw_vector_adapter_service_data *)mz->addr;

	sw->service_data = service_data;
	sw->adapter->data->unified_service_id = service_data->service_id;
	sw->adapter->data->service_inited = 1;
	return 0;
}

static int
sw_vector_adapter_create(struct rte_event_vector_adapter *adapter)
{
#define NSEC2TICK(__ns, __freq) (((__ns) * (__freq)) / 1E9)
#define SW_VECTOR_ADAPTER_NAME	64
	char name[SW_VECTOR_ADAPTER_NAME];
	struct sw_vector_adapter_data *sw;
	struct rte_event ev;

	snprintf(name, SW_VECTOR_ADAPTER_NAME, "sw_vector_%" PRIx32, adapter->data->id);
	sw = rte_zmalloc_socket(name, sizeof(*sw), RTE_CACHE_LINE_SIZE, adapter->data->socket_id);
	if (sw == NULL) {
		EVVEC_LOG_ERR("failed to allocate space for private data");
		rte_errno = ENOMEM;
		return -1;
	}

	/* Connect storage to adapter instance */
	adapter->data->adapter_priv = sw;
	sw->adapter = adapter;
	sw->dev_id = adapter->data->event_dev_id;
	sw->port_id = adapter->data->event_port_id;

	sw->vector_sz = adapter->data->conf.vector_sz;
	sw->vector_mp = adapter->data->conf.vector_mp;
	sw->vector_tmo_ticks = NSEC2TICK(adapter->data->conf.vector_timeout_ns, rte_get_timer_hz());

	ev = adapter->data->conf.ev;
	ev.op = RTE_EVENT_OP_NEW;
	sw->event_meta = ev.event;

	ev = adapter->data->conf.ev_fallback;
	ev.op = RTE_EVENT_OP_NEW;
	ev.priority = adapter->data->conf.ev.priority;
	ev.queue_id = adapter->data->conf.ev.queue_id;
	ev.sched_type = adapter->data->conf.ev.sched_type;
	sw->fallback_event_meta = ev.event;

	rte_spinlock_init(&sw->lock);
	sw_vector_adapter_service_init(sw);

	return 0;
}

static int
sw_vector_adapter_destroy(struct rte_event_vector_adapter *adapter)
{
	struct sw_vector_adapter_data *sw = sw_vector_adapter_priv(adapter);

	rte_free(sw);
	adapter->data->adapter_priv = NULL;

	return 0;
}

static int
sw_vector_adapter_flush_single_event(struct sw_vector_adapter_data *sw, uint64_t obj)
{
	struct rte_event ev;

	ev.event = sw->fallback_event_meta;
	ev.u64 = obj;
	if (rte_event_enqueue_burst(sw->dev_id, sw->port_id, &ev, 1) != 1)
		return -ENOSPC;

	return 0;
}

static int
sw_vector_adapter_enqueue(struct rte_event_vector_adapter *adapter, uint64_t objs[],
			  uint16_t num_elem, uint64_t flags)
{
	struct sw_vector_adapter_data *sw = sw_vector_adapter_priv(adapter);
	uint16_t cnt = num_elem, n;
	int ret;

	rte_spinlock_lock(&sw->lock);
	if (flags & RTE_EVENT_VECTOR_ENQ_FLUSH) {
		sw_vector_adapter_flush(sw);
		sw->stats.vectors_flushed++;
		rte_spinlock_unlock(&sw->lock);
		return 0;
	}

	if (num_elem == 0) {
		rte_spinlock_unlock(&sw->lock);
		return 0;
	}

	if (flags & RTE_EVENT_VECTOR_ENQ_SOV && sw->vector != NULL) {
		while (sw_vector_adapter_flush(sw) != 0)
			;
		sw->stats.vectors_flushed++;
	}

	while (num_elem) {
		if (sw->vector == NULL) {
			ret = rte_mempool_get(sw->vector_mp, (void **)&sw->vector);
			if (ret) {
				if (sw_vector_adapter_flush_single_event(sw, *objs) == 0) {
					sw->stats.alloc_failures++;
					num_elem--;
					objs++;
					continue;
				}
				rte_errno = -ENOSPC;
				goto done;
			}
			sw->vector->nb_elem = 0;
			sw->vector->attr_valid = 0;
			sw->vector->elem_offset = 0;
		}
		n = RTE_MIN(sw->vector_sz - sw->vector->nb_elem, num_elem);
		memcpy(&sw->vector->u64s[sw->vector->nb_elem], objs, n * sizeof(uint64_t));
		sw->vector->nb_elem += n;
		num_elem -= n;
		objs += n;

		if (sw->vector_sz == sw->vector->nb_elem) {
			ret = sw_vector_adapter_flush(sw);
			if (ret)
				goto done;
			sw->stats.vectorized++;
		}
	}

	if (flags & RTE_EVENT_VECTOR_ENQ_EOV && sw->vector != NULL) {
		while (sw_vector_adapter_flush(sw) != 0)
			;
		sw->stats.vectors_flushed++;
	}

	if (sw->vector != NULL && sw->vector->nb_elem) {
		sw->timestamp = rte_get_timer_cycles();
		rte_ring_enqueue(sw->service_data->ring, sw);
	}

done:
	rte_spinlock_unlock(&sw->lock);
	return cnt - num_elem;
}

static int
sw_vector_adapter_stats_get(const struct rte_event_vector_adapter *adapter,
			    struct rte_event_vector_adapter_stats *stats)
{
	struct sw_vector_adapter_data *sw = sw_vector_adapter_priv(adapter);

	*stats = sw->stats;
	return 0;
}

static int
sw_vector_adapter_stats_reset(const struct rte_event_vector_adapter *adapter)
{
	struct sw_vector_adapter_data *sw = sw_vector_adapter_priv(adapter);

	memset(&sw->stats, 0, sizeof(sw->stats));
	return 0;
}

static const struct event_vector_adapter_ops sw_ops = {
	.create = sw_vector_adapter_create,
	.destroy = sw_vector_adapter_destroy,
	.enqueue = sw_vector_adapter_enqueue,
	.stats_get = sw_vector_adapter_stats_get,
	.stats_reset = sw_vector_adapter_stats_reset,
};

static const struct rte_event_vector_adapter_info sw_info = {
	.min_vector_sz = MIN_VECTOR_SIZE,
	.max_vector_sz = MAX_VECTOR_SIZE,
	.min_vector_timeout_ns = MIN_VECTOR_NS,
	.max_vector_timeout_ns = MAX_VECTOR_NS,
	.log2_sz = 0,
};
