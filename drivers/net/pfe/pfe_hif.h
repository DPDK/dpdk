/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 NXP
 */

#ifndef _PFE_HIF_H_
#define _PFE_HIF_H_

#define HIF_CLIENT_QUEUES_MAX	16
#define HIF_RX_PKT_MIN_SIZE RTE_CACHE_LINE_SIZE
/*
 * HIF_TX_DESC_NT value should be always greter than 4,
 * Otherwise HIF_TX_POLL_MARK will become zero.
 */
#define HIF_RX_DESC_NT		64
#define HIF_TX_DESC_NT		2048

enum {
	PFE_CL_GEM0 = 0,
	PFE_CL_GEM1,
	HIF_CLIENTS_MAX
};

/*structure to store client queue info */
struct hif_rx_queue {
	struct rx_queue_desc *base;
	u32	size;
	u32	write_idx;
};

struct hif_tx_queue {
	struct tx_queue_desc *base;
	u32	size;
	u32	ack_idx;
};

/*Structure to store the client info */
struct hif_client {
	unsigned int	rx_qn;
	struct hif_rx_queue	rx_q[HIF_CLIENT_QUEUES_MAX];
	unsigned int	tx_qn;
	struct hif_tx_queue	tx_q[HIF_CLIENT_QUEUES_MAX];
};

/*HIF hardware buffer descriptor */
struct hif_desc {
	u32 ctrl;
	u32 status;
	u32 data;
	u32 next;
};

struct __hif_desc {
	u32 ctrl;
	u32 status;
	u32 data;
};

struct hif_desc_sw {
	dma_addr_t data;
	u16 len;
	u8 client_id;
	u8 q_no;
	u16 flags;
};

struct pfe_hif {
	/* To store registered clients in hif layer */
	struct hif_client client[HIF_CLIENTS_MAX];
	struct hif_shm *shm;

	void	*descr_baseaddr_v;
	unsigned long	descr_baseaddr_p;

	struct hif_desc *rx_base;
	u32	rx_ring_size;
	u32	rxtoclean_index;
	void	*rx_buf_addr[HIF_RX_DESC_NT];
	void	*rx_buf_vaddr[HIF_RX_DESC_NT];
	int	rx_buf_len[HIF_RX_DESC_NT];
	unsigned int qno;
	unsigned int client_id;
	unsigned int client_ctrl;
	unsigned int started;
	unsigned int setuped;

	struct hif_desc *tx_base;
	u32	tx_ring_size;
	u32	txtosend;
	u32	txtoclean;
	u32	txavail;
	u32	txtoflush;
	struct hif_desc_sw tx_sw_queue[HIF_TX_DESC_NT];
	int32_t	epoll_fd; /**< File descriptor created for interrupt polling */

/* tx_lock synchronizes hif packet tx as well as pfe_hif structure access */
	rte_spinlock_t tx_lock;
/* lock synchronizes hif rx queue processing */
	rte_spinlock_t lock;
	struct rte_device *dev;
};

int pfe_hif_init(struct pfe *pfe);
void pfe_hif_exit(struct pfe *pfe);
void pfe_hif_rx_idle(struct pfe_hif *hif);

#endif /* _PFE_HIF_H_ */
