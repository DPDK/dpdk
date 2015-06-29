/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Chelsio Communications.
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
 *     * Neither the name of Chelsio Communications nor the names of its
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
	FW_ETH_TX_PKT_WR	= 0x08,
	FW_ETH_TX_PKTS_WR	= 0x09,
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
	FW_RESET_CMD                   = 0x03,
	FW_HELLO_CMD                   = 0x04,
	FW_BYE_CMD                     = 0x05,
	FW_INITIALIZE_CMD              = 0x06,
	FW_CAPS_CONFIG_CMD             = 0x07,
	FW_PARAMS_CMD                  = 0x08,
	FW_IQ_CMD                      = 0x10,
	FW_EQ_ETH_CMD                  = 0x12,
	FW_VI_CMD                      = 0x14,
	FW_VI_MAC_CMD                  = 0x15,
	FW_VI_RXMODE_CMD               = 0x16,
	FW_VI_ENABLE_CMD               = 0x17,
	FW_PORT_CMD                    = 0x1b,
	FW_RSS_IND_TBL_CMD             = 0x20,
	FW_RSS_VI_CONFIG_CMD           = 0x23,
	FW_DEBUG_CMD                   = 0x81,
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
	__be16 r4;
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
	FW_PARAMS_MNEM_DMAQ		= 4,	/* dma queue params */
};

/*
 * device parameters
 */
enum fw_params_param_dev {
	FW_PARAMS_PARAM_DEV_CCLK	= 0x00, /* chip core clock in khz */
	FW_PARAMS_PARAM_DEV_PORTVEC	= 0x01, /* the port vector */
	FW_PARAMS_PARAM_DEV_ULPTX_MEMWRITE_DSGL = 0x17,
};

/*
 * physical and virtual function parameters
 */
enum fw_params_param_pfvf {
	FW_PARAMS_PARAM_PFVF_CPLFW4MSG_ENCAP = 0x31
};

/*
 * dma queue parameters
 */
enum fw_params_param_dmaq {
	FW_PARAMS_PARAM_DMAQ_IQ_INTCNTTHRESH = 0x01,
	FW_PARAMS_PARAM_DMAQ_CONM_CTXT = 0x20,
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

/*
 * ingress queue type; the first 1K ingress queues can have associated 0,
 * 1 or 2 free lists and an interrupt, all other ingress queues lack these
 * capabilities
 */
enum fw_iq_type {
	FW_IQ_TYPE_FL_INT_CAP,
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

#define S_FW_IQ_CMD_IQFLINTCONGEN	27
#define M_FW_IQ_CMD_IQFLINTCONGEN	0x1
#define V_FW_IQ_CMD_IQFLINTCONGEN(x)	((x) << S_FW_IQ_CMD_IQFLINTCONGEN)
#define G_FW_IQ_CMD_IQFLINTCONGEN(x)	\
	(((x) >> S_FW_IQ_CMD_IQFLINTCONGEN) & M_FW_IQ_CMD_IQFLINTCONGEN)
#define F_FW_IQ_CMD_IQFLINTCONGEN	V_FW_IQ_CMD_IQFLINTCONGEN(1U)

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

enum fw_vi_func {
	FW_VI_FUNC_ETH,
};

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

enum fw_vi_mac_smac {
	FW_VI_MAC_MPS_TCAM_ENTRY,
	FW_VI_MAC_SMT_AND_MPSTCAM
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
	} u;
};

#define S_FW_VI_MAC_CMD_VIID	0
#define M_FW_VI_MAC_CMD_VIID	0xfff
#define V_FW_VI_MAC_CMD_VIID(x)	((x) << S_FW_VI_MAC_CMD_VIID)
#define G_FW_VI_MAC_CMD_VIID(x)	\
	(((x) >> S_FW_VI_MAC_CMD_VIID) & M_FW_VI_MAC_CMD_VIID)

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

/* port capabilities bitmap */
enum fw_port_cap {
	FW_PORT_CAP_SPEED_100M		= 0x0001,
	FW_PORT_CAP_SPEED_1G		= 0x0002,
	FW_PORT_CAP_SPEED_2_5G		= 0x0004,
	FW_PORT_CAP_SPEED_10G		= 0x0008,
	FW_PORT_CAP_SPEED_40G		= 0x0010,
	FW_PORT_CAP_SPEED_100G		= 0x0020,
	FW_PORT_CAP_FC_RX		= 0x0040,
	FW_PORT_CAP_FC_TX		= 0x0080,
	FW_PORT_CAP_ANEG		= 0x0100,
	FW_PORT_CAP_MDIX		= 0x0200,
	FW_PORT_CAP_MDIAUTO		= 0x0400,
	FW_PORT_CAP_FEC			= 0x0800,
	FW_PORT_CAP_TECHKR		= 0x1000,
	FW_PORT_CAP_TECHKX4		= 0x2000,
	FW_PORT_CAP_802_3_PAUSE		= 0x4000,
	FW_PORT_CAP_802_3_ASM_DIR	= 0x8000,
};

enum fw_port_mdi {
	FW_PORT_CAP_MDI_AUTO,
};

#define S_FW_PORT_CAP_MDI 9
#define M_FW_PORT_CAP_MDI 3
#define V_FW_PORT_CAP_MDI(x) ((x) << S_FW_PORT_CAP_MDI)
#define G_FW_PORT_CAP_MDI(x) (((x) >> S_FW_PORT_CAP_MDI) & M_FW_PORT_CAP_MDI)

enum fw_port_action {
	FW_PORT_ACTION_L1_CFG		= 0x0001,
	FW_PORT_ACTION_GET_PORT_INFO	= 0x0003,
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

#define S_FW_PORT_CMD_MDIOCAP		21
#define M_FW_PORT_CMD_MDIOCAP		0x1
#define V_FW_PORT_CMD_MDIOCAP(x)	((x) << S_FW_PORT_CMD_MDIOCAP)
#define G_FW_PORT_CMD_MDIOCAP(x)	\
	(((x) >> S_FW_PORT_CMD_MDIOCAP) & M_FW_PORT_CMD_MDIOCAP)
#define F_FW_PORT_CMD_MDIOCAP	V_FW_PORT_CMD_MDIOCAP(1U)

#define S_FW_PORT_CMD_MDIOADDR		16
#define M_FW_PORT_CMD_MDIOADDR		0x1f
#define V_FW_PORT_CMD_MDIOADDR(x)	((x) << S_FW_PORT_CMD_MDIOADDR)
#define G_FW_PORT_CMD_MDIOADDR(x)	\
	(((x) >> S_FW_PORT_CMD_MDIOADDR) & M_FW_PORT_CMD_MDIOADDR)

#define S_FW_PORT_CMD_PTYPE	8
#define M_FW_PORT_CMD_PTYPE	0x1f
#define V_FW_PORT_CMD_PTYPE(x)	((x) << S_FW_PORT_CMD_PTYPE)
#define G_FW_PORT_CMD_PTYPE(x)	\
	(((x) >> S_FW_PORT_CMD_PTYPE) & M_FW_PORT_CMD_PTYPE)

#define S_FW_PORT_CMD_LINKDNRC		5
#define M_FW_PORT_CMD_LINKDNRC		0x7
#define V_FW_PORT_CMD_LINKDNRC(x)	((x) << S_FW_PORT_CMD_LINKDNRC)
#define G_FW_PORT_CMD_LINKDNRC(x)	\
	(((x) >> S_FW_PORT_CMD_LINKDNRC) & M_FW_PORT_CMD_LINKDNRC)

#define S_FW_PORT_CMD_MODTYPE		0
#define M_FW_PORT_CMD_MODTYPE		0x1f
#define V_FW_PORT_CMD_MODTYPE(x)	((x) << S_FW_PORT_CMD_MODTYPE)
#define G_FW_PORT_CMD_MODTYPE(x)	\
	(((x) >> S_FW_PORT_CMD_MODTYPE) & M_FW_PORT_CMD_MODTYPE)

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
	FW_PORT_MOD_TYPE_ERROR		= M_FW_PORT_CMD_MODTYPE - 3,
	FW_PORT_MOD_TYPE_UNKNOWN	= M_FW_PORT_CMD_MODTYPE - 2,
	FW_PORT_MOD_TYPE_NOTSUPPORTED	= M_FW_PORT_CMD_MODTYPE - 1,
	FW_PORT_MOD_TYPE_NONE		= M_FW_PORT_CMD_MODTYPE
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
