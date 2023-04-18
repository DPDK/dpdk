/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 * All rights reserved.
 */

#include <string.h>
#include <stdbool.h>
#include <rte_common.h>
#include <dev_driver.h>
#include <rte_errno.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_service_component.h>

#include "rte_eventdev.h"
#include "eventdev_pmd.h"
#include "eventdev_trace.h"
#include "rte_event_crypto_adapter.h"

#define BATCH_SIZE 32
#define DEFAULT_MAX_NB 128
#define CRYPTO_ADAPTER_NAME_LEN 32
#define CRYPTO_ADAPTER_MEM_NAME_LEN 32
#define CRYPTO_ADAPTER_MAX_EV_ENQ_RETRIES 100

/* MAX_OPS_IN_BUFFER contains size for  batch of dequeued events */
#define MAX_OPS_IN_BUFFER BATCH_SIZE

/* CRYPTO_ADAPTER_OPS_BUFFER_SZ to accommodate MAX_OPS_IN_BUFFER +
 * additional space for one batch
 */
#define CRYPTO_ADAPTER_OPS_BUFFER_SZ (MAX_OPS_IN_BUFFER + BATCH_SIZE)

#define CRYPTO_ADAPTER_BUFFER_SZ 1024

/* Flush an instance's enqueue buffers every CRYPTO_ENQ_FLUSH_THRESHOLD
 * iterations of eca_crypto_adapter_enq_run()
 */
#define CRYPTO_ENQ_FLUSH_THRESHOLD 1024

#define ECA_ADAPTER_ARRAY "crypto_adapter_array"

struct crypto_ops_circular_buffer {
	/* index of head element in circular buffer */
	uint16_t head;
	/* index of tail element in circular buffer */
	uint16_t tail;
	/* number of elements in buffer */
	uint16_t count;
	/* size of circular buffer */
	uint16_t size;
	/* Pointer to hold rte_crypto_ops for batching */
	struct rte_crypto_op **op_buffer;
} __rte_cache_aligned;

struct event_crypto_adapter {
	/* Event device identifier */
	uint8_t eventdev_id;
	/* Event port identifier */
	uint8_t event_port_id;
	/* Store event port's implicit release capability */
	uint8_t implicit_release_disabled;
	/* Flag to indicate backpressure at cryptodev
	 * Stop further dequeuing events from eventdev
	 */
	bool stop_enq_to_cryptodev;
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
	/* Circular buffer for batching crypto ops to eventdev */
	struct crypto_ops_circular_buffer ebuf;
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
	/* Circular buffer for batching crypto ops to cdev */
	struct crypto_ops_circular_buffer cbuf;
} __rte_cache_aligned;

static struct event_crypto_adapter **event_crypto_adapter;

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
	const struct rte_memzone *mz;
	unsigned int sz;

	sz = sizeof(*event_crypto_adapter) *
	    RTE_EVENT_CRYPTO_ADAPTER_MAX_INSTANCE;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);

	mz = rte_memzone_lookup(ECA_ADAPTER_ARRAY);
	if (mz == NULL) {
		mz = rte_memzone_reserve_aligned(ECA_ADAPTER_ARRAY, sz,
						 rte_socket_id(), 0,
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

static int
eca_memzone_lookup(void)
{
	const struct rte_memzone *mz;

	if (event_crypto_adapter == NULL) {
		mz = rte_memzone_lookup(ECA_ADAPTER_ARRAY);
		if (mz == NULL)
			return -ENOMEM;

		event_crypto_adapter = mz->addr;
	}

	return 0;
}

static inline bool
eca_circular_buffer_batch_ready(struct crypto_ops_circular_buffer *bufp)
{
	return bufp->count >= BATCH_SIZE;
}

static inline bool
eca_circular_buffer_space_for_batch(struct crypto_ops_circular_buffer *bufp)
{
	/* circular buffer can have atmost MAX_OPS_IN_BUFFER */
	return (bufp->size - bufp->count) >= MAX_OPS_IN_BUFFER;
}

static inline void
eca_circular_buffer_free(struct crypto_ops_circular_buffer *bufp)
{
	rte_free(bufp->op_buffer);
}

static inline int
eca_circular_buffer_init(const char *name,
			 struct crypto_ops_circular_buffer *bufp,
			 uint16_t sz)
{
	bufp->op_buffer = rte_zmalloc(name,
				      sizeof(struct rte_crypto_op *) * sz,
				      0);
	if (bufp->op_buffer == NULL)
		return -ENOMEM;

	bufp->size = sz;
	return 0;
}

static inline int
eca_circular_buffer_add(struct crypto_ops_circular_buffer *bufp,
			struct rte_crypto_op *op)
{
	uint16_t *tailp = &bufp->tail;

	bufp->op_buffer[*tailp] = op;
	/* circular buffer, go round */
	*tailp = (*tailp + 1) % bufp->size;
	bufp->count++;

	return 0;
}

static inline int
eca_circular_buffer_flush_to_cdev(struct crypto_ops_circular_buffer *bufp,
				  uint8_t cdev_id, uint16_t qp_id,
				  uint16_t *nb_ops_flushed)
{
	uint16_t n = 0;
	uint16_t *headp = &bufp->head;
	uint16_t *tailp = &bufp->tail;
	struct rte_crypto_op **ops = bufp->op_buffer;

	if (*tailp > *headp)
		n = *tailp - *headp;
	else if (*tailp < *headp)
		n = bufp->size - *headp;
	else {
		*nb_ops_flushed = 0;
		return 0;  /* buffer empty */
	}

	*nb_ops_flushed = rte_cryptodev_enqueue_burst(cdev_id, qp_id,
						      &ops[*headp], n);
	bufp->count -= *nb_ops_flushed;
	if (!bufp->count) {
		*headp = 0;
		*tailp = 0;
	} else
		*headp = (*headp + *nb_ops_flushed) % bufp->size;

	return *nb_ops_flushed == n ? 0 : -1;
}

static inline struct event_crypto_adapter *
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
	struct event_crypto_adapter *adapter = eca_id_to_adapter(id);

	if (adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[adapter->eventdev_id];
	dev_conf = dev->data->dev_conf;

	started = dev->data->dev_started;
	if (started)
		rte_event_dev_stop(dev_id);
	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports += 1;
	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_SINGLE_LINK)
		dev_conf.nb_single_link_event_port_queues += 1;

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
	struct event_crypto_adapter *adapter;
	char mem_name[CRYPTO_ADAPTER_NAME_LEN];
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

	if (eca_circular_buffer_init("eca_edev_circular_buffer",
				     &adapter->ebuf,
				     CRYPTO_ADAPTER_BUFFER_SZ)) {
		RTE_EDEV_LOG_ERR("Failed to get memory for eventdev buffer");
		rte_free(adapter);
		return -ENOMEM;
	}

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
		eca_circular_buffer_free(&adapter->ebuf);
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

	rte_eventdev_trace_crypto_adapter_create(id, dev_id, port_config, mode,	ret);

	return ret;
}

int
rte_event_crypto_adapter_free(uint8_t id)
{
	struct event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	if (adapter->nb_qps) {
		RTE_EDEV_LOG_ERR("%" PRIu16 "Queue pairs not deleted",
				adapter->nb_qps);
		return -EBUSY;
	}

	rte_eventdev_trace_crypto_adapter_free(id, adapter);
	if (adapter->default_cb_arg)
		rte_free(adapter->conf_arg);
	rte_free(adapter->cdevs);
	rte_free(adapter);
	event_crypto_adapter[id] = NULL;

	return 0;
}

static inline unsigned int
eca_enq_to_cryptodev(struct event_crypto_adapter *adapter, struct rte_event *ev,
		     unsigned int cnt)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	union rte_event_crypto_metadata *m_data = NULL;
	struct crypto_queue_pair_info *qp_info = NULL;
	struct rte_crypto_op *crypto_op;
	unsigned int i, n;
	uint16_t qp_id, nb_enqueued = 0;
	uint8_t cdev_id;
	int ret;

	ret = 0;
	n = 0;
	stats->event_deq_count += cnt;

	for (i = 0; i < cnt; i++) {
		crypto_op = ev[i].event_ptr;
		if (crypto_op == NULL)
			continue;
		m_data = rte_cryptodev_session_event_mdata_get(crypto_op);
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
		eca_circular_buffer_add(&qp_info->cbuf, crypto_op);

		if (eca_circular_buffer_batch_ready(&qp_info->cbuf)) {
			ret = eca_circular_buffer_flush_to_cdev(&qp_info->cbuf,
								cdev_id,
								qp_id,
								&nb_enqueued);
			stats->crypto_enq_count += nb_enqueued;
			n += nb_enqueued;

			/**
			 * If some crypto ops failed to flush to cdev and
			 * space for another batch is not available, stop
			 * dequeue from eventdev momentarily
			 */
			if (unlikely(ret < 0 &&
				!eca_circular_buffer_space_for_batch(
							&qp_info->cbuf)))
				adapter->stop_enq_to_cryptodev = true;
		}
	}

	return n;
}

static unsigned int
eca_crypto_cdev_flush(struct event_crypto_adapter *adapter,
		      uint8_t cdev_id, uint16_t *nb_ops_flushed)
{
	struct crypto_device_info *curr_dev;
	struct crypto_queue_pair_info *curr_queue;
	struct rte_cryptodev *dev;
	uint16_t nb = 0, nb_enqueued = 0;
	uint16_t qp;

	curr_dev = &adapter->cdevs[cdev_id];
	dev = rte_cryptodev_pmd_get_dev(cdev_id);

	for (qp = 0; qp < dev->data->nb_queue_pairs; qp++) {

		curr_queue = &curr_dev->qpairs[qp];
		if (unlikely(curr_queue == NULL || !curr_queue->qp_enabled))
			continue;

		eca_circular_buffer_flush_to_cdev(&curr_queue->cbuf,
						  cdev_id,
						  qp,
						  &nb_enqueued);
		*nb_ops_flushed += curr_queue->cbuf.count;
		nb += nb_enqueued;
	}

	return nb;
}

static unsigned int
eca_crypto_enq_flush(struct event_crypto_adapter *adapter)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	uint8_t cdev_id;
	uint16_t nb_enqueued = 0;
	uint16_t nb_ops_flushed = 0;
	uint16_t num_cdev = rte_cryptodev_count();

	for (cdev_id = 0; cdev_id < num_cdev; cdev_id++)
		nb_enqueued += eca_crypto_cdev_flush(adapter,
						    cdev_id,
						    &nb_ops_flushed);
	/**
	 * Enable dequeue from eventdev if all ops from circular
	 * buffer flushed to cdev
	 */
	if (!nb_ops_flushed)
		adapter->stop_enq_to_cryptodev = false;

	stats->crypto_enq_count += nb_enqueued;

	return nb_enqueued;
}

static int
eca_crypto_adapter_enq_run(struct event_crypto_adapter *adapter,
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

		if (unlikely(adapter->stop_enq_to_cryptodev)) {
			nb_enqueued += eca_crypto_enq_flush(adapter);

			if (unlikely(adapter->stop_enq_to_cryptodev))
				break;
		}

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

static inline uint16_t
eca_ops_enqueue_burst(struct event_crypto_adapter *adapter,
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

		m_data = rte_cryptodev_session_event_mdata_get(ops[i]);
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

	stats->event_enq_fail_count += nb_ev - nb_enqueued;
	stats->event_enq_count += nb_enqueued;
	stats->event_enq_retry_count += retry - 1;

	return nb_enqueued;
}

static int
eca_circular_buffer_flush_to_evdev(struct event_crypto_adapter *adapter,
				   struct crypto_ops_circular_buffer *bufp)
{
	uint16_t n = 0, nb_ops_flushed;
	uint16_t *headp = &bufp->head;
	uint16_t *tailp = &bufp->tail;
	struct rte_crypto_op **ops = bufp->op_buffer;

	if (*tailp > *headp)
		n = *tailp - *headp;
	else if (*tailp < *headp)
		n = bufp->size - *headp;
	else
		return 0;  /* buffer empty */

	nb_ops_flushed =  eca_ops_enqueue_burst(adapter, &ops[*headp], n);
	bufp->count -= nb_ops_flushed;
	if (!bufp->count) {
		*headp = 0;
		*tailp = 0;
		return 0;  /* buffer empty */
	}

	*headp = (*headp + nb_ops_flushed) % bufp->size;
	return 1;
}


static void
eca_ops_buffer_flush(struct event_crypto_adapter *adapter)
{
	if (likely(adapter->ebuf.count == 0))
		return;

	while (eca_circular_buffer_flush_to_evdev(adapter,
						  &adapter->ebuf))
		;
}
static inline unsigned int
eca_crypto_adapter_deq_run(struct event_crypto_adapter *adapter,
			   unsigned int max_deq)
{
	struct rte_event_crypto_adapter_stats *stats = &adapter->crypto_stats;
	struct crypto_device_info *curr_dev;
	struct crypto_queue_pair_info *curr_queue;
	struct rte_crypto_op *ops[BATCH_SIZE];
	uint16_t n, nb_deq, nb_enqueued, i;
	struct rte_cryptodev *dev;
	uint8_t cdev_id;
	uint16_t qp, dev_qps;
	bool done;
	uint16_t num_cdev = rte_cryptodev_count();

	nb_deq = 0;
	eca_ops_buffer_flush(adapter);

	do {
		done = true;

		for (cdev_id = adapter->next_cdev_id;
			cdev_id < num_cdev; cdev_id++) {
			uint16_t queues = 0;

			curr_dev = &adapter->cdevs[cdev_id];
			dev = curr_dev->dev;
			if (unlikely(dev == NULL))
				continue;

			dev_qps = dev->data->nb_queue_pairs;

			for (qp = curr_dev->next_queue_pair_id;
				queues < dev_qps; qp = (qp + 1) % dev_qps,
				queues++) {

				curr_queue = &curr_dev->qpairs[qp];
				if (unlikely(curr_queue == NULL ||
				    !curr_queue->qp_enabled))
					continue;

				n = rte_cryptodev_dequeue_burst(cdev_id, qp,
					ops, BATCH_SIZE);
				if (!n)
					continue;

				done = false;
				nb_enqueued = 0;

				stats->crypto_deq_count += n;

				if (unlikely(!adapter->ebuf.count))
					nb_enqueued = eca_ops_enqueue_burst(
							adapter, ops, n);

				if (likely(nb_enqueued == n))
					goto check;

				/* Failed to enqueue events case */
				for (i = nb_enqueued; i < n; i++)
					eca_circular_buffer_add(
						&adapter->ebuf,
						ops[i]);

check:
				nb_deq += n;

				if (nb_deq >= max_deq) {
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
		adapter->next_cdev_id = 0;
	} while (done == false);
	return nb_deq;
}

static int
eca_crypto_adapter_run(struct event_crypto_adapter *adapter,
		       unsigned int max_ops)
{
	unsigned int ops_left = max_ops;

	while (ops_left > 0) {
		unsigned int e_cnt, d_cnt;

		e_cnt = eca_crypto_adapter_deq_run(adapter, ops_left);
		ops_left -= RTE_MIN(ops_left, e_cnt);

		d_cnt = eca_crypto_adapter_enq_run(adapter, ops_left);
		ops_left -= RTE_MIN(ops_left, d_cnt);

		if (e_cnt == 0 && d_cnt == 0)
			break;

	}

	if (ops_left == max_ops) {
		rte_event_maintain(adapter->eventdev_id,
				   adapter->event_port_id, 0);
		return -EAGAIN;
	} else
		return 0;
}

static int
eca_service_func(void *args)
{
	struct event_crypto_adapter *adapter = args;
	int ret;

	if (rte_spinlock_trylock(&adapter->lock) == 0)
		return 0;
	ret = eca_crypto_adapter_run(adapter, adapter->max_nb);
	rte_spinlock_unlock(&adapter->lock);

	return ret;
}

static int
eca_init_service(struct event_crypto_adapter *adapter, uint8_t id)
{
	struct rte_event_crypto_adapter_conf adapter_conf;
	struct rte_service_spec service;
	int ret;
	uint32_t impl_rel;

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

	if (rte_event_port_attr_get(adapter->eventdev_id,
				adapter->event_port_id,
				RTE_EVENT_PORT_ATTR_IMPLICIT_RELEASE_DISABLE,
				&impl_rel)) {
		RTE_EDEV_LOG_ERR("Failed to get port info for eventdev %" PRId32,
				 adapter->eventdev_id);
		eca_circular_buffer_free(&adapter->ebuf);
		rte_free(adapter);
		return -EINVAL;
	}

	adapter->implicit_release_disabled = (uint8_t)impl_rel;
	adapter->service_inited = 1;

	return ret;
}

static void
eca_update_qp_info(struct event_crypto_adapter *adapter,
		   struct crypto_device_info *dev_info, int32_t queue_pair_id,
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
eca_add_queue_pair(struct event_crypto_adapter *adapter, uint8_t cdev_id,
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

		if (eca_circular_buffer_init("eca_cdev_circular_buffer",
					     &qpairs->cbuf,
					     CRYPTO_ADAPTER_OPS_BUFFER_SZ)) {
			RTE_EDEV_LOG_ERR("Failed to get memory for cryptodev "
					 "buffer");
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
			const struct rte_event_crypto_adapter_queue_conf *conf)
{
	struct rte_event_crypto_adapter_vector_limits limits;
	struct event_crypto_adapter *adapter;
	struct crypto_device_info *dev_info;
	struct rte_eventdev *dev;
	uint32_t cap;
	int ret;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (!rte_cryptodev_is_valid_dev(cdev_id)) {
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

	if (conf == NULL) {
		if (cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND) {
			RTE_EDEV_LOG_ERR("Conf value can not be NULL for dev_id=%u",
					 cdev_id);
			return -EINVAL;
		}
	} else {
		if (conf->flags & RTE_EVENT_CRYPTO_ADAPTER_EVENT_VECTOR) {
			if ((cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_EVENT_VECTOR) == 0) {
				RTE_EDEV_LOG_ERR("Event vectorization is not supported,"
						 "dev %" PRIu8 " cdev %" PRIu8, id,
						 cdev_id);
				return -ENOTSUP;
			}

			ret = rte_event_crypto_adapter_vector_limits_get(
				adapter->eventdev_id, cdev_id, &limits);
			if (ret < 0) {
				RTE_EDEV_LOG_ERR("Failed to get event device vector "
						 "limits, dev %" PRIu8 " cdev %" PRIu8,
						 id, cdev_id);
				return -EINVAL;
			}

			if (conf->vector_sz < limits.min_sz ||
			    conf->vector_sz > limits.max_sz ||
			    conf->vector_timeout_ns < limits.min_timeout_ns ||
			    conf->vector_timeout_ns > limits.max_timeout_ns ||
			    conf->vector_mp == NULL) {
				RTE_EDEV_LOG_ERR("Invalid event vector configuration,"
						" dev %" PRIu8 " cdev %" PRIu8,
						id, cdev_id);
				return -EINVAL;
			}

			if (conf->vector_mp->elt_size < (sizeof(struct rte_event_vector) +
			    (sizeof(uintptr_t) * conf->vector_sz))) {
				RTE_EDEV_LOG_ERR("Invalid event vector configuration,"
						" dev %" PRIu8 " cdev %" PRIu8,
						id, cdev_id);
				return -EINVAL;
			}
		}
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
		if (*dev->dev_ops->crypto_adapter_queue_pair_add == NULL)
			return -ENOTSUP;
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
				conf);
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
	     !(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
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

	rte_eventdev_trace_crypto_adapter_queue_pair_add(id, cdev_id,
		queue_pair_id, conf);
	return 0;
}

int
rte_event_crypto_adapter_queue_pair_del(uint8_t id, uint8_t cdev_id,
					int32_t queue_pair_id)
{
	struct event_crypto_adapter *adapter;
	struct crypto_device_info *dev_info;
	struct rte_eventdev *dev;
	int ret;
	uint32_t cap;
	uint16_t i;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (!rte_cryptodev_is_valid_dev(cdev_id)) {
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
		if (*dev->dev_ops->crypto_adapter_queue_pair_del == NULL)
			return -ENOTSUP;
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

	rte_eventdev_trace_crypto_adapter_queue_pair_del(id, cdev_id,
		queue_pair_id, ret);
	return ret;
}

static int
eca_adapter_ctrl(uint8_t id, int start)
{
	struct event_crypto_adapter *adapter;
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
	struct event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	rte_eventdev_trace_crypto_adapter_start(id, adapter);
	return eca_adapter_ctrl(id, 1);
}

int
rte_event_crypto_adapter_stop(uint8_t id)
{
	rte_eventdev_trace_crypto_adapter_stop(id);
	return eca_adapter_ctrl(id, 0);
}

int
rte_event_crypto_adapter_stats_get(uint8_t id,
				struct rte_event_crypto_adapter_stats *stats)
{
	struct event_crypto_adapter *adapter;
	struct rte_event_crypto_adapter_stats dev_stats_sum = { 0 };
	struct rte_event_crypto_adapter_stats dev_stats;
	struct rte_eventdev *dev;
	struct crypto_device_info *dev_info;
	uint32_t i;
	int ret;

	if (eca_memzone_lookup())
		return -ENOMEM;

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

	rte_eventdev_trace_crypto_adapter_stats_get(id, stats,
		stats->event_poll_count, stats->event_deq_count,
		stats->crypto_enq_count, stats->crypto_enq_fail,
		stats->crypto_deq_count, stats->event_enq_count,
		stats->event_enq_retry_count, stats->event_enq_fail_count);

	return 0;
}

int
rte_event_crypto_adapter_stats_reset(uint8_t id)
{
	struct event_crypto_adapter *adapter;
	struct crypto_device_info *dev_info;
	struct rte_eventdev *dev;
	uint32_t i;

	rte_eventdev_trace_crypto_adapter_stats_reset(id);

	if (eca_memzone_lookup())
		return -ENOMEM;

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
rte_event_crypto_adapter_runtime_params_init(
		struct rte_event_crypto_adapter_runtime_params *params)
{
	if (params == NULL)
		return -EINVAL;

	memset(params, 0, sizeof(*params));
	params->max_nb = DEFAULT_MAX_NB;

	return 0;
}

static int
crypto_adapter_cap_check(struct event_crypto_adapter *adapter)
{
	int ret;
	uint32_t caps;

	if (!adapter->nb_qps)
		return -EINVAL;
	ret = rte_event_crypto_adapter_caps_get(adapter->eventdev_id,
						adapter->next_cdev_id,
						&caps);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps dev %" PRIu8
			" cdev %" PRIu8, adapter->eventdev_id,
			adapter->next_cdev_id);
		return ret;
	}

	if ((caps & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) ||
	    (caps & RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		return -ENOTSUP;

	return 0;
}

int
rte_event_crypto_adapter_runtime_params_set(uint8_t id,
		struct rte_event_crypto_adapter_runtime_params *params)
{
	struct event_crypto_adapter *adapter;
	int ret;

	if (eca_memzone_lookup())
		return -ENOMEM;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (params == NULL) {
		RTE_EDEV_LOG_ERR("params pointer is NULL\n");
		return -EINVAL;
	}

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	ret = crypto_adapter_cap_check(adapter);
	if (ret)
		return ret;

	rte_spinlock_lock(&adapter->lock);
	adapter->max_nb = params->max_nb;
	rte_spinlock_unlock(&adapter->lock);

	return 0;
}

int
rte_event_crypto_adapter_runtime_params_get(uint8_t id,
		struct rte_event_crypto_adapter_runtime_params *params)
{
	struct event_crypto_adapter *adapter;
	int ret;

	if (eca_memzone_lookup())
		return -ENOMEM;


	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (params == NULL) {
		RTE_EDEV_LOG_ERR("params pointer is NULL\n");
		return -EINVAL;
	}

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	ret = crypto_adapter_cap_check(adapter);
	if (ret)
		return ret;

	params->max_nb = adapter->max_nb;

	return 0;
}

int
rte_event_crypto_adapter_service_id_get(uint8_t id, uint32_t *service_id)
{
	struct event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL || service_id == NULL)
		return -EINVAL;

	if (adapter->service_inited)
		*service_id = adapter->service_id;

	rte_eventdev_trace_crypto_adapter_service_id_get(id, *service_id);

	return adapter->service_inited ? 0 : -ESRCH;
}

int
rte_event_crypto_adapter_event_port_get(uint8_t id, uint8_t *event_port_id)
{
	struct event_crypto_adapter *adapter;

	EVENT_CRYPTO_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = eca_id_to_adapter(id);
	if (adapter == NULL || event_port_id == NULL)
		return -EINVAL;

	*event_port_id = adapter->event_port_id;

	rte_eventdev_trace_crypto_adapter_event_port_get(id, *event_port_id);

	return 0;
}

int
rte_event_crypto_adapter_vector_limits_get(
	uint8_t dev_id, uint16_t cdev_id,
	struct rte_event_crypto_adapter_vector_limits *limits)
{
	struct rte_cryptodev *cdev;
	struct rte_eventdev *dev;
	uint32_t cap;
	int ret;

	rte_eventdev_trace_crypto_adapter_vector_limits_get(dev_id, cdev_id, limits);

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	if (!rte_cryptodev_is_valid_dev(cdev_id)) {
		RTE_EDEV_LOG_ERR("Invalid dev_id=%" PRIu8, cdev_id);
		return -EINVAL;
	}

	if (limits == NULL) {
		RTE_EDEV_LOG_ERR("Invalid limits storage provided");
		return -EINVAL;
	}

	dev = &rte_eventdevs[dev_id];
	cdev = rte_cryptodev_pmd_get_dev(cdev_id);

	ret = rte_event_crypto_adapter_caps_get(dev_id, cdev_id, &cap);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps edev %" PRIu8
				 "cdev %" PRIu16, dev_id, cdev_id);
		return ret;
	}

	if (!(cap & RTE_EVENT_CRYPTO_ADAPTER_CAP_EVENT_VECTOR)) {
		RTE_EDEV_LOG_ERR("Event vectorization is not supported,"
				 "dev %" PRIu8 " cdev %" PRIu8, dev_id, cdev_id);
		return -ENOTSUP;
	}

	if ((*dev->dev_ops->crypto_adapter_vector_limits_get) == NULL)
		return -ENOTSUP;

	return dev->dev_ops->crypto_adapter_vector_limits_get(
		dev, cdev, limits);
}
