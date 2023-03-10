/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net_pmd.h
 *
 * Netronome NFP_NET PMD
 */

#ifndef _NFP_COMMON_H_
#define _NFP_COMMON_H_

#include "nfp_ctrl.h"

#define NFP_NET_PMD_VERSION "0.1"
#define PCI_VENDOR_ID_NETRONOME         0x19ee
#define PCI_VENDOR_ID_CORIGINE          0x1da8

#define PCI_DEVICE_ID_NFP3800_PF_NIC    0x3800
#define PCI_DEVICE_ID_NFP3800_VF_NIC    0x3803
#define PCI_DEVICE_ID_NFP4000_PF_NIC    0x4000
#define PCI_DEVICE_ID_NFP6000_PF_NIC    0x6000
#define PCI_DEVICE_ID_NFP6000_VF_NIC    0x6003  /* Include NFP4000VF */

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

#define NFP_PCIE_QCP_NFP3800_OFFSET            0x400000
#define NFP_PCIE_QCP_NFP6000_OFFSET            0x80000
#define NFP_PCIE_QUEUE_NFP3800_MASK            0x1ff
#define NFP_PCIE_QUEUE_NFP6000_MASK            0xff
#define NFP_PCIE_QCP_PF_OFFSET                 0x0
#define NFP_PCIE_QCP_VF_OFFSET                 0x0

/* The offset of the queue controller queues in the PCIe Target */
#define NFP_PCIE_QUEUE(_offset, _q, _mask)    \
		((_offset) + (NFP_QCP_QUEUE_ADDR_SZ * ((_q) & (_mask))))

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

/* Maximum supported NFP frame size (MTU + layer 2 headers) */
#define NFP_FRAME_SIZE_MAX	10048
#define DEFAULT_FLBUF_SIZE        9216

#include <linux/types.h>
#include <rte_io.h>

/* Firmware application ID's */
enum nfp_app_fw_id {
	NFP_APP_FW_CORE_NIC               = 0x1,
	NFP_APP_FW_FLOWER_NIC             = 0x3,
};

/* nfp_qcp_ptr - Read or Write Pointer of a queue */
enum nfp_qcp_ptr {
	NFP_QCP_READ_PTR = 0,
	NFP_QCP_WRITE_PTR
};

enum nfp_net_meta_format {
	NFP_NET_METAFORMAT_SINGLE,
	NFP_NET_METAFORMAT_CHAINED,
};

struct nfp_pf_dev {
	/* Backpointer to associated pci device */
	struct rte_pci_device *pci_dev;

	enum nfp_app_fw_id app_fw_id;

	/* Pointer to the app running on the PF */
	void *app_fw_priv;

	/* The eth table reported by firmware */
	struct nfp_eth_table *nfp_eth_table;

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

	union eth_table_entry *eth_table;

	struct nfp_hwinfo *hwinfo;
	struct nfp_rtsym_table *sym_tbl;

	/* service id of cpp bridge service */
	uint32_t cpp_bridge_id;
};

struct nfp_app_fw_nic {
	/* Backpointer to the PF device */
	struct nfp_pf_dev *pf_dev;

	/*
	 * Array of physical ports belonging to the this CoreNIC app
	 * This is really a list of vNIC's. One for each physical port
	 */
	struct nfp_net_hw *ports[NFP_MAX_PHYPORTS];

	bool multiport;
	uint8_t total_phyports;
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
	enum nfp_net_meta_format meta_format;

	/* Current values for control */
	uint32_t ctrl;

	uint8_t *ctrl_bar;
	uint8_t *tx_bar;
	uint8_t *rx_bar;

	int stride_rx;
	int stride_tx;

	uint16_t vxlan_ports[NFP_NET_N_VXLAN_PORTS];
	uint8_t vxlan_usecnt[NFP_NET_N_VXLAN_PORTS];

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

	union eth_table_entry *eth_table;
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

static inline uint16_t nn_readw(volatile const void *addr)
{
	return rte_read16(addr);
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

static inline uint16_t
nn_cfg_readw(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_16(nn_readw(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writew(struct nfp_net_hw *hw, int off, uint16_t val)
{
	nn_writew(rte_cpu_to_le_16(val), hw->ctrl_bar + off);
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
 */
static inline void
nfp_qcp_ptr_add(uint8_t *q, enum nfp_qcp_ptr ptr, uint32_t val)
{
	uint32_t off;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_ADD_RPTR;
	else
		off = NFP_QCP_QUEUE_ADD_WPTR;

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

static inline uint32_t
nfp_pci_queue(struct rte_pci_device *pdev, uint16_t queue)
{
	switch (pdev->id.device_id) {
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
		return NFP_PCIE_QUEUE(NFP_PCIE_QCP_PF_OFFSET, queue,
				NFP_PCIE_QUEUE_NFP6000_MASK);
	case PCI_DEVICE_ID_NFP3800_VF_NIC:
		return NFP_PCIE_QUEUE(NFP_PCIE_QCP_VF_OFFSET, queue,
				NFP_PCIE_QUEUE_NFP3800_MASK);
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		return NFP_PCIE_QUEUE(NFP_PCIE_QCP_VF_OFFSET, queue,
				NFP_PCIE_QUEUE_NFP6000_MASK);
	default:
		return NFP_PCIE_QUEUE(NFP_PCIE_QCP_PF_OFFSET, queue,
				NFP_PCIE_QUEUE_NFP3800_MASK);
	}
}

/* Prototypes for common NFP functions */
int nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t ctrl, uint32_t update);
int nfp_net_configure(struct rte_eth_dev *dev);
void nfp_net_log_device_information(const struct nfp_net_hw *hw);
void nfp_net_enable_queues(struct rte_eth_dev *dev);
void nfp_net_disable_queues(struct rte_eth_dev *dev);
void nfp_net_params_setup(struct nfp_net_hw *hw);
void nfp_eth_copy_mac(uint8_t *dst, const uint8_t *src);
void nfp_net_write_mac(struct nfp_net_hw *hw, uint8_t *mac);
int nfp_net_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);
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
void nfp_net_dev_interrupt_delayed_handler(void *param);
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
void nfp_net_stop_rx_queue(struct rte_eth_dev *dev);
void nfp_net_close_rx_queue(struct rte_eth_dev *dev);
void nfp_net_stop_tx_queue(struct rte_eth_dev *dev);
void nfp_net_close_tx_queue(struct rte_eth_dev *dev);
int nfp_net_set_vxlan_port(struct nfp_net_hw *hw, size_t idx, uint16_t port);
int nfp_net_rx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_rx_desc,
		uint16_t *max_rx_desc);
int nfp_net_tx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_tx_desc,
		uint16_t *max_tx_desc);
int nfp_net_check_dma_mask(struct nfp_net_hw *hw, char *name);
void nfp_net_init_metadata_format(struct nfp_net_hw *hw);

#define NFP_NET_DEV_PRIVATE_TO_HW(adapter)\
	(&((struct nfp_net_adapter *)adapter)->hw)

#define NFP_NET_DEV_PRIVATE_TO_PF(dev_priv)\
	(((struct nfp_net_hw *)dev_priv)->pf_dev)

#define NFP_PRIV_TO_APP_FW_NIC(app_fw_priv)\
	((struct nfp_app_fw_nic *)app_fw_priv)

#define NFP_PRIV_TO_APP_FW_FLOWER(app_fw_priv)\
	((struct nfp_app_fw_flower *)app_fw_priv)

#endif /* _NFP_COMMON_H_ */
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
