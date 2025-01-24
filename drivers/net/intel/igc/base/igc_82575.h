/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _IGC_82575_H_
#define _IGC_82575_H_

#define ID_LED_DEFAULT_82575_SERDES	((ID_LED_DEF1_DEF2 << 12) | \
					 (ID_LED_DEF1_DEF2 <<  8) | \
					 (ID_LED_DEF1_DEF2 <<  4) | \
					 (ID_LED_OFF1_ON2))
/*
 * Receive Address Register Count
 * Number of high/low register pairs in the RAR.  The RAR (Receive Address
 * Registers) holds the directed and multicast addresses that we monitor.
 * These entries are also used for MAC-based filtering.
 */
/*
 * For 82576, there are an additional set of RARs that begin at an offset
 * separate from the first set of RARs.
 */
#define IGC_RAR_ENTRIES_82575	16
#define IGC_RAR_ENTRIES_82576	24
#define IGC_RAR_ENTRIES_82580	24
#define IGC_RAR_ENTRIES_I350	32
#define IGC_SW_SYNCH_MB	0x00000100
#define IGC_STAT_DEV_RST_SET	0x00100000

struct igc_adv_data_desc {
	__le64 buffer_addr;    /* Address of the descriptor's data buffer */
	union {
		u32 data;
		struct {
			u32 datalen:16; /* Data buffer length */
			u32 rsvd:4;
			u32 dtyp:4;  /* Descriptor type */
			u32 dcmd:8;  /* Descriptor command */
		} config;
	} lower;
	union {
		u32 data;
		struct {
			u32 status:4;  /* Descriptor status */
			u32 idx:4;
			u32 popts:6;  /* Packet Options */
			u32 paylen:18; /* Payload length */
		} options;
	} upper;
};

#define IGC_TXD_DTYP_ADV_C	0x2  /* Advanced Context Descriptor */
#define IGC_TXD_DTYP_ADV_D	0x3  /* Advanced Data Descriptor */
#define IGC_ADV_TXD_CMD_DEXT	0x20 /* Descriptor extension (0 = legacy) */
#define IGC_ADV_TUCMD_IPV4	0x2  /* IP Packet Type: 1=IPv4 */
#define IGC_ADV_TUCMD_IPV6	0x0  /* IP Packet Type: 0=IPv6 */
#define IGC_ADV_TUCMD_L4T_UDP	0x0  /* L4 Packet TYPE of UDP */
#define IGC_ADV_TUCMD_L4T_TCP	0x4  /* L4 Packet TYPE of TCP */
#define IGC_ADV_TUCMD_MKRREQ	0x10 /* Indicates markers are required */
#define IGC_ADV_DCMD_EOP	0x1  /* End of Packet */
#define IGC_ADV_DCMD_IFCS	0x2  /* Insert FCS (Ethernet CRC) */
#define IGC_ADV_DCMD_RS	0x8  /* Report Status */
#define IGC_ADV_DCMD_VLE	0x40 /* Add VLAN tag */
#define IGC_ADV_DCMD_TSE	0x80 /* TCP Seg enable */
/* Extended Device Control */
#define IGC_CTRL_EXT_NSICR	0x00000001 /* Disable Intr Clear all on read */

struct igc_adv_context_desc {
	union {
		u32 ip_config;
		struct {
			u32 iplen:9;
			u32 maclen:7;
			u32 vlan_tag:16;
		} fields;
	} ip_setup;
	u32 seq_num;
	union {
		u64 l4_config;
		struct {
			u32 mkrloc:9;
			u32 tucmd:11;
			u32 dtyp:4;
			u32 adv:8;
			u32 rsvd:4;
			u32 idx:4;
			u32 l4len:8;
			u32 mss:16;
		} fields;
	} l4_setup;
};

/* SRRCTL bit definitions */
#define IGC_SRRCTL_BSIZEHDRSIZE_MASK		0x00000F00
#define IGC_SRRCTL_DESCTYPE_LEGACY		0x00000000
#define IGC_SRRCTL_DESCTYPE_HDR_SPLIT		0x04000000
#define IGC_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS	0x0A000000
#define IGC_SRRCTL_DESCTYPE_HDR_REPLICATION	0x06000000
#define IGC_SRRCTL_DESCTYPE_HDR_REPLICATION_LARGE_PKT 0x08000000
#define IGC_SRRCTL_DESCTYPE_MASK		0x0E000000
#define IGC_SRRCTL_TIMESTAMP			0x40000000
#define IGC_SRRCTL_DROP_EN			0x80000000

#define IGC_SRRCTL_BSIZEPKT_MASK		0x0000007F
#define IGC_SRRCTL_BSIZEHDR_MASK		0x00003F00

#define IGC_TX_HEAD_WB_ENABLE		0x1
#define IGC_TX_SEQNUM_WB_ENABLE	0x2

#define IGC_MRQC_ENABLE_RSS_4Q		0x00000002
#define IGC_MRQC_ENABLE_VMDQ			0x00000003
#define IGC_MRQC_ENABLE_VMDQ_RSS_2Q		0x00000005
#define IGC_MRQC_RSS_FIELD_IPV4_UDP		0x00400000
#define IGC_MRQC_RSS_FIELD_IPV6_UDP		0x00800000
#define IGC_MRQC_RSS_FIELD_IPV6_UDP_EX	0x01000000
#define IGC_MRQC_ENABLE_RSS_8Q		0x00000002

#define IGC_VMRCTL_MIRROR_PORT_SHIFT		8
#define IGC_VMRCTL_MIRROR_DSTPORT_MASK	(7 << \
						 IGC_VMRCTL_MIRROR_PORT_SHIFT)
#define IGC_VMRCTL_POOL_MIRROR_ENABLE		(1 << 0)
#define IGC_VMRCTL_UPLINK_MIRROR_ENABLE	(1 << 1)
#define IGC_VMRCTL_DOWNLINK_MIRROR_ENABLE	(1 << 2)

#define IGC_EICR_TX_QUEUE ( \
	IGC_EICR_TX_QUEUE0 |    \
	IGC_EICR_TX_QUEUE1 |    \
	IGC_EICR_TX_QUEUE2 |    \
	IGC_EICR_TX_QUEUE3)

#define IGC_EICR_RX_QUEUE ( \
	IGC_EICR_RX_QUEUE0 |    \
	IGC_EICR_RX_QUEUE1 |    \
	IGC_EICR_RX_QUEUE2 |    \
	IGC_EICR_RX_QUEUE3)

#define IGC_EIMS_RX_QUEUE	IGC_EICR_RX_QUEUE
#define IGC_EIMS_TX_QUEUE	IGC_EICR_TX_QUEUE

#define EIMS_ENABLE_MASK ( \
	IGC_EIMS_RX_QUEUE  | \
	IGC_EIMS_TX_QUEUE  | \
	IGC_EIMS_TCP_TIMER | \
	IGC_EIMS_OTHER)

/* Immediate Interrupt Rx (A.K.A. Low Latency Interrupt) */
#define IGC_IMIR_PORT_IM_EN	0x00010000  /* TCP port enable */
#define IGC_IMIR_PORT_BP	0x00020000  /* TCP port check bypass */
#define IGC_IMIREXT_CTRL_URG	0x00002000  /* Check URG bit in header */
#define IGC_IMIREXT_CTRL_ACK	0x00004000  /* Check ACK bit in header */
#define IGC_IMIREXT_CTRL_PSH	0x00008000  /* Check PSH bit in header */
#define IGC_IMIREXT_CTRL_RST	0x00010000  /* Check RST bit in header */
#define IGC_IMIREXT_CTRL_SYN	0x00020000  /* Check SYN bit in header */
#define IGC_IMIREXT_CTRL_FIN	0x00040000  /* Check FIN bit in header */

#define IGC_RXDADV_RSSTYPE_MASK	0x0000000F
#define IGC_RXDADV_RSSTYPE_SHIFT	12
#define IGC_RXDADV_HDRBUFLEN_MASK	0x7FE0
#define IGC_RXDADV_HDRBUFLEN_SHIFT	5
#define IGC_RXDADV_SPLITHEADER_EN	0x00001000
#define IGC_RXDADV_SPH		0x8000
#define IGC_RXDADV_STAT_TS		0x10000 /* Pkt was time stamped */
#define IGC_RXDADV_ERR_HBO		0x00800000

/* RSS Hash results */
#define IGC_RXDADV_RSSTYPE_NONE	0x00000000
#define IGC_RXDADV_RSSTYPE_IPV4_TCP	0x00000001
#define IGC_RXDADV_RSSTYPE_IPV4	0x00000002
#define IGC_RXDADV_RSSTYPE_IPV6_TCP	0x00000003
#define IGC_RXDADV_RSSTYPE_IPV6_EX	0x00000004
#define IGC_RXDADV_RSSTYPE_IPV6	0x00000005
#define IGC_RXDADV_RSSTYPE_IPV6_TCP_EX 0x00000006
#define IGC_RXDADV_RSSTYPE_IPV4_UDP	0x00000007
#define IGC_RXDADV_RSSTYPE_IPV6_UDP	0x00000008
#define IGC_RXDADV_RSSTYPE_IPV6_UDP_EX 0x00000009

/* RSS Packet Types as indicated in the receive descriptor */
#define IGC_RXDADV_PKTTYPE_ILMASK	0x000000F0
#define IGC_RXDADV_PKTTYPE_TLMASK	0x00000F00
#define IGC_RXDADV_PKTTYPE_NONE	0x00000000
#define IGC_RXDADV_PKTTYPE_IPV4	0x00000010 /* IPV4 hdr present */
#define IGC_RXDADV_PKTTYPE_IPV4_EX	0x00000020 /* IPV4 hdr + extensions */
#define IGC_RXDADV_PKTTYPE_IPV6	0x00000040 /* IPV6 hdr present */
#define IGC_RXDADV_PKTTYPE_IPV6_EX	0x00000080 /* IPV6 hdr + extensions */
#define IGC_RXDADV_PKTTYPE_TCP	0x00000100 /* TCP hdr present */
#define IGC_RXDADV_PKTTYPE_UDP	0x00000200 /* UDP hdr present */
#define IGC_RXDADV_PKTTYPE_SCTP	0x00000400 /* SCTP hdr present */
#define IGC_RXDADV_PKTTYPE_NFS	0x00000800 /* NFS hdr present */

#define IGC_RXDADV_PKTTYPE_IPSEC_ESP	0x00001000 /* IPSec ESP */
#define IGC_RXDADV_PKTTYPE_IPSEC_AH	0x00002000 /* IPSec AH */
#define IGC_RXDADV_PKTTYPE_LINKSEC	0x00004000 /* LinkSec Encap */
#define IGC_RXDADV_PKTTYPE_ETQF	0x00008000 /* PKTTYPE is ETQF index */
#define IGC_RXDADV_PKTTYPE_ETQF_MASK	0x00000070 /* ETQF has 8 indices */
#define IGC_RXDADV_PKTTYPE_ETQF_SHIFT	4 /* Right-shift 4 bits */

/* LinkSec results */
/* Security Processing bit Indication */
#define IGC_RXDADV_LNKSEC_STATUS_SECP		0x00020000
#define IGC_RXDADV_LNKSEC_ERROR_BIT_MASK	0x18000000
#define IGC_RXDADV_LNKSEC_ERROR_NO_SA_MATCH	0x08000000
#define IGC_RXDADV_LNKSEC_ERROR_REPLAY_ERROR	0x10000000
#define IGC_RXDADV_LNKSEC_ERROR_BAD_SIG	0x18000000

#define IGC_RXDADV_IPSEC_STATUS_SECP			0x00020000
#define IGC_RXDADV_IPSEC_ERROR_BIT_MASK		0x18000000
#define IGC_RXDADV_IPSEC_ERROR_INVALID_PROTOCOL	0x08000000
#define IGC_RXDADV_IPSEC_ERROR_INVALID_LENGTH		0x10000000
#define IGC_RXDADV_IPSEC_ERROR_AUTHENTICATION_FAILED	0x18000000

#define IGC_TXDCTL_SWFLSH		0x04000000 /* Tx Desc. wbk flushing */
/* Tx Queue Arbitration Priority 0=low, 1=high */
#define IGC_TXDCTL_PRIORITY		0x08000000

#define IGC_RXDCTL_SWFLSH		0x04000000 /* Rx Desc. wbk flushing */

/* Direct Cache Access (DCA) definitions */
#define IGC_DCA_CTRL_DCA_ENABLE	0x00000000 /* DCA Enable */
#define IGC_DCA_CTRL_DCA_DISABLE	0x00000001 /* DCA Disable */

#define IGC_DCA_CTRL_DCA_MODE_CB1	0x00 /* DCA Mode CB1 */
#define IGC_DCA_CTRL_DCA_MODE_CB2	0x02 /* DCA Mode CB2 */

#define IGC_DCA_RXCTRL_CPUID_MASK	0x0000001F /* Rx CPUID Mask */
#define IGC_DCA_RXCTRL_DESC_DCA_EN	(1 << 5) /* DCA Rx Desc enable */
#define IGC_DCA_RXCTRL_HEAD_DCA_EN	(1 << 6) /* DCA Rx Desc header ena */
#define IGC_DCA_RXCTRL_DATA_DCA_EN	(1 << 7) /* DCA Rx Desc payload ena */
#define IGC_DCA_RXCTRL_DESC_RRO_EN	(1 << 9) /* DCA Rx Desc Relax Order */

#define IGC_DCA_TXCTRL_CPUID_MASK	0x0000001F /* Tx CPUID Mask */
#define IGC_DCA_TXCTRL_DESC_DCA_EN	(1 << 5) /* DCA Tx Desc enable */
#define IGC_DCA_TXCTRL_DESC_RRO_EN	(1 << 9) /* Tx rd Desc Relax Order */
#define IGC_DCA_TXCTRL_TX_WB_RO_EN	(1 << 11) /* Tx Desc writeback RO bit */
#define IGC_DCA_TXCTRL_DATA_RRO_EN	(1 << 13) /* Tx rd data Relax Order */

#define IGC_DCA_TXCTRL_CPUID_MASK_82576	0xFF000000 /* Tx CPUID Mask */
#define IGC_DCA_RXCTRL_CPUID_MASK_82576	0xFF000000 /* Rx CPUID Mask */
#define IGC_DCA_TXCTRL_CPUID_SHIFT_82576	24 /* Tx CPUID */
#define IGC_DCA_RXCTRL_CPUID_SHIFT_82576	24 /* Rx CPUID */

/* Additional interrupt register bit definitions */
#define IGC_ICR_LSECPNS	0x00000020 /* PN threshold - server */
#define IGC_IMS_LSECPNS	IGC_ICR_LSECPNS /* PN threshold - server */
#define IGC_ICS_LSECPNS	IGC_ICR_LSECPNS /* PN threshold - server */

/* ETQF register bit definitions */
#define IGC_ETQF_FILTER_ENABLE	(1 << 26)
#define IGC_ETQF_IMM_INT		(1 << 29)
#define IGC_ETQF_QUEUE_ENABLE		(1 << 31)
/*
 * ETQF filter list: one static filter per filter consumer. This is
 *                   to avoid filter collisions later. Add new filters
 *                   here!!
 *
 * Current filters:
 *    EAPOL 802.1x (0x888e): Filter 0
 */
#define IGC_ETQF_FILTER_EAPOL		0

#define IGC_FTQF_MASK_SOURCE_ADDR_BP	0x20000000
#define IGC_FTQF_MASK_DEST_ADDR_BP	0x40000000
#define IGC_FTQF_MASK_SOURCE_PORT_BP	0x80000000

#define IGC_NVM_APME_82575		0x0400
#define MAX_NUM_VFS			7

#define IGC_DTXSWC_MAC_SPOOF_MASK	0x000000FF /* Per VF MAC spoof cntrl */
#define IGC_DTXSWC_VLAN_SPOOF_MASK	0x0000FF00 /* Per VF VLAN spoof cntrl */
#define IGC_DTXSWC_LLE_MASK		0x00FF0000 /* Per VF Local LB enables */
#define IGC_DTXSWC_VLAN_SPOOF_SHIFT	8
#define IGC_DTXSWC_LLE_SHIFT		16
#define IGC_DTXSWC_VMDQ_LOOPBACK_EN	(1 << 31)  /* global VF LB enable */

/* Easy defines for setting default pool, would normally be left a zero */
#define IGC_VT_CTL_DEFAULT_POOL_SHIFT	7
#define IGC_VT_CTL_DEFAULT_POOL_MASK	(0x7 << IGC_VT_CTL_DEFAULT_POOL_SHIFT)

/* Other useful VMD_CTL register defines */
#define IGC_VT_CTL_IGNORE_MAC		(1 << 28)
#define IGC_VT_CTL_DISABLE_DEF_POOL	(1 << 29)
#define IGC_VT_CTL_VM_REPL_EN		(1 << 30)

/* Per VM Offload register setup */
#define IGC_VMOLR_RLPML_MASK	0x00003FFF /* Long Packet Maximum Length mask */
#define IGC_VMOLR_LPE		0x00010000 /* Accept Long packet */
#define IGC_VMOLR_RSSE	0x00020000 /* Enable RSS */
#define IGC_VMOLR_AUPE	0x01000000 /* Accept untagged packets */
#define IGC_VMOLR_ROMPE	0x02000000 /* Accept overflow multicast */
#define IGC_VMOLR_ROPE	0x04000000 /* Accept overflow unicast */
#define IGC_VMOLR_BAM		0x08000000 /* Accept Broadcast packets */
#define IGC_VMOLR_MPME	0x10000000 /* Multicast promiscuous mode */
#define IGC_VMOLR_STRVLAN	0x40000000 /* Vlan stripping enable */
#define IGC_VMOLR_STRCRC	0x80000000 /* CRC stripping enable */

#define IGC_VMOLR_VPE		0x00800000 /* VLAN promiscuous enable */
#define IGC_VMOLR_UPE		0x20000000 /* Unicast promisuous enable */
#define IGC_DVMOLR_HIDVLAN	0x20000000 /* Vlan hiding enable */
#define IGC_DVMOLR_STRVLAN	0x40000000 /* Vlan stripping enable */
#define IGC_DVMOLR_STRCRC	0x80000000 /* CRC stripping enable */

#define IGC_PBRWAC_WALPB	0x00000007 /* Wrap around event on LAN Rx PB */
#define IGC_PBRWAC_PBE	0x00000008 /* Rx packet buffer empty */

#define IGC_VLVF_ARRAY_SIZE		32
#define IGC_VLVF_VLANID_MASK		0x00000FFF
#define IGC_VLVF_POOLSEL_SHIFT	12
#define IGC_VLVF_POOLSEL_MASK		(0xFF << IGC_VLVF_POOLSEL_SHIFT)
#define IGC_VLVF_LVLAN		0x00100000
#define IGC_VLVF_VLANID_ENABLE	0x80000000

#define IGC_VMVIR_VLANA_DEFAULT	0x40000000 /* Always use default VLAN */
#define IGC_VMVIR_VLANA_NEVER		0x80000000 /* Never insert VLAN tag */

#define IGC_VF_INIT_TIMEOUT	200 /* Number of retries to clear RSTI */

#define IGC_IOVCTL		0x05BBC
#define IGC_IOVCTL_REUSE_VFQ	0x00000001

#define IGC_RPLOLR_STRVLAN	0x40000000
#define IGC_RPLOLR_STRCRC	0x80000000

#define IGC_TCTL_EXT_COLD	0x000FFC00
#define IGC_TCTL_EXT_COLD_SHIFT	10

#define IGC_DTXCTL_8023LL	0x0004
#define IGC_DTXCTL_VLAN_ADDED	0x0008
#define IGC_DTXCTL_OOS_ENABLE	0x0010
#define IGC_DTXCTL_MDP_EN	0x0020
#define IGC_DTXCTL_SPOOF_INT	0x0040

#define IGC_EEPROM_PCS_AUTONEG_DISABLE_BIT	(1 << 14)

#define ALL_QUEUES		0xFFFF

s32 igc_reset_init_script_82575(struct igc_hw *hw);
s32 igc_init_nvm_params_82575(struct igc_hw *hw);

/* Rx packet buffer size defines */
#define IGC_RXPBS_SIZE_MASK_82576	0x0000007F
void igc_vmdq_set_loopback_pf(struct igc_hw *hw, bool enable);
void igc_vmdq_set_anti_spoofing_pf(struct igc_hw *hw, bool enable, int pf);
void igc_vmdq_set_replication_pf(struct igc_hw *hw, bool enable);

enum igc_promisc_type {
	igc_promisc_disabled = 0,   /* all promisc modes disabled */
	igc_promisc_unicast = 1,    /* unicast promiscuous enabled */
	igc_promisc_multicast = 2,  /* multicast promiscuous enabled */
	igc_promisc_enabled = 3,    /* both uni and multicast promisc */
	igc_num_promisc_types
};

#endif /* _IGC_82575_H_ */
