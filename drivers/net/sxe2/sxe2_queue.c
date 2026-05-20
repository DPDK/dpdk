/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include "sxe2_ethdev.h"
#include "sxe2_queue.h"
#include "sxe2_common_log.h"

void sxe2_sw_queue_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_queue_caps *q_caps)
{
	adapter->q_ctxt.qp_cnt_assign = q_caps->queues_cnt;
	adapter->q_ctxt.base_idx_in_pf = q_caps->base_idx_in_pf;
}

int32_t sxe2_queues_init(struct rte_eth_dev *dev)
{
	int32_t ret = 0;
	uint16_t buf_size;
	uint16_t frame_size;
	struct sxe2_rx_queue *rxq;
	uint16_t nb_rxq;

	frame_size = dev->data->mtu + SXE2_ETH_OVERHEAD;
	for (nb_rxq = 0; nb_rxq < dev->data->nb_rx_queues; nb_rxq++) {
		rxq = dev->data->rx_queues[nb_rxq];
		if (!rxq)
			continue;

		buf_size = rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
		rxq->rx_buf_len = RTE_ALIGN_FLOOR(buf_size, (1 << SXE2_RXQ_CTX_DBUFF_SHIFT));
		rxq->rx_buf_len = RTE_MIN(rxq->rx_buf_len, SXE2_RX_MAX_DATA_BUF_SIZE);
		if (frame_size > rxq->rx_buf_len)
			dev->data->scattered_rx = 1;
	}

	return ret;
}
