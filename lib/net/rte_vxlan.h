/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Intel Corporation
 */

#ifndef _RTE_VXLAN_H_
#define _RTE_VXLAN_H_

/**
 * @file
 *
 * VXLAN-related definitions
 */

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_udp.h>

/** VXLAN default port. */
#define RTE_VXLAN_DEFAULT_PORT 4789
/** VXLAN GPE port. */
#define RTE_VXLAN_GPE_DEFAULT_PORT 4790

/**
 * VXLAN protocol header.
 * Contains the 8-bit flag, 24-bit VXLAN Network Identifier and
 * Reserved fields (24 bits and 8 bits)
 */
__extension__ /* no named member in struct */
struct __rte_packed_begin rte_vxlan_hdr {
	union {
		rte_be32_t vx_flags; /**< flags (8 bits) + extensions (24 bits). */
		struct __rte_packed_begin {
			union {
				uint8_t flags; /**< Default is I bit, others are extensions. */
				struct __rte_packed_begin {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
					uint8_t flag_g:1,     /**< GBP bit. */
						flag_rsvd:1,  /*   Reserved. */
						flag_ver:2,   /**< GPE Protocol Version. */
						flag_i:1,     /**< VNI bit. */
						flag_p:1,     /**< GPE Next Protocol bit. */
						flag_b:1,     /**< GPE Ingress-Replicated BUM. */
						flag_o:1;     /**< GPE OAM Packet bit. */
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
					uint8_t flag_o:1,
						flag_b:1,
						flag_p:1,
						flag_i:1,
						flag_ver:2,
						flag_rsvd:1,
						flag_g:1;
#endif
				} __rte_packed_end;
			}; /* end of 1st byte */
			union {
				uint8_t rsvd0[3]; /* Reserved for extensions. */
				struct __rte_packed_begin {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
					uint8_t rsvd0_gbp1:1, /*   Reserved. */
						flag_d:1,     /**< GBP Don't Learn bit. */
						rsvd0_gbp2:2, /*   Reserved. */
						flag_a:1,     /**< GBP Applied bit. */
						rsvd0_gbp3:3; /*   Reserved. */
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
					uint8_t rsvd0_gbp3:3,
						flag_a:1,
						rsvd0_gbp2:2,
						flag_d:1,
						rsvd0_gbp1:1;
#endif
					union {
						uint16_t policy_id; /**< GBP Identifier. */
						struct __rte_packed_begin {
							uint8_t rsvd0_gpe; /* Reserved. */
							uint8_t proto; /**< GPE Next protocol. */
								/* 0x01 : IPv4
								 * 0x02 : IPv6
								 * 0x03 : Ethernet
								 * 0x04 : Network Service Header
								 */
						} __rte_packed_end;
					};
				} __rte_packed_end;
			};
		} __rte_packed_end;
	}; /* end of 1st 32-bit word */
	union {
		rte_be32_t vx_vni; /**< VNI (24 bits) + reserved (8 bits). */
		struct __rte_packed_begin {
			uint8_t    vni[3];   /**< VXLAN Identifier. */
			union {
				uint8_t    rsvd1;        /**< Reserved. */
				uint8_t    last_rsvd;    /**< Reserved. */
			};
		} __rte_packed_end;
	}; /* end of 2nd 32-bit word */
} __rte_packed_end;

/** VXLAN tunnel header length. */
#define RTE_ETHER_VXLAN_HLEN \
	(sizeof(struct rte_udp_hdr) + sizeof(struct rte_vxlan_hdr))


/**
 * @deprecated
 * @see rte_vxlan_hdr
 *
 * VXLAN-GPE protocol header (draft-ietf-nvo3-vxlan-gpe-05).
 * Contains the 8-bit flag, 8-bit next-protocol, 24-bit VXLAN Network
 * Identifier and Reserved fields (16 bits and 8 bits).
 */
__extension__ /* no named member in struct */
struct __rte_packed_begin rte_vxlan_gpe_hdr {
	union {
		struct {
			uint8_t vx_flags;    /**< flag (8). */
			uint8_t reserved[2]; /**< Reserved (16). */
			uint8_t protocol;    /**< next-protocol (8). */
			rte_be32_t vx_vni;   /**< VNI (24) + Reserved (8). */
		};
		struct {
			uint8_t flags;    /**< Flags. */
			uint8_t rsvd0[2]; /**< Reserved. */
			uint8_t proto;    /**< Next protocol. */
			uint8_t vni[3];   /**< VXLAN identifier. */
			uint8_t rsvd1;    /**< Reserved. */
		};
	};
} __rte_packed_end;

/**
 * @deprecated
 * @see RTE_ETHER_VXLAN_HLEN
 *
 * VXLAN-GPE tunnel header length.
 */
#define RTE_ETHER_VXLAN_GPE_HLEN (sizeof(struct rte_udp_hdr) + \
		sizeof(struct rte_vxlan_gpe_hdr))

/* VXLAN-GPE next protocol types */
#define RTE_VXLAN_GPE_TYPE_IPV4 1 /**< IPv4 Protocol. */
#define RTE_VXLAN_GPE_TYPE_IPV6 2 /**< IPv6 Protocol. */
#define RTE_VXLAN_GPE_TYPE_ETH  3 /**< Ethernet Protocol. */
#define RTE_VXLAN_GPE_TYPE_NSH  4 /**< NSH Protocol. */
#define RTE_VXLAN_GPE_TYPE_MPLS 5 /**< MPLS Protocol. */
#define RTE_VXLAN_GPE_TYPE_GBP  6 /**< GBP Protocol. */
#define RTE_VXLAN_GPE_TYPE_VBNG 7 /**< vBNG Protocol. */

#endif /* RTE_VXLAN_H_ */
