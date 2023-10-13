/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CTRL_H__
#define __NFP_CTRL_H__

#include <stdint.h>

#include <ethdev_driver.h>

/*
 * Configuration BAR size.
 *
 * On the NFP6000, due to THB-350, the configuration BAR is 32K in size.
 */
#define NFP_NET_CFG_BAR_SZ              (32 * 1024)

/* Offset in Freelist buffer where packet starts on RX */
#define NFP_NET_RX_OFFSET               32

/* Working with metadata api (NFD version > 3.0) */
#define NFP_NET_META_FIELD_SIZE         4
#define NFP_NET_META_FIELD_MASK ((1 << NFP_NET_META_FIELD_SIZE) - 1)
#define NFP_NET_META_HEADER_SIZE        4
#define NFP_NET_META_NFDK_LENGTH        8

/* Working with metadata vlan api (NFD version >= 2.0) */
#define NFP_NET_META_VLAN_INFO          16
#define NFP_NET_META_VLAN_OFFLOAD       31
#define NFP_NET_META_VLAN_TPID          3
#define NFP_NET_META_VLAN_MASK          ((1 << NFP_NET_META_VLAN_INFO) - 1)
#define NFP_NET_META_VLAN_TPID_MASK     ((1 << NFP_NET_META_VLAN_TPID) - 1)
#define NFP_NET_META_TPID(d)            (((d) >> NFP_NET_META_VLAN_INFO) & \
						NFP_NET_META_VLAN_TPID_MASK)

/* Prepend field types */
#define NFP_NET_META_HASH               1 /* Next field carries hash type */
#define NFP_NET_META_VLAN               4
#define NFP_NET_META_PORTID             5
#define NFP_NET_META_IPSEC              9

#define NFP_META_PORT_ID_CTRL           ~0U

/* Hash type prepended when a RSS hash was computed */
#define NFP_NET_RSS_NONE                0
#define NFP_NET_RSS_IPV4                1
#define NFP_NET_RSS_IPV6                2
#define NFP_NET_RSS_IPV6_EX             3
#define NFP_NET_RSS_IPV4_TCP            4
#define NFP_NET_RSS_IPV6_TCP            5
#define NFP_NET_RSS_IPV6_EX_TCP         6
#define NFP_NET_RSS_IPV4_UDP            7
#define NFP_NET_RSS_IPV6_UDP            8
#define NFP_NET_RSS_IPV6_EX_UDP         9
#define NFP_NET_RSS_IPV4_SCTP           10
#define NFP_NET_RSS_IPV6_SCTP           11

/*
 * @NFP_NET_TXR_MAX:         Maximum number of TX rings
 * @NFP_NET_TXR_MASK:        Mask for TX rings
 * @NFP_NET_RXR_MAX:         Maximum number of RX rings
 * @NFP_NET_RXR_MASK:        Mask for RX rings
 */
#define NFP_NET_TXR_MAX                 64
#define NFP_NET_TXR_MASK                (NFP_NET_TXR_MAX - 1)
#define NFP_NET_RXR_MAX                 64
#define NFP_NET_RXR_MASK                (NFP_NET_RXR_MAX - 1)

/*
 * Read/Write config words (0x0000 - 0x002c)
 * @NFP_NET_CFG_CTRL:        Global control
 * @NFP_NET_CFG_UPDATE:      Indicate which fields are updated
 * @NFP_NET_CFG_TXRS_ENABLE: Bitmask of enabled TX rings
 * @NFP_NET_CFG_RXRS_ENABLE: Bitmask of enabled RX rings
 * @NFP_NET_CFG_MTU:         Set MTU size
 * @NFP_NET_CFG_FLBUFSZ:     Set freelist buffer size (must be larger than MTU)
 * @NFP_NET_CFG_EXN:         MSI-X table entry for exceptions
 * @NFP_NET_CFG_LSC:         MSI-X table entry for link state changes
 * @NFP_NET_CFG_MACADDR:     MAC address
 *
 * TODO:
 * - define Error details in UPDATE
 */
#define NFP_NET_CFG_CTRL                0x0000
#define   NFP_NET_CFG_CTRL_ENABLE         (0x1 <<  0) /* Global enable */
#define   NFP_NET_CFG_CTRL_PROMISC        (0x1 <<  1) /* Enable Promisc mode */
#define   NFP_NET_CFG_CTRL_L2BC           (0x1 <<  2) /* Allow L2 Broadcast */
#define   NFP_NET_CFG_CTRL_L2MC           (0x1 <<  3) /* Allow L2 Multicast */
#define   NFP_NET_CFG_CTRL_RXCSUM         (0x1 <<  4) /* Enable RX Checksum */
#define   NFP_NET_CFG_CTRL_TXCSUM         (0x1 <<  5) /* Enable TX Checksum */
#define   NFP_NET_CFG_CTRL_RXVLAN         (0x1 <<  6) /* Enable VLAN strip */
#define   NFP_NET_CFG_CTRL_TXVLAN         (0x1 <<  7) /* Enable VLAN insert */
#define   NFP_NET_CFG_CTRL_SCATTER        (0x1 <<  8) /* Scatter DMA */
#define   NFP_NET_CFG_CTRL_GATHER         (0x1 <<  9) /* Gather DMA */
#define   NFP_NET_CFG_CTRL_LSO            (0x1 << 10) /* LSO/TSO */
#define   NFP_NET_CFG_CTRL_RXQINQ         (0x1 << 13) /* Enable QINQ strip */
#define   NFP_NET_CFG_CTRL_RXVLAN_V2      (0x1 << 15) /* Enable VLAN strip with metadata */
#define   NFP_NET_CFG_CTRL_RINGCFG        (0x1 << 16) /* Ring runtime changes */
#define   NFP_NET_CFG_CTRL_RSS            (0x1 << 17) /* RSS */
#define   NFP_NET_CFG_CTRL_IRQMOD         (0x1 << 18) /* Interrupt moderation */
#define   NFP_NET_CFG_CTRL_RINGPRIO       (0x1 << 19) /* Ring priorities */
#define   NFP_NET_CFG_CTRL_MSIXAUTO       (0x1 << 20) /* MSI-X auto-masking */
#define   NFP_NET_CFG_CTRL_TXRWB          (0x1 << 21) /* Write-back of TX ring */
#define   NFP_NET_CFG_CTRL_L2SWITCH       (0x1 << 22) /* L2 Switch */
#define   NFP_NET_CFG_CTRL_TXVLAN_V2      (0x1 << 23) /* Enable VLAN insert with metadata */
#define   NFP_NET_CFG_CTRL_VXLAN          (0x1 << 24) /* Enable VXLAN */
#define   NFP_NET_CFG_CTRL_NVGRE          (0x1 << 25) /* Enable NVGRE */
#define   NFP_NET_CFG_CTRL_MSIX_TX_OFF    (0x1 << 26) /* Disable MSIX for TX */
#define   NFP_NET_CFG_CTRL_LSO2           (0x1 << 28) /* LSO/TSO (version 2) */
#define   NFP_NET_CFG_CTRL_RSS2           (0x1 << 29) /* RSS (version 2) */
#define   NFP_NET_CFG_CTRL_CSUM_COMPLETE  (0x1 << 30) /* Checksum complete */
#define   NFP_NET_CFG_CTRL_LIVE_ADDR      (0x1U << 31) /* Live MAC addr change */
#define NFP_NET_CFG_UPDATE              0x0004
#define   NFP_NET_CFG_UPDATE_GEN          (0x1 <<  0) /* General update */
#define   NFP_NET_CFG_UPDATE_RING         (0x1 <<  1) /* Ring config change */
#define   NFP_NET_CFG_UPDATE_RSS          (0x1 <<  2) /* RSS config change */
#define   NFP_NET_CFG_UPDATE_TXRPRIO      (0x1 <<  3) /* TX Ring prio change */
#define   NFP_NET_CFG_UPDATE_RXRPRIO      (0x1 <<  4) /* RX Ring prio change */
#define   NFP_NET_CFG_UPDATE_MSIX         (0x1 <<  5) /* MSI-X change */
#define   NFP_NET_CFG_UPDATE_L2SWITCH     (0x1 <<  6) /* Switch changes */
#define   NFP_NET_CFG_UPDATE_RESET        (0x1 <<  7) /* Update due to FLR */
#define   NFP_NET_CFG_UPDATE_IRQMOD       (0x1 <<  8) /* IRQ mod change */
#define   NFP_NET_CFG_UPDATE_VXLAN        (0x1 <<  9) /* VXLAN port change */
#define   NFP_NET_CFG_UPDATE_MACADDR      (0x1 << 11) /* MAC address change */
#define   NFP_NET_CFG_UPDATE_MBOX         (0x1 << 12) /* Mailbox update */
#define   NFP_NET_CFG_UPDATE_ERR          (0x1U << 31) /* A error occurred */
#define NFP_NET_CFG_TXRS_ENABLE         0x0008
#define NFP_NET_CFG_RXRS_ENABLE         0x0010
#define NFP_NET_CFG_MTU                 0x0018
#define NFP_NET_CFG_FLBUFSZ             0x001c
#define NFP_NET_CFG_EXN                 0x001f
#define NFP_NET_CFG_LSC                 0x0020
#define NFP_NET_CFG_MACADDR             0x0024

#define NFP_NET_CFG_CTRL_LSO_ANY (NFP_NET_CFG_CTRL_LSO | NFP_NET_CFG_CTRL_LSO2)
#define NFP_NET_CFG_CTRL_RSS_ANY (NFP_NET_CFG_CTRL_RSS | NFP_NET_CFG_CTRL_RSS2)

#define NFP_NET_CFG_CTRL_CHAIN_META (NFP_NET_CFG_CTRL_RSS2 | \
					NFP_NET_CFG_CTRL_CSUM_COMPLETE)

/* Version number helper defines */
struct nfp_net_fw_ver {
	uint8_t minor;
	uint8_t major;
	uint8_t class;
	/**
	 * This byte can be extended for more use.
	 * BIT0: NFD dp type, refer NFP_NET_CFG_VERSION_DP_NFDx
	 * BIT[7:1]: reserved
	 */
	uint8_t extend;
};

/*
 * Read-only words (0x0030 - 0x0050):
 * @NFP_NET_CFG_VERSION:     Firmware version number
 * @NFP_NET_CFG_STS:         Status
 * @NFP_NET_CFG_CAP:         Capabilities (same bits as @NFP_NET_CFG_CTRL)
 * @NFP_NET_MAX_TXRINGS:     Maximum number of TX rings
 * @NFP_NET_MAX_RXRINGS:     Maximum number of RX rings
 * @NFP_NET_MAX_MTU:         Maximum support MTU
 * @NFP_NET_CFG_START_TXQ:   Start Queue Control Queue to use for TX (PF only)
 * @NFP_NET_CFG_START_RXQ:   Start Queue Control Queue to use for RX (PF only)
 *
 * TODO:
 * - define more STS bits
 */
#define NFP_NET_CFG_VERSION             0x0030
#define   NFP_NET_CFG_VERSION_DP_NFD3   0
#define   NFP_NET_CFG_VERSION_DP_NFDK   1
#define NFP_NET_CFG_STS                 0x0034
#define   NFP_NET_CFG_STS_LINK            (0x1 << 0) /* Link up or down */
/* Link rate */
#define   NFP_NET_CFG_STS_LINK_RATE_SHIFT 1
#define   NFP_NET_CFG_STS_LINK_RATE_MASK  0xF
#define   NFP_NET_CFG_STS_LINK_RATE_UNSUPPORTED   0
#define   NFP_NET_CFG_STS_LINK_RATE_UNKNOWN       1
#define   NFP_NET_CFG_STS_LINK_RATE_1G            2
#define   NFP_NET_CFG_STS_LINK_RATE_10G           3
#define   NFP_NET_CFG_STS_LINK_RATE_25G           4
#define   NFP_NET_CFG_STS_LINK_RATE_40G           5
#define   NFP_NET_CFG_STS_LINK_RATE_50G           6
#define   NFP_NET_CFG_STS_LINK_RATE_100G          7

/*
 * NSP Link rate is a 16-bit word. It is no longer determined by
 * firmware, instead it is read from the nfp_eth_table of the
 * associated pf_dev and written to the NFP_NET_CFG_STS_NSP_LINK_RATE
 * address by the PMD each time the port is reconfigured.
 */
#define NFP_NET_CFG_STS_NSP_LINK_RATE   0x0036

#define NFP_NET_CFG_CAP                 0x0038
#define NFP_NET_CFG_MAX_TXRINGS         0x003c
#define NFP_NET_CFG_MAX_RXRINGS         0x0040
#define NFP_NET_CFG_MAX_MTU             0x0044
/* Next two words are being used by VFs for solving THB350 issue */
#define NFP_NET_CFG_START_TXQ           0x0048
#define NFP_NET_CFG_START_RXQ           0x004c

/*
 * NFP-3200 workaround (0x0050 - 0x0058)
 * @NFP_NET_CFG_SPARE_ADDR:  DMA address for ME code to use (e.g. YDS-155 fix)
 */
#define NFP_NET_CFG_SPARE_ADDR          0x0050
/*
 * NFP6000/NFP4000 - Prepend configuration
 */
#define NFP_NET_CFG_RX_OFFSET           0x0050
#define NFP_NET_CFG_RX_OFFSET_DYNAMIC          0    /* Prepend mode */

/* Start anchor of the TLV area */
#define NFP_NET_CFG_TLV_BASE            0x0058

/**
 * Reuse spare address to contain the offset from the start of
 * the host buffer where the first byte of the received frame
 * will land.  Any metadata will come prior to that offset.  If the
 * value in this field is 0, it means that the metadata will
 * always land starting at the first byte of the host buffer and
 * packet data will immediately follow the metadata.  As always,
 * the RX descriptor indicates the presence or absence of metadata
 * along with the length thereof.
 */
#define NFP_NET_CFG_RX_OFFSET_ADDR      0x0050

#define NFP_NET_CFG_VXLAN_PORT          0x0060
#define NFP_NET_CFG_VXLAN_SZ            0x0008

/* Offload definitions */
#define NFP_NET_N_VXLAN_PORTS  (NFP_NET_CFG_VXLAN_SZ / sizeof(uint16_t))

/*
 * 3 words reserved for extended ctrl words (0x0098 - 0x00a4)
 * 3 words reserved for extended cap words (0x00a4 - 0x00b0)
 * Currently only one word is used, can be extended in future.
 */
#define NFP_NET_CFG_CTRL_WORD1          0x0098
#define NFP_NET_CFG_CTRL_PKT_TYPE         (0x1 << 0)
#define NFP_NET_CFG_CTRL_IPSEC            (0x1 << 1) /**< IPsec offload */
#define NFP_NET_CFG_CTRL_IPSEC_SM_LOOKUP  (0x1 << 3) /**< SA short match lookup */
#define NFP_NET_CFG_CTRL_IPSEC_LM_LOOKUP  (0x1 << 4) /**< SA long match lookup */

#define NFP_NET_CFG_CAP_WORD1           0x00a4

/* 16B reserved for future use (0x00b0 - 0x00c0). */
#define NFP_NET_CFG_RESERVED            0x00b0
#define NFP_NET_CFG_RESERVED_SZ         0x0010

/*
 * RSS configuration (0x0100 - 0x01ac):
 * Used only when NFP_NET_CFG_CTRL_RSS_ANY is enabled
 * @NFP_NET_CFG_RSS_CFG:     RSS configuration word
 * @NFP_NET_CFG_RSS_KEY:     RSS "secret" key
 * @NFP_NET_CFG_RSS_ITBL:    RSS indirection table
 */
#define NFP_NET_CFG_RSS_BASE            0x0100
#define NFP_NET_CFG_RSS_CTRL            NFP_NET_CFG_RSS_BASE
#define   NFP_NET_CFG_RSS_MASK            (0x7f)
#define   NFP_NET_CFG_RSS_MASK_of(_x)     ((_x) & 0x7f)
#define   NFP_NET_CFG_RSS_IPV4            (1 <<  8) /* RSS for IPv4 */
#define   NFP_NET_CFG_RSS_IPV6            (1 <<  9) /* RSS for IPv6 */
#define   NFP_NET_CFG_RSS_IPV4_TCP        (1 << 10) /* RSS for IPv4/TCP */
#define   NFP_NET_CFG_RSS_IPV4_UDP        (1 << 11) /* RSS for IPv4/UDP */
#define   NFP_NET_CFG_RSS_IPV6_TCP        (1 << 12) /* RSS for IPv6/TCP */
#define   NFP_NET_CFG_RSS_IPV6_UDP        (1 << 13) /* RSS for IPv6/UDP */
#define   NFP_NET_CFG_RSS_IPV4_SCTP       (1 << 14) /* RSS for IPv4/SCTP */
#define   NFP_NET_CFG_RSS_IPV6_SCTP       (1 << 15) /* RSS for IPv6/SCTP */
#define   NFP_NET_CFG_RSS_TOEPLITZ        (1 << 24) /* Use Toeplitz hash */
#define NFP_NET_CFG_RSS_KEY             (NFP_NET_CFG_RSS_BASE + 0x4)
#define NFP_NET_CFG_RSS_KEY_SZ          0x28
#define NFP_NET_CFG_RSS_ITBL            (NFP_NET_CFG_RSS_BASE + 0x4 + \
					 NFP_NET_CFG_RSS_KEY_SZ)
#define NFP_NET_CFG_RSS_ITBL_SZ         0x80

/*
 * TX ring configuration (0x200 - 0x800)
 * @NFP_NET_CFG_TXR_BASE:    Base offset for TX ring configuration
 * @NFP_NET_CFG_TXR_ADDR:    Per TX ring DMA address (8B entries)
 * @NFP_NET_CFG_TXR_WB_ADDR: Per TX ring write back DMA address (8B entries)
 * @NFP_NET_CFG_TXR_SZ:      Per TX ring size (1B entries)
 * @NFP_NET_CFG_TXR_VEC:     Per TX ring MSI-X table entry (1B entries)
 * @NFP_NET_CFG_TXR_PRIO:    Per TX ring priority (1B entries)
 * @NFP_NET_CFG_TXR_IRQ_MOD: Per TX ring interrupt moderation (4B entries)
 */
#define NFP_NET_CFG_TXR_BASE            0x0200
#define NFP_NET_CFG_TXR_ADDR(_x)        (NFP_NET_CFG_TXR_BASE + ((_x) * 0x8))
#define NFP_NET_CFG_TXR_WB_ADDR(_x)     (NFP_NET_CFG_TXR_BASE + 0x200 + \
					 ((_x) * 0x8))
#define NFP_NET_CFG_TXR_SZ(_x)          (NFP_NET_CFG_TXR_BASE + 0x400 + (_x))
#define NFP_NET_CFG_TXR_VEC(_x)         (NFP_NET_CFG_TXR_BASE + 0x440 + (_x))
#define NFP_NET_CFG_TXR_PRIO(_x)        (NFP_NET_CFG_TXR_BASE + 0x480 + (_x))
#define NFP_NET_CFG_TXR_IRQ_MOD(_x)     (NFP_NET_CFG_TXR_BASE + 0x500 + \
					 ((_x) * 0x4))

/*
 * RX ring configuration (0x0800 - 0x0c00)
 * @NFP_NET_CFG_RXR_BASE:    Base offset for RX ring configuration
 * @NFP_NET_CFG_RXR_ADDR:    Per TX ring DMA address (8B entries)
 * @NFP_NET_CFG_RXR_SZ:      Per TX ring size (1B entries)
 * @NFP_NET_CFG_RXR_VEC:     Per TX ring MSI-X table entry (1B entries)
 * @NFP_NET_CFG_RXR_PRIO:    Per TX ring priority (1B entries)
 * @NFP_NET_CFG_RXR_IRQ_MOD: Per TX ring interrupt moderation (4B entries)
 */
#define NFP_NET_CFG_RXR_BASE            0x0800
#define NFP_NET_CFG_RXR_ADDR(_x)        (NFP_NET_CFG_RXR_BASE + ((_x) * 0x8))
#define NFP_NET_CFG_RXR_SZ(_x)          (NFP_NET_CFG_RXR_BASE + 0x200 + (_x))
#define NFP_NET_CFG_RXR_VEC(_x)         (NFP_NET_CFG_RXR_BASE + 0x240 + (_x))
#define NFP_NET_CFG_RXR_PRIO(_x)        (NFP_NET_CFG_RXR_BASE + 0x280 + (_x))
#define NFP_NET_CFG_RXR_IRQ_MOD(_x)     (NFP_NET_CFG_RXR_BASE + 0x300 + \
					 ((_x) * 0x4))

/*
 * Interrupt Control/Cause registers (0x0c00 - 0x0d00)
 * These registers are only used when MSI-X auto-masking is not
 * enabled (@NFP_NET_CFG_CTRL_MSIXAUTO not set).  The array is index
 * by MSI-X entry and are 1B in size.  If an entry is zero, the
 * corresponding entry is enabled.  If the FW generates an interrupt,
 * it writes a cause into the corresponding field.  This also masks
 * the MSI-X entry and the host driver must clear the register to
 * re-enable the interrupt.
 */
#define NFP_NET_CFG_ICR_BASE            0x0c00
#define NFP_NET_CFG_ICR(_x)             (NFP_NET_CFG_ICR_BASE + (_x))
#define   NFP_NET_CFG_ICR_UNMASKED      0x0
#define   NFP_NET_CFG_ICR_RXTX          0x1
#define   NFP_NET_CFG_ICR_LSC           0x2

/*
 * General device stats (0x0d00 - 0x0d90)
 * All counters are 64bit.
 */
#define NFP_NET_CFG_STATS_BASE          0x0d00
#define NFP_NET_CFG_STATS_RX_DISCARDS   (NFP_NET_CFG_STATS_BASE + 0x00)
#define NFP_NET_CFG_STATS_RX_ERRORS     (NFP_NET_CFG_STATS_BASE + 0x08)
#define NFP_NET_CFG_STATS_RX_OCTETS     (NFP_NET_CFG_STATS_BASE + 0x10)
#define NFP_NET_CFG_STATS_RX_UC_OCTETS  (NFP_NET_CFG_STATS_BASE + 0x18)
#define NFP_NET_CFG_STATS_RX_MC_OCTETS  (NFP_NET_CFG_STATS_BASE + 0x20)
#define NFP_NET_CFG_STATS_RX_BC_OCTETS  (NFP_NET_CFG_STATS_BASE + 0x28)
#define NFP_NET_CFG_STATS_RX_FRAMES     (NFP_NET_CFG_STATS_BASE + 0x30)
#define NFP_NET_CFG_STATS_RX_MC_FRAMES  (NFP_NET_CFG_STATS_BASE + 0x38)
#define NFP_NET_CFG_STATS_RX_BC_FRAMES  (NFP_NET_CFG_STATS_BASE + 0x40)

#define NFP_NET_CFG_STATS_TX_DISCARDS   (NFP_NET_CFG_STATS_BASE + 0x48)
#define NFP_NET_CFG_STATS_TX_ERRORS     (NFP_NET_CFG_STATS_BASE + 0x50)
#define NFP_NET_CFG_STATS_TX_OCTETS     (NFP_NET_CFG_STATS_BASE + 0x58)
#define NFP_NET_CFG_STATS_TX_UC_OCTETS  (NFP_NET_CFG_STATS_BASE + 0x60)
#define NFP_NET_CFG_STATS_TX_MC_OCTETS  (NFP_NET_CFG_STATS_BASE + 0x68)
#define NFP_NET_CFG_STATS_TX_BC_OCTETS  (NFP_NET_CFG_STATS_BASE + 0x70)
#define NFP_NET_CFG_STATS_TX_FRAMES     (NFP_NET_CFG_STATS_BASE + 0x78)
#define NFP_NET_CFG_STATS_TX_MC_FRAMES  (NFP_NET_CFG_STATS_BASE + 0x80)
#define NFP_NET_CFG_STATS_TX_BC_FRAMES  (NFP_NET_CFG_STATS_BASE + 0x88)

#define NFP_NET_CFG_STATS_APP0_FRAMES   (NFP_NET_CFG_STATS_BASE + 0x90)
#define NFP_NET_CFG_STATS_APP0_BYTES    (NFP_NET_CFG_STATS_BASE + 0x98)
#define NFP_NET_CFG_STATS_APP1_FRAMES   (NFP_NET_CFG_STATS_BASE + 0xa0)
#define NFP_NET_CFG_STATS_APP1_BYTES    (NFP_NET_CFG_STATS_BASE + 0xa8)
#define NFP_NET_CFG_STATS_APP2_FRAMES   (NFP_NET_CFG_STATS_BASE + 0xb0)
#define NFP_NET_CFG_STATS_APP2_BYTES    (NFP_NET_CFG_STATS_BASE + 0xb8)
#define NFP_NET_CFG_STATS_APP3_FRAMES   (NFP_NET_CFG_STATS_BASE + 0xc0)
#define NFP_NET_CFG_STATS_APP3_BYTES    (NFP_NET_CFG_STATS_BASE + 0xc8)

/*
 * Per ring stats (0x1000 - 0x1800)
 * Options, 64bit per entry
 * @NFP_NET_CFG_TXR_STATS:   TX ring statistics (Packet and Byte count)
 * @NFP_NET_CFG_RXR_STATS:   RX ring statistics (Packet and Byte count)
 */
#define NFP_NET_CFG_TXR_STATS_BASE      0x1000
#define NFP_NET_CFG_TXR_STATS(_x)       (NFP_NET_CFG_TXR_STATS_BASE + \
					 ((_x) * 0x10))
#define NFP_NET_CFG_RXR_STATS_BASE      0x1400
#define NFP_NET_CFG_RXR_STATS(_x)       (NFP_NET_CFG_RXR_STATS_BASE + \
					 ((_x) * 0x10))

/*
 * Mac stats (0x0000 - 0x0200)
 * All counters are 64bit.
 */
#define NFP_MAC_STATS_BASE                0x0000
#define NFP_MAC_STATS_SIZE                0x0200

#define NFP_MAC_STATS_RX_IN_OCTS                (NFP_MAC_STATS_BASE + 0x000)
#define NFP_MAC_STATS_RX_FRAME_TOO_LONG_ERRORS  (NFP_MAC_STATS_BASE + 0x010)
#define NFP_MAC_STATS_RX_RANGE_LENGTH_ERRORS    (NFP_MAC_STATS_BASE + 0x018)
#define NFP_MAC_STATS_RX_VLAN_RECEIVED_OK       (NFP_MAC_STATS_BASE + 0x020)
#define NFP_MAC_STATS_RX_IN_ERRORS              (NFP_MAC_STATS_BASE + 0x028)
#define NFP_MAC_STATS_RX_IN_BROADCAST_PKTS      (NFP_MAC_STATS_BASE + 0x030)
#define NFP_MAC_STATS_RX_DROP_EVENTS            (NFP_MAC_STATS_BASE + 0x038)
#define NFP_MAC_STATS_RX_ALIGNMENT_ERRORS       (NFP_MAC_STATS_BASE + 0x040)
#define NFP_MAC_STATS_RX_PAUSE_MAC_CTRL_FRAMES  (NFP_MAC_STATS_BASE + 0x048)
#define NFP_MAC_STATS_RX_FRAMES_RECEIVED_OK     (NFP_MAC_STATS_BASE + 0x050)
#define NFP_MAC_STATS_RX_FRAME_CHECK_SEQ_ERRORS (NFP_MAC_STATS_BASE + 0x058)
#define NFP_MAC_STATS_RX_UNICAST_PKTS           (NFP_MAC_STATS_BASE + 0x060)
#define NFP_MAC_STATS_RX_MULTICAST_PKTS         (NFP_MAC_STATS_BASE + 0x068)
#define NFP_MAC_STATS_RX_PKTS                   (NFP_MAC_STATS_BASE + 0x070)
#define NFP_MAC_STATS_RX_UNDERSIZE_PKTS         (NFP_MAC_STATS_BASE + 0x078)
#define NFP_MAC_STATS_RX_PKTS_64_OCTS           (NFP_MAC_STATS_BASE + 0x080)
#define NFP_MAC_STATS_RX_PKTS_65_TO_127_OCTS    (NFP_MAC_STATS_BASE + 0x088)
#define NFP_MAC_STATS_RX_PKTS_512_TO_1023_OCTS  (NFP_MAC_STATS_BASE + 0x090)
#define NFP_MAC_STATS_RX_PKTS_1024_TO_1518_OCTS (NFP_MAC_STATS_BASE + 0x098)
#define NFP_MAC_STATS_RX_JABBERS                (NFP_MAC_STATS_BASE + 0x0a0)
#define NFP_MAC_STATS_RX_FRAGMENTS              (NFP_MAC_STATS_BASE + 0x0a8)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS2    (NFP_MAC_STATS_BASE + 0x0b0)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS3    (NFP_MAC_STATS_BASE + 0x0b8)
#define NFP_MAC_STATS_RX_PKTS_128_TO_255_OCTS   (NFP_MAC_STATS_BASE + 0x0c0)
#define NFP_MAC_STATS_RX_PKTS_256_TO_511_OCTS   (NFP_MAC_STATS_BASE + 0x0c8)
#define NFP_MAC_STATS_RX_PKTS_1519_TO_MAX_OCTS  (NFP_MAC_STATS_BASE + 0x0d0)
#define NFP_MAC_STATS_RX_OVERSIZE_PKTS          (NFP_MAC_STATS_BASE + 0x0d8)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS0    (NFP_MAC_STATS_BASE + 0x0e0)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS1    (NFP_MAC_STATS_BASE + 0x0e8)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS4    (NFP_MAC_STATS_BASE + 0x0f0)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS5    (NFP_MAC_STATS_BASE + 0x0f8)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS6    (NFP_MAC_STATS_BASE + 0x100)
#define NFP_MAC_STATS_RX_PAUSE_FRAMES_CLASS7    (NFP_MAC_STATS_BASE + 0x108)
#define NFP_MAC_STATS_RX_MAC_CTRL_FRAMES_REC    (NFP_MAC_STATS_BASE + 0x110)
#define NFP_MAC_STATS_RX_MAC_HEAD_DROP          (NFP_MAC_STATS_BASE + 0x118)
#define NFP_MAC_STATS_TX_QUEUE_DROP             (NFP_MAC_STATS_BASE + 0x138)
#define NFP_MAC_STATS_TX_OUT_OCTS               (NFP_MAC_STATS_BASE + 0x140)
#define NFP_MAC_STATS_TX_VLAN_TRANSMITTED_OK    (NFP_MAC_STATS_BASE + 0x150)
#define NFP_MAC_STATS_TX_OUT_ERRORS             (NFP_MAC_STATS_BASE + 0x158)
#define NFP_MAC_STATS_TX_BROADCAST_PKTS         (NFP_MAC_STATS_BASE + 0x160)
#define NFP_MAC_STATS_TX_PKTS_64_OCTS           (NFP_MAC_STATS_BASE + 0x168)
#define NFP_MAC_STATS_TX_PKTS_256_TO_511_OCTS   (NFP_MAC_STATS_BASE + 0x170)
#define NFP_MAC_STATS_TX_PKTS_512_TO_1023_OCTS  (NFP_MAC_STATS_BASE + 0x178)
#define NFP_MAC_STATS_TX_PAUSE_MAC_CTRL_FRAMES  (NFP_MAC_STATS_BASE + 0x180)
#define NFP_MAC_STATS_TX_FRAMES_TRANSMITTED_OK  (NFP_MAC_STATS_BASE + 0x188)
#define NFP_MAC_STATS_TX_UNICAST_PKTS           (NFP_MAC_STATS_BASE + 0x190)
#define NFP_MAC_STATS_TX_MULTICAST_PKTS         (NFP_MAC_STATS_BASE + 0x198)
#define NFP_MAC_STATS_TX_PKTS_65_TO_127_OCTS    (NFP_MAC_STATS_BASE + 0x1a0)
#define NFP_MAC_STATS_TX_PKTS_128_TO_255_OCTS   (NFP_MAC_STATS_BASE + 0x1a8)
#define NFP_MAC_STATS_TX_PKTS_1024_TO_1518_OCTS (NFP_MAC_STATS_BASE + 0x1b0)
#define NFP_MAC_STATS_TX_PKTS_1519_TO_MAX_OCTS  (NFP_MAC_STATS_BASE + 0x1b8)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS0    (NFP_MAC_STATS_BASE + 0x1c0)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS1    (NFP_MAC_STATS_BASE + 0x1c8)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS4    (NFP_MAC_STATS_BASE + 0x1d0)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS5    (NFP_MAC_STATS_BASE + 0x1d8)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS2    (NFP_MAC_STATS_BASE + 0x1e0)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS3    (NFP_MAC_STATS_BASE + 0x1e8)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS6    (NFP_MAC_STATS_BASE + 0x1f0)
#define NFP_MAC_STATS_TX_PAUSE_FRAMES_CLASS7    (NFP_MAC_STATS_BASE + 0x1f8)

/*
 * General use mailbox area (0x1800 - 0x19ff)
 * 4B used for update command and 4B return code followed by
 * a max of 504B of variable length value.
 */
#define NFP_NET_CFG_MBOX_BASE                 0x1800
#define NFP_NET_CFG_MBOX_VAL                  0x1808
#define NFP_NET_CFG_MBOX_VAL_MAX_SZ           0x1F8
#define NFP_NET_CFG_MBOX_SIMPLE_CMD           0x0
#define NFP_NET_CFG_MBOX_SIMPLE_RET           0x4
#define NFP_NET_CFG_MBOX_SIMPLE_VAL           0x8

#define NFP_NET_CFG_MBOX_CMD_IPSEC            3

/*
 * TLV capabilities
 * @NFP_NET_CFG_TLV_TYPE:          Offset of type within the TLV
 * @NFP_NET_CFG_TLV_TYPE_REQUIRED: Driver must be able to parse the TLV
 * @NFP_NET_CFG_TLV_LENGTH:        Offset of length within the TLV
 * @NFP_NET_CFG_TLV_LENGTH_INC:    TLV length increments
 * @NFP_NET_CFG_TLV_VALUE:         Offset of value with the TLV
 * @NFP_NET_CFG_TLV_STATS_OFFSET:  Length of TLV stats offset
 *
 * List of simple TLV structures, first one starts at @NFP_NET_CFG_TLV_BASE.
 * Last structure must be of type @NFP_NET_CFG_TLV_TYPE_END. Presence of TLVs
 * is indicated by @NFP_NET_CFG_TLV_BASE being non-zero. TLV structures may
 * fill the entire remainder of the BAR or be shorter. FW must make sure TLVs
 * don't conflict with other features which allocate space beyond
 * @NFP_NET_CFG_TLV_BASE. @NFP_NET_CFG_TLV_TYPE_RESERVED should be used to wrap
 * space used by such features.
 *
 * Note that the 4 byte TLV header is not counted in %NFP_NET_CFG_TLV_LENGTH.
 */
#define NFP_NET_CFG_TLV_TYPE                  0x00
#define NFP_NET_CFG_TLV_TYPE_REQUIRED         0x8000
#define NFP_NET_CFG_TLV_LENGTH                0x02
#define NFP_NET_CFG_TLV_LENGTH_INC            4
#define NFP_NET_CFG_TLV_VALUE                 0x04
#define NFP_NET_CFG_TLV_STATS_OFFSET          0x08

#define NFP_NET_CFG_TLV_HEADER_REQUIRED       0x80000000
#define NFP_NET_CFG_TLV_HEADER_TYPE           0x7fff0000
#define NFP_NET_CFG_TLV_HEADER_LENGTH         0x0000ffff

/*
 * Capability TLV types
 *
 * @NFP_NET_CFG_TLV_TYPE_UNKNOWN:
 * Special TLV type to catch bugs, should never be encountered. Drivers should
 * treat encountering this type as error and refuse to probe.
 *
 * @NFP_NET_CFG_TLV_TYPE_RESERVED:
 * Reserved space, may contain legacy fixed-offset fields, or be used for
 * padding. The use of this type should be otherwise avoided.
 *
 * @NFP_NET_CFG_TLV_TYPE_END:
 * Empty, end of TLV list. Must be the last TLV. Drivers will stop processing
 * further TLVs when encountered.
 *
 * @NFP_NET_CFG_TLV_TYPE_ME_FREQ:
 * Single word, ME frequency in MHz as used in calculation for
 * @NFP_NET_CFG_RXR_IRQ_MOD and @NFP_NET_CFG_TXR_IRQ_MOD.
 *
 * @NFP_NET_CFG_TLV_TYPE_MBOX:
 * Variable, mailbox area. Overwrites the default location which is
 * @NFP_NET_CFG_MBOX_BASE and length @NFP_NET_CFG_MBOX_VAL_MAX_SZ.
 *
 * @NFP_NET_CFG_TLV_TYPE_EXPERIMENTAL0:
 * @NFP_NET_CFG_TLV_TYPE_EXPERIMENTAL1:
 * Variable, experimental IDs. IDs designated for internal development and
 * experiments before a stable TLV ID has been allocated to a feature. Should
 * never be present in production FW.
 *
 * @NFP_NET_CFG_TLV_TYPE_REPR_CAP:
 * Single word, equivalent of %NFP_NET_CFG_CAP for representors, features which
 * can be used on representors.
 *
 * @NFP_NET_CFG_TLV_TYPE_MBOX_CMSG_TYPES:
 * Variable, bitmap of control message types supported by the mailbox handler.
 * Bit 0 corresponds to message type 0, bit 1 to 1, etc. Control messages are
 * encapsulated into simple TLVs, with an end TLV and written to the Mailbox.
 *
 * @NFP_NET_CFG_TLV_TYPE_CRYPTO_OPS:
 * 8 words, bitmaps of supported and enabled crypto operations.
 * First 16B (4 words) contains a bitmap of supported crypto operations,
 * and next 16B contain the enabled operations.
 * This capability is obsoleted by ones with better sync methods.
 *
 * @NFP_NET_CFG_TLV_TYPE_VNIC_STATS:
 * Variable, per-vNIC statistics, data should be 8B aligned (FW should insert
 * zero-length RESERVED TLV to pad).
 * TLV data has two sections. First is an array of statistics' IDs (2B each).
 * Second 8B statistics themselves. Statistics are 8B aligned, meaning there
 * may be a padding between sections.
 * Number of statistics can be determined as floor(tlv.length / (2 + 8)).
 * This TLV overwrites %NFP_NET_CFG_STATS_* values (statistics in this TLV
 * duplicate the old ones, so driver should be careful not to unnecessarily
 * render both).
 *
 * @NFP_NET_CFG_TLV_TYPE_CRYPTO_OPS_RX_SCAN:
 * Same as %NFP_NET_CFG_TLV_TYPE_CRYPTO_OPS, but crypto TLS does stream scan
 * RX sync, rather than kernel-assisted sync.
 *
 * @NFP_NET_CFG_TLV_TYPE_CRYPTO_OPS_LENGTH:
 * CRYPTO OPS TLV should be at least 32B.
 */
#define NFP_NET_CFG_TLV_TYPE_UNKNOWN            0
#define NFP_NET_CFG_TLV_TYPE_RESERVED           1
#define NFP_NET_CFG_TLV_TYPE_END                2
#define NFP_NET_CFG_TLV_TYPE_MBOX               4
#define NFP_NET_CFG_TLV_TYPE_MBOX_CMSG_TYPES    10

int nfp_net_tlv_caps_parse(struct rte_eth_dev *dev);

/**
 * Get RSS flag based on firmware's capability
 *
 * @param hw_cap
 *   The firmware's capabilities
 */
static inline uint32_t
nfp_net_cfg_ctrl_rss(uint32_t hw_cap)
{
	if ((hw_cap & NFP_NET_CFG_CTRL_RSS2) != 0)
		return NFP_NET_CFG_CTRL_RSS2;

	return NFP_NET_CFG_CTRL_RSS;
}

#endif /* __NFP_CTRL_H__ */
