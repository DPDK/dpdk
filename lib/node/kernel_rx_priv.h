/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#ifndef __KERNEL_RX_PRIV_H__
#define __KERNEL_RX_PRIV_H__

#define KERN_RX_CACHE_COUNT 64

typedef struct kernel_rx_info {
	struct rte_mbuf *rx_bufs[KERN_RX_CACHE_COUNT];
	uint16_t node_next;
	uint16_t idx;
	uint16_t cnt;
	int sock;
} kernel_rx_info_t;

/* kernel_rx node context structure */
typedef struct kernel_rx_node_ctx {
	struct rte_mempool *pktmbuf_pool;
	kernel_rx_info_t *recv_info;
} kernel_rx_node_ctx_t;

/* kernel_rx node list element structure */
typedef struct kernel_rx_node_elem {
	struct kernel_rx_node_elem *next; /* Pointer to the next node element. */
	struct kernel_rx_node_ctx ctx;    /* kernel_rx node context. */
	rte_node_t nid;			  /* Node identifier of the kernel_rx node. */
} kernel_rx_node_elem_t;

enum kernel_rx_next_nodes {
	KERNEL_RX_NEXT_PKT_CLS,
	KERNEL_RX_NEXT_IP4_LOOKUP,
	KERNEL_RX_NEXT_MAX,
};

/* kernel_rx node main structure */
struct kernel_rx_node_main {
	kernel_rx_node_elem_t *head; /* Pointer to the head node element. */
};

/* Get the pointer of kernel_rx node data */
struct kernel_rx_node_main *kernel_rx_node_data_get(void);

/* Get the pointer of kernel_rx node register structure */
struct rte_node_register *kernel_rx_node_get(void);

#endif /* __KERNEL_RX_PRIV_H__ */
