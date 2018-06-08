/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_FILTER_H_
#define _CXGBE_FILTER_H_

#include "t4_msg.h"
/*
 * Defined bit width of user definable filter tuples
 */
#define ETHTYPE_BITWIDTH 16
#define FRAG_BITWIDTH 1
#define MACIDX_BITWIDTH 9
#define FCOE_BITWIDTH 1
#define IPORT_BITWIDTH 3
#define MATCHTYPE_BITWIDTH 3
#define PROTO_BITWIDTH 8
#define TOS_BITWIDTH 8
#define PF_BITWIDTH 8
#define VF_BITWIDTH 8
#define IVLAN_BITWIDTH 16
#define OVLAN_BITWIDTH 16

/*
 * Filter matching rules.  These consist of a set of ingress packet field
 * (value, mask) tuples.  The associated ingress packet field matches the
 * tuple when ((field & mask) == value).  (Thus a wildcard "don't care" field
 * rule can be constructed by specifying a tuple of (0, 0).)  A filter rule
 * matches an ingress packet when all of the individual individual field
 * matching rules are true.
 *
 * Partial field masks are always valid, however, while it may be easy to
 * understand their meanings for some fields (e.g. IP address to match a
 * subnet), for others making sensible partial masks is less intuitive (e.g.
 * MPS match type) ...
 */
struct ch_filter_tuple {
	/*
	 * Compressed header matching field rules.  The TP_VLAN_PRI_MAP
	 * register selects which of these fields will participate in the
	 * filter match rules -- up to a maximum of 36 bits.  Because
	 * TP_VLAN_PRI_MAP is a global register, all filters must use the same
	 * set of fields.
	 */
	uint32_t ethtype:ETHTYPE_BITWIDTH;	/* Ethernet type */
	uint32_t frag:FRAG_BITWIDTH;		/* IP fragmentation header */
	uint32_t ivlan_vld:1;			/* inner VLAN valid */
	uint32_t ovlan_vld:1;			/* outer VLAN valid */
	uint32_t pfvf_vld:1;			/* PF/VF valid */
	uint32_t macidx:MACIDX_BITWIDTH;	/* exact match MAC index */
	uint32_t fcoe:FCOE_BITWIDTH;		/* FCoE packet */
	uint32_t iport:IPORT_BITWIDTH;		/* ingress port */
	uint32_t matchtype:MATCHTYPE_BITWIDTH;	/* MPS match type */
	uint32_t proto:PROTO_BITWIDTH;		/* protocol type */
	uint32_t tos:TOS_BITWIDTH;		/* TOS/Traffic Type */
	uint32_t pf:PF_BITWIDTH;		/* PCI-E PF ID */
	uint32_t vf:VF_BITWIDTH;		/* PCI-E VF ID */
	uint32_t ivlan:IVLAN_BITWIDTH;		/* inner VLAN */
	uint32_t ovlan:OVLAN_BITWIDTH;		/* outer VLAN */

	/*
	 * Uncompressed header matching field rules.  These are always
	 * available for field rules.
	 */
	uint8_t lip[16];	/* local IP address (IPv4 in [3:0]) */
	uint8_t fip[16];	/* foreign IP address (IPv4 in [3:0]) */
	uint16_t lport;		/* local port */
	uint16_t fport;		/* foreign port */

	/* reservations for future additions */
	uint8_t rsvd[12];
};

/*
 * Filter specification
 */
struct ch_filter_specification {
	/* Filter rule value/mask pairs. */
	struct ch_filter_tuple val;
	struct ch_filter_tuple mask;
};

/*
 * Host shadow copy of ingress filter entry.  This is in host native format
 * and doesn't match the ordering or bit order, etc. of the hardware or the
 * firmware command.
 */
struct filter_entry {
	/*
	 * The filter itself.
	 */
	struct ch_filter_specification fs;
};

#endif /* _CXGBE_FILTER_H_ */
