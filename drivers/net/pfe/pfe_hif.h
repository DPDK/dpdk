/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _PFE_HIF_H_
#define _PFE_HIF_H_

#define HIF_CLIENT_QUEUES_MAX	16
#define HIF_RX_PKT_MIN_SIZE RTE_CACHE_LINE_SIZE
/*
 * HIF_TX_DESC_NT value should be always greater than 4,
 * Otherwise HIF_TX_POLL_MARK will become zero.
 */
#define HIF_RX_DESC_NT		64
#define HIF_TX_DESC_NT		2048

#define HIF_FIRST_BUFFER	BIT(0)
#define HIF_LAST_BUFFER		BIT(1)
#define HIF_DONT_DMA_MAP	BIT(2)
#define HIF_DATA_VALID		BIT(3)
#define HIF_TSO			BIT(4)

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

struct hif_hdr {
	u8 client_id;
	u8 q_num;
	u16 client_ctrl;
	u16 client_ctrl1;
};

struct __hif_hdr {
	union {
		struct hif_hdr hdr;
		u32 word[2];
	};
};

struct hif_ipsec_hdr {
	u16	sa_handle[2];
} __packed;

struct pfe_parse {
	unsigned int packet_type;
	uint16_t hash;
	uint16_t parse_incomplete;
	unsigned long long ol_flags;
};

/*  HIF_CTRL_TX... defines */
#define HIF_CTRL_TX_CHECKSUM		BIT(2)

/*  HIF_CTRL_RX... defines */
#define HIF_CTRL_RX_OFFSET_OFST         (24)
#define HIF_CTRL_RX_CHECKSUMMED		BIT(2)
#define HIF_CTRL_RX_CONTINUED		BIT(1)

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

void hif_xmit_pkt(struct pfe_hif *hif, unsigned int client_id, unsigned int
			q_no, void *data, u32 len, unsigned int flags);
void hif_process_client_req(struct pfe_hif *hif, int req, int data1, int
				data2);
int pfe_hif_init(struct pfe *pfe);
void pfe_hif_exit(struct pfe *pfe);
void pfe_hif_rx_idle(struct pfe_hif *hif);
int pfe_hif_rx_process(struct pfe *pfe, int budget);
int pfe_hif_init_buffers(struct pfe_hif *hif);
void pfe_tx_do_cleanup(struct pfe *pfe);

#define __memcpy8(dst, src)		memcpy(dst, src, 8)
#define __memcpy12(dst, src)		memcpy(dst, src, 12)
#define __memcpy(dst, src, len)		memcpy(dst, src, len)

#endif /* _PFE_HIF_H_ */
