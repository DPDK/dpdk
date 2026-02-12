/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DEF_HW_H_
#define _NBL_DEF_HW_H_

#include "nbl_include.h"

#define NBL_HW_OPS_TBL_TO_OPS(hw_ops_tbl)	((hw_ops_tbl)->ops)
#define NBL_HW_OPS_TBL_TO_PRIV(hw_ops_tbl)	((hw_ops_tbl)->priv)

struct nbl_hw_ops {
	/* queue */
	void (*update_tail_ptr)(void *priv, u16 notify_qid, u16 tail_ptr);
	u8 *(*get_tail_ptr)(void *priv);

	/* mailbox */
	void (*config_mailbox_rxq)(void *priv, uint64_t dma_addr, int size_bwid);
	void (*config_mailbox_txq)(void *priv, uint64_t dma_addr, int size_bwid);
	void (*stop_mailbox_rxq)(void *priv);
	void (*stop_mailbox_txq)(void *priv);
	uint16_t (*get_mailbox_rx_tail_ptr)(void *priv);
	void (*update_mailbox_queue_tail_ptr)(void *priv, uint16_t tail_ptr, uint8_t txrx);
};

struct nbl_hw_ops_tbl {
	const struct nbl_hw_ops *ops;
	void *priv;
};

int nbl_hw_init_leonis_snic(void *adapter);
void nbl_hw_remove_leonis_snic(void *adapter);

#endif
