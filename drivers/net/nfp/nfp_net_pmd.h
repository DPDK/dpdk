/*
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net_pmd.h
 *
 * Netronome NFP_NET PMD driver
 */

#ifndef _NFP_NET_PMD_H_
#define _NFP_NET_PMD_H_

#define NFP_NET_PMD_VERSION "0.1"
#define PCI_VENDOR_ID_NETRONOME         0x19ee
#define PCI_DEVICE_ID_NFP4000_PF_NIC    0x4000
#define PCI_DEVICE_ID_NFP6000_PF_NIC    0x6000
#define PCI_DEVICE_ID_NFP6000_VF_NIC    0x6003

/* Forward declaration */
struct nfp_net_adapter;

/*
 * The maximum number of descriptors is limited by design as
 * DPDK uses uint16_t variables for these values
 */
#define NFP_NET_MAX_TX_DESC (32 * 1024)
#define NFP_NET_MIN_TX_DESC 64

#define NFP_NET_MAX_RX_DESC (32 * 1024)
#define NFP_NET_MIN_RX_DESC 64

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

#include <linux/types.h>
#include <rte_io.h>

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

/* TX descriptor format */
#define PCIE_DESC_TX_EOP                (1 << 7)
#define PCIE_DESC_TX_OFFSET_MASK        (0x7f)

/* Flags in the host TX descriptor */
#define PCIE_DESC_TX_CSUM               (1 << 7)
#define PCIE_DESC_TX_IP4_CSUM           (1 << 6)
#define PCIE_DESC_TX_TCP_CSUM           (1 << 5)
#define PCIE_DESC_TX_UDP_CSUM           (1 << 4)
#define PCIE_DESC_TX_VLAN               (1 << 3)
#define PCIE_DESC_TX_LSO                (1 << 2)
#define PCIE_DESC_TX_ENCAP_NONE         (0)
#define PCIE_DESC_TX_ENCAP_VXLAN        (1 << 1)
#define PCIE_DESC_TX_ENCAP_GRE          (1 << 0)

struct nfp_net_tx_desc {
	union {
		struct {
			uint8_t dma_addr_hi; /* High bits of host buf address */
			__le16 dma_len;     /* Length to DMA for this desc */
			uint8_t offset_eop; /* Offset in buf where pkt starts +
					     * highest bit is eop flag.
					     */
			__le32 dma_addr_lo; /* Low 32bit of host buf addr */

			__le16 mss;         /* MSS to be used for LSO */
			uint8_t lso_hdrlen; /* LSO, where the data starts */
			uint8_t flags;      /* TX Flags, see @PCIE_DESC_TX_* */

			union {
				struct {
					/*
					 * L3 and L4 header offsets required
					 * for TSOv2
					 */
					uint8_t l3_offset;
					uint8_t l4_offset;
				};
				__le16 vlan; /* VLAN tag to add if indicated */
			};
			__le16 data_len;    /* Length of frame + meta data */
		} __attribute__((__packed__));
		__le32 vals[4];
	};
};

struct nfp_net_txq {
	struct nfp_net_hw *hw; /* Backpointer to nfp_net structure */

	/*
	 * Queue information: @qidx is the queue index from Linux's
	 * perspective.  @tx_qcidx is the index of the Queue
	 * Controller Peripheral queue relative to the TX queue BAR.
	 * @cnt is the size of the queue in number of
	 * descriptors. @qcp_q is a pointer to the base of the queue
	 * structure on the NFP
	 */
	uint8_t *qcp_q;

	/*
	 * Read and Write pointers.  @wr_p and @rd_p are host side pointer,
	 * they are free running and have little relation to the QCP pointers *
	 * @qcp_rd_p is a local copy queue controller peripheral read pointer
	 */

	uint32_t wr_p;
	uint32_t rd_p;

	uint32_t tx_count;

	uint32_t tx_free_thresh;

	/*
	 * For each descriptor keep a reference to the mbuf and
	 * DMA address used until completion is signalled.
	 */
	struct {
		struct rte_mbuf *mbuf;
	} *txbufs;

	/*
	 * Information about the host side queue location. @txds is
	 * the virtual address for the queue, @dma is the DMA address
	 * of the queue and @size is the size in bytes for the queue
	 * (needed for free)
	 */
	struct nfp_net_tx_desc *txds;

	/*
	 * At this point 48 bytes have been used for all the fields in the
	 * TX critical path. We have room for 8 bytes and still all placed
	 * in a cache line. We are not using the threshold values below but
	 * if we need to, we can add the most used in the remaining bytes.
	 */
	uint32_t tx_rs_thresh; /* not used by now. Future? */
	uint32_t tx_pthresh;   /* not used by now. Future? */
	uint32_t tx_hthresh;   /* not used by now. Future? */
	uint32_t tx_wthresh;   /* not used by now. Future? */
	uint16_t port_id;
	int qidx;
	int tx_qcidx;
	__le64 dma;
} __attribute__ ((__aligned__(64)));

/* RX and freelist descriptor format */
#define PCIE_DESC_RX_DD                 (1 << 7)
#define PCIE_DESC_RX_META_LEN_MASK      (0x7f)

/* Flags in the RX descriptor */
#define PCIE_DESC_RX_RSS                (1 << 15)
#define PCIE_DESC_RX_I_IP4_CSUM         (1 << 14)
#define PCIE_DESC_RX_I_IP4_CSUM_OK      (1 << 13)
#define PCIE_DESC_RX_I_TCP_CSUM         (1 << 12)
#define PCIE_DESC_RX_I_TCP_CSUM_OK      (1 << 11)
#define PCIE_DESC_RX_I_UDP_CSUM         (1 << 10)
#define PCIE_DESC_RX_I_UDP_CSUM_OK      (1 <<  9)
#define PCIE_DESC_RX_SPARE              (1 <<  8)
#define PCIE_DESC_RX_EOP                (1 <<  7)
#define PCIE_DESC_RX_IP4_CSUM           (1 <<  6)
#define PCIE_DESC_RX_IP4_CSUM_OK        (1 <<  5)
#define PCIE_DESC_RX_TCP_CSUM           (1 <<  4)
#define PCIE_DESC_RX_TCP_CSUM_OK        (1 <<  3)
#define PCIE_DESC_RX_UDP_CSUM           (1 <<  2)
#define PCIE_DESC_RX_UDP_CSUM_OK        (1 <<  1)
#define PCIE_DESC_RX_VLAN               (1 <<  0)

#define PCIE_DESC_RX_L4_CSUM_OK         (PCIE_DESC_RX_TCP_CSUM_OK | \
					 PCIE_DESC_RX_UDP_CSUM_OK)
struct nfp_net_rx_desc {
	union {
		/* Freelist descriptor */
		struct {
			uint8_t dma_addr_hi;
			__le16 spare;
			uint8_t dd;

			__le32 dma_addr_lo;
		} __attribute__((__packed__)) fld;

		/* RX descriptor */
		struct {
			__le16 data_len;
			uint8_t reserved;
			uint8_t meta_len_dd;

			__le16 flags;
			__le16 vlan;
		} __attribute__((__packed__)) rxd;

		__le32 vals[2];
	};
};

struct nfp_net_rx_buff {
	struct rte_mbuf *mbuf;
};

struct nfp_net_rxq {
	struct nfp_net_hw *hw;	/* Backpointer to nfp_net structure */

	 /*
	  * @qcp_fl and @qcp_rx are pointers to the base addresses of the
	  * freelist and RX queue controller peripheral queue structures on the
	  * NFP
	  */
	uint8_t *qcp_fl;
	uint8_t *qcp_rx;

	/*
	 * Read and Write pointers.  @wr_p and @rd_p are host side
	 * pointer, they are free running and have little relation to
	 * the QCP pointers. @wr_p is where the driver adds new
	 * freelist descriptors and @rd_p is where the driver start
	 * reading descriptors for newly arrive packets from.
	 */
	uint32_t rd_p;

	/*
	 * For each buffer placed on the freelist, record the
	 * associated SKB
	 */
	struct nfp_net_rx_buff *rxbufs;

	/*
	 * Information about the host side queue location.  @rxds is
	 * the virtual address for the queue
	 */
	struct nfp_net_rx_desc *rxds;

	/*
	 * The mempool is created by the user specifying a mbuf size.
	 * We save here the reference of the mempool needed in the RX
	 * path and the mbuf size for checking received packets can be
	 * safely copied to the mbuf using the NFP_NET_RX_OFFSET
	 */
	struct rte_mempool *mem_pool;
	uint16_t mbuf_size;

	/*
	 * Next two fields are used for giving more free descriptors
	 * to the NFP
	 */
	uint16_t rx_free_thresh;
	uint16_t nb_rx_hold;

	 /* the size of the queue in number of descriptors */
	uint16_t rx_count;

	/*
	 * Fields above this point fit in a single cache line and are all used
	 * in the RX critical path. Fields below this point are just used
	 * during queue configuration or not used at all (yet)
	 */

	/* referencing dev->data->port_id */
	uint16_t port_id;

	uint8_t  crc_len; /* Not used by now */
	uint8_t  drop_en; /* Not used by now */

	/* DMA address of the queue */
	__le64 dma;

	/*
	 * Queue information: @qidx is the queue index from Linux's
	 * perspective.  @fl_qcidx is the index of the Queue
	 * Controller peripheral queue relative to the RX queue BAR
	 * used for the freelist and @rx_qcidx is the Queue Controller
	 * Peripheral index for the RX queue.
	 */
	int qidx;
	int fl_qcidx;
	int rx_qcidx;
} __attribute__ ((__aligned__(64)));

struct nfp_net_hw {
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

	uint8_t mac_addr[ETHER_ADDR_LEN];

	/* Records starting point for counters */
	struct rte_eth_stats eth_stats_base;

	struct nfp_cpp *cpp;
	struct nfp_cpp_area *ctrl_area;
	struct nfp_cpp_area *hwqueues_area;
	struct nfp_cpp_area *msix_area;

	uint8_t *hw_queues;
	uint8_t is_pf;
	uint8_t pf_port_idx;
	uint8_t pf_multiport_enabled;
	uint8_t total_ports;

	union eth_table_entry *eth_table;

	struct nfp_hwinfo *hwinfo;
	struct nfp_rtsym_table *sym_tbl;
	uint32_t nfp_cpp_service_id;
};

struct nfp_net_adapter {
	struct nfp_net_hw hw;
};

#define NFP_NET_DEV_PRIVATE_TO_HW(adapter)\
	(&((struct nfp_net_adapter *)adapter)->hw)

#endif /* _NFP_NET_PMD_H_ */
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
