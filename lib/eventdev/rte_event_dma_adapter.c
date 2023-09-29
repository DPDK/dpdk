/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <eventdev_pmd.h>
#include <rte_service_component.h>

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

	/* Next queue pair to be processed */
	uint16_t next_vchan_id;

	/* Set to indicate processing has been started */
	uint8_t dev_started;

	/* Set to indicate dmadev->eventdev packet
	 * transfer uses a hardware mechanism
	 */
	uint8_t internal_event_port;
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

	/* Next dma device to be processed */
	uint16_t next_dmadev_id;

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

	/* Per adapter EAL service ID */
	uint32_t service_id;

	/* Service initialization state */
	uint8_t service_initialized;

	/* Max DMA ops processed in any service function invocation */
	uint32_t max_nb;

	/* Store event port's implicit release capability */
	uint8_t implicit_release_disabled;

	/* Flag to indicate backpressure at dma_dev
	 * Stop further dequeuing events from eventdev
	 */
	bool stop_enq_to_dma_dev;

	/* Loop counter to flush dma ops */
	uint16_t transmit_loop_count;

	/* Per instance stats structure */
	struct rte_event_dma_adapter_stats dma_stats;
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

static inline bool
edma_circular_buffer_batch_ready(struct dma_ops_circular_buffer *bufp)
{
	return bufp->count >= DMA_BATCH_SIZE;
}

static inline bool
edma_circular_buffer_space_for_batch(struct dma_ops_circular_buffer *bufp)
{
	return (bufp->size - bufp->count) >= DMA_BATCH_SIZE;
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

static inline int
edma_circular_buffer_add(struct dma_ops_circular_buffer *bufp, struct rte_event_dma_adapter_op *op)
{
	uint16_t *tail = &bufp->tail;

	bufp->op_buffer[*tail] = op;

	/* circular buffer, go round */
	*tail = (*tail + 1) % bufp->size;
	bufp->count++;

	return 0;
}

static inline int
edma_circular_buffer_flush_to_dma_dev(struct event_dma_adapter *adapter,
				      struct dma_ops_circular_buffer *bufp, uint8_t dma_dev_id,
				      uint16_t vchan, uint16_t *nb_ops_flushed)
{
	struct rte_event_dma_adapter_op *op;
	struct dma_vchan_info *tq;
	uint16_t *head = &bufp->head;
	uint16_t *tail = &bufp->tail;
	uint16_t n;
	uint16_t i;
	int ret;

	if (*tail > *head)
		n = *tail - *head;
	else if (*tail < *head)
		n = bufp->size - *head;
	else {
		*nb_ops_flushed = 0;
		return 0; /* buffer empty */
	}

	tq = &adapter->dma_devs[dma_dev_id].tqmap[vchan];

	for (i = 0; i < n; i++)	{
		op = bufp->op_buffer[*head];
		if (op->nb_src == 1 && op->nb_dst == 1)
			ret = rte_dma_copy(dma_dev_id, vchan, op->src_seg->addr, op->dst_seg->addr,
					   op->src_seg->length, op->flags);
		else
			ret = rte_dma_copy_sg(dma_dev_id, vchan, op->src_seg, op->dst_seg,
					      op->nb_src, op->nb_dst, op->flags);
		if (ret < 0)
			break;

		/* Enqueue in transaction queue. */
		edma_circular_buffer_add(&tq->dma_buf, op);

		*head = (*head + 1) % bufp->size;
	}

	*nb_ops_flushed = i;
	bufp->count -= *nb_ops_flushed;
	if (!bufp->count) {
		*head = 0;
		*tail = 0;
	}

	return *nb_ops_flushed == n ? 0 : -1;
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

int
rte_event_dma_adapter_event_port_get(uint8_t id, uint8_t *event_port_id)
{
	struct event_dma_adapter *adapter;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL || event_port_id == NULL)
		return -EINVAL;

	*event_port_id = adapter->event_port_id;

	return 0;
}

static inline unsigned int
edma_enq_to_dma_dev(struct event_dma_adapter *adapter, struct rte_event *ev, unsigned int cnt)
{
	struct rte_event_dma_adapter_stats *stats = &adapter->dma_stats;
	struct dma_vchan_info *vchan_qinfo = NULL;
	struct rte_event_dma_adapter_op *dma_op;
	uint16_t vchan, nb_enqueued = 0;
	int16_t dma_dev_id;
	unsigned int i, n;
	int ret;

	ret = 0;
	n = 0;
	stats->event_deq_count += cnt;

	for (i = 0; i < cnt; i++) {
		dma_op = ev[i].event_ptr;
		if (dma_op == NULL)
			continue;

		/* Expected to have response info appended to dma_op. */

		dma_dev_id = dma_op->dma_dev_id;
		vchan = dma_op->vchan;
		vchan_qinfo = &adapter->dma_devs[dma_dev_id].vchanq[vchan];
		if (!vchan_qinfo->vq_enabled) {
			if (dma_op != NULL && dma_op->op_mp != NULL)
				rte_mempool_put(dma_op->op_mp, dma_op);
			continue;
		}
		edma_circular_buffer_add(&vchan_qinfo->dma_buf, dma_op);

		if (edma_circular_buffer_batch_ready(&vchan_qinfo->dma_buf)) {
			ret = edma_circular_buffer_flush_to_dma_dev(adapter, &vchan_qinfo->dma_buf,
								    dma_dev_id, vchan,
								    &nb_enqueued);
			stats->dma_enq_count += nb_enqueued;
			n += nb_enqueued;

			/**
			 * If some dma ops failed to flush to dma_dev and
			 * space for another batch is not available, stop
			 * dequeue from eventdev momentarily
			 */
			if (unlikely(ret < 0 &&
				     !edma_circular_buffer_space_for_batch(&vchan_qinfo->dma_buf)))
				adapter->stop_enq_to_dma_dev = true;
		}
	}

	return n;
}

static unsigned int
edma_adapter_dev_flush(struct event_dma_adapter *adapter, int16_t dma_dev_id,
		       uint16_t *nb_ops_flushed)
{
	struct dma_vchan_info *vchan_info;
	struct dma_device_info *dev_info;
	uint16_t nb = 0, nb_enqueued = 0;
	uint16_t vchan, nb_vchans;

	dev_info = &adapter->dma_devs[dma_dev_id];
	nb_vchans = dev_info->num_vchanq;

	for (vchan = 0; vchan < nb_vchans; vchan++) {

		vchan_info = &dev_info->vchanq[vchan];
		if (unlikely(vchan_info == NULL || !vchan_info->vq_enabled))
			continue;

		edma_circular_buffer_flush_to_dma_dev(adapter, &vchan_info->dma_buf, dma_dev_id,
						      vchan, &nb_enqueued);
		*nb_ops_flushed += vchan_info->dma_buf.count;
		nb += nb_enqueued;
	}

	return nb;
}

static unsigned int
edma_adapter_enq_flush(struct event_dma_adapter *adapter)
{
	struct rte_event_dma_adapter_stats *stats = &adapter->dma_stats;
	int16_t dma_dev_id;
	uint16_t nb_enqueued = 0;
	uint16_t nb_ops_flushed = 0;
	uint16_t num_dma_dev = rte_dma_count_avail();

	for (dma_dev_id = 0; dma_dev_id < num_dma_dev; dma_dev_id++)
		nb_enqueued += edma_adapter_dev_flush(adapter, dma_dev_id, &nb_ops_flushed);
	/**
	 * Enable dequeue from eventdev if all ops from circular
	 * buffer flushed to dma_dev
	 */
	if (!nb_ops_flushed)
		adapter->stop_enq_to_dma_dev = false;

	stats->dma_enq_count += nb_enqueued;

	return nb_enqueued;
}

/* Flush an instance's enqueue buffers every DMA_ENQ_FLUSH_THRESHOLD
 * iterations of edma_adapter_enq_run()
 */
#define DMA_ENQ_FLUSH_THRESHOLD 1024

static int
edma_adapter_enq_run(struct event_dma_adapter *adapter, unsigned int max_enq)
{
	struct rte_event_dma_adapter_stats *stats = &adapter->dma_stats;
	uint8_t event_port_id = adapter->event_port_id;
	uint8_t event_dev_id = adapter->eventdev_id;
	struct rte_event ev[DMA_BATCH_SIZE];
	unsigned int nb_enq, nb_enqueued;
	uint16_t n;

	if (adapter->mode == RTE_EVENT_DMA_ADAPTER_OP_NEW)
		return 0;

	nb_enqueued = 0;
	for (nb_enq = 0; nb_enq < max_enq; nb_enq += n) {

		if (unlikely(adapter->stop_enq_to_dma_dev)) {
			nb_enqueued += edma_adapter_enq_flush(adapter);

			if (unlikely(adapter->stop_enq_to_dma_dev))
				break;
		}

		stats->event_poll_count++;
		n = rte_event_dequeue_burst(event_dev_id, event_port_id, ev, DMA_BATCH_SIZE, 0);

		if (!n)
			break;

		nb_enqueued += edma_enq_to_dma_dev(adapter, ev, n);
	}

	if ((++adapter->transmit_loop_count & (DMA_ENQ_FLUSH_THRESHOLD - 1)) == 0)
		nb_enqueued += edma_adapter_enq_flush(adapter);

	return nb_enqueued;
}

#define DMA_ADAPTER_MAX_EV_ENQ_RETRIES 100

static inline uint16_t
edma_ops_enqueue_burst(struct event_dma_adapter *adapter, struct rte_event_dma_adapter_op **ops,
		       uint16_t num)
{
	struct rte_event_dma_adapter_stats *stats = &adapter->dma_stats;
	uint8_t event_port_id = adapter->event_port_id;
	uint8_t event_dev_id = adapter->eventdev_id;
	struct rte_event events[DMA_BATCH_SIZE];
	struct rte_event *response_info;
	uint16_t nb_enqueued, nb_ev;
	uint8_t retry;
	uint8_t i;

	nb_ev = 0;
	retry = 0;
	nb_enqueued = 0;
	num = RTE_MIN(num, DMA_BATCH_SIZE);
	for (i = 0; i < num; i++) {
		struct rte_event *ev = &events[nb_ev++];

		/* Expected to have response info appended to dma_op. */
		response_info = (struct rte_event *)((uint8_t *)ops[i] +
							  sizeof(struct rte_event_dma_adapter_op));
		if (unlikely(response_info == NULL)) {
			if (ops[i] != NULL && ops[i]->op_mp != NULL)
				rte_mempool_put(ops[i]->op_mp, ops[i]);
			continue;
		}

		rte_memcpy(ev, response_info, sizeof(struct rte_event));
		ev->event_ptr = ops[i];
		ev->event_type = RTE_EVENT_TYPE_DMADEV;
		if (adapter->implicit_release_disabled)
			ev->op = RTE_EVENT_OP_FORWARD;
		else
			ev->op = RTE_EVENT_OP_NEW;
	}

	do {
		nb_enqueued += rte_event_enqueue_burst(event_dev_id, event_port_id,
						       &events[nb_enqueued], nb_ev - nb_enqueued);

	} while (retry++ < DMA_ADAPTER_MAX_EV_ENQ_RETRIES && nb_enqueued < nb_ev);

	stats->event_enq_fail_count += nb_ev - nb_enqueued;
	stats->event_enq_count += nb_enqueued;
	stats->event_enq_retry_count += retry - 1;

	return nb_enqueued;
}

static int
edma_circular_buffer_flush_to_evdev(struct event_dma_adapter *adapter,
				    struct dma_ops_circular_buffer *bufp,
				    uint16_t *enqueue_count)
{
	struct rte_event_dma_adapter_op **ops = bufp->op_buffer;
	uint16_t n = 0, nb_ops_flushed;
	uint16_t *head = &bufp->head;
	uint16_t *tail = &bufp->tail;

	if (*tail > *head)
		n = *tail - *head;
	else if (*tail < *head)
		n = bufp->size - *head;
	else {
		if (enqueue_count)
			*enqueue_count = 0;
		return 0; /* buffer empty */
	}

	if (enqueue_count && n > *enqueue_count)
		n = *enqueue_count;

	nb_ops_flushed = edma_ops_enqueue_burst(adapter, &ops[*head], n);
	if (enqueue_count)
		*enqueue_count = nb_ops_flushed;

	bufp->count -= nb_ops_flushed;
	if (!bufp->count) {
		*head = 0;
		*tail = 0;
		return 0; /* buffer empty */
	}

	*head = (*head + nb_ops_flushed) % bufp->size;
	return 1;
}

static void
edma_ops_buffer_flush(struct event_dma_adapter *adapter)
{
	if (likely(adapter->ebuf.count == 0))
		return;

	while (edma_circular_buffer_flush_to_evdev(adapter, &adapter->ebuf, NULL))
		;
}

static inline unsigned int
edma_adapter_deq_run(struct event_dma_adapter *adapter, unsigned int max_deq)
{
	struct rte_event_dma_adapter_stats *stats = &adapter->dma_stats;
	struct dma_vchan_info *vchan_info;
	struct dma_ops_circular_buffer *tq_buf;
	struct rte_event_dma_adapter_op *ops;
	uint16_t n, nb_deq, nb_enqueued, i;
	struct dma_device_info *dev_info;
	uint16_t vchan, num_vchan;
	uint16_t num_dma_dev;
	int16_t dma_dev_id;
	uint16_t index;
	bool done;
	bool err;

	nb_deq = 0;
	edma_ops_buffer_flush(adapter);

	num_dma_dev = rte_dma_count_avail();
	do {
		done = true;

		for (dma_dev_id = adapter->next_dmadev_id; dma_dev_id < num_dma_dev; dma_dev_id++) {
			uint16_t queues = 0;
			dev_info = &adapter->dma_devs[dma_dev_id];
			num_vchan = dev_info->num_vchanq;

			for (vchan = dev_info->next_vchan_id; queues < num_vchan;
			     vchan = (vchan + 1) % num_vchan, queues++) {

				vchan_info = &dev_info->vchanq[vchan];
				if (unlikely(vchan_info == NULL || !vchan_info->vq_enabled))
					continue;

				n = rte_dma_completed(dma_dev_id, vchan, DMA_BATCH_SIZE,
						&index, &err);
				if (!n)
					continue;

				done = false;
				stats->dma_deq_count += n;

				tq_buf = &dev_info->tqmap[vchan].dma_buf;

				nb_enqueued = n;
				if (unlikely(!adapter->ebuf.count))
					edma_circular_buffer_flush_to_evdev(adapter, tq_buf,
									    &nb_enqueued);

				if (likely(nb_enqueued == n))
					goto check;

				/* Failed to enqueue events case */
				for (i = nb_enqueued; i < n; i++) {
					ops = tq_buf->op_buffer[tq_buf->head];
					edma_circular_buffer_add(&adapter->ebuf, ops);
					tq_buf->head = (tq_buf->head + 1) % tq_buf->size;
				}

check:
				nb_deq += n;
				if (nb_deq >= max_deq) {
					if ((vchan + 1) == num_vchan)
						adapter->next_dmadev_id =
								(dma_dev_id + 1) % num_dma_dev;

					dev_info->next_vchan_id = (vchan + 1) % num_vchan;

					return nb_deq;
				}
			}
		}
		adapter->next_dmadev_id = 0;

	} while (done == false);

	return nb_deq;
}

static int
edma_adapter_run(struct event_dma_adapter *adapter, unsigned int max_ops)
{
	unsigned int ops_left = max_ops;

	while (ops_left > 0) {
		unsigned int e_cnt, d_cnt;

		e_cnt = edma_adapter_deq_run(adapter, ops_left);
		ops_left -= RTE_MIN(ops_left, e_cnt);

		d_cnt = edma_adapter_enq_run(adapter, ops_left);
		ops_left -= RTE_MIN(ops_left, d_cnt);

		if (e_cnt == 0 && d_cnt == 0)
			break;
	}

	if (ops_left == max_ops) {
		rte_event_maintain(adapter->eventdev_id, adapter->event_port_id, 0);
		return -EAGAIN;
	} else
		return 0;
}

static int
edma_service_func(void *args)
{
	struct event_dma_adapter *adapter = args;
	int ret;

	if (rte_spinlock_trylock(&adapter->lock) == 0)
		return 0;
	ret = edma_adapter_run(adapter, adapter->max_nb);
	rte_spinlock_unlock(&adapter->lock);

	return ret;
}

static int
edma_init_service(struct event_dma_adapter *adapter, uint8_t id)
{
	struct rte_event_dma_adapter_conf adapter_conf;
	struct rte_service_spec service;
	uint32_t impl_rel;
	int ret;

	if (adapter->service_initialized)
		return 0;

	memset(&service, 0, sizeof(service));
	snprintf(service.name, DMA_ADAPTER_NAME_LEN, "rte_event_dma_adapter_%d", id);
	service.socket_id = adapter->socket_id;
	service.callback = edma_service_func;
	service.callback_userdata = adapter;

	/* Service function handles locking for queue add/del updates */
	service.capabilities = RTE_SERVICE_CAP_MT_SAFE;
	ret = rte_service_component_register(&service, &adapter->service_id);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to register service %s err = %" PRId32, service.name, ret);
		return ret;
	}

	ret = adapter->conf_cb(id, adapter->eventdev_id, &adapter_conf, adapter->conf_arg);
	if (ret) {
		RTE_EDEV_LOG_ERR("configuration callback failed err = %" PRId32, ret);
		return ret;
	}

	adapter->max_nb = adapter_conf.max_nb;
	adapter->event_port_id = adapter_conf.event_port_id;

	if (rte_event_port_attr_get(adapter->eventdev_id, adapter->event_port_id,
				    RTE_EVENT_PORT_ATTR_IMPLICIT_RELEASE_DISABLE, &impl_rel)) {
		RTE_EDEV_LOG_ERR("Failed to get port info for eventdev %" PRId32,
				 adapter->eventdev_id);
		edma_circular_buffer_free(&adapter->ebuf);
		rte_free(adapter);
		return -EINVAL;
	}

	adapter->implicit_release_disabled = (uint8_t)impl_rel;
	adapter->service_initialized = 1;

	return ret;
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

static int
edma_add_vchan(struct event_dma_adapter *adapter, int16_t dma_dev_id, uint16_t vchan)
{
	struct dma_device_info *dev_info = &adapter->dma_devs[dma_dev_id];
	struct dma_vchan_info *vchanq;
	struct dma_vchan_info *tqmap;
	uint16_t nb_vchans;
	uint32_t i;

	if (dev_info->vchanq == NULL) {
		nb_vchans = dev_info->num_dma_dev_vchan;

		dev_info->vchanq = rte_zmalloc_socket(adapter->mem_name,
				nb_vchans * sizeof(struct dma_vchan_info),
				0, adapter->socket_id);
		if (dev_info->vchanq == NULL)
			return -ENOMEM;

		dev_info->tqmap = rte_zmalloc_socket(adapter->mem_name,
				nb_vchans * sizeof(struct dma_vchan_info),
				0, adapter->socket_id);
		if (dev_info->tqmap == NULL)
			return -ENOMEM;

		for (i = 0; i < nb_vchans; i++) {
			vchanq = &dev_info->vchanq[i];

			if (edma_circular_buffer_init("dma_dev_circular_buffer", &vchanq->dma_buf,
						DMA_ADAPTER_OPS_BUFFER_SIZE)) {
				RTE_EDEV_LOG_ERR("Failed to get memory for dma_dev buffer");
				rte_free(vchanq);
				return -ENOMEM;
			}

			tqmap = &dev_info->tqmap[i];
			if (edma_circular_buffer_init("dma_dev_circular_trans_buf", &tqmap->dma_buf,
						DMA_ADAPTER_OPS_BUFFER_SIZE)) {
				RTE_EDEV_LOG_ERR(
					"Failed to get memory for dma_dev transaction buffer");
				rte_free(tqmap);
				return -ENOMEM;
			}
		}
	}

	if (vchan == RTE_DMA_ALL_VCHAN) {
		for (i = 0; i < dev_info->num_dma_dev_vchan; i++)
			edma_update_vchanq_info(adapter, dev_info, i, 1);
	} else
		edma_update_vchanq_info(adapter, dev_info, vchan, 1);

	return 0;
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

	/* In case HW cap is RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW, or SW adapter, initiate
	 * services so the application can choose which ever way it wants to use the adapter.
	 *
	 * Case 1: RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW. Application may wants to use one
	 * of below two modes
	 *
	 * a. OP_FORWARD mode -> HW Dequeue + SW enqueue
	 * b. OP_NEW mode -> HW Dequeue
	 *
	 * Case 2: No HW caps, use SW adapter
	 *
	 * a. OP_FORWARD mode -> SW enqueue & dequeue
	 * b. OP_NEW mode -> SW Dequeue
	 */
	if ((cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW &&
	     !(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	     adapter->mode == RTE_EVENT_DMA_ADAPTER_OP_FORWARD) ||
	    (!(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW) &&
	     !(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) &&
	     !(cap & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND))) {
		rte_spinlock_lock(&adapter->lock);
		ret = edma_init_service(adapter, id);
		if (ret == 0)
			ret = edma_add_vchan(adapter, dma_dev_id, vchan);
		rte_spinlock_unlock(&adapter->lock);

		if (ret)
			return ret;

		rte_service_component_runstate_set(adapter->service_id, 1);
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
		rte_service_component_runstate_set(adapter->service_id, adapter->nb_vchanq);
	}

	return ret;
}

int
rte_event_dma_adapter_service_id_get(uint8_t id, uint32_t *service_id)
{
	struct event_dma_adapter *adapter;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL || service_id == NULL)
		return -EINVAL;

	if (adapter->service_initialized)
		*service_id = adapter->service_id;

	return adapter->service_initialized ? 0 : -ESRCH;
}

static int
edma_adapter_ctrl(uint8_t id, int start)
{
	struct event_dma_adapter *adapter;
	struct dma_device_info *dev_info;
	struct rte_eventdev *dev;
	uint16_t num_dma_dev;
	int stop = !start;
	int use_service;
	uint32_t i;

	use_service = 0;
	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	num_dma_dev = rte_dma_count_avail();
	dev = &rte_eventdevs[adapter->eventdev_id];

	for (i = 0; i < num_dma_dev; i++) {
		dev_info = &adapter->dma_devs[i];
		/* start check for num queue pairs */
		if (start && !dev_info->num_vchanq)
			continue;
		/* stop check if dev has been started */
		if (stop && !dev_info->dev_started)
			continue;
		use_service |= !dev_info->internal_event_port;
		dev_info->dev_started = start;
		if (dev_info->internal_event_port == 0)
			continue;
		start ? (*dev->dev_ops->dma_adapter_start)(dev, i) :
			(*dev->dev_ops->dma_adapter_stop)(dev, i);
	}

	if (use_service)
		rte_service_runstate_set(adapter->service_id, start);

	return 0;
}

int
rte_event_dma_adapter_start(uint8_t id)
{
	struct event_dma_adapter *adapter;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	return edma_adapter_ctrl(id, 1);
}

int
rte_event_dma_adapter_stop(uint8_t id)
{
	return edma_adapter_ctrl(id, 0);
}

#define DEFAULT_MAX_NB 128

int
rte_event_dma_adapter_runtime_params_init(struct rte_event_dma_adapter_runtime_params *params)
{
	if (params == NULL)
		return -EINVAL;

	memset(params, 0, sizeof(*params));
	params->max_nb = DEFAULT_MAX_NB;

	return 0;
}

static int
dma_adapter_cap_check(struct event_dma_adapter *adapter)
{
	uint32_t caps;
	int ret;

	if (!adapter->nb_vchanq)
		return -EINVAL;

	ret = rte_event_dma_adapter_caps_get(adapter->eventdev_id, adapter->next_dmadev_id, &caps);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps dev %" PRIu8 " cdev %" PRIu8,
				 adapter->eventdev_id, adapter->next_dmadev_id);
		return ret;
	}

	if ((caps & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD) ||
	    (caps & RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW))
		return -ENOTSUP;

	return 0;
}

int
rte_event_dma_adapter_runtime_params_set(uint8_t id,
					 struct rte_event_dma_adapter_runtime_params *params)
{
	struct event_dma_adapter *adapter;
	int ret;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (params == NULL) {
		RTE_EDEV_LOG_ERR("params pointer is NULL\n");
		return -EINVAL;
	}

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	ret = dma_adapter_cap_check(adapter);
	if (ret)
		return ret;

	rte_spinlock_lock(&adapter->lock);
	adapter->max_nb = params->max_nb;
	rte_spinlock_unlock(&adapter->lock);

	return 0;
}

int
rte_event_dma_adapter_runtime_params_get(uint8_t id,
					 struct rte_event_dma_adapter_runtime_params *params)
{
	struct event_dma_adapter *adapter;
	int ret;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	if (params == NULL) {
		RTE_EDEV_LOG_ERR("params pointer is NULL\n");
		return -EINVAL;
	}

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	ret = dma_adapter_cap_check(adapter);
	if (ret)
		return ret;

	params->max_nb = adapter->max_nb;

	return 0;
}

int
rte_event_dma_adapter_stats_get(uint8_t id, struct rte_event_dma_adapter_stats *stats)
{
	struct rte_event_dma_adapter_stats dev_stats_sum = {0};
	struct rte_event_dma_adapter_stats dev_stats;
	struct event_dma_adapter *adapter;
	struct dma_device_info *dev_info;
	struct rte_eventdev *dev;
	uint16_t num_dma_dev;
	uint32_t i;
	int ret;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL || stats == NULL)
		return -EINVAL;

	num_dma_dev = rte_dma_count_avail();
	dev = &rte_eventdevs[adapter->eventdev_id];
	memset(stats, 0, sizeof(*stats));
	for (i = 0; i < num_dma_dev; i++) {
		dev_info = &adapter->dma_devs[i];

		if (dev_info->internal_event_port == 0 ||
		    dev->dev_ops->dma_adapter_stats_get == NULL)
			continue;

		ret = (*dev->dev_ops->dma_adapter_stats_get)(dev, i, &dev_stats);
		if (ret)
			continue;

		dev_stats_sum.dma_deq_count += dev_stats.dma_deq_count;
		dev_stats_sum.event_enq_count += dev_stats.event_enq_count;
	}

	if (adapter->service_initialized)
		*stats = adapter->dma_stats;

	stats->dma_deq_count += dev_stats_sum.dma_deq_count;
	stats->event_enq_count += dev_stats_sum.event_enq_count;

	return 0;
}

int
rte_event_dma_adapter_stats_reset(uint8_t id)
{
	struct event_dma_adapter *adapter;
	struct dma_device_info *dev_info;
	struct rte_eventdev *dev;
	uint16_t num_dma_dev;
	uint32_t i;

	EVENT_DMA_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	adapter = edma_id_to_adapter(id);
	if (adapter == NULL)
		return -EINVAL;

	num_dma_dev = rte_dma_count_avail();
	dev = &rte_eventdevs[adapter->eventdev_id];
	for (i = 0; i < num_dma_dev; i++) {
		dev_info = &adapter->dma_devs[i];

		if (dev_info->internal_event_port == 0 ||
		    dev->dev_ops->dma_adapter_stats_reset == NULL)
			continue;

		(*dev->dev_ops->dma_adapter_stats_reset)(dev, i);
	}

	memset(&adapter->dma_stats, 0, sizeof(adapter->dma_stats));

	return 0;
}

uint16_t
rte_event_dma_adapter_enqueue(uint8_t dev_id, uint8_t port_id, struct rte_event ev[],
			      uint16_t nb_events)
{
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];

	return fp_ops->dma_enqueue(port, ev, nb_events);
}
