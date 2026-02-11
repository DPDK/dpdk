/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef _COMMON_INTEL_TX_SCALAR_H_
#define _COMMON_INTEL_TX_SCALAR_H_

#include <stdint.h>
#include <rte_byteorder.h>

/* depends on common Tx definitions. */
#include "tx.h"

/*
 * Common transmit descriptor cleanup function for Intel drivers.
 *
 * Returns:
 *   0 on success
 *  -1 if cleanup cannot proceed (descriptors not yet processed by HW)
 */
static __rte_always_inline int
ci_tx_xmit_cleanup(struct ci_tx_queue *txq)
{
	struct ci_tx_entry *sw_ring = txq->sw_ring;
	volatile struct ci_tx_desc *txd = txq->ci_tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	/* Determine the last descriptor needing to be cleaned */
	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	/* Check if descriptor is done */
	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if ((txd[desc_to_clean_to].cmd_type_offset_bsz & rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) !=
			rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE))
		return -1;

	/* Figure out how many descriptors will be cleaned */
	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) + desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to - last_desc_cleaned);

	/* The last descriptor to clean is done, so that means all the
	 * descriptors from the last descriptor that was cleaned
	 * up to the last descriptor with the RS bit set
	 * are done. Only reset the threshold descriptor.
	 */
	txd[desc_to_clean_to].cmd_type_offset_bsz = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	return 0;
}

#endif /* _COMMON_INTEL_TX_SCALAR_H_ */
