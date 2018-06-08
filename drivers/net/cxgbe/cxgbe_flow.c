/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */
#include "common.h"
#include "cxgbe_flow.h"

#define __CXGBE_FILL_FS(__v, __m, fs, elem, e) \
do { \
	if (!((fs)->val.elem || (fs)->mask.elem)) { \
		(fs)->val.elem = (__v); \
		(fs)->mask.elem = (__m); \
	} else { \
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, \
					  NULL, "a filter can be specified" \
					  " only once"); \
	} \
} while (0)

#define __CXGBE_FILL_FS_MEMCPY(__v, __m, fs, elem) \
do { \
	memcpy(&(fs)->val.elem, &(__v), sizeof(__v)); \
	memcpy(&(fs)->mask.elem, &(__m), sizeof(__m)); \
} while (0)

#define CXGBE_FILL_FS(v, m, elem) \
	__CXGBE_FILL_FS(v, m, fs, elem, e)

#define CXGBE_FILL_FS_MEMCPY(v, m, elem) \
	__CXGBE_FILL_FS_MEMCPY(v, m, fs, elem)

static int
cxgbe_validate_item(const struct rte_flow_item *i, struct rte_flow_error *e)
{
	/* rte_flow specification does not allow it. */
	if (!i->spec && (i->mask ||  i->last))
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   i, "last or mask given without spec");
	/*
	 * We don't support it.
	 * Although, we can support values in last as 0's or last == spec.
	 * But this will not provide user with any additional functionality
	 * and will only increase the complexity for us.
	 */
	if (i->last)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				   i, "last is not supported by chelsio pmd");
	return 0;
}

static int
ch_rte_parsetype_udp(const void *dmask, const struct rte_flow_item *item,
		     struct ch_filter_specification *fs,
		     struct rte_flow_error *e)
{
	const struct rte_flow_item_udp *val = item->spec;
	const struct rte_flow_item_udp *umask = item->mask;
	const struct rte_flow_item_udp *mask;

	mask = umask ? umask : (const struct rte_flow_item_udp *)dmask;

	if (mask->hdr.dgram_len || mask->hdr.dgram_cksum)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "udp: only src/dst port supported");

	CXGBE_FILL_FS(IPPROTO_UDP, 0xff, proto);
	if (!val)
		return 0;
	CXGBE_FILL_FS(be16_to_cpu(val->hdr.src_port),
		      be16_to_cpu(mask->hdr.src_port), fport);
	CXGBE_FILL_FS(be16_to_cpu(val->hdr.dst_port),
		      be16_to_cpu(mask->hdr.dst_port), lport);
	return 0;
}

static int
ch_rte_parsetype_tcp(const void *dmask, const struct rte_flow_item *item,
		     struct ch_filter_specification *fs,
		     struct rte_flow_error *e)
{
	const struct rte_flow_item_tcp *val = item->spec;
	const struct rte_flow_item_tcp *umask = item->mask;
	const struct rte_flow_item_tcp *mask;

	mask = umask ? umask : (const struct rte_flow_item_tcp *)dmask;

	if (mask->hdr.sent_seq || mask->hdr.recv_ack || mask->hdr.data_off ||
	    mask->hdr.tcp_flags || mask->hdr.rx_win || mask->hdr.cksum ||
	    mask->hdr.tcp_urp)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "tcp: only src/dst port supported");

	CXGBE_FILL_FS(IPPROTO_TCP, 0xff, proto);
	if (!val)
		return 0;
	CXGBE_FILL_FS(be16_to_cpu(val->hdr.src_port),
		      be16_to_cpu(mask->hdr.src_port), fport);
	CXGBE_FILL_FS(be16_to_cpu(val->hdr.dst_port),
		      be16_to_cpu(mask->hdr.dst_port), lport);
	return 0;
}

static int
ch_rte_parsetype_ipv4(const void *dmask, const struct rte_flow_item *item,
		      struct ch_filter_specification *fs,
		      struct rte_flow_error *e)
{
	const struct rte_flow_item_ipv4 *val = item->spec;
	const struct rte_flow_item_ipv4 *umask = item->mask;
	const struct rte_flow_item_ipv4 *mask;

	mask = umask ? umask : (const struct rte_flow_item_ipv4 *)dmask;

	if (mask->hdr.time_to_live || mask->hdr.type_of_service)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item, "ttl/tos are not supported");

	fs->type = FILTER_TYPE_IPV4;
	CXGBE_FILL_FS(ETHER_TYPE_IPv4, 0xffff, ethtype);
	if (!val)
		return 0; /* ipv4 wild card */

	CXGBE_FILL_FS(val->hdr.next_proto_id, mask->hdr.next_proto_id, proto);
	CXGBE_FILL_FS_MEMCPY(val->hdr.dst_addr, mask->hdr.dst_addr, lip);
	CXGBE_FILL_FS_MEMCPY(val->hdr.src_addr, mask->hdr.src_addr, fip);

	return 0;
}

static int
ch_rte_parsetype_ipv6(const void *dmask, const struct rte_flow_item *item,
		      struct ch_filter_specification *fs,
		      struct rte_flow_error *e)
{
	const struct rte_flow_item_ipv6 *val = item->spec;
	const struct rte_flow_item_ipv6 *umask = item->mask;
	const struct rte_flow_item_ipv6 *mask;

	mask = umask ? umask : (const struct rte_flow_item_ipv6 *)dmask;

	if (mask->hdr.vtc_flow ||
	    mask->hdr.payload_len || mask->hdr.hop_limits)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "tc/flow/hop are not supported");

	fs->type = FILTER_TYPE_IPV6;
	CXGBE_FILL_FS(ETHER_TYPE_IPv6, 0xffff, ethtype);
	if (!val)
		return 0; /* ipv6 wild card */

	CXGBE_FILL_FS(val->hdr.proto, mask->hdr.proto, proto);
	CXGBE_FILL_FS_MEMCPY(val->hdr.dst_addr, mask->hdr.dst_addr, lip);
	CXGBE_FILL_FS_MEMCPY(val->hdr.src_addr, mask->hdr.src_addr, fip);

	return 0;
}

static int
cxgbe_rtef_parse_attr(struct rte_flow *flow, const struct rte_flow_attr *attr,
		      struct rte_flow_error *e)
{
	if (attr->egress)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR,
					  attr, "attribute:<egress> is"
					  " not supported !");
	if (attr->group > 0)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR,
					  attr, "group parameter is"
					  " not supported.");

	flow->fidx = attr->priority ? attr->priority - 1 : FILTER_ID_MAX;

	return 0;
}

static inline int check_rxq(struct rte_eth_dev *dev, uint16_t rxq)
{
	struct port_info *pi = ethdev2pinfo(dev);

	if (rxq > pi->n_rx_qsets)
		return -EINVAL;
	return 0;
}

static int cxgbe_validate_fidxondel(struct filter_entry *f, unsigned int fidx)
{
	struct adapter *adap = ethdev2adap(f->dev);
	struct ch_filter_specification fs = f->fs;

	if (fidx >= adap->tids.nftids) {
		dev_err(adap, "invalid flow index %d.\n", fidx);
		return -EINVAL;
	}
	if (!is_filter_set(&adap->tids, fidx, fs.type)) {
		dev_err(adap, "Already free fidx:%d f:%p\n", fidx, f);
		return -EINVAL;
	}

	return 0;
}

static int
cxgbe_validate_fidxonadd(struct ch_filter_specification *fs,
			 struct adapter *adap, unsigned int fidx)
{
	if (is_filter_set(&adap->tids, fidx, fs->type)) {
		dev_err(adap, "filter index: %d is busy.\n", fidx);
		return -EBUSY;
	}
	if (fidx >= adap->tids.nftids) {
		dev_err(adap, "filter index (%u) >= max(%u)\n",
			fidx, adap->tids.nftids);
		return -ERANGE;
	}

	return 0;
}

static int
cxgbe_verify_fidx(struct rte_flow *flow, unsigned int fidx, uint8_t del)
{
	return del ? cxgbe_validate_fidxondel(flow->f, fidx) :
		cxgbe_validate_fidxonadd(&flow->fs,
					 ethdev2adap(flow->dev), fidx);
}

static int cxgbe_get_fidx(struct rte_flow *flow, unsigned int *fidx)
{
	struct ch_filter_specification *fs = &flow->fs;
	struct adapter *adap = ethdev2adap(flow->dev);

	/* For tcam get the next available slot, if default value specified */
	if (flow->fidx == FILTER_ID_MAX) {
		int idx;

		idx = cxgbe_alloc_ftid(adap, fs->type);
		if (idx < 0) {
			dev_err(adap, "unable to get a filter index in tcam\n");
			return -ENOMEM;
		}
		*fidx = (unsigned int)idx;
	} else {
		*fidx = flow->fidx;
	}

	return 0;
}

static int
cxgbe_rtef_parse_actions(struct rte_flow *flow,
			 const struct rte_flow_action action[],
			 struct rte_flow_error *e)
{
	struct ch_filter_specification *fs = &flow->fs;
	const struct rte_flow_action_queue *q;
	const struct rte_flow_action *a;
	char abit = 0;

	for (a = action; a->type != RTE_FLOW_ACTION_TYPE_END; a++) {
		switch (a->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		case RTE_FLOW_ACTION_TYPE_DROP:
			if (abit++)
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, a,
						"specify only 1 pass/drop");
			fs->action = FILTER_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			q = (const struct rte_flow_action_queue *)a->conf;
			if (!q)
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, q,
						"specify rx queue index");
			if (check_rxq(flow->dev, q->index))
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, q,
						"Invalid rx queue");
			if (abit++)
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, a,
						"specify only 1 pass/drop");
			fs->action = FILTER_PASS;
			fs->dirsteer = 1;
			fs->iq = q->index;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			fs->hitcnts = 1;
			break;
		default:
			/* Not supported action : return error */
			return rte_flow_error_set(e, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  a, "Action not supported");
		}
	}

	return 0;
}

struct chrte_fparse parseitem[] = {
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.fptr  = ch_rte_parsetype_ipv4,
		.dmask = &rte_flow_item_ipv4_mask,
	},

	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.fptr  = ch_rte_parsetype_ipv6,
		.dmask = &rte_flow_item_ipv6_mask,
	},

	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.fptr  = ch_rte_parsetype_udp,
		.dmask = &rte_flow_item_udp_mask,
	},

	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.fptr  = ch_rte_parsetype_tcp,
		.dmask = &rte_flow_item_tcp_mask,
	},
};

static int
cxgbe_rtef_parse_items(struct rte_flow *flow,
		       const struct rte_flow_item items[],
		       struct rte_flow_error *e)
{
	const struct rte_flow_item *i;
	char repeat[ARRAY_SIZE(parseitem)] = {0};

	for (i = items; i->type != RTE_FLOW_ITEM_TYPE_END; i++) {
		struct chrte_fparse *idx = &flow->item_parser[i->type];
		int ret;

		if (i->type > ARRAY_SIZE(parseitem))
			return rte_flow_error_set(e, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  i, "Item not supported");

		switch (i->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			continue;
		default:
			/* check if item is repeated */
			if (repeat[i->type])
				return rte_flow_error_set(e, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ITEM, i,
						"parse items cannot be repeated (except void)");
			repeat[i->type] = 1;

			/* validate the item */
			ret = cxgbe_validate_item(i, e);
			if (ret)
				return ret;

			if (!idx || !idx->fptr) {
				return rte_flow_error_set(e, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ITEM, i,
						"Item not supported");
			} else {
				ret = idx->fptr(idx->dmask, i, &flow->fs, e);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

static int
cxgbe_flow_parse(struct rte_flow *flow,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item item[],
		 const struct rte_flow_action action[],
		 struct rte_flow_error *e)
{
	int ret;

	/* parse user request into ch_filter_specification */
	ret = cxgbe_rtef_parse_attr(flow, attr, e);
	if (ret)
		return ret;
	ret = cxgbe_rtef_parse_items(flow, item, e);
	if (ret)
		return ret;
	return cxgbe_rtef_parse_actions(flow, action, e);
}

static int
cxgbe_flow_validate(struct rte_eth_dev *dev,
		    const struct rte_flow_attr *attr,
		    const struct rte_flow_item item[],
		    const struct rte_flow_action action[],
		    struct rte_flow_error *e)
{
	struct adapter *adap = ethdev2adap(dev);
	struct rte_flow *flow;
	unsigned int fidx;
	int ret;

	flow = t4_os_alloc(sizeof(struct rte_flow));
	if (!flow)
		return rte_flow_error_set(e, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"Unable to allocate memory for filter_entry");

	flow->item_parser = parseitem;
	flow->dev = dev;

	ret = cxgbe_flow_parse(flow, attr, item, action, e);
	if (ret) {
		t4_os_free(flow);
		return ret;
	}

	if (validate_filter(adap, &flow->fs)) {
		t4_os_free(flow);
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"validation failed. Check f/w config file.");
	}

	if (cxgbe_get_fidx(flow, &fidx)) {
		t4_os_free(flow);
		return rte_flow_error_set(e, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
					  NULL, "no memory in tcam.");
	}

	if (cxgbe_verify_fidx(flow, fidx, 0)) {
		t4_os_free(flow);
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
					  NULL, "validation failed");
	}

	t4_os_free(flow);
	return 0;
}

static const struct rte_flow_ops cxgbe_flow_ops = {
	.validate	= cxgbe_flow_validate,
	.create		= NULL,
	.destroy	= NULL,
	.flush		= NULL,
	.query		= NULL,
	.isolate	= NULL,
};

int
cxgbe_dev_filter_ctrl(struct rte_eth_dev *dev,
		      enum rte_filter_type filter_type,
		      enum rte_filter_op filter_op,
		      void *arg)
{
	int ret = 0;

	RTE_SET_USED(dev);
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &cxgbe_flow_ops;
		break;
	default:
		ret = -ENOTSUP;
		break;
	}
	return ret;
}
