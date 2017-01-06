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

static int i40e_flow_validate(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
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
static int i40e_flow_parse_attr(const struct rte_flow_attr *attr,
				struct rte_flow_error *error);
static int i40e_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item pattern[],
				    const struct rte_flow_action actions[],
				    struct rte_flow_error *error,
				    union i40e_filter_t *filter);

const struct rte_flow_ops i40e_flow_ops = {
	.validate = i40e_flow_validate,
};

union i40e_filter_t cons_filter;

/* Pattern matched ethertype filter */
static enum rte_flow_item_type pattern_ethertype[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_END,
};

static struct i40e_valid_pattern i40e_supported_patterns[] = {
	/* Ethertype */
	{ pattern_ethertype, i40e_flow_parse_ethertype_filter },
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
