/*
 * Copyright 2008-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ident "$Id: wq_enet_desc.h 59839 2010-09-27 20:36:31Z roprabhu $"

#ifndef _WQ_ENET_DESC_H_
#define _WQ_ENET_DESC_H_

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
	desc->address = cpu_to_le64(address);
	desc->length = cpu_to_le16(length & WQ_ENET_LEN_MASK);
	desc->mss_loopback = cpu_to_le16((mss & WQ_ENET_MSS_MASK) <<
		WQ_ENET_MSS_SHIFT | (loopback & 1) << WQ_ENET_LOOPBACK_SHIFT);
	desc->header_length_flags = cpu_to_le16(
		(header_length & WQ_ENET_HDRLEN_MASK) |
		(offload_mode & WQ_ENET_FLAGS_OM_MASK) << WQ_ENET_HDRLEN_BITS |
		(eop & 1) << WQ_ENET_FLAGS_EOP_SHIFT |
		(cq_entry & 1) << WQ_ENET_FLAGS_CQ_ENTRY_SHIFT |
		(fcoe_encap & 1) << WQ_ENET_FLAGS_FCOE_ENCAP_SHIFT |
		(vlan_tag_insert & 1) << WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT);
	desc->vlan_tag = cpu_to_le16(vlan_tag);
}

static inline void wq_enet_desc_dec(struct wq_enet_desc *desc,
	u64 *address, u16 *length, u16 *mss, u16 *header_length,
	u8 *offload_mode, u8 *eop, u8 *cq_entry, u8 *fcoe_encap,
	u8 *vlan_tag_insert, u16 *vlan_tag, u8 *loopback)
{
	*address = le64_to_cpu(desc->address);
	*length = le16_to_cpu(desc->length) & WQ_ENET_LEN_MASK;
	*mss = (le16_to_cpu(desc->mss_loopback) >> WQ_ENET_MSS_SHIFT) &
		WQ_ENET_MSS_MASK;
	*loopback = (u8)((le16_to_cpu(desc->mss_loopback) >>
		WQ_ENET_LOOPBACK_SHIFT) & 1);
	*header_length = le16_to_cpu(desc->header_length_flags) &
		WQ_ENET_HDRLEN_MASK;
	*offload_mode = (u8)((le16_to_cpu(desc->header_length_flags) >>
		WQ_ENET_HDRLEN_BITS) & WQ_ENET_FLAGS_OM_MASK);
	*eop = (u8)((le16_to_cpu(desc->header_length_flags) >>
		WQ_ENET_FLAGS_EOP_SHIFT) & 1);
	*cq_entry = (u8)((le16_to_cpu(desc->header_length_flags) >>
		WQ_ENET_FLAGS_CQ_ENTRY_SHIFT) & 1);
	*fcoe_encap = (u8)((le16_to_cpu(desc->header_length_flags) >>
		WQ_ENET_FLAGS_FCOE_ENCAP_SHIFT) & 1);
	*vlan_tag_insert = (u8)((le16_to_cpu(desc->header_length_flags) >>
		WQ_ENET_FLAGS_VLAN_TAG_INSERT_SHIFT) & 1);
	*vlan_tag = le16_to_cpu(desc->vlan_tag);
}

#endif /* _WQ_ENET_DESC_H_ */
