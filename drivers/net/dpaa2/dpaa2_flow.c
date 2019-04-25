/* * SPDX-License-Identifier: BSD-3-Clause
 *   Copyright 2018 NXP
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include <fsl_dpni.h>
#include <fsl_dpkg.h>

#include <dpaa2_ethdev.h>
#include <dpaa2_pmd_logs.h>

struct rte_flow {
	LIST_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	struct dpni_rule_cfg rule;
	uint8_t key_size;
	uint8_t tc_id;
	uint8_t flow_type;
	uint8_t index;
	enum rte_flow_action_type action;
	uint16_t flow_id;
};

/* Layout for rule compositions for supported patterns */
/* TODO: Current design only supports Ethernet + IPv4 based classification. */
/* So corresponding offset macros are valid only. Rest are placeholder for */
/* now. Once support for other netwrok headers will be added then */
/* corresponding macros will be updated with correct values*/
#define DPAA2_CLS_RULE_OFFSET_ETH	0	/*Start of buffer*/
#define DPAA2_CLS_RULE_OFFSET_VLAN	14	/* DPAA2_CLS_RULE_OFFSET_ETH */
						/*	+ Sizeof Eth fields  */
#define DPAA2_CLS_RULE_OFFSET_IPV4	14	/* DPAA2_CLS_RULE_OFFSET_VLAN */
						/*	+ Sizeof VLAN fields */
#define DPAA2_CLS_RULE_OFFSET_IPV6	25	/* DPAA2_CLS_RULE_OFFSET_IPV4 */
						/*	+ Sizeof IPV4 fields */
#define DPAA2_CLS_RULE_OFFSET_ICMP	58	/* DPAA2_CLS_RULE_OFFSET_IPV6 */
						/*	+ Sizeof IPV6 fields */
#define DPAA2_CLS_RULE_OFFSET_UDP	60	/* DPAA2_CLS_RULE_OFFSET_ICMP */
						/*	+ Sizeof ICMP fields */
#define DPAA2_CLS_RULE_OFFSET_TCP	64	/* DPAA2_CLS_RULE_OFFSET_UDP  */
						/*	+ Sizeof UDP fields  */
#define DPAA2_CLS_RULE_OFFSET_SCTP	68	/* DPAA2_CLS_RULE_OFFSET_TCP  */
						/*	+ Sizeof TCP fields  */
#define DPAA2_CLS_RULE_OFFSET_GRE	72	/* DPAA2_CLS_RULE_OFFSET_SCTP */
						/*	+ Sizeof SCTP fields */

static const
enum rte_flow_item_type dpaa2_supported_pattern_type[] = {
	RTE_FLOW_ITEM_TYPE_END,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_GRE,
};

static const
enum rte_flow_action_type dpaa2_supported_action_type[] = {
	RTE_FLOW_ACTION_TYPE_END,
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_RSS
};

enum rte_filter_type dpaa2_filter_type = RTE_ETH_FILTER_NONE;
static const void *default_mask;

static int
dpaa2_configure_flow_eth(struct rte_flow *flow,
			 struct rte_eth_dev *dev,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_item *pattern,
			 const struct rte_flow_action actions[] __rte_unused,
			 struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_eth *spec, *mask;

	/* TODO: Currently upper bound of range parameter is not implemented */
	const struct rte_flow_item_eth *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	/* TODO: pattern is an array of 9 elements where 9th pattern element */
	/* is for QoS table and 1-8th pattern element is for FS tables. */
	/* It can be changed to macro. */
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_ETH;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_ETH_SA;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_ETH;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_ETH_DA;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_ETH;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_ETH_TYPE;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_ETH;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_ETH_SA;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_ETH;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_ETH_DA;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_ETH;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_ETH_TYPE;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_eth *)pattern->spec;
	last	= (const struct rte_flow_item_eth *)pattern->last;
	mask	= (const struct rte_flow_item_eth *)
			(pattern->mask ? pattern->mask : default_mask);

	/* Key rule */
	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_ETH;
	memcpy((void *)key_iova, (const void *)(spec->src.addr_bytes),
						sizeof(struct ether_addr));
	key_iova += sizeof(struct ether_addr);
	memcpy((void *)key_iova, (const void *)(spec->dst.addr_bytes),
						sizeof(struct ether_addr));
	key_iova += sizeof(struct ether_addr);
	memcpy((void *)key_iova, (const void *)(&spec->type),
						sizeof(rte_be16_t));

	/* Key mask */
	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_ETH;
	memcpy((void *)mask_iova, (const void *)(mask->src.addr_bytes),
						sizeof(struct ether_addr));
	mask_iova += sizeof(struct ether_addr);
	memcpy((void *)mask_iova, (const void *)(mask->dst.addr_bytes),
						sizeof(struct ether_addr));
	mask_iova += sizeof(struct ether_addr);
	memcpy((void *)mask_iova, (const void *)(&mask->type),
						sizeof(rte_be16_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_ETH +
				((2  * sizeof(struct ether_addr)) +
				sizeof(rte_be16_t)));
	return device_configured;
}

static int
dpaa2_configure_flow_vlan(struct rte_flow *flow,
			  struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item *pattern,
			  const struct rte_flow_action actions[] __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_vlan *spec, *mask;

	const struct rte_flow_item_vlan *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/*  more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}


	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_VLAN;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_VLAN_TCI;
		priv->extract.qos_key_cfg.num_extracts++;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_VLAN;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_VLAN_TCI;
		priv->extract.fs_key_cfg[group].num_extracts++;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_vlan *)pattern->spec;
	last	= (const struct rte_flow_item_vlan *)pattern->last;
	mask	= (const struct rte_flow_item_vlan *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_VLAN;
	memcpy((void *)key_iova, (const void *)(&spec->tci),
							sizeof(rte_be16_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_VLAN;
	memcpy((void *)mask_iova, (const void *)(&mask->tci),
							sizeof(rte_be16_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_VLAN + sizeof(rte_be16_t));
	return device_configured;
}

static int
dpaa2_configure_flow_ipv4(struct rte_flow *flow,
			  struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item *pattern,
			  const struct rte_flow_action actions[] __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_ipv4 *spec, *mask;

	const struct rte_flow_item_ipv4 *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_SRC;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_DST;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_SRC;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_DST;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_ipv4 *)pattern->spec;
	last	= (const struct rte_flow_item_ipv4 *)pattern->last;
	mask	= (const struct rte_flow_item_ipv4 *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_IPV4;
	memcpy((void *)key_iova, (const void *)&spec->hdr.src_addr,
							sizeof(uint32_t));
	key_iova += sizeof(uint32_t);
	memcpy((void *)key_iova, (const void *)&spec->hdr.dst_addr,
							sizeof(uint32_t));
	key_iova += sizeof(uint32_t);
	memcpy((void *)key_iova, (const void *)&spec->hdr.next_proto_id,
							sizeof(uint8_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_IPV4;
	memcpy((void *)mask_iova, (const void *)&mask->hdr.src_addr,
							sizeof(uint32_t));
	mask_iova += sizeof(uint32_t);
	memcpy((void *)mask_iova, (const void *)&mask->hdr.dst_addr,
							sizeof(uint32_t));
	mask_iova += sizeof(uint32_t);
	memcpy((void *)mask_iova, (const void *)&mask->hdr.next_proto_id,
							sizeof(uint8_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_IPV4 +
				(2 * sizeof(uint32_t)) + sizeof(uint8_t));

	return device_configured;
}

static int
dpaa2_configure_flow_ipv6(struct rte_flow *flow,
			  struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item *pattern,
			  const struct rte_flow_action actions[] __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_ipv6 *spec, *mask;

	const struct rte_flow_item_ipv6 *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more	than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_SRC;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_DST;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_SRC;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_DST;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_ipv6 *)pattern->spec;
	last	= (const struct rte_flow_item_ipv6 *)pattern->last;
	mask	= (const struct rte_flow_item_ipv6 *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_IPV6;
	memcpy((void *)key_iova, (const void *)(spec->hdr.src_addr),
						sizeof(spec->hdr.src_addr));
	key_iova += sizeof(spec->hdr.src_addr);
	memcpy((void *)key_iova, (const void *)(spec->hdr.dst_addr),
						sizeof(spec->hdr.dst_addr));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_IPV6;
	memcpy((void *)mask_iova, (const void *)(mask->hdr.src_addr),
						sizeof(mask->hdr.src_addr));
	mask_iova += sizeof(mask->hdr.src_addr);
	memcpy((void *)mask_iova, (const void *)(mask->hdr.dst_addr),
						sizeof(mask->hdr.dst_addr));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_IPV6 +
					sizeof(spec->hdr.src_addr) +
					sizeof(mask->hdr.dst_addr));
	return device_configured;
}

static int
dpaa2_configure_flow_icmp(struct rte_flow *flow,
			  struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item *pattern,
			  const struct rte_flow_action actions[] __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_icmp *spec, *mask;

	const struct rte_flow_item_icmp *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_ICMP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_ICMP_TYPE;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_ICMP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_ICMP_CODE;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_ICMP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_ICMP_TYPE;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_ICMP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_ICMP_CODE;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_icmp *)pattern->spec;
	last	= (const struct rte_flow_item_icmp *)pattern->last;
	mask	= (const struct rte_flow_item_icmp *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_ICMP;
	memcpy((void *)key_iova, (const void *)&spec->hdr.icmp_type,
							sizeof(uint8_t));
	key_iova += sizeof(uint8_t);
	memcpy((void *)key_iova, (const void *)&spec->hdr.icmp_code,
							sizeof(uint8_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_ICMP;
	memcpy((void *)mask_iova, (const void *)&mask->hdr.icmp_type,
							sizeof(uint8_t));
	key_iova += sizeof(uint8_t);
	memcpy((void *)mask_iova, (const void *)&mask->hdr.icmp_code,
							sizeof(uint8_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_ICMP +
				(2 * sizeof(uint8_t)));

	return device_configured;
}

static int
dpaa2_configure_flow_udp(struct rte_flow *flow,
			 struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item *pattern,
			  const struct rte_flow_action actions[] __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_udp *spec, *mask;

	const struct rte_flow_item_udp *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			 entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_UDP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_UDP_PORT_SRC;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type = DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_UDP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_UDP_PORT_DST;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_UDP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_UDP_PORT_SRC;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_UDP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_UDP_PORT_DST;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_udp *)pattern->spec;
	last	= (const struct rte_flow_item_udp *)pattern->last;
	mask	= (const struct rte_flow_item_udp *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_IPV4 +
					(2 * sizeof(uint32_t));
	memset((void *)key_iova, 0x11, sizeof(uint8_t));
	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_UDP;
	memcpy((void *)key_iova, (const void *)(&spec->hdr.src_port),
							sizeof(uint16_t));
	key_iova +=  sizeof(uint16_t);
	memcpy((void *)key_iova, (const void *)(&spec->hdr.dst_port),
							sizeof(uint16_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_UDP;
	memcpy((void *)mask_iova, (const void *)(&mask->hdr.src_port),
							sizeof(uint16_t));
	mask_iova +=  sizeof(uint16_t);
	memcpy((void *)mask_iova, (const void *)(&mask->hdr.dst_port),
							sizeof(uint16_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_UDP +
				(2 * sizeof(uint16_t)));

	return device_configured;
}

static int
dpaa2_configure_flow_tcp(struct rte_flow *flow,
			 struct rte_eth_dev *dev,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_item *pattern,
			 const struct rte_flow_action actions[] __rte_unused,
			 struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_tcp *spec, *mask;

	const struct rte_flow_item_tcp *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too.*/
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_TCP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_TCP_PORT_SRC;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_TCP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_TCP_PORT_DST;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_TCP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_TCP_PORT_SRC;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_TCP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_TCP_PORT_DST;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_tcp *)pattern->spec;
	last	= (const struct rte_flow_item_tcp *)pattern->last;
	mask	= (const struct rte_flow_item_tcp *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_IPV4 +
					(2 * sizeof(uint32_t));
	memset((void *)key_iova, 0x06, sizeof(uint8_t));
	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_TCP;
	memcpy((void *)key_iova, (const void *)(&spec->hdr.src_port),
							sizeof(uint16_t));
	key_iova += sizeof(uint16_t);
	memcpy((void *)key_iova, (const void *)(&spec->hdr.dst_port),
							sizeof(uint16_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_TCP;
	memcpy((void *)mask_iova, (const void *)(&mask->hdr.src_port),
							sizeof(uint16_t));
	mask_iova += sizeof(uint16_t);
	memcpy((void *)mask_iova, (const void *)(&mask->hdr.dst_port),
							sizeof(uint16_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_TCP +
				(2 * sizeof(uint16_t)));

	return device_configured;
}

static int
dpaa2_configure_flow_sctp(struct rte_flow *flow,
			  struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_item *pattern,
			  const struct rte_flow_action actions[] __rte_unused,
			  struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_sctp *spec, *mask;

	const struct rte_flow_item_sctp *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too. */
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_SCTP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_SCTP_PORT_SRC;
		index++;

		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_SCTP;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_SCTP_PORT_DST;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_IP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_IP_PROTO;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_SCTP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_SCTP_PORT_SRC;
		index++;

		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_SCTP;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_SCTP_PORT_DST;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_sctp *)pattern->spec;
	last	= (const struct rte_flow_item_sctp *)pattern->last;
	mask	= (const struct rte_flow_item_sctp *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_IPV4 +
						(2 * sizeof(uint32_t));
	memset((void *)key_iova, 0x84, sizeof(uint8_t));
	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_SCTP;
	memcpy((void *)key_iova, (const void *)(&spec->hdr.src_port),
							sizeof(uint16_t));
	key_iova += sizeof(uint16_t);
	memcpy((void *)key_iova, (const void *)(&spec->hdr.dst_port),
							sizeof(uint16_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_SCTP;
	memcpy((void *)mask_iova, (const void *)(&mask->hdr.src_port),
							sizeof(uint16_t));
	mask_iova += sizeof(uint16_t);
	memcpy((void *)mask_iova, (const void *)(&mask->hdr.dst_port),
							sizeof(uint16_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_SCTP +
				(2 * sizeof(uint16_t)));
	return device_configured;
}

static int
dpaa2_configure_flow_gre(struct rte_flow *flow,
			 struct rte_eth_dev *dev,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_item *pattern,
			 const struct rte_flow_action actions[] __rte_unused,
			 struct rte_flow_error *error __rte_unused)
{
	int index, j = 0;
	size_t key_iova;
	size_t mask_iova;
	int device_configured = 0, entry_found = 0;
	uint32_t group;
	const struct rte_flow_item_gre *spec, *mask;

	const struct rte_flow_item_gre *last __rte_unused;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too. */
	if (priv->pattern[8].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	if (priv->pattern[group].item_count >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Maximum limit for different pattern type = %d\n",
						DPKG_MAX_NUM_OF_EXTRACTS);
		return -ENOTSUP;
	}

	for (j = 0; j < priv->pattern[8].item_count; j++) {
		if (priv->pattern[8].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[8].pattern_type[j] = pattern->type;
		priv->pattern[8].item_count++;
		device_configured |= DPAA2_QOS_TABLE_RECONFIGURE;
	}

	entry_found = 0;
	for (j = 0; j < priv->pattern[group].item_count; j++) {
		if (priv->pattern[group].pattern_type[j] != pattern->type) {
			continue;
		} else {
			entry_found = 1;
			break;
		}
	}

	if (!entry_found) {
		priv->pattern[group].pattern_type[j] = pattern->type;
		priv->pattern[group].item_count++;
		device_configured |= DPAA2_FS_TABLE_RECONFIGURE;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->index = attr->priority;

	if (device_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
		index = priv->extract.qos_key_cfg.num_extracts;
		priv->extract.qos_key_cfg.extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.prot = NET_PROT_GRE;
		priv->extract.qos_key_cfg.extracts[index].extract.from_hdr.field = NH_FLD_GRE_TYPE;
		index++;

		priv->extract.qos_key_cfg.num_extracts = index;
	}

	if (device_configured & DPAA2_FS_TABLE_RECONFIGURE) {
		index = priv->extract.fs_key_cfg[group].num_extracts;
		priv->extract.fs_key_cfg[group].extracts[index].type =
							DPKG_EXTRACT_FROM_HDR;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.type = DPKG_FULL_FIELD;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.prot = NET_PROT_GRE;
		priv->extract.fs_key_cfg[group].extracts[index].extract.from_hdr.field = NH_FLD_GRE_TYPE;
		index++;

		priv->extract.fs_key_cfg[group].num_extracts = index;
	}

	/* Parse pattern list to get the matching parameters */
	spec	= (const struct rte_flow_item_gre *)pattern->spec;
	last	= (const struct rte_flow_item_gre *)pattern->last;
	mask	= (const struct rte_flow_item_gre *)
			(pattern->mask ? pattern->mask : default_mask);

	key_iova = flow->rule.key_iova + DPAA2_CLS_RULE_OFFSET_GRE;
	memcpy((void *)key_iova, (const void *)(&spec->protocol),
							sizeof(rte_be16_t));

	mask_iova = flow->rule.mask_iova + DPAA2_CLS_RULE_OFFSET_GRE;
	memcpy((void *)mask_iova, (const void *)(&mask->protocol),
							sizeof(rte_be16_t));

	flow->rule.key_size = (DPAA2_CLS_RULE_OFFSET_GRE + sizeof(rte_be16_t));

	return device_configured;
}

static int
dpaa2_generic_flow_set(struct rte_flow *flow,
		       struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_error *error)
{
	const struct rte_flow_action_queue *dest_queue;
	const struct rte_flow_action_rss *rss_conf;
	uint16_t index;
	int is_keycfg_configured = 0, end_of_list = 0;
	int ret = 0, i = 0, j = 0;
	struct dpni_attr nic_attr;
	struct dpni_rx_tc_dist_cfg tc_cfg;
	struct dpni_qos_tbl_cfg qos_cfg;
	struct dpkg_profile_cfg key_cfg;
	struct dpni_fs_action_cfg action;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	size_t param;
	struct rte_flow *curr = LIST_FIRST(&priv->flows);

	/* Parse pattern list to get the matching parameters */
	while (!end_of_list) {
		switch (pattern[i].type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			is_keycfg_configured = dpaa2_configure_flow_eth(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			is_keycfg_configured = dpaa2_configure_flow_vlan(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			is_keycfg_configured = dpaa2_configure_flow_ipv4(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			is_keycfg_configured = dpaa2_configure_flow_ipv6(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			is_keycfg_configured = dpaa2_configure_flow_icmp(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			is_keycfg_configured = dpaa2_configure_flow_udp(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			is_keycfg_configured = dpaa2_configure_flow_tcp(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			is_keycfg_configured = dpaa2_configure_flow_sctp(flow,
									dev, attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			is_keycfg_configured = dpaa2_configure_flow_gre(flow,
									dev,
									attr,
									&pattern[i],
									actions,
									error);
			break;
		case RTE_FLOW_ITEM_TYPE_END:
			end_of_list = 1;
			break; /*End of List*/
		default:
			DPAA2_PMD_ERR("Invalid action type");
			ret = -ENOTSUP;
			break;
		}
		i++;
	}

	/* Let's parse action on matching traffic */
	end_of_list = 0;
	while (!end_of_list) {
		switch (actions[j].type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			dest_queue = (const struct rte_flow_action_queue *)(actions[j].conf);
			flow->flow_id = dest_queue->index;
			flow->action = RTE_FLOW_ACTION_TYPE_QUEUE;
			memset(&action, 0, sizeof(struct dpni_fs_action_cfg));
			action.flow_id = flow->flow_id;
			if (is_keycfg_configured & DPAA2_QOS_TABLE_RECONFIGURE) {
				if (dpkg_prepare_key_cfg(&priv->extract.qos_key_cfg,
							 (uint8_t *)(size_t)priv->extract.qos_extract_param) < 0) {
					DPAA2_PMD_ERR(
					"Unable to prepare extract parameters");
					return -1;
				}

				memset(&qos_cfg, 0, sizeof(struct dpni_qos_tbl_cfg));
				qos_cfg.discard_on_miss = true;
				qos_cfg.keep_entries = true;
				qos_cfg.key_cfg_iova = (size_t)priv->extract.qos_extract_param;
				ret = dpni_set_qos_table(dpni, CMD_PRI_LOW,
							 priv->token, &qos_cfg);
				if (ret < 0) {
					DPAA2_PMD_ERR(
					"Distribution cannot be configured.(%d)"
					, ret);
					return -1;
				}
			}
			if (is_keycfg_configured & DPAA2_FS_TABLE_RECONFIGURE) {
				if (dpkg_prepare_key_cfg(&priv->extract.fs_key_cfg[flow->tc_id],
						(uint8_t *)(size_t)priv->extract.fs_extract_param[flow->tc_id]) < 0) {
					DPAA2_PMD_ERR(
					"Unable to prepare extract parameters");
					return -1;
				}

				memset(&tc_cfg, 0, sizeof(struct dpni_rx_tc_dist_cfg));
				tc_cfg.dist_size = priv->nb_rx_queues / priv->num_rx_tc;
				tc_cfg.dist_mode = DPNI_DIST_MODE_FS;
				tc_cfg.key_cfg_iova =
					(uint64_t)priv->extract.fs_extract_param[flow->tc_id];
				tc_cfg.fs_cfg.miss_action = DPNI_FS_MISS_DROP;
				tc_cfg.fs_cfg.keep_entries = true;
				ret = dpni_set_rx_tc_dist(dpni, CMD_PRI_LOW,
							 priv->token,
							 flow->tc_id, &tc_cfg);
				if (ret < 0) {
					DPAA2_PMD_ERR(
					"Distribution cannot be configured.(%d)"
					, ret);
					return -1;
				}
			}
			/* Configure QoS table first */
			memset(&nic_attr, 0, sizeof(struct dpni_attr));
			ret = dpni_get_attributes(dpni, CMD_PRI_LOW,
						 priv->token, &nic_attr);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"Failure to get attribute. dpni@%p err code(%d)\n",
				dpni, ret);
				return ret;
			}

			action.flow_id = action.flow_id % nic_attr.num_rx_tcs;
			index = flow->index + (flow->tc_id * nic_attr.fs_entries);
			ret = dpni_add_qos_entry(dpni, CMD_PRI_LOW,
						priv->token, &flow->rule,
						flow->tc_id, index);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"Error in addnig entry to QoS table(%d)", ret);
				return ret;
			}

			/* Then Configure FS table */
			ret = dpni_add_fs_entry(dpni, CMD_PRI_LOW, priv->token,
						flow->tc_id, flow->index,
						&flow->rule, &action);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"Error in adding entry to FS table(%d)", ret);
				return ret;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = dpni_get_attributes(dpni, CMD_PRI_LOW,
						 priv->token, &nic_attr);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"Failure to get attribute. dpni@%p err code(%d)\n",
				dpni, ret);
				return ret;
			}
			rss_conf = (const struct rte_flow_action_rss *)(actions[j].conf);
			for (i = 0; i < (int)rss_conf->queue_num; i++) {
				if (rss_conf->queue[i] < (attr->group * nic_attr.num_queues) ||
				    rss_conf->queue[i] >= ((attr->group + 1) * nic_attr.num_queues)) {
					DPAA2_PMD_ERR(
					"Queue/Group combination are not supported\n");
					return -ENOTSUP;
				}
			}

			flow->action = RTE_FLOW_ACTION_TYPE_RSS;
			ret = dpaa2_distset_to_dpkg_profile_cfg(rss_conf->types,
								&key_cfg);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"unable to set flow distribution.please check queue config\n");
				return ret;
			}

			/* Allocate DMA'ble memory to write the rules */
			param = (size_t)rte_malloc(NULL, 256, 64);
			if (!param) {
				DPAA2_PMD_ERR("Memory allocation failure\n");
				return -1;
			}

			if (dpkg_prepare_key_cfg(&key_cfg, (uint8_t *)param) < 0) {
				DPAA2_PMD_ERR(
				"Unable to prepare extract parameters");
				rte_free((void *)param);
				return -1;
			}

			memset(&tc_cfg, 0, sizeof(struct dpni_rx_tc_dist_cfg));
			tc_cfg.dist_size = rss_conf->queue_num;
			tc_cfg.dist_mode = DPNI_DIST_MODE_HASH;
			tc_cfg.key_cfg_iova = (size_t)param;
			tc_cfg.fs_cfg.miss_action = DPNI_FS_MISS_DROP;

			ret = dpni_set_rx_tc_dist(dpni, CMD_PRI_LOW,
						 priv->token, flow->tc_id,
						 &tc_cfg);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"Distribution cannot be configured: %d\n", ret);
				rte_free((void *)param);
				return -1;
			}

			rte_free((void *)param);
			if (is_keycfg_configured & DPAA2_FS_TABLE_RECONFIGURE) {
				if (dpkg_prepare_key_cfg(&priv->extract.qos_key_cfg,
					(uint8_t *)(size_t)priv->extract.qos_extract_param) < 0) {
					DPAA2_PMD_ERR(
					"Unable to prepare extract parameters");
					return -1;
				}
				memset(&qos_cfg, 0,
					sizeof(struct dpni_qos_tbl_cfg));
				qos_cfg.discard_on_miss = true;
				qos_cfg.keep_entries = true;
				qos_cfg.key_cfg_iova = (size_t)priv->extract.qos_extract_param;
				ret = dpni_set_qos_table(dpni, CMD_PRI_LOW,
							 priv->token, &qos_cfg);
				if (ret < 0) {
					DPAA2_PMD_ERR(
					"Distribution can not be configured(%d)\n",
					ret);
					return -1;
				}
			}

			/* Add Rule into QoS table */
			index = flow->index + (flow->tc_id * nic_attr.fs_entries);
			ret = dpni_add_qos_entry(dpni, CMD_PRI_LOW, priv->token,
						&flow->rule, flow->tc_id,
						index);
			if (ret < 0) {
				DPAA2_PMD_ERR(
				"Error in entry addition in QoS table(%d)",
				ret);
				return ret;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			end_of_list = 1;
			break;
		default:
			DPAA2_PMD_ERR("Invalid action type");
			ret = -ENOTSUP;
			break;
		}
		j++;
	}

	if (!ret) {
		/* New rules are inserted. */
		if (!curr) {
			LIST_INSERT_HEAD(&priv->flows, flow, next);
		} else {
			while (LIST_NEXT(curr, next))
				curr = LIST_NEXT(curr, next);
			LIST_INSERT_AFTER(curr, flow, next);
		}
	}
	return ret;
}

static inline int
dpaa2_dev_verify_attr(struct dpni_attr *dpni_attr,
		      const struct rte_flow_attr *attr)
{
	int ret = 0;

	if (unlikely(attr->group >= dpni_attr->num_rx_tcs)) {
		DPAA2_PMD_ERR("Priority group is out of range\n");
		ret = -ENOTSUP;
	}
	if (unlikely(attr->priority >= dpni_attr->fs_entries)) {
		DPAA2_PMD_ERR("Priority within the group is out of range\n");
		ret = -ENOTSUP;
	}
	if (unlikely(attr->egress)) {
		DPAA2_PMD_ERR(
			"Flow configuration is not supported on egress side\n");
		ret = -ENOTSUP;
	}
	if (unlikely(!attr->ingress)) {
		DPAA2_PMD_ERR("Ingress flag must be configured\n");
		ret = -EINVAL;
	}
	return ret;
}

static inline void
dpaa2_dev_update_default_mask(const struct rte_flow_item *pattern)
{
	switch (pattern->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		default_mask = (const void *)&rte_flow_item_eth_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		default_mask = (const void *)&rte_flow_item_vlan_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		default_mask = (const void *)&rte_flow_item_ipv4_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		default_mask = (const void *)&rte_flow_item_ipv6_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP:
		default_mask = (const void *)&rte_flow_item_icmp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		default_mask = (const void *)&rte_flow_item_udp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		default_mask = (const void *)&rte_flow_item_tcp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		default_mask = (const void *)&rte_flow_item_sctp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		default_mask = (const void *)&rte_flow_item_gre_mask;
		break;
	default:
		DPAA2_PMD_ERR("Invalid pattern type");
	}
}

static inline int
dpaa2_dev_verify_patterns(struct dpaa2_dev_priv *dev_priv,
			  const struct rte_flow_item pattern[])
{
	unsigned int i, j, k, is_found = 0;
	int ret = 0;

	for (j = 0; pattern[j].type != RTE_FLOW_ITEM_TYPE_END; j++) {
		for (i = 0; i < RTE_DIM(dpaa2_supported_pattern_type); i++) {
			if (dpaa2_supported_pattern_type[i] == pattern[j].type) {
				is_found = 1;
				break;
			}
		}
		if (!is_found) {
			ret = -ENOTSUP;
			break;
		}
	}
	/* Lets verify other combinations of given pattern rules */
	for (j = 0; pattern[j].type != RTE_FLOW_ITEM_TYPE_END; j++) {
		if (!pattern[j].spec) {
			ret = -EINVAL;
			break;
		}
		if ((pattern[j].last) && (!pattern[j].mask))
			dpaa2_dev_update_default_mask(&pattern[j]);
	}

	/* DPAA2 platform has a limitation that extract parameter can not be */
	/* more	than DPKG_MAX_NUM_OF_EXTRACTS. Verify this limitation too. */
	for (i = 0; pattern[i].type != RTE_FLOW_ITEM_TYPE_END; i++) {
		for (j = 0; j < MAX_TCS + 1; j++) {
				for (k = 0; k < DPKG_MAX_NUM_OF_EXTRACTS; k++) {
					if (dev_priv->pattern[j].pattern_type[k] == pattern[i].type)
						break;
				}
			if (dev_priv->pattern[j].item_count >= DPKG_MAX_NUM_OF_EXTRACTS)
				ret = -ENOTSUP;
		}
	}
	return ret;
}

static inline int
dpaa2_dev_verify_actions(const struct rte_flow_action actions[])
{
	unsigned int i, j, is_found = 0;
	int ret = 0;

	for (j = 0; actions[j].type != RTE_FLOW_ACTION_TYPE_END; j++) {
		for (i = 0; i < RTE_DIM(dpaa2_supported_action_type); i++) {
			if (dpaa2_supported_action_type[i] == actions[j].type) {
				is_found = 1;
				break;
			}
		}
		if (!is_found) {
			ret = -ENOTSUP;
			break;
		}
	}
	for (j = 0; actions[j].type != RTE_FLOW_ACTION_TYPE_END; j++) {
		if ((actions[j].type != RTE_FLOW_ACTION_TYPE_DROP) && (!actions[j].conf))
			ret = -EINVAL;
	}
	return ret;
}

static
int dpaa2_flow_validate(struct rte_eth_dev *dev,
			const struct rte_flow_attr *flow_attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_flow_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpni_attr dpni_attr;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	uint16_t token = priv->token;
	int ret = 0;

	memset(&dpni_attr, 0, sizeof(struct dpni_attr));
	ret = dpni_get_attributes(dpni, CMD_PRI_LOW, token, &dpni_attr);
	if (ret < 0) {
		DPAA2_PMD_ERR(
			"Failure to get dpni@%p attribute, err code  %d\n",
			dpni, ret);
		rte_flow_error_set(error, EPERM,
			   RTE_FLOW_ERROR_TYPE_ATTR,
			   flow_attr, "invalid");
		return ret;
	}

	/* Verify input attributes */
	ret = dpaa2_dev_verify_attr(&dpni_attr, flow_attr);
	if (ret < 0) {
		DPAA2_PMD_ERR(
			"Invalid attributes are given\n");
		rte_flow_error_set(error, EPERM,
			   RTE_FLOW_ERROR_TYPE_ATTR,
			   flow_attr, "invalid");
		goto not_valid_params;
	}
	/* Verify input pattern list */
	ret = dpaa2_dev_verify_patterns(priv, pattern);
	if (ret < 0) {
		DPAA2_PMD_ERR(
			"Invalid pattern list is given\n");
		rte_flow_error_set(error, EPERM,
			   RTE_FLOW_ERROR_TYPE_ITEM,
			   pattern, "invalid");
		goto not_valid_params;
	}
	/* Verify input action list */
	ret = dpaa2_dev_verify_actions(actions);
	if (ret < 0) {
		DPAA2_PMD_ERR(
			"Invalid action list is given\n");
		rte_flow_error_set(error, EPERM,
			   RTE_FLOW_ERROR_TYPE_ACTION,
			   actions, "invalid");
		goto not_valid_params;
	}
not_valid_params:
	return ret;
}

static
struct rte_flow *dpaa2_flow_create(struct rte_eth_dev *dev,
				   const struct rte_flow_attr *attr,
				   const struct rte_flow_item pattern[],
				   const struct rte_flow_action actions[],
				   struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;
	size_t key_iova = 0, mask_iova = 0;
	int ret;

	flow = rte_malloc(NULL, sizeof(struct rte_flow), RTE_CACHE_LINE_SIZE);
	if (!flow) {
		DPAA2_PMD_ERR("Failure to allocate memory for flow");
		goto mem_failure;
	}
	/* Allocate DMA'ble memory to write the rules */
	key_iova = (size_t)rte_malloc(NULL, 256, 64);
	if (!key_iova) {
		DPAA2_PMD_ERR(
			"Memory allocation failure for rule configration\n");
		goto mem_failure;
	}
	mask_iova = (size_t)rte_malloc(NULL, 256, 64);
	if (!mask_iova) {
		DPAA2_PMD_ERR(
			"Memory allocation failure for rule configration\n");
		goto mem_failure;
	}

	flow->rule.key_iova = key_iova;
	flow->rule.mask_iova = mask_iova;
	flow->rule.key_size = 0;

	switch (dpaa2_filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		ret = dpaa2_generic_flow_set(flow, dev, attr, pattern,
					     actions, error);
		if (ret < 0) {
			if (error->type > RTE_FLOW_ERROR_TYPE_ACTION)
				rte_flow_error_set(error, EPERM,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						attr, "unknown");
			DPAA2_PMD_ERR(
			"Failure to create flow, return code (%d)", ret);
			goto creation_error;
		}
		break;
	default:
		DPAA2_PMD_ERR("Filter type (%d) not supported",
		dpaa2_filter_type);
		break;
	}

	return flow;
mem_failure:
	rte_flow_error_set(error, EPERM,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL, "memory alloc");
creation_error:
	rte_free((void *)flow);
	rte_free((void *)key_iova);
	rte_free((void *)mask_iova);

	return NULL;
}

static
int dpaa2_flow_destroy(struct rte_eth_dev *dev,
		       struct rte_flow *flow,
		       struct rte_flow_error *error)
{
	int ret = 0;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	switch (flow->action) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		/* Remove entry from QoS table first */
		ret = dpni_remove_qos_entry(dpni, CMD_PRI_LOW, priv->token,
					   &flow->rule);
		if (ret < 0) {
			DPAA2_PMD_ERR(
				"Error in adding entry to QoS table(%d)", ret);
			goto error;
		}

		/* Then remove entry from FS table */
		ret = dpni_remove_fs_entry(dpni, CMD_PRI_LOW, priv->token,
					   flow->tc_id, &flow->rule);
		if (ret < 0) {
			DPAA2_PMD_ERR(
				"Error in entry addition in FS table(%d)", ret);
			goto error;
		}
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		ret = dpni_remove_qos_entry(dpni, CMD_PRI_LOW, priv->token,
					   &flow->rule);
		if (ret < 0) {
			DPAA2_PMD_ERR(
			"Error in entry addition in QoS table(%d)", ret);
			goto error;
		}
		break;
	default:
		DPAA2_PMD_ERR(
		"Action type (%d) is not supported", flow->action);
		ret = -ENOTSUP;
		break;
	}

	LIST_REMOVE(flow, next);
	/* Now free the flow */
	rte_free(flow);

error:
	if (ret)
		rte_flow_error_set(error, EPERM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "unknown");
	return ret;
}

/**
 * Destroy user-configured flow rules.
 *
 * This function skips internal flows rules.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
static int
dpaa2_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = LIST_FIRST(&priv->flows);

	while (flow) {
		struct rte_flow *next = LIST_NEXT(flow, next);

		dpaa2_flow_destroy(dev, flow, error);
		flow = next;
	}
	return 0;
}

static int
dpaa2_flow_query(struct rte_eth_dev *dev __rte_unused,
		struct rte_flow *flow __rte_unused,
		const struct rte_flow_action *actions __rte_unused,
		void *data __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	return 0;
}

/**
 * Clean up all flow rules.
 *
 * Unlike dpaa2_flow_flush(), this function takes care of all remaining flow
 * rules regardless of whether they are internal or user-configured.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
dpaa2_flow_clean(struct rte_eth_dev *dev)
{
	struct rte_flow *flow;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	while ((flow = LIST_FIRST(&priv->flows)))
		dpaa2_flow_destroy(dev, flow, NULL);
}

const struct rte_flow_ops dpaa2_flow_ops = {
	.create	= dpaa2_flow_create,
	.validate = dpaa2_flow_validate,
	.destroy = dpaa2_flow_destroy,
	.flush	= dpaa2_flow_flush,
	.query	= dpaa2_flow_query,
};
