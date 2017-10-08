/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
 *     * Neither the name of Cavium networks nor the names of its
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
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_alarm.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_devargs.h>
#include <rte_dev.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_vdev.h>

#include "octeontx_ethdev.h"
#include "octeontx_logs.h"

struct octeontx_vdev_init_params {
	uint8_t	nr_port;
};

enum octeontx_link_speed {
	OCTEONTX_LINK_SPEED_SGMII,
	OCTEONTX_LINK_SPEED_XAUI,
	OCTEONTX_LINK_SPEED_RXAUI,
	OCTEONTX_LINK_SPEED_10G_R,
	OCTEONTX_LINK_SPEED_40G_R,
	OCTEONTX_LINK_SPEED_RESERVE1,
	OCTEONTX_LINK_SPEED_QSGMII,
	OCTEONTX_LINK_SPEED_RESERVE2
};

/* Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = (int *)extra_args;

	*i = atoi(value);
	if (*i < 0) {
		octeontx_log_err("argument has to be positive.");
		return -1;
	}

	return 0;
}

static int
octeontx_parse_vdev_init_params(struct octeontx_vdev_init_params *params,
				struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	static const char * const octeontx_vdev_valid_params[] = {
		OCTEONTX_VDEV_NR_PORT_ARG,
		NULL
	};

	const char *input_args = rte_vdev_device_args(dev);
	if (params == NULL)
		return -EINVAL;


	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				octeontx_vdev_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
					OCTEONTX_VDEV_NR_PORT_ARG,
					&parse_integer_arg,
					&params->nr_port);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
octeontx_port_open(struct octeontx_nic *nic)
{
	octeontx_mbox_bgx_port_conf_t bgx_port_conf;
	int res;

	res = 0;

	PMD_INIT_FUNC_TRACE();

	res = octeontx_bgx_port_open(nic->port_id, &bgx_port_conf);
	if (res < 0) {
		octeontx_log_err("failed to open port %d", res);
		return res;
	}

	nic->node = bgx_port_conf.node;
	nic->port_ena = bgx_port_conf.enable;
	nic->base_ichan = bgx_port_conf.base_chan;
	nic->base_ochan = bgx_port_conf.base_chan;
	nic->num_ichans = bgx_port_conf.num_chans;
	nic->num_ochans = bgx_port_conf.num_chans;
	nic->mtu = bgx_port_conf.mtu;
	nic->bpen = bgx_port_conf.bpen;
	nic->fcs_strip = bgx_port_conf.fcs_strip;
	nic->bcast_mode = bgx_port_conf.bcast_mode;
	nic->mcast_mode = bgx_port_conf.mcast_mode;
	nic->speed	= bgx_port_conf.mode;

	memcpy(&nic->mac_addr[0], &bgx_port_conf.macaddr[0], ETHER_ADDR_LEN);

	octeontx_log_dbg("port opened %d", nic->port_id);
	return res;
}

static void
octeontx_port_close(struct octeontx_nic *nic)
{
	PMD_INIT_FUNC_TRACE();

	octeontx_bgx_port_close(nic->port_id);
	octeontx_log_dbg("port closed %d", nic->port_id);
}

static void
octeontx_port_promisc_set(struct octeontx_nic *nic, int en)
{
	struct rte_eth_dev *dev;
	int res;

	res = 0;
	PMD_INIT_FUNC_TRACE();
	dev = nic->dev;

	res = octeontx_bgx_port_promisc_set(nic->port_id, en);
	if (res < 0)
		octeontx_log_err("failed to set promiscuous mode %d",
				nic->port_id);

	/* Set proper flag for the mode */
	dev->data->promiscuous = (en != 0) ? 1 : 0;

	octeontx_log_dbg("port %d : promiscuous mode %s",
			nic->port_id, en ? "set" : "unset");
}

static void
octeontx_port_stats(struct octeontx_nic *nic, struct rte_eth_stats *stats)
{
	octeontx_mbox_bgx_port_stats_t bgx_stats;
	int res;

	PMD_INIT_FUNC_TRACE();

	res = octeontx_bgx_port_stats(nic->port_id, &bgx_stats);
	if (res < 0)
		octeontx_log_err("failed to get port stats %d", nic->port_id);

	stats->ipackets = bgx_stats.rx_packets;
	stats->ibytes = bgx_stats.rx_bytes;
	stats->imissed = bgx_stats.rx_dropped;
	stats->ierrors = bgx_stats.rx_errors;
	stats->opackets = bgx_stats.tx_packets;
	stats->obytes = bgx_stats.tx_bytes;
	stats->oerrors = bgx_stats.tx_errors;

	octeontx_log_dbg("port%d stats inpkts=%" PRIx64 " outpkts=%" PRIx64 "",
			nic->port_id, stats->ipackets, stats->opackets);
}

static void
octeontx_port_stats_clr(struct octeontx_nic *nic)
{
	PMD_INIT_FUNC_TRACE();

	octeontx_bgx_port_stats_clr(nic->port_id);
}

static inline void
devconf_set_default_sane_values(struct rte_event_dev_config *dev_conf,
				struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;

	dev_conf->nb_event_ports = info->max_event_ports;
	dev_conf->nb_event_queues = info->max_event_queues;

	dev_conf->nb_event_queue_flows = info->max_event_queue_flows;
	dev_conf->nb_event_port_dequeue_depth =
			info->max_event_port_dequeue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_event_port_enqueue_depth =
			info->max_event_port_enqueue_depth;
	dev_conf->nb_events_limit =
			info->max_num_events;
}

static int
octeontx_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(conf);

	if (!rte_eal_has_hugepages()) {
		octeontx_log_err("huge page is not configured");
		return -EINVAL;
	}

	if (txmode->mq_mode) {
		octeontx_log_err("tx mq_mode DCB or VMDq not supported");
		return -EINVAL;
	}

	if (rxmode->mq_mode != ETH_MQ_RX_NONE &&
		rxmode->mq_mode != ETH_MQ_RX_RSS) {
		octeontx_log_err("unsupported rx qmode %d", rxmode->mq_mode);
		return -EINVAL;
	}

	if (!rxmode->hw_strip_crc) {
		PMD_INIT_LOG(NOTICE, "can't disable hw crc strip");
		rxmode->hw_strip_crc = 1;
	}

	if (rxmode->hw_ip_checksum) {
		PMD_INIT_LOG(NOTICE, "rxcksum not supported");
		rxmode->hw_ip_checksum = 0;
	}

	if (rxmode->split_hdr_size) {
		octeontx_log_err("rxmode does not support split header");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_filter) {
		octeontx_log_err("VLAN filter not supported");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_extend) {
		octeontx_log_err("VLAN extended not supported");
		return -EINVAL;
	}

	if (rxmode->enable_lro) {
		octeontx_log_err("LRO not supported");
		return -EINVAL;
	}

	if (conf->link_speeds & ETH_LINK_SPEED_FIXED) {
		octeontx_log_err("setting link speed/duplex not supported");
		return -EINVAL;
	}

	if (conf->dcb_capability_en) {
		octeontx_log_err("DCB enable not supported");
		return -EINVAL;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		octeontx_log_err("flow director not supported");
		return -EINVAL;
	}

	nic->num_tx_queues = dev->data->nb_tx_queues;

	ret = octeontx_pko_channel_open(nic->port_id * PKO_VF_NUM_DQ,
					nic->num_tx_queues,
					nic->base_ochan);
	if (ret) {
		octeontx_log_err("failed to open channel %d no-of-txq %d",
			   nic->base_ochan, nic->num_tx_queues);
		return -EFAULT;
	}

	nic->pki.classifier_enable = false;
	nic->pki.hash_enable = true;
	nic->pki.initialized = false;

	return 0;
}

static void
octeontx_dev_promisc_enable(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	octeontx_port_promisc_set(nic, 1);
}

static void
octeontx_dev_promisc_disable(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	octeontx_port_promisc_set(nic, 0);
}

static inline int
octeontx_atomic_write_link_status(struct rte_eth_dev *dev,
				  struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
		*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static int
octeontx_port_link_status(struct octeontx_nic *nic)
{
	int res;

	PMD_INIT_FUNC_TRACE();
	res = octeontx_bgx_port_link_status(nic->port_id);
	if (res < 0) {
		octeontx_log_err("failed to get port %d link status",
				nic->port_id);
		return res;
	}

	nic->link_up = (uint8_t)res;
	octeontx_log_dbg("port %d link status %d", nic->port_id, nic->link_up);

	return res;
}

/*
 * Return 0 means link status changed, -1 means not changed
 */
static int
octeontx_dev_link_update(struct rte_eth_dev *dev,
			 int wait_to_complete __rte_unused)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	struct rte_eth_link link;
	int res;

	res = 0;
	PMD_INIT_FUNC_TRACE();

	res = octeontx_port_link_status(nic);
	if (res < 0) {
		octeontx_log_err("failed to request link status %d", res);
		return res;
	}

	link.link_status = nic->link_up;

	switch (nic->speed) {
	case OCTEONTX_LINK_SPEED_SGMII:
		link.link_speed = ETH_SPEED_NUM_1G;
		break;

	case OCTEONTX_LINK_SPEED_XAUI:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;

	case OCTEONTX_LINK_SPEED_RXAUI:
	case OCTEONTX_LINK_SPEED_10G_R:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case OCTEONTX_LINK_SPEED_QSGMII:
		link.link_speed = ETH_SPEED_NUM_5G;
		break;
	case OCTEONTX_LINK_SPEED_40G_R:
		link.link_speed = ETH_SPEED_NUM_40G;
		break;

	case OCTEONTX_LINK_SPEED_RESERVE1:
	case OCTEONTX_LINK_SPEED_RESERVE2:
	default:
		octeontx_log_err("incorrect link speed %d", nic->speed);
		break;
	}

	link.link_duplex = ETH_LINK_AUTONEG;
	link.link_autoneg = ETH_LINK_SPEED_AUTONEG;

	return octeontx_atomic_write_link_status(dev, &link);
}

static void
octeontx_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	octeontx_port_stats(nic, stats);
}

static void
octeontx_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();
	octeontx_port_stats_clr(nic);
}

static void
octeontx_dev_default_mac_addr_set(struct rte_eth_dev *dev,
					struct ether_addr *addr)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);
	int ret;

	ret = octeontx_bgx_port_mac_set(nic->port_id, addr->addr_bytes);
	if (ret != 0)
		octeontx_log_err("failed to set MAC address on port %d",
				nic->port_id);
}

static void
octeontx_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	RTE_SET_USED(dev);

	/* Autonegotiation may be disabled */
	dev_info->speed_capa = ETH_LINK_SPEED_FIXED;
	dev_info->speed_capa |= ETH_LINK_SPEED_10M | ETH_LINK_SPEED_100M |
			ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G |
			ETH_LINK_SPEED_40G;

	dev_info->driver_name = RTE_STR(rte_octeontx_pmd);
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = PKI_MAX_PKTLEN;
	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = PKO_MAX_NUM_DQ;
	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = 0,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = 0,
		.txq_flags =
			ETH_TXQ_FLAGS_NOMULTSEGS |
			ETH_TXQ_FLAGS_NOOFFLOADS |
			ETH_TXQ_FLAGS_NOXSUMS,
	};

	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_MT_LOCKFREE;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops octeontx_dev_ops = {
	.dev_configure		 = octeontx_dev_configure,
	.dev_infos_get		 = octeontx_dev_info,
	.promiscuous_enable	 = octeontx_dev_promisc_enable,
	.promiscuous_disable	 = octeontx_dev_promisc_disable,
	.link_update		 = octeontx_dev_link_update,
	.stats_get		 = octeontx_dev_stats_get,
	.stats_reset		 = octeontx_dev_stats_reset,
	.mac_addr_set		 = octeontx_dev_default_mac_addr_set,
};

/* Create Ethdev interface per BGX LMAC ports */
static int
octeontx_create(struct rte_vdev_device *dev, int port, uint8_t evdev,
			int socket_id)
{
	int res;
	char octtx_name[OCTEONTX_MAX_NAME_LEN];
	struct octeontx_nic *nic = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_eth_dev_data *data = NULL;
	const char *name = rte_vdev_device_name(dev);

	PMD_INIT_FUNC_TRACE();

	sprintf(octtx_name, "%s_%d", name, port);
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_attach_secondary(octtx_name);
		if (eth_dev == NULL)
			return -ENODEV;

		return 0;
	}

	data = rte_zmalloc_socket(octtx_name, sizeof(*data), 0, socket_id);
	if (data == NULL) {
		octeontx_log_err("failed to allocate devdata");
		res = -ENOMEM;
		goto err;
	}

	nic = rte_zmalloc_socket(octtx_name, sizeof(*nic), 0, socket_id);
	if (nic == NULL) {
		octeontx_log_err("failed to allocate nic structure");
		res = -ENOMEM;
		goto err;
	}

	nic->port_id = port;
	nic->evdev = evdev;

	res = octeontx_port_open(nic);
	if (res < 0)
		goto err;

	/* Rx side port configuration */
	res = octeontx_pki_port_open(port);
	if (res != 0) {
		octeontx_log_err("failed to open PKI port %d", port);
		res = -ENODEV;
		goto err;
	}

	/* Reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(octtx_name);
	if (eth_dev == NULL) {
		octeontx_log_err("failed to allocate rte_eth_dev");
		res = -ENOMEM;
		goto err;
	}

	eth_dev->device = &dev->device;
	eth_dev->intr_handle = NULL;
	eth_dev->data->kdrv = RTE_KDRV_NONE;
	eth_dev->data->numa_node = dev->device.numa_node;

	rte_memcpy(data, (eth_dev)->data, sizeof(*data));
	data->dev_private = nic;

	data->port_id = eth_dev->data->port_id;
	snprintf(data->name, sizeof(data->name), "%s", eth_dev->data->name);

	nic->ev_queues = 1;
	nic->ev_ports = 1;

	data->dev_link.link_status = ETH_LINK_DOWN;
	data->dev_started = 0;
	data->promiscuous = 0;
	data->all_multicast = 0;
	data->scattered_rx = 0;

	data->mac_addrs = rte_zmalloc_socket(octtx_name, ETHER_ADDR_LEN, 0,
							socket_id);
	if (data->mac_addrs == NULL) {
		octeontx_log_err("failed to allocate memory for mac_addrs");
		res = -ENOMEM;
		goto err;
	}

	eth_dev->data = data;
	eth_dev->dev_ops = &octeontx_dev_ops;

	/* Finally save ethdev pointer to the NIC structure */
	nic->dev = eth_dev;

	if (nic->port_id != data->port_id) {
		octeontx_log_err("eth_dev->port_id (%d) is diff to orig (%d)",
				data->port_id, nic->port_id);
		res = -EINVAL;
		goto err;
	}

	/* Update port_id mac to eth_dev */
	memcpy(data->mac_addrs, nic->mac_addr, ETHER_ADDR_LEN);

	PMD_INIT_LOG(DEBUG, "ethdev info: ");
	PMD_INIT_LOG(DEBUG, "port %d, port_ena %d ochan %d num_ochan %d tx_q %d",
				nic->port_id, nic->port_ena,
				nic->base_ochan, nic->num_ochans,
				nic->num_tx_queues);
	PMD_INIT_LOG(DEBUG, "speed %d mtu %d", nic->speed, nic->mtu);

	return data->port_id;

err:
	if (port)
		octeontx_port_close(nic);

	if (eth_dev != NULL) {
		rte_free(eth_dev->data->mac_addrs);
		rte_free(data);
		rte_free(nic);
		rte_eth_dev_release_port(eth_dev);
	}

	return res;
}

/* Un initialize octeontx device */
static int
octeontx_remove(struct rte_vdev_device *dev)
{
	char octtx_name[OCTEONTX_MAX_NAME_LEN];
	struct rte_eth_dev *eth_dev = NULL;
	struct octeontx_nic *nic = NULL;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT; i++) {
		sprintf(octtx_name, "eth_octeontx_%d", i);

		/* reserve an ethdev entry */
		eth_dev = rte_eth_dev_allocated(octtx_name);
		if (eth_dev == NULL)
			return -ENODEV;

		nic = octeontx_pmd_priv(eth_dev);
		rte_event_dev_stop(nic->evdev);
		PMD_INIT_LOG(INFO, "Closing octeontx device %s", octtx_name);

		rte_free(eth_dev->data->mac_addrs);
		rte_free(eth_dev->data->dev_private);
		rte_free(eth_dev->data);
		rte_eth_dev_release_port(eth_dev);
		rte_event_dev_close(nic->evdev);
	}

	/* Free FC resource */
	octeontx_pko_fc_free();

	return 0;
}

/* Initialize octeontx device */
static int
octeontx_probe(struct rte_vdev_device *dev)
{
	const char *dev_name;
	static int probe_once;
	uint8_t socket_id, qlist;
	int tx_vfcnt, port_id, evdev, qnum, pnum, res, i;
	struct rte_event_dev_config dev_conf;
	const char *eventdev_name = "event_octeontx";
	struct rte_event_dev_info info;

	struct octeontx_vdev_init_params init_params = {
		OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT
	};

	dev_name = rte_vdev_device_name(dev);
	res = octeontx_parse_vdev_init_params(&init_params, dev);
	if (res < 0)
		return -EINVAL;

	if (init_params.nr_port > OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT) {
		octeontx_log_err("nr_port (%d) > max (%d)", init_params.nr_port,
				OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT);
		return -ENOTSUP;
	}

	PMD_INIT_LOG(DEBUG, "initializing %s pmd", dev_name);

	socket_id = rte_socket_id();

	tx_vfcnt = octeontx_pko_vf_count();

	if (tx_vfcnt < init_params.nr_port) {
		octeontx_log_err("not enough PKO (%d) for port number (%d)",
				tx_vfcnt, init_params.nr_port);
		return -EINVAL;
	}
	evdev = rte_event_dev_get_dev_id(eventdev_name);
	if (evdev < 0) {
		octeontx_log_err("eventdev %s not found", eventdev_name);
		return -ENODEV;
	}

	res = rte_event_dev_info_get(evdev, &info);
	if (res < 0) {
		octeontx_log_err("failed to eventdev info %d", res);
		return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "max_queue %d max_port %d",
			info.max_event_queues, info.max_event_ports);

	if (octeontx_pko_init_fc(tx_vfcnt))
		return -ENOMEM;

	devconf_set_default_sane_values(&dev_conf, &info);
	res = rte_event_dev_configure(evdev, &dev_conf);
	if (res < 0)
		goto parse_error;

	rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_PORT_COUNT,
			(uint32_t *)&pnum);
	rte_event_dev_attr_get(evdev, RTE_EVENT_DEV_ATTR_QUEUE_COUNT,
			(uint32_t *)&qnum);
	if (pnum < qnum) {
		octeontx_log_err("too few event ports (%d) for event_q(%d)",
				pnum, qnum);
		res = -EINVAL;
		goto parse_error;
	}
	if (pnum > qnum) {
		/*
		 * We don't poll on event ports
		 * that do not have any queues assigned.
		 */
		pnum = qnum;
		PMD_INIT_LOG(INFO,
			"reducing number of active event ports to %d", pnum);
	}
	for (i = 0; i < qnum; i++) {
		res = rte_event_queue_setup(evdev, i, NULL);
		if (res < 0) {
			octeontx_log_err("failed to setup event_q(%d): res %d",
					i, res);
			goto parse_error;
		}
	}

	for (i = 0; i < pnum; i++) {
		res = rte_event_port_setup(evdev, i, NULL);
		if (res < 0) {
			res = -ENODEV;
			octeontx_log_err("failed to setup ev port(%d) res=%d",
						i, res);
			goto parse_error;
		}
		/* Link one queue to one event port */
		qlist = i;
		res = rte_event_port_link(evdev, i, &qlist, NULL, 1);
		if (res < 0) {
			res = -ENODEV;
			octeontx_log_err("failed to link port (%d): res=%d",
					i, res);
			goto parse_error;
		}
	}

	/* Create ethdev interface */
	for (i = 0; i < init_params.nr_port; i++) {
		port_id = octeontx_create(dev, i, evdev, socket_id);
		if (port_id < 0) {
			octeontx_log_err("failed to create device %s",
					dev_name);
			res = -ENODEV;
			goto parse_error;
		}

		PMD_INIT_LOG(INFO, "created ethdev %s for port %d", dev_name,
					port_id);
	}

	if (probe_once) {
		octeontx_log_err("interface %s not supported", dev_name);
		octeontx_remove(dev);
		res = -ENOTSUP;
		goto parse_error;
	}
	probe_once = 1;

	return 0;

parse_error:
	octeontx_pko_fc_free();
	return res;
}

static struct rte_vdev_driver octeontx_pmd_drv = {
	.probe = octeontx_probe,
	.remove = octeontx_remove,
};

RTE_PMD_REGISTER_VDEV(OCTEONTX_PMD, octeontx_pmd_drv);
RTE_PMD_REGISTER_ALIAS(OCTEONTX_PMD, eth_octeontx);
RTE_PMD_REGISTER_PARAM_STRING(OCTEONTX_PMD, "nr_port=<int> ");
