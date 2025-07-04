/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_COMMON_CTRL_H__
#define __NFP_COMMON_CTRL_H__

/*
 * Configuration BAR size.
 *
 * On the NFP6000, due to THB-350, the configuration BAR is 32K in size.
 */
#define NFP_NET_CFG_BAR_SZ_32K          (32 * 1024)
#define NFP_NET_CFG_BAR_SZ_8K           (8 * 1024)
#define NFP_NET_CFG_BAR_SZ_MIN          NFP_NET_CFG_BAR_SZ_8K

/*
 * Configuration sriov VF.
 * The configuration memory begins with a mailbox region for communication with
 * the firmware followed by individual VF entries.
 */
#define NFP_NET_VF_CFG_SZ               16
#define NFP_NET_VF_CFG_MB_SZ            16

/* VF config mailbox */
#define NFP_NET_VF_CFG_MB               0x0
#define NFP_NET_VF_CFG_MB_CAP           0x0
#define   NFP_NET_VF_CFG_MB_CAP_QUEUE_CONFIG      (0x1 << 7)
#define   NFP_NET_VF_CFG_MB_CAP_SPLIT             (0x1 << 8)
#define NFP_NET_VF_CFG_MB_RET           0x2
#define NFP_NET_VF_CFG_MB_UPD           0x4
#define   NFP_NET_VF_CFG_MB_UPD_QUEUE_CONFIG      (0x1 << 7)
#define   NFP_NET_VF_CFG_MB_UPD_SPLIT             (0x1 << 8)
#define NFP_NET_VF_CFG_MB_VF_CNT        0x6
#define NFP_NET_VF_CFG_MB_VF_NUM        0x7

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
#define   NFP_NET_CFG_UPDATE_VF           (0x1 << 13) /* VF settings change */
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
	/**
	 * BIT0: class, refer NFP_NET_CFG_VERSION_CLASS_*
	 * BIT[7:1]: reserved
	 */
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
#define   NFP_NET_CFG_VERSION_CLASS_GENERIC    0
#define   NFP_NET_CFG_VERSION_CLASS_NO_EMEM    1
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
 * NFP6000/NFP4000 - Prepend configuration
 */
#define NFP_NET_CFG_RX_OFFSET           0x0050
#define NFP_NET_CFG_RX_OFFSET_DYNAMIC          0    /* Prepend mode */

#define NFP_NET_CFG_RSS_CAP             0x0054

/* Start anchor of the TLV area */
#define NFP_NET_CFG_TLV_BASE            0x0058

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
#define NFP_NET_CFG_CTRL_MCAST_FILTER     (0x1 << 2) /**< Multicast Filter */
#define NFP_NET_CFG_CTRL_IPSEC_SM_LOOKUP  (0x1 << 3) /**< SA short match lookup */
#define NFP_NET_CFG_CTRL_IPSEC_LM_LOOKUP  (0x1 << 4) /**< SA long match lookup */
#define NFP_NET_CFG_CTRL_MULTI_PF         (0x1 << 5)
#define NFP_NET_CFG_CTRL_FLOW_STEER       (0x1 << 8) /**< Flow Steering */
#define NFP_NET_CFG_CTRL_VIRTIO           (0x1 << 10) /**< Virtio offload */
#define NFP_NET_CFG_CTRL_IN_ORDER         (0x1 << 11) /**< Virtio in-order flag */
#define NFP_NET_CFG_CTRL_LM_RELAY         (0x1 << 12) /**< Virtio live migration relay start */
#define NFP_NET_CFG_CTRL_NOTIFY_DATA      (0x1 << 13) /**< Virtio notification data flag */
#define NFP_NET_CFG_CTRL_SWLM             (0x1 << 14) /**< Virtio SW live migration enable */
#define NFP_NET_CFG_CTRL_USO              (0x1 << 16) /**< UDP segmentation offload */

#define NFP_NET_CFG_CAP_WORD1           0x00a4

#define NFP_NET_CFG_TX_USED_INDEX       0x00b0
#define NFP_NET_CFG_RX_USED_INDEX       0x00b4

/* 16B reserved for future use (0x00b0 - 0x00c0). */
#define NFP_NET_CFG_MAX_FS_CAP          0x00b8

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
#define   NFP_NET_CFG_RSS_XOR             (1 << 25) /* Use XOR as hash */
#define   NFP_NET_CFG_RSS_CRC32           (1 << 26) /* Use CRC32 hash */
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

#endif /* __NFP_COMMON_CTRL_H__ */
