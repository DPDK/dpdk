/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#ifndef __DPAA2_EVENTDEV_H__
#define __DPAA2_EVENTDEV_H__

#include <rte_eventdev_pmd.h>
#include <rte_eventdev_pmd_vdev.h>
#include <rte_atomic.h>
#include <mc/fsl_dpcon.h>
#include <mc/fsl_mc_sys.h>

#define EVENTDEV_NAME_DPAA2_PMD		event_dpaa2

#define DPAA2_EVENT_DEFAULT_DPCI_PRIO 0

#define DPAA2_EVENT_MAX_QUEUES			16
#define DPAA2_EVENT_MIN_DEQUEUE_TIMEOUT		1
#define DPAA2_EVENT_MAX_DEQUEUE_TIMEOUT		(UINT32_MAX - 1)
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
		RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID)
/**< Ethernet Rx adapter cap to return If the packet transfers from
 * the ethdev to eventdev with DPAA2 devices.
 */

struct dpaa2_dpcon_dev {
	TAILQ_ENTRY(dpaa2_dpcon_dev) next;
	struct fsl_mc_io dpcon;
	uint16_t token;
	rte_atomic16_t in_use;
	uint32_t dpcon_id;
	uint16_t qbman_ch_id;
	uint8_t num_priorities;
	uint8_t channel_index;
};

struct evq_info_t {
	/* DPcon device */
	struct dpaa2_dpcon_dev *dpcon;
	/* Attached DPCI device */
	struct dpaa2_dpci_dev *dpci;
	/* Configuration provided by the user */
	uint32_t event_queue_cfg;
};

struct dpaa2_eventdev {
	struct evq_info_t evq_info[DPAA2_EVENT_MAX_QUEUES];
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

#endif /* __DPAA2_EVENTDEV_H__ */
