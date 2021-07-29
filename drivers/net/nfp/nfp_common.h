/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net_pmd.h
 *
 * Netronome NFP_NET PMD driver
 */

#ifndef _NFP_COMMON_H_
#define _NFP_COMMON_H_

#define NFP_NET_PMD_VERSION "0.1"
#define PCI_VENDOR_ID_NETRONOME         0x19ee
#define PCI_DEVICE_ID_NFP4000_PF_NIC    0x4000
#define PCI_DEVICE_ID_NFP6000_PF_NIC    0x6000
#define PCI_DEVICE_ID_NFP6000_VF_NIC    0x6003

/* Forward declaration */
struct nfp_net_adapter;

#define NFP_TX_MAX_SEG     UINT8_MAX
#define NFP_TX_MAX_MTU_SEG 8

/* Bar allocation */
#define NFP_NET_CRTL_BAR        0
#define NFP_NET_TX_BAR          2
#define NFP_NET_RX_BAR          2
#define NFP_QCP_QUEUE_AREA_SZ			0x80000

/* Macros for accessing the Queue Controller Peripheral 'CSRs' */
#define NFP_QCP_QUEUE_OFF(_x)                 ((_x) * 0x800)
#define NFP_QCP_QUEUE_ADD_RPTR                  0x0000
#define NFP_QCP_QUEUE_ADD_WPTR                  0x0004
#define NFP_QCP_QUEUE_STS_LO                    0x0008
#define NFP_QCP_QUEUE_STS_LO_READPTR_mask     (0x3ffff)
#define NFP_QCP_QUEUE_STS_HI                    0x000c
#define NFP_QCP_QUEUE_STS_HI_WRITEPTR_mask    (0x3ffff)

/* The offset of the queue controller queues in the PCIe Target */
#define NFP_PCIE_QUEUE(_q) (0x80000 + (NFP_QCP_QUEUE_ADDR_SZ * ((_q) & 0xff)))

/* Maximum value which can be added to a queue with one transaction */
#define NFP_QCP_MAX_ADD	0x7f

/* Interrupt definitions */
#define NFP_NET_IRQ_LSC_IDX             0

/* Default values for RX/TX configuration */
#define DEFAULT_RX_FREE_THRESH  32
#define DEFAULT_RX_PTHRESH      8
#define DEFAULT_RX_HTHRESH      8
#define DEFAULT_RX_WTHRESH      0

#define DEFAULT_TX_RS_THRESH	32
#define DEFAULT_TX_FREE_THRESH  32
#define DEFAULT_TX_PTHRESH      32
#define DEFAULT_TX_HTHRESH      0
#define DEFAULT_TX_WTHRESH      0
#define DEFAULT_TX_RSBIT_THRESH 32

/* Alignment for dma zones */
#define NFP_MEMZONE_ALIGN	128

/*
 * This is used by the reconfig protocol. It sets the maximum time waiting in
 * milliseconds before a reconfig timeout happens.
 */
#define NFP_NET_POLL_TIMEOUT    5000

#define NFP_QCP_QUEUE_ADDR_SZ   (0x800)

#define NFP_NET_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define NFP_NET_LINK_UP_CHECK_TIMEOUT   1000 /* ms */

/* Version number helper defines */
#define NFD_CFG_CLASS_VER_msk       0xff
#define NFD_CFG_CLASS_VER_shf       24
#define NFD_CFG_CLASS_VER(x)        (((x) & 0xff) << 24)
#define NFD_CFG_CLASS_VER_of(x)     (((x) >> 24) & 0xff)
#define NFD_CFG_CLASS_TYPE_msk      0xff
#define NFD_CFG_CLASS_TYPE_shf      16
#define NFD_CFG_CLASS_TYPE(x)       (((x) & 0xff) << 16)
#define NFD_CFG_CLASS_TYPE_of(x)    (((x) >> 16) & 0xff)
#define NFD_CFG_MAJOR_VERSION_msk   0xff
#define NFD_CFG_MAJOR_VERSION_shf   8
#define NFD_CFG_MAJOR_VERSION(x)    (((x) & 0xff) << 8)
#define NFD_CFG_MAJOR_VERSION_of(x) (((x) >> 8) & 0xff)
#define NFD_CFG_MINOR_VERSION_msk   0xff
#define NFD_CFG_MINOR_VERSION_shf   0
#define NFD_CFG_MINOR_VERSION(x)    (((x) & 0xff) << 0)
#define NFD_CFG_MINOR_VERSION_of(x) (((x) >> 0) & 0xff)

/* Number of supported physical ports */
#define NFP_MAX_PHYPORTS	12

#include <linux/types.h>
#include <rte_io.h>

/* nfp_qcp_ptr - Read or Write Pointer of a queue */
enum nfp_qcp_ptr {
	NFP_QCP_READ_PTR = 0,
	NFP_QCP_WRITE_PTR
};

struct nfp_pf_dev {
	/* Backpointer to associated pci device */
	struct rte_pci_device *pci_dev;

	/* Array of physical ports belonging to this PF */
	struct nfp_net_hw *ports[NFP_MAX_PHYPORTS];

	/* Current values for control */
	uint32_t ctrl;

	uint8_t *ctrl_bar;
	uint8_t *tx_bar;
	uint8_t *rx_bar;

	uint8_t *qcp_cfg;
	rte_spinlock_t reconfig_lock;

	uint16_t flbufsz;
	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;
#if defined(DSTQ_SELECTION)
#if DSTQ_SELECTION
	uint16_t device_function;
#endif
#endif

	struct nfp_cpp *cpp;
	struct nfp_cpp_area *ctrl_area;
	struct nfp_cpp_area *hwqueues_area;
	struct nfp_cpp_area *msix_area;

	uint8_t *hw_queues;
	uint8_t total_phyports;
	bool	multiport;

	union eth_table_entry *eth_table;

	struct nfp_hwinfo *hwinfo;
	struct nfp_rtsym_table *sym_tbl;
	uint32_t nfp_cpp_service_id;
};

struct nfp_net_hw {
	/* Backpointer to the PF this port belongs to */
	struct nfp_pf_dev *pf_dev;

	/* Backpointer to the eth_dev of this port*/
	struct rte_eth_dev *eth_dev;

	/* Info from the firmware */
	uint32_t ver;
	uint32_t cap;
	uint32_t max_mtu;
	uint32_t mtu;
	uint32_t rx_offset;

	/* Current values for control */
	uint32_t ctrl;

	uint8_t *ctrl_bar;
	uint8_t *tx_bar;
	uint8_t *rx_bar;

	int stride_rx;
	int stride_tx;

	uint8_t *qcp_cfg;
	rte_spinlock_t reconfig_lock;

	uint32_t max_tx_queues;
	uint32_t max_rx_queues;
	uint16_t flbufsz;
	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;
#if defined(DSTQ_SELECTION)
#if DSTQ_SELECTION
	uint16_t device_function;
#endif
#endif

	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];

	/* Records starting point for counters */
	struct rte_eth_stats eth_stats_base;

	struct nfp_cpp *cpp;
	struct nfp_cpp_area *ctrl_area;
	struct nfp_cpp_area *hwqueues_area;
	struct nfp_cpp_area *msix_area;

	uint8_t *hw_queues;
	/* Sequential physical port number */
	uint8_t idx;
	/* Internal port number as seen from NFP */
	uint8_t nfp_idx;
	bool	is_phyport;

	union eth_table_entry *eth_table;

	uint32_t nfp_cpp_service_id;
};

struct nfp_net_adapter {
	struct nfp_net_hw hw;
};

static inline uint8_t nn_readb(volatile const void *addr)
{
	return rte_read8(addr);
}

static inline void nn_writeb(uint8_t val, volatile void *addr)
{
	rte_write8(val, addr);
}

static inline uint32_t nn_readl(volatile const void *addr)
{
	return rte_read32(addr);
}

static inline void nn_writel(uint32_t val, volatile void *addr)
{
	rte_write32(val, addr);
}

static inline void nn_writew(uint16_t val, volatile void *addr)
{
	rte_write16(val, addr);
}

static inline uint64_t nn_readq(volatile void *addr)
{
	const volatile uint32_t *p = addr;
	uint32_t low, high;

	high = nn_readl((volatile const void *)(p + 1));
	low = nn_readl((volatile const void *)p);

	return low + ((uint64_t)high << 32);
}

static inline void nn_writeq(uint64_t val, volatile void *addr)
{
	nn_writel(val >> 32, (volatile char *)addr + 4);
	nn_writel(val, addr);
}

/*
 * Functions to read/write from/to Config BAR
 * Performs any endian conversion necessary.
 */
static inline uint8_t
nn_cfg_readb(struct nfp_net_hw *hw, int off)
{
	return nn_readb(hw->ctrl_bar + off);
}

static inline void
nn_cfg_writeb(struct nfp_net_hw *hw, int off, uint8_t val)
{
	nn_writeb(val, hw->ctrl_bar + off);
}

static inline uint32_t
nn_cfg_readl(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_32(nn_readl(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writel(struct nfp_net_hw *hw, int off, uint32_t val)
{
	nn_writel(rte_cpu_to_le_32(val), hw->ctrl_bar + off);
}

static inline uint64_t
nn_cfg_readq(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_64(nn_readq(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writeq(struct nfp_net_hw *hw, int off, uint64_t val)
{
	nn_writeq(rte_cpu_to_le_64(val), hw->ctrl_bar + off);
}

/*
 * nfp_qcp_ptr_add - Add the value to the selected pointer of a queue
 * @q: Base address for queue structure
 * @ptr: Add to the Read or Write pointer
 * @val: Value to add to the queue pointer
 *
 * If @val is greater than @NFP_QCP_MAX_ADD multiple writes are performed.
 */
static inline void
nfp_qcp_ptr_add(uint8_t *q, enum nfp_qcp_ptr ptr, uint32_t val)
{
	uint32_t off;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_ADD_RPTR;
	else
		off = NFP_QCP_QUEUE_ADD_WPTR;

	while (val > NFP_QCP_MAX_ADD) {
		nn_writel(rte_cpu_to_le_32(NFP_QCP_MAX_ADD), q + off);
		val -= NFP_QCP_MAX_ADD;
}

nn_writel(rte_cpu_to_le_32(val), q + off);
}

/*
 * nfp_qcp_read - Read the current Read/Write pointer value for a queue
 * @q:  Base address for queue structure
 * @ptr: Read or Write pointer
 */
static inline uint32_t
nfp_qcp_read(uint8_t *q, enum nfp_qcp_ptr ptr)
{
	uint32_t off;
	uint32_t val;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_STS_LO;
	else
		off = NFP_QCP_QUEUE_STS_HI;

	val = rte_cpu_to_le_32(nn_readl(q + off));

	if (ptr == NFP_QCP_READ_PTR)
		return val & NFP_QCP_QUEUE_STS_LO_READPTR_mask;
	else
		return val & NFP_QCP_QUEUE_STS_HI_WRITEPTR_mask;
}

/* Prototypes for common NFP functions */
int nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t ctrl, uint32_t update);
int nfp_net_configure(struct rte_eth_dev *dev);
void nfp_net_enable_queues(struct rte_eth_dev *dev);
void nfp_net_disable_queues(struct rte_eth_dev *dev);
void nfp_net_params_setup(struct nfp_net_hw *hw);
void nfp_eth_copy_mac(uint8_t *dst, const uint8_t *src);
void nfp_net_write_mac(struct nfp_net_hw *hw, uint8_t *mac);
int nfp_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);
int nfp_configure_rx_interrupt(struct rte_eth_dev *dev,
			       struct rte_intr_handle *intr_handle);
uint32_t nfp_check_offloads(struct rte_eth_dev *dev);
int nfp_net_promisc_enable(struct rte_eth_dev *dev);
int nfp_net_promisc_disable(struct rte_eth_dev *dev);
int nfp_net_link_update(struct rte_eth_dev *dev,
			__rte_unused int wait_to_complete);
int nfp_net_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int nfp_net_stats_reset(struct rte_eth_dev *dev);
int nfp_net_infos_get(struct rte_eth_dev *dev,
		      struct rte_eth_dev_info *dev_info);
const uint32_t *nfp_net_supported_ptypes_get(struct rte_eth_dev *dev);
int nfp_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);
int nfp_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);
void nfp_net_params_setup(struct nfp_net_hw *hw);
void nfp_net_cfg_queue_setup(struct nfp_net_hw *hw);
void nfp_eth_copy_mac(uint8_t *dst, const uint8_t *src);
void nfp_net_dev_interrupt_handler(void *param);
int nfp_net_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
int nfp_net_vlan_offload_set(struct rte_eth_dev *dev, int mask);
int nfp_net_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int nfp_net_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size);
int nfp_net_rss_hash_update(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf);
int nfp_net_rss_hash_conf_get(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf);
int nfp_net_rss_config_default(struct rte_eth_dev *dev);

#define NFP_NET_DEV_PRIVATE_TO_HW(adapter)\
	(&((struct nfp_net_adapter *)adapter)->hw)

#define NFP_NET_DEV_PRIVATE_TO_PF(dev_priv)\
	(((struct nfp_net_hw *)dev_priv)->pf_dev)

#endif /* _NFP_COMMON_H_ */
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
