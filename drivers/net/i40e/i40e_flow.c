/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>

#include "i40e_logs.h"
#include "base/i40e_type.h"
#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"

#define I40E_IPV4_TC_SHIFT	4
#define I40E_IPV6_TC_MASK	(0x00FF << I40E_IPV4_TC_SHIFT)
#define I40E_IPV6_FRAG_HEADER	44
#define I40E_TENANT_ARRAY_NUM	3
#define I40E_TCI_MASK		0xFFFF

static int i40e_flow_validate(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error);
static struct rte_flow *i40e_flow_create(struct rte_eth_dev *dev,
					 const struct rte_flow_attr *attr,
					 const struct rte_flow_item pattern[],
					 const struct rte_flow_action actions[],
					 struct rte_flow_error *error);
static int i40e_flow_destroy(struct rte_eth_dev *dev,
			     struct rte_flow *flow,
			     struct rte_flow_error *error);
static int i40e_flow_flush(struct rte_eth_dev *dev,
			   struct rte_flow_error *error);
static int
i40e_flow_parse_ethertype_pattern(struct rte_eth_dev *dev,
				  const struct rte_flow_item *pattern,
				  struct rte_flow_error *error,
				  struct rte_eth_ethertype_filter *filter);
static int i40e_flow_parse_ethertype_action(struct rte_eth_dev *dev,
				    const struct rte_flow_action *actions,
				    struct rte_flow_error *error,
				    struct rte_eth_ethertype_filter *filter);
static int i40e_flow_parse_fdir_pattern(struct rte_eth_dev *dev,
					const struct rte_flow_item *pattern,
					struct rte_flow_error *error,
					struct rte_eth_fdir_filter *filter);
static int i40e_flow_parse_fdir_action(struct rte_eth_dev *dev,
				       const struct rte_flow_action *actions,
				       struct rte_flow_error *error,
				       struct rte_eth_fdir_filter *filter);
static int i40e_flow_parse_tunnel_pattern(__rte_unused struct rte_eth_dev *dev,
				  const struct rte_flow_item *pattern,
				  struct rte_flow_error *error,
				  struct rte_eth_tunnel_filter_conf *filter);
static int i40e_flow_parse_tunnel_action(struct rte_eth_dev *dev,
				 const struct rte_flow_action *actions,
				 struct rte_flow_error *error,
				 struct rte_eth_tunnel_filter_conf *filter);
static int i40e_flow_parse_attr(const struct rte_flow_attr *attr,
				struct rte_flow_error *error);
static int i40e_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item pattern[],
				    const struct rte_flow_action actions[],
				    struct rte_flow_error *error,
				    union i40e_filter_t *filter);
static int i40e_flow_parse_fdir_filter(struct rte_eth_dev *dev,
				       const struct rte_flow_attr *attr,
				       const struct rte_flow_item pattern[],
				       const struct rte_flow_action actions[],
				       struct rte_flow_error *error,
				       union i40e_filter_t *filter);
static int i40e_flow_parse_tunnel_filter(struct rte_eth_dev *dev,
					 const struct rte_flow_attr *attr,
					 const struct rte_flow_item pattern[],
					 const struct rte_flow_action actions[],
					 struct rte_flow_error *error,
					 union i40e_filter_t *filter);
static int i40e_flow_destroy_ethertype_filter(struct i40e_pf *pf,
				      struct i40e_ethertype_filter *filter);
static int i40e_flow_destroy_tunnel_filter(struct i40e_pf *pf,
					   struct i40e_tunnel_filter *filter);
static int i40e_flow_flush_fdir_filter(struct i40e_pf *pf);
static int i40e_flow_flush_ethertype_filter(struct i40e_pf *pf);
static int i40e_flow_flush_tunnel_filter(struct i40e_pf *pf);

const struct rte_flow_ops i40e_flow_ops = {
	.validate = i40e_flow_validate,
	.create = i40e_flow_create,
	.destroy = i40e_flow_destroy,
	.flush = i40e_flow_flush,
};

union i40e_filter_t cons_filter;
enum rte_filter_type cons_filter_type = RTE_ETH_FILTER_NONE;

/* Pattern matched ethertype filter */
static enum rte_flow_item_type pattern_ethertype[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Pattern matched flow director filter */
static enum rte_flow_item_type pattern_fdir_ipv4[] = {
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp[] = {
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_udp_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp[] = {
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_tcp_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp[] = {
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv4_sctp_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6[] = {
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp[] = {
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_udp_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp[] = {
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_tcp_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp[] = {
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_fdir_ipv6_sctp_ext[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Pattern matched tunnel filter */
static enum rte_flow_item_type pattern_vxlan_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_vxlan_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_vxlan_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static enum rte_flow_item_type pattern_vxlan_4[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_END,
};

static struct i40e_valid_pattern i40e_supported_patterns[] = {
	/* Ethertype */
	{ pattern_ethertype, i40e_flow_parse_ethertype_filter },
	/* FDIR */
	{ pattern_fdir_ipv4, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_udp_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_tcp_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv4_sctp_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_udp_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_tcp_ext, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp, i40e_flow_parse_fdir_filter },
	{ pattern_fdir_ipv6_sctp_ext, i40e_flow_parse_fdir_filter },
	/* tunnel */
	{ pattern_vxlan_1, i40e_flow_parse_tunnel_filter },
	{ pattern_vxlan_2, i40e_flow_parse_tunnel_filter },
	{ pattern_vxlan_3, i40e_flow_parse_tunnel_filter },
	{ pattern_vxlan_4, i40e_flow_parse_tunnel_filter },
};

#define NEXT_ITEM_OF_ACTION(act, actions, index)                        \
	do {                                                            \
		act = actions + index;                                  \
		while (act->type == RTE_FLOW_ACTION_TYPE_VOID) {        \
			index++;                                        \
			act = actions + index;                          \
		}                                                       \
	} while (0)

/* Find the first VOID or non-VOID item pointer */
static const struct rte_flow_item *
i40e_find_first_item(const struct rte_flow_item *item, bool is_void)
{
	bool is_find;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (is_void)
			is_find = item->type == RTE_FLOW_ITEM_TYPE_VOID;
		else
			is_find = item->type != RTE_FLOW_ITEM_TYPE_VOID;
		if (is_find)
			break;
		item++;
	}
	return item;
}

/* Skip all VOID items of the pattern */
static void
i40e_pattern_skip_void_item(struct rte_flow_item *items,
			    const struct rte_flow_item *pattern)
{
	uint32_t cpy_count = 0;
	const struct rte_flow_item *pb = pattern, *pe = pattern;

	for (;;) {
		/* Find a non-void item first */
		pb = i40e_find_first_item(pb, false);
		if (pb->type == RTE_FLOW_ITEM_TYPE_END) {
			pe = pb;
			break;
		}

		/* Find a void item */
		pe = i40e_find_first_item(pb + 1, true);

		cpy_count = pe - pb;
		rte_memcpy(items, pb, sizeof(struct rte_flow_item) * cpy_count);

		items += cpy_count;

		if (pe->type == RTE_FLOW_ITEM_TYPE_END) {
			pb = pe;
			break;
		}

		pb = pe + 1;
	}
	/* Copy the END item. */
	rte_memcpy(items, pe, sizeof(struct rte_flow_item));
}

/* Check if the pattern matches a supported item type array */
static bool
i40e_match_pattern(enum rte_flow_item_type *item_array,
		   struct rte_flow_item *pattern)
{
	struct rte_flow_item *item = pattern;

	while ((*item_array == item->type) &&
	       (*item_array != RTE_FLOW_ITEM_TYPE_END)) {
		item_array++;
		item++;
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

/* Find if there's parse filter function matched */
static parse_filter_t
i40e_find_parse_filter_func(struct rte_flow_item *pattern)
{
	parse_filter_t parse_filter = NULL;
	uint8_t i = 0;

	for (; i < RTE_DIM(i40e_supported_patterns); i++) {
		if (i40e_match_pattern(i40e_supported_patterns[i].items,
					pattern)) {
			parse_filter = i40e_supported_patterns[i].parse_filter;
			break;
		}
	}

	return parse_filter;
}

/* Parse attributes */
static int
i40e_flow_parse_attr(const struct rte_flow_attr *attr,
		     struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->egress) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->priority) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Not support priority.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->group) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   attr, "Not support group.");
		return -rte_errno;
	}

	return 0;
}

static uint16_t
i40e_get_outer_vlan(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int qinq = dev->data->dev_conf.rxmode.hw_vlan_extend;
	uint64_t reg_r = 0;
	uint16_t reg_id;
	uint16_t tpid;

	if (qinq)
		reg_id = 2;
	else
		reg_id = 3;

	i40e_aq_debug_read_register(hw, I40E_GL_SWT_L2TAGCTRL(reg_id),
				    &reg_r, NULL);

	tpid = (reg_r >> I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_SHIFT) & 0xFFFF;

	return tpid;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: MAC_ETHTYPE and ETHTYPE.
 * 3. SRC mac_addr mask should be 00:00:00:00:00:00.
 * 4. DST mac_addr mask should be 00:00:00:00:00:00 or
 *    FF:FF:FF:FF:FF:FF
 * 5. Ether_type mask should be 0xFFFF.
 */
static int
i40e_flow_parse_ethertype_pattern(struct rte_eth_dev *dev,
				  const struct rte_flow_item *pattern,
				  struct rte_flow_error *error,
				  struct rte_eth_ethertype_filter *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	enum rte_flow_item_type item_type;
	uint16_t outer_tpid;

	outer_tpid = i40e_get_outer_vlan(dev);

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = (const struct rte_flow_item_eth *)item->spec;
			eth_mask = (const struct rte_flow_item_eth *)item->mask;
			/* Get the MAC info. */
			if (!eth_spec || !eth_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL ETH spec/mask");
				return -rte_errno;
			}

			/* Mask bits of source MAC address must be full of 0.
			 * Mask bits of destination MAC address must be full
			 * of 1 or full of 0.
			 */
			if (!is_zero_ether_addr(&eth_mask->src) ||
			    (!is_zero_ether_addr(&eth_mask->dst) &&
			     !is_broadcast_ether_addr(&eth_mask->dst))) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid MAC_addr mask");
				return -rte_errno;
			}

			if ((eth_mask->type & UINT16_MAX) != UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ethertype mask");
				return -rte_errno;
			}

			/* If mask bits of destination MAC address
			 * are full of 1, set RTE_ETHTYPE_FLAGS_MAC.
			 */
			if (is_broadcast_ether_addr(&eth_mask->dst)) {
				filter->mac_addr = eth_spec->dst;
				filter->flags |= RTE_ETHTYPE_FLAGS_MAC;
			} else {
				filter->flags &= ~RTE_ETHTYPE_FLAGS_MAC;
			}
			filter->ether_type = rte_be_to_cpu_16(eth_spec->type);

			if (filter->ether_type == ETHER_TYPE_IPv4 ||
			    filter->ether_type == ETHER_TYPE_IPv6 ||
			    filter->ether_type == outer_tpid) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Unsupported ether_type in"
						   " control packet filter.");
				return -rte_errno;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

/* Ethertype action only supports QUEUE or DROP. */
static int
i40e_flow_parse_ethertype_action(struct rte_eth_dev *dev,
				 const struct rte_flow_action *actions,
				 struct rte_flow_error *error,
				 struct rte_eth_ethertype_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	uint32_t index = 0;

	/* Check if the first non-void action is QUEUE or DROP. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE &&
	    act->type != RTE_FLOW_ACTION_TYPE_DROP) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		act_q = (const struct rte_flow_action_queue *)act->conf;
		filter->queue = act_q->index;
		if (filter->queue >= pf->dev_data->nb_rx_queues) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act, "Invalid queue ID for"
					   " ethertype_filter.");
			return -rte_errno;
		}
	} else {
		filter->flags |= RTE_ETHTYPE_FLAGS_DROP;
	}

	/* Check if the next non-void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

static int
i40e_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
				 const struct rte_flow_attr *attr,
				 const struct rte_flow_item pattern[],
				 const struct rte_flow_action actions[],
				 struct rte_flow_error *error,
				 union i40e_filter_t *filter)
{
	struct rte_eth_ethertype_filter *ethertype_filter =
		&filter->ethertype_filter;
	int ret;

	ret = i40e_flow_parse_ethertype_pattern(dev, pattern, error,
						ethertype_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_ethertype_action(dev, actions, error,
					       ethertype_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_ETHERTYPE;

	return ret;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported flow type and input set: refer to array
 *    default_inset_table in i40e_ethdev.c.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_fdir_pattern(struct rte_eth_dev *dev,
			     const struct rte_flow_item *pattern,
			     struct rte_flow_error *error,
			     struct rte_eth_fdir_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_vf *vf_spec;
	uint32_t flow_type = RTE_ETH_FLOW_UNKNOWN;
	enum i40e_filter_pctype pctype;
	uint64_t input_set = I40E_INSET_NONE;
	uint16_t flag_offset;
	enum rte_flow_item_type item_type;
	enum rte_flow_item_type l3 = RTE_FLOW_ITEM_TYPE_END;
	uint32_t j;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = (const struct rte_flow_item_eth *)item->spec;
			eth_mask = (const struct rte_flow_item_eth *)item->mask;
			if (eth_spec || eth_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ETH spec/mask");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec =
				(const struct rte_flow_item_ipv4 *)item->spec;
			ipv4_mask =
				(const struct rte_flow_item_ipv4 *)item->mask;
			if (!ipv4_spec || !ipv4_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL IPv4 spec/mask");
				return -rte_errno;
			}

			/* Check IPv4 mask and update input set */
			if (ipv4_mask->hdr.version_ihl ||
			    ipv4_mask->hdr.total_length ||
			    ipv4_mask->hdr.packet_id ||
			    ipv4_mask->hdr.fragment_offset ||
			    ipv4_mask->hdr.hdr_checksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
				return -rte_errno;
			}

			if (ipv4_mask->hdr.src_addr == UINT32_MAX)
				input_set |= I40E_INSET_IPV4_SRC;
			if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
				input_set |= I40E_INSET_IPV4_DST;
			if (ipv4_mask->hdr.type_of_service == UINT8_MAX)
				input_set |= I40E_INSET_IPV4_TOS;
			if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
				input_set |= I40E_INSET_IPV4_TTL;
			if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
				input_set |= I40E_INSET_IPV4_PROTO;

			/* Get filter info */
			flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER;
			/* Check if it is fragment. */
			flag_offset =
			      rte_be_to_cpu_16(ipv4_spec->hdr.fragment_offset);
			if (flag_offset & IPV4_HDR_OFFSET_MASK ||
			    flag_offset & IPV4_HDR_MF_FLAG)
				flow_type = RTE_ETH_FLOW_FRAG_IPV4;

			/* Get the filter info */
			filter->input.flow.ip4_flow.proto =
				ipv4_spec->hdr.next_proto_id;
			filter->input.flow.ip4_flow.tos =
				ipv4_spec->hdr.type_of_service;
			filter->input.flow.ip4_flow.ttl =
				ipv4_spec->hdr.time_to_live;
			filter->input.flow.ip4_flow.src_ip =
				ipv4_spec->hdr.src_addr;
			filter->input.flow.ip4_flow.dst_ip =
				ipv4_spec->hdr.dst_addr;

			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			l3 = RTE_FLOW_ITEM_TYPE_IPV6;
			ipv6_spec =
				(const struct rte_flow_item_ipv6 *)item->spec;
			ipv6_mask =
				(const struct rte_flow_item_ipv6 *)item->mask;
			if (!ipv6_spec || !ipv6_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL IPv6 spec/mask");
				return -rte_errno;
			}

			/* Check IPv6 mask and update input set */
			if (ipv6_mask->hdr.payload_len) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask");
				return -rte_errno;
			}

			/* SCR and DST address of IPv6 shouldn't be masked */
			for (j = 0; j < RTE_DIM(ipv6_mask->hdr.src_addr); j++) {
				if (ipv6_mask->hdr.src_addr[j] != UINT8_MAX ||
				    ipv6_mask->hdr.dst_addr[j] != UINT8_MAX) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask");
					return -rte_errno;
				}
			}

			input_set |= I40E_INSET_IPV6_SRC;
			input_set |= I40E_INSET_IPV6_DST;

			if ((ipv6_mask->hdr.vtc_flow &
			     rte_cpu_to_be_16(I40E_IPV6_TC_MASK))
			    == rte_cpu_to_be_16(I40E_IPV6_TC_MASK))
				input_set |= I40E_INSET_IPV6_TC;
			if (ipv6_mask->hdr.proto == UINT8_MAX)
				input_set |= I40E_INSET_IPV6_NEXT_HDR;
			if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
				input_set |= I40E_INSET_IPV6_HOP_LIMIT;

			/* Get filter info */
			filter->input.flow.ipv6_flow.tc =
				(uint8_t)(ipv6_spec->hdr.vtc_flow <<
					  I40E_IPV4_TC_SHIFT);
			filter->input.flow.ipv6_flow.proto =
				ipv6_spec->hdr.proto;
			filter->input.flow.ipv6_flow.hop_limits =
				ipv6_spec->hdr.hop_limits;

			rte_memcpy(filter->input.flow.ipv6_flow.src_ip,
				   ipv6_spec->hdr.src_addr, 16);
			rte_memcpy(filter->input.flow.ipv6_flow.dst_ip,
				   ipv6_spec->hdr.dst_addr, 16);

			/* Check if it is fragment. */
			if (ipv6_spec->hdr.proto == I40E_IPV6_FRAG_HEADER)
				flow_type = RTE_ETH_FLOW_FRAG_IPV6;
			else
				flow_type = RTE_ETH_FLOW_NONFRAG_IPV6_OTHER;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = (const struct rte_flow_item_tcp *)item->spec;
			tcp_mask = (const struct rte_flow_item_tcp *)item->mask;
			if (!tcp_spec || !tcp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL TCP spec/mask");
				return -rte_errno;
			}

			/* Check TCP mask and update input set */
			if (tcp_mask->hdr.sent_seq ||
			    tcp_mask->hdr.recv_ack ||
			    tcp_mask->hdr.data_off ||
			    tcp_mask->hdr.tcp_flags ||
			    tcp_mask->hdr.rx_win ||
			    tcp_mask->hdr.cksum ||
			    tcp_mask->hdr.tcp_urp) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
				return -rte_errno;
			}

			if (tcp_mask->hdr.src_port != UINT16_MAX ||
			    tcp_mask->hdr.dst_port != UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
				return -rte_errno;
			}

			input_set |= I40E_INSET_SRC_PORT;
			input_set |= I40E_INSET_DST_PORT;

			/* Get filter info */
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_TCP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = RTE_ETH_FLOW_NONFRAG_IPV6_TCP;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				filter->input.flow.tcp4_flow.src_port =
					tcp_spec->hdr.src_port;
				filter->input.flow.tcp4_flow.dst_port =
					tcp_spec->hdr.dst_port;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				filter->input.flow.tcp6_flow.src_port =
					tcp_spec->hdr.src_port;
				filter->input.flow.tcp6_flow.dst_port =
					tcp_spec->hdr.dst_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = (const struct rte_flow_item_udp *)item->spec;
			udp_mask = (const struct rte_flow_item_udp *)item->mask;
			if (!udp_spec || !udp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL UDP spec/mask");
				return -rte_errno;
			}

			/* Check UDP mask and update input set*/
			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}

			if (udp_mask->hdr.src_port != UINT16_MAX ||
			    udp_mask->hdr.dst_port != UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}

			input_set |= I40E_INSET_SRC_PORT;
			input_set |= I40E_INSET_DST_PORT;

			/* Get filter info */
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type =
					RTE_ETH_FLOW_NONFRAG_IPV4_UDP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type =
					RTE_ETH_FLOW_NONFRAG_IPV6_UDP;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				filter->input.flow.udp4_flow.src_port =
					udp_spec->hdr.src_port;
				filter->input.flow.udp4_flow.dst_port =
					udp_spec->hdr.dst_port;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				filter->input.flow.udp6_flow.src_port =
					udp_spec->hdr.src_port;
				filter->input.flow.udp6_flow.dst_port =
					udp_spec->hdr.dst_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec =
				(const struct rte_flow_item_sctp *)item->spec;
			sctp_mask =
				(const struct rte_flow_item_sctp *)item->mask;
			if (!sctp_spec || !sctp_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "NULL SCTP spec/mask");
				return -rte_errno;
			}

			/* Check SCTP mask and update input set */
			if (sctp_mask->hdr.cksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}

			if (sctp_mask->hdr.src_port != UINT16_MAX ||
			    sctp_mask->hdr.dst_port != UINT16_MAX ||
			    sctp_mask->hdr.tag != UINT32_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}
			input_set |= I40E_INSET_SRC_PORT;
			input_set |= I40E_INSET_DST_PORT;
			input_set |= I40E_INSET_SCTP_VT;

			/* Get filter info */
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_SCTP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = RTE_ETH_FLOW_NONFRAG_IPV6_SCTP;

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				filter->input.flow.sctp4_flow.src_port =
					sctp_spec->hdr.src_port;
				filter->input.flow.sctp4_flow.dst_port =
					sctp_spec->hdr.dst_port;
				filter->input.flow.sctp4_flow.verify_tag =
					sctp_spec->hdr.tag;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				filter->input.flow.sctp6_flow.src_port =
					sctp_spec->hdr.src_port;
				filter->input.flow.sctp6_flow.dst_port =
					sctp_spec->hdr.dst_port;
				filter->input.flow.sctp6_flow.verify_tag =
					sctp_spec->hdr.tag;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VF:
			vf_spec = (const struct rte_flow_item_vf *)item->spec;
			filter->input.flow_ext.is_vf = 1;
			filter->input.flow_ext.dst_id = vf_spec->id;
			if (filter->input.flow_ext.is_vf &&
			    filter->input.flow_ext.dst_id >= pf->vf_num) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VF ID for FDIR.");
				return -rte_errno;
			}
			break;
		default:
			break;
		}
	}

	pctype = i40e_flowtype_to_pctype(flow_type);
	if (pctype == 0 || pctype > I40E_FILTER_PCTYPE_L2_PAYLOAD) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Unsupported flow type");
		return -rte_errno;
	}

	if (input_set != i40e_get_default_input_set(pctype)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Invalid input set.");
		return -rte_errno;
	}
	filter->input.flow_type = flow_type;

	return 0;
}

/* Parse to get the action info of a FDIR filter.
 * FDIR action supports QUEUE or (QUEUE + MARK).
 */
static int
i40e_flow_parse_fdir_action(struct rte_eth_dev *dev,
			    const struct rte_flow_action *actions,
			    struct rte_flow_error *error,
			    struct rte_eth_fdir_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_mark *mark_spec;
	uint32_t index = 0;

	/* Check if the first non-void action is QUEUE or DROP. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE &&
	    act->type != RTE_FLOW_ACTION_TYPE_DROP) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid action.");
		return -rte_errno;
	}

	act_q = (const struct rte_flow_action_queue *)act->conf;
	filter->action.flex_off = 0;
	if (act->type == RTE_FLOW_ACTION_TYPE_QUEUE)
		filter->action.behavior = RTE_ETH_FDIR_ACCEPT;
	else
		filter->action.behavior = RTE_ETH_FDIR_REJECT;

	filter->action.report_status = RTE_ETH_FDIR_REPORT_ID;
	filter->action.rx_queue = act_q->index;

	if (filter->action.rx_queue >= pf->dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Invalid queue ID for FDIR.");
		return -rte_errno;
	}

	/* Check if the next non-void item is MARK or END. */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_MARK &&
	    act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid action.");
		return -rte_errno;
	}

	if (act->type == RTE_FLOW_ACTION_TYPE_MARK) {
		mark_spec = (const struct rte_flow_action_mark *)act->conf;
		filter->soft_id = mark_spec->id;

		/* Check if the next non-void item is END */
		index++;
		NEXT_ITEM_OF_ACTION(act, actions, index);
		if (act->type != RTE_FLOW_ACTION_TYPE_END) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act, "Invalid action.");
			return -rte_errno;
		}
	}

	return 0;
}

static int
i40e_flow_parse_fdir_filter(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct rte_flow_error *error,
			    union i40e_filter_t *filter)
{
	struct rte_eth_fdir_filter *fdir_filter =
		&filter->fdir_filter;
	int ret;

	ret = i40e_flow_parse_fdir_pattern(dev, pattern, error, fdir_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_fdir_action(dev, actions, error, fdir_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_FDIR;

	if (dev->data->dev_conf.fdir_conf.mode !=
	    RTE_FDIR_MODE_PERFECT) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Check the mode in fdir_conf.");
		return -rte_errno;
	}

	return 0;
}

/* Parse to get the action info of a tunnle filter
 * Tunnel action only supports QUEUE.
 */
static int
i40e_flow_parse_tunnel_action(struct rte_eth_dev *dev,
			      const struct rte_flow_action *actions,
			      struct rte_flow_error *error,
			      struct rte_eth_tunnel_filter_conf *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action *act;
	const struct rte_flow_action_queue *act_q;
	uint32_t index = 0;

	/* Check if the first non-void action is QUEUE. */
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	act_q = (const struct rte_flow_action_queue *)act->conf;
	filter->queue_id = act_q->index;
	if (filter->queue_id >= pf->dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid queue ID for tunnel filter");
		return -rte_errno;
	}

	/* Check if the next non-void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Not supported action.");
		return -rte_errno;
	}

	return 0;
}

static int
i40e_check_tenant_id_mask(const uint8_t *mask)
{
	uint32_t j;
	int is_masked = 0;

	for (j = 0; j < I40E_TENANT_ARRAY_NUM; j++) {
		if (*(mask + j) == UINT8_MAX) {
			if (j > 0 && (*(mask + j) != *(mask + j - 1)))
				return -EINVAL;
			is_masked = 0;
		} else if (*(mask + j) == 0) {
			if (j > 0 && (*(mask + j) != *(mask + j - 1)))
				return -EINVAL;
			is_masked = 1;
		} else {
			return -EINVAL;
		}
	}

	return is_masked;
}

/* 1. Last in item should be NULL as range is not supported.
 * 2. Supported filter types: IMAC_IVLAN_TENID, IMAC_IVLAN,
 *    IMAC_TENID, OMAC_TENID_IMAC and IMAC.
 * 3. Mask of fields which need to be matched should be
 *    filled with 1.
 * 4. Mask of fields which needn't to be matched should be
 *    filled with 0.
 */
static int
i40e_flow_parse_vxlan_pattern(const struct rte_flow_item *pattern,
			      struct rte_flow_error *error,
			      struct rte_eth_tunnel_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;
	const struct rte_flow_item_eth *o_eth_spec = NULL;
	const struct rte_flow_item_eth *o_eth_mask = NULL;
	const struct rte_flow_item_vxlan *vxlan_spec = NULL;
	const struct rte_flow_item_vxlan *vxlan_mask = NULL;
	const struct rte_flow_item_eth *i_eth_spec = NULL;
	const struct rte_flow_item_eth *i_eth_mask = NULL;
	const struct rte_flow_item_vlan *vlan_spec = NULL;
	const struct rte_flow_item_vlan *vlan_mask = NULL;
	bool is_vni_masked = 0;
	enum rte_flow_item_type item_type;
	bool vxlan_flag = 0;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return -rte_errno;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = (const struct rte_flow_item_eth *)item->spec;
			eth_mask = (const struct rte_flow_item_eth *)item->mask;
			if ((!eth_spec && eth_mask) ||
			    (eth_spec && !eth_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ether spec/mask");
				return -rte_errno;
			}

			if (eth_spec && eth_mask) {
				/* DST address of inner MAC shouldn't be masked.
				 * SRC address of Inner MAC should be masked.
				 */
				if (!is_broadcast_ether_addr(&eth_mask->dst) ||
				    !is_zero_ether_addr(&eth_mask->src) ||
				    eth_mask->type) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ether spec/mask");
					return -rte_errno;
				}

				if (!vxlan_flag)
					rte_memcpy(&filter->outer_mac,
						   &eth_spec->dst,
						   ETHER_ADDR_LEN);
				else
					rte_memcpy(&filter->inner_mac,
						   &eth_spec->dst,
						   ETHER_ADDR_LEN);
			}

			if (!vxlan_flag) {
				o_eth_spec = eth_spec;
				o_eth_mask = eth_mask;
			} else {
				i_eth_spec = eth_spec;
				i_eth_mask = eth_mask;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec =
				(const struct rte_flow_item_vlan *)item->spec;
			vlan_mask =
				(const struct rte_flow_item_vlan *)item->mask;
			if (vxlan_flag) {
				vlan_spec =
				(const struct rte_flow_item_vlan *)item->spec;
				vlan_mask =
				(const struct rte_flow_item_vlan *)item->mask;
				if (!(vlan_spec && vlan_mask)) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid vlan item");
					return -rte_errno;
				}
			} else {
				if (vlan_spec || vlan_mask)
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid vlan item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
		case RTE_FLOW_ITEM_TYPE_IPV6:
		case RTE_FLOW_ITEM_TYPE_UDP:
			/* IPv4/IPv6/UDP are used to describe protocol,
			 * spec amd mask should be NULL.
			 */
			if (item->spec || item->mask) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid IPv4 item");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan_spec =
				(const struct rte_flow_item_vxlan *)item->spec;
			vxlan_mask =
				(const struct rte_flow_item_vxlan *)item->mask;
			/* Check if VXLAN item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, either spec or mask shouldn't be NULL.
			 */
			if ((!vxlan_spec && vxlan_mask) ||
			    (vxlan_spec && !vxlan_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid VXLAN item");
				return -rte_errno;
			}

			/* Check if VNI is masked. */
			if (vxlan_mask) {
				is_vni_masked =
				i40e_check_tenant_id_mask(vxlan_mask->vni);
				if (is_vni_masked < 0) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid VNI mask");
					return -rte_errno;
				}
			}
			vxlan_flag = 1;
			break;
		default:
			break;
		}
	}

	/* Check specification and mask to get the filter type */
	if (vlan_spec && vlan_mask &&
	    (vlan_mask->tci == rte_cpu_to_be_16(I40E_TCI_MASK))) {
		/* If there's inner vlan */
		filter->inner_vlan = rte_be_to_cpu_16(vlan_spec->tci)
			& I40E_TCI_MASK;
		if (vxlan_spec && vxlan_mask && !is_vni_masked) {
			/* If there's vxlan */
			rte_memcpy(&filter->tenant_id, vxlan_spec->vni,
				   RTE_DIM(vxlan_spec->vni));
			if (!o_eth_spec && !o_eth_mask &&
				i_eth_spec && i_eth_mask)
				filter->filter_type =
					RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID;
			else {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   NULL,
						   "Invalid filter type");
				return -rte_errno;
			}
		} else if (!vxlan_spec && !vxlan_mask) {
			/* If there's no vxlan */
			if (!o_eth_spec && !o_eth_mask &&
				i_eth_spec && i_eth_mask)
				filter->filter_type =
					RTE_TUNNEL_FILTER_IMAC_IVLAN;
			else {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   NULL,
						   "Invalid filter type");
				return -rte_errno;
			}
		} else {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL,
					   "Invalid filter type");
			return -rte_errno;
		}
	} else if ((!vlan_spec && !vlan_mask) ||
		   (vlan_spec && vlan_mask && vlan_mask->tci == 0x0)) {
		/* If there's no inner vlan */
		if (vxlan_spec && vxlan_mask && !is_vni_masked) {
			/* If there's vxlan */
			rte_memcpy(&filter->tenant_id, vxlan_spec->vni,
				   RTE_DIM(vxlan_spec->vni));
			if (!o_eth_spec && !o_eth_mask &&
				i_eth_spec && i_eth_mask)
				filter->filter_type =
					RTE_TUNNEL_FILTER_IMAC_TENID;
			else if (o_eth_spec && o_eth_mask &&
				i_eth_spec && i_eth_mask)
				filter->filter_type =
					RTE_TUNNEL_FILTER_OMAC_TENID_IMAC;
		} else if (!vxlan_spec && !vxlan_mask) {
			/* If there's no vxlan */
			if (!o_eth_spec && !o_eth_mask &&
				i_eth_spec && i_eth_mask) {
				filter->filter_type = ETH_TUNNEL_FILTER_IMAC;
			} else {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					   "Invalid filter type");
				return -rte_errno;
			}
		} else {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					   "Invalid filter type");
			return -rte_errno;
		}
	} else {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "Not supported by tunnel filter.");
		return -rte_errno;
	}

	filter->tunnel_type = RTE_TUNNEL_TYPE_VXLAN;

	return 0;
}

static int
i40e_flow_parse_tunnel_pattern(__rte_unused struct rte_eth_dev *dev,
			       const struct rte_flow_item *pattern,
			       struct rte_flow_error *error,
			       struct rte_eth_tunnel_filter_conf *filter)
{
	int ret;

	ret = i40e_flow_parse_vxlan_pattern(pattern, error, filter);

	return ret;
}

static int
i40e_flow_parse_tunnel_filter(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error,
			      union i40e_filter_t *filter)
{
	struct rte_eth_tunnel_filter_conf *tunnel_filter =
		&filter->tunnel_filter;
	int ret;

	ret = i40e_flow_parse_tunnel_pattern(dev, pattern,
					     error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_tunnel_action(dev, actions, error, tunnel_filter);
	if (ret)
		return ret;

	ret = i40e_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	cons_filter_type = RTE_ETH_FILTER_TUNNEL;

	return ret;
}

static int
i40e_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct rte_flow_item *items; /* internal pattern w/o VOID items */
	parse_filter_t parse_filter;
	uint32_t item_num = 0; /* non-void item number of pattern*/
	uint32_t i = 0;
	int ret;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}

	memset(&cons_filter, 0, sizeof(cons_filter));

	/* Get the non-void item number of pattern */
	while ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_END) {
		if ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_VOID)
			item_num++;
		i++;
	}
	item_num++;

	items = rte_zmalloc("i40e_pattern",
			    item_num * sizeof(struct rte_flow_item), 0);
	if (!items) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "No memory for PMD internal items.");
		return -ENOMEM;
	}

	i40e_pattern_skip_void_item(items, pattern);

	/* Find if there's matched parse filter function */
	parse_filter = i40e_find_parse_filter_func(items);
	if (!parse_filter) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   pattern, "Unsupported pattern");
		return -rte_errno;
	}

	ret = parse_filter(dev, attr, items, actions, error, &cons_filter);

	rte_free(items);

	return ret;
}

static struct rte_flow *
i40e_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *flow;
	int ret;

	flow = rte_zmalloc("i40e_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return flow;
	}

	ret = i40e_flow_validate(dev, attr, pattern, actions, error);
	if (ret < 0)
		return NULL;

	switch (cons_filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = i40e_ethertype_filter_set(pf,
					&cons_filter.ethertype_filter, 1);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->ethertype.ethertype_list,
					i40e_ethertype_filter_list);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = i40e_add_del_fdir_filter(dev,
				       &cons_filter.fdir_filter, 1);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->fdir.fdir_list,
					i40e_fdir_filter_list);
		break;
	case RTE_ETH_FILTER_TUNNEL:
		ret = i40e_dev_tunnel_filter_set(pf,
					 &cons_filter.tunnel_filter, 1);
		if (ret)
			goto free_flow;
		flow->rule = TAILQ_LAST(&pf->tunnel.tunnel_list,
					i40e_tunnel_filter_list);
		break;
	default:
		goto free_flow;
	}

	flow->filter_type = cons_filter_type;
	TAILQ_INSERT_TAIL(&pf->flow_list, flow, node);
	return flow;

free_flow:
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");
	rte_free(flow);
	return NULL;
}

static int
i40e_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum rte_filter_type filter_type = flow->filter_type;
	int ret = 0;

	switch (filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = i40e_flow_destroy_ethertype_filter(pf,
			 (struct i40e_ethertype_filter *)flow->rule);
		break;
	case RTE_ETH_FILTER_TUNNEL:
		ret = i40e_flow_destroy_tunnel_filter(pf,
			      (struct i40e_tunnel_filter *)flow->rule);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = i40e_add_del_fdir_filter(dev,
		       &((struct i40e_fdir_filter *)flow->rule)->fdir, 0);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
			    filter_type);
		ret = -EINVAL;
		break;
	}

	if (!ret) {
		TAILQ_REMOVE(&pf->flow_list, flow, node);
		rte_free(flow);
	} else
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");

	return ret;
}

static int
i40e_flow_destroy_ethertype_filter(struct i40e_pf *pf,
				   struct i40e_ethertype_filter *filter)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_ethertype_rule *ethertype_rule = &pf->ethertype;
	struct i40e_ethertype_filter *node;
	struct i40e_control_filter_stats stats;
	uint16_t flags = 0;
	int ret = 0;

	if (!(filter->flags & RTE_ETHTYPE_FLAGS_MAC))
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC;
	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP)
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP;
	flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TO_QUEUE;

	memset(&stats, 0, sizeof(stats));
	ret = i40e_aq_add_rem_control_packet_filter(hw,
				    filter->input.mac_addr.addr_bytes,
				    filter->input.ether_type,
				    flags, pf->main_vsi->seid,
				    filter->queue, 0, &stats, NULL);
	if (ret < 0)
		return ret;

	node = i40e_sw_ethertype_filter_lookup(ethertype_rule, &filter->input);
	if (!node)
		return -EINVAL;

	ret = i40e_sw_ethertype_filter_del(pf, &node->input);

	return ret;
}

static int
i40e_flow_destroy_tunnel_filter(struct i40e_pf *pf,
				struct i40e_tunnel_filter *filter)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct i40e_aqc_add_remove_cloud_filters_element_data cld_filter;
	struct i40e_tunnel_rule *tunnel_rule = &pf->tunnel;
	struct i40e_tunnel_filter *node;
	int ret = 0;

	memset(&cld_filter, 0, sizeof(cld_filter));
	ether_addr_copy((struct ether_addr *)&filter->input.outer_mac,
			(struct ether_addr *)&cld_filter.outer_mac);
	ether_addr_copy((struct ether_addr *)&filter->input.inner_mac,
			(struct ether_addr *)&cld_filter.inner_mac);
	cld_filter.inner_vlan = filter->input.inner_vlan;
	cld_filter.flags = filter->input.flags;
	cld_filter.tenant_id = filter->input.tenant_id;
	cld_filter.queue_number = filter->queue;

	ret = i40e_aq_remove_cloud_filters(hw, vsi->seid,
					   &cld_filter, 1);
	if (ret < 0)
		return ret;

	node = i40e_sw_tunnel_filter_lookup(tunnel_rule, &filter->input);
	if (!node)
		return -EINVAL;

	ret = i40e_sw_tunnel_filter_del(pf, &node->input);

	return ret;
}

static int
i40e_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	ret = i40e_flow_flush_fdir_filter(pf);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush FDIR flows.");
		return -rte_errno;
	}

	ret = i40e_flow_flush_ethertype_filter(pf);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to ethertype flush flows.");
		return -rte_errno;
	}

	ret = i40e_flow_flush_tunnel_filter(pf);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to flush tunnel flows.");
		return -rte_errno;
	}

	return ret;
}

static int
i40e_flow_flush_fdir_filter(struct i40e_pf *pf)
{
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_filter *fdir_filter;
	struct rte_flow *flow;
	void *temp;
	int ret;

	ret = i40e_fdir_flush(dev);
	if (!ret) {
		/* Delete FDIR filters in FDIR list. */
		while ((fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list))) {
			ret = i40e_sw_fdir_filter_del(pf,
						      &fdir_filter->fdir.input);
			if (ret < 0)
				return ret;
		}

		/* Delete FDIR flows in flow list. */
		TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, temp) {
			if (flow->filter_type == RTE_ETH_FILTER_FDIR) {
				TAILQ_REMOVE(&pf->flow_list, flow, node);
				rte_free(flow);
			}
		}
	}

	return ret;
}

/* Flush all ethertype filters */
static int
i40e_flow_flush_ethertype_filter(struct i40e_pf *pf)
{
	struct i40e_ethertype_filter_list
		*ethertype_list = &pf->ethertype.ethertype_list;
	struct i40e_ethertype_filter *filter;
	struct rte_flow *flow;
	void *temp;
	int ret = 0;

	while ((filter = TAILQ_FIRST(ethertype_list))) {
		ret = i40e_flow_destroy_ethertype_filter(pf, filter);
		if (ret)
			return ret;
	}

	/* Delete ethertype flows in flow list. */
	TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, temp) {
		if (flow->filter_type == RTE_ETH_FILTER_ETHERTYPE) {
			TAILQ_REMOVE(&pf->flow_list, flow, node);
			rte_free(flow);
		}
	}

	return ret;
}

/* Flush all tunnel filters */
static int
i40e_flow_flush_tunnel_filter(struct i40e_pf *pf)
{
	struct i40e_tunnel_filter_list
		*tunnel_list = &pf->tunnel.tunnel_list;
	struct i40e_tunnel_filter *filter;
	struct rte_flow *flow;
	void *temp;
	int ret = 0;

	while ((filter = TAILQ_FIRST(tunnel_list))) {
		ret = i40e_flow_destroy_tunnel_filter(pf, filter);
		if (ret)
			return ret;
	}

	/* Delete tunnel flows in flow list. */
	TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, temp) {
		if (flow->filter_type == RTE_ETH_FILTER_TUNNEL) {
			TAILQ_REMOVE(&pf->flow_list, flow, node);
			rte_free(flow);
		}
	}

	return ret;
}
