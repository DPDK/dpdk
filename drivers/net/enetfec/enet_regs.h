/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 NXP
 */

#ifndef __ENETFEC_REGS_H
#define __ENETFEC_REGS_H

/* Ethernet receive use control and status of buffer descriptor
 */
#define RX_BD_TR	((ushort)0x0001) /* Truncated */
#define RX_BD_OV	((ushort)0x0002) /* Over-run */
#define RX_BD_CR	((ushort)0x0004) /* CRC or Frame error */
#define RX_BD_SH	((ushort)0x0008) /* Reserved */
#define RX_BD_NO	((ushort)0x0010) /* Rcvd non-octet aligned frame */
#define RX_BD_LG	((ushort)0x0020) /* Rcvd frame length violation */
#define RX_BD_FIRST	((ushort)0x0400) /* Reserved */
#define RX_BD_LAST	((ushort)0x0800) /* last buffer in the frame */
#define RX_BD_INT	0x00800000
#define RX_BD_ICE	0x00000020
#define RX_BD_PCR	0x00000010

/*
 * 0 The next BD in consecutive location
 * 1 The next BD in ENETFECn_RDSR.
 */
#define RX_BD_WRAP	((ushort)0x2000)
#define RX_BD_EMPTY	((ushort)0x8000) /* BD is empty */
#define RX_BD_STATS	((ushort)0x013f) /* All buffer descriptor status bits */

/* Ethernet receive use control and status of enhanced buffer descriptor */
#define BD_ENETFEC_RX_VLAN	0x00000004

#define RX_FLAG_CSUM_EN		(RX_BD_ICE | RX_BD_PCR)
#define RX_FLAG_CSUM_ERR	(RX_BD_ICE | RX_BD_PCR)

/* Ethernet transmit use control and status of buffer descriptor */
#define TX_BD_TC	((ushort)0x0400) /* Transmit CRC */
#define TX_BD_LAST	((ushort)0x0800) /* Last in frame */
#define TX_BD_READY	((ushort)0x8000) /* Data is ready */
#define TX_BD_STATS	((ushort)0x0fff) /* All buffer descriptor status bits */
#define TX_BD_WRAP	((ushort)0x2000)

/* Ethernet transmit use control and status of enhanced buffer descriptor */
#define TX_BD_IINS		0x08000000
#define TX_BD_PINS		0x10000000

#define ENETFEC_RD_START(X)	(((X) == 1) ? ENETFEC_RD_START_1 : \
				(((X) == 2) ? \
				   ENETFEC_RD_START_2 : ENETFEC_RD_START_0))
#define ENETFEC_TD_START(X)	(((X) == 1) ? ENETFEC_TD_START_1 : \
				(((X) == 2) ? \
				   ENETFEC_TD_START_2 : ENETFEC_TD_START_0))
#define ENETFEC_MRB_SIZE(X)	(((X) == 1) ? ENETFEC_MRB_SIZE_1 : \
				(((X) == 2) ? \
				   ENETFEC_MRB_SIZE_2 : ENETFEC_MRB_SIZE_0))

#define ENETFEC_ETHEREN		((uint)0x00000002)
#define ENETFEC_TXC_DLY		((uint)0x00010000)
#define ENETFEC_RXC_DLY		((uint)0x00020000)

/* ENETFEC MAC is in controller */
#define QUIRK_HAS_ENETFEC_MAC	(1 << 0)
/* GBIT supported in controller */
#define QUIRK_GBIT		(1 << 3)
/* Controller support hardware checksum */
#define QUIRK_CSUM		(1 << 5)
/* Controller support hardware vlan */
#define QUIRK_VLAN		(1 << 6)
/* RACC register supported by controller */
#define QUIRK_RACC		(1 << 12)
/* i.MX8 ENETFEC IP version added the feature to generate the delayed TXC or
 * RXC. For its implementation, ENETFEC uses synchronized clocks (250MHz) for
 * generating delay of 2ns.
 */
#define QUIRK_SUPPORT_DELAYED_CLKS	(1 << 18)

#define ENETFEC_EIR	0x004 /* Interrupt event register */
#define ENETFEC_EIMR	0x008 /* Interrupt mask register */
#define ENETFEC_RDAR_0	0x010 /* Receive descriptor active register ring0 */
#define ENETFEC_TDAR_0	0x014 /* Transmit descriptor active register ring0 */
#define ENETFEC_ECR	0x024 /* Ethernet control register */
#define ENETFEC_MSCR	0x044 /* MII speed control register */
#define ENETFEC_MIBC	0x064 /* MIB control and status register */
#define ENETFEC_RCR	0x084 /* Receive control register */
#define ENETFEC_TCR	0x0c4 /* Transmit Control register */
#define ENETFEC_PALR	0x0e4 /* MAC address low 32 bits */
#define ENETFEC_PAUR	0x0e8 /* MAC address high 16 bits */
#define ENETFEC_OPD	0x0ec /* Opcode/Pause duration register */
#define ENETFEC_IAUR	0x118 /* hash table 32 bits high */
#define ENETFEC_IALR	0x11c /* hash table 32 bits low */
#define ENETFEC_GAUR	0x120 /* grp hash table 32 bits high */
#define ENETFEC_GALR	0x124 /* grp hash table 32 bits low */
#define ENETFEC_TFWR	0x144 /* transmit FIFO water_mark */
#define ENETFEC_RACC	0x1c4 /* Receive Accelerator function configuration*/
#define ENETFEC_DMA1CFG	0x1d8 /* DMA class based configuration ring1 */
#define ENETFEC_DMA2CFG	0x1dc /* DMA class based Configuration ring2 */
#define ENETFEC_RDAR_1	0x1e0 /* Rx descriptor active register ring1 */
#define ENETFEC_TDAR_1	0x1e4 /* Tx descriptor active register ring1 */
#define ENETFEC_RDAR_2	0x1e8 /* Rx descriptor active register ring2 */
#define ENETFEC_TDAR_2	0x1ec /* Tx descriptor active register ring2 */
#define ENETFEC_RD_START_1	0x160 /* Receive descriptor ring1 start reg */
#define ENETFEC_TD_START_1	0x164 /* Transmit descriptor ring1 start reg */
#define ENETFEC_MRB_SIZE_1	0x168 /* Max receive buffer size reg ring1 */
#define ENETFEC_RD_START_2	0x16c /* Receive descriptor ring2 start reg */
#define ENETFEC_TD_START_2	0x170 /* Transmit descriptor ring2 start reg */
#define ENETFEC_MRB_SIZE_2	0x174 /* Max receive buffer size reg ring2 */
#define ENETFEC_RD_START_0	0x180 /* Receive descriptor ring0 start reg */
#define ENETFEC_TD_START_0	0x184 /* Transmit descriptor ring0 start reg */
#define ENETFEC_MRB_SIZE_0	0x188 /* Max receive buffer size reg ring0*/
#define ENETFEC_R_FIFO_SFL	0x190 /* Rx FIFO full threshold */
#define ENETFEC_R_FIFO_SEM	0x194 /* Rx FIFO empty threshold */
#define ENETFEC_R_FIFO_AEM	0x198 /* Rx FIFO almost empty threshold */
#define ENETFEC_R_FIFO_AFL	0x19c /* Rx FIFO almost full threshold */
#define ENETFEC_FRAME_TRL	0x1b0 /* Frame truncation length */

#endif /*__ENETFEC_REGS_H */
