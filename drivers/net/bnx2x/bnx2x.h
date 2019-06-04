/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2007-2013 Broadcom Corporation.
 *
 * Eric Davis        <edavis@broadcom.com>
 * David Christensen <davidch@broadcom.com>
 * Gary Zambrano     <zambrano@broadcom.com>
 *
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * Copyright (c) 2015-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __BNX2X_H__
#define __BNX2X_H__

#include <rte_byteorder.h>
#include <rte_spinlock.h>
#include <rte_bus_pci.h>
#include <rte_io.h>

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN RTE_LITTLE_ENDIAN
#endif
#undef __BIG_ENDIAN
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN    RTE_BIG_ENDIAN
#endif
#undef __LITTLE_ENDIAN
#endif

#include "bnx2x_ethdev.h"
#include "ecore_mfw_req.h"
#include "ecore_fw_defs.h"
#include "ecore_hsi.h"
#include "ecore_reg.h"
#include "bnx2x_stats.h"
#include "bnx2x_vfpf.h"

#include "elink.h"

#ifndef __FreeBSD__
#include <linux/pci_regs.h>

#define PCIY_PMG                       PCI_CAP_ID_PM
#define PCIY_MSI                       PCI_CAP_ID_MSI
#define PCIY_EXPRESS                   PCI_CAP_ID_EXP
#define PCIY_MSIX                      PCI_CAP_ID_MSIX
#define PCIR_EXPRESS_DEVICE_STA        PCI_EXP_TYPE_RC_EC
#define PCIM_EXP_STA_TRANSACTION_PND   PCI_EXP_DEVSTA_TRPND
#define PCIR_EXPRESS_LINK_STA          PCI_EXP_LNKSTA
#define PCIM_LINK_STA_WIDTH            PCI_EXP_LNKSTA_NLW
#define PCIM_LINK_STA_SPEED            PCI_EXP_LNKSTA_CLS
#define PCIR_EXPRESS_DEVICE_CTL        PCI_EXP_DEVCTL
#define PCIM_EXP_CTL_MAX_PAYLOAD       PCI_EXP_DEVCTL_PAYLOAD
#define PCIM_EXP_CTL_MAX_READ_REQUEST  PCI_EXP_DEVCTL_READRQ
#define PCIR_POWER_STATUS              PCI_PM_CTRL
#define PCIM_PSTAT_DMASK               PCI_PM_CTRL_STATE_MASK
#define PCIM_PSTAT_PME                 PCI_PM_CTRL_PME_STATUS
#define PCIM_PSTAT_D3                  0x3
#define PCIM_PSTAT_PMEENABLE           PCI_PM_CTRL_PME_ENABLE
#define PCIR_MSIX_CTRL                 PCI_MSIX_FLAGS
#define PCIM_MSIXCTRL_TABLE_SIZE       PCI_MSIX_FLAGS_QSIZE
#else
#include <dev/pci/pcireg.h>
#endif

#define IFM_10G_CX4                    20 /* 10GBase CX4 copper */
#define IFM_10G_TWINAX                 22 /* 10GBase Twinax copper */
#define IFM_10G_T                      26 /* 10GBase-T - RJ45 */

#ifndef __FreeBSD__
#define PCIR_EXPRESS_DEVICE_STA        PCI_EXP_TYPE_RC_EC
#define PCIM_EXP_STA_TRANSACTION_PND   PCI_EXP_DEVSTA_TRPND
#define PCIR_EXPRESS_LINK_STA          PCI_EXP_LNKSTA
#define PCIM_LINK_STA_WIDTH            PCI_EXP_LNKSTA_NLW
#define PCIM_LINK_STA_SPEED            PCI_EXP_LNKSTA_CLS
#define PCIR_EXPRESS_DEVICE_CTL        PCI_EXP_DEVCTL
#define PCIM_EXP_CTL_MAX_PAYLOAD       PCI_EXP_DEVCTL_PAYLOAD
#define PCIM_EXP_CTL_MAX_READ_REQUEST  PCI_EXP_DEVCTL_READRQ
#else
#define PCIR_EXPRESS_DEVICE_STA	PCIER_DEVICE_STA
#define PCIM_EXP_STA_TRANSACTION_PND   PCIEM_STA_TRANSACTION_PND
#define PCIR_EXPRESS_LINK_STA          PCIER_LINK_STA
#define PCIM_LINK_STA_WIDTH            PCIEM_LINK_STA_WIDTH
#define PCIM_LINK_STA_SPEED            PCIEM_LINK_STA_SPEED
#define PCIR_EXPRESS_DEVICE_CTL        PCIER_DEVICE_CTL
#define PCIM_EXP_CTL_MAX_PAYLOAD       PCIEM_CTL_MAX_PAYLOAD
#define PCIM_EXP_CTL_MAX_READ_REQUEST  PCIEM_CTL_MAX_READ_REQUEST
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#ifndef ARRSIZE
#define ARRSIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif
#ifndef roundup
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif
#ifndef ilog2
static inline
int bnx2x_ilog2(int x)
{
	int log = 0;
	x >>= 1;

	while(x) {
		log++;
		x >>= 1;
	}
	return log;
}
#define ilog2(x) bnx2x_ilog2(x)
#endif

#define BNX2X_BC_VER		0x040200

#include "ecore_sp.h"

struct bnx2x_device_type {
	uint16_t bnx2x_vid;
	uint16_t bnx2x_did;
	uint16_t bnx2x_svid;
	uint16_t bnx2x_sdid;
	char     *bnx2x_name;
};

#define BNX2X_PAGE_SHIFT       12
#define BNX2X_PAGE_SIZE        (1 << BNX2X_PAGE_SHIFT)
#define BNX2X_PAGE_MASK        (~(BNX2X_PAGE_SIZE - 1))
#define BNX2X_PAGE_ALIGN(addr) ((addr + BNX2X_PAGE_SIZE - 1) & BNX2X_PAGE_MASK)

#if BNX2X_PAGE_SIZE != 4096
#error Page sizes other than 4KB are unsupported!
#endif

#define U64_LO(addr) ((uint32_t)(((uint64_t)(addr)) & 0xFFFFFFFF))
#define U64_HI(addr) ((uint32_t)(((uint64_t)(addr)) >> 32))
#define HILO_U64(hi, lo) ((((uint64_t)(hi)) << 32) + (lo))

/* dropless fc FW/HW related params */
#define BRB_SIZE(sc)         (CHIP_IS_E3(sc) ? 1024 : 512)
#define MAX_AGG_QS(sc)       ETH_MAX_AGGREGATION_QUEUES_E1H_E2
#define FW_DROP_LEVEL(sc)    (3 + MAX_SPQ_PENDING + MAX_AGG_QS(sc))
#define FW_PREFETCH_CNT      16U
#define DROPLESS_FC_HEADROOM 100

/*
 * Transmit Buffer Descriptor (tx_bd) definitions*
 */
/* NUM_TX_PAGES must be a power of 2. */
#define NUM_TX_PAGES		 16
#define TOTAL_TX_BD_PER_PAGE     (BNX2X_PAGE_SIZE / sizeof(union eth_tx_bd_types)) /*  256 */
#define USABLE_TX_BD_PER_PAGE    (TOTAL_TX_BD_PER_PAGE - 1)                      /*  255 */

#define TOTAL_TX_BD(q)           (TOTAL_TX_BD_PER_PAGE * q->nb_tx_pages)         /*  512 */
#define USABLE_TX_BD(q)          (USABLE_TX_BD_PER_PAGE * q->nb_tx_pages)        /*  510 */
#define MAX_TX_BD(q)             (TOTAL_TX_BD(q) - 1)                            /*  511 */
#define MAX_TX_AVAIL		 (USABLE_TX_BD_PER_PAGE * NUM_TX_PAGES - 2)
#define NEXT_TX_BD(x)                                                   \
	((((x) & USABLE_TX_BD_PER_PAGE) ==                              \
	  (USABLE_TX_BD_PER_PAGE - 1)) ? (x) + 2 : (x) + 1)

#define TX_BD(x, q)             ((x) & MAX_TX_BD(q))
#define TX_PAGE(x)              (((x) & ~USABLE_TX_BD_PER_PAGE) >> 8)
#define TX_IDX(x)               ((x) & USABLE_TX_BD_PER_PAGE)

#define BDS_PER_TX_PKT		(3)

/*
 * Trigger pending transmits when the number of available BDs is greater
 * than 1/8 of the total number of usable BDs.
 */
#define BNX2X_TX_CLEANUP_THRESHOLD(q) (USABLE_TX_BD(q) / 8)
#define BNX2X_TX_TIMEOUT 5

/*
 * Receive Buffer Descriptor (rx_bd) definitions*
 */
#define MAX_RX_PAGES            8
#define TOTAL_RX_BD_PER_PAGE    (BNX2X_PAGE_SIZE / sizeof(struct eth_rx_bd))      /*  512 */
#define USABLE_RX_BD_PER_PAGE   (TOTAL_RX_BD_PER_PAGE - 2)                      /*  510 */
#define RX_BD_PER_PAGE_MASK     (TOTAL_RX_BD_PER_PAGE - 1)                      /*  511 */
#define TOTAL_RX_BD(q)          (TOTAL_RX_BD_PER_PAGE * q->nb_rx_pages)         /*  512 */
#define USABLE_RX_BD(q)         (USABLE_RX_BD_PER_PAGE * q->nb_rx_pages)        /*  510 */
#define MAX_RX_BD(q)            (TOTAL_RX_BD(q) - 1)                            /*  511 */
#define MAX_RX_AVAIL		(USABLE_RX_BD_PER_PAGE * MAX_RX_PAGES - 2)
#define RX_BD_NEXT_PAGE_DESC_CNT 2

#define NEXT_RX_BD(x)                                                   \
	((((x) & RX_BD_PER_PAGE_MASK) ==                                \
	(USABLE_RX_BD_PER_PAGE - 1)) ? (x) + 3 : (x) + 1)

/* x & 0x3ff */
#define RX_BD(x, q)             ((x) & MAX_RX_BD(q))
#define RX_PAGE(x)              (((x) & ~RX_BD_PER_PAGE_MASK) >> 9)
#define RX_IDX(x)               ((x) & RX_BD_PER_PAGE_MASK)

/*
 * Receive Completion Queue definitions*
 */
//#define NUM_RCQ_PAGES           (NUM_RX_PAGES * 4)
#define TOTAL_RCQ_ENTRIES_PER_PAGE (BNX2X_PAGE_SIZE / sizeof(union eth_rx_cqe))   /*  128 */
#define USABLE_RCQ_ENTRIES_PER_PAGE (TOTAL_RCQ_ENTRIES_PER_PAGE - 1)            /*  127 */
#define TOTAL_RCQ_ENTRIES(q)    (TOTAL_RCQ_ENTRIES_PER_PAGE * q->nb_cq_pages)   /*  512 */
#define USABLE_RCQ_ENTRIES(q)   (USABLE_RCQ_ENTRIES_PER_PAGE * q->nb_cq_pages)  /*  508 */
#define MAX_RCQ_ENTRIES(q)      (TOTAL_RCQ_ENTRIES(q) - 1)                      /*  511 */
#define RCQ_NEXT_PAGE_DESC_CNT 1

#define NEXT_RCQ_IDX(x)                                                 \
	((((x) & USABLE_RCQ_ENTRIES_PER_PAGE) ==                        \
	(USABLE_RCQ_ENTRIES_PER_PAGE - 1)) ? (x) + 2 : (x) + 1)

#define CQE_BD_REL                                                      \
	(sizeof(union eth_rx_cqe) / sizeof(struct eth_rx_bd))

#define RCQ_BD_PAGES(q)                                                 \
	(q->nb_rx_pages * CQE_BD_REL)

#define RCQ_ENTRY(x, q)         ((x) & MAX_RCQ_ENTRIES(q))
#define RCQ_PAGE(x)             (((x) & ~USABLE_RCQ_ENTRIES_PER_PAGE) >> 7)
#define RCQ_IDX(x)              ((x) & USABLE_RCQ_ENTRIES_PER_PAGE)

/*
 * dropless fc calculations for BDs
 * Number of BDs should be as number of buffers in BRB:
 * Low threshold takes into account RX_BD_NEXT_PAGE_DESC_CNT
 * "next" elements on each page
 */
#define NUM_BD_REQ(sc) \
	BRB_SIZE(sc)
#define NUM_BD_PG_REQ(sc)                                                  \
	((NUM_BD_REQ(sc) + USABLE_RX_BD_PER_PAGE - 1) / USABLE_RX_BD_PER_PAGE)
#define BD_TH_LO(sc)                                \
	(NUM_BD_REQ(sc) +			    \
	 NUM_BD_PG_REQ(sc) * RX_BD_NEXT_PAGE_DESC_CNT + \
	 FW_DROP_LEVEL(sc))
#define BD_TH_HI(sc)                      \
	(BD_TH_LO(sc) + DROPLESS_FC_HEADROOM)
#define MIN_RX_AVAIL(sc)				\
	((sc)->dropless_fc ? BD_TH_HI(sc) + 128 : 128)

#define MIN_RX_SIZE_NONTPA_HW	ETH_MIN_RX_CQES_WITHOUT_TPA
#define MIN_RX_SIZE_NONTPA	(RTE_MAX((uint32_t)MIN_RX_SIZE_NONTPA_HW,\
					(uint32_t)MIN_RX_AVAIL(sc)))

/*
 * dropless fc calculations for RCQs
 * Number of RCQs should be as number of buffers in BRB:
 * Low threshold takes into account RCQ_NEXT_PAGE_DESC_CNT
 * "next" elements on each page
 */
#define NUM_RCQ_REQ(sc) \
    BRB_SIZE(sc)
#define NUM_RCQ_PG_REQ(sc)                                              \
    ((NUM_RCQ_REQ(sc) + USABLE_RCQ_ENTRIES_PER_PAGE - 1) / USABLE_RCQ_ENTRIES_PER_PAGE)
#define RCQ_TH_LO(sc)                              \
    (NUM_RCQ_REQ(sc) +                             \
     NUM_RCQ_PG_REQ(sc) * RCQ_NEXT_PAGE_DESC_CNT + \
     FW_DROP_LEVEL(sc))
#define RCQ_TH_HI(sc)                      \
    (RCQ_TH_LO(sc) + DROPLESS_FC_HEADROOM)

/* Load / Unload modes */
#define LOAD_NORMAL       0
#define LOAD_OPEN         1
#define LOAD_DIAG         2
#define LOAD_LOOPBACK_EXT 3
#define UNLOAD_NORMAL     0
#define UNLOAD_CLOSE      1
#define UNLOAD_RECOVERY   2

/* Some constants... */
//#define MAX_PATH_NUM       2
//#define E2_MAX_NUM_OF_VFS  64
//#define E1H_FUNC_MAX       8
//#define E2_FUNC_MAX        4   /* per path */
#define MAX_VNIC_NUM       4
#define MAX_FUNC_NUM       8   /* common to all chips */
//#define MAX_NDSB           HC_SB_MAX_SB_E2 /* max non-default status block */
#define MAX_RSS_CHAINS     16 /* a constant for HW limit */
#define MAX_MSI_VECTOR     8  /* a constant for HW limit */

#define ILT_NUM_PAGE_ENTRIES 3072
/*
 * 57711 we use whole table since we have 8 functions.
 * 57712 we have only 4 functions, but use same size per func, so only half
 * of the table is used.
 */
#define ILT_PER_FUNC        (ILT_NUM_PAGE_ENTRIES / 8)
#define FUNC_ILT_BASE(func) (func * ILT_PER_FUNC)
/*
 * the phys address is shifted right 12 bits and has an added
 * 1=valid bit added to the 53rd bit
 * then since this is a wide register(TM)
 * we split it into two 32 bit writes
 */
#define ONCHIP_ADDR1(x) ((uint32_t)(((uint64_t)x >> 12) & 0xFFFFFFFF))
#define ONCHIP_ADDR2(x) ((uint32_t)((1 << 20) | ((uint64_t)x >> 44)))

/* L2 header size + 2*VLANs (8 bytes) + LLC SNAP (8 bytes) */
#define ETH_HLEN                  14
#define ETH_OVERHEAD              (ETH_HLEN + 8 + 8)
#define ETH_MIN_PACKET_SIZE       60
#define ETH_MAX_PACKET_SIZE       ETHERMTU /* 1500 */
#define ETH_MAX_JUMBO_PACKET_SIZE 9600
/* TCP with Timestamp Option (32) + IPv6 (40) */

/* max supported alignment is 256 (8 shift) */
#define BNX2X_RX_ALIGN_SHIFT	RTE_MAX(6, min(8, RTE_CACHE_LINE_SIZE_LOG2))

#define BNX2X_PXP_DRAM_ALIGN (BNX2X_RX_ALIGN_SHIFT - 5)

struct bnx2x_bar {
	void *base_addr;
};

/* Used to manage DMA allocations. */
struct bnx2x_dma {
	struct bnx2x_softc        *sc;
	rte_iova_t              paddr;
	void                    *vaddr;
	int                     nseg;
	const void		*mzone;
	char                    msg[RTE_MEMZONE_NAMESIZE - 6];
};

/* attn group wiring */
#define MAX_DYNAMIC_ATTN_GRPS 8

struct attn_route {
	uint32_t sig[5];
};

struct iro {
	uint32_t base;
	uint16_t m1;
	uint16_t m2;
	uint16_t m3;
	uint16_t size;
};

union bnx2x_host_hc_status_block {
	/* pointer to fp status block e2 */
	struct host_hc_status_block_e2  *e2_sb;
	/* pointer to fp status block e1x */
	struct host_hc_status_block_e1x *e1x_sb;
};

union bnx2x_db_prod {
	struct doorbell_set_prod data;
	uint32_t                 raw;
};

struct bnx2x_sw_tx_bd {
	struct mbuf  *m;
	uint16_t     first_bd;
	uint8_t      flags;
/* set on the first BD descriptor when there is a split BD */
#define BNX2X_TSO_SPLIT_BD (1 << 0)
};

/*
 * This is the HSI fastpath data structure. There can be up to MAX_RSS_CHAIN
 * instances of the fastpath structure when using multiple queues.
 */
struct bnx2x_fastpath {
	/* pointer back to parent structure */
	struct bnx2x_softc *sc;

	/* status block */
	struct bnx2x_dma                 sb_dma;
	union bnx2x_host_hc_status_block status_block;

	rte_iova_t tx_desc_mapping;

	rte_iova_t rx_desc_mapping;
	rte_iova_t rx_comp_mapping;

	uint16_t *sb_index_values;
	uint16_t *sb_running_index;
	uint32_t ustorm_rx_prods_offset;

	uint8_t igu_sb_id; /* status block number in HW */
	uint8_t fw_sb_id;  /* status block number in FW */

	uint32_t rx_buf_size;

	int state;
#define BNX2X_FP_STATE_CLOSED  0x01
#define BNX2X_FP_STATE_IRQ     0x02
#define BNX2X_FP_STATE_OPENING 0x04
#define BNX2X_FP_STATE_OPEN    0x08
#define BNX2X_FP_STATE_HALTING 0x10
#define BNX2X_FP_STATE_HALTED  0x20

	/* reference back to this fastpath queue number */
	uint8_t index; /* this is also the 'cid' */
#define FP_IDX(fp) (fp->index)

	/* ethernet client ID (each fastpath set of RX/TX/CQE is a client) */
	uint8_t cl_id;
#define FP_CL_ID(fp) (fp->cl_id)
	uint8_t cl_qzone_id;

	uint16_t fp_hc_idx;

	union bnx2x_db_prod tx_db;

	struct tstorm_per_queue_stats old_tclient;
	struct ustorm_per_queue_stats old_uclient;
	struct xstorm_per_queue_stats old_xclient;
	struct bnx2x_eth_q_stats        eth_q_stats;
	struct bnx2x_eth_q_stats_old    eth_q_stats_old;

	/* Pointer to the receive consumer in the status block */
	uint16_t *rx_cq_cons_sb;

	/* Pointer to the transmit consumer in the status block */
	uint16_t *tx_cons_sb;

	/* transmit timeout until chip reset */
	int watchdog_timer;

}; /* struct bnx2x_fastpath */

#define BNX2X_MAX_NUM_OF_VFS 64
#define BNX2X_VF_ID_INVALID  0xFF

/* maximum number of fast-path interrupt contexts */
#define FP_SB_MAX_E1x 16
#define FP_SB_MAX_E2  HC_SB_MAX_SB_E2

union cdu_context {
    struct eth_context eth;
    char pad[1024];
};

/* CDU host DB constants */
#define CDU_ILT_PAGE_SZ_HW 2
#define CDU_ILT_PAGE_SZ    (8192 << CDU_ILT_PAGE_SZ_HW) /* 32K */
#define ILT_PAGE_CIDS      (CDU_ILT_PAGE_SZ / sizeof(union cdu_context))

#define CNIC_ISCSI_CID_MAX 256
#define CNIC_FCOE_CID_MAX  2048
#define CNIC_CID_MAX       (CNIC_ISCSI_CID_MAX + CNIC_FCOE_CID_MAX)
#define CNIC_ILT_LINES     DIV_ROUND_UP(CNIC_CID_MAX, ILT_PAGE_CIDS)

#define QM_ILT_PAGE_SZ_HW  0
#define QM_ILT_PAGE_SZ     (4096 << QM_ILT_PAGE_SZ_HW) /* 4K */
#define QM_CID_ROUND       1024

/* TM (timers) host DB constants */
#define TM_ILT_PAGE_SZ_HW  0
#define TM_ILT_PAGE_SZ     (4096 << TM_ILT_PAGE_SZ_HW) /* 4K */
/*#define TM_CONN_NUM        (CNIC_STARTING_CID+CNIC_ISCSI_CXT_MAX) */
#define TM_CONN_NUM        1024
#define TM_ILT_SZ          (8 * TM_CONN_NUM)
#define TM_ILT_LINES       DIV_ROUND_UP(TM_ILT_SZ, TM_ILT_PAGE_SZ)

/* SRC (Searcher) host DB constants */
#define SRC_ILT_PAGE_SZ_HW 0
#define SRC_ILT_PAGE_SZ    (4096 << SRC_ILT_PAGE_SZ_HW) /* 4K */
#define SRC_HASH_BITS      10
#define SRC_CONN_NUM       (1 << SRC_HASH_BITS) /* 1024 */
#define SRC_ILT_SZ         (sizeof(struct src_ent) * SRC_CONN_NUM)
#define SRC_T2_SZ          SRC_ILT_SZ
#define SRC_ILT_LINES      DIV_ROUND_UP(SRC_ILT_SZ, SRC_ILT_PAGE_SZ)

struct hw_context {
    struct bnx2x_dma    vcxt_dma;
    union cdu_context *vcxt;
    //rte_iova_t        cxt_mapping;
    size_t            size;
};

#define SM_RX_ID 0
#define SM_TX_ID 1

/* defines for multiple tx priority indices */
#define FIRST_TX_ONLY_COS_INDEX 1
#define FIRST_TX_COS_INDEX      0

#define CID_TO_FP(cid, sc) ((cid) % BNX2X_NUM_NON_CNIC_QUEUES(sc))

#define HC_INDEX_ETH_RX_CQ_CONS       1
#define HC_INDEX_OOO_TX_CQ_CONS       4
#define HC_INDEX_ETH_TX_CQ_CONS_COS0  5
#define HC_INDEX_ETH_TX_CQ_CONS_COS1  6
#define HC_INDEX_ETH_TX_CQ_CONS_COS2  7
#define HC_INDEX_ETH_FIRST_TX_CQ_CONS HC_INDEX_ETH_TX_CQ_CONS_COS0

/* congestion management fairness mode */
#define CMNG_FNS_NONE   0
#define CMNG_FNS_MINMAX 1

/* CMNG constants, as derived from system spec calculations */
/* default MIN rate in case VNIC min rate is configured to zero - 100Mbps */
#define DEF_MIN_RATE 100
/* resolution of the rate shaping timer - 400 usec */
#define RS_PERIODIC_TIMEOUT_USEC 400
/* number of bytes in single QM arbitration cycle -
 * coefficient for calculating the fairness timer */
#define QM_ARB_BYTES 160000
/* resolution of Min algorithm 1:100 */
#define MIN_RES 100
/* how many bytes above threshold for the minimal credit of Min algorithm*/
#define MIN_ABOVE_THRESH 32768
/* fairness algorithm integration time coefficient -
 * for calculating the actual Tfair */
#define T_FAIR_COEF ((MIN_ABOVE_THRESH + QM_ARB_BYTES) * 8 * MIN_RES)
/* memory of fairness algorithm - 2 cycles */
#define FAIR_MEM 2

#define HC_SEG_ACCESS_DEF   0 /* Driver decision 0-3 */
#define HC_SEG_ACCESS_ATTN  4
#define HC_SEG_ACCESS_NORM  0 /* Driver decision 0-1 */

/*
 * The total number of L2 queues, MSIX vectors and HW contexts (CIDs) is
 * control by the number of fast-path status blocks supported by the
 * device (HW/FW). Each fast-path status block (FP-SB) aka non-default
 * status block represents an independent interrupts context that can
 * serve a regular L2 networking queue. However special L2 queues such
 * as the FCoE queue do not require a FP-SB and other components like
 * the CNIC may consume FP-SB reducing the number of possible L2 queues
 *
 * If the maximum number of FP-SB available is X then:
 * a. If CNIC is supported it consumes 1 FP-SB thus the max number of
 *    regular L2 queues is Y=X-1
 * b. in MF mode the actual number of L2 queues is Y= (X-1/MF_factor)
 * c. If the FCoE L2 queue is supported the actual number of L2 queues
 *    is Y+1
 * d. The number of irqs (MSIX vectors) is either Y+1 (one extra for
 *    slow-path interrupts) or Y+2 if CNIC is supported (one additional
 *    FP interrupt context for the CNIC).
 * e. The number of HW context (CID count) is always X or X+1 if FCoE
 *    L2 queue is supported. the cid for the FCoE L2 queue is always X.
 *
 * So this is quite simple for now as no ULPs are supported yet. :-)
 */
#define BNX2X_NUM_QUEUES(sc)          ((sc)->num_queues)
#define BNX2X_NUM_ETH_QUEUES(sc)      BNX2X_NUM_QUEUES(sc)
#define BNX2X_NUM_NON_CNIC_QUEUES(sc) BNX2X_NUM_QUEUES(sc)
#define BNX2X_NUM_RX_QUEUES(sc)       BNX2X_NUM_QUEUES(sc)

#define FOR_EACH_QUEUE(sc, var)                          \
    for ((var) = 0; (var) < BNX2X_NUM_QUEUES(sc); (var)++)

#define FOR_EACH_NONDEFAULT_QUEUE(sc, var)               \
    for ((var) = 1; (var) < BNX2X_NUM_QUEUES(sc); (var)++)

#define FOR_EACH_ETH_QUEUE(sc, var)                          \
    for ((var) = 0; (var) < BNX2X_NUM_ETH_QUEUES(sc); (var)++)

#define FOR_EACH_NONDEFAULT_ETH_QUEUE(sc, var)               \
    for ((var) = 1; (var) < BNX2X_NUM_ETH_QUEUES(sc); (var)++)

#define FOR_EACH_COS_IN_TX_QUEUE(sc, var)           \
    for ((var) = 0; (var) < (sc)->max_cos; (var)++)

#define FOR_EACH_CNIC_QUEUE(sc, var)     \
    for ((var) = BNX2X_NUM_ETH_QUEUES(sc); \
	 (var) < BNX2X_NUM_QUEUES(sc);     \
	 (var)++)

enum {
    OOO_IDX_OFFSET,
    FCOE_IDX_OFFSET,
    FWD_IDX_OFFSET,
};

#define FCOE_IDX(sc)              (BNX2X_NUM_NON_CNIC_QUEUES(sc) + FCOE_IDX_OFFSET)
#define bnx2x_fcoe_fp(sc)           (&sc->fp[FCOE_IDX(sc)])
#define bnx2x_fcoe(sc, var)         (bnx2x_fcoe_fp(sc)->var)
#define bnx2x_fcoe_inner_sp_obj(sc) (&sc->sp_objs[FCOE_IDX(sc)])
#define bnx2x_fcoe_sp_obj(sc, var)  (bnx2x_fcoe_inner_sp_obj(sc)->var)
#define bnx2x_fcoe_tx(sc, var)      (bnx2x_fcoe_fp(sc)->txdata_ptr[FIRST_TX_COS_INDEX]->var)

#define OOO_IDX(sc)               (BNX2X_NUM_NON_CNIC_QUEUES(sc) + OOO_IDX_OFFSET)
#define bnx2x_ooo_fp(sc)            (&sc->fp[OOO_IDX(sc)])
#define bnx2x_ooo(sc, var)          (bnx2x_ooo_fp(sc)->var)
#define bnx2x_ooo_inner_sp_obj(sc)  (&sc->sp_objs[OOO_IDX(sc)])
#define bnx2x_ooo_sp_obj(sc, var)   (bnx2x_ooo_inner_sp_obj(sc)->var)

#define FWD_IDX(sc)               (BNX2X_NUM_NON_CNIC_QUEUES(sc) + FWD_IDX_OFFSET)
#define bnx2x_fwd_fp(sc)            (&sc->fp[FWD_IDX(sc)])
#define bnx2x_fwd(sc, var)          (bnx2x_fwd_fp(sc)->var)
#define bnx2x_fwd_inner_sp_obj(sc)  (&sc->sp_objs[FWD_IDX(sc)])
#define bnx2x_fwd_sp_obj(sc, var)   (bnx2x_fwd_inner_sp_obj(sc)->var)
#define bnx2x_fwd_txdata(fp)        (fp->txdata_ptr[FIRST_TX_COS_INDEX])

#define IS_ETH_FP(fp)    ((fp)->index < BNX2X_NUM_ETH_QUEUES((fp)->sc))
#define IS_FCOE_FP(fp)   ((fp)->index == FCOE_IDX((fp)->sc))
#define IS_FCOE_IDX(idx) ((idx) == FCOE_IDX(sc))
#define IS_FWD_FP(fp)    ((fp)->index == FWD_IDX((fp)->sc))
#define IS_FWD_IDX(idx)  ((idx) == FWD_IDX(sc))
#define IS_OOO_FP(fp)    ((fp)->index == OOO_IDX((fp)->sc))
#define IS_OOO_IDX(idx)  ((idx) == OOO_IDX(sc))

enum {
    BNX2X_PORT_QUERY_IDX,
    BNX2X_PF_QUERY_IDX,
    BNX2X_FCOE_QUERY_IDX,
    BNX2X_FIRST_QUEUE_QUERY_IDX,
};

struct bnx2x_fw_stats_req {
    struct stats_query_header hdr;
    struct stats_query_entry  query[FP_SB_MAX_E1x +
				    BNX2X_FIRST_QUEUE_QUERY_IDX];
};

struct bnx2x_fw_stats_data {
    struct stats_counter          storm_counters;
    struct per_port_stats         port;
    struct per_pf_stats           pf;
    struct per_queue_stats        queue_stats[1];
};

/* IGU MSIX STATISTICS on 57712: 64 for VFs; 4 for PFs; 4 for Attentions */
#define BNX2X_IGU_STAS_MSG_VF_CNT 64
#define BNX2X_IGU_STAS_MSG_PF_CNT 4

#define MAX_DMAE_C 8

/*
 * This is the slowpath data structure. It is mapped into non-paged memory
 * so that the hardware can access it's contents directly and must be page
 * aligned.
 */
struct bnx2x_slowpath {

    /* used by the DMAE command executer */
    struct dmae_command dmae[MAX_DMAE_C];

    /* statistics completion */
    uint32_t stats_comp;

    /* firmware defined statistics blocks */
    union mac_stats        mac_stats;
    struct nig_stats       nig_stats;
    struct host_port_stats port_stats;
    struct host_func_stats func_stats;

    /* DMAE completion value and data source/sink */
    uint32_t wb_comp;
    uint32_t wb_data[4];

    union {
	struct mac_configuration_cmd          e1x;
	struct eth_classify_rules_ramrod_data e2;
    } mac_rdata;

    union {
	struct tstorm_eth_mac_filter_config e1x;
	struct eth_filter_rules_ramrod_data e2;
    } rx_mode_rdata;

    struct eth_rss_update_ramrod_data rss_rdata;

    union {
	struct mac_configuration_cmd           e1;
	struct eth_multicast_rules_ramrod_data e2;
    } mcast_rdata;

    union {
	struct function_start_data        func_start;
	struct flow_control_configuration pfc_config; /* for DCBX ramrod */
    } func_rdata;

    /* Queue State related ramrods */
    union {
	struct client_init_ramrod_data   init_data;
	struct client_update_ramrod_data update_data;
    } q_rdata;

    /*
     * AFEX ramrod can not be a part of func_rdata union because these
     * events might arrive in parallel to other events from func_rdata.
     * If they were defined in the same union the data can get corrupted.
     */
    struct afex_vif_list_ramrod_data func_afex_rdata;

    union drv_info_to_mcp drv_info_to_mcp;
}; /* struct bnx2x_slowpath */

/*
 * Port specifc data structure.
 */
struct bnx2x_port {
    /*
     * Port Management Function (for 57711E only).
     * When this field is set the driver instance is
     * responsible for managing port specifc
     * configurations such as handling link attentions.
     */
    uint32_t pmf;

    /* Ethernet maximum transmission unit. */
    uint16_t ether_mtu;

    uint32_t link_config[ELINK_LINK_CONFIG_SIZE];

    uint32_t ext_phy_config;

    /* Port feature config.*/
    uint32_t config;

    /* Defines the features supported by the PHY. */
    uint32_t supported[ELINK_LINK_CONFIG_SIZE];

    /* Defines the features advertised by the PHY. */
    uint32_t advertising[ELINK_LINK_CONFIG_SIZE];
#define ADVERTISED_10baseT_Half    (1 << 1)
#define ADVERTISED_10baseT_Full    (1 << 2)
#define ADVERTISED_100baseT_Half   (1 << 3)
#define ADVERTISED_100baseT_Full   (1 << 4)
#define ADVERTISED_1000baseT_Half  (1 << 5)
#define ADVERTISED_1000baseT_Full  (1 << 6)
#define ADVERTISED_TP              (1 << 7)
#define ADVERTISED_FIBRE           (1 << 8)
#define ADVERTISED_Autoneg         (1 << 9)
#define ADVERTISED_Asym_Pause      (1 << 10)
#define ADVERTISED_Pause           (1 << 11)
#define ADVERTISED_2500baseX_Full  (1 << 15)
#define ADVERTISED_10000baseT_Full (1 << 16)

    uint32_t    phy_addr;

	/* Used to synchronize phy accesses. */
	rte_spinlock_t	phy_mtx;
	char		phy_mtx_name[32];

#define BNX2X_PHY_LOCK(sc)          rte_spinlock_lock(&sc->port.phy_mtx)
#define BNX2X_PHY_UNLOCK(sc)        rte_spinlock_unlock(&sc->port.phy_mtx)

    /*
     * MCP scratchpad address for port specific statistics.
     * The device is responsible for writing statistcss
     * back to the MCP for use with management firmware such
     * as UMP/NC-SI.
     */
    uint32_t port_stx;

    struct nig_stats old_nig_stats;
}; /* struct bnx2x_port */

struct bnx2x_mf_info {
	uint32_t mf_config[E1HVN_MAX];

	uint32_t vnics_per_port;   /* 1, 2 or 4 */
	uint32_t multi_vnics_mode; /* can be set even if vnics_per_port = 1 */
	uint32_t path_has_ovlan;   /* MF mode in the path (can be different than the MF mode of the function */

#define IS_MULTI_VNIC(sc)  ((sc)->devinfo.mf_info.multi_vnics_mode)
#define VNICS_PER_PORT(sc) ((sc)->devinfo.mf_info.vnics_per_port)
#define VNICS_PER_PATH(sc)                                  \
	((sc)->devinfo.mf_info.vnics_per_port *                 \
	 ((CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) ? 2 : 1 ))

	uint8_t min_bw[MAX_VNIC_NUM];
	uint8_t max_bw[MAX_VNIC_NUM];

	uint16_t ext_id; /* vnic outer vlan or VIF ID */
#define VALID_OVLAN(ovlan) ((ovlan) <= 4096)
#define INVALID_VIF_ID 0xFFFF
#define OVLAN(sc) ((sc)->devinfo.mf_info.ext_id)
#define VIF_ID(sc) ((sc)->devinfo.mf_info.ext_id)

	uint16_t default_vlan;
#define NIV_DEFAULT_VLAN(sc) ((sc)->devinfo.mf_info.default_vlan)

	uint8_t niv_allowed_priorities;
#define NIV_ALLOWED_PRIORITIES(sc) ((sc)->devinfo.mf_info.niv_allowed_priorities)

	uint8_t niv_default_cos;
#define NIV_DEFAULT_COS(sc) ((sc)->devinfo.mf_info.niv_default_cos)

	uint8_t niv_mba_enabled;

	enum mf_cfg_afex_vlan_mode afex_vlan_mode;
#define AFEX_VLAN_MODE(sc) ((sc)->devinfo.mf_info.afex_vlan_mode)
	int                        afex_def_vlan_tag;
	uint32_t                   pending_max;

	uint16_t flags;
#define MF_INFO_VALID_MAC       0x0001

	uint16_t mf_ov;
	uint8_t mf_mode; /* Switch-Dependent or Switch-Independent */
#define IS_MF(sc)                        \
	(IS_MULTI_VNIC(sc) &&                \
	 ((sc)->devinfo.mf_info.mf_mode != 0))
#define IS_MF_SD(sc)                                     \
	(IS_MULTI_VNIC(sc) &&                                \
	 ((sc)->devinfo.mf_info.mf_mode == MULTI_FUNCTION_SD))
#define IS_MF_SI(sc)                                     \
	(IS_MULTI_VNIC(sc) &&                                \
	 ((sc)->devinfo.mf_info.mf_mode == MULTI_FUNCTION_SI))
#define IS_MF_AFEX(sc)                              \
	(IS_MULTI_VNIC(sc) &&                           \
	 ((sc)->devinfo.mf_info.mf_mode == MULTI_FUNCTION_AFEX))
#define IS_MF_SD_MODE(sc)   IS_MF_SD(sc)
#define IS_MF_SI_MODE(sc)   IS_MF_SI(sc)
#define IS_MF_AFEX_MODE(sc) IS_MF_AFEX(sc)

	uint32_t mf_protos_supported;
	#define MF_PROTO_SUPPORT_ETHERNET 0x1
	#define MF_PROTO_SUPPORT_ISCSI    0x2
	#define MF_PROTO_SUPPORT_FCOE     0x4
}; /* struct bnx2x_mf_info */

/* Device information data structure. */
struct bnx2x_devinfo {
#if 1
#define NAME_SIZE 128
	char name[NAME_SIZE];
#endif
	/* PCIe info */
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t subvendor_id;
	uint16_t subdevice_id;

	/*
	 * chip_id = 0b'CCCCCCCCCCCCCCCCRRRRMMMMMMMMBBBB'
	 *   C = Chip Number   (bits 16-31)
	 *   R = Chip Revision (bits 12-15)
	 *   M = Chip Metal    (bits 4-11)
	 *   B = Chip Bond ID  (bits 0-3)
	 */
	uint32_t chip_id;
#define CHIP_ID(sc)           ((sc)->devinfo.chip_id & 0xffff0000)
#define CHIP_NUM(sc)          ((sc)->devinfo.chip_id >> 16)
/* device ids */
#define CHIP_NUM_57710        0x164e
#define CHIP_NUM_57711        0x164f
#define CHIP_NUM_57711E       0x1650
#define CHIP_NUM_57712        0x1662
#define CHIP_NUM_57712_MF     0x1663
#define CHIP_NUM_57712_VF     0x166f
#define CHIP_NUM_57800        0x168a
#define CHIP_NUM_57800_MF     0x16a5
#define CHIP_NUM_57800_VF     0x16a9
#define CHIP_NUM_57810        0x168e
#define CHIP_NUM_57810_MF     0x16ae
#define CHIP_NUM_57810_VF     0x16af
#define CHIP_NUM_57811        0x163d
#define CHIP_NUM_57811_MF     0x163e
#define CHIP_NUM_57811_VF     0x163f
#define CHIP_NUM_57840_OBS    0x168d
#define CHIP_NUM_57840_OBS_MF 0x16ab
#define CHIP_NUM_57840_4_10   0x16a1
#define CHIP_NUM_57840_2_20   0x16a2
#define CHIP_NUM_57840_MF     0x16a4
#define CHIP_NUM_57840_VF     0x16ad

#define CHIP_REV_SHIFT      12
#define CHIP_REV_MASK       (0xF << CHIP_REV_SHIFT)
#define CHIP_REV(sc)        ((sc)->devinfo.chip_id & CHIP_REV_MASK)

#define CHIP_REV_Ax         (0x0 << CHIP_REV_SHIFT)
#define CHIP_REV_Bx         (0x1 << CHIP_REV_SHIFT)
#define CHIP_REV_Cx         (0x2 << CHIP_REV_SHIFT)

#define CHIP_REV_IS_SLOW(sc)    \
	(CHIP_REV(sc) > 0x00005000)
#define CHIP_REV_IS_FPGA(sc)                              \
	(CHIP_REV_IS_SLOW(sc) && (CHIP_REV(sc) & 0x00001000))
#define CHIP_REV_IS_EMUL(sc)                               \
	(CHIP_REV_IS_SLOW(sc) && !(CHIP_REV(sc) & 0x00001000))
#define CHIP_REV_IS_ASIC(sc) \
	(!CHIP_REV_IS_SLOW(sc))

#define CHIP_METAL(sc)      ((sc->devinfo.chip_id) & 0x00000ff0)
#define CHIP_BOND_ID(sc)    ((sc->devinfo.chip_id) & 0x0000000f)

#define CHIP_IS_E1(sc)      (CHIP_NUM(sc) == CHIP_NUM_57710)
#define CHIP_IS_57710(sc)   (CHIP_NUM(sc) == CHIP_NUM_57710)
#define CHIP_IS_57711(sc)   (CHIP_NUM(sc) == CHIP_NUM_57711)
#define CHIP_IS_57711E(sc)  (CHIP_NUM(sc) == CHIP_NUM_57711E)
#define CHIP_IS_E1H(sc)     ((CHIP_IS_57711(sc)) || \
			     (CHIP_IS_57711E(sc)))
#define CHIP_IS_E1x(sc)     CHIP_IS_E1H(sc)

#define CHIP_IS_57712(sc)    (CHIP_NUM(sc) == CHIP_NUM_57712)
#define CHIP_IS_57712_MF(sc) (CHIP_NUM(sc) == CHIP_NUM_57712_MF)
#define CHIP_IS_57712_VF(sc) (CHIP_NUM(sc) == CHIP_NUM_57712_VF)
#define CHIP_IS_E2(sc)       (CHIP_IS_57712(sc) ||  \
			      CHIP_IS_57712_MF(sc))

#define CHIP_IS_57800(sc)    (CHIP_NUM(sc) == CHIP_NUM_57800)
#define CHIP_IS_57800_MF(sc) (CHIP_NUM(sc) == CHIP_NUM_57800_MF)
#define CHIP_IS_57800_VF(sc) (CHIP_NUM(sc) == CHIP_NUM_57800_VF)
#define CHIP_IS_57810(sc)    (CHIP_NUM(sc) == CHIP_NUM_57810)
#define CHIP_IS_57810_MF(sc) (CHIP_NUM(sc) == CHIP_NUM_57810_MF)
#define CHIP_IS_57810_VF(sc) (CHIP_NUM(sc) == CHIP_NUM_57810_VF)
#define CHIP_IS_57811(sc)    (CHIP_NUM(sc) == CHIP_NUM_57811)
#define CHIP_IS_57811_MF(sc) (CHIP_NUM(sc) == CHIP_NUM_57811_MF)
#define CHIP_IS_57811_VF(sc) (CHIP_NUM(sc) == CHIP_NUM_57811_VF)
#define CHIP_IS_57840(sc)    ((CHIP_NUM(sc) == CHIP_NUM_57840_OBS)  || \
			      (CHIP_NUM(sc) == CHIP_NUM_57840_4_10) || \
			      (CHIP_NUM(sc) == CHIP_NUM_57840_2_20))
#define CHIP_IS_57840_MF(sc) ((CHIP_NUM(sc) == CHIP_NUM_57840_OBS_MF) || \
			      (CHIP_NUM(sc) == CHIP_NUM_57840_MF))
#define CHIP_IS_57840_VF(sc) (CHIP_NUM(sc) == CHIP_NUM_57840_VF)

#define CHIP_IS_E3(sc)      (CHIP_IS_57800(sc)    || \
			     CHIP_IS_57800_MF(sc) || \
			     CHIP_IS_57800_VF(sc) || \
			     CHIP_IS_57810(sc)    || \
			     CHIP_IS_57810_MF(sc) || \
			     CHIP_IS_57810_VF(sc) || \
			     CHIP_IS_57811(sc)    || \
			     CHIP_IS_57811_MF(sc) || \
			     CHIP_IS_57811_VF(sc) || \
			     CHIP_IS_57840(sc)    || \
			     CHIP_IS_57840_MF(sc) || \
			     CHIP_IS_57840_VF(sc))
#define CHIP_IS_E3A0(sc)    (CHIP_IS_E3(sc) &&              \
			     (CHIP_REV(sc) == CHIP_REV_Ax))
#define CHIP_IS_E3B0(sc)    (CHIP_IS_E3(sc) &&              \
			     (CHIP_REV(sc) == CHIP_REV_Bx))

#define USES_WARPCORE(sc)   (CHIP_IS_E3(sc))
#define CHIP_IS_E2E3(sc)    (CHIP_IS_E2(sc) || \
			     CHIP_IS_E3(sc))

#define CHIP_IS_MF_CAP(sc)  (CHIP_IS_57711E(sc)  ||  \
			     CHIP_IS_57712_MF(sc) || \
			     CHIP_IS_E3(sc))

#define IS_VF(sc)           ((sc)->flags & BNX2X_IS_VF_FLAG)
#define IS_PF(sc)           (!IS_VF(sc))

/*
 * This define is used in two main places:
 * 1. In the early stages of nic_load, to know if to configure Parser/Searcher
 * to nic-only mode or to offload mode. Offload mode is configured if either
 * the chip is E1x (where NIC_MODE register is not applicable), or if cnic
 * already registered for this port (which means that the user wants storage
 * services).
 * 2. During cnic-related load, to know if offload mode is already configured
 * in the HW or needs to be configrued. Since the transition from nic-mode to
 * offload-mode in HW causes traffic coruption, nic-mode is configured only
 * in ports on which storage services where never requested.
 */
#define CONFIGURE_NIC_MODE(sc) (!CHIP_IS_E1x(sc) && !CNIC_ENABLED(sc))

	uint8_t  chip_port_mode;
#define CHIP_4_PORT_MODE        0x0
#define CHIP_2_PORT_MODE        0x1
#define CHIP_PORT_MODE_NONE     0x2
#define CHIP_PORT_MODE(sc)      ((sc)->devinfo.chip_port_mode)
#define CHIP_IS_MODE_4_PORT(sc) (CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE)

	uint8_t int_block;
#define INT_BLOCK_HC            0
#define INT_BLOCK_IGU           1
#define INT_BLOCK_MODE_NORMAL   0
#define INT_BLOCK_MODE_BW_COMP  2
#define CHIP_INT_MODE_IS_NBC(sc)                          \
	(!CHIP_IS_E1x(sc) &&                                  \
	 !((sc)->devinfo.int_block & INT_BLOCK_MODE_BW_COMP))
#define CHIP_INT_MODE_IS_BC(sc) (!CHIP_INT_MODE_IS_NBC(sc))

	uint32_t shmem_base;
	uint32_t shmem2_base;
	uint32_t bc_ver;
	char bc_ver_str[32];
	uint32_t mf_cfg_base; /* bootcode shmem address in BAR memory */
	struct bnx2x_mf_info mf_info;

	uint32_t flash_size;
#define NVRAM_1MB_SIZE      0x20000
#define NVRAM_TIMEOUT_COUNT 30000
#define NVRAM_PAGE_SIZE     256

	/* PCIe capability information */
	uint32_t pcie_cap_flags;
#define BNX2X_PM_CAPABLE_FLAG     0x00000001
#define BNX2X_PCIE_CAPABLE_FLAG   0x00000002
#define BNX2X_MSI_CAPABLE_FLAG    0x00000004
#define BNX2X_MSIX_CAPABLE_FLAG   0x00000008
	uint16_t pcie_pm_cap_reg;
	uint16_t pcie_link_width;
	uint16_t pcie_link_speed;
	uint16_t pcie_msi_cap_reg;
	uint16_t pcie_msix_cap_reg;

	/* device configuration read from bootcode shared memory */
	uint32_t hw_config;
	uint32_t hw_config2;
}; /* struct bnx2x_devinfo */

struct bnx2x_sp_objs {
	struct ecore_vlan_mac_obj mac_obj; /* MACs object */
	struct ecore_queue_sp_obj q_obj; /* Queue State object */
}; /* struct bnx2x_sp_objs */

/*
 * Data that will be used to create a link report message. We will keep the
 * data used for the last link report in order to prevent reporting the same
 * link parameters twice.
 */
struct bnx2x_link_report_data {
	uint16_t      line_speed;        /* Effective line speed */
	unsigned long link_report_flags; /* BNX2X_LINK_REPORT_XXX flags */
};

enum {
	BNX2X_LINK_REPORT_FULL_DUPLEX,
	BNX2X_LINK_REPORT_LINK_DOWN,
	BNX2X_LINK_REPORT_RX_FC_ON,
	BNX2X_LINK_REPORT_TX_FC_ON
};

#define BNX2X_RX_CHAIN_PAGE_SZ    BNX2X_PAGE_SIZE

struct bnx2x_pci_cap {
	struct bnx2x_pci_cap *next;
	uint16_t id;
	uint16_t type;
	uint16_t addr;
};

struct bnx2x_vfdb;

/* Top level device private data structure. */
struct bnx2x_softc {

	void            **rx_queues;
	void            **tx_queues;
	uint32_t        max_tx_queues;
	uint32_t        max_rx_queues;
	const struct rte_pci_device *pci_dev;
	uint32_t        pci_val;
	struct bnx2x_pci_cap *pci_caps;
#define BNX2X_INTRS_POLL_PERIOD   1

	void            *firmware;
	uint64_t        fw_len;

	/* MAC address operations */
	struct bnx2x_mac_ops mac_ops;

	/* structures for VF mbox/response/bulletin */
	struct bnx2x_vf_mbx_msg		*vf2pf_mbox;
	struct bnx2x_dma		 vf2pf_mbox_mapping;
	struct vf_acquire_resp_tlv	 acquire_resp;
	struct bnx2x_vf_bulletin	*pf2vf_bulletin;
	struct bnx2x_dma		 pf2vf_bulletin_mapping;
	struct bnx2x_vf_bulletin	 old_bulletin;
	rte_spinlock_t			 vf2pf_lock;

	int             media;

	int             state; /* device state */
#define BNX2X_STATE_CLOSED                 0x0000
#define BNX2X_STATE_OPENING_WAITING_LOAD   0x1000
#define BNX2X_STATE_OPENING_WAITING_PORT   0x2000
#define BNX2X_STATE_OPEN                   0x3000
#define BNX2X_STATE_CLOSING_WAITING_HALT   0x4000
#define BNX2X_STATE_CLOSING_WAITING_DELETE 0x5000
#define BNX2X_STATE_CLOSING_WAITING_UNLOAD 0x6000
#define BNX2X_STATE_DISABLED               0xD000
#define BNX2X_STATE_DIAG                   0xE000
#define BNX2X_STATE_ERROR                  0xF000

	int flags;
#define BNX2X_ONE_PORT_FLAG     0x1
#define BNX2X_NO_FCOE_FLAG      0x2
#define BNX2X_NO_WOL_FLAG       0x4
#define BNX2X_NO_MCP_FLAG       0x8
#define BNX2X_NO_ISCSI_OOO_FLAG 0x10
#define BNX2X_NO_ISCSI_FLAG     0x20
#define BNX2X_MF_FUNC_DIS       0x40
#define BNX2X_TX_SWITCHING      0x80
#define BNX2X_IS_VF_FLAG        0x100

#define BNX2X_ONE_PORT(sc)      (sc->flags & BNX2X_ONE_PORT_FLAG)
#define BNX2X_NOFCOE(sc)        (sc->flags & BNX2X_NO_FCOE_FLAG)
#define BNX2X_NOMCP(sc)         (sc->flags & BNX2X_NO_MCP_FLAG)

#define MAX_BARS 5
	struct bnx2x_bar bar[MAX_BARS]; /* map BARs 0, 2, 4 */

	uint16_t doorbell_size;

	/* periodic timer callout */
#define PERIODIC_STOP 0
#define PERIODIC_GO   1
	volatile unsigned long periodic_flags;
	rte_atomic32_t	scan_fp;
	struct bnx2x_fastpath fp[MAX_RSS_CHAINS];
	struct bnx2x_sp_objs  sp_objs[MAX_RSS_CHAINS];

	uint8_t  unit; /* driver instance number */

	int pcie_bus;    /* PCIe bus number */
	int pcie_device; /* PCIe device/slot number */
	int pcie_func;   /* PCIe function number */

	uint8_t pfunc_rel; /* function relative */
	uint8_t pfunc_abs; /* function absolute */
	uint8_t path_id;   /* function absolute */
#define SC_PATH(sc)     (sc->path_id)
#define SC_PORT(sc)     (sc->pfunc_rel & 1)
#define SC_FUNC(sc)     (sc->pfunc_rel)
#define SC_ABS_FUNC(sc) (sc->pfunc_abs)
#define SC_VN(sc)       (sc->pfunc_rel >> 1)
#define SC_L_ID(sc)     (SC_VN(sc) << 2)
#define PORT_ID(sc)     SC_PORT(sc)
#define PATH_ID(sc)     SC_PATH(sc)
#define VNIC_ID(sc)     SC_VN(sc)
#define FUNC_ID(sc)     SC_FUNC(sc)
#define ABS_FUNC_ID(sc) SC_ABS_FUNC(sc)
#define SC_FW_MB_IDX_VN(sc, vn)                                \
	(SC_PORT(sc) + (vn) *                                      \
	 ((CHIP_IS_E1x(sc) || (CHIP_IS_MODE_4_PORT(sc))) ? 2 : 1))
#define SC_FW_MB_IDX(sc) SC_FW_MB_IDX_VN(sc, SC_VN(sc))

	int if_capen; /* enabled interface capabilities */

	struct bnx2x_devinfo devinfo;
	char fw_ver_str[32];
	char mf_mode_str[32];
	char pci_link_str[32];

	struct iro *iro_array;

	int dmae_ready;
#define DMAE_READY(sc) (sc->dmae_ready)

	struct ecore_credit_pool_obj vlans_pool;
	struct ecore_credit_pool_obj macs_pool;
	struct ecore_rx_mode_obj     rx_mode_obj;
	struct ecore_mcast_obj       mcast_obj;
	struct ecore_rss_config_obj  rss_conf_obj;
	struct ecore_func_sp_obj     func_obj;

	uint16_t fw_seq;
	uint16_t fw_drv_pulse_wr_seq;
	uint32_t func_stx;

	struct elink_params         link_params;
	struct elink_vars           link_vars;
	uint32_t                    link_cnt;
	struct bnx2x_link_report_data last_reported_link;
	char mac_addr_str[32];

	uint32_t tx_ring_size;
	uint32_t rx_ring_size;
	int wol;

	int is_leader;
	int recovery_state;
#define BNX2X_RECOVERY_DONE        1
#define BNX2X_RECOVERY_INIT        2
#define BNX2X_RECOVERY_WAIT        3
#define BNX2X_RECOVERY_FAILED      4
#define BNX2X_RECOVERY_NIC_LOADING 5

	uint32_t rx_mode;
#define BNX2X_RX_MODE_NONE             0
#define BNX2X_RX_MODE_NORMAL           1
#define BNX2X_RX_MODE_ALLMULTI         2
#define BNX2X_RX_MODE_ALLMULTI_PROMISC 3
#define BNX2X_RX_MODE_PROMISC          4
#define BNX2X_MAX_MULTICAST            64

	struct bnx2x_port port;

	struct cmng_init cmng;

	/* user configs */
	uint8_t  num_queues;
	int      hc_rx_ticks;
	int      hc_tx_ticks;
	uint32_t rx_budget;
	int      interrupt_mode;
#define INTR_MODE_INTX 0
#define INTR_MODE_MSI  1
#define INTR_MODE_MSIX 2
#define INTR_MODE_SINGLE_MSIX 3
	int      udp_rss;

	uint8_t         igu_dsb_id;
	uint8_t         igu_base_sb;
	uint8_t         igu_sb_cnt;
	uint32_t        igu_base_addr;
	uint8_t         base_fw_ndsb;
#define DEF_SB_IGU_ID 16
#define DEF_SB_ID     HC_SP_SB_ID

	/* default status block */
	struct bnx2x_dma              def_sb_dma;
	struct host_sp_status_block *def_sb;
	uint16_t                    def_idx;
	uint16_t                    def_att_idx;
	uint32_t                    attn_state;
	struct attn_route           attn_group[MAX_DYNAMIC_ATTN_GRPS];

	/* general SP events - stats query, cfc delete, etc */
#define HC_SP_INDEX_ETH_DEF_CONS         3
	/* EQ completions */
#define HC_SP_INDEX_EQ_CONS              7
	/* FCoE L2 connection completions */
#define HC_SP_INDEX_ETH_FCOE_TX_CQ_CONS  6
#define HC_SP_INDEX_ETH_FCOE_RX_CQ_CONS  4
	/* iSCSI L2 */
#define HC_SP_INDEX_ETH_ISCSI_CQ_CONS    5
#define HC_SP_INDEX_ETH_ISCSI_RX_CQ_CONS 1

	/* event queue */
	struct bnx2x_dma        eq_dma;
	union event_ring_elem *eq;
	uint16_t              eq_prod;
	uint16_t              eq_cons;
	uint16_t              *eq_cons_sb;
#define NUM_EQ_PAGES     1 /* must be a power of 2 */
#define EQ_DESC_CNT_PAGE (BNX2X_PAGE_SIZE / sizeof(union event_ring_elem))
#define EQ_DESC_MAX_PAGE (EQ_DESC_CNT_PAGE - 1)
#define NUM_EQ_DESC      (EQ_DESC_CNT_PAGE * NUM_EQ_PAGES)
#define EQ_DESC_MASK     (NUM_EQ_DESC - 1)
#define MAX_EQ_AVAIL     (EQ_DESC_MAX_PAGE * NUM_EQ_PAGES - 2)
	/* depends on EQ_DESC_CNT_PAGE being a power of 2 */
#define NEXT_EQ_IDX(x)                                      \
	((((x) & EQ_DESC_MAX_PAGE) == (EQ_DESC_MAX_PAGE - 1)) ? \
	 ((x) + 2) : ((x) + 1))
	/* depends on the above and on NUM_EQ_PAGES being a power of 2 */
#define EQ_DESC(x) ((x) & EQ_DESC_MASK)

	/* slow path */
	struct bnx2x_dma      sp_dma;
	struct bnx2x_slowpath *sp;
	unsigned long       sp_state;

	/* slow path queue */
	struct bnx2x_dma spq_dma;
	struct eth_spe *spq;
#define SP_DESC_CNT     (BNX2X_PAGE_SIZE / sizeof(struct eth_spe))
#define MAX_SP_DESC_CNT (SP_DESC_CNT - 1)
#define MAX_SPQ_PENDING 8

	uint16_t       spq_prod_idx;
	struct eth_spe *spq_prod_bd;
	struct eth_spe *spq_last_bd;
	uint16_t       *dsb_sp_prod;

	volatile unsigned long eq_spq_left; /* COMMON_xxx ramrod credit */
	volatile unsigned long cq_spq_left; /* ETH_xxx ramrod credit */

	/* fw decompression buffer */
	struct bnx2x_dma gz_buf_dma;
	void           *gz_buf;
	uint32_t       gz_outlen;
#define GUNZIP_BUF(sc)    (sc->gz_buf)
#define GUNZIP_OUTLEN(sc) (sc->gz_outlen)
#define GUNZIP_PHYS(sc)   (rte_iova_t)(sc->gz_buf_dma.paddr)
#define FW_BUF_SIZE       0x40000

	struct raw_op *init_ops;
	uint16_t *init_ops_offsets; /* init block offsets inside init_ops */
	uint32_t *init_data;        /* data blob, 32 bit granularity */
	uint32_t       init_mode_flags;
#define INIT_MODE_FLAGS(sc) (sc->init_mode_flags)
	/* PRAM blobs - raw data */
	const uint8_t *tsem_int_table_data;
	const uint8_t *tsem_pram_data;
	const uint8_t *usem_int_table_data;
	const uint8_t *usem_pram_data;
	const uint8_t *xsem_int_table_data;
	const uint8_t *xsem_pram_data;
	const uint8_t *csem_int_table_data;
	const uint8_t *csem_pram_data;
#define INIT_OPS(sc)                 (sc->init_ops)
#define INIT_OPS_OFFSETS(sc)         (sc->init_ops_offsets)
#define INIT_DATA(sc)                (sc->init_data)
#define INIT_TSEM_INT_TABLE_DATA(sc) (sc->tsem_int_table_data)
#define INIT_TSEM_PRAM_DATA(sc)      (sc->tsem_pram_data)
#define INIT_USEM_INT_TABLE_DATA(sc) (sc->usem_int_table_data)
#define INIT_USEM_PRAM_DATA(sc)      (sc->usem_pram_data)
#define INIT_XSEM_INT_TABLE_DATA(sc) (sc->xsem_int_table_data)
#define INIT_XSEM_PRAM_DATA(sc)      (sc->xsem_pram_data)
#define INIT_CSEM_INT_TABLE_DATA(sc) (sc->csem_int_table_data)
#define INIT_CSEM_PRAM_DATA(sc)      (sc->csem_pram_data)

#define PHY_FW_VER_LEN			20
	char			fw_ver[32];

	/* ILT
	 * For max 196 cids (64*3 + non-eth), 32KB ILT page size and 1KB
	 * context size we need 8 ILT entries.
	 */
#define ILT_MAX_L2_LINES 8
	struct hw_context context[ILT_MAX_L2_LINES];
	struct ecore_ilt *ilt;
#define ILT_MAX_LINES 256

	/* max supported number of RSS queues: IGU SBs minus one for CNIC */
#define BNX2X_MAX_RSS_COUNT(sc) ((sc)->igu_sb_cnt - CNIC_SUPPORT(sc))
	/* max CID count: Max RSS * Max_Tx_Multi_Cos + FCoE + iSCSI */
#define BNX2X_L2_MAX_CID(sc)                                              \
	(BNX2X_MAX_RSS_COUNT(sc) * ECORE_MULTI_TX_COS + 2 * CNIC_SUPPORT(sc))
#define BNX2X_L2_CID_COUNT(sc)                                             \
	(BNX2X_NUM_ETH_QUEUES(sc) * ECORE_MULTI_TX_COS + 2 * CNIC_SUPPORT(sc))
#define L2_ILT_LINES(sc)                                \
	(DIV_ROUND_UP(BNX2X_L2_CID_COUNT(sc), ILT_PAGE_CIDS))

	int qm_cid_count;

	uint8_t dropless_fc;

	/* total number of FW statistics requests */
	uint8_t fw_stats_num;
	/*
	 * This is a memory buffer that will contain both statistics ramrod
	 * request and data.
	 */
	struct bnx2x_dma fw_stats_dma;
	/*
	 * FW statistics request shortcut (points at the beginning of fw_stats
	 * buffer).
	 */
	int                     fw_stats_req_size;
	struct bnx2x_fw_stats_req *fw_stats_req;
	rte_iova_t              fw_stats_req_mapping;
	/*
	 * FW statistics data shortcut (points at the beginning of fw_stats
	 * buffer + fw_stats_req_size).
	 */
	int                      fw_stats_data_size;
	struct bnx2x_fw_stats_data *fw_stats_data;
	rte_iova_t               fw_stats_data_mapping;

	/* tracking a pending STAT_QUERY ramrod */
	uint16_t stats_pending;
	/* number of completed statistics ramrods */
	uint16_t stats_comp;
	uint16_t stats_counter;
	uint8_t  stats_init;
	int      stats_state;

	struct bnx2x_eth_stats         eth_stats;
	struct host_func_stats       func_stats;
	struct bnx2x_eth_stats_old     eth_stats_old;
	struct bnx2x_net_stats_old     net_stats_old;
	struct bnx2x_fw_port_stats_old fw_stats_old;

	struct dmae_command stats_dmae; /* used by dmae command loader */
	int                 executer_idx;

	int mtu;

	/* DCB support on/off */
	int dcb_state;
#define BNX2X_DCB_STATE_OFF 0
#define BNX2X_DCB_STATE_ON  1
	/* DCBX engine mode */
	int dcbx_enabled;
#define BNX2X_DCBX_ENABLED_OFF        0
#define BNX2X_DCBX_ENABLED_ON_NEG_OFF 1
#define BNX2X_DCBX_ENABLED_ON_NEG_ON  2
#define BNX2X_DCBX_ENABLED_INVALID    -1

	uint8_t cnic_support;
	uint8_t cnic_enabled;
	uint8_t cnic_loaded;
#define CNIC_SUPPORT(sc) 0 /* ((sc)->cnic_support) */
#define CNIC_ENABLED(sc) 0 /* ((sc)->cnic_enabled) */
#define CNIC_LOADED(sc)  0 /* ((sc)->cnic_loaded) */

	/* multiple tx classes of service */
	uint8_t max_cos;
#define BNX2X_MAX_PRIORITY 8
	/* priority to cos mapping */
	uint8_t prio_to_cos[BNX2X_MAX_PRIORITY];

	int panic;
}; /* struct bnx2x_softc */

/* IOCTL sub-commands for edebug and firmware upgrade */
#define BNX2X_IOC_RD_NVRAM        1
#define BNX2X_IOC_WR_NVRAM        2
#define BNX2X_IOC_STATS_SHOW_NUM  3
#define BNX2X_IOC_STATS_SHOW_STR  4
#define BNX2X_IOC_STATS_SHOW_CNT  5

struct bnx2x_nvram_data {
    uint32_t op; /* ioctl sub-command */
    uint32_t offset;
    uint32_t len;
    uint32_t value[1]; /* variable */
};

union bnx2x_stats_show_data {
    uint32_t op; /* ioctl sub-command */

    struct {
	uint32_t num; /* return number of stats */
	uint32_t len; /* length of each string item */
    } desc;

    /* variable length... */
    char str[1]; /* holds names of desc.num stats, each desc.len in length */

    /* variable length... */
    uint64_t stats[1]; /* holds all stats */
};

/* function init flags */
#define FUNC_FLG_RSS     0x0001
#define FUNC_FLG_STATS   0x0002
/* FUNC_FLG_UNMATCHED       0x0004 */
#define FUNC_FLG_SPQ     0x0010
#define FUNC_FLG_LEADING 0x0020 /* PF only */

struct bnx2x_func_init_params {
    rte_iova_t fw_stat_map; /* (dma) valid if FUNC_FLG_STATS */
    rte_iova_t spq_map;     /* (dma) valid if FUNC_FLG_SPQ */
    uint16_t   func_flgs;
    uint16_t   func_id;     /* abs function id */
    uint16_t   pf_id;
    uint16_t   spq_prod;    /* valid if FUNC_FLG_SPQ */
};

/* memory resources reside at BARs 0, 2, 4 */
/* Run `pciconf -lb` to see mappings */
#define BAR0 0
#define BAR1 2
#define BAR2 4

static inline void
bnx2x_reg_write8(struct bnx2x_softc *sc, size_t offset, uint8_t val)
{
	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "offset=0x%08lx val=0x%02x",
			       (unsigned long)offset, val);
	rte_write8(val, ((uint8_t *)sc->bar[BAR0].base_addr + offset));
}

static inline void
bnx2x_reg_write16(struct bnx2x_softc *sc, size_t offset, uint16_t val)
{
#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
	if ((offset % 2) != 0)
		PMD_DRV_LOG(NOTICE, sc, "Unaligned 16-bit write to 0x%08lx",
			    (unsigned long)offset);
#endif
	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "offset=0x%08lx val=0x%04x",
			       (unsigned long)offset, val);
	rte_write16(val, ((uint8_t *)sc->bar[BAR0].base_addr + offset));

}

static inline void
bnx2x_reg_write32(struct bnx2x_softc *sc, size_t offset, uint32_t val)
{
#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
	if ((offset % 4) != 0)
		PMD_DRV_LOG(NOTICE, sc, "Unaligned 32-bit write to 0x%08lx",
			    (unsigned long)offset);
#endif

	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "offset=0x%08lx val=0x%08x",
			       (unsigned long)offset, val);
	rte_write32(val, ((uint8_t *)sc->bar[BAR0].base_addr + offset));
}

static inline uint8_t
bnx2x_reg_read8(struct bnx2x_softc *sc, size_t offset)
{
	uint8_t val;

	val = rte_read8((uint8_t *)sc->bar[BAR0].base_addr + offset);
	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "offset=0x%08lx val=0x%02x",
			       (unsigned long)offset, val);

	return val;
}

static inline uint16_t
bnx2x_reg_read16(struct bnx2x_softc *sc, size_t offset)
{
	uint16_t val;

#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
	if ((offset % 2) != 0)
		PMD_DRV_LOG(NOTICE, sc, "Unaligned 16-bit read from 0x%08lx",
			    (unsigned long)offset);
#endif

	val = rte_read16(((uint8_t *)sc->bar[BAR0].base_addr + offset));
	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "offset=0x%08lx val=0x%08x",
			       (unsigned long)offset, val);

	return val;
}

static inline uint32_t
bnx2x_reg_read32(struct bnx2x_softc *sc, size_t offset)
{
	uint32_t val;

#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
	if ((offset % 4) != 0)
		PMD_DRV_LOG(NOTICE, sc, "Unaligned 32-bit read from 0x%08lx",
			    (unsigned long)offset);
#endif

	val = rte_read32(((uint8_t *)sc->bar[BAR0].base_addr + offset));
	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "offset=0x%08lx val=0x%08x",
			       (unsigned long)offset, val);

	return val;
}

#define REG_ADDR(sc, offset) (((uint64_t)sc->bar[BAR0].base_addr) + (offset))

#define REG_RD8(sc, offset)  bnx2x_reg_read8(sc, (offset))
#define REG_RD16(sc, offset) bnx2x_reg_read16(sc, (offset))
#define REG_RD32(sc, offset) bnx2x_reg_read32(sc, (offset))

#define REG_WR8(sc, offset, val)  bnx2x_reg_write8(sc, (offset), val)
#define REG_WR16(sc, offset, val) bnx2x_reg_write16(sc, (offset), val)
#define REG_WR32(sc, offset, val) bnx2x_reg_write32(sc, (offset), val)

#define REG_RD(sc, offset)      REG_RD32(sc, offset)
#define REG_WR(sc, offset, val) REG_WR32(sc, offset, val)

#define BNX2X_SP(sc, var) (&(sc)->sp->var)
#define BNX2X_SP_MAPPING(sc, var) \
    (sc->sp_dma.paddr + offsetof(struct bnx2x_slowpath, var))

#define BNX2X_FP(sc, nr, var) ((sc)->fp[(nr)].var)
#define BNX2X_SP_OBJ(sc, fp) ((sc)->sp_objs[(fp)->index])

#define bnx2x_fp(sc, nr, var)   ((sc)->fp[nr].var)

#define REG_RD_DMAE(sc, offset, valp, len32)               \
    do {                                                   \
	(void)bnx2x_read_dmae(sc, offset, len32);                  \
	rte_memcpy(valp, BNX2X_SP(sc, wb_data[0]), (len32) * 4); \
    } while (0)

#define REG_WR_DMAE(sc, offset, valp, len32)                            \
    do {                                                                \
	rte_memcpy(BNX2X_SP(sc, wb_data[0]), valp, (len32) * 4);              \
	(void)bnx2x_write_dmae(sc, BNX2X_SP_MAPPING(sc, wb_data), offset, len32); \
    } while (0)

#define REG_WR_DMAE_LEN(sc, offset, valp, len32) \
    REG_WR_DMAE(sc, offset, valp, len32)

#define REG_RD_DMAE_LEN(sc, offset, valp, len32) \
    REG_RD_DMAE(sc, offset, valp, len32)

#define VIRT_WR_DMAE_LEN(sc, data, addr, len32, le32_swap)         \
    do {                                                           \
	/* if (le32_swap) {                                     */ \
	/*    PMD_PWARN_LOG(sc, "VIRT_WR_DMAE_LEN with le32_swap=1"); */ \
	/* }                                                    */ \
	rte_memcpy(GUNZIP_BUF(sc), data, len32 * 4);                   \
	ecore_write_big_buf_wb(sc, addr, len32);                   \
    } while (0)

#define BNX2X_DB_MIN_SHIFT 3   /* 8 bytes */
#define BNX2X_DB_SHIFT     7   /* 128 bytes */
#if (BNX2X_DB_SHIFT < BNX2X_DB_MIN_SHIFT)
#error "Minimum DB doorbell stride is 8"
#endif
#define DPM_TRIGGER_TYPE 0x40

/* Doorbell macro */
#define BNX2X_DB_WRITE(db_bar, val) rte_write32_relaxed((val), (db_bar))

#define BNX2X_DB_READ(db_bar) rte_read32_relaxed(db_bar)

#define DOORBELL_ADDR(sc, offset) \
	(volatile uint32_t *)(((char *)(sc)->bar[BAR1].base_addr + (offset)))

#define DOORBELL(sc, cid, val) \
	if (IS_PF(sc)) \
	BNX2X_DB_WRITE((DOORBELL_ADDR(sc, sc->doorbell_size * (cid) + DPM_TRIGGER_TYPE)), (val)); \
	else \
	BNX2X_DB_WRITE((DOORBELL_ADDR(sc, sc->doorbell_size * (cid))), (val)) \

#define SHMEM_ADDR(sc, field)                                       \
    (sc->devinfo.shmem_base + offsetof(struct shmem_region, field))
#define SHMEM_RD(sc, field)      REG_RD(sc, SHMEM_ADDR(sc, field))
#define SHMEM_RD16(sc, field)    REG_RD16(sc, SHMEM_ADDR(sc, field))
#define SHMEM_WR(sc, field, val) REG_WR(sc, SHMEM_ADDR(sc, field), val)

#define SHMEM2_ADDR(sc, field)                                        \
    (sc->devinfo.shmem2_base + offsetof(struct shmem2_region, field))
#define SHMEM2_HAS(sc, field)                                            \
    (sc->devinfo.shmem2_base && (REG_RD(sc, SHMEM2_ADDR(sc, size)) >     \
				 offsetof(struct shmem2_region, field)))
#define SHMEM2_RD(sc, field)      REG_RD(sc, SHMEM2_ADDR(sc, field))
#define SHMEM2_WR(sc, field, val) REG_WR(sc, SHMEM2_ADDR(sc, field), val)

#define MFCFG_ADDR(sc, field)                                  \
    (sc->devinfo.mf_cfg_base + offsetof(struct mf_cfg, field))
#define MFCFG_RD(sc, field)      REG_RD(sc, MFCFG_ADDR(sc, field))
#define MFCFG_RD16(sc, field)    REG_RD16(sc, MFCFG_ADDR(sc, field))
#define MFCFG_WR(sc, field, val) REG_WR(sc, MFCFG_ADDR(sc, field), val)

/* DMAE command defines */

#define DMAE_TIMEOUT      -1
#define DMAE_PCI_ERROR    -2 /* E2 and onward */
#define DMAE_NOT_RDY      -3
#define DMAE_PCI_ERR_FLAG 0x80000000

#define DMAE_SRC_PCI      0
#define DMAE_SRC_GRC      1

#define DMAE_DST_NONE     0
#define DMAE_DST_PCI      1
#define DMAE_DST_GRC      2

#define DMAE_COMP_PCI     0
#define DMAE_COMP_GRC     1

#define DMAE_COMP_REGULAR 0
#define DMAE_COM_SET_ERR  1

#define DMAE_CMD_SRC_PCI (DMAE_SRC_PCI << DMAE_COMMAND_SRC_SHIFT)
#define DMAE_CMD_SRC_GRC (DMAE_SRC_GRC << DMAE_COMMAND_SRC_SHIFT)
#define DMAE_CMD_DST_PCI (DMAE_DST_PCI << DMAE_COMMAND_DST_SHIFT)
#define DMAE_CMD_DST_GRC (DMAE_DST_GRC << DMAE_COMMAND_DST_SHIFT)

#define DMAE_CMD_C_DST_PCI (DMAE_COMP_PCI << DMAE_COMMAND_C_DST_SHIFT)
#define DMAE_CMD_C_DST_GRC (DMAE_COMP_GRC << DMAE_COMMAND_C_DST_SHIFT)

#define DMAE_CMD_ENDIANITY_NO_SWAP   (0 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define DMAE_CMD_ENDIANITY_B_SWAP    (1 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define DMAE_CMD_ENDIANITY_DW_SWAP   (2 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define DMAE_CMD_ENDIANITY_B_DW_SWAP (3 << DMAE_COMMAND_ENDIANITY_SHIFT)

#define DMAE_CMD_PORT_0 0
#define DMAE_CMD_PORT_1 DMAE_COMMAND_PORT

#define DMAE_SRC_PF 0
#define DMAE_SRC_VF 1

#define DMAE_DST_PF 0
#define DMAE_DST_VF 1

#define DMAE_C_SRC 0
#define DMAE_C_DST 1

#define DMAE_LEN32_RD_MAX     0x80
#define DMAE_LEN32_WR_MAX(sc) 0x2000

#define DMAE_COMP_VAL 0x60d0d0ae /* E2 and beyond, upper bit indicates error */

#define MAX_DMAE_C_PER_PORT 8
#define INIT_DMAE_C(sc)     ((SC_PORT(sc) * MAX_DMAE_C_PER_PORT) + SC_VN(sc))
#define PMF_DMAE_C(sc)      ((SC_PORT(sc) * MAX_DMAE_C_PER_PORT) + E1HVN_MAX)

static const uint32_t dmae_reg_go_c[] = {
    DMAE_REG_GO_C0,  DMAE_REG_GO_C1,  DMAE_REG_GO_C2,  DMAE_REG_GO_C3,
    DMAE_REG_GO_C4,  DMAE_REG_GO_C5,  DMAE_REG_GO_C6,  DMAE_REG_GO_C7,
    DMAE_REG_GO_C8,  DMAE_REG_GO_C9,  DMAE_REG_GO_C10, DMAE_REG_GO_C11,
    DMAE_REG_GO_C12, DMAE_REG_GO_C13, DMAE_REG_GO_C14, DMAE_REG_GO_C15
};

#define ATTN_NIG_FOR_FUNC     (1L << 8)
#define ATTN_SW_TIMER_4_FUNC  (1L << 9)
#define GPIO_2_FUNC           (1L << 10)
#define GPIO_3_FUNC           (1L << 11)
#define GPIO_4_FUNC           (1L << 12)
#define ATTN_GENERAL_ATTN_1   (1L << 13)
#define ATTN_GENERAL_ATTN_2   (1L << 14)
#define ATTN_GENERAL_ATTN_3   (1L << 15)
#define ATTN_GENERAL_ATTN_4   (1L << 13)
#define ATTN_GENERAL_ATTN_5   (1L << 14)
#define ATTN_GENERAL_ATTN_6   (1L << 15)
#define ATTN_HARD_WIRED_MASK  0xff00
#define ATTENTION_ID          4

#define AEU_IN_ATTN_BITS_PXPPCICLOCKCLIENT_PARITY_ERROR \
    AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_PARITY_ERROR

#define MAX_IGU_ATTN_ACK_TO 100

#define STORM_ASSERT_ARRAY_SIZE 50

#define BNX2X_PMF_LINK_ASSERT(sc) \
    GENERAL_ATTEN_OFFSET(LINK_SYNC_ATTENTION_BIT_FUNC_0 + SC_FUNC(sc))

#define BNX2X_MC_ASSERT_BITS \
    (GENERAL_ATTEN_OFFSET(TSTORM_FATAL_ASSERT_ATTENTION_BIT) | \
     GENERAL_ATTEN_OFFSET(USTORM_FATAL_ASSERT_ATTENTION_BIT) | \
     GENERAL_ATTEN_OFFSET(CSTORM_FATAL_ASSERT_ATTENTION_BIT) | \
     GENERAL_ATTEN_OFFSET(XSTORM_FATAL_ASSERT_ATTENTION_BIT))

#define BNX2X_MCP_ASSERT \
    GENERAL_ATTEN_OFFSET(MCP_FATAL_ASSERT_ATTENTION_BIT)

#define BNX2X_GRC_TIMEOUT GENERAL_ATTEN_OFFSET(LATCHED_ATTN_TIMEOUT_GRC)
#define BNX2X_GRC_RSV     (GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCR) | \
			 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCT) | \
			 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCN) | \
			 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCU) | \
			 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCP) | \
			 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RSVD_GRC))

#define MULTI_MASK 0x7f

#define PFS_PER_PORT(sc)                               \
    ((CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) ? 2 : 4)
#define SC_MAX_VN_NUM(sc) PFS_PER_PORT(sc)

#define FIRST_ABS_FUNC_IN_PORT(sc)                    \
    ((CHIP_PORT_MODE(sc) == CHIP_PORT_MODE_NONE) ?    \
     PORT_ID(sc) : (PATH_ID(sc) + (2 * PORT_ID(sc))))

#define FOREACH_ABS_FUNC_IN_PORT(sc, i)            \
    for ((i) = FIRST_ABS_FUNC_IN_PORT(sc);         \
	 (i) < MAX_FUNC_NUM;                       \
	 (i) += (MAX_FUNC_NUM / PFS_PER_PORT(sc)))

#define BNX2X_SWCID_SHIFT 17
#define BNX2X_SWCID_MASK  ((0x1 << BNX2X_SWCID_SHIFT) - 1)

#define SW_CID(x)  (le32toh(x) & BNX2X_SWCID_MASK)
#define CQE_CMD(x) (le32toh(x) >> COMMON_RAMROD_ETH_RX_CQE_CMD_ID_SHIFT)

#define CQE_TYPE(cqe_fp_flags)   ((cqe_fp_flags) & ETH_FAST_PATH_RX_CQE_TYPE)
#define CQE_TYPE_START(cqe_type) ((cqe_type) == RX_ETH_CQE_TYPE_ETH_START_AGG)
#define CQE_TYPE_STOP(cqe_type)  ((cqe_type) == RX_ETH_CQE_TYPE_ETH_STOP_AGG)
#define CQE_TYPE_SLOW(cqe_type)  ((cqe_type) == RX_ETH_CQE_TYPE_ETH_RAMROD)
#define CQE_TYPE_FAST(cqe_type)  ((cqe_type) == RX_ETH_CQE_TYPE_ETH_FASTPATH)

/* must be used on a CID before placing it on a HW ring */
#define HW_CID(sc, x) \
    ((SC_PORT(sc) << 23) | (SC_VN(sc) << BNX2X_SWCID_SHIFT) | (x))

#define SPEED_10    10
#define SPEED_100   100
#define SPEED_1000  1000
#define SPEED_2500  2500
#define SPEED_10000 10000

#define PCI_PM_D0    1
#define PCI_PM_D3hot 2

int  bnx2x_test_bit(int nr, volatile unsigned long * addr);
void bnx2x_set_bit(unsigned int nr, volatile unsigned long * addr);
void bnx2x_clear_bit(int nr, volatile unsigned long * addr);
int  bnx2x_test_and_clear_bit(int nr, volatile unsigned long * addr);
int  bnx2x_cmpxchg(volatile int *addr, int old, int new);

int bnx2x_dma_alloc(struct bnx2x_softc *sc, size_t size,
		struct bnx2x_dma *dma, const char *msg, uint32_t align);
void bnx2x_dma_free(struct bnx2x_dma *dma);
uint32_t bnx2x_dmae_opcode_add_comp(uint32_t opcode, uint8_t comp_type);
uint32_t bnx2x_dmae_opcode_clr_src_reset(uint32_t opcode);
uint32_t bnx2x_dmae_opcode(struct bnx2x_softc *sc, uint8_t src_type,
			 uint8_t dst_type, uint8_t with_comp,
			 uint8_t comp_type);
void bnx2x_post_dmae(struct bnx2x_softc *sc, struct dmae_command *dmae, int idx);
void bnx2x_read_dmae(struct bnx2x_softc *sc, uint32_t src_addr, uint32_t len32);
void bnx2x_write_dmae(struct bnx2x_softc *sc, rte_iova_t dma_addr,
		    uint32_t dst_addr, uint32_t len32);
void bnx2x_set_ctx_validation(struct bnx2x_softc *sc, struct eth_context *cxt,
			    uint32_t cid);
void bnx2x_update_coalesce_sb_index(struct bnx2x_softc *sc, uint8_t fw_sb_id,
				  uint8_t sb_index, uint8_t disable,
				  uint16_t usec);

int bnx2x_sp_post(struct bnx2x_softc *sc, int command, int cid,
		uint32_t data_hi, uint32_t data_lo, int cmd_type);

void ecore_init_e1h_firmware(struct bnx2x_softc *sc);
void ecore_init_e2_firmware(struct bnx2x_softc *sc);

void ecore_storm_memset_struct(struct bnx2x_softc *sc, uint32_t addr,
			       size_t size, uint32_t *data);

#define CATC_TRIGGER(sc, data) REG_WR((sc), 0x2000, (data));
#define CATC_TRIGGER_START(sc) CATC_TRIGGER((sc), 0xcafecafe)

#define BNX2X_MAC_FMT		"%pM"
#define BNX2X_MAC_PRN_LIST(mac)	(mac)

/***********/
/* INLINES */
/***********/

static inline uint32_t
reg_poll(struct bnx2x_softc *sc, uint32_t reg, uint32_t expected, int ms, int wait)
{
    uint32_t val;
    do {
	val = REG_RD(sc, reg);
	if (val == expected) {
	    break;
	}
	ms -= wait;
	DELAY(wait * 1000);
    } while (ms > 0);

    return val;
}

static inline void
bnx2x_update_fp_sb_idx(struct bnx2x_fastpath *fp)
{
	mb(); /* status block is written to by the chip */
	fp->fp_hc_idx = fp->sb_running_index[SM_RX_ID];
}

static inline void
bnx2x_igu_ack_sb_gen(struct bnx2x_softc *sc, uint8_t segment,
	uint16_t index, uint8_t op, uint8_t update, uint32_t igu_addr)
{
	struct igu_regular cmd_data = {0};

	cmd_data.sb_id_and_flags =
		((index << IGU_REGULAR_SB_INDEX_SHIFT) |
		 (segment << IGU_REGULAR_SEGMENT_ACCESS_SHIFT) |
		 (update << IGU_REGULAR_BUPDATE_SHIFT) |
		 (op << IGU_REGULAR_ENABLE_INT_SHIFT));

	REG_WR(sc, igu_addr, cmd_data.sb_id_and_flags);

	/* Make sure that ACK is written */
	mb();
}

static inline void
bnx2x_hc_ack_sb(struct bnx2x_softc *sc, uint8_t sb_id, uint8_t storm,
		uint16_t index, uint8_t op, uint8_t update)
{
	uint32_t hc_addr = (HC_REG_COMMAND_REG + SC_PORT(sc) * 32 +
			COMMAND_REG_INT_ACK);
	union igu_ack_register igu_ack;

	igu_ack.sb.status_block_index = index;
	igu_ack.sb.sb_id_and_flags =
		((sb_id << IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT) |
		 (storm << IGU_ACK_REGISTER_STORM_ID_SHIFT) |
		 (update << IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT) |
		 (op << IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT));

	REG_WR(sc, hc_addr, igu_ack.raw_data);

	/* Make sure that ACK is written */
	mb();
}

static inline uint32_t
bnx2x_hc_ack_int(struct bnx2x_softc *sc)
{
	uint32_t hc_addr = (HC_REG_COMMAND_REG + SC_PORT(sc) * 32 +
			COMMAND_REG_SIMD_MASK);
	uint32_t result = REG_RD(sc, hc_addr);

	mb();
	return result;
}

static inline uint32_t
bnx2x_igu_ack_int(struct bnx2x_softc *sc)
{
	uint32_t igu_addr = (BAR_IGU_INTMEM + IGU_REG_SISR_MDPC_WMASK_LSB_UPPER * 8);
	uint32_t result = REG_RD(sc, igu_addr);

	/* PMD_PDEBUG_LOG(sc, DBG_INTR, "read 0x%08x from IGU addr 0x%x",
			result, igu_addr); */

	mb();
	return result;
}

static inline uint32_t
bnx2x_ack_int(struct bnx2x_softc *sc)
{
	mb();
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		return bnx2x_hc_ack_int(sc);
	} else {
		return bnx2x_igu_ack_int(sc);
	}
}

static inline int
func_by_vn(struct bnx2x_softc *sc, int vn)
{
    return 2 * vn + SC_PORT(sc);
}

/*
 * send notification to other functions.
 */
static inline void
bnx2x_link_sync_notify(struct bnx2x_softc *sc)
{
	int func, vn;

	/* Set the attention towards other drivers on the same port */
	for (vn = VN_0; vn < SC_MAX_VN_NUM(sc); vn++) {
		if (vn == SC_VN(sc))
			continue;

		func = func_by_vn(sc, vn);
		REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_0 +
				(LINK_SYNC_ATTENTION_BIT_FUNC_0 + func) * 4, 1);
	}
}

/*
 * Statistics ID are global per chip/path, while Client IDs for E1x
 * are per port.
 */
static inline uint8_t
bnx2x_stats_id(struct bnx2x_fastpath *fp)
{
    struct bnx2x_softc *sc = fp->sc;

    if (!CHIP_IS_E1x(sc)) {
	return fp->cl_id;
    }

    return fp->cl_id + SC_PORT(sc) * FP_SB_MAX_E1x;
}

int bnx2x_init(struct bnx2x_softc *sc);
void bnx2x_load_firmware(struct bnx2x_softc *sc);
int bnx2x_attach(struct bnx2x_softc *sc);
int bnx2x_nic_unload(struct bnx2x_softc *sc, uint32_t unload_mode, uint8_t keep_link);
int bnx2x_alloc_hsi_mem(struct bnx2x_softc *sc);
int bnx2x_alloc_ilt_mem(struct bnx2x_softc *sc);
void bnx2x_free_ilt_mem(struct bnx2x_softc *sc);
void bnx2x_dump_tx_chain(struct bnx2x_fastpath * fp, int bd_prod, int count);
int bnx2x_tx_encap(struct bnx2x_tx_queue *txq, struct rte_mbuf *m0);
uint8_t bnx2x_txeof(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp);
void bnx2x_print_adapter_info(struct bnx2x_softc *sc);
void bnx2x_print_device_info(struct bnx2x_softc *sc);
int bnx2x_intr_legacy(struct bnx2x_softc *sc);
void bnx2x_link_status_update(struct bnx2x_softc *sc);
int bnx2x_complete_sp(struct bnx2x_softc *sc);
int bnx2x_set_storm_rx_mode(struct bnx2x_softc *sc);
void bnx2x_periodic_callout(struct bnx2x_softc *sc);
void bnx2x_periodic_stop(void *param);

int bnx2x_vf_get_resources(struct bnx2x_softc *sc, uint8_t tx_count, uint8_t rx_count);
void bnx2x_vf_close(struct bnx2x_softc *sc);
int bnx2x_vf_init(struct bnx2x_softc *sc);
void bnx2x_vf_unload(struct bnx2x_softc *sc);
int bnx2x_vf_setup_queue(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
	int leading);
void bnx2x_free_hsi_mem(struct bnx2x_softc *sc);
int bnx2x_vf_set_rx_mode(struct bnx2x_softc *sc);
int bnx2x_check_bull(struct bnx2x_softc *sc);

//#define BNX2X_PULSE

#define BNX2X_PCI_CAP  1
#define BNX2X_PCI_ECAP 2

static inline struct bnx2x_pci_cap*
pci_find_cap(struct bnx2x_softc *sc, uint8_t id, uint8_t type)
{
	struct bnx2x_pci_cap *cap = sc->pci_caps;

	while (cap) {
		if (cap->id == id && cap->type == type)
			return cap;
		cap = cap->next;
	}

	return NULL;
}

static inline void
bnx2x_set_rx_mode(struct bnx2x_softc *sc)
{
	if (sc->state == BNX2X_STATE_OPEN) {
		if (IS_PF(sc)) {
			bnx2x_set_storm_rx_mode(sc);
		} else {
			sc->rx_mode = BNX2X_RX_MODE_PROMISC;
			bnx2x_vf_set_rx_mode(sc);
		}
	} else {
		PMD_DRV_LOG(INFO, sc, "Card is not ready to change mode");
	}
}

static inline int pci_read(struct bnx2x_softc *sc, size_t addr,
			   void *val, uint8_t size)
{
	if (rte_pci_read_config(sc->pci_dev, val, size, addr) <= 0) {
		PMD_DRV_LOG(ERR, sc, "Can't read from PCI config space");
		return ENXIO;
	}

	return 0;
}

static inline int pci_write_word(struct bnx2x_softc *sc, size_t addr, off_t val)
{
	uint16_t val16 = val;

	if (rte_pci_write_config(sc->pci_dev, &val16,
				     sizeof(val16), addr) <= 0) {
		PMD_DRV_LOG(ERR, sc, "Can't write to PCI config space");
		return ENXIO;
	}

	return 0;
}

static inline int pci_write_long(struct bnx2x_softc *sc, size_t addr, off_t val)
{
	uint32_t val32 = val;
	if (rte_pci_write_config(sc->pci_dev, &val32,
				     sizeof(val32), addr) <= 0) {
		PMD_DRV_LOG(ERR, sc, "Can't write to PCI config space");
		return ENXIO;
	}

	return 0;
}

#endif /* __BNX2X_H__ */
