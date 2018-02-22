/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_H_
#define _IXGBE_H_

#ifndef IXGBE_NO_LRO
#include <net/tcp.h>
#endif

#include <linux/pci.h>
#include <linux/netdevice.h>
#ifdef HAVE_IRQ_AFFINITY_HINT
#include <linux/cpumask.h>
#endif /* HAVE_IRQ_AFFINITY_HINT */
#include <linux/vmalloc.h>

#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif
#ifdef NETIF_F_HW_VLAN_TX
#include <linux/if_vlan.h>
#endif
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
#define IXGBE_DCA
#include <linux/dca.h>
#endif
#include "ixgbe_dcb.h"

#include "kcompat.h"

#ifdef HAVE_SCTP
#include <linux/sctp.h>
#endif

#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#define IXGBE_FCOE
#include "ixgbe_fcoe.h"
#endif /* CONFIG_FCOE or CONFIG_FCOE_MODULE */

#if defined(CONFIG_PTP_1588_CLOCK) || defined(CONFIG_PTP_1588_CLOCK_MODULE)
#define HAVE_IXGBE_PTP
#endif

#include "ixgbe_api.h"

#define PFX "ixgbe: "
#define DPRINTK(nlevel, klevel, fmt, args...) \
	((void)((NETIF_MSG_##nlevel & adapter->msg_enable) && \
	printk(KERN_##klevel PFX "%s: %s: " fmt, adapter->netdev->name, \
		__func__ , ## args)))

/* TX/RX descriptor defines */
#define IXGBE_DEFAULT_TXD		512
#define IXGBE_DEFAULT_TX_WORK		256
#define IXGBE_MAX_TXD			4096
#define IXGBE_MIN_TXD			64

#define IXGBE_DEFAULT_RXD		512
#define IXGBE_DEFAULT_RX_WORK		256
#define IXGBE_MAX_RXD			4096
#define IXGBE_MIN_RXD			64


/* flow control */
#define IXGBE_MIN_FCRTL			0x40
#define IXGBE_MAX_FCRTL			0x7FF80
#define IXGBE_MIN_FCRTH			0x600
#define IXGBE_MAX_FCRTH			0x7FFF0
#define IXGBE_DEFAULT_FCPAUSE		0xFFFF
#define IXGBE_MIN_FCPAUSE		0
#define IXGBE_MAX_FCPAUSE		0xFFFF

/* Supported Rx Buffer Sizes */
#define IXGBE_RXBUFFER_512	512    /* Used for packet split */
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
#define IXGBE_RXBUFFER_1536	1536
#define IXGBE_RXBUFFER_2K	2048
#define IXGBE_RXBUFFER_3K	3072
#define IXGBE_RXBUFFER_4K	4096
#define IXGBE_RXBUFFER_7K	7168
#define IXGBE_RXBUFFER_8K	8192
#define IXGBE_RXBUFFER_15K	15360
#endif /* CONFIG_IXGBE_DISABLE_PACKET_SPLIT */
#define IXGBE_MAX_RXBUFFER	16384  /* largest size for single descriptor */

/*
 * NOTE: netdev_alloc_skb reserves up to 64 bytes, NET_IP_ALIGN mans we
 * reserve 2 more, and skb_shared_info adds an additional 384 bytes more,
 * this adds up to 512 bytes of extra data meaning the smallest allocation
 * we could have is 1K.
 * i.e. RXBUFFER_512 --> size-1024 slab
 */
#define IXGBE_RX_HDR_SIZE	IXGBE_RXBUFFER_512

#define MAXIMUM_ETHERNET_VLAN_SIZE	(VLAN_ETH_FRAME_LEN + ETH_FCS_LEN)

/* How many Rx Buffers do we bundle into one write to the hardware ? */
#define IXGBE_RX_BUFFER_WRITE	16	/* Must be power of 2 */

#define IXGBE_TX_FLAGS_CSUM		(u32)(1)
#define IXGBE_TX_FLAGS_HW_VLAN		(u32)(1 << 1)
#define IXGBE_TX_FLAGS_SW_VLAN		(u32)(1 << 2)
#define IXGBE_TX_FLAGS_TSO		(u32)(1 << 3)
#define IXGBE_TX_FLAGS_IPV4		(u32)(1 << 4)
#define IXGBE_TX_FLAGS_FCOE		(u32)(1 << 5)
#define IXGBE_TX_FLAGS_FSO		(u32)(1 << 6)
#define IXGBE_TX_FLAGS_TXSW		(u32)(1 << 7)
#define IXGBE_TX_FLAGS_TSTAMP		(u32)(1 << 8)
#define IXGBE_TX_FLAGS_VLAN_MASK	0xffff0000
#define IXGBE_TX_FLAGS_VLAN_PRIO_MASK	0xe0000000
#define IXGBE_TX_FLAGS_VLAN_PRIO_SHIFT	29
#define IXGBE_TX_FLAGS_VLAN_SHIFT	16

#define IXGBE_MAX_RX_DESC_POLL		10

#define IXGBE_MAX_VF_MC_ENTRIES		30
#define IXGBE_MAX_VF_FUNCTIONS		64
#define IXGBE_MAX_VFTA_ENTRIES		128
#define MAX_EMULATION_MAC_ADDRS		16
#define IXGBE_MAX_PF_MACVLANS		15
#define IXGBE_82599_VF_DEVICE_ID	0x10ED
#define IXGBE_X540_VF_DEVICE_ID		0x1515

#ifdef CONFIG_PCI_IOV
#define VMDQ_P(p)	((p) + adapter->num_vfs)
#else
#define VMDQ_P(p)	(p)
#endif

#define UPDATE_VF_COUNTER_32bit(reg, last_counter, counter)	\
	{							\
		u32 current_counter = IXGBE_READ_REG(hw, reg);	\
		if (current_counter < last_counter)		\
			counter += 0x100000000LL;		\
		last_counter = current_counter;			\
		counter &= 0xFFFFFFFF00000000LL;		\
		counter |= current_counter;			\
	}

#define UPDATE_VF_COUNTER_36bit(reg_lsb, reg_msb, last_counter, counter) \
	{								 \
		u64 current_counter_lsb = IXGBE_READ_REG(hw, reg_lsb);	 \
		u64 current_counter_msb = IXGBE_READ_REG(hw, reg_msb);	 \
		u64 current_counter = (current_counter_msb << 32) |	 \
			current_counter_lsb;				 \
		if (current_counter < last_counter)			 \
			counter += 0x1000000000LL;			 \
		last_counter = current_counter;				 \
		counter &= 0xFFFFFFF000000000LL;			 \
		counter |= current_counter;				 \
	}

struct vf_stats {
	u64 gprc;
	u64 gorc;
	u64 gptc;
	u64 gotc;
	u64 mprc;
};

struct vf_data_storage {
	unsigned char vf_mac_addresses[ETH_ALEN];
	u16 vf_mc_hashes[IXGBE_MAX_VF_MC_ENTRIES];
	u16 num_vf_mc_hashes;
	u16 default_vf_vlan_id;
	u16 vlans_enabled;
	bool clear_to_send;
	struct vf_stats vfstats;
	struct vf_stats last_vfstats;
	struct vf_stats saved_rst_vfstats;
	bool pf_set_mac;
	u16 pf_vlan; /* When set, guest VLAN config not allowed. */
	u16 pf_qos;
	u16 tx_rate;
	u16 vlan_count;
	u8 spoofchk_enabled;
	struct pci_dev *vfdev;
};

struct vf_macvlans {
	struct list_head l;
	int vf;
	bool free;
	bool is_macvlan;
	u8 vf_macvlan[ETH_ALEN];
};

#ifndef IXGBE_NO_LRO
#define IXGBE_LRO_MAX		32	/*Maximum number of LRO descriptors*/
#define IXGBE_LRO_GLOBAL	10

struct ixgbe_lro_stats {
	u32 flushed;
	u32 coal;
};

/*
 * ixgbe_lro_header - header format to be aggregated by LRO
 * @iph: IP header without options
 * @tcp: TCP header
 * @ts:  Optional TCP timestamp data in TCP options
 *
 * This structure relies on the check above that verifies that the header
 * is IPv4 and does not contain any options.
 */
struct ixgbe_lrohdr {
	struct iphdr iph;
	struct tcphdr th;
	__be32 ts[0];
};

struct ixgbe_lro_list {
	struct sk_buff_head active;
	struct ixgbe_lro_stats stats;
};

#endif /* IXGBE_NO_LRO */
#define IXGBE_MAX_TXD_PWR	14
#define IXGBE_MAX_DATA_PER_TXD	(1 << IXGBE_MAX_TXD_PWR)

/* Tx Descriptors needed, worst case */
#define TXD_USE_COUNT(S)	DIV_ROUND_UP((S), IXGBE_MAX_DATA_PER_TXD)
#ifdef MAX_SKB_FRAGS
#define DESC_NEEDED	((MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE)) + 4)
#else
#define DESC_NEEDED	4
#endif

/* wrapper around a pointer to a socket buffer,
 * so a DMA handle can be stored along with the buffer */
struct ixgbe_tx_buffer {
	union ixgbe_adv_tx_desc *next_to_watch;
	unsigned long time_stamp;
	struct sk_buff *skb;
	unsigned int bytecount;
	unsigned short gso_segs;
	__be16 protocol;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};

struct ixgbe_rx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
#ifndef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
	struct page *page;
	unsigned int page_offset;
#endif
};

struct ixgbe_queue_stats {
	u64 packets;
	u64 bytes;
};

struct ixgbe_tx_queue_stats {
	u64 restart_queue;
	u64 tx_busy;
	u64 tx_done_old;
};

struct ixgbe_rx_queue_stats {
	u64 rsc_count;
	u64 rsc_flush;
	u64 non_eop_descs;
	u64 alloc_rx_page_failed;
	u64 alloc_rx_buff_failed;
	u64 csum_err;
};

enum ixgbe_ring_state_t {
	__IXGBE_TX_FDIR_INIT_DONE,
	__IXGBE_TX_DETECT_HANG,
	__IXGBE_HANG_CHECK_ARMED,
	__IXGBE_RX_RSC_ENABLED,
#ifndef HAVE_NDO_SET_FEATURES
	__IXGBE_RX_CSUM_ENABLED,
#endif
	__IXGBE_RX_CSUM_UDP_ZERO_ERR,
#ifdef IXGBE_FCOE
	__IXGBE_RX_FCOE_BUFSZ,
#endif
};

#define check_for_tx_hang(ring) \
	test_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define set_check_for_tx_hang(ring) \
	set_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define clear_check_for_tx_hang(ring) \
	clear_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#ifndef IXGBE_NO_HW_RSC
#define ring_is_rsc_enabled(ring) \
	test_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#else
#define ring_is_rsc_enabled(ring)	false
#endif
#define set_ring_rsc_enabled(ring) \
	set_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#define clear_ring_rsc_enabled(ring) \
	clear_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#define netdev_ring(ring) (ring->netdev)
#define ring_queue_index(ring) (ring->queue_index)


struct ixgbe_ring {
	struct ixgbe_ring *next;	/* pointer to next ring in q_vector */
	struct ixgbe_q_vector *q_vector; /* backpointer to host q_vector */
	struct net_device *netdev;	/* netdev ring belongs to */
	struct device *dev;		/* device for DMA mapping */
	void *desc;			/* descriptor ring memory */
	union {
		struct ixgbe_tx_buffer *tx_buffer_info;
		struct ixgbe_rx_buffer *rx_buffer_info;
	};
	unsigned long state;
	u8 __iomem *tail;
	dma_addr_t dma;			/* phys. address of descriptor ring */
	unsigned int size;		/* length in bytes */

	u16 count;			/* amount of descriptors */

	u8 queue_index; /* needed for multiqueue queue management */
	u8 reg_idx;			/* holds the special value that gets
					 * the hardware register offset
					 * associated with this ring, which is
					 * different for DCB and RSS modes
					 */
	u16 next_to_use;
	u16 next_to_clean;

	union {
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
		u16 rx_buf_len;
#else
		u16 next_to_alloc;
#endif
		struct {
			u8 atr_sample_rate;
			u8 atr_count;
		};
	};

	u8 dcb_tc;
	struct ixgbe_queue_stats stats;
	union {
		struct ixgbe_tx_queue_stats tx_stats;
		struct ixgbe_rx_queue_stats rx_stats;
	};
} ____cacheline_internodealigned_in_smp;

enum ixgbe_ring_f_enum {
	RING_F_NONE = 0,
	RING_F_VMDQ,  /* SR-IOV uses the same ring feature */
	RING_F_RSS,
	RING_F_FDIR,
#ifdef IXGBE_FCOE
	RING_F_FCOE,
#endif /* IXGBE_FCOE */
	RING_F_ARRAY_SIZE  /* must be last in enum set */
};

#define IXGBE_MAX_DCB_INDICES	8
#define IXGBE_MAX_RSS_INDICES	16
#define IXGBE_MAX_VMDQ_INDICES	64
#define IXGBE_MAX_FDIR_INDICES	64
#ifdef IXGBE_FCOE
#define IXGBE_MAX_FCOE_INDICES	8
#define MAX_RX_QUEUES	(IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#define MAX_TX_QUEUES	(IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#else
#define MAX_RX_QUEUES	IXGBE_MAX_FDIR_INDICES
#define MAX_TX_QUEUES	IXGBE_MAX_FDIR_INDICES
#endif /* IXGBE_FCOE */
struct ixgbe_ring_feature {
	int indices;
	int mask;
};

#ifndef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
/*
 * FCoE requires that all Rx buffers be over 2200 bytes in length.  Since
 * this is twice the size of a half page we need to double the page order
 * for FCoE enabled Rx queues.
 */
#if defined(IXGBE_FCOE) && (PAGE_SIZE < 8192)
static inline unsigned int ixgbe_rx_pg_order(struct ixgbe_ring *ring)
{
	return test_bit(__IXGBE_RX_FCOE_BUFSZ, &ring->state) ? 1 : 0;
}
#else
#define ixgbe_rx_pg_order(_ring) 0
#endif
#define ixgbe_rx_pg_size(_ring) (PAGE_SIZE << ixgbe_rx_pg_order(_ring))
#define ixgbe_rx_bufsz(_ring) ((PAGE_SIZE / 2) << ixgbe_rx_pg_order(_ring))

#endif
struct ixgbe_ring_container {
	struct ixgbe_ring *ring;	/* pointer to linked list of rings */
	unsigned int total_bytes;	/* total bytes processed this int */
	unsigned int total_packets;	/* total packets processed this int */
	u16 work_limit;			/* total work allowed per interrupt */
	u8 count;			/* total number of rings in vector */
	u8 itr;				/* current ITR setting for ring */
};

/* iterator for handling rings in ring container */
#define ixgbe_for_each_ring(pos, head) \
	for (pos = (head).ring; pos != NULL; pos = pos->next)

#define MAX_RX_PACKET_BUFFERS	((adapter->flags & IXGBE_FLAG_DCB_ENABLED) \
				 ? 8 : 1)
#define MAX_TX_PACKET_BUFFERS	MAX_RX_PACKET_BUFFERS

/* MAX_MSIX_Q_VECTORS of these are allocated,
 * but we only use one per queue-specific vector.
 */
struct ixgbe_q_vector {
	struct ixgbe_adapter *adapter;
	int cpu;	/* CPU for DCA */
	u16 v_idx;	/* index of q_vector within array, also used for
			 * finding the bit in EICR and friends that
			 * represents the vector for this ring */
	u16 itr;	/* Interrupt throttle rate written to EITR */
	struct ixgbe_ring_container rx, tx;

#ifdef CONFIG_IXGBE_NAPI
	struct napi_struct napi;
#endif
#ifndef HAVE_NETDEV_NAPI_LIST
	struct net_device poll_dev;
#endif
#ifdef HAVE_IRQ_AFFINITY_HINT
	cpumask_t affinity_mask;
#endif
#ifndef IXGBE_NO_LRO
	struct ixgbe_lro_list lrolist;   /* LRO list for queue vector*/
#endif
	int numa_node;
	char name[IFNAMSIZ + 9];

	/* for dynamic allocation of rings associated with this q_vector */
	struct ixgbe_ring ring[0] ____cacheline_internodealigned_in_smp;
};

/*
 * microsecond values for various ITR rates shifted by 2 to fit itr register
 * with the first 3 bits reserved 0
 */
#define IXGBE_MIN_RSC_ITR	24
#define IXGBE_100K_ITR		40
#define IXGBE_20K_ITR		200
#define IXGBE_16K_ITR		248
#define IXGBE_10K_ITR		400
#define IXGBE_8K_ITR		500

/* ixgbe_test_staterr - tests bits in Rx descriptor status and error fields */
static inline __le32 ixgbe_test_staterr(union ixgbe_adv_rx_desc *rx_desc,
					const u32 stat_err_bits)
{
	return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}

/* ixgbe_desc_unused - calculate if we have unused descriptors */
static inline u16 ixgbe_desc_unused(struct ixgbe_ring *ring)
{
	u16 ntc = ring->next_to_clean;
	u16 ntu = ring->next_to_use;

	return ((ntc > ntu) ? 0 : ring->count) + ntc - ntu - 1;
}

#define IXGBE_RX_DESC(R, i)	\
	(&(((union ixgbe_adv_rx_desc *)((R)->desc))[i]))
#define IXGBE_TX_DESC(R, i)	\
	(&(((union ixgbe_adv_tx_desc *)((R)->desc))[i]))
#define IXGBE_TX_CTXTDESC(R, i)	\
	(&(((struct ixgbe_adv_tx_context_desc *)((R)->desc))[i]))

#define IXGBE_MAX_JUMBO_FRAME_SIZE	16128
#ifdef IXGBE_FCOE
/* use 3K as the baby jumbo frame size for FCoE */
#define IXGBE_FCOE_JUMBO_FRAME_SIZE	3072
#endif /* IXGBE_FCOE */

#define TCP_TIMER_VECTOR	0
#define OTHER_VECTOR	1
#define NON_Q_VECTORS	(OTHER_VECTOR + TCP_TIMER_VECTOR)

#define IXGBE_MAX_MSIX_Q_VECTORS_82599	64
#define IXGBE_MAX_MSIX_Q_VECTORS_82598	16

struct ixgbe_mac_addr {
	u8 addr[ETH_ALEN];
	u16 queue;
	u16 state; /* bitmask */
};
#define IXGBE_MAC_STATE_DEFAULT		0x1
#define IXGBE_MAC_STATE_MODIFIED	0x2
#define IXGBE_MAC_STATE_IN_USE		0x4

#ifdef IXGBE_PROCFS
struct ixgbe_therm_proc_data {
	struct ixgbe_hw *hw;
	struct ixgbe_thermal_diode_data *sensor_data;
};

#endif /* IXGBE_PROCFS */

/*
 * Only for array allocations in our adapter struct.  On 82598, there will be
 * unused entries in the array, but that's not a big deal.  Also, in 82599,
 * we can actually assign 64 queue vectors based on our extended-extended
 * interrupt registers.  This is different than 82598, which is limited to 16.
 */
#define MAX_MSIX_Q_VECTORS	IXGBE_MAX_MSIX_Q_VECTORS_82599
#define MAX_MSIX_COUNT		IXGBE_MAX_MSIX_VECTORS_82599

#define MIN_MSIX_Q_VECTORS	1
#define MIN_MSIX_COUNT		(MIN_MSIX_Q_VECTORS + NON_Q_VECTORS)

/* default to trying for four seconds */
#define IXGBE_TRY_LINK_TIMEOUT	(4 * HZ)

/* board specific private data structure */
struct ixgbe_adapter {
#ifdef NETIF_F_HW_VLAN_TX
#ifdef HAVE_VLAN_RX_REGISTER
	struct vlan_group *vlgrp; /* must be first, see ixgbe_receive_skb */
#else
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif
#endif /* NETIF_F_HW_VLAN_TX */
	/* OS defined structs */
	struct net_device *netdev;
	struct pci_dev *pdev;

	unsigned long state;

	/* Some features need tri-state capability,
	 * thus the additional *_CAPABLE flags.
	 */
	u32 flags;
#define IXGBE_FLAG_MSI_CAPABLE			(u32)(1 << 0)
#define IXGBE_FLAG_MSI_ENABLED			(u32)(1 << 1)
#define IXGBE_FLAG_MSIX_CAPABLE			(u32)(1 << 2)
#define IXGBE_FLAG_MSIX_ENABLED			(u32)(1 << 3)
#ifndef IXGBE_NO_LLI
#define IXGBE_FLAG_LLI_PUSH			(u32)(1 << 4)
#endif
#define IXGBE_FLAG_IN_NETPOLL                   (u32)(1 << 8)
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
#define IXGBE_FLAG_DCA_ENABLED			(u32)(1 << 9)
#define IXGBE_FLAG_DCA_CAPABLE			(u32)(1 << 10)
#define IXGBE_FLAG_DCA_ENABLED_DATA		(u32)(1 << 11)
#else
#define IXGBE_FLAG_DCA_ENABLED			(u32)0
#define IXGBE_FLAG_DCA_CAPABLE			(u32)0
#define IXGBE_FLAG_DCA_ENABLED_DATA             (u32)0
#endif
#define IXGBE_FLAG_MQ_CAPABLE			(u32)(1 << 12)
#define IXGBE_FLAG_DCB_ENABLED			(u32)(1 << 13)
#define IXGBE_FLAG_DCB_CAPABLE			(u32)(1 << 14)
#define IXGBE_FLAG_RSS_ENABLED			(u32)(1 << 15)
#define IXGBE_FLAG_RSS_CAPABLE			(u32)(1 << 16)
#define IXGBE_FLAG_VMDQ_ENABLED			(u32)(1 << 18)
#define IXGBE_FLAG_FAN_FAIL_CAPABLE		(u32)(1 << 19)
#define IXGBE_FLAG_NEED_LINK_UPDATE		(u32)(1 << 20)
#define IXGBE_FLAG_NEED_LINK_CONFIG		(u32)(1 << 21)
#define IXGBE_FLAG_FDIR_HASH_CAPABLE		(u32)(1 << 22)
#define IXGBE_FLAG_FDIR_PERFECT_CAPABLE		(u32)(1 << 23)
#ifdef IXGBE_FCOE
#define IXGBE_FLAG_FCOE_CAPABLE			(u32)(1 << 24)
#define IXGBE_FLAG_FCOE_ENABLED			(u32)(1 << 25)
#endif /* IXGBE_FCOE */
#define IXGBE_FLAG_SRIOV_CAPABLE		(u32)(1 << 26)
#define IXGBE_FLAG_SRIOV_ENABLED		(u32)(1 << 27)
#define IXGBE_FLAG_SRIOV_REPLICATION_ENABLE	(u32)(1 << 28)
#define IXGBE_FLAG_SRIOV_L2SWITCH_ENABLE	(u32)(1 << 29)
#define IXGBE_FLAG_SRIOV_L2LOOPBACK_ENABLE	(u32)(1 << 30)
#define IXGBE_FLAG_RX_BB_CAPABLE		(u32)(1 << 31)

	u32 flags2;
#ifndef IXGBE_NO_HW_RSC
#define IXGBE_FLAG2_RSC_CAPABLE			(u32)(1)
#define IXGBE_FLAG2_RSC_ENABLED			(u32)(1 << 1)
#else
#define IXGBE_FLAG2_RSC_CAPABLE			0
#define IXGBE_FLAG2_RSC_ENABLED			0
#endif
#define IXGBE_FLAG2_VMDQ_DEFAULT_OVERRIDE	(u32)(1 << 2)
#define IXGBE_FLAG2_TEMP_SENSOR_CAPABLE		(u32)(1 << 4)
#define IXGBE_FLAG2_TEMP_SENSOR_EVENT		(u32)(1 << 5)
#define IXGBE_FLAG2_SEARCH_FOR_SFP		(u32)(1 << 6)
#define IXGBE_FLAG2_SFP_NEEDS_RESET		(u32)(1 << 7)
#define IXGBE_FLAG2_RESET_REQUESTED		(u32)(1 << 8)
#define IXGBE_FLAG2_FDIR_REQUIRES_REINIT	(u32)(1 << 9)
#define IXGBE_FLAG2_RSS_FIELD_IPV4_UDP		(u32)(1 << 10)
#define IXGBE_FLAG2_RSS_FIELD_IPV6_UDP		(u32)(1 << 11)
#define IXGBE_FLAG2_OVERFLOW_CHECK_ENABLED      (u32)(1 << 12)

	/* Tx fast path data */
	int num_tx_queues;
	u16 tx_itr_setting;
	u16 tx_work_limit;

	/* Rx fast path data */
	int num_rx_queues;
	u16 rx_itr_setting;
	u16 rx_work_limit;

	/* TX */
	struct ixgbe_ring *tx_ring[MAX_TX_QUEUES] ____cacheline_aligned_in_smp;

	u64 restart_queue;
	u64 lsc_int;
	u32 tx_timeout_count;

	/* RX */
	struct ixgbe_ring *rx_ring[MAX_RX_QUEUES];
	int num_rx_pools;		/* == num_rx_queues in 82598 */
	int num_rx_queues_per_pool;	/* 1 if 82598, can be many if 82599 */
	u64 hw_csum_rx_error;
	u64 hw_rx_no_dma_resources;
	u64 rsc_total_count;
	u64 rsc_total_flush;
	u64 non_eop_descs;
#ifndef CONFIG_IXGBE_NAPI
	u64 rx_dropped_backlog;		/* count drops from rx intr handler */
#endif
	u32 alloc_rx_page_failed;
	u32 alloc_rx_buff_failed;

	struct ixgbe_q_vector *q_vector[MAX_MSIX_Q_VECTORS];

#ifdef HAVE_DCBNL_IEEE
	struct ieee_pfc *ixgbe_ieee_pfc;
	struct ieee_ets *ixgbe_ieee_ets;
#endif
	struct ixgbe_dcb_config dcb_cfg;
	struct ixgbe_dcb_config temp_dcb_cfg;
	u8 dcb_set_bitmap;
	u8 dcbx_cap;
#ifndef HAVE_MQPRIO
	u8 tc;
#endif
	enum ixgbe_fc_mode last_lfc_mode;

	int num_msix_vectors;
	int max_msix_q_vectors;         /* true count of q_vectors for device */
	struct ixgbe_ring_feature ring_feature[RING_F_ARRAY_SIZE];
	struct msix_entry *msix_entries;

#ifndef HAVE_NETDEV_STATS_IN_NETDEV
	struct net_device_stats net_stats;
#endif
#ifndef IXGBE_NO_LRO
	struct ixgbe_lro_stats lro_stats;
#endif

#ifdef ETHTOOL_TEST
	u32 test_icr;
	struct ixgbe_ring test_tx_ring;
	struct ixgbe_ring test_rx_ring;
#endif

	/* structs defined in ixgbe_hw.h */
	struct ixgbe_hw hw;
	u16 msg_enable;
	struct ixgbe_hw_stats stats;
#ifndef IXGBE_NO_LLI
	u32 lli_port;
	u32 lli_size;
	u32 lli_etype;
	u32 lli_vlan_pri;
#endif /* IXGBE_NO_LLI */

	u32 *config_space;
	u64 tx_busy;
	unsigned int tx_ring_count;
	unsigned int rx_ring_count;

	u32 link_speed;
	bool link_up;
	unsigned long link_check_timeout;

	struct timer_list service_timer;
	struct work_struct service_task;

	struct hlist_head fdir_filter_list;
	unsigned long fdir_overflow; /* number of times ATR was backed off */
	union ixgbe_atr_input fdir_mask;
	int fdir_filter_count;
	u32 fdir_pballoc;
	u32 atr_sample_rate;
	spinlock_t fdir_perfect_lock;

#ifdef IXGBE_FCOE
	struct ixgbe_fcoe fcoe;
#endif /* IXGBE_FCOE */
	u32 wol;

	u16 bd_number;

	char eeprom_id[32];
	u16 eeprom_cap;
	bool netdev_registered;
	u32 interrupt_event;
#ifdef HAVE_ETHTOOL_SET_PHYS_ID
	u32 led_reg;
#endif

	DECLARE_BITMAP(active_vfs, IXGBE_MAX_VF_FUNCTIONS);
	unsigned int num_vfs;
	struct vf_data_storage *vfinfo;
	int vf_rate_link_speed;
	struct vf_macvlans vf_mvs;
	struct vf_macvlans *mv_list;
#ifdef CONFIG_PCI_IOV
	u32 timer_event_accumulator;
	u32 vferr_refcount;
#endif
	struct ixgbe_mac_addr *mac_table;
#ifdef IXGBE_SYSFS
	struct kobject *info_kobj;
	struct kobject *therm_kobj[IXGBE_MAX_SENSORS];
#else /* IXGBE_SYSFS */
#ifdef IXGBE_PROCFS
	struct proc_dir_entry *eth_dir;
	struct proc_dir_entry *info_dir;
	struct proc_dir_entry *therm_dir[IXGBE_MAX_SENSORS];
	struct ixgbe_therm_proc_data therm_data[IXGBE_MAX_SENSORS];
#endif /* IXGBE_PROCFS */
#endif /* IXGBE_SYSFS */
};

struct ixgbe_fdir_filter {
	struct  hlist_node fdir_node;
	union ixgbe_atr_input filter;
	u16 sw_idx;
	u16 action;
};

enum ixgbe_state_t {
	__IXGBE_TESTING,
	__IXGBE_RESETTING,
	__IXGBE_DOWN,
	__IXGBE_SERVICE_SCHED,
	__IXGBE_IN_SFP_INIT,
};

struct ixgbe_cb {
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
	union {				/* Union defining head/tail partner */
		struct sk_buff *head;
		struct sk_buff *tail;
	};
#endif
	dma_addr_t dma;
#ifndef IXGBE_NO_LRO
	__be32	tsecr;			/* timestamp echo response */
	u32	tsval;			/* timestamp value in host order */
	u32	next_seq;		/* next expected sequence number */
	u16	free;			/* 65521 minus total size */
	u16	mss;			/* size of data portion of packet */
#endif /* IXGBE_NO_LRO */
#ifdef HAVE_VLAN_RX_REGISTER
	u16	vid;			/* VLAN tag */
#endif
	u16	append_cnt;		/* number of skb's appended */
#ifndef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
	bool	page_released;
#endif
};
#define IXGBE_CB(skb) ((struct ixgbe_cb *)(skb)->cb)

#ifdef IXGBE_SYSFS
void ixgbe_sysfs_exit(struct ixgbe_adapter *adapter);
int ixgbe_sysfs_init(struct ixgbe_adapter *adapter);
#endif /* IXGBE_SYSFS */
#ifdef IXGBE_PROCFS
void ixgbe_procfs_exit(struct ixgbe_adapter *adapter);
int ixgbe_procfs_init(struct ixgbe_adapter *adapter);
int ixgbe_procfs_topdir_init(void);
void ixgbe_procfs_topdir_exit(void);
#endif /* IXGBE_PROCFS */

extern struct dcbnl_rtnl_ops dcbnl_ops;
extern int ixgbe_copy_dcb_cfg(struct ixgbe_adapter *adapter, int tc_max);

extern u8 ixgbe_dcb_txq_to_tc(struct ixgbe_adapter *adapter, u8 index);

/* needed by ixgbe_main.c */
extern int ixgbe_validate_mac_addr(u8 *mc_addr);
extern void ixgbe_check_options(struct ixgbe_adapter *adapter);
extern void ixgbe_assign_netdev_ops(struct net_device *netdev);

/* needed by ixgbe_ethtool.c */
extern char ixgbe_driver_name[];
extern const char ixgbe_driver_version[];

extern void ixgbe_up(struct ixgbe_adapter *adapter);
extern void ixgbe_down(struct ixgbe_adapter *adapter);
extern void ixgbe_reinit_locked(struct ixgbe_adapter *adapter);
extern void ixgbe_reset(struct ixgbe_adapter *adapter);
extern void ixgbe_set_ethtool_ops(struct net_device *netdev);
extern int ixgbe_setup_rx_resources(struct ixgbe_ring *);
extern int ixgbe_setup_tx_resources(struct ixgbe_ring *);
extern void ixgbe_free_rx_resources(struct ixgbe_ring *);
extern void ixgbe_free_tx_resources(struct ixgbe_ring *);
extern void ixgbe_configure_rx_ring(struct ixgbe_adapter *,
				    struct ixgbe_ring *);
extern void ixgbe_configure_tx_ring(struct ixgbe_adapter *,
				    struct ixgbe_ring *);
extern void ixgbe_update_stats(struct ixgbe_adapter *adapter);
extern int ixgbe_init_interrupt_scheme(struct ixgbe_adapter *adapter);
extern void ixgbe_clear_interrupt_scheme(struct ixgbe_adapter *adapter);
extern bool ixgbe_is_ixgbe(struct pci_dev *pcidev);
extern netdev_tx_t ixgbe_xmit_frame_ring(struct sk_buff *,
					 struct ixgbe_adapter *,
					 struct ixgbe_ring *);
extern void ixgbe_unmap_and_free_tx_resource(struct ixgbe_ring *,
					     struct ixgbe_tx_buffer *);
extern void ixgbe_alloc_rx_buffers(struct ixgbe_ring *, u16);
extern void ixgbe_configure_rscctl(struct ixgbe_adapter *adapter,
				   struct ixgbe_ring *);
extern void ixgbe_clear_rscctl(struct ixgbe_adapter *adapter,
			       struct ixgbe_ring *);
extern void ixgbe_set_rx_mode(struct net_device *netdev);
extern int ixgbe_write_mc_addr_list(struct net_device *netdev);
extern int ixgbe_setup_tc(struct net_device *dev, u8 tc);
#ifdef IXGBE_FCOE
extern void ixgbe_tx_ctxtdesc(struct ixgbe_ring *, u32, u32, u32, u32);
#endif /* IXGBE_FCOE */
extern void ixgbe_do_reset(struct net_device *netdev);
extern void ixgbe_write_eitr(struct ixgbe_q_vector *q_vector);
extern void ixgbe_disable_rx_queue(struct ixgbe_adapter *adapter,
				   struct ixgbe_ring *);
extern void ixgbe_vlan_stripping_enable(struct ixgbe_adapter *adapter);
extern void ixgbe_vlan_stripping_disable(struct ixgbe_adapter *adapter);
#ifdef ETHTOOL_OPS_COMPAT
extern int ethtool_ioctl(struct ifreq *ifr);
#endif

#ifdef IXGBE_FCOE
extern void ixgbe_configure_fcoe(struct ixgbe_adapter *adapter);
extern int ixgbe_fso(struct ixgbe_ring *tx_ring,
		     struct ixgbe_tx_buffer *first,
		     u8 *hdr_len);
extern void ixgbe_cleanup_fcoe(struct ixgbe_adapter *adapter);
extern int ixgbe_fcoe_ddp(struct ixgbe_adapter *adapter,
			  union ixgbe_adv_rx_desc *rx_desc,
			  struct sk_buff *skb);
extern int ixgbe_fcoe_ddp_get(struct net_device *netdev, u16 xid,
			      struct scatterlist *sgl, unsigned int sgc);
#ifdef HAVE_NETDEV_OPS_FCOE_DDP_TARGET
extern int ixgbe_fcoe_ddp_target(struct net_device *netdev, u16 xid,
				 struct scatterlist *sgl, unsigned int sgc);
#endif /* HAVE_NETDEV_OPS_FCOE_DDP_TARGET */
extern int ixgbe_fcoe_ddp_put(struct net_device *netdev, u16 xid);
#ifdef HAVE_NETDEV_OPS_FCOE_ENABLE
extern int ixgbe_fcoe_enable(struct net_device *netdev);
extern int ixgbe_fcoe_disable(struct net_device *netdev);
#endif /* HAVE_NETDEV_OPS_FCOE_ENABLE */
#ifdef CONFIG_DCB
#ifdef HAVE_DCBNL_OPS_GETAPP
extern u8 ixgbe_fcoe_getapp(struct net_device *netdev);
#endif /* HAVE_DCBNL_OPS_GETAPP */
extern u8 ixgbe_fcoe_setapp(struct ixgbe_adapter *adapter, u8 up);
#endif /* CONFIG_DCB */
#ifdef HAVE_NETDEV_OPS_FCOE_GETWWN
extern int ixgbe_fcoe_get_wwn(struct net_device *netdev, u64 *wwn, int type);
#endif
#endif /* IXGBE_FCOE */

#ifdef CONFIG_DCB
#ifdef HAVE_DCBNL_IEEE
s32 ixgbe_dcb_hw_ets(struct ixgbe_hw *hw, struct ieee_ets *ets, int max_frame);
#endif /* HAVE_DCBNL_IEEE */
#endif /* CONFIG_DCB */

extern void ixgbe_clean_rx_ring(struct ixgbe_ring *rx_ring);
extern int ixgbe_get_settings(struct net_device *netdev,
			      struct ethtool_cmd *ecmd);
extern int ixgbe_write_uc_addr_list(struct ixgbe_adapter *adapter,
			    struct net_device *netdev, unsigned int vfn);
extern void ixgbe_full_sync_mac_table(struct ixgbe_adapter *adapter);
extern int ixgbe_add_mac_filter(struct ixgbe_adapter *adapter,
				u8 *addr, u16 queue);
extern int ixgbe_del_mac_filter(struct ixgbe_adapter *adapter,
				u8 *addr, u16 queue);
extern int ixgbe_available_rars(struct ixgbe_adapter *adapter);
#ifndef HAVE_VLAN_RX_REGISTER
extern void ixgbe_vlan_mode(struct net_device *, u32);
#endif
#ifndef ixgbe_get_netdev_tc_txq
#define ixgbe_get_netdev_tc_txq(dev, tc) (&dev->tc_to_txq[tc])
#endif
extern void ixgbe_set_rx_drop_en(struct ixgbe_adapter *adapter);
#endif /* _IXGBE_H_ */
