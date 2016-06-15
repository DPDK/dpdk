/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HSI_STRUCT_DEF_EXTERNAL_H_
#define _HSI_STRUCT_DEF_EXTERNAL_H_

/*
 * per-context HW statistics -- chip view
 */

struct ctx_hw_stats64 {
	uint64_t rx_ucast_pkts;
	uint64_t rx_mcast_pkts;
	uint64_t rx_bcast_pkts;
	uint64_t rx_drop_pkts;
	uint64_t rx_err_pkts;
	uint64_t rx_ucast_bytes;
	uint64_t rx_mcast_bytes;
	uint64_t rx_bcast_bytes;

	uint64_t tx_ucast_pkts;
	uint64_t tx_mcast_pkts;
	uint64_t tx_bcast_pkts;
	uint64_t tx_drop_pkts;
	uint64_t tx_err_pkts;
	uint64_t tx_ucast_bytes;
	uint64_t tx_mcast_bytes;
	uint64_t tx_bcast_bytes;

	uint64_t tpa_pkts;
	uint64_t tpa_bytes;
	uint64_t tpa_events;
	uint64_t tpa_aborts;
} ctx_hw_stats64_t;

/* HW Resource Manager Specification 1.2.0 */
#define HWRM_VERSION_MAJOR	1
#define HWRM_VERSION_MINOR	2
#define HWRM_VERSION_UPDATE	0

/*
 * Following is the signature for HWRM message field that indicates not
 * applicable (All F's). Need to cast it the size of the field if needed.
 */
#define HWRM_NA_SIGNATURE        ((uint32_t)(-1))
#define HWRM_MAX_REQ_LEN	(128)  /* hwrm_func_buf_rgtr */
#define HWRM_MAX_RESP_LEN	(176)  /* hwrm_func_qstats */
#define HW_HASH_INDEX_SIZE      0x80    /* 7 bit indirection table index. */
#define HW_HASH_KEY_SIZE        40
#define HWRM_RESP_VALID_KEY	1 /* valid key for HWRM response */

/*
 * Request types
 */
#define HWRM_VER_GET			(UINT32_C(0x0))
#define HWRM_FUNC_RESET			(UINT32_C(0x11))
#define HWRM_FUNC_QCAPS			(UINT32_C(0x15))
#define HWRM_FUNC_DRV_UNRGTR		(UINT32_C(0x1a))
#define HWRM_FUNC_DRV_RGTR		(UINT32_C(0x1d))
#define HWRM_PORT_PHY_CFG		(UINT32_C(0x20))
#define HWRM_PORT_PHY_QCFG		(UINT32_C(0x27))
#define HWRM_QUEUE_QPORTCFG		(UINT32_C(0x30))
#define HWRM_VNIC_ALLOC			(UINT32_C(0x40))
#define HWRM_VNIC_FREE			(UINT32_C(0x41))
#define HWRM_VNIC_CFG			(UINT32_C(0x42))
#define HWRM_VNIC_RSS_CFG		(UINT32_C(0x46))
#define HWRM_RING_ALLOC			(UINT32_C(0x50))
#define HWRM_RING_FREE			(UINT32_C(0x51))
#define HWRM_RING_GRP_ALLOC		(UINT32_C(0x60))
#define HWRM_RING_GRP_FREE		(UINT32_C(0x61))
#define HWRM_VNIC_RSS_COS_LB_CTX_ALLOC	(UINT32_C(0x70))
#define HWRM_VNIC_RSS_COS_LB_CTX_FREE	(UINT32_C(0x71))
#define HWRM_CFA_L2_FILTER_ALLOC	(UINT32_C(0x90))
#define HWRM_CFA_L2_FILTER_FREE		(UINT32_C(0x91))
#define HWRM_CFA_L2_FILTER_CFG		(UINT32_C(0x92))
#define HWRM_CFA_L2_SET_RX_MASK		(UINT32_C(0x93))
#define HWRM_STAT_CTX_ALLOC		(UINT32_C(0xb0))
#define HWRM_STAT_CTX_FREE		(UINT32_C(0xb1))
#define HWRM_STAT_CTX_CLR_STATS		(UINT32_C(0xb3))
#define HWRM_EXEC_FWD_RESP		(UINT32_C(0xd0))

/* Return Codes */
#define HWRM_ERR_CODE_INVALID_PARAMS                      (UINT32_C(0x2))
#define HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED              (UINT32_C(0x3))

/* Short TX BD (16 bytes) */
struct tx_bd_short {
	/*
	 * All bits in this field must be valid on the first BD of a packet.
	 * Only the packet_end bit must be valid for the remaining BDs of a
	 * packet.
	 */
	/* This value identifies the type of buffer descriptor. */
	#define TX_BD_SHORT_TYPE_MASK			UINT32_C(0x3f)
	#define TX_BD_SHORT_TYPE_SFT			0
		/*
		 * Indicates that this BD is 16B long and is used for normal L2
		 * packet transmission.
		 */
	#define TX_BD_SHORT_TYPE_TX_BD_SHORT		(UINT32_C(0x0) << 0)
	/*
	 * If set to 1, the packet ends with the data in the buffer pointed to
	 * by this descriptor. This flag must be valid on every BD.
	 */
	#define TX_BD_SHORT_FLAGS_PACKET_END		UINT32_C(0x40)
	/*
	 * If set to 1, the device will not generate a completion for this
	 * transmit packet unless there is an error in it's processing. If this
	 * bit is set to 0, then the packet will be completed normally. This bit
	 * must be valid only on the first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_NO_CMPL		UINT32_C(0x80)
	/*
	 * This value indicates how many 16B BD locations are consumed in the
	 * ring by this packet. A value of 1 indicates that this BD is the only
	 * BD (and that the it is a short BD). A value of 3 indicates either 3
	 * short BDs or 1 long BD and one short BD in the packet. A value of 0
	 * indicates that there are 32 BD locations in the packet (the maximum).
	 * This field is valid only on the first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_BD_CNT_MASK		UINT32_C(0x1f00)
	#define TX_BD_SHORT_FLAGS_BD_CNT_SFT		8
	/*
	 * This value is a hint for the length of the entire packet. It is used
	 * by the chip to optimize internal processing. The packet will be
	 * dropped if the hint is too short. This field is valid only on the
	 * first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_LHINT_MASK		UINT32_C(0x6000)
	#define TX_BD_SHORT_FLAGS_LHINT_SFT		13
		/* indicates packet length < 512B */
	#define TX_BD_SHORT_FLAGS_LHINT_LT512		(UINT32_C(0x0) << 13)
		/* indicates 512 <= packet length < 1KB */
	#define TX_BD_SHORT_FLAGS_LHINT_LT1K		(UINT32_C(0x1) << 13)
		/* indicates 1KB <= packet length < 2KB */
	#define TX_BD_SHORT_FLAGS_LHINT_LT2K		(UINT32_C(0x2) << 13)
		/* indicates packet length >= 2KB */
	#define TX_BD_SHORT_FLAGS_LHINT_GTE2K		(UINT32_C(0x3) << 13)
	#define TX_BD_SHORT_FLAGS_LHINT_LAST	TX_BD_SHORT_FLAGS_LHINT_GTE2K
	/*
	 * If set to 1, the device immediately updates the Send Consumer Index
	 * after the buffer associated with this descriptor has been transferred
	 * via DMA to NIC memory from host memory. An interrupt may or may not
	 * be generated according to the state of the interrupt avoidance
	 * mechanisms. If this bit is set to 0, then the Consumer Index is only
	 * updated as soon as one of the host interrupt coalescing conditions
	 * has been met. This bit must be valid on the first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_COAL_NOW		UINT32_C(0x8000)
	/*
	 * All bits in this field must be valid on the first BD of a packet.
	 * Only the packet_end bit must be valid for the remaining BDs of a
	 * packet.
	 */
	#define TX_BD_SHORT_FLAGS_MASK			UINT32_C(0xffc0)
	#define TX_BD_SHORT_FLAGS_SFT			6
	uint16_t flags_type;

	/*
	 * This is the length of the host physical buffer this BD describes in
	 * bytes. This field must be valid on all BDs of a packet.
	 */
	uint16_t len;
	/*
	 * The opaque data field is pass through to the completion and can be
	 * used for any data that the driver wants to associate with the
	 * transmit BD. This field must be valid on the first BD of a packet.
	 */
	uint32_t opaque;

	/*
	 * This is the host physical address for the portion of the packet
	 * described by this TX BD. This value must be valid on all BDs of a
	 * packet.
	 */
	uint64_t addr;
} __attribute__((packed));

/* Long TX BD (32 bytes split to 2 16-byte struct) */
struct tx_bd_long {
	/*
	 * All bits in this field must be valid on the first BD of a packet.
	 * Only the packet_end bit must be valid for the remaining BDs of a
	 * packet.
	 */
	/* This value identifies the type of buffer descriptor. */
	#define TX_BD_LONG_TYPE_MASK			UINT32_C(0x3f)
	#define TX_BD_LONG_TYPE_SFT			0
		/*
		 * Indicates that this BD is 32B long and is used for normal L2
		 * packet transmission.
		 */
	#define TX_BD_LONG_TYPE_TX_BD_LONG		(UINT32_C(0x10) << 0)
	/*
	 * If set to 1, the packet ends with the data in the buffer pointed to
	 * by this descriptor. This flag must be valid on every BD.
	 */
	#define TX_BD_LONG_FLAGS_PACKET_END		UINT32_C(0x40)
	/*
	 * If set to 1, the device will not generate a completion for this
	 * transmit packet unless there is an error in it's processing. If this
	 * bit is set to 0, then the packet will be completed normally. This bit
	 * must be valid only on the first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_NO_CMPL		UINT32_C(0x80)
	/*
	 * This value indicates how many 16B BD locations are consumed in the
	 * ring by this packet. A value of 1 indicates that this BD is the only
	 * BD (and that the it is a short BD). A value of 3 indicates either 3
	 * short BDs or 1 long BD and one short BD in the packet. A value of 0
	 * indicates that there are 32 BD locations in the packet (the maximum).
	 * This field is valid only on the first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_BD_CNT_MASK		UINT32_C(0x1f00)
	#define TX_BD_LONG_FLAGS_BD_CNT_SFT		8
	/*
	 * This value is a hint for the length of the entire packet. It is used
	 * by the chip to optimize internal processing. The packet will be
	 * dropped if the hint is too short. This field is valid only on the
	 * first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_LHINT_MASK		UINT32_C(0x6000)
	#define TX_BD_LONG_FLAGS_LHINT_SFT		13
		/* indicates packet length < 512B */
	#define TX_BD_LONG_FLAGS_LHINT_LT512		(UINT32_C(0x0) << 13)
		/* indicates 512 <= packet length < 1KB */
	#define TX_BD_LONG_FLAGS_LHINT_LT1K		(UINT32_C(0x1) << 13)
		/* indicates 1KB <= packet length < 2KB */
	#define TX_BD_LONG_FLAGS_LHINT_LT2K		(UINT32_C(0x2) << 13)
		/* indicates packet length >= 2KB */
	#define TX_BD_LONG_FLAGS_LHINT_GTE2K		(UINT32_C(0x3) << 13)
	#define TX_BD_LONG_FLAGS_LHINT_LAST	TX_BD_LONG_FLAGS_LHINT_GTE2K
	/*
	 * If set to 1, the device immediately updates the Send Consumer Index
	 * after the buffer associated with this descriptor has been transferred
	 * via DMA to NIC memory from host memory. An interrupt may or may not
	 * be generated according to the state of the interrupt avoidance
	 * mechanisms. If this bit is set to 0, then the Consumer Index is only
	 * updated as soon as one of the host interrupt coalescing conditions
	 * has been met. This bit must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_COAL_NOW		UINT32_C(0x8000)
	/*
	 * All bits in this field must be valid on the first BD of a packet.
	 * Only the packet_end bit must be valid for the remaining BDs of a
	 * packet.
	 */
	#define TX_BD_LONG_FLAGS_MASK			UINT32_C(0xffc0)
	#define TX_BD_LONG_FLAGS_SFT			6
	uint16_t flags_type;

	/*
	 * This is the length of the host physical buffer this BD describes in
	 * bytes. This field must be valid on all BDs of a packet.
	 */
	uint16_t len;

	/*
	 * The opaque data field is pass through to the completion and can be
	 * used for any data that the driver wants to associate with the
	 * transmit BD. This field must be valid on the first BD of a packet.
	 */
	uint32_t opaque;

	/*
	 * This is the host physical address for the portion of the packet
	 * described by this TX BD. This value must be valid on all BDs of a
	 * packet.
	 */
	uint64_t addr;
} __attribute__((packed));

/* last 16 bytes of Long TX BD */

struct tx_bd_long_hi {
	/*
	 * All bits in this field must be valid on the first BD of a packet.
	 * Their value on other BDs of the packet will be ignored.
	 */
	/*
	 * If set to 1, the controller replaces the TCP/UPD checksum fields of
	 * normal TCP/UPD checksum, or the inner TCP/UDP checksum field of the
	 * encapsulated TCP/UDP packets with the hardware calculated TCP/UDP
	 * checksum for the packet associated with this descriptor. This bit
	 * must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_LFLAGS_TCP_UDP_CHKSUM	UINT32_C(0x1)
	/*
	 * If set to 1, the controller replaces the IP checksum of the normal
	 * packets, or the inner IP checksum of the encapsulated packets with
	 * the hardware calculated IP checksum for the packet associated with
	 * this descriptor. This bit must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_LFLAGS_IP_CHKSUM		UINT32_C(0x2)
	/*
	 * If set to 1, the controller will not append an Ethernet CRC to the
	 * end of the frame. This bit must be valid on the first BD of a packet.
	 * Packet must be 64B or longer when this flag is set. It is not useful
	 * to use this bit with any form of TX offload such as CSO or LSO. The
	 * intent is that the packet from the host already has a valid Ethernet
	 * CRC on the packet.
	 */
	#define TX_BD_LONG_LFLAGS_NOCRC			UINT32_C(0x4)
	/*
	 * If set to 1, the device will record the time at which the packet was
	 * actually transmitted at the TX MAC. This bit must be valid on the
	 * first BD of a packet.
	 */
	#define TX_BD_LONG_LFLAGS_STAMP			UINT32_C(0x8)
	/*
	 * If set to 1, The controller replaces the tunnel IP checksum field
	 * with hardware calculated IP checksum for the IP header of the packet
	 * associated with this descriptor. In case of VXLAN, the controller
	 * also replaces the outer header UDP checksum with hardware calculated
	 * UDP checksum for the packet associated with this descriptor.
	 */
	#define TX_BD_LONG_LFLAGS_T_IP_CHKSUM		UINT32_C(0x10)
	/*
	 * If set to 1, the device will treat this packet with LSO(Large Send
	 * Offload) processing for both normal or encapsulated packets, which is
	 * a form of TCP segmentation. When this bit is 1, the hdr_size and mss
	 * fields must be valid. The driver doesn't need to set t_ip_chksum,
	 * ip_chksum, and tcp_udp_chksum flags since the controller will replace
	 * the appropriate checksum fields for segmented packets. When this bit
	 * is 1, the hdr_size and mss fields must be valid.
	 */
	#define TX_BD_LONG_LFLAGS_LSO			UINT32_C(0x20)
	/*
	 * If set to zero when LSO is '1', then the IPID will be treated as a
	 * 16b number and will be wrapped if it exceeds a value of 0xffff. If
	 * set to one when LSO is '1', then the IPID will be treated as a 15b
	 * number and will be wrapped if it exceeds a value 0f 0x7fff.
	 */
	#define TX_BD_LONG_LFLAGS_IPID_FMT		UINT32_C(0x40)
	/*
	 * If set to zero when LSO is '1', then the IPID of the tunnel IP header
	 * will not be modified during LSO operations. If set to one when LSO is
	 * '1', then the IPID of the tunnel IP header will be incremented for
	 * each subsequent segment of an LSO operation.
	 */
	#define TX_BD_LONG_LFLAGS_T_IPID		UINT32_C(0x80)
	/*
	 * If set to '1', then the RoCE ICRC will be appended to the packet.
	 * Packet must be a valid RoCE format packet.
	 */
	#define TX_BD_LONG_LFLAGS_ROCE_CRC		UINT32_C(0x100)
	/*
	 * If set to '1', then the FCoE CRC will be appended to the packet.
	 * Packet must be a valid FCoE format packet.
	 */
	#define TX_BD_LONG_LFLAGS_FCOE_CRC		UINT32_C(0x200)
	uint16_t lflags;

	/*
	 * When LSO is '1', this field must contain the offset of the TCP
	 * payload from the beginning of the packet in as 16b words. In case of
	 * encapsulated/tunneling packet, this field contains the offset of the
	 * inner TCP payload from beginning of the packet as 16-bit words. This
	 * value must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_HDR_SIZE_MASK		UINT32_C(0x1ff)
	#define TX_BD_LONG_HDR_SIZE_SFT			0
	uint16_t hdr_size;

	/*
	 * This is the MSS value that will be used to do the LSO processing. The
	 * value is the length in bytes of the TCP payload for each segment
	 * generated by the LSO operation. This value must be valid on the first
	 * BD of a packet.
	 */
	#define TX_BD_LONG_MSS_MASK			UINT32_C(0x7fff)
	#define TX_BD_LONG_MSS_SFT			0
	uint32_t mss;

	uint16_t unused_2;

	/*
	 * This value selects a CFA action to perform on the packet. Set this
	 * value to zero if no CFA action is desired. This value must be valid
	 * on the first BD of a packet.
	 */
	uint16_t cfa_action;

	/*
	 * This value is action meta-data that defines CFA edit operations that
	 * are done in addition to any action editing.
	 */
	/* When key=1, This is the VLAN tag VID value. */
	#define TX_BD_LONG_CFA_META_VLAN_VID_MASK	UINT32_C(0xfff)
	#define TX_BD_LONG_CFA_META_VLAN_VID_SFT	0
	/* When key=1, This is the VLAN tag DE value. */
	#define TX_BD_LONG_CFA_META_VLAN_DE		UINT32_C(0x1000)
	/* When key=1, This is the VLAN tag PRI value. */
	#define TX_BD_LONG_CFA_META_VLAN_PRI_MASK	UINT32_C(0xe000)
	#define TX_BD_LONG_CFA_META_VLAN_PRI_SFT	13
	/* When key=1, This is the VLAN tag TPID select value. */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_MASK	UINT32_C(0x70000)
	#define TX_BD_LONG_CFA_META_VLAN_TPID_SFT	16
		/* 0x88a8 */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_TPID88A8	(UINT32_C(0x0) << 16)
		/* 0x8100 */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_TPID8100	(UINT32_C(0x1) << 16)
		/* 0x9100 */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_TPID9100	(UINT32_C(0x2) << 16)
		/* 0x9200 */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_TPID9200	(UINT32_C(0x3) << 16)
		/* 0x9300 */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_TPID9300	(UINT32_C(0x4) << 16)
		/* Value programmed in CFA VLANTPID register. */
	#define TX_BD_LONG_CFA_META_VLAN_TPID_TPIDCFG	(UINT32_C(0x5) << 16)
	#define TX_BD_LONG_CFA_META_VLAN_TPID_LAST \
					TX_BD_LONG_CFA_META_VLAN_TPID_TPIDCFG
	/* When key=1, This is the VLAN tag TPID select value. */
	#define TX_BD_LONG_CFA_META_VLAN_RESERVED_MASK	UINT32_C(0xff80000)
	#define TX_BD_LONG_CFA_META_VLAN_RESERVED_SFT	19
	/*
	 * This field identifies the type of edit to be performed on the packet.
	 * This value must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_CFA_META_KEY_MASK		UINT32_C(0xf0000000)
	#define TX_BD_LONG_CFA_META_KEY_SFT		28
		/* No editing */
	#define TX_BD_LONG_CFA_META_KEY_NONE		(UINT32_C(0x0) << 28)
		/*
		 * - meta[17:16] - TPID select value (0 = 0x8100). - meta[15:12]
		 * - PRI/DE value. - meta[11:0] - VID value.
		 */
	#define TX_BD_LONG_CFA_META_KEY_VLAN_TAG	(UINT32_C(0x1) << 28)
	#define TX_BD_LONG_CFA_META_KEY_LAST	TX_BD_LONG_CFA_META_KEY_VLAN_TAG
	uint32_t cfa_meta;
} __attribute__((packed));

/* RX Producer Packet BD (16 bytes) */
struct rx_prod_pkt_bd {
	/* This value identifies the type of buffer descriptor. */
	#define RX_PROD_PKT_BD_TYPE_MASK		UINT32_C(0x3f)
	#define RX_PROD_PKT_BD_TYPE_SFT			0
		/*
		 * Indicates that this BD is 16B long and is an RX Producer (ie.
		 * empty) buffer descriptor.
		 */
	#define RX_PROD_PKT_BD_TYPE_RX_PROD_PKT		(UINT32_C(0x4) << 0)
	/*
	 * If set to 1, the packet will be placed at the address plus 2B. The 2
	 * Bytes of padding will be written as zero.
	 */
	/*
	 * This is intended to be used when the host buffer is cache-line
	 * aligned to produce packets that are easy to parse in host memory
	 * while still allowing writes to be cache line aligned.
	 */
	#define RX_PROD_PKT_BD_FLAGS_SOP_PAD		UINT32_C(0x40)
	/*
	 * If set to 1, the packet write will be padded out to the nearest
	 * cache-line with zero value padding.
	 */
	/*
	 * If receive buffers start/end on cache-line boundaries, this feature
	 * will ensure that all data writes on the PCI bus start/end on cache
	 * line boundaries.
	 */
	#define RX_PROD_PKT_BD_FLAGS_EOP_PAD		UINT32_C(0x80)
	/*
	 * This value is the number of additional buffers in the ring that
	 * describe the buffer space to be consumed for the this packet. If the
	 * value is zero, then the packet must fit within the space described by
	 * this BD. If this value is 1 or more, it indicates how many additional
	 * "buffer" BDs are in the ring immediately following this BD to be used
	 * for the same network packet. Even if the packet to be placed does not
	 * need all the additional buffers, they will be consumed anyway.
	 */
	#define RX_PROD_PKT_BD_FLAGS_BUFFERS_MASK	UINT32_C(0x300)
	#define RX_PROD_PKT_BD_FLAGS_BUFFERS_SFT	8
	#define RX_PROD_PKT_BD_FLAGS_MASK		UINT32_C(0xffc0)
	#define RX_PROD_PKT_BD_FLAGS_SFT		6
	uint16_t flags_type;

	/*
	 * This is the length in Bytes of the host physical buffer where data
	 * for the packet may be placed in host memory.
	 */
	/*
	 * While this is a Byte resolution value, it is often advantageous to
	 * ensure that the buffers provided end on a host cache line.
	 */
	uint16_t len;

	/*
	 * The opaque data field is pass through to the completion and can be
	 * used for any data that the driver wants to associate with this
	 * receive buffer set.
	 */
	uint32_t opaque;

	/*
	 * This is the host physical address where data for the packet may by
	 * placed in host memory.
	 */
	/*
	 * While this is a Byte resolution value, it is often advantageous to
	 * ensure that the buffers provide start on a host cache line.
	 */
	uint64_t addr;
} __attribute__((packed));

/* Completion Ring Structures */
/* Note: This structure is used by the HWRM to communicate HWRM Error. */
/* Base Completion Record (16 bytes) */
struct cmpl_base {
	/* unused is 10 b */
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define CMPL_BASE_TYPE_MASK			UINT32_C(0x3f)
	#define CMPL_BASE_TYPE_SFT			0
		/* TX L2 completion: Completion of TX packet. Length = 16B */
	#define CMPL_BASE_TYPE_TX_L2			(UINT32_C(0x0) << 0)
		/*
		 * RX L2 completion: Completion of and L2 RX packet.
		 * Length = 32B
		*/
	#define CMPL_BASE_TYPE_RX_L2			(UINT32_C(0x11) << 0)
		/*
		 * RX Aggregation Buffer completion : Completion of an L2
		 * aggregation buffer in support of TPA, HDS, or Jumbo packet
		 * completion. Length = 16B
		 */
	#define CMPL_BASE_TYPE_RX_AGG			(UINT32_C(0x12) << 0)
		/*
		 * RX L2 TPA Start Completion: Completion at the beginning of a
		 * TPA operation. Length = 32B
		 */
	#define CMPL_BASE_TYPE_RX_TPA_START		(UINT32_C(0x13) << 0)
		/*
		 * RX L2 TPA End Completion: Completion at the end of a TPA
		 * operation. Length = 32B
		 */
	#define CMPL_BASE_TYPE_RX_TPA_END		(UINT32_C(0x15) << 0)
		/*
		 * Statistics Ejection Completion: Completion of statistics data
		 * ejection buffer. Length = 16B
		 */
	#define CMPL_BASE_TYPE_STAT_EJECT		(UINT32_C(0x1a) << 0)
		/* HWRM Command Completion: Completion of an HWRM command. */
	#define CMPL_BASE_TYPE_HWRM_DONE		(UINT32_C(0x20) << 0)
		/* Forwarded HWRM Request */
	#define CMPL_BASE_TYPE_HWRM_FWD_REQ		(UINT32_C(0x22) << 0)
		/* Forwarded HWRM Response */
	#define CMPL_BASE_TYPE_HWRM_FWD_RESP		(UINT32_C(0x24) << 0)
		/* HWRM Asynchronous Event Information */
	#define CMPL_BASE_TYPE_HWRM_ASYNC_EVENT		(UINT32_C(0x2e) << 0)
		/* CQ Notification */
	#define CMPL_BASE_TYPE_CQ_NOTIFICATION		(UINT32_C(0x30) << 0)
		/* SRQ Threshold Event */
	#define CMPL_BASE_TYPE_SRQ_EVENT		(UINT32_C(0x32) << 0)
		/* DBQ Threshold Event */
	#define CMPL_BASE_TYPE_DBQ_EVENT		(UINT32_C(0x34) << 0)
		/* QP Async Notification */
	#define CMPL_BASE_TYPE_QP_EVENT			(UINT32_C(0x38) << 0)
		/* Function Async Notification */
	#define CMPL_BASE_TYPE_FUNC_EVENT		(UINT32_C(0x3a) << 0)
	uint16_t type;

	uint16_t info1;
	uint32_t info2;

	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define CMPL_BASE_V				UINT32_C(0x1)
	/* info3 is 31 b */
	#define CMPL_BASE_INFO3_MASK			UINT32_C(0xfffffffe)
	#define CMPL_BASE_INFO3_SFT			1
	uint32_t info3_v;

	uint32_t info4;
} __attribute__((packed));

/* TX Completion Record (16 bytes) */
struct tx_cmpl {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define TX_CMPL_TYPE_MASK			UINT32_C(0x3f)
	#define TX_CMPL_TYPE_SFT			0
		/* TX L2 completion: Completion of TX packet. Length = 16B */
	#define TX_CMPL_TYPE_TX_L2			(UINT32_C(0x0) << 0)
	/*
	 * When this bit is '1', it indicates a packet that has an error of some
	 * type. Type of error is indicated in error_flags.
	 */
	#define TX_CMPL_FLAGS_ERROR			UINT32_C(0x40)
	/*
	 * When this bit is '1', it indicates that the packet completed was
	 * transmitted using the push acceleration data provided by the driver.
	 * When this bit is '0', it indicates that the packet had not push
	 * acceleration data written or was executed as a normal packet even
	 * though push data was provided.
	 */
	#define TX_CMPL_FLAGS_PUSH			UINT32_C(0x80)
	#define TX_CMPL_FLAGS_MASK			UINT32_C(0xffc0)
	#define TX_CMPL_FLAGS_SFT			6
	uint16_t flags_type;

	uint16_t unused_0;

	/*
	 * This is a copy of the opaque field from the first TX BD of this
	 * transmitted packet.
	 */
	uint32_t opaque;

	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define TX_CMPL_V				UINT32_C(0x1)
	/*
	 * This error indicates that there was some sort of problem with the BDs
	 * for the packet.
	 */
	#define TX_CMPL_ERRORS_BUFFER_ERROR_MASK	UINT32_C(0xe)
	#define TX_CMPL_ERRORS_BUFFER_ERROR_SFT		1
		/* No error */
	#define TX_CMPL_ERRORS_BUFFER_ERROR_NO_ERROR	(UINT32_C(0x0) << 1)
		/* Bad Format: BDs were not formatted correctly. */
	#define TX_CMPL_ERRORS_BUFFER_ERROR_BAD_FMT	(UINT32_C(0x2) << 1)
	#define TX_CMPL_ERRORS_BUFFER_ERROR_LAST \
					TX_CMPL_ERRORS_BUFFER_ERROR_BAD_FMT
	/*
	 * When this bit is '1', it indicates that the length of the packet was
	 * zero. No packet was transmitted.
	 */
	#define TX_CMPL_ERRORS_ZERO_LENGTH_PKT		UINT32_C(0x10)
	/*
	 * When this bit is '1', it indicates that the packet was longer than
	 * the programmed limit in TDI. No packet was transmitted.
	 */
	#define TX_CMPL_ERRORS_EXCESSIVE_BD_LENGTH	UINT32_C(0x20)
	/*
	 * When this bit is '1', it indicates that one or more of the BDs
	 * associated with this packet generated a PCI error. This probably
	 * means the address was not valid.
	 */
	#define TX_CMPL_ERRORS_DMA_ERROR		UINT32_C(0x40)
	/*
	 * When this bit is '1', it indicates that the packet was longer than
	 * indicated by the hint. No packet was transmitted.
	 */
	#define TX_CMPL_ERRORS_HINT_TOO_SHORT		UINT32_C(0x80)
	/*
	 * When this bit is '1', it indicates that the packet was dropped due to
	 * Poison TLP error on one or more of the TLPs in the PXP completion.
	 */
	#define TX_CMPL_ERRORS_POISON_TLP_ERROR		UINT32_C(0x100)
	#define TX_CMPL_ERRORS_MASK			UINT32_C(0xfffe)
	#define TX_CMPL_ERRORS_SFT			1
	uint16_t errors_v;

	uint16_t unused_1;
	uint32_t unused_2;
} __attribute__((packed)) tx_cmpl_t, *ptx_cmpl_t;

/* RX Packet Completion Record (32 bytes split to 2 16-byte struct) */
struct rx_pkt_cmpl {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define RX_PKT_CMPL_TYPE_MASK			UINT32_C(0x3f)
	#define RX_PKT_CMPL_TYPE_SFT			0
		/*
		 * RX L2 completion: Completion of and L2 RX packet.
		 * Length = 32B
		 */
	#define RX_PKT_CMPL_TYPE_RX_L2			(UINT32_C(0x11) << 0)
	/*
	 * When this bit is '1', it indicates a packet that has an error of some
	 * type. Type of error is indicated in error_flags.
	 */
	#define RX_PKT_CMPL_FLAGS_ERROR			UINT32_C(0x40)
	/* This field indicates how the packet was placed in the buffer. */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_MASK	UINT32_C(0x380)
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_SFT		7
		/* Normal: Packet was placed using normal algorithm. */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_NORMAL	(UINT32_C(0x0) << 7)
		/* Jumbo: Packet was placed using jumbo algorithm. */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_JUMBO	(UINT32_C(0x1) << 7)
		/*
		 * Header/Data Separation: Packet was placed using Header/Data
		 * separation algorithm. The separation location is indicated by
		 * the itype field.
		 */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_HDS		(UINT32_C(0x2) << 7)
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_LAST \
						RX_PKT_CMPL_FLAGS_PLACEMENT_HDS
	/* This bit is '1' if the RSS field in this completion is valid. */
	#define RX_PKT_CMPL_FLAGS_RSS_VALID		UINT32_C(0x400)
	/*
	 * This value indicates what the inner packet determined for the packet
	 * was.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_MASK		UINT32_C(0xf000)
	#define RX_PKT_CMPL_FLAGS_ITYPE_SFT		12
		/* Not Known: Indicates that the packet type was not known. */
	#define RX_PKT_CMPL_FLAGS_ITYPE_NOT_KNOWN	(UINT32_C(0x0) << 12)
		/*
		 * IP Packet: Indicates that the packet was an IP packet, but
		 * further classification was not possible.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_IP		(UINT32_C(0x1) << 12)
		/*
		 * TCP Packet: Indicates that the packet was IP and TCP. This
		 * indicates that the payload_offset field is valid.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_TCP		(UINT32_C(0x2) << 12)
		/*
		 * UDP Packet: Indicates that the packet was IP and UDP. This
		 * indicates that the payload_offset field is valid.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_UDP		(UINT32_C(0x3) << 12)
		/*
		 * FCoE Packet: Indicates that the packet was recognized as a
		 * FCoE. This also indicates that the payload_offset field is
		 * valid.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_FCOE		(UINT32_C(0x4) << 12)
		/*
		 * RoCE Packet: Indicates that the packet was recognized as a
		 * RoCE. This also indicates that the payload_offset field is
		 * valid.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_ROCE		(UINT32_C(0x5) << 12)
		/*
		 * ICMP Packet: Indicates that the packet was recognized as
		 * ICMP. This indicates that the payload_offset field is valid.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_ICMP		(UINT32_C(0x7) << 12)
		/*
		 * PtP packet wo/timestamp: Indicates that the packet was
		 * recognized as a PtP packet.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_PTP_WO_TIMESTAMP \
							(UINT32_C(0x8) << 12)
		/*
		 * PtP packet w/timestamp: Indicates that the packet was
		 * recognized as a PtP packet and that a timestamp was taken for
		 * the packet.
		 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP	(UINT32_C(0x9) << 12)
	#define RX_PKT_CMPL_FLAGS_ITYPE_LAST \
					RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP
	#define RX_PKT_CMPL_FLAGS_MASK			UINT32_C(0xffc0)
	#define RX_PKT_CMPL_FLAGS_SFT			6
	uint16_t flags_type;

	/*
	 * This is the length of the data for the packet stored in the buffer(s)
	 * identified by the opaque value. This includes the packet BD and any
	 * associated buffer BDs. This does not include the the length of any
	 * data places in aggregation BDs.
	 */
	uint16_t len;

	/*
	 * This is a copy of the opaque field from the RX BD this completion
	 * corresponds to.
	 */
	uint32_t opaque;

	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define RX_PKT_CMPL_V1				UINT32_C(0x1)
	/*
	 * This value is the number of aggregation buffers that follow this
	 * entry in the completion ring that are a part of this packet. If the
	 * value is zero, then the packet is completely contained in the buffer
	 * space provided for the packet in the RX ring.
	 */
	#define RX_PKT_CMPL_AGG_BUFS_MASK		UINT32_C(0x3e)
	#define RX_PKT_CMPL_AGG_BUFS_SFT		1
	uint8_t agg_bufs_v1;

	/*
	 * This is the RSS hash type for the packet. The value is packed
	 * {tuple_extrac_op[1:0],rss_profile_id[4:0],tuple_extrac_op[2]}.
	 */
	uint8_t rss_hash_type;

	/*
	 * This value indicates the offset from the beginning of the packet
	 * where the inner payload starts. This value is valid for TCP, UDP,
	 * FCoE, and RoCE packets.
	 */
	uint8_t payload_offset;

	uint8_t unused_1;

	/*
	 * This value is the RSS hash value calculated for the packet based on
	 * the mode bits and key value in the VNIC.
	 */
	uint32_t rss_hash;
} __attribute__((packed));

/* last 16 bytes of RX Packet Completion Record */
struct rx_pkt_cmpl_hi {
	/*
	 * This indicates that the ip checksum was calculated for the inner
	 * packet and that the ip_cs_error field indicates if there was an
	 * error.
	 */
	#define RX_PKT_CMPL_FLAGS2_IP_CS_CALC		UINT32_C(0x1)
	/*
	 * This indicates that the TCP, UDP or ICMP checksum was calculated for
	 * the inner packet and that the l4_cs_error field indicates if there
	 * was an error.
	 */
	#define RX_PKT_CMPL_FLAGS2_L4_CS_CALC		UINT32_C(0x2)
	/*
	 * This indicates that the ip checksum was calculated for the tunnel
	 * header and that the t_ip_cs_error field indicates if there was an
	 * error.
	 */
	#define RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC		UINT32_C(0x4)
	/*
	 * This indicates that the UDP checksum was calculated for the tunnel
	 * packet and that the t_l4_cs_error field indicates if there was an
	 * error.
	 */
	#define RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC		UINT32_C(0x8)
	/* This value indicates what format the metadata field is. */
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_MASK	UINT32_C(0xf0)
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_SFT	4
		/* No metadata informtaion. Value is zero. */
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_NONE	(UINT32_C(0x0) << 4)
		/*
		 * The metadata field contains the VLAN tag and TPID value. -
		 * metadata[11:0] contains the vlan VID value. - metadata[12]
		 * contains the vlan DE value. - metadata[15:13] contains the
		 * vlan PRI value. - metadata[31:16] contains the vlan TPID
		 * value.
		 */
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN	(UINT32_C(0x1) << 4)
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_LAST \
					RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN
	/*
	 * This field indicates the IP type for the inner-most IP header. A
	 * value of '0' indicates IPv4. A value of '1' indicates IPv6. This
	 * value is only valid if itype indicates a packet with an IP header.
	 */
	#define RX_PKT_CMPL_FLAGS2_IP_TYPE		UINT32_C(0x100)
	uint32_t flags2;

	/*
	 * This is data from the CFA block as indicated by the meta_format
	 * field.
	 */
	/* When meta_format=1, this value is the VLAN VID. */
	#define RX_PKT_CMPL_METADATA_VID_MASK		UINT32_C(0xfff)
	#define RX_PKT_CMPL_METADATA_VID_SFT		0
	/* When meta_format=1, this value is the VLAN DE. */
	#define RX_PKT_CMPL_METADATA_DE			UINT32_C(0x1000)
	/* When meta_format=1, this value is the VLAN PRI. */
	#define RX_PKT_CMPL_METADATA_PRI_MASK		UINT32_C(0xe000)
	#define RX_PKT_CMPL_METADATA_PRI_SFT		13
	/* When meta_format=1, this value is the VLAN TPID. */
	#define RX_PKT_CMPL_METADATA_TPID_MASK		UINT32_C(0xffff0000)
	#define RX_PKT_CMPL_METADATA_TPID_SFT		16
	uint32_t metadata;

	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define RX_PKT_CMPL_V2				UINT32_C(0x1)
	/*
	 * This error indicates that there was some sort of problem with the BDs
	 * for the packet that was found after part of the packet was already
	 * placed. The packet should be treated as invalid.
	 */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_MASK	UINT32_C(0xe)
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_SFT	1
		/* No buffer error */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_NO_BUFFER \
							(UINT32_C(0x0) << 1)
		/*
		 * Did Not Fit: Packet did not fit into packet buffer provided.
		 * For regular placement, this means the packet did not fit in
		 * the buffer provided. For HDS and jumbo placement, this means
		 * that the packet could not be placed into 7 physical buffers
		 * or less.
		 */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_DID_NOT_FIT \
							(UINT32_C(0x1) << 1)
		/*
		 * Not On Chip: All BDs needed for the packet were not on-chip
		 * when the packet arrived.
		 */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_NOT_ON_CHIP \
							(UINT32_C(0x2) << 1)
		/* Bad Format: BDs were not formatted correctly. */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_BAD_FORMAT \
							(UINT32_C(0x3) << 1)
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_LAST \
				RX_PKT_CMPL_ERRORS_BUFFER_ERROR_BAD_FORMAT
	/* This indicates that there was an error in the IP header checksum. */
	#define RX_PKT_CMPL_ERRORS_IP_CS_ERROR		UINT32_C(0x10)
	/*
	 * This indicates that there was an error in the TCP, UDP or ICMP
	 * checksum.
	 */
	#define RX_PKT_CMPL_ERRORS_L4_CS_ERROR		UINT32_C(0x20)
	/*
	 * This indicates that there was an error in the tunnel IP header
	 * checksum.
	 */
	#define RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR	UINT32_C(0x40)
	/* This indicates that there was an error in the tunnel UDP checksum. */
	#define RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR	UINT32_C(0x80)
	/*
	 * This indicates that there was a CRC error on either an FCoE or RoCE
	 * packet. The itype indicates the packet type.
	 */
	#define RX_PKT_CMPL_ERRORS_CRC_ERROR		UINT32_C(0x100)
	/*
	 * This indicates that there was an error in the tunnel portion of the
	 * packet when this field is non-zero.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_MASK	UINT32_C(0xe00)
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_SFT	9
		/*
		 * No additional error occurred on the tunnel portion of the
		 * packet of the packet does not have a tunnel.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_NO_ERROR	(UINT32_C(0x0) << 9)
		/*
		 * Indicates that IP header version does not match expectation
		 * from L2 Ethertype for IPv4 and IPv6 in the tunnel header.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_VERSION \
							(UINT32_C(0x1) << 9)
		/*
		 * Indicates that header length is out of range in the tunnel
		 * header. Valid for IPv4.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_HDR_LEN \
							(UINT32_C(0x2) << 9)
		/*
		 * Indicates that the physical packet is shorter than that
		 * claimed by the PPPoE header length for a tunnel PPPoE packet.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_TUNNEL_TOTAL_ERROR \
							(UINT32_C(0x3) << 9)
		/*
		 * Indicates that physical packet is shorter than that claimed
		 * by the tunnel l3 header length. Valid for IPv4, or IPv6
		 * tunnel packet packets.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_IP_TOTAL_ERROR \
							(UINT32_C(0x4) << 9)
		/*
		 * Indicates that the physical packet is shorter than that
		 * claimed by the tunnel UDP header length for a tunnel UDP
		 * packet that is not fragmented.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_UDP_TOTAL_ERROR \
							(UINT32_C(0x5) << 9)
		/*
		 * indicates that the IPv4 TTL or IPv6 hop limit check have
		 * failed (e.g. TTL = 0) in the tunnel header. Valid for IPv4,
		 * and IPv6.
		 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL \
							(UINT32_C(0x6) << 9)
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_LAST \
				RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL
	/*
	 * This indicates that there was an error in the inner portion of the
	 * packet when this field is non-zero.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_MASK	UINT32_C(0xf000)
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_SFT	12
		/*
		 * No additional error occurred on the tunnel portion of the
		 * packet of the packet does not have a tunnel.
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_NO_ERROR	(UINT32_C(0x0) << 12)
		/*
		 * Indicates that IP header version does not match expectation
		 * from L2 Ethertype for IPv4 and IPv6 or that option other than
		 * VFT was parsed on FCoE packet.
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_VERSION \
							(UINT32_C(0x1) << 12)
		/*
		 * indicates that header length is out of range. Valid for IPv4
		 * and RoCE
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_HDR_LEN \
							(UINT32_C(0x2) << 12)
		/*
		 * indicates that the IPv4 TTL or IPv6 hop limit check have
		 * failed (e.g. TTL = 0). Valid for IPv4, and IPv6
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_TTL	(UINT32_C(0x3) << 12)
		/*
		 * Indicates that physical packet is shorter than that claimed
		 * by the l3 header length. Valid for IPv4, IPv6 packet or RoCE
		 * packets.
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_IP_TOTAL_ERROR \
							(UINT32_C(0x4) << 12)
		/*
		 * Indicates that the physical packet is shorter than that
		 * claimed by the UDP header length for a UDP packet that is not
		 * fragmented.
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_UDP_TOTAL_ERROR \
							(UINT32_C(0x5) << 12)
		/*
		 * Indicates that TCP header length > IP payload. Valid for TCP
		 * packets only.
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN \
							(UINT32_C(0x6) << 12)
		/* Indicates that TCP header length < 5. Valid for TCP. */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN_TOO_SMALL \
							(UINT32_C(0x7) << 12)
		/*
		 * Indicates that TCP option headers result in a TCP header size
		 * that does not match data offset in TCP header. Valid for TCP.
		 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN \
							(UINT32_C(0x8) << 12)
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_LAST \
				RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN
	#define RX_PKT_CMPL_ERRORS_MASK			UINT32_C(0xfffe)
	#define RX_PKT_CMPL_ERRORS_SFT			1
	uint16_t errors_v2;

	/*
	 * This field identifies the CFA action rule that was used for this
	 * packet.
	 */
	uint16_t cfa_code;

	/*
	 * This value holds the reordering sequence number for the packet. If
	 * the reordering sequence is not valid, then this value is zero. The
	 * reordering domain for the packet is in the bottom 8 to 10b of the
	 * rss_hash value. The bottom 20b of this value contain the ordering
	 * domain value for the packet.
	 */
	#define RX_PKT_CMPL_REORDER_MASK		UINT32_C(0xffffff)
	#define RX_PKT_CMPL_REORDER_SFT			0
	uint32_t reorder;
} __attribute__((packed));

/* HWRM Forwarded Request (16 bytes) */
struct hwrm_fwd_req_cmpl {
	/* Length of forwarded request in bytes. */
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define HWRM_FWD_REQ_CMPL_TYPE_MASK		UINT32_C(0x3f)
	#define HWRM_FWD_REQ_CMPL_TYPE_SFT		0
		/* Forwarded HWRM Request */
	#define HWRM_FWD_REQ_CMPL_TYPE_HWRM_FWD_REQ	(UINT32_C(0x22) << 0)
	/* Length of forwarded request in bytes. */
	#define HWRM_FWD_REQ_CMPL_REQ_LEN_MASK		UINT32_C(0xffc0)
	#define HWRM_FWD_REQ_CMPL_REQ_LEN_SFT		6
	uint16_t req_len_type;

	/*
	 * Source ID of this request. Typically used in forwarding requests and
	 * responses. 0x0 - 0xFFF8 - Used for function ids 0xFFF8 - 0xFFFE -
	 * Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t source_id;

	uint32_t unused_0;

	/* Address of forwarded request. */
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define HWRM_FWD_REQ_CMPL_V			UINT32_C(0x1)
	/* Address of forwarded request. */
	#define HWRM_FWD_REQ_CMPL_REQ_BUF_ADDR_MASK	UINT32_C(0xfffffffe)
	#define HWRM_FWD_REQ_CMPL_REQ_BUF_ADDR_SFT	1
	uint64_t req_buf_addr_v;
} __attribute__((packed));

/* HWRM Asynchronous Event Completion Record (16 bytes) */
struct hwrm_async_event_cmpl {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_MASK		UINT32_C(0x3f)
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_SFT		0
		/* HWRM Asynchronous Event Information */
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_HWRM_ASYNC_EVENT \
							(UINT32_C(0x2e) << 0)
	uint16_t type;

	/* Identifiers of events. */
		/* Link status changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE \
							(UINT32_C(0x0) << 0)
		/* Link MTU changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_MTU_CHANGE \
							(UINT32_C(0x1) << 0)
		/* Link speed changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE \
							(UINT32_C(0x2) << 0)
		/* DCB Configuration changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE \
							(UINT32_C(0x3) << 0)
		/* Port connection not allowed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED \
							(UINT32_C(0x4) << 0)
		/* Link speed configuration was not allowed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_NOT_ALLOWED \
							(UINT32_C(0x5) << 0)
		/* Function driver unloaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_UNLOAD \
							(UINT32_C(0x10) << 0)
		/* Function driver loaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_LOAD \
							(UINT32_C(0x11) << 0)
		/* PF driver unloaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD \
							(UINT32_C(0x20) << 0)
		/* PF driver loaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_LOAD \
							(UINT32_C(0x21) << 0)
		/* VF Function Level Reset (FLR) */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_FLR	(UINT32_C(0x30) << 0)
		/* VF MAC Address Change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_MAC_ADDR_CHANGE \
							(UINT32_C(0x31) << 0)
		/* PF-VF communication channel status change. */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_VF_COMM_STATUS_CHANGE \
							(UINT32_C(0x32) << 0)
		/* HWRM Error */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_HWRM_ERROR \
							(UINT32_C(0xff) << 0)
	uint16_t event_id;

	/* Event specific data */
	uint32_t event_data2;

	/* opaque is 7 b */
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define HWRM_ASYNC_EVENT_CMPL_V				UINT32_C(0x1)
	/* opaque is 7 b */
	#define HWRM_ASYNC_EVENT_CMPL_OPAQUE_MASK		UINT32_C(0xfe)
	#define HWRM_ASYNC_EVENT_CMPL_OPAQUE_SFT		1
	uint8_t opaque_v;

	/* 8-lsb timestamp from POR (100-msec resolution) */
	uint8_t timestamp_lo;

	/* 16-lsb timestamp from POR (100-msec resolution) */
	uint16_t timestamp_hi;

	/* Event specific data */
	uint32_t event_data1;
} __attribute__((packed));

/*
 * Note: The Hardware Resource Manager (HWRM) manages various hardware resources
 * inside the chip. The HWRM is implemented in firmware, and runs on embedded
 * processors inside the chip. This firmware is vital part of the chip's
 * hardware. The chip can not be used by driver without it.
 */

/* Input (16 bytes) */
struct input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;
} __attribute__((packed));

/* Output (8 bytes) */
struct output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;
} __attribute__((packed));

/* hwrm_cfa_l2_filter_alloc */
/*
 * A filter is used to identify traffic that contains a matching set of
 * parameters like unicast or broadcast MAC address or a VLAN tag amongst
 * other things which then allows the ASIC to direct the  incoming traffic
 * to an appropriate VNIC or Rx ring.
 */

/* Input (96 bytes) */
struct hwrm_cfa_l2_filter_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both TX and RX
	 * paths of the chip.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH \
							UINT32_C(0x1)
		/* tx path */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_TX \
							(UINT32_C(0x0) << 0)
		/* rx path */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX \
							(UINT32_C(0x1) << 0)
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_LAST \
				HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX
	/*
	 * Setting of this flag indicates the applicability to the loopback
	 * path.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_LOOPBACK \
							UINT32_C(0x2)
	/*
	 * Setting of this flag indicates drop action. If this flag is not set,
	 * then it should be considered accept action.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_DROP \
							UINT32_C(0x4)
	/*
	 * If this flag is set, all t_l2_* fields are invalid and they should
	 * not be specified. If this flag is set, then l2_* fields refer to
	 * fields of outermost L2 header.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_OUTERMOST \
							UINT32_C(0x8)
	uint32_t flags;

	/* This bit must be '1' for the l2_addr field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR \
							UINT32_C(0x1)
	/* This bit must be '1' for the l2_addr_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK \
							UINT32_C(0x2)
	/* This bit must be '1' for the l2_ovlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN \
							UINT32_C(0x4)
	/* This bit must be '1' for the l2_ovlan_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN_MASK \
							UINT32_C(0x8)
	/* This bit must be '1' for the l2_ivlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN \
							UINT32_C(0x10)
	/* This bit must be '1' for the l2_ivlan_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN_MASK \
							UINT32_C(0x20)
	/* This bit must be '1' for the t_l2_addr field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_ADDR \
							UINT32_C(0x40)
	/*
	 * This bit must be '1' for the t_l2_addr_mask field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_ADDR_MASK \
							UINT32_C(0x80)
	/* This bit must be '1' for the t_l2_ovlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_OVLAN \
							UINT32_C(0x100)
	/*
	 * This bit must be '1' for the t_l2_ovlan_mask field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_OVLAN_MASK \
							UINT32_C(0x200)
	/* This bit must be '1' for the t_l2_ivlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_IVLAN \
							UINT32_C(0x400)
	/*
	 * This bit must be '1' for the t_l2_ivlan_mask field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_IVLAN_MASK \
							UINT32_C(0x800)
	/* This bit must be '1' for the src_type field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_TYPE \
							UINT32_C(0x1000)
	/* This bit must be '1' for the src_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_ID \
							UINT32_C(0x2000)
	/* This bit must be '1' for the tunnel_type field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_TUNNEL_TYPE \
							UINT32_C(0x4000)
	/* This bit must be '1' for the dst_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID \
							UINT32_C(0x8000)
	/*
	 * This bit must be '1' for the mirror_vnic_id field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID \
							UINT32_C(0x10000)
	uint32_t enables;

	/*
	 * This value sets the match value for the L2 MAC address. Destination
	 * MAC address for RX path. Source MAC address for TX path.
	 */
	uint8_t l2_addr[6];

	uint8_t unused_0;
	uint8_t unused_1;

	/*
	 * This value sets the mask value for the L2 address. A value of 0 will
	 * mask the corresponding bit from compare.
	 */
	uint8_t l2_addr_mask[6];

	/* This value sets VLAN ID value for outer VLAN. */
	uint16_t l2_ovlan;

	/*
	 * This value sets the mask value for the ovlan id. A value of 0 will
	 * mask the corresponding bit from compare.
	 */
	uint16_t l2_ovlan_mask;

	/* This value sets VLAN ID value for inner VLAN. */
	uint16_t l2_ivlan;

	/*
	 * This value sets the mask value for the ivlan id. A value of 0 will
	 * mask the corresponding bit from compare.
	 */
	uint16_t l2_ivlan_mask;

	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This value sets the match value for the tunnel L2 MAC address.
	 * Destination MAC address for RX path. Source MAC address for TX path.
	 */
	uint8_t t_l2_addr[6];

	uint8_t unused_4;
	uint8_t unused_5;

	/*
	 * This value sets the mask value for the tunnel L2 address. A value of
	 * 0 will mask the corresponding bit from compare.
	 */
	uint8_t t_l2_addr_mask[6];

	/* This value sets VLAN ID value for tunnel outer VLAN. */
	uint16_t t_l2_ovlan;

	/*
	 * This value sets the mask value for the tunnel ovlan id. A value of 0
	 * will mask the corresponding bit from compare.
	 */
	uint16_t t_l2_ovlan_mask;

	/* This value sets VLAN ID value for tunnel inner VLAN. */
	uint16_t t_l2_ivlan;

	/*
	 * This value sets the mask value for the tunnel ivlan id. A value of 0
	 * will mask the corresponding bit from compare.
	 */
	uint16_t t_l2_ivlan_mask;

	/* This value identifies the type of source of the packet. */
		/* Network port */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_NPORT \
							(UINT32_C(0x0) << 0)
		/* Physical function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_PF \
							(UINT32_C(0x1) << 0)
		/* Virtual function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_VF \
							(UINT32_C(0x2) << 0)
		/* Virtual NIC of a function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_VNIC \
							(UINT32_C(0x3) << 0)
		/* Embedded processor for CFA management */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_KONG \
							(UINT32_C(0x4) << 0)
		/* Embedded processor for OOB management */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_APE \
							(UINT32_C(0x5) << 0)
		/* Embedded processor for RoCE */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_BONO \
							(UINT32_C(0x6) << 0)
		/* Embedded processor for network proxy functions */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_TANG \
							(UINT32_C(0x7) << 0)
	uint8_t src_type;

	uint8_t unused_6;
	/*
	 * This value is the id of the source. For a network port, it represents
	 * port_id. For a physical function, it represents fid. For a virtual
	 * function, it represents vf_id. For a vnic, it represents vnic_id. For
	 * embedded processors, this id is not valid. Notes: 1. The function ID
	 * is implied if it src_id is not provided for a src_type that is either
	 */
	uint32_t src_id;

	/* Tunnel Type. */
		/* Non-tunnel */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NONTUNNEL \
							(UINT32_C(0x0) << 0)
		/* Virtual eXtensible Local Area Network (VXLAN) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_VXLAN \
							(UINT32_C(0x1) << 0)
		/*
		 * Network Virtualization Generic Routing Encapsulation (NVGRE)
		 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NVGRE \
							(UINT32_C(0x2) << 0)
		/*
		 * Generic Routing Encapsulation (GRE) inside Ethernet payload
		 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_L2GRE \
							(UINT32_C(0x3) << 0)
		/* IP in IP */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPIP \
							(UINT32_C(0x4) << 0)
		/* Generic Network Virtualization Encapsulation (Geneve) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_GENEVE \
							(UINT32_C(0x5) << 0)
		/* Multi-Protocol Lable Switching (MPLS) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_MPLS \
							(UINT32_C(0x6) << 0)
		/* Stateless Transport Tunnel (STT) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_STT \
							(UINT32_C(0x7) << 0)
		/*
		 * Generic Routing Encapsulation (GRE) inside IP datagram
		 * payload
		 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPGRE \
							(UINT32_C(0x8) << 0)
		/* Any tunneled traffic */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_ANYTUNNEL \
							(UINT32_C(0xff) << 0)
	uint8_t tunnel_type;

	uint8_t unused_7;

	/*
	 * If set, this value shall represent the Logical VNIC ID of the
	 * destination VNIC for the RX path and network port id of the
	 * destination port for the TX path.
	 */
	uint16_t dst_id;

	/* Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint16_t mirror_vnic_id;

	/*
	 * This hint is provided to help in placing the filter in the filter
	 * table.
	 */
		/* No preference */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_NO_PREFER \
							(UINT32_C(0x0) << 0)
		/* Above the given filter */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_ABOVE_FILTER \
							(UINT32_C(0x1) << 0)
		/* Below the given filter */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_BELOW_FILTER \
							(UINT32_C(0x2) << 0)
		/* As high as possible */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_MAX \
							(UINT32_C(0x3) << 0)
		/* As low as possible */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_MIN \
							(UINT32_C(0x4) << 0)
	uint8_t pri_hint;

	uint8_t unused_8;
	uint32_t unused_9;

	/*
	 * This is the ID of the filter that goes along with the pri_hint. This
	 * field is valid only for the following values. 1 - Above the given
	 * filter 2 - Below the given filter
	 */
	uint64_t l2_filter_id_hint;
} __attribute__((packed));

/* Output (24 bytes) */
struct hwrm_cfa_l2_filter_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * This value identifies a set of CFA data structures used for an L2
	 * context.
	 */
	uint64_t l2_filter_id;

	/*
	 * This is the ID of the flow associated with this filter. This value
	 * shall be used to match and associate the flow identifier returned in
	 * completion records. A value of 0xFFFFFFFF shall indicate no flow id.
	 */
	uint32_t flow_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_cfa_l2_filter_free */
/*
 * Description: Free a L2 filter. The HWRM shall free all associated filter
 * resources with the L2 filter.
 */

/* Input (24 bytes) */
struct hwrm_cfa_l2_filter_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * This value identifies a set of CFA data structures used for an L2
	 * context.
	 */
	uint64_t l2_filter_id;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_cfa_l2_filter_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_cfa_l2_set_rx_mask */
/* Description: This command will set rx mask of the function. */

/* Input (40 bytes) */
struct hwrm_cfa_l2_set_rx_mask_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* VNIC ID */
	uint32_t vnic_id;

	/* Reserved for future use. */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_RESERVED	UINT32_C(0x1)
	/*
	 * When this bit is '1', the function is requested to accept multi-cast
	 * packets specified by the multicast addr table.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST	UINT32_C(0x2)
	/*
	 * When this bit is '1', the function is requested to accept all multi-
	 * cast packets.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST	UINT32_C(0x4)
	/*
	 * When this bit is '1', the function is requested to accept broadcast
	 * packets.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST	UINT32_C(0x8)
	/*
	 * When this bit is '1', the function is requested to be put in the
	 * promiscuous mode. The HWRM should accept any function to set up
	 * promiscuous mode. The HWRM shall follow the semantics below for the
	 * promiscuous mode support. # When partitioning is not enabled on a
	 * port (i.e. single PF on the port), then the PF shall be allowed to be
	 * in the promiscuous mode. When the PF is in the promiscuous mode, then
	 * it shall receive all host bound traffic on that port. # When
	 * partitioning is enabled on a port (i.e. multiple PFs per port) and a
	 * PF on that port is in the promiscuous mode, then the PF receives all
	 * traffic within that partition as identified by a unique identifier
	 * for the PF (e.g. S-Tag). If a unique outer VLAN for the PF is
	 * specified, then the setting of promiscuous mode on that PF shall
	 * result in the PF receiving all host bound traffic with matching outer
	 * VLAN. # A VF shall can be set in the promiscuous mode. In the
	 * promiscuous mode, the VF does not receive any traffic unless a unique
	 * outer VLAN for the VF is specified. If a unique outer VLAN for the VF
	 * is specified, then the setting of promiscuous mode on that VF shall
	 * result in the VF receiving all host bound traffic with the matching
	 * outer VLAN. # The HWRM shall allow the setting of promiscuous mode on
	 * a function independently from the promiscuous mode settings on other
	 * functions.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS	UINT32_C(0x10)
	/*
	 * If this flag is set, the corresponding RX filters shall be set up to
	 * cover multicast/broadcast filters for the outermost Layer 2
	 * destination MAC address field.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_OUTERMOST	UINT32_C(0x20)
	uint32_t mask;

	/* This is the address for mcast address tbl. */
	uint64_t mc_tbl_addr;

	/*
	 * This value indicates how many entries in mc_tbl are valid. Each entry
	 * is 6 bytes.
	 */
	uint32_t num_mc_entries;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_cfa_l2_set_rx_mask_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_exec_fwd_resp */
/*
 * Description: This command is used to send an encapsulated request to the
 * HWRM. This command instructs the HWRM to execute the request and forward the
 * response of the encapsulated request to the location specified in the
 * original request that is encapsulated. The target id of this command shall be
 * set to 0xFFFF (HWRM). The response location in this command shall be used to
 * acknowledge the receipt of the encapsulated request and forwarding of the
 * response.
 */

/* Input (128 bytes) */
struct hwrm_exec_fwd_resp_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * This is an encapsulated request. This request should be executed by
	 * the HWRM and the response should be provided in the response buffer
	 * inside the encapsulated request.
	 */
	uint32_t encap_request[26];

	/*
	 * This value indicates the target id of the response to the
	 * encapsulated request. 0x0 - 0xFFF8 - Used for function ids 0xFFF8 -
	 * 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t encap_resp_target_id;

	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_exec_fwd_resp_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_qcaps */
/*
 * Description: This command returns capabilities of a function. The input FID
 * value is used to indicate what function is being queried. This allows a
 * physical function driver to query virtual functions that are children of the
 * physical function. The output FID value is needed to configure Rings and
 * MSI-X vectors so their DMA operations appear correctly on the PCI bus.
 */

/* Input (24 bytes) */
struct hwrm_func_qcaps_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * Function ID of the function that is being queried. 0xFF... (All Fs)
	 * if the query is for the requesting function.
	 */
	uint16_t fid;

	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (80 bytes) */
struct hwrm_func_qcaps_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * FID value. This value is used to identify operations on the PCI bus
	 * as belonging to a particular PCI function.
	 */
	uint16_t fid;

	/*
	 * Port ID of port that this function is associated with. Valid only for
	 * the PF. 0xFF... (All Fs) if this function is not associated with any
	 * port. 0xFF... (All Fs) if this function is called from a VF.
	 */
	uint16_t port_id;

	/* If 1, then Push mode is supported on this function. */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PUSH_MODE_SUPPORTED   UINT32_C(0x1)
	/*
	 * If 1, then the global MSI-X auto-masking is enabled for the device.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_GLOBAL_MSIX_AUTOMASKING \
								UINT32_C(0x2)
	/*
	 * If 1, then the Precision Time Protocol (PTP) processing is supported
	 * on this function. The HWRM should enable PTP on only a single
	 * Physical Function (PF) per port.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PTP_SUPPORTED         UINT32_C(0x4)
	uint32_t flags;

	/*
	 * This value is current MAC address configured for this function. A
	 * value of 00-00-00-00-00-00 indicates no MAC address is currently
	 * configured.
	 */
	uint8_t perm_mac_address[6];

	/*
	 * The maximum number of RSS/COS contexts that can be allocated to the
	 * function.
	 */
	uint16_t max_rsscos_ctx;

	/*
	 * The maximum number of completion rings that can be allocated to the
	 * function.
	 */
	uint16_t max_cmpl_rings;

	/*
	 * The maximum number of transmit rings that can be allocated to the
	 * function.
	 */
	uint16_t max_tx_rings;

	/*
	 * The maximum number of receive rings that can be allocated to the
	 * function.
	 */
	uint16_t max_rx_rings;

	/*
	 * The maximum number of L2 contexts that can be allocated to the
	 * function.
	 */
	uint16_t max_l2_ctxs;

	/* The maximum number of VNICs that can be allocated to the function. */
	uint16_t max_vnics;

	/*
	 * The identifier for the first VF enabled on a PF. This is valid only
	 * on the PF with SR-IOV enabled. 0xFF... (All Fs) if this command is
	 * called on a PF with SR-IOV disabled or on a VF.
	 */
	uint16_t first_vf_id;

	/*
	 * The maximum number of VFs that can be allocated to the function. This
	 * is valid only on the PF with SR-IOV enabled. 0xFF... (All Fs) if this
	 * command is called on a PF with SR-IOV disabled or on a VF.
	 */
	uint16_t max_vfs;

	/*
	 * The maximum number of statistic contexts that can be allocated to the
	 * function.
	 */
	uint16_t max_stat_ctx;

	/*
	 * The maximum number of Encapsulation records that can be offloaded by
	 * this function.
	 */
	uint32_t max_encap_records;

	/*
	 * The maximum number of decapsulation records that can be offloaded by
	 * this function.
	 */
	uint32_t max_decap_records;

	/*
	 * The maximum number of Exact Match (EM) flows that can be offloaded by
	 * this function on the TX side.
	 */
	uint32_t max_tx_em_flows;

	/*
	 * The maximum number of Wildcard Match (WM) flows that can be offloaded
	 * by this function on the TX side.
	 */
	uint32_t max_tx_wm_flows;

	/*
	 * The maximum number of Exact Match (EM) flows that can be offloaded by
	 * this function on the RX side.
	 */
	uint32_t max_rx_em_flows;

	/*
	 * The maximum number of Wildcard Match (WM) flows that can be offloaded
	 * by this function on the RX side.
	 */
	uint32_t max_rx_wm_flows;

	/*
	 * The maximum number of multicast filters that can be supported by this
	 * function on the RX side.
	 */
	uint32_t max_mcast_filters;

	/*
	 * The maximum value of flow_id that can be supported in completion
	 * records.
	 */
	uint32_t max_flow_id;

	/*
	 * The maximum number of HW ring groups that can be supported on this
	 * function.
	 */
	uint32_t max_hw_ring_grps;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_reset */
/*
 * Description: This command resets a hardware function (PCIe function) and
 * frees any resources used by the function. This command shall be initiated by
 * the driver after an FLR has occurred to prepare the function for re-use. This
 * command may also be initiated by a driver prior to doing it's own
 * configuration. This command puts the function into the reset state. In the
 * reset state, global and port related features of the chip are not available.
 */
/*
 * Note: This command will reset a function that has already been disabled or
 * idled. The command returns all the resources owned by the function so a new
 * driver may allocate and configure resources normally.
 */

/* Input (24 bytes) */
struct hwrm_func_reset_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* This bit must be '1' for the vf_id_valid field to be configured. */
	#define HWRM_FUNC_RESET_INPUT_ENABLES_VF_ID_VALID \
							UINT32_C(0x1)
	uint32_t enables;

	/*
	 * The ID of the VF that this PF is trying to reset. Only the parent PF
	 * shall be allowed to reset a child VF. A parent PF driver shall use
	 * this field only when a specific child VF is requested to be reset.
	 */
	uint16_t vf_id;

	/* This value indicates the level of a function reset. */
		/*
		 * Reset the caller function and its children VFs (if any). If
		 * no children functions exist, then reset the caller function
		 * only.
		 */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETALL \
							(UINT32_C(0x0) << 0)
		/* Reset the caller function only */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETME \
							(UINT32_C(0x1) << 0)
		/*
		 * Reset all children VFs of the caller function driver if the
		 * caller is a PF driver. It is an error to specify this level
		 * by a VF driver. It is an error to specify this level by a PF
		 * driver with no children VFs.
		 */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETCHILDREN \
							(UINT32_C(0x2) << 0)
		/*
		 * Reset a specific VF of the caller function driver if the
		 * caller is the parent PF driver. It is an error to specify
		 * this level by a VF driver. It is an error to specify this
		 * level by a PF driver that is not the parent of the VF that is
		 * being requested to reset.
		 */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETVF \
							(UINT32_C(0x3) << 0)
	uint8_t func_reset_level;

	uint8_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_func_reset_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_port_phy_cfg */
/*
 * Description: This command configures the PHY device for the port. It allows
 * setting of the most generic settings for the PHY. The HWRM shall complete
 * this command as soon as PHY settings are configured. They may not be applied
 * when the command response is provided. A VF driver shall not be allowed to
 * configure PHY using this command. In a network partition mode, a PF driver
 * shall not be allowed to configure PHY using this command.
 */

/* Input (56 bytes) */
struct hwrm_port_phy_cfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is set to '1', the PHY for the port shall be reset. #
	 * If this bit is set to 1, then the HWRM shall reset the PHY after
	 * applying PHY configuration changes specified in this command. # In
	 * order to guarantee that PHY configuration changes specified in this
	 * command take effect, the HWRM client should set this flag to 1. # If
	 * this bit is not set to 1, then the HWRM may reset the PHY depending
	 * on the current PHY configuration and settings specified in this
	 * command.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY            UINT32_C(0x1)
	/*
	 * When this bit is set to '1', the link shall be forced to be taken
	 * down. # When this bit is set to '1", all other command input settings
	 * related to the link speed shall be ignored. Once the link state is
	 * forced down, it can be explicitly cleared from that state by setting
	 * this flag to '0'. # If this flag is set to '0', then the link shall
	 * be cleared from forced down state if the link is in forced down
	 * state. There may be conditions (e.g. out-of-band or sideband
	 * configuration changes for the link) outside the scope of the HWRM
	 * implementation that may clear forced down link state.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE_LINK_DOWN      UINT32_C(0x2)
	/*
	 * When this bit is set to '1', the link shall be forced to the
	 * force_link_speed value. When this bit is set to '1', the HWRM client
	 * should not enable any of the auto negotiation related fields
	 * represented by auto_XXX fields in this command. When this bit is set
	 * to '1' and the HWRM client has enabled a auto_XXX field in this
	 * command, then the HWRM shall ignore the enabled auto_XXX field. When
	 * this bit is set to zero, the link shall be allowed to autoneg.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE                UINT32_C(0x4)
	/*
	 * When this bit is set to '1', the auto-negotiation process shall be
	 * restarted on the link.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG      UINT32_C(0x8)
	/*
	 * When this bit is set to '1', Energy Efficient Ethernet (EEE) is
	 * requested to be enabled on this link. If EEE is not supported on this
	 * port, then this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_ENABLE	UINT32_C(0x10)
	/*
	 * When this bit is set to '1', Energy Efficient Ethernet (EEE) is
	 * requested to be disabled on this link. If EEE is not supported on
	 * this port, then this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_DISABLE	UINT32_C(0x20)
	/*
	 * When this bit is set to '1' and EEE is enabled on this link, then TX
	 * LPI is requested to be enabled on the link. If EEE is not supported
	 * on this port, then this flag shall be ignored by the HWRM. If EEE is
	 * disabled on this port, then this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_TX_LPI	UINT32_C(0x40)
	uint32_t flags;

	/* This bit must be '1' for the auto_mode field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE          UINT32_C(0x1)
	/* This bit must be '1' for the auto_duplex field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_DUPLEX        UINT32_C(0x2)
	/* This bit must be '1' for the auto_pause field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE         UINT32_C(0x4)
	/*
	 * This bit must be '1' for the auto_link_speed field to be configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED    UINT32_C(0x8)
	/*
	 * This bit must be '1' for the auto_link_speed_mask field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK \
								UINT32_C(0x10)
	/* This bit must be '1' for the wirespeed field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_WIRESPEED	UINT32_C(0x20)
	/* This bit must be '1' for the lpbk field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_LPBK		UINT32_C(0x40)
	/* This bit must be '1' for the preemphasis field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_PREEMPHASIS	UINT32_C(0x80)
	/* This bit must be '1' for the force_pause field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAUSE	UINT32_C(0x100)
	/*
	 * This bit must be '1' for the eee_link_speed_mask field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_EEE_LINK_SPEED_MASK \
								UINT32_C(0x200)
	/* This bit must be '1' for the tx_lpi_timer field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_TX_LPI_TIMER	UINT32_C(0x400)
	uint32_t enables;

	/* Port ID of port that is to be configured. */
	uint16_t port_id;

	/*
	 * This is the speed that will be used if the force bit is '1'. If
	 * unsupported speed is selected, an error will be generated.
	 */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t force_link_speed;

	/*
	 * This value is used to identify what autoneg mode is used when the
	 * link speed is not being forced.
	 */
		/*
		 * Disable autoneg or autoneg disabled. No speeds are selected.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_NONE	(UINT32_C(0x0) << 0)
		/* Select all possible speeds for autoneg mode. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ALL_SPEEDS \
							(UINT32_C(0x1) << 0)
		/*
		 * Select only the auto_link_speed speed for autoneg mode. This
		 * mode has been DEPRECATED. An HWRM client should not use this
		 * mode.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ONE_SPEED \
							(UINT32_C(0x2) << 0)
		/*
		 * Select the auto_link_speed or any speed below that speed for
		 * autoneg. This mode has been DEPRECATED. An HWRM client should
		 * not use this mode.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ONE_OR_BELOW \
							(UINT32_C(0x3) << 0)
		/*
		 * Select the speeds based on the corresponding link speed mask
		 * value that is provided.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK \
							(UINT32_C(0x4) << 0)
	uint8_t auto_mode;

	/*
	 * This is the duplex setting that will be used if the autoneg_mode is
	 * "one_speed" or "one_or_below".
	 */
		/* Half Duplex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF \
							(UINT32_C(0x0) << 0)
		/* Full duplex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_FULL \
							(UINT32_C(0x1) << 0)
		/* Both Half and Full dupex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH \
							(UINT32_C(0x2) << 0)
	uint8_t auto_duplex;

	/*
	 * This value is used to configure the pause that will be used for
	 * autonegotiation. Add text on the usage of auto_pause and force_pause.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX              UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX              UINT32_C(0x2)
	/*
	 * When set to 1, the advertisement of pause is enabled. # When the
	 * auto_mode is not set to none and this flag is set to 1, then the
	 * auto_pause bits on this port are being advertised and autoneg pause
	 * results are being interpreted. # When the auto_mode is not set to
	 * none and this flag is set to 0, the pause is forced as indicated in
	 * force_pause, and also advertised as auto_pause bits, but the autoneg
	 * results are not interpreted since the pause configuration is being
	 * forced. # When the auto_mode is set to none and this flag is set to
	 * 1, auto_pause bits should be ignored and should be set to 0.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_AUTONEG_PAUSE   UINT32_C(0x4)
	uint8_t auto_pause;

	uint8_t unused_0;

	/*
	 * This is the speed that will be used if the autoneg_mode is
	 * "one_speed" or "one_or_below". If an unsupported speed is selected,
	 * an error will be generated.
	 */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t auto_link_speed;

	/*
	 * This is a mask of link speeds that will be used if autoneg_mode is
	 * "mask". If unsupported speed is enabled an error will be generated.
	 */
	/* 100Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MBHD \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB \
							UINT32_C(0x2)
	/* 1Gb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GBHD \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GB \
							UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2GB \
							UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2_5GB \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10GB \
							UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_20GB \
							UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_25GB \
							UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_40GB \
							UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_50GB \
							UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100GB \
							UINT32_C(0x800)
	/* 10Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10MBHD \
							UINT32_C(0x1000)
	/* 10Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10MB \
							UINT32_C(0x2000)
	uint16_t auto_link_speed_mask;

	/* This value controls the wirespeed feature. */
		/* Wirespeed feature is disabled. */
	#define HWRM_PORT_PHY_CFG_INPUT_WIRESPEED_OFF	(UINT32_C(0x0) << 0)
		/* Wirespeed feature is enabled. */
	#define HWRM_PORT_PHY_CFG_INPUT_WIRESPEED_ON	(UINT32_C(0x1) << 0)
	uint8_t wirespeed;

	/* This value controls the loopback setting for the PHY. */
		/* No loopback is selected. Normal operation. */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_NONE	(UINT32_C(0x0) << 0)
		/*
		 * The HW will be configured with local loopback such that host
		 * data is sent back to the host without modification.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_LOCAL	(UINT32_C(0x1) << 0)
		/*
		 * The HW will be configured with remote loopback such that port
		 * logic will send packets back out the transmitter that are
		 * received.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_REMOTE	(UINT32_C(0x2) << 0)
	uint8_t lpbk;

	/*
	 * This value is used to configure the pause that will be used for force
	 * mode.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX             UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX             UINT32_C(0x2)
	uint8_t force_pause;

	uint8_t unused_1;

	/*
	 * This value controls the pre-emphasis to be used for the link. Driver
	 * should not set this value (use enable.preemphasis = 0) unless driver
	 * is sure of setting. Normally HWRM FW will determine proper pre-
	 * emphasis.
	 */
	uint32_t preemphasis;

	/*
	 * Setting for link speed mask that is used to advertise speeds during
	 * autonegotiation when EEE is enabled. This field is valid only when
	 * EEE is enabled. The speeds specified in this field shall be a subset
	 * of speeds specified in auto_link_speed_mask. If EEE is enabled,then
	 * at least one speed shall be provided in this mask.
	 */
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD1  UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_100MB  UINT32_C(0x2)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD2  UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_1GB    UINT32_C(0x8)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD3 \
								UINT32_C(0x10)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD4 \
								UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_10GB \
								UINT32_C(0x40)
	uint16_t eee_link_speed_mask;

	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * Reuested setting of TX LPI timer in microseconds. This field is valid
	 * only when EEE is enabled and TX LPI is enabled.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_TX_LPI_TIMER_MASK \
							UINT32_C(0xffffff)
	#define HWRM_PORT_PHY_CFG_INPUT_TX_LPI_TIMER_SFT           0
	uint32_t tx_lpi_timer;

	uint32_t unused_4;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_port_phy_cfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_port_phy_qcfg */
/* Description: This command queries the PHY configuration for the port. */
/* Input (24 bytes) */

struct hwrm_port_phy_qcfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* Port ID of port that is to be queried. */
	uint16_t port_id;

	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (96 bytes) */
struct hwrm_port_phy_qcfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* This value indicates the current link status. */
		/* There is no link or cable detected. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_NO_LINK	(UINT32_C(0x0) << 0)
		/* There is no link, but a cable has been detected. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SIGNAL	(UINT32_C(0x1) << 0)
		/* There is a link. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK	(UINT32_C(0x2) << 0)
	uint8_t link;

	uint8_t unused_0;

	/* This value indicates the current link speed of the connection. */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t link_speed;

	/* This value is indicates the duplex of the current connection. */
		/* Half Duplex connection. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_HALF	(UINT32_C(0x0) << 0)
		/* Full duplex connection. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_FULL	(UINT32_C(0x1) << 0)
	uint8_t duplex;

	/*
	 * This value is used to indicate the current pause configuration. When
	 * autoneg is enabled, this value represents the autoneg results of
	 * pause configuration.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_RX	UINT32_C(0x2)
	uint8_t pause;

	/*
	 * The supported speeds for the port. This is a bit mask. For each speed
	 * that is supported, the corrresponding bit will be set to '1'.
	 */
	/* 100Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100MBHD \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100MB \
							UINT32_C(0x2)
	/* 1Gb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_1GBHD \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_1GB \
							UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_2GB \
							UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_2_5GB \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_10GB \
							UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_20GB \
							UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_25GB \
							UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_40GB \
							UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_50GB \
							UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100GB \
							UINT32_C(0x800)
	/* 10Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_10MBHD \
							UINT32_C(0x1000)
	/* 10Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_10MB \
							UINT32_C(0x2000)
	uint16_t support_speeds;

	/*
	 * Current setting of forced link speed. When the link speed is not
	 * being forced, this value shall be set to 0.
	 */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t force_link_speed;

	/* Current setting of auto negotiation mode. */
		/*
		 * Disable autoneg or autoneg disabled. No speeds are selected.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE \
							(UINT32_C(0x0) << 0)
		/* Select all possible speeds for autoneg mode. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_ALL_SPEEDS \
							(UINT32_C(0x1) << 0)
		/*
		 * Select only the auto_link_speed speed for autoneg mode. This
		 * mode has been DEPRECATED. An HWRM client should not use this
		 * mode.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_ONE_SPEED \
							(UINT32_C(0x2) << 0)
		/*
		 * Select the auto_link_speed or any speed below that speed for
		 * autoneg. This mode has been DEPRECATED. An HWRM client should
		 * not use this mode.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_ONE_OR_BELOW \
							(UINT32_C(0x3) << 0)
		/*
		 * Select the speeds based on the corresponding link speed mask
		 * value that is provided.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_SPEED_MASK \
							(UINT32_C(0x4) << 0)
	uint8_t auto_mode;

	/*
	 * Current setting of pause autonegotiation. Move autoneg_pause flag
	 * here.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_PAUSE_RX	UINT32_C(0x2)
	/*
	 * When set to 1, the advertisement of pause is enabled. # When the
	 * auto_mode is not set to none and this flag is set to 1, then the
	 * auto_pause bits on this port are being advertised and autoneg pause
	 * results are being interpreted. # When the auto_mode is not set to
	 * none and this flag is set to 0, the pause is forced as indicated in
	 * force_pause, and also advertised as auto_pause bits, but the autoneg
	 * results are not interpreted since the pause configuration is being
	 * forced. # When the auto_mode is set to none and this flag is set to
	 * 1, auto_pause bits should be ignored and should be set to 0.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_PAUSE_AUTONEG_PAUSE \
							UINT32_C(0x4)
	uint8_t auto_pause;

	/*
	 * Current setting for auto_link_speed. This field is only valid when
	 * auto_mode is set to "one_speed" or "one_or_below".
	 */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t auto_link_speed;

	/*
	 * Current setting for auto_link_speed_mask that is used to advertise
	 * speeds during autonegotiation. This field is only valid when
	 * auto_mode is set to "mask". The speeds specified in this field shall
	 * be a subset of supported speeds on this port.
	 */
	/* 100Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_100MBHD \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_100MB \
							UINT32_C(0x2)
	/* 1Gb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_1GBHD \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_1GB \
							UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_2GB \
							UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_2_5GB \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_10GB \
							UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_20GB \
							UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_25GB \
							UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_40GB \
							UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_50GB \
							UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_100GB \
							UINT32_C(0x800)
	/* 10Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_10MBHD \
							UINT32_C(0x1000)
	/* 10Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_10MB \
							UINT32_C(0x2000)
	uint16_t auto_link_speed_mask;

	/* Current setting for wirespeed. */
		/* Wirespeed feature is disabled. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_WIRESPEED_OFF	(UINT32_C(0x0) << 0)
		/* Wirespeed feature is enabled. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_WIRESPEED_ON	(UINT32_C(0x1) << 0)
	uint8_t wirespeed;

	/* Current setting for loopback. */
		/* No loopback is selected. Normal operation. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LPBK_NONE	(UINT32_C(0x0) << 0)
		/*
		 * The HW will be configured with local loopback such that host
		 * data is sent back to the host without modification.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LPBK_LOCAL	(UINT32_C(0x1) << 0)
		/*
		 * The HW will be configured with remote loopback such that port
		 * logic will send packets back out the transmitter that are
		 * received.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LPBK_REMOTE	(UINT32_C(0x2) << 0)
	uint8_t lpbk;

	/*
	 * Current setting of forced pause. When the pause configuration is not
	 * being forced, then this value shall be set to 0.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_PAUSE_TX \
							UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_PAUSE_RX \
							UINT32_C(0x2)
	uint8_t force_pause;

	/*
	 * This value indicates the current status of the optics module on this
	 * port.
	 */
		/* Module is inserted and accepted */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_NONE \
							(UINT32_C(0x0) << 0)
		/* Module is rejected and transmit side Laser is disabled. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_DISABLETX \
							(UINT32_C(0x1) << 0)
		/* Module mismatch warning. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_WARNINGMSG \
							(UINT32_C(0x2) << 0)
		/* Module is rejected and powered down. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_PWRDOWN \
							(UINT32_C(0x3) << 0)
		/* Module is not inserted. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_NOTINSERTED \
							(UINT32_C(0x4) << 0)
		/* Module status is not applicable. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_NOTAPPLICABLE \
							(UINT32_C(0xff) << 0)
	uint8_t module_status;

	/* Current setting for preemphasis. */
	uint32_t preemphasis;

	/* This field represents the major version of the PHY. */
	uint8_t phy_maj;

	/* This field represents the minor version of the PHY. */
	uint8_t phy_min;

	/* This field represents the build version of the PHY. */
	uint8_t phy_bld;

	/* This value represents a PHY type. */
		/* Unknown */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_UNKNOWN \
							(UINT32_C(0x0) << 0)
		/* BASE-CR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASECR \
							(UINT32_C(0x1) << 0)
		/* BASE-KR4 (Deprecated) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR4 \
							(UINT32_C(0x2) << 0)
		/* BASE-LR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASELR \
							(UINT32_C(0x3) << 0)
		/* BASE-SR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASESR \
							(UINT32_C(0x4) << 0)
		/* BASE-KR2 (Deprecated) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR2 \
							(UINT32_C(0x5) << 0)
		/* BASE-KX */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKX \
							(UINT32_C(0x6) << 0)
		/* BASE-KR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR \
							(UINT32_C(0x7) << 0)
		/* BASE-T */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET \
							(UINT32_C(0x8) << 0)
		/* EEE capable BASE-T */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASETE \
							(UINT32_C(0x9) << 0)
		/* SGMII connected external PHY */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_SGMIIEXTPHY \
							(UINT32_C(0xa) << 0)
	uint8_t phy_type;

	/* This value represents a media type. */
		/* Unknown */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_UNKNOWN \
							(UINT32_C(0x0) << 0)
		/* Twisted Pair */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_TP	(UINT32_C(0x1) << 0)
		/* Direct Attached Copper */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_DAC \
							(UINT32_C(0x2) << 0)
		/* Fiber */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_FIBRE \
							(UINT32_C(0x3) << 0)
	uint8_t media_type;

	/* This value represents a transceiver type. */
		/* PHY and MAC are in the same package */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_PKG_TYPE_XCVR_INTERNAL \
							(UINT32_C(0x1) << 0)
		/* PHY and MAC are in different packages */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_PKG_TYPE_XCVR_EXTERNAL \
							(UINT32_C(0x2) << 0)
	uint8_t xcvr_pkg_type;

	/*
	 * This field represents flags related to EEE configuration. These EEE
	 * configuration flags are valid only when the auto_mode is not set to
	 * none (in other words autonegotiation is enabled).
	 */
	/* This field represents PHY address. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_ADDR_MASK	UINT32_C(0x1f)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_ADDR_SFT	0
	/*
	 * When set to 1, Energy Efficient Ethernet (EEE) mode is enabled.
	 * Speeds for autoneg with EEE mode enabled are based on
	 * eee_link_speed_mask.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_EEE_ENABLED \
							UINT32_C(0x20)
	/*
	 * This flag is valid only when eee_enabled is set to 1. # If
	 * eee_enabled is set to 0, then EEE mode is disabled and this flag
	 * shall be ignored. # If eee_enabled is set to 1 and this flag is set
	 * to 1, then Energy Efficient Ethernet (EEE) mode is enabled and in
	 * use. # If eee_enabled is set to 1 and this flag is set to 0, then
	 * Energy Efficient Ethernet (EEE) mode is enabled but is currently not
	 * in use.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_EEE_ACTIVE \
							UINT32_C(0x40)
	/*
	 * This flag is valid only when eee_enabled is set to 1. # If
	 * eee_enabled is set to 0, then EEE mode is disabled and this flag
	 * shall be ignored. # If eee_enabled is set to 1 and this flag is set
	 * to 1, then Energy Efficient Ethernet (EEE) mode is enabled and TX LPI
	 * is enabled. # If eee_enabled is set to 1 and this flag is set to 0,
	 * then Energy Efficient Ethernet (EEE) mode is enabled but TX LPI is
	 * disabled.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_EEE_TX_LPI \
							UINT32_C(0x80)
	/*
	 * This field represents flags related to EEE configuration. These EEE
	 * configuration flags are valid only when the auto_mode is not set to
	 * none (in other words autonegotiation is enabled).
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_MASK \
							UINT32_C(0xe0)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_SFT	5
	uint8_t eee_config_phy_addr;

	/* Reserved field, set to 0 */
	/*
	 * When set to 1, the parallel detection is used to determine the speed
	 * of the link partner. Parallel detection is used when a
	 * autonegotiation capable device is connected to a link parter that is
	 * not capable of autonegotiation.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PARALLEL_DETECT \
							UINT32_C(0x1)
	/* Reserved field, set to 0 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_RESERVED_MASK	UINT32_C(0xfe)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_RESERVED_SFT	1
	uint8_t parallel_detect;

	/*
	 * The advertised speeds for the port by the link partner. Each
	 * advertised speed will be set to '1'.
	 */
	/* 100Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_100MBHD \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_100MB \
							UINT32_C(0x2)
	/* 1Gb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_1GBHD \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_1GB \
							UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_2GB \
							UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_2_5GB \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_10GB \
							UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_20GB \
							UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_25GB \
							UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_40GB \
							UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_50GB \
							UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_100GB \
							UINT32_C(0x800)
	/* 10Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_10MBHD \
							UINT32_C(0x1000)
	/* 10Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_10MB \
							UINT32_C(0x2000)
	uint16_t link_partner_adv_speeds;

	/*
	 * The advertised autoneg for the port by the link partner. This field
	 * is deprecated and should be set to 0.
	 */
		/*
		 * Disable autoneg or autoneg disabled. No speeds are selected.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_NONE \
							(UINT32_C(0x0) << 0)
		/* Select all possible speeds for autoneg mode. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_ALL_SPEEDS\
							(UINT32_C(0x1) << 0)
		/*
		 * Select only the auto_link_speed speed for autoneg mode. This
		 * mode has been DEPRECATED. An HWRM client should not use this
		 * mode.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_ONE_SPEED \
							(UINT32_C(0x2) << 0)
		/*
		 * Select the auto_link_speed or any speed below that speed for
		 * autoneg. This mode has been DEPRECATED. An HWRM client should
		 * not use this mode.
		 */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_ONE_OR_BELOW \
							(UINT32_C(0x3) << 0)
		/*
		 * Select the speeds based on the corresponding link speed mask
		 * value that is provided.
		 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_SPEED_MASK\
							(UINT32_C(0x4) << 0)
	uint8_t link_partner_adv_auto_mode;

	/* The advertised pause settings on the port by the link partner. */
	/*
	 * When this bit is '1', Generation of tx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_PAUSE_TX \
							UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_PAUSE_RX \
							UINT32_C(0x2)
	uint8_t link_partner_adv_pause;

	/*
	 * Current setting for link speed mask that is used to advertise speeds
	 * during autonegotiation when EEE is enabled. This field is valid only
	 * when eee_enabled flags is set to 1. The speeds specified in this
	 * field shall be a subset of speeds specified in auto_link_speed_mask.
	 */
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD1 \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_100MB \
							UINT32_C(0x2)
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD2 \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_1GB \
							UINT32_C(0x8)
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD3 \
							UINT32_C(0x10)
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD4 \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_10GB \
							UINT32_C(0x40)
	uint16_t adv_eee_link_speed_mask;

	/*
	 * Current setting for link speed mask that is advertised by the link
	 * partner when EEE is enabled. This field is valid only when
	 * eee_enabled flags is set to 1.
	 */
	/* Reserved */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_RSVD1 \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_100MB \
							UINT32_C(0x2)
	/* Reserved */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_RSVD2 \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_1GB \
							UINT32_C(0x8)
	/* Reserved */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_RSVD3 \
							UINT32_C(0x10)
	/* Reserved */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_RSVD4 \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_10GB \
							UINT32_C(0x40)
	uint16_t link_partner_adv_eee_link_speed_mask;

	/* This value represents transceiver identifier type. */
	/*
	 * Current setting of TX LPI timer in microseconds. This field is valid
	 * only when_eee_enabled flag is set to 1 and tx_lpi_enabled is set to
	 * 1.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_TX_LPI_TIMER_MASK \
							UINT32_C(0xffffff)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_TX_LPI_TIMER_SFT         0
	/* This value represents transceiver identifier type. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_MASK \
							UINT32_C(0xff000000)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_SFT \
							24
		/* Unknown */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_UNKNOWN \
							(UINT32_C(0x0) << 24)
		/* SFP/SFP+/SFP28 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_SFP \
							(UINT32_C(0x3) << 24)
		/* QSFP */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_QSFP \
							(UINT32_C(0xc) << 24)
		/* QSFP+ */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_QSFPPLUS \
							(UINT32_C(0xd) << 24)
		/* QSFP28 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_QSFP28 \
							(UINT32_C(0x11) << 24)
	uint32_t xcvr_identifier_type_tx_lpi_timer;

	uint32_t unused_1;

	/*
	 * Up to 16 bytes of null padded ASCII string representing PHY vendor.
	 * If the string is set to null, then the vendor name is not available.
	 */
	char phy_vendor_name[16];

	/*
	 * Up to 16 bytes of null padded ASCII string that identifies vendor
	 * specific part number of the PHY. If the string is set to null, then
	 * the vendor specific part number is not available.
	 */
	char phy_vendor_partnumber[16];

	uint32_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;
	uint8_t unused_5;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_ver_get */
/*
 * Description: This function is called by a driver to determine the HWRM
 * interface version supported by the HWRM firmware, the version of HWRM
 * firmware implementation, the name of HWRM firmware, the versions of other
 * embedded firmwares, and the names of other embedded firmwares, etc. Any
 * interface or firmware version with major = 0, minor = 0, and update = 0 shall
 * be considered an invalid version.
 */

/* Input (24 bytes) */
struct hwrm_ver_get_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * This field represents the major version of HWRM interface
	 * specification supported by the driver HWRM implementation. The
	 * interface major version is intended to change only when non backward
	 * compatible changes are made to the HWRM interface specification.
	 */
	uint8_t hwrm_intf_maj;

	/*
	 * This field represents the minor version of HWRM interface
	 * specification supported by the driver HWRM implementation. A change
	 * in interface minor version is used to reflect significant backward
	 * compatible modification to HWRM interface specification. This can be
	 * due to addition or removal of functionality. HWRM interface
	 * specifications with the same major version but different minor
	 * versions are compatible.
	 */
	uint8_t hwrm_intf_min;

	/*
	 * This field represents the update version of HWRM interface
	 * specification supported by the driver HWRM implementation. The
	 * interface update version is used to reflect minor changes or bug
	 * fixes to a released HWRM interface specification.
	 */
	uint8_t hwrm_intf_upd;

	uint8_t unused_0[5];
} __attribute__((packed));

/* Output (128 bytes) */
struct hwrm_ver_get_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * This field represents the major version of HWRM interface
	 * specification supported by the HWRM implementation. The interface
	 * major version is intended to change only when non backward compatible
	 * changes are made to the HWRM interface specification. A HWRM
	 * implementation that is compliant with this specification shall
	 * provide value of 1 in this field.
	 */
	uint8_t hwrm_intf_maj;

	/*
	 * This field represents the minor version of HWRM interface
	 * specification supported by the HWRM implementation. A change in
	 * interface minor version is used to reflect significant backward
	 * compatible modification to HWRM interface specification. This can be
	 * due to addition or removal of functionality. HWRM interface
	 * specifications with the same major version but different minor
	 * versions are compatible. A HWRM implementation that is compliant with
	 * this specification shall provide value of 0 in this field.
	 */
	uint8_t hwrm_intf_min;

	/*
	 * This field represents the update version of HWRM interface
	 * specification supported by the HWRM implementation. The interface
	 * update version is used to reflect minor changes or bug fixes to a
	 * released HWRM interface specification. A HWRM implementation that is
	 * compliant with this specification shall provide value of 1 in this
	 * field.
	 */
	uint8_t hwrm_intf_upd;

	uint8_t hwrm_intf_rsvd;

	/*
	 * This field represents the major version of HWRM firmware. A change in
	 * firmware major version represents a major firmware release.
	 */
	uint8_t hwrm_fw_maj;

	/*
	 * This field represents the minor version of HWRM firmware. A change in
	 * firmware minor version represents significant firmware functionality
	 * changes.
	 */
	uint8_t hwrm_fw_min;

	/*
	 * This field represents the build version of HWRM firmware. A change in
	 * firmware build version represents bug fixes to a released firmware.
	 */
	uint8_t hwrm_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version of the HWRM firmware.
	 */
	uint8_t hwrm_fw_rsvd;

	/*
	 * This field represents the major version of mgmt firmware. A change in
	 * major version represents a major release.
	 */
	uint8_t mgmt_fw_maj;

	/*
	 * This field represents the minor version of mgmt firmware. A change in
	 * minor version represents significant functionality changes.
	 */
	uint8_t mgmt_fw_min;

	/*
	 * This field represents the build version of mgmt firmware. A change in
	 * update version represents bug fixes.
	 */
	uint8_t mgmt_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version
	 */
	uint8_t mgmt_fw_rsvd;

	/*
	 * This field represents the major version of network control firmware.
	 * A change in major version represents a major release.
	 */
	uint8_t netctrl_fw_maj;

	/*
	 * This field represents the minor version of network control firmware.
	 * A change in minor version represents significant functionality
	 * changes.
	 */
	uint8_t netctrl_fw_min;

	/*
	 * This field represents the build version of network control firmware.
	 * A change in update version represents bug fixes.
	 */
	uint8_t netctrl_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version
	 */
	uint8_t netctrl_fw_rsvd;

	/*
	 * This field is reserved for future use. The responder should set it to
	 * 0. The requester should ignore this field.
	 */
	uint32_t reserved1;

	/*
	 * This field represents the major version of RoCE firmware. A change in
	 * major version represents a major release.
	 */
	uint8_t roce_fw_maj;

	/*
	 * This field represents the minor version of RoCE firmware. A change in
	 * minor version represents significant functionality changes.
	 */
	uint8_t roce_fw_min;

	/*
	 * This field represents the build version of RoCE firmware. A change in
	 * update version represents bug fixes.
	 */
	uint8_t roce_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version
	 */
	uint8_t roce_fw_rsvd;

	/*
	 * This field represents the name of HWRM FW (ASCII chars without NULL
	 * at the end).
	 */
	char hwrm_fw_name[16];

	/*
	 * This field represents the name of mgmt FW (ASCII chars without NULL
	 * at the end).
	 */
	char mgmt_fw_name[16];

	/*
	 * This field represents the name of network control firmware (ASCII
	 * chars without NULL at the end).
	 */
	char netctrl_fw_name[16];

	/*
	 * This field is reserved for future use. The responder should set it to
	 * 0. The requester should ignore this field.
	 */
	uint32_t reserved2[4];

	/*
	 * This field represents the name of RoCE FW (ASCII chars without NULL
	 * at the end).
	 */
	char roce_fw_name[16];

	/* This field returns the chip number. */
	uint16_t chip_num;

	/* This field returns the revision of chip. */
	uint8_t chip_rev;

	/* This field returns the chip metal number. */
	uint8_t chip_metal;

	/* This field returns the bond id of the chip. */
	uint8_t chip_bond_id;

	/*
	 * This value indicates the type of platform used for chip
	 * implementation.
	 */
	/* ASIC */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_ASIC \
							(UINT32_C(0x0) << 0)
	/* FPGA platform of the chip. */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_FPGA \
							(UINT32_C(0x1) << 0)
	/* Palladium platform of the chip. */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_PALLADIUM \
							(UINT32_C(0x2) << 0)
	uint8_t chip_platform_type;

	/*
	 * This field returns the maximum value of request window that is
	 * supported by the HWRM. The request window is mapped into device
	 * address space using MMIO.
	 */
	uint16_t max_req_win_len;

	/*
	 * This field returns the maximum value of response buffer in bytes. If
	 * a request specifies the response buffer length that is greater than
	 * this value, then the HWRM should fail it. The value of this field
	 * shall be 4KB or more.
	 */
	uint16_t max_resp_len;

	/*
	 * This field returns the default request timeout value in milliseconds.
	 */
	uint16_t def_req_timeout;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_queue_qportcfg */
/*
 * Description: This function is called by a driver to query queue configuration
 * of a port. # The HWRM shall at least advertise one queue with lossy service
 * profile. # The driver shall use this command to query queue ids before
 * configuring or using any queues. # If a service profile is not set for a
 * queue, then the driver shall not use that queue without configuring a service
 * profile for it. # If the driver is not allowed to configure service profiles,
 * then the driver shall only use queues for which service profiles are pre-
 * configured.
 */

/* Input (24 bytes) */
struct hwrm_queue_qportcfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both TX and RX
	 * paths of the chip.
	 */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH \
							UINT32_C(0x1)
		/* tx path */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX \
							(UINT32_C(0x0) << 0)
		/* rx path */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX \
							(UINT32_C(0x1) << 0)
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_LAST \
					HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX
	uint32_t flags;

	/*
	 * Port ID of port for which the queue configuration is being queried.
	 * This field is only required when sent by IPC.
	 */
	uint16_t port_id;

	uint16_t unused_0;
} __attribute__((packed));

/* hwrm_ring_alloc */
/*
 * Description: This command allocates and does basic preparation for a ring.
 */

/* Input (80 bytes) */
struct hwrm_ring_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* This bit must be '1' for the Reserved1 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED1		UINT32_C(0x1)
	/* This bit must be '1' for the Reserved2 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED2		UINT32_C(0x2)
	/* This bit must be '1' for the Reserved3 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED3		UINT32_C(0x4)
	/*
	 * This bit must be '1' for the stat_ctx_id_valid field to be
	 * configured.
	 */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID	UINT32_C(0x8)
	/* This bit must be '1' for the Reserved4 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED4		UINT32_C(0x10)
	/* This bit must be '1' for the max_bw_valid field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_MAX_BW_VALID	UINT32_C(0x20)
	uint32_t enables;

	/* Ring Type. */
		/* Completion Ring (CR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_CMPL	(UINT32_C(0x0) << 0)
		/* TX Ring (TR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_TX	(UINT32_C(0x1) << 0)
		/* RX Ring (RR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_RX	(UINT32_C(0x2) << 0)
	uint8_t ring_type;

	uint8_t unused_0;
	uint16_t unused_1;

	/* This value is a pointer to the page table for the Ring. */
	uint64_t page_tbl_addr;

	/* First Byte Offset of the first entry in the first page. */
	uint32_t fbo;

	/*
	 * Actual page size in 2^page_size. The supported range is increments in
	 * powers of 2 from 16 bytes to 1GB. - 4 = 16 B Page size is 16 B. - 12
	 * = 4 KB Page size is 4 KB. - 13 = 8 KB Page size is 8 KB. - 16 = 64 KB
	 * Page size is 64 KB. - 22 = 2 MB Page size is 2 MB. - 23 = 4 MB Page
	 * size is 4 MB. - 31 = 1 GB Page size is 1 GB.
	 */
	uint8_t page_size;

	/*
	 * This value indicates the depth of page table. For this version of the
	 * specification, value other than 0 or 1 shall be considered as an
	 * invalid value. When the page_tbl_depth = 0, then it is treated as a
	 * special case with the following. 1. FBO and page size fields are not
	 * valid. 2. page_tbl_addr is the physical address of the first element
	 * of the ring.
	 */
	uint8_t page_tbl_depth;

	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * Number of 16B units in the ring. Minimum size for a ring is 16 16B
	 * entries.
	 */
	uint32_t length;

	/*
	 * Logical ring number for the ring to be allocated. This value
	 * determines the position in the doorbell area where the update to the
	 * ring will be made. For completion rings, this value is also the MSI-X
	 * vector number for the function the completion ring is associated
	 * with.
	 */
	uint16_t logical_id;

	/*
	 * This field is used only when ring_type is a TX ring. This value
	 * indicates what completion ring the TX ring is associated with.
	 */
	uint16_t cmpl_ring_id;

	/*
	 * This field is used only when ring_type is a TX ring. This value
	 * indicates what CoS queue the TX ring is associated with.
	 */
	uint16_t queue_id;

	uint8_t unused_4;
	uint8_t unused_5;

	/* This field is reserved for the future use. It shall be set to 0. */
	uint32_t reserved1;
	/* This field is reserved for the future use. It shall be set to 0. */
	uint16_t reserved2;

	uint8_t unused_6;
	uint8_t unused_7;
	/* This field is reserved for the future use. It shall be set to 0. */
	uint32_t reserved3;

	/*
	 * This field is used only when ring_type is a TX ring. This input
	 * indicates what statistics context this ring should be associated
	 * with.
	 */
	uint32_t stat_ctx_id;

	/* This field is reserved for the future use. It shall be set to 0. */
	uint32_t reserved4;

	/*
	 * This field is used only when ring_type is a TX ring. Maximum BW
	 * allocated to this TX ring in Mbps. The HWRM will translate this value
	 * into byte counter and time interval used for this ring inside the
	 * device.
	 */
	uint32_t max_bw;

	/*
	 * This field is used only when ring_type is a Completion ring. This
	 * value indicates what interrupt mode should be used on this completion
	 * ring. Note: In the legacy interrupt mode, no more than 16 completion
	 * rings are allowed.
	 */
		/* Legacy INTA */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_LEGACY	(UINT32_C(0x0) << 0)
		/* Reserved */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_RSVD	(UINT32_C(0x1) << 0)
		/* MSI-X */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX	(UINT32_C(0x2) << 0)
		/* No Interrupt - Polled mode */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_POLL	(UINT32_C(0x3) << 0)
	uint8_t int_mode;

	uint8_t unused_8[3];
} __attribute__((packed));

/* Output (16 bytes) */

struct hwrm_ring_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* Physical number of ring allocated. */
	uint16_t ring_id;

	/* Logical number of ring allocated. */
	uint16_t logical_ring_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_ring_free */
/*
 * Description: This command is used to free a ring and associated resources.
 */
/* Input (24 bytes) */

struct hwrm_ring_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* Ring Type. */
		/* Completion Ring (CR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_CMPL	(UINT32_C(0x0) << 0)
		/* TX Ring (TR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_TX	(UINT32_C(0x1) << 0)
		/* RX Ring (RR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_RX	(UINT32_C(0x2) << 0)
	uint8_t ring_type;

	uint8_t unused_0;

	/* Physical number of ring allocated. */
	uint16_t ring_id;

	uint32_t unused_1;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_ring_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_ring_grp_alloc */
/*
 * Description: This API allocates and does basic preparation for a ring group.
 */

/* Input (24 bytes) */
struct hwrm_ring_grp_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* This value identifies the CR associated with the ring group. */
	uint16_t cr;

	/* This value identifies the main RR associated with the ring group. */
	uint16_t rr;

	/*
	 * This value identifies the aggregation RR associated with the ring
	 * group. If this value is 0xFF... (All Fs), then no Aggregation ring
	 * will be set.
	 */
	uint16_t ar;

	/*
	 * This value identifies the statistics context associated with the ring
	 * group.
	 */
	uint16_t sc;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_ring_grp_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * This is the ring group ID value. Use this value to program the
	 * default ring group for the VNIC or as table entries in an RSS/COS
	 * context.
	 */
	uint32_t ring_group_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_ring_grp_free */
/*
 * Description: This API frees a ring group and associated resources. # If a
 * ring in the ring group is reset or free, then the associated rings in the
 * ring group shall also be reset/free using hwrm_ring_free. # A function driver
 * shall always use hwrm_ring_grp_free after freeing all rings in a group. # As
 * a part of executing this command, the HWRM shall reset all associated ring
 * group resources.
 */

/* Input (24 bytes) */
struct hwrm_ring_grp_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* This is the ring group ID value. */
	uint32_t ring_group_id;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_ring_grp_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_stat_ctx_alloc */
/*
 * Description: This command allocates and does basic preparation for a stat
 * context.
 */

/* Input (32 bytes) */
struct hwrm_stat_ctx_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* This is the address for statistic block. */
	uint64_t stats_dma_addr;

	/*
	 * The statistic block update period in ms. e.g. 250ms, 500ms, 750ms,
	 * 1000ms.
	 */
	uint32_t update_period_ms;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_stat_ctx_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* This is the statistics context ID value. */
	uint32_t stat_ctx_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_stat_ctx_clr_stats */
/* Description: This command clears statistics of a context. */

/* Input (24 bytes) */
struct hwrm_stat_ctx_clr_stats_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* ID of the statistics context that is being queried. */
	uint32_t stat_ctx_id;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_stat_ctx_clr_stats_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_stat_ctx_free */
/* Description: This command is used to free a stat context. */
/* Input (24 bytes) */

struct hwrm_stat_ctx_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* ID of the statistics context that is being queried. */
	uint32_t stat_ctx_id;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */

struct hwrm_stat_ctx_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* This is the statistics context ID value. */
	uint32_t stat_ctx_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_vnic_alloc */
/*
 * Description: This VNIC is a resource in the RX side of the chip that is used
 * to represent a virtual host "interface". # At the time of VNIC allocation or
 * configuration, the function can specify whether it wants the requested VNIC
 * to be the default VNIC for the function or not. # If a function requests
 * allocation of a VNIC for the first time and a VNIC is successfully allocated
 * by the HWRM, then the HWRM shall make the allocated VNIC as the default VNIC
 * for that function. # The default VNIC shall be used for the default action
 * for a partition or function. # For each VNIC allocated on a function, a
 * mapping on the RX side to map the allocated VNIC to source virtual interface
 * shall be performed by the HWRM. This should be hidden to the function driver
 * requesting the VNIC allocation. This enables broadcast/multicast replication
 * with source knockout. # If multicast replication with source knockout is
 * enabled, then the internal VNIC to SVIF mapping data structures shall be
 * programmed at the time of VNIC allocation.
 */

/* Input (24 bytes) */
struct hwrm_vnic_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', this VNIC is requested to be the default VNIC
	 * for this function.
	 */
	#define HWRM_VNIC_ALLOC_INPUT_FLAGS_DEFAULT                UINT32_C(0x1)
	uint32_t flags;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* Logical vnic ID */
	uint32_t vnic_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_vnic_cfg */
/* Description: Configure the RX VNIC structure. */

/* Input (40 bytes) */
struct hwrm_vnic_cfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the VNIC is requested to be the default VNIC
	 * for the function.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_DEFAULT		UINT32_C(0x1)
	/*
	 * When this bit is '1', the VNIC is being configured to strip VLAN in
	 * the RX path. If set to '0', then VLAN stripping is disabled on this
	 * VNIC.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE	UINT32_C(0x2)
	/*
	 * When this bit is '1', the VNIC is being configured to buffer receive
	 * packets in the hardware until the host posts new receive buffers. If
	 * set to '0', then bd_stall is being configured to be disabled on this
	 * VNIC.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_BD_STALL_MODE		UINT32_C(0x4)
	/*
	 * When this bit is '1', the VNIC is being configured to receive both
	 * RoCE and non-RoCE traffic. If set to '0', then this VNIC is not
	 * configured to be operating in dual VNIC mode.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_ROCE_DUAL_VNIC_MODE	UINT32_C(0x8)
	/*
	 * When this flag is set to '1', the VNIC is requested to be configured
	 * to receive only RoCE traffic. If this flag is set to '0', then this
	 * flag shall be ignored by the HWRM. If roce_dual_vnic_mode flag is set
	 * to '1', then the HWRM client shall not set this flag to '1'.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_ROCE_ONLY_VNIC_MODE	UINT32_C(0x10)
	uint32_t flags;

	/* This bit must be '1' for the dflt_ring_grp field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP	UINT32_C(0x1)
	/* This bit must be '1' for the rss_rule field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE		UINT32_C(0x2)
	/* This bit must be '1' for the cos_rule field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_COS_RULE		UINT32_C(0x4)
	/* This bit must be '1' for the lb_rule field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_LB_RULE		UINT32_C(0x8)
	/* This bit must be '1' for the mru field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_MRU			UINT32_C(0x10)
	uint32_t enables;

	/* Logical vnic ID */
	uint16_t vnic_id;

	/*
	 * Default Completion ring for the VNIC. This ring will be chosen if
	 * packet does not match any RSS rules and if there is no COS rule.
	 */
	uint16_t dflt_ring_grp;

	/*
	 * RSS ID for RSS rule/table structure. 0xFF... (All Fs) if there is no
	 * RSS rule.
	 */
	uint16_t rss_rule;

	/*
	 * RSS ID for COS rule/table structure. 0xFF... (All Fs) if there is no
	 * COS rule.
	 */
	uint16_t cos_rule;

	/*
	 * RSS ID for load balancing rule/table structure. 0xFF... (All Fs) if
	 * there is no LB rule.
	 */
	uint16_t lb_rule;

	/*
	 * The maximum receive unit of the vnic. Each vnic is associated with a
	 * function. The vnic mru value overwrites the mru setting of the
	 * associated function. The HWRM shall make sure that vnic mru does not
	 * exceed the mru of the port the function is associated with.
	 */
	uint16_t mru;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_cfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_vnic_free */
/*
 * Description: Free a VNIC resource. Idle any resources associated with the
 * VNIC as well as the VNIC. Reset and release all resources associated with the
 * VNIC.
 */

/* Input (24 bytes) */
struct hwrm_vnic_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* Logical vnic ID */
	uint32_t vnic_id;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_vnic_rss_cfg */
/* Description: This function is used to enable RSS configuration. */

/* Input (48 bytes) */
struct hwrm_vnic_rss_cfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the RSS hash shall be computed over source and
	 * destination IPv4 addresses of IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4		UINT32_C(0x1)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv4 addresses and source/destination ports of
	 * TCP/IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4	UINT32_C(0x2)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv4 addresses and source/destination ports of
	 * UDP/IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4	UINT32_C(0x4)
	/*
	 * When this bit is '1', the RSS hash shall be computed over source and
	 * destination IPv4 addresses of IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6		UINT32_C(0x8)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv6 addresses and source/destination ports of
	 * TCP/IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6	UINT32_C(0x10)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv6 addresses and source/destination ports of
	 * UDP/IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6	UINT32_C(0x20)
	uint32_t hash_type;

	uint32_t unused_0;

	/* This is the address for rss ring group table */
	uint64_t ring_grp_tbl_addr;

	/* This is the address for rss hash key table */
	uint64_t hash_key_tbl_addr;

	/* Index to the rss indirection table. */
	uint16_t rss_ctx_idx;

	uint16_t unused_1[3];
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_rss_cfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* Input (16 bytes) */
struct hwrm_vnic_rss_cos_lb_ctx_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;
} __attribute__((packed));

/* Output (16 bytes) */

struct hwrm_vnic_rss_cos_lb_ctx_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* rss_cos_lb_ctx_id is 16 b */
	uint16_t rss_cos_lb_ctx_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_vnic_rss_cos_lb_ctx_free */
/* Description: This function can be used to free COS/Load Balance context. */
/* Input (24 bytes) */

struct hwrm_vnic_rss_cos_lb_ctx_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* rss_cos_lb_ctx_id is 16 b */
	uint16_t rss_cos_lb_ctx_id;

	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_rss_cos_lb_ctx_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* Output (32 bytes) */
struct hwrm_queue_qportcfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* The maximum number of queues that can be configured. */
	uint8_t max_configurable_queues;

	/* The maximum number of lossless queues that can be configured. */
	uint8_t max_configurable_lossless_queues;

	/*
	 * 0 - Not allowed. Non-zero - Allowed. If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure queues using
	 * hwrm_queue_cfg.
	 */
	uint8_t queue_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure queue buffers
	 * using hwrm_queue_buffers_cfg.
	 */
	uint8_t queue_buffers_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure PFC using
	 * hwrm_queue_pfcenable_cfg.
	 */
	uint8_t queue_pfcenable_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure Priority to CoS
	 * mapping using hwrm_queue_pri2cos_cfg.
	 */
	uint8_t queue_pri2cos_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure CoS Bandwidth
	 * configuration using hwrm_queue_cos2bw_cfg.
	 */
	uint8_t queue_cos2bw_cfg_allowed;

	/* ID of CoS Queue 0. FF - Invalid id */
	uint8_t queue_id0;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id0_service_profile;

	/* ID of CoS Queue 1. FF - Invalid id */
	uint8_t queue_id1;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id1_service_profile;

	/* ID of CoS Queue 2. FF - Invalid id */
	uint8_t queue_id2;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id2_service_profile;

	/* ID of CoS Queue 3. FF - Invalid id */
	uint8_t queue_id3;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id3_service_profile;

	/* ID of CoS Queue 4. FF - Invalid id */
	uint8_t queue_id4;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id4_service_profile;

	/* ID of CoS Queue 5. FF - Invalid id */
	uint8_t queue_id5;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id5_service_profile;

	/* ID of CoS Queue 6. FF - Invalid id */
	uint8_t queue_id6_service_profile;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id6;

	/* ID of CoS Queue 7. FF - Invalid id */
	uint8_t queue_id7;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id7_service_profile;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_drv_rgtr */
/*
 * Description: This command is used by the function driver to register its
 * information with the HWRM. A function driver shall implement this command. A
 * function driver shall use this command during the driver initialization right
 * after the HWRM version discovery and default ring resources allocation.
 */

/* Input (80 bytes) */
struct hwrm_func_drv_rgtr_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the function driver is requesting all requests
	 * from its children VF drivers to be forwarded to itself. This flag can
	 * only be set by the PF driver. If a VF driver sets this flag, it
	 * should be ignored by the HWRM.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FWD_ALL_MODE        UINT32_C(0x1)
	/*
	 * When this bit is '1', the function is requesting none of the requests
	 * from its children VF drivers to be forwarded to itself. This flag can
	 * only be set by the PF driver. If a VF driver sets this flag, it
	 * should be ignored by the HWRM.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FWD_NONE_MODE       UINT32_C(0x2)
	uint32_t flags;

	/* This bit must be '1' for the os_type field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_OS_TYPE           UINT32_C(0x1)
	/* This bit must be '1' for the ver field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER               UINT32_C(0x2)
	/* This bit must be '1' for the timestamp field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_TIMESTAMP         UINT32_C(0x4)
	/* This bit must be '1' for the vf_req_fwd field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VF_REQ_FWD        UINT32_C(0x8)
	/*
	 * This bit must be '1' for the async_event_fwd field to be configured.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD \
								UINT32_C(0x10)
	uint32_t enables;

	/* This value indicates the type of OS. */
		/* Unknown */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_UNKNOWN \
							(UINT32_C(0x0) << 0)
		/* Other OS not listed below. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_OTHER \
							(UINT32_C(0x1) << 0)
		/* MSDOS OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_MSDOS \
							(UINT32_C(0xe) << 0)
		/* Windows OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WINDOWS \
							(UINT32_C(0x12) << 0)
		/* Solaris OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_SOLARIS \
							(UINT32_C(0x1d) << 0)
		/* Linux OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_LINUX \
							(UINT32_C(0x24) << 0)
		/* FreeBSD OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_FREEBSD \
							(UINT32_C(0x2a) << 0)
		/* VMware ESXi OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_ESXI \
							(UINT32_C(0x68) << 0)
		/* Microsoft Windows 8 64-bit OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WIN864 \
							(UINT32_C(0x73) << 0)
		/* Microsoft Windows Server 2012 R2 OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WIN2012R2 \
							(UINT32_C(0x74) << 0)
	uint16_t os_type;

	/* This is the major version of the driver. */
	uint8_t ver_maj;

	/* This is the minor version of the driver. */
	uint8_t ver_min;

	/* This is the update version of the driver. */
	uint8_t ver_upd;

	uint8_t unused_0;
	uint16_t unused_1;

	/*
	 * This is a 32-bit timestamp provided by the driver for keep alive. The
	 * timestamp is in multiples of 1ms.
	 */
	uint32_t timestamp;

	uint32_t unused_2;

	/*
	 * This is a 256-bit bit mask provided by the PF driver for letting the
	 * HWRM know what commands issued by the VF driver to the HWRM should be
	 * forwarded to the PF driver. Nth bit refers to the Nth req_type.
	 * Setting Nth bit to 1 indicates that requests from the VF driver with
	 * req_type equal to N shall be forwarded to the parent PF driver. This
	 * field is not valid for the VF driver.
	 */
	uint32_t vf_req_fwd[8];

	/*
	 * This is a 256-bit bit mask provided by the function driver (PF or VF
	 * driver) to indicate the list of asynchronous event completions to be
	 * forwarded. Nth bit refers to the Nth event_id. Setting Nth bit to 1
	 * by the function driver shall result in the HWRM forwarding
	 * asynchronous event completion with event_id equal to N. If all bits
	 * are set to 0 (value of 0), then the HWRM shall not forward any
	 * asynchronous event completion to this function driver.
	 */
	uint32_t async_event_fwd[8];
} __attribute__((packed));

/* Output (16 bytes) */

struct hwrm_func_drv_rgtr_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_drv_unrgtr */
/*
 * Description: This command is used by the function driver to un register with
 * the HWRM. A function driver shall implement this command. A function driver
 * shall use this command during the driver unloading.
 */
/* Input (24 bytes) */

struct hwrm_func_drv_unrgtr_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the function driver is notifying the HWRM to
	 * prepare for the shutdown.
	 */
	#define HWRM_FUNC_DRV_UNRGTR_INPUT_FLAGS_PREPARE_FOR_SHUTDOWN \
							UINT32_C(0x1)
	uint32_t flags;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_func_drv_unrgtr_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

#endif
