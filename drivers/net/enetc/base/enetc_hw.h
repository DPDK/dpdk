/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2020 NXP
 */

#ifndef _ENETC_HW_H_
#define _ENETC_HW_H_
#include <rte_io.h>

#define BIT(x)		((uint64_t)1 << ((x)))

/* ENETC device IDs */
#define ENETC_DEV_ID_VF		0xef00
#define ENETC_DEV_ID		0xe100

/* BD RING ALIGNMENT */
#define ENETC_BD_RING_ALIGN	128

/* ENETC register block BAR */
#define ENETC_BAR_REGS			0x0

/* SI regs, offset: 0h */
#define ENETC_SIMR			0x0
#define ENETC_SIMR_EN			BIT(31)

#define ENETC_SICAR0			0x40
#define ENETC_SICAR0_COHERENT		0x2B2B6727
#define ENETC_SIPMAR0			0x80
#define ENETC_SIPMAR1			0x84

#define ENETC_SICAPR0			0x900
#define ENETC_SICAPR1			0x904

#define ENETC_SIMSITRV(n)		(0xB00 + (n) * 0x4)
#define ENETC_SIMSIRRV(n)		(0xB80 + (n) * 0x4)

#define ENETC_SICCAPR			0x1200

/* enum for BD type */
enum enetc_bdr_type {TX, RX};

#define ENETC_BDR(type, n, off)		(0x8000 + (type) * 0x100 + (n) * 0x200 \
							+ (off))
/* RX BDR reg offsets */
#define ENETC_RBMR		0x0 /* RX BDR mode register*/
#define ENETC_RBMR_EN		BIT(31)

#define ENETC_RBSR		0x4  /* Rx BDR status register*/
#define ENETC_RBBSR		0x8  /* Rx BDR buffer size register*/
#define ENETC_RBCIR		0xc  /* Rx BDR consumer index register*/
#define ENETC_RBBAR0		0x10 /* Rx BDR base address register 0 */
#define ENETC_RBBAR1		0x14 /* Rx BDR base address register 1*/
#define ENETC_RBPIR		0x18 /* Rx BDR producer index register*/
#define ENETC_RBLENR		0x20 /* Rx BDR length register*/
#define ENETC_RBIER		0xa0 /* Rx BDR interrupt enable register*/
#define ENETC_RBIER_RXTIE	BIT(0)
#define ENETC_RBIDR		0xa4 /* Rx BDR interrupt detect register*/
#define ENETC_RBICIR0		0xa8 /* Rx BDR inetrrupt coalescing register 0*/
#define ENETC_RBICIR0_ICEN	BIT(31)


#define ENETC_TBMR	0x0  /* Tx BDR mode register (TBMR) 32 RW */
#define ENETC_TBSR	0x4  /* x BDR status register (TBSR) 32 RO */
#define ENETC_TBBAR0	0x10 /* Tx BDR base address register 0 (TBBAR0) 32 RW */
#define ENETC_TBBAR1	0x14 /* Tx BDR base address register 1 (TBBAR1) 32 RW */
#define ENETC_TBCIR	0x18 /* Tx BDR consumer index register (TBCIR) 32 RW */
#define ENETC_TBCISR	0x1C /* Tx BDR consumer index shadow register 32 RW */
#define ENETC_TBIER	0xA0 /* Tx BDR interrupt enable register 32 RW */
#define ENETC_TBIDR	0xA4 /* Tx BDR interrupt detect register 32 RO */
#define ENETC_TBICR0	0xA8 /* Tx BDR interrupt coalescing register 0 32 RW */
#define ENETC_TBICR1	0xAC /* Tx BDR interrupt coalescing register 1 32 RW */
#define ENETC_TBLENR	0x20

#define ENETC_TBCISR_IDX_MASK		0xffff
#define ENETC_TBIER_TXFIE		BIT(1)

#define ENETC_RTBLENR_LEN(n)		((n) & ~0x7)
#define ENETC_TBMR_EN			BIT(31)

/* Port regs, offset: 1_0000h */
#define ENETC_PORT_BASE			0x10000
#define ENETC_PMR			0x00000
#define ENETC_PMR_EN			(BIT(16) | BIT(17) | BIT(18))
#define ENETC_PSR			0x00004 /* RO */
#define ENETC_PSIPMR			0x00018
#define ENETC_PSIPMR_SET_UP(n)		(0x1 << (n)) /* n = SI index */
#define ENETC_PSIPMR_SET_MP(n)		(0x1 << ((n) + 16))
#define ENETC_PSIPMAR0(n)		(0x00100 + (n) * 0x20)
#define ENETC_PSIPMAR1(n)		(0x00104 + (n) * 0x20)
#define ENETC_PCAPR0			0x00900
#define ENETC_PCAPR1			0x00904
#define ENETC_PM0_RX_FIFO		0x801C
#define ENETC_PM0_IF_MODE		0x8300
#define ENETC_PM1_IF_MODE		0x9300
#define ENETC_PMO_IFM_RG		BIT(2)
#define ENETC_PM0_IFM_RLP		(BIT(5) | BIT(11))
#define ENETC_PM0_IFM_RGAUTO		(BIT(15) | ENETC_PMO_IFM_RG | BIT(1))
#define ENETC_PM0_IFM_XGMII		BIT(12)

#define ENETC_PV0CFGR(n)		(0x00920 + (n) * 0x10)
#define ENETC_PVCFGR_SET_TXBDR(val)	((val) & 0xff)
#define ENETC_PVCFGR_SET_RXBDR(val)	(((val) & 0xff) << 16)

#define ENETC_PM0_CMD_CFG		0x08008
#define ENETC_PM0_TX_EN			BIT(0)
#define ENETC_PM0_RX_EN			BIT(1)
#define ENETC_PM0_CRC			BIT(6)

#define ENETC_PAR_PORT_CFG		0x03050
#define L3_CKSUM			BIT(0)
#define L4_CKSUM			BIT(1)

#define ENETC_PM0_MAXFRM		0x08014
#define ENETC_SET_TX_MTU(val)		((val) << 16)
#define ENETC_SET_MAXFRM(val)		((val) & 0xffff)
#define ENETC_PTXMBAR			0x0608
/* n = TC index [0..7] */
#define ENETC_PTCMSDUR(n)		(0x2020 + (n) * 4)

#define ENETC_PM0_STATUS		0x08304
#define ENETC_LINK_MODE			0x0000000000080000ULL
#define ENETC_LINK_STATUS		0x0000000000010000ULL
#define ENETC_LINK_SPEED_MASK		0x0000000000060000ULL
#define ENETC_LINK_SPEED_10M		0x0ULL
#define ENETC_LINK_SPEED_100M		0x0000000000020000ULL
#define ENETC_LINK_SPEED_1G		0x0000000000040000ULL

/* Global regs, offset: 2_0000h */
#define ENETC_GLOBAL_BASE		0x20000
#define ENETC_G_EIPBRR0			0x00bf8
#define ENETC_G_EIPBRR1			0x00bfc

/* MAC Counters */
/* Config register to reset counters*/
#define ENETC_PM0_STAT_CONFIG		0x080E0
/* Receive frames counter without error */
#define ENETC_PM0_RFRM			0x08120
/* Receive packets counter, good + bad */
#define ENETC_PM0_RPKT			0x08160
/* Received octets, good + bad */
#define ENETC_PM0_REOCT			0x08120
/* Transmit octets, good + bad */
#define ENETC_PM0_TEOCT			0x08200
/* Transmit frames counter without error */
#define ENETC_PM0_TFRM			0x08220
/* Transmit packets counter, good + bad */
#define ENETC_PM0_TPKT			0x08260
/* Dropped not Truncated packets counter */
#define ENETC_PM0_RDRNTP		0x081C8
/* Dropped + trucated packets counter */
#define ENETC_PM0_RDRP			0x08158
/* Receive packets error counter */
#define ENETC_PM0_RERR			0x08138
/* Transmit packets error counter */
#define ENETC_PM0_TERR			0x08238

/* Stats Reset Bit*/
#define ENETC_CLEAR_STATS		BIT(2)

#define ENETC_G_EPFBLPR(n)		(0xd00 + 4 * (n))
#define ENETC_G_EPFBLPR1_XGMII		0x80000000

/* general register accessors */
#define enetc_rd_reg(reg)	rte_read32((void *)(reg))
#define enetc_wr_reg(reg, val)	rte_write32((val), (void *)(reg))
#define enetc_rd(hw, off)	enetc_rd_reg((size_t)(hw)->reg + (off))
#define enetc_wr(hw, off, val)	enetc_wr_reg((size_t)(hw)->reg + (off), val)
/* port register accessors - PF only */
#define enetc_port_rd(hw, off)	enetc_rd_reg((size_t)(hw)->port + (off))
#define enetc_port_wr(hw, off, val) \
				enetc_wr_reg((size_t)(hw)->port + (off), val)
/* global register accessors - PF only */
#define enetc_global_rd(hw, off) \
				enetc_rd_reg((size_t)(hw)->global + (off))
#define enetc_global_wr(hw, off, val) \
				enetc_wr_reg((size_t)(hw)->global + (off), val)
/* BDR register accessors, see ENETC_BDR() */
#define enetc_bdr_rd(hw, t, n, off) \
				enetc_rd(hw, ENETC_BDR(t, n, off))
#define enetc_bdr_wr(hw, t, n, off, val) \
				enetc_wr(hw, ENETC_BDR(t, n, off), val)

#define enetc_txbdr_rd(hw, n, off) enetc_bdr_rd(hw, TX, n, off)
#define enetc_rxbdr_rd(hw, n, off) enetc_bdr_rd(hw, RX, n, off)
#define enetc_txbdr_wr(hw, n, off, val) \
				enetc_bdr_wr(hw, TX, n, off, val)
#define enetc_rxbdr_wr(hw, n, off, val) \
				enetc_bdr_wr(hw, RX, n, off, val)

#define ENETC_TX_ADDR(txq, addr) ((void *)((txq)->enetc_txbdr + (addr)))

#define ENETC_TXBD_FLAGS_IE		BIT(13)
#define ENETC_TXBD_FLAGS_F		BIT(15)

/* ENETC Parsed values (Little Endian) */
#define ENETC_PARSE_ERROR		0x8000
#define ENETC_PKT_TYPE_ETHER            0x0060
#define ENETC_PKT_TYPE_IPV4             0x0000
#define ENETC_PKT_TYPE_IPV6             0x0020
#define ENETC_PKT_TYPE_IPV4_TCP \
			(0x0010 | ENETC_PKT_TYPE_IPV4)
#define ENETC_PKT_TYPE_IPV6_TCP \
			(0x0010 | ENETC_PKT_TYPE_IPV6)
#define ENETC_PKT_TYPE_IPV4_UDP \
			(0x0011 | ENETC_PKT_TYPE_IPV4)
#define ENETC_PKT_TYPE_IPV6_UDP \
			(0x0011 | ENETC_PKT_TYPE_IPV6)
#define ENETC_PKT_TYPE_IPV4_SCTP \
			(0x0013 | ENETC_PKT_TYPE_IPV4)
#define ENETC_PKT_TYPE_IPV6_SCTP \
			(0x0013 | ENETC_PKT_TYPE_IPV6)
#define ENETC_PKT_TYPE_IPV4_ICMP \
			(0x0003 | ENETC_PKT_TYPE_IPV4)
#define ENETC_PKT_TYPE_IPV6_ICMP \
			(0x0003 | ENETC_PKT_TYPE_IPV6)

/* PCI device info */
struct enetc_hw {
	void *reg;	/* SI registers, used by all PCI functions */
	void *port;	/* Port registers, PF only */
	void *global;	/* IP global registers, PF only */
};

struct enetc_eth_mac_info {
	uint8_t addr[RTE_ETHER_ADDR_LEN];
	uint8_t perm_addr[RTE_ETHER_ADDR_LEN];
	uint8_t get_link_status;
};

struct enetc_eth_hw {
	struct rte_eth_dev *ndev;
	struct enetc_hw hw;
	uint16_t device_id;
	uint16_t vendor_id;
	uint8_t revision_id;
	struct enetc_eth_mac_info mac;
};

/* Transmit Descriptor */
struct enetc_tx_desc {
	uint64_t addr;
	uint16_t frm_len;
	uint16_t buf_len;
	uint32_t flags_errors;
};

/* TX Buffer Descriptors (BD) */
struct enetc_tx_bd {
	uint64_t addr;
	uint16_t buf_len;
	uint16_t frm_len;
	uint16_t err_csum;
	uint16_t flags;
};

/* RX buffer descriptor */
union enetc_rx_bd {
	struct {
		uint64_t addr;
		uint8_t reserved[8];
	} w;
	struct {
		uint16_t inet_csum;
		uint16_t parse_summary;
		uint32_t rss_hash;
		uint16_t buf_len;
		uint16_t vlan_opt;
		union {
			struct {
				uint16_t flags;
				uint16_t error;
			};
			uint32_t lstatus;
		};
	} r;
};

#endif
