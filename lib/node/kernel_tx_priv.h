/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#ifndef __KERNEL_TX_PRIV_H__
#define __KERNEL_TX_PRIV_H__

/* kernel_tx node context structure. */
typedef struct kernel_tx_node_ctx {
	int sock;
} kernel_tx_node_ctx_t;

/* Get the pointer to kernel_tx node register structure */
struct rte_node_register *kernel_tx_node_get(void);

#endif /* __KERNEL_TX_PRIV_H__ */
