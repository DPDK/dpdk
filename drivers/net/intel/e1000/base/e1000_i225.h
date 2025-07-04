/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _E1000_I225_H_
#define _E1000_I225_H_

bool e1000_get_flash_presence_i225(struct e1000_hw *hw);
s32 e1000_update_flash_i225(struct e1000_hw *hw);
s32 e1000_update_nvm_checksum_i225(struct e1000_hw *hw);
s32 e1000_validate_nvm_checksum_i225(struct e1000_hw *hw);
s32 e1000_write_nvm_srwr_i225(struct e1000_hw *hw, u16 offset,
			      u16 words, u16 *data);
s32 e1000_read_nvm_srrd_i225(struct e1000_hw *hw, u16 offset,
			     u16 words, u16 *data);
s32 e1000_set_flsw_flash_burst_counter_i225(struct e1000_hw *hw,
					    u32 burst_counter);
s32 e1000_write_erase_flash_command_i225(struct e1000_hw *hw, u32 opcode,
					 u32 address);
s32 e1000_id_led_init_i225(struct e1000_hw *hw);
s32 e1000_blink_led_i225(struct e1000_hw *hw);
s32 e1000_check_for_link_i225(struct e1000_hw *hw);
s32 e1000_set_ltr_i225(struct e1000_hw *hw, bool link);
s32 e1000_acquire_swfw_sync_i225(struct e1000_hw *hw, u16 mask);
void e1000_release_swfw_sync_i225(struct e1000_hw *hw, u16 mask);
s32 e1000_init_hw_i225(struct e1000_hw *hw);
s32 e1000_setup_copper_link_i225(struct e1000_hw *hw);
s32 e1000_set_d0_lplu_state_i225(struct e1000_hw *hw, bool active);
s32 e1000_set_d3_lplu_state_i225(struct e1000_hw *hw, bool active);
s32 e1000_set_eee_i225(struct e1000_hw *hw, bool adv2p5G, bool adv1G,
		       bool adv100M);

#define ID_LED_DEFAULT_I225		((ID_LED_OFF1_ON2  << 8) | \
					 (ID_LED_DEF1_DEF2 <<  4) | \
					 (ID_LED_OFF1_OFF2))
#define ID_LED_DEFAULT_I225_SERDES	((ID_LED_DEF1_DEF2 << 8) | \
					 (ID_LED_DEF1_DEF2 <<  4) | \
					 (ID_LED_OFF1_ON2))

/* NVM offset defaults for I225 devices */
#define NVM_INIT_CTRL_2_DEFAULT_I225	0X7243
#define NVM_INIT_CTRL_4_DEFAULT_I225	0x00C1
#define NVM_LED_1_CFG_DEFAULT_I225	0x0184
#define NVM_LED_0_2_CFG_DEFAULT_I225	0x200C

#define E1000_MRQC_ENABLE_RSS_4Q		0x00000002
#define E1000_MRQC_ENABLE_VMDQ			0x00000003
#define E1000_MRQC_ENABLE_VMDQ_RSS_2Q		0x00000005
#define E1000_MRQC_RSS_FIELD_IPV4_UDP		0x00400000
#define E1000_MRQC_RSS_FIELD_IPV6_UDP		0x00800000
#define E1000_MRQC_RSS_FIELD_IPV6_UDP_EX	0x01000000
#define E1000_I225_SHADOW_RAM_SIZE		4096
#define E1000_I225_ERASE_CMD_OPCODE		0x02000000
#define E1000_I225_WRITE_CMD_OPCODE		0x01000000
#define E1000_FLSWCTL_DONE			0x40000000
#define E1000_FLSWCTL_CMDV			0x10000000

/* SRRCTL bit definitions */
#define E1000_SRRCTL_BSIZEHDRSIZE_MASK		0x00000F00
#define E1000_SRRCTL_DESCTYPE_LEGACY		0x00000000
#define E1000_SRRCTL_DESCTYPE_HDR_SPLIT		0x04000000
#define E1000_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS	0x0A000000
#define E1000_SRRCTL_DESCTYPE_HDR_REPLICATION	0x06000000
#define E1000_SRRCTL_DESCTYPE_HDR_REPLICATION_LARGE_PKT 0x08000000
#define E1000_SRRCTL_DESCTYPE_MASK		0x0E000000
#define E1000_SRRCTL_DROP_EN			0x80000000
#define E1000_SRRCTL_BSIZEPKT_MASK		0x0000007F
#define E1000_SRRCTL_BSIZEHDR_MASK		0x00003F00

#define E1000_RXDADV_RSSTYPE_MASK	0x0000000F
#define E1000_RXDADV_RSSTYPE_SHIFT	12
#define E1000_RXDADV_HDRBUFLEN_MASK	0x7FE0
#define E1000_RXDADV_HDRBUFLEN_SHIFT	5
#define E1000_RXDADV_SPLITHEADER_EN	0x00001000
#define E1000_RXDADV_SPH		0x8000
#define E1000_RXDADV_STAT_TS		0x10000 /* Pkt was time stamped */
#define E1000_RXDADV_ERR_HBO		0x00800000

/* RSS Hash results */
#define E1000_RXDADV_RSSTYPE_NONE	0x00000000
#define E1000_RXDADV_RSSTYPE_IPV4_TCP	0x00000001
#define E1000_RXDADV_RSSTYPE_IPV4	0x00000002
#define E1000_RXDADV_RSSTYPE_IPV6_TCP	0x00000003
#define E1000_RXDADV_RSSTYPE_IPV6_EX	0x00000004
#define E1000_RXDADV_RSSTYPE_IPV6	0x00000005
#define E1000_RXDADV_RSSTYPE_IPV6_TCP_EX 0x00000006
#define E1000_RXDADV_RSSTYPE_IPV4_UDP	0x00000007
#define E1000_RXDADV_RSSTYPE_IPV6_UDP	0x00000008
#define E1000_RXDADV_RSSTYPE_IPV6_UDP_EX 0x00000009

/* RSS Packet Types as indicated in the receive descriptor */
#define E1000_RXDADV_PKTTYPE_ILMASK	0x000000F0
#define E1000_RXDADV_PKTTYPE_TLMASK	0x00000F00
#define E1000_RXDADV_PKTTYPE_NONE	0x00000000
#define E1000_RXDADV_PKTTYPE_IPV4	0x00000010 /* IPV4 hdr present */
#define E1000_RXDADV_PKTTYPE_IPV4_EX	0x00000020 /* IPV4 hdr + extensions */
#define E1000_RXDADV_PKTTYPE_IPV6	0x00000040 /* IPV6 hdr present */
#define E1000_RXDADV_PKTTYPE_IPV6_EX	0x00000080 /* IPV6 hdr + extensions */
#define E1000_RXDADV_PKTTYPE_TCP	0x00000100 /* TCP hdr present */
#define E1000_RXDADV_PKTTYPE_UDP	0x00000200 /* UDP hdr present */
#define E1000_RXDADV_PKTTYPE_SCTP	0x00000400 /* SCTP hdr present */
#define E1000_RXDADV_PKTTYPE_NFS	0x00000800 /* NFS hdr present */

#define E1000_RXDADV_PKTTYPE_IPSEC_ESP	0x00001000 /* IPSec ESP */
#define E1000_RXDADV_PKTTYPE_IPSEC_AH	0x00002000 /* IPSec AH */
#define E1000_RXDADV_PKTTYPE_LINKSEC	0x00004000 /* LinkSec Encap */
#define E1000_RXDADV_PKTTYPE_ETQF	0x00008000 /* PKTTYPE is ETQF index */
#define E1000_RXDADV_PKTTYPE_ETQF_MASK	0x00000070 /* ETQF has 8 indices */
#define E1000_RXDADV_PKTTYPE_ETQF_SHIFT	4 /* Right-shift 4 bits */

/* LED Control */
#define E1000_GLOBAL_BLINK_MODE	0x00000020 /*Blink at 200 ms on and 200 ms off.*/
#define E1000_LED1_MODE_MASK	0x00000F00
#define E1000_LED1_BLINK	0x00008000
#define E1000_LED2_MODE_MASK	0x000F0000

#endif
