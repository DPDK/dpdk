/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 *
 * This header file defines the register offsets and bit fields
 * of ENETC4 PF and VFs.
 */

#ifndef _ENETC4_HW_H_
#define _ENETC4_HW_H_
#include <rte_io.h>

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
#define ENETC4_TXBD_FLAGS_L_TX_CKSUM	BIT(3)
#define ENETC4_TXBD_FLAGS_F		BIT(7)
/* L4 type */
#define ENETC4_TXBD_L4T_UDP		BIT(0)
#define ENETC4_TXBD_L4T_TCP		BIT(1)
/* L3 type is set to 0 for IPv4 and 1 for IPv6 */
#define ENETC4_TXBD_L3T			0
/* IPv4 checksum */
#define ENETC4_TXBD_IPCS		1

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

/* i.MX95 supports jumbo frame, but it is recommended to set the max frame
 * size to 2000 bytes.
 */
#define ENETC4_MAC_MAXFRM_SIZE  2000

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

#define ENETC4_DEF_VSI_WAIT_TIMEOUT_UPDATE     100
#define ENETC4_DEF_VSI_WAIT_DELAY_UPDATE       2000 /* us */

/* Station interface statistics */
#define ENETC4_SIROCT0           0x300
#define ENETC4_SIRFRM0           0x308
#define ENETC4_SITOCT0           0x320
#define ENETC4_SITFRM0           0x328
#define ENETC4_SITDFCR           0x340

/* VSI MSG Registers */
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

/* general register accessors */
#define enetc4_rd_reg(reg)	rte_read32((void *)(reg))
#define enetc4_wr_reg(reg, val)  rte_write32((val), (void *)(reg))

#define enetc4_rd(hw, off)	 enetc4_rd_reg((size_t)(hw)->reg + (off))
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
