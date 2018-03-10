/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef __T4_HW_H
#define __T4_HW_H

enum {
	NCHAN           = 4,     /* # of HW channels */
	EEPROMSIZE      = 17408, /* Serial EEPROM physical size */
	EEPROMVSIZE     = 32768, /* Serial EEPROM virtual address space size */
	EEPROMPFSIZE    = 1024,  /* EEPROM writable area size for PFn, n>0 */
	NMTUS           = 16,    /* size of MTU table */
	NCCTRL_WIN      = 32,    /* # of congestion control windows */
	MBOX_LEN        = 64,    /* mailbox size in bytes */
	UDBS_SEG_SIZE   = 128,   /* segment size for BAR2 user doorbells */
};

enum {
	CIMLA_SIZE     = 2048,  /* # of 32-bit words in CIM LA */
};

enum {
	SF_SEC_SIZE = 64 * 1024,      /* serial flash sector size */
};

enum {
	SGE_NTIMERS = 6,          /* # of interrupt holdoff timer values */
	SGE_NCOUNTERS = 4,        /* # of interrupt packet counter values */
};

/* PCI-e memory window access */
enum pcie_memwin {
	MEMWIN_NIC      = 0,
};

enum {
	SGE_MAX_WR_LEN = 512,     /* max WR size in bytes */
	SGE_EQ_IDXSIZE = 64,      /* egress queue pidx/cidx unit size */
	/* max no. of desc allowed in WR */
	SGE_MAX_WR_NDESC = SGE_MAX_WR_LEN / SGE_EQ_IDXSIZE,
};

struct sge_qstat {                /* data written to SGE queue status entries */
	__be32 qid;
	__be16 cidx;
	__be16 pidx;
};

/*
 * Structure for last 128 bits of response descriptors
 */
struct rsp_ctrl {
	__be32 hdrbuflen_pidx;
	__be32 pldbuflen_qid;
	union {
		u8 type_gen;
		__be64 last_flit;
	} u;
};

#define S_RSPD_NEWBUF    31
#define V_RSPD_NEWBUF(x) ((x) << S_RSPD_NEWBUF)
#define F_RSPD_NEWBUF    V_RSPD_NEWBUF(1U)

#define S_RSPD_LEN    0
#define M_RSPD_LEN    0x7fffffff
#define V_RSPD_LEN(x) ((x) << S_RSPD_LEN)
#define G_RSPD_LEN(x) (((x) >> S_RSPD_LEN) & M_RSPD_LEN)

#define S_RSPD_GEN    7
#define V_RSPD_GEN(x) ((x) << S_RSPD_GEN)
#define F_RSPD_GEN    V_RSPD_GEN(1U)

#define S_RSPD_TYPE    4
#define M_RSPD_TYPE    0x3
#define V_RSPD_TYPE(x) ((x) << S_RSPD_TYPE)
#define G_RSPD_TYPE(x) (((x) >> S_RSPD_TYPE) & M_RSPD_TYPE)

/* Rx queue interrupt deferral field: timer index */
#define S_QINTR_CNT_EN    0
#define V_QINTR_CNT_EN(x) ((x) << S_QINTR_CNT_EN)
#define F_QINTR_CNT_EN    V_QINTR_CNT_EN(1U)

#define S_QINTR_TIMER_IDX    1
#define M_QINTR_TIMER_IDX    0x7
#define V_QINTR_TIMER_IDX(x) ((x) << S_QINTR_TIMER_IDX)
#define G_QINTR_TIMER_IDX(x) (((x) >> S_QINTR_TIMER_IDX) & M_QINTR_TIMER_IDX)

/*
 * Flash layout.
 */
#define FLASH_START(start)      ((start) * SF_SEC_SIZE)
#define FLASH_MAX_SIZE(nsecs)   ((nsecs) * SF_SEC_SIZE)

enum {
	/*
	 * Various Expansion-ROM boot images, etc.
	 */
	FLASH_EXP_ROM_START_SEC = 0,
	FLASH_EXP_ROM_NSECS = 6,
	FLASH_EXP_ROM_START = FLASH_START(FLASH_EXP_ROM_START_SEC),
	FLASH_EXP_ROM_MAX_SIZE = FLASH_MAX_SIZE(FLASH_EXP_ROM_NSECS),

	/*
	 * Location of firmware image in FLASH.
	 */
	FLASH_FW_START_SEC = 8,
	FLASH_FW_NSECS = 16,
	FLASH_FW_START = FLASH_START(FLASH_FW_START_SEC),
	FLASH_FW_MAX_SIZE = FLASH_MAX_SIZE(FLASH_FW_NSECS),

	/*
	 * Location of bootstrap firmware image in FLASH.
	 */
	FLASH_FWBOOTSTRAP_START_SEC = 27,
	FLASH_FWBOOTSTRAP_NSECS = 1,
	FLASH_FWBOOTSTRAP_START = FLASH_START(FLASH_FWBOOTSTRAP_START_SEC),
	FLASH_FWBOOTSTRAP_MAX_SIZE = FLASH_MAX_SIZE(FLASH_FWBOOTSTRAP_NSECS),

	/*
	 * Location of Firmware Configuration File in FLASH.
	 */
	FLASH_CFG_START_SEC = 31,
	FLASH_CFG_NSECS = 1,
	FLASH_CFG_START = FLASH_START(FLASH_CFG_START_SEC),
	FLASH_CFG_MAX_SIZE = FLASH_MAX_SIZE(FLASH_CFG_NSECS),

	/*
	 * We don't support FLASH devices which can't support the full
	 * standard set of sections which we need for normal operations.
	 */
	FLASH_MIN_SIZE = FLASH_CFG_START + FLASH_CFG_MAX_SIZE,
};

#undef FLASH_START
#undef FLASH_MAX_SIZE

#endif /* __T4_HW_H */
