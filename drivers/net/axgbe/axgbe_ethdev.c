/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_rxtx.h"
#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"
#include "axgbe_regs.h"
#include "rte_time.h"

#include "eal_filesystem.h"

#include <rte_vect.h>

#ifdef RTE_ARCH_X86
#include <cpuid.h>
#else
#define __cpuid(n, a, b, c, d)
#endif

static int eth_axgbe_dev_init(struct rte_eth_dev *eth_dev);
static int  axgbe_dev_configure(struct rte_eth_dev *dev);
static int  axgbe_dev_start(struct rte_eth_dev *dev);
static int  axgbe_dev_stop(struct rte_eth_dev *dev);
static void axgbe_dev_interrupt_handler(void *param);
static int axgbe_dev_close(struct rte_eth_dev *dev);
static int axgbe_dev_reset(struct rte_eth_dev *dev);
static int axgbe_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int axgbe_dev_promiscuous_disable(struct rte_eth_dev *dev);
static int axgbe_dev_allmulticast_enable(struct rte_eth_dev *dev);
static int axgbe_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int axgbe_dev_mac_addr_set(struct rte_eth_dev *dev,
				  struct rte_ether_addr *mac_addr);
static int axgbe_dev_mac_addr_add(struct rte_eth_dev *dev,
				  struct rte_ether_addr *mac_addr,
				  uint32_t index,
				  uint32_t vmdq);
static void axgbe_dev_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
static int axgbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
				      struct rte_ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr);
static int axgbe_dev_uc_hash_table_set(struct rte_eth_dev *dev,
				       struct rte_ether_addr *mac_addr,
				       uint8_t add);
static int axgbe_dev_uc_all_hash_table_set(struct rte_eth_dev *dev,
					   uint8_t add);
static int axgbe_dev_link_update(struct rte_eth_dev *dev,
				 int wait_to_complete);
static int axgbe_dev_get_regs(struct rte_eth_dev *dev,
			      struct rte_dev_reg_info *regs);
static int axgbe_dev_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *stats);
static int axgbe_dev_stats_reset(struct rte_eth_dev *dev);
static int axgbe_dev_xstats_get(struct rte_eth_dev *dev,
				struct rte_eth_xstat *stats,
				unsigned int n);
static int
axgbe_dev_xstats_get_names(struct rte_eth_dev *dev,
			   struct rte_eth_xstat_name *xstats_names,
			   unsigned int size);
static int
axgbe_dev_xstats_get_by_id(struct rte_eth_dev *dev,
			   const uint64_t *ids,
			   uint64_t *values,
			   unsigned int n);
static int
axgbe_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
				 const uint64_t *ids,
				 struct rte_eth_xstat_name *xstats_names,
				 unsigned int size);
static int axgbe_dev_xstats_reset(struct rte_eth_dev *dev);
static int axgbe_dev_rss_reta_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_reta_entry64 *reta_conf,
			  uint16_t reta_size);
static int axgbe_dev_rss_reta_query(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size);
static int axgbe_dev_rss_hash_update(struct rte_eth_dev *dev,
				     struct rte_eth_rss_conf *rss_conf);
static int axgbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				       struct rte_eth_rss_conf *rss_conf);
static int  axgbe_dev_info_get(struct rte_eth_dev *dev,
			       struct rte_eth_dev_info *dev_info);
static int axgbe_flow_ctrl_get(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
static int axgbe_flow_ctrl_set(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
static int axgbe_priority_flow_ctrl_set(struct rte_eth_dev *dev,
				struct rte_eth_pfc_conf *pfc_conf);
static void axgbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);
static void axgbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);
const uint32_t *axgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev);
static int axgb_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

static int
axgbe_timesync_enable(struct rte_eth_dev *dev);
static int
axgbe_timesync_disable(struct rte_eth_dev *dev);
static int
axgbe_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
			struct timespec *timestamp, uint32_t flags);
static int
axgbe_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
			struct timespec *timestamp);
static int
axgbe_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta);
static int
axgbe_timesync_read_time(struct rte_eth_dev *dev,
			struct timespec *timestamp);
static int
axgbe_timesync_write_time(struct rte_eth_dev *dev,
			const struct timespec *timestamp);
static void
axgbe_set_tstamp_time(struct axgbe_port *pdata, unsigned int sec,
			unsigned int nsec);
static void
axgbe_update_tstamp_addend(struct axgbe_port *pdata,
			unsigned int addend);
static int
	axgbe_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vid, int on);
static int axgbe_vlan_tpid_set(struct rte_eth_dev *dev,
				enum rte_vlan_type vlan_type, uint16_t tpid);
static int axgbe_vlan_offload_set(struct rte_eth_dev *dev, int mask);

struct axgbe_xstats {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	int offset;
};

#define AXGMAC_MMC_STAT(_string, _var)                           \
	{ _string,                                              \
	  offsetof(struct axgbe_mmc_stats, _var),       \
	}

static const struct axgbe_xstats axgbe_xstats_strings[] = {
	AXGMAC_MMC_STAT("tx_bytes", txoctetcount_gb),
	AXGMAC_MMC_STAT("tx_packets", txframecount_gb),
	AXGMAC_MMC_STAT("tx_unicast_packets", txunicastframes_gb),
	AXGMAC_MMC_STAT("tx_broadcast_packets", txbroadcastframes_gb),
	AXGMAC_MMC_STAT("tx_multicast_packets", txmulticastframes_gb),
	AXGMAC_MMC_STAT("tx_vlan_packets", txvlanframes_g),
	AXGMAC_MMC_STAT("tx_64_byte_packets", tx64octets_gb),
	AXGMAC_MMC_STAT("tx_65_to_127_byte_packets", tx65to127octets_gb),
	AXGMAC_MMC_STAT("tx_128_to_255_byte_packets", tx128to255octets_gb),
	AXGMAC_MMC_STAT("tx_256_to_511_byte_packets", tx256to511octets_gb),
	AXGMAC_MMC_STAT("tx_512_to_1023_byte_packets", tx512to1023octets_gb),
	AXGMAC_MMC_STAT("tx_1024_to_max_byte_packets", tx1024tomaxoctets_gb),
	AXGMAC_MMC_STAT("tx_underflow_errors", txunderflowerror),
	AXGMAC_MMC_STAT("tx_pause_frames", txpauseframes),

	AXGMAC_MMC_STAT("rx_bytes", rxoctetcount_gb),
	AXGMAC_MMC_STAT("rx_packets", rxframecount_gb),
	AXGMAC_MMC_STAT("rx_unicast_packets", rxunicastframes_g),
	AXGMAC_MMC_STAT("rx_broadcast_packets", rxbroadcastframes_g),
	AXGMAC_MMC_STAT("rx_multicast_packets", rxmulticastframes_g),
	AXGMAC_MMC_STAT("rx_vlan_packets", rxvlanframes_gb),
	AXGMAC_MMC_STAT("rx_64_byte_packets", rx64octets_gb),
	AXGMAC_MMC_STAT("rx_65_to_127_byte_packets", rx65to127octets_gb),
	AXGMAC_MMC_STAT("rx_128_to_255_byte_packets", rx128to255octets_gb),
	AXGMAC_MMC_STAT("rx_256_to_511_byte_packets", rx256to511octets_gb),
	AXGMAC_MMC_STAT("rx_512_to_1023_byte_packets", rx512to1023octets_gb),
	AXGMAC_MMC_STAT("rx_1024_to_max_byte_packets", rx1024tomaxoctets_gb),
	AXGMAC_MMC_STAT("rx_undersize_packets", rxundersize_g),
	AXGMAC_MMC_STAT("rx_oversize_packets", rxoversize_g),
	AXGMAC_MMC_STAT("rx_crc_errors", rxcrcerror),
	AXGMAC_MMC_STAT("rx_crc_errors_small_packets", rxrunterror),
	AXGMAC_MMC_STAT("rx_crc_errors_giant_packets", rxjabbererror),
	AXGMAC_MMC_STAT("rx_length_errors", rxlengtherror),
	AXGMAC_MMC_STAT("rx_out_of_range_errors", rxoutofrangetype),
	AXGMAC_MMC_STAT("rx_fifo_overflow_errors", rxfifooverflow),
	AXGMAC_MMC_STAT("rx_watchdog_errors", rxwatchdogerror),
	AXGMAC_MMC_STAT("rx_pause_frames", rxpauseframes),
};

#define AXGBE_XSTATS_COUNT        ARRAY_SIZE(axgbe_xstats_strings)

/* The set of PCI devices this driver supports */
#define AMD_PCI_VENDOR_ID       0x1022

#define	Fam17h	0x17
#define	Fam19h	0x19

#define	CPUID_VENDOR_AuthenticAMD_ebx	0x68747541
#define	CPUID_VENDOR_AuthenticAMD_ecx	0x444d4163
#define	CPUID_VENDOR_AuthenticAMD_edx	0x69746e65

#define AMD_PCI_AXGBE_DEVICE_V2A 0x1458
#define AMD_PCI_AXGBE_DEVICE_V2B 0x1459

static const struct rte_pci_id pci_id_axgbe_map[] = {
	{RTE_PCI_DEVICE(AMD_PCI_VENDOR_ID, AMD_PCI_AXGBE_DEVICE_V2A)},
	{RTE_PCI_DEVICE(AMD_PCI_VENDOR_ID, AMD_PCI_AXGBE_DEVICE_V2B)},
	{ .vendor_id = 0, },
};

static struct axgbe_version_data axgbe_v2a = {
	.init_function_ptrs_phy_impl    = axgbe_init_function_ptrs_phy_v2,
	.xpcs_access			= AXGBE_XPCS_ACCESS_V2,
	.mmc_64bit			= 1,
	.tx_max_fifo_size		= 229376,
	.rx_max_fifo_size		= 229376,
	.tx_tstamp_workaround		= 1,
	.ecc_support			= 1,
	.i2c_support			= 1,
	.an_cdr_workaround		= 1,
};

static struct axgbe_version_data axgbe_v2b = {
	.init_function_ptrs_phy_impl    = axgbe_init_function_ptrs_phy_v2,
	.xpcs_access			= AXGBE_XPCS_ACCESS_V2,
	.mmc_64bit			= 1,
	.tx_max_fifo_size		= 65536,
	.rx_max_fifo_size		= 65536,
	.tx_tstamp_workaround		= 1,
	.ecc_support			= 1,
	.i2c_support			= 1,
	.an_cdr_workaround		= 1,
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = AXGBE_MAX_RING_DESC,
	.nb_min = AXGBE_MIN_RING_DESC,
	.nb_align = 8,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = AXGBE_MAX_RING_DESC,
	.nb_min = AXGBE_MIN_RING_DESC,
	.nb_align = 8,
};

static const struct eth_dev_ops axgbe_eth_dev_ops = {
	.dev_configure        = axgbe_dev_configure,
	.dev_start            = axgbe_dev_start,
	.dev_stop             = axgbe_dev_stop,
	.dev_close            = axgbe_dev_close,
	.dev_reset            = axgbe_dev_reset,
	.promiscuous_enable   = axgbe_dev_promiscuous_enable,
	.promiscuous_disable  = axgbe_dev_promiscuous_disable,
	.allmulticast_enable  = axgbe_dev_allmulticast_enable,
	.allmulticast_disable = axgbe_dev_allmulticast_disable,
	.mac_addr_set         = axgbe_dev_mac_addr_set,
	.mac_addr_add         = axgbe_dev_mac_addr_add,
	.mac_addr_remove      = axgbe_dev_mac_addr_remove,
	.set_mc_addr_list     = axgbe_dev_set_mc_addr_list,
	.uc_hash_table_set    = axgbe_dev_uc_hash_table_set,
	.uc_all_hash_table_set = axgbe_dev_uc_all_hash_table_set,
	.link_update          = axgbe_dev_link_update,
	.get_reg	      = axgbe_dev_get_regs,
	.stats_get            = axgbe_dev_stats_get,
	.stats_reset          = axgbe_dev_stats_reset,
	.xstats_get	      = axgbe_dev_xstats_get,
	.xstats_reset	      = axgbe_dev_xstats_reset,
	.xstats_get_names     = axgbe_dev_xstats_get_names,
	.xstats_get_names_by_id = axgbe_dev_xstats_get_names_by_id,
	.xstats_get_by_id     = axgbe_dev_xstats_get_by_id,
	.reta_update          = axgbe_dev_rss_reta_update,
	.reta_query           = axgbe_dev_rss_reta_query,
	.rss_hash_update      = axgbe_dev_rss_hash_update,
	.rss_hash_conf_get    = axgbe_dev_rss_hash_conf_get,
	.dev_infos_get        = axgbe_dev_info_get,
	.rx_queue_setup       = axgbe_dev_rx_queue_setup,
	.rx_queue_release     = axgbe_dev_rx_queue_release,
	.tx_queue_setup       = axgbe_dev_tx_queue_setup,
	.tx_queue_release     = axgbe_dev_tx_queue_release,
	.flow_ctrl_get        = axgbe_flow_ctrl_get,
	.flow_ctrl_set        = axgbe_flow_ctrl_set,
	.priority_flow_ctrl_set = axgbe_priority_flow_ctrl_set,
	.rxq_info_get                 = axgbe_rxq_info_get,
	.txq_info_get                 = axgbe_txq_info_get,
	.dev_supported_ptypes_get     = axgbe_dev_supported_ptypes_get,
	.mtu_set		= axgb_mtu_set,
	.vlan_filter_set      = axgbe_vlan_filter_set,
	.vlan_tpid_set        = axgbe_vlan_tpid_set,
	.vlan_offload_set     = axgbe_vlan_offload_set,
	.timesync_enable              = axgbe_timesync_enable,
	.timesync_disable             = axgbe_timesync_disable,
	.timesync_read_rx_timestamp   = axgbe_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp   = axgbe_timesync_read_tx_timestamp,
	.timesync_adjust_time         = axgbe_timesync_adjust_time,
	.timesync_read_time           = axgbe_timesync_read_time,
	.timesync_write_time          = axgbe_timesync_write_time,
	.fw_version_get			= axgbe_dev_fw_version_get,
};

static int axgbe_phy_reset(struct axgbe_port *pdata)
{
	pdata->phy_link = -1;
	pdata->phy_speed = SPEED_UNKNOWN;
	return pdata->phy_if.phy_reset(pdata);
}

/*
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 *
 * @return
 *  void
 */
static void
axgbe_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int dma_isr, dma_ch_isr;

	pdata->phy_if.an_isr(pdata);
	/*DMA related interrupts*/
	dma_isr = AXGMAC_IOREAD(pdata, DMA_ISR);
	PMD_DRV_LOG(DEBUG, "DMA_ISR=%#010x\n", dma_isr);
	if (dma_isr) {
		if (dma_isr & 1) {
			dma_ch_isr =
				AXGMAC_DMA_IOREAD((struct axgbe_rx_queue *)
						  pdata->rx_queues[0],
						  DMA_CH_SR);
			PMD_DRV_LOG(DEBUG, "DMA_CH0_ISR=%#010x\n", dma_ch_isr);
			AXGMAC_DMA_IOWRITE((struct axgbe_rx_queue *)
					   pdata->rx_queues[0],
					   DMA_CH_SR, dma_ch_isr);
		}
	}
	/* Unmask interrupts since disabled after generation */
	rte_intr_ack(pdata->pci_dev->intr_handle);
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
axgbe_dev_configure(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata =  dev->data->dev_private;
	/* Checksum offload to hardware */
	pdata->rx_csum_enable = dev->data->dev_conf.rxmode.offloads &
				RTE_ETH_RX_OFFLOAD_CHECKSUM;
	return 0;
}

static int
axgbe_dev_rx_mq_config(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	if (dev->data->dev_conf.rxmode.mq_mode == RTE_ETH_MQ_RX_RSS)
		pdata->rss_enable = 1;
	else if (dev->data->dev_conf.rxmode.mq_mode == RTE_ETH_MQ_RX_NONE)
		pdata->rss_enable = 0;
	else
		return  -1;
	return 0;
}

static int
axgbe_dev_start(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	uint16_t i;
	int ret;

	dev->dev_ops = &axgbe_eth_dev_ops;

	PMD_INIT_FUNC_TRACE();

	/* Multiqueue RSS */
	ret = axgbe_dev_rx_mq_config(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Unable to config RX MQ\n");
		return ret;
	}
	ret = axgbe_phy_reset(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "phy reset failed\n");
		return ret;
	}
	ret = pdata->hw_if.init(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "dev_init failed\n");
		return ret;
	}

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(pdata->pci_dev->intr_handle);

	/* phy start*/
	pdata->phy_if.phy_start(pdata);
	axgbe_dev_enable_tx(dev);
	axgbe_dev_enable_rx(dev);

	rte_bit_relaxed_clear32(AXGBE_STOPPED, &pdata->dev_state);
	rte_bit_relaxed_clear32(AXGBE_DOWN, &pdata->dev_state);

	axgbe_set_rx_function(dev);
	axgbe_set_tx_function(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
static int
axgbe_dev_stop(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	rte_intr_disable(pdata->pci_dev->intr_handle);

	if (rte_bit_relaxed_get32(AXGBE_STOPPED, &pdata->dev_state))
		return 0;

	rte_bit_relaxed_set32(AXGBE_STOPPED, &pdata->dev_state);
	axgbe_dev_disable_tx(dev);
	axgbe_dev_disable_rx(dev);

	pdata->phy_if.phy_stop(pdata);
	pdata->hw_if.exit(pdata);
	memset(&dev->data->dev_link, 0, sizeof(struct rte_eth_link));
	rte_bit_relaxed_set32(AXGBE_DOWN, &pdata->dev_state);

	return 0;
}

static int
axgbe_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PR, 1);

	return 0;
}

static int
axgbe_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PR, 0);

	return 0;
}

static int
axgbe_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (AXGMAC_IOREAD_BITS(pdata, MAC_PFR, PM))
		return 0;
	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PM, 1);

	return 0;
}

static int
axgbe_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (!AXGMAC_IOREAD_BITS(pdata, MAC_PFR, PM))
		return 0;
	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PM, 0);

	return 0;
}

static int
axgbe_dev_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	/* Set Default MAC Addr */
	axgbe_set_mac_addn_addr(pdata, (u8 *)mac_addr, 0);

	return 0;
}

static int
axgbe_dev_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
			      uint32_t index, uint32_t pool __rte_unused)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;

	if (index > hw_feat->addn_mac) {
		PMD_DRV_LOG(ERR, "Invalid Index %d\n", index);
		return -EINVAL;
	}
	axgbe_set_mac_addn_addr(pdata, (u8 *)mac_addr, index);
	return 0;
}

static int
axgbe_dev_rss_reta_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_reta_entry64 *reta_conf,
			  uint16_t reta_size)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i, idx, shift;
	int ret;

	if (!pdata->rss_enable) {
		PMD_DRV_LOG(ERR, "RSS not enabled\n");
		return -ENOTSUP;
	}

	if (reta_size == 0 || reta_size > AXGBE_RSS_MAX_TABLE_SIZE) {
		PMD_DRV_LOG(ERR, "reta_size %d is not supported\n", reta_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) == 0)
			continue;
		pdata->rss_table[i] = reta_conf[idx].reta[shift];
	}

	/* Program the lookup table */
	ret = axgbe_write_rss_lookup_table(pdata);
	return ret;
}

static int
axgbe_dev_rss_reta_query(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i, idx, shift;

	if (!pdata->rss_enable) {
		PMD_DRV_LOG(ERR, "RSS not enabled\n");
		return -ENOTSUP;
	}

	if (reta_size == 0 || reta_size > AXGBE_RSS_MAX_TABLE_SIZE) {
		PMD_DRV_LOG(ERR, "reta_size %d is not supported\n", reta_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) == 0)
			continue;
		reta_conf[idx].reta[shift] = pdata->rss_table[i];
	}
	return 0;
}

static int
axgbe_dev_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	int ret;

	if (!pdata->rss_enable) {
		PMD_DRV_LOG(ERR, "RSS not enabled\n");
		return -ENOTSUP;
	}

	if (rss_conf == NULL) {
		PMD_DRV_LOG(ERR, "rss_conf value isn't valid\n");
		return -EINVAL;
	}

	if (rss_conf->rss_key != NULL &&
	    rss_conf->rss_key_len == AXGBE_RSS_HASH_KEY_SIZE) {
		rte_memcpy(pdata->rss_key, rss_conf->rss_key,
		       AXGBE_RSS_HASH_KEY_SIZE);
		/* Program the hash key */
		ret = axgbe_write_rss_hash_key(pdata);
		if (ret != 0)
			return ret;
	}

	pdata->rss_hf = rss_conf->rss_hf & AXGBE_RSS_OFFLOAD;

	if (pdata->rss_hf & (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_IPV6))
		AXGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, IP2TE, 1);
	if (pdata->rss_hf &
	    (RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV6_TCP))
		AXGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, TCP4TE, 1);
	if (pdata->rss_hf &
	    (RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV6_UDP))
		AXGMAC_SET_BITS(pdata->rss_options, MAC_RSSCR, UDP4TE, 1);

	/* Set the RSS options */
	AXGMAC_IOWRITE(pdata, MAC_RSSCR, pdata->rss_options);

	return 0;
}

static int
axgbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	if (!pdata->rss_enable) {
		PMD_DRV_LOG(ERR, "RSS not enabled\n");
		return -ENOTSUP;
	}

	if (rss_conf == NULL) {
		PMD_DRV_LOG(ERR, "rss_conf value isn't valid\n");
		return -EINVAL;
	}

	if (rss_conf->rss_key != NULL &&
	    rss_conf->rss_key_len >= AXGBE_RSS_HASH_KEY_SIZE) {
		rte_memcpy(rss_conf->rss_key, pdata->rss_key,
		       AXGBE_RSS_HASH_KEY_SIZE);
	}
	rss_conf->rss_key_len = AXGBE_RSS_HASH_KEY_SIZE;
	rss_conf->rss_hf = pdata->rss_hf;
	return 0;
}

static int
axgbe_dev_reset(struct rte_eth_dev *dev)
{
	int ret = 0;

	ret = axgbe_dev_close(dev);
	if (ret)
		return ret;

	ret = eth_axgbe_dev_init(dev);

	return ret;
}

static void
axgbe_dev_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;

	if (index > hw_feat->addn_mac) {
		PMD_DRV_LOG(ERR, "Invalid Index %d\n", index);
		return;
	}
	axgbe_set_mac_addn_addr(pdata, NULL, index);
}

static int
axgbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
				      struct rte_ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;
	uint32_t index = 1; /* 0 is always default mac */
	uint32_t i;

	if (nb_mc_addr > hw_feat->addn_mac) {
		PMD_DRV_LOG(ERR, "Invalid Index %d\n", nb_mc_addr);
		return -EINVAL;
	}

	/* clear unicast addresses */
	for (i = 1; i < hw_feat->addn_mac; i++) {
		if (rte_is_zero_ether_addr(&dev->data->mac_addrs[i]))
			continue;
		memset(&dev->data->mac_addrs[i], 0,
		       sizeof(struct rte_ether_addr));
	}

	while (nb_mc_addr--)
		axgbe_set_mac_addn_addr(pdata, (u8 *)mc_addr_set++, index++);

	return 0;
}

static int
axgbe_dev_uc_hash_table_set(struct rte_eth_dev *dev,
			    struct rte_ether_addr *mac_addr, uint8_t add)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;

	if (!hw_feat->hash_table_size) {
		PMD_DRV_LOG(ERR, "MAC Hash Table not supported\n");
		return -ENOTSUP;
	}

	axgbe_set_mac_hash_table(pdata, (u8 *)mac_addr, add);

	if (pdata->uc_hash_mac_addr > 0) {
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HPF, 1);
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HUC, 1);
	} else {
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HPF, 0);
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HUC, 0);
	}
	return 0;
}

static int
axgbe_dev_uc_all_hash_table_set(struct rte_eth_dev *dev, uint8_t add)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;
	uint32_t index;

	if (!hw_feat->hash_table_size) {
		PMD_DRV_LOG(ERR, "MAC Hash Table not supported\n");
		return -ENOTSUP;
	}

	for (index = 0; index < pdata->hash_table_count; index++) {
		if (add)
			pdata->uc_hash_table[index] = ~0;
		else
			pdata->uc_hash_table[index] = 0;

		PMD_DRV_LOG(DEBUG, "%s MAC hash table at Index %#x\n",
			    add ? "set" : "clear", index);

		AXGMAC_IOWRITE(pdata, MAC_HTR(index),
			       pdata->uc_hash_table[index]);
	}

	if (add) {
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HPF, 1);
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HUC, 1);
	} else {
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HPF, 0);
		AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, HUC, 0);
	}
	return 0;
}

/* return 0 means link status changed, -1 means not changed */
static int
axgbe_dev_link_update(struct rte_eth_dev *dev,
		      int wait_to_complete __rte_unused)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct rte_eth_link link;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	rte_delay_ms(800);

	pdata->phy_if.phy_status(pdata);

	memset(&link, 0, sizeof(struct rte_eth_link));
	link.link_duplex = pdata->phy.duplex;
	link.link_status = pdata->phy_link;
	link.link_speed = pdata->phy_speed;
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			      RTE_ETH_LINK_SPEED_FIXED);
	ret = rte_eth_linkstatus_set(dev, &link);
	if (ret == -1)
		PMD_DRV_LOG(ERR, "No change in link status\n");

	return ret;
}

static int
axgbe_dev_get_regs(struct rte_eth_dev *dev, struct rte_dev_reg_info *regs)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	if (regs->data == NULL) {
		regs->length = axgbe_regs_get_count(pdata);
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* Only full register dump is supported */
	if (regs->length &&
	    regs->length != (uint32_t)axgbe_regs_get_count(pdata))
		return -ENOTSUP;

	regs->version = pdata->pci_dev->id.vendor_id << 16 |
			pdata->pci_dev->id.device_id;
	axgbe_regs_dump(pdata, regs->data);
	return 0;
}
static void axgbe_read_mmc_stats(struct axgbe_port *pdata)
{
	struct axgbe_mmc_stats *stats = &pdata->mmc_stats;

	/* Freeze counters */
	AXGMAC_IOWRITE_BITS(pdata, MMC_CR, MCF, 1);

	/* Tx counters */
	stats->txoctetcount_gb +=
		AXGMAC_IOREAD(pdata, MMC_TXOCTETCOUNT_GB_LO);
	stats->txoctetcount_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXOCTETCOUNT_GB_HI) << 32);

	stats->txframecount_gb +=
		AXGMAC_IOREAD(pdata, MMC_TXFRAMECOUNT_GB_LO);
	stats->txframecount_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXFRAMECOUNT_GB_HI) << 32);

	stats->txbroadcastframes_g +=
		AXGMAC_IOREAD(pdata, MMC_TXBROADCASTFRAMES_G_LO);
	stats->txbroadcastframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXBROADCASTFRAMES_G_HI) << 32);

	stats->txmulticastframes_g +=
		AXGMAC_IOREAD(pdata, MMC_TXMULTICASTFRAMES_G_LO);
	stats->txmulticastframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXMULTICASTFRAMES_G_HI) << 32);

	stats->tx64octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_TX64OCTETS_GB_LO);
	stats->tx64octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TX64OCTETS_GB_HI) << 32);

	stats->tx65to127octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_TX65TO127OCTETS_GB_LO);
	stats->tx65to127octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TX65TO127OCTETS_GB_HI) << 32);

	stats->tx128to255octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_TX128TO255OCTETS_GB_LO);
	stats->tx128to255octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TX128TO255OCTETS_GB_HI) << 32);

	stats->tx256to511octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_TX256TO511OCTETS_GB_LO);
	stats->tx256to511octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TX256TO511OCTETS_GB_HI) << 32);

	stats->tx512to1023octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_TX512TO1023OCTETS_GB_LO);
	stats->tx512to1023octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TX512TO1023OCTETS_GB_HI) << 32);

	stats->tx1024tomaxoctets_gb +=
		AXGMAC_IOREAD(pdata, MMC_TX1024TOMAXOCTETS_GB_LO);
	stats->tx1024tomaxoctets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TX1024TOMAXOCTETS_GB_HI) << 32);

	stats->txunicastframes_gb +=
		AXGMAC_IOREAD(pdata, MMC_TXUNICASTFRAMES_GB_LO);
	stats->txunicastframes_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXUNICASTFRAMES_GB_HI) << 32);

	stats->txmulticastframes_gb +=
		AXGMAC_IOREAD(pdata, MMC_TXMULTICASTFRAMES_GB_LO);
	stats->txmulticastframes_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXMULTICASTFRAMES_GB_HI) << 32);

	stats->txbroadcastframes_g +=
		AXGMAC_IOREAD(pdata, MMC_TXBROADCASTFRAMES_GB_LO);
	stats->txbroadcastframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXBROADCASTFRAMES_GB_HI) << 32);

	stats->txunderflowerror +=
		AXGMAC_IOREAD(pdata, MMC_TXUNDERFLOWERROR_LO);
	stats->txunderflowerror +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXUNDERFLOWERROR_HI) << 32);

	stats->txoctetcount_g +=
		AXGMAC_IOREAD(pdata, MMC_TXOCTETCOUNT_G_LO);
	stats->txoctetcount_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXOCTETCOUNT_G_HI) << 32);

	stats->txframecount_g +=
		AXGMAC_IOREAD(pdata, MMC_TXFRAMECOUNT_G_LO);
	stats->txframecount_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXFRAMECOUNT_G_HI) << 32);

	stats->txpauseframes +=
		AXGMAC_IOREAD(pdata, MMC_TXPAUSEFRAMES_LO);
	stats->txpauseframes +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXPAUSEFRAMES_HI) << 32);

	stats->txvlanframes_g +=
		AXGMAC_IOREAD(pdata, MMC_TXVLANFRAMES_G_LO);
	stats->txvlanframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_TXVLANFRAMES_G_HI) << 32);

	/* Rx counters */
	stats->rxframecount_gb +=
		AXGMAC_IOREAD(pdata, MMC_RXFRAMECOUNT_GB_LO);
	stats->rxframecount_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXFRAMECOUNT_GB_HI) << 32);

	stats->rxoctetcount_gb +=
		AXGMAC_IOREAD(pdata, MMC_RXOCTETCOUNT_GB_LO);
	stats->rxoctetcount_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXOCTETCOUNT_GB_HI) << 32);

	stats->rxoctetcount_g +=
		AXGMAC_IOREAD(pdata, MMC_RXOCTETCOUNT_G_LO);
	stats->rxoctetcount_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXOCTETCOUNT_G_HI) << 32);

	stats->rxbroadcastframes_g +=
		AXGMAC_IOREAD(pdata, MMC_RXBROADCASTFRAMES_G_LO);
	stats->rxbroadcastframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXBROADCASTFRAMES_G_HI) << 32);

	stats->rxmulticastframes_g +=
		AXGMAC_IOREAD(pdata, MMC_RXMULTICASTFRAMES_G_LO);
	stats->rxmulticastframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXMULTICASTFRAMES_G_HI) << 32);

	stats->rxcrcerror +=
		AXGMAC_IOREAD(pdata, MMC_RXCRCERROR_LO);
	stats->rxcrcerror +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXCRCERROR_HI) << 32);

	stats->rxrunterror +=
		AXGMAC_IOREAD(pdata, MMC_RXRUNTERROR);

	stats->rxjabbererror +=
		AXGMAC_IOREAD(pdata, MMC_RXJABBERERROR);

	stats->rxundersize_g +=
		AXGMAC_IOREAD(pdata, MMC_RXUNDERSIZE_G);

	stats->rxoversize_g +=
		AXGMAC_IOREAD(pdata, MMC_RXOVERSIZE_G);

	stats->rx64octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_RX64OCTETS_GB_LO);
	stats->rx64octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RX64OCTETS_GB_HI) << 32);

	stats->rx65to127octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_RX65TO127OCTETS_GB_LO);
	stats->rx65to127octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RX65TO127OCTETS_GB_HI) << 32);

	stats->rx128to255octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_RX128TO255OCTETS_GB_LO);
	stats->rx128to255octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RX128TO255OCTETS_GB_HI) << 32);

	stats->rx256to511octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_RX256TO511OCTETS_GB_LO);
	stats->rx256to511octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RX256TO511OCTETS_GB_HI) << 32);

	stats->rx512to1023octets_gb +=
		AXGMAC_IOREAD(pdata, MMC_RX512TO1023OCTETS_GB_LO);
	stats->rx512to1023octets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RX512TO1023OCTETS_GB_HI) << 32);

	stats->rx1024tomaxoctets_gb +=
		AXGMAC_IOREAD(pdata, MMC_RX1024TOMAXOCTETS_GB_LO);
	stats->rx1024tomaxoctets_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RX1024TOMAXOCTETS_GB_HI) << 32);

	stats->rxunicastframes_g +=
		AXGMAC_IOREAD(pdata, MMC_RXUNICASTFRAMES_G_LO);
	stats->rxunicastframes_g +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXUNICASTFRAMES_G_HI) << 32);

	stats->rxlengtherror +=
		AXGMAC_IOREAD(pdata, MMC_RXLENGTHERROR_LO);
	stats->rxlengtherror +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXLENGTHERROR_HI) << 32);

	stats->rxoutofrangetype +=
		AXGMAC_IOREAD(pdata, MMC_RXOUTOFRANGETYPE_LO);
	stats->rxoutofrangetype +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXOUTOFRANGETYPE_HI) << 32);

	stats->rxpauseframes +=
		AXGMAC_IOREAD(pdata, MMC_RXPAUSEFRAMES_LO);
	stats->rxpauseframes +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXPAUSEFRAMES_HI) << 32);

	stats->rxfifooverflow +=
		AXGMAC_IOREAD(pdata, MMC_RXFIFOOVERFLOW_LO);
	stats->rxfifooverflow +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXFIFOOVERFLOW_HI) << 32);

	stats->rxvlanframes_gb +=
		AXGMAC_IOREAD(pdata, MMC_RXVLANFRAMES_GB_LO);
	stats->rxvlanframes_gb +=
	((uint64_t)AXGMAC_IOREAD(pdata, MMC_RXVLANFRAMES_GB_HI) << 32);

	stats->rxwatchdogerror +=
		AXGMAC_IOREAD(pdata, MMC_RXWATCHDOGERROR);

	/* Un-freeze counters */
	AXGMAC_IOWRITE_BITS(pdata, MMC_CR, MCF, 0);
}

static int
axgbe_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *stats,
		     unsigned int n)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int i;

	if (n < AXGBE_XSTATS_COUNT)
		return AXGBE_XSTATS_COUNT;

	axgbe_read_mmc_stats(pdata);

	for (i = 0; i < AXGBE_XSTATS_COUNT; i++) {
		stats[i].id = i;
		stats[i].value = *(u64 *)((uint8_t *)&pdata->mmc_stats +
				axgbe_xstats_strings[i].offset);
	}

	return AXGBE_XSTATS_COUNT;
}

static int
axgbe_dev_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
			   struct rte_eth_xstat_name *xstats_names,
			   unsigned int n)
{
	unsigned int i;

	if (n >= AXGBE_XSTATS_COUNT && xstats_names) {
		for (i = 0; i < AXGBE_XSTATS_COUNT; ++i) {
			snprintf(xstats_names[i].name,
				 RTE_ETH_XSTATS_NAME_SIZE, "%s",
				 axgbe_xstats_strings[i].name);
		}
	}

	return AXGBE_XSTATS_COUNT;
}

static int
axgbe_dev_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
			   uint64_t *values, unsigned int n)
{
	unsigned int i;
	uint64_t values_copy[AXGBE_XSTATS_COUNT];

	if (!ids) {
		struct axgbe_port *pdata = dev->data->dev_private;

		if (n < AXGBE_XSTATS_COUNT)
			return AXGBE_XSTATS_COUNT;

		axgbe_read_mmc_stats(pdata);

		for (i = 0; i < AXGBE_XSTATS_COUNT; i++) {
			values[i] = *(u64 *)((uint8_t *)&pdata->mmc_stats +
					axgbe_xstats_strings[i].offset);
		}

		return i;
	}

	axgbe_dev_xstats_get_by_id(dev, NULL, values_copy, AXGBE_XSTATS_COUNT);

	for (i = 0; i < n; i++) {
		if (ids[i] >= AXGBE_XSTATS_COUNT) {
			PMD_DRV_LOG(ERR, "id value isn't valid\n");
			return -1;
		}
		values[i] = values_copy[ids[i]];
	}
	return n;
}

static int
axgbe_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
				 const uint64_t *ids,
				 struct rte_eth_xstat_name *xstats_names,
				 unsigned int size)
{
	struct rte_eth_xstat_name xstats_names_copy[AXGBE_XSTATS_COUNT];
	unsigned int i;

	if (!ids)
		return axgbe_dev_xstats_get_names(dev, xstats_names, size);

	axgbe_dev_xstats_get_names(dev, xstats_names_copy, size);

	for (i = 0; i < size; i++) {
		if (ids[i] >= AXGBE_XSTATS_COUNT) {
			PMD_DRV_LOG(ERR, "id value isn't valid\n");
			return -1;
		}
		strcpy(xstats_names[i].name, xstats_names_copy[ids[i]].name);
	}
	return size;
}

static int
axgbe_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_mmc_stats *stats = &pdata->mmc_stats;

	/* MMC registers are configured for reset on read */
	axgbe_read_mmc_stats(pdata);

	/* Reset stats */
	memset(stats, 0, sizeof(*stats));

	return 0;
}

static int
axgbe_dev_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct axgbe_rx_queue *rxq;
	struct axgbe_tx_queue *txq;
	struct axgbe_port *pdata = dev->data->dev_private;
	struct axgbe_mmc_stats *mmc_stats = &pdata->mmc_stats;
	unsigned int i;

	axgbe_read_mmc_stats(pdata);

	stats->imissed = mmc_stats->rxfifooverflow;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq) {
			stats->q_ipackets[i] = rxq->pkts;
			stats->ipackets += rxq->pkts;
			stats->q_ibytes[i] = rxq->bytes;
			stats->ibytes += rxq->bytes;
			stats->rx_nombuf += rxq->rx_mbuf_alloc_failed;
			stats->q_errors[i] = rxq->errors
				+ rxq->rx_mbuf_alloc_failed;
			stats->ierrors += rxq->errors;
		} else {
			PMD_DRV_LOG(DEBUG, "Rx queue not setup for port %d\n",
					dev->data->port_id);
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq) {
			stats->q_opackets[i] = txq->pkts;
			stats->opackets += txq->pkts;
			stats->q_obytes[i] = txq->bytes;
			stats->obytes += txq->bytes;
			stats->oerrors += txq->errors;
		} else {
			PMD_DRV_LOG(DEBUG, "Tx queue not setup for port %d\n",
					dev->data->port_id);
		}
	}

	return 0;
}

static int
axgbe_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct axgbe_rx_queue *rxq;
	struct axgbe_tx_queue *txq;
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq) {
			rxq->pkts = 0;
			rxq->bytes = 0;
			rxq->errors = 0;
			rxq->rx_mbuf_alloc_failed = 0;
		} else {
			PMD_DRV_LOG(DEBUG, "Rx queue not setup for port %d\n",
					dev->data->port_id);
		}
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq) {
			txq->pkts = 0;
			txq->bytes = 0;
			txq->errors = 0;
		} else {
			PMD_DRV_LOG(DEBUG, "Tx queue not setup for port %d\n",
					dev->data->port_id);
		}
	}

	return 0;
}

static int
axgbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	dev_info->max_rx_queues = pdata->rx_ring_count;
	dev_info->max_tx_queues = pdata->tx_ring_count;
	dev_info->min_rx_bufsize = AXGBE_RX_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = AXGBE_RX_MAX_BUF_SIZE;
	dev_info->max_mac_addrs = pdata->hw_feat.addn_mac + 1;
	dev_info->max_hash_mac_addrs = pdata->hw_feat.hash_table_size;
	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10G;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		RTE_ETH_RX_OFFLOAD_VLAN_EXTEND |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM  |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM  |
		RTE_ETH_RX_OFFLOAD_SCATTER	  |
		RTE_ETH_RX_OFFLOAD_KEEP_CRC;

	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM  |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS  |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM   |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM;

	if (pdata->hw_feat.rss) {
		dev_info->flow_type_rss_offloads = AXGBE_RSS_OFFLOAD;
		dev_info->reta_size = pdata->hw_feat.hash_table_size;
		dev_info->hash_key_size =  AXGBE_RSS_HASH_KEY_SIZE;
	}

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = AXGBE_RX_FREE_THRESH,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = AXGBE_TX_FREE_THRESH,
	};

	return 0;
}

static int
axgbe_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct xgbe_fc_info fc = pdata->fc;
	unsigned int reg, reg_val = 0;

	reg = MAC_Q0TFCR;
	reg_val = AXGMAC_IOREAD(pdata, reg);
	fc.low_water[0] =  AXGMAC_MTL_IOREAD_BITS(pdata, 0, MTL_Q_RQFCR, RFA);
	fc.high_water[0] =  AXGMAC_MTL_IOREAD_BITS(pdata, 0, MTL_Q_RQFCR, RFD);
	fc.pause_time[0] = AXGMAC_GET_BITS(reg_val, MAC_Q0TFCR, PT);
	fc.autoneg = pdata->pause_autoneg;

	if (pdata->rx_pause && pdata->tx_pause)
		fc.mode = RTE_ETH_FC_FULL;
	else if (pdata->rx_pause)
		fc.mode = RTE_ETH_FC_RX_PAUSE;
	else if (pdata->tx_pause)
		fc.mode = RTE_ETH_FC_TX_PAUSE;
	else
		fc.mode = RTE_ETH_FC_NONE;

	fc_conf->high_water =  (1024 + (fc.low_water[0] << 9)) / 1024;
	fc_conf->low_water =  (1024 + (fc.high_water[0] << 9)) / 1024;
	fc_conf->pause_time = fc.pause_time[0];
	fc_conf->send_xon = fc.send_xon;
	fc_conf->mode = fc.mode;

	return 0;
}

static int
axgbe_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct xgbe_fc_info fc = pdata->fc;
	unsigned int reg, reg_val = 0;
	reg = MAC_Q0TFCR;

	pdata->pause_autoneg = fc_conf->autoneg;
	pdata->phy.pause_autoneg = pdata->pause_autoneg;
	fc.send_xon = fc_conf->send_xon;
	AXGMAC_MTL_IOWRITE_BITS(pdata, 0, MTL_Q_RQFCR, RFA,
			AXGMAC_FLOW_CONTROL_VALUE(1024 * fc_conf->high_water));
	AXGMAC_MTL_IOWRITE_BITS(pdata, 0, MTL_Q_RQFCR, RFD,
			AXGMAC_FLOW_CONTROL_VALUE(1024 * fc_conf->low_water));
	AXGMAC_SET_BITS(reg_val, MAC_Q0TFCR, PT, fc_conf->pause_time);
	AXGMAC_IOWRITE(pdata, reg, reg_val);
	fc.mode = fc_conf->mode;

	if (fc.mode == RTE_ETH_FC_FULL) {
		pdata->tx_pause = 1;
		pdata->rx_pause = 1;
	} else if (fc.mode == RTE_ETH_FC_RX_PAUSE) {
		pdata->tx_pause = 0;
		pdata->rx_pause = 1;
	} else if (fc.mode == RTE_ETH_FC_TX_PAUSE) {
		pdata->tx_pause = 1;
		pdata->rx_pause = 0;
	} else {
		pdata->tx_pause = 0;
		pdata->rx_pause = 0;
	}

	if (pdata->tx_pause != (unsigned int)pdata->phy.tx_pause)
		pdata->hw_if.config_tx_flow_control(pdata);

	if (pdata->rx_pause != (unsigned int)pdata->phy.rx_pause)
		pdata->hw_if.config_rx_flow_control(pdata);

	pdata->hw_if.config_flow_control(pdata);
	pdata->phy.tx_pause = pdata->tx_pause;
	pdata->phy.rx_pause = pdata->rx_pause;

	return 0;
}

static int
axgbe_priority_flow_ctrl_set(struct rte_eth_dev *dev,
		struct rte_eth_pfc_conf *pfc_conf)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct xgbe_fc_info fc = pdata->fc;
	uint8_t tc_num;

	tc_num = pdata->pfc_map[pfc_conf->priority];

	if (pfc_conf->priority >= pdata->hw_feat.tc_cnt) {
		PMD_INIT_LOG(ERR, "Max supported  traffic class: %d\n",
				pdata->hw_feat.tc_cnt);
	return -EINVAL;
	}

	pdata->pause_autoneg = pfc_conf->fc.autoneg;
	pdata->phy.pause_autoneg = pdata->pause_autoneg;
	fc.send_xon = pfc_conf->fc.send_xon;
	AXGMAC_MTL_IOWRITE_BITS(pdata, tc_num, MTL_Q_RQFCR, RFA,
		AXGMAC_FLOW_CONTROL_VALUE(1024 * pfc_conf->fc.high_water));
	AXGMAC_MTL_IOWRITE_BITS(pdata, tc_num, MTL_Q_RQFCR, RFD,
		AXGMAC_FLOW_CONTROL_VALUE(1024 * pfc_conf->fc.low_water));

	switch (tc_num) {
	case 0:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM0R,
				PSTC0, pfc_conf->fc.pause_time);
		break;
	case 1:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM0R,
				PSTC1, pfc_conf->fc.pause_time);
		break;
	case 2:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM0R,
				PSTC2, pfc_conf->fc.pause_time);
		break;
	case 3:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM0R,
				PSTC3, pfc_conf->fc.pause_time);
		break;
	case 4:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM1R,
				PSTC4, pfc_conf->fc.pause_time);
		break;
	case 5:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM1R,
				PSTC5, pfc_conf->fc.pause_time);
		break;
	case 7:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM1R,
				PSTC6, pfc_conf->fc.pause_time);
		break;
	case 6:
		AXGMAC_IOWRITE_BITS(pdata, MTL_TCPM1R,
				PSTC7, pfc_conf->fc.pause_time);
		break;
	}

	fc.mode = pfc_conf->fc.mode;

	if (fc.mode == RTE_ETH_FC_FULL) {
		pdata->tx_pause = 1;
		pdata->rx_pause = 1;
		AXGMAC_IOWRITE_BITS(pdata, MAC_RFCR, PFCE, 1);
	} else if (fc.mode == RTE_ETH_FC_RX_PAUSE) {
		pdata->tx_pause = 0;
		pdata->rx_pause = 1;
		AXGMAC_IOWRITE_BITS(pdata, MAC_RFCR, PFCE, 1);
	} else if (fc.mode == RTE_ETH_FC_TX_PAUSE) {
		pdata->tx_pause = 1;
		pdata->rx_pause = 0;
		AXGMAC_IOWRITE_BITS(pdata, MAC_RFCR, PFCE, 0);
	} else {
		pdata->tx_pause = 0;
		pdata->rx_pause = 0;
		AXGMAC_IOWRITE_BITS(pdata, MAC_RFCR, PFCE, 0);
	}

	if (pdata->tx_pause != (unsigned int)pdata->phy.tx_pause)
		pdata->hw_if.config_tx_flow_control(pdata);

	if (pdata->rx_pause != (unsigned int)pdata->phy.rx_pause)
		pdata->hw_if.config_rx_flow_control(pdata);
	pdata->hw_if.config_flow_control(pdata);
	pdata->phy.tx_pause = pdata->tx_pause;
	pdata->phy.rx_pause = pdata->rx_pause;

	return 0;
}

void
axgbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct   axgbe_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];
	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_desc;
	qinfo->conf.rx_free_thresh = rxq->free_thresh;
}

void
axgbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct  axgbe_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];
	qinfo->nb_desc = txq->nb_desc;
	qinfo->conf.tx_free_thresh = txq->free_thresh;
}
const uint32_t *
axgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L2_ETHER_VLAN,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == axgbe_recv_pkts)
		return ptypes;
	return NULL;
}

static int axgb_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int val;

	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port %d must be stopped before configuration",
				dev->data->port_id);
		return -EBUSY;
	}
	val = mtu > RTE_ETHER_MTU ? 1 : 0;
	AXGMAC_IOWRITE_BITS(pdata, MAC_RCR, JE, val);

	return 0;
}

static void
axgbe_update_tstamp_time(struct axgbe_port *pdata,
		unsigned int sec, unsigned int nsec, int addsub)
{
	unsigned int count = 100;
	uint32_t sub_val = 0;
	uint32_t sub_val_sec = 0xFFFFFFFF;
	uint32_t sub_val_nsec = 0x3B9ACA00;

	if (addsub) {
		if (sec)
			sub_val = sub_val_sec - (sec - 1);
		else
			sub_val = sec;

		AXGMAC_IOWRITE(pdata, MAC_STSUR, sub_val);
		sub_val = sub_val_nsec - nsec;
		AXGMAC_IOWRITE(pdata, MAC_STNUR, sub_val);
		AXGMAC_IOWRITE_BITS(pdata, MAC_STNUR, ADDSUB, 1);
	} else {
		AXGMAC_IOWRITE(pdata, MAC_STSUR, sec);
		AXGMAC_IOWRITE_BITS(pdata, MAC_STNUR, ADDSUB, 0);
		AXGMAC_IOWRITE(pdata, MAC_STNUR, nsec);
	}
	AXGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSUPDT, 1);
	/* Wait for time update to complete */
	while (--count && AXGMAC_IOREAD_BITS(pdata, MAC_TSCR, TSUPDT))
		rte_delay_ms(1);
}

static inline uint64_t
div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline uint64_t
div_u64(uint64_t dividend, uint32_t divisor)
{
	uint32_t remainder;
	return div_u64_rem(dividend, divisor, &remainder);
}

static int
axgbe_adjfreq(struct axgbe_port *pdata, int64_t delta)
{
	uint64_t adjust;
	uint32_t addend, diff;
	unsigned int neg_adjust = 0;

	if (delta < 0) {
		neg_adjust = 1;
		delta = -delta;
	}
	adjust = (uint64_t)pdata->tstamp_addend;
	adjust *= delta;
	diff = (uint32_t)div_u64(adjust, 1000000000UL);
	addend = (neg_adjust) ? pdata->tstamp_addend - diff :
				pdata->tstamp_addend + diff;
	pdata->tstamp_addend = addend;
	axgbe_update_tstamp_addend(pdata, addend);
	return 0;
}

static int
axgbe_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct timespec timestamp_delta;

	axgbe_adjfreq(pdata, delta);
	pdata->systime_tc.nsec += delta;

	if (delta < 0) {
		delta = -delta;
		timestamp_delta = rte_ns_to_timespec(delta);
		axgbe_update_tstamp_time(pdata, timestamp_delta.tv_sec,
				timestamp_delta.tv_nsec, 1);
	} else {
		timestamp_delta = rte_ns_to_timespec(delta);
		axgbe_update_tstamp_time(pdata, timestamp_delta.tv_sec,
				timestamp_delta.tv_nsec, 0);
	}
	return 0;
}

static int
axgbe_timesync_read_time(struct rte_eth_dev *dev,
		struct timespec *timestamp)
{
	uint64_t nsec;
	struct axgbe_port *pdata = dev->data->dev_private;

	nsec = AXGMAC_IOREAD(pdata, MAC_STSR);
	nsec *= NSEC_PER_SEC;
	nsec += AXGMAC_IOREAD(pdata, MAC_STNR);
	*timestamp = rte_ns_to_timespec(nsec);
	return 0;
}
static int
axgbe_timesync_write_time(struct rte_eth_dev *dev,
				    const struct timespec *timestamp)
{
	unsigned int count = 100;
	struct axgbe_port *pdata = dev->data->dev_private;

	AXGMAC_IOWRITE(pdata, MAC_STSUR, timestamp->tv_sec);
	AXGMAC_IOWRITE(pdata, MAC_STNUR, timestamp->tv_nsec);
	AXGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSUPDT, 1);
	/* Wait for time update to complete */
	while (--count && AXGMAC_IOREAD_BITS(pdata, MAC_TSCR, TSUPDT))
		rte_delay_ms(1);
	if (!count)
		PMD_DRV_LOG(ERR, "Timed out update timestamp\n");
	return 0;
}

static void
axgbe_update_tstamp_addend(struct axgbe_port *pdata,
		uint32_t addend)
{
	unsigned int count = 100;

	AXGMAC_IOWRITE(pdata, MAC_TSAR, addend);
	AXGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSADDREG, 1);

	/* Wait for addend update to complete */
	while (--count && AXGMAC_IOREAD_BITS(pdata, MAC_TSCR, TSADDREG))
		rte_delay_ms(1);
	if (!count)
		PMD_DRV_LOG(ERR, "Timed out updating timestamp addend register\n");
}

static void
axgbe_set_tstamp_time(struct axgbe_port *pdata, unsigned int sec,
		unsigned int nsec)
{
	unsigned int count = 100;

	/*System Time Sec Update*/
	AXGMAC_IOWRITE(pdata, MAC_STSUR, sec);
	/*System Time nanoSec Update*/
	AXGMAC_IOWRITE(pdata, MAC_STNUR, nsec);
	/*Initialize Timestamp*/
	AXGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSINIT, 1);

	/* Wait for time update to complete */
	while (--count && AXGMAC_IOREAD_BITS(pdata, MAC_TSCR, TSINIT))
		rte_delay_ms(1);
	if (!count)
		PMD_DRV_LOG(ERR, "Timed out initializing timestamp\n");
}

static int
axgbe_timesync_enable(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int mac_tscr = 0;
	uint64_t dividend;
	struct timespec timestamp;
	uint64_t nsec;

	/* Set one nano-second accuracy */
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSCTRLSSR, 1);

	/* Set fine timestamp update */
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSCFUPDT, 1);

	/* Overwrite earlier timestamps */
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TXTSSTSM, 1);

	AXGMAC_IOWRITE(pdata, MAC_TSCR, mac_tscr);

	/* Enabling processing of ptp over eth pkt */
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSIPENA, 1);
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSVER2ENA, 1);
	/* Enable timestamp for all pkts*/
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSENALL, 1);

	/* enabling timestamp */
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSENA, 1);
	AXGMAC_IOWRITE(pdata, MAC_TSCR, mac_tscr);

	/* Exit if timestamping is not enabled */
	if (!AXGMAC_GET_BITS(mac_tscr, MAC_TSCR, TSENA)) {
		PMD_DRV_LOG(ERR, "Exiting as timestamp is not enabled\n");
		return 0;
	}

	/* Sub-second Increment Value*/
	AXGMAC_IOWRITE_BITS(pdata, MAC_SSIR, SSINC, AXGBE_TSTAMP_SSINC);
	/* Sub-nanosecond Increment Value */
	AXGMAC_IOWRITE_BITS(pdata, MAC_SSIR, SNSINC, AXGBE_TSTAMP_SNSINC);

	pdata->ptpclk_rate = AXGBE_V2_PTP_CLOCK_FREQ;
	dividend = 50000000;
	dividend <<= 32;
	pdata->tstamp_addend = div_u64(dividend, pdata->ptpclk_rate);

	axgbe_update_tstamp_addend(pdata, pdata->tstamp_addend);
	axgbe_set_tstamp_time(pdata, 0, 0);

	/* Initialize the timecounter */
	memset(&pdata->systime_tc, 0, sizeof(struct rte_timecounter));

	pdata->systime_tc.cc_mask = AXGBE_CYCLECOUNTER_MASK;
	pdata->systime_tc.cc_shift = 0;
	pdata->systime_tc.nsec_mask = 0;

	PMD_DRV_LOG(DEBUG, "Initializing system time counter with realtime\n");

	/* Updating the counter once with clock real time */
	clock_gettime(CLOCK_REALTIME, &timestamp);
	nsec = rte_timespec_to_ns(&timestamp);
	nsec = rte_timecounter_update(&pdata->systime_tc, nsec);
	axgbe_set_tstamp_time(pdata, timestamp.tv_sec, timestamp.tv_nsec);
	return 0;
}

static int
axgbe_timesync_disable(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int mac_tscr = 0;

	/*disable timestamp for all pkts*/
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSENALL, 0);
	/*disable the addened register*/
	AXGMAC_IOWRITE_BITS(pdata, MAC_TSCR, TSADDREG, 0);
	/* disable timestamp update */
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSCFUPDT, 0);
	/*disable time stamp*/
	AXGMAC_SET_BITS(mac_tscr, MAC_TSCR, TSENA, 0);
	return 0;
}

static int
axgbe_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp, uint32_t flags)
{
	uint64_t nsec = 0;
	volatile union axgbe_rx_desc *desc;
	uint16_t idx, pmt;
	struct axgbe_rx_queue *rxq = *dev->data->rx_queues;

	idx = AXGBE_GET_DESC_IDX(rxq, rxq->cur);
	desc = &rxq->desc[idx];

	while (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, OWN))
		rte_delay_ms(1);
	if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_NORMAL_DESC3, CTXT)) {
		if (AXGMAC_GET_BITS_LE(desc->write.desc3, RX_CONTEXT_DESC3, TSA) &&
				!AXGMAC_GET_BITS_LE(desc->write.desc3,
					RX_CONTEXT_DESC3, TSD)) {
			pmt = AXGMAC_GET_BITS_LE(desc->write.desc3,
					RX_CONTEXT_DESC3, PMT);
			nsec = rte_le_to_cpu_32(desc->write.desc1);
			nsec *= NSEC_PER_SEC;
			nsec += rte_le_to_cpu_32(desc->write.desc0);
			if (nsec != 0xffffffffffffffffULL) {
				if (pmt == 0x01)
					*timestamp = rte_ns_to_timespec(nsec);
				PMD_DRV_LOG(DEBUG,
					"flags = 0x%x nsec = %"PRIu64"\n",
					flags, nsec);
			}
		}
	}

	return 0;
}

static int
axgbe_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp)
{
	uint64_t nsec;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int tx_snr, tx_ssr;

	rte_delay_us(5);
	if (pdata->vdata->tx_tstamp_workaround) {
		tx_snr = AXGMAC_IOREAD(pdata, MAC_TXSNR);
		tx_ssr = AXGMAC_IOREAD(pdata, MAC_TXSSR);

	} else {
		tx_ssr = AXGMAC_IOREAD(pdata, MAC_TXSSR);
		tx_snr = AXGMAC_IOREAD(pdata, MAC_TXSNR);
	}
	if (AXGMAC_GET_BITS(tx_snr, MAC_TXSNR, TXTSSTSMIS)) {
		PMD_DRV_LOG(DEBUG, "Waiting for TXTSSTSMIS\n");
		return 0;
	}
	nsec = tx_ssr;
	nsec *= NSEC_PER_SEC;
	nsec += tx_snr;
	PMD_DRV_LOG(DEBUG, "nsec = %"PRIu64" tx_ssr = %d tx_snr = %d\n",
			nsec, tx_ssr, tx_snr);
	*timestamp = rte_ns_to_timespec(nsec);
	return 0;
}

static int
axgbe_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vid, int on)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned long vid_bit, vid_idx;

	vid_bit = VLAN_TABLE_BIT(vid);
	vid_idx = VLAN_TABLE_IDX(vid);

	if (on) {
		PMD_DRV_LOG(DEBUG, "Set VLAN vid=%d for device = %s\n",
			    vid, pdata->eth_dev->device->name);
		pdata->active_vlans[vid_idx] |= vid_bit;
	} else {
		PMD_DRV_LOG(DEBUG, "Reset VLAN vid=%d for device = %s\n",
			    vid, pdata->eth_dev->device->name);
		pdata->active_vlans[vid_idx] &= ~vid_bit;
	}
	pdata->hw_if.update_vlan_hash_table(pdata);
	return 0;
}

static int
axgbe_vlan_tpid_set(struct rte_eth_dev *dev,
		    enum rte_vlan_type vlan_type,
		    uint16_t tpid)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	uint32_t reg = 0;
	uint32_t qinq = 0;

	qinq = AXGMAC_IOREAD_BITS(pdata, MAC_VLANTR, EDVLP);
	PMD_DRV_LOG(DEBUG, "EDVLP: qinq = 0x%x\n", qinq);

	switch (vlan_type) {
	case RTE_ETH_VLAN_TYPE_INNER:
		PMD_DRV_LOG(DEBUG, "RTE_ETH_VLAN_TYPE_INNER\n");
		if (qinq) {
			if (tpid != 0x8100 && tpid != 0x88a8)
				PMD_DRV_LOG(ERR,
					    "tag supported 0x8100/0x88A8\n");
			PMD_DRV_LOG(DEBUG, "qinq with inner tag\n");

			/*Enable Inner VLAN Tag */
			AXGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, ERIVLT, 1);
			reg = AXGMAC_IOREAD_BITS(pdata, MAC_VLANTR, ERIVLT);
			PMD_DRV_LOG(DEBUG, "bit ERIVLT = 0x%x\n", reg);

		} else {
			PMD_DRV_LOG(ERR,
				    "Inner type not supported in single tag\n");
		}
		break;
	case RTE_ETH_VLAN_TYPE_OUTER:
		PMD_DRV_LOG(DEBUG, "RTE_ETH_VLAN_TYPE_OUTER\n");
		if (qinq) {
			PMD_DRV_LOG(DEBUG, "double tagging is enabled\n");
			/*Enable outer VLAN tag*/
			AXGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, ERIVLT, 0);
			reg = AXGMAC_IOREAD_BITS(pdata, MAC_VLANTR, ERIVLT);
			PMD_DRV_LOG(DEBUG, "bit ERIVLT = 0x%x\n", reg);

			AXGMAC_IOWRITE_BITS(pdata, MAC_VLANIR, CSVL, 1);
			reg = AXGMAC_IOREAD_BITS(pdata, MAC_VLANIR, CSVL);
			PMD_DRV_LOG(DEBUG, "bit CSVL = 0x%x\n", reg);
		} else {
			if (tpid != 0x8100 && tpid != 0x88a8)
				PMD_DRV_LOG(ERR,
					    "tag supported 0x8100/0x88A8\n");
		}
		break;
	case RTE_ETH_VLAN_TYPE_MAX:
		PMD_DRV_LOG(ERR, "RTE_ETH_VLAN_TYPE_MAX\n");
		break;
	case RTE_ETH_VLAN_TYPE_UNKNOWN:
		PMD_DRV_LOG(ERR, "RTE_ETH_VLAN_TYPE_UNKNOWN\n");
		break;
	}
	return 0;
}

static void axgbe_vlan_extend_enable(struct axgbe_port *pdata)
{
	int qinq = 0;

	AXGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, EDVLP, 1);
	qinq = AXGMAC_IOREAD_BITS(pdata, MAC_VLANTR, EDVLP);
	PMD_DRV_LOG(DEBUG, "vlan double tag enabled EDVLP:qinq=0x%x\n", qinq);
}

static void axgbe_vlan_extend_disable(struct axgbe_port *pdata)
{
	int qinq = 0;

	AXGMAC_IOWRITE_BITS(pdata, MAC_VLANTR, EDVLP, 0);
	qinq = AXGMAC_IOREAD_BITS(pdata, MAC_VLANTR, EDVLP);
	PMD_DRV_LOG(DEBUG, "vlan double tag disable EDVLP:qinq=0x%x\n", qinq);
}

static int
axgbe_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct axgbe_port *pdata = dev->data->dev_private;

	/* Indicate that VLAN Tx CTAGs come from context descriptors */
	AXGMAC_IOWRITE_BITS(pdata, MAC_VLANIR, CSVL, 0);
	AXGMAC_IOWRITE_BITS(pdata, MAC_VLANIR, VLTI, 1);

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) {
			PMD_DRV_LOG(DEBUG, "Strip ON for device = %s\n",
				    pdata->eth_dev->device->name);
			pdata->hw_if.enable_rx_vlan_stripping(pdata);
		} else {
			PMD_DRV_LOG(DEBUG, "Strip OFF for device = %s\n",
				    pdata->eth_dev->device->name);
			pdata->hw_if.disable_rx_vlan_stripping(pdata);
		}
	}
	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) {
			PMD_DRV_LOG(DEBUG, "Filter ON for device = %s\n",
				    pdata->eth_dev->device->name);
			pdata->hw_if.enable_rx_vlan_filtering(pdata);
		} else {
			PMD_DRV_LOG(DEBUG, "Filter OFF for device = %s\n",
				    pdata->eth_dev->device->name);
			pdata->hw_if.disable_rx_vlan_filtering(pdata);
		}
	}
	if (mask & RTE_ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND) {
			PMD_DRV_LOG(DEBUG, "enabling vlan extended mode\n");
			axgbe_vlan_extend_enable(pdata);
			/* Set global registers with default ethertype*/
			axgbe_vlan_tpid_set(dev, RTE_ETH_VLAN_TYPE_OUTER,
					    RTE_ETHER_TYPE_VLAN);
			axgbe_vlan_tpid_set(dev, RTE_ETH_VLAN_TYPE_INNER,
					    RTE_ETHER_TYPE_VLAN);
		} else {
			PMD_DRV_LOG(DEBUG, "disabling vlan extended mode\n");
			axgbe_vlan_extend_disable(pdata);
		}
	}
	return 0;
}

static void axgbe_get_all_hw_features(struct axgbe_port *pdata)
{
	unsigned int mac_hfr0, mac_hfr1, mac_hfr2, mac_hfr3;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;

	mac_hfr0 = AXGMAC_IOREAD(pdata, MAC_HWF0R);
	mac_hfr1 = AXGMAC_IOREAD(pdata, MAC_HWF1R);
	mac_hfr2 = AXGMAC_IOREAD(pdata, MAC_HWF2R);
	mac_hfr3 = AXGMAC_IOREAD(pdata, MAC_HWF3R);

	memset(hw_feat, 0, sizeof(*hw_feat));

	hw_feat->version = AXGMAC_IOREAD(pdata, MAC_VR);

	/* Hardware feature register 0 */
	hw_feat->gmii        = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, GMIISEL);
	hw_feat->vlhash      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, VLHASH);
	hw_feat->sma         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, SMASEL);
	hw_feat->rwk         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, RWKSEL);
	hw_feat->mgk         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, MGKSEL);
	hw_feat->mmc         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, MMCSEL);
	hw_feat->aoe         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, ARPOFFSEL);
	hw_feat->ts          = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, TSSEL);
	hw_feat->eee         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, EEESEL);
	hw_feat->tx_coe      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, TXCOESEL);
	hw_feat->rx_coe      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, RXCOESEL);
	hw_feat->addn_mac    = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R,
					      ADDMACADRSEL);
	hw_feat->ts_src      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, TSSTSSEL);
	hw_feat->sa_vlan_ins = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, SAVLANINS);

	/* Hardware feature register 1 */
	hw_feat->rx_fifo_size  = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						RXFIFOSIZE);
	hw_feat->tx_fifo_size  = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						TXFIFOSIZE);
	hw_feat->adv_ts_hi     = AXGMAC_GET_BITS(mac_hfr1,
						 MAC_HWF1R, ADVTHWORD);
	hw_feat->dma_width     = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, ADDR64);
	hw_feat->dcb           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, DCBEN);
	hw_feat->sph           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, SPHEN);
	hw_feat->tso           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, TSOEN);
	hw_feat->dma_debug     = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, DBGMEMA);
	hw_feat->rss           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, RSSEN);
	hw_feat->tc_cnt	       = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, NUMTC);
	hw_feat->hash_table_size = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						  HASHTBLSZ);
	hw_feat->l3l4_filter_num = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						  L3L4FNUM);

	/* Hardware feature register 2 */
	hw_feat->rx_q_cnt     = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, RXQCNT);
	hw_feat->tx_q_cnt     = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, TXQCNT);
	hw_feat->rx_ch_cnt    = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, RXCHCNT);
	hw_feat->tx_ch_cnt    = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, TXCHCNT);
	hw_feat->pps_out_num  = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, PPSOUTNUM);
	hw_feat->aux_snap_num = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R,
						AUXSNAPNUM);

	/* Hardware feature register 3 */
	hw_feat->tx_q_vlan_tag_ins  = AXGMAC_GET_BITS(mac_hfr3,
						      MAC_HWF3R, CBTISEL);
	hw_feat->no_of_vlan_extn    = AXGMAC_GET_BITS(mac_hfr3,
						      MAC_HWF3R, NRVF);

	/* Translate the Hash Table size into actual number */
	switch (hw_feat->hash_table_size) {
	case 0:
		break;
	case 1:
		hw_feat->hash_table_size = 64;
		break;
	case 2:
		hw_feat->hash_table_size = 128;
		break;
	case 3:
		hw_feat->hash_table_size = 256;
		break;
	}

	/* Translate the address width setting into actual number */
	switch (hw_feat->dma_width) {
	case 0:
		hw_feat->dma_width = 32;
		break;
	case 1:
		hw_feat->dma_width = 40;
		break;
	case 2:
		hw_feat->dma_width = 48;
		break;
	default:
		hw_feat->dma_width = 32;
	}

	/* The Queue, Channel and TC counts are zero based so increment them
	 * to get the actual number
	 */
	hw_feat->rx_q_cnt++;
	hw_feat->tx_q_cnt++;
	hw_feat->rx_ch_cnt++;
	hw_feat->tx_ch_cnt++;
	hw_feat->tc_cnt++;

	/* Translate the fifo sizes into actual numbers */
	hw_feat->rx_fifo_size = 1 << (hw_feat->rx_fifo_size + 7);
	hw_feat->tx_fifo_size = 1 << (hw_feat->tx_fifo_size + 7);
}

static void axgbe_init_all_fptrs(struct axgbe_port *pdata)
{
	axgbe_init_function_ptrs_dev(&pdata->hw_if);
	axgbe_init_function_ptrs_phy(&pdata->phy_if);
	axgbe_init_function_ptrs_i2c(&pdata->i2c_if);
	pdata->vdata->init_function_ptrs_phy_impl(&pdata->phy_if);
}

static void axgbe_set_counts(struct axgbe_port *pdata)
{
	/* Set all the function pointers */
	axgbe_init_all_fptrs(pdata);

	/* Populate the hardware features */
	axgbe_get_all_hw_features(pdata);

	/* Set default max values if not provided */
	if (!pdata->tx_max_channel_count)
		pdata->tx_max_channel_count = pdata->hw_feat.tx_ch_cnt;
	if (!pdata->rx_max_channel_count)
		pdata->rx_max_channel_count = pdata->hw_feat.rx_ch_cnt;

	if (!pdata->tx_max_q_count)
		pdata->tx_max_q_count = pdata->hw_feat.tx_q_cnt;
	if (!pdata->rx_max_q_count)
		pdata->rx_max_q_count = pdata->hw_feat.rx_q_cnt;

	/* Calculate the number of Tx and Rx rings to be created
	 *  -Tx (DMA) Channels map 1-to-1 to Tx Queues so set
	 *   the number of Tx queues to the number of Tx channels
	 *   enabled
	 *  -Rx (DMA) Channels do not map 1-to-1 so use the actual
	 *   number of Rx queues or maximum allowed
	 */
	pdata->tx_ring_count = RTE_MIN(pdata->hw_feat.tx_ch_cnt,
				     pdata->tx_max_channel_count);
	pdata->tx_ring_count = RTE_MIN(pdata->tx_ring_count,
				     pdata->tx_max_q_count);

	pdata->tx_q_count = pdata->tx_ring_count;

	pdata->rx_ring_count = RTE_MIN(pdata->hw_feat.rx_ch_cnt,
				     pdata->rx_max_channel_count);

	pdata->rx_q_count = RTE_MIN(pdata->hw_feat.rx_q_cnt,
				  pdata->rx_max_q_count);
}

static void axgbe_default_config(struct axgbe_port *pdata)
{
	pdata->pblx8 = DMA_PBL_X8_ENABLE;
	pdata->tx_sf_mode = MTL_TSF_ENABLE;
	pdata->tx_threshold = MTL_TX_THRESHOLD_64;
	pdata->tx_pbl = DMA_PBL_32;
	pdata->tx_osp_mode = DMA_OSP_ENABLE;
	pdata->rx_sf_mode = MTL_RSF_ENABLE;
	pdata->rx_threshold = MTL_RX_THRESHOLD_64;
	pdata->rx_pbl = DMA_PBL_32;
	pdata->pause_autoneg = 1;
	pdata->tx_pause = 0;
	pdata->rx_pause = 0;
	pdata->phy_speed = SPEED_UNKNOWN;
	pdata->power_down = 0;
}

/* Used in dev_start by primary process and then
 * in dev_init by secondary process when attaching to an existing ethdev.
 */
void
axgbe_set_tx_function(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	dev->tx_pkt_burst = &axgbe_xmit_pkts;

	if (pdata->multi_segs_tx)
		dev->tx_pkt_burst = &axgbe_xmit_pkts_seg;
#ifdef RTE_ARCH_X86
	struct axgbe_tx_queue *txq = dev->data->tx_queues[0];
	if (!txq->vector_disable &&
			rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)
		dev->tx_pkt_burst = &axgbe_xmit_pkts_vec;
#endif
}

void
axgbe_set_rx_function(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *dev_data = dev->data;
	uint16_t max_pkt_len;
	struct axgbe_port *pdata;

	pdata = dev->data->dev_private;
	max_pkt_len = dev_data->mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
	if ((dev_data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER) ||
			max_pkt_len > pdata->rx_buf_size)
		dev_data->scattered_rx = 1;
	/*  Scatter Rx handling */
	if (dev_data->scattered_rx)
		dev->rx_pkt_burst = &eth_axgbe_recv_scattered_pkts;
	else
		dev->rx_pkt_burst = &axgbe_recv_pkts;
}

/*
 * It returns 0 on success.
 */
static int
eth_axgbe_dev_init(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata;
	struct rte_pci_device *pci_dev;
	uint32_t reg, mac_lo, mac_hi;
	uint32_t len;
	int ret;

	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
	unsigned char cpu_family = 0, cpu_model = 0;

	eth_dev->dev_ops = &axgbe_eth_dev_ops;

	eth_dev->rx_descriptor_status = axgbe_dev_rx_descriptor_status;
	eth_dev->tx_descriptor_status = axgbe_dev_tx_descriptor_status;

	eth_dev->tx_pkt_burst = &axgbe_xmit_pkts;
	eth_dev->rx_pkt_burst = &axgbe_recv_pkts;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		axgbe_set_tx_function(eth_dev);
		axgbe_set_rx_function(eth_dev);
		return 0;
	}

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	pdata = eth_dev->data->dev_private;
	/* initial state */
	rte_bit_relaxed_set32(AXGBE_DOWN, &pdata->dev_state);
	rte_bit_relaxed_set32(AXGBE_STOPPED, &pdata->dev_state);
	pdata->eth_dev = eth_dev;

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	pdata->pci_dev = pci_dev;

	pdata->xgmac_regs =
		(void *)pci_dev->mem_resource[AXGBE_AXGMAC_BAR].addr;
	pdata->xprop_regs = (void *)((uint8_t *)pdata->xgmac_regs
				     + AXGBE_MAC_PROP_OFFSET);
	pdata->xi2c_regs = (void *)((uint8_t *)pdata->xgmac_regs
				    + AXGBE_I2C_CTRL_OFFSET);
	pdata->xpcs_regs = (void *)pci_dev->mem_resource[AXGBE_XPCS_BAR].addr;

	/* version specific driver data*/
	if (pci_dev->id.device_id == AMD_PCI_AXGBE_DEVICE_V2A)
		pdata->vdata = &axgbe_v2a;
	else
		pdata->vdata = &axgbe_v2b;

	/*
	 * Use CPUID to get Family and model ID to identify the CPU
	 */
	__cpuid(0x0, eax, ebx, ecx, edx);

	if (ebx == CPUID_VENDOR_AuthenticAMD_ebx &&
		edx == CPUID_VENDOR_AuthenticAMD_edx &&
		ecx == CPUID_VENDOR_AuthenticAMD_ecx) {
		int unknown_cpu = 0;
		eax = 0, ebx = 0, ecx = 0, edx = 0;

		__cpuid(0x1, eax, ebx, ecx, edx);

		cpu_family = ((GET_BITS(eax, 8, 4)) + (GET_BITS(eax, 20, 8)));
		cpu_model = ((GET_BITS(eax, 4, 4)) | (((GET_BITS(eax, 16, 4)) << 4) & 0xF0));

		switch (cpu_family) {
		case Fam17h:
		/* V1000/R1000 */
		if (cpu_model >= 0x10 && cpu_model <= 0x1F) {
			pdata->xpcs_window_def_reg = PCS_V2_RV_WINDOW_DEF;
			pdata->xpcs_window_sel_reg = PCS_V2_RV_WINDOW_SELECT;
		/* EPYC 3000 */
		} else if (cpu_model >= 0x01 && cpu_model <= 0x0F) {
			pdata->xpcs_window_def_reg = PCS_V2_WINDOW_DEF;
			pdata->xpcs_window_sel_reg = PCS_V2_WINDOW_SELECT;
		} else {
			unknown_cpu = 1;
		}
		break;
		case Fam19h:
		/* V3000 (Yellow Carp) */
		if (cpu_model >= 0x44 && cpu_model <= 0x47) {
			pdata->xpcs_window_def_reg = PCS_V2_YC_WINDOW_DEF;
			pdata->xpcs_window_sel_reg = PCS_V2_YC_WINDOW_SELECT;

			/* Yellow Carp devices do not need cdr workaround */
			pdata->vdata->an_cdr_workaround = 0;
		} else {
			unknown_cpu = 1;
		}
		break;
		default:
			unknown_cpu = 1;
			break;
		}
		if (unknown_cpu) {
			PMD_DRV_LOG(ERR, "Unknown CPU family, no supported axgbe device found\n");
			return -ENODEV;
		}
	}

	/* Configure the PCS indirect addressing support */
	reg = XPCS32_IOREAD(pdata, pdata->xpcs_window_def_reg);
	pdata->xpcs_window = XPCS_GET_BITS(reg, PCS_V2_WINDOW_DEF, OFFSET);
	pdata->xpcs_window <<= 6;
	pdata->xpcs_window_size = XPCS_GET_BITS(reg, PCS_V2_WINDOW_DEF, SIZE);
	pdata->xpcs_window_size = 1 << (pdata->xpcs_window_size + 7);
	pdata->xpcs_window_mask = pdata->xpcs_window_size - 1;

	PMD_INIT_LOG(DEBUG,
		     "xpcs window :%x, size :%x, mask :%x ", pdata->xpcs_window,
		     pdata->xpcs_window_size, pdata->xpcs_window_mask);
	XP_IOWRITE(pdata, XP_INT_EN, 0x1fffff);

	/* Retrieve the MAC address */
	mac_lo = XP_IOREAD(pdata, XP_MAC_ADDR_LO);
	mac_hi = XP_IOREAD(pdata, XP_MAC_ADDR_HI);
	pdata->mac_addr.addr_bytes[0] = mac_lo & 0xff;
	pdata->mac_addr.addr_bytes[1] = (mac_lo >> 8) & 0xff;
	pdata->mac_addr.addr_bytes[2] = (mac_lo >> 16) & 0xff;
	pdata->mac_addr.addr_bytes[3] = (mac_lo >> 24) & 0xff;
	pdata->mac_addr.addr_bytes[4] = mac_hi & 0xff;
	pdata->mac_addr.addr_bytes[5] = (mac_hi >> 8)  &  0xff;

	len = RTE_ETHER_ADDR_LEN * AXGBE_MAX_MAC_ADDRS;
	eth_dev->data->mac_addrs = rte_zmalloc("axgbe_mac_addr", len, 0);

	if (!eth_dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR,
			     "Failed to alloc %u bytes needed to "
			     "store MAC addresses", len);
		return -ENOMEM;
	}

	/* Allocate memory for storing hash filter MAC addresses */
	len = RTE_ETHER_ADDR_LEN * AXGBE_MAX_HASH_MAC_ADDRS;
	eth_dev->data->hash_mac_addrs = rte_zmalloc("axgbe_hash_mac_addr",
						    len, 0);

	if (eth_dev->data->hash_mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to "
			     "store MAC addresses", len);
		return -ENOMEM;
	}

	if (!rte_is_valid_assigned_ether_addr(&pdata->mac_addr))
		rte_eth_random_addr(pdata->mac_addr.addr_bytes);

	/* Copy the permanent MAC address */
	rte_ether_addr_copy(&pdata->mac_addr, &eth_dev->data->mac_addrs[0]);

	/* Clock settings */
	pdata->sysclk_rate = AXGBE_V2_DMA_CLOCK_FREQ;
	pdata->ptpclk_rate = AXGBE_V2_PTP_CLOCK_FREQ;

	/* Set the DMA coherency values */
	pdata->coherent = 1;
	pdata->axdomain = AXGBE_DMA_OS_AXDOMAIN;
	pdata->arcache = AXGBE_DMA_OS_ARCACHE;
	pdata->awcache = AXGBE_DMA_OS_AWCACHE;

	/* Set the maximum channels and queues */
	reg = XP_IOREAD(pdata, XP_PROP_1);
	pdata->tx_max_channel_count = XP_GET_BITS(reg, XP_PROP_1, MAX_TX_DMA);
	pdata->rx_max_channel_count = XP_GET_BITS(reg, XP_PROP_1, MAX_RX_DMA);
	pdata->tx_max_q_count = XP_GET_BITS(reg, XP_PROP_1, MAX_TX_QUEUES);
	pdata->rx_max_q_count = XP_GET_BITS(reg, XP_PROP_1, MAX_RX_QUEUES);

	/* Set the hardware channel and queue counts */
	axgbe_set_counts(pdata);

	/* Set the maximum fifo amounts */
	reg = XP_IOREAD(pdata, XP_PROP_2);
	pdata->tx_max_fifo_size = XP_GET_BITS(reg, XP_PROP_2, TX_FIFO_SIZE);
	pdata->tx_max_fifo_size *= 16384;
	pdata->tx_max_fifo_size = RTE_MIN(pdata->tx_max_fifo_size,
					  pdata->vdata->tx_max_fifo_size);
	pdata->rx_max_fifo_size = XP_GET_BITS(reg, XP_PROP_2, RX_FIFO_SIZE);
	pdata->rx_max_fifo_size *= 16384;
	pdata->rx_max_fifo_size = RTE_MIN(pdata->rx_max_fifo_size,
					  pdata->vdata->rx_max_fifo_size);
	/* Issue software reset to DMA */
	ret = pdata->hw_if.exit(pdata);
	if (ret)
		PMD_DRV_LOG(ERR, "hw_if->exit EBUSY error\n");

	/* Set default configuration data */
	axgbe_default_config(pdata);

	/* Set default max values if not provided */
	if (!pdata->tx_max_fifo_size)
		pdata->tx_max_fifo_size = pdata->hw_feat.tx_fifo_size;
	if (!pdata->rx_max_fifo_size)
		pdata->rx_max_fifo_size = pdata->hw_feat.rx_fifo_size;

	pdata->tx_desc_count = AXGBE_MAX_RING_DESC;
	pdata->rx_desc_count = AXGBE_MAX_RING_DESC;
	pthread_mutex_init(&pdata->xpcs_mutex, NULL);
	pthread_mutex_init(&pdata->i2c_mutex, NULL);
	pthread_mutex_init(&pdata->an_mutex, NULL);
	pthread_mutex_init(&pdata->phy_mutex, NULL);

	ret = pdata->phy_if.phy_init(pdata);
	if (ret) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		return ret;
	}

	rte_intr_callback_register(pci_dev->intr_handle,
				   axgbe_dev_interrupt_handler,
				   (void *)eth_dev);
	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	return 0;
}

static int
axgbe_dev_close(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	axgbe_dev_clear_queues(eth_dev);

	/* disable uio intr before callback unregister */
	rte_intr_disable(pci_dev->intr_handle);
	rte_intr_callback_unregister(pci_dev->intr_handle,
				     axgbe_dev_interrupt_handler,
				     (void *)eth_dev);

	return 0;
}

static int eth_axgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct axgbe_port), eth_axgbe_dev_init);
}

static int eth_axgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, axgbe_dev_close);
}

static struct rte_pci_driver rte_axgbe_pmd = {
	.id_table = pci_id_axgbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_axgbe_pci_probe,
	.remove = eth_axgbe_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_axgbe, rte_axgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_axgbe, pci_id_axgbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_axgbe, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_LOG_REGISTER_SUFFIX(axgbe_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(axgbe_logtype_driver, driver, NOTICE);
