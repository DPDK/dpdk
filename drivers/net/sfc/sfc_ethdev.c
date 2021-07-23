/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_dev.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_ether.h>

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

uint32_t sfc_logtype_driver;

static struct sfc_dp_list sfc_dp_head =
	TAILQ_HEAD_INITIALIZER(sfc_dp_head);


static void sfc_eth_dev_clear_ops(struct rte_eth_dev *dev);


static int
sfc_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	efx_nic_fw_info_t enfi;
	int ret;
	int rc;

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

static int
sfc_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	const struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(dev);
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_rss *rss = &sas->rss;
	struct sfc_mae *mae = &sa->mae;
	uint64_t txq_offloads_def = 0;

	sfc_log_init(sa, "entry");

	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mtu = EFX_MAC_SDU_MAX;

	dev_info->max_rx_pktlen = EFX_MAC_PDU_MAX;

	dev_info->max_vfs = sa->sriov.num_vfs;

	/* Autonegotiation may be disabled */
	dev_info->speed_capa = ETH_LINK_SPEED_FIXED;
	if (sa->port.phy_adv_cap_mask & (1u << EFX_PHY_CAP_1000FDX))
		dev_info->speed_capa |= ETH_LINK_SPEED_1G;
	if (sa->port.phy_adv_cap_mask & (1u << EFX_PHY_CAP_10000FDX))
		dev_info->speed_capa |= ETH_LINK_SPEED_10G;
	if (sa->port.phy_adv_cap_mask & (1u << EFX_PHY_CAP_25000FDX))
		dev_info->speed_capa |= ETH_LINK_SPEED_25G;
	if (sa->port.phy_adv_cap_mask & (1u << EFX_PHY_CAP_40000FDX))
		dev_info->speed_capa |= ETH_LINK_SPEED_40G;
	if (sa->port.phy_adv_cap_mask & (1u << EFX_PHY_CAP_50000FDX))
		dev_info->speed_capa |= ETH_LINK_SPEED_50G;
	if (sa->port.phy_adv_cap_mask & (1u << EFX_PHY_CAP_100000FDX))
		dev_info->speed_capa |= ETH_LINK_SPEED_100G;

	dev_info->max_rx_queues = sa->rxq_max;
	dev_info->max_tx_queues = sa->txq_max;

	/* By default packets are dropped if no descriptors are available */
	dev_info->default_rxconf.rx_drop_en = 1;

	dev_info->rx_queue_offload_capa = sfc_rx_get_queue_offload_caps(sa);

	/*
	 * rx_offload_capa includes both device and queue offloads since
	 * the latter may be requested on a per device basis which makes
	 * sense when some offloads are needed to be set on all queues.
	 */
	dev_info->rx_offload_capa = sfc_rx_get_dev_offload_caps(sa) |
				    dev_info->rx_queue_offload_capa;

	dev_info->tx_queue_offload_capa = sfc_tx_get_queue_offload_caps(sa);

	/*
	 * tx_offload_capa includes both device and queue offloads since
	 * the latter may be requested on a per device basis which makes
	 * sense when some offloads are needed to be set on all queues.
	 */
	dev_info->tx_offload_capa = sfc_tx_get_dev_offload_caps(sa) |
				    dev_info->tx_queue_offload_capa;

	if (dev_info->tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		txq_offloads_def |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	dev_info->default_txconf.offloads |= txq_offloads_def;

	if (rss->context_type != EFX_RX_SCALE_UNAVAILABLE) {
		uint64_t rte_hf = 0;
		unsigned int i;

		for (i = 0; i < rss->hf_map_nb_entries; ++i)
			rte_hf |= rss->hf_map[i].rte;

		dev_info->reta_size = EFX_RSS_TBL_SIZE;
		dev_info->hash_key_size = EFX_RSS_KEY_SIZE;
		dev_info->flow_type_rss_offloads = rte_hf;
	}

	/* Initialize to hardware limits */
	dev_info->rx_desc_lim.nb_max = sa->rxq_max_entries;
	dev_info->rx_desc_lim.nb_min = sa->rxq_min_entries;
	/* The RXQ hardware requires that the descriptor count is a power
	 * of 2, but rx_desc_lim cannot properly describe that constraint.
	 */
	dev_info->rx_desc_lim.nb_align = sa->rxq_min_entries;

	/* Initialize to hardware limits */
	dev_info->tx_desc_lim.nb_max = sa->txq_max_entries;
	dev_info->tx_desc_lim.nb_min = sa->txq_min_entries;
	/*
	 * The TXQ hardware requires that the descriptor count is a power
	 * of 2, but tx_desc_lim cannot properly describe that constraint
	 */
	dev_info->tx_desc_lim.nb_align = sa->txq_min_entries;

	if (sap->dp_rx->get_dev_info != NULL)
		sap->dp_rx->get_dev_info(dev_info);
	if (sap->dp_tx->get_dev_info != NULL)
		sap->dp_tx->get_dev_info(dev_info);

	dev_info->dev_capa = RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
			     RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;

	if (mae->status == SFC_MAE_STATUS_SUPPORTED) {
		dev_info->switch_info.name = dev->device->driver->name;
		dev_info->switch_info.domain_id = mae->switch_domain_id;
		dev_info->switch_info.port_id = mae->switch_port_id;
	}

	return 0;
}

static const uint32_t *
sfc_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	const struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(dev);

	return sap->dp_rx->supported_ptypes_get(sap->shared->tunnel_encaps);
}

static int
sfc_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *dev_data = dev->data;
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
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
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
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
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct rte_eth_link current_link;
	int ret;

	sfc_log_init(sa, "entry");

	if (sa->state != SFC_ADAPTER_STARTED) {
		sfc_port_link_mode_to_info(EFX_LINK_UNKNOWN, &current_link);
	} else if (wait_to_complete) {
		efx_link_mode_t link_mode;

		if (efx_port_poll(sa->nic, &link_mode) != 0)
			link_mode = EFX_LINK_UNKNOWN;
		sfc_port_link_mode_to_info(link_mode, &current_link);

	} else {
		sfc_ev_mgmt_qpoll(sa);
		rte_eth_linkstatus_get(dev, &current_link);
	}

	ret = rte_eth_linkstatus_set(dev, &current_link);
	if (ret == 0)
		sfc_notice(sa, "Link status is %s",
			   current_link.link_status ? "UP" : "DOWN");

	return ret;
}

static int
sfc_dev_stop(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	sfc_stop(sa);
	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");

	return 0;
}

static int
sfc_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
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
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);
	sfc_stop(sa);
	sfc_adapter_unlock(sa);

	return 0;
}

static void
sfc_eth_dev_secondary_clear_ops(struct rte_eth_dev *dev)
{
	free(dev->process_private);
	rte_eth_dev_release_port(dev);
}

static int
sfc_dev_close(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);

	sfc_log_init(sa, "entry");

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		sfc_eth_dev_secondary_clear_ops(dev);
		return 0;
	}

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

	/*
	 * Cleanup all resources.
	 * Rollback primary process sfc_eth_dev_init() below.
	 */

	sfc_eth_dev_clear_ops(dev);

	sfc_detach(sa);
	sfc_unprobe(sa);

	sfc_kvargs_cleanup(sa);

	sfc_adapter_unlock(sa);
	sfc_adapter_lock_fini(sa);

	sfc_log_init(sa, "done");

	/* Required for logging, so cleanup last */
	sa->eth_dev = NULL;

	free(sa);

	return 0;
}

static int
sfc_dev_filter_set(struct rte_eth_dev *dev, enum sfc_dev_filter_mode mode,
		   boolean_t enabled)
{
	struct sfc_port *port;
	boolean_t *toggle;
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	boolean_t allmulti = (mode == SFC_DEV_FILTER_MODE_ALLMULTI);
	const char *desc = (allmulti) ? "all-multi" : "promiscuous";
	int rc = 0;

	sfc_adapter_lock(sa);

	port = &sa->port;
	toggle = (allmulti) ? (&port->allmulti) : (&port->promisc);

	if (*toggle != enabled) {
		*toggle = enabled;

		if (sfc_sa2shared(sa)->isolated) {
			sfc_warn(sa, "isolated mode is active on the port");
			sfc_warn(sa, "the change is to be applied on the next "
				     "start provided that isolated mode is "
				     "disabled prior the next start");
		} else if ((sa->state == SFC_ADAPTER_STARTED) &&
			   ((rc = sfc_set_rx_mode(sa)) != 0)) {
			*toggle = !(enabled);
			sfc_warn(sa, "Failed to %s %s mode, rc = %d",
				 ((enabled) ? "enable" : "disable"), desc, rc);

			/*
			 * For promiscuous and all-multicast filters a
			 * permission failure should be reported as an
			 * unsupported filter.
			 */
			if (rc == EPERM)
				rc = ENOTSUP;
		}
	}

	sfc_adapter_unlock(sa);
	return rc;
}

static int
sfc_dev_promisc_enable(struct rte_eth_dev *dev)
{
	int rc = sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_PROMISC, B_TRUE);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_promisc_disable(struct rte_eth_dev *dev)
{
	int rc = sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_PROMISC, B_FALSE);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_allmulti_enable(struct rte_eth_dev *dev)
{
	int rc = sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_ALLMULTI, B_TRUE);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_allmulti_disable(struct rte_eth_dev *dev)
{
	int rc = sfc_dev_filter_set(dev, SFC_DEV_FILTER_MODE_ALLMULTI, B_FALSE);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		   uint16_t nb_rx_desc, unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf,
		   struct rte_mempool *mb_pool)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int rc;

	sfc_log_init(sa, "RxQ=%u nb_rx_desc=%u socket_id=%u",
		     rx_queue_id, nb_rx_desc, socket_id);

	sfc_adapter_lock(sa);

	rc = sfc_rx_qinit(sa, rx_queue_id, nb_rx_desc, socket_id,
			  rx_conf, mb_pool);
	if (rc != 0)
		goto fail_rx_qinit;

	dev->data->rx_queues[rx_queue_id] = sas->rxq_info[rx_queue_id].dp;

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

	sw_index = dp_rxq->dpq.queue_id;

	sfc_log_init(sa, "RxQ=%u", sw_index);

	sfc_rx_qfini(sa, sw_index);

	sfc_adapter_unlock(sa);
}

static int
sfc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		   uint16_t nb_tx_desc, unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int rc;

	sfc_log_init(sa, "TxQ = %u, nb_tx_desc = %u, socket_id = %u",
		     tx_queue_id, nb_tx_desc, socket_id);

	sfc_adapter_lock(sa);

	rc = sfc_tx_qinit(sa, tx_queue_id, nb_tx_desc, socket_id, tx_conf);
	if (rc != 0)
		goto fail_tx_qinit;

	dev->data->tx_queues[tx_queue_id] = sas->txq_info[tx_queue_id].dp;

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
	sw_index = dp_txq->dpq.queue_id;

	SFC_ASSERT(txq->evq != NULL);
	sa = txq->evq->sa;

	sfc_log_init(sa, "TxQ = %u", sw_index);

	sfc_adapter_lock(sa);

	sfc_tx_qfini(sa, sw_index);

	sfc_adapter_unlock(sa);
}

/*
 * Some statistics are computed as A - B where A and B each increase
 * monotonically with some hardware counter(s) and the counters are read
 * asynchronously.
 *
 * If packet X is counted in A, but not counted in B yet, computed value is
 * greater than real.
 *
 * If packet X is not counted in A at the moment of reading the counter,
 * but counted in B at the moment of reading the counter, computed value
 * is less than real.
 *
 * However, counter which grows backward is worse evil than slightly wrong
 * value. So, let's try to guarantee that it never happens except may be
 * the case when the MAC stats are zeroed as a result of a NIC reset.
 */
static void
sfc_update_diff_stat(uint64_t *stat, uint64_t newval)
{
	if ((int64_t)(newval - *stat) > 0 || newval == 0)
		*stat = newval;
}

static int
sfc_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	uint64_t *mac_stats;
	int ret;

	sfc_adapter_lock(sa);

	ret = sfc_port_update_mac_stats(sa, B_FALSE);
	if (ret != 0)
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
		stats->imissed = mac_stats[EFX_MAC_VADAPTER_RX_BAD_PACKETS];
		stats->oerrors = mac_stats[EFX_MAC_VADAPTER_TX_BAD_PACKETS];

		/* CRC is included in these stats, but shouldn't be */
		stats->ibytes -= stats->ipackets * RTE_ETHER_CRC_LEN;
		stats->obytes -= stats->opackets * RTE_ETHER_CRC_LEN;
	} else {
		stats->opackets = mac_stats[EFX_MAC_TX_PKTS];
		stats->ibytes = mac_stats[EFX_MAC_RX_OCTETS];
		stats->obytes = mac_stats[EFX_MAC_TX_OCTETS];

		/* CRC is included in these stats, but shouldn't be */
		stats->ibytes -= mac_stats[EFX_MAC_RX_PKTS] * RTE_ETHER_CRC_LEN;
		stats->obytes -= mac_stats[EFX_MAC_TX_PKTS] * RTE_ETHER_CRC_LEN;

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

		/* Exclude missed, errors and pauses from Rx packets */
		sfc_update_diff_stat(&port->ipackets,
			mac_stats[EFX_MAC_RX_PKTS] -
			mac_stats[EFX_MAC_RX_PAUSE_PKTS] -
			stats->imissed - stats->ierrors);
		stats->ipackets = port->ipackets;
	}

unlock:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(ret >= 0);
	return -ret;
}

static int
sfc_stats_reset(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	int rc;

	sfc_adapter_lock(sa);

	if (sa->state != SFC_ADAPTER_STARTED) {
		/*
		 * The operation cannot be done if port is not started; it
		 * will be scheduled to be done during the next port start
		 */
		port->mac_stats_reset_pending = B_TRUE;
		sfc_adapter_unlock(sa);
		return 0;
	}

	rc = sfc_port_reset_mac_stats(sa);
	if (rc != 0)
		sfc_err(sa, "failed to reset statistics (rc = %d)", rc);

	sfc_adapter_unlock(sa);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
	       unsigned int xstats_count)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	uint64_t *mac_stats;
	int rc;
	unsigned int i;
	int nstats = 0;

	sfc_adapter_lock(sa);

	rc = sfc_port_update_mac_stats(sa, B_FALSE);
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
	sfc_adapter_unlock(sa);

	return nstats;
}

static int
sfc_xstats_get_names(struct rte_eth_dev *dev,
		     struct rte_eth_xstat_name *xstats_names,
		     unsigned int xstats_count)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	unsigned int i;
	unsigned int nstats = 0;

	for (i = 0; i < EFX_MAC_NSTATS; ++i) {
		if (EFX_MAC_STAT_SUPPORTED(port->mac_stats_mask, i)) {
			if (xstats_names != NULL && nstats < xstats_count)
				strlcpy(xstats_names[nstats].name,
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
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	uint64_t *mac_stats;
	unsigned int i;
	int ret;
	int rc;

	if (unlikely(ids == NULL || values == NULL))
		return -EINVAL;

	sfc_adapter_lock(sa);

	rc = sfc_port_update_mac_stats(sa, B_FALSE);
	if (rc != 0) {
		SFC_ASSERT(rc > 0);
		ret = -rc;
		goto unlock;
	}

	mac_stats = port->mac_stats_buf;

	SFC_ASSERT(port->mac_stats_nb_supported <=
		   RTE_DIM(port->mac_stats_by_id));

	for (i = 0; i < n; i++) {
		if (ids[i] < port->mac_stats_nb_supported) {
			values[i] = mac_stats[port->mac_stats_by_id[ids[i]]];
		} else {
			ret = i;
			goto unlock;
		}
	}

	ret = n;

unlock:
	sfc_adapter_unlock(sa);

	return ret;
}

static int
sfc_xstats_get_names_by_id(struct rte_eth_dev *dev,
			   struct rte_eth_xstat_name *xstats_names,
			   const uint64_t *ids, unsigned int size)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	unsigned int nb_supported;
	unsigned int i;

	if (unlikely(xstats_names == NULL && ids != NULL) ||
	    unlikely(xstats_names != NULL && ids == NULL))
		return -EINVAL;

	sfc_adapter_lock(sa);

	if (unlikely(xstats_names == NULL && ids == NULL)) {
		nb_supported = port->mac_stats_nb_supported;
		sfc_adapter_unlock(sa);
		return nb_supported;
	}

	SFC_ASSERT(port->mac_stats_nb_supported <=
		   RTE_DIM(port->mac_stats_by_id));

	for (i = 0; i < size; i++) {
		if (ids[i] < port->mac_stats_nb_supported) {
			strlcpy(xstats_names[i].name,
				efx_mac_stat_name(sa->nic,
						 port->mac_stats_by_id[ids[i]]),
				sizeof(xstats_names[0].name));
		} else {
			sfc_adapter_unlock(sa);
			return i;
		}
	}

	sfc_adapter_unlock(sa);

	return size;
}

static int
sfc_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
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
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
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
sfc_check_scatter_on_all_rx_queues(struct sfc_adapter *sa, size_t pdu)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	boolean_t scatter_enabled;
	const char *error;
	unsigned int i;

	for (i = 0; i < sas->rxq_count; i++) {
		if ((sas->rxq_info[i].state & SFC_RXQ_INITIALIZED) == 0)
			continue;

		scatter_enabled = (sas->rxq_info[i].type_flags &
				   EFX_RXQ_FLAG_SCATTER);

		if (!sfc_rx_check_scatter(pdu, sa->rxq_ctrl[i].buf_size,
					  encp->enc_rx_prefix_size,
					  scatter_enabled,
					  encp->enc_rx_scatter_max, &error)) {
			sfc_err(sa, "MTU check for RxQ %u failed: %s", i,
				error);
			return EINVAL;
		}
	}

	return 0;
}

static int
sfc_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
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
			(unsigned int)EFX_MAC_PDU_MAX);
		goto fail_inval;
	}

	sfc_adapter_lock(sa);

	rc = sfc_check_scatter_on_all_rx_queues(sa, pdu);
	if (rc != 0)
		goto fail_check_scatter;

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
	 * The driver does not use it, but other PMDs update jumbo frame
	 * flag and max_rx_pkt_len when MTU is set.
	 */
	if (mtu > RTE_ETHER_MTU) {
		struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
		rxmode->offloads |= DEV_RX_OFFLOAD_JUMBO_FRAME;
	}

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

fail_check_scatter:
	sfc_adapter_unlock(sa);

fail_inval:
	sfc_log_init(sa, "failed %d", rc);
	SFC_ASSERT(rc > 0);
	return -rc;
}
static int
sfc_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_port *port = &sa->port;
	struct rte_ether_addr *old_addr = &dev->data->mac_addrs[0];
	int rc = 0;

	sfc_adapter_lock(sa);

	if (rte_is_same_ether_addr(mac_addr, &port->default_mac_addr))
		goto unlock;

	/*
	 * Copy the address to the device private data so that
	 * it could be recalled in the case of adapter restart.
	 */
	rte_ether_addr_copy(mac_addr, &port->default_mac_addr);

	/*
	 * Neither of the two following checks can return
	 * an error. The new MAC address is preserved in
	 * the device private data and can be activated
	 * on the next port start if the user prevents
	 * isolated mode from being enabled.
	 */
	if (sfc_sa2shared(sa)->isolated) {
		sfc_warn(sa, "isolated mode is active on the port");
		sfc_warn(sa, "will not set MAC address");
		goto unlock;
	}

	if (sa->state != SFC_ADAPTER_STARTED) {
		sfc_notice(sa, "the port is not started");
		sfc_notice(sa, "the new MAC address will be set on port start");

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
		rc = sfc_set_rx_mode_unchecked(sa);
		if (rc != 0) {
			sfc_err(sa, "cannot set filter (rc = %u)", rc);
			/* Rollback the old address */
			(void)efx_mac_addr_set(sa->nic, old_addr->addr_bytes);
			(void)sfc_set_rx_mode_unchecked(sa);
		}
	} else {
		sfc_warn(sa, "cannot set MAC address with filters installed");
		sfc_warn(sa, "adapter will be restarted to pick the new MAC");
		sfc_warn(sa, "(some traffic may be dropped)");

		/*
		 * Since setting MAC address with filters installed is not
		 * allowed on the adapter, the new MAC address will be set
		 * by means of adapter restart. sfc_start() shall retrieve
		 * the new address from the device private data and set it.
		 */
		sfc_stop(sa);
		rc = sfc_start(sa);
		if (rc != 0)
			sfc_err(sa, "cannot restart adapter (rc = %u)", rc);
	}

unlock:
	if (rc != 0)
		rte_ether_addr_copy(old_addr, &port->default_mac_addr);

	sfc_adapter_unlock(sa);

	SFC_ASSERT(rc >= 0);
	return -rc;
}


static int
sfc_set_mc_addr_list(struct rte_eth_dev *dev,
		struct rte_ether_addr *mc_addr_set, uint32_t nb_mc_addr)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_port *port = &sa->port;
	uint8_t *mc_addrs = port->mcast_addrs;
	int rc;
	unsigned int i;

	if (sfc_sa2shared(sa)->isolated) {
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
		rte_memcpy(mc_addrs, mc_addr_set[i].addr_bytes,
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

	SFC_ASSERT(rc >= 0);
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
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_rxq_info *rxq_info;

	SFC_ASSERT(rx_queue_id < sas->rxq_count);

	rxq_info = &sas->rxq_info[rx_queue_id];

	qinfo->mp = rxq_info->refill_mb_pool;
	qinfo->conf.rx_free_thresh = rxq_info->refill_threshold;
	qinfo->conf.rx_drop_en = 1;
	qinfo->conf.rx_deferred_start = rxq_info->deferred_start;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
	if (rxq_info->type_flags & EFX_RXQ_FLAG_SCATTER) {
		qinfo->conf.offloads |= DEV_RX_OFFLOAD_SCATTER;
		qinfo->scattered_rx = 1;
	}
	qinfo->nb_desc = rxq_info->entries;
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static void
sfc_tx_queue_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		      struct rte_eth_txq_info *qinfo)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_txq_info *txq_info;

	SFC_ASSERT(tx_queue_id < sas->txq_count);

	txq_info = &sas->txq_info[tx_queue_id];

	memset(qinfo, 0, sizeof(*qinfo));

	qinfo->conf.offloads = txq_info->offloads;
	qinfo->conf.tx_free_thresh = txq_info->free_thresh;
	qinfo->conf.tx_deferred_start = txq_info->deferred_start;
	qinfo->nb_desc = txq_info->entries;
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static uint32_t
sfc_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	const struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(dev);
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_rxq_info *rxq_info;

	SFC_ASSERT(rx_queue_id < sas->rxq_count);
	rxq_info = &sas->rxq_info[rx_queue_id];

	if ((rxq_info->state & SFC_RXQ_STARTED) == 0)
		return 0;

	return sap->dp_rx->qdesc_npending(rxq_info->dp);
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static int
sfc_rx_descriptor_done(void *queue, uint16_t offset)
{
	struct sfc_dp_rxq *dp_rxq = queue;
	const struct sfc_dp_rx *dp_rx;

	dp_rx = sfc_dp_rx_by_dp_rxq(dp_rxq);

	return offset < dp_rx->qdesc_npending(dp_rxq);
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static int
sfc_rx_descriptor_status(void *queue, uint16_t offset)
{
	struct sfc_dp_rxq *dp_rxq = queue;
	const struct sfc_dp_rx *dp_rx;

	dp_rx = sfc_dp_rx_by_dp_rxq(dp_rxq);

	return dp_rx->qdesc_status(dp_rxq, offset);
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static int
sfc_tx_descriptor_status(void *queue, uint16_t offset)
{
	struct sfc_dp_txq *dp_txq = queue;
	const struct sfc_dp_tx *dp_tx;

	dp_tx = sfc_dp_tx_by_dp_txq(dp_txq);

	return dp_tx->qdesc_status(dp_txq, offset);
}

static int
sfc_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int rc;

	sfc_log_init(sa, "RxQ=%u", rx_queue_id);

	sfc_adapter_lock(sa);

	rc = EINVAL;
	if (sa->state != SFC_ADAPTER_STARTED)
		goto fail_not_started;

	if (sas->rxq_info[rx_queue_id].state != SFC_RXQ_INITIALIZED)
		goto fail_not_setup;

	rc = sfc_rx_qstart(sa, rx_queue_id);
	if (rc != 0)
		goto fail_rx_qstart;

	sas->rxq_info[rx_queue_id].deferred_started = B_TRUE;

	sfc_adapter_unlock(sa);

	return 0;

fail_rx_qstart:
fail_not_setup:
fail_not_started:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);

	sfc_log_init(sa, "RxQ=%u", rx_queue_id);

	sfc_adapter_lock(sa);
	sfc_rx_qstop(sa, rx_queue_id);

	sas->rxq_info[rx_queue_id].deferred_started = B_FALSE;

	sfc_adapter_unlock(sa);

	return 0;
}

static int
sfc_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int rc;

	sfc_log_init(sa, "TxQ = %u", tx_queue_id);

	sfc_adapter_lock(sa);

	rc = EINVAL;
	if (sa->state != SFC_ADAPTER_STARTED)
		goto fail_not_started;

	if (sas->txq_info[tx_queue_id].state != SFC_TXQ_INITIALIZED)
		goto fail_not_setup;

	rc = sfc_tx_qstart(sa, tx_queue_id);
	if (rc != 0)
		goto fail_tx_qstart;

	sas->txq_info[tx_queue_id].deferred_started = B_TRUE;

	sfc_adapter_unlock(sa);
	return 0;

fail_tx_qstart:

fail_not_setup:
fail_not_started:
	sfc_adapter_unlock(sa);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);

	sfc_log_init(sa, "TxQ = %u", tx_queue_id);

	sfc_adapter_lock(sa);

	sfc_tx_qstop(sa, tx_queue_id);

	sas->txq_info[tx_queue_id].deferred_started = B_FALSE;

	sfc_adapter_unlock(sa);
	return 0;
}

static efx_tunnel_protocol_t
sfc_tunnel_rte_type_to_efx_udp_proto(enum rte_eth_tunnel_type rte_type)
{
	switch (rte_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		return EFX_TUNNEL_PROTOCOL_VXLAN;
	case RTE_TUNNEL_TYPE_GENEVE:
		return EFX_TUNNEL_PROTOCOL_GENEVE;
	default:
		return EFX_TUNNEL_NPROTOS;
	}
}

enum sfc_udp_tunnel_op_e {
	SFC_UDP_TUNNEL_ADD_PORT,
	SFC_UDP_TUNNEL_DEL_PORT,
};

static int
sfc_dev_udp_tunnel_op(struct rte_eth_dev *dev,
		      struct rte_eth_udp_tunnel *tunnel_udp,
		      enum sfc_udp_tunnel_op_e op)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	efx_tunnel_protocol_t tunnel_proto;
	int rc;

	sfc_log_init(sa, "%s udp_port=%u prot_type=%u",
		     (op == SFC_UDP_TUNNEL_ADD_PORT) ? "add" :
		     (op == SFC_UDP_TUNNEL_DEL_PORT) ? "delete" : "unknown",
		     tunnel_udp->udp_port, tunnel_udp->prot_type);

	tunnel_proto =
		sfc_tunnel_rte_type_to_efx_udp_proto(tunnel_udp->prot_type);
	if (tunnel_proto >= EFX_TUNNEL_NPROTOS) {
		rc = ENOTSUP;
		goto fail_bad_proto;
	}

	sfc_adapter_lock(sa);

	switch (op) {
	case SFC_UDP_TUNNEL_ADD_PORT:
		rc = efx_tunnel_config_udp_add(sa->nic,
					       tunnel_udp->udp_port,
					       tunnel_proto);
		break;
	case SFC_UDP_TUNNEL_DEL_PORT:
		rc = efx_tunnel_config_udp_remove(sa->nic,
						  tunnel_udp->udp_port,
						  tunnel_proto);
		break;
	default:
		rc = EINVAL;
		goto fail_bad_op;
	}

	if (rc != 0)
		goto fail_op;

	if (sa->state == SFC_ADAPTER_STARTED) {
		rc = efx_tunnel_reconfigure(sa->nic);
		if (rc == EAGAIN) {
			/*
			 * Configuration is accepted by FW and MC reboot
			 * is initiated to apply the changes. MC reboot
			 * will be handled in a usual way (MC reboot
			 * event on management event queue and adapter
			 * restart).
			 */
			rc = 0;
		} else if (rc != 0) {
			goto fail_reconfigure;
		}
	}

	sfc_adapter_unlock(sa);
	return 0;

fail_reconfigure:
	/* Remove/restore entry since the change makes the trouble */
	switch (op) {
	case SFC_UDP_TUNNEL_ADD_PORT:
		(void)efx_tunnel_config_udp_remove(sa->nic,
						   tunnel_udp->udp_port,
						   tunnel_proto);
		break;
	case SFC_UDP_TUNNEL_DEL_PORT:
		(void)efx_tunnel_config_udp_add(sa->nic,
						tunnel_udp->udp_port,
						tunnel_proto);
		break;
	}

fail_op:
fail_bad_op:
	sfc_adapter_unlock(sa);

fail_bad_proto:
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
			    struct rte_eth_udp_tunnel *tunnel_udp)
{
	return sfc_dev_udp_tunnel_op(dev, tunnel_udp, SFC_UDP_TUNNEL_ADD_PORT);
}

static int
sfc_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
			    struct rte_eth_udp_tunnel *tunnel_udp)
{
	return sfc_dev_udp_tunnel_op(dev, tunnel_udp, SFC_UDP_TUNNEL_DEL_PORT);
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static int
sfc_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_rss *rss = &sas->rss;

	if (rss->context_type != EFX_RX_SCALE_EXCLUSIVE)
		return -ENOTSUP;

	/*
	 * Mapping of hash configuration between RTE and EFX is not one-to-one,
	 * hence, conversion is done here to derive a correct set of ETH_RSS
	 * flags which corresponds to the active EFX configuration stored
	 * locally in 'sfc_adapter' and kept up-to-date
	 */
	rss_conf->rss_hf = sfc_rx_hf_efx_to_rte(rss, rss->hash_types);
	rss_conf->rss_key_len = EFX_RSS_KEY_SIZE;
	if (rss_conf->rss_key != NULL)
		rte_memcpy(rss_conf->rss_key, rss->key, EFX_RSS_KEY_SIZE);

	return 0;
}

static int
sfc_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	unsigned int efx_hash_types;
	uint32_t contexts[] = {EFX_RSS_CONTEXT_DEFAULT, rss->dummy_rss_context};
	unsigned int n_contexts;
	unsigned int mode_i = 0;
	unsigned int key_i = 0;
	unsigned int i = 0;
	int rc = 0;

	n_contexts = rss->dummy_rss_context == EFX_RSS_CONTEXT_DEFAULT ? 1 : 2;

	if (sfc_sa2shared(sa)->isolated)
		return -ENOTSUP;

	if (rss->context_type != EFX_RX_SCALE_EXCLUSIVE) {
		sfc_err(sa, "RSS is not available");
		return -ENOTSUP;
	}

	if (rss->channels == 0) {
		sfc_err(sa, "RSS is not configured");
		return -EINVAL;
	}

	if ((rss_conf->rss_key != NULL) &&
	    (rss_conf->rss_key_len != sizeof(rss->key))) {
		sfc_err(sa, "RSS key size is wrong (should be %zu)",
			sizeof(rss->key));
		return -EINVAL;
	}

	sfc_adapter_lock(sa);

	rc = sfc_rx_hf_rte_to_efx(sa, rss_conf->rss_hf, &efx_hash_types);
	if (rc != 0)
		goto fail_rx_hf_rte_to_efx;

	for (mode_i = 0; mode_i < n_contexts; mode_i++) {
		rc = efx_rx_scale_mode_set(sa->nic, contexts[mode_i],
					   rss->hash_alg, efx_hash_types,
					   B_TRUE);
		if (rc != 0)
			goto fail_scale_mode_set;
	}

	if (rss_conf->rss_key != NULL) {
		if (sa->state == SFC_ADAPTER_STARTED) {
			for (key_i = 0; key_i < n_contexts; key_i++) {
				rc = efx_rx_scale_key_set(sa->nic,
							  contexts[key_i],
							  rss_conf->rss_key,
							  sizeof(rss->key));
				if (rc != 0)
					goto fail_scale_key_set;
			}
		}

		rte_memcpy(rss->key, rss_conf->rss_key, sizeof(rss->key));
	}

	rss->hash_types = efx_hash_types;

	sfc_adapter_unlock(sa);

	return 0;

fail_scale_key_set:
	for (i = 0; i < key_i; i++) {
		if (efx_rx_scale_key_set(sa->nic, contexts[i], rss->key,
					 sizeof(rss->key)) != 0)
			sfc_err(sa, "failed to restore RSS key");
	}

fail_scale_mode_set:
	for (i = 0; i < mode_i; i++) {
		if (efx_rx_scale_mode_set(sa->nic, contexts[i],
					  EFX_RX_HASHALG_TOEPLITZ,
					  rss->hash_types, B_TRUE) != 0)
			sfc_err(sa, "failed to restore RSS mode");
	}

fail_rx_hf_rte_to_efx:
	sfc_adapter_unlock(sa);
	return -rc;
}

/*
 * The function is used by the secondary process as well. It must not
 * use any process-local pointers from the adapter data.
 */
static int
sfc_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_rss *rss = &sas->rss;
	int entry;

	if (rss->context_type != EFX_RX_SCALE_EXCLUSIVE || sas->isolated)
		return -ENOTSUP;

	if (rss->channels == 0)
		return -EINVAL;

	if (reta_size != EFX_RSS_TBL_SIZE)
		return -EINVAL;

	for (entry = 0; entry < reta_size; entry++) {
		int grp = entry / RTE_RETA_GROUP_SIZE;
		int grp_idx = entry % RTE_RETA_GROUP_SIZE;

		if ((reta_conf[grp].mask >> grp_idx) & 1)
			reta_conf[grp].reta[grp_idx] = rss->tbl[entry];
	}

	return 0;
}

static int
sfc_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	unsigned int *rss_tbl_new;
	uint16_t entry;
	int rc = 0;


	if (sfc_sa2shared(sa)->isolated)
		return -ENOTSUP;

	if (rss->context_type != EFX_RX_SCALE_EXCLUSIVE) {
		sfc_err(sa, "RSS is not available");
		return -ENOTSUP;
	}

	if (rss->channels == 0) {
		sfc_err(sa, "RSS is not configured");
		return -EINVAL;
	}

	if (reta_size != EFX_RSS_TBL_SIZE) {
		sfc_err(sa, "RETA size is wrong (should be %u)",
			EFX_RSS_TBL_SIZE);
		return -EINVAL;
	}

	rss_tbl_new = rte_zmalloc("rss_tbl_new", sizeof(rss->tbl), 0);
	if (rss_tbl_new == NULL)
		return -ENOMEM;

	sfc_adapter_lock(sa);

	rte_memcpy(rss_tbl_new, rss->tbl, sizeof(rss->tbl));

	for (entry = 0; entry < reta_size; entry++) {
		int grp_idx = entry % RTE_RETA_GROUP_SIZE;
		struct rte_eth_rss_reta_entry64 *grp;

		grp = &reta_conf[entry / RTE_RETA_GROUP_SIZE];

		if (grp->mask & (1ull << grp_idx)) {
			if (grp->reta[grp_idx] >= rss->channels) {
				rc = EINVAL;
				goto bad_reta_entry;
			}
			rss_tbl_new[entry] = grp->reta[grp_idx];
		}
	}

	if (sa->state == SFC_ADAPTER_STARTED) {
		rc = efx_rx_scale_tbl_set(sa->nic, EFX_RSS_CONTEXT_DEFAULT,
					  rss_tbl_new, EFX_RSS_TBL_SIZE);
		if (rc != 0)
			goto fail_scale_tbl_set;
	}

	rte_memcpy(rss->tbl, rss_tbl_new, sizeof(rss->tbl));

fail_scale_tbl_set:
bad_reta_entry:
	sfc_adapter_unlock(sa);

	rte_free(rss_tbl_new);

	SFC_ASSERT(rc >= 0);
	return -rc;
}

static int
sfc_dev_filter_ctrl(struct rte_eth_dev *dev, enum rte_filter_type filter_type,
		    enum rte_filter_op filter_op,
		    void *arg)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int rc = ENOTSUP;

	sfc_log_init(sa, "entry");

	switch (filter_type) {
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

static int
sfc_pool_ops_supported(struct rte_eth_dev *dev, const char *pool)
{
	const struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(dev);

	/*
	 * If Rx datapath does not provide callback to check mempool,
	 * all pools are supported.
	 */
	if (sap->dp_rx->pool_ops_supported == NULL)
		return 1;

	return sap->dp_rx->pool_ops_supported(pool);
}

static int
sfc_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	const struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(dev);
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_rxq_info *rxq_info;

	SFC_ASSERT(queue_id < sas->rxq_count);
	rxq_info = &sas->rxq_info[queue_id];

	return sap->dp_rx->intr_enable(rxq_info->dp);
}

static int
sfc_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	const struct sfc_adapter_priv *sap = sfc_adapter_priv_by_eth_dev(dev);
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_rxq_info *rxq_info;

	SFC_ASSERT(queue_id < sas->rxq_count);
	rxq_info = &sas->rxq_info[queue_id];

	return sap->dp_rx->intr_disable(rxq_info->dp);
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
	.rx_queue_intr_enable		= sfc_rx_queue_intr_enable,
	.rx_queue_intr_disable		= sfc_rx_queue_intr_disable,
	.tx_queue_setup			= sfc_tx_queue_setup,
	.tx_queue_release		= sfc_tx_queue_release,
	.flow_ctrl_get			= sfc_flow_ctrl_get,
	.flow_ctrl_set			= sfc_flow_ctrl_set,
	.mac_addr_set			= sfc_mac_addr_set,
	.udp_tunnel_port_add		= sfc_dev_udp_tunnel_port_add,
	.udp_tunnel_port_del		= sfc_dev_udp_tunnel_port_del,
	.reta_update			= sfc_dev_rss_reta_update,
	.reta_query			= sfc_dev_rss_reta_query,
	.rss_hash_update		= sfc_dev_rss_hash_update,
	.rss_hash_conf_get		= sfc_dev_rss_hash_conf_get,
	.filter_ctrl			= sfc_dev_filter_ctrl,
	.set_mc_addr_list		= sfc_set_mc_addr_list,
	.rxq_info_get			= sfc_rx_queue_info_get,
	.txq_info_get			= sfc_tx_queue_info_get,
	.fw_version_get			= sfc_fw_version_get,
	.xstats_get_by_id		= sfc_xstats_get_by_id,
	.xstats_get_names_by_id		= sfc_xstats_get_names_by_id,
	.pool_ops_supported		= sfc_pool_ops_supported,
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
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	const struct sfc_dp_rx *dp_rx;
	const struct sfc_dp_tx *dp_tx;
	const efx_nic_cfg_t *encp;
	unsigned int avail_caps = 0;
	const char *rx_name = NULL;
	const char *tx_name = NULL;
	int rc;

	switch (sa->family) {
	case EFX_FAMILY_HUNTINGTON:
	case EFX_FAMILY_MEDFORD:
	case EFX_FAMILY_MEDFORD2:
		avail_caps |= SFC_DP_HW_FW_CAP_EF10;
		avail_caps |= SFC_DP_HW_FW_CAP_RX_EFX;
		avail_caps |= SFC_DP_HW_FW_CAP_TX_EFX;
		break;
	case EFX_FAMILY_RIVERHEAD:
		avail_caps |= SFC_DP_HW_FW_CAP_EF100;
		break;
	default:
		break;
	}

	encp = efx_nic_cfg_get(sa->nic);
	if (encp->enc_rx_es_super_buffer_supported)
		avail_caps |= SFC_DP_HW_FW_CAP_RX_ES_SUPER_BUFFER;

	rc = sfc_kvargs_process(sa, SFC_KVARG_RX_DATAPATH,
				sfc_kvarg_string_handler, &rx_name);
	if (rc != 0)
		goto fail_kvarg_rx_datapath;

	if (rx_name != NULL) {
		dp_rx = sfc_dp_find_rx_by_name(&sfc_dp_head, rx_name);
		if (dp_rx == NULL) {
			sfc_err(sa, "Rx datapath %s not found", rx_name);
			rc = ENOENT;
			goto fail_dp_rx;
		}
		if (!sfc_dp_match_hw_fw_caps(&dp_rx->dp, avail_caps)) {
			sfc_err(sa,
				"Insufficient Hw/FW capabilities to use Rx datapath %s",
				rx_name);
			rc = EINVAL;
			goto fail_dp_rx_caps;
		}
	} else {
		dp_rx = sfc_dp_find_rx_by_caps(&sfc_dp_head, avail_caps);
		if (dp_rx == NULL) {
			sfc_err(sa, "Rx datapath by caps %#x not found",
				avail_caps);
			rc = ENOENT;
			goto fail_dp_rx;
		}
	}

	sas->dp_rx_name = sfc_strdup(dp_rx->dp.name);
	if (sas->dp_rx_name == NULL) {
		rc = ENOMEM;
		goto fail_dp_rx_name;
	}

	sfc_notice(sa, "use %s Rx datapath", sas->dp_rx_name);

	rc = sfc_kvargs_process(sa, SFC_KVARG_TX_DATAPATH,
				sfc_kvarg_string_handler, &tx_name);
	if (rc != 0)
		goto fail_kvarg_tx_datapath;

	if (tx_name != NULL) {
		dp_tx = sfc_dp_find_tx_by_name(&sfc_dp_head, tx_name);
		if (dp_tx == NULL) {
			sfc_err(sa, "Tx datapath %s not found", tx_name);
			rc = ENOENT;
			goto fail_dp_tx;
		}
		if (!sfc_dp_match_hw_fw_caps(&dp_tx->dp, avail_caps)) {
			sfc_err(sa,
				"Insufficient Hw/FW capabilities to use Tx datapath %s",
				tx_name);
			rc = EINVAL;
			goto fail_dp_tx_caps;
		}
	} else {
		dp_tx = sfc_dp_find_tx_by_caps(&sfc_dp_head, avail_caps);
		if (dp_tx == NULL) {
			sfc_err(sa, "Tx datapath by caps %#x not found",
				avail_caps);
			rc = ENOENT;
			goto fail_dp_tx;
		}
	}

	sas->dp_tx_name = sfc_strdup(dp_tx->dp.name);
	if (sas->dp_tx_name == NULL) {
		rc = ENOMEM;
		goto fail_dp_tx_name;
	}

	sfc_notice(sa, "use %s Tx datapath", sas->dp_tx_name);

	sa->priv.dp_rx = dp_rx;
	sa->priv.dp_tx = dp_tx;

	dev->rx_pkt_burst = dp_rx->pkt_burst;
	dev->tx_pkt_prepare = dp_tx->pkt_prepare;
	dev->tx_pkt_burst = dp_tx->pkt_burst;

	dev->rx_queue_count = sfc_rx_queue_count;
	dev->rx_descriptor_done = sfc_rx_descriptor_done;
	dev->rx_descriptor_status = sfc_rx_descriptor_status;
	dev->tx_descriptor_status = sfc_tx_descriptor_status;
	dev->dev_ops = &sfc_eth_dev_ops;

	return 0;

fail_dp_tx_name:
fail_dp_tx_caps:
fail_dp_tx:
fail_kvarg_tx_datapath:
	rte_free(sas->dp_rx_name);
	sas->dp_rx_name = NULL;

fail_dp_rx_name:
fail_dp_rx_caps:
fail_dp_rx:
fail_kvarg_rx_datapath:
	return rc;
}

static void
sfc_eth_dev_clear_ops(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);

	dev->dev_ops = NULL;
	dev->tx_pkt_prepare = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	rte_free(sas->dp_tx_name);
	sas->dp_tx_name = NULL;
	sa->priv.dp_tx = NULL;

	rte_free(sas->dp_rx_name);
	sas->dp_rx_name = NULL;
	sa->priv.dp_rx = NULL;
}

static const struct eth_dev_ops sfc_eth_dev_secondary_ops = {
	.dev_supported_ptypes_get	= sfc_dev_supported_ptypes_get,
	.reta_query			= sfc_dev_rss_reta_query,
	.rss_hash_conf_get		= sfc_dev_rss_hash_conf_get,
	.rxq_info_get			= sfc_rx_queue_info_get,
	.txq_info_get			= sfc_tx_queue_info_get,
};

static int
sfc_eth_dev_secondary_init(struct rte_eth_dev *dev, uint32_t logtype_main)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct sfc_adapter_priv *sap;
	const struct sfc_dp_rx *dp_rx;
	const struct sfc_dp_tx *dp_tx;
	int rc;

	/*
	 * Allocate process private data from heap, since it should not
	 * be located in shared memory allocated using rte_malloc() API.
	 */
	sap = calloc(1, sizeof(*sap));
	if (sap == NULL) {
		rc = ENOMEM;
		goto fail_alloc_priv;
	}

	sap->logtype_main = logtype_main;

	dp_rx = sfc_dp_find_rx_by_name(&sfc_dp_head, sas->dp_rx_name);
	if (dp_rx == NULL) {
		SFC_LOG(sas, RTE_LOG_ERR, logtype_main,
			"cannot find %s Rx datapath", sas->dp_rx_name);
		rc = ENOENT;
		goto fail_dp_rx;
	}
	if (~dp_rx->features & SFC_DP_RX_FEAT_MULTI_PROCESS) {
		SFC_LOG(sas, RTE_LOG_ERR, logtype_main,
			"%s Rx datapath does not support multi-process",
			sas->dp_rx_name);
		rc = EINVAL;
		goto fail_dp_rx_multi_process;
	}

	dp_tx = sfc_dp_find_tx_by_name(&sfc_dp_head, sas->dp_tx_name);
	if (dp_tx == NULL) {
		SFC_LOG(sas, RTE_LOG_ERR, logtype_main,
			"cannot find %s Tx datapath", sas->dp_tx_name);
		rc = ENOENT;
		goto fail_dp_tx;
	}
	if (~dp_tx->features & SFC_DP_TX_FEAT_MULTI_PROCESS) {
		SFC_LOG(sas, RTE_LOG_ERR, logtype_main,
			"%s Tx datapath does not support multi-process",
			sas->dp_tx_name);
		rc = EINVAL;
		goto fail_dp_tx_multi_process;
	}

	sap->dp_rx = dp_rx;
	sap->dp_tx = dp_tx;

	dev->process_private = sap;
	dev->rx_pkt_burst = dp_rx->pkt_burst;
	dev->tx_pkt_prepare = dp_tx->pkt_prepare;
	dev->tx_pkt_burst = dp_tx->pkt_burst;
	dev->rx_queue_count = sfc_rx_queue_count;
	dev->rx_descriptor_done = sfc_rx_descriptor_done;
	dev->rx_descriptor_status = sfc_rx_descriptor_status;
	dev->tx_descriptor_status = sfc_tx_descriptor_status;
	dev->dev_ops = &sfc_eth_dev_secondary_ops;

	return 0;

fail_dp_tx_multi_process:
fail_dp_tx:
fail_dp_rx_multi_process:
fail_dp_rx:
	free(sap);

fail_alloc_priv:
	return rc;
}

static void
sfc_register_dp(void)
{
	/* Register once */
	if (TAILQ_EMPTY(&sfc_dp_head)) {
		/* Prefer EF10 datapath */
		sfc_dp_register(&sfc_dp_head, &sfc_ef100_rx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_ef10_essb_rx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_ef10_rx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_efx_rx.dp);

		sfc_dp_register(&sfc_dp_head, &sfc_ef100_tx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_ef10_tx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_efx_tx.dp);
		sfc_dp_register(&sfc_dp_head, &sfc_ef10_simple_tx.dp);
	}
}

static int
sfc_eth_dev_init(struct rte_eth_dev *dev)
{
	struct sfc_adapter_shared *sas = sfc_adapter_shared_by_eth_dev(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	uint32_t logtype_main;
	struct sfc_adapter *sa;
	int rc;
	const efx_nic_cfg_t *encp;
	const struct rte_ether_addr *from;
	int ret;

	sfc_register_dp();

	logtype_main = sfc_register_logtype(&pci_dev->addr,
					    SFC_LOGTYPE_MAIN_STR,
					    RTE_LOG_NOTICE);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -sfc_eth_dev_secondary_init(dev, logtype_main);

	/* Required for logging */
	ret = snprintf(sas->log_prefix, sizeof(sas->log_prefix),
			"PMD: sfc_efx " PCI_PRI_FMT " #%" PRIu16 ": ",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function,
			dev->data->port_id);
	if (ret < 0 || ret >= (int)sizeof(sas->log_prefix)) {
		SFC_GENERIC_LOG(ERR,
			"reserved log prefix is too short for " PCI_PRI_FMT,
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
		return -EINVAL;
	}
	sas->pci_addr = pci_dev->addr;
	sas->port_id = dev->data->port_id;

	/*
	 * Allocate process private data from heap, since it should not
	 * be located in shared memory allocated using rte_malloc() API.
	 */
	sa = calloc(1, sizeof(*sa));
	if (sa == NULL) {
		rc = ENOMEM;
		goto fail_alloc_sa;
	}

	dev->process_private = sa;

	/* Required for logging */
	sa->priv.shared = sas;
	sa->priv.logtype_main = logtype_main;

	sa->eth_dev = dev;

	/* Copy PCI device info to the dev->data */
	rte_eth_copy_pci_info(dev, pci_dev);
	dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	dev->data->dev_flags |= RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE;

	rc = sfc_kvargs_parse(sa);
	if (rc != 0)
		goto fail_kvargs_parse;

	sfc_log_init(sa, "entry");

	dev->data->mac_addrs = rte_zmalloc("sfc", RTE_ETHER_ADDR_LEN, 0);
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
	from = (const struct rte_ether_addr *)(encp->enc_mac_addr);
	rte_ether_addr_copy(from, &dev->data->mac_addrs[0]);

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
	sfc_kvargs_cleanup(sa);

fail_kvargs_parse:
	sfc_log_init(sa, "failed %d", rc);
	dev->process_private = NULL;
	free(sa);

fail_alloc_sa:
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_eth_dev_uninit(struct rte_eth_dev *dev)
{
	sfc_dev_close(dev);

	return 0;
}

static const struct rte_pci_id pci_id_sfc_efx_map[] = {
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_FARMINGDALE) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_FARMINGDALE_VF) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_GREENPORT) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_GREENPORT_VF) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD_VF) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD2) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD2_VF) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_XILINX, EFX_PCI_DEVID_RIVERHEAD) },
	{ .vendor_id = 0 /* sentinel */ }
};

static int sfc_eth_dev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct sfc_adapter_shared), sfc_eth_dev_init);
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
	SFC_KVARG_FW_VARIANT "=" SFC_KVARG_VALUES_FW_VARIANT " "
	SFC_KVARG_RXD_WAIT_TIMEOUT_NS "=<long> "
	SFC_KVARG_STATS_UPDATE_PERIOD_MS "=<long>");

RTE_INIT(sfc_driver_register_logtype)
{
	int ret;

	ret = rte_log_register_type_and_pick_level(SFC_LOGTYPE_PREFIX "driver",
						   RTE_LOG_NOTICE);
	sfc_logtype_driver = (ret < 0) ? RTE_LOGTYPE_PMD : ret;
}
