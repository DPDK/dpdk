/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef T4_MSG_H
#define T4_MSG_H

enum {
	CPL_ACT_OPEN_REQ      = 0x3,
	CPL_SET_TCB_FIELD     = 0x5,
	CPL_ABORT_REQ         = 0xA,
	CPL_ABORT_RPL         = 0xB,
	CPL_L2T_WRITE_REQ     = 0x12,
	CPL_SMT_WRITE_REQ     = 0x14,
	CPL_TID_RELEASE       = 0x1A,
	CPL_L2T_WRITE_RPL     = 0x23,
	CPL_ACT_OPEN_RPL      = 0x25,
	CPL_ABORT_RPL_RSS     = 0x2D,
	CPL_SMT_WRITE_RPL     = 0x2E,
	CPL_SET_TCB_RPL       = 0x3A,
	CPL_ACT_OPEN_REQ6     = 0x83,
	CPL_SGE_EGR_UPDATE    = 0xA5,
	CPL_FW4_MSG           = 0xC0,
	CPL_FW6_MSG           = 0xE0,
	CPL_TX_PKT_LSO        = 0xED,
	CPL_TX_PKT_XT         = 0xEE,
};

enum CPL_error {
	CPL_ERR_NONE               = 0,
	CPL_ERR_TCAM_FULL          = 3,
};

enum {
	ULP_MODE_NONE          = 0,
	ULP_MODE_TCPDDP        = 5,
};

enum {
	CPL_ABORT_SEND_RST = 0,
	CPL_ABORT_NO_RST,
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

#define S_CPL_OPCODE    24
#define V_CPL_OPCODE(x) ((x) << S_CPL_OPCODE)

#define G_TID(x)    ((x) & 0xFFFFFF)

/* tid is assumed to be 24-bits */
#define MK_OPCODE_TID(opcode, tid) (V_CPL_OPCODE(opcode) | (tid))

#define OPCODE_TID(cmd) ((cmd)->ot.opcode_tid)

/* extract the TID from a CPL command */
#define GET_TID(cmd) (G_TID(be32_to_cpu(OPCODE_TID(cmd))))

/* partitioning of TID fields that also carry a queue id */
#define S_TID_TID    0
#define M_TID_TID    0x3fff
#define G_TID_TID(x) (((x) >> S_TID_TID) & M_TID_TID)

#define S_TID_QID    14
#define V_TID_QID(x) ((x) << S_TID_QID)

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

#define S_COOKIE    5
#define M_COOKIE    0x7
#define V_COOKIE(x) ((x) << S_COOKIE)
#define G_COOKIE(x) (((x) >> S_COOKIE) & M_COOKIE)

/* option 0 fields */
#define S_TX_CHAN    2
#define V_TX_CHAN(x) ((x) << S_TX_CHAN)

#define S_DELACK    5
#define V_DELACK(x) ((x) << S_DELACK)

#define S_NON_OFFLOAD    7
#define V_NON_OFFLOAD(x) ((x) << S_NON_OFFLOAD)
#define F_NON_OFFLOAD    V_NON_OFFLOAD(1U)

#define S_ULP_MODE    8
#define V_ULP_MODE(x) ((x) << S_ULP_MODE)

#define S_SMAC_SEL    28
#define V_SMAC_SEL(x) ((__u64)(x) << S_SMAC_SEL)

#define S_TCAM_BYPASS    48
#define V_TCAM_BYPASS(x) ((__u64)(x) << S_TCAM_BYPASS)
#define F_TCAM_BYPASS    V_TCAM_BYPASS(1ULL)

#define S_L2T_IDX    36
#define V_L2T_IDX(x) ((__u64)(x) << S_L2T_IDX)

#define S_NAGLE    49
#define V_NAGLE(x) ((__u64)(x) << S_NAGLE)

/* option 2 fields */
#define S_RSS_QUEUE    0
#define V_RSS_QUEUE(x) ((x) << S_RSS_QUEUE)

#define S_RSS_QUEUE_VALID    10
#define V_RSS_QUEUE_VALID(x) ((x) << S_RSS_QUEUE_VALID)
#define F_RSS_QUEUE_VALID    V_RSS_QUEUE_VALID(1U)

#define S_CONG_CNTRL    14
#define V_CONG_CNTRL(x) ((x) << S_CONG_CNTRL)

#define S_RX_CHANNEL    26
#define V_RX_CHANNEL(x) ((x) << S_RX_CHANNEL)
#define F_RX_CHANNEL    V_RX_CHANNEL(1U)

#define S_CCTRL_ECN    27
#define V_CCTRL_ECN(x) ((x) << S_CCTRL_ECN)

#define S_SACK_EN    30
#define V_SACK_EN(x) ((x) << S_SACK_EN)

#define S_T5_OPT_2_VALID    31
#define V_T5_OPT_2_VALID(x) ((x) << S_T5_OPT_2_VALID)
#define F_T5_OPT_2_VALID    V_T5_OPT_2_VALID(1U)

struct cpl_t6_act_open_req {
	WR_HDR;
	union opcode_tid ot;
	__be16 local_port;
	__be16 peer_port;
	__be32 local_ip;
	__be32 peer_ip;
	__be64 opt0;
	__be32 rsvd;
	__be32 opt2;
	__be64 params;
	__be32 rsvd2;
	__be32 opt3;
};

struct cpl_t6_act_open_req6 {
	WR_HDR;
	union opcode_tid ot;
	__be16 local_port;
	__be16 peer_port;
	__be64 local_ip_hi;
	__be64 local_ip_lo;
	__be64 peer_ip_hi;
	__be64 peer_ip_lo;
	__be64 opt0;
	__be32 rsvd;
	__be32 opt2;
	__be64 params;
	__be32 rsvd2;
	__be32 opt3;
};

#define S_FILTER_TUPLE	24
#define V_FILTER_TUPLE(x) ((x) << S_FILTER_TUPLE)

struct cpl_act_open_rpl {
	RSS_HDR
	union opcode_tid ot;
	__be32 atid_status;
};

/* cpl_act_open_rpl.atid_status fields */
#define S_AOPEN_STATUS    0
#define M_AOPEN_STATUS    0xFF
#define G_AOPEN_STATUS(x) (((x) >> S_AOPEN_STATUS) & M_AOPEN_STATUS)

#define S_AOPEN_ATID    8
#define M_AOPEN_ATID    0xFFFFFF
#define G_AOPEN_ATID(x) (((x) >> S_AOPEN_ATID) & M_AOPEN_ATID)

struct cpl_set_tcb_field {
	WR_HDR;
	union opcode_tid ot;
	__be16 reply_ctrl;
	__be16 word_cookie;
	__be64 mask;
	__be64 val;
};

/* cpl_set_tcb_field.word_cookie fields */
#define S_WORD    0
#define V_WORD(x) ((x) << S_WORD)

/* cpl_get_tcb.reply_ctrl fields */
#define S_QUEUENO    0
#define V_QUEUENO(x) ((x) << S_QUEUENO)

#define S_REPLY_CHAN    14
#define V_REPLY_CHAN(x) ((x) << S_REPLY_CHAN)

#define S_NO_REPLY    15
#define V_NO_REPLY(x) ((x) << S_NO_REPLY)

struct cpl_set_tcb_rpl {
	RSS_HDR
	union opcode_tid ot;
	__be16 rsvd;
	__u8   cookie;
	__u8   status;
	__be64 oldval;
};

/* cpl_abort_req status command code
 */
struct cpl_abort_req {
	WR_HDR;
	union opcode_tid ot;
	__be32 rsvd0;
	__u8  rsvd1;
	__u8  cmd;
	__u8  rsvd2[6];
};

struct cpl_abort_rpl_rss {
	RSS_HDR
	union opcode_tid ot;
	__u8  rsvd[3];
	__u8  status;
};

struct cpl_abort_rpl {
	WR_HDR;
	union opcode_tid ot;
	__be32 rsvd0;
	__u8  rsvd1;
	__u8  cmd;
	__u8  rsvd2[6];
};

struct cpl_tid_release {
	WR_HDR;
	union opcode_tid ot;
	__be32 rsvd;
};

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

struct cpl_l2t_write_req {
	WR_HDR;
	union opcode_tid ot;
	__be16 params;
	__be16 l2t_idx;
	__be16 vlan;
	__u8   dst_mac[6];
};

/* cpl_l2t_write_req.params fields */
#define S_L2T_W_PORT    8
#define V_L2T_W_PORT(x) ((x) << S_L2T_W_PORT)

#define S_L2T_W_LPBK    10
#define V_L2T_W_LPBK(x) ((x) << S_L2T_W_LPBK)

#define S_L2T_W_ARPMISS         11
#define V_L2T_W_ARPMISS(x)      ((x) << S_L2T_W_ARPMISS)

#define S_L2T_W_NOREPLY    15
#define V_L2T_W_NOREPLY(x) ((x) << S_L2T_W_NOREPLY)

struct cpl_l2t_write_rpl {
	RSS_HDR
	union opcode_tid ot;
	__u8 status;
	__u8 rsvd[3];
};

struct cpl_smt_write_req {
	WR_HDR;
	union opcode_tid ot;
	__be32 params;
	__be16 pfvf1;
	__u8   src_mac1[6];
	__be16 pfvf0;
	__u8   src_mac0[6];
};

struct cpl_t6_smt_write_req {
	WR_HDR;
	union opcode_tid ot;
	__be32 params;
	__be64 tag;
	__be16 pfvf0;
	__u8   src_mac0[6];
	__be32 local_ip;
	__be32 rsvd;
};

struct cpl_smt_write_rpl {
	RSS_HDR
	union opcode_tid ot;
	u8 status;
	u8 rsvd[3];
};

/* cpl_smt_{read,write}_req.params fields */
#define S_SMTW_OVLAN_IDX    16
#define V_SMTW_OVLAN_IDX(x) ((x) << S_SMTW_OVLAN_IDX)

#define S_SMTW_IDX    20
#define V_SMTW_IDX(x) ((x) << S_SMTW_IDX)

#define S_SMTW_NORPL    31
#define V_SMTW_NORPL(x) ((x) << S_SMTW_NORPL)

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

/* ULP_TX opcodes */
enum {
	ULP_TX_PKT = 4
};

enum {
	ULP_TX_SC_NOOP = 0x80,
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
