/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017 NXP
 *
 */

#ifndef __DPDK_RXTX_H__
#define __DPDK_RXTX_H__

/* internal offset from where IC is copied to packet buffer*/
#define DEFAULT_ICIOF          32
/* IC transfer size */
#define DEFAULT_ICSZ	48

/* IC offsets from buffer header address */
#define DEFAULT_RX_ICEOF	16
#define DEFAULT_TX_ICEOF	16

/*
 * Values for the L3R field of the FM Parse Results
 */
/* L3 Type field: First IP Present IPv4 */
#define DPAA_L3_PARSE_RESULT_IPV4 0x80
/* L3 Type field: First IP Present IPv6 */
#define DPAA_L3_PARSE_RESULT_IPV6	0x40
/* Values for the L4R field of the FM Parse Results
 * See $8.8.4.7.20 - L4 HXS - L4 Results from DPAA-Rev2 Reference Manual.
 */
/* L4 Type field: UDP */
#define DPAA_L4_PARSE_RESULT_UDP	0x40
/* L4 Type field: TCP */
#define DPAA_L4_PARSE_RESULT_TCP	0x20

#define DPAA_SGT_MAX_ENTRIES 16 /* maximum number of entries in SG Table */

#define DPAA_MAX_DEQUEUE_NUM_FRAMES    63
	/** <Maximum number of frames to be dequeued in a single rx call*/

/* FD structure masks and offset */
#define DPAA_FD_FORMAT_MASK 0xE0000000
#define DPAA_FD_OFFSET_MASK 0x1FF00000
#define DPAA_FD_LENGTH_MASK 0xFFFFF
#define DPAA_FD_FORMAT_SHIFT 29
#define DPAA_FD_OFFSET_SHIFT 20

/* Parsing mask (Little Endian) - 0x00E044ED00800000
 *	Classification Plan ID 0x00
 *	L4R 0xE0 -
 *		0x20 - TCP
 *		0x40 - UDP
 *		0x80 - SCTP
 *	L3R 0xEDC4 (in Big Endian) -
 *		0x8000 - IPv4
 *		0x4000 - IPv6
 *		0x8140 - IPv4 Ext + Frag
 *		0x8040 - IPv4 Frag
 *		0x8100 - IPv4 Ext
 *		0x4140 - IPv6 Ext + Frag
 *		0x4040 - IPv6 Frag
 *		0x4100 - IPv6 Ext
 *	L2R 0x8000 (in Big Endian) -
 *		0x8000 - Ethernet type
 *	ShimR & Logical Port ID 0x0000
 */
#define DPAA_PARSE_MASK			0x00E044ED00800000
#define DPAA_PARSE_VLAN_MASK		0x0000000000700000

/* Parsed values (Little Endian) */
#define DPAA_PKT_TYPE_NONE		0x0000000000000000
#define DPAA_PKT_TYPE_ETHER		0x0000000000800000
#define DPAA_PKT_TYPE_IPV4 \
			(0x0000008000000000 | DPAA_PKT_TYPE_ETHER)
#define DPAA_PKT_TYPE_IPV6 \
			(0x0000004000000000 | DPAA_PKT_TYPE_ETHER)
#define DPAA_PKT_TYPE_GRE \
			(0x0000002000000000 | DPAA_PKT_TYPE_ETHER)
#define DPAA_PKT_TYPE_IPV4_FRAG	\
			(0x0000400000000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_IPV6_FRAG	\
			(0x0000400000000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_IPV4_EXT \
			(0x0000000100000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_IPV6_EXT \
			(0x0000000100000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_IPV4_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_IPV6_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_IPV4_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_IPV6_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_IPV4_SCTP	\
			(0x0080000000000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_IPV6_SCTP	\
			(0x0080000000000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_IPV4_FRAG_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_IPV4_FRAG)
#define DPAA_PKT_TYPE_IPV6_FRAG_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_IPV6_FRAG)
#define DPAA_PKT_TYPE_IPV4_FRAG_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_IPV4_FRAG)
#define DPAA_PKT_TYPE_IPV6_FRAG_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_IPV6_FRAG)
#define DPAA_PKT_TYPE_IPV4_FRAG_SCTP \
			(0x0080000000000000 | DPAA_PKT_TYPE_IPV4_FRAG)
#define DPAA_PKT_TYPE_IPV6_FRAG_SCTP \
			(0x0080000000000000 | DPAA_PKT_TYPE_IPV6_FRAG)
#define DPAA_PKT_TYPE_IPV4_EXT_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_IPV4_EXT)
#define DPAA_PKT_TYPE_IPV6_EXT_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_IPV6_EXT)
#define DPAA_PKT_TYPE_IPV4_EXT_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_IPV4_EXT)
#define DPAA_PKT_TYPE_IPV6_EXT_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_IPV6_EXT)
#define DPAA_PKT_TYPE_TUNNEL_4_4 \
			(0x0000000800000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_TUNNEL_6_6 \
			(0x0000000400000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_TUNNEL_4_6 \
			(0x0000000400000000 | DPAA_PKT_TYPE_IPV4)
#define DPAA_PKT_TYPE_TUNNEL_6_4 \
			(0x0000000800000000 | DPAA_PKT_TYPE_IPV6)
#define DPAA_PKT_TYPE_TUNNEL_4_4_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_TUNNEL_4_4)
#define DPAA_PKT_TYPE_TUNNEL_6_6_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_TUNNEL_6_6)
#define DPAA_PKT_TYPE_TUNNEL_4_6_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_TUNNEL_4_6)
#define DPAA_PKT_TYPE_TUNNEL_6_4_UDP \
			(0x0040000000000000 | DPAA_PKT_TYPE_TUNNEL_6_4)
#define DPAA_PKT_TYPE_TUNNEL_4_4_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_TUNNEL_4_4)
#define DPAA_PKT_TYPE_TUNNEL_6_6_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_TUNNEL_6_6)
#define DPAA_PKT_TYPE_TUNNEL_4_6_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_TUNNEL_4_6)
#define DPAA_PKT_TYPE_TUNNEL_6_4_TCP \
			(0x0020000000000000 | DPAA_PKT_TYPE_TUNNEL_6_4)
#define DPAA_PKT_L3_LEN_SHIFT	7

/**
 * FMan parse result array
 */
struct dpaa_eth_parse_results_t {
	 uint8_t     lpid;		 /**< Logical port id */
	 uint8_t     shimr;		 /**< Shim header result  */
	 union {
		uint16_t              l2r;	/**< Layer 2 result */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			uint16_t      ethernet:1;
			uint16_t      vlan:1;
			uint16_t      llc_snap:1;
			uint16_t      mpls:1;
			uint16_t      ppoe_ppp:1;
			uint16_t      unused_1:3;
			uint16_t      unknown_eth_proto:1;
			uint16_t      eth_frame_type:2;
			uint16_t      l2r_err:5;
			/*00-unicast, 01-multicast, 11-broadcast*/
#else
			uint16_t      l2r_err:5;
			uint16_t      eth_frame_type:2;
			uint16_t      unknown_eth_proto:1;
			uint16_t      unused_1:3;
			uint16_t      ppoe_ppp:1;
			uint16_t      mpls:1;
			uint16_t      llc_snap:1;
			uint16_t      vlan:1;
			uint16_t      ethernet:1;
#endif
		} __attribute__((__packed__));
	 } __attribute__((__packed__));
	 union {
		uint16_t              l3r;	/**< Layer 3 result */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			uint16_t      first_ipv4:1;
			uint16_t      first_ipv6:1;
			uint16_t      gre:1;
			uint16_t      min_enc:1;
			uint16_t      last_ipv4:1;
			uint16_t      last_ipv6:1;
			uint16_t      first_info_err:1;/*0 info, 1 error*/
			uint16_t      first_ip_err_code:5;
			uint16_t      last_info_err:1;	/*0 info, 1 error*/
			uint16_t      last_ip_err_code:3;
#else
			uint16_t      last_ip_err_code:3;
			uint16_t      last_info_err:1;	/*0 info, 1 error*/
			uint16_t      first_ip_err_code:5;
			uint16_t      first_info_err:1;/*0 info, 1 error*/
			uint16_t      last_ipv6:1;
			uint16_t      last_ipv4:1;
			uint16_t      min_enc:1;
			uint16_t      gre:1;
			uint16_t      first_ipv6:1;
			uint16_t      first_ipv4:1;
#endif
		} __attribute__((__packed__));
	 } __attribute__((__packed__));
	 union {
		uint8_t               l4r;	/**< Layer 4 result */
		struct{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			uint8_t	       l4_type:3;
			uint8_t	       l4_info_err:1;
			uint8_t	       l4_result:4;
					/* if type IPSec: 1 ESP, 2 AH */
#else
			uint8_t        l4_result:4;
					/* if type IPSec: 1 ESP, 2 AH */
			uint8_t        l4_info_err:1;
			uint8_t        l4_type:3;
#endif
		} __attribute__((__packed__));
	 } __attribute__((__packed__));
	 uint8_t     cplan;		 /**< Classification plan id */
	 uint16_t    nxthdr;		 /**< Next Header  */
	 uint16_t    cksum;		 /**< Checksum */
	 uint32_t    lcv;		 /**< LCV */
	 uint8_t     shim_off[3];	 /**< Shim offset */
	 uint8_t     eth_off;		 /**< ETH offset */
	 uint8_t     llc_snap_off;	 /**< LLC_SNAP offset */
	 uint8_t     vlan_off[2];	 /**< VLAN offset */
	 uint8_t     etype_off;		 /**< ETYPE offset */
	 uint8_t     pppoe_off;		 /**< PPP offset */
	 uint8_t     mpls_off[2];	 /**< MPLS offset */
	 uint8_t     ip_off[2];		 /**< IP offset */
	 uint8_t     gre_off;		 /**< GRE offset */
	 uint8_t     l4_off;		 /**< Layer 4 offset */
	 uint8_t     nxthdr_off;	 /**< Parser end point */
} __attribute__ ((__packed__));

/* The structure is the Prepended Data to the Frame which is used by FMAN */
struct annotations_t {
	uint8_t reserved[DEFAULT_RX_ICEOF];
	struct dpaa_eth_parse_results_t parse;	/**< Pointer to Parsed result*/
	uint64_t reserved1;
	uint64_t hash;			/**< Hash Result */
};

#define GET_ANNOTATIONS(_buf) \
	(struct annotations_t *)(_buf)

#define GET_RX_PRS(_buf) \
	(struct dpaa_eth_parse_results_t *)((uint8_t *)(_buf) + \
	DEFAULT_RX_ICEOF)

#define GET_TX_PRS(_buf) \
	(struct dpaa_eth_parse_results_t *)((uint8_t *)(_buf) + \
	DEFAULT_TX_ICEOF)

uint16_t dpaa_eth_queue_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs);

uint16_t dpaa_eth_queue_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs);

uint16_t dpaa_eth_tx_drop_all(void *q  __rte_unused,
			      struct rte_mbuf **bufs __rte_unused,
			      uint16_t nb_bufs __rte_unused);

struct rte_mbuf *dpaa_eth_sg_to_mbuf(const struct qm_fd *fd, uint32_t ifid);

int dpaa_eth_mbuf_to_sg_fd(struct rte_mbuf *mbuf,
			   struct qm_fd *fd,
			   uint32_t bpid);

void dpaa_rx_cb(struct qman_fq **fq,
		struct qm_dqrr_entry **dqrr, void **bufs, int num_bufs);

void dpaa_rx_cb_prepare(struct qm_dqrr_entry *dq, void **bufs);
#endif
