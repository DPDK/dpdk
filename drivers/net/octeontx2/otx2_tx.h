/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TX_H__
#define __OTX2_TX_H__

#define NIX_TX_OFFLOAD_NONE		(0)
#define NIX_TX_OFFLOAD_L3_L4_CSUM_F	BIT(0)
#define NIX_TX_OFFLOAD_OL3_OL4_CSUM_F	BIT(1)
#define NIX_TX_OFFLOAD_VLAN_QINQ_F	BIT(2)
#define NIX_TX_OFFLOAD_MBUF_NOFF_F	BIT(3)
#define NIX_TX_OFFLOAD_TSTAMP_F		BIT(4)

/* Flags to control xmit_prepare function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_TX_MULTI_SEG_F		BIT(15)

#define NIX_TX_NEED_SEND_HDR_W1	\
	(NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |	\
	 NIX_TX_OFFLOAD_VLAN_QINQ_F)

#define NIX_TX_NEED_EXT_HDR \
	(NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSTAMP_F)

#endif /* __OTX2_TX_H__ */
