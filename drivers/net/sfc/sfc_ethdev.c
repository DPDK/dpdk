/*-
 * Copyright (c) 2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_pci.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_kvargs.h"
#include "sfc_ev.h"
#include "sfc_rx.h"


static void
sfc_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	dev_info->pci_dev = RTE_DEV_TO_PCI(dev->device);
	dev_info->max_rx_pktlen = EFX_MAC_PDU_MAX;

	dev_info->max_rx_queues = sa->rxq_max;
	dev_info->max_tx_queues = sa->txq_max;

	/* By default packets are dropped if no descriptors are available */
	dev_info->default_rxconf.rx_drop_en = 1;

	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM;

	dev_info->default_txconf.txq_flags = ETH_TXQ_FLAGS_NOVLANOFFL |
					     ETH_TXQ_FLAGS_NOXSUMSCTP;

	dev_info->rx_desc_lim.nb_max = EFX_RXQ_MAXNDESCS;
	dev_info->rx_desc_lim.nb_min = EFX_RXQ_MINNDESCS;
	/* The RXQ hardware requires that the descriptor count is a power
	 * of 2, but rx_desc_lim cannot properly describe that constraint.
	 */
	dev_info->rx_desc_lim.nb_align = EFX_RXQ_MINNDESCS;

	dev_info->tx_desc_lim.nb_max = sa->txq_max_entries;
	dev_info->tx_desc_lim.nb_min = EFX_TXQ_MINNDESCS;
	/*
	 * The TXQ hardware requires that the descriptor count is a power
	 * of 2, but tx_desc_lim cannot properly describe that constraint
	 */
	dev_info->tx_desc_lim.nb_align = EFX_TXQ_MINNDESCS;
}

static int
sfc_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *dev_data = dev->data;
	struct sfc_adapter *sa = dev_data->dev_private;
	int rc;

	sfc_log_init(sa, "entry n_rxq=%u n_txq=%u",
		     dev_data->nb_rx_queues, dev_data->nb_tx_queues);

	sfc_adapter_lock(sa);
	switch (sa->state) {
	case SFC_ADAPTER_CONFIGURED:
		sfc_close(sa);
		SFC_ASSERT(sa->state == SFC_ADAPTER_INITIALIZED);
		/* FALLTHROUGH */
	case SFC_ADAPTER_INITIALIZED:
		rc = sfc_configure(sa);
		break;
	default:
		sfc_err(sa, "unexpected adapter state %u to configure",
			sa->state);
		rc = EINVAL;
		break;
	}
	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done %d", rc);
	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_start(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	rc = sfc_start(sa);
	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done %d", rc);
	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_eth_link *dev_link = &dev->data->dev_link;
	struct rte_eth_link old_link;
	struct rte_eth_link current_link;

	sfc_log_init(sa, "entry");

	if (sa->state != SFC_ADAPTER_STARTED)
		return 0;

retry:
	EFX_STATIC_ASSERT(sizeof(*dev_link) == sizeof(rte_atomic64_t));
	*(int64_t *)&old_link = rte_atomic64_read((rte_atomic64_t *)dev_link);

	if (wait_to_complete) {
		efx_link_mode_t link_mode;

		efx_port_poll(sa->nic, &link_mode);
		sfc_port_link_mode_to_info(link_mode, &current_link);

		if (!rte_atomic64_cmpset((volatile uint64_t *)dev_link,
					 *(uint64_t *)&old_link,
					 *(uint64_t *)&current_link))
			goto retry;
	} else {
		sfc_ev_mgmt_qpoll(sa);
		*(int64_t *)&current_link =
			rte_atomic64_read((rte_atomic64_t *)dev_link);
	}

	if (old_link.link_status != current_link.link_status)
		sfc_info(sa, "Link status is %s",
			 current_link.link_status ? "UP" : "DOWN");

	return old_link.link_status == current_link.link_status ? 0 : -1;
}

static void
sfc_dev_stop(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	sfc_stop(sa);
	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");
}

static void
sfc_dev_close(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	switch (sa->state) {
	case SFC_ADAPTER_STARTED:
		sfc_stop(sa);
		SFC_ASSERT(sa->state == SFC_ADAPTER_CONFIGURED);
		/* FALLTHROUGH */
	case SFC_ADAPTER_CONFIGURED:
		sfc_close(sa);
		SFC_ASSERT(sa->state == SFC_ADAPTER_INITIALIZED);
		/* FALLTHROUGH */
	case SFC_ADAPTER_INITIALIZED:
		break;
	default:
		sfc_err(sa, "unexpected adapter state %u on close", sa->state);
		break;
	}
	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");
}

static int
sfc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		   uint16_t nb_rx_desc, unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf,
		   struct rte_mempool *mb_pool)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	sfc_log_init(sa, "RxQ=%u nb_rx_desc=%u socket_id=%u",
		     rx_queue_id, nb_rx_desc, socket_id);

	sfc_adapter_lock(sa);

	rc = sfc_rx_qinit(sa, rx_queue_id, nb_rx_desc, socket_id,
			  rx_conf, mb_pool);
	if (rc != 0)
		goto fail_rx_qinit;

	dev->data->rx_queues[rx_queue_id] = sa->rxq_info[rx_queue_id].rxq;

	sfc_adapter_unlock(sa);

	return 0;

fail_rx_qinit:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static void
sfc_rx_queue_release(void *queue)
{
	struct sfc_rxq *rxq = queue;
	struct sfc_adapter *sa;
	unsigned int sw_index;

	if (rxq == NULL)
		return;

	sa = rxq->evq->sa;
	sfc_adapter_lock(sa);

	sw_index = sfc_rxq_sw_index(rxq);

	sfc_log_init(sa, "RxQ=%u", sw_index);

	sa->eth_dev->data->rx_queues[sw_index] = NULL;

	sfc_rx_qfini(sa, sw_index);

	sfc_adapter_unlock(sa);
}

static const struct eth_dev_ops sfc_eth_dev_ops = {
	.dev_configure			= sfc_dev_configure,
	.dev_start			= sfc_dev_start,
	.dev_stop			= sfc_dev_stop,
	.dev_close			= sfc_dev_close,
	.link_update			= sfc_dev_link_update,
	.dev_infos_get			= sfc_dev_infos_get,
	.rx_queue_setup			= sfc_rx_queue_setup,
	.rx_queue_release		= sfc_rx_queue_release,
};

static int
sfc_eth_dev_init(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_pci_device *pci_dev = SFC_DEV_TO_PCI(dev);
	int rc;
	const efx_nic_cfg_t *encp;
	const struct ether_addr *from;

	/* Required for logging */
	sa->eth_dev = dev;

	/* Copy PCI device info to the dev->data */
	rte_eth_copy_pci_info(dev, pci_dev);

	rc = sfc_kvargs_parse(sa);
	if (rc != 0)
		goto fail_kvargs_parse;

	rc = sfc_kvargs_process(sa, SFC_KVARG_DEBUG_INIT,
				sfc_kvarg_bool_handler, &sa->debug_init);
	if (rc != 0)
		goto fail_kvarg_debug_init;

	sfc_log_init(sa, "entry");

	dev->data->mac_addrs = rte_zmalloc("sfc", ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		rc = ENOMEM;
		goto fail_mac_addrs;
	}

	sfc_adapter_lock_init(sa);
	sfc_adapter_lock(sa);

	sfc_log_init(sa, "attaching");
	rc = sfc_attach(sa);
	if (rc != 0)
		goto fail_attach;

	encp = efx_nic_cfg_get(sa->nic);

	/*
	 * The arguments are really reverse order in comparison to
	 * Linux kernel. Copy from NIC config to Ethernet device data.
	 */
	from = (const struct ether_addr *)(encp->enc_mac_addr);
	ether_addr_copy(from, &dev->data->mac_addrs[0]);

	dev->dev_ops = &sfc_eth_dev_ops;
	dev->rx_pkt_burst = &sfc_recv_pkts;

	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");
	return 0;

fail_attach:
	sfc_adapter_unlock(sa);
	sfc_adapter_lock_fini(sa);
	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

fail_mac_addrs:
fail_kvarg_debug_init:
	sfc_kvargs_cleanup(sa);

fail_kvargs_parse:
	sfc_log_init(sa, "failed %d", rc);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_eth_dev_uninit(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);

	sfc_detach(sa);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;

	sfc_kvargs_cleanup(sa);

	sfc_adapter_unlock(sa);
	sfc_adapter_lock_fini(sa);

	sfc_log_init(sa, "done");

	/* Required for logging, so cleanup last */
	sa->eth_dev = NULL;
	return 0;
}

static const struct rte_pci_id pci_id_sfc_efx_map[] = {
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_FARMINGDALE) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_GREENPORT) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD) },
	{ .vendor_id = 0 /* sentinel */ }
};

static struct eth_driver sfc_efx_pmd = {
	.pci_drv = {
		.id_table = pci_id_sfc_efx_map,
		.drv_flags =
			RTE_PCI_DRV_NEED_MAPPING,
		.probe = rte_eth_dev_pci_probe,
		.remove = rte_eth_dev_pci_remove,
	},
	.eth_dev_init = sfc_eth_dev_init,
	.eth_dev_uninit = sfc_eth_dev_uninit,
	.dev_private_size = sizeof(struct sfc_adapter),
};

RTE_PMD_REGISTER_PCI(net_sfc_efx, sfc_efx_pmd.pci_drv);
RTE_PMD_REGISTER_PCI_TABLE(net_sfc_efx, pci_id_sfc_efx_map);
RTE_PMD_REGISTER_PARAM_STRING(net_sfc_efx,
	SFC_KVARG_DEBUG_INIT "=" SFC_KVARG_VALUES_BOOL);
