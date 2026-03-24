/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "base/hinic3_cmd.h"
#include "base/hinic3_compat.h"
#include "base/hinic3_hwdev.h"
#include "base/hinic3_hwif.h"
#include "base/hinic3_nic_cfg.h"
#include "hinic3_ethdev.h"
#include "hinic3_nic_io.h"

#define HINIC3_INVALID_INDEX	-1

#define HINIC3_DEV_PRIVATE_TO_TCAM_INFO(nic_dev) \
	(&((struct hinic3_nic_dev *)(nic_dev))->tcam)

/**
 * Perform a bitwise AND operation on the input key value and mask, and stores
 * the result in the key_y array.
 *
 * @param[out] key_y
 * Array for storing results.
 * @param[in] src_input
 * Input key array.
 * @param[in] mask
 * Mask array.
 * @param[in] len
 * Length of the key value and mask.
 */
static void
tcam_translate_key_y(uint8_t *key_y, uint8_t *src_input, uint8_t *mask, uint8_t len)
{
	uint8_t idx;

	for (idx = 0; idx < len; idx++)
		key_y[idx] = src_input[idx] & mask[idx];
}

/**
 * Convert key_y to key_x using the exclusive OR operation.
 *
 * @param[out] key_x
 * Array for storing results.
 * @param[in] key_y
 * Input key array.
 * @param[in] mask
 * Mask array.
 * @param[in] len
 * Length of the key value and mask.
 */
static void
tcam_translate_key_x(uint8_t *key_x, uint8_t *key_y, uint8_t *mask, uint8_t len)
{
	uint8_t idx;

	for (idx = 0; idx < len; idx++)
		key_x[idx] = key_y[idx] ^ mask[idx];
}

static void
tcam_key_calculate(struct hinic3_tcam_key *tcam_key,
		   struct hinic3_tcam_cfg_rule *fdir_tcam_rule)
{
	tcam_translate_key_y(fdir_tcam_rule->key.y, (uint8_t *)(&tcam_key->key_info),
			     (uint8_t *)(&tcam_key->key_mask),
			     HINIC3_TCAM_FLOW_KEY_SIZE);
	tcam_translate_key_x(fdir_tcam_rule->key.x, fdir_tcam_rule->key.y,
			     (uint8_t *)(&tcam_key->key_mask),
			     HINIC3_TCAM_FLOW_KEY_SIZE);
}

static void
hinic3_fdir_tcam_ipv4_init(struct hinic3_fdir_filter *rule,
			   struct hinic3_tcam_key *tcam_key)
{
	/* Fill type of ip. */
	tcam_key->key_mask.ip_type = HINIC3_UINT1_MAX;
	tcam_key->key_info.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;
	tcam_key->key_mask.vlan_flag = HINIC3_UINT1_MAX;
	tcam_key->key_info.vlan_flag = 0;

	/* Fill src IPv4. */
	tcam_key->key_mask.sipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv4.src_ip);
	tcam_key->key_mask.sipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv4.src_ip);
	tcam_key->key_info.sipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv4.src_ip);
	tcam_key->key_info.sipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv4.src_ip);

	/* Fill dst IPv4. */
	tcam_key->key_mask.dipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv4.dst_ip);
	tcam_key->key_mask.dipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv4.dst_ip);
	tcam_key->key_info.dipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv4.dst_ip);
	tcam_key->key_info.dipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv4.dst_ip);
}

static void hinic3_fdir_ipv6_tcam_key_init_sip(struct hinic3_fdir_filter *rule,
					       struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_ipv6.sipv6_key0 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.src_ip[0]);
	tcam_key->key_mask_ipv6.sipv6_key1 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.src_ip[0]);
	tcam_key->key_mask_ipv6.sipv6_key2 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.src_ip[0x1]);
	tcam_key->key_mask_ipv6.sipv6_key3 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.src_ip[0x1]);
	tcam_key->key_mask_ipv6.sipv6_key4 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.src_ip[0x2]);
	tcam_key->key_mask_ipv6.sipv6_key5 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.src_ip[0x2]);
	tcam_key->key_mask_ipv6.sipv6_key6 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.src_ip[0x3]);
	tcam_key->key_mask_ipv6.sipv6_key7 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.src_ip[0x3]);
	tcam_key->key_info_ipv6.sipv6_key0 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.src_ip[0]);
	tcam_key->key_info_ipv6.sipv6_key1 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.src_ip[0]);
	tcam_key->key_info_ipv6.sipv6_key2 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.src_ip[0x1]);
	tcam_key->key_info_ipv6.sipv6_key3 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.src_ip[0x1]);
	tcam_key->key_info_ipv6.sipv6_key4 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.src_ip[0x2]);
	tcam_key->key_info_ipv6.sipv6_key5 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.src_ip[0x2]);
	tcam_key->key_info_ipv6.sipv6_key6 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.src_ip[0x3]);
	tcam_key->key_info_ipv6.sipv6_key7 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.src_ip[0x3]);
}

static void hinic3_fdir_ipv6_tcam_key_init_dip(struct hinic3_fdir_filter *rule,
					       struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_ipv6.dipv6_key0 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.dst_ip[0]);
	tcam_key->key_mask_ipv6.dipv6_key1 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.dst_ip[0]);
	tcam_key->key_mask_ipv6.dipv6_key2 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.dst_ip[0x1]);
	tcam_key->key_mask_ipv6.dipv6_key3 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.dst_ip[0x1]);
	tcam_key->key_mask_ipv6.dipv6_key4 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.dst_ip[0x2]);
	tcam_key->key_mask_ipv6.dipv6_key5 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.dst_ip[0x2]);
	tcam_key->key_mask_ipv6.dipv6_key6 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv6.dst_ip[0x3]);
	tcam_key->key_mask_ipv6.dipv6_key7 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv6.dst_ip[0x3]);
	tcam_key->key_info_ipv6.dipv6_key0 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.dst_ip[0]);
	tcam_key->key_info_ipv6.dipv6_key1 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.dst_ip[0]);
	tcam_key->key_info_ipv6.dipv6_key2 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.dst_ip[0x1]);
	tcam_key->key_info_ipv6.dipv6_key3 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.dst_ip[0x1]);
	tcam_key->key_info_ipv6.dipv6_key4 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.dst_ip[0x2]);
	tcam_key->key_info_ipv6.dipv6_key5 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.dst_ip[0x2]);
	tcam_key->key_info_ipv6.dipv6_key6 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv6.dst_ip[0x3]);
	tcam_key->key_info_ipv6.dipv6_key7 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv6.dst_ip[0x3]);
}

static void hinic3_fdir_ipv6_tcam_key_init(struct hinic3_fdir_filter *rule,
					   struct hinic3_tcam_key *tcam_key)
{
	hinic3_fdir_ipv6_tcam_key_init_sip(rule, tcam_key);
	hinic3_fdir_ipv6_tcam_key_init_dip(rule, tcam_key);
}

static void
hinic3_fdir_tcam_ipv6_init(struct hinic3_fdir_filter *rule,
			   struct hinic3_tcam_key *tcam_key)
{
	/* Fill type of ip. */
	tcam_key->key_mask_ipv6.ip_type = HINIC3_UINT1_MAX;
	tcam_key->key_info_ipv6.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;
	tcam_key->key_mask_ipv6.vlan_flag = HINIC3_UINT1_MAX;
	tcam_key->key_info_ipv6.vlan_flag = 0;

	hinic3_fdir_ipv6_tcam_key_init(rule, tcam_key);
}

/**
 * Set the TCAM information in notunnel scenario.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] rule
 * Pointer to the filtering rule.
 * @param[in] tcam_key
 * Pointer to the TCAM key.
 */
static void
hinic3_fdir_tcam_notunnel_init(struct rte_eth_dev *dev,
			       struct hinic3_fdir_filter *rule,
			       struct hinic3_tcam_key *tcam_key)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	/* Fill tcam_key info. */
	tcam_key->key_mask.sport = rule->key_mask.src_port;
	tcam_key->key_info.sport = rule->key_spec.src_port;

	tcam_key->key_mask.dport = rule->key_mask.dst_port;
	tcam_key->key_info.dport = rule->key_spec.dst_port;

	tcam_key->key_mask.tunnel_type = HINIC3_UINT4_MAX;
	tcam_key->key_info.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_NORMAL;

	tcam_key->key_mask.function_id = HINIC3_UINT15_MAX;

	tcam_key->key_mask.vlan_flag = 1;
	tcam_key->key_info.vlan_flag = 0;

	tcam_key->key_info.function_id = hinic3_global_func_id(nic_dev->hwdev) &
					 HINIC3_UINT15_MAX;

	tcam_key->key_mask.ip_proto = rule->key_mask.proto;
	tcam_key->key_info.ip_proto = rule->key_spec.proto;

	if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV4)
		hinic3_fdir_tcam_ipv4_init(rule, tcam_key);
	else if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV6)
		hinic3_fdir_tcam_ipv6_init(rule, tcam_key);
}

static void
hinic3_fdir_tcam_vxlan_ipv4_init(struct hinic3_fdir_filter *rule,
				 struct hinic3_tcam_key *tcam_key)
{
	/* Fill type of ip. */
	tcam_key->key_mask.ip_type = HINIC3_UINT1_MAX;
	tcam_key->key_info.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;
	tcam_key->key_mask.vlan_flag = HINIC3_UINT1_MAX;
	tcam_key->key_info.vlan_flag = 0;

	/* Fill src ipv4. */
	tcam_key->key_mask.sipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.inner_ipv4.src_ip);
	tcam_key->key_mask.sipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.inner_ipv4.src_ip);
	tcam_key->key_info.sipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.inner_ipv4.src_ip);
	tcam_key->key_info.sipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.inner_ipv4.src_ip);

	/* Fill dst ipv4. */
	tcam_key->key_mask.dipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.inner_ipv4.dst_ip);
	tcam_key->key_mask.dipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.inner_ipv4.dst_ip);
	tcam_key->key_info.dipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.inner_ipv4.dst_ip);
	tcam_key->key_info.dipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.inner_ipv4.dst_ip);
}

static void
hinic3_fdir_tcam_vxlan_ipv6_init(struct hinic3_fdir_filter *rule,
				 struct hinic3_tcam_key *tcam_key)
{
	/* Fill type of ip. */
	tcam_key->key_mask_vxlan_ipv6.ip_type = HINIC3_UINT1_MAX;
	tcam_key->key_info_vxlan_ipv6.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;
	tcam_key->key_mask_vxlan_ipv6.vlan_flag = HINIC3_UINT1_MAX;
	tcam_key->key_info_vxlan_ipv6.vlan_flag = 0;

	/* Use inner dst ipv6 to fill the dst ipv6 of tcam_key. */
	tcam_key->key_mask_vxlan_ipv6.dipv6_key0 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key1 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key2 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0x1]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key3 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0x1]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key4 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0x2]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key5 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0x2]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key6 =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0x3]);
	tcam_key->key_mask_vxlan_ipv6.dipv6_key7 =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.inner_ipv6.dst_ip[0x3]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key0 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key1 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key2 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0x1]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key3 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0x1]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key4 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0x2]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key5 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0x2]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key6 =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0x3]);
	tcam_key->key_info_vxlan_ipv6.dipv6_key7 =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.inner_ipv6.dst_ip[0x3]);
}

static void
hinic3_fdir_tcam_ipv6_vxlan_init(struct rte_eth_dev *dev,
				 struct hinic3_fdir_filter *rule,
				 struct hinic3_tcam_key *tcam_key)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	tcam_key->key_mask_ipv6.ip_proto = rule->key_mask.proto;
	tcam_key->key_info_ipv6.ip_proto = rule->key_spec.proto;

	tcam_key->key_mask_ipv6.tunnel_type = HINIC3_UINT4_MAX;
	tcam_key->key_info_ipv6.tunnel_type = rule->tunnel_type;

	tcam_key->key_mask_ipv6.outer_ip_type = HINIC3_UINT1_MAX;
	tcam_key->key_info_ipv6.outer_ip_type = HINIC3_FDIR_IP_TYPE_IPV6;

	tcam_key->key_mask_ipv6.vlan_flag = HINIC3_UINT1_MAX;
	tcam_key->key_info_ipv6.vlan_flag = 0;

	tcam_key->key_mask_ipv6.function_id = HINIC3_UINT15_MAX;
	tcam_key->key_info_ipv6.function_id =
		hinic3_global_func_id(nic_dev->hwdev) & HINIC3_UINT15_MAX;

	tcam_key->key_mask_ipv6.dport = rule->key_mask.dst_port;
	tcam_key->key_info_ipv6.dport = rule->key_spec.dst_port;

	tcam_key->key_mask_ipv6.sport = rule->key_mask.src_port;
	tcam_key->key_info_ipv6.sport = rule->key_spec.src_port;

	if (rule->ip_type == HINIC3_FDIR_IP_TYPE_ANY)
		hinic3_fdir_ipv6_tcam_key_init(rule, tcam_key);
}

/**
 * Sets the TCAM information in the VXLAN scenario.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] rule
 * Pointer to the filtering rule.
 * @param[in] tcam_key
 * Pointer to the TCAM key.
 */
static void
hinic3_fdir_tcam_vxlan_init(struct rte_eth_dev *dev,
			    struct hinic3_fdir_filter *rule,
			    struct hinic3_tcam_key *tcam_key)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (rule->outer_ip_type == HINIC3_FDIR_IP_TYPE_IPV6) {
		hinic3_fdir_tcam_ipv6_vxlan_init(dev, rule, tcam_key);
		return;
	}

	tcam_key->key_mask.ip_proto = rule->key_mask.proto;
	tcam_key->key_info.ip_proto = rule->key_spec.proto;

	tcam_key->key_mask.sport = rule->key_mask.src_port;
	tcam_key->key_info.sport = rule->key_spec.src_port;

	tcam_key->key_mask.dport = rule->key_mask.dst_port;
	tcam_key->key_info.dport = rule->key_spec.dst_port;

	tcam_key->key_mask.outer_sipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv4.src_ip);
	tcam_key->key_mask.outer_sipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv4.src_ip);
	tcam_key->key_info.outer_sipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv4.src_ip);
	tcam_key->key_info.outer_sipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv4.src_ip);

	tcam_key->key_mask.outer_dipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.ipv4.dst_ip);
	tcam_key->key_mask.outer_dipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.ipv4.dst_ip);
	tcam_key->key_info.outer_dipv4_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.ipv4.dst_ip);
	tcam_key->key_info.outer_dipv4_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.ipv4.dst_ip);

	tcam_key->key_mask.vni_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.tunnel.tunnel_id);
	tcam_key->key_mask.vni_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.tunnel.tunnel_id);
	tcam_key->key_info.vni_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.tunnel.tunnel_id);
	tcam_key->key_info.vni_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.tunnel.tunnel_id);

	tcam_key->key_mask.tunnel_type = HINIC3_UINT4_MAX;
	tcam_key->key_info.tunnel_type = rule->tunnel_type;

	tcam_key->key_mask.vlan_flag = 1;
	tcam_key->key_mask.function_id = HINIC3_UINT15_MAX;
	tcam_key->key_info.vlan_flag = 0;
	tcam_key->key_info.function_id = hinic3_global_func_id(nic_dev->hwdev) &
					 HINIC3_UINT15_MAX;

	if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV4)
		hinic3_fdir_tcam_vxlan_ipv4_init(rule, tcam_key);

	else if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV6)
		hinic3_fdir_tcam_vxlan_ipv6_init(rule, tcam_key);
}

static void
hinic3_fdir_tcam_info_init(struct rte_eth_dev *dev,
			   struct hinic3_fdir_filter *rule,
			   struct hinic3_tcam_key *tcam_key,
			   struct hinic3_tcam_cfg_rule *fdir_tcam_rule)
{
	/* Initialize the TCAM based on the tunnel type. */
	if (rule->tunnel_type == HINIC3_FDIR_TUNNEL_MODE_NORMAL)
		hinic3_fdir_tcam_notunnel_init(dev, rule, tcam_key);
	else
		hinic3_fdir_tcam_vxlan_init(dev, rule, tcam_key);

	/* Set the queue index. */
	fdir_tcam_rule->data.qid = rule->rq_index;
	/* Calculate key of TCAM. */
	tcam_key_calculate(tcam_key, fdir_tcam_rule);
}

static void
hinic3_fdir_tcam_key_set_ipv4_sip_dip(struct rte_eth_ipv4_flow *ipv4_mask,
				      struct rte_eth_ipv4_flow *ipv4_spec,
				      struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_htn.sipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_mask->src_ip);
	tcam_key->key_mask_htn.sipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_mask->src_ip);
	tcam_key->key_info_htn.sipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_spec->src_ip);
	tcam_key->key_info_htn.sipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_spec->src_ip);

	tcam_key->key_mask_htn.dipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_mask->dst_ip);
	tcam_key->key_mask_htn.dipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_mask->dst_ip);
	tcam_key->key_info_htn.dipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_spec->dst_ip);
	tcam_key->key_info_htn.dipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_spec->dst_ip);
}

static void
hinic3_fdir_tcam_key_set_ipv6_sip(struct rte_eth_ipv6_flow *ipv6_mask,
				  struct rte_eth_ipv6_flow *ipv6_spec,
				  struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_ipv6_htn.sipv6_key0 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->src_ip[0]);
	tcam_key->key_mask_ipv6_htn.sipv6_key1 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->src_ip[0]);
	tcam_key->key_mask_ipv6_htn.sipv6_key2 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->src_ip[0x1]);
	tcam_key->key_mask_ipv6_htn.sipv6_key3 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->src_ip[0x1]);
	tcam_key->key_mask_ipv6_htn.sipv6_key4 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->src_ip[0x2]);
	tcam_key->key_mask_ipv6_htn.sipv6_key5 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->src_ip[0x2]);
	tcam_key->key_mask_ipv6_htn.sipv6_key6 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->src_ip[0x3]);
	tcam_key->key_mask_ipv6_htn.sipv6_key7 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->src_ip[0x3]);
	tcam_key->key_info_ipv6_htn.sipv6_key0 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->src_ip[0]);
	tcam_key->key_info_ipv6_htn.sipv6_key1 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->src_ip[0]);
	tcam_key->key_info_ipv6_htn.sipv6_key2 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->src_ip[0x1]);
	tcam_key->key_info_ipv6_htn.sipv6_key3 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->src_ip[0x1]);
	tcam_key->key_info_ipv6_htn.sipv6_key4 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->src_ip[0x2]);
	tcam_key->key_info_ipv6_htn.sipv6_key5 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->src_ip[0x2]);
	tcam_key->key_info_ipv6_htn.sipv6_key6 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->src_ip[0x3]);
	tcam_key->key_info_ipv6_htn.sipv6_key7 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->src_ip[0x3]);
}

static void
hinic3_fdir_tcam_key_set_ipv6_dip(struct rte_eth_ipv6_flow *ipv6_mask,
				  struct rte_eth_ipv6_flow *ipv6_spec,
				  struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_ipv6_htn.dipv6_key0 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->dst_ip[0]);
	tcam_key->key_mask_ipv6_htn.dipv6_key1 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->dst_ip[0]);
	tcam_key->key_mask_ipv6_htn.dipv6_key2 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->dst_ip[0x1]);
	tcam_key->key_mask_ipv6_htn.dipv6_key3 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->dst_ip[0x1]);
	tcam_key->key_mask_ipv6_htn.dipv6_key4 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->dst_ip[0x2]);
	tcam_key->key_mask_ipv6_htn.dipv6_key5 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->dst_ip[0x2]);
	tcam_key->key_mask_ipv6_htn.dipv6_key6 =
		HINIC3_32_UPPER_16_BITS(ipv6_mask->dst_ip[0x3]);
	tcam_key->key_mask_ipv6_htn.dipv6_key7 =
		HINIC3_32_LOWER_16_BITS(ipv6_mask->dst_ip[0x3]);
	tcam_key->key_info_ipv6_htn.dipv6_key0 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->dst_ip[0]);
	tcam_key->key_info_ipv6_htn.dipv6_key1 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->dst_ip[0]);
	tcam_key->key_info_ipv6_htn.dipv6_key2 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->dst_ip[0x1]);
	tcam_key->key_info_ipv6_htn.dipv6_key3 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->dst_ip[0x1]);
	tcam_key->key_info_ipv6_htn.dipv6_key4 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->dst_ip[0x2]);
	tcam_key->key_info_ipv6_htn.dipv6_key5 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->dst_ip[0x2]);
	tcam_key->key_info_ipv6_htn.dipv6_key6 =
		HINIC3_32_UPPER_16_BITS(ipv6_spec->dst_ip[0x3]);
	tcam_key->key_info_ipv6_htn.dipv6_key7 =
		HINIC3_32_LOWER_16_BITS(ipv6_spec->dst_ip[0x3]);
}

static void
hinic3_fdir_tcam_key_set_outer_ipv4_sip_dip(struct rte_eth_ipv4_flow *ipv4_mask,
					    struct rte_eth_ipv4_flow *ipv4_spec,
					    struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_htn.outer_sipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_mask->src_ip);
	tcam_key->key_mask_htn.outer_sipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_mask->src_ip);
	tcam_key->key_info_htn.outer_sipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_spec->src_ip);
	tcam_key->key_info_htn.outer_sipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_spec->src_ip);

	tcam_key->key_mask_htn.outer_dipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_mask->dst_ip);
	tcam_key->key_mask_htn.outer_dipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_mask->dst_ip);
	tcam_key->key_info_htn.outer_dipv4_h =
		HINIC3_32_UPPER_16_BITS(ipv4_spec->dst_ip);
	tcam_key->key_info_htn.outer_dipv4_l =
		HINIC3_32_LOWER_16_BITS(ipv4_spec->dst_ip);
}

static void
hinic3_fdir_tcam_key_set_ipv4_info(struct hinic3_fdir_filter *rule,
				   struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_htn.ip_type = HINIC3_UINT2_MAX;
	tcam_key->key_info_htn.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;

	hinic3_fdir_tcam_key_set_ipv4_sip_dip(&rule->key_mask.ipv4,
					      &rule->key_spec.ipv4, tcam_key);
}

static void hinic3_fdir_tcam_key_set_ipv6_info(struct hinic3_fdir_filter *rule,
					       struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_ipv6_htn.ip_type = HINIC3_UINT2_MAX;
	tcam_key->key_info_ipv6_htn.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;

	hinic3_fdir_tcam_key_set_ipv6_sip(&rule->key_mask.ipv6,
					  &rule->key_spec.ipv6, tcam_key);
	hinic3_fdir_tcam_key_set_ipv6_dip(&rule->key_mask.ipv6,
					  &rule->key_spec.ipv6, tcam_key);
}

static void
hinic3_fdir_tcam_notunnel_htn_init(struct hinic3_fdir_filter *rule,
				   struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_htn.tunnel_type = HINIC3_UINT3_MAX;
	tcam_key->key_info_htn.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_NORMAL;

	if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV4)
		hinic3_fdir_tcam_key_set_ipv4_info(rule, tcam_key);
	else if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV6)
		hinic3_fdir_tcam_key_set_ipv6_info(rule, tcam_key);
}

static void
hinic3_fdir_tcam_key_set_outer_ipv4_info(struct hinic3_fdir_filter *rule,
					 struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_ipv6_htn.outer_ip_type = HINIC3_UINT1_MAX;
	tcam_key->key_info_ipv6_htn.outer_ip_type = HINIC3_FDIR_IP_TYPE_IPV4;

	hinic3_fdir_tcam_key_set_outer_ipv4_sip_dip(&rule->key_mask.ipv4,
						    &rule->key_spec.ipv4, tcam_key);
}

static void
hinic3_fdir_tcam_key_set_inner_ipv4_info(struct hinic3_fdir_filter *rule,
					 struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_htn.ip_type = HINIC3_UINT2_MAX;
	tcam_key->key_info_htn.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;

	hinic3_fdir_tcam_key_set_ipv4_sip_dip(&rule->key_mask.inner_ipv4,
					      &rule->key_spec.inner_ipv4, tcam_key);
}

static void
hinic3_fdir_tcam_key_set_inner_ipv6_info(struct hinic3_fdir_filter *rule,
					 struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_vxlan_ipv6_htn.ip_type = HINIC3_UINT2_MAX;
	tcam_key->key_info_vxlan_ipv6_htn.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;

	hinic3_fdir_tcam_key_set_ipv6_dip(&rule->key_mask.inner_ipv6,
					  &rule->key_spec.inner_ipv6, tcam_key);
}

static void
hinic3_fdir_tcam_tunnel_htn_init(struct hinic3_fdir_filter *rule,
				 struct hinic3_tcam_key *tcam_key)
{
	tcam_key->key_mask_htn.tunnel_type = HINIC3_UINT3_MAX;
	tcam_key->key_info_htn.tunnel_type = rule->tunnel_type;

	tcam_key->key_mask_htn.vni_h =
		HINIC3_32_UPPER_16_BITS(rule->key_mask.tunnel.tunnel_id);
	tcam_key->key_mask_htn.vni_l =
		HINIC3_32_LOWER_16_BITS(rule->key_mask.tunnel.tunnel_id);
	tcam_key->key_info_htn.vni_h =
		HINIC3_32_UPPER_16_BITS(rule->key_spec.tunnel.tunnel_id);
	tcam_key->key_info_htn.vni_l =
		HINIC3_32_LOWER_16_BITS(rule->key_spec.tunnel.tunnel_id);

	if (rule->outer_ip_type == HINIC3_FDIR_IP_TYPE_IPV4)
		hinic3_fdir_tcam_key_set_outer_ipv4_info(rule, tcam_key);

	if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV4)
		hinic3_fdir_tcam_key_set_inner_ipv4_info(rule, tcam_key);
	else if (rule->ip_type == HINIC3_FDIR_IP_TYPE_IPV6)
		hinic3_fdir_tcam_key_set_inner_ipv6_info(rule, tcam_key);
}

static void
hinic3_fdir_tcam_info_htn_init(struct rte_eth_dev *dev,
			       struct hinic3_fdir_filter *rule,
			       struct hinic3_tcam_key *tcam_key,
			       struct hinic3_tcam_cfg_rule *fdir_tcam_rule)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	tcam_key->key_mask_htn.function_id_h = HINIC3_UINT5_MAX;
	tcam_key->key_mask_htn.function_id_l = HINIC3_UINT5_MAX;
	tcam_key->key_info_htn.function_id_l =
		hinic3_global_func_id(nic_dev->hwdev) & HINIC3_UINT5_MAX;
	tcam_key->key_info_htn.function_id_h =
		(hinic3_global_func_id(nic_dev->hwdev) >> HINIC3_UINT5_WIDTH) & HINIC3_UINT5_MAX;

	tcam_key->key_mask_htn.ip_proto = rule->key_mask.proto;
	tcam_key->key_info_htn.ip_proto = rule->key_spec.proto;

	tcam_key->key_mask_htn.sport = rule->key_mask.src_port;
	tcam_key->key_info_htn.sport = rule->key_spec.src_port;

	tcam_key->key_mask_htn.dport = rule->key_mask.dst_port;
	tcam_key->key_info_htn.dport = rule->key_spec.dst_port;
	if (rule->tunnel_type == HINIC3_FDIR_TUNNEL_MODE_NORMAL)
		hinic3_fdir_tcam_notunnel_htn_init(rule, tcam_key);
	else
		hinic3_fdir_tcam_tunnel_htn_init(rule, tcam_key);

	fdir_tcam_rule->data.qid = rule->rq_index;

	tcam_key_calculate(tcam_key, fdir_tcam_rule);
}

/**
 * Find filter in given ethertype filter list.
 *
 * @param[in] filter_list
 * Point to the Ether filter list.
 * @param[in] key
 * The tcam key to find.
 * @return
 * If a matching filter is found, the filter is returned, otherwise
 * RTE_ETH_FILTER_NONE.
 */
static inline uint16_t
hinic3_ethertype_filter_lookup(struct hinic3_ethertype_filter_list *ethertype_list,
			       uint16_t type)
{
	struct rte_flow *it;
	struct hinic3_filter_t *filter_rules;

	TAILQ_FOREACH(it, ethertype_list, node) {
		filter_rules = it->rule;
		if (type == filter_rules->ethertype_filter.ether_type)
			return filter_rules->ethertype_filter.ether_type;
	}

	return RTE_ETH_FILTER_NONE;
}

/**
 * Find the filter that matches the given key in the TCAM filter list.
 *
 * @param[in] filter_list
 * Point to the tcam filter list.
 * @param[in] key
 * The tcam key to find.
 * @param[in] action_type
 * The type of action.
 * @param[in] tcam_index
 * The index of tcam.
 * @return
 * If a matching filter is found, the filter is returned, otherwise NULL.
 */
static inline struct hinic3_tcam_filter *
hinic3_tcam_filter_lookup(struct hinic3_tcam_filter_list *filter_list,
			  struct hinic3_tcam_key *key,
			  uint8_t action_type, uint16_t tcam_index)
{
	struct hinic3_tcam_filter *it;

	if (action_type == HINIC3_ACTION_ADD) {
		TAILQ_FOREACH(it, filter_list, entries) {
			if (memcmp(key, &it->tcam_key, sizeof(struct hinic3_tcam_key)) == 0)
				return it;
		}
	} else {
		TAILQ_FOREACH(it, filter_list, entries) {
			if (tcam_index ==
			   (it->index + HINIC3_PKT_TCAM_DYNAMIC_INDEX_START(it->dynamic_block_id)))
				return it;
		}
	}

	return NULL;
}
/**
 * Allocate memory for dynamic blocks and then add them to the queue.
 *
 * @param[in] tcam_info
 * Point to TCAM information.
 * @param[in] dynamic_block_id
 * Indicate the ID of a dynamic block.
 * @return
 * Return the pointer to the dynamic block, or NULL if the allocation fails.
 */
static struct hinic3_tcam_dynamic_block *
hinic3_alloc_dynamic_block_resource(struct hinic3_tcam_info *tcam_info,
				    uint16_t dynamic_block_id)
{
	struct hinic3_tcam_dynamic_block *dynamic_block_ptr = NULL;

	dynamic_block_ptr =
		rte_zmalloc("hinic3_tcam_dynamic_mem",
			    sizeof(struct hinic3_tcam_dynamic_block), 0);
	if (dynamic_block_ptr == NULL) {
		PMD_DRV_LOG(ERR,
			    "Alloc fdir filter dynamic block index %d memory failed!",
			    dynamic_block_id);
		return NULL;
	}

	dynamic_block_ptr->dynamic_block_id = dynamic_block_id;

	/* Add new block to the end of the TCAM dynamic block list. */
	TAILQ_INSERT_TAIL(&tcam_info->tcam_dynamic_info.tcam_dynamic_list,
			  dynamic_block_ptr, entries);

	tcam_info->tcam_dynamic_info.dynamic_block_cnt++;

	return dynamic_block_ptr;
}

static void
hinic3_free_dynamic_block_resource(struct hinic3_tcam_info *tcam_info,
				   struct hinic3_tcam_dynamic_block *dynamic_block_ptr)
{
	if (dynamic_block_ptr == NULL)
		return;

	/* Remove the incoming dynamic block from the TCAM dynamic list. */
	TAILQ_REMOVE(&tcam_info->tcam_dynamic_info.tcam_dynamic_list,
		     dynamic_block_ptr, entries);
	rte_free(dynamic_block_ptr);

	tcam_info->tcam_dynamic_info.dynamic_block_cnt--;
}

/**
 * Check whether there are free positions in the dynamic TCAM filter.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] tcam_info
 * Ternary Content-Addressable Memory (TCAM) information.
 * @param[out] tcam_index
 * Indicate the TCAM index to be searched for.
 * @result
 * Pointer to the TCAM dynamic block. If the search fails, NULL is returned.
 */
static struct hinic3_tcam_dynamic_block *
hinic3_dynamic_lookup_tcam_filter(struct hinic3_nic_dev *nic_dev,
				  struct hinic3_tcam_info *tcam_info,
				  uint16_t *tcam_index)
{
	uint16_t block_cnt = tcam_info->tcam_dynamic_info.dynamic_block_cnt;
	struct hinic3_tcam_dynamic_block *dynamic_block_ptr = NULL;
	struct hinic3_tcam_dynamic_block *tmp = NULL;
	uint16_t rule_nums = nic_dev->tcam_rule_nums;
	int block_alloc_flag = 0;
	uint16_t dynamic_block_id = 0;
	uint16_t index;
	int err;

	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) != 0)
		rule_nums += nic_dev->ethertype_rule_nums;
	/*
	 * Check whether the number of filtering rules reaches the maximum
	 * capacity of dynamic TCAM blocks.
	 */
	if (rule_nums >= block_cnt * HINIC3_TCAM_DYNAMIC_BLOCK_SIZE) {
		if (block_cnt >= (HINIC3_TCAM_DYNAMIC_MAX_FILTERS /
				  HINIC3_TCAM_DYNAMIC_BLOCK_SIZE)) {
			PMD_DRV_LOG(ERR,
				"Dynamic tcam block is full, alloc failed!");
			goto failed;
		}
		/*
		 * The TCAM blocks are insufficient.
		 * Apply for a new TCAM block.
		 */
		err = hinic3_alloc_tcam_block(nic_dev->hwdev, &dynamic_block_id);
		if (err) {
			PMD_DRV_LOG(ERR,
				"Fdir filter dynamic tcam alloc block failed!");
			goto failed;
		}

		block_alloc_flag = 1;

		/* Applying for Memory. */
		dynamic_block_ptr =
			hinic3_alloc_dynamic_block_resource(tcam_info,
							    dynamic_block_id);
		if (dynamic_block_ptr == NULL) {
			PMD_DRV_LOG(ERR, "Fdir filter dynamic alloc block memory failed!");
			goto block_alloc_failed;
		}
	}

	/*
	 * Find the first dynamic TCAM block that meets dynamci_index_cnt <
	 * HINIC3_TCAM_DYNAMIC_BLOCK_SIZE.
	 */
	TAILQ_FOREACH(tmp, &tcam_info->tcam_dynamic_info.tcam_dynamic_list,
		      entries) {
		if (tmp->dynamic_index_cnt < HINIC3_TCAM_DYNAMIC_BLOCK_SIZE)
			break;
	}

	if (tmp == NULL ||
	    tmp->dynamic_index_cnt >= HINIC3_TCAM_DYNAMIC_BLOCK_SIZE) {
		PMD_DRV_LOG(ERR, "Fdir filter dynamic lookup for index failed!");
		goto look_up_failed;
	}

	for (index = 0; index < HINIC3_TCAM_DYNAMIC_BLOCK_SIZE; index++) {
		if (tmp->dynamic_index[index] == 0)
			break;
	}

	/* Find the first free position. */
	if (index == HINIC3_TCAM_DYNAMIC_BLOCK_SIZE) {
		PMD_DRV_LOG(ERR, "tcam block 0x%x supports filter rules is full!",
			    tmp->dynamic_block_id);
		goto look_up_failed;
	}

	*tcam_index = index;

	return tmp;

look_up_failed:
	if (dynamic_block_ptr != NULL)
		hinic3_free_dynamic_block_resource(tcam_info, dynamic_block_ptr);

block_alloc_failed:
	if (block_alloc_flag == 1)
		hinic3_free_tcam_block(nic_dev->hwdev, &dynamic_block_id);

failed:
	return NULL;
}

static void
hinic3_tcam_index_free(struct hinic3_nic_dev *nic_dev, uint16_t index, uint16_t block_id)
{
	struct hinic3_tcam_info *tcam_info = HINIC3_DEV_PRIVATE_TO_TCAM_INFO(nic_dev);
	struct hinic3_tcam_dynamic_block *tmp = NULL;

	TAILQ_FOREACH(tmp, &tcam_info->tcam_dynamic_info.tcam_dynamic_list, entries) {
			if (tmp->dynamic_block_id == block_id)
				break;
	}

	if (tmp == NULL || tmp->dynamic_block_id != block_id) {
		PMD_DRV_LOG(ERR, "Fdir filter del dynamic lookup for block failed!");
		return;
	}

	tmp->dynamic_index[index] = 0;
	tmp->dynamic_index_cnt--;
	if (tmp->dynamic_index_cnt == 0) {
		hinic3_free_tcam_block(nic_dev->hwdev, &block_id);
		hinic3_free_dynamic_block_resource(tcam_info, tmp);
	}
}

static uint16_t
hinic3_tcam_alloc_index(void *dev, uint16_t *block_id)
{
	struct hinic3_nic_dev *nic_dev = (struct hinic3_nic_dev *)dev;
	struct hinic3_tcam_info *tcam_info = HINIC3_DEV_PRIVATE_TO_TCAM_INFO(nic_dev);
	struct hinic3_tcam_dynamic_block *tmp = NULL;
	uint16_t index = 0;

	tmp = hinic3_dynamic_lookup_tcam_filter(nic_dev, tcam_info, &index);
	if (tmp == NULL) {
		PMD_DRV_LOG(ERR, "Dynamic lookup tcam filter failed!");
		return HINIC3_TCAM_INVALID_INDEX;
	}

	tmp->dynamic_index[index] = 1;
	tmp->dynamic_index_cnt++;

	*block_id = tmp->dynamic_block_id;

	return index;
}

static int
hinic3_set_fdir_ethertype_filter(void *hwdev, uint8_t pkt_type, void *filter, uint8_t en)
{
	struct hinic3_set_fdir_ethertype_rule ethertype_cmd;
	struct hinic3_ethertype_filter *ethertype_filter = (struct hinic3_ethertype_filter *)filter;
	uint16_t out_size = sizeof(ethertype_cmd);
	uint16_t block_id;
	uint32_t index = 0;
	int err;

	if (!hwdev)
		return -EINVAL;
	struct hinic3_nic_dev *nic_dev =
		(struct hinic3_nic_dev *)((struct hinic3_hwdev *)hwdev)->dev_handle;

	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) != 0) {
		if (en != 0) {
			index = hinic3_tcam_alloc_index(nic_dev, &block_id);
			if (index == HINIC3_TCAM_INVALID_INDEX)
				return -ENOMEM;
			index += HINIC3_PKT_TCAM_DYNAMIC_INDEX_START(block_id);
		} else {
			index = ethertype_filter->tcam_index[pkt_type];
		}
	}

	memset(&ethertype_cmd, 0, sizeof(struct hinic3_set_fdir_ethertype_rule));
	ethertype_cmd.func_id = hinic3_global_func_id(hwdev);
	ethertype_cmd.pkt_type = pkt_type;
	ethertype_cmd.pkt_type_en = en;
	ethertype_cmd.index = index;
	ethertype_cmd.qid = (uint8_t)ethertype_filter->queue;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_L2NIC,
				     HINIC3_NIC_CMD_SET_FDIR_STATUS,
				     &ethertype_cmd, sizeof(ethertype_cmd),
				     &ethertype_cmd, &out_size);
	if (err || ethertype_cmd.head.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "set fdir ethertype rule failed, err: %d, status: 0x%x, out size: 0x%x, func_id %d",
			    err, ethertype_cmd.head.status, out_size, ethertype_cmd.func_id);
		return -EIO;
	}
	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) != 0) {
		if (en == 0) {
			hinic3_tcam_index_free(nic_dev, HINIC3_TCAM_GET_INDEX_IN_BLOCK(index),
					       HINIC3_TCAM_GET_DYNAMIC_BLOCK_INDEX(index));
		} else {
			ethertype_filter->tcam_index[pkt_type] = index;
		}
	}

	return 0;
}

/**
 * Add a TCAM filter.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] tcam_key
 * Pointer to the TCAM key.
 * @param[in] fdir_tcam_rule
 * Pointer to the  TCAM filtering rule.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_add_tcam_filter(struct rte_eth_dev *dev,
		       struct hinic3_tcam_key *tcam_key,
		       struct hinic3_tcam_cfg_rule *fdir_tcam_rule)
{
	struct hinic3_tcam_info *tcam_info =
		HINIC3_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_tcam_filter *tcam_filter;
	int err;

	/* Alloc TCAM filter memory. */
	tcam_filter = rte_zmalloc("hinic3_fdir_filter",
				  sizeof(struct hinic3_tcam_filter), 0);
	if (tcam_filter == NULL)
		return -ENOMEM;

	tcam_filter->tcam_key = *tcam_key;
	tcam_filter->queue = (uint16_t)(fdir_tcam_rule->data.qid);
	tcam_filter->index = hinic3_tcam_alloc_index(nic_dev, &tcam_filter->dynamic_block_id);
	if (tcam_filter->index == HINIC3_TCAM_INVALID_INDEX)
		goto failed;
	fdir_tcam_rule->index = HINIC3_PKT_TCAM_DYNAMIC_INDEX_START(tcam_filter->dynamic_block_id) +
								    tcam_filter->index;

	/* Add a new TCAM rule to the network device. */
	err = hinic3_add_tcam_rule(nic_dev->hwdev, fdir_tcam_rule, TCAM_RULE_FDIR_TYPE);
	if (err) {
		PMD_DRV_LOG(ERR, "Fdir_tcam_rule add failed!");
		goto add_tcam_rules_failed;
	}

	/* If there are no rules, TCAM filtering is enabled. */
	if (!(nic_dev->ethertype_rule_nums + nic_dev->tcam_rule_nums)) {
		err = hinic3_set_fdir_tcam_rule_filter(nic_dev->hwdev, true);
		if (err)
			goto enable_failed;
	}

	/* Add a filter to the end of the queue. */
	TAILQ_INSERT_TAIL(&tcam_info->tcam_list, tcam_filter, entries);

	nic_dev->tcam_rule_nums++;

	PMD_DRV_LOG(INFO,
		    "Add fdir tcam rule succeed, function_id: 0x%x",
		    hinic3_global_func_id(nic_dev->hwdev));
	PMD_DRV_LOG(INFO,
		    "tcam_block_id: %d, local_index: %d, global_index: %d, queue: %d, tcam_rule_nums: %d",
		    tcam_filter->dynamic_block_id, tcam_filter->index, fdir_tcam_rule->index,
		    fdir_tcam_rule->data.qid, nic_dev->tcam_rule_nums);
	return 0;

enable_failed:
	hinic3_del_tcam_rule(nic_dev->hwdev,
			     fdir_tcam_rule->index,
			     TCAM_RULE_FDIR_TYPE);

add_tcam_rules_failed:
	hinic3_tcam_index_free(nic_dev, tcam_filter->index, tcam_filter->dynamic_block_id);

failed:
	rte_free(tcam_filter);
	return -EFAULT;
}

/**
 * Delete a TCAM filter.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] tcam_filter
 * TCAM Filters to Delete.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_del_dynamic_tcam_filter(struct rte_eth_dev *dev,
			       struct hinic3_tcam_filter *tcam_filter)
{
	struct hinic3_tcam_info *tcam_info =
		HINIC3_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	uint16_t dynamic_block_id = tcam_filter->dynamic_block_id;
	struct hinic3_tcam_dynamic_block *tmp = NULL;
	uint32_t index = 0;
	int err;

	/* Traverse to find the block that matches the given ID. */
	TAILQ_FOREACH(tmp, &tcam_info->tcam_dynamic_info.tcam_dynamic_list,
		      entries) {
		if (tmp->dynamic_block_id == dynamic_block_id)
			break;
	}

	if (tmp == NULL || tmp->dynamic_block_id != dynamic_block_id) {
		PMD_DRV_LOG(ERR, "Fdir filter del dynamic lookup for block failed!");
		return -EINVAL;
	}
	/* Calculate TCAM index. */
	index = HINIC3_PKT_TCAM_DYNAMIC_INDEX_START(tmp->dynamic_block_id) +
		tcam_filter->index;

	/* Delete a specified rule. */
	err = hinic3_del_tcam_rule(nic_dev->hwdev, index, TCAM_RULE_FDIR_TYPE);
	if (err) {
		PMD_DRV_LOG(ERR, "Fdir tcam rule del failed!");
		return -EFAULT;
	}

	PMD_DRV_LOG(INFO,
		    "Del fdir_tcam_dynamic_rule succeed, function_id: 0x%x",
		    hinic3_global_func_id(nic_dev->hwdev));
	PMD_DRV_LOG(INFO,
		    "tcam_block_id: %d, local_index: %d, global_index: %d, local_rules_nums: %d, global_rule_nums: %d",
		    dynamic_block_id, tcam_filter->index, index,
		    tmp->dynamic_index_cnt - 1, nic_dev->tcam_rule_nums - 1);

	hinic3_tcam_index_free(nic_dev, tcam_filter->index, tmp->dynamic_block_id);

	nic_dev->tcam_rule_nums--;

	/* If the number of rules is 0, the TCAM filter is disabled. */
	if (!(nic_dev->ethertype_rule_nums + nic_dev->tcam_rule_nums))
		hinic3_set_fdir_tcam_rule_filter(nic_dev->hwdev, false);

	return 0;
}

static int
hinic3_del_tcam_filter(struct rte_eth_dev *dev,
		       struct hinic3_tcam_filter *tcam_filter)
{
	struct hinic3_tcam_info *tcam_info =
		HINIC3_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	int err;

	err = hinic3_del_dynamic_tcam_filter(dev, tcam_filter);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Del dynamic tcam filter failed!");
		return err;
	}

	/* Remove the filter from the TCAM list. */
	TAILQ_REMOVE(&tcam_info->tcam_list, tcam_filter, entries);

	rte_free(tcam_filter);

	return 0;
}

/**
 * Add or deletes an fdir filter rule. This is the core function for operating
 * filters.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] fdir_filter
 * Pointer to the fdir filter.
 * @param[in] add
 * This is a Boolean value (of the bool type) indicating whether the action to
 * be performed is to add (true) or delete (false) the filter rule.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_flow_add_del_fdir_filter(struct rte_eth_dev *dev,
				struct hinic3_fdir_filter *fdir_filter,
				bool add)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_tcam_info *tcam_info =
		HINIC3_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic3_tcam_filter *tcam_filter;
	struct hinic3_tcam_cfg_rule fdir_tcam_rule;
	struct hinic3_tcam_key tcam_key;
	int ret;

	memset(&fdir_tcam_rule, 0, sizeof(struct hinic3_tcam_cfg_rule));
	memset((void *)&tcam_key, 0, sizeof(struct hinic3_tcam_key));

	if ((hinic3_get_driver_feature(nic_dev) & NIC_F_HTN_FDIR) == 0)
		hinic3_fdir_tcam_info_init(dev, fdir_filter, &tcam_key, &fdir_tcam_rule);
	else
		hinic3_fdir_tcam_info_htn_init(dev, fdir_filter, &tcam_key, &fdir_tcam_rule);

	/* Search for a filter. */
	tcam_filter =
		hinic3_tcam_filter_lookup(&tcam_info->tcam_list, &tcam_key,
					  HINIC3_ACTION_ADD, HINIC3_INVALID_INDEX);
	if (tcam_filter != NULL && add) {
		PMD_DRV_LOG(ERR, "Filter exists.");
		return -EEXIST;
	}
	if (tcam_filter == NULL && !add) {
		PMD_DRV_LOG(ERR, "Filter doesn't exist.");
		return -ENOENT;
	}

	/*
	 * If the value of Add is true, the system performs the adding
	 * operation.
	 */
	if (add) {
		ret = hinic3_add_tcam_filter(dev, &tcam_key, &fdir_tcam_rule);
		if (ret)
			goto cfg_tcam_filter_err;

		fdir_filter->tcam_index = (int)(fdir_tcam_rule.index);
	} else {
		tcam_filter = hinic3_tcam_filter_lookup(&tcam_info->tcam_list, &tcam_key,
							HINIC3_ACTION_NOT_ADD,
							fdir_filter->tcam_index);
		if (tcam_filter == NULL) {
			PMD_DRV_LOG(ERR, "Filter doesn't exist.");
			return -ENOENT;
		}
		PMD_DRV_LOG(INFO, "begin to del tcam filter");
		ret = hinic3_del_tcam_filter(dev, tcam_filter);
		if (ret)
			goto cfg_tcam_filter_err;
	}

	return 0;

cfg_tcam_filter_err:

	return ret;
}

/**
 * Enable or disable the TCAM filter for the receive queue.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] able
 * Flag to enable or disable the filter.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_enable_rxq_fdir_filter(struct rte_eth_dev *dev, uint32_t queue_id, uint32_t able)
{
	struct hinic3_tcam_info *tcam_info =
		HINIC3_DEV_PRIVATE_TO_TCAM_INFO(dev->data->dev_private);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_tcam_filter *it;
	struct hinic3_tcam_cfg_rule fdir_tcam_rule;
	int ret;
	uint32_t queue_res;
	uint16_t index;

	memset(&fdir_tcam_rule, 0, sizeof(struct hinic3_tcam_cfg_rule));

	if (able) {
		TAILQ_FOREACH(it, &tcam_info->tcam_list, entries) {
			if (queue_id == it->queue) {
				index = (uint16_t)(HINIC3_PKT_TCAM_DYNAMIC_INDEX_START
					      (it->dynamic_block_id) + it->index);

				/*
				 * When the rxq is start, find invalid rxq_id
				 * and delete the fdir rule from the tcam.
				 */
				ret = hinic3_del_tcam_rule(nic_dev->hwdev,
							   index,
							   TCAM_RULE_FDIR_TYPE);
				if (ret) {
					PMD_DRV_LOG(ERR, "del invalid tcam rule failed!");
					return -EFAULT;
				}

				fdir_tcam_rule.index = index;
				fdir_tcam_rule.data.qid = queue_id;
				tcam_key_calculate(&it->tcam_key,
						   &fdir_tcam_rule);

				/* To enable a rule, add a rule. */
				ret = hinic3_add_tcam_rule(nic_dev->hwdev,
							   &fdir_tcam_rule,
							   TCAM_RULE_FDIR_TYPE);
				if (ret) {
					PMD_DRV_LOG(ERR, "add correct tcam rule failed!");
					return -EFAULT;
				}
			}
		}
	} else {
		queue_res = HINIC3_INVALID_QID_BASE | queue_id;

		TAILQ_FOREACH(it, &tcam_info->tcam_list, entries) {
			if (queue_id == it->queue) {
				index = (uint16_t)(HINIC3_PKT_TCAM_DYNAMIC_INDEX_START
					      (it->dynamic_block_id) + it->index);

				/*
				 * When the rxq is stop, delete the fdir rule
				 * from the tcam and add the correct fdir rule
				 * from the tcam.
				 */
				ret = hinic3_del_tcam_rule(nic_dev->hwdev,
							   index,
							   TCAM_RULE_FDIR_TYPE);
				if (ret) {
					PMD_DRV_LOG(ERR, "del correct tcam rule failed!");
					return -EFAULT;
				}

				fdir_tcam_rule.index = index;
				fdir_tcam_rule.data.qid = queue_res;
				tcam_key_calculate(&it->tcam_key,
						   &fdir_tcam_rule);

				/* Add the correct fdir rule from the tcam. */
				ret = hinic3_add_tcam_rule(nic_dev->hwdev,
							   &fdir_tcam_rule,
							   TCAM_RULE_FDIR_TYPE);
				if (ret) {
					PMD_DRV_LOG(ERR, "add invalid tcam rule failed!");
					return -EFAULT;
				}
			}
		}
	}

	return ret;
}

void
hinic3_free_fdir_filter(struct rte_eth_dev *dev)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	hinic3_set_fdir_tcam_rule_filter(nic_dev->hwdev, false);

	hinic3_flush_tcam_rule(nic_dev->hwdev);
}

static int
hinic3_flow_set_arp_filter(struct rte_eth_dev *dev,
			   struct hinic3_ethertype_filter *ethertype_filter,
			   bool add)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	/* Setting the ARP Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_ARP,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s fdir ethertype rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		return ret;
	}

	/* Setting the ARP Request Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_ARP_REQ,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s arp request rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		goto set_arp_req_failed;
	}

	/* Setting the ARP Response Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_ARP_REP,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s arp response rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		goto set_arp_rep_failed;
	}

	return 0;

set_arp_rep_failed:
	hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					 HINIC3_PKT_TYPE_ARP_REQ,
					 ethertype_filter, !add);

set_arp_req_failed:
	hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					 HINIC3_PKT_TYPE_ARP,
					 ethertype_filter, !add);

	return ret;
}

static int
hinic3_flow_set_slow_filter(struct rte_eth_dev *dev,
			    struct hinic3_ethertype_filter *ethertype_filter,
			    bool add)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	/* Setting the LACP Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_LACP,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s lacp fdir rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		return ret;
	}

	/* Setting the OAM Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_OAM,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s oam rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		goto set_arp_oam_failed;
	}

	return 0;

set_arp_oam_failed:
	hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					 HINIC3_PKT_TYPE_LACP,
					 ethertype_filter, !add);

	return ret;
}

static int
hinic3_flow_set_lldp_filter(struct rte_eth_dev *dev,
			    struct hinic3_ethertype_filter *ethertype_filter,
			    bool add)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;

	/* Setting the LLDP Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_LLDP,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s lldp fdir rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		return ret;
	}

	/* Setting the CDCP Filter. */
	ret = hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					       HINIC3_PKT_TYPE_CDCP,
					       ethertype_filter, add);
	if (ret) {
		PMD_DRV_LOG(ERR, "%s cdcp fdir rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		goto set_arp_cdcp_failed;
	}

	return 0;

set_arp_cdcp_failed:
	hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
					 HINIC3_PKT_TYPE_LLDP,
					 ethertype_filter, !add);

	return ret;
}

static int
hinic3_flow_add_del_ethertype_filter_rule(struct rte_eth_dev *dev,
					  struct hinic3_ethertype_filter *ethertype_filter,
					  bool add)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_ethertype_filter_list *ethertype_list =
		&nic_dev->filter_ethertype_list;

	/* Check whether the transferred rule exists. */
	if (hinic3_ethertype_filter_lookup(ethertype_list,
					   ethertype_filter->ether_type)) {
		if (add) {
			PMD_DRV_LOG(ERR,
				"The rule already exists, can not to be added");
			return -EPERM;
		}
	} else {
		if (!add) {
			PMD_DRV_LOG(ERR,
				"The rule not exists, can not to be delete");
			return -EPERM;
		}
	}
	/* Create a filter based on the protocol type. */
	switch (ethertype_filter->ether_type) {
	case RTE_ETHER_TYPE_ARP:
		return hinic3_flow_set_arp_filter(dev, ethertype_filter, add);
	case RTE_ETHER_TYPE_RARP:
		return hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
			HINIC3_PKT_TYPE_RARP, ethertype_filter, add);

	case RTE_ETHER_TYPE_SLOW:
		return hinic3_flow_set_slow_filter(dev, ethertype_filter, add);

	case RTE_ETHER_TYPE_LLDP:
		return hinic3_flow_set_lldp_filter(dev, ethertype_filter, add);

	case RTE_ETHER_TYPE_CNM:
		return hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
			HINIC3_PKT_TYPE_CNM, ethertype_filter, add);

	case RTE_ETHER_TYPE_ECP:
		return hinic3_set_fdir_ethertype_filter(nic_dev->hwdev,
			HINIC3_PKT_TYPE_ECP, ethertype_filter, add);

	default:
		PMD_DRV_LOG(ERR, "Unknown ethertype %d queue_id %d",
			    ethertype_filter->ether_type,
			    ethertype_filter->queue);
		return -EPERM;
	}
}

static int
hinic3_flow_ethertype_rule_nums(struct hinic3_ethertype_filter *ethertype_filter)
{
	switch (ethertype_filter->ether_type) {
	case RTE_ETHER_TYPE_ARP:
		return HINIC3_ARP_RULE_NUM;
	case RTE_ETHER_TYPE_RARP:
		return HINIC3_RARP_RULE_NUM;
	case RTE_ETHER_TYPE_SLOW:
		return HINIC3_SLOW_RULE_NUM;
	case RTE_ETHER_TYPE_LLDP:
		return HINIC3_LLDP_RULE_NUM;
	case RTE_ETHER_TYPE_CNM:
		return HINIC3_CNM_RULE_NUM;
	case RTE_ETHER_TYPE_ECP:
		return HINIC3_ECP_RULE_NUM;

	default:
		PMD_DRV_LOG(ERR, "Unknown ethertype %d",
			    ethertype_filter->ether_type);
		return 0;
	}
}

/**
 * Add or delete an Ethernet type filter rule.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] ethertype_filter
 * Pointer to ethertype filter.
 * @param[in] add
 * This is a Boolean value (of the bool type) indicating whether the action to
 * be performed is to add (true) or delete (false) the Ethernet type filter
 * rule.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_flow_add_del_ethertype_filter(struct rte_eth_dev *dev,
				     struct hinic3_ethertype_filter *ethertype_filter,
				     bool add)
{
	/* Get dev private info. */
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	int ret;
	/* Add or remove an Ethernet type filter rule. */
	ret = hinic3_flow_add_del_ethertype_filter_rule(dev, ethertype_filter, add);

	if (ret) {
		PMD_DRV_LOG(ERR, "%s fdir ethertype rule failed, err: %d",
			    add ? "Add" : "Del", ret);
		return ret;
	}
	/*
	 * If a rule is added and the rule is the first rule, rule filtering is
	 * enabled. If a rule is deleted and the rule is the last one, rule
	 * filtering is disabled.
	 */
	if (add) {
		if (nic_dev->ethertype_rule_nums == 0) {
			ret = hinic3_set_fdir_tcam_rule_filter(nic_dev->hwdev, true);
			if (ret) {
				PMD_DRV_LOG(ERR,
					    "enable fdir rule failed, err: %d",
					    ret);
				goto enable_fdir_failed;
			}
		}
		nic_dev->ethertype_rule_nums =
			nic_dev->ethertype_rule_nums +
			hinic3_flow_ethertype_rule_nums(ethertype_filter);
	} else {
		nic_dev->ethertype_rule_nums =
			nic_dev->ethertype_rule_nums -
			hinic3_flow_ethertype_rule_nums(ethertype_filter);

		if (!(nic_dev->ethertype_rule_nums + nic_dev->tcam_rule_nums)) {
			ret = hinic3_set_fdir_tcam_rule_filter(nic_dev->hwdev, false);
			if (ret) {
				PMD_DRV_LOG(ERR,
					    "disable fdir rule failed, err: %d",
					    ret);
			}
		}
	}

	return 0;

enable_fdir_failed:
	hinic3_flow_add_del_ethertype_filter_rule(dev, ethertype_filter, !add);
	return ret;
}
