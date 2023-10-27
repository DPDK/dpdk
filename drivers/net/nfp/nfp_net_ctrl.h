/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NET_CTRL_H__
#define __NFP_NET_CTRL_H__

#include <stdint.h>

#include <ethdev_driver.h>

#include <nfp_common_ctrl.h>

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

#endif /* __NFP_NET_CTRL_H__ */
