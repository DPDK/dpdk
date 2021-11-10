/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_bus_vdev.h>

#include "skeleton_eventdev.h"

#define EVENTDEV_NAME_SKELETON_PMD event_skeleton
/**< Skeleton event device PMD name */

static uint16_t
skeleton_eventdev_enqueue(void *port, const struct rte_event *ev)
{
	struct skeleton_port *sp = port;

	RTE_SET_USED(sp);
	RTE_SET_USED(ev);
	RTE_SET_USED(port);

	return 0;
}

static uint16_t
skeleton_eventdev_enqueue_burst(void *port, const struct rte_event ev[],
			uint16_t nb_events)
{
	struct skeleton_port *sp = port;

	RTE_SET_USED(sp);
	RTE_SET_USED(ev);
	RTE_SET_USED(port);
	RTE_SET_USED(nb_events);

	return 0;
}

static uint16_t
skeleton_eventdev_dequeue(void *port, struct rte_event *ev,
				uint64_t timeout_ticks)
{
	struct skeleton_port *sp = port;

	RTE_SET_USED(sp);
	RTE_SET_USED(ev);
	RTE_SET_USED(timeout_ticks);

	return 0;
}

static uint16_t
skeleton_eventdev_dequeue_burst(void *port, struct rte_event ev[],
		uint16_t nb_events, uint64_t timeout_ticks)
{
	struct skeleton_port *sp = port;

	RTE_SET_USED(sp);
	RTE_SET_USED(ev);
	RTE_SET_USED(nb_events);
	RTE_SET_USED(timeout_ticks);

	return 0;
}

static void
skeleton_eventdev_info_get(struct rte_eventdev *dev,
		struct rte_event_dev_info *dev_info)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);

	dev_info->min_dequeue_timeout_ns = 1;
	dev_info->max_dequeue_timeout_ns = 10000;
	dev_info->dequeue_timeout_ns = 25;
	dev_info->max_event_queues = 64;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 8;
	dev_info->max_event_ports = 32;
	dev_info->max_event_port_dequeue_depth = 16;
	dev_info->max_event_port_enqueue_depth = 16;
	dev_info->max_num_events = (1ULL << 20);
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_BURST_MODE |
					RTE_EVENT_DEV_CAP_EVENT_QOS |
					RTE_EVENT_DEV_CAP_CARRY_FLOW_ID |
					RTE_EVENT_DEV_CAP_MAINTENANCE_FREE;
}

static int
skeleton_eventdev_configure(const struct rte_eventdev *dev)
{
	struct rte_eventdev_data *data = dev->data;
	struct rte_event_dev_config *conf = &data->dev_conf;
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(conf);
	RTE_SET_USED(skel);

	PMD_DRV_LOG(DEBUG, "Configured eventdev devid=%d", dev->data->dev_id);
	return 0;
}

static int
skeleton_eventdev_start(struct rte_eventdev *dev)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);

	return 0;
}

static void
skeleton_eventdev_stop(struct rte_eventdev *dev)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
}

static int
skeleton_eventdev_close(struct rte_eventdev *dev)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);

	return 0;
}

static void
skeleton_eventdev_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
				 struct rte_event_queue_conf *queue_conf)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
	RTE_SET_USED(queue_id);

	queue_conf->nb_atomic_flows = (1ULL << 20);
	queue_conf->nb_atomic_order_sequences = (1ULL << 20);
	queue_conf->event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

static void
skeleton_eventdev_queue_release(struct rte_eventdev *dev, uint8_t queue_id)
{
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
}

static int
skeleton_eventdev_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
			      const struct rte_event_queue_conf *queue_conf)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
	RTE_SET_USED(queue_conf);
	RTE_SET_USED(queue_id);

	return 0;
}

static void
skeleton_eventdev_port_def_conf(struct rte_eventdev *dev, uint8_t port_id,
				 struct rte_event_port_conf *port_conf)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
	RTE_SET_USED(port_id);

	port_conf->new_event_threshold = 32 * 1024;
	port_conf->dequeue_depth = 16;
	port_conf->enqueue_depth = 16;
	port_conf->event_port_cfg = 0;
}

static void
skeleton_eventdev_port_release(void *port)
{
	struct skeleton_port *sp = port;
	PMD_DRV_FUNC_TRACE();

	rte_free(sp);
}

static int
skeleton_eventdev_port_setup(struct rte_eventdev *dev, uint8_t port_id,
				const struct rte_event_port_conf *port_conf)
{
	struct skeleton_port *sp;
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
	RTE_SET_USED(port_conf);

	/* Free memory prior to re-allocation if needed */
	if (dev->data->ports[port_id] != NULL) {
		PMD_DRV_LOG(DEBUG, "Freeing memory prior to re-allocation %d",
				port_id);
		skeleton_eventdev_port_release(dev->data->ports[port_id]);
		dev->data->ports[port_id] = NULL;
	}

	/* Allocate event port memory */
	sp = rte_zmalloc_socket("eventdev port",
			sizeof(struct skeleton_port), RTE_CACHE_LINE_SIZE,
			dev->data->socket_id);
	if (sp == NULL) {
		PMD_DRV_ERR("Failed to allocate sp port_id=%d", port_id);
		return -ENOMEM;
	}

	sp->port_id = port_id;

	PMD_DRV_LOG(DEBUG, "[%d] sp=%p", port_id, sp);

	dev->data->ports[port_id] = sp;
	return 0;
}

static int
skeleton_eventdev_port_link(struct rte_eventdev *dev, void *port,
			const uint8_t queues[], const uint8_t priorities[],
			uint16_t nb_links)
{
	struct skeleton_port *sp = port;
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(sp);
	RTE_SET_USED(queues);
	RTE_SET_USED(priorities);

	/* Linked all the queues */
	return (int)nb_links;
}

static int
skeleton_eventdev_port_unlink(struct rte_eventdev *dev, void *port,
				 uint8_t queues[], uint16_t nb_unlinks)
{
	struct skeleton_port *sp = port;
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(sp);
	RTE_SET_USED(queues);

	/* Unlinked all the queues */
	return (int)nb_unlinks;

}

static int
skeleton_eventdev_timeout_ticks(struct rte_eventdev *dev, uint64_t ns,
				 uint64_t *timeout_ticks)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);
	uint32_t scale = 1;

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
	*timeout_ticks = ns * scale;

	return 0;
}

static void
skeleton_eventdev_dump(struct rte_eventdev *dev, FILE *f)
{
	struct skeleton_eventdev *skel = skeleton_pmd_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(skel);
	RTE_SET_USED(f);
}


/* Initialize and register event driver with DPDK Application */
static struct eventdev_ops skeleton_eventdev_ops = {
	.dev_infos_get    = skeleton_eventdev_info_get,
	.dev_configure    = skeleton_eventdev_configure,
	.dev_start        = skeleton_eventdev_start,
	.dev_stop         = skeleton_eventdev_stop,
	.dev_close        = skeleton_eventdev_close,
	.queue_def_conf   = skeleton_eventdev_queue_def_conf,
	.queue_setup      = skeleton_eventdev_queue_setup,
	.queue_release    = skeleton_eventdev_queue_release,
	.port_def_conf    = skeleton_eventdev_port_def_conf,
	.port_setup       = skeleton_eventdev_port_setup,
	.port_release     = skeleton_eventdev_port_release,
	.port_link        = skeleton_eventdev_port_link,
	.port_unlink      = skeleton_eventdev_port_unlink,
	.timeout_ticks    = skeleton_eventdev_timeout_ticks,
	.dump             = skeleton_eventdev_dump
};

static int
skeleton_eventdev_init(struct rte_eventdev *eventdev)
{
	struct rte_pci_device *pci_dev;
	struct skeleton_eventdev *skel = skeleton_pmd_priv(eventdev);
	int ret = 0;

	PMD_DRV_FUNC_TRACE();

	eventdev->dev_ops       = &skeleton_eventdev_ops;
	eventdev->enqueue       = skeleton_eventdev_enqueue;
	eventdev->enqueue_burst = skeleton_eventdev_enqueue_burst;
	eventdev->dequeue       = skeleton_eventdev_dequeue;
	eventdev->dequeue_burst = skeleton_eventdev_dequeue_burst;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = RTE_DEV_TO_PCI(eventdev->dev);

	skel->reg_base = (uintptr_t)pci_dev->mem_resource[0].addr;
	if (!skel->reg_base) {
		PMD_DRV_ERR("Failed to map BAR0");
		ret = -ENODEV;
		goto fail;
	}

	skel->device_id = pci_dev->id.device_id;
	skel->vendor_id = pci_dev->id.vendor_id;
	skel->subsystem_device_id = pci_dev->id.subsystem_device_id;
	skel->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	PMD_DRV_LOG(DEBUG, "pci device (%x:%x) %u:%u:%u:%u",
			pci_dev->id.vendor_id, pci_dev->id.device_id,
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);

	PMD_DRV_LOG(INFO, "dev_id=%d socket_id=%d (%x:%x)",
		eventdev->data->dev_id, eventdev->data->socket_id,
		skel->vendor_id, skel->device_id);

fail:
	return ret;
}

/* PCI based event device */

#define EVENTDEV_SKEL_VENDOR_ID         0x177d
#define EVENTDEV_SKEL_PRODUCT_ID        0x0001

static const struct rte_pci_id pci_id_skeleton_map[] = {
	{
		RTE_PCI_DEVICE(EVENTDEV_SKEL_VENDOR_ID,
			       EVENTDEV_SKEL_PRODUCT_ID)
	},
	{
		.vendor_id = 0,
	},
};

static int
event_skeleton_pci_probe(struct rte_pci_driver *pci_drv,
			 struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
		sizeof(struct skeleton_eventdev), skeleton_eventdev_init);
}

static int
event_skeleton_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, NULL);
}

static struct rte_pci_driver pci_eventdev_skeleton_pmd = {
	.id_table = pci_id_skeleton_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = event_skeleton_pci_probe,
	.remove = event_skeleton_pci_remove,
};

RTE_PMD_REGISTER_PCI(event_skeleton_pci, pci_eventdev_skeleton_pmd);
RTE_PMD_REGISTER_PCI_TABLE(event_skeleton_pci, pci_id_skeleton_map);

/* VDEV based event device */

static int
skeleton_eventdev_create(const char *name, int socket_id)
{
	struct rte_eventdev *eventdev;

	eventdev = rte_event_pmd_vdev_init(name,
			sizeof(struct skeleton_eventdev), socket_id);
	if (eventdev == NULL) {
		PMD_DRV_ERR("Failed to create eventdev vdev %s", name);
		goto fail;
	}

	eventdev->dev_ops       = &skeleton_eventdev_ops;
	eventdev->enqueue       = skeleton_eventdev_enqueue;
	eventdev->enqueue_burst = skeleton_eventdev_enqueue_burst;
	eventdev->dequeue       = skeleton_eventdev_dequeue;
	eventdev->dequeue_burst = skeleton_eventdev_dequeue_burst;

	event_dev_probing_finish(eventdev);
	return 0;
fail:
	return -EFAULT;
}

static int
skeleton_eventdev_probe(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	RTE_LOG(INFO, PMD, "Initializing %s on NUMA node %d\n", name,
			rte_socket_id());
	return skeleton_eventdev_create(name, rte_socket_id());
}

static int
skeleton_eventdev_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Closing %s on NUMA node %d", name, rte_socket_id());

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_eventdev_skeleton_pmd = {
	.probe = skeleton_eventdev_probe,
	.remove = skeleton_eventdev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_SKELETON_PMD, vdev_eventdev_skeleton_pmd);
