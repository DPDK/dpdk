/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation.
 * All rights reserved.
 */
#include <rte_cycles.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_service_component.h>
#include <rte_thash.h>

#include "rte_eventdev.h"
#include "rte_eventdev_pmd.h"
#include "rte_event_eth_rx_adapter.h"

#define BATCH_SIZE		32
#define BLOCK_CNT_THRESHOLD	10
#define ETH_EVENT_BUFFER_SIZE	(4*BATCH_SIZE)

#define ETH_RX_ADAPTER_SERVICE_NAME_LEN	32
#define ETH_RX_ADAPTER_MEM_NAME_LEN	32

#define RSS_KEY_SIZE	40

/*
 * There is an instance of this struct per polled Rx queue added to the
 * adapter
 */
struct eth_rx_poll_entry {
	/* Eth port to poll */
	uint16_t eth_dev_id;
	/* Eth rx queue to poll */
	uint16_t eth_rx_qid;
};

/* Instance per adapter */
struct rte_eth_event_enqueue_buffer {
	/* Count of events in this buffer */
	uint16_t count;
	/* Array of events in this buffer */
	struct rte_event events[ETH_EVENT_BUFFER_SIZE];
};

struct rte_event_eth_rx_adapter {
	/* RSS key */
	uint8_t rss_key_be[RSS_KEY_SIZE];
	/* Event device identifier */
	uint8_t eventdev_id;
	/* Per ethernet device structure */
	struct eth_device_info *eth_devices;
	/* Event port identifier */
	uint8_t event_port_id;
	/* Lock to serialize config updates with service function */
	rte_spinlock_t rx_lock;
	/* Max mbufs processed in any service function invocation */
	uint32_t max_nb_rx;
	/* Receive queues that need to be polled */
	struct eth_rx_poll_entry *eth_rx_poll;
	/* Size of the eth_rx_poll array */
	uint16_t num_rx_polled;
	/* Weighted round robin schedule */
	uint32_t *wrr_sched;
	/* wrr_sched[] size */
	uint32_t wrr_len;
	/* Next entry in wrr[] to begin polling */
	uint32_t wrr_pos;
	/* Event burst buffer */
	struct rte_eth_event_enqueue_buffer event_enqueue_buffer;
	/* Per adapter stats */
	struct rte_event_eth_rx_adapter_stats stats;
	/* Block count, counts up to BLOCK_CNT_THRESHOLD */
	uint16_t enq_block_count;
	/* Block start ts */
	uint64_t rx_enq_block_start_ts;
	/* Configuration callback for rte_service configuration */
	rte_event_eth_rx_adapter_conf_cb conf_cb;
	/* Configuration callback argument */
	void *conf_arg;
	/* Set if  default_cb is being used */
	int default_cb_arg;
	/* Service initialization state */
	uint8_t service_inited;
	/* Total count of Rx queues in adapter */
	uint32_t nb_queues;
	/* Memory allocation name */
	char mem_name[ETH_RX_ADAPTER_MEM_NAME_LEN];
	/* Socket identifier cached from eventdev */
	int socket_id;
	/* Per adapter EAL service */
	uint32_t service_id;
} __rte_cache_aligned;

/* Per eth device */
struct eth_device_info {
	struct rte_eth_dev *dev;
	struct eth_rx_queue_info *rx_queue;
	/* Set if ethdev->eventdev packet transfer uses a
	 * hardware mechanism
	 */
	uint8_t internal_event_port;
	/* Set if the adapter is processing rx queues for
	 * this eth device and packet processing has been
	 * started, allows for the code to know if the PMD
	 * rx_adapter_stop callback needs to be invoked
	 */
	uint8_t dev_rx_started;
	/* If nb_dev_queues > 0, the start callback will
	 * be invoked if not already invoked
	 */
	uint16_t nb_dev_queues;
};

/* Per Rx queue */
struct eth_rx_queue_info {
	int queue_enabled;	/* True if added */
	uint16_t wt;		/* Polling weight */
	uint8_t event_queue_id;	/* Event queue to enqueue packets to */
	uint8_t sched_type;	/* Sched type for events */
	uint8_t priority;	/* Event priority */
	uint32_t flow_id;	/* App provided flow identifier */
	uint32_t flow_id_mask;	/* Set to ~0 if app provides flow id else 0 */
};

static struct rte_event_eth_rx_adapter **event_eth_rx_adapter;

static inline int
valid_id(uint8_t id)
{
	return id < RTE_EVENT_ETH_RX_ADAPTER_MAX_INSTANCE;
}

#define RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, retval) do { \
	if (!valid_id(id)) { \
		RTE_EDEV_LOG_ERR("Invalid eth Rx adapter id = %d\n", id); \
		return retval; \
	} \
} while (0)

static inline int
sw_rx_adapter_queue_count(struct rte_event_eth_rx_adapter *rx_adapter)
{
	return rx_adapter->num_rx_polled;
}

/* Greatest common divisor */
static uint16_t gcd_u16(uint16_t a, uint16_t b)
{
	uint16_t r = a % b;

	return r ? gcd_u16(b, r) : b;
}

/* Returns the next queue in the polling sequence
 *
 * http://kb.linuxvirtualserver.org/wiki/Weighted_Round-Robin_Scheduling
 */
static int
wrr_next(struct rte_event_eth_rx_adapter *rx_adapter,
	 unsigned int n, int *cw,
	 struct eth_rx_poll_entry *eth_rx_poll, uint16_t max_wt,
	 uint16_t gcd, int prev)
{
	int i = prev;
	uint16_t w;

	while (1) {
		uint16_t q;
		uint16_t d;

		i = (i + 1) % n;
		if (i == 0) {
			*cw = *cw - gcd;
			if (*cw <= 0)
				*cw = max_wt;
		}

		q = eth_rx_poll[i].eth_rx_qid;
		d = eth_rx_poll[i].eth_dev_id;
		w = rx_adapter->eth_devices[d].rx_queue[q].wt;

		if ((int)w >= *cw)
			return i;
	}
}

/* Precalculate WRR polling sequence for all queues in rx_adapter */
static int
eth_poll_wrr_calc(struct rte_event_eth_rx_adapter *rx_adapter)
{
	uint16_t d;
	uint16_t q;
	unsigned int i;

	/* Initialize variables for calculation of wrr schedule */
	uint16_t max_wrr_pos = 0;
	unsigned int poll_q = 0;
	uint16_t max_wt = 0;
	uint16_t gcd = 0;

	struct eth_rx_poll_entry *rx_poll = NULL;
	uint32_t *rx_wrr = NULL;

	if (rx_adapter->num_rx_polled) {
		size_t len = RTE_ALIGN(rx_adapter->num_rx_polled *
				sizeof(*rx_adapter->eth_rx_poll),
				RTE_CACHE_LINE_SIZE);
		rx_poll = rte_zmalloc_socket(rx_adapter->mem_name,
					     len,
					     RTE_CACHE_LINE_SIZE,
					     rx_adapter->socket_id);
		if (rx_poll == NULL)
			return -ENOMEM;

		/* Generate array of all queues to poll, the size of this
		 * array is poll_q
		 */
		RTE_ETH_FOREACH_DEV(d) {
			uint16_t nb_rx_queues;
			struct eth_device_info *dev_info =
					&rx_adapter->eth_devices[d];
			nb_rx_queues = dev_info->dev->data->nb_rx_queues;
			if (dev_info->rx_queue == NULL)
				continue;
			for (q = 0; q < nb_rx_queues; q++) {
				struct eth_rx_queue_info *queue_info =
					&dev_info->rx_queue[q];
				if (queue_info->queue_enabled == 0)
					continue;

				uint16_t wt = queue_info->wt;
				rx_poll[poll_q].eth_dev_id = d;
				rx_poll[poll_q].eth_rx_qid = q;
				max_wrr_pos += wt;
				max_wt = RTE_MAX(max_wt, wt);
				gcd = (gcd) ? gcd_u16(gcd, wt) : wt;
				poll_q++;
			}
		}

		len = RTE_ALIGN(max_wrr_pos * sizeof(*rx_wrr),
				RTE_CACHE_LINE_SIZE);
		rx_wrr = rte_zmalloc_socket(rx_adapter->mem_name,
					    len,
					    RTE_CACHE_LINE_SIZE,
					    rx_adapter->socket_id);
		if (rx_wrr == NULL) {
			rte_free(rx_poll);
			return -ENOMEM;
		}

		/* Generate polling sequence based on weights */
		int prev = -1;
		int cw = -1;
		for (i = 0; i < max_wrr_pos; i++) {
			rx_wrr[i] = wrr_next(rx_adapter, poll_q, &cw,
					     rx_poll, max_wt, gcd, prev);
			prev = rx_wrr[i];
		}
	}

	rte_free(rx_adapter->eth_rx_poll);
	rte_free(rx_adapter->wrr_sched);

	rx_adapter->eth_rx_poll = rx_poll;
	rx_adapter->wrr_sched = rx_wrr;
	rx_adapter->wrr_len = max_wrr_pos;

	return 0;
}

static inline void
mtoip(struct rte_mbuf *m, struct ipv4_hdr **ipv4_hdr,
	struct ipv6_hdr **ipv6_hdr)
{
	struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	struct vlan_hdr *vlan_hdr;

	*ipv4_hdr = NULL;
	*ipv6_hdr = NULL;

	switch (eth_hdr->ether_type) {
	case RTE_BE16(ETHER_TYPE_IPv4):
		*ipv4_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
		break;

	case RTE_BE16(ETHER_TYPE_IPv6):
		*ipv6_hdr = (struct ipv6_hdr *)(eth_hdr + 1);
		break;

	case RTE_BE16(ETHER_TYPE_VLAN):
		vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
		switch (vlan_hdr->eth_proto) {
		case RTE_BE16(ETHER_TYPE_IPv4):
			*ipv4_hdr = (struct ipv4_hdr *)(vlan_hdr + 1);
			break;
		case RTE_BE16(ETHER_TYPE_IPv6):
			*ipv6_hdr = (struct ipv6_hdr *)(vlan_hdr + 1);
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/* Calculate RSS hash for IPv4/6 */
static inline uint32_t
do_softrss(struct rte_mbuf *m, const uint8_t *rss_key_be)
{
	uint32_t input_len;
	void *tuple;
	struct rte_ipv4_tuple ipv4_tuple;
	struct rte_ipv6_tuple ipv6_tuple;
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;

	mtoip(m, &ipv4_hdr, &ipv6_hdr);

	if (ipv4_hdr) {
		ipv4_tuple.src_addr = rte_be_to_cpu_32(ipv4_hdr->src_addr);
		ipv4_tuple.dst_addr = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
		tuple = &ipv4_tuple;
		input_len = RTE_THASH_V4_L3_LEN;
	} else if (ipv6_hdr) {
		rte_thash_load_v6_addrs(ipv6_hdr,
					(union rte_thash_tuple *)&ipv6_tuple);
		tuple = &ipv6_tuple;
		input_len = RTE_THASH_V6_L3_LEN;
	} else
		return 0;

	return rte_softrss_be(tuple, input_len, rss_key_be);
}

static inline int
rx_enq_blocked(struct rte_event_eth_rx_adapter *rx_adapter)
{
	return !!rx_adapter->enq_block_count;
}

static inline void
rx_enq_block_start_ts(struct rte_event_eth_rx_adapter *rx_adapter)
{
	if (rx_adapter->rx_enq_block_start_ts)
		return;

	rx_adapter->enq_block_count++;
	if (rx_adapter->enq_block_count < BLOCK_CNT_THRESHOLD)
		return;

	rx_adapter->rx_enq_block_start_ts = rte_get_tsc_cycles();
}

static inline void
rx_enq_block_end_ts(struct rte_event_eth_rx_adapter *rx_adapter,
		    struct rte_event_eth_rx_adapter_stats *stats)
{
	if (unlikely(!stats->rx_enq_start_ts))
		stats->rx_enq_start_ts = rte_get_tsc_cycles();

	if (likely(!rx_enq_blocked(rx_adapter)))
		return;

	rx_adapter->enq_block_count = 0;
	if (rx_adapter->rx_enq_block_start_ts) {
		stats->rx_enq_end_ts = rte_get_tsc_cycles();
		stats->rx_enq_block_cycles += stats->rx_enq_end_ts -
		    rx_adapter->rx_enq_block_start_ts;
		rx_adapter->rx_enq_block_start_ts = 0;
	}
}

/* Add event to buffer, free space check is done prior to calling
 * this function
 */
static inline void
buf_event_enqueue(struct rte_event_eth_rx_adapter *rx_adapter,
		  struct rte_event *ev)
{
	struct rte_eth_event_enqueue_buffer *buf =
	    &rx_adapter->event_enqueue_buffer;
	rte_memcpy(&buf->events[buf->count++], ev, sizeof(struct rte_event));
}

/* Enqueue buffered events to event device */
static inline uint16_t
flush_event_buffer(struct rte_event_eth_rx_adapter *rx_adapter)
{
	struct rte_eth_event_enqueue_buffer *buf =
	    &rx_adapter->event_enqueue_buffer;
	struct rte_event_eth_rx_adapter_stats *stats = &rx_adapter->stats;

	uint16_t n = rte_event_enqueue_new_burst(rx_adapter->eventdev_id,
					rx_adapter->event_port_id,
					buf->events,
					buf->count);
	if (n != buf->count) {
		memmove(buf->events,
			&buf->events[n],
			(buf->count - n) * sizeof(struct rte_event));
		stats->rx_enq_retry++;
	}

	n ? rx_enq_block_end_ts(rx_adapter, stats) :
		rx_enq_block_start_ts(rx_adapter);

	buf->count -= n;
	stats->rx_enq_count += n;

	return n;
}

static inline void
fill_event_buffer(struct rte_event_eth_rx_adapter *rx_adapter,
	uint8_t dev_id,
	uint16_t rx_queue_id,
	struct rte_mbuf **mbufs,
	uint16_t num)
{
	uint32_t i;
	struct eth_device_info *eth_device_info =
					&rx_adapter->eth_devices[dev_id];
	struct eth_rx_queue_info *eth_rx_queue_info =
					&eth_device_info->rx_queue[rx_queue_id];

	int32_t qid = eth_rx_queue_info->event_queue_id;
	uint8_t sched_type = eth_rx_queue_info->sched_type;
	uint8_t priority = eth_rx_queue_info->priority;
	uint32_t flow_id;
	struct rte_event events[BATCH_SIZE];
	struct rte_mbuf *m = mbufs[0];
	uint32_t rss_mask;
	uint32_t rss;
	int do_rss;
	uint64_t ts;

	/* 0xffff ffff if PKT_RX_RSS_HASH is set, otherwise 0 */
	rss_mask = ~(((m->ol_flags & PKT_RX_RSS_HASH) != 0) - 1);
	do_rss = !rss_mask && !eth_rx_queue_info->flow_id_mask;

	if ((m->ol_flags & PKT_RX_TIMESTAMP) == 0) {
		ts = rte_get_tsc_cycles();
		for (i = 0; i < num; i++) {
			m = mbufs[i];

			m->timestamp = ts;
			m->ol_flags |= PKT_RX_TIMESTAMP;
		}
	}

	for (i = 0; i < num; i++) {
		m = mbufs[i];
		struct rte_event *ev = &events[i];

		rss = do_rss ?
			do_softrss(m, rx_adapter->rss_key_be) : m->hash.rss;
		flow_id =
		    eth_rx_queue_info->flow_id &
				eth_rx_queue_info->flow_id_mask;
		flow_id |= rss & ~eth_rx_queue_info->flow_id_mask;
		ev->flow_id = flow_id;
		ev->op = RTE_EVENT_OP_NEW;
		ev->sched_type = sched_type;
		ev->queue_id = qid;
		ev->event_type = RTE_EVENT_TYPE_ETH_RX_ADAPTER;
		ev->sub_event_type = 0;
		ev->priority = priority;
		ev->mbuf = m;

		buf_event_enqueue(rx_adapter, ev);
	}
}

/*
 * Polls receive queues added to the event adapter and enqueues received
 * packets to the event device.
 *
 * The receive code enqueues initially to a temporary buffer, the
 * temporary buffer is drained anytime it holds >= BATCH_SIZE packets
 *
 * If there isn't space available in the temporary buffer, packets from the
 * Rx queue aren't dequeued from the eth device, this back pressures the
 * eth device, in virtual device environments this back pressure is relayed to
 * the hypervisor's switching layer where adjustments can be made to deal with
 * it.
 */
static inline uint32_t
eth_rx_poll(struct rte_event_eth_rx_adapter *rx_adapter)
{
	uint32_t num_queue;
	uint16_t n;
	uint32_t nb_rx = 0;
	struct rte_mbuf *mbufs[BATCH_SIZE];
	struct rte_eth_event_enqueue_buffer *buf;
	uint32_t wrr_pos;
	uint32_t max_nb_rx;

	wrr_pos = rx_adapter->wrr_pos;
	max_nb_rx = rx_adapter->max_nb_rx;
	buf = &rx_adapter->event_enqueue_buffer;
	struct rte_event_eth_rx_adapter_stats *stats = &rx_adapter->stats;

	/* Iterate through a WRR sequence */
	for (num_queue = 0; num_queue < rx_adapter->wrr_len; num_queue++) {
		unsigned int poll_idx = rx_adapter->wrr_sched[wrr_pos];
		uint16_t qid = rx_adapter->eth_rx_poll[poll_idx].eth_rx_qid;
		uint16_t d = rx_adapter->eth_rx_poll[poll_idx].eth_dev_id;

		/* Don't do a batch dequeue from the rx queue if there isn't
		 * enough space in the enqueue buffer.
		 */
		if (buf->count >= BATCH_SIZE)
			flush_event_buffer(rx_adapter);
		if (BATCH_SIZE > (ETH_EVENT_BUFFER_SIZE - buf->count))
			break;

		stats->rx_poll_count++;
		n = rte_eth_rx_burst(d, qid, mbufs, BATCH_SIZE);

		if (n) {
			stats->rx_packets += n;
			/* The check before rte_eth_rx_burst() ensures that
			 * all n mbufs can be buffered
			 */
			fill_event_buffer(rx_adapter, d, qid, mbufs, n);
			nb_rx += n;
			if (nb_rx > max_nb_rx) {
				rx_adapter->wrr_pos =
				    (wrr_pos + 1) % rx_adapter->wrr_len;
				return nb_rx;
			}
		}

		if (++wrr_pos == rx_adapter->wrr_len)
			wrr_pos = 0;
	}

	return nb_rx;
}

static int
event_eth_rx_adapter_service_func(void *args)
{
	struct rte_event_eth_rx_adapter *rx_adapter = args;
	struct rte_eth_event_enqueue_buffer *buf;

	buf = &rx_adapter->event_enqueue_buffer;
	if (rte_spinlock_trylock(&rx_adapter->rx_lock) == 0)
		return 0;
	if (eth_rx_poll(rx_adapter) == 0 && buf->count)
		flush_event_buffer(rx_adapter);
	rte_spinlock_unlock(&rx_adapter->rx_lock);
	return 0;
}

static int
rte_event_eth_rx_adapter_init(void)
{
	const char *name = "rte_event_eth_rx_adapter_array";
	const struct rte_memzone *mz;
	unsigned int sz;

	sz = sizeof(*event_eth_rx_adapter) *
	    RTE_EVENT_ETH_RX_ADAPTER_MAX_INSTANCE;
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

	event_eth_rx_adapter = mz->addr;
	return 0;
}

static inline struct rte_event_eth_rx_adapter *
id_to_rx_adapter(uint8_t id)
{
	return event_eth_rx_adapter ?
		event_eth_rx_adapter[id] : NULL;
}

static int
default_conf_cb(uint8_t id, uint8_t dev_id,
		struct rte_event_eth_rx_adapter_conf *conf, void *arg)
{
	int ret;
	struct rte_eventdev *dev;
	struct rte_event_dev_config dev_conf;
	int started;
	uint8_t port_id;
	struct rte_event_port_conf *port_conf = arg;
	struct rte_event_eth_rx_adapter *rx_adapter = id_to_rx_adapter(id);

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	dev_conf = dev->data->dev_conf;

	started = dev->data->dev_started;
	if (started)
		rte_event_dev_stop(dev_id);
	port_id = dev_conf.nb_event_ports;
	dev_conf.nb_event_ports += 1;
	ret = rte_event_dev_configure(dev_id, &dev_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to configure event dev %u\n",
						dev_id);
		if (started) {
			if (rte_event_dev_start(dev_id))
				return -EIO;
		}
		return ret;
	}

	ret = rte_event_port_setup(dev_id, port_id, port_conf);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to setup event port %u\n",
					port_id);
		return ret;
	}

	conf->event_port_id = port_id;
	conf->max_nb_rx = 128;
	if (started)
		ret = rte_event_dev_start(dev_id);
	rx_adapter->default_cb_arg = 1;
	return ret;
}

static int
init_service(struct rte_event_eth_rx_adapter *rx_adapter, uint8_t id)
{
	int ret;
	struct rte_service_spec service;
	struct rte_event_eth_rx_adapter_conf rx_adapter_conf;

	if (rx_adapter->service_inited)
		return 0;

	memset(&service, 0, sizeof(service));
	snprintf(service.name, ETH_RX_ADAPTER_SERVICE_NAME_LEN,
		"rte_event_eth_rx_adapter_%d", id);
	service.socket_id = rx_adapter->socket_id;
	service.callback = event_eth_rx_adapter_service_func;
	service.callback_userdata = rx_adapter;
	/* Service function handles locking for queue add/del updates */
	service.capabilities = RTE_SERVICE_CAP_MT_SAFE;
	ret = rte_service_component_register(&service, &rx_adapter->service_id);
	if (ret) {
		RTE_EDEV_LOG_ERR("failed to register service %s err = %" PRId32,
			service.name, ret);
		return ret;
	}

	ret = rx_adapter->conf_cb(id, rx_adapter->eventdev_id,
		&rx_adapter_conf, rx_adapter->conf_arg);
	if (ret) {
		RTE_EDEV_LOG_ERR("configuration callback failed err = %" PRId32,
			ret);
		goto err_done;
	}
	rx_adapter->event_port_id = rx_adapter_conf.event_port_id;
	rx_adapter->max_nb_rx = rx_adapter_conf.max_nb_rx;
	rx_adapter->service_inited = 1;
	return 0;

err_done:
	rte_service_component_unregister(rx_adapter->service_id);
	return ret;
}


static void
update_queue_info(struct rte_event_eth_rx_adapter *rx_adapter,
		struct eth_device_info *dev_info,
		int32_t rx_queue_id,
		uint8_t add)
{
	struct eth_rx_queue_info *queue_info;
	int enabled;
	uint16_t i;

	if (dev_info->rx_queue == NULL)
		return;

	if (rx_queue_id == -1) {
		for (i = 0; i < dev_info->dev->data->nb_rx_queues; i++)
			update_queue_info(rx_adapter, dev_info, i, add);
	} else {
		queue_info = &dev_info->rx_queue[rx_queue_id];
		enabled = queue_info->queue_enabled;
		if (add) {
			rx_adapter->nb_queues += !enabled;
			dev_info->nb_dev_queues += !enabled;
		} else {
			rx_adapter->nb_queues -= enabled;
			dev_info->nb_dev_queues -= enabled;
		}
		queue_info->queue_enabled = !!add;
	}
}

static int
event_eth_rx_adapter_queue_del(struct rte_event_eth_rx_adapter *rx_adapter,
			    struct eth_device_info *dev_info,
			    uint16_t rx_queue_id)
{
	struct eth_rx_queue_info *queue_info;

	if (rx_adapter->nb_queues == 0)
		return 0;

	queue_info = &dev_info->rx_queue[rx_queue_id];
	rx_adapter->num_rx_polled -= queue_info->queue_enabled;
	update_queue_info(rx_adapter, dev_info, rx_queue_id, 0);
	return 0;
}

static void
event_eth_rx_adapter_queue_add(struct rte_event_eth_rx_adapter *rx_adapter,
		struct eth_device_info *dev_info,
		uint16_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *conf)

{
	struct eth_rx_queue_info *queue_info;
	const struct rte_event *ev = &conf->ev;

	queue_info = &dev_info->rx_queue[rx_queue_id];
	queue_info->event_queue_id = ev->queue_id;
	queue_info->sched_type = ev->sched_type;
	queue_info->priority = ev->priority;
	queue_info->wt = conf->servicing_weight;

	if (conf->rx_queue_flags &
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID) {
		queue_info->flow_id = ev->flow_id;
		queue_info->flow_id_mask = ~0;
	}

	/* The same queue can be added more than once */
	rx_adapter->num_rx_polled += !queue_info->queue_enabled;
	update_queue_info(rx_adapter, dev_info, rx_queue_id, 1);
}

static int add_rx_queue(struct rte_event_eth_rx_adapter *rx_adapter,
		uint16_t eth_dev_id,
		int rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct eth_device_info *dev_info = &rx_adapter->eth_devices[eth_dev_id];
	struct rte_event_eth_rx_adapter_queue_conf temp_conf;
	uint32_t i;
	int ret;

	if (queue_conf->servicing_weight == 0) {

		struct rte_eth_dev_data *data = dev_info->dev->data;
		if (data->dev_conf.intr_conf.rxq) {
			RTE_EDEV_LOG_ERR("Interrupt driven queues"
					" not supported");
			return -ENOTSUP;
		}
		temp_conf = *queue_conf;

		/* If Rx interrupts are disabled set wt = 1 */
		temp_conf.servicing_weight = 1;
		queue_conf = &temp_conf;
	}

	if (dev_info->rx_queue == NULL) {
		dev_info->rx_queue =
		    rte_zmalloc_socket(rx_adapter->mem_name,
				       dev_info->dev->data->nb_rx_queues *
				       sizeof(struct eth_rx_queue_info), 0,
				       rx_adapter->socket_id);
		if (dev_info->rx_queue == NULL)
			return -ENOMEM;
	}

	if (rx_queue_id == -1) {
		for (i = 0; i < dev_info->dev->data->nb_rx_queues; i++)
			event_eth_rx_adapter_queue_add(rx_adapter,
						dev_info, i,
						queue_conf);
	} else {
		event_eth_rx_adapter_queue_add(rx_adapter, dev_info,
					  (uint16_t)rx_queue_id,
					  queue_conf);
	}

	ret = eth_poll_wrr_calc(rx_adapter);
	if (ret) {
		event_eth_rx_adapter_queue_del(rx_adapter,
					dev_info, rx_queue_id);
		return ret;
	}

	return ret;
}

static int
rx_adapter_ctrl(uint8_t id, int start)
{
	struct rte_event_eth_rx_adapter *rx_adapter;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	uint32_t i;
	int use_service = 0;
	int stop = !start;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];

	RTE_ETH_FOREACH_DEV(i) {
		dev_info = &rx_adapter->eth_devices[i];
		/* if start  check for num dev queues */
		if (start && !dev_info->nb_dev_queues)
			continue;
		/* if stop check if dev has been started */
		if (stop && !dev_info->dev_rx_started)
			continue;
		use_service |= !dev_info->internal_event_port;
		dev_info->dev_rx_started = start;
		if (dev_info->internal_event_port == 0)
			continue;
		start ? (*dev->dev_ops->eth_rx_adapter_start)(dev,
						&rte_eth_devices[i]) :
			(*dev->dev_ops->eth_rx_adapter_stop)(dev,
						&rte_eth_devices[i]);
	}

	if (use_service)
		rte_service_runstate_set(rx_adapter->service_id, start);

	return 0;
}

int
rte_event_eth_rx_adapter_create_ext(uint8_t id, uint8_t dev_id,
				rte_event_eth_rx_adapter_conf_cb conf_cb,
				void *conf_arg)
{
	struct rte_event_eth_rx_adapter *rx_adapter;
	int ret;
	int socket_id;
	uint16_t i;
	char mem_name[ETH_RX_ADAPTER_SERVICE_NAME_LEN];
	const uint8_t default_rss_key[] = {
		0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
		0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
		0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
		0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
		0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
	};

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	if (conf_cb == NULL)
		return -EINVAL;

	if (event_eth_rx_adapter == NULL) {
		ret = rte_event_eth_rx_adapter_init();
		if (ret)
			return ret;
	}

	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter != NULL) {
		RTE_EDEV_LOG_ERR("Eth Rx adapter exists id = %" PRIu8, id);
		return -EEXIST;
	}

	socket_id = rte_event_dev_socket_id(dev_id);
	snprintf(mem_name, ETH_RX_ADAPTER_MEM_NAME_LEN,
		"rte_event_eth_rx_adapter_%d",
		id);

	rx_adapter = rte_zmalloc_socket(mem_name, sizeof(*rx_adapter),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (rx_adapter == NULL) {
		RTE_EDEV_LOG_ERR("failed to get mem for rx adapter");
		return -ENOMEM;
	}

	rx_adapter->eventdev_id = dev_id;
	rx_adapter->socket_id = socket_id;
	rx_adapter->conf_cb = conf_cb;
	rx_adapter->conf_arg = conf_arg;
	strcpy(rx_adapter->mem_name, mem_name);
	rx_adapter->eth_devices = rte_zmalloc_socket(rx_adapter->mem_name,
					/* FIXME: incompatible with hotplug */
					rte_eth_dev_count_total() *
					sizeof(struct eth_device_info), 0,
					socket_id);
	rte_convert_rss_key((const uint32_t *)default_rss_key,
			(uint32_t *)rx_adapter->rss_key_be,
			    RTE_DIM(default_rss_key));

	if (rx_adapter->eth_devices == NULL) {
		RTE_EDEV_LOG_ERR("failed to get mem for eth devices\n");
		rte_free(rx_adapter);
		return -ENOMEM;
	}
	rte_spinlock_init(&rx_adapter->rx_lock);
	RTE_ETH_FOREACH_DEV(i)
		rx_adapter->eth_devices[i].dev = &rte_eth_devices[i];

	event_eth_rx_adapter[id] = rx_adapter;
	if (conf_cb == default_conf_cb)
		rx_adapter->default_cb_arg = 1;
	return 0;
}

int
rte_event_eth_rx_adapter_create(uint8_t id, uint8_t dev_id,
		struct rte_event_port_conf *port_config)
{
	struct rte_event_port_conf *pc;
	int ret;

	if (port_config == NULL)
		return -EINVAL;
	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	pc = rte_malloc(NULL, sizeof(*pc), 0);
	if (pc == NULL)
		return -ENOMEM;
	*pc = *port_config;
	ret = rte_event_eth_rx_adapter_create_ext(id, dev_id,
					default_conf_cb,
					pc);
	if (ret)
		rte_free(pc);
	return ret;
}

int
rte_event_eth_rx_adapter_free(uint8_t id)
{
	struct rte_event_eth_rx_adapter *rx_adapter;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	if (rx_adapter->nb_queues) {
		RTE_EDEV_LOG_ERR("%" PRIu16 " Rx queues not deleted",
				rx_adapter->nb_queues);
		return -EBUSY;
	}

	if (rx_adapter->default_cb_arg)
		rte_free(rx_adapter->conf_arg);
	rte_free(rx_adapter->eth_devices);
	rte_free(rx_adapter);
	event_eth_rx_adapter[id] = NULL;

	return 0;
}

int
rte_event_eth_rx_adapter_queue_add(uint8_t id,
		uint16_t eth_dev_id,
		int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	int ret;
	uint32_t cap;
	struct rte_event_eth_rx_adapter *rx_adapter;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	int start_service;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = id_to_rx_adapter(id);
	if ((rx_adapter == NULL) || (queue_conf == NULL))
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	ret = rte_event_eth_rx_adapter_caps_get(rx_adapter->eventdev_id,
						eth_dev_id,
						&cap);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps edev %" PRIu8
			"eth port %" PRIu16, id, eth_dev_id);
		return ret;
	}

	if ((cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) == 0
		&& (queue_conf->rx_queue_flags &
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID)) {
		RTE_EDEV_LOG_ERR("Flow ID override is not supported,"
				" eth port: %" PRIu16 " adapter id: %" PRIu8,
				eth_dev_id, id);
		return -EINVAL;
	}

	if ((cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) == 0 &&
		(rx_queue_id != -1)) {
		RTE_EDEV_LOG_ERR("Rx queues can only be connected to single "
			"event queue, eth port: %" PRIu16 " adapter id: %"
			PRIu8, eth_dev_id, id);
		return -EINVAL;
	}

	if (rx_queue_id != -1 && (uint16_t)rx_queue_id >=
			rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %" PRIu16,
			 (uint16_t)rx_queue_id);
		return -EINVAL;
	}

	start_service = 0;
	dev_info = &rx_adapter->eth_devices[eth_dev_id];

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->eth_rx_adapter_queue_add,
					-ENOTSUP);
		if (dev_info->rx_queue == NULL) {
			dev_info->rx_queue =
			    rte_zmalloc_socket(rx_adapter->mem_name,
					dev_info->dev->data->nb_rx_queues *
					sizeof(struct eth_rx_queue_info), 0,
					rx_adapter->socket_id);
			if (dev_info->rx_queue == NULL)
				return -ENOMEM;
		}

		ret = (*dev->dev_ops->eth_rx_adapter_queue_add)(dev,
				&rte_eth_devices[eth_dev_id],
				rx_queue_id, queue_conf);
		if (ret == 0) {
			update_queue_info(rx_adapter,
					&rx_adapter->eth_devices[eth_dev_id],
					rx_queue_id,
					1);
		}
	} else {
		rte_spinlock_lock(&rx_adapter->rx_lock);
		ret = init_service(rx_adapter, id);
		if (ret == 0)
			ret = add_rx_queue(rx_adapter, eth_dev_id, rx_queue_id,
					queue_conf);
		rte_spinlock_unlock(&rx_adapter->rx_lock);
		if (ret == 0)
			start_service = !!sw_rx_adapter_queue_count(rx_adapter);
	}

	if (ret)
		return ret;

	if (start_service)
		rte_service_component_runstate_set(rx_adapter->service_id, 1);

	return 0;
}

int
rte_event_eth_rx_adapter_queue_del(uint8_t id, uint16_t eth_dev_id,
				int32_t rx_queue_id)
{
	int ret = 0;
	struct rte_eventdev *dev;
	struct rte_event_eth_rx_adapter *rx_adapter;
	struct eth_device_info *dev_info;
	uint32_t cap;
	uint16_t i;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	ret = rte_event_eth_rx_adapter_caps_get(rx_adapter->eventdev_id,
						eth_dev_id,
						&cap);
	if (ret)
		return ret;

	if (rx_queue_id != -1 && (uint16_t)rx_queue_id >=
		rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %" PRIu16,
			 (uint16_t)rx_queue_id);
		return -EINVAL;
	}

	dev_info = &rx_adapter->eth_devices[eth_dev_id];

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->eth_rx_adapter_queue_del,
				 -ENOTSUP);
		ret = (*dev->dev_ops->eth_rx_adapter_queue_del)(dev,
						&rte_eth_devices[eth_dev_id],
						rx_queue_id);
		if (ret == 0) {
			update_queue_info(rx_adapter,
					&rx_adapter->eth_devices[eth_dev_id],
					rx_queue_id,
					0);
			if (dev_info->nb_dev_queues == 0) {
				rte_free(dev_info->rx_queue);
				dev_info->rx_queue = NULL;
			}
		}
	} else {
		int rc;
		rte_spinlock_lock(&rx_adapter->rx_lock);
		if (rx_queue_id == -1) {
			for (i = 0; i < dev_info->dev->data->nb_rx_queues; i++)
				event_eth_rx_adapter_queue_del(rx_adapter,
							dev_info,
							i);
		} else {
			event_eth_rx_adapter_queue_del(rx_adapter,
						dev_info,
						(uint16_t)rx_queue_id);
		}

		rc = eth_poll_wrr_calc(rx_adapter);
		if (rc)
			RTE_EDEV_LOG_ERR("WRR recalculation failed %" PRId32,
					rc);

		if (dev_info->nb_dev_queues == 0) {
			rte_free(dev_info->rx_queue);
			dev_info->rx_queue = NULL;
		}

		rte_spinlock_unlock(&rx_adapter->rx_lock);
		rte_service_component_runstate_set(rx_adapter->service_id,
				sw_rx_adapter_queue_count(rx_adapter));
	}

	return ret;
}


int
rte_event_eth_rx_adapter_start(uint8_t id)
{
	return rx_adapter_ctrl(id, 1);
}

int
rte_event_eth_rx_adapter_stop(uint8_t id)
{
	return rx_adapter_ctrl(id, 0);
}

int
rte_event_eth_rx_adapter_stats_get(uint8_t id,
			       struct rte_event_eth_rx_adapter_stats *stats)
{
	struct rte_event_eth_rx_adapter *rx_adapter;
	struct rte_event_eth_rx_adapter_stats dev_stats_sum = { 0 };
	struct rte_event_eth_rx_adapter_stats dev_stats;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	uint32_t i;
	int ret;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter  == NULL || stats == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	memset(stats, 0, sizeof(*stats));
	RTE_ETH_FOREACH_DEV(i) {
		dev_info = &rx_adapter->eth_devices[i];
		if (dev_info->internal_event_port == 0 ||
			dev->dev_ops->eth_rx_adapter_stats_get == NULL)
			continue;
		ret = (*dev->dev_ops->eth_rx_adapter_stats_get)(dev,
						&rte_eth_devices[i],
						&dev_stats);
		if (ret)
			continue;
		dev_stats_sum.rx_packets += dev_stats.rx_packets;
		dev_stats_sum.rx_enq_count += dev_stats.rx_enq_count;
	}

	if (rx_adapter->service_inited)
		*stats = rx_adapter->stats;

	stats->rx_packets += dev_stats_sum.rx_packets;
	stats->rx_enq_count += dev_stats_sum.rx_enq_count;
	return 0;
}

int
rte_event_eth_rx_adapter_stats_reset(uint8_t id)
{
	struct rte_event_eth_rx_adapter *rx_adapter;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	uint32_t i;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	RTE_ETH_FOREACH_DEV(i) {
		dev_info = &rx_adapter->eth_devices[i];
		if (dev_info->internal_event_port == 0 ||
			dev->dev_ops->eth_rx_adapter_stats_reset == NULL)
			continue;
		(*dev->dev_ops->eth_rx_adapter_stats_reset)(dev,
							&rte_eth_devices[i]);
	}

	memset(&rx_adapter->stats, 0, sizeof(rx_adapter->stats));
	return 0;
}

int
rte_event_eth_rx_adapter_service_id_get(uint8_t id, uint32_t *service_id)
{
	struct rte_event_eth_rx_adapter *rx_adapter;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = id_to_rx_adapter(id);
	if (rx_adapter == NULL || service_id == NULL)
		return -EINVAL;

	if (rx_adapter->service_inited)
		*service_id = rx_adapter->service_id;

	return rx_adapter->service_inited ? 0 : -ESRCH;
}
