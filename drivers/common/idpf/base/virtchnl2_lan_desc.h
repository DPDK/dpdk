/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
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
#define VIRTCHNL2_TXDID_DATA				BIT(0)
#define VIRTCHNL2_TXDID_CTX				BIT(1)
#define VIRTCHNL2_TXDID_REINJECT_CTX			BIT(2)
#define VIRTCHNL2_TXDID_FLEX_DATA			BIT(3)
#define VIRTCHNL2_TXDID_FLEX_CTX			BIT(4)
#define VIRTCHNL2_TXDID_FLEX_TSO_CTX			BIT(5)
#define VIRTCHNL2_TXDID_FLEX_TSYN_L2TAG1		BIT(6)
#define VIRTCHNL2_TXDID_FLEX_L2TAG1_L2TAG2		BIT(7)
#define VIRTCHNL2_TXDID_FLEX_TSO_L2TAG2_PARSTAG_CTX	BIT(8)
#define VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_SA_TSO_CTX	BIT(9)
#define VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_SA_CTX		BIT(10)
#define VIRTCHNL2_TXDID_FLEX_L2TAG2_CTX			BIT(11)
#define VIRTCHNL2_TXDID_FLEX_FLOW_SCHED			BIT(12)
#define VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_TSO_CTX		BIT(13)
#define VIRTCHNL2_TXDID_FLEX_HOSTSPLIT_CTX		BIT(14)
#define VIRTCHNL2_TXDID_DESC_DONE			BIT(15)

/* VIRTCHNL2_RX_DESC_IDS
 * Receive descriptor IDs (range from 0 to 63)
 */
#define VIRTCHNL2_RXDID_0_16B_BASE			0
#define VIRTCHNL2_RXDID_1_32B_BASE			1
/* FLEX_SQ_NIC and FLEX_SPLITQ share desc ids because they can be
 * differentiated based on queue model; e.g. single queue model can
 * only use FLEX_SQ_NIC and split queue model can only use FLEX_SPLITQ
 * for DID 2.
 */
#define VIRTCHNL2_RXDID_2_FLEX_SPLITQ			2
#define VIRTCHNL2_RXDID_2_FLEX_SQ_NIC			2
#define VIRTCHNL2_RXDID_3_FLEX_SQ_SW			3
#define VIRTCHNL2_RXDID_4_FLEX_SQ_NIC_VEB		4
#define VIRTCHNL2_RXDID_5_FLEX_SQ_NIC_ACL		5
#define VIRTCHNL2_RXDID_6_FLEX_SQ_NIC_2			6
#define VIRTCHNL2_RXDID_7_HW_RSVD			7
/* 9 through 15 are reserved */
#define VIRTCHNL2_RXDID_16_COMMS_GENERIC		16
#define VIRTCHNL2_RXDID_17_COMMS_AUX_VLAN		17
#define VIRTCHNL2_RXDID_18_COMMS_AUX_IPV4		18
#define VIRTCHNL2_RXDID_19_COMMS_AUX_IPV6		19
#define VIRTCHNL2_RXDID_20_COMMS_AUX_FLOW		20
#define VIRTCHNL2_RXDID_21_COMMS_AUX_TCP		21
/* 22 through 63 are reserved */

/* VIRTCHNL2_RX_DESC_ID_BITMASKS
 * Receive descriptor ID bitmasks
 */
#define VIRTCHNL2_RXDID_0_16B_BASE_M		BIT(VIRTCHNL2_RXDID_0_16B_BASE)
#define VIRTCHNL2_RXDID_1_32B_BASE_M		BIT(VIRTCHNL2_RXDID_1_32B_BASE)
#define VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M		BIT(VIRTCHNL2_RXDID_2_FLEX_SPLITQ)
#define VIRTCHNL2_RXDID_2_FLEX_SQ_NIC_M		BIT(VIRTCHNL2_RXDID_2_FLEX_SQ_NIC)
#define VIRTCHNL2_RXDID_3_FLEX_SQ_SW_M		BIT(VIRTCHNL2_RXDID_3_FLEX_SQ_SW)
#define VIRTCHNL2_RXDID_4_FLEX_SQ_NIC_VEB_M	BIT(VIRTCHNL2_RXDID_4_FLEX_SQ_NIC_VEB)
#define VIRTCHNL2_RXDID_5_FLEX_SQ_NIC_ACL_M	BIT(VIRTCHNL2_RXDID_5_FLEX_SQ_NIC_ACL)
#define VIRTCHNL2_RXDID_6_FLEX_SQ_NIC_2_M	BIT(VIRTCHNL2_RXDID_6_FLEX_SQ_NIC_2)
#define VIRTCHNL2_RXDID_7_HW_RSVD_M		BIT(VIRTCHNL2_RXDID_7_HW_RSVD)
/* 9 through 15 are reserved */
#define VIRTCHNL2_RXDID_16_COMMS_GENERIC_M	BIT(VIRTCHNL2_RXDID_16_COMMS_GENERIC)
#define VIRTCHNL2_RXDID_17_COMMS_AUX_VLAN_M	BIT(VIRTCHNL2_RXDID_17_COMMS_AUX_VLAN)
#define VIRTCHNL2_RXDID_18_COMMS_AUX_IPV4_M	BIT(VIRTCHNL2_RXDID_18_COMMS_AUX_IPV4)
#define VIRTCHNL2_RXDID_19_COMMS_AUX_IPV6_M	BIT(VIRTCHNL2_RXDID_19_COMMS_AUX_IPV6)
#define VIRTCHNL2_RXDID_20_COMMS_AUX_FLOW_M	BIT(VIRTCHNL2_RXDID_20_COMMS_AUX_FLOW)
#define VIRTCHNL2_RXDID_21_COMMS_AUX_TCP_M	BIT(VIRTCHNL2_RXDID_21_COMMS_AUX_TCP)
/* 22 through 63 are reserved */

/* Rx */
/* For splitq virtchnl2_rx_flex_desc_adv desc members */
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RXDID_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RXDID_M		\
	IDPF_M(0xFUL, VIRTCHNL2_RX_FLEX_DESC_ADV_RXDID_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_M		\
	IDPF_M(0x3FFUL, VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_UMBCAST_S		10
#define VIRTCHNL2_RX_FLEX_DESC_ADV_UMBCAST_M		\
	IDPF_M(0x3UL, VIRTCHNL2_RX_FLEX_DESC_ADV_UMBCAST_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF0_S		12
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF0_M			\
	IDPF_M(0xFUL, VIRTCHNL2_RX_FLEX_DESC_ADV_FF0_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_M	\
	IDPF_M(0x3FFFUL, VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_S		14
#define VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_M			\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_S		15
#define VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_HDR_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_HDR_M		\
	IDPF_M(0x3FFUL, VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_HDR_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RSC_S		10
#define VIRTCHNL2_RX_FLEX_DESC_ADV_RSC_M			\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_RSC_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_SPH_S		11
#define VIRTCHNL2_RX_FLEX_DESC_ADV_SPH_M			\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_SPH_S)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF1_S		12
#define VIRTCHNL2_RX_FLEX_DESC_ADV_FF1_M			\
	IDPF_M(0x7UL, VIRTCHNL2_RX_FLEX_DESC_ADV_FF1_M)
#define VIRTCHNL2_RX_FLEX_DESC_ADV_MISS_S		15
#define VIRTCHNL2_RX_FLEX_DESC_ADV_MISS_M		\
	BIT_ULL(VIRTCHNL2_RX_FLEX_DESC_ADV_MISS_S)

/* VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS_ERROR_0_QW1_BITS
 * for splitq virtchnl2_rx_flex_desc_adv
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_DD_S			0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_EOF_S		1
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_HBO_S		2
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_L3L4P_S		3
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_IPE_S		4
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_L4E_S		5
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EIPE_S		6
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EUDPE_S		7

/* VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS_ERROR_0_QW0_BITS
 * for splitq virtchnl2_rx_flex_desc_adv
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_LPBK_S		0
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_IPV6EXADD_S		1
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_RXE_S		2
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_CRCP_S		3
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_RSS_VALID_S		4
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_L2TAG1P_S		5
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XTRMD0_VALID_S	6
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XTRMD1_VALID_S	7
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_LAST			8 /* this entry must be last!!! */

/* VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS_ERROR_1_BITS
 * for splitq virtchnl2_rx_flex_desc_adv
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_RSVD_S		0 /* 2 bits */
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_ATRAEFAIL_S		2
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_L2TAG2P_S		3
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD2_VALID_S	4
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD3_VALID_S	5
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD4_VALID_S	6
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_XTRMD5_VALID_S	7
#define VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS1_LAST			8 /* this entry must be last!!! */

/* for singleq (flex) virtchnl2_rx_flex_desc fields */
/* for virtchnl2_rx_flex_desc.ptype_flex_flags0 member */
#define VIRTCHNL2_RX_FLEX_DESC_PTYPE_S			0
#define VIRTCHNL2_RX_FLEX_DESC_PTYPE_M			\
	IDPF_M(0x3FFUL, VIRTCHNL2_RX_FLEX_DESC_PTYPE_S) /* 10 bits */

/* for virtchnl2_rx_flex_desc.pkt_length member */
#define VIRTCHNL2_RX_FLEX_DESC_PKT_LEN_S			0
#define VIRTCHNL2_RX_FLEX_DESC_PKT_LEN_M			\
	IDPF_M(0x3FFFUL, VIRTCHNL2_RX_FLEX_DESC_PKT_LEN_S) /* 14 bits */

/* VIRTCHNL2_RX_FLEX_DESC_STATUS_ERROR_0_BITS
 * for singleq (flex) virtchnl2_rx_flex_desc
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_DD_S			0
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_EOF_S			1
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_HBO_S			2
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_L3L4P_S			3
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_IPE_S		4
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_L4E_S		5
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S		6
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S		7
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_LPBK_S			8
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_IPV6EXADD_S		9
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_RXE_S			10
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_CRCP_S			11
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_RSS_VALID_S		12
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_L2TAG1P_S		13
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_XTRMD0_VALID_S		14
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_XTRMD1_VALID_S		15
#define VIRTCHNL2_RX_FLEX_DESC_STATUS0_LAST			16 /* this entry must be last!!! */

/* VIRTCHNL2_RX_FLEX_DESC_STATUS_ERROR_1_BITS
 * for singleq (flex) virtchnl2_rx_flex_desc
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_CPM_S			0 /* 4 bits */
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_NAT_S			4
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_CRYPTO_S			5
/* [10:6] reserved */
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_L2TAG2P_S		11
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD2_VALID_S		12
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD3_VALID_S		13
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD4_VALID_S		14
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_XTRMD5_VALID_S		15
#define VIRTCHNL2_RX_FLEX_DESC_STATUS1_LAST			16 /* this entry must be last!!! */

/* for virtchnl2_rx_flex_desc.ts_low member */
#define VIRTCHNL2_RX_FLEX_TSTAMP_VALID				BIT(0)

/* For singleq (non flex) virtchnl2_singleq_base_rx_desc legacy desc members */
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_SPH_S	63
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_SPH_M	\
	BIT_ULL(VIRTCHNL2_RX_BASE_DESC_QW1_LEN_SPH_S)
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_HBUF_S	52
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_HBUF_M	\
	IDPF_M(0x7FFULL, VIRTCHNL2_RX_BASE_DESC_QW1_LEN_HBUF_S)
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_PBUF_S	38
#define VIRTCHNL2_RX_BASE_DESC_QW1_LEN_PBUF_M	\
	IDPF_M(0x3FFFULL, VIRTCHNL2_RX_BASE_DESC_QW1_LEN_PBUF_S)
#define VIRTCHNL2_RX_BASE_DESC_QW1_PTYPE_S	30
#define VIRTCHNL2_RX_BASE_DESC_QW1_PTYPE_M	\
	IDPF_M(0xFFULL, VIRTCHNL2_RX_BASE_DESC_QW1_PTYPE_S)
#define VIRTCHNL2_RX_BASE_DESC_QW1_ERROR_S	19
#define VIRTCHNL2_RX_BASE_DESC_QW1_ERROR_M	\
	IDPF_M(0xFFUL, VIRTCHNL2_RX_BASE_DESC_QW1_ERROR_S)
#define VIRTCHNL2_RX_BASE_DESC_QW1_STATUS_S	0
#define VIRTCHNL2_RX_BASE_DESC_QW1_STATUS_M	\
	IDPF_M(0x7FFFFUL, VIRTCHNL2_RX_BASE_DESC_QW1_STATUS_S)

/* VIRTCHNL2_RX_BASE_DESC_STATUS_BITS
 * for singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_BASE_DESC_STATUS_DD_S		0
#define VIRTCHNL2_RX_BASE_DESC_STATUS_EOF_S		1
#define VIRTCHNL2_RX_BASE_DESC_STATUS_L2TAG1P_S		2
#define VIRTCHNL2_RX_BASE_DESC_STATUS_L3L4P_S		3
#define VIRTCHNL2_RX_BASE_DESC_STATUS_CRCP_S		4
#define VIRTCHNL2_RX_BASE_DESC_STATUS_RSVD_S		5 /* 3 bits */
#define VIRTCHNL2_RX_BASE_DESC_STATUS_EXT_UDP_0_S	8
#define VIRTCHNL2_RX_BASE_DESC_STATUS_UMBCAST_S		9 /* 2 bits */
#define VIRTCHNL2_RX_BASE_DESC_STATUS_FLM_S		11
#define VIRTCHNL2_RX_BASE_DESC_STATUS_FLTSTAT_S		12 /* 2 bits */
#define VIRTCHNL2_RX_BASE_DESC_STATUS_LPBK_S		14
#define VIRTCHNL2_RX_BASE_DESC_STATUS_IPV6EXADD_S	15
#define VIRTCHNL2_RX_BASE_DESC_STATUS_RSVD1_S		16 /* 2 bits */
#define VIRTCHNL2_RX_BASE_DESC_STATUS_INT_UDP_0_S	18
#define VIRTCHNL2_RX_BASE_DESC_STATUS_LAST		19 /* this entry must be last!!! */

/* VIRTCHNL2_RX_BASE_DESC_EXT_STATUS_BITS
 * for singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_BASE_DESC_EXT_STATUS_L2TAG2P_S	0

/* VIRTCHNL2_RX_BASE_DESC_ERROR_BITS
 * for singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_BASE_DESC_ERROR_RXE_S		0
#define VIRTCHNL2_RX_BASE_DESC_ERROR_ATRAEFAIL_S	1
#define VIRTCHNL2_RX_BASE_DESC_ERROR_HBO_S		2
#define VIRTCHNL2_RX_BASE_DESC_ERROR_L3L4E_S		3 /* 3 bits */
#define VIRTCHNL2_RX_BASE_DESC_ERROR_IPE_S		3
#define VIRTCHNL2_RX_BASE_DESC_ERROR_L4E_S		4
#define VIRTCHNL2_RX_BASE_DESC_ERROR_EIPE_S		5
#define VIRTCHNL2_RX_BASE_DESC_ERROR_OVERSIZE_S		6
#define VIRTCHNL2_RX_BASE_DESC_ERROR_PPRS_S		7

/* VIRTCHNL2_RX_BASE_DESC_FLTSTAT_VALUES
 * for singleq (base) virtchnl2_rx_base_desc
 * Note: These are predefined bit offsets
 */
#define VIRTCHNL2_RX_BASE_DESC_FLTSTAT_NO_DATA		0
#define VIRTCHNL2_RX_BASE_DESC_FLTSTAT_FD_ID		1
#define VIRTCHNL2_RX_BASE_DESC_FLTSTAT_RSV		2
#define VIRTCHNL2_RX_BASE_DESC_FLTSTAT_RSS_HASH		3

/* Receive Descriptors */
/* splitq buf
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
		__le16  buf_id; /* Buffer Identifier */
		__le16  rsvd0;
		__le32  rsvd1;
	} qword0;
	__le64  pkt_addr; /* Packet buffer address */
	__le64  hdr_addr; /* Header buffer address */
	__le64  rsvd2;
}; /* read used with buffer queues*/

/* singleq buf
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
	__le64  pkt_addr; /* Packet buffer address */
	__le64  hdr_addr; /* Header buffer address */
	__le64  rsvd1;
	__le64  rsvd2;
}; /* read used with buffer queues*/

union virtchnl2_rx_buf_desc {
	struct virtchnl2_singleq_rx_buf_desc		read;
	struct virtchnl2_splitq_rx_buf_desc		split_rd;
};

/* (0x00) singleq wb(compl) */
struct virtchnl2_singleq_base_rx_desc {
	struct {
		struct {
			__le16 mirroring_status;
			__le16 l2tag1;
		} lo_dword;
		union {
			__le32 rss; /* RSS Hash */
			__le32 fd_id; /* Flow Director filter id */
		} hi_dword;
	} qword0;
	struct {
		/* status/error/PTYPE/length */
		__le64 status_error_ptype_len;
	} qword1;
	struct {
		__le16 ext_status; /* extended status */
		__le16 rsvd;
		__le16 l2tag2_1;
		__le16 l2tag2_2;
	} qword2;
	struct {
		__le32 reserved;
		__le32 fd_id;
	} qword3;
}; /* writeback */

/* (0x01) singleq flex compl */
struct virtchnl2_rx_flex_desc {
	/* Qword 0 */
	u8 rxdid; /* descriptor builder profile id */
	u8 mir_id_umb_cast; /* mirror=[5:0], umb=[7:6] */
	__le16 ptype_flex_flags0; /* ptype=[9:0], ff0=[15:10] */
	__le16 pkt_len; /* [15:14] are reserved */
	__le16 hdr_len_sph_flex_flags1; /* header=[10:0] */
					/* sph=[11:11] */
					/* ff1/ext=[15:12] */

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

/* (0x02) */
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

/* Rx Flex Descriptor Switch Profile
 * RxDID Profile Id 3
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
	__le16 src_vsi; /* [10:15] are reserved */
	__le16 flex_md1_rsvd;

	/* Qword 2 */
	__le16 status_error1;
	u8 flex_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 rsvd; /* flex words 2-3 are reserved */
	__le32 ts_high;
};


/* Rx Flex Descriptor NIC Profile
 * RxDID Profile Id 6
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

/* Rx Flex Descriptor Advanced (Split Queue Model)
 * RxDID Profile Id 7
 */
struct virtchnl2_rx_flex_desc_adv {
	/* Qword 0 */
	u8 rxdid_ucast; /* profile_id=[3:0] */
			/* rsvd=[5:4] */
			/* ucast=[7:6] */
	u8 status_err0_qw0;
	__le16 ptype_err_fflags0;	/* ptype=[9:0] */
					/* ip_hdr_err=[10:10] */
					/* udp_len_err=[11:11] */
					/* ff0=[15:12] */
	__le16 pktlen_gen_bufq_id;	/* plen=[13:0] */
					/* gen=[14:14]  only in splitq */
					/* bufq_id=[15:15] only in splitq */
	__le16 hdrlen_flags;		/* header=[9:0] */
					/* rsc=[10:10] only in splitq */
					/* sph=[11:11] only in splitq */
					/* ext_udp_0=[12:12] */
					/* int_udp_0=[13:13] */
					/* trunc_mirr=[14:14] */
					/* miss_prepend=[15:15] */
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
}; /* writeback */

/* Rx Flex Descriptor Advanced (Split Queue Model) NIC Profile
 * RxDID Profile Id 8
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
	u8 rxdid_ucast; /* profile_id=[3:0] */
			/* rsvd=[5:4] */
			/* ucast=[7:6] */
	u8 status_err0_qw0;
	__le16 ptype_err_fflags0;	/* ptype=[9:0] */
					/* ip_hdr_err=[10:10] */
					/* udp_len_err=[11:11] */
					/* ff0=[15:12] */
	__le16 pktlen_gen_bufq_id;	/* plen=[13:0] */
					/* gen=[14:14]  only in splitq */
					/* bufq_id=[15:15] only in splitq */
	__le16 hdrlen_flags;		/* header=[9:0] */
					/* rsc=[10:10] only in splitq */
					/* sph=[11:11] only in splitq */
					/* ext_udp_0=[12:12] */
					/* int_udp_0=[13:13] */
					/* trunc_mirr=[14:14] */
					/* miss_prepend=[15:15] */
	/* Qword 1 */
	u8 status_err0_qw1;
	u8 status_err1;
	u8 fflags1;
	u8 ts_low;
	__le16 buf_id; /* only in splitq */
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
}; /* writeback */

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
