/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __OCTEONTX_PKI_VAR_H__
#define __OCTEONTX_PKI_VAR_H__

#include <rte_byteorder.h>

#define OCTTX_PACKET_WQE_SKIP			128
#define OCTTX_PACKET_FIRST_SKIP_MAXREGVAL	496
#define OCTTX_PACKET_FIRST_SKIP_MAXLEN		512
#define OCTTX_PACKET_FIRST_SKIP_ADJUST(x)				\
		(RTE_MIN(x, OCTTX_PACKET_FIRST_SKIP_MAXREGVAL))
#define OCTTX_PACKET_FIRST_SKIP_SUM(p)					\
				(OCTTX_PACKET_WQE_SKIP			\
				+ rte_pktmbuf_priv_size(p)		\
				+ RTE_PKTMBUF_HEADROOM)
#define OCTTX_PACKET_FIRST_SKIP(p)					\
	OCTTX_PACKET_FIRST_SKIP_ADJUST(OCTTX_PACKET_FIRST_SKIP_SUM(p))
#define OCTTX_PACKET_LATER_SKIP		128

/* WQE descriptor */
typedef union octtx_wqe_s {
	uint64_t	w[6];

	struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		struct {
			uint64_t	pknd : 6;
			uint64_t	rsvd0 : 10;
			uint64_t	style : 8;
			uint64_t	bufs : 8;
			uint64_t	chan : 12;
			uint64_t	apad : 3;
			uint64_t	rsvd1 : 1;
			uint64_t	aura : 12;
			uint64_t	rsvd2 : 4;
		} w0;

		struct {
			uint64_t	tag :	32;
			uint64_t	tt :	2;
			uint64_t	grp :	10;
			uint64_t	rsvd0 : 2;
			uint64_t	rsvd1 : 2;
			uint64_t	len :	16;
		} w1;

		struct {
			uint64_t	op_code : 8;
			uint64_t	err_lev : 3;
			uint64_t	raw	: 1;
			uint64_t	l2m	: 1;
			uint64_t	l2b	: 1;
			uint64_t	l3m	: 1;
			uint64_t	l3b	: 1;
			uint64_t	l3fr	: 1;
			uint64_t	pf1	: 1;
			uint64_t	pf2	: 1;
			uint64_t	pf3	: 1;
			uint64_t	pf4	: 1;
			uint64_t	sh	: 1;
			uint64_t	vs	: 1;
			uint64_t	vv	: 1;
			uint64_t	rsvd0	: 8;
			uint64_t	lae	: 1;
			uint64_t	lbty	: 5;
			uint64_t	lcty	: 5;
			uint64_t	ldty	: 5;
			uint64_t	lety	: 5;
			uint64_t	lfty	: 5;
			uint64_t	lgty	: 5;
			uint64_t	sw	: 1;
		} w2;

		struct {
			uint64_t	addr;	/* Byte addr of start-of-pkt */
		} w3;

		struct {
			uint64_t	laptr : 8;
			uint64_t	lbptr : 8;
			uint64_t	lcptr : 8;
			uint64_t	ldprt : 8;
			uint64_t	leptr : 8;
			uint64_t	lfptr : 8;
			uint64_t	lgptr : 8;
			uint64_t	vlptr : 8;
		} w4;

		struct {
			uint64_t	rsvd0 : 47;
			uint64_t	dwd : 1;
			uint64_t	size : 16;
		} w5;
#else
		struct {
			uint64_t	rsvd2 : 4;
			uint64_t	aura : 12;
			uint64_t	rsvd1 : 1;
			uint64_t	apad : 3;
			uint64_t	chan : 12;
			uint64_t	bufs : 8;
			uint64_t	style : 8;
			uint64_t	rsvd0 : 10;
			uint64_t	pknd : 6;
		} w0;

		struct {
			uint64_t	len :   16;
			uint64_t	rsvd1 : 2;
			uint64_t	rsvd0 : 2;
			uint64_t	grp :   10;
			uint64_t	tt :    2;
			uint64_t	tag :   32;
		} w1;

		struct {
			uint64_t	sw	: 1;
			uint64_t	lgty	: 5;
			uint64_t	lfty	: 5;
			uint64_t	lety	: 5;
			uint64_t	ldty	: 5;
			uint64_t	lcty	: 5;
			uint64_t	lbty	: 5;
			uint64_t	lae	: 1;
			uint64_t	rsvd0	: 8;
			uint64_t	vv	: 1;
			uint64_t	vs	: 1;
			uint64_t	sh	: 1;
			uint64_t	pf4	: 1;
			uint64_t	pf3	: 1;
			uint64_t	pf2	: 1;
			uint64_t	pf1	: 1;
			uint64_t	l3fr	: 1;
			uint64_t	l3b	: 1;
			uint64_t	l3m	: 1;
			uint64_t	l2b	: 1;
			uint64_t	l2m	: 1;
			uint64_t	raw	: 1;
	uint64_t	err_lev : 3;
			uint64_t	op_code : 8;
		} w2;

		struct {
			uint64_t	addr;	/* Byte addr of start-of-pkt */
		} w3;

		struct {
			uint64_t	vlptr : 8;
			uint64_t	lgptr : 8;
			uint64_t	lfptr : 8;
			uint64_t	leptr : 8;
			uint64_t	ldprt : 8;
			uint64_t	lcptr : 8;
			uint64_t	lbptr : 8;
			uint64_t	laptr : 8;
		} w4;
#endif
	} s;

} __rte_packed octtx_wqe_t;

enum occtx_pki_ltype_e {
	OCCTX_PKI_LTYPE_NONE		= 0,
	OCCTX_PKI_LTYPE_ENET		= 1,
	OCCTX_PKI_LTYPE_VLAN		= 2,
	OCCTX_PKI_LTYPE_SNAP_PAYLD	= 5,
	OCCTX_PKI_LTYPE_ARP		= 6,
	OCCTX_PKI_LTYPE_RARP		= 7,
	OCCTX_PKI_LTYPE_IP4		= 8,
	OCCTX_PKI_LTYPE_IP4_OPT		= 9,
	OCCTX_PKI_LTYPE_IP6		= 0xa,
	OCCTX_PKI_LTYPE_IP6_OPT		= 0xb,
	OCCTX_PKI_LTYPE_IPSEC_ESP	= 0xc,
	OCCTX_PKI_LTYPE_IPFRAG		= 0xd,
	OCCTX_PKI_LTYPE_IPCOMP		= 0xe,
	OCCTX_PKI_LTYPE_TCP		= 0x10,
	OCCTX_PKI_LTYPE_UDP		= 0x11,
	OCCTX_PKI_LTYPE_SCTP		= 0x12,
	OCCTX_PKI_LTYPE_UDP_VXLAN	= 0x13,
	OCCTX_PKI_LTYPE_GRE		= 0x14,
	OCCTX_PKI_LTYPE_NVGRE		= 0x15,
	OCCTX_PKI_LTYPE_GTP		= 0x16,
	OCCTX_PKI_LTYPE_UDP_GENEVE	= 0x17,
	OCCTX_PKI_LTYPE_SW28		= 0x1c,
	OCCTX_PKI_LTYPE_SW29		= 0x1d,
	OCCTX_PKI_LTYPE_SW30		= 0x1e,
	OCCTX_PKI_LTYPE_SW31		= 0x1f,
	OCCTX_PKI_LTYPE_LAST
};

enum lc_type_e {
	LC_NONE		= OCCTX_PKI_LTYPE_NONE,
	LC_IPV4		= OCCTX_PKI_LTYPE_IP4,
	LC_IPV4_OPT	= OCCTX_PKI_LTYPE_IP4_OPT,
	LC_IPV6		= OCCTX_PKI_LTYPE_IP6,
	LC_IPV6_OPT	= OCCTX_PKI_LTYPE_IP6_OPT,
};

enum le_type_e {
	LE_NONE		= OCCTX_PKI_LTYPE_NONE,
};

enum lf_type_e {
	LF_NONE		= OCCTX_PKI_LTYPE_NONE,
	LF_IPSEC_ESP	= OCCTX_PKI_LTYPE_IPSEC_ESP,
	LF_IPFRAG	= OCCTX_PKI_LTYPE_IPFRAG,
	LF_IPCOMP	= OCCTX_PKI_LTYPE_IPCOMP,
	LF_TCP		= OCCTX_PKI_LTYPE_TCP,
	LF_UDP		= OCCTX_PKI_LTYPE_UDP,
	LF_GRE		= OCCTX_PKI_LTYPE_GRE,
	LF_UDP_GENEVE	= OCCTX_PKI_LTYPE_UDP_GENEVE,
	LF_UDP_VXLAN	= OCCTX_PKI_LTYPE_UDP_VXLAN,
	LF_NVGRE	= OCCTX_PKI_LTYPE_NVGRE,
};

/* Word 0 of HW segment buflink structure */
typedef union octtx_pki_buflink_w0_u {
	uint64_t v;
	struct {
		uint64_t        size:16;
		uint64_t        rsvd1:15;
		uint64_t        invfree:1;
		/** Aura number of the next segment */
		uint64_t        aura:16;
		uint64_t        sw:9;
		uint64_t        later_invfree:1;
		uint64_t        rsvd2:5;
		/** 1 if aura number is set */
		uint64_t        has_aura:1;
	} s;
} octtx_pki_buflink_w0_t;

/* Word 1 of HW segment buflink structure */
typedef union octtx_pki_buflink_w1_u {
	uint64_t v;
	struct {
		uint64_t        addr;
	} s;
} octtx_pki_buflink_w1_t;

/* HW structure linking packet segments into singly linked list */
typedef struct octtx_pki_buflink_s {
	octtx_pki_buflink_w0_t    w0; /* Word 0 of the buflink */
	octtx_pki_buflink_w1_t    w1; /* Word 1 of the buflink */
} octtx_pki_buflink_t;

#endif /* __OCTEONTX_PKI_VAR_H__ */
