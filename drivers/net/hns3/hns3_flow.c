/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <rte_flow_driver.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_flow.h"

#define NEXT_ITEM_OF_ACTION(act, actions, index) \
	do { \
		(act) = (actions) + (index); \
		while ((act)->type == RTE_FLOW_ACTION_TYPE_VOID) { \
			(index)++; \
			(act) = (actions) + (index); \
		} \
	} while (0)

#define NEXT_ITEM_OF_PATTERN(item, pattern, index) \
	do { \
		(item) = (pattern) + (index); \
		while ((item)->type == RTE_FLOW_ITEM_TYPE_VOID) { \
			(index)++; \
			(item) = (pattern) + (index); \
		} \
	} while (0)

#define HNS3_HASH_HDR_ETH	RTE_BIT64(0)
#define HNS3_HASH_HDR_IPV4	RTE_BIT64(1)
#define HNS3_HASH_HDR_IPV6	RTE_BIT64(2)
#define HNS3_HASH_HDR_TCP	RTE_BIT64(3)
#define HNS3_HASH_HDR_UDP	RTE_BIT64(4)
#define HNS3_HASH_HDR_SCTP	RTE_BIT64(5)

#define HNS3_HASH_VOID_NEXT_ALLOW	BIT_ULL(RTE_FLOW_ITEM_TYPE_ETH)

#define HNS3_HASH_ETH_NEXT_ALLOW	(BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV4) | \
					 BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV6))

#define HNS3_HASH_IP_NEXT_ALLOW		(BIT_ULL(RTE_FLOW_ITEM_TYPE_TCP) | \
					 BIT_ULL(RTE_FLOW_ITEM_TYPE_UDP) | \
					 BIT_ULL(RTE_FLOW_ITEM_TYPE_SCTP))

static const uint64_t hash_pattern_next_allow_items[] = {
	[RTE_FLOW_ITEM_TYPE_VOID] = HNS3_HASH_VOID_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_ETH]  = HNS3_HASH_ETH_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_IPV4] = HNS3_HASH_IP_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_IPV6] = HNS3_HASH_IP_NEXT_ALLOW,
};

static const uint64_t hash_pattern_item_header[] = {
	[RTE_FLOW_ITEM_TYPE_ETH]  = HNS3_HASH_HDR_ETH,
	[RTE_FLOW_ITEM_TYPE_IPV4] = HNS3_HASH_HDR_IPV4,
	[RTE_FLOW_ITEM_TYPE_IPV6] = HNS3_HASH_HDR_IPV6,
	[RTE_FLOW_ITEM_TYPE_TCP]  = HNS3_HASH_HDR_TCP,
	[RTE_FLOW_ITEM_TYPE_UDP]  = HNS3_HASH_HDR_UDP,
	[RTE_FLOW_ITEM_TYPE_SCTP] = HNS3_HASH_HDR_SCTP,
};

#define HNS3_HASH_IPV4		(HNS3_HASH_HDR_ETH | HNS3_HASH_HDR_IPV4)
#define HNS3_HASH_IPV4_TCP	(HNS3_HASH_HDR_ETH | \
				 HNS3_HASH_HDR_IPV4 | \
				 HNS3_HASH_HDR_TCP)
#define HNS3_HASH_IPV4_UDP	(HNS3_HASH_HDR_ETH | \
				 HNS3_HASH_HDR_IPV4 | \
				 HNS3_HASH_HDR_UDP)
#define HNS3_HASH_IPV4_SCTP	(HNS3_HASH_HDR_ETH | \
				 HNS3_HASH_HDR_IPV4 | \
				 HNS3_HASH_HDR_SCTP)
#define HNS3_HASH_IPV6		(HNS3_HASH_HDR_ETH | HNS3_HASH_HDR_IPV6)
#define HNS3_HASH_IPV6_TCP	(HNS3_HASH_HDR_ETH | \
				 HNS3_HASH_HDR_IPV6 | \
				 HNS3_HASH_HDR_TCP)
#define HNS3_HASH_IPV6_UDP	(HNS3_HASH_HDR_ETH | \
				 HNS3_HASH_HDR_IPV6 | \
				 HNS3_HASH_HDR_UDP)
#define HNS3_HASH_IPV6_SCTP	(HNS3_HASH_HDR_ETH | \
				 HNS3_HASH_HDR_IPV6 | \
				 HNS3_HASH_HDR_SCTP)

static const struct hns3_hash_map_info {
	/* flow type specified, zero means action works for all flow types. */
	uint64_t pattern_type;
	uint64_t rss_pctype; /* packet type with prefix RTE_ETH_RSS_xxx */
	uint64_t l3l4_types; /* Supported L3/L4 RSS types for this packet type */
	uint64_t hw_pctype; /* packet type in driver */
	uint64_t tuple_mask; /* full tuples of the hw_pctype */
} hash_map_table[] = {
	/* IPV4 */
	{ HNS3_HASH_IPV4,
	  RTE_ETH_RSS_IPV4, HNS3_RSS_SUPPORT_L3_SRC_DST,
	  HNS3_RSS_PCTYPE_IPV4_NONF, HNS3_RSS_TUPLE_IPV4_NONF_M },
	{ HNS3_HASH_IPV4,
	  RTE_ETH_RSS_NONFRAG_IPV4_OTHER, HNS3_RSS_SUPPORT_L3_SRC_DST,
	  HNS3_RSS_PCTYPE_IPV4_NONF, HNS3_RSS_TUPLE_IPV4_NONF_M },
	{ HNS3_HASH_IPV4,
	  RTE_ETH_RSS_FRAG_IPV4, HNS3_RSS_SUPPORT_L3_SRC_DST,
	  HNS3_RSS_PCTYPE_IPV4_FLAG, HNS3_RSS_TUPLE_IPV4_FLAG_M },
	{ HNS3_HASH_IPV4_TCP,
	  RTE_ETH_RSS_NONFRAG_IPV4_TCP, HNS3_RSS_SUPPORT_L3L4,
	  HNS3_RSS_PCTYPE_IPV4_TCP, HNS3_RSS_TUPLE_IPV4_TCP_M },
	{ HNS3_HASH_IPV4_UDP,
	  RTE_ETH_RSS_NONFRAG_IPV4_UDP, HNS3_RSS_SUPPORT_L3L4,
	  HNS3_RSS_PCTYPE_IPV4_UDP, HNS3_RSS_TUPLE_IPV4_UDP_M },
	{ HNS3_HASH_IPV4_SCTP,
	  RTE_ETH_RSS_NONFRAG_IPV4_SCTP, HNS3_RSS_SUPPORT_L3L4,
	  HNS3_RSS_PCTYPE_IPV4_SCTP, HNS3_RSS_TUPLE_IPV4_SCTP_M },
	/* IPV6 */
	{ HNS3_HASH_IPV6,
	  RTE_ETH_RSS_IPV6, HNS3_RSS_SUPPORT_L3_SRC_DST,
	  HNS3_RSS_PCTYPE_IPV6_NONF, HNS3_RSS_TUPLE_IPV6_NONF_M },
	{ HNS3_HASH_IPV6,
	  RTE_ETH_RSS_NONFRAG_IPV6_OTHER, HNS3_RSS_SUPPORT_L3_SRC_DST,
	  HNS3_RSS_PCTYPE_IPV6_NONF, HNS3_RSS_TUPLE_IPV6_NONF_M },
	{ HNS3_HASH_IPV6,
	  RTE_ETH_RSS_FRAG_IPV6, HNS3_RSS_SUPPORT_L3_SRC_DST,
	  HNS3_RSS_PCTYPE_IPV6_FLAG, HNS3_RSS_TUPLE_IPV6_FLAG_M },
	{ HNS3_HASH_IPV6_TCP,
	  RTE_ETH_RSS_NONFRAG_IPV6_TCP, HNS3_RSS_SUPPORT_L3L4,
	  HNS3_RSS_PCTYPE_IPV6_TCP, HNS3_RSS_TUPLE_IPV6_TCP_M },
	{ HNS3_HASH_IPV6_UDP,
	  RTE_ETH_RSS_NONFRAG_IPV6_UDP, HNS3_RSS_SUPPORT_L3L4,
	  HNS3_RSS_PCTYPE_IPV6_UDP, HNS3_RSS_TUPLE_IPV6_UDP_M },
	{ HNS3_HASH_IPV6_SCTP,
	  RTE_ETH_RSS_NONFRAG_IPV6_SCTP, HNS3_RSS_SUPPORT_L3L4,
	  HNS3_RSS_PCTYPE_IPV6_SCTP, HNS3_RSS_TUPLE_IPV6_SCTP_M },
};

static const uint8_t full_mask[VNI_OR_TNI_LEN] = { 0xFF, 0xFF, 0xFF };
static const uint8_t zero_mask[VNI_OR_TNI_LEN] = { 0x00, 0x00, 0x00 };

/* Special Filter id for non-specific packet flagging. Don't change value */
#define HNS3_MAX_FILTER_ID	0x0FFF

#define ETHER_TYPE_MASK		0xFFFF
#define IPPROTO_MASK		0xFF
#define TUNNEL_TYPE_MASK	0xFFFF

#define HNS3_TUNNEL_TYPE_VXLAN		0x12B5
#define HNS3_TUNNEL_TYPE_VXLAN_GPE	0x12B6
#define HNS3_TUNNEL_TYPE_GENEVE		0x17C1
#define HNS3_TUNNEL_TYPE_NVGRE		0x6558

static enum rte_flow_item_type first_items[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_GENEVE,
	RTE_FLOW_ITEM_TYPE_VXLAN_GPE
};

static enum rte_flow_item_type L2_next_items[] = {
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_IPV6
};

static enum rte_flow_item_type L3_next_items[] = {
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_NVGRE,
	RTE_FLOW_ITEM_TYPE_ICMP
};

static enum rte_flow_item_type L4_next_items[] = {
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_GENEVE,
	RTE_FLOW_ITEM_TYPE_VXLAN_GPE
};

static enum rte_flow_item_type tunnel_next_items[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN
};

struct items_step_mngr {
	enum rte_flow_item_type *items;
	size_t count;
};

static inline void
net_addr_to_host(uint32_t *dst, const rte_be32_t *src, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		dst[i] = rte_be_to_cpu_32(src[i]);
}

/*
 * This function is used to parse filter type.
 * 1. As we know RSS is used to spread packets among several queues, the flow
 *    API provide the struct rte_flow_action_rss, user could config its field
 *    sush as: func/level/types/key/queue to control RSS function.
 * 2. The flow API also supports queue region configuration for hns3. It was
 *    implemented by FDIR + RSS in hns3 hardware, user can create one FDIR rule
 *    which action is RSS queues region.
 * 3. When action is RSS, we use the following rule to distinguish:
 *    Case 1: pattern has ETH and all fields in RSS action except 'queues' are
 *            zero or default, indicate it is queue region configuration.
 *    Case other: an rss general action.
 */
static void
hns3_parse_filter_type(const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct hns3_filter_info *filter_info)
{
	const struct rte_flow_action_rss *rss_act;
	const struct rte_flow_action *act = NULL;
	bool only_has_queues = false;
	bool have_eth = false;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_RSS) {
			act = actions;
			break;
		}
	}
	if (act == NULL) {
		filter_info->type = RTE_ETH_FILTER_FDIR;
		return;
	}

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		if (pattern->type == RTE_FLOW_ITEM_TYPE_ETH) {
			have_eth = true;
			break;
		}
	}

	rss_act = act->conf;
	only_has_queues = (rss_act->queue_num > 0) &&
			  (rss_act->func == RTE_ETH_HASH_FUNCTION_DEFAULT &&
			   rss_act->types == 0 && rss_act->key_len == 0);
	if (have_eth && only_has_queues) {
		/*
		 * Pattern has ETH and all fields in RSS action except 'queues'
		 * are zero or default, which indicates this is queue region
		 * configuration.
		 */
		filter_info->type = RTE_ETH_FILTER_FDIR;
		return;
	}

	filter_info->type = RTE_ETH_FILTER_HASH;
}

static inline struct hns3_flow_counter *
hns3_counter_lookup(struct rte_eth_dev *dev, uint32_t id)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_flow_counter *cnt;

	LIST_FOREACH(cnt, &pf->flow_counters, next) {
		if (cnt->id == id)
			return cnt;
	}
	return NULL;
}

static int
hns3_counter_new(struct rte_eth_dev *dev, uint32_t indirect, uint32_t id,
		 struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_flow_counter *cnt;
	uint64_t value;
	int ret;

	cnt = hns3_counter_lookup(dev, id);
	if (cnt) {
		if (!cnt->indirect || cnt->indirect != indirect)
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				cnt,
				"Counter id is used, indirect flag not match");
		/* Clear the indirect counter on first use. */
		if (cnt->indirect && cnt->ref_cnt == 1)
			(void)hns3_fd_get_count(hw, id, &value);
		cnt->ref_cnt++;
		return 0;
	}

	/* Clear the counter by read ops because the counter is read-clear */
	ret = hns3_fd_get_count(hw, id, &value);
	if (ret)
		return rte_flow_error_set(error, EIO,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "Clear counter failed!");

	cnt = rte_zmalloc("hns3 counter", sizeof(*cnt), 0);
	if (cnt == NULL)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_HANDLE, cnt,
					  "Alloc mem for counter failed");
	cnt->id = id;
	cnt->indirect = indirect;
	cnt->ref_cnt = 1;
	cnt->hits = 0;
	LIST_INSERT_HEAD(&pf->flow_counters, cnt, next);
	return 0;
}

static int
hns3_counter_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		   struct rte_flow_query_count *qc,
		   struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_flow_counter *cnt;
	uint64_t value;
	int ret;

	/* FDIR is available only in PF driver */
	if (hns->is_vf)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "Fdir is not supported in VF");
	cnt = hns3_counter_lookup(dev, flow->counter_id);
	if (cnt == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "Can't find counter id");

	ret = hns3_fd_get_count(&hns->hw, flow->counter_id, &value);
	if (ret) {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Read counter fail.");
		return ret;
	}
	qc->hits_set = 1;
	qc->hits = value;
	qc->bytes_set = 0;
	qc->bytes = 0;

	return 0;
}

static int
hns3_counter_release(struct rte_eth_dev *dev, uint32_t id)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_flow_counter *cnt;

	cnt = hns3_counter_lookup(dev, id);
	if (cnt == NULL) {
		hns3_err(hw, "Can't find available counter to release");
		return -EINVAL;
	}
	cnt->ref_cnt--;
	if (cnt->ref_cnt == 0) {
		LIST_REMOVE(cnt, next);
		rte_free(cnt);
	}
	return 0;
}

static void
hns3_counter_flush(struct rte_eth_dev *dev)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	LIST_HEAD(counters, hns3_flow_counter) indir_counters;
	struct hns3_flow_counter *cnt_ptr;

	LIST_INIT(&indir_counters);
	cnt_ptr = LIST_FIRST(&pf->flow_counters);
	while (cnt_ptr) {
		LIST_REMOVE(cnt_ptr, next);
		if (cnt_ptr->indirect)
			LIST_INSERT_HEAD(&indir_counters, cnt_ptr, next);
		else
			rte_free(cnt_ptr);
		cnt_ptr = LIST_FIRST(&pf->flow_counters);
	}

	/* Reset the indirect action and add to pf->flow_counters list. */
	cnt_ptr = LIST_FIRST(&indir_counters);
	while (cnt_ptr) {
		LIST_REMOVE(cnt_ptr, next);
		cnt_ptr->ref_cnt = 1;
		cnt_ptr->hits = 0;
		LIST_INSERT_HEAD(&pf->flow_counters, cnt_ptr, next);
		cnt_ptr = LIST_FIRST(&indir_counters);
	}
}

static int
hns3_handle_action_queue(struct rte_eth_dev *dev,
			 const struct rte_flow_action *action,
			 struct hns3_fdir_rule *rule,
			 struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	const struct rte_flow_action_queue *queue;
	struct hns3_hw *hw = &hns->hw;

	queue = (const struct rte_flow_action_queue *)action->conf;
	if (queue->index >= hw->data->nb_rx_queues) {
		hns3_err(hw, "queue ID(%u) is greater than number of available queue (%u) in driver.",
			 queue->index, hw->data->nb_rx_queues);
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  action, "Invalid queue ID in PF");
	}

	rule->queue_id = queue->index;
	rule->nb_queues = 1;
	rule->action = HNS3_FD_ACTION_ACCEPT_PACKET;
	return 0;
}

static int
hns3_handle_action_queue_region(struct rte_eth_dev *dev,
				const struct rte_flow_action *action,
				struct hns3_fdir_rule *rule,
				struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	const struct rte_flow_action_rss *conf = action->conf;
	struct hns3_hw *hw = &hns->hw;
	uint16_t idx;

	if (!hns3_dev_get_support(hw, FD_QUEUE_REGION))
		return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION, action,
			"Not support config queue region!");

	if ((!rte_is_power_of_2(conf->queue_num)) ||
		conf->queue_num > hw->rss_size_max ||
		conf->queue[0] >= hw->data->nb_rx_queues ||
		conf->queue[0] + conf->queue_num > hw->data->nb_rx_queues) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION_CONF, action,
			"Invalid start queue ID and queue num! the start queue "
			"ID must valid, the queue num must be power of 2 and "
			"<= rss_size_max.");
	}

	for (idx = 1; idx < conf->queue_num; idx++) {
		if (conf->queue[idx] != conf->queue[idx - 1] + 1)
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, action,
				"Invalid queue ID sequence! the queue ID "
				"must be continuous increment.");
	}

	rule->queue_id = conf->queue[0];
	rule->nb_queues = conf->queue_num;
	rule->action = HNS3_FD_ACTION_ACCEPT_PACKET;
	return 0;
}

static int
hns3_handle_action_indirect(struct rte_eth_dev *dev,
			    const struct rte_flow_action *action,
			    struct hns3_fdir_rule *rule,
			    struct rte_flow_error *error)
{
	const struct rte_flow_action_handle *indir = action->conf;

	if (indir->indirect_type != HNS3_INDIRECT_ACTION_TYPE_COUNT)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				action, "Invalid indirect type");

	if (hns3_counter_lookup(dev, indir->counter_id) == NULL)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				action, "Counter id not exist");

	rule->act_cnt.id = indir->counter_id;
	rule->flags |= (HNS3_RULE_FLAG_COUNTER | HNS3_RULE_FLAG_COUNTER_INDIR);

	return 0;
}

/*
 * Parse actions structure from the provided pattern.
 * The pattern is validated as the items are copied.
 *
 * @param actions[in]
 * @param rule[out]
 *   NIC specific actions derived from the actions.
 * @param error[out]
 */
static int
hns3_handle_actions(struct rte_eth_dev *dev,
		    const struct rte_flow_action actions[],
		    struct hns3_fdir_rule *rule, struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	const struct rte_flow_action_count *act_count;
	const struct rte_flow_action_mark *mark;
	struct hns3_pf *pf = &hns->pf;
	uint32_t counter_num;
	int ret;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = hns3_handle_action_queue(dev, actions, rule,
						       error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			rule->action = HNS3_FD_ACTION_DROP_PACKET;
			break;
		/*
		 * Here RSS's real action is queue region.
		 * Queue region is implemented by FDIR + RSS in hns3 hardware,
		 * the FDIR's action is one queue region (start_queue_id and
		 * queue_num), then RSS spread packets to the queue region by
		 * RSS algorithm.
		 */
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = hns3_handle_action_queue_region(dev, actions,
							      rule, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			mark =
			    (const struct rte_flow_action_mark *)actions->conf;
			if (mark->id >= HNS3_MAX_FILTER_ID)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						actions,
						"Invalid Mark ID");
			rule->fd_id = mark->id;
			rule->flags |= HNS3_RULE_FLAG_FDID;
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			rule->fd_id = HNS3_MAX_FILTER_ID;
			rule->flags |= HNS3_RULE_FLAG_FDID;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			act_count =
			    (const struct rte_flow_action_count *)actions->conf;
			counter_num = pf->fdir.fd_cfg.cnt_num[HNS3_FD_STAGE_1];
			if (act_count->id >= counter_num)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						actions,
						"Invalid counter id");
			rule->act_cnt = *act_count;
			rule->flags |= HNS3_RULE_FLAG_COUNTER;
			rule->flags &= ~HNS3_RULE_FLAG_COUNTER_INDIR;
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			ret = hns3_handle_action_indirect(dev, actions, rule,
							  error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "Unsupported action");
		}
	}

	return 0;
}

static int
hns3_check_attr(const struct rte_flow_attr *attr, struct rte_flow_error *error)
{
	if (!attr->ingress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  attr, "Ingress can't be zero");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					  attr, "Not support egress");
	if (attr->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  attr, "No support for transfer");
	if (attr->priority)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  attr, "Not support priority");
	if (attr->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  attr, "Not support group");
	return 0;
}

static int
hns3_parse_eth(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
	       struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		eth_mask = item->mask;
		if (eth_mask->hdr.ether_type) {
			hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 1);
			rule->key_conf.mask.ether_type =
			    rte_be_to_cpu_16(eth_mask->hdr.ether_type);
		}
		if (!rte_is_zero_ether_addr(&eth_mask->hdr.src_addr)) {
			hns3_set_bit(rule->input_set, INNER_SRC_MAC, 1);
			memcpy(rule->key_conf.mask.src_mac,
			       eth_mask->hdr.src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
		}
		if (!rte_is_zero_ether_addr(&eth_mask->hdr.dst_addr)) {
			hns3_set_bit(rule->input_set, INNER_DST_MAC, 1);
			memcpy(rule->key_conf.mask.dst_mac,
			       eth_mask->hdr.dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
		}
	}

	eth_spec = item->spec;
	rule->key_conf.spec.ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);
	memcpy(rule->key_conf.spec.src_mac, eth_spec->hdr.src_addr.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	memcpy(rule->key_conf.spec.dst_mac, eth_spec->hdr.dst_addr.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	return 0;
}

static int
hns3_parse_vlan(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_spec;
	const struct rte_flow_item_vlan *vlan_mask;

	rule->key_conf.vlan_num++;
	if (rule->key_conf.vlan_num > VLAN_TAG_NUM_MAX)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Vlan_num is more than 2");

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		vlan_mask = item->mask;
		if (vlan_mask->hdr.vlan_tci) {
			if (rule->key_conf.vlan_num == 1) {
				hns3_set_bit(rule->input_set, INNER_VLAN_TAG1,
					     1);
				rule->key_conf.mask.vlan_tag1 =
				    rte_be_to_cpu_16(vlan_mask->hdr.vlan_tci);
			} else {
				hns3_set_bit(rule->input_set, INNER_VLAN_TAG2,
					     1);
				rule->key_conf.mask.vlan_tag2 =
				    rte_be_to_cpu_16(vlan_mask->hdr.vlan_tci);
			}
		}
	}

	vlan_spec = item->spec;
	if (rule->key_conf.vlan_num == 1)
		rule->key_conf.spec.vlan_tag1 =
		    rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci);
	else
		rule->key_conf.spec.vlan_tag2 =
		    rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci);
	return 0;
}

static bool
hns3_check_ipv4_mask_supported(const struct rte_flow_item_ipv4 *ipv4_mask)
{
	if (ipv4_mask->hdr.total_length || ipv4_mask->hdr.packet_id ||
	    ipv4_mask->hdr.fragment_offset || ipv4_mask->hdr.time_to_live ||
	    ipv4_mask->hdr.hdr_checksum)
		return false;

	return true;
}

static int
hns3_parse_ipv4(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;

	hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 1);
	rule->key_conf.spec.ether_type = RTE_ETHER_TYPE_IPV4;
	rule->key_conf.mask.ether_type = ETHER_TYPE_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		ipv4_mask = item->mask;
		if (!hns3_check_ipv4_mask_supported(ipv4_mask)) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
						  item,
						  "Only support src & dst ip,tos,proto in IPV4");
		}

		if (ipv4_mask->hdr.src_addr) {
			hns3_set_bit(rule->input_set, INNER_SRC_IP, 1);
			rule->key_conf.mask.src_ip[IP_ADDR_KEY_ID] =
			    rte_be_to_cpu_32(ipv4_mask->hdr.src_addr);
		}

		if (ipv4_mask->hdr.dst_addr) {
			hns3_set_bit(rule->input_set, INNER_DST_IP, 1);
			rule->key_conf.mask.dst_ip[IP_ADDR_KEY_ID] =
			    rte_be_to_cpu_32(ipv4_mask->hdr.dst_addr);
		}

		if (ipv4_mask->hdr.type_of_service) {
			hns3_set_bit(rule->input_set, INNER_IP_TOS, 1);
			rule->key_conf.mask.ip_tos =
			    ipv4_mask->hdr.type_of_service;
		}

		if (ipv4_mask->hdr.next_proto_id) {
			hns3_set_bit(rule->input_set, INNER_IP_PROTO, 1);
			rule->key_conf.mask.ip_proto =
			    ipv4_mask->hdr.next_proto_id;
		}
	}

	ipv4_spec = item->spec;
	rule->key_conf.spec.src_ip[IP_ADDR_KEY_ID] =
	    rte_be_to_cpu_32(ipv4_spec->hdr.src_addr);
	rule->key_conf.spec.dst_ip[IP_ADDR_KEY_ID] =
	    rte_be_to_cpu_32(ipv4_spec->hdr.dst_addr);
	rule->key_conf.spec.ip_tos = ipv4_spec->hdr.type_of_service;
	rule->key_conf.spec.ip_proto = ipv4_spec->hdr.next_proto_id;
	return 0;
}

static int
hns3_parse_ipv6(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *ipv6_spec;
	const struct rte_flow_item_ipv6 *ipv6_mask;

	hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 1);
	rule->key_conf.spec.ether_type = RTE_ETHER_TYPE_IPV6;
	rule->key_conf.mask.ether_type = ETHER_TYPE_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		ipv6_mask = item->mask;
		if (ipv6_mask->hdr.vtc_flow || ipv6_mask->hdr.payload_len ||
		    ipv6_mask->hdr.hop_limits) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
						  item,
						  "Only support src & dst ip,proto in IPV6");
		}
		net_addr_to_host(rule->key_conf.mask.src_ip,
				 (const rte_be32_t *)ipv6_mask->hdr.src_addr,
				 IP_ADDR_LEN);
		net_addr_to_host(rule->key_conf.mask.dst_ip,
				 (const rte_be32_t *)ipv6_mask->hdr.dst_addr,
				 IP_ADDR_LEN);
		rule->key_conf.mask.ip_proto = ipv6_mask->hdr.proto;
		if (rule->key_conf.mask.src_ip[IP_ADDR_KEY_ID])
			hns3_set_bit(rule->input_set, INNER_SRC_IP, 1);
		if (rule->key_conf.mask.dst_ip[IP_ADDR_KEY_ID])
			hns3_set_bit(rule->input_set, INNER_DST_IP, 1);
		if (ipv6_mask->hdr.proto)
			hns3_set_bit(rule->input_set, INNER_IP_PROTO, 1);
	}

	ipv6_spec = item->spec;
	net_addr_to_host(rule->key_conf.spec.src_ip,
			 (const rte_be32_t *)ipv6_spec->hdr.src_addr,
			 IP_ADDR_LEN);
	net_addr_to_host(rule->key_conf.spec.dst_ip,
			 (const rte_be32_t *)ipv6_spec->hdr.dst_addr,
			 IP_ADDR_LEN);
	rule->key_conf.spec.ip_proto = ipv6_spec->hdr.proto;

	return 0;
}

static bool
hns3_check_tcp_mask_supported(const struct rte_flow_item_tcp *tcp_mask)
{
	if (tcp_mask->hdr.sent_seq || tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off || tcp_mask->hdr.tcp_flags ||
	    tcp_mask->hdr.rx_win || tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp)
		return false;

	return true;
}

static int
hns3_parse_tcp(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;

	hns3_set_bit(rule->input_set, INNER_IP_PROTO, 1);
	rule->key_conf.spec.ip_proto = IPPROTO_TCP;
	rule->key_conf.mask.ip_proto = IPPROTO_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		tcp_mask = item->mask;
		if (!hns3_check_tcp_mask_supported(tcp_mask)) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
						  item,
						  "Only support src & dst port in TCP");
		}

		if (tcp_mask->hdr.src_port) {
			hns3_set_bit(rule->input_set, INNER_SRC_PORT, 1);
			rule->key_conf.mask.src_port =
			    rte_be_to_cpu_16(tcp_mask->hdr.src_port);
		}
		if (tcp_mask->hdr.dst_port) {
			hns3_set_bit(rule->input_set, INNER_DST_PORT, 1);
			rule->key_conf.mask.dst_port =
			    rte_be_to_cpu_16(tcp_mask->hdr.dst_port);
		}
	}

	tcp_spec = item->spec;
	rule->key_conf.spec.src_port = rte_be_to_cpu_16(tcp_spec->hdr.src_port);
	rule->key_conf.spec.dst_port = rte_be_to_cpu_16(tcp_spec->hdr.dst_port);

	return 0;
}

static int
hns3_parse_udp(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *udp_spec;
	const struct rte_flow_item_udp *udp_mask;

	hns3_set_bit(rule->input_set, INNER_IP_PROTO, 1);
	rule->key_conf.spec.ip_proto = IPPROTO_UDP;
	rule->key_conf.mask.ip_proto = IPPROTO_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		udp_mask = item->mask;
		if (udp_mask->hdr.dgram_len || udp_mask->hdr.dgram_cksum) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
						  item,
						  "Only support src & dst port in UDP");
		}
		if (udp_mask->hdr.src_port) {
			hns3_set_bit(rule->input_set, INNER_SRC_PORT, 1);
			rule->key_conf.mask.src_port =
			    rte_be_to_cpu_16(udp_mask->hdr.src_port);
		}
		if (udp_mask->hdr.dst_port) {
			hns3_set_bit(rule->input_set, INNER_DST_PORT, 1);
			rule->key_conf.mask.dst_port =
			    rte_be_to_cpu_16(udp_mask->hdr.dst_port);
		}
	}

	udp_spec = item->spec;
	rule->key_conf.spec.src_port = rte_be_to_cpu_16(udp_spec->hdr.src_port);
	rule->key_conf.spec.dst_port = rte_be_to_cpu_16(udp_spec->hdr.dst_port);

	return 0;
}

static int
hns3_parse_sctp(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_sctp *sctp_spec;
	const struct rte_flow_item_sctp *sctp_mask;

	hns3_set_bit(rule->input_set, INNER_IP_PROTO, 1);
	rule->key_conf.spec.ip_proto = IPPROTO_SCTP;
	rule->key_conf.mask.ip_proto = IPPROTO_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		sctp_mask = item->mask;
		if (sctp_mask->hdr.cksum)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
						  item,
						  "Only support src & dst port in SCTP");
		if (sctp_mask->hdr.src_port) {
			hns3_set_bit(rule->input_set, INNER_SRC_PORT, 1);
			rule->key_conf.mask.src_port =
			    rte_be_to_cpu_16(sctp_mask->hdr.src_port);
		}
		if (sctp_mask->hdr.dst_port) {
			hns3_set_bit(rule->input_set, INNER_DST_PORT, 1);
			rule->key_conf.mask.dst_port =
			    rte_be_to_cpu_16(sctp_mask->hdr.dst_port);
		}
		if (sctp_mask->hdr.tag) {
			hns3_set_bit(rule->input_set, INNER_SCTP_TAG, 1);
			rule->key_conf.mask.sctp_tag =
			    rte_be_to_cpu_32(sctp_mask->hdr.tag);
		}
	}

	sctp_spec = item->spec;
	rule->key_conf.spec.src_port =
	    rte_be_to_cpu_16(sctp_spec->hdr.src_port);
	rule->key_conf.spec.dst_port =
	    rte_be_to_cpu_16(sctp_spec->hdr.dst_port);
	rule->key_conf.spec.sctp_tag = rte_be_to_cpu_32(sctp_spec->hdr.tag);

	return 0;
}

/*
 * Check items before tunnel, save inner configs to outer configs, and clear
 * inner configs.
 * The key consists of two parts: meta_data and tuple keys.
 * Meta data uses 15 bits, including vlan_num(2bit), des_port(12bit) and tunnel
 * packet(1bit).
 * Tuple keys uses 384bit, including ot_dst-mac(48bit), ot_dst-port(16bit),
 * ot_tun_vni(24bit), ot_flow_id(8bit), src-mac(48bit), dst-mac(48bit),
 * src-ip(32/128bit), dst-ip(32/128bit), src-port(16bit), dst-port(16bit),
 * tos(8bit), ether-proto(16bit), ip-proto(8bit), vlantag1(16bit),
 * Vlantag2(16bit) and sctp-tag(32bit).
 */
static int
hns3_handle_tunnel(const struct rte_flow_item *item,
		   struct hns3_fdir_rule *rule, struct rte_flow_error *error)
{
	/* check eth config */
	if (rule->input_set & (BIT(INNER_SRC_MAC) | BIT(INNER_DST_MAC)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item, "Outer eth mac is unsupported");
	if (rule->input_set & BIT(INNER_ETH_TYPE)) {
		hns3_set_bit(rule->input_set, OUTER_ETH_TYPE, 1);
		rule->key_conf.spec.outer_ether_type =
		    rule->key_conf.spec.ether_type;
		rule->key_conf.mask.outer_ether_type =
		    rule->key_conf.mask.ether_type;
		hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 0);
		rule->key_conf.spec.ether_type = 0;
		rule->key_conf.mask.ether_type = 0;
	}

	/* check vlan config */
	if (rule->input_set & (BIT(INNER_VLAN_TAG1) | BIT(INNER_VLAN_TAG2)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "Outer vlan tags is unsupported");

	/* clear vlan_num for inner vlan select */
	rule->key_conf.outer_vlan_num = rule->key_conf.vlan_num;
	rule->key_conf.vlan_num = 0;

	/* check L3 config */
	if (rule->input_set &
	    (BIT(INNER_SRC_IP) | BIT(INNER_DST_IP) | BIT(INNER_IP_TOS)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item, "Outer ip is unsupported");
	if (rule->input_set & BIT(INNER_IP_PROTO)) {
		hns3_set_bit(rule->input_set, OUTER_IP_PROTO, 1);
		rule->key_conf.spec.outer_proto = rule->key_conf.spec.ip_proto;
		rule->key_conf.mask.outer_proto = rule->key_conf.mask.ip_proto;
		hns3_set_bit(rule->input_set, INNER_IP_PROTO, 0);
		rule->key_conf.spec.ip_proto = 0;
		rule->key_conf.mask.ip_proto = 0;
	}

	/* check L4 config */
	if (rule->input_set & BIT(INNER_SCTP_TAG))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Outer sctp tag is unsupported");

	if (rule->input_set & BIT(INNER_SRC_PORT)) {
		hns3_set_bit(rule->input_set, OUTER_SRC_PORT, 1);
		rule->key_conf.spec.outer_src_port =
		    rule->key_conf.spec.src_port;
		rule->key_conf.mask.outer_src_port =
		    rule->key_conf.mask.src_port;
		hns3_set_bit(rule->input_set, INNER_SRC_PORT, 0);
		rule->key_conf.spec.src_port = 0;
		rule->key_conf.mask.src_port = 0;
	}
	if (rule->input_set & BIT(INNER_DST_PORT)) {
		hns3_set_bit(rule->input_set, INNER_DST_PORT, 0);
		rule->key_conf.spec.dst_port = 0;
		rule->key_conf.mask.dst_port = 0;
	}
	return 0;
}

static int
hns3_parse_vxlan(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		 struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *vxlan_spec;
	const struct rte_flow_item_vxlan *vxlan_mask;

	hns3_set_bit(rule->input_set, OUTER_DST_PORT, 1);
	rule->key_conf.mask.tunnel_type = TUNNEL_TYPE_MASK;
	if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN)
		rule->key_conf.spec.tunnel_type = HNS3_TUNNEL_TYPE_VXLAN;
	else
		rule->key_conf.spec.tunnel_type = HNS3_TUNNEL_TYPE_VXLAN_GPE;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	vxlan_mask = item->mask;
	vxlan_spec = item->spec;

	if (vxlan_mask->hdr.flags)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					  "Flags is not supported in VxLAN");

	/* VNI must be totally masked or not. */
	if (memcmp(vxlan_mask->hdr.vni, full_mask, VNI_OR_TNI_LEN) &&
	    memcmp(vxlan_mask->hdr.vni, zero_mask, VNI_OR_TNI_LEN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					  "VNI must be totally masked or not in VxLAN");
	if (vxlan_mask->hdr.vni[0]) {
		hns3_set_bit(rule->input_set, OUTER_TUN_VNI, 1);
		memcpy(rule->key_conf.mask.outer_tun_vni, vxlan_mask->hdr.vni,
			   VNI_OR_TNI_LEN);
	}
	memcpy(rule->key_conf.spec.outer_tun_vni, vxlan_spec->hdr.vni,
		   VNI_OR_TNI_LEN);
	return 0;
}

static int
hns3_parse_nvgre(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		 struct rte_flow_error *error)
{
	const struct rte_flow_item_nvgre *nvgre_spec;
	const struct rte_flow_item_nvgre *nvgre_mask;

	hns3_set_bit(rule->input_set, OUTER_IP_PROTO, 1);
	rule->key_conf.spec.outer_proto = IPPROTO_GRE;
	rule->key_conf.mask.outer_proto = IPPROTO_MASK;

	hns3_set_bit(rule->input_set, OUTER_DST_PORT, 1);
	rule->key_conf.spec.tunnel_type = HNS3_TUNNEL_TYPE_NVGRE;
	rule->key_conf.mask.tunnel_type = ~HNS3_TUNNEL_TYPE_NVGRE;
	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	nvgre_mask = item->mask;
	nvgre_spec = item->spec;

	if (nvgre_mask->protocol || nvgre_mask->c_k_s_rsvd0_ver)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					  "Ver/protocol is not supported in NVGRE");

	/* TNI must be totally masked or not. */
	if (memcmp(nvgre_mask->tni, full_mask, VNI_OR_TNI_LEN) &&
	    memcmp(nvgre_mask->tni, zero_mask, VNI_OR_TNI_LEN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					  "TNI must be totally masked or not in NVGRE");

	if (nvgre_mask->tni[0]) {
		hns3_set_bit(rule->input_set, OUTER_TUN_VNI, 1);
		memcpy(rule->key_conf.mask.outer_tun_vni, nvgre_mask->tni,
			   VNI_OR_TNI_LEN);
	}
	memcpy(rule->key_conf.spec.outer_tun_vni, nvgre_spec->tni,
		   VNI_OR_TNI_LEN);

	if (nvgre_mask->flow_id) {
		hns3_set_bit(rule->input_set, OUTER_TUN_FLOW_ID, 1);
		rule->key_conf.mask.outer_tun_flow_id = nvgre_mask->flow_id;
	}
	rule->key_conf.spec.outer_tun_flow_id = nvgre_spec->flow_id;
	return 0;
}

static int
hns3_parse_geneve(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		  struct rte_flow_error *error)
{
	const struct rte_flow_item_geneve *geneve_spec;
	const struct rte_flow_item_geneve *geneve_mask;

	hns3_set_bit(rule->input_set, OUTER_DST_PORT, 1);
	rule->key_conf.spec.tunnel_type = HNS3_TUNNEL_TYPE_GENEVE;
	rule->key_conf.mask.tunnel_type = TUNNEL_TYPE_MASK;
	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	geneve_mask = item->mask;
	geneve_spec = item->spec;

	if (geneve_mask->ver_opt_len_o_c_rsvd0 || geneve_mask->protocol)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					  "Ver/protocol is not supported in GENEVE");
	/* VNI must be totally masked or not. */
	if (memcmp(geneve_mask->vni, full_mask, VNI_OR_TNI_LEN) &&
	    memcmp(geneve_mask->vni, zero_mask, VNI_OR_TNI_LEN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					  "VNI must be totally masked or not in GENEVE");
	if (geneve_mask->vni[0]) {
		hns3_set_bit(rule->input_set, OUTER_TUN_VNI, 1);
		memcpy(rule->key_conf.mask.outer_tun_vni, geneve_mask->vni,
			   VNI_OR_TNI_LEN);
	}
	memcpy(rule->key_conf.spec.outer_tun_vni, geneve_spec->vni,
		   VNI_OR_TNI_LEN);
	return 0;
}

static int
hns3_parse_tunnel(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		  struct rte_flow_error *error)
{
	int ret;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask "
					  "but without spec");
	else if (item->spec && (item->mask == NULL))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Tunnel packets must configure "
					  "with mask");

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_VXLAN:
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		ret = hns3_parse_vxlan(item, rule, error);
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		ret = hns3_parse_nvgre(item, rule, error);
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		ret = hns3_parse_geneve(item, rule, error);
		break;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "Unsupported tunnel type!");
	}
	if (ret)
		return ret;
	return hns3_handle_tunnel(item, rule, error);
}

static int
hns3_parse_normal(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		  struct items_step_mngr *step_mngr,
		  struct rte_flow_error *error)
{
	int ret;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask "
					  "but without spec");

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		ret = hns3_parse_eth(item, rule, error);
		step_mngr->items = L2_next_items;
		step_mngr->count = RTE_DIM(L2_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		ret = hns3_parse_vlan(item, rule, error);
		step_mngr->items = L2_next_items;
		step_mngr->count = RTE_DIM(L2_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		ret = hns3_parse_ipv4(item, rule, error);
		step_mngr->items = L3_next_items;
		step_mngr->count = RTE_DIM(L3_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		ret = hns3_parse_ipv6(item, rule, error);
		step_mngr->items = L3_next_items;
		step_mngr->count = RTE_DIM(L3_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		ret = hns3_parse_tcp(item, rule, error);
		step_mngr->items = L4_next_items;
		step_mngr->count = RTE_DIM(L4_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		ret = hns3_parse_udp(item, rule, error);
		step_mngr->items = L4_next_items;
		step_mngr->count = RTE_DIM(L4_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		ret = hns3_parse_sctp(item, rule, error);
		step_mngr->items = L4_next_items;
		step_mngr->count = RTE_DIM(L4_next_items);
		break;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "Unsupported normal type!");
	}

	return ret;
}

static int
hns3_validate_item(const struct rte_flow_item *item,
		   struct items_step_mngr step_mngr,
		   struct rte_flow_error *error)
{
	uint32_t i;

	if (item->last)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_LAST, item,
					  "Not supported last point for range");

	for (i = 0; i < step_mngr.count; i++) {
		if (item->type == step_mngr.items[i])
			break;
	}

	if (i == step_mngr.count) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item, "Inval or missing item");
	}
	return 0;
}

static inline bool
is_tunnel_packet(enum rte_flow_item_type type)
{
	if (type == RTE_FLOW_ITEM_TYPE_VXLAN_GPE ||
	    type == RTE_FLOW_ITEM_TYPE_VXLAN ||
	    type == RTE_FLOW_ITEM_TYPE_NVGRE ||
	    type == RTE_FLOW_ITEM_TYPE_GENEVE)
		return true;
	return false;
}

/*
 * Parse the flow director rule.
 * The supported PATTERN:
 *   case: non-tunnel packet:
 *     ETH : src-mac, dst-mac, ethertype
 *     VLAN: tag1, tag2
 *     IPv4: src-ip, dst-ip, tos, proto
 *     IPv6: src-ip(last 32 bit addr), dst-ip(last 32 bit addr), proto
 *     UDP : src-port, dst-port
 *     TCP : src-port, dst-port
 *     SCTP: src-port, dst-port, tag
 *   case: tunnel packet:
 *     OUTER-ETH: ethertype
 *     OUTER-L3 : proto
 *     OUTER-L4 : src-port, dst-port
 *     TUNNEL   : vni, flow-id(only valid when NVGRE)
 *     INNER-ETH/VLAN/IPv4/IPv6/UDP/TCP/SCTP: same as non-tunnel packet
 * The supported ACTION:
 *    QUEUE
 *    DROP
 *    COUNT
 *    MARK: the id range [0, 4094]
 *    FLAG
 *    RSS: only valid if firmware support FD_QUEUE_REGION.
 */
static int
hns3_parse_fdir_filter(struct rte_eth_dev *dev,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct hns3_fdir_rule *rule,
		       struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	const struct rte_flow_item *item;
	struct items_step_mngr step_mngr;
	int ret;

	/* FDIR is available only in PF driver */
	if (hns->is_vf)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "Fdir not supported in VF");

	step_mngr.items = first_items;
	step_mngr.count = RTE_DIM(first_items);
	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;

		ret = hns3_validate_item(item, step_mngr, error);
		if (ret)
			return ret;

		if (is_tunnel_packet(item->type)) {
			ret = hns3_parse_tunnel(item, rule, error);
			if (ret)
				return ret;
			step_mngr.items = tunnel_next_items;
			step_mngr.count = RTE_DIM(tunnel_next_items);
		} else {
			ret = hns3_parse_normal(item, rule, &step_mngr, error);
			if (ret)
				return ret;
		}
	}

	return hns3_handle_actions(dev, actions, rule, error);
}

static void
hns3_filterlist_flush(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_fdir_rule_ele *fdir_rule_ptr;
	struct hns3_flow_mem *flow_node;

	fdir_rule_ptr = TAILQ_FIRST(&hw->flow_fdir_list);
	while (fdir_rule_ptr) {
		TAILQ_REMOVE(&hw->flow_fdir_list, fdir_rule_ptr, entries);
		rte_free(fdir_rule_ptr);
		fdir_rule_ptr = TAILQ_FIRST(&hw->flow_fdir_list);
	}

	flow_node = TAILQ_FIRST(&hw->flow_list);
	while (flow_node) {
		TAILQ_REMOVE(&hw->flow_list, flow_node, entries);
		rte_free(flow_node->flow);
		rte_free(flow_node);
		flow_node = TAILQ_FIRST(&hw->flow_list);
	}
}

static bool
hns3_flow_rule_key_same(const struct rte_flow_action_rss *comp,
			const struct rte_flow_action_rss *with)
{
	if (comp->key_len != with->key_len)
		return false;

	if (with->key_len == 0)
		return true;

	if (comp->key == NULL && with->key == NULL)
		return true;

	if (!(comp->key != NULL && with->key != NULL))
		return false;

	return !memcmp(comp->key, with->key, with->key_len);
}

static bool
hns3_flow_rule_queues_same(const struct rte_flow_action_rss *comp,
			   const struct rte_flow_action_rss *with)
{
	if (comp->queue_num != with->queue_num)
		return false;

	if (with->queue_num == 0)
		return true;

	if (comp->queue == NULL && with->queue == NULL)
		return true;

	if (!(comp->queue != NULL && with->queue != NULL))
		return false;

	return !memcmp(comp->queue, with->queue, with->queue_num);
}

static bool
hns3_action_rss_same(const struct rte_flow_action_rss *comp,
		     const struct rte_flow_action_rss *with)
{
	bool same_level;
	bool same_types;
	bool same_func;

	same_level = (comp->level == with->level);
	same_types = (comp->types == with->types);
	same_func = (comp->func == with->func);

	return same_level && same_types && same_func &&
		hns3_flow_rule_key_same(comp, with) &&
		hns3_flow_rule_queues_same(comp, with);
}

static bool
hns3_valid_ipv6_sctp_rss_types(struct hns3_hw *hw, uint64_t types)
{
	/*
	 * Some hardware don't support to use src/dst port fields to hash
	 * for IPV6 SCTP packet type.
	 */
	if (types & RTE_ETH_RSS_NONFRAG_IPV6_SCTP &&
	    types & HNS3_RSS_SUPPORT_L4_SRC_DST &&
	    !hw->rss_info.ipv6_sctp_offload_supported)
		return false;

	return true;
}

static int
hns3_flow_parse_hash_func(const struct rte_flow_action_rss *rss_act,
			  struct hns3_flow_rss_conf *rss_conf,
			  struct rte_flow_error *error)
{
	if (rss_act->func >= RTE_ETH_HASH_FUNCTION_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "RSS hash func are not supported");

	rss_conf->conf.func = rss_act->func;
	return 0;
}

static int
hns3_flow_parse_hash_key(struct hns3_hw *hw,
			 const struct rte_flow_action_rss *rss_act,
			 struct hns3_flow_rss_conf *rss_conf,
			 struct rte_flow_error *error)
{
	if (rss_act->key_len != hw->rss_key_size)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "invalid RSS key length");

	if (rss_act->key != NULL)
		memcpy(rss_conf->key, rss_act->key, rss_act->key_len);
	else
		memcpy(rss_conf->key, hns3_hash_key,
			RTE_MIN(sizeof(hns3_hash_key), rss_act->key_len));
	/* Need to record if user sets hash key. */
	rss_conf->conf.key = rss_act->key;
	rss_conf->conf.key_len = rss_act->key_len;

	return 0;
}

static int
hns3_flow_parse_queues(struct hns3_hw *hw,
		       const struct rte_flow_action_rss *rss_act,
		       struct hns3_flow_rss_conf *rss_conf,
		       struct rte_flow_error *error)
{
	uint16_t i;

	if (rss_act->queue_num > hw->rss_ind_tbl_size)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "queue number can not exceed RSS indirection table.");

	if (rss_act->queue_num > HNS3_RSS_QUEUES_BUFFER_NUM)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "queue number configured exceeds queue buffer size driver supported");

	for (i = 0; i < rss_act->queue_num; i++) {
		if (rss_act->queue[i] >= hw->alloc_rss_size)
			return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						NULL,
						"queue id must be less than queue number allocated to a TC");
	}

	memcpy(rss_conf->queue, rss_act->queue,
	       rss_act->queue_num * sizeof(rss_conf->queue[0]));
	rss_conf->conf.queue = rss_conf->queue;
	rss_conf->conf.queue_num = rss_act->queue_num;

	return 0;
}

static int
hns3_flow_get_hw_pctype(struct hns3_hw *hw,
			const struct rte_flow_action_rss *rss_act,
			const struct hns3_hash_map_info *map,
			struct hns3_flow_rss_conf *rss_conf,
			struct rte_flow_error *error)
{
	uint64_t l3l4_src_dst, l3l4_refine, left_types;

	if (rss_act->types == 0) {
		/* Disable RSS hash of this packet type if types is zero. */
		rss_conf->hw_pctypes |= map->hw_pctype;
		return 0;
	}

	/*
	 * Can not have extra types except rss_pctype and l3l4_type in this map.
	 */
	left_types = ~map->rss_pctype & rss_act->types;
	if (left_types & ~map->l3l4_types)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "cannot set extra types.");

	l3l4_src_dst = left_types;
	/* L3/L4 SRC and DST shouldn't be specified at the same time. */
	l3l4_refine = rte_eth_rss_hf_refine(l3l4_src_dst);
	if (l3l4_refine != l3l4_src_dst)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "cannot specify L3_SRC/DST_ONLY or L4_SRC/DST_ONLY at the same.");

	if (!hns3_valid_ipv6_sctp_rss_types(hw, rss_act->types))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "hardware doesn't support to use L4 src/dst to hash for IPV6-SCTP.");

	rss_conf->hw_pctypes |= map->hw_pctype;

	return 0;
}

static int
hns3_flow_parse_rss_types_by_ptype(struct hns3_hw *hw,
				   const struct rte_flow_action_rss *rss_act,
				   uint64_t pattern_type,
				   struct hns3_flow_rss_conf *rss_conf,
				   struct rte_flow_error *error)
{
	const struct hns3_hash_map_info *map;
	bool matched = false;
	uint16_t i;
	int ret;

	for (i = 0; i < RTE_DIM(hash_map_table); i++) {
		map = &hash_map_table[i];
		if (map->pattern_type != pattern_type) {
			/*
			 * If the target pattern type is already matched with
			 * the one before this pattern in the hash map table,
			 * no need to continue walk.
			 */
			if (matched)
				break;
			continue;
		}
		matched = true;

		/*
		 * If pattern type is matched and the 'types' is zero, all packet flow
		 * types related to this pattern type disable RSS hash.
		 * Otherwise, RSS types must match the pattern type and cannot have no
		 * extra or unsupported types.
		 */
		if (rss_act->types != 0 && !(map->rss_pctype & rss_act->types))
			continue;

		ret = hns3_flow_get_hw_pctype(hw, rss_act, map, rss_conf, error);
		if (ret != 0)
			return ret;
	}

	if (rss_conf->hw_pctypes != 0)
		return 0;

	if (matched)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "RSS types are unsupported");

	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				  NULL, "Pattern specified is unsupported");
}

static uint64_t
hns3_flow_get_all_hw_pctypes(uint64_t types)
{
	uint64_t hw_pctypes = 0;
	uint16_t i;

	for (i = 0; i < RTE_DIM(hash_map_table); i++) {
		if (types & hash_map_table[i].rss_pctype)
			hw_pctypes |= hash_map_table[i].hw_pctype;
	}

	return hw_pctypes;
}

static int
hns3_flow_parse_rss_types(struct hns3_hw *hw,
			  const struct rte_flow_action_rss *rss_act,
			  uint64_t pattern_type,
			  struct hns3_flow_rss_conf *rss_conf,
			  struct rte_flow_error *error)
{
	rss_conf->conf.types = rss_act->types;

	/* no pattern specified to set global RSS types. */
	if (pattern_type == 0) {
		if (!hns3_check_rss_types_valid(hw, rss_act->types))
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					NULL, "RSS types is invalid.");
		rss_conf->hw_pctypes =
				hns3_flow_get_all_hw_pctypes(rss_act->types);
		return 0;
	}

	return hns3_flow_parse_rss_types_by_ptype(hw, rss_act, pattern_type,
						  rss_conf, error);
}

static int
hns3_flow_parse_hash_global_conf(struct rte_eth_dev *dev,
				 const struct rte_flow_action_rss *rss_act,
				 struct hns3_flow_rss_conf *rss_conf,
				 struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	ret = hns3_flow_parse_hash_func(rss_act, rss_conf, error);
	if (ret != 0)
		return ret;

	if (rss_act->queue_num > 0) {
		ret = hns3_flow_parse_queues(hw, rss_act, rss_conf, error);
		if (ret != 0)
			return ret;
	}

	if (rss_act->key_len > 0) {
		ret = hns3_flow_parse_hash_key(hw, rss_act, rss_conf, error);
		if (ret != 0)
			return ret;
	}

	return hns3_flow_parse_rss_types(hw, rss_act, rss_conf->pattern_type,
					 rss_conf, error);
}

static int
hns3_flow_parse_pattern_type(const struct rte_flow_item pattern[],
			     uint64_t *ptype, struct rte_flow_error *error)
{
	enum rte_flow_item_type pre_type = RTE_FLOW_ITEM_TYPE_VOID;
	const char *message = "Pattern specified isn't supported";
	uint64_t item_hdr, pattern_hdrs = 0;
	enum rte_flow_item_type cur_type;

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		if (pattern->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;
		if (pattern->mask || pattern->spec || pattern->last) {
			message = "Header info shouldn't be specified";
			goto unsup;
		}

		/* Check the sub-item allowed by the previous item . */
		if (pre_type >= RTE_DIM(hash_pattern_next_allow_items) ||
		    !(hash_pattern_next_allow_items[pre_type] &
				BIT_ULL(pattern->type)))
			goto unsup;

		cur_type = pattern->type;
		/* Unsupported for current type being greater than array size. */
		if (cur_type >= RTE_DIM(hash_pattern_item_header))
			goto unsup;

		/* The value is zero, which means unsupported current header. */
		item_hdr = hash_pattern_item_header[cur_type];
		if (item_hdr == 0)
			goto unsup;

		/* Have duplicate pattern header. */
		if (item_hdr & pattern_hdrs)
			goto unsup;
		pre_type = cur_type;
		pattern_hdrs |= item_hdr;
	}

	if (pattern_hdrs != 0) {
		*ptype = pattern_hdrs;
		return 0;
	}

unsup:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  pattern, message);
}

static int
hns3_flow_parse_pattern_act(struct rte_eth_dev *dev,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action_rss *rss_act,
			    struct hns3_flow_rss_conf *rss_conf,
			    struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	ret = hns3_flow_parse_hash_func(rss_act, rss_conf, error);
	if (ret != 0)
		return ret;

	if (rss_act->key_len > 0) {
		ret = hns3_flow_parse_hash_key(hw, rss_act, rss_conf, error);
		if (ret != 0)
			return ret;
	}

	if (rss_act->queue_num > 0) {
		ret = hns3_flow_parse_queues(hw, rss_act, rss_conf, error);
		if (ret != 0)
			return ret;
	}

	ret = hns3_flow_parse_pattern_type(pattern, &rss_conf->pattern_type,
					   error);
	if (ret != 0)
		return ret;

	ret = hns3_flow_parse_rss_types(hw, rss_act, rss_conf->pattern_type,
					rss_conf, error);
	if (ret != 0)
		return ret;

	if (rss_act->func != RTE_ETH_HASH_FUNCTION_DEFAULT ||
	    rss_act->key_len > 0 || rss_act->queue_num > 0)
		hns3_warn(hw, "hash func, key and queues are global config, which work for all flow types. "
			  "Recommend: don't set them together with pattern.");

	return 0;
}

static bool
hns3_rss_action_is_dup(struct hns3_hw *hw,
		       const struct hns3_flow_rss_conf *conf)
{
	struct hns3_rss_conf_ele *filter;

	TAILQ_FOREACH(filter, &hw->flow_rss_list, entries) {
		if (conf->pattern_type != filter->filter_info.pattern_type)
			continue;

		if (hns3_action_rss_same(&filter->filter_info.conf, &conf->conf))
			return true;
	}

	return false;
}

/*
 * This function is used to parse rss action validation.
 */
static int
hns3_parse_rss_filter(struct rte_eth_dev *dev,
		      const struct rte_flow_item pattern[],
		      const struct rte_flow_action *actions,
		      struct hns3_flow_rss_conf *rss_conf,
		      struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	const struct rte_flow_action_rss *rss_act;
	const struct rte_flow_action *act;
	const struct rte_flow_item *pat;
	struct hns3_hw *hw = &hns->hw;
	uint32_t index = 0;
	int ret;

	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (actions[1].type != RTE_FLOW_ACTION_TYPE_END)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  &actions[1],
					  "Only support one action for RSS.");

	rss_act = (const struct rte_flow_action_rss *)act->conf;
	if (rss_act == NULL) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  act, "lost RSS action configuration");
	}

	if (rss_act->level != 0)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  act,
					  "RSS level is not supported");

	index = 0;
	NEXT_ITEM_OF_PATTERN(pat, pattern, index);
	if (pat[0].type == RTE_FLOW_ITEM_TYPE_END) {
		rss_conf->pattern_type = 0;
		ret = hns3_flow_parse_hash_global_conf(dev, rss_act,
						       rss_conf, error);
	} else {
		ret = hns3_flow_parse_pattern_act(dev, pat, rss_act,
						  rss_conf, error);
	}
	if (ret != 0)
		return ret;

	if (hns3_rss_action_is_dup(hw, rss_conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  act, "duplicate RSS rule");

	return 0;
}

static int
hns3_update_indir_table(struct hns3_hw *hw,
			const struct rte_flow_action_rss *conf, uint16_t num)
{
	uint16_t indir_tbl[HNS3_RSS_IND_TBL_SIZE_MAX];
	uint16_t j;
	uint32_t i;

	/* Fill in redirection table */
	for (i = 0, j = 0; i < hw->rss_ind_tbl_size; i++, j++) {
		j %= num;
		if (conf->queue[j] >= hw->alloc_rss_size) {
			hns3_err(hw, "queue id(%u) set to redirection table "
				 "exceeds queue number(%u) allocated to a TC.",
				 conf->queue[j], hw->alloc_rss_size);
			return -EINVAL;
		}
		indir_tbl[i] = conf->queue[j];
	}

	return hns3_set_rss_indir_table(hw, indir_tbl, hw->rss_ind_tbl_size);
}

static uint64_t
hns3_flow_get_pctype_tuple_mask(uint64_t hw_pctype)
{
	uint64_t tuple_mask = 0;
	uint16_t i;

	for (i = 0; i < RTE_DIM(hash_map_table); i++) {
		if (hw_pctype == hash_map_table[i].hw_pctype) {
			tuple_mask = hash_map_table[i].tuple_mask;
			break;
		}
	}

	return tuple_mask;
}

static int
hns3_flow_set_rss_ptype_tuple(struct hns3_hw *hw,
			      struct hns3_flow_rss_conf *rss_conf)
{
	uint64_t old_tuple_fields, new_tuple_fields;
	uint64_t hw_pctypes, tuples, tuple_mask = 0;
	bool cfg_global_tuple;
	int ret;

	cfg_global_tuple = (rss_conf->pattern_type == 0);
	if (!cfg_global_tuple) {
		/*
		 * To ensure that different packets do not affect each other,
		 * we have to first read all tuple fields, and then only modify
		 * the tuples for the specified packet type.
		 */
		ret = hns3_get_rss_tuple_field(hw, &old_tuple_fields);
		if (ret != 0)
			return ret;

		new_tuple_fields = old_tuple_fields;
		hw_pctypes = rss_conf->hw_pctypes;
		while (hw_pctypes > 0) {
			uint32_t idx = rte_bsf64(hw_pctypes);
			uint64_t pctype = BIT_ULL(idx);

			tuple_mask = hns3_flow_get_pctype_tuple_mask(pctype);
			tuples = hns3_rss_calc_tuple_filed(rss_conf->conf.types);
			new_tuple_fields &= ~tuple_mask;
			new_tuple_fields |= tuples;
			hw_pctypes &= ~pctype;
		}
	} else {
		new_tuple_fields =
			hns3_rss_calc_tuple_filed(rss_conf->conf.types);
	}

	ret = hns3_set_rss_tuple_field(hw, new_tuple_fields);
	if (ret != 0)
		return ret;

	if (!cfg_global_tuple)
		hns3_info(hw, "RSS tuple fields changed from 0x%" PRIx64 " to 0x%" PRIx64,
			  old_tuple_fields, new_tuple_fields);

	return 0;
}

static int
hns3_config_rss_filter(struct hns3_hw *hw,
		       struct hns3_flow_rss_conf *rss_conf)
{
	struct rte_flow_action_rss *rss_act;
	int ret;

	rss_act = &rss_conf->conf;
	if (rss_act->queue_num > 0) {
		ret = hns3_update_indir_table(hw, rss_act, rss_act->queue_num);
		if (ret) {
			hns3_err(hw, "set queues action failed, ret = %d", ret);
			return ret;
		}
	}

	if (rss_act->key_len > 0 ||
	    rss_act->func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		ret = hns3_update_rss_algo_key(hw, rss_act->func, rss_conf->key,
					       rss_act->key_len);
		if (ret != 0) {
			hns3_err(hw, "set func or hash key action failed, ret = %d",
				 ret);
			return ret;
		}
	}

	if (rss_conf->hw_pctypes > 0) {
		ret = hns3_flow_set_rss_ptype_tuple(hw, rss_conf);
		if (ret != 0) {
			hns3_err(hw, "set types action failed, ret = %d", ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3_clear_rss_filter(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_rss_conf_ele *rss_filter_ptr;
	struct hns3_hw *hw = &hns->hw;

	rss_filter_ptr = TAILQ_FIRST(&hw->flow_rss_list);
	while (rss_filter_ptr) {
		TAILQ_REMOVE(&hw->flow_rss_list, rss_filter_ptr, entries);
		rte_free(rss_filter_ptr);
		rss_filter_ptr = TAILQ_FIRST(&hw->flow_rss_list);
	}

	return hns3_config_rss(hns);
}

static int
hns3_reconfig_all_rss_filter(struct hns3_hw *hw)
{
	struct hns3_rss_conf_ele *filter;
	uint32_t rule_no = 0;
	int ret;

	TAILQ_FOREACH(filter, &hw->flow_rss_list, entries) {
		ret = hns3_config_rss_filter(hw, &filter->filter_info);
		if (ret != 0) {
			hns3_err(hw, "config %uth RSS filter failed, ret = %d",
				 rule_no, ret);
			return ret;
		}
		rule_no++;
	}

	return 0;
}

static int
hns3_restore_rss_filter(struct hns3_hw *hw)
{
	int ret;

	pthread_mutex_lock(&hw->flows_lock);
	ret = hns3_reconfig_all_rss_filter(hw);
	pthread_mutex_unlock(&hw->flows_lock);

	return ret;
}

int
hns3_restore_filter(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_restore_all_fdir_filter(hns);
	if (ret != 0)
		return ret;

	return hns3_restore_rss_filter(hw);
}

static int
hns3_flow_args_check(const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[],
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	if (pattern == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_NUM,
					  NULL, "NULL pattern.");

	if (actions == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_NUM,
					  NULL, "NULL action.");

	if (attr == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR,
					  NULL, "NULL attribute.");

	return hns3_check_attr(attr, error);
}

/*
 * Check if the flow rule is supported by hns3.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int
hns3_flow_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error,
		   struct hns3_filter_info *filter_info)
{
	union hns3_filter_conf *conf;
	int ret;

	ret = hns3_flow_args_check(attr, pattern, actions, error);
	if (ret)
		return ret;

	hns3_parse_filter_type(pattern, actions, filter_info);
	conf = &filter_info->conf;
	if (filter_info->type == RTE_ETH_FILTER_HASH)
		return hns3_parse_rss_filter(dev, pattern, actions,
					     &conf->rss_conf, error);

	return hns3_parse_fdir_filter(dev, pattern, actions,
				      &conf->fdir_conf, error);
}

static int
hns3_flow_rebuild_all_rss_filter(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_config_rss(hns);
	if (ret != 0) {
		hns3_err(hw, "restore original RSS configuration failed, ret = %d.",
			 ret);
		return ret;
	}
	ret = hns3_reconfig_all_rss_filter(hw);
	if (ret != 0)
		hns3_err(hw, "rebuild all RSS filter failed, ret = %d.", ret);

	return ret;
}

static int
hns3_flow_create_rss_rule(struct rte_eth_dev *dev,
			  struct hns3_flow_rss_conf *rss_conf,
			  struct rte_flow *flow)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_rss_conf_ele *rss_filter_ptr;
	struct hns3_flow_rss_conf *new_conf;
	struct rte_flow_action_rss *rss_act;
	int ret;

	rss_filter_ptr = rte_zmalloc("hns3 rss filter",
				     sizeof(struct hns3_rss_conf_ele), 0);
	if (rss_filter_ptr == NULL) {
		hns3_err(hw, "failed to allocate hns3_rss_filter memory");
		return -ENOMEM;
	}

	new_conf = &rss_filter_ptr->filter_info;
	memcpy(new_conf, rss_conf, sizeof(*new_conf));
	rss_act = &new_conf->conf;
	if (rss_act->queue_num > 0)
		new_conf->conf.queue = new_conf->queue;
	/*
	 * There are two ways to deliver hash key action:
	 * 1> 'key_len' is greater than zero and 'key' isn't NULL.
	 * 2> 'key_len' is greater than zero, but 'key' is NULL.
	 * For case 2, we need to keep 'key' of the new_conf is NULL so as to
	 * inherit the configuration from user in case of failing to verify
	 * duplicate rule later.
	 */
	if (rss_act->key_len > 0 && rss_act->key != NULL)
		new_conf->conf.key = new_conf->key;

	ret = hns3_config_rss_filter(hw, new_conf);
	if (ret != 0) {
		rte_free(rss_filter_ptr);
		(void)hns3_flow_rebuild_all_rss_filter(hns);
		return ret;
	}

	TAILQ_INSERT_TAIL(&hw->flow_rss_list, rss_filter_ptr, entries);
	flow->rule = rss_filter_ptr;
	flow->filter_type = RTE_ETH_FILTER_HASH;

	return 0;
}

static int
hns3_flow_create_fdir_rule(struct rte_eth_dev *dev,
			   struct hns3_fdir_rule *fdir_rule,
			   struct rte_flow_error *error,
			   struct rte_flow *flow)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_fdir_rule_ele *fdir_rule_ptr;
	bool indir;
	int ret;

	indir = !!(fdir_rule->flags & HNS3_RULE_FLAG_COUNTER_INDIR);
	if (fdir_rule->flags & HNS3_RULE_FLAG_COUNTER) {
		ret = hns3_counter_new(dev, indir, fdir_rule->act_cnt.id,
				       error);
		if (ret != 0)
			return ret;

		flow->counter_id = fdir_rule->act_cnt.id;
	}

	fdir_rule_ptr = rte_zmalloc("hns3 fdir rule",
				    sizeof(struct hns3_fdir_rule_ele), 0);
	if (fdir_rule_ptr == NULL) {
		hns3_err(hw, "failed to allocate fdir_rule memory.");
		ret = -ENOMEM;
		goto err_malloc;
	}

	/*
	 * After all the preceding tasks are successfully configured, configure
	 * rules to the hardware to simplify the rollback of rules in the
	 * hardware.
	 */
	ret = hns3_fdir_filter_program(hns, fdir_rule, false);
	if (ret != 0)
		goto err_fdir_filter;

	memcpy(&fdir_rule_ptr->fdir_conf, fdir_rule,
		sizeof(struct hns3_fdir_rule));
	TAILQ_INSERT_TAIL(&hw->flow_fdir_list, fdir_rule_ptr, entries);
	flow->rule = fdir_rule_ptr;
	flow->filter_type = RTE_ETH_FILTER_FDIR;

	return 0;

err_fdir_filter:
	rte_free(fdir_rule_ptr);
err_malloc:
	if (fdir_rule->flags & HNS3_RULE_FLAG_COUNTER)
		hns3_counter_release(dev, fdir_rule->act_cnt.id);

	return ret;
}

/*
 * Create or destroy a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hit first.
 * So, the sequence matters.
 */
static struct rte_flow *
hns3_flow_create(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_filter_info filter_info = {0};
	struct hns3_flow_mem *flow_node;
	struct hns3_hw *hw = &hns->hw;
	union hns3_filter_conf *conf;
	struct rte_flow *flow;
	int ret;

	ret = hns3_flow_validate(dev, attr, pattern, actions, error,
				 &filter_info);
	if (ret)
		return NULL;

	flow = rte_zmalloc("hns3 flow", sizeof(struct rte_flow), 0);
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to allocate flow memory");
		return NULL;
	}
	flow_node = rte_zmalloc("hns3 flow node",
				sizeof(struct hns3_flow_mem), 0);
	if (flow_node == NULL) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to allocate flow list memory");
		rte_free(flow);
		return NULL;
	}

	flow_node->flow = flow;
	conf = &filter_info.conf;
	TAILQ_INSERT_TAIL(&hw->flow_list, flow_node, entries);
	if (filter_info.type == RTE_ETH_FILTER_HASH)
		ret = hns3_flow_create_rss_rule(dev, &conf->rss_conf, flow);
	else
		ret = hns3_flow_create_fdir_rule(dev, &conf->fdir_conf,
						 error, flow);
	if (ret == 0)
		return flow;

	rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow");
	TAILQ_REMOVE(&hw->flow_list, flow_node, entries);
	rte_free(flow_node);
	rte_free(flow);

	return NULL;
}

/* Destroy a flow rule on hns3. */
static int
hns3_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_fdir_rule_ele *fdir_rule_ptr;
	struct hns3_rss_conf_ele *rss_filter_ptr;
	struct hns3_flow_mem *flow_node;
	enum rte_filter_type filter_type;
	struct hns3_fdir_rule fdir_rule;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (flow == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_HANDLE,
					  flow, "Flow is NULL");

	filter_type = flow->filter_type;
	switch (filter_type) {
	case RTE_ETH_FILTER_FDIR:
		fdir_rule_ptr = (struct hns3_fdir_rule_ele *)flow->rule;
		memcpy(&fdir_rule, &fdir_rule_ptr->fdir_conf,
			   sizeof(struct hns3_fdir_rule));

		ret = hns3_fdir_filter_program(hns, &fdir_rule, true);
		if (ret)
			return rte_flow_error_set(error, EIO,
						  RTE_FLOW_ERROR_TYPE_HANDLE,
						  flow,
						  "Destroy FDIR fail.Try again");
		if (fdir_rule.flags & HNS3_RULE_FLAG_COUNTER)
			hns3_counter_release(dev, fdir_rule.act_cnt.id);
		TAILQ_REMOVE(&hw->flow_fdir_list, fdir_rule_ptr, entries);
		rte_free(fdir_rule_ptr);
		fdir_rule_ptr = NULL;
		break;
	case RTE_ETH_FILTER_HASH:
		rss_filter_ptr = (struct hns3_rss_conf_ele *)flow->rule;
		TAILQ_REMOVE(&hw->flow_rss_list, rss_filter_ptr, entries);
		rte_free(rss_filter_ptr);
		rss_filter_ptr = NULL;
		(void)hns3_flow_rebuild_all_rss_filter(hns);
		break;
	default:
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_HANDLE, flow,
					  "Unsupported filter type");
	}

	TAILQ_FOREACH(flow_node, &hw->flow_list, entries) {
		if (flow_node->flow == flow) {
			TAILQ_REMOVE(&hw->flow_list, flow_node, entries);
			rte_free(flow_node);
			flow_node = NULL;
			break;
		}
	}
	rte_free(flow);

	return 0;
}

/*  Destroy all flow rules associated with a port on hns3. */
static int
hns3_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	int ret;

	/* FDIR is available only in PF driver */
	if (!hns->is_vf) {
		ret = hns3_clear_all_fdir_filter(hns);
		if (ret) {
			rte_flow_error_set(error, ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL, "Failed to flush rule");
			return ret;
		}
		hns3_counter_flush(dev);
	}

	ret = hns3_clear_rss_filter(dev);
	if (ret) {
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to flush rss filter");
		return ret;
	}

	hns3_filterlist_flush(dev);

	return 0;
}

/* Query an existing flow rule. */
static int
hns3_flow_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		const struct rte_flow_action *actions, void *data,
		struct rte_flow_error *error)
{
	struct rte_flow_action_rss *rss_conf;
	struct hns3_rss_conf_ele *rss_rule;
	struct rte_flow_query_count *qc;
	int ret;

	if (!flow->rule)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "invalid rule");

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			qc = (struct rte_flow_query_count *)data;
			ret = hns3_counter_query(dev, flow, qc, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (flow->filter_type != RTE_ETH_FILTER_HASH) {
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions, "action is not supported");
			}
			rss_conf = (struct rte_flow_action_rss *)data;
			rss_rule = (struct hns3_rss_conf_ele *)flow->rule;
			rte_memcpy(rss_conf, &rss_rule->filter_info.conf,
				   sizeof(struct rte_flow_action_rss));
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION,
				actions, "action is not supported");
		}
	}

	return 0;
}

static int
hns3_flow_validate_wrap(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_filter_info filter_info = {0};
	int ret;

	pthread_mutex_lock(&hw->flows_lock);
	ret = hns3_flow_validate(dev, attr, pattern, actions, error,
				 &filter_info);
	pthread_mutex_unlock(&hw->flows_lock);

	return ret;
}

static struct rte_flow *
hns3_flow_create_wrap(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		      const struct rte_flow_item pattern[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_flow *flow;

	pthread_mutex_lock(&hw->flows_lock);
	flow = hns3_flow_create(dev, attr, pattern, actions, error);
	pthread_mutex_unlock(&hw->flows_lock);

	return flow;
}

static int
hns3_flow_destroy_wrap(struct rte_eth_dev *dev, struct rte_flow *flow,
		       struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	pthread_mutex_lock(&hw->flows_lock);
	ret = hns3_flow_destroy(dev, flow, error);
	pthread_mutex_unlock(&hw->flows_lock);

	return ret;
}

static int
hns3_flow_flush_wrap(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	pthread_mutex_lock(&hw->flows_lock);
	ret = hns3_flow_flush(dev, error);
	pthread_mutex_unlock(&hw->flows_lock);

	return ret;
}

static int
hns3_flow_query_wrap(struct rte_eth_dev *dev, struct rte_flow *flow,
		     const struct rte_flow_action *actions, void *data,
		     struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	pthread_mutex_lock(&hw->flows_lock);
	ret = hns3_flow_query(dev, flow, actions, data, error);
	pthread_mutex_unlock(&hw->flows_lock);

	return ret;
}

static int
hns3_check_indir_action(const struct rte_flow_indir_action_conf *conf,
			const struct rte_flow_action *action,
			struct rte_flow_error *error)
{
	if (!conf->ingress)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "Indir action ingress can't be zero");

	if (conf->egress)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "Indir action not support egress");

	if (conf->transfer)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "Indir action not support transfer");

	if (action->type != RTE_FLOW_ACTION_TYPE_COUNT)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "Indir action only support count");

	return 0;
}

static struct rte_flow_action_handle *
hns3_flow_action_create(struct rte_eth_dev *dev,
			const struct rte_flow_indir_action_conf *conf,
			const struct rte_flow_action *action,
			struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct rte_flow_action_count *act_count;
	struct rte_flow_action_handle *handle = NULL;
	struct hns3_flow_counter *counter;

	if (hns3_check_indir_action(conf, action, error))
		return NULL;

	handle = rte_zmalloc("hns3 action handle",
			     sizeof(struct rte_flow_action_handle), 0);
	if (handle == NULL) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to allocate action memory");
		return NULL;
	}

	pthread_mutex_lock(&hw->flows_lock);

	act_count = (const struct rte_flow_action_count *)action->conf;
	if (act_count->id >= pf->fdir.fd_cfg.cnt_num[HNS3_FD_STAGE_1]) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				   action, "Invalid counter id");
		goto err_exit;
	}

	if (hns3_counter_new(dev, false, act_count->id, error))
		goto err_exit;

	counter = hns3_counter_lookup(dev, act_count->id);
	if (counter == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				   action, "Counter id not found");
		goto err_exit;
	}

	counter->indirect = true;
	handle->indirect_type = HNS3_INDIRECT_ACTION_TYPE_COUNT;
	handle->counter_id = counter->id;

	pthread_mutex_unlock(&hw->flows_lock);
	return handle;

err_exit:
	pthread_mutex_unlock(&hw->flows_lock);
	rte_free(handle);
	return NULL;
}

static int
hns3_flow_action_destroy(struct rte_eth_dev *dev,
			 struct rte_flow_action_handle *handle,
			 struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_flow_counter *counter;

	pthread_mutex_lock(&hw->flows_lock);

	if (handle->indirect_type != HNS3_INDIRECT_ACTION_TYPE_COUNT) {
		pthread_mutex_unlock(&hw->flows_lock);
		return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					handle, "Invalid indirect type");
	}

	counter = hns3_counter_lookup(dev, handle->counter_id);
	if (counter == NULL) {
		pthread_mutex_unlock(&hw->flows_lock);
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				handle, "Counter id not exist");
	}

	if (counter->ref_cnt > 1) {
		pthread_mutex_unlock(&hw->flows_lock);
		return rte_flow_error_set(error, EBUSY,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				handle, "Counter id in use");
	}

	(void)hns3_counter_release(dev, handle->counter_id);
	rte_free(handle);

	pthread_mutex_unlock(&hw->flows_lock);
	return 0;
}

static int
hns3_flow_action_query(struct rte_eth_dev *dev,
		 const struct rte_flow_action_handle *handle,
		 void *data,
		 struct rte_flow_error *error)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_flow flow;
	int ret;

	pthread_mutex_lock(&hw->flows_lock);

	if (handle->indirect_type != HNS3_INDIRECT_ACTION_TYPE_COUNT) {
		pthread_mutex_unlock(&hw->flows_lock);
		return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					handle, "Invalid indirect type");
	}

	memset(&flow, 0, sizeof(flow));
	flow.counter_id = handle->counter_id;
	ret = hns3_counter_query(dev, &flow,
				 (struct rte_flow_query_count *)data, error);
	pthread_mutex_unlock(&hw->flows_lock);
	return ret;
}

static const struct rte_flow_ops hns3_flow_ops = {
	.validate = hns3_flow_validate_wrap,
	.create = hns3_flow_create_wrap,
	.destroy = hns3_flow_destroy_wrap,
	.flush = hns3_flow_flush_wrap,
	.query = hns3_flow_query_wrap,
	.isolate = NULL,
	.action_handle_create = hns3_flow_action_create,
	.action_handle_destroy = hns3_flow_action_destroy,
	.action_handle_query = hns3_flow_action_query,
};

int
hns3_dev_flow_ops_get(struct rte_eth_dev *dev,
		      const struct rte_flow_ops **ops)
{
	struct hns3_hw *hw;

	hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (hw->adapter_state >= HNS3_NIC_CLOSED)
		return -ENODEV;

	*ops = &hns3_flow_ops;
	return 0;
}

void
hns3_flow_init(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pthread_mutexattr_t attr;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&hw->flows_lock, &attr);
	dev->data->dev_flags |= RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE;

	TAILQ_INIT(&hw->flow_fdir_list);
	TAILQ_INIT(&hw->flow_rss_list);
	TAILQ_INIT(&hw->flow_list);
}

void
hns3_flow_uninit(struct rte_eth_dev *dev)
{
	struct rte_flow_error error;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		hns3_flow_flush_wrap(dev, &error);
}
