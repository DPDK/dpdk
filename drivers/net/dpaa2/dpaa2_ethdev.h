/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright (c) 2016 NXP. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DPAA2_ETHDEV_H
#define _DPAA2_ETHDEV_H

#include <mc/fsl_dpni.h>
#include <mc/fsl_mc_sys.h>

#define DPAA2_MIN_RX_BUF_SIZE 512
#define DPAA2_MAX_RX_PKT_LEN  10240 /*WRIOP support*/

#define MAX_TCS			DPNI_MAX_TC
#define MAX_RX_QUEUES		16
#define MAX_TX_QUEUES		16

/*default tc to be used for ,congestion, distribution etc configuration. */
#define DPAA2_DEF_TC		0

/* Threshold for a queue to *Enter* Congestion state.
 * It is set to 32KB
 */
#define CONG_ENTER_TX_THRESHOLD   (32 * 1024)

/* Threshold for a queue to *Exit* Congestion state.
 */
#define CONG_EXIT_TX_THRESHOLD    (24 * 1024)

/* RX queue tail drop threshold
 * currently considering 32 KB packets
 */
#define CONG_THRESHOLD_RX_Q  (32 * 1024)

/* Size of the input SMMU mapped memory required by MC */
#define DIST_PARAM_IOVA_SIZE 256

/* Enable TX Congestion control support
 * default is disable
 */
#define DPAA2_TX_CGR_SUPPORT	0x01

/* Disable RX tail drop, default is enable */
#define DPAA2_RX_TAILDROP_OFF	0x04

struct dpaa2_dev_priv {
	void *hw;
	int32_t hw_id;
	int32_t qdid;
	uint16_t token;
	uint8_t nb_tx_queues;
	uint8_t nb_rx_queues;
	void *rx_vq[MAX_RX_QUEUES];
	void *tx_vq[MAX_TX_QUEUES];

	struct dpaa2_bp_list *bp_list; /**<Attached buffer pool list */
	uint32_t options;
	uint16_t num_dist_per_tc[MAX_TCS];
	uint8_t max_mac_filters;
	uint8_t max_vlan_filters;
	uint8_t num_tc;
	uint8_t flags; /*dpaa2 config flags */
};

int dpaa2_setup_flow_dist(struct rte_eth_dev *eth_dev,
			  uint32_t req_dist_set);

int dpaa2_remove_flow_dist(struct rte_eth_dev *eth_dev,
			   uint8_t tc_index);

int dpaa2_attach_bp_list(struct dpaa2_dev_priv *priv, void *blist);

uint16_t dpaa2_dev_prefetch_rx(void *queue, struct rte_mbuf **bufs,
			       uint16_t nb_pkts);
uint16_t dpaa2_dev_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts);
uint16_t dummy_dev_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts);
#endif /* _DPAA2_ETHDEV_H */
