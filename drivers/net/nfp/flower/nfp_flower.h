/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_FLOWER_H_
#define _NFP_FLOWER_H_

/*
 * Flower fallback and ctrl path always adds and removes
 * 8 bytes of prepended data. Tx descriptors must point
 * to the correct packet data offset after metadata has
 * been added
 */
#define FLOWER_PKT_DATA_OFFSET 8

#define MAX_FLOWER_PHYPORTS 8
#define MAX_FLOWER_VFS 64

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
};

int nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev);
int nfp_secondary_init_app_fw_flower(struct nfp_cpp *cpp);
uint16_t nfp_flower_pf_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);
uint16_t nfp_flower_pf_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
int nfp_flower_pf_start(struct rte_eth_dev *dev);
int nfp_flower_pf_stop(struct rte_eth_dev *dev);

#endif /* _NFP_FLOWER_H_ */
