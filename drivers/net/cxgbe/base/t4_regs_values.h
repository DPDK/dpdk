/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef __T4_REGS_VALUES_H__
#define __T4_REGS_VALUES_H__

/*
 * This file contains definitions for various T4 register value hardware
 * constants.  The types of values encoded here are predominantly those for
 * register fields which control "modal" behavior.  For the most part, we do
 * not include definitions for register fields which are simple numeric
 * metrics, etc.
 */

/*
 * SGE definitions.
 * ================
 */

/*
 * SGE register field values.
 */

/* CONTROL register */
#define X_RXPKTCPLMODE_SPLIT		1
#define X_INGPCIEBOUNDARY_32B		0
#define X_INGPADBOUNDARY_SHIFT		5
#define X_INGPADBOUNDARY_32B		0

#define X_T6_INGPADBOUNDARY_SHIFT	3
#define X_T6_INGPADBOUNDARY_8B		0

/* CONTROL2 register */
#define X_INGPACKBOUNDARY_SHIFT		5
#define X_INGPACKBOUNDARY_16B		0
#define X_INGPACKBOUNDARY_64B		1

/* GTS register */
#define X_TIMERREG_RESTART_COUNTER	6
#define X_TIMERREG_UPDATE_CIDX		7

/*
 * Egress Context field values
 */
#define X_FETCHBURSTMIN_64B		2
#define X_FETCHBURSTMIN_128B		3
#define X_FETCHBURSTMAX_256B		2
#define X_FETCHBURSTMAX_512B		3

#define X_HOSTFCMODE_NONE		0

/*
 * Ingress Context field values
 */
#define X_UPDATEDELIVERY_STATUS_PAGE	2

#define X_RSPD_TYPE_FLBUF		0
#define X_RSPD_TYPE_CPL			1

/*
 * Context field definitions.  This is by no means a complete list of SGE
 * Context fields.  In the vast majority of cases the firmware initializes
 * things the way they need to be set up.  But in a few small cases, we need
 * to compute new values and ship them off to the firmware to be applied to
 * the SGE Conexts ...
 */

/*
 * Congestion Manager Definitions.
 */
#define S_CONMCTXT_CNGTPMODE		19
#define M_CONMCTXT_CNGTPMODE		0x3
#define V_CONMCTXT_CNGTPMODE(x)		((x) << S_CONMCTXT_CNGTPMODE)
#define G_CONMCTXT_CNGTPMODE(x)  \
	(((x) >> S_CONMCTXT_CNGTPMODE) & M_CONMCTXT_CNGTPMODE)
#define S_CONMCTXT_CNGCHMAP		0
#define M_CONMCTXT_CNGCHMAP		0xffff
#define V_CONMCTXT_CNGCHMAP(x)		((x) << S_CONMCTXT_CNGCHMAP)
#define G_CONMCTXT_CNGCHMAP(x)   \
	(((x) >> S_CONMCTXT_CNGCHMAP) & M_CONMCTXT_CNGCHMAP)

#define X_CONMCTXT_CNGTPMODE_QUEUE	1
#define X_CONMCTXT_CNGTPMODE_CHANNEL	2

/*
 * T5 and later support a new BAR2-based doorbell mechanism for Egress Queues.
 * The User Doorbells are each 128 bytes in length with a Simple Doorbell at
 * offsets 8x and a Write Combining single 64-byte Egress Queue Unit
 * (X_IDXSIZE_UNIT) Gather Buffer interface at offset 64.  For Ingress Queues,
 * we have a Going To Sleep register at offsets 8x+4.
 *
 * As noted above, we have many instances of the Simple Doorbell and Going To
 * Sleep registers at offsets 8x and 8x+4, respectively.  We want to use a
 * non-64-byte aligned offset for the Simple Doorbell in order to attempt to
 * avoid buffering of the writes to the Simple Doorbell and we want to use a
 * non-contiguous offset for the Going To Sleep writes in order to avoid
 * possible combining between them.
 */
#define SGE_UDB_SIZE		128
#define SGE_UDB_KDOORBELL	8
#define SGE_UDB_GTS		20

/*
 * CIM definitions.
 * ================
 */

/*
 * CIM register field values.
 */
#define X_MBOWNER_NONE			0
#define X_MBOWNER_FW			1
#define X_MBOWNER_PL			2

/*
 * PCI-E definitions.
 * ==================
 */
#define X_WINDOW_SHIFT			10
#define X_PCIEOFST_SHIFT		10

/*
 * TP definitions.
 * ===============
 */

/*
 * TP_VLAN_PRI_MAP controls which subset of fields will be present in the
 * Compressed Filter Tuple for LE filters.  Each bit set in TP_VLAN_PRI_MAP
 * selects for a particular field being present.  These fields, when present
 * in the Compressed Filter Tuple, have the following widths in bits.
 */
#define W_FT_FCOE			1
#define W_FT_PORT			3
#define W_FT_VNIC_ID			17
#define W_FT_VLAN			17
#define W_FT_TOS			8
#define W_FT_PROTOCOL			8
#define W_FT_ETHERTYPE			16
#define W_FT_MACMATCH			9
#define W_FT_MPSHITTYPE			3
#define W_FT_FRAGMENTATION		1

/*
 * Some of the Compressed Filter Tuple fields have internal structure.  These
 * bit shifts/masks describe those structures.  All shifts are relative to the
 * base position of the fields within the Compressed Filter Tuple
 */
#define S_FT_VLAN_VLD			16
#define V_FT_VLAN_VLD(x)		((x) << S_FT_VLAN_VLD)
#define F_FT_VLAN_VLD			V_FT_VLAN_VLD(1U)

#endif /* __T4_REGS_VALUES_H__ */
