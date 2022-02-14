/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation.
 * All rights reserved.
 */
#if defined(LINUX)
#include <sys/epoll.h>
#endif
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_service_component.h>
#include <rte_thash.h>
#include <rte_interrupts.h>
#include <rte_mbuf_dyn.h>
#include <rte_telemetry.h>

#include "rte_eventdev.h"
#include "eventdev_pmd.h"
#include "eventdev_trace.h"
#include "rte_event_eth_rx_adapter.h"

#define BATCH_SIZE		32
#define BLOCK_CNT_THRESHOLD	10
#define ETH_EVENT_BUFFER_SIZE	(6*BATCH_SIZE)
#define MAX_VECTOR_SIZE		1024
#define MIN_VECTOR_SIZE		4
#define MAX_VECTOR_NS		1E9
#define MIN_VECTOR_NS		1E5

#define ETH_RX_ADAPTER_SERVICE_NAME_LEN	32
#define ETH_RX_ADAPTER_MEM_NAME_LEN	32

#define RSS_KEY_SIZE	40
/* value written to intr thread pipe to signal thread exit */
#define ETH_BRIDGE_INTR_THREAD_EXIT	1
/* Sentinel value to detect initialized file handle */
#define INIT_FD		-1

#define RXA_ADAPTER_ARRAY "rte_event_eth_rx_adapter_array"

/*
 * Used to store port and queue ID of interrupting Rx queue
 */
union queue_data {
	RTE_STD_C11
	void *ptr;
	struct {
		uint16_t port;
		uint16_t queue;
	};
};

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

struct eth_rx_vector_data {
	TAILQ_ENTRY(eth_rx_vector_data) next;
	uint16_t port;
	uint16_t queue;
	uint16_t max_vector_count;
	uint64_t event;
	uint64_t ts;
	uint64_t vector_timeout_ticks;
	struct rte_mempool *vector_pool;
	struct rte_event_vector *vector_ev;
} __rte_cache_aligned;

TAILQ_HEAD(eth_rx_vector_data_list, eth_rx_vector_data);

/* Instance per adapter */
struct eth_event_enqueue_buffer {
	/* Count of events in this buffer */
	uint16_t count;
	/* Array of events in this buffer */
	struct rte_event *events;
	/* size of event buffer */
	uint16_t events_size;
	/* Event enqueue happens from head */
	uint16_t head;
	/* New packets from rte_eth_rx_burst is enqued from tail */
	uint16_t tail;
	/* last element in the buffer before rollover */
	uint16_t last;
	uint16_t last_mask;
};

struct event_eth_rx_adapter {
	/* RSS key */
	uint8_t rss_key_be[RSS_KEY_SIZE];
	/* Event device identifier */
	uint8_t eventdev_id;
	/* Event port identifier */
	uint8_t event_port_id;
	/* Flag indicating per rxq event buffer */
	bool use_queue_event_buf;
	/* Per ethernet device structure */
	struct eth_device_info *eth_devices;
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
	struct eth_event_enqueue_buffer event_enqueue_buffer;
	/* Vector enable flag */
	uint8_t ena_vector;
	/* Timestamp of previous vector expiry list traversal */
	uint64_t prev_expiry_ts;
	/* Minimum ticks to wait before traversing expiry list */
	uint64_t vector_tmo_ticks;
	/* vector list */
	struct eth_rx_vector_data_list vector_list;
	/* Per adapter stats */
	struct rte_event_eth_rx_adapter_stats stats;
	/* Block count, counts up to BLOCK_CNT_THRESHOLD */
	uint16_t enq_block_count;
	/* Block start ts */
	uint64_t rx_enq_block_start_ts;
	/* epoll fd used to wait for Rx interrupts */
	int epd;
	/* Num of interrupt driven interrupt queues */
	uint32_t num_rx_intr;
	/* Used to send <dev id, queue id> of interrupting Rx queues from
	 * the interrupt thread to the Rx thread
	 */
	struct rte_ring *intr_ring;
	/* Rx Queue data (dev id, queue id) for the last non-empty
	 * queue polled
	 */
	union queue_data qd;
	/* queue_data is valid */
	int qd_valid;
	/* Interrupt ring lock, synchronizes Rx thread
	 * and interrupt thread
	 */
	rte_spinlock_t intr_ring_lock;
	/* event array passed to rte_poll_wait */
	struct rte_epoll_event *epoll_events;
	/* Count of interrupt vectors in use */
	uint32_t num_intr_vec;
	/* Thread blocked on Rx interrupts */
	pthread_t rx_intr_thread;
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
	/* Adapter started flag */
	uint8_t rxa_started;
	/* Adapter ID */
	uint8_t id;
} __rte_cache_aligned;

/* Per eth device */
struct eth_device_info {
	struct rte_eth_dev *dev;
	struct eth_rx_queue_info *rx_queue;
	/* Rx callback */
	rte_event_eth_rx_adapter_cb_fn cb_fn;
	/* Rx callback argument */
	void *cb_arg;
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
	/* Number of queues added for this device */
	uint16_t nb_dev_queues;
	/* Number of poll based queues
	 * If nb_rx_poll > 0, the start callback will
	 * be invoked if not already invoked
	 */
	uint16_t nb_rx_poll;
	/* Number of interrupt based queues
	 * If nb_rx_intr > 0, the start callback will
	 * be invoked if not already invoked.
	 */
	uint16_t nb_rx_intr;
	/* Number of queues that use the shared interrupt */
	uint16_t nb_shared_intr;
	/* sum(wrr(q)) for all queues within the device
	 * useful when deleting all device queues
	 */
	uint32_t wrr_len;
	/* Intr based queue index to start polling from, this is used
	 * if the number of shared interrupts is non-zero
	 */
	uint16_t next_q_idx;
	/* Intr based queue indices */
	uint16_t *intr_queue;
	/* device generates per Rx queue interrupt for queue index
	 * for queue indices < RTE_MAX_RXTX_INTR_VEC_ID - 1
	 */
	int multi_intr_cap;
	/* shared interrupt enabled */
	int shared_intr_enabled;
};

/* Per Rx queue */
struct eth_rx_queue_info {
	int queue_enabled;	/* True if added */
	int intr_enabled;
	uint8_t ena_vector;
	uint16_t wt;		/* Polling weight */
	uint32_t flow_id_mask;	/* Set to ~0 if app provides flow id else 0 */
	uint64_t event;
	struct eth_rx_vector_data vector_data;
	struct eth_event_enqueue_buffer *event_buf;
	/* use adapter stats struct for queue level stats,
	 * as same stats need to be updated for adapter and queue
	 */
	struct rte_event_eth_rx_adapter_stats *stats;
};

static struct event_eth_rx_adapter **event_eth_rx_adapter;

/* Enable dynamic timestamp field in mbuf */
static uint64_t event_eth_rx_timestamp_dynflag;
static int event_eth_rx_timestamp_dynfield_offset = -1;

static inline rte_mbuf_timestamp_t *
rxa_timestamp_dynfield(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
		event_eth_rx_timestamp_dynfield_offset, rte_mbuf_timestamp_t *);
}

static inline int
rxa_validate_id(uint8_t id)
{
	return id < RTE_EVENT_ETH_RX_ADAPTER_MAX_INSTANCE;
}

static inline struct eth_event_enqueue_buffer *
rxa_event_buf_get(struct event_eth_rx_adapter *rx_adapter, uint16_t eth_dev_id,
		  uint16_t rx_queue_id,
		  struct rte_event_eth_rx_adapter_stats **stats)
{
	if (rx_adapter->use_queue_event_buf) {
		struct eth_device_info *dev_info =
			&rx_adapter->eth_devices[eth_dev_id];
		*stats = dev_info->rx_queue[rx_queue_id].stats;
		return dev_info->rx_queue[rx_queue_id].event_buf;
	} else {
		*stats = &rx_adapter->stats;
		return &rx_adapter->event_enqueue_buffer;
	}
}

#define RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, retval) do { \
	if (!rxa_validate_id(id)) { \
		RTE_EDEV_LOG_ERR("Invalid eth Rx adapter id = %d\n", id); \
		return retval; \
	} \
} while (0)

#define RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_GOTO_ERR_RET(id, retval) do { \
	if (!rxa_validate_id(id)) { \
		RTE_EDEV_LOG_ERR("Invalid eth Rx adapter id = %d\n", id); \
		ret = retval; \
		goto error; \
	} \
} while (0)

#define RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, retval) do { \
	if ((token) == NULL || strlen(token) == 0 || !isdigit(*token)) { \
		RTE_EDEV_LOG_ERR("Invalid eth Rx adapter token\n"); \
		ret = retval; \
		goto error; \
	} \
} while (0)

#define RTE_ETH_VALID_PORTID_OR_GOTO_ERR_RET(port_id, retval) do { \
	if (!rte_eth_dev_is_valid_port(port_id)) { \
		RTE_ETHDEV_LOG(ERR, "Invalid port_id=%u\n", port_id); \
		ret = retval; \
		goto error; \
	} \
} while (0)

static inline int
rxa_sw_adapter_queue_count(struct event_eth_rx_adapter *rx_adapter)
{
	return rx_adapter->num_rx_polled + rx_adapter->num_rx_intr;
}

/* Greatest common divisor */
static uint16_t rxa_gcd_u16(uint16_t a, uint16_t b)
{
	uint16_t r = a % b;

	return r ? rxa_gcd_u16(b, r) : b;
}

/* Returns the next queue in the polling sequence
 *
 * http://kb.linuxvirtualserver.org/wiki/Weighted_Round-Robin_Scheduling
 */
static int
rxa_wrr_next(struct event_eth_rx_adapter *rx_adapter, unsigned int n, int *cw,
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

static inline int
rxa_shared_intr(struct eth_device_info *dev_info,
	int rx_queue_id)
{
	int multi_intr_cap;

	if (dev_info->dev->intr_handle == NULL)
		return 0;

	multi_intr_cap = rte_intr_cap_multiple(dev_info->dev->intr_handle);
	return !multi_intr_cap ||
		rx_queue_id >= RTE_MAX_RXTX_INTR_VEC_ID - 1;
}

static inline int
rxa_intr_queue(struct eth_device_info *dev_info,
	int rx_queue_id)
{
	struct eth_rx_queue_info *queue_info;

	queue_info = &dev_info->rx_queue[rx_queue_id];
	return dev_info->rx_queue &&
		!dev_info->internal_event_port &&
		queue_info->queue_enabled && queue_info->wt == 0;
}

static inline int
rxa_polled_queue(struct eth_device_info *dev_info,
	int rx_queue_id)
{
	struct eth_rx_queue_info *queue_info;

	queue_info = &dev_info->rx_queue[rx_queue_id];
	return !dev_info->internal_event_port &&
		dev_info->rx_queue &&
		queue_info->queue_enabled && queue_info->wt != 0;
}

/* Calculate change in number of vectors after Rx queue ID is add/deleted */
static int
rxa_nb_intr_vect(struct eth_device_info *dev_info, int rx_queue_id, int add)
{
	uint16_t i;
	int n, s;
	uint16_t nbq;

	nbq = dev_info->dev->data->nb_rx_queues;
	n = 0; /* non shared count */
	s = 0; /* shared count */

	if (rx_queue_id == -1) {
		for (i = 0; i < nbq; i++) {
			if (!rxa_shared_intr(dev_info, i))
				n += add ? !rxa_intr_queue(dev_info, i) :
					rxa_intr_queue(dev_info, i);
			else
				s += add ? !rxa_intr_queue(dev_info, i) :
					rxa_intr_queue(dev_info, i);
		}

		if (s > 0) {
			if ((add && dev_info->nb_shared_intr == 0) ||
				(!add && dev_info->nb_shared_intr))
				n += 1;
		}
	} else {
		if (!rxa_shared_intr(dev_info, rx_queue_id))
			n = add ? !rxa_intr_queue(dev_info, rx_queue_id) :
				rxa_intr_queue(dev_info, rx_queue_id);
		else
			n = add ? !dev_info->nb_shared_intr :
				dev_info->nb_shared_intr == 1;
	}

	return add ? n : -n;
}

/* Calculate nb_rx_intr after deleting interrupt mode rx queues
 */
static void
rxa_calc_nb_post_intr_del(struct event_eth_rx_adapter *rx_adapter,
			  struct eth_device_info *dev_info, int rx_queue_id,
			  uint32_t *nb_rx_intr)
{
	uint32_t intr_diff;

	if (rx_queue_id == -1)
		intr_diff = dev_info->nb_rx_intr;
	else
		intr_diff = rxa_intr_queue(dev_info, rx_queue_id);

	*nb_rx_intr = rx_adapter->num_rx_intr - intr_diff;
}

/* Calculate nb_rx_* after adding interrupt mode rx queues, newly added
 * interrupt queues could currently be poll mode Rx queues
 */
static void
rxa_calc_nb_post_add_intr(struct event_eth_rx_adapter *rx_adapter,
			  struct eth_device_info *dev_info, int rx_queue_id,
			  uint32_t *nb_rx_poll, uint32_t *nb_rx_intr,
			  uint32_t *nb_wrr)
{
	uint32_t intr_diff;
	uint32_t poll_diff;
	uint32_t wrr_len_diff;

	if (rx_queue_id == -1) {
		intr_diff = dev_info->dev->data->nb_rx_queues -
						dev_info->nb_rx_intr;
		poll_diff = dev_info->nb_rx_poll;
		wrr_len_diff = dev_info->wrr_len;
	} else {
		intr_diff = !rxa_intr_queue(dev_info, rx_queue_id);
		poll_diff = rxa_polled_queue(dev_info, rx_queue_id);
		wrr_len_diff = poll_diff ? dev_info->rx_queue[rx_queue_id].wt :
					0;
	}

	*nb_rx_intr = rx_adapter->num_rx_intr + intr_diff;
	*nb_rx_poll = rx_adapter->num_rx_polled - poll_diff;
	*nb_wrr = rx_adapter->wrr_len - wrr_len_diff;
}

/* Calculate size of the eth_rx_poll and wrr_sched arrays
 * after deleting poll mode rx queues
 */
static void
rxa_calc_nb_post_poll_del(struct event_eth_rx_adapter *rx_adapter,
			  struct eth_device_info *dev_info, int rx_queue_id,
			  uint32_t *nb_rx_poll, uint32_t *nb_wrr)
{
	uint32_t poll_diff;
	uint32_t wrr_len_diff;

	if (rx_queue_id == -1) {
		poll_diff = dev_info->nb_rx_poll;
		wrr_len_diff = dev_info->wrr_len;
	} else {
		poll_diff = rxa_polled_queue(dev_info, rx_queue_id);
		wrr_len_diff = poll_diff ? dev_info->rx_queue[rx_queue_id].wt :
					0;
	}

	*nb_rx_poll = rx_adapter->num_rx_polled - poll_diff;
	*nb_wrr = rx_adapter->wrr_len - wrr_len_diff;
}

/* Calculate nb_rx_* after adding poll mode rx queues
 */
static void
rxa_calc_nb_post_add_poll(struct event_eth_rx_adapter *rx_adapter,
			  struct eth_device_info *dev_info, int rx_queue_id,
			  uint16_t wt, uint32_t *nb_rx_poll,
			  uint32_t *nb_rx_intr, uint32_t *nb_wrr)
{
	uint32_t intr_diff;
	uint32_t poll_diff;
	uint32_t wrr_len_diff;

	if (rx_queue_id == -1) {
		intr_diff = dev_info->nb_rx_intr;
		poll_diff = dev_info->dev->data->nb_rx_queues -
						dev_info->nb_rx_poll;
		wrr_len_diff = wt*dev_info->dev->data->nb_rx_queues
				- dev_info->wrr_len;
	} else {
		intr_diff = rxa_intr_queue(dev_info, rx_queue_id);
		poll_diff = !rxa_polled_queue(dev_info, rx_queue_id);
		wrr_len_diff = rxa_polled_queue(dev_info, rx_queue_id) ?
				wt - dev_info->rx_queue[rx_queue_id].wt :
				wt;
	}

	*nb_rx_poll = rx_adapter->num_rx_polled + poll_diff;
	*nb_rx_intr = rx_adapter->num_rx_intr - intr_diff;
	*nb_wrr = rx_adapter->wrr_len + wrr_len_diff;
}

/* Calculate nb_rx_* after adding rx_queue_id */
static void
rxa_calc_nb_post_add(struct event_eth_rx_adapter *rx_adapter,
		     struct eth_device_info *dev_info, int rx_queue_id,
		     uint16_t wt, uint32_t *nb_rx_poll, uint32_t *nb_rx_intr,
		     uint32_t *nb_wrr)
{
	if (wt != 0)
		rxa_calc_nb_post_add_poll(rx_adapter, dev_info, rx_queue_id,
					wt, nb_rx_poll, nb_rx_intr, nb_wrr);
	else
		rxa_calc_nb_post_add_intr(rx_adapter, dev_info, rx_queue_id,
					nb_rx_poll, nb_rx_intr, nb_wrr);
}

/* Calculate nb_rx_* after deleting rx_queue_id */
static void
rxa_calc_nb_post_del(struct event_eth_rx_adapter *rx_adapter,
		     struct eth_device_info *dev_info, int rx_queue_id,
		     uint32_t *nb_rx_poll, uint32_t *nb_rx_intr,
		     uint32_t *nb_wrr)
{
	rxa_calc_nb_post_poll_del(rx_adapter, dev_info, rx_queue_id, nb_rx_poll,
				nb_wrr);
	rxa_calc_nb_post_intr_del(rx_adapter, dev_info, rx_queue_id,
				nb_rx_intr);
}

/*
 * Allocate the rx_poll array
 */
static struct eth_rx_poll_entry *
rxa_alloc_poll(struct event_eth_rx_adapter *rx_adapter, uint32_t num_rx_polled)
{
	size_t len;

	len  = RTE_ALIGN(num_rx_polled * sizeof(*rx_adapter->eth_rx_poll),
							RTE_CACHE_LINE_SIZE);
	return  rte_zmalloc_socket(rx_adapter->mem_name,
				len,
				RTE_CACHE_LINE_SIZE,
				rx_adapter->socket_id);
}

/*
 * Allocate the WRR array
 */
static uint32_t *
rxa_alloc_wrr(struct event_eth_rx_adapter *rx_adapter, int nb_wrr)
{
	size_t len;

	len = RTE_ALIGN(nb_wrr * sizeof(*rx_adapter->wrr_sched),
			RTE_CACHE_LINE_SIZE);
	return  rte_zmalloc_socket(rx_adapter->mem_name,
				len,
				RTE_CACHE_LINE_SIZE,
				rx_adapter->socket_id);
}

static int
rxa_alloc_poll_arrays(struct event_eth_rx_adapter *rx_adapter, uint32_t nb_poll,
		      uint32_t nb_wrr, struct eth_rx_poll_entry **rx_poll,
		      uint32_t **wrr_sched)
{

	if (nb_poll == 0) {
		*rx_poll = NULL;
		*wrr_sched = NULL;
		return 0;
	}

	*rx_poll = rxa_alloc_poll(rx_adapter, nb_poll);
	if (*rx_poll == NULL) {
		*wrr_sched = NULL;
		return -ENOMEM;
	}

	*wrr_sched = rxa_alloc_wrr(rx_adapter, nb_wrr);
	if (*wrr_sched == NULL) {
		rte_free(*rx_poll);
		return -ENOMEM;
	}
	return 0;
}

/* Precalculate WRR polling sequence for all queues in rx_adapter */
static void
rxa_calc_wrr_sequence(struct event_eth_rx_adapter *rx_adapter,
		      struct eth_rx_poll_entry *rx_poll, uint32_t *rx_wrr)
{
	uint16_t d;
	uint16_t q;
	unsigned int i;
	int prev = -1;
	int cw = -1;

	/* Initialize variables for calculation of wrr schedule */
	uint16_t max_wrr_pos = 0;
	unsigned int poll_q = 0;
	uint16_t max_wt = 0;
	uint16_t gcd = 0;

	if (rx_poll == NULL)
		return;

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
		if (dev_info->internal_event_port)
			continue;
		dev_info->wrr_len = 0;
		for (q = 0; q < nb_rx_queues; q++) {
			struct eth_rx_queue_info *queue_info =
				&dev_info->rx_queue[q];
			uint16_t wt;

			if (!rxa_polled_queue(dev_info, q))
				continue;
			wt = queue_info->wt;
			rx_poll[poll_q].eth_dev_id = d;
			rx_poll[poll_q].eth_rx_qid = q;
			max_wrr_pos += wt;
			dev_info->wrr_len += wt;
			max_wt = RTE_MAX(max_wt, wt);
			gcd = (gcd) ? rxa_gcd_u16(gcd, wt) : wt;
			poll_q++;
		}
	}

	/* Generate polling sequence based on weights */
	prev = -1;
	cw = -1;
	for (i = 0; i < max_wrr_pos; i++) {
		rx_wrr[i] = rxa_wrr_next(rx_adapter, poll_q, &cw,
				     rx_poll, max_wt, gcd, prev);
		prev = rx_wrr[i];
	}
}

static inline void
rxa_mtoip(struct rte_mbuf *m, struct rte_ipv4_hdr **ipv4_hdr,
	struct rte_ipv6_hdr **ipv6_hdr)
{
	struct rte_ether_hdr *eth_hdr =
		rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	struct rte_vlan_hdr *vlan_hdr;

	*ipv4_hdr = NULL;
	*ipv6_hdr = NULL;

	switch (eth_hdr->ether_type) {
	case RTE_BE16(RTE_ETHER_TYPE_IPV4):
		*ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
		break;

	case RTE_BE16(RTE_ETHER_TYPE_IPV6):
		*ipv6_hdr = (struct rte_ipv6_hdr *)(eth_hdr + 1);
		break;

	case RTE_BE16(RTE_ETHER_TYPE_VLAN):
		vlan_hdr = (struct rte_vlan_hdr *)(eth_hdr + 1);
		switch (vlan_hdr->eth_proto) {
		case RTE_BE16(RTE_ETHER_TYPE_IPV4):
			*ipv4_hdr = (struct rte_ipv4_hdr *)(vlan_hdr + 1);
			break;
		case RTE_BE16(RTE_ETHER_TYPE_IPV6):
			*ipv6_hdr = (struct rte_ipv6_hdr *)(vlan_hdr + 1);
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
rxa_do_softrss(struct rte_mbuf *m, const uint8_t *rss_key_be)
{
	uint32_t input_len;
	void *tuple;
	struct rte_ipv4_tuple ipv4_tuple;
	struct rte_ipv6_tuple ipv6_tuple;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;

	rxa_mtoip(m, &ipv4_hdr, &ipv6_hdr);

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
rxa_enq_blocked(struct event_eth_rx_adapter *rx_adapter)
{
	return !!rx_adapter->enq_block_count;
}

static inline void
rxa_enq_block_start_ts(struct event_eth_rx_adapter *rx_adapter)
{
	if (rx_adapter->rx_enq_block_start_ts)
		return;

	rx_adapter->enq_block_count++;
	if (rx_adapter->enq_block_count < BLOCK_CNT_THRESHOLD)
		return;

	rx_adapter->rx_enq_block_start_ts = rte_get_tsc_cycles();
}

static inline void
rxa_enq_block_end_ts(struct event_eth_rx_adapter *rx_adapter,
		     struct rte_event_eth_rx_adapter_stats *stats)
{
	if (unlikely(!stats->rx_enq_start_ts))
		stats->rx_enq_start_ts = rte_get_tsc_cycles();

	if (likely(!rxa_enq_blocked(rx_adapter)))
		return;

	rx_adapter->enq_block_count = 0;
	if (rx_adapter->rx_enq_block_start_ts) {
		stats->rx_enq_end_ts = rte_get_tsc_cycles();
		stats->rx_enq_block_cycles += stats->rx_enq_end_ts -
		    rx_adapter->rx_enq_block_start_ts;
		rx_adapter->rx_enq_block_start_ts = 0;
	}
}

/* Enqueue buffered events to event device */
static inline uint16_t
rxa_flush_event_buffer(struct event_eth_rx_adapter *rx_adapter,
		       struct eth_event_enqueue_buffer *buf,
		       struct rte_event_eth_rx_adapter_stats *stats)
{
	uint16_t count = buf->count;
	uint16_t n = 0;

	if (!count)
		return 0;

	if (buf->last)
		count = buf->last - buf->head;

	if (count) {
		n = rte_event_enqueue_new_burst(rx_adapter->eventdev_id,
						rx_adapter->event_port_id,
						&buf->events[buf->head],
						count);
		if (n != count)
			stats->rx_enq_retry++;

		buf->head += n;
	}

	if (buf->last && n == count) {
		uint16_t n1;

		n1 = rte_event_enqueue_new_burst(rx_adapter->eventdev_id,
					rx_adapter->event_port_id,
					&buf->events[0],
					buf->tail);

		if (n1 != buf->tail)
			stats->rx_enq_retry++;

		buf->last = 0;
		buf->head = n1;
		buf->last_mask = 0;
		n += n1;
	}

	n ? rxa_enq_block_end_ts(rx_adapter, stats) :
		rxa_enq_block_start_ts(rx_adapter);

	buf->count -= n;
	stats->rx_enq_count += n;

	return n;
}

static inline void
rxa_init_vector(struct event_eth_rx_adapter *rx_adapter,
		struct eth_rx_vector_data *vec)
{
	vec->vector_ev->nb_elem = 0;
	vec->vector_ev->port = vec->port;
	vec->vector_ev->queue = vec->queue;
	vec->vector_ev->attr_valid = true;
	TAILQ_INSERT_TAIL(&rx_adapter->vector_list, vec, next);
}

static inline uint16_t
rxa_create_event_vector(struct event_eth_rx_adapter *rx_adapter,
			struct eth_rx_queue_info *queue_info,
			struct eth_event_enqueue_buffer *buf,
			struct rte_mbuf **mbufs, uint16_t num)
{
	struct rte_event *ev = &buf->events[buf->count];
	struct eth_rx_vector_data *vec;
	uint16_t filled, space, sz;

	filled = 0;
	vec = &queue_info->vector_data;

	if (vec->vector_ev == NULL) {
		if (rte_mempool_get(vec->vector_pool,
				    (void **)&vec->vector_ev) < 0) {
			rte_pktmbuf_free_bulk(mbufs, num);
			return 0;
		}
		rxa_init_vector(rx_adapter, vec);
	}
	while (num) {
		if (vec->vector_ev->nb_elem == vec->max_vector_count) {
			/* Event ready. */
			ev->event = vec->event;
			ev->vec = vec->vector_ev;
			ev++;
			filled++;
			vec->vector_ev = NULL;
			TAILQ_REMOVE(&rx_adapter->vector_list, vec, next);
			if (rte_mempool_get(vec->vector_pool,
					    (void **)&vec->vector_ev) < 0) {
				rte_pktmbuf_free_bulk(mbufs, num);
				return 0;
			}
			rxa_init_vector(rx_adapter, vec);
		}

		space = vec->max_vector_count - vec->vector_ev->nb_elem;
		sz = num > space ? space : num;
		memcpy(vec->vector_ev->mbufs + vec->vector_ev->nb_elem, mbufs,
		       sizeof(void *) * sz);
		vec->vector_ev->nb_elem += sz;
		num -= sz;
		mbufs += sz;
		vec->ts = rte_rdtsc();
	}

	if (vec->vector_ev->nb_elem == vec->max_vector_count) {
		ev->event = vec->event;
		ev->vec = vec->vector_ev;
		ev++;
		filled++;
		vec->vector_ev = NULL;
		TAILQ_REMOVE(&rx_adapter->vector_list, vec, next);
	}

	return filled;
}

static inline void
rxa_buffer_mbufs(struct event_eth_rx_adapter *rx_adapter, uint16_t eth_dev_id,
		 uint16_t rx_queue_id, struct rte_mbuf **mbufs, uint16_t num,
		 struct eth_event_enqueue_buffer *buf,
		 struct rte_event_eth_rx_adapter_stats *stats)
{
	uint32_t i;
	struct eth_device_info *dev_info =
					&rx_adapter->eth_devices[eth_dev_id];
	struct eth_rx_queue_info *eth_rx_queue_info =
					&dev_info->rx_queue[rx_queue_id];
	uint16_t new_tail = buf->tail;
	uint64_t event = eth_rx_queue_info->event;
	uint32_t flow_id_mask = eth_rx_queue_info->flow_id_mask;
	struct rte_mbuf *m = mbufs[0];
	uint32_t rss_mask;
	uint32_t rss;
	int do_rss;
	uint16_t nb_cb;
	uint16_t dropped;
	uint64_t ts, ts_mask;

	if (!eth_rx_queue_info->ena_vector) {
		ts = m->ol_flags & event_eth_rx_timestamp_dynflag ?
						0 : rte_get_tsc_cycles();

		/* 0xffff ffff ffff ffff if RTE_MBUF_F_RX_TIMESTAMP is set,
		 * otherwise 0
		 */
		ts_mask = (uint64_t)(!(m->ol_flags &
				       event_eth_rx_timestamp_dynflag)) - 1ULL;

		/* 0xffff ffff if RTE_MBUF_F_RX_RSS_HASH is set, otherwise 0 */
		rss_mask = ~(((m->ol_flags & RTE_MBUF_F_RX_RSS_HASH) != 0) - 1);
		do_rss = !rss_mask && !eth_rx_queue_info->flow_id_mask;
		for (i = 0; i < num; i++) {
			struct rte_event *ev;

			m = mbufs[i];
			*rxa_timestamp_dynfield(m) = ts |
					(*rxa_timestamp_dynfield(m) & ts_mask);

			ev = &buf->events[new_tail];

			rss = do_rss ? rxa_do_softrss(m, rx_adapter->rss_key_be)
				     : m->hash.rss;
			ev->event = event;
			ev->flow_id = (rss & ~flow_id_mask) |
				      (ev->flow_id & flow_id_mask);
			ev->mbuf = m;
			new_tail++;
		}
	} else {
		num = rxa_create_event_vector(rx_adapter, eth_rx_queue_info,
					      buf, mbufs, num);
	}

	if (num && dev_info->cb_fn) {

		dropped = 0;
		nb_cb = dev_info->cb_fn(eth_dev_id, rx_queue_id,
				       buf->last |
				       (buf->events_size & ~buf->last_mask),
				       buf->count >= BATCH_SIZE ?
						buf->count - BATCH_SIZE : 0,
				       &buf->events[buf->tail],
				       num,
				       dev_info->cb_arg,
				       &dropped);
		if (unlikely(nb_cb > num))
			RTE_EDEV_LOG_ERR("Rx CB returned %d (> %d) events",
				nb_cb, num);
		else
			num = nb_cb;
		if (dropped)
			stats->rx_dropped += dropped;
	}

	buf->count += num;
	buf->tail += num;
}

static inline bool
rxa_pkt_buf_available(struct eth_event_enqueue_buffer *buf)
{
	uint32_t nb_req = buf->tail + BATCH_SIZE;

	if (!buf->last) {
		if (nb_req <= buf->events_size)
			return true;

		if (buf->head >= BATCH_SIZE) {
			buf->last_mask = ~0;
			buf->last = buf->tail;
			buf->tail = 0;
			return true;
		}
	}

	return nb_req <= buf->head;
}

/* Enqueue packets from  <port, q>  to event buffer */
static inline uint32_t
rxa_eth_rx(struct event_eth_rx_adapter *rx_adapter, uint16_t port_id,
	   uint16_t queue_id, uint32_t rx_count, uint32_t max_rx,
	   int *rxq_empty, struct eth_event_enqueue_buffer *buf,
	   struct rte_event_eth_rx_adapter_stats *stats)
{
	struct rte_mbuf *mbufs[BATCH_SIZE];
	uint16_t n;
	uint32_t nb_rx = 0;
	uint32_t nb_flushed = 0;

	if (rxq_empty)
		*rxq_empty = 0;
	/* Don't do a batch dequeue from the rx queue if there isn't
	 * enough space in the enqueue buffer.
	 */
	while (rxa_pkt_buf_available(buf)) {
		if (buf->count >= BATCH_SIZE)
			nb_flushed +=
				rxa_flush_event_buffer(rx_adapter, buf, stats);

		stats->rx_poll_count++;
		n = rte_eth_rx_burst(port_id, queue_id, mbufs, BATCH_SIZE);
		if (unlikely(!n)) {
			if (rxq_empty)
				*rxq_empty = 1;
			break;
		}
		rxa_buffer_mbufs(rx_adapter, port_id, queue_id, mbufs, n, buf,
				 stats);
		nb_rx += n;
		if (rx_count + nb_rx > max_rx)
			break;
	}

	if (buf->count > 0)
		nb_flushed += rxa_flush_event_buffer(rx_adapter, buf, stats);

	stats->rx_packets += nb_rx;
	if (nb_flushed == 0)
		rte_event_maintain(rx_adapter->eventdev_id,
				   rx_adapter->event_port_id, 0);

	return nb_rx;
}

static inline void
rxa_intr_ring_enqueue(struct event_eth_rx_adapter *rx_adapter, void *data)
{
	uint16_t port_id;
	uint16_t queue;
	int err;
	union queue_data qd;
	struct eth_device_info *dev_info;
	struct eth_rx_queue_info *queue_info;
	int *intr_enabled;

	qd.ptr = data;
	port_id = qd.port;
	queue = qd.queue;

	dev_info = &rx_adapter->eth_devices[port_id];
	queue_info = &dev_info->rx_queue[queue];
	rte_spinlock_lock(&rx_adapter->intr_ring_lock);
	if (rxa_shared_intr(dev_info, queue))
		intr_enabled = &dev_info->shared_intr_enabled;
	else
		intr_enabled = &queue_info->intr_enabled;

	if (*intr_enabled) {
		*intr_enabled = 0;
		err = rte_ring_enqueue(rx_adapter->intr_ring, data);
		/* Entry should always be available.
		 * The ring size equals the maximum number of interrupt
		 * vectors supported (an interrupt vector is shared in
		 * case of shared interrupts)
		 */
		if (err)
			RTE_EDEV_LOG_ERR("Failed to enqueue interrupt"
				" to ring: %s", strerror(-err));
		else
			rte_eth_dev_rx_intr_disable(port_id, queue);
	}
	rte_spinlock_unlock(&rx_adapter->intr_ring_lock);
}

static int
rxa_intr_ring_check_avail(struct event_eth_rx_adapter *rx_adapter,
			  uint32_t num_intr_vec)
{
	if (rx_adapter->num_intr_vec + num_intr_vec >
				RTE_EVENT_ETH_INTR_RING_SIZE) {
		RTE_EDEV_LOG_ERR("Exceeded intr ring slots current"
		" %d needed %d limit %d", rx_adapter->num_intr_vec,
		num_intr_vec, RTE_EVENT_ETH_INTR_RING_SIZE);
		return -ENOSPC;
	}

	return 0;
}

/* Delete entries for (dev, queue) from the interrupt ring */
static void
rxa_intr_ring_del_entries(struct event_eth_rx_adapter *rx_adapter,
			  struct eth_device_info *dev_info,
			  uint16_t rx_queue_id)
{
	int i, n;
	union queue_data qd;

	rte_spinlock_lock(&rx_adapter->intr_ring_lock);

	n = rte_ring_count(rx_adapter->intr_ring);
	for (i = 0; i < n; i++) {
		rte_ring_dequeue(rx_adapter->intr_ring, &qd.ptr);
		if (!rxa_shared_intr(dev_info, rx_queue_id)) {
			if (qd.port == dev_info->dev->data->port_id &&
				qd.queue == rx_queue_id)
				continue;
		} else {
			if (qd.port == dev_info->dev->data->port_id)
				continue;
		}
		rte_ring_enqueue(rx_adapter->intr_ring, qd.ptr);
	}

	rte_spinlock_unlock(&rx_adapter->intr_ring_lock);
}

/* pthread callback handling interrupt mode receive queues
 * After receiving an Rx interrupt, it enqueues the port id and queue id of the
 * interrupting queue to the adapter's ring buffer for interrupt events.
 * These events are picked up by rxa_intr_ring_dequeue() which is invoked from
 * the adapter service function.
 */
static void *
rxa_intr_thread(void *arg)
{
	struct event_eth_rx_adapter *rx_adapter = arg;
	struct rte_epoll_event *epoll_events = rx_adapter->epoll_events;
	int n, i;

	while (1) {
		n = rte_epoll_wait(rx_adapter->epd, epoll_events,
				RTE_EVENT_ETH_INTR_RING_SIZE, -1);
		if (unlikely(n < 0))
			RTE_EDEV_LOG_ERR("rte_epoll_wait returned error %d",
					n);
		for (i = 0; i < n; i++) {
			rxa_intr_ring_enqueue(rx_adapter,
					epoll_events[i].epdata.data);
		}
	}

	return NULL;
}

/* Dequeue <port, q> from interrupt ring and enqueue received
 * mbufs to eventdev
 */
static inline void
rxa_intr_ring_dequeue(struct event_eth_rx_adapter *rx_adapter)
{
	uint32_t n;
	uint32_t nb_rx = 0;
	int rxq_empty;
	struct eth_event_enqueue_buffer *buf;
	struct rte_event_eth_rx_adapter_stats *stats;
	rte_spinlock_t *ring_lock;
	uint8_t max_done = 0;

	if (rx_adapter->num_rx_intr == 0)
		return;

	if (rte_ring_count(rx_adapter->intr_ring) == 0
		&& !rx_adapter->qd_valid)
		return;

	buf = &rx_adapter->event_enqueue_buffer;
	stats = &rx_adapter->stats;
	ring_lock = &rx_adapter->intr_ring_lock;

	if (buf->count >= BATCH_SIZE)
		rxa_flush_event_buffer(rx_adapter, buf, stats);

	while (rxa_pkt_buf_available(buf)) {
		struct eth_device_info *dev_info;
		uint16_t port;
		uint16_t queue;
		union queue_data qd  = rx_adapter->qd;
		int err;

		if (!rx_adapter->qd_valid) {
			struct eth_rx_queue_info *queue_info;

			rte_spinlock_lock(ring_lock);
			err = rte_ring_dequeue(rx_adapter->intr_ring, &qd.ptr);
			if (err) {
				rte_spinlock_unlock(ring_lock);
				break;
			}

			port = qd.port;
			queue = qd.queue;
			rx_adapter->qd = qd;
			rx_adapter->qd_valid = 1;
			dev_info = &rx_adapter->eth_devices[port];
			if (rxa_shared_intr(dev_info, queue))
				dev_info->shared_intr_enabled = 1;
			else {
				queue_info = &dev_info->rx_queue[queue];
				queue_info->intr_enabled = 1;
			}
			rte_eth_dev_rx_intr_enable(port, queue);
			rte_spinlock_unlock(ring_lock);
		} else {
			port = qd.port;
			queue = qd.queue;

			dev_info = &rx_adapter->eth_devices[port];
		}

		if (rxa_shared_intr(dev_info, queue)) {
			uint16_t i;
			uint16_t nb_queues;

			nb_queues = dev_info->dev->data->nb_rx_queues;
			n = 0;
			for (i = dev_info->next_q_idx; i < nb_queues; i++) {
				uint8_t enq_buffer_full;

				if (!rxa_intr_queue(dev_info, i))
					continue;
				n = rxa_eth_rx(rx_adapter, port, i, nb_rx,
					rx_adapter->max_nb_rx,
					&rxq_empty, buf, stats);
				nb_rx += n;

				enq_buffer_full = !rxq_empty && n == 0;
				max_done = nb_rx > rx_adapter->max_nb_rx;

				if (enq_buffer_full || max_done) {
					dev_info->next_q_idx = i;
					goto done;
				}
			}

			rx_adapter->qd_valid = 0;

			/* Reinitialize for next interrupt */
			dev_info->next_q_idx = dev_info->multi_intr_cap ?
						RTE_MAX_RXTX_INTR_VEC_ID - 1 :
						0;
		} else {
			n = rxa_eth_rx(rx_adapter, port, queue, nb_rx,
				rx_adapter->max_nb_rx,
				&rxq_empty, buf, stats);
			rx_adapter->qd_valid = !rxq_empty;
			nb_rx += n;
			if (nb_rx > rx_adapter->max_nb_rx)
				break;
		}
	}

done:
	rx_adapter->stats.rx_intr_packets += nb_rx;
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
static inline void
rxa_poll(struct event_eth_rx_adapter *rx_adapter)
{
	uint32_t num_queue;
	uint32_t nb_rx = 0;
	struct eth_event_enqueue_buffer *buf = NULL;
	struct rte_event_eth_rx_adapter_stats *stats = NULL;
	uint32_t wrr_pos;
	uint32_t max_nb_rx;

	wrr_pos = rx_adapter->wrr_pos;
	max_nb_rx = rx_adapter->max_nb_rx;

	/* Iterate through a WRR sequence */
	for (num_queue = 0; num_queue < rx_adapter->wrr_len; num_queue++) {
		unsigned int poll_idx = rx_adapter->wrr_sched[wrr_pos];
		uint16_t qid = rx_adapter->eth_rx_poll[poll_idx].eth_rx_qid;
		uint16_t d = rx_adapter->eth_rx_poll[poll_idx].eth_dev_id;

		buf = rxa_event_buf_get(rx_adapter, d, qid, &stats);

		/* Don't do a batch dequeue from the rx queue if there isn't
		 * enough space in the enqueue buffer.
		 */
		if (buf->count >= BATCH_SIZE)
			rxa_flush_event_buffer(rx_adapter, buf, stats);
		if (!rxa_pkt_buf_available(buf)) {
			if (rx_adapter->use_queue_event_buf)
				goto poll_next_entry;
			else {
				rx_adapter->wrr_pos = wrr_pos;
				return;
			}
		}

		nb_rx += rxa_eth_rx(rx_adapter, d, qid, nb_rx, max_nb_rx,
				NULL, buf, stats);
		if (nb_rx > max_nb_rx) {
			rx_adapter->wrr_pos =
				    (wrr_pos + 1) % rx_adapter->wrr_len;
			break;
		}

poll_next_entry:
		if (++wrr_pos == rx_adapter->wrr_len)
			wrr_pos = 0;
	}
}

static void
rxa_vector_expire(struct eth_rx_vector_data *vec, void *arg)
{
	struct event_eth_rx_adapter *rx_adapter = arg;
	struct eth_event_enqueue_buffer *buf = NULL;
	struct rte_event_eth_rx_adapter_stats *stats = NULL;
	struct rte_event *ev;

	buf = rxa_event_buf_get(rx_adapter, vec->port, vec->queue, &stats);

	if (buf->count)
		rxa_flush_event_buffer(rx_adapter, buf, stats);

	if (vec->vector_ev->nb_elem == 0)
		return;
	ev = &buf->events[buf->count];

	/* Event ready. */
	ev->event = vec->event;
	ev->vec = vec->vector_ev;
	buf->count++;

	vec->vector_ev = NULL;
	vec->ts = 0;
}

static int
rxa_service_func(void *args)
{
	struct event_eth_rx_adapter *rx_adapter = args;

	if (rte_spinlock_trylock(&rx_adapter->rx_lock) == 0)
		return 0;
	if (!rx_adapter->rxa_started) {
		rte_spinlock_unlock(&rx_adapter->rx_lock);
		return 0;
	}

	if (rx_adapter->ena_vector) {
		if ((rte_rdtsc() - rx_adapter->prev_expiry_ts) >=
		    rx_adapter->vector_tmo_ticks) {
			struct eth_rx_vector_data *vec;

			TAILQ_FOREACH(vec, &rx_adapter->vector_list, next) {
				uint64_t elapsed_time = rte_rdtsc() - vec->ts;

				if (elapsed_time >= vec->vector_timeout_ticks) {
					rxa_vector_expire(vec, rx_adapter);
					TAILQ_REMOVE(&rx_adapter->vector_list,
						     vec, next);
				}
			}
			rx_adapter->prev_expiry_ts = rte_rdtsc();
		}
	}

	rxa_intr_ring_dequeue(rx_adapter);
	rxa_poll(rx_adapter);

	rte_spinlock_unlock(&rx_adapter->rx_lock);

	return 0;
}

static int
rte_event_eth_rx_adapter_init(void)
{
	const char *name = RXA_ADAPTER_ARRAY;
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

static int
rxa_memzone_lookup(void)
{
	const struct rte_memzone *mz;

	if (event_eth_rx_adapter == NULL) {
		mz = rte_memzone_lookup(RXA_ADAPTER_ARRAY);
		if (mz == NULL)
			return -ENOMEM;
		event_eth_rx_adapter = mz->addr;
	}

	return 0;
}

static inline struct event_eth_rx_adapter *
rxa_id_to_adapter(uint8_t id)
{
	return event_eth_rx_adapter ?
		event_eth_rx_adapter[id] : NULL;
}

static int
rxa_default_conf_cb(uint8_t id, uint8_t dev_id,
		struct rte_event_eth_rx_adapter_conf *conf, void *arg)
{
	int ret;
	struct rte_eventdev *dev;
	struct rte_event_dev_config dev_conf;
	int started;
	uint8_t port_id;
	struct rte_event_port_conf *port_conf = arg;
	struct event_eth_rx_adapter *rx_adapter = rxa_id_to_adapter(id);

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
rxa_epoll_create1(void)
{
#if defined(LINUX)
	int fd;
	fd = epoll_create1(EPOLL_CLOEXEC);
	return fd < 0 ? -errno : fd;
#elif defined(BSD)
	return -ENOTSUP;
#endif
}

static int
rxa_init_epd(struct event_eth_rx_adapter *rx_adapter)
{
	if (rx_adapter->epd != INIT_FD)
		return 0;

	rx_adapter->epd = rxa_epoll_create1();
	if (rx_adapter->epd < 0) {
		int err = rx_adapter->epd;
		rx_adapter->epd = INIT_FD;
		RTE_EDEV_LOG_ERR("epoll_create1() failed, err %d", err);
		return err;
	}

	return 0;
}

static int
rxa_create_intr_thread(struct event_eth_rx_adapter *rx_adapter)
{
	int err;
	char thread_name[RTE_MAX_THREAD_NAME_LEN];

	if (rx_adapter->intr_ring)
		return 0;

	rx_adapter->intr_ring = rte_ring_create("intr_ring",
					RTE_EVENT_ETH_INTR_RING_SIZE,
					rte_socket_id(), 0);
	if (!rx_adapter->intr_ring)
		return -ENOMEM;

	rx_adapter->epoll_events = rte_zmalloc_socket(rx_adapter->mem_name,
					RTE_EVENT_ETH_INTR_RING_SIZE *
					sizeof(struct rte_epoll_event),
					RTE_CACHE_LINE_SIZE,
					rx_adapter->socket_id);
	if (!rx_adapter->epoll_events) {
		err = -ENOMEM;
		goto error;
	}

	rte_spinlock_init(&rx_adapter->intr_ring_lock);

	snprintf(thread_name, RTE_MAX_THREAD_NAME_LEN,
			"rx-intr-thread-%d", rx_adapter->id);

	err = rte_ctrl_thread_create(&rx_adapter->rx_intr_thread, thread_name,
				NULL, rxa_intr_thread, rx_adapter);
	if (!err)
		return 0;

	RTE_EDEV_LOG_ERR("Failed to create interrupt thread err = %d\n", err);
	rte_free(rx_adapter->epoll_events);
error:
	rte_ring_free(rx_adapter->intr_ring);
	rx_adapter->intr_ring = NULL;
	rx_adapter->epoll_events = NULL;
	return err;
}

static int
rxa_destroy_intr_thread(struct event_eth_rx_adapter *rx_adapter)
{
	int err;

	err = pthread_cancel(rx_adapter->rx_intr_thread);
	if (err)
		RTE_EDEV_LOG_ERR("Can't cancel interrupt thread err = %d\n",
				err);

	err = pthread_join(rx_adapter->rx_intr_thread, NULL);
	if (err)
		RTE_EDEV_LOG_ERR("Can't join interrupt thread err = %d\n", err);

	rte_free(rx_adapter->epoll_events);
	rte_ring_free(rx_adapter->intr_ring);
	rx_adapter->intr_ring = NULL;
	rx_adapter->epoll_events = NULL;
	return 0;
}

static int
rxa_free_intr_resources(struct event_eth_rx_adapter *rx_adapter)
{
	int ret;

	if (rx_adapter->num_rx_intr == 0)
		return 0;

	ret = rxa_destroy_intr_thread(rx_adapter);
	if (ret)
		return ret;

	close(rx_adapter->epd);
	rx_adapter->epd = INIT_FD;

	return ret;
}

static int
rxa_disable_intr(struct event_eth_rx_adapter *rx_adapter,
		 struct eth_device_info *dev_info, uint16_t rx_queue_id)
{
	int err;
	uint16_t eth_dev_id = dev_info->dev->data->port_id;
	int sintr = rxa_shared_intr(dev_info, rx_queue_id);

	err = rte_eth_dev_rx_intr_disable(eth_dev_id, rx_queue_id);
	if (err) {
		RTE_EDEV_LOG_ERR("Could not disable interrupt for Rx queue %u",
			rx_queue_id);
		return err;
	}

	err = rte_eth_dev_rx_intr_ctl_q(eth_dev_id, rx_queue_id,
					rx_adapter->epd,
					RTE_INTR_EVENT_DEL,
					0);
	if (err)
		RTE_EDEV_LOG_ERR("Interrupt event deletion failed %d", err);

	if (sintr)
		dev_info->rx_queue[rx_queue_id].intr_enabled = 0;
	else
		dev_info->shared_intr_enabled = 0;
	return err;
}

static int
rxa_del_intr_queue(struct event_eth_rx_adapter *rx_adapter,
		   struct eth_device_info *dev_info, int rx_queue_id)
{
	int err;
	int i;
	int s;

	if (dev_info->nb_rx_intr == 0)
		return 0;

	err = 0;
	if (rx_queue_id == -1) {
		s = dev_info->nb_shared_intr;
		for (i = 0; i < dev_info->nb_rx_intr; i++) {
			int sintr;
			uint16_t q;

			q = dev_info->intr_queue[i];
			sintr = rxa_shared_intr(dev_info, q);
			s -= sintr;

			if (!sintr || s == 0) {

				err = rxa_disable_intr(rx_adapter, dev_info,
						q);
				if (err)
					return err;
				rxa_intr_ring_del_entries(rx_adapter, dev_info,
							q);
			}
		}
	} else {
		if (!rxa_intr_queue(dev_info, rx_queue_id))
			return 0;
		if (!rxa_shared_intr(dev_info, rx_queue_id) ||
				dev_info->nb_shared_intr == 1) {
			err = rxa_disable_intr(rx_adapter, dev_info,
					rx_queue_id);
			if (err)
				return err;
			rxa_intr_ring_del_entries(rx_adapter, dev_info,
						rx_queue_id);
		}

		for (i = 0; i < dev_info->nb_rx_intr; i++) {
			if (dev_info->intr_queue[i] == rx_queue_id) {
				for (; i < dev_info->nb_rx_intr - 1; i++)
					dev_info->intr_queue[i] =
						dev_info->intr_queue[i + 1];
				break;
			}
		}
	}

	return err;
}

static int
rxa_config_intr(struct event_eth_rx_adapter *rx_adapter,
		struct eth_device_info *dev_info, uint16_t rx_queue_id)
{
	int err, err1;
	uint16_t eth_dev_id = dev_info->dev->data->port_id;
	union queue_data qd;
	int init_fd;
	uint16_t *intr_queue;
	int sintr = rxa_shared_intr(dev_info, rx_queue_id);

	if (rxa_intr_queue(dev_info, rx_queue_id))
		return 0;

	intr_queue = dev_info->intr_queue;
	if (dev_info->intr_queue == NULL) {
		size_t len =
			dev_info->dev->data->nb_rx_queues * sizeof(uint16_t);
		dev_info->intr_queue =
			rte_zmalloc_socket(
				rx_adapter->mem_name,
				len,
				0,
				rx_adapter->socket_id);
		if (dev_info->intr_queue == NULL)
			return -ENOMEM;
	}

	init_fd = rx_adapter->epd;
	err = rxa_init_epd(rx_adapter);
	if (err)
		goto err_free_queue;

	qd.port = eth_dev_id;
	qd.queue = rx_queue_id;

	err = rte_eth_dev_rx_intr_ctl_q(eth_dev_id, rx_queue_id,
					rx_adapter->epd,
					RTE_INTR_EVENT_ADD,
					qd.ptr);
	if (err) {
		RTE_EDEV_LOG_ERR("Failed to add interrupt event for"
			" Rx Queue %u err %d", rx_queue_id, err);
		goto err_del_fd;
	}

	err = rte_eth_dev_rx_intr_enable(eth_dev_id, rx_queue_id);
	if (err) {
		RTE_EDEV_LOG_ERR("Could not enable interrupt for"
				" Rx Queue %u err %d", rx_queue_id, err);

		goto err_del_event;
	}

	err = rxa_create_intr_thread(rx_adapter);
	if (!err)  {
		if (sintr)
			dev_info->shared_intr_enabled = 1;
		else
			dev_info->rx_queue[rx_queue_id].intr_enabled = 1;
		return 0;
	}


	err = rte_eth_dev_rx_intr_disable(eth_dev_id, rx_queue_id);
	if (err)
		RTE_EDEV_LOG_ERR("Could not disable interrupt for"
				" Rx Queue %u err %d", rx_queue_id, err);
err_del_event:
	err1 = rte_eth_dev_rx_intr_ctl_q(eth_dev_id, rx_queue_id,
					rx_adapter->epd,
					RTE_INTR_EVENT_DEL,
					0);
	if (err1) {
		RTE_EDEV_LOG_ERR("Could not delete event for"
				" Rx Queue %u err %d", rx_queue_id, err1);
	}
err_del_fd:
	if (init_fd == INIT_FD) {
		close(rx_adapter->epd);
		rx_adapter->epd = -1;
	}
err_free_queue:
	if (intr_queue == NULL)
		rte_free(dev_info->intr_queue);

	return err;
}

static int
rxa_add_intr_queue(struct event_eth_rx_adapter *rx_adapter,
		   struct eth_device_info *dev_info, int rx_queue_id)

{
	int i, j, err;
	int si = -1;
	int shared_done = (dev_info->nb_shared_intr > 0);

	if (rx_queue_id != -1) {
		if (rxa_shared_intr(dev_info, rx_queue_id) && shared_done)
			return 0;
		return rxa_config_intr(rx_adapter, dev_info, rx_queue_id);
	}

	err = 0;
	for (i = 0; i < dev_info->dev->data->nb_rx_queues; i++) {

		if (rxa_shared_intr(dev_info, i) && shared_done)
			continue;

		err = rxa_config_intr(rx_adapter, dev_info, i);

		shared_done = err == 0 && rxa_shared_intr(dev_info, i);
		if (shared_done) {
			si = i;
			dev_info->shared_intr_enabled = 1;
		}
		if (err)
			break;
	}

	if (err == 0)
		return 0;

	shared_done = (dev_info->nb_shared_intr > 0);
	for (j = 0; j < i; j++) {
		if (rxa_intr_queue(dev_info, j))
			continue;
		if (rxa_shared_intr(dev_info, j) && si != j)
			continue;
		err = rxa_disable_intr(rx_adapter, dev_info, j);
		if (err)
			break;

	}

	return err;
}

static int
rxa_init_service(struct event_eth_rx_adapter *rx_adapter, uint8_t id)
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
	service.callback = rxa_service_func;
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
	rx_adapter->epd = INIT_FD;
	return 0;

err_done:
	rte_service_component_unregister(rx_adapter->service_id);
	return ret;
}

static void
rxa_update_queue(struct event_eth_rx_adapter *rx_adapter,
		 struct eth_device_info *dev_info, int32_t rx_queue_id,
		 uint8_t add)
{
	struct eth_rx_queue_info *queue_info;
	int enabled;
	uint16_t i;

	if (dev_info->rx_queue == NULL)
		return;

	if (rx_queue_id == -1) {
		for (i = 0; i < dev_info->dev->data->nb_rx_queues; i++)
			rxa_update_queue(rx_adapter, dev_info, i, add);
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

static void
rxa_set_vector_data(struct eth_rx_queue_info *queue_info, uint16_t vector_count,
		    uint64_t vector_ns, struct rte_mempool *mp, uint32_t qid,
		    uint16_t port_id)
{
#define NSEC2TICK(__ns, __freq) (((__ns) * (__freq)) / 1E9)
	struct eth_rx_vector_data *vector_data;
	uint32_t flow_id;

	vector_data = &queue_info->vector_data;
	vector_data->max_vector_count = vector_count;
	vector_data->port = port_id;
	vector_data->queue = qid;
	vector_data->vector_pool = mp;
	vector_data->vector_timeout_ticks =
		NSEC2TICK(vector_ns, rte_get_timer_hz());
	vector_data->ts = 0;
	flow_id = queue_info->event & 0xFFFFF;
	flow_id =
		flow_id == 0 ? (qid & 0xFFF) | (port_id & 0xFF) << 12 : flow_id;
	vector_data->event = (queue_info->event & ~0xFFFFF) | flow_id;
}

static void
rxa_sw_del(struct event_eth_rx_adapter *rx_adapter,
	   struct eth_device_info *dev_info, int32_t rx_queue_id)
{
	struct eth_rx_vector_data *vec;
	int pollq;
	int intrq;
	int sintrq;


	if (rx_adapter->nb_queues == 0)
		return;

	if (rx_queue_id == -1) {
		uint16_t nb_rx_queues;
		uint16_t i;

		nb_rx_queues = dev_info->dev->data->nb_rx_queues;
		for (i = 0; i <	nb_rx_queues; i++)
			rxa_sw_del(rx_adapter, dev_info, i);
		return;
	}

	/* Push all the partial event vectors to event device. */
	TAILQ_FOREACH(vec, &rx_adapter->vector_list, next) {
		if (vec->queue != rx_queue_id)
			continue;
		rxa_vector_expire(vec, rx_adapter);
		TAILQ_REMOVE(&rx_adapter->vector_list, vec, next);
	}

	pollq = rxa_polled_queue(dev_info, rx_queue_id);
	intrq = rxa_intr_queue(dev_info, rx_queue_id);
	sintrq = rxa_shared_intr(dev_info, rx_queue_id);
	rxa_update_queue(rx_adapter, dev_info, rx_queue_id, 0);
	rx_adapter->num_rx_polled -= pollq;
	dev_info->nb_rx_poll -= pollq;
	rx_adapter->num_rx_intr -= intrq;
	dev_info->nb_rx_intr -= intrq;
	dev_info->nb_shared_intr -= intrq && sintrq;
	if (rx_adapter->use_queue_event_buf) {
		struct eth_event_enqueue_buffer *event_buf =
			dev_info->rx_queue[rx_queue_id].event_buf;
		struct rte_event_eth_rx_adapter_stats *stats =
			dev_info->rx_queue[rx_queue_id].stats;
		rte_free(event_buf->events);
		rte_free(event_buf);
		rte_free(stats);
		dev_info->rx_queue[rx_queue_id].event_buf = NULL;
		dev_info->rx_queue[rx_queue_id].stats = NULL;
	}
}

static int
rxa_add_queue(struct event_eth_rx_adapter *rx_adapter,
	      struct eth_device_info *dev_info, int32_t rx_queue_id,
	      const struct rte_event_eth_rx_adapter_queue_conf *conf)
{
	struct eth_rx_queue_info *queue_info;
	const struct rte_event *ev = &conf->ev;
	int pollq;
	int intrq;
	int sintrq;
	struct rte_event *qi_ev;
	struct eth_event_enqueue_buffer *new_rx_buf = NULL;
	struct rte_event_eth_rx_adapter_stats *stats = NULL;
	uint16_t eth_dev_id = dev_info->dev->data->port_id;
	int ret;

	if (rx_queue_id == -1) {
		uint16_t nb_rx_queues;
		uint16_t i;

		nb_rx_queues = dev_info->dev->data->nb_rx_queues;
		for (i = 0; i <	nb_rx_queues; i++) {
			ret = rxa_add_queue(rx_adapter, dev_info, i, conf);
			if (ret)
				return ret;
		}
		return 0;
	}

	pollq = rxa_polled_queue(dev_info, rx_queue_id);
	intrq = rxa_intr_queue(dev_info, rx_queue_id);
	sintrq = rxa_shared_intr(dev_info, rx_queue_id);

	queue_info = &dev_info->rx_queue[rx_queue_id];
	queue_info->wt = conf->servicing_weight;

	qi_ev = (struct rte_event *)&queue_info->event;
	qi_ev->event = ev->event;
	qi_ev->op = RTE_EVENT_OP_NEW;
	qi_ev->event_type = RTE_EVENT_TYPE_ETH_RX_ADAPTER;
	qi_ev->sub_event_type = 0;

	if (conf->rx_queue_flags &
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID) {
		queue_info->flow_id_mask = ~0;
	} else
		qi_ev->flow_id = 0;

	if (conf->rx_queue_flags &
	    RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR) {
		queue_info->ena_vector = 1;
		qi_ev->event_type = RTE_EVENT_TYPE_ETH_RX_ADAPTER_VECTOR;
		rxa_set_vector_data(queue_info, conf->vector_sz,
				    conf->vector_timeout_ns, conf->vector_mp,
				    rx_queue_id, dev_info->dev->data->port_id);
		rx_adapter->ena_vector = 1;
		rx_adapter->vector_tmo_ticks =
			rx_adapter->vector_tmo_ticks ?
				      RTE_MIN(queue_info->vector_data
							.vector_timeout_ticks >>
						1,
					rx_adapter->vector_tmo_ticks) :
				queue_info->vector_data.vector_timeout_ticks >>
					1;
	}

	rxa_update_queue(rx_adapter, dev_info, rx_queue_id, 1);
	if (rxa_polled_queue(dev_info, rx_queue_id)) {
		rx_adapter->num_rx_polled += !pollq;
		dev_info->nb_rx_poll += !pollq;
		rx_adapter->num_rx_intr -= intrq;
		dev_info->nb_rx_intr -= intrq;
		dev_info->nb_shared_intr -= intrq && sintrq;
	}

	if (rxa_intr_queue(dev_info, rx_queue_id)) {
		rx_adapter->num_rx_polled -= pollq;
		dev_info->nb_rx_poll -= pollq;
		rx_adapter->num_rx_intr += !intrq;
		dev_info->nb_rx_intr += !intrq;
		dev_info->nb_shared_intr += !intrq && sintrq;
		if (dev_info->nb_shared_intr == 1) {
			if (dev_info->multi_intr_cap)
				dev_info->next_q_idx =
					RTE_MAX_RXTX_INTR_VEC_ID - 1;
			else
				dev_info->next_q_idx = 0;
		}
	}

	if (!rx_adapter->use_queue_event_buf)
		return 0;

	new_rx_buf = rte_zmalloc_socket("rx_buffer_meta",
				sizeof(*new_rx_buf), 0,
				rte_eth_dev_socket_id(eth_dev_id));
	if (new_rx_buf == NULL) {
		RTE_EDEV_LOG_ERR("Failed to allocate event buffer meta for "
				 "dev_id: %d queue_id: %d",
				 eth_dev_id, rx_queue_id);
		return -ENOMEM;
	}

	new_rx_buf->events_size = RTE_ALIGN(conf->event_buf_size, BATCH_SIZE);
	new_rx_buf->events_size += (2 * BATCH_SIZE);
	new_rx_buf->events = rte_zmalloc_socket("rx_buffer",
				sizeof(struct rte_event) *
				new_rx_buf->events_size, 0,
				rte_eth_dev_socket_id(eth_dev_id));
	if (new_rx_buf->events == NULL) {
		rte_free(new_rx_buf);
		RTE_EDEV_LOG_ERR("Failed to allocate event buffer for "
				 "dev_id: %d queue_id: %d",
				 eth_dev_id, rx_queue_id);
		return -ENOMEM;
	}

	queue_info->event_buf = new_rx_buf;

	/* Allocate storage for adapter queue stats */
	stats = rte_zmalloc_socket("rx_queue_stats",
				sizeof(*stats), 0,
				rte_eth_dev_socket_id(eth_dev_id));
	if (stats == NULL) {
		rte_free(new_rx_buf->events);
		rte_free(new_rx_buf);
		RTE_EDEV_LOG_ERR("Failed to allocate stats storage for"
				 " dev_id: %d queue_id: %d",
				 eth_dev_id, rx_queue_id);
		return -ENOMEM;
	}

	queue_info->stats = stats;

	return 0;
}

static int
rxa_sw_add(struct event_eth_rx_adapter *rx_adapter, uint16_t eth_dev_id,
	   int rx_queue_id,
	   const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct eth_device_info *dev_info = &rx_adapter->eth_devices[eth_dev_id];
	struct rte_event_eth_rx_adapter_queue_conf temp_conf;
	int ret;
	struct eth_rx_poll_entry *rx_poll;
	struct eth_rx_queue_info *rx_queue;
	uint32_t *rx_wrr;
	uint16_t nb_rx_queues;
	uint32_t nb_rx_poll, nb_wrr;
	uint32_t nb_rx_intr;
	int num_intr_vec;
	uint16_t wt;

	if (queue_conf->servicing_weight == 0) {
		struct rte_eth_dev_data *data = dev_info->dev->data;

		temp_conf = *queue_conf;
		if (!data->dev_conf.intr_conf.rxq) {
			/* If Rx interrupts are disabled set wt = 1 */
			temp_conf.servicing_weight = 1;
		}
		queue_conf = &temp_conf;

		if (queue_conf->servicing_weight == 0 &&
		    rx_adapter->use_queue_event_buf) {

			RTE_EDEV_LOG_ERR("Use of queue level event buffer "
					 "not supported for interrupt queues "
					 "dev_id: %d queue_id: %d",
					 eth_dev_id, rx_queue_id);
			return -EINVAL;
		}
	}

	nb_rx_queues = dev_info->dev->data->nb_rx_queues;
	rx_queue = dev_info->rx_queue;
	wt = queue_conf->servicing_weight;

	if (dev_info->rx_queue == NULL) {
		dev_info->rx_queue =
		    rte_zmalloc_socket(rx_adapter->mem_name,
				       nb_rx_queues *
				       sizeof(struct eth_rx_queue_info), 0,
				       rx_adapter->socket_id);
		if (dev_info->rx_queue == NULL)
			return -ENOMEM;
	}
	rx_wrr = NULL;
	rx_poll = NULL;

	rxa_calc_nb_post_add(rx_adapter, dev_info, rx_queue_id,
			queue_conf->servicing_weight,
			&nb_rx_poll, &nb_rx_intr, &nb_wrr);

	if (dev_info->dev->intr_handle)
		dev_info->multi_intr_cap =
			rte_intr_cap_multiple(dev_info->dev->intr_handle);

	ret = rxa_alloc_poll_arrays(rx_adapter, nb_rx_poll, nb_wrr,
				&rx_poll, &rx_wrr);
	if (ret)
		goto err_free_rxqueue;

	if (wt == 0) {
		num_intr_vec = rxa_nb_intr_vect(dev_info, rx_queue_id, 1);

		ret = rxa_intr_ring_check_avail(rx_adapter, num_intr_vec);
		if (ret)
			goto err_free_rxqueue;

		ret = rxa_add_intr_queue(rx_adapter, dev_info, rx_queue_id);
		if (ret)
			goto err_free_rxqueue;
	} else {

		num_intr_vec = 0;
		if (rx_adapter->num_rx_intr > nb_rx_intr) {
			num_intr_vec = rxa_nb_intr_vect(dev_info,
						rx_queue_id, 0);
			/* interrupt based queues are being converted to
			 * poll mode queues, delete the interrupt configuration
			 * for those.
			 */
			ret = rxa_del_intr_queue(rx_adapter,
						dev_info, rx_queue_id);
			if (ret)
				goto err_free_rxqueue;
		}
	}

	if (nb_rx_intr == 0) {
		ret = rxa_free_intr_resources(rx_adapter);
		if (ret)
			goto err_free_rxqueue;
	}

	if (wt == 0) {
		uint16_t i;

		if (rx_queue_id  == -1) {
			for (i = 0; i < dev_info->dev->data->nb_rx_queues; i++)
				dev_info->intr_queue[i] = i;
		} else {
			if (!rxa_intr_queue(dev_info, rx_queue_id))
				dev_info->intr_queue[nb_rx_intr - 1] =
					rx_queue_id;
		}
	}



	ret = rxa_add_queue(rx_adapter, dev_info, rx_queue_id, queue_conf);
	if (ret)
		goto err_free_rxqueue;
	rxa_calc_wrr_sequence(rx_adapter, rx_poll, rx_wrr);

	rte_free(rx_adapter->eth_rx_poll);
	rte_free(rx_adapter->wrr_sched);

	rx_adapter->eth_rx_poll = rx_poll;
	rx_adapter->wrr_sched = rx_wrr;
	rx_adapter->wrr_len = nb_wrr;
	rx_adapter->num_intr_vec += num_intr_vec;
	return 0;

err_free_rxqueue:
	if (rx_queue == NULL) {
		rte_free(dev_info->rx_queue);
		dev_info->rx_queue = NULL;
	}

	rte_free(rx_poll);
	rte_free(rx_wrr);

	return ret;
}

static int
rxa_ctrl(uint8_t id, int start)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	uint32_t i;
	int use_service = 0;
	int stop = !start;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	rx_adapter = rxa_id_to_adapter(id);
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

	if (use_service) {
		rte_spinlock_lock(&rx_adapter->rx_lock);
		rx_adapter->rxa_started = start;
		rte_service_runstate_set(rx_adapter->service_id, start);
		rte_spinlock_unlock(&rx_adapter->rx_lock);
	}

	return 0;
}

static int
rxa_create(uint8_t id, uint8_t dev_id,
	   struct rte_event_eth_rx_adapter_params *rxa_params,
	   rte_event_eth_rx_adapter_conf_cb conf_cb,
	   void *conf_arg)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_event_enqueue_buffer *buf;
	struct rte_event *events;
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

	rx_adapter = rxa_id_to_adapter(id);
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
	rx_adapter->id = id;
	TAILQ_INIT(&rx_adapter->vector_list);
	strcpy(rx_adapter->mem_name, mem_name);
	rx_adapter->eth_devices = rte_zmalloc_socket(rx_adapter->mem_name,
					RTE_MAX_ETHPORTS *
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

	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		rx_adapter->eth_devices[i].dev = &rte_eth_devices[i];

	/* Rx adapter event buffer allocation */
	rx_adapter->use_queue_event_buf = rxa_params->use_queue_event_buf;

	if (!rx_adapter->use_queue_event_buf) {
		buf = &rx_adapter->event_enqueue_buffer;
		buf->events_size = rxa_params->event_buf_size;

		events = rte_zmalloc_socket(rx_adapter->mem_name,
					    buf->events_size * sizeof(*events),
					    0, socket_id);
		if (events == NULL) {
			RTE_EDEV_LOG_ERR("Failed to allocate memory "
					 "for adapter event buffer");
			rte_free(rx_adapter->eth_devices);
			rte_free(rx_adapter);
			return -ENOMEM;
		}

		rx_adapter->event_enqueue_buffer.events = events;
	}

	event_eth_rx_adapter[id] = rx_adapter;

	if (conf_cb == rxa_default_conf_cb)
		rx_adapter->default_cb_arg = 1;

	if (rte_mbuf_dyn_rx_timestamp_register(
			&event_eth_rx_timestamp_dynfield_offset,
			&event_eth_rx_timestamp_dynflag) != 0) {
		RTE_EDEV_LOG_ERR("Error registering timestamp field in mbuf\n");
		return -rte_errno;
	}

	rte_eventdev_trace_eth_rx_adapter_create(id, dev_id, conf_cb,
		conf_arg);
	return 0;
}

int
rte_event_eth_rx_adapter_create_ext(uint8_t id, uint8_t dev_id,
				rte_event_eth_rx_adapter_conf_cb conf_cb,
				void *conf_arg)
{
	struct rte_event_eth_rx_adapter_params rxa_params = {0};

	/* use default values for adapter params */
	rxa_params.event_buf_size = ETH_EVENT_BUFFER_SIZE;
	rxa_params.use_queue_event_buf = false;

	return rxa_create(id, dev_id, &rxa_params, conf_cb, conf_arg);
}

int
rte_event_eth_rx_adapter_create_with_params(uint8_t id, uint8_t dev_id,
			struct rte_event_port_conf *port_config,
			struct rte_event_eth_rx_adapter_params *rxa_params)
{
	struct rte_event_port_conf *pc;
	int ret;
	struct rte_event_eth_rx_adapter_params temp_params = {0};

	if (port_config == NULL)
		return -EINVAL;

	if (rxa_params == NULL) {
		/* use default values if rxa_params is NULL */
		rxa_params = &temp_params;
		rxa_params->event_buf_size = ETH_EVENT_BUFFER_SIZE;
		rxa_params->use_queue_event_buf = false;
	} else if ((!rxa_params->use_queue_event_buf &&
		    rxa_params->event_buf_size == 0) ||
		   (rxa_params->use_queue_event_buf &&
		    rxa_params->event_buf_size != 0)) {
		RTE_EDEV_LOG_ERR("Invalid adapter params\n");
		return -EINVAL;
	} else if (!rxa_params->use_queue_event_buf) {
		/* adjust event buff size with BATCH_SIZE used for fetching
		 * packets from NIC rx queues to get full buffer utilization
		 * and prevent unnecessary rollovers.
		 */

		rxa_params->event_buf_size =
			RTE_ALIGN(rxa_params->event_buf_size, BATCH_SIZE);
		rxa_params->event_buf_size += (BATCH_SIZE + BATCH_SIZE);
	}

	pc = rte_malloc(NULL, sizeof(*pc), 0);
	if (pc == NULL)
		return -ENOMEM;

	*pc = *port_config;

	ret = rxa_create(id, dev_id, rxa_params, rxa_default_conf_cb, pc);
	if (ret)
		rte_free(pc);

	return ret;
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
					rxa_default_conf_cb,
					pc);
	if (ret)
		rte_free(pc);
	return ret;
}

int
rte_event_eth_rx_adapter_free(uint8_t id)
{
	struct event_eth_rx_adapter *rx_adapter;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
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
	if (!rx_adapter->use_queue_event_buf)
		rte_free(rx_adapter->event_enqueue_buffer.events);
	rte_free(rx_adapter);
	event_eth_rx_adapter[id] = NULL;

	rte_eventdev_trace_eth_rx_adapter_free(id);
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
	struct event_eth_rx_adapter *rx_adapter;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	struct rte_event_eth_rx_adapter_vector_limits limits;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
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

	if (queue_conf->rx_queue_flags &
	    RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR) {

		if ((cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_EVENT_VECTOR) == 0) {
			RTE_EDEV_LOG_ERR("Event vectorization is not supported,"
					 " eth port: %" PRIu16
					 " adapter id: %" PRIu8,
					 eth_dev_id, id);
			return -EINVAL;
		}

		ret = rte_event_eth_rx_adapter_vector_limits_get(
			rx_adapter->eventdev_id, eth_dev_id, &limits);
		if (ret < 0) {
			RTE_EDEV_LOG_ERR("Failed to get event device vector limits,"
					 " eth port: %" PRIu16
					 " adapter id: %" PRIu8,
					 eth_dev_id, id);
			return -EINVAL;
		}
		if (queue_conf->vector_sz < limits.min_sz ||
		    queue_conf->vector_sz > limits.max_sz ||
		    queue_conf->vector_timeout_ns < limits.min_timeout_ns ||
		    queue_conf->vector_timeout_ns > limits.max_timeout_ns ||
		    queue_conf->vector_mp == NULL) {
			RTE_EDEV_LOG_ERR("Invalid event vector configuration,"
					 " eth port: %" PRIu16
					 " adapter id: %" PRIu8,
					 eth_dev_id, id);
			return -EINVAL;
		}
		if (queue_conf->vector_mp->elt_size <
		    (sizeof(struct rte_event_vector) +
		     (sizeof(uintptr_t) * queue_conf->vector_sz))) {
			RTE_EDEV_LOG_ERR("Invalid event vector configuration,"
					 " eth port: %" PRIu16
					 " adapter id: %" PRIu8,
					 eth_dev_id, id);
			return -EINVAL;
		}
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

	if ((rx_adapter->use_queue_event_buf &&
	     queue_conf->event_buf_size == 0) ||
	    (!rx_adapter->use_queue_event_buf &&
	     queue_conf->event_buf_size != 0)) {
		RTE_EDEV_LOG_ERR("Invalid Event buffer size for the queue");
		return -EINVAL;
	}

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
			dev_info->internal_event_port = 1;
			rxa_update_queue(rx_adapter,
					&rx_adapter->eth_devices[eth_dev_id],
					rx_queue_id,
					1);
		}
	} else {
		rte_spinlock_lock(&rx_adapter->rx_lock);
		dev_info->internal_event_port = 0;
		ret = rxa_init_service(rx_adapter, id);
		if (ret == 0) {
			uint32_t service_id = rx_adapter->service_id;
			ret = rxa_sw_add(rx_adapter, eth_dev_id, rx_queue_id,
					queue_conf);
			rte_service_component_runstate_set(service_id,
				rxa_sw_adapter_queue_count(rx_adapter));
		}
		rte_spinlock_unlock(&rx_adapter->rx_lock);
	}

	rte_eventdev_trace_eth_rx_adapter_queue_add(id, eth_dev_id,
		rx_queue_id, queue_conf, ret);
	if (ret)
		return ret;

	return 0;
}

static int
rxa_sw_vector_limits(struct rte_event_eth_rx_adapter_vector_limits *limits)
{
	limits->max_sz = MAX_VECTOR_SIZE;
	limits->min_sz = MIN_VECTOR_SIZE;
	limits->max_timeout_ns = MAX_VECTOR_NS;
	limits->min_timeout_ns = MIN_VECTOR_NS;

	return 0;
}

int
rte_event_eth_rx_adapter_queue_del(uint8_t id, uint16_t eth_dev_id,
				int32_t rx_queue_id)
{
	int ret = 0;
	struct rte_eventdev *dev;
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_device_info *dev_info;
	uint32_t cap;
	uint32_t nb_rx_poll = 0;
	uint32_t nb_wrr = 0;
	uint32_t nb_rx_intr;
	struct eth_rx_poll_entry *rx_poll = NULL;
	uint32_t *rx_wrr = NULL;
	int num_intr_vec;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
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
			rxa_update_queue(rx_adapter,
					&rx_adapter->eth_devices[eth_dev_id],
					rx_queue_id,
					0);
			if (dev_info->nb_dev_queues == 0) {
				rte_free(dev_info->rx_queue);
				dev_info->rx_queue = NULL;
			}
		}
	} else {
		rxa_calc_nb_post_del(rx_adapter, dev_info, rx_queue_id,
			&nb_rx_poll, &nb_rx_intr, &nb_wrr);

		ret = rxa_alloc_poll_arrays(rx_adapter, nb_rx_poll, nb_wrr,
			&rx_poll, &rx_wrr);
		if (ret)
			return ret;

		rte_spinlock_lock(&rx_adapter->rx_lock);

		num_intr_vec = 0;
		if (rx_adapter->num_rx_intr > nb_rx_intr) {

			num_intr_vec = rxa_nb_intr_vect(dev_info,
						rx_queue_id, 0);
			ret = rxa_del_intr_queue(rx_adapter, dev_info,
					rx_queue_id);
			if (ret)
				goto unlock_ret;
		}

		if (nb_rx_intr == 0) {
			ret = rxa_free_intr_resources(rx_adapter);
			if (ret)
				goto unlock_ret;
		}

		rxa_sw_del(rx_adapter, dev_info, rx_queue_id);
		rxa_calc_wrr_sequence(rx_adapter, rx_poll, rx_wrr);

		rte_free(rx_adapter->eth_rx_poll);
		rte_free(rx_adapter->wrr_sched);

		if (nb_rx_intr == 0) {
			rte_free(dev_info->intr_queue);
			dev_info->intr_queue = NULL;
		}

		rx_adapter->eth_rx_poll = rx_poll;
		rx_adapter->wrr_sched = rx_wrr;
		rx_adapter->wrr_len = nb_wrr;
		/*
		 * reset next poll start position (wrr_pos) to avoid buffer
		 * overrun when wrr_len is reduced in case of queue delete
		 */
		rx_adapter->wrr_pos = 0;
		rx_adapter->num_intr_vec += num_intr_vec;

		if (dev_info->nb_dev_queues == 0) {
			rte_free(dev_info->rx_queue);
			dev_info->rx_queue = NULL;
		}
unlock_ret:
		rte_spinlock_unlock(&rx_adapter->rx_lock);
		if (ret) {
			rte_free(rx_poll);
			rte_free(rx_wrr);
			return ret;
		}

		rte_service_component_runstate_set(rx_adapter->service_id,
				rxa_sw_adapter_queue_count(rx_adapter));
	}

	rte_eventdev_trace_eth_rx_adapter_queue_del(id, eth_dev_id,
		rx_queue_id, ret);
	return ret;
}

int
rte_event_eth_rx_adapter_vector_limits_get(
	uint8_t dev_id, uint16_t eth_port_id,
	struct rte_event_eth_rx_adapter_vector_limits *limits)
{
	struct rte_eventdev *dev;
	uint32_t cap;
	int ret;

	RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_port_id, -EINVAL);

	if (limits == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[dev_id];

	ret = rte_event_eth_rx_adapter_caps_get(dev_id, eth_port_id, &cap);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps edev %" PRIu8
				 "eth port %" PRIu16,
				 dev_id, eth_port_id);
		return ret;
	}

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT) {
		RTE_FUNC_PTR_OR_ERR_RET(
			*dev->dev_ops->eth_rx_adapter_vector_limits_get,
			-ENOTSUP);
		ret = dev->dev_ops->eth_rx_adapter_vector_limits_get(
			dev, &rte_eth_devices[eth_port_id], limits);
	} else {
		ret = rxa_sw_vector_limits(limits);
	}

	return ret;
}

int
rte_event_eth_rx_adapter_start(uint8_t id)
{
	rte_eventdev_trace_eth_rx_adapter_start(id);
	return rxa_ctrl(id, 1);
}

int
rte_event_eth_rx_adapter_stop(uint8_t id)
{
	rte_eventdev_trace_eth_rx_adapter_stop(id);
	return rxa_ctrl(id, 0);
}

static inline void
rxa_queue_stats_reset(struct eth_rx_queue_info *queue_info)
{
	struct rte_event_eth_rx_adapter_stats *q_stats;

	q_stats = queue_info->stats;
	memset(q_stats, 0, sizeof(*q_stats));
}

int
rte_event_eth_rx_adapter_stats_get(uint8_t id,
			       struct rte_event_eth_rx_adapter_stats *stats)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_event_enqueue_buffer *buf;
	struct rte_event_eth_rx_adapter_stats dev_stats_sum = { 0 };
	struct rte_event_eth_rx_adapter_stats dev_stats;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	struct eth_rx_queue_info *queue_info;
	struct rte_event_eth_rx_adapter_stats *q_stats;
	uint32_t i, j;
	int ret;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter  == NULL || stats == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	memset(stats, 0, sizeof(*stats));

	if (rx_adapter->service_inited)
		*stats = rx_adapter->stats;

	RTE_ETH_FOREACH_DEV(i) {
		dev_info = &rx_adapter->eth_devices[i];

		if (rx_adapter->use_queue_event_buf && dev_info->rx_queue) {

			for (j = 0; j < dev_info->dev->data->nb_rx_queues;
			     j++) {
				queue_info = &dev_info->rx_queue[j];
				if (!queue_info->queue_enabled)
					continue;
				q_stats = queue_info->stats;

				stats->rx_packets += q_stats->rx_packets;
				stats->rx_poll_count += q_stats->rx_poll_count;
				stats->rx_enq_count += q_stats->rx_enq_count;
				stats->rx_enq_retry += q_stats->rx_enq_retry;
				stats->rx_dropped += q_stats->rx_dropped;
				stats->rx_enq_block_cycles +=
						q_stats->rx_enq_block_cycles;
			}
		}

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

	buf = &rx_adapter->event_enqueue_buffer;
	stats->rx_packets += dev_stats_sum.rx_packets;
	stats->rx_enq_count += dev_stats_sum.rx_enq_count;
	stats->rx_event_buf_count = buf->count;
	stats->rx_event_buf_size = buf->events_size;

	return 0;
}

int
rte_event_eth_rx_adapter_queue_stats_get(uint8_t id,
		uint16_t eth_dev_id,
		uint16_t rx_queue_id,
		struct rte_event_eth_rx_adapter_queue_stats *stats)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_device_info *dev_info;
	struct eth_rx_queue_info *queue_info;
	struct eth_event_enqueue_buffer *event_buf;
	struct rte_event_eth_rx_adapter_stats *q_stats;
	struct rte_eventdev *dev;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);

	if (rx_adapter == NULL || stats == NULL)
		return -EINVAL;

	if (!rx_adapter->use_queue_event_buf)
		return -EINVAL;

	if (rx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %" PRIu16, rx_queue_id);
		return -EINVAL;
	}

	dev_info = &rx_adapter->eth_devices[eth_dev_id];
	if (dev_info->rx_queue == NULL ||
	    !dev_info->rx_queue[rx_queue_id].queue_enabled) {
		RTE_EDEV_LOG_ERR("Rx queue %u not added", rx_queue_id);
		return -EINVAL;
	}

	if (dev_info->internal_event_port == 0) {
		queue_info = &dev_info->rx_queue[rx_queue_id];
		event_buf = queue_info->event_buf;
		q_stats = queue_info->stats;

		stats->rx_event_buf_count = event_buf->count;
		stats->rx_event_buf_size = event_buf->events_size;
		stats->rx_packets = q_stats->rx_packets;
		stats->rx_poll_count = q_stats->rx_poll_count;
		stats->rx_dropped = q_stats->rx_dropped;
	}

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	if (dev->dev_ops->eth_rx_adapter_queue_stats_get != NULL) {
		return (*dev->dev_ops->eth_rx_adapter_queue_stats_get)(dev,
						&rte_eth_devices[eth_dev_id],
						rx_queue_id, stats);
	}

	return 0;
}

int
rte_event_eth_rx_adapter_stats_reset(uint8_t id)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct rte_eventdev *dev;
	struct eth_device_info *dev_info;
	struct eth_rx_queue_info *queue_info;
	uint32_t i, j;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];

	RTE_ETH_FOREACH_DEV(i) {
		dev_info = &rx_adapter->eth_devices[i];

		if (rx_adapter->use_queue_event_buf  && dev_info->rx_queue) {

			for (j = 0; j < dev_info->dev->data->nb_rx_queues;
						j++) {
				queue_info = &dev_info->rx_queue[j];
				if (!queue_info->queue_enabled)
					continue;
				rxa_queue_stats_reset(queue_info);
			}
		}

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
rte_event_eth_rx_adapter_queue_stats_reset(uint8_t id,
		uint16_t eth_dev_id,
		uint16_t rx_queue_id)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_device_info *dev_info;
	struct eth_rx_queue_info *queue_info;
	struct rte_eventdev *dev;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	if (!rx_adapter->use_queue_event_buf)
		return -EINVAL;

	if (rx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %" PRIu16, rx_queue_id);
		return -EINVAL;
	}

	dev_info = &rx_adapter->eth_devices[eth_dev_id];

	if (dev_info->rx_queue == NULL ||
	    !dev_info->rx_queue[rx_queue_id].queue_enabled) {
		RTE_EDEV_LOG_ERR("Rx queue %u not added", rx_queue_id);
		return -EINVAL;
	}

	if (dev_info->internal_event_port == 0) {
		queue_info = &dev_info->rx_queue[rx_queue_id];
		rxa_queue_stats_reset(queue_info);
	}

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	if (dev->dev_ops->eth_rx_adapter_queue_stats_reset != NULL) {
		return (*dev->dev_ops->eth_rx_adapter_queue_stats_reset)(dev,
						&rte_eth_devices[eth_dev_id],
						rx_queue_id);
	}

	return 0;
}

int
rte_event_eth_rx_adapter_service_id_get(uint8_t id, uint32_t *service_id)
{
	struct event_eth_rx_adapter *rx_adapter;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter == NULL || service_id == NULL)
		return -EINVAL;

	if (rx_adapter->service_inited)
		*service_id = rx_adapter->service_id;

	return rx_adapter->service_inited ? 0 : -ESRCH;
}

int
rte_event_eth_rx_adapter_event_port_get(uint8_t id, uint8_t *event_port_id)
{
	struct event_eth_rx_adapter *rx_adapter;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter == NULL || event_port_id == NULL)
		return -EINVAL;

	if (rx_adapter->service_inited)
		*event_port_id = rx_adapter->event_port_id;

	return rx_adapter->service_inited ? 0 : -ESRCH;
}

int
rte_event_eth_rx_adapter_cb_register(uint8_t id,
					uint16_t eth_dev_id,
					rte_event_eth_rx_adapter_cb_fn cb_fn,
					void *cb_arg)
{
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_device_info *dev_info;
	uint32_t cap;
	int ret;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	dev_info = &rx_adapter->eth_devices[eth_dev_id];
	if (dev_info->rx_queue == NULL)
		return -EINVAL;

	ret = rte_event_eth_rx_adapter_caps_get(rx_adapter->eventdev_id,
						eth_dev_id,
						&cap);
	if (ret) {
		RTE_EDEV_LOG_ERR("Failed to get adapter caps edev %" PRIu8
			"eth port %" PRIu16, id, eth_dev_id);
		return ret;
	}

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT) {
		RTE_EDEV_LOG_ERR("Rx callback not supported for eth port %"
				PRIu16, eth_dev_id);
		return -EINVAL;
	}

	rte_spinlock_lock(&rx_adapter->rx_lock);
	dev_info->cb_fn = cb_fn;
	dev_info->cb_arg = cb_arg;
	rte_spinlock_unlock(&rx_adapter->rx_lock);

	return 0;
}

int
rte_event_eth_rx_adapter_queue_conf_get(uint8_t id,
			uint16_t eth_dev_id,
			uint16_t rx_queue_id,
			struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
#define TICK2NSEC(_ticks, _freq) (((_ticks) * (1E9)) / (_freq))
	struct rte_eventdev *dev;
	struct event_eth_rx_adapter *rx_adapter;
	struct eth_device_info *dev_info;
	struct eth_rx_queue_info *queue_info;
	int ret;

	if (rxa_memzone_lookup())
		return -ENOMEM;

	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(id, -EINVAL);
	RTE_ETH_VALID_PORTID_OR_ERR_RET(eth_dev_id, -EINVAL);

	if (rx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %u", rx_queue_id);
		return -EINVAL;
	}

	if (queue_conf == NULL) {
		RTE_EDEV_LOG_ERR("Rx queue conf struct cannot be NULL");
		return -EINVAL;
	}

	rx_adapter = rxa_id_to_adapter(id);
	if (rx_adapter == NULL)
		return -EINVAL;

	dev_info = &rx_adapter->eth_devices[eth_dev_id];
	if (dev_info->rx_queue == NULL ||
	    !dev_info->rx_queue[rx_queue_id].queue_enabled) {
		RTE_EDEV_LOG_ERR("Rx queue %u not added", rx_queue_id);
		return -EINVAL;
	}

	queue_info = &dev_info->rx_queue[rx_queue_id];

	memset(queue_conf, 0, sizeof(*queue_conf));
	queue_conf->rx_queue_flags = 0;
	if (queue_info->flow_id_mask != 0)
		queue_conf->rx_queue_flags |=
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	queue_conf->servicing_weight = queue_info->wt;

	queue_conf->ev.event = queue_info->event;

	queue_conf->vector_sz = queue_info->vector_data.max_vector_count;
	queue_conf->vector_mp = queue_info->vector_data.vector_pool;
	/* need to be converted from ticks to ns */
	queue_conf->vector_timeout_ns = TICK2NSEC(
		queue_info->vector_data.vector_timeout_ticks, rte_get_timer_hz());

	if (queue_info->event_buf != NULL)
		queue_conf->event_buf_size = queue_info->event_buf->events_size;
	else
		queue_conf->event_buf_size = 0;

	dev = &rte_eventdevs[rx_adapter->eventdev_id];
	if (dev->dev_ops->eth_rx_adapter_queue_conf_get != NULL) {
		ret = (*dev->dev_ops->eth_rx_adapter_queue_conf_get)(dev,
						&rte_eth_devices[eth_dev_id],
						rx_queue_id,
						queue_conf);
		return ret;
	}

	return 0;
}

#define RXA_ADD_DICT(stats, s) rte_tel_data_add_dict_u64(d, #s, stats.s)

static int
handle_rxa_stats(const char *cmd __rte_unused,
		 const char *params,
		 struct rte_tel_data *d)
{
	uint8_t rx_adapter_id;
	struct rte_event_eth_rx_adapter_stats rx_adptr_stats;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get Rx adapter ID from parameter string */
	rx_adapter_id = atoi(params);
	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(rx_adapter_id, -EINVAL);

	/* Get Rx adapter stats */
	if (rte_event_eth_rx_adapter_stats_get(rx_adapter_id,
					       &rx_adptr_stats)) {
		RTE_EDEV_LOG_ERR("Failed to get Rx adapter stats\n");
		return -1;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_u64(d, "rx_adapter_id", rx_adapter_id);
	RXA_ADD_DICT(rx_adptr_stats, rx_packets);
	RXA_ADD_DICT(rx_adptr_stats, rx_poll_count);
	RXA_ADD_DICT(rx_adptr_stats, rx_dropped);
	RXA_ADD_DICT(rx_adptr_stats, rx_enq_retry);
	RXA_ADD_DICT(rx_adptr_stats, rx_event_buf_count);
	RXA_ADD_DICT(rx_adptr_stats, rx_event_buf_size);
	RXA_ADD_DICT(rx_adptr_stats, rx_enq_count);
	RXA_ADD_DICT(rx_adptr_stats, rx_enq_start_ts);
	RXA_ADD_DICT(rx_adptr_stats, rx_enq_block_cycles);
	RXA_ADD_DICT(rx_adptr_stats, rx_enq_end_ts);
	RXA_ADD_DICT(rx_adptr_stats, rx_intr_packets);

	return 0;
}

static int
handle_rxa_stats_reset(const char *cmd __rte_unused,
		       const char *params,
		       struct rte_tel_data *d __rte_unused)
{
	uint8_t rx_adapter_id;

	if (params == NULL || strlen(params) == 0 || ~isdigit(*params))
		return -1;

	/* Get Rx adapter ID from parameter string */
	rx_adapter_id = atoi(params);
	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_ERR_RET(rx_adapter_id, -EINVAL);

	/* Reset Rx adapter stats */
	if (rte_event_eth_rx_adapter_stats_reset(rx_adapter_id)) {
		RTE_EDEV_LOG_ERR("Failed to reset Rx adapter stats\n");
		return -1;
	}

	return 0;
}

static int
handle_rxa_get_queue_conf(const char *cmd __rte_unused,
			  const char *params,
			  struct rte_tel_data *d)
{
	uint8_t rx_adapter_id;
	uint16_t rx_queue_id;
	int eth_dev_id, ret = -1;
	char *token, *l_params;
	struct rte_event_eth_rx_adapter_queue_conf queue_conf;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get Rx adapter ID from parameter string */
	l_params = strdup(params);
	if (l_params == NULL)
		return -ENOMEM;
	token = strtok(l_params, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);
	rx_adapter_id = strtoul(token, NULL, 10);
	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_GOTO_ERR_RET(rx_adapter_id, -EINVAL);

	token = strtok(NULL, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);

	/* Get device ID from parameter string */
	eth_dev_id = strtoul(token, NULL, 10);
	RTE_ETH_VALID_PORTID_OR_GOTO_ERR_RET(eth_dev_id, -EINVAL);

	token = strtok(NULL, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);

	/* Get Rx queue ID from parameter string */
	rx_queue_id = strtoul(token, NULL, 10);
	if (rx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %u", rx_queue_id);
		ret = -EINVAL;
		goto error;
	}

	token = strtok(NULL, "\0");
	if (token != NULL)
		RTE_EDEV_LOG_ERR("Extra parameters passed to eventdev"
				 " telemetry command, ignoring");
	/* Parsing parameter finished */
	free(l_params);

	if (rte_event_eth_rx_adapter_queue_conf_get(rx_adapter_id, eth_dev_id,
						    rx_queue_id, &queue_conf)) {
		RTE_EDEV_LOG_ERR("Failed to get Rx adapter queue config");
		return -1;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_u64(d, "rx_adapter_id", rx_adapter_id);
	rte_tel_data_add_dict_u64(d, "eth_dev_id", eth_dev_id);
	rte_tel_data_add_dict_u64(d, "rx_queue_id", rx_queue_id);
	RXA_ADD_DICT(queue_conf, rx_queue_flags);
	RXA_ADD_DICT(queue_conf, servicing_weight);
	RXA_ADD_DICT(queue_conf.ev, queue_id);
	RXA_ADD_DICT(queue_conf.ev, sched_type);
	RXA_ADD_DICT(queue_conf.ev, priority);
	RXA_ADD_DICT(queue_conf.ev, flow_id);

	return 0;

error:
	free(l_params);
	return ret;
}

static int
handle_rxa_get_queue_stats(const char *cmd __rte_unused,
			   const char *params,
			   struct rte_tel_data *d)
{
	uint8_t rx_adapter_id;
	uint16_t rx_queue_id;
	int eth_dev_id, ret = -1;
	char *token, *l_params;
	struct rte_event_eth_rx_adapter_queue_stats q_stats;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get Rx adapter ID from parameter string */
	l_params = strdup(params);
	if (l_params == NULL)
		return -ENOMEM;
	token = strtok(l_params, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);
	rx_adapter_id = strtoul(token, NULL, 10);
	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_GOTO_ERR_RET(rx_adapter_id, -EINVAL);

	token = strtok(NULL, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);

	/* Get device ID from parameter string */
	eth_dev_id = strtoul(token, NULL, 10);
	RTE_ETH_VALID_PORTID_OR_GOTO_ERR_RET(eth_dev_id, -EINVAL);

	token = strtok(NULL, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);

	/* Get Rx queue ID from parameter string */
	rx_queue_id = strtoul(token, NULL, 10);
	if (rx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %u", rx_queue_id);
		ret = -EINVAL;
		goto error;
	}

	token = strtok(NULL, "\0");
	if (token != NULL)
		RTE_EDEV_LOG_ERR("Extra parameters passed to eventdev"
				 " telemetry command, ignoring");
	/* Parsing parameter finished */
	free(l_params);

	if (rte_event_eth_rx_adapter_queue_stats_get(rx_adapter_id, eth_dev_id,
						    rx_queue_id, &q_stats)) {
		RTE_EDEV_LOG_ERR("Failed to get Rx adapter queue stats");
		return -1;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_u64(d, "rx_adapter_id", rx_adapter_id);
	rte_tel_data_add_dict_u64(d, "eth_dev_id", eth_dev_id);
	rte_tel_data_add_dict_u64(d, "rx_queue_id", rx_queue_id);
	RXA_ADD_DICT(q_stats, rx_event_buf_count);
	RXA_ADD_DICT(q_stats, rx_event_buf_size);
	RXA_ADD_DICT(q_stats, rx_poll_count);
	RXA_ADD_DICT(q_stats, rx_packets);
	RXA_ADD_DICT(q_stats, rx_dropped);

	return 0;

error:
	free(l_params);
	return ret;
}

static int
handle_rxa_queue_stats_reset(const char *cmd __rte_unused,
			     const char *params,
			     struct rte_tel_data *d __rte_unused)
{
	uint8_t rx_adapter_id;
	uint16_t rx_queue_id;
	int eth_dev_id, ret = -1;
	char *token, *l_params;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	/* Get Rx adapter ID from parameter string */
	l_params = strdup(params);
	if (l_params == NULL)
		return -ENOMEM;
	token = strtok(l_params, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);
	rx_adapter_id = strtoul(token, NULL, 10);
	RTE_EVENT_ETH_RX_ADAPTER_ID_VALID_OR_GOTO_ERR_RET(rx_adapter_id, -EINVAL);

	token = strtok(NULL, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);

	/* Get device ID from parameter string */
	eth_dev_id = strtoul(token, NULL, 10);
	RTE_ETH_VALID_PORTID_OR_GOTO_ERR_RET(eth_dev_id, -EINVAL);

	token = strtok(NULL, ",");
	RTE_EVENT_ETH_RX_ADAPTER_TOKEN_VALID_OR_GOTO_ERR_RET(token, -1);

	/* Get Rx queue ID from parameter string */
	rx_queue_id = strtoul(token, NULL, 10);
	if (rx_queue_id >= rte_eth_devices[eth_dev_id].data->nb_rx_queues) {
		RTE_EDEV_LOG_ERR("Invalid rx queue_id %u", rx_queue_id);
		ret = -EINVAL;
		goto error;
	}

	token = strtok(NULL, "\0");
	if (token != NULL)
		RTE_EDEV_LOG_ERR("Extra parameters passed to eventdev"
				 " telemetry command, ignoring");
	/* Parsing parameter finished */
	free(l_params);

	if (rte_event_eth_rx_adapter_queue_stats_reset(rx_adapter_id,
						       eth_dev_id,
						       rx_queue_id)) {
		RTE_EDEV_LOG_ERR("Failed to reset Rx adapter queue stats");
		return -1;
	}

	return 0;

error:
	free(l_params);
	return ret;
}

RTE_INIT(rxa_init_telemetry)
{
	rte_telemetry_register_cmd("/eventdev/rxa_stats",
		handle_rxa_stats,
		"Returns Rx adapter stats. Parameter: rxa_id");

	rte_telemetry_register_cmd("/eventdev/rxa_stats_reset",
		handle_rxa_stats_reset,
		"Reset Rx adapter stats. Parameter: rxa_id");

	rte_telemetry_register_cmd("/eventdev/rxa_queue_conf",
		handle_rxa_get_queue_conf,
		"Returns Rx queue config. Parameter: rxa_id, dev_id, queue_id");

	rte_telemetry_register_cmd("/eventdev/rxa_queue_stats",
		handle_rxa_get_queue_stats,
		"Returns Rx queue stats. Parameter: rxa_id, dev_id, queue_id");

	rte_telemetry_register_cmd("/eventdev/rxa_queue_stats_reset",
		handle_rxa_queue_stats_reset,
		"Reset Rx queue stats. Parameter: rxa_id, dev_id, queue_id");
}
