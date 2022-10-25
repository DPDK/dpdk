/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include <arpa/inet.h>

#include "mrvl_flow.h"
#include "mrvl_qos.h"

/** Number of rules in the classifier table. */
#define MRVL_CLS_MAX_NUM_RULES 20

/** Size of the classifier key and mask strings. */
#define MRVL_CLS_STR_SIZE_MAX 40

#define MRVL_VLAN_ID_MASK 0x0fff
#define MRVL_VLAN_PRI_MASK 0x7000
#define MRVL_IPV4_DSCP_MASK 0xfc
#define MRVL_IPV4_ADDR_MASK 0xffffffff
#define MRVL_IPV6_FLOW_MASK 0x0fffff

/**
 * Allocate memory for classifier rule key and mask fields.
 *
 * @param field Pointer to the classifier rule.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_alloc_key_mask(struct pp2_cls_rule_key_field *field)
{
	unsigned int id = rte_socket_id();

	field->key = rte_zmalloc_socket(NULL, MRVL_CLS_STR_SIZE_MAX, 0, id);
	if (!field->key)
		goto out;

	field->mask = rte_zmalloc_socket(NULL, MRVL_CLS_STR_SIZE_MAX, 0, id);
	if (!field->mask)
		goto out_mask;

	return 0;
out_mask:
	rte_free(field->key);
out:
	field->key = NULL;
	field->mask = NULL;
	return -1;
}

/**
 * Free memory allocated for classifier rule key and mask fields.
 *
 * @param field Pointer to the classifier rule.
 */
static void
mrvl_free_key_mask(struct pp2_cls_rule_key_field *field)
{
	rte_free(field->key);
	rte_free(field->mask);
	field->key = NULL;
	field->mask = NULL;
}

/**
 * Free memory allocated for all classifier rule key and mask fields.
 *
 * @param rule Pointer to the classifier table rule.
 */
static void
mrvl_free_all_key_mask(struct pp2_cls_tbl_rule *rule)
{
	int i;

	for (i = 0; i < rule->num_fields; i++)
		mrvl_free_key_mask(&rule->fields[i]);
	rule->num_fields = 0;
}

/*
 * Initialize rte flow item parsing.
 *
 * @param item Pointer to the flow item.
 * @param spec_ptr Pointer to the specific item pointer.
 * @param mask_ptr Pointer to the specific item's mask pointer.
 * @def_mask Pointer to the default mask.
 * @size Size of the flow item.
 * @error Pointer to the rte flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_parse_init(const struct rte_flow_item *item,
		const void **spec_ptr,
		const void **mask_ptr,
		const void *def_mask,
		unsigned int size,
		struct rte_flow_error *error)
{
	const uint8_t *spec;
	const uint8_t *mask;
	const uint8_t *last;
	uint8_t zeros[size];

	memset(zeros, 0, size);

	if (item == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "NULL item\n");
		return -rte_errno;
	}

	if ((item->last != NULL || item->mask != NULL) && item->spec == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Mask or last is set without spec\n");
		return -rte_errno;
	}

	/*
	 * If "mask" is not set, default mask is used,
	 * but if default mask is NULL, "mask" should be set.
	 */
	if (item->mask == NULL) {
		if (def_mask == NULL) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					   "Mask should be specified\n");
			return -rte_errno;
		}

		mask = (const uint8_t *)def_mask;
	} else {
		mask = (const uint8_t *)item->mask;
	}

	spec = (const uint8_t *)item->spec;
	last = (const uint8_t *)item->last;

	if (spec == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Spec should be specified\n");
		return -rte_errno;
	}

	/*
	 * If field values in "last" are either 0 or equal to the corresponding
	 * values in "spec" then they are ignored.
	 */
	if (last != NULL &&
	    !memcmp(last, zeros, size) &&
	    memcmp(last, spec, size) != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "Ranging is not supported\n");
		return -rte_errno;
	}

	*spec_ptr = spec;
	*mask_ptr = mask;

	return 0;
}

/**
 * Parse the eth flow item.
 *
 * This will create classifier rule that matches either destination or source
 * mac.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param parse_dst Parse either destination or source mac address.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_mac(const struct rte_flow_item_eth *spec,
	       const struct rte_flow_item_eth *mask,
	       int parse_dst, struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	const uint8_t *k, *m;

	if (parse_dst) {
		k = spec->hdr.dst_addr.addr_bytes;
		m = mask->hdr.dst_addr.addr_bytes;

		flow->table_key.proto_field[flow->rule.num_fields].field.eth =
			MV_NET_ETH_F_DA;
	} else {
		k = spec->hdr.src_addr.addr_bytes;
		m = mask->hdr.src_addr.addr_bytes;

		flow->table_key.proto_field[flow->rule.num_fields].field.eth =
			MV_NET_ETH_F_SA;
	}

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 6;

	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX,
		 RTE_ETHER_ADDR_PRT_FMT,
		 k[0], k[1], k[2], k[3], k[4], k[5]);

	snprintf((char *)key_field->mask, MRVL_CLS_STR_SIZE_MAX,
		 RTE_ETHER_ADDR_PRT_FMT,
		 m[0], m[1], m[2], m[3], m[4], m[5]);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_ETH;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Helper for parsing the eth flow item destination mac address.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_dmac(const struct rte_flow_item_eth *spec,
		const struct rte_flow_item_eth *mask,
		struct rte_flow *flow)
{
	return mrvl_parse_mac(spec, mask, 1, flow);
}

/**
 * Helper for parsing the eth flow item source mac address.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_smac(const struct rte_flow_item_eth *spec,
		const struct rte_flow_item_eth *mask,
		struct rte_flow *flow)
{
	return mrvl_parse_mac(spec, mask, 0, flow);
}

/**
 * Parse the ether type field of the eth flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_type(const struct rte_flow_item_eth *spec,
		const struct rte_flow_item_eth *mask __rte_unused,
		struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint16_t k;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 2;

	k = rte_be_to_cpu_16(spec->hdr.ether_type);
	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_ETH;
	flow->table_key.proto_field[flow->rule.num_fields].field.eth =
		MV_NET_ETH_F_TYPE;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse the vid field of the vlan rte flow item.
 *
 * This will create classifier rule that matches vid.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_vlan_id(const struct rte_flow_item_vlan *spec,
		   const struct rte_flow_item_vlan *mask __rte_unused,
		   struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint16_t k;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 2;

	k = rte_be_to_cpu_16(spec->hdr.vlan_tci) & MRVL_VLAN_ID_MASK;
	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_VLAN;
	flow->table_key.proto_field[flow->rule.num_fields].field.vlan =
		MV_NET_VLAN_F_ID;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse the pri field of the vlan rte flow item.
 *
 * This will create classifier rule that matches pri.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_vlan_pri(const struct rte_flow_item_vlan *spec,
		    const struct rte_flow_item_vlan *mask __rte_unused,
		    struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint16_t k;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 1;

	k = (rte_be_to_cpu_16(spec->hdr.vlan_tci) & MRVL_VLAN_PRI_MASK) >> 13;
	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_VLAN;
	flow->table_key.proto_field[flow->rule.num_fields].field.vlan =
		MV_NET_VLAN_F_PRI;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse the dscp field of the ipv4 rte flow item.
 *
 * This will create classifier rule that matches dscp field.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_ip4_dscp(const struct rte_flow_item_ipv4 *spec,
		    const struct rte_flow_item_ipv4 *mask,
		    struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint8_t k, m;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 1;

	k = (spec->hdr.type_of_service & MRVL_IPV4_DSCP_MASK) >> 2;
	m = (mask->hdr.type_of_service & MRVL_IPV4_DSCP_MASK) >> 2;
	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);
	snprintf((char *)key_field->mask, MRVL_CLS_STR_SIZE_MAX, "%u", m);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_IP4;
	flow->table_key.proto_field[flow->rule.num_fields].field.ipv4 =
		MV_NET_IP4_F_DSCP;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse either source or destination ip addresses of the ipv4 flow item.
 *
 * This will create classifier rule that matches either destination
 * or source ip field.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param parse_dst Parse either destination or source ip address.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_ip4_addr(const struct rte_flow_item_ipv4 *spec,
		    const struct rte_flow_item_ipv4 *mask,
		    int parse_dst, struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	struct in_addr k;
	uint32_t m;

	memset(&k, 0, sizeof(k));
	if (parse_dst) {
		k.s_addr = spec->hdr.dst_addr;
		m = rte_be_to_cpu_32(mask->hdr.dst_addr);

		flow->table_key.proto_field[flow->rule.num_fields].field.ipv4 =
			MV_NET_IP4_F_DA;
	} else {
		k.s_addr = spec->hdr.src_addr;
		m = rte_be_to_cpu_32(mask->hdr.src_addr);

		flow->table_key.proto_field[flow->rule.num_fields].field.ipv4 =
			MV_NET_IP4_F_SA;
	}

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 4;

	inet_ntop(AF_INET, &k, (char *)key_field->key, MRVL_CLS_STR_SIZE_MAX);
	snprintf((char *)key_field->mask, MRVL_CLS_STR_SIZE_MAX, "0x%x", m);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_IP4;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Helper for parsing destination ip of the ipv4 flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_ip4_dip(const struct rte_flow_item_ipv4 *spec,
		   const struct rte_flow_item_ipv4 *mask,
		   struct rte_flow *flow)
{
	return mrvl_parse_ip4_addr(spec, mask, 1, flow);
}

/**
 * Helper for parsing source ip of the ipv4 flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_ip4_sip(const struct rte_flow_item_ipv4 *spec,
		   const struct rte_flow_item_ipv4 *mask,
		   struct rte_flow *flow)
{
	return mrvl_parse_ip4_addr(spec, mask, 0, flow);
}

/**
 * Parse the proto field of the ipv4 rte flow item.
 *
 * This will create classifier rule that matches proto field.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_ip4_proto(const struct rte_flow_item_ipv4 *spec,
		     const struct rte_flow_item_ipv4 *mask __rte_unused,
		     struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint8_t k = spec->hdr.next_proto_id;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 1;

	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_IP4;
	flow->table_key.proto_field[flow->rule.num_fields].field.ipv4 =
		MV_NET_IP4_F_PROTO;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse either source or destination ip addresses of the ipv6 rte flow item.
 *
 * This will create classifier rule that matches either destination
 * or source ip field.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param parse_dst Parse either destination or source ipv6 address.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_ip6_addr(const struct rte_flow_item_ipv6 *spec,
	       const struct rte_flow_item_ipv6 *mask,
	       int parse_dst, struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	int size = sizeof(spec->hdr.dst_addr);
	struct in6_addr k, m;

	memset(&k, 0, sizeof(k));
	if (parse_dst) {
		memcpy(k.s6_addr, spec->hdr.dst_addr, size);
		memcpy(m.s6_addr, mask->hdr.dst_addr, size);

		flow->table_key.proto_field[flow->rule.num_fields].field.ipv6 =
			MV_NET_IP6_F_DA;
	} else {
		memcpy(k.s6_addr, spec->hdr.src_addr, size);
		memcpy(m.s6_addr, mask->hdr.src_addr, size);

		flow->table_key.proto_field[flow->rule.num_fields].field.ipv6 =
			MV_NET_IP6_F_SA;
	}

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 16;

	inet_ntop(AF_INET6, &k, (char *)key_field->key, MRVL_CLS_STR_SIZE_MAX);
	inet_ntop(AF_INET6, &m, (char *)key_field->mask, MRVL_CLS_STR_SIZE_MAX);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_IP6;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Helper for parsing destination ip of the ipv6 flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_ip6_dip(const struct rte_flow_item_ipv6 *spec,
		   const struct rte_flow_item_ipv6 *mask,
		   struct rte_flow *flow)
{
	return mrvl_parse_ip6_addr(spec, mask, 1, flow);
}

/**
 * Helper for parsing source ip of the ipv6 flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_ip6_sip(const struct rte_flow_item_ipv6 *spec,
		   const struct rte_flow_item_ipv6 *mask,
		   struct rte_flow *flow)
{
	return mrvl_parse_ip6_addr(spec, mask, 0, flow);
}

/**
 * Parse the flow label of the ipv6 flow item.
 *
 * This will create classifier rule that matches flow field.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_ip6_flow(const struct rte_flow_item_ipv6 *spec,
		    const struct rte_flow_item_ipv6 *mask,
		    struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint32_t k = rte_be_to_cpu_32(spec->hdr.vtc_flow) & MRVL_IPV6_FLOW_MASK,
		 m = rte_be_to_cpu_32(mask->hdr.vtc_flow) & MRVL_IPV6_FLOW_MASK;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 3;

	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);
	snprintf((char *)key_field->mask, MRVL_CLS_STR_SIZE_MAX, "%u", m);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_IP6;
	flow->table_key.proto_field[flow->rule.num_fields].field.ipv6 =
		MV_NET_IP6_F_FLOW;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse the next header of the ipv6 flow item.
 *
 * This will create classifier rule that matches next header field.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_ip6_next_hdr(const struct rte_flow_item_ipv6 *spec,
			const struct rte_flow_item_ipv6 *mask __rte_unused,
			struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint8_t k = spec->hdr.proto;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 1;

	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_IP6;
	flow->table_key.proto_field[flow->rule.num_fields].field.ipv6 =
		MV_NET_IP6_F_NEXT_HDR;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Parse destination or source port of the tcp flow item.
 *
 * This will create classifier rule that matches either destination or
 * source tcp port.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param parse_dst Parse either destination or source port.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_tcp_port(const struct rte_flow_item_tcp *spec,
		    const struct rte_flow_item_tcp *mask __rte_unused,
		    int parse_dst, struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint16_t k;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 2;

	if (parse_dst) {
		k = rte_be_to_cpu_16(spec->hdr.dst_port);

		flow->table_key.proto_field[flow->rule.num_fields].field.tcp =
			MV_NET_TCP_F_DP;
	} else {
		k = rte_be_to_cpu_16(spec->hdr.src_port);

		flow->table_key.proto_field[flow->rule.num_fields].field.tcp =
			MV_NET_TCP_F_SP;
	}

	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_TCP;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Helper for parsing the tcp source port of the tcp flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_tcp_sport(const struct rte_flow_item_tcp *spec,
		     const struct rte_flow_item_tcp *mask,
		     struct rte_flow *flow)
{
	return mrvl_parse_tcp_port(spec, mask, 0, flow);
}

/**
 * Helper for parsing the tcp destination port of the tcp flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_tcp_dport(const struct rte_flow_item_tcp *spec,
		     const struct rte_flow_item_tcp *mask,
		     struct rte_flow *flow)
{
	return mrvl_parse_tcp_port(spec, mask, 1, flow);
}

/**
 * Parse destination or source port of the udp flow item.
 *
 * This will create classifier rule that matches either destination or
 * source udp port.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param parse_dst Parse either destination or source port.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static int
mrvl_parse_udp_port(const struct rte_flow_item_udp *spec,
		    const struct rte_flow_item_udp *mask __rte_unused,
		    int parse_dst, struct rte_flow *flow)
{
	struct pp2_cls_rule_key_field *key_field;
	uint16_t k;

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);
	key_field->size = 2;

	if (parse_dst) {
		k = rte_be_to_cpu_16(spec->hdr.dst_port);

		flow->table_key.proto_field[flow->rule.num_fields].field.udp =
			MV_NET_UDP_F_DP;
	} else {
		k = rte_be_to_cpu_16(spec->hdr.src_port);

		flow->table_key.proto_field[flow->rule.num_fields].field.udp =
			MV_NET_UDP_F_SP;
	}

	snprintf((char *)key_field->key, MRVL_CLS_STR_SIZE_MAX, "%u", k);

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_PROTO_UDP;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Helper for parsing the udp source port of the udp flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_udp_sport(const struct rte_flow_item_udp *spec,
		     const struct rte_flow_item_udp *mask,
		     struct rte_flow *flow)
{
	return mrvl_parse_udp_port(spec, mask, 0, flow);
}

/**
 * Helper for parsing the udp destination port of the udp flow item.
 *
 * @param spec Pointer to the specific flow item.
 * @param mask Pointer to the specific flow item's mask.
 * @param flow Pointer to the flow.
 * @return 0 in case of success, negative error value otherwise.
 */
static inline int
mrvl_parse_udp_dport(const struct rte_flow_item_udp *spec,
		     const struct rte_flow_item_udp *mask,
		     struct rte_flow *flow)
{
	return mrvl_parse_udp_port(spec, mask, 1, flow);
}

/**
 * Parse eth flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_eth(const struct rte_flow_item *item, struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *spec = NULL, *mask = NULL;
	struct rte_ether_addr zero;
	int ret;

	ret = mrvl_parse_init(item, (const void **)&spec, (const void **)&mask,
			      &rte_flow_item_eth_mask,
			      sizeof(struct rte_flow_item_eth), error);
	if (ret)
		return ret;

	memset(&zero, 0, sizeof(zero));

	if (memcmp(&mask->hdr.dst_addr, &zero, sizeof(mask->hdr.dst_addr))) {
		ret = mrvl_parse_dmac(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (memcmp(&mask->hdr.src_addr, &zero, sizeof(mask->hdr.src_addr))) {
		ret = mrvl_parse_smac(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.ether_type) {
		MRVL_LOG(WARNING, "eth type mask is ignored");
		ret = mrvl_parse_type(spec, mask, flow);
		if (ret)
			goto out;
	}

	return 0;
out:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Reached maximum number of fields in cls tbl key\n");
	return -rte_errno;
}

/**
 * Parse vlan flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_vlan(const struct rte_flow_item *item,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *spec = NULL, *mask = NULL;
	uint16_t m;
	int ret, i;

	ret = mrvl_parse_init(item, (const void **)&spec, (const void **)&mask,
			      &rte_flow_item_vlan_mask,
			      sizeof(struct rte_flow_item_vlan), error);
	if (ret)
		return ret;

	m = rte_be_to_cpu_16(mask->hdr.vlan_tci);
	if (m & MRVL_VLAN_ID_MASK) {
		MRVL_LOG(WARNING, "vlan id mask is ignored");
		ret = mrvl_parse_vlan_id(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (m & MRVL_VLAN_PRI_MASK) {
		MRVL_LOG(WARNING, "vlan pri mask is ignored");
		ret = mrvl_parse_vlan_pri(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.eth_proto) {
		struct rte_flow_item_eth spec_eth = {
			.hdr.ether_type = spec->hdr.eth_proto,
		};
		struct rte_flow_item_eth mask_eth = {
			.hdr.ether_type = mask->hdr.eth_proto,
		};

		/* TPID is not supported so if ETH_TYPE was selected,
		 * error is return. else, classify eth-type with the tpid value
		 */
		for (i = 0; i < flow->rule.num_fields; i++)
			if (flow->table_key.proto_field[i].proto ==
			    MV_NET_PROTO_ETH &&
			    flow->table_key.proto_field[i].field.eth ==
			    MV_NET_ETH_F_TYPE) {
				rte_flow_error_set(error, ENOTSUP,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "VLAN TPID matching is not supported");
				return -rte_errno;
			}

		MRVL_LOG(WARNING, "inner eth type mask is ignored");
		ret = mrvl_parse_type(&spec_eth, &mask_eth, flow);
		if (ret)
			goto out;
	}

	return 0;
out:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Reached maximum number of fields in cls tbl key\n");
	return -rte_errno;
}

/**
 * Parse ipv4 flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_ip4(const struct rte_flow_item *item,
	       struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *spec = NULL, *mask = NULL;
	int ret;

	ret = mrvl_parse_init(item, (const void **)&spec, (const void **)&mask,
			      &rte_flow_item_ipv4_mask,
			      sizeof(struct rte_flow_item_ipv4), error);
	if (ret)
		return ret;

	if (mask->hdr.version_ihl ||
	    mask->hdr.total_length ||
	    mask->hdr.packet_id ||
	    mask->hdr.fragment_offset ||
	    mask->hdr.time_to_live ||
	    mask->hdr.hdr_checksum) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by classifier\n");
		return -rte_errno;
	}

	if (mask->hdr.type_of_service & MRVL_IPV4_DSCP_MASK) {
		ret = mrvl_parse_ip4_dscp(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.src_addr) {
		ret = mrvl_parse_ip4_sip(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.dst_addr) {
		ret = mrvl_parse_ip4_dip(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.next_proto_id) {
		MRVL_LOG(WARNING, "next proto id mask is ignored");
		ret = mrvl_parse_ip4_proto(spec, mask, flow);
		if (ret)
			goto out;
	}

	return 0;
out:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Reached maximum number of fields in cls tbl key\n");
	return -rte_errno;
}

/**
 * Parse ipv6 flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_ip6(const struct rte_flow_item *item,
	       struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *spec = NULL, *mask = NULL;
	struct rte_ipv6_hdr zero;
	uint32_t flow_mask;
	int ret;

	ret = mrvl_parse_init(item, (const void **)&spec,
			      (const void **)&mask,
			      &rte_flow_item_ipv6_mask,
			      sizeof(struct rte_flow_item_ipv6),
			      error);
	if (ret)
		return ret;

	memset(&zero, 0, sizeof(zero));

	if (mask->hdr.payload_len ||
	    mask->hdr.hop_limits) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by classifier\n");
		return -rte_errno;
	}

	if (memcmp(mask->hdr.src_addr,
		   zero.src_addr, sizeof(mask->hdr.src_addr))) {
		ret = mrvl_parse_ip6_sip(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (memcmp(mask->hdr.dst_addr,
		   zero.dst_addr, sizeof(mask->hdr.dst_addr))) {
		ret = mrvl_parse_ip6_dip(spec, mask, flow);
		if (ret)
			goto out;
	}

	flow_mask = rte_be_to_cpu_32(mask->hdr.vtc_flow) & MRVL_IPV6_FLOW_MASK;
	if (flow_mask) {
		ret = mrvl_parse_ip6_flow(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.proto) {
		MRVL_LOG(WARNING, "next header mask is ignored");
		ret = mrvl_parse_ip6_next_hdr(spec, mask, flow);
		if (ret)
			goto out;
	}

	return 0;
out:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Reached maximum number of fields in cls tbl key\n");
	return -rte_errno;
}

/**
 * Parse tcp flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_tcp(const struct rte_flow_item *item,
	       struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *spec = NULL, *mask = NULL;
	int ret;

	ret = mrvl_parse_init(item, (const void **)&spec, (const void **)&mask,
			      &rte_flow_item_tcp_mask,
			      sizeof(struct rte_flow_item_tcp), error);
	if (ret)
		return ret;

	if (mask->hdr.sent_seq ||
	    mask->hdr.recv_ack ||
	    mask->hdr.data_off ||
	    mask->hdr.tcp_flags ||
	    mask->hdr.rx_win ||
	    mask->hdr.cksum ||
	    mask->hdr.tcp_urp) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by classifier\n");
		return -rte_errno;
	}

	if (mask->hdr.src_port) {
		MRVL_LOG(WARNING, "tcp sport mask is ignored");
		ret = mrvl_parse_tcp_sport(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.dst_port) {
		MRVL_LOG(WARNING, "tcp dport mask is ignored");
		ret = mrvl_parse_tcp_dport(spec, mask, flow);
		if (ret)
			goto out;
	}

	return 0;
out:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Reached maximum number of fields in cls tbl key\n");
	return -rte_errno;
}

/**
 * Parse udp flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_udp(const struct rte_flow_item *item,
	       struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = NULL, *mask = NULL;
	int ret;

	ret = mrvl_parse_init(item, (const void **)&spec, (const void **)&mask,
			      &rte_flow_item_udp_mask,
			      sizeof(struct rte_flow_item_udp), error);
	if (ret)
		return ret;

	if (mask->hdr.dgram_len ||
	    mask->hdr.dgram_cksum) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "Not supported by classifier\n");
		return -rte_errno;
	}

	if (mask->hdr.src_port) {
		MRVL_LOG(WARNING, "udp sport mask is ignored");
		ret = mrvl_parse_udp_sport(spec, mask, flow);
		if (ret)
			goto out;
	}

	if (mask->hdr.dst_port) {
		MRVL_LOG(WARNING, "udp dport mask is ignored");
		ret = mrvl_parse_udp_dport(spec, mask, flow);
		if (ret)
			goto out;
	}

	return 0;
out:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Reached maximum number of fields in cls tbl key\n");
	return -rte_errno;
}

static int
mrvl_string_to_hex_values(const uint8_t *input_string,
			  uint8_t *hex_key,
			  uint8_t *length)
{
	char tmp_arr[3], tmp_string[MRVL_CLS_STR_SIZE_MAX], *string_iter;
	int i;

	strcpy(tmp_string, (const char *)input_string);
	string_iter = tmp_string;

	string_iter += 2; /* skip the '0x' */
	*length = ((*length - 2) + 1) / 2;

	for (i = 0; i < *length; i++) {
		strncpy(tmp_arr, string_iter, 2);
		tmp_arr[2] = '\0';
		if (get_val_securely8(tmp_arr, 16,
				      &hex_key[*length - 1 - i]) < 0)
			return -1;
		string_iter += 2;
	}

	return 0;
}

/**
 * Parse raw flow item.
 *
 * @param item Pointer to the flow item.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_parse_raw(const struct rte_flow_item *item,
	       struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_raw *spec = NULL, *mask = NULL;
	struct pp2_cls_rule_key_field *key_field;
	struct mv_net_udf *udf_params;
	uint8_t length;
	int ret;

	ret = mrvl_parse_init(item, (const void **)&spec, (const void **)&mask,
			      &rte_flow_item_raw_mask,
			      sizeof(struct rte_flow_item_raw), error);
	if (ret)
		return ret;

	if (!spec->pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "pattern pointer MUST be given\n");
		return -rte_errno;
	}

	/* Only hex string is supported; so, it must start with '0x' */
	if (strncmp((const char *)spec->pattern, "0x", 2) != 0)  {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "'pattern' string must start with '0x'\n");
		return -rte_errno;
	}

	if (mask->pattern &&
	    strncmp((const char *)mask->pattern, "0x", 2) != 0)  {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "'mask-pattern' string must start with '0x'\n");
		return -rte_errno;
	}

	if (mask->search && spec->search) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "'search' option must be '0'\n");
		return -rte_errno;
	}

	if (mask->offset && spec->offset != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "'offset' option must be '0'\n");
		return -rte_errno;
	}

	if (!mask->relative || !spec->relative) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "'relative' option must be given and enabled\n");
		return -rte_errno;
	}

	length = spec->length & mask->length;
	if (!length) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL, "'length' option must be given bigger than '0'\n");
		return -rte_errno;
	}

	key_field = &flow->rule.fields[flow->rule.num_fields];
	mrvl_alloc_key_mask(key_field);

	/* pattern and length refer to string bytes. we need to convert it to
	 * values.
	 */
	key_field->size = length;
	ret = mrvl_string_to_hex_values(spec->pattern, key_field->key,
					&key_field->size);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   NULL,
				   "can't convert pattern from string to hex\n");
		return -rte_errno;
	}
	if (mask->pattern) {
		ret = mrvl_string_to_hex_values(mask->pattern, key_field->mask,
						&length);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					   NULL,
					   "can't convert mask-pattern from string to hex\n");
			return -rte_errno;
		}
	} else {
		rte_free(key_field->mask);
		key_field->mask = NULL;
	}

	flow->table_key.proto_field[flow->rule.num_fields].proto =
		MV_NET_UDF;
	udf_params =
		&flow->table_key.proto_field[flow->rule.num_fields].field.udf;
	udf_params->id = flow->next_udf_id++;
	udf_params->size = key_field->size;
	flow->table_key.key_size += key_field->size;

	flow->rule.num_fields += 1;

	return 0;
}

/**
 * Structure used to map specific flow pattern to the pattern parse callback
 * which will iterate over each pattern item and extract relevant data.
 */
static const struct {
	const enum rte_flow_item_type pattern_type;
	int (*parse)(const struct rte_flow_item *pattern,
		struct rte_flow *flow,
		struct rte_flow_error *error);
} mrvl_patterns[] = {
	{ RTE_FLOW_ITEM_TYPE_ETH, mrvl_parse_eth },
	{ RTE_FLOW_ITEM_TYPE_VLAN, mrvl_parse_vlan },
	{ RTE_FLOW_ITEM_TYPE_IPV4, mrvl_parse_ip4 },
	{ RTE_FLOW_ITEM_TYPE_IPV6, mrvl_parse_ip6 },
	{ RTE_FLOW_ITEM_TYPE_TCP, mrvl_parse_tcp },
	{ RTE_FLOW_ITEM_TYPE_UDP, mrvl_parse_udp },
	{ RTE_FLOW_ITEM_TYPE_RAW, mrvl_parse_raw },
	{ RTE_FLOW_ITEM_TYPE_END, NULL }
};

/**
 * Parse flow attribute.
 *
 * This will check whether the provided attribute's flags are supported.
 *
 * @param priv Unused
 * @param attr Pointer to the flow attribute.
 * @param flow Unused
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_parse_attr(struct mrvl_priv *priv __rte_unused,
		     const struct rte_flow_attr *attr,
		     struct rte_flow *flow __rte_unused,
		     struct rte_flow_error *error)
{
	if (!attr) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute");
		return -rte_errno;
	}

	if (attr->group) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL,
				   "Groups are not supported");
		return -rte_errno;
	}
	if (attr->priority) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, NULL,
				   "Priorities are not supported");
		return -rte_errno;
	}
	if (!attr->ingress) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, NULL,
				   "Only ingress is supported");
		return -rte_errno;
	}
	if (attr->egress) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
				   "Egress is not supported");
		return -rte_errno;
	}
	if (attr->transfer) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER, NULL,
				   "Transfer is not supported");
		return -rte_errno;
	}

	return 0;
}

/**
 * Parse flow pattern.
 *
 * Specific classifier rule will be created as well.
 *
 * @param priv Unused
 * @param pattern Pointer to the flow pattern.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_parse_pattern(struct mrvl_priv *priv __rte_unused,
			const struct rte_flow_item pattern[],
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	unsigned int i, j;
	int ret;

	for (i = 0; pattern[i].type != RTE_FLOW_ITEM_TYPE_END; i++) {
		if (pattern[i].type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		for (j = 0; mrvl_patterns[j].pattern_type !=
			RTE_FLOW_ITEM_TYPE_END; j++) {
			if (mrvl_patterns[j].pattern_type != pattern[i].type)
				continue;

			if (flow->rule.num_fields >=
			    PP2_CLS_TBL_MAX_NUM_FIELDS) {
				rte_flow_error_set(error, ENOSPC,
						   RTE_FLOW_ERROR_TYPE_ITEM_NUM,
						   NULL,
						   "too many pattern (max %d)");
				return -rte_errno;
			}

			ret = mrvl_patterns[j].parse(&pattern[i], flow, error);
			if (ret) {
				mrvl_free_all_key_mask(&flow->rule);
				return ret;
			}
			break;
		}
		if (mrvl_patterns[j].pattern_type == RTE_FLOW_ITEM_TYPE_END) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					   "Unsupported pattern");
			return -rte_errno;
		}
	}

	flow->table_key.num_fields = flow->rule.num_fields;

	return 0;
}

/**
 * Parse flow actions.
 *
 * @param priv Pointer to the port's private data.
 * @param actions Pointer the action table.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_parse_actions(struct mrvl_priv *priv,
			const struct rte_flow_action actions[],
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	const struct rte_flow_action *action = actions;
	int specified = 0;

	for (; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		if (action->type == RTE_FLOW_ACTION_TYPE_VOID)
			continue;

		if (action->type == RTE_FLOW_ACTION_TYPE_DROP) {
			flow->cos.ppio = priv->ppio;
			flow->cos.tc = 0;
			flow->action.type = PP2_CLS_TBL_ACT_DROP;
			flow->action.cos = &flow->cos;
			specified++;
		} else if (action->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *q =
				(const struct rte_flow_action_queue *)
				action->conf;

			if (q->index > priv->nb_rx_queues) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Queue index out of range");
				return -rte_errno;
			}

			if (priv->rxq_map[q->index].tc == MRVL_UNKNOWN_TC) {
				/*
				 * Unknown TC mapping, mapping will not have
				 * a correct queue.
				 */
				MRVL_LOG(ERR,
					"Unknown TC mapping for queue %hu eth%hhu",
					q->index, priv->ppio_id);

				rte_flow_error_set(error, EFAULT,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						NULL, NULL);
				return -rte_errno;
			}

			MRVL_LOG(DEBUG,
				"Action: Assign packets to queue %d, tc:%d, q:%d",
				q->index, priv->rxq_map[q->index].tc,
				priv->rxq_map[q->index].inq);

			flow->cos.ppio = priv->ppio;
			flow->cos.tc = priv->rxq_map[q->index].tc;
			flow->action.type = PP2_CLS_TBL_ACT_DONE;
			flow->action.cos = &flow->cos;
			specified++;
		} else if (action->type == RTE_FLOW_ACTION_TYPE_METER) {
			const struct rte_flow_action_meter *meter;
			struct mrvl_mtr *mtr;

			meter = action->conf;
			if (!meter)
				return -rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL, "Invalid meter\n");

			LIST_FOREACH(mtr, &priv->mtrs, next)
				if (mtr->mtr_id == meter->mtr_id)
					break;

			if (!mtr)
				return -rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Meter id does not exist\n");

			if (!mtr->shared && mtr->refcnt)
				return -rte_flow_error_set(error, EPERM,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Meter cannot be shared\n");

			/*
			 * In case cos has already been set
			 * do not modify it.
			 */
			if (!flow->cos.ppio) {
				flow->cos.ppio = priv->ppio;
				flow->cos.tc = 0;
			}

			flow->action.type = PP2_CLS_TBL_ACT_DONE;
			flow->action.cos = &flow->cos;
			flow->action.plcr = mtr->enabled ? mtr->plcr : NULL;
			flow->mtr = mtr;
			mtr->refcnt++;
			specified++;
		} else {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					   "Action not supported");
			return -rte_errno;
		}
	}

	if (!specified) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Action not specified");
		return -rte_errno;
	}

	return 0;
}

/**
 * Parse flow attribute, pattern and actions.
 *
 * @param priv Pointer to the port's private data.
 * @param attr Pointer to the flow attribute.
 * @param pattern Pointer to the flow pattern.
 * @param actions Pointer to the flow actions.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_flow_parse(struct mrvl_priv *priv, const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	int ret;

	ret = mrvl_flow_parse_attr(priv, attr, flow, error);
	if (ret)
		return ret;

	ret = mrvl_flow_parse_pattern(priv, pattern, flow, error);
	if (ret)
		return ret;

	return mrvl_flow_parse_actions(priv, actions, flow, error);
}

/**
 * Get engine type for the given flow.
 *
 * @param field Pointer to the flow.
 * @returns The type of the engine.
 */
static inline enum pp2_cls_tbl_type
mrvl_engine_type(const struct rte_flow *flow)
{
	int i, size = 0;

	for (i = 0; i < flow->rule.num_fields; i++)
		size += flow->rule.fields[i].size;

	/*
	 * For maskable engine type the key size must be up to 8 bytes.
	 * For keys with size bigger than 8 bytes, engine type must
	 * be set to exact match.
	 */
	if (size > 8)
		return PP2_CLS_TBL_EXACT_MATCH;

	return PP2_CLS_TBL_MASKABLE;
}

/**
 * Create classifier table.
 *
 * @param dev Pointer to the device.
 * @param flow Pointer to the very first flow.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_create_cls_table(struct rte_eth_dev *dev, struct rte_flow *first_flow)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct pp2_cls_tbl_key *key = &priv->cls_tbl_params.key;
	int ret;

	if (priv->cls_tbl) {
		pp2_cls_tbl_deinit(priv->cls_tbl);
		priv->cls_tbl = NULL;
	}

	memset(&priv->cls_tbl_params, 0, sizeof(priv->cls_tbl_params));

	priv->cls_tbl_params.type = mrvl_engine_type(first_flow);
	MRVL_LOG(INFO, "Setting cls search engine type to %s",
			priv->cls_tbl_params.type == PP2_CLS_TBL_EXACT_MATCH ?
			"exact" : "maskable");
	priv->cls_tbl_params.max_num_rules = MRVL_CLS_MAX_NUM_RULES;
	priv->cls_tbl_params.default_act.type = PP2_CLS_TBL_ACT_DONE;
	priv->cls_tbl_params.default_act.cos = &first_flow->cos;
	memcpy(key, &first_flow->table_key, sizeof(struct pp2_cls_tbl_key));

	ret = pp2_cls_tbl_init(&priv->cls_tbl_params, &priv->cls_tbl);

	return ret;
}

/**
 * Check whether new flow can be added to the table
 *
 * @param priv Pointer to the port's private data.
 * @param flow Pointer to the new flow.
 * @return 1 in case flow can be added, 0 otherwise.
 */
static inline int
mrvl_flow_can_be_added(struct mrvl_priv *priv, const struct rte_flow *flow)
{
	int same = memcmp(&flow->table_key, &priv->cls_tbl_params.key,
			  sizeof(struct pp2_cls_tbl_key)) == 0;

	return same && mrvl_engine_type(flow) == priv->cls_tbl_params.type;
}

/**
 * DPDK flow create callback called when flow is to be created.
 *
 * @param dev Pointer to the device.
 * @param attr Pointer to the flow attribute.
 * @param pattern Pointer to the flow pattern.
 * @param actions Pointer to the flow actions.
 * @param error Pointer to the flow error.
 * @returns Pointer to the created flow in case of success, NULL otherwise.
 */
static struct rte_flow *
mrvl_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct rte_flow *flow, *first;
	int ret;

	if (!dev->data->dev_started) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Port must be started first\n");
		return NULL;
	}

	flow = rte_zmalloc_socket(NULL, sizeof(*flow), 0, rte_socket_id());
	if (!flow)
		return NULL;

	ret = mrvl_flow_parse(priv, attr, pattern, actions, flow, error);
	if (ret)
		goto out;

	/*
	 * Four cases here:
	 *
	 * 1. In case table does not exist - create one.
	 * 2. In case table exists, is empty and new flow cannot be added
	 *    recreate table.
	 * 3. In case table is not empty and new flow matches table format
	 *    add it.
	 * 4. Otherwise flow cannot be added.
	 */
	first = LIST_FIRST(&priv->flows);
	if (!priv->cls_tbl) {
		ret = mrvl_create_cls_table(dev, flow);
	} else if (!first && !mrvl_flow_can_be_added(priv, flow)) {
		ret = mrvl_create_cls_table(dev, flow);
	} else if (mrvl_flow_can_be_added(priv, flow)) {
		ret = 0;
	} else {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Pattern does not match cls table format\n");
		goto out;
	}

	if (ret) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to create cls table\n");
		goto out;
	}

	ret = pp2_cls_tbl_add_rule(priv->cls_tbl, &flow->rule, &flow->action);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to add rule\n");
		goto out;
	}

	LIST_INSERT_HEAD(&priv->flows, flow, next);

	return flow;
out:
	rte_free(flow);
	return NULL;
}

/**
 * Remove classifier rule associated with given flow.
 *
 * @param priv Pointer to the port's private data.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_remove(struct mrvl_priv *priv, struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	int ret;

	if (!priv->cls_tbl) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Classifier table not initialized");
		return -rte_errno;
	}

	ret = pp2_cls_tbl_remove_rule(priv->cls_tbl, &flow->rule);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to remove rule");
		return -rte_errno;
	}

	mrvl_free_all_key_mask(&flow->rule);

	if (flow->mtr) {
		flow->mtr->refcnt--;
		flow->mtr = NULL;
	}

	return 0;
}

/**
 * DPDK flow destroy callback called when flow is to be removed.
 *
 * @param dev Pointer to the device.
 * @param flow Pointer to the flow.
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;
	struct rte_flow *f;
	int ret;

	LIST_FOREACH(f, &priv->flows, next) {
		if (f == flow)
			break;
	}

	if (!flow) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Rule was not found");
		return -rte_errno;
	}

	LIST_REMOVE(f, next);

	ret = mrvl_flow_remove(priv, flow, error);
	if (ret)
		return ret;

	rte_free(flow);

	return 0;
}

/**
 * DPDK flow callback called to verify given attribute, pattern and actions.
 *
 * @param dev Pointer to the device.
 * @param attr Pointer to the flow attribute.
 * @param pattern Pointer to the flow pattern.
 * @param actions Pointer to the flow actions.
 * @param error Pointer to the flow error.
 * @returns 0 on success, negative value otherwise.
 */
static int
mrvl_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	static struct rte_flow *flow;

	flow = mrvl_flow_create(dev, attr, pattern, actions, error);
	if (!flow)
		return -rte_errno;

	mrvl_flow_destroy(dev, flow, error);

	return 0;
}

/**
 * DPDK flow flush callback called when flows are to be flushed.
 *
 * @param dev Pointer to the device.
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	while (!LIST_EMPTY(&priv->flows)) {
		struct rte_flow *flow = LIST_FIRST(&priv->flows);
		int ret = mrvl_flow_remove(priv, flow, error);
		if (ret)
			return ret;

		LIST_REMOVE(flow, next);
		rte_free(flow);
	}

	if (priv->cls_tbl) {
		pp2_cls_tbl_deinit(priv->cls_tbl);
		priv->cls_tbl = NULL;
	}

	return 0;
}

/**
 * DPDK flow isolate callback called to isolate port.
 *
 * @param dev Pointer to the device.
 * @param enable Pass 0/1 to disable/enable port isolation.
 * @param error Pointer to the flow error.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_flow_isolate(struct rte_eth_dev *dev, int enable,
		  struct rte_flow_error *error)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	if (dev->data->dev_started) {
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Port must be stopped first\n");
		return -rte_errno;
	}

	priv->isolated = enable;

	return 0;
}

const struct rte_flow_ops mrvl_flow_ops = {
	.validate = mrvl_flow_validate,
	.create = mrvl_flow_create,
	.destroy = mrvl_flow_destroy,
	.flush = mrvl_flow_flush,
	.isolate = mrvl_flow_isolate
};

/**
 * Initialize flow resources.
 *
 * @param dev Pointer to the device.
 */
void
mrvl_flow_init(struct rte_eth_dev *dev)
{
	struct mrvl_priv *priv = dev->data->dev_private;

	LIST_INIT(&priv->flows);
}

/**
 * Cleanup flow resources.
 *
 * @param dev Pointer to the device.
 */
void
mrvl_flow_deinit(struct rte_eth_dev *dev)
{
	mrvl_flow_flush(dev, NULL);
}
