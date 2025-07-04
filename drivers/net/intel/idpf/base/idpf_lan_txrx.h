/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2024 Intel Corporation
 */

#ifndef _IDPF_LAN_TXRX_H_
#define _IDPF_LAN_TXRX_H_

#include "idpf_osdep.h"

enum idpf_rss_hash {
	IDPF_HASH_INVALID			= 0,
	/* Values 1 - 28 are reserved for future use */
	IDPF_HASH_NONF_UNICAST_IPV4_UDP		= 29,
	IDPF_HASH_NONF_MULTICAST_IPV4_UDP,
	IDPF_HASH_NONF_IPV4_UDP,
	IDPF_HASH_NONF_IPV4_TCP_SYN_NO_ACK,
	IDPF_HASH_NONF_IPV4_TCP,
	IDPF_HASH_NONF_IPV4_SCTP,
	IDPF_HASH_NONF_IPV4_OTHER,
	IDPF_HASH_FRAG_IPV4,
	/* Values 37-38 are reserved */
	IDPF_HASH_NONF_UNICAST_IPV6_UDP		= 39,
	IDPF_HASH_NONF_MULTICAST_IPV6_UDP,
	IDPF_HASH_NONF_IPV6_UDP,
	IDPF_HASH_NONF_IPV6_TCP_SYN_NO_ACK,
	IDPF_HASH_NONF_IPV6_TCP,
	IDPF_HASH_NONF_IPV6_SCTP,
	IDPF_HASH_NONF_IPV6_OTHER,
	IDPF_HASH_FRAG_IPV6,
	IDPF_HASH_NONF_RSVD47,
	IDPF_HASH_NONF_FCOE_OX,
	IDPF_HASH_NONF_FCOE_RX,
	IDPF_HASH_NONF_FCOE_OTHER,
	/* Values 51-62 are reserved */
	IDPF_HASH_L2_PAYLOAD			= 63,
	IDPF_HASH_MAX
};

/* Supported RSS offloads */
#define IDPF_DEFAULT_RSS_HASH			\
	(BIT_ULL(IDPF_HASH_NONF_IPV4_UDP) |	\
	BIT_ULL(IDPF_HASH_NONF_IPV4_SCTP) |	\
	BIT_ULL(IDPF_HASH_NONF_IPV4_TCP) |	\
	BIT_ULL(IDPF_HASH_NONF_IPV4_OTHER) |	\
	BIT_ULL(IDPF_HASH_FRAG_IPV4) |		\
	BIT_ULL(IDPF_HASH_NONF_IPV6_UDP) |	\
	BIT_ULL(IDPF_HASH_NONF_IPV6_TCP) |	\
	BIT_ULL(IDPF_HASH_NONF_IPV6_SCTP) |	\
	BIT_ULL(IDPF_HASH_NONF_IPV6_OTHER) |	\
	BIT_ULL(IDPF_HASH_FRAG_IPV6) |		\
	BIT_ULL(IDPF_HASH_L2_PAYLOAD))

#define IDPF_DEFAULT_RSS_HASH_EXPANDED (IDPF_DEFAULT_RSS_HASH | \
	BIT_ULL(IDPF_HASH_NONF_IPV4_TCP_SYN_NO_ACK) |		\
	BIT_ULL(IDPF_HASH_NONF_UNICAST_IPV4_UDP) |		\
	BIT_ULL(IDPF_HASH_NONF_MULTICAST_IPV4_UDP) |		\
	BIT_ULL(IDPF_HASH_NONF_IPV6_TCP_SYN_NO_ACK) |		\
	BIT_ULL(IDPF_HASH_NONF_UNICAST_IPV6_UDP) |		\
	BIT_ULL(IDPF_HASH_NONF_MULTICAST_IPV6_UDP))

/* For idpf_splitq_base_tx_compl_desc */
#define IDPF_TXD_COMPLQ_GEN_S		15
#define IDPF_TXD_COMPLQ_GEN_M		BIT_ULL(IDPF_TXD_COMPLQ_GEN_S)
#define IDPF_TXD_COMPLQ_COMPL_TYPE_S	11
#define IDPF_TXD_COMPLQ_COMPL_TYPE_M	GENMASK_ULL(13, 11)
#define IDPF_TXD_COMPLQ_QID_S		0
#define IDPF_TXD_COMPLQ_QID_M		GENMASK_ULL(9, 0)

/* For base mode TX descriptors */

#define IDPF_TXD_CTX_QW0_TUNN_L4T_CS_S		23
#define IDPF_TXD_CTX_QW0_TUNN_L4T_CS_M		\
	BIT_ULL(IDPF_TXD_CTX_QW0_TUNN_L4T_CS_S)
#define IDPF_TXD_CTX_QW0_TUNN_DECTTL_S		19
#define IDPF_TXD_CTX_QW0_TUNN_DECTTL_M		GENMASK_ULL(22, 19)
#define IDPF_TXD_CTX_QW0_TUNN_NATLEN_S		12
#define IDPF_TXD_CTX_QW0_TUNN_NATLEN_M		GENMASK_ULL(18, 12)
#define IDPF_TXD_CTX_QW0_TUNN_EIP_NOINC_S	11
#define IDPF_TXD_CTX_QW0_TUNN_EIP_NOINC_M	\
	BIT_ULL(IDPF_TXD_CTX_QW0_TUNN_EIP_NOINC_S)
#define IDPF_TXD_CTX_EIP_NOINC_IPID_CONST	\
	IDPF_TXD_CTX_QW0_TUNN_EIP_NOINC_M
#define IDPF_TXD_CTX_QW0_TUNN_NATT_S		9
#define IDPF_TXD_CTX_QW0_TUNN_NATT_M		GENMASK_ULL(10, 9)
#define IDPF_TXD_CTX_UDP_TUNNELING		BIT_ULL(9)
#define IDPF_TXD_CTX_GRE_TUNNELING		BIT_ULL(10)
#define IDPF_TXD_CTX_QW0_TUNN_EXT_IPLEN_S	2
#define IDPF_TXD_CTX_QW0_TUNN_EXT_IPLEN_M	GENMASK_ULL(7, 2)
#define IDPF_TXD_CTX_QW0_TUNN_EXT_IP_S		0
#define IDPF_TXD_CTX_QW0_TUNN_EXT_IP_M		GENMASK_ULL(1, 0)

#define IDPF_TXD_CTX_QW1_MSS_S			50
#define IDPF_TXD_CTX_QW1_MSS_M			GENMASK_ULL(63, 50)
#define IDPF_TXD_CTX_QW1_TSO_LEN_S		30
#define IDPF_TXD_CTX_QW1_TSO_LEN_M		GENMASK_ULL(47, 30)
#define IDPF_TXD_CTX_QW1_CMD_S			4
#define IDPF_TXD_CTX_QW1_CMD_M			GENMASK_ULL(15, 4)
#define IDPF_TXD_CTX_QW1_DTYPE_S		0
#define IDPF_TXD_CTX_QW1_DTYPE_M		GENMASK_ULL(3, 0)
#define IDPF_TXD_QW1_L2TAG1_S			48
#define IDPF_TXD_QW1_L2TAG1_M			GENMASK_ULL(63, 48)
#define IDPF_TXD_QW1_TX_BUF_SZ_S		34
#define IDPF_TXD_QW1_TX_BUF_SZ_M		GENMASK_ULL(47, 34)
#define IDPF_TXD_QW1_OFFSET_S			16
#define IDPF_TXD_QW1_OFFSET_M			GENMASK_ULL(33, 16)
#define IDPF_TXD_QW1_CMD_S			4
#define IDPF_TXD_QW1_CMD_M			GENMASK_ULL(15, 4)
#define IDPF_TXD_QW1_DTYPE_S			0
#define IDPF_TXD_QW1_DTYPE_M			GENMASK_ULL(3, 0)

/* TX Completion Descriptor Completion Types */
#define IDPF_TXD_COMPLT_ITR_FLUSH	0
#define IDPF_TXD_COMPLT_RULE_MISS	1
#define IDPF_TXD_COMPLT_RS		2
#define IDPF_TXD_COMPLT_REINJECTED	3
#define IDPF_TXD_COMPLT_RE		4
#define IDPF_TXD_COMPLT_SW_MARKER	5

enum idpf_tx_desc_dtype_value {
	IDPF_TX_DESC_DTYPE_DATA				= 0,
	IDPF_TX_DESC_DTYPE_CTX				= 1,
	/* DTYPE 2 is reserved
	 * DTYPE 3 is free for future use
	 * DTYPE 4 is reserved
	 */
	IDPF_TX_DESC_DTYPE_FLEX_TSO_CTX			= 5,
	/* DTYPE 6 is reserved */
	IDPF_TX_DESC_DTYPE_FLEX_L2TAG1_L2TAG2		= 7,
	/* DTYPE 8, 9 are free for future use
	 * DTYPE 10 is reserved
	 * DTYPE 11 is free for future use
	 */
	IDPF_TX_DESC_DTYPE_FLEX_FLOW_SCHE		= 12,
	/* DTYPE 13, 14 are free for future use */
	/* DESC_DONE - HW has completed write-back of descriptor */
	IDPF_TX_DESC_DTYPE_DESC_DONE			= 15,
};

enum idpf_tx_ctx_desc_cmd_bits {
	IDPF_TX_CTX_DESC_TSO		= 0x01,
	IDPF_TX_CTX_DESC_TSYN		= 0x02,
	IDPF_TX_CTX_DESC_IL2TAG2	= 0x04,
	IDPF_TX_CTX_DESC_RSVD		= 0x08,
	IDPF_TX_CTX_DESC_SWTCH_NOTAG	= 0x00,
	IDPF_TX_CTX_DESC_SWTCH_UPLINK	= 0x10,
	IDPF_TX_CTX_DESC_SWTCH_LOCAL	= 0x20,
	IDPF_TX_CTX_DESC_SWTCH_VSI	= 0x30,
	IDPF_TX_CTX_DESC_FILT_AU_EN	= 0x40,
	IDPF_TX_CTX_DESC_FILT_AU_EVICT	= 0x80,
	IDPF_TX_CTX_DESC_RSVD1		= 0xF00
};

enum idpf_tx_desc_len_fields {
	/* Note: These are predefined bit offsets */
	IDPF_TX_DESC_LEN_MACLEN_S	= 0, /* 7 BITS */
	IDPF_TX_DESC_LEN_IPLEN_S	= 7, /* 7 BITS */
	IDPF_TX_DESC_LEN_L4_LEN_S	= 14 /* 4 BITS */
};

#define IDPF_TXD_QW1_MACLEN_M		GENMASK_ULL(6, 0)
#define IDPF_TXD_QW1_IPLEN_M		GENMASK_ULL(13, 7)
#define IDPF_TXD_QW1_L4LEN_M		GENMASK_ULL(17, 14)
#define IDPF_TXD_QW1_FCLEN_M		GENMASK_ULL(17, 14)

enum idpf_tx_base_desc_cmd_bits {
	IDPF_TX_DESC_CMD_EOP			= 0x0001,
	IDPF_TX_DESC_CMD_RS			= 0x0002,
	 /* only on VFs else RSVD */
	IDPF_TX_DESC_CMD_ICRC			= 0x0004,
	IDPF_TX_DESC_CMD_IL2TAG1		= 0x0008,
	IDPF_TX_DESC_CMD_RSVD1			= 0x0010,
	IDPF_TX_DESC_CMD_IIPT_NONIP		= 0x0000, /* 2 BITS */
	IDPF_TX_DESC_CMD_IIPT_IPV6		= 0x0020, /* 2 BITS */
	IDPF_TX_DESC_CMD_IIPT_IPV4		= 0x0040, /* 2 BITS */
	IDPF_TX_DESC_CMD_IIPT_IPV4_CSUM		= 0x0060, /* 2 BITS */
	IDPF_TX_DESC_CMD_RSVD2			= 0x0080,
	IDPF_TX_DESC_CMD_L4T_EOFT_UNK		= 0x0000, /* 2 BITS */
	IDPF_TX_DESC_CMD_L4T_EOFT_TCP		= 0x0100, /* 2 BITS */
	IDPF_TX_DESC_CMD_L4T_EOFT_SCTP		= 0x0200, /* 2 BITS */
	IDPF_TX_DESC_CMD_L4T_EOFT_UDP		= 0x0300, /* 2 BITS */
	IDPF_TX_DESC_CMD_RSVD3			= 0x0400,
	IDPF_TX_DESC_CMD_RSVD4			= 0x0800,
};

/* Transmit descriptors  */
/* splitq tx buf, singleq tx buf and singleq compl desc */
struct idpf_base_tx_desc {
	__le64 buf_addr; /* Address of descriptor's data buf */
	__le64 qw1; /* type_cmd_offset_bsz_l2tag1 */
};/* read used with buffer queues*/

struct idpf_splitq_tx_compl_desc {
	/* qid=[10:0] comptype=[13:11] rsvd=[14] gen=[15] */
	__le16 qid_comptype_gen;
	union {
		__le16 q_head; /* Queue head */
		__le16 compl_tag; /* Completion tag */
	} q_head_compl_tag;
	u32 rsvd;

};/* writeback used with completion queues*/

/* Context descriptors */
struct idpf_base_tx_ctx_desc {
	struct {
		__le32 tunneling_params;
		__le16 l2tag2;
		__le16 rsvd1;
	} qw0;
	__le64 qw1; /* type_cmd_tlen_mss/rt_hint */
};

/* Common cmd field defines for all desc except Flex Flow Scheduler (0x0C) */
enum idpf_tx_flex_desc_cmd_bits {
	IDPF_TX_FLEX_DESC_CMD_EOP			= 0x01,
	IDPF_TX_FLEX_DESC_CMD_RS			= 0x02,
	IDPF_TX_FLEX_DESC_CMD_RE			= 0x04,
	IDPF_TX_FLEX_DESC_CMD_IL2TAG1			= 0x08,
	IDPF_TX_FLEX_DESC_CMD_DUMMY			= 0x10,
	IDPF_TX_FLEX_DESC_CMD_CS_EN			= 0x20,
	IDPF_TX_FLEX_DESC_CMD_FILT_AU_EN		= 0x40,
	IDPF_TX_FLEX_DESC_CMD_FILT_AU_EVICT		= 0x80,
};

struct idpf_flex_tx_desc {
	__le64 buf_addr;	/* Packet buffer address */
	struct {
#define IDPF_FLEX_TXD_QW1_DTYPE_S	0
#define IDPF_FLEX_TXD_QW1_DTYPE_M	GENMASK(4, 0)
#define IDPF_FLEX_TXD_QW1_CMD_S		5
#define IDPF_FLEX_TXD_QW1_CMD_M		GENMASK(15, 5)
		__le16 cmd_dtype;
		union {
			/* DTYPE=IDPF_TX_DESC_DTYPE_FLEX_L2TAG1_L2TAG2 (0x07) */
			struct {
				__le16 l2tag1;
				__le16 l2tag2;
			} l2tags;
		};
		__le16 buf_size;
	} qw1;
};

struct idpf_flex_tx_sched_desc {
	__le64 buf_addr;	/* Packet buffer address */

	/* DTYPE = IDPF_TX_DESC_DTYPE_FLEX_FLOW_SCHE_16B (0x0C) */
	struct {
		u8 cmd_dtype;
#define IDPF_TXD_FLEX_FLOW_DTYPE_M	0x1F
#define IDPF_TXD_FLEX_FLOW_CMD_EOP	0x20
#define IDPF_TXD_FLEX_FLOW_CMD_CS_EN	0x40
#define IDPF_TXD_FLEX_FLOW_CMD_RE	0x80

		u8 rsvd[3];

		__le16 compl_tag;
		__le16 rxr_bufsize;
#define IDPF_TXD_FLEX_FLOW_RXR		0x4000
#define IDPF_TXD_FLEX_FLOW_BUFSIZE_M	0x3FFF
	} qw1;
};

/* Common cmd fields for all flex context descriptors
 * Note: these defines already account for the 5 bit dtype in the cmd_dtype
 * field
 */
enum idpf_tx_flex_ctx_desc_cmd_bits {
	IDPF_TX_FLEX_CTX_DESC_CMD_TSO			= 0x0020,
	IDPF_TX_FLEX_CTX_DESC_CMD_TSYN_EN		= 0x0040,
	IDPF_TX_FLEX_CTX_DESC_CMD_L2TAG2		= 0x0080,
	IDPF_TX_FLEX_CTX_DESC_CMD_SWTCH_UPLNK		= 0x0200, /* 2 bits */
	IDPF_TX_FLEX_CTX_DESC_CMD_SWTCH_LOCAL		= 0x0400, /* 2 bits */
	IDPF_TX_FLEX_CTX_DESC_CMD_SWTCH_TARGETVSI	= 0x0600, /* 2 bits */
};

/* Standard flex descriptor TSO context quad word */
struct idpf_flex_tx_tso_ctx_qw {
	__le32 flex_tlen;
#define IDPF_TXD_FLEX_CTX_TLEN_M	0x3FFFF
#define IDPF_TXD_FLEX_TSO_CTX_FLEX_S	24
	__le16 mss_rt;
#define IDPF_TXD_FLEX_CTX_MSS_RT_M	0x3FFF
	u8 hdr_len;
	u8 flex;
};

union idpf_flex_tx_ctx_desc {
		/* DTYPE = IDPF_TX_DESC_DTYPE_CTX (0x01) */
	struct  {
		struct {
			u8 rsv[4];
			__le16 l2tag2;
			u8 rsv_2[2];
		} qw0;
		struct {
			__le16 cmd_dtype;
			__le16 tsyn_reg_l;
#define IDPF_TX_DESC_CTX_TSYN_L_M	GENMASK(15, 14)
			__le16 tsyn_reg_h;
#define IDPF_TX_DESC_CTX_TSYN_H_M	GENMASK(15, 0)
			__le16 mss;
#define IDPF_TX_DESC_CTX_MSS_M		GENMASK(14, 2)
		} qw1;
	} tsyn;

	/* DTYPE = IDPF_TX_DESC_DTYPE_FLEX_TSO_CTX (0x05) */
	struct {
		struct idpf_flex_tx_tso_ctx_qw qw0;
		struct {
			__le16 cmd_dtype;
			u8 flex[6];
		} qw1;
	} tso;
};

#endif /* _IDPF_LAN_TXRX_H_ */
