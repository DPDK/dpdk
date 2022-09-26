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
