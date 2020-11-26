/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * This file contain the implementations of the items
 * related methods. Each Item have a method to prepare
 * the item and add it into items array in given index.
 */

#include <stdint.h>
#include <rte_flow.h>

#include "items_gen.h"
#include "config.h"

/* Storage for additional parameters for items */
struct additional_para {
	rte_be32_t src_ip;
};

static void
add_ether(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_eth eth_spec;
	static struct rte_flow_item_eth eth_mask;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_ETH;
	items[items_counter].spec = &eth_spec;
	items[items_counter].mask = &eth_mask;
}

static void
add_vlan(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_vlan vlan_spec = {
		.tci = RTE_BE16(VLAN_VALUE),
	};
	static struct rte_flow_item_vlan vlan_mask = {
		.tci = RTE_BE16(0xffff),
	};

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_VLAN;
	items[items_counter].spec = &vlan_spec;
	items[items_counter].mask = &vlan_mask;
}

static void
add_ipv4(struct rte_flow_item *items,
	uint8_t items_counter, struct additional_para para)
{
	static struct rte_flow_item_ipv4 ipv4_spec;
	static struct rte_flow_item_ipv4 ipv4_mask;

	ipv4_spec.hdr.src_addr = RTE_BE32(para.src_ip);
	ipv4_mask.hdr.src_addr = RTE_BE32(0xffffffff);

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_IPV4;
	items[items_counter].spec = &ipv4_spec;
	items[items_counter].mask = &ipv4_mask;
}


static void
add_ipv6(struct rte_flow_item *items,
	uint8_t items_counter, struct additional_para para)
{
	static struct rte_flow_item_ipv6 ipv6_spec;
	static struct rte_flow_item_ipv6 ipv6_mask;

	/** Set ipv6 src **/
	memset(&ipv6_spec.hdr.src_addr, para.src_ip,
		sizeof(ipv6_spec.hdr.src_addr) / 2);

	/** Full mask **/
	memset(&ipv6_mask.hdr.src_addr, 0xff,
		sizeof(ipv6_spec.hdr.src_addr));

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_IPV6;
	items[items_counter].spec = &ipv6_spec;
	items[items_counter].mask = &ipv6_mask;
}

static void
add_tcp(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_tcp tcp_spec;
	static struct rte_flow_item_tcp tcp_mask;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_TCP;
	items[items_counter].spec = &tcp_spec;
	items[items_counter].mask = &tcp_mask;
}

static void
add_udp(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_udp udp_spec;
	static struct rte_flow_item_udp udp_mask;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_UDP;
	items[items_counter].spec = &udp_spec;
	items[items_counter].mask = &udp_mask;
}

static void
add_vxlan(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_vxlan vxlan_spec;
	static struct rte_flow_item_vxlan vxlan_mask;

	uint32_t vni_value;
	uint8_t i;

	vni_value = VNI_VALUE;

	/* Set standard vxlan vni */
	for (i = 0; i < 3; i++) {
		vxlan_spec.vni[2 - i] = vni_value >> (i * 8);
		vxlan_mask.vni[2 - i] = 0xff;
	}

	/* Standard vxlan flags */
	vxlan_spec.flags = 0x8;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_VXLAN;
	items[items_counter].spec = &vxlan_spec;
	items[items_counter].mask = &vxlan_mask;
}

static void
add_vxlan_gpe(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_vxlan_gpe vxlan_gpe_spec;
	static struct rte_flow_item_vxlan_gpe vxlan_gpe_mask;

	uint32_t vni_value;
	uint8_t i;

	vni_value = VNI_VALUE;

	/* Set vxlan-gpe vni */
	for (i = 0; i < 3; i++) {
		vxlan_gpe_spec.vni[2 - i] = vni_value >> (i * 8);
		vxlan_gpe_mask.vni[2 - i] = 0xff;
	}

	/* vxlan-gpe flags */
	vxlan_gpe_spec.flags = 0x0c;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_VXLAN_GPE;
	items[items_counter].spec = &vxlan_gpe_spec;
	items[items_counter].mask = &vxlan_gpe_mask;
}

static void
add_gre(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_gre gre_spec = {
		.protocol = RTE_BE16(RTE_ETHER_TYPE_TEB),
	};
	static struct rte_flow_item_gre gre_mask = {
		.protocol = RTE_BE16(0xffff),
	};

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_GRE;
	items[items_counter].spec = &gre_spec;
	items[items_counter].mask = &gre_mask;
}

static void
add_geneve(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_geneve geneve_spec;
	static struct rte_flow_item_geneve geneve_mask;

	uint32_t vni_value;
	uint8_t i;

	vni_value = VNI_VALUE;

	for (i = 0; i < 3; i++) {
		geneve_spec.vni[2 - i] = vni_value >> (i * 8);
		geneve_mask.vni[2 - i] = 0xff;
	}

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_GENEVE;
	items[items_counter].spec = &geneve_spec;
	items[items_counter].mask = &geneve_mask;
}

static void
add_gtp(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_gtp gtp_spec = {
		.teid = RTE_BE32(TEID_VALUE),
	};
	static struct rte_flow_item_gtp gtp_mask = {
		.teid = RTE_BE32(0xffffffff),
	};

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_GTP;
	items[items_counter].spec = &gtp_spec;
	items[items_counter].mask = &gtp_mask;
}

static void
add_meta_data(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_meta meta_spec = {
		.data = RTE_BE32(META_DATA),
	};
	static struct rte_flow_item_meta meta_mask = {
		.data = RTE_BE32(0xffffffff),
	};

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_META;
	items[items_counter].spec = &meta_spec;
	items[items_counter].mask = &meta_mask;
}


static void
add_meta_tag(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_tag tag_spec = {
		.data = RTE_BE32(META_DATA),
		.index = TAG_INDEX,
	};
	static struct rte_flow_item_tag tag_mask = {
		.data = RTE_BE32(0xffffffff),
		.index = 0xff,
	};

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_TAG;
	items[items_counter].spec = &tag_spec;
	items[items_counter].mask = &tag_mask;
}

static void
add_icmpv4(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_icmp icmpv4_spec;
	static struct rte_flow_item_icmp icmpv4_mask;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_ICMP;
	items[items_counter].spec = &icmpv4_spec;
	items[items_counter].mask = &icmpv4_mask;
}

static void
add_icmpv6(struct rte_flow_item *items,
	uint8_t items_counter,
	__rte_unused struct additional_para para)
{
	static struct rte_flow_item_icmp6 icmpv6_spec;
	static struct rte_flow_item_icmp6 icmpv6_mask;

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_ICMP6;
	items[items_counter].spec = &icmpv6_spec;
	items[items_counter].mask = &icmpv6_mask;
}

void
fill_items(struct rte_flow_item *items,
	uint64_t *flow_items, uint32_t outer_ip_src)
{
	uint8_t items_counter = 0;
	uint8_t i, j;
	struct additional_para additional_para_data = {
		.src_ip = outer_ip_src,
	};

	/* Support outer items up to tunnel layer only. */
	static const struct items_dict {
		uint64_t mask;
		void (*funct)(
			struct rte_flow_item *items,
			uint8_t items_counter,
			struct additional_para para
			);
	} items_list[] = {
		{
			.mask = RTE_FLOW_ITEM_TYPE_META,
			.funct = add_meta_data,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_TAG,
			.funct = add_meta_tag,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_ETH,
			.funct = add_ether,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_VLAN,
			.funct = add_vlan,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_IPV4,
			.funct = add_ipv4,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_IPV6,
			.funct = add_ipv6,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_TCP,
			.funct = add_tcp,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_UDP,
			.funct = add_udp,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_VXLAN,
			.funct = add_vxlan,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
			.funct = add_vxlan_gpe,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_GRE,
			.funct = add_gre,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_GENEVE,
			.funct = add_geneve,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_GTP,
			.funct = add_gtp,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_ICMP,
			.funct = add_icmpv4,
		},
		{
			.mask = RTE_FLOW_ITEM_TYPE_ICMP6,
			.funct = add_icmpv6,
		},
	};

	for (j = 0; j < MAX_ITEMS_NUM; j++) {
		if (flow_items[j] == 0)
			break;
		for (i = 0; i < RTE_DIM(items_list); i++) {
			if ((flow_items[j] &
				FLOW_ITEM_MASK(items_list[i].mask)) == 0)
				continue;
			items_list[i].funct(
				items, items_counter++,
				additional_para_data
			);
			break;
		}
	}

	items[items_counter].type = RTE_FLOW_ITEM_TYPE_END;
}
