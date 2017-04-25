/*
 * Copyright (c) 2017 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_errno.h>

#include "qede_ethdev.h"

#define IP_VERSION				(0x40)
#define IP_HDRLEN				(0x5)
#define QEDE_FDIR_IP_DEFAULT_VERSION_IHL	(IP_VERSION | IP_HDRLEN)
#define QEDE_FDIR_TCP_DEFAULT_DATAOFF		(0x50)
#define QEDE_FDIR_IPV4_DEF_TTL			(64)

/* Sum of length of header types of L2, L3, L4.
 * L2 : ether_hdr + vlan_hdr + vxlan_hdr
 * L3 : ipv6_hdr
 * L4 : tcp_hdr
 */
#define QEDE_MAX_FDIR_PKT_LEN			(86)

#ifndef IPV6_ADDR_LEN
#define IPV6_ADDR_LEN				(16)
#endif

#define QEDE_VALID_FLOW(flow_type) \
	((flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_TCP	|| \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV4_UDP	|| \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_TCP	|| \
	(flow_type) == RTE_ETH_FLOW_NONFRAG_IPV6_UDP)

/* Note: Flowdir support is only partial.
 * For ex: drop_queue, FDIR masks, flex_conf are not supported.
 * Parameters like pballoc/status fields are irrelevant here.
 */
int qede_check_fdir_support(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_fdir_conf *fdir = &eth_dev->data->dev_conf.fdir_conf;

	/* check FDIR modes */
	switch (fdir->mode) {
	case RTE_FDIR_MODE_NONE:
		qdev->fdir_info.arfs.arfs_enable = false;
		DP_INFO(edev, "flowdir is disabled\n");
	break;
	case RTE_FDIR_MODE_PERFECT:
		if (edev->num_hwfns > 1) {
			DP_ERR(edev, "flowdir is not supported in 100G mode\n");
			qdev->fdir_info.arfs.arfs_enable = false;
			return -ENOTSUP;
		}
		qdev->fdir_info.arfs.arfs_enable = true;
		DP_INFO(edev, "flowdir is enabled\n");
	break;
	case RTE_FDIR_MODE_PERFECT_TUNNEL:
	case RTE_FDIR_MODE_SIGNATURE:
	case RTE_FDIR_MODE_PERFECT_MAC_VLAN:
		DP_ERR(edev, "Unsupported flowdir mode %d\n", fdir->mode);
		return -ENOTSUP;
	}

	return 0;
}

void qede_fdir_dealloc_resc(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct qede_fdir_entry *tmp = NULL;

	SLIST_FOREACH(tmp, &qdev->fdir_info.fdir_list_head, list) {
		if (tmp) {
			if (tmp->mz)
				rte_memzone_free(tmp->mz);
			SLIST_REMOVE(&qdev->fdir_info.fdir_list_head, tmp,
				     qede_fdir_entry, list);
			rte_free(tmp);
		}
	}
}

static int
qede_config_cmn_fdir_filter(struct rte_eth_dev *eth_dev,
			    struct rte_eth_fdir_filter *fdir_filter,
			    bool add)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	char mz_name[RTE_MEMZONE_NAMESIZE] = {0};
	struct qede_fdir_entry *tmp = NULL;
	struct qede_fdir_entry *fdir = NULL;
	const struct rte_memzone *mz;
	struct ecore_hwfn *p_hwfn;
	enum _ecore_status_t rc;
	uint16_t pkt_len;
	void *pkt;

	if (add) {
		if (qdev->fdir_info.filter_count == QEDE_RFS_MAX_FLTR - 1) {
			DP_ERR(edev, "Reached max flowdir filter limit\n");
			return -EINVAL;
		}
		fdir = rte_malloc(NULL, sizeof(struct qede_fdir_entry),
				  RTE_CACHE_LINE_SIZE);
		if (!fdir) {
			DP_ERR(edev, "Did not allocate memory for fdir\n");
			return -ENOMEM;
		}
	}
	/* soft_id could have been used as memzone string, but soft_id is
	 * not currently used so it has no significance.
	 */
	snprintf(mz_name, sizeof(mz_name) - 1, "%lx",
		 (unsigned long)rte_get_timer_cycles());
	mz = rte_memzone_reserve_aligned(mz_name, QEDE_MAX_FDIR_PKT_LEN,
					 SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);
	if (!mz) {
		DP_ERR(edev, "Failed to allocate memzone for fdir, err = %s\n",
		       rte_strerror(rte_errno));
		rc = -rte_errno;
		goto err1;
	}

	pkt = mz->addr;
	memset(pkt, 0, QEDE_MAX_FDIR_PKT_LEN);
	pkt_len = qede_fdir_construct_pkt(eth_dev, fdir_filter, pkt,
					  &qdev->fdir_info.arfs);
	if (pkt_len == 0) {
		rc = -EINVAL;
		goto err2;
	}
	DP_INFO(edev, "pkt_len = %u memzone = %s\n", pkt_len, mz_name);
	if (add) {
		SLIST_FOREACH(tmp, &qdev->fdir_info.fdir_list_head, list) {
			if (memcmp(tmp->mz->addr, pkt, pkt_len) == 0) {
				DP_ERR(edev, "flowdir filter exist\n");
				rc = -EEXIST;
				goto err2;
			}
		}
	} else {
		SLIST_FOREACH(tmp, &qdev->fdir_info.fdir_list_head, list) {
			if (memcmp(tmp->mz->addr, pkt, pkt_len) == 0)
				break;
		}
		if (!tmp) {
			DP_ERR(edev, "flowdir filter does not exist\n");
			rc = -EEXIST;
			goto err2;
		}
	}
	p_hwfn = ECORE_LEADING_HWFN(edev);
	if (add) {
		if (!qdev->fdir_info.arfs.arfs_enable) {
			/* Force update */
			eth_dev->data->dev_conf.fdir_conf.mode =
						RTE_FDIR_MODE_PERFECT;
			qdev->fdir_info.arfs.arfs_enable = true;
			DP_INFO(edev, "Force enable flowdir in perfect mode\n");
		}
		/* Enable ARFS searcher with updated flow_types */
		ecore_arfs_mode_configure(p_hwfn, p_hwfn->p_arfs_ptt,
					  &qdev->fdir_info.arfs);
	}
	/* configure filter with ECORE_SPQ_MODE_EBLOCK */
	rc = ecore_configure_rfs_ntuple_filter(p_hwfn, p_hwfn->p_arfs_ptt, NULL,
					       (dma_addr_t)mz->phys_addr,
					       pkt_len,
					       fdir_filter->action.rx_queue,
					       0, add);
	if (rc == ECORE_SUCCESS) {
		if (add) {
			fdir->rx_queue = fdir_filter->action.rx_queue;
			fdir->pkt_len = pkt_len;
			fdir->mz = mz;
			SLIST_INSERT_HEAD(&qdev->fdir_info.fdir_list_head,
					  fdir, list);
			qdev->fdir_info.filter_count++;
			DP_INFO(edev, "flowdir filter added, count = %d\n",
				qdev->fdir_info.filter_count);
		} else {
			rte_memzone_free(tmp->mz);
			SLIST_REMOVE(&qdev->fdir_info.fdir_list_head, tmp,
				     qede_fdir_entry, list);
			rte_free(tmp); /* the node deleted */
			rte_memzone_free(mz); /* temp node allocated */
			qdev->fdir_info.filter_count--;
			DP_INFO(edev, "Fdir filter deleted, count = %d\n",
				qdev->fdir_info.filter_count);
		}
	} else {
		DP_ERR(edev, "flowdir filter failed, rc=%d filter_count=%d\n",
		       rc, qdev->fdir_info.filter_count);
	}

	/* Disable ARFS searcher if there are no more filters */
	if (qdev->fdir_info.filter_count == 0) {
		memset(&qdev->fdir_info.arfs, 0,
		       sizeof(struct ecore_arfs_config_params));
		DP_INFO(edev, "Disabling flowdir\n");
		qdev->fdir_info.arfs.arfs_enable = false;
		ecore_arfs_mode_configure(p_hwfn, p_hwfn->p_arfs_ptt,
					  &qdev->fdir_info.arfs);
	}
	return 0;

err2:
	rte_memzone_free(mz);
err1:
	if (add)
		rte_free(fdir);
	return rc;
}

static int
qede_fdir_filter_add(struct rte_eth_dev *eth_dev,
		     struct rte_eth_fdir_filter *fdir,
		     bool add)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	if (!QEDE_VALID_FLOW(fdir->input.flow_type)) {
		DP_ERR(edev, "invalid flow_type input\n");
		return -EINVAL;
	}

	if (fdir->action.rx_queue >= QEDE_RSS_COUNT(qdev)) {
		DP_ERR(edev, "invalid queue number %u\n",
		       fdir->action.rx_queue);
		return -EINVAL;
	}

	if (fdir->input.flow_ext.is_vf) {
		DP_ERR(edev, "flowdir is not supported over VF\n");
		return -EINVAL;
	}

	return qede_config_cmn_fdir_filter(eth_dev, fdir, add);
}

/* Fills the L3/L4 headers and returns the actual length  of flowdir packet */
uint16_t
qede_fdir_construct_pkt(struct rte_eth_dev *eth_dev,
			struct rte_eth_fdir_filter *fdir,
			void *buff,
			struct ecore_arfs_config_params *params)

{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	uint16_t *ether_type;
	uint8_t *raw_pkt;
	struct rte_eth_fdir_input *input;
	static uint8_t vlan_frame[] = {0x81, 0, 0, 0};
	struct ipv4_hdr *ip;
	struct ipv6_hdr *ip6;
	struct udp_hdr *udp;
	struct tcp_hdr *tcp;
	uint16_t len;
	static const uint8_t next_proto[] = {
		[RTE_ETH_FLOW_NONFRAG_IPV4_TCP] = IPPROTO_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] = IPPROTO_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_TCP] = IPPROTO_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] = IPPROTO_UDP,
	};
	raw_pkt = (uint8_t *)buff;
	input = &fdir->input;
	DP_INFO(edev, "flow_type %d\n", input->flow_type);

	len =  2 * sizeof(struct ether_addr);
	raw_pkt += 2 * sizeof(struct ether_addr);
	if (input->flow_ext.vlan_tci) {
		DP_INFO(edev, "adding VLAN header\n");
		rte_memcpy(raw_pkt, vlan_frame, sizeof(vlan_frame));
		rte_memcpy(raw_pkt + sizeof(uint16_t),
			   &input->flow_ext.vlan_tci,
			   sizeof(uint16_t));
		raw_pkt += sizeof(vlan_frame);
		len += sizeof(vlan_frame);
	}
	ether_type = (uint16_t *)raw_pkt;
	raw_pkt += sizeof(uint16_t);
	len += sizeof(uint16_t);

	switch (input->flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		/* fill the common ip header */
		ip = (struct ipv4_hdr *)raw_pkt;
		*ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		ip->version_ihl = QEDE_FDIR_IP_DEFAULT_VERSION_IHL;
		ip->total_length = sizeof(struct ipv4_hdr);
		ip->next_proto_id = input->flow.ip4_flow.proto ?
				    input->flow.ip4_flow.proto :
				    next_proto[input->flow_type];
		ip->time_to_live = input->flow.ip4_flow.ttl ?
				   input->flow.ip4_flow.ttl :
				   QEDE_FDIR_IPV4_DEF_TTL;
		ip->type_of_service = input->flow.ip4_flow.tos;
		ip->dst_addr = input->flow.ip4_flow.dst_ip;
		ip->src_addr = input->flow.ip4_flow.src_ip;
		len += sizeof(struct ipv4_hdr);
		params->ipv4 = true;

		raw_pkt = (uint8_t *)buff;
		/* UDP */
		if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV4_UDP) {
			udp = (struct udp_hdr *)(raw_pkt + len);
			udp->dst_port = input->flow.udp4_flow.dst_port;
			udp->src_port = input->flow.udp4_flow.src_port;
			udp->dgram_len = sizeof(struct udp_hdr);
			len += sizeof(struct udp_hdr);
			/* adjust ip total_length */
			ip->total_length += sizeof(struct udp_hdr);
			params->udp = true;
		} else { /* TCP */
			tcp = (struct tcp_hdr *)(raw_pkt + len);
			tcp->src_port = input->flow.tcp4_flow.src_port;
			tcp->dst_port = input->flow.tcp4_flow.dst_port;
			tcp->data_off = QEDE_FDIR_TCP_DEFAULT_DATAOFF;
			len += sizeof(struct tcp_hdr);
			/* adjust ip total_length */
			ip->total_length += sizeof(struct tcp_hdr);
			params->tcp = true;
		}
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		ip6 = (struct ipv6_hdr *)raw_pkt;
		*ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
		ip6->proto = input->flow.ipv6_flow.proto ?
					input->flow.ipv6_flow.proto :
					next_proto[input->flow_type];
		rte_memcpy(&ip6->src_addr, &input->flow.ipv6_flow.dst_ip,
			   IPV6_ADDR_LEN);
		rte_memcpy(&ip6->dst_addr, &input->flow.ipv6_flow.src_ip,
			   IPV6_ADDR_LEN);
		len += sizeof(struct ipv6_hdr);

		raw_pkt = (uint8_t *)buff;
		/* UDP */
		if (input->flow_type == RTE_ETH_FLOW_NONFRAG_IPV6_UDP) {
			udp = (struct udp_hdr *)(raw_pkt + len);
			udp->src_port = input->flow.udp6_flow.dst_port;
			udp->dst_port = input->flow.udp6_flow.src_port;
			len += sizeof(struct udp_hdr);
			params->udp = true;
		} else { /* TCP */
			tcp = (struct tcp_hdr *)(raw_pkt + len);
			tcp->src_port = input->flow.tcp4_flow.src_port;
			tcp->dst_port = input->flow.tcp4_flow.dst_port;
			tcp->data_off = QEDE_FDIR_TCP_DEFAULT_DATAOFF;
			len += sizeof(struct tcp_hdr);
			params->tcp = true;
		}
		break;
	default:
		DP_ERR(edev, "Unsupported flow_type %u\n",
		       input->flow_type);
		return 0;
	}

	return len;
}

int
qede_fdir_filter_conf(struct rte_eth_dev *eth_dev,
		      enum rte_filter_op filter_op,
		      void *arg)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_fdir_filter *fdir;
	int ret;

	fdir = (struct rte_eth_fdir_filter *)arg;
	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		/* Typically used to query flowdir support */
		if (edev->num_hwfns > 1) {
			DP_ERR(edev, "flowdir is not supported in 100G mode\n");
			return -ENOTSUP;
		}
		return 0; /* means supported */
	case RTE_ETH_FILTER_ADD:
		ret = qede_fdir_filter_add(eth_dev, fdir, 1);
	break;
	case RTE_ETH_FILTER_DELETE:
		ret = qede_fdir_filter_add(eth_dev, fdir, 0);
	break;
	case RTE_ETH_FILTER_FLUSH:
	case RTE_ETH_FILTER_UPDATE:
	case RTE_ETH_FILTER_INFO:
		return -ENOTSUP;
	break;
	default:
		DP_ERR(edev, "unknown operation %u", filter_op);
		ret = -EINVAL;
	}

	return ret;
}

int qede_ntuple_filter_conf(struct rte_eth_dev *eth_dev,
			    enum rte_filter_op filter_op,
			    void *arg)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_ntuple_filter *ntuple;
	struct rte_eth_fdir_filter fdir_entry;
	struct rte_eth_tcpv4_flow *tcpv4_flow;
	struct rte_eth_udpv4_flow *udpv4_flow;
	bool add = false;

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		/* Typically used to query fdir support */
		if (edev->num_hwfns > 1) {
			DP_ERR(edev, "flowdir is not supported in 100G mode\n");
			return -ENOTSUP;
		}
		return 0; /* means supported */
	case RTE_ETH_FILTER_ADD:
		add = true;
	break;
	case RTE_ETH_FILTER_DELETE:
	break;
	case RTE_ETH_FILTER_INFO:
	case RTE_ETH_FILTER_GET:
	case RTE_ETH_FILTER_UPDATE:
	case RTE_ETH_FILTER_FLUSH:
	case RTE_ETH_FILTER_SET:
	case RTE_ETH_FILTER_STATS:
	case RTE_ETH_FILTER_OP_MAX:
		DP_ERR(edev, "Unsupported filter_op %d\n", filter_op);
		return -ENOTSUP;
	}
	ntuple = (struct rte_eth_ntuple_filter *)arg;
	/* Internally convert ntuple to fdir entry */
	memset(&fdir_entry, 0, sizeof(fdir_entry));
	if (ntuple->proto == IPPROTO_TCP) {
		fdir_entry.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_TCP;
		tcpv4_flow = &fdir_entry.input.flow.tcp4_flow;
		tcpv4_flow->ip.src_ip = ntuple->src_ip;
		tcpv4_flow->ip.dst_ip = ntuple->dst_ip;
		tcpv4_flow->ip.proto = IPPROTO_TCP;
		tcpv4_flow->src_port = ntuple->src_port;
		tcpv4_flow->dst_port = ntuple->dst_port;
	} else {
		fdir_entry.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_UDP;
		udpv4_flow = &fdir_entry.input.flow.udp4_flow;
		udpv4_flow->ip.src_ip = ntuple->src_ip;
		udpv4_flow->ip.dst_ip = ntuple->dst_ip;
		udpv4_flow->ip.proto = IPPROTO_TCP;
		udpv4_flow->src_port = ntuple->src_port;
		udpv4_flow->dst_port = ntuple->dst_port;
	}
	return qede_config_cmn_fdir_filter(eth_dev, &fdir_entry, add);
}
