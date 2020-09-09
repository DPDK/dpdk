/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Hisilicon Limited.
 */

#include <rte_io.h>
#include <rte_ethdev_driver.h>

#include "hns3_ethdev.h"
#include "hns3_rxtx.h"
#include "hns3_rxtx_vec.h"

#if defined RTE_ARCH_ARM64
#include "hns3_rxtx_vec_neon.h"
#endif

int
hns3_tx_check_vec_support(struct rte_eth_dev *dev)
{
	struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;

	/* Only support DEV_TX_OFFLOAD_MBUF_FAST_FREE */
	if (txmode->offloads != DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		return -ENOTSUP;

	return 0;
}

uint16_t
hns3_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct hns3_tx_queue *txq = (struct hns3_tx_queue *)tx_queue;
	uint16_t nb_tx = 0;

	while (nb_pkts) {
		uint16_t ret, new_burst;

		new_burst = RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = hns3_xmit_fixed_burst_vec(tx_queue, &tx_pkts[nb_tx],
						new_burst);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < new_burst)
			break;
	}

	return nb_tx;
}
