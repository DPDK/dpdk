/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "eventdev_pmd.h"
#include "rte_eventdev.h"

static uint16_t
dummy_event_enqueue(__rte_unused void *port,
		    __rte_unused const struct rte_event *ev)
{
	RTE_EDEV_LOG_ERR(
		"event enqueue requested for unconfigured event device");
	return 0;
}

static uint16_t
dummy_event_enqueue_burst(__rte_unused void *port,
			  __rte_unused const struct rte_event ev[],
			  __rte_unused uint16_t nb_events)
{
	RTE_EDEV_LOG_ERR(
		"event enqueue burst requested for unconfigured event device");
	return 0;
}

static uint16_t
dummy_event_dequeue(__rte_unused void *port, __rte_unused struct rte_event *ev,
		    __rte_unused uint64_t timeout_ticks)
{
	RTE_EDEV_LOG_ERR(
		"event dequeue requested for unconfigured event device");
	return 0;
}

static uint16_t
dummy_event_dequeue_burst(__rte_unused void *port,
			  __rte_unused struct rte_event ev[],
			  __rte_unused uint16_t nb_events,
			  __rte_unused uint64_t timeout_ticks)
{
	RTE_EDEV_LOG_ERR(
		"event dequeue burst requested for unconfigured event device");
	return 0;
}

static void
dummy_event_maintain(__rte_unused void *port, __rte_unused int op)
{
	RTE_EDEV_LOG_ERR(
		"maintenance requested for unconfigured event device");
}

static uint16_t
dummy_event_tx_adapter_enqueue(__rte_unused void *port,
			       __rte_unused struct rte_event ev[],
			       __rte_unused uint16_t nb_events)
{
	RTE_EDEV_LOG_ERR(
		"event Tx adapter enqueue requested for unconfigured event device");
	return 0;
}

static uint16_t
dummy_event_tx_adapter_enqueue_same_dest(__rte_unused void *port,
					 __rte_unused struct rte_event ev[],
					 __rte_unused uint16_t nb_events)
{
	RTE_EDEV_LOG_ERR(
		"event Tx adapter enqueue same destination requested for unconfigured event device");
	return 0;
}

static uint16_t
dummy_event_crypto_adapter_enqueue(__rte_unused void *port,
				   __rte_unused struct rte_event ev[],
				   __rte_unused uint16_t nb_events)
{
	RTE_EDEV_LOG_ERR(
		"event crypto adapter enqueue requested for unconfigured event device");
	return 0;
}

static uint16_t
dummy_event_dma_adapter_enqueue(__rte_unused void *port, __rte_unused struct rte_event ev[],
			       __rte_unused uint16_t nb_events)
{
	RTE_EDEV_LOG_ERR("event DMA adapter enqueue requested for unconfigured event device");
	return 0;
}

static int
dummy_event_port_profile_switch(__rte_unused void *port, __rte_unused uint8_t profile_id)
{
	RTE_EDEV_LOG_ERR("change profile requested for unconfigured event device");
	return -EINVAL;
}

void
event_dev_fp_ops_reset(struct rte_event_fp_ops *fp_op)
{
	static void *dummy_data[RTE_MAX_QUEUES_PER_PORT];
	static const struct rte_event_fp_ops dummy = {
		.enqueue = dummy_event_enqueue,
		.enqueue_burst = dummy_event_enqueue_burst,
		.enqueue_new_burst = dummy_event_enqueue_burst,
		.enqueue_forward_burst = dummy_event_enqueue_burst,
		.dequeue = dummy_event_dequeue,
		.dequeue_burst = dummy_event_dequeue_burst,
		.maintain = dummy_event_maintain,
		.txa_enqueue = dummy_event_tx_adapter_enqueue,
		.txa_enqueue_same_dest =
			dummy_event_tx_adapter_enqueue_same_dest,
		.ca_enqueue = dummy_event_crypto_adapter_enqueue,
		.dma_enqueue = dummy_event_dma_adapter_enqueue,
		.profile_switch = dummy_event_port_profile_switch,
		.data = dummy_data,
	};

	*fp_op = dummy;
}

void
event_dev_fp_ops_set(struct rte_event_fp_ops *fp_op,
		     const struct rte_eventdev *dev)
{
	fp_op->enqueue = dev->enqueue;
	fp_op->enqueue_burst = dev->enqueue_burst;
	fp_op->enqueue_new_burst = dev->enqueue_new_burst;
	fp_op->enqueue_forward_burst = dev->enqueue_forward_burst;
	fp_op->dequeue = dev->dequeue;
	fp_op->dequeue_burst = dev->dequeue_burst;
	fp_op->maintain = dev->maintain;
	fp_op->txa_enqueue = dev->txa_enqueue;
	fp_op->txa_enqueue_same_dest = dev->txa_enqueue_same_dest;
	fp_op->ca_enqueue = dev->ca_enqueue;
	fp_op->dma_enqueue = dev->dma_enqueue;
	fp_op->profile_switch = dev->profile_switch;
	fp_op->data = dev->data->ports;
}
