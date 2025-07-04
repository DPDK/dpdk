/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */
/*
 * Copyright (C) 2019 Intel Corporation
 *
 * For licensing information, see the file 'LICENSE' in the root folder
 */
#ifndef _VIRTCHNL2_LAN_DESC_H_
#define _VIRTCHNL2_LAN_DESC_H_

/* VIRTCHNL2_TX_DESC_IDS
 * Transmit descriptor ID flags
 */
enum virtchnl2_tx_desc_ids {
	VIRTCHNL2_TXDID_DATA				= BIT(0),
	VIRTCHNL2_TXDID_CTX				= BIT(1),
	VIRTCHNL2_TXDID_REINJECT_CTX			= BIT(2),
	VIRTCHNL2_TXDID_FLEX_DATA			= BIT(3),
	VIRTCHNL2_TXDID_FLEX_CTX			= BIT(4),
	VIRTCHNL2_TXDID_FLEX_TSO_CTX			= BIT(5),
	VIRTCHNL2_TXDID_FLEX_TSYN_L2TAG1		= BIT(6),
	VIRTCHNL2_TXDID_FLEX_L2TAG1_L2TAG2		= BIT(7),
	VIRTCHNL2_TXDID_FLEX_TSO_L2TAG2_PARSTAG_CTX	= BIT(8),
	VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_SA_TSO_CTX	= BIT(9),
	VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_SA_CTX		= BIT(10),
	VIRTCHNL2_TXDID_FLEX_L2TAG2_CTX			= BIT(11),
	VIRTCHNL2_TXDID_FLEX_FLOW_SCHED			= BIT(12),
	VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_TSO_CTX		= BIT(13),
	VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_CTX		= BIT(14),
	VIRTCHNL2_TXDID_DESC_DONE			= BIT(15),
};

/**
 * VIRTCHNL2_RX_DESC_IDS
 * Receive descriptor IDs (range from 0 to 63)
 */
enum virtchnl2_rx_desc_ids {
	VIRTCHNL2_RXDID_0_16B_BASE,
	VIRTCHNL2_RXDID_1_32B_BASE,
	/* FLEX_SQ_NIC and FLEX_SPLITQ share desc ids because they can be
	 * differentiated based on queue model; e.g. single queue model can
	 * only use FLEX_SQ_NIC and split queue model can only use FLEX_SPLITQ
	 * for DID 2.
	 */
	VIRTCHNL2_RXDID_2_FLEX_SPLITQ		= 2,
	VIRTCHNL2_RXDID_2_FLEX_SQ_NIC		= VIRTCHNL2_RXDID_2_FLEX_SPLITQ,
	VIRTCHNL2_RXDID_3_FLEX_SQ_SW		= 3,
	VIRTCHNL2_RXDID_4_FLEX_SQ_NIC_VEB	= 4,
	VIRTCHNL2_RXDID_5_FLEX_SQ_NIC_ACL	= 5,
	VIRTCHNL2_RXDID_6_FLEX_SQ_NIC_2		= 6,
	VIRTCHNL2_RXDID_7_HW_RSVD		= 7,
	/* 9 through 15 are reserved */
	VIRTCHNL2_RXDID_16_COMMS_GENERIC	= 16,
	VIRTCHNL2_RXDID_17_COMMS_AUX_VLAN	= 17,
	VIRTCHNL2_RXDID_18_COMMS_AUX_IPV4	= 18,
	VIRTCHNL2_RXDID_19_COMMS_AUX_IPV6	= 19,
	VIRTCHNL2_RXDID_20_COMMS_AUX_FLOW	= 20,
	VIRTCHNL2_RXDID_21_COMMS_AUX_TCP	= 21,
	/* 22 through 63 are reserved */
};

/**
 * VIRTCHNL2_RX_DESC_ID_BITMASKS
 * Receive descriptor ID bitmasks
 */
#define VIRTCHNL2_RXDID_M(bit)			BIT_ULL(VIRTCHNL2_RXDID_##bit)

enum virtchnl2_rx_desc_id_bitmasks {
	VIRTCHNL2_RXDID_0_16B_BASE_M		= VIRTCHNL2_RXDID_M(0_16B_BASE),
	VIRTCHNL2_RXDID_1_32B_BASE_M		= VIRTCHNL2_RXDID_M(1_32B_BASE),
	VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M		= VIRTCHNL2_RXDID_M(2_FLEX_SPLITQ),
	VIRTCHNL2_RXDID_2_FLEX_SQ_NIC_M		= VIRTCHNL2_RXDID_M(2_FLEX_SQ_NIC),
	VIRTCHNL2_RXDID_3_FLEX_SQ_SW_M		= VIRTCHNL2_RXDID_M(3_FLEX_SQ_SW),
	VIRTCHNL2_RXDID_4_FLEX_SQ_NIC_VEB_M	= VIRTCHNL2_RXDID_M(4_FLEX_SQ_NIC_VEB),
	VIRTCHNL2_RXDID_5_FLEX_SQ_NIC_ACL_M	= VIRTCHNL2_RXDID_M(5_FLEX_SQ_NIC_ACL),
	VIRTCHNL2_RXDID_6_FLEX_SQ_NIC_2_M	= VIRTCHNL2_RXDID_M(6_FLEX_SQ_NIC_2),
	VIRTCHNL2_RXDID_7_HW_RSVD_M		= VIRTCHNL2_RXDID_M(7_HW_RSVD),
	/* 9 through 15 are reserved */
	VIRTCHNL2_RXDID_16_COMMS_GENERIC_M	= VIRTCHNL2_RXDID_M(16_COMMS_GENERIC),
	VIRTCHNL2_RXDID_17_COMMS_AUX_VLAN_M	= VIRTCHNL2_RXDID_M(17_COMMS_AUX_VLAN),
	VIRTCHNL2_RXDID_18_COMMS_AUX_IPV4_M	= VIRTCHNL2_RXDID_M(18_COMMS_AUX_IPV4),
	VIRTCHNL2_RXDID_19_COMMS_AUX_IPV6_M	= VIRTCHNL2_RXDID_M(19_COMMS_AUX_IPV6),
	VIRTCHNL2_RXDID_20_COMMS_AUX_FLOW_M	= VIRTCHNL2_RXDID_M(20_COMMS_AUX_FLOW),
	VIRTCHNL2_RXDID_21_COMMS_AUX_TCP_M	= VIRTCHNL2_RXDID_M(21_COMMS_AUX_TCP),
	/* 22 through 63 are reserved */
};

/* For splitq virtchnl2_rx_flex_desc_adv desc members */
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RXDID_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RXDID_M		GENMASK(3, 0)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_UMBCAST_S		6
#define VIRTCHNL2_RX_FLEX_DESC_ADV_UMBCAST_M		GENMASK(7, 6)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_M		GENMASK(9, 0)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF0_S		12
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF0_M		GENMASK(15, 13)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_M		GENMASK(13, 0)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_S		14
#define VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_S		15
#define VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_HDR_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_HDR_M		GENMASK(9, 0)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RSC_S		10
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RSC_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_RSC_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_SPH_S		11
#define VIRTCHNL2_RX_FLEX_DESC_ADV_SPH_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_SPH_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF1_S		12
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF1_M		GENMASK(14, 12)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_MISS_S		15
#define VIRTCHNL2_RX_FLEX_DESC_ADV_MISS_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_MISS_S)

/**
 * VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS_ERROR_0_QW1_BITS
 * For splitq virtchnl2_rx_flex_desc_adv
 * Note: These are predefined bit offsets
 */
enum virtchl2_rx_flex_desc_adv_status_error_0_qw1_bits {
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_DD_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_EOF_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_HBO_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_L3L4P_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_IPE_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_L4E_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EIPE_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EUDPE_S,
};

/**
 * VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS_ERROR_0_QW0_BITS
 * For splitq virtchnl2_rx_flex_desc_adv
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_flex_desc_adv_status_error_0_qw0_bits {
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_LPBK_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_IPV6EXADD_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_RXE_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_CRCP_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_RSS_VALID_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_L2TAG1P_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XTRMD0_VALID_S,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XTRMD1_VALID_S,
	/* this entry must be last!!! */
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_LAST,
};

/**
 * VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS_ERROR_1_BITS
 * For splitq virtchnl2_rx_flex_desc_adv
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_flex_desc_adv_status_error_1_bits {
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_RSVD_S		= 0,
	/* 2 bits */
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_ATRAEFAIL_S		= 2,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_L2TAG2P_S		= 3,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD2_VALID_S	= 4,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD3_VALID_S	= 5,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD4_VALID_S	= 6,
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD5_VALID_S	= 7,
	/* this entry must be last!!! */
	VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_LAST			= 8,
};

/* for singleq (flex) virtchnl2_rx_flex_desc fields
 * for virtchnl2_rx_flex_desc.ptype_flex_flags0 member
 */
#define VIRTCHNL2_RX_FLEX_DESC_PTYPE_S			0
#define VIRTCHNL2_RX_FLEX_DESC_PTYPE_M			GENMASK(9, 0)

/* For virtchnl2_rx_flex_desc.pkt_len member */
#define VIRTCHNL2_RX_FLEX_DESC_PKT_LEN_S		0
#define VIRTCHNL2_RX_FLEX_DESC_PKT_LEN_M		GENMASK(13, 0)

/**
 * VIRTCHNL2_RX_FLEX_DESC_STATUS_ERROR_0_BITS
 * For singleq (flex) virtchnl2_rx_flex_desc
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_flex_desc_status_error_0_bits {
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_DD_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_EOF_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_HBO_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_L3L4P_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_IPE_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_L4E_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_LPBK_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_IPV6EXADD_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_RXE_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_CRCP_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_RSS_VALID_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_L2TAG1P_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_XTRMD0_VALID_S,
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_XTRMD1_VALID_S,
	/* this entry must be last!!! */
	VIRTCHNL2_RX_FLEX_DESC_STATUS0_LAST,
};

/**
 * VIRTCHNL2_RX_FLEX_DESC_STATUS_ERROR_1_BITS
 * For singleq (flex) virtchnl2_rx_flex_desc
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_flex_desc_status_error_1_bits {
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_CPM_S			= 0,
	/* 4 bits */
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_NAT_S			= 4,
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_CRYPTO_S			= 5,
	/* [10:6] reserved */
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_L2TAG2P_S		= 11,
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD2_VALID_S		= 12,
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD3_VALID_S		= 13,
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD4_VALID_S		= 14,
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD5_VALID_S		= 15,
	/* this entry must be last!!! */
	VIRTCHNL2_RX_FLEX_DESC_STATUS1_LAST			= 16,
};

/* For virtchnl2_rx_flex_desc.ts_low member */
#define VIRTCHNL2_RX_FLEX_TSTAMP_VALID				BIT(0)

/* For singleq (non flex) virtchnl2_singleq_base_rx_desc legacy desc members */
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_SPH_S	63
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_SPH_M	\
	BIT_ULL(VIRTCHNL2_RX_BASE_DESC_QW1_LEN_SPH_S)
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_HBUF_S	52
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_HBUF_M	GENMASK_ULL(62, 52)
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_PBUF_S	38
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_PBUF_M	GENMASK_ULL(51, 38)
#define VIRTCHNL2_RX_BASE_DESC_QW1_PTYPE_S	30
#define VIRTCHNL2_RX_BASE_DESC_QW1_PTYPE_M	GENMASK_ULL(37, 30)
#define VIRTCHNL2_RX_BASE_DESC_QW1_ERROR_S	19
#define VIRTCHNL2_RX_BASE_DESC_QW1_ERROR_M	GENMASK_ULL(26, 19)
#define VIRTCHNL2_RX_BASE_DESC_QW1_STATUS_S	0
#define VIRTCHNL2_RX_BASE_DESC_QW1_STATUS_M	GENMASK_ULL(18, 0)

/**
 * VIRTCHNL2_RX_BASE_DESC_STATUS_BITS
 * For singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_base_desc_status_bits {
	VIRTCHNL2_RX_BASE_DESC_STATUS_DD_S		= 0,
	VIRTCHNL2_RX_BASE_DESC_STATUS_EOF_S		= 1,
	VIRTCHNL2_RX_BASE_DESC_STATUS_L2TAG1P_S		= 2,
	VIRTCHNL2_RX_BASE_DESC_STATUS_L3L4P_S		= 3,
	VIRTCHNL2_RX_BASE_DESC_STATUS_CRCP_S		= 4,
	VIRTCHNL2_RX_BASE_DESC_STATUS_RSVD_S		= 5, /* 3 bits */
	VIRTCHNL2_RX_BASE_DESC_STATUS_EXT_UDP_0_S	= 8,
	VIRTCHNL2_RX_BASE_DESC_STATUS_UMBCAST_S		= 9, /* 2 bits */
	VIRTCHNL2_RX_BASE_DESC_STATUS_FLM_S		= 11,
	VIRTCHNL2_RX_BASE_DESC_STATUS_FLTSTAT_S		= 12, /* 2 bits */
	VIRTCHNL2_RX_BASE_DESC_STATUS_LPBK_S		= 14,
	VIRTCHNL2_RX_BASE_DESC_STATUS_IPV6EXADD_S	= 15,
	VIRTCHNL2_RX_BASE_DESC_STATUS_RSVD1_S		= 16, /* 2 bits */
	VIRTCHNL2_RX_BASE_DESC_STATUS_INT_UDP_0_S	= 18,
	VIRTCHNL2_RX_BASE_DESC_STATUS_LAST		= 19, /* this entry must be last!!! */
};

/**
 * VIRTCHNL2_RX_BASE_DESC_EXT_STATUS_BITS
 * For singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
enum virtcnl2_rx_base_desc_status_bits {
	VIRTCHNL2_RX_BASE_DESC_EXT_STATUS_L2TAG2P_S,
};

/**
 * VIRTCHNL2_RX_BASE_DESC_ERROR_BITS
 * For singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_base_desc_error_bits {
	VIRTCHNL2_RX_BASE_DESC_ERROR_RXE_S		= 0,
	VIRTCHNL2_RX_BASE_DESC_ERROR_ATRAEFAIL_S	= 1,
	VIRTCHNL2_RX_BASE_DESC_ERROR_HBO_S		= 2,
	VIRTCHNL2_RX_BASE_DESC_ERROR_L3L4E_S		= 3, /* 3 bits */
	VIRTCHNL2_RX_BASE_DESC_ERROR_IPE_S		= 3,
	VIRTCHNL2_RX_BASE_DESC_ERROR_L4E_S		= 4,
	VIRTCHNL2_RX_BASE_DESC_ERROR_EIPE_S		= 5,
	VIRTCHNL2_RX_BASE_DESC_ERROR_OVERSIZE_S		= 6,
	VIRTCHNL2_RX_BASE_DESC_ERROR_PPRS_S		= 7,
};

/**
 * VIRTCHNL2_RX_BASE_DESC_FLTSTAT_VALUES
 * For singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
enum virtchnl2_rx_base_desc_flstat_values {
	VIRTCHNL2_RX_BASE_DESC_FLTSTAT_NO_DATA,
	VIRTCHNL2_RX_BASE_DESC_FLTSTAT_FD_ID,
	VIRTCHNL2_RX_BASE_DESC_FLTSTAT_RSV,
	VIRTCHNL2_RX_BASE_DESC_FLTSTAT_RSS_HASH,
};

/**
 * struct virtchnl2_splitq_rx_buf_desc - SplitQ RX buffer descriptor format
 * @qword0: RX buffer struct
 * @qword0.buf_id: Buffer identifier
 * @qword0.rsvd0: Reserved
 * @qword0.rsvd1: Reserved
 * @pkt_addr: Packet buffer address
 * @hdr_addr: Header buffer address
 * @rsvd2: Reserved
 *
 * Receive Descriptors
 * SplitQ buffer
 * |                                       16|                   0|
 * ----------------------------------------------------------------
 * | RSV                                     | Buffer ID          |
 * ----------------------------------------------------------------
 * | Rx packet buffer address                                     |
 * ----------------------------------------------------------------
 * | Rx header buffer address                                     |
 * ----------------------------------------------------------------
 * | RSV                                                          |
 * ----------------------------------------------------------------
 * |                                                             0|
 */
struct virtchnl2_splitq_rx_buf_desc {
	struct {
		__le16  buf_id;
		__le16  rsvd0;
		__le32  rsvd1;
	} qword0;
	__le64  pkt_addr;
	__le64  hdr_addr;
	__le64  rsvd2;
};

/**
 * struct virtchnl2_singleq_rx_buf_desc - SingleQ RX buffer descriptor format
 * @pkt_addr: Packet buffer address
 * @hdr_addr: Header buffer address
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 *
 * SingleQ buffer
 * |                                                             0|
 * ----------------------------------------------------------------
 * | Rx packet buffer address                                     |
 * ----------------------------------------------------------------
 * | Rx header buffer address                                     |
 * ----------------------------------------------------------------
 * | RSV                                                          |
 * ----------------------------------------------------------------
 * | RSV                                                          |
 * ----------------------------------------------------------------
 * |                                                             0|
 */
struct virtchnl2_singleq_rx_buf_desc {
	__le64  pkt_addr;
	__le64  hdr_addr;
	__le64  rsvd1;
	__le64  rsvd2;
};

/**
 * union virtchnl2_rx_buf_desc - RX buffer descriptor
 * @read: Singleq RX buffer descriptor format
 * @split_rd: Splitq RX buffer descriptor format
 */
union virtchnl2_rx_buf_desc {
	struct virtchnl2_singleq_rx_buf_desc		read;
	struct virtchnl2_splitq_rx_buf_desc		split_rd;
};

/**
 * struct virtchnl2_singleq_base_rx_desc - RX descriptor writeback format
 * @qword0: First quad word struct
 * @qword0.lo_dword: Lower dual word struct
 * @qword0.lo_dword.mirroring_status: Mirrored packet status
 * @qword0.lo_dword.l2tag1: Stripped L2 tag from the received packet
 * @qword0.hi_dword: High dual word union
 * @qword0.hi_dword.rss: RSS hash
 * @qword0.hi_dword.fd_id: Flow director filter id
 * @qword1: Second quad word struct
 * @qword1.status_error_ptype_len: Status/error/PTYPE/length
 * @qword2: Third quad word struct
 * @qword2.ext_status: Extended status
 * @qword2.rsvd: Reserved
 * @qword2.l2tag2_1: Extracted L2 tag 2 from the packet
 * @qword2.l2tag2_2: Reserved
 * @qword3: Fourth quad word struct
 * @qword3.reserved: Reserved
 * @qword3.fd_id: Flow director filter id
 *
 * Profile ID 0x1, SingleQ, base writeback format.
 */
struct virtchnl2_singleq_base_rx_desc {
	struct {
		struct {
			__le16 mirroring_status;
			__le16 l2tag1;
		} lo_dword;
		union {
			__le32 rss;
			__le32 fd_id;
		} hi_dword;
	} qword0;
	struct {
		__le64 status_error_ptype_len;
	} qword1;
	struct {
		__le16 ext_status;
		__le16 rsvd;
		__le16 l2tag2_1;
		__le16 l2tag2_2;
	} qword2;
	struct {
		__le32 reserved;
		__le32 fd_id;
	} qword3;
};

/**
 * struct virtchnl2_rx_flex_desc - RX descriptor writeback format
 * @rxdid: Descriptor builder profile id
 * @mir_id_umb_cast: umb_cast=[7:6], mirror=[5:0]
 * @ptype_flex_flags0: ff0=[15:10], ptype=[9:0]
 * @pkt_len: Packet length, [15:14] are reserved
 * @hdr_len_sph_flex_flags1: ff1/ext=[15:12], sph=[11], header=[10:0]
 * @status_error0: Status/Error section 0
 * @l2tag1: Stripped L2 tag from the received packet
 * @flex_meta0: Flexible metadata container 0
 * @flex_meta1: Flexible metadata container 1
 * @status_error1: Status/Error section 1
 * @flex_flags2: Flexible flags section 2
 * @time_stamp_low: Lower word of timestamp value
 * @l2tag2_1st: First L2TAG2
 * @l2tag2_2nd: Second L2TAG2
 * @flex_meta2: Flexible metadata container 2
 * @flex_meta3: Flexible metadata container 3
 * @flex_ts: Timestamp and flexible flow id union
 * @flex_ts.flex.flex_meta4: Flexible metadata container 4
 * @flex_ts.flex.flex_meta5: Flexible metadata container 5
 * @flex_ts.ts_high: Timestamp higher word of the timestamp value
 *
 * Profile ID 0x1, SingleQ, flex completion writeback format.
 */
struct virtchnl2_rx_flex_desc {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flex_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;
	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 flex_meta0;
	__le16 flex_meta1;

	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 time_stamp_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le16 flex_meta2;
	__le16 flex_meta3;
	union {
		struct {
			__le16 flex_meta4;
			__le16 flex_meta5;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/**
 * struct virtchnl2_rx_flex_desc_nic - RX descriptor writeback format
 * @rxdid: Descriptor builder profile id
 * @mir_id_umb_cast: umb_cast=[7:6], mirror=[5:0]
 * @ptype_flex_flags0: ff0=[15:10], ptype=[9:0]
 * @pkt_len: Packet length, [15:14] are reserved
 * @hdr_len_sph_flex_flags1: ff1/ext=[15:12], sph=[11], header=[10:0]
 * @status_error0: Status/Error section 0
 * @l2tag1: Stripped L2 tag from the received packet
 * @rss_hash: RSS hash
 * @status_error1: Status/Error section 1
 * @flexi_flags2: Flexible flags section 2
 * @ts_low: Lower word of timestamp value
 * @l2tag2_1st: First L2TAG2
 * @l2tag2_2nd: Second L2TAG2
 * @flow_id: Flow id
 * @flex_ts: Timestamp and flexible flow id union
 * @flex_ts.flex.rsvd: Reserved
 * @flex_ts.flex.flow_id_ipv6: IPv6 flow id
 * @flex_ts.ts_high: Timestamp higher word of the timestamp value
 *
 * Profile ID 0x2, SingleQ, flex writeback format.
 */
struct virtchnl2_rx_flex_desc_nic {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flex_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 rss_hash;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 flow_id;
	union {
		struct {
			__le16 rsvd;
			__le16 flow_id_ipv6;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/**
 * struct virtchnl2_rx_flex_desc_sw - RX descriptor writeback format
 * @rxdid: Descriptor builder profile id
 * @mir_id_umb_cast: umb_cast=[7:6], mirror=[5:0]
 * @ptype_flex_flags0: ff0=[15:10], ptype=[9:0]
 * @pkt_len: Packet length, [15:14] are reserved
 * @hdr_len_sph_flex_flags1: ff1/ext=[15:12], sph=[11], header=[10:0]
 * @status_error0: Status/Error section 0
 * @l2tag1: Stripped L2 tag from the received packet
 * @src_vsi: Source VSI, [10:15] are reserved
 * @flex_md1_rsvd: Flexible metadata container 1
 * @status_error1: Status/Error section 1
 * @flex_flags2: Flexible flags section 2
 * @ts_low: Lower word of timestamp value
 * @l2tag2_1st: First L2TAG2
 * @l2tag2_2nd: Second L2TAG2
 * @rsvd: Reserved
 * @ts_high: Timestamp higher word of the timestamp value
 *
 * Rx Flex Descriptor Switch Profile
 * RxDID Profile ID 0x3, SingleQ
 * Flex-field 0: Source Vsi
 */
struct virtchnl2_rx_flex_desc_sw {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flex_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 src_vsi;
	__le16 flex_md1_rsvd;
	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;
	/* Qword 3 */
	__le32 rsvd;
	__le32 ts_high;
};

#ifndef EXTERNAL_RELEASE
/**
 * struct virtchnl2_rx_flex_desc_nic_veb_dbg - RX descriptor writeback format
 * @rxdid: Descriptor builder profile id
 * @mir_id_umb_cast: umb_cast=[7:6], mirror=[5:0]
 * @ptype_flex_flags0: ff0=[15:10], ptype=[9:0]
 * @pkt_len: Packet length, [15:14] are reserved
 * @hdr_len_sph_flex_flags1: ff1/ext=[15:12], sph=[11], header=[10:0]
 * @status_error0: Status/Error section 0
 * @l2tag1: Stripped L2 tag from the received packet
 * @dst_vsi: Destination VSI, [10:15] are reserved
 * @flex_field_1: Flexible metadata container 1
 * @status_error1: Status/Error section 1
 * @flex_flags2: Flexible flags section 2
 * @ts_low: Lower word of timestamp value
 * @l2tag2_1st: First L2TAG2
 * @l2tag2_2nd: Second L2TAG2
 * @rsvd: Flex words 2-3 are reserved
 * @ts_high: Timestamp higher word of the timestamp value
 *
 * Rx Flex Descriptor NIC VEB Profile
 * RxDID Profile Id 0x4
 * Flex-field 0: Destination Vsi
 */
struct virtchnl2_rx_flex_desc_nic_veb_dbg {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flex_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;
	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 dst_vsi;
	__le16 flex_field_1;
	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 rsvd;
	__le32 ts_high;
};

/**
 * struct virtchnl2_rx_flex_desc_nic_acl_dbg - RX descriptor writeback format
 * @rxdid: Descriptor builder profile id
 * @mir_id_umb_cast: umb_cast=[7:6], mirror=[5:0]
 * @ptype_flex_flags0: ff0=[15:10], ptype=[9:0]
 * @pkt_len: Packet length, [15:14] are reserved
 * @hdr_len_sph_flex_flags1: ff1/ext=[15:12], sph=[11], header=[10:0]
 * @status_error0: Status/Error section 0
 * @l2tag1: Stripped L2 tag from the received packet
 * @acl_ctr0: ACL counter 0
 * @acl_ctr1: ACL counter 1
 * @status_error1: Status/Error section 1
 * @flex_flags2: Flexible flags section 2
 * @ts_low: Lower word of timestamp value
 * @l2tag2_1st: First L2TAG2
 * @l2tag2_2nd: Second L2TAG2
 * @acl_ctr2: ACL counter 2
 * @rsvd: Flex words 2-3 are reserved
 * @ts_high: Timestamp higher word of the timestamp value
 *
 * Rx Flex Descriptor NIC ACL Profile
 * RxDID Profile ID 0x5
 * Flex-field 0: ACL Counter 0
 * Flex-field 1: ACL Counter 1
 * Flex-field 2: ACL Counter 2
 */
struct virtchnl2_rx_flex_desc_nic_acl_dbg {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flex_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;
	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le16 acl_ctr0;
	__le16 acl_ctr1;
	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;
	/* Qword 3 */
	__le16 acl_ctr2;
	__le16 rsvd;
	__le32 ts_high;
};
#endif /* !EXTERNAL_RELEASE */

/**
 * struct virtchnl2_rx_flex_desc_nic_2 - RX descriptor writeback format
 * @rxdid: Descriptor builder profile id
 * @mir_id_umb_cast: umb_cast=[7:6], mirror=[5:0]
 * @ptype_flex_flags0: ff0=[15:10], ptype=[9:0]
 * @pkt_len: Packet length, [15:14] are reserved
 * @hdr_len_sph_flex_flags1: ff1/ext=[15:12], sph=[11], header=[10:0]
 * @status_error0: Status/Error section 0
 * @l2tag1: Stripped L2 tag from the received packet
 * @rss_hash: RSS hash
 * @status_error1: Status/Error section 1
 * @flexi_flags2: Flexible flags section 2
 * @ts_low: Lower word of timestamp value
 * @l2tag2_1st: First L2TAG2
 * @l2tag2_2nd: Second L2TAG2
 * @flow_id: Flow id
 * @src_vsi: Source VSI
 * @flex_ts: Timestamp and flexible flow id union
 * @flex_ts.flex.rsvd: Reserved
 * @flex_ts.flex.flow_id_ipv6: IPv6 flow id
 * @flex_ts.ts_high: Timestamp higher word of the timestamp value
 *
 * Rx Flex Descriptor NIC Profile
 * RxDID Profile ID 0x6
 * Flex-field 0: RSS hash lower 16-bits
 * Flex-field 1: RSS hash upper 16-bits
 * Flex-field 2: Flow Id lower 16-bits
 * Flex-field 3: Source Vsi
 * Flex-field 4: reserved, Vlan id taken from L2Tag
 */
struct virtchnl2_rx_flex_desc_nic_2 {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flex_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 rss_hash;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le16 flow_id;
	__le16 src_vsi;
	union {
		struct {
			__le16 rsvd;
			__le16 flow_id_ipv6;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

/**
 * struct virtchnl2_rx_flex_desc_adv - RX descriptor writeback format
 * @rxdid_ucast: ucast=[7:6], rsvd=[5:4], profile_id=[3:0]
 * @status_err0_qw0: Status/Error section 0 in quad word 0
 * @ptype_err_fflags0: ff0=[15:12], udp_len_err=[11], ip_hdr_err=[10],
 *		       ptype=[9:0]
 * @pktlen_gen_bufq_id: bufq_id=[15] only in splitq, gen=[14] only in splitq,
 *			plen=[13:0]
 * @hdrlen_flags: miss_prepend=[15], trunc_mirr=[14], int_udp_0=[13],
 *		  ext_udp0=[12], sph=[11] only in splitq, rsc=[10]
 *		  only in splitq, header=[9:0]
 * @status_err0_qw1: Status/Error section 0 in quad word 1
 * @status_err1: Status/Error section 1
 * @fflags1: Flexible flags section 1
 * @ts_low: Lower word of timestamp value
 * @fmd0: Flexible metadata container 0
 * @fmd1: Flexible metadata container 1
 * @fmd2: Flexible metadata container 2
 * @fflags2: Flags
 * @hash3: Upper bits of Rx hash value
 * @fmd3: Flexible metadata container 3
 * @fmd4: Flexible metadata container 4
 * @fmd5: Flexible metadata container 5
 * @fmd6: Flexible metadata container 6
 * @fmd7_0: Flexible metadata container 7.0
 * @fmd7_1: Flexible metadata container 7.1
 *
 * RX Flex Descriptor Advanced (Split Queue Model)
 * RxDID Profile ID 0x2
 */
struct virtchnl2_rx_flex_desc_adv {
	/* Qword 0 */
	u8 rxdid_ucast;
	u8 status_err0_qw0;
	__le16 ptype_err_fflags0;
	__le16 pktlen_gen_bufq_id;
	__le16 hdrlen_flags;
	/* Qword 1 */
	u8 status_err0_qw1;
	u8 status_err1;
	u8 fflags1;
	u8 ts_low;
	__le16 fmd0;
	__le16 fmd1;
	/* Qword 2 */
	__le16 fmd2;
	u8 fflags2;
	u8 hash3;
	__le16 fmd3;
	__le16 fmd4;
	/* Qword 3 */
	__le16 fmd5;
	__le16 fmd6;
	__le16 fmd7_0;
	__le16 fmd7_1;
};

/**
 * struct virtchnl2_rx_flex_desc_adv_nic_3 - RX descriptor writeback format
 * @rxdid_ucast: ucast=[7:6], rsvd=[5:4], profile_id=[3:0]
 * @status_err0_qw0: Status/Error section 0 in quad word 0
 * @ptype_err_fflags0: ff0=[15:12], udp_len_err=[11], ip_hdr_err=[10],
 *		       ptype=[9:0]
 * @pktlen_gen_bufq_id: bufq_id=[15] only in splitq, gen=[14] only in splitq,
 *			plen=[13:0]
 * @hdrlen_flags: miss_prepend=[15], trunc_mirr=[14], int_udp_0=[13],
 *		  ext_udp0=[12], sph=[11] only in splitq, rsc=[10]
 *		  only in splitq, header=[9:0]
 * @status_err0_qw1: Status/Error section 0 in quad word 1
 * @status_err1: Status/Error section 1
 * @fflags1: Flexible flags section 1
 * @ts_low: Lower word of timestamp value
 * @buf_id: Buffer identifier. Only in splitq mode.
 * @misc: Union
 * @misc.raw_cs: Raw checksum
 * @misc.l2tag1: Stripped L2 tag from the received packet
 * @misc.rscseglen: RSC segment length
 * @hash1: Lower 16 bits of Rx hash value, hash[15:0]
 * @ff2_mirrid_hash2: Union
 * @ff2_mirrid_hash2.fflags2: Flexible flags section 2
 * @ff2_mirrid_hash2.mirrorid: Mirror id
 * @ff2_mirrid_hash2.hash2: 8 bits of Rx hash value, hash[23:16]
 * @hash3: Upper 8 bits of Rx hash value, hash[31:24]
 * @l2tag2: Extracted L2 tag 2 from the packet
 * @fmd4: Flexible metadata container 4
 * @l2tag1: Stripped L2 tag from the received packet
 * @fmd6: Flexible metadata container 6
 * @ts_high: Timestamp higher word of the timestamp value
 *
 * Profile ID 0x2, SplitQ, flex writeback format.
 *
 * Flex-field 0: BufferID
 * Flex-field 1: Raw checksum/L2TAG1/RSC Seg Len (determined by HW)
 * Flex-field 2: Hash[15:0]
 * Flex-flags 2: Hash[23:16]
 * Flex-field 3: L2TAG2
 * Flex-field 5: L2TAG1
 * Flex-field 7: Timestamp (upper 32 bits)
 */
struct virtchnl2_rx_flex_desc_adv_nic_3 {
	/* Qword 0 */
	u8 rxdid_ucast;
	u8 status_err0_qw0;
	__le16 ptype_err_fflags0;
	__le16 pktlen_gen_bufq_id;
	__le16 hdrlen_flags;
	/* Qword 1 */
	u8 status_err0_qw1;
	u8 status_err1;
	u8 fflags1;
	u8 ts_low;
	__le16 buf_id;
	union {
		__le16 raw_cs;
		__le16 l2tag1;
		__le16 rscseglen;
	} misc;
	/* Qword 2 */
	__le16 hash1;
	union {
		u8 fflags2;
		u8 mirrorid;
		u8 hash2;
	} ff2_mirrid_hash2;
	u8 hash3;
	__le16 l2tag2;
	__le16 fmd4;
	/* Qword 3 */
	__le16 l2tag1;
	__le16 fmd6;
	__le32 ts_high;
};

union virtchnl2_rx_desc {
	struct virtchnl2_singleq_rx_buf_desc		read;
	struct virtchnl2_singleq_base_rx_desc		base_wb;
	struct virtchnl2_rx_flex_desc			flex_wb;
	struct virtchnl2_rx_flex_desc_nic		flex_nic_wb;
	struct virtchnl2_rx_flex_desc_sw		flex_sw_wb;
	struct virtchnl2_rx_flex_desc_nic_2		flex_nic_2_wb;
	struct virtchnl2_rx_flex_desc_adv		flex_adv_wb;
	struct virtchnl2_rx_flex_desc_adv_nic_3		flex_adv_nic_3_wb;
};

#endif /* _VIRTCHNL_LAN_DESC_H_ */
