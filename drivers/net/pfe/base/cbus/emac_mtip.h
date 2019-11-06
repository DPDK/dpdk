/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _EMAC_H_
#define _EMAC_H_

/* This file is for Ethernet MAC registers and offsets
 */

#include <linux/ethtool.h>

#define EMAC_IEVENT_REG		0x004
#define EMAC_IMASK_REG		0x008
#define EMAC_R_DES_ACTIVE_REG	0x010
#define EMAC_X_DES_ACTIVE_REG	0x014
#define EMAC_ECNTRL_REG		0x024
#define EMAC_MII_DATA_REG	0x040
#define EMAC_MII_CTRL_REG	0x044
#define EMAC_MIB_CTRL_STS_REG	0x064
#define EMAC_RCNTRL_REG		0x084
#define EMAC_TCNTRL_REG		0x0C4
#define EMAC_PHY_ADDR_LOW	0x0E4
#define EMAC_PHY_ADDR_HIGH	0x0E8
#define EMAC_GAUR		0x120
#define EMAC_GALR		0x124
#define EMAC_TFWR_STR_FWD	0x144
#define EMAC_RX_SECTION_FULL	0x190
#define EMAC_RX_SECTION_EMPTY	0x194
#define EMAC_TX_SECTION_EMPTY	0x1A0
#define EMAC_TRUNC_FL		0x1B0

#define RMON_T_DROP	0x200 /* Count of frames not cntd correctly */
#define RMON_T_PACKETS	0x204 /* RMON TX packet count */
#define RMON_T_BC_PKT	0x208 /* RMON TX broadcast pkts */
#define RMON_T_MC_PKT	0x20c /* RMON TX multicast pkts */
#define RMON_T_CRC_ALIGN	0x210 /* RMON TX pkts with CRC align err */
#define RMON_T_UNDERSIZE	0x214 /* RMON TX pkts < 64 bytes, good CRC */
#define RMON_T_OVERSIZE	0x218 /* RMON TX pkts > MAX_FL bytes good CRC */
#define RMON_T_FRAG	0x21c /* RMON TX pkts < 64 bytes, bad CRC */
#define RMON_T_JAB	0x220 /* RMON TX pkts > MAX_FL bytes, bad CRC */
#define RMON_T_COL	0x224 /* RMON TX collision count */
#define RMON_T_P64	0x228 /* RMON TX 64 byte pkts */
#define RMON_T_P65TO127	0x22c /* RMON TX 65 to 127 byte pkts */
#define RMON_T_P128TO255	0x230 /* RMON TX 128 to 255 byte pkts */
#define RMON_T_P256TO511	0x234 /* RMON TX 256 to 511 byte pkts */
#define RMON_T_P512TO1023	0x238 /* RMON TX 512 to 1023 byte pkts */
#define RMON_T_P1024TO2047	0x23c /* RMON TX 1024 to 2047 byte pkts */
#define RMON_T_P_GTE2048	0x240 /* RMON TX pkts > 2048 bytes */
#define RMON_T_OCTETS	0x244 /* RMON TX octets */
#define IEEE_T_DROP	0x248 /* Count of frames not counted crtly */
#define IEEE_T_FRAME_OK	0x24c /* Frames tx'd OK */
#define IEEE_T_1COL	0x250 /* Frames tx'd with single collision */
#define IEEE_T_MCOL	0x254 /* Frames tx'd with multiple collision */
#define IEEE_T_DEF	0x258 /* Frames tx'd after deferral delay */
#define IEEE_T_LCOL	0x25c /* Frames tx'd with late collision */
#define IEEE_T_EXCOL	0x260 /* Frames tx'd with excesv collisions */
#define IEEE_T_MACERR	0x264 /* Frames tx'd with TX FIFO underrun */
#define IEEE_T_CSERR	0x268 /* Frames tx'd with carrier sense err */
#define IEEE_T_SQE	0x26c /* Frames tx'd with SQE err */
#define IEEE_T_FDXFC	0x270 /* Flow control pause frames tx'd */
#define IEEE_T_OCTETS_OK	0x274 /* Octet count for frames tx'd w/o err */
#define RMON_R_PACKETS	0x284 /* RMON RX packet count */
#define RMON_R_BC_PKT	0x288 /* RMON RX broadcast pkts */
#define RMON_R_MC_PKT	0x28c /* RMON RX multicast pkts */
#define RMON_R_CRC_ALIGN	0x290 /* RMON RX pkts with CRC alignment err */
#define RMON_R_UNDERSIZE	0x294 /* RMON RX pkts < 64 bytes, good CRC */
#define RMON_R_OVERSIZE	0x298 /* RMON RX pkts > MAX_FL bytes good CRC */
#define RMON_R_FRAG	0x29c /* RMON RX pkts < 64 bytes, bad CRC */
#define RMON_R_JAB	0x2a0 /* RMON RX pkts > MAX_FL bytes, bad CRC */
#define RMON_R_RESVD_O	0x2a4 /* Reserved */
#define RMON_R_P64	0x2a8 /* RMON RX 64 byte pkts */
#define RMON_R_P65TO127	0x2ac /* RMON RX 65 to 127 byte pkts */
#define RMON_R_P128TO255	0x2b0 /* RMON RX 128 to 255 byte pkts */
#define RMON_R_P256TO511	0x2b4 /* RMON RX 256 to 511 byte pkts */
#define RMON_R_P512TO1023	0x2b8 /* RMON RX 512 to 1023 byte pkts */
#define RMON_R_P1024TO2047	0x2bc /* RMON RX 1024 to 2047 byte pkts */
#define RMON_R_P_GTE2048	0x2c0 /* RMON RX pkts > 2048 bytes */
#define RMON_R_OCTETS	0x2c4 /* RMON RX octets */
#define IEEE_R_DROP	0x2c8 /* Count frames not counted correctly */
#define IEEE_R_FRAME_OK	0x2cc /* Frames rx'd OK */
#define IEEE_R_CRC	0x2d0 /* Frames rx'd with CRC err */
#define IEEE_R_ALIGN	0x2d4 /* Frames rx'd with alignment err */
#define IEEE_R_MACERR	0x2d8 /* Receive FIFO overflow count */
#define IEEE_R_FDXFC	0x2dc /* Flow control pause frames rx'd */
#define IEEE_R_OCTETS_OK	0x2e0 /* Octet cnt for frames rx'd w/o err */

#define EMAC_SMAC_0_0	0x500 /*Supplemental MAC Address 0 (RW).*/
#define EMAC_SMAC_0_1	0x504 /*Supplemental MAC Address 0 (RW).*/

/* GEMAC definitions and settings */

#define EMAC_PORT_0	0
#define EMAC_PORT_1	1

/* GEMAC Bit definitions */
#define EMAC_IEVENT_HBERR		 0x80000000
#define EMAC_IEVENT_BABR		 0x40000000
#define EMAC_IEVENT_BABT		 0x20000000
#define EMAC_IEVENT_GRA			 0x10000000
#define EMAC_IEVENT_TXF			 0x08000000
#define EMAC_IEVENT_TXB			 0x04000000
#define EMAC_IEVENT_RXF			 0x02000000
#define EMAC_IEVENT_RXB			 0x01000000
#define EMAC_IEVENT_MII			 0x00800000
#define EMAC_IEVENT_EBERR		 0x00400000
#define EMAC_IEVENT_LC			 0x00200000
#define EMAC_IEVENT_RL			 0x00100000
#define EMAC_IEVENT_UN			 0x00080000

#define EMAC_IMASK_HBERR                 0x80000000
#define EMAC_IMASK_BABR                  0x40000000
#define EMAC_IMASKT_BABT                 0x20000000
#define EMAC_IMASK_GRA                   0x10000000
#define EMAC_IMASKT_TXF                  0x08000000
#define EMAC_IMASK_TXB                   0x04000000
#define EMAC_IMASKT_RXF                  0x02000000
#define EMAC_IMASK_RXB                   0x01000000
#define EMAC_IMASK_MII                   0x00800000
#define EMAC_IMASK_EBERR                 0x00400000
#define EMAC_IMASK_LC                    0x00200000
#define EMAC_IMASKT_RL                   0x00100000
#define EMAC_IMASK_UN                    0x00080000

#define EMAC_RCNTRL_MAX_FL_SHIFT         16
#define EMAC_RCNTRL_LOOP                 0x00000001
#define EMAC_RCNTRL_DRT                  0x00000002
#define EMAC_RCNTRL_MII_MODE             0x00000004
#define EMAC_RCNTRL_PROM                 0x00000008
#define EMAC_RCNTRL_BC_REJ               0x00000010
#define EMAC_RCNTRL_FCE                  0x00000020
#define EMAC_RCNTRL_RGMII                0x00000040
#define EMAC_RCNTRL_SGMII                0x00000080
#define EMAC_RCNTRL_RMII                 0x00000100
#define EMAC_RCNTRL_RMII_10T             0x00000200
#define EMAC_RCNTRL_CRC_FWD		 0x00004000

#define EMAC_TCNTRL_GTS                  0x00000001
#define EMAC_TCNTRL_HBC                  0x00000002
#define EMAC_TCNTRL_FDEN                 0x00000004
#define EMAC_TCNTRL_TFC_PAUSE            0x00000008
#define EMAC_TCNTRL_RFC_PAUSE            0x00000010

#define EMAC_ECNTRL_RESET                0x00000001      /* reset the EMAC */
#define EMAC_ECNTRL_ETHER_EN             0x00000002      /* enable the EMAC */
#define EMAC_ECNTRL_MAGIC_ENA		 0x00000004
#define EMAC_ECNTRL_SLEEP		 0x00000008
#define EMAC_ECNTRL_SPEED                0x00000020
#define EMAC_ECNTRL_DBSWAP               0x00000100

#define EMAC_X_WMRK_STRFWD               0x00000100

#define EMAC_X_DES_ACTIVE_TDAR           0x01000000
#define EMAC_R_DES_ACTIVE_RDAR           0x01000000

#define EMAC_RX_SECTION_EMPTY_V		0x00010006
/*
 * The possible operating speeds of the MAC, currently supporting 10, 100 and
 * 1000Mb modes.
 */
enum mac_speed {SPEED_10M, SPEED_100M, SPEED_1000M, SPEED_1000M_PCS};

/* MII-related definitios */
#define EMAC_MII_DATA_ST         0x40000000      /* Start of frame delimiter */
#define EMAC_MII_DATA_OP_RD      0x20000000      /* Perform a read operation */
#define EMAC_MII_DATA_OP_CL45_RD 0x30000000      /* Perform a read operation */
#define EMAC_MII_DATA_OP_WR      0x10000000      /* Perform a write operation */
#define EMAC_MII_DATA_OP_CL45_WR 0x10000000      /* Perform a write operation */
#define EMAC_MII_DATA_PA_MSK     0x0f800000      /* PHY Address field mask */
#define EMAC_MII_DATA_RA_MSK     0x007c0000      /* PHY Register field mask */
#define EMAC_MII_DATA_TA         0x00020000      /* Turnaround */
#define EMAC_MII_DATA_DATAMSK    0x0000ffff      /* PHY data field */

#define EMAC_MII_DATA_RA_SHIFT   18      /* MII Register address bits */
#define EMAC_MII_DATA_RA_MASK	 0x1F      /* MII Register address mask */
#define EMAC_MII_DATA_PA_SHIFT   23      /* MII PHY address bits */
#define EMAC_MII_DATA_PA_MASK    0x1F      /* MII PHY address mask */

#define EMAC_MII_DATA_RA(v) (((v) & EMAC_MII_DATA_RA_MASK) << \
				EMAC_MII_DATA_RA_SHIFT)
#define EMAC_MII_DATA_PA(v) (((v) & EMAC_MII_DATA_RA_MASK) << \
				EMAC_MII_DATA_PA_SHIFT)
#define EMAC_MII_DATA(v)    ((v) & 0xffff)

#define EMAC_MII_SPEED_SHIFT	1
#define EMAC_HOLDTIME_SHIFT	8
#define EMAC_HOLDTIME_MASK	0x7
#define EMAC_HOLDTIME(v)	(((v) & EMAC_HOLDTIME_MASK) << \
					EMAC_HOLDTIME_SHIFT)

/*
 * The Address organisation for the MAC device.  All addresses are split into
 * two 32-bit register fields.  The first one (bottom) is the lower 32-bits of
 * the address and the other field are the high order bits - this may be 16-bits
 * in the case of MAC addresses, or 32-bits for the hash address.
 * In terms of memory storage, the first item (bottom) is assumed to be at a
 * lower address location than 'top'. i.e. top should be at address location of
 * 'bottom' + 4 bytes.
 */
struct pfe_mac_addr {
	u32 bottom;     /* Lower 32-bits of address. */
	u32 top;        /* Upper 32-bits of address. */
};

/*
 * The following is the organisation of the address filters section of the MAC
 * registers.  The Cadence MAC contains four possible specific address match
 * addresses, if an incoming frame corresponds to any one of these four
 * addresses then the frame will be copied to memory.
 * It is not necessary for all four of the address match registers to be
 * programmed, this is application dependent.
 */
struct spec_addr {
	struct pfe_mac_addr one;        /* Specific address register 1. */
	struct pfe_mac_addr two;        /* Specific address register 2. */
	struct pfe_mac_addr three;      /* Specific address register 3. */
	struct pfe_mac_addr four;       /* Specific address register 4. */
};

struct gemac_cfg {
	u32 mode;
	u32 speed;
	u32 duplex;
};

/* EMAC Hash size */
#define EMAC_HASH_REG_BITS       64

#define EMAC_SPEC_ADDR_MAX	4

#endif /* _EMAC_H_ */
