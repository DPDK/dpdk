/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/mii.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

/* NAPI enable/disable flags here */
#define NAPI

#define adapter_struct igb_adapter
#define adapter_q_vector igb_q_vector
#define NAPI

/* and finally set defines so that the code sees the changes */
#ifdef NAPI
#else
#endif /* NAPI */

/* packet split disable/enable */
#ifdef DISABLE_PACKET_SPLIT
#ifndef CONFIG_IGB_DISABLE_PACKET_SPLIT
#define CONFIG_IGB_DISABLE_PACKET_SPLIT
#endif
#endif /* DISABLE_PACKET_SPLIT */

/* MSI compatibility code for all kernels and drivers */
#ifdef DISABLE_PCI_MSI
#undef CONFIG_PCI_MSI
#endif
#ifndef CONFIG_PCI_MSI
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8) )
struct msix_entry {
	u16 vector; /* kernel uses to write allocated vector */
	u16 entry;  /* driver uses to specify entry, OS writes */
};
#endif
#undef pci_enable_msi
#define pci_enable_msi(a) -ENOTSUPP
#undef pci_disable_msi
#define pci_disable_msi(a) do {} while (0)
#undef pci_enable_msix
#define pci_enable_msix(a, b, c) -ENOTSUPP
#undef pci_disable_msix
#define pci_disable_msix(a) do {} while (0)
#define msi_remove_pci_irq_vectors(a) do {} while (0)
#endif /* CONFIG_PCI_MSI */
#ifdef DISABLE_PM
#undef CONFIG_PM
#endif

#ifdef DISABLE_NET_POLL_CONTROLLER
#undef CONFIG_NET_POLL_CONTROLLER
#endif

#ifndef PMSG_SUSPEND
#define PMSG_SUSPEND 3
#endif

/* generic boolean compatibility */
#undef TRUE
#undef FALSE
#define TRUE true
#define FALSE false
#ifdef GCC_VERSION
#if ( GCC_VERSION < 3000 )
#define _Bool char
#endif
#else
#define _Bool char
#endif

/* kernels less than 2.4.14 don't have this */
#ifndef ETH_P_8021Q
#define ETH_P_8021Q 0x8100
#endif

#ifndef module_param
#define module_param(v,t,p) MODULE_PARM(v, "i");
#endif

#ifndef DMA_64BIT_MASK
#define DMA_64BIT_MASK  0xffffffffffffffffULL
#endif

#ifndef DMA_32BIT_MASK
#define DMA_32BIT_MASK  0x00000000ffffffffULL
#endif

#ifndef PCI_CAP_ID_EXP
#define PCI_CAP_ID_EXP 0x10
#endif

#ifndef PCIE_LINK_STATE_L0S
#define PCIE_LINK_STATE_L0S 1
#endif
#ifndef PCIE_LINK_STATE_L1
#define PCIE_LINK_STATE_L1 2
#endif

#ifndef mmiowb
#ifdef CONFIG_IA64
#define mmiowb() asm volatile ("mf.a" ::: "memory")
#else
#define mmiowb()
#endif
#endif

#ifndef SET_NETDEV_DEV
#define SET_NETDEV_DEV(net, pdev)
#endif

#if !defined(HAVE_FREE_NETDEV) && ( LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0) )
#define free_netdev(x)	kfree(x)
#endif

#ifdef HAVE_POLL_CONTROLLER
#define CONFIG_NET_POLL_CONTROLLER
#endif

#ifndef SKB_DATAREF_SHIFT
/* if we do not have the infrastructure to detect if skb_header is cloned
   just return false in all cases */
#define skb_header_cloned(x) 0
#endif

#ifndef NETIF_F_GSO
#define gso_size tso_size
#define gso_segs tso_segs
#endif

#ifndef NETIF_F_GRO
#define vlan_gro_receive(_napi, _vlgrp, _vlan, _skb) \
		vlan_hwaccel_receive_skb(_skb, _vlgrp, _vlan)
#define napi_gro_receive(_napi, _skb) netif_receive_skb(_skb)
#endif

#ifndef NETIF_F_SCTP_CSUM
#define NETIF_F_SCTP_CSUM 0
#endif

#ifndef NETIF_F_LRO
#define NETIF_F_LRO (1 << 15)
#endif

#ifndef NETIF_F_NTUPLE
#define NETIF_F_NTUPLE (1 << 27)
#endif

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifndef CHECKSUM_PARTIAL
#define CHECKSUM_PARTIAL CHECKSUM_HW
#define CHECKSUM_COMPLETE CHECKSUM_HW
#endif

#ifndef __read_mostly
#define __read_mostly
#endif

#ifndef MII_RESV1
#define MII_RESV1		0x17		/* Reserved...		*/
#endif

#ifndef unlikely
#define unlikely(_x) _x
#define likely(_x) _x
#endif

#ifndef WARN_ON
#define WARN_ON(x)
#endif

#ifndef PCI_DEVICE
#define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#endif

#ifndef node_online
#define node_online(node) ((node) == 0)
#endif

#ifndef num_online_cpus
#define num_online_cpus() smp_num_cpus
#endif

#ifndef cpu_online
#define cpu_online(cpuid) test_bit((cpuid), &cpu_online_map)
#endif

#ifndef _LINUX_RANDOM_H
#include <linux/random.h>
#endif

#ifndef DECLARE_BITMAP
#ifndef BITS_TO_LONGS
#define BITS_TO_LONGS(bits) (((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#endif
#define DECLARE_BITMAP(name,bits) long name[BITS_TO_LONGS(bits)]
#endif

#ifndef VLAN_HLEN
#define VLAN_HLEN 4
#endif

#ifndef VLAN_ETH_HLEN
#define VLAN_ETH_HLEN 18
#endif

#ifndef VLAN_ETH_FRAME_LEN
#define VLAN_ETH_FRAME_LEN 1518
#endif

#if !defined(IXGBE_DCA) && !defined(IGB_DCA)
#define dca_get_tag(b) 0
#define dca_add_requester(a) -1
#define dca_remove_requester(b) do { } while(0)
#define DCA_PROVIDER_ADD     0x0001
#define DCA_PROVIDER_REMOVE  0x0002
#endif

#ifndef DCA_GET_TAG_TWO_ARGS
#define dca3_get_tag(a,b) dca_get_tag(b)
#endif

#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#if defined(__i386__) || defined(__x86_64__)
#define CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#endif
#endif

/* taken from 2.6.24 definition in linux/kernel.h */
#ifndef IS_ALIGNED
#define IS_ALIGNED(x,a)         (((x) % ((typeof(x))(a))) == 0)
#endif

#ifdef IS_ENABLED
#undef IS_ENABLED
#undef __ARG_PLACEHOLDER_1
#undef config_enabled
#undef _config_enabled
#undef __config_enabled
#undef ___config_enabled
#endif

#define __ARG_PLACEHOLDER_1 0,
#define config_enabled(cfg) _config_enabled(cfg)
#define _config_enabled(value) __config_enabled(__ARG_PLACEHOLDER_##value)
#define __config_enabled(arg1_or_junk) ___config_enabled(arg1_or_junk 1, 0)
#define ___config_enabled(__ignored, val, ...) val

#define IS_ENABLED(option) \
	(config_enabled(option) || config_enabled(option##_MODULE))

#if !defined(NETIF_F_HW_VLAN_TX) && !defined(NETIF_F_HW_VLAN_CTAG_TX)
struct _kc_vlan_ethhdr {
	unsigned char	h_dest[ETH_ALEN];
	unsigned char	h_source[ETH_ALEN];
	__be16		h_vlan_proto;
	__be16		h_vlan_TCI;
	__be16		h_vlan_encapsulated_proto;
};
#define vlan_ethhdr _kc_vlan_ethhdr
struct _kc_vlan_hdr {
	__be16		h_vlan_TCI;
	__be16		h_vlan_encapsulated_proto;
};
#define vlan_hdr _kc_vlan_hdr
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) )
#define vlan_tx_tag_present(_skb) 0
#define vlan_tx_tag_get(_skb) 0
#endif
#endif /* NETIF_F_HW_VLAN_TX && NETIF_F_HW_VLAN_CTAG_TX */

#ifndef VLAN_PRIO_SHIFT
#define VLAN_PRIO_SHIFT 13
#endif


#ifndef __GFP_COLD
#define __GFP_COLD 0
#endif

#ifndef __GFP_COMP
#define __GFP_COMP 0
#endif

/*****************************************************************************/
/* Installations with ethtool version without eeprom, adapter id, or statistics
 * support */

#ifndef ETH_GSTRING_LEN
#define ETH_GSTRING_LEN 32
#endif

#ifndef ETHTOOL_GSTATS
#define ETHTOOL_GSTATS 0x1d
#undef ethtool_drvinfo
#define ethtool_drvinfo k_ethtool_drvinfo
struct k_ethtool_drvinfo {
	u32 cmd;
	char driver[32];
	char version[32];
	char fw_version[32];
	char bus_info[32];
	char reserved1[32];
	char reserved2[16];
	u32 n_stats;
	u32 testinfo_len;
	u32 eedump_len;
	u32 regdump_len;
};

struct ethtool_stats {
	u32 cmd;
	u32 n_stats;
	u64 data[0];
};
#endif /* ETHTOOL_GSTATS */

#ifndef ETHTOOL_PHYS_ID
#define ETHTOOL_PHYS_ID 0x1c
#endif /* ETHTOOL_PHYS_ID */

#ifndef ETHTOOL_GSTRINGS
#define ETHTOOL_GSTRINGS 0x1b
enum ethtool_stringset {
	ETH_SS_TEST             = 0,
	ETH_SS_STATS,
};
struct ethtool_gstrings {
	u32 cmd;            /* ETHTOOL_GSTRINGS */
	u32 string_set;     /* string set id e.c. ETH_SS_TEST, etc*/
	u32 len;            /* number of strings in the string set */
	u8 data[0];
};
#endif /* ETHTOOL_GSTRINGS */

#ifndef ETHTOOL_TEST
#define ETHTOOL_TEST 0x1a
enum ethtool_test_flags {
	ETH_TEST_FL_OFFLINE	= (1 << 0),
	ETH_TEST_FL_FAILED	= (1 << 1),
};
struct ethtool_test {
	u32 cmd;
	u32 flags;
	u32 reserved;
	u32 len;
	u64 data[0];
};
#endif /* ETHTOOL_TEST */

#ifndef ETHTOOL_GEEPROM
#define ETHTOOL_GEEPROM 0xb
#undef ETHTOOL_GREGS
struct ethtool_eeprom {
	u32 cmd;
	u32 magic;
	u32 offset;
	u32 len;
	u8 data[0];
};

struct ethtool_value {
	u32 cmd;
	u32 data;
};
#endif /* ETHTOOL_GEEPROM */

#ifndef ETHTOOL_GLINK
#define ETHTOOL_GLINK 0xa
#endif /* ETHTOOL_GLINK */

#ifndef ETHTOOL_GWOL
#define ETHTOOL_GWOL 0x5
#define ETHTOOL_SWOL 0x6
#define SOPASS_MAX      6
struct ethtool_wolinfo {
	u32 cmd;
	u32 supported;
	u32 wolopts;
	u8 sopass[SOPASS_MAX]; /* SecureOn(tm) password */
};
#endif /* ETHTOOL_GWOL */

#ifndef ETHTOOL_GREGS
#define ETHTOOL_GREGS		0x00000004 /* Get NIC registers */
#define ethtool_regs _kc_ethtool_regs
/* for passing big chunks of data */
struct _kc_ethtool_regs {
	u32 cmd;
	u32 version; /* driver-specific, indicates different chips/revs */
	u32 len; /* bytes */
	u8 data[0];
};
#endif /* ETHTOOL_GREGS */

#ifndef ETHTOOL_GMSGLVL
#define ETHTOOL_GMSGLVL		0x00000007 /* Get driver message level */
#endif
#ifndef ETHTOOL_SMSGLVL
#define ETHTOOL_SMSGLVL		0x00000008 /* Set driver msg level, priv. */
#endif
#ifndef ETHTOOL_NWAY_RST
#define ETHTOOL_NWAY_RST	0x00000009 /* Restart autonegotiation, priv */
#endif
#ifndef ETHTOOL_GLINK
#define ETHTOOL_GLINK		0x0000000a /* Get link status */
#endif
#ifndef ETHTOOL_GEEPROM
#define ETHTOOL_GEEPROM		0x0000000b /* Get EEPROM data */
#endif
#ifndef ETHTOOL_SEEPROM
#define ETHTOOL_SEEPROM		0x0000000c /* Set EEPROM data */
#endif
#ifndef ETHTOOL_GCOALESCE
#define ETHTOOL_GCOALESCE	0x0000000e /* Get coalesce config */
/* for configuring coalescing parameters of chip */
#define ethtool_coalesce _kc_ethtool_coalesce
struct _kc_ethtool_coalesce {
	u32	cmd;	/* ETHTOOL_{G,S}COALESCE */

	/* How many usecs to delay an RX interrupt after
	 * a packet arrives.  If 0, only rx_max_coalesced_frames
	 * is used.
	 */
	u32	rx_coalesce_usecs;

	/* How many packets to delay an RX interrupt after
	 * a packet arrives.  If 0, only rx_coalesce_usecs is
	 * used.  It is illegal to set both usecs and max frames
	 * to zero as this would cause RX interrupts to never be
	 * generated.
	 */
	u32	rx_max_coalesced_frames;

	/* Same as above two parameters, except that these values
	 * apply while an IRQ is being serviced by the host.  Not
	 * all cards support this feature and the values are ignored
	 * in that case.
	 */
	u32	rx_coalesce_usecs_irq;
	u32	rx_max_coalesced_frames_irq;

	/* How many usecs to delay a TX interrupt after
	 * a packet is sent.  If 0, only tx_max_coalesced_frames
	 * is used.
	 */
	u32	tx_coalesce_usecs;

	/* How many packets to delay a TX interrupt after
	 * a packet is sent.  If 0, only tx_coalesce_usecs is
	 * used.  It is illegal to set both usecs and max frames
	 * to zero as this would cause TX interrupts to never be
	 * generated.
	 */
	u32	tx_max_coalesced_frames;

	/* Same as above two parameters, except that these values
	 * apply while an IRQ is being serviced by the host.  Not
	 * all cards support this feature and the values are ignored
	 * in that case.
	 */
	u32	tx_coalesce_usecs_irq;
	u32	tx_max_coalesced_frames_irq;

	/* How many usecs to delay in-memory statistics
	 * block updates.  Some drivers do not have an in-memory
	 * statistic block, and in such cases this value is ignored.
	 * This value must not be zero.
	 */
	u32	stats_block_coalesce_usecs;

	/* Adaptive RX/TX coalescing is an algorithm implemented by
	 * some drivers to improve latency under low packet rates and
	 * improve throughput under high packet rates.  Some drivers
	 * only implement one of RX or TX adaptive coalescing.  Anything
	 * not implemented by the driver causes these values to be
	 * silently ignored.
	 */
	u32	use_adaptive_rx_coalesce;
	u32	use_adaptive_tx_coalesce;

	/* When the packet rate (measured in packets per second)
	 * is below pkt_rate_low, the {rx,tx}_*_low parameters are
	 * used.
	 */
	u32	pkt_rate_low;
	u32	rx_coalesce_usecs_low;
	u32	rx_max_coalesced_frames_low;
	u32	tx_coalesce_usecs_low;
	u32	tx_max_coalesced_frames_low;

	/* When the packet rate is below pkt_rate_high but above
	 * pkt_rate_low (both measured in packets per second) the
	 * normal {rx,tx}_* coalescing parameters are used.
	 */

	/* When the packet rate is (measured in packets per second)
	 * is above pkt_rate_high, the {rx,tx}_*_high parameters are
	 * used.
	 */
	u32	pkt_rate_high;
	u32	rx_coalesce_usecs_high;
	u32	rx_max_coalesced_frames_high;
	u32	tx_coalesce_usecs_high;
	u32	tx_max_coalesced_frames_high;

	/* How often to do adaptive coalescing packet rate sampling,
	 * measured in seconds.  Must not be zero.
	 */
	u32	rate_sample_interval;
};
#endif /* ETHTOOL_GCOALESCE */

#ifndef ETHTOOL_SCOALESCE
#define ETHTOOL_SCOALESCE	0x0000000f /* Set coalesce config. */
#endif
#ifndef ETHTOOL_GRINGPARAM
#define ETHTOOL_GRINGPARAM	0x00000010 /* Get ring parameters */
/* for configuring RX/TX ring parameters */
#define ethtool_ringparam _kc_ethtool_ringparam
struct _kc_ethtool_ringparam {
	u32	cmd;	/* ETHTOOL_{G,S}RINGPARAM */

	/* Read only attributes.  These indicate the maximum number
	 * of pending RX/TX ring entries the driver will allow the
	 * user to set.
	 */
	u32	rx_max_pending;
	u32	rx_mini_max_pending;
	u32	rx_jumbo_max_pending;
	u32	tx_max_pending;

	/* Values changeable by the user.  The valid values are
	 * in the range 1 to the "*_max_pending" counterpart above.
	 */
	u32	rx_pending;
	u32	rx_mini_pending;
	u32	rx_jumbo_pending;
	u32	tx_pending;
};
#endif /* ETHTOOL_GRINGPARAM */

#ifndef ETHTOOL_SRINGPARAM
#define ETHTOOL_SRINGPARAM	0x00000011 /* Set ring parameters, priv. */
#endif
#ifndef ETHTOOL_GPAUSEPARAM
#define ETHTOOL_GPAUSEPARAM	0x00000012 /* Get pause parameters */
/* for configuring link flow control parameters */
#define ethtool_pauseparam _kc_ethtool_pauseparam
struct _kc_ethtool_pauseparam {
	u32	cmd;	/* ETHTOOL_{G,S}PAUSEPARAM */

	/* If the link is being auto-negotiated (via ethtool_cmd.autoneg
	 * being true) the user may set 'autoneg' here non-zero to have the
	 * pause parameters be auto-negotiated too.  In such a case, the
	 * {rx,tx}_pause values below determine what capabilities are
	 * advertised.
	 *
	 * If 'autoneg' is zero or the link is not being auto-negotiated,
	 * then {rx,tx}_pause force the driver to use/not-use pause
	 * flow control.
	 */
	u32	autoneg;
	u32	rx_pause;
	u32	tx_pause;
};
#endif /* ETHTOOL_GPAUSEPARAM */

#ifndef ETHTOOL_SPAUSEPARAM
#define ETHTOOL_SPAUSEPARAM	0x00000013 /* Set pause parameters. */
#endif
#ifndef ETHTOOL_GRXCSUM
#define ETHTOOL_GRXCSUM		0x00000014 /* Get RX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_SRXCSUM
#define ETHTOOL_SRXCSUM		0x00000015 /* Set RX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_GTXCSUM
#define ETHTOOL_GTXCSUM		0x00000016 /* Get TX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_STXCSUM
#define ETHTOOL_STXCSUM		0x00000017 /* Set TX hw csum enable (ethtool_value) */
#endif
#ifndef ETHTOOL_GSG
#define ETHTOOL_GSG		0x00000018 /* Get scatter-gather enable
					    * (ethtool_value) */
#endif
#ifndef ETHTOOL_SSG
#define ETHTOOL_SSG		0x00000019 /* Set scatter-gather enable
					    * (ethtool_value). */
#endif
#ifndef ETHTOOL_TEST
#define ETHTOOL_TEST		0x0000001a /* execute NIC self-test, priv. */
#endif
#ifndef ETHTOOL_GSTRINGS
#define ETHTOOL_GSTRINGS	0x0000001b /* get specified string set */
#endif
#ifndef ETHTOOL_PHYS_ID
#define ETHTOOL_PHYS_ID		0x0000001c /* identify the NIC */
#endif
#ifndef ETHTOOL_GSTATS
#define ETHTOOL_GSTATS		0x0000001d /* get NIC-specific statistics */
#endif
#ifndef ETHTOOL_GTSO
#define ETHTOOL_GTSO		0x0000001e /* Get TSO enable (ethtool_value) */
#endif
#ifndef ETHTOOL_STSO
#define ETHTOOL_STSO		0x0000001f /* Set TSO enable (ethtool_value) */
#endif

#ifndef ETHTOOL_BUSINFO_LEN
#define ETHTOOL_BUSINFO_LEN	32
#endif

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a,b) (((a) << 8) + (b))
#endif
#ifndef AX_RELEASE_VERSION
#define AX_RELEASE_VERSION(a,b) (((a) << 8) + (b))
#endif

#ifndef AX_RELEASE_CODE
#define AX_RELEASE_CODE 0
#endif

#if (AX_RELEASE_CODE && AX_RELEASE_CODE == AX_RELEASE_VERSION(3,0))
#define RHEL_RELEASE_CODE RHEL_RELEASE_VERSION(5,0)
#elif (AX_RELEASE_CODE && AX_RELEASE_CODE == AX_RELEASE_VERSION(3,1))
#define RHEL_RELEASE_CODE RHEL_RELEASE_VERSION(5,1)
#elif (AX_RELEASE_CODE && AX_RELEASE_CODE == AX_RELEASE_VERSION(3,2))
#define RHEL_RELEASE_CODE RHEL_RELEASE_VERSION(5,3)
#endif

#ifndef RHEL_RELEASE_CODE
/* NOTE: RHEL_RELEASE_* introduced in RHEL4.5 */
#define RHEL_RELEASE_CODE 0
#endif

/* SuSE version macro is the same as Linux kernel version */
#ifndef SLE_VERSION
#define SLE_VERSION(a,b,c) KERNEL_VERSION(a,b,c)
#endif
#ifdef CONFIG_SUSE_KERNEL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 57))
/* SLES12SP3 is at least 4.4.57+ based */
#define SLE_VERSION_CODE SLE_VERSION(12, 3, 0)
#elif ( LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,28) )
/* SLES12 is at least 3.12.28+ based */
#define SLE_VERSION_CODE SLE_VERSION(12,0,0)
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,61)) && \
       (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)))
/* SLES11 SP3 is at least 3.0.61+ based */
#define SLE_VERSION_CODE SLE_VERSION(11,3,0)
#elif ( LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32) )
/* SLES11 SP1 is 2.6.32 based */
#define SLE_VERSION_CODE SLE_VERSION(11,1,0)
#elif ( LINUX_VERSION_CODE == KERNEL_VERSION(2,6,27) )
/* SLES11 GA is 2.6.27 based */
#define SLE_VERSION_CODE SLE_VERSION(11,0,0)
#endif /* LINUX_VERSION_CODE == KERNEL_VERSION(x,y,z) */
#endif /* CONFIG_SUSE_KERNEL */
#ifndef SLE_VERSION_CODE
#define SLE_VERSION_CODE 0
#endif /* SLE_VERSION_CODE */

/* Ubuntu release and kernel codes must be specified from Makefile */
#ifndef UBUNTU_RELEASE_VERSION
#define UBUNTU_RELEASE_VERSION(a,b) (((a) * 100) + (b))
#endif
#ifndef UBUNTU_KERNEL_VERSION
#define UBUNTU_KERNEL_VERSION(a,b,c,abi,upload) (((a) << 40) + ((b) << 32) + ((c) << 24) + ((abi) << 8) + (upload))
#endif
#ifndef UBUNTU_RELEASE_CODE
#define UBUNTU_RELEASE_CODE 0
#endif
#ifndef UBUNTU_KERNEL_CODE
#define UBUNTU_KERNEL_CODE 0
#endif

#ifdef __KLOCWORK__
#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#endif /* __KLOCWORK__ */

/*****************************************************************************/
/* 2.4.3 => 2.4.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3) )

/**************************************/
/* PCI DRIVER API */

#ifndef pci_set_dma_mask
#define pci_set_dma_mask _kc_pci_set_dma_mask
extern int _kc_pci_set_dma_mask(struct pci_dev *dev, dma_addr_t mask);
#endif

#ifndef pci_request_regions
#define pci_request_regions _kc_pci_request_regions
extern int _kc_pci_request_regions(struct pci_dev *pdev, char *res_name);
#endif

#ifndef pci_release_regions
#define pci_release_regions _kc_pci_release_regions
extern void _kc_pci_release_regions(struct pci_dev *pdev);
#endif

/**************************************/
/* NETWORK DRIVER API */

#ifndef alloc_etherdev
#define alloc_etherdev _kc_alloc_etherdev
extern struct net_device * _kc_alloc_etherdev(int sizeof_priv);
#endif

#ifndef is_valid_ether_addr
#define is_valid_ether_addr _kc_is_valid_ether_addr
extern int _kc_is_valid_ether_addr(u8 *addr);
#endif

/**************************************/
/* MISCELLANEOUS */

#ifndef INIT_TQUEUE
#define INIT_TQUEUE(_tq, _routine, _data)		\
	do {						\
		INIT_LIST_HEAD(&(_tq)->list);		\
		(_tq)->sync = 0;			\
		(_tq)->routine = _routine;		\
		(_tq)->data = _data;			\
	} while (0)
#endif

#endif /* 2.4.3 => 2.4.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,5) )
/* Generic MII registers. */
#define MII_BMCR            0x00        /* Basic mode control register */
#define MII_BMSR            0x01        /* Basic mode status register  */
#define MII_PHYSID1         0x02        /* PHYS ID 1                   */
#define MII_PHYSID2         0x03        /* PHYS ID 2                   */
#define MII_ADVERTISE       0x04        /* Advertisement control reg   */
#define MII_LPA             0x05        /* Link partner ability reg    */
#define MII_EXPANSION       0x06        /* Expansion register          */
/* Basic mode control register. */
#define BMCR_FULLDPLX           0x0100  /* Full duplex                 */
#define BMCR_ANENABLE           0x1000  /* Enable auto negotiation     */
/* Basic mode status register. */
#define BMSR_ERCAP              0x0001  /* Ext-reg capability          */
#define BMSR_ANEGCAPABLE        0x0008  /* Able to do auto-negotiation */
#define BMSR_10HALF             0x0800  /* Can do 10mbps, half-duplex  */
#define BMSR_10FULL             0x1000  /* Can do 10mbps, full-duplex  */
#define BMSR_100HALF            0x2000  /* Can do 100mbps, half-duplex */
#define BMSR_100FULL            0x4000  /* Can do 100mbps, full-duplex */
/* Advertisement control register. */
#define ADVERTISE_CSMA          0x0001  /* Only selector supported     */
#define ADVERTISE_10HALF        0x0020  /* Try for 10mbps half-duplex  */
#define ADVERTISE_10FULL        0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_100HALF       0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_100FULL       0x0100  /* Try for 100mbps full-duplex */
#define ADVERTISE_ALL (ADVERTISE_10HALF | ADVERTISE_10FULL | \
                       ADVERTISE_100HALF | ADVERTISE_100FULL)
/* Expansion register for auto-negotiation. */
#define EXPANSION_ENABLENPAGE   0x0004  /* This enables npage words    */
#endif

/*****************************************************************************/
/* 2.4.6 => 2.4.3 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6) )

#ifndef pci_set_power_state
#define pci_set_power_state _kc_pci_set_power_state
extern int _kc_pci_set_power_state(struct pci_dev *dev, int state);
#endif

#ifndef pci_enable_wake
#define pci_enable_wake _kc_pci_enable_wake
extern int _kc_pci_enable_wake(struct pci_dev *pdev, u32 state, int enable);
#endif

#ifndef pci_disable_device
#define pci_disable_device _kc_pci_disable_device
extern void _kc_pci_disable_device(struct pci_dev *pdev);
#endif

/* PCI PM entry point syntax changed, so don't support suspend/resume */
#undef CONFIG_PM

#endif /* 2.4.6 => 2.4.3 */

#ifndef HAVE_PCI_SET_MWI
#define pci_set_mwi(X) pci_write_config_word(X, \
			       PCI_COMMAND, adapter->hw.bus.pci_cmd_word | \
			       PCI_COMMAND_INVALIDATE);
#define pci_clear_mwi(X) pci_write_config_word(X, \
			       PCI_COMMAND, adapter->hw.bus.pci_cmd_word & \
			       ~PCI_COMMAND_INVALIDATE);
#endif

/*****************************************************************************/
/* 2.4.10 => 2.4.9 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10) )

/**************************************/
/* MODULE API */

#ifndef MODULE_LICENSE
	#define MODULE_LICENSE(X)
#endif

/**************************************/
/* OTHER */

#undef min
#define min(x,y) ({ \
	const typeof(x) _x = (x);	\
	const typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

#undef max
#define max(x,y) ({ \
	const typeof(x) _x = (x);	\
	const typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

#define min_t(type,x,y) ({ \
	type _x = (x); \
	type _y = (y); \
	_x < _y ? _x : _y; })

#define max_t(type,x,y) ({ \
	type _x = (x); \
	type _y = (y); \
	_x > _y ? _x : _y; })

#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)
#endif

#ifndef ____cacheline_aligned_in_smp
#ifdef CONFIG_SMP
#define ____cacheline_aligned_in_smp ____cacheline_aligned
#else
#define ____cacheline_aligned_in_smp
#endif /* CONFIG_SMP */
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,8) )
extern int _kc_snprintf(char * buf, size_t size, const char *fmt, ...);
#define snprintf(buf, size, fmt, args...) _kc_snprintf(buf, size, fmt, ##args)
extern int _kc_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
#define vsnprintf(buf, size, fmt, args) _kc_vsnprintf(buf, size, fmt, args)
#else /* 2.4.8 => 2.4.9 */
extern int snprintf(char * buf, size_t size, const char *fmt, ...);
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
#endif
#endif /* 2.4.10 -> 2.4.6 */


/*****************************************************************************/
/* 2.4.12 => 2.4.10 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,12) )
#ifndef HAVE_NETIF_MSG
#define HAVE_NETIF_MSG 1
enum {
	NETIF_MSG_DRV		= 0x0001,
	NETIF_MSG_PROBE		= 0x0002,
	NETIF_MSG_LINK		= 0x0004,
	NETIF_MSG_TIMER		= 0x0008,
	NETIF_MSG_IFDOWN	= 0x0010,
	NETIF_MSG_IFUP		= 0x0020,
	NETIF_MSG_RX_ERR	= 0x0040,
	NETIF_MSG_TX_ERR	= 0x0080,
	NETIF_MSG_TX_QUEUED	= 0x0100,
	NETIF_MSG_INTR		= 0x0200,
	NETIF_MSG_TX_DONE	= 0x0400,
	NETIF_MSG_RX_STATUS	= 0x0800,
	NETIF_MSG_PKTDATA	= 0x1000,
	NETIF_MSG_HW		= 0x2000,
	NETIF_MSG_WOL		= 0x4000,
};

#define netif_msg_drv(p)	((p)->msg_enable & NETIF_MSG_DRV)
#define netif_msg_probe(p)	((p)->msg_enable & NETIF_MSG_PROBE)
#define netif_msg_link(p)	((p)->msg_enable & NETIF_MSG_LINK)
#define netif_msg_timer(p)	((p)->msg_enable & NETIF_MSG_TIMER)
#define netif_msg_ifdown(p)	((p)->msg_enable & NETIF_MSG_IFDOWN)
#define netif_msg_ifup(p)	((p)->msg_enable & NETIF_MSG_IFUP)
#define netif_msg_rx_err(p)	((p)->msg_enable & NETIF_MSG_RX_ERR)
#define netif_msg_tx_err(p)	((p)->msg_enable & NETIF_MSG_TX_ERR)
#define netif_msg_tx_queued(p)	((p)->msg_enable & NETIF_MSG_TX_QUEUED)
#define netif_msg_intr(p)	((p)->msg_enable & NETIF_MSG_INTR)
#define netif_msg_tx_done(p)	((p)->msg_enable & NETIF_MSG_TX_DONE)
#define netif_msg_rx_status(p)	((p)->msg_enable & NETIF_MSG_RX_STATUS)
#define netif_msg_pktdata(p)	((p)->msg_enable & NETIF_MSG_PKTDATA)
#endif /* !HAVE_NETIF_MSG */
#endif /* 2.4.12 => 2.4.10 */

/*****************************************************************************/
/* 2.4.13 => 2.4.12 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,13) )

/**************************************/
/* PCI DMA MAPPING */

#ifndef virt_to_page
	#define virt_to_page(v) (mem_map + (virt_to_phys(v) >> PAGE_SHIFT))
#endif

#ifndef pci_map_page
#define pci_map_page _kc_pci_map_page
extern u64 _kc_pci_map_page(struct pci_dev *dev, struct page *page, unsigned long offset, size_t size, int direction);
#endif

#ifndef pci_unmap_page
#define pci_unmap_page _kc_pci_unmap_page
extern void _kc_pci_unmap_page(struct pci_dev *dev, u64 dma_addr, size_t size, int direction);
#endif

/* pci_set_dma_mask takes dma_addr_t, which is only 32-bits prior to 2.4.13 */

#undef DMA_32BIT_MASK
#define DMA_32BIT_MASK	0xffffffff
#undef DMA_64BIT_MASK
#define DMA_64BIT_MASK	0xffffffff

/**************************************/
/* OTHER */

#ifndef cpu_relax
#define cpu_relax()	rep_nop()
#endif

struct vlan_ethhdr {
	unsigned char h_dest[ETH_ALEN];
	unsigned char h_source[ETH_ALEN];
	unsigned short h_vlan_proto;
	unsigned short h_vlan_TCI;
	unsigned short h_vlan_encapsulated_proto;
};
#endif /* 2.4.13 => 2.4.12 */

/*****************************************************************************/
/* 2.4.17 => 2.4.12 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,17) )

#ifndef __devexit_p
	#define __devexit_p(x) &(x)
#endif

#else
        /* For Kernel 3.8 these are not defined - so undefine all */
        #undef __devexit_p
        #undef __devexit
        #undef __devinit
        #undef __devinitdata
        #define __devexit_p(x) &(x)
        #define __devexit
        #define __devinit
        #define __devinitdata

#endif /* 2.4.17 => 2.4.13 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18) )
#define NETIF_MSG_HW	0x2000
#define NETIF_MSG_WOL	0x4000

#ifndef netif_msg_hw
#define netif_msg_hw(p)		((p)->msg_enable & NETIF_MSG_HW)
#endif
#ifndef netif_msg_wol
#define netif_msg_wol(p)	((p)->msg_enable & NETIF_MSG_WOL)
#endif
#endif /* 2.4.18 */

/*****************************************************************************/

/*****************************************************************************/
/* 2.4.20 => 2.4.19 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20) )

/* we won't support NAPI on less than 2.4.20 */
#ifdef NAPI
#undef NAPI
#endif

#endif /* 2.4.20 => 2.4.19 */

/*****************************************************************************/
/* 2.4.22 => 2.4.17 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22) )
#define pci_name(x)	((x)->slot_name)

#ifndef SUPPORTED_10000baseT_Full
#define SUPPORTED_10000baseT_Full	(1 << 12)
#endif
#ifndef ADVERTISED_10000baseT_Full
#define ADVERTISED_10000baseT_Full	(1 << 12)
#endif
#endif

/*****************************************************************************/
/* 2.4.22 => 2.4.17 */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22) )
#ifndef IGB_NO_LRO
#define IGB_NO_LRO
#endif
#endif

/*****************************************************************************/
/*****************************************************************************/
/* 2.4.23 => 2.4.22 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23) )
/*****************************************************************************/
#ifdef NAPI
#ifndef netif_poll_disable
#define netif_poll_disable(x) _kc_netif_poll_disable(x)
static inline void _kc_netif_poll_disable(struct net_device *netdev)
{
	while (test_and_set_bit(__LINK_STATE_RX_SCHED, &netdev->state)) {
		/* No hurry */
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
	}
}
#endif
#ifndef netif_poll_enable
#define netif_poll_enable(x) _kc_netif_poll_enable(x)
static inline void _kc_netif_poll_enable(struct net_device *netdev)
{
	clear_bit(__LINK_STATE_RX_SCHED, &netdev->state);
}
#endif
#endif /* NAPI */
#ifndef netif_tx_disable
#define netif_tx_disable(x) _kc_netif_tx_disable(x)
static inline void _kc_netif_tx_disable(struct net_device *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	netif_stop_queue(dev);
	spin_unlock_bh(&dev->xmit_lock);
}
#endif
#else /* 2.4.23 => 2.4.22 */
#define HAVE_SCTP
#endif /* 2.4.23 => 2.4.22 */

/*****************************************************************************/
/* 2.6.4 => 2.6.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,25) || \
    ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) && \
      LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4) ) )
#define ETHTOOL_OPS_COMPAT
#endif /* 2.6.4 => 2.6.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27) )
#define __user
#endif /* < 2.4.27 */

/*****************************************************************************/
/* 2.5.71 => 2.4.x */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,71) )
#define sk_protocol protocol
#define pci_get_device pci_find_device
#endif /* 2.5.70 => 2.4.x */

/*****************************************************************************/
/* < 2.4.27 or 2.6.0 <= 2.6.5 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27) || \
    ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) && \
      LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5) ) )

#ifndef netif_msg_init
#define netif_msg_init _kc_netif_msg_init
static inline u32 _kc_netif_msg_init(int debug_value, int default_msg_enable_bits)
{
	/* use default */
	if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
		return default_msg_enable_bits;
	if (debug_value == 0) /* no output */
		return 0;
	/* set low N bits */
	return (1 << debug_value) -1;
}
#endif

#endif /* < 2.4.27 or 2.6.0 <= 2.6.5 */
/*****************************************************************************/
#if (( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27) ) || \
     (( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) ) && \
      ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3) )))
#define netdev_priv(x) x->priv
#endif

/*****************************************************************************/
/* <= 2.5.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0) )
#include <linux/rtnetlink.h>
#undef pci_register_driver
#define pci_register_driver pci_module_init

/*
 * Most of the dma compat code is copied/modified from the 2.4.37
 * /include/linux/libata-compat.h header file
 */
/* These definitions mirror those in pci.h, so they can be used
 * interchangeably with their PCI_ counterparts */
enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

struct device {
	struct pci_dev pdev;
};

static inline struct pci_dev *to_pci_dev (struct device *dev)
{
	return (struct pci_dev *) dev;
}
static inline struct device *pci_dev_to_dev(struct pci_dev *pdev)
{
	return (struct device *) pdev;
}

#define pdev_printk(lvl, pdev, fmt, args...)	\
	printk("%s %s: " fmt, lvl, pci_name(pdev), ## args)
#define dev_err(dev, fmt, args...)            \
	pdev_printk(KERN_ERR, to_pci_dev(dev), fmt, ## args)
#define dev_info(dev, fmt, args...)            \
	pdev_printk(KERN_INFO, to_pci_dev(dev), fmt, ## args)
#define dev_warn(dev, fmt, args...)            \
	pdev_printk(KERN_WARNING, to_pci_dev(dev), fmt, ## args)
#define dev_notice(dev, fmt, args...)            \
	pdev_printk(KERN_NOTICE, to_pci_dev(dev), fmt, ## args)
#define dev_dbg(dev, fmt, args...) \
	pdev_printk(KERN_DEBUG, to_pci_dev(dev), fmt, ## args)

/* NOTE: dangerous! we ignore the 'gfp' argument */
#define dma_alloc_coherent(dev,sz,dma,gfp) \
	pci_alloc_consistent(to_pci_dev(dev),(sz),(dma))
#define dma_free_coherent(dev,sz,addr,dma_addr) \
	pci_free_consistent(to_pci_dev(dev),(sz),(addr),(dma_addr))

#define dma_map_page(dev,a,b,c,d) \
	pci_map_page(to_pci_dev(dev),(a),(b),(c),(d))
#define dma_unmap_page(dev,a,b,c) \
	pci_unmap_page(to_pci_dev(dev),(a),(b),(c))

#define dma_map_single(dev,a,b,c) \
	pci_map_single(to_pci_dev(dev),(a),(b),(c))
#define dma_unmap_single(dev,a,b,c) \
	pci_unmap_single(to_pci_dev(dev),(a),(b),(c))

#define dma_map_sg(dev, sg, nents, dir) \
	pci_map_sg(to_pci_dev(dev), (sg), (nents), (dir)
#define dma_unmap_sg(dev, sg, nents, dir) \
	pci_unmap_sg(to_pci_dev(dev), (sg), (nents), (dir)

#define dma_sync_single(dev,a,b,c) \
	pci_dma_sync_single(to_pci_dev(dev),(a),(b),(c))

/* for range just sync everything, that's all the pci API can do */
#define dma_sync_single_range(dev,addr,off,sz,dir) \
	pci_dma_sync_single(to_pci_dev(dev),(addr),(off)+(sz),(dir))

#define dma_set_mask(dev,mask) \
	pci_set_dma_mask(to_pci_dev(dev),(mask))

/* hlist_* code - double linked lists */
struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
	*pprev = next;
	if (next)
	next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = NULL;
	n->pprev = NULL;
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

static inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}
#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

#ifndef might_sleep
#define might_sleep()
#endif
#else
static inline struct device *pci_dev_to_dev(struct pci_dev *pdev)
{
	return &pdev->dev;
}
#endif /* <= 2.5.0 */

/*****************************************************************************/
/* 2.5.28 => 2.4.23 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,28) )

#include <linux/tqueue.h>
#define work_struct tq_struct
#undef INIT_WORK
#define INIT_WORK(a,b) INIT_TQUEUE(a,(void (*)(void *))b,a)
#undef container_of
#define container_of list_entry
#define schedule_work schedule_task
#define flush_scheduled_work flush_scheduled_tasks
#define cancel_work_sync(x) flush_scheduled_work()

#endif /* 2.5.28 => 2.4.17 */

/*****************************************************************************/
/* 2.6.0 => 2.5.28 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
#ifndef read_barrier_depends
#define read_barrier_depends() rmb()
#endif

#undef get_cpu
#define get_cpu() smp_processor_id()
#undef put_cpu
#define put_cpu() do { } while(0)
#define MODULE_INFO(version, _version)
#ifndef CONFIG_E1000_DISABLE_PACKET_SPLIT
#define CONFIG_E1000_DISABLE_PACKET_SPLIT 1
#endif
#ifndef CONFIG_IGB_DISABLE_PACKET_SPLIT
#define CONFIG_IGB_DISABLE_PACKET_SPLIT 1
#endif

#define dma_set_coherent_mask(dev,mask) 1

#undef dev_put
#define dev_put(dev) __dev_put(dev)

#ifndef skb_fill_page_desc
#define skb_fill_page_desc _kc_skb_fill_page_desc
extern void _kc_skb_fill_page_desc(struct sk_buff *skb, int i, struct page *page, int off, int size);
#endif

#undef ALIGN
#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))

#ifndef page_count
#define page_count(p) atomic_read(&(p)->count)
#endif

#ifdef MAX_NUMNODES
#undef MAX_NUMNODES
#endif
#define MAX_NUMNODES 1

/* find_first_bit and find_next bit are not defined for most
 * 2.4 kernels (except for the redhat 2.4.21 kernels
 */
#include <linux/bitops.h>
#define BITOP_WORD(nr)          ((nr) / BITS_PER_LONG)
#undef find_next_bit
#define find_next_bit _kc_find_next_bit
extern unsigned long _kc_find_next_bit(const unsigned long *addr,
                                       unsigned long size,
                                       unsigned long offset);
#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)


#ifndef netdev_name
static inline const char *_kc_netdev_name(const struct net_device *dev)
{
	if (strchr(dev->name, '%'))
		return "(unregistered net_device)";
	return dev->name;
}
#define netdev_name(netdev)	_kc_netdev_name(netdev)
#endif /* netdev_name */

#ifndef strlcpy
#define strlcpy _kc_strlcpy
extern size_t _kc_strlcpy(char *dest, const char *src, size_t size);
#endif /* strlcpy */

#ifndef do_div
#if BITS_PER_LONG == 64
# define do_div(n,base) ({					\
	uint32_t __base = (base);				\
	uint32_t __rem;						\
	__rem = ((uint64_t)(n)) % __base;			\
	(n) = ((uint64_t)(n)) / __base;				\
	__rem;							\
 })
#elif BITS_PER_LONG == 32
extern uint32_t _kc__div64_32(uint64_t *dividend, uint32_t divisor);
# define do_div(n,base) ({				\
	uint32_t __base = (base);			\
	uint32_t __rem;					\
	if (likely(((n) >> 32) == 0)) {			\
		__rem = (uint32_t)(n) % __base;		\
		(n) = (uint32_t)(n) / __base;		\
	} else 						\
		__rem = _kc__div64_32(&(n), __base);	\
	__rem;						\
 })
#else /* BITS_PER_LONG == ?? */
# error do_div() does not yet support the C64
#endif /* BITS_PER_LONG */
#endif /* do_div */

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC	1000000000L
#endif

#undef HAVE_I2C_SUPPORT
#else /* 2.6.0 */
#if IS_ENABLED(CONFIG_I2C_ALGOBIT) && \
	(RHEL_RELEASE_CODE && (RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(4,9)))
#define HAVE_I2C_SUPPORT
#endif /* IS_ENABLED(CONFIG_I2C_ALGOBIT) */

#endif /* 2.6.0 => 2.5.28 */
/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3) )
#define dma_pool pci_pool
#define dma_pool_destroy pci_pool_destroy
#define dma_pool_alloc pci_pool_alloc
#define dma_pool_free pci_pool_free

#define dma_pool_create(name,dev,size,align,allocation) \
       pci_pool_create((name),to_pci_dev(dev),(size),(align),(allocation))
#endif /* < 2.6.3 */

/*****************************************************************************/
/* 2.6.4 => 2.6.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4) )
#define MODULE_VERSION(_version) MODULE_INFO(version, _version)
#endif /* 2.6.4 => 2.6.0 */

/*****************************************************************************/
/* 2.6.5 => 2.6.0 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5) )
#define dma_sync_single_for_cpu		dma_sync_single
#define dma_sync_single_for_device	dma_sync_single
#define dma_sync_single_range_for_cpu		dma_sync_single_range
#define dma_sync_single_range_for_device	dma_sync_single_range
#ifndef pci_dma_mapping_error
#define pci_dma_mapping_error _kc_pci_dma_mapping_error
static inline int _kc_pci_dma_mapping_error(dma_addr_t dma_addr)
{
	return dma_addr == 0;
}
#endif
#endif /* 2.6.5 => 2.6.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4) )
extern int _kc_scnprintf(char * buf, size_t size, const char *fmt, ...);
#define scnprintf(buf, size, fmt, args...) _kc_scnprintf(buf, size, fmt, ##args)
#endif /* < 2.6.4 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6) )
/* taken from 2.6 include/linux/bitmap.h */
#undef bitmap_zero
#define bitmap_zero _kc_bitmap_zero
static inline void _kc_bitmap_zero(unsigned long *dst, int nbits)
{
        if (nbits <= BITS_PER_LONG)
                *dst = 0UL;
        else {
                int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
                memset(dst, 0, len);
        }
}
#define random_ether_addr _kc_random_ether_addr
static inline void _kc_random_ether_addr(u8 *addr)
{
        get_random_bytes(addr, ETH_ALEN);
        addr[0] &= 0xfe; /* clear multicast */
        addr[0] |= 0x02; /* set local assignment */
}
#define page_to_nid(x) 0

#endif /* < 2.6.6 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7) )
#undef if_mii
#define if_mii _kc_if_mii
static inline struct mii_ioctl_data *_kc_if_mii(struct ifreq *rq)
{
	return (struct mii_ioctl_data *) &rq->ifr_ifru;
}

#ifndef __force
#define __force
#endif
#endif /* < 2.6.7 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8) )
#ifndef PCI_EXP_DEVCTL
#define PCI_EXP_DEVCTL 8
#endif
#ifndef PCI_EXP_DEVCTL_CERE
#define PCI_EXP_DEVCTL_CERE 0x0001
#endif
#define PCI_EXP_FLAGS		2	/* Capabilities register */
#define PCI_EXP_FLAGS_VERS	0x000f	/* Capability version */
#define PCI_EXP_FLAGS_TYPE	0x00f0	/* Device/Port type */
#define  PCI_EXP_TYPE_ENDPOINT	0x0	/* Express Endpoint */
#define  PCI_EXP_TYPE_LEG_END	0x1	/* Legacy Endpoint */
#define  PCI_EXP_TYPE_ROOT_PORT 0x4	/* Root Port */
#define  PCI_EXP_TYPE_DOWNSTREAM 0x6	/* Downstream Port */
#define PCI_EXP_FLAGS_SLOT	0x0100	/* Slot implemented */
#define PCI_EXP_DEVCAP		4	/* Device capabilities */
#define PCI_EXP_DEVSTA		10	/* Device Status */
#define msleep(x)	do { set_current_state(TASK_UNINTERRUPTIBLE); \
				schedule_timeout((x * HZ)/1000 + 2); \
			} while (0)

#endif /* < 2.6.8 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#include <net/dsfield.h>
#define __iomem

#ifndef kcalloc
#define kcalloc(n, size, flags) _kc_kzalloc(((n) * (size)), flags)
extern void *_kc_kzalloc(size_t size, int flags);
#endif
#define MSEC_PER_SEC    1000L
static inline unsigned int _kc_jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
	return (j * MSEC_PER_SEC) / HZ;
#endif
}
static inline unsigned long _kc_msecs_to_jiffies(const unsigned int m)
{
	if (m > _kc_jiffies_to_msecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (m + (MSEC_PER_SEC / HZ) - 1) / (MSEC_PER_SEC / HZ);
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return m * (HZ / MSEC_PER_SEC);
#else
	return (m * HZ + MSEC_PER_SEC - 1) / MSEC_PER_SEC;
#endif
}

#define msleep_interruptible _kc_msleep_interruptible
static inline unsigned long _kc_msleep_interruptible(unsigned int msecs)
{
	unsigned long timeout = _kc_msecs_to_jiffies(msecs) + 1;

	while (timeout && !signal_pending(current)) {
		__set_current_state(TASK_INTERRUPTIBLE);
		timeout = schedule_timeout(timeout);
	}
	return _kc_jiffies_to_msecs(timeout);
}

/* Basic mode control register. */
#define BMCR_SPEED1000		0x0040  /* MSB of Speed (1000)         */

#ifndef __le16
#define __le16 u16
#endif
#ifndef __le32
#define __le32 u32
#endif
#ifndef __le64
#define __le64 u64
#endif
#ifndef __be16
#define __be16 u16
#endif
#ifndef __be32
#define __be32 u32
#endif
#ifndef __be64
#define __be64 u64
#endif

static inline struct vlan_ethhdr *vlan_eth_hdr(const struct sk_buff *skb)
{
	return (struct vlan_ethhdr *)skb->mac.raw;
}

/* Wake-On-Lan options. */
#define WAKE_PHY		(1 << 0)
#define WAKE_UCAST		(1 << 1)
#define WAKE_MCAST		(1 << 2)
#define WAKE_BCAST		(1 << 3)
#define WAKE_ARP		(1 << 4)
#define WAKE_MAGIC		(1 << 5)
#define WAKE_MAGICSECURE	(1 << 6) /* only meaningful if WAKE_MAGIC */

#define skb_header_pointer _kc_skb_header_pointer
static inline void *_kc_skb_header_pointer(const struct sk_buff *skb,
					    int offset, int len, void *buffer)
{
	int hlen = skb_headlen(skb);

	if (hlen - offset >= len)
		return skb->data + offset;

#ifdef MAX_SKB_FRAGS
	if (skb_copy_bits(skb, offset, buffer, len) < 0)
		return NULL;

	return buffer;
#else
	return NULL;
#endif

#ifndef NETDEV_TX_OK
#define NETDEV_TX_OK 0
#endif
#ifndef NETDEV_TX_BUSY
#define NETDEV_TX_BUSY 1
#endif
#ifndef NETDEV_TX_LOCKED
#define NETDEV_TX_LOCKED -1
#endif
}

#ifndef __bitwise
#define __bitwise
#endif
#endif /* < 2.6.9 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) )
#ifdef module_param_array_named
#undef module_param_array_named
#define module_param_array_named(name, array, type, nump, perm)          \
	static struct kparam_array __param_arr_##name                    \
	= { ARRAY_SIZE(array), nump, param_set_##type, param_get_##type, \
	    sizeof(array[0]), array };                                   \
	module_param_call(name, param_array_set, param_array_get,        \
			  &__param_arr_##name, perm)
#endif /* module_param_array_named */
/*
 * num_online is broken for all < 2.6.10 kernels.  This is needed to support
 * Node module parameter of ixgbe.
 */
#undef num_online_nodes
#define num_online_nodes(n) 1
extern DECLARE_BITMAP(_kcompat_node_online_map, MAX_NUMNODES);
#undef node_online_map
#define node_online_map _kcompat_node_online_map
#define pci_get_class pci_find_class
#endif /* < 2.6.10 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11) )
#define PCI_D0      0
#define PCI_D1      1
#define PCI_D2      2
#define PCI_D3hot   3
#define PCI_D3cold  4
typedef int pci_power_t;
#define pci_choose_state(pdev,state) state
#define PMSG_SUSPEND 3
#define PCI_EXP_LNKCTL	16

#undef NETIF_F_LLTX

#ifndef ARCH_HAS_PREFETCH
#define prefetch(X)
#endif

#ifndef NET_IP_ALIGN
#define NET_IP_ALIGN 2
#endif

#define KC_USEC_PER_SEC	1000000L
#define usecs_to_jiffies _kc_usecs_to_jiffies
static inline unsigned int _kc_jiffies_to_usecs(const unsigned long j)
{
#if HZ <= KC_USEC_PER_SEC && !(KC_USEC_PER_SEC % HZ)
	return (KC_USEC_PER_SEC / HZ) * j;
#elif HZ > KC_USEC_PER_SEC && !(HZ % KC_USEC_PER_SEC)
	return (j + (HZ / KC_USEC_PER_SEC) - 1)/(HZ / KC_USEC_PER_SEC);
#else
	return (j * KC_USEC_PER_SEC) / HZ;
#endif
}
static inline unsigned long _kc_usecs_to_jiffies(const unsigned int m)
{
	if (m > _kc_jiffies_to_usecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;
#if HZ <= KC_USEC_PER_SEC && !(KC_USEC_PER_SEC % HZ)
	return (m + (KC_USEC_PER_SEC / HZ) - 1) / (KC_USEC_PER_SEC / HZ);
#elif HZ > KC_USEC_PER_SEC && !(HZ % KC_USEC_PER_SEC)
	return m * (HZ / KC_USEC_PER_SEC);
#else
	return (m * HZ + KC_USEC_PER_SEC - 1) / KC_USEC_PER_SEC;
#endif
}

#define PCI_EXP_LNKCAP		12	/* Link Capabilities */
#define PCI_EXP_LNKSTA		18	/* Link Status */
#define PCI_EXP_SLTCAP		20	/* Slot Capabilities */
#define PCI_EXP_SLTCTL		24	/* Slot Control */
#define PCI_EXP_SLTSTA		26	/* Slot Status */
#define PCI_EXP_RTCTL		28	/* Root Control */
#define PCI_EXP_RTCAP		30	/* Root Capabilities */
#define PCI_EXP_RTSTA		32	/* Root Status */
#endif /* < 2.6.11 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12) )
#include <linux/reboot.h>
#define USE_REBOOT_NOTIFIER

/* Generic MII registers. */
#define MII_CTRL1000        0x09        /* 1000BASE-T control          */
#define MII_STAT1000        0x0a        /* 1000BASE-T status           */
/* Advertisement control register. */
#define ADVERTISE_PAUSE_CAP     0x0400  /* Try for pause               */
#define ADVERTISE_PAUSE_ASYM    0x0800  /* Try for asymmetric pause     */
/* Link partner ability register. */
#define LPA_PAUSE_CAP		0x0400	/* Can pause                   */
#define LPA_PAUSE_ASYM		0x0800	/* Can pause asymetrically     */
/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL      0x0200  /* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF	0x0100  /* Advertise 1000BASE-T half duplex */
/* 1000BASE-T Status register */
#define LPA_1000LOCALRXOK	0x2000	/* Link partner local receiver status */
#define LPA_1000REMRXOK		0x1000	/* Link partner remote receiver status */

#ifndef is_zero_ether_addr
#define is_zero_ether_addr _kc_is_zero_ether_addr
static inline int _kc_is_zero_ether_addr(const u8 *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}
#endif /* is_zero_ether_addr */
#ifndef is_multicast_ether_addr
#define is_multicast_ether_addr _kc_is_multicast_ether_addr
static inline int _kc_is_multicast_ether_addr(const u8 *addr)
{
	return addr[0] & 0x01;
}
#endif /* is_multicast_ether_addr */
#endif /* < 2.6.12 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13) )
#ifndef kstrdup
#define kstrdup _kc_kstrdup
extern char *_kc_kstrdup(const char *s, unsigned int gfp);
#endif
#endif /* < 2.6.13 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14) )
#define pm_message_t u32
#ifndef kzalloc
#define kzalloc _kc_kzalloc
extern void *_kc_kzalloc(size_t size, int flags);
#endif

/* Generic MII registers. */
#define MII_ESTATUS	    0x0f	/* Extended Status */
/* Basic mode status register. */
#define BMSR_ESTATEN		0x0100	/* Extended Status in R15 */
/* Extended status register. */
#define ESTATUS_1000_TFULL	0x2000	/* Can do 1000BT Full */
#define ESTATUS_1000_THALF	0x1000	/* Can do 1000BT Half */

#define SUPPORTED_Pause	        (1 << 13)
#define SUPPORTED_Asym_Pause	(1 << 14)
#define ADVERTISED_Pause	(1 << 13)
#define ADVERTISED_Asym_Pause	(1 << 14)

#if (!(RHEL_RELEASE_CODE && \
       (RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(4,3)) && \
       (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(5,0))))
#if ((LINUX_VERSION_CODE == KERNEL_VERSION(2,6,9)) && !defined(gfp_t))
#define gfp_t unsigned
#else
typedef unsigned gfp_t;
#endif
#endif /* !RHEL4.3->RHEL5.0 */

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9) )
#ifdef CONFIG_X86_64
#define dma_sync_single_range_for_cpu(dev, addr, off, sz, dir)       \
	dma_sync_single_for_cpu((dev), (addr), (off) + (sz), (dir))
#define dma_sync_single_range_for_device(dev, addr, off, sz, dir)    \
	dma_sync_single_for_device((dev), (addr), (off) + (sz), (dir))
#endif
#endif
#endif /* < 2.6.14 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15) )
#ifndef vmalloc_node
#define vmalloc_node(a,b) vmalloc(a)
#endif /* vmalloc_node*/

#define setup_timer(_timer, _function, _data) \
do { \
	(_timer)->function = _function; \
	(_timer)->data = _data; \
	init_timer(_timer); \
} while (0)
#ifndef device_can_wakeup
#define device_can_wakeup(dev)	(1)
#endif
#ifndef device_set_wakeup_enable
#define device_set_wakeup_enable(dev, val)	do{}while(0)
#endif
#ifndef device_init_wakeup
#define device_init_wakeup(dev,val) do {} while (0)
#endif
static inline unsigned _kc_compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *) addr1;
	const u16 *b = (const u16 *) addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}
#undef compare_ether_addr
#define compare_ether_addr(addr1, addr2) _kc_compare_ether_addr(addr1, addr2)
#endif /* < 2.6.15 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) )
#undef DEFINE_MUTEX
#define DEFINE_MUTEX(x)	DECLARE_MUTEX(x)
#define mutex_lock(x)	down_interruptible(x)
#define mutex_unlock(x)	up(x)

#ifndef ____cacheline_internodealigned_in_smp
#ifdef CONFIG_SMP
#define ____cacheline_internodealigned_in_smp ____cacheline_aligned_in_smp
#else
#define ____cacheline_internodealigned_in_smp
#endif /* CONFIG_SMP */
#endif /* ____cacheline_internodealigned_in_smp */
#undef HAVE_PCI_ERS
#else /* 2.6.16 and above */
#undef HAVE_PCI_ERS
#define HAVE_PCI_ERS
#if ( SLE_VERSION_CODE && SLE_VERSION_CODE == SLE_VERSION(10,4,0) )
#ifdef device_can_wakeup
#undef device_can_wakeup
#endif /* device_can_wakeup */
#define device_can_wakeup(dev) 1
#endif /* SLE_VERSION(10,4,0) */
#endif /* < 2.6.16 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17) )
#ifndef dev_notice
#define dev_notice(dev, fmt, args...)            \
	dev_printk(KERN_NOTICE, dev, fmt, ## args)
#endif

#ifndef first_online_node
#define first_online_node 0
#endif
#ifndef NET_SKB_PAD
#define NET_SKB_PAD 16
#endif
#endif /* < 2.6.17 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) )

#ifndef IRQ_HANDLED
#define irqreturn_t void
#define IRQ_HANDLED
#define IRQ_NONE
#endif

#ifndef IRQF_PROBE_SHARED
#ifdef SA_PROBEIRQ
#define IRQF_PROBE_SHARED SA_PROBEIRQ
#else
#define IRQF_PROBE_SHARED 0
#endif
#endif

#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef FIELD_SIZEOF
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#endif

#ifndef skb_is_gso
#ifdef NETIF_F_TSO
#define skb_is_gso _kc_skb_is_gso
static inline int _kc_skb_is_gso(const struct sk_buff *skb)
{
	return skb_shinfo(skb)->gso_size;
}
#else
#define skb_is_gso(a) 0
#endif
#endif

#ifndef resource_size_t
#define resource_size_t unsigned long
#endif

#ifdef skb_pad
#undef skb_pad
#endif
#define skb_pad(x,y) _kc_skb_pad(x, y)
int _kc_skb_pad(struct sk_buff *skb, int pad);
#ifdef skb_padto
#undef skb_padto
#endif
#define skb_padto(x,y) _kc_skb_padto(x, y)
static inline int _kc_skb_padto(struct sk_buff *skb, unsigned int len)
{
	unsigned int size = skb->len;
	if(likely(size >= len))
		return 0;
	return _kc_skb_pad(skb, len - size);
}

#ifndef DECLARE_PCI_UNMAP_ADDR
#define DECLARE_PCI_UNMAP_ADDR(ADDR_NAME) \
	dma_addr_t ADDR_NAME
#define DECLARE_PCI_UNMAP_LEN(LEN_NAME) \
	u32 LEN_NAME
#define pci_unmap_addr(PTR, ADDR_NAME) \
	((PTR)->ADDR_NAME)
#define pci_unmap_addr_set(PTR, ADDR_NAME, VAL) \
	(((PTR)->ADDR_NAME) = (VAL))
#define pci_unmap_len(PTR, LEN_NAME) \
	((PTR)->LEN_NAME)
#define pci_unmap_len_set(PTR, LEN_NAME, VAL) \
	(((PTR)->LEN_NAME) = (VAL))
#endif /* DECLARE_PCI_UNMAP_ADDR */
#endif /* < 2.6.18 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) )

#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5,0)))
#define i_private u.generic_ip
#endif /* >= RHEL 5.0 */

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif
#ifndef __ALIGN_MASK
#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#endif
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) )
#if (!((RHEL_RELEASE_CODE && \
        ((RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(4,4) && \
          RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(5,0)) || \
         (RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,0))))))
typedef irqreturn_t (*irq_handler_t)(int, void*, struct pt_regs *);
#endif
#if (RHEL_RELEASE_CODE && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,0))
#undef CONFIG_INET_LRO
#undef CONFIG_INET_LRO_MODULE
#ifdef IXGBE_FCOE
#undef CONFIG_FCOE
#undef CONFIG_FCOE_MODULE
#endif /* IXGBE_FCOE */
#endif
typedef irqreturn_t (*new_handler_t)(int, void*);
static inline irqreturn_t _kc_request_irq(unsigned int irq, new_handler_t handler, unsigned long flags, const char *devname, void *dev_id)
#else /* 2.4.x */
typedef void (*irq_handler_t)(int, void*, struct pt_regs *);
typedef void (*new_handler_t)(int, void*);
static inline int _kc_request_irq(unsigned int irq, new_handler_t handler, unsigned long flags, const char *devname, void *dev_id)
#endif /* >= 2.5.x */
{
	irq_handler_t new_handler = (irq_handler_t) handler;
	return request_irq(irq, new_handler, flags, devname, dev_id);
}

#undef request_irq
#define request_irq(irq, handler, flags, devname, dev_id) _kc_request_irq((irq), (handler), (flags), (devname), (dev_id))

#define irq_handler_t new_handler_t
/* pci_restore_state and pci_save_state handles MSI/PCIE from 2.6.19 */
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5,4)))
#define PCIE_CONFIG_SPACE_LEN 256
#define PCI_CONFIG_SPACE_LEN 64
#define PCIE_LINK_STATUS 0x12
#define pci_config_space_ich8lan() do {} while(0)
#undef pci_save_state
extern int _kc_pci_save_state(struct pci_dev *);
#define pci_save_state(pdev) _kc_pci_save_state(pdev)
#undef pci_restore_state
extern void _kc_pci_restore_state(struct pci_dev *);
#define pci_restore_state(pdev) _kc_pci_restore_state(pdev)
#endif /* !(RHEL_RELEASE_CODE >= RHEL 5.4) */

#ifdef HAVE_PCI_ERS
#undef free_netdev
extern void _kc_free_netdev(struct net_device *);
#define free_netdev(netdev) _kc_free_netdev(netdev)
#endif
static inline int pci_enable_pcie_error_reporting(struct pci_dev *dev)
{
	return 0;
}
#define pci_disable_pcie_error_reporting(dev) do {} while (0)
#define pci_cleanup_aer_uncorrect_error_status(dev) do {} while (0)

extern void *_kc_kmemdup(const void *src, size_t len, unsigned gfp);
#define kmemdup(src, len, gfp) _kc_kmemdup(src, len, gfp)
#ifndef bool
#define bool _Bool
#define true 1
#define false 0
#endif
#else /* 2.6.19 */
#include <linux/aer.h>
#include <linux/string.h>
#endif /* < 2.6.19 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) )
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,28) )
#undef INIT_WORK
#define INIT_WORK(_work, _func) \
do { \
	INIT_LIST_HEAD(&(_work)->entry); \
	(_work)->pending = 0; \
	(_work)->func = (void (*)(void *))_func; \
	(_work)->data = _work; \
	init_timer(&(_work)->timer); \
} while (0)
#endif

#ifndef PCI_VDEVICE
#define PCI_VDEVICE(ven, dev)        \
	PCI_VENDOR_ID_##ven, (dev),  \
	PCI_ANY_ID, PCI_ANY_ID, 0, 0
#endif

#ifndef PCI_VENDOR_ID_INTEL
#define PCI_VENDOR_ID_INTEL 0x8086
#endif

#ifndef round_jiffies
#define round_jiffies(x) x
#endif

#define csum_offset csum

#define HAVE_EARLY_VMALLOC_NODE
#define dev_to_node(dev) -1
#undef set_dev_node
/* remove compiler warning with b=b, for unused variable */
#define set_dev_node(a, b) do { (b) = (b); } while(0)

#if (!(RHEL_RELEASE_CODE && \
       (((RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(4,7)) && \
         (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(5,0))) || \
        (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5,6)))) && \
     !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(10,2,0)))
typedef __u16 __bitwise __sum16;
typedef __u32 __bitwise __wsum;
#endif

#if (!(RHEL_RELEASE_CODE && \
       (((RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(4,7)) && \
         (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(5,0))) || \
        (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5,4)))) && \
     !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(10,2,0)))
static inline __wsum csum_unfold(__sum16 n)
{
	return (__force __wsum)n;
}
#endif

#else /* < 2.6.20 */
#define HAVE_DEVICE_NUMA_NODE
#endif /* < 2.6.20 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21) )
#define to_net_dev(class) container_of(class, struct net_device, class_dev)
#define NETDEV_CLASS_DEV
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,5)))
#define vlan_group_get_device(vg, id) (vg->vlan_devices[id])
#define vlan_group_set_device(vg, id, dev)		\
	do {						\
		if (vg) vg->vlan_devices[id] = dev;	\
	} while (0)
#endif /* !(RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,5)) */
#define pci_channel_offline(pdev) (pdev->error_state && \
	pdev->error_state != pci_channel_io_normal)
#define pci_request_selected_regions(pdev, bars, name) \
        pci_request_regions(pdev, name)
#define pci_release_selected_regions(pdev, bars) pci_release_regions(pdev);

#ifndef __aligned
#define __aligned(x)			__attribute__((aligned(x)))
#endif

extern struct pci_dev *_kc_netdev_to_pdev(struct net_device *netdev);
#define netdev_to_dev(netdev)	\
	pci_dev_to_dev(_kc_netdev_to_pdev(netdev))
#else
static inline struct device *netdev_to_dev(struct net_device *netdev)
{
	return &netdev->dev;
}

#endif /* < 2.6.21 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) )
#define tcp_hdr(skb) (skb->h.th)
#define tcp_hdrlen(skb) (skb->h.th->doff << 2)
#define skb_transport_offset(skb) (skb->h.raw - skb->data)
#define skb_transport_header(skb) (skb->h.raw)
#define ipv6_hdr(skb) (skb->nh.ipv6h)
#define ip_hdr(skb) (skb->nh.iph)
#define skb_network_offset(skb) (skb->nh.raw - skb->data)
#define skb_network_header(skb) (skb->nh.raw)
#define skb_tail_pointer(skb) skb->tail
#define skb_reset_tail_pointer(skb) \
	do { \
		skb->tail = skb->data; \
	} while (0)
#define skb_set_tail_pointer(skb, offset) \
	do { \
		skb->tail = skb->data + offset; \
	} while (0)
#define skb_copy_to_linear_data(skb, from, len) \
				memcpy(skb->data, from, len)
#define skb_copy_to_linear_data_offset(skb, offset, from, len) \
				memcpy(skb->data + offset, from, len)
#define skb_network_header_len(skb) (skb->h.raw - skb->nh.raw)
#define pci_register_driver pci_module_init
#define skb_mac_header(skb) skb->mac.raw

#ifdef NETIF_F_MULTI_QUEUE
#ifndef alloc_etherdev_mq
#define alloc_etherdev_mq(_a, _b) alloc_etherdev(_a)
#endif
#endif /* NETIF_F_MULTI_QUEUE */

#ifndef ETH_FCS_LEN
#define ETH_FCS_LEN 4
#endif
#define cancel_work_sync(x) flush_scheduled_work()
#ifndef udp_hdr
#define udp_hdr _udp_hdr
static inline struct udphdr *_udp_hdr(const struct sk_buff *skb)
{
	return (struct udphdr *)skb_transport_header(skb);
}
#endif

#ifdef cpu_to_be16
#undef cpu_to_be16
#endif
#define cpu_to_be16(x) __constant_htons(x)

#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,1)))
enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};
#endif /* !(RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,1)) */
#ifndef hex_asc
#define hex_asc(x)	"0123456789abcdef"[x]
#endif
#include <linux/ctype.h>
extern void _kc_print_hex_dump(const char *level, const char *prefix_str,
			       int prefix_type, int rowsize, int groupsize,
			       const void *buf, size_t len, bool ascii);
#define print_hex_dump(lvl, s, t, r, g, b, l, a) \
		_kc_print_hex_dump(lvl, s, t, r, g, b, l, a)
#ifndef ADVERTISED_2500baseX_Full
#define ADVERTISED_2500baseX_Full (1 << 15)
#endif
#ifndef SUPPORTED_2500baseX_Full
#define SUPPORTED_2500baseX_Full (1 << 15)
#endif

#ifdef HAVE_I2C_SUPPORT
#include <linux/i2c.h>
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,5)))
struct i2c_board_info {
	char	driver_name[KOBJ_NAME_LEN];
	char	type[I2C_NAME_SIZE];
	unsigned short	flags;
	unsigned short	addr;
	void		*platform_data;
};
#define I2C_BOARD_INFO(driver, dev_addr) .driver_name = (driver),\
			.addr = (dev_addr)
#endif /* !(RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(5,5)) */
#define i2c_new_device(adap, info) _kc_i2c_new_device(adap, info)
extern struct i2c_client *
_kc_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info);
#endif /* HAVE_I2C_SUPPORT */

#else /* 2.6.22 */
#define ETH_TYPE_TRANS_SETS_DEV
#define HAVE_NETDEV_STATS_IN_NETDEV
#endif /* < 2.6.22 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22) )
#undef SET_MODULE_OWNER
#define SET_MODULE_OWNER(dev) do { } while (0)
#endif /* > 2.6.22 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23) )
#define netif_subqueue_stopped(_a, _b) 0
#ifndef PTR_ALIGN
#define PTR_ALIGN(p, a)         ((typeof(p))ALIGN((unsigned long)(p), (a)))
#endif

#ifndef CONFIG_PM_SLEEP
#define CONFIG_PM_SLEEP	CONFIG_PM
#endif

#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13) )
#define HAVE_ETHTOOL_GET_PERM_ADDR
#endif /* 2.6.14 through 2.6.22 */
#endif /* < 2.6.23 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) )
#ifndef ETH_FLAG_LRO
#define ETH_FLAG_LRO NETIF_F_LRO
#endif

/* if GRO is supported then the napi struct must already exist */
#ifndef NETIF_F_GRO
/* NAPI API changes in 2.6.24 break everything */
struct napi_struct {
	/* used to look up the real NAPI polling routine */
	int (*poll)(struct napi_struct *, int);
	struct net_device *dev;
	int weight;
};
#endif

#ifdef NAPI
extern int __kc_adapter_clean(struct net_device *, int *);
extern struct net_device *napi_to_poll_dev(const struct napi_struct *napi);
#define netif_napi_add(_netdev, _napi, _poll, _weight) \
	do { \
		struct napi_struct *__napi = (_napi); \
		struct net_device *poll_dev = napi_to_poll_dev(__napi); \
		poll_dev->poll = &(__kc_adapter_clean); \
		poll_dev->priv = (_napi); \
		poll_dev->weight = (_weight); \
		set_bit(__LINK_STATE_RX_SCHED, &poll_dev->state); \
		set_bit(__LINK_STATE_START, &poll_dev->state);\
		dev_hold(poll_dev); \
		__napi->poll = &(_poll); \
		__napi->weight = (_weight); \
		__napi->dev = (_netdev); \
	} while (0)
#define netif_napi_del(_napi) \
	do { \
		struct net_device *poll_dev = napi_to_poll_dev(_napi); \
		WARN_ON(!test_bit(__LINK_STATE_RX_SCHED, &poll_dev->state)); \
		dev_put(poll_dev); \
		memset(poll_dev, 0, sizeof(struct net_device));\
	} while (0)
#define napi_schedule_prep(_napi) \
	(netif_running((_napi)->dev) && netif_rx_schedule_prep(napi_to_poll_dev(_napi)))
#define napi_schedule(_napi) \
	do { \
		if (napi_schedule_prep(_napi)) \
			__netif_rx_schedule(napi_to_poll_dev(_napi)); \
	} while (0)
#define napi_enable(_napi) netif_poll_enable(napi_to_poll_dev(_napi))
#define napi_disable(_napi) netif_poll_disable(napi_to_poll_dev(_napi))
#ifdef CONFIG_SMP
static inline void napi_synchronize(const struct napi_struct *n)
{
	struct net_device *dev = napi_to_poll_dev(n);

	while (test_bit(__LINK_STATE_RX_SCHED, &dev->state)) {
		/* No hurry. */
		msleep(1);
	}
}
#else
#define napi_synchronize(n)	barrier()
#endif /* CONFIG_SMP */
#define __napi_schedule(_napi) __netif_rx_schedule(napi_to_poll_dev(_napi))
#ifndef NETIF_F_GRO
#define napi_complete(_napi) netif_rx_complete(napi_to_poll_dev(_napi))
#else
#define napi_complete(_napi) \
	do { \
		napi_gro_flush(_napi); \
		netif_rx_complete(napi_to_poll_dev(_napi)); \
	} while (0)
#endif /* NETIF_F_GRO */
#else /* NAPI */
#define netif_napi_add(_netdev, _napi, _poll, _weight) \
	do { \
		struct napi_struct *__napi = _napi; \
		_netdev->poll = &(_poll); \
		_netdev->weight = (_weight); \
		__napi->poll = &(_poll); \
		__napi->weight = (_weight); \
		__napi->dev = (_netdev); \
	} while (0)
#define netif_napi_del(_a) do {} while (0)
#endif /* NAPI */

#undef dev_get_by_name
#define dev_get_by_name(_a, _b) dev_get_by_name(_b)
#define __netif_subqueue_stopped(_a, _b) netif_subqueue_stopped(_a, _b)
#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n)	(((n) == 64) ? DMA_64BIT_MASK : ((1ULL<<(n))-1))
#endif

#ifdef NETIF_F_TSO6
#define skb_is_gso_v6 _kc_skb_is_gso_v6
static inline int _kc_skb_is_gso_v6(const struct sk_buff *skb)
{
	return skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6;
}
#endif /* NETIF_F_TSO6 */

#ifndef KERN_CONT
#define KERN_CONT	""
#endif
#ifndef pr_err
#define pr_err(fmt, arg...) \
	printk(KERN_ERR fmt, ##arg)
#endif
#else /* < 2.6.24 */
#define HAVE_ETHTOOL_GET_SSET_COUNT
#define HAVE_NETDEV_NAPI_LIST
#endif /* < 2.6.24 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24) )
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0) )
#include <linux/pm_qos_params.h>
#else /* >= 3.2.0 */
#include <linux/pm_qos.h>
#endif /* else >= 3.2.0 */
#endif /* > 2.6.24 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25) )
#define PM_QOS_CPU_DMA_LATENCY	1

#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18) )
#include <linux/latency.h>
#define PM_QOS_DEFAULT_VALUE	INFINITE_LATENCY
#define pm_qos_add_requirement(pm_qos_class, name, value) \
		set_acceptable_latency(name, value)
#define pm_qos_remove_requirement(pm_qos_class, name) \
		remove_acceptable_latency(name)
#define pm_qos_update_requirement(pm_qos_class, name, value) \
		modify_acceptable_latency(name, value)
#else
#define PM_QOS_DEFAULT_VALUE	-1
#define pm_qos_add_requirement(pm_qos_class, name, value)
#define pm_qos_remove_requirement(pm_qos_class, name)
#define pm_qos_update_requirement(pm_qos_class, name, value) { \
	if (value != PM_QOS_DEFAULT_VALUE) { \
		printk(KERN_WARNING "%s: unable to set PM QoS requirement\n", \
			pci_name(adapter->pdev)); \
	} \
}

#endif /* > 2.6.18 */

#define pci_enable_device_mem(pdev) pci_enable_device(pdev)

#ifndef DEFINE_PCI_DEVICE_TABLE
#define DEFINE_PCI_DEVICE_TABLE(_table) struct pci_device_id _table[]
#endif /* DEFINE_PCI_DEVICE_TABLE */


#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) )
#ifndef IGB_PROCFS
#define IGB_PROCFS
#endif /* IGB_PROCFS */
#endif /* >= 2.6.0 */

#else /* < 2.6.25 */


#if IS_ENABLED(CONFIG_HWMON)
#ifndef IGB_HWMON
#define IGB_HWMON
#endif /* IGB_HWMON */
#endif /* CONFIG_HWMON */

#endif /* < 2.6.25 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26) )
#ifndef clamp_t
#define clamp_t(type, val, min, max) ({		\
	type __val = (val);			\
	type __min = (min);			\
	type __max = (max);			\
	__val = __val < __min ? __min : __val;	\
	__val > __max ? __max : __val; })
#endif /* clamp_t */
#undef kzalloc_node
#define kzalloc_node(_size, _flags, _node) kzalloc(_size, _flags)

extern void _kc_pci_disable_link_state(struct pci_dev *dev, int state);
#define pci_disable_link_state(p, s) _kc_pci_disable_link_state(p, s)
#else /* < 2.6.26 */
#include <linux/pci-aspm.h>
#define HAVE_NETDEV_VLAN_FEATURES
#ifndef PCI_EXP_LNKCAP_ASPMS
#define PCI_EXP_LNKCAP_ASPMS 0x00000c00 /* ASPM Support */
#endif /* PCI_EXP_LNKCAP_ASPMS */
#endif /* < 2.6.26 */
/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) )
static inline void _kc_ethtool_cmd_speed_set(struct ethtool_cmd *ep,
					     __u32 speed)
{
	ep->speed = (__u16)speed;
	/* ep->speed_hi = (__u16)(speed >> 16); */
}
#define ethtool_cmd_speed_set _kc_ethtool_cmd_speed_set

static inline __u32 _kc_ethtool_cmd_speed(struct ethtool_cmd *ep)
{
	/* no speed_hi before 2.6.27, and probably no need for it yet */
	return (__u32)ep->speed;
}
#define ethtool_cmd_speed _kc_ethtool_cmd_speed

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15) )
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) && defined(CONFIG_PM))
#define ANCIENT_PM 1
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)) && \
       (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)) && \
       defined(CONFIG_PM_SLEEP))
#define NEWER_PM 1
#endif
#if defined(ANCIENT_PM) || defined(NEWER_PM)
#undef device_set_wakeup_enable
#define device_set_wakeup_enable(dev, val) \
	do { \
		u16 pmc = 0; \
		int pm = pci_find_capability(adapter->pdev, PCI_CAP_ID_PM); \
		if (pm) { \
			pci_read_config_word(adapter->pdev, pm + PCI_PM_PMC, \
				&pmc); \
		} \
		(dev)->power.can_wakeup = !!(pmc >> 11); \
		(dev)->power.should_wakeup = (val && (pmc >> 11)); \
	} while (0)
#endif /* 2.6.15-2.6.22 and CONFIG_PM or 2.6.23-2.6.25 and CONFIG_PM_SLEEP */
#endif /* 2.6.15 through 2.6.27 */
#ifndef netif_napi_del
#define netif_napi_del(_a) do {} while (0)
#ifdef NAPI
#ifdef CONFIG_NETPOLL
#undef netif_napi_del
#define netif_napi_del(_a) list_del(&(_a)->dev_list);
#endif
#endif
#endif /* netif_napi_del */
#ifdef dma_mapping_error
#undef dma_mapping_error
#endif
#define dma_mapping_error(dev, dma_addr) pci_dma_mapping_error(dma_addr)

#ifdef CONFIG_NETDEVICES_MULTIQUEUE
#define HAVE_TX_MQ
#endif

#ifdef HAVE_TX_MQ
extern void _kc_netif_tx_stop_all_queues(struct net_device *);
extern void _kc_netif_tx_wake_all_queues(struct net_device *);
extern void _kc_netif_tx_start_all_queues(struct net_device *);
#define netif_tx_stop_all_queues(a) _kc_netif_tx_stop_all_queues(a)
#define netif_tx_wake_all_queues(a) _kc_netif_tx_wake_all_queues(a)
#define netif_tx_start_all_queues(a) _kc_netif_tx_start_all_queues(a)
#undef netif_stop_subqueue
#define netif_stop_subqueue(_ndev,_qi) do { \
	if (netif_is_multiqueue((_ndev))) \
		netif_stop_subqueue((_ndev), (_qi)); \
	else \
		netif_stop_queue((_ndev)); \
	} while (0)
#undef netif_start_subqueue
#define netif_start_subqueue(_ndev,_qi) do { \
	if (netif_is_multiqueue((_ndev))) \
		netif_start_subqueue((_ndev), (_qi)); \
	else \
		netif_start_queue((_ndev)); \
	} while (0)
#else /* HAVE_TX_MQ */
#define netif_tx_stop_all_queues(a) netif_stop_queue(a)
#define netif_tx_wake_all_queues(a) netif_wake_queue(a)
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12) )
#define netif_tx_start_all_queues(a) netif_start_queue(a)
#else
#define netif_tx_start_all_queues(a) do {} while (0)
#endif
#define netif_stop_subqueue(_ndev,_qi) netif_stop_queue((_ndev))
#define netif_start_subqueue(_ndev,_qi) netif_start_queue((_ndev))
#endif /* HAVE_TX_MQ */
#ifndef NETIF_F_MULTI_QUEUE
#define NETIF_F_MULTI_QUEUE 0
#define netif_is_multiqueue(a) 0
#define netif_wake_subqueue(a, b)
#endif /* NETIF_F_MULTI_QUEUE */

#ifndef __WARN_printf
extern void __kc_warn_slowpath(const char *file, const int line,
		const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#define __WARN_printf(arg...) __kc_warn_slowpath(__FILE__, __LINE__, arg)
#endif /* __WARN_printf */

#ifndef WARN
#define WARN(condition, format...) ({						\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		__WARN_printf(format);					\
	unlikely(__ret_warn_on);					\
})
#endif /* WARN */
#undef HAVE_IXGBE_DEBUG_FS
#undef HAVE_IGB_DEBUG_FS
#else /* < 2.6.27 */
#define HAVE_TX_MQ
#define HAVE_NETDEV_SELECT_QUEUE
#ifdef CONFIG_DEBUG_FS
#define HAVE_IXGBE_DEBUG_FS
#define HAVE_IGB_DEBUG_FS
#endif /* CONFIG_DEBUG_FS */
#endif /* < 2.6.27 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) )
#define pci_ioremap_bar(pdev, bar)	ioremap(pci_resource_start(pdev, bar), \
					        pci_resource_len(pdev, bar))
#define pci_wake_from_d3 _kc_pci_wake_from_d3
#define pci_prepare_to_sleep _kc_pci_prepare_to_sleep
extern int _kc_pci_wake_from_d3(struct pci_dev *dev, bool enable);
extern int _kc_pci_prepare_to_sleep(struct pci_dev *dev);
#define netdev_alloc_page(a) alloc_page(GFP_ATOMIC)
#ifndef __skb_queue_head_init
static inline void __kc_skb_queue_head_init(struct sk_buff_head *list)
{
	list->prev = list->next = (struct sk_buff *)list;
	list->qlen = 0;
}
#define __skb_queue_head_init(_q) __kc_skb_queue_head_init(_q)
#endif

#define PCI_EXP_DEVCAP2		36	/* Device Capabilities 2 */
#define PCI_EXP_DEVCTL2		40	/* Device Control 2 */

#endif /* < 2.6.28 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) )
#ifndef swap
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)
#endif
#define pci_request_selected_regions_exclusive(pdev, bars, name) \
		pci_request_selected_regions(pdev, bars, name)
#ifndef CONFIG_NR_CPUS
#define CONFIG_NR_CPUS 1
#endif /* CONFIG_NR_CPUS */
#ifndef pcie_aspm_enabled
#define pcie_aspm_enabled()   (1)
#endif /* pcie_aspm_enabled */

#define  PCI_EXP_SLTSTA_PDS	0x0040	/* Presence Detect State */

#ifndef pci_clear_master
extern void _kc_pci_clear_master(struct pci_dev *dev);
#define pci_clear_master(dev)	_kc_pci_clear_master(dev)
#endif

#ifndef PCI_EXP_LNKCTL_ASPMC
#define  PCI_EXP_LNKCTL_ASPMC	0x0003	/* ASPM Control */
#endif
#else /* < 2.6.29 */
#ifndef HAVE_NET_DEVICE_OPS
#define HAVE_NET_DEVICE_OPS
#endif
#ifdef CONFIG_DCB
#define HAVE_PFC_MODE_ENABLE
#endif /* CONFIG_DCB */
#endif /* < 2.6.29 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30) )
#define skb_rx_queue_recorded(a) false
#define skb_get_rx_queue(a) 0
#define skb_record_rx_queue(a, b) do {} while (0)
#define skb_tx_hash(n, s) ___kc_skb_tx_hash((n), (s), (n)->real_num_tx_queues)
#ifndef CONFIG_PCI_IOV
#undef pci_enable_sriov
#define pci_enable_sriov(a, b) -ENOTSUPP
#undef pci_disable_sriov
#define pci_disable_sriov(a) do {} while (0)
#endif /* CONFIG_PCI_IOV */
#ifndef pr_cont
#define pr_cont(fmt, ...) \
	printk(KERN_CONT fmt, ##__VA_ARGS__)
#endif /* pr_cont */
static inline void _kc_synchronize_irq(unsigned int a)
{
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,5,28) )
	synchronize_irq();
#else /* < 2.5.28 */
	synchronize_irq(a);
#endif /* < 2.5.28 */
}
#undef synchronize_irq
#define synchronize_irq(a) _kc_synchronize_irq(a)

#define PCI_EXP_LNKCTL2		48	/* Link Control 2 */

#else /* < 2.6.30 */
#define HAVE_ASPM_QUIRKS
#endif /* < 2.6.30 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31) )
#define ETH_P_1588 0x88F7
#define ETH_P_FIP  0x8914
#ifndef netdev_uc_count
#define netdev_uc_count(dev) ((dev)->uc_count)
#endif
#ifndef netdev_for_each_uc_addr
#define netdev_for_each_uc_addr(uclist, dev) \
	for (uclist = dev->uc_list; uclist; uclist = uclist->next)
#endif
#ifndef PORT_OTHER
#define PORT_OTHER 0xff
#endif
#ifndef MDIO_PHY_ID_PRTAD
#define MDIO_PHY_ID_PRTAD 0x03e0
#endif
#ifndef MDIO_PHY_ID_DEVAD
#define MDIO_PHY_ID_DEVAD 0x001f
#endif
#ifndef skb_dst
#define skb_dst(s) ((s)->dst)
#endif

#ifndef SUPPORTED_1000baseKX_Full
#define SUPPORTED_1000baseKX_Full	(1 << 17)
#endif
#ifndef SUPPORTED_10000baseKX4_Full
#define SUPPORTED_10000baseKX4_Full	(1 << 18)
#endif
#ifndef SUPPORTED_10000baseKR_Full
#define SUPPORTED_10000baseKR_Full	(1 << 19)
#endif

#ifndef ADVERTISED_1000baseKX_Full
#define ADVERTISED_1000baseKX_Full	(1 << 17)
#endif
#ifndef ADVERTISED_10000baseKX4_Full
#define ADVERTISED_10000baseKX4_Full	(1 << 18)
#endif
#ifndef ADVERTISED_10000baseKR_Full
#define ADVERTISED_10000baseKR_Full	(1 << 19)
#endif

#else /* < 2.6.31 */
#ifndef HAVE_NETDEV_STORAGE_ADDRESS
#define HAVE_NETDEV_STORAGE_ADDRESS
#endif
#ifndef HAVE_NETDEV_HW_ADDR
#define HAVE_NETDEV_HW_ADDR
#endif
#ifndef HAVE_TRANS_START_IN_QUEUE
#define HAVE_TRANS_START_IN_QUEUE
#endif
#ifndef HAVE_INCLUDE_LINUX_MDIO_H
#define HAVE_INCLUDE_LINUX_MDIO_H
#endif
#endif /* < 2.6.31 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32) )
#undef netdev_tx_t
#define netdev_tx_t int
#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#ifndef NETIF_F_FCOE_MTU
#define NETIF_F_FCOE_MTU       (1 << 26)
#endif
#endif /* CONFIG_FCOE || CONFIG_FCOE_MODULE */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
static inline int _kc_pm_runtime_get_sync()
{
	return 1;
}
#define pm_runtime_get_sync(dev)	_kc_pm_runtime_get_sync()
#else /* 2.6.0 => 2.6.32 */
static inline int _kc_pm_runtime_get_sync(struct device *dev)
{
	return 1;
}
#ifndef pm_runtime_get_sync
#define pm_runtime_get_sync(dev)	_kc_pm_runtime_get_sync(dev)
#endif
#endif /* 2.6.0 => 2.6.32 */
#ifndef pm_runtime_put
#define pm_runtime_put(dev)		do {} while (0)
#endif
#ifndef pm_runtime_put_sync
#define pm_runtime_put_sync(dev)	do {} while (0)
#endif
#ifndef pm_runtime_resume
#define pm_runtime_resume(dev)		do {} while (0)
#endif
#ifndef pm_schedule_suspend
#define pm_schedule_suspend(dev, t)	do {} while (0)
#endif
#ifndef pm_runtime_set_suspended
#define pm_runtime_set_suspended(dev)	do {} while (0)
#endif
#ifndef pm_runtime_disable
#define pm_runtime_disable(dev)		do {} while (0)
#endif
#ifndef pm_runtime_put_noidle
#define pm_runtime_put_noidle(dev)	do {} while (0)
#endif
#ifndef pm_runtime_set_active
#define pm_runtime_set_active(dev)	do {} while (0)
#endif
#ifndef pm_runtime_enable
#define pm_runtime_enable(dev)	do {} while (0)
#endif
#ifndef pm_runtime_get_noresume
#define pm_runtime_get_noresume(dev)	do {} while (0)
#endif
#else /* < 2.6.32 */
#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#ifndef HAVE_NETDEV_OPS_FCOE_ENABLE
#define HAVE_NETDEV_OPS_FCOE_ENABLE
#endif
#endif /* CONFIG_FCOE || CONFIG_FCOE_MODULE */
#ifdef CONFIG_DCB
#ifndef HAVE_DCBNL_OPS_GETAPP
#define HAVE_DCBNL_OPS_GETAPP
#endif
#endif /* CONFIG_DCB */
#include <linux/pm_runtime.h>
/* IOV bad DMA target work arounds require at least this kernel rev support */
#define HAVE_PCIE_TYPE
#endif /* < 2.6.32 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33) )
#ifndef pci_pcie_cap
#define pci_pcie_cap(pdev) pci_find_capability(pdev, PCI_CAP_ID_EXP)
#endif
#ifndef IPV4_FLOW
#define IPV4_FLOW 0x10
#endif /* IPV4_FLOW */
#ifndef IPV6_FLOW
#define IPV6_FLOW 0x11
#endif /* IPV6_FLOW */
/* Features back-ported to RHEL6 or SLES11 SP1 after 2.6.32 */
#if ( (RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,0)) || \
      (SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,1,0)) )
#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#ifndef HAVE_NETDEV_OPS_FCOE_GETWWN
#define HAVE_NETDEV_OPS_FCOE_GETWWN
#endif
#endif /* CONFIG_FCOE || CONFIG_FCOE_MODULE */
#endif /* RHEL6 or SLES11 SP1 */
#ifndef __percpu
#define __percpu
#endif /* __percpu */
#ifndef PORT_DA
#define PORT_DA PORT_OTHER
#endif
#ifndef PORT_NONE
#define PORT_NONE PORT_OTHER
#endif

#if ((RHEL_RELEASE_CODE && \
     (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,3)) && \
     (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,0))))
#if !defined(CONFIG_X86_32) && !defined(CONFIG_NEED_DMA_MAP_STATE)
#undef DEFINE_DMA_UNMAP_ADDR
#define DEFINE_DMA_UNMAP_ADDR(ADDR_NAME)	dma_addr_t ADDR_NAME
#undef DEFINE_DMA_UNMAP_LEN
#define DEFINE_DMA_UNMAP_LEN(LEN_NAME)		__u32 LEN_NAME
#undef dma_unmap_addr
#define dma_unmap_addr(PTR, ADDR_NAME)		((PTR)->ADDR_NAME)
#undef dma_unmap_addr_set
#define dma_unmap_addr_set(PTR, ADDR_NAME, VAL)	(((PTR)->ADDR_NAME) = (VAL))
#undef dma_unmap_len
#define dma_unmap_len(PTR, LEN_NAME)		((PTR)->LEN_NAME)
#undef dma_unmap_len_set
#define dma_unmap_len_set(PTR, LEN_NAME, VAL)	(((PTR)->LEN_NAME) = (VAL))
#endif /* CONFIG_X86_64 && !CONFIG_NEED_DMA_MAP_STATE */
#endif /* RHEL_RELEASE_CODE */

#if (!(RHEL_RELEASE_CODE && \
       (((RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5,8)) && \
         (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,0))) || \
        ((RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,1)) && \
         (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,0))))))
static inline bool pci_is_pcie(struct pci_dev *dev)
{
	return !!pci_pcie_cap(dev);
}
#endif /* RHEL_RELEASE_CODE */

#ifndef __always_unused
#define __always_unused __attribute__((__unused__))
#endif
#ifndef __maybe_unused
#define __maybe_unused __attribute__((__unused__))
#endif

#if (!(RHEL_RELEASE_CODE && \
      (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,2))))
#define sk_tx_queue_get(_sk) (-1)
#define sk_tx_queue_set(_sk, _tx_queue) do {} while(0)
#endif /* !(RHEL >= 6.2) */

#if (RHEL_RELEASE_CODE && \
     (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,4)) && \
     (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,0)))
#define HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#define HAVE_ETHTOOL_SET_PHYS_ID
#define HAVE_ETHTOOL_GET_TS_INFO
#endif /* RHEL >= 6.4 && RHEL < 7.0 */

#if (RHEL_RELEASE_CODE && \
     (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,5)) && \
     (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,0)))
#define HAVE_RHEL6_NETDEV_OPS_EXT_FDB
#endif /* RHEL >= 6.5 && RHEL < 7.0 */

#else /* < 2.6.33 */
#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#ifndef HAVE_NETDEV_OPS_FCOE_GETWWN
#define HAVE_NETDEV_OPS_FCOE_GETWWN
#endif
#endif /* CONFIG_FCOE || CONFIG_FCOE_MODULE */
#endif /* < 2.6.33 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34) )
#if (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,0))
#ifndef pci_num_vf
#define pci_num_vf(pdev) _kc_pci_num_vf(pdev)
extern int _kc_pci_num_vf(struct pci_dev *dev);
#endif
#endif /* RHEL_RELEASE_CODE */

#ifndef ETH_FLAG_NTUPLE
#define ETH_FLAG_NTUPLE NETIF_F_NTUPLE
#endif

#ifndef netdev_mc_count
#define netdev_mc_count(dev) ((dev)->mc_count)
#endif
#ifndef netdev_mc_empty
#define netdev_mc_empty(dev) (netdev_mc_count(dev) == 0)
#endif
#ifndef netdev_for_each_mc_addr
#define netdev_for_each_mc_addr(mclist, dev) \
	for (mclist = dev->mc_list; mclist; mclist = mclist->next)
#endif
#ifndef netdev_uc_count
#define netdev_uc_count(dev) ((dev)->uc.count)
#endif
#ifndef netdev_uc_empty
#define netdev_uc_empty(dev) (netdev_uc_count(dev) == 0)
#endif
#ifndef netdev_for_each_uc_addr
#define netdev_for_each_uc_addr(ha, dev) \
	list_for_each_entry(ha, &dev->uc.list, list)
#endif
#ifndef dma_set_coherent_mask
#define dma_set_coherent_mask(dev,mask) \
	pci_set_consistent_dma_mask(to_pci_dev(dev),(mask))
#endif
#ifndef pci_dev_run_wake
#define pci_dev_run_wake(pdev)	(0)
#endif

/* netdev logging taken from include/linux/netdevice.h */
#ifndef netdev_name
static inline const char *_kc_netdev_name(const struct net_device *dev)
{
	if (dev->reg_state != NETREG_REGISTERED)
		return "(unregistered net_device)";
	return dev->name;
}
#define netdev_name(netdev)	_kc_netdev_name(netdev)
#endif /* netdev_name */

#undef netdev_printk
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
#define netdev_printk(level, netdev, format, args...)		\
do {								\
	struct pci_dev *pdev = _kc_netdev_to_pdev(netdev);	\
	printk(level "%s: " format, pci_name(pdev), ##args);	\
} while(0)
#elif ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21) )
#define netdev_printk(level, netdev, format, args...)		\
do {								\
	struct pci_dev *pdev = _kc_netdev_to_pdev(netdev);	\
	struct device *dev = pci_dev_to_dev(pdev);		\
	dev_printk(level, dev, "%s: " format,			\
		   netdev_name(netdev), ##args);		\
} while(0)
#else /* 2.6.21 => 2.6.34 */
#define netdev_printk(level, netdev, format, args...)		\
	dev_printk(level, (netdev)->dev.parent,			\
		   "%s: " format,				\
		   netdev_name(netdev), ##args)
#endif /* <2.6.0 <2.6.21 <2.6.34 */
#undef netdev_emerg
#define netdev_emerg(dev, format, args...)			\
	netdev_printk(KERN_EMERG, dev, format, ##args)
#undef netdev_alert
#define netdev_alert(dev, format, args...)			\
	netdev_printk(KERN_ALERT, dev, format, ##args)
#undef netdev_crit
#define netdev_crit(dev, format, args...)			\
	netdev_printk(KERN_CRIT, dev, format, ##args)
#undef netdev_err
#define netdev_err(dev, format, args...)			\
	netdev_printk(KERN_ERR, dev, format, ##args)
#undef netdev_warn
#define netdev_warn(dev, format, args...)			\
	netdev_printk(KERN_WARNING, dev, format, ##args)
#undef netdev_notice
#define netdev_notice(dev, format, args...)			\
	netdev_printk(KERN_NOTICE, dev, format, ##args)
#undef netdev_info
#define netdev_info(dev, format, args...)			\
	netdev_printk(KERN_INFO, dev, format, ##args)
#undef netdev_dbg
#if defined(DEBUG)
#define netdev_dbg(__dev, format, args...)			\
	netdev_printk(KERN_DEBUG, __dev, format, ##args)
#elif defined(CONFIG_DYNAMIC_DEBUG)
#define netdev_dbg(__dev, format, args...)			\
do {								\
	dynamic_dev_dbg((__dev)->dev.parent, "%s: " format,	\
			netdev_name(__dev), ##args);		\
} while (0)
#else /* DEBUG */
#define netdev_dbg(__dev, format, args...)			\
({								\
	if (0)							\
		netdev_printk(KERN_DEBUG, __dev, format, ##args); \
	0;							\
})
#endif /* DEBUG */

#undef netif_printk
#define netif_printk(priv, type, level, dev, fmt, args...)	\
do {								\
	if (netif_msg_##type(priv))				\
		netdev_printk(level, (dev), fmt, ##args);	\
} while (0)

#undef netif_emerg
#define netif_emerg(priv, type, dev, fmt, args...)		\
	netif_level(emerg, priv, type, dev, fmt, ##args)
#undef netif_alert
#define netif_alert(priv, type, dev, fmt, args...)		\
	netif_level(alert, priv, type, dev, fmt, ##args)
#undef netif_crit
#define netif_crit(priv, type, dev, fmt, args...)		\
	netif_level(crit, priv, type, dev, fmt, ##args)
#undef netif_err
#define netif_err(priv, type, dev, fmt, args...)		\
	netif_level(err, priv, type, dev, fmt, ##args)
#undef netif_warn
#define netif_warn(priv, type, dev, fmt, args...)		\
	netif_level(warn, priv, type, dev, fmt, ##args)
#undef netif_notice
#define netif_notice(priv, type, dev, fmt, args...)		\
	netif_level(notice, priv, type, dev, fmt, ##args)
#undef netif_info
#define netif_info(priv, type, dev, fmt, args...)		\
	netif_level(info, priv, type, dev, fmt, ##args)
#undef netif_dbg
#define netif_dbg(priv, type, dev, fmt, args...)		\
	netif_level(dbg, priv, type, dev, fmt, ##args)

#ifdef SET_SYSTEM_SLEEP_PM_OPS
#define HAVE_SYSTEM_SLEEP_PM_OPS
#endif

#ifndef for_each_set_bit
#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size)); \
		(bit) < (size); \
		(bit) = find_next_bit((addr), (size), (bit) + 1))
#endif /* for_each_set_bit */

#ifndef DEFINE_DMA_UNMAP_ADDR
#define DEFINE_DMA_UNMAP_ADDR DECLARE_PCI_UNMAP_ADDR
#define DEFINE_DMA_UNMAP_LEN DECLARE_PCI_UNMAP_LEN
#define dma_unmap_addr pci_unmap_addr
#define dma_unmap_addr_set pci_unmap_addr_set
#define dma_unmap_len pci_unmap_len
#define dma_unmap_len_set pci_unmap_len_set
#endif /* DEFINE_DMA_UNMAP_ADDR */

#if (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,3))
#ifdef IGB_HWMON
#ifdef CONFIG_DEBUG_LOCK_ALLOC
#define sysfs_attr_init(attr)				\
	do {						\
		static struct lock_class_key __key;	\
		(attr)->key = &__key;			\
	} while (0)
#else
#define sysfs_attr_init(attr) do {} while (0)
#endif /* CONFIG_DEBUG_LOCK_ALLOC */
#endif /* IGB_HWMON */
#endif /* RHEL_RELEASE_CODE */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
static inline bool _kc_pm_runtime_suspended()
{
	return false;
}
#define pm_runtime_suspended(dev)	_kc_pm_runtime_suspended()
#else /* 2.6.0 => 2.6.34 */
static inline bool _kc_pm_runtime_suspended(struct device *dev)
{
	return false;
}
#ifndef pm_runtime_suspended
#define pm_runtime_suspended(dev)	_kc_pm_runtime_suspended(dev)
#endif
#endif /* 2.6.0 => 2.6.34 */

#else /* < 2.6.34 */
#define HAVE_SYSTEM_SLEEP_PM_OPS
#ifndef HAVE_SET_RX_MODE
#define HAVE_SET_RX_MODE
#endif

#endif /* < 2.6.34 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) )

ssize_t _kc_simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
				   const void __user *from, size_t count);
#define simple_write_to_buffer _kc_simple_write_to_buffer

#ifndef numa_node_id
#define numa_node_id() 0
#endif
#ifdef HAVE_TX_MQ
#include <net/sch_generic.h>
#ifndef CONFIG_NETDEVICES_MULTIQUEUE
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,0)))
void _kc_netif_set_real_num_tx_queues(struct net_device *, unsigned int);
#define netif_set_real_num_tx_queues  _kc_netif_set_real_num_tx_queues
#endif /* !(RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,0)) */
#else /* CONFIG_NETDEVICES_MULTI_QUEUE */
#define netif_set_real_num_tx_queues(_netdev, _count) \
	do { \
		(_netdev)->egress_subqueue_count = _count; \
	} while (0)
#endif /* CONFIG_NETDEVICES_MULTI_QUEUE */
#else /* HAVE_TX_MQ */
#define netif_set_real_num_tx_queues(_netdev, _count) do {} while(0)
#endif /* HAVE_TX_MQ */
#ifndef ETH_FLAG_RXHASH
#define ETH_FLAG_RXHASH (1<<28)
#endif /* ETH_FLAG_RXHASH */
#if (RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,0))
#define HAVE_IRQ_AFFINITY_HINT
#endif
#else /* < 2.6.35 */
#define HAVE_PM_QOS_REQUEST_LIST
#define HAVE_IRQ_AFFINITY_HINT
#endif /* < 2.6.35 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36) )
extern int _kc_ethtool_op_set_flags(struct net_device *, u32, u32);
#define ethtool_op_set_flags _kc_ethtool_op_set_flags
extern u32 _kc_ethtool_op_get_flags(struct net_device *);
#define ethtool_op_get_flags _kc_ethtool_op_get_flags

#ifdef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#ifdef NET_IP_ALIGN
#undef NET_IP_ALIGN
#endif
#define NET_IP_ALIGN 0
#endif /* CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS */

#ifdef NET_SKB_PAD
#undef NET_SKB_PAD
#endif

#if (L1_CACHE_BYTES > 32)
#define NET_SKB_PAD L1_CACHE_BYTES
#else
#define NET_SKB_PAD 32
#endif

static inline struct sk_buff *_kc_netdev_alloc_skb_ip_align(struct net_device *dev,
							    unsigned int length)
{
	struct sk_buff *skb;

	skb = alloc_skb(length + NET_SKB_PAD + NET_IP_ALIGN, GFP_ATOMIC);
	if (skb) {
#if (NET_IP_ALIGN + NET_SKB_PAD)
		skb_reserve(skb, NET_IP_ALIGN + NET_SKB_PAD);
#endif
		skb->dev = dev;
	}
	return skb;
}

#ifdef netdev_alloc_skb_ip_align
#undef netdev_alloc_skb_ip_align
#endif
#define netdev_alloc_skb_ip_align(n, l) _kc_netdev_alloc_skb_ip_align(n, l)

#undef netif_level
#define netif_level(level, priv, type, dev, fmt, args...)	\
do {								\
	if (netif_msg_##type(priv))				\
		netdev_##level(dev, fmt, ##args);		\
} while (0)

#undef usleep_range
#define usleep_range(min, max)	msleep(DIV_ROUND_UP(min, 1000))

#define u64_stats_update_begin(a) do { } while(0)
#define u64_stats_update_end(a) do { } while(0)
#define u64_stats_fetch_begin(a) do { } while(0)
#define u64_stats_fetch_retry_bh(a) (0)
#define u64_stats_fetch_begin_bh(a) (0)

#if (RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,1))
#define HAVE_8021P_SUPPORT
#endif

#else /* < 2.6.36 */


#define HAVE_PM_QOS_REQUEST_ACTIVE
#define HAVE_8021P_SUPPORT
#define HAVE_NDO_GET_STATS64
#endif /* < 2.6.36 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37) )
#ifndef netif_set_real_num_rx_queues
static inline int __kc_netif_set_real_num_rx_queues(struct net_device *dev,
						    unsigned int rxq)
{
	return 0;
}
#define netif_set_real_num_rx_queues(dev, rxq) \
	__kc_netif_set_real_num_rx_queues((dev), (rxq))
#endif
#ifndef ETHTOOL_RXNTUPLE_ACTION_CLEAR
#define ETHTOOL_RXNTUPLE_ACTION_CLEAR (-2)
#endif
#ifndef VLAN_N_VID
#define VLAN_N_VID	VLAN_GROUP_ARRAY_LEN
#endif /* VLAN_N_VID */
#ifndef ETH_FLAG_TXVLAN
#define ETH_FLAG_TXVLAN (1 << 7)
#endif /* ETH_FLAG_TXVLAN */
#ifndef ETH_FLAG_RXVLAN
#define ETH_FLAG_RXVLAN (1 << 8)
#endif /* ETH_FLAG_RXVLAN */

static inline void _kc_skb_checksum_none_assert(struct sk_buff *skb)
{
	WARN_ON(skb->ip_summed != CHECKSUM_NONE);
}
#define skb_checksum_none_assert(skb) _kc_skb_checksum_none_assert(skb)

static inline void *_kc_vzalloc_node(unsigned long size, int node)
{
	void *addr = vmalloc_node(size, node);
	if (addr)
		memset(addr, 0, size);
	return addr;
}
#define vzalloc_node(_size, _node) _kc_vzalloc_node(_size, _node)

static inline void *_kc_vzalloc(unsigned long size)
{
	void *addr = vmalloc(size);
	if (addr)
		memset(addr, 0, size);
	return addr;
}
#define vzalloc(_size) _kc_vzalloc(_size)

#ifndef vlan_get_protocol
static inline __be16 __kc_vlan_get_protocol(const struct sk_buff *skb)
{
	if (vlan_tx_tag_present(skb) ||
	    skb->protocol != cpu_to_be16(ETH_P_8021Q))
		return skb->protocol;

	if (skb_headlen(skb) < sizeof(struct vlan_ethhdr))
		return 0;

	return ((struct vlan_ethhdr*)skb->data)->h_vlan_encapsulated_proto;
}
#define vlan_get_protocol(_skb) __kc_vlan_get_protocol(_skb)
#endif
#ifdef HAVE_HW_TIME_STAMP
#define SKBTX_HW_TSTAMP (1 << 0)
#define SKBTX_IN_PROGRESS (1 << 2)
#define SKB_SHARED_TX_IS_UNION
#endif

#ifndef device_wakeup_enable
#define device_wakeup_enable(dev)	device_set_wakeup_enable(dev, true)
#endif

#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,4,18) )
#ifndef HAVE_VLAN_RX_REGISTER
#define HAVE_VLAN_RX_REGISTER
#endif
#endif /* > 2.4.18 */
#endif /* < 2.6.37 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38) )
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) )
#define skb_checksum_start_offset(skb) skb_transport_offset(skb)
#else /* 2.6.22 -> 2.6.37 */
static inline int _kc_skb_checksum_start_offset(const struct sk_buff *skb)
{
        return skb->csum_start - skb_headroom(skb);
}
#define skb_checksum_start_offset(skb) _kc_skb_checksum_start_offset(skb)
#endif /* 2.6.22 -> 2.6.37 */
#ifdef CONFIG_DCB
#ifndef IEEE_8021QAZ_MAX_TCS
#define IEEE_8021QAZ_MAX_TCS 8
#endif
#ifndef DCB_CAP_DCBX_HOST
#define DCB_CAP_DCBX_HOST		0x01
#endif
#ifndef DCB_CAP_DCBX_LLD_MANAGED
#define DCB_CAP_DCBX_LLD_MANAGED	0x02
#endif
#ifndef DCB_CAP_DCBX_VER_CEE
#define DCB_CAP_DCBX_VER_CEE		0x04
#endif
#ifndef DCB_CAP_DCBX_VER_IEEE
#define DCB_CAP_DCBX_VER_IEEE		0x08
#endif
#ifndef DCB_CAP_DCBX_STATIC
#define DCB_CAP_DCBX_STATIC		0x10
#endif
#endif /* CONFIG_DCB */
#if (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,2))
#define CONFIG_XPS
#endif /* RHEL_RELEASE_VERSION(6,2) */
#endif /* < 2.6.38 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39) )
#ifndef NETIF_F_RXCSUM
#define NETIF_F_RXCSUM		(1 << 29)
#endif
#ifndef skb_queue_reverse_walk_safe
#define skb_queue_reverse_walk_safe(queue, skb, tmp)				\
		for (skb = (queue)->prev, tmp = skb->prev;			\
		     skb != (struct sk_buff *)(queue);				\
		     skb = tmp, tmp = skb->prev)
#endif
#else /* < 2.6.39 */
#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#ifndef HAVE_NETDEV_OPS_FCOE_DDP_TARGET
#define HAVE_NETDEV_OPS_FCOE_DDP_TARGET
#endif
#endif /* CONFIG_FCOE || CONFIG_FCOE_MODULE */
#ifndef HAVE_MQPRIO
#define HAVE_MQPRIO
#endif
#ifndef HAVE_SETUP_TC
#define HAVE_SETUP_TC
#endif
#ifdef CONFIG_DCB
#ifndef HAVE_DCBNL_IEEE
#define HAVE_DCBNL_IEEE
#endif
#endif /* CONFIG_DCB */
#ifndef HAVE_NDO_SET_FEATURES
#define HAVE_NDO_SET_FEATURES
#endif
#endif /* < 2.6.39 */

/*****************************************************************************/
/* use < 2.6.40 because of a Fedora 15 kernel update where they
 * updated the kernel version to 2.6.40.x and they back-ported 3.0 features
 * like set_phys_id for ethtool.
 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,40) )
#ifdef ETHTOOL_GRXRINGS
#ifndef FLOW_EXT
#define	FLOW_EXT	0x80000000
union _kc_ethtool_flow_union {
	struct ethtool_tcpip4_spec		tcp_ip4_spec;
	struct ethtool_usrip4_spec		usr_ip4_spec;
	__u8					hdata[60];
};
struct _kc_ethtool_flow_ext {
	__be16	vlan_etype;
	__be16	vlan_tci;
	__be32	data[2];
};
struct _kc_ethtool_rx_flow_spec {
	__u32		flow_type;
	union _kc_ethtool_flow_union h_u;
	struct _kc_ethtool_flow_ext h_ext;
	union _kc_ethtool_flow_union m_u;
	struct _kc_ethtool_flow_ext m_ext;
	__u64		ring_cookie;
	__u32		location;
};
#define ethtool_rx_flow_spec _kc_ethtool_rx_flow_spec
#endif /* FLOW_EXT */
#endif

#define pci_disable_link_state_locked pci_disable_link_state

#ifndef PCI_LTR_VALUE_MASK
#define  PCI_LTR_VALUE_MASK	0x000003ff
#endif
#ifndef PCI_LTR_SCALE_MASK
#define  PCI_LTR_SCALE_MASK	0x00001c00
#endif
#ifndef PCI_LTR_SCALE_SHIFT
#define  PCI_LTR_SCALE_SHIFT	10
#endif

#else /* < 2.6.40 */
#define HAVE_ETHTOOL_SET_PHYS_ID
#endif /* < 2.6.40 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0) )
#define USE_LEGACY_PM_SUPPORT
#endif /* < 3.0.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0) )
#ifndef __netdev_alloc_skb_ip_align
#define __netdev_alloc_skb_ip_align(d,l,_g) netdev_alloc_skb_ip_align(d,l)
#endif /* __netdev_alloc_skb_ip_align */
#define dcb_ieee_setapp(dev, app) dcb_setapp(dev, app)
#define dcb_ieee_delapp(dev, app) 0
#define dcb_ieee_getapp_mask(dev, app) (1 << app->priority)

/* 1000BASE-T Control register */
#define CTL1000_AS_MASTER	0x0800
#define CTL1000_ENABLE_MASTER	0x1000

#else /* < 3.1.0 */
#ifndef HAVE_DCBNL_IEEE_DELAPP
#define HAVE_DCBNL_IEEE_DELAPP
#endif
#endif /* < 3.1.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0) )
#ifdef ETHTOOL_GRXRINGS
#define HAVE_ETHTOOL_GET_RXNFC_VOID_RULE_LOCS
#endif /* ETHTOOL_GRXRINGS */

#ifndef skb_frag_size
#define skb_frag_size(frag)	_kc_skb_frag_size(frag)
static inline unsigned int _kc_skb_frag_size(const skb_frag_t *frag)
{
	return frag->size;
}
#endif /* skb_frag_size */

#ifndef skb_frag_size_sub
#define skb_frag_size_sub(frag, delta)	_kc_skb_frag_size_sub(frag, delta)
static inline void _kc_skb_frag_size_sub(skb_frag_t *frag, int delta)
{
	frag->size -= delta;
}
#endif /* skb_frag_size_sub */

#ifndef skb_frag_page
#define skb_frag_page(frag)	_kc_skb_frag_page(frag)
static inline struct page *_kc_skb_frag_page(const skb_frag_t *frag)
{
	return frag->page;
}
#endif /* skb_frag_page */

#ifndef skb_frag_address
#define skb_frag_address(frag)	_kc_skb_frag_address(frag)
static inline void *_kc_skb_frag_address(const skb_frag_t *frag)
{
	return page_address(skb_frag_page(frag)) + frag->page_offset;
}
#endif /* skb_frag_address */

#ifndef skb_frag_dma_map
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) )
#include <linux/dma-mapping.h>
#endif
#define skb_frag_dma_map(dev,frag,offset,size,dir) \
		_kc_skb_frag_dma_map(dev,frag,offset,size,dir)
static inline dma_addr_t _kc_skb_frag_dma_map(struct device *dev,
					      const skb_frag_t *frag,
					      size_t offset, size_t size,
					      enum dma_data_direction dir)
{
	return dma_map_page(dev, skb_frag_page(frag),
			    frag->page_offset + offset, size, dir);
}
#endif /* skb_frag_dma_map */

#ifndef __skb_frag_unref
#define __skb_frag_unref(frag) __kc_skb_frag_unref(frag)
static inline void __kc_skb_frag_unref(skb_frag_t *frag)
{
	put_page(skb_frag_page(frag));
}
#endif /* __skb_frag_unref */

#ifndef SPEED_UNKNOWN
#define SPEED_UNKNOWN	-1
#endif
#ifndef DUPLEX_UNKNOWN
#define DUPLEX_UNKNOWN	0xff
#endif
#if (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,3))
#ifndef HAVE_PCI_DEV_FLAGS_ASSIGNED
#define HAVE_PCI_DEV_FLAGS_ASSIGNED
#endif
#endif
#else /* < 3.2.0 */
#ifndef HAVE_PCI_DEV_FLAGS_ASSIGNED
#define HAVE_PCI_DEV_FLAGS_ASSIGNED
#define HAVE_VF_SPOOFCHK_CONFIGURE
#endif
#endif /* < 3.2.0 */

#if (RHEL_RELEASE_CODE && RHEL_RELEASE_CODE == RHEL_RELEASE_VERSION(6,2))
#undef ixgbe_get_netdev_tc_txq
#define ixgbe_get_netdev_tc_txq(dev, tc) (&netdev_extended(dev)->qos_data.tc_to_txq[tc])
#endif
/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0) )
typedef u32 kni_netdev_features_t;
#undef PCI_EXP_TYPE_RC_EC
#define  PCI_EXP_TYPE_RC_EC	0xa	/* Root Complex Event Collector */
#ifndef CONFIG_BQL
#define netdev_tx_completed_queue(_q, _p, _b) do {} while (0)
#define netdev_completed_queue(_n, _p, _b) do {} while (0)
#define netdev_tx_sent_queue(_q, _b) do {} while (0)
#define netdev_sent_queue(_n, _b) do {} while (0)
#define netdev_tx_reset_queue(_q) do {} while (0)
#define netdev_reset_queue(_n) do {} while (0)
#endif
#else /* ! < 3.3.0 */
typedef netdev_features_t kni_netdev_features_t;
#define HAVE_INT_NDO_VLAN_RX_ADD_VID
#ifdef ETHTOOL_SRXNTUPLE
#undef ETHTOOL_SRXNTUPLE
#endif
#endif /* < 3.3.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0) )
#ifndef NETIF_F_RXFCS
#define NETIF_F_RXFCS	0
#endif /* NETIF_F_RXFCS */
#ifndef NETIF_F_RXALL
#define NETIF_F_RXALL	0
#endif /* NETIF_F_RXALL */

#if !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0))
#define NUMTCS_RETURNS_U8

int _kc_simple_open(struct inode *inode, struct file *file);
#define simple_open _kc_simple_open
#endif /* !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0)) */


#ifndef skb_add_rx_frag
#define skb_add_rx_frag _kc_skb_add_rx_frag
extern void _kc_skb_add_rx_frag(struct sk_buff *, int, struct page *,
				int, int, unsigned int);
#endif
#ifdef NET_ADDR_RANDOM
#define eth_hw_addr_random(N) do { \
	random_ether_addr(N->dev_addr); \
	N->addr_assign_type |= NET_ADDR_RANDOM; \
	} while (0)
#else /* NET_ADDR_RANDOM */
#define eth_hw_addr_random(N) random_ether_addr(N->dev_addr)
#endif /* NET_ADDR_RANDOM */
#else /* < 3.4.0 */
#include <linux/kconfig.h>
#endif /* >= 3.4.0 */

/*****************************************************************************/
#if defined(E1000E_PTP) || defined(IGB_PTP) || defined(IXGBE_PTP) || defined(I40E_PTP)
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) ) && IS_ENABLED(CONFIG_PTP_1588_CLOCK)
#define HAVE_PTP_1588_CLOCK
#else
#error Cannot enable PTP Hardware Clock support due to a pre-3.0 kernel version or CONFIG_PTP_1588_CLOCK not enabled in the kernel
#endif /* > 3.0.0 && IS_ENABLED(CONFIG_PTP_1588_CLOCK) */
#endif /* E1000E_PTP || IGB_PTP || IXGBE_PTP || I40E_PTP */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0) )
#define skb_tx_timestamp(skb) do {} while (0)
static inline bool __kc_ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
	return !compare_ether_addr(addr1, addr2);
}
#define ether_addr_equal(_addr1, _addr2) __kc_ether_addr_equal((_addr1),(_addr2))
#else
#define HAVE_FDB_OPS
#define HAVE_ETHTOOL_GET_TS_INFO
#endif /* < 3.5.0 */

/*****************************************************************************/
#include <linux/mdio.h>
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0) )
#define PCI_EXP_LNKCAP2		44	/* Link Capability 2 */

#ifndef MDIO_EEE_100TX
#define MDIO_EEE_100TX		0x0002	/* 100TX EEE cap */
#endif
#ifndef MDIO_EEE_1000T
#define MDIO_EEE_1000T		0x0004	/* 1000T EEE cap */
#endif
#ifndef MDIO_EEE_10GT
#define MDIO_EEE_10GT		0x0008	/* 10GT EEE cap */
#endif
#ifndef MDIO_EEE_1000KX
#define MDIO_EEE_1000KX		0x0010	/* 1000KX EEE cap */
#endif
#ifndef MDIO_EEE_10GKX4
#define MDIO_EEE_10GKX4		0x0020	/* 10G KX4 EEE cap */
#endif
#ifndef MDIO_EEE_10GKR
#define MDIO_EEE_10GKR		0x0040	/* 10G KR EEE cap */
#endif
#endif /* < 3.6.0 */

/******************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0) )
#ifndef ADVERTISED_40000baseKR4_Full
/* these defines were all added in one commit, so should be safe
 * to trigger activiation on one define
 */
#define SUPPORTED_40000baseKR4_Full	(1 << 23)
#define SUPPORTED_40000baseCR4_Full	(1 << 24)
#define SUPPORTED_40000baseSR4_Full	(1 << 25)
#define SUPPORTED_40000baseLR4_Full	(1 << 26)
#define ADVERTISED_40000baseKR4_Full	(1 << 23)
#define ADVERTISED_40000baseCR4_Full	(1 << 24)
#define ADVERTISED_40000baseSR4_Full	(1 << 25)
#define ADVERTISED_40000baseLR4_Full	(1 << 26)
#endif

/**
 * mmd_eee_cap_to_ethtool_sup_t
 * @eee_cap: value of the MMD EEE Capability register
 *
 * A small helper function that translates MMD EEE Capability (3.20) bits
 * to ethtool supported settings.
 */
static inline u32 __kc_mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap)
{
	u32 supported = 0;

	if (eee_cap & MDIO_EEE_100TX)
		supported |= SUPPORTED_100baseT_Full;
	if (eee_cap & MDIO_EEE_1000T)
		supported |= SUPPORTED_1000baseT_Full;
	if (eee_cap & MDIO_EEE_10GT)
		supported |= SUPPORTED_10000baseT_Full;
	if (eee_cap & MDIO_EEE_1000KX)
		supported |= SUPPORTED_1000baseKX_Full;
	if (eee_cap & MDIO_EEE_10GKX4)
		supported |= SUPPORTED_10000baseKX4_Full;
	if (eee_cap & MDIO_EEE_10GKR)
		supported |= SUPPORTED_10000baseKR_Full;

	return supported;
}
#define mmd_eee_cap_to_ethtool_sup_t(eee_cap) \
	__kc_mmd_eee_cap_to_ethtool_sup_t(eee_cap)

/**
 * mmd_eee_adv_to_ethtool_adv_t
 * @eee_adv: value of the MMD EEE Advertisement/Link Partner Ability registers
 *
 * A small helper function that translates the MMD EEE Advertisement (7.60)
 * and MMD EEE Link Partner Ability (7.61) bits to ethtool advertisement
 * settings.
 */
static inline u32 __kc_mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv)
{
	u32 adv = 0;

	if (eee_adv & MDIO_EEE_100TX)
		adv |= ADVERTISED_100baseT_Full;
	if (eee_adv & MDIO_EEE_1000T)
		adv |= ADVERTISED_1000baseT_Full;
	if (eee_adv & MDIO_EEE_10GT)
		adv |= ADVERTISED_10000baseT_Full;
	if (eee_adv & MDIO_EEE_1000KX)
		adv |= ADVERTISED_1000baseKX_Full;
	if (eee_adv & MDIO_EEE_10GKX4)
		adv |= ADVERTISED_10000baseKX4_Full;
	if (eee_adv & MDIO_EEE_10GKR)
		adv |= ADVERTISED_10000baseKR_Full;

	return adv;
}
#define mmd_eee_adv_to_ethtool_adv_t(eee_adv) \
	__kc_mmd_eee_adv_to_ethtool_adv_t(eee_adv)

/**
 * ethtool_adv_to_mmd_eee_adv_t
 * @adv: the ethtool advertisement settings
 *
 * A small helper function that translates ethtool advertisement settings
 * to EEE advertisements for the MMD EEE Advertisement (7.60) and
 * MMD EEE Link Partner Ability (7.61) registers.
 */
static inline u16 __kc_ethtool_adv_to_mmd_eee_adv_t(u32 adv)
{
	u16 reg = 0;

	if (adv & ADVERTISED_100baseT_Full)
		reg |= MDIO_EEE_100TX;
	if (adv & ADVERTISED_1000baseT_Full)
		reg |= MDIO_EEE_1000T;
	if (adv & ADVERTISED_10000baseT_Full)
		reg |= MDIO_EEE_10GT;
	if (adv & ADVERTISED_1000baseKX_Full)
		reg |= MDIO_EEE_1000KX;
	if (adv & ADVERTISED_10000baseKX4_Full)
		reg |= MDIO_EEE_10GKX4;
	if (adv & ADVERTISED_10000baseKR_Full)
		reg |= MDIO_EEE_10GKR;

	return reg;
}
#define ethtool_adv_to_mmd_eee_adv_t(adv) \
	__kc_ethtool_adv_to_mmd_eee_adv_t(adv)

#ifndef pci_pcie_type
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) )
static inline u8 pci_pcie_type(struct pci_dev *pdev)
{
	int pos;
	u16 reg16;

	pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (!pos)
		BUG();
	pci_read_config_word(pdev, pos + PCI_EXP_FLAGS, &reg16);
	return (reg16 & PCI_EXP_FLAGS_TYPE) >> 4;
}
#else /* < 2.6.24 */
#define pci_pcie_type(x)	(x)->pcie_type
#endif /* < 2.6.24 */
#endif /* pci_pcie_type */

#define ptp_clock_register(caps, args...) ptp_clock_register(caps)

#ifndef PCI_EXP_LNKSTA2
int __kc_pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val);
#define pcie_capability_read_word(d,p,v) __kc_pcie_capability_read_word(d,p,v)
int __kc_pcie_capability_write_word(struct pci_dev *dev, int pos, u16 val);
#define pcie_capability_write_word(d,p,v) __kc_pcie_capability_write_word(d,p,v)
int __kc_pcie_capability_clear_and_set_word(struct pci_dev *dev, int pos,
					    u16 clear, u16 set);
#define pcie_capability_clear_and_set_word(d,p,c,s) \
	__kc_pcie_capability_clear_and_set_word(d,p,c,s)

#define PCI_EXP_LNKSTA2		50	/* Link Status 2 */

static inline int pcie_capability_clear_word(struct pci_dev *dev, int pos,
					     u16 clear)
{
	return __kc_pcie_capability_clear_and_set_word(dev, pos, clear, 0);
}
#endif /* !PCI_EXP_LNKSTA2 */

#if (SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0))
#define USE_CONST_DEV_UC_CHAR
#endif

#else /* >= 3.7.0 */
#define HAVE_CONST_STRUCT_PCI_ERROR_HANDLERS
#define USE_CONST_DEV_UC_CHAR
#endif /* >= 3.7.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0) )
#ifndef PCI_EXP_LNKCTL_ASPM_L0S
#define  PCI_EXP_LNKCTL_ASPM_L0S  0x01	/* L0s Enable */
#endif
#ifndef PCI_EXP_LNKCTL_ASPM_L1
#define  PCI_EXP_LNKCTL_ASPM_L1   0x02	/* L1 Enable */
#endif
#define HAVE_CONFIG_HOTPLUG
/* Reserved Ethernet Addresses per IEEE 802.1Q */
static const u8 eth_reserved_addr_base[ETH_ALEN] __aligned(2) = {
	0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };
#if !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0)) &&\
    !(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,5))
static inline bool is_link_local_ether_addr(const u8 *addr)
{
	__be16 *a = (__be16 *)addr;
	static const __be16 *b = (const __be16 *)eth_reserved_addr_base;
	static const __be16 m = cpu_to_be16(0xfff0);

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | ((a[2] ^ b[2]) & m)) == 0;
}
#endif /* !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0)) */
#else /* >= 3.8.0 */
#ifndef __devinit
#define __devinit
#define HAVE_ENCAP_CSUM_OFFLOAD
#endif

#ifndef __devinitdata
#define __devinitdata
#endif

#ifndef __devexit
#define __devexit
#endif

#ifndef __devexit_p
#define __devexit_p
#endif

#ifndef HAVE_SRIOV_CONFIGURE
#define HAVE_SRIOV_CONFIGURE
#endif

#define HAVE_BRIDGE_ATTRIBS
#ifndef BRIDGE_MODE_VEB
#define BRIDGE_MODE_VEB		0	/* Default loopback mode */
#endif /* BRIDGE_MODE_VEB */
#ifndef BRIDGE_MODE_VEPA
#define BRIDGE_MODE_VEPA	1	/* 802.1Qbg defined VEPA mode */
#endif /* BRIDGE_MODE_VEPA */
#endif /* >= 3.8.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) )

#undef hlist_entry
#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#undef hlist_entry_safe
#define hlist_entry_safe(ptr, type, member) \
	(ptr) ? hlist_entry(ptr, type, member) : NULL

#undef hlist_for_each_entry
#define hlist_for_each_entry(pos, head, member)                             \
	for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); \
	     pos;                                                           \
	     pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#undef hlist_for_each_entry_safe
#define hlist_for_each_entry_safe(pos, n, head, member)		    \
	for (pos = hlist_entry_safe((head)->first, typeof(*pos), member);   \
	     pos && ({ n = pos->member.next; 1; });			    \
	     pos = hlist_entry_safe(n, typeof(*pos), member))

#ifdef CONFIG_XPS
extern int __kc_netif_set_xps_queue(struct net_device *, struct cpumask *, u16);
#define netif_set_xps_queue(_dev, _mask, _idx) __kc_netif_set_xps_queue((_dev), (_mask), (_idx))
#else /* CONFIG_XPS */
#define netif_set_xps_queue(_dev, _mask, _idx) do {} while (0)
#endif /* CONFIG_XPS */

#ifdef HAVE_NETDEV_SELECT_QUEUE
#define _kc_hashrnd 0xd631614b /* not so random hash salt */
extern u16 __kc_netdev_pick_tx(struct net_device *dev, struct sk_buff *skb);
#define __netdev_pick_tx __kc_netdev_pick_tx
#endif /* HAVE_NETDEV_SELECT_QUEUE */
#else
#define HAVE_BRIDGE_FILTER
#define USE_DEFAULT_FDB_DEL_DUMP
#endif /* < 3.9.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) )
#ifdef CONFIG_PCI_IOV
extern int __kc_pci_vfs_assigned(struct pci_dev *dev);
#else
static inline int __kc_pci_vfs_assigned(struct pci_dev *dev)
{
	return 0;
}
#endif
#define pci_vfs_assigned(dev) __kc_pci_vfs_assigned(dev)

#ifndef VLAN_TX_COOKIE_MAGIC
static inline struct sk_buff *__kc__vlan_hwaccel_put_tag(struct sk_buff *skb,
							 u16 vlan_tci)
{
#ifdef VLAN_TAG_PRESENT
	vlan_tci |= VLAN_TAG_PRESENT;
#endif
	skb->vlan_tci = vlan_tci;
        return skb;
}
#define __vlan_hwaccel_put_tag(skb, vlan_proto, vlan_tci) \
	__kc__vlan_hwaccel_put_tag(skb, vlan_tci)
#endif

#else /* >= 3.10.0 */
#define HAVE_ENCAP_TSO_OFFLOAD
#endif /* >= 3.10.0 */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0) )
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,6)))
#if (!(UBUNTU_KERNEL_CODE >= UBUNTU_KERNEL_VERSION(3,13,0,30,0) \
    && (UBUNTU_RELEASE_CODE == UBUNTU_RELEASE_VERSION(12,4) \
     || UBUNTU_RELEASE_CODE == UBUNTU_RELEASE_VERSION(14,4))))
#if (!(SLE_VERSION_CODE == SLE_VERSION(12,0,0)))
#ifdef NETIF_F_RXHASH
#define PKT_HASH_TYPE_L3 0
static inline void
skb_set_hash(struct sk_buff *skb, __u32 hash, __always_unused int type)
{
	skb->rxhash = hash;
}
#endif /* NETIF_F_RXHASH */
#endif /* < SLES12 */
#endif /* < 3.13.0-30.54 (Ubuntu 14.04) */
#endif /* < RHEL7 */
#endif /* < 3.14.0 */

#if (( LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0) ) \
    || ( RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,2) ))
#undef SET_ETHTOOL_OPS
#define SET_ETHTOOL_OPS(netdev, ops) ((netdev)->ethtool_ops = (ops))
#define HAVE_VF_MIN_MAX_TXRATE 1
#endif /* >= 3.16.0 */

#if (( LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) ) \
    || ( RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,2) ))
#define HAVE_NDO_DFLT_BRIDGE_ADD_MASK
#if ( RHEL_RELEASE_CODE != RHEL_RELEASE_VERSION(7,2) )
#define HAVE_NDO_FDB_ADD_VID
#endif /* !RHEL 7.2 */
#endif /* >= 3.19.0 */

#if (( LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0) ) \
    || ( RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,2) ))
/* vlan_tx_xx functions got renamed to skb_vlan */
#define vlan_tx_tag_get skb_vlan_tag_get
#define vlan_tx_tag_present skb_vlan_tag_present
#if ( RHEL_RELEASE_CODE != RHEL_RELEASE_VERSION(7,2) )
#define HAVE_NDO_BRIDGE_SET_DEL_LINK_FLAGS
#endif /* !RHEL 7.2 */
#endif /* 4.0.0 */

#if (( LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0) ) \
    || ( RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,3) ))
/* ndo_bridge_getlink adds new nlflags parameter */
#define HAVE_NDO_BRIDGE_GETLINK_NLFLAGS
#endif /* >= 4.1.0 */

#if (( LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0) ) \
    || ( RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,4) ))
/* ndo_bridge_getlink adds new filter_mask and vlan_fill parameters */
#define HAVE_NDO_BRIDGE_GETLINK_FILTER_MASK_VLAN_FILL
#endif /* >= 4.2.0 */

/*
 * vlan_tx_tag_* macros renamed to skb_vlan_tag_* (Linux commit: df8a39defad4)
 * For older kernels backported this commit, need to use renamed functions.
 * This fix is specific to RedHat/CentOS kernels.
 */
#if (defined(RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6, 8)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34)))
#define vlan_tx_tag_get skb_vlan_tag_get
#define vlan_tx_tag_present skb_vlan_tag_present
#endif

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)) || \
     (SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(12, 3, 0)) || \
     (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 4)))
#define HAVE_VF_VLAN_PROTO
#if (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 4))
/* In RHEL/Centos 7.4, the "new" version of ndo_set_vf_vlan
 * is in the struct net_device_ops_extended */
#define ndo_set_vf_vlan extended.ndo_set_vf_vlan
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#define HAVE_PCI_ENABLE_MSIX
#endif

#if defined(timer_setup) && defined(from_timer)
#define HAVE_TIMER_SETUP
#endif

#endif /* _KCOMPAT_H_ */
