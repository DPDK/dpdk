/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef __DPAA2_EVENTDEV_H__
#define __DPAA2_EVENTDEV_H__

#include <eventdev_pmd.h>
#include <eventdev_pmd_vdev.h>
#include <rte_atomic.h>
#include <mc/fsl_dpcon.h>
#include <mc/fsl_mc_sys.h>

#define EVENTDEV_NAME_DPAA2_PMD		event_dpaa2

#define DPAA2_EVENT_DEFAULT_DPCI_PRIO 0

#define DPAA2_EVENT_MAX_QUEUES			16
#define DPAA2_EVENT_MIN_DEQUEUE_TIMEOUT		1
#define DPAA2_EVENT_MAX_DEQUEUE_TIMEOUT		(UINT32_MAX - 1)
#define DPAA2_EVENT_PORT_DEQUEUE_TIMEOUT_NS	100UL
#define DPAA2_EVENT_MAX_QUEUE_FLOWS		2048
#define DPAA2_EVENT_MAX_QUEUE_PRIORITY_LEVELS	8
#define DPAA2_EVENT_MAX_EVENT_PRIORITY_LEVELS	0
#define DPAA2_EVENT_MAX_PORT_DEQUEUE_DEPTH	8
#define DPAA2_EVENT_MAX_PORT_ENQUEUE_DEPTH	8
#define DPAA2_EVENT_MAX_NUM_EVENTS		(INT32_MAX - 1)

#define DPAA2_EVENT_QUEUE_ATOMIC_FLOWS		2048
#define DPAA2_EVENT_QUEUE_ORDER_SEQUENCES	2048

enum {
	DPAA2_EVENT_DPCI_PARALLEL_QUEUE,
	DPAA2_EVENT_DPCI_ATOMIC_QUEUE,
	DPAA2_EVENT_DPCI_MAX_QUEUES
};

#define RTE_EVENT_ETH_RX_ADAPTER_DPAA2_CAP \
		(RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT | \
		RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ | \
		RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID | \
		RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT)

/**< Crypto Rx adapter cap to return If the packet transfers from
 * the cryptodev to eventdev with DPAA2 devices.
 */
#define RTE_EVENT_CRYPTO_ADAPTER_DPAA2_CAP \
		(RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW | \
		RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND | \
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA)

/**< Ethernet Rx adapter cap to return If the packet transfers from
 * the ethdev to eventdev with DPAA2 devices.
 */

struct dpaa2_eventq {
	/* DPcon device */
	struct dpaa2_dpcon_dev *dpcon;
	/* Attached DPCI device */
	struct dpaa2_dpci_dev *dpci;
	/* Mapped event port */
	struct dpaa2_io_portal_t *event_port;
	/* Configuration provided by the user */
	uint32_t event_queue_cfg;
	uint32_t event_queue_id;
};

struct dpaa2_port {
	struct dpaa2_eventq evq_info[DPAA2_EVENT_MAX_QUEUES];
	uint8_t num_linked_evq;
	uint8_t is_port_linked;
	uint64_t timeout_us;
};

struct dpaa2_eventdev {
	struct dpaa2_eventq evq_info[DPAA2_EVENT_MAX_QUEUES];
	uint32_t dequeue_timeout_ns;
	uint8_t max_event_queues;
	uint8_t nb_event_queues;
	uint8_t nb_event_ports;
	uint8_t resvd_1;
	uint32_t nb_event_queue_flows;
	uint32_t nb_event_port_dequeue_depth;
	uint32_t nb_event_port_enqueue_depth;
	uint32_t event_dev_cfg;
};

struct dpaa2_dpcon_dev *rte_dpaa2_alloc_dpcon_dev(void);
void rte_dpaa2_free_dpcon_dev(struct dpaa2_dpcon_dev *dpcon);

int test_eventdev_dpaa2(void);

#endif /* __DPAA2_EVENTDEV_H__ */
