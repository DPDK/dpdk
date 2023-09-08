/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * Copyright (c) 2015-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#include "bnx2x.h"
#include "bnx2x_rxtx.h"

#include <rte_string_fns.h>
#include <dev_driver.h>
#include <ethdev_pci.h>
#include <rte_alarm.h>

/*
 * The set of PCI devices this driver supports
 */
#define BROADCOM_PCI_VENDOR_ID 0x14E4
#define QLOGIC_PCI_VENDOR_ID 0x1077
static const struct rte_pci_id pci_id_bnx2x_map[] = {
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57800) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57711) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57810) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57811) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57840_OBS) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57840_4_10) },
	{ RTE_PCI_DEVICE(QLOGIC_PCI_VENDOR_ID, CHIP_NUM_57840_4_10) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57840_2_20) },
#ifdef RTE_LIBRTE_BNX2X_MF_SUPPORT
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57810_MF) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57811_MF) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57840_MF) },
	{ RTE_PCI_DEVICE(QLOGIC_PCI_VENDOR_ID, CHIP_NUM_57840_MF) },
#endif
	{ .vendor_id = 0, }
};

static const struct rte_pci_id pci_id_bnx2xvf_map[] = {
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57800_VF) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57810_VF) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57811_VF) },
	{ RTE_PCI_DEVICE(BROADCOM_PCI_VENDOR_ID, CHIP_NUM_57840_VF) },
	{ RTE_PCI_DEVICE(QLOGIC_PCI_VENDOR_ID, CHIP_NUM_57840_VF) },
	{ .vendor_id = 0, }
};

struct rte_bnx2x_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t offset_hi;
	uint32_t offset_lo;
};

static const struct rte_bnx2x_xstats_name_off bnx2x_xstats_strings[] = {
	{"rx_buffer_drops",
		offsetof(struct bnx2x_eth_stats, brb_drop_hi),
		offsetof(struct bnx2x_eth_stats, brb_drop_lo)},
	{"rx_buffer_truncates",
		offsetof(struct bnx2x_eth_stats, brb_truncate_hi),
		offsetof(struct bnx2x_eth_stats, brb_truncate_lo)},
	{"rx_buffer_truncate_discard",
		offsetof(struct bnx2x_eth_stats, brb_truncate_discard),
		offsetof(struct bnx2x_eth_stats, brb_truncate_discard)},
	{"mac_filter_discard",
		offsetof(struct bnx2x_eth_stats, mac_filter_discard),
		offsetof(struct bnx2x_eth_stats, mac_filter_discard)},
	{"no_match_vlan_tag_discard",
		offsetof(struct bnx2x_eth_stats, mf_tag_discard),
		offsetof(struct bnx2x_eth_stats, mf_tag_discard)},
	{"tx_pause",
		offsetof(struct bnx2x_eth_stats, pause_frames_sent_hi),
		offsetof(struct bnx2x_eth_stats, pause_frames_sent_lo)},
	{"rx_pause",
		offsetof(struct bnx2x_eth_stats, pause_frames_received_hi),
		offsetof(struct bnx2x_eth_stats, pause_frames_received_lo)},
	{"tx_priority_flow_control",
		offsetof(struct bnx2x_eth_stats, pfc_frames_sent_hi),
		offsetof(struct bnx2x_eth_stats, pfc_frames_sent_lo)},
	{"rx_priority_flow_control",
		offsetof(struct bnx2x_eth_stats, pfc_frames_received_hi),
		offsetof(struct bnx2x_eth_stats, pfc_frames_received_lo)}
};

static int
bnx2x_link_update(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	struct rte_eth_link link;

	PMD_INIT_FUNC_TRACE(sc);

	memset(&link, 0, sizeof(link));
	mb();
	link.link_speed = sc->link_vars.line_speed;
	switch (sc->link_vars.duplex) {
		case DUPLEX_FULL:
			link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case DUPLEX_HALF:
			link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
			break;
	}
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
		 RTE_ETH_LINK_SPEED_FIXED);
	link.link_status = sc->link_vars.link_up;

	return rte_eth_linkstatus_set(dev, &link);
}

static void
bnx2x_interrupt_action(struct rte_eth_dev *dev, int intr_cxt)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	uint32_t link_status;

	bnx2x_intr_legacy(sc);

	if ((atomic_load_acq_long(&sc->periodic_flags) == PERIODIC_GO) &&
	    !intr_cxt)
		bnx2x_periodic_callout(sc);
	link_status = REG_RD(sc, sc->link_params.shmem_base +
			offsetof(struct shmem_region,
				port_mb[sc->link_params.port].link_status));
	if ((link_status & LINK_STATUS_LINK_UP) != dev->data->dev_link.link_status)
		bnx2x_link_update(dev);
}

static void
bnx2x_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_DEBUG_PERIODIC_LOG(INFO, sc, "Interrupt handled");

	bnx2x_interrupt_action(dev, 1);
	rte_intr_ack(sc->pci_dev->intr_handle);
}

static void bnx2x_periodic_start(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct bnx2x_softc *sc = dev->data->dev_private;
	int ret = 0;

	atomic_store_rel_long(&sc->periodic_flags, PERIODIC_GO);
	bnx2x_interrupt_action(dev, 0);
	if (IS_PF(sc)) {
		ret = rte_eal_alarm_set(BNX2X_SP_TIMER_PERIOD,
					bnx2x_periodic_start, (void *)dev);
		if (ret) {
			PMD_DRV_LOG(ERR, sc, "Unable to start periodic"
					     " timer rc %d", ret);
		}
	}
}

void bnx2x_periodic_stop(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct bnx2x_softc *sc = dev->data->dev_private;

	atomic_store_rel_long(&sc->periodic_flags, PERIODIC_STOP);

	rte_eal_alarm_cancel(bnx2x_periodic_start, (void *)dev);

	PMD_DRV_LOG(DEBUG, sc, "Periodic poll stopped");
}

/*
 * Devops - helper functions can be called from user application
 */

static int
bnx2x_dev_configure(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	int mp_ncpus = sysconf(_SC_NPROCESSORS_CONF);

	PMD_INIT_FUNC_TRACE(sc);

	sc->mtu = dev->data->dev_conf.rxmode.mtu;

	if (dev->data->nb_tx_queues > dev->data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, sc, "The number of TX queues is greater than number of RX queues");
		return -EINVAL;
	}

	sc->num_queues = MAX(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	if (sc->num_queues > mp_ncpus) {
		PMD_DRV_LOG(ERR, sc, "The number of queues is more than number of CPUs");
		return -EINVAL;
	}

	PMD_DRV_LOG(DEBUG, sc, "num_queues=%d, mtu=%d",
		       sc->num_queues, sc->mtu);

	/* allocate ilt */
	if (bnx2x_alloc_ilt_mem(sc) != 0) {
		PMD_DRV_LOG(ERR, sc, "bnx2x_alloc_ilt_mem was failed");
		return -ENXIO;
	}

	bnx2x_dev_rxtx_init_dummy(dev);
	return 0;
}

static int
bnx2x_dev_start(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int ret = 0;
	uint16_t i;

	PMD_INIT_FUNC_TRACE(sc);

	/* start the periodic callout */
	if (IS_PF(sc)) {
		if (atomic_load_acq_long(&sc->periodic_flags) ==
		    PERIODIC_STOP) {
			bnx2x_periodic_start(dev);
			PMD_DRV_LOG(DEBUG, sc, "Periodic poll re-started");
		}
	}

	ret = bnx2x_init(sc);
	if (ret) {
		PMD_DRV_LOG(DEBUG, sc, "bnx2x_init failed (%d)", ret);
		return -1;
	}

	if (IS_PF(sc)) {
		rte_intr_callback_register(sc->pci_dev->intr_handle,
				bnx2x_interrupt_handler, (void *)dev);

		if (rte_intr_enable(sc->pci_dev->intr_handle))
			PMD_DRV_LOG(ERR, sc, "rte_intr_enable failed");
	}

	/* Configure the previously stored Multicast address list */
	if (IS_VF(sc))
		bnx2x_vfpf_set_mcast(sc, sc->mc_addrs, sc->mc_addrs_num);
	bnx2x_dev_rxtx_init(dev);

	bnx2x_print_device_info(sc);

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return ret;
}

static int
bnx2x_dev_stop(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int ret = 0;
	uint16_t i;

	PMD_INIT_FUNC_TRACE(sc);

	bnx2x_dev_rxtx_init_dummy(dev);

	if (IS_PF(sc)) {
		rte_intr_disable(sc->pci_dev->intr_handle);
		rte_intr_callback_unregister(sc->pci_dev->intr_handle,
				bnx2x_interrupt_handler, (void *)dev);

		/* stop the periodic callout */
		bnx2x_periodic_stop(dev);
	}
	/* Remove the configured Multicast list
	 * Sending NULL for the list of address and the
	 * Number is set to 0 denoting DEL_CMD
	 */
	if (IS_VF(sc))
		bnx2x_vfpf_set_mcast(sc, NULL, 0);
	ret = bnx2x_nic_unload(sc, UNLOAD_NORMAL, FALSE);
	if (ret) {
		PMD_DRV_LOG(DEBUG, sc, "bnx2x_nic_unload failed (%d)", ret);
		return ret;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
bnx2x_dev_close(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE(sc);

	/* only close in case of the primary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (IS_VF(sc))
		bnx2x_vf_close(sc);

	bnx2x_dev_clear_queues(dev);
	memset(&(dev->data->dev_link), 0 , sizeof(struct rte_eth_link));

	/* free ilt */
	bnx2x_free_ilt_mem(sc);

	/* mac_addrs must not be freed alone because part of dev_private */
	dev->data->mac_addrs = NULL;

	return 0;
}

static int
bnx2x_promisc_enable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE(sc);
	sc->rx_mode = BNX2X_RX_MODE_PROMISC;
	if (rte_eth_allmulticast_get(dev->data->port_id) == 1)
		sc->rx_mode = BNX2X_RX_MODE_ALLMULTI_PROMISC;
	bnx2x_set_rx_mode(sc);

	return 0;
}

static int
bnx2x_promisc_disable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE(sc);
	sc->rx_mode = BNX2X_RX_MODE_NORMAL;
	if (rte_eth_allmulticast_get(dev->data->port_id) == 1)
		sc->rx_mode = BNX2X_RX_MODE_ALLMULTI;
	bnx2x_set_rx_mode(sc);

	return 0;
}

static int
bnx2x_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE(sc);
	sc->rx_mode = BNX2X_RX_MODE_ALLMULTI;
	if (rte_eth_promiscuous_get(dev->data->port_id) == 1)
		sc->rx_mode = BNX2X_RX_MODE_ALLMULTI_PROMISC;
	bnx2x_set_rx_mode(sc);

	return 0;
}

static int
bnx2x_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE(sc);
	sc->rx_mode = BNX2X_RX_MODE_NORMAL;
	if (rte_eth_promiscuous_get(dev->data->port_id) == 1)
		sc->rx_mode = BNX2X_RX_MODE_PROMISC;
	bnx2x_set_rx_mode(sc);

	return 0;
}

static int
bnx2x_dev_set_mc_addr_list(struct rte_eth_dev *dev,
		struct rte_ether_addr *mc_addrs, uint32_t mc_addrs_num)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int err;
	PMD_INIT_FUNC_TRACE(sc);
	/* flush previous addresses */
	err = bnx2x_vfpf_set_mcast(sc, NULL, 0);
	if (err)
		return err;
	sc->mc_addrs_num = 0;

	/* Add new ones */
	err = bnx2x_vfpf_set_mcast(sc, mc_addrs, mc_addrs_num);
	if (err)
		return err;

	sc->mc_addrs_num = mc_addrs_num;
	memcpy(sc->mc_addrs, mc_addrs, mc_addrs_num * sizeof(*mc_addrs));

	return 0;
}

static int
bnx2x_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE(sc);

	return bnx2x_link_update(dev);
}

static int
bnx2xvf_dev_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	int ret = 0;

	ret = bnx2x_link_update(dev);

	bnx2x_check_bull(sc);
	if (sc->old_bulletin.valid_bitmap & (1 << CHANNEL_DOWN)) {
		PMD_DRV_LOG(ERR, sc, "PF indicated channel is down."
				"VF device is no longer operational");
		dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	}

	return ret;
}

static int
bnx2x_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	uint32_t brb_truncate_discard;
	uint64_t brb_drops;
	uint64_t brb_truncates;

	PMD_INIT_FUNC_TRACE(sc);

	bnx2x_stats_handle(sc, STATS_EVENT_UPDATE);

	memset(stats, 0, sizeof (struct rte_eth_stats));

	stats->ipackets =
		HILO_U64(sc->eth_stats.total_unicast_packets_received_hi,
				sc->eth_stats.total_unicast_packets_received_lo) +
		HILO_U64(sc->eth_stats.total_multicast_packets_received_hi,
				sc->eth_stats.total_multicast_packets_received_lo) +
		HILO_U64(sc->eth_stats.total_broadcast_packets_received_hi,
				sc->eth_stats.total_broadcast_packets_received_lo);

	stats->opackets =
		HILO_U64(sc->eth_stats.total_unicast_packets_transmitted_hi,
				sc->eth_stats.total_unicast_packets_transmitted_lo) +
		HILO_U64(sc->eth_stats.total_multicast_packets_transmitted_hi,
				sc->eth_stats.total_multicast_packets_transmitted_lo) +
		HILO_U64(sc->eth_stats.total_broadcast_packets_transmitted_hi,
				sc->eth_stats.total_broadcast_packets_transmitted_lo);

	stats->ibytes =
		HILO_U64(sc->eth_stats.total_bytes_received_hi,
				sc->eth_stats.total_bytes_received_lo);

	stats->obytes =
		HILO_U64(sc->eth_stats.total_bytes_transmitted_hi,
				sc->eth_stats.total_bytes_transmitted_lo);

	stats->ierrors =
		HILO_U64(sc->eth_stats.error_bytes_received_hi,
				sc->eth_stats.error_bytes_received_lo);

	stats->oerrors = 0;

	stats->rx_nombuf =
		HILO_U64(sc->eth_stats.no_buff_discard_hi,
				sc->eth_stats.no_buff_discard_lo);

	brb_drops =
		HILO_U64(sc->eth_stats.brb_drop_hi,
			 sc->eth_stats.brb_drop_lo);

	brb_truncates =
		HILO_U64(sc->eth_stats.brb_truncate_hi,
			 sc->eth_stats.brb_truncate_lo);

	brb_truncate_discard = sc->eth_stats.brb_truncate_discard;

	stats->imissed = brb_drops + brb_truncates +
			 brb_truncate_discard + stats->rx_nombuf;

	return 0;
}

static int
bnx2x_get_xstats_names(__rte_unused struct rte_eth_dev *dev,
		       struct rte_eth_xstat_name *xstats_names,
		       __rte_unused unsigned limit)
{
	unsigned int i, stat_cnt = RTE_DIM(bnx2x_xstats_strings);

	if (xstats_names != NULL)
		for (i = 0; i < stat_cnt; i++)
			strlcpy(xstats_names[i].name,
				bnx2x_xstats_strings[i].name,
				sizeof(xstats_names[i].name));

	return stat_cnt;
}

static int
bnx2x_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		     unsigned int n)
{
	struct bnx2x_softc *sc = dev->data->dev_private;
	unsigned int num = RTE_DIM(bnx2x_xstats_strings);

	if (n < num)
		return num;

	bnx2x_stats_handle(sc, STATS_EVENT_UPDATE);

	for (num = 0; num < n; num++) {
		if (bnx2x_xstats_strings[num].offset_hi !=
		    bnx2x_xstats_strings[num].offset_lo)
			xstats[num].value = HILO_U64(
					  *(uint32_t *)((char *)&sc->eth_stats +
					  bnx2x_xstats_strings[num].offset_hi),
					  *(uint32_t *)((char *)&sc->eth_stats +
					  bnx2x_xstats_strings[num].offset_lo));
		else
			xstats[num].value =
					  *(uint64_t *)((char *)&sc->eth_stats +
					  bnx2x_xstats_strings[num].offset_lo);
		xstats[num].id = num;
	}

	return num;
}

static int
bnx2x_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	dev_info->max_rx_queues  = sc->max_rx_queues;
	dev_info->max_tx_queues  = sc->max_tx_queues;
	dev_info->min_rx_bufsize = BNX2X_MIN_RX_BUF_SIZE;
	dev_info->max_rx_pktlen  = BNX2X_MAX_RX_PKT_LEN;
	dev_info->max_mac_addrs  = BNX2X_MAX_MAC_ADDRS;
	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_20G;

	dev_info->rx_desc_lim.nb_max = MAX_RX_AVAIL;
	dev_info->rx_desc_lim.nb_min = MIN_RX_SIZE_NONTPA;
	dev_info->rx_desc_lim.nb_mtu_seg_max = 1;
	dev_info->tx_desc_lim.nb_max = MAX_TX_AVAIL;

	return 0;
}

static int
bnx2x_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		uint32_t index, uint32_t pool)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	if (sc->mac_ops.mac_addr_add) {
		sc->mac_ops.mac_addr_add(dev, mac_addr, index, pool);
		return 0;
	}
	return -ENOTSUP;
}

static void
bnx2x_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct bnx2x_softc *sc = dev->data->dev_private;

	if (sc->mac_ops.mac_addr_remove)
		sc->mac_ops.mac_addr_remove(dev, index);
}

static const struct eth_dev_ops bnx2x_eth_dev_ops = {
	.dev_configure                = bnx2x_dev_configure,
	.dev_start                    = bnx2x_dev_start,
	.dev_stop                     = bnx2x_dev_stop,
	.dev_close                    = bnx2x_dev_close,
	.promiscuous_enable           = bnx2x_promisc_enable,
	.promiscuous_disable          = bnx2x_promisc_disable,
	.allmulticast_enable          = bnx2x_dev_allmulticast_enable,
	.allmulticast_disable         = bnx2x_dev_allmulticast_disable,
	.link_update                  = bnx2x_dev_link_update,
	.stats_get                    = bnx2x_dev_stats_get,
	.xstats_get                   = bnx2x_dev_xstats_get,
	.xstats_get_names             = bnx2x_get_xstats_names,
	.dev_infos_get                = bnx2x_dev_infos_get,
	.rx_queue_setup               = bnx2x_dev_rx_queue_setup,
	.rx_queue_release             = bnx2x_dev_rx_queue_release,
	.tx_queue_setup               = bnx2x_dev_tx_queue_setup,
	.tx_queue_release             = bnx2x_dev_tx_queue_release,
	.mac_addr_add                 = bnx2x_mac_addr_add,
	.mac_addr_remove              = bnx2x_mac_addr_remove,
};

/*
 * dev_ops for virtual function
 */
static const struct eth_dev_ops bnx2xvf_eth_dev_ops = {
	.dev_configure                = bnx2x_dev_configure,
	.dev_start                    = bnx2x_dev_start,
	.dev_stop                     = bnx2x_dev_stop,
	.dev_close                    = bnx2x_dev_close,
	.promiscuous_enable           = bnx2x_promisc_enable,
	.promiscuous_disable          = bnx2x_promisc_disable,
	.allmulticast_enable          = bnx2x_dev_allmulticast_enable,
	.allmulticast_disable         = bnx2x_dev_allmulticast_disable,
	.set_mc_addr_list             = bnx2x_dev_set_mc_addr_list,
	.link_update                  = bnx2xvf_dev_link_update,
	.stats_get                    = bnx2x_dev_stats_get,
	.xstats_get                   = bnx2x_dev_xstats_get,
	.xstats_get_names             = bnx2x_get_xstats_names,
	.dev_infos_get                = bnx2x_dev_infos_get,
	.rx_queue_setup               = bnx2x_dev_rx_queue_setup,
	.rx_queue_release             = bnx2x_dev_rx_queue_release,
	.tx_queue_setup               = bnx2x_dev_tx_queue_setup,
	.tx_queue_release             = bnx2x_dev_tx_queue_release,
	.mac_addr_add                 = bnx2x_mac_addr_add,
	.mac_addr_remove              = bnx2x_mac_addr_remove,
};


static int
bnx2x_common_dev_init(struct rte_eth_dev *eth_dev, int is_vf)
{
	int ret = 0;
	struct rte_pci_device *pci_dev;
	struct rte_pci_addr pci_addr;
	struct bnx2x_softc *sc;
	static bool adapter_info = true;

	/* Extract key data structures */
	sc = eth_dev->data->dev_private;
	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	pci_addr = pci_dev->addr;

	snprintf(sc->devinfo.name, NAME_SIZE, PCI_SHORT_PRI_FMT ":dpdk-port-%u",
		 pci_addr.bus, pci_addr.devid, pci_addr.function,
		 eth_dev->data->port_id);

	PMD_INIT_FUNC_TRACE(sc);

	eth_dev->dev_ops = is_vf ? &bnx2xvf_eth_dev_ops : &bnx2x_eth_dev_ops;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		PMD_DRV_LOG(ERR, sc, "Skipping device init from secondary process");
		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	sc->pcie_bus    = pci_dev->addr.bus;
	sc->pcie_device = pci_dev->addr.devid;

	sc->devinfo.vendor_id    = pci_dev->id.vendor_id;
	sc->devinfo.device_id    = pci_dev->id.device_id;
	sc->devinfo.subvendor_id = pci_dev->id.subsystem_vendor_id;
	sc->devinfo.subdevice_id = pci_dev->id.subsystem_device_id;

	if (is_vf)
		sc->flags = BNX2X_IS_VF_FLAG;

	sc->pcie_func = pci_dev->addr.function;
	sc->bar[BAR0].base_addr = (void *)pci_dev->mem_resource[0].addr;
	if (is_vf)
		sc->bar[BAR1].base_addr = (void *)
			((uintptr_t)pci_dev->mem_resource[0].addr + PXP_VF_ADDR_DB_START);
	else
		sc->bar[BAR1].base_addr = pci_dev->mem_resource[2].addr;

	assert(sc->bar[BAR0].base_addr);
	assert(sc->bar[BAR1].base_addr);

	bnx2x_load_firmware(sc);
	assert(sc->firmware);

	if (eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		sc->udp_rss = 1;

	sc->rx_budget = BNX2X_RX_BUDGET;
	sc->hc_rx_ticks = BNX2X_RX_TICKS;
	sc->hc_tx_ticks = BNX2X_TX_TICKS;

	sc->interrupt_mode = INTR_MODE_SINGLE_MSIX;
	sc->rx_mode = BNX2X_RX_MODE_NORMAL;

	sc->pci_dev = pci_dev;
	ret = bnx2x_attach(sc);
	if (ret) {
		PMD_DRV_LOG(ERR, sc, "bnx2x_attach failed (%d)", ret);
		return ret;
	}

	/* Print important adapter info for the user. */
	if (adapter_info) {
		bnx2x_print_adapter_info(sc);
		adapter_info = false;
	}

	/* schedule periodic poll for slowpath link events */
	if (IS_PF(sc)) {
		PMD_DRV_LOG(DEBUG, sc, "Scheduling periodic poll for slowpath link events");
		ret = rte_eal_alarm_set(BNX2X_SP_TIMER_PERIOD,
					bnx2x_periodic_start, (void *)eth_dev);
		if (ret) {
			PMD_DRV_LOG(ERR, sc, "Unable to start periodic"
					     " timer rc %d", ret);
			return -EINVAL;
		}
	}

	eth_dev->data->mac_addrs =
		(struct rte_ether_addr *)sc->link_params.mac_addr;

	if (IS_VF(sc)) {
		rte_spinlock_init(&sc->vf2pf_lock);

		ret = bnx2x_dma_alloc(sc, sizeof(struct bnx2x_vf_mbx_msg),
				      &sc->vf2pf_mbox_mapping, "vf2pf_mbox",
				      RTE_CACHE_LINE_SIZE);
		if (ret)
			goto out;

		sc->vf2pf_mbox = (struct bnx2x_vf_mbx_msg *)
					 sc->vf2pf_mbox_mapping.vaddr;

		ret = bnx2x_dma_alloc(sc, sizeof(struct bnx2x_vf_bulletin),
				      &sc->pf2vf_bulletin_mapping, "vf2pf_bull",
				      RTE_CACHE_LINE_SIZE);
		if (ret)
			goto out;

		sc->pf2vf_bulletin = (struct bnx2x_vf_bulletin *)
					     sc->pf2vf_bulletin_mapping.vaddr;

		ret = bnx2x_vf_get_resources(sc, sc->max_tx_queues,
					     sc->max_rx_queues);
		if (ret)
			goto out;
	}

	return 0;

out:
	if (IS_PF(sc))
		bnx2x_periodic_stop(eth_dev);

	return ret;
}

static int
eth_bnx2x_dev_init(struct rte_eth_dev *eth_dev)
{
	struct bnx2x_softc *sc = eth_dev->data->dev_private;
	PMD_INIT_FUNC_TRACE(sc);
	return bnx2x_common_dev_init(eth_dev, 0);
}

static int
eth_bnx2xvf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct bnx2x_softc *sc = eth_dev->data->dev_private;
	PMD_INIT_FUNC_TRACE(sc);
	return bnx2x_common_dev_init(eth_dev, 1);
}

static int eth_bnx2x_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct bnx2x_softc *sc = eth_dev->data->dev_private;
	PMD_INIT_FUNC_TRACE(sc);
	bnx2x_dev_close(eth_dev);
	return 0;
}

static struct rte_pci_driver rte_bnx2x_pmd;
static struct rte_pci_driver rte_bnx2xvf_pmd;

static int eth_bnx2x_pci_probe(struct rte_pci_driver *pci_drv,
	struct rte_pci_device *pci_dev)
{
	if (pci_drv == &rte_bnx2x_pmd)
		return rte_eth_dev_pci_generic_probe(pci_dev,
				sizeof(struct bnx2x_softc), eth_bnx2x_dev_init);
	else if (pci_drv == &rte_bnx2xvf_pmd)
		return rte_eth_dev_pci_generic_probe(pci_dev,
				sizeof(struct bnx2x_softc), eth_bnx2xvf_dev_init);
	else
		return -EINVAL;
}

static int eth_bnx2x_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_bnx2x_dev_uninit);
}

static struct rte_pci_driver rte_bnx2x_pmd = {
	.id_table = pci_id_bnx2x_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_bnx2x_pci_probe,
	.remove = eth_bnx2x_pci_remove,
};

/*
 * virtual function driver struct
 */
static struct rte_pci_driver rte_bnx2xvf_pmd = {
	.id_table = pci_id_bnx2xvf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_bnx2x_pci_probe,
	.remove = eth_bnx2x_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_bnx2x, rte_bnx2x_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_bnx2x, pci_id_bnx2x_map);
RTE_PMD_REGISTER_KMOD_DEP(net_bnx2x, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PCI(net_bnx2xvf, rte_bnx2xvf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_bnx2xvf, pci_id_bnx2xvf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_bnx2xvf, "* igb_uio | vfio-pci");
RTE_LOG_REGISTER_SUFFIX(bnx2x_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(bnx2x_logtype_driver, driver, NOTICE);
