/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_RTE_PARSER_H_
#define _ULP_RTE_PARSER_H_

#include <rte_log.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"

/* defines to be used in the tunnel header parsing */
#define BNXT_ULP_ENCAP_IPV4_VER_HLEN_TOS	2
#define BNXT_ULP_ENCAP_IPV4_ID_PROTO		6
#define BNXT_ULP_ENCAP_IPV4_DEST_IP		4
#define BNXT_ULP_ENCAP_IPV4_SIZE		12
#define BNXT_ULP_ENCAP_IPV6_SIZE		8
#define BNXT_ULP_ENCAP_UDP_SIZE			4
#define BNXT_ULP_INVALID_SVIF_VAL		-1U

#define	BNXT_ULP_GET_IPV6_VER(vtcf)		\
			(((vtcf) & BNXT_ULP_PARSER_IPV6_VER_MASK) >> 28)
#define	BNXT_ULP_GET_IPV6_TC(vtcf)		\
			(((vtcf) & BNXT_ULP_PARSER_IPV6_TC) >> 20)
#define	BNXT_ULP_GET_IPV6_FLOWLABEL(vtcf)	\
			((vtcf) & BNXT_ULP_PARSER_IPV6_FLOW_LABEL)
#define	BNXT_ULP_PARSER_IPV6_VER_MASK		0xf0000000
#define	BNXT_ULP_PARSER_IPV6_TC			0x0ff00000
#define	BNXT_ULP_PARSER_IPV6_FLOW_LABEL		0x000fffff

/* Function to handle the parsing of the RTE port id. */
int32_t
ulp_rte_parser_implicit_match_port_process(struct ulp_rte_parser_params *param);

/* Function to handle the implicit action port id */
int32_t
ulp_rte_parser_implicit_act_port_process(struct ulp_rte_parser_params *params);

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow items into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_hdr_parse(const struct rte_flow_item pattern[],
			      struct ulp_rte_parser_params *params);

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow actions into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_act_parse(const struct rte_flow_action actions[],
			      struct ulp_rte_parser_params *params);

/*
 * Function to handle the post processing of the parsing details
 */
int32_t
bnxt_ulp_rte_parser_post_process(struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item PF Header. */
int32_t
ulp_rte_pf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item VF Header. */
int32_t
ulp_rte_vf_hdr_handler(const struct rte_flow_item *item,
		       struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item port id Header. */
int32_t
ulp_rte_port_id_hdr_handler(const struct rte_flow_item *item,
			    struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item port Header. */
int32_t
ulp_rte_phy_port_hdr_handler(const struct rte_flow_item *item,
			     struct ulp_rte_parser_params *params);

/* Function to handle the RTE item Ethernet Header. */
int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item Vlan Header. */
int32_t
ulp_rte_vlan_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item IPV6 Header. */
int32_t
ulp_rte_ipv6_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item UDP Header. */
int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item TCP Header. */
int32_t
ulp_rte_tcp_hdr_handler(const struct rte_flow_item *item,
			struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item Vxlan Header. */
int32_t
ulp_rte_vxlan_hdr_handler(const struct rte_flow_item *item,
			  struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow item void Header. */
int32_t
ulp_rte_void_hdr_handler(const struct rte_flow_item *item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action void Header. */
int32_t
ulp_rte_void_act_handler(const struct rte_flow_action *action_item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action RSS Header. */
int32_t
ulp_rte_rss_act_handler(const struct rte_flow_action *action_item,
			struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action Mark Header. */
int32_t
ulp_rte_mark_act_handler(const struct rte_flow_action *action_item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action vxlan_encap Header. */
int32_t
ulp_rte_vxlan_encap_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action vxlan_encap Header. */
int32_t
ulp_rte_vxlan_decap_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action drop Header. */
int32_t
ulp_rte_drop_act_handler(const struct rte_flow_action *action_item,
			 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action count. */
int32_t
ulp_rte_count_act_handler(const struct rte_flow_action *action_item,
			  struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action PF. */
int32_t
ulp_rte_pf_act_handler(const struct rte_flow_action *action_item,
		       struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action VF. */
int32_t
ulp_rte_vf_act_handler(const struct rte_flow_action *action_item,
		       struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action port_id. */
int32_t
ulp_rte_port_id_act_handler(const struct rte_flow_action *act_item,
			    struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action phy_port. */
int32_t
ulp_rte_phy_port_act_handler(const struct rte_flow_action *action_item,
			     struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action pop vlan. */
int32_t
ulp_rte_of_pop_vlan_act_handler(const struct rte_flow_action *action_item,
				struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action push vlan. */
int32_t
ulp_rte_of_push_vlan_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action set vlan id. */
int32_t
ulp_rte_of_set_vlan_vid_act_handler(const struct rte_flow_action *action_item,
				    struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action set vlan pcp. */
int32_t
ulp_rte_of_set_vlan_pcp_act_handler(const struct rte_flow_action *action_item,
				    struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action set ipv4 src.*/
int32_t
ulp_rte_set_ipv4_src_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action set ipv4 dst.*/
int32_t
ulp_rte_set_ipv4_dst_act_handler(const struct rte_flow_action *action_item,
				 struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action set tp src.*/
int32_t
ulp_rte_set_tp_src_act_handler(const struct rte_flow_action *action_item,
			       struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action set tp dst.*/
int32_t
ulp_rte_set_tp_dst_act_handler(const struct rte_flow_action *action_item,
			       struct ulp_rte_parser_params *params);

/* Function to handle the parsing of RTE Flow action dec ttl.*/
int32_t
ulp_rte_dec_ttl_act_handler(const struct rte_flow_action *action_item,
			    struct ulp_rte_parser_params *params);

#endif /* _ULP_RTE_PARSER_H_ */
