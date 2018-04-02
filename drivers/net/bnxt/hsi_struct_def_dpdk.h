/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2018 Broadcom
 * All rights reserved.
 */

#ifndef _HSI_STRUCT_DEF_DPDK_
#define _HSI_STRUCT_DEF_DPDK_
/* HSI and HWRM Specification 1.8.2 */
#define HWRM_VERSION_MAJOR	1
#define HWRM_VERSION_MINOR	8
#define HWRM_VERSION_UPDATE	2

#define HWRM_VERSION_RSVD	0 /* non-zero means beta version */

#define HWRM_VERSION_STR	"1.8.2.0"
/*
 * Following is the signature for HWRM message field that indicates not
 * applicable	(All F's). Need to cast it the size of the field if needed.
 */
#define HWRM_NA_SIGNATURE	((uint32_t)(-1))
#define HWRM_MAX_REQ_LEN	(128)  /* hwrm_func_buf_rgtr */
#define HWRM_MAX_RESP_LEN	(280)  /* hwrm_selftest_qlist */
#define HW_HASH_INDEX_SIZE	 0x80	/* 7 bit indirection table index. */
#define HW_HASH_KEY_SIZE	40
#define HWRM_RESP_VALID_KEY	1 /* valid key for HWRM response */
#define HWRM_ROCE_SP_HSI_VERSION_MAJOR	1
#define HWRM_ROCE_SP_HSI_VERSION_MINOR	8
#define HWRM_ROCE_SP_HSI_VERSION_UPDATE	2

/*
 * Request types
 */
#define HWRM_VER_GET			(UINT32_C(0x0))
#define HWRM_FUNC_BUF_UNRGTR		(UINT32_C(0xe))
#define HWRM_FUNC_VF_CFG		(UINT32_C(0xf))
    /* Reserved for future use */
#define RESERVED1			(UINT32_C(0x10))
#define HWRM_FUNC_RESET			(UINT32_C(0x11))
#define HWRM_FUNC_GETFID		(UINT32_C(0x12))
#define HWRM_FUNC_VF_ALLOC		(UINT32_C(0x13))
#define HWRM_FUNC_VF_FREE		(UINT32_C(0x14))
#define HWRM_FUNC_QCAPS			(UINT32_C(0x15))
#define HWRM_FUNC_QCFG			(UINT32_C(0x16))
#define HWRM_FUNC_CFG			(UINT32_C(0x17))
#define HWRM_FUNC_QSTATS		(UINT32_C(0x18))
#define HWRM_FUNC_CLR_STATS		(UINT32_C(0x19))
#define HWRM_FUNC_DRV_UNRGTR		(UINT32_C(0x1a))
#define HWRM_FUNC_VF_RESC_FREE		(UINT32_C(0x1b))
#define HWRM_FUNC_VF_VNIC_IDS_QUERY	(UINT32_C(0x1c))
#define HWRM_FUNC_DRV_RGTR		(UINT32_C(0x1d))
#define HWRM_FUNC_DRV_QVER		(UINT32_C(0x1e))
#define HWRM_FUNC_BUF_RGTR		(UINT32_C(0x1f))
#define HWRM_PORT_PHY_CFG		(UINT32_C(0x20))
#define HWRM_PORT_MAC_CFG		(UINT32_C(0x21))
#define HWRM_PORT_QSTATS		(UINT32_C(0x23))
#define HWRM_PORT_LPBK_QSTATS		(UINT32_C(0x24))
#define HWRM_PORT_CLR_STATS		(UINT32_C(0x25))
#define HWRM_PORT_PHY_QCFG		(UINT32_C(0x27))
#define HWRM_PORT_MAC_QCFG		(UINT32_C(0x28))
#define HWRM_PORT_MAC_PTP_QCFG          (UINT32_C(0x29))
#define HWRM_PORT_PHY_QCAPS		(UINT32_C(0x2a))
#define HWRM_PORT_LED_CFG		(UINT32_C(0x2d))
#define HWRM_PORT_LED_QCFG		(UINT32_C(0x2e))
#define HWRM_PORT_LED_QCAPS		(UINT32_C(0x2f))
#define HWRM_QUEUE_QPORTCFG		(UINT32_C(0x30))
#define HWRM_QUEUE_QCFG			(UINT32_C(0x31))
#define HWRM_QUEUE_CFG			(UINT32_C(0x32))
#define HWRM_FUNC_VLAN_CFG		(UINT32_C(0x33))
#define HWRM_FUNC_VLAN_QCFG		(UINT32_C(0x34))
#define HWRM_QUEUE_PFCENABLE_QCFG	(UINT32_C(0x35))
#define HWRM_QUEUE_PFCENABLE_CFG	(UINT32_C(0x36))
#define HWRM_QUEUE_PRI2COS_QCFG		(UINT32_C(0x37))
#define HWRM_QUEUE_PRI2COS_CFG		(UINT32_C(0x38))
#define HWRM_QUEUE_COS2BW_QCFG		(UINT32_C(0x39))
#define HWRM_QUEUE_COS2BW_CFG		(UINT32_C(0x3a))
#define HWRM_VNIC_ALLOC			(UINT32_C(0x40))
#define HWRM_VNIC_ALLOC			(UINT32_C(0x40))
#define HWRM_VNIC_FREE			(UINT32_C(0x41))
#define HWRM_VNIC_CFG			(UINT32_C(0x42))
#define HWRM_VNIC_QCFG			(UINT32_C(0x43))
#define HWRM_VNIC_TPA_CFG		(UINT32_C(0x44))
#define HWRM_VNIC_RSS_CFG		(UINT32_C(0x46))
#define HWRM_VNIC_RSS_QCFG		(UINT32_C(0x47))
#define HWRM_VNIC_PLCMODES_CFG		(UINT32_C(0x48))
#define HWRM_VNIC_PLCMODES_QCFG		(UINT32_C(0x49))
#define HWRM_VNIC_QCAPS			(UINT32_C(0x4a))
#define HWRM_RING_ALLOC			(UINT32_C(0x50))
#define HWRM_RING_FREE			(UINT32_C(0x51))
#define HWRM_RING_CMPL_RING_QAGGINT_PARAMS	(UINT32_C(0x52))
#define HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS	(UINT32_C(0x53))
#define HWRM_RING_RESET			(UINT32_C(0x5e))
#define HWRM_RING_GRP_ALLOC		(UINT32_C(0x60))
#define HWRM_RING_GRP_FREE		(UINT32_C(0x61))
#define HWRM_VNIC_RSS_COS_LB_CTX_ALLOC	(UINT32_C(0x70))
#define HWRM_VNIC_RSS_COS_LB_CTX_FREE	(UINT32_C(0x71))
#define HWRM_CFA_L2_FILTER_ALLOC	(UINT32_C(0x90))
#define HWRM_CFA_L2_FILTER_FREE		(UINT32_C(0x91))
#define HWRM_CFA_L2_FILTER_CFG		(UINT32_C(0x92))
#define HWRM_CFA_L2_SET_RX_MASK		(UINT32_C(0x93))
    /* Reserved for future use */
#define HWRM_CFA_VLAN_ANTISPOOF_CFG	(UINT32_C(0x94))
#define HWRM_CFA_TUNNEL_FILTER_ALLOC	(UINT32_C(0x95))
#define HWRM_CFA_TUNNEL_FILTER_FREE	(UINT32_C(0x96))
#define HWRM_CFA_NTUPLE_FILTER_ALLOC	(UINT32_C(0x99))
#define HWRM_CFA_NTUPLE_FILTER_FREE	(UINT32_C(0x9a))
#define HWRM_CFA_NTUPLE_FILTER_CFG	(UINT32_C(0x9b))
#define HWRM_CFA_EM_FLOW_ALLOC		(UINT32_C(0x9c))
#define HWRM_CFA_EM_FLOW_FREE		(UINT32_C(0x9d))
#define HWRM_CFA_EM_FLOW_CFG		(UINT32_C(0x9e))
#define HWRM_TUNNEL_DST_PORT_QUERY	(UINT32_C(0xa0))
#define HWRM_TUNNEL_DST_PORT_ALLOC	(UINT32_C(0xa1))
#define HWRM_TUNNEL_DST_PORT_FREE	(UINT32_C(0xa2))
#define HWRM_STAT_CTX_ALLOC		(UINT32_C(0xb0))
#define HWRM_STAT_CTX_FREE		(UINT32_C(0xb1))
#define HWRM_STAT_CTX_QUERY		(UINT32_C(0xb2))
#define HWRM_STAT_CTX_CLR_STATS		(UINT32_C(0xb3))
#define HWRM_FW_RESET			(UINT32_C(0xc0))
#define HWRM_FW_QSTATUS			(UINT32_C(0xc1))
#define HWRM_EXEC_FWD_RESP		(UINT32_C(0xd0))
#define HWRM_REJECT_FWD_RESP		(UINT32_C(0xd1))
#define HWRM_FWD_RESP			(UINT32_C(0xd2))
#define HWRM_FWD_ASYNC_EVENT_CMPL	(UINT32_C(0xd3))
#define HWRM_TEMP_MONITOR_QUERY		(UINT32_C(0xe0))
#define HWRM_WOL_FILTER_ALLOC		(UINT32_C(0xf0))
#define HWRM_WOL_FILTER_FREE		(UINT32_C(0xf1))
#define HWRM_WOL_FILTER_QCFG		(UINT32_C(0xf2))
#define HWRM_WOL_REASON_QCFG		(UINT32_C(0xf3))
#define HWRM_DBG_DUMP			(UINT32_C(0xff14))
#define HWRM_NVM_VALIDATE_OPTION	(UINT32_C(0xffef))
#define HWRM_NVM_FLUSH			(UINT32_C(0xfff0))
#define HWRM_NVM_GET_VARIABLE		(UINT32_C(0xfff1))
#define HWRM_NVM_SET_VARIABLE		(UINT32_C(0xfff2))
#define HWRM_NVM_INSTALL_UPDATE		(UINT32_C(0xfff3))
#define HWRM_NVM_MODIFY			(UINT32_C(0xfff4))
#define HWRM_NVM_VERIFY_UPDATE		(UINT32_C(0xfff5))
#define HWRM_NVM_GET_DEV_INFO		(UINT32_C(0xfff6))
#define HWRM_NVM_ERASE_DIR_ENTRY	(UINT32_C(0xfff7))
#define HWRM_NVM_MOD_DIR_ENTRY		(UINT32_C(0xfff8))
#define HWRM_NVM_FIND_DIR_ENTRY		(UINT32_C(0xfff9))
#define HWRM_NVM_GET_DIR_ENTRIES	(UINT32_C(0xfffa))
#define HWRM_NVM_GET_DIR_INFO		(UINT32_C(0xfffb))
#define HWRM_NVM_RAW_DUMP		(UINT32_C(0xfffc))
#define HWRM_NVM_READ			(UINT32_C(0xfffd))
#define HWRM_NVM_WRITE			(UINT32_C(0xfffe))
#define HWRM_NVM_RAW_WRITE_BLK		(UINT32_C(0xffff))

/*
 * Note: The Host Software Interface (HSI) and Hardware Resource Manager (HWRM)
 * specification describes the data structures used in Ethernet packet or RDMA
 * message data transfers as well as an abstract interface for managing Ethernet
 * NIC hardware resources.
 */
/* Ethernet Data path Host Structures */
/*
 * Description: The following three sections document the host structures used
 * between device and software drivers for communicating Ethernet packets.
 */
/* BD Ring Structures */
/*
 * Description: This structure is used to inform the NIC of a location for and
 * an aggregation buffer that will be used for packet data that is received. An
 * aggregation buffer creates a different kind of completion operation for a
 * packet where a variable number of BDs may be used to place the packet in the
 * host. RX Rings that have aggregation buffers are known as aggregation rings
 * and must contain only aggregation buffers.
 */
/* Short TX BD	(16 bytes) */
struct tx_bd_short {
	uint16_t flags_type;
	/*
	 * All bits in this field must be valid on the first BD of a
	 * packet. Only the packet_end bit must be valid for the
	 * remaining BDs of a packet.
	 */
	/* This value identifies the type of buffer descriptor. */
	#define TX_BD_SHORT_TYPE_MASK	UINT32_C(0x3f)
	#define TX_BD_SHORT_TYPE_SFT	0
	/*
	 * Indicates that this BD is 16B long and is
	 * used for normal L2 packet transmission.
	 */
	#define TX_BD_SHORT_TYPE_TX_BD_SHORT	UINT32_C(0x0)
	/*
	 * If set to 1, the packet ends with the data in the buffer
	 * pointed to by this descriptor. This flag must be valid on
	 * every BD.
	 */
	#define TX_BD_SHORT_FLAGS_PACKET_END	UINT32_C(0x40)
	/*
	 * If set to 1, the device will not generate a completion for
	 * this transmit packet unless there is an error in it's
	 * processing. If this bit is set to 0, then the packet will be
	 * completed normally. This bit must be valid only on the first
	 * BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_NO_CMPL	UINT32_C(0x80)
	/*
	 * This value indicates how many 16B BD locations are consumed
	 * in the ring by this packet. A value of 1 indicates that this
	 * BD is the only BD	(and that the it is a short BD). A value of
	 * 3 indicates either 3 short BDs or 1 long BD and one short BD
	 * in the packet. A value of 0 indicates that there are 32 BD
	 * locations in the packet	(the maximum). This field is valid
	 * only on the first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_BD_CNT_MASK	UINT32_C(0x1f00)
	#define TX_BD_SHORT_FLAGS_BD_CNT_SFT	8
	/*
	 * This value is a hint for the length of the entire packet. It
	 * is used by the chip to optimize internal processing. The
	 * packet will be dropped if the hint is too short. This field
	 * is valid only on the first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_LHINT_MASK	UINT32_C(0x6000)
	#define TX_BD_SHORT_FLAGS_LHINT_SFT	13
	/* indicates packet length < 512B */
	#define TX_BD_SHORT_FLAGS_LHINT_LT512	(UINT32_C(0x0) << 13)
	/* indicates 512 <= packet length < 1KB */
	#define TX_BD_SHORT_FLAGS_LHINT_LT1K	(UINT32_C(0x1) << 13)
	/* indicates 1KB <= packet length < 2KB */
	#define TX_BD_SHORT_FLAGS_LHINT_LT2K	(UINT32_C(0x2) << 13)
	/* indicates packet length >= 2KB */
	#define TX_BD_SHORT_FLAGS_LHINT_GTE2K	(UINT32_C(0x3) << 13)
	#define TX_BD_SHORT_FLAGS_LHINT_LAST \
		TX_BD_SHORT_FLAGS_LHINT_GTE2K
	/*
	 * If set to 1, the device immediately updates the Send Consumer
	 * Index after the buffer associated with this descriptor has
	 * been transferred via DMA to NIC memory from host memory. An
	 * interrupt may or may not be generated according to the state
	 * of the interrupt avoidance mechanisms. If this bit is set to
	 * 0, then the Consumer Index is only updated as soon as one of
	 * the host interrupt coalescing conditions has been met. This
	 * bit must be valid on the first BD of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_COAL_NOW	UINT32_C(0x8000)
	/*
	 * All bits in this field must be valid on the first BD of a
	 * packet. Only the packet_end bit must be valid for the
	 * remaining BDs of a packet.
	 */
	#define TX_BD_SHORT_FLAGS_MASK	UINT32_C(0xffc0)
	#define TX_BD_SHORT_FLAGS_SFT	6
	uint16_t len;
	/*
	 * This is the length of the host physical buffer this BD
	 * describes in bytes. This field must be valid on all BDs of a
	 * packet.
	 */
	uint32_t opaque;
	/*
	 * The opaque data field is pass through to the completion and
	 * can be used for any data that the driver wants to associate
	 * with the transmit BD. This field must be valid on the first
	 * BD of a packet.
	 */
	uint64_t addr;
	/*
	 * This is the host physical address for the portion of the
	 * packet described by this TX BD. This value must be valid on
	 * all BDs of a packet.
	 */
} __attribute__((packed));

/* Long TX BD	(32 bytes split to 2 16-byte struct) */
struct tx_bd_long {
	uint16_t flags_type;
	/*
	 * All bits in this field must be valid on the first BD of a
	 * packet. Only the packet_end bit must be valid for the
	 * remaining BDs of a packet.
	 */
	/* This value identifies the type of buffer descriptor. */
	#define TX_BD_LONG_TYPE_MASK	UINT32_C(0x3f)
	#define TX_BD_LONG_TYPE_SFT	0
	/*
	 * Indicates that this BD is 32B long and is
	 * used for normal L2 packet transmission.
	 */
	#define TX_BD_LONG_TYPE_TX_BD_LONG	UINT32_C(0x10)
	/*
	 * If set to 1, the packet ends with the data in the buffer
	 * pointed to by this descriptor. This flag must be valid on
	 * every BD.
	 */
	#define TX_BD_LONG_FLAGS_PACKET_END	UINT32_C(0x40)
	/*
	 * If set to 1, the device will not generate a completion for
	 * this transmit packet unless there is an error in it's
	 * processing. If this bit is set to 0, then the packet will be
	 * completed normally. This bit must be valid only on the first
	 * BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_NO_CMPL	UINT32_C(0x80)
	/*
	 * This value indicates how many 16B BD locations are consumed
	 * in the ring by this packet. A value of 1 indicates that this
	 * BD is the only BD	(and that the it is a short BD). A value of
	 * 3 indicates either 3 short BDs or 1 long BD and one short BD
	 * in the packet. A value of 0 indicates that there are 32 BD
	 * locations in the packet	(the maximum). This field is valid
	 * only on the first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_BD_CNT_MASK	UINT32_C(0x1f00)
	#define TX_BD_LONG_FLAGS_BD_CNT_SFT	8
	/*
	 * This value is a hint for the length of the entire packet. It
	 * is used by the chip to optimize internal processing. The
	 * packet will be dropped if the hint is too short. This field
	 * is valid only on the first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_LHINT_MASK	UINT32_C(0x6000)
	#define TX_BD_LONG_FLAGS_LHINT_SFT	13
	/* indicates packet length < 512B */
	#define TX_BD_LONG_FLAGS_LHINT_LT512	(UINT32_C(0x0) << 13)
	/* indicates 512 <= packet length < 1KB */
	#define TX_BD_LONG_FLAGS_LHINT_LT1K	(UINT32_C(0x1) << 13)
	/* indicates 1KB <= packet length < 2KB */
	#define TX_BD_LONG_FLAGS_LHINT_LT2K	(UINT32_C(0x2) << 13)
	/* indicates packet length >= 2KB */
	#define TX_BD_LONG_FLAGS_LHINT_GTE2K	(UINT32_C(0x3) << 13)
	#define TX_BD_LONG_FLAGS_LHINT_LAST \
		TX_BD_LONG_FLAGS_LHINT_GTE2K
	/*
	 * If set to 1, the device immediately updates the Send Consumer
	 * Index after the buffer associated with this descriptor has
	 * been transferred via DMA to NIC memory from host memory. An
	 * interrupt may or may not be generated according to the state
	 * of the interrupt avoidance mechanisms. If this bit is set to
	 * 0, then the Consumer Index is only updated as soon as one of
	 * the host interrupt coalescing conditions has been met. This
	 * bit must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_FLAGS_COAL_NOW	UINT32_C(0x8000)
	/*
	 * All bits in this field must be valid on the first BD of a
	 * packet. Only the packet_end bit must be valid for the
	 * remaining BDs of a packet.
	 */
	#define TX_BD_LONG_FLAGS_MASK	UINT32_C(0xffc0)
	#define TX_BD_LONG_FLAGS_SFT	6
	uint16_t len;
	/*
	 * This is the length of the host physical buffer this BD
	 * describes in bytes. This field must be valid on all BDs of a
	 * packet.
	 */
	uint32_t opaque;
	/*
	 * The opaque data field is pass through to the completion and
	 * can be used for any data that the driver wants to associate
	 * with the transmit BD. This field must be valid on the first
	 * BD of a packet.
	 */
	uint64_t addr;
	/*
	 * This is the host physical address for the portion of the
	 * packet described by this TX BD. This value must be valid on
	 * all BDs of a packet.
	 */
} __attribute__((packed));

/* last 16 bytes of Long TX BD */
struct tx_bd_long_hi {
	uint16_t lflags;
	/*
	 * All bits in this field must be valid on the first BD of a
	 * packet. Their value on other BDs of the packet will be
	 * ignored.
	 */
	/*
	 * If set to 1, the controller replaces the TCP/UPD checksum
	 * fields of normal TCP/UPD checksum, or the inner TCP/UDP
	 * checksum field of the encapsulated TCP/UDP packets with the
	 * hardware calculated TCP/UDP checksum for the packet
	 * associated with this descriptor. The flag is ignored if the
	 * LSO flag is set. This bit must be valid on the first BD of a
	 * packet.
	 */
	#define TX_BD_LONG_LFLAGS_TCP_UDP_CHKSUM	UINT32_C(0x1)
	/*
	 * If set to 1, the controller replaces the IP checksum of the
	 * normal packets, or the inner IP checksum of the encapsulated
	 * packets with the hardware calculated IP checksum for the
	 * packet associated with this descriptor. This bit must be
	 * valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_LFLAGS_IP_CHKSUM	UINT32_C(0x2)
	/*
	 * If set to 1, the controller will not append an Ethernet CRC
	 * to the end of the frame. This bit must be valid on the first
	 * BD of a packet. Packet must be 64B or longer when this flag
	 * is set. It is not useful to use this bit with any form of TX
	 * offload such as CSO or LSO. The intent is that the packet
	 * from the host already has a valid Ethernet CRC on the packet.
	 */
	#define TX_BD_LONG_LFLAGS_NOCRC	UINT32_C(0x4)
	/*
	 * If set to 1, the device will record the time at which the
	 * packet was actually transmitted at the TX MAC. This bit must
	 * be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_LFLAGS_STAMP	UINT32_C(0x8)
	/*
	 * If set to 1, The controller replaces the tunnel IP checksum
	 * field with hardware calculated IP checksum for the IP header
	 * of the packet associated with this descriptor. For outer UDP
	 * checksum, global outer UDP checksum TE_NIC register needs to
	 * be enabled. If the global outer UDP checksum TE_NIC register
	 * bit is set, outer UDP checksum will be calculated for the
	 * following cases: 1. Packets with tcp_udp_chksum flag set to
	 * offload checksum for inner packet AND the inner packet is
	 * TCP/UDP. If the inner packet is ICMP for example	(non-
	 * TCP/UDP), even if the tcp_udp_chksum is set, the outer UDP
	 * checksum will not be calculated. 2. Packets with lso flag set
	 * which implies inner TCP checksum calculation as part of LSO
	 * operation.
	 */
	#define TX_BD_LONG_LFLAGS_T_IP_CHKSUM	UINT32_C(0x10)
	/*
	 * If set to 1, the device will treat this packet with LSO(Large
	 * Send Offload) processing for both normal or encapsulated
	 * packets, which is a form of TCP segmentation. When this bit
	 * is 1, the hdr_size and mss fields must be valid. The driver
	 * doesn't need to set t_ip_chksum, ip_chksum, and
	 * tcp_udp_chksum flags since the controller will replace the
	 * appropriate checksum fields for segmented packets. When this
	 * bit is 1, the hdr_size and mss fields must be valid.
	 */
	#define TX_BD_LONG_LFLAGS_LSO	UINT32_C(0x20)
	/*
	 * If set to zero when LSO is '1', then the IPID will be treated
	 * as a 16b number and will be wrapped if it exceeds a value of
	 * 0xffff. If set to one when LSO is '1', then the IPID will be
	 * treated as a 15b number and will be wrapped if it exceeds a
	 * value 0f 0x7fff.
	 */
	#define TX_BD_LONG_LFLAGS_IPID_FMT	UINT32_C(0x40)
	/*
	 * If set to zero when LSO is '1', then the IPID of the tunnel
	 * IP header will not be modified during LSO operations. If set
	 * to one when LSO is '1', then the IPID of the tunnel IP header
	 * will be incremented for each subsequent segment of an LSO
	 * operation. The flag is ignored if the LSO packet is a normal
	 *	(non-tunneled) TCP packet.
	 */
	#define TX_BD_LONG_LFLAGS_T_IPID	UINT32_C(0x80)
	/*
	 * If set to '1', then the RoCE ICRC will be appended to the
	 * packet. Packet must be a valid RoCE format packet.
	 */
	#define TX_BD_LONG_LFLAGS_ROCE_CRC	UINT32_C(0x100)
	/*
	 * If set to '1', then the FCoE CRC will be appended to the
	 * packet. Packet must be a valid FCoE format packet.
	 */
	#define TX_BD_LONG_LFLAGS_FCOE_CRC	UINT32_C(0x200)
	uint16_t hdr_size;
	/*
	 * When LSO is '1', this field must contain the offset of the
	 * TCP payload from the beginning of the packet in as 16b words.
	 * In case of encapsulated/tunneling packet, this field contains
	 * the offset of the inner TCP payload from beginning of the
	 * packet as 16-bit words. This value must be valid on the first
	 * BD of a packet.
	 */
	#define TX_BD_LONG_HDR_SIZE_MASK	UINT32_C(0x1ff)
	#define TX_BD_LONG_HDR_SIZE_SFT	0
	uint32_t mss;
	/*
	 * This is the MSS value that will be used to do the LSO
	 * processing. The value is the length in bytes of the TCP
	 * payload for each segment generated by the LSO operation. This
	 * value must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_MSS_MASK	UINT32_C(0x7fff)
	#define TX_BD_LONG_MSS_SFT	0
	uint16_t unused_2;
	uint16_t cfa_action;
	/*
	 * This value selects a CFA action to perform on the packet. Set
	 * this value to zero if no CFA action is desired. This value
	 * must be valid on the first BD of a packet.
	 */
	uint32_t cfa_meta;
	/*
	 * This value is action meta-data that defines CFA edit
	 * operations that are done in addition to any action editing.
	 */
	/* When key=1, This is the VLAN tag VID value. */
	#define TX_BD_LONG_CFA_META_VLAN_VID_MASK	UINT32_C(0xfff)
	#define TX_BD_LONG_CFA_META_VLAN_VID_SFT	0
	/* When key=1, This is the VLAN tag DE value. */
	#define TX_BD_LONG_CFA_META_VLAN_DE	UINT32_C(0x1000)
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
	 * This field identifies the type of edit to be performed on the
	 * packet. This value must be valid on the first BD of a packet.
	 */
	#define TX_BD_LONG_CFA_META_KEY_MASK	UINT32_C(0xf0000000)
	#define TX_BD_LONG_CFA_META_KEY_SFT	28
	/* No editing */
	#define TX_BD_LONG_CFA_META_KEY_NONE	(UINT32_C(0x0) << 28)
	/*
	 * - meta[17:16] - TPID select value	(0 =
	 * 0x8100). - meta[15:12] - PRI/DE value. -
	 * meta[11:0] - VID value.
	 */
	#define TX_BD_LONG_CFA_META_KEY_VLAN_TAG	(UINT32_C(0x1) << 28)
	#define TX_BD_LONG_CFA_META_KEY_LAST \
		TX_BD_LONG_CFA_META_KEY_VLAN_TAG
} __attribute__((packed));

/* RX Producer Packet BD	(16 bytes) */
struct rx_prod_pkt_bd {
	uint16_t flags_type;
	/* This value identifies the type of buffer descriptor. */
	#define RX_PROD_PKT_BD_TYPE_MASK	UINT32_C(0x3f)
	#define RX_PROD_PKT_BD_TYPE_SFT	0
	/*
	 * Indicates that this BD is 16B long and is an
	 * RX Producer	(ie. empty) buffer descriptor.
	 */
	#define RX_PROD_PKT_BD_TYPE_RX_PROD_PKT	UINT32_C(0x4)
	/*
	 * If set to 1, the packet will be placed at the address plus
	 * 2B. The 2 Bytes of padding will be written as zero.
	 */
	/*
	 * This is intended to be used when the host buffer is cache-
	 * line aligned to produce packets that are easy to parse in
	 * host memory while still allowing writes to be cache line
	 * aligned.
	 */
	#define RX_PROD_PKT_BD_FLAGS_SOP_PAD	UINT32_C(0x40)
	/*
	 * If set to 1, the packet write will be padded out to the
	 * nearest cache-line with zero value padding.
	 */
	/*
	 * If receive buffers start/end on cache-line boundaries, this
	 * feature will ensure that all data writes on the PCI bus
	 * start/end on cache line boundaries.
	 */
	#define RX_PROD_PKT_BD_FLAGS_EOP_PAD	UINT32_C(0x80)
	/*
	 * This value is the number of additional buffers in the ring
	 * that describe the buffer space to be consumed for the this
	 * packet. If the value is zero, then the packet must fit within
	 * the space described by this BD. If this value is 1 or more,
	 * it indicates how many additional "buffer" BDs are in the ring
	 * immediately following this BD to be used for the same network
	 * packet. Even if the packet to be placed does not need all the
	 * additional buffers, they will be consumed anyway.
	 */
	#define RX_PROD_PKT_BD_FLAGS_BUFFERS_MASK	UINT32_C(0x300)
	#define RX_PROD_PKT_BD_FLAGS_BUFFERS_SFT	8
	#define RX_PROD_PKT_BD_FLAGS_MASK	UINT32_C(0xffc0)
	#define RX_PROD_PKT_BD_FLAGS_SFT	6
	uint16_t len;
	/*
	 * This is the length in Bytes of the host physical buffer where
	 * data for the packet may be placed in host memory.
	 */
	/*
	 * While this is a Byte resolution value, it is often
	 * advantageous to ensure that the buffers provided end on a
	 * host cache line.
	 */
	uint32_t opaque;
	/*
	 * The opaque data field is pass through to the completion and
	 * can be used for any data that the driver wants to associate
	 * with this receive buffer set.
	 */
	uint64_t addr;
	/*
	 * This is the host physical address where data for the packet
	 * may by placed in host memory.
	 */
	/*
	 * While this is a Byte resolution value, it is often
	 * advantageous to ensure that the buffers provide start on a
	 * host cache line.
	 */
} __attribute__((packed));

/* Completion Ring Structures */
/* Note: This structure is used by the HWRM to communicate HWRM Error. */
/* Base Completion Record	(16 bytes) */
struct cmpl_base {
	uint16_t type;
	/* unused is 10 b */
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define CMPL_BASE_TYPE_MASK	UINT32_C(0x3f)
	#define CMPL_BASE_TYPE_SFT	0
	/* TX L2 completion: Completion of TX packet. Length = 16B */
	#define CMPL_BASE_TYPE_TX_L2	UINT32_C(0x0)
	/*
	 * RX L2 completion: Completion of and L2 RX
	 * packet. Length = 32B
	 */
	#define CMPL_BASE_TYPE_RX_L2	UINT32_C(0x11)
	/*
	 * RX Aggregation Buffer completion : Completion
	 * of an L2 aggregation buffer in support of
	 * TPA, HDS, or Jumbo packet completion. Length
	 * = 16B
	 */
	#define CMPL_BASE_TYPE_RX_AGG	UINT32_C(0x12)
	/*
	 * RX L2 TPA Start Completion: Completion at the
	 * beginning of a TPA operation. Length = 32B
	 */
	#define CMPL_BASE_TYPE_RX_TPA_START	UINT32_C(0x13)
	/*
	 * RX L2 TPA End Completion: Completion at the
	 * end of a TPA operation. Length = 32B
	 */
	#define CMPL_BASE_TYPE_RX_TPA_END	UINT32_C(0x15)
	/*
	 * Statistics Ejection Completion: Completion of
	 * statistics data ejection buffer. Length = 16B
	 */
	#define CMPL_BASE_TYPE_STAT_EJECT	UINT32_C(0x1a)
	/* HWRM Command Completion: Completion of an HWRM command. */
	#define CMPL_BASE_TYPE_HWRM_DONE	UINT32_C(0x20)
	/* Forwarded HWRM Request */
	#define CMPL_BASE_TYPE_HWRM_FWD_REQ	UINT32_C(0x22)
	/* Forwarded HWRM Response */
	#define CMPL_BASE_TYPE_HWRM_FWD_RESP	UINT32_C(0x24)
	/* HWRM Asynchronous Event Information */
	#define CMPL_BASE_TYPE_HWRM_ASYNC_EVENT	UINT32_C(0x2e)
	/* CQ Notification */
	#define CMPL_BASE_TYPE_CQ_NOTIFICATION	UINT32_C(0x30)
	/* SRQ Threshold Event */
	#define CMPL_BASE_TYPE_SRQ_EVENT	UINT32_C(0x32)
	/* DBQ Threshold Event */
	#define CMPL_BASE_TYPE_DBQ_EVENT	UINT32_C(0x34)
	/* QP Async Notification */
	#define CMPL_BASE_TYPE_QP_EVENT	UINT32_C(0x38)
	/* Function Async Notification */
	#define CMPL_BASE_TYPE_FUNC_EVENT	UINT32_C(0x3a)
	/* unused is 10 b */
	uint16_t info1;
	/* info1 is 16 b */
	uint32_t info2;
	/* info2 is 32 b */
	uint32_t info3_v;
	/* info3 is 31 b */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define CMPL_BASE_V	UINT32_C(0x1)
	/* info3 is 31 b */
	#define CMPL_BASE_INFO3_MASK	UINT32_C(0xfffffffe)
	#define CMPL_BASE_INFO3_SFT	1
	uint32_t info4;
	/* info4 is 32 b */
} __attribute__((packed));

/* TX Completion Record	(16 bytes) */
struct tx_cmpl {
	uint16_t flags_type;
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define TX_CMPL_TYPE_MASK	UINT32_C(0x3f)
	#define TX_CMPL_TYPE_SFT	0
	/* TX L2 completion: Completion of TX packet. Length = 16B */
	#define TX_CMPL_TYPE_TX_L2	UINT32_C(0x0)
	/*
	 * When this bit is '1', it indicates a packet that has an error
	 * of some type. Type of error is indicated in error_flags.
	 */
	#define TX_CMPL_FLAGS_ERROR	UINT32_C(0x40)
	/*
	 * When this bit is '1', it indicates that the packet completed
	 * was transmitted using the push acceleration data provided by
	 * the driver. When this bit is '0', it indicates that the
	 * packet had not push acceleration data written or was executed
	 * as a normal packet even though push data was provided.
	 */
	#define TX_CMPL_FLAGS_PUSH	UINT32_C(0x80)
	#define TX_CMPL_FLAGS_MASK	UINT32_C(0xffc0)
	#define TX_CMPL_FLAGS_SFT	6
	uint16_t unused_0;
	/* unused1 is 16 b */
	uint32_t opaque;
	/*
	 * This is a copy of the opaque field from the first TX BD of
	 * this transmitted packet.
	 */
	uint16_t errors_v;
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define TX_CMPL_V	UINT32_C(0x1)
	/*
	 * This error indicates that there was some sort of problem with
	 * the BDs for the packet.
	 */
	#define TX_CMPL_ERRORS_BUFFER_ERROR_MASK	UINT32_C(0xe)
	#define TX_CMPL_ERRORS_BUFFER_ERROR_SFT	1
	/* No error */
	#define TX_CMPL_ERRORS_BUFFER_ERROR_NO_ERROR	(UINT32_C(0x0) << 1)
	/* Bad Format: BDs were not formatted correctly. */
	#define TX_CMPL_ERRORS_BUFFER_ERROR_BAD_FMT	(UINT32_C(0x2) << 1)
	#define TX_CMPL_ERRORS_BUFFER_ERROR_LAST \
		TX_CMPL_ERRORS_BUFFER_ERROR_BAD_FMT
	/*
	 * When this bit is '1', it indicates that the length of the
	 * packet was zero. No packet was transmitted.
	 */
	#define TX_CMPL_ERRORS_ZERO_LENGTH_PKT	UINT32_C(0x10)
	/*
	 * When this bit is '1', it indicates that the packet was longer
	 * than the programmed limit in TDI. No packet was transmitted.
	 */
	#define TX_CMPL_ERRORS_EXCESSIVE_BD_LENGTH	UINT32_C(0x20)
	/*
	 * When this bit is '1', it indicates that one or more of the
	 * BDs associated with this packet generated a PCI error. This
	 * probably means the address was not valid.
	 */
	#define TX_CMPL_ERRORS_DMA_ERROR	UINT32_C(0x40)
	/*
	 * When this bit is '1', it indicates that the packet was longer
	 * than indicated by the hint. No packet was transmitted.
	 */
	#define TX_CMPL_ERRORS_HINT_TOO_SHORT	UINT32_C(0x80)
	/*
	 * When this bit is '1', it indicates that the packet was
	 * dropped due to Poison TLP error on one or more of the TLPs in
	 * the PXP completion.
	 */
	#define TX_CMPL_ERRORS_POISON_TLP_ERROR	UINT32_C(0x100)
	#define TX_CMPL_ERRORS_MASK	UINT32_C(0xfffe)
	#define TX_CMPL_ERRORS_SFT	1
	uint16_t unused_1;
	/* unused2 is 16 b */
	uint32_t unused_2;
	/* unused3 is 32 b */
} __attribute__((packed));

/* RX Packet Completion Record	(32 bytes split to 2 16-byte struct) */
struct rx_pkt_cmpl {
	uint16_t flags_type;
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define RX_PKT_CMPL_TYPE_MASK	UINT32_C(0x3f)
	#define RX_PKT_CMPL_TYPE_SFT	0
	/*
	 * RX L2 completion: Completion of and L2 RX
	 * packet. Length = 32B
	 */
	#define RX_PKT_CMPL_TYPE_RX_L2			UINT32_C(0x11)
	/*
	 * When this bit is '1', it indicates a packet that has an error
	 * of some type. Type of error is indicated in error_flags.
	 */
	#define RX_PKT_CMPL_FLAGS_ERROR	UINT32_C(0x40)
	/* This field indicates how the packet was placed in the buffer. */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_MASK	UINT32_C(0x380)
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_SFT	7
	/* Normal: Packet was placed using normal algorithm. */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_NORMAL	(UINT32_C(0x0) << 7)
	/* Jumbo: Packet was placed using jumbo algorithm. */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_JUMBO	(UINT32_C(0x1) << 7)
	/*
	 * Header/Data Separation: Packet was placed
	 * using Header/Data separation algorithm. The
	 * separation location is indicated by the itype
	 * field.
	 */
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_HDS	(UINT32_C(0x2) << 7)
	#define RX_PKT_CMPL_FLAGS_PLACEMENT_LAST \
		RX_PKT_CMPL_FLAGS_PLACEMENT_HDS
	/* This bit is '1' if the RSS field in this completion is valid. */
	#define RX_PKT_CMPL_FLAGS_RSS_VALID	UINT32_C(0x400)
	/* unused is 1 b */
	#define RX_PKT_CMPL_FLAGS_UNUSED	UINT32_C(0x800)
	/*
	 * This value indicates what the inner packet determined for the
	 * packet was.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_MASK	UINT32_C(0xf000)
	#define RX_PKT_CMPL_FLAGS_ITYPE_SFT	12
	/* Not Known: Indicates that the packet type was not known. */
	#define RX_PKT_CMPL_FLAGS_ITYPE_NOT_KNOWN	(UINT32_C(0x0) << 12)
	/*
	 * IP Packet: Indicates that the packet was an
	 * IP packet, but further classification was not
	 * possible.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_IP	(UINT32_C(0x1) << 12)
	/*
	 * TCP Packet: Indicates that the packet was IP
	 * and TCP. This indicates that the
	 * payload_offset field is valid.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_TCP	(UINT32_C(0x2) << 12)
	/*
	 * UDP Packet: Indicates that the packet was IP
	 * and UDP. This indicates that the
	 * payload_offset field is valid.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_UDP	(UINT32_C(0x3) << 12)
	/*
	 * FCoE Packet: Indicates that the packet was
	 * recognized as a FCoE. This also indicates
	 * that the payload_offset field is valid.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_FCOE	(UINT32_C(0x4) << 12)
	/*
	 * RoCE Packet: Indicates that the packet was
	 * recognized as a RoCE. This also indicates
	 * that the payload_offset field is valid.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_ROCE	(UINT32_C(0x5) << 12)
	/*
	 * ICMP Packet: Indicates that the packet was
	 * recognized as ICMP. This indicates that the
	 * payload_offset field is valid.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_ICMP	(UINT32_C(0x7) << 12)
	/*
	 * PtP packet wo/timestamp: Indicates that the
	 * packet was recognized as a PtP packet.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_PTP_WO_TIMESTAMP	(UINT32_C(0x8) << 12)
	/*
	 * PtP packet w/timestamp: Indicates that the
	 * packet was recognized as a PtP packet and
	 * that a timestamp was taken for the packet.
	 */
	#define RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP	(UINT32_C(0x9) << 12)
	#define RX_PKT_CMPL_FLAGS_ITYPE_LAST \
		RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP
	#define RX_PKT_CMPL_FLAGS_MASK	UINT32_C(0xffc0)
	#define RX_PKT_CMPL_FLAGS_SFT	6
	uint16_t len;
	/*
	 * This is the length of the data for the packet stored in the
	 * buffer(s) identified by the opaque value. This includes the
	 * packet BD and any associated buffer BDs. This does not
	 * include the length of any data places in aggregation BDs.
	 */
	uint32_t opaque;
	/*
	 * This is a copy of the opaque field from the RX BD this
	 * completion corresponds to.
	 */
	uint8_t agg_bufs_v1;
	/* unused1 is 2 b */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define RX_PKT_CMPL_V1	UINT32_C(0x1)
	/*
	 * This value is the number of aggregation buffers that follow
	 * this entry in the completion ring that are a part of this
	 * packet. If the value is zero, then the packet is completely
	 * contained in the buffer space provided for the packet in the
	 * RX ring.
	 */
	#define RX_PKT_CMPL_AGG_BUFS_MASK	UINT32_C(0x3e)
	#define RX_PKT_CMPL_AGG_BUFS_SFT	1
	/* unused1 is 2 b */
	uint8_t rss_hash_type;
	/*
	 * This is the RSS hash type for the packet. The value is packed
	 * {tuple_extrac_op[1:0],rss_profile_id[4:0],tuple_extrac_op[2]}
	 * . The value of tuple_extrac_op provides the information about
	 * what fields the hash was computed on. * 0: The RSS hash was
	 * computed over source IP address, destination IP address,
	 * source port, and destination port of inner IP and TCP or UDP
	 * headers. Note: For non-tunneled packets, the packet headers
	 * are considered inner packet headers for the RSS hash
	 * computation purpose. * 1: The RSS hash was computed over
	 * source IP address and destination IP address of inner IP
	 * header. Note: For non-tunneled packets, the packet headers
	 * are considered inner packet headers for the RSS hash
	 * computation purpose. * 2: The RSS hash was computed over
	 * source IP address, destination IP address, source port, and
	 * destination port of IP and TCP or UDP headers of outer tunnel
	 * headers. Note: For non-tunneled packets, this value is not
	 * applicable. * 3: The RSS hash was computed over source IP
	 * address and destination IP address of IP header of outer
	 * tunnel headers. Note: For non-tunneled packets, this value is
	 * not applicable. Note that 4-tuples values listed above are
	 * applicable for layer 4 protocols supported and enabled for
	 * RSS in the hardware, HWRM firmware, and drivers. For example,
	 * if RSS hash is supported and enabled for TCP traffic only,
	 * then the values of tuple_extract_op corresponding to 4-tuples
	 * are only valid for TCP traffic.
	 */
	uint8_t payload_offset;
	/*
	 * This value indicates the offset in bytes from the beginning
	 * of the packet where the inner payload starts. This value is
	 * valid for TCP, UDP, FCoE, and RoCE packets. A value of zero
	 * indicates that header is 256B into the packet.
	 */
	uint8_t unused_1;
	/* unused2 is 8 b */
	uint32_t rss_hash;
	/*
	 * This value is the RSS hash value calculated for the packet
	 * based on the mode bits and key value in the VNIC.
	 */
} __attribute__((packed));

/* last 16 bytes of RX Packet Completion Record */
struct rx_pkt_cmpl_hi {
	uint32_t flags2;
	/*
	 * This indicates that the ip checksum was calculated for the
	 * inner packet and that the ip_cs_error field indicates if
	 * there was an error.
	 */
	#define RX_PKT_CMPL_FLAGS2_IP_CS_CALC	UINT32_C(0x1)
	/*
	 * This indicates that the TCP, UDP or ICMP checksum was
	 * calculated for the inner packet and that the l4_cs_error
	 * field indicates if there was an error.
	 */
	#define RX_PKT_CMPL_FLAGS2_L4_CS_CALC	UINT32_C(0x2)
	/*
	 * This indicates that the ip checksum was calculated for the
	 * tunnel header and that the t_ip_cs_error field indicates if
	 * there was an error.
	 */
	#define RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC	UINT32_C(0x4)
	/*
	 * This indicates that the UDP checksum was calculated for the
	 * tunnel packet and that the t_l4_cs_error field indicates if
	 * there was an error.
	 */
	#define RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC	UINT32_C(0x8)
	/* This value indicates what format the metadata field is. */
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_MASK	UINT32_C(0xf0)
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_SFT	4
	/* No metadata informtaion. Value is zero. */
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_NONE	(UINT32_C(0x0) << 4)
	/*
	 * The metadata field contains the VLAN tag and
	 * TPID value. - metadata[11:0] contains the
	 * vlan VID value. - metadata[12] contains the
	 * vlan DE value. - metadata[15:13] contains the
	 * vlan PRI value. - metadata[31:16] contains
	 * the vlan TPID value.
	 */
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN	(UINT32_C(0x1) << 4)
	#define RX_PKT_CMPL_FLAGS2_META_FORMAT_LAST \
		RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN
	/*
	 * This field indicates the IP type for the inner-most IP
	 * header. A value of '0' indicates IPv4. A value of '1'
	 * indicates IPv6. This value is only valid if itype indicates a
	 * packet with an IP header.
	 */
	#define RX_PKT_CMPL_FLAGS2_IP_TYPE	UINT32_C(0x100)
	uint32_t metadata;
	/*
	 * This is data from the CFA block as indicated by the
	 * meta_format field.
	 */
	/* When meta_format=1, this value is the VLAN VID. */
	#define RX_PKT_CMPL_METADATA_VID_MASK	UINT32_C(0xfff)
	#define RX_PKT_CMPL_METADATA_VID_SFT	0
	/* When meta_format=1, this value is the VLAN DE. */
	#define RX_PKT_CMPL_METADATA_DE	UINT32_C(0x1000)
	/* When meta_format=1, this value is the VLAN PRI. */
	#define RX_PKT_CMPL_METADATA_PRI_MASK	UINT32_C(0xe000)
	#define RX_PKT_CMPL_METADATA_PRI_SFT	13
	/* When meta_format=1, this value is the VLAN TPID. */
	#define RX_PKT_CMPL_METADATA_TPID_MASK	UINT32_C(0xffff0000)
	#define RX_PKT_CMPL_METADATA_TPID_SFT	16
	uint16_t errors_v2;
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define RX_PKT_CMPL_V2	UINT32_C(0x1)
	/*
	 * This error indicates that there was some sort of problem with
	 * the BDs for the packet that was found after part of the
	 * packet was already placed. The packet should be treated as
	 * invalid.
	 */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_MASK	UINT32_C(0xe)
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_SFT	1
	/* No buffer error */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_NO_BUFFER	(UINT32_C(0x0) << 1)
	/*
	 * Did Not Fit: Packet did not fit into packet
	 * buffer provided. For regular placement, this
	 * means the packet did not fit in the buffer
	 * provided. For HDS and jumbo placement, this
	 * means that the packet could not be placed
	 * into 7 physical buffers or less.
	 */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_DID_NOT_FIT \
		(UINT32_C(0x1) << 1)
	/*
	 * Not On Chip: All BDs needed for the packet
	 * were not on-chip when the packet arrived.
	 */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_NOT_ON_CHIP \
		(UINT32_C(0x2) << 1)
	/* Bad Format: BDs were not formatted correctly. */
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_BAD_FORMAT \
		(UINT32_C(0x3) << 1)
	#define RX_PKT_CMPL_ERRORS_BUFFER_ERROR_LAST \
		RX_PKT_CMPL_ERRORS_BUFFER_ERROR_BAD_FORMAT
	/* This indicates that there was an error in the IP header checksum. */
	#define RX_PKT_CMPL_ERRORS_IP_CS_ERROR	UINT32_C(0x10)
	/*
	 * This indicates that there was an error in the TCP, UDP or
	 * ICMP checksum.
	 */
	#define RX_PKT_CMPL_ERRORS_L4_CS_ERROR	UINT32_C(0x20)
	/*
	 * This indicates that there was an error in the tunnel IP
	 * header checksum.
	 */
	#define RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR	UINT32_C(0x40)
	/*
	 * This indicates that there was an error in the tunnel UDP
	 * checksum.
	 */
	#define RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR	UINT32_C(0x80)
	/*
	 * This indicates that there was a CRC error on either an FCoE
	 * or RoCE packet. The itype indicates the packet type.
	 */
	#define RX_PKT_CMPL_ERRORS_CRC_ERROR	UINT32_C(0x100)
	/*
	 * This indicates that there was an error in the tunnel portion
	 * of the packet when this field is non-zero.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_MASK	UINT32_C(0xe00)
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_SFT	9
	/*
	 * No additional error occurred on the tunnel
	 * portion of the packet of the packet does not
	 * have a tunnel.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_NO_ERROR	(UINT32_C(0x0) << 9)
	/*
	 * Indicates that IP header version does not
	 * match expectation from L2 Ethertype for IPv4
	 * and IPv6 in the tunnel header.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_VERSION \
		(UINT32_C(0x1) << 9)
	/*
	 * Indicates that header length is out of range
	 * in the tunnel header. Valid for IPv4.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_HDR_LEN \
		(UINT32_C(0x2) << 9)
	/*
	 * Indicates that the physical packet is shorter
	 * than that claimed by the PPPoE header length
	 * for a tunnel PPPoE packet.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_TUNNEL_TOTAL_ERROR \
		(UINT32_C(0x3) << 9)
	/*
	 * Indicates that physical packet is shorter
	 * than that claimed by the tunnel l3 header
	 * length. Valid for IPv4, or IPv6 tunnel packet
	 * packets.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_IP_TOTAL_ERROR \
		(UINT32_C(0x4) << 9)
	/*
	 * Indicates that the physical packet is shorter
	 * than that claimed by the tunnel UDP header
	 * length for a tunnel UDP packet that is not
	 * fragmented.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_UDP_TOTAL_ERROR \
		(UINT32_C(0x5) << 9)
	/*
	 * indicates that the IPv4 TTL or IPv6 hop limit
	 * check have failed	(e.g. TTL = 0) in the
	 * tunnel header. Valid for IPv4, and IPv6.
	 */
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL \
		(UINT32_C(0x6) << 9)
	#define RX_PKT_CMPL_ERRORS_T_PKT_ERROR_LAST \
		RX_PKT_CMPL_ERRORS_T_PKT_ERROR_T_L3_BAD_TTL
	/*
	 * This indicates that there was an error in the inner portion
	 * of the packet when this field is non-zero.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_MASK	UINT32_C(0xf000)
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_SFT	12
	/*
	 * No additional error occurred on the tunnel
	 * portion of the packet of the packet does not
	 * have a tunnel.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_NO_ERROR	(UINT32_C(0x0) << 12)
	/*
	 * Indicates that IP header version does not
	 * match expectation from L2 Ethertype for IPv4
	 * and IPv6 or that option other than VFT was
	 * parsed on FCoE packet.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_VERSION \
		(UINT32_C(0x1) << 12)
	/*
	 * indicates that header length is out of range.
	 * Valid for IPv4 and RoCE
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_HDR_LEN \
		(UINT32_C(0x2) << 12)
	/*
	 * indicates that the IPv4 TTL or IPv6 hop limit
	 * check have failed	(e.g. TTL = 0). Valid for
	 * IPv4, and IPv6
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L3_BAD_TTL	(UINT32_C(0x3) << 12)
	/*
	 * Indicates that physical packet is shorter
	 * than that claimed by the l3 header length.
	 * Valid for IPv4, IPv6 packet or RoCE packets.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_IP_TOTAL_ERROR \
		(UINT32_C(0x4) << 12)
	/*
	 * Indicates that the physical packet is shorter
	 * than that claimed by the UDP header length
	 * for a UDP packet that is not fragmented.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_UDP_TOTAL_ERROR \
		(UINT32_C(0x5) << 12)
	/*
	 * Indicates that TCP header length > IP
	 * payload. Valid for TCP packets only.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN \
		(UINT32_C(0x6) << 12)
	/* Indicates that TCP header length < 5. Valid for TCP. */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_HDR_LEN_TOO_SMALL \
		(UINT32_C(0x7) << 12)
	/*
	 * Indicates that TCP option headers result in a
	 * TCP header size that does not match data
	 * offset in TCP header. Valid for TCP.
	 */
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN \
		(UINT32_C(0x8) << 12)
	#define RX_PKT_CMPL_ERRORS_PKT_ERROR_LAST \
		RX_PKT_CMPL_ERRORS_PKT_ERROR_L4_BAD_OPT_LEN
	#define RX_PKT_CMPL_ERRORS_MASK	UINT32_C(0xfffe)
	#define RX_PKT_CMPL_ERRORS_SFT	1
	uint16_t cfa_code;
	/*
	 * This field identifies the CFA action rule that was used for
	 * this packet.
	 */
	uint32_t reorder;
	/*
	 * This value holds the reordering sequence number for the
	 * packet. If the reordering sequence is not valid, then this
	 * value is zero. The reordering domain for the packet is in the
	 * bottom 8 to 10b of the rss_hash value. The bottom 20b of this
	 * value contain the ordering domain value for the packet.
	 */
	#define RX_PKT_CMPL_REORDER_MASK	UINT32_C(0xffffff)
	#define RX_PKT_CMPL_REORDER_SFT	0
} __attribute__((packed));

/* RX L2 TPA Start Completion Record (32 bytes split to 2 16-byte struct) */
struct rx_tpa_start_cmpl {
	uint16_t flags_type;
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define RX_TPA_START_CMPL_TYPE_MASK	UINT32_C(0x3f)
	#define RX_TPA_START_CMPL_TYPE_SFT	0
	/*
	 * RX L2 TPA Start Completion: Completion at the
	 * beginning of a TPA operation. Length = 32B
	 */
	#define RX_TPA_START_CMPL_TYPE_RX_TPA_START	UINT32_C(0x13)
	/* This bit will always be '0' for TPA start completions. */
	#define RX_TPA_START_CMPL_FLAGS_ERROR	UINT32_C(0x40)
	/* This field indicates how the packet was placed in the buffer. */
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_MASK	UINT32_C(0x380)
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_SFT	7
	/*
	 * Jumbo: TPA Packet was placed using jumbo
	 * algorithm. This means that the first buffer
	 * will be filled with data before moving to
	 * aggregation buffers. Each aggregation buffer
	 * will be filled before moving to the next
	 * aggregation buffer.
	 */
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_JUMBO	(UINT32_C(0x1) << 7)
	/*
	 * Header/Data Separation: Packet was placed
	 * using Header/Data separation algorithm. The
	 * separation location is indicated by the itype
	 * field.
	 */
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_HDS	(UINT32_C(0x2) << 7)
	/*
	 * GRO/Jumbo: Packet will be placed using
	 * GRO/Jumbo where the first packet is filled
	 * with data. Subsequent packets will be placed
	 * such that any one packet does not span two
	 * aggregation buffers unless it starts at the
	 * beginning of an aggregation buffer.
	 */
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_GRO_JUMBO \
		(UINT32_C(0x5) << 7)
	/*
	 * GRO/Header-Data Separation: Packet will be
	 * placed using GRO/HDS where the header is in
	 * the first packet. Payload of each packet will
	 * be placed such that any one packet does not
	 * span two aggregation buffers unless it starts
	 * at the beginning of an aggregation buffer.
	 */
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_GRO_HDS (UINT32_C(0x6) << 7)
	#define RX_TPA_START_CMPL_FLAGS_PLACEMENT_LAST \
		RX_TPA_START_CMPL_FLAGS_PLACEMENT_GRO_HDS
	/* This bit is '1' if the RSS field in this completion is valid. */
	#define RX_TPA_START_CMPL_FLAGS_RSS_VALID	UINT32_C(0x400)
	/* unused is 1 b */
	#define RX_TPA_START_CMPL_FLAGS_UNUSED	UINT32_C(0x800)
	/*
	 * This value indicates what the inner packet determined for the
	 * packet was.
	 */
	#define RX_TPA_START_CMPL_FLAGS_ITYPE_MASK	UINT32_C(0xf000)
	#define RX_TPA_START_CMPL_FLAGS_ITYPE_SFT	12
	/* TCP Packet: Indicates that the packet was IP and TCP. */
	#define RX_TPA_START_CMPL_FLAGS_ITYPE_TCP	(UINT32_C(0x2) << 12)
	#define RX_TPA_START_CMPL_FLAGS_ITYPE_LAST \
		RX_TPA_START_CMPL_FLAGS_ITYPE_TCP
	#define RX_TPA_START_CMPL_FLAGS_MASK	UINT32_C(0xffc0)
	#define RX_TPA_START_CMPL_FLAGS_SFT	6
	uint16_t len;
	/*
	 * This value indicates the amount of packet data written to the
	 * buffer the opaque field in this completion corresponds to.
	 */
	uint32_t opaque;
	/*
	 * This is a copy of the opaque field from the RX BD this
	 * completion corresponds to.
	 */
	uint8_t v1;
	/* unused1 is 7 b */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define RX_TPA_START_CMPL_V1	UINT32_C(0x1)
	/* unused1 is 7 b */
	uint8_t rss_hash_type;
	/*
	 * This is the RSS hash type for the packet. The value is packed
	 * {tuple_extrac_op[1:0],rss_profile_id[4:0],tuple_extrac_op[2]}
	 * . The value of tuple_extrac_op provides the information about
	 * what fields the hash was computed on. * 0: The RSS hash was
	 * computed over source IP address, destination IP address,
	 * source port, and destination port of inner IP and TCP or UDP
	 * headers. Note: For non-tunneled packets, the packet headers
	 * are considered inner packet headers for the RSS hash
	 * computation purpose. * 1: The RSS hash was computed over
	 * source IP address and destination IP address of inner IP
	 * header. Note: For non-tunneled packets, the packet headers
	 * are considered inner packet headers for the RSS hash
	 * computation purpose. * 2: The RSS hash was computed over
	 * source IP address, destination IP address, source port, and
	 * destination port of IP and TCP or UDP headers of outer tunnel
	 * headers. Note: For non-tunneled packets, this value is not
	 * applicable. * 3: The RSS hash was computed over source IP
	 * address and destination IP address of IP header of outer
	 * tunnel headers. Note: For non-tunneled packets, this value is
	 * not applicable. Note that 4-tuples values listed above are
	 * applicable for layer 4 protocols supported and enabled for
	 * RSS in the hardware, HWRM firmware, and drivers. For example,
	 * if RSS hash is supported and enabled for TCP traffic only,
	 * then the values of tuple_extract_op corresponding to 4-tuples
	 * are only valid for TCP traffic.
	 */
	uint16_t agg_id;
	/*
	 * This is the aggregation ID that the completion is associated
	 * with. Use this number to correlate the TPA start completion
	 * with the TPA end completion.
	 */
	/* unused2 is 9 b */
	/*
	 * This is the aggregation ID that the completion is associated
	 * with. Use this number to correlate the TPA start completion
	 * with the TPA end completion.
	 */
	#define RX_TPA_START_CMPL_AGG_ID_MASK	UINT32_C(0xfe00)
	#define RX_TPA_START_CMPL_AGG_ID_SFT	9
	uint32_t rss_hash;
	/*
	 * This value is the RSS hash value calculated for the packet
	 * based on the mode bits and key value in the VNIC.
	 */
} __attribute__((packed));

/* last 16 bytes of RX L2 TPA Start Completion Record */
struct rx_tpa_start_cmpl_hi {
	uint32_t flags2;
	/*
	 * This indicates that the ip checksum was calculated for the
	 * inner packet and that the sum passed for all segments
	 * included in the aggregation.
	 */
	#define RX_TPA_START_CMPL_FLAGS2_IP_CS_CALC	UINT32_C(0x1)
	/*
	 * This indicates that the TCP, UDP or ICMP checksum was
	 * calculated for the inner packet and that the sum passed for
	 * all segments included in the aggregation.
	 */
	#define RX_TPA_START_CMPL_FLAGS2_L4_CS_CALC	UINT32_C(0x2)
	/*
	 * This indicates that the ip checksum was calculated for the
	 * tunnel header and that the sum passed for all segments
	 * included in the aggregation.
	 */
	#define RX_TPA_START_CMPL_FLAGS2_T_IP_CS_CALC	UINT32_C(0x4)
	/*
	 * This indicates that the UDP checksum was calculated for the
	 * tunnel packet and that the sum passed for all segments
	 * included in the aggregation.
	 */
	#define RX_TPA_START_CMPL_FLAGS2_T_L4_CS_CALC	UINT32_C(0x8)
	/* This value indicates what format the metadata field is. */
	#define RX_TPA_START_CMPL_FLAGS2_META_FORMAT_MASK UINT32_C(0xf0)
	#define RX_TPA_START_CMPL_FLAGS2_META_FORMAT_SFT	4
	/* No metadata informtaion. Value is zero. */
	#define RX_TPA_START_CMPL_FLAGS2_META_FORMAT_NONE (UINT32_C(0x0) << 4)
	/*
	 * The metadata field contains the VLAN tag and
	 * TPID value. - metadata[11:0] contains the
	 * vlan VID value. - metadata[12] contains the
	 * vlan DE value. - metadata[15:13] contains the
	 * vlan PRI value. - metadata[31:16] contains
	 * the vlan TPID value.
	 */
	#define RX_TPA_START_CMPL_FLAGS2_META_FORMAT_VLAN (UINT32_C(0x1) << 4)
	#define RX_TPA_START_CMPL_FLAGS2_META_FORMAT_LAST \
		RX_TPA_START_CMPL_FLAGS2_META_FORMAT_VLAN
	/*
	 * This field indicates the IP type for the inner-most IP
	 * header. A value of '0' indicates IPv4. A value of '1'
	 * indicates IPv6.
	 */
	#define RX_TPA_START_CMPL_FLAGS2_IP_TYPE	UINT32_C(0x100)
	uint32_t metadata;
	/*
	 * This is data from the CFA block as indicated by the
	 * meta_format field.
	 */
	/* When meta_format=1, this value is the VLAN VID. */
	#define RX_TPA_START_CMPL_METADATA_VID_MASK	UINT32_C(0xfff)
	#define RX_TPA_START_CMPL_METADATA_VID_SFT	0
	/* When meta_format=1, this value is the VLAN DE. */
	#define RX_TPA_START_CMPL_METADATA_DE	UINT32_C(0x1000)
	/* When meta_format=1, this value is the VLAN PRI. */
	#define RX_TPA_START_CMPL_METADATA_PRI_MASK UINT32_C(0xe000)
	#define RX_TPA_START_CMPL_METADATA_PRI_SFT	13
	/* When meta_format=1, this value is the VLAN TPID. */
	#define RX_TPA_START_CMPL_METADATA_TPID_MASK	UINT32_C(0xffff0000)
	#define RX_TPA_START_CMPL_METADATA_TPID_SFT	16
	uint16_t v2;
	/* unused4 is 15 b */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define RX_TPA_START_CMPL_V2	UINT32_C(0x1)
	/* unused4 is 15 b */
	uint16_t cfa_code;
	/*
	 * This field identifies the CFA action rule that was used for
	 * this packet.
	 */
	uint32_t inner_l4_size_inner_l3_offset_inner_l2_offset_outer_l3_offset;
	/*
	 * This is the size in bytes of the inner most L4 header. This
	 * can be subtracted from the payload_offset to determine the
	 * start of the inner most L4 header.
	 */
	/*
	 * This is the offset from the beginning of the packet in bytes
	 * for the outer L3 header. If there is no outer L3 header, then
	 * this value is zero.
	 */
	#define RX_TPA_START_CMPL_OUTER_L3_OFFSET_MASK	UINT32_C(0x1ff)
	#define RX_TPA_START_CMPL_OUTER_L3_OFFSET_SFT	0
	/*
	 * This is the offset from the beginning of the packet in bytes
	 * for the inner most L2 header.
	 */
	#define RX_TPA_START_CMPL_INNER_L2_OFFSET_MASK	UINT32_C(0x3fe00)
	#define RX_TPA_START_CMPL_INNER_L2_OFFSET_SFT	9
	/*
	 * This is the offset from the beginning of the packet in bytes
	 * for the inner most L3 header.
	 */
	#define RX_TPA_START_CMPL_INNER_L3_OFFSET_MASK	UINT32_C(0x7fc0000)
	#define RX_TPA_START_CMPL_INNER_L3_OFFSET_SFT	18
	/*
	 * This is the size in bytes of the inner most L4 header. This
	 * can be subtracted from the payload_offset to determine the
	 * start of the inner most L4 header.
	 */
	#define RX_TPA_START_CMPL_INNER_L4_SIZE_MASK	UINT32_C(0xf8000000)
	#define RX_TPA_START_CMPL_INNER_L4_SIZE_SFT	27
} __attribute__((packed));

/* RX TPA End Completion Record (32 bytes split to 2 16-byte struct) */
struct rx_tpa_end_cmpl {
	uint16_t flags_type;
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define RX_TPA_END_CMPL_TYPE_MASK	UINT32_C(0x3f)
	#define RX_TPA_END_CMPL_TYPE_SFT	0
	/*
	 * RX L2 TPA End Completion: Completion at the
	 * end of a TPA operation. Length = 32B
	 */
	#define RX_TPA_END_CMPL_TYPE_RX_TPA_END	UINT32_C(0x15)
	/*
	 * When this bit is '1', it indicates a packet that has an error
	 * of some type. Type of error is indicated in error_flags.
	 */
	#define RX_TPA_END_CMPL_FLAGS_ERROR	UINT32_C(0x40)
	/* This field indicates how the packet was placed in the buffer. */
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_MASK	UINT32_C(0x380)
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_SFT	7
	/*
	 * Jumbo: TPA Packet was placed using jumbo
	 * algorithm. This means that the first buffer
	 * will be filled with data before moving to
	 * aggregation buffers. Each aggregation buffer
	 * will be filled before moving to the next
	 * aggregation buffer.
	 */
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_JUMBO	(UINT32_C(0x1) << 7)
	/*
	 * Header/Data Separation: Packet was placed
	 * using Header/Data separation algorithm. The
	 * separation location is indicated by the itype
	 * field.
	 */
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_HDS	(UINT32_C(0x2) << 7)
	/*
	 * GRO/Jumbo: Packet will be placed using
	 * GRO/Jumbo where the first packet is filled
	 * with data. Subsequent packets will be placed
	 * such that any one packet does not span two
	 * aggregation buffers unless it starts at the
	 * beginning of an aggregation buffer.
	 */
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_GRO_JUMBO (UINT32_C(0x5) << 7)
	/*
	 * GRO/Header-Data Separation: Packet will be
	 * placed using GRO/HDS where the header is in
	 * the first packet. Payload of each packet will
	 * be placed such that any one packet does not
	 * span two aggregation buffers unless it starts
	 * at the beginning of an aggregation buffer.
	 */
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_GRO_HDS	(UINT32_C(0x6) << 7)
	#define RX_TPA_END_CMPL_FLAGS_PLACEMENT_LAST \
		RX_TPA_END_CMPL_FLAGS_PLACEMENT_GRO_HDS
	/* unused is 2 b */
	#define RX_TPA_END_CMPL_FLAGS_UNUSED_MASK	UINT32_C(0xc00)
	#define RX_TPA_END_CMPL_FLAGS_UNUSED_SFT	10
	/*
	 * This value indicates what the inner packet determined for the
	 * packet was. - 2 TCP Packet Indicates that the packet was IP
	 * and TCP. This indicates that the ip_cs field is valid and
	 * that the tcp_udp_cs field is valid and contains the TCP
	 * checksum. This also indicates that the payload_offset field
	 * is valid.
	 */
	#define RX_TPA_END_CMPL_FLAGS_ITYPE_MASK	UINT32_C(0xf000)
	#define RX_TPA_END_CMPL_FLAGS_ITYPE_SFT	12
	#define RX_TPA_END_CMPL_FLAGS_MASK	UINT32_C(0xffc0)
	#define RX_TPA_END_CMPL_FLAGS_SFT	6
	uint16_t len;
	/*
	 * This value is zero for TPA End completions. There is no data
	 * in the buffer that corresponds to the opaque value in this
	 * completion.
	 */
	uint32_t opaque;
	/*
	 * This is a copy of the opaque field from the RX BD this
	 * completion corresponds to.
	 */
	uint8_t agg_bufs_v1;
	/* unused1 is 1 b */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define RX_TPA_END_CMPL_V1	UINT32_C(0x1)
	/*
	 * This value is the number of aggregation buffers that follow
	 * this entry in the completion ring that are a part of this
	 * aggregation packet. If the value is zero, then the packet is
	 * completely contained in the buffer space provided in the
	 * aggregation start completion.
	 */
	#define RX_TPA_END_CMPL_AGG_BUFS_MASK	UINT32_C(0x7e)
	#define RX_TPA_END_CMPL_AGG_BUFS_SFT	1
	/* unused1 is 1 b */
	uint8_t tpa_segs;
	/* This value is the number of segments in the TPA operation. */
	uint8_t payload_offset;
	/*
	 * This value indicates the offset in bytes from the beginning
	 * of the packet where the inner payload starts. This value is
	 * valid for TCP, UDP, FCoE, and RoCE packets. A value of zero
	 * indicates an offset of 256 bytes.
	 */
	uint8_t agg_id;
	/*
	 * This is the aggregation ID that the completion is associated
	 * with. Use this number to correlate the TPA start completion
	 * with the TPA end completion.
	 */
	/* unused2 is 1 b */
	/*
	 * This is the aggregation ID that the completion is associated
	 * with. Use this number to correlate the TPA start completion
	 * with the TPA end completion.
	 */
	#define RX_TPA_END_CMPL_AGG_ID_MASK	UINT32_C(0xfe)
	#define RX_TPA_END_CMPL_AGG_ID_SFT	1
	uint32_t tsdelta;
	/*
	 * For non-GRO packets, this value is the timestamp delta
	 * between earliest and latest timestamp values for TPA packet.
	 * If packets were not time stamped, then delta will be zero.
	 * For GRO packets, this field is zero except for the following
	 * sub-fields. - tsdelta[31] Timestamp present indication. When
	 * '0', no Timestamp option is in the packet. When '1', then a
	 * Timestamp option is present in the packet.
	 */
} __attribute__((packed));

/* last 16 bytes of RX TPA End Completion Record */
struct rx_tpa_end_cmpl_hi {
	uint32_t tpa_dup_acks;
	/* unused3 is 28 b */
	/*
	 * This value is the number of duplicate ACKs that have been
	 * received as part of the TPA operation.
	 */
	#define RX_TPA_END_CMPL_TPA_DUP_ACKS_MASK	UINT32_C(0xf)
	#define RX_TPA_END_CMPL_TPA_DUP_ACKS_SFT	0
	/* unused3 is 28 b */
	uint16_t tpa_seg_len;
	/*
	 * This value is the valid when TPA completion is active. It
	 * indicates the length of the longest segment of the TPA
	 * operation for LRO mode and the length of the first segment in
	 * GRO mode. This value may be used by GRO software to re-
	 * construct the original packet stream from the TPA packet.
	 * This is the length of all but the last segment for GRO. In
	 * LRO mode this value may be used to indicate MSS size to the
	 * stack.
	 */
	uint16_t unused_3;
	/* unused4 is 16 b */
	uint16_t errors_v2;
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define RX_TPA_END_CMPL_V2	UINT32_C(0x1)
	/*
	 * This error indicates that there was some sort of problem with
	 * the BDs for the packet that was found after part of the
	 * packet was already placed. The packet should be treated as
	 * invalid.
	 */
	#define RX_TPA_END_CMPL_ERRORS_BUFFER_ERROR_MASK	UINT32_C(0xe)
	#define RX_TPA_END_CMPL_ERRORS_BUFFER_ERROR_SFT	1
	/*
	 * This error occurs when there is a fatal HW
	 * problem in the chip only. It indicates that
	 * there were not BDs on chip but that there was
	 * adequate reservation. provided by the TPA
	 * block.
	 */
	#define RX_TPA_END_CMPL_ERRORS_BUFFER_ERROR_NOT_ON_CHIP \
		(UINT32_C(0x2) << 1)
	/*
	 * This error occurs when TPA block was not
	 * configured to reserve adequate BDs for TPA
	 * operations on this RX ring. All data for the
	 * TPA operation was not placed. This error can
	 * also be generated when the number of segments
	 * is not programmed correctly in TPA and the 33
	 * total aggregation buffers allowed for the TPA
	 * operation has been exceeded.
	 */
	#define RX_TPA_END_CMPL_ERRORS_BUFFER_ERROR_RSV_ERROR \
		(UINT32_C(0x4) << 1)
	#define RX_TPA_END_CMPL_ERRORS_BUFFER_ERROR_LAST \
		RX_TPA_END_CMPL_ERRORS_BUFFER_ERROR_RSV_ERROR
	#define RX_TPA_END_CMPL_ERRORS_MASK	UINT32_C(0xfffe)
	#define RX_TPA_END_CMPL_ERRORS_SFT	1
	uint16_t unused_4;
	/* unused5 is 16 b */
	uint32_t start_opaque;
	/*
	 * This is the opaque value that was completed for the TPA start
	 * completion that corresponds to this TPA end completion.
	 */
} __attribute__((packed));

/* HWRM Forwarded Request	(16 bytes) */
struct hwrm_fwd_req_cmpl {
	uint16_t req_len_type;
	/* Length of forwarded request in bytes. */
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define HWRM_FWD_INPUT_CMPL_TYPE_MASK	UINT32_C(0x3f)
	#define HWRM_FWD_INPUT_CMPL_TYPE_SFT	0
	/* Forwarded HWRM Request */
	#define HWRM_FWD_INPUT_CMPL_TYPE_HWRM_FWD_INPUT	UINT32_C(0x22)
	/* Length of forwarded request in bytes. */
	#define HWRM_FWD_REQ_CMPL_REQ_LEN_MASK	UINT32_C(0xffc0)
	#define HWRM_FWD_REQ_CMPL_REQ_LEN_SFT	6
	uint16_t source_id;
	/*
	 * Source ID of this request. Typically used in forwarding
	 * requests and responses. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF -
	 * HWRM
	 */
	uint32_t unused_0;
	/* unused1 is 32 b */
	uint32_t req_buf_addr_v[2];
	/* Address of forwarded request. */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define HWRM_FWD_INPUT_CMPL_V	UINT32_C(0x1)
	/* Address of forwarded request. */
	#define HWRM_FWD_REQ_CMPL_REQ_BUF_ADDR_MASK	UINT32_C(0xfffffffe)
	#define HWRM_FWD_REQ_CMPL_REQ_BUF_ADDR_SFT	1
} __attribute__((packed));

/* HWRM Asynchronous Event Completion Record	(16 bytes) */
struct hwrm_async_event_cmpl {
	uint16_t type;
	/* unused1 is 10 b */
	/*
	 * This field indicates the exact type of the completion. By
	 * convention, the LSB identifies the length of the record in
	 * 16B units. Even values indicate 16B records. Odd values
	 * indicate 32B records.
	 */
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_MASK	UINT32_C(0x3f)
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_SFT	0
	/* HWRM Asynchronous Event Information */
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_HWRM_ASYNC_EVENT	UINT32_C(0x2e)
	/* unused1 is 10 b */
	uint16_t event_id;
	/* Identifiers of events. */
	/* Link status changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE UINT32_C(0x0)
	/* Link MTU changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_MTU_CHANGE	UINT32_C(0x1)
	/* Link speed changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE UINT32_C(0x2)
	/* DCB Configuration changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE UINT32_C(0x3)
	/* Port connection not allowed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED UINT32_C(0x4)
	/* Link speed configuration was not allowed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_NOT_ALLOWED \
		UINT32_C(0x5)
	/* Link speed configuration change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE UINT32_C(0x6)
	/* Port PHY configuration change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_PHY_CFG_CHANGE UINT32_C(0x7)
	/* Function driver unloaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_UNLOAD UINT32_C(0x10)
	/* Function driver loaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_LOAD	UINT32_C(0x11)
	/* Function FLR related processing has completed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_FLR_PROC_CMPLT UINT32_C(0x12)
	/* PF driver unloaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD	UINT32_C(0x20)
	/* PF driver loaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_LOAD	UINT32_C(0x21)
	/* VF Function Level Reset	(FLR) */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_FLR	UINT32_C(0x30)
	/* VF MAC Address Change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_MAC_ADDR_CHANGE UINT32_C(0x31)
	/* PF-VF communication channel status change. */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_VF_COMM_STATUS_CHANGE \
		UINT32_C(0x32)
	/* VF Configuration Change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE	UINT32_C(0x33)
	/* LLFC/PFC Configuration Change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LLFC_PFC_CHANGE UINT32_C(0x34)
	/* HWRM Error */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_HWRM_ERROR	UINT32_C(0xff)
	uint32_t event_data2;
	/* Event specific data */
	uint8_t opaque_v;
	/* opaque is 7 b */
	/*
	 * This value is written by the NIC such that it will be
	 * different for each pass through the completion queue. The
	 * even passes will write 1. The odd passes will write 0.
	 */
	#define HWRM_ASYNC_EVENT_CMPL_V	UINT32_C(0x1)
	/* opaque is 7 b */
	#define HWRM_ASYNC_EVENT_CMPL_OPAQUE_MASK	UINT32_C(0xfe)
	#define HWRM_ASYNC_EVENT_CMPL_OPAQUE_SFT	1
	uint8_t timestamp_lo;
	/* 8-lsb timestamp from POR	(100-msec resolution) */
	uint16_t timestamp_hi;
	/* 16-lsb timestamp from POR	(100-msec resolution) */
	uint32_t event_data1;
	/* Event specific data */
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
/* Input	(24 bytes) */
struct hwrm_ver_get_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint8_t hwrm_intf_maj;
	/*
	 * This field represents the major version of HWRM interface
	 * specification supported by the driver HWRM implementation.
	 * The interface major version is intended to change only when
	 * non backward compatible changes are made to the HWRM
	 * interface specification.
	 */
	uint8_t hwrm_intf_min;
	/*
	 * This field represents the minor version of HWRM interface
	 * specification supported by the driver HWRM implementation. A
	 * change in interface minor version is used to reflect
	 * significant backward compatible modification to HWRM
	 * interface specification. This can be due to addition or
	 * removal of functionality. HWRM interface specifications with
	 * the same major version but different minor versions are
	 * compatible.
	 */
	uint8_t hwrm_intf_upd;
	/*
	 * This field represents the update version of HWRM interface
	 * specification supported by the driver HWRM implementation.
	 * The interface update version is used to reflect minor changes
	 * or bug fixes to a released HWRM interface specification.
	 */
	uint8_t unused_0[5];
} __attribute__((packed));

/* Output	(128 bytes) */
struct hwrm_ver_get_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint8_t hwrm_intf_maj;
	/*
	 * This field represents the major version of HWRM interface
	 * specification supported by the HWRM implementation. The
	 * interface major version is intended to change only when non
	 * backward compatible changes are made to the HWRM interface
	 * specification. A HWRM implementation that is compliant with
	 * this specification shall provide value of 1 in this field.
	 */
	uint8_t hwrm_intf_min;
	/*
	 * This field represents the minor version of HWRM interface
	 * specification supported by the HWRM implementation. A change
	 * in interface minor version is used to reflect significant
	 * backward compatible modification to HWRM interface
	 * specification. This can be due to addition or removal of
	 * functionality. HWRM interface specifications with the same
	 * major version but different minor versions are compatible. A
	 * HWRM implementation that is compliant with this specification
	 * shall provide value of 2 in this field.
	 */
	uint8_t hwrm_intf_upd;
	/*
	 * This field represents the update version of HWRM interface
	 * specification supported by the HWRM implementation. The
	 * interface update version is used to reflect minor changes or
	 * bug fixes to a released HWRM interface specification. A HWRM
	 * implementation that is compliant with this specification
	 * shall provide value of 2 in this field.
	 */
	uint8_t hwrm_intf_rsvd;
	uint8_t hwrm_fw_maj;
	/*
	 * This field represents the major version of HWRM firmware. A
	 * change in firmware major version represents a major firmware
	 * release.
	 */
	uint8_t hwrm_fw_min;
	/*
	 * This field represents the minor version of HWRM firmware. A
	 * change in firmware minor version represents significant
	 * firmware functionality changes.
	 */
	uint8_t hwrm_fw_bld;
	/*
	 * This field represents the build version of HWRM firmware. A
	 * change in firmware build version represents bug fixes to a
	 * released firmware.
	 */
	uint8_t hwrm_fw_rsvd;
	/*
	 * This field is a reserved field. This field can be used to
	 * represent firmware branches or customer specific releases
	 * tied to a specific	(major,minor,update) version of the HWRM
	 * firmware.
	 */
	uint8_t mgmt_fw_maj;
	/*
	 * This field represents the major version of mgmt firmware. A
	 * change in major version represents a major release.
	 */
	uint8_t mgmt_fw_min;
	/*
	 * This field represents the minor version of mgmt firmware. A
	 * change in minor version represents significant functionality
	 * changes.
	 */
	uint8_t mgmt_fw_bld;
	/*
	 * This field represents the build version of mgmt firmware. A
	 * change in update version represents bug fixes.
	 */
	uint8_t mgmt_fw_rsvd;
	/*
	 * This field is a reserved field. This field can be used to
	 * represent firmware branches or customer specific releases
	 * tied to a specific	(major,minor,update) version
	 */
	uint8_t netctrl_fw_maj;
	/*
	 * This field represents the major version of network control
	 * firmware. A change in major version represents a major
	 * release.
	 */
	uint8_t netctrl_fw_min;
	/*
	 * This field represents the minor version of network control
	 * firmware. A change in minor version represents significant
	 * functionality changes.
	 */
	uint8_t netctrl_fw_bld;
	/*
	 * This field represents the build version of network control
	 * firmware. A change in update version represents bug fixes.
	 */
	uint8_t netctrl_fw_rsvd;
	/*
	 * This field is a reserved field. This field can be used to
	 * represent firmware branches or customer specific releases
	 * tied to a specific	(major,minor,update) version
	 */
	uint32_t dev_caps_cfg;
	/*
	 * This field is used to indicate device's capabilities and
	 * configurations.
	 */
	/*
	 * If set to 1, then secure firmware update behavior is
	 * supported. If set to 0, then secure firmware update behavior
	 * is not supported.
	 */
	#define HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SECURE_FW_UPD_SUPPORTED  \
		UINT32_C(0x1)
	/*
	 * If set to 1, then firmware based DCBX agent is supported. If
	 * set to 0, then firmware based DCBX agent capability is not
	 * supported on this device.
	 */
	#define HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_FW_DCBX_AGENT_SUPPORTED  \
		UINT32_C(0x2)
	/*
	 * If set to 1, then HWRM short command format is supported. If
	 * set to 0, then HWRM short command format is not supported.
	 */
	#define HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED	 \
		UINT32_C(0x4)
	/*
	 * If set to 1, then HWRM short command format is required. If
	 * set to 0, then HWRM short command format is not required.
	 */
	#define HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_INPUTUIRED	\
		UINT32_C(0x8)
	uint8_t roce_fw_maj;
	/*
	 * This field represents the major version of RoCE firmware. A
	 * change in major version represents a major release.
	 */
	uint8_t roce_fw_min;
	/*
	 * This field represents the minor version of RoCE firmware. A
	 * change in minor version represents significant functionality
	 * changes.
	 */
	uint8_t roce_fw_bld;
	/*
	 * This field represents the build version of RoCE firmware. A
	 * change in update version represents bug fixes.
	 */
	uint8_t roce_fw_rsvd;
	/*
	 * This field is a reserved field. This field can be used to
	 * represent firmware branches or customer specific releases
	 * tied to a specific	(major,minor,update) version
	 */
	char hwrm_fw_name[16];
	/*
	 * This field represents the name of HWRM FW	(ASCII chars with
	 * NULL at the end).
	 */
	char mgmt_fw_name[16];
	/*
	 * This field represents the name of mgmt FW	(ASCII chars with
	 * NULL at the end).
	 */
	char netctrl_fw_name[16];
	/*
	 * This field represents the name of network control firmware
	 *	(ASCII chars with NULL at the end).
	 */
	uint32_t reserved2[4];
	/*
	 * This field is reserved for future use. The responder should
	 * set it to 0. The requester should ignore this field.
	 */
	char roce_fw_name[16];
	/*
	 * This field represents the name of RoCE FW	(ASCII chars with
	 * NULL at the end).
	 */
	uint16_t chip_num;
	/* This field returns the chip number. */
	uint8_t chip_rev;
	/* This field returns the revision of chip. */
	uint8_t chip_metal;
	/* This field returns the chip metal number. */
	uint8_t chip_bond_id;
	/* This field returns the bond id of the chip. */
	uint8_t chip_platform_type;
	/*
	 * This value indicates the type of platform used for chip
	 * implementation.
	 */
	/* ASIC */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_ASIC	UINT32_C(0x0)
	/* FPGA platform of the chip. */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_FPGA	UINT32_C(0x1)
	/* Palladium platform of the chip. */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_PALLADIUM UINT32_C(0x2)
	uint16_t max_req_win_len;
	/*
	 * This field returns the maximum value of request window that
	 * is supported by the HWRM. The request window is mapped into
	 * device address space using MMIO.
	 */
	uint16_t max_resp_len;
	/* This field returns the maximum value of response buffer in bytes. */
	uint16_t def_req_timeout;
	/*
	 * This field returns the default request timeout value in
	 * milliseconds.
	 */
	uint8_t init_pending;
	/*
	 * This field will indicate if any subsystems is not fully
	 * initialized.
	 */
	/*
	 * If set to 1, device is not ready. If set to 0, device is
	 * ready to accept all HWRM commands.
	 */
	#define HWRM_VER_GET_OUTPUT_INIT_PENDING_DEV_NOT_RDY UINT32_C(0x1)
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_reset */
/*
 * Description: This command resets a hardware function	(PCIe function) and
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
/* Input	(24 bytes) */
struct hwrm_func_reset_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the vf_id_valid field to be configured. */
	#define HWRM_FUNC_RESET_INPUT_ENABLES_VF_ID_VALID	UINT32_C(0x1)
	uint16_t vf_id;
	/*
	 * The ID of the VF that this PF is trying to reset. Only the
	 * parent PF shall be allowed to reset a child VF. A parent PF
	 * driver shall use this field only when a specific child VF is
	 * requested to be reset.
	 */
	uint8_t func_reset_level;
	/* This value indicates the level of a function reset. */
	/*
	 * Reset the caller function and its children
	 * VFs	(if any). If no children functions exist,
	 * then reset the caller function only.
	 */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETALL	UINT32_C(0x0)
	/* Reset the caller function only */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETME	UINT32_C(0x1)
	/*
	 * Reset all children VFs of the caller function
	 * driver if the caller is a PF driver. It is an
	 * error to specify this level by a VF driver.
	 * It is an error to specify this level by a PF
	 * driver with no children VFs.
	 */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETCHILDREN \
		UINT32_C(0x2)
	/*
	 * Reset a specific VF of the caller function
	 * driver if the caller is the parent PF driver.
	 * It is an error to specify this level by a VF
	 * driver. It is an error to specify this level
	 * by a PF driver that is not the parent of the
	 * VF that is being requested to reset.
	 */
	#define HWRM_FUNC_RESET_INPUT_FUNC_RESET_LEVEL_RESETVF	UINT32_C(0x3)
	uint8_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_reset_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_vf_cfg */
/*
 * Description: This command allows configuration of a VF by its driver. If this
 * function is called by a PF driver, then the HWRM shall fail this command. If
 * guest VLAN and/or MAC address are provided in this command, then the HWRM
 * shall set up appropriate MAC/VLAN filters for the VF that is being
 * configured. A VF driver should set VF MTU/MRU using this command prior to
 * allocating RX VNICs or TX rings for the corresponding VF.
 */
/* Input (32 bytes) */
struct hwrm_func_vf_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the mtu field to be configured. */
	#define HWRM_FUNC_VF_CFG_INPUT_ENABLES_MTU                 UINT32_C(0x1)
	/* This bit must be '1' for the guest_vlan field to be configured. */
	#define HWRM_FUNC_VF_CFG_INPUT_ENABLES_GUEST_VLAN          UINT32_C(0x2)
	/*
	 * This bit must be '1' for the async_event_cr field to be configured.
	 */
	#define HWRM_FUNC_VF_CFG_INPUT_ENABLES_ASYNC_EVENT_CR      UINT32_C(0x4)
	/* This bit must be '1' for the dflt_mac_addr field to be configured. */
	#define HWRM_FUNC_VF_CFG_INPUT_ENABLES_DFLT_MAC_ADDR       UINT32_C(0x8)
	uint16_t mtu;
	/*
	 * The maximum transmission unit requested on the function. The HWRM
	 * should make sure that the mtu of the function does not exceed the mtu
	 * of the physical port that this function is associated with. In
	 * addition to requesting mtu per function, it is possible to configure
	 * mtu per transmit ring. By default, the mtu of each transmit ring
	 * associated with a function is equal to the mtu of the function. The
	 * HWRM should make sure that the mtu of each transmit ring that is
	 * assigned to a function has a valid mtu.
	 */
	uint16_t guest_vlan;
	/*
	 * The guest VLAN for the function being configured. This field's format
	 * is same as 802.1Q Tag's Tag Control Information (TCI) format that
	 * includes both Priority Code Point (PCP) and VLAN Identifier (VID).
	 */
	uint16_t async_event_cr;
	/*
	 * ID of the target completion ring for receiving asynchronous event
	 * completions. If this field is not valid, then the HWRM shall use the
	 * default completion ring of the function that is being configured as
	 * the target completion ring for providing any asynchronous event
	 * completions for that function. If this field is valid, then the HWRM
	 * shall use the completion ring identified by this ID as the target
	 * completion ring for providing any asynchronous event completions for
	 * the function that is being configured.
	 */
	uint8_t dflt_mac_addr[6];
	/*
	 * This value is the current MAC address requested by the VF driver to
	 * be configured on this VF. A value of 00-00-00-00-00-00 indicates no
	 * MAC address configuration is requested by the VF driver. The parent
	 * PF driver may reject or overwrite this MAC address.
	 */
} __attribute__((packed));

/* Output (16 bytes) */

struct hwrm_func_vf_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_qcaps */
/*
 * Description: This command returns capabilities of a function. The input FID
 * value is used to indicate what function is being queried. This allows a
 * physical function driver to query virtual functions that are children of the
 * physical function. The output FID value is needed to configure Rings and
 * MSI-X vectors so their DMA operations appear correctly on the PCI bus.
 */
/* Input	(24 bytes) */
struct hwrm_func_qcaps_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being queried. 0xFF...
	 *	(All Fs) if the query is for the requesting function.
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(80 bytes) */
struct hwrm_func_qcaps_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t fid;
	/*
	 * FID value. This value is used to identify operations on the
	 * PCI bus as belonging to a particular PCI function.
	 */
	uint16_t port_id;
	/*
	 * Port ID of port that this function is associated with. Valid
	 * only for the PF. 0xFF...	(All Fs) if this function is not
	 * associated with any port. 0xFF...	(All Fs) if this function
	 * is called from a VF.
	 */
	uint32_t flags;
	/* If 1, then Push mode is supported on this function. */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PUSH_MODE_SUPPORTED UINT32_C(0x1)
	/*
	 * If 1, then the global MSI-X auto-masking is enabled for the
	 * device.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_GLOBAL_MSIX_AUTOMASKING	 \
		UINT32_C(0x2)
	/*
	 * If 1, then the Precision Time Protocol	(PTP) processing is
	 * supported on this function. The HWRM should enable PTP on
	 * only a single Physical Function	(PF) per port.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PTP_SUPPORTED	UINT32_C(0x4)
	/*
	 * If 1, then RDMA over Converged Ethernet	(RoCE) v1 is
	 * supported on this function.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ROCE_V1_SUPPORTED UINT32_C(0x8)
	/*
	 * If 1, then RDMA over Converged Ethernet	(RoCE) v2 is
	 * supported on this function.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ROCE_V2_SUPPORTED UINT32_C(0x10)
	/*
	 * If 1, then control and configuration of WoL magic packet are
	 * supported on this function.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_WOL_MAGICPKT_SUPPORTED	\
		UINT32_C(0x20)
	/*
	 * If 1, then control and configuration of bitmap pattern packet
	 * are supported on this function.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_WOL_BMP_SUPPORTED UINT32_C(0x40)
	/*
	 * If set to 1, then the control and configuration of rate limit
	 * of an allocated TX ring on the queried function is supported.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_TX_RING_RL_SUPPORTED UINT32_C(0x80)
	/*
	 * If 1, then control and configuration of minimum and maximum
	 * bandwidths are supported on the queried function.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_TX_BW_CFG_SUPPORTED UINT32_C(0x100)
	/*
	 * If the query is for a VF, then this flag shall be ignored. If
	 * this query is for a PF and this flag is set to 1, then the PF
	 * has the capability to set the rate limits on the TX rings of
	 * its children VFs. If this query is for a PF and this flag is
	 * set to 0, then the PF does not have the capability to set the
	 * rate limits on the TX rings of its children VFs.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_VF_TX_RING_RL_SUPPORTED	 \
		UINT32_C(0x200)
	/*
	 * If the query is for a VF, then this flag shall be ignored. If
	 * this query is for a PF and this flag is set to 1, then the PF
	 * has the capability to set the minimum and/or maximum
	 * bandwidths for its children VFs. If this query is for a PF
	 * and this flag is set to 0, then the PF does not have the
	 * capability to set the minimum or maximum bandwidths for its
	 * children VFs.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_VF_BW_CFG_SUPPORTED UINT32_C(0x400)
	/*
	 * Standard TX Ring mode is used for the allocation of TX ring
	 * and underlying scheduling resources that allow bandwidth
	 * reservation and limit settings on the queried function. If
	 * set to 1, then standard TX ring mode is supported on the
	 * queried function. If set to 0, then standard TX ring mode is
	 * not available on the queried function.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_STD_TX_RING_MODE_SUPPORTED   \
		UINT32_C(0x800)
	uint8_t mac_address[6];
	/*
	 * This value is current MAC address configured for this
	 * function. A value of 00-00-00-00-00-00 indicates no MAC
	 * address is currently configured.
	 */
	uint16_t max_rsscos_ctx;
	/*
	 * The maximum number of RSS/COS contexts that can be allocated
	 * to the function.
	 */
	uint16_t max_cmpl_rings;
	/*
	 * The maximum number of completion rings that can be allocated
	 * to the function.
	 */
	uint16_t max_tx_rings;
	/*
	 * The maximum number of transmit rings that can be allocated to
	 * the function.
	 */
	uint16_t max_rx_rings;
	/*
	 * The maximum number of receive rings that can be allocated to
	 * the function.
	 */
	uint16_t max_l2_ctxs;
	/*
	 * The maximum number of L2 contexts that can be allocated to
	 * the function.
	 */
	uint16_t max_vnics;
	/*
	 * The maximum number of VNICs that can be allocated to the
	 * function.
	 */
	uint16_t first_vf_id;
	/*
	 * The identifier for the first VF enabled on a PF. This is
	 * valid only on the PF with SR-IOV enabled. 0xFF...	(All Fs) if
	 * this command is called on a PF with SR-IOV disabled or on a
	 * VF.
	 */
	uint16_t max_vfs;
	/*
	 * The maximum number of VFs that can be allocated to the
	 * function. This is valid only on the PF with SR-IOV enabled.
	 * 0xFF...	(All Fs) if this command is called on a PF with SR-
	 * IOV disabled or on a VF.
	 */
	uint16_t max_stat_ctx;
	/*
	 * The maximum number of statistic contexts that can be
	 * allocated to the function.
	 */
	uint32_t max_encap_records;
	/*
	 * The maximum number of Encapsulation records that can be
	 * offloaded by this function.
	 */
	uint32_t max_decap_records;
	/*
	 * The maximum number of decapsulation records that can be
	 * offloaded by this function.
	 */
	uint32_t max_tx_em_flows;
	/*
	 * The maximum number of Exact Match	(EM) flows that can be
	 * offloaded by this function on the TX side.
	 */
	uint32_t max_tx_wm_flows;
	/*
	 * The maximum number of Wildcard Match	(WM) flows that can be
	 * offloaded by this function on the TX side.
	 */
	uint32_t max_rx_em_flows;
	/*
	 * The maximum number of Exact Match	(EM) flows that can be
	 * offloaded by this function on the RX side.
	 */
	uint32_t max_rx_wm_flows;
	/*
	 * The maximum number of Wildcard Match	(WM) flows that can be
	 * offloaded by this function on the RX side.
	 */
	uint32_t max_mcast_filters;
	/*
	 * The maximum number of multicast filters that can be supported
	 * by this function on the RX side.
	 */
	uint32_t max_flow_id;
	/*
	 * The maximum value of flow_id that can be supported in
	 * completion records.
	 */
	uint32_t max_hw_ring_grps;
	/*
	 * The maximum number of HW ring groups that can be supported on
	 * this function.
	 */
	uint16_t max_sp_tx_rings;
	/*
	 * The maximum number of strict priority transmit rings that can
	 * be allocated to the function. This number indicates the
	 * maximum number of TX rings that can be assigned strict
	 * priorities out of the maximum number of TX rings that can be
	 * allocated	(max_tx_rings) to the function.
	 */
	uint8_t unused_0;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_qcfg */
/*
 * Description: This command returns the current configuration of a function.
 * The input FID value is used to indicate what function is being queried. This
 * allows a physical function driver to query virtual functions that are
 * children of the physical function. The output FID value is needed to
 * configure Rings and MSI-X vectors so their DMA operations appear correctly on
 * the PCI bus. This command should be called by every driver after
 * 'hwrm_func_cfg' to get the actual number of resources allocated by the HWRM.
 * The values returned by hwrm_func_qcfg are the values the driver shall use.
 * These values may be different than what was originally requested in the
 * 'hwrm_func_cfg' command.
 */
/* Input	(24 bytes) */
struct hwrm_func_qcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being queried. 0xFF...
	 *	(All Fs) if the query is for the requesting function.
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(72 bytes) */
struct hwrm_func_qcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t fid;
	/*
	 * FID value. This value is used to identify operations on the
	 * PCI bus as belonging to a particular PCI function.
	 */
	uint16_t port_id;
	/*
	 * Port ID of port that this function is associated with.
	 * 0xFF...	(All Fs) if this function is not associated with any
	 * port.
	 */
	uint16_t vlan;
	/*
	 * This value is the current VLAN setting for this function. The
	 * value of 0 for this field indicates no priority tagging or
	 * VLAN is used. This field's format is same as 802.1Q Tag's Tag
	 * Control Information	(TCI) format that includes both Priority
	 * Code Point	(PCP) and VLAN Identifier	(VID).
	 */
	uint16_t flags;
	/*
	 * If 1, then magic packet based Out-Of-Box WoL is enabled on
	 * the port associated with this function.
	 */
	#define HWRM_FUNC_QCFG_OUTPUT_FLAGS_OOB_WOL_MAGICPKT_ENABLED	 \
		UINT32_C(0x1)
	/*
	 * If 1, then bitmap pattern based Out-Of-Box WoL packet is
	 * enabled on the port associated with this function.
	 */
	#define HWRM_FUNC_QCFG_OUTPUT_FLAGS_OOB_WOL_BMP_ENABLED	UINT32_C(0x2)
	/*
	 * If set to 1, then FW based DCBX agent is enabled and running
	 * on the port associated with this function. If set to 0, then
	 * DCBX agent is not running in the firmware.
	 */
	#define HWRM_FUNC_QCFG_OUTPUT_FLAGS_FW_DCBX_AGENT_ENABLED \
		UINT32_C(0x4)
	/*
	 * Standard TX Ring mode is used for the allocation of TX ring
	 * and underlying scheduling resources that allow bandwidth
	 * reservation and limit settings on the queried function. If
	 * set to 1, then standard TX ring mode is enabled on the
	 * queried function. If set to 0, then the standard TX ring mode
	 * is disabled on the queried function. In this extended TX ring
	 * resource mode, the minimum and maximum bandwidth settings are
	 * not supported to allow the allocation of TX rings to span
	 * multiple scheduler nodes.
	 */
	#define HWRM_FUNC_QCFG_OUTPUT_FLAGS_STD_TX_RING_MODE_ENABLED	 \
		UINT32_C(0x8)
	/*
	 * If set to 1 then FW based LLDP agent is enabled and running
	 * on the port associated with this function. If set to 0 then
	 * the LLDP agent is not running in the firmware.
	 */
	#define HWRM_FUNC_QCFG_OUTPUT_FLAGS_FW_LLDP_AGENT_ENABLED UINT32_C(0x10)
	/*
	 * If set to 1, then multi-host mode is active for this
	 * function. If set to 0, then multi-host mode is inactive for
	 * this function or not applicable for this device.
	 */
	#define HWRM_FUNC_QCFG_OUTPUT_FLAGS_MULTI_HOST		UINT32_C(0x20)
	uint8_t mac_address[6];
	/*
	 * This value is current MAC address configured for this
	 * function. A value of 00-00-00-00-00-00 indicates no MAC
	 * address is currently configured.
	 */
	uint16_t pci_id;
	/*
	 * This value is current PCI ID of this function. If ARI is
	 * enabled, then it is Bus Number	(8b):Function Number(8b).
	 * Otherwise, it is Bus Number	(8b):Device Number	(4b):Function
	 * Number(4b).
	 */
	uint16_t alloc_rsscos_ctx;
	/*
	 * The number of RSS/COS contexts currently allocated to the
	 * function.
	 */
	uint16_t alloc_cmpl_rings;
	/*
	 * The number of completion rings currently allocated to the
	 * function. This does not include the rings allocated to any
	 * children functions if any.
	 */
	uint16_t alloc_tx_rings;
	/*
	 * The number of transmit rings currently allocated to the
	 * function. This does not include the rings allocated to any
	 * children functions if any.
	 */
	uint16_t alloc_rx_rings;
	/*
	 * The number of receive rings currently allocated to the
	 * function. This does not include the rings allocated to any
	 * children functions if any.
	 */
	uint16_t alloc_l2_ctx;
	/* The allocated number of L2 contexts to the function. */
	uint16_t alloc_vnics;
	/* The allocated number of vnics to the function. */
	uint16_t mtu;
	/*
	 * The maximum transmission unit of the function. For rings
	 * allocated on this function, this default value is used if
	 * ring MTU is not specified.
	 */
	uint16_t mru;
	/*
	 * The maximum receive unit of the function. For vnics allocated
	 * on this function, this default value is used if vnic MRU is
	 * not specified.
	 */
	uint16_t stat_ctx_id;
	/* The statistics context assigned to a function. */
	uint8_t port_partition_type;
	/*
	 * The HWRM shall return Unknown value for this field when this
	 * command is used to query VF's configuration.
	 */
	/* Single physical function */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_SPF	UINT32_C(0x0)
	/* Multiple physical functions */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_MPFS	UINT32_C(0x1)
	/* Network Partitioning 1.0 */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_0 UINT32_C(0x2)
	/* Network Partitioning 1.5 */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_5 UINT32_C(0x3)
	/* Network Partitioning 2.0 */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR2_0 UINT32_C(0x4)
	/* Unknown */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_UNKNOWN UINT32_C(0xff)
	uint8_t port_pf_cnt;
	/*
	 * This field will indicate number of physical functions on this
	 * port_partition. HWRM shall return unavail (i.e. value of 0)
	 * for this field when this command is used to query VF's
	 * configuration or from older firmware that doesn't support
	 * this field.
	 */
	/* number of PFs is not available */
	#define HWRM_FUNC_QCFG_OUTPUT_PORT_PF_CNT_UNAVAIL	UINT32_C(0x0)
	uint16_t dflt_vnic_id;
	/* The default VNIC ID assigned to a function that is being queried. */
	uint16_t max_mtu_configured;
	/*
	 * This value specifies the MAX MTU that can be configured by
	 * host drivers. This 'max_mtu_configure' can be HW max MTU or
	 * OEM applications specified value. Host drivers can't
	 * configure the MTU greater than this value. Host drivers
	 * should read this value prior to configuring the MTU. FW will
	 * fail the host request with MTU greater than
	 * 'max_mtu_configured'.
	 */
	uint32_t min_bw;
	/*
	 * Minimum BW allocated for this function. The HWRM will
	 * translate this value into byte counter and time interval used
	 * for the scheduler inside the device. A value of 0 indicates
	 * the minimum bandwidth is not configured.
	 */
	/* The bandwidth value. */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_MASK UINT32_C(0xfffffff)
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_SFT	0
	/* The granularity of the value	(bits or bytes). */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_SCALE	UINT32_C(0x10000000)
	/* Value is in bits. */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_SCALE_BITS	(UINT32_C(0x0) << 28)
	/* Value is in bytes. */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_SCALE_BYTES \
		(UINT32_C(0x1) << 28)
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_SCALE_LAST \
		FUNC_QCFG_OUTPUT_MIN_BW_SCALE_BYTES
	/* bw_value_unit is 3 b */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_MASK	\
		UINT32_C(0xe0000000)
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_SFT	29
	/* Value is in Mb or MB	(base 10). */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_MEGA \
		(UINT32_C(0x0) << 29)
	/* Value is in Kb or KB	(base 10). */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_KILO \
		(UINT32_C(0x2) << 29)
	/* Value is in bits or bytes. */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_BASE \
		(UINT32_C(0x4) << 29)
	/* Value is in Gb or GB	(base 10). */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_GIGA \
		(UINT32_C(0x6) << 29)
	/* Value is in 1/100th of a percentage of total bandwidth. */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_PERCENT1_100 \
		(UINT32_C(0x1) << 29)
	/* Invalid unit */
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_INVALID \
		(UINT32_C(0x7) << 29)
	#define HWRM_FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_LAST \
		FUNC_QCFG_OUTPUT_MIN_BW_BW_VALUE_UNIT_INVALID
	uint32_t max_bw;
	/*
	 * Maximum BW allocated for this function. The HWRM will
	 * translate this value into byte counter and time interval used
	 * for the scheduler inside the device. A value of 0 indicates
	 * that the maximum bandwidth is not configured.
	 */
	/* The bandwidth value. */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_MASK UINT32_C(0xfffffff)
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_SFT	0
	/* The granularity of the value	(bits or bytes). */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_SCALE	UINT32_C(0x10000000)
	/* Value is in bits. */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_SCALE_BITS	(UINT32_C(0x0) << 28)
	/* Value is in bytes. */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_SCALE_BYTES \
		(UINT32_C(0x1) << 28)
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_SCALE_LAST \
		FUNC_QCFG_OUTPUT_MAX_BW_SCALE_BYTES
	/* bw_value_unit is 3 b */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_MASK	\
		UINT32_C(0xe0000000)
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_SFT	29
	/* Value is in Mb or MB	(base 10). */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_MEGA \
		(UINT32_C(0x0) << 29)
	/* Value is in Kb or KB	(base 10). */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_KILO \
		(UINT32_C(0x2) << 29)
	/* Value is in bits or bytes. */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_BASE \
		(UINT32_C(0x4) << 29)
	/* Value is in Gb or GB	(base 10). */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_GIGA \
		(UINT32_C(0x6) << 29)
	/* Value is in 1/100th of a percentage of total bandwidth. */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_PERCENT1_100 \
		(UINT32_C(0x1) << 29)
	/* Invalid unit */
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_INVALID \
		(UINT32_C(0x7) << 29)
	#define HWRM_FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_LAST \
		FUNC_QCFG_OUTPUT_MAX_BW_BW_VALUE_UNIT_INVALID
	uint8_t evb_mode;
	/*
	 * This value indicates the Edge virtual bridge mode for the
	 * domain that this function belongs to.
	 */
	/* No Edge Virtual Bridging	(EVB) */
	#define HWRM_FUNC_QCFG_OUTPUT_EVB_MODE_NO_EVB	UINT32_C(0x0)
	/* Virtual Ethernet Bridge	(VEB) */
	#define HWRM_FUNC_QCFG_OUTPUT_EVB_MODE_VEB	UINT32_C(0x1)
	/* Virtual Ethernet Port Aggregator	(VEPA) */
	#define HWRM_FUNC_QCFG_OUTPUT_EVB_MODE_VEPA	UINT32_C(0x2)
	uint8_t unused_0;
	uint16_t alloc_vfs;
	/*
	 * The number of VFs that are allocated to the function. This is
	 * valid only on the PF with SR-IOV enabled. 0xFF...	(All Fs) if
	 * this command is called on a PF with SR-IOV disabled or on a
	 * VF.
	 */
	uint32_t alloc_mcast_filters;
	/*
	 * The number of allocated multicast filters for this function
	 * on the RX side.
	 */
	uint32_t alloc_hw_ring_grps;
	/* The number of allocated HW ring groups for this function. */
	uint16_t alloc_sp_tx_rings;
	/*
	 * The number of strict priority transmit rings out of currently
	 * allocated TX rings to the function	(alloc_tx_rings).
	 */
	uint8_t unused_1;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_vlan_qcfg */
/*
 * Description: This command should be called by PF driver to get the current
 * C-TAG, S-TAG and correcponsing PCP and TPID values configured for the
 * function.
 */
/* Input (24 bytes) */
struct hwrm_func_vlan_qcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being configured. If set
	 * to 0xFF... (All Fs), then the configuration is for the
	 * requesting function.
	 */
	uint16_t unused_0[3];
};

/* Output (40 bytes) */
struct hwrm_func_vlan_qcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
	uint16_t stag_vid;
	/* S-TAG VLAN identifier configured for the function. */
	uint8_t stag_pcp;
	/* S-TAG PCP value configured for the function. */
	uint8_t unused_4;
	uint16_t stag_tpid;
	/*
	 * S-TAG TPID value configured for the function. This field is
	 * specified in network byte order.
	 */
	uint16_t ctag_vid;
	/* C-TAG VLAN identifier configured for the function. */
	uint8_t ctag_pcp;
	/* C-TAG PCP value configured for the function. */
	uint8_t unused_5;
	uint16_t ctag_tpid;
	/*
	 * C-TAG TPID value configured for the function. This field is
	 * specified in network byte order.
	 */
	uint32_t rsvd2;
	/* Future use. */
	uint32_t rsvd3;
	/* Future use. */
	uint32_t unused_6;
};

/* hwrm_func_vlan_cfg */
/*
 * Description: This command allows PF driver to configure C-TAG, S-TAG and
 * corresponding PCP and TPID values for a function.
 */
/* Input (48 bytes) */
struct hwrm_func_vlan_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being configured. If set
	 * to 0xFF... (All Fs), then the configuration is for the
	 * requesting function.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t enables;
	/* This bit must be '1' for the stag_vid field to be configured. */
	#define HWRM_FUNC_VLAN_CFG_INPUT_ENABLES_STAG_VID	UINT32_C(0x1)
	/* This bit must be '1' for the ctag_vid field to be configured. */
	#define HWRM_FUNC_VLAN_CFG_INPUT_ENABLES_CTAG_VID	UINT32_C(0x2)
	/* This bit must be '1' for the stag_pcp field to be configured. */
	#define HWRM_FUNC_VLAN_CFG_INPUT_ENABLES_STAG_PCP	UINT32_C(0x4)
	/* This bit must be '1' for the ctag_pcp field to be configured. */
	#define HWRM_FUNC_VLAN_CFG_INPUT_ENABLES_CTAG_PCP	UINT32_C(0x8)
	/* This bit must be '1' for the stag_tpid field to be configured. */
	#define HWRM_FUNC_VLAN_CFG_INPUT_ENABLES_STAG_TPID	UINT32_C(0x10)
	/* This bit must be '1' for the ctag_tpid field to be configured. */
	#define HWRM_FUNC_VLAN_CFG_INPUT_ENABLES_CTAG_TPID	UINT32_C(0x20)
	uint16_t stag_vid;
	/* S-TAG VLAN identifier configured for the function. */
	uint8_t stag_pcp;
	/* S-TAG PCP value configured for the function. */
	uint8_t unused_2;
	uint16_t stag_tpid;
	/*
	 * S-TAG TPID value configured for the function. This field is
	 * specified in network byte order.
	 */
	uint16_t ctag_vid;
	/* C-TAG VLAN identifier configured for the function. */
	uint8_t ctag_pcp;
	/* C-TAG PCP value configured for the function. */
	uint8_t unused_3;
	uint16_t ctag_tpid;
	/*
	 * C-TAG TPID value configured for the function. This field is
	 * specified in network byte order.
	 */
	uint32_t rsvd1;
	/* Future use. */
	uint32_t rsvd2;
	/* Future use. */
	uint32_t unused_4;
};

/* Output (16 bytes) */
struct hwrm_func_vlan_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
};

/* hwrm_func_cfg */
/*
 * Description: This command allows configuration of a PF by the corresponding
 * PF driver. This command also allows configuration of a child VF by its parent
 * PF driver. The input FID value is used to indicate what function is being
 * configured. This allows a PF driver to configure the PF owned by itself or a
 * virtual function that is a child of the PF. This command allows to reserve
 * resources for a VF by its parent PF. To reverse the process, the command
 * should be called with all enables flags cleared for resources. This will free
 * allocated resources for the VF and return them to the resource pool. If this
 * command is requested by a VF driver to configure or reserve resources, then
 * the HWRM shall fail this command. If default MAC address and/or VLAN are
 * provided in this command, then the HWRM shall set up appropriate MAC/VLAN
 * filters for the function that is being configured. If source properties
 * checks are enabled and default MAC address and/or IP address are provided in
 * this command, then the HWRM shall set appropriate source property checks
 * based on provided MAC and/or IP addresses. The parent PF driver should not
 * set MTU/MRU for a VF using this command. This is to allow MTU/MRU setting by
 * the VF driver. If the MTU or MRU for a VF is set by the PF driver, then the
 * HWRM should ignore it. A function's MTU/MRU should be set prior to allocating
 * RX VNICs or TX rings. A PF driver calls hwrm_func_cfg to allocate resources
 * for itself or its children VFs. All function drivers shall call hwrm_func_cfg
 * to reserve resources. A request to hwrm_func_cfg may not be fully granted;
 * that is, a request for resources may be larger than what can be supported by
 * the device and the HWRM will allocate the best set of resources available,
 * but that may be less than requested. If all the amounts requested could not
 * be fulfilled, the HWRM shall allocate what it could and return a status code
 * of success. A function driver should call hwrm_func_qcfg immediately after
 * hwrm_func_cfg to determine what resources were assigned to the configured
 * function. A call by a PF driver to hwrm_func_cfg to allocate resources for
 * itself shall only allocate resources for the PF driver to use, not for its
 * children VFs. Likewise, a call to hwrm_func_qcfg shall return the resources
 * available for the PF driver to use, not what is available to its children
 * VFs.
 */
/* Input	(88 bytes) */
struct hwrm_func_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being configured. If set
	 * to 0xFF...	(All Fs), then the configuration is for the
	 * requesting function.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t flags;
	/*
	 * When this bit is '1', the function is disabled with source
	 * MAC address check. This is an anti-spoofing check. If this
	 * flag is set, then the function shall be configured to
	 * disallow transmission of frames with the source MAC address
	 * that is configured for this function.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_SRC_MAC_ADDR_CHECK_DISABLE	 \
		UINT32_C(0x1)
	/*
	 * When this bit is '1', the function is enabled with source MAC
	 * address check. This is an anti-spoofing check. If this flag
	 * is set, then the function shall be configured to allow
	 * transmission of frames with the source MAC address that is
	 * configured for this function.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_SRC_MAC_ADDR_CHECK_ENABLE	\
		UINT32_C(0x2)
	/* reserved */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_RSVD_MASK	UINT32_C(0x1fc)
	#define HWRM_FUNC_CFG_INPUT_FLAGS_RSVD_SFT	2
	/*
	 * Standard TX Ring mode is used for the allocation of TX ring
	 * and underlying scheduling resources that allow bandwidth
	 * reservation and limit settings on the queried function. If
	 * set to 1, then standard TX ring mode is requested to be
	 * enabled on the function being configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE	\
		UINT32_C(0x200)
	/*
	 * Standard TX Ring mode is used for the allocation of TX ring
	 * and underlying scheduling resources that allow bandwidth
	 * reservation and limit settings on the queried function. If
	 * set to 1, then the standard TX ring mode is requested to be
	 * disabled on the function being configured. In this extended
	 * TX ring resource mode, the minimum and maximum bandwidth
	 * settings are not supported to allow the allocation of TX
	 * rings to span multiple scheduler nodes.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE	\
		UINT32_C(0x400)
	/*
	 * If this bit is set, virtual mac address configured in this
	 * command will be persistent over warm boot.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_VIRT_MAC_PERSIST	UINT32_C(0x800)
	/*
	 * This bit only applies to the VF. If this bit is set, the
	 * statistic context counters will not be cleared when the
	 * statistic context is freed or a function reset is called on
	 * VF. This bit will be cleared when the PF is unloaded or a
	 * function reset is called on the PF.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_NO_AUTOCLEAR_STATISTIC	\
		UINT32_C(0x1000)
	/*
	 * This bit requests that the firmware test to see if all the
	 * assets requested in this command (i.e. number of TX rings)
	 * are available. The firmware will return an error if the
	 * requested assets are not available. The firwmare will NOT
	 * reserve the assets if they are available.
	 */
	#define HWRM_FUNC_CFG_INPUT_FLAGS_TX_ASSETS_TEST UINT32_C(0x2000)
	uint32_t enables;
	/* This bit must be '1' for the mtu field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_MTU	UINT32_C(0x1)
	/* This bit must be '1' for the mru field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_MRU	UINT32_C(0x2)
	/*
	 * This bit must be '1' for the num_rsscos_ctxs field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS	UINT32_C(0x4)
	/*
	 * This bit must be '1' for the num_cmpl_rings field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_CMPL_RINGS	UINT32_C(0x8)
	/* This bit must be '1' for the num_tx_rings field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_TX_RINGS	UINT32_C(0x10)
	/* This bit must be '1' for the num_rx_rings field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RX_RINGS	UINT32_C(0x20)
	/* This bit must be '1' for the num_l2_ctxs field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_L2_CTXS	UINT32_C(0x40)
	/* This bit must be '1' for the num_vnics field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_VNICS	UINT32_C(0x80)
	/*
	 * This bit must be '1' for the num_stat_ctxs field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_STAT_CTXS	UINT32_C(0x100)
	/*
	 * This bit must be '1' for the dflt_mac_addr field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR	UINT32_C(0x200)
	/* This bit must be '1' for the dflt_vlan field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_VLAN	UINT32_C(0x400)
	/* This bit must be '1' for the dflt_ip_addr field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_IP_ADDR	UINT32_C(0x800)
	/* This bit must be '1' for the min_bw field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_MIN_BW	UINT32_C(0x1000)
	/* This bit must be '1' for the max_bw field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_MAX_BW	UINT32_C(0x2000)
	/*
	 * This bit must be '1' for the async_event_cr field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_ASYNC_EVENT_CR	UINT32_C(0x4000)
	/*
	 * This bit must be '1' for the vlan_antispoof_mode field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_VLAN_ANTISPOOF_MODE	UINT32_C(0x8000)
	/*
	 * This bit must be '1' for the allowed_vlan_pris field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_ALLOWED_VLAN_PRIS UINT32_C(0x10000)
	/* This bit must be '1' for the evb_mode field to be configured. */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_EVB_MODE	UINT32_C(0x20000)
	/*
	 * This bit must be '1' for the num_mcast_filters field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_MCAST_FILTERS UINT32_C(0x40000)
	/*
	 * This bit must be '1' for the num_hw_ring_grps field to be
	 * configured.
	 */
	#define HWRM_FUNC_CFG_INPUT_ENABLES_NUM_HW_RING_GRPS UINT32_C(0x80000)
	uint16_t mtu;
	/*
	 * The maximum transmission unit of the function. The HWRM
	 * should make sure that the mtu of the function does not exceed
	 * the mtu of the physical port that this function is associated
	 * with. In addition to configuring mtu per function, it is
	 * possible to configure mtu per transmit ring. By default, the
	 * mtu of each transmit ring associated with a function is equal
	 * to the mtu of the function. The HWRM should make sure that
	 * the mtu of each transmit ring that is assigned to a function
	 * has a valid mtu.
	 */
	uint16_t mru;
	/*
	 * The maximum receive unit of the function. The HWRM should
	 * make sure that the mru of the function does not exceed the
	 * mru of the physical port that this function is associated
	 * with. In addition to configuring mru per function, it is
	 * possible to configure mru per vnic. By default, the mru of
	 * each vnic associated with a function is equal to the mru of
	 * the function. The HWRM should make sure that the mru of each
	 * vnic that is assigned to a function has a valid mru.
	 */
	uint16_t num_rsscos_ctxs;
	/* The number of RSS/COS contexts requested for the function. */
	uint16_t num_cmpl_rings;
	/*
	 * The number of completion rings requested for the function.
	 * This does not include the rings allocated to any children
	 * functions if any.
	 */
	uint16_t num_tx_rings;
	/*
	 * The number of transmit rings requested for the function. This
	 * does not include the rings allocated to any children
	 * functions if any.
	 */
	uint16_t num_rx_rings;
	/*
	 * The number of receive rings requested for the function. This
	 * does not include the rings allocated to any children
	 * functions if any.
	 */
	uint16_t num_l2_ctxs;
	/* The requested number of L2 contexts for the function. */
	uint16_t num_vnics;
	/* The requested number of vnics for the function. */
	uint16_t num_stat_ctxs;
	/* The requested number of statistic contexts for the function. */
	uint16_t num_hw_ring_grps;
	/*
	 * The number of HW ring groups that should be reserved for this
	 * function.
	 */
	uint8_t dflt_mac_addr[6];
	/* The default MAC address for the function being configured. */
	uint16_t dflt_vlan;
	/*
	 * The default VLAN for the function being configured. This
	 * field's format is same as 802.1Q Tag's Tag Control
	 * Information	(TCI) format that includes both Priority Code
	 * Point	(PCP) and VLAN Identifier	(VID).
	 */
	uint32_t dflt_ip_addr[4];
	/*
	 * The default IP address for the function being configured.
	 * This address is only used in enabling source property check.
	 */
	uint32_t min_bw;
	/*
	 * Minimum BW allocated for this function. The HWRM will
	 * translate this value into byte counter and time interval used
	 * for the scheduler inside the device.
	 */
	/* The bandwidth value. */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_MASK UINT32_C(0xfffffff)
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_SFT	0
	/* The granularity of the value	(bits or bytes). */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_SCALE	UINT32_C(0x10000000)
	/* Value is in bits. */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_SCALE_BITS	(UINT32_C(0x0) << 28)
	/* Value is in bytes. */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_SCALE_BYTES	(UINT32_C(0x1) << 28)
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_SCALE_LAST \
		FUNC_CFG_INPUT_MIN_BW_SCALE_BYTES
	/* bw_value_unit is 3 b */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_MASK	 \
		UINT32_C(0xe0000000)
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_SFT	29
	/* Value is in Mb or MB	(base 10). */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_MEGA \
		(UINT32_C(0x0) << 29)
	/* Value is in Kb or KB	(base 10). */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_KILO \
		(UINT32_C(0x2) << 29)
	/* Value is in bits or bytes. */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_BASE \
		(UINT32_C(0x4) << 29)
	/* Value is in Gb or GB	(base 10). */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_GIGA \
		(UINT32_C(0x6) << 29)
	/* Value is in 1/100th of a percentage of total bandwidth. */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_PERCENT1_100 \
		(UINT32_C(0x1) << 29)
	/* Invalid unit */
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_INVALID \
		(UINT32_C(0x7) << 29)
	#define HWRM_FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_LAST \
		FUNC_CFG_INPUT_MIN_BW_BW_VALUE_UNIT_INVALID
	uint32_t max_bw;
	/*
	 * Maximum BW allocated for this function. The HWRM will
	 * translate this value into byte counter and time interval used
	 * for the scheduler inside the device.
	 */
	/* The bandwidth value. */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_MASK \
		UINT32_C(0xfffffff)
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_SFT	0
	/* The granularity of the value	(bits or bytes). */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_SCALE	UINT32_C(0x10000000)
	/* Value is in bits. */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_SCALE_BITS	(UINT32_C(0x0) << 28)
	/* Value is in bytes. */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_SCALE_BYTES	(UINT32_C(0x1) << 28)
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_SCALE_LAST \
		FUNC_CFG_INPUT_MAX_BW_SCALE_BYTES
	/* bw_value_unit is 3 b */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_MASK	 \
		UINT32_C(0xe0000000)
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_SFT	29
	/* Value is in Mb or MB	(base 10). */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_MEGA	\
		(UINT32_C(0x0) << 29)
	/* Value is in Kb or KB	(base 10). */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_KILO	\
		(UINT32_C(0x2) << 29)
	/* Value is in bits or bytes. */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_BASE \
		(UINT32_C(0x4) << 29)
	/* Value is in Gb or GB	(base 10). */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_GIGA \
		(UINT32_C(0x6) << 29)
	/* Value is in 1/100th of a percentage of total bandwidth. */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_PERCENT1_100 \
		(UINT32_C(0x1) << 29)
	/* Invalid unit */
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_INVALID \
		(UINT32_C(0x7) << 29)
	#define HWRM_FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_LAST \
		FUNC_CFG_INPUT_MAX_BW_BW_VALUE_UNIT_INVALID
	uint16_t async_event_cr;
	/*
	 * ID of the target completion ring for receiving asynchronous
	 * event completions. If this field is not valid, then the HWRM
	 * shall use the default completion ring of the function that is
	 * being configured as the target completion ring for providing
	 * any asynchronous event completions for that function. If this
	 * field is valid, then the HWRM shall use the completion ring
	 * identified by this ID as the target completion ring for
	 * providing any asynchronous event completions for the function
	 * that is being configured.
	 */
	uint8_t vlan_antispoof_mode;
	/* VLAN Anti-spoofing mode. */
	/* No VLAN anti-spoofing checks are enabled */
	#define HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_NOCHECK	UINT32_C(0x0)
	/* Validate VLAN against the configured VLAN(s) */
	#define HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_VALIDATE_VLAN \
		UINT32_C(0x1)
	/* Insert VLAN if it does not exist, otherwise discard */
	#define HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_INSERT_IF_VLANDNE \
		UINT32_C(0x2)
	/*
	 * Insert VLAN if it does not exist, override
	 * VLAN if it exists
	 */
	#define \
	HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_INSERT_OR_OVERRIDE_VLAN \
		UINT32_C(0x3)
	uint8_t allowed_vlan_pris;
	/*
	 * This bit field defines VLAN PRIs that are allowed on this
	 * function. If nth bit is set, then VLAN PRI n is allowed on
	 * this function.
	 */
	uint8_t evb_mode;
	/*
	 * The HWRM shall allow a PF driver to change EVB mode for the
	 * partition it belongs to. The HWRM shall not allow a VF driver
	 * to change the EVB mode. The HWRM shall take into account the
	 * switching of EVB mode from one to another and reconfigure
	 * hardware resources as appropriately. The switching from VEB
	 * to VEPA mode requires the disabling of the loopback traffic.
	 * Additionally, source knock outs are handled differently in
	 * VEB and VEPA modes.
	 */
	/* No Edge Virtual Bridging	(EVB) */
	#define HWRM_FUNC_CFG_INPUT_EVB_MODE_NO_EVB	UINT32_C(0x0)
	/* Virtual Ethernet Bridge	(VEB) */
	#define HWRM_FUNC_CFG_INPUT_EVB_MODE_VEB	UINT32_C(0x1)
	/* Virtual Ethernet Port Aggregator	(VEPA) */
	#define HWRM_FUNC_CFG_INPUT_EVB_MODE_VEPA	UINT32_C(0x2)
	uint8_t unused_2;
	uint16_t num_mcast_filters;
	/*
	 * The number of multicast filters that should be reserved for
	 * this function on the RX side.
	 */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_qstats */
/*
 * Description: This command returns statistics of a function. The input FID
 * value is used to indicate what function is being queried. This allows a
 * physical function driver to query virtual functions that are children of the
 * physical function. The HWRM shall return any unsupported counter with a value
 * of 0xFFFFFFFF for 32-bit counters and 0xFFFFFFFFFFFFFFFF for 64-bit counters.
 */
/* Input	(24 bytes) */
struct hwrm_func_qstats_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being queried. 0xFF...
	 *	(All Fs) if the query is for the requesting function.
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(176 bytes) */
struct hwrm_func_qstats_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint64_t tx_ucast_pkts;
	/* Number of transmitted unicast packets on the function. */
	uint64_t tx_mcast_pkts;
	/* Number of transmitted multicast packets on the function. */
	uint64_t tx_bcast_pkts;
	/* Number of transmitted broadcast packets on the function. */
	uint64_t tx_err_pkts;
	/*
	 * Number of transmitted packets that were discarded due to
	 * internal NIC resource problems. For transmit, this can only
	 * happen if TMP is configured to allow dropping in HOL blocking
	 * conditions, which is not a normal configuration.
	 */
	uint64_t tx_drop_pkts;
	/*
	 * Number of dropped packets on transmit path on the function.
	 * These are packets that have been marked for drop by the TE
	 * CFA block or are packets that exceeded the transmit MTU limit
	 * for the function.
	 */
	uint64_t tx_ucast_bytes;
	/* Number of transmitted bytes for unicast traffic on the function. */
	uint64_t tx_mcast_bytes;
	/*
	 * Number of transmitted bytes for multicast traffic on the
	 * function.
	 */
	uint64_t tx_bcast_bytes;
	/*
	 * Number of transmitted bytes for broadcast traffic on the
	 * function.
	 */
	uint64_t rx_ucast_pkts;
	/* Number of received unicast packets on the function. */
	uint64_t rx_mcast_pkts;
	/* Number of received multicast packets on the function. */
	uint64_t rx_bcast_pkts;
	/* Number of received broadcast packets on the function. */
	uint64_t rx_err_pkts;
	/*
	 * Number of received packets that were discarded on the
	 * function due to resource limitations. This can happen for 3
	 * reasons. # The BD used for the packet has a bad format. #
	 * There were no BDs available in the ring for the packet. #
	 * There were no BDs available on-chip for the packet.
	 */
	uint64_t rx_drop_pkts;
	/*
	 * Number of dropped packets on received path on the function.
	 * These are packets that have been marked for drop by the RE
	 * CFA.
	 */
	uint64_t rx_ucast_bytes;
	/* Number of received bytes for unicast traffic on the function. */
	uint64_t rx_mcast_bytes;
	/* Number of received bytes for multicast traffic on the function. */
	uint64_t rx_bcast_bytes;
	/* Number of received bytes for broadcast traffic on the function. */
	uint64_t rx_agg_pkts;
	/* Number of aggregated unicast packets on the function. */
	uint64_t rx_agg_bytes;
	/* Number of aggregated unicast bytes on the function. */
	uint64_t rx_agg_events;
	/* Number of aggregation events on the function. */
	uint64_t rx_agg_aborts;
	/* Number of aborted aggregations on the function. */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_clr_stats */
/*
 * Description: This command clears statistics of a function. The input FID
 * value is used to indicate what function's statistics is being cleared. This
 * allows a physical function driver to clear statistics of virtual functions
 * that are children of the physical function.
 */
/* Input	(24 bytes) */
struct hwrm_func_clr_stats_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function. 0xFF...	(All Fs) if the query is
	 * for the requesting function.
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_clr_stats_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_vf_vnic_ids_query */
/* Description: This command is used to query vf vnic ids. */
/* Input	(32 bytes) */
struct hwrm_func_vf_vnic_ids_query_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t vf_id;
	/*
	 * This value is used to identify a Virtual Function	(VF). The
	 * scope of VF ID is local within a PF.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t max_vnic_id_cnt;
	/* Max number of vnic ids in vnic id table */
	uint64_t vnic_id_tbl_addr;
	/* This is the address for VF VNIC ID table */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_vf_vnic_ids_query_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t vnic_id_cnt;
	/*
	 * Actual number of vnic ids Each VNIC ID is written as a 32-bit
	 * number.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_drv_rgtr */
/*
 * Description: This command is used by the function driver to register its
 * information with the HWRM. A function driver shall implement this command. A
 * function driver shall use this command during the driver initialization right
 * after the HWRM version discovery and default ring resources allocation.
 */
/* Input	(80 bytes) */
struct hwrm_func_drv_rgtr_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', the function driver is requesting all
	 * requests from its children VF drivers to be forwarded to
	 * itself. This flag can only be set by the PF driver. If a VF
	 * driver sets this flag, it should be ignored by the HWRM.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FWD_ALL_MODE	UINT32_C(0x1)
	/*
	 * When this bit is '1', the function is requesting none of the
	 * requests from its children VF drivers to be forwarded to
	 * itself. This flag can only be set by the PF driver. If a VF
	 * driver sets this flag, it should be ignored by the HWRM.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FWD_NONE_MODE	UINT32_C(0x2)
	uint32_t enables;
	/* This bit must be '1' for the os_type field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_OS_TYPE	UINT32_C(0x1)
	/* This bit must be '1' for the ver field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER	UINT32_C(0x2)
	/* This bit must be '1' for the timestamp field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_TIMESTAMP	UINT32_C(0x4)
	/* This bit must be '1' for the vf_req_fwd field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VF_INPUT_FWD	UINT32_C(0x8)
	/*
	 * This bit must be '1' for the async_event_fwd field to be
	 * configured.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD UINT32_C(0x10)
	uint16_t os_type;
	/*
	 * This value indicates the type of OS. The values are based on
	 * CIM_OperatingSystem.mof file as published by the DMTF.
	 */
	/* Unknown */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_UNKNOWN	UINT32_C(0x0)
	/* Other OS not listed below. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_OTHER	UINT32_C(0x1)
	/* MSDOS OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_MSDOS	UINT32_C(0xe)
	/* Windows OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WINDOWS	UINT32_C(0x12)
	/* Solaris OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_SOLARIS	UINT32_C(0x1d)
	/* Linux OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_LINUX	UINT32_C(0x24)
	/* FreeBSD OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_FREEBSD	UINT32_C(0x2a)
	/* VMware ESXi OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_ESXI	UINT32_C(0x68)
	/* Microsoft Windows 8 64-bit OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WIN864	UINT32_C(0x73)
	/* Microsoft Windows Server 2012 R2 OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WIN2012R2	UINT32_C(0x74)
	uint8_t ver_maj;
	/* This is the major version of the driver. */
	uint8_t ver_min;
	/* This is the minor version of the driver. */
	uint8_t ver_upd;
	/* This is the update version of the driver. */
	uint8_t unused_0;
	uint16_t unused_1;
	uint32_t timestamp;
	/*
	 * This is a 32-bit timestamp provided by the driver for keep
	 * alive. The timestamp is in multiples of 1ms.
	 */
	uint32_t unused_2;
	uint32_t vf_req_fwd[8];
	/*
	 * This is a 256-bit bit mask provided by the PF driver for
	 * letting the HWRM know what commands issued by the VF driver
	 * to the HWRM should be forwarded to the PF driver. Nth bit
	 * refers to the Nth req_type. Setting Nth bit to 1 indicates
	 * that requests from the VF driver with req_type equal to N
	 * shall be forwarded to the parent PF driver. This field is not
	 * valid for the VF driver.
	 */
	uint32_t async_event_fwd[8];
	/*
	 * This is a 256-bit bit mask provided by the function driver
	 *	(PF or VF driver) to indicate the list of asynchronous event
	 * completions to be forwarded. Nth bit refers to the Nth
	 * event_id. Setting Nth bit to 1 by the function driver shall
	 * result in the HWRM forwarding asynchronous event completion
	 * with event_id equal to N. If all bits are set to 0	(value of
	 * 0), then the HWRM shall not forward any asynchronous event
	 * completion to this function driver.
	 */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_drv_rgtr_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_drv_unrgtr */
/*
 * Description: This command is used by the function driver to un register with
 * the HWRM. A function driver shall implement this command. A function driver
 * shall use this command during the driver unloading.
 */
/* Input	(24 bytes) */
struct hwrm_func_drv_unrgtr_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', the function driver is notifying the
	 * HWRM to prepare for the shutdown.
	 */
	#define HWRM_FUNC_DRV_UNRGTR_INPUT_FLAGS_PREPARE_FOR_SHUTDOWN	\
		UINT32_C(0x1)
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_drv_unrgtr_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_buf_rgtr */
/*
 * Description: This command is used by the PF driver to register buffers used
 * in the PF-VF communication with the HWRM. The PF driver uses this command to
 * register buffers for each PF-VF channel. A parent PF may issue this command
 * per child VF. If VF ID is not valid, then this command is used to register
 * buffers for all children VFs of the PF.
 */
/* Input	(128 bytes) */
struct hwrm_func_buf_rgtr_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the vf_id field to be configured. */
	#define HWRM_FUNC_BUF_RGTR_INPUT_ENABLES_VF_ID	UINT32_C(0x1)
	/* This bit must be '1' for the err_buf_addr field to be configured. */
	#define HWRM_FUNC_BUF_RGTR_INPUT_ENABLES_ERR_BUF_ADDR	UINT32_C(0x2)
	uint16_t vf_id;
	/*
	 * This value is used to identify a Virtual Function	(VF). The
	 * scope of VF ID is local within a PF.
	 */
	uint16_t req_buf_num_pages;
	/*
	 * This field represents the number of pages used for request
	 * buffer(s).
	 */
	uint16_t req_buf_page_size;
	/* This field represents the page size used for request buffer(s). */
	/* 16 bytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_16B UINT32_C(0x4)
	/* 4 Kbytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_4K	UINT32_C(0xc)
	/* 8 Kbytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_8K	UINT32_C(0xd)
	/* 64 Kbytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_64K UINT32_C(0x10)
	/* 2 Mbytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_2M	UINT32_C(0x15)
	/* 4 Mbytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_4M	UINT32_C(0x16)
	/* 1 Gbytes */
	#define HWRM_FUNC_BUF_RGTR_INPUT_INPUT_BUF_PAGE_SIZE_1G	UINT32_C(0x1e)
	uint16_t req_buf_len;
	/* The length of the request buffer per VF in bytes. */
	uint16_t resp_buf_len;
	/* The length of the response buffer in bytes. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint64_t req_buf_page_addr[10];
	/* This field represents the page address of req buffer. */
	uint64_t error_buf_addr;
	/*
	 * This field is used to receive the error reporting from the
	 * chipset. Only applicable for PFs.
	 */
	uint64_t resp_buf_addr;
	/* This field is used to receive the response forwarded by the HWRM. */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_buf_rgtr_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_func_buf_unrgtr */
/*
 * Description: This command is used by the PF driver to unregister buffers used
 * in the PF-VF communication with the HWRM. The PF driver uses this command to
 * unregister buffers for PF-VF communication. A parent PF may issue this
 * command to unregister buffers for communication between the PF and a specific
 * VF. If the VF ID is not valid, then this command is used to unregister
 * buffers used for communications with all children VFs of the PF.
 */
/* Input	(24 bytes) */
struct hwrm_func_buf_unrgtr_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the vf_id field to be configured. */
	#define HWRM_FUNC_BUF_UNRGTR_INPUT_ENABLES_VF_ID	UINT32_C(0x1)
	uint16_t vf_id;
	/*
	 * This value is used to identify a Virtual Function	(VF). The
	 * scope of VF ID is local within a PF.
	 */
	uint16_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_func_buf_unrgtr_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
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
/* Input	(56 bytes) */
struct hwrm_port_phy_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is set to '1', the PHY for the port shall be
	 * reset. # If this bit is set to 1, then the HWRM shall reset
	 * the PHY after applying PHY configuration changes specified in
	 * this command. # In order to guarantee that PHY configuration
	 * changes specified in this command take effect, the HWRM
	 * client should set this flag to 1. # If this bit is not set to
	 * 1, then the HWRM may reset the PHY depending on the current
	 * PHY configuration and settings specified in this command.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY	UINT32_C(0x1)
	/* deprecated bit. Do not use!!! */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_DEPRECATED	UINT32_C(0x2)
	/*
	 * When this bit is set to '1', the link shall be forced to the
	 * force_link_speed value. When this bit is set to '1', the HWRM
	 * client should not enable any of the auto negotiation related
	 * fields represented by auto_XXX fields in this command. When
	 * this bit is set to '1' and the HWRM client has enabled a
	 * auto_XXX field in this command, then the HWRM shall ignore
	 * the enabled auto_XXX field. When this bit is set to zero, the
	 * link shall be allowed to autoneg.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE	UINT32_C(0x4)
	/*
	 * When this bit is set to '1', the auto-negotiation process
	 * shall be restarted on the link.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG	UINT32_C(0x8)
	/*
	 * When this bit is set to '1', Energy Efficient Ethernet	(EEE)
	 * is requested to be enabled on this link. If EEE is not
	 * supported on this port, then this flag shall be ignored by
	 * the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_ENABLE	UINT32_C(0x10)
	/*
	 * When this bit is set to '1', Energy Efficient Ethernet	(EEE)
	 * is requested to be disabled on this link. If EEE is not
	 * supported on this port, then this flag shall be ignored by
	 * the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_DISABLE	UINT32_C(0x20)
	/*
	 * When this bit is set to '1' and EEE is enabled on this link,
	 * then TX LPI is requested to be enabled on the link. If EEE is
	 * not supported on this port, then this flag shall be ignored
	 * by the HWRM. If EEE is disabled on this port, then this flag
	 * shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_TX_LPI_ENABLE	UINT32_C(0x40)
	/*
	 * When this bit is set to '1' and EEE is enabled on this link,
	 * then TX LPI is requested to be disabled on the link. If EEE
	 * is not supported on this port, then this flag shall be
	 * ignored by the HWRM. If EEE is disabled on this port, then
	 * this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_TX_LPI_DISABLE UINT32_C(0x80)
	/*
	 * When set to 1, then the HWRM shall enable FEC
	 * autonegotitation on this port if supported. When set to 0,
	 * then this flag shall be ignored. If FEC autonegotiation is
	 * not supported, then the HWRM shall ignore this flag.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FEC_AUTONEG_ENABLE UINT32_C(0x100)
	/*
	 * When set to 1, then the HWRM shall disable FEC
	 * autonegotiation on this port if supported. When set to 0,
	 * then this flag shall be ignored. If FEC autonegotiation is
	 * not supported, then the HWRM shall ignore this flag.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FEC_AUTONEG_DISABLE	\
		UINT32_C(0x200)
	/*
	 * When set to 1, then the HWRM shall enable FEC CLAUSE 74	(Fire
	 * Code) on this port if supported. When set to 0, then this
	 * flag shall be ignored. If FEC CLAUSE 74 is not supported,
	 * then the HWRM shall ignore this flag.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FEC_CLAUSE74_ENABLE	\
		UINT32_C(0x400)
	/*
	 * When set to 1, then the HWRM shall disable FEC CLAUSE 74
	 *	(Fire Code) on this port if supported. When set to 0, then
	 * this flag shall be ignored. If FEC CLAUSE 74 is not
	 * supported, then the HWRM shall ignore this flag.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FEC_CLAUSE74_DISABLE	\
		UINT32_C(0x800)
	/*
	 * When set to 1, then the HWRM shall enable FEC CLAUSE 91	(Reed
	 * Solomon) on this port if supported. When set to 0, then this
	 * flag shall be ignored. If FEC CLAUSE 91 is not supported,
	 * then the HWRM shall ignore this flag.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FEC_CLAUSE91_ENABLE	\
		UINT32_C(0x1000)
	/*
	 * When set to 1, then the HWRM shall disable FEC CLAUSE 91
	 *	(Reed Solomon) on this port if supported. When set to 0, then
	 * this flag shall be ignored. If FEC CLAUSE 91 is not
	 * supported, then the HWRM shall ignore this flag.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FEC_CLAUSE91_DISABLE	\
		UINT32_C(0x2000)
	/*
	 * When this bit is set to '1', the link shall be forced to be
	 * taken down. # When this bit is set to '1", all other command
	 * input settings related to the link speed shall be ignored.
	 * Once the link state is forced down, it can be explicitly
	 * cleared from that state by setting this flag to '0'. # If
	 * this flag is set to '0', then the link shall be cleared from
	 * forced down state if the link is in forced down state. There
	 * may be conditions	(e.g. out-of-band or sideband configuration
	 * changes for the link) outside the scope of the HWRM
	 * implementation that may clear forced down link state.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE_LINK_DWN UINT32_C(0x4000)
	uint32_t enables;
	/* This bit must be '1' for the auto_mode field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE	UINT32_C(0x1)
	/* This bit must be '1' for the auto_duplex field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_DUPLEX	UINT32_C(0x2)
	/* This bit must be '1' for the auto_pause field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE	UINT32_C(0x4)
	/*
	 * This bit must be '1' for the auto_link_speed field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED	UINT32_C(0x8)
	/*
	 * This bit must be '1' for the auto_link_speed_mask field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK	 \
		UINT32_C(0x10)
	/* This bit must be '1' for the wirespeed field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_WIOUTPUTEED	UINT32_C(0x20)
	/* This bit must be '1' for the lpbk field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_LPBK	UINT32_C(0x40)
	/* This bit must be '1' for the preemphasis field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_PREEMPHASIS	UINT32_C(0x80)
	/* This bit must be '1' for the force_pause field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAUSE	UINT32_C(0x100)
	/*
	 * This bit must be '1' for the eee_link_speed_mask field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_EEE_LINK_SPEED_MASK	\
		UINT32_C(0x200)
	/* This bit must be '1' for the tx_lpi_timer field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_TX_LPI_TIMER	UINT32_C(0x400)
	uint16_t port_id;
	/* Port ID of port that is to be configured. */
	uint16_t force_link_speed;
	/*
	 * This is the speed that will be used if the force bit is '1'.
	 * If unsupported speed is selected, an error will be generated.
	 */
	/* 100Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100MB	UINT32_C(0x1)
	/* 1Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_1GB	UINT32_C(0xa)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2GB	UINT32_C(0x14)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2_5GB	UINT32_C(0x19)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB	UINT32_C(0x64)
	/* 20Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_20GB	UINT32_C(0xc8)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_25GB	UINT32_C(0xfa)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB	UINT32_C(0x190)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB	UINT32_C(0x1f4)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100GB	UINT32_C(0x3e8)
	/* 10Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10MB	UINT32_C(0xffff)
	uint8_t auto_mode;
	/*
	 * This value is used to identify what autoneg mode is used when
	 * the link speed is not being forced.
	 */
	/*
	 * Disable autoneg or autoneg disabled. No
	 * speeds are selected.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_NONE	UINT32_C(0x0)
	/* Select all possible speeds for autoneg mode. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ALL_SPEEDS	UINT32_C(0x1)
	/*
	 * Select only the auto_link_speed speed for
	 * autoneg mode. This mode has been DEPRECATED.
	 * An HWRM client should not use this mode.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ONE_SPEED	UINT32_C(0x2)
	/*
	 * Select the auto_link_speed or any speed below
	 * that speed for autoneg. This mode has been
	 * DEPRECATED. An HWRM client should not use
	 * this mode.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ONE_OR_BELOW	UINT32_C(0x3)
	/*
	 * Select the speeds based on the corresponding
	 * link speed mask value that is provided.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK	UINT32_C(0x4)
	uint8_t auto_duplex;
	/*
	 * This is the duplex setting that will be used if the
	 * autoneg_mode is "one_speed" or "one_or_below".
	 */
	/* Half Duplex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF	UINT32_C(0x0)
	/* Full duplex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_FULL	UINT32_C(0x1)
	/* Both Half and Full dupex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH	UINT32_C(0x2)
	uint8_t auto_pause;
	/*
	 * This value is used to configure the pause that will be used
	 * for autonegotiation. Add text on the usage of auto_pause and
	 * force_pause.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages has
	 * been requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX	UINT32_C(0x2)
	/*
	 * When set to 1, the advertisement of pause is enabled. # When
	 * the auto_mode is not set to none and this flag is set to 1,
	 * then the auto_pause bits on this port are being advertised
	 * and autoneg pause results are being interpreted. # When the
	 * auto_mode is not set to none and this flag is set to 0, the
	 * pause is forced as indicated in force_pause, and also
	 * advertised as auto_pause bits, but the autoneg results are
	 * not interpreted since the pause configuration is being
	 * forced. # When the auto_mode is set to none and this flag is
	 * set to 1, auto_pause bits should be ignored and should be set
	 * to 0.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_AUTONEG_PAUSE UINT32_C(0x4)
	uint8_t unused_0;
	uint16_t auto_link_speed;
	/*
	 * This is the speed that will be used if the autoneg_mode is
	 * "one_speed" or "one_or_below". If an unsupported speed is
	 * selected, an error will be generated.
	 */
	/* 100Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100MB	UINT32_C(0x1)
	/* 1Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_1GB	UINT32_C(0xa)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2GB	UINT32_C(0x14)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2_5GB	UINT32_C(0x19)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_10GB	UINT32_C(0x64)
	/* 20Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_20GB	UINT32_C(0xc8)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_25GB	UINT32_C(0xfa)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_40GB	UINT32_C(0x190)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_50GB	UINT32_C(0x1f4)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100GB	UINT32_C(0x3e8)
	/* 10Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_10MB	UINT32_C(0xffff)
	uint16_t auto_link_speed_mask;
	/*
	 * This is a mask of link speeds that will be used if
	 * autoneg_mode is "mask". If unsupported speed is enabled an
	 * error will be generated.
	 */
	/* 100Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MBHD	 \
		UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB \
		UINT32_C(0x2)
	/* 1Gb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GBHD \
		UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GB	\
		UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2GB	\
		UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2_5GB	\
		UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10GB UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_20GB UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_25GB	\
		UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_40GB	\
		UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_50GB	\
		UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100GB	\
		UINT32_C(0x800)
	/* 10Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10MBHD	\
		UINT32_C(0x1000)
	/* 10Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10MB	\
		UINT32_C(0x2000)
	uint8_t wirespeed;
	/* This value controls the wirespeed feature. */
	/* Wirespeed feature is disabled. */
	#define HWRM_PORT_PHY_CFG_INPUT_WIOUTPUTEED_OFF	UINT32_C(0x0)
	/* Wirespeed feature is enabled. */
	#define HWRM_PORT_PHY_CFG_INPUT_WIOUTPUTEED_ON	UINT32_C(0x1)
	uint8_t lpbk;
	/* This value controls the loopback setting for the PHY. */
	/* No loopback is selected. Normal operation. */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_NONE	UINT32_C(0x0)
	/*
	 * The HW will be configured with local loopback
	 * such that host data is sent back to the host
	 * without modification.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_LOCAL	UINT32_C(0x1)
	/*
	 * The HW will be configured with remote
	 * loopback such that port logic will send
	 * packets back out the transmitter that are
	 * received.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_REMOTE	UINT32_C(0x2)
	uint8_t force_pause;
	/*
	 * This value is used to configure the pause that will be used
	 * for force mode.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX	UINT32_C(0x2)
	uint8_t unused_1;
	uint32_t preemphasis;
	/*
	 * This value controls the pre-emphasis to be used for the link.
	 * Driver should not set this value	(use enable.preemphasis = 0)
	 * unless driver is sure of setting. Normally HWRM FW will
	 * determine proper pre-emphasis.
	 */
	uint16_t eee_link_speed_mask;
	/*
	 * Setting for link speed mask that is used to advertise speeds
	 * during autonegotiation when EEE is enabled. This field is
	 * valid only when EEE is enabled. The speeds specified in this
	 * field shall be a subset of speeds specified in
	 * auto_link_speed_mask. If EEE is enabled,then at least one
	 * speed shall be provided in this mask.
	 */
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD1 UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_100MB UINT32_C(0x2)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD2 UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_1GB	UINT32_C(0x8)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD3 UINT32_C(0x10)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD4 UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_10GB UINT32_C(0x40)
	uint8_t unused_2;
	uint8_t unused_3;
	uint32_t tx_lpi_timer;
	uint32_t unused_4;
	/*
	 * Reuested setting of TX LPI timer in microseconds. This field
	 * is valid only when EEE is enabled and TX LPI is enabled.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_TX_LPI_TIMER_MASK UINT32_C(0xffffff)
	#define HWRM_PORT_PHY_CFG_INPUT_TX_LPI_TIMER_SFT	0
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_port_phy_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_port_phy_qcfg */
/* Description: This command queries the PHY configuration for the port. */
/* Input	(24 bytes) */
struct hwrm_port_phy_qcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t port_id;
	/* Port ID of port that is to be queried. */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(96 bytes) */
struct hwrm_port_phy_qcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint8_t link;
	/* This value indicates the current link status. */
	/* There is no link or cable detected. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_NO_LINK	UINT32_C(0x0)
	/* There is no link, but a cable has been detected. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SIGNAL	UINT32_C(0x1)
	/* There is a link. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK	UINT32_C(0x2)
	uint8_t unused_0;
	uint16_t link_speed;
	/* This value indicates the current link speed of the connection. */
	/* 100Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100MB	UINT32_C(0x1)
	/* 1Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB	UINT32_C(0xa)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2GB	UINT32_C(0x14)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2_5GB	UINT32_C(0x19)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB	UINT32_C(0x64)
	/* 20Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB	UINT32_C(0xc8)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB	UINT32_C(0xfa)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB	UINT32_C(0x190)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB	UINT32_C(0x1f4)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100GB	UINT32_C(0x3e8)
	/* 10Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10MB	UINT32_C(0xffff)
	uint8_t duplex_cfg;
	/* This value is indicates the duplex of the current connection. */
	/* Half Duplex connection. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_CFG_HALF UINT32_C(0x0)
	/* Full duplex connection. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_CFG_FULL UINT32_C(0x1)
	uint8_t pause;
	/*
	 * This value is used to indicate the current pause
	 * configuration. When autoneg is enabled, this value represents
	 * the autoneg results of pause configuration.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PAUSE_RX	UINT32_C(0x2)
	uint16_t support_speeds;
	/*
	 * The supported speeds for the port. This is a bit mask. For
	 * each speed that is supported, the corrresponding bit will be
	 * set to '1'.
	 */
	/* 100Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100MBHD UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100MB UINT32_C(0x2)
	/* 1Gb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_1GBHD UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_1GB	UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_2GB	UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_2_5GB UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_10GB UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_20GB UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_25GB UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_40GB UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_50GB UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_100GB UINT32_C(0x800)
	/* 10Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_10MBHD	UINT32_C(0x1000)
	/* 10Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_SUPPORT_SPEEDS_10MB UINT32_C(0x2000)
	uint16_t force_link_speed;
	/*
	 * Current setting of forced link speed. When the link speed is
	 * not being forced, this value shall be set to 0.
	 */
	/* 100Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_100MB UINT32_C(0x1)
	/* 1Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_1GB	UINT32_C(0xa)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_2GB	UINT32_C(0x14)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_2_5GB UINT32_C(0x19)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_10GB	UINT32_C(0x64)
	/* 20Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_20GB	UINT32_C(0xc8)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_25GB	UINT32_C(0xfa)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_40GB UINT32_C(0x190)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_50GB UINT32_C(0x1f4)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_100GB UINT32_C(0x3e8)
	/* 10Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_LINK_SPEED_10MB UINT32_C(0xffff)
	uint8_t auto_mode;
	/* Current setting of auto negotiation mode. */
	/*
	 * Disable autoneg or autoneg disabled. No
	 * speeds are selected.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE	UINT32_C(0x0)
	/* Select all possible speeds for autoneg mode. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_ALL_SPEEDS	UINT32_C(0x1)
	/*
	 * Select only the auto_link_speed speed for
	 * autoneg mode. This mode has been DEPRECATED.
	 * An HWRM client should not use this mode.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_ONE_SPEED	UINT32_C(0x2)
	/*
	 * Select the auto_link_speed or any speed below
	 * that speed for autoneg. This mode has been
	 * DEPRECATED. An HWRM client should not use
	 * this mode.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_ONE_OR_BELOW UINT32_C(0x3)
	/*
	 * Select the speeds based on the corresponding
	 * link speed mask value that is provided.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_SPEED_MASK	UINT32_C(0x4)
	uint8_t auto_pause;
	/*
	 * Current setting of pause autonegotiation. Move autoneg_pause
	 * flag here.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages has
	 * been requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_PAUSE_RX	UINT32_C(0x2)
	/*
	 * When set to 1, the advertisement of pause is enabled. # When
	 * the auto_mode is not set to none and this flag is set to 1,
	 * then the auto_pause bits on this port are being advertised
	 * and autoneg pause results are being interpreted. # When the
	 * auto_mode is not set to none and this flag is set to 0, the
	 * pause is forced as indicated in force_pause, and also
	 * advertised as auto_pause bits, but the autoneg results are
	 * not interpreted since the pause configuration is being
	 * forced. # When the auto_mode is set to none and this flag is
	 * set to 1, auto_pause bits should be ignored and should be set
	 * to 0.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_PAUSE_AUTONEG_PAUSE UINT32_C(0x4)
	uint16_t auto_link_speed;
	/*
	 * Current setting for auto_link_speed. This field is only valid
	 * when auto_mode is set to "one_speed" or "one_or_below".
	 */
	/* 100Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_100MB	UINT32_C(0x1)
	/* 1Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_1GB	UINT32_C(0xa)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_2GB	UINT32_C(0x14)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_2_5GB	UINT32_C(0x19)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_10GB	UINT32_C(0x64)
	/* 20Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_20GB	UINT32_C(0xc8)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_25GB	UINT32_C(0xfa)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_40GB	UINT32_C(0x190)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_50GB	UINT32_C(0x1f4)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_100GB UINT32_C(0x3e8)
	/* 10Mb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_10MB UINT32_C(0xffff)
	uint16_t auto_link_speed_mask;
	/*
	 * Current setting for auto_link_speed_mask that is used to
	 * advertise speeds during autonegotiation. This field is only
	 * valid when auto_mode is set to "mask". The speeds specified
	 * in this field shall be a subset of supported speeds on this
	 * port.
	 */
	/* 100Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_100MBHD	\
		UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_100MB	 \
		UINT32_C(0x2)
	/* 1Gb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_1GBHD	 \
		UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_1GB UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_2GB	\
		UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_2_5GB	 \
		UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_10GB	\
		UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_20GB	\
		UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_25GB	\
		UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_40GB	\
		UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_50GB	\
		UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_100GB	 \
		UINT32_C(0x800)
	/* 10Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_10MBHD	\
		UINT32_C(0x1000)
	/* 10Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_LINK_SPEED_MASK_10MB	\
		UINT32_C(0x2000)
	uint8_t wirespeed;
	/* Current setting for wirespeed. */
	/* Wirespeed feature is disabled. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_WIOUTPUTEED_OFF	UINT32_C(0x0)
	/* Wirespeed feature is enabled. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_WIOUTPUTEED_ON	UINT32_C(0x1)
	uint8_t lpbk;
	/* Current setting for loopback. */
	/* No loopback is selected. Normal operation. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LPBK_NONE	UINT32_C(0x0)
	/*
	 * The HW will be configured with local loopback
	 * such that host data is sent back to the host
	 * without modification.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LPBK_LOCAL	UINT32_C(0x1)
	/*
	 * The HW will be configured with remote
	 * loopback such that port logic will send
	 * packets back out the transmitter that are
	 * received.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LPBK_REMOTE	UINT32_C(0x2)
	uint8_t force_pause;
	/*
	 * Current setting of forced pause. When the pause configuration
	 * is not being forced, then this value shall be set to 0.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_PAUSE_TX	UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FORCE_PAUSE_RX	UINT32_C(0x2)
	uint8_t module_status;
	/*
	 * This value indicates the current status of the optics module
	 * on this port.
	 */
	/* Module is inserted and accepted */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_NONE	UINT32_C(0x0)
	/* Module is rejected and transmit side Laser is disabled. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_DISABLETX UINT32_C(0x1)
	/* Module mismatch warning. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_WARNINGMSG UINT32_C(0x2)
	/* Module is rejected and powered down. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_PWRDOWN	UINT32_C(0x3)
	/* Module is not inserted. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_NOTINSERTED \
		UINT32_C(0x4)
	/* Module status is not applicable. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MODULE_STATUS_NOTAPPLICABLE \
		UINT32_C(0xff)
	uint32_t preemphasis;
	/* Current setting for preemphasis. */
	uint8_t phy_maj;
	/* This field represents the major version of the PHY. */
	uint8_t phy_min;
	/* This field represents the minor version of the PHY. */
	uint8_t phy_bld;
	/* This field represents the build version of the PHY. */
	uint8_t phy_type;
	/* This value represents a PHY type. */
	/* Unknown */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_UNKNOWN	UINT32_C(0x0)
	/* BASE-CR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASECR	UINT32_C(0x1)
	/* BASE-KR4	(Deprecated) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR4	UINT32_C(0x2)
	/* BASE-LR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASELR	UINT32_C(0x3)
	/* BASE-SR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASESR	UINT32_C(0x4)
	/* BASE-KR2	(Deprecated) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR2	UINT32_C(0x5)
	/* BASE-KX */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKX	UINT32_C(0x6)
	/* BASE-KR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASEKR	UINT32_C(0x7)
	/* BASE-T */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET	UINT32_C(0x8)
	/* EEE capable BASE-T */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASETE	UINT32_C(0x9)
	/* SGMII connected external PHY */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_SGMIIEXTPHY	UINT32_C(0xa)
	/* 25G_BASECR_CA_L */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASECR_CA_L UINT32_C(0xb)
	/* 25G_BASECR_CA_S */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASECR_CA_S UINT32_C(0xc)
	/* 25G_BASECR_CA_N */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASECR_CA_N UINT32_C(0xd)
	/* 25G_BASESR */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_25G_BASESR	UINT32_C(0xe)
	/* 100G_BASECR4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASECR4	UINT32_C(0xf)
	/* 100G_BASESR4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR4	UINT32_C(0x10)
	/* 100G_BASELR4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASELR4	UINT32_C(0x11)
	/* 100G_BASEER4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASEER4	UINT32_C(0x12)
	/* 100G_BASESR10 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_100G_BASESR10 UINT32_C(0x13)
	/* 40G_BASECR4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASECR4	UINT32_C(0x14)
	/* 40G_BASESR4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASESR4	UINT32_C(0x15)
	/* 40G_BASELR4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASELR4	UINT32_C(0x16)
	/* 40G_BASEER4 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_BASEER4	UINT32_C(0x17)
	/* 40G_ACTIVE_CABLE */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_40G_ACTIVE_CABLE \
		UINT32_C(0x18)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASET UINT32_C(0x19)
	/* 1G_baseSX */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASESX UINT32_C(0x1a)
	/* 1G_baseCX */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_1G_BASECX UINT32_C(0x1b)
	uint8_t media_type;
	/* This value represents a media type. */
	/* Unknown */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_UNKNOWN	UINT32_C(0x0)
	/* Twisted Pair */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_TP	UINT32_C(0x1)
	/* Direct Attached Copper */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_DAC	UINT32_C(0x2)
	/* Fiber */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_FIBRE	UINT32_C(0x3)
	uint8_t xcvr_pkg_type;
	/* This value represents a transceiver type. */
	/* PHY and MAC are in the same package */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_PKG_TYPE_XCVR_INTERNAL \
		UINT32_C(0x1)
	/* PHY and MAC are in different packages */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_PKG_TYPE_XCVR_EXTERNAL \
		UINT32_C(0x2)
	uint8_t eee_config_phy_addr;
	/*
	 * This field represents flags related to EEE configuration.
	 * These EEE configuration flags are valid only when the
	 * auto_mode is not set to none	(in other words autonegotiation
	 * is enabled).
	 */
	/* This field represents PHY address. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_ADDR_MASK	UINT32_C(0x1f)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PHY_ADDR_SFT	0
	/*
	 * When set to 1, Energy Efficient Ethernet	(EEE) mode is
	 * enabled. Speeds for autoneg with EEE mode enabled are based
	 * on eee_link_speed_mask.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_EEE_ENABLED UINT32_C(0x20)
	/*
	 * This flag is valid only when eee_enabled is set to 1. # If
	 * eee_enabled is set to 0, then EEE mode is disabled and this
	 * flag shall be ignored. # If eee_enabled is set to 1 and this
	 * flag is set to 1, then Energy Efficient Ethernet	(EEE) mode
	 * is enabled and in use. # If eee_enabled is set to 1 and this
	 * flag is set to 0, then Energy Efficient Ethernet	(EEE) mode
	 * is enabled but is currently not in use.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_EEE_ACTIVE	UINT32_C(0x40)
	/*
	 * This flag is valid only when eee_enabled is set to 1. # If
	 * eee_enabled is set to 0, then EEE mode is disabled and this
	 * flag shall be ignored. # If eee_enabled is set to 1 and this
	 * flag is set to 1, then Energy Efficient Ethernet	(EEE) mode
	 * is enabled and TX LPI is enabled. # If eee_enabled is set to
	 * 1 and this flag is set to 0, then Energy Efficient Ethernet
	 *	(EEE) mode is enabled but TX LPI is disabled.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_EEE_TX_LPI	UINT32_C(0x80)
	/*
	 * This field represents flags related to EEE configuration.
	 * These EEE configuration flags are valid only when the
	 * auto_mode is not set to none	(in other words autonegotiation
	 * is enabled).
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_MASK	UINT32_C(0xe0)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_EEE_CONFIG_SFT	5
	uint8_t parallel_detect;
	/* Reserved field, set to 0 */
	/*
	 * When set to 1, the parallel detection is used to determine
	 * the speed of the link partner. Parallel detection is used
	 * when a autonegotiation capable device is connected to a link
	 * parter that is not capable of autonegotiation.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_PARALLEL_DETECT	UINT32_C(0x1)
	/* Reserved field, set to 0 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_RESERVED_MASK	UINT32_C(0xfe)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_RESERVED_SFT	1
	uint16_t link_partner_adv_speeds;
	/*
	 * The advertised speeds for the port by the link partner. Each
	 * advertised speed will be set to '1'.
	 */
	/* 100Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_100MBHD \
		UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_100MB   \
		UINT32_C(0x2)
	/* 1Gb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_1GBHD   \
		UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_1GB	\
		UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_2GB	\
		UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_2_5GB   \
		UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_10GB	\
		UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_20GB	\
		UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_25GB	\
		UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_40GB	\
		UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_50GB	\
		UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_100GB   \
		UINT32_C(0x800)
	/* 10Mb link speed	(Half-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_10MBHD  \
		UINT32_C(0x1000)
	/* 10Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_SPEEDS_10MB	\
		UINT32_C(0x2000)
	uint8_t link_partner_adv_auto_mode;
	/*
	 * The advertised autoneg for the port by the link partner. This
	 * field is deprecated and should be set to 0.
	 */
	/*
	 * Disable autoneg or autoneg disabled. No
	 * speeds are selected.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_NONE \
		UINT32_C(0x0)
	/* Select all possible speeds for autoneg mode. */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_ALL_SPEEDS \
		UINT32_C(0x1)
	/*
	 * Select only the auto_link_speed speed for
	 * autoneg mode. This mode has been DEPRECATED.
	 * An HWRM client should not use this mode.
	 */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_ONE_SPEED \
		UINT32_C(0x2)
	/*
	 * Select the auto_link_speed or any speed below
	 * that speed for autoneg. This mode has been
	 * DEPRECATED. An HWRM client should not use
	 * this mode.
	 */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_ONE_OR_BELOW \
		UINT32_C(0x3)
	/*
	 * Select the speeds based on the corresponding
	 * link speed mask value that is provided.
	 */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_AUTO_MODE_SPEED_MASK \
		UINT32_C(0x4)
	uint8_t link_partner_adv_pause;
	/* The advertised pause settings on the port by the link partner. */
	/*
	 * When this bit is '1', Generation of tx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_PAUSE_TX	\
		UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is
	 * supported. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_PAUSE_RX	\
		UINT32_C(0x2)
	uint16_t adv_eee_link_speed_mask;
	/*
	 * Current setting for link speed mask that is used to advertise
	 * speeds during autonegotiation when EEE is enabled. This field
	 * is valid only when eee_enabled flags is set to 1. The speeds
	 * specified in this field shall be a subset of speeds specified
	 * in auto_link_speed_mask.
	 */
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD1   \
		UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_100MB   \
		UINT32_C(0x2)
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD2   \
		UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_1GB	\
		UINT32_C(0x8)
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD3   \
		UINT32_C(0x10)
	/* Reserved */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_RSVD4   \
		UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_ADV_EEE_LINK_SPEED_MASK_10GB	\
		UINT32_C(0x40)
	uint16_t link_partner_adv_eee_link_speed_mask;
	/*
	 * Current setting for link speed mask that is advertised by the
	 * link partner when EEE is enabled. This field is valid only
	 * when eee_enabled flags is set to 1.
	 */
	/* Reserved */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_RSVD1 \
		UINT32_C(0x1)
	/* 100Mb link speed	(Full-duplex) */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_100MB \
		UINT32_C(0x2)
	/* Reserved */
	#define \
	HWRM_PORT_PHY_QCFG_OUTPUT_LINK_PARTNER_ADV_EEE_LINK_SPEED_MASK_RSVD2 \
		UINT32_C(0x4)
	/* 1Gb link speed	(Full-duplex) */
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
	uint32_t xcvr_identifier_type_tx_lpi_timer;
	/* This value represents transceiver identifier type. */
	/*
	 * Current setting of TX LPI timer in microseconds. This field
	 * is valid only when_eee_enabled flag is set to 1 and
	 * tx_lpi_enabled is set to 1.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_TX_LPI_TIMER_MASK UINT32_C(0xffffff)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_TX_LPI_TIMER_SFT	0
	/* This value represents transceiver identifier type. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_MASK	\
		UINT32_C(0xff000000)
	#define HWRM_PORT_PHY_QCFG_OUTPUT_XCVR_IDENTIFIER_TYPE_SFT	24
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
	uint16_t fec_cfg;
	/*
	 * This value represents the current configuration of Forward
	 * Error Correction	(FEC) on the port.
	 */
	/*
	 * When set to 1, then FEC is not supported on this port. If
	 * this flag is set to 1, then all other FEC configuration flags
	 * shall be ignored. When set to 0, then FEC is supported as
	 * indicated by other configuration flags. If no cable is
	 * attached and the HWRM does not yet know the FEC capability,
	 * then the HWRM shall set this flag to 1 when reporting FEC
	 * capability.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_NONE_SUPPORTED	 \
		UINT32_C(0x1)
	/*
	 * When set to 1, then FEC autonegotiation is supported on this
	 * port. When set to 0, then FEC autonegotiation is not
	 * supported on this port.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_AUTONEG_SUPPORTED   \
		UINT32_C(0x2)
	/*
	 * When set to 1, then FEC autonegotiation is enabled on this
	 * port. When set to 0, then FEC autonegotiation is disabled if
	 * supported. This flag should be ignored if FEC autonegotiation
	 * is not supported on this port.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_AUTONEG_ENABLED	\
		UINT32_C(0x4)
	/*
	 * When set to 1, then FEC CLAUSE 74	(Fire Code) is supported on
	 * this port. When set to 0, then FEC CLAUSE 74	(Fire Code) is
	 * not supported on this port.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_CLAUSE74_SUPPORTED  \
		UINT32_C(0x8)
	/*
	 * When set to 1, then FEC CLAUSE 74	(Fire Code) is enabled on
	 * this port. When set to 0, then FEC CLAUSE 74	(Fire Code) is
	 * disabled if supported. This flag should be ignored if FEC
	 * CLAUSE 74 is not supported on this port.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_CLAUSE74_ENABLED	\
		UINT32_C(0x10)
	/*
	 * When set to 1, then FEC CLAUSE 91	(Reed Solomon) is supported
	 * on this port. When set to 0, then FEC CLAUSE 91	(Reed
	 * Solomon) is not supported on this port.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_CLAUSE91_SUPPORTED  \
		UINT32_C(0x20)
	/*
	 * When set to 1, then FEC CLAUSE 91	(Reed Solomon) is enabled
	 * on this port. When set to 0, then FEC CLAUSE 91	(Reed
	 * Solomon) is disabled if supported. This flag should be
	 * ignored if FEC CLAUSE 91 is not supported on this port.
	 */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_FEC_CFG_FEC_CLAUSE91_ENABLED	\
		UINT32_C(0x40)
	uint8_t duplex_state;
	/*
	 * This value is indicates the duplex of the current connection
	 * state.
	 */
	/* Half Duplex connection. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_STATE_HALF UINT32_C(0x0)
	/* Full duplex connection. */
	#define HWRM_PORT_PHY_QCFG_OUTPUT_DUPLEX_STATE_FULL UINT32_C(0x1)
	uint8_t unused_1;
	char phy_vendor_name[16];
	/*
	 * Up to 16 bytes of null padded ASCII string representing PHY
	 * vendor. If the string is set to null, then the vendor name is
	 * not available.
	 */
	char phy_vendor_partnumber[16];
	/*
	 * Up to 16 bytes of null padded ASCII string that identifies
	 * vendor specific part number of the PHY. If the string is set
	 * to null, then the vendor specific part number is not
	 * available.
	 */
	uint32_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;
	uint8_t unused_5;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_port_qstats */
/* Description: This function returns per port Ethernet statistics. */
/* Input	(40 bytes) */
struct hwrm_port_qstats_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t port_id;
	/* Port ID of port that is being queried. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2[3];
	uint8_t unused_3;
	uint64_t tx_stat_host_addr;
	/* This is the host address where Tx port statistics will be stored */
	uint64_t rx_stat_host_addr;
	/* This is the host address where Rx port statistics will be stored */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_port_qstats_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t tx_stat_size;
	/* The size of TX port statistics block in bytes. */
	uint16_t rx_stat_size;
	/* The size of RX port statistics block in bytes. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_port_clr_stats */
/*
 * Description: This function clears per port statistics. The HWRM shall not
 * allow a VF driver to clear port statistics. The HWRM shall not allow a PF
 * driver to clear port statistics in a partitioning mode. The HWRM may allow a
 * PF driver to clear port statistics in the non-partitioning mode.
 */
/* Input	(24 bytes) */
struct hwrm_port_clr_stats_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t port_id;
	/* Port ID of port that is being queried. */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_port_clr_stats_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_port_led_cfg */
/*
 * Description: This function is used to configure LEDs on a given port. Each
 * port has individual set of LEDs associated with it. These LEDs are used for
 * speed/link configuration as well as activity indicator configuration. Up to
 * three LEDs can be configured, one for activity and two for speeds.
 */
/* Input	(64 bytes) */
struct hwrm_port_led_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the led0_id field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_ID	UINT32_C(0x1)
	/* This bit must be '1' for the led0_state field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_STATE	UINT32_C(0x2)
	/* This bit must be '1' for the led0_color field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_COLOR	UINT32_C(0x4)
	/*
	 * This bit must be '1' for the led0_blink_on field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_BLINK_ON	UINT32_C(0x8)
	/*
	 * This bit must be '1' for the led0_blink_off field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_BLINK_OFF UINT32_C(0x10)
	/*
	 * This bit must be '1' for the led0_group_id field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_GROUP_ID UINT32_C(0x20)
	/* This bit must be '1' for the led1_id field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED1_ID	UINT32_C(0x40)
	/* This bit must be '1' for the led1_state field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED1_STATE	UINT32_C(0x80)
	/* This bit must be '1' for the led1_color field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED1_COLOR	UINT32_C(0x100)
	/*
	 * This bit must be '1' for the led1_blink_on field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED1_BLINK_ON UINT32_C(0x200)
	/*
	 * This bit must be '1' for the led1_blink_off field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED1_BLINK_OFF UINT32_C(0x400)
	/*
	 * This bit must be '1' for the led1_group_id field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED1_GROUP_ID UINT32_C(0x800)
	/* This bit must be '1' for the led2_id field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED2_ID	UINT32_C(0x1000)
	/* This bit must be '1' for the led2_state field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED2_STATE	UINT32_C(0x2000)
	/* This bit must be '1' for the led2_color field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED2_COLOR	UINT32_C(0x4000)
	/*
	 * This bit must be '1' for the led2_blink_on field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED2_BLINK_ON UINT32_C(0x8000)
	/*
	 * This bit must be '1' for the led2_blink_off field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED2_BLINK_OFF UINT32_C(0x10000)
	/*
	 * This bit must be '1' for the led2_group_id field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED2_GROUP_ID UINT32_C(0x20000)
	/* This bit must be '1' for the led3_id field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED3_ID	UINT32_C(0x40000)
	/* This bit must be '1' for the led3_state field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED3_STATE  UINT32_C(0x80000)
	/* This bit must be '1' for the led3_color field to be configured. */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED3_COLOR  UINT32_C(0x100000)
	/*
	 * This bit must be '1' for the led3_blink_on field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED3_BLINK_ON UINT32_C(0x200000)
	/*
	 * This bit must be '1' for the led3_blink_off field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED3_BLINK_OFF	\
		UINT32_C(0x400000)
	/*
	 * This bit must be '1' for the led3_group_id field to be
	 * configured.
	 */
	#define HWRM_PORT_LED_CFG_INPUT_ENABLES_LED3_GROUP_ID UINT32_C(0x800000)
	uint16_t port_id;
	/* Port ID of port whose LEDs are configured. */
	uint8_t num_leds;
	/*
	 * The number of LEDs that are being configured. Up to 4 LEDs
	 * can be configured with this command.
	 */
	uint8_t rsvd;
	/* Reserved field. */
	uint8_t led0_id;
	/* An identifier for the LED #0. */
	uint8_t led0_state;
	/* The requested state of the LED #0. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led0_color;
	/* The requested color of LED #0. */
	/* Default */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED0_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_0;
	uint16_t led0_blink_on;
	/*
	 * If the LED #0 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led0_blink_off;
	/*
	 * If the LED #0 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led0_group_id;
	/*
	 * An identifier for the group of LEDs that LED #0 belongs to.
	 * If set to 0, then the LED #0 shall not be grouped and shall
	 * be treated as an individual resource. For all other non-zero
	 * values of this field, LED #0 shall be grouped together with
	 * the LEDs with the same group ID value.
	 */
	uint8_t rsvd0;
	/* Reserved field. */
	uint8_t led1_id;
	/* An identifier for the LED #1. */
	uint8_t led1_state;
	/* The requested state of the LED #1. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led1_color;
	/* The requested color of LED #1. */
	/* Default */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED1_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_1;
	uint16_t led1_blink_on;
	/*
	 * If the LED #1 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led1_blink_off;
	/*
	 * If the LED #1 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led1_group_id;
	/*
	 * An identifier for the group of LEDs that LED #1 belongs to.
	 * If set to 0, then the LED #1 shall not be grouped and shall
	 * be treated as an individual resource. For all other non-zero
	 * values of this field, LED #1 shall be grouped together with
	 * the LEDs with the same group ID value.
	 */
	uint8_t rsvd1;
	/* Reserved field. */
	uint8_t led2_id;
	/* An identifier for the LED #2. */
	uint8_t led2_state;
	/* The requested state of the LED #2. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led2_color;
	/* The requested color of LED #2. */
	/* Default */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED2_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_2;
	uint16_t led2_blink_on;
	/*
	 * If the LED #2 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led2_blink_off;
	/*
	 * If the LED #2 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led2_group_id;
	/*
	 * An identifier for the group of LEDs that LED #2 belongs to.
	 * If set to 0, then the LED #2 shall not be grouped and shall
	 * be treated as an individual resource. For all other non-zero
	 * values of this field, LED #2 shall be grouped together with
	 * the LEDs with the same group ID value.
	 */
	uint8_t rsvd2;
	/* Reserved field. */
	uint8_t led3_id;
	/* An identifier for the LED #3. */
	uint8_t led3_state;
	/* The requested state of the LED #3. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led3_color;
	/* The requested color of LED #3. */
	/* Default */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_CFG_INPUT_LED3_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_3;
	uint16_t led3_blink_on;
	/*
	 * If the LED #3 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led3_blink_off;
	/*
	 * If the LED #3 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led3_group_id;
	/*
	 * An identifier for the group of LEDs that LED #3 belongs to.
	 * If set to 0, then the LED #3 shall not be grouped and shall
	 * be treated as an individual resource. For all other non-zero
	 * values of this field, LED #3 shall be grouped together with
	 * the LEDs with the same group ID value.
	 */
	uint8_t rsvd3;
	/* Reserved field. */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_port_led_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_port_led_qcfg */
/*
 * Description: This function is used to query configuration of LEDs on a given
 * port. Each port has individual set of LEDs associated with it. These LEDs are
 * used for speed/link configuration as well as activity indicator
 * configuration. Up to three LEDs can be configured, one for activity and two
 * for speeds.
 */
/* Input	(24 bytes) */
struct hwrm_port_led_qcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t port_id;
	/* Port ID of port whose LED configuration is being queried. */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(56 bytes) */
struct hwrm_port_led_qcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint8_t num_leds;
	/*
	 * The number of LEDs that are configured on this port. Up to 4
	 * LEDs can be returned in the response.
	 */
	uint8_t led0_id;
	/* An identifier for the LED #0. */
	uint8_t led0_type;
	/* The type of LED #0. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led0_state;
	/* The current state of the LED #0. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led0_color;
	/* The color of LED #0. */
	/* Default */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED0_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_0;
	uint16_t led0_blink_on;
	/*
	 * If the LED #0 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led0_blink_off;
	/*
	 * If the LED #0 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led0_group_id;
	/*
	 * An identifier for the group of LEDs that LED #0 belongs to.
	 * If set to 0, then the LED #0 is not grouped. For all other
	 * non-zero values of this field, LED #0 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t led1_id;
	/* An identifier for the LED #1. */
	uint8_t led1_type;
	/* The type of LED #1. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led1_state;
	/* The current state of the LED #1. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led1_color;
	/* The color of LED #1. */
	/* Default */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED1_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_1;
	uint16_t led1_blink_on;
	/*
	 * If the LED #1 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led1_blink_off;
	/*
	 * If the LED #1 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led1_group_id;
	/*
	 * An identifier for the group of LEDs that LED #1 belongs to.
	 * If set to 0, then the LED #1 is not grouped. For all other
	 * non-zero values of this field, LED #1 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t led2_id;
	/* An identifier for the LED #2. */
	uint8_t led2_type;
	/* The type of LED #2. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led2_state;
	/* The current state of the LED #2. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led2_color;
	/* The color of LED #2. */
	/* Default */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED2_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_2;
	uint16_t led2_blink_on;
	/*
	 * If the LED #2 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led2_blink_off;
	/*
	 * If the LED #2 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led2_group_id;
	/*
	 * An identifier for the group of LEDs that LED #2 belongs to.
	 * If set to 0, then the LED #2 is not grouped. For all other
	 * non-zero values of this field, LED #2 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t led3_id;
	/* An identifier for the LED #3. */
	uint8_t led3_type;
	/* The type of LED #3. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led3_state;
	/* The current state of the LED #3. */
	/* Default state of the LED */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_STATE_DEFAULT	UINT32_C(0x0)
	/* Off */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_STATE_OFF	UINT32_C(0x1)
	/* On */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_STATE_ON	UINT32_C(0x2)
	/* Blink */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_STATE_BLINK	UINT32_C(0x3)
	/* Blink Alternately */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_STATE_BLINKALT	UINT32_C(0x4)
	uint8_t led3_color;
	/* The color of LED #3. */
	/* Default */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_COLOR_DEFAULT	UINT32_C(0x0)
	/* Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_COLOR_AMBER	UINT32_C(0x1)
	/* Green */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_COLOR_GREEN	UINT32_C(0x2)
	/* Green or Amber */
	#define HWRM_PORT_LED_QCFG_OUTPUT_LED3_COLOR_GREENAMBER	UINT32_C(0x3)
	uint8_t unused_3;
	uint16_t led3_blink_on;
	/*
	 * If the LED #3 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED on
	 * between cycles.
	 */
	uint16_t led3_blink_off;
	/*
	 * If the LED #3 state is "blink" or "blinkalt", then this field
	 * represents the requested time in milliseconds to keep LED off
	 * between cycles.
	 */
	uint8_t led3_group_id;
	/*
	 * An identifier for the group of LEDs that LED #3 belongs to.
	 * If set to 0, then the LED #3 is not grouped. For all other
	 * non-zero values of this field, LED #3 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t unused_4;
	uint16_t unused_5;
	uint8_t unused_6;
	uint8_t unused_7;
	uint8_t unused_8;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_port_led_qcaps */
/*
 * Description: This function is used to query capabilities of LEDs on a given
 * port. Each port has individual set of LEDs associated with it. These LEDs are
 * used for speed/link configuration as well as activity indicator
 * configuration.
 */
/* Input	(24 bytes) */
struct hwrm_port_led_qcaps_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t port_id;
	/* Port ID of port whose LED configuration is being queried. */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(48 bytes) */
struct hwrm_port_led_qcaps_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint8_t num_leds;
	/*
	 * The number of LEDs that are configured on this port. Up to 4
	 * LEDs can be returned in the response.
	 */
	uint8_t unused_0[3];
	/* Reserved for future use. */
	uint8_t led0_id;
	/* An identifier for the LED #0. */
	uint8_t led0_type;
	/* The type of LED #0. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led0_group_id;
	/*
	 * An identifier for the group of LEDs that LED #0 belongs to.
	 * If set to 0, then the LED #0 cannot be grouped. For all other
	 * non-zero values of this field, LED #0 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t unused_1;
	uint16_t led0_state_caps;
	/* The states supported by LED #0. */
	/*
	 * If set to 1, this LED is enabled. If set to 0, this LED is
	 * disabled.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_STATE_CAPS_ENABLED UINT32_C(0x1)
	/*
	 * If set to 1, off state is supported on this LED. If set to 0,
	 * off state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_STATE_CAPS_OFF_SUPPORTED  \
		UINT32_C(0x2)
	/*
	 * If set to 1, on state is supported on this LED. If set to 0,
	 * on state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_STATE_CAPS_ON_SUPPORTED   \
		UINT32_C(0x4)
	/*
	 * If set to 1, blink state is supported on this LED. If set to
	 * 0, blink state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_STATE_CAPS_BLINK_SUPPORTED \
		UINT32_C(0x8)
	/*
	 * If set to 1, blink_alt state is supported on this LED. If set
	 * to 0, blink_alt state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_STATE_CAPS_BLINK_ALT_SUPPORTED \
		UINT32_C(0x10)
	uint16_t led0_color_caps;
	/* The colors supported by LED #0. */
	/* reserved */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_COLOR_CAPS_RSVD	UINT32_C(0x1)
	/*
	 * If set to 1, Amber color is supported on this LED. If set to
	 * 0, Amber color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_COLOR_CAPS_AMBER_SUPPORTED \
		UINT32_C(0x2)
	/*
	 * If set to 1, Green color is supported on this LED. If set to
	 * 0, Green color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED0_COLOR_CAPS_GREEN_SUPPORTED \
		UINT32_C(0x4)
	uint8_t led1_id;
	/* An identifier for the LED #1. */
	uint8_t led1_type;
	/* The type of LED #1. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led1_group_id;
	/*
	 * An identifier for the group of LEDs that LED #1 belongs to.
	 * If set to 0, then the LED #0 cannot be grouped. For all other
	 * non-zero values of this field, LED #0 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t unused_2;
	uint16_t led1_state_caps;
	/* The states supported by LED #1. */
	/*
	 * If set to 1, this LED is enabled. If set to 0, this LED is
	 * disabled.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_STATE_CAPS_ENABLED UINT32_C(0x1)
	/*
	 * If set to 1, off state is supported on this LED. If set to 0,
	 * off state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_STATE_CAPS_OFF_SUPPORTED  \
		UINT32_C(0x2)
	/*
	 * If set to 1, on state is supported on this LED. If set to 0,
	 * on state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_STATE_CAPS_ON_SUPPORTED   \
		UINT32_C(0x4)
	/*
	 * If set to 1, blink state is supported on this LED. If set to
	 * 0, blink state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_STATE_CAPS_BLINK_SUPPORTED \
		UINT32_C(0x8)
	/*
	 * If set to 1, blink_alt state is supported on this LED. If set
	 * to 0, blink_alt state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_STATE_CAPS_BLINK_ALT_SUPPORTED \
		UINT32_C(0x10)
	uint16_t led1_color_caps;
	/* The colors supported by LED #1. */
	/* reserved */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_COLOR_CAPS_RSVD	UINT32_C(0x1)
	/*
	 * If set to 1, Amber color is supported on this LED. If set to
	 * 0, Amber color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_COLOR_CAPS_AMBER_SUPPORTED \
		UINT32_C(0x2)
	/*
	 * If set to 1, Green color is supported on this LED. If set to
	 * 0, Green color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED1_COLOR_CAPS_GREEN_SUPPORTED \
		UINT32_C(0x4)
	uint8_t led2_id;
	/* An identifier for the LED #2. */
	uint8_t led2_type;
	/* The type of LED #2. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led2_group_id;
	/*
	 * An identifier for the group of LEDs that LED #0 belongs to.
	 * If set to 0, then the LED #0 cannot be grouped. For all other
	 * non-zero values of this field, LED #0 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t unused_3;
	uint16_t led2_state_caps;
	/* The states supported by LED #2. */
	/*
	 * If set to 1, this LED is enabled. If set to 0, this LED is
	 * disabled.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_STATE_CAPS_ENABLED UINT32_C(0x1)
	/*
	 * If set to 1, off state is supported on this LED. If set to 0,
	 * off state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_STATE_CAPS_OFF_SUPPORTED  \
		UINT32_C(0x2)
	/*
	 * If set to 1, on state is supported on this LED. If set to 0,
	 * on state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_STATE_CAPS_ON_SUPPORTED   \
		UINT32_C(0x4)
	/*
	 * If set to 1, blink state is supported on this LED. If set to
	 * 0, blink state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_STATE_CAPS_BLINK_SUPPORTED \
		UINT32_C(0x8)
	/*
	 * If set to 1, blink_alt state is supported on this LED. If set
	 * to 0, blink_alt state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_STATE_CAPS_BLINK_ALT_SUPPORTED \
		UINT32_C(0x10)
	uint16_t led2_color_caps;
	/* The colors supported by LED #2. */
	/* reserved */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_COLOR_CAPS_RSVD	UINT32_C(0x1)
	/*
	 * If set to 1, Amber color is supported on this LED. If set to
	 * 0, Amber color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_COLOR_CAPS_AMBER_SUPPORTED \
		UINT32_C(0x2)
	/*
	 * If set to 1, Green color is supported on this LED. If set to
	 * 0, Green color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED2_COLOR_CAPS_GREEN_SUPPORTED \
		UINT32_C(0x4)
	uint8_t led3_id;
	/* An identifier for the LED #3. */
	uint8_t led3_type;
	/* The type of LED #3. */
	/* Speed LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_TYPE_SPEED	UINT32_C(0x0)
	/* Activity LED */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_TYPE_ACTIVITY	UINT32_C(0x1)
	/* Invalid */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_TYPE_INVALID	UINT32_C(0xff)
	uint8_t led3_group_id;
	/*
	 * An identifier for the group of LEDs that LED #3 belongs to.
	 * If set to 0, then the LED #0 cannot be grouped. For all other
	 * non-zero values of this field, LED #0 is grouped together
	 * with the LEDs with the same group ID value.
	 */
	uint8_t unused_4;
	uint16_t led3_state_caps;
	/* The states supported by LED #3. */
	/*
	 * If set to 1, this LED is enabled. If set to 0, this LED is
	 * disabled.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_STATE_CAPS_ENABLED UINT32_C(0x1)
	/*
	 * If set to 1, off state is supported on this LED. If set to 0,
	 * off state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_STATE_CAPS_OFF_SUPPORTED  \
		UINT32_C(0x2)
	/*
	 * If set to 1, on state is supported on this LED. If set to 0,
	 * on state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_STATE_CAPS_ON_SUPPORTED   \
		UINT32_C(0x4)
	/*
	 * If set to 1, blink state is supported on this LED. If set to
	 * 0, blink state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_STATE_CAPS_BLINK_SUPPORTED \
		UINT32_C(0x8)
	/*
	 * If set to 1, blink_alt state is supported on this LED. If set
	 * to 0, blink_alt state is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_STATE_CAPS_BLINK_ALT_SUPPORTED \
		UINT32_C(0x10)
	uint16_t led3_color_caps;
	/* The colors supported by LED #3. */
	/* reserved */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_COLOR_CAPS_RSVD	UINT32_C(0x1)
	/*
	 * If set to 1, Amber color is supported on this LED. If set to
	 * 0, Amber color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_COLOR_CAPS_AMBER_SUPPORTED \
		UINT32_C(0x2)
	/*
	 * If set to 1, Green color is supported on this LED. If set to
	 * 0, Green color is not supported on this LED.
	 */
	#define HWRM_PORT_LED_QCAPS_OUTPUT_LED3_COLOR_CAPS_GREEN_SUPPORTED \
		UINT32_C(0x4)
	uint8_t unused_5;
	uint8_t unused_6;
	uint8_t unused_7;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
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
/* Input	(24 bytes) */
struct hwrm_queue_qportcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both
	 * TX and RX paths of the chip.
	 */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH	UINT32_C(0x1)
	/* tx path */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX	UINT32_C(0x0)
	/* rx path */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX	UINT32_C(0x1)
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_LAST \
		QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX
	uint16_t port_id;
	/*
	 * Port ID of port for which the queue configuration is being
	 * queried. This field is only required when sent by IPC.
	 */
	uint16_t unused_0;
} __attribute__((packed));

/* Output	(32 bytes) */
struct hwrm_queue_qportcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint8_t max_configurable_queues;
	/*
	 * The maximum number of queues that can be configured on this
	 * port. Valid values range from 1 through 8.
	 */
	uint8_t max_configurable_lossless_queues;
	/*
	 * The maximum number of lossless queues that can be configured
	 * on this port. Valid values range from 0 through 8.
	 */
	uint8_t queue_cfg_allowed;
	/*
	 * Bitmask indicating which queues can be configured by the
	 * hwrm_queue_cfg command. Each bit represents a specific queue
	 * where bit 0 represents queue 0 and bit 7 represents queue 7.
	 * # A value of 0 indicates that the queue is not configurable
	 * by the hwrm_queue_cfg command. # A value of 1 indicates that
	 * the queue is configurable. # A hwrm_queue_cfg command shall
	 * return error when trying to configure a queue not
	 * configurable.
	 */
	uint8_t queue_cfg_info;
	/* Information about queue configuration. */
	/*
	 * If this flag is set to '1', then the queues are configured
	 * asymmetrically on TX and RX sides. If this flag is set to
	 * '0', then the queues are configured symmetrically on TX and
	 * RX sides. For symmetric configuration, the queue
	 * configuration including queue ids and service profiles on the
	 * TX side is the same as the corresponding queue configuration
	 * on the RX side.
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_CFG_INFO_ASYM_CFG UINT32_C(0x1)
	uint8_t queue_pfcenable_cfg_allowed;
	/*
	 * Bitmask indicating which queues can be configured by the
	 * hwrm_queue_pfcenable_cfg command. Each bit represents a
	 * specific priority where bit 0 represents priority 0 and bit 7
	 * represents priority 7. # A value of 0 indicates that the
	 * priority is not configurable by the hwrm_queue_pfcenable_cfg
	 * command. # A value of 1 indicates that the priority is
	 * configurable. # A hwrm_queue_pfcenable_cfg command shall
	 * return error when trying to configure a priority that is not
	 * configurable.
	 */
	uint8_t queue_pri2cos_cfg_allowed;
	/*
	 * Bitmask indicating which queues can be configured by the
	 * hwrm_queue_pri2cos_cfg command. Each bit represents a
	 * specific queue where bit 0 represents queue 0 and bit 7
	 * represents queue 7. # A value of 0 indicates that the queue
	 * is not configurable by the hwrm_queue_pri2cos_cfg command. #
	 * A value of 1 indicates that the queue is configurable. # A
	 * hwrm_queue_pri2cos_cfg command shall return error when trying
	 * to configure a queue that is not configurable.
	 */
	uint8_t queue_cos2bw_cfg_allowed;
	/*
	 * Bitmask indicating which queues can be configured by the
	 * hwrm_queue_pri2cos_cfg command. Each bit represents a
	 * specific queue where bit 0 represents queue 0 and bit 7
	 * represents queue 7. # A value of 0 indicates that the queue
	 * is not configurable by the hwrm_queue_pri2cos_cfg command. #
	 * A value of 1 indicates that the queue is configurable. # A
	 * hwrm_queue_pri2cos_cfg command shall return error when trying
	 * to configure a queue not configurable.
	 */
	uint8_t queue_id0;
	/*
	 * ID of CoS Queue 0. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id0_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id1;
	/*
	 * ID of CoS Queue 1. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id1_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id2;
	/*
	 * ID of CoS Queue 2. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id2_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id3;
	/*
	 * ID of CoS Queue 3. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id3_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id4;
	/*
	 * ID of CoS Queue 4. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id4_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id5;
	/*
	 * ID of CoS Queue 5. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id5_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id6;
	/*
	 * ID of CoS Queue 6. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id6_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t queue_id7;
	/*
	 * ID of CoS Queue 7. FF - Invalid id # This ID can be used on
	 * any subsequent call to an hwrm command that takes a queue id.
	 * # IDs must always be queried by this command before any use
	 * by the driver or software. # Any driver or software should
	 * not make any assumptions about queue IDs. # A value of 0xff
	 * indicates that the queue is not available. # Available queues
	 * may not be in sequential order.
	 */
	uint8_t queue_id7_service_profile;
	/* This value is applicable to CoS queues only. */
	/* Lossy	(best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_LOSSY \
		UINT32_C(0x0)
	/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_LOSSLESS \
		UINT32_C(0x1)
	/*
	 * Set to 0xFF...	(All Fs) if there is no
	 * service profile specified
	 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_UNKNOWN \
		UINT32_C(0xff)
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/*********************
 * hwrm_port_mac_cfg *
 *********************/


/* hwrm_port_mac_cfg_input (size:320b/40B) */
struct hwrm_port_mac_cfg_input {
	uint16_t	req_type;
	uint16_t	cmpl_ring;
	uint16_t	seq_id;
	uint16_t	target_id;
	uint64_t	resp_addr;
	uint32_t	flags;
	#define PORT_MAC_CFG_REQ_FLAGS_MATCH_LINK                    0x1UL
	#define PORT_MAC_CFG_REQ_FLAGS_VLAN_PRI2COS_ENABLE           0x2UL
	#define PORT_MAC_CFG_REQ_FLAGS_TUNNEL_PRI2COS_ENABLE         0x4UL
	#define PORT_MAC_CFG_REQ_FLAGS_IP_DSCP2COS_ENABLE            0x8UL
	#define PORT_MAC_CFG_REQ_FLAGS_PTP_RX_TS_CAPTURE_ENABLE      0x10UL
	#define PORT_MAC_CFG_REQ_FLAGS_PTP_RX_TS_CAPTURE_DISABLE     0x20UL
	#define PORT_MAC_CFG_REQ_FLAGS_PTP_TX_TS_CAPTURE_ENABLE      0x40UL
	#define PORT_MAC_CFG_REQ_FLAGS_PTP_TX_TS_CAPTURE_DISABLE     0x80UL
	#define PORT_MAC_CFG_REQ_FLAGS_OOB_WOL_ENABLE                0x100UL
	#define PORT_MAC_CFG_REQ_FLAGS_OOB_WOL_DISABLE               0x200UL
	#define PORT_MAC_CFG_REQ_FLAGS_VLAN_PRI2COS_DISABLE          0x400UL
	#define PORT_MAC_CFG_REQ_FLAGS_TUNNEL_PRI2COS_DISABLE        0x800UL
	#define PORT_MAC_CFG_REQ_FLAGS_IP_DSCP2COS_DISABLE           0x1000UL
	uint32_t	enables;
	#define PORT_MAC_CFG_REQ_ENABLES_IPG                            0x1UL
	#define PORT_MAC_CFG_REQ_ENABLES_LPBK                           0x2UL
	#define PORT_MAC_CFG_REQ_ENABLES_VLAN_PRI2COS_MAP_PRI           0x4UL
	#define PORT_MAC_CFG_REQ_ENABLES_TUNNEL_PRI2COS_MAP_PRI         0x10UL
	#define PORT_MAC_CFG_REQ_ENABLES_DSCP2COS_MAP_PRI               0x20UL
	#define PORT_MAC_CFG_REQ_ENABLES_RX_TS_CAPTURE_PTP_MSG_TYPE     0x40UL
	#define PORT_MAC_CFG_REQ_ENABLES_TX_TS_CAPTURE_PTP_MSG_TYPE     0x80UL
	#define PORT_MAC_CFG_REQ_ENABLES_COS_FIELD_CFG                  0x100UL
	uint16_t	port_id;
	uint8_t		ipg;
	uint8_t		lpbk;
	#define PORT_MAC_CFG_REQ_LPBK_NONE   0x0UL
	#define PORT_MAC_CFG_REQ_LPBK_LOCAL  0x1UL
	#define PORT_MAC_CFG_REQ_LPBK_REMOTE 0x2UL
	#define PORT_MAC_CFG_REQ_LPBK_LAST  PORT_MAC_CFG_REQ_LPBK_REMOTE
	uint8_t		vlan_pri2cos_map_pri;
	uint8_t		reserved1;
	uint8_t		tunnel_pri2cos_map_pri;
	uint8_t		dscp2pri_map_pri;
	uint16_t	rx_ts_capture_ptp_msg_type;
	uint16_t	tx_ts_capture_ptp_msg_type;
	uint8_t		cos_field_cfg;
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_RSVD1                     0x1UL
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_MASK         0x6UL
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_SFT          1
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_INNERMOST \
		(0x0UL << 1)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_OUTER \
		(0x1UL << 1)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_OUTERMOST \
		(0x2UL << 1)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_UNSPECIFIED \
		(0x3UL << 1)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_LAST \
		PORT_MAC_CFG_REQ_COS_FIELD_CFG_VLAN_PRI_SEL_UNSPECIFIED
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_MASK       0x18UL
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_SFT        3
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_INNERMOST \
		(0x0UL << 3)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_OUTER \
		(0x1UL << 3)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_OUTERMOST \
		(0x2UL << 3)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_UNSPECIFIED \
		(0x3UL << 3)
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_LAST \
		PORT_MAC_CFG_REQ_COS_FIELD_CFG_T_VLAN_PRI_SEL_UNSPECIFIED
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_DEFAULT_COS_MASK          0xe0UL
	#define PORT_MAC_CFG_REQ_COS_FIELD_CFG_DEFAULT_COS_SFT           5
	uint8_t		unused_0[3];
};


/* hwrm_port_mac_cfg_output (size:128b/16B) */
struct hwrm_port_mac_cfg_output {
	uint16_t	error_code;
	uint16_t	req_type;
	uint16_t	seq_id;
	uint16_t	resp_len;
	uint16_t	mru;
	uint16_t	mtu;
	uint8_t		ipg;
	uint8_t		lpbk;
	#define PORT_MAC_CFG_RESP_LPBK_NONE   0x0UL
	#define PORT_MAC_CFG_RESP_LPBK_LOCAL  0x1UL
	#define PORT_MAC_CFG_RESP_LPBK_REMOTE 0x2UL
	#define PORT_MAC_CFG_RESP_LPBK_LAST  PORT_MAC_CFG_RESP_LPBK_REMOTE
	uint8_t		unused_0;
	uint8_t		valid;
};


/**********************
 * hwrm_port_mac_qcfg *
 **********************/


/* hwrm_port_mac_qcfg_input (size:192b/24B) */
struct hwrm_port_mac_qcfg_input {
	uint16_t	req_type;
	uint16_t	cmpl_ring;
	uint16_t	seq_id;
	uint16_t	target_id;
	uint64_t	resp_addr;
	uint16_t	port_id;
	uint8_t		unused_0[6];
};


/* hwrm_port_mac_qcfg_output (size:192b/24B) */
struct hwrm_port_mac_qcfg_output {
	uint16_t	error_code;
	uint16_t	req_type;
	uint16_t	seq_id;
	uint16_t	resp_len;
	uint16_t	mru;
	uint16_t	mtu;
	uint8_t		ipg;
	uint8_t		lpbk;
	#define PORT_MAC_QCFG_RESP_LPBK_NONE   0x0UL
	#define PORT_MAC_QCFG_RESP_LPBK_LOCAL  0x1UL
	#define PORT_MAC_QCFG_RESP_LPBK_REMOTE 0x2UL
	#define PORT_MAC_QCFG_RESP_LPBK_LAST  PORT_MAC_QCFG_RESP_LPBK_REMOTE
	uint8_t		vlan_pri2cos_map_pri;
	uint8_t		flags;
	#define PORT_MAC_QCFG_RESP_FLAGS_VLAN_PRI2COS_ENABLE          0x1UL
	#define PORT_MAC_QCFG_RESP_FLAGS_TUNNEL_PRI2COS_ENABLE        0x2UL
	#define PORT_MAC_QCFG_RESP_FLAGS_IP_DSCP2COS_ENABLE           0x4UL
	#define PORT_MAC_QCFG_RESP_FLAGS_OOB_WOL_ENABLE               0x8UL
	#define PORT_MAC_QCFG_RESP_FLAGS_PTP_RX_TS_CAPTURE_ENABLE     0x10UL
	#define PORT_MAC_QCFG_RESP_FLAGS_PTP_TX_TS_CAPTURE_ENABLE     0x20UL
	uint8_t		tunnel_pri2cos_map_pri;
	uint8_t		dscp2pri_map_pri;
	uint16_t	rx_ts_capture_ptp_msg_type;
	uint16_t	tx_ts_capture_ptp_msg_type;
	uint8_t		cos_field_cfg;
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_RSVD                      0x1UL
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_MASK         0x6UL
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_SFT          1
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_INNERMOST \
		(0x0UL << 1)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_OUTER \
		(0x1UL << 1)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_OUTERMOST \
		(0x2UL << 1)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_UNSPECIFIED \
		(0x3UL << 1)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_LAST \
		PORT_MAC_QCFG_RESP_COS_FIELD_CFG_VLAN_PRI_SEL_UNSPECIFIED
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_MASK	0x18UL
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_SFT        3
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_INNERMOST \
		(0x0UL << 3)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_OUTER \
		(0x1UL << 3)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_OUTERMOST \
		(0x2UL << 3)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_UNSPECIFIED \
		(0x3UL << 3)
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_LAST \
		PORT_MAC_QCFG_RESP_COS_FIELD_CFG_T_VLAN_PRI_SEL_UNSPECIFIED
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_DEFAULT_COS_MASK	0xe0UL
	#define PORT_MAC_QCFG_RESP_COS_FIELD_CFG_DEFAULT_COS_SFT           5
	uint8_t		valid;
};


/**************************
 * hwrm_port_mac_ptp_qcfg *
 **************************/


/* hwrm_port_mac_ptp_qcfg_input (size:192b/24B) */
struct hwrm_port_mac_ptp_qcfg_input {
	uint16_t	req_type;
	uint16_t	cmpl_ring;
	uint16_t	seq_id;
	uint16_t	target_id;
	uint64_t	resp_addr;
	uint16_t	port_id;
	uint8_t		unused_0[6];
};


/* hwrm_port_mac_ptp_qcfg_output (size:640b/80B) */
struct hwrm_port_mac_ptp_qcfg_output {
	uint16_t	error_code;
	uint16_t	req_type;
	uint16_t	seq_id;
	uint16_t	resp_len;
	uint8_t		flags;
	#define PORT_MAC_PTP_QCFG_RESP_FLAGS_DIRECT_ACCESS     0x1UL
	#define PORT_MAC_PTP_QCFG_RESP_FLAGS_HWRM_ACCESS       0x2UL
	uint8_t		unused_0[3];
	uint32_t	rx_ts_reg_off_lower;
	uint32_t	rx_ts_reg_off_upper;
	uint32_t	rx_ts_reg_off_seq_id;
	uint32_t	rx_ts_reg_off_src_id_0;
	uint32_t	rx_ts_reg_off_src_id_1;
	uint32_t	rx_ts_reg_off_src_id_2;
	uint32_t	rx_ts_reg_off_domain_id;
	uint32_t	rx_ts_reg_off_fifo;
	uint32_t	rx_ts_reg_off_fifo_adv;
	uint32_t	rx_ts_reg_off_granularity;
	uint32_t	tx_ts_reg_off_lower;
	uint32_t	tx_ts_reg_off_upper;
	uint32_t	tx_ts_reg_off_seq_id;
	uint32_t	tx_ts_reg_off_fifo;
	uint32_t	tx_ts_reg_off_granularity;
	uint8_t		unused_1[7];
	uint8_t		valid;
};


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
/* Input	(24 bytes) */
struct hwrm_vnic_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', this VNIC is requested to be the
	 * default VNIC for this function.
	 */
	#define HWRM_VNIC_ALLOC_INPUT_FLAGS_DEFAULT	UINT32_C(0x1)
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t vnic_id;
	/* Logical vnic ID */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_free */
/*
 * Description: Free a VNIC resource. Idle any resources associated with the
 * VNIC as well as the VNIC. Reset and release all resources associated with the
 * VNIC.
 */
/* Input	(24 bytes) */
struct hwrm_vnic_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t vnic_id;
	/* Logical vnic ID */
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_cfg */
/* Description: Configure the RX VNIC structure. */
/* Input	(40 bytes) */
struct hwrm_vnic_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', the VNIC is requested to be the default
	 * VNIC for the function.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_DEFAULT	UINT32_C(0x1)
	/*
	 * When this bit is '1', the VNIC is being configured to strip
	 * VLAN in the RX path. If set to '0', then VLAN stripping is
	 * disabled on this VNIC.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE	UINT32_C(0x2)
	/*
	 * When this bit is '1', the VNIC is being configured to buffer
	 * receive packets in the hardware until the host posts new
	 * receive buffers. If set to '0', then bd_stall is being
	 * configured to be disabled on this VNIC.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_BD_STALL_MODE	UINT32_C(0x4)
	/*
	 * When this bit is '1', the VNIC is being configured to receive
	 * both RoCE and non-RoCE traffic. If set to '0', then this VNIC
	 * is not configured to be operating in dual VNIC mode.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_ROCE_DUAL_VNIC_MODE	UINT32_C(0x8)
	/*
	 * When this flag is set to '1', the VNIC is requested to be
	 * configured to receive only RoCE traffic. If this flag is set
	 * to '0', then this flag shall be ignored by the HWRM. If
	 * roce_dual_vnic_mode flag is set to '1', then the HWRM client
	 * shall not set this flag to '1'.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_ROCE_ONLY_VNIC_MODE UINT32_C(0x10)
	/*
	 * When a VNIC uses one destination ring group for certain
	 * application	(e.g. Receive Flow Steering) where exact match is
	 * used to direct packets to a VNIC with one destination ring
	 * group only, there is no need to configure RSS indirection
	 * table for that VNIC as only one destination ring group is
	 * used. This flag is used to enable a mode where RSS is enabled
	 * in the VNIC using a RSS context for computing RSS hash but
	 * the RSS indirection table is not configured using
	 * hwrm_vnic_rss_cfg. If this mode is enabled, then the driver
	 * should not program RSS indirection table for the RSS context
	 * that is used for computing RSS hash only.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_RSS_DFLT_CR_MODE	UINT32_C(0x20)
	/*
	 * When this bit is '1', the VNIC is being configured to receive
	 * both RoCE and non-RoCE traffic, but forward only the RoCE
	 * traffic further. Also, RoCE traffic can be mirrored to L2
	 * driver.
	 */
	#define HWRM_VNIC_CFG_INPUT_FLAGS_ROCE_MIRRORING_CAPABLE_VNIC_MODE \
	UINT32_C(0x40)
	uint32_t enables;
	/*
	 * This bit must be '1' for the dflt_ring_grp field to be
	 * configured.
	 */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP	UINT32_C(0x1)
	/* This bit must be '1' for the rss_rule field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE	UINT32_C(0x2)
	/* This bit must be '1' for the cos_rule field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_COS_RULE	UINT32_C(0x4)
	/* This bit must be '1' for the lb_rule field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_LB_RULE	UINT32_C(0x8)
	/* This bit must be '1' for the mru field to be configured. */
	#define HWRM_VNIC_CFG_INPUT_ENABLES_MRU	UINT32_C(0x10)
	uint16_t vnic_id;
	/* Logical vnic ID */
	uint16_t dflt_ring_grp;
	/*
	 * Default Completion ring for the VNIC. This ring will be
	 * chosen if packet does not match any RSS rules and if there is
	 * no COS rule.
	 */
	uint16_t rss_rule;
	/*
	 * RSS ID for RSS rule/table structure. 0xFF...	(All Fs) if
	 * there is no RSS rule.
	 */
	uint16_t cos_rule;
	/*
	 * RSS ID for COS rule/table structure. 0xFF...	(All Fs) if
	 * there is no COS rule.
	 */
	uint16_t lb_rule;
	/*
	 * RSS ID for load balancing rule/table structure. 0xFF...	(All
	 * Fs) if there is no LB rule.
	 */
	uint16_t mru;
	/*
	 * The maximum receive unit of the vnic. Each vnic is associated
	 * with a function. The vnic mru value overwrites the mru
	 * setting of the associated function. The HWRM shall make sure
	 * that vnic mru does not exceed the mru of the port the
	 * function is associated with.
	 */
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_qcfg */
/*
 * Description: Query the RX VNIC structure. This function can be used by a PF
 * driver to query its own VNIC resource or VNIC resource of its child VF. This
 * function can also be used by a VF driver to query its own VNIC resource.
 */
/* Input	(32 bytes) */
struct hwrm_vnic_qcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the vf_id_valid field to be configured. */
	#define HWRM_VNIC_QCFG_INPUT_ENABLES_VF_ID_VALID	UINT32_C(0x1)
	uint32_t vnic_id;
	/* Logical vnic ID */
	uint16_t vf_id;
	/* ID of Virtual Function whose VNIC resource is being queried. */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(32 bytes) */
struct hwrm_vnic_qcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t dflt_ring_grp;
	/* Default Completion ring for the VNIC. */
	uint16_t rss_rule;
	/*
	 * RSS ID for RSS rule/table structure. 0xFF...	(All Fs) if
	 * there is no RSS rule.
	 */
	uint16_t cos_rule;
	/*
	 * RSS ID for COS rule/table structure. 0xFF...	(All Fs) if
	 * there is no COS rule.
	 */
	uint16_t lb_rule;
	/*
	 * RSS ID for load balancing rule/table structure. 0xFF...	(All
	 * Fs) if there is no LB rule.
	 */
	uint16_t mru;
	/* The maximum receive unit of the vnic. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t flags;
	/*
	 * When this bit is '1', the VNIC is the default VNIC for the
	 * function.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_DEFAULT	UINT32_C(0x1)
	/*
	 * When this bit is '1', the VNIC is configured to strip VLAN in
	 * the RX path. If set to '0', then VLAN stripping is disabled
	 * on this VNIC.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_VLAN_STRIP_MODE	UINT32_C(0x2)
	/*
	 * When this bit is '1', the VNIC is configured to buffer
	 * receive packets in the hardware until the host posts new
	 * receive buffers. If set to '0', then bd_stall is disabled on
	 * this VNIC.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_BD_STALL_MODE	UINT32_C(0x4)
	/*
	 * When this bit is '1', the VNIC is configured to receive both
	 * RoCE and non-RoCE traffic. If set to '0', then this VNIC is
	 * not configured to operate in dual VNIC mode.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_DUAL_VNIC_MODE	UINT32_C(0x8)
	/*
	 * When this flag is set to '1', the VNIC is configured to
	 * receive only RoCE traffic. When this flag is set to '0', the
	 * VNIC is not configured to receive only RoCE traffic. If
	 * roce_dual_vnic_mode flag and this flag both are set to '1',
	 * then it is an invalid configuration of the VNIC. The HWRM
	 * should not allow that type of mis-configuration by HWRM
	 * clients.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_ONLY_VNIC_MODE	UINT32_C(0x10)
	/*
	 * When a VNIC uses one destination ring group for certain
	 * application	(e.g. Receive Flow Steering) where exact match is
	 * used to direct packets to a VNIC with one destination ring
	 * group only, there is no need to configure RSS indirection
	 * table for that VNIC as only one destination ring group is
	 * used. When this bit is set to '1', then the VNIC is enabled
	 * in a mode where RSS is enabled in the VNIC using a RSS
	 * context for computing RSS hash but the RSS indirection table
	 * is not configured.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_RSS_DFLT_CR_MODE	UINT32_C(0x20)
	/*
	 * When this bit is '1', the VNIC is configured to receive both
	 * RoCE and non-RoCE traffic, but forward only RoCE traffic
	 * further. Also RoCE traffic can be mirrored to L2 driver.
	 */
	#define HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_MIRRORING_CAPABLE_VNIC_MODE \
	UINT32_C(0x40)
	uint32_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;
	uint8_t unused_5;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));


/* hwrm_vnic_tpa_cfg */
/* Description: This function is used to enable/configure TPA on the VNIC. */
/* Input	(40 bytes) */
struct hwrm_vnic_tpa_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', the VNIC shall be configured to perform
	 * transparent packet aggregation	(TPA) of non-tunneled TCP
	 * packets.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_TPA	UINT32_C(0x1)
	/*
	 * When this bit is '1', the VNIC shall be configured to perform
	 * transparent packet aggregation	(TPA) of tunneled TCP packets.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_ENCAP_TPA	UINT32_C(0x2)
	/*
	 * When this bit is '1', the VNIC shall be configured to perform
	 * transparent packet aggregation	(TPA) according to Windows
	 * Receive Segment Coalescing	(RSC) rules.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_RSC_WND_UPDATE	UINT32_C(0x4)
	/*
	 * When this bit is '1', the VNIC shall be configured to perform
	 * transparent packet aggregation	(TPA) according to Linux
	 * Generic Receive Offload	(GRO) rules.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_GRO	UINT32_C(0x8)
	/*
	 * When this bit is '1', the VNIC shall be configured to perform
	 * transparent packet aggregation	(TPA) for TCP packets with IP
	 * ECN set to non-zero.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_ECN	UINT32_C(0x10)
	/*
	 * When this bit is '1', the VNIC shall be configured to perform
	 * transparent packet aggregation	(TPA) for GRE tunneled TCP
	 * packets only if all packets have the same GRE sequence.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_SAME_GRE_SEQ	\
		UINT32_C(0x20)
	/*
	 * When this bit is '1' and the GRO mode is enabled, the VNIC
	 * shall be configured to perform transparent packet aggregation
	 *	(TPA) for TCP/IPv4 packets with consecutively increasing
	 * IPIDs. In other words, the last packet that is being
	 * aggregated to an already existing aggregation context shall
	 * have IPID 1 more than the IPID of the last packet that was
	 * aggregated in that aggregation context.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_GRO_IPID_CHECK	UINT32_C(0x40)
	/*
	 * When this bit is '1' and the GRO mode is enabled, the VNIC
	 * shall be configured to perform transparent packet aggregation
	 *	(TPA) for TCP packets with the same TTL	(IPv4) or Hop limit
	 *	(IPv6) value.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_FLAGS_GRO_TTL_CHECK	UINT32_C(0x80)
	uint32_t enables;
	/* This bit must be '1' for the max_agg_segs field to be configured. */
	#define HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGG_SEGS	UINT32_C(0x1)
	/* This bit must be '1' for the max_aggs field to be configured. */
	#define HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGGS	UINT32_C(0x2)
	/*
	 * This bit must be '1' for the max_agg_timer field to be
	 * configured.
	 */
	#define HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGG_TIMER	UINT32_C(0x4)
	/* This bit must be '1' for the min_agg_len field to be configured. */
	#define HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MIN_AGG_LEN	UINT32_C(0x8)
	uint16_t vnic_id;
	/* Logical vnic ID */
	uint16_t max_agg_segs;
	/*
	 * This is the maximum number of TCP segments that can be
	 * aggregated	(unit is Log2). Max value is 31.
	 */
	/* 1 segment */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_1	UINT32_C(0x0)
	/* 2 segments */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_2	UINT32_C(0x1)
	/* 4 segments */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_4	UINT32_C(0x2)
	/* 8 segments */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_8	UINT32_C(0x3)
	/* Any segment size larger than this is not valid */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGG_SEGS_MAX	UINT32_C(0x1f)
	uint16_t max_aggs;
	/*
	 * This is the maximum number of aggregations this VNIC is
	 * allowed	(unit is Log2). Max value is 7
	 */
	/* 1 aggregation */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_1	UINT32_C(0x0)
	/* 2 aggregations */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_2	UINT32_C(0x1)
	/* 4 aggregations */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_4	UINT32_C(0x2)
	/* 8 aggregations */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_8	UINT32_C(0x3)
	/* 16 aggregations */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_16	UINT32_C(0x4)
	/* Any aggregation size larger than this is not valid */
	#define HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_MAX	UINT32_C(0x7)
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t max_agg_timer;
	/*
	 * This is the maximum amount of time allowed for an aggregation
	 * context to complete after it was initiated.
	 */
	uint32_t min_agg_len;
	/*
	 * This is the minimum amount of payload length required to
	 * start an aggregation context.
	 */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_tpa_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_rss_cfg */
/* Description: This function is used to enable RSS configuration. */
/* Input	(48 bytes) */
struct hwrm_vnic_rss_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t hash_type;
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source and destination IPv4 addresses of IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4	UINT32_C(0x1)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv4 addresses and source/destination
	 * ports of TCP/IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4	UINT32_C(0x2)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv4 addresses and source/destination
	 * ports of UDP/IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4	UINT32_C(0x4)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source and destination IPv4 addresses of IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6	UINT32_C(0x8)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv6 addresses and source/destination
	 * ports of TCP/IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6	UINT32_C(0x10)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv6 addresses and source/destination
	 * ports of UDP/IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6	UINT32_C(0x20)
	uint32_t unused_0;
	uint64_t ring_grp_tbl_addr;
	/* This is the address for rss ring group table */
	uint64_t hash_key_tbl_addr;
	/* This is the address for rss hash key table */
	uint16_t rss_ctx_idx;
	/* Index to the rss indirection table. */
	uint16_t unused_1[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_rss_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_plcmodes_cfg */
/*
 * Description: This function can be used to set placement mode configuration of
 * the VNIC.
 */
/* Input (40 bytes) */
struct hwrm_vnic_plcmodes_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', the VNIC shall be configured to use regular
	 * placement algorithm. By default, the regular placement algorithm
	 * shall be enabled on the VNIC.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_REGULAR_PLACEMENT \
		UINT32_C(0x1)
	/*
	 * When this bit is '1', the VNIC shall be configured use the jumbo
	 * placement algorithm.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_JUMBO_PLACEMENT \
		UINT32_C(0x2)
	/*
	 * When this bit is '1', the VNIC shall be configured to enable Header-
	 * Data split for IPv4 packets according to the following rules: # If
	 * the packet is identified as TCP/IPv4, then the packet is split at the
	 * beginning of the TCP payload. # If the packet is identified as
	 * UDP/IPv4, then the packet is split at the beginning of UDP payload. #
	 * If the packet is identified as non-TCP and non-UDP IPv4 packet, then
	 * the packet is split at the beginning of the upper layer protocol
	 * header carried in the IPv4 packet.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_HDS_IPV4        UINT32_C(0x4)
	/*
	 * When this bit is '1', the VNIC shall be configured to enable Header-
	 * Data split for IPv6 packets according to the following rules: # If
	 * the packet is identified as TCP/IPv6, then the packet is split at the
	 * beginning of the TCP payload. # If the packet is identified as
	 * UDP/IPv6, then the packet is split at the beginning of UDP payload. #
	 * If the packet is identified as non-TCP and non-UDP IPv6 packet, then
	 * the packet is split at the beginning of the upper layer protocol
	 * header carried in the IPv6 packet.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_HDS_IPV6        UINT32_C(0x8)
	/*
	 * When this bit is '1', the VNIC shall be configured to enable Header-
	 * Data split for FCoE packets at the beginning of FC payload.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_HDS_FCOE       UINT32_C(0x10)
	/*
	 * When this bit is '1', the VNIC shall be configured to enable Header-
	 * Data split for RoCE packets at the beginning of RoCE payload (after
	 * BTH/GRH headers).
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_HDS_ROCE       UINT32_C(0x20)
	uint32_t enables;
	/*
	 * This bit must be '1' for the jumbo_thresh_valid field to be
	 * configured.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_JUMBO_THRESH_VALID \
		UINT32_C(0x1)
	/*
	 * This bit must be '1' for the hds_offset_valid field to be configured.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_HDS_OFFSET_VALID \
		UINT32_C(0x2)
	/*
	 * This bit must be '1' for the hds_threshold_valid field to be
	 * configured.
	 */
	#define HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_HDS_THRESHOLD_VALID \
		UINT32_C(0x4)
	uint32_t vnic_id;
	/* Logical vnic ID */
	uint16_t jumbo_thresh;
	/*
	 * When jumbo placement algorithm is enabled, this value is used to
	 * determine the threshold for jumbo placement. Packets with length
	 * larger than this value will be placed according to the jumbo
	 * placement algorithm.
	 */
	uint16_t hds_offset;
	/*
	 * This value is used to determine the offset into packet buffer where
	 * the split data (payload) will be placed according to one of of HDS
	 * placement algorithm. The lengths of packet buffers provided for split
	 * data shall be larger than this value.
	 */
	uint16_t hds_threshold;
	/*
	 * When one of the HDS placement algorithm is enabled, this value is
	 * used to determine the threshold for HDS placement. Packets with
	 * length larger than this value will be placed according to the HDS
	 * placement algorithm. This value shall be in multiple of 4 bytes.
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_plcmodes_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_plcmodes_qcfg */
/*
 * Description: This function can be used to query placement mode configuration
 * of the VNIC.
 */
/* Input (24 bytes) */
struct hwrm_vnic_plcmodes_qcfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint32_t vnic_id;
	/* Logical vnic ID */
	uint32_t unused_0;
} __attribute__((packed));

/* Output (24 bytes) */
struct hwrm_vnic_plcmodes_qcfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint32_t flags;
	/*
	 * When this bit is '1', the VNIC is configured to use regular placement
	 * algorithm.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_REGULAR_PLACEMENT \
		UINT32_C(0x1)
	/*
	 * When this bit is '1', the VNIC is configured to use the jumbo
	 * placement algorithm.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_JUMBO_PLACEMENT \
		UINT32_C(0x2)
	/*
	 * When this bit is '1', the VNIC is configured to enable Header-Data
	 * split for IPv4 packets.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_HDS_IPV4      UINT32_C(0x4)
	/*
	 * When this bit is '1', the VNIC is configured to enable Header-Data
	 * split for IPv6 packets.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_HDS_IPV6      UINT32_C(0x8)
	/*
	 * When this bit is '1', the VNIC is configured to enable Header-Data
	 * split for FCoE packets.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_HDS_FCOE     UINT32_C(0x10)
	/*
	 * When this bit is '1', the VNIC is configured to enable Header-Data
	 * split for RoCE packets.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_HDS_ROCE     UINT32_C(0x20)
	/*
	 * When this bit is '1', the VNIC is configured to be the default VNIC
	 * of the requesting function.
	 */
	#define HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_DFLT_VNIC    UINT32_C(0x40)
	uint16_t jumbo_thresh;
	/*
	 * When jumbo placement algorithm is enabled, this value is used to
	 * determine the threshold for jumbo placement. Packets with length
	 * larger than this value will be placed according to the jumbo
	 * placement algorithm.
	 */
	uint16_t hds_offset;
	/*
	 * This value is used to determine the offset into packet buffer where
	 * the split data (payload) will be placed according to one of of HDS
	 * placement algorithm. The lengths of packet buffers provided for split
	 * data shall be larger than this value.
	 */
	uint16_t hds_threshold;
	/*
	 * When one of the HDS placement algorithm is enabled, this value is
	 * used to determine the threshold for HDS placement. Packets with
	 * length larger than this value will be placed according to the HDS
	 * placement algorithm. This value shall be in multiple of 4 bytes.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_rss_cos_lb_ctx_alloc */
/* Description: This function is used to allocate COS/Load Balance context. */
/* Input	(16 bytes) */
struct hwrm_vnic_rss_cos_lb_ctx_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_rss_cos_lb_ctx_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t rss_cos_lb_ctx_id;
	/* rss_cos_lb_ctx_id is 16 b */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_vnic_rss_cos_lb_ctx_free */
/* Description: This function can be used to free COS/Load Balance context. */
/* Input	(24 bytes) */
struct hwrm_vnic_rss_cos_lb_ctx_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t rss_cos_lb_ctx_id;
	/* rss_cos_lb_ctx_id is 16 b */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_vnic_rss_cos_lb_ctx_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_ring_alloc */
/*
 * Description: This command allocates and does basic preparation for a ring.
 */
/* Input	(80 bytes) */
struct hwrm_ring_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the Reserved1 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED1	UINT32_C(0x1)
	/* This bit must be '1' for the ring_arb_cfg field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RING_ARB_CFG	UINT32_C(0x2)
	/* This bit must be '1' for the Reserved3 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED3	UINT32_C(0x4)
	/*
	 * This bit must be '1' for the stat_ctx_id_valid field to be
	 * configured.
	 */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID	UINT32_C(0x8)
	/* This bit must be '1' for the Reserved4 field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_RESERVED4	UINT32_C(0x10)
	/* This bit must be '1' for the max_bw_valid field to be configured. */
	#define HWRM_RING_ALLOC_INPUT_ENABLES_MAX_BW_VALID	UINT32_C(0x20)
	uint8_t ring_type;
	/* Ring Type. */
	/* L2 Completion Ring	(CR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL	UINT32_C(0x0)
	/* TX Ring	(TR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_TX	UINT32_C(0x1)
	/* RX Ring	(RR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_RX	UINT32_C(0x2)
	/* RoCE Notification Completion Ring	(ROCE_CR) */
	#define HWRM_RING_ALLOC_INPUT_RING_TYPE_ROCE_CMPL	UINT32_C(0x3)
	uint8_t unused_0;
	uint16_t unused_1;
	uint64_t page_tbl_addr;
	/* This value is a pointer to the page table for the Ring. */
	uint32_t fbo;
	/* First Byte Offset of the first entry in the first page. */
	uint8_t page_size;
	/*
	 * Actual page size in 2^page_size. The supported range is
	 * increments in powers of 2 from 16 bytes to 1GB. - 4 = 16 B
	 * Page size is 16 B. - 12 = 4 KB Page size is 4 KB. - 13 = 8 KB
	 * Page size is 8 KB. - 16 = 64 KB Page size is 64 KB. - 21 = 2
	 * MB Page size is 2 MB. - 22 = 4 MB Page size is 4 MB. - 30 = 1
	 * GB Page size is 1 GB.
	 */
	uint8_t page_tbl_depth;
	/*
	 * This value indicates the depth of page table. For this
	 * version of the specification, value other than 0 or 1 shall
	 * be considered as an invalid value. When the page_tbl_depth =
	 * 0, then it is treated as a special case with the following.
	 * 1. FBO and page size fields are not valid. 2. page_tbl_addr
	 * is the physical address of the first element of the ring.
	 */
	uint8_t unused_2;
	uint8_t unused_3;
	uint32_t length;
	/*
	 * Number of 16B units in the ring. Minimum size for a ring is
	 * 16 16B entries.
	 */
	uint16_t logical_id;
	/*
	 * Logical ring number for the ring to be allocated. This value
	 * determines the position in the doorbell area where the update
	 * to the ring will be made. For completion rings, this value is
	 * also the MSI-X vector number for the function the completion
	 * ring is associated with.
	 */
	uint16_t cmpl_ring_id;
	/*
	 * This field is used only when ring_type is a TX ring. This
	 * value indicates what completion ring the TX ring is
	 * associated with.
	 */
	uint16_t queue_id;
	/*
	 * This field is used only when ring_type is a TX ring. This
	 * value indicates what CoS queue the TX ring is associated
	 * with.
	 */
	uint8_t unused_4;
	uint8_t unused_5;
	uint32_t reserved1;
	/* This field is reserved for the future use. It shall be set to 0. */
	uint16_t ring_arb_cfg;
	/*
	 * This field is used only when ring_type is a TX ring. This
	 * field is used to configure arbitration related parameters for
	 * a TX ring.
	 */
	/* Arbitration policy used for the ring. */
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_MASK UINT32_C(0xf)
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_SFT	0
	/*
	 * Use strict priority for the TX ring. Priority
	 * value is specified in arb_policy_param
	 */
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_SP \
		(UINT32_C(0x1) << 0)
	/*
	 * Use weighted fair queue arbitration for the
	 * TX ring. Weight is specified in
	 * arb_policy_param
	 */
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_WFQ \
		(UINT32_C(0x2) << 0)
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_LAST \
		RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_WFQ
	/* Reserved field. */
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_RSVD_MASK	UINT32_C(0xf0)
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_RSVD_SFT	4
	/*
	 * Arbitration policy specific parameter. # For strict priority
	 * arbitration policy, this field represents a priority value.
	 * If set to 0, then the priority is not specified and the HWRM
	 * is allowed to select any priority for this TX ring. # For
	 * weighted fair queue arbitration policy, this field represents
	 * a weight value. If set to 0, then the weight is not specified
	 * and the HWRM is allowed to select any weight for this TX
	 * ring.
	 */
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_PARAM_MASK  \
		UINT32_C(0xff00)
	#define HWRM_RING_ALLOC_INPUT_RING_ARB_CFG_ARB_POLICY_PARAM_SFT	8
	uint8_t unused_6;
	uint8_t unused_7;
	uint32_t reserved3;
	/* This field is reserved for the future use. It shall be set to 0. */
	uint32_t stat_ctx_id;
	/*
	 * This field is used only when ring_type is a TX ring. This
	 * input indicates what statistics context this ring should be
	 * associated with.
	 */
	uint32_t reserved4;
	/* This field is reserved for the future use. It shall be set to 0. */
	uint32_t max_bw;
	/*
	 * This field is used only when ring_type is a TX ring to
	 * specify maximum BW allocated to the TX ring. The HWRM will
	 * translate this value into byte counter and time interval used
	 * for this ring inside the device.
	 */
	/* The bandwidth value. */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_MASK UINT32_C(0xfffffff)
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_SFT	0
	/* The granularity of the value	(bits or bytes). */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_SCALE	UINT32_C(0x10000000)
	/* Value is in bits. */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_SCALE_BITS	(UINT32_C(0x0) << 28)
	/* Value is in bytes. */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_SCALE_BYTES (UINT32_C(0x1) << 28)
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_SCALE_LAST \
		RING_ALLOC_INPUT_MAX_BW_SCALE_BYTES
	/* bw_value_unit is 3 b */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_MASK	\
		UINT32_C(0xe0000000)
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_SFT	29
	/* Value is in Mb or MB	(base 10). */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_MEGA	\
		(UINT32_C(0x0) << 29)
	/* Value is in Kb or KB	(base 10). */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_KILO	\
		(UINT32_C(0x2) << 29)
	/* Value is in bits or bytes. */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_BASE	\
		(UINT32_C(0x4) << 29)
	/* Value is in Gb or GB	(base 10). */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_GIGA	\
		(UINT32_C(0x6) << 29)
	/* Value is in 1/100th of a percentage of total bandwidth. */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_PERCENT1_100 \
		(UINT32_C(0x1) << 29)
	/* Invalid unit */
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_INVALID \
		(UINT32_C(0x7) << 29)
	#define HWRM_RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_LAST \
		RING_ALLOC_INPUT_MAX_BW_BW_VALUE_UNIT_INVALID
	uint8_t int_mode;
	/*
	 * This field is used only when ring_type is a Completion ring.
	 * This value indicates what interrupt mode should be used on
	 * this completion ring. Note: In the legacy interrupt mode, no
	 * more than 16 completion rings are allowed.
	 */
	/* Legacy INTA */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_LEGACY	UINT32_C(0x0)
	/* Reserved */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_RSVD	UINT32_C(0x1)
	/* MSI-X */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX	UINT32_C(0x2)
	/* No Interrupt - Polled mode */
	#define HWRM_RING_ALLOC_INPUT_INT_MODE_POLL	UINT32_C(0x3)
	uint8_t unused_8[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_ring_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t ring_id;
	/*
	 * Physical number of ring allocated. This value shall be unique
	 * for a ring type.
	 */
	uint16_t logical_ring_id;
	/* Logical number of ring allocated. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_ring_free */
/*
 * Description: This command is used to free a ring and associated resources.
 * With QoS and DCBx agents, it is possible the traffic classes will be moved
 * from one CoS queue to another. When this occurs, the driver shall call
 * 'hwrm_ring_free' to free the allocated rings and then call 'hwrm_ring_alloc'
 * to re-allocate each ring and assign it to a new CoS queue. hwrm_ring_free
 * shall be called on a ring only after it has been idle for 500ms or more and
 * no frames have been posted to the ring during this time. All frames queued
 * for transmission shall be completed and at least 500ms time elapsed from the
 * last completion before calling this command.
 */
/* Input	(24 bytes) */
struct hwrm_ring_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint8_t ring_type;
	/* Ring Type. */
	/* L2 Completion Ring	(CR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_L2_CMPL	UINT32_C(0x0)
	/* TX Ring	(TR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_TX	UINT32_C(0x1)
	/* RX Ring	(RR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_RX	UINT32_C(0x2)
	/* RoCE Notification Completion Ring	(ROCE_CR) */
	#define HWRM_RING_FREE_INPUT_RING_TYPE_ROCE_CMPL	UINT32_C(0x3)
	uint8_t unused_0;
	uint16_t ring_id;
	/* Physical number of ring allocated. */
	uint32_t unused_1;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_ring_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_ring_grp_alloc */
/*
 * Description: This API allocates and does basic preparation for a ring group.
 */
/* Input	(24 bytes) */
struct hwrm_ring_grp_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint16_t cr;
	/* This value identifies the CR associated with the ring group. */
	uint16_t rr;
	/* This value identifies the main RR associated with the ring group. */
	uint16_t ar;
	/*
	 * This value identifies the aggregation RR associated with the
	 * ring group. If this value is 0xFF...	(All Fs), then no
	 * Aggregation ring will be set.
	 */
	uint16_t sc;
	/*
	 * This value identifies the statistics context associated with
	 * the ring group.
	 */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_ring_grp_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t ring_group_id;
	/*
	 * This is the ring group ID value. Use this value to program
	 * the default ring group for the VNIC or as table entries in an
	 * RSS/COS context.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
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
/* Input	(24 bytes) */
struct hwrm_ring_grp_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t ring_group_id;
	/* This is the ring group ID value. */
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_ring_grp_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_l2_filter_alloc */
/*
 * Description: An L2 filter is a filter resource that is used to identify a
 * vnic or ring for a packet based on layer 2 fields. Layer 2 fields for
 * encapsulated packets include both outer L2 header and/or inner l2 header of
 * encapsulated packet. The L2 filter resource covers the following OS specific
 * L2 filters. Linux/FreeBSD (per function): # Broadcast enable/disable # List
 * of individual multicast filters # All multicast enable/disable filter #
 * Unicast filters # Promiscuous mode VMware: # Broadcast enable/disable (per
 * physical function) # All multicast enable/disable	(per function) # Unicast
 * filters per ring or vnic # Promiscuous mode per PF Windows: # Broadcast
 * enable/disable (per physical function) # List of individual multicast filters
 * (Driver needs to advertise the maximum number of filters supported) # All
 * multicast enable/disable per physical function # Unicast filters per vnic #
 * Promiscuous mode per PF Implementation notes on the use of VNIC in this
 * command: # By default, these filters belong to default vnic for the function.
 * # Once these filters are set up, only destination VNIC can be modified. # If
 * the destination VNIC is not specified in this command, then the HWRM shall
 * only create an l2 context id. HWRM Implementation notes for multicast
 * filters: # The hwrm_filter_alloc command can be used to set up multicast
 * filters (perfect match or partial match). Each individual function driver can
 * set up multicast filters independently. # The HWRM needs to keep track of
 * multicast filters set up by function drivers and maintain multicast group
 * replication records to enable a subset of functions to receive traffic for a
 * specific multicast address. # When a specific multicast filter cannot be set,
 * the HWRM shall return an error. In this error case, the driver should fall
 * back to using one general filter	(rather than specific) for all multicast
 * traffic. # When the SR-IOV is enabled, the HWRM needs to additionally track
 * source knockout per multicast group record. Examples of setting unicast
 * filters: For a unicast MAC based filter, one can use a combination of the
 * fields and masks provided in this command to set up the filter. Below are
 * some examples: # MAC + no VLAN filter: This filter is used to identify
 * traffic that does not contain any VLAN tags and matches destination	(or
 * source) MAC address. This filter can be set up by setting only l2_addr field
 * to be a valid field. All other fields are not valid. The following value is
 * set for l2_addr. l2_addr = MAC # MAC + Any VLAN filter: This filter is used
 * to identify traffic that carries single VLAN tag and matches	(destination or
 * source) MAC address. This filter can be set up by setting only l2_addr and
 * l2_ovlan_mask fields to be valid fields. All other fields are not valid. The
 * following values are set for those two valid fields. l2_addr = MAC,
 * l2_ovlan_mask = 0xFFFF # MAC + no VLAN or VLAN ID=0: This filter is used to
 * identify untagged traffic that does not contain any VLAN tags or a VLAN tag
 * with VLAN ID = 0 and matches destination (or source) MAC address. This filter
 * can be set up by setting only l2_addr and l2_ovlan fields to be valid fields.
 * All other fields are not valid. The following value are set for l2_addr and
 * l2_ovlan. l2_addr = MAC, l2_ovlan = 0x0 # MAC + no VLAN or any VLAN: This
 * filter is used to identify traffic that contains zero or 1 VLAN tag and
 * matches destination	(or source) MAC address. This filter can be set up by
 * setting only l2_addr, l2_ovlan, and l2_mask fields to be valid fields. All
 * other fields are not valid. The following value are set for l2_addr,
 * l2_ovlan, and l2_mask fields. l2_addr = MAC, l2_ovlan = 0x0, l2_ovlan_mask =
 * 0xFFFF # MAC + VLAN ID filter: This filter can be set up by setting only
 * l2_addr, l2_ovlan, and l2_ovlan_mask fields to be valid fields. All other
 * fields are not valid. The following values are set for those three valid
 * fields. l2_addr = MAC, l2_ovlan = VLAN ID, l2_ovlan_mask = 0xF000
 */
/* Input	(96 bytes) */
struct hwrm_cfa_l2_filter_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both
	 * TX and RX paths of the chip.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH	UINT32_C(0x1)
	/* tx path */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_TX	\
		(UINT32_C(0x0) << 0)
	/* rx path */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX	\
		(UINT32_C(0x1) << 0)
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_LAST \
		CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX
	/*
	 * Setting of this flag indicates the applicability to the
	 * loopback path.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_LOOPBACK	UINT32_C(0x2)
	/*
	 * Setting of this flag indicates drop action. If this flag is
	 * not set, then it should be considered accept action.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_DROP	UINT32_C(0x4)
	/*
	 * If this flag is set, all t_l2_* fields are invalid and they
	 * should not be specified. If this flag is set, then l2_*
	 * fields refer to fields of outermost L2 header.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_OUTERMOST UINT32_C(0x8)
	uint32_t enables;
	/* This bit must be '1' for the l2_addr field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR UINT32_C(0x1)
	/* This bit must be '1' for the l2_addr_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK	\
		UINT32_C(0x2)
	/* This bit must be '1' for the l2_ovlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN	UINT32_C(0x4)
	/*
	 * This bit must be '1' for the l2_ovlan_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN_MASK	 \
		UINT32_C(0x8)
	/* This bit must be '1' for the l2_ivlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN	UINT32_C(0x10)
	/*
	 * This bit must be '1' for the l2_ivlan_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN_MASK	 \
		UINT32_C(0x20)
	/* This bit must be '1' for the t_l2_addr field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_ADDR UINT32_C(0x40)
	/*
	 * This bit must be '1' for the t_l2_addr_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_ADDR_MASK	\
		UINT32_C(0x80)
	/* This bit must be '1' for the t_l2_ovlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_OVLAN	\
		UINT32_C(0x100)
	/*
	 * This bit must be '1' for the t_l2_ovlan_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_OVLAN_MASK	\
		UINT32_C(0x200)
	/* This bit must be '1' for the t_l2_ivlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_IVLAN	\
		UINT32_C(0x400)
	/*
	 * This bit must be '1' for the t_l2_ivlan_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_IVLAN_MASK	\
		UINT32_C(0x800)
	/* This bit must be '1' for the src_type field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_TYPE	UINT32_C(0x1000)
	/* This bit must be '1' for the src_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_ID UINT32_C(0x2000)
	/* This bit must be '1' for the tunnel_type field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_TUNNEL_TYPE	\
		UINT32_C(0x4000)
	/* This bit must be '1' for the dst_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID UINT32_C(0x8000)
	/*
	 * This bit must be '1' for the mirror_vnic_id field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID	\
		UINT32_C(0x10000)
	uint8_t l2_addr[6];
	/*
	 * This value sets the match value for the L2 MAC address.
	 * Destination MAC address for RX path. Source MAC address for
	 * TX path.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t l2_addr_mask[6];
	/*
	 * This value sets the mask value for the L2 address. A value of
	 * 0 will mask the corresponding bit from compare.
	 */
	uint16_t l2_ovlan;
	/* This value sets VLAN ID value for outer VLAN. */
	uint16_t l2_ovlan_mask;
	/*
	 * This value sets the mask value for the ovlan id. A value of 0
	 * will mask the corresponding bit from compare.
	 */
	uint16_t l2_ivlan;
	/* This value sets VLAN ID value for inner VLAN. */
	uint16_t l2_ivlan_mask;
	/*
	 * This value sets the mask value for the ivlan id. A value of 0
	 * will mask the corresponding bit from compare.
	 */
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t t_l2_addr[6];
	/*
	 * This value sets the match value for the tunnel L2 MAC
	 * address. Destination MAC address for RX path. Source MAC
	 * address for TX path.
	 */
	uint8_t unused_4;
	uint8_t unused_5;
	uint8_t t_l2_addr_mask[6];
	/*
	 * This value sets the mask value for the tunnel L2 address. A
	 * value of 0 will mask the corresponding bit from compare.
	 */
	uint16_t t_l2_ovlan;
	/* This value sets VLAN ID value for tunnel outer VLAN. */
	uint16_t t_l2_ovlan_mask;
	/*
	 * This value sets the mask value for the tunnel ovlan id. A
	 * value of 0 will mask the corresponding bit from compare.
	 */
	uint16_t t_l2_ivlan;
	/* This value sets VLAN ID value for tunnel inner VLAN. */
	uint16_t t_l2_ivlan_mask;
	/*
	 * This value sets the mask value for the tunnel ivlan id. A
	 * value of 0 will mask the corresponding bit from compare.
	 */
	uint8_t src_type;
	/* This value identifies the type of source of the packet. */
	/* Network port */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_NPORT	UINT32_C(0x0)
	/* Physical function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_PF	UINT32_C(0x1)
	/* Virtual function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_VF	UINT32_C(0x2)
	/* Virtual NIC of a function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_VNIC	UINT32_C(0x3)
	/* Embedded processor for CFA management */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_KONG	UINT32_C(0x4)
	/* Embedded processor for OOB management */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_APE	UINT32_C(0x5)
	/* Embedded processor for RoCE */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_BONO	UINT32_C(0x6)
	/* Embedded processor for network proxy functions */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_TANG	UINT32_C(0x7)
	uint8_t unused_6;
	uint32_t src_id;
	/*
	 * This value is the id of the source. For a network port, it
	 * represents port_id. For a physical function, it represents
	 * fid. For a virtual function, it represents vf_id. For a vnic,
	 * it represents vnic_id. For embedded processors, this id is
	 * not valid. Notes: 1. The function ID is implied if it src_id
	 * is not provided for a src_type that is either
	 */
	uint8_t tunnel_type;
	/* Tunnel Type. */
	/* Non-tunnel */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NONTUNNEL \
		UINT32_C(0x0)
	/* Virtual eXtensible Local Area Network	(VXLAN) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_VXLAN \
		UINT32_C(0x1)
	/*
	 * Network Virtualization Generic Routing
	 * Encapsulation	(NVGRE)
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NVGRE \
		UINT32_C(0x2)
	/*
	 * Generic Routing Encapsulation	(GRE) inside
	 * Ethernet payload
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_L2GRE UINT32_C(0x3)
	/* IP in IP */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPIP	UINT32_C(0x4)
	/* Generic Network Virtualization Encapsulation	(Geneve) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_GENEVE UINT32_C(0x5)
	/* Multi-Protocol Lable Switching	(MPLS) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_MPLS	UINT32_C(0x6)
	/* Stateless Transport Tunnel	(STT) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_STT	UINT32_C(0x7)
	/*
	 * Generic Routing Encapsulation	(GRE) inside IP
	 * datagram payload
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPGRE UINT32_C(0x8)
	/*
	 * IPV4 over virtual eXtensible Local Area
	 * Network (IPV4oVXLAN)
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_VXLAN_V4 \
		UINT32_C(0x9)
	/* Any tunneled traffic */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_ANYTUNNEL \
		UINT32_C(0xff)
	uint8_t unused_7;
	uint16_t dst_id;
	/*
	 * If set, this value shall represent the Logical VNIC ID of the
	 * destination VNIC for the RX path and network port id of the
	 * destination port for the TX path.
	 */
	uint16_t mirror_vnic_id;
	/* Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint8_t pri_hint;
	/*
	 * This hint is provided to help in placing the filter in the
	 * filter table.
	 */
	/* No preference */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_NO_PREFER \
		UINT32_C(0x0)
	/* Above the given filter */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_ABOVE_FILTER \
		UINT32_C(0x1)
	/* Below the given filter */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_BELOW_FILTER \
		UINT32_C(0x2)
	/* As high as possible */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_MAX	UINT32_C(0x3)
	/* As low as possible */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_MIN	UINT32_C(0x4)
	uint8_t unused_8;
	uint32_t unused_9;
	uint64_t l2_filter_id_hint;
	/*
	 * This is the ID of the filter that goes along with the
	 * pri_hint. This field is valid only for the following values.
	 * 1 - Above the given filter 2 - Below the given filter
	 */
} __attribute__((packed));

/* Output	(24 bytes) */
struct hwrm_cfa_l2_filter_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint64_t l2_filter_id;
	/*
	 * This value identifies a set of CFA data structures used for
	 * an L2 context.
	 */
	uint32_t flow_id;
	/*
	 * This is the ID of the flow associated with this filter. This
	 * value shall be used to match and associate the flow
	 * identifier returned in completion records. A value of
	 * 0xFFFFFFFF shall indicate no flow id.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_l2_filter_free */
/*
 * Description: Free a L2 filter. The HWRM shall free all associated filter
 * resources with the L2 filter.
 */
/* Input	(24 bytes) */
struct hwrm_cfa_l2_filter_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint64_t l2_filter_id;
	/*
	 * This value identifies a set of CFA data structures used for
	 * an L2 context.
	 */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_l2_filter_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_l2_filter_cfg */
/* Description: Change the configuration of an existing L2 filter */
/* Input	(40 bytes) */
struct hwrm_cfa_l2_filter_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both
	 * TX and RX paths of the chip.
	 */
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_FLAGS_PATH	UINT32_C(0x1)
	/* tx path */
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_FLAGS_PATH_TX \
		(UINT32_C(0x0) << 0)
	/* rx path */
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_FLAGS_PATH_RX \
		(UINT32_C(0x1) << 0)
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_FLAGS_PATH_LAST \
		CFA_L2_FILTER_CFG_INPUT_FLAGS_PATH_RX
	/*
	 * Setting of this flag indicates drop action. If this flag is
	 * not set, then it should be considered accept action.
	 */
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_FLAGS_DROP	UINT32_C(0x2)
	uint32_t enables;
	/* This bit must be '1' for the dst_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_ENABLES_DST_ID	UINT32_C(0x1)
	/*
	 * This bit must be '1' for the new_mirror_vnic_id field to be
	 * configured.
	 */
	#define HWRM_CFA_L2_FILTER_CFG_INPUT_ENABLES_NEW_MIRROR_VNIC_ID   \
		UINT32_C(0x2)
	uint64_t l2_filter_id;
	/*
	 * This value identifies a set of CFA data structures used for
	 * an L2 context.
	 */
	uint32_t dst_id;
	/*
	 * If set, this value shall represent the Logical VNIC ID of the
	 * destination VNIC for the RX path and network port id of the
	 * destination port for the TX path.
	 */
	uint32_t new_mirror_vnic_id;
	/* New Logical VNIC ID of the VNIC where traffic is mirrored. */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_l2_filter_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_l2_set_rx_mask */
/* Description: This command will set rx mask of the function. */
/* Input	(56 bytes) */
struct hwrm_cfa_l2_set_rx_mask_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t vnic_id;
	/* VNIC ID */
	uint32_t mask;
	/* Reserved for future use. */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_RESERVED	UINT32_C(0x1)
	/*
	 * When this bit is '1', the function is requested to accept
	 * multi-cast packets specified by the multicast addr table.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST	UINT32_C(0x2)
	/*
	 * When this bit is '1', the function is requested to accept all
	 * multi-cast packets.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST	UINT32_C(0x4)
	/*
	 * When this bit is '1', the function is requested to accept
	 * broadcast packets.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST	UINT32_C(0x8)
	/*
	 * When this bit is '1', the function is requested to be put in
	 * the promiscuous mode. The HWRM should accept any function to
	 * set up promiscuous mode. The HWRM shall follow the semantics
	 * below for the promiscuous mode support. # When partitioning
	 * is not enabled on a port	(i.e. single PF on the port), then
	 * the PF shall be allowed to be in the promiscuous mode. When
	 * the PF is in the promiscuous mode, then it shall receive all
	 * host bound traffic on that port. # When partitioning is
	 * enabled on a port	(i.e. multiple PFs per port) and a PF on
	 * that port is in the promiscuous mode, then the PF receives
	 * all traffic within that partition as identified by a unique
	 * identifier for the PF	(e.g. S-Tag). If a unique outer VLAN
	 * for the PF is specified, then the setting of promiscuous mode
	 * on that PF shall result in the PF receiving all host bound
	 * traffic with matching outer VLAN. # A VF shall can be set in
	 * the promiscuous mode. In the promiscuous mode, the VF does
	 * not receive any traffic unless a unique outer VLAN for the VF
	 * is specified. If a unique outer VLAN for the VF is specified,
	 * then the setting of promiscuous mode on that VF shall result
	 * in the VF receiving all host bound traffic with the matching
	 * outer VLAN. # The HWRM shall allow the setting of promiscuous
	 * mode on a function independently from the promiscuous mode
	 * settings on other functions.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS UINT32_C(0x10)
	/*
	 * If this flag is set, the corresponding RX filters shall be
	 * set up to cover multicast/broadcast filters for the outermost
	 * Layer 2 destination MAC address field.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_OUTERMOST	UINT32_C(0x20)
	/*
	 * If this flag is set, the corresponding RX filters shall be
	 * set up to cover multicast/broadcast filters for the VLAN-
	 * tagged packets that match the TPID and VID fields of VLAN
	 * tags in the VLAN tag table specified in this command.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLANONLY	UINT32_C(0x40)
	/*
	 * If this flag is set, the corresponding RX filters shall be
	 * set up to cover multicast/broadcast filters for non-VLAN
	 * tagged packets and VLAN-tagged packets that match the TPID
	 * and VID fields of VLAN tags in the VLAN tag table specified
	 * in this command.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN	UINT32_C(0x80)
	/*
	 * If this flag is set, the corresponding RX filters shall be
	 * set up to cover multicast/broadcast filters for non-VLAN
	 * tagged packets and VLAN-tagged packets matching any VLAN tag.
	 * If this flag is set, then the HWRM shall ignore VLAN tags
	 * specified in vlan_tag_tbl. If none of vlanonly, vlan_nonvlan,
	 * and anyvlan_nonvlan flags is set, then the HWRM shall ignore
	 * VLAN tags specified in vlan_tag_tbl. The HWRM client shall
	 * set at most one flag out of vlanonly, vlan_nonvlan, and
	 * anyvlan_nonvlan.
	 */
	#define HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ANYVLAN_NONVLAN	\
		UINT32_C(0x100)
	uint64_t mc_tbl_addr;
	/* This is the address for mcast address tbl. */
	uint32_t num_mc_entries;
	/*
	 * This value indicates how many entries in mc_tbl are valid.
	 * Each entry is 6 bytes.
	 */
	uint32_t unused_0;
	uint64_t vlan_tag_tbl_addr;
	/*
	 * This is the address for VLAN tag table. Each VLAN entry in
	 * the table is 4 bytes of a VLAN tag including TPID, PCP, DEI,
	 * and VID fields in network byte order.
	 */
	uint32_t num_vlan_tags;
	/*
	 * This value indicates how many entries in vlan_tag_tbl are
	 * valid. Each entry is 4 bytes.
	 */
	uint32_t unused_1;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_l2_set_rx_mask_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* Command specific Error Codes (8 bytes) */
struct hwrm_cfa_l2_set_rx_mask_cmd_err {
	uint8_t code;
	/*
	 * command specific error codes that goes to the cmd_err field
	 * in Common HWRM Error Response.
	 */
	/* Unknown error */
	#define HWRM_CFA_L2_SET_RX_MASK_CMD_ERR_CODE_UNKNOWN UINT32_C(0x0)
	/*
	 * Unable to complete operation due to conflict
	 * with Ntuple Filter
	 */
	#define \
	HWRM_CFA_L2_SET_RX_MASK_CMD_ERR_CODE_NTUPLE_FILTER_CONFLICT_ERR \
	UINT32_C(0x1)
	uint8_t unused_0[7];
} __attribute__((packed));

/* hwrm_cfa_vlan_antispoof_cfg */
/* Description: Configures vlan anti-spoof filters for VF. */
/* Input (32 bytes) */
struct hwrm_cfa_vlan_antispoof_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint16_t fid;
	/*
	 * Function ID of the function that is being configured. Only valid for
	 * a VF FID configured by the PF.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t num_vlan_entries;
	/* Number of VLAN entries in the vlan_tag_mask_tbl. */
	uint64_t vlan_tag_mask_tbl_addr;
	/*
	 * The vlan_tag_mask_tbl_addr is the DMA address of the VLAN antispoof
	 * table. Each table entry contains the 16-bit TPID (0x8100 or 0x88a8
	 * only), 16-bit VLAN ID, and a 16-bit mask, all in network order to
	 * match hwrm_cfa_l2_set_rx_mask. For an individual VLAN entry, the mask
	 * value should be 0xfff for the 12-bit VLAN ID.
	 */
};

/* Output (16 bytes) */
struct hwrm_cfa_vlan_antispoof_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
};

/* hwrm_cfa_ntuple_filter_alloc */
/*
 * Description: This is a ntuple filter that uses fields from L4/L3 header and
 * optionally fields from L2. The ntuple filters apply to receive traffic only.
 * All L2/L3/L4 header fields are specified in network byte order. These filters
 * can be used for Receive Flow Steering (RFS). # For ethertype value, only
 * 0x0800 (IPv4) and 0x86dd (IPv6) shall be supported for ntuple filters. # If a
 * field specified in this command is not enabled as a valid field, then that
 * field shall not be used in matching packet header fields against this filter.
 */
/* Input	(128 bytes) */
struct hwrm_cfa_ntuple_filter_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * Setting of this flag indicates the applicability to the
	 * loopback path.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_LOOPBACK	\
		UINT32_C(0x1)
	/*
	 * Setting of this flag indicates drop action. If this flag is
	 * not set, then it should be considered accept action.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_DROP	UINT32_C(0x2)
	/*
	 * Setting of this flag indicates that a meter is expected to be
	 * attached to this flow. This hint can be used when choosing
	 * the action record format required for the flow.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_METER UINT32_C(0x4)
	uint32_t enables;
	/* This bit must be '1' for the l2_filter_id field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID   \
		UINT32_C(0x1)
	/* This bit must be '1' for the ethertype field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_ETHERTYPE	 \
		UINT32_C(0x2)
	/* This bit must be '1' for the tunnel_type field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_TUNNEL_TYPE	\
		UINT32_C(0x4)
	/* This bit must be '1' for the src_macaddr field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_MACADDR	\
		UINT32_C(0x8)
	/* This bit must be '1' for the ipaddr_type field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_IPADDR_TYPE	\
		UINT32_C(0x10)
	/* This bit must be '1' for the src_ipaddr field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_IPADDR	\
		UINT32_C(0x20)
	/*
	 * This bit must be '1' for the src_ipaddr_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_IPADDR_MASK \
		UINT32_C(0x40)
	/* This bit must be '1' for the dst_ipaddr field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_IPADDR	\
		UINT32_C(0x80)
	/*
	 * This bit must be '1' for the dst_ipaddr_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_IPADDR_MASK \
		UINT32_C(0x100)
	/* This bit must be '1' for the ip_protocol field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_IP_PROTOCOL	\
		UINT32_C(0x200)
	/* This bit must be '1' for the src_port field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_PORT	\
		UINT32_C(0x400)
	/*
	 * This bit must be '1' for the src_port_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_PORT_MASK  \
		UINT32_C(0x800)
	/* This bit must be '1' for the dst_port field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_PORT	\
		UINT32_C(0x1000)
	/*
	 * This bit must be '1' for the dst_port_mask field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_PORT_MASK  \
		UINT32_C(0x2000)
	/* This bit must be '1' for the pri_hint field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_PRI_HINT	\
		UINT32_C(0x4000)
	/*
	 * This bit must be '1' for the ntuple_filter_id field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_NTUPLE_FILTER_ID \
		UINT32_C(0x8000)
	/* This bit must be '1' for the dst_id field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_ID	\
		UINT32_C(0x10000)
	/*
	 * This bit must be '1' for the mirror_vnic_id field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID \
		UINT32_C(0x20000)
	/* This bit must be '1' for the dst_macaddr field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_MACADDR	\
		UINT32_C(0x40000)
	uint64_t l2_filter_id;
	/*
	 * This value identifies a set of CFA data structures used for
	 * an L2 context.
	 */
	uint8_t src_macaddr[6];
	/*
	 * This value indicates the source MAC address in the Ethernet
	 * header.
	 */
	uint16_t ethertype;
	/* This value indicates the ethertype in the Ethernet header. */
	uint8_t ip_addr_type;
	/*
	 * This value indicates the type of IP address. 4 - IPv4 6 -
	 * IPv6 All others are invalid.
	 */
	/* invalid */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_ADDR_TYPE_UNKNOWN \
		UINT32_C(0x0)
	/* IPv4 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_ADDR_TYPE_IPV4 \
		UINT32_C(0x4)
	/* IPv6 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_ADDR_TYPE_IPV6 \
		UINT32_C(0x6)
	uint8_t ip_protocol;
	/*
	 * The value of protocol filed in IP header. Applies to UDP and
	 * TCP traffic. 6 - TCP 17 - UDP
	 */
	/* invalid */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_PROTOCOL_UNKNOWN \
		UINT32_C(0x0)
	/* TCP */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_PROTOCOL_TCP \
		UINT32_C(0x6)
	/* UDP */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_PROTOCOL_UDP \
		UINT32_C(0x11)
	uint16_t dst_id;
	/*
	 * If set, this value shall represent the Logical VNIC ID of the
	 * destination VNIC for the RX path and network port id of the
	 * destination port for the TX path.
	 */
	uint16_t mirror_vnic_id;
	/* Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint8_t tunnel_type;
	/*
	 * This value indicates the tunnel type for this filter. If this
	 * field is not specified, then the filter shall apply to both
	 * non-tunneled and tunneled packets. If this field conflicts
	 * with the tunnel_type specified in the l2_filter_id, then the
	 * HWRM shall return an error for this command.
	 */
	/* Non-tunnel */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NONTUNNEL \
		UINT32_C(0x0)
	/* Virtual eXtensible Local Area Network	(VXLAN) */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_VXLAN \
		UINT32_C(0x1)
	/*
	 * Network Virtualization Generic Routing
	 * Encapsulation	(NVGRE)
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NVGRE \
		UINT32_C(0x2)
	/*
	 * Generic Routing Encapsulation	(GRE) inside
	 * Ethernet payload
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_L2GRE \
		UINT32_C(0x3)
	/* IP in IP */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPIP \
		UINT32_C(0x4)
	/* Generic Network Virtualization Encapsulation	(Geneve) */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_GENEVE \
		UINT32_C(0x5)
	/* Multi-Protocol Lable Switching	(MPLS) */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_MPLS \
		UINT32_C(0x6)
	/* Stateless Transport Tunnel	(STT) */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_STT UINT32_C(0x7)
	/*
	 * Generic Routing Encapsulation	(GRE) inside IP
	 * datagram payload
	 */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPGRE \
		UINT32_C(0x8)
	/* Any tunneled traffic */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_TUNNEL_TYPE_ANYTUNNEL \
		UINT32_C(0xff)
	uint8_t pri_hint;
	/*
	 * This hint is provided to help in placing the filter in the
	 * filter table.
	 */
	/* No preference */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_PRI_HINT_NO_PREFER \
		UINT32_C(0x0)
	/* Above the given filter */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_PRI_HINT_ABOVE UINT32_C(0x1)
	/* Below the given filter */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_PRI_HINT_BELOW UINT32_C(0x2)
	/* As high as possible */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_PRI_HINT_HIGHEST \
		UINT32_C(0x3)
	/* As low as possible */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_PRI_HINT_LOWEST UINT32_C(0x4)
	uint32_t src_ipaddr[4];
	/*
	 * The value of source IP address to be used in filtering. For
	 * IPv4, first four bytes represent the IP address.
	 */
	uint32_t src_ipaddr_mask[4];
	/*
	 * The value of source IP address mask to be used in filtering.
	 * For IPv4, first four bytes represent the IP address mask.
	 */
	uint32_t dst_ipaddr[4];
	/*
	 * The value of destination IP address to be used in filtering.
	 * For IPv4, first four bytes represent the IP address.
	 */
	uint32_t dst_ipaddr_mask[4];
	/*
	 * The value of destination IP address mask to be used in
	 * filtering. For IPv4, first four bytes represent the IP
	 * address mask.
	 */
	uint16_t src_port;
	/*
	 * The value of source port to be used in filtering. Applies to
	 * UDP and TCP traffic.
	 */
	uint16_t src_port_mask;
	/*
	 * The value of source port mask to be used in filtering.
	 * Applies to UDP and TCP traffic.
	 */
	uint16_t dst_port;
	/*
	 * The value of destination port to be used in filtering.
	 * Applies to UDP and TCP traffic.
	 */
	uint16_t dst_port_mask;
	/*
	 * The value of destination port mask to be used in filtering.
	 * Applies to UDP and TCP traffic.
	 */
	uint64_t ntuple_filter_id_hint;
	/* This is the ID of the filter that goes along with the pri_hint. */
} __attribute__((packed));

/* Output	(24 bytes) */
struct hwrm_cfa_ntuple_filter_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint64_t ntuple_filter_id;
	/* This value is an opaque id into CFA data structures. */
	uint32_t flow_id;
	/*
	 * This is the ID of the flow associated with this filter. This
	 * value shall be used to match and associate the flow
	 * identifier returned in completion records. A value of
	 * 0xFFFFFFFF shall indicate no flow id.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* Command specific Error Codes (8 bytes) */
struct hwrm_cfa_ntuple_filter_alloc_cmd_err {
	uint8_t code;
	/*
	 * command specific error codes that goes to the cmd_err field
	 * in Common HWRM Error Response.
	 */
	/* Unknown error */
	#define HWRM_CFA_NTUPLE_FILTER_ALLOC_CMD_ERR_CODE_UNKNOWN UINT32_C(0x0)
	/*
	 * Unable to complete operation due to conflict
	 * with Rx Mask VLAN
	 */
	#define \
	HWRM_CFA_NTUPLE_FILTER_ALLOC_CMD_ERR_CODE_RX_MASK_VLAN_CONFLICT_ERR \
	UINT32_C(0x1)
	uint8_t unused_0[7];
} __attribute__((packed));

/* hwrm_cfa_ntuple_filter_free */
/* Description: Free an ntuple filter */
/* Input	(24 bytes) */
struct hwrm_cfa_ntuple_filter_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint64_t ntuple_filter_id;
	/* This value is an opaque id into CFA data structures. */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_ntuple_filter_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_ntuple_filter_cfg */
/*
 * Description: Configure an ntuple filter with a new destination VNIC and/or
 * meter.
 */
/* Input	(48 bytes) */
struct hwrm_cfa_ntuple_filter_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the new_dst_id field to be configured. */
	#define HWRM_CFA_NTUPLE_FILTER_CFG_INPUT_ENABLES_NEW_DST_ID	\
		UINT32_C(0x1)
	/*
	 * This bit must be '1' for the new_mirror_vnic_id field to be
	 * configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_CFG_INPUT_ENABLES_NEW_MIRROR_VNIC_ID \
		UINT32_C(0x2)
	/*
	 * This bit must be '1' for the new_meter_instance_id field to
	 * be configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_CFG_INPUT_ENABLES_NEW_METER_INSTANCE_ID \
		UINT32_C(0x4)
	uint32_t unused_0;
	uint64_t ntuple_filter_id;
	/* This value is an opaque id into CFA data structures. */
	uint32_t new_dst_id;
	/*
	 * If set, this value shall represent the new Logical VNIC ID of
	 * the destination VNIC for the RX path and new network port id
	 * of the destination port for the TX path.
	 */
	uint32_t new_mirror_vnic_id;
	/* New Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint16_t new_meter_instance_id;
	/*
	 * New meter to attach to the flow. Specifying the invalid
	 * instance ID is used to remove any existing meter from the
	 * flow.
	 */
	/*
	 * A value of 0xfff is considered invalid and
	 * implies the instance is not configured.
	 */
	#define HWRM_CFA_NTUPLE_FILTER_CFG_INPUT_NEW_METER_INSTANCE_ID_INVALID \
		UINT32_C(0xffff)
	uint16_t unused_1[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_ntuple_filter_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_em_flow_alloc */
/*
 * Description: This is a generic Exact Match	(EM) flow that uses fields from
 * L4/L3/L2 headers. The EM flows apply to transmit and receive traffic. All
 * L2/L3/L4 header fields are specified in network byte order. For each EM flow,
 * there is an associated set of actions specified. For tunneled packets, all
 * L2/L3/L4 fields specified are fields of inner headers unless otherwise
 * specified. # If a field specified in this command is not enabled as a valid
 * field, then that field shall not be used in matching packet header fields
 * against this EM flow entry.
 */
/* Input	(112 bytes) */
struct hwrm_cfa_em_flow_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t flags;
	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both
	 * TX and RX paths of the chip.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH	UINT32_C(0x1)
	/* tx path */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH_TX	\
		(UINT32_C(0x0) << 0)
	/* rx path */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH_RX	\
		(UINT32_C(0x1) << 0)
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH_LAST \
		CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH_RX
	/*
	 * Setting of this flag indicates enabling of a byte counter for
	 * a given flow.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_BYTE_CTR	UINT32_C(0x2)
	/*
	 * Setting of this flag indicates enabling of a packet counter
	 * for a given flow.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PKT_CTR	UINT32_C(0x4)
	/*
	 * Setting of this flag indicates de-capsulation action for the
	 * given flow.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_DECAP	UINT32_C(0x8)
	/*
	 * Setting of this flag indicates encapsulation action for the
	 * given flow.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_ENCAP	UINT32_C(0x10)
	/*
	 * Setting of this flag indicates drop action. If this flag is
	 * not set, then it should be considered accept action.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_DROP	UINT32_C(0x20)
	/*
	 * Setting of this flag indicates that a meter is expected to be
	 * attached to this flow. This hint can be used when choosing
	 * the action record format required for the flow.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_METER	UINT32_C(0x40)
	uint32_t enables;
	/* This bit must be '1' for the l2_filter_id field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_L2_FILTER_ID UINT32_C(0x1)
	/* This bit must be '1' for the tunnel_type field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_TUNNEL_TYPE UINT32_C(0x2)
	/* This bit must be '1' for the tunnel_id field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_TUNNEL_ID UINT32_C(0x4)
	/* This bit must be '1' for the src_macaddr field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_MACADDR UINT32_C(0x8)
	/* This bit must be '1' for the dst_macaddr field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_MACADDR UINT32_C(0x10)
	/* This bit must be '1' for the ovlan_vid field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_OVLAN_VID UINT32_C(0x20)
	/* This bit must be '1' for the ivlan_vid field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IVLAN_VID UINT32_C(0x40)
	/* This bit must be '1' for the ethertype field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_ETHERTYPE UINT32_C(0x80)
	/* This bit must be '1' for the src_ipaddr field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_IPADDR	UINT32_C(0x100)
	/* This bit must be '1' for the dst_ipaddr field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_IPADDR	UINT32_C(0x200)
	/* This bit must be '1' for the ipaddr_type field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IPADDR_TYPE UINT32_C(0x400)
	/* This bit must be '1' for the ip_protocol field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IP_PROTOCOL UINT32_C(0x800)
	/* This bit must be '1' for the src_port field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_PORT UINT32_C(0x1000)
	/* This bit must be '1' for the dst_port field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_PORT UINT32_C(0x2000)
	/* This bit must be '1' for the dst_id field to be configured. */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_ID	UINT32_C(0x4000)
	/*
	 * This bit must be '1' for the mirror_vnic_id field to be
	 * configured.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID	\
		UINT32_C(0x8000)
	/*
	 * This bit must be '1' for the encap_record_id field to be
	 * configured.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_ENCAP_RECORD_ID	 \
		UINT32_C(0x10000)
	/*
	 * This bit must be '1' for the meter_instance_id field to be
	 * configured.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_METER_INSTANCE_ID	\
		UINT32_C(0x20000)
	uint64_t l2_filter_id;
	/*
	 * This value identifies a set of CFA data structures used for
	 * an L2 context.
	 */
	uint8_t tunnel_type;
	/* Tunnel Type. */
	/* Non-tunnel */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_NONTUNNEL \
		UINT32_C(0x0)
	/* Virtual eXtensible Local Area Network	(VXLAN) */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_VXLAN	UINT32_C(0x1)
	/*
	 * Network Virtualization Generic Routing
	 * Encapsulation	(NVGRE)
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_NVGRE	UINT32_C(0x2)
	/*
	 * Generic Routing Encapsulation	(GRE) inside
	 * Ethernet payload
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_L2GRE	UINT32_C(0x3)
	/* IP in IP */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_IPIP	UINT32_C(0x4)
	/* Generic Network Virtualization Encapsulation	(Geneve) */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_GENEVE	UINT32_C(0x5)
	/* Multi-Protocol Lable Switching	(MPLS) */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_MPLS	UINT32_C(0x6)
	/* Stateless Transport Tunnel	(STT) */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_STT	UINT32_C(0x7)
	/*
	 * Generic Routing Encapsulation	(GRE) inside IP
	 * datagram payload
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_IPGRE	UINT32_C(0x8)
	/*
	 * IPV4 over virtual eXtensible Local Area
	 * Network (IPV4oVXLAN)
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_VXLAN_V4 UINT32_C(0x9)
	/* Any tunneled traffic */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_TUNNEL_TYPE_ANYTUNNEL \
		UINT32_C(0xff)
	uint8_t unused_0;
	uint16_t unused_1;
	uint32_t tunnel_id;
	/*
	 * Tunnel identifier. Virtual Network Identifier	(VNI). Only
	 * valid with tunnel_types VXLAN, NVGRE, and Geneve. Only lower
	 * 24-bits of VNI field are used in setting up the filter.
	 */
	uint8_t src_macaddr[6];
	/*
	 * This value indicates the source MAC address in the Ethernet
	 * header.
	 */
	uint16_t meter_instance_id;
	/* The meter instance to attach to the flow. */
	/*
	 * A value of 0xfff is considered invalid and
	 * implies the instance is not configured.
	 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_METER_INSTANCE_ID_INVALID   \
		UINT32_C(0xffff)
	uint8_t dst_macaddr[6];
	/*
	 * This value indicates the destination MAC address in the
	 * Ethernet header.
	 */
	uint16_t ovlan_vid;
	/*
	 * This value indicates the VLAN ID of the outer VLAN tag in the
	 * Ethernet header.
	 */
	uint16_t ivlan_vid;
	/*
	 * This value indicates the VLAN ID of the inner VLAN tag in the
	 * Ethernet header.
	 */
	uint16_t ethertype;
	/* This value indicates the ethertype in the Ethernet header. */
	uint8_t ip_addr_type;
	/*
	 * This value indicates the type of IP address. 4 - IPv4 6 -
	 * IPv6 All others are invalid.
	 */
	/* invalid */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_UNKNOWN UINT32_C(0x0)
	/* IPv4 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_IPV4	UINT32_C(0x4)
	/* IPv6 */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_IPV6	UINT32_C(0x6)
	uint8_t ip_protocol;
	/*
	 * The value of protocol filed in IP header. Applies to UDP and
	 * TCP traffic. 6 - TCP 17 - UDP
	 */
	/* invalid */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_PROTOCOL_UNKNOWN UINT32_C(0x0)
	/* TCP */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_PROTOCOL_TCP UINT32_C(0x6)
	/* UDP */
	#define HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_PROTOCOL_UDP UINT32_C(0x11)
	uint8_t unused_2;
	uint8_t unused_3;
	uint32_t src_ipaddr[4];
	/*
	 * The value of source IP address to be used in filtering. For
	 * IPv4, first four bytes represent the IP address.
	 */
	uint32_t dst_ipaddr[4];
	/*
	 * big_endian = True The value of destination IP address to be
	 * used in filtering. For IPv4, first four bytes represent the
	 * IP address.
	 */
	uint16_t src_port;
	/*
	 * The value of source port to be used in filtering. Applies to
	 * UDP and TCP traffic.
	 */
	uint16_t dst_port;
	/*
	 * The value of destination port to be used in filtering.
	 * Applies to UDP and TCP traffic.
	 */
	uint16_t dst_id;
	/*
	 * If set, this value shall represent the Logical VNIC ID of the
	 * destination VNIC for the RX path and network port id of the
	 * destination port for the TX path.
	 */
	uint16_t mirror_vnic_id;
	/* Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint32_t encap_record_id;
	/* Logical ID of the encapsulation record. */
	uint32_t unused_4;
} __attribute__((packed));

/* Output	(24 bytes) */
struct hwrm_cfa_em_flow_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint64_t em_filter_id;
	/* This value is an opaque id into CFA data structures. */
	uint32_t flow_id;
	/*
	 * This is the ID of the flow associated with this filter. This
	 * value shall be used to match and associate the flow
	 * identifier returned in completion records. A value of
	 * 0xFFFFFFFF shall indicate no flow id.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_em_flow_free */
/* Description: Free an EM flow table entry */
/* Input	(24 bytes) */
struct hwrm_cfa_em_flow_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint64_t em_filter_id;
	/* This value is an opaque id into CFA data structures. */
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_em_flow_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_cfa_em_flow_cfg */
/*
 * Description: Configure an EM flow with a new destination VNIC and/or meter.
 */
/* Input	(48 bytes) */
struct hwrm_cfa_em_flow_cfg_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t enables;
	/* This bit must be '1' for the new_dst_id field to be configured. */
	#define HWRM_CFA_EM_FLOW_CFG_INPUT_ENABLES_NEW_DST_ID	UINT32_C(0x1)
	/*
	 * This bit must be '1' for the new_mirror_vnic_id field to be
	 * configured.
	 */
	#define HWRM_CFA_EM_FLOW_CFG_INPUT_ENABLES_NEW_MIRROR_VNIC_ID	\
		UINT32_C(0x2)
	/*
	 * This bit must be '1' for the new_meter_instance_id field to
	 * be configured.
	 */
	#define HWRM_CFA_EM_FLOW_CFG_INPUT_ENABLES_NEW_METER_INSTANCE_ID  \
		UINT32_C(0x4)
	uint32_t unused_0;
	uint64_t em_filter_id;
	/* This value is an opaque id into CFA data structures. */
	uint32_t new_dst_id;
	/*
	 * If set, this value shall represent the new Logical VNIC ID of
	 * the destination VNIC for the RX path and network port id of
	 * the destination port for the TX path.
	 */
	uint32_t new_mirror_vnic_id;
	/* New Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint16_t new_meter_instance_id;
	/*
	 * New meter to attach to the flow. Specifying the invalid
	 * instance ID is used to remove any existing meter from the
	 * flow.
	 */
	/*
	 * A value of 0xfff is considered invalid and
	 * implies the instance is not configured.
	 */
	#define HWRM_CFA_EM_FLOW_CFG_INPUT_NEW_METER_INSTANCE_ID_INVALID \
		UINT32_C(0xffff)
	uint16_t unused_1[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_cfa_em_flow_cfg_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_tunnel_dst_port_query */
/*
 * Description: This function is called by a driver to query tunnel type
 * specific destination port configuration.
 */
/* Input	(24 bytes) */
struct hwrm_tunnel_dst_port_query_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint8_t tunnel_type;
	/* Tunnel Type. */
	/* Virtual eXtensible Local Area Network	(VXLAN) */
	#define HWRM_TUNNEL_DST_PORT_QUERY_INPUT_TUNNEL_TYPE_VXLAN \
		UINT32_C(0x1)
	/* Generic Network Virtualization Encapsulation	(Geneve) */
	#define HWRM_TUNNEL_DST_PORT_QUERY_INPUT_TUNNEL_TYPE_GENEVE \
		UINT32_C(0x5)
	/*
	 * IPV4 over virtual eXtensible Local Area
	 * Network (IPV4oVXLAN)
	 */
	#define HWRM_TUNNEL_DST_PORT_QUERY_INPUT_TUNNEL_TYPE_VXLAN_V4 \
		UINT32_C(0x9)
	uint8_t unused_0[7];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_tunnel_dst_port_query_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t tunnel_dst_port_id;
	/*
	 * This field represents the identifier of L4 destination port
	 * used for the given tunnel type. This field is valid for
	 * specific tunnel types that use layer 4	(e.g. UDP) transports
	 * for tunneling.
	 */
	uint16_t tunnel_dst_port_val;
	/*
	 * This field represents the value of L4 destination port
	 * identified by tunnel_dst_port_id. This field is valid for
	 * specific tunnel types that use layer 4	(e.g. UDP) transports
	 * for tunneling. This field is in network byte order. A value
	 * of 0 means that the destination port is not configured.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_tunnel_dst_port_alloc */
/*
 * Description: This function is called by a driver to allocate l4 destination
 * port for a specific tunnel type. The destination port value is provided in
 * the input. If the HWRM supports only one global destination port for a tunnel
 * type, then the HWRM shall keep track of its usage as described below. # The
 * first caller that allocates a destination port shall always succeed and the
 * HWRM shall save the destination port configuration for that tunnel type and
 * increment the usage count to 1. # Subsequent callers allocating the same
 * destination port for that tunnel type shall succeed and the HWRM shall
 * increment the usage count for that port for each subsequent caller that
 * succeeds. # Any subsequent caller trying to allocate a different destination
 * port for that tunnel type shall fail until the usage count for the original
 * destination port goes to zero. # A caller that frees a port will cause the
 * usage count for that port to decrement.
 */
/* Input	(24 bytes) */
struct hwrm_tunnel_dst_port_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint8_t tunnel_type;
	/* Tunnel Type. */
	/* Virtual eXtensible Local Area Network	(VXLAN) */
	#define HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_VXLAN UINT32_C(0x1)
	/* Generic Network Virtualization Encapsulation	(Geneve) */
	#define HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_GENEVE \
		UINT32_C(0x5)
	/*
	 * IPV4 over virtual eXtensible Local Area
	 * Network (IPV4oVXLAN)
	 */
	#define HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_VXLAN_V4 \
		UINT32_C(0x9)
	uint8_t unused_0;
	uint16_t tunnel_dst_port_val;
	/*
	 * This field represents the value of L4 destination port used
	 * for the given tunnel type. This field is valid for specific
	 * tunnel types that use layer 4	(e.g. UDP) transports for
	 * tunneling. This field is in network byte order. A value of 0
	 * shall fail the command.
	 */
	uint32_t unused_1;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_tunnel_dst_port_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint16_t tunnel_dst_port_id;
	/*
	 * Identifier of a tunnel L4 destination port value. Only
	 * applies to tunnel types that has l4 destination port
	 * parameters.
	 */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t unused_4;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_tunnel_dst_port_free */
/*
 * Description: This function is called by a driver to free l4 destination port
 * for a specific tunnel type.
 */
/* Input	(24 bytes) */
struct hwrm_tunnel_dst_port_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint8_t tunnel_type;
	/* Tunnel Type. */
	/* Virtual eXtensible Local Area Network	(VXLAN) */
	#define HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN UINT32_C(0x1)
	/* Generic Network Virtualization Encapsulation	(Geneve) */
	#define HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_GENEVE UINT32_C(0x5)
	/*
	 * IPV4 over virtual eXtensible Local Area
	 * Network (IPV4oVXLAN)
	 */
	#define HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN_V4 \
		UINT32_C(0x9)
	uint8_t unused_0;
	uint16_t tunnel_dst_port_id;
	/*
	 * Identifier of a tunnel L4 destination port value. Only
	 * applies to tunnel types that has l4 destination port
	 * parameters.
	 */
	uint32_t unused_1;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_tunnel_dst_port_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_stat_ctx_alloc */
/*
 * Description: This command allocates and does basic preparation for a stat
 * context.
 */
/* Input	(32 bytes) */
struct hwrm_stat_ctx_alloc_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint64_t stats_dma_addr;
	/* This is the address for statistic block. */
	uint32_t update_period_ms;
	/*
	 * The statistic block update period in ms. e.g. 250ms, 500ms,
	 * 750ms, 1000ms. If update_period_ms is 0, then the stats
	 * update shall be never done and the DMA address shall not be
	 * used. In this case, the stat block can only be read by
	 * hwrm_stat_ctx_query command.
	 */
	uint8_t stat_ctx_flags;
	/*
	 * This field is used to specify statistics context specific
	 * configuration flags.
	 */
	/*
	 * When this bit is set to '1', the statistics context shall be
	 * allocated for RoCE traffic only. In this case, traffic other
	 * than offloaded RoCE traffic shall not be included in this
	 * statistic context. When this bit is set to '0', the
	 * statistics context shall be used for the network traffic
	 * other than offloaded RoCE traffic.
	 */
	#define HWRM_STAT_CTX_ALLOC_INPUT_STAT_CTX_FLAGS_ROCE	UINT32_C(0x1)
	uint8_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_stat_ctx_alloc_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t stat_ctx_id;
	/* This is the statistics context ID value. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_stat_ctx_free */
/* Description: This command is used to free a stat context. */
/* Input	(24 bytes) */
struct hwrm_stat_ctx_free_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t stat_ctx_id;
	/* ID of the statistics context that is being queried. */
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_stat_ctx_free_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t stat_ctx_id;
	/* This is the statistics context ID value. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_stat_ctx_query */
/* Description: This command returns statistics of a context. */
/* Input (24 bytes) */
struct hwrm_stat_ctx_query_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint32_t stat_ctx_id;
	/* ID of the statistics context that is being queried. */
	uint32_t unused_0;
} __attribute__((packed));

/* Output (176 bytes) */
struct hwrm_stat_ctx_query_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint64_t tx_ucast_pkts;
	/* Number of transmitted unicast packets */
	uint64_t tx_mcast_pkts;
	/* Number of transmitted multicast packets */
	uint64_t tx_bcast_pkts;
	/* Number of transmitted broadcast packets */
	uint64_t tx_err_pkts;
	/* Number of transmitted packets with error */
	uint64_t tx_drop_pkts;
	/* Number of dropped packets on transmit path */
	uint64_t tx_ucast_bytes;
	/* Number of transmitted bytes for unicast traffic */
	uint64_t tx_mcast_bytes;
	/* Number of transmitted bytes for multicast traffic */
	uint64_t tx_bcast_bytes;
	/* Number of transmitted bytes for broadcast traffic */
	uint64_t rx_ucast_pkts;
	/* Number of received unicast packets */
	uint64_t rx_mcast_pkts;
	/* Number of received multicast packets */
	uint64_t rx_bcast_pkts;
	/* Number of received broadcast packets */
	uint64_t rx_err_pkts;
	/* Number of received packets with error */
	uint64_t rx_drop_pkts;
	/* Number of dropped packets on received path */
	uint64_t rx_ucast_bytes;
	/* Number of received bytes for unicast traffic */
	uint64_t rx_mcast_bytes;
	/* Number of received bytes for multicast traffic */
	uint64_t rx_bcast_bytes;
	/* Number of received bytes for broadcast traffic */
	uint64_t rx_agg_pkts;
	/* Number of aggregated unicast packets */
	uint64_t rx_agg_bytes;
	/* Number of aggregated unicast bytes */
	uint64_t rx_agg_events;
	/* Number of aggregation events */
	uint64_t rx_agg_aborts;
	/* Number of aborted aggregations */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
} __attribute__((packed));

/* hwrm_stat_ctx_clr_stats */
/* Description: This command clears statistics of a context. */
/* Input	(24 bytes) */
struct hwrm_stat_ctx_clr_stats_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t stat_ctx_id;
	/* ID of the statistics context that is being queried. */
	uint32_t unused_0;
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_stat_ctx_clr_stats_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
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
/* Input	(128 bytes) */
struct hwrm_exec_fwd_resp_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t encap_request[26];
	/*
	 * This is an encapsulated request. This request should be
	 * executed by the HWRM and the response should be provided in
	 * the response buffer inside the encapsulated request.
	 */
	uint16_t encap_resp_target_id;
	/*
	 * This value indicates the target id of the response to the
	 * encapsulated request. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF -
	 * HWRM
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_exec_fwd_resp_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_reject_fwd_resp */
/*
 * Description: This command is used to send an encapsulated request to the
 * HWRM. This command instructs the HWRM to reject the request and forward the
 * error response of the encapsulated request to the location specified in the
 * original request that is encapsulated. The target id of this command shall be
 * set to 0xFFFF (HWRM). The response location in this command shall be used to
 * acknowledge the receipt of the encapsulated request and forwarding of the
 * response.
 */
/* Input	(128 bytes) */
struct hwrm_reject_fwd_resp_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint32_t encap_request[26];
	/*
	 * This is an encapsulated request. This request should be
	 * rejected by the HWRM and the error response should be
	 * provided in the response buffer inside the encapsulated
	 * request.
	 */
	uint16_t encap_resp_target_id;
	/*
	 * This value indicates the target id of the response to the
	 * encapsulated request. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF -
	 * HWRM
	 */
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_reject_fwd_resp_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_nvm_get_dir_entries */
/* Input (24 bytes) */
struct hwrm_nvm_get_dir_entries_input {
	uint16_t req_type;
	uint16_t cmpl_ring;
	uint16_t seq_id;
	uint16_t target_id;
	uint64_t resp_addr;
	uint64_t host_dest_addr;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_nvm_get_dir_entries_output {
	uint16_t error_code;
	uint16_t req_type;
	uint16_t seq_id;
	uint16_t resp_len;
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
} __attribute__((packed));


/* hwrm_nvm_erase_dir_entry */
/* Input (24 bytes) */
struct hwrm_nvm_erase_dir_entry_input {
	uint16_t req_type;
	uint16_t cmpl_ring;
	uint16_t seq_id;
	uint16_t target_id;
	uint64_t resp_addr;
	uint16_t dir_idx;
	uint16_t unused_0[3];
};

/* Output (16 bytes) */
struct hwrm_nvm_erase_dir_entry_output {
	uint16_t error_code;
	uint16_t req_type;
	uint16_t seq_id;
	uint16_t resp_len;
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
};

/* hwrm_nvm_get_dir_info */
/* Input (16 bytes) */
struct hwrm_nvm_get_dir_info_input {
	uint16_t req_type;
	uint16_t cmpl_ring;
	uint16_t seq_id;
	uint16_t target_id;
	uint64_t resp_addr;
} __attribute__((packed));

/* Output (24 bytes) */
struct hwrm_nvm_get_dir_info_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t entries;
	/* Number of directory entries in the directory. */
	uint32_t entry_length;
	/* Size of each directory entry, in bytes. */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_nvm_write */
/*
 * Note: Write to the allocated NVRAM of an item referenced by an existing
 * directory entry.
 */
/* Input (48 bytes) */
struct hwrm_nvm_write_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint64_t host_src_addr;
	/* 64-bit Host Source Address. This is where the source data is. */
	uint16_t dir_type;
	/*
	 * The Directory Entry Type (valid values are defined in the
	 * bnxnvm_directory_type enum defined in the file
	 * bnxnvm_defs.h).
	 */
	uint16_t dir_ordinal;
	/*
	 * Directory ordinal. The 0-based instance of the combined
	 * Directory Entry Type and Extension.
	 */
	uint16_t dir_ext;
	/*
	 * The Directory Entry Extension flags (see BNX_DIR_EXT_* in the
	 * file bnxnvm_defs.h).
	 */
	uint16_t dir_attr;
	/*
	 * Directory Entry Attribute flags (see BNX_DIR_ATTR_* in the
	 * file bnxnvm_defs.h).
	 */
	uint32_t dir_data_length;
	/*
	 * Length of data to write, in bytes. May be less than or equal
	 * to the allocated size for the directory entry. The data
	 * length stored in the directory entry will be updated to
	 * reflect this value once the write is complete.
	 */
	uint16_t option;
	/* Option. */
	uint16_t flags;
	/*
	 * When this bit is '1', the original active image will not be
	 * removed. TBD: what purpose is this?
	 */
	#define HWRM_NVM_WRITE_INPUT_FLAGS_KEEP_ORIG_ACTIVE_IMG UINT32_C(0x1)
	uint32_t dir_item_length;
	/*
	 * The requested length of the allocated NVM for the item, in
	 * bytes. This value may be greater than or equal to the
	 * specified data length (dir_data_length). If this value is
	 * less than the specified data length, it will be ignored. The
	 * response will contain the actual allocated item length, which
	 * may be greater than the requested item length. The purpose
	 * for allocating more than the required number of bytes for an
	 * item's data is to pre-allocate extra storage (padding) to
	 * accommodate the potential future growth of an item (e.g.
	 * upgraded firmware with a size increase, log growth, expanded
	 * configuration data).
	 */
	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_nvm_write_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t dir_item_length;
	/*
	 * Length of the allocated NVM for the item, in bytes. The value
	 * may be greater than or equal to the specified data length or
	 * the requested item length. The actual item length used when
	 * creating a new directory entry will be a multiple of an NVM
	 * block size.
	 */
	uint16_t dir_idx;
	/* The directory index of the created or modified item. */
	uint8_t unused_0;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* hwrm_nvm_read */
/*
 * Note: Read the contents of an NVRAM item as referenced (indexed) by an
 * existing directory entry.
 */
/* Input (40 bytes) */
struct hwrm_nvm_read_input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
	uint64_t host_dest_addr;
	/*
	 * 64-bit Host Destination Address. This is the host address
	 * where the data will be written to.
	 */
	uint16_t dir_idx;
	/* The 0-based index of the directory entry. */
	uint8_t unused_0;
	uint8_t unused_1;
	uint32_t offset;
	/* The NVRAM byte-offset to read from. */
	uint32_t len;
	/* The length of the data to be read, in bytes. */
	uint32_t unused_2;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_nvm_read_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* Hardware Resource Manager Specification */
/* Description: This structure is used to specify port description. */
/*
 * Note: The Hardware Resource Manager (HWRM) manages various hardware resources
 * inside the chip. The HWRM is implemented in firmware, and runs on embedded
 * processors inside the chip. This firmware service is vital part of the chip.
 * The chip can not be used by a driver or HWRM client without the HWRM.
 */
/* Input	(16 bytes) */
struct input {
	uint16_t req_type;
	/*
	 * This value indicates what type of request this is. The format
	 * for the rest of the command is determined by this field.
	 */
	uint16_t cmpl_ring;
	/*
	 * This value indicates the what completion ring the request
	 * will be optionally completed on. If the value is -1, then no
	 * CR completion will be generated. Any other value must be a
	 * valid CR ring_id value for this function.
	 */
	uint16_t seq_id;
	/* This value indicates the command sequence number. */
	uint16_t target_id;
	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function
	 * ids 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF
	 * - HWRM
	 */
	uint64_t resp_addr;
	/*
	 * This is the host address where the response will be written
	 * when the request is complete. This area must be 16B aligned
	 * and must be cleared to zero before the request is made.
	 */
} __attribute__((packed));

/* Output	(8 bytes) */
struct output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
} __attribute__((packed));

/* Short Command Structure (16 bytes) */
struct hwrm_short_input {
	uint16_t req_type;
	/*
	 * This field indicates the type of request in the request
	 * buffer. The format for the rest of the command (request) is
	 * determined by this field.
	 */
	uint16_t signature;
	/*
	 * This field indicates a signature that is used to identify
	 * short form of the command listed here. This field shall be
	 * set to 17185 (0x4321).
	 */
	/* Signature indicating this is a short form of HWRM command */
	#define HWRM_SHORT_REQ_SIGNATURE_SHORT_CMD UINT32_C(0x4321)
	uint16_t unused_0;
	/* Reserved for future use. */
	uint16_t size;
	/* This value indicates the length of the request. */
	uint64_t req_addr;
	/*
	 * This is the host address where the request was written. This
	 * area must be 16B aligned.
	 */
} __attribute__((packed));

#define HWRM_GET_HWRM_ERROR_CODE(arg) \
	{ \
		typeof(arg) x = (arg); \
	((x) == 0xf ? "HWRM_ERROR" : \
	((x) == 0xffff ? "CMD_NOT_SUPPORTED" : \
	((x) == 0xfffe ? "UNKNOWN_ERR" : \
	((x) == 0x4 ? "RESOURCE_ALLOC_ERROR" : \
	((x) == 0x5 ? "INVALID_FLAGS" : \
	((x) == 0x6 ? "INVALID_ENABLES" : \
	((x) == 0x0 ? "SUCCESS" : \
	((x) == 0x1 ? "FAIL" : \
	((x) == 0x2 ? "INVALID_PARAMS" : \
	((x) == 0x3 ? "RESOURCE_ACCESS_DENIED" : \
	"Unknown error_code")))))))))) \
	}

/* Return Codes	(8 bytes) */
struct ret_codes {
	uint16_t error_code;
	/* These are numbers assigned to return/error codes. */
	/* Request was successfully executed by the HWRM. */
	#define HWRM_ERR_CODE_SUCCESS	(UINT32_C(0x0))
	/* THe HWRM failed to execute the request. */
	#define HWRM_ERR_CODE_FAIL	(UINT32_C(0x1))
	/*
	 * The request contains invalid argument(s) or
	 * input parameters.
	 */
	#define HWRM_ERR_CODE_INVALID_PARAMS	(UINT32_C(0x2))
	/*
	 * The requester is not allowed to access the
	 * requested resource. This error code shall be
	 * provided in a response to a request to query
	 * or modify an existing resource that is not
	 * accessible by the requester.
	 */
	#define HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED	(UINT32_C(0x3))
	/*
	 * The HWRM is unable to allocate the requested
	 * resource. This code only applies to requests
	 * for HWRM resource allocations.
	 */
	#define HWRM_ERR_CODE_RESOURCE_ALLOC_ERROR	(UINT32_C(0x4))
	/* Invalid combination of flags is specified in the request. */
	#define HWRM_ERR_CODE_INVALID_FLAGS	(UINT32_C(0x5))
	/*
	 * Invalid combination of enables fields is
	 * specified in the request.
	 */
	#define HWRM_ERR_CODE_INVALID_ENABLES	(UINT32_C(0x6))
	/*
	 * Generic HWRM execution error that represents
	 * an internal error.
	 */
	#define HWRM_ERR_CODE_HWRM_ERROR	(UINT32_C(0xf))
	/* Unknown error */
	#define HWRM_ERR_CODE_UNKNOWN_ERR	(UINT32_C(0xfffe))
	/* Unsupported or invalid command */
	#define HWRM_ERR_CODE_CMD_NOT_SUPPORTED	(UINT32_C(0xffff))
	uint16_t unused_0[3];
} __attribute__((packed));

/* Output	(16 bytes) */
struct hwrm_err_output {
	uint16_t error_code;
	/*
	 * Pass/Fail or error type Note: receiver to verify the in
	 * parameters, and fail the call with an error when appropriate
	 */
	uint16_t req_type;
	/* This field returns the type of original request. */
	uint16_t seq_id;
	/* This field provides original sequence number of the command. */
	uint16_t resp_len;
	/*
	 * This field is the length of the response in bytes. The last
	 * byte of the response is a valid flag that will read as '1'
	 * when the command has been completely written to memory.
	 */
	uint32_t opaque_0;
	/* debug info for this error response. */
	uint16_t opaque_1;
	/* debug info for this error response. */
	uint8_t cmd_err;
	/*
	 * In the case of an error response, command specific error code
	 * is returned in this field.
	 */
	uint8_t valid;
	/*
	 * This field is used in Output records to indicate that the
	 * output is completely written to RAM. This field should be
	 * read as '1' to indicate that the output has been completely
	 * written. When writing a command completion or response to an
	 * internal processor, the order of writes has to be such that
	 * this field is written last.
	 */
} __attribute__((packed));

/* Port Tx Statistics Formats	(408 bytes) */
struct tx_port_stats {
	uint64_t tx_64b_frames;
	/* Total Number of 64 Bytes frames transmitted */
	uint64_t tx_65b_127b_frames;
	/* Total Number of 65-127 Bytes frames transmitted */
	uint64_t tx_128b_255b_frames;
	/* Total Number of 128-255 Bytes frames transmitted */
	uint64_t tx_256b_511b_frames;
	/* Total Number of 256-511 Bytes frames transmitted */
	uint64_t tx_512b_1023b_frames;
	/* Total Number of 512-1023 Bytes frames transmitted */
	uint64_t tx_1024b_1518_frames;
	/* Total Number of 1024-1518 Bytes frames transmitted */
	uint64_t tx_good_vlan_frames;
	/*
	 * Total Number of each good VLAN	(exludes FCS errors) frame
	 * transmitted which is 1519 to 1522 bytes in length inclusive
	 *	(excluding framing bits but including FCS bytes).
	 */
	uint64_t tx_1519b_2047_frames;
	/* Total Number of 1519-2047 Bytes frames transmitted */
	uint64_t tx_2048b_4095b_frames;
	/* Total Number of 2048-4095 Bytes frames transmitted */
	uint64_t tx_4096b_9216b_frames;
	/* Total Number of 4096-9216 Bytes frames transmitted */
	uint64_t tx_9217b_16383b_frames;
	/* Total Number of 9217-16383 Bytes frames transmitted */
	uint64_t tx_good_frames;
	/* Total Number of good frames transmitted */
	uint64_t tx_total_frames;
	/* Total Number of frames transmitted */
	uint64_t tx_ucast_frames;
	/* Total number of unicast frames transmitted */
	uint64_t tx_mcast_frames;
	/* Total number of multicast frames transmitted */
	uint64_t tx_bcast_frames;
	/* Total number of broadcast frames transmitted */
	uint64_t tx_pause_frames;
	/* Total number of PAUSE control frames transmitted */
	uint64_t tx_pfc_frames;
	/* Total number of PFC/per-priority PAUSE control frames transmitted */
	uint64_t tx_jabber_frames;
	/* Total number of jabber frames transmitted */
	uint64_t tx_fcs_err_frames;
	/* Total number of frames transmitted with FCS error */
	uint64_t tx_control_frames;
	/* Total number of control frames transmitted */
	uint64_t tx_oversz_frames;
	/* Total number of over-sized frames transmitted */
	uint64_t tx_single_dfrl_frames;
	/* Total number of frames with single deferral */
	uint64_t tx_multi_dfrl_frames;
	/* Total number of frames with multiple deferrals */
	uint64_t tx_single_coll_frames;
	/* Total number of frames with single collision */
	uint64_t tx_multi_coll_frames;
	/* Total number of frames with multiple collisions */
	uint64_t tx_late_coll_frames;
	/* Total number of frames with late collisions */
	uint64_t tx_excessive_coll_frames;
	/* Total number of frames with excessive collisions */
	uint64_t tx_frag_frames;
	/* Total number of fragmented frames transmitted */
	uint64_t tx_err;
	/* Total number of transmit errors */
	uint64_t tx_tagged_frames;
	/* Total number of single VLAN tagged frames transmitted */
	uint64_t tx_dbl_tagged_frames;
	/* Total number of double VLAN tagged frames transmitted */
	uint64_t tx_runt_frames;
	/* Total number of runt frames transmitted */
	uint64_t tx_fifo_underruns;
	/* Total number of TX FIFO under runs */
	uint64_t tx_pfc_ena_frames_pri0;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 0
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri1;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 1
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri2;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 2
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri3;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 3
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri4;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 4
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri5;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 5
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri6;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 6
	 * transmitted
	 */
	uint64_t tx_pfc_ena_frames_pri7;
	/*
	 * Total number of PFC frames with PFC enabled bit for Pri 7
	 * transmitted
	 */
	uint64_t tx_eee_lpi_events;
	/* Total number of EEE LPI Events on TX */
	uint64_t tx_eee_lpi_duration;
	/* EEE LPI Duration Counter on TX */
	uint64_t tx_llfc_logical_msgs;
	/*
	 * Total number of Link Level Flow Control	(LLFC) messages
	 * transmitted
	 */
	uint64_t tx_hcfc_msgs;
	/* Total number of HCFC messages transmitted */
	uint64_t tx_total_collisions;
	/* Total number of TX collisions */
	uint64_t tx_bytes;
	/* Total number of transmitted bytes */
	uint64_t tx_xthol_frames;
	/* Total number of end-to-end HOL frames */
	uint64_t tx_stat_discard;
	/* Total Tx Drops per Port reported by STATS block */
	uint64_t tx_stat_error;
	/* Total Tx Error Drops per Port reported by STATS block */
} __attribute__((packed));

/* Port Rx Statistics Formats	(528 bytes) */
struct rx_port_stats {
	uint64_t rx_64b_frames;
	/* Total Number of 64 Bytes frames received */
	uint64_t rx_65b_127b_frames;
	/* Total Number of 65-127 Bytes frames received */
	uint64_t rx_128b_255b_frames;
	/* Total Number of 128-255 Bytes frames received */
	uint64_t rx_256b_511b_frames;
	/* Total Number of 256-511 Bytes frames received */
	uint64_t rx_512b_1023b_frames;
	/* Total Number of 512-1023 Bytes frames received */
	uint64_t rx_1024b_1518_frames;
	/* Total Number of 1024-1518 Bytes frames received */
	uint64_t rx_good_vlan_frames;
	/*
	 * Total Number of each good VLAN	(exludes FCS errors) frame
	 * received which is 1519 to 1522 bytes in length inclusive
	 *	(excluding framing bits but including FCS bytes).
	 */
	uint64_t rx_1519b_2047b_frames;
	/* Total Number of 1519-2047 Bytes frames received */
	uint64_t rx_2048b_4095b_frames;
	/* Total Number of 2048-4095 Bytes frames received */
	uint64_t rx_4096b_9216b_frames;
	/* Total Number of 4096-9216 Bytes frames received */
	uint64_t rx_9217b_16383b_frames;
	/* Total Number of 9217-16383 Bytes frames received */
	uint64_t rx_total_frames;
	/* Total number of frames received */
	uint64_t rx_ucast_frames;
	/* Total number of unicast frames received */
	uint64_t rx_mcast_frames;
	/* Total number of multicast frames received */
	uint64_t rx_bcast_frames;
	/* Total number of broadcast frames received */
	uint64_t rx_fcs_err_frames;
	/* Total number of received frames with FCS error */
	uint64_t rx_ctrl_frames;
	/* Total number of control frames received */
	uint64_t rx_pause_frames;
	/* Total number of PAUSE frames received */
	uint64_t rx_pfc_frames;
	/* Total number of PFC frames received */
	uint64_t rx_unsupported_opcode_frames;
	/* Total number of frames received with an unsupported opcode */
	uint64_t rx_unsupported_da_pausepfc_frames;
	/*
	 * Total number of frames received with an unsupported DA for
	 * pause and PFC
	 */
	uint64_t rx_wrong_sa_frames;
	/* Total number of frames received with an unsupported SA */
	uint64_t rx_align_err_frames;
	/* Total number of received packets with alignment error */
	uint64_t rx_oor_len_frames;
	/* Total number of received frames with out-of-range length */
	uint64_t rx_code_err_frames;
	/* Total number of received frames with error termination */
	uint64_t rx_false_carrier_frames;
	/*
	 * Total number of received frames with a false carrier is
	 * detected during idle, as defined by RX_ER samples active and
	 * RXD is 0xE. The event is reported along with the statistics
	 * generated on the next received frame. Only one false carrier
	 * condition can be detected and logged between frames. Carrier
	 * event, valid for 10M/100M speed modes only.
	 */
	uint64_t rx_ovrsz_frames;
	/* Total number of over-sized frames received */
	uint64_t rx_jbr_frames;
	/* Total number of jabber packets received */
	uint64_t rx_mtu_err_frames;
	/* Total number of received frames with MTU error */
	uint64_t rx_match_crc_frames;
	/* Total number of received frames with CRC match */
	uint64_t rx_promiscuous_frames;
	/* Total number of frames received promiscuously */
	uint64_t rx_tagged_frames;
	/* Total number of received frames with one or two VLAN tags */
	uint64_t rx_double_tagged_frames;
	/* Total number of received frames with two VLAN tags */
	uint64_t rx_trunc_frames;
	/* Total number of truncated frames received */
	uint64_t rx_good_frames;
	/* Total number of good frames	(without errors) received */
	uint64_t rx_pfc_xon2xoff_frames_pri0;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 0
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri1;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 1
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri2;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 2
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri3;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 3
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri4;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 4
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri5;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 5
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri6;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 6
	 */
	uint64_t rx_pfc_xon2xoff_frames_pri7;
	/*
	 * Total number of received PFC frames with transition from XON
	 * to XOFF on Pri 7
	 */
	uint64_t rx_pfc_ena_frames_pri0;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 0
	 */
	uint64_t rx_pfc_ena_frames_pri1;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 1
	 */
	uint64_t rx_pfc_ena_frames_pri2;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 2
	 */
	uint64_t rx_pfc_ena_frames_pri3;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 3
	 */
	uint64_t rx_pfc_ena_frames_pri4;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 4
	 */
	uint64_t rx_pfc_ena_frames_pri5;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 5
	 */
	uint64_t rx_pfc_ena_frames_pri6;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 6
	 */
	uint64_t rx_pfc_ena_frames_pri7;
	/*
	 * Total number of received PFC frames with PFC enabled bit for
	 * Pri 7
	 */
	uint64_t rx_sch_crc_err_frames;
	/* Total Number of frames received with SCH CRC error */
	uint64_t rx_undrsz_frames;
	/* Total Number of under-sized frames received */
	uint64_t rx_frag_frames;
	/* Total Number of fragmented frames received */
	uint64_t rx_eee_lpi_events;
	/* Total number of RX EEE LPI Events */
	uint64_t rx_eee_lpi_duration;
	/* EEE LPI Duration Counter on RX */
	uint64_t rx_llfc_physical_msgs;
	/*
	 * Total number of physical type Link Level Flow Control	(LLFC)
	 * messages received
	 */
	uint64_t rx_llfc_logical_msgs;
	/*
	 * Total number of logical type Link Level Flow Control	(LLFC)
	 * messages received
	 */
	uint64_t rx_llfc_msgs_with_crc_err;
	/*
	 * Total number of logical type Link Level Flow Control	(LLFC)
	 * messages received with CRC error
	 */
	uint64_t rx_hcfc_msgs;
	/* Total number of HCFC messages received */
	uint64_t rx_hcfc_msgs_with_crc_err;
	/* Total number of HCFC messages received with CRC error */
	uint64_t rx_bytes;
	/* Total number of received bytes */
	uint64_t rx_runt_bytes;
	/* Total number of bytes received in runt frames */
	uint64_t rx_runt_frames;
	/* Total number of runt frames received */
	uint64_t rx_stat_discard;
	/* Total Rx Discards per Port reported by STATS block */
	uint64_t rx_stat_err;
	/* Total Rx Error Drops per Port reported by STATS block */
} __attribute__((packed));

/* Periodic Statistics Context DMA to host	(160 bytes) */
/*
 * per-context HW statistics -- chip view
 */

struct ctx_hw_stats64 {
	uint64_t rx_ucast_pkts;
	uint64_t rx_mcast_pkts;
	uint64_t rx_bcast_pkts;
	uint64_t rx_drop_pkts;
	uint64_t rx_discard_pkts;
	uint64_t rx_ucast_bytes;
	uint64_t rx_mcast_bytes;
	uint64_t rx_bcast_bytes;

	uint64_t tx_ucast_pkts;
	uint64_t tx_mcast_pkts;
	uint64_t tx_bcast_pkts;
	uint64_t tx_drop_pkts;
	uint64_t tx_discard_pkts;
	uint64_t tx_ucast_bytes;
	uint64_t tx_mcast_bytes;
	uint64_t tx_bcast_bytes;

	uint64_t tpa_pkts;
	uint64_t tpa_bytes;
	uint64_t tpa_events;
	uint64_t tpa_aborts;
} __attribute__((packed));

#endif /* _HSI_STRUCT_DEF_DPDK_ */
