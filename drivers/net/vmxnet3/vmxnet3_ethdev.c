/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_dev.h>

#include "base/vmxnet3_defs.h"

#include "vmxnet3_ring.h"
#include "vmxnet3_logs.h"
#include "vmxnet3_ethdev.h"

#define PROCESS_SYS_EVENTS 0

#define	VMXNET3_TX_MAX_SEG	UINT8_MAX

#define VMXNET3_TX_OFFLOAD_CAP		\
	(DEV_TX_OFFLOAD_VLAN_INSERT |	\
	 DEV_TX_OFFLOAD_TCP_CKSUM |	\
	 DEV_TX_OFFLOAD_UDP_CKSUM |	\
	 DEV_TX_OFFLOAD_TCP_TSO |	\
	 DEV_TX_OFFLOAD_MULTI_SEGS)

#define VMXNET3_RX_OFFLOAD_CAP		\
	(DEV_RX_OFFLOAD_VLAN_STRIP |	\
	 DEV_RX_OFFLOAD_VLAN_FILTER |   \
	 DEV_RX_OFFLOAD_SCATTER |	\
	 DEV_RX_OFFLOAD_UDP_CKSUM |	\
	 DEV_RX_OFFLOAD_TCP_CKSUM |	\
	 DEV_RX_OFFLOAD_TCP_LRO |	\
	 DEV_RX_OFFLOAD_JUMBO_FRAME |   \
	 DEV_RX_OFFLOAD_RSS_HASH)

int vmxnet3_segs_dynfield_offset = -1;

static int eth_vmxnet3_dev_init(struct rte_eth_dev *eth_dev);
static int eth_vmxnet3_dev_uninit(struct rte_eth_dev *eth_dev);
static int vmxnet3_dev_configure(struct rte_eth_dev *dev);
static int vmxnet3_dev_start(struct rte_eth_dev *dev);
static int vmxnet3_dev_stop(struct rte_eth_dev *dev);
static int vmxnet3_dev_close(struct rte_eth_dev *dev);
static void vmxnet3_dev_set_rxmode(struct vmxnet3_hw *hw, uint32_t feature, int set);
static int vmxnet3_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int vmxnet3_dev_promiscuous_disable(struct rte_eth_dev *dev);
static int vmxnet3_dev_allmulticast_enable(struct rte_eth_dev *dev);
static int vmxnet3_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int __vmxnet3_dev_link_update(struct rte_eth_dev *dev,
				     int wait_to_complete);
static int vmxnet3_dev_link_update(struct rte_eth_dev *dev,
				   int wait_to_complete);
static void vmxnet3_hw_stats_save(struct vmxnet3_hw *hw);
static int vmxnet3_dev_stats_get(struct rte_eth_dev *dev,
				  struct rte_eth_stats *stats);
static int vmxnet3_dev_stats_reset(struct rte_eth_dev *dev);
static int vmxnet3_dev_xstats_get_names(struct rte_eth_dev *dev,
					struct rte_eth_xstat_name *xstats,
					unsigned int n);
static int vmxnet3_dev_xstats_get(struct rte_eth_dev *dev,
				  struct rte_eth_xstat *xstats, unsigned int n);
static int vmxnet3_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static const uint32_t *
vmxnet3_dev_supported_ptypes_get(struct rte_eth_dev *dev);
static int vmxnet3_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int vmxnet3_dev_vlan_filter_set(struct rte_eth_dev *dev,
				       uint16_t vid, int on);
static int vmxnet3_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int vmxnet3_mac_addr_set(struct rte_eth_dev *dev,
				 struct rte_ether_addr *mac_addr);
static void vmxnet3_interrupt_handler(void *param);

/*
 * The set of PCI devices this driver supports
 */
#define VMWARE_PCI_VENDOR_ID 0x15AD
#define VMWARE_DEV_ID_VMXNET3 0x07B0
static const struct rte_pci_id pci_id_vmxnet3_map[] = {
	{ RTE_PCI_DEVICE(VMWARE_PCI_VENDOR_ID, VMWARE_DEV_ID_VMXNET3) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops vmxnet3_eth_dev_ops = {
	.dev_configure        = vmxnet3_dev_configure,
	.dev_start            = vmxnet3_dev_start,
	.dev_stop             = vmxnet3_dev_stop,
	.dev_close            = vmxnet3_dev_close,
	.promiscuous_enable   = vmxnet3_dev_promiscuous_enable,
	.promiscuous_disable  = vmxnet3_dev_promiscuous_disable,
	.allmulticast_enable  = vmxnet3_dev_allmulticast_enable,
	.allmulticast_disable = vmxnet3_dev_allmulticast_disable,
	.link_update          = vmxnet3_dev_link_update,
	.stats_get            = vmxnet3_dev_stats_get,
	.xstats_get_names     = vmxnet3_dev_xstats_get_names,
	.xstats_get           = vmxnet3_dev_xstats_get,
	.stats_reset          = vmxnet3_dev_stats_reset,
	.mac_addr_set         = vmxnet3_mac_addr_set,
	.dev_infos_get        = vmxnet3_dev_info_get,
	.dev_supported_ptypes_get = vmxnet3_dev_supported_ptypes_get,
	.mtu_set              = vmxnet3_dev_mtu_set,
	.vlan_filter_set      = vmxnet3_dev_vlan_filter_set,
	.vlan_offload_set     = vmxnet3_dev_vlan_offload_set,
	.rx_queue_setup       = vmxnet3_dev_rx_queue_setup,
	.rx_queue_release     = vmxnet3_dev_rx_queue_release,
	.tx_queue_setup       = vmxnet3_dev_tx_queue_setup,
	.tx_queue_release     = vmxnet3_dev_tx_queue_release,
};

struct vmxnet3_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

/* tx_qX_ is prepended to the name string here */
static const struct vmxnet3_xstats_name_off vmxnet3_txq_stat_strings[] = {
	{"drop_total",         offsetof(struct vmxnet3_txq_stats, drop_total)},
	{"drop_too_many_segs", offsetof(struct vmxnet3_txq_stats, drop_too_many_segs)},
	{"drop_tso",           offsetof(struct vmxnet3_txq_stats, drop_tso)},
	{"tx_ring_full",       offsetof(struct vmxnet3_txq_stats, tx_ring_full)},
};

/* rx_qX_ is prepended to the name string here */
static const struct vmxnet3_xstats_name_off vmxnet3_rxq_stat_strings[] = {
	{"drop_total",           offsetof(struct vmxnet3_rxq_stats, drop_total)},
	{"drop_err",             offsetof(struct vmxnet3_rxq_stats, drop_err)},
	{"drop_fcs",             offsetof(struct vmxnet3_rxq_stats, drop_fcs)},
	{"rx_buf_alloc_failure", offsetof(struct vmxnet3_rxq_stats, rx_buf_alloc_failure)},
};

static const struct rte_memzone *
gpa_zone_reserve(struct rte_eth_dev *dev, uint32_t size,
		 const char *post_string, int socket_id,
		 uint16_t align, bool reuse)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "eth_p%d_%s",
			dev->data->port_id, post_string);

	mz = rte_memzone_lookup(z_name);
	if (!reuse) {
		if (mz)
			rte_memzone_free(mz);
		return rte_memzone_reserve_aligned(z_name, size, socket_id,
				RTE_MEMZONE_IOVA_CONTIG, align);
	}

	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(z_name, size, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, align);
}

/*
 * This function is based on vmxnet3_disable_intr()
 */
static void
vmxnet3_disable_intr(struct vmxnet3_hw *hw)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	hw->shared->devRead.intrConf.intrCtrl |= VMXNET3_IC_DISABLE_ALL;
	for (i = 0; i < hw->num_intrs; i++)
		VMXNET3_WRITE_BAR0_REG(hw, VMXNET3_REG_IMR + i * 8, 1);
}

static void
vmxnet3_enable_intr(struct vmxnet3_hw *hw)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	hw->shared->devRead.intrConf.intrCtrl &= ~VMXNET3_IC_DISABLE_ALL;
	for (i = 0; i < hw->num_intrs; i++)
		VMXNET3_WRITE_BAR0_REG(hw, VMXNET3_REG_IMR + i * 8, 0);
}

/*
 * Gets tx data ring descriptor size.
 */
static uint16_t
eth_vmxnet3_txdata_get(struct vmxnet3_hw *hw)
{
	uint16 txdata_desc_size;

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
			       VMXNET3_CMD_GET_TXDATA_DESC_SIZE);
	txdata_desc_size = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);

	return (txdata_desc_size < VMXNET3_TXDATA_DESC_MIN_SIZE ||
		txdata_desc_size > VMXNET3_TXDATA_DESC_MAX_SIZE ||
		txdata_desc_size & VMXNET3_TXDATA_DESC_SIZE_MASK) ?
		sizeof(struct Vmxnet3_TxDataDesc) : txdata_desc_size;
}

/*
 * It returns 0 on success.
 */
static int
eth_vmxnet3_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct vmxnet3_hw *hw = eth_dev->data->dev_private;
	uint32_t mac_hi, mac_lo, ver;
	struct rte_eth_link link;
	static const struct rte_mbuf_dynfield vmxnet3_segs_dynfield_desc = {
		.name = VMXNET3_SEGS_DYNFIELD_NAME,
		.size = sizeof(vmxnet3_segs_dynfield_t),
		.align = __alignof__(vmxnet3_segs_dynfield_t),
	};

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &vmxnet3_eth_dev_ops;
	eth_dev->rx_pkt_burst = &vmxnet3_recv_pkts;
	eth_dev->tx_pkt_burst = &vmxnet3_xmit_pkts;
	eth_dev->tx_pkt_prepare = vmxnet3_prep_pkts;
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* extra mbuf field is required to guess MSS */
	vmxnet3_segs_dynfield_offset =
		rte_mbuf_dynfield_register(&vmxnet3_segs_dynfield_desc);
	if (vmxnet3_segs_dynfield_offset < 0) {
		PMD_INIT_LOG(ERR, "Cannot register mbuf field.");
		return -rte_errno;
	}

	/*
	 * for secondary processes, we don't initialize any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr0 = (void *)pci_dev->mem_resource[0].addr;
	hw->hw_addr1 = (void *)pci_dev->mem_resource[1].addr;

	hw->num_rx_queues = 1;
	hw->num_tx_queues = 1;
	hw->bufs_per_pkt = 1;

	/* Check h/w version compatibility with driver. */
	ver = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_VRRS);
	PMD_INIT_LOG(DEBUG, "Hardware version : %d", ver);

	if (ver & (1 << VMXNET3_REV_4)) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_VRRS,
				       1 << VMXNET3_REV_4);
		hw->version = VMXNET3_REV_4 + 1;
	} else if (ver & (1 << VMXNET3_REV_3)) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_VRRS,
				       1 << VMXNET3_REV_3);
		hw->version = VMXNET3_REV_3 + 1;
	} else if (ver & (1 << VMXNET3_REV_2)) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_VRRS,
				       1 << VMXNET3_REV_2);
		hw->version = VMXNET3_REV_2 + 1;
	} else if (ver & (1 << VMXNET3_REV_1)) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_VRRS,
				       1 << VMXNET3_REV_1);
		hw->version = VMXNET3_REV_1 + 1;
	} else {
		PMD_INIT_LOG(ERR, "Incompatible hardware version: %d", ver);
		return -EIO;
	}

	PMD_INIT_LOG(DEBUG, "Using device version %d\n", hw->version);

	/* Check UPT version compatibility with driver. */
	ver = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_UVRS);
	PMD_INIT_LOG(DEBUG, "UPT hardware version : %d", ver);
	if (ver & 0x1)
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_UVRS, 1);
	else {
		PMD_INIT_LOG(ERR, "Incompatible UPT version.");
		return -EIO;
	}

	/* Getting MAC Address */
	mac_lo = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_MACL);
	mac_hi = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_MACH);
	memcpy(hw->perm_addr, &mac_lo, 4);
	memcpy(hw->perm_addr + 4, &mac_hi, 2);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("vmxnet3", RTE_ETHER_ADDR_LEN *
					       VMXNET3_MAX_MAC_ADDRS, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %d bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * VMXNET3_MAX_MAC_ADDRS);
		return -ENOMEM;
	}
	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->perm_addr,
			&eth_dev->data->mac_addrs[0]);

	PMD_INIT_LOG(DEBUG, "MAC Address : %02x:%02x:%02x:%02x:%02x:%02x",
		     hw->perm_addr[0], hw->perm_addr[1], hw->perm_addr[2],
		     hw->perm_addr[3], hw->perm_addr[4], hw->perm_addr[5]);

	/* Put device in Quiesce Mode */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_QUIESCE_DEV);

	/* allow untagged pkts */
	VMXNET3_SET_VFTABLE_ENTRY(hw->shadow_vfta, 0);

	hw->txdata_desc_size = VMXNET3_VERSION_GE_3(hw) ?
		eth_vmxnet3_txdata_get(hw) : sizeof(struct Vmxnet3_TxDataDesc);

	hw->rxdata_desc_size = VMXNET3_VERSION_GE_3(hw) ?
		VMXNET3_DEF_RXDATA_DESC_SIZE : 0;
	RTE_ASSERT((hw->rxdata_desc_size & ~VMXNET3_RXDATA_DESC_SIZE_MASK) ==
		   hw->rxdata_desc_size);

	/* clear shadow stats */
	memset(hw->saved_tx_stats, 0, sizeof(hw->saved_tx_stats));
	memset(hw->saved_rx_stats, 0, sizeof(hw->saved_rx_stats));

	/* clear snapshot stats */
	memset(hw->snapshot_tx_stats, 0, sizeof(hw->snapshot_tx_stats));
	memset(hw->snapshot_rx_stats, 0, sizeof(hw->snapshot_rx_stats));

	/* set the initial link status */
	memset(&link, 0, sizeof(link));
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_speed = ETH_SPEED_NUM_10G;
	link.link_autoneg = ETH_LINK_FIXED;
	rte_eth_linkstatus_set(eth_dev, &link);

	return 0;
}

static int
eth_vmxnet3_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct vmxnet3_hw *hw = eth_dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (hw->adapter_stopped == 0) {
		PMD_INIT_LOG(DEBUG, "Device has not been closed.");
		return -EBUSY;
	}

	return 0;
}

static int eth_vmxnet3_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct vmxnet3_hw), eth_vmxnet3_dev_init);
}

static int eth_vmxnet3_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_vmxnet3_dev_uninit);
}

static struct rte_pci_driver rte_vmxnet3_pmd = {
	.id_table = pci_id_vmxnet3_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_vmxnet3_pci_probe,
	.remove = eth_vmxnet3_pci_remove,
};

static int
vmxnet3_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_memzone *mz;
	struct vmxnet3_hw *hw = dev->data->dev_private;
	size_t size;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	if (dev->data->nb_tx_queues > VMXNET3_MAX_TX_QUEUES ||
	    dev->data->nb_rx_queues > VMXNET3_MAX_RX_QUEUES) {
		PMD_INIT_LOG(ERR, "ERROR: Number of queues not supported");
		return -EINVAL;
	}

	if (!rte_is_power_of_2(dev->data->nb_rx_queues)) {
		PMD_INIT_LOG(ERR, "ERROR: Number of rx queues not power of 2");
		return -EINVAL;
	}

	size = dev->data->nb_rx_queues * sizeof(struct Vmxnet3_TxQueueDesc) +
		dev->data->nb_tx_queues * sizeof(struct Vmxnet3_RxQueueDesc);

	if (size > UINT16_MAX)
		return -EINVAL;

	hw->num_rx_queues = (uint8_t)dev->data->nb_rx_queues;
	hw->num_tx_queues = (uint8_t)dev->data->nb_tx_queues;

	/*
	 * Allocate a memzone for Vmxnet3_DriverShared - Vmxnet3_DSDevRead
	 * on current socket
	 */
	mz = gpa_zone_reserve(dev, sizeof(struct Vmxnet3_DriverShared),
			      "shared", rte_socket_id(), 8, 1);

	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating shared zone");
		return -ENOMEM;
	}
	memset(mz->addr, 0, mz->len);

	hw->shared = mz->addr;
	hw->sharedPA = mz->iova;

	/*
	 * Allocate a memzone for Vmxnet3_RxQueueDesc - Vmxnet3_TxQueueDesc
	 * on current socket.
	 *
	 * We cannot reuse this memzone from previous allocation as its size
	 * depends on the number of tx and rx queues, which could be different
	 * from one config to another.
	 */
	mz = gpa_zone_reserve(dev, size, "queuedesc", rte_socket_id(),
			      VMXNET3_QUEUE_DESC_ALIGN, 0);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating queue descriptors zone");
		return -ENOMEM;
	}
	memset(mz->addr, 0, mz->len);

	hw->tqd_start = (Vmxnet3_TxQueueDesc *)mz->addr;
	hw->rqd_start = (Vmxnet3_RxQueueDesc *)(hw->tqd_start + hw->num_tx_queues);

	hw->queueDescPA = mz->iova;
	hw->queue_desc_len = (uint16_t)size;

	if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
		/* Allocate memory structure for UPT1_RSSConf and configure */
		mz = gpa_zone_reserve(dev, sizeof(struct VMXNET3_RSSConf),
				      "rss_conf", rte_socket_id(),
				      RTE_CACHE_LINE_SIZE, 1);
		if (mz == NULL) {
			PMD_INIT_LOG(ERR,
				     "ERROR: Creating rss_conf structure zone");
			return -ENOMEM;
		}
		memset(mz->addr, 0, mz->len);

		hw->rss_conf = mz->addr;
		hw->rss_confPA = mz->iova;
	}

	return 0;
}

static void
vmxnet3_write_mac(struct vmxnet3_hw *hw, const uint8_t *addr)
{
	uint32_t val;

	PMD_INIT_LOG(DEBUG,
		     "Writing MAC Address : %02x:%02x:%02x:%02x:%02x:%02x",
		     addr[0], addr[1], addr[2],
		     addr[3], addr[4], addr[5]);

	memcpy(&val, addr, 4);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_MACL, val);

	memcpy(&val, addr + 4, 2);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_MACH, val);
}

static int
vmxnet3_dev_setup_memreg(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	Vmxnet3_DriverShared *shared = hw->shared;
	Vmxnet3_CmdInfo *cmdInfo;
	struct rte_mempool *mp[VMXNET3_MAX_RX_QUEUES];
	uint8_t index[VMXNET3_MAX_RX_QUEUES + VMXNET3_MAX_TX_QUEUES];
	uint32_t num, i, j, size;

	if (hw->memRegsPA == 0) {
		const struct rte_memzone *mz;

		size = sizeof(Vmxnet3_MemRegs) +
			(VMXNET3_MAX_RX_QUEUES + VMXNET3_MAX_TX_QUEUES) *
			sizeof(Vmxnet3_MemoryRegion);

		mz = gpa_zone_reserve(dev, size, "memRegs", rte_socket_id(), 8,
				      1);
		if (mz == NULL) {
			PMD_INIT_LOG(ERR, "ERROR: Creating memRegs zone");
			return -ENOMEM;
		}
		memset(mz->addr, 0, mz->len);
		hw->memRegs = mz->addr;
		hw->memRegsPA = mz->iova;
	}

	num = hw->num_rx_queues;

	for (i = 0; i < num; i++) {
		vmxnet3_rx_queue_t *rxq = dev->data->rx_queues[i];

		mp[i] = rxq->mp;
		index[i] = 1 << i;
	}

	/*
	 * The same mempool could be used by multiple queues. In such a case,
	 * remove duplicate mempool entries. Only one entry is kept with
	 * bitmask indicating queues that are using this mempool.
	 */
	for (i = 1; i < num; i++) {
		for (j = 0; j < i; j++) {
			if (mp[i] == mp[j]) {
				mp[i] = NULL;
				index[j] |= 1 << i;
				break;
			}
		}
	}

	j = 0;
	for (i = 0; i < num; i++) {
		if (mp[i] == NULL)
			continue;

		Vmxnet3_MemoryRegion *mr = &hw->memRegs->memRegs[j];

		mr->startPA =
			(uintptr_t)STAILQ_FIRST(&mp[i]->mem_list)->iova;
		mr->length = STAILQ_FIRST(&mp[i]->mem_list)->len <= INT32_MAX ?
			STAILQ_FIRST(&mp[i]->mem_list)->len : INT32_MAX;
		mr->txQueueBits = index[i];
		mr->rxQueueBits = index[i];

		PMD_INIT_LOG(INFO,
			     "index: %u startPA: %" PRIu64 " length: %u, "
			     "rxBits: %x",
			     j, mr->startPA, mr->length, mr->rxQueueBits);
		j++;
	}
	hw->memRegs->numRegs = j;
	PMD_INIT_LOG(INFO, "numRegs: %u", j);

	size = sizeof(Vmxnet3_MemRegs) +
		(j - 1) * sizeof(Vmxnet3_MemoryRegion);

	cmdInfo = &shared->cu.cmdInfo;
	cmdInfo->varConf.confVer = 1;
	cmdInfo->varConf.confLen = size;
	cmdInfo->varConf.confPA = hw->memRegsPA;

	return 0;
}

static int
vmxnet3_setup_driver_shared(struct rte_eth_dev *dev)
{
	struct rte_eth_conf port_conf = dev->data->dev_conf;
	struct vmxnet3_hw *hw = dev->data->dev_private;
	uint32_t mtu = dev->data->mtu;
	Vmxnet3_DriverShared *shared = hw->shared;
	Vmxnet3_DSDevRead *devRead = &shared->devRead;
	uint64_t rx_offloads = dev->data->dev_conf.rxmode.offloads;
	uint32_t i;
	int ret;

	hw->mtu = mtu;

	shared->magic = VMXNET3_REV1_MAGIC;
	devRead->misc.driverInfo.version = VMXNET3_DRIVER_VERSION_NUM;

	/* Setting up Guest OS information */
	devRead->misc.driverInfo.gos.gosBits   = sizeof(void *) == 4 ?
		VMXNET3_GOS_BITS_32 : VMXNET3_GOS_BITS_64;
	devRead->misc.driverInfo.gos.gosType   = VMXNET3_GOS_TYPE_LINUX;
	devRead->misc.driverInfo.vmxnet3RevSpt = 1;
	devRead->misc.driverInfo.uptVerSpt     = 1;

	devRead->misc.mtu = rte_le_to_cpu_32(mtu);
	devRead->misc.queueDescPA  = hw->queueDescPA;
	devRead->misc.queueDescLen = hw->queue_desc_len;
	devRead->misc.numTxQueues  = hw->num_tx_queues;
	devRead->misc.numRxQueues  = hw->num_rx_queues;

	/*
	 * Set number of interrupts to 1
	 * PMD by default disables all the interrupts but this is MUST
	 * to activate device. It needs at least one interrupt for
	 * link events to handle
	 */
	hw->num_intrs = devRead->intrConf.numIntrs = 1;
	devRead->intrConf.intrCtrl |= VMXNET3_IC_DISABLE_ALL;

	for (i = 0; i < hw->num_tx_queues; i++) {
		Vmxnet3_TxQueueDesc *tqd = &hw->tqd_start[i];
		vmxnet3_tx_queue_t *txq  = dev->data->tx_queues[i];

		txq->shared = &hw->tqd_start[i];

		tqd->ctrl.txNumDeferred  = 0;
		tqd->ctrl.txThreshold    = 1;
		tqd->conf.txRingBasePA   = txq->cmd_ring.basePA;
		tqd->conf.compRingBasePA = txq->comp_ring.basePA;
		tqd->conf.dataRingBasePA = txq->data_ring.basePA;

		tqd->conf.txRingSize   = txq->cmd_ring.size;
		tqd->conf.compRingSize = txq->comp_ring.size;
		tqd->conf.dataRingSize = txq->data_ring.size;
		tqd->conf.txDataRingDescSize = txq->txdata_desc_size;
		tqd->conf.intrIdx      = txq->comp_ring.intr_idx;
		tqd->status.stopped    = TRUE;
		tqd->status.error      = 0;
		memset(&tqd->stats, 0, sizeof(tqd->stats));
	}

	for (i = 0; i < hw->num_rx_queues; i++) {
		Vmxnet3_RxQueueDesc *rqd  = &hw->rqd_start[i];
		vmxnet3_rx_queue_t *rxq   = dev->data->rx_queues[i];

		rxq->shared = &hw->rqd_start[i];

		rqd->conf.rxRingBasePA[0] = rxq->cmd_ring[0].basePA;
		rqd->conf.rxRingBasePA[1] = rxq->cmd_ring[1].basePA;
		rqd->conf.compRingBasePA  = rxq->comp_ring.basePA;

		rqd->conf.rxRingSize[0]   = rxq->cmd_ring[0].size;
		rqd->conf.rxRingSize[1]   = rxq->cmd_ring[1].size;
		rqd->conf.compRingSize    = rxq->comp_ring.size;
		rqd->conf.intrIdx         = rxq->comp_ring.intr_idx;
		if (VMXNET3_VERSION_GE_3(hw)) {
			rqd->conf.rxDataRingBasePA = rxq->data_ring.basePA;
			rqd->conf.rxDataRingDescSize = rxq->data_desc_size;
		}
		rqd->status.stopped       = TRUE;
		rqd->status.error         = 0;
		memset(&rqd->stats, 0, sizeof(rqd->stats));
	}

	/* RxMode set to 0 of VMXNET3_RXM_xxx */
	devRead->rxFilterConf.rxMode = 0;

	/* Setting up feature flags */
	if (rx_offloads & DEV_RX_OFFLOAD_CHECKSUM)
		devRead->misc.uptFeatures |= VMXNET3_F_RXCSUM;

	if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO) {
		devRead->misc.uptFeatures |= VMXNET3_F_LRO;
		devRead->misc.maxNumRxSG = 0;
	}

	if (port_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
		ret = vmxnet3_rss_configure(dev);
		if (ret != VMXNET3_SUCCESS)
			return ret;

		devRead->misc.uptFeatures |= VMXNET3_F_RSS;
		devRead->rssConfDesc.confVer = 1;
		devRead->rssConfDesc.confLen = sizeof(struct VMXNET3_RSSConf);
		devRead->rssConfDesc.confPA  = hw->rss_confPA;
	}

	ret = vmxnet3_dev_vlan_offload_set(dev,
			ETH_VLAN_STRIP_MASK | ETH_VLAN_FILTER_MASK);
	if (ret)
		return ret;

	vmxnet3_write_mac(hw, dev->data->mac_addrs->addr_bytes);

	return VMXNET3_SUCCESS;
}

/*
 * Configure device link speed and setup link.
 * Must be called after eth_vmxnet3_dev_init. Other wise it might fail
 * It returns 0 on success.
 */
static int
vmxnet3_dev_start(struct rte_eth_dev *dev)
{
	int ret;
	struct vmxnet3_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	/* Save stats before it is reset by CMD_ACTIVATE */
	vmxnet3_hw_stats_save(hw);

	ret = vmxnet3_setup_driver_shared(dev);
	if (ret != VMXNET3_SUCCESS)
		return ret;

	/* check if lsc interrupt feature is enabled */
	if (dev->data->dev_conf.intr_conf.lsc) {
		struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

		/* Setup interrupt callback  */
		rte_intr_callback_register(&pci_dev->intr_handle,
					   vmxnet3_interrupt_handler, dev);

		if (rte_intr_enable(&pci_dev->intr_handle) < 0) {
			PMD_INIT_LOG(ERR, "interrupt enable failed");
			return -EIO;
		}
	}

	/* Exchange shared data with device */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAL,
			       VMXNET3_GET_ADDR_LO(hw->sharedPA));
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAH,
			       VMXNET3_GET_ADDR_HI(hw->sharedPA));

	/* Activate device by register write */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_ACTIVATE_DEV);
	ret = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);

	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Device activation: UNSUCCESSFUL");
		return -EINVAL;
	}

	/* Setup memory region for rx buffers */
	ret = vmxnet3_dev_setup_memreg(dev);
	if (ret == 0) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
				       VMXNET3_CMD_REGISTER_MEMREGS);
		ret = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);
		if (ret != 0)
			PMD_INIT_LOG(DEBUG,
				     "Failed in setup memory region cmd\n");
		ret = 0;
	} else {
		PMD_INIT_LOG(DEBUG, "Failed to setup memory region\n");
	}

	if (VMXNET3_VERSION_GE_4(hw) &&
	    dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
		/* Check for additional RSS  */
		ret = vmxnet3_v4_rss_configure(dev);
		if (ret != VMXNET3_SUCCESS) {
			PMD_INIT_LOG(ERR, "Failed to configure v4 RSS");
			return ret;
		}
	}

	/* Disable interrupts */
	vmxnet3_disable_intr(hw);

	/*
	 * Load RX queues with blank mbufs and update next2fill index for device
	 * Update RxMode of the device
	 */
	ret = vmxnet3_dev_rxtx_init(dev);
	if (ret != VMXNET3_SUCCESS) {
		PMD_INIT_LOG(ERR, "Device queue init: UNSUCCESSFUL");
		return ret;
	}

	hw->adapter_stopped = FALSE;

	/* Setting proper Rx Mode and issue Rx Mode Update command */
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_UCAST | VMXNET3_RXM_BCAST, 1);

	if (dev->data->dev_conf.intr_conf.lsc) {
		vmxnet3_enable_intr(hw);

		/*
		 * Update link state from device since this won't be
		 * done upon starting with lsc in use. This is done
		 * only after enabling interrupts to avoid any race
		 * where the link state could change without an
		 * interrupt being fired.
		 */
		__vmxnet3_dev_link_update(dev, 0);
	}

	return VMXNET3_SUCCESS;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static int
vmxnet3_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct vmxnet3_hw *hw = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	if (hw->adapter_stopped == 1) {
		PMD_INIT_LOG(DEBUG, "Device already stopped.");
		return 0;
	}

	/* disable interrupts */
	vmxnet3_disable_intr(hw);

	if (dev->data->dev_conf.intr_conf.lsc) {
		struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

		rte_intr_disable(&pci_dev->intr_handle);

		rte_intr_callback_unregister(&pci_dev->intr_handle,
					     vmxnet3_interrupt_handler, dev);
	}

	/* quiesce the device first */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_QUIESCE_DEV);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAL, 0);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAH, 0);

	/* reset the device */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_RESET_DEV);
	PMD_INIT_LOG(DEBUG, "Device reset.");

	vmxnet3_dev_clear_queues(dev);

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_speed = ETH_SPEED_NUM_10G;
	link.link_autoneg = ETH_LINK_FIXED;
	rte_eth_linkstatus_set(dev, &link);

	hw->adapter_stopped = 1;
	dev->data->dev_started = 0;

	return 0;
}

static void
vmxnet3_free_queues(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		void *rxq = dev->data->rx_queues[i];

		vmxnet3_dev_rx_queue_release(rxq);
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		void *txq = dev->data->tx_queues[i];

		vmxnet3_dev_tx_queue_release(txq);
	}
	dev->data->nb_tx_queues = 0;
}

/*
 * Reset and stop device.
 */
static int
vmxnet3_dev_close(struct rte_eth_dev *dev)
{
	int ret;
	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = vmxnet3_dev_stop(dev);
	vmxnet3_free_queues(dev);

	return ret;
}

static void
vmxnet3_hw_tx_stats_get(struct vmxnet3_hw *hw, unsigned int q,
			struct UPT1_TxStats *res)
{
#define VMXNET3_UPDATE_TX_STAT(h, i, f, r)		\
		((r)->f = (h)->tqd_start[(i)].stats.f +	\
			(h)->saved_tx_stats[(i)].f)

	VMXNET3_UPDATE_TX_STAT(hw, q, ucastPktsTxOK, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, mcastPktsTxOK, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, bcastPktsTxOK, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, ucastBytesTxOK, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, mcastBytesTxOK, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, bcastBytesTxOK, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, pktsTxError, res);
	VMXNET3_UPDATE_TX_STAT(hw, q, pktsTxDiscard, res);

#undef VMXNET3_UPDATE_TX_STAT
}

static void
vmxnet3_hw_rx_stats_get(struct vmxnet3_hw *hw, unsigned int q,
			struct UPT1_RxStats *res)
{
#define VMXNET3_UPDATE_RX_STAT(h, i, f, r)		\
		((r)->f = (h)->rqd_start[(i)].stats.f +	\
			(h)->saved_rx_stats[(i)].f)

	VMXNET3_UPDATE_RX_STAT(hw, q, ucastPktsRxOK, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, mcastPktsRxOK, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, bcastPktsRxOK, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, ucastBytesRxOK, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, mcastBytesRxOK, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, bcastBytesRxOK, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, pktsRxError, res);
	VMXNET3_UPDATE_RX_STAT(hw, q, pktsRxOutOfBuf, res);

#undef VMXNET3_UPDATE_RX_STAT
}

static void
vmxnet3_tx_stats_get(struct vmxnet3_hw *hw, unsigned int q,
					struct UPT1_TxStats *res)
{
		vmxnet3_hw_tx_stats_get(hw, q, res);

#define VMXNET3_REDUCE_SNAPSHOT_TX_STAT(h, i, f, r)	\
		((r)->f -= (h)->snapshot_tx_stats[(i)].f)

	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, ucastPktsTxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, mcastPktsTxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, bcastPktsTxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, ucastBytesTxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, mcastBytesTxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, bcastBytesTxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, pktsTxError, res);
	VMXNET3_REDUCE_SNAPSHOT_TX_STAT(hw, q, pktsTxDiscard, res);

#undef VMXNET3_REDUCE_SNAPSHOT_TX_STAT
}

static void
vmxnet3_rx_stats_get(struct vmxnet3_hw *hw, unsigned int q,
					struct UPT1_RxStats *res)
{
		vmxnet3_hw_rx_stats_get(hw, q, res);

#define VMXNET3_REDUCE_SNAPSHOT_RX_STAT(h, i, f, r)	\
		((r)->f -= (h)->snapshot_rx_stats[(i)].f)

	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, ucastPktsRxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, mcastPktsRxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, bcastPktsRxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, ucastBytesRxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, mcastBytesRxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, bcastBytesRxOK, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, pktsRxError, res);
	VMXNET3_REDUCE_SNAPSHOT_RX_STAT(hw, q, pktsRxOutOfBuf, res);

#undef VMXNET3_REDUCE_SNAPSHOT_RX_STAT
}

static void
vmxnet3_hw_stats_save(struct vmxnet3_hw *hw)
{
	unsigned int i;

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_STATS);

	RTE_BUILD_BUG_ON(RTE_ETHDEV_QUEUE_STAT_CNTRS < VMXNET3_MAX_TX_QUEUES);

	for (i = 0; i < hw->num_tx_queues; i++)
		vmxnet3_hw_tx_stats_get(hw, i, &hw->saved_tx_stats[i]);
	for (i = 0; i < hw->num_rx_queues; i++)
		vmxnet3_hw_rx_stats_get(hw, i, &hw->saved_rx_stats[i]);
}

static int
vmxnet3_dev_xstats_get_names(struct rte_eth_dev *dev,
			     struct rte_eth_xstat_name *xstats_names,
			     unsigned int n)
{
	unsigned int i, t, count = 0;
	unsigned int nstats =
		dev->data->nb_tx_queues * RTE_DIM(vmxnet3_txq_stat_strings) +
		dev->data->nb_rx_queues * RTE_DIM(vmxnet3_rxq_stat_strings);

	if (!xstats_names || n < nstats)
		return nstats;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!dev->data->rx_queues[i])
			continue;

		for (t = 0; t < RTE_DIM(vmxnet3_rxq_stat_strings); t++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rx_q%u_%s", i,
				 vmxnet3_rxq_stat_strings[t].name);
			count++;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (!dev->data->tx_queues[i])
			continue;

		for (t = 0; t < RTE_DIM(vmxnet3_txq_stat_strings); t++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "tx_q%u_%s", i,
				 vmxnet3_txq_stat_strings[t].name);
			count++;
		}
	}

	return count;
}

static int
vmxnet3_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		       unsigned int n)
{
	unsigned int i, t, count = 0;
	unsigned int nstats =
		dev->data->nb_tx_queues * RTE_DIM(vmxnet3_txq_stat_strings) +
		dev->data->nb_rx_queues * RTE_DIM(vmxnet3_rxq_stat_strings);

	if (n < nstats)
		return nstats;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct vmxnet3_rx_queue *rxq = dev->data->rx_queues[i];

		if (rxq == NULL)
			continue;

		for (t = 0; t < RTE_DIM(vmxnet3_rxq_stat_strings); t++) {
			xstats[count].value = *(uint64_t *)(((char *)&rxq->stats) +
				vmxnet3_rxq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct vmxnet3_tx_queue *txq = dev->data->tx_queues[i];

		if (txq == NULL)
			continue;

		for (t = 0; t < RTE_DIM(vmxnet3_txq_stat_strings); t++) {
			xstats[count].value = *(uint64_t *)(((char *)&txq->stats) +
				vmxnet3_txq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}

	return count;
}

static int
vmxnet3_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned int i;
	struct vmxnet3_hw *hw = dev->data->dev_private;
	struct UPT1_TxStats txStats;
	struct UPT1_RxStats rxStats;

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_STATS);

	RTE_BUILD_BUG_ON(RTE_ETHDEV_QUEUE_STAT_CNTRS < VMXNET3_MAX_TX_QUEUES);
	for (i = 0; i < hw->num_tx_queues; i++) {
		vmxnet3_tx_stats_get(hw, i, &txStats);

		stats->q_opackets[i] = txStats.ucastPktsTxOK +
			txStats.mcastPktsTxOK +
			txStats.bcastPktsTxOK;

		stats->q_obytes[i] = txStats.ucastBytesTxOK +
			txStats.mcastBytesTxOK +
			txStats.bcastBytesTxOK;

		stats->opackets += stats->q_opackets[i];
		stats->obytes += stats->q_obytes[i];
		stats->oerrors += txStats.pktsTxError + txStats.pktsTxDiscard;
	}

	RTE_BUILD_BUG_ON(RTE_ETHDEV_QUEUE_STAT_CNTRS < VMXNET3_MAX_RX_QUEUES);
	for (i = 0; i < hw->num_rx_queues; i++) {
		vmxnet3_rx_stats_get(hw, i, &rxStats);

		stats->q_ipackets[i] = rxStats.ucastPktsRxOK +
			rxStats.mcastPktsRxOK +
			rxStats.bcastPktsRxOK;

		stats->q_ibytes[i] = rxStats.ucastBytesRxOK +
			rxStats.mcastBytesRxOK +
			rxStats.bcastBytesRxOK;

		stats->ipackets += stats->q_ipackets[i];
		stats->ibytes += stats->q_ibytes[i];

		stats->q_errors[i] = rxStats.pktsRxError;
		stats->ierrors += rxStats.pktsRxError;
		stats->imissed += rxStats.pktsRxOutOfBuf;
	}

	return 0;
}

static int
vmxnet3_dev_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct vmxnet3_hw *hw = dev->data->dev_private;
	struct UPT1_TxStats txStats = {0};
	struct UPT1_RxStats rxStats = {0};

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_STATS);

	RTE_BUILD_BUG_ON(RTE_ETHDEV_QUEUE_STAT_CNTRS < VMXNET3_MAX_TX_QUEUES);

	for (i = 0; i < hw->num_tx_queues; i++) {
		vmxnet3_hw_tx_stats_get(hw, i, &txStats);
		memcpy(&hw->snapshot_tx_stats[i], &txStats,
			sizeof(hw->snapshot_tx_stats[0]));
	}
	for (i = 0; i < hw->num_rx_queues; i++) {
		vmxnet3_hw_rx_stats_get(hw, i, &rxStats);
		memcpy(&hw->snapshot_rx_stats[i], &rxStats,
			sizeof(hw->snapshot_rx_stats[0]));
	}

	return 0;
}

static int
vmxnet3_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;

	dev_info->max_rx_queues = VMXNET3_MAX_RX_QUEUES;
	dev_info->max_tx_queues = VMXNET3_MAX_TX_QUEUES;
	dev_info->min_rx_bufsize = 1518 + RTE_PKTMBUF_HEADROOM;
	dev_info->max_rx_pktlen = 16384; /* includes CRC, cf MAXFRS register */
	dev_info->min_mtu = VMXNET3_MIN_MTU;
	dev_info->max_mtu = VMXNET3_MAX_MTU;
	dev_info->speed_capa = ETH_LINK_SPEED_10G;
	dev_info->max_mac_addrs = VMXNET3_MAX_MAC_ADDRS;

	dev_info->flow_type_rss_offloads = VMXNET3_RSS_OFFLOAD_ALL;

	if (VMXNET3_VERSION_GE_4(hw)) {
		dev_info->flow_type_rss_offloads |= VMXNET3_V4_RSS_MASK;
	}

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = VMXNET3_RX_RING_MAX_SIZE,
		.nb_min = VMXNET3_DEF_RX_RING_SIZE,
		.nb_align = 1,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = VMXNET3_TX_RING_MAX_SIZE,
		.nb_min = VMXNET3_DEF_TX_RING_SIZE,
		.nb_align = 1,
		.nb_seg_max = VMXNET3_TX_MAX_SEG,
		.nb_mtu_seg_max = VMXNET3_MAX_TXD_PER_PKT,
	};

	dev_info->rx_offload_capa = VMXNET3_RX_OFFLOAD_CAP;
	dev_info->rx_queue_offload_capa = 0;
	dev_info->tx_offload_capa = VMXNET3_TX_OFFLOAD_CAP;
	dev_info->tx_queue_offload_capa = 0;

	return 0;
}

static const uint32_t *
vmxnet3_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == vmxnet3_recv_pkts)
		return ptypes;
	return NULL;
}

static int
vmxnet3_dev_mtu_set(struct rte_eth_dev *dev, __rte_unused uint16_t mtu)
{
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "Port %d must be stopped to configure MTU",
			    dev->data->port_id);
		return -EBUSY;
	}

	return 0;
}

static int
vmxnet3_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;

	rte_ether_addr_copy(mac_addr, (struct rte_ether_addr *)(hw->perm_addr));
	vmxnet3_write_mac(hw, mac_addr->addr_bytes);
	return 0;
}

/* return 0 means link status changed, -1 means not changed */
static int
__vmxnet3_dev_link_update(struct rte_eth_dev *dev,
			  __rte_unused int wait_to_complete)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	struct rte_eth_link link;
	uint32_t ret;

	memset(&link, 0, sizeof(link));

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_LINK);
	ret = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);

	if (ret & 0x1)
		link.link_status = ETH_LINK_UP;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_speed = ETH_SPEED_NUM_10G;
	link.link_autoneg = ETH_LINK_FIXED;

	return rte_eth_linkstatus_set(dev, &link);
}

static int
vmxnet3_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	/* Link status doesn't change for stopped dev */
	if (dev->data->dev_started == 0)
		return -1;

	return __vmxnet3_dev_link_update(dev, wait_to_complete);
}

/* Updating rxmode through Vmxnet3_DriverShared structure in adapter */
static void
vmxnet3_dev_set_rxmode(struct vmxnet3_hw *hw, uint32_t feature, int set)
{
	struct Vmxnet3_RxFilterConf *rxConf = &hw->shared->devRead.rxFilterConf;

	if (set)
		rxConf->rxMode = rxConf->rxMode | feature;
	else
		rxConf->rxMode = rxConf->rxMode & (~feature);

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_UPDATE_RX_MODE);
}

/* Promiscuous supported only if Vmxnet3_DriverShared is initialized in adapter */
static int
vmxnet3_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	uint32_t *vf_table = hw->shared->devRead.rxFilterConf.vfTable;

	memset(vf_table, 0, VMXNET3_VFT_TABLE_SIZE);
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_PROMISC, 1);

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);

	return 0;
}

/* Promiscuous supported only if Vmxnet3_DriverShared is initialized in adapter */
static int
vmxnet3_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	uint32_t *vf_table = hw->shared->devRead.rxFilterConf.vfTable;
	uint64_t rx_offloads = dev->data->dev_conf.rxmode.offloads;

	if (rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
		memcpy(vf_table, hw->shadow_vfta, VMXNET3_VFT_TABLE_SIZE);
	else
		memset(vf_table, 0xff, VMXNET3_VFT_TABLE_SIZE);
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_PROMISC, 0);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);

	return 0;
}

/* Allmulticast supported only if Vmxnet3_DriverShared is initialized in adapter */
static int
vmxnet3_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;

	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_ALL_MULTI, 1);

	return 0;
}

/* Allmulticast supported only if Vmxnet3_DriverShared is initialized in adapter */
static int
vmxnet3_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;

	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_ALL_MULTI, 0);

	return 0;
}

/* Enable/disable filter on vlan */
static int
vmxnet3_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vid, int on)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	struct Vmxnet3_RxFilterConf *rxConf = &hw->shared->devRead.rxFilterConf;
	uint32_t *vf_table = rxConf->vfTable;

	/* save state for restore */
	if (on)
		VMXNET3_SET_VFTABLE_ENTRY(hw->shadow_vfta, vid);
	else
		VMXNET3_CLEAR_VFTABLE_ENTRY(hw->shadow_vfta, vid);

	/* don't change active filter if in promiscuous mode */
	if (rxConf->rxMode & VMXNET3_RXM_PROMISC)
		return 0;

	/* set in hardware */
	if (on)
		VMXNET3_SET_VFTABLE_ENTRY(vf_table, vid);
	else
		VMXNET3_CLEAR_VFTABLE_ENTRY(vf_table, vid);

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
	return 0;
}

static int
vmxnet3_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	Vmxnet3_DSDevRead *devRead = &hw->shared->devRead;
	uint32_t *vf_table = devRead->rxFilterConf.vfTable;
	uint64_t rx_offloads = dev->data->dev_conf.rxmode.offloads;

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			devRead->misc.uptFeatures |= UPT1_F_RXVLAN;
		else
			devRead->misc.uptFeatures &= ~UPT1_F_RXVLAN;

		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
				       VMXNET3_CMD_UPDATE_FEATURE);
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
			memcpy(vf_table, hw->shadow_vfta, VMXNET3_VFT_TABLE_SIZE);
		else
			memset(vf_table, 0xff, VMXNET3_VFT_TABLE_SIZE);

		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
				       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
	}

	return 0;
}

static void
vmxnet3_process_events(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	uint32_t events = hw->shared->ecr;

	if (!events)
		return;

	/*
	 * ECR bits when written with 1b are cleared. Hence write
	 * events back to ECR so that the bits which were set will be reset.
	 */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_ECR, events);

	/* Check if link state has changed */
	if (events & VMXNET3_ECR_LINK) {
		PMD_DRV_LOG(DEBUG, "Process events: VMXNET3_ECR_LINK event");
		if (vmxnet3_dev_link_update(dev, 0) == 0)
			rte_eth_dev_callback_process(dev,
						     RTE_ETH_EVENT_INTR_LSC,
						     NULL);
	}

	/* Check if there is an error on xmit/recv queues */
	if (events & (VMXNET3_ECR_TQERR | VMXNET3_ECR_RQERR)) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
				       VMXNET3_CMD_GET_QUEUE_STATUS);

		if (hw->tqd_start->status.stopped)
			PMD_DRV_LOG(ERR, "tq error 0x%x",
				    hw->tqd_start->status.error);

		if (hw->rqd_start->status.stopped)
			PMD_DRV_LOG(ERR, "rq error 0x%x",
				     hw->rqd_start->status.error);

		/* Reset the device */
		/* Have to reset the device */
	}

	if (events & VMXNET3_ECR_DIC)
		PMD_DRV_LOG(DEBUG, "Device implementation change event.");

	if (events & VMXNET3_ECR_DEBUG)
		PMD_DRV_LOG(DEBUG, "Debug event generated by device.");
}

static void
vmxnet3_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	vmxnet3_process_events(dev);

	if (rte_intr_ack(&pci_dev->intr_handle) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");
}

RTE_PMD_REGISTER_PCI(net_vmxnet3, rte_vmxnet3_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_vmxnet3, pci_id_vmxnet3_map);
RTE_PMD_REGISTER_KMOD_DEP(net_vmxnet3, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_LOG_REGISTER(vmxnet3_logtype_init, pmd.net.vmxnet3.init, NOTICE);
RTE_LOG_REGISTER(vmxnet3_logtype_driver, pmd.net.vmxnet3.driver, NOTICE);
