/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Microsoft Corp.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_bus_vmbus.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>

#include "hn_logs.h"
#include "hn_var.h"
#include "hn_nvs.h"

/* Search for VF with matching MAC address, return port id */
static int hn_vf_match(const struct rte_eth_dev *dev)
{
	const struct rte_ether_addr *mac = dev->data->mac_addrs;
	int i;

	RTE_ETH_FOREACH_DEV(i) {
		const struct rte_eth_dev *vf_dev = &rte_eth_devices[i];
		const struct rte_ether_addr *vf_mac = vf_dev->data->mac_addrs;

		if (vf_dev == dev)
			continue;

		if (rte_is_same_ether_addr(mac, vf_mac))
			return i;
	}
	return -ENOENT;
}


/*
 * Attach new PCI VF device and return the port_id
 */
static int hn_vf_attach(struct rte_eth_dev *dev, struct hn_data *hv)
{
	struct rte_eth_dev_owner owner = { .id = RTE_ETH_DEV_NO_OWNER };
	int port, ret;

	if (hv->vf_ctx.vf_attached) {
		PMD_DRV_LOG(ERR, "VF already attached");
		return 0;
	}

	port = hn_vf_match(dev);
	if (port < 0) {
		PMD_DRV_LOG(NOTICE, "Couldn't find port for VF");
		return port;
	}

	PMD_DRV_LOG(NOTICE, "found matching VF port %d", port);
	ret = rte_eth_dev_owner_get(port, &owner);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can not find owner for port %d", port);
		return ret;
	}

	if (owner.id != RTE_ETH_DEV_NO_OWNER) {
		PMD_DRV_LOG(ERR, "Port %u already owned by other device %s",
			    port, owner.name);
		return -EBUSY;
	}

	ret = rte_eth_dev_owner_set(port, &hv->owner);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Can set owner for port %d", port);
		return ret;
	}

	PMD_DRV_LOG(DEBUG, "Attach VF device %u", port);
	hv->vf_ctx.vf_attached = true;
	hv->vf_ctx.vf_port = port;
	return 0;
}

static void hn_vf_remove(struct hn_data *hv);

static void hn_remove_delayed(void *args)
{
	struct hn_data *hv = args;
	uint16_t port_id = hv->vf_ctx.vf_port;
	struct rte_device *dev = rte_eth_devices[port_id].device;
	int ret;

	/* Tell VSP to switch data path to synthetic */
	hn_vf_remove(hv);

	PMD_DRV_LOG(NOTICE, "Start to remove port %d", port_id);
	rte_rwlock_write_lock(&hv->vf_lock);

	/* Give back ownership */
	ret = rte_eth_dev_owner_unset(port_id, hv->owner.id);
	if (ret)
		PMD_DRV_LOG(ERR, "rte_eth_dev_owner_unset failed ret=%d",
			    ret);
	hv->vf_ctx.vf_attached = false;

	ret = rte_eth_dev_callback_unregister(port_id, RTE_ETH_EVENT_INTR_RMV,
					      hn_eth_rmv_event_callback, hv);
	if (ret)
		PMD_DRV_LOG(ERR,
			    "rte_eth_dev_callback_unregister failed ret=%d",
			    ret);

	/* Detach and release port_id from system */
	ret = rte_eth_dev_stop(port_id);
	if (ret)
		PMD_DRV_LOG(ERR, "rte_eth_dev_stop failed port_id=%u ret=%d",
			    port_id, ret);

	ret = rte_eth_dev_close(port_id);
	if (ret)
		PMD_DRV_LOG(ERR, "rte_eth_dev_close failed port_id=%u ret=%d",
			    port_id, ret);

	ret = rte_dev_remove(dev);
	hv->vf_ctx.vf_state = vf_removed;

	rte_rwlock_write_unlock(&hv->vf_lock);
}

int hn_eth_rmv_event_callback(uint16_t port_id,
			      enum rte_eth_event_type event __rte_unused,
			      void *cb_arg, void *out __rte_unused)
{
	struct hn_data *hv = cb_arg;

	PMD_DRV_LOG(NOTICE, "Removing VF portid %d", port_id);
	rte_eal_alarm_set(1, hn_remove_delayed, hv);

	return 0;
}

static int hn_setup_vf_queues(int port, struct rte_eth_dev *dev)
{
	struct hn_rx_queue *rx_queue;
	struct rte_eth_txq_info txinfo;
	struct rte_eth_rxq_info rxinfo;
	int i, ret = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ret = rte_eth_tx_queue_info_get(dev->data->port_id, i, &txinfo);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "rte_eth_tx_queue_info_get failed ret=%d",
				    ret);
			return ret;
		}

		ret = rte_eth_tx_queue_setup(port, i, txinfo.nb_desc, 0,
					     &txinfo.conf);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "rte_eth_tx_queue_setup failed ret=%d",
				    ret);
			return ret;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ret = rte_eth_rx_queue_info_get(dev->data->port_id, i, &rxinfo);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "rte_eth_rx_queue_info_get failed ret=%d",
				    ret);
			return ret;
		}

		rx_queue = dev->data->rx_queues[i];

		ret = rte_eth_rx_queue_setup(port, i, rxinfo.nb_desc, 0,
					     &rxinfo.conf, rx_queue->mb_pool);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "rte_eth_rx_queue_setup failed ret=%d",
				    ret);
			return ret;
		}
	}

	return ret;
}

int hn_vf_add(struct rte_eth_dev *dev, struct hn_data *hv);

static void hn_vf_add_retry(void *args)
{
	struct rte_eth_dev *dev = args;
	struct hn_data *hv = dev->data->dev_private;

	hn_vf_add(dev, hv);
}

int hn_vf_configure(struct rte_eth_dev *dev,
		    const struct rte_eth_conf *dev_conf);

/* Add new VF device to synthetic device */
int hn_vf_add(struct rte_eth_dev *dev, struct hn_data *hv)
{
	int ret, port;

	if (!hv->vf_ctx.vf_vsp_reported || hv->vf_ctx.vf_vsc_switched)
		return 0;

	rte_rwlock_write_lock(&hv->vf_lock);

	ret = hn_vf_attach(dev, hv);
	if (ret) {
		PMD_DRV_LOG(NOTICE,
			    "RNDIS reports VF but device not found, retrying");
		rte_eal_alarm_set(1000000, hn_vf_add_retry, dev);
		goto exit;
	}

	port = hv->vf_ctx.vf_port;

	/* If the primary device has started, this is a VF host add.
	 * Configure and start VF device.
	 */
	if (dev->data->dev_started) {
		if (rte_eth_devices[port].data->dev_started) {
			PMD_DRV_LOG(ERR, "VF already started on hot add");
			goto exit;
		}

		PMD_DRV_LOG(NOTICE, "configuring VF port %d", port);
		ret = hn_vf_configure(dev, &dev->data->dev_conf);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to configure VF port %d",
				    port);
			goto exit;
		}

		ret = hn_setup_vf_queues(port, dev);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "Failed to configure VF queues port %d",
				    port);
			goto exit;
		}

		PMD_DRV_LOG(NOTICE, "Starting VF port %d", port);
		ret = rte_eth_dev_start(port);
		if (ret) {
			PMD_DRV_LOG(ERR, "rte_eth_dev_start failed ret=%d",
				    ret);
			goto exit;
		}
		hv->vf_ctx.vf_state = vf_started;
	}

	ret = hn_nvs_set_datapath(hv, NVS_DATAPATH_VF);
	if (ret == 0)
		hv->vf_ctx.vf_vsc_switched = true;

exit:
	rte_rwlock_write_unlock(&hv->vf_lock);
	return ret;
}

/* Switch data path to VF device */
static void hn_vf_remove(struct hn_data *hv)
{
	int ret;

	if (!hv->vf_ctx.vf_vsc_switched) {
		PMD_DRV_LOG(ERR, "VF path not active");
		return;
	}

	rte_rwlock_write_lock(&hv->vf_lock);
	if (!hv->vf_ctx.vf_vsc_switched) {
		PMD_DRV_LOG(ERR, "VF path not active");
	} else {
		/* Stop incoming packets from arriving on VF */
		ret = hn_nvs_set_datapath(hv, NVS_DATAPATH_SYNTHETIC);
		if (ret == 0)
			hv->vf_ctx.vf_vsc_switched = false;
	}
	rte_rwlock_write_unlock(&hv->vf_lock);
}

/* Handle VF association message from host */
void
hn_nvs_handle_vfassoc(struct rte_eth_dev *dev,
		      const struct vmbus_chanpkt_hdr *hdr,
		      const void *data)
{
	struct hn_data *hv = dev->data->dev_private;
	const struct hn_nvs_vf_association *vf_assoc = data;

	if (unlikely(vmbus_chanpkt_datalen(hdr) < sizeof(*vf_assoc))) {
		PMD_DRV_LOG(ERR, "invalid vf association NVS");
		return;
	}

	PMD_DRV_LOG(DEBUG, "VF serial %u %s port %u",
		    vf_assoc->serial,
		    vf_assoc->allocated ? "add to" : "remove from",
		    dev->data->port_id);

	hv->vf_ctx.vf_vsp_reported = vf_assoc->allocated;

	if (dev->state == RTE_ETH_DEV_ATTACHED) {
		if (vf_assoc->allocated)
			hn_vf_add(dev, hv);
		else
			hn_vf_remove(hv);
	}
}

static void
hn_vf_merge_desc_lim(struct rte_eth_desc_lim *lim,
		     const struct rte_eth_desc_lim *vf_lim)
{
	lim->nb_max = RTE_MIN(vf_lim->nb_max, lim->nb_max);
	lim->nb_min = RTE_MAX(vf_lim->nb_min, lim->nb_min);
	lim->nb_align = RTE_MAX(vf_lim->nb_align, lim->nb_align);
	lim->nb_seg_max = RTE_MIN(vf_lim->nb_seg_max, lim->nb_seg_max);
	lim->nb_mtu_seg_max = RTE_MIN(vf_lim->nb_seg_max, lim->nb_seg_max);
}

/*
 * Merge the info from the VF and synthetic path.
 * use the default config of the VF
 * and the minimum number of queues and buffer sizes.
 */
static int hn_vf_info_merge(struct rte_eth_dev *vf_dev,
			     struct rte_eth_dev_info *info)
{
	struct rte_eth_dev_info vf_info;
	int ret;

	ret = rte_eth_dev_info_get(vf_dev->data->port_id, &vf_info);
	if (ret != 0)
		return ret;

	info->speed_capa = vf_info.speed_capa;
	info->default_rxportconf = vf_info.default_rxportconf;
	info->default_txportconf = vf_info.default_txportconf;

	info->max_rx_queues = RTE_MIN(vf_info.max_rx_queues,
				      info->max_rx_queues);
	info->rx_offload_capa &= vf_info.rx_offload_capa;
	info->rx_queue_offload_capa &= vf_info.rx_queue_offload_capa;
	info->flow_type_rss_offloads &= vf_info.flow_type_rss_offloads;

	info->max_tx_queues = RTE_MIN(vf_info.max_tx_queues,
				      info->max_tx_queues);
	info->tx_offload_capa &= vf_info.tx_offload_capa;
	info->tx_queue_offload_capa &= vf_info.tx_queue_offload_capa;
	hn_vf_merge_desc_lim(&info->tx_desc_lim, &vf_info.tx_desc_lim);

	info->min_rx_bufsize = RTE_MAX(vf_info.min_rx_bufsize,
				       info->min_rx_bufsize);
	info->max_rx_pktlen  = RTE_MAX(vf_info.max_rx_pktlen,
				       info->max_rx_pktlen);
	hn_vf_merge_desc_lim(&info->rx_desc_lim, &vf_info.rx_desc_lim);

	return 0;
}

int hn_vf_info_get(struct hn_data *hv, struct rte_eth_dev_info *info)
{
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = hn_vf_info_merge(vf_dev, info);
	rte_rwlock_read_unlock(&hv->vf_lock);
	return ret;
}

int hn_vf_configure(struct rte_eth_dev *dev,
		    const struct rte_eth_conf *dev_conf)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_conf vf_conf = *dev_conf;
	int ret = 0;

	/* link state interrupt does not matter here. */
	vf_conf.intr_conf.lsc = 0;

	/* need to monitor removal event */
	vf_conf.intr_conf.rmv = 1;

	if (hv->vf_ctx.vf_attached) {
		ret = rte_eth_dev_callback_register(hv->vf_ctx.vf_port,
						    RTE_ETH_EVENT_INTR_RMV,
						    hn_eth_rmv_event_callback,
						    hv);
		if (ret) {
			PMD_DRV_LOG(ERR,
				    "Registering callback failed for vf port %d ret %d",
				    hv->vf_ctx.vf_port, ret);
			return ret;
		}

		ret = rte_eth_dev_configure(hv->vf_ctx.vf_port,
					    dev->data->nb_rx_queues,
					    dev->data->nb_tx_queues,
					    &vf_conf);
		if (ret) {
			PMD_DRV_LOG(ERR, "VF configuration failed: %d", ret);

			rte_eth_dev_callback_unregister(hv->vf_ctx.vf_port,
							RTE_ETH_EVENT_INTR_RMV,
							hn_eth_rmv_event_callback,
							hv);

			return ret;
		}

		hv->vf_ctx.vf_state = vf_configured;
	}

	return ret;
}

/* Configure VF if present.
 * VF device will have the same number of queues as the synthetic device
 */
int hn_vf_configure_locked(struct rte_eth_dev *dev,
			   const struct rte_eth_conf *dev_conf)
{
	struct hn_data *hv = dev->data->dev_private;
	int ret = 0;

	rte_rwlock_write_lock(&hv->vf_lock);
	ret = hn_vf_configure(dev, dev_conf);
	rte_rwlock_write_unlock(&hv->vf_lock);

	return ret;
}

const uint32_t *hn_vf_supported_ptypes(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	const uint32_t *ptypes = NULL;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev && vf_dev->dev_ops->dev_supported_ptypes_get)
		ptypes = (*vf_dev->dev_ops->dev_supported_ptypes_get)(vf_dev);
	rte_rwlock_read_unlock(&hv->vf_lock);

	return ptypes;
}

int hn_vf_start(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = rte_eth_dev_start(vf_dev->data->port_id);
	rte_rwlock_read_unlock(&hv->vf_lock);
	return ret;
}

int hn_vf_stop(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev) {
		ret = rte_eth_dev_stop(vf_dev->data->port_id);
		if (ret != 0)
			PMD_DRV_LOG(ERR, "Failed to stop device on port %u",
				    vf_dev->data->port_id);
	}
	rte_rwlock_read_unlock(&hv->vf_lock);

	return ret;
}

/* If VF is present, then cascade configuration down */
#define VF_ETHDEV_FUNC(dev, func)				\
	{							\
		struct hn_data *hv = (dev)->data->dev_private;	\
		struct rte_eth_dev *vf_dev;			\
		rte_rwlock_read_lock(&hv->vf_lock);		\
		vf_dev = hn_get_vf_dev(hv);			\
		if (vf_dev)					\
			func(vf_dev->data->port_id);		\
		rte_rwlock_read_unlock(&hv->vf_lock);		\
	}

/* If VF is present, then cascade configuration down */
#define VF_ETHDEV_FUNC_RET_STATUS(dev, func)			\
	{							\
		struct hn_data *hv = (dev)->data->dev_private;	\
		struct rte_eth_dev *vf_dev;			\
		int ret = 0;					\
		rte_rwlock_read_lock(&hv->vf_lock);		\
		vf_dev = hn_get_vf_dev(hv);			\
		if (vf_dev)					\
			ret = func(vf_dev->data->port_id);	\
		rte_rwlock_read_unlock(&hv->vf_lock);		\
		return ret;					\
	}

void hn_vf_reset(struct rte_eth_dev *dev)
{
	VF_ETHDEV_FUNC(dev, rte_eth_dev_reset);
}

int hn_vf_close(struct rte_eth_dev *dev)
{
	int ret = 0;
	struct hn_data *hv = dev->data->dev_private;

	rte_eal_alarm_cancel(hn_vf_add_retry, dev);

	rte_rwlock_read_lock(&hv->vf_lock);
	if (hv->vf_ctx.vf_attached) {
		rte_eth_dev_callback_unregister(hv->vf_ctx.vf_port,
						RTE_ETH_EVENT_INTR_RMV,
						hn_eth_rmv_event_callback,
						hv);
		rte_eal_alarm_cancel(hn_remove_delayed, hv);
		ret = rte_eth_dev_close(hv->vf_ctx.vf_port);
		hv->vf_ctx.vf_attached = false;
	}
	rte_rwlock_read_unlock(&hv->vf_lock);

	return ret;
}

int hn_vf_stats_reset(struct rte_eth_dev *dev)
{
	VF_ETHDEV_FUNC_RET_STATUS(dev, rte_eth_stats_reset);
}

int hn_vf_allmulticast_enable(struct rte_eth_dev *dev)
{
	VF_ETHDEV_FUNC_RET_STATUS(dev, rte_eth_allmulticast_enable);
}

int hn_vf_allmulticast_disable(struct rte_eth_dev *dev)
{
	VF_ETHDEV_FUNC_RET_STATUS(dev, rte_eth_allmulticast_disable);
}

int hn_vf_promiscuous_enable(struct rte_eth_dev *dev)
{
	VF_ETHDEV_FUNC_RET_STATUS(dev, rte_eth_promiscuous_enable);
}

int hn_vf_promiscuous_disable(struct rte_eth_dev *dev)
{
	VF_ETHDEV_FUNC_RET_STATUS(dev, rte_eth_promiscuous_disable);
}

int hn_vf_mc_addr_list(struct rte_eth_dev *dev,
			struct rte_ether_addr *mc_addr_set,
			uint32_t nb_mc_addr)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = rte_eth_dev_set_mc_addr_list(vf_dev->data->port_id,
						   mc_addr_set, nb_mc_addr);
	rte_rwlock_read_unlock(&hv->vf_lock);
	return ret;
}

int hn_vf_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx, uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = rte_eth_tx_queue_setup(vf_dev->data->port_id,
					     queue_idx, nb_desc,
					     socket_id, tx_conf);
	rte_rwlock_read_unlock(&hv->vf_lock);
	return ret;
}

void hn_vf_tx_queue_release(struct hn_data *hv, uint16_t queue_id)
{
	struct rte_eth_dev *vf_dev;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev && vf_dev->dev_ops->tx_queue_release)
		(*vf_dev->dev_ops->tx_queue_release)(vf_dev, queue_id);

	rte_rwlock_read_unlock(&hv->vf_lock);
}

int hn_vf_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx, uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = rte_eth_rx_queue_setup(vf_dev->data->port_id,
					     queue_idx, nb_desc,
					     socket_id, rx_conf, mp);
	rte_rwlock_read_unlock(&hv->vf_lock);
	return ret;
}

void hn_vf_rx_queue_release(struct hn_data *hv, uint16_t queue_id)
{
	struct rte_eth_dev *vf_dev;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev && vf_dev->dev_ops->rx_queue_release)
		(*vf_dev->dev_ops->rx_queue_release)(vf_dev, queue_id);
	rte_rwlock_read_unlock(&hv->vf_lock);
}

int hn_vf_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = rte_eth_stats_get(vf_dev->data->port_id, stats);
	rte_rwlock_read_unlock(&hv->vf_lock);
	return ret;
}

int hn_vf_xstats_get_names(struct rte_eth_dev *dev,
			   struct rte_eth_xstat_name *names,
			   unsigned int n)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int i, count = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		count = rte_eth_xstats_get_names(vf_dev->data->port_id,
						 names, n);
	rte_rwlock_read_unlock(&hv->vf_lock);

	/* add vf_ prefix to xstat names */
	if (names) {
		for (i = 0; i < count; i++) {
			char tmp[RTE_ETH_XSTATS_NAME_SIZE];

			snprintf(tmp, sizeof(tmp), "vf_%s", names[i].name);
			strlcpy(names[i].name, tmp, sizeof(names[i].name));
		}
	}

	return count;
}

int hn_vf_xstats_get(struct rte_eth_dev *dev,
		     struct rte_eth_xstat *xstats,
		     unsigned int offset,
		     unsigned int n)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int i, count = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		count = rte_eth_xstats_get(vf_dev->data->port_id,
					   xstats + offset, n - offset);
	rte_rwlock_read_unlock(&hv->vf_lock);

	/* Offset id's for VF stats */
	if (count > 0) {
		for (i = 0; i < count; i++)
			xstats[i + offset].id += offset;
	}

	return count;
}

int hn_vf_xstats_reset(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev)
		ret = rte_eth_xstats_reset(vf_dev->data->port_id);
	else
		ret = -EINVAL;
	rte_rwlock_read_unlock(&hv->vf_lock);

	return ret;
}

int hn_vf_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev && vf_dev->dev_ops->rss_hash_update)
		ret = vf_dev->dev_ops->rss_hash_update(vf_dev, rss_conf);
	rte_rwlock_read_unlock(&hv->vf_lock);

	return ret;
}

int hn_vf_reta_hash_update(struct rte_eth_dev *dev,
			   struct rte_eth_rss_reta_entry64 *reta_conf,
			   uint16_t reta_size)
{
	struct hn_data *hv = dev->data->dev_private;
	struct rte_eth_dev *vf_dev;
	int ret = 0;

	rte_rwlock_read_lock(&hv->vf_lock);
	vf_dev = hn_get_vf_dev(hv);
	if (vf_dev && vf_dev->dev_ops->reta_update)
		ret = vf_dev->dev_ops->reta_update(vf_dev,
						   reta_conf, reta_size);
	rte_rwlock_read_unlock(&hv->vf_lock);

	return ret;
}
