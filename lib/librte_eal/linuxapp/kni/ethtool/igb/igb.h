/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "LICENSE.GPL".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

/* Linux PRO/1000 Ethernet Driver main header file */

#ifndef _IGB_H_
#define _IGB_H_

#include <linux/kobject.h>

#ifndef IGB_NO_LRO
#include <net/tcp.h>
#endif

#undef HAVE_HW_TIME_STAMP
#ifdef HAVE_HW_TIME_STAMP
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>

#endif
#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif

struct igb_adapter;

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
//#define IGB_DCA
#endif
#ifdef IGB_DCA
#include <linux/dca.h>
#endif

#include "kcompat.h"

#ifdef HAVE_SCTP
#include <linux/sctp.h>
#endif

#include "e1000_api.h"
#include "e1000_82575.h"
#include "e1000_manage.h"
#include "e1000_mbx.h"

#define IGB_ERR(args...) printk(KERN_ERR "igb: " args)

#define PFX "igb: "
#define DPRINTK(nlevel, klevel, fmt, args...) \
	(void)((NETIF_MSG_##nlevel & adapter->msg_enable) && \
	printk(KERN_##klevel PFX "%s: %s: " fmt, adapter->netdev->name, \
		__FUNCTION__ , ## args))

#ifdef HAVE_PTP_1588_CLOCK
#include <linux/clocksource.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#endif /* HAVE_PTP_1588_CLOCK */

#ifdef HAVE_I2C_SUPPORT
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#endif /* HAVE_I2C_SUPPORT */

/* Interrupt defines */
#define IGB_START_ITR                    648 /* ~6000 ints/sec */
#define IGB_4K_ITR                       980
#define IGB_20K_ITR                      196
#define IGB_70K_ITR                       56

/* Interrupt modes, as used by the IntMode parameter */
#define IGB_INT_MODE_LEGACY                0
#define IGB_INT_MODE_MSI                   1
#define IGB_INT_MODE_MSIX                  2

/* TX/RX descriptor defines */
#define IGB_DEFAULT_TXD                  256
#define IGB_DEFAULT_TX_WORK		 128
#define IGB_MIN_TXD                       80
#define IGB_MAX_TXD                     4096

#define IGB_DEFAULT_RXD                  256
#define IGB_MIN_RXD                       80
#define IGB_MAX_RXD                     4096

#define IGB_MIN_ITR_USECS                 10 /* 100k irq/sec */
#define IGB_MAX_ITR_USECS               8191 /* 120  irq/sec */

#define NON_Q_VECTORS                      1
#define MAX_Q_VECTORS                     10

/* Transmit and receive queues */
#define IGB_MAX_RX_QUEUES                 16
#define IGB_MAX_TX_QUEUES                 16

#define IGB_MAX_VF_MC_ENTRIES             30
#define IGB_MAX_VF_FUNCTIONS               8
#define IGB_82576_VF_DEV_ID           0x10CA
#define IGB_I350_VF_DEV_ID            0x1520
#define IGB_MAX_UTA_ENTRIES              128
#define MAX_EMULATION_MAC_ADDRS           16
#define OUI_LEN                            3
#define IGB_MAX_VMDQ_QUEUES                8


struct vf_data_storage {
	unsigned char vf_mac_addresses[ETH_ALEN];
	u16 vf_mc_hashes[IGB_MAX_VF_MC_ENTRIES];
	u16 num_vf_mc_hashes;
	u16 default_vf_vlan_id;
	u16 vlans_enabled;
	unsigned char em_mac_addresses[MAX_EMULATION_MAC_ADDRS * ETH_ALEN];
	u32 uta_table_copy[IGB_MAX_UTA_ENTRIES];
	u32 flags;
	unsigned long last_nack;
#ifdef IFLA_VF_MAX
	u16 pf_vlan; /* When set, guest VLAN config not allowed. */
	u16 pf_qos;
	u16 tx_rate;
#ifdef HAVE_VF_SPOOFCHK_CONFIGURE
	bool spoofchk_enabled;
#endif
#endif
};

#define IGB_VF_FLAG_CTS            0x00000001 /* VF is clear to send data */
#define IGB_VF_FLAG_UNI_PROMISC    0x00000002 /* VF has unicast promisc */
#define IGB_VF_FLAG_MULTI_PROMISC  0x00000004 /* VF has multicast promisc */
#define IGB_VF_FLAG_PF_SET_MAC     0x00000008 /* PF has set MAC address */

/* RX descriptor control thresholds.
 * PTHRESH - MAC will consider prefetch if it has fewer than this number of
 *           descriptors available in its onboard memory.
 *           Setting this to 0 disables RX descriptor prefetch.
 * HTHRESH - MAC will only prefetch if there are at least this many descriptors
 *           available in host memory.
 *           If PTHRESH is 0, this should also be 0.
 * WTHRESH - RX descriptor writeback threshold - MAC will delay writing back
 *           descriptors until either it has this many to write back, or the
 *           ITR timer expires.
 */
#define IGB_RX_PTHRESH	((hw->mac.type == e1000_i354) ? 12 : 8)
#define IGB_RX_HTHRESH	8
#define IGB_TX_PTHRESH	((hw->mac.type == e1000_i354) ? 20 : 8)
#define IGB_TX_HTHRESH	1
#define IGB_RX_WTHRESH	((hw->mac.type == e1000_82576 && \
			  adapter->msix_entries) ? 1 : 4)

/* this is the size past which hardware will drop packets when setting LPE=0 */
#define MAXIMUM_ETHERNET_VLAN_SIZE 1522

/* NOTE: netdev_alloc_skb reserves 16 bytes, NET_IP_ALIGN means we
 * reserve 2 more, and skb_shared_info adds an additional 384 more,
 * this adds roughly 448 bytes of extra data meaning the smallest
 * allocation we could have is 1K.
 * i.e. RXBUFFER_512 --> size-1024 slab
 */
/* Supported Rx Buffer Sizes */
#define IGB_RXBUFFER_256   256
#define IGB_RXBUFFER_2048  2048
#define IGB_RXBUFFER_16384 16384
#define IGB_RX_HDR_LEN	   IGB_RXBUFFER_256
#if MAX_SKB_FRAGS < 8
#define IGB_RX_BUFSZ	   ALIGN(MAX_JUMBO_FRAME_SIZE / MAX_SKB_FRAGS, 1024)
#else
#define IGB_RX_BUFSZ	   IGB_RXBUFFER_2048
#endif


/* Packet Buffer allocations */
#define IGB_PBA_BYTES_SHIFT 0xA
#define IGB_TX_HEAD_ADDR_SHIFT 7
#define IGB_PBA_TX_MASK 0xFFFF0000

#define IGB_FC_PAUSE_TIME 0x0680 /* 858 usec */

/* How many Rx Buffers do we bundle into one write to the hardware ? */
#define IGB_RX_BUFFER_WRITE	16	/* Must be power of 2 */

#define IGB_EEPROM_APME         0x0400
#define AUTO_ALL_MODES          0

#ifndef IGB_MASTER_SLAVE
/* Switch to override PHY master/slave setting */
#define IGB_MASTER_SLAVE	e1000_ms_hw_default
#endif

#define IGB_MNG_VLAN_NONE -1

#ifndef IGB_NO_LRO
#define IGB_LRO_MAX 32 /*Maximum number of LRO descriptors*/
struct igb_lro_stats {
	u32 flushed;
	u32 coal;
};

/*
 * igb_lro_header - header format to be aggregated by LRO
 * @iph: IP header without options
 * @tcp: TCP header
 * @ts:  Optional TCP timestamp data in TCP options
 *
 * This structure relies on the check above that verifies that the header
 * is IPv4 and does not contain any options.
 */
struct igb_lrohdr {
	struct iphdr iph;
	struct tcphdr th;
	__be32 ts[0];
};

struct igb_lro_list {
	struct sk_buff_head active;
	struct igb_lro_stats stats;
};

#endif /* IGB_NO_LRO */
struct igb_cb {
#ifndef IGB_NO_LRO
#ifdef CONFIG_IGB_DISABLE_PACKET_SPLIT
	union {				/* Union defining head/tail partner */
		struct sk_buff *head;
		struct sk_buff *tail;
	};
#endif
	__be32	tsecr;			/* timestamp echo response */
	u32	tsval;			/* timestamp value in host order */
	u32	next_seq;		/* next expected sequence number */
	u16	free;			/* 65521 minus total size */
	u16	mss;			/* size of data portion of packet */
	u16	append_cnt;		/* number of skb's appended */
#endif /* IGB_NO_LRO */
#ifdef HAVE_VLAN_RX_REGISTER
	u16	vid;			/* VLAN tag */
#endif
};
#define IGB_CB(skb) ((struct igb_cb *)(skb)->cb)

enum igb_tx_flags {
	/* cmd_type flags */
	IGB_TX_FLAGS_VLAN	= 0x01,
	IGB_TX_FLAGS_TSO	= 0x02,
	IGB_TX_FLAGS_TSTAMP	= 0x04,

	/* olinfo flags */
	IGB_TX_FLAGS_IPV4	= 0x10,
	IGB_TX_FLAGS_CSUM	= 0x20,
};

/* VLAN info */
#define IGB_TX_FLAGS_VLAN_MASK		0xffff0000
#define IGB_TX_FLAGS_VLAN_SHIFT		        16

/*
 * The largest size we can write to the descriptor is 65535.  In order to
 * maintain a power of two alignment we have to limit ourselves to 32K.
 */
#define IGB_MAX_TXD_PWR		15
#define IGB_MAX_DATA_PER_TXD	(1 << IGB_MAX_TXD_PWR)

/* Tx Descriptors needed, worst case */
#define TXD_USE_COUNT(S)	DIV_ROUND_UP((S), IGB_MAX_DATA_PER_TXD)
#ifndef MAX_SKB_FRAGS
#define DESC_NEEDED	4
#elif (MAX_SKB_FRAGS < 16)
#define DESC_NEEDED	((MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE)) + 4)
#else
#define DESC_NEEDED	(MAX_SKB_FRAGS + 4)
#endif

/* wrapper around a pointer to a socket buffer,
 * so a DMA handle can be stored along with the buffer */
struct igb_tx_buffer {
	union e1000_adv_tx_desc *next_to_watch;
	unsigned long time_stamp;
	struct sk_buff *skb;
	unsigned int bytecount;
	u16 gso_segs;
	__be16 protocol;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};

struct igb_rx_buffer {
	dma_addr_t dma;
#ifdef CONFIG_IGB_DISABLE_PACKET_SPLIT
	struct sk_buff *skb;
#else
	struct page *page;
	u32 page_offset;
#endif
};

struct igb_tx_queue_stats {
	u64 packets;
	u64 bytes;
	u64 restart_queue;
};

struct igb_rx_queue_stats {
	u64 packets;
	u64 bytes;
	u64 drops;
	u64 csum_err;
	u64 alloc_failed;
	u64 ipv4_packets;      /* IPv4 headers processed */
	u64 ipv4e_packets;     /* IPv4E headers with extensions processed */
	u64 ipv6_packets;      /* IPv6 headers processed */
	u64 ipv6e_packets;     /* IPv6E headers with extensions processed */
	u64 tcp_packets;       /* TCP headers processed */
	u64 udp_packets;       /* UDP headers processed */
	u64 sctp_packets;      /* SCTP headers processed */
	u64 nfs_packets;       /* NFS headers processe */
};

struct igb_ring_container {
	struct igb_ring *ring;		/* pointer to linked list of rings */
	unsigned int total_bytes;	/* total bytes processed this int */
	unsigned int total_packets;	/* total packets processed this int */
	u16 work_limit;			/* total work allowed per interrupt */
	u8 count;			/* total number of rings in vector */
	u8 itr;				/* current ITR setting for ring */
};

struct igb_ring {
	struct igb_q_vector *q_vector;  /* backlink to q_vector */
	struct net_device *netdev;      /* back pointer to net_device */
	struct device *dev;             /* device for dma mapping */
	union {				/* array of buffer info structs */
		struct igb_tx_buffer *tx_buffer_info;
		struct igb_rx_buffer *rx_buffer_info;
	};
#ifdef HAVE_PTP_1588_CLOCK
	unsigned long last_rx_timestamp;
#endif /* HAVE_PTP_1588_CLOCK */
	void *desc;                     /* descriptor ring memory */
	unsigned long flags;            /* ring specific flags */
	void __iomem *tail;             /* pointer to ring tail register */
	dma_addr_t dma;			/* phys address of the ring */
	unsigned int size;		/* length of desc. ring in bytes */

	u16 count;                      /* number of desc. in the ring */
	u8 queue_index;                 /* logical index of the ring*/
	u8 reg_idx;                     /* physical index of the ring */

	/* everything past this point are written often */
	u16 next_to_clean;
	u16 next_to_use;
	u16 next_to_alloc;

	union {
		/* TX */
		struct {
			struct igb_tx_queue_stats tx_stats;
		};
		/* RX */
		struct {
			struct igb_rx_queue_stats rx_stats;
#ifdef CONFIG_IGB_DISABLE_PACKET_SPLIT
			u16 rx_buffer_len;
#else
			struct sk_buff *skb;
#endif
		};
	};
#ifdef CONFIG_IGB_VMDQ_NETDEV
	struct net_device *vmdq_netdev;
	int vqueue_index;		/* queue index for virtual netdev */
#endif
} ____cacheline_internodealigned_in_smp;

struct igb_q_vector {
	struct igb_adapter *adapter;	/* backlink */
	int cpu;			/* CPU for DCA */
	u32 eims_value;			/* EIMS mask value */

	u16 itr_val;
	u8 set_itr;
	void __iomem *itr_register;

	struct igb_ring_container rx, tx;

	struct napi_struct napi;
#ifndef IGB_NO_LRO
	struct igb_lro_list lrolist;   /* LRO list for queue vector*/
#endif
	char name[IFNAMSIZ + 9];
#ifndef HAVE_NETDEV_NAPI_LIST
	struct net_device poll_dev;
#endif

	/* for dynamic allocation of rings associated with this q_vector */
	struct igb_ring ring[0] ____cacheline_internodealigned_in_smp;
};

enum e1000_ring_flags_t {
#ifndef HAVE_NDO_SET_FEATURES
	IGB_RING_FLAG_RX_CSUM,
#endif
	IGB_RING_FLAG_RX_SCTP_CSUM,
	IGB_RING_FLAG_RX_LB_VLAN_BSWAP,
	IGB_RING_FLAG_TX_CTX_IDX,
	IGB_RING_FLAG_TX_DETECT_HANG,
};

struct igb_mac_addr {
	u8 addr[ETH_ALEN];
	u16 queue;
	u16 state; /* bitmask */
};
#define IGB_MAC_STATE_DEFAULT	0x1
#define IGB_MAC_STATE_MODIFIED	0x2
#define IGB_MAC_STATE_IN_USE	0x4

#define IGB_TXD_DCMD (E1000_ADVTXD_DCMD_EOP | E1000_ADVTXD_DCMD_RS)

#define IGB_RX_DESC(R, i)	    \
	(&(((union e1000_adv_rx_desc *)((R)->desc))[i]))
#define IGB_TX_DESC(R, i)	    \
	(&(((union e1000_adv_tx_desc *)((R)->desc))[i]))
#define IGB_TX_CTXTDESC(R, i)	    \
	(&(((struct e1000_adv_tx_context_desc *)((R)->desc))[i]))

#ifdef CONFIG_IGB_VMDQ_NETDEV
#define netdev_ring(ring) \
	((ring->vmdq_netdev ? ring->vmdq_netdev : ring->netdev))
#define ring_queue_index(ring) \
	((ring->vmdq_netdev ? ring->vqueue_index : ring->queue_index))
#else
#define netdev_ring(ring) (ring->netdev)
#define ring_queue_index(ring) (ring->queue_index)
#endif /* CONFIG_IGB_VMDQ_NETDEV */

/* igb_test_staterr - tests bits within Rx descriptor status and error fields */
static inline __le32 igb_test_staterr(union e1000_adv_rx_desc *rx_desc,
				      const u32 stat_err_bits)
{
	return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}

/* igb_desc_unused - calculate if we have unused descriptors */
static inline u16 igb_desc_unused(const struct igb_ring *ring)
{
	u16 ntc = ring->next_to_clean;
	u16 ntu = ring->next_to_use;

	return ((ntc > ntu) ? 0 : ring->count) + ntc - ntu - 1;
}

#ifdef CONFIG_BQL
static inline struct netdev_queue *txring_txq(const struct igb_ring *tx_ring)
{
	return netdev_get_tx_queue(tx_ring->netdev, tx_ring->queue_index);
}
#endif /* CONFIG_BQL */

// #ifdef EXT_THERMAL_SENSOR_SUPPORT
// #ifdef IGB_PROCFS
struct igb_therm_proc_data
{
	struct e1000_hw *hw;
	struct e1000_thermal_diode_data *sensor_data;
};

//  #endif /* IGB_PROCFS */
// #endif /* EXT_THERMAL_SENSOR_SUPPORT */

#ifdef IGB_HWMON
#define IGB_HWMON_TYPE_LOC	0
#define IGB_HWMON_TYPE_TEMP	1
#define IGB_HWMON_TYPE_CAUTION	2
#define IGB_HWMON_TYPE_MAX	3

struct hwmon_attr {
	struct device_attribute dev_attr;
	struct e1000_hw *hw;
	struct e1000_thermal_diode_data *sensor;
	char name[12];
	};

struct hwmon_buff {
	struct device *device;
	struct hwmon_attr *hwmon_list;
	unsigned int n_hwmon;
	};
#endif /* IGB_HWMON */

/* board specific private data structure */
struct igb_adapter {
#ifdef HAVE_VLAN_RX_REGISTER
	/* vlgrp must be first member of structure */
	struct vlan_group *vlgrp;
#else
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif
	struct net_device *netdev;

	unsigned long state;
	unsigned int flags;

	unsigned int num_q_vectors;
	struct msix_entry *msix_entries;


	/* TX */
	u16 tx_work_limit;
	u32 tx_timeout_count;
	int num_tx_queues;
	struct igb_ring *tx_ring[IGB_MAX_TX_QUEUES];

	/* RX */
	int num_rx_queues;
	struct igb_ring *rx_ring[IGB_MAX_RX_QUEUES];

	struct timer_list watchdog_timer;
	struct timer_list dma_err_timer;
	struct timer_list phy_info_timer;
	u16 mng_vlan_id;
	u32 bd_number;
	u32 wol;
	u32 en_mng_pt;
	u16 link_speed;
	u16 link_duplex;
	u8 port_num;

	/* Interrupt Throttle Rate */
	u32 rx_itr_setting;
	u32 tx_itr_setting;

	struct work_struct reset_task;
	struct work_struct watchdog_task;
	struct work_struct dma_err_task;
	bool fc_autoneg;
	u8  tx_timeout_factor;

#ifdef DEBUG
	bool tx_hang_detected;
	bool disable_hw_reset;
#endif
	u32 max_frame_size;

	/* OS defined structs */
	struct pci_dev *pdev;
#ifndef HAVE_NETDEV_STATS_IN_NETDEV
	struct net_device_stats net_stats;
#endif
#ifndef IGB_NO_LRO
	struct igb_lro_stats lro_stats;
#endif

	/* structs defined in e1000_hw.h */
	struct e1000_hw hw;
	struct e1000_hw_stats stats;
	struct e1000_phy_info phy_info;
	struct e1000_phy_stats phy_stats;

#ifdef ETHTOOL_TEST
	u32 test_icr;
	struct igb_ring test_tx_ring;
	struct igb_ring test_rx_ring;
#endif

	int msg_enable;

	struct igb_q_vector *q_vector[MAX_Q_VECTORS];
	u32 eims_enable_mask;
	u32 eims_other;

	/* to not mess up cache alignment, always add to the bottom */
	u32 *config_space;
	u16 tx_ring_count;
	u16 rx_ring_count;
	struct vf_data_storage *vf_data;
#ifdef IFLA_VF_MAX
	int vf_rate_link_speed;
#endif
	u32 lli_port;
	u32 lli_size;
	unsigned int vfs_allocated_count;
	/* Malicious Driver Detection flag. Valid only when SR-IOV is enabled */
	bool mdd;
	int int_mode;
	u32 rss_queues;
	u32 vmdq_pools;
	char fw_version[43];
	u32 wvbr;
	struct igb_mac_addr *mac_table;
#ifdef CONFIG_IGB_VMDQ_NETDEV
	struct net_device *vmdq_netdev[IGB_MAX_VMDQ_QUEUES];
#endif
	int vferr_refcount;
	int dmac;
	u32 *shadow_vfta;

	/* External Thermal Sensor support flag */
	bool ets;
#ifdef IGB_HWMON
	struct hwmon_buff igb_hwmon_buff;
#else /* IGB_HWMON */
#ifdef IGB_PROCFS
	struct proc_dir_entry *eth_dir;
	struct proc_dir_entry *info_dir;
	struct proc_dir_entry *therm_dir[E1000_MAX_SENSORS];
	struct igb_therm_proc_data therm_data[E1000_MAX_SENSORS];
	bool old_lsc;
#endif /* IGB_PROCFS */
#endif /* IGB_HWMON */
	u32 etrack_id;

#ifdef HAVE_PTP_1588_CLOCK
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_caps;
	struct delayed_work ptp_overflow_work;
	struct work_struct ptp_tx_work;
	struct sk_buff *ptp_tx_skb;
	unsigned long ptp_tx_start;
	unsigned long last_rx_ptp_check;
	spinlock_t tmreg_lock;
	struct cyclecounter cc;
	struct timecounter tc;
	u32 tx_hwtstamp_timeouts;
	u32 rx_hwtstamp_cleared;
#endif /* HAVE_PTP_1588_CLOCK */

#ifdef HAVE_I2C_SUPPORT
	struct i2c_algo_bit_data i2c_algo;
	struct i2c_adapter i2c_adap;
	struct i2c_client *i2c_client;
#endif /* HAVE_I2C_SUPPORT */
	unsigned long link_check_timeout;


	int devrc;

	int copper_tries;
	u16 eee_advert;
};

#ifdef CONFIG_IGB_VMDQ_NETDEV
struct igb_vmdq_adapter {
#ifdef HAVE_VLAN_RX_REGISTER
	/* vlgrp must be first member of structure */
	struct vlan_group *vlgrp;
#else
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif
	struct igb_adapter *real_adapter;
	struct net_device *vnetdev;
	struct net_device_stats net_stats;
	struct igb_ring *tx_ring;
	struct igb_ring *rx_ring;
};
#endif

#define IGB_FLAG_HAS_MSI		(1 << 0)
#define IGB_FLAG_DCA_ENABLED		(1 << 1)
#define IGB_FLAG_LLI_PUSH		(1 << 2)
#define IGB_FLAG_QUAD_PORT_A		(1 << 3)
#define IGB_FLAG_QUEUE_PAIRS		(1 << 4)
#define IGB_FLAG_EEE			(1 << 5)
#define IGB_FLAG_DMAC			(1 << 6)
#define IGB_FLAG_DETECT_BAD_DMA		(1 << 7)
#define IGB_FLAG_PTP			(1 << 8)
#define IGB_FLAG_RSS_FIELD_IPV4_UDP	(1 << 9)
#define IGB_FLAG_RSS_FIELD_IPV6_UDP	(1 << 10)
#define IGB_FLAG_WOL_SUPPORTED		(1 << 11)
#define IGB_FLAG_NEED_LINK_UPDATE	(1 << 12)
#define IGB_FLAG_LOOPBACK_ENABLE	(1 << 13)
#define IGB_FLAG_MEDIA_RESET		(1 << 14)
#define IGB_FLAG_MAS_ENABLE		(1 << 15)

/* Media Auto Sense */
#define IGB_MAS_ENABLE_0		0X0001
#define IGB_MAS_ENABLE_1		0X0002
#define IGB_MAS_ENABLE_2		0X0004
#define IGB_MAS_ENABLE_3		0X0008

#define IGB_MIN_TXPBSIZE           20408
#define IGB_TX_BUF_4096            4096

#define IGB_DMCTLX_DCFLUSH_DIS     0x80000000  /* Disable DMA Coal Flush */

/* DMA Coalescing defines */
#define IGB_DMAC_DISABLE          0
#define IGB_DMAC_MIN            250
#define IGB_DMAC_500            500
#define IGB_DMAC_EN_DEFAULT    1000
#define IGB_DMAC_2000          2000
#define IGB_DMAC_3000          3000
#define IGB_DMAC_4000          4000
#define IGB_DMAC_5000          5000
#define IGB_DMAC_6000          6000
#define IGB_DMAC_7000          7000
#define IGB_DMAC_8000          8000
#define IGB_DMAC_9000          9000
#define IGB_DMAC_MAX          10000

#define IGB_82576_TSYNC_SHIFT 19
#define IGB_82580_TSYNC_SHIFT 24
#define IGB_TS_HDR_LEN        16

/* CEM Support */
#define FW_HDR_LEN           0x4
#define FW_CMD_DRV_INFO      0xDD
#define FW_CMD_DRV_INFO_LEN  0x5
#define FW_CMD_RESERVED      0X0
#define FW_RESP_SUCCESS      0x1
#define FW_UNUSED_VER        0x0
#define FW_MAX_RETRIES       3
#define FW_STATUS_SUCCESS    0x1
#define FW_FAMILY_DRV_VER    0Xffffffff

#define IGB_MAX_LINK_TRIES   20

struct e1000_fw_hdr {
	u8 cmd;
	u8 buf_len;
	union
	{
		u8 cmd_resv;
		u8 ret_status;
	} cmd_or_resp;
	u8 checksum;
};

#pragma pack(push,1)
struct e1000_fw_drv_info {
	struct e1000_fw_hdr hdr;
	u8 port_num;
	u32 drv_version;
	u16 pad; /* end spacing to ensure length is mult. of dword */
	u8  pad2; /* end spacing to ensure length is mult. of dword2 */
};
#pragma pack(pop)

enum e1000_state_t {
	__IGB_TESTING,
	__IGB_RESETTING,
	__IGB_DOWN
};

extern char igb_driver_name[];
extern char igb_driver_version[];

extern int igb_up(struct igb_adapter *);
extern void igb_down(struct igb_adapter *);
extern void igb_reinit_locked(struct igb_adapter *);
extern void igb_reset(struct igb_adapter *);
extern int igb_set_spd_dplx(struct igb_adapter *, u16);
extern int igb_setup_tx_resources(struct igb_ring *);
extern int igb_setup_rx_resources(struct igb_ring *);
extern void igb_free_tx_resources(struct igb_ring *);
extern void igb_free_rx_resources(struct igb_ring *);
extern void igb_configure_tx_ring(struct igb_adapter *, struct igb_ring *);
extern void igb_configure_rx_ring(struct igb_adapter *, struct igb_ring *);
extern void igb_setup_tctl(struct igb_adapter *);
extern void igb_setup_rctl(struct igb_adapter *);
extern netdev_tx_t igb_xmit_frame_ring(struct sk_buff *, struct igb_ring *);
extern void igb_unmap_and_free_tx_resource(struct igb_ring *,
                                           struct igb_tx_buffer *);
extern void igb_alloc_rx_buffers(struct igb_ring *, u16);
extern void igb_clean_rx_ring(struct igb_ring *);
extern void igb_update_stats(struct igb_adapter *);
extern bool igb_has_link(struct igb_adapter *adapter);
extern void igb_set_ethtool_ops(struct net_device *);
extern void igb_check_options(struct igb_adapter *);
extern void igb_power_up_link(struct igb_adapter *);
#ifdef HAVE_PTP_1588_CLOCK
extern void igb_ptp_init(struct igb_adapter *adapter);
extern void igb_ptp_stop(struct igb_adapter *adapter);
extern void igb_ptp_reset(struct igb_adapter *adapter);
extern void igb_ptp_tx_work(struct work_struct *work);
extern void igb_ptp_rx_hang(struct igb_adapter *adapter);
extern void igb_ptp_tx_hwtstamp(struct igb_adapter *adapter);
extern void igb_ptp_rx_rgtstamp(struct igb_q_vector *q_vector,
				struct sk_buff *skb);
extern void igb_ptp_rx_pktstamp(struct igb_q_vector *q_vector,
				unsigned char *va,
				struct sk_buff *skb);
static inline void igb_ptp_rx_hwtstamp(struct igb_ring *rx_ring,
				       union e1000_adv_rx_desc *rx_desc,
				       struct sk_buff *skb)
{
	if (igb_test_staterr(rx_desc, E1000_RXDADV_STAT_TSIP)) {
#ifdef CONFIG_IGB_DISABLE_PACKET_SPLIT
		igb_ptp_rx_pktstamp(rx_ring->q_vector, skb->data, skb);
		skb_pull(skb, IGB_TS_HDR_LEN);
#endif
		return;
	}

	if (igb_test_staterr(rx_desc, E1000_RXDADV_STAT_TS))
		igb_ptp_rx_rgtstamp(rx_ring->q_vector, skb);

	/* Update the last_rx_timestamp timer in order to enable watchdog check
	 * for error case of latched timestamp on a dropped packet.
	 */
	rx_ring->last_rx_timestamp = jiffies;
}

extern int igb_ptp_hwtstamp_ioctl(struct net_device *netdev,
				  struct ifreq *ifr, int cmd);
#endif /* HAVE_PTP_1588_CLOCK */
#ifdef ETHTOOL_OPS_COMPAT
extern int ethtool_ioctl(struct ifreq *);
#endif
extern int igb_write_mc_addr_list(struct net_device *netdev);
extern int igb_add_mac_filter(struct igb_adapter *adapter, u8 *addr, u16 queue);
extern int igb_del_mac_filter(struct igb_adapter *adapter, u8* addr, u16 queue);
extern int igb_available_rars(struct igb_adapter *adapter);
extern s32 igb_vlvf_set(struct igb_adapter *, u32, bool, u32);
extern void igb_configure_vt_default_pool(struct igb_adapter *adapter);
extern void igb_enable_vlan_tags(struct igb_adapter *adapter);
#ifndef HAVE_VLAN_RX_REGISTER
extern void igb_vlan_mode(struct net_device *, u32);
#endif

#define E1000_PCS_CFG_IGN_SD	1

#ifdef IGB_HWMON
void igb_sysfs_exit(struct igb_adapter *adapter);
int igb_sysfs_init(struct igb_adapter *adapter);
#else
#ifdef IGB_PROCFS
int igb_procfs_init(struct igb_adapter* adapter);
void igb_procfs_exit(struct igb_adapter* adapter);
int igb_procfs_topdir_init(void);
void igb_procfs_topdir_exit(void);
#endif /* IGB_PROCFS */
#endif /* IGB_HWMON */



#endif /* _IGB_H_ */
