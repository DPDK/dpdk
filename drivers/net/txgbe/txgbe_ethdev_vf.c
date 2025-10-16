/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <rte_log.h>
#include <ethdev_pci.h>
#include <rte_alarm.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"
#include "txgbe_regs_group.h"

static const struct reg_info txgbevf_regs_general[] = {
	{TXGBE_VFRST, 1, 1, "TXGBE_VFRST"},
	{TXGBE_VFSTATUS, 1, 1, "TXGBE_VFSTATUS"},
	{TXGBE_VFMBCTL, 1, 1, "TXGBE_VFMAILBOX"},
	{TXGBE_VFMBX, 16, 4, "TXGBE_VFMBX"},
	{TXGBE_VFPBWRAP, 1, 1, "TXGBE_VFPBWRAP"},
	{0, 0, 0, ""}
};

static const struct reg_info txgbevf_regs_interrupt[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbevf_regs_rxdma[] = {
	{0, 0, 0, ""}
};

static const struct reg_info txgbevf_regs_tx[] = {
	{0, 0, 0, ""}
};

/* VF registers */
static const struct reg_info *txgbevf_regs[] = {
				txgbevf_regs_general,
				txgbevf_regs_interrupt,
				txgbevf_regs_rxdma,
				txgbevf_regs_tx,
				NULL};

static int txgbevf_dev_xstats_get(struct rte_eth_dev *dev,
				  struct rte_eth_xstat *xstats, unsigned int n);
static int txgbevf_dev_info_get(struct rte_eth_dev *dev,
				 struct rte_eth_dev_info *dev_info);
static int  txgbevf_dev_configure(struct rte_eth_dev *dev);
static int  txgbevf_dev_start(struct rte_eth_dev *dev);
static int txgbevf_dev_link_update(struct rte_eth_dev *dev,
				   int wait_to_complete);
static int txgbevf_dev_stop(struct rte_eth_dev *dev);
static int txgbevf_dev_close(struct rte_eth_dev *dev);
static void txgbevf_intr_disable(struct rte_eth_dev *dev);
static void txgbevf_intr_enable(struct rte_eth_dev *dev);
static int txgbevf_dev_stats_reset(struct rte_eth_dev *dev);
static int txgbevf_vlan_offload_config(struct rte_eth_dev *dev, int mask);
static void txgbevf_set_vfta_all(struct rte_eth_dev *dev, bool on);
static void txgbevf_configure_msix(struct rte_eth_dev *dev);
static int txgbevf_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int txgbevf_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void txgbevf_remove_mac_addr(struct rte_eth_dev *dev, uint32_t index);
static void txgbevf_dev_interrupt_handler(void *param);

/*
 * The set of PCI devices this driver supports (for VF)
 */
static const struct rte_pci_id pci_id_txgbevf_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_SP1000_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, TXGBE_DEV_ID_WX1820_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = TXGBE_RING_DESC_MAX,
	.nb_min = TXGBE_RING_DESC_MIN,
	.nb_align = TXGBE_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = TXGBE_RING_DESC_MAX,
	.nb_min = TXGBE_RING_DESC_MIN,
	.nb_align = TXGBE_TXD_ALIGN,
	.nb_seg_max = TXGBE_TX_MAX_SEG,
	.nb_mtu_seg_max = TXGBE_TX_MAX_SEG,
};

static const struct eth_dev_ops txgbevf_eth_dev_ops;

static const struct rte_txgbe_xstats_name_off rte_txgbevf_stats_strings[] = {
	{"rx_multicast_packets_0",
			offsetof(struct txgbevf_hw_stats, qp[0].vfmprc)},
	{"rx_multicast_packets_1",
			offsetof(struct txgbevf_hw_stats, qp[1].vfmprc)},
	{"rx_multicast_packets_2",
			offsetof(struct txgbevf_hw_stats, qp[2].vfmprc)},
	{"rx_multicast_packets_3",
			offsetof(struct txgbevf_hw_stats, qp[3].vfmprc)},
	{"rx_multicast_packets_4",
			offsetof(struct txgbevf_hw_stats, qp[4].vfmprc)},
	{"rx_multicast_packets_5",
			offsetof(struct txgbevf_hw_stats, qp[5].vfmprc)},
	{"rx_multicast_packets_6",
			offsetof(struct txgbevf_hw_stats, qp[6].vfmprc)},
	{"rx_multicast_packets_7",
			offsetof(struct txgbevf_hw_stats, qp[7].vfmprc)}
};

#define TXGBEVF_NB_XSTATS (sizeof(rte_txgbevf_stats_strings) /	\
		sizeof(rte_txgbevf_stats_strings[0]))

/*
 * Negotiate mailbox API version with the PF.
 * After reset API version is always set to the basic one (txgbe_mbox_api_10).
 * Then we try to negotiate starting with the most recent one.
 * If all negotiation attempts fail, then we will proceed with
 * the default one (txgbe_mbox_api_10).
 */
static void
txgbevf_negotiate_api(struct txgbe_hw *hw)
{
	int32_t i;

	/* start with highest supported, proceed down */
	static const int sup_ver[] = {
		txgbe_mbox_api_21,
		txgbe_mbox_api_13,
		txgbe_mbox_api_12,
		txgbe_mbox_api_11,
		txgbe_mbox_api_10,
	};

	for (i = 0; i < ARRAY_SIZE(sup_ver); i++) {
		if (txgbevf_negotiate_api_version(hw, sup_ver[i]) == 0)
			break;
	}
}

static void
generate_random_mac_addr(struct rte_ether_addr *mac_addr)
{
	uint64_t random;

	/* Set Organizationally Unique Identifier (OUI) prefix. */
	mac_addr->addr_bytes[0] = 0x00;
	mac_addr->addr_bytes[1] = 0x09;
	mac_addr->addr_bytes[2] = 0xC0;
	/* Force indication of locally assigned MAC address. */
	mac_addr->addr_bytes[0] |= RTE_ETHER_LOCAL_ADMIN_ADDR;
	/* Generate the last 3 bytes of the MAC address with a random number. */
	random = rte_rand();
	memcpy(&mac_addr->addr_bytes[3], &random, 3);
}

int
txgbevf_inject_5tuple_filter(struct rte_eth_dev *dev,
			     struct txgbe_5tuple_filter *filter)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t mask = TXGBE_5TFCTL0_MASK;
	uint16_t index = filter->index;
	uint32_t msg[TXGBEVF_5T_MAX];
	int err;

	memset(msg, 0, sizeof(*msg));

	/* 0 means compare */
	mask &= ~TXGBE_5TFCTL0_MPOOL;
	if (filter->filter_info.src_ip_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MSADDR;
	if (filter->filter_info.dst_ip_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MDADDR;
	if (filter->filter_info.src_port_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MSPORT;
	if (filter->filter_info.dst_port_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MDPORT;
	if (filter->filter_info.proto_mask == 0)
		mask &= ~TXGBE_5TFCTL0_MPROTO;

	msg[TXGBEVF_5T_CTRL0] = mask;
	msg[TXGBEVF_5T_CTRL0] |= TXGBE_5TFCTL0_ENA;
	msg[TXGBEVF_5T_CTRL0] |= TXGBE_5TFCTL0_PROTO(filter->filter_info.proto);
	msg[TXGBEVF_5T_CTRL0] |= TXGBE_5TFCTL0_PRI(filter->filter_info.priority);
	msg[TXGBEVF_5T_CTRL1] = TXGBE_5TFCTL1_QP(filter->queue);
	msg[TXGBEVF_5T_PORT] = TXGBE_5TFPORT_DST(be_to_le16(filter->filter_info.dst_port));
	msg[TXGBEVF_5T_PORT] |= TXGBE_5TFPORT_SRC(be_to_le16(filter->filter_info.src_port));
	msg[TXGBEVF_5T_DA] = be_to_le32(filter->filter_info.dst_ip);
	msg[TXGBEVF_5T_SA] = be_to_le32(filter->filter_info.src_ip);

	err = txgbevf_add_5tuple_filter(hw, msg, index);
	if (err)
		PMD_DRV_LOG(ERR, "VF request PF to add 5tuple filters failed.");

	return err;
}

void
txgbevf_remove_5tuple_filter(struct rte_eth_dev *dev, u16 index)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int err;

	err = txgbevf_del_5tuple_filter(hw, index);
	if (err)
		PMD_DRV_LOG(ERR, "VF request PF to delete 5tuple filters failed.");
}

/*
 * Virtual Function device init
 */
static int
eth_txgbevf_dev_init(struct rte_eth_dev *eth_dev)
{
	int err;
	uint32_t tc, tcs;
	struct txgbe_adapter *ad = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	struct txgbe_vfta *shadow_vfta = TXGBE_DEV_VFTA(eth_dev);
	struct txgbe_hwstrip *hwstrip = TXGBE_DEV_HWSTRIP(eth_dev);
	struct rte_ether_addr *perm_addr =
			(struct rte_ether_addr *)hw->mac.perm_addr;
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &txgbevf_eth_dev_ops;
	eth_dev->rx_descriptor_status = txgbe_dev_rx_descriptor_status;
	eth_dev->tx_descriptor_status = txgbe_dev_tx_descriptor_status;
	eth_dev->rx_pkt_burst = &txgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &txgbe_xmit_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		struct txgbe_tx_queue *txq;
		uint16_t nb_tx_queues = eth_dev->data->nb_tx_queues;
		/* TX queue function in primary, set by last queue initialized
		 * Tx queue may not initialized by primary process
		 */
		if (eth_dev->data->tx_queues) {
			txq = eth_dev->data->tx_queues[nb_tx_queues - 1];
			txgbe_set_tx_function(eth_dev, txq);
		} else {
			/* Use default TX function if we get here */
			PMD_INIT_LOG(NOTICE,
				     "No TX queues configured yet. Using default TX function.");
		}

		txgbe_set_rx_function(eth_dev);

		return 0;
	}

	rte_atomic_store_explicit(&ad->link_thread_running, 0, rte_memory_order_seq_cst);
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	/* initialize the vfta */
	memset(shadow_vfta, 0, sizeof(*shadow_vfta));

	/* initialize the hw strip bitmap*/
	memset(hwstrip, 0, sizeof(*hwstrip));

	/* Initialize the shared code (base driver) */
	err = txgbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR,
			"Shared code init failed for txgbevf: %d", err);
		return -EIO;
	}

	/* init_mailbox_params */
	hw->mbx.init_params(hw);

	/* Reset the hw statistics */
	txgbevf_dev_stats_reset(eth_dev);

	/* Disable the interrupts for VF */
	txgbevf_intr_disable(eth_dev);

	hw->mac.num_rar_entries = 128; /* The MAX of the underlying PF */
	err = hw->mac.reset_hw(hw);

	/*
	 * The VF reset operation returns the TXGBE_ERR_INVALID_MAC_ADDR when
	 * the underlying PF driver has not assigned a MAC address to the VF.
	 * In this case, assign a random MAC address.
	 */
	if (err != 0 && err != TXGBE_ERR_INVALID_MAC_ADDR) {
		PMD_INIT_LOG(ERR, "VF Initialization Failure: %d", err);
		/*
		 * This error code will be propagated to the app by
		 * rte_eth_dev_reset, so use a public error code rather than
		 * the internal-only TXGBE_ERR_RESET_FAILED
		 */
		return -EAGAIN;
	}

	/* negotiate mailbox API version to use with the PF. */
	txgbevf_negotiate_api(hw);

	/* Get Rx/Tx queue count via mailbox, which is ready after reset_hw */
	txgbevf_get_queues(hw, &tcs, &tc);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("txgbevf", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store "
			     "MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* Generate a random MAC address, if none was assigned by PF. */
	if (rte_is_zero_ether_addr(perm_addr)) {
		generate_random_mac_addr(perm_addr);
		err = txgbe_set_rar_vf(hw, 1, perm_addr->addr_bytes, 0, 1);
		if (err) {
			rte_free(eth_dev->data->mac_addrs);
			eth_dev->data->mac_addrs = NULL;
			return err;
		}
		PMD_INIT_LOG(INFO, "\tVF MAC address not assigned by Host PF");
		PMD_INIT_LOG(INFO, "\tAssign randomly generated MAC address "
			     RTE_ETHER_ADDR_PRT_FMT,
				 RTE_ETHER_ADDR_BYTES(perm_addr));
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy(perm_addr, &eth_dev->data->mac_addrs[0]);

	/* reset the hardware with the new settings */
	err = hw->mac.start_hw(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "VF Initialization Failure: %d", err);
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		return -EIO;
	}

	/* enter promiscuous mode */
	txgbevf_dev_promiscuous_enable(eth_dev);

	rte_intr_callback_register(intr_handle,
				   txgbevf_dev_interrupt_handler, eth_dev);
	rte_intr_enable(intr_handle);
	txgbevf_intr_enable(eth_dev);

	/* initialize filter info */
	memset(filter_info, 0,
	       sizeof(struct txgbe_filter_info));

	/* initialize 5tuple filter list */
	TAILQ_INIT(&filter_info->fivetuple_list);

	/* initialize flow filter lists */
	txgbe_filterlist_init();

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x mac.type=%s",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id, "txgbe_mac_sp_vf");

	return 0;
}

/* Virtual Function device uninit */
static int
eth_txgbevf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	txgbevf_dev_close(eth_dev);

	return 0;
}

static int eth_txgbevf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct txgbe_adapter), eth_txgbevf_dev_init);
}

static int eth_txgbevf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_txgbevf_dev_uninit);
}

/*
 * virtual function driver struct
 */
static struct rte_pci_driver rte_txgbevf_pmd = {
	.id_table = pci_id_txgbevf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_txgbevf_pci_probe,
	.remove = eth_txgbevf_pci_remove,
};

static int txgbevf_dev_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned int limit)
{
	unsigned int i;

	if (limit < TXGBEVF_NB_XSTATS && xstats_names != NULL)
		return -ENOMEM;

	if (xstats_names != NULL)
		for (i = 0; i < TXGBEVF_NB_XSTATS; i++)
			snprintf(xstats_names[i].name,
				sizeof(xstats_names[i].name),
				"%s", rte_txgbevf_stats_strings[i].name);
	return TXGBEVF_NB_XSTATS;
}

static void
txgbevf_update_stats(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbevf_hw_stats *hw_stats = (struct txgbevf_hw_stats *)
			  TXGBE_DEV_STATS(dev);
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		/* Good Rx packet, include VF loopback */
		TXGBE_UPDCNT32(TXGBE_QPRXPKT(i),
		hw_stats->qp[i].last_vfgprc, hw_stats->qp[i].vfgprc);

		/* Good Rx octets, include VF loopback */
		TXGBE_UPDCNT36(TXGBE_QPRXOCTL(i),
		hw_stats->qp[i].last_vfgorc, hw_stats->qp[i].vfgorc);

		/* Rx Multicst Packet */
		TXGBE_UPDCNT32(TXGBE_QPRXMPKT(i),
		hw_stats->qp[i].last_vfmprc, hw_stats->qp[i].vfmprc);
	}
	hw->rx_loaded = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		/* Good Tx packet, include VF loopback */
		TXGBE_UPDCNT32(TXGBE_QPTXPKT(i),
		hw_stats->qp[i].last_vfgptc, hw_stats->qp[i].vfgptc);

		/* Good Tx octets, include VF loopback */
		TXGBE_UPDCNT36(TXGBE_QPTXOCTL(i),
		hw_stats->qp[i].last_vfgotc, hw_stats->qp[i].vfgotc);
	}
	hw->offset_loaded = 0;
}

static int
txgbevf_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		       unsigned int n)
{
	struct txgbevf_hw_stats *hw_stats = (struct txgbevf_hw_stats *)
			TXGBE_DEV_STATS(dev);
	unsigned int i;

	if (n < TXGBEVF_NB_XSTATS)
		return TXGBEVF_NB_XSTATS;

	txgbevf_update_stats(dev);

	if (!xstats)
		return 0;

	/* Extended stats */
	for (i = 0; i < TXGBEVF_NB_XSTATS; i++) {
		xstats[i].id = i;
		xstats[i].value = *(uint64_t *)(((char *)hw_stats) +
			rte_txgbevf_stats_strings[i].offset);
	}

	return TXGBEVF_NB_XSTATS;
}

static int
txgbevf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct txgbevf_hw_stats *hw_stats = (struct txgbevf_hw_stats *)
			  TXGBE_DEV_STATS(dev);
	uint32_t i;

	txgbevf_update_stats(dev);

	if (stats == NULL)
		return -EINVAL;

	stats->ipackets = 0;
	stats->ibytes = 0;
	stats->opackets = 0;
	stats->obytes = 0;

	for (i = 0; i < 8; i++) {
		stats->ipackets += hw_stats->qp[i].vfgprc;
		stats->ibytes += hw_stats->qp[i].vfgorc;
		stats->opackets += hw_stats->qp[i].vfgptc;
		stats->obytes += hw_stats->qp[i].vfgotc;
	}

	return 0;
}

static int
txgbevf_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct txgbevf_hw_stats *hw_stats = (struct txgbevf_hw_stats *)
			TXGBE_DEV_STATS(dev);
	uint32_t i;

	/* Sync HW register to the last stats */
	txgbevf_dev_stats_get(dev, NULL);

	/* reset HW current stats*/
	for (i = 0; i < 8; i++) {
		hw_stats->qp[i].vfgprc = 0;
		hw_stats->qp[i].vfgorc = 0;
		hw_stats->qp[i].vfgptc = 0;
		hw_stats->qp[i].vfgotc = 0;
	}

	return 0;
}

static int
txgbevf_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = TXGBE_FRAME_SIZE_MAX;
	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
	dev_info->max_hash_mac_addrs = TXGBE_VMDQ_NUM_UC_MAC;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->max_vmdq_pools = RTE_ETH_64_POOLS;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;
	dev_info->rx_queue_offload_capa = txgbe_get_rx_queue_offloads(dev);
	dev_info->rx_offload_capa = (txgbe_get_rx_port_offloads(dev) |
				     dev_info->rx_queue_offload_capa);
	dev_info->tx_queue_offload_capa = txgbe_get_tx_queue_offloads(dev);
	dev_info->tx_offload_capa = txgbe_get_tx_port_offloads(dev);
	dev_info->hash_key_size = TXGBE_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = RTE_ETH_RSS_RETA_SIZE_128;
	dev_info->flow_type_rss_offloads = TXGBE_RSS_OFFLOAD_ALL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = TXGBE_DEFAULT_RX_PTHRESH,
			.hthresh = TXGBE_DEFAULT_RX_HTHRESH,
			.wthresh = TXGBE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = TXGBE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = TXGBE_DEFAULT_TX_PTHRESH,
			.hthresh = TXGBE_DEFAULT_TX_HTHRESH,
			.wthresh = TXGBE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = TXGBE_DEFAULT_TX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->err_handle_mode = RTE_ETH_ERROR_HANDLE_MODE_PASSIVE;

	return 0;
}

static int
txgbevf_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	return txgbe_dev_link_update_share(dev, wait_to_complete);
}

/*
 * Virtual Function operations
 */
static void
txgbevf_intr_disable(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();

	/* Clear interrupt mask to stop from interrupts being generated */
	wr32(hw, TXGBE_VFIMS, TXGBE_VFIMS_MASK);

	txgbe_flush(hw);

	/* Clear mask value. */
	intr->mask_misc = TXGBE_VFIMS_MASK;
}

static void
txgbevf_intr_enable(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();

	/* VF enable interrupt autoclean */
	wr32(hw, TXGBE_VFIMC, TXGBE_VFIMC_MASK);

	txgbe_flush(hw);

	intr->mask_misc = 0;
}

static int
txgbevf_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);

	PMD_INIT_LOG(DEBUG, "Configured Virtual Function port id: %d",
		     dev->data->port_id);

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/*
	 * VF has no ability to enable/disable HW CRC
	 * Keep the persistent behavior the same as Host PF
	 */
#ifndef RTE_LIBRTE_TXGBE_PF_DISABLE_STRIP_CRC
	if (conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) {
		PMD_INIT_LOG(NOTICE, "VF can't disable HW CRC Strip");
		conf->rxmode.offloads &= ~RTE_ETH_RX_OFFLOAD_KEEP_CRC;
	}
#else
	if (!(conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)) {
		PMD_INIT_LOG(NOTICE, "VF can't enable HW CRC Strip");
		conf->rxmode.offloads |= RTE_ETH_RX_OFFLOAD_KEEP_CRC;
	}
#endif

	/*
	 * Initialize to TRUE. If any of Rx queues doesn't meet the bulk
	 * allocation or vector Rx preconditions we will reset it.
	 */
	adapter->rx_bulk_alloc_allowed = true;
	adapter->rx_vec_allowed = true;

	return 0;
}

static int
txgbevf_dev_start(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t intr_vector = 0;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	int err, mask = 0;

	PMD_INIT_FUNC_TRACE();

	/* Stop the link setup handler before resetting the HW. */
	txgbe_dev_wait_setup_link_complete(dev, 0);

	err = hw->mac.reset_hw(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to reset vf hardware (%d)", err);
		return err;
	}
	hw->mac.get_link_status = true;
	hw->dev_start = true;

	/* negotiate mailbox API version to use with the PF. */
	txgbevf_negotiate_api(hw);

	txgbevf_dev_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = txgbevf_dev_rx_init(dev);

	/**
	 * In this case, reuses the MAC address assigned by VF
	 * initialization.
	 */
	if (err != 0 && err != TXGBE_ERR_INVALID_MAC_ADDR) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware (%d)", err);
		txgbe_dev_clear_queues(dev);
		return err;
	}

	/* Set vfta */
	txgbevf_set_vfta_all(dev, 1);

	/* Set HW strip */
	mask = RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK |
		RTE_ETH_VLAN_EXTEND_MASK;
	err = txgbevf_vlan_offload_config(dev, mask);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to set VLAN offload (%d)", err);
		txgbe_dev_clear_queues(dev);
		return err;
	}

	txgbevf_dev_rxtx_start(dev);

	/* check and configure queue intr-vector mapping */
	if (rte_intr_cap_multiple(intr_handle) &&
	    dev->data->dev_conf.intr_conf.rxq) {
		/* According to datasheet, only vector 0/1/2 can be used,
		 * now only one vector is used for Rx queue
		 */
		intr_vector = 1;
		if (rte_intr_efd_enable(intr_handle, intr_vector)) {
			txgbe_dev_clear_queues(dev);
			return -1;
		}
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						   dev->data->nb_rx_queues)) {
			PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
				     " intr_vec", dev->data->nb_rx_queues);
			txgbe_dev_clear_queues(dev);
			return -ENOMEM;
		}
	}
	txgbevf_configure_msix(dev);

	/* When a VF port is bound to VFIO-PCI, only miscellaneous interrupt
	 * is mapped to VFIO vector 0 in eth_txgbevf_dev_init( ).
	 * If previous VFIO interrupt mapping setting in eth_txgbevf_dev_init( )
	 * is not cleared, it will fail when following rte_intr_enable( ) tries
	 * to map Rx queue interrupt to other VFIO vectors.
	 * So clear uio/vfio intr/evevnfd first to avoid failure.
	 */
	rte_intr_disable(intr_handle);

	rte_intr_enable(intr_handle);

	/* Re-enable interrupt for VF */
	txgbevf_intr_enable(dev);

	/*
	 * Update link status right before return, because it may
	 * start link configuration process in a separate thread.
	 */
	txgbevf_dev_link_update(dev, 0);

	hw->adapter_stopped = false;

	return 0;
}

static int
txgbevf_dev_stop(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	if (hw->adapter_stopped)
		return 0;

	PMD_INIT_FUNC_TRACE();

	txgbe_dev_wait_setup_link_complete(dev, 0);

	txgbevf_intr_disable(dev);

	hw->adapter_stopped = 1;
	hw->mac.stop_hw(hw);

	/*
	 * Clear what we set, but we still keep shadow_vfta to
	 * restore after device starts
	 */
	txgbevf_set_vfta_all(dev, 0);

	/* Clear stored conf */
	dev->data->scattered_rx = 0;

	txgbe_dev_clear_queues(dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);

	adapter->rss_reta_updated = 0;
	hw->dev_start = false;

	return 0;
}

static int
txgbevf_dev_close(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw->mac.reset_hw(hw);

	ret = txgbevf_dev_stop(dev);

	txgbe_dev_free_queues(dev);

	/**
	 * Remove the VF MAC address ro ensure
	 * that the VF traffic goes to the PF
	 * after stop, close and detach of the VF
	 **/
	txgbevf_remove_mac_addr(dev, 0);

	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	/* Disable the interrupts for VF */
	txgbevf_intr_disable(dev);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	rte_intr_disable(intr_handle);
	rte_intr_callback_unregister(intr_handle,
				     txgbevf_dev_interrupt_handler, dev);

	/* Remove all ntuple filters of the device */
	txgbe_ntuple_filter_uninit(dev);

	/* clear all the filters list */
	txgbe_filterlist_flush();

	return ret;
}

/*
 * Reset VF device
 */
static int
txgbevf_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	ret = eth_txgbevf_dev_uninit(dev);
	if (ret)
		return ret;

	ret = eth_txgbevf_dev_init(dev);

	return ret;
}

static void txgbevf_set_vfta_all(struct rte_eth_dev *dev, bool on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_vfta *shadow_vfta = TXGBE_DEV_VFTA(dev);
	int i = 0, j = 0, vfta = 0, mask = 1;

	for (i = 0; i < TXGBE_VFTA_SIZE; i++) {
		vfta = shadow_vfta->vfta[i];
		if (vfta) {
			mask = 1;
			for (j = 0; j < 32; j++) {
				if (vfta & mask)
					hw->mac.set_vfta(hw, (i << 5) + j, 0,
						       on, false);
				mask <<= 1;
			}
		}
	}
}

static int
txgbevf_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_vfta *shadow_vfta = TXGBE_DEV_VFTA(dev);
	uint32_t vid_idx = 0;
	uint32_t vid_bit = 0;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	/* vind is not used in VF driver, set to 0, check txgbe_set_vfta_vf */
	ret = hw->mac.set_vfta(hw, vlan_id, 0, !!on, false);
	if (ret) {
		PMD_INIT_LOG(ERR, "Unable to set VF vlan");
		return ret;
	}
	vid_idx = (uint32_t)((vlan_id >> 5) & 0x7F);
	vid_bit = (uint32_t)(1 << (vlan_id & 0x1F));

	/* Save what we set and restore it after device reset */
	if (on)
		shadow_vfta->vfta[vid_idx] |= vid_bit;
	else
		shadow_vfta->vfta[vid_idx] &= ~vid_bit;

	return 0;
}

static void
txgbevf_vlan_strip_q_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	if (queue >= hw->mac.max_rx_queues)
		return;

	ctrl = rd32(hw, TXGBE_RXCFG(queue));
	if (on)
		ctrl |= TXGBE_RXCFG_VLAN;
	else
		ctrl &= ~TXGBE_RXCFG_VLAN;
	wr32(hw, TXGBE_RXCFG(queue), ctrl);

	txgbe_vlan_hw_strip_bitmap_set(dev, queue, on);
}

static void
txgbevf_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (!hw->adapter_stopped) {
		PMD_DRV_LOG(ERR, "Please stop port first");
		return;
	}

	txgbevf_vlan_strip_q_set(dev, queue, on);
}

static int
txgbevf_vlan_offload_config(struct rte_eth_dev *dev, int mask)
{
	struct txgbe_rx_queue *rxq;
	uint16_t i;
	int on = 0;

	/* VF function only support hw strip feature, others are not support */
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			rxq = dev->data->rx_queues[i];
			on = !!(rxq->offloads &	RTE_ETH_RX_OFFLOAD_VLAN_STRIP);
			txgbevf_vlan_strip_q_set(dev, i, on);
		}
	}

	return 0;
}

static int
txgbevf_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	if (!hw->adapter_stopped && (mask & RTE_ETH_VLAN_STRIP_MASK)) {
		PMD_DRV_LOG(ERR, "Please stop port first");
		return -EPERM;
	}

	txgbe_config_vlan_strip_on_all_queues(dev, mask);

	txgbevf_vlan_offload_config(dev, mask);

	return 0;
}

static int
txgbevf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t vec = TXGBE_MISC_VEC_ID;

	if (rte_intr_allow_others(intr_handle))
		vec = TXGBE_RX_VEC_START;
	intr->mask_misc &= ~(1 << vec);
	RTE_SET_USED(queue_id);
	wr32(hw, TXGBE_VFIMC, ~intr->mask_misc);

	rte_intr_enable(intr_handle);

	return 0;
}

static int
txgbevf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t vec = TXGBE_MISC_VEC_ID;

	if (rte_intr_allow_others(intr_handle))
		vec = TXGBE_RX_VEC_START;
	intr->mask_misc |= (1 << vec);
	RTE_SET_USED(queue_id);
	wr32(hw, TXGBE_VFIMS, intr->mask_misc);

	return 0;
}

static void
txgbevf_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
		     uint8_t queue, uint8_t msix_vector)
{
	uint32_t tmp, idx;

	if (direction == -1) {
		/* other causes */
		msix_vector |= TXGBE_VFIVAR_VLD;
		tmp = rd32(hw, TXGBE_VFIVARMISC);
		tmp &= ~0xFF;
		tmp |= msix_vector;
		wr32(hw, TXGBE_VFIVARMISC, tmp);
	} else {
		/* rx or tx cause */
		msix_vector |= TXGBE_VFIVAR_VLD; /* Workaround for ICR lost */
		idx = ((16 * (queue & 1)) + (8 * direction));
		tmp = rd32(hw, TXGBE_VFIVAR(queue >> 1));
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, TXGBE_VFIVAR(queue >> 1), tmp);
	}
}

static void
txgbevf_configure_msix(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t q_idx;
	uint32_t vector_idx = TXGBE_MISC_VEC_ID;
	uint32_t base = TXGBE_MISC_VEC_ID;

	/* Configure VF other cause ivar */
	txgbevf_set_ivar_map(hw, -1, 1, vector_idx);

	/* won't configure msix register if no mapping is done
	 * between intr vector and event fd.
	 */
	if (!rte_intr_dp_is_en(intr_handle))
		return;

	if (rte_intr_allow_others(intr_handle)) {
		base = TXGBE_RX_VEC_START;
		vector_idx = TXGBE_RX_VEC_START;
	}

	/* Configure all RX queues of VF */
	for (q_idx = 0; q_idx < dev->data->nb_rx_queues; q_idx++) {
		/* Force all queue use vector 0,
		 * as TXGBE_VF_MAXMSIVECTOR = 1
		 */
		txgbevf_set_ivar_map(hw, 0, q_idx, vector_idx);
		rte_intr_vec_list_index_set(intr_handle, q_idx,
						   vector_idx);
		if (vector_idx < base + rte_intr_nb_efd_get(intr_handle)
		    - 1)
			vector_idx++;
	}

	/* As RX queue setting above show, all queues use the vector 0.
	 * Set only the ITR value of TXGBE_MISC_VEC_ID.
	 */
	wr32(hw, TXGBE_ITR(TXGBE_MISC_VEC_ID),
		TXGBE_ITR_IVAL(TXGBE_QUEUE_ITR_INTERVAL_DEFAULT)
		| TXGBE_ITR_WRDSA);
}

static int
txgbevf_add_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		     __rte_unused uint32_t index,
		     __rte_unused uint32_t pool)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int err;

	/*
	 * On a VF, adding again the same MAC addr is not an idempotent
	 * operation. Trap this case to avoid exhausting the [very limited]
	 * set of PF resources used to store VF MAC addresses.
	 */
	if (memcmp(hw->mac.perm_addr, mac_addr,
			sizeof(struct rte_ether_addr)) == 0)
		return -1;
	err = txgbevf_set_uc_addr_vf(hw, 2, mac_addr->addr_bytes);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Unable to add MAC address "
			    RTE_ETHER_ADDR_PRT_FMT " - err=%d",
			    RTE_ETHER_ADDR_BYTES(mac_addr), err);
	return err;
}

static void
txgbevf_remove_mac_addr(struct rte_eth_dev *dev, uint32_t index)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_ether_addr *perm_addr =
			(struct rte_ether_addr *)hw->mac.perm_addr;
	struct rte_ether_addr *mac_addr;
	uint32_t i;
	int err;

	/*
	 * The TXGBE_VF_SET_MACVLAN command of the txgbe-pf driver does
	 * not support the deletion of a given MAC address.
	 * Instead, it imposes to delete all MAC addresses, then to add again
	 * all MAC addresses with the exception of the one to be deleted.
	 */
	(void)txgbevf_set_uc_addr_vf(hw, 0, NULL);

	/*
	 * Add again all MAC addresses, with the exception of the deleted one
	 * and of the permanent MAC address.
	 */
	for (i = 0, mac_addr = dev->data->mac_addrs;
	     i < hw->mac.num_rar_entries; i++, mac_addr++) {
		/* Skip the deleted MAC address */
		if (i == index)
			continue;
		/* Skip NULL MAC addresses */
		if (rte_is_zero_ether_addr(mac_addr))
			continue;
		/* Skip the permanent MAC address */
		if (memcmp(perm_addr, mac_addr,
				sizeof(struct rte_ether_addr)) == 0)
			continue;
		err = txgbevf_set_uc_addr_vf(hw, 2, mac_addr->addr_bytes);
		if (err != 0)
			PMD_DRV_LOG(ERR,
				    "Adding again MAC address "
				    RTE_ETHER_ADDR_PRT_FMT " failed "
				    "err=%d",
				    RTE_ETHER_ADDR_BYTES(mac_addr), err);
	}
}

static int
txgbevf_set_default_mac_addr(struct rte_eth_dev *dev,
		struct rte_ether_addr *addr)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	hw->mac.set_rar(hw, 0, (void *)addr, 0, 0);

	return 0;
}

static int
txgbevf_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct txgbe_hw *hw;
	uint32_t max_frame = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
	struct rte_eth_dev_data *dev_data = dev->data;

	hw = TXGBE_DEV_HW(dev);

	if (mtu < RTE_ETHER_MIN_MTU ||
			max_frame > RTE_ETHER_MAX_JUMBO_FRAME_LEN)
		return -EINVAL;

	/* If device is started, refuse mtu that requires the support of
	 * scattered packets when this feature has not been enabled before.
	 */
	if (dev_data->dev_started && !dev_data->scattered_rx &&
	    (max_frame + 2 * RTE_VLAN_HLEN >
	     dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)) {
		PMD_INIT_LOG(ERR, "Stop port first.");
		return -EINVAL;
	}

	/*
	 * When supported by the underlying PF driver, use the TXGBE_VF_SET_MTU
	 * request of the version 2.0 of the mailbox API.
	 * For now, use the TXGBE_VF_SET_LPE request of the version 1.0
	 * of the mailbox API.
	 */
	if (txgbevf_rlpml_set_vf(hw, max_frame))
		return -EINVAL;

	return 0;
}

static int
txgbevf_get_reg_length(struct rte_eth_dev *dev __rte_unused)
{
	int count = 0;
	int g_ind = 0;
	const struct reg_info *reg_group;

	while ((reg_group = txgbevf_regs[g_ind++]))
		count += txgbe_regs_group_count(reg_group);

	return count;
}

static int
txgbevf_get_regs(struct rte_eth_dev *dev,
		struct rte_dev_reg_info *regs)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t *data = regs->data;
	int g_ind = 0;
	int count = 0;
	const struct reg_info *reg_group;

	if (data == NULL) {
		regs->length = txgbevf_get_reg_length(dev);
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* Support only full register dump */
	if (regs->length == 0 ||
	    regs->length == (uint32_t)txgbevf_get_reg_length(dev)) {
		regs->version = hw->mac.type << 24 | hw->revision_id << 16 |
			hw->device_id;
		while ((reg_group = txgbevf_regs[g_ind++]))
			count += txgbe_read_regs_group(dev, &data[count],
						      reg_group);
		return 0;
	}

	return -ENOTSUP;
}

static int
txgbevf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int ret;

	switch (hw->mac.update_xcast_mode(hw, TXGBEVF_XCAST_MODE_PROMISC)) {
	case 0:
		ret = 0;
		break;
	case TXGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static int
txgbevf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int mode = TXGBEVF_XCAST_MODE_NONE;
	int ret;

	if (dev->data->all_multicast)
		mode = TXGBEVF_XCAST_MODE_ALLMULTI;

	switch (hw->mac.update_xcast_mode(hw, mode)) {
	case 0:
		ret = 0;
		break;
	case TXGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static int
txgbevf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int ret;

	if (dev->data->promiscuous)
		return 0;

	switch (hw->mac.update_xcast_mode(hw, TXGBEVF_XCAST_MODE_ALLMULTI)) {
	case 0:
		ret = 0;
		break;
	case TXGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static int
txgbevf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int ret;

	if (dev->data->promiscuous)
		return 0;

	switch (hw->mac.update_xcast_mode(hw, TXGBEVF_XCAST_MODE_MULTI)) {
	case 0:
		ret = 0;
		break;
	case TXGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static void txgbevf_mbx_process(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	u32 in_msg = 0;

	/* peek the message first */
	in_msg = rd32(hw, TXGBE_VFMBX);

	/* PF reset VF event */
	if (in_msg == TXGBE_PF_CONTROL_MSG) {
		/* dummy mbx read to ack pf */
		if (txgbe_read_mbx(hw, &in_msg, 1, 0))
			return;
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RESET,
					      NULL);
	}
}

static int
txgbevf_dev_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t eicr;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);
	txgbevf_intr_disable(dev);

	/* read-on-clear nic registers here */
	eicr = rd32(hw, TXGBE_VFICR);
	intr->flags = 0;

	/* only one misc vector supported - mailbox */
	eicr &= TXGBE_VFICR_MASK;
	/* Workaround for ICR lost */
	intr->flags |= TXGBE_FLAG_MAILBOX;

	/* To avoid compiler warnings set eicr to used. */
	RTE_SET_USED(eicr);

	return 0;
}

static int
txgbevf_dev_interrupt_action(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	if (intr->flags & TXGBE_FLAG_MAILBOX) {
		txgbevf_mbx_process(dev);
		intr->flags &= ~TXGBE_FLAG_MAILBOX;
	}

	txgbevf_intr_enable(dev);

	return 0;
}

static void
txgbevf_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	txgbevf_dev_interrupt_get_status(dev);
	txgbevf_dev_interrupt_action(dev);
}

static int
txgbevf_dev_flow_ops_get(__rte_unused struct rte_eth_dev *dev,
			 const struct rte_flow_ops **ops)
{
	*ops = &txgbe_flow_ops;
	return 0;
}

/*
 * dev_ops for virtual function, bare necessities for basic vf
 * operation have been implemented
 */
static const struct eth_dev_ops txgbevf_eth_dev_ops = {
	.dev_configure        = txgbevf_dev_configure,
	.dev_start            = txgbevf_dev_start,
	.dev_stop             = txgbevf_dev_stop,
	.link_update          = txgbevf_dev_link_update,
	.stats_get            = txgbevf_dev_stats_get,
	.xstats_get           = txgbevf_dev_xstats_get,
	.stats_reset          = txgbevf_dev_stats_reset,
	.xstats_reset         = txgbevf_dev_stats_reset,
	.xstats_get_names     = txgbevf_dev_xstats_get_names,
	.dev_close            = txgbevf_dev_close,
	.dev_reset	      = txgbevf_dev_reset,
	.promiscuous_enable   = txgbevf_dev_promiscuous_enable,
	.promiscuous_disable  = txgbevf_dev_promiscuous_disable,
	.allmulticast_enable  = txgbevf_dev_allmulticast_enable,
	.allmulticast_disable = txgbevf_dev_allmulticast_disable,
	.dev_infos_get        = txgbevf_dev_info_get,
	.dev_supported_ptypes_get = txgbe_dev_supported_ptypes_get,
	.mtu_set              = txgbevf_dev_set_mtu,
	.vlan_filter_set      = txgbevf_vlan_filter_set,
	.vlan_strip_queue_set = txgbevf_vlan_strip_queue_set,
	.vlan_offload_set     = txgbevf_vlan_offload_set,
	.rx_queue_setup       = txgbe_dev_rx_queue_setup,
	.rx_queue_release     = txgbe_dev_rx_queue_release,
	.tx_queue_setup       = txgbe_dev_tx_queue_setup,
	.tx_queue_release     = txgbe_dev_tx_queue_release,
	.rx_queue_intr_enable = txgbevf_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable = txgbevf_dev_rx_queue_intr_disable,
	.mac_addr_add         = txgbevf_add_mac_addr,
	.mac_addr_remove      = txgbevf_remove_mac_addr,
	.set_mc_addr_list     = txgbe_dev_set_mc_addr_list,
	.rxq_info_get         = txgbe_rxq_info_get,
	.txq_info_get         = txgbe_txq_info_get,
	.mac_addr_set         = txgbevf_set_default_mac_addr,
	.get_reg              = txgbevf_get_regs,
	.reta_update          = txgbe_dev_rss_reta_update,
	.reta_query           = txgbe_dev_rss_reta_query,
	.rss_hash_update      = txgbe_dev_rss_hash_update,
	.rss_hash_conf_get    = txgbe_dev_rss_hash_conf_get,
	.tx_done_cleanup      = txgbe_dev_tx_done_cleanup,
	.flow_ops_get         = txgbevf_dev_flow_ops_get,
};

RTE_PMD_REGISTER_PCI(net_txgbe_vf, rte_txgbevf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_txgbe_vf, pci_id_txgbevf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_txgbe_vf, "* igb_uio | vfio-pci");
