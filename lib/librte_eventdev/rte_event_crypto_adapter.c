/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 * All rights reserved.
 */

#include <string.h>
#include <stdbool.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_service_component.h>

#include "rte_eventdev.h"
#include "rte_eventdev_pmd.h"
#include "rte_event_crypto_adapter.h"

#define BATCH_SIZE 32
#define DEFAULT_MAX_NB 128
#define CRYPTO_ADAPTER_NAME_LEN 32
#define CRYPTO_ADAPTER_MEM_NAME_LEN 32
#define CRYPTO_ADAPTER_MAX_EV_ENQ_RETRIES 100

/* Flush an instance's enqueue buffers every CRYPTO_ENQ_FLUSH_THRESHOLD
 * iterations of eca_crypto_adapter_enq_run()
 */
#define CRYPTO_ENQ_FLUSH_THRESHOLD 1024

struct rte_event_crypto_adapter {
	/* Event device identifier */
	uint8_t eventdev_id;
	/* Event port identifier */
	uint8_t event_port_id;
	/* Store event device's implicit release capability */
	uint8_t implicit_release_disabled;
	/* Max crypto ops processed in any service function invocation */
	uint32_t max_nb;
	/* Lock to serialize config updates with service function */
	rte_spinlock_t lock;
	/* Next crypto device to be processed */
	uint16_t next_cdev_id;
	/* Per crypto device structure */
	struct crypto_device_info *cdevs;
	/* Loop counter to flush crypto ops */
	uint16_t transmit_loop_count;
	/* Per instance stats structure */
	struct rte_event_crypto_adapter_stats crypto_stats;
	/* Configuration callback for rte_service configuration */
	rte_event_crypto_adapter_conf_cb conf_cb;
	/* Configuration callback argument */
	void *conf_arg;
	/* Set if  default_cb is being used */
	int default_cb_arg;
	/* Service initialization state */
	uint8_t service_inited;
	/* Memory allocation name */
	char mem_name[CRYPTO_ADAPTER_MEM_NAME_LEN];
	/* Socket identifier cached from eventdev */
	int socket_id;
	/* Per adapter EAL service */
	uint32_t service_id;
	/* No. of queue pairs configured */
	uint16_t nb_qps;
	/* Adapter mode */
	enum rte_event_crypto_adapter_mode mode;
} __rte_cache_aligned;

/* Per crypto device information */
struct crypto_device_info {
	/* Pointer to cryptodev */
	struct rte_cryptodev *dev;
	/* Pointer to queue pair info */
	struct crypto_queue_pair_info *qpairs;
	/* Next queue pair to be processed */
	uint16_t next_queue_pair_id;
	/* Set to indicate cryptodev->eventdev packet
	 * transfer uses a hardware mechanism
	 */
	uint8_t internal_event_port;
	/* Set to indicate processing has been started */
	uint8_t dev_started;
	/* If num_qpairs > 0, the start callback will
	 * be invoked if not already invoked
	 */
	uint16_t num_qpairs;
} __rte_cache_aligned;

/* Per queue pair information */
struct crypto_queue_pair_info {
	/* Set to indicate queue pair is enabled */
	bool qp_enabled;
	/* Pointer to hold rte_crypto_ops for batching */
	struct rte_crypto_op **op_buffer;
	/* No of crypto ops accumulated */
	uint8_t len;
} __rte_cache_aligned;

static struct rte_event_crypto_adapter **event_crypto_adapter;

/* Macros to check for valid adapter */
#define EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, retval) do { \
	if (!eca_valid_id(id)) { \
		RTE_EDEV_LOG_ERR("Invalid crypto adapter id = %d\n", id); \
		return retval; \
	} \
} while (0)

static inline int
eca_valid_id(uint8_t id)
{
	return id < RTE_EVENT_CRYPTO_ADAPTER_MAX_INSTANCE;
}

static int
eca_init(void)
{
	const char *name = "crypto_adapter_array";
	const struct rte_memzone *mz;
	unsigned int sz;

	sz = sizeof(*event_crypto_adapter) *
	    RTE_EVENT_CRYPTO_ADAPTER_MAX_INSTANCE;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	mz = rte_memzone_lookup(name);
	if (mz == NULL) {
		mz = rte_memzone_reserve_aligned(name, sz, rte_socket_id(), 0,
						 RTE_CACHE_LINE_SIZE);
		if (mz == NULL) {
			RTE_EDEV_LOG_ERR("failed to reserve memzone err = %"
					PRId32, rte_errno);
			return -rte_errno;
		}
	}

	event_crypto_adapter = mz->addr;
	return 0;
}

static inline struct rte_event_crypto_adapter *
eca_id_to_adapter(uint8_t id)
{
	return event_crypto_adapter ?
		event_crypto_adapter[id] : NULL;
}

static int
eca_default_config_cb(uint8_t id, uint8_t dev_id,
			struct rte_event_crypto_adapter_conf *conf, void *arg)
{
	struct rte_event_dev_config dev_conf;
	struct rte_eventdev *dev;
	uint8_t port_id;
	int started;
	int ret;
	struct rte_event_port_conf *port_conf = arg;
	struct rte_event_crypto_adapter *adapter = eca_id_to_adapter(id);

	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	dev_conf = dev->data->dev_conf;

	started = dev->data->dev_started;
	if (started)
		rte_event_dev_stop(dev_id);
	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports += 1;
	ret = rte_event_dev_configure(dev_id, &dev_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to configure event dev %u\n", dev_id);
		if (started) {
			if (rte_event_dev_start(dev_id))
				return -EIO;
		}
		return ret;
	}

	ret = rte_event_port_setup(dev_id, port_id, port_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to setup event port %u\n", port_id);
		return ret;
	}

	conf->event_port_id = port_id;
	conf->max_nb = DEFAULT_MAX_NB;
	if (started)
		ret = rte_event_dev_start(dev_id);

	adapter->default_cb_arg = 1;
	return ret;
}

int
rte_event_crypto_adapter_create_ext(uint8_t id, uint8_t dev_id,
				rte_event_crypto_adapter_conf_cb conf_cb,
				enum rte_event_crypto_adapter_mode mode,
				void *conf_arg)
{
	struct rte_event_crypto_adapter *adapter;
	char mem_name[CRYPTO_ADAPTER_NAME_LEN];
	struct rte_event_dev_info dev_info;
	int socket_id;
	uint8_t i;
	int ret;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	if (conf_cb == NULL)
		return -EINVAL;

	if (event_crypto_adapter == NULL) {
		ret = eca_init();
		if (ret)
			return ret;
	}

	adapter = eca_id_to_adapter(id);
	if (adapter != NULL) {
		RTE_EDEV_LOG_ERR("Crypto adapter id %u already exists!", id);
		return -EEXIST;
	}

	socket_id = rte_event_dev_socket_id(dev_id);
	snprintf(mem_name, CRYPTO_ADAPTER_MEM_NAME_LEN,
		 "rte_event_crypto_adapter_%d", id);

	adapter = rte_zmalloc_socket(mem_name, sizeof(*adapter),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (adapter == NULL) {
		RTE_EDEV_LOG_ERR("Failed to get mem for event crypto adapter!");
		return -ENOMEM;
	}

	ret = rte_event_dev_info_get(dev_id, &dev_info);
	if (ret < 0) {
		RTE_EDEV_LOG_ERR("Failed to get info for eventdev %d: %s!",
				 dev_id, dev_info.driver_name);
		return ret;
	}

	adapter->implicit_release_disabled = (dev_info.event_dev_cap &
			RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE);
	adapter->eventdev_id = dev_id;
	adapter->socket_id = socket_id;
	adapter->conf_cb = conf_cb;
	adapter->conf_arg = conf_arg;
	adapter->mode = mode;
	strcpy(adapter->mem_name, mem_name);
	adapter->cdevs = rte_zmalloc_socket(adapter->mem_name,
					rte_cryptodev_count() *
					sizeof(struct crypto_device_info), 0,
					socket_id);
	if (adapter->cdevs == NULL) {
		RTE_EDEV_LOG_ERR("Failed to get mem for crypto devices\n");
		rte_free(adapter);
		return -ENOMEM;
	}

	rte_spinlock_init(&adapter->lock);
	for (i = 0; i < rte_cryptodev_count(); i++)
		adapter->cdevs[i].dev = rte_cryptodev_pmd_get_dev(i);

	event_crypto_adapter[id] = adapter;

	return 0;
}


int
rte_event_crypto_adapter_create(uint8_t id, uint8_t dev_id,
				struct rte_event_port_conf *port_config,
				enum rte_event_crypto_adapter_mode mode)
{
	struct rte_event_port_conf *pc;
	int ret;

	if (port_config == NULL)
		return -EINVAL;
	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	pc = rte_malloc(NULL, sizeof(*pc), 0);
	if (pc == NULL)
		return -ENOMEM;
	*pc = *port_config;
	ret = rte_event_crypto_adapter_create_ext(id, dev_id,
						  eca_default_config_cb,
						  mode,
						  pc);
	if (ret)
		rte_free(pc);

	return ret;
}

int
rte_event_crypto_adapter_free(uint8_t id)
{
	struct rte_event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	if (adapter->nb_qps) {
		RTE_EDEV_LOG_ERR("%" PRIu16 "Queue pairs not deleted",
				adapter->nb_qps);
		return -EBUSY;
	}

	if (adapter->default_cb_arg)
		rte_free(adapter->conf_arg);
	rte_free(adapter->cdevs);
	rte_free(adapter);
	event_crypto_adapter[id] = NULL;

	return 0;
}

static inline unsigned int
eca_enq_to_cryptodev(struct rte_event_crypto_adapter *adapter,
		 struct rte_event *ev, unsigned int cnt)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	union rte_event_crypto_metadata *m_data = NULL;
	struct crypto_queue_pair_info *qp_info = NULL;
	struct rte_crypto_op *crypto_op;
	unsigned int i, n;
	uint16_t qp_id, len, ret;
	uint8_t cdev_id;

	len = 0;
	ret = 0;
	n = 0;
	stats->event_deq_count += cnt;

	for (i = 0; i < cnt; i++) {
		crypto_op = ev[i].event_ptr;
		if (crypto_op == NULL)
			continue;
		if (crypto_op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			m_data = rte_cryptodev_sym_session_get_user_data(
					crypto_op->sym->session);
			if (m_data == NULL) {
				rte_pktmbuf_free(crypto_op->sym->m_src);
				rte_crypto_op_free(crypto_op);
				continue;
			}

			cdev_id = m_data->request_info.cdev_id;
			qp_id = m_data->request_info.queue_pair_id;
			qp_info = &adapter->cdevs[cdev_id].qpairs[qp_id];
			if (!qp_info->qp_enabled) {
				rte_pktmbuf_free(crypto_op->sym->m_src);
				rte_crypto_op_free(crypto_op);
				continue;
			}
			len = qp_info->len;
			qp_info->op_buffer[len] = crypto_op;
			len++;
		} else if (crypto_op->sess_type == RTE_CRYPTO_OP_SESSIONLESS &&
				crypto_op->private_data_offset) {
			m_data = (union rte_event_crypto_metadata *)
				 ((uint8_t *)crypto_op +
					crypto_op->private_data_offset);
			cdev_id = m_data->request_info.cdev_id;
			qp_id = m_data->request_info.queue_pair_id;
			qp_info = &adapter->cdevs[cdev_id].qpairs[qp_id];
			if (!qp_info->qp_enabled) {
				rte_pktmbuf_free(crypto_op->sym->m_src);
				rte_crypto_op_free(crypto_op);
				continue;
			}
			len = qp_info->len;
			qp_info->op_buffer[len] = crypto_op;
			len++;
		} else {
			rte_pktmbuf_free(crypto_op->sym->m_src);
			rte_crypto_op_free(crypto_op);
			continue;
		}

		if (len == BATCH_SIZE) {
			struct rte_crypto_op **op_buffer = qp_info->op_buffer;
			ret = rte_cryptodev_enqueue_burst(cdev_id,
							  qp_id,
							  op_buffer,
							  BATCH_SIZE);

			stats->crypto_enq_count += ret;

			while (ret < len) {
				struct rte_crypto_op *op;
				op = op_buffer[ret++];
				stats->crypto_enq_fail++;
				rte_pktmbuf_free(op->sym->m_src);
				rte_crypto_op_free(op);
			}

			len = 0;
		}

		if (qp_info)
			qp_info->len = len;
		n += ret;
	}

	return n;
}

static unsigned int
eca_crypto_enq_flush(struct rte_event_crypto_adapter *adapter)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	struct crypto_device_info *curr_dev;
	struct crypto_queue_pair_info *curr_queue;
	struct rte_crypto_op **op_buffer;
	struct rte_cryptodev *dev;
	uint8_t cdev_id;
	uint16_t qp;
	uint16_t ret;
	uint16_t num_cdev = rte_cryptodev_count();

	ret = 0;
	for (cdev_id = 0; cdev_id < num_cdev; cdev_id++) {
		curr_dev = &adapter->cdevs[cdev_id];
		dev = curr_dev->dev;
		if (dev == NULL)
			continue;
		for (qp = 0; qp < dev->data->nb_queue_pairs; qp++) {

			curr_queue = &curr_dev->qpairs[qp];
			if (!curr_queue->qp_enabled)
				continue;

			op_buffer = curr_queue->op_buffer;
			ret = rte_cryptodev_enqueue_burst(cdev_id,
							  qp,
							  op_buffer,
							  curr_queue->len);
			stats->crypto_enq_count += ret;

			while (ret < curr_queue->len) {
				struct rte_crypto_op *op;
				op = op_buffer[ret++];
				stats->crypto_enq_fail++;
				rte_pktmbuf_free(op->sym->m_src);
				rte_crypto_op_free(op);
			}
			curr_queue->len = 0;
		}
	}

	return ret;
}

static int
eca_crypto_adapter_enq_run(struct rte_event_crypto_adapter *adapter,
			unsigned int max_enq)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	struct rte_event ev[BATCH_SIZE];
	unsigned int nb_enq, nb_enqueued;
	uint16_t n;
	uint8_t event_dev_id = adapter->eventdev_id;
	uint8_t event_port_id = adapter->event_port_id;

	nb_enqueued = 0;
	if (adapter->mode == RTE_EVENT_CRYPTO_ADAPTER_OP_NEW)
		return 0;

	for (nb_enq = 0; nb_enq < max_enq; nb_enq += n) {
		stats->event_poll_count++;
		n = rte_event_dequeue_burst(event_dev_id,
					    event_port_id, ev, BATCH_SIZE, 0);

		if (!n)
			break;

		nb_enqueued += eca_enq_to_cryptodev(adapter, ev, n);
	}

	if ((++adapter->transmit_loop_count &
		(CRYPTO_ENQ_FLUSH_THRESHOLD - 1)) == 0) {
		nb_enqueued += eca_crypto_enq_flush(adapter);
	}

	return nb_enqueued;
}

static inline void
eca_ops_enqueue_burst(struct rte_event_crypto_adapter *adapter,
		  struct rte_crypto_op **ops, uint16_t num)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	union rte_event_crypto_metadata *m_data = NULL;
	uint8_t event_dev_id = adapter->eventdev_id;
	uint8_t event_port_id = adapter->event_port_id;
	struct rte_event events[BATCH_SIZE];
	uint16_t nb_enqueued, nb_ev;
	uint8_t retry;
	uint8_t i;

	nb_ev = 0;
	retry = 0;
	nb_enqueued = 0;
	num = RTE_MIN(num, BATCH_SIZE);
	for (i = 0; i < num; i++) {
		struct rte_event *ev = &events[nb_ev++];
		if (ops[i]->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			m_data = rte_cryptodev_sym_session_get_user_data(
					ops[i]->sym->session);
		} else if (ops[i]->sess_type == RTE_CRYPTO_OP_SESSIONLESS &&
				ops[i]->private_data_offset) {
			m_data = (union rte_event_crypto_metadata *)
				 ((uint8_t *)ops[i] +
				  ops[i]->private_data_offset);
		}

		if (unlikely(m_data == NULL)) {
			rte_pktmbuf_free(ops[i]->sym->m_src);
			rte_crypto_op_free(ops[i]);
			continue;
		}

		rte_memcpy(ev, &m_data->response_info, sizeof(*ev));
		ev->event_ptr = ops[i];
		ev->event_type = RTE_EVENT_TYPE_CRYPTODEV;
		if (adapter->implicit_release_disabled)
			ev->op = RTE_EVENT_OP_FORWARD;
		else
			ev->op = RTE_EVENT_OP_NEW;
	}

	do {
		nb_enqueued += rte_event_enqueue_burst(event_dev_id,
						  event_port_id,
						  &events[nb_enqueued],
						  nb_ev - nb_enqueued);
	} while (retry++ < CRYPTO_ADAPTER_MAX_EV_ENQ_RETRIES &&
		 nb_enqueued < nb_ev);

	/* Free mbufs and rte_crypto_ops for failed events */
	for (i = nb_enqueued; i < nb_ev; i++) {
		struct rte_crypto_op *op = events[i].event_ptr;
		rte_pktmbuf_free(op->sym->m_src);
		rte_crypto_op_free(op);
	}

	stats->event_enq_fail_count += nb_ev - nb_enqueued;
	stats->event_enq_count += nb_enqueued;
	stats->event_enq_retry_count += retry - 1;
}

static inline unsigned int
eca_crypto_adapter_deq_run(struct rte_event_crypto_adapter *adapter,
			unsigned int max_deq)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	struct crypto_device_info *curr_dev;
	struct crypto_queue_pair_info *curr_queue;
	struct rte_crypto_op *ops[BATCH_SIZE];
	uint16_t n, nb_deq;
	struct rte_cryptodev *dev;
	uint8_t cdev_id;
	uint16_t qp, dev_qps;
	bool done;
	uint16_t num_cdev = rte_cryptodev_count();

	nb_deq = 0;
	do {
		uint16_t queues = 0;
		done = true;

		for (cdev_id = adapter->next_cdev_id;
			cdev_id < num_cdev; cdev_id++) {
			curr_dev = &adapter->cdevs[cdev_id];
			dev = curr_dev->dev;
			if (dev == NULL)
				continue;
			dev_qps = dev->data->nb_queue_pairs;

			for (qp = curr_dev->next_queue_pair_id;
				queues < dev_qps; qp = (qp + 1) % dev_qps,
				queues++) {

				curr_queue = &curr_dev->qpairs[qp];
				if (!curr_queue->qp_enabled)
					continue;

				n = rte_cryptodev_dequeue_burst(cdev_id, qp,
					ops, BATCH_SIZE);
				if (!n)
					continue;

				done = false;
				stats->crypto_deq_count += n;
				eca_ops_enqueue_burst(adapter, ops, n);
				nb_deq += n;

				if (nb_deq > max_deq) {
					if ((qp + 1) == dev_qps) {
						adapter->next_cdev_id =
							(cdev_id + 1)
							% num_cdev;
					}
					curr_dev->next_queue_pair_id = (qp + 1)
						% dev->data->nb_queue_pairs;

					return nb_deq;
				}
			}
		}
	} while (done == false);
	return nb_deq;
}

static void
eca_crypto_adapter_run(struct rte_event_crypto_adapter *adapter,
			unsigned int max_ops)
{
	while (max_ops) {
		unsigned int e_cnt, d_cnt;

		e_cnt = eca_crypto_adapter_deq_run(adapter, max_ops);
		max_ops -= RTE_MIN(max_ops, e_cnt);

		d_cnt = eca_crypto_adapter_enq_run(adapter, max_ops);
		max_ops -= RTE_MIN(max_ops, d_cnt);

		if (e_cnt == 0 && d_cnt == 0)
			break;

	}
}

static int
eca_service_func(void *args)
{
	struct rte_event_crypto_adapter *adapter = args;

	if (rte_spinlock_trylock(&adapter->lock) == 0)
		return 0;
	eca_crypto_adapter_run(adapter, adapter->max_nb);
	rte_spinlock_unlock(&adapter->lock);

	return 0;
}

static int
eca_init_service(struct rte_event_crypto_adapter *adapter, uint8_t id)
{
	struct rte_event_crypto_adapter_conf adapter_conf;
	struct rte_service_spec service;
	int ret;

	if (adapter->service_inited)
		return 0;

	memset(&service, 0, sizeof(service));
	snprintf(service.name, CRYPTO_ADAPTER_NAME_LEN,
		"rte_event_crypto_adapter_%d", id);
	service.socket_id = adapter->socket_id;
	service.callback = eca_service_func;
	service.callback_userdata = adapter;
	/* Service function handles locking for queue add/del updates */
	service.capabilities = RTE_SERVICE_CAP_MT_SAFE;
	ret = rte_service_component_register(&service, &adapter->service_id);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to register service %s err = %" PRId32,
			service.name, ret);
		return ret;
	}

	ret = adapter->conf_cb(id, adapter->eventdev_id,
		&adapter_conf, adapter->conf_arg);
	if (ret) {
		RTE_EDEV_LOG_ERR("configuration callback failed err = %" PRId32,
			ret);
		return ret;
	}

	adapter->max_nb = adapter_conf.max_nb;
	adapter->event_port_id = adapter_conf.event_port_id;
	adapter->service_inited = 1;

	return ret;
}

static void
eca_update_qp_info(struct rte_event_crypto_adapter *adapter,
			struct crypto_device_info *dev_info,
			int32_t queue_pair_id,
			uint8_t add)
{
	struct crypto_queue_pair_info *qp_info;
	int enabled;
	uint16_t i;

	if (dev_info->qpairs == NULL)
		return;

	if (queue_pair_id == -1) {
		for (i = 0; i < dev_info->dev->data->nb_queue_pairs; i++)
			eca_update_qp_info(adapter, dev_info, i, add);
	} else {
		qp_info = &dev_info->qpairs[queue_pair_id];
		enabled = qp_info->qp_enabled;
		if (add) {
			adapter->nb_qps += !enabled;
			dev_info->num_qpairs += !enabled;
		} else {
			adapter->nb_qps -= enabled;
			dev_info->num_qpairs -= enabled;
		}
		qp_info->qp_enabled = !!add;
	}
}

static int
eca_add_queue_pair(struct rte_event_crypto_adapter *adapter,
		uint8_t cdev_id,
		int queue_pair_id)
{
	struct crypto_device_info *dev_info = &adapter->cdevs[cdev_id];
	struct crypto_queue_pair_info *qpairs;
	uint32_t i;

	if (dev_info->qpairs == NULL) {
		dev_info->qpairs =
		    rte_zmalloc_socket(adapter->mem_name,
					dev_info->dev->data->nb_queue_pairs *
					sizeof(struct crypto_queue_pair_info),
					0, adapter->socket_id);
		if (dev_info->qpairs == NULL)
			return -ENOMEM;

		qpairs = dev_info->qpairs;
		qpairs->op_buffer = rte_zmalloc_socket(adapter->mem_name,
					BATCH_SIZE *
					sizeof(struct rte_crypto_op *),
					0, adapter->socket_id);
		if (!qpairs->op_buffer) {
			rte_free(qpairs);
			return -ENOMEM;
		}
	}

	if (queue_pair_id == -1) {
		for (i = 0; i < dev_info->dev->data->nb_queue_pairs; i++)
			eca_update_qp_info(adapter, dev_info, i, 1);
	} else
		eca_update_qp_info(adapter, dev_info,
					(uint16_t)queue_pair_id, 1);

	return 0;
}

int
rte_event_crypto_adapter_queue_pair_add(uint8_t id,
			uint8_t cdev_id,
			int32_t queue_pair_id,
			const struct rte_event *event)
{
	struct rte_event_crypto_adapter *adapter;
	struct rte_eventdev *dev;
	struct crypto_device_info *dev_info;
	uint32_t cap;
	int ret;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (!rte_cryptodev_pmd_is_valid_dev(cdev_id)) {
		RTE_EDEV_LOG_ERR("Invalid dev_id=%" PRIu8, cdev_id);
		return -EINVAL;
	}

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	ret = rte_event_crypto_adapter_caps_get(adapter->eventdev_id,
						cdev_id,
						&cap);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps dev %" PRIu8
			" cdev %" PRIu8, id, cdev_id);
		return ret;
	}

	if ((cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) &&
	    (event == NULL)) {
		RTE_EDEV_LOG_ERR("Conf value can not be NULL for dev_id=%u",
				  cdev_id);
		return -EINVAL;
	}

	dev_info = &adapter->cdevs[cdev_id];

	if (queue_pair_id != -1 &&
	    (uint16_t)queue_pair_id >= dev_info->dev->data->nb_queue_pairs) {
		RTE_EDEV_LOG_ERR("Invalid queue_pair_id %" PRIu16,
				 (uint16_t)queue_pair_id);
		return -EINVAL;
	}

	/* In case HW cap is RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD,
	 * no need of service core as HW supports event forward capability.
	 */
	if ((cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) ||
	    (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND &&
	     adapter->mode == RTE_EVENT_CRYPTO_ADAPTER_OP_NEW) ||
	    (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW &&
	     adapter->mode == RTE_EVENT_CRYPTO_ADAPTER_OP_NEW)) {
		RTE_FUNC_PTR_OR_ERR_RET(
			*dev->dev_ops->crypto_adapter_queue_pair_add,
			-ENOTSUP);
		if (dev_info->qpairs == NULL) {
			dev_info->qpairs =
			    rte_zmalloc_socket(adapter->mem_name,
					dev_info->dev->data->nb_queue_pairs *
					sizeof(struct crypto_queue_pair_info),
					0, adapter->socket_id);
			if (dev_info->qpairs == NULL)
				return -ENOMEM;
		}

		ret = (*dev->dev_ops->crypto_adapter_queue_pair_add)(dev,
				dev_info->dev,
				queue_pair_id,
				event);
		if (ret)
			return ret;

		else
			eca_update_qp_info(adapter, &adapter->cdevs[cdev_id],
					   queue_pair_id, 1);
	}

	/* In case HW cap is RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW,
	 * or SW adapter, initiate services so the application can choose
	 * which ever way it wants to use the adapter.
	 * Case 1: RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW
	 *         Application may wants to use one of below two mode
	 *          a. OP_FORWARD mode -> HW Dequeue + SW enqueue
	 *          b. OP_NEW mode -> HW Dequeue
	 * Case 2: No HW caps, use SW adapter
	 *          a. OP_FORWARD mode -> SW enqueue & dequeue
	 *          b. OP_NEW mode -> SW Dequeue
	 */
	if ((cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW &&
	     adapter->mode == RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD) ||
	     (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW) &&
	      !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	      !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) &&
	       (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA))) {
		rte_spinlock_lock(&adapter->lock);
		ret = eca_init_service(adapter, id);
		if (ret == 0)
			ret = eca_add_queue_pair(adapter, cdev_id,
						 queue_pair_id);
		rte_spinlock_unlock(&adapter->lock);

		if (ret)
			return ret;

		rte_service_component_runstate_set(adapter->service_id, 1);
	}

	return 0;
}

int
rte_event_crypto_adapter_queue_pair_del(uint8_t id, uint8_t cdev_id,
					int32_t queue_pair_id)
{
	struct rte_event_crypto_adapter *adapter;
	struct crypto_device_info *dev_info;
	struct rte_eventdev *dev;
	int ret;
	uint32_t cap;
	uint16_t i;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (!rte_cryptodev_pmd_is_valid_dev(cdev_id)) {
		RTE_EDEV_LOG_ERR("Invalid dev_id=%" PRIu8, cdev_id);
		return -EINVAL;
	}

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	ret = rte_event_crypto_adapter_caps_get(adapter->eventdev_id,
						cdev_id,
						&cap);
	if (ret)
		return ret;

	dev_info = &adapter->cdevs[cdev_id];

	if (queue_pair_id != -1 &&
	    (uint16_t)queue_pair_id >= dev_info->dev->data->nb_queue_pairs) {
		RTE_EDEV_LOG_ERR("Invalid queue_pair_id %" PRIu16,
				 (uint16_t)queue_pair_id);
		return -EINVAL;
	}

	if ((cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) ||
	    (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW &&
	     adapter->mode == RTE_EVENT_CRYPTO_ADAPTER_OP_NEW)) {
		RTE_FUNC_PTR_OR_ERR_RET(
			*dev->dev_ops->crypto_adapter_queue_pair_del,
			-ENOTSUP);
		ret = (*dev->dev_ops->crypto_adapter_queue_pair_del)(dev,
						dev_info->dev,
						queue_pair_id);
		if (ret == 0) {
			eca_update_qp_info(adapter,
					&adapter->cdevs[cdev_id],
					queue_pair_id,
					0);
			if (dev_info->num_qpairs == 0) {
				rte_free(dev_info->qpairs);
				dev_info->qpairs = NULL;
			}
		}
	} else {
		if (adapter->nb_qps == 0)
			return 0;

		rte_spinlock_lock(&adapter->lock);
		if (queue_pair_id == -1) {
			for (i = 0; i < dev_info->dev->data->nb_queue_pairs;
				i++)
				eca_update_qp_info(adapter, dev_info,
							queue_pair_id, 0);
		} else {
			eca_update_qp_info(adapter, dev_info,
						(uint16_t)queue_pair_id, 0);
		}

		if (dev_info->num_qpairs == 0) {
			rte_free(dev_info->qpairs);
			dev_info->qpairs = NULL;
		}

		rte_spinlock_unlock(&adapter->lock);
		rte_service_component_runstate_set(adapter->service_id,
				adapter->nb_qps);
	}

	return ret;
}

static int
eca_adapter_ctrl(uint8_t id, int start)
{
	struct rte_event_crypto_adapter *adapter;
	struct crypto_device_info *dev_info;
	struct rte_eventdev *dev;
	uint32_t i;
	int use_service;
	int stop = !start;

	use_service = 0;
	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];

	for (i = 0; i < rte_cryptodev_count(); i++) {
		dev_info = &adapter->cdevs[i];
		/* if start  check for num queue pairs */
		if (start && !dev_info->num_qpairs)
			continue;
		/* if stop check if dev has been started */
		if (stop && !dev_info->dev_started)
			continue;
		use_service |= !dev_info->internal_event_port;
		dev_info->dev_started = start;
		if (dev_info->internal_event_port == 0)
			continue;
		start ? (*dev->dev_ops->crypto_adapter_start)(dev,
						&dev_info->dev[i]) :
			(*dev->dev_ops->crypto_adapter_stop)(dev,
						&dev_info->dev[i]);
	}

	if (use_service)
		rte_service_runstate_set(adapter->service_id, start);

	return 0;
}

int
rte_event_crypto_adapter_start(uint8_t id)
{
	struct rte_event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	return eca_adapter_ctrl(id, 1);
}

int
rte_event_crypto_adapter_stop(uint8_t id)
{
	return eca_adapter_ctrl(id, 0);
}

int
rte_event_crypto_adapter_stats_get(uint8_t id,
				struct rte_event_crypto_adapter_stats *stats)
{
	struct rte_event_crypto_adapter *adapter;
	struct rte_event_crypto_adapter_stats dev_stats_sum = { 0 };
	struct rte_event_crypto_adapter_stats dev_stats;
	struct rte_eventdev *dev;
	struct crypto_device_info *dev_info;
	uint32_t i;
	int ret;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL || stats == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	memset(stats, 0, sizeof(*stats));
	for (i = 0; i < rte_cryptodev_count(); i++) {
		dev_info = &adapter->cdevs[i];
		if (dev_info->internal_event_port == 0 ||
			dev->dev_ops->crypto_adapter_stats_get == NULL)
			continue;
		ret = (*dev->dev_ops->crypto_adapter_stats_get)(dev,
						dev_info->dev,
						&dev_stats);
		if (ret)
			continue;

		dev_stats_sum.crypto_deq_count += dev_stats.crypto_deq_count;
		dev_stats_sum.event_enq_count +=
			dev_stats.event_enq_count;
	}

	if (adapter->service_inited)
		*stats = adapter->crypto_stats;

	stats->crypto_deq_count += dev_stats_sum.crypto_deq_count;
	stats->event_enq_count += dev_stats_sum.event_enq_count;

	return 0;
}

int
rte_event_crypto_adapter_stats_reset(uint8_t id)
{
	struct rte_event_crypto_adapter *adapter;
	struct crypto_device_info *dev_info;
	struct rte_eventdev *dev;
	uint32_t i;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	for (i = 0; i < rte_cryptodev_count(); i++) {
		dev_info = &adapter->cdevs[i];
		if (dev_info->internal_event_port == 0 ||
			dev->dev_ops->crypto_adapter_stats_reset == NULL)
			continue;
		(*dev->dev_ops->crypto_adapter_stats_reset)(dev,
						dev_info->dev);
	}

	memset(&adapter->crypto_stats, 0, sizeof(adapter->crypto_stats));
	return 0;
}

int
rte_event_crypto_adapter_service_id_get(uint8_t id, uint32_t *service_id)
{
	struct rte_event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL || service_id == NULL)
		return -EINVAL;

	if (adapter->service_inited)
		*service_id = adapter->service_id;

	return adapter->service_inited ? 0 : -ESRCH;
}

int
rte_event_crypto_adapter_event_port_get(uint8_t id, uint8_t *event_port_id)
{
	struct rte_event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL || event_port_id == NULL)
		return -EINVAL;

	*event_port_id = adapter->event_port_id;

	return 0;
}
