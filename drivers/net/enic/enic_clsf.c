/*
 * Copyright 2008-2014 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <libgen.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_byteorder.h>

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

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif

#define ENICPMD_CLSF_HASH_ENTRIES       ENICPMD_FDIR_MAX

void enic_fdir_stats_get(struct enic *enic, struct rte_eth_fdir_stats *stats)
{
	*stats = enic->fdir.stats;
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
			&key->fltr_id, NULL);
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
	struct filter fltr = {0};
	int32_t pos;
	u8 do_free = 0;
	u16 old_fltr_id = 0;
	u32 flowtype_supported;
	u16 flex_bytes;
	u16 queue;

	flowtype_supported = (
		(RTE_ETH_FLOW_NONFRAG_IPV4_TCP == params->input.flow_type) ||
		(RTE_ETH_FLOW_NONFRAG_IPV4_UDP == params->input.flow_type));

	flex_bytes = ((params->input.flow_ext.flexbytes[1] << 8 & 0xFF00) |
		(params->input.flow_ext.flexbytes[0] & 0xFF));

	if (!enic->fdir.hash ||
		(params->input.flow_ext.vlan_tci & 0xFFF) ||
		!flowtype_supported || flex_bytes ||
		params->action.behavior /* drop */) {
		enic->fdir.stats.f_add++;
		return -ENOTSUP;
	}

	queue = params->action.rx_queue;
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
				&key->fltr_id, NULL);
			enic->fdir.stats.free++;
		}

		break;
	}

	key->filter = *params;
	key->rq_index = queue;

	fltr.type = FILTER_IPV4_5TUPLE;
	fltr.u.ipv4.src_addr = rte_be_to_cpu_32(
		params->input.flow.ip4_flow.src_ip);
	fltr.u.ipv4.dst_addr = rte_be_to_cpu_32(
		params->input.flow.ip4_flow.dst_ip);
	fltr.u.ipv4.src_port = rte_be_to_cpu_16(
		params->input.flow.udp4_flow.src_port);
	fltr.u.ipv4.dst_port = rte_be_to_cpu_16(
		params->input.flow.udp4_flow.dst_port);

	if (RTE_ETH_FLOW_NONFRAG_IPV4_TCP == params->input.flow_type)
		fltr.u.ipv4.protocol = PROTO_TCP;
	else
		fltr.u.ipv4.protocol = PROTO_UDP;

	fltr.u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;

	if (!vnic_dev_classifier(enic->vdev, CLSF_ADD, &queue, &fltr)) {
		key->fltr_id = queue;
	} else {
		dev_err(enic, "Add classifier entry failed\n");
		enic->fdir.stats.f_add++;
		rte_free(key);
		return -1;
	}

	if (do_free)
		vnic_dev_classifier(enic->vdev, CLSF_DEL, &old_fltr_id, NULL);
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
				&key->fltr_id, NULL);
			rte_free(key);
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
