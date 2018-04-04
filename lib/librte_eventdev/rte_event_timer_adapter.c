/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation.
 * All rights reserved.
 */

#include <string.h>
#include <inttypes.h>

#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_dev.h>
#include <rte_errno.h>

#include "rte_eventdev.h"
#include "rte_eventdev_pmd.h"
#include "rte_event_timer_adapter.h"
#include "rte_event_timer_adapter_pmd.h"

#define DATA_MZ_NAME_MAX_LEN 64
#define DATA_MZ_NAME_FORMAT "rte_event_timer_adapter_data_%d"

static int evtim_logtype;

static struct rte_event_timer_adapter adapters[RTE_EVENT_TIMER_ADAPTER_NUM_MAX];

#define EVTIM_LOG(level, logtype, ...) \
	rte_log(RTE_LOG_ ## level, logtype, \
		RTE_FMT("EVTIMER: %s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) \
			"\n", __func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#define EVTIM_LOG_ERR(...) EVTIM_LOG(ERR, evtim_logtype, __VA_ARGS__)

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
#define EVTIM_LOG_DBG(...) \
	EVTIM_LOG(DEBUG, evtim_logtype, __VA_ARGS__)
#else
#define EVTIM_LOG_DBG(...) (void)0
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

RTE_INIT(event_timer_adapter_init_log);
static void
event_timer_adapter_init_log(void)
{
	evtim_logtype = rte_log_register("lib.eventdev.adapter.timer");
	if (evtim_logtype >= 0)
		rte_log_set_level(evtim_logtype, RTE_LOG_NOTICE);
}
