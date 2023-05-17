/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */
#include <rte_spinlock.h>
#include <rte_service_component.h>
#include <ethdev_driver.h>

#include "eventdev_pmd.h"
#include "eventdev_trace.h"
#include "rte_event_eth_tx_adapter.h"

#define TXA_BATCH_SIZE		32
#define TXA_SERVICE_NAME_LEN	32
#define TXA_MEM_NAME_LEN	32
#define TXA_FLUSH_THRESHOLD	1024
#define TXA_RETRY_CNT		100
#define TXA_MAX_NB_TX		128
#define TXA_INVALID_DEV_ID	INT32_C(-1)
#define TXA_INVALID_SERVICE_ID	INT64_C(-1)

#define TXA_ADAPTER_ARRAY "txa_adapter_array"
#define TXA_SERVICE_DATA_ARRAY "txa_service_data_array"

#define txa_evdev(id) (&rte_eventdevs[txa_dev_id_array[(id)]])

#define txa_dev_caps_get(id) txa_evdev((id))->dev_ops->eth_tx_adapter_caps_get

#define txa_dev_adapter_create(t) txa_evdev(t)->dev_ops->eth_tx_adapter_create

#define txa_dev_adapter_create_ext(t) \
				txa_evdev(t)->dev_ops->eth_tx_adapter_create

#define txa_dev_adapter_free(t) txa_evdev(t)->dev_ops->eth_tx_adapter_free

#define txa_dev_queue_add(id) txa_evdev(id)->dev_ops->eth_tx_adapter_queue_add

#define txa_dev_queue_del(t) txa_evdev(t)->dev_ops->eth_tx_adapter_queue_del

#define txa_dev_start(t) txa_evdev(t)->dev_ops->eth_tx_adapter_start

#define txa_dev_stop(t) txa_evdev(t)->dev_ops->eth_tx_adapter_stop

#define txa_dev_stats_reset(t) txa_evdev(t)->dev_ops->eth_tx_adapter_stats_reset

#define txa_dev_stats_get(t) txa_evdev(t)->dev_ops->eth_tx_adapter_stats_get

#define txa_dev_instance_get(id) \
			txa_evdev(id)->dev_ops->eth_tx_adapter_instance_get

#define txa_dev_queue_start(id) \
			txa_evdev(id)->dev_ops->eth_tx_adapter_queue_start

#define txa_dev_queue_stop(id) \
			txa_evdev(id)->dev_ops->eth_tx_adapter_queue_stop

#define RTE_EVENT_ETH_TX_ADAPTER_ID_VALID_OR_ERR_RET(id, retval) \
do { \
	if (!txa_valid_id(id)) { \
		RTE_EDEV_LOG_ERR("Invalid eth Tx adapter id = %d", id); \
		return retval; \
	} \
} while (0)

#define TXA_CHECK_OR_ERR_RET(id) \
do {\
	int ret; \
	RTE_EVENT_ETH_TX_ADAPTER_ID_VALID_OR_ERR_RET((id), -EINVAL); \
	ret = txa_init(); \
	if (ret != 0) \
		return ret; \
	if (!txa_adapter_exist((id))) \
		return -EINVAL; \
} while (0)

#define TXA_CHECK_TXQ(dev, queue) \
do {\
	if ((dev)->data->nb_tx_queues == 0) { \
		RTE_EDEV_LOG_ERR("No tx queues configured"); \
		return -EINVAL; \
	} \
	if ((queue) != -1 && \
		(uint16_t)(queue) >= (dev)->data->nb_tx_queues) { \
		RTE_EDEV_LOG_ERR("Invalid tx queue_id %" PRIu16, \
				(uint16_t)(queue)); \
		return -EINVAL; \
	} \
} while (0)

/* Tx retry callback structure */
struct txa_retry {
	/* Ethernet port id */
	uint16_t port_id;
	/* Tx queue */
	uint16_t tx_queue;
	/* Adapter ID */
	uint8_t id;
};

/* Per queue structure */
struct txa_service_queue_info {
	/* Queue has been added */
	uint8_t added;
	/* Queue is stopped */
	bool stopped;
	/* Retry callback argument */
	struct txa_retry txa_retry;
	/* Tx buffer */
	struct rte_eth_dev_tx_buffer *tx_buf;
};

/* PMD private structure */
struct txa_service_data {
	/* Max mbufs processed in any service function invocation */
	uint32_t max_nb_tx;
	/* Number of Tx queues in adapter */
	uint32_t nb_queues;
	/*  Synchronization with data path */
	rte_spinlock_t tx_lock;
	/* Event port ID */
	uint8_t port_id;
	/* Event device identifier */
	uint8_t eventdev_id;
	/* Highest port id supported + 1 */
	uint16_t dev_count;
	/* Loop count to flush Tx buffers */
	int loop_cnt;
	/* Loop count threshold to flush Tx buffers */
	uint16_t flush_threshold;
	/* Per ethernet device structure */
	struct txa_service_ethdev *txa_ethdev;
	/* Statistics */
	struct rte_event_eth_tx_adapter_stats stats;
	/* Adapter Identifier */
	uint8_t id;
	/* Conf arg must be freed */
	uint8_t conf_free;
	/* Configuration callback */
	rte_event_eth_tx_adapter_conf_cb conf_cb;
	/* Configuration callback argument */
	void *conf_arg;
	/* socket id */
	int socket_id;
	/* Per adapter EAL service */
	int64_t service_id;
	/* Memory allocation name */
	char mem_name[TXA_MEM_NAME_LEN];
} __rte_cache_aligned;

/* Per eth device structure */
struct txa_service_ethdev {
	/* Pointer to ethernet device */
	struct rte_eth_dev *dev;
	/* Number of queues added */
	uint16_t nb_queues;
	/* PMD specific queue data */
	void *queues;
};

/* Array of adapter instances, initialized with event device id
 * when adapter is created
 */
static int *txa_dev_id_array;

/* Array of pointers to service implementation data */
static struct txa_service_data **txa_service_data_array;

static int32_t txa_service_func(void *args);
static int txa_service_adapter_create_ext(uint8_t id,
			struct rte_eventdev *dev,
			rte_event_eth_tx_adapter_conf_cb conf_cb,
			void *conf_arg);
static int txa_service_queue_del(uint8_t id,
				const struct rte_eth_dev *dev,
				int32_t tx_queue_id);

static int
txa_adapter_exist(uint8_t id)
{
	return txa_dev_id_array[id] != TXA_INVALID_DEV_ID;
}

static inline int
txa_valid_id(uint8_t id)
{
	return id < RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE;
}

static void *
txa_memzone_array_get(const char *name, unsigned int elt_size, int nb_elems)
{
	const struct rte_memzone *mz;
	unsigned int sz;

	sz = elt_size * nb_elems;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	mz = rte_memzone_lookup(name);
	if (mz == NULL) {
		mz = rte_memzone_reserve_aligned(name, sz, rte_socket_id(), 0,
						 RTE_CACHE_LINE_SIZE);
		if (mz == NULL) {
			RTE_EDEV_LOG_ERR("failed to reserve memzone"
					" name = %s err = %"
					PRId32, name, rte_errno);
			return NULL;
		}
	}

	return  mz->addr;
}

static int
txa_lookup(void)
{
	const struct rte_memzone *mz;

	if (txa_dev_id_array == NULL) {
		mz = rte_memzone_lookup(TXA_ADAPTER_ARRAY);
		if (mz == NULL)
			return -ENOMEM;
		txa_dev_id_array = mz->addr;
	}

	if (txa_service_data_array == NULL) {
		mz = rte_memzone_lookup(TXA_SERVICE_DATA_ARRAY);
		if (mz == NULL)
			return -ENOMEM;
		txa_service_data_array = mz->addr;
	}

	return 0;
}

static int
txa_dev_id_array_init(void)
{
	if (txa_dev_id_array == NULL) {
		int i;

		txa_dev_id_array = txa_memzone_array_get(TXA_ADAPTER_ARRAY,
					sizeof(int),
					RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE);
		if (txa_dev_id_array == NULL)
			return -ENOMEM;

		for (i = 0; i < RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE; i++)
			txa_dev_id_array[i] = TXA_INVALID_DEV_ID;
	}

	return 0;
}

static int
txa_init(void)
{
	return txa_dev_id_array_init();
}

static int
txa_service_data_init(void)
{
	if (txa_service_data_array == NULL) {
		int i;

		txa_service_data_array =
				txa_memzone_array_get(TXA_SERVICE_DATA_ARRAY,
					sizeof(*txa_service_data_array),
					RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE);
		if (txa_service_data_array == NULL)
			return -ENOMEM;

		/* Reset the txa service pointers */
		for (i = 0; i < RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE; i++)
			txa_service_data_array[i] = NULL;
	}

	return 0;
}

static inline struct txa_service_data *
txa_service_id_to_data(uint8_t id)
{
	return txa_service_data_array[id];
}

static inline struct txa_service_queue_info *
txa_service_queue(struct txa_service_data *txa, uint16_t port_id,
		uint16_t tx_queue_id)
{
	struct txa_service_queue_info *tqi;

	if (unlikely(txa->txa_ethdev == NULL || txa->dev_count < port_id + 1))
		return NULL;

	tqi = txa->txa_ethdev[port_id].queues;

	return likely(tqi != NULL) ? tqi + tx_queue_id : NULL;
}

static int
txa_service_conf_cb(uint8_t __rte_unused id, uint8_t dev_id,
		struct rte_event_eth_tx_adapter_conf *conf, void *arg)
{
	int ret;
	struct rte_eventdev *dev;
	struct rte_event_port_conf *pc;
	struct rte_event_dev_config dev_conf;
	int started;
	uint8_t port_id;

	pc = arg;
	dev = &rte_eventdevs[dev_id];
	dev_conf = dev->data->dev_conf;

	started = dev->data->dev_started;
	if (started)
		rte_event_dev_stop(dev_id);

	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports += 1;
	if (pc->event_port_cfg & RTE_EVENT_PORT_CFG_SINGLE_LINK)
		dev_conf.nb_single_link_event_port_queues += 1;

	ret = rte_event_dev_configure(dev_id, &dev_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to configure event dev %u",
						dev_id);
		if (started) {
			if (rte_event_dev_start(dev_id))
				return -EIO;
		}
		return ret;
	}

	ret = rte_event_port_setup(dev_id, port_id, pc);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to setup event port %u\n",
					port_id);
		if (started) {
			if (rte_event_dev_start(dev_id))
				return -EIO;
		}
		return ret;
	}

	conf->event_port_id = port_id;
	conf->max_nb_tx = TXA_MAX_NB_TX;
	if (started)
		ret = rte_event_dev_start(dev_id);
	return ret;
}

static int
txa_service_ethdev_alloc(struct txa_service_data *txa)
{
	struct txa_service_ethdev *txa_ethdev;
	uint16_t i, dev_count;

	dev_count = rte_eth_dev_count_avail();
	if (txa->txa_ethdev && dev_count == txa->dev_count)
		return 0;

	txa_ethdev = rte_zmalloc_socket(txa->mem_name,
					dev_count * sizeof(*txa_ethdev),
					0,
					txa->socket_id);
	if (txa_ethdev == NULL) {
		RTE_EDEV_LOG_ERR("Failed to alloc txa::txa_ethdev ");
		return -ENOMEM;
	}

	if (txa->dev_count)
		memcpy(txa_ethdev, txa->txa_ethdev,
			txa->dev_count * sizeof(*txa_ethdev));

	RTE_ETH_FOREACH_DEV(i) {
		if (i == dev_count)
			break;
		txa_ethdev[i].dev = &rte_eth_devices[i];
	}

	txa->txa_ethdev = txa_ethdev;
	txa->dev_count = dev_count;
	return 0;
}

static int
txa_service_queue_array_alloc(struct txa_service_data *txa,
			uint16_t port_id)
{
	struct txa_service_queue_info *tqi;
	uint16_t nb_queue;
	int ret;

	ret = txa_service_ethdev_alloc(txa);
	if (ret != 0)
		return ret;

	if (txa->txa_ethdev[port_id].queues)
		return 0;

	nb_queue = txa->txa_ethdev[port_id].dev->data->nb_tx_queues;
	tqi = rte_zmalloc_socket(txa->mem_name,
				nb_queue *
				sizeof(struct txa_service_queue_info), 0,
				txa->socket_id);
	if (tqi == NULL)
		return -ENOMEM;
	txa->txa_ethdev[port_id].queues = tqi;
	return 0;
}

static void
txa_service_queue_array_free(struct txa_service_data *txa,
			uint16_t port_id)
{
	struct txa_service_ethdev *txa_ethdev;
	struct txa_service_queue_info *tqi;

	txa_ethdev = &txa->txa_ethdev[port_id];
	if (txa->txa_ethdev == NULL || txa_ethdev->nb_queues != 0)
		return;

	tqi = txa_ethdev->queues;
	txa_ethdev->queues = NULL;
	rte_free(tqi);

	if (txa->nb_queues == 0) {
		rte_free(txa->txa_ethdev);
		txa->txa_ethdev = NULL;
	}
}

static void
txa_service_unregister(struct txa_service_data *txa)
{
	if (txa->service_id != TXA_INVALID_SERVICE_ID) {
		rte_service_component_runstate_set(txa->service_id, 0);
		while (rte_service_may_be_active(txa->service_id))
			rte_pause();
		rte_service_component_unregister(txa->service_id);
	}
	txa->service_id = TXA_INVALID_SERVICE_ID;
}

static int
txa_service_register(struct txa_service_data *txa)
{
	int ret;
	struct rte_service_spec service;
	struct rte_event_eth_tx_adapter_conf conf;

	if (txa->service_id != TXA_INVALID_SERVICE_ID)
		return 0;

	memset(&service, 0, sizeof(service));
	snprintf(service.name, TXA_SERVICE_NAME_LEN, "txa_%d", txa->id);
	service.socket_id = txa->socket_id;
	service.callback = txa_service_func;
	service.callback_userdata = txa;
	service.capabilities = RTE_SERVICE_CAP_MT_SAFE;
	ret = rte_service_component_register(&service,
					(uint32_t *)&txa->service_id);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to register service %s err = %"
				 PRId32, service.name, ret);
		return ret;
	}

	ret = txa->conf_cb(txa->id, txa->eventdev_id, &conf, txa->conf_arg);
	if (ret) {
		txa_service_unregister(txa);
		return ret;
	}

	rte_service_component_runstate_set(txa->service_id, 1);
	txa->port_id = conf.event_port_id;
	txa->max_nb_tx = conf.max_nb_tx;
	return 0;
}

static struct rte_eth_dev_tx_buffer *
txa_service_tx_buf_alloc(struct txa_service_data *txa,
			const struct rte_eth_dev *dev)
{
	struct rte_eth_dev_tx_buffer *tb;
	uint16_t port_id;

	port_id = dev->data->port_id;
	tb = rte_zmalloc_socket(txa->mem_name,
				RTE_ETH_TX_BUFFER_SIZE(TXA_BATCH_SIZE),
				0,
				rte_eth_dev_socket_id(port_id));
	if (tb == NULL)
		RTE_EDEV_LOG_ERR("Failed to allocate memory for tx buffer");
	return tb;
}

static int
txa_service_is_queue_added(struct txa_service_data *txa,
			const struct rte_eth_dev *dev,
			uint16_t tx_queue_id)
{
	struct txa_service_queue_info *tqi;

	tqi = txa_service_queue(txa, dev->data->port_id, tx_queue_id);
	return tqi && tqi->added;
}

static int
txa_service_ctrl(uint8_t id, int start)
{
	int ret;
	struct txa_service_data *txa;

	txa = txa_service_id_to_data(id);
	if (txa == NULL || txa->service_id == TXA_INVALID_SERVICE_ID)
		return 0;

	rte_spinlock_lock(&txa->tx_lock);
	ret = rte_service_runstate_set(txa->service_id, start);
	rte_spinlock_unlock(&txa->tx_lock);

	return ret;
}

static void
txa_service_buffer_retry(struct rte_mbuf **pkts, uint16_t unsent,
			void *userdata)
{
	struct txa_retry *tr;
	struct txa_service_data *data;
	struct rte_event_eth_tx_adapter_stats *stats;
	uint16_t sent = 0;
	unsigned int retry = 0;
	uint16_t i, n;

	tr = (struct txa_retry *)(uintptr_t)userdata;
	data = txa_service_id_to_data(tr->id);
	stats = &data->stats;

	do {
		n = rte_eth_tx_burst(tr->port_id, tr->tx_queue,
			       &pkts[sent], unsent - sent);

		sent += n;
	} while (sent != unsent && retry++ < TXA_RETRY_CNT);

	for (i = sent; i < unsent; i++)
		rte_pktmbuf_free(pkts[i]);

	stats->tx_retry += retry;
	stats->tx_packets += sent;
	stats->tx_dropped += unsent - sent;
}

static uint16_t
txa_process_event_vector(struct txa_service_data *txa,
			 struct rte_event_vector *vec)
{
	struct txa_service_queue_info *tqi;
	uint16_t port, queue, nb_tx = 0;
	struct rte_mbuf **mbufs;
	int i;

	mbufs = (struct rte_mbuf **)vec->mbufs;
	if (vec->attr_valid) {
		port = vec->port;
		queue = vec->queue;
		tqi = txa_service_queue(txa, port, queue);
		if (unlikely(tqi == NULL || !tqi->added || tqi->stopped)) {
			rte_pktmbuf_free_bulk(&mbufs[vec->elem_offset],
					      vec->nb_elem);
			rte_mempool_put(rte_mempool_from_obj(vec), vec);
			return 0;
		}
		for (i = 0; i < vec->nb_elem; i++) {
			nb_tx += rte_eth_tx_buffer(port, queue, tqi->tx_buf,
						   mbufs[i + vec->elem_offset]);
		}
	} else {
		for (i = vec->elem_offset; i < vec->elem_offset + vec->nb_elem;
		     i++) {
			port = mbufs[i]->port;
			queue = rte_event_eth_tx_adapter_txq_get(mbufs[i]);
			tqi = txa_service_queue(txa, port, queue);
			if (unlikely(tqi == NULL || !tqi->added ||
				     tqi->stopped)) {
				rte_pktmbuf_free(mbufs[i]);
				continue;
			}
			nb_tx += rte_eth_tx_buffer(port, queue, tqi->tx_buf,
						   mbufs[i]);
		}
	}
	rte_mempool_put(rte_mempool_from_obj(vec), vec);

	return nb_tx;
}

static void
txa_service_tx(struct txa_service_data *txa, struct rte_event *ev,
	uint32_t n)
{
	uint32_t i;
	uint16_t nb_tx;
	struct rte_event_eth_tx_adapter_stats *stats;

	stats = &txa->stats;

	nb_tx = 0;
	for (i = 0; i < n; i++) {
		uint16_t port;
		uint16_t queue;
		struct txa_service_queue_info *tqi;

		if (!(ev[i].event_type & RTE_EVENT_TYPE_VECTOR)) {
			struct rte_mbuf *m;

			m = ev[i].mbuf;
			port = m->port;
			queue = rte_event_eth_tx_adapter_txq_get(m);

			tqi = txa_service_queue(txa, port, queue);
			if (unlikely(tqi == NULL || !tqi->added ||
				     tqi->stopped)) {
				rte_pktmbuf_free(m);
				continue;
			}

			nb_tx += rte_eth_tx_buffer(port, queue, tqi->tx_buf, m);
		} else {
			nb_tx += txa_process_event_vector(txa, ev[i].vec);
		}
	}

	stats->tx_packets += nb_tx;
}

static int32_t
txa_service_func(void *args)
{
	struct txa_service_data *txa = args;
	uint8_t dev_id;
	uint8_t port;
	int ret = -EAGAIN;
	uint16_t n;
	uint32_t nb_tx, max_nb_tx;
	struct rte_event ev[TXA_BATCH_SIZE];

	dev_id = txa->eventdev_id;
	max_nb_tx = txa->max_nb_tx;
	port = txa->port_id;

	if (txa->nb_queues == 0)
		return ret;

	if (!rte_spinlock_trylock(&txa->tx_lock))
		return ret;

	for (nb_tx = 0; nb_tx < max_nb_tx; nb_tx += n) {

		n = rte_event_dequeue_burst(dev_id, port, ev, RTE_DIM(ev), 0);
		if (!n)
			break;
		txa_service_tx(txa, ev, n);
		ret = 0;
	}

	if (txa->loop_cnt++ == txa->flush_threshold) {

		struct txa_service_ethdev *tdi;
		struct txa_service_queue_info *tqi;
		struct rte_eth_dev *dev;
		uint16_t i;

		txa->loop_cnt = 0;
		tdi = txa->txa_ethdev;
		nb_tx = 0;

		RTE_ETH_FOREACH_DEV(i) {
			uint16_t q;

			if (i >= txa->dev_count)
				break;

			dev = tdi[i].dev;
			if (tdi[i].nb_queues == 0)
				continue;
			for (q = 0; q < dev->data->nb_tx_queues; q++) {

				tqi = txa_service_queue(txa, i, q);
				if (unlikely(tqi == NULL || !tqi->added ||
					     tqi->stopped))
					continue;

				nb_tx += rte_eth_tx_buffer_flush(i, q,
							tqi->tx_buf);
			}
		}

		if (likely(nb_tx > 0)) {
			txa->stats.tx_packets += nb_tx;
			ret = 0;
		}
	}
	rte_spinlock_unlock(&txa->tx_lock);
	return ret;
}

static int
txa_service_adapter_create(uint8_t id, struct rte_eventdev *dev,
			struct rte_event_port_conf *port_conf)
{
	struct txa_service_data *txa;
	struct rte_event_port_conf *cb_conf;
	int ret;

	cb_conf = rte_malloc(NULL, sizeof(*cb_conf), 0);
	if (cb_conf == NULL)
		return -ENOMEM;

	*cb_conf = *port_conf;
	ret = txa_service_adapter_create_ext(id, dev, txa_service_conf_cb,
					cb_conf);
	if (ret) {
		rte_free(cb_conf);
		return ret;
	}

	txa = txa_service_id_to_data(id);
	txa->conf_free = 1;
	return ret;
}

static int
txa_service_adapter_create_ext(uint8_t id, struct rte_eventdev *dev,
			rte_event_eth_tx_adapter_conf_cb conf_cb,
			void *conf_arg)
{
	struct txa_service_data *txa;
	int socket_id;
	char mem_name[TXA_SERVICE_NAME_LEN];
	int ret;

	if (conf_cb == NULL)
		return -EINVAL;

	socket_id = dev->data->socket_id;
	snprintf(mem_name, TXA_MEM_NAME_LEN,
		"rte_event_eth_txa_%d",
		id);

	ret = txa_service_data_init();
	if (ret != 0)
		return ret;

	txa = rte_zmalloc_socket(mem_name,
				sizeof(*txa),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (txa == NULL) {
		RTE_EDEV_LOG_ERR("failed to get mem for tx adapter");
		return -ENOMEM;
	}

	txa->id = id;
	txa->eventdev_id = dev->data->dev_id;
	txa->socket_id = socket_id;
	strncpy(txa->mem_name, mem_name, TXA_SERVICE_NAME_LEN);
	txa->conf_cb = conf_cb;
	txa->conf_arg = conf_arg;
	txa->service_id = TXA_INVALID_SERVICE_ID;
	rte_spinlock_init(&txa->tx_lock);
	txa_service_data_array[id] = txa;
	txa->flush_threshold = TXA_FLUSH_THRESHOLD;

	return 0;
}

static int
txa_service_event_port_get(uint8_t id, uint8_t *port)
{
	struct txa_service_data *txa;

	txa = txa_service_id_to_data(id);
	if (txa->service_id == TXA_INVALID_SERVICE_ID)
		return -ENODEV;

	*port = txa->port_id;
	return 0;
}

static int
txa_service_adapter_free(uint8_t id)
{
	struct txa_service_data *txa;

	txa = txa_service_id_to_data(id);
	if (txa->nb_queues) {
		RTE_EDEV_LOG_ERR("%" PRIu16 " Tx queues not deleted",
				txa->nb_queues);
		return -EBUSY;
	}

	if (txa->conf_free)
		rte_free(txa->conf_arg);
	rte_free(txa);
	return 0;
}

static int
txa_service_queue_add(uint8_t id,
		__rte_unused struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev,
		int32_t tx_queue_id)
{
	struct txa_service_data *txa;
	struct txa_service_ethdev *tdi;
	struct txa_service_queue_info *tqi;
	struct rte_eth_dev_tx_buffer *tb;
	struct txa_retry *txa_retry;
	int ret = 0;

	txa = txa_service_id_to_data(id);

	if (tx_queue_id == -1) {
		int nb_queues;
		uint16_t i, j;
		uint16_t *qdone;

		nb_queues = eth_dev->data->nb_tx_queues;
		if (txa->dev_count > eth_dev->data->port_id) {
			tdi = &txa->txa_ethdev[eth_dev->data->port_id];
			nb_queues -= tdi->nb_queues;
		}

		qdone = rte_zmalloc(txa->mem_name,
				nb_queues * sizeof(*qdone), 0);
		if (qdone == NULL)
			return -ENOMEM;
		j = 0;
		for (i = 0; i < nb_queues; i++) {
			if (txa_service_is_queue_added(txa, eth_dev, i))
				continue;
			ret = txa_service_queue_add(id, dev, eth_dev, i);
			if (ret == 0)
				qdone[j++] = i;
			else
				break;
		}

		if (i != nb_queues) {
			for (i = 0; i < j; i++)
				txa_service_queue_del(id, eth_dev, qdone[i]);
		}
		rte_free(qdone);
		return ret;
	}

	ret = txa_service_register(txa);
	if (ret)
		return ret;

	rte_spinlock_lock(&txa->tx_lock);

	if (txa_service_is_queue_added(txa, eth_dev, tx_queue_id))
		goto ret_unlock;

	ret = txa_service_queue_array_alloc(txa, eth_dev->data->port_id);
	if (ret)
		goto err_unlock;

	tb = txa_service_tx_buf_alloc(txa, eth_dev);
	if (tb == NULL)
		goto err_unlock;

	tdi = &txa->txa_ethdev[eth_dev->data->port_id];
	tqi = txa_service_queue(txa, eth_dev->data->port_id, tx_queue_id);
	if (tqi == NULL)
		goto err_unlock;

	txa_retry = &tqi->txa_retry;
	txa_retry->id = txa->id;
	txa_retry->port_id = eth_dev->data->port_id;
	txa_retry->tx_queue = tx_queue_id;

	rte_eth_tx_buffer_init(tb, TXA_BATCH_SIZE);
	rte_eth_tx_buffer_set_err_callback(tb,
		txa_service_buffer_retry, txa_retry);

	tqi->tx_buf = tb;
	tqi->added = 1;
	tqi->stopped = false;
	tdi->nb_queues++;
	txa->nb_queues++;

ret_unlock:
	rte_spinlock_unlock(&txa->tx_lock);
	return 0;

err_unlock:
	if (txa->nb_queues == 0) {
		txa_service_queue_array_free(txa,
					eth_dev->data->port_id);
		txa_service_unregister(txa);
	}

	rte_spinlock_unlock(&txa->tx_lock);
	return -1;
}

static inline void
txa_txq_buffer_drain(struct txa_service_queue_info *tqi)
{
	struct rte_eth_dev_tx_buffer *b;
	uint16_t i;

	b = tqi->tx_buf;

	for (i = 0; i < b->length; i++)
		rte_pktmbuf_free(b->pkts[i]);

	b->length = 0;
}

static int
txa_service_queue_del(uint8_t id,
		const struct rte_eth_dev *dev,
		int32_t tx_queue_id)
{
	struct txa_service_data *txa;
	struct txa_service_queue_info *tqi;
	struct rte_eth_dev_tx_buffer *tb;
	uint16_t port_id;

	txa = txa_service_id_to_data(id);
	port_id = dev->data->port_id;

	if (tx_queue_id == -1) {
		uint16_t i, q, nb_queues;
		int ret = 0;

		if (txa->txa_ethdev == NULL)
			return 0;
		nb_queues = txa->txa_ethdev[port_id].nb_queues;
		if (nb_queues == 0)
			return 0;

		i = 0;
		q = 0;
		tqi = txa->txa_ethdev[port_id].queues;

		while (i < nb_queues) {

			if (tqi[q].added) {
				ret = txa_service_queue_del(id, dev, q);
				i++;
				if (ret != 0)
					break;
			}
			q++;
		}
		return ret;
	}

	txa = txa_service_id_to_data(id);

	rte_spinlock_lock(&txa->tx_lock);
	tqi = txa_service_queue(txa, port_id, tx_queue_id);
	if (tqi == NULL || !tqi->added)
		goto ret_unlock;

	/* Drain the buffered mbufs */
	txa_txq_buffer_drain(tqi);
	tb = tqi->tx_buf;
	tqi->added = 0;
	tqi->tx_buf = NULL;
	rte_free(tb);
	txa->nb_queues--;
	txa->txa_ethdev[port_id].nb_queues--;

	txa_service_queue_array_free(txa, port_id);

ret_unlock:
	rte_spinlock_unlock(&txa->tx_lock);
	return 0;
}

static int
txa_service_id_get(uint8_t id, uint32_t *service_id)
{
	struct txa_service_data *txa;

	txa = txa_service_id_to_data(id);
	if (txa->service_id == TXA_INVALID_SERVICE_ID)
		return -ESRCH;

	if (service_id == NULL)
		return -EINVAL;

	*service_id = txa->service_id;

	rte_eventdev_trace_eth_tx_adapter_service_id_get(id, *service_id);
	return 0;
}

static int
txa_service_start(uint8_t id)
{
	return txa_service_ctrl(id, 1);
}

static int
txa_service_stats_get(uint8_t id,
		struct rte_event_eth_tx_adapter_stats *stats)
{
	struct txa_service_data *txa;

	txa = txa_service_id_to_data(id);
	*stats = txa->stats;
	return 0;
}

static int
txa_service_stats_reset(uint8_t id)
{
	struct txa_service_data *txa;

	txa = txa_service_id_to_data(id);
	memset(&txa->stats, 0, sizeof(txa->stats));
	return 0;
}

static int
txa_service_stop(uint8_t id)
{
	return txa_service_ctrl(id, 0);
}


int
rte_event_eth_tx_adapter_create(uint8_t id, uint8_t dev_id,
				struct rte_event_port_conf *port_conf)
{
	struct rte_eventdev *dev;
	int ret;

	if (port_conf == NULL)
		return -EINVAL;

	RTE_EVENT_ETH_TX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_eventdevs[dev_id];

	ret = txa_init();
	if (ret != 0)
		return ret;

	if (txa_adapter_exist(id))
		return -EEXIST;

	txa_dev_id_array[id] = dev_id;
	if (txa_dev_adapter_create(id))
		ret = txa_dev_adapter_create(id)(id, dev);

	if (ret != 0) {
		txa_dev_id_array[id] = TXA_INVALID_DEV_ID;
		return ret;
	}

	ret = txa_service_adapter_create(id, dev, port_conf);
	if (ret != 0) {
		if (txa_dev_adapter_free(id))
			txa_dev_adapter_free(id)(id, dev);
		txa_dev_id_array[id] = TXA_INVALID_DEV_ID;
		return ret;
	}
	rte_eventdev_trace_eth_tx_adapter_create(id, dev_id, NULL, port_conf,
		ret);
	txa_dev_id_array[id] = dev_id;
	return 0;
}

int
rte_event_eth_tx_adapter_create_ext(uint8_t id, uint8_t dev_id,
				rte_event_eth_tx_adapter_conf_cb conf_cb,
				void *conf_arg)
{
	struct rte_eventdev *dev;
	int ret;

	RTE_EVENT_ETH_TX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	ret = txa_init();
	if (ret != 0)
		return ret;

	if (txa_adapter_exist(id))
		return -EINVAL;

	dev = &rte_eventdevs[dev_id];

	txa_dev_id_array[id] = dev_id;
	if (txa_dev_adapter_create_ext(id))
		ret = txa_dev_adapter_create_ext(id)(id, dev);

	if (ret != 0) {
		txa_dev_id_array[id] = TXA_INVALID_DEV_ID;
		return ret;
	}

	ret = txa_service_adapter_create_ext(id, dev, conf_cb, conf_arg);
	if (ret != 0) {
		if (txa_dev_adapter_free(id))
			txa_dev_adapter_free(id)(id, dev);
		txa_dev_id_array[id] = TXA_INVALID_DEV_ID;
		return ret;
	}

	rte_eventdev_trace_eth_tx_adapter_create(id, dev_id, conf_cb, conf_arg,
		ret);
	txa_dev_id_array[id] = dev_id;
	return 0;
}


int
rte_event_eth_tx_adapter_event_port_get(uint8_t id, uint8_t *event_port_id)
{
	rte_eventdev_trace_eth_tx_adapter_event_port_get(id);

	TXA_CHECK_OR_ERR_RET(id);

	return txa_service_event_port_get(id, event_port_id);
}

int
rte_event_eth_tx_adapter_free(uint8_t id)
{
	int ret;

	TXA_CHECK_OR_ERR_RET(id);

	ret = txa_dev_adapter_free(id) ?
		txa_dev_adapter_free(id)(id, txa_evdev(id)) :
		0;

	if (ret == 0)
		ret = txa_service_adapter_free(id);
	txa_dev_id_array[id] = TXA_INVALID_DEV_ID;

	rte_eventdev_trace_eth_tx_adapter_free(id, ret);
	return ret;
}

int
rte_event_eth_tx_adapter_queue_add(uint8_t id,
				uint16_t eth_dev_id,
				int32_t queue)
{
	struct rte_eth_dev *eth_dev;
	int ret;
	uint32_t caps;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);
	TXA_CHECK_OR_ERR_RET(id);

	eth_dev = &rte_eth_devices[eth_dev_id];
	TXA_CHECK_TXQ(eth_dev, queue);

	caps = 0;
	if (txa_dev_caps_get(id))
		txa_dev_caps_get(id)(txa_evdev(id), eth_dev, &caps);

	if (caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT)
		ret =  txa_dev_queue_add(id) ?
					txa_dev_queue_add(id)(id,
							txa_evdev(id),
							eth_dev,
							queue) : 0;
	else
		ret = txa_service_queue_add(id, txa_evdev(id), eth_dev, queue);

	rte_eventdev_trace_eth_tx_adapter_queue_add(id, eth_dev_id, queue,
		ret);
	return ret;
}

int
rte_event_eth_tx_adapter_queue_del(uint8_t id,
				uint16_t eth_dev_id,
				int32_t queue)
{
	struct rte_eth_dev *eth_dev;
	int ret;
	uint32_t caps;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);
	TXA_CHECK_OR_ERR_RET(id);

	eth_dev = &rte_eth_devices[eth_dev_id];

	caps = 0;

	if (txa_dev_caps_get(id))
		txa_dev_caps_get(id)(txa_evdev(id), eth_dev, &caps);

	if (caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT)
		ret =  txa_dev_queue_del(id) ?
					txa_dev_queue_del(id)(id, txa_evdev(id),
							eth_dev,
							queue) : 0;
	else
		ret = txa_service_queue_del(id, eth_dev, queue);

	rte_eventdev_trace_eth_tx_adapter_queue_del(id, eth_dev_id, queue,
		ret);
	return ret;
}

int
rte_event_eth_tx_adapter_service_id_get(uint8_t id, uint32_t *service_id)
{
	TXA_CHECK_OR_ERR_RET(id);

	return txa_service_id_get(id, service_id);
}

int
rte_event_eth_tx_adapter_start(uint8_t id)
{
	int ret;

	TXA_CHECK_OR_ERR_RET(id);

	ret = txa_dev_start(id) ? txa_dev_start(id)(id, txa_evdev(id)) : 0;
	if (ret == 0)
		ret = txa_service_start(id);
	rte_eventdev_trace_eth_tx_adapter_start(id, ret);
	return ret;
}

int
rte_event_eth_tx_adapter_stats_get(uint8_t id,
				struct rte_event_eth_tx_adapter_stats *stats)
{
	int ret;

	TXA_CHECK_OR_ERR_RET(id);

	if (stats == NULL)
		return -EINVAL;

	*stats = (struct rte_event_eth_tx_adapter_stats){0};

	ret = txa_dev_stats_get(id) ?
			txa_dev_stats_get(id)(id, txa_evdev(id), stats) : 0;

	if (ret == 0 && txa_service_id_get(id, NULL) != ESRCH) {
		if (txa_dev_stats_get(id)) {
			struct rte_event_eth_tx_adapter_stats service_stats;

			ret = txa_service_stats_get(id, &service_stats);
			if (ret == 0) {
				stats->tx_retry += service_stats.tx_retry;
				stats->tx_packets += service_stats.tx_packets;
				stats->tx_dropped += service_stats.tx_dropped;
			}
		} else
			ret = txa_service_stats_get(id, stats);
	}

	rte_eventdev_trace_eth_tx_adapter_stats_get(id, stats->tx_retry, stats->tx_packets,
						    stats->tx_dropped, ret);

	return ret;
}

int
rte_event_eth_tx_adapter_stats_reset(uint8_t id)
{
	int ret;

	TXA_CHECK_OR_ERR_RET(id);

	ret = txa_dev_stats_reset(id) ?
		txa_dev_stats_reset(id)(id, txa_evdev(id)) : 0;
	if (ret == 0)
		ret = txa_service_stats_reset(id);

	rte_eventdev_trace_eth_tx_adapter_stats_reset(id, ret);

	return ret;
}

int
rte_event_eth_tx_adapter_runtime_params_init(
		struct rte_event_eth_tx_adapter_runtime_params *txa_params)
{
	if (txa_params == NULL)
		return -EINVAL;

	memset(txa_params, 0, sizeof(*txa_params));
	txa_params->max_nb_tx = TXA_MAX_NB_TX;
	txa_params->flush_threshold = TXA_FLUSH_THRESHOLD;

	return 0;
}

static int
txa_caps_check(struct txa_service_data *txa)
{
	if (!txa->dev_count)
		return -EINVAL;

	if (txa->service_id != TXA_INVALID_SERVICE_ID)
		return 0;

	return -ENOTSUP;
}

int
rte_event_eth_tx_adapter_runtime_params_set(uint8_t id,
		struct rte_event_eth_tx_adapter_runtime_params *txa_params)
{
	struct txa_service_data *txa;
	int ret;

	if (txa_lookup())
		return -ENOMEM;

	TXA_CHECK_OR_ERR_RET(id);

	if (txa_params == NULL)
		return -EINVAL;

	txa = txa_service_id_to_data(id);
	if (txa == NULL)
		return -EINVAL;

	ret = txa_caps_check(txa);
	if (ret)
		return ret;

	rte_spinlock_lock(&txa->tx_lock);
	txa->flush_threshold = txa_params->flush_threshold;
	txa->max_nb_tx = txa_params->max_nb_tx;
	rte_spinlock_unlock(&txa->tx_lock);

	return 0;
}

int
rte_event_eth_tx_adapter_runtime_params_get(uint8_t id,
		struct rte_event_eth_tx_adapter_runtime_params *txa_params)
{
	struct txa_service_data *txa;
	int ret;

	if (txa_lookup())
		return -ENOMEM;

	TXA_CHECK_OR_ERR_RET(id);

	if (txa_params == NULL)
		return -EINVAL;

	txa = txa_service_id_to_data(id);
	if (txa == NULL)
		return -EINVAL;

	ret = txa_caps_check(txa);
	if (ret)
		return ret;

	rte_spinlock_lock(&txa->tx_lock);
	txa_params->flush_threshold = txa->flush_threshold;
	txa_params->max_nb_tx = txa->max_nb_tx;
	rte_spinlock_unlock(&txa->tx_lock);

	return 0;
}

int
rte_event_eth_tx_adapter_stop(uint8_t id)
{
	int ret;

	TXA_CHECK_OR_ERR_RET(id);

	ret = txa_dev_stop(id) ? txa_dev_stop(id)(id,  txa_evdev(id)) : 0;
	if (ret == 0)
		ret = txa_service_stop(id);
	rte_eventdev_trace_eth_tx_adapter_stop(id, ret);
	return ret;
}

int
rte_event_eth_tx_adapter_instance_get(uint16_t eth_dev_id,
				      uint16_t tx_queue_id,
				      uint8_t *txa_inst_id)
{
	uint8_t id;
	int ret = -EINVAL;
	uint32_t caps;
	struct txa_service_data *txa;

	if (txa_lookup())
		return -ENOMEM;

	if (eth_dev_id >= rte_eth_dev_count_avail()) {
		RTE_EDEV_LOG_ERR("Invalid ethernet port id %u", eth_dev_id);
		return -EINVAL;
	}

	if (tx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_tx_queues) {
		RTE_EDEV_LOG_ERR("Invalid tx queue id %u", tx_queue_id);
		return -EINVAL;
	}

	if (txa_inst_id == NULL) {
		RTE_EDEV_LOG_ERR("txa_instance_id cannot be NULL");
		return -EINVAL;
	}

	/* Iterate through all Tx adapter instances */
	for (id = 0; id < RTE_EVENT_ETH_TX_ADAPTER_MAX_INSTANCE; id++) {
		txa = txa_service_id_to_data(id);
		if (!txa)
			continue;

		caps = 0;
		if (rte_event_eth_tx_adapter_caps_get(txa->eventdev_id,
						      eth_dev_id,
						      &caps))
			continue;

		if (caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT) {
			ret = txa_dev_instance_get(id) ?
					txa_dev_instance_get(id)(eth_dev_id,
								 tx_queue_id,
								 txa_inst_id)
							: -EINVAL;
			if (ret == 0) {
				rte_eventdev_trace_eth_tx_adapter_instance_get(eth_dev_id,
								tx_queue_id, *txa_inst_id);
				return ret;
			}
		} else {
			struct rte_eth_dev *eth_dev;

			eth_dev = &rte_eth_devices[eth_dev_id];

			if (txa_service_is_queue_added(txa, eth_dev,
						       tx_queue_id)) {
				*txa_inst_id = txa->id;
				rte_eventdev_trace_eth_tx_adapter_instance_get(eth_dev_id,
								tx_queue_id, *txa_inst_id);
				return 0;
			}
		}
	}

	return -EINVAL;
}

static inline int
txa_sw_queue_start_state_set(uint16_t eth_dev_id, uint16_t tx_queue_id,
			     bool start_state, struct txa_service_data *txa)
{
	struct txa_service_queue_info *tqi = NULL;

	rte_spinlock_lock(&txa->tx_lock);
	tqi = txa_service_queue(txa, eth_dev_id, tx_queue_id);
	if (unlikely(tqi == NULL || !tqi->added)) {
		rte_spinlock_unlock(&txa->tx_lock);
		return -EINVAL;
	}
	if (start_state == false)
		txa_txq_buffer_drain(tqi);

	tqi->stopped = !start_state;
	rte_spinlock_unlock(&txa->tx_lock);
	return 0;
}

static int
txa_queue_start_state_set(uint16_t eth_dev_id, uint16_t tx_queue_id,
			  bool start_state)
{
	struct txa_service_data *txa;
	uint8_t txa_inst_id;
	int ret;
	uint32_t caps = 0;

	/* Below API already does validation of input parameters.
	 * Hence skipping the validation here.
	 */
	ret = rte_event_eth_tx_adapter_instance_get(eth_dev_id,
						    tx_queue_id,
						    &txa_inst_id);
	if (ret < 0)
		return -EINVAL;

	txa = txa_service_id_to_data(txa_inst_id);
	ret = rte_event_eth_tx_adapter_caps_get(txa->eventdev_id,
						eth_dev_id,
						&caps);
	if (ret < 0)
		return -EINVAL;

	if (caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT) {
		if (start_state == true) {
			ret = txa_dev_queue_start(txa_inst_id) ?
			      txa_dev_queue_start(txa_inst_id)(txa_inst_id,
							       eth_dev_id,
							       tx_queue_id) : 0;
		} else {
			ret = txa_dev_queue_stop(txa_inst_id) ?
			      txa_dev_queue_stop(txa_inst_id)(txa_inst_id,
							      eth_dev_id,
							      tx_queue_id) : 0;
		}
		return ret;
	}

	return txa_sw_queue_start_state_set(eth_dev_id, tx_queue_id,
					    start_state, txa);
}

int
rte_event_eth_tx_adapter_queue_start(uint16_t eth_dev_id, uint16_t tx_queue_id)
{
	rte_eventdev_trace_eth_tx_adapter_queue_start(eth_dev_id, tx_queue_id);

	return txa_queue_start_state_set(eth_dev_id, tx_queue_id, true);
}

int
rte_event_eth_tx_adapter_queue_stop(uint16_t eth_dev_id, uint16_t tx_queue_id)
{
	rte_eventdev_trace_eth_tx_adapter_queue_stop(eth_dev_id, tx_queue_id);

	return txa_queue_start_state_set(eth_dev_id, tx_queue_id, false);
}
