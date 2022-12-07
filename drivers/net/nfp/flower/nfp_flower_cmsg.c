/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include "../nfpcore/nfp_nsp.h"
#include "../nfp_logs.h"
#include "../nfp_common.h"
#include "nfp_flower.h"
#include "nfp_flower_cmsg.h"
#include "nfp_flower_ctrl.h"
#include "nfp_flower_representor.h"

static void *
nfp_flower_cmsg_init(struct rte_mbuf *m,
		enum nfp_flower_cmsg_type type,
		uint32_t size)
{
	char *pkt;
	uint32_t data;
	uint32_t new_size = size;
	struct nfp_flower_cmsg_hdr *hdr;

	pkt = rte_pktmbuf_mtod(m, char *);
	PMD_DRV_LOG(DEBUG, "flower_cmsg_init using pkt at %p", pkt);

	data = rte_cpu_to_be_32(NFP_NET_META_PORTID);
	rte_memcpy(pkt, &data, 4);
	pkt += 4;
	new_size += 4;

	/* First the metadata as flower requires it */
	data = rte_cpu_to_be_32(NFP_META_PORT_ID_CTRL);
	rte_memcpy(pkt, &data, 4);
	pkt += 4;
	new_size += 4;

	/* Now the ctrl header */
	hdr = (struct nfp_flower_cmsg_hdr *)pkt;
	hdr->pad     = 0;
	hdr->type    = type;
	hdr->version = NFP_FLOWER_CMSG_VER1;

	pkt = (char *)hdr + NFP_FLOWER_CMSG_HLEN;
	new_size += NFP_FLOWER_CMSG_HLEN;

	m->pkt_len = new_size;
	m->data_len = m->pkt_len;

	return pkt;
}

static void
nfp_flower_cmsg_mac_repr_init(struct rte_mbuf *mbuf, int num_ports)
{
	uint32_t size;
	struct nfp_flower_cmsg_mac_repr *msg;
	enum nfp_flower_cmsg_type type = NFP_FLOWER_CMSG_TYPE_MAC_REPR;

	size = sizeof(*msg) + (num_ports * sizeof(msg->ports[0]));
	msg = nfp_flower_cmsg_init(mbuf, type, size);
	memset(msg->reserved, 0, sizeof(msg->reserved));
	msg->num_ports = num_ports;
}

static void
nfp_flower_cmsg_mac_repr_fill(struct rte_mbuf *m,
		unsigned int idx,
		unsigned int nbi,
		unsigned int nbi_port,
		unsigned int phys_port)
{
	struct nfp_flower_cmsg_mac_repr *msg;

	msg = (struct nfp_flower_cmsg_mac_repr *)nfp_flower_cmsg_get_data(m);
	msg->ports[idx].idx       = idx;
	msg->ports[idx].info      = nbi & NFP_FLOWER_CMSG_MAC_REPR_NBI;
	msg->ports[idx].nbi_port  = nbi_port;
	msg->ports[idx].phys_port = phys_port;
}

int
nfp_flower_cmsg_mac_repr(struct nfp_app_fw_flower *app_fw_flower)
{
	int i;
	uint16_t cnt;
	unsigned int nbi;
	unsigned int nbi_port;
	unsigned int phys_port;
	struct rte_mbuf *mbuf;
	struct nfp_eth_table *nfp_eth_table;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(ERR, "Could not allocate mac repr cmsg");
		return -ENOMEM;
	}

	nfp_flower_cmsg_mac_repr_init(mbuf, app_fw_flower->num_phyport_reprs);

	/* Fill in the mac repr cmsg */
	nfp_eth_table = app_fw_flower->pf_hw->pf_dev->nfp_eth_table;
	for (i = 0; i < app_fw_flower->num_phyport_reprs; i++) {
		nbi = nfp_eth_table->ports[i].nbi;
		nbi_port = nfp_eth_table->ports[i].base;
		phys_port = nfp_eth_table->ports[i].index;

		nfp_flower_cmsg_mac_repr_fill(mbuf, i, nbi, nbi_port, phys_port);
	}

	/* Send the cmsg via the ctrl vNIC */
	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_repr_reify(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_flower_representor *repr)
{
	uint16_t cnt;
	struct rte_mbuf *mbuf;
	struct nfp_flower_cmsg_port_reify *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "alloc mbuf for repr reify failed");
		return -ENOMEM;
	}

	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_PORT_REIFY, sizeof(*msg));
	msg->portnum  = rte_cpu_to_be_32(repr->port_id);
	msg->reserved = 0;
	msg->info     = rte_cpu_to_be_16(1);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_port_mod(struct nfp_app_fw_flower *app_fw_flower,
		uint32_t port_id, bool carrier_ok)
{
	uint16_t cnt;
	struct rte_mbuf *mbuf;
	struct nfp_flower_cmsg_port_mod *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "alloc mbuf for repr portmod failed");
		return -ENOMEM;
	}

	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_PORT_MOD, sizeof(*msg));
	msg->portnum  = rte_cpu_to_be_32(port_id);
	msg->reserved = 0;
	msg->info     = carrier_ok;
	msg->mtu      = 9000;

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_flow_delete(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *flow)
{
	char *msg;
	uint16_t cnt;
	uint32_t msg_len;
	struct rte_mbuf *mbuf;
	struct nfp_fl_rule_metadata *nfp_flow_meta;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for flow delete.");
		return -ENOMEM;
	}

	/* Copy the flow to mbuf */
	nfp_flow_meta = flow->payload.meta;
	msg_len = (nfp_flow_meta->key_len + nfp_flow_meta->mask_len +
			nfp_flow_meta->act_len) << NFP_FL_LW_SIZ;
	msg_len += sizeof(struct nfp_fl_rule_metadata);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_FLOW_DEL, msg_len);
	rte_memcpy(msg, flow->payload.meta, msg_len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_flow_add(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *flow)
{
	char *msg;
	uint16_t cnt;
	uint32_t msg_len;
	struct rte_mbuf *mbuf;
	struct nfp_fl_rule_metadata *nfp_flow_meta;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for flow add.");
		return -ENOMEM;
	}

	/* copy the flow to mbuf */
	nfp_flow_meta = flow->payload.meta;
	msg_len = (nfp_flow_meta->key_len + nfp_flow_meta->mask_len +
			nfp_flow_meta->act_len) << NFP_FL_LW_SIZ;
	msg_len += sizeof(struct nfp_fl_rule_metadata);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_FLOW_ADD, msg_len);
	rte_memcpy(msg, flow->payload.meta, msg_len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_tun_neigh_v4_rule(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_flower_cmsg_tun_neigh_v4 *payload)
{
	uint16_t cnt;
	size_t msg_len;
	struct rte_mbuf *mbuf;
	struct nfp_flower_cmsg_tun_neigh_v4 *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for v4 tun neigh");
		return -ENOMEM;
	}

	msg_len = sizeof(struct nfp_flower_cmsg_tun_neigh_v4);
	if (!nfp_flower_support_decap_v2(app_fw_flower))
		msg_len -= sizeof(struct nfp_flower_tun_neigh_ext);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_TUN_NEIGH, msg_len);
	memcpy(msg, payload, msg_len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_tun_neigh_v6_rule(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_flower_cmsg_tun_neigh_v6 *payload)
{
	uint16_t cnt;
	size_t msg_len;
	struct rte_mbuf *mbuf;
	struct nfp_flower_cmsg_tun_neigh_v6 *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for v6 tun neigh");
		return -ENOMEM;
	}

	msg_len = sizeof(struct nfp_flower_cmsg_tun_neigh_v6);
	if (!nfp_flower_support_decap_v2(app_fw_flower))
		msg_len -= sizeof(struct nfp_flower_tun_neigh_ext);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_TUN_NEIGH_V6, msg_len);
	memcpy(msg, payload, msg_len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_tun_off_v4(struct nfp_app_fw_flower *app_fw_flower)
{
	uint16_t cnt;
	uint32_t count = 0;
	struct rte_mbuf *mbuf;
	struct nfp_flow_priv *priv;
	struct nfp_ipv4_addr_entry *entry;
	struct nfp_flower_cmsg_tun_ipv4_addr *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for v4 tun addr");
		return -ENOMEM;
	}

	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_TUN_IPS, sizeof(*msg));

	priv = app_fw_flower->flow_priv;
	rte_spinlock_lock(&priv->ipv4_off_lock);
	LIST_FOREACH(entry, &priv->ipv4_off_list, next) {
		if (count >= NFP_FL_IPV4_ADDRS_MAX) {
			rte_spinlock_unlock(&priv->ipv4_off_lock);
			PMD_DRV_LOG(ERR, "IPv4 offload exceeds limit.");
			return -ERANGE;
		}
		msg->ipv4_addr[count] = entry->ipv4_addr;
		count++;
	}
	msg->count = rte_cpu_to_be_32(count);
	rte_spinlock_unlock(&priv->ipv4_off_lock);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_tun_off_v6(struct nfp_app_fw_flower *app_fw_flower)
{
	uint16_t cnt;
	uint32_t count = 0;
	struct rte_mbuf *mbuf;
	struct nfp_flow_priv *priv;
	struct nfp_ipv6_addr_entry *entry;
	struct nfp_flower_cmsg_tun_ipv6_addr *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for v6 tun addr");
		return -ENOMEM;
	}

	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_TUN_IPS_V6, sizeof(*msg));

	priv = app_fw_flower->flow_priv;
	rte_spinlock_lock(&priv->ipv6_off_lock);
	LIST_FOREACH(entry, &priv->ipv6_off_list, next) {
		if (count >= NFP_FL_IPV6_ADDRS_MAX) {
			rte_spinlock_unlock(&priv->ipv6_off_lock);
			PMD_DRV_LOG(ERR, "IPv6 offload exceeds limit.");
			return -ERANGE;
		}
		memcpy(&msg->ipv6_addr[count * 16], entry->ipv6_addr, 16UL);
		count++;
	}
	msg->count = rte_cpu_to_be_32(count);
	rte_spinlock_unlock(&priv->ipv6_off_lock);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_pre_tunnel_rule(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		uint16_t mac_idx,
		bool is_del)
{
	uint16_t cnt;
	struct rte_mbuf *mbuf;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_flower_cmsg_pre_tun_rule *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for pre tunnel rule");
		return -ENOMEM;
	}

	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_PRE_TUN_RULE, sizeof(*msg));

	meta_tci = (struct nfp_flower_meta_tci *)((char *)nfp_flow_meta +
			sizeof(struct nfp_fl_rule_metadata));
	if (meta_tci->tci)
		msg->vlan_tci = meta_tci->tci;
	else
		msg->vlan_tci = 0xffff;

	if (is_del)
		msg->flags = rte_cpu_to_be_32(NFP_TUN_PRE_TUN_RULE_DEL);

	msg->port_idx = rte_cpu_to_be_16(mac_idx);
	msg->host_ctx_id = nfp_flow_meta->host_ctx_id;

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_tun_mac_rule(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_ether_addr *mac,
		uint16_t mac_idx,
		bool is_del)
{
	uint16_t cnt;
	struct rte_mbuf *mbuf;
	struct nfp_flower_cmsg_tun_mac *msg;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for tunnel mac");
		return -ENOMEM;
	}

	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_TUN_MAC, sizeof(*msg));

	msg->count = rte_cpu_to_be_16(1);
	msg->index = rte_cpu_to_be_16(mac_idx);
	rte_ether_addr_copy(mac, &msg->addr);
	if (is_del)
		msg->flags = rte_cpu_to_be_16(NFP_TUN_MAC_OFFLOAD_DEL_FLAG);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_qos_add(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_profile_conf *conf)
{
	char *msg;
	uint16_t cnt;
	uint32_t len;
	struct rte_mbuf *mbuf;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for qos add");
		return -ENOMEM;
	}

	len = sizeof(struct nfp_profile_conf);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_QOS_MOD, len);
	rte_memcpy(msg, conf, len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_qos_delete(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_profile_conf *conf)
{
	char *msg;
	uint16_t cnt;
	uint32_t len;
	struct rte_mbuf *mbuf;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for qos delete");
		return -ENOMEM;
	}

	len = sizeof(struct nfp_profile_conf);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_QOS_DEL, len);
	rte_memcpy(msg, conf, len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}

int
nfp_flower_cmsg_qos_stats(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_cfg_head *head)
{
	char *msg;
	uint16_t cnt;
	uint32_t len;
	struct rte_mbuf *mbuf;

	mbuf = rte_pktmbuf_alloc(app_fw_flower->ctrl_pktmbuf_pool);
	if (mbuf == NULL) {
		PMD_DRV_LOG(DEBUG, "Failed to alloc mbuf for qos stats");
		return -ENOMEM;
	}

	len = sizeof(struct nfp_cfg_head);
	msg = nfp_flower_cmsg_init(mbuf, NFP_FLOWER_CMSG_TYPE_QOS_STATS, len);
	rte_memcpy(msg, head, len);

	cnt = nfp_flower_ctrl_vnic_xmit(app_fw_flower, mbuf);
	if (cnt == 0) {
		PMD_DRV_LOG(ERR, "Send cmsg through ctrl vnic failed.");
		rte_pktmbuf_free(mbuf);
		return -EIO;
	}

	return 0;
}
