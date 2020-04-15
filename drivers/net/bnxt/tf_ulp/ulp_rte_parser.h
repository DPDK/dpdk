/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_RTE_PARSER_H_
#define _ULP_RTE_PARSER_H_

#include <rte_log.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "ulp_template_db.h"
#include "ulp_template_struct.h"

/*
 * Function to handle the parsing of RTE Flows and placing
 * the RTE flow items into the ulp structures.
 */
int32_t
bnxt_ulp_rte_parser_hdr_parse(const struct rte_flow_item pattern[],
			      struct ulp_rte_hdr_bitmap *hdr_bitmap,
			      struct ulp_rte_hdr_field  *hdr_field);

/* Function to handle the parsing of RTE Flow item PF Header. */
int32_t
ulp_rte_pf_hdr_handler(const struct rte_flow_item	*item,
		       struct ulp_rte_hdr_bitmap	*hdr_bitmap,
		       struct ulp_rte_hdr_field		*hdr_field,
		       uint32_t				*field_idx,
		       uint32_t				*vlan_idx);

/* Function to handle the parsing of RTE Flow item VF Header. */
int32_t
ulp_rte_vf_hdr_handler(const struct rte_flow_item	*item,
		       struct ulp_rte_hdr_bitmap	*hdr_bitmap,
		       struct ulp_rte_hdr_field		*hdr_field,
		       uint32_t				*field_idx,
		       uint32_t				*vlan_idx);

/* Function to handle the parsing of RTE Flow item port id Header. */
int32_t
ulp_rte_port_id_hdr_handler(const struct rte_flow_item	*item,
			    struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			    struct ulp_rte_hdr_field	*hdr_field,
			    uint32_t			*field_idx,
			    uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item port id Header. */
int32_t
ulp_rte_phy_port_hdr_handler(const struct rte_flow_item	*item,
			     struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			     struct ulp_rte_hdr_field	*hdr_field,
			     uint32_t			*field_idx,
			     uint32_t			*vlan_idx);

/* Function to handle the RTE item Ethernet Header. */
int32_t
ulp_rte_eth_hdr_handler(const struct rte_flow_item	*item,
			struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			struct ulp_rte_hdr_field	*hdr_field,
			uint32_t			*field_idx,
			uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item Vlan Header. */
int32_t
ulp_rte_vlan_hdr_handler(const struct rte_flow_item	*item,
			 struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			 struct ulp_rte_hdr_field	*hdr_field,
			 uint32_t			*field_idx,
			 uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item IPV4 Header. */
int32_t
ulp_rte_ipv4_hdr_handler(const struct rte_flow_item	*item,
			 struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			 struct ulp_rte_hdr_field	*hdr_field,
			 uint32_t			*field_idx,
			 uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item IPV6 Header. */
int32_t
ulp_rte_ipv6_hdr_handler(const struct rte_flow_item	*item,
			 struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			 struct ulp_rte_hdr_field	*hdr_field,
			 uint32_t			*field_idx,
			 uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item UDP Header. */
int32_t
ulp_rte_udp_hdr_handler(const struct rte_flow_item	*item,
			struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			struct ulp_rte_hdr_field	*hdr_field,
			uint32_t			*field_idx,
			uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item TCP Header. */
int32_t
ulp_rte_tcp_hdr_handler(const struct rte_flow_item	*item,
			struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			struct ulp_rte_hdr_field	*hdr_field,
			uint32_t			*field_idx,
			uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item Vxlan Header. */
int32_t
ulp_rte_vxlan_hdr_handler(const struct rte_flow_item	*item,
			  struct ulp_rte_hdr_bitmap	*hdrbitmap,
			  struct ulp_rte_hdr_field	*hdr_field,
			  uint32_t			*field_idx,
			  uint32_t			*vlan_idx);

/* Function to handle the parsing of RTE Flow item void Header. */
int32_t
ulp_rte_void_hdr_handler(const struct rte_flow_item	*item,
			 struct ulp_rte_hdr_bitmap	*hdr_bitmap,
			 struct ulp_rte_hdr_field	*hdr_field,
			 uint32_t			*field_idx,
			 uint32_t			*vlan_idx);

#endif /* _ULP_RTE_PARSER_H_ */
