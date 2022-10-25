/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <sys/queue.h>

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>
#include <rte_alarm.h>
#include <rte_cycles.h>

#include "bnxt.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

static int
bnxt_flow_args_validate(const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_flow_error *error)
{
	if (!pattern) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL,
				   "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL,
				   "NULL action.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL,
				   "NULL attribute.");
		return -rte_errno;
	}

	return 0;
}

static const struct rte_flow_item *
bnxt_flow_non_void_item(const struct rte_flow_item *cur)
{
	while (1) {
		if (cur->type != RTE_FLOW_ITEM_TYPE_VOID)
			return cur;
		cur++;
	}
}

static const struct rte_flow_action *
bnxt_flow_non_void_action(const struct rte_flow_action *cur)
{
	while (1) {
		if (cur->type != RTE_FLOW_ACTION_TYPE_VOID)
			return cur;
		cur++;
	}
}

static int
bnxt_filter_type_check(const struct rte_flow_item pattern[],
		       struct rte_flow_error *error)
{
	const struct rte_flow_item *item =
		bnxt_flow_non_void_item(pattern);
	int use_ntuple = 1;
	bool has_vlan = 0;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ANY:
		case RTE_FLOW_ITEM_TYPE_ETH:
			use_ntuple = 0;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			use_ntuple = 0;
			has_vlan = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
		case RTE_FLOW_ITEM_TYPE_IPV6:
		case RTE_FLOW_ITEM_TYPE_TCP:
		case RTE_FLOW_ITEM_TYPE_UDP:
			/* FALLTHROUGH */
			/* need ntuple match, reset exact match */
			use_ntuple |= 1;
			break;
		default:
			PMD_DRV_LOG(DEBUG, "Unknown Flow type\n");
			use_ntuple |= 0;
		}
		item++;
	}

	if (has_vlan && use_ntuple) {
		PMD_DRV_LOG(ERR,
			    "VLAN flow cannot use NTUPLE filter\n");
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "Cannot use VLAN with NTUPLE");
		return -rte_errno;
	}

	return use_ntuple;
}

static int
bnxt_validate_and_parse_flow_type(const struct rte_flow_attr *attr,
				  const struct rte_flow_item pattern[],
				  struct rte_flow_error *error,
				  struct bnxt_filter_info *filter)
{
	const struct rte_flow_item *item = bnxt_flow_non_void_item(pattern);
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_ether_addr *dst, *src;
	const struct rte_flow_item_nvgre *nvgre_spec;
	const struct rte_flow_item_nvgre *nvgre_mask;
	const struct rte_flow_item_gre *gre_spec;
	const struct rte_flow_item_gre *gre_mask;
	const struct rte_flow_item_vxlan *vxlan_spec;
	const struct rte_flow_item_vxlan *vxlan_mask;
	uint8_t vni_mask[] = {0xFF, 0xFF, 0xFF};
	uint8_t tni_mask[] = {0xFF, 0xFF, 0xFF};
	uint32_t tenant_id_be = 0, valid_flags = 0;
	bool vni_masked = 0;
	bool tni_masked = 0;
	uint32_t en_ethertype;
	uint8_t inner = 0;
	uint32_t en = 0;
	int use_ntuple;

	use_ntuple = bnxt_filter_type_check(pattern, error);
	if (use_ntuple < 0)
		return use_ntuple;
	PMD_DRV_LOG(DEBUG, "Use NTUPLE %d\n", use_ntuple);

	filter->filter_type = use_ntuple ?
		HWRM_CFA_NTUPLE_FILTER : HWRM_CFA_L2_FILTER;
	en_ethertype = use_ntuple ?
		NTUPLE_FLTR_ALLOC_INPUT_EN_ETHERTYPE :
		EM_FLOW_ALLOC_INPUT_EN_ETHERTYPE;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (item->last) {
			/* last or range is NOT supported as match criteria */
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "No support for range");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ANY:
			inner =
			((const struct rte_flow_item_any *)item->spec)->num > 3;
			if (inner)
				PMD_DRV_LOG(DEBUG, "Parse inner header\n");
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (!item->spec)
				break;

			eth_spec = item->spec;

			if (item->mask)
				eth_mask = item->mask;
			else
				eth_mask = &rte_flow_item_eth_mask;

			/* Source MAC address mask cannot be partially set.
			 * Should be All 0's or all 1's.
			 * Destination MAC address mask must not be partially
			 * set. Should be all 1's or all 0's.
			 */
			if ((!rte_is_zero_ether_addr(&eth_mask->hdr.src_addr) &&
			     !rte_is_broadcast_ether_addr(&eth_mask->hdr.src_addr)) ||
			    (!rte_is_zero_ether_addr(&eth_mask->hdr.dst_addr) &&
			     !rte_is_broadcast_ether_addr(&eth_mask->hdr.dst_addr))) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "MAC_addr mask not valid");
				return -rte_errno;
			}

			/* Mask is not allowed. Only exact matches are */
			if (eth_mask->hdr.ether_type &&
			    eth_mask->hdr.ether_type != RTE_BE16(0xffff)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "ethertype mask not valid");
				return -rte_errno;
			}

			if (rte_is_broadcast_ether_addr(&eth_mask->hdr.dst_addr)) {
				dst = &eth_spec->hdr.dst_addr;
				if (!rte_is_valid_assigned_ether_addr(dst)) {
					rte_flow_error_set(error,
							   EINVAL,
							   RTE_FLOW_ERROR_TYPE_ITEM,
							   item,
							   "DMAC is invalid");
					PMD_DRV_LOG(ERR,
						    "DMAC is invalid!\n");
					return -rte_errno;
				}
				rte_memcpy(filter->dst_macaddr,
					   &eth_spec->hdr.dst_addr, RTE_ETHER_ADDR_LEN);
				en |= use_ntuple ?
					NTUPLE_FLTR_ALLOC_INPUT_EN_DST_MACADDR :
					EM_FLOW_ALLOC_INPUT_EN_DST_MACADDR;
				valid_flags |= inner ?
					BNXT_FLOW_L2_INNER_DST_VALID_FLAG :
					BNXT_FLOW_L2_DST_VALID_FLAG;
				filter->priority = attr->priority;
				PMD_DRV_LOG(DEBUG,
					    "Creating a priority flow\n");
			}
			if (rte_is_broadcast_ether_addr(&eth_mask->hdr.src_addr)) {
				src = &eth_spec->hdr.src_addr;
				if (!rte_is_valid_assigned_ether_addr(src)) {
					rte_flow_error_set(error,
							   EINVAL,
							   RTE_FLOW_ERROR_TYPE_ITEM,
							   item,
							   "SMAC is invalid");
					PMD_DRV_LOG(ERR,
						    "SMAC is invalid!\n");
					return -rte_errno;
				}
				rte_memcpy(filter->src_macaddr,
					   &eth_spec->hdr.src_addr, RTE_ETHER_ADDR_LEN);
				en |= use_ntuple ?
					NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_MACADDR :
					EM_FLOW_ALLOC_INPUT_EN_SRC_MACADDR;
				valid_flags |= inner ?
					BNXT_FLOW_L2_INNER_SRC_VALID_FLAG :
					BNXT_FLOW_L2_SRC_VALID_FLAG;
			} /*
			   * else {
			   *  PMD_DRV_LOG(ERR, "Handle this condition\n");
			   * }
			   */
			if (eth_mask->hdr.ether_type) {
				filter->ethertype =
					rte_be_to_cpu_16(eth_spec->hdr.ether_type);
				en |= en_ethertype;
			}
			if (inner)
				valid_flags |= BNXT_FLOW_PARSE_INNER_FLAG;

			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;

			if (item->mask)
				vlan_mask = item->mask;
			else
				vlan_mask = &rte_flow_item_vlan_mask;

			if (en & en_ethertype) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "VLAN TPID matching is not"
						   " supported");
				return -rte_errno;
			}
			if (vlan_mask->hdr.vlan_tci &&
			    vlan_mask->hdr.vlan_tci == RTE_BE16(0x0fff)) {
				/* Only the VLAN ID can be matched. */
				filter->l2_ovlan =
					rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci &
							 RTE_BE16(0x0fff));
				en |= EM_FLOW_ALLOC_INPUT_EN_OVLAN_VID;
			} else {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "VLAN mask is invalid");
				return -rte_errno;
			}
			if (vlan_mask->hdr.eth_proto &&
			    vlan_mask->hdr.eth_proto != RTE_BE16(0xffff)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "inner ethertype mask not"
						   " valid");
				return -rte_errno;
			}
			if (vlan_mask->hdr.eth_proto) {
				filter->ethertype =
					rte_be_to_cpu_16(vlan_spec->hdr.eth_proto);
				en |= en_ethertype;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			/* If mask is not involved, we could use EM filters. */
			ipv4_spec = item->spec;

			if (!item->spec)
				break;

			if (item->mask)
				ipv4_mask = item->mask;
			else
				ipv4_mask = &rte_flow_item_ipv4_mask;

			/* Only IP DST and SRC fields are maskable. */
			if (ipv4_mask->hdr.version_ihl ||
			    ipv4_mask->hdr.type_of_service ||
			    ipv4_mask->hdr.total_length ||
			    ipv4_mask->hdr.packet_id ||
			    ipv4_mask->hdr.fragment_offset ||
			    ipv4_mask->hdr.time_to_live ||
			    ipv4_mask->hdr.next_proto_id ||
			    ipv4_mask->hdr.hdr_checksum) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
				return -rte_errno;
			}

			filter->dst_ipaddr[0] = ipv4_spec->hdr.dst_addr;
			filter->src_ipaddr[0] = ipv4_spec->hdr.src_addr;

			if (use_ntuple)
				en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR |
					NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
			else
				en |= EM_FLOW_ALLOC_INPUT_EN_SRC_IPADDR |
					EM_FLOW_ALLOC_INPUT_EN_DST_IPADDR;

			if (ipv4_mask->hdr.src_addr) {
				filter->src_ipaddr_mask[0] =
					ipv4_mask->hdr.src_addr;
				en |= !use_ntuple ? 0 :
				     NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
			}

			if (ipv4_mask->hdr.dst_addr) {
				filter->dst_ipaddr_mask[0] =
					ipv4_mask->hdr.dst_addr;
				en |= !use_ntuple ? 0 :
				     NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
			}

			filter->ip_addr_type = use_ntuple ?
			 HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_IP_ADDR_TYPE_IPV4 :
			 HWRM_CFA_EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_IPV4;

			if (ipv4_spec->hdr.next_proto_id) {
				filter->ip_protocol =
					ipv4_spec->hdr.next_proto_id;
				if (use_ntuple)
					en |= NTUPLE_FLTR_ALLOC_IN_EN_IP_PROTO;
				else
					en |= EM_FLOW_ALLOC_INPUT_EN_IP_PROTO;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6_spec = item->spec;

			if (!item->spec)
				break;

			if (item->mask)
				ipv6_mask = item->mask;
			else
				ipv6_mask = &rte_flow_item_ipv6_mask;

			/* Only IP DST and SRC fields are maskable. */
			if (ipv6_mask->hdr.vtc_flow ||
			    ipv6_mask->hdr.payload_len ||
			    ipv6_mask->hdr.proto ||
			    ipv6_mask->hdr.hop_limits) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask.");
				return -rte_errno;
			}

			if (use_ntuple)
				en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR |
					NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR;
			else
				en |= EM_FLOW_ALLOC_INPUT_EN_SRC_IPADDR |
					EM_FLOW_ALLOC_INPUT_EN_DST_IPADDR;

			rte_memcpy(filter->src_ipaddr,
				   ipv6_spec->hdr.src_addr, 16);
			rte_memcpy(filter->dst_ipaddr,
				   ipv6_spec->hdr.dst_addr, 16);

			if (!bnxt_check_zero_bytes(ipv6_mask->hdr.src_addr,
						   16)) {
				rte_memcpy(filter->src_ipaddr_mask,
					   ipv6_mask->hdr.src_addr, 16);
				en |= !use_ntuple ? 0 :
				    NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_IPADDR_MASK;
			}

			if (!bnxt_check_zero_bytes(ipv6_mask->hdr.dst_addr,
						   16)) {
				rte_memcpy(filter->dst_ipaddr_mask,
					   ipv6_mask->hdr.dst_addr, 16);
				en |= !use_ntuple ? 0 :
				     NTUPLE_FLTR_ALLOC_INPUT_EN_DST_IPADDR_MASK;
			}

			filter->ip_addr_type = use_ntuple ?
				NTUPLE_FLTR_ALLOC_INPUT_IP_ADDR_TYPE_IPV6 :
				EM_FLOW_ALLOC_INPUT_IP_ADDR_TYPE_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;

			if (!item->spec)
				break;

			if (item->mask)
				tcp_mask = item->mask;
			else
				tcp_mask = &rte_flow_item_tcp_mask;

			/* Check TCP mask. Only DST & SRC ports are maskable */
			if (tcp_mask->hdr.sent_seq ||
			    tcp_mask->hdr.recv_ack ||
			    tcp_mask->hdr.data_off ||
			    tcp_mask->hdr.tcp_flags ||
			    tcp_mask->hdr.rx_win ||
			    tcp_mask->hdr.cksum ||
			    tcp_mask->hdr.tcp_urp) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
				return -rte_errno;
			}

			filter->src_port = tcp_spec->hdr.src_port;
			filter->dst_port = tcp_spec->hdr.dst_port;

			if (use_ntuple)
				en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT |
					NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT;
			else
				en |= EM_FLOW_ALLOC_INPUT_EN_SRC_PORT |
					EM_FLOW_ALLOC_INPUT_EN_DST_PORT;

			if (tcp_mask->hdr.dst_port) {
				filter->dst_port_mask = tcp_mask->hdr.dst_port;
				en |= !use_ntuple ? 0 :
				  NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
			}

			if (tcp_mask->hdr.src_port) {
				filter->src_port_mask = tcp_mask->hdr.src_port;
				en |= !use_ntuple ? 0 :
				  NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;

			if (!item->spec)
				break;

			if (item->mask)
				udp_mask = item->mask;
			else
				udp_mask = &rte_flow_item_udp_mask;

			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}

			filter->src_port = udp_spec->hdr.src_port;
			filter->dst_port = udp_spec->hdr.dst_port;

			if (use_ntuple)
				en |= NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT |
					NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT;
			else
				en |= EM_FLOW_ALLOC_INPUT_EN_SRC_PORT |
					EM_FLOW_ALLOC_INPUT_EN_DST_PORT;

			if (udp_mask->hdr.dst_port) {
				filter->dst_port_mask = udp_mask->hdr.dst_port;
				en |= !use_ntuple ? 0 :
				  NTUPLE_FLTR_ALLOC_INPUT_EN_DST_PORT_MASK;
			}

			if (udp_mask->hdr.src_port) {
				filter->src_port_mask = udp_mask->hdr.src_port;
				en |= !use_ntuple ? 0 :
				  NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_PORT_MASK;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;
			/* Check if VXLAN item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!vxlan_spec && vxlan_mask) ||
			    (vxlan_spec && !vxlan_mask)) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VXLAN item");
				return -rte_errno;
			}

			if (!vxlan_spec && !vxlan_mask) {
				filter->tunnel_type =
				CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_VXLAN;
				break;
			}

			if ((vxlan_spec->hdr.rsvd0[0] != 0) ||
			    (vxlan_spec->hdr.rsvd0[1] != 0) ||
			    (vxlan_spec->hdr.rsvd0[2] != 0) ||
			    (vxlan_spec->hdr.rsvd1 != 0) ||
			    (vxlan_spec->hdr.flags != 8)) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VXLAN item");
				return -rte_errno;
			}

			/* Check if VNI is masked. */
			if (vxlan_mask != NULL) {
				vni_masked =
					!!memcmp(vxlan_mask->hdr.vni, vni_mask,
						 RTE_DIM(vni_mask));
				if (vni_masked) {
					rte_flow_error_set
						(error,
						 EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 item,
						 "Invalid VNI mask");
					return -rte_errno;
				}

				rte_memcpy(((uint8_t *)&tenant_id_be + 1),
					   vxlan_spec->hdr.vni, 3);
				filter->vni =
					rte_be_to_cpu_32(tenant_id_be);
				filter->tunnel_type =
				 CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_VXLAN;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			nvgre_spec = item->spec;
			nvgre_mask = item->mask;
			/* Check if NVGRE item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!nvgre_spec && nvgre_mask) ||
			    (nvgre_spec && !nvgre_mask)) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid NVGRE item");
				return -rte_errno;
			}

			if (!nvgre_spec && !nvgre_mask) {
				filter->tunnel_type =
				CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_NVGRE;
				break;
			}

			if (nvgre_spec->c_k_s_rsvd0_ver != 0x2000 ||
			    nvgre_spec->protocol != 0x6558) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid NVGRE item");
				return -rte_errno;
			}

			if (nvgre_spec && nvgre_mask) {
				tni_masked =
					!!memcmp(nvgre_mask->tni, tni_mask,
						 RTE_DIM(tni_mask));
				if (tni_masked) {
					rte_flow_error_set
						(error,
						 EINVAL,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 item,
						 "Invalid TNI mask");
					return -rte_errno;
				}
				rte_memcpy(((uint8_t *)&tenant_id_be + 1),
					   nvgre_spec->tni, 3);
				filter->vni =
					rte_be_to_cpu_32(tenant_id_be);
				filter->tunnel_type =
				 CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_NVGRE;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_GRE:
			gre_spec = (const struct rte_flow_item_gre *)item->spec;
			gre_mask = (const struct rte_flow_item_gre *)item->mask;

			/*
			 *Check if GRE item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if (!!gre_spec ^ !!gre_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GRE item");
				return -rte_errno;
			}

			if (!gre_spec && !gre_mask) {
				filter->tunnel_type =
				CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_IPGRE;
				break;
			}
			break;

		default:
			break;
		}
		item++;
	}
	filter->enables = en;
	filter->valid_flags = valid_flags;

	/* Items parsed but no filter to create in HW. */
	if (filter->enables == 0 && filter->valid_flags == 0)
		filter->filter_type = HWRM_CFA_CONFIG;

	return 0;
}

/* Parse attributes */
static int
bnxt_flow_parse_attr(const struct rte_flow_attr *attr,
		     struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr,
				   "Only support ingress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->egress) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr,
				   "No support for egress.");
		return -rte_errno;
	}

	return 0;
}

static struct bnxt_filter_info *
bnxt_find_matching_l2_filter(struct bnxt *bp, struct bnxt_filter_info *nf)
{
	struct bnxt_filter_info *mf, *f0;
	struct bnxt_vnic_info *vnic0;
	int i;

	vnic0 = BNXT_GET_DEFAULT_VNIC(bp);
	f0 = STAILQ_FIRST(&vnic0->filter);

	/* This flow has same DST MAC as the port/l2 filter. */
	if (memcmp(f0->l2_addr, nf->dst_macaddr, RTE_ETHER_ADDR_LEN) == 0)
		return f0;

	for (i = bp->max_vnics - 1; i >= 0; i--) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		if (vnic->fw_vnic_id == INVALID_VNIC_ID)
			continue;

		STAILQ_FOREACH(mf, &vnic->filter, next) {

			if (mf->matching_l2_fltr_ptr)
				continue;

			if (mf->ethertype == nf->ethertype &&
			    mf->l2_ovlan == nf->l2_ovlan &&
			    mf->l2_ovlan_mask == nf->l2_ovlan_mask &&
			    mf->l2_ivlan == nf->l2_ivlan &&
			    mf->l2_ivlan_mask == nf->l2_ivlan_mask &&
			    !memcmp(mf->src_macaddr, nf->src_macaddr,
				    RTE_ETHER_ADDR_LEN) &&
			    !memcmp(mf->dst_macaddr, nf->dst_macaddr,
				    RTE_ETHER_ADDR_LEN))
				return mf;
		}
	}
	return NULL;
}

static struct bnxt_filter_info *
bnxt_create_l2_filter(struct bnxt *bp, struct bnxt_filter_info *nf,
		      struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter1;
	int rc;

	/* Alloc new L2 filter.
	 * This flow needs MAC filter which does not match any existing
	 * L2 filters.
	 */
	filter1 = bnxt_get_unused_filter(bp);
	if (filter1 == NULL)
		return NULL;

	memcpy(filter1, nf, sizeof(*filter1));

	filter1->flags = HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_XDP_DISABLE;
	filter1->flags |= HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
	if (nf->valid_flags & BNXT_FLOW_L2_SRC_VALID_FLAG ||
	    nf->valid_flags & BNXT_FLOW_L2_DST_VALID_FLAG) {
		filter1->flags |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_OUTERMOST;
		PMD_DRV_LOG(DEBUG, "Create Outer filter\n");
	}

	if (nf->filter_type == HWRM_CFA_L2_FILTER &&
	    (nf->valid_flags & BNXT_FLOW_L2_SRC_VALID_FLAG ||
	     nf->valid_flags & BNXT_FLOW_L2_INNER_SRC_VALID_FLAG)) {
		PMD_DRV_LOG(DEBUG, "Create L2 filter for SRC MAC\n");
		filter1->flags |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_SOURCE_VALID;
		memcpy(filter1->l2_addr, nf->src_macaddr, RTE_ETHER_ADDR_LEN);
	} else {
		PMD_DRV_LOG(DEBUG, "Create L2 filter for DST MAC\n");
		memcpy(filter1->l2_addr, nf->dst_macaddr, RTE_ETHER_ADDR_LEN);
	}

	if (nf->priority &&
	    (nf->valid_flags & BNXT_FLOW_L2_DST_VALID_FLAG ||
	     nf->valid_flags & BNXT_FLOW_L2_INNER_DST_VALID_FLAG)) {
		/* Tell the FW where to place the filter in the table. */
		if (nf->priority > 65535) {
			filter1->pri_hint =
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_BELOW_FILTER;
			/* This will place the filter in TCAM */
			filter1->l2_filter_id_hint = (uint64_t)-1;
		}
	}

	if (nf->valid_flags & (BNXT_FLOW_L2_DST_VALID_FLAG |
			       BNXT_FLOW_L2_SRC_VALID_FLAG |
			       BNXT_FLOW_L2_INNER_SRC_VALID_FLAG |
			       BNXT_FLOW_L2_INNER_DST_VALID_FLAG)) {
		filter1->enables =
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			L2_FILTER_ALLOC_INPUT_EN_L2_ADDR_MASK;
		memset(filter1->l2_addr_mask, 0xff, RTE_ETHER_ADDR_LEN);
	}

	if (nf->valid_flags & BNXT_FLOW_L2_DROP_FLAG) {
		filter1->flags |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_DROP;
		if (nf->ethertype == RTE_ETHER_TYPE_IPV4) {
			/* Num VLANs for drop filter will/should be 0.
			 * If the req is memset to 0, then the count will
			 * be automatically set to 0.
			 */
			if (nf->valid_flags & BNXT_FLOW_PARSE_INNER_FLAG) {
				filter1->enables |=
					L2_FILTER_ALLOC_INPUT_EN_T_NUM_VLANS;
			} else {
				filter1->enables |=
					L2_FILTER_ALLOC_INPUT_EN_NUM_VLANS;
				filter1->flags |=
				HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_OUTERMOST;
			}
		}
	}

	rc = bnxt_hwrm_set_l2_filter(bp, vnic->fw_vnic_id,
				     filter1);
	if (rc) {
		bnxt_free_filter(bp, filter1);
		return NULL;
	}
	return filter1;
}

struct bnxt_filter_info *
bnxt_get_l2_filter(struct bnxt *bp, struct bnxt_filter_info *nf,
		   struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *l2_filter = NULL;

	l2_filter = bnxt_find_matching_l2_filter(bp, nf);
	if (l2_filter) {
		l2_filter->l2_ref_cnt++;
	} else {
		l2_filter = bnxt_create_l2_filter(bp, nf, vnic);
		if (l2_filter) {
			STAILQ_INSERT_TAIL(&vnic->filter, l2_filter, next);
			l2_filter->vnic = vnic;
		}
	}
	nf->matching_l2_fltr_ptr = l2_filter;

	return l2_filter;
}

static void bnxt_vnic_cleanup(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	if (vnic->rx_queue_cnt > 1)
		bnxt_hwrm_vnic_ctx_free(bp, vnic);

	bnxt_hwrm_vnic_free(bp, vnic);

	rte_free(vnic->fw_grp_ids);
	vnic->fw_grp_ids = NULL;

	vnic->rx_queue_cnt = 0;
}

static int bnxt_vnic_prep(struct bnxt *bp, struct bnxt_vnic_info *vnic,
			  const struct rte_flow_action *act,
			  struct rte_flow_error *error)
{
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	uint64_t rx_offloads = dev_conf->rxmode.offloads;
	int rc;

	if (bp->nr_vnics > bp->max_vnics - 1)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "Group id is invalid");

	rc = bnxt_vnic_grp_alloc(bp, vnic);
	if (rc)
		return rte_flow_error_set(error, -rc,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act,
					  "Failed to alloc VNIC group");

	rc = bnxt_hwrm_vnic_alloc(bp, vnic);
	if (rc) {
		rte_flow_error_set(error, -rc,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Failed to alloc VNIC");
		goto ret;
	}

	/* RSS context is required only when there is more than one RSS ring */
	if (vnic->rx_queue_cnt > 1) {
		rc = bnxt_hwrm_vnic_ctx_alloc(bp, vnic, 0);
		if (rc) {
			rte_flow_error_set(error, -rc,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Failed to alloc VNIC context");
			goto ret;
		}
	}

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		vnic->vlan_strip = true;
	else
		vnic->vlan_strip = false;

	rc = bnxt_hwrm_vnic_cfg(bp, vnic);
	if (rc) {
		rte_flow_error_set(error, -rc,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Failed to configure VNIC");
		goto ret;
	}

	rc = bnxt_hwrm_vnic_plcmode_cfg(bp, vnic);
	if (rc) {
		rte_flow_error_set(error, -rc,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Failed to configure VNIC plcmode");
		goto ret;
	}

	bp->nr_vnics++;

	return 0;

ret:
	bnxt_vnic_cleanup(bp, vnic);
	return rc;
}

static int match_vnic_rss_cfg(struct bnxt *bp,
			      struct bnxt_vnic_info *vnic,
			      const struct rte_flow_action_rss *rss)
{
	unsigned int match = 0, i;

	if (vnic->rx_queue_cnt != rss->queue_num)
		return -EINVAL;

	for (i = 0; i < rss->queue_num; i++) {
		if (!bp->rx_queues[rss->queue[i]]->vnic->rx_queue_cnt &&
		    !bp->rx_queues[rss->queue[i]]->rx_started)
			return -EINVAL;
	}

	for (i = 0; i < vnic->rx_queue_cnt; i++) {
		int j;

		for (j = 0; j < vnic->rx_queue_cnt; j++) {
			if (bp->grp_info[rss->queue[i]].fw_grp_id ==
			    vnic->fw_grp_ids[j])
				match++;
		}
	}

	if (match != vnic->rx_queue_cnt) {
		PMD_DRV_LOG(ERR,
			    "VNIC queue count %d vs queues matched %d\n",
			    match, vnic->rx_queue_cnt);
		return -EINVAL;
	}

	return 0;
}

static void
bnxt_update_filter_flags_en(struct bnxt_filter_info *filter,
			    struct bnxt_filter_info *filter1,
			    int use_ntuple)
{
	if (!use_ntuple &&
	    !(filter->valid_flags &
	      ~(BNXT_FLOW_L2_DST_VALID_FLAG |
		BNXT_FLOW_L2_SRC_VALID_FLAG |
		BNXT_FLOW_L2_INNER_SRC_VALID_FLAG |
		BNXT_FLOW_L2_INNER_DST_VALID_FLAG |
		BNXT_FLOW_L2_DROP_FLAG |
		BNXT_FLOW_PARSE_INNER_FLAG))) {
		filter->flags = filter1->flags;
		filter->enables = filter1->enables;
		filter->filter_type = HWRM_CFA_L2_FILTER;
		memcpy(filter->l2_addr, filter1->l2_addr, RTE_ETHER_ADDR_LEN);
		memset(filter->l2_addr_mask, 0xff, RTE_ETHER_ADDR_LEN);
		filter->pri_hint = filter1->pri_hint;
		filter->l2_filter_id_hint = filter1->l2_filter_id_hint;
	}
	filter->fw_l2_filter_id = filter1->fw_l2_filter_id;
	filter->l2_ref_cnt = filter1->l2_ref_cnt;
	filter->flow_id = filter1->flow_id;
	PMD_DRV_LOG(DEBUG,
		"l2_filter: %p fw_l2_filter_id %" PRIx64 " l2_ref_cnt %u\n",
		filter1, filter->fw_l2_filter_id, filter->l2_ref_cnt);
}

static int
bnxt_validate_rss_action(const struct rte_flow_action actions[])
{
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			break;
		default:
			return -ENOTSUP;
		}
	}

	return 0;
}

static int
bnxt_get_vnic(struct bnxt *bp, uint32_t group)
{
	int vnic_id = 0;

	/* For legacy NS3 based implementations,
	 * group_id will be mapped to a VNIC ID.
	 */
	if (BNXT_STINGRAY(bp))
		vnic_id = group;

	/* Non NS3 cases, group_id will be ignored.
	 * Setting will be configured on default VNIC.
	 */
	return vnic_id;
}

static int
bnxt_vnic_rss_cfg_update(struct bnxt *bp,
			 struct bnxt_vnic_info *vnic,
			 const struct rte_flow_action *act,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss;
	unsigned int rss_idx, i, j, fw_idx;
	uint16_t hash_type;
	uint64_t types;
	int rc;

	rss = (const struct rte_flow_action_rss *)act->conf;

	/* must specify either all the Rx queues created by application or zero queues */
	if (rss->queue_num && vnic->rx_queue_cnt != rss->queue_num) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Incorrect RXQ count");
		rc = -rte_errno;
		goto ret;
	}

	/* Validate Rx queues */
	for (i = 0; i < rss->queue_num; i++) {
		PMD_DRV_LOG(DEBUG, "RSS action Queue %d\n", rss->queue[i]);

		if (rss->queue[i] >= bp->rx_nr_rings ||
		    !bp->rx_queues[rss->queue[i]]) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Invalid queue ID for RSS");
			rc = -rte_errno;
			goto ret;
		}
	}

	/* Duplicate queue ids are not supported. */
	for (i = 0; i < rss->queue_num; i++) {
		for (j = i + 1; j < rss->queue_num; j++) {
			if (rss->queue[i] == rss->queue[j]) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   act,
						   "Duplicate queue ID for RSS");
				rc = -rte_errno;
				goto ret;
			}
		}
	}

	/* Currently only Toeplitz hash is supported. */
	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT &&
	    rss->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ) {
		rte_flow_error_set(error,
				   ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Unsupported RSS hash function");
		rc = -rte_errno;
		goto ret;
	}

	/* key_len should match the hash key supported by hardware */
	if (rss->key_len != 0 && rss->key_len != HW_HASH_KEY_SIZE) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Incorrect hash key parameters");
		rc = -rte_errno;
		goto ret;
	}

	/* Currently RSS hash on inner and outer headers are supported.
	 * 0 => Default (innermost RSS) setting
	 * 1 => Outermost
	 */
	if (rss->level > 1) {
		rte_flow_error_set(error,
				   ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Unsupported hash level");
		rc = -rte_errno;
		goto ret;
	}

	if ((rss->queue_num == 0 && rss->queue != NULL) ||
	    (rss->queue_num != 0 && rss->queue == NULL)) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Invalid queue config specified");
		rc = -rte_errno;
		goto ret;
	}

	/* If RSS types is 0, use a best effort configuration */
	types = rss->types ? rss->types : RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_IPV6;

	hash_type = bnxt_rte_to_hwrm_hash_types(types);

	/* If requested types can't be supported, leave existing settings */
	if (hash_type)
		vnic->hash_type = hash_type;

	vnic->hash_mode =
		bnxt_rte_to_hwrm_hash_level(bp, rss->types, rss->level);

	/* Update RSS key only if key_len != 0 */
	if (rss->key_len != 0)
		memcpy(vnic->rss_hash_key, rss->key, rss->key_len);

	if (rss->queue_num == 0)
		goto skip_rss_table;

	/* Prepare the indirection table */
	for (rss_idx = 0, fw_idx = 0; rss_idx < HW_HASH_INDEX_SIZE;
	     rss_idx++, fw_idx++) {
		uint8_t *rxq_state = bp->eth_dev->data->rx_queue_state;
		struct bnxt_rx_queue *rxq;
		uint32_t idx;

		for (i = 0; i < bp->rx_cp_nr_rings; i++) {
			idx = rss->queue[fw_idx % rss->queue_num];
			if (rxq_state[idx] != RTE_ETH_QUEUE_STATE_STOPPED)
				break;
			fw_idx++;
		}

		if (i == bp->rx_cp_nr_rings)
			return 0;

		if (BNXT_CHIP_P5(bp)) {
			rxq = bp->rx_queues[idx];
			vnic->rss_table[rss_idx * 2] =
				rxq->rx_ring->rx_ring_struct->fw_ring_id;
			vnic->rss_table[rss_idx * 2 + 1] =
				rxq->cp_ring->cp_ring_struct->fw_ring_id;
		} else {
			vnic->rss_table[rss_idx] = vnic->fw_grp_ids[idx];
		}
	}

skip_rss_table:
	rc = bnxt_hwrm_vnic_rss_cfg(bp, vnic);
	if (rc != 0) {
		rte_flow_error_set(error,
				   -rc,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "VNIC RSS configure failed");
		rc = -rte_errno;
		goto ret;
	}
ret:
	return rc;
}

static int
bnxt_validate_and_parse_flow(struct rte_eth_dev *dev,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     const struct rte_flow_attr *attr,
			     struct rte_flow_error *error,
			     struct bnxt_filter_info *filter)
{
	const struct rte_flow_action *act =
		bnxt_flow_non_void_action(actions);
	struct bnxt *bp = dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_vnic_info *vnic = NULL, *vnic0 = NULL;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_vf *act_vf;
	struct bnxt_filter_info *filter1 = NULL;
	const struct rte_flow_action_rss *rss;
	struct bnxt_rx_queue *rxq = NULL;
	int dflt_vnic, vnic_id;
	unsigned int rss_idx;
	uint32_t vf = 0, i;
	int rc, use_ntuple;

	rc =
	bnxt_validate_and_parse_flow_type(attr, pattern, error, filter);
	if (rc != 0)
		goto ret;

	rc = bnxt_flow_parse_attr(attr, error);
	if (rc != 0)
		goto ret;

	/* Since we support ingress attribute only - right now. */
	if (filter->filter_type == HWRM_CFA_EM_FILTER)
		filter->flags = HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH_RX;

	use_ntuple = bnxt_filter_type_check(pattern, error);

start:
	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		/* Allow this flow. Redirect to a VNIC. */
		act_q = (const struct rte_flow_action_queue *)act->conf;
		if (!act_q->index || act_q->index >= bp->rx_nr_rings) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Invalid queue ID.");
			rc = -rte_errno;
			goto ret;
		}
		PMD_DRV_LOG(DEBUG, "Queue index %d\n", act_q->index);

		vnic_id = attr->group;
		if (!vnic_id) {
			PMD_DRV_LOG(DEBUG, "Group id is 0\n");
			vnic_id = act_q->index;
		}

		BNXT_VALID_VNIC_OR_RET(bp, vnic_id);

		vnic = &bp->vnic_info[vnic_id];
		if (vnic->rx_queue_cnt) {
			if (vnic->start_grp_id != act_q->index) {
				PMD_DRV_LOG(ERR,
					    "VNIC already in use\n");
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   act,
						   "VNIC already in use");
				rc = -rte_errno;
				goto ret;
			}
			goto use_vnic;
		}

		rxq = bp->rx_queues[act_q->index];

		if (!(dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS) && rxq &&
		    vnic->fw_vnic_id != INVALID_HW_RING_ID)
			goto use_vnic;

		if (!rxq) {
			PMD_DRV_LOG(ERR,
				    "Queue invalid or used with other VNIC\n");
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Queue invalid queue or in use");
			rc = -rte_errno;
			goto ret;
		}

		rxq->vnic = vnic;
		rxq->rx_started = 1;
		vnic->rx_queue_cnt++;
		vnic->start_grp_id = act_q->index;
		vnic->end_grp_id = act_q->index;
		vnic->func_default = 0;	//This is not a default VNIC.

		PMD_DRV_LOG(DEBUG, "VNIC found\n");

		rc = bnxt_vnic_prep(bp, vnic, act, error);
		if (rc)
			goto ret;

		PMD_DRV_LOG(DEBUG,
			    "vnic[%d] = %p vnic->fw_grp_ids = %p\n",
			    act_q->index, vnic, vnic->fw_grp_ids);

use_vnic:
		vnic->ff_pool_idx = vnic_id;
		PMD_DRV_LOG(DEBUG,
			    "Setting vnic ff_idx %d\n", vnic->ff_pool_idx);
		filter->dst_id = vnic->fw_vnic_id;

		/* For ntuple filter, create the L2 filter with default VNIC.
		 * The user specified redirect queue will be set while creating
		 * the ntuple filter in hardware.
		 */
		vnic0 = BNXT_GET_DEFAULT_VNIC(bp);
		if (use_ntuple)
			filter1 = bnxt_get_l2_filter(bp, filter, vnic0);
		else
			filter1 = bnxt_get_l2_filter(bp, filter, vnic);
		if (filter1 == NULL) {
			rte_flow_error_set(error,
					   ENOSPC,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Filter not available");
			rc = -rte_errno;
			goto ret;
		}

		PMD_DRV_LOG(DEBUG, "new fltr: %p l2fltr: %p l2_ref_cnt: %d\n",
			    filter, filter1, filter1->l2_ref_cnt);
		bnxt_update_filter_flags_en(filter, filter1, use_ntuple);
		break;
	case RTE_FLOW_ACTION_TYPE_DROP:
		vnic0 = &bp->vnic_info[0];
		filter->dst_id = vnic0->fw_vnic_id;
		filter->valid_flags |= BNXT_FLOW_L2_DROP_FLAG;
		filter1 = bnxt_get_l2_filter(bp, filter, vnic0);
		if (filter1 == NULL) {
			rte_flow_error_set(error,
					   ENOSPC,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Filter not available");
			rc = -rte_errno;
			goto ret;
		}

		if (filter->filter_type == HWRM_CFA_EM_FILTER)
			filter->flags =
				HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_DROP;
		else if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER)
			filter->flags =
				HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_DROP;

		bnxt_update_filter_flags_en(filter, filter1, use_ntuple);
		break;
	case RTE_FLOW_ACTION_TYPE_VF:
		act_vf = (const struct rte_flow_action_vf *)act->conf;
		vf = act_vf->id;

		if (filter->tunnel_type ==
		    CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_VXLAN ||
		    filter->tunnel_type ==
		    CFA_NTUPLE_FILTER_ALLOC_REQ_TUNNEL_TYPE_IPGRE) {
			/* If issued on a VF, ensure id is 0 and is trusted */
			if (BNXT_VF(bp)) {
				if (!BNXT_VF_IS_TRUSTED(bp) || vf) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						act,
						"Incorrect VF");
					rc = -rte_errno;
					goto ret;
				}
			}

			filter->enables |= filter->tunnel_type;
			filter->filter_type = HWRM_CFA_TUNNEL_REDIRECT_FILTER;
			goto done;
		}

		if (vf >= bp->pdev->max_vfs) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Incorrect VF id!");
			rc = -rte_errno;
			goto ret;
		}

		filter->mirror_vnic_id =
		dflt_vnic = bnxt_hwrm_func_qcfg_vf_dflt_vnic_id(bp, vf);
		if (dflt_vnic < 0) {
			/* This simply indicates there's no driver loaded.
			 * This is not an error.
			 */
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Unable to get default VNIC for VF");
			rc = -rte_errno;
			goto ret;
		}

		filter->mirror_vnic_id = dflt_vnic;
		filter->enables |= NTUPLE_FLTR_ALLOC_INPUT_EN_MIRROR_VNIC_ID;

		vnic0 = &bp->vnic_info[0];
		filter1 = bnxt_get_l2_filter(bp, filter, vnic0);
		if (filter1 == NULL) {
			rte_flow_error_set(error,
					   ENOSPC,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "New filter not available");
			rc = -rte_errno;
			goto ret;
		}

		filter->fw_l2_filter_id = filter1->fw_l2_filter_id;
		filter->flow_id = filter1->flow_id;
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		rc = bnxt_validate_rss_action(actions);
		if (rc != 0) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Invalid actions specified with RSS");
			rc = -rte_errno;
			goto ret;
		}

		rss = (const struct rte_flow_action_rss *)act->conf;

		vnic_id = bnxt_get_vnic(bp, attr->group);

		BNXT_VALID_VNIC_OR_RET(bp, vnic_id);
		vnic = &bp->vnic_info[vnic_id];

		/*
		 * For non NS3 cases, rte_flow_items will not be considered
		 * for RSS updates.
		 */
		if (filter->filter_type == HWRM_CFA_CONFIG) {
			/* RSS config update requested */
			rc = bnxt_vnic_rss_cfg_update(bp, vnic, act, error);
			if (rc != 0)
				goto ret;

			filter->dst_id = vnic->fw_vnic_id;
			break;
		}

		/* Check if requested RSS config matches RSS config of VNIC
		 * only if it is not a fresh VNIC configuration.
		 * Otherwise the existing VNIC configuration can be used.
		 */
		if (vnic->rx_queue_cnt) {
			rc = match_vnic_rss_cfg(bp, vnic, rss);
			if (rc) {
				PMD_DRV_LOG(ERR,
					    "VNIC and RSS config mismatch\n");
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   act,
						   "VNIC and RSS cfg mismatch");
				rc = -rte_errno;
				goto ret;
			}
			goto vnic_found;
		}

		for (i = 0; i < rss->queue_num; i++) {
			PMD_DRV_LOG(DEBUG, "RSS action Queue %d\n",
				    rss->queue[i]);

			if (!rss->queue[i] ||
			    rss->queue[i] >= bp->rx_nr_rings ||
			    !bp->rx_queues[rss->queue[i]]) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   act,
						   "Invalid queue ID for RSS");
				rc = -rte_errno;
				goto ret;
			}
			rxq = bp->rx_queues[rss->queue[i]];

			if (bp->vnic_info[0].fw_grp_ids[rss->queue[i]] !=
			    INVALID_HW_RING_ID) {
				PMD_DRV_LOG(ERR,
					    "queue active with other VNIC\n");
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   act,
						   "Invalid queue ID for RSS");
				rc = -rte_errno;
				goto ret;
			}

			rxq->vnic = vnic;
			rxq->rx_started = 1;
			vnic->rx_queue_cnt++;
		}

		vnic->start_grp_id = rss->queue[0];
		vnic->end_grp_id = rss->queue[rss->queue_num - 1];
		vnic->func_default = 0;	//This is not a default VNIC.

		rc = bnxt_vnic_prep(bp, vnic, act, error);
		if (rc)
			goto ret;

		PMD_DRV_LOG(DEBUG,
			    "vnic[%d] = %p vnic->fw_grp_ids = %p\n",
			    vnic_id, vnic, vnic->fw_grp_ids);

		vnic->ff_pool_idx = vnic_id;
		PMD_DRV_LOG(DEBUG,
			    "Setting vnic ff_pool_idx %d\n", vnic->ff_pool_idx);

		/* This can be done only after vnic_grp_alloc is done. */
		for (i = 0; i < vnic->rx_queue_cnt; i++) {
			vnic->fw_grp_ids[i] =
				bp->grp_info[rss->queue[i]].fw_grp_id;
			/* Make sure vnic0 does not use these rings. */
			bp->vnic_info[0].fw_grp_ids[rss->queue[i]] =
				INVALID_HW_RING_ID;
		}

		for (rss_idx = 0; rss_idx < HW_HASH_INDEX_SIZE; ) {
			for (i = 0; i < vnic->rx_queue_cnt; i++)
				vnic->rss_table[rss_idx++] =
					vnic->fw_grp_ids[i];
		}

		/* Configure RSS only if the queue count is > 1 */
		if (vnic->rx_queue_cnt > 1) {
			vnic->hash_type =
				bnxt_rte_to_hwrm_hash_types(rss->types);
			vnic->hash_mode =
			bnxt_rte_to_hwrm_hash_level(bp, rss->types, rss->level);

			if (!rss->key_len) {
				/* If hash key has not been specified,
				 * use random hash key.
				 */
				bnxt_prandom_bytes(vnic->rss_hash_key,
						   HW_HASH_KEY_SIZE);
			} else {
				if (rss->key_len > HW_HASH_KEY_SIZE)
					memcpy(vnic->rss_hash_key,
					       rss->key,
					       HW_HASH_KEY_SIZE);
				else
					memcpy(vnic->rss_hash_key,
					       rss->key,
					       rss->key_len);
			}
			bnxt_hwrm_vnic_rss_cfg(bp, vnic);
		} else {
			PMD_DRV_LOG(DEBUG, "No RSS config required\n");
		}

vnic_found:
		filter->dst_id = vnic->fw_vnic_id;
		filter1 = bnxt_get_l2_filter(bp, filter, vnic);
		if (filter1 == NULL) {
			rte_flow_error_set(error,
					   ENOSPC,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "New filter not available");
			rc = -rte_errno;
			goto ret;
		}

		PMD_DRV_LOG(DEBUG, "L2 filter created\n");
		bnxt_update_filter_flags_en(filter, filter1, use_ntuple);
		break;
	case RTE_FLOW_ACTION_TYPE_MARK:
		if (bp->mark_table == NULL) {
			rte_flow_error_set(error,
					   ENOMEM,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Mark table not allocated.");
			rc = -rte_errno;
			goto ret;
		}

		if (bp->flags & BNXT_FLAG_RX_VECTOR_PKT_MODE) {
			PMD_DRV_LOG(DEBUG,
				    "Disabling vector processing for mark\n");
			bp->eth_dev->rx_pkt_burst = bnxt_recv_pkts;
			bp->flags &= ~BNXT_FLAG_RX_VECTOR_PKT_MODE;
		}

		filter->valid_flags |= BNXT_FLOW_MARK_FLAG;
		filter->mark = ((const struct rte_flow_action_mark *)
				act->conf)->id;
		PMD_DRV_LOG(DEBUG, "Mark the flow %d\n", filter->mark);
		break;
	default:
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Invalid action.");
		rc = -rte_errno;
		goto ret;
	}

done:
	act = bnxt_flow_non_void_action(++act);
	while (act->type != RTE_FLOW_ACTION_TYPE_END)
		goto start;

	return rc;
ret:

	if (filter1) {
		bnxt_hwrm_clear_l2_filter(bp, filter1);
		bnxt_free_filter(bp, filter1);
	}

	if (rte_errno)  {
		if (vnic && STAILQ_EMPTY(&vnic->filter))
			vnic->rx_queue_cnt = 0;

		if (rxq && !vnic->rx_queue_cnt)
			rxq->vnic = &bp->vnic_info[0];
	}
	return -rte_errno;
}

static
struct bnxt_vnic_info *find_matching_vnic(struct bnxt *bp,
					  struct bnxt_filter_info *filter)
{
	struct bnxt_vnic_info *vnic = NULL;
	unsigned int i;

	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic->fw_vnic_id != INVALID_VNIC_ID &&
		    filter->dst_id == vnic->fw_vnic_id) {
			PMD_DRV_LOG(DEBUG, "Found matching VNIC Id %d\n",
				    vnic->ff_pool_idx);
			return vnic;
		}
	}
	return NULL;
}

static int
bnxt_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct bnxt *bp = dev->data->dev_private;
	struct bnxt_vnic_info *vnic = NULL;
	struct bnxt_filter_info *filter;
	int ret = 0;

	bnxt_acquire_flow_lock(bp);
	ret = bnxt_flow_args_validate(attr, pattern, actions, error);
	if (ret != 0) {
		bnxt_release_flow_lock(bp);
		return ret;
	}

	filter = bnxt_get_unused_filter(bp);
	if (filter == NULL) {
		rte_flow_error_set(error, ENOSPC,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Not enough resources for a new flow");
		bnxt_release_flow_lock(bp);
		return -ENOSPC;
	}

	ret = bnxt_validate_and_parse_flow(dev, pattern, actions, attr,
					   error, filter);
	if (ret)
		goto exit;

	vnic = find_matching_vnic(bp, filter);
	if (vnic) {
		if (STAILQ_EMPTY(&vnic->filter)) {
			bnxt_vnic_cleanup(bp, vnic);
			bp->nr_vnics--;
			PMD_DRV_LOG(DEBUG, "Free VNIC\n");
		}
	}

	if (filter->filter_type == HWRM_CFA_EM_FILTER)
		bnxt_hwrm_clear_em_filter(bp, filter);
	else if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER)
		bnxt_hwrm_clear_ntuple_filter(bp, filter);
	else
		bnxt_hwrm_clear_l2_filter(bp, filter);

exit:
	/* No need to hold on to this filter if we are just validating flow */
	bnxt_free_filter(bp, filter);
	bnxt_release_flow_lock(bp);

	return ret;
}

static void
bnxt_update_filter(struct bnxt *bp, struct bnxt_filter_info *old_filter,
		   struct bnxt_filter_info *new_filter)
{
	/* Clear the new L2 filter that was created in the previous step in
	 * bnxt_validate_and_parse_flow. For L2 filters, we will use the new
	 * filter which points to the new destination queue and so we clear
	 * the previous L2 filter. For ntuple filters, we are going to reuse
	 * the old L2 filter and create new NTUPLE filter with this new
	 * destination queue subsequently during bnxt_flow_create. So we
	 * decrement the ref cnt of the L2 filter that would've been bumped
	 * up previously in bnxt_validate_and_parse_flow as the old n-tuple
	 * filter that was referencing it will be deleted now.
	 */
	bnxt_hwrm_clear_l2_filter(bp, old_filter);
	if (new_filter->filter_type == HWRM_CFA_L2_FILTER) {
		bnxt_hwrm_set_l2_filter(bp, new_filter->dst_id, new_filter);
	} else {
		if (new_filter->filter_type == HWRM_CFA_EM_FILTER)
			bnxt_hwrm_clear_em_filter(bp, old_filter);
		if (new_filter->filter_type == HWRM_CFA_NTUPLE_FILTER)
			bnxt_hwrm_clear_ntuple_filter(bp, old_filter);
	}
}

static int
bnxt_match_filter(struct bnxt *bp, struct bnxt_filter_info *nf)
{
	struct bnxt_filter_info *mf;
	struct rte_flow *flow;
	int i;

	for (i = bp->max_vnics - 1; i >= 0; i--) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		if (vnic->fw_vnic_id == INVALID_VNIC_ID)
			continue;

		STAILQ_FOREACH(flow, &vnic->flow_list, next) {
			mf = flow->filter;

			if (mf->filter_type == nf->filter_type &&
			    mf->flags == nf->flags &&
			    mf->src_port == nf->src_port &&
			    mf->src_port_mask == nf->src_port_mask &&
			    mf->dst_port == nf->dst_port &&
			    mf->dst_port_mask == nf->dst_port_mask &&
			    mf->ip_protocol == nf->ip_protocol &&
			    mf->ip_addr_type == nf->ip_addr_type &&
			    mf->ethertype == nf->ethertype &&
			    mf->vni == nf->vni &&
			    mf->tunnel_type == nf->tunnel_type &&
			    mf->l2_ovlan == nf->l2_ovlan &&
			    mf->l2_ovlan_mask == nf->l2_ovlan_mask &&
			    mf->l2_ivlan == nf->l2_ivlan &&
			    mf->l2_ivlan_mask == nf->l2_ivlan_mask &&
			    !memcmp(mf->l2_addr, nf->l2_addr,
				    RTE_ETHER_ADDR_LEN) &&
			    !memcmp(mf->l2_addr_mask, nf->l2_addr_mask,
				    RTE_ETHER_ADDR_LEN) &&
			    !memcmp(mf->src_macaddr, nf->src_macaddr,
				    RTE_ETHER_ADDR_LEN) &&
			    !memcmp(mf->dst_macaddr, nf->dst_macaddr,
				    RTE_ETHER_ADDR_LEN) &&
			    !memcmp(mf->src_ipaddr, nf->src_ipaddr,
				    sizeof(nf->src_ipaddr)) &&
			    !memcmp(mf->src_ipaddr_mask, nf->src_ipaddr_mask,
				    sizeof(nf->src_ipaddr_mask)) &&
			    !memcmp(mf->dst_ipaddr, nf->dst_ipaddr,
				    sizeof(nf->dst_ipaddr)) &&
			    !memcmp(mf->dst_ipaddr_mask, nf->dst_ipaddr_mask,
				    sizeof(nf->dst_ipaddr_mask))) {
				if (mf->dst_id == nf->dst_id)
					return -EEXIST;
				/* Free the old filter, update flow
				 * with new filter
				 */
				bnxt_update_filter(bp, mf, nf);
				STAILQ_REMOVE(&vnic->filter, mf,
					      bnxt_filter_info, next);
				STAILQ_INSERT_TAIL(&vnic->filter, nf, next);
				bnxt_free_filter(bp, mf);
				flow->filter = nf;
				return -EXDEV;
			}
		}
	}
	return 0;
}

static void
bnxt_setup_flow_counter(struct bnxt *bp)
{
	if (bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_COUNTERS &&
	    !(bp->flags & BNXT_FLAG_FC_THREAD) && BNXT_FLOW_XSTATS_EN(bp)) {
		rte_eal_alarm_set(US_PER_S * BNXT_FC_TIMER,
				  bnxt_flow_cnt_alarm_cb,
				  (void *)bp);
		bp->flags |= BNXT_FLAG_FC_THREAD;
	}
}

void bnxt_flow_cnt_alarm_cb(void *arg)
{
	int rc = 0;
	struct bnxt *bp = arg;

	if (!bp->flow_stat->rx_fc_out_tbl.va) {
		PMD_DRV_LOG(ERR, "bp->flow_stat->rx_fc_out_tbl.va is NULL?\n");
		bnxt_cancel_fc_thread(bp);
		return;
	}

	if (!bp->flow_stat->flow_count) {
		bnxt_cancel_fc_thread(bp);
		return;
	}

	if (!bp->eth_dev->data->dev_started) {
		bnxt_cancel_fc_thread(bp);
		return;
	}

	rc = bnxt_flow_stats_req(bp);
	if (rc) {
		PMD_DRV_LOG(ERR, "Flow stat alarm not rescheduled.\n");
		return;
	}

	rte_eal_alarm_set(US_PER_S * BNXT_FC_TIMER,
			  bnxt_flow_cnt_alarm_cb,
			  (void *)bp);
}


static struct rte_flow *
bnxt_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct bnxt *bp = dev->data->dev_private;
	struct bnxt_vnic_info *vnic = NULL;
	struct bnxt_filter_info *filter;
	bool update_flow = false;
	struct rte_flow *flow;
	int ret = 0;
	uint32_t tun_type, flow_id;

	if (BNXT_VF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to create flow, Not a Trusted VF!");
		return NULL;
	}

	if (!dev->data->dev_started) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Device must be started");
		return NULL;
	}

	flow = rte_zmalloc("bnxt_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return flow;
	}

	bnxt_acquire_flow_lock(bp);
	ret = bnxt_flow_args_validate(attr, pattern, actions, error);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Not a validate flow.\n");
		goto free_flow;
	}

	filter = bnxt_get_unused_filter(bp);
	if (filter == NULL) {
		rte_flow_error_set(error, ENOSPC,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Not enough resources for a new flow");
		goto free_flow;
	}

	ret = bnxt_validate_and_parse_flow(dev, pattern, actions, attr,
					   error, filter);
	if (ret != 0)
		goto free_filter;

	ret = bnxt_match_filter(bp, filter);
	if (ret == -EEXIST) {
		PMD_DRV_LOG(DEBUG, "Flow already exists.\n");
		/* Clear the filter that was created as part of
		 * validate_and_parse_flow() above
		 */
		bnxt_hwrm_clear_l2_filter(bp, filter);
		goto free_filter;
	} else if (ret == -EXDEV) {
		PMD_DRV_LOG(DEBUG, "Flow with same pattern exists\n");
		PMD_DRV_LOG(DEBUG, "Updating with different destination\n");
		update_flow = true;
	}

	/* If tunnel redirection to a VF/PF is specified then only tunnel_type
	 * is set and enable is set to the tunnel type. Issue hwrm cmd directly
	 * in such a case.
	 */
	if (filter->filter_type == HWRM_CFA_TUNNEL_REDIRECT_FILTER &&
	    filter->enables == filter->tunnel_type) {
		ret = bnxt_hwrm_tunnel_redirect_query(bp, &tun_type);
		if (ret) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Unable to query tunnel to VF");
			goto free_filter;
		}
		if (tun_type == (1U << filter->tunnel_type)) {
			ret =
			bnxt_hwrm_tunnel_redirect_free(bp,
						       filter->tunnel_type);
			if (ret) {
				PMD_DRV_LOG(ERR,
					    "Unable to free existing tunnel\n");
				rte_flow_error_set(error, -ret,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "Unable to free preexisting "
						   "tunnel on VF");
				goto free_filter;
			}
		}
		ret = bnxt_hwrm_tunnel_redirect(bp, filter->tunnel_type);
		if (ret) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Unable to redirect tunnel to VF");
			goto free_filter;
		}
		vnic = &bp->vnic_info[0];
		goto done;
	}

	if (filter->filter_type == HWRM_CFA_EM_FILTER) {
		filter->enables |=
			HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
		ret = bnxt_hwrm_set_em_filter(bp, filter->dst_id, filter);
		if (ret != 0) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to create EM filter");
			goto free_filter;
		}
	}

	if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER) {
		filter->enables |=
			HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
		ret = bnxt_hwrm_set_ntuple_filter(bp, filter->dst_id, filter);
		if (ret != 0) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to create ntuple filter");
			goto free_filter;
		}
	}

	vnic = find_matching_vnic(bp, filter);
done:
	if (!ret || update_flow) {
		flow->filter = filter;
		flow->vnic = vnic;
		if (update_flow) {
			ret = -EXDEV;
			goto free_flow;
		}

		if (filter->valid_flags & BNXT_FLOW_MARK_FLAG) {
			PMD_DRV_LOG(DEBUG,
				    "Mark action: mark id 0x%x, flow id 0x%x\n",
				    filter->mark, filter->flow_id);

			/* TCAM and EM should be 16-bit only.
			 * Other modes not supported.
			 */
			flow_id = filter->flow_id & BNXT_FLOW_ID_MASK;
			if (bp->mark_table[flow_id].valid) {
				rte_flow_error_set(error, EEXIST,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "Flow with mark id exists");
				bnxt_clear_one_vnic_filter(bp, filter);
				goto free_filter;
			}
			bp->mark_table[flow_id].valid = true;
			bp->mark_table[flow_id].mark_id = filter->mark;
		}

		STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
		STAILQ_INSERT_TAIL(&vnic->flow_list, flow, next);

		if (BNXT_FLOW_XSTATS_EN(bp))
			bp->flow_stat->flow_count++;
		bnxt_release_flow_lock(bp);
		bnxt_setup_flow_counter(bp);
		PMD_DRV_LOG(DEBUG, "Successfully created flow.\n");
		return flow;
	}

free_filter:
	bnxt_free_filter(bp, filter);
free_flow:
	if (ret == -EEXIST)
		rte_flow_error_set(error, ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Matching Flow exists.");
	else if (ret == -EXDEV)
		rte_flow_error_set(error, 0,
				   RTE_FLOW_ERROR_TYPE_NONE, NULL,
				   "Flow with pattern exists, updating destination queue");
	else if (!rte_errno)
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to create flow.");
	rte_free(flow);
	flow = NULL;
	bnxt_release_flow_lock(bp);
	return flow;
}

static int bnxt_handle_tunnel_redirect_destroy(struct bnxt *bp,
					       struct bnxt_filter_info *filter,
					       struct rte_flow_error *error)
{
	uint16_t tun_dst_fid;
	uint32_t tun_type;
	int ret = 0;

	ret = bnxt_hwrm_tunnel_redirect_query(bp, &tun_type);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Unable to query tunnel to VF");
		return ret;
	}
	if (tun_type == (1U << filter->tunnel_type)) {
		ret = bnxt_hwrm_tunnel_redirect_info(bp, filter->tunnel_type,
						     &tun_dst_fid);
		if (ret) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL,
					   "tunnel_redirect info cmd fail");
			return ret;
		}
		PMD_DRV_LOG(INFO, "Pre-existing tunnel fid = %x vf->fid = %x\n",
			    tun_dst_fid + bp->first_vf_id, bp->fw_fid);

		/* Tunnel doesn't belong to this VF, so don't send HWRM
		 * cmd, just delete the flow from driver
		 */
		if (bp->fw_fid != (tun_dst_fid + bp->first_vf_id)) {
			PMD_DRV_LOG(ERR,
				    "Tunnel does not belong to this VF, skip hwrm_tunnel_redirect_free\n");
		} else {
			ret = bnxt_hwrm_tunnel_redirect_free(bp,
							filter->tunnel_type);
			if (ret) {
				rte_flow_error_set(error, -ret,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "Unable to free tunnel redirection");
				return ret;
			}
		}
	}
	return ret;
}

static int
_bnxt_flow_destroy(struct bnxt *bp,
		   struct rte_flow *flow,
		    struct rte_flow_error *error)
{
	struct bnxt_filter_info *filter;
	struct bnxt_vnic_info *vnic;
	int ret = 0;
	uint32_t flow_id;

	filter = flow->filter;
	vnic = flow->vnic;

	if (filter->filter_type == HWRM_CFA_TUNNEL_REDIRECT_FILTER &&
	    filter->enables == filter->tunnel_type) {
		ret = bnxt_handle_tunnel_redirect_destroy(bp, filter, error);
		if (!ret)
			goto done;
		else
			return ret;
	}

	/* For config type, there is no filter in HW. Finish cleanup here */
	if (filter->filter_type == HWRM_CFA_CONFIG)
		goto done;

	ret = bnxt_match_filter(bp, filter);
	if (ret == 0)
		PMD_DRV_LOG(ERR, "Could not find matching flow\n");

	if (filter->valid_flags & BNXT_FLOW_MARK_FLAG) {
		flow_id = filter->flow_id & BNXT_FLOW_ID_MASK;
		memset(&bp->mark_table[flow_id], 0,
		       sizeof(bp->mark_table[flow_id]));
		filter->flow_id = 0;
	}

	ret = bnxt_clear_one_vnic_filter(bp, filter);

done:
	if (!ret) {
		/* If it is a L2 drop filter, when the filter is created,
		 * the FW updates the BC/MC records.
		 * Once this filter is removed, issue the set_rx_mask command
		 * to reset the BC/MC records in the HW to the settings
		 * before the drop counter is created.
		 */
		if (filter->valid_flags & BNXT_FLOW_L2_DROP_FLAG)
			bnxt_set_rx_mask_no_vlan(bp, &bp->vnic_info[0]);

		STAILQ_REMOVE(&vnic->filter, filter, bnxt_filter_info, next);
		bnxt_free_filter(bp, filter);
		STAILQ_REMOVE(&vnic->flow_list, flow, rte_flow, next);
		rte_free(flow);
		if (BNXT_FLOW_XSTATS_EN(bp))
			bp->flow_stat->flow_count--;

		/* If this was the last flow associated with this vnic,
		 * switch the queue back to RSS pool.
		 */
		if (vnic && !vnic->func_default &&
		    STAILQ_EMPTY(&vnic->flow_list)) {
			bnxt_vnic_cleanup(bp, vnic);
			bp->nr_vnics--;
		}
	} else {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");
	}

	return ret;
}

static int
bnxt_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct bnxt *bp = dev->data->dev_private;
	int ret = 0;

	bnxt_acquire_flow_lock(bp);
	if (!flow) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Invalid flow: failed to destroy flow.");
		bnxt_release_flow_lock(bp);
		return -EINVAL;
	}

	if (!flow->filter) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Invalid flow: failed to destroy flow.");
		bnxt_release_flow_lock(bp);
		return -EINVAL;
	}
	ret = _bnxt_flow_destroy(bp, flow, error);
	bnxt_release_flow_lock(bp);

	return ret;
}

void bnxt_cancel_fc_thread(struct bnxt *bp)
{
	bp->flags &= ~BNXT_FLAG_FC_THREAD;
	rte_eal_alarm_cancel(bnxt_flow_cnt_alarm_cb, (void *)bp);
}

static int
bnxt_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct bnxt *bp = dev->data->dev_private;
	struct bnxt_vnic_info *vnic;
	struct rte_flow *flow;
	unsigned int i;
	int ret = 0;

	bnxt_acquire_flow_lock(bp);
	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic && vnic->fw_vnic_id == INVALID_VNIC_ID)
			continue;

		while (!STAILQ_EMPTY(&vnic->flow_list)) {
			flow = STAILQ_FIRST(&vnic->flow_list);

			if (!flow->filter)
				continue;

			ret = _bnxt_flow_destroy(bp, flow, error);
			if (ret)
				break;
		}
	}

	bnxt_cancel_fc_thread(bp);
	bnxt_release_flow_lock(bp);

	return ret;
}

const struct rte_flow_ops bnxt_flow_ops = {
	.validate = bnxt_flow_validate,
	.create = bnxt_flow_create,
	.destroy = bnxt_flow_destroy,
	.flush = bnxt_flow_flush,
};
