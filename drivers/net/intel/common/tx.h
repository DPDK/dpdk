/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _COMMON_INTEL_TX_H_
#define _COMMON_INTEL_TX_H_

#include <stdint.h>
#include <rte_mbuf.h>

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct ci_tx_entry {
	struct rte_mbuf *mbuf; /* mbuf associated with TX desc, if any. */
	uint16_t next_id; /* Index of next descriptor in ring. */
	uint16_t last_id; /* Index of last scattered descriptor. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue in vector Tx.
 */
struct ci_tx_entry_vec {
	struct rte_mbuf *mbuf; /* mbuf associated with TX desc, if any. */
};

#endif /* _COMMON_INTEL_TX_H_ */
