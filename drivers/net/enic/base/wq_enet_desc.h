/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _WQ_ENET_DESC_H_
#define _WQ_ENET_DESC_H_

#include <rte_byteorder.h>

/* Ethernet work queue descriptor: 16B */
struct wq_enet_desc {
	uint64_t address;
	uint16_t length;
	uint16_t mss_loopback;
	uint16_t header_length_flags;
	uint16_t vlan_tag;
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
	uint64_t address, uint16_t length, uint16_t mss, uint16_t header_length,
	uint8_t offload_mode, uint8_t eop, uint8_t cq_entry, uint8_t fcoe_encap,
	uint8_t vlan_tag_insert, uint16_t vlan_tag, uint8_t loopback)
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
	uint64_t *address, uint16_t *length, uint16_t *mss,
	uint16_t *header_length, uint8_t *offload_mode, uint8_t *eop,
	uint8_t *cq_entry, uint8_t *fcoe_encap, uint8_t *vlan_tag_insert,
	uint16_t *vlan_tag, uint8_t *loopback)
{
	*address = rte_le_to_cpu_64(desc->address);
	*length = rte_le_to_cpu_16(desc->length) & WQ_ENET_LEN_MASK;
	*mss = (rte_le_to_cpu_16(desc->mss_loopback) >> WQ_ENET_MSS_SHIFT) &
		WQ_ENET_MSS_MASK;
	*loopback = (uint8_t)((rte_le_to_cpu_16(desc->mss_loopback) >>
		WQ_ENET_LOOPBACK_SHIFT) & 1);
	*header_length = rte_le_to_cpu_16(desc->header_length_flags) &
		WQ_ENET_HDRLEN_MASK;
	*offload_mode =
		(uint8_t)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_HDRLEN_BITS) & WQ_ENET_FLAGS_OM_MASK);
	*eop = (uint8_t)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_EOP_SHIFT) & 1);
	*cq_entry = (uint8_t)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_CQ_ENTRY_SHIFT) & 1);
	*fcoe_encap = (uint8_t)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_FCOE_ENCAP_SHIFT) & 1);
	*vlan_tag_insert =
		(uint8_t)((rte_le_to_cpu_16(desc->header_length_flags) >>
		WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT) & 1);
	*vlan_tag = rte_le_to_cpu_16(desc->vlan_tag);
}

#endif /* _WQ_ENET_DESC_H_ */
