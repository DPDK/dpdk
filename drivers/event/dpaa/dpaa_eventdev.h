/*   SPDX-License-Identifier:        BSD-3-Clause
 *   Copyright 2017 NXP
 */

#ifndef __DPAA_EVENTDEV_H__
#define __DPAA_EVENTDEV_H__

#include <eventdev_pmd.h>
#include <eventdev_pmd_vdev.h>
#include <rte_atomic.h>
#include <rte_per_lcore.h>

#define EVENTDEV_NAME_DPAA_PMD		event_dpaa1

#define DPAA_EVENT_MAX_PORTS			4
#define DPAA_EVENT_MAX_QUEUES			8
#define DPAA_EVENT_MIN_DEQUEUE_TIMEOUT	1
#define DPAA_EVENT_MAX_DEQUEUE_TIMEOUT	(UINT32_MAX - 1)
#define DPAA_EVENT_MAX_QUEUE_FLOWS		2048
#define DPAA_EVENT_MAX_QUEUE_PRIORITY_LEVELS	8
#define DPAA_EVENT_MAX_EVENT_PRIORITY_LEVELS	0
#define DPAA_EVENT_MAX_EVENT_PORT		RTE_MIN(RTE_MAX_LCORE, INT8_MAX)
#define DPAA_EVENT_MAX_PORT_DEQUEUE_DEPTH	8
#define DPAA_EVENT_PORT_DEQUEUE_TIMEOUT_NS	100000UL
#define DPAA_EVENT_PORT_DEQUEUE_TIMEOUT_INVALID	((uint64_t)-1)
#define DPAA_EVENT_MAX_PORT_ENQUEUE_DEPTH	1
#define DPAA_EVENT_MAX_NUM_EVENTS		(INT32_MAX - 1)

#define DPAA_EVENT_DEV_CAP			\
do {						\
	RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |	\
	RTE_EVENT_DEV_CAP_BURST_MODE;		\
} while (0)

#define DPAA_EVENT_QUEUE_ATOMIC_FLOWS		2048
#define DPAA_EVENT_QUEUE_ORDER_SEQUENCES	2048

#define RTE_EVENT_ETH_RX_ADAPTER_DPAA_CAP \
		(RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT | \
		RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ | \
		RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID)

#define RTE_EVENT_CRYPTO_ADAPTER_DPAA_CAP \
		(RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW | \
		RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND | \
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA)

struct dpaa_eventq {
	/* Channel Id */
	uint16_t ch_id;
	/* Configuration provided by the user */
	uint32_t event_queue_cfg;
	uint32_t event_queue_id;
	/* Event port */
	void *event_port;
};

struct dpaa_port {
	struct dpaa_eventq evq_info[DPAA_EVENT_MAX_QUEUES];
	uint8_t num_linked_evq;
	uint8_t is_port_linked;
	uint64_t timeout_us;
};

struct dpaa_eventdev {
	struct dpaa_eventq evq_info[DPAA_EVENT_MAX_QUEUES];
	struct dpaa_port ports[DPAA_EVENT_MAX_PORTS];
	uint32_t dequeue_timeout_ns;
	uint32_t nb_events_limit;
	uint8_t max_event_queues;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint8_t intr_mode;
	uint32_t nb_event_queue_flows;
	uint32_t nb_event_port_dequeue_depth;
	uint32_t nb_event_port_enqueue_depth;
	uint32_t event_dev_cfg;
};

#define DPAA_EVENTDEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_eventdev, "%s(): " fmt "\n", \
		__func__, ##args)

#define EVENTDEV_INIT_FUNC_TRACE() DPAA_EVENTDEV_LOG(DEBUG, " >>")

#define DPAA_EVENTDEV_DEBUG(fmt, args...) \
	DPAA_EVENTDEV_LOG(DEBUG, fmt, ## args)
#define DPAA_EVENTDEV_ERR(fmt, args...) \
	DPAA_EVENTDEV_LOG(ERR, fmt, ## args)
#define DPAA_EVENTDEV_INFO(fmt, args...) \
	DPAA_EVENTDEV_LOG(INFO, fmt, ## args)
#define DPAA_EVENTDEV_WARN(fmt, args...) \
	DPAA_EVENTDEV_LOG(WARNING, fmt, ## args)

#endif /* __DPAA_EVENTDEV_H__ */
