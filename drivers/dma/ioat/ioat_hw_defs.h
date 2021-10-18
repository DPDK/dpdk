/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef IOAT_HW_DEFS_H
#define IOAT_HW_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define IOAT_PCI_CHANERR_INT_OFFSET	0x180

#define IOAT_VER_3_0	0x30
#define IOAT_VER_3_3	0x33

#define IOAT_VENDOR_ID		0x8086
#define IOAT_DEVICE_ID_SKX	0x2021
#define IOAT_DEVICE_ID_BDX0	0x6f20
#define IOAT_DEVICE_ID_BDX1	0x6f21
#define IOAT_DEVICE_ID_BDX2	0x6f22
#define IOAT_DEVICE_ID_BDX3	0x6f23
#define IOAT_DEVICE_ID_BDX4	0x6f24
#define IOAT_DEVICE_ID_BDX5	0x6f25
#define IOAT_DEVICE_ID_BDX6	0x6f26
#define IOAT_DEVICE_ID_BDX7	0x6f27
#define IOAT_DEVICE_ID_BDXE	0x6f2E
#define IOAT_DEVICE_ID_BDXF	0x6f2F
#define IOAT_DEVICE_ID_ICX	0x0b00

#define IOAT_COMP_UPDATE_SHIFT	3
#define IOAT_CMD_OP_SHIFT	24

/* DMA Channel Registers */
#define IOAT_CHANCTRL_CHANNEL_PRIORITY_MASK		0xF000
#define IOAT_CHANCTRL_COMPL_DCA_EN			0x0200
#define IOAT_CHANCTRL_CHANNEL_IN_USE			0x0100
#define IOAT_CHANCTRL_DESCRIPTOR_ADDR_SNOOP_CONTROL	0x0020
#define IOAT_CHANCTRL_ERR_INT_EN			0x0010
#define IOAT_CHANCTRL_ANY_ERR_ABORT_EN			0x0008
#define IOAT_CHANCTRL_ERR_COMPLETION_EN			0x0004
#define IOAT_CHANCTRL_INT_REARM				0x0001

struct ioat_registers {
	uint8_t		chancnt;
	uint8_t		xfercap;
	uint8_t		genctrl;
	uint8_t		intrctrl;
	uint32_t	attnstatus;
	uint8_t		cbver;		/* 0x08 */
	uint8_t		reserved4[0x3]; /* 0x09 */
	uint16_t	intrdelay;	/* 0x0C */
	uint16_t	cs_status;	/* 0x0E */
	uint32_t	dmacapability;	/* 0x10 */
	uint8_t		reserved5[0x6C]; /* 0x14 */
	uint16_t	chanctrl;	/* 0x80 */
	uint8_t		reserved6[0x2];	/* 0x82 */
	uint8_t		chancmd;	/* 0x84 */
	uint8_t		reserved3[1];	/* 0x85 */
	uint16_t	dmacount;	/* 0x86 */
	uint64_t	chansts;	/* 0x88 */
	uint64_t	chainaddr;	/* 0x90 */
	uint64_t	chancmp;	/* 0x98 */
	uint8_t		reserved2[0x8];	/* 0xA0 */
	uint32_t	chanerr;	/* 0xA8 */
	uint32_t	chanerrmask;	/* 0xAC */
} __rte_packed;

#define IOAT_CHANCMD_RESET	0x20
#define IOAT_CHANCMD_SUSPEND	0x04

#define IOAT_CHANCMP_ALIGN	8 /* CHANCMP address must be 64-bit aligned */

#ifdef __cplusplus
}
#endif

#endif /* IOAT_HW_DEFS_H */
