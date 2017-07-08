/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
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
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_errno.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_kvargs.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_tx.h"
#include "sfc_flow.h"
#include "sfc_dp.h"
#include "sfc_dp_rx.h"

static struct sfc_dp_list sfc_dp_head =
	TAILQ_HEAD_INITIALIZER(sfc_dp_head);

static int
sfc_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	efx_nic_fw_info_t enfi;
	int ret;
	int rc;

	/*
	 * Return value of the callback is likely supposed to be
	 * equal to or greater than 0, nevertheless, if an error
	 * occurs, it will be desirable to pass it to the caller
	 */
	if ((fw_version == NULL) || (fw_size == 0))
		return -EINVAL;

	rc = efx_nic_get_fw_version(sa->nic, &enfi);
	if (rc != 0)
		return -rc;

	ret = snprintf(fw_version, fw_size,
		       "%" PRIu16 ".%" PRIu16 ".%" PRIu16 ".%" PRIu16,
		       enfi.enfi_mc_fw_version[0], enfi.enfi_mc_fw_version[1],
		       enfi.enfi_mc_fw_version[2], enfi.enfi_mc_fw_version[3]);
	if (ret < 0)
		return ret;

	if (enfi.enfi_dpcpu_fw_ids_valid) {
		size_t dpcpu_fw_ids_offset = MIN(fw_size - 1, (size_t)ret);
		int ret_extra;

		ret_extra = snprintf(fw_version + dpcpu_fw_ids_offset,
				     fw_size - dpcpu_fw_ids_offset,
				     " rx%" PRIx16 " tx%" PRIx16,
				     enfi.enfi_rx_dpcpu_fw_id,
				     enfi.enfi_tx_dpcpu_fw_id);
		if (ret_extra < 0)
			return ret_extra;

		ret += ret_extra;
	}

	if (fw_size < (size_t)(++ret))
		return ret;
	else
		return 0;
}

static void
sfc_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);

	sfc_log_init(sa, "entry");

	dev_info->pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	dev_info->max_rx_pktlen = EFX_MAC_PDU_MAX;

	/* Autonegotiation may be disabled */
	dev_info->speed_capa = ETH_LINK_SPEED_FIXED;
	if (sa->port.phy_adv_cap_mask & EFX_PHY_CAP_1000FDX)
		dev_info->speed_capa |= ETH_LINK_SPEED_1G;
	if (sa->port.phy_adv_cap_mask & EFX_PHY_CAP_10000FDX)
		dev_info->speed_capa |= ETH_LINK_SPEED_10G;
	if (sa->port.phy_adv_cap_mask & EFX_PHY_CAP_40000FDX)
		dev_info->speed_capa |= ETH_LINK_SPEED_40G;

	dev_info->max_rx_queues = sa->rxq_max;
	dev_info->max_tx_queues = sa->txq_max;

	/* By default packets are dropped if no descriptors are available */
	dev_info->default_rxconf.rx_drop_en = 1;

	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM;

	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM;

	dev_info->default_txconf.txq_flags = ETH_TXQ_FLAGS_NOXSUMSCTP;
	if ((~sa->dp_tx->features & SFC_DP_TX_FEAT_VLAN_INSERT) ||
	    !encp->enc_hw_tx_insert_vlan_enabled)
		dev_info->default_txconf.txq_flags |= ETH_TXQ_FLAGS_NOVLANOFFL;
	else
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_VLAN_INSERT;

	if (~sa->dp_tx->features & SFC_DP_TX_FEAT_MULTI_SEG)
		dev_info->default_txconf.txq_flags |= ETH_TXQ_FLAGS_NOMULTSEGS;

#if EFSYS_OPT_RX_SCALE
	if (sa->rss_support != EFX_RX_SCALE_UNAVAILABLE) {
		dev_info->reta_size = EFX_RSS_TBL_SIZE;
		dev_info->hash_key_size = SFC_RSS_KEY_SIZE;
		dev_info->flow_type_rss_offloads = SFC_RSS_OFFLOADS;
	}
#endif

	if (sa->tso)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_TSO;

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

static const uint32_t *
sfc_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	return sa->dp_rx->supported_ptypes_get();
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

retry:
	EFX_STATIC_ASSERT(sizeof(*dev_link) == sizeof(rte_atomic64_t));
	*(int64_t *)&old_link = rte_atomic64_read((rte_atomic64_t *)dev_link);

	if (sa->state != SFC_ADAPTER_STARTED) {
		sfc_port_link_mode_to_info(EFX_LINK_UNKNOWN, &current_link);
		if (!rte_atomic64_cmpset((volatile uint64_t *)dev_link,
					 *(uint64_t *)&old_link,
					 *(uint64_t *)&current_link))
			goto retry;
	} else if (wait_to_complete) {
		efx_link_mode_t link_mode;

		if (efx_port_poll(sa->nic, &link_mode) != 0)
			link_mode = EFX_LINK_UNKNOWN;
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

static int
sfc_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	rc = sfc_start(sa);
	sfc_adapter_unlock(sa);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	sfc_stop(sa);
	sfc_adapter_unlock(sa);

	return 0;
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

static void
sfc_dev_filter_set(struct rte_eth_dev *dev, enum sfc_dev_filter_mode mode,
		   boolean_t enabled)
{
	struct sfc_port *port;
	boolean_t *toggle;
	struct sfc_adapter *sa = dev->data->dev_private;
	boolean_t allmulti = (mode == SFC_DEV_FILTER_MODE_ALLMULTI);
	const char *desc = (allmulti) ? "all-multi" : "promiscuous";

	sfc_adapter_lock(sa);

	port = &sa->port;
	toggle = (allmulti) ? (&port->allmulti) : (&port->promisc);

	if (*toggle != enabled) {
		*toggle = enabled;

		if (port->isolated) {
			sfc_warn(sa, "isolated mode is active on the port");
			sfc_warn(sa, "the change is to be applied on the next "
				     "start provided that isolated mode is "
				     "disabled prior the next start");
		} else if ((sa->state == SFC_ADAPTER_STARTED) &&
			   (sfc_set_rx_mode(sa) != 0)) {
			*toggle = !(enabled);
			sfc_warn(sa, "Failed to %s %s mode",
				 ((enabled) ? "enable" : "disable"), desc);
		}
	}

	sfc_adapter_unlock(sa);
}

static void
sfc_dev_promisc_enable(struct rte_eth_dev *dev)
{
	sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_PROMISC, B_TRUE);
}

static void
sfc_dev_promisc_disable(struct rte_eth_dev *dev)
{
	sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_PROMISC, B_FALSE);
}

static void
sfc_dev_allmulti_enable(struct rte_eth_dev *dev)
{
	sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_ALLMULTI, B_TRUE);
}

static void
sfc_dev_allmulti_disable(struct rte_eth_dev *dev)
{
	sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_ALLMULTI, B_FALSE);
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

	dev->data->rx_queues[rx_queue_id] = sa->rxq_info[rx_queue_id].rxq->dp;

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
	struct sfc_dp_rxq *dp_rxq = queue;
	struct sfc_rxq *rxq;
	struct sfc_adapter *sa;
	unsigned int sw_index;

	if (dp_rxq == NULL)
		return;

	rxq = sfc_rxq_by_dp_rxq(dp_rxq);
	sa = rxq->evq->sa;
	sfc_adapter_lock(sa);

	sw_index = sfc_rxq_sw_index(rxq);

	sfc_log_init(sa, "RxQ=%u", sw_index);

	sa->eth_dev->data->rx_queues[sw_index] = NULL;

	sfc_rx_qfini(sa, sw_index);

	sfc_adapter_unlock(sa);
}

static int
sfc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		   uint16_t nb_tx_desc, unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	sfc_log_init(sa, "TxQ = %u, nb_tx_desc = %u, socket_id = %u",
		     tx_queue_id, nb_tx_desc, socket_id);

	sfc_adapter_lock(sa);

	rc = sfc_tx_qinit(sa, tx_queue_id, nb_tx_desc, socket_id, tx_conf);
	if (rc != 0)
		goto fail_tx_qinit;

	dev->data->tx_queues[tx_queue_id] = sa->txq_info[tx_queue_id].txq->dp;

	sfc_adapter_unlock(sa);
	return 0;

fail_tx_qinit:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static void
sfc_tx_queue_release(void *queue)
{
	struct sfc_dp_txq *dp_txq = queue;
	struct sfc_txq *txq;
	unsigned int sw_index;
	struct sfc_adapter *sa;

	if (dp_txq == NULL)
		return;

	txq = sfc_txq_by_dp_txq(dp_txq);
	sw_index = sfc_txq_sw_index(txq);

	SFC_ASSERT(txq->evq != NULL);
	sa = txq->evq->sa;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	sfc_adapter_lock(sa);

	SFC_ASSERT(sw_index < sa->eth_dev->data->nb_tx_queues);
	sa->eth_dev->data->tx_queues[sw_index] = NULL;

	sfc_tx_qfini(sa, sw_index);

	sfc_adapter_unlock(sa);
}

static void
sfc_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	uint64_t *mac_stats;

	rte_spinlock_lock(&port->mac_stats_lock);

	if (sfc_port_update_mac_stats(sa) != 0)
		goto unlock;

	mac_stats = port->mac_stats_buf;

	if (EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask,
				   EFX_MAC_VADAPTER_RX_UNICAST_PACKETS)) {
		stats->ipackets =
			mac_stats[EFX_MAC_VADAPTER_RX_UNICAST_PACKETS] +
			mac_stats[EFX_MAC_VADAPTER_RX_MULTICAST_PACKETS] +
			mac_stats[EFX_MAC_VADAPTER_RX_BROADCAST_PACKETS];
		stats->opackets =
			mac_stats[EFX_MAC_VADAPTER_TX_UNICAST_PACKETS] +
			mac_stats[EFX_MAC_VADAPTER_TX_MULTICAST_PACKETS] +
			mac_stats[EFX_MAC_VADAPTER_TX_BROADCAST_PACKETS];
		stats->ibytes =
			mac_stats[EFX_MAC_VADAPTER_RX_UNICAST_BYTES] +
			mac_stats[EFX_MAC_VADAPTER_RX_MULTICAST_BYTES] +
			mac_stats[EFX_MAC_VADAPTER_RX_BROADCAST_BYTES];
		stats->obytes =
			mac_stats[EFX_MAC_VADAPTER_TX_UNICAST_BYTES] +
			mac_stats[EFX_MAC_VADAPTER_TX_MULTICAST_BYTES] +
			mac_stats[EFX_MAC_VADAPTER_TX_BROADCAST_BYTES];
		stats->imissed = mac_stats[EFX_MAC_VADAPTER_RX_OVERFLOW];
		stats->ierrors = mac_stats[EFX_MAC_VADAPTER_RX_BAD_PACKETS];
		stats->oerrors = mac_stats[EFX_MAC_VADAPTER_TX_BAD_PACKETS];
	} else {
		stats->ipackets = mac_stats[EFX_MAC_RX_PKTS];
		stats->opackets = mac_stats[EFX_MAC_TX_PKTS];
		stats->ibytes = mac_stats[EFX_MAC_RX_OCTETS];
		stats->obytes = mac_stats[EFX_MAC_TX_OCTETS];
		/*
		 * Take into account stats which are whenever supported
		 * on EF10. If some stat is not supported by current
		 * firmware variant or HW revision, it is guaranteed
		 * to be zero in mac_stats.
		 */
		stats->imissed =
			mac_stats[EFX_MAC_RX_NODESC_DROP_CNT] +
			mac_stats[EFX_MAC_PM_TRUNC_BB_OVERFLOW] +
			mac_stats[EFX_MAC_PM_DISCARD_BB_OVERFLOW] +
			mac_stats[EFX_MAC_PM_TRUNC_VFIFO_FULL] +
			mac_stats[EFX_MAC_PM_DISCARD_VFIFO_FULL] +
			mac_stats[EFX_MAC_PM_TRUNC_QBB] +
			mac_stats[EFX_MAC_PM_DISCARD_QBB] +
			mac_stats[EFX_MAC_PM_DISCARD_MAPPING] +
			mac_stats[EFX_MAC_RXDP_Q_DISABLED_PKTS] +
			mac_stats[EFX_MAC_RXDP_DI_DROPPED_PKTS];
		stats->ierrors =
			mac_stats[EFX_MAC_RX_FCS_ERRORS] +
			mac_stats[EFX_MAC_RX_ALIGN_ERRORS] +
			mac_stats[EFX_MAC_RX_JABBER_PKTS];
		/* no oerrors counters supported on EF10 */
	}

unlock:
	rte_spinlock_unlock(&port->mac_stats_lock);
}

static void
sfc_stats_reset(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	int rc;

	if (sa->state != SFC_ADAPTER_STARTED) {
		/*
		 * The operation cannot be done if port is not started; it
		 * will be scheduled to be done during the next port start
		 */
		port->mac_stats_reset_pending = B_TRUE;
		return;
	}

	rc = sfc_port_reset_mac_stats(sa);
	if (rc != 0)
		sfc_err(sa, "failed to reset statistics (rc = %d)", rc);
}

static int
sfc_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
	       unsigned int xstats_count)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	uint64_t *mac_stats;
	int rc;
	unsigned int i;
	int nstats = 0;

	rte_spinlock_lock(&port->mac_stats_lock);

	rc = sfc_port_update_mac_stats(sa);
	if (rc != 0) {
		SFC_ASSERT(rc > 0);
		nstats = -rc;
		goto unlock;
	}

	mac_stats = port->mac_stats_buf;

	for (i = 0; i < EFX_MAC_NSTATS; ++i) {
		if (EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask, i)) {
			if (xstats != NULL && nstats < (int)xstats_count) {
				xstats[nstats].id = nstats;
				xstats[nstats].value = mac_stats[i];
			}
			nstats++;
		}
	}

unlock:
	rte_spinlock_unlock(&port->mac_stats_lock);

	return nstats;
}

static int
sfc_xstats_get_names(struct rte_eth_dev *dev,
		     struct rte_eth_xstat_name *xstats_names,
		     unsigned int xstats_count)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	unsigned int i;
	unsigned int nstats = 0;

	for (i = 0; i < EFX_MAC_NSTATS; ++i) {
		if (EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask, i)) {
			if (xstats_names != NULL && nstats < xstats_count)
				strncpy(xstats_names[nstats].name,
					efx_mac_stat_name(sa->nic, i),
					sizeof(xstats_names[0].name));
			nstats++;
		}
	}

	return nstats;
}

static int
sfc_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
		     uint64_t *values, unsigned int n)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	uint64_t *mac_stats;
	unsigned int nb_supported = 0;
	unsigned int nb_written = 0;
	unsigned int i;
	int ret;
	int rc;

	if (unlikely(values == NULL) ||
	    unlikely((ids == NULL) && (n < port->mac_stats_nb_supported)))
		return port->mac_stats_nb_supported;

	rte_spinlock_lock(&port->mac_stats_lock);

	rc = sfc_port_update_mac_stats(sa);
	if (rc != 0) {
		SFC_ASSERT(rc > 0);
		ret = -rc;
		goto unlock;
	}

	mac_stats = port->mac_stats_buf;

	for (i = 0; (i < EFX_MAC_NSTATS) && (nb_written < n); ++i) {
		if (!EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask, i))
			continue;

		if ((ids == NULL) || (ids[nb_written] == nb_supported))
			values[nb_written++] = mac_stats[i];

		++nb_supported;
	}

	ret = nb_written;

unlock:
	rte_spinlock_unlock(&port->mac_stats_lock);

	return ret;
}

static int
sfc_xstats_get_names_by_id(struct rte_eth_dev *dev,
			   struct rte_eth_xstat_name *xstats_names,
			   const uint64_t *ids, unsigned int size)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	unsigned int nb_supported = 0;
	unsigned int nb_written = 0;
	unsigned int i;

	if (unlikely(xstats_names == NULL) ||
	    unlikely((ids == NULL) && (size < port->mac_stats_nb_supported)))
		return port->mac_stats_nb_supported;

	for (i = 0; (i < EFX_MAC_NSTATS) && (nb_written < size); ++i) {
		if (!EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask, i))
			continue;

		if ((ids == NULL) || (ids[nb_written] == nb_supported)) {
			char *name = xstats_names[nb_written++].name;

			strncpy(name, efx_mac_stat_name(sa->nic, i),
				sizeof(xstats_names[0].name));
			name[sizeof(xstats_names[0].name) - 1] = '\0';
		}

		++nb_supported;
	}

	return nb_written;
}

static int
sfc_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	unsigned int wanted_fc, link_fc;

	memset(fc_conf, 0, sizeof(*fc_conf));

	sfc_adapter_lock(sa);

	if (sa->state == SFC_ADAPTER_STARTED)
		efx_mac_fcntl_get(sa->nic, &wanted_fc, &link_fc);
	else
		link_fc = sa->port.flow_ctrl;

	switch (link_fc) {
	case 0:
		fc_conf->mode = RTE_FC_NONE;
		break;
	case EFX_FCNTL_RESPOND:
		fc_conf->mode = RTE_FC_RX_PAUSE;
		break;
	case EFX_FCNTL_GENERATE:
		fc_conf->mode = RTE_FC_TX_PAUSE;
		break;
	case (EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE):
		fc_conf->mode = RTE_FC_FULL;
		break;
	default:
		sfc_err(sa, "%s: unexpected flow control value %#x",
			__func__, link_fc);
	}

	fc_conf->autoneg = sa->port.flow_ctrl_autoneg;

	sfc_adapter_unlock(sa);

	return 0;
}

static int
sfc_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	unsigned int fcntl;
	int rc;

	if (fc_conf->high_water != 0 || fc_conf->low_water != 0 ||
	    fc_conf->pause_time != 0 || fc_conf->send_xon != 0 ||
	    fc_conf->mac_ctrl_frame_fwd != 0) {
		sfc_err(sa, "unsupported flow control settings specified");
		rc = EINVAL;
		goto fail_inval;
	}

	switch (fc_conf->mode) {
	case RTE_FC_NONE:
		fcntl = 0;
		break;
	case RTE_FC_RX_PAUSE:
		fcntl = EFX_FCNTL_RESPOND;
		break;
	case RTE_FC_TX_PAUSE:
		fcntl = EFX_FCNTL_GENERATE;
		break;
	case RTE_FC_FULL:
		fcntl = EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE;
		break;
	default:
		rc = EINVAL;
		goto fail_inval;
	}

	sfc_adapter_lock(sa);

	if (sa->state == SFC_ADAPTER_STARTED) {
		rc = efx_mac_fcntl_set(sa->nic, fcntl, fc_conf->autoneg);
		if (rc != 0)
			goto fail_mac_fcntl_set;
	}

	port->flow_ctrl = fcntl;
	port->flow_ctrl_autoneg = fc_conf->autoneg;

	sfc_adapter_unlock(sa);

	return 0;

fail_mac_fcntl_set:
	sfc_adapter_unlock(sa);
fail_inval:
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	size_t pdu = EFX_MAC_PDU(mtu);
	size_t old_pdu;
	int rc;

	sfc_log_init(sa, "mtu=%u", mtu);

	rc = EINVAL;
	if (pdu < EFX_MAC_PDU_MIN) {
		sfc_err(sa, "too small MTU %u (PDU size %u less than min %u)",
			(unsigned int)mtu, (unsigned int)pdu,
			EFX_MAC_PDU_MIN);
		goto fail_inval;
	}
	if (pdu > EFX_MAC_PDU_MAX) {
		sfc_err(sa, "too big MTU %u (PDU size %u greater than max %u)",
			(unsigned int)mtu, (unsigned int)pdu,
			EFX_MAC_PDU_MAX);
		goto fail_inval;
	}

	sfc_adapter_lock(sa);

	if (pdu != sa->port.pdu) {
		if (sa->state == SFC_ADAPTER_STARTED) {
			sfc_stop(sa);

			old_pdu = sa->port.pdu;
			sa->port.pdu = pdu;
			rc = sfc_start(sa);
			if (rc != 0)
				goto fail_start;
		} else {
			sa->port.pdu = pdu;
		}
	}

	/*
	 * The driver does not use it, but other PMDs update jumbo_frame
	 * flag and max_rx_pkt_len when MTU is set.
	 */
	dev->data->dev_conf.rxmode.jumbo_frame = (mtu > ETHER_MAX_LEN);
	dev->data->dev_conf.rxmode.max_rx_pkt_len = sa->port.pdu;

	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");
	return 0;

fail_start:
	sa->port.pdu = old_pdu;
	if (sfc_start(sa) != 0)
		sfc_err(sa, "cannot start with neither new (%u) nor old (%u) "
			"PDU max size - port is stopped",
			(unsigned int)pdu, (unsigned int)old_pdu);
	sfc_adapter_unlock(sa);

fail_inval:
	sfc_log_init(sa, "failed %d", rc);
	SFC_ASSERT(rc > 0);
	return -rc;
}
static void
sfc_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_port *port = &sa->port;
	int rc;

	sfc_adapter_lock(sa);

	if (port->isolated) {
		sfc_err(sa, "isolated mode is active on the port");
		sfc_err(sa, "will not set MAC address");
		goto unlock;
	}

	if (sa->state != SFC_ADAPTER_STARTED) {
		sfc_info(sa, "the port is not started");
		sfc_info(sa, "the new MAC address will be set on port start");

		goto unlock;
	}

	if (encp->enc_allow_set_mac_with_installed_filters) {
		rc = efx_mac_addr_set(sa->nic, mac_addr->addr_bytes);
		if (rc != 0) {
			sfc_err(sa, "cannot set MAC address (rc = %u)", rc);
			goto unlock;
		}

		/*
		 * Changing the MAC address by means of MCDI request
		 * has no effect on received traffic, therefore
		 * we also need to update unicast filters
		 */
		rc = sfc_set_rx_mode(sa);
		if (rc != 0)
			sfc_err(sa, "cannot set filter (rc = %u)", rc);
	} else {
		sfc_warn(sa, "cannot set MAC address with filters installed");
		sfc_warn(sa, "adapter will be restarted to pick the new MAC");
		sfc_warn(sa, "(some traffic may be dropped)");

		/*
		 * Since setting MAC address with filters installed is not
		 * allowed on the adapter, one needs to simply restart adapter
		 * so that the new MAC address will be taken from an outer
		 * storage and set flawlessly by means of sfc_start() call
		 */
		sfc_stop(sa);
		rc = sfc_start(sa);
		if (rc != 0)
			sfc_err(sa, "cannot restart adapter (rc = %u)", rc);
	}

unlock:
	sfc_adapter_unlock(sa);
}


static int
sfc_set_mc_addr_list(struct rte_eth_dev *dev, struct ether_addr *mc_addr_set,
		     uint32_t nb_mc_addr)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	uint8_t *mc_addrs = port->mcast_addrs;
	int rc;
	unsigned int i;

	if (port->isolated) {
		sfc_err(sa, "isolated mode is active on the port");
		sfc_err(sa, "will not set multicast address list");
		return -ENOTSUP;
	}

	if (mc_addrs == NULL)
		return -ENOBUFS;

	if (nb_mc_addr > port->max_mcast_addrs) {
		sfc_err(sa, "too many multicast addresses: %u > %u",
			 nb_mc_addr, port->max_mcast_addrs);
		return -EINVAL;
	}

	for (i = 0; i < nb_mc_addr; ++i) {
		(void)rte_memcpy(mc_addrs, mc_addr_set[i].addr_bytes,
				 EFX_MAC_ADDR_LEN);
		mc_addrs += EFX_MAC_ADDR_LEN;
	}

	port->nb_mcast_addrs = nb_mc_addr;

	if (sa->state != SFC_ADAPTER_STARTED)
		return 0;

	rc = efx_mac_multicast_list_set(sa->nic, port->mcast_addrs,
					port->nb_mcast_addrs);
	if (rc != 0)
		sfc_err(sa, "cannot set multicast address list (rc = %u)", rc);

	SFC_ASSERT(rc > 0);
	return -rc;
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static void
sfc_rx_queue_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		      struct rte_eth_rxq_info *qinfo)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;

	sfc_adapter_lock(sa);

	SFC_ASSERT(rx_queue_id < sa->rxq_count);

	rxq_info = &sa->rxq_info[rx_queue_id];
	rxq = rxq_info->rxq;
	SFC_ASSERT(rxq != NULL);

	qinfo->mp = rxq->refill_mb_pool;
	qinfo->conf.rx_free_thresh = rxq->refill_threshold;
	qinfo->conf.rx_drop_en = 1;
	qinfo->conf.rx_deferred_start = rxq_info->deferred_start;
	qinfo->scattered_rx = (rxq_info->type == EFX_RXQ_TYPE_SCATTER);
	qinfo->nb_desc = rxq_info->entries;

	sfc_adapter_unlock(sa);
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static void
sfc_tx_queue_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		      struct rte_eth_txq_info *qinfo)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_txq_info *txq_info;

	sfc_adapter_lock(sa);

	SFC_ASSERT(tx_queue_id < sa->txq_count);

	txq_info = &sa->txq_info[tx_queue_id];
	SFC_ASSERT(txq_info->txq != NULL);

	memset(qinfo, 0, sizeof(*qinfo));

	qinfo->conf.txq_flags = txq_info->txq->flags;
	qinfo->conf.tx_free_thresh = txq_info->txq->free_thresh;
	qinfo->conf.tx_deferred_start = txq_info->deferred_start;
	qinfo->nb_desc = txq_info->entries;

	sfc_adapter_unlock(sa);
}

static uint32_t
sfc_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "RxQ=%u", rx_queue_id);

	return sfc_rx_qdesc_npending(sa, rx_queue_id);
}

static int
sfc_rx_descriptor_done(void *queue, uint16_t offset)
{
	struct sfc_dp_rxq *dp_rxq = queue;

	return sfc_rx_qdesc_done(dp_rxq, offset);
}

static int
sfc_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	sfc_log_init(sa, "RxQ=%u", rx_queue_id);

	sfc_adapter_lock(sa);

	rc = EINVAL;
	if (sa->state != SFC_ADAPTER_STARTED)
		goto fail_not_started;

	rc = sfc_rx_qstart(sa, rx_queue_id);
	if (rc != 0)
		goto fail_rx_qstart;

	sa->rxq_info[rx_queue_id].deferred_started = B_TRUE;

	sfc_adapter_unlock(sa);

	return 0;

fail_rx_qstart:
fail_not_started:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "RxQ=%u", rx_queue_id);

	sfc_adapter_lock(sa);
	sfc_rx_qstop(sa, rx_queue_id);

	sa->rxq_info[rx_queue_id].deferred_started = B_FALSE;

	sfc_adapter_unlock(sa);

	return 0;
}

static int
sfc_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	sfc_log_init(sa, "TxQ = %u", tx_queue_id);

	sfc_adapter_lock(sa);

	rc = EINVAL;
	if (sa->state != SFC_ADAPTER_STARTED)
		goto fail_not_started;

	rc = sfc_tx_qstart(sa, tx_queue_id);
	if (rc != 0)
		goto fail_tx_qstart;

	sa->txq_info[tx_queue_id].deferred_started = B_TRUE;

	sfc_adapter_unlock(sa);
	return 0;

fail_tx_qstart:

fail_not_started:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "TxQ = %u", tx_queue_id);

	sfc_adapter_lock(sa);

	sfc_tx_qstop(sa, tx_queue_id);

	sa->txq_info[tx_queue_id].deferred_started = B_FALSE;

	sfc_adapter_unlock(sa);
	return 0;
}

#if EFSYS_OPT_RX_SCALE
static int
sfc_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;

	if ((sa->rss_support != EFX_RX_SCALE_EXCLUSIVE) || port->isolated)
		return -ENOTSUP;

	if (sa->rss_channels == 0)
		return -EINVAL;

	sfc_adapter_lock(sa);

	/*
	 * Mapping of hash configuration between RTE and EFX is not one-to-one,
	 * hence, conversion is done here to derive a correct set of ETH_RSS
	 * flags which corresponds to the active EFX configuration stored
	 * locally in 'sfc_adapter' and kept up-to-date
	 */
	rss_conf->rss_hf = sfc_efx_to_rte_hash_type(sa->rss_hash_types);
	rss_conf->rss_key_len = SFC_RSS_KEY_SIZE;
	if (rss_conf->rss_key != NULL)
		rte_memcpy(rss_conf->rss_key, sa->rss_key, SFC_RSS_KEY_SIZE);

	sfc_adapter_unlock(sa);

	return 0;
}

static int
sfc_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	unsigned int efx_hash_types;
	int rc = 0;

	if (port->isolated)
		return -ENOTSUP;

	if (sa->rss_support != EFX_RX_SCALE_EXCLUSIVE) {
		sfc_err(sa, "RSS is not available");
		return -ENOTSUP;
	}

	if (sa->rss_channels == 0) {
		sfc_err(sa, "RSS is not configured");
		return -EINVAL;
	}

	if ((rss_conf->rss_key != NULL) &&
	    (rss_conf->rss_key_len != sizeof(sa->rss_key))) {
		sfc_err(sa, "RSS key size is wrong (should be %lu)",
			sizeof(sa->rss_key));
		return -EINVAL;
	}

	if ((rss_conf->rss_hf & ~SFC_RSS_OFFLOADS) != 0) {
		sfc_err(sa, "unsupported hash functions requested");
		return -EINVAL;
	}

	sfc_adapter_lock(sa);

	efx_hash_types = sfc_rte_to_efx_hash_type(rss_conf->rss_hf);

	rc = efx_rx_scale_mode_set(sa->nic, EFX_RX_HASHALG_TOEPLITZ,
				   efx_hash_types, B_TRUE);
	if (rc != 0)
		goto fail_scale_mode_set;

	if (rss_conf->rss_key != NULL) {
		if (sa->state == SFC_ADAPTER_STARTED) {
			rc = efx_rx_scale_key_set(sa->nic, rss_conf->rss_key,
						  sizeof(sa->rss_key));
			if (rc != 0)
				goto fail_scale_key_set;
		}

		rte_memcpy(sa->rss_key, rss_conf->rss_key, sizeof(sa->rss_key));
	}

	sa->rss_hash_types = efx_hash_types;

	sfc_adapter_unlock(sa);

	return 0;

fail_scale_key_set:
	if (efx_rx_scale_mode_set(sa->nic, EFX_RX_HASHALG_TOEPLITZ,
				  sa->rss_hash_types, B_TRUE) != 0)
		sfc_err(sa, "failed to restore RSS mode");

fail_scale_mode_set:
	sfc_adapter_unlock(sa);
	return -rc;
}

static int
sfc_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	int entry;

	if ((sa->rss_support != EFX_RX_SCALE_EXCLUSIVE) || port->isolated)
		return -ENOTSUP;

	if (sa->rss_channels == 0)
		return -EINVAL;

	if (reta_size != EFX_RSS_TBL_SIZE)
		return -EINVAL;

	sfc_adapter_lock(sa);

	for (entry = 0; entry < reta_size; entry++) {
		int grp = entry / RTE_RETA_GROUP_SIZE;
		int grp_idx = entry % RTE_RETA_GROUP_SIZE;

		if ((reta_conf[grp].mask >> grp_idx) & 1)
			reta_conf[grp].reta[grp_idx] = sa->rss_tbl[entry];
	}

	sfc_adapter_unlock(sa);

	return 0;
}

static int
sfc_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct sfc_port *port = &sa->port;
	unsigned int *rss_tbl_new;
	uint16_t entry;
	int rc;


	if (port->isolated)
		return -ENOTSUP;

	if (sa->rss_support != EFX_RX_SCALE_EXCLUSIVE) {
		sfc_err(sa, "RSS is not available");
		return -ENOTSUP;
	}

	if (sa->rss_channels == 0) {
		sfc_err(sa, "RSS is not configured");
		return -EINVAL;
	}

	if (reta_size != EFX_RSS_TBL_SIZE) {
		sfc_err(sa, "RETA size is wrong (should be %u)",
			EFX_RSS_TBL_SIZE);
		return -EINVAL;
	}

	rss_tbl_new = rte_zmalloc("rss_tbl_new", sizeof(sa->rss_tbl), 0);
	if (rss_tbl_new == NULL)
		return -ENOMEM;

	sfc_adapter_lock(sa);

	rte_memcpy(rss_tbl_new, sa->rss_tbl, sizeof(sa->rss_tbl));

	for (entry = 0; entry < reta_size; entry++) {
		int grp_idx = entry % RTE_RETA_GROUP_SIZE;
		struct rte_eth_rss_reta_entry64 *grp;

		grp = &reta_conf[entry / RTE_RETA_GROUP_SIZE];

		if (grp->mask & (1ull << grp_idx)) {
			if (grp->reta[grp_idx] >= sa->rss_channels) {
				rc = EINVAL;
				goto bad_reta_entry;
			}
			rss_tbl_new[entry] = grp->reta[grp_idx];
		}
	}

	rc = efx_rx_scale_tbl_set(sa->nic, rss_tbl_new, EFX_RSS_TBL_SIZE);
	if (rc == 0)
		rte_memcpy(sa->rss_tbl, rss_tbl_new, sizeof(sa->rss_tbl));

bad_reta_entry:
	sfc_adapter_unlock(sa);

	rte_free(rss_tbl_new);

	SFC_ASSERT(rc >= 0);
	return -rc;
}
#endif

static int
sfc_dev_filter_ctrl(struct rte_eth_dev *dev, enum rte_filter_type filter_type,
		    enum rte_filter_op filter_op,
		    void *arg)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc = ENOTSUP;

	sfc_log_init(sa, "entry");

	switch (filter_type) {
	case RTE_ETH_FILTER_NONE:
		sfc_err(sa, "Global filters configuration not supported");
		break;
	case RTE_ETH_FILTER_MACVLAN:
		sfc_err(sa, "MACVLAN filters not supported");
		break;
	case RTE_ETH_FILTER_ETHERTYPE:
		sfc_err(sa, "EtherType filters not supported");
		break;
	case RTE_ETH_FILTER_FLEXIBLE:
		sfc_err(sa, "Flexible filters not supported");
		break;
	case RTE_ETH_FILTER_SYN:
		sfc_err(sa, "SYN filters not supported");
		break;
	case RTE_ETH_FILTER_NTUPLE:
		sfc_err(sa, "NTUPLE filters not supported");
		break;
	case RTE_ETH_FILTER_TUNNEL:
		sfc_err(sa, "Tunnel filters not supported");
		break;
	case RTE_ETH_FILTER_FDIR:
		sfc_err(sa, "Flow Director filters not supported");
		break;
	case RTE_ETH_FILTER_HASH:
		sfc_err(sa, "Hash filters not supported");
		break;
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET) {
			rc = EINVAL;
		} else {
			*(const void **)arg = &sfc_flow_ops;
			rc = 0;
		}
		break;
	default:
		sfc_err(sa, "Unknown filter type %u", filter_type);
		break;
	}

	sfc_log_init(sa, "exit: %d", -rc);
	SFC_ASSERT(rc >= 0);
	return -rc;
}

static const struct eth_dev_ops sfc_eth_dev_ops = {
	.dev_configure			= sfc_dev_configure,
	.dev_start			= sfc_dev_start,
	.dev_stop			= sfc_dev_stop,
	.dev_set_link_up		= sfc_dev_set_link_up,
	.dev_set_link_down		= sfc_dev_set_link_down,
	.dev_close			= sfc_dev_close,
	.promiscuous_enable		= sfc_dev_promisc_enable,
	.promiscuous_disable		= sfc_dev_promisc_disable,
	.allmulticast_enable		= sfc_dev_allmulti_enable,
	.allmulticast_disable		= sfc_dev_allmulti_disable,
	.link_update			= sfc_dev_link_update,
	.stats_get			= sfc_stats_get,
	.stats_reset			= sfc_stats_reset,
	.xstats_get			= sfc_xstats_get,
	.xstats_reset			= sfc_stats_reset,
	.xstats_get_names		= sfc_xstats_get_names,
	.dev_infos_get			= sfc_dev_infos_get,
	.dev_supported_ptypes_get	= sfc_dev_supported_ptypes_get,
	.mtu_set			= sfc_dev_set_mtu,
	.rx_queue_start			= sfc_rx_queue_start,
	.rx_queue_stop			= sfc_rx_queue_stop,
	.tx_queue_start			= sfc_tx_queue_start,
	.tx_queue_stop			= sfc_tx_queue_stop,
	.rx_queue_setup			= sfc_rx_queue_setup,
	.rx_queue_release		= sfc_rx_queue_release,
	.rx_queue_count			= sfc_rx_queue_count,
	.rx_descriptor_done		= sfc_rx_descriptor_done,
	.tx_queue_setup			= sfc_tx_queue_setup,
	.tx_queue_release		= sfc_tx_queue_release,
	.flow_ctrl_get			= sfc_flow_ctrl_get,
	.flow_ctrl_set			= sfc_flow_ctrl_set,
	.mac_addr_set			= sfc_mac_addr_set,
#if EFSYS_OPT_RX_SCALE
	.reta_update			= sfc_dev_rss_reta_update,
	.reta_query			= sfc_dev_rss_reta_query,
	.rss_hash_update		= sfc_dev_rss_hash_update,
	.rss_hash_conf_get		= sfc_dev_rss_hash_conf_get,
#endif
	.filter_ctrl			= sfc_dev_filter_ctrl,
	.set_mc_addr_list		= sfc_set_mc_addr_list,
	.rxq_info_get			= sfc_rx_queue_info_get,
	.txq_info_get			= sfc_tx_queue_info_get,
	.fw_version_get			= sfc_fw_version_get,
	.xstats_get_by_id		= sfc_xstats_get_by_id,
	.xstats_get_names_by_id		= sfc_xstats_get_names_by_id,
};

/**
 * Duplicate a string in potentially shared memory required for
 * multi-process support.
 *
 * strdup() allocates from process-local heap/memory.
 */
static char *
sfc_strdup(const char *str)
{
	size_t size;
	char *copy;

	if (str == NULL)
		return NULL;

	size = strlen(str) + 1;
	copy = rte_malloc(__func__, size, 0);
	if (copy != NULL)
		rte_memcpy(copy, str, size);

	return copy;
}

static int
sfc_eth_dev_set_ops(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	unsigned int avail_caps = 0;
	const char *rx_name = NULL;
	const char *tx_name = NULL;
	int rc;

	switch (sa->family) {
	case EFX_FAMILY_HUNTINGTON:
	case EFX_FAMILY_MEDFORD:
		avail_caps |= SFC_DP_HW_FW_CAP_EF10;
		break;
	default:
		break;
	}

	rc = sfc_kvargs_process(sa, SFC_KVARG_RX_DATAPATH,
				sfc_kvarg_string_handler, &rx_name);
	if (rc != 0)
		goto fail_kvarg_rx_datapath;

	if (rx_name != NULL) {
		sa->dp_rx = sfc_dp_find_rx_by_name(&sfc_dp_head, rx_name);
		if (sa->dp_rx == NULL) {
			sfc_err(sa, "Rx datapath %s not found", rx_name);
			rc = ENOENT;
			goto fail_dp_rx;
		}
		if (!sfc_dp_match_hw_fw_caps(&sa->dp_rx->dp, avail_caps)) {
			sfc_err(sa,
				"Insufficient Hw/FW capabilities to use Rx datapath %s",
				rx_name);
			rc = EINVAL;
			goto fail_dp_rx_caps;
		}
	} else {
		sa->dp_rx = sfc_dp_find_rx_by_caps(&sfc_dp_head, avail_caps);
		if (sa->dp_rx == NULL) {
			sfc_err(sa, "Rx datapath by caps %#x not found",
				avail_caps);
			rc = ENOENT;
			goto fail_dp_rx;
		}
	}

	sa->dp_rx_name = sfc_strdup(sa->dp_rx->dp.name);
	if (sa->dp_rx_name == NULL) {
		rc = ENOMEM;
		goto fail_dp_rx_name;
	}

	sfc_info(sa, "use %s Rx datapath", sa->dp_rx_name);

	dev->rx_pkt_burst = sa->dp_rx->pkt_burst;

	rc = sfc_kvargs_process(sa, SFC_KVARG_TX_DATAPATH,
				sfc_kvarg_string_handler, &tx_name);
	if (rc != 0)
		goto fail_kvarg_tx_datapath;

	if (tx_name != NULL) {
		sa->dp_tx = sfc_dp_find_tx_by_name(&sfc_dp_head, tx_name);
		if (sa->dp_tx == NULL) {
			sfc_err(sa, "Tx datapath %s not found", tx_name);
			rc = ENOENT;
			goto fail_dp_tx;
		}
		if (!sfc_dp_match_hw_fw_caps(&sa->dp_tx->dp, avail_caps)) {
			sfc_err(sa,
				"Insufficient Hw/FW capabilities to use Tx datapath %s",
				tx_name);
			rc = EINVAL;
			goto fail_dp_tx_caps;
		}
	} else {
		sa->dp_tx = sfc_dp_find_tx_by_caps(&sfc_dp_head, avail_caps);
		if (sa->dp_tx == NULL) {
			sfc_err(sa, "Tx datapath by caps %#x not found",
				avail_caps);
			rc = ENOENT;
			goto fail_dp_tx;
		}
	}

	sa->dp_tx_name = sfc_strdup(sa->dp_tx->dp.name);
	if (sa->dp_tx_name == NULL) {
		rc = ENOMEM;
		goto fail_dp_tx_name;
	}

	sfc_info(sa, "use %s Tx datapath", sa->dp_tx_name);

	dev->tx_pkt_burst = sa->dp_tx->pkt_burst;

	dev->dev_ops = &sfc_eth_dev_ops;

	return 0;

fail_dp_tx_name:
fail_dp_tx_caps:
	sa->dp_tx = NULL;

fail_dp_tx:
fail_kvarg_tx_datapath:
	rte_free(sa->dp_rx_name);
	sa->dp_rx_name = NULL;

fail_dp_rx_name:
fail_dp_rx_caps:
	sa->dp_rx = NULL;

fail_dp_rx:
fail_kvarg_rx_datapath:
	return rc;
}

static void
sfc_eth_dev_clear_ops(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	rte_free(sa->dp_tx_name);
	sa->dp_tx_name = NULL;
	sa->dp_tx = NULL;

	rte_free(sa->dp_rx_name);
	sa->dp_rx_name = NULL;
	sa->dp_rx = NULL;
}

static const struct eth_dev_ops sfc_eth_dev_secondary_ops = {
	.rxq_info_get			= sfc_rx_queue_info_get,
	.txq_info_get			= sfc_tx_queue_info_get,
};

static int
sfc_eth_dev_secondary_set_ops(struct rte_eth_dev *dev)
{
	/*
	 * Device private data has really many process-local pointers.
	 * Below code should be extremely careful to use data located
	 * in shared memory only.
	 */
	struct sfc_adapter *sa = dev->data->dev_private;
	const struct sfc_dp_rx *dp_rx;
	const struct sfc_dp_tx *dp_tx;
	int rc;

	dp_rx = sfc_dp_find_rx_by_name(&sfc_dp_head, sa->dp_rx_name);
	if (dp_rx == NULL) {
		sfc_err(sa, "cannot find %s Rx datapath", sa->dp_tx_name);
		rc = ENOENT;
		goto fail_dp_rx;
	}
	if (~dp_rx->features & SFC_DP_RX_FEAT_MULTI_PROCESS) {
		sfc_err(sa, "%s Rx datapath does not support multi-process",
			sa->dp_tx_name);
		rc = EINVAL;
		goto fail_dp_rx_multi_process;
	}

	dp_tx = sfc_dp_find_tx_by_name(&sfc_dp_head, sa->dp_tx_name);
	if (dp_tx == NULL) {
		sfc_err(sa, "cannot find %s Tx datapath", sa->dp_tx_name);
		rc = ENOENT;
		goto fail_dp_tx;
	}
	if (~dp_tx->features & SFC_DP_TX_FEAT_MULTI_PROCESS) {
		sfc_err(sa, "%s Tx datapath does not support multi-process",
			sa->dp_tx_name);
		rc = EINVAL;
		goto fail_dp_tx_multi_process;
	}

	dev->rx_pkt_burst = dp_rx->pkt_burst;
	dev->tx_pkt_burst = dp_tx->pkt_burst;
	dev->dev_ops = &sfc_eth_dev_secondary_ops;

	return 0;

fail_dp_tx_multi_process:
fail_dp_tx:
fail_dp_rx_multi_process:
fail_dp_rx:
	return rc;
}

static void
sfc_eth_dev_secondary_clear_ops(struct rte_eth_dev *dev)
{
	dev->dev_ops = NULL;
	dev->tx_pkt_burst = NULL;
	dev->rx_pkt_burst = NULL;
}

static void
sfc_register_dp(void)
{
	/* Register once */
	if (TAILQ_EMPTY(&sfc_dp_head)) {
		/* Prefer EF10 datapath */
		sfc_dp_register(&sfc_dp_head, &sfc_ef10_rx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_efx_rx.dp);

		sfc_dp_register(&sfc_dp_head, &sfc_ef10_tx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_efx_tx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_ef10_simple_tx.dp);
	}
}

static int
sfc_eth_dev_init(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	int rc;
	const efx_nic_cfg_t *encp;
	const struct ether_addr *from;

	sfc_register_dp();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -sfc_eth_dev_secondary_set_ops(dev);

	/* Required for logging */
	sa->pci_addr = pci_dev->addr;
	sa->port_id = dev->data->port_id;

	sa->eth_dev = dev;

	/* Copy PCI device info to the dev->data */
	rte_eth_copy_pci_info(dev, pci_dev);

	dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

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

	sfc_log_init(sa, "probing");
	rc = sfc_probe(sa);
	if (rc != 0)
		goto fail_probe;

	sfc_log_init(sa, "set device ops");
	rc = sfc_eth_dev_set_ops(dev);
	if (rc != 0)
		goto fail_set_ops;

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

	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");
	return 0;

fail_attach:
	sfc_eth_dev_clear_ops(dev);

fail_set_ops:
	sfc_unprobe(sa);

fail_probe:
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
	struct sfc_adapter *sa;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		sfc_eth_dev_secondary_clear_ops(dev);
		return 0;
	}

	sa = dev->data->dev_private;
	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);

	sfc_eth_dev_clear_ops(dev);

	sfc_detach(sa);
	sfc_unprobe(sa);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

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
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_FARMINGDALE_VF) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_GREENPORT) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_GREENPORT_VF) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD_VF) },
	{ .vendor_id = 0 /* sentinel */ }
};

static int sfc_eth_dev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct sfc_adapter), sfc_eth_dev_init);
}

static int sfc_eth_dev_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, sfc_eth_dev_uninit);
}

static struct rte_pci_driver sfc_efx_pmd = {
	.id_table = pci_id_sfc_efx_map,
	.drv_flags =
		RTE_PCI_DRV_INTR_LSC |
		RTE_PCI_DRV_NEED_MAPPING,
	.probe = sfc_eth_dev_pci_probe,
	.remove = sfc_eth_dev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_sfc_efx, sfc_efx_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_sfc_efx, pci_id_sfc_efx_map);
RTE_PMD_REGISTER_KMOD_DEP(net_sfc_efx, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_sfc_efx,
	SFC_KVARG_RX_DATAPATH "=" SFC_KVARG_VALUES_RX_DATAPATH " "
	SFC_KVARG_TX_DATAPATH "=" SFC_KVARG_VALUES_TX_DATAPATH " "
	SFC_KVARG_PERF_PROFILE "=" SFC_KVARG_VALUES_PERF_PROFILE " "
	SFC_KVARG_STATS_UPDATE_PERIOD_MS "=<long> "
	SFC_KVARG_MCDI_LOGGING "=" SFC_KVARG_VALUES_BOOL " "
	SFC_KVARG_DEBUG_INIT "=" SFC_KVARG_VALUES_BOOL);
