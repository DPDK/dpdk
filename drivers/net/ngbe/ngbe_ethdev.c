/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <errno.h>
#include <rte_common.h>
#include <ethdev_pci.h>

#include <rte_alarm.h>

#include "ngbe_logs.h"
#include "ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"
#include "ngbe_regs_group.h"

static const struct reg_info ngbe_regs_general[] = {
	{NGBE_RST, 1, 1, "NGBE_RST"},
	{NGBE_STAT, 1, 1, "NGBE_STAT"},
	{NGBE_PORTCTL, 1, 1, "NGBE_PORTCTL"},
	{NGBE_GPIODATA, 1, 1, "NGBE_GPIODATA"},
	{NGBE_GPIOCTL, 1, 1, "NGBE_GPIOCTL"},
	{NGBE_LEDCTL, 1, 1, "NGBE_LEDCTL"},
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_nvm[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_interrupt[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_fctl_others[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_rxdma[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_rx[] = {
	{0, 0, 0, ""}
};

static struct reg_info ngbe_regs_tx[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_wakeup[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_mac[] = {
	{0, 0, 0, ""}
};

static const struct reg_info ngbe_regs_diagnostic[] = {
	{0, 0, 0, ""},
};

/* PF registers */
static const struct reg_info *ngbe_regs_others[] = {
				ngbe_regs_general,
				ngbe_regs_nvm,
				ngbe_regs_interrupt,
				ngbe_regs_fctl_others,
				ngbe_regs_rxdma,
				ngbe_regs_rx,
				ngbe_regs_tx,
				ngbe_regs_wakeup,
				ngbe_regs_mac,
				ngbe_regs_diagnostic,
				NULL};

static int ngbe_dev_close(struct rte_eth_dev *dev);
static int ngbe_dev_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);
static int ngbe_dev_stats_reset(struct rte_eth_dev *dev);
static void ngbe_vlan_hw_strip_enable(struct rte_eth_dev *dev, uint16_t queue);
static void ngbe_vlan_hw_strip_disable(struct rte_eth_dev *dev,
					uint16_t queue);

static void ngbe_dev_link_status_print(struct rte_eth_dev *dev);
static int ngbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on);
static int ngbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev);
static int ngbe_dev_misc_interrupt_setup(struct rte_eth_dev *dev);
static int ngbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev);
static void ngbe_dev_interrupt_handler(void *param);
static void ngbe_configure_msix(struct rte_eth_dev *dev);

#define NGBE_SET_HWSTRIP(h, q) do {\
		uint32_t idx = (q) / (sizeof((h)->bitmap[0]) * NBBY); \
		uint32_t bit = (q) % (sizeof((h)->bitmap[0]) * NBBY); \
		(h)->bitmap[idx] |= 1 << bit;\
	} while (0)

#define NGBE_CLEAR_HWSTRIP(h, q) do {\
		uint32_t idx = (q) / (sizeof((h)->bitmap[0]) * NBBY); \
		uint32_t bit = (q) % (sizeof((h)->bitmap[0]) * NBBY); \
		(h)->bitmap[idx] &= ~(1 << bit);\
	} while (0)

#define NGBE_GET_HWSTRIP(h, q, r) do {\
		uint32_t idx = (q) / (sizeof((h)->bitmap[0]) * NBBY); \
		uint32_t bit = (q) % (sizeof((h)->bitmap[0]) * NBBY); \
		(r) = (h)->bitmap[idx] >> bit & 1;\
	} while (0)

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_ngbe_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4S) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860NCSI) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1L) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL_W) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = NGBE_RING_DESC_MAX,
	.nb_min = NGBE_RING_DESC_MIN,
	.nb_align = NGBE_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = NGBE_RING_DESC_MAX,
	.nb_min = NGBE_RING_DESC_MIN,
	.nb_align = NGBE_TXD_ALIGN,
	.nb_seg_max = NGBE_TX_MAX_SEG,
	.nb_mtu_seg_max = NGBE_TX_MAX_SEG,
};

static const struct eth_dev_ops ngbe_eth_dev_ops;

#define HW_XSTAT(m) {#m, offsetof(struct ngbe_hw_stats, m)}
#define HW_XSTAT_NAME(m, n) {n, offsetof(struct ngbe_hw_stats, m)}
static const struct rte_ngbe_xstats_name_off rte_ngbe_stats_strings[] = {
	/* MNG RxTx */
	HW_XSTAT(mng_bmc2host_packets),
	HW_XSTAT(mng_host2bmc_packets),
	/* Basic RxTx */
	HW_XSTAT(rx_packets),
	HW_XSTAT(tx_packets),
	HW_XSTAT(rx_bytes),
	HW_XSTAT(tx_bytes),
	HW_XSTAT(rx_total_bytes),
	HW_XSTAT(rx_total_packets),
	HW_XSTAT(tx_total_packets),
	HW_XSTAT(rx_total_missed_packets),
	HW_XSTAT(rx_broadcast_packets),
	HW_XSTAT(rx_multicast_packets),
	HW_XSTAT(rx_management_packets),
	HW_XSTAT(tx_management_packets),
	HW_XSTAT(rx_management_dropped),
	HW_XSTAT(rx_dma_drop),
	HW_XSTAT(tx_dma_drop),
	HW_XSTAT(tx_secdrp_packets),

	/* Basic Error */
	HW_XSTAT(rx_crc_errors),
	HW_XSTAT(rx_illegal_byte_errors),
	HW_XSTAT(rx_error_bytes),
	HW_XSTAT(rx_mac_short_packet_dropped),
	HW_XSTAT(rx_length_errors),
	HW_XSTAT(rx_undersize_errors),
	HW_XSTAT(rx_fragment_errors),
	HW_XSTAT(rx_oversize_cnt),
	HW_XSTAT(rx_jabber_errors),
	HW_XSTAT(rx_l3_l4_xsum_error),
	HW_XSTAT(mac_local_errors),
	HW_XSTAT(mac_remote_errors),

	/* PB Stats */
	HW_XSTAT(rx_up_dropped),
	HW_XSTAT(rdb_pkt_cnt),
	HW_XSTAT(rdb_repli_cnt),
	HW_XSTAT(rdb_drp_cnt),

	/* MACSEC */
	HW_XSTAT(tx_macsec_pkts_untagged),
	HW_XSTAT(tx_macsec_pkts_encrypted),
	HW_XSTAT(tx_macsec_pkts_protected),
	HW_XSTAT(tx_macsec_octets_encrypted),
	HW_XSTAT(tx_macsec_octets_protected),
	HW_XSTAT(rx_macsec_pkts_untagged),
	HW_XSTAT(rx_macsec_pkts_badtag),
	HW_XSTAT(rx_macsec_pkts_nosci),
	HW_XSTAT(rx_macsec_pkts_unknownsci),
	HW_XSTAT(rx_macsec_octets_decrypted),
	HW_XSTAT(rx_macsec_octets_validated),
	HW_XSTAT(rx_macsec_sc_pkts_unchecked),
	HW_XSTAT(rx_macsec_sc_pkts_delayed),
	HW_XSTAT(rx_macsec_sc_pkts_late),
	HW_XSTAT(rx_macsec_sa_pkts_ok),
	HW_XSTAT(rx_macsec_sa_pkts_invalid),
	HW_XSTAT(rx_macsec_sa_pkts_notvalid),
	HW_XSTAT(rx_macsec_sa_pkts_unusedsa),
	HW_XSTAT(rx_macsec_sa_pkts_notusingsa),

	/* MAC RxTx */
	HW_XSTAT(rx_size_64_packets),
	HW_XSTAT(rx_size_65_to_127_packets),
	HW_XSTAT(rx_size_128_to_255_packets),
	HW_XSTAT(rx_size_256_to_511_packets),
	HW_XSTAT(rx_size_512_to_1023_packets),
	HW_XSTAT(rx_size_1024_to_max_packets),
	HW_XSTAT(tx_size_64_packets),
	HW_XSTAT(tx_size_65_to_127_packets),
	HW_XSTAT(tx_size_128_to_255_packets),
	HW_XSTAT(tx_size_256_to_511_packets),
	HW_XSTAT(tx_size_512_to_1023_packets),
	HW_XSTAT(tx_size_1024_to_max_packets),

	/* Flow Control */
	HW_XSTAT(tx_xon_packets),
	HW_XSTAT(rx_xon_packets),
	HW_XSTAT(tx_xoff_packets),
	HW_XSTAT(rx_xoff_packets),

	HW_XSTAT_NAME(tx_xon_packets, "tx_flow_control_xon_packets"),
	HW_XSTAT_NAME(rx_xon_packets, "rx_flow_control_xon_packets"),
	HW_XSTAT_NAME(tx_xoff_packets, "tx_flow_control_xoff_packets"),
	HW_XSTAT_NAME(rx_xoff_packets, "rx_flow_control_xoff_packets"),
};

#define NGBE_NB_HW_STATS (sizeof(rte_ngbe_stats_strings) / \
			   sizeof(rte_ngbe_stats_strings[0]))

/* Per-queue statistics */
#define QP_XSTAT(m) {#m, offsetof(struct ngbe_hw_stats, qp[0].m)}
static const struct rte_ngbe_xstats_name_off rte_ngbe_qp_strings[] = {
	QP_XSTAT(rx_qp_packets),
	QP_XSTAT(tx_qp_packets),
	QP_XSTAT(rx_qp_bytes),
	QP_XSTAT(tx_qp_bytes),
	QP_XSTAT(rx_qp_mc_packets),
};

#define NGBE_NB_QP_STATS (sizeof(rte_ngbe_qp_strings) / \
			   sizeof(rte_ngbe_qp_strings[0]))

static inline int32_t
ngbe_pf_reset_hw(struct ngbe_hw *hw)
{
	uint32_t ctrl_ext;
	int32_t status;

	status = hw->mac.reset_hw(hw);

	ctrl_ext = rd32(hw, NGBE_PORTCTL);
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= NGBE_PORTCTL_RSTDONE;
	wr32(hw, NGBE_PORTCTL, ctrl_ext);
	ngbe_flush(hw);

	if (status == NGBE_ERR_SFP_NOT_PRESENT)
		status = 0;
	return status;
}

static inline void
ngbe_enable_intr(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	wr32(hw, NGBE_IENMISC, intr->mask_misc);
	wr32(hw, NGBE_IMC(0), intr->mask & BIT_MASK32);
	ngbe_flush(hw);
}

static void
ngbe_disable_intr(struct ngbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	wr32(hw, NGBE_IMS(0), NGBE_IMS_MASK);
	ngbe_flush(hw);
}

/*
 * Ensure that all locks are released before first NVM or PHY access
 */
static void
ngbe_swfw_lock_reset(struct ngbe_hw *hw)
{
	uint16_t mask;

	/*
	 * These ones are more tricky since they are common to all ports; but
	 * swfw_sync retries last long enough (1s) to be almost sure that if
	 * lock can not be taken it is due to an improper lock of the
	 * semaphore.
	 */
	mask = NGBE_MNGSEM_SWPHY |
	       NGBE_MNGSEM_SWMBX |
	       NGBE_MNGSEM_SWFLASH;
	if (hw->mac.acquire_swfw_sync(hw, mask) < 0)
		PMD_DRV_LOG(DEBUG, "SWFW common locks released");

	hw->mac.release_swfw_sync(hw, mask);
}

static int
eth_ngbe_dev_init(struct rte_eth_dev *eth_dev, void *init_params __rte_unused)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct ngbe_hw *hw = ngbe_dev_hw(eth_dev);
	struct ngbe_vfta *shadow_vfta = NGBE_DEV_VFTA(eth_dev);
	struct ngbe_hwstrip *hwstrip = NGBE_DEV_HWSTRIP(eth_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	const struct rte_memzone *mz;
	uint32_t ctrl_ext;
	u32 led_conf = 0;
	int err, ret;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &ngbe_eth_dev_ops;
	eth_dev->rx_queue_count       = ngbe_dev_rx_queue_count;
	eth_dev->rx_descriptor_status = ngbe_dev_rx_descriptor_status;
	eth_dev->tx_descriptor_status = ngbe_dev_tx_descriptor_status;
	eth_dev->rx_pkt_burst = &ngbe_recv_pkts;
	eth_dev->tx_pkt_burst = &ngbe_xmit_pkts;
	eth_dev->tx_pkt_prepare = &ngbe_prep_pkts;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * Rx and Tx function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		struct ngbe_tx_queue *txq;
		/* Tx queue function in primary, set by last queue initialized
		 * Tx queue may not initialized by primary process
		 */
		if (eth_dev->data->tx_queues) {
			uint16_t nb_tx_queues = eth_dev->data->nb_tx_queues;
			txq = eth_dev->data->tx_queues[nb_tx_queues - 1];
			ngbe_set_tx_function(eth_dev, txq);
		} else {
			/* Use default Tx function if we get here */
			PMD_INIT_LOG(NOTICE,
				"No Tx queues configured yet. Using default Tx function.");
		}

		ngbe_set_rx_function(eth_dev);

		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	/* Vendor and Device ID need to be set before init of shared code */
	hw->back = pci_dev;
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	if (pci_dev->id.subsystem_vendor_id == PCI_VENDOR_ID_WANGXUN) {
		hw->sub_system_id = pci_dev->id.subsystem_device_id;
	} else {
		u32 ssid;

		ssid = ngbe_flash_read_dword(hw, 0xFFFDC);
		if (ssid == 0x1) {
			PMD_INIT_LOG(ERR,
				"Read of internal subsystem device id failed\n");
			return -ENODEV;
		}
		hw->sub_system_id = (u16)ssid >> 8 | (u16)ssid << 8;
	}
	ngbe_map_device_id(hw);

	/* Reserve memory for interrupt status block */
	mz = rte_eth_dma_zone_reserve(eth_dev, "ngbe_driver", -1,
		NGBE_ISB_SIZE, NGBE_ALIGN, SOCKET_ID_ANY);
	if (mz == NULL)
		return -ENOMEM;

	hw->isb_dma = TMZ_PADDR(mz);
	hw->isb_mem = TMZ_VADDR(mz);

	/* Initialize the shared code (base driver) */
	err = ngbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", err);
		return -EIO;
	}

	/* Unlock any pending hardware semaphore */
	ngbe_swfw_lock_reset(hw);

	/* Get Hardware Flow Control setting */
	hw->fc.requested_mode = ngbe_fc_full;
	hw->fc.current_mode = ngbe_fc_full;
	hw->fc.pause_time = NGBE_FC_PAUSE_TIME;
	hw->fc.low_water = NGBE_FC_XON_LOTH;
	hw->fc.high_water = NGBE_FC_XOFF_HITH;
	hw->fc.send_xon = 1;

	err = hw->rom.init_params(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM init failed: %d", err);
		return -EIO;
	}

	/* Make sure we have a good EEPROM before we read from it */
	err = hw->rom.validate_checksum(hw, NULL);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "The EEPROM checksum is not valid: %d", err);
		return -EIO;
	}

	err = hw->phy.led_oem_chk(hw, &led_conf);
	if (err == 0)
		hw->led_conf = led_conf;
	else
		hw->led_conf = 0xFFFF;

	err = hw->mac.init_hw(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Hardware Initialization Failure: %d", err);
		return -EIO;
	}

	/* Reset the hw statistics */
	ngbe_dev_stats_reset(eth_dev);

	/* disable interrupt */
	ngbe_disable_intr(hw);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ngbe", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* Allocate memory for storing hash filter MAC addresses */
	eth_dev->data->hash_mac_addrs = rte_zmalloc("ngbe",
			RTE_ETHER_ADDR_LEN * NGBE_VMDQ_NUM_UC_MAC, 0);
	if (eth_dev->data->hash_mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * NGBE_VMDQ_NUM_UC_MAC);
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		return -ENOMEM;
	}

	/* initialize the vfta */
	memset(shadow_vfta, 0, sizeof(*shadow_vfta));

	/* initialize the hw strip bitmap*/
	memset(hwstrip, 0, sizeof(*hwstrip));

	/* initialize PF if max_vfs not zero */
	ret = ngbe_pf_host_init(eth_dev);
	if (ret) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		rte_free(eth_dev->data->hash_mac_addrs);
		eth_dev->data->hash_mac_addrs = NULL;
		return ret;
	}

	ctrl_ext = rd32(hw, NGBE_PORTCTL);
	/* let hardware know driver is loaded */
	ctrl_ext |= NGBE_PORTCTL_DRVLOAD;
	/* Set PF Reset Done bit so PF/VF Mail Ops can work */
	ctrl_ext |= NGBE_PORTCTL_RSTDONE;
	wr32(hw, NGBE_PORTCTL, ctrl_ext);
	ngbe_flush(hw);

	PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d",
			(int)hw->mac.type, (int)hw->phy.type);

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	rte_intr_callback_register(intr_handle,
				   ngbe_dev_interrupt_handler, eth_dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* enable support intr */
	ngbe_enable_intr(eth_dev);

	return 0;
}

static int
eth_ngbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ngbe_dev_close(eth_dev);

	return 0;
}

static int
eth_ngbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_create(&pci_dev->device, pci_dev->device.name,
			sizeof(struct ngbe_adapter),
			eth_dev_pci_specific_init, pci_dev,
			eth_ngbe_dev_init, NULL);
}

static int eth_ngbe_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *ethdev;

	ethdev = rte_eth_dev_allocated(pci_dev->device.name);
	if (ethdev == NULL)
		return 0;

	return rte_eth_dev_destroy(ethdev, eth_ngbe_dev_uninit);
}

static struct rte_pci_driver rte_ngbe_pmd = {
	.id_table = pci_id_ngbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = eth_ngbe_pci_probe,
	.remove = eth_ngbe_pci_remove,
};

static int
ngbe_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_vfta *shadow_vfta = NGBE_DEV_VFTA(dev);
	uint32_t vfta;
	uint32_t vid_idx;
	uint32_t vid_bit;

	vid_idx = (uint32_t)((vlan_id >> 5) & 0x7F);
	vid_bit = (uint32_t)(1 << (vlan_id & 0x1F));
	vfta = rd32(hw, NGBE_VLANTBL(vid_idx));
	if (on)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;
	wr32(hw, NGBE_VLANTBL(vid_idx), vfta);

	/* update local VFTA copy */
	shadow_vfta->vfta[vid_idx] = vfta;

	return 0;
}

static void
ngbe_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_rx_queue *rxq;
	bool restart;
	uint32_t rxcfg, rxbal, rxbah;

	if (on)
		ngbe_vlan_hw_strip_enable(dev, queue);
	else
		ngbe_vlan_hw_strip_disable(dev, queue);

	rxq = dev->data->rx_queues[queue];
	rxbal = rd32(hw, NGBE_RXBAL(rxq->reg_idx));
	rxbah = rd32(hw, NGBE_RXBAH(rxq->reg_idx));
	rxcfg = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) {
		restart = (rxcfg & NGBE_RXCFG_ENA) &&
			!(rxcfg & NGBE_RXCFG_VLAN);
		rxcfg |= NGBE_RXCFG_VLAN;
	} else {
		restart = (rxcfg & NGBE_RXCFG_ENA) &&
			(rxcfg & NGBE_RXCFG_VLAN);
		rxcfg &= ~NGBE_RXCFG_VLAN;
	}
	rxcfg &= ~NGBE_RXCFG_ENA;

	if (restart) {
		/* set vlan strip for ring */
		ngbe_dev_rx_queue_stop(dev, queue);
		wr32(hw, NGBE_RXBAL(rxq->reg_idx), rxbal);
		wr32(hw, NGBE_RXBAH(rxq->reg_idx), rxbah);
		wr32(hw, NGBE_RXCFG(rxq->reg_idx), rxcfg);
		ngbe_dev_rx_queue_start(dev, queue);
	}
}

static int
ngbe_vlan_tpid_set(struct rte_eth_dev *dev,
		    enum rte_vlan_type vlan_type,
		    uint16_t tpid)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int ret = 0;
	uint32_t portctrl, vlan_ext, qinq;

	portctrl = rd32(hw, NGBE_PORTCTL);

	vlan_ext = (portctrl & NGBE_PORTCTL_VLANEXT);
	qinq = vlan_ext && (portctrl & NGBE_PORTCTL_QINQ);
	switch (vlan_type) {
	case RTE_ETH_VLAN_TYPE_INNER:
		if (vlan_ext) {
			wr32m(hw, NGBE_VLANCTL,
				NGBE_VLANCTL_TPID_MASK,
				NGBE_VLANCTL_TPID(tpid));
			wr32m(hw, NGBE_DMATXCTRL,
				NGBE_DMATXCTRL_TPID_MASK,
				NGBE_DMATXCTRL_TPID(tpid));
		} else {
			ret = -ENOTSUP;
			PMD_DRV_LOG(ERR,
				"Inner type is not supported by single VLAN");
		}

		if (qinq) {
			wr32m(hw, NGBE_TAGTPID(0),
				NGBE_TAGTPID_LSB_MASK,
				NGBE_TAGTPID_LSB(tpid));
		}
		break;
	case RTE_ETH_VLAN_TYPE_OUTER:
		if (vlan_ext) {
			/* Only the high 16-bits is valid */
			wr32m(hw, NGBE_EXTAG,
				NGBE_EXTAG_VLAN_MASK,
				NGBE_EXTAG_VLAN(tpid));
		} else {
			wr32m(hw, NGBE_VLANCTL,
				NGBE_VLANCTL_TPID_MASK,
				NGBE_VLANCTL_TPID(tpid));
			wr32m(hw, NGBE_DMATXCTRL,
				NGBE_DMATXCTRL_TPID_MASK,
				NGBE_DMATXCTRL_TPID(tpid));
		}

		if (qinq) {
			wr32m(hw, NGBE_TAGTPID(0),
				NGBE_TAGTPID_MSB_MASK,
				NGBE_TAGTPID_MSB(tpid));
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported VLAN type %d", vlan_type);
		return -EINVAL;
	}

	return ret;
}

void
ngbe_vlan_hw_filter_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t vlnctrl;

	PMD_INIT_FUNC_TRACE();

	/* Filter Table Disable */
	vlnctrl = rd32(hw, NGBE_VLANCTL);
	vlnctrl &= ~NGBE_VLANCTL_VFE;
	wr32(hw, NGBE_VLANCTL, vlnctrl);
}

void
ngbe_vlan_hw_filter_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_vfta *shadow_vfta = NGBE_DEV_VFTA(dev);
	uint32_t vlnctrl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	/* Filter Table Enable */
	vlnctrl = rd32(hw, NGBE_VLANCTL);
	vlnctrl &= ~NGBE_VLANCTL_CFIENA;
	vlnctrl |= NGBE_VLANCTL_VFE;
	wr32(hw, NGBE_VLANCTL, vlnctrl);

	/* write whatever is in local vfta copy */
	for (i = 0; i < NGBE_VFTA_SIZE; i++)
		wr32(hw, NGBE_VLANTBL(i), shadow_vfta->vfta[i]);
}

void
ngbe_vlan_hw_strip_bitmap_set(struct rte_eth_dev *dev, uint16_t queue, bool on)
{
	struct ngbe_hwstrip *hwstrip = NGBE_DEV_HWSTRIP(dev);
	struct ngbe_rx_queue *rxq;

	if (queue >= NGBE_MAX_RX_QUEUE_NUM)
		return;

	if (on)
		NGBE_SET_HWSTRIP(hwstrip, queue);
	else
		NGBE_CLEAR_HWSTRIP(hwstrip, queue);

	if (queue >= dev->data->nb_rx_queues)
		return;

	rxq = dev->data->rx_queues[queue];

	if (on) {
		rxq->vlan_flags = RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		rxq->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	} else {
		rxq->vlan_flags = RTE_MBUF_F_RX_VLAN;
		rxq->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	}
}

static void
ngbe_vlan_hw_strip_disable(struct rte_eth_dev *dev, uint16_t queue)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, NGBE_RXCFG(queue));
	ctrl &= ~NGBE_RXCFG_VLAN;
	wr32(hw, NGBE_RXCFG(queue), ctrl);

	/* record those setting for HW strip per queue */
	ngbe_vlan_hw_strip_bitmap_set(dev, queue, 0);
}

static void
ngbe_vlan_hw_strip_enable(struct rte_eth_dev *dev, uint16_t queue)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, NGBE_RXCFG(queue));
	ctrl |= NGBE_RXCFG_VLAN;
	wr32(hw, NGBE_RXCFG(queue), ctrl);

	/* record those setting for HW strip per queue */
	ngbe_vlan_hw_strip_bitmap_set(dev, queue, 1);
}

static void
ngbe_vlan_hw_extend_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, NGBE_PORTCTL);
	ctrl &= ~NGBE_PORTCTL_VLANEXT;
	ctrl &= ~NGBE_PORTCTL_QINQ;
	wr32(hw, NGBE_PORTCTL, ctrl);
}

static void
ngbe_vlan_hw_extend_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl  = rd32(hw, NGBE_PORTCTL);
	ctrl |= NGBE_PORTCTL_VLANEXT | NGBE_PORTCTL_QINQ;
	wr32(hw, NGBE_PORTCTL, ctrl);
}

static void
ngbe_qinq_hw_strip_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl = rd32(hw, NGBE_PORTCTL);
	ctrl &= ~NGBE_PORTCTL_QINQ;
	wr32(hw, NGBE_PORTCTL, ctrl);
}

static void
ngbe_qinq_hw_strip_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t ctrl;

	PMD_INIT_FUNC_TRACE();

	ctrl  = rd32(hw, NGBE_PORTCTL);
	ctrl |= NGBE_PORTCTL_QINQ | NGBE_PORTCTL_VLANEXT;
	wr32(hw, NGBE_PORTCTL, ctrl);
}

void
ngbe_vlan_hw_strip_config(struct rte_eth_dev *dev)
{
	struct ngbe_rx_queue *rxq;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			ngbe_vlan_hw_strip_enable(dev, i);
		else
			ngbe_vlan_hw_strip_disable(dev, i);
	}
}

void
ngbe_config_vlan_strip_on_all_queues(struct rte_eth_dev *dev, int mask)
{
	uint16_t i;
	struct rte_eth_rxmode *rxmode;
	struct ngbe_rx_queue *rxq;

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		rxmode = &dev->data->dev_conf.rxmode;
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				rxq->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
			}
		else
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				rxq->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
			}
	}
}

static int
ngbe_vlan_offload_config(struct rte_eth_dev *dev, int mask)
{
	struct rte_eth_rxmode *rxmode;
	rxmode = &dev->data->dev_conf.rxmode;

	if (mask & RTE_ETH_VLAN_STRIP_MASK)
		ngbe_vlan_hw_strip_config(dev);

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
			ngbe_vlan_hw_filter_enable(dev);
		else
			ngbe_vlan_hw_filter_disable(dev);
	}

	if (mask & RTE_ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND)
			ngbe_vlan_hw_extend_enable(dev);
		else
			ngbe_vlan_hw_extend_disable(dev);
	}

	if (mask & RTE_ETH_QINQ_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP)
			ngbe_qinq_hw_strip_enable(dev);
		else
			ngbe_qinq_hw_strip_disable(dev);
	}

	return 0;
}

static int
ngbe_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	ngbe_config_vlan_strip_on_all_queues(dev, mask);

	ngbe_vlan_offload_config(dev, mask);

	return 0;
}

static int
ngbe_dev_configure(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* set flag to update link status after init */
	intr->flags |= NGBE_FLAG_NEED_LINK_UPDATE;

	/*
	 * Initialize to TRUE. If any of Rx queues doesn't meet the bulk
	 * allocation Rx preconditions we will reset it.
	 */
	adapter->rx_bulk_alloc_allowed = true;

	return 0;
}

static void
ngbe_dev_phy_intr_setup(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	wr32(hw, NGBE_GPIODIR, NGBE_GPIODIR_DDR(1));
	wr32(hw, NGBE_GPIOINTEN, NGBE_GPIOINTEN_INT(3));
	wr32(hw, NGBE_GPIOINTTYPE, NGBE_GPIOINTTYPE_LEVEL(0));
	if (hw->phy.type == ngbe_phy_yt8521s_sfi)
		wr32(hw, NGBE_GPIOINTPOL, NGBE_GPIOINTPOL_ACT(0));
	else
		wr32(hw, NGBE_GPIOINTPOL, NGBE_GPIOINTPOL_ACT(3));

	intr->mask_misc |= NGBE_ICRMISC_GPIO | NGBE_ICRMISC_HEAT;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
ngbe_dev_start(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	int err;
	bool link_up = false, negotiate = false;
	uint32_t speed = 0;
	uint32_t allowed_speeds = 0;
	int mask = 0;
	int status;
	uint32_t *link_speeds;

	PMD_INIT_FUNC_TRACE();

	/* Stop the link setup handler before resetting the HW. */
	rte_eal_alarm_cancel(ngbe_dev_setup_link_alarm_handler, dev);

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	/* stop adapter */
	hw->adapter_stopped = 0;

	/* reinitialize adapter, this calls reset and start */
	hw->nb_rx_queues = dev->data->nb_rx_queues;
	hw->nb_tx_queues = dev->data->nb_tx_queues;
	status = ngbe_pf_reset_hw(hw);
	if (status != 0)
		return -1;
	hw->mac.start_hw(hw);
	hw->mac.get_link_status = true;

	ngbe_set_pcie_master(hw, true);

	/* configure PF module if SRIOV enabled */
	ngbe_pf_host_configure(dev);

	ngbe_dev_phy_intr_setup(dev);

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						   dev->data->nb_rx_queues)) {
			PMD_INIT_LOG(ERR,
				     "Failed to allocate %d rx_queues intr_vec",
				     dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* configure MSI-X for sleep until Rx interrupt */
	ngbe_configure_msix(dev);

	/* initialize transmission unit */
	ngbe_dev_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = ngbe_dev_rx_init(dev);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Unable to initialize Rx hardware");
		goto error;
	}

	mask = RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK |
		RTE_ETH_VLAN_EXTEND_MASK;
	err = ngbe_vlan_offload_config(dev, mask);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Unable to set VLAN offload");
		goto error;
	}

	hw->mac.setup_pba(hw);
	ngbe_configure_port(dev);

	err = ngbe_dev_rxtx_start(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Unable to start rxtx queues");
		goto error;
	}

	/* Skip link setup if loopback mode is enabled. */
	if (hw->is_pf && dev->data->dev_conf.lpbk_mode)
		goto skip_link_setup;

	err = hw->mac.check_link(hw, &speed, &link_up, 0);
	if (err != 0)
		goto error;
	dev->data->dev_link.link_status = link_up;

	link_speeds = &dev->data->dev_conf.link_speeds;
	if (*link_speeds == RTE_ETH_LINK_SPEED_AUTONEG)
		negotiate = true;

	err = hw->mac.get_link_capabilities(hw, &speed, &negotiate);
	if (err != 0)
		goto error;

	allowed_speeds = 0;
	if (hw->mac.default_speeds & NGBE_LINK_SPEED_1GB_FULL)
		allowed_speeds |= RTE_ETH_LINK_SPEED_1G;
	if (hw->mac.default_speeds & NGBE_LINK_SPEED_100M_FULL)
		allowed_speeds |= RTE_ETH_LINK_SPEED_100M;
	if (hw->mac.default_speeds & NGBE_LINK_SPEED_10M_FULL)
		allowed_speeds |= RTE_ETH_LINK_SPEED_10M;

	if (((*link_speeds) >> 1) & ~(allowed_speeds >> 1)) {
		PMD_INIT_LOG(ERR, "Invalid link setting");
		goto error;
	}

	speed = 0x0;
	if (*link_speeds == RTE_ETH_LINK_SPEED_AUTONEG) {
		speed = hw->mac.default_speeds;
	} else {
		if (*link_speeds & RTE_ETH_LINK_SPEED_1G)
			speed |= NGBE_LINK_SPEED_1GB_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_100M)
			speed |= NGBE_LINK_SPEED_100M_FULL;
		if (*link_speeds & RTE_ETH_LINK_SPEED_10M)
			speed |= NGBE_LINK_SPEED_10M_FULL;
	}

	err = hw->phy.init_hw(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "PHY init failed");
		goto error;
	}
	err = hw->mac.setup_link(hw, speed, link_up);
	if (err != 0)
		goto error;

skip_link_setup:

	if (rte_intr_allow_others(intr_handle)) {
		ngbe_dev_misc_interrupt_setup(dev);
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			ngbe_dev_lsc_interrupt_setup(dev, TRUE);
		else
			ngbe_dev_lsc_interrupt_setup(dev, FALSE);
		ngbe_dev_macsec_interrupt_setup(dev);
		ngbe_set_ivar_map(hw, -1, 1, NGBE_MISC_VEC_ID);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     ngbe_dev_interrupt_handler, dev);
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO,
				     "LSC won't enable because of no intr multiplex");
	}

	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq != 0 &&
	    rte_intr_dp_is_en(intr_handle))
		ngbe_dev_rxq_interrupt_setup(dev);

	/* enable UIO/VFIO intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	/* resume enabled intr since HW reset */
	ngbe_enable_intr(dev);

	if (hw->gpio_ctl) {
		/* gpio0 is used to power on/off control*/
		wr32(hw, NGBE_GPIODATA, 0);
	}

	/*
	 * Update link status right before return, because it may
	 * start link configuration process in a separate thread.
	 */
	ngbe_dev_link_update(dev, 0);

	ngbe_read_stats_registers(hw, hw_stats);
	hw->offset_loaded = 1;

	return 0;

error:
	PMD_INIT_LOG(ERR, "failure in dev start: %d", err);
	ngbe_dev_clear_queues(dev);
	return -EIO;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int
ngbe_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_vf_info *vfinfo = *NGBE_DEV_VFDATA(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int vf;

	if (hw->adapter_stopped)
		return 0;

	PMD_INIT_FUNC_TRACE();

	rte_eal_alarm_cancel(ngbe_dev_setup_link_alarm_handler, dev);

	if (hw->gpio_ctl) {
		/* gpio0 is used to power on/off control*/
		wr32(hw, NGBE_GPIODATA, NGBE_GPIOBIT_0);
	}

	/* disable interrupts */
	ngbe_disable_intr(hw);

	/* reset the NIC */
	ngbe_pf_reset_hw(hw);
	hw->adapter_stopped = 0;

	/* stop adapter */
	ngbe_stop_hw(hw);

	for (vf = 0; vfinfo != NULL && vf < pci_dev->max_vfs; vf++)
		vfinfo[vf].clear_to_send = false;

	hw->phy.set_phy_power(hw, false);

	ngbe_dev_clear_queues(dev);

	/* Clear stored conf */
	dev->data->scattered_rx = 0;

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   ngbe_dev_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);

	ngbe_set_pcie_master(hw, true);

	adapter->rss_reta_updated = 0;

	hw->adapter_stopped = true;
	dev->data->dev_started = 0;

	return 0;
}

/*
 * Set device link up: power on.
 */
static int
ngbe_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	hw->phy.set_phy_power(hw, true);

	return 0;
}

/*
 * Set device link down: power off.
 */
static int
ngbe_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	hw->phy.set_phy_power(hw, false);

	return 0;
}

/*
 * Reset and stop device.
 */
static int
ngbe_dev_close(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int retries = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	ngbe_pf_reset_hw(hw);

	ngbe_dev_stop(dev);

	ngbe_dev_free_queues(dev);

	ngbe_set_pcie_master(hw, false);

	/* reprogram the RAR[0] in case user changed it. */
	ngbe_set_rar(hw, 0, hw->mac.addr, 0, true);

	/* Unlock any pending hardware semaphore */
	ngbe_swfw_lock_reset(hw);

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	do {
		ret = rte_intr_callback_unregister(intr_handle,
				ngbe_dev_interrupt_handler, dev);
		if (ret >= 0 || ret == -ENOENT) {
			break;
		} else if (ret != -EAGAIN) {
			PMD_INIT_LOG(ERR,
				"intr callback unregister failed: %d",
				ret);
		}
		rte_delay_ms(100);
	} while (retries++ < (10 + NGBE_LINK_UP_TIME));

	/* uninitialize PF if max_vfs not zero */
	ngbe_pf_host_uninit(dev);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	rte_free(dev->data->hash_mac_addrs);
	dev->data->hash_mac_addrs = NULL;

	return ret;
}

/*
 * Reset PF device.
 */
static int
ngbe_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	/* When a DPDK PMD PF begin to reset PF port, it should notify all
	 * its VF to make them align with it. The detailed notification
	 * mechanism is PMD specific. As to ngbe PF, it is rather complex.
	 * To avoid unexpected behavior in VF, currently reset of PF with
	 * SR-IOV activation is not supported. It might be supported later.
	 */
	if (dev->data->sriov.active)
		return -ENOTSUP;

	ret = eth_ngbe_dev_uninit(dev);
	if (ret != 0)
		return ret;

	ret = eth_ngbe_dev_init(dev, NULL);

	return ret;
}

#define UPDATE_QP_COUNTER_32bit(reg, last_counter, counter)     \
	{                                                       \
		uint32_t current_counter = rd32(hw, reg);       \
		if (current_counter < last_counter)             \
			current_counter += 0x100000000LL;       \
		if (!hw->offset_loaded)                         \
			last_counter = current_counter;         \
		counter = current_counter - last_counter;       \
		counter &= 0xFFFFFFFFLL;                        \
	}

#define UPDATE_QP_COUNTER_36bit(reg_lsb, reg_msb, last_counter, counter) \
	{                                                                \
		uint64_t current_counter_lsb = rd32(hw, reg_lsb);        \
		uint64_t current_counter_msb = rd32(hw, reg_msb);        \
		uint64_t current_counter = (current_counter_msb << 32) | \
			current_counter_lsb;                             \
		if (current_counter < last_counter)                      \
			current_counter += 0x1000000000LL;               \
		if (!hw->offset_loaded)                                  \
			last_counter = current_counter;                  \
		counter = current_counter - last_counter;                \
		counter &= 0xFFFFFFFFFLL;                                \
	}

void
ngbe_read_stats_registers(struct ngbe_hw *hw,
			   struct ngbe_hw_stats *hw_stats)
{
	unsigned int i;

	/* QP Stats */
	for (i = 0; i < hw->nb_rx_queues; i++) {
		UPDATE_QP_COUNTER_32bit(NGBE_QPRXPKT(i),
			hw->qp_last[i].rx_qp_packets,
			hw_stats->qp[i].rx_qp_packets);
		UPDATE_QP_COUNTER_36bit(NGBE_QPRXOCTL(i), NGBE_QPRXOCTH(i),
			hw->qp_last[i].rx_qp_bytes,
			hw_stats->qp[i].rx_qp_bytes);
		UPDATE_QP_COUNTER_32bit(NGBE_QPRXMPKT(i),
			hw->qp_last[i].rx_qp_mc_packets,
			hw_stats->qp[i].rx_qp_mc_packets);
		UPDATE_QP_COUNTER_32bit(NGBE_QPRXBPKT(i),
			hw->qp_last[i].rx_qp_bc_packets,
			hw_stats->qp[i].rx_qp_bc_packets);
	}

	for (i = 0; i < hw->nb_tx_queues; i++) {
		UPDATE_QP_COUNTER_32bit(NGBE_QPTXPKT(i),
			hw->qp_last[i].tx_qp_packets,
			hw_stats->qp[i].tx_qp_packets);
		UPDATE_QP_COUNTER_36bit(NGBE_QPTXOCTL(i), NGBE_QPTXOCTH(i),
			hw->qp_last[i].tx_qp_bytes,
			hw_stats->qp[i].tx_qp_bytes);
		UPDATE_QP_COUNTER_32bit(NGBE_QPTXMPKT(i),
			hw->qp_last[i].tx_qp_mc_packets,
			hw_stats->qp[i].tx_qp_mc_packets);
		UPDATE_QP_COUNTER_32bit(NGBE_QPTXBPKT(i),
			hw->qp_last[i].tx_qp_bc_packets,
			hw_stats->qp[i].tx_qp_bc_packets);
	}

	/* PB Stats */
	hw_stats->rx_up_dropped += rd32(hw, NGBE_PBRXMISS);
	hw_stats->rdb_pkt_cnt += rd32(hw, NGBE_PBRXPKT);
	hw_stats->rdb_repli_cnt += rd32(hw, NGBE_PBRXREP);
	hw_stats->rdb_drp_cnt += rd32(hw, NGBE_PBRXDROP);
	hw_stats->tx_xoff_packets += rd32(hw, NGBE_PBTXLNKXOFF);
	hw_stats->tx_xon_packets += rd32(hw, NGBE_PBTXLNKXON);

	hw_stats->rx_xon_packets += rd32(hw, NGBE_PBRXLNKXON);
	hw_stats->rx_xoff_packets += rd32(hw, NGBE_PBRXLNKXOFF);

	/* DMA Stats */
	hw_stats->rx_dma_drop += rd32(hw, NGBE_DMARXDROP);
	hw_stats->tx_dma_drop += rd32(hw, NGBE_DMATXDROP);
	hw_stats->tx_secdrp_packets += rd32(hw, NGBE_DMATXSECDROP);
	hw_stats->rx_packets += rd32(hw, NGBE_DMARXPKT);
	hw_stats->tx_packets += rd32(hw, NGBE_DMATXPKT);
	hw_stats->rx_bytes += rd64(hw, NGBE_DMARXOCTL);
	hw_stats->tx_bytes += rd64(hw, NGBE_DMATXOCTL);

	/* MAC Stats */
	hw_stats->rx_crc_errors += rd64(hw, NGBE_MACRXERRCRCL);
	hw_stats->rx_multicast_packets += rd64(hw, NGBE_MACRXMPKTL);
	hw_stats->tx_multicast_packets += rd64(hw, NGBE_MACTXMPKTL);

	hw_stats->rx_total_packets += rd64(hw, NGBE_MACRXPKTL);
	hw_stats->tx_total_packets += rd64(hw, NGBE_MACTXPKTL);
	hw_stats->rx_total_bytes += rd64(hw, NGBE_MACRXGBOCTL);

	hw_stats->rx_broadcast_packets += rd64(hw, NGBE_MACRXOCTL);
	hw_stats->tx_broadcast_packets += rd32(hw, NGBE_MACTXOCTL);

	hw_stats->rx_size_64_packets += rd64(hw, NGBE_MACRX1TO64L);
	hw_stats->rx_size_65_to_127_packets += rd64(hw, NGBE_MACRX65TO127L);
	hw_stats->rx_size_128_to_255_packets += rd64(hw, NGBE_MACRX128TO255L);
	hw_stats->rx_size_256_to_511_packets += rd64(hw, NGBE_MACRX256TO511L);
	hw_stats->rx_size_512_to_1023_packets +=
			rd64(hw, NGBE_MACRX512TO1023L);
	hw_stats->rx_size_1024_to_max_packets +=
			rd64(hw, NGBE_MACRX1024TOMAXL);
	hw_stats->tx_size_64_packets += rd64(hw, NGBE_MACTX1TO64L);
	hw_stats->tx_size_65_to_127_packets += rd64(hw, NGBE_MACTX65TO127L);
	hw_stats->tx_size_128_to_255_packets += rd64(hw, NGBE_MACTX128TO255L);
	hw_stats->tx_size_256_to_511_packets += rd64(hw, NGBE_MACTX256TO511L);
	hw_stats->tx_size_512_to_1023_packets +=
			rd64(hw, NGBE_MACTX512TO1023L);
	hw_stats->tx_size_1024_to_max_packets +=
			rd64(hw, NGBE_MACTX1024TOMAXL);

	hw_stats->rx_undersize_errors += rd64(hw, NGBE_MACRXERRLENL);
	hw_stats->rx_oversize_cnt += rd32(hw, NGBE_MACRXOVERSIZE);
	hw_stats->rx_jabber_errors += rd32(hw, NGBE_MACRXJABBER);

	/* MNG Stats */
	hw_stats->mng_bmc2host_packets = rd32(hw, NGBE_MNGBMC2OS);
	hw_stats->mng_host2bmc_packets = rd32(hw, NGBE_MNGOS2BMC);
	hw_stats->rx_management_packets = rd32(hw, NGBE_DMARXMNG);
	hw_stats->tx_management_packets = rd32(hw, NGBE_DMATXMNG);

	/* MACsec Stats */
	hw_stats->tx_macsec_pkts_untagged += rd32(hw, NGBE_LSECTX_UTPKT);
	hw_stats->tx_macsec_pkts_encrypted +=
			rd32(hw, NGBE_LSECTX_ENCPKT);
	hw_stats->tx_macsec_pkts_protected +=
			rd32(hw, NGBE_LSECTX_PROTPKT);
	hw_stats->tx_macsec_octets_encrypted +=
			rd32(hw, NGBE_LSECTX_ENCOCT);
	hw_stats->tx_macsec_octets_protected +=
			rd32(hw, NGBE_LSECTX_PROTOCT);
	hw_stats->rx_macsec_pkts_untagged += rd32(hw, NGBE_LSECRX_UTPKT);
	hw_stats->rx_macsec_pkts_badtag += rd32(hw, NGBE_LSECRX_BTPKT);
	hw_stats->rx_macsec_pkts_nosci += rd32(hw, NGBE_LSECRX_NOSCIPKT);
	hw_stats->rx_macsec_pkts_unknownsci += rd32(hw, NGBE_LSECRX_UNSCIPKT);
	hw_stats->rx_macsec_octets_decrypted += rd32(hw, NGBE_LSECRX_DECOCT);
	hw_stats->rx_macsec_octets_validated += rd32(hw, NGBE_LSECRX_VLDOCT);
	hw_stats->rx_macsec_sc_pkts_unchecked +=
			rd32(hw, NGBE_LSECRX_UNCHKPKT);
	hw_stats->rx_macsec_sc_pkts_delayed += rd32(hw, NGBE_LSECRX_DLYPKT);
	hw_stats->rx_macsec_sc_pkts_late += rd32(hw, NGBE_LSECRX_LATEPKT);
	for (i = 0; i < 2; i++) {
		hw_stats->rx_macsec_sa_pkts_ok +=
			rd32(hw, NGBE_LSECRX_OKPKT(i));
		hw_stats->rx_macsec_sa_pkts_invalid +=
			rd32(hw, NGBE_LSECRX_INVPKT(i));
		hw_stats->rx_macsec_sa_pkts_notvalid +=
			rd32(hw, NGBE_LSECRX_BADPKT(i));
	}
	for (i = 0; i < 4; i++) {
		hw_stats->rx_macsec_sa_pkts_unusedsa +=
			rd32(hw, NGBE_LSECRX_INVSAPKT(i));
		hw_stats->rx_macsec_sa_pkts_notusingsa +=
			rd32(hw, NGBE_LSECRX_BADSAPKT(i));
	}
	hw_stats->rx_total_missed_packets =
			hw_stats->rx_up_dropped;
}

static int
ngbe_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);
	struct ngbe_stat_mappings *stat_mappings =
			NGBE_DEV_STAT_MAPPINGS(dev);
	uint32_t i, j;

	ngbe_read_stats_registers(hw, hw_stats);

	if (stats == NULL)
		return -EINVAL;

	/* Fill out the rte_eth_stats statistics structure */
	stats->ipackets = hw_stats->rx_packets;
	stats->ibytes = hw_stats->rx_bytes;
	stats->opackets = hw_stats->tx_packets;
	stats->obytes = hw_stats->tx_bytes;

	memset(&stats->q_ipackets, 0, sizeof(stats->q_ipackets));
	memset(&stats->q_opackets, 0, sizeof(stats->q_opackets));
	memset(&stats->q_ibytes, 0, sizeof(stats->q_ibytes));
	memset(&stats->q_obytes, 0, sizeof(stats->q_obytes));
	memset(&stats->q_errors, 0, sizeof(stats->q_errors));
	for (i = 0; i < NGBE_MAX_QP; i++) {
		uint32_t n = i / NB_QMAP_FIELDS_PER_QSM_REG;
		uint32_t offset = (i % NB_QMAP_FIELDS_PER_QSM_REG) * 8;
		uint32_t q_map;

		q_map = (stat_mappings->rqsm[n] >> offset)
				& QMAP_FIELD_RESERVED_BITS_MASK;
		j = (q_map < RTE_ETHDEV_QUEUE_STAT_CNTRS
		     ? q_map : q_map % RTE_ETHDEV_QUEUE_STAT_CNTRS);
		stats->q_ipackets[j] += hw_stats->qp[i].rx_qp_packets;
		stats->q_ibytes[j] += hw_stats->qp[i].rx_qp_bytes;

		q_map = (stat_mappings->tqsm[n] >> offset)
				& QMAP_FIELD_RESERVED_BITS_MASK;
		j = (q_map < RTE_ETHDEV_QUEUE_STAT_CNTRS
		     ? q_map : q_map % RTE_ETHDEV_QUEUE_STAT_CNTRS);
		stats->q_opackets[j] += hw_stats->qp[i].tx_qp_packets;
		stats->q_obytes[j] += hw_stats->qp[i].tx_qp_bytes;
	}

	/* Rx Errors */
	stats->imissed  = hw_stats->rx_total_missed_packets +
			  hw_stats->rx_dma_drop;
	stats->ierrors  = hw_stats->rx_crc_errors +
			  hw_stats->rx_mac_short_packet_dropped +
			  hw_stats->rx_length_errors +
			  hw_stats->rx_undersize_errors +
			  hw_stats->rdb_drp_cnt +
			  hw_stats->rx_illegal_byte_errors +
			  hw_stats->rx_error_bytes +
			  hw_stats->rx_fragment_errors;

	/* Tx Errors */
	stats->oerrors  = 0;
	return 0;
}

static int
ngbe_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);

	/* HW registers are cleared on read */
	hw->offset_loaded = 0;
	ngbe_dev_stats_get(dev, NULL);
	hw->offset_loaded = 1;

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));

	return 0;
}

/* This function calculates the number of xstats based on the current config */
static unsigned
ngbe_xstats_calc_num(struct rte_eth_dev *dev)
{
	int nb_queues = max(dev->data->nb_rx_queues, dev->data->nb_tx_queues);
	return NGBE_NB_HW_STATS +
	       NGBE_NB_QP_STATS * nb_queues;
}

static inline int
ngbe_get_name_by_id(uint32_t id, char *name, uint32_t size)
{
	int nb, st;

	/* Extended stats from ngbe_hw_stats */
	if (id < NGBE_NB_HW_STATS) {
		snprintf(name, size, "[hw]%s",
			rte_ngbe_stats_strings[id].name);
		return 0;
	}
	id -= NGBE_NB_HW_STATS;

	/* Queue Stats */
	if (id < NGBE_NB_QP_STATS * NGBE_MAX_QP) {
		nb = id / NGBE_NB_QP_STATS;
		st = id % NGBE_NB_QP_STATS;
		snprintf(name, size, "[q%u]%s", nb,
			rte_ngbe_qp_strings[st].name);
		return 0;
	}
	id -= NGBE_NB_QP_STATS * NGBE_MAX_QP;

	return -(int)(id + 1);
}

static inline int
ngbe_get_offset_by_id(uint32_t id, uint32_t *offset)
{
	int nb, st;

	/* Extended stats from ngbe_hw_stats */
	if (id < NGBE_NB_HW_STATS) {
		*offset = rte_ngbe_stats_strings[id].offset;
		return 0;
	}
	id -= NGBE_NB_HW_STATS;

	/* Queue Stats */
	if (id < NGBE_NB_QP_STATS * NGBE_MAX_QP) {
		nb = id / NGBE_NB_QP_STATS;
		st = id % NGBE_NB_QP_STATS;
		*offset = rte_ngbe_qp_strings[st].offset +
			nb * (NGBE_NB_QP_STATS * sizeof(uint64_t));
		return 0;
	}

	return -1;
}

static int ngbe_dev_xstats_get_names(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned int limit)
{
	unsigned int i, count;

	count = ngbe_xstats_calc_num(dev);
	if (xstats_names == NULL)
		return count;

	/* Note: limit >= cnt_stats checked upstream
	 * in rte_eth_xstats_names()
	 */
	limit = min(limit, count);

	/* Extended stats from ngbe_hw_stats */
	for (i = 0; i < limit; i++) {
		if (ngbe_get_name_by_id(i, xstats_names[i].name,
			sizeof(xstats_names[i].name))) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
	}

	return i;
}

static int ngbe_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
	const uint64_t *ids,
	struct rte_eth_xstat_name *xstats_names,
	unsigned int limit)
{
	unsigned int i;

	if (ids == NULL)
		return ngbe_dev_xstats_get_names(dev, xstats_names, limit);

	for (i = 0; i < limit; i++) {
		if (ngbe_get_name_by_id(ids[i], xstats_names[i].name,
				sizeof(xstats_names[i].name))) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			return -1;
		}
	}

	return i;
}

static int
ngbe_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
					 unsigned int limit)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);
	unsigned int i, count;

	ngbe_read_stats_registers(hw, hw_stats);

	/* If this is a reset xstats is NULL, and we have cleared the
	 * registers by reading them.
	 */
	count = ngbe_xstats_calc_num(dev);
	if (xstats == NULL)
		return count;

	limit = min(limit, ngbe_xstats_calc_num(dev));

	/* Extended stats from ngbe_hw_stats */
	for (i = 0; i < limit; i++) {
		uint32_t offset = 0;

		if (ngbe_get_offset_by_id(i, &offset)) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
		xstats[i].value = *(uint64_t *)(((char *)hw_stats) + offset);
		xstats[i].id = i;
	}

	return i;
}

static int
ngbe_dev_xstats_get_(struct rte_eth_dev *dev, uint64_t *values,
					 unsigned int limit)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);
	unsigned int i, count;

	ngbe_read_stats_registers(hw, hw_stats);

	/* If this is a reset xstats is NULL, and we have cleared the
	 * registers by reading them.
	 */
	count = ngbe_xstats_calc_num(dev);
	if (values == NULL)
		return count;

	limit = min(limit, ngbe_xstats_calc_num(dev));

	/* Extended stats from ngbe_hw_stats */
	for (i = 0; i < limit; i++) {
		uint32_t offset;

		if (ngbe_get_offset_by_id(i, &offset)) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
		values[i] = *(uint64_t *)(((char *)hw_stats) + offset);
	}

	return i;
}

static int
ngbe_dev_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
		uint64_t *values, unsigned int limit)
{
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);
	unsigned int i;

	if (ids == NULL)
		return ngbe_dev_xstats_get_(dev, values, limit);

	for (i = 0; i < limit; i++) {
		uint32_t offset;

		if (ngbe_get_offset_by_id(ids[i], &offset)) {
			PMD_INIT_LOG(WARNING, "id value %d isn't valid", i);
			break;
		}
		values[i] = *(uint64_t *)(((char *)hw_stats) + offset);
	}

	return i;
}

static int
ngbe_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_hw_stats *hw_stats = NGBE_DEV_STATS(dev);

	/* HW registers are cleared on read */
	hw->offset_loaded = 0;
	ngbe_read_stats_registers(hw, hw_stats);
	hw->offset_loaded = 1;

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));

	return 0;
}

static int
ngbe_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int ret;

	ret = snprintf(fw_version, fw_size, "0x%08x", hw->eeprom_id);

	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;

	return 0;
}

static int
ngbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = 15872;
	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
	dev_info->max_hash_mac_addrs = NGBE_VMDQ_NUM_UC_MAC;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->rx_queue_offload_capa = ngbe_get_rx_queue_offloads(dev);
	dev_info->rx_offload_capa = (ngbe_get_rx_port_offloads(dev) |
				     dev_info->rx_queue_offload_capa);
	dev_info->tx_queue_offload_capa = 0;
	dev_info->tx_offload_capa = ngbe_get_tx_port_offloads(dev);

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = NGBE_DEFAULT_RX_PTHRESH,
			.hthresh = NGBE_DEFAULT_RX_HTHRESH,
			.wthresh = NGBE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = NGBE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = NGBE_DEFAULT_TX_PTHRESH,
			.hthresh = NGBE_DEFAULT_TX_HTHRESH,
			.wthresh = NGBE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = NGBE_DEFAULT_TX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->hash_key_size = NGBE_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = RTE_ETH_RSS_RETA_SIZE_128;
	dev_info->flow_type_rss_offloads = NGBE_RSS_OFFLOAD_ALL;

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_100M |
				RTE_ETH_LINK_SPEED_10M;

	/* Driver-preferred Rx/Tx parameters */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = 256;
	dev_info->default_txportconf.ring_size = 256;

	return 0;
}

const uint32_t *
ngbe_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	if (dev->rx_pkt_burst == ngbe_recv_pkts ||
	    dev->rx_pkt_burst == ngbe_recv_pkts_sc_single_alloc ||
	    dev->rx_pkt_burst == ngbe_recv_pkts_sc_bulk_alloc ||
	    dev->rx_pkt_burst == ngbe_recv_pkts_bulk_alloc)
		return ngbe_get_supported_ptypes();

	return NULL;
}

static void
ngbe_dev_overheat(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	s32 temp_state;

	temp_state = hw->mac.check_overtemp(hw);
	if (!temp_state)
		return;

	if (temp_state == NGBE_ERR_UNDERTEMP) {
		PMD_DRV_LOG(CRIT, "Network adapter has been started again, "
			"since the temperature has been back to normal state.");
		wr32m(hw, NGBE_PBRXCTL, NGBE_PBRXCTL_ENA, NGBE_PBRXCTL_ENA);
		ngbe_dev_set_link_up(dev);
	} else if (temp_state == NGBE_ERR_OVERTEMP) {
		PMD_DRV_LOG(CRIT, "Network adapter has been stopped because it has over heated.");
		wr32m(hw, NGBE_PBRXCTL, NGBE_PBRXCTL_ENA, 0);
		ngbe_dev_set_link_down(dev);
	}
}

void
ngbe_dev_setup_link_alarm_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	u32 speed;
	bool autoneg = false;

	speed = hw->phy.autoneg_advertised;
	if (!speed)
		hw->mac.get_link_capabilities(hw, &speed, &autoneg);

	hw->mac.setup_link(hw, speed, true);

	intr->flags &= ~NGBE_FLAG_NEED_LINK_CONFIG;
}

/* return 0 means link status changed, -1 means not changed */
int
ngbe_dev_link_update_share(struct rte_eth_dev *dev,
			    int wait_to_complete)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct rte_eth_link link;
	u32 link_speed = NGBE_LINK_SPEED_UNKNOWN;
	u32 lan_speed = 0;
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	bool link_up;
	int err;
	int wait = 1;

	memset(&link, 0, sizeof(link));
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			~RTE_ETH_LINK_SPEED_AUTONEG);

	hw->mac.get_link_status = true;

	if (intr->flags & NGBE_FLAG_NEED_LINK_CONFIG)
		return rte_eth_linkstatus_set(dev, &link);

	/* check if it needs to wait to complete, if lsc interrupt is enabled */
	if (wait_to_complete == 0 || dev->data->dev_conf.intr_conf.lsc != 0)
		wait = 0;

	err = hw->mac.check_link(hw, &link_speed, &link_up, wait);
	if (err != 0) {
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		return rte_eth_linkstatus_set(dev, &link);
	}

	if (!link_up)
		return rte_eth_linkstatus_set(dev, &link);

	intr->flags &= ~NGBE_FLAG_NEED_LINK_CONFIG;
	link.link_status = RTE_ETH_LINK_UP;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	switch (link_speed) {
	default:
	case NGBE_LINK_SPEED_UNKNOWN:
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;

	case NGBE_LINK_SPEED_10M_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_10M;
		lan_speed = 0;
		break;

	case NGBE_LINK_SPEED_100M_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_100M;
		lan_speed = 1;
		break;

	case NGBE_LINK_SPEED_1GB_FULL:
		link.link_speed = RTE_ETH_SPEED_NUM_1G;
		lan_speed = 2;
		break;
	}

	if (hw->is_pf) {
		wr32m(hw, NGBE_LAN_SPEED, NGBE_LAN_SPEED_MASK, lan_speed);
		if (link_speed & (NGBE_LINK_SPEED_1GB_FULL |
				NGBE_LINK_SPEED_100M_FULL |
				NGBE_LINK_SPEED_10M_FULL)) {
			wr32m(hw, NGBE_MACTXCFG, NGBE_MACTXCFG_SPEED_MASK,
				NGBE_MACTXCFG_SPEED_1G | NGBE_MACTXCFG_TE);
		}
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
ngbe_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	return ngbe_dev_link_update_share(dev, wait_to_complete);
}

static int
ngbe_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t fctrl;

	fctrl = rd32(hw, NGBE_PSRCTL);
	fctrl |= (NGBE_PSRCTL_UCP | NGBE_PSRCTL_MCP);
	wr32(hw, NGBE_PSRCTL, fctrl);

	return 0;
}

static int
ngbe_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t fctrl;

	fctrl = rd32(hw, NGBE_PSRCTL);
	fctrl &= (~NGBE_PSRCTL_UCP);
	if (dev->data->all_multicast == 1)
		fctrl |= NGBE_PSRCTL_MCP;
	else
		fctrl &= (~NGBE_PSRCTL_MCP);
	wr32(hw, NGBE_PSRCTL, fctrl);

	return 0;
}

static int
ngbe_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t fctrl;

	fctrl = rd32(hw, NGBE_PSRCTL);
	fctrl |= NGBE_PSRCTL_MCP;
	wr32(hw, NGBE_PSRCTL, fctrl);

	return 0;
}

static int
ngbe_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t fctrl;

	if (dev->data->promiscuous == 1)
		return 0; /* must remain in all_multicast mode */

	fctrl = rd32(hw, NGBE_PSRCTL);
	fctrl &= (~NGBE_PSRCTL_MCP);
	wr32(hw, NGBE_PSRCTL, fctrl);

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param on
 *  Enable or Disable.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	ngbe_dev_link_status_print(dev);
	if (on != 0) {
		intr->mask_misc |= NGBE_ICRMISC_PHY;
		intr->mask_misc |= NGBE_ICRMISC_GPIO;
	} else {
		intr->mask_misc &= ~NGBE_ICRMISC_PHY;
		intr->mask_misc &= ~NGBE_ICRMISC_GPIO;
	}

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_misc_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	u64 mask;

	mask = NGBE_ICR_MASK;
	mask &= (1ULL << NGBE_MISC_VEC_ID);
	intr->mask |= mask;
	intr->mask_misc |= NGBE_ICRMISC_GPIO;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_rxq_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);
	u64 mask;

	mask = NGBE_ICR_MASK;
	mask &= ~((1ULL << NGBE_RX_VEC_START) - 1);
	intr->mask |= mask;

	return 0;
}

/**
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during NIC initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_macsec_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	intr->mask_misc |= NGBE_ICRMISC_LNKSEC;

	return 0;
}

/*
 * It reads ICR and sets flag for the link_update.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t eicr;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	/* read-on-clear nic registers here */
	eicr = ((u32 *)hw->isb_mem)[NGBE_ISB_MISC];
	PMD_DRV_LOG(DEBUG, "eicr %x", eicr);

	intr->flags = 0;

	/* set flag for async link update */
	if (eicr & NGBE_ICRMISC_PHY)
		intr->flags |= NGBE_FLAG_NEED_LINK_UPDATE;

	if (eicr & NGBE_ICRMISC_VFMBX)
		intr->flags |= NGBE_FLAG_MAILBOX;

	if (eicr & NGBE_ICRMISC_LNKSEC)
		intr->flags |= NGBE_FLAG_MACSEC;

	if (eicr & NGBE_ICRMISC_GPIO)
		intr->flags |= NGBE_FLAG_NEED_LINK_UPDATE;

	if (eicr & NGBE_ICRMISC_HEAT)
		intr->flags |= NGBE_FLAG_OVERHEAT;

	((u32 *)hw->isb_mem)[NGBE_ISB_MISC] = 0;

	return 0;
}

/**
 * It gets and then prints the link status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static void
ngbe_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;

	rte_eth_linkstatus_get(dev, &link);

	if (link.link_status == RTE_ETH_LINK_UP) {
		PMD_INIT_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
					(int)(dev->data->port_id),
					(unsigned int)link.link_speed,
			link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
					"full-duplex" : "half-duplex");
	} else {
		PMD_INIT_LOG(INFO, " Port %d: Link Down",
				(int)(dev->data->port_id));
	}
	PMD_INIT_LOG(DEBUG, "PCI Address: " PCI_PRI_FMT,
				pci_dev->addr.domain,
				pci_dev->addr.bus,
				pci_dev->addr.devid,
				pci_dev->addr.function);
}

/*
 * It executes link_update after knowing an interrupt occurred.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ngbe_dev_interrupt_action(struct rte_eth_dev *dev)
{
	struct ngbe_interrupt *intr = ngbe_dev_intr(dev);

	PMD_DRV_LOG(DEBUG, "intr action type %d", intr->flags);

	if (intr->flags & NGBE_FLAG_MAILBOX) {
		ngbe_pf_mbx_process(dev);
		intr->flags &= ~NGBE_FLAG_MAILBOX;
	}

	if (intr->flags & NGBE_FLAG_NEED_LINK_UPDATE) {
		struct rte_eth_link link;

		/*get the link status before link update, for predicting later*/
		rte_eth_linkstatus_get(dev, &link);

		ngbe_dev_link_update(dev, 0);
		intr->flags &= ~NGBE_FLAG_NEED_LINK_UPDATE;
		ngbe_dev_link_status_print(dev);
		if (dev->data->dev_link.link_speed != link.link_speed)
			rte_eth_dev_callback_process(dev,
				RTE_ETH_EVENT_INTR_LSC, NULL);
	}

	if (intr->flags & NGBE_FLAG_OVERHEAT) {
		ngbe_dev_overheat(dev);
		intr->flags &= ~NGBE_FLAG_OVERHEAT;
	}

	PMD_DRV_LOG(DEBUG, "enable intr immediately");
	ngbe_enable_intr(dev);

	return 0;
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
ngbe_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	ngbe_dev_interrupt_get_status(dev);
	ngbe_dev_interrupt_action(dev);
}

static int
ngbe_dev_led_on(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	return hw->mac.led_on(hw, 0) == 0 ? 0 : -ENOTSUP;
}

static int
ngbe_dev_led_off(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	return hw->mac.led_off(hw, 0) == 0 ? 0 : -ENOTSUP;
}

static int
ngbe_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t mflcn_reg;
	uint32_t fccfg_reg;
	int rx_pause;
	int tx_pause;

	fc_conf->pause_time = hw->fc.pause_time;
	fc_conf->high_water = hw->fc.high_water;
	fc_conf->low_water = hw->fc.low_water;
	fc_conf->send_xon = hw->fc.send_xon;
	fc_conf->autoneg = !hw->fc.disable_fc_autoneg;

	/*
	 * Return rx_pause status according to actual setting of
	 * RXFCCFG register.
	 */
	mflcn_reg = rd32(hw, NGBE_RXFCCFG);
	if (mflcn_reg & NGBE_RXFCCFG_FC)
		rx_pause = 1;
	else
		rx_pause = 0;

	/*
	 * Return tx_pause status according to actual setting of
	 * TXFCCFG register.
	 */
	fccfg_reg = rd32(hw, NGBE_TXFCCFG);
	if (fccfg_reg & NGBE_TXFCCFG_FC)
		tx_pause = 1;
	else
		tx_pause = 0;

	if (rx_pause && tx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else if (tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;

	return 0;
}

static int
ngbe_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int err;
	uint32_t rx_buf_size;
	uint32_t max_high_water;
	enum ngbe_fc_mode rte_fcmode_2_ngbe_fcmode[] = {
		ngbe_fc_none,
		ngbe_fc_rx_pause,
		ngbe_fc_tx_pause,
		ngbe_fc_full
	};

	PMD_INIT_FUNC_TRACE();

	rx_buf_size = rd32(hw, NGBE_PBRXSIZE);
	PMD_INIT_LOG(DEBUG, "Rx packet buffer size = 0x%x", rx_buf_size);

	/*
	 * At least reserve one Ethernet frame for watermark
	 * high_water/low_water in kilo bytes for ngbe
	 */
	max_high_water = (rx_buf_size - RTE_ETHER_MAX_LEN) >> 10;
	if (fc_conf->high_water > max_high_water ||
	    fc_conf->high_water < fc_conf->low_water) {
		PMD_INIT_LOG(ERR, "Invalid high/low water setup value in KB");
		PMD_INIT_LOG(ERR, "High_water must <= 0x%x", max_high_water);
		return -EINVAL;
	}

	hw->fc.requested_mode = rte_fcmode_2_ngbe_fcmode[fc_conf->mode];
	hw->fc.pause_time     = fc_conf->pause_time;
	hw->fc.high_water     = fc_conf->high_water;
	hw->fc.low_water      = fc_conf->low_water;
	hw->fc.send_xon       = fc_conf->send_xon;
	hw->fc.disable_fc_autoneg = !fc_conf->autoneg;

	err = hw->mac.fc_enable(hw);

	/* Not negotiated is not an error case */
	if (err == 0 || err == NGBE_ERR_FC_NOT_NEGOTIATED) {
		wr32m(hw, NGBE_MACRXFLT, NGBE_MACRXFLT_CTL_MASK,
		      (fc_conf->mac_ctrl_frame_fwd
		       ? NGBE_MACRXFLT_CTL_NOPS : NGBE_MACRXFLT_CTL_DROP));
		ngbe_flush(hw);

		return 0;
	}

	PMD_INIT_LOG(ERR, "ngbe_fc_enable = 0x%x", err);
	return -EIO;
}

int
ngbe_dev_rss_reta_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_reta_entry64 *reta_conf,
			  uint16_t reta_size)
{
	uint8_t i, j, mask;
	uint32_t reta;
	uint16_t idx, shift;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	PMD_INIT_FUNC_TRACE();

	if (!hw->is_pf) {
		PMD_DRV_LOG(ERR, "RSS reta update is not supported on this "
			"NIC.");
		return -ENOTSUP;
	}

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_128) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, RTE_ETH_RSS_RETA_SIZE_128);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i += 4) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)RS64(reta_conf[idx].mask, shift, 0xF);
		if (!mask)
			continue;

		reta = rd32a(hw, NGBE_REG_RSSTBL, i >> 2);
		for (j = 0; j < 4; j++) {
			if (RS8(mask, j, 0x1)) {
				reta  &= ~(MS32(8 * j, 0xFF));
				reta |= LS32(reta_conf[idx].reta[shift + j],
						8 * j, 0xFF);
			}
		}
		wr32a(hw, NGBE_REG_RSSTBL, i >> 2, reta);
	}
	adapter->rss_reta_updated = 1;

	return 0;
}

int
ngbe_dev_rss_reta_query(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint8_t i, j, mask;
	uint32_t reta;
	uint16_t idx, shift;

	PMD_INIT_FUNC_TRACE();

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_128) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, RTE_ETH_RSS_RETA_SIZE_128);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i += 4) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)RS64(reta_conf[idx].mask, shift, 0xF);
		if (!mask)
			continue;

		reta = rd32a(hw, NGBE_REG_RSSTBL, i >> 2);
		for (j = 0; j < 4; j++) {
			if (RS8(mask, j, 0x1))
				reta_conf[idx].reta[shift + j] =
					(uint16_t)RS32(reta, 8 * j, 0xFF);
		}
	}

	return 0;
}

static int
ngbe_add_rar(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
				uint32_t index, uint32_t pool)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t enable_addr = 1;

	return ngbe_set_rar(hw, index, mac_addr->addr_bytes,
			     pool, enable_addr);
}

static void
ngbe_remove_rar(struct rte_eth_dev *dev, uint32_t index)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	ngbe_clear_rar(hw, index);
}

static int
ngbe_set_default_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	ngbe_remove_rar(dev, 0);
	ngbe_add_rar(dev, addr, 0, pci_dev->max_vfs);

	return 0;
}

static int
ngbe_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t frame_size = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
	struct rte_eth_dev_data *dev_data = dev->data;

	/* If device is started, refuse mtu that requires the support of
	 * scattered packets when this feature has not been enabled before.
	 */
	if (dev_data->dev_started && !dev_data->scattered_rx &&
	    (frame_size + 2 * RTE_VLAN_HLEN >
	     dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)) {
		PMD_INIT_LOG(ERR, "Stop port first.");
		return -EINVAL;
	}

	wr32m(hw, NGBE_FRMSZ, NGBE_FRMSZ_MAX_MASK,
		NGBE_FRMSZ_MAX(frame_size));

	return 0;
}

static uint32_t
ngbe_uta_vector(struct ngbe_hw *hw, struct rte_ether_addr *uc_addr)
{
	uint32_t vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 4) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 3) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((uc_addr->addr_bytes[4] >> 2) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((uc_addr->addr_bytes[4]) |
			(((uint16_t)uc_addr->addr_bytes[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

static int
ngbe_uc_hash_table_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *mac_addr, uint8_t on)
{
	uint32_t vector;
	uint32_t uta_idx;
	uint32_t reg_val;
	uint32_t uta_mask;
	uint32_t psrctl;

	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_uta_info *uta_info = NGBE_DEV_UTA_INFO(dev);

	vector = ngbe_uta_vector(hw, mac_addr);
	uta_idx = (vector >> 5) & 0x7F;
	uta_mask = 0x1UL << (vector & 0x1F);

	if (!!on == !!(uta_info->uta_shadow[uta_idx] & uta_mask))
		return 0;

	reg_val = rd32(hw, NGBE_UCADDRTBL(uta_idx));
	if (on) {
		uta_info->uta_in_use++;
		reg_val |= uta_mask;
		uta_info->uta_shadow[uta_idx] |= uta_mask;
	} else {
		uta_info->uta_in_use--;
		reg_val &= ~uta_mask;
		uta_info->uta_shadow[uta_idx] &= ~uta_mask;
	}

	wr32(hw, NGBE_UCADDRTBL(uta_idx), reg_val);

	psrctl = rd32(hw, NGBE_PSRCTL);
	if (uta_info->uta_in_use > 0)
		psrctl |= NGBE_PSRCTL_UCHFENA;
	else
		psrctl &= ~NGBE_PSRCTL_UCHFENA;

	psrctl &= ~NGBE_PSRCTL_ADHF12_MASK;
	psrctl |= NGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, NGBE_PSRCTL, psrctl);

	return 0;
}

static int
ngbe_uc_all_hash_table_set(struct rte_eth_dev *dev, uint8_t on)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_uta_info *uta_info = NGBE_DEV_UTA_INFO(dev);
	uint32_t psrctl;
	int i;

	if (on) {
		for (i = 0; i < RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY; i++) {
			uta_info->uta_shadow[i] = ~0;
			wr32(hw, NGBE_UCADDRTBL(i), ~0);
		}
	} else {
		for (i = 0; i < RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY; i++) {
			uta_info->uta_shadow[i] = 0;
			wr32(hw, NGBE_UCADDRTBL(i), 0);
		}
	}

	psrctl = rd32(hw, NGBE_PSRCTL);
	if (on)
		psrctl |= NGBE_PSRCTL_UCHFENA;
	else
		psrctl &= ~NGBE_PSRCTL_UCHFENA;

	psrctl &= ~NGBE_PSRCTL_ADHF12_MASK;
	psrctl |= NGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, NGBE_PSRCTL, psrctl);

	return 0;
}

/**
 * Set the IVAR registers, mapping interrupt causes to vectors
 * @param hw
 *  pointer to ngbe_hw struct
 * @direction
 *  0 for Rx, 1 for Tx, -1 for other causes
 * @queue
 *  queue to map the corresponding interrupt to
 * @msix_vector
 *  the vector to map to the corresponding queue
 */
void
ngbe_set_ivar_map(struct ngbe_hw *hw, int8_t direction,
		   uint8_t queue, uint8_t msix_vector)
{
	uint32_t tmp, idx;

	if (direction == -1) {
		/* other causes */
		msix_vector |= NGBE_IVARMISC_VLD;
		idx = 0;
		tmp = rd32(hw, NGBE_IVARMISC);
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, NGBE_IVARMISC, tmp);
	} else {
		/* rx or tx causes */
		/* Workaround for ICR lost */
		idx = ((16 * (queue & 1)) + (8 * direction));
		tmp = rd32(hw, NGBE_IVAR(queue >> 1));
		tmp &= ~(0xFF << idx);
		tmp |= (msix_vector << idx);
		wr32(hw, NGBE_IVAR(queue >> 1), tmp);
	}
}

/**
 * Sets up the hardware to properly generate MSI-X interrupts
 * @hw
 *  board private structure
 */
static void
ngbe_configure_msix(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t queue_id, base = NGBE_MISC_VEC_ID;
	uint32_t vec = NGBE_MISC_VEC_ID;
	uint32_t gpie;

	/*
	 * Won't configure MSI-X register if no mapping is done
	 * between intr vector and event fd
	 * but if MSI-X has been enabled already, need to configure
	 * auto clean, auto mask and throttling.
	 */
	gpie = rd32(hw, NGBE_GPIE);
	if (!rte_intr_dp_is_en(intr_handle) &&
	    !(gpie & NGBE_GPIE_MSIX))
		return;

	if (rte_intr_allow_others(intr_handle)) {
		base = NGBE_RX_VEC_START;
		vec = base;
	}

	/* setup GPIE for MSI-X mode */
	gpie = rd32(hw, NGBE_GPIE);
	gpie |= NGBE_GPIE_MSIX;
	wr32(hw, NGBE_GPIE, gpie);

	/* Populate the IVAR table and set the ITR values to the
	 * corresponding register.
	 */
	if (rte_intr_dp_is_en(intr_handle)) {
		for (queue_id = 0; queue_id < dev->data->nb_rx_queues;
			queue_id++) {
			/* by default, 1:1 mapping */
			ngbe_set_ivar_map(hw, 0, queue_id, vec);
			rte_intr_vec_list_index_set(intr_handle,
							   queue_id, vec);
			if (vec < base + rte_intr_nb_efd_get(intr_handle)
			    - 1)
				vec++;
		}

		ngbe_set_ivar_map(hw, -1, 1, NGBE_MISC_VEC_ID);
	}
	wr32(hw, NGBE_ITR(NGBE_MISC_VEC_ID),
			NGBE_ITR_IVAL_1G(NGBE_QUEUE_ITR_INTERVAL_DEFAULT)
			| NGBE_ITR_WRDSA);
}

static u8 *
ngbe_dev_addr_list_itr(__rte_unused struct ngbe_hw *hw,
			u8 **mc_addr_ptr, u32 *vmdq)
{
	u8 *mc_addr;

	*vmdq = 0;
	mc_addr = *mc_addr_ptr;
	*mc_addr_ptr = (mc_addr + sizeof(struct rte_ether_addr));
	return mc_addr;
}

int
ngbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
			  struct rte_ether_addr *mc_addr_set,
			  uint32_t nb_mc_addr)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	u8 *mc_addr_list;

	mc_addr_list = (u8 *)mc_addr_set;
	return hw->mac.update_mc_addr_list(hw, mc_addr_list, nb_mc_addr,
					 ngbe_dev_addr_list_itr, TRUE);
}

static uint64_t
ngbe_read_systime_cyclecounter(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint64_t systime_cycles;

	systime_cycles = (uint64_t)rd32(hw, NGBE_TSTIMEL);
	systime_cycles |= (uint64_t)rd32(hw, NGBE_TSTIMEH) << 32;

	return systime_cycles;
}

static uint64_t
ngbe_read_rx_tstamp_cyclecounter(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint64_t rx_tstamp_cycles;

	/* TSRXSTMPL stores ns and TSRXSTMPH stores seconds. */
	rx_tstamp_cycles = (uint64_t)rd32(hw, NGBE_TSRXSTMPL);
	rx_tstamp_cycles |= (uint64_t)rd32(hw, NGBE_TSRXSTMPH) << 32;

	return rx_tstamp_cycles;
}

static uint64_t
ngbe_read_tx_tstamp_cyclecounter(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint64_t tx_tstamp_cycles;

	/* TSTXSTMPL stores ns and TSTXSTMPH stores seconds. */
	tx_tstamp_cycles = (uint64_t)rd32(hw, NGBE_TSTXSTMPL);
	tx_tstamp_cycles |= (uint64_t)rd32(hw, NGBE_TSTXSTMPH) << 32;

	return tx_tstamp_cycles;
}

static void
ngbe_start_timecounters(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	uint32_t incval = 0;
	uint32_t shift = 0;

	incval = NGBE_INCVAL_1GB;
	shift = NGBE_INCVAL_SHIFT_1GB;

	wr32(hw, NGBE_TSTIMEINC, NGBE_TSTIMEINC_IV(incval));

	memset(&adapter->systime_tc, 0, sizeof(struct rte_timecounter));
	memset(&adapter->rx_tstamp_tc, 0, sizeof(struct rte_timecounter));
	memset(&adapter->tx_tstamp_tc, 0, sizeof(struct rte_timecounter));

	adapter->systime_tc.cc_mask = NGBE_CYCLECOUNTER_MASK;
	adapter->systime_tc.cc_shift = shift;
	adapter->systime_tc.nsec_mask = (1ULL << shift) - 1;

	adapter->rx_tstamp_tc.cc_mask = NGBE_CYCLECOUNTER_MASK;
	adapter->rx_tstamp_tc.cc_shift = shift;
	adapter->rx_tstamp_tc.nsec_mask = (1ULL << shift) - 1;

	adapter->tx_tstamp_tc.cc_mask = NGBE_CYCLECOUNTER_MASK;
	adapter->tx_tstamp_tc.cc_shift = shift;
	adapter->tx_tstamp_tc.nsec_mask = (1ULL << shift) - 1;
}

static int
ngbe_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	adapter->systime_tc.nsec += delta;
	adapter->rx_tstamp_tc.nsec += delta;
	adapter->tx_tstamp_tc.nsec += delta;

	return 0;
}

static int
ngbe_timesync_write_time(struct rte_eth_dev *dev, const struct timespec *ts)
{
	uint64_t ns;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	ns = rte_timespec_to_ns(ts);
	/* Set the timecounters to a new value. */
	adapter->systime_tc.nsec = ns;
	adapter->rx_tstamp_tc.nsec = ns;
	adapter->tx_tstamp_tc.nsec = ns;

	return 0;
}

static int
ngbe_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts)
{
	uint64_t ns, systime_cycles;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	systime_cycles = ngbe_read_systime_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->systime_tc, systime_cycles);
	*ts = rte_ns_to_timespec(ns);

	return 0;
}

static int
ngbe_timesync_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t tsync_ctl;

	/* Stop the timesync system time. */
	wr32(hw, NGBE_TSTIMEINC, 0x0);
	/* Reset the timesync system time value. */
	wr32(hw, NGBE_TSTIMEL, 0x0);
	wr32(hw, NGBE_TSTIMEH, 0x0);

	ngbe_start_timecounters(dev);

	/* Enable L2 filtering of IEEE1588/802.1AS Ethernet frame types. */
	wr32(hw, NGBE_ETFLT(NGBE_ETF_ID_1588),
		RTE_ETHER_TYPE_1588 | NGBE_ETFLT_ENA | NGBE_ETFLT_1588);

	/* Enable timestamping of received PTP packets. */
	tsync_ctl = rd32(hw, NGBE_TSRXCTL);
	tsync_ctl |= NGBE_TSRXCTL_ENA;
	wr32(hw, NGBE_TSRXCTL, tsync_ctl);

	/* Enable timestamping of transmitted PTP packets. */
	tsync_ctl = rd32(hw, NGBE_TSTXCTL);
	tsync_ctl |= NGBE_TSTXCTL_ENA;
	wr32(hw, NGBE_TSTXCTL, tsync_ctl);

	ngbe_flush(hw);

	return 0;
}

static int
ngbe_timesync_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t tsync_ctl;

	/* Disable timestamping of transmitted PTP packets. */
	tsync_ctl = rd32(hw, NGBE_TSTXCTL);
	tsync_ctl &= ~NGBE_TSTXCTL_ENA;
	wr32(hw, NGBE_TSTXCTL, tsync_ctl);

	/* Disable timestamping of received PTP packets. */
	tsync_ctl = rd32(hw, NGBE_TSRXCTL);
	tsync_ctl &= ~NGBE_TSRXCTL_ENA;
	wr32(hw, NGBE_TSRXCTL, tsync_ctl);

	/* Disable L2 filtering of IEEE1588/802.1AS Ethernet frame types. */
	wr32(hw, NGBE_ETFLT(NGBE_ETF_ID_1588), 0);

	/* Stop incrementing the System Time registers. */
	wr32(hw, NGBE_TSTIMEINC, 0);

	return 0;
}

static int
ngbe_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				 struct timespec *timestamp,
				 uint32_t flags __rte_unused)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	uint32_t tsync_rxctl;
	uint64_t rx_tstamp_cycles;
	uint64_t ns;

	tsync_rxctl = rd32(hw, NGBE_TSRXCTL);
	if ((tsync_rxctl & NGBE_TSRXCTL_VLD) == 0)
		return -EINVAL;

	rx_tstamp_cycles = ngbe_read_rx_tstamp_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->rx_tstamp_tc, rx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return  0;
}

static int
ngbe_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				 struct timespec *timestamp)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	uint32_t tsync_txctl;
	uint64_t tx_tstamp_cycles;
	uint64_t ns;

	tsync_txctl = rd32(hw, NGBE_TSTXCTL);
	if ((tsync_txctl & NGBE_TSTXCTL_VLD) == 0)
		return -EINVAL;

	tx_tstamp_cycles = ngbe_read_tx_tstamp_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->tx_tstamp_tc, tx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

static int
ngbe_get_reg_length(struct rte_eth_dev *dev __rte_unused)
{
	int count = 0;
	int g_ind = 0;
	const struct reg_info *reg_group;
	const struct reg_info **reg_set = ngbe_regs_others;

	while ((reg_group = reg_set[g_ind++]))
		count += ngbe_regs_group_count(reg_group);

	return count;
}

static int
ngbe_get_regs(struct rte_eth_dev *dev,
	      struct rte_dev_reg_info *regs)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t *data = regs->data;
	int g_ind = 0;
	int count = 0;
	const struct reg_info *reg_group;
	const struct reg_info **reg_set = ngbe_regs_others;

	if (data == NULL) {
		regs->length = ngbe_get_reg_length(dev);
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* Support only full register dump */
	if (regs->length == 0 ||
	    regs->length == (uint32_t)ngbe_get_reg_length(dev)) {
		regs->version = hw->mac.type << 24 |
				hw->revision_id << 16 |
				hw->device_id;
		while ((reg_group = reg_set[g_ind++]))
			count += ngbe_read_regs_group(dev, &data[count],
						      reg_group);
		return 0;
	}

	return -ENOTSUP;
}

static int
ngbe_get_eeprom_length(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	/* Return unit is byte count */
	return hw->rom.word_size * 2;
}

static int
ngbe_get_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *in_eeprom)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_rom_info *eeprom = &hw->rom;
	uint16_t *data = in_eeprom->data;
	int first, length;

	first = in_eeprom->offset >> 1;
	length = in_eeprom->length >> 1;
	if (first > hw->rom.word_size ||
	    ((first + length) > hw->rom.word_size))
		return -EINVAL;

	in_eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	return eeprom->readw_buffer(hw, first, length, data);
}

static int
ngbe_set_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *in_eeprom)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_rom_info *eeprom = &hw->rom;
	uint16_t *data = in_eeprom->data;
	int first, length;

	first = in_eeprom->offset >> 1;
	length = in_eeprom->length >> 1;
	if (first > hw->rom.word_size ||
	    ((first + length) > hw->rom.word_size))
		return -EINVAL;

	in_eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	return eeprom->writew_buffer(hw,  first, length, data);
}

static const struct eth_dev_ops ngbe_eth_dev_ops = {
	.dev_configure              = ngbe_dev_configure,
	.dev_infos_get              = ngbe_dev_info_get,
	.dev_start                  = ngbe_dev_start,
	.dev_stop                   = ngbe_dev_stop,
	.dev_set_link_up            = ngbe_dev_set_link_up,
	.dev_set_link_down          = ngbe_dev_set_link_down,
	.dev_close                  = ngbe_dev_close,
	.dev_reset                  = ngbe_dev_reset,
	.promiscuous_enable         = ngbe_dev_promiscuous_enable,
	.promiscuous_disable        = ngbe_dev_promiscuous_disable,
	.allmulticast_enable        = ngbe_dev_allmulticast_enable,
	.allmulticast_disable       = ngbe_dev_allmulticast_disable,
	.link_update                = ngbe_dev_link_update,
	.stats_get                  = ngbe_dev_stats_get,
	.xstats_get                 = ngbe_dev_xstats_get,
	.xstats_get_by_id           = ngbe_dev_xstats_get_by_id,
	.stats_reset                = ngbe_dev_stats_reset,
	.xstats_reset               = ngbe_dev_xstats_reset,
	.xstats_get_names           = ngbe_dev_xstats_get_names,
	.xstats_get_names_by_id     = ngbe_dev_xstats_get_names_by_id,
	.fw_version_get             = ngbe_fw_version_get,
	.dev_supported_ptypes_get   = ngbe_dev_supported_ptypes_get,
	.mtu_set                    = ngbe_dev_mtu_set,
	.vlan_filter_set            = ngbe_vlan_filter_set,
	.vlan_tpid_set              = ngbe_vlan_tpid_set,
	.vlan_offload_set           = ngbe_vlan_offload_set,
	.vlan_strip_queue_set       = ngbe_vlan_strip_queue_set,
	.rx_queue_start	            = ngbe_dev_rx_queue_start,
	.rx_queue_stop              = ngbe_dev_rx_queue_stop,
	.tx_queue_start	            = ngbe_dev_tx_queue_start,
	.tx_queue_stop              = ngbe_dev_tx_queue_stop,
	.rx_queue_setup             = ngbe_dev_rx_queue_setup,
	.rx_queue_release           = ngbe_dev_rx_queue_release,
	.tx_queue_setup             = ngbe_dev_tx_queue_setup,
	.tx_queue_release           = ngbe_dev_tx_queue_release,
	.dev_led_on                 = ngbe_dev_led_on,
	.dev_led_off                = ngbe_dev_led_off,
	.flow_ctrl_get              = ngbe_flow_ctrl_get,
	.flow_ctrl_set              = ngbe_flow_ctrl_set,
	.mac_addr_add               = ngbe_add_rar,
	.mac_addr_remove            = ngbe_remove_rar,
	.mac_addr_set               = ngbe_set_default_mac_addr,
	.uc_hash_table_set          = ngbe_uc_hash_table_set,
	.uc_all_hash_table_set      = ngbe_uc_all_hash_table_set,
	.reta_update                = ngbe_dev_rss_reta_update,
	.reta_query                 = ngbe_dev_rss_reta_query,
	.rss_hash_update            = ngbe_dev_rss_hash_update,
	.rss_hash_conf_get          = ngbe_dev_rss_hash_conf_get,
	.set_mc_addr_list           = ngbe_dev_set_mc_addr_list,
	.rxq_info_get               = ngbe_rxq_info_get,
	.txq_info_get               = ngbe_txq_info_get,
	.rx_burst_mode_get          = ngbe_rx_burst_mode_get,
	.tx_burst_mode_get          = ngbe_tx_burst_mode_get,
	.timesync_enable            = ngbe_timesync_enable,
	.timesync_disable           = ngbe_timesync_disable,
	.timesync_read_rx_timestamp = ngbe_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = ngbe_timesync_read_tx_timestamp,
	.get_reg                    = ngbe_get_regs,
	.get_eeprom_length          = ngbe_get_eeprom_length,
	.get_eeprom                 = ngbe_get_eeprom,
	.set_eeprom                 = ngbe_set_eeprom,
	.timesync_adjust_time       = ngbe_timesync_adjust_time,
	.timesync_read_time         = ngbe_timesync_read_time,
	.timesync_write_time        = ngbe_timesync_write_time,
	.tx_done_cleanup            = ngbe_dev_tx_done_cleanup,
};

RTE_PMD_REGISTER_PCI(net_ngbe, rte_ngbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ngbe, pci_id_ngbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ngbe, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_driver, driver, NOTICE);

#ifdef RTE_ETHDEV_DEBUG_RX
	RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_rx, rx, DEBUG);
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
	RTE_LOG_REGISTER_SUFFIX(ngbe_logtype_tx, tx, DEBUG);
#endif
