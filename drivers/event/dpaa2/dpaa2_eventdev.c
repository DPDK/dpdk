/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of NXP nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>
#include <sys/epoll.h>

#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_fslmc.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_vdev.h>

#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_hw_dpio.h>
#include "dpaa2_eventdev.h"
#include <portal/dpaa2_hw_pvt.h>
#include <mc/fsl_dpci.h>

/* Clarifications
 * Evendev = SoC Instance
 * Eventport = DPIO Instance
 * Eventqueue = DPCON Instance
 * 1 Eventdev can have N Eventqueue
 * Soft Event Flow is DPCI Instance
 */

static uint16_t
dpaa2_eventdev_enqueue_burst(void *port, const struct rte_event ev[],
			     uint16_t nb_events)
{
	RTE_SET_USED(port);
	RTE_SET_USED(ev);
	RTE_SET_USED(nb_events);

	return 0;
}

static uint16_t
dpaa2_eventdev_enqueue(void *port, const struct rte_event *ev)
{
	return dpaa2_eventdev_enqueue_burst(port, ev, 1);
}

static uint16_t
dpaa2_eventdev_dequeue_burst(void *port, struct rte_event ev[],
			     uint16_t nb_events, uint64_t timeout_ticks)
{
	RTE_SET_USED(port);
	RTE_SET_USED(ev);
	RTE_SET_USED(nb_events);
	RTE_SET_USED(timeout_ticks);

	return 0;
}

static uint16_t
dpaa2_eventdev_dequeue(void *port, struct rte_event *ev,
		       uint64_t timeout_ticks)
{
	return dpaa2_eventdev_dequeue_burst(port, ev, 1, timeout_ticks);
}

static const struct rte_eventdev_ops dpaa2_eventdev_ops;

static int
dpaa2_eventdev_setup_dpci(struct dpaa2_dpci_dev *dpci_dev,
			  struct dpaa2_dpcon_dev *dpcon_dev)
{
	struct dpci_rx_queue_cfg rx_queue_cfg;
	int ret, i;

	/*Do settings to get the frame on a DPCON object*/
	rx_queue_cfg.options = DPCI_QUEUE_OPT_DEST;
	rx_queue_cfg.dest_cfg.dest_type = DPCI_DEST_DPCON;
	rx_queue_cfg.dest_cfg.dest_id = dpcon_dev->dpcon_id;
	rx_queue_cfg.dest_cfg.priority = DPAA2_EVENT_DEFAULT_DPCI_PRIO;

	for (i = 0 ; i < DPAA2_EVENT_DPCI_MAX_QUEUES; i++) {
		rx_queue_cfg.user_ctx = (uint64_t)(&dpci_dev->queue[i]);
		ret = dpci_set_rx_queue(&dpci_dev->dpci,
					CMD_PRI_LOW,
					dpci_dev->token, i,
					&rx_queue_cfg);
		if (ret) {
			PMD_DRV_LOG(ERR, PMD,
				    "set_rx_q failed with err code: %d", ret);
			return ret;
		}
	}
	return 0;
}

static int
dpaa2_eventdev_create(const char *name)
{
	struct rte_eventdev *eventdev;
	struct dpaa2_eventdev *priv;
	struct dpaa2_dpcon_dev *dpcon_dev = NULL;
	struct dpaa2_dpci_dev *dpci_dev = NULL;
	int ret;

	eventdev = rte_event_pmd_vdev_init(name,
					   sizeof(struct dpaa2_eventdev),
					   rte_socket_id());
	if (eventdev == NULL) {
		PMD_DRV_ERR("Failed to create eventdev vdev %s", name);
		goto fail;
	}

	eventdev->dev_ops       = &dpaa2_eventdev_ops;
	eventdev->schedule      = NULL;
	eventdev->enqueue       = dpaa2_eventdev_enqueue;
	eventdev->enqueue_burst = dpaa2_eventdev_enqueue_burst;
	eventdev->dequeue       = dpaa2_eventdev_dequeue;
	eventdev->dequeue_burst = dpaa2_eventdev_dequeue_burst;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	priv = eventdev->data->dev_private;
	priv->max_event_queues = 0;

	do {
		dpcon_dev = rte_dpaa2_alloc_dpcon_dev();
		if (!dpcon_dev)
			break;
		priv->evq_info[priv->max_event_queues].dpcon = dpcon_dev;

		dpci_dev = rte_dpaa2_alloc_dpci_dev();
		if (!dpci_dev) {
			rte_dpaa2_free_dpcon_dev(dpcon_dev);
			break;
		}
		priv->evq_info[priv->max_event_queues].dpci = dpci_dev;

		ret = dpaa2_eventdev_setup_dpci(dpci_dev, dpcon_dev);
		if (ret) {
			PMD_DRV_LOG(ERR, PMD,
				    "dpci setup failed with err code: %d", ret);
			return ret;
		}
		priv->max_event_queues++;
	} while (dpcon_dev && dpci_dev);

	return 0;
fail:
	return -EFAULT;
}

static int
dpaa2_eventdev_probe(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, PMD, "Initializing %s\n", name);
	return dpaa2_eventdev_create(name);
}

static int
dpaa2_eventdev_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Closing %s", name);

	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_eventdev_dpaa2_pmd = {
	.probe = dpaa2_eventdev_probe,
	.remove = dpaa2_eventdev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_DPAA2_PMD, vdev_eventdev_dpaa2_pmd);
