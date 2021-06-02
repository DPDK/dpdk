/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _T4FW_INTERFACE_H_
#define _T4FW_INTERFACE_H_

/******************************************************************************
 *   R E T U R N   V A L U E S
 ********************************/

enum fw_retval {
	FW_SUCCESS		= 0,	/* completed successfully */
	FW_EPERM		= 1,	/* operation not permitted */
	FW_ENOENT		= 2,	/* no such file or directory */
	FW_EIO			= 5,	/* input/output error; hw bad */
	FW_ENOEXEC		= 8,	/* exec format error; inv microcode */
	FW_EAGAIN		= 11,	/* try again */
	FW_ENOMEM		= 12,	/* out of memory */
	FW_EFAULT		= 14,	/* bad address; fw bad */
	FW_EBUSY		= 16,	/* resource busy */
	FW_EEXIST		= 17,	/* file exists */
	FW_ENODEV		= 19,	/* no such device */
	FW_EINVAL		= 22,	/* invalid argument */
	FW_ENOSPC		= 28,	/* no space left on device */
	FW_ENOSYS		= 38,	/* functionality not implemented */
	FW_ENODATA		= 61,	/* no data available */
	FW_EPROTO		= 71,	/* protocol error */
	FW_EADDRINUSE		= 98,	/* address already in use */
	FW_EADDRNOTAVAIL	= 99,	/* cannot assigned requested address */
	FW_ENETDOWN		= 100,	/* network is down */
	FW_ENETUNREACH		= 101,	/* network is unreachable */
	FW_ENOBUFS		= 105,	/* no buffer space available */
	FW_ETIMEDOUT		= 110,	/* timeout */
	FW_EINPROGRESS		= 115,	/* fw internal */
};

/******************************************************************************
 *   M E M O R Y   T Y P E s
 ******************************/

enum fw_memtype {
	FW_MEMTYPE_EDC0		= 0x0,
	FW_MEMTYPE_EDC1		= 0x1,
	FW_MEMTYPE_EXTMEM	= 0x2,
	FW_MEMTYPE_FLASH	= 0x4,
	FW_MEMTYPE_INTERNAL	= 0x5,
	FW_MEMTYPE_EXTMEM1	= 0x6,
};

/******************************************************************************
 *   W O R K   R E Q U E S T s
 ********************************/

enum fw_wr_opcodes {
	FW_FILTER_WR		= 0x02,
	FW_ULPTX_WR		= 0x04,
	FW_TP_WR		= 0x05,
	FW_ETH_TX_PKT_WR	= 0x08,
	FW_ETH_TX_PKTS_WR	= 0x09,
	FW_ETH_TX_PKT_VM_WR	= 0x11,
	FW_ETH_TX_PKTS_VM_WR	= 0x12,
	FW_FILTER2_WR		= 0x77,
	FW_ETH_TX_PKTS2_WR      = 0x78,
};

/*
 * Generic work request header flit0
 */
struct fw_wr_hdr {
	__be32 hi;
	__be32 lo;
};

/* work request opcode (hi)
 */
#define S_FW_WR_OP		24
#define M_FW_WR_OP		0xff
#define V_FW_WR_OP(x)		((x) << S_FW_WR_OP)
#define G_FW_WR_OP(x)		(((x) >> S_FW_WR_OP) & M_FW_WR_OP)

/* atomic flag (hi) - firmware encapsulates CPLs in CPL_BARRIER
 */
#define S_FW_WR_ATOMIC		23
#define V_FW_WR_ATOMIC(x)	((x) << S_FW_WR_ATOMIC)

/* work request immediate data length (hi)
 */
#define S_FW_WR_IMMDLEN	0
#define M_FW_WR_IMMDLEN	0xff
#define V_FW_WR_IMMDLEN(x)	((x) << S_FW_WR_IMMDLEN)
#define G_FW_WR_IMMDLEN(x)	\
	(((x) >> S_FW_WR_IMMDLEN) & M_FW_WR_IMMDLEN)

/* egress queue status update to egress queue status entry (lo)
 */
#define S_FW_WR_EQUEQ		30
#define M_FW_WR_EQUEQ		0x1
#define V_FW_WR_EQUEQ(x)	((x) << S_FW_WR_EQUEQ)
#define G_FW_WR_EQUEQ(x)	(((x) >> S_FW_WR_EQUEQ) & M_FW_WR_EQUEQ)
#define F_FW_WR_EQUEQ		V_FW_WR_EQUEQ(1U)

/* flow context identifier (lo)
 */
#define S_FW_WR_FLOWID		8
#define V_FW_WR_FLOWID(x)	((x) << S_FW_WR_FLOWID)

/* length in units of 16-bytes (lo)
 */
#define S_FW_WR_LEN16		0
#define M_FW_WR_LEN16		0xff
#define V_FW_WR_LEN16(x)	((x) << S_FW_WR_LEN16)
#define G_FW_WR_LEN16(x)	(((x) >> S_FW_WR_LEN16) & M_FW_WR_LEN16)

struct fw_eth_tx_pkt_wr {
	__be32 op_immdlen;
	__be32 equiq_to_len16;
	__be64 r3;
};

#define S_FW_ETH_TX_PKT_WR_IMMDLEN	0
#define M_FW_ETH_TX_PKT_WR_IMMDLEN	0x1ff
#define V_FW_ETH_TX_PKT_WR_IMMDLEN(x)	((x) << S_FW_ETH_TX_PKT_WR_IMMDLEN)
#define G_FW_ETH_TX_PKT_WR_IMMDLEN(x)	\
	(((x) >> S_FW_ETH_TX_PKT_WR_IMMDLEN) & M_FW_ETH_TX_PKT_WR_IMMDLEN)

struct fw_eth_tx_pkts_wr {
	__be32 op_pkd;
	__be32 equiq_to_len16;
	__be32 r3;
	__be16 plen;
	__u8   npkt;
	__u8   type;
};

struct fw_eth_tx_pkt_vm_wr {
	__be32 op_immdlen;
	__be32 equiq_to_len16;
	__be32 r3[2];
	__u8   ethmacdst[6];
	__u8   ethmacsrc[6];
	__be16 ethtype;
	__be16 vlantci;
};

struct fw_eth_tx_pkts_vm_wr {
	__be32 op_pkd;
	__be32 equiq_to_len16;
	__be32 r3;
	__be16 plen;
	__u8   npkt;
	__u8   r4;
	__u8   ethmacdst[6];
	__u8   ethmacsrc[6];
	__be16 ethtype;
	__be16 vlantci;
};

/* filter wr reply code in cookie in CPL_SET_TCB_RPL */
enum fw_filter_wr_cookie {
	FW_FILTER_WR_SUCCESS,
	FW_FILTER_WR_FLT_ADDED,
	FW_FILTER_WR_FLT_DELETED,
	FW_FILTER_WR_SMT_TBL_FULL,
	FW_FILTER_WR_EINVAL,
};

struct fw_filter2_wr {
	__be32 op_pkd;
	__be32 len16_pkd;
	__be64 r3;
	__be32 tid_to_iq;
	__be32 del_filter_to_l2tix;
	__be16 ethtype;
	__be16 ethtypem;
	__u8   frag_to_ovlan_vldm;
	__u8   smac_sel;
	__be16 rx_chan_rx_rpl_iq;
	__be32 maci_to_matchtypem;
	__u8   ptcl;
	__u8   ptclm;
	__u8   ttyp;
	__u8   ttypm;
	__be16 ivlan;
	__be16 ivlanm;
	__be16 ovlan;
	__be16 ovlanm;
	__u8   lip[16];
	__u8   lipm[16];
	__u8   fip[16];
	__u8   fipm[16];
	__be16 lp;
	__be16 lpm;
	__be16 fp;
	__be16 fpm;
	__be16 r7;
	__u8   sma[6];
	__be16 r8;
	__u8   filter_type_swapmac;
	__u8   natmode_to_ulp_type;
	__be16 newlport;
	__be16 newfport;
	__u8   newlip[16];
	__u8   newfip[16];
	__be32 natseqcheck;
	__be32 r9;
	__be64 r10;
	__be64 r11;
	__be64 r12;
	__be64 r13;
};

#define S_FW_FILTER_WR_TID	12
#define V_FW_FILTER_WR_TID(x)	((x) << S_FW_FILTER_WR_TID)

#define S_FW_FILTER_WR_RQTYPE		11
#define V_FW_FILTER_WR_RQTYPE(x)	((x) << S_FW_FILTER_WR_RQTYPE)

#define S_FW_FILTER_WR_NOREPLY		10
#define V_FW_FILTER_WR_NOREPLY(x)	((x) << S_FW_FILTER_WR_NOREPLY)

#define S_FW_FILTER_WR_IQ	0
#define V_FW_FILTER_WR_IQ(x)	((x) << S_FW_FILTER_WR_IQ)

#define S_FW_FILTER_WR_DEL_FILTER	31
#define V_FW_FILTER_WR_DEL_FILTER(x)	((x) << S_FW_FILTER_WR_DEL_FILTER)
#define F_FW_FILTER_WR_DEL_FILTER	V_FW_FILTER_WR_DEL_FILTER(1U)

#define S_FW_FILTER_WR_RPTTID		25
#define V_FW_FILTER_WR_RPTTID(x)	((x) << S_FW_FILTER_WR_RPTTID)

#define S_FW_FILTER_WR_DROP	24
#define V_FW_FILTER_WR_DROP(x)	((x) << S_FW_FILTER_WR_DROP)

#define S_FW_FILTER_WR_DIRSTEER		23
#define V_FW_FILTER_WR_DIRSTEER(x)	((x) << S_FW_FILTER_WR_DIRSTEER)

#define S_FW_FILTER_WR_MASKHASH		22
#define V_FW_FILTER_WR_MASKHASH(x)	((x) << S_FW_FILTER_WR_MASKHASH)

#define S_FW_FILTER_WR_DIRSTEERHASH	21
#define V_FW_FILTER_WR_DIRSTEERHASH(x)	((x) << S_FW_FILTER_WR_DIRSTEERHASH)

#define S_FW_FILTER_WR_LPBK	20
#define V_FW_FILTER_WR_LPBK(x)	((x) << S_FW_FILTER_WR_LPBK)

#define S_FW_FILTER_WR_DMAC	19
#define V_FW_FILTER_WR_DMAC(x)	((x) << S_FW_FILTER_WR_DMAC)

#define S_FW_FILTER_WR_SMAC     18
#define V_FW_FILTER_WR_SMAC(x)  ((x) << S_FW_FILTER_WR_SMAC)

#define S_FW_FILTER_WR_INSVLAN		17
#define V_FW_FILTER_WR_INSVLAN(x)	((x) << S_FW_FILTER_WR_INSVLAN)

#define S_FW_FILTER_WR_RMVLAN		16
#define V_FW_FILTER_WR_RMVLAN(x)	((x) << S_FW_FILTER_WR_RMVLAN)

#define S_FW_FILTER_WR_HITCNTS		15
#define V_FW_FILTER_WR_HITCNTS(x)	((x) << S_FW_FILTER_WR_HITCNTS)

#define S_FW_FILTER_WR_TXCHAN		13
#define V_FW_FILTER_WR_TXCHAN(x)	((x) << S_FW_FILTER_WR_TXCHAN)

#define S_FW_FILTER_WR_PRIO	12
#define V_FW_FILTER_WR_PRIO(x)	((x) << S_FW_FILTER_WR_PRIO)

#define S_FW_FILTER_WR_L2TIX	0
#define V_FW_FILTER_WR_L2TIX(x)	((x) << S_FW_FILTER_WR_L2TIX)

#define S_FW_FILTER_WR_FRAG	7
#define V_FW_FILTER_WR_FRAG(x)	((x) << S_FW_FILTER_WR_FRAG)

#define S_FW_FILTER_WR_FRAGM	6
#define V_FW_FILTER_WR_FRAGM(x)	((x) << S_FW_FILTER_WR_FRAGM)

#define S_FW_FILTER_WR_IVLAN_VLD	5
#define V_FW_FILTER_WR_IVLAN_VLD(x)	((x) << S_FW_FILTER_WR_IVLAN_VLD)

#define S_FW_FILTER_WR_OVLAN_VLD	4
#define V_FW_FILTER_WR_OVLAN_VLD(x)	((x) << S_FW_FILTER_WR_OVLAN_VLD)

#define S_FW_FILTER_WR_IVLAN_VLDM	3
#define V_FW_FILTER_WR_IVLAN_VLDM(x)	((x) << S_FW_FILTER_WR_IVLAN_VLDM)

#define S_FW_FILTER_WR_OVLAN_VLDM	2
#define V_FW_FILTER_WR_OVLAN_VLDM(x)	((x) << S_FW_FILTER_WR_OVLAN_VLDM)

#define S_FW_FILTER_WR_RX_CHAN		15
#define V_FW_FILTER_WR_RX_CHAN(x)	((x) << S_FW_FILTER_WR_RX_CHAN)

#define S_FW_FILTER_WR_RX_RPL_IQ	0
#define V_FW_FILTER_WR_RX_RPL_IQ(x)	((x) << S_FW_FILTER_WR_RX_RPL_IQ)

#define S_FW_FILTER_WR_MACI	23
#define V_FW_FILTER_WR_MACI(x)	((x) << S_FW_FILTER_WR_MACI)

#define S_FW_FILTER_WR_MACIM	14
#define V_FW_FILTER_WR_MACIM(x)	((x) << S_FW_FILTER_WR_MACIM)

#define S_FW_FILTER_WR_FCOE	13
#define V_FW_FILTER_WR_FCOE(x)	((x) << S_FW_FILTER_WR_FCOE)

#define S_FW_FILTER_WR_FCOEM	12
#define V_FW_FILTER_WR_FCOEM(x)	((x) << S_FW_FILTER_WR_FCOEM)

#define S_FW_FILTER_WR_PORT	9
#define V_FW_FILTER_WR_PORT(x)	((x) << S_FW_FILTER_WR_PORT)

#define S_FW_FILTER_WR_PORTM	6
#define V_FW_FILTER_WR_PORTM(x)	((x) << S_FW_FILTER_WR_PORTM)

#define S_FW_FILTER_WR_MATCHTYPE	3
#define V_FW_FILTER_WR_MATCHTYPE(x)	((x) << S_FW_FILTER_WR_MATCHTYPE)

#define S_FW_FILTER_WR_MATCHTYPEM	0
#define V_FW_FILTER_WR_MATCHTYPEM(x)	((x) << S_FW_FILTER_WR_MATCHTYPEM)

#define S_FW_FILTER2_WR_SWAPMAC		0
#define V_FW_FILTER2_WR_SWAPMAC(x)	((x) << S_FW_FILTER2_WR_SWAPMAC)

#define S_FW_FILTER2_WR_NATMODE		5
#define V_FW_FILTER2_WR_NATMODE(x)	((x) << S_FW_FILTER2_WR_NATMODE)

#define S_FW_FILTER2_WR_ULP_TYPE	0
#define V_FW_FILTER2_WR_ULP_TYPE(x)	((x) << S_FW_FILTER2_WR_ULP_TYPE)

/******************************************************************************
 *  C O M M A N D s
 *********************/

/*
 * The maximum length of time, in miliseconds, that we expect any firmware
 * command to take to execute and return a reply to the host.  The RESET
 * and INITIALIZE commands can take a fair amount of time to execute but
 * most execute in far less time than this maximum.  This constant is used
 * by host software to determine how long to wait for a firmware command
 * reply before declaring the firmware as dead/unreachable ...
 */
#define FW_CMD_MAX_TIMEOUT	10000

/*
 * If a host driver does a HELLO and discovers that there's already a MASTER
 * selected, we may have to wait for that MASTER to finish issuing RESET,
 * configuration and INITIALIZE commands.  Also, there's a possibility that
 * our own HELLO may get lost if it happens right as the MASTER is issuign a
 * RESET command, so we need to be willing to make a few retries of our HELLO.
 */
#define FW_CMD_HELLO_TIMEOUT	(3 * FW_CMD_MAX_TIMEOUT)
#define FW_CMD_HELLO_RETRIES	3

enum fw_cmd_opcodes {
	FW_LDST_CMD		       = 0x01,
	FW_RESET_CMD                   = 0x03,
	FW_HELLO_CMD                   = 0x04,
	FW_BYE_CMD                     = 0x05,
	FW_INITIALIZE_CMD              = 0x06,
	FW_CAPS_CONFIG_CMD             = 0x07,
	FW_PARAMS_CMD                  = 0x08,
	FW_PFVF_CMD		       = 0x09,
	FW_IQ_CMD                      = 0x10,
	FW_EQ_ETH_CMD                  = 0x12,
	FW_EQ_CTRL_CMD                 = 0x13,
	FW_VI_CMD                      = 0x14,
	FW_VI_MAC_CMD                  = 0x15,
	FW_VI_RXMODE_CMD               = 0x16,
	FW_VI_ENABLE_CMD               = 0x17,
	FW_VI_STATS_CMD		       = 0x1a,
	FW_PORT_CMD                    = 0x1b,
	FW_RSS_IND_TBL_CMD             = 0x20,
	FW_RSS_GLB_CONFIG_CMD	       = 0x22,
	FW_RSS_VI_CONFIG_CMD           = 0x23,
	FW_CLIP_CMD                    = 0x28,
	FW_DEBUG_CMD                   = 0x81,
};

enum fw_cmd_cap {
	FW_CMD_CAP_PORT		= 0x04,
};

/*
 * Generic command header flit0
 */
struct fw_cmd_hdr {
	__be32 hi;
	__be32 lo;
};

#define S_FW_CMD_OP		24
#define M_FW_CMD_OP		0xff
#define V_FW_CMD_OP(x)		((x) << S_FW_CMD_OP)
#define G_FW_CMD_OP(x)		(((x) >> S_FW_CMD_OP) & M_FW_CMD_OP)

#define S_FW_CMD_REQUEST	23
#define M_FW_CMD_REQUEST	0x1
#define V_FW_CMD_REQUEST(x)	((x) << S_FW_CMD_REQUEST)
#define G_FW_CMD_REQUEST(x)	(((x) >> S_FW_CMD_REQUEST) & M_FW_CMD_REQUEST)
#define F_FW_CMD_REQUEST	V_FW_CMD_REQUEST(1U)

#define S_FW_CMD_READ		22
#define M_FW_CMD_READ		0x1
#define V_FW_CMD_READ(x)	((x) << S_FW_CMD_READ)
#define G_FW_CMD_READ(x)	(((x) >> S_FW_CMD_READ) & M_FW_CMD_READ)
#define F_FW_CMD_READ		V_FW_CMD_READ(1U)

#define S_FW_CMD_WRITE		21
#define M_FW_CMD_WRITE		0x1
#define V_FW_CMD_WRITE(x)	((x) << S_FW_CMD_WRITE)
#define G_FW_CMD_WRITE(x)	(((x) >> S_FW_CMD_WRITE) & M_FW_CMD_WRITE)
#define F_FW_CMD_WRITE		V_FW_CMD_WRITE(1U)

#define S_FW_CMD_EXEC		20
#define M_FW_CMD_EXEC		0x1
#define V_FW_CMD_EXEC(x)	((x) << S_FW_CMD_EXEC)
#define G_FW_CMD_EXEC(x)	(((x) >> S_FW_CMD_EXEC) & M_FW_CMD_EXEC)
#define F_FW_CMD_EXEC		V_FW_CMD_EXEC(1U)

#define S_FW_CMD_RETVAL		8
#define M_FW_CMD_RETVAL		0xff
#define V_FW_CMD_RETVAL(x)	((x) << S_FW_CMD_RETVAL)
#define G_FW_CMD_RETVAL(x)	(((x) >> S_FW_CMD_RETVAL) & M_FW_CMD_RETVAL)

#define S_FW_CMD_LEN16		0
#define M_FW_CMD_LEN16		0xff
#define V_FW_CMD_LEN16(x)	((x) << S_FW_CMD_LEN16)
#define G_FW_CMD_LEN16(x)	(((x) >> S_FW_CMD_LEN16) & M_FW_CMD_LEN16)

#define FW_LEN16(fw_struct) V_FW_CMD_LEN16(sizeof(fw_struct) / 16)

/* address spaces
 */
enum fw_ldst_addrspc {
	FW_LDST_ADDRSPC_TP_PIO    = 0x0010,
};

struct fw_ldst_cmd {
	__be32 op_to_addrspace;
	__be32 cycles_to_len16;
	union fw_ldst {
		struct fw_ldst_addrval {
			__be32 addr;
			__be32 val;
		} addrval;
		struct fw_ldst_idctxt {
			__be32 physid;
			__be32 msg_ctxtflush;
			__be32 ctxt_data7;
			__be32 ctxt_data6;
			__be32 ctxt_data5;
			__be32 ctxt_data4;
			__be32 ctxt_data3;
			__be32 ctxt_data2;
			__be32 ctxt_data1;
			__be32 ctxt_data0;
		} idctxt;
		struct fw_ldst_mdio {
			__be16 paddr_mmd;
			__be16 raddr;
			__be16 vctl;
			__be16 rval;
		} mdio;
		struct fw_ldst_mps {
			__be16 fid_ctl;
			__be16 rplcpf_pkd;
			__be32 rplc127_96;
			__be32 rplc95_64;
			__be32 rplc63_32;
			__be32 rplc31_0;
			__be32 atrb;
			__be16 vlan[16];
		} mps;
		struct fw_ldst_func {
			__u8   access_ctl;
			__u8   mod_index;
			__be16 ctl_id;
			__be32 offset;
			__be64 data0;
			__be64 data1;
		} func;
		struct fw_ldst_pcie {
			__u8   ctrl_to_fn;
			__u8   bnum;
			__u8   r;
			__u8   ext_r;
			__u8   select_naccess;
			__u8   pcie_fn;
			__be16 nset_pkd;
			__be32 data[12];
		} pcie;
		struct fw_ldst_i2c_deprecated {
			__u8   pid_pkd;
			__u8   base;
			__u8   boffset;
			__u8   data;
			__be32 r9;
		} i2c_deprecated;
		struct fw_ldst_i2c {
			__u8   pid;
			__u8   did;
			__u8   boffset;
			__u8   blen;
			__be32 r9;
			__u8   data[48];
		} i2c;
		struct fw_ldst_le {
			__be32 index;
			__be32 r9;
			__u8   val[33];
			__u8   r11[7];
		} le;
	} u;
};

#define S_FW_LDST_CMD_ADDRSPACE         0
#define M_FW_LDST_CMD_ADDRSPACE         0xff
#define V_FW_LDST_CMD_ADDRSPACE(x)      ((x) << S_FW_LDST_CMD_ADDRSPACE)

struct fw_reset_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be32 val;
	__be32 halt_pkd;
};

#define S_FW_RESET_CMD_HALT	31
#define M_FW_RESET_CMD_HALT	0x1
#define V_FW_RESET_CMD_HALT(x)	((x) << S_FW_RESET_CMD_HALT)
#define G_FW_RESET_CMD_HALT(x)	\
	(((x) >> S_FW_RESET_CMD_HALT) & M_FW_RESET_CMD_HALT)
#define F_FW_RESET_CMD_HALT	V_FW_RESET_CMD_HALT(1U)

enum {
	FW_HELLO_CMD_STAGE_OS		= 0,
};

struct fw_hello_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be32 err_to_clearinit;
	__be32 fwrev;
};

#define S_FW_HELLO_CMD_ERR	31
#define M_FW_HELLO_CMD_ERR	0x1
#define V_FW_HELLO_CMD_ERR(x)	((x) << S_FW_HELLO_CMD_ERR)
#define G_FW_HELLO_CMD_ERR(x)	\
	(((x) >> S_FW_HELLO_CMD_ERR) & M_FW_HELLO_CMD_ERR)
#define F_FW_HELLO_CMD_ERR	V_FW_HELLO_CMD_ERR(1U)

#define S_FW_HELLO_CMD_INIT	30
#define M_FW_HELLO_CMD_INIT	0x1
#define V_FW_HELLO_CMD_INIT(x)	((x) << S_FW_HELLO_CMD_INIT)
#define G_FW_HELLO_CMD_INIT(x)	\
	(((x) >> S_FW_HELLO_CMD_INIT) & M_FW_HELLO_CMD_INIT)
#define F_FW_HELLO_CMD_INIT	V_FW_HELLO_CMD_INIT(1U)

#define S_FW_HELLO_CMD_MASTERDIS	29
#define M_FW_HELLO_CMD_MASTERDIS	0x1
#define V_FW_HELLO_CMD_MASTERDIS(x)	((x) << S_FW_HELLO_CMD_MASTERDIS)
#define G_FW_HELLO_CMD_MASTERDIS(x)	\
	(((x) >> S_FW_HELLO_CMD_MASTERDIS) & M_FW_HELLO_CMD_MASTERDIS)
#define F_FW_HELLO_CMD_MASTERDIS	V_FW_HELLO_CMD_MASTERDIS(1U)

#define S_FW_HELLO_CMD_MASTERFORCE	28
#define M_FW_HELLO_CMD_MASTERFORCE	0x1
#define V_FW_HELLO_CMD_MASTERFORCE(x)	((x) << S_FW_HELLO_CMD_MASTERFORCE)
#define G_FW_HELLO_CMD_MASTERFORCE(x)	\
	(((x) >> S_FW_HELLO_CMD_MASTERFORCE) & M_FW_HELLO_CMD_MASTERFORCE)
#define F_FW_HELLO_CMD_MASTERFORCE	V_FW_HELLO_CMD_MASTERFORCE(1U)

#define S_FW_HELLO_CMD_MBMASTER		24
#define M_FW_HELLO_CMD_MBMASTER		0xf
#define V_FW_HELLO_CMD_MBMASTER(x)	((x) << S_FW_HELLO_CMD_MBMASTER)
#define G_FW_HELLO_CMD_MBMASTER(x)	\
	(((x) >> S_FW_HELLO_CMD_MBMASTER) & M_FW_HELLO_CMD_MBMASTER)

#define S_FW_HELLO_CMD_MBASYNCNOT	20
#define M_FW_HELLO_CMD_MBASYNCNOT	0x7
#define V_FW_HELLO_CMD_MBASYNCNOT(x)	((x) << S_FW_HELLO_CMD_MBASYNCNOT)
#define G_FW_HELLO_CMD_MBASYNCNOT(x)	\
	(((x) >> S_FW_HELLO_CMD_MBASYNCNOT) & M_FW_HELLO_CMD_MBASYNCNOT)

#define S_FW_HELLO_CMD_STAGE	17
#define M_FW_HELLO_CMD_STAGE	0x7
#define V_FW_HELLO_CMD_STAGE(x)	((x) << S_FW_HELLO_CMD_STAGE)
#define G_FW_HELLO_CMD_STAGE(x)	\
	(((x) >> S_FW_HELLO_CMD_STAGE) & M_FW_HELLO_CMD_STAGE)

#define S_FW_HELLO_CMD_CLEARINIT	16
#define M_FW_HELLO_CMD_CLEARINIT	0x1
#define V_FW_HELLO_CMD_CLEARINIT(x)	((x) << S_FW_HELLO_CMD_CLEARINIT)
#define G_FW_HELLO_CMD_CLEARINIT(x)	\
	(((x) >> S_FW_HELLO_CMD_CLEARINIT) & M_FW_HELLO_CMD_CLEARINIT)
#define F_FW_HELLO_CMD_CLEARINIT	V_FW_HELLO_CMD_CLEARINIT(1U)

struct fw_bye_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be64 r3;
};

struct fw_initialize_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be64 r3;
};

enum fw_caps_config_nic {
	FW_CAPS_CONFIG_NIC_HASHFILTER	= 0x00000020,
	FW_CAPS_CONFIG_NIC_ETHOFLD	= 0x00000040,
};

enum fw_memtype_cf {
	FW_MEMTYPE_CF_FLASH		= FW_MEMTYPE_FLASH,
};

struct fw_caps_config_cmd {
	__be32 op_to_write;
	__be32 cfvalid_to_len16;
	__be32 r2;
	__be32 hwmbitmap;
	__be16 nbmcaps;
	__be16 linkcaps;
	__be16 switchcaps;
	__be16 r3;
	__be16 niccaps;
	__be16 toecaps;
	__be16 rdmacaps;
	__be16 cryptocaps;
	__be16 iscsicaps;
	__be16 fcoecaps;
	__be32 cfcsum;
	__be32 finiver;
	__be32 finicsum;
};

#define S_FW_CAPS_CONFIG_CMD_CFVALID	27
#define M_FW_CAPS_CONFIG_CMD_CFVALID	0x1
#define V_FW_CAPS_CONFIG_CMD_CFVALID(x)	((x) << S_FW_CAPS_CONFIG_CMD_CFVALID)
#define G_FW_CAPS_CONFIG_CMD_CFVALID(x)	\
	(((x) >> S_FW_CAPS_CONFIG_CMD_CFVALID) & M_FW_CAPS_CONFIG_CMD_CFVALID)
#define F_FW_CAPS_CONFIG_CMD_CFVALID	V_FW_CAPS_CONFIG_CMD_CFVALID(1U)

#define S_FW_CAPS_CONFIG_CMD_MEMTYPE_CF		24
#define M_FW_CAPS_CONFIG_CMD_MEMTYPE_CF		0x7
#define V_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(x)	\
	((x) << S_FW_CAPS_CONFIG_CMD_MEMTYPE_CF)
#define G_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(x)	\
	(((x) >> S_FW_CAPS_CONFIG_CMD_MEMTYPE_CF) & \
	 M_FW_CAPS_CONFIG_CMD_MEMTYPE_CF)

#define S_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF	16
#define M_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF	0xff
#define V_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(x)	\
	((x) << S_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF)
#define G_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(x)	\
	(((x) >> S_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF) & \
	 M_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF)

/*
 * params command mnemonics
 */
enum fw_params_mnem {
	FW_PARAMS_MNEM_DEV		= 1,	/* device params */
	FW_PARAMS_MNEM_PFVF		= 2,	/* function params */
	FW_PARAMS_MNEM_REG		= 3,	/* limited register access */
	FW_PARAMS_MNEM_DMAQ		= 4,	/* dma queue params */
};

/*
 * device parameters
 */

#define S_FW_PARAMS_PARAM_FILTER_MODE 16
#define M_FW_PARAMS_PARAM_FILTER_MODE 0xffff
#define V_FW_PARAMS_PARAM_FILTER_MODE(x)          \
	((x) << S_FW_PARAMS_PARAM_FILTER_MODE)
#define G_FW_PARAMS_PARAM_FILTER_MODE(x)          \
	(((x) >> S_FW_PARAMS_PARAM_FILTER_MODE) & \
	M_FW_PARAMS_PARAM_FILTER_MODE)

#define S_FW_PARAMS_PARAM_FILTER_MASK 0
#define M_FW_PARAMS_PARAM_FILTER_MASK 0xffff
#define V_FW_PARAMS_PARAM_FILTER_MASK(x)          \
	((x) << S_FW_PARAMS_PARAM_FILTER_MASK)
#define G_FW_PARAMS_PARAM_FILTER_MASK(x)          \
	(((x) >> S_FW_PARAMS_PARAM_FILTER_MASK) & \
	M_FW_PARAMS_PARAM_FILTER_MASK)

enum fw_params_param_dev {
	FW_PARAMS_PARAM_DEV_CCLK	= 0x00, /* chip core clock in khz */
	FW_PARAMS_PARAM_DEV_PORTVEC	= 0x01, /* the port vector */
	FW_PARAMS_PARAM_DEV_NTID        = 0x02, /* reads the number of TIDs
						 * allocated by the device's
						 * Lookup Engine
						 */
	FW_PARAMS_PARAM_DEV_FWREV	= 0x0B, /* fw version */
	FW_PARAMS_PARAM_DEV_TPREV	= 0x0C, /* tp version */
	FW_PARAMS_PARAM_DEV_ULPTX_MEMWRITE_DSGL = 0x17,
	FW_PARAMS_PARAM_DEV_FILTER2_WR	= 0x1D,
	FW_PARAMS_PARAM_DEV_OPAQUE_VIID_SMT_EXTN = 0x27,
	FW_PARAMS_PARAM_DEV_HASHFILTER_WITH_OFLD = 0x28,
	FW_PARAMS_PARAM_DEV_FILTER      = 0x2E,
	FW_PARAMS_PARAM_DEV_VI_ENABLE_INGRESS_AFTER_LINKUP = 0x32,
	FW_PARAMS_PARAM_PFVF_RAWF_START = 0x36,
	FW_PARAMS_PARAM_PFVF_RAWF_END   = 0x37,
};

/*
 * physical and virtual function parameters
 */
enum fw_params_param_pfvf {
	FW_PARAMS_PARAM_PFVF_CLIP_START = 0x03,
	FW_PARAMS_PARAM_PFVF_CLIP_END = 0x04,
	FW_PARAMS_PARAM_PFVF_FILTER_START = 0x05,
	FW_PARAMS_PARAM_PFVF_FILTER_END = 0x06,
	FW_PARAMS_PARAM_PFVF_L2T_START = 0x13,
	FW_PARAMS_PARAM_PFVF_L2T_END = 0x14,
	FW_PARAMS_PARAM_PFVF_CPLFW4MSG_ENCAP = 0x31,
	FW_PARAMS_PARAM_PFVF_PORT_CAPS32 = 0x3A,
	FW_PARAMS_PARAM_PFVF_MAX_PKTS_PER_ETH_TX_PKTS_WR = 0x3D,
	FW_PARAMS_PARAM_PFVF_GET_SMT_START = 0x3E,
	FW_PARAMS_PARAM_PFVF_GET_SMT_SIZE = 0x3F,
};

/*
 * dma queue parameters
 */
enum fw_params_param_dmaq {
	FW_PARAMS_PARAM_DMAQ_IQ_INTCNTTHRESH = 0x01,
	FW_PARAMS_PARAM_DMAQ_CONM_CTXT = 0x20,
};

enum fw_params_param_dev_filter {
	FW_PARAM_DEV_FILTER_VNIC_MODE   = 0x00,
	FW_PARAM_DEV_FILTER_MODE_MASK   = 0x01,
};

#define S_FW_PARAMS_MNEM	24
#define M_FW_PARAMS_MNEM	0xff
#define V_FW_PARAMS_MNEM(x)	((x) << S_FW_PARAMS_MNEM)
#define G_FW_PARAMS_MNEM(x)	\
	(((x) >> S_FW_PARAMS_MNEM) & M_FW_PARAMS_MNEM)

#define S_FW_PARAMS_PARAM_X	16
#define M_FW_PARAMS_PARAM_X	0xff
#define V_FW_PARAMS_PARAM_X(x) ((x) << S_FW_PARAMS_PARAM_X)
#define G_FW_PARAMS_PARAM_X(x) \
	(((x) >> S_FW_PARAMS_PARAM_X) & M_FW_PARAMS_PARAM_X)

#define S_FW_PARAMS_PARAM_Y	8
#define M_FW_PARAMS_PARAM_Y	0xff
#define V_FW_PARAMS_PARAM_Y(x) ((x) << S_FW_PARAMS_PARAM_Y)
#define G_FW_PARAMS_PARAM_Y(x) \
	(((x) >> S_FW_PARAMS_PARAM_Y) & M_FW_PARAMS_PARAM_Y)

#define S_FW_PARAMS_PARAM_Z	0
#define M_FW_PARAMS_PARAM_Z	0xff
#define V_FW_PARAMS_PARAM_Z(x) ((x) << S_FW_PARAMS_PARAM_Z)
#define G_FW_PARAMS_PARAM_Z(x) \
	(((x) >> S_FW_PARAMS_PARAM_Z) & M_FW_PARAMS_PARAM_Z)

#define S_FW_PARAMS_PARAM_YZ	0
#define M_FW_PARAMS_PARAM_YZ	0xffff
#define V_FW_PARAMS_PARAM_YZ(x) ((x) << S_FW_PARAMS_PARAM_YZ)
#define G_FW_PARAMS_PARAM_YZ(x) \
	(((x) >> S_FW_PARAMS_PARAM_YZ) & M_FW_PARAMS_PARAM_YZ)

#define S_FW_PARAMS_PARAM_XYZ		0
#define M_FW_PARAMS_PARAM_XYZ		0xffffff
#define V_FW_PARAMS_PARAM_XYZ(x)	((x) << S_FW_PARAMS_PARAM_XYZ)

struct fw_params_cmd {
	__be32 op_to_vfn;
	__be32 retval_len16;
	struct fw_params_param {
		__be32 mnem;
		__be32 val;
	} param[7];
};

#define S_FW_PARAMS_CMD_PFN	8
#define M_FW_PARAMS_CMD_PFN	0x7
#define V_FW_PARAMS_CMD_PFN(x)	((x) << S_FW_PARAMS_CMD_PFN)
#define G_FW_PARAMS_CMD_PFN(x)	\
	(((x) >> S_FW_PARAMS_CMD_PFN) & M_FW_PARAMS_CMD_PFN)

#define S_FW_PARAMS_CMD_VFN	0
#define M_FW_PARAMS_CMD_VFN	0xff
#define V_FW_PARAMS_CMD_VFN(x)	((x) << S_FW_PARAMS_CMD_VFN)
#define G_FW_PARAMS_CMD_VFN(x)	\
	(((x) >> S_FW_PARAMS_CMD_VFN) & M_FW_PARAMS_CMD_VFN)

struct fw_pfvf_cmd {
	__be32 op_to_vfn;
	__be32 retval_len16;
	__be32 niqflint_niq;
	__be32 type_to_neq;
	__be32 tc_to_nexactf;
	__be32 r_caps_to_nethctrl;
	__be16 nricq;
	__be16 nriqp;
	__be32 r4;
};

#define S_FW_PFVF_CMD_PFN		8
#define V_FW_PFVF_CMD_PFN(x)		((x) << S_FW_PFVF_CMD_PFN)

#define S_FW_PFVF_CMD_VFN		0
#define V_FW_PFVF_CMD_VFN(x)		((x) << S_FW_PFVF_CMD_VFN)

#define S_FW_PFVF_CMD_NIQFLINT          20
#define M_FW_PFVF_CMD_NIQFLINT          0xfff
#define G_FW_PFVF_CMD_NIQFLINT(x)       \
	(((x) >> S_FW_PFVF_CMD_NIQFLINT) & M_FW_PFVF_CMD_NIQFLINT)

#define S_FW_PFVF_CMD_NIQ               0
#define M_FW_PFVF_CMD_NIQ               0xfffff
#define G_FW_PFVF_CMD_NIQ(x)            \
	(((x) >> S_FW_PFVF_CMD_NIQ) & M_FW_PFVF_CMD_NIQ)

#define S_FW_PFVF_CMD_PMASK             20
#define M_FW_PFVF_CMD_PMASK             0xf
#define G_FW_PFVF_CMD_PMASK(x)          \
	(((x) >> S_FW_PFVF_CMD_PMASK) & M_FW_PFVF_CMD_PMASK)

#define S_FW_PFVF_CMD_NEQ               0
#define M_FW_PFVF_CMD_NEQ               0xfffff
#define G_FW_PFVF_CMD_NEQ(x)            \
	(((x) >> S_FW_PFVF_CMD_NEQ) & M_FW_PFVF_CMD_NEQ)

#define S_FW_PFVF_CMD_TC                24
#define M_FW_PFVF_CMD_TC                0xff
#define G_FW_PFVF_CMD_TC(x)             \
	(((x) >> S_FW_PFVF_CMD_TC) & M_FW_PFVF_CMD_TC)

#define S_FW_PFVF_CMD_NVI               16
#define M_FW_PFVF_CMD_NVI               0xff
#define G_FW_PFVF_CMD_NVI(x)            \
	(((x) >> S_FW_PFVF_CMD_NVI) & M_FW_PFVF_CMD_NVI)

#define S_FW_PFVF_CMD_NEXACTF           0
#define M_FW_PFVF_CMD_NEXACTF           0xffff
#define G_FW_PFVF_CMD_NEXACTF(x)        \
	(((x) >> S_FW_PFVF_CMD_NEXACTF) & M_FW_PFVF_CMD_NEXACTF)

#define S_FW_PFVF_CMD_R_CAPS            24
#define M_FW_PFVF_CMD_R_CAPS            0xff
#define G_FW_PFVF_CMD_R_CAPS(x)         \
	(((x) >> S_FW_PFVF_CMD_R_CAPS) & M_FW_PFVF_CMD_R_CAPS)

#define S_FW_PFVF_CMD_WX_CAPS           16
#define M_FW_PFVF_CMD_WX_CAPS           0xff
#define G_FW_PFVF_CMD_WX_CAPS(x)        \
	(((x) >> S_FW_PFVF_CMD_WX_CAPS) & M_FW_PFVF_CMD_WX_CAPS)

#define S_FW_PFVF_CMD_NETHCTRL          0
#define M_FW_PFVF_CMD_NETHCTRL          0xffff
#define G_FW_PFVF_CMD_NETHCTRL(x)       \
	(((x) >> S_FW_PFVF_CMD_NETHCTRL) & M_FW_PFVF_CMD_NETHCTRL)

/*
 * ingress queue type; the first 1K ingress queues can have associated 0,
 * 1 or 2 free lists and an interrupt, all other ingress queues lack these
 * capabilities
 */
enum fw_iq_type {
	FW_IQ_TYPE_FL_INT_CAP,
};

enum fw_iq_iqtype {
	FW_IQ_IQTYPE_NIC = 1,
	FW_IQ_IQTYPE_OFLD,
};

struct fw_iq_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be16 physiqid;
	__be16 iqid;
	__be16 fl0id;
	__be16 fl1id;
	__be32 type_to_iqandstindex;
	__be16 iqdroprss_to_iqesize;
	__be16 iqsize;
	__be64 iqaddr;
	__be32 iqns_to_fl0congen;
	__be16 fl0dcaen_to_fl0cidxfthresh;
	__be16 fl0size;
	__be64 fl0addr;
	__be32 fl1cngchmap_to_fl1congen;
	__be16 fl1dcaen_to_fl1cidxfthresh;
	__be16 fl1size;
	__be64 fl1addr;
};

#define S_FW_IQ_CMD_PFN		8
#define M_FW_IQ_CMD_PFN		0x7
#define V_FW_IQ_CMD_PFN(x)	((x) << S_FW_IQ_CMD_PFN)
#define G_FW_IQ_CMD_PFN(x)	(((x) >> S_FW_IQ_CMD_PFN) & M_FW_IQ_CMD_PFN)

#define S_FW_IQ_CMD_VFN		0
#define M_FW_IQ_CMD_VFN		0xff
#define V_FW_IQ_CMD_VFN(x)	((x) << S_FW_IQ_CMD_VFN)
#define G_FW_IQ_CMD_VFN(x)	(((x) >> S_FW_IQ_CMD_VFN) & M_FW_IQ_CMD_VFN)

#define S_FW_IQ_CMD_ALLOC	31
#define M_FW_IQ_CMD_ALLOC	0x1
#define V_FW_IQ_CMD_ALLOC(x)	((x) << S_FW_IQ_CMD_ALLOC)
#define G_FW_IQ_CMD_ALLOC(x)	\
	(((x) >> S_FW_IQ_CMD_ALLOC) & M_FW_IQ_CMD_ALLOC)
#define F_FW_IQ_CMD_ALLOC	V_FW_IQ_CMD_ALLOC(1U)

#define S_FW_IQ_CMD_FREE	30
#define M_FW_IQ_CMD_FREE	0x1
#define V_FW_IQ_CMD_FREE(x)	((x) << S_FW_IQ_CMD_FREE)
#define G_FW_IQ_CMD_FREE(x)	(((x) >> S_FW_IQ_CMD_FREE) & M_FW_IQ_CMD_FREE)
#define F_FW_IQ_CMD_FREE	V_FW_IQ_CMD_FREE(1U)

#define S_FW_IQ_CMD_IQSTART	28
#define M_FW_IQ_CMD_IQSTART	0x1
#define V_FW_IQ_CMD_IQSTART(x)	((x) << S_FW_IQ_CMD_IQSTART)
#define G_FW_IQ_CMD_IQSTART(x)	\
	(((x) >> S_FW_IQ_CMD_IQSTART) & M_FW_IQ_CMD_IQSTART)
#define F_FW_IQ_CMD_IQSTART	V_FW_IQ_CMD_IQSTART(1U)

#define S_FW_IQ_CMD_IQSTOP	27
#define M_FW_IQ_CMD_IQSTOP	0x1
#define V_FW_IQ_CMD_IQSTOP(x)	((x) << S_FW_IQ_CMD_IQSTOP)
#define G_FW_IQ_CMD_IQSTOP(x)	\
	(((x) >> S_FW_IQ_CMD_IQSTOP) & M_FW_IQ_CMD_IQSTOP)
#define F_FW_IQ_CMD_IQSTOP	V_FW_IQ_CMD_IQSTOP(1U)

#define S_FW_IQ_CMD_TYPE	29
#define M_FW_IQ_CMD_TYPE	0x7
#define V_FW_IQ_CMD_TYPE(x)	((x) << S_FW_IQ_CMD_TYPE)
#define G_FW_IQ_CMD_TYPE(x)	(((x) >> S_FW_IQ_CMD_TYPE) & M_FW_IQ_CMD_TYPE)

#define S_FW_IQ_CMD_IQASYNCH	28
#define M_FW_IQ_CMD_IQASYNCH	0x1
#define V_FW_IQ_CMD_IQASYNCH(x)	((x) << S_FW_IQ_CMD_IQASYNCH)
#define G_FW_IQ_CMD_IQASYNCH(x)	\
	(((x) >> S_FW_IQ_CMD_IQASYNCH) & M_FW_IQ_CMD_IQASYNCH)
#define F_FW_IQ_CMD_IQASYNCH	V_FW_IQ_CMD_IQASYNCH(1U)

#define S_FW_IQ_CMD_VIID	16
#define M_FW_IQ_CMD_VIID	0xfff
#define V_FW_IQ_CMD_VIID(x)	((x) << S_FW_IQ_CMD_VIID)
#define G_FW_IQ_CMD_VIID(x)	(((x) >> S_FW_IQ_CMD_VIID) & M_FW_IQ_CMD_VIID)

#define S_FW_IQ_CMD_IQANDST	15
#define M_FW_IQ_CMD_IQANDST	0x1
#define V_FW_IQ_CMD_IQANDST(x)	((x) << S_FW_IQ_CMD_IQANDST)
#define G_FW_IQ_CMD_IQANDST(x)	\
	(((x) >> S_FW_IQ_CMD_IQANDST) & M_FW_IQ_CMD_IQANDST)
#define F_FW_IQ_CMD_IQANDST	V_FW_IQ_CMD_IQANDST(1U)

#define S_FW_IQ_CMD_IQANUD	12
#define M_FW_IQ_CMD_IQANUD	0x3
#define V_FW_IQ_CMD_IQANUD(x)	((x) << S_FW_IQ_CMD_IQANUD)
#define G_FW_IQ_CMD_IQANUD(x)	\
	(((x) >> S_FW_IQ_CMD_IQANUD) & M_FW_IQ_CMD_IQANUD)

#define S_FW_IQ_CMD_IQANDSTINDEX	0
#define M_FW_IQ_CMD_IQANDSTINDEX	0xfff
#define V_FW_IQ_CMD_IQANDSTINDEX(x)	((x) << S_FW_IQ_CMD_IQANDSTINDEX)
#define G_FW_IQ_CMD_IQANDSTINDEX(x)	\
	(((x) >> S_FW_IQ_CMD_IQANDSTINDEX) & M_FW_IQ_CMD_IQANDSTINDEX)

#define S_FW_IQ_CMD_IQGTSMODE		14
#define M_FW_IQ_CMD_IQGTSMODE		0x1
#define V_FW_IQ_CMD_IQGTSMODE(x)	((x) << S_FW_IQ_CMD_IQGTSMODE)
#define G_FW_IQ_CMD_IQGTSMODE(x)	\
	(((x) >> S_FW_IQ_CMD_IQGTSMODE) & M_FW_IQ_CMD_IQGTSMODE)
#define F_FW_IQ_CMD_IQGTSMODE	V_FW_IQ_CMD_IQGTSMODE(1U)

#define S_FW_IQ_CMD_IQPCIECH	12
#define M_FW_IQ_CMD_IQPCIECH	0x3
#define V_FW_IQ_CMD_IQPCIECH(x)	((x) << S_FW_IQ_CMD_IQPCIECH)
#define G_FW_IQ_CMD_IQPCIECH(x)	\
	(((x) >> S_FW_IQ_CMD_IQPCIECH) & M_FW_IQ_CMD_IQPCIECH)

#define S_FW_IQ_CMD_IQINTCNTTHRESH	4
#define M_FW_IQ_CMD_IQINTCNTTHRESH	0x3
#define V_FW_IQ_CMD_IQINTCNTTHRESH(x)	((x) << S_FW_IQ_CMD_IQINTCNTTHRESH)
#define G_FW_IQ_CMD_IQINTCNTTHRESH(x)	\
	(((x) >> S_FW_IQ_CMD_IQINTCNTTHRESH) & M_FW_IQ_CMD_IQINTCNTTHRESH)

#define S_FW_IQ_CMD_IQESIZE	0
#define M_FW_IQ_CMD_IQESIZE	0x3
#define V_FW_IQ_CMD_IQESIZE(x)	((x) << S_FW_IQ_CMD_IQESIZE)
#define G_FW_IQ_CMD_IQESIZE(x)	\
	(((x) >> S_FW_IQ_CMD_IQESIZE) & M_FW_IQ_CMD_IQESIZE)

#define S_FW_IQ_CMD_IQRO                30
#define M_FW_IQ_CMD_IQRO                0x1
#define V_FW_IQ_CMD_IQRO(x)             ((x) << S_FW_IQ_CMD_IQRO)
#define G_FW_IQ_CMD_IQRO(x)             \
	(((x) >> S_FW_IQ_CMD_IQRO) & M_FW_IQ_CMD_IQRO)
#define F_FW_IQ_CMD_IQRO                V_FW_IQ_CMD_IQRO(1U)

#define S_FW_IQ_CMD_IQFLINTCONGEN	27
#define M_FW_IQ_CMD_IQFLINTCONGEN	0x1
#define V_FW_IQ_CMD_IQFLINTCONGEN(x)	((x) << S_FW_IQ_CMD_IQFLINTCONGEN)
#define G_FW_IQ_CMD_IQFLINTCONGEN(x)	\
	(((x) >> S_FW_IQ_CMD_IQFLINTCONGEN) & M_FW_IQ_CMD_IQFLINTCONGEN)
#define F_FW_IQ_CMD_IQFLINTCONGEN	V_FW_IQ_CMD_IQFLINTCONGEN(1U)

#define S_FW_IQ_CMD_IQTYPE	24
#define V_FW_IQ_CMD_IQTYPE(x)	((x) << S_FW_IQ_CMD_IQTYPE)

#define S_FW_IQ_CMD_FL0CNGCHMAP		20
#define M_FW_IQ_CMD_FL0CNGCHMAP		0xf
#define V_FW_IQ_CMD_FL0CNGCHMAP(x)	((x) << S_FW_IQ_CMD_FL0CNGCHMAP)
#define G_FW_IQ_CMD_FL0CNGCHMAP(x)	\
	(((x) >> S_FW_IQ_CMD_FL0CNGCHMAP) & M_FW_IQ_CMD_FL0CNGCHMAP)

#define S_FW_IQ_CMD_FL0DATARO		12
#define M_FW_IQ_CMD_FL0DATARO		0x1
#define V_FW_IQ_CMD_FL0DATARO(x)	((x) << S_FW_IQ_CMD_FL0DATARO)
#define G_FW_IQ_CMD_FL0DATARO(x)	\
	(((x) >> S_FW_IQ_CMD_FL0DATARO) & M_FW_IQ_CMD_FL0DATARO)
#define F_FW_IQ_CMD_FL0DATARO	V_FW_IQ_CMD_FL0DATARO(1U)

#define S_FW_IQ_CMD_FL0CONGCIF		11
#define M_FW_IQ_CMD_FL0CONGCIF		0x1
#define V_FW_IQ_CMD_FL0CONGCIF(x)	((x) << S_FW_IQ_CMD_FL0CONGCIF)
#define G_FW_IQ_CMD_FL0CONGCIF(x)	\
	(((x) >> S_FW_IQ_CMD_FL0CONGCIF) & M_FW_IQ_CMD_FL0CONGCIF)
#define F_FW_IQ_CMD_FL0CONGCIF	V_FW_IQ_CMD_FL0CONGCIF(1U)

#define S_FW_IQ_CMD_FL0FETCHRO		6
#define M_FW_IQ_CMD_FL0FETCHRO		0x1
#define V_FW_IQ_CMD_FL0FETCHRO(x)	((x) << S_FW_IQ_CMD_FL0FETCHRO)
#define G_FW_IQ_CMD_FL0FETCHRO(x)	\
	(((x) >> S_FW_IQ_CMD_FL0FETCHRO) & M_FW_IQ_CMD_FL0FETCHRO)
#define F_FW_IQ_CMD_FL0FETCHRO	V_FW_IQ_CMD_FL0FETCHRO(1U)

#define S_FW_IQ_CMD_FL0HOSTFCMODE	4
#define M_FW_IQ_CMD_FL0HOSTFCMODE	0x3
#define V_FW_IQ_CMD_FL0HOSTFCMODE(x)	((x) << S_FW_IQ_CMD_FL0HOSTFCMODE)
#define G_FW_IQ_CMD_FL0HOSTFCMODE(x)	\
	(((x) >> S_FW_IQ_CMD_FL0HOSTFCMODE) & M_FW_IQ_CMD_FL0HOSTFCMODE)

#define S_FW_IQ_CMD_FL0PADEN	2
#define M_FW_IQ_CMD_FL0PADEN	0x1
#define V_FW_IQ_CMD_FL0PADEN(x)	((x) << S_FW_IQ_CMD_FL0PADEN)
#define G_FW_IQ_CMD_FL0PADEN(x)	\
	(((x) >> S_FW_IQ_CMD_FL0PADEN) & M_FW_IQ_CMD_FL0PADEN)
#define F_FW_IQ_CMD_FL0PADEN	V_FW_IQ_CMD_FL0PADEN(1U)

#define S_FW_IQ_CMD_FL0PACKEN		1
#define M_FW_IQ_CMD_FL0PACKEN		0x1
#define V_FW_IQ_CMD_FL0PACKEN(x)	((x) << S_FW_IQ_CMD_FL0PACKEN)
#define G_FW_IQ_CMD_FL0PACKEN(x)	\
	(((x) >> S_FW_IQ_CMD_FL0PACKEN) & M_FW_IQ_CMD_FL0PACKEN)
#define F_FW_IQ_CMD_FL0PACKEN	V_FW_IQ_CMD_FL0PACKEN(1U)

#define S_FW_IQ_CMD_FL0CONGEN		0
#define M_FW_IQ_CMD_FL0CONGEN		0x1
#define V_FW_IQ_CMD_FL0CONGEN(x)	((x) << S_FW_IQ_CMD_FL0CONGEN)
#define G_FW_IQ_CMD_FL0CONGEN(x)	\
	(((x) >> S_FW_IQ_CMD_FL0CONGEN) & M_FW_IQ_CMD_FL0CONGEN)
#define F_FW_IQ_CMD_FL0CONGEN	V_FW_IQ_CMD_FL0CONGEN(1U)

#define S_FW_IQ_CMD_FL0FBMIN	7
#define M_FW_IQ_CMD_FL0FBMIN	0x7
#define V_FW_IQ_CMD_FL0FBMIN(x)	((x) << S_FW_IQ_CMD_FL0FBMIN)
#define G_FW_IQ_CMD_FL0FBMIN(x)	\
	(((x) >> S_FW_IQ_CMD_FL0FBMIN) & M_FW_IQ_CMD_FL0FBMIN)

#define S_FW_IQ_CMD_FL0FBMAX	4
#define M_FW_IQ_CMD_FL0FBMAX	0x7
#define V_FW_IQ_CMD_FL0FBMAX(x)	((x) << S_FW_IQ_CMD_FL0FBMAX)
#define G_FW_IQ_CMD_FL0FBMAX(x)	\
	(((x) >> S_FW_IQ_CMD_FL0FBMAX) & M_FW_IQ_CMD_FL0FBMAX)

struct fw_eq_eth_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be32 eqid_pkd;
	__be32 physeqid_pkd;
	__be32 fetchszm_to_iqid;
	__be32 dcaen_to_eqsize;
	__be64 eqaddr;
	__be32 autoequiqe_to_viid;
	__be32 r8_lo;
	__be64 r9;
};

#define S_FW_EQ_ETH_CMD_PFN	8
#define M_FW_EQ_ETH_CMD_PFN	0x7
#define V_FW_EQ_ETH_CMD_PFN(x)	((x) << S_FW_EQ_ETH_CMD_PFN)
#define G_FW_EQ_ETH_CMD_PFN(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_PFN) & M_FW_EQ_ETH_CMD_PFN)

#define S_FW_EQ_ETH_CMD_VFN	0
#define M_FW_EQ_ETH_CMD_VFN	0xff
#define V_FW_EQ_ETH_CMD_VFN(x)	((x) << S_FW_EQ_ETH_CMD_VFN)
#define G_FW_EQ_ETH_CMD_VFN(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_VFN) & M_FW_EQ_ETH_CMD_VFN)

#define S_FW_EQ_ETH_CMD_ALLOC		31
#define M_FW_EQ_ETH_CMD_ALLOC		0x1
#define V_FW_EQ_ETH_CMD_ALLOC(x)	((x) << S_FW_EQ_ETH_CMD_ALLOC)
#define G_FW_EQ_ETH_CMD_ALLOC(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_ALLOC) & M_FW_EQ_ETH_CMD_ALLOC)
#define F_FW_EQ_ETH_CMD_ALLOC	V_FW_EQ_ETH_CMD_ALLOC(1U)

#define S_FW_EQ_ETH_CMD_FREE	30
#define M_FW_EQ_ETH_CMD_FREE	0x1
#define V_FW_EQ_ETH_CMD_FREE(x)	((x) << S_FW_EQ_ETH_CMD_FREE)
#define G_FW_EQ_ETH_CMD_FREE(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_FREE) & M_FW_EQ_ETH_CMD_FREE)
#define F_FW_EQ_ETH_CMD_FREE	V_FW_EQ_ETH_CMD_FREE(1U)

#define S_FW_EQ_ETH_CMD_EQSTART		28
#define M_FW_EQ_ETH_CMD_EQSTART		0x1
#define V_FW_EQ_ETH_CMD_EQSTART(x)	((x) << S_FW_EQ_ETH_CMD_EQSTART)
#define G_FW_EQ_ETH_CMD_EQSTART(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_EQSTART) & M_FW_EQ_ETH_CMD_EQSTART)
#define F_FW_EQ_ETH_CMD_EQSTART	V_FW_EQ_ETH_CMD_EQSTART(1U)

#define S_FW_EQ_ETH_CMD_EQID	0
#define M_FW_EQ_ETH_CMD_EQID	0xfffff
#define V_FW_EQ_ETH_CMD_EQID(x)	((x) << S_FW_EQ_ETH_CMD_EQID)
#define G_FW_EQ_ETH_CMD_EQID(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_EQID) & M_FW_EQ_ETH_CMD_EQID)

#define S_FW_EQ_ETH_CMD_PHYSEQID        0
#define M_FW_EQ_ETH_CMD_PHYSEQID        0xfffff
#define G_FW_EQ_ETH_CMD_PHYSEQID(x)     \
	(((x) >> S_FW_EQ_ETH_CMD_PHYSEQID) & M_FW_EQ_ETH_CMD_PHYSEQID)

#define S_FW_EQ_ETH_CMD_FETCHRO		22
#define M_FW_EQ_ETH_CMD_FETCHRO		0x1
#define V_FW_EQ_ETH_CMD_FETCHRO(x)	((x) << S_FW_EQ_ETH_CMD_FETCHRO)
#define G_FW_EQ_ETH_CMD_FETCHRO(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_FETCHRO) & M_FW_EQ_ETH_CMD_FETCHRO)
#define F_FW_EQ_ETH_CMD_FETCHRO	V_FW_EQ_ETH_CMD_FETCHRO(1U)

#define S_FW_EQ_ETH_CMD_HOSTFCMODE	20
#define M_FW_EQ_ETH_CMD_HOSTFCMODE	0x3
#define V_FW_EQ_ETH_CMD_HOSTFCMODE(x)	((x) << S_FW_EQ_ETH_CMD_HOSTFCMODE)
#define G_FW_EQ_ETH_CMD_HOSTFCMODE(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_HOSTFCMODE) & M_FW_EQ_ETH_CMD_HOSTFCMODE)

#define S_FW_EQ_ETH_CMD_PCIECHN		16
#define M_FW_EQ_ETH_CMD_PCIECHN		0x3
#define V_FW_EQ_ETH_CMD_PCIECHN(x)	((x) << S_FW_EQ_ETH_CMD_PCIECHN)
#define G_FW_EQ_ETH_CMD_PCIECHN(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_PCIECHN) & M_FW_EQ_ETH_CMD_PCIECHN)

#define S_FW_EQ_ETH_CMD_IQID	0
#define M_FW_EQ_ETH_CMD_IQID	0xffff
#define V_FW_EQ_ETH_CMD_IQID(x)	((x) << S_FW_EQ_ETH_CMD_IQID)
#define G_FW_EQ_ETH_CMD_IQID(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_IQID) & M_FW_EQ_ETH_CMD_IQID)

#define S_FW_EQ_ETH_CMD_FBMIN		23
#define M_FW_EQ_ETH_CMD_FBMIN		0x7
#define V_FW_EQ_ETH_CMD_FBMIN(x)	((x) << S_FW_EQ_ETH_CMD_FBMIN)
#define G_FW_EQ_ETH_CMD_FBMIN(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_FBMIN) & M_FW_EQ_ETH_CMD_FBMIN)

#define S_FW_EQ_ETH_CMD_FBMAX		20
#define M_FW_EQ_ETH_CMD_FBMAX		0x7
#define V_FW_EQ_ETH_CMD_FBMAX(x)	((x) << S_FW_EQ_ETH_CMD_FBMAX)
#define G_FW_EQ_ETH_CMD_FBMAX(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_FBMAX) & M_FW_EQ_ETH_CMD_FBMAX)

#define S_FW_EQ_ETH_CMD_CIDXFTHRESH	16
#define M_FW_EQ_ETH_CMD_CIDXFTHRESH	0x7
#define V_FW_EQ_ETH_CMD_CIDXFTHRESH(x)	((x) << S_FW_EQ_ETH_CMD_CIDXFTHRESH)
#define G_FW_EQ_ETH_CMD_CIDXFTHRESH(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_CIDXFTHRESH) & M_FW_EQ_ETH_CMD_CIDXFTHRESH)

#define S_FW_EQ_ETH_CMD_EQSIZE		0
#define M_FW_EQ_ETH_CMD_EQSIZE		0xffff
#define V_FW_EQ_ETH_CMD_EQSIZE(x)	((x) << S_FW_EQ_ETH_CMD_EQSIZE)
#define G_FW_EQ_ETH_CMD_EQSIZE(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_EQSIZE) & M_FW_EQ_ETH_CMD_EQSIZE)

#define S_FW_EQ_ETH_CMD_AUTOEQUEQE	30
#define M_FW_EQ_ETH_CMD_AUTOEQUEQE	0x1
#define V_FW_EQ_ETH_CMD_AUTOEQUEQE(x)	((x) << S_FW_EQ_ETH_CMD_AUTOEQUEQE)
#define G_FW_EQ_ETH_CMD_AUTOEQUEQE(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_AUTOEQUEQE) & M_FW_EQ_ETH_CMD_AUTOEQUEQE)
#define F_FW_EQ_ETH_CMD_AUTOEQUEQE	V_FW_EQ_ETH_CMD_AUTOEQUEQE(1U)

#define S_FW_EQ_ETH_CMD_VIID	16
#define M_FW_EQ_ETH_CMD_VIID	0xfff
#define V_FW_EQ_ETH_CMD_VIID(x)	((x) << S_FW_EQ_ETH_CMD_VIID)
#define G_FW_EQ_ETH_CMD_VIID(x)	\
	(((x) >> S_FW_EQ_ETH_CMD_VIID) & M_FW_EQ_ETH_CMD_VIID)

struct fw_eq_ctrl_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be32 cmpliqid_eqid;
	__be32 physeqid_pkd;
	__be32 fetchszm_to_iqid;
	__be32 dcaen_to_eqsize;
	__be64 eqaddr;
};

#define S_FW_EQ_CTRL_CMD_PFN		8
#define V_FW_EQ_CTRL_CMD_PFN(x)		((x) << S_FW_EQ_CTRL_CMD_PFN)

#define S_FW_EQ_CTRL_CMD_VFN		0
#define V_FW_EQ_CTRL_CMD_VFN(x)		((x) << S_FW_EQ_CTRL_CMD_VFN)

#define S_FW_EQ_CTRL_CMD_ALLOC		31
#define V_FW_EQ_CTRL_CMD_ALLOC(x)	((x) << S_FW_EQ_CTRL_CMD_ALLOC)
#define F_FW_EQ_CTRL_CMD_ALLOC		V_FW_EQ_CTRL_CMD_ALLOC(1U)

#define S_FW_EQ_CTRL_CMD_FREE		30
#define V_FW_EQ_CTRL_CMD_FREE(x)	((x) << S_FW_EQ_CTRL_CMD_FREE)
#define F_FW_EQ_CTRL_CMD_FREE		V_FW_EQ_CTRL_CMD_FREE(1U)

#define S_FW_EQ_CTRL_CMD_EQSTART	28
#define V_FW_EQ_CTRL_CMD_EQSTART(x)	((x) << S_FW_EQ_CTRL_CMD_EQSTART)
#define F_FW_EQ_CTRL_CMD_EQSTART	V_FW_EQ_CTRL_CMD_EQSTART(1U)

#define S_FW_EQ_CTRL_CMD_CMPLIQID	20
#define V_FW_EQ_CTRL_CMD_CMPLIQID(x)	((x) << S_FW_EQ_CTRL_CMD_CMPLIQID)

#define S_FW_EQ_CTRL_CMD_EQID		0
#define M_FW_EQ_CTRL_CMD_EQID		0xfffff
#define V_FW_EQ_CTRL_CMD_EQID(x)	((x) << S_FW_EQ_CTRL_CMD_EQID)
#define G_FW_EQ_CTRL_CMD_EQID(x)	\
	(((x) >> S_FW_EQ_CTRL_CMD_EQID) & M_FW_EQ_CTRL_CMD_EQID)

#define S_FW_EQ_CTRL_CMD_PHYSEQID       0
#define M_FW_EQ_CTRL_CMD_PHYSEQID       0xfffff
#define V_FW_EQ_CTRL_CMD_PHYSEQID(x)    ((x) << S_FW_EQ_CTRL_CMD_PHYSEQID)
#define G_FW_EQ_CTRL_CMD_PHYSEQID(x)    \
	(((x) >> S_FW_EQ_CTRL_CMD_PHYSEQID) & M_FW_EQ_CTRL_CMD_PHYSEQID)

#define S_FW_EQ_CTRL_CMD_FETCHRO	22
#define V_FW_EQ_CTRL_CMD_FETCHRO(x)	((x) << S_FW_EQ_CTRL_CMD_FETCHRO)
#define F_FW_EQ_CTRL_CMD_FETCHRO	V_FW_EQ_CTRL_CMD_FETCHRO(1U)

#define S_FW_EQ_CTRL_CMD_HOSTFCMODE	20
#define M_FW_EQ_CTRL_CMD_HOSTFCMODE	0x3
#define V_FW_EQ_CTRL_CMD_HOSTFCMODE(x)	((x) << S_FW_EQ_CTRL_CMD_HOSTFCMODE)

#define S_FW_EQ_CTRL_CMD_PCIECHN	16
#define V_FW_EQ_CTRL_CMD_PCIECHN(x)	((x) << S_FW_EQ_CTRL_CMD_PCIECHN)

#define S_FW_EQ_CTRL_CMD_IQID		0
#define V_FW_EQ_CTRL_CMD_IQID(x)	((x) << S_FW_EQ_CTRL_CMD_IQID)

#define S_FW_EQ_CTRL_CMD_FBMIN		23
#define V_FW_EQ_CTRL_CMD_FBMIN(x)	((x) << S_FW_EQ_CTRL_CMD_FBMIN)

#define S_FW_EQ_CTRL_CMD_FBMAX		20
#define V_FW_EQ_CTRL_CMD_FBMAX(x)	((x) << S_FW_EQ_CTRL_CMD_FBMAX)

#define S_FW_EQ_CTRL_CMD_CIDXFTHRESH	16
#define V_FW_EQ_CTRL_CMD_CIDXFTHRESH(x)	((x) << S_FW_EQ_CTRL_CMD_CIDXFTHRESH)

#define S_FW_EQ_CTRL_CMD_EQSIZE		0
#define V_FW_EQ_CTRL_CMD_EQSIZE(x)	((x) << S_FW_EQ_CTRL_CMD_EQSIZE)

enum fw_vi_func {
	FW_VI_FUNC_ETH,
};

/* Macros for VIID parsing:
 * VIID - [10:8] PFN, [7] VI Valid, [6:0] VI number
 */

#define S_FW_VIID_VIVLD         7
#define M_FW_VIID_VIVLD         0x1
#define G_FW_VIID_VIVLD(x)      (((x) >> S_FW_VIID_VIVLD) & M_FW_VIID_VIVLD)

#define S_FW_VIID_VIN           0
#define M_FW_VIID_VIN           0x7F
#define G_FW_VIID_VIN(x)        (((x) >> S_FW_VIID_VIN) & M_FW_VIID_VIN)

struct fw_vi_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be16 type_to_viid;
	__u8   mac[6];
	__u8   portid_pkd;
	__u8   nmac;
	__u8   nmac0[6];
	__be16 norss_rsssize;
	__u8   nmac1[6];
	__be16 idsiiq_pkd;
	__u8   nmac2[6];
	__be16 idseiq_pkd;
	__u8   nmac3[6];
	__be64 r9;
	__be64 r10;
};

#define S_FW_VI_CMD_PFN		8
#define M_FW_VI_CMD_PFN		0x7
#define V_FW_VI_CMD_PFN(x)	((x) << S_FW_VI_CMD_PFN)
#define G_FW_VI_CMD_PFN(x)	(((x) >> S_FW_VI_CMD_PFN) & M_FW_VI_CMD_PFN)

#define S_FW_VI_CMD_VFN		0
#define M_FW_VI_CMD_VFN		0xff
#define V_FW_VI_CMD_VFN(x)	((x) << S_FW_VI_CMD_VFN)
#define G_FW_VI_CMD_VFN(x)	(((x) >> S_FW_VI_CMD_VFN) & M_FW_VI_CMD_VFN)

#define S_FW_VI_CMD_ALLOC	31
#define M_FW_VI_CMD_ALLOC	0x1
#define V_FW_VI_CMD_ALLOC(x)	((x) << S_FW_VI_CMD_ALLOC)
#define G_FW_VI_CMD_ALLOC(x)	\
	(((x) >> S_FW_VI_CMD_ALLOC) & M_FW_VI_CMD_ALLOC)
#define F_FW_VI_CMD_ALLOC	V_FW_VI_CMD_ALLOC(1U)

#define S_FW_VI_CMD_FREE	30
#define M_FW_VI_CMD_FREE	0x1
#define V_FW_VI_CMD_FREE(x)	((x) << S_FW_VI_CMD_FREE)
#define G_FW_VI_CMD_FREE(x)	(((x) >> S_FW_VI_CMD_FREE) & M_FW_VI_CMD_FREE)
#define F_FW_VI_CMD_FREE	V_FW_VI_CMD_FREE(1U)

#define S_FW_VI_CMD_VFVLD       24
#define M_FW_VI_CMD_VFVLD       0x1
#define G_FW_VI_CMD_VFVLD(x)    \
	(((x) >> S_FW_VI_CMD_VFVLD) & M_FW_VI_CMD_VFVLD)

#define S_FW_VI_CMD_VIN         16
#define M_FW_VI_CMD_VIN         0xff
#define G_FW_VI_CMD_VIN(x)      \
	(((x) >> S_FW_VI_CMD_VIN) & M_FW_VI_CMD_VIN)

#define S_FW_VI_CMD_TYPE	15
#define M_FW_VI_CMD_TYPE	0x1
#define V_FW_VI_CMD_TYPE(x)	((x) << S_FW_VI_CMD_TYPE)
#define G_FW_VI_CMD_TYPE(x)	(((x) >> S_FW_VI_CMD_TYPE) & M_FW_VI_CMD_TYPE)
#define F_FW_VI_CMD_TYPE	V_FW_VI_CMD_TYPE(1U)

#define S_FW_VI_CMD_FUNC	12
#define M_FW_VI_CMD_FUNC	0x7
#define V_FW_VI_CMD_FUNC(x)	((x) << S_FW_VI_CMD_FUNC)
#define G_FW_VI_CMD_FUNC(x)	(((x) >> S_FW_VI_CMD_FUNC) & M_FW_VI_CMD_FUNC)

#define S_FW_VI_CMD_VIID	0
#define M_FW_VI_CMD_VIID	0xfff
#define V_FW_VI_CMD_VIID(x)	((x) << S_FW_VI_CMD_VIID)
#define G_FW_VI_CMD_VIID(x)	(((x) >> S_FW_VI_CMD_VIID) & M_FW_VI_CMD_VIID)

#define S_FW_VI_CMD_PORTID	4
#define M_FW_VI_CMD_PORTID	0xf
#define V_FW_VI_CMD_PORTID(x)	((x) << S_FW_VI_CMD_PORTID)
#define G_FW_VI_CMD_PORTID(x)	\
	(((x) >> S_FW_VI_CMD_PORTID) & M_FW_VI_CMD_PORTID)

#define S_FW_VI_CMD_RSSSIZE	0
#define M_FW_VI_CMD_RSSSIZE	0x7ff
#define V_FW_VI_CMD_RSSSIZE(x)	((x) << S_FW_VI_CMD_RSSSIZE)
#define G_FW_VI_CMD_RSSSIZE(x)	\
	(((x) >> S_FW_VI_CMD_RSSSIZE) & M_FW_VI_CMD_RSSSIZE)

/* Special VI_MAC command index ids */
#define FW_VI_MAC_ADD_MAC		0x3FF
#define FW_VI_MAC_ADD_PERSIST_MAC	0x3FE
#define FW_VI_MAC_ID_BASED_FREE         0x3FC

enum fw_vi_mac_smac {
	FW_VI_MAC_MPS_TCAM_ENTRY = 0x0,
	FW_VI_MAC_SMT_AND_MPSTCAM = 0x3
};

enum fw_vi_mac_entry_types {
	FW_VI_MAC_TYPE_RAW = 0x2,
};

struct fw_vi_mac_cmd {
	__be32 op_to_viid;
	__be32 freemacs_to_len16;
	union fw_vi_mac {
		struct fw_vi_mac_exact {
			__be16 valid_to_idx;
			__u8   macaddr[6];
		} exact[7];
		struct fw_vi_mac_hash {
			__be64 hashvec;
		} hash;
		struct fw_vi_mac_raw {
			__be32 raw_idx_pkd;
			__be32 data0_pkd;
			__be32 data1[2];
			__be64 data0m_pkd;
			__be32 data1m[2];
		} raw;
	} u;
};

#define S_FW_VI_MAC_CMD_VIID	0
#define M_FW_VI_MAC_CMD_VIID	0xfff
#define V_FW_VI_MAC_CMD_VIID(x)	((x) << S_FW_VI_MAC_CMD_VIID)
#define G_FW_VI_MAC_CMD_VIID(x)	\
	(((x) >> S_FW_VI_MAC_CMD_VIID) & M_FW_VI_MAC_CMD_VIID)

#define S_FW_VI_MAC_CMD_FREEMACS	31
#define V_FW_VI_MAC_CMD_FREEMACS(x)	((x) << S_FW_VI_MAC_CMD_FREEMACS)

#define S_FW_VI_MAC_CMD_ENTRY_TYPE      23
#define V_FW_VI_MAC_CMD_ENTRY_TYPE(x)   ((x) << S_FW_VI_MAC_CMD_ENTRY_TYPE)

#define S_FW_VI_MAC_CMD_VALID		15
#define M_FW_VI_MAC_CMD_VALID		0x1
#define V_FW_VI_MAC_CMD_VALID(x)	((x) << S_FW_VI_MAC_CMD_VALID)
#define G_FW_VI_MAC_CMD_VALID(x)	\
	(((x) >> S_FW_VI_MAC_CMD_VALID) & M_FW_VI_MAC_CMD_VALID)
#define F_FW_VI_MAC_CMD_VALID	V_FW_VI_MAC_CMD_VALID(1U)

#define S_FW_VI_MAC_CMD_SMAC_RESULT	10
#define M_FW_VI_MAC_CMD_SMAC_RESULT	0x3
#define V_FW_VI_MAC_CMD_SMAC_RESULT(x)	((x) << S_FW_VI_MAC_CMD_SMAC_RESULT)
#define G_FW_VI_MAC_CMD_SMAC_RESULT(x)	\
	(((x) >> S_FW_VI_MAC_CMD_SMAC_RESULT) & M_FW_VI_MAC_CMD_SMAC_RESULT)

#define S_FW_VI_MAC_CMD_IDX	0
#define M_FW_VI_MAC_CMD_IDX	0x3ff
#define V_FW_VI_MAC_CMD_IDX(x)	((x) << S_FW_VI_MAC_CMD_IDX)
#define G_FW_VI_MAC_CMD_IDX(x)	\
	(((x) >> S_FW_VI_MAC_CMD_IDX) & M_FW_VI_MAC_CMD_IDX)

#define S_FW_VI_MAC_CMD_RAW_IDX         16
#define M_FW_VI_MAC_CMD_RAW_IDX         0xffff
#define V_FW_VI_MAC_CMD_RAW_IDX(x)      ((x) << S_FW_VI_MAC_CMD_RAW_IDX)
#define G_FW_VI_MAC_CMD_RAW_IDX(x)      \
	(((x) >> S_FW_VI_MAC_CMD_RAW_IDX) & M_FW_VI_MAC_CMD_RAW_IDX)

struct fw_vi_rxmode_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	__be32 mtu_to_vlanexen;
	__be32 r4_lo;
};

#define S_FW_VI_RXMODE_CMD_VIID		0
#define M_FW_VI_RXMODE_CMD_VIID		0xfff
#define V_FW_VI_RXMODE_CMD_VIID(x)	((x) << S_FW_VI_RXMODE_CMD_VIID)
#define G_FW_VI_RXMODE_CMD_VIID(x)	\
	(((x) >> S_FW_VI_RXMODE_CMD_VIID) & M_FW_VI_RXMODE_CMD_VIID)

#define S_FW_VI_RXMODE_CMD_MTU		16
#define M_FW_VI_RXMODE_CMD_MTU		0xffff
#define V_FW_VI_RXMODE_CMD_MTU(x)	((x) << S_FW_VI_RXMODE_CMD_MTU)
#define G_FW_VI_RXMODE_CMD_MTU(x)	\
	(((x) >> S_FW_VI_RXMODE_CMD_MTU) & M_FW_VI_RXMODE_CMD_MTU)

#define S_FW_VI_RXMODE_CMD_PROMISCEN	14
#define M_FW_VI_RXMODE_CMD_PROMISCEN	0x3
#define V_FW_VI_RXMODE_CMD_PROMISCEN(x)	((x) << S_FW_VI_RXMODE_CMD_PROMISCEN)
#define G_FW_VI_RXMODE_CMD_PROMISCEN(x)	\
	(((x) >> S_FW_VI_RXMODE_CMD_PROMISCEN) & M_FW_VI_RXMODE_CMD_PROMISCEN)

#define S_FW_VI_RXMODE_CMD_ALLMULTIEN		12
#define M_FW_VI_RXMODE_CMD_ALLMULTIEN		0x3
#define V_FW_VI_RXMODE_CMD_ALLMULTIEN(x)	\
	((x) << S_FW_VI_RXMODE_CMD_ALLMULTIEN)
#define G_FW_VI_RXMODE_CMD_ALLMULTIEN(x)	\
	(((x) >> S_FW_VI_RXMODE_CMD_ALLMULTIEN) & M_FW_VI_RXMODE_CMD_ALLMULTIEN)

#define S_FW_VI_RXMODE_CMD_BROADCASTEN		10
#define M_FW_VI_RXMODE_CMD_BROADCASTEN		0x3
#define V_FW_VI_RXMODE_CMD_BROADCASTEN(x)	\
	((x) << S_FW_VI_RXMODE_CMD_BROADCASTEN)
#define G_FW_VI_RXMODE_CMD_BROADCASTEN(x)	\
	(((x) >> S_FW_VI_RXMODE_CMD_BROADCASTEN) & \
	 M_FW_VI_RXMODE_CMD_BROADCASTEN)

#define S_FW_VI_RXMODE_CMD_VLANEXEN	8
#define M_FW_VI_RXMODE_CMD_VLANEXEN	0x3
#define V_FW_VI_RXMODE_CMD_VLANEXEN(x)	((x) << S_FW_VI_RXMODE_CMD_VLANEXEN)
#define G_FW_VI_RXMODE_CMD_VLANEXEN(x)	\
	(((x) >> S_FW_VI_RXMODE_CMD_VLANEXEN) & M_FW_VI_RXMODE_CMD_VLANEXEN)

struct fw_vi_enable_cmd {
	__be32 op_to_viid;
	__be32 ien_to_len16;
	__be16 blinkdur;
	__be16 r3;
	__be32 r4;
};

#define S_FW_VI_ENABLE_CMD_VIID		0
#define M_FW_VI_ENABLE_CMD_VIID		0xfff
#define V_FW_VI_ENABLE_CMD_VIID(x)	((x) << S_FW_VI_ENABLE_CMD_VIID)
#define G_FW_VI_ENABLE_CMD_VIID(x)	\
	(((x) >> S_FW_VI_ENABLE_CMD_VIID) & M_FW_VI_ENABLE_CMD_VIID)

#define S_FW_VI_ENABLE_CMD_IEN		31
#define M_FW_VI_ENABLE_CMD_IEN		0x1
#define V_FW_VI_ENABLE_CMD_IEN(x)	((x) << S_FW_VI_ENABLE_CMD_IEN)
#define G_FW_VI_ENABLE_CMD_IEN(x)	\
	(((x) >> S_FW_VI_ENABLE_CMD_IEN) & M_FW_VI_ENABLE_CMD_IEN)
#define F_FW_VI_ENABLE_CMD_IEN	V_FW_VI_ENABLE_CMD_IEN(1U)

#define S_FW_VI_ENABLE_CMD_EEN		30
#define M_FW_VI_ENABLE_CMD_EEN		0x1
#define V_FW_VI_ENABLE_CMD_EEN(x)	((x) << S_FW_VI_ENABLE_CMD_EEN)
#define G_FW_VI_ENABLE_CMD_EEN(x)	\
	(((x) >> S_FW_VI_ENABLE_CMD_EEN) & M_FW_VI_ENABLE_CMD_EEN)
#define F_FW_VI_ENABLE_CMD_EEN	V_FW_VI_ENABLE_CMD_EEN(1U)

#define S_FW_VI_ENABLE_CMD_DCB_INFO	28
#define M_FW_VI_ENABLE_CMD_DCB_INFO	0x1
#define V_FW_VI_ENABLE_CMD_DCB_INFO(x)	((x) << S_FW_VI_ENABLE_CMD_DCB_INFO)
#define G_FW_VI_ENABLE_CMD_DCB_INFO(x)	\
	(((x) >> S_FW_VI_ENABLE_CMD_DCB_INFO) & M_FW_VI_ENABLE_CMD_DCB_INFO)
#define F_FW_VI_ENABLE_CMD_DCB_INFO	V_FW_VI_ENABLE_CMD_DCB_INFO(1U)

/* VI VF stats offset definitions */
#define VI_VF_NUM_STATS 16

/* VI PF stats offset definitions */
#define VI_PF_NUM_STATS	17
enum fw_vi_stats_pf_index {
	FW_VI_PF_STAT_TX_BCAST_BYTES_IX,
	FW_VI_PF_STAT_TX_BCAST_FRAMES_IX,
	FW_VI_PF_STAT_TX_MCAST_BYTES_IX,
	FW_VI_PF_STAT_TX_MCAST_FRAMES_IX,
	FW_VI_PF_STAT_TX_UCAST_BYTES_IX,
	FW_VI_PF_STAT_TX_UCAST_FRAMES_IX,
	FW_VI_PF_STAT_TX_OFLD_BYTES_IX,
	FW_VI_PF_STAT_TX_OFLD_FRAMES_IX,
	FW_VI_PF_STAT_RX_BYTES_IX,
	FW_VI_PF_STAT_RX_FRAMES_IX,
	FW_VI_PF_STAT_RX_BCAST_BYTES_IX,
	FW_VI_PF_STAT_RX_BCAST_FRAMES_IX,
	FW_VI_PF_STAT_RX_MCAST_BYTES_IX,
	FW_VI_PF_STAT_RX_MCAST_FRAMES_IX,
	FW_VI_PF_STAT_RX_UCAST_BYTES_IX,
	FW_VI_PF_STAT_RX_UCAST_FRAMES_IX,
	FW_VI_PF_STAT_RX_ERR_FRAMES_IX
};

struct fw_vi_stats_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	union fw_vi_stats {
		struct fw_vi_stats_ctl {
			__be16 nstats_ix;
			__be16 r6;
			__be32 r7;
			__be64 stat0;
			__be64 stat1;
			__be64 stat2;
			__be64 stat3;
			__be64 stat4;
			__be64 stat5;
		} ctl;
		struct fw_vi_stats_pf {
			__be64 tx_bcast_bytes;
			__be64 tx_bcast_frames;
			__be64 tx_mcast_bytes;
			__be64 tx_mcast_frames;
			__be64 tx_ucast_bytes;
			__be64 tx_ucast_frames;
			__be64 tx_offload_bytes;
			__be64 tx_offload_frames;
			__be64 rx_pf_bytes;
			__be64 rx_pf_frames;
			__be64 rx_bcast_bytes;
			__be64 rx_bcast_frames;
			__be64 rx_mcast_bytes;
			__be64 rx_mcast_frames;
			__be64 rx_ucast_bytes;
			__be64 rx_ucast_frames;
			__be64 rx_err_frames;
		} pf;
		struct fw_vi_stats_vf {
			__be64 tx_bcast_bytes;
			__be64 tx_bcast_frames;
			__be64 tx_mcast_bytes;
			__be64 tx_mcast_frames;
			__be64 tx_ucast_bytes;
			__be64 tx_ucast_frames;
			__be64 tx_drop_frames;
			__be64 tx_offload_bytes;
			__be64 tx_offload_frames;
			__be64 rx_bcast_bytes;
			__be64 rx_bcast_frames;
			__be64 rx_mcast_bytes;
			__be64 rx_mcast_frames;
			__be64 rx_ucast_bytes;
			__be64 rx_ucast_frames;
			__be64 rx_err_frames;
		} vf;
	} u;
};

#define S_FW_VI_STATS_CMD_VIID		0
#define V_FW_VI_STATS_CMD_VIID(x)	((x) << S_FW_VI_STATS_CMD_VIID)

#define S_FW_VI_STATS_CMD_NSTATS	12
#define V_FW_VI_STATS_CMD_NSTATS(x)	((x) << S_FW_VI_STATS_CMD_NSTATS)

#define S_FW_VI_STATS_CMD_IX		0
#define V_FW_VI_STATS_CMD_IX(x)		((x) << S_FW_VI_STATS_CMD_IX)

/* new 32-bit port capabilities bitmap (fw_port_cap32_t) */
#define FW_PORT_CAP32_SPEED_100M        0x00000001UL
#define FW_PORT_CAP32_SPEED_1G          0x00000002UL
#define FW_PORT_CAP32_SPEED_10G         0x00000004UL
#define FW_PORT_CAP32_SPEED_25G         0x00000008UL
#define FW_PORT_CAP32_SPEED_40G         0x00000010UL
#define FW_PORT_CAP32_SPEED_50G         0x00000020UL
#define FW_PORT_CAP32_SPEED_100G        0x00000040UL
#define FW_PORT_CAP32_FC_RX             0x00010000UL
#define FW_PORT_CAP32_FC_TX             0x00020000UL
#define FW_PORT_CAP32_802_3_PAUSE       0x00040000UL
#define FW_PORT_CAP32_802_3_ASM_DIR     0x00080000UL
#define FW_PORT_CAP32_ANEG              0x00100000UL
#define FW_PORT_CAP32_MDIX              0x00200000UL
#define FW_PORT_CAP32_MDIAUTO           0x00400000UL
#define FW_PORT_CAP32_FEC_RS            0x00800000UL
#define FW_PORT_CAP32_FEC_BASER_RS      0x01000000UL
#define FW_PORT_CAP32_FEC_NO_FEC        0x02000000UL
#define FW_PORT_CAP32_FORCE_PAUSE       0x10000000UL
#define FW_PORT_CAP32_FORCE_FEC         0x20000000UL

#define S_FW_PORT_CAP32_SPEED           0
#define M_FW_PORT_CAP32_SPEED           0xfff
#define V_FW_PORT_CAP32_SPEED(x)        ((x) << S_FW_PORT_CAP32_SPEED)
#define G_FW_PORT_CAP32_SPEED(x) \
	(((x) >> S_FW_PORT_CAP32_SPEED) & M_FW_PORT_CAP32_SPEED)

#define S_FW_PORT_CAP32_FC             16
#define M_FW_PORT_CAP32_FC             0x3
#define V_FW_PORT_CAP32_FC(x)          ((x) << S_FW_PORT_CAP32_FC)

#define S_FW_PORT_CAP32_802_3          18
#define M_FW_PORT_CAP32_802_3          0x3
#define V_FW_PORT_CAP32_802_3(x)       ((x) << S_FW_PORT_CAP32_802_3)

enum fw_port_mdi32 {
	FW_PORT_CAP32_MDI_AUTO = 1,
};

#define S_FW_PORT_CAP32_MDI 21
#define M_FW_PORT_CAP32_MDI 3
#define V_FW_PORT_CAP32_MDI(x) ((x) << S_FW_PORT_CAP32_MDI)
#define G_FW_PORT_CAP32_MDI(x) \
	(((x) >> S_FW_PORT_CAP32_MDI) & M_FW_PORT_CAP32_MDI)

#define S_FW_PORT_CAP32_FEC     23
#define M_FW_PORT_CAP32_FEC     0x1f
#define V_FW_PORT_CAP32_FEC(x)  ((x) << S_FW_PORT_CAP32_FEC)

enum fw_port_action {
	FW_PORT_ACTION_L1_CFG32         = 0x0009,
	FW_PORT_ACTION_GET_PORT_INFO32  = 0x000a,
};

struct fw_port_cmd {
	__be32 op_to_portid;
	__be32 action_to_len16;
	union fw_port {
		struct fw_port_l1cfg {
			__be32 rcap;
			__be32 r;
		} l1cfg;
		struct fw_port_l2cfg {
			__u8   ctlbf;
			__u8   ovlan3_to_ivlan0;
			__be16 ivlantype;
			__be16 txipg_force_pinfo;
			__be16 mtu;
			__be16 ovlan0mask;
			__be16 ovlan0type;
			__be16 ovlan1mask;
			__be16 ovlan1type;
			__be16 ovlan2mask;
			__be16 ovlan2type;
			__be16 ovlan3mask;
			__be16 ovlan3type;
		} l2cfg;
		struct fw_port_info {
			__be32 lstatus_to_modtype;
			__be16 pcap;
			__be16 acap;
			__be16 mtu;
			__u8   cbllen;
			__u8   auxlinfo;
			__u8   dcbxdis_pkd;
			__u8   r8_lo;
			__be16 lpacap;
			__be64 r9;
		} info;
		struct fw_port_diags {
			__u8   diagop;
			__u8   r[3];
			__be32 diagval;
		} diags;
		union fw_port_dcb {
			struct fw_port_dcb_pgid {
				__u8   type;
				__u8   apply_pkd;
				__u8   r10_lo[2];
				__be32 pgid;
				__be64 r11;
			} pgid;
			struct fw_port_dcb_pgrate {
				__u8   type;
				__u8   apply_pkd;
				__u8   r10_lo[5];
				__u8   num_tcs_supported;
				__u8   pgrate[8];
				__u8   tsa[8];
			} pgrate;
			struct fw_port_dcb_priorate {
				__u8   type;
				__u8   apply_pkd;
				__u8   r10_lo[6];
				__u8   strict_priorate[8];
			} priorate;
			struct fw_port_dcb_pfc {
				__u8   type;
				__u8   pfcen;
				__u8   r10[5];
				__u8   max_pfc_tcs;
				__be64 r11;
			} pfc;
			struct fw_port_app_priority {
				__u8   type;
				__u8   r10[2];
				__u8   idx;
				__u8   user_prio_map;
				__u8   sel_field;
				__be16 protocolid;
				__be64 r12;
			} app_priority;
			struct fw_port_dcb_control {
				__u8   type;
				__u8   all_syncd_pkd;
				__be16 dcb_version_to_app_state;
				__be32 r11;
				__be64 r12;
			} control;
		} dcb;
		struct fw_port_l1cfg32 {
			__be32 rcap32;
			__be32 r;
		} l1cfg32;
		struct fw_port_info32 {
			__be32 lstatus32_to_cbllen32;
			__be32 auxlinfo32_mtu32;
			__be32 linkattr32;
			__be32 pcaps32;
			__be32 acaps32;
			__be32 lpacaps32;
		} info32;
	} u;
};

#define S_FW_PORT_CMD_PORTID	0
#define M_FW_PORT_CMD_PORTID	0xf
#define V_FW_PORT_CMD_PORTID(x)	((x) << S_FW_PORT_CMD_PORTID)
#define G_FW_PORT_CMD_PORTID(x)	\
	(((x) >> S_FW_PORT_CMD_PORTID) & M_FW_PORT_CMD_PORTID)

#define S_FW_PORT_CMD_ACTION	16
#define M_FW_PORT_CMD_ACTION	0xffff
#define V_FW_PORT_CMD_ACTION(x)	((x) << S_FW_PORT_CMD_ACTION)
#define G_FW_PORT_CMD_ACTION(x)	\
	(((x) >> S_FW_PORT_CMD_ACTION) & M_FW_PORT_CMD_ACTION)

#define S_FW_PORT_CMD_LSTATUS		31
#define M_FW_PORT_CMD_LSTATUS		0x1
#define V_FW_PORT_CMD_LSTATUS(x)	((x) << S_FW_PORT_CMD_LSTATUS)
#define G_FW_PORT_CMD_LSTATUS(x)	\
	(((x) >> S_FW_PORT_CMD_LSTATUS) & M_FW_PORT_CMD_LSTATUS)
#define F_FW_PORT_CMD_LSTATUS	V_FW_PORT_CMD_LSTATUS(1U)

#define S_FW_PORT_CMD_LSPEED	24
#define M_FW_PORT_CMD_LSPEED	0x3f
#define V_FW_PORT_CMD_LSPEED(x)	((x) << S_FW_PORT_CMD_LSPEED)
#define G_FW_PORT_CMD_LSPEED(x)	\
	(((x) >> S_FW_PORT_CMD_LSPEED) & M_FW_PORT_CMD_LSPEED)

#define S_FW_PORT_CMD_TXPAUSE		23
#define M_FW_PORT_CMD_TXPAUSE		0x1
#define V_FW_PORT_CMD_TXPAUSE(x)	((x) << S_FW_PORT_CMD_TXPAUSE)
#define G_FW_PORT_CMD_TXPAUSE(x)	\
	(((x) >> S_FW_PORT_CMD_TXPAUSE) & M_FW_PORT_CMD_TXPAUSE)
#define F_FW_PORT_CMD_TXPAUSE	V_FW_PORT_CMD_TXPAUSE(1U)

#define S_FW_PORT_CMD_RXPAUSE		22
#define M_FW_PORT_CMD_RXPAUSE		0x1
#define V_FW_PORT_CMD_RXPAUSE(x)	((x) << S_FW_PORT_CMD_RXPAUSE)
#define G_FW_PORT_CMD_RXPAUSE(x)	\
	(((x) >> S_FW_PORT_CMD_RXPAUSE) & M_FW_PORT_CMD_RXPAUSE)
#define F_FW_PORT_CMD_RXPAUSE	V_FW_PORT_CMD_RXPAUSE(1U)

#define S_FW_PORT_CMD_PTYPE	8
#define M_FW_PORT_CMD_PTYPE	0x1f
#define V_FW_PORT_CMD_PTYPE(x)	((x) << S_FW_PORT_CMD_PTYPE)
#define G_FW_PORT_CMD_PTYPE(x)	\
	(((x) >> S_FW_PORT_CMD_PTYPE) & M_FW_PORT_CMD_PTYPE)

#define S_FW_PORT_CMD_LSTATUS32                31
#define M_FW_PORT_CMD_LSTATUS32                0x1
#define V_FW_PORT_CMD_LSTATUS32(x)     ((x) << S_FW_PORT_CMD_LSTATUS32)
#define F_FW_PORT_CMD_LSTATUS32        V_FW_PORT_CMD_LSTATUS32(1U)

#define S_FW_PORT_CMD_LINKDNRC32       28
#define M_FW_PORT_CMD_LINKDNRC32       0x7
#define G_FW_PORT_CMD_LINKDNRC32(x)    \
	(((x) >> S_FW_PORT_CMD_LINKDNRC32) & M_FW_PORT_CMD_LINKDNRC32)

#define S_FW_PORT_CMD_MDIOCAP32                26
#define M_FW_PORT_CMD_MDIOCAP32                0x1
#define V_FW_PORT_CMD_MDIOCAP32(x)     ((x) << S_FW_PORT_CMD_MDIOCAP32)
#define F_FW_PORT_CMD_MDIOCAP32        V_FW_PORT_CMD_MDIOCAP32(1U)

#define S_FW_PORT_CMD_MDIOADDR32       21
#define M_FW_PORT_CMD_MDIOADDR32       0x1f
#define G_FW_PORT_CMD_MDIOADDR32(x)    \
	(((x) >> S_FW_PORT_CMD_MDIOADDR32) & M_FW_PORT_CMD_MDIOADDR32)

#define S_FW_PORT_CMD_PORTTYPE32        13
#define M_FW_PORT_CMD_PORTTYPE32        0xff
#define G_FW_PORT_CMD_PORTTYPE32(x)     \
	(((x) >> S_FW_PORT_CMD_PORTTYPE32) & M_FW_PORT_CMD_PORTTYPE32)

#define S_FW_PORT_CMD_MODTYPE32                8
#define M_FW_PORT_CMD_MODTYPE32                0x1f
#define G_FW_PORT_CMD_MODTYPE32(x)     \
	(((x) >> S_FW_PORT_CMD_MODTYPE32) & M_FW_PORT_CMD_MODTYPE32)

/*
 * These are configured into the VPD and hence tools that generate
 * VPD may use this enumeration.
 * extPHY #lanes T4_I2C extI2C BP_Eq BP_ANEG Speed
 *
 * REMEMBER:
 * Update the Common Code t4_hw.c:t4_get_port_type_description()
 * with any new Firmware Port Technology Types!
 */
enum fw_port_type {
	FW_PORT_TYPE_FIBER_XFI	=  0, /* Y, 1, N, Y, N, N, 10G */
	FW_PORT_TYPE_FIBER_XAUI	=  1, /* Y, 4, N, Y, N, N, 10G */
	FW_PORT_TYPE_BT_SGMII	=  2, /* Y, 1, No, No, No, No, 1G/100M */
	FW_PORT_TYPE_BT_XFI	=  3, /* Y, 1, No, No, No, No, 10G */
	FW_PORT_TYPE_BT_XAUI	=  4, /* Y, 4, No, No, No, No, 10G/1G/100M? */
	FW_PORT_TYPE_KX4	=  5, /* No, 4, No, No, Yes, Yes, 10G */
	FW_PORT_TYPE_CX4	=  6, /* No, 4, No, No, No, No, 10G */
	FW_PORT_TYPE_KX		=  7, /* No, 1, No, No, Yes, No, 1G */
	FW_PORT_TYPE_KR		=  8, /* No, 1, No, No, Yes, Yes, 10G */
	FW_PORT_TYPE_SFP	=  9, /* No, 1, Yes, No, No, No, 10G */
	FW_PORT_TYPE_BP_AP	= 10,
	/* No, 1, No, No, Yes, Yes, 10G, BP ANGE */
	FW_PORT_TYPE_BP4_AP	= 11,
	/* No, 4, No, No, Yes, Yes, 10G, BP ANGE */
	FW_PORT_TYPE_QSFP_10G	= 12, /* No, 1, Yes, No, No, No, 10G */
	FW_PORT_TYPE_QSA	= 13, /* No, 1, Yes, No, No, No, 10G */
	FW_PORT_TYPE_QSFP	= 14, /* No, 4, Yes, No, No, No, 40G */
	FW_PORT_TYPE_BP40_BA	= 15,
	/* No, 4, No, No, Yes, Yes, 40G/10G/1G, BP ANGE */
	FW_PORT_TYPE_KR4_100G	= 16, /* No, 4, 100G/40G/25G, Backplane */
	FW_PORT_TYPE_CR4_QSFP	= 17, /* No, 4, 100G/40G/25G */
	FW_PORT_TYPE_CR_QSFP	= 18, /* No, 1, 25G Spider cable */
	FW_PORT_TYPE_CR2_QSFP	= 19, /* No, 2, 50G */
	FW_PORT_TYPE_SFP28	= 20, /* No, 1, 25G/10G/1G */
	FW_PORT_TYPE_KR_SFP28	= 21, /* No, 1, 25G/10G/1G using Backplane */
	FW_PORT_TYPE_NONE = M_FW_PORT_CMD_PTYPE
};

/* These are read from module's EEPROM and determined once the
 * module is inserted.
 */
enum fw_port_module_type {
	FW_PORT_MOD_TYPE_NA		= 0x0,
	FW_PORT_MOD_TYPE_LR		= 0x1,
	FW_PORT_MOD_TYPE_SR		= 0x2,
	FW_PORT_MOD_TYPE_ER		= 0x3,
	FW_PORT_MOD_TYPE_TWINAX_PASSIVE	= 0x4,
	FW_PORT_MOD_TYPE_TWINAX_ACTIVE	= 0x5,
	FW_PORT_MOD_TYPE_LRM		= 0x6,
	FW_PORT_MOD_TYPE_ERROR		= M_FW_PORT_CMD_MODTYPE32 - 3,
	FW_PORT_MOD_TYPE_UNKNOWN	= M_FW_PORT_CMD_MODTYPE32 - 2,
	FW_PORT_MOD_TYPE_NOTSUPPORTED	= M_FW_PORT_CMD_MODTYPE32 - 1,
	FW_PORT_MOD_TYPE_NONE		= M_FW_PORT_CMD_MODTYPE32
};

/* used by FW and tools may use this to generate VPD */
enum fw_port_mod_sub_type {
	FW_PORT_MOD_SUB_TYPE_NA,
	FW_PORT_MOD_SUB_TYPE_MV88E114X	= 0x1,
	FW_PORT_MOD_SUB_TYPE_TN8022	= 0x2,
	FW_PORT_MOD_SUB_TYPE_AQ1202	= 0x3,
	FW_PORT_MOD_SUB_TYPE_88x3120	= 0x4,
	FW_PORT_MOD_SUB_TYPE_BCM84834	= 0x5,
	FW_PORT_MOD_SUB_TYPE_BCM5482	= 0x6,
	FW_PORT_MOD_SUB_TYPE_BCM84856	= 0x7,
	FW_PORT_MOD_SUB_TYPE_BT_VSC8634	= 0x8,

	/*
	 * The following will never been in the VPD.  They are TWINAX cable
	 * lengths decoded from SFP+ module i2c PROMs.  These should almost
	 * certainly go somewhere else ...
	 */
	FW_PORT_MOD_SUB_TYPE_TWINAX_1	= 0x9,
	FW_PORT_MOD_SUB_TYPE_TWINAX_3	= 0xA,
	FW_PORT_MOD_SUB_TYPE_TWINAX_5	= 0xB,
	FW_PORT_MOD_SUB_TYPE_TWINAX_7	= 0xC,
};

/* link down reason codes (3b) */
enum fw_port_link_dn_rc {
	FW_PORT_LINK_DN_RC_NONE,
	FW_PORT_LINK_DN_RC_REMFLT,	/* Remote fault detected */
	FW_PORT_LINK_DN_ANEG_F,		/* Auto-negotiation fault */
	FW_PORT_LINK_DN_RESERVED3,
	FW_PORT_LINK_DN_OVERHEAT,	/* Port overheated */
	FW_PORT_LINK_DN_UNKNOWN,	/* Unable to determine reason */
	FW_PORT_LINK_DN_RX_LOS,		/* No RX signal detected */
	FW_PORT_LINK_DN_RESERVED7
};

/* port stats */
#define FW_NUM_PORT_STATS 50
#define FW_NUM_PORT_TX_STATS 23
#define FW_NUM_PORT_RX_STATS 27

enum fw_port_stats_tx_index {
	FW_STAT_TX_PORT_BYTES_IX,
	FW_STAT_TX_PORT_FRAMES_IX,
	FW_STAT_TX_PORT_BCAST_IX,
	FW_STAT_TX_PORT_MCAST_IX,
	FW_STAT_TX_PORT_UCAST_IX,
	FW_STAT_TX_PORT_ERROR_IX,
	FW_STAT_TX_PORT_64B_IX,
	FW_STAT_TX_PORT_65B_127B_IX,
	FW_STAT_TX_PORT_128B_255B_IX,
	FW_STAT_TX_PORT_256B_511B_IX,
	FW_STAT_TX_PORT_512B_1023B_IX,
	FW_STAT_TX_PORT_1024B_1518B_IX,
	FW_STAT_TX_PORT_1519B_MAX_IX,
	FW_STAT_TX_PORT_DROP_IX,
	FW_STAT_TX_PORT_PAUSE_IX,
	FW_STAT_TX_PORT_PPP0_IX,
	FW_STAT_TX_PORT_PPP1_IX,
	FW_STAT_TX_PORT_PPP2_IX,
	FW_STAT_TX_PORT_PPP3_IX,
	FW_STAT_TX_PORT_PPP4_IX,
	FW_STAT_TX_PORT_PPP5_IX,
	FW_STAT_TX_PORT_PPP6_IX,
	FW_STAT_TX_PORT_PPP7_IX
};

enum fw_port_stat_rx_index {
	FW_STAT_RX_PORT_BYTES_IX,
	FW_STAT_RX_PORT_FRAMES_IX,
	FW_STAT_RX_PORT_BCAST_IX,
	FW_STAT_RX_PORT_MCAST_IX,
	FW_STAT_RX_PORT_UCAST_IX,
	FW_STAT_RX_PORT_MTU_ERROR_IX,
	FW_STAT_RX_PORT_MTU_CRC_ERROR_IX,
	FW_STAT_RX_PORT_CRC_ERROR_IX,
	FW_STAT_RX_PORT_LEN_ERROR_IX,
	FW_STAT_RX_PORT_SYM_ERROR_IX,
	FW_STAT_RX_PORT_64B_IX,
	FW_STAT_RX_PORT_65B_127B_IX,
	FW_STAT_RX_PORT_128B_255B_IX,
	FW_STAT_RX_PORT_256B_511B_IX,
	FW_STAT_RX_PORT_512B_1023B_IX,
	FW_STAT_RX_PORT_1024B_1518B_IX,
	FW_STAT_RX_PORT_1519B_MAX_IX,
	FW_STAT_RX_PORT_PAUSE_IX,
	FW_STAT_RX_PORT_PPP0_IX,
	FW_STAT_RX_PORT_PPP1_IX,
	FW_STAT_RX_PORT_PPP2_IX,
	FW_STAT_RX_PORT_PPP3_IX,
	FW_STAT_RX_PORT_PPP4_IX,
	FW_STAT_RX_PORT_PPP5_IX,
	FW_STAT_RX_PORT_PPP6_IX,
	FW_STAT_RX_PORT_PPP7_IX,
	FW_STAT_RX_PORT_LESS_64B_IX
};

struct fw_port_stats_cmd {
	__be32 op_to_portid;
	__be32 retval_len16;
	union fw_port_stats {
		struct fw_port_stats_ctl {
			__u8   nstats_bg_bm;
			__u8   tx_ix;
			__be16 r6;
			__be32 r7;
			__be64 stat0;
			__be64 stat1;
			__be64 stat2;
			__be64 stat3;
			__be64 stat4;
			__be64 stat5;
		} ctl;
		struct fw_port_stats_all {
			__be64 tx_bytes;
			__be64 tx_frames;
			__be64 tx_bcast;
			__be64 tx_mcast;
			__be64 tx_ucast;
			__be64 tx_error;
			__be64 tx_64b;
			__be64 tx_65b_127b;
			__be64 tx_128b_255b;
			__be64 tx_256b_511b;
			__be64 tx_512b_1023b;
			__be64 tx_1024b_1518b;
			__be64 tx_1519b_max;
			__be64 tx_drop;
			__be64 tx_pause;
			__be64 tx_ppp0;
			__be64 tx_ppp1;
			__be64 tx_ppp2;
			__be64 tx_ppp3;
			__be64 tx_ppp4;
			__be64 tx_ppp5;
			__be64 tx_ppp6;
			__be64 tx_ppp7;
			__be64 rx_bytes;
			__be64 rx_frames;
			__be64 rx_bcast;
			__be64 rx_mcast;
			__be64 rx_ucast;
			__be64 rx_mtu_error;
			__be64 rx_mtu_crc_error;
			__be64 rx_crc_error;
			__be64 rx_len_error;
			__be64 rx_sym_error;
			__be64 rx_64b;
			__be64 rx_65b_127b;
			__be64 rx_128b_255b;
			__be64 rx_256b_511b;
			__be64 rx_512b_1023b;
			__be64 rx_1024b_1518b;
			__be64 rx_1519b_max;
			__be64 rx_pause;
			__be64 rx_ppp0;
			__be64 rx_ppp1;
			__be64 rx_ppp2;
			__be64 rx_ppp3;
			__be64 rx_ppp4;
			__be64 rx_ppp5;
			__be64 rx_ppp6;
			__be64 rx_ppp7;
			__be64 rx_less_64b;
			__be64 rx_bg_drop;
			__be64 rx_bg_trunc;
		} all;
	} u;
};

struct fw_rss_ind_tbl_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	__be16 niqid;
	__be16 startidx;
	__be32 r3;
	__be32 iq0_to_iq2;
	__be32 iq3_to_iq5;
	__be32 iq6_to_iq8;
	__be32 iq9_to_iq11;
	__be32 iq12_to_iq14;
	__be32 iq15_to_iq17;
	__be32 iq18_to_iq20;
	__be32 iq21_to_iq23;
	__be32 iq24_to_iq26;
	__be32 iq27_to_iq29;
	__be32 iq30_iq31;
	__be32 r15_lo;
};

#define S_FW_RSS_IND_TBL_CMD_VIID	0
#define M_FW_RSS_IND_TBL_CMD_VIID	0xfff
#define V_FW_RSS_IND_TBL_CMD_VIID(x)	((x) << S_FW_RSS_IND_TBL_CMD_VIID)
#define G_FW_RSS_IND_TBL_CMD_VIID(x)	\
	(((x) >> S_FW_RSS_IND_TBL_CMD_VIID) & M_FW_RSS_IND_TBL_CMD_VIID)

#define S_FW_RSS_IND_TBL_CMD_IQ0	20
#define M_FW_RSS_IND_TBL_CMD_IQ0	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ0(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ0)
#define G_FW_RSS_IND_TBL_CMD_IQ0(x)	\
	(((x) >> S_FW_RSS_IND_TBL_CMD_IQ0) & M_FW_RSS_IND_TBL_CMD_IQ0)

#define S_FW_RSS_IND_TBL_CMD_IQ1	10
#define M_FW_RSS_IND_TBL_CMD_IQ1	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ1(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ1)
#define G_FW_RSS_IND_TBL_CMD_IQ1(x)	\
	(((x) >> S_FW_RSS_IND_TBL_CMD_IQ1) & M_FW_RSS_IND_TBL_CMD_IQ1)

#define S_FW_RSS_IND_TBL_CMD_IQ2	0
#define M_FW_RSS_IND_TBL_CMD_IQ2	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ2(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ2)
#define G_FW_RSS_IND_TBL_CMD_IQ2(x)	\
	(((x) >> S_FW_RSS_IND_TBL_CMD_IQ2) & M_FW_RSS_IND_TBL_CMD_IQ2)

struct fw_rss_glb_config_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	union fw_rss_glb_config {
		struct fw_rss_glb_config_manual {
			__be32 mode_pkd;
			__be32 r3;
			__be64 r4;
			__be64 r5;
		} manual;
		struct fw_rss_glb_config_basicvirtual {
			__be32 mode_keymode;
			__be32 synmapen_to_hashtoeplitz;
			__be64 r8;
			__be64 r9;
		} basicvirtual;
	} u;
};

#define S_FW_RSS_GLB_CONFIG_CMD_MODE    28
#define M_FW_RSS_GLB_CONFIG_CMD_MODE    0xf
#define G_FW_RSS_GLB_CONFIG_CMD_MODE(x) \
	(((x) >> S_FW_RSS_GLB_CONFIG_CMD_MODE) & M_FW_RSS_GLB_CONFIG_CMD_MODE)

#define FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL 1

#define S_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN 8
#define V_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN)
#define F_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN V_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6 7
#define V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6 \
	V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6 6
#define V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6 \
	V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4 5
#define V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4 \
	V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4 4
#define V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4 \
	V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN 3
#define V_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN)
#define F_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN V_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN 2
#define V_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN)
#define F_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN V_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP 1
#define V_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP)
#define F_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP \
	V_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ 0
#define V_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ(x) \
	((x) << S_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ)
#define F_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ \
	V_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ(1U)

struct fw_rss_vi_config_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	union fw_rss_vi_config {
		struct fw_rss_vi_config_manual {
			__be64 r3;
			__be64 r4;
			__be64 r5;
		} manual;
		struct fw_rss_vi_config_basicvirtual {
			__be32 r6;
			__be32 defaultq_to_udpen;
			__be64 r9;
			__be64 r10;
		} basicvirtual;
	} u;
};

#define S_FW_RSS_VI_CONFIG_CMD_VIID	0
#define M_FW_RSS_VI_CONFIG_CMD_VIID	0xfff
#define V_FW_RSS_VI_CONFIG_CMD_VIID(x)	((x) << S_FW_RSS_VI_CONFIG_CMD_VIID)
#define G_FW_RSS_VI_CONFIG_CMD_VIID(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_VIID) & M_FW_RSS_VI_CONFIG_CMD_VIID)

#define S_FW_RSS_VI_CONFIG_CMD_DEFAULTQ		16
#define M_FW_RSS_VI_CONFIG_CMD_DEFAULTQ		0x3ff
#define V_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(x)	\
	((x) << S_FW_RSS_VI_CONFIG_CMD_DEFAULTQ)
#define G_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_DEFAULTQ) & \
	 M_FW_RSS_VI_CONFIG_CMD_DEFAULTQ)

#define S_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN	4
#define M_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN(x)	\
	((x) << S_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN) & \
	 M_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN	\
	V_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN	3
#define M_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN(x)	\
	((x) << S_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN) & \
	 M_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN	\
	V_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN	2
#define M_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN(x)	\
	((x) << S_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN) & \
	 M_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN	\
	V_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN	1
#define M_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN(x)	\
	((x) << S_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN) & \
	 M_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN	\
	V_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_UDPEN	0
#define M_FW_RSS_VI_CONFIG_CMD_UDPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_UDPEN(x)	((x) << S_FW_RSS_VI_CONFIG_CMD_UDPEN)
#define G_FW_RSS_VI_CONFIG_CMD_UDPEN(x)	\
	(((x) >> S_FW_RSS_VI_CONFIG_CMD_UDPEN) & M_FW_RSS_VI_CONFIG_CMD_UDPEN)
#define F_FW_RSS_VI_CONFIG_CMD_UDPEN	V_FW_RSS_VI_CONFIG_CMD_UDPEN(1U)

struct fw_clip_cmd {
	__be32 op_to_write;
	__be32 alloc_to_len16;
	__be64 ip_hi;
	__be64 ip_lo;
	__be32 r4[2];
};

#define S_FW_CLIP_CMD_ALLOC		31
#define V_FW_CLIP_CMD_ALLOC(x)		((x) << S_FW_CLIP_CMD_ALLOC)
#define F_FW_CLIP_CMD_ALLOC		V_FW_CLIP_CMD_ALLOC(1U)

#define S_FW_CLIP_CMD_FREE		30
#define V_FW_CLIP_CMD_FREE(x)		((x) << S_FW_CLIP_CMD_FREE)
#define F_FW_CLIP_CMD_FREE		V_FW_CLIP_CMD_FREE(1U)

/******************************************************************************
 *   D E B U G   C O M M A N D s
 ******************************************************/

struct fw_debug_cmd {
	__be32 op_type;
	__be32 len16_pkd;
	union fw_debug {
		struct fw_debug_assert {
			__be32 fcid;
			__be32 line;
			__be32 x;
			__be32 y;
			__u8   filename_0_7[8];
			__u8   filename_8_15[8];
			__be64 r3;
		} assert;
		struct fw_debug_prt {
			__be16 dprtstridx;
			__be16 r3[3];
			__be32 dprtstrparam0;
			__be32 dprtstrparam1;
			__be32 dprtstrparam2;
			__be32 dprtstrparam3;
		} prt;
	} u;
};

#define S_FW_DEBUG_CMD_TYPE	0
#define M_FW_DEBUG_CMD_TYPE	0xff
#define V_FW_DEBUG_CMD_TYPE(x)	((x) << S_FW_DEBUG_CMD_TYPE)
#define G_FW_DEBUG_CMD_TYPE(x)	\
	(((x) >> S_FW_DEBUG_CMD_TYPE) & M_FW_DEBUG_CMD_TYPE)

/******************************************************************************
 *   P C I E   F W   R E G I S T E R
 **************************************/

/*
 * Register definitions for the PCIE_FW register which the firmware uses
 * to retain status across RESETs.  This register should be considered
 * as a READ-ONLY register for Host Software and only to be used to
 * track firmware initialization/error state, etc.
 */
#define S_PCIE_FW_ERR		31
#define M_PCIE_FW_ERR		0x1
#define V_PCIE_FW_ERR(x)	((x) << S_PCIE_FW_ERR)
#define G_PCIE_FW_ERR(x)	(((x) >> S_PCIE_FW_ERR) & M_PCIE_FW_ERR)
#define F_PCIE_FW_ERR		V_PCIE_FW_ERR(1U)

#define S_PCIE_FW_INIT		30
#define M_PCIE_FW_INIT		0x1
#define V_PCIE_FW_INIT(x)	((x) << S_PCIE_FW_INIT)
#define G_PCIE_FW_INIT(x)	(((x) >> S_PCIE_FW_INIT) & M_PCIE_FW_INIT)
#define F_PCIE_FW_INIT		V_PCIE_FW_INIT(1U)

#define S_PCIE_FW_HALT          29
#define M_PCIE_FW_HALT          0x1
#define V_PCIE_FW_HALT(x)       ((x) << S_PCIE_FW_HALT)
#define G_PCIE_FW_HALT(x)       (((x) >> S_PCIE_FW_HALT) & M_PCIE_FW_HALT)
#define F_PCIE_FW_HALT          V_PCIE_FW_HALT(1U)

#define S_PCIE_FW_EVAL		24
#define M_PCIE_FW_EVAL		0x7
#define V_PCIE_FW_EVAL(x)	((x) << S_PCIE_FW_EVAL)
#define G_PCIE_FW_EVAL(x)	(((x) >> S_PCIE_FW_EVAL) & M_PCIE_FW_EVAL)

#define S_PCIE_FW_MASTER_VLD	15
#define M_PCIE_FW_MASTER_VLD	0x1
#define V_PCIE_FW_MASTER_VLD(x)	((x) << S_PCIE_FW_MASTER_VLD)
#define G_PCIE_FW_MASTER_VLD(x)	\
	(((x) >> S_PCIE_FW_MASTER_VLD) & M_PCIE_FW_MASTER_VLD)
#define F_PCIE_FW_MASTER_VLD	V_PCIE_FW_MASTER_VLD(1U)

#define S_PCIE_FW_MASTER	12
#define M_PCIE_FW_MASTER	0x7
#define V_PCIE_FW_MASTER(x)	((x) << S_PCIE_FW_MASTER)
#define G_PCIE_FW_MASTER(x)	(((x) >> S_PCIE_FW_MASTER) & M_PCIE_FW_MASTER)

/******************************************************************************
 *   B I N A R Y   H E A D E R   F O R M A T
 **********************************************/

/*
 * firmware binary header format
 */
struct fw_hdr {
	__u8	ver;
	__u8	chip;			/* terminator chip family */
	__be16	len512;			/* bin length in units of 512-bytes */
	__be32	fw_ver;			/* firmware version */
	__be32	tp_microcode_ver;	/* tcp processor microcode version */
	__u8	intfver_nic;
	__u8	intfver_vnic;
	__u8	intfver_ofld;
	__u8	intfver_ri;
	__u8	intfver_iscsipdu;
	__u8	intfver_iscsi;
	__u8	intfver_fcoepdu;
	__u8	intfver_fcoe;
	__u32	reserved2;
	__u32	reserved3;
	__u32	magic;			/* runtime or bootstrap fw */
	__be32	flags;
	__be32	reserved6[23];
};

#define S_FW_HDR_FW_VER_MAJOR	24
#define M_FW_HDR_FW_VER_MAJOR	0xff
#define V_FW_HDR_FW_VER_MAJOR(x) \
	((x) << S_FW_HDR_FW_VER_MAJOR)
#define G_FW_HDR_FW_VER_MAJOR(x) \
	(((x) >> S_FW_HDR_FW_VER_MAJOR) & M_FW_HDR_FW_VER_MAJOR)

#define S_FW_HDR_FW_VER_MINOR	16
#define M_FW_HDR_FW_VER_MINOR	0xff
#define V_FW_HDR_FW_VER_MINOR(x) \
	((x) << S_FW_HDR_FW_VER_MINOR)
#define G_FW_HDR_FW_VER_MINOR(x) \
	(((x) >> S_FW_HDR_FW_VER_MINOR) & M_FW_HDR_FW_VER_MINOR)

#define S_FW_HDR_FW_VER_MICRO	8
#define M_FW_HDR_FW_VER_MICRO	0xff
#define V_FW_HDR_FW_VER_MICRO(x) \
	((x) << S_FW_HDR_FW_VER_MICRO)
#define G_FW_HDR_FW_VER_MICRO(x) \
	(((x) >> S_FW_HDR_FW_VER_MICRO) & M_FW_HDR_FW_VER_MICRO)

#define S_FW_HDR_FW_VER_BUILD	0
#define M_FW_HDR_FW_VER_BUILD	0xff
#define V_FW_HDR_FW_VER_BUILD(x) \
	((x) << S_FW_HDR_FW_VER_BUILD)
#define G_FW_HDR_FW_VER_BUILD(x) \
	(((x) >> S_FW_HDR_FW_VER_BUILD) & M_FW_HDR_FW_VER_BUILD)

#endif /* _T4FW_INTERFACE_H_ */
