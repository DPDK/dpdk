/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */
#include "base/common.h"
#include "cxgbe_flow.h"

#define __CXGBE_FILL_FS(__v, __m, fs, elem, e) \
do { \
	if ((fs)->mask.elem && ((fs)->val.elem != (__v))) \
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, \
					  NULL, "Redefined match item with" \
					  " different values found"); \
	(fs)->val.elem = (__v); \
	(fs)->mask.elem = (__m); \
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

static void
cxgbe_fill_filter_region(struct adapter *adap,
			 struct ch_filter_specification *fs)
{
	struct tp_params *tp = &adap->params.tp;
	u64 hash_filter_mask = tp->hash_filter_mask;
	u64 ntuple_mask = 0;

	fs->cap = 0;

	if (!is_hashfilter(adap))
		return;

	if (fs->type) {
		uint8_t biton[16] = {0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff};
		uint8_t bitoff[16] = {0};

		if (!memcmp(fs->val.lip, bitoff, sizeof(bitoff)) ||
		    !memcmp(fs->val.fip, bitoff, sizeof(bitoff)) ||
		    memcmp(fs->mask.lip, biton, sizeof(biton)) ||
		    memcmp(fs->mask.fip, biton, sizeof(biton)))
			return;
	} else {
		uint32_t biton  = 0xffffffff;
		uint32_t bitoff = 0x0U;

		if (!memcmp(fs->val.lip, &bitoff, sizeof(bitoff)) ||
		    !memcmp(fs->val.fip, &bitoff, sizeof(bitoff)) ||
		    memcmp(fs->mask.lip, &biton, sizeof(biton)) ||
		    memcmp(fs->mask.fip, &biton, sizeof(biton)))
			return;
	}

	if (!fs->val.lport || fs->mask.lport != 0xffff)
		return;
	if (!fs->val.fport || fs->mask.fport != 0xffff)
		return;

	if (tp->protocol_shift >= 0)
		ntuple_mask |= (u64)fs->mask.proto << tp->protocol_shift;
	if (tp->ethertype_shift >= 0)
		ntuple_mask |= (u64)fs->mask.ethtype << tp->ethertype_shift;
	if (tp->port_shift >= 0)
		ntuple_mask |= (u64)fs->mask.iport << tp->port_shift;
	if (tp->macmatch_shift >= 0)
		ntuple_mask |= (u64)fs->mask.macidx << tp->macmatch_shift;

	if (ntuple_mask != hash_filter_mask)
		return;

	fs->cap = 1;	/* use hash region */
}

static int
ch_rte_parsetype_eth(const void *dmask, const struct rte_flow_item *item,
		     struct ch_filter_specification *fs,
		     struct rte_flow_error *e)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *umask = item->mask;
	const struct rte_flow_item_eth *mask;

	/* If user has not given any mask, then use chelsio supported mask. */
	mask = umask ? umask : (const struct rte_flow_item_eth *)dmask;

	/* we don't support SRC_MAC filtering*/
	if (!is_zero_ether_addr(&mask->src))
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "src mac filtering not supported");

	if (!is_zero_ether_addr(&mask->dst)) {
		const u8 *addr = (const u8 *)&spec->dst.addr_bytes[0];
		const u8 *m = (const u8 *)&mask->dst.addr_bytes[0];
		struct rte_flow *flow = (struct rte_flow *)fs->private;
		struct port_info *pi = (struct port_info *)
					(flow->dev->data->dev_private);
		int idx;

		idx = cxgbe_mpstcam_alloc(pi, addr, m);
		if (idx <= 0)
			return rte_flow_error_set(e, idx,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "unable to allocate mac"
						  " entry in h/w");
		CXGBE_FILL_FS(idx, 0x1ff, macidx);
	}

	CXGBE_FILL_FS(be16_to_cpu(spec->type),
		      be16_to_cpu(mask->type), ethtype);
	return 0;
}

static int
ch_rte_parsetype_port(const void *dmask, const struct rte_flow_item *item,
		      struct ch_filter_specification *fs,
		      struct rte_flow_error *e)
{
	const struct rte_flow_item_phy_port *val = item->spec;
	const struct rte_flow_item_phy_port *umask = item->mask;
	const struct rte_flow_item_phy_port *mask;

	mask = umask ? umask : (const struct rte_flow_item_phy_port *)dmask;

	if (val->index > 0x7)
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "port index upto 0x7 is supported");

	CXGBE_FILL_FS(val->index, mask->index, iport);

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
	if (flow->fs.cap)
		return 0; /* Hash filters */
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
cxgbe_get_flow_item_index(const struct rte_flow_item items[], u32 type)
{
	const struct rte_flow_item *i;
	int j, index = -ENOENT;

	for (i = items, j = 0; i->type != RTE_FLOW_ITEM_TYPE_END; i++, j++) {
		if (i->type == type) {
			index = j;
			break;
		}
	}

	return index;
}

static int
ch_rte_parse_nat(uint8_t nmode, struct ch_filter_specification *fs)
{
	/* nmode:
	 * BIT_0 = [src_ip],   BIT_1 = [dst_ip]
	 * BIT_2 = [src_port], BIT_3 = [dst_port]
	 *
	 * Only below cases are supported as per our spec.
	 */
	switch (nmode) {
	case 0:  /* 0000b */
		fs->nat_mode = NAT_MODE_NONE;
		break;
	case 2:  /* 0010b */
		fs->nat_mode = NAT_MODE_DIP;
		break;
	case 5:  /* 0101b */
		fs->nat_mode = NAT_MODE_SIP_SP;
		break;
	case 7:  /* 0111b */
		fs->nat_mode = NAT_MODE_DIP_SIP_SP;
		break;
	case 10: /* 1010b */
		fs->nat_mode = NAT_MODE_DIP_DP;
		break;
	case 11: /* 1011b */
		fs->nat_mode = NAT_MODE_DIP_DP_SIP;
		break;
	case 14: /* 1110b */
		fs->nat_mode = NAT_MODE_DIP_DP_SP;
		break;
	case 15: /* 1111b */
		fs->nat_mode = NAT_MODE_ALL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
ch_rte_parse_atype_switch(const struct rte_flow_action *a,
			  const struct rte_flow_item items[],
			  uint8_t *nmode,
			  struct ch_filter_specification *fs,
			  struct rte_flow_error *e)
{
	const struct rte_flow_action_of_set_vlan_vid *vlanid;
	const struct rte_flow_action_of_push_vlan *pushvlan;
	const struct rte_flow_action_set_ipv4 *ipv4;
	const struct rte_flow_action_set_ipv6 *ipv6;
	const struct rte_flow_action_set_tp *tp_port;
	const struct rte_flow_action_phy_port *port;
	int item_index;

	switch (a->type) {
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		vlanid = (const struct rte_flow_action_of_set_vlan_vid *)
			  a->conf;
		fs->newvlan = VLAN_REWRITE;
		fs->vlan = vlanid->vlan_vid;
		break;
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		pushvlan = (const struct rte_flow_action_of_push_vlan *)
			    a->conf;
		if (pushvlan->ethertype != ETHER_TYPE_VLAN)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "only ethertype 0x8100 "
						  "supported for push vlan.");
		fs->newvlan = VLAN_INSERT;
		break;
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
		fs->newvlan = VLAN_REMOVE;
		break;
	case RTE_FLOW_ACTION_TYPE_PHY_PORT:
		port = (const struct rte_flow_action_phy_port *)a->conf;
		fs->eport = port->index;
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_IPV4);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_IPV4 "
						  "found.");

		ipv4 = (const struct rte_flow_action_set_ipv4 *)a->conf;
		memcpy(fs->nat_fip, &ipv4->ipv4_addr, sizeof(ipv4->ipv4_addr));
		*nmode |= 1 << 0;
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_IPV4);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_IPV4 "
						  "found.");

		ipv4 = (const struct rte_flow_action_set_ipv4 *)a->conf;
		memcpy(fs->nat_lip, &ipv4->ipv4_addr, sizeof(ipv4->ipv4_addr));
		*nmode |= 1 << 1;
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_IPV6);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_IPV6 "
						  "found.");

		ipv6 = (const struct rte_flow_action_set_ipv6 *)a->conf;
		memcpy(fs->nat_fip, ipv6->ipv6_addr, sizeof(ipv6->ipv6_addr));
		*nmode |= 1 << 0;
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_IPV6);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_IPV6 "
						  "found.");

		ipv6 = (const struct rte_flow_action_set_ipv6 *)a->conf;
		memcpy(fs->nat_lip, ipv6->ipv6_addr, sizeof(ipv6->ipv6_addr));
		*nmode |= 1 << 1;
		break;
	case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_TCP);
		if (item_index < 0) {
			item_index =
				cxgbe_get_flow_item_index(items,
						RTE_FLOW_ITEM_TYPE_UDP);
			if (item_index < 0)
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, a,
						"No RTE_FLOW_ITEM_TYPE_TCP or "
						"RTE_FLOW_ITEM_TYPE_UDP found");
		}

		tp_port = (const struct rte_flow_action_set_tp *)a->conf;
		fs->nat_fport = be16_to_cpu(tp_port->port);
		*nmode |= 1 << 2;
		break;
	case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_TCP);
		if (item_index < 0) {
			item_index =
				cxgbe_get_flow_item_index(items,
						RTE_FLOW_ITEM_TYPE_UDP);
			if (item_index < 0)
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, a,
						"No RTE_FLOW_ITEM_TYPE_TCP or "
						"RTE_FLOW_ITEM_TYPE_UDP found");
		}

		tp_port = (const struct rte_flow_action_set_tp *)a->conf;
		fs->nat_lport = be16_to_cpu(tp_port->port);
		*nmode |= 1 << 3;
		break;
	case RTE_FLOW_ACTION_TYPE_MAC_SWAP:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_ETH);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_ETH "
						  "found");
		fs->swapmac = 1;
		break;
	default:
		/* We are not supposed to come here */
		return rte_flow_error_set(e, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, a,
					  "Action not supported");
	}

	return 0;
}

static int
cxgbe_rtef_parse_actions(struct rte_flow *flow,
			 const struct rte_flow_item items[],
			 const struct rte_flow_action action[],
			 struct rte_flow_error *e)
{
	struct ch_filter_specification *fs = &flow->fs;
	uint8_t nmode = 0, nat_ipv4 = 0, nat_ipv6 = 0;
	const struct rte_flow_action_queue *q;
	const struct rte_flow_action *a;
	char abit = 0;
	int ret;

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
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
		case RTE_FLOW_ACTION_TYPE_PHY_PORT:
		case RTE_FLOW_ACTION_TYPE_MAC_SWAP:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			nat_ipv4++;
			goto action_switch;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			nat_ipv6++;
			goto action_switch;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
action_switch:
			/* We allow multiple switch actions, but switch is
			 * not compatible with either queue or drop
			 */
			if (abit++ && fs->action != FILTER_SWITCH)
				return rte_flow_error_set(e, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, a,
						"overlapping action specified");
			if (nat_ipv4 && nat_ipv6)
				return rte_flow_error_set(e, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, a,
					"Can't have one address ipv4 and the"
					" other ipv6");

			ret = ch_rte_parse_atype_switch(a, items, &nmode, fs,
							e);
			if (ret)
				return ret;
			fs->action = FILTER_SWITCH;
			break;
		default:
			/* Not supported action : return error */
			return rte_flow_error_set(e, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  a, "Action not supported");
		}
	}

	if (ch_rte_parse_nat(nmode, fs))
		return rte_flow_error_set(e, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, a,
					  "invalid settings for swich action");
	return 0;
}

static struct chrte_fparse parseitem[] = {
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.fptr  = ch_rte_parsetype_eth,
		.dmask = &(const struct rte_flow_item_eth){
			.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0xffff,
		}
	},

	[RTE_FLOW_ITEM_TYPE_PHY_PORT] = {
		.fptr = ch_rte_parsetype_port,
		.dmask = &(const struct rte_flow_item_phy_port){
			.index = 0x7,
		}
	},

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
	struct adapter *adap = ethdev2adap(flow->dev);
	const struct rte_flow_item *i;
	char repeat[ARRAY_SIZE(parseitem)] = {0};

	for (i = items; i->type != RTE_FLOW_ITEM_TYPE_END; i++) {
		struct chrte_fparse *idx;
		int ret;

		if (i->type >= ARRAY_SIZE(parseitem))
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

			/* No spec found for this pattern item. Skip it */
			if (!i->spec)
				break;

			/* validate the item */
			ret = cxgbe_validate_item(i, e);
			if (ret)
				return ret;

			idx = &flow->item_parser[i->type];
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

	cxgbe_fill_filter_region(adap, &flow->fs);

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
	return cxgbe_rtef_parse_actions(flow, item, action, e);
}

static int __cxgbe_flow_create(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct ch_filter_specification *fs = &flow->fs;
	struct adapter *adap = ethdev2adap(dev);
	struct tid_info *t = &adap->tids;
	struct filter_ctx ctx;
	unsigned int fidx;
	int err;

	if (cxgbe_get_fidx(flow, &fidx))
		return -ENOMEM;
	if (cxgbe_verify_fidx(flow, fidx, 0))
		return -1;

	t4_init_completion(&ctx.completion);
	/* go create the filter */
	err = cxgbe_set_filter(dev, fidx, fs, &ctx);
	if (err) {
		dev_err(adap, "Error %d while creating filter.\n", err);
		return err;
	}

	/* Poll the FW for reply */
	err = cxgbe_poll_for_completion(&adap->sge.fw_evtq,
					CXGBE_FLOW_POLL_MS,
					CXGBE_FLOW_POLL_CNT,
					&ctx.completion);
	if (err) {
		dev_err(adap, "Filter set operation timed out (%d)\n", err);
		return err;
	}
	if (ctx.result) {
		dev_err(adap, "Hardware error %d while creating the filter.\n",
			ctx.result);
		return ctx.result;
	}

	if (fs->cap) { /* to destroy the filter */
		flow->fidx = ctx.tid;
		flow->f = lookup_tid(t, ctx.tid);
	} else {
		flow->fidx = fidx;
		flow->f = &adap->tids.ftid_tab[fidx];
	}

	return 0;
}

static struct rte_flow *
cxgbe_flow_create(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item item[],
		  const struct rte_flow_action action[],
		  struct rte_flow_error *e)
{
	struct rte_flow *flow;
	int ret;

	flow = t4_os_alloc(sizeof(struct rte_flow));
	if (!flow) {
		rte_flow_error_set(e, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Unable to allocate memory for"
				   " filter_entry");
		return NULL;
	}

	flow->item_parser = parseitem;
	flow->dev = dev;
	flow->fs.private = (void *)flow;

	if (cxgbe_flow_parse(flow, attr, item, action, e)) {
		t4_os_free(flow);
		return NULL;
	}

	/* go, interact with cxgbe_filter */
	ret = __cxgbe_flow_create(dev, flow);
	if (ret) {
		rte_flow_error_set(e, ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Unable to create flow rule");
		t4_os_free(flow);
		return NULL;
	}

	flow->f->private = flow; /* Will be used during flush */

	return flow;
}

static int __cxgbe_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct adapter *adap = ethdev2adap(dev);
	struct filter_entry *f = flow->f;
	struct ch_filter_specification *fs;
	struct filter_ctx ctx;
	int err;

	fs = &f->fs;
	if (cxgbe_verify_fidx(flow, flow->fidx, 1))
		return -1;

	t4_init_completion(&ctx.completion);
	err = cxgbe_del_filter(dev, flow->fidx, fs, &ctx);
	if (err) {
		dev_err(adap, "Error %d while deleting filter.\n", err);
		return err;
	}

	/* Poll the FW for reply */
	err = cxgbe_poll_for_completion(&adap->sge.fw_evtq,
					CXGBE_FLOW_POLL_MS,
					CXGBE_FLOW_POLL_CNT,
					&ctx.completion);
	if (err) {
		dev_err(adap, "Filter delete operation timed out (%d)\n", err);
		return err;
	}
	if (ctx.result) {
		dev_err(adap, "Hardware error %d while deleting the filter.\n",
			ctx.result);
		return ctx.result;
	}

	fs = &flow->fs;
	if (fs->mask.macidx) {
		struct port_info *pi = (struct port_info *)
					(dev->data->dev_private);
		int ret;

		ret = cxgbe_mpstcam_remove(pi, fs->val.macidx);
		if (!ret)
			return ret;
	}

	return 0;
}

static int
cxgbe_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		   struct rte_flow_error *e)
{
	int ret;

	ret = __cxgbe_flow_destroy(dev, flow);
	if (ret)
		return rte_flow_error_set(e, ret, RTE_FLOW_ERROR_TYPE_HANDLE,
					  flow, "error destroying filter.");
	t4_os_free(flow);
	return 0;
}

static int __cxgbe_flow_query(struct rte_flow *flow, u64 *count,
			      u64 *byte_count)
{
	struct adapter *adap = ethdev2adap(flow->dev);
	struct ch_filter_specification fs = flow->f->fs;
	unsigned int fidx = flow->fidx;
	int ret = 0;

	ret = cxgbe_get_filter_count(adap, fidx, count, fs.cap, 0);
	if (ret)
		return ret;
	return cxgbe_get_filter_count(adap, fidx, byte_count, fs.cap, 1);
}

static int
cxgbe_flow_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		 const struct rte_flow_action *action, void *data,
		 struct rte_flow_error *e)
{
	struct adapter *adap = ethdev2adap(flow->dev);
	struct ch_filter_specification fs;
	struct rte_flow_query_count *c;
	struct filter_entry *f;
	int ret;

	RTE_SET_USED(dev);

	f = flow->f;
	fs = f->fs;

	if (action->type != RTE_FLOW_ACTION_TYPE_COUNT)
		return rte_flow_error_set(e, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "only count supported for query");

	/*
	 * This is a valid operation, Since we are allowed to do chelsio
	 * specific operations in rte side of our code but not vise-versa
	 *
	 * So, fs can be queried/modified here BUT rte_flow_query_count
	 * cannot be worked on by the lower layer since we want to maintain
	 * it as rte_flow agnostic.
	 */
	if (!fs.hitcnts)
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					  &fs, "filter hit counters were not"
					  " enabled during filter creation");

	c = (struct rte_flow_query_count *)data;
	ret = __cxgbe_flow_query(flow, &c->hits, &c->bytes);
	if (ret)
		return rte_flow_error_set(e, -ret, RTE_FLOW_ERROR_TYPE_ACTION,
					  f, "cxgbe pmd failed to"
					  " perform query");

	/* Query was successful */
	c->bytes_set = 1;
	c->hits_set = 1;
	if (c->reset)
		cxgbe_clear_filter_count(adap, flow->fidx, f->fs.cap, true);

	return 0; /* success / partial_success */
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

/*
 * @ret : > 0 filter destroyed succsesfully
 *        < 0 error destroying filter
 *        == 1 filter not active / not found
 */
static int
cxgbe_check_n_destroy(struct filter_entry *f, struct rte_eth_dev *dev,
		      struct rte_flow_error *e)
{
	if (f && (f->valid || f->pending) &&
	    f->dev == dev && /* Only if user has asked for this port */
	     f->private) /* We (rte_flow) created this filter */
		return cxgbe_flow_destroy(dev, (struct rte_flow *)f->private,
					  e);
	return 1;
}

static int cxgbe_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *e)
{
	struct adapter *adap = ethdev2adap(dev);
	unsigned int i;
	int ret = 0;

	if (adap->tids.ftid_tab) {
		struct filter_entry *f = &adap->tids.ftid_tab[0];

		for (i = 0; i < adap->tids.nftids; i++, f++) {
			ret = cxgbe_check_n_destroy(f, dev, e);
			if (ret < 0)
				goto out;
		}
	}

	if (is_hashfilter(adap) && adap->tids.tid_tab) {
		struct filter_entry *f;

		for (i = adap->tids.hash_base; i <= adap->tids.ntids; i++) {
			f = (struct filter_entry *)adap->tids.tid_tab[i];

			ret = cxgbe_check_n_destroy(f, dev, e);
			if (ret < 0)
				goto out;
		}
	}

out:
	return ret >= 0 ? 0 : ret;
}

static const struct rte_flow_ops cxgbe_flow_ops = {
	.validate	= cxgbe_flow_validate,
	.create		= cxgbe_flow_create,
	.destroy	= cxgbe_flow_destroy,
	.flush		= cxgbe_flow_flush,
	.query		= cxgbe_flow_query,
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
