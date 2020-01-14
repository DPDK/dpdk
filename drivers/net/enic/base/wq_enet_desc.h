/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _WQ_ENET_DESC_H_
#define _WQ_ENET_DESC_H_

#include <rte_byteorder.h>

/* Ethernet work queue descriptor: 16B */
struct wq_enet_desc {
	__le64 address;
	__le16 length;
	__le16 mss_loopback;
	__le16 header_length_flags;
	__le16 vlan_tag;
};

#define WQ_ENET_ADDR_BITS		64
#define WQ_ENET_LEN_BITS		14
#define WQ_ENET_LEN_MASK		((1 << WQ_ENET_LEN_BITS) - 1)
#define WQ_ENET_MSS_BITS		14
#define WQ_ENET_MSS_MASK		((1 << WQ_ENET_MSS_BITS) - 1)
#define WQ_ENET_MSS_SHIFT		2
#define WQ_ENET_LOOPBACK_SHIFT		1
#define WQ_ENET_HDRLEN_BITS		10
#define WQ_ENET_HDRLEN_MASK		((1 << WQ_ENET_HDRLEN_BITS) - 1)
#define WQ_ENET_FLAGS_OM_BITS		2
#define WQ_ENET_FLAGS_OM_MASK		((1 << WQ_ENET_FLAGS_OM_BITS) - 1)
#define WQ_ENET_FLAGS_EOP_SHIFT		12
#define WQ_ENET_FLAGS_CQ_ENTRY_SHIFT	13
#define WQ_ENET_FLAGS_FCOE_ENCAP_SHIFT	14
#define WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT	15

#define WQ_ENET_OFFLOAD_MODE_CSUM	0
#define WQ_ENET_OFFLOAD_MODE_RESERVED	1
#define WQ_ENET_OFFLOAD_MODE_CSUM_L4	2
#define WQ_ENET_OFFLOAD_MODE_TSO	3

static inline void wq_enet_desc_enc(struct wq_enet_desc *desc,
	u64 address, u16 length, u16 mss, u16 header_length,
	u8 offload_mode, u8 eop, u8 cq_entry, u8 fcoe_encap,
	u8 vlan_tag_insert, u16 vlan_tag, u8 loopback)
{
	desc->address = rte_cpu_to_le_64(address);
	desc->length = rte_cpu_to_le_16(length & WQ_ENET_LEN_MASK);
	desc->mss_loopback = rte_cpu_to_le_16((mss & WQ_ENET_MSS_MASK) <<
		WQ_ENET_MSS_SHIFT | (loopback & 1) << WQ_ENET_LOOPBACK_SHIFT);
	desc->header_length_flags = rte_cpu_to_le_16
		((header_length & WQ_ENET_HDRLEN_MASK) |
		(offload_mode & WQ_ENET_FLAGS_OM_MASK) << WQ_ENET_HDRLEN_BITS |
		(eop & 1) << WQ_ENET_FLAGS_EOP_SHIFT |
		(cq_entry & 1) << WQ_ENET_FLAGS_CQ_ENTRY_SHIFT |
		(fcoe_encap & 1) << WQ_ENET_FLAGS_FCOE_ENCAP_SHIFT |
		(vlan_tag_insert & 1) << WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT);
	desc->vlan_tag = rte_cpu_to_le_16(vlan_tag);
}

static inline void wq_enet_desc_dec(struct wq_enet_desc *desc,
	u64 *address, u16 *length, u16 *mss, u16 *header_length,
	u8 *offload_mode, u8 *eop, u8 *cq_entry, u8 *fcoe_encap,
	u8 *vlan_tag_insert, u16 *vlan_tag, u8 *loopback)
{
	*address = rte_le_to_cpu_64(desc->address);
	*length = rte_le_to_cpu_16(desc->length) & WQ_ENET_LEN_MASK;
	*mss = (rte_le_to_cpu_16(desc->mss_loopback) >> WQ_ENET_MSS_SHIFT) &
		WQ_ENET_MSS_MASK;
	*loopback = (u8)((rte_le_to_cpu_16(desc->mss_loopback) >>
		WQ_ENET_LOOPBACK_SHIFT) & 1);
	*header_length = rte_le_to_cpu_16(desc->header_length_flags) &
		WQ_ENET_HDRLEN_MASK;
	*offload_mode = (u8)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_HDRLEN_BITS) & WQ_ENET_FLAGS_OM_MASK);
	*eop = (u8)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_EOP_SHIFT) & 1);
	*cq_entry = (u8)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_CQ_ENTRY_SHIFT) & 1);
	*fcoe_encap = (u8)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_FCOE_ENCAP_SHIFT) & 1);
	*vlan_tag_insert = (u8)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT) & 1);
	*vlan_tag = rte_le_to_cpu_16(desc->vlan_tag);
}

#endif /* _WQ_ENET_DESC_H_ */
