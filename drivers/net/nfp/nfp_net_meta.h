/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NET_META_H__
#define __NFP_NET_META_H__

#include "nfp_rxtx.h"

/* Hash type prepended when a RSS hash was computed */
#define NFP_NET_META_RSS_NONE           0
#define NFP_NET_META_RSS_IPV4           1
#define NFP_NET_META_RSS_IPV6           2
#define NFP_NET_META_RSS_IPV6_EX        3
#define NFP_NET_META_RSS_IPV4_TCP       4
#define NFP_NET_META_RSS_IPV6_TCP       5
#define NFP_NET_META_RSS_IPV6_EX_TCP    6
#define NFP_NET_META_RSS_IPV4_UDP       7
#define NFP_NET_META_RSS_IPV6_UDP       8
#define NFP_NET_META_RSS_IPV6_EX_UDP    9
#define NFP_NET_META_RSS_IPV4_SCTP      10
#define NFP_NET_META_RSS_IPV6_SCTP      11

/* Offset in Freelist buffer where packet starts on RX */
#define NFP_NET_RX_OFFSET               32

/* Working with metadata api (NFD version > 3.0) */
#define NFP_NET_META_FIELD_SIZE         4
#define NFP_NET_META_FIELD_MASK ((1 << NFP_NET_META_FIELD_SIZE) - 1)
#define NFP_NET_META_HEADER_SIZE        4
#define NFP_NET_META_NFDK_LENGTH        8

/* Working with metadata vlan api (NFD version >= 2.0) */
#define NFP_NET_META_VLAN_INFO          16
#define NFP_NET_META_VLAN_OFFLOAD       31
#define NFP_NET_META_VLAN_TPID          3
#define NFP_NET_META_VLAN_MASK          ((1 << NFP_NET_META_VLAN_INFO) - 1)
#define NFP_NET_META_VLAN_TPID_MASK     ((1 << NFP_NET_META_VLAN_TPID) - 1)
#define NFP_NET_META_TPID(d)            (((d) >> NFP_NET_META_VLAN_INFO) & \
						NFP_NET_META_VLAN_TPID_MASK)

/* Prepend field types */
#define NFP_NET_META_HASH               1 /* Next field carries hash type */
#define NFP_NET_META_MARK               2
#define NFP_NET_META_VLAN               4
#define NFP_NET_META_PORTID             5
#define NFP_NET_META_IPSEC              9

#define NFP_NET_META_PORT_ID_CTRL       ~0U

#define NFP_DESC_META_LEN(d) ((d)->rxd.meta_len_dd & PCIE_DESC_RX_META_LEN_MASK)

/* Maximum number of NFP packet metadata fields. */
#define NFP_NET_META_MAX_FIELDS      8

/* Maximum number of supported VLANs in parsed form packet metadata. */
#define NFP_NET_META_MAX_VLANS       2

enum nfp_net_meta_format {
	NFP_NET_METAFORMAT_SINGLE,
	NFP_NET_METAFORMAT_CHAINED,
};

/* Describe the raw metadata format. */
struct nfp_net_meta_raw {
	uint32_t header; /**< Field type header (see format in nfp.rst) */
	uint32_t data[NFP_NET_META_MAX_FIELDS]; /**< Array of each fields data member */
	uint8_t length; /**< Number of valid fields in @header */
};

/* Record metadata parsed from packet */
struct nfp_net_meta_parsed {
	uint32_t port_id;         /**< Port id value */
	uint32_t sa_idx;          /**< IPsec SA index */
	uint32_t hash;            /**< RSS hash value */
	uint32_t mark_id;         /**< Mark id value */
	uint16_t flags;           /**< Bitmap to indicate if meta exist */
	uint8_t hash_type;        /**< RSS hash type */
	uint8_t vlan_layer;       /**< The valid number of value in @vlan[] */
	/**
	 * Holds information parses from NFP_NET_META_VLAN.
	 * The inner most vlan starts at position 0
	 */
	struct {
		uint8_t offload;  /**< Flag indicates whether VLAN is offloaded */
		uint8_t tpid;     /**< Vlan TPID */
		uint16_t tci;     /**< Vlan TCI (PCP + Priority + VID) */
	} vlan[NFP_NET_META_MAX_VLANS];
};

struct nfp_pf_dev;

void nfp_net_meta_init_format(struct nfp_net_hw *hw,
		struct nfp_pf_dev *pf_dev);
void nfp_net_meta_parse(struct nfp_net_rx_desc *rxds,
		struct nfp_net_rxq *rxq,
		struct nfp_net_hw *hw,
		struct rte_mbuf *mb,
		struct nfp_net_meta_parsed *meta);
void nfp_net_meta_set_vlan(struct nfp_net_meta_raw *meta_data,
		struct rte_mbuf *pkt,
		uint8_t layer);
void nfp_net_meta_set_ipsec(struct nfp_net_meta_raw *meta_data,
		struct nfp_net_txq *txq,
		struct rte_mbuf *pkt,
		uint8_t layer,
		uint8_t ipsec_layer);

#endif /* __NFP_NET_META_H__ */
