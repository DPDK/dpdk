/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic_compat.h"
#include "rte_ethdev_driver.h"
#include "wq_enet_desc.h"
#include "rq_enet_desc.h"
#include "cq_enet_desc.h"
#include "vnic_resource.h"
#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_stats.h"
#include "vnic_nic.h"
#include "vnic_rss.h"
#include "enic_res.h"
#include "enic.h"

int enic_get_vnic_config(struct enic *enic)
{
	struct vnic_enet_config *c = &enic->config;
	int err;

	err = vnic_dev_get_mac_addr(enic->vdev, enic->mac_addr);
	if (err) {
		dev_err(enic_get_dev(enic),
			"Error getting MAC addr, %d\n", err);
		return err;
	}


#define GET_CONFIG(m) \
	do { \
		err = vnic_dev_spec(enic->vdev, \
			offsetof(struct vnic_enet_config, m), \
			sizeof(c->m), &c->m); \
		if (err) { \
			dev_err(enic_get_dev(enic), \
				"Error getting %s, %d\n", #m, err); \
			return err; \
		} \
	} while (0)

	GET_CONFIG(flags);
	GET_CONFIG(wq_desc_count);
	GET_CONFIG(rq_desc_count);
	GET_CONFIG(mtu);
	GET_CONFIG(intr_timer_type);
	GET_CONFIG(intr_mode);
	GET_CONFIG(intr_timer_usec);
	GET_CONFIG(loop_tag);
	GET_CONFIG(num_arfs);
	GET_CONFIG(max_pkt_size);

	/* max packet size is only defined in newer VIC firmware
	 * and will be 0 for legacy firmware and VICs
	 */
	if (c->max_pkt_size > ENIC_DEFAULT_RX_MAX_PKT_SIZE)
		enic->max_mtu = c->max_pkt_size - RTE_ETHER_HDR_LEN;
	else
		enic->max_mtu = ENIC_DEFAULT_RX_MAX_PKT_SIZE -
			RTE_ETHER_HDR_LEN;
	if (c->mtu == 0)
		c->mtu = 1500;

	enic->rte_dev->data->mtu = RTE_MIN(enic->max_mtu,
				RTE_MAX((uint16_t)ENIC_MIN_MTU, c->mtu));

	enic->adv_filters = vnic_dev_capable_adv_filters(enic->vdev);
	dev_info(enic, "Advanced Filters %savailable\n", ((enic->adv_filters)
		 ? "" : "not "));

	err = vnic_dev_capable_filter_mode(enic->vdev, &enic->flow_filter_mode,
					   &enic->filter_actions);
	if (err) {
		dev_err(enic_get_dev(enic),
			"Error getting filter modes, %d\n", err);
		return err;
	}
	vnic_dev_capable_udp_rss_weak(enic->vdev, &enic->nic_cfg_chk,
				      &enic->udp_rss_weak);

	dev_info(enic, "Flow api filter mode: %s Actions: %s%s%s%s\n",
		((enic->flow_filter_mode == FILTER_FLOWMAN) ? "FLOWMAN" :
		((enic->flow_filter_mode == FILTER_DPDK_1) ? "DPDK" :
		((enic->flow_filter_mode == FILTER_USNIC_IP) ? "USNIC" :
		((enic->flow_filter_mode == FILTER_IPV4_5TUPLE) ? "5TUPLE" :
		"NONE")))),
		((enic->filter_actions & FILTER_ACTION_RQ_STEERING_FLAG) ?
		 "steer " : ""),
		((enic->filter_actions & FILTER_ACTION_FILTER_ID_FLAG) ?
		 "tag " : ""),
		((enic->filter_actions & FILTER_ACTION_DROP_FLAG) ?
		 "drop " : ""),
		((enic->filter_actions & FILTER_ACTION_COUNTER_FLAG) ?
		 "count " : ""));

	c->wq_desc_count = RTE_MIN((uint32_t)ENIC_MAX_WQ_DESCS,
			RTE_MAX((uint32_t)ENIC_MIN_WQ_DESCS, c->wq_desc_count));
	c->wq_desc_count &= 0xffffffe0; /* must be aligned to groups of 32 */

	c->rq_desc_count = RTE_MIN((uint32_t)ENIC_MAX_RQ_DESCS,
			RTE_MAX((uint32_t)ENIC_MIN_RQ_DESCS, c->rq_desc_count));
	c->rq_desc_count &= 0xffffffe0; /* must be aligned to groups of 32 */

	c->intr_timer_usec = RTE_MIN(c->intr_timer_usec,
				  vnic_dev_get_intr_coal_timer_max(enic->vdev));

	dev_info(enic_get_dev(enic),
		"vNIC MAC addr %02x:%02x:%02x:%02x:%02x:%02x "
		"wq/rq %d/%d mtu %d, max mtu:%d\n",
		enic->mac_addr[0], enic->mac_addr[1], enic->mac_addr[2],
		enic->mac_addr[3], enic->mac_addr[4], enic->mac_addr[5],
		c->wq_desc_count, c->rq_desc_count,
		enic->rte_dev->data->mtu, enic->max_mtu);
	dev_info(enic_get_dev(enic), "vNIC csum tx/rx %s/%s "
		"rss %s intr mode %s type %s timer %d usec "
		"loopback tag 0x%04x\n",
		ENIC_SETTING(enic, TXCSUM) ? "yes" : "no",
		ENIC_SETTING(enic, RXCSUM) ? "yes" : "no",
		ENIC_SETTING(enic, RSS) ?
			(ENIC_SETTING(enic, RSSHASH_UDPIPV4) ? "+UDP" :
			((enic->udp_rss_weak ? "+udp" :
			"yes"))) : "no",
		c->intr_mode == VENET_INTR_MODE_INTX ? "INTx" :
		c->intr_mode == VENET_INTR_MODE_MSI ? "MSI" :
		c->intr_mode == VENET_INTR_MODE_ANY ? "any" :
		"unknown",
		c->intr_timer_type == VENET_INTR_TYPE_MIN ? "min" :
		c->intr_timer_type == VENET_INTR_TYPE_IDLE ? "idle" :
		"unknown",
		c->intr_timer_usec,
		c->loop_tag);

	/* RSS settings from vNIC */
	enic->reta_size = ENIC_RSS_RETA_SIZE;
	enic->hash_key_size = ENIC_RSS_HASH_KEY_SIZE;
	enic->flow_type_rss_offloads = 0;
	if (ENIC_SETTING(enic, RSSHASH_IPV4))
		/*
		 * IPV4 hash type handles both non-frag and frag packet types.
		 * TCP/UDP is controlled via a separate flag below.
		 */
		enic->flow_type_rss_offloads |= ETH_RSS_IPV4 |
			ETH_RSS_FRAG_IPV4 | ETH_RSS_NONFRAG_IPV4_OTHER;
	if (ENIC_SETTING(enic, RSSHASH_TCPIPV4))
		enic->flow_type_rss_offloads |= ETH_RSS_NONFRAG_IPV4_TCP;
	if (ENIC_SETTING(enic, RSSHASH_IPV6))
		/*
		 * The VIC adapter can perform RSS on IPv6 packets with and
		 * without extension headers. An IPv6 "fragment" is an IPv6
		 * packet with the fragment extension header.
		 */
		enic->flow_type_rss_offloads |= ETH_RSS_IPV6 |
			ETH_RSS_IPV6_EX | ETH_RSS_FRAG_IPV6 |
			ETH_RSS_NONFRAG_IPV6_OTHER;
	if (ENIC_SETTING(enic, RSSHASH_TCPIPV6))
		enic->flow_type_rss_offloads |= ETH_RSS_NONFRAG_IPV6_TCP |
			ETH_RSS_IPV6_TCP_EX;
	if (enic->udp_rss_weak)
		enic->flow_type_rss_offloads |=
			ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_NONFRAG_IPV6_UDP |
			ETH_RSS_IPV6_UDP_EX;
	if (ENIC_SETTING(enic, RSSHASH_UDPIPV4))
		enic->flow_type_rss_offloads |= ETH_RSS_NONFRAG_IPV4_UDP;
	if (ENIC_SETTING(enic, RSSHASH_UDPIPV6))
		enic->flow_type_rss_offloads |= ETH_RSS_NONFRAG_IPV6_UDP |
			ETH_RSS_IPV6_UDP_EX;

	/* Zero offloads if RSS is not enabled */
	if (!ENIC_SETTING(enic, RSS))
		enic->flow_type_rss_offloads = 0;

	enic->vxlan = ENIC_SETTING(enic, VXLAN) &&
		vnic_dev_capable_vxlan(enic->vdev);
	if (vnic_dev_capable_geneve(enic->vdev)) {
		dev_info(NULL, "Geneve with options offload available\n");
		enic->geneve_opt_avail = 1;
	}
	/*
	 * Default hardware capabilities. enic_dev_init() may add additional
	 * flags if it enables overlay offloads.
	 */
	enic->tx_queue_offload_capa = 0;
	enic->tx_offload_capa =
		enic->tx_queue_offload_capa |
		DEV_TX_OFFLOAD_MULTI_SEGS |
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO;
	enic->rx_offload_capa =
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_RSS_HASH;
	enic->tx_offload_mask =
		PKT_TX_IPV6 |
		PKT_TX_IPV4 |
		PKT_TX_VLAN |
		PKT_TX_IP_CKSUM |
		PKT_TX_L4_MASK |
		PKT_TX_TCP_SEG;

	return 0;
}

int enic_set_nic_cfg(struct enic *enic, uint8_t rss_default_cpu,
		     uint8_t rss_hash_type, uint8_t rss_hash_bits,
		     uint8_t rss_base_cpu, uint8_t rss_enable,
		     uint8_t tso_ipid_split_en, uint8_t ig_vlan_strip_en)
{
	enum vnic_devcmd_cmd cmd;
	uint64_t a0, a1;
	uint32_t nic_cfg;
	int wait = 1000;

	vnic_set_nic_cfg(&nic_cfg, rss_default_cpu,
		rss_hash_type, rss_hash_bits, rss_base_cpu,
		rss_enable, tso_ipid_split_en, ig_vlan_strip_en);

	a0 = nic_cfg;
	a1 = 0;
	cmd = enic->nic_cfg_chk ? CMD_NIC_CFG_CHK : CMD_NIC_CFG;
	return vnic_dev_cmd(enic->vdev, cmd, &a0, &a1, wait);
}

int enic_set_rss_key(struct enic *enic, dma_addr_t key_pa, uint64_t len)
{
	uint64_t a0 = (uint64_t)key_pa, a1 = len;
	int wait = 1000;

	return vnic_dev_cmd(enic->vdev, CMD_RSS_KEY, &a0, &a1, wait);
}

int enic_set_rss_cpu(struct enic *enic, dma_addr_t cpu_pa, uint64_t len)
{
	uint64_t a0 = (uint64_t)cpu_pa, a1 = len;
	int wait = 1000;

	return vnic_dev_cmd(enic->vdev, CMD_RSS_CPU, &a0, &a1, wait);
}

void enic_free_vnic_resources(struct enic *enic)
{
	unsigned int i;

	for (i = 0; i < enic->wq_count; i++)
		vnic_wq_free(&enic->wq[i]);
	for (i = 0; i < enic_vnic_rq_count(enic); i++)
		if (enic->rq[i].in_use)
			vnic_rq_free(&enic->rq[i]);
	for (i = 0; i < enic->cq_count; i++)
		vnic_cq_free(&enic->cq[i]);
	for (i = 0; i < enic->intr_count; i++)
		vnic_intr_free(&enic->intr[i]);
}

void enic_get_res_counts(struct enic *enic)
{
	enic->conf_wq_count = vnic_dev_get_res_count(enic->vdev, RES_TYPE_WQ);
	enic->conf_rq_count = vnic_dev_get_res_count(enic->vdev, RES_TYPE_RQ);
	enic->conf_cq_count = vnic_dev_get_res_count(enic->vdev, RES_TYPE_CQ);
	enic->conf_intr_count = vnic_dev_get_res_count(enic->vdev,
		RES_TYPE_INTR_CTRL);

	dev_info(enic_get_dev(enic),
		"vNIC resources avail: wq %d rq %d cq %d intr %d\n",
		enic->conf_wq_count, enic->conf_rq_count,
		enic->conf_cq_count, enic->conf_intr_count);
}
