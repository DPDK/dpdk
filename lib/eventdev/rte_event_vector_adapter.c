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
