/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <sys/queue.h>

#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include "bnxt.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_vnic.h"
#include "bnxt_util.h"
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
		       struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item *item =
		bnxt_flow_non_void_item(pattern);
	int use_ntuple = 1;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			use_ntuple = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			use_ntuple = 0;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
		case RTE_FLOW_ITEM_TYPE_IPV6:
		case RTE_FLOW_ITEM_TYPE_TCP:
		case RTE_FLOW_ITEM_TYPE_UDP:
			/* FALLTHROUGH */
			/* need ntuple match, reset exact match */
			if (!use_ntuple) {
				PMD_DRV_LOG(ERR,
					"VLAN flow cannot use NTUPLE filter\n");
				rte_flow_error_set
					(error,
					 EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 item,
					 "Cannot use VLAN with NTUPLE");
				return -rte_errno;
			}
			use_ntuple |= 1;
			break;
		default:
			PMD_DRV_LOG(ERR, "Unknown Flow type\n");
			use_ntuple |= 1;
		}
		item++;
	}
	return use_ntuple;
}

static int
bnxt_validate_and_parse_flow_type(struct bnxt *bp,
				  const struct rte_flow_attr *attr,
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
	const struct rte_flow_item_nvgre *nvgre_spec;
	const struct rte_flow_item_nvgre *nvgre_mask;
	const struct rte_flow_item_vxlan *vxlan_spec;
	const struct rte_flow_item_vxlan *vxlan_mask;
	uint8_t vni_mask[] = {0xFF, 0xFF, 0xFF};
	uint8_t tni_mask[] = {0xFF, 0xFF, 0xFF};
	const struct rte_flow_item_vf *vf_spec;
	uint32_t tenant_id_be = 0;
	bool vni_masked = 0;
	bool tni_masked = 0;
	uint32_t vf = 0;
	int use_ntuple;
	uint32_t en = 0;
	uint32_t en_ethertype;
	int dflt_vnic;

	use_ntuple = bnxt_filter_type_check(pattern, error);
	PMD_DRV_LOG(DEBUG, "Use NTUPLE %d\n", use_ntuple);
	if (use_ntuple < 0)
		return use_ntuple;

	filter->filter_type = use_ntuple ?
		HWRM_CFA_NTUPLE_FILTER : HWRM_CFA_EM_FILTER;
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

		if (!item->spec || !item->mask) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "spec/mask is NULL");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			/* Source MAC address mask cannot be partially set.
			 * Should be All 0's or all 1's.
			 * Destination MAC address mask must not be partially
			 * set. Should be all 1's or all 0's.
			 */
			if ((!is_zero_ether_addr(&eth_mask->src) &&
			     !is_broadcast_ether_addr(&eth_mask->src)) ||
			    (!is_zero_ether_addr(&eth_mask->dst) &&
			     !is_broadcast_ether_addr(&eth_mask->dst))) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "MAC_addr mask not valid");
				return -rte_errno;
			}

			/* Mask is not allowed. Only exact matches are */
			if (eth_mask->type &&
			    eth_mask->type != RTE_BE16(0xffff)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "ethertype mask not valid");
				return -rte_errno;
			}

			if (is_broadcast_ether_addr(&eth_mask->dst)) {
				rte_memcpy(filter->dst_macaddr,
					   &eth_spec->dst, 6);
				en |= use_ntuple ?
					NTUPLE_FLTR_ALLOC_INPUT_EN_DST_MACADDR :
					EM_FLOW_ALLOC_INPUT_EN_DST_MACADDR;
			}

			if (is_broadcast_ether_addr(&eth_mask->src)) {
				rte_memcpy(filter->src_macaddr,
					   &eth_spec->src, 6);
				en |= use_ntuple ?
					NTUPLE_FLTR_ALLOC_INPUT_EN_SRC_MACADDR :
					EM_FLOW_ALLOC_INPUT_EN_SRC_MACADDR;
			} /*
			   * else {
			   *  PMD_DRV_LOG(ERR, "Handle this condition\n");
			   * }
			   */
			if (eth_mask->type) {
				filter->ethertype =
					rte_be_to_cpu_16(eth_spec->type);
				en |= en_ethertype;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;
			if (en & en_ethertype) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "VLAN TPID matching is not"
						   " supported");
				return -rte_errno;
			}
			if (vlan_mask->tci &&
			    vlan_mask->tci == RTE_BE16(0x0fff)) {
				/* Only the VLAN ID can be matched. */
				filter->l2_ovlan =
					rte_be_to_cpu_16(vlan_spec->tci &
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
			if (vlan_mask->inner_type &&
			    vlan_mask->inner_type != RTE_BE16(0xffff)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "inner ethertype mask not"
						   " valid");
				return -rte_errno;
			}
			if (vlan_mask->inner_type) {
				filter->ethertype =
					rte_be_to_cpu_16(vlan_spec->inner_type);
				en |= en_ethertype;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			/* If mask is not involved, we could use EM filters. */
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;
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
			ipv6_mask = item->mask;

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
			tcp_mask = item->mask;

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
			udp_mask = item->mask;

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

			if (vxlan_spec->rsvd1 || vxlan_spec->rsvd0[0] ||
			    vxlan_spec->rsvd0[1] || vxlan_spec->rsvd0[2] ||
			    vxlan_spec->flags != 0x8) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VXLAN item");
				return -rte_errno;
			}

			/* Check if VNI is masked. */
			if (vxlan_spec && vxlan_mask) {
				vni_masked =
					!!memcmp(vxlan_mask->vni, vni_mask,
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
					   vxlan_spec->vni, 3);
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
		case RTE_FLOW_ITEM_TYPE_VF:
			vf_spec = item->spec;
			vf = vf_spec->id;

			if (!BNXT_PF(bp)) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Configuring on a VF!");
				return -rte_errno;
			}

			if (vf >= bp->pdev->max_vfs) {
				rte_flow_error_set(error,
						   EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Incorrect VF id!");
				return -rte_errno;
			}

			if (!attr->transfer) {
				rte_flow_error_set(error,
						   ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Matching VF traffic without"
						   " affecting it (transfer attribute)"
						   " is unsupported");
				return -rte_errno;
			}

			filter->mirror_vnic_id =
			dflt_vnic = bnxt_hwrm_func_qcfg_vf_dflt_vnic_id(bp, vf);
			if (dflt_vnic < 0) {
				/* This simply indicates there's no driver
				 * loaded. This is not an error.
				 */
				rte_flow_error_set
					(error,
					 EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 item,
					 "Unable to get default VNIC for VF");
				return -rte_errno;
			}

			filter->mirror_vnic_id = dflt_vnic;
			en |= NTUPLE_FLTR_ALLOC_INPUT_EN_MIRROR_VNIC_ID;
			break;
		default:
			break;
		}
		item++;
	}
	filter->enables = en;

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

	/* Not supported */
	if (attr->priority) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr,
				   "No support for priority.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->group) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   attr,
				   "No support for group.");
		return -rte_errno;
	}

	return 0;
}

struct bnxt_filter_info *
bnxt_get_l2_filter(struct bnxt *bp, struct bnxt_filter_info *nf,
		   struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter1, *f0;
	struct bnxt_vnic_info *vnic0;
	int rc;

	vnic0 = &bp->vnic_info[0];
	f0 = STAILQ_FIRST(&vnic0->filter);

	/* This flow has same DST MAC as the port/l2 filter. */
	if (memcmp(f0->l2_addr, nf->dst_macaddr, ETHER_ADDR_LEN) == 0)
		return f0;

	/* This flow needs DST MAC which is not same as port/l2 */
	PMD_DRV_LOG(DEBUG, "Create L2 filter for DST MAC\n");
	filter1 = bnxt_get_unused_filter(bp);
	if (filter1 == NULL)
		return NULL;

	filter1->flags = HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX;
	filter1->enables = HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR |
			L2_FILTER_ALLOC_INPUT_EN_L2_ADDR_MASK;
	memcpy(filter1->l2_addr, nf->dst_macaddr, ETHER_ADDR_LEN);
	memset(filter1->l2_addr_mask, 0xff, ETHER_ADDR_LEN);
	rc = bnxt_hwrm_set_l2_filter(bp, vnic->fw_vnic_id,
				     filter1);
	if (rc) {
		bnxt_free_filter(bp, filter1);
		return NULL;
	}
	return filter1;
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
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_vf *act_vf;
	struct bnxt_vnic_info *vnic, *vnic0;
	struct bnxt_filter_info *filter1;
	uint32_t vf = 0;
	int dflt_vnic;
	int rc;

	if (bp->eth_dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS) {
		PMD_DRV_LOG(ERR, "Cannot create flow on RSS queues\n");
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Cannot create flow on RSS queues");
		rc = -rte_errno;
		goto ret;
	}

	rc =
	bnxt_validate_and_parse_flow_type(bp, attr, pattern, error, filter);
	if (rc != 0)
		goto ret;

	rc = bnxt_flow_parse_attr(attr, error);
	if (rc != 0)
		goto ret;

	/* Since we support ingress attribute only - right now. */
	if (filter->filter_type == HWRM_CFA_EM_FILTER)
		filter->flags = HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_PATH_RX;

	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		/* Allow this flow. Redirect to a VNIC. */
		act_q = (const struct rte_flow_action_queue *)act->conf;
		if (act_q->index >= bp->rx_nr_rings) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Invalid queue ID.");
			rc = -rte_errno;
			goto ret;
		}
		PMD_DRV_LOG(DEBUG, "Queue index %d\n", act_q->index);

		vnic0 = &bp->vnic_info[0];
		vnic =  &bp->vnic_info[act_q->index];
		if (vnic == NULL) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "No matching VNIC for queue ID.");
			rc = -rte_errno;
			goto ret;
		}

		filter->dst_id = vnic->fw_vnic_id;
		filter1 = bnxt_get_l2_filter(bp, filter, vnic);
		if (filter1 == NULL) {
			rc = -ENOSPC;
			goto ret;
		}

		filter->fw_l2_filter_id = filter1->fw_l2_filter_id;
		PMD_DRV_LOG(DEBUG, "VNIC found\n");
		break;
	case RTE_FLOW_ACTION_TYPE_DROP:
		vnic0 = &bp->vnic_info[0];
		filter1 = bnxt_get_l2_filter(bp, filter, vnic0);
		if (filter1 == NULL) {
			rc = -ENOSPC;
			goto ret;
		}

		filter->fw_l2_filter_id = filter1->fw_l2_filter_id;
		if (filter->filter_type == HWRM_CFA_EM_FILTER)
			filter->flags =
				HWRM_CFA_EM_FLOW_ALLOC_INPUT_FLAGS_DROP;
		else
			filter->flags =
				HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_DROP;
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		vnic0 = &bp->vnic_info[0];
		filter1 = bnxt_get_l2_filter(bp, filter, vnic0);
		if (filter1 == NULL) {
			rc = -ENOSPC;
			goto ret;
		}

		filter->fw_l2_filter_id = filter1->fw_l2_filter_id;
		filter->flags = HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_FLAGS_METER;
		break;
	case RTE_FLOW_ACTION_TYPE_VF:
		act_vf = (const struct rte_flow_action_vf *)act->conf;
		vf = act_vf->id;

		if (!BNXT_PF(bp)) {
			rte_flow_error_set(error,
					   EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act,
					   "Configuring on a VF!");
			rc = -rte_errno;
			goto ret;
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
			rc = -ENOSPC;
			goto ret;
		}

		filter->fw_l2_filter_id = filter1->fw_l2_filter_id;
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

	if (filter1) {
		bnxt_free_filter(bp, filter1);
		filter1->fw_l2_filter_id = -1;
	}

	act = bnxt_flow_non_void_action(++act);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error,
				   EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act,
				   "Invalid action.");
		rc = -rte_errno;
		goto ret;
	}
ret:
	return rc;
}

static int
bnxt_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_filter_info *filter;
	int ret = 0;

	ret = bnxt_flow_args_validate(attr, pattern, actions, error);
	if (ret != 0)
		return ret;

	filter = bnxt_get_unused_filter(bp);
	if (filter == NULL) {
		PMD_DRV_LOG(ERR, "Not enough resources for a new flow.\n");
		return -ENOMEM;
	}

	ret = bnxt_validate_and_parse_flow(dev, pattern, actions, attr,
					   error, filter);
	/* No need to hold on to this filter if we are just validating flow */
	filter->fw_l2_filter_id = UINT64_MAX;
	bnxt_free_filter(bp, filter);

	return ret;
}

static int
bnxt_match_filter(struct bnxt *bp, struct bnxt_filter_info *nf)
{
	struct bnxt_filter_info *mf;
	struct rte_flow *flow;
	int i;

	for (i = bp->nr_vnics - 1; i >= 0; i--) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

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
			    !memcmp(mf->l2_addr, nf->l2_addr, ETHER_ADDR_LEN) &&
			    !memcmp(mf->l2_addr_mask, nf->l2_addr_mask,
				    ETHER_ADDR_LEN) &&
			    !memcmp(mf->src_macaddr, nf->src_macaddr,
				    ETHER_ADDR_LEN) &&
			    !memcmp(mf->dst_macaddr, nf->dst_macaddr,
				    ETHER_ADDR_LEN) &&
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
				/*
				 * Same Flow, Different queue
				 * Clear the old ntuple filter
				 * Reuse the matching L2 filter
				 * ID for the new filter
				 */
				nf->fw_l2_filter_id = mf->fw_l2_filter_id;
				if (nf->filter_type == HWRM_CFA_EM_FILTER)
					bnxt_hwrm_clear_em_filter(bp, mf);
				if (nf->filter_type == HWRM_CFA_NTUPLE_FILTER)
					bnxt_hwrm_clear_ntuple_filter(bp, mf);
				/* Free the old filter, update flow
				 * with new filter
				 */
				bnxt_free_filter(bp, mf);
				flow->filter = nf;
				return -EXDEV;
			}
		}
	}
	return 0;
}

static struct rte_flow *
bnxt_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_filter_info *filter;
	struct bnxt_vnic_info *vnic = NULL;
	bool update_flow = false;
	struct rte_flow *flow;
	unsigned int i;
	int ret = 0;

	flow = rte_zmalloc("bnxt_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return flow;
	}

	ret = bnxt_flow_args_validate(attr, pattern, actions, error);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Not a validate flow.\n");
		goto free_flow;
	}

	filter = bnxt_get_unused_filter(bp);
	if (filter == NULL) {
		PMD_DRV_LOG(ERR, "Not enough resources for a new flow.\n");
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

	if (filter->filter_type == HWRM_CFA_EM_FILTER) {
		filter->enables |=
			HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
		ret = bnxt_hwrm_set_em_filter(bp, filter->dst_id, filter);
	}

	if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER) {
		filter->enables |=
			HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID;
		ret = bnxt_hwrm_set_ntuple_filter(bp, filter->dst_id, filter);
	}

	for (i = 0; i < bp->nr_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (filter->dst_id == vnic->fw_vnic_id)
			break;
	}

	if (!ret) {
		flow->filter = filter;
		flow->vnic = vnic;
		if (update_flow) {
			ret = -EXDEV;
			goto free_flow;
		}
		PMD_DRV_LOG(ERR, "Successfully created flow.\n");
		STAILQ_INSERT_TAIL(&vnic->flow_list, flow, next);
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
		rte_flow_error_set(error, ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Flow with pattern exists, updating destination queue");
	else
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to create flow.");
	rte_free(flow);
	flow = NULL;
	return flow;
}

static int
bnxt_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_filter_info *filter = flow->filter;
	struct bnxt_vnic_info *vnic = flow->vnic;
	int ret = 0;

	ret = bnxt_match_filter(bp, filter);
	if (ret == 0)
		PMD_DRV_LOG(ERR, "Could not find matching flow\n");
	if (filter->filter_type == HWRM_CFA_EM_FILTER)
		ret = bnxt_hwrm_clear_em_filter(bp, filter);
	if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER)
		ret = bnxt_hwrm_clear_ntuple_filter(bp, filter);
	else
		ret = bnxt_hwrm_clear_l2_filter(bp, filter);
	if (!ret) {
		STAILQ_REMOVE(&vnic->flow_list, flow, rte_flow, next);
		rte_free(flow);
	} else {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");
	}

	return ret;
}

static int
bnxt_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct bnxt_vnic_info *vnic;
	struct rte_flow *flow;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < bp->nr_vnics; i++) {
		vnic = &bp->vnic_info[i];
		STAILQ_FOREACH(flow, &vnic->flow_list, next) {
			struct bnxt_filter_info *filter = flow->filter;

			if (filter->filter_type == HWRM_CFA_EM_FILTER)
				ret = bnxt_hwrm_clear_em_filter(bp, filter);
			if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER)
				ret = bnxt_hwrm_clear_ntuple_filter(bp, filter);

			if (ret) {
				rte_flow_error_set
					(error,
					 -ret,
					 RTE_FLOW_ERROR_TYPE_HANDLE,
					 NULL,
					 "Failed to flush flow in HW.");
				return -rte_errno;
			}

			STAILQ_REMOVE(&vnic->flow_list, flow,
				      rte_flow, next);
			rte_free(flow);
		}
	}

	return ret;
}

const struct rte_flow_ops bnxt_flow_ops = {
	.validate = bnxt_flow_validate,
	.create = bnxt_flow_create,
	.destroy = bnxt_flow_destroy,
	.flush = bnxt_flow_flush,
};
