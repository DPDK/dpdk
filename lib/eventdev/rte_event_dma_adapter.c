/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <eventdev_pmd.h>

#include "rte_event_dma_adapter.h"

#define DMA_BATCH_SIZE 32
#define DMA_DEFAULT_MAX_NB 128
#define DMA_ADAPTER_NAME_LEN 32
#define DMA_ADAPTER_BUFFER_SIZE 1024

#define DMA_ADAPTER_OPS_BUFFER_SIZE (DMA_BATCH_SIZE + DMA_BATCH_SIZE)

#define DMA_ADAPTER_ARRAY "event_dma_adapter_array"

/* Macros to check for valid adapter */
#define EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, retval) \
	do { \
		if (!edma_adapter_valid_id(id)) { \
			RTE_EDEV_LOG_ERR("Invalid DMA adapter id = %d\n", id); \
			return retval; \
		} \
	} while (0)

/* DMA ops circular buffer */
struct dma_ops_circular_buffer {
	/* Index of head element */
	uint16_t head;

	/* Index of tail element */
	uint16_t tail;

	/* Number of elements in buffer */
	uint16_t count;

	/* Size of circular buffer */
	uint16_t size;

	/* Pointer to hold rte_event_dma_adapter_op for processing */
	struct rte_event_dma_adapter_op **op_buffer;
} __rte_cache_aligned;

/* Vchan information */
struct dma_vchan_info {
	/* Set to indicate vchan queue is enabled */
	bool vq_enabled;

	/* Circular buffer for batching DMA ops to dma_dev */
	struct dma_ops_circular_buffer dma_buf;
} __rte_cache_aligned;

/* DMA device information */
struct dma_device_info {
	/* Pointer to vchan queue info */
	struct dma_vchan_info *vchanq;

	/* Pointer to vchan queue info.
	 * This holds ops passed by application till the
	 * dma completion is done.
	 */
	struct dma_vchan_info *tqmap;

	/* If num_vchanq > 0, the start callback will
	 * be invoked if not already invoked
	 */
	uint16_t num_vchanq;

	/* Number of vchans configured for a DMA device. */
	uint16_t num_dma_dev_vchan;
} __rte_cache_aligned;

struct event_dma_adapter {
	/* Event device identifier */
	uint8_t eventdev_id;

	/* Event port identifier */
	uint8_t event_port_id;

	/* Adapter mode */
	enum rte_event_dma_adapter_mode mode;

	/* Memory allocation name */
	char mem_name[DMA_ADAPTER_NAME_LEN];

	/* Socket identifier cached from eventdev */
	int socket_id;

	/* Lock to serialize config updates with service function */
	rte_spinlock_t lock;

	/* DMA device structure array */
	struct dma_device_info *dma_devs;

	/* Circular buffer for processing DMA ops to eventdev */
	struct dma_ops_circular_buffer ebuf;

	/* Configuration callback for rte_service configuration */
	rte_event_dma_adapter_conf_cb conf_cb;

	/* Configuration callback argument */
	void *conf_arg;

	/* Set if  default_cb is being used */
	int default_cb_arg;

	/* No. of vchan queue configured */
	uint16_t nb_vchanq;
} __rte_cache_aligned;

static struct event_dma_adapter **event_dma_adapter;

static inline int
edma_adapter_valid_id(uint8_t id)
{
	return id < RTE_EVENT_DMA_ADAPTER_MAX_INSTANCE;
}

static inline struct event_dma_adapter *
edma_id_to_adapter(uint8_t id)
{
	return event_dma_adapter ? event_dma_adapter[id] : NULL;
}

static int
edma_array_init(void)
{
	const struct rte_memzone *mz;
	uint32_t sz;

	mz = rte_memzone_lookup(DMA_ADAPTER_ARRAY);
	if (mz == NULL) {
		sz = sizeof(struct event_dma_adapter *) * RTE_EVENT_DMA_ADAPTER_MAX_INSTANCE;
		sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

		mz = rte_memzone_reserve_aligned(DMA_ADAPTER_ARRAY, sz, rte_socket_id(), 0,
						 RTE_CACHE_LINE_SIZE);
		if (mz == NULL) {
			RTE_EDEV_LOG_ERR("Failed to reserve memzone : %s, err = %d",
					 DMA_ADAPTER_ARRAY, rte_errno);
			return -rte_errno;
		}
	}

	event_dma_adapter = mz->addr;

	return 0;
}

static inline int
edma_circular_buffer_init(const char *name, struct dma_ops_circular_buffer *buf, uint16_t sz)
{
	buf->op_buffer = rte_zmalloc(name, sizeof(struct rte_event_dma_adapter_op *) * sz, 0);
	if (buf->op_buffer == NULL)
		return -ENOMEM;

	buf->size = sz;

	return 0;
}

static inline void
edma_circular_buffer_free(struct dma_ops_circular_buffer *buf)
{
	rte_free(buf->op_buffer);
}

static int
edma_default_config_cb(uint8_t id, uint8_t evdev_id, struct rte_event_dma_adapter_conf *conf,
		       void *arg)
{
	struct rte_event_port_conf *port_conf;
	struct rte_event_dev_config dev_conf;
	struct event_dma_adapter *adapter;
	struct rte_eventdev *dev;
	uint8_t port_id;
	int started;
	int ret;

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	dev_conf = dev->data->dev_conf;

	started = dev->data->dev_started;
	if (started)
		rte_event_dev_stop(evdev_id);

	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports += 1;

	port_conf = arg;
	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_SINGLE_LINK)
		dev_conf.nb_single_link_event_port_queues += 1;

	ret = rte_event_dev_configure(evdev_id, &dev_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to configure event dev %u\n", evdev_id);
		if (started) {
			if (rte_event_dev_start(evdev_id))
				return -EIO;
		}
		return ret;
	}

	ret = rte_event_port_setup(evdev_id, port_id, port_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to setup event port %u\n", port_id);
		return ret;
	}

	conf->event_port_id = port_id;
	conf->max_nb = DMA_DEFAULT_MAX_NB;
	if (started)
		ret = rte_event_dev_start(evdev_id);

	adapter->default_cb_arg = 1;
	adapter->event_port_id = conf->event_port_id;

	return ret;
}

int
rte_event_dma_adapter_create_ext(uint8_t id, uint8_t evdev_id,
				 rte_event_dma_adapter_conf_cb conf_cb,
				 enum rte_event_dma_adapter_mode mode, void *conf_arg)
{
	struct rte_event_dev_info dev_info;
	struct event_dma_adapter *adapter;
	char name[DMA_ADAPTER_NAME_LEN];
	struct rte_dma_info info;
	uint16_t num_dma_dev;
	int socket_id;
	uint8_t i;
	int ret;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(evdev_id, -EINVAL);

	if (conf_cb == NULL)
		return -EINVAL;

	if (event_dma_adapter == NULL) {
		ret = edma_array_init();
		if (ret)
			return ret;
	}

	adapter = edma_id_to_adapter(id);
	if (adapter != NULL) {
		RTE_EDEV_LOG_ERR("ML adapter ID %d already exists!", id);
		return -EEXIST;
	}

	socket_id = rte_event_dev_socket_id(evdev_id);
	snprintf(name, DMA_ADAPTER_NAME_LEN, "rte_event_dma_adapter_%d", id);
	adapter = rte_zmalloc_socket(name, sizeof(struct event_dma_adapter), RTE_CACHE_LINE_SIZE,
				     socket_id);
	if (adapter == NULL) {
		RTE_EDEV_LOG_ERR("Failed to get mem for event ML adapter!");
		return -ENOMEM;
	}

	if (edma_circular_buffer_init("edma_circular_buffer", &adapter->ebuf,
				      DMA_ADAPTER_BUFFER_SIZE)) {
		RTE_EDEV_LOG_ERR("Failed to get memory for event adapter circular buffer");
		rte_free(adapter);
		return -ENOMEM;
	}

	ret = rte_event_dev_info_get(evdev_id, &dev_info);
	if (ret < 0) {
		RTE_EDEV_LOG_ERR("Failed to get info for eventdev %d: %s", evdev_id,
				 dev_info.driver_name);
		edma_circular_buffer_free(&adapter->ebuf);
		rte_free(adapter);
		return ret;
	}

	num_dma_dev = rte_dma_count_avail();

	adapter->eventdev_id = evdev_id;
	adapter->mode = mode;
	rte_strscpy(adapter->mem_name, name, DMA_ADAPTER_NAME_LEN);
	adapter->socket_id = socket_id;
	adapter->conf_cb = conf_cb;
	adapter->conf_arg = conf_arg;
	adapter->dma_devs = rte_zmalloc_socket(adapter->mem_name,
					       num_dma_dev * sizeof(struct dma_device_info), 0,
					       socket_id);
	if (adapter->dma_devs == NULL) {
		RTE_EDEV_LOG_ERR("Failed to get memory for DMA devices\n");
		edma_circular_buffer_free(&adapter->ebuf);
		rte_free(adapter);
		return -ENOMEM;
	}

	rte_spinlock_init(&adapter->lock);
	for (i = 0; i < num_dma_dev; i++) {
		ret = rte_dma_info_get(i, &info);
		if (ret) {
			RTE_EDEV_LOG_ERR("Failed to get dma device info\n");
			edma_circular_buffer_free(&adapter->ebuf);
			rte_free(adapter);
			return ret;
		}

		adapter->dma_devs[i].num_dma_dev_vchan = info.nb_vchans;
	}

	event_dma_adapter[id] = adapter;

	return 0;
}

int
rte_event_dma_adapter_create(uint8_t id, uint8_t evdev_id, struct rte_event_port_conf *port_config,
			    enum rte_event_dma_adapter_mode mode)
{
	struct rte_event_port_conf *pc;
	int ret;

	if (port_config == NULL)
		return -EINVAL;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	pc = rte_malloc(NULL, sizeof(struct rte_event_port_conf), 0);
	if (pc == NULL)
		return -ENOMEM;

	rte_memcpy(pc, port_config, sizeof(struct rte_event_port_conf));
	ret = rte_event_dma_adapter_create_ext(id, evdev_id, edma_default_config_cb, mode, pc);
	if (ret != 0)
		rte_free(pc);

	return ret;
}

int
rte_event_dma_adapter_free(uint8_t id)
{
	struct event_dma_adapter *adapter;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	rte_free(adapter->conf_arg);
	rte_free(adapter->dma_devs);
	edma_circular_buffer_free(&adapter->ebuf);
	rte_free(adapter);
	event_dma_adapter[id] = NULL;

	return 0;
}

static void
edma_update_vchanq_info(struct event_dma_adapter *adapter, struct dma_device_info *dev_info,
			uint16_t vchan, uint8_t add)
{
	struct dma_vchan_info *vchan_info;
	struct dma_vchan_info *tqmap_info;
	int enabled;
	uint16_t i;

	if (dev_info->vchanq == NULL)
		return;

	if (vchan == RTE_DMA_ALL_VCHAN) {
		for (i = 0; i < dev_info->num_dma_dev_vchan; i++)
			edma_update_vchanq_info(adapter, dev_info, i, add);
	} else {
		tqmap_info = &dev_info->tqmap[vchan];
		vchan_info = &dev_info->vchanq[vchan];
		enabled = vchan_info->vq_enabled;
		if (add) {
			adapter->nb_vchanq += !enabled;
			dev_info->num_vchanq += !enabled;
		} else {
			adapter->nb_vchanq -= enabled;
			dev_info->num_vchanq -= enabled;
		}
		vchan_info->vq_enabled = !!add;
		tqmap_info->vq_enabled = !!add;
	}
}

int
rte_event_dma_adapter_vchan_add(uint8_t id, int16_t dma_dev_id, uint16_t vchan,
				const struct rte_event *event)
{
	struct event_dma_adapter *adapter;
	struct dma_device_info *dev_info;
	struct rte_eventdev *dev;
	uint32_t cap;
	int ret;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (!rte_dma_is_valid(dma_dev_id)) {
		RTE_EDEV_LOG_ERR("Invalid dma_dev_id = %" PRIu8, dma_dev_id);
		return -EINVAL;
	}

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	ret = rte_event_dma_adapter_caps_get(adapter->eventdev_id, dma_dev_id, &cap);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps dev %u dma_dev %u", id, dma_dev_id);
		return ret;
	}

	if ((cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND) && (event == NULL)) {
		RTE_EDEV_LOG_ERR("Event can not be NULL for dma_dev_id = %u", dma_dev_id);
		return -EINVAL;
	}

	dev_info = &adapter->dma_devs[dma_dev_id];
	if (vchan != RTE_DMA_ALL_VCHAN && vchan >= dev_info->num_dma_dev_vchan) {
		RTE_EDEV_LOG_ERR("Invalid vhcan %u", vchan);
		return -EINVAL;
	}

	/* In case HW cap is RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD, no
	 * need of service core as HW supports event forward capability.
	 */
	if ((cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) ||
	    (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND &&
	     adapter->mode == RTE_EVENT_DMA_ADAPTER_OP_NEW) ||
	    (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW &&
	     adapter->mode == RTE_EVENT_DMA_ADAPTER_OP_NEW)) {
		if (*dev->dev_ops->dma_adapter_vchan_add == NULL)
			return -ENOTSUP;
		if (dev_info->vchanq == NULL) {
			dev_info->vchanq = rte_zmalloc_socket(adapter->mem_name,
							dev_info->num_dma_dev_vchan *
							sizeof(struct dma_vchan_info),
							0, adapter->socket_id);
			if (dev_info->vchanq == NULL) {
				printf("Queue pair add not supported\n");
				return -ENOMEM;
			}
		}

		if (dev_info->tqmap == NULL) {
			dev_info->tqmap = rte_zmalloc_socket(adapter->mem_name,
						dev_info->num_dma_dev_vchan *
						sizeof(struct dma_vchan_info),
						0, adapter->socket_id);
			if (dev_info->tqmap == NULL) {
				printf("tq pair add not supported\n");
				return -ENOMEM;
			}
		}

		ret = (*dev->dev_ops->dma_adapter_vchan_add)(dev, dma_dev_id, vchan, event);
		if (ret)
			return ret;

		else
			edma_update_vchanq_info(adapter, &adapter->dma_devs[dma_dev_id], vchan, 1);
	}

	return 0;
}

int
rte_event_dma_adapter_vchan_del(uint8_t id, int16_t dma_dev_id, uint16_t vchan)
{
	struct event_dma_adapter *adapter;
	struct dma_device_info *dev_info;
	struct rte_eventdev *dev;
	uint32_t cap;
	int ret;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (!rte_dma_is_valid(dma_dev_id)) {
		RTE_EDEV_LOG_ERR("Invalid dma_dev_id = %" PRIu8, dma_dev_id);
		return -EINVAL;
	}

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	ret = rte_event_dma_adapter_caps_get(adapter->eventdev_id, dma_dev_id, &cap);
	if (ret)
		return ret;

	dev_info = &adapter->dma_devs[dma_dev_id];

	if (vchan != RTE_DMA_ALL_VCHAN && vchan >= dev_info->num_dma_dev_vchan) {
		RTE_EDEV_LOG_ERR("Invalid vhcan %" PRIu16, vchan);
		return -EINVAL;
	}

	if ((cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) ||
	    (cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW &&
	     adapter->mode == RTE_EVENT_DMA_ADAPTER_OP_NEW)) {
		if (*dev->dev_ops->dma_adapter_vchan_del == NULL)
			return -ENOTSUP;
		ret = (*dev->dev_ops->dma_adapter_vchan_del)(dev, dma_dev_id, vchan);
		if (ret == 0) {
			edma_update_vchanq_info(adapter, dev_info, vchan, 0);
			if (dev_info->num_vchanq == 0) {
				rte_free(dev_info->vchanq);
				dev_info->vchanq = NULL;
			}
		}
	} else {
		if (adapter->nb_vchanq == 0)
			return 0;

		rte_spinlock_lock(&adapter->lock);
		edma_update_vchanq_info(adapter, dev_info, vchan, 0);

		if (dev_info->num_vchanq == 0) {
			rte_free(dev_info->vchanq);
			rte_free(dev_info->tqmap);
			dev_info->vchanq = NULL;
			dev_info->tqmap = NULL;
		}

		rte_spinlock_unlock(&adapter->lock);
	}

	return ret;
}
