/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>

#include "enic_compat.h"
#include "enic.h"
#include "wq_enet_desc.h"
#include "rq_enet_desc.h"
#include "cq_enet_desc.h"
#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_nic.h"

#ifdef RTE_ARCH_X86
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif

#define ENICPMD_CLSF_HASH_ENTRIES       ENICPMD_FDIR_MAX

static void copy_fltr_v1(struct filter_v2 *fltr,
		const struct rte_eth_fdir_input *input,
		const struct rte_eth_fdir_masks *masks);
static void copy_fltr_v2(struct filter_v2 *fltr,
		const struct rte_eth_fdir_input *input,
		const struct rte_eth_fdir_masks *masks);

void enic_fdir_stats_get(struct enic *enic, struct rte_eth_fdir_stats *stats)
{
	*stats = enic->fdir.stats;
}

void enic_fdir_info_get(struct enic *enic, struct rte_eth_fdir_info *info)
{
	info->mode = (enum rte_fdir_mode)enic->fdir.modes;
	info->flow_types_mask[0] = enic->fdir.types_mask;
}

void enic_fdir_info(struct enic *enic)
{
	enic->fdir.modes = (u32)RTE_FDIR_MODE_PERFECT;
	enic->fdir.types_mask  = 1 << RTE_ETH_FLOW_NONFRAG_IPV4_UDP |
				 1 << RTE_ETH_FLOW_NONFRAG_IPV4_TCP;
	if (enic->adv_filters) {
		enic->fdir.types_mask |= 1 << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER |
					 1 << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP |
					 1 << RTE_ETH_FLOW_NONFRAG_IPV6_UDP |
					 1 << RTE_ETH_FLOW_NONFRAG_IPV6_TCP |
					 1 << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP |
					 1 << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER;
		enic->fdir.copy_fltr_fn = copy_fltr_v2;
	} else {
		enic->fdir.copy_fltr_fn = copy_fltr_v1;
	}
}

static void
enic_set_layer(struct filter_generic_1 *gp, unsigned int flag,
	       enum filter_generic_1_layer layer, void *mask, void *val,
	       unsigned int len)
{
	gp->mask_flags |= flag;
	gp->val_flags |= gp->mask_flags;
	memcpy(gp->layer[layer].mask, mask, len);
	memcpy(gp->layer[layer].val, val, len);
}

/* Copy Flow Director filter to a VIC ipv4 filter (for Cisco VICs
 * without advanced filter support.
 */
static void
copy_fltr_v1(struct filter_v2 *fltr, const struct rte_eth_fdir_input *input,
	     __rte_unused const struct rte_eth_fdir_masks *masks)
{
	fltr->type = FILTER_IPV4_5TUPLE;
	fltr->u.ipv4.src_addr = rte_be_to_cpu_32(
		input->flow.ip4_flow.src_ip);
	fltr->u.ipv4.dst_addr = rte_be_to_cpu_32(
		input->flow.ip4_flow.dst_ip);
	fltr->u.ipv4.src_port = rte_be_to_cpu_16(
		input->flow.udp4_flow.src_port);
	fltr->u.ipv4.dst_port = rte_be_to_cpu_16(
		input->flow.udp4_flow.dst_port);

	if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_TCP)
		fltr->u.ipv4.protocol = PROTO_TCP;
	else
		fltr->u.ipv4.protocol = PROTO_UDP;

	fltr->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
}

/* Copy Flow Director filter to a VIC generic filter (requires advanced
 * filter support.
 */
static void
copy_fltr_v2(struct filter_v2 *fltr, const struct rte_eth_fdir_input *input,
	     const struct rte_eth_fdir_masks *masks)
{
	struct filter_generic_1 *gp = &fltr->u.generic_1;

	fltr->type = FILTER_DPDK_1;
	memset(gp, 0, sizeof(*gp));

	if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_UDP) {
		struct udp_hdr udp_mask, udp_val;
		memset(&udp_mask, 0, sizeof(udp_mask));
		memset(&udp_val, 0, sizeof(udp_val));

		if (input->flow.udp4_flow.src_port) {
			udp_mask.src_port = masks->src_port_mask;
			udp_val.src_port = input->flow.udp4_flow.src_port;
		}
		if (input->flow.udp4_flow.dst_port) {
			udp_mask.dst_port = masks->dst_port_mask;
			udp_val.dst_port = input->flow.udp4_flow.dst_port;
		}

		enic_set_layer(gp, FILTER_GENERIC_1_UDP, FILTER_GENERIC_1_L4,
			       &udp_mask, &udp_val, sizeof(struct udp_hdr));
	} else if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_TCP) {
		struct tcp_hdr tcp_mask, tcp_val;
		memset(&tcp_mask, 0, sizeof(tcp_mask));
		memset(&tcp_val, 0, sizeof(tcp_val));

		if (input->flow.tcp4_flow.src_port) {
			tcp_mask.src_port = masks->src_port_mask;
			tcp_val.src_port = input->flow.tcp4_flow.src_port;
		}
		if (input->flow.tcp4_flow.dst_port) {
			tcp_mask.dst_port = masks->dst_port_mask;
			tcp_val.dst_port = input->flow.tcp4_flow.dst_port;
		}

		enic_set_layer(gp, FILTER_GENERIC_1_TCP, FILTER_GENERIC_1_L4,
			       &tcp_mask, &tcp_val, sizeof(struct tcp_hdr));
	} else if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) {
		struct sctp_hdr sctp_mask, sctp_val;
		memset(&sctp_mask, 0, sizeof(sctp_mask));
		memset(&sctp_val, 0, sizeof(sctp_val));

		if (input->flow.sctp4_flow.src_port) {
			sctp_mask.src_port = masks->src_port_mask;
			sctp_val.src_port = input->flow.sctp4_flow.src_port;
		}
		if (input->flow.sctp4_flow.dst_port) {
			sctp_mask.dst_port = masks->dst_port_mask;
			sctp_val.dst_port = input->flow.sctp4_flow.dst_port;
		}
		if (input->flow.sctp4_flow.verify_tag) {
			sctp_mask.tag = 0xffffffff;
			sctp_val.tag = input->flow.sctp4_flow.verify_tag;
		}

		/*
		 * Unlike UDP/TCP (FILTER_GENERIC_1_{UDP,TCP}), the firmware
		 * has no "packet is SCTP" flag. Use flag=0 (generic L4) and
		 * manually set proto_id=sctp below.
		 */
		enic_set_layer(gp, 0, FILTER_GENERIC_1_L4, &sctp_mask,
			       &sctp_val, sizeof(struct sctp_hdr));
	}

	if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_UDP ||
	    input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_TCP ||
	    input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_SCTP ||
	    input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) {
		struct ipv4_hdr ip4_mask, ip4_val;
		memset(&ip4_mask, 0, sizeof(struct ipv4_hdr));
		memset(&ip4_val, 0, sizeof(struct ipv4_hdr));

		if (input->flow.ip4_flow.tos) {
			ip4_mask.type_of_service = masks->ipv4_mask.tos;
			ip4_val.type_of_service = input->flow.ip4_flow.tos;
		}
		if (input->flow.ip4_flow.ttl) {
			ip4_mask.time_to_live = masks->ipv4_mask.ttl;
			ip4_val.time_to_live = input->flow.ip4_flow.ttl;
		}
		if (input->flow.ip4_flow.proto) {
			ip4_mask.next_proto_id = masks->ipv4_mask.proto;
			ip4_val.next_proto_id = input->flow.ip4_flow.proto;
		} else if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) {
			/* Explicitly match the SCTP protocol number */
			ip4_mask.next_proto_id = 0xff;
			ip4_val.next_proto_id = IPPROTO_SCTP;
		}
		if (input->flow.ip4_flow.src_ip) {
			ip4_mask.src_addr =  masks->ipv4_mask.src_ip;
			ip4_val.src_addr = input->flow.ip4_flow.src_ip;
		}
		if (input->flow.ip4_flow.dst_ip) {
			ip4_mask.dst_addr =  masks->ipv4_mask.dst_ip;
			ip4_val.dst_addr = input->flow.ip4_flow.dst_ip;
		}

		enic_set_layer(gp, FILTER_GENERIC_1_IPV4, FILTER_GENERIC_1_L3,
			       &ip4_mask, &ip4_val, sizeof(struct ipv4_hdr));
	}

	if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_UDP) {
		struct udp_hdr udp_mask, udp_val;
		memset(&udp_mask, 0, sizeof(udp_mask));
		memset(&udp_val, 0, sizeof(udp_val));

		if (input->flow.udp6_flow.src_port) {
			udp_mask.src_port = masks->src_port_mask;
			udp_val.src_port = input->flow.udp6_flow.src_port;
		}
		if (input->flow.udp6_flow.dst_port) {
			udp_mask.dst_port = masks->dst_port_mask;
			udp_val.dst_port = input->flow.udp6_flow.dst_port;
		}
		enic_set_layer(gp, FILTER_GENERIC_1_UDP, FILTER_GENERIC_1_L4,
			       &udp_mask, &udp_val, sizeof(struct udp_hdr));
	} else if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_TCP) {
		struct tcp_hdr tcp_mask, tcp_val;
		memset(&tcp_mask, 0, sizeof(tcp_mask));
		memset(&tcp_val, 0, sizeof(tcp_val));

		if (input->flow.tcp6_flow.src_port) {
			tcp_mask.src_port = masks->src_port_mask;
			tcp_val.src_port = input->flow.tcp6_flow.src_port;
		}
		if (input->flow.tcp6_flow.dst_port) {
			tcp_mask.dst_port = masks->dst_port_mask;
			tcp_val.dst_port = input->flow.tcp6_flow.dst_port;
		}
		enic_set_layer(gp, FILTER_GENERIC_1_TCP, FILTER_GENERIC_1_L4,
			       &tcp_mask, &tcp_val, sizeof(struct tcp_hdr));
	} else if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) {
		struct sctp_hdr sctp_mask, sctp_val;
		memset(&sctp_mask, 0, sizeof(sctp_mask));
		memset(&sctp_val, 0, sizeof(sctp_val));

		if (input->flow.sctp6_flow.src_port) {
			sctp_mask.src_port = masks->src_port_mask;
			sctp_val.src_port = input->flow.sctp6_flow.src_port;
		}
		if (input->flow.sctp6_flow.dst_port) {
			sctp_mask.dst_port = masks->dst_port_mask;
			sctp_val.dst_port = input->flow.sctp6_flow.dst_port;
		}
		if (input->flow.sctp6_flow.verify_tag) {
			sctp_mask.tag = 0xffffffff;
			sctp_val.tag = input->flow.sctp6_flow.verify_tag;
		}

		enic_set_layer(gp, 0, FILTER_GENERIC_1_L4, &sctp_mask,
			       &sctp_val, sizeof(struct sctp_hdr));
	}

	if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_UDP ||
	    input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_TCP ||
	    input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_SCTP ||
	    input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_OTHER) {
		struct ipv6_hdr ipv6_mask, ipv6_val;
		memset(&ipv6_mask, 0, sizeof(struct ipv6_hdr));
		memset(&ipv6_val, 0, sizeof(struct ipv6_hdr));

		if (input->flow.ipv6_flow.proto) {
			ipv6_mask.proto = masks->ipv6_mask.proto;
			ipv6_val.proto = input->flow.ipv6_flow.proto;
		} else if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) {
			/* See comments for IPv4 SCTP above. */
			ipv6_mask.proto = 0xff;
			ipv6_val.proto = IPPROTO_SCTP;
		}
		memcpy(ipv6_mask.src_addr, masks->ipv6_mask.src_ip,
		       sizeof(ipv6_mask.src_addr));
		memcpy(ipv6_val.src_addr, input->flow.ipv6_flow.src_ip,
		       sizeof(ipv6_val.src_addr));
		memcpy(ipv6_mask.dst_addr, masks->ipv6_mask.dst_ip,
		       sizeof(ipv6_mask.dst_addr));
		memcpy(ipv6_val.dst_addr, input->flow.ipv6_flow.dst_ip,
		       sizeof(ipv6_val.dst_addr));
		if (input->flow.ipv6_flow.tc) {
			ipv6_mask.vtc_flow = masks->ipv6_mask.tc << 12;
			ipv6_val.vtc_flow = input->flow.ipv6_flow.tc << 12;
		}
		if (input->flow.ipv6_flow.hop_limits) {
			ipv6_mask.hop_limits = masks->ipv6_mask.hop_limits;
			ipv6_val.hop_limits = input->flow.ipv6_flow.hop_limits;
		}

		enic_set_layer(gp, FILTER_GENERIC_1_IPV6, FILTER_GENERIC_1_L3,
			       &ipv6_mask, &ipv6_val, sizeof(struct ipv6_hdr));
	}
}

int enic_fdir_del_fltr(struct enic *enic, struct rte_eth_fdir_filter *params)
{
	int32_t pos;
	struct enic_fdir_node *key;
	/* See if the key is in the table */
	pos = rte_hash_del_key(enic->fdir.hash, params);
	switch (pos) {
	case -EINVAL:
	case -ENOENT:
		enic->fdir.stats.f_remove++;
		return -EINVAL;
	default:
		/* The entry is present in the table */
		key = enic->fdir.nodes[pos];

		/* Delete the filter */
		vnic_dev_classifier(enic->vdev, CLSF_DEL,
			&key->fltr_id, NULL, NULL);
		rte_free(key);
		enic->fdir.nodes[pos] = NULL;
		enic->fdir.stats.free++;
		enic->fdir.stats.remove++;
		break;
	}
	return 0;
}

int enic_fdir_add_fltr(struct enic *enic, struct rte_eth_fdir_filter *params)
{
	struct enic_fdir_node *key;
	struct filter_v2 fltr;
	int32_t pos;
	u8 do_free = 0;
	u16 old_fltr_id = 0;
	u32 flowtype_supported;
	u16 flex_bytes;
	u16 queue;
	struct filter_action_v2 action;

	memset(&fltr, 0, sizeof(fltr));
	memset(&action, 0, sizeof(action));
	flowtype_supported = enic->fdir.types_mask
			     & (1 << params->input.flow_type);

	flex_bytes = ((params->input.flow_ext.flexbytes[1] << 8 & 0xFF00) |
		(params->input.flow_ext.flexbytes[0] & 0xFF));

	if (!enic->fdir.hash ||
		(params->input.flow_ext.vlan_tci & 0xFFF) ||
		!flowtype_supported || flex_bytes ||
		params->action.behavior /* drop */) {
		enic->fdir.stats.f_add++;
		return -ENOTSUP;
	}

	/* Get the enicpmd RQ from the DPDK Rx queue */
	queue = enic_rte_rq_idx_to_sop_idx(params->action.rx_queue);

	if (!enic->rq[queue].in_use)
		return -EINVAL;

	/* See if the key is already there in the table */
	pos = rte_hash_del_key(enic->fdir.hash, params);
	switch (pos) {
	case -EINVAL:
		enic->fdir.stats.f_add++;
		return -EINVAL;
	case -ENOENT:
		/* Add a new classifier entry */
		if (!enic->fdir.stats.free) {
			enic->fdir.stats.f_add++;
			return -ENOSPC;
		}
		key = rte_zmalloc("enic_fdir_node",
				  sizeof(struct enic_fdir_node), 0);
		if (!key) {
			enic->fdir.stats.f_add++;
			return -ENOMEM;
		}
		break;
	default:
		/* The entry is already present in the table.
		 * Check if there is a change in queue
		 */
		key = enic->fdir.nodes[pos];
		enic->fdir.nodes[pos] = NULL;
		if (unlikely(key->rq_index == queue)) {
			/* Nothing to be done */
			enic->fdir.stats.f_add++;
			pos = rte_hash_add_key(enic->fdir.hash, params);
			if (pos < 0) {
				dev_err(enic, "Add hash key failed\n");
				return pos;
			}
			enic->fdir.nodes[pos] = key;
			dev_warning(enic,
				"FDIR rule is already present\n");
			return 0;
		}

		if (likely(enic->fdir.stats.free)) {
			/* Add the filter and then delete the old one.
			 * This is to avoid packets from going into the
			 * default queue during the window between
			 * delete and add
			 */
			do_free = 1;
			old_fltr_id = key->fltr_id;
		} else {
			/* No free slots in the classifier.
			 * Delete the filter and add the modified one later
			 */
			vnic_dev_classifier(enic->vdev, CLSF_DEL,
				&key->fltr_id, NULL, NULL);
			enic->fdir.stats.free++;
		}

		break;
	}

	key->filter = *params;
	key->rq_index = queue;

	enic->fdir.copy_fltr_fn(&fltr, &params->input,
				&enic->rte_dev->data->dev_conf.fdir_conf.mask);
	action.type = FILTER_ACTION_RQ_STEERING;
	action.rq_idx = queue;

	if (!vnic_dev_classifier(enic->vdev, CLSF_ADD, &queue, &fltr,
	    &action)) {
		key->fltr_id = queue;
	} else {
		dev_err(enic, "Add classifier entry failed\n");
		enic->fdir.stats.f_add++;
		rte_free(key);
		return -1;
	}

	if (do_free)
		vnic_dev_classifier(enic->vdev, CLSF_DEL, &old_fltr_id, NULL,
				    NULL);
	else{
		enic->fdir.stats.free--;
		enic->fdir.stats.add++;
	}

	pos = rte_hash_add_key(enic->fdir.hash, params);
	if (pos < 0) {
		enic->fdir.stats.f_add++;
		dev_err(enic, "Add hash key failed\n");
		return pos;
	}

	enic->fdir.nodes[pos] = key;
	return 0;
}

void enic_clsf_destroy(struct enic *enic)
{
	u32 index;
	struct enic_fdir_node *key;
	/* delete classifier entries */
	for (index = 0; index < ENICPMD_FDIR_MAX; index++) {
		key = enic->fdir.nodes[index];
		if (key) {
			vnic_dev_classifier(enic->vdev, CLSF_DEL,
				&key->fltr_id, NULL, NULL);
			rte_free(key);
			enic->fdir.nodes[index] = NULL;
		}
	}

	if (enic->fdir.hash) {
		rte_hash_free(enic->fdir.hash);
		enic->fdir.hash = NULL;
	}
}

int enic_clsf_init(struct enic *enic)
{
	char clsf_name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters hash_params = {
		.name = clsf_name,
		.entries = ENICPMD_CLSF_HASH_ENTRIES,
		.key_len = sizeof(struct rte_eth_fdir_filter),
		.hash_func = DEFAULT_HASH_FUNC,
		.hash_func_init_val = 0,
		.socket_id = SOCKET_ID_ANY,
	};
	snprintf(clsf_name, RTE_HASH_NAMESIZE, "enic_clsf_%s", enic->bdf_name);
	enic->fdir.hash = rte_hash_create(&hash_params);
	memset(&enic->fdir.stats, 0, sizeof(enic->fdir.stats));
	enic->fdir.stats.free = ENICPMD_FDIR_MAX;
	return NULL == enic->fdir.hash;
}
