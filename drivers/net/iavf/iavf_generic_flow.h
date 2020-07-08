/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _IAVF_GENERIC_FLOW_H_
#define _IAVF_GENERIC_FLOW_H_

#include <rte_flow_driver.h>

/* protocol */

#define IAVF_PROT_MAC_INNER         (1ULL << 1)
#define IAVF_PROT_MAC_OUTER         (1ULL << 2)
#define IAVF_PROT_VLAN_INNER        (1ULL << 3)
#define IAVF_PROT_VLAN_OUTER        (1ULL << 4)
#define IAVF_PROT_IPV4_INNER        (1ULL << 5)
#define IAVF_PROT_IPV4_OUTER        (1ULL << 6)
#define IAVF_PROT_IPV6_INNER        (1ULL << 7)
#define IAVF_PROT_IPV6_OUTER        (1ULL << 8)
#define IAVF_PROT_TCP_INNER         (1ULL << 9)
#define IAVF_PROT_TCP_OUTER         (1ULL << 10)
#define IAVF_PROT_UDP_INNER         (1ULL << 11)
#define IAVF_PROT_UDP_OUTER         (1ULL << 12)
#define IAVF_PROT_SCTP_INNER        (1ULL << 13)
#define IAVF_PROT_SCTP_OUTER        (1ULL << 14)
#define IAVF_PROT_ICMP4_INNER       (1ULL << 15)
#define IAVF_PROT_ICMP4_OUTER       (1ULL << 16)
#define IAVF_PROT_ICMP6_INNER       (1ULL << 17)
#define IAVF_PROT_ICMP6_OUTER       (1ULL << 18)
#define IAVF_PROT_VXLAN             (1ULL << 19)
#define IAVF_PROT_NVGRE             (1ULL << 20)
#define IAVF_PROT_GTPU              (1ULL << 21)
#define IAVF_PROT_ESP		    (1ULL << 22)
#define IAVF_PROT_AH		    (1ULL << 23)
#define IAVF_PROT_L2TPV3OIP	    (1ULL << 24)
#define IAVF_PROT_PFCP		    (1ULL << 25)


/* field */

#define IAVF_SMAC                   (1ULL << 63)
#define IAVF_DMAC                   (1ULL << 62)
#define IAVF_ETHERTYPE              (1ULL << 61)
#define IAVF_IP_SRC                 (1ULL << 60)
#define IAVF_IP_DST                 (1ULL << 59)
#define IAVF_IP_PROTO               (1ULL << 58)
#define IAVF_IP_TTL                 (1ULL << 57)
#define IAVF_IP_TOS                 (1ULL << 56)
#define IAVF_SPORT                  (1ULL << 55)
#define IAVF_DPORT                  (1ULL << 54)
#define IAVF_ICMP_TYPE              (1ULL << 53)
#define IAVF_ICMP_CODE              (1ULL << 52)
#define IAVF_VXLAN_VNI              (1ULL << 51)
#define IAVF_NVGRE_TNI              (1ULL << 50)
#define IAVF_GTPU_TEID              (1ULL << 49)
#define IAVF_GTPU_QFI               (1ULL << 48)
#define IAVF_ESP_SPI		    (1ULL << 47)
#define IAVF_AH_SPI		    (1ULL << 46)
#define IAVF_L2TPV3OIP_SESSION_ID   (1ULL << 45)
#define IAVF_PFCP_S_FIELD	    (1ULL << 44)
#define IAVF_PFCP_SEID		    (1ULL << 43)

/* input set */

#define IAVF_INSET_NONE             0ULL

/* non-tunnel */

#define IAVF_INSET_SMAC         (IAVF_PROT_MAC_OUTER | IAVF_SMAC)
#define IAVF_INSET_DMAC         (IAVF_PROT_MAC_OUTER | IAVF_DMAC)
#define IAVF_INSET_VLAN_INNER   (IAVF_PROT_VLAN_INNER)
#define IAVF_INSET_VLAN_OUTER   (IAVF_PROT_VLAN_OUTER)
#define IAVF_INSET_ETHERTYPE    (IAVF_ETHERTYPE)

#define IAVF_INSET_IPV4_SRC \
	(IAVF_PROT_IPV4_OUTER | IAVF_IP_SRC)
#define IAVF_INSET_IPV4_DST \
	(IAVF_PROT_IPV4_OUTER | IAVF_IP_DST)
#define IAVF_INSET_IPV4_TOS \
	(IAVF_PROT_IPV4_OUTER | IAVF_IP_TOS)
#define IAVF_INSET_IPV4_PROTO \
	(IAVF_PROT_IPV4_OUTER | IAVF_IP_PROTO)
#define IAVF_INSET_IPV4_TTL \
	(IAVF_PROT_IPV4_OUTER | IAVF_IP_TTL)
#define IAVF_INSET_IPV6_SRC \
	(IAVF_PROT_IPV6_OUTER | IAVF_IP_SRC)
#define IAVF_INSET_IPV6_DST \
	(IAVF_PROT_IPV6_OUTER | IAVF_IP_DST)
#define IAVF_INSET_IPV6_NEXT_HDR \
	(IAVF_PROT_IPV6_OUTER | IAVF_IP_PROTO)
#define IAVF_INSET_IPV6_HOP_LIMIT \
	(IAVF_PROT_IPV6_OUTER | IAVF_IP_TTL)
#define IAVF_INSET_IPV6_TC \
	(IAVF_PROT_IPV6_OUTER | IAVF_IP_TOS)

#define IAVF_INSET_TCP_SRC_PORT \
	(IAVF_PROT_TCP_OUTER | IAVF_SPORT)
#define IAVF_INSET_TCP_DST_PORT \
	(IAVF_PROT_TCP_OUTER | IAVF_DPORT)
#define IAVF_INSET_UDP_SRC_PORT \
	(IAVF_PROT_UDP_OUTER | IAVF_SPORT)
#define IAVF_INSET_UDP_DST_PORT \
	(IAVF_PROT_UDP_OUTER | IAVF_DPORT)
#define IAVF_INSET_SCTP_SRC_PORT \
	(IAVF_PROT_SCTP_OUTER | IAVF_SPORT)
#define IAVF_INSET_SCTP_DST_PORT \
	(IAVF_PROT_SCTP_OUTER | IAVF_DPORT)
#define IAVF_INSET_ICMP4_SRC_PORT \
	(IAVF_PROT_ICMP4_OUTER | IAVF_SPORT)
#define IAVF_INSET_ICMP4_DST_PORT \
	(IAVF_PROT_ICMP4_OUTER | IAVF_DPORT)
#define IAVF_INSET_ICMP6_SRC_PORT \
	(IAVF_PROT_ICMP6_OUTER | IAVF_SPORT)
#define IAVF_INSET_ICMP6_DST_PORT \
	(IAVF_PROT_ICMP6_OUTER | IAVF_DPORT)
#define IAVF_INSET_ICMP4_TYPE \
	(IAVF_PROT_ICMP4_OUTER | IAVF_ICMP_TYPE)
#define IAVF_INSET_ICMP4_CODE \
	(IAVF_PROT_ICMP4_OUTER | IAVF_ICMP_CODE)
#define IAVF_INSET_ICMP6_TYPE \
	(IAVF_PROT_ICMP6_OUTER | IAVF_ICMP_TYPE)
#define IAVF_INSET_ICMP6_CODE \
	(IAVF_PROT_ICMP6_OUTER | IAVF_ICMP_CODE)
#define IAVF_INSET_GTPU_TEID \
	(IAVF_PROT_GTPU | IAVF_GTPU_TEID)
#define IAVF_INSET_GTPU_QFI \
	(IAVF_PROT_GTPU | IAVF_GTPU_QFI)
#define IAVF_INSET_ESP_SPI \
	(IAVF_PROT_ESP | IAVF_ESP_SPI)
#define IAVF_INSET_AH_SPI \
	(IAVF_PROT_AH | IAVF_AH_SPI)
#define IAVF_INSET_L2TPV3OIP_SESSION_ID \
	(IAVF_PROT_L2TPV3OIP | IAVF_L2TPV3OIP_SESSION_ID)
#define IAVF_INSET_PFCP_S_FIELD \
	(IAVF_PROT_PFCP | IAVF_PFCP_S_FIELD)
#define IAVF_INSET_PFCP_SEID \
	(IAVF_PROT_PFCP | IAVF_PFCP_S_FIELD | IAVF_PFCP_SEID)


/* empty pattern */
extern enum rte_flow_item_type iavf_pattern_empty[];

/* L2 */
extern enum rte_flow_item_type iavf_pattern_ethertype[];
extern enum rte_flow_item_type iavf_pattern_ethertype_vlan[];
extern enum rte_flow_item_type iavf_pattern_ethertype_qinq[];

/* ARP */
extern enum rte_flow_item_type iavf_pattern_eth_arp[];

/* non-tunnel IPv4 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_sctp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_sctp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_sctp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_icmp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv4_icmp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv4_icmp[];

/* non-tunnel IPv6 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_sctp[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_sctp[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_sctp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_icmp6[];
extern enum rte_flow_item_type iavf_pattern_eth_vlan_ipv6_icmp6[];
extern enum rte_flow_item_type iavf_pattern_eth_qinq_ipv6_icmp6[];

/* IPv4 GTPU (EH) */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh[];

/* IPv6 GTPU (EH) */
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh[];

/* IPv4 GTPU IPv4 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv4_icmp[];

/* IPv4 GTPU IPv6 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_ipv6_icmp[];

/* IPv6 GTPU IPv4 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv4_icmp[];

/* IPv6 GTPU IPv6 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_ipv6_icmp[];

/* IPv4 GTPU EH IPv4 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv4_icmp[];

/* IPv4 GTPU EH IPv6 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_gtpu_eh_ipv6_icmp[];

/* IPv6 GTPU EH IPv4 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv4_icmp[];

/* IPv6 GTPU EH IPv6 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6_udp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6_tcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_gtpu_eh_ipv6_icmp[];

/* ESP */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_esp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_udp_esp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_esp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_udp_esp[];

/* AH */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_ah[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_ah[];

/* L2TPV3 */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_l2tpv3[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_l2tpv3[];

/* PFCP */
extern enum rte_flow_item_type iavf_pattern_eth_ipv4_pfcp[];
extern enum rte_flow_item_type iavf_pattern_eth_ipv6_pfcp[];


extern const struct rte_flow_ops iavf_flow_ops;

/* pattern structure */
struct iavf_pattern_match_item {
	enum rte_flow_item_type *pattern_list;
	/* pattern_list must end with RTE_FLOW_ITEM_TYPE_END */
	uint64_t input_set_mask;
	void *meta;
};

typedef int (*engine_init_t)(struct iavf_adapter *ad);
typedef void (*engine_uninit_t)(struct iavf_adapter *ad);
typedef int (*engine_validation_t)(struct iavf_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error);
typedef int (*engine_create_t)(struct iavf_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error);
typedef int (*engine_destroy_t)(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error);
typedef int (*engine_query_t)(struct iavf_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_query_count *count,
		struct rte_flow_error *error);
typedef void (*engine_free_t) (struct rte_flow *flow);
typedef int (*parse_pattern_action_t)(struct iavf_adapter *ad,
		struct iavf_pattern_match_item *array,
		uint32_t array_len,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		void **meta,
		struct rte_flow_error *error);

/* engine types. */
enum iavf_flow_engine_type {
	IAVF_FLOW_ENGINE_NONE = 0,
	IAVF_FLOW_ENGINE_FDIR,
	IAVF_FLOW_ENGINE_HASH,
	IAVF_FLOW_ENGINE_MAX,
};

/**
 * classification stages.
 * for non-pipeline mode, we have two classification stages: Distributor/RSS
 * for pipeline-mode we have three classification stages:
 * Permission/Distributor/RSS
 */
enum iavf_flow_classification_stage {
	IAVF_FLOW_STAGE_NONE = 0,
	IAVF_FLOW_STAGE_RSS,
	IAVF_FLOW_STAGE_DISTRIBUTOR,
	IAVF_FLOW_STAGE_MAX,
};

/* Struct to store engine created. */
struct iavf_flow_engine {
	TAILQ_ENTRY(iavf_flow_engine) node;
	engine_init_t init;
	engine_uninit_t uninit;
	engine_validation_t validation;
	engine_create_t create;
	engine_destroy_t destroy;
	engine_query_t query_count;
	engine_free_t free;
	enum iavf_flow_engine_type type;
};

TAILQ_HEAD(iavf_engine_list, iavf_flow_engine);

/* Struct to store flow created. */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) node;
	struct iavf_flow_engine *engine;
	void *rule;
};

struct iavf_flow_parser {
	struct iavf_flow_engine *engine;
	struct iavf_pattern_match_item *array;
	uint32_t array_len;
	parse_pattern_action_t parse_pattern_action;
	enum iavf_flow_classification_stage stage;
};

/* Struct to store parser created. */
struct iavf_flow_parser_node {
	TAILQ_ENTRY(iavf_flow_parser_node) node;
	struct iavf_flow_parser *parser;
};

void iavf_register_flow_engine(struct iavf_flow_engine *engine);
int iavf_flow_init(struct iavf_adapter *ad);
void iavf_flow_uninit(struct iavf_adapter *ad);
int iavf_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error);
int iavf_register_parser(struct iavf_flow_parser *parser,
			 struct iavf_adapter *ad);
void iavf_unregister_parser(struct iavf_flow_parser *parser,
			    struct iavf_adapter *ad);
struct iavf_pattern_match_item *
iavf_search_pattern_match_item(const struct rte_flow_item pattern[],
		struct iavf_pattern_match_item *array,
		uint32_t array_len,
		struct rte_flow_error *error);
#endif
