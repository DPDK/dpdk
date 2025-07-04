/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Arm Limited.
 */

#include <stdint.h>
#include <ethdev_driver.h>

#include "base/i40e_prototype.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

#include "i40e_rxtx_vec_common.h"

#include "../common/recycle_mbufs.h"

void
i40e_recycle_rx_descriptors_refill_vec(void *rx_queue, uint16_t nb_mbufs)
{
	ci_rx_recycle_mbufs(rx_queue, nb_mbufs);
}

uint16_t
i40e_recycle_tx_mbufs_reuse_vec(void *tx_queue,
	struct rte_eth_recycle_rxq_info *recycle_rxq_info)
{
	struct ci_tx_queue *txq = tx_queue;

	return ci_tx_recycle_mbufs(txq, i40e_tx_desc_done, recycle_rxq_info);
}
