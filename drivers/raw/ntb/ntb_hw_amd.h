/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Advanced Micro Devices, Inc.
 */

#ifndef _NTB_HW_AMD_H_
#define _NTB_HW_AMD_H_

#include <stdint.h>

/* NTB vendor and device IDs (EPYC Embedded Turin/Genoa/Siena). */
#define NTB_AMD_VENDOR_ID		0x1022
#define NTB_AMD_DEV_ID_PRI		0x14c0	/* Primary NTB endpoint. */
#define NTB_AMD_DEV_ID_SEC		0x14c3	/* Secondary NTB endpoint. */

/* Device data (spec Table 12). */
#define AMD_MW_COUNT			2
#define AMD_DB_COUNT			16
#define AMD_SPAD_COUNT			16
#define AMD_MSIX_VECTOR_COUNT		24

/* Peer register window: peer register = primary offset + 0x400. */
#define AMD_PEER_OFFSET			0x400

/* Peer register window base, derived from the local BAR0 mapping. */
#define AMD_PEER_MMIO(ntb)		RTE_PTR_ADD((ntb)->hw_addr, AMD_PEER_OFFSET)

/* Primary-side MMIO register offsets (BAR0), spec Table 1. */
#define AMD_CNTL_OFFSET			0x200	/* Link Control. */
#define AMD_SPADMUTEX_OFFSET		0x20C	/* Scratchpad mutex. */
#define AMD_SPAD_OFFSET			0x210	/* Scratchpad registers. */
#define AMD_SIDEINFO_OFFSET		0x408	/* Link Status (PSIDE_INFO). */
#define AMD_BAR23_LIMIT_OFFSET		0x418	/* MW BAR23 size. */
#define AMD_BAR45_LIMIT_OFFSET		0x420	/* MW BAR45 size. */
#define AMD_BAR23_XLAT_OFFSET		0x438	/* MW BAR23 translation. */
#define AMD_BAR45_XLAT_OFFSET		0x440	/* MW BAR45 translation. */
#define AMD_DBFM_OFFSET			0x450	/* Doorbell flush mode. */
#define AMD_DBREQ_OFFSET		0x454	/* Doorbell request. */
#define AMD_DBMASK_OFFSET		0x45C	/* Doorbell mask. */
#define AMD_DBSTAT_OFFSET		0x460	/* Doorbell status. */
#define AMD_INTMASK_OFFSET		0x470	/* Interrupt mask. */
#define AMD_INTSTAT_OFFSET		0x474	/* Interrupt status. */
#define AMD_PMESTAT_OFFSET		0x480	/* PME status. */
#define AMD_SMUACK_OFFSET		0x4A0	/* SMU control (PSMU_ACK). */

/* Link Status Register (0x408) bits, spec Table 5. */
#define AMD_SIDE_MASK			(1 << 0) /* 0: primary, 1: secondary. */
#define AMD_SIDE_READY			(1 << 1) /* Side link ready. */

/* Link Control Register (0x200) bits, spec Table 2. */
#define AMD_SMM_REG_CTL			(1 << 20)
#define AMD_PMM_REG_CTL			(1 << 21)

/* Interrupt Status/Mask event bits, spec Table 7. */
#define AMD_PEER_FLUSH_EVENT		(1 << 0)
#define AMD_PEER_RESET_EVENT		(1 << 1)
#define AMD_PEER_D3_EVENT		(1 << 2)
#define AMD_PEER_PMETO_EVENT		(1 << 3)
#define AMD_PEER_D0_EVENT		(1 << 4)
#define AMD_LINK_UP_EVENT		(1 << 5)
#define AMD_LINK_DOWN_EVENT		(1 << 6)
#define AMD_EVENT_INTMASK		(AMD_PEER_FLUSH_EVENT | \
					 AMD_PEER_RESET_EVENT | \
					 AMD_PEER_D3_EVENT | \
					 AMD_PEER_PMETO_EVENT | \
					 AMD_PEER_D0_EVENT | \
					 AMD_LINK_UP_EVENT | \
					 AMD_LINK_DOWN_EVENT)

/* PCIe link status decoding (link speed/width). */
#define AMD_LNK_STA_SPEED_MASK		0x000f
#define AMD_LNK_STA_WIDTH_MASK		0x03f0
#define AMD_LNK_STA_SPEED(x)		((x) & AMD_LNK_STA_SPEED_MASK)
#define AMD_LNK_STA_WIDTH(x)		(((x) & AMD_LNK_STA_WIDTH_MASK) >> 4)

/* The 16-register scratchpad bank is shared between both sides (there is no
 * separate peer-scratchpad window). It is split into two disjoint 8-register
 * sets so each side owns one half (offset 0 for one side, 0x20 for the other)
 * and no mutex is required. Because only 8 registers are available per side,
 * the driver uses its own packed handshake layout instead of the built-in
 * protocol.
 */
#define AMD_SPAD_SET_OFFSET		0x20
#define AMD_SPAD_PER_SIDE		(AMD_SPAD_COUNT >> 1)

/* Byte offset of the scratchpad set owned by the local side (@p peer false)
 * or the peer side (@p peer true). The primary owns offset 0, the secondary
 * owns offset AMD_SPAD_SET_OFFSET.
 */
#define AMD_SPAD_BASE(ntb, peer) \
	((((ntb)->topo == NTB_TOPO_PRI) == !!(peer)) ? AMD_SPAD_SET_OFFSET : 0)

/* Packed scratchpad handshake layout (indices within an 8-register set).
 * Indices 6 and 7 are reserved for application user scratchpads.
 */
enum amd_spad_idx {
	AMD_SPAD_CNT_INFO = 0,	/* num_mws | num_qps<<8 | used_mws<<16 */
	AMD_SPAD_QUEUE_SZ,	/* queue_size */
	AMD_SPAD_MW0_BA_L,	/* mw0 base address, low 32 bits */
	AMD_SPAD_MW0_BA_H,	/* mw0 base address, high 32 bits */
	AMD_SPAD_MW1_BA_L,	/* mw1 base address, low 32 bits */
	AMD_SPAD_MW1_BA_H,	/* mw1 base address, high 32 bits */
};

enum amd_ntb_bar {
	AMD_NTB_BAR23 = 2,
	AMD_NTB_BAR45 = 4,
};

extern const struct ntb_dev_ops amd_ntb_ops;

#endif /* _NTB_HW_AMD_H_ */
