/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_FLOWER_H_
#define _NFP_FLOWER_H_

#include "../nfp_common.h"

/* Extra features bitmap. */
#define NFP_FL_FEATS_GENEVE             RTE_BIT64(0)
#define NFP_FL_NBI_MTU_SETTING          RTE_BIT64(1)
#define NFP_FL_FEATS_GENEVE_OPT         RTE_BIT64(2)
#define NFP_FL_FEATS_VLAN_PCP           RTE_BIT64(3)
#define NFP_FL_FEATS_VF_RLIM            RTE_BIT64(4)
#define NFP_FL_FEATS_FLOW_MOD           RTE_BIT64(5)
#define NFP_FL_FEATS_PRE_TUN_RULES      RTE_BIT64(6)
#define NFP_FL_FEATS_IPV6_TUN           RTE_BIT64(7)
#define NFP_FL_FEATS_VLAN_QINQ          RTE_BIT64(8)
#define NFP_FL_FEATS_QOS_PPS            RTE_BIT64(9)
#define NFP_FL_FEATS_QOS_METER          RTE_BIT64(10)
#define NFP_FL_FEATS_DECAP_V2           RTE_BIT64(11)
#define NFP_FL_FEATS_HOST_ACK           RTE_BIT64(31)

/*
 * Flower fallback and ctrl path always adds and removes
 * 8 bytes of prepended data. Tx descriptors must point
 * to the correct packet data offset after metadata has
 * been added
 */
#define FLOWER_PKT_DATA_OFFSET (NFP_NET_META_HEADER_SIZE + NFP_NET_META_FIELD_SIZE)

#define MAX_FLOWER_PHYPORTS 8
#define MAX_FLOWER_VFS 64

struct nfp_app_fw_flower;

/* The function pointers for different NFD version */
struct nfp_flower_nfd_func {
	/** Function used to add metadata into pkt. */
	uint32_t (*pkt_add_metadata_t)(struct rte_mbuf *mbuf,
		uint32_t port_id);
	/** Function used to xmit pkt through ctrl vnic. */
	uint16_t (*ctrl_vnic_xmit_t)(struct nfp_app_fw_flower *app_fw_flower,
			struct rte_mbuf *mbuf);
	/** Function used to xmit pkts through PF. */
	uint16_t (*pf_xmit_t)(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
};

/* The flower application's private structure */
struct nfp_app_fw_flower {
	/* switch domain for this app */
	uint16_t switch_domain_id;

	/* Number of VF representors */
	uint8_t num_vf_reprs;

	/* Number of phyport representors */
	uint8_t num_phyport_reprs;

	/* Pointer to the PF vNIC */
	struct nfp_net_hw *pf_hw;

	/* Pointer to a mempool for the ctrlvNIC */
	struct rte_mempool *ctrl_pktmbuf_pool;

	/* Pointer to the ctrl vNIC */
	struct nfp_net_hw *ctrl_hw;

	/* Ctrl vNIC Rx counter */
	uint64_t ctrl_vnic_rx_count;

	/* Ctrl vNIC Tx counter */
	uint64_t ctrl_vnic_tx_count;

	/* Array of phyport representors */
	struct nfp_flower_representor *phy_reprs[MAX_FLOWER_PHYPORTS];

	/* Array of VF representors */
	struct nfp_flower_representor *vf_reprs[MAX_FLOWER_VFS];

	/* PF representor */
	struct nfp_flower_representor *pf_repr;

	/* service id of ctrl vnic service */
	uint32_t ctrl_vnic_id;

	/* Flower extra features */
	uint64_t ext_features;

	struct nfp_flow_priv *flow_priv;
	struct nfp_mtr_priv *mtr_priv;

	/* Function pointers for different NFD version */
	struct nfp_flower_nfd_func nfd_func;
};

static inline bool
nfp_flower_support_decap_v2(const struct nfp_app_fw_flower *app_fw_flower)
{
	return app_fw_flower->ext_features & NFP_FL_FEATS_DECAP_V2;
}

int nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev);
int nfp_secondary_init_app_fw_flower(struct nfp_cpp *cpp);
bool nfp_flower_pf_dispatch_pkts(struct nfp_net_hw *hw,
		struct rte_mbuf *mbuf,
		uint32_t port_id);
uint16_t nfp_flower_pf_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
int nfp_flower_pf_start(struct rte_eth_dev *dev);
int nfp_flower_pf_stop(struct rte_eth_dev *dev);
uint32_t nfp_flower_pkt_add_metadata(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf, uint32_t port_id);

#endif /* _NFP_FLOWER_H_ */
