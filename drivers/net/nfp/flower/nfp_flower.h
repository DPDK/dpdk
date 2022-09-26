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

/* The flower application's private structure */
struct nfp_app_fw_flower {
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
};

int nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev);
int nfp_secondary_init_app_fw_flower(struct nfp_cpp *cpp);

#endif /* _NFP_FLOWER_H_ */
