/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef T4_MSG_H
#define T4_MSG_H

enum {
	CPL_SGE_EGR_UPDATE    = 0xA5,
	CPL_FW4_MSG           = 0xC0,
	CPL_FW6_MSG           = 0xE0,
	CPL_TX_PKT_LSO        = 0xED,
	CPL_TX_PKT_XT         = 0xEE,
};

enum {                     /* TX_PKT_XT checksum types */
	TX_CSUM_TCPIP  = 8,
	TX_CSUM_UDPIP  = 9,
	TX_CSUM_TCPIP6 = 10,
};

union opcode_tid {
	__be32 opcode_tid;
	__u8 opcode;
};

struct rss_header {
	__u8 opcode;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	__u8 channel:2;
	__u8 filter_hit:1;
	__u8 filter_tid:1;
	__u8 hash_type:2;
	__u8 ipv6:1;
	__u8 send2fw:1;
#else
	__u8 send2fw:1;
	__u8 ipv6:1;
	__u8 hash_type:2;
	__u8 filter_tid:1;
	__u8 filter_hit:1;
	__u8 channel:2;
#endif
	__be16 qid;
	__be32 hash_val;
};

#if defined(RSS_HDR_VLD) || defined(CHELSIO_FW)
#define RSS_HDR struct rss_header rss_hdr
#else
#define RSS_HDR
#endif

#ifndef CHELSIO_FW
struct work_request_hdr {
	__be32 wr_hi;
	__be32 wr_mid;
	__be64 wr_lo;
};

#define WR_HDR struct work_request_hdr wr
#define WR_HDR_SIZE sizeof(struct work_request_hdr)
#else
#define WR_HDR
#define WR_HDR_SIZE 0
#endif

struct cpl_tx_data {
	union opcode_tid ot;
	__be32 len;
	__be32 rsvd;
	__be32 flags;
};

struct cpl_tx_pkt_core {
	__be32 ctrl0;
	__be16 pack;
	__be16 len;
	__be64 ctrl1;
};

struct cpl_tx_pkt {
	WR_HDR;
	struct cpl_tx_pkt_core c;
};

/* cpl_tx_pkt_core.ctrl0 fields */
#define S_TXPKT_PF    8
#define M_TXPKT_PF    0x7
#define V_TXPKT_PF(x) ((x) << S_TXPKT_PF)
#define G_TXPKT_PF(x) (((x) >> S_TXPKT_PF) & M_TXPKT_PF)

#define S_TXPKT_INTF    16
#define M_TXPKT_INTF    0xF
#define V_TXPKT_INTF(x) ((x) << S_TXPKT_INTF)
#define G_TXPKT_INTF(x) (((x) >> S_TXPKT_INTF) & M_TXPKT_INTF)

#define S_TXPKT_OPCODE    24
#define M_TXPKT_OPCODE    0xFF
#define V_TXPKT_OPCODE(x) ((x) << S_TXPKT_OPCODE)
#define G_TXPKT_OPCODE(x) (((x) >> S_TXPKT_OPCODE) & M_TXPKT_OPCODE)

/* cpl_tx_pkt_core.ctrl1 fields */
#define S_TXPKT_IPHDR_LEN    20
#define M_TXPKT_IPHDR_LEN    0x3FFF
#define V_TXPKT_IPHDR_LEN(x) ((__u64)(x) << S_TXPKT_IPHDR_LEN)
#define G_TXPKT_IPHDR_LEN(x) (((x) >> S_TXPKT_IPHDR_LEN) & M_TXPKT_IPHDR_LEN)

#define S_TXPKT_ETHHDR_LEN    34
#define M_TXPKT_ETHHDR_LEN    0x3F
#define V_TXPKT_ETHHDR_LEN(x) ((__u64)(x) << S_TXPKT_ETHHDR_LEN)
#define G_TXPKT_ETHHDR_LEN(x) (((x) >> S_TXPKT_ETHHDR_LEN) & M_TXPKT_ETHHDR_LEN)

#define S_T6_TXPKT_ETHHDR_LEN    32
#define M_T6_TXPKT_ETHHDR_LEN    0xFF
#define V_T6_TXPKT_ETHHDR_LEN(x) ((__u64)(x) << S_T6_TXPKT_ETHHDR_LEN)
#define G_T6_TXPKT_ETHHDR_LEN(x) \
	(((x) >> S_T6_TXPKT_ETHHDR_LEN) & M_T6_TXPKT_ETHHDR_LEN)

#define S_TXPKT_CSUM_TYPE    40
#define M_TXPKT_CSUM_TYPE    0xF
#define V_TXPKT_CSUM_TYPE(x) ((__u64)(x) << S_TXPKT_CSUM_TYPE)
#define G_TXPKT_CSUM_TYPE(x) (((x) >> S_TXPKT_CSUM_TYPE) & M_TXPKT_CSUM_TYPE)

#define S_TXPKT_VLAN    44
#define M_TXPKT_VLAN    0xFFFF
#define V_TXPKT_VLAN(x) ((__u64)(x) << S_TXPKT_VLAN)
#define G_TXPKT_VLAN(x) (((x) >> S_TXPKT_VLAN) & M_TXPKT_VLAN)

#define S_TXPKT_VLAN_VLD    60
#define V_TXPKT_VLAN_VLD(x) ((__u64)(x) << S_TXPKT_VLAN_VLD)
#define F_TXPKT_VLAN_VLD    V_TXPKT_VLAN_VLD(1ULL)

#define S_TXPKT_IPCSUM_DIS    62
#define V_TXPKT_IPCSUM_DIS(x) ((__u64)(x) << S_TXPKT_IPCSUM_DIS)
#define F_TXPKT_IPCSUM_DIS    V_TXPKT_IPCSUM_DIS(1ULL)

#define S_TXPKT_L4CSUM_DIS    63
#define V_TXPKT_L4CSUM_DIS(x) ((__u64)(x) << S_TXPKT_L4CSUM_DIS)
#define F_TXPKT_L4CSUM_DIS    V_TXPKT_L4CSUM_DIS(1ULL)

struct cpl_tx_pkt_lso_core {
	__be32 lso_ctrl;
	__be16 ipid_ofst;
	__be16 mss;
	__be32 seqno_offset;
	__be32 len;
	/* encapsulated CPL (TX_PKT, TX_PKT_XT or TX_DATA) follows here */
};

struct cpl_tx_pkt_lso {
	WR_HDR;
	struct cpl_tx_pkt_lso_core c;
	/* encapsulated CPL (TX_PKT, TX_PKT_XT or TX_DATA) follows here */
};

/* cpl_tx_pkt_lso_core.lso_ctrl fields */
#define S_LSO_TCPHDR_LEN    0
#define M_LSO_TCPHDR_LEN    0xF
#define V_LSO_TCPHDR_LEN(x) ((x) << S_LSO_TCPHDR_LEN)
#define G_LSO_TCPHDR_LEN(x) (((x) >> S_LSO_TCPHDR_LEN) & M_LSO_TCPHDR_LEN)

#define S_LSO_IPHDR_LEN    4
#define M_LSO_IPHDR_LEN    0xFFF
#define V_LSO_IPHDR_LEN(x) ((x) << S_LSO_IPHDR_LEN)
#define G_LSO_IPHDR_LEN(x) (((x) >> S_LSO_IPHDR_LEN) & M_LSO_IPHDR_LEN)

#define S_LSO_ETHHDR_LEN    16
#define M_LSO_ETHHDR_LEN    0xF
#define V_LSO_ETHHDR_LEN(x) ((x) << S_LSO_ETHHDR_LEN)
#define G_LSO_ETHHDR_LEN(x) (((x) >> S_LSO_ETHHDR_LEN) & M_LSO_ETHHDR_LEN)

#define S_LSO_IPV6    20
#define V_LSO_IPV6(x) ((x) << S_LSO_IPV6)
#define F_LSO_IPV6    V_LSO_IPV6(1U)

#define S_LSO_LAST_SLICE    22
#define V_LSO_LAST_SLICE(x) ((x) << S_LSO_LAST_SLICE)
#define F_LSO_LAST_SLICE    V_LSO_LAST_SLICE(1U)

#define S_LSO_FIRST_SLICE    23
#define V_LSO_FIRST_SLICE(x) ((x) << S_LSO_FIRST_SLICE)
#define F_LSO_FIRST_SLICE    V_LSO_FIRST_SLICE(1U)

#define S_LSO_OPCODE    24
#define M_LSO_OPCODE    0xFF
#define V_LSO_OPCODE(x) ((x) << S_LSO_OPCODE)
#define G_LSO_OPCODE(x) (((x) >> S_LSO_OPCODE) & M_LSO_OPCODE)

#define S_LSO_T5_XFER_SIZE	   0
#define M_LSO_T5_XFER_SIZE    0xFFFFFFF
#define V_LSO_T5_XFER_SIZE(x) ((x) << S_LSO_T5_XFER_SIZE)
#define G_LSO_T5_XFER_SIZE(x) (((x) >> S_LSO_T5_XFER_SIZE) & M_LSO_T5_XFER_SIZE)

struct cpl_rx_pkt {
	RSS_HDR;
	__u8 opcode;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	__u8 iff:4;
	__u8 csum_calc:1;
	__u8 ipmi_pkt:1;
	__u8 vlan_ex:1;
	__u8 ip_frag:1;
#else
	__u8 ip_frag:1;
	__u8 vlan_ex:1;
	__u8 ipmi_pkt:1;
	__u8 csum_calc:1;
	__u8 iff:4;
#endif
	__be16 csum;
	__be16 vlan;
	__be16 len;
	__be32 l2info;
	__be16 hdr_len;
	__be16 err_vec;
};

/* rx_pkt.l2info fields */
#define S_RXF_UDP    22
#define V_RXF_UDP(x) ((x) << S_RXF_UDP)
#define F_RXF_UDP    V_RXF_UDP(1U)

#define S_RXF_TCP    23
#define V_RXF_TCP(x) ((x) << S_RXF_TCP)
#define F_RXF_TCP    V_RXF_TCP(1U)

#define S_RXF_IP    24
#define V_RXF_IP(x) ((x) << S_RXF_IP)
#define F_RXF_IP    V_RXF_IP(1U)

#define S_RXF_IP6    25
#define V_RXF_IP6(x) ((x) << S_RXF_IP6)
#define F_RXF_IP6    V_RXF_IP6(1U)

/* rx_pkt.err_vec fields */
/* In T6, rx_pkt.err_vec indicates
 * RxError Error vector (16b) or
 * Encapsulating header length (8b),
 * Outer encapsulation type (2b) and
 * compressed error vector (6b) if CRxPktEnc is
 * enabled in TP_OUT_CONFIG
 */
#define S_T6_COMPR_RXERR_VEC    0
#define M_T6_COMPR_RXERR_VEC    0x3F
#define V_T6_COMPR_RXERR_VEC(x) ((x) << S_T6_COMPR_RXERR_VEC)
#define G_T6_COMPR_RXERR_VEC(x) \
	(((x) >> S_T6_COMPR_RXERR_VEC) & M_T6_COMPR_RXERR_VEC)

/* cpl_fw*.type values */
enum {
	FW_TYPE_RSSCPL = 4,
};

struct cpl_fw4_msg {
	RSS_HDR;
	u8 opcode;
	u8 type;
	__be16 rsvd0;
	__be32 rsvd1;
	__be64 data[2];
};

struct cpl_fw6_msg {
	RSS_HDR;
	u8 opcode;
	u8 type;
	__be16 rsvd0;
	__be32 rsvd1;
	__be64 data[4];
};

enum {
	ULP_TX_SC_IMM  = 0x81,
	ULP_TX_SC_DSGL = 0x82,
	ULP_TX_SC_ISGL = 0x83
};

#define S_ULPTX_CMD    24
#define M_ULPTX_CMD    0xFF
#define V_ULPTX_CMD(x) ((x) << S_ULPTX_CMD)

#define S_ULP_TX_SC_MORE 23
#define V_ULP_TX_SC_MORE(x) ((x) << S_ULP_TX_SC_MORE)
#define F_ULP_TX_SC_MORE  V_ULP_TX_SC_MORE(1U)

struct ulptx_sge_pair {
	__be32 len[2];
	__be64 addr[2];
};

struct ulptx_sgl {
	__be32 cmd_nsge;
	__be32 len0;
	__be64 addr0;

#if !(defined C99_NOT_SUPPORTED)
	struct ulptx_sge_pair sge[0];
#endif

};

struct ulptx_idata {
	__be32 cmd_more;
	__be32 len;
};

#define S_ULPTX_NSGE    0
#define M_ULPTX_NSGE    0xFFFF
#define V_ULPTX_NSGE(x) ((x) << S_ULPTX_NSGE)

struct ulp_txpkt {
	__be32 cmd_dest;
	__be32 len;
};

/* ulp_txpkt.cmd_dest fields */
#define S_ULP_TXPKT_DEST    16
#define M_ULP_TXPKT_DEST    0x3
#define V_ULP_TXPKT_DEST(x) ((x) << S_ULP_TXPKT_DEST)

#define S_ULP_TXPKT_FID	    4
#define M_ULP_TXPKT_FID     0x7ff
#define V_ULP_TXPKT_FID(x)  ((x) << S_ULP_TXPKT_FID)

#define S_ULP_TXPKT_RO      3
#define V_ULP_TXPKT_RO(x) ((x) << S_ULP_TXPKT_RO)
#define F_ULP_TXPKT_RO V_ULP_TXPKT_RO(1U)

#endif  /* T4_MSG_H */
