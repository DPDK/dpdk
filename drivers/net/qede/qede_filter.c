/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_errno.h>
#include <rte_flow_driver.h>

#include "qede_ethdev.h"

/* VXLAN tunnel classification mapping */
const struct _qede_udp_tunn_types {
	uint16_t rte_filter_type;
	enum ecore_filter_ucast_type qede_type;
	enum ecore_tunn_clss qede_tunn_clss;
	const char *string;
} qede_tunn_types[] = {
	{
		RTE_ETH_TUNNEL_FILTER_OMAC,
		ECORE_FILTER_MAC,
		ECORE_TUNN_CLSS_MAC_VLAN,
		"outer-mac"
	},
	{
		RTE_ETH_TUNNEL_FILTER_TENID,
		ECORE_FILTER_VNI,
		ECORE_TUNN_CLSS_MAC_VNI,
		"vni"
	},
	{
		RTE_ETH_TUNNEL_FILTER_IMAC,
		ECORE_FILTER_INNER_MAC,
		ECORE_TUNN_CLSS_INNER_MAC_VLAN,
		"inner-mac"
	},
	{
		RTE_ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_INNER_VLAN,
		ECORE_TUNN_CLSS_INNER_MAC_VLAN,
		"inner-vlan"
	},
	{
		RTE_ETH_TUNNEL_FILTER_OMAC | RTE_ETH_TUNNEL_FILTER_TENID,
		ECORE_FILTER_MAC_VNI_PAIR,
		ECORE_TUNN_CLSS_MAC_VNI,
		"outer-mac and vni"
	},
	{
		RTE_ETH_TUNNEL_FILTER_OMAC | RTE_ETH_TUNNEL_FILTER_IMAC,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"outer-mac and inner-mac"
	},
	{
		RTE_ETH_TUNNEL_FILTER_OMAC | RTE_ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"outer-mac and inner-vlan"
	},
	{
		RTE_ETH_TUNNEL_FILTER_TENID | RTE_ETH_TUNNEL_FILTER_IMAC,
		ECORE_FILTER_INNER_MAC_VNI_PAIR,
		ECORE_TUNN_CLSS_INNER_MAC_VNI,
		"vni and inner-mac",
	},
	{
		RTE_ETH_TUNNEL_FILTER_TENID | RTE_ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"vni and inner-vlan",
	},
	{
		RTE_ETH_TUNNEL_FILTER_IMAC | RTE_ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_INNER_PAIR,
		ECORE_TUNN_CLSS_INNER_MAC_VLAN,
		"inner-mac and inner-vlan",
	},
	{
		RTE_ETH_TUNNEL_FILTER_OIP,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"outer-IP"
	},
	{
		RTE_ETH_TUNNEL_FILTER_IIP,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"inner-IP"
	},
	{
		RTE_ETH_TUNNEL_FILTER_IMAC_IVLAN,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"IMAC_IVLAN"
	},
	{
		RTE_ETH_TUNNEL_FILTER_IMAC_IVLAN_TENID,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"IMAC_IVLAN_TENID"
	},
	{
		RTE_ETH_TUNNEL_FILTER_IMAC_TENID,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"IMAC_TENID"
	},
	{
		RTE_ETH_TUNNEL_FILTER_OMAC_TENID_IMAC,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"OMAC_TENID_IMAC"
	},
};

#define IP_VERSION				(0x40)
#define IP_HDRLEN				(0x5)
#define QEDE_FDIR_IP_DEFAULT_VERSION_IHL	(IP_VERSION | IP_HDRLEN)
#define QEDE_FDIR_TCP_DEFAULT_DATAOFF		(0x50)
#define QEDE_FDIR_IPV4_DEF_TTL			(64)
#define QEDE_FDIR_IPV6_DEFAULT_VTC_FLOW		(0x60000000)
/* Sum of length of header types of L2, L3, L4.
 * L2 : ether_hdr + vlan_hdr + vxlan_hdr
 * L3 : ipv6_hdr
 * L4 : tcp_hdr
 */
#define QEDE_MAX_FDIR_PKT_LEN			(86)

static uint16_t
qede_arfs_construct_pkt(struct rte_eth_dev *eth_dev,
			struct qede_arfs_entry *arfs,
			void *buff,
			struct ecore_arfs_config_params *params);

/* Note: Flowdir support is only partial.
 * For ex: drop_queue, FDIR masks, flex_conf are not supported.
 * Parameters like pballoc/status fields are irrelevant here.
 */
int qede_check_fdir_support(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	qdev->arfs_info.arfs.mode = ECORE_FILTER_CONFIG_MODE_DISABLE;
	DP_INFO(edev, "flowdir is disabled\n");

	return 0;
}

void qede_fdir_dealloc_resc(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct qede_arfs_entry *tmp = NULL;

	SLIST_FOREACH(tmp, &qdev->arfs_info.arfs_list_head, list) {
		if (tmp) {
			if (tmp->mz)
				rte_memzone_free(tmp->mz);
			SLIST_REMOVE(&qdev->arfs_info.arfs_list_head, tmp,
				     qede_arfs_entry, list);
			rte_free(tmp);
		}
	}
}

static int
qede_config_arfs_filter(struct rte_eth_dev *eth_dev,
			struct qede_arfs_entry *arfs,
			bool add)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_ntuple_filter_params params;
	char mz_name[RTE_MEMZONE_NAMESIZE] = {0};
	struct qede_arfs_entry *tmp = NULL;
	const struct rte_memzone *mz;
	struct ecore_hwfn *p_hwfn;
	enum _ecore_status_t rc;
	uint16_t pkt_len;
	void *pkt;

	if (add) {
		if (qdev->arfs_info.filter_count == QEDE_RFS_MAX_FLTR - 1) {
			DP_ERR(edev, "Reached max flowdir filter limit\n");
			return -EINVAL;
		}
	}

	/* soft_id could have been used as memzone string, but soft_id is
	 * not currently used so it has no significance.
	 */
	snprintf(mz_name, sizeof(mz_name), "%lx",
		 (unsigned long)rte_get_timer_cycles());
	mz = rte_memzone_reserve_aligned(mz_name, QEDE_MAX_FDIR_PKT_LEN,
					 SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);
	if (!mz) {
		DP_ERR(edev, "Failed to allocate memzone for fdir, err = %s\n",
		       rte_strerror(rte_errno));
		return -rte_errno;
	}

	pkt = mz->addr;
	memset(pkt, 0, QEDE_MAX_FDIR_PKT_LEN);
	pkt_len = qede_arfs_construct_pkt(eth_dev, arfs, pkt,
					  &qdev->arfs_info.arfs);
	if (pkt_len == 0) {
		rc = -EINVAL;
		goto err1;
	}

	DP_INFO(edev, "pkt_len = %u memzone = %s\n", pkt_len, mz_name);
	if (add) {
		SLIST_FOREACH(tmp, &qdev->arfs_info.arfs_list_head, list) {
			if (memcmp(tmp->mz->addr, pkt, pkt_len) == 0) {
				DP_INFO(edev, "flowdir filter exist\n");
				rc = -EEXIST;
				goto err1;
			}
		}
	} else {
		SLIST_FOREACH(tmp, &qdev->arfs_info.arfs_list_head, list) {
			if (memcmp(tmp->mz->addr, pkt, pkt_len) == 0)
				break;
		}
		if (!tmp) {
			DP_ERR(edev, "flowdir filter does not exist\n");
			rc = -EEXIST;
			goto err1;
		}
	}
	p_hwfn = ECORE_LEADING_HWFN(edev);
	if (add) {
		if (qdev->arfs_info.arfs.mode ==
			ECORE_FILTER_CONFIG_MODE_DISABLE) {
			qdev->arfs_info.arfs.mode =
					ECORE_FILTER_CONFIG_MODE_5_TUPLE;
			DP_INFO(edev, "Force enable flowdir in perfect mode\n");
		}
		/* Enable ARFS searcher with updated flow_types */
		ecore_arfs_mode_configure(p_hwfn, p_hwfn->p_arfs_ptt,
					  &qdev->arfs_info.arfs);
	}

	memset(&params, 0, sizeof(params));
	params.addr = (dma_addr_t)mz->iova;
	params.length = pkt_len;
	params.qid = arfs->rx_queue;
	params.vport_id = 0;
	params.b_is_add = add;
	params.b_is_drop = arfs->is_drop;

	/* configure filter with ECORE_SPQ_MODE_EBLOCK */
	rc = ecore_configure_rfs_ntuple_filter(p_hwfn, NULL,
					       &params);
	if (rc == ECORE_SUCCESS) {
		if (add) {
			arfs->pkt_len = pkt_len;
			arfs->mz = mz;
			SLIST_INSERT_HEAD(&qdev->arfs_info.arfs_list_head,
					  arfs, list);
			qdev->arfs_info.filter_count++;
			DP_INFO(edev, "flowdir filter added, count = %d\n",
				qdev->arfs_info.filter_count);
		} else {
			rte_memzone_free(tmp->mz);
			SLIST_REMOVE(&qdev->arfs_info.arfs_list_head, tmp,
				     qede_arfs_entry, list);
			rte_free(tmp); /* the node deleted */
			rte_memzone_free(mz); /* temp node allocated */
			qdev->arfs_info.filter_count--;
			DP_INFO(edev, "Fdir filter deleted, count = %d\n",
				qdev->arfs_info.filter_count);
		}
	} else {
		DP_ERR(edev, "flowdir filter failed, rc=%d filter_count=%d\n",
		       rc, qdev->arfs_info.filter_count);
	}

	/* Disable ARFS searcher if there are no more filters */
	if (qdev->arfs_info.filter_count == 0) {
		memset(&qdev->arfs_info.arfs, 0,
		       sizeof(struct ecore_arfs_config_params));
		DP_INFO(edev, "Disabling flowdir\n");
		qdev->arfs_info.arfs.mode = ECORE_FILTER_CONFIG_MODE_DISABLE;
		ecore_arfs_mode_configure(p_hwfn, p_hwfn->p_arfs_ptt,
					  &qdev->arfs_info.arfs);
	}
	return 0;

err1:
	rte_memzone_free(mz);
	return rc;
}

/* Fills the L3/L4 headers and returns the actual length  of flowdir packet */
static uint16_t
qede_arfs_construct_pkt(struct rte_eth_dev *eth_dev,
			struct qede_arfs_entry *arfs,
			void *buff,
			struct ecore_arfs_config_params *params)

{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	uint16_t *ether_type;
	uint8_t *raw_pkt;
	struct rte_ipv4_hdr *ip;
	struct rte_ipv6_hdr *ip6;
	struct rte_udp_hdr *udp;
	struct rte_tcp_hdr *tcp;
	uint16_t len;

	raw_pkt = (uint8_t *)buff;

	len =  2 * sizeof(struct rte_ether_addr);
	raw_pkt += 2 * sizeof(struct rte_ether_addr);
	ether_type = (uint16_t *)raw_pkt;
	raw_pkt += sizeof(uint16_t);
	len += sizeof(uint16_t);

	*ether_type = rte_cpu_to_be_16(arfs->tuple.eth_proto);
	switch (arfs->tuple.eth_proto) {
	case RTE_ETHER_TYPE_IPV4:
		ip = (struct rte_ipv4_hdr *)raw_pkt;
		ip->version_ihl = QEDE_FDIR_IP_DEFAULT_VERSION_IHL;
		ip->total_length = sizeof(struct rte_ipv4_hdr);
		ip->next_proto_id = arfs->tuple.ip_proto;
		ip->time_to_live = QEDE_FDIR_IPV4_DEF_TTL;
		ip->dst_addr = arfs->tuple.dst_ipv4;
		ip->src_addr = arfs->tuple.src_ipv4;
		len += sizeof(struct rte_ipv4_hdr);
		params->ipv4 = true;

		raw_pkt = (uint8_t *)buff;
		/* UDP */
		if (arfs->tuple.ip_proto == IPPROTO_UDP) {
			udp = (struct rte_udp_hdr *)(raw_pkt + len);
			udp->dst_port = arfs->tuple.dst_port;
			udp->src_port = arfs->tuple.src_port;
			udp->dgram_len = sizeof(struct rte_udp_hdr);
			len += sizeof(struct rte_udp_hdr);
			/* adjust ip total_length */
			ip->total_length += sizeof(struct rte_udp_hdr);
			params->udp = true;
		} else { /* TCP */
			tcp = (struct rte_tcp_hdr *)(raw_pkt + len);
			tcp->src_port = arfs->tuple.src_port;
			tcp->dst_port = arfs->tuple.dst_port;
			tcp->data_off = QEDE_FDIR_TCP_DEFAULT_DATAOFF;
			len += sizeof(struct rte_tcp_hdr);
			/* adjust ip total_length */
			ip->total_length += sizeof(struct rte_tcp_hdr);
			params->tcp = true;
		}
		break;
	case RTE_ETHER_TYPE_IPV6:
		ip6 = (struct rte_ipv6_hdr *)raw_pkt;
		ip6->proto = arfs->tuple.ip_proto;
		ip6->vtc_flow =
			rte_cpu_to_be_32(QEDE_FDIR_IPV6_DEFAULT_VTC_FLOW);

		memcpy(&ip6->src_addr, arfs->tuple.src_ipv6, IPV6_ADDR_LEN);
		memcpy(&ip6->dst_addr, arfs->tuple.dst_ipv6, IPV6_ADDR_LEN);
		len += sizeof(struct rte_ipv6_hdr);
		params->ipv6 = true;

		raw_pkt = (uint8_t *)buff;
		/* UDP */
		if (arfs->tuple.ip_proto == IPPROTO_UDP) {
			udp = (struct rte_udp_hdr *)(raw_pkt + len);
			udp->src_port = arfs->tuple.src_port;
			udp->dst_port = arfs->tuple.dst_port;
			len += sizeof(struct rte_udp_hdr);
			params->udp = true;
		} else { /* TCP */
			tcp = (struct rte_tcp_hdr *)(raw_pkt + len);
			tcp->src_port = arfs->tuple.src_port;
			tcp->dst_port = arfs->tuple.dst_port;
			tcp->data_off = QEDE_FDIR_TCP_DEFAULT_DATAOFF;
			len += sizeof(struct rte_tcp_hdr);
			params->tcp = true;
		}
		break;
	default:
		DP_ERR(edev, "Unsupported eth_proto %u\n",
		       arfs->tuple.eth_proto);
		return 0;
	}

	return len;
}

static int
qede_tunnel_update(struct qede_dev *qdev,
		   struct ecore_tunnel_info *tunn_info)
{
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_hwfn *p_hwfn;
	struct ecore_ptt *p_ptt;
	int i;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		if (IS_PF(edev)) {
			p_ptt = ecore_ptt_acquire(p_hwfn);
			if (!p_ptt) {
				DP_ERR(p_hwfn, "Can't acquire PTT\n");
				return -EAGAIN;
			}
		} else {
			p_ptt = NULL;
		}

		rc = ecore_sp_pf_update_tunn_cfg(p_hwfn, p_ptt,
				tunn_info, ECORE_SPQ_MODE_CB, NULL);
		if (IS_PF(edev))
			ecore_ptt_release(p_hwfn, p_ptt);

		if (rc != ECORE_SUCCESS)
			break;
	}

	return rc;
}

static int
qede_vxlan_enable(struct rte_eth_dev *eth_dev, uint8_t clss,
		  bool enable)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_tunnel_info tunn;

	if (qdev->vxlan.enable == enable)
		return ECORE_SUCCESS;

	memset(&tunn, 0, sizeof(struct ecore_tunnel_info));
	tunn.vxlan.b_update_mode = true;
	tunn.vxlan.b_mode_enabled = enable;
	tunn.b_update_rx_cls = true;
	tunn.b_update_tx_cls = true;
	tunn.vxlan.tun_cls = clss;

	tunn.vxlan_port.b_update_port = true;
	tunn.vxlan_port.port = enable ? QEDE_VXLAN_DEF_PORT : 0;

	rc = qede_tunnel_update(qdev, &tunn);
	if (rc == ECORE_SUCCESS) {
		qdev->vxlan.enable = enable;
		qdev->vxlan.udp_port = (enable) ? QEDE_VXLAN_DEF_PORT : 0;
		DP_INFO(edev, "vxlan is %s, UDP port = %d\n",
			enable ? "enabled" : "disabled", qdev->vxlan.udp_port);
	} else {
		DP_ERR(edev, "Failed to update tunn_clss %u\n",
		       tunn.vxlan.tun_cls);
	}

	return rc;
}

static int
qede_geneve_enable(struct rte_eth_dev *eth_dev, uint8_t clss,
		  bool enable)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_tunnel_info tunn;

	memset(&tunn, 0, sizeof(struct ecore_tunnel_info));
	tunn.l2_geneve.b_update_mode = true;
	tunn.l2_geneve.b_mode_enabled = enable;
	tunn.ip_geneve.b_update_mode = true;
	tunn.ip_geneve.b_mode_enabled = enable;
	tunn.l2_geneve.tun_cls = clss;
	tunn.ip_geneve.tun_cls = clss;
	tunn.b_update_rx_cls = true;
	tunn.b_update_tx_cls = true;

	tunn.geneve_port.b_update_port = true;
	tunn.geneve_port.port = enable ? QEDE_GENEVE_DEF_PORT : 0;

	rc = qede_tunnel_update(qdev, &tunn);
	if (rc == ECORE_SUCCESS) {
		qdev->geneve.enable = enable;
		qdev->geneve.udp_port = (enable) ? QEDE_GENEVE_DEF_PORT : 0;
		DP_INFO(edev, "GENEVE is %s, UDP port = %d\n",
			enable ? "enabled" : "disabled", qdev->geneve.udp_port);
	} else {
		DP_ERR(edev, "Failed to update tunn_clss %u\n",
		       clss);
	}

	return rc;
}

int
qede_udp_dst_port_del(struct rte_eth_dev *eth_dev,
		      struct rte_eth_udp_tunnel *tunnel_udp)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_tunnel_info tunn; /* @DPDK */
	uint16_t udp_port;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	memset(&tunn, 0, sizeof(tunn));

	switch (tunnel_udp->prot_type) {
	case RTE_ETH_TUNNEL_TYPE_VXLAN:
		if (qdev->vxlan.udp_port != tunnel_udp->udp_port) {
			DP_ERR(edev, "UDP port %u doesn't exist\n",
				tunnel_udp->udp_port);
			return ECORE_INVAL;
		}
		udp_port = 0;

		tunn.vxlan_port.b_update_port = true;
		tunn.vxlan_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u\n",
			       tunn.vxlan_port.port);
			return rc;
		}

		qdev->vxlan.udp_port = udp_port;
		/* If the request is to delete UDP port and if the number of
		 * VXLAN filters have reached 0 then VxLAN offload can be
		 * disabled.
		 */
		if (qdev->vxlan.enable && qdev->vxlan.num_filters == 0)
			return qede_vxlan_enable(eth_dev,
					ECORE_TUNN_CLSS_MAC_VLAN, false);

		break;
	case RTE_ETH_TUNNEL_TYPE_GENEVE:
		if (qdev->geneve.udp_port != tunnel_udp->udp_port) {
			DP_ERR(edev, "UDP port %u doesn't exist\n",
				tunnel_udp->udp_port);
			return ECORE_INVAL;
		}

		udp_port = 0;

		tunn.geneve_port.b_update_port = true;
		tunn.geneve_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u\n",
			       tunn.vxlan_port.port);
			return rc;
		}

		qdev->vxlan.udp_port = udp_port;
		/* If the request is to delete UDP port and if the number of
		 * GENEVE filters have reached 0 then GENEVE offload can be
		 * disabled.
		 */
		if (qdev->geneve.enable && qdev->geneve.num_filters == 0)
			return qede_geneve_enable(eth_dev,
					ECORE_TUNN_CLSS_MAC_VLAN, false);

		break;

	default:
		return ECORE_INVAL;
	}

	return 0;
}

int
qede_udp_dst_port_add(struct rte_eth_dev *eth_dev,
		      struct rte_eth_udp_tunnel *tunnel_udp)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_tunnel_info tunn; /* @DPDK */
	uint16_t udp_port;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	memset(&tunn, 0, sizeof(tunn));

	switch (tunnel_udp->prot_type) {
	case RTE_ETH_TUNNEL_TYPE_VXLAN:
		if (qdev->vxlan.udp_port == tunnel_udp->udp_port) {
			DP_INFO(edev,
				"UDP port %u for VXLAN was already configured\n",
				tunnel_udp->udp_port);
			return ECORE_SUCCESS;
		}

		/* Enable VxLAN tunnel with default MAC/VLAN classification if
		 * it was not enabled while adding VXLAN filter before UDP port
		 * update.
		 */
		if (!qdev->vxlan.enable) {
			rc = qede_vxlan_enable(eth_dev,
				ECORE_TUNN_CLSS_MAC_VLAN, true);
			if (rc != ECORE_SUCCESS) {
				DP_ERR(edev, "Failed to enable VXLAN "
					"prior to updating UDP port\n");
				return rc;
			}
		}
		udp_port = tunnel_udp->udp_port;

		tunn.vxlan_port.b_update_port = true;
		tunn.vxlan_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u for VXLAN\n",
			       udp_port);
			return rc;
		}

		DP_INFO(edev, "Updated UDP port %u for VXLAN\n", udp_port);

		qdev->vxlan.udp_port = udp_port;
		break;
	case RTE_ETH_TUNNEL_TYPE_GENEVE:
		if (qdev->geneve.udp_port == tunnel_udp->udp_port) {
			DP_INFO(edev,
				"UDP port %u for GENEVE was already configured\n",
				tunnel_udp->udp_port);
			return ECORE_SUCCESS;
		}

		/* Enable GENEVE tunnel with default MAC/VLAN classification if
		 * it was not enabled while adding GENEVE filter before UDP port
		 * update.
		 */
		if (!qdev->geneve.enable) {
			rc = qede_geneve_enable(eth_dev,
				ECORE_TUNN_CLSS_MAC_VLAN, true);
			if (rc != ECORE_SUCCESS) {
				DP_ERR(edev, "Failed to enable GENEVE "
					"prior to updating UDP port\n");
				return rc;
			}
		}
		udp_port = tunnel_udp->udp_port;

		tunn.geneve_port.b_update_port = true;
		tunn.geneve_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u for GENEVE\n",
			       udp_port);
			return rc;
		}

		DP_INFO(edev, "Updated UDP port %u for GENEVE\n", udp_port);

		qdev->geneve.udp_port = udp_port;
		break;
	default:
		return ECORE_INVAL;
	}

	return 0;
}

static int
qede_flow_validate_attr(__rte_unused struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			struct rte_flow_error *error)
{
	if (attr == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "NULL attribute");
		return -rte_errno;
	}

	if (attr->group != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP, attr,
				   "Groups are not supported");
		return -rte_errno;
	}

	if (attr->priority != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, attr,
				   "Priorities are not supported");
		return -rte_errno;
	}

	if (attr->egress != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, attr,
				   "Egress is not supported");
		return -rte_errno;
	}

	if (attr->transfer != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER, attr,
				   "Transfer is not supported");
		return -rte_errno;
	}

	if (attr->ingress == 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, attr,
				   "Only ingress is supported");
		return -rte_errno;
	}

	return 0;
}

static int
qede_flow_parse_pattern(__rte_unused struct rte_eth_dev *dev,
			const struct rte_flow_item pattern[],
			struct rte_flow_error *error,
			struct rte_flow *flow)
{
	bool l3 = false, l4 = false;

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_NUM, NULL,
				   "NULL pattern");
		return -rte_errno;
	}

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		if (!pattern->spec) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   pattern,
					   "Item spec not defined");
			return -rte_errno;
		}

		if (pattern->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   pattern,
					   "Item last not supported");
			return -rte_errno;
		}

		if (pattern->mask) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   pattern,
					   "Item mask not supported");
			return -rte_errno;
		}

		/* Below validation is only for 4 tuple flow
		 * (GFT_PROFILE_TYPE_4_TUPLE)
		 * - src and dst L3 address (IPv4 or IPv6)
		 * - src and dst L4 port (TCP or UDP)
		 */

		switch (pattern->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = true;

			if (flow) {
				const struct rte_flow_item_ipv4 *spec;

				spec = pattern->spec;
				flow->entry.tuple.src_ipv4 = spec->hdr.src_addr;
				flow->entry.tuple.dst_ipv4 = spec->hdr.dst_addr;
				flow->entry.tuple.eth_proto =
					RTE_ETHER_TYPE_IPV4;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			l3 = true;

			if (flow) {
				const struct rte_flow_item_ipv6 *spec;

				spec = pattern->spec;
				memcpy(flow->entry.tuple.src_ipv6,
				       spec->hdr.src_addr, IPV6_ADDR_LEN);
				memcpy(flow->entry.tuple.dst_ipv6,
				       spec->hdr.dst_addr, IPV6_ADDR_LEN);
				flow->entry.tuple.eth_proto =
					RTE_ETHER_TYPE_IPV6;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			l4 = true;

			if (flow) {
				const struct rte_flow_item_udp *spec;

				spec = pattern->spec;
				flow->entry.tuple.src_port =
						spec->hdr.src_port;
				flow->entry.tuple.dst_port =
						spec->hdr.dst_port;
				flow->entry.tuple.ip_proto = IPPROTO_UDP;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			l4 = true;

			if (flow) {
				const struct rte_flow_item_tcp *spec;

				spec = pattern->spec;
				flow->entry.tuple.src_port =
						spec->hdr.src_port;
				flow->entry.tuple.dst_port =
						spec->hdr.dst_port;
				flow->entry.tuple.ip_proto = IPPROTO_TCP;
			}

			break;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   pattern,
					   "Only 4 tuple (IPV4, IPV6, UDP and TCP) item types supported");
			return -rte_errno;
		}
	}

	if (!(l3 && l4)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   pattern,
				   "Item types need to have both L3 and L4 protocols");
		return -rte_errno;
	}

	return 0;
}

static int
qede_flow_parse_actions(struct rte_eth_dev *dev,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error,
			struct rte_flow *flow)
{
	const struct rte_flow_action_queue *queue;

	if (actions == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "NULL actions");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			queue = actions->conf;

			if (queue->index >= QEDE_RSS_COUNT(dev)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "Bad QUEUE action");
				return -rte_errno;
			}

			if (flow)
				flow->entry.rx_queue = queue->index;

			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			if (flow)
				flow->entry.is_drop = true;
			break;
		default:
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions,
					   "Action is not supported - only ACTION_TYPE_QUEUE and ACTION_TYPE_DROP supported");
			return -rte_errno;
		}
	}

	return 0;
}

static int
qede_flow_parse(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item patterns[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error,
		struct rte_flow *flow)

{
	int rc = 0;

	rc = qede_flow_validate_attr(dev, attr, error);
	if (rc)
		return rc;

	/* parse and validate item pattern and actions.
	 * Given item list and actions will be translate to qede PMD
	 * specific arfs structure.
	 */
	rc = qede_flow_parse_pattern(dev, patterns, error, flow);
	if (rc)
		return rc;

	rc = qede_flow_parse_actions(dev, actions, error, flow);

	return rc;
}

static int
qede_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item patterns[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	return qede_flow_parse(dev, attr, patterns, actions, error, NULL);
}

static struct rte_flow *
qede_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;
	int rc;

	flow = rte_zmalloc("qede_rte_flow", sizeof(*flow), 0);
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to allocate memory");
		return NULL;
	}

	rc = qede_flow_parse(dev, attr, pattern, actions, error, flow);
	if (rc < 0) {
		rte_free(flow);
		return NULL;
	}

	rc = qede_config_arfs_filter(dev, &flow->entry, true);
	if (rc < 0) {
		rte_flow_error_set(error, rc,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to configure flow filter");
		rte_free(flow);
		return NULL;
	}

	return flow;
}

static int
qede_flow_destroy(struct rte_eth_dev *eth_dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	int rc = 0;

	rc = qede_config_arfs_filter(eth_dev, &flow->entry, false);
	if (rc < 0) {
		rte_flow_error_set(error, rc,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to delete flow filter");
		rte_free(flow);
	}

	return rc;
}

static int
qede_flow_flush(struct rte_eth_dev *eth_dev,
		struct rte_flow_error *error)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct qede_arfs_entry *tmp = NULL;
	int rc = 0;

	while (!SLIST_EMPTY(&qdev->arfs_info.arfs_list_head)) {
		tmp = SLIST_FIRST(&qdev->arfs_info.arfs_list_head);

		rc = qede_config_arfs_filter(eth_dev, tmp, false);
		if (rc < 0)
			rte_flow_error_set(error, rc,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to flush flow filter");
	}

	return rc;
}

const struct rte_flow_ops qede_flow_ops = {
	.validate = qede_flow_validate,
	.create = qede_flow_create,
	.destroy = qede_flow_destroy,
	.flush = qede_flow_flush,
};

int
qede_dev_flow_ops_get(struct rte_eth_dev *eth_dev,
		      const struct rte_flow_ops **ops)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	if (ECORE_IS_CMT(edev)) {
		DP_ERR(edev, "flowdir is not supported in 100G mode\n");
		return -ENOTSUP;
	}

	*ops = &qede_flow_ops;
	return 0;
}
