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

/**
 * Apart from the 4-tuple IPv4/IPv6 - TCP/UDP information,
 * there's only 40-bits available to store match fields.
 * So, to save space, optimize filter spec for some common
 * known fields that hardware can parse against incoming
 * packets automatically.
 */
static void
cxgbe_tweak_filter_spec(struct adapter *adap,
			struct ch_filter_specification *fs)
{
	/* Save 16-bit ethertype field space, by setting corresponding
	 * 1-bit flags in the filter spec for common known ethertypes.
	 * When hardware sees these flags, it automatically infers and
	 * matches incoming packets against the corresponding ethertype.
	 */
	if (fs->mask.ethtype == 0xffff) {
		switch (fs->val.ethtype) {
		case RTE_ETHER_TYPE_IPV4:
			if (adap->params.tp.ethertype_shift < 0) {
				fs->type = FILTER_TYPE_IPV4;
				fs->val.ethtype = 0;
				fs->mask.ethtype = 0;
			}
			break;
		case RTE_ETHER_TYPE_IPV6:
			if (adap->params.tp.ethertype_shift < 0) {
				fs->type = FILTER_TYPE_IPV6;
				fs->val.ethtype = 0;
				fs->mask.ethtype = 0;
			}
			break;
		case RTE_ETHER_TYPE_VLAN:
			if (adap->params.tp.ethertype_shift < 0 &&
			    adap->params.tp.vlan_shift >= 0) {
				fs->val.ivlan_vld = 1;
				fs->mask.ivlan_vld = 1;
				fs->val.ethtype = 0;
				fs->mask.ethtype = 0;
			}
			break;
		case RTE_ETHER_TYPE_QINQ:
			if (adap->params.tp.ethertype_shift < 0 &&
			    adap->params.tp.vnic_shift >= 0) {
				fs->val.ovlan_vld = 1;
				fs->mask.ovlan_vld = 1;
				fs->val.ethtype = 0;
				fs->mask.ethtype = 0;
			}
			break;
		default:
			break;
		}
	}
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
	if (tp->vlan_shift >= 0 && fs->mask.ivlan_vld)
		ntuple_mask |= (u64)(F_FT_VLAN_VLD | fs->mask.ivlan) <<
			       tp->vlan_shift;
	if (tp->vnic_shift >= 0) {
		if (fs->mask.ovlan_vld)
			ntuple_mask |= (u64)(fs->val.ovlan_vld << 16 |
					     fs->mask.ovlan) << tp->vnic_shift;
		else if (fs->mask.pfvf_vld)
			ntuple_mask |= (u64)(fs->mask.pfvf_vld << 16 |
					     fs->mask.pf << 13 |
					     fs->mask.vf) << tp->vnic_shift;
	}
	if (tp->tos_shift >= 0)
		ntuple_mask |= (u64)fs->mask.tos << tp->tos_shift;

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

	if (!spec)
		return 0;

	/* we don't support SRC_MAC filtering*/
	if (!rte_is_zero_ether_addr(&spec->hdr.src_addr) ||
	    (umask && !rte_is_zero_ether_addr(&umask->hdr.src_addr)))
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "src mac filtering not supported");

	if (!rte_is_zero_ether_addr(&spec->hdr.dst_addr) ||
	    (umask && !rte_is_zero_ether_addr(&umask->hdr.dst_addr))) {
		CXGBE_FILL_FS(0, 0x1ff, macidx);
		CXGBE_FILL_FS_MEMCPY(spec->hdr.dst_addr.addr_bytes, mask->hdr.dst_addr.addr_bytes,
				     dmac);
	}

	if (spec->hdr.ether_type || (umask && umask->hdr.ether_type))
		CXGBE_FILL_FS(be16_to_cpu(spec->hdr.ether_type),
			      be16_to_cpu(mask->hdr.ether_type), ethtype);

	return 0;
}

static int
ch_rte_parsetype_vlan(const void *dmask, const struct rte_flow_item *item,
		      struct ch_filter_specification *fs,
		      struct rte_flow_error *e)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *umask = item->mask;
	const struct rte_flow_item_vlan *mask;

	/* If user has not given any mask, then use chelsio supported mask. */
	mask = umask ? umask : (const struct rte_flow_item_vlan *)dmask;

	/* If ethertype is already set and is not VLAN (0x8100) or
	 * QINQ(0x88A8), then don't proceed further. Otherwise,
	 * reset the outer ethertype, so that it can be replaced by
	 * innermost ethertype. Note that hardware will automatically
	 * match against VLAN or QINQ packets, based on 'ivlan_vld' or
	 * 'ovlan_vld' bit set in Chelsio filter spec, respectively.
	 */
	if (fs->mask.ethtype) {
		if (fs->val.ethtype != RTE_ETHER_TYPE_VLAN &&
		    fs->val.ethtype != RTE_ETHER_TYPE_QINQ)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "Ethertype must be 0x8100 or 0x88a8");
	}

	if (fs->val.ethtype == RTE_ETHER_TYPE_QINQ) {
		CXGBE_FILL_FS(1, 1, ovlan_vld);
		if (spec) {
			if (spec->hdr.vlan_tci || (umask && umask->hdr.vlan_tci))
				CXGBE_FILL_FS(be16_to_cpu(spec->hdr.vlan_tci),
					      be16_to_cpu(mask->hdr.vlan_tci), ovlan);
			fs->mask.ethtype = 0;
			fs->val.ethtype = 0;
		}
	} else {
		CXGBE_FILL_FS(1, 1, ivlan_vld);
		if (spec) {
			if (spec->hdr.vlan_tci || (umask && umask->hdr.vlan_tci))
				CXGBE_FILL_FS(be16_to_cpu(spec->hdr.vlan_tci),
					      be16_to_cpu(mask->hdr.vlan_tci), ivlan);
			fs->mask.ethtype = 0;
			fs->val.ethtype = 0;
		}
	}

	if (spec && (spec->hdr.eth_proto || (umask && umask->hdr.eth_proto)))
		CXGBE_FILL_FS(be16_to_cpu(spec->hdr.eth_proto),
			      be16_to_cpu(mask->hdr.eth_proto), ethtype);

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

	if (val->hdr.src_port || (umask && umask->hdr.src_port))
		CXGBE_FILL_FS(be16_to_cpu(val->hdr.src_port),
			      be16_to_cpu(mask->hdr.src_port), fport);

	if (val->hdr.dst_port || (umask && umask->hdr.dst_port))
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

	if (val->hdr.src_port || (umask && umask->hdr.src_port))
		CXGBE_FILL_FS(be16_to_cpu(val->hdr.src_port),
			      be16_to_cpu(mask->hdr.src_port), fport);

	if (val->hdr.dst_port || (umask && umask->hdr.dst_port))
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

	if (mask->hdr.time_to_live)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item, "ttl is not supported");

	if (fs->mask.ethtype &&
	    (fs->val.ethtype != RTE_ETHER_TYPE_IPV4))
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "Couldn't find IPv4 ethertype");
	fs->type = FILTER_TYPE_IPV4;
	if (!val)
		return 0; /* ipv4 wild card */

	if (val->hdr.next_proto_id || (umask && umask->hdr.next_proto_id))
		CXGBE_FILL_FS(val->hdr.next_proto_id, mask->hdr.next_proto_id,
			      proto);

	if (val->hdr.dst_addr || (umask && umask->hdr.dst_addr))
		CXGBE_FILL_FS_MEMCPY(val->hdr.dst_addr, mask->hdr.dst_addr,
				     lip);

	if (val->hdr.src_addr || (umask && umask->hdr.src_addr))
		CXGBE_FILL_FS_MEMCPY(val->hdr.src_addr, mask->hdr.src_addr,
				     fip);

	if (val->hdr.type_of_service || (umask && umask->hdr.type_of_service))
		CXGBE_FILL_FS(val->hdr.type_of_service,
			      mask->hdr.type_of_service, tos);

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
	u32 vtc_flow, vtc_flow_mask;
	u8 z[16] = { 0 };

	mask = umask ? umask : (const struct rte_flow_item_ipv6 *)dmask;

	vtc_flow_mask = be32_to_cpu(mask->hdr.vtc_flow);

	if (vtc_flow_mask & RTE_IPV6_HDR_FL_MASK ||
	    mask->hdr.payload_len || mask->hdr.hop_limits)
		return rte_flow_error_set(e, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "flow/hop are not supported");

	if (fs->mask.ethtype &&
	    (fs->val.ethtype != RTE_ETHER_TYPE_IPV6))
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "Couldn't find IPv6 ethertype");
	fs->type = FILTER_TYPE_IPV6;
	if (!val)
		return 0; /* ipv6 wild card */

	if (val->hdr.proto || (umask && umask->hdr.proto))
		CXGBE_FILL_FS(val->hdr.proto, mask->hdr.proto, proto);

	vtc_flow = be32_to_cpu(val->hdr.vtc_flow);
	if (val->hdr.vtc_flow || (umask && umask->hdr.vtc_flow))
		CXGBE_FILL_FS((vtc_flow & RTE_IPV6_HDR_TC_MASK) >>
			      RTE_IPV6_HDR_TC_SHIFT,
			      (vtc_flow_mask & RTE_IPV6_HDR_TC_MASK) >>
			      RTE_IPV6_HDR_TC_SHIFT,
			      tos);

	if (memcmp(val->hdr.dst_addr, z, sizeof(val->hdr.dst_addr)) ||
	    (umask &&
	     memcmp(umask->hdr.dst_addr, z, sizeof(umask->hdr.dst_addr))))
		CXGBE_FILL_FS_MEMCPY(val->hdr.dst_addr, mask->hdr.dst_addr,
				     lip);

	if (memcmp(val->hdr.src_addr, z, sizeof(val->hdr.src_addr)) ||
	    (umask &&
	     memcmp(umask->hdr.src_addr, z, sizeof(umask->hdr.src_addr))))
		CXGBE_FILL_FS_MEMCPY(val->hdr.src_addr, mask->hdr.src_addr,
				     fip);

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
	u8 nentries;

	if (fidx >= adap->tids.nftids) {
		dev_err(adap, "invalid flow index %d.\n", fidx);
		return -EINVAL;
	}

	nentries = cxgbe_filter_slots(adap, fs.type);
	if (!cxgbe_is_filter_set(&adap->tids, fidx, nentries)) {
		dev_err(adap, "Already free fidx:%d f:%p\n", fidx, f);
		return -EINVAL;
	}

	return 0;
}

static int
cxgbe_validate_fidxonadd(struct ch_filter_specification *fs,
			 struct adapter *adap, unsigned int fidx)
{
	u8 nentries;

	nentries = cxgbe_filter_slots(adap, fs->type);
	if (cxgbe_is_filter_set(&adap->tids, fidx, nentries)) {
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
		u8 nentries;
		int idx;

		nentries = cxgbe_filter_slots(adap, fs->type);
		idx = cxgbe_alloc_ftid(adap, nentries);
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
	const struct rte_flow_action_of_set_vlan_pcp *vlanpcp;
	const struct rte_flow_action_of_push_vlan *pushvlan;
	const struct rte_flow_action_set_ipv4 *ipv4;
	const struct rte_flow_action_set_ipv6 *ipv6;
	const struct rte_flow_action_set_tp *tp_port;
	const struct rte_flow_action_set_mac *mac;
	int item_index;
	u16 tmp_vlan;

	switch (a->type) {
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		vlanid = (const struct rte_flow_action_of_set_vlan_vid *)
			  a->conf;
		/* If explicitly asked to push a new VLAN header,
		 * then don't set rewrite mode. Otherwise, the
		 * incoming VLAN packets will get their VLAN fields
		 * rewritten, instead of adding an additional outer
		 * VLAN header.
		 */
		if (fs->newvlan != VLAN_INSERT)
			fs->newvlan = VLAN_REWRITE;
		tmp_vlan = fs->vlan & 0xe000;
		fs->vlan = (be16_to_cpu(vlanid->vlan_vid) & 0xfff) | tmp_vlan;
		break;
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
		vlanpcp = (const struct rte_flow_action_of_set_vlan_pcp *)
			  a->conf;
		/* If explicitly asked to push a new VLAN header,
		 * then don't set rewrite mode. Otherwise, the
		 * incoming VLAN packets will get their VLAN fields
		 * rewritten, instead of adding an additional outer
		 * VLAN header.
		 */
		if (fs->newvlan != VLAN_INSERT)
			fs->newvlan = VLAN_REWRITE;
		tmp_vlan = fs->vlan & 0xfff;
		fs->vlan = (vlanpcp->vlan_pcp << 13) | tmp_vlan;
		break;
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		pushvlan = (const struct rte_flow_action_of_push_vlan *)
			    a->conf;
		if (be16_to_cpu(pushvlan->ethertype) != RTE_ETHER_TYPE_VLAN)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "only ethertype 0x8100 "
						  "supported for push vlan.");
		fs->newvlan = VLAN_INSERT;
		break;
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
		fs->newvlan = VLAN_REMOVE;
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
	case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_ETH);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_ETH "
						  "found");
		mac = (const struct rte_flow_action_set_mac *)a->conf;

		fs->newsmac = 1;
		memcpy(fs->smac, mac->mac_addr, sizeof(fs->smac));
		break;
	case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
		item_index = cxgbe_get_flow_item_index(items,
						       RTE_FLOW_ITEM_TYPE_ETH);
		if (item_index < 0)
			return rte_flow_error_set(e, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, a,
						  "No RTE_FLOW_ITEM_TYPE_ETH found");
		mac = (const struct rte_flow_action_set_mac *)a->conf;

		fs->newdmac = 1;
		memcpy(fs->dmac, mac->mac_addr, sizeof(fs->dmac));
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
	uint8_t vlan_set_vid = 0, vlan_set_pcp = 0;
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
			vlan_set_vid++;
			goto action_switch;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			vlan_set_pcp++;
			goto action_switch;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
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
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
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

	if (fs->newvlan == VLAN_REWRITE && (!vlan_set_vid || !vlan_set_pcp))
		return rte_flow_error_set(e, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, a,
					  "Both OF_SET_VLAN_VID and "
					  "OF_SET_VLAN_PCP must be specified");

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
			.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
			.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.hdr.ether_type = 0xffff,
		}
	},

	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.fptr = ch_rte_parsetype_vlan,
		.dmask = &(const struct rte_flow_item_vlan){
			.hdr.vlan_tci = 0xffff,
			.hdr.eth_proto = 0xffff,
		}
	},

	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.fptr  = ch_rte_parsetype_ipv4,
		.dmask = &(const struct rte_flow_item_ipv4) {
			.hdr = {
				.src_addr = RTE_BE32(0xffffffff),
				.dst_addr = RTE_BE32(0xffffffff),
				.type_of_service = 0xff,
			},
		},
	},

	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.fptr  = ch_rte_parsetype_ipv6,
		.dmask = &(const struct rte_flow_item_ipv6) {
			.hdr = {
				.src_addr =
					"\xff\xff\xff\xff\xff\xff\xff\xff"
					"\xff\xff\xff\xff\xff\xff\xff\xff",
				.dst_addr =
					"\xff\xff\xff\xff\xff\xff\xff\xff"
					"\xff\xff\xff\xff\xff\xff\xff\xff",
				.vtc_flow = RTE_BE32(0xff000000),
			},
		},
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
			if (repeat[i->type] &&
			    i->type != RTE_FLOW_ITEM_TYPE_VLAN)
				return rte_flow_error_set(e, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ITEM, i,
						"parse items cannot be repeated(except void/vlan)");

			repeat[i->type] = 1;

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

	cxgbe_tweak_filter_spec(adap, &flow->fs);
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
	struct adapter *adap = ethdev2adap(dev);
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

	t4_os_lock(&adap->flow_lock);
	/* go, interact with cxgbe_filter */
	ret = __cxgbe_flow_create(dev, flow);
	t4_os_unlock(&adap->flow_lock);
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

	return 0;
}

static int
cxgbe_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		   struct rte_flow_error *e)
{
	struct adapter *adap = ethdev2adap(dev);
	int ret;

	t4_os_lock(&adap->flow_lock);
	ret = __cxgbe_flow_destroy(dev, flow);
	t4_os_unlock(&adap->flow_lock);
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

	t4_os_lock(&adap->flow_lock);
	ret = __cxgbe_flow_query(flow, &c->hits, &c->bytes);
	if (ret) {
		rte_flow_error_set(e, -ret, RTE_FLOW_ERROR_TYPE_ACTION,
				   f, "cxgbe pmd failed to perform query");
		goto out;
	}

	/* Query was successful */
	c->bytes_set = 1;
	c->hits_set = 1;
	if (c->reset)
		cxgbe_clear_filter_count(adap, flow->fidx, f->fs.cap, true);

out:
	t4_os_unlock(&adap->flow_lock);
	return ret;
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
	int ret = 0;

	flow = t4_os_alloc(sizeof(struct rte_flow));
	if (!flow)
		return rte_flow_error_set(e, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"Unable to allocate memory for filter_entry");

	flow->item_parser = parseitem;
	flow->dev = dev;
	flow->fs.private = (void *)flow;

	ret = cxgbe_flow_parse(flow, attr, item, action, e);
	if (ret) {
		t4_os_free(flow);
		return ret;
	}

	if (cxgbe_validate_filter(adap, &flow->fs)) {
		t4_os_free(flow);
		return rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL,
				"validation failed. Check f/w config file.");
	}

	t4_os_lock(&adap->flow_lock);
	if (cxgbe_get_fidx(flow, &fidx)) {
		ret = rte_flow_error_set(e, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
					 NULL, "no memory in tcam.");
		goto out;
	}

	if (cxgbe_verify_fidx(flow, fidx, 0)) {
		ret = rte_flow_error_set(e, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
					 NULL, "validation failed");
		goto out;
	}

out:
	t4_os_unlock(&adap->flow_lock);
	t4_os_free(flow);
	return ret;
}

/*
 * @ret : > 0 filter destroyed successfully
 *        < 0 error destroying filter
 *        == 1 filter not active / not found
 */
static int
cxgbe_check_n_destroy(struct filter_entry *f, struct rte_eth_dev *dev)
{
	if (f && (f->valid || f->pending) &&
	    f->dev == dev && /* Only if user has asked for this port */
	     f->private) /* We (rte_flow) created this filter */
		return __cxgbe_flow_destroy(dev, (struct rte_flow *)f->private);
	return 1;
}

static int cxgbe_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *e)
{
	struct adapter *adap = ethdev2adap(dev);
	unsigned int i;
	int ret = 0;

	t4_os_lock(&adap->flow_lock);
	if (adap->tids.ftid_tab) {
		struct filter_entry *f = &adap->tids.ftid_tab[0];

		for (i = 0; i < adap->tids.nftids; i++, f++) {
			ret = cxgbe_check_n_destroy(f, dev);
			if (ret < 0) {
				rte_flow_error_set(e, ret,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   f->private,
						   "error destroying TCAM "
						   "filter.");
				goto out;
			}
		}
	}

	if (is_hashfilter(adap) && adap->tids.tid_tab) {
		struct filter_entry *f;

		for (i = adap->tids.hash_base; i <= adap->tids.ntids; i++) {
			f = (struct filter_entry *)adap->tids.tid_tab[i];

			ret = cxgbe_check_n_destroy(f, dev);
			if (ret < 0) {
				rte_flow_error_set(e, ret,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   f->private,
						   "error destroying HASH "
						   "filter.");
				goto out;
			}
		}
	}

out:
	t4_os_unlock(&adap->flow_lock);
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
cxgbe_dev_flow_ops_get(struct rte_eth_dev *dev __rte_unused,
		       const struct rte_flow_ops **ops)
{
	*ops = &cxgbe_flow_ops;
	return 0;
}
