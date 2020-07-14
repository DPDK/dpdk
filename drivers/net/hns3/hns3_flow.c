/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <stdbool.h>
#include <sys/queue.h>
#include <rte_flow_driver.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"

/* Default default keys */
static uint8_t hns3_hash_key[] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA
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
	RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
	RTE_FLOW_ITEM_TYPE_MPLS
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
	RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
	RTE_FLOW_ITEM_TYPE_MPLS
};

static enum rte_flow_item_type tunnel_next_items[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN
};

struct items_step_mngr {
	enum rte_flow_item_type *items;
	int count;
};

static inline void
net_addr_to_host(uint32_t *dst, const rte_be32_t *src, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		dst[i] = rte_be_to_cpu_32(src[i]);
}

static inline const struct rte_flow_action *
find_rss_action(const struct rte_flow_action actions[])
{
	const struct rte_flow_action *next = &actions[0];

	for (; next->type != RTE_FLOW_ACTION_TYPE_END; next++) {
		if (next->type == RTE_FLOW_ACTION_TYPE_RSS)
			return next;
	}
	return NULL;
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
hns3_counter_new(struct rte_eth_dev *dev, uint32_t shared, uint32_t id,
		 struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_flow_counter *cnt;

	cnt = hns3_counter_lookup(dev, id);
	if (cnt) {
		if (!cnt->shared || cnt->shared != shared)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  cnt,
						  "Counter id is used,shared flag not match");
		cnt->ref_cnt++;
		return 0;
	}

	cnt = rte_zmalloc("hns3 counter", sizeof(*cnt), 0);
	if (cnt == NULL)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_ACTION, cnt,
					  "Alloc mem for counter failed");
	cnt->id = id;
	cnt->shared = shared;
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
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Can't find counter id");

	ret = hns3_get_count(&hns->hw, flow->counter_id, &value);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Read counter fail.");
		return ret;
	}
	qc->hits_set = 1;
	qc->hits = value;

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
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_flow_counter *cnt_ptr;

	cnt_ptr = LIST_FIRST(&pf->flow_counters);
	while (cnt_ptr) {
		LIST_REMOVE(cnt_ptr, next);
		rte_free(cnt_ptr);
		cnt_ptr = LIST_FIRST(&pf->flow_counters);
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
	if (queue->index >= hw->used_rx_queues) {
		hns3_err(hw, "queue ID(%d) is greater than number of "
			  "available queue (%d) in driver.",
			  queue->index, hw->used_rx_queues);
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Invalid queue ID in PF");
	}

	rule->queue_id = queue->index;
	rule->action = HNS3_FD_ACTION_ACCEPT_PACKET;
	return 0;
}

/*
 * Parse actions structure from the provided pattern.
 * The pattern is validated as the items are copied.
 *
 * @param actions[in]
 * @param rule[out]
 *   NIC specfilc actions derived from the actions.
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
		case RTE_FLOW_ACTION_TYPE_MARK:
			mark =
			    (const struct rte_flow_action_mark *)actions->conf;
			if (mark->id >= HNS3_MAX_FILTER_ID)
				return rte_flow_error_set(error, EINVAL,
						     RTE_FLOW_ERROR_TYPE_ACTION,
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
						     RTE_FLOW_ERROR_TYPE_ACTION,
						     actions,
						     "Invalid counter id");
			rule->act_cnt = *act_count;
			rule->flags |= HNS3_RULE_FLAG_COUNTER;
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

/* Parse to get the attr and action info of flow director rule. */
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
hns3_parse_eth(const struct rte_flow_item *item,
		   struct hns3_fdir_rule *rule, struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		eth_mask = item->mask;
		if (eth_mask->type) {
			hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 1);
			rule->key_conf.mask.ether_type =
			    rte_be_to_cpu_16(eth_mask->type);
		}
		if (!rte_is_zero_ether_addr(&eth_mask->src)) {
			hns3_set_bit(rule->input_set, INNER_SRC_MAC, 1);
			memcpy(rule->key_conf.mask.src_mac,
			       eth_mask->src.addr_bytes, RTE_ETHER_ADDR_LEN);
		}
		if (!rte_is_zero_ether_addr(&eth_mask->dst)) {
			hns3_set_bit(rule->input_set, INNER_DST_MAC, 1);
			memcpy(rule->key_conf.mask.dst_mac,
			       eth_mask->dst.addr_bytes, RTE_ETHER_ADDR_LEN);
		}
	}

	eth_spec = item->spec;
	rule->key_conf.spec.ether_type = rte_be_to_cpu_16(eth_spec->type);
	memcpy(rule->key_conf.spec.src_mac, eth_spec->src.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	memcpy(rule->key_conf.spec.dst_mac, eth_spec->dst.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	return 0;
}

static int
hns3_parse_vlan(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_spec;
	const struct rte_flow_item_vlan *vlan_mask;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

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
		if (vlan_mask->tci) {
			if (rule->key_conf.vlan_num == 1) {
				hns3_set_bit(rule->input_set, INNER_VLAN_TAG1,
					     1);
				rule->key_conf.mask.vlan_tag1 =
				    rte_be_to_cpu_16(vlan_mask->tci);
			} else {
				hns3_set_bit(rule->input_set, INNER_VLAN_TAG2,
					     1);
				rule->key_conf.mask.vlan_tag2 =
				    rte_be_to_cpu_16(vlan_mask->tci);
			}
		}
	}

	vlan_spec = item->spec;
	if (rule->key_conf.vlan_num == 1)
		rule->key_conf.spec.vlan_tag1 =
		    rte_be_to_cpu_16(vlan_spec->tci);
	else
		rule->key_conf.spec.vlan_tag2 =
		    rte_be_to_cpu_16(vlan_spec->tci);
	return 0;
}

static int
hns3_parse_ipv4(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

	hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 1);
	rule->key_conf.spec.ether_type = RTE_ETHER_TYPE_IPV4;
	rule->key_conf.mask.ether_type = ETHER_TYPE_MASK;
	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		ipv4_mask = item->mask;

		if (ipv4_mask->hdr.total_length ||
		    ipv4_mask->hdr.packet_id ||
		    ipv4_mask->hdr.fragment_offset ||
		    ipv4_mask->hdr.time_to_live ||
		    ipv4_mask->hdr.hdr_checksum) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
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

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

	hns3_set_bit(rule->input_set, INNER_ETH_TYPE, 1);
	rule->key_conf.spec.ether_type = RTE_ETHER_TYPE_IPV6;
	rule->key_conf.mask.ether_type = ETHER_TYPE_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		ipv6_mask = item->mask;
		if (ipv6_mask->hdr.vtc_flow ||
		    ipv6_mask->hdr.payload_len || ipv6_mask->hdr.hop_limits) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
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

static int
hns3_parse_tcp(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
	       struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

	hns3_set_bit(rule->input_set, INNER_IP_PROTO, 1);
	rule->key_conf.spec.ip_proto = IPPROTO_TCP;
	rule->key_conf.mask.ip_proto = IPPROTO_MASK;

	/* Only used to describe the protocol stack. */
	if (item->spec == NULL && item->mask == NULL)
		return 0;

	if (item->mask) {
		tcp_mask = item->mask;
		if (tcp_mask->hdr.sent_seq ||
		    tcp_mask->hdr.recv_ack ||
		    tcp_mask->hdr.data_off ||
		    tcp_mask->hdr.tcp_flags ||
		    tcp_mask->hdr.rx_win ||
		    tcp_mask->hdr.cksum || tcp_mask->hdr.tcp_urp) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
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

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

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
						  RTE_FLOW_ERROR_TYPE_ITEM,
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

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");

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
						  RTE_FLOW_ERROR_TYPE_ITEM,
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
 * Check items before tunnel, save inner configs to outer configs,and clear
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

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");
	else if (item->spec && (item->mask == NULL))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Tunnel packets must configure with mask");

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

	if (vxlan_mask->flags)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Flags is not supported in VxLAN");

	/* VNI must be totally masked or not. */
	if (memcmp(vxlan_mask->vni, full_mask, VNI_OR_TNI_LEN) &&
	    memcmp(vxlan_mask->vni, zero_mask, VNI_OR_TNI_LEN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VNI must be totally masked or not in VxLAN");
	if (vxlan_mask->vni[0]) {
		hns3_set_bit(rule->input_set, OUTER_TUN_VNI, 1);
		memcpy(rule->key_conf.mask.outer_tun_vni, vxlan_mask->vni,
			   VNI_OR_TNI_LEN);
	}
	memcpy(rule->key_conf.spec.outer_tun_vni, vxlan_spec->vni,
		   VNI_OR_TNI_LEN);
	return 0;
}

static int
hns3_parse_nvgre(const struct rte_flow_item *item, struct hns3_fdir_rule *rule,
		 struct rte_flow_error *error)
{
	const struct rte_flow_item_nvgre *nvgre_spec;
	const struct rte_flow_item_nvgre *nvgre_mask;

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");
	else if (item->spec && (item->mask == NULL))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Tunnel packets must configure with mask");

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
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Ver/protocal is not supported in NVGRE");

	/* TNI must be totally masked or not. */
	if (memcmp(nvgre_mask->tni, full_mask, VNI_OR_TNI_LEN) &&
	    memcmp(nvgre_mask->tni, zero_mask, VNI_OR_TNI_LEN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
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

	if (item->spec == NULL && item->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Can't configure FDIR with mask but without spec");
	else if (item->spec && (item->mask == NULL))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Tunnel packets must configure with mask");

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
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Ver/protocal is not supported in GENEVE");
	/* VNI must be totally masked or not. */
	if (memcmp(geneve_mask->vni, full_mask, VNI_OR_TNI_LEN) &&
	    memcmp(geneve_mask->vni, zero_mask, VNI_OR_TNI_LEN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
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
					  RTE_FLOW_ERROR_TYPE_HANDLE,
					  NULL, "Unsupported tunnel type!");
	}
	if (ret)
		return ret;
	return hns3_handle_tunnel(item, rule, error);
}

static int
hns3_parse_normal(const struct rte_flow_item *item,
		  struct hns3_fdir_rule *rule,
		  struct items_step_mngr *step_mngr,
		  struct rte_flow_error *error)
{
	int ret;

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		ret = hns3_parse_eth(item, rule, error);
		step_mngr->items = L2_next_items;
		step_mngr->count = ARRAY_SIZE(L2_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		ret = hns3_parse_vlan(item, rule, error);
		step_mngr->items = L2_next_items;
		step_mngr->count = ARRAY_SIZE(L2_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		ret = hns3_parse_ipv4(item, rule, error);
		step_mngr->items = L3_next_items;
		step_mngr->count = ARRAY_SIZE(L3_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		ret = hns3_parse_ipv6(item, rule, error);
		step_mngr->items = L3_next_items;
		step_mngr->count = ARRAY_SIZE(L3_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		ret = hns3_parse_tcp(item, rule, error);
		step_mngr->items = L4_next_items;
		step_mngr->count = ARRAY_SIZE(L4_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		ret = hns3_parse_udp(item, rule, error);
		step_mngr->items = L4_next_items;
		step_mngr->count = ARRAY_SIZE(L4_next_items);
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		ret = hns3_parse_sctp(item, rule, error);
		step_mngr->items = L4_next_items;
		step_mngr->count = ARRAY_SIZE(L4_next_items);
		break;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_HANDLE,
					  NULL, "Unsupported normal type!");
	}

	return ret;
}

static int
hns3_validate_item(const struct rte_flow_item *item,
		   struct items_step_mngr step_mngr,
		   struct rte_flow_error *error)
{
	int i;

	if (item->last)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, item,
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
	    type == RTE_FLOW_ITEM_TYPE_GENEVE ||
	    type == RTE_FLOW_ITEM_TYPE_MPLS)
		return true;
	return false;
}

/*
 * Parse the rule to see if it is a IP or MAC VLAN flow director rule.
 * And get the flow director filter info BTW.
 * UDP/TCP/SCTP PATTERN:
 * The first not void item can be ETH or IPV4 or IPV6
 * The second not void item must be IPV4 or IPV6 if the first one is ETH.
 * The next not void item could be UDP or TCP or SCTP (optional)
 * The next not void item could be RAW (for flexbyte, optional)
 * The next not void item must be END.
 * A Fuzzy Match pattern can appear at any place before END.
 * Fuzzy Match is optional for IPV4 but is required for IPV6
 * MAC VLAN PATTERN:
 * The first not void item must be ETH.
 * The second not void item must be MAC VLAN.
 * The next not void item must be END.
 * ACTION:
 * The first not void action should be QUEUE or DROP.
 * The second not void optional action should be MARK,
 * mark_id is a uint32_t number.
 * The next not void action should be END.
 * UDP/TCP/SCTP pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4		src_addr 192.168.1.20	0xFFFFFFFF
 *		dst_addr 192.167.3.50	0xFFFFFFFF
 * UDP/TCP/SCTP	src_port	80	0xFFFF
 *		dst_port	80	0xFFFF
 * END
 * MAC VLAN pattern example:
 * ITEM		Spec			Mask
 * ETH		dst_addr
		{0xAC, 0x7B, 0xA1,	{0xFF, 0xFF, 0xFF,
		0x2C, 0x6D, 0x36}	0xFF, 0xFF, 0xFF}
 * MAC VLAN	tci	0x2016		0xEFFF
 * END
 * Other members in mask and spec should set to 0x00.
 * Item->last should be NULL.
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

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_PERFECT)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_NUM, NULL,
					  "fdir_conf.mode isn't perfect");

	step_mngr.items = first_items;
	step_mngr.count = ARRAY_SIZE(first_items);
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
			step_mngr.count = ARRAY_SIZE(tunnel_next_items);
		} else {
			ret = hns3_parse_normal(item, rule, &step_mngr, error);
			if (ret)
				return ret;
		}
	}

	return hns3_handle_actions(dev, actions, rule, error);
}

void
hns3_filterlist_init(struct rte_eth_dev *dev)
{
	struct hns3_process_private *process_list = dev->process_private;

	TAILQ_INIT(&process_list->fdir_list);
	TAILQ_INIT(&process_list->filter_rss_list);
	TAILQ_INIT(&process_list->flow_list);
}

static void
hns3_filterlist_flush(struct rte_eth_dev *dev)
{
	struct hns3_process_private *process_list = dev->process_private;
	struct hns3_fdir_rule_ele *fdir_rule_ptr;
	struct hns3_rss_conf_ele *rss_filter_ptr;
	struct hns3_flow_mem *flow_node;

	fdir_rule_ptr = TAILQ_FIRST(&process_list->fdir_list);
	while (fdir_rule_ptr) {
		TAILQ_REMOVE(&process_list->fdir_list, fdir_rule_ptr, entries);
		rte_free(fdir_rule_ptr);
		fdir_rule_ptr = TAILQ_FIRST(&process_list->fdir_list);
	}

	rss_filter_ptr = TAILQ_FIRST(&process_list->filter_rss_list);
	while (rss_filter_ptr) {
		TAILQ_REMOVE(&process_list->filter_rss_list, rss_filter_ptr,
			     entries);
		rte_free(rss_filter_ptr);
		rss_filter_ptr = TAILQ_FIRST(&process_list->filter_rss_list);
	}

	flow_node = TAILQ_FIRST(&process_list->flow_list);
	while (flow_node) {
		TAILQ_REMOVE(&process_list->flow_list, flow_node, entries);
		rte_free(flow_node->flow);
		rte_free(flow_node);
		flow_node = TAILQ_FIRST(&process_list->flow_list);
	}
}

static bool
hns3_action_rss_same(const struct rte_flow_action_rss *comp,
		     const struct rte_flow_action_rss *with)
{
	return (comp->func == with->func &&
		comp->level == with->level &&
		comp->types == with->types &&
		comp->key_len == with->key_len &&
		comp->queue_num == with->queue_num &&
		!memcmp(comp->key, with->key, with->key_len) &&
		!memcmp(comp->queue, with->queue,
			sizeof(*with->queue) * with->queue_num));
}

static int
hns3_rss_conf_copy(struct hns3_rss_conf *out,
		   const struct rte_flow_action_rss *in)
{
	if (in->key_len > RTE_DIM(out->key) ||
	    in->queue_num > RTE_DIM(out->queue))
		return -EINVAL;
	if (in->key == NULL && in->key_len)
		return -EINVAL;
	out->conf = (struct rte_flow_action_rss) {
		.func = in->func,
		.level = in->level,
		.types = in->types,
		.key_len = in->key_len,
		.queue_num = in->queue_num,
	};
	out->conf.queue =
		memcpy(out->queue, in->queue,
		       sizeof(*in->queue) * in->queue_num);
	if (in->key)
		out->conf.key = memcpy(out->key, in->key, in->key_len);

	return 0;
}

/*
 * This function is used to parse rss action validatation.
 */
static int
hns3_parse_rss_filter(struct rte_eth_dev *dev,
		      const struct rte_flow_action *actions,
		      struct rte_flow_error *error)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_conf = &hw->rss_info;
	const struct rte_flow_action_rss *rss;
	const struct rte_flow_action *act;
	uint32_t act_index = 0;
	uint64_t flow_types;
	uint16_t n;

	NEXT_ITEM_OF_ACTION(act, actions, act_index);
	/* Get configuration args from APP cmdline input */
	rss = act->conf;

	if (rss == NULL || rss->queue_num == 0) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act, "no valid queues");
	}

	for (n = 0; n < rss->queue_num; n++) {
		if (rss->queue[n] < dev->data->nb_rx_queues)
			continue;
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act,
					  "queue id > max number of queues");
	}

	/* Parse flow types of RSS */
	if (!(rss->types & HNS3_ETH_RSS_SUPPORT) && rss->types)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act,
					  "Flow types is unsupported by "
					  "hns3's RSS");

	flow_types = rss->types & HNS3_ETH_RSS_SUPPORT;
	if (flow_types != rss->types)
		hns3_warn(hw, "RSS flow types(%" PRIx64 ") include unsupported "
			  "flow types", rss->types);

	/* Parse RSS related parameters from RSS configuration */
	switch (rss->func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
		break;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, act,
					  "input RSS hash functions are not supported");
	}

	if (rss->level)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, act,
					  "a nonzero RSS encapsulation level is not supported");
	if (rss->key_len && rss->key_len != RTE_DIM(rss_conf->key))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, act,
					  "RSS hash key must be exactly 40 bytes");
	if (rss->queue_num > RTE_DIM(rss_conf->queue))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, act,
					  "too many queues for RSS context");

	act_index++;

	/* Check if the next not void action is END */
	NEXT_ITEM_OF_ACTION(act, actions, act_index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(rss_conf, 0, sizeof(struct hns3_rss_conf));
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  act, "Not supported action.");
	}

	return 0;
}

static int
hns3_disable_rss(struct hns3_hw *hw)
{
	int ret;

	/* Redirected the redirection table to queue 0 */
	ret = hns3_rss_reset_indir_table(hw);
	if (ret)
		return ret;

	/* Disable RSS */
	hw->rss_info.conf.types = 0;
	hw->rss_dis_flag = true;

	return 0;
}

static void
hns3_parse_rss_key(struct hns3_hw *hw, struct rte_flow_action_rss *rss_conf)
{
	if (rss_conf->key == NULL ||
	    rss_conf->key_len < HNS3_RSS_KEY_SIZE) {
		hns3_info(hw, "Default RSS hash key to be set");
		rss_conf->key = hns3_hash_key;
		rss_conf->key_len = HNS3_RSS_KEY_SIZE;
	}
}

static int
hns3_parse_rss_algorithm(struct hns3_hw *hw, enum rte_eth_hash_function *func,
			 uint8_t *hash_algo)
{
	enum rte_eth_hash_function algo_func = *func;
	switch (algo_func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
		/* Keep *hash_algo as what it used to be */
		algo_func = hw->rss_info.conf.func;
		break;
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
		*hash_algo = HNS3_RSS_HASH_ALGO_TOEPLITZ;
		break;
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		*hash_algo = HNS3_RSS_HASH_ALGO_SIMPLE;
		break;
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
		*hash_algo = HNS3_RSS_HASH_ALGO_SYMMETRIC_TOEP;
		break;
	default:
		hns3_err(hw, "Invalid RSS algorithm configuration(%u)",
			 algo_func);
		return -EINVAL;
	}
	*func = algo_func;

	return 0;
}

static int
hns3_hw_rss_hash_set(struct hns3_hw *hw, struct rte_flow_action_rss *rss_config)
{
	struct hns3_rss_tuple_cfg *tuple;
	int ret;

	/* Parse hash key */
	hns3_parse_rss_key(hw, rss_config);

	/* Parse hash algorithm */
	ret = hns3_parse_rss_algorithm(hw, &rss_config->func,
				       &hw->rss_info.hash_algo);
	if (ret)
		return ret;

	ret = hns3_set_rss_algo_key(hw, rss_config->key);
	if (ret)
		return ret;

	/* Update algorithm of hw */
	hw->rss_info.conf.func = rss_config->func;

	/* Set flow type supported */
	tuple = &hw->rss_info.rss_tuple_sets;
	ret = hns3_set_rss_tuple_by_rss_hf(hw, tuple, rss_config->types);
	if (ret)
		hns3_err(hw, "Update RSS tuples by rss hf failed %d", ret);

	return ret;
}

static int
hns3_update_indir_table(struct rte_eth_dev *dev,
			const struct rte_flow_action_rss *conf, uint16_t num)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint8_t indir_tbl[HNS3_RSS_IND_TBL_SIZE];
	uint16_t j, allow_rss_queues;
	uint8_t queue_id;
	uint32_t i;

	if (num == 0) {
		hns3_err(hw, "No PF queues are configured to enable RSS");
		return -ENOTSUP;
	}

	allow_rss_queues = RTE_MIN(dev->data->nb_rx_queues, hw->rss_size_max);
	/* Fill in redirection table */
	memcpy(indir_tbl, hw->rss_info.rss_indirection_tbl,
	       HNS3_RSS_IND_TBL_SIZE);
	for (i = 0, j = 0; i < HNS3_RSS_IND_TBL_SIZE; i++, j++) {
		j %= num;
		if (conf->queue[j] >= allow_rss_queues) {
			hns3_err(hw, "Invalid queue id(%u) to be set in "
				     "redirection table, max number of rss "
				     "queues: %u", conf->queue[j],
				 allow_rss_queues);
			return -EINVAL;
		}
		queue_id = conf->queue[j];
		indir_tbl[i] = queue_id;
	}

	return hns3_set_rss_indir_table(hw, indir_tbl, HNS3_RSS_IND_TBL_SIZE);
}

static int
hns3_config_rss_filter(struct rte_eth_dev *dev,
		       const struct hns3_rss_conf *conf, bool add)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_info;
	uint64_t flow_types;
	uint16_t num;
	int ret;

	struct rte_flow_action_rss rss_flow_conf = {
		.func = conf->conf.func,
		.level = conf->conf.level,
		.types = conf->conf.types,
		.key_len = conf->conf.key_len,
		.queue_num = conf->conf.queue_num,
		.key = conf->conf.key_len ?
		    (void *)(uintptr_t)conf->conf.key : NULL,
		.queue = conf->conf.queue,
	};

	/* The types is Unsupported by hns3' RSS */
	if (!(rss_flow_conf.types & HNS3_ETH_RSS_SUPPORT) &&
	    rss_flow_conf.types) {
		hns3_err(hw,
			 "Flow types(%" PRIx64 ") is unsupported by hns3's RSS",
			 rss_flow_conf.types);
		return -EINVAL;
	}

	if (rss_flow_conf.key_len &&
	    rss_flow_conf.key_len > RTE_DIM(rss_info->key)) {
		hns3_err(hw,
			"input hash key(%u) greater than supported len(%zu)",
			rss_flow_conf.key_len, RTE_DIM(rss_info->key));
		return -EINVAL;
	}

	/* Filter the unsupported flow types */
	flow_types = conf->conf.types ?
		     rss_flow_conf.types & HNS3_ETH_RSS_SUPPORT :
		     hw->rss_info.conf.types;
	if (flow_types != rss_flow_conf.types)
		hns3_warn(hw, "modified RSS types based on hardware support, "
			      "requested:%" PRIx64 " configured:%" PRIx64,
			  rss_flow_conf.types, flow_types);
	/* Update the useful flow types */
	rss_flow_conf.types = flow_types;

	rss_info = &hw->rss_info;
	if (!add) {
		if (hns3_action_rss_same(&rss_info->conf, &rss_flow_conf)) {
			ret = hns3_disable_rss(hw);
			if (ret) {
				hns3_err(hw, "RSS disable failed(%d)", ret);
				return ret;
			}
			memset(rss_info, 0, sizeof(struct hns3_rss_conf));
			return 0;
		}
		return -EINVAL;
	}

	/* Get rx queues num */
	num = dev->data->nb_rx_queues;

	/* Set rx queues to use */
	num = RTE_MIN(num, rss_flow_conf.queue_num);
	if (rss_flow_conf.queue_num > num)
		hns3_warn(hw, "Config queue numbers %u are beyond the scope of truncated",
			  rss_flow_conf.queue_num);
	hns3_info(hw, "Max of contiguous %u PF queues are configured", num);

	rte_spinlock_lock(&hw->lock);
	/* Update redirection talbe of rss */
	ret = hns3_update_indir_table(dev, &rss_flow_conf, num);
	if (ret)
		goto rss_config_err;

	/* Set hash algorithm and flow types by the user's config */
	ret = hns3_hw_rss_hash_set(hw, &rss_flow_conf);
	if (ret)
		goto rss_config_err;

	ret = hns3_rss_conf_copy(rss_info, &rss_flow_conf);
	if (ret) {
		hns3_err(hw, "RSS config init fail(%d)", ret);
		goto rss_config_err;
	}

rss_config_err:
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

/* Remove the rss filter */
static int
hns3_clear_rss_filter(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	if (hw->rss_info.conf.queue_num == 0)
		return 0;

	return hns3_config_rss_filter(dev, &hw->rss_info, false);
}

/* Restore the rss filter */
int
hns3_restore_rss_filter(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	if (hw->rss_info.conf.queue_num == 0)
		return 0;

	return hns3_config_rss_filter(dev, &hw->rss_info, true);
}

static int
hns3_flow_parse_rss(struct rte_eth_dev *dev,
		    const struct hns3_rss_conf *conf, bool add)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	bool ret;

	/* Action rss same */
	ret = hns3_action_rss_same(&hw->rss_info.conf, &conf->conf);
	if (ret) {
		hns3_err(hw, "Enter duplicate RSS configuration : %d", ret);
		return -EINVAL;
	}

	return hns3_config_rss_filter(dev, conf, add);
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
 * It only checkes the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int
hns3_flow_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct hns3_fdir_rule fdir_rule;
	int ret;

	ret = hns3_flow_args_check(attr, pattern, actions, error);
	if (ret)
		return ret;

	if (find_rss_action(actions))
		return hns3_parse_rss_filter(dev, actions, error);

	memset(&fdir_rule, 0, sizeof(struct hns3_fdir_rule));
	return hns3_parse_fdir_filter(dev, pattern, actions, &fdir_rule, error);
}

/*
 * Create or destroy a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hitt first.
 * So, the sequence matters.
 */
static struct rte_flow *
hns3_flow_create(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct hns3_process_private *process_list = dev->process_private;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	const struct hns3_rss_conf *rss_conf;
	struct hns3_fdir_rule_ele *fdir_rule_ptr;
	struct hns3_rss_conf_ele *rss_filter_ptr;
	struct hns3_flow_mem *flow_node;
	const struct rte_flow_action *act;
	struct rte_flow *flow;
	struct hns3_fdir_rule fdir_rule;
	int ret;

	ret = hns3_flow_args_check(attr, pattern, actions, error);
	if (ret)
		return NULL;

	flow = rte_zmalloc("hns3 flow", sizeof(struct rte_flow), 0);
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate flow memory");
		return NULL;
	}
	flow_node = rte_zmalloc("hns3 flow node",
				sizeof(struct hns3_flow_mem), 0);
	if (flow_node == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate flow list memory");
		rte_free(flow);
		return NULL;
	}

	flow_node->flow = flow;
	TAILQ_INSERT_TAIL(&process_list->flow_list, flow_node, entries);

	act = find_rss_action(actions);
	if (act) {
		rss_conf = act->conf;

		ret = hns3_flow_parse_rss(dev, rss_conf, true);
		if (ret)
			goto err;

		rss_filter_ptr = rte_zmalloc("hns3 rss filter",
					     sizeof(struct hns3_rss_conf_ele),
					     0);
		if (rss_filter_ptr == NULL) {
			hns3_err(hw,
				    "Failed to allocate hns3_rss_filter memory");
			ret = -ENOMEM;
			goto err;
		}
		memcpy(&rss_filter_ptr->filter_info, rss_conf,
			sizeof(struct hns3_rss_conf));
		TAILQ_INSERT_TAIL(&process_list->filter_rss_list,
				  rss_filter_ptr, entries);

		flow->rule = rss_filter_ptr;
		flow->filter_type = RTE_ETH_FILTER_HASH;
		return flow;
	}

	memset(&fdir_rule, 0, sizeof(struct hns3_fdir_rule));
	ret = hns3_parse_fdir_filter(dev, pattern, actions, &fdir_rule, error);
	if (ret)
		goto out;

	if (fdir_rule.flags & HNS3_RULE_FLAG_COUNTER) {
		ret = hns3_counter_new(dev, fdir_rule.act_cnt.shared,
				       fdir_rule.act_cnt.id, error);
		if (ret)
			goto out;

		flow->counter_id = fdir_rule.act_cnt.id;
	}
	ret = hns3_fdir_filter_program(hns, &fdir_rule, false);
	if (!ret) {
		fdir_rule_ptr = rte_zmalloc("hns3 fdir rule",
					    sizeof(struct hns3_fdir_rule_ele),
					    0);
		if (fdir_rule_ptr == NULL) {
			hns3_err(hw, "Failed to allocate fdir_rule memory");
			ret = -ENOMEM;
			goto err_fdir;
		}
		memcpy(&fdir_rule_ptr->fdir_conf, &fdir_rule,
			sizeof(struct hns3_fdir_rule));
		TAILQ_INSERT_TAIL(&process_list->fdir_list,
				  fdir_rule_ptr, entries);
		flow->rule = fdir_rule_ptr;
		flow->filter_type = RTE_ETH_FILTER_FDIR;

		return flow;
	}

err_fdir:
	if (fdir_rule.flags & HNS3_RULE_FLAG_COUNTER)
		hns3_counter_release(dev, fdir_rule.act_cnt.id);

err:
	rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow");
out:
	TAILQ_REMOVE(&process_list->flow_list, flow_node, entries);
	rte_free(flow_node);
	rte_free(flow);
	return NULL;
}

/* Destroy a flow rule on hns3. */
static int
hns3_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct hns3_process_private *process_list = dev->process_private;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_fdir_rule_ele *fdir_rule_ptr;
	struct hns3_rss_conf_ele *rss_filter_ptr;
	struct hns3_flow_mem *flow_node;
	struct hns3_hw *hw = &hns->hw;
	enum rte_filter_type filter_type;
	struct hns3_fdir_rule fdir_rule;
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
		TAILQ_REMOVE(&process_list->fdir_list, fdir_rule_ptr, entries);
		rte_free(fdir_rule_ptr);
		fdir_rule_ptr = NULL;
		break;
	case RTE_ETH_FILTER_HASH:
		rss_filter_ptr = (struct hns3_rss_conf_ele *)flow->rule;
		ret = hns3_config_rss_filter(dev, &hw->rss_info, false);
		if (ret)
			return rte_flow_error_set(error, EIO,
						  RTE_FLOW_ERROR_TYPE_HANDLE,
						  flow,
						  "Destroy RSS fail.Try again");
		TAILQ_REMOVE(&process_list->filter_rss_list, rss_filter_ptr,
			     entries);
		rte_free(rss_filter_ptr);
		rss_filter_ptr = NULL;
		break;
	default:
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_HANDLE, flow,
					  "Unsupported filter type");
	}

	TAILQ_FOREACH(flow_node, &process_list->flow_list, entries) {
		if (flow_node->flow == flow) {
			TAILQ_REMOVE(&process_list->flow_list, flow_node,
				     entries);
			rte_free(flow_node);
			flow_node = NULL;
			break;
		}
	}
	rte_free(flow);
	flow = NULL;

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
	struct rte_flow_query_count *qc;
	int ret;

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
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "Query action only support count");
		}
	}
	return 0;
}

static const struct rte_flow_ops hns3_flow_ops = {
	.validate = hns3_flow_validate,
	.create = hns3_flow_create,
	.destroy = hns3_flow_destroy,
	.flush = hns3_flow_flush,
	.query = hns3_flow_query,
	.isolate = NULL,
};

/*
 * The entry of flow API.
 * @param dev
 *   Pointer to Ethernet device.
 * @return
 *   0 on success, a negative errno value otherwise is set.
 */
int
hns3_dev_filter_ctrl(struct rte_eth_dev *dev, enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op, void *arg)
{
	struct hns3_hw *hw;
	int ret = 0;

	hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		if (hw->adapter_state >= HNS3_NIC_CLOSED)
			return -ENODEV;
		*(const void **)arg = &hns3_flow_ops;
		break;
	default:
		hns3_err(hw, "Filter type (%d) not supported", filter_type);
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
