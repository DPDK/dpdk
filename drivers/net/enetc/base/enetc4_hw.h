/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024-2026 NXP
 *
 * This header file defines the register offsets and bit fields
 * of ENETC4 PF and VFs.
 */

#ifndef _ENETC4_HW_H_
#define _ENETC4_HW_H_
#include <rte_io.h>
#include "enetc_hw.h"

#define BIT(x)		((uint64_t)1 << ((x)))

/* ENETC4 device IDs */
#define ENETC4_DEV_ID		0xe101
#define ENETC4_DEV_ID_VF	0xef00
#define PCI_VENDOR_ID_NXP	0x1131

struct enetc_msg_swbd {
	void *vaddr;
	uint64_t dma;
	uint32_t size;
};

/* enetc4 txbd flags */
#define ENETC4_TXBD_FLAGS_L4CS		BIT(0)
/* Request LSO (Large Send Offload) segmentation for this frame */
#define ENETC4_TXBD_FLAGS_LSO		BIT(1)
/* L4 checksum insertion (also used for checksum update on LSO) */
#define ENETC4_TXBD_FLAGS_L_TX_CKSUM	BIT(3)
/* Extended descriptor: the next 16B ring entry is an extension BD */
#define ENETC4_TXBD_FLAGS_EXT		BIT(6)
#define ENETC4_TXBD_FLAGS_F		BIT(7)
/* L4 type */
#define ENETC4_TXBD_L4T_UDP		BIT(0)
#define ENETC4_TXBD_L4T_TCP		BIT(1)
/* L3 type is set to 0 for IPv4 and 1 for IPv6 */
#define ENETC4_TXBD_L3T			0
/* IPv4 checksum */
#define ENETC4_TXBD_IPCS		1

/*
 * Extension Transmit Buffer Descriptor (16B). When the standard BD has the
 * extended flag set, this occupies the next ring entry and carries the extra
 * fields required by LSO.
 */
struct enetc_tx_bd_ext {
	uint32_t timestamp;	/* PTP timestamp, unused for LSO */
	uint32_t vlan;		/* VLAN insert, unused for LSO */
	uint32_t lso;		/* LSO_MAX_SEG_SIZE and FRM_LEN_EXT */
	uint32_t flags;		/* extension flags */
};

/* LSO_MAX_SEG_SIZE occupies bits 13-0 of the LSO word */
#define ENETC4_TXBD_EXT_LSO_SEG_MASK	0x3fff
#define ENETC4_TXBD_EXT_LSO_SEG(mss) \
		((uint32_t)((mss) & ENETC4_TXBD_EXT_LSO_SEG_MASK))
/* FRM_LEN_EXT occupies bits 19-16 of the LSO word (top 4 bits of data len) */
#define ENETC4_TXBD_EXT_FRM_LEN_EXT(x) \
		(((uint32_t)((x) & 0xf)) << 16)

/* NETC does not create LSO frames larger than this many bytes */
#define ENETC4_LSO_MAX_FRAME		9600
/* Maximum LSO data unit (payload to be segmented) supported by HW: 256KB */
#define ENETC4_LSO_MAX_DATA_UNIT	(256 * 1024)

/***************************ENETC port registers**************************/
#define ENETC4_PMR		0x10
#define ENETC4_PMR_EN		(BIT(16) | BIT(17) | BIT(18))

#define ENETC4_PARCSCR		0x9c
#define L3_CKSUM		BIT(0)
#define L4_CKSUM		BIT(1)

/* Port Station interface promiscuous MAC mode register */
#define ENETC4_PSIPMMR		0x200
#define PSIPMMR_SI0_MAC_UP	BIT(0)
#define PSIPMMR_SI_MAC_UP	(BIT(0) | BIT(1) | BIT(2))
#define PSIPMMR_SI0_MAC_MP	BIT(16)
#define PSIPMMR_SI_MAC_MP	(BIT(16) | BIT(17) | BIT(18))

/* Port Station interface a primary MAC address registers */
#define ENETC4_PSIPMAR0(a)	((a) * 0x80 + 0x2000)
#define ENETC4_PSIPMAR1(a)	((a) * 0x80 + 0x2004)

/* Port MAC address register 0/1 */
#define ENETC4_PMAR0		0x4020
#define ENETC4_PMAR1		0x4024

/* Port operational register */
#define ENETC4_POR		0x4100

/* Port traffic class a transmit maximum SDU register */
#define ENETC4_PTCTMSDUR(a)	((a) * 0x20 + 0x4208)
#define SDU_TYPE_MPDU		BIT(16)

#define ENETC4_PM_CMD_CFG(mac)		(0x5008 + (mac) * 0x400)
#define PM_CMD_CFG_TX_EN		BIT(0)
#define PM_CMD_CFG_RX_EN		BIT(1)

/* RBaMR[CRC]: 0 = FCS removed, 1 = FCS preserved (KEEP_CRC) */
#define ENETC4_RBMR_CRC			BIT(8)
/*
 * RBaMR[BDS]: buffer descriptor size select for a receive ring.
 * 0 = standard 16B descriptors, 1 = extended 32B descriptors.
 * RSC requires 32B descriptors (BDS = 1). Matches the Linux enetc
 * driver definition (ENETC_RBMR_BDS = BIT(2)).
 */
#define ENETC4_RBMR_BDS			BIT(2)

/*
 * Rx BDR a RSC register (RBaRSCR), offset 0x30 from the ring base.
 * Controls Receive Segment Coalesce (RSC / LRO) for the ring.
 */
#define ENETC4_RBRSCR			0x30
/* Enable RSC on this ring */
#define ENETC4_RBRSCR_EN		BIT(31)
/* Permit coalescing of TCP segments that carry the timestamp option */
#define ENETC4_RBRSCR_CT		BIT(29)
/* SIZE field (bits 15-0): maximum coalesced frame size produced by RSC */
#define ENETC4_RBRSCR_SIZE(x)		((uint32_t)((x) & 0xffff))

/*
 * RBaICR0[ICEN]: interrupt coalescing enable. RSC requires interrupt
 * coalescing to be enabled; the coalescing timer doubles as the RSC flush
 * timer. ICPT (bits 8-0) is the packet-count threshold.
 */
#define ENETC4_RBICR0			0xa8
#define ENETC4_RBICR0_ICEN		BIT(31)
#define ENETC4_RBICR0_ICPT(x)		((uint32_t)((x) & 0x1ff))
/* Rx BDR a interrupt coalescing register 1 (threshold timer) */
#define ENETC4_RBICR1			0xac

/*
 * SI-level Rx interrupt detect register 0 (SIRXIDR0). W1C, one bit per Rx
 * ring (RX0..RX23). The interrupt-coalescing timer (which also gates RSC
 * coalescing) does not re-arm while a ring's detect bit stays set, so the
 * poll-mode driver writes BIT(ring index) here every poll to keep RSC
 * coalescing. The per-ring RBaIDR (0xa4) is read-only and cannot be used.
 */
#define ENETC_SIRXIDR			0xa28

/*
 * RSC (Receive Segment Coalesce) limits and defaults.
 * Maximum coalesced frame size the HW will build (programmed in RBaRSCR[SIZE]).
 * Bounded to 16 bits by the SIZE field width.
 */
#define ENETC4_RSC_MAX_FRAME		0xffff
/* Default interrupt coalescing packet threshold used to satisfy RSC's ICEN
 * precondition. The PMD is poll-mode, so this only gates the RSC flush timer.
 */
#define ENETC4_RSC_DEF_ICPT		1
/*
 * Default interrupt coalescing timer threshold (RBaICR1[ICTT]), in NETC
 * platform clock cycles. This timer is the RSC coalesce-hold window: HW keeps
 * a coalesced frame open while it runs and appends in-order segments. A value
 * of 0 disables the timer and flushes every segment separately (no
 * coalescing), so it must be non-zero for RSC to merge anything.
 */
#define ENETC4_RSC_DEF_ICTT		0x10000

/*
 * Extended 32B receive writeback buffer descriptor. Used only on RSC-enabled
 * rings (RBaMR[BDS] = 1). The first 16 bytes match the standard descriptor
 * writeback layout; the second 16 bytes carry the RSC and timestamp fields.
 * RSC_FRAMES reports how many frames were coalesced (1 to 255; 1 means the
 * frame was not coalesced).
 */
struct enetc_rx_bd_ext {
	uint32_t timestamp;	/* offset 0x10: PTP timestamp */
	uint32_t rsc_frames;	/* offset 0x14: RSC_FRAMES in bits 7-0 */
	uint32_t rsc_abs_ts_delta; /* offset 0x18 */
	uint32_t reserved;	/* offset 0x1c */
};

/* RSC_FRAMES occupies bits 7-0 of the rsc_frames word */
#define ENETC4_RXBD_EXT_RSC_FRAMES(x)	((x) & 0xff)

/* i.MX95 supports jumbo frame, but it is recommended to set the max frame
 * size to 2000 bytes.
 */
#define ENETC4_MAC_MAXFRM_SIZE  2000

/* Number of MAC Address Filter table entries */
#define ENETC4_MAC_ENTRIES      4

/* Port MAC 0/1 Maximum Frame Length Register */
#define ENETC4_PM_MAXFRM(mac)		(0x5014 + (mac) * 0x400)

/* Config register to reset counters */
#define ENETC4_PM0_STAT_CONFIG		0x50e0
/* Stats Reset Bit */
#define ENETC4_CLEAR_STATS		BIT(2)

/* Port MAC 0/1 Receive Ethernet Octets Counter */
#define ENETC4_PM_REOCT(mac)            (0x5100 + (mac) * 0x400)

/* Port MAC 0/1 Receive Frame Error Counter */
#define ENETC4_PM_RERR(mac)		(0x5138 + (mac) * 0x400)

/* Port MAC 0/1 Receive Dropped Packets Counter */
#define ENETC4_PM_RDRP(mac)		(0x5158 + (mac) * 0x400)

/* Port MAC 0/1 Receive Packets Counter */
#define ENETC4_PM_RPKT(mac)		(0x5160 + (mac) * 0x400)

/* Port MAC 0/1 Transmit Frame Error Counter */
#define ENETC4_PM_TERR(mac)		(0x5238 + (mac) * 0x400)

/* Port MAC 0/1 Transmit Ethernet Octets Counter */
#define ENETC4_PM_TEOCT(mac)            (0x5200 + (mac) * 0x400)

/* Port MAC 0/1 Transmit Packets Counter */
#define ENETC4_PM_TPKT(mac)		(0x5260 + (mac) * 0x400)

/* Port MAC 0 Interface Mode Control Register */
#define ENETC4_PM_IF_MODE(mac)		(0x5300 + (mac) * 0x400)
#define PM_IF_MODE_IFMODE		(BIT(0) | BIT(1) | BIT(2))
#define IFMODE_XGMII			0
#define IFMODE_RMII			3
#define IFMODE_RGMII			4
#define IFMODE_SGMII			5
#define PM_IF_MODE_ENA			BIT(15)

/* Port MAC 0 Interface Status Register */
#define ENETC4_PM_IF_STATUS(mac)	(0x5304 + (mac) * 0x400)
#define ENETC4_LINK_MODE                 0x0000000000080000ULL
#define ENETC4_LINK_STATUS               0x0000000000010000ULL
#define ENETC4_LINK_SPEED_MASK           0x0000000000060000ULL
#define ENETC4_LINK_SPEED_10M            0x0ULL
#define ENETC4_LINK_SPEED_100M           0x0000000000020000ULL
#define ENETC4_LINK_SPEED_1G             0x0000000000040000ULL

#define ENETC4_DEF_VSI_WAIT_TIMEOUT_UPDATE     100
#define ENETC4_DEF_VSI_WAIT_DELAY_UPDATE       2000 /* us */

/* Station interface statistics */
#define ENETC4_SIROCT0           0x300
#define ENETC4_SIRFRM0           0x308
#define ENETC4_SITOCT0           0x320
#define ENETC4_SITFRM0           0x328
#define ENETC4_SITDFCR           0x340

/* Station interface interrupts */
#define ENETC4_SIMSIVR           0xA30
#define ENETC4_VSIIER            0xA00
#define ENETC4_VSIIDR            0xA08
#define ENETC4_VSIIER_MRIE       BIT(9)
#define ENETC4_SI_INT_IDX        0
/* MSI-X vector base for per-Rx-queue interrupts; vector 0 is the mailbox. */
#define ENETC4_VF_RX_VEC_BASE    1

/* VSI Registers */
#define ENETC4_VSIMSGSR  0x204   /* RO */
#define ENETC4_VSIMSGSR_MB       BIT(0)
#define ENETC4_VSIMSGSR_MS       BIT(1)
#define ENETC4_VSIMSGSNDAR0      0x210
#define ENETC4_VSIMSGSNDAR1      0x214

#define ENETC4_VSIMSGRR		 0x208
#define ENETC4_VSIMSGRR_MR       BIT(0)

#define ENETC_SIMSGSR_SET_MC(val) ((val) << 16)
#define ENETC_SIMSGSR_GET_MC(val) ((val) >> 16)

/* Control BDR regs */
#define ENETC4_SICBDRMR		0x800
#define ENETC4_SICBDRSR		0x804   /* RO */
#define ENETC4_SICBDRBAR0	0x810
#define ENETC4_SICBDRBAR1	0x814
#define ENETC4_SICBDRPIR	0x818
#define ENETC4_SICBDRCIR	0x81c
#define ENETC4_SICBDRLENR	0x820
#define ENETC4_SICTR0		0x18
#define ENETC4_SICTR1		0x1c

/* PSI SI VLAN register: per-SI VLAN tag for insertion/removal */
#define ENETC4_PSIVLANR(a)		((a) * 0x80 + 0x2008)
#define ENETC4_PSIVLANR_E		BIT(31)  /* enable SI VLAN processing */
#define ENETC4_PSIVLANR_VTEA		BIT(30)  /* 0=strip tag, 1=zero VID */
#define ENETC4_PSIVLANR_PCP(v)		(((uint32_t)(v) & 0x7) << 13)
#define ENETC4_PSIVLANR_DEI		BIT(12)
#define ENETC4_PSIVLANR_VID(v)		((uint32_t)(v) & 0xfff)

/* PSI SI configuration register 0 */
#define ENETC4_PSICFGR0(a)		((a) * 0x80 + 0x2010)
#define ENETC4_PSICFGR0_SIVC_CVLAN	BIT(24)  /* allow C-VLAN 0x8100 */
#define ENETC4_PSICFGR0_SIVIE		BIT(14)  /* SI VLAN insertion enable */
#define ENETC4_PSICFGR0_VTE		BIT(12)  /* SI VLAN removal enable */

/* general register accessors */
#define enetc4_rd_reg(reg)	rte_read32((void *)(reg))
#define enetc4_wr_reg(reg, val)  rte_write32((val), (void *)(reg))

#define enetc4_rd(hw, off)	 enetc4_rd_reg((size_t)(hw)->reg + (off))
static inline uint64_t
enetc4_rd64(struct enetc_hw *hw, uint32_t off)
{
	size_t base = (size_t)hw->reg + off;
	uint32_t lo, hi, hi_check;

	/*
	 * A 64-bit statistics counter is read as two 32-bit accesses, so
	 * the low word can carry into the high word between the two reads
	 * and produce a value that is off by 2^32. Read the high word,
	 * then the low word, then re-read the high word; if the high word
	 * changed a carry happened during the sequence, so retry.
	 */
	do {
		hi = enetc4_rd_reg(base + 4);
		lo = enetc4_rd_reg(base);
		hi_check = enetc4_rd_reg(base + 4);
	} while (hi != hi_check);

	return (uint64_t)hi << 32 | lo;
}
#define enetc4_wr(hw, off, val)  enetc4_wr_reg((size_t)(hw)->reg + (off), val)
/* port register accessors - PF only */
#define enetc4_port_rd(hw, off)  enetc4_rd_reg((size_t)(hw)->port + (off))
#define enetc4_port_wr(hw, off, val) \
				enetc4_wr_reg((size_t)(hw)->port + (off), val)
/* BDR register accessors, see ENETC_BDR() */
#define enetc4_bdr_rd(hw, t, n, off) \
				enetc4_rd(hw, ENETC_BDR(t, n, off))
#define enetc4_bdr_wr(hw, t, n, off, val) \
				enetc4_wr(hw, ENETC_BDR(t, n, off), val)
#define enetc4_txbdr_rd(hw, n, off) enetc4_bdr_rd(hw, TX, n, off)
#define enetc4_rxbdr_rd(hw, n, off) enetc4_bdr_rd(hw, RX, n, off)
#define enetc4_txbdr_wr(hw, n, off, val) \
				enetc4_bdr_wr(hw, TX, n, off, val)
#define enetc4_rxbdr_wr(hw, n, off, val) \
				enetc4_bdr_wr(hw, RX, n, off, val)
#endif
