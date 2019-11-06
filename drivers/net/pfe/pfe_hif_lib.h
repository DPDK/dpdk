/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _PFE_HIF_LIB_H_
#define _PFE_HIF_LIB_H_

#include "pfe_hif.h"

#define HIF_CL_REQ_TIMEOUT	10
#define GFP_DMA_PFE 0

enum {
	REQUEST_CL_REGISTER = 0,
	REQUEST_CL_UNREGISTER,
	HIF_REQUEST_MAX
};

enum {
	/* Event to indicate that client rx queue is reached water mark level */
	EVENT_HIGH_RX_WM = 0,
	/* Event to indicate that, packet received for client */
	EVENT_RX_PKT_IND,
	/* Event to indicate that, packet tx done for client */
	EVENT_TXDONE_IND,
	HIF_EVENT_MAX
};

/*structure to store client queue info */

/*structure to store client queue info */
struct hif_client_rx_queue {
	struct rx_queue_desc *base;
	u32	size;
	u32	read_idx;
	u32	write_idx;
	u16	queue_id;
	u16	port_id;
	void   *priv;
};

struct hif_client_tx_queue {
	struct tx_queue_desc *base;
	u32	size;
	u32	read_idx;
	u32	write_idx;
	u32	tx_pending;
	unsigned long jiffies_last_packet;
	u32	nocpy_flag;
	u32	prev_tmu_tx_pkts;
	u32	done_tmu_tx_pkts;
	u16	queue_id;
	u16	port_id;
	void   *priv;
};

struct hif_client_s {
	int	id;
	unsigned int	tx_qn;
	unsigned int	rx_qn;
	void	*rx_qbase;
	void	*tx_qbase;
	int	tx_qsize;
	int	rx_qsize;
	int	cpu_id;
	int	port_id;
	struct hif_client_tx_queue tx_q[HIF_CLIENT_QUEUES_MAX];
	struct hif_client_rx_queue rx_q[HIF_CLIENT_QUEUES_MAX];
	int (*event_handler)(void *data, int event, int qno);
	unsigned long queue_mask[HIF_EVENT_MAX];
	struct pfe *pfe;
	void *priv;
};

/*
 * Client specific shared memory
 * It contains number of Rx/Tx queues, base addresses and queue sizes
 */
struct hif_client_shm {
	u32 ctrl; /*0-7: number of Rx queues, 8-15: number of tx queues */
	unsigned long rx_qbase; /*Rx queue base address */
	u32 rx_qsize; /*each Rx queue size, all Rx queues are of same size */
	unsigned long tx_qbase; /* Tx queue base address */
	u32 tx_qsize; /*each Tx queue size, all Tx queues are of same size */
};

/*Client shared memory ctrl bit description */
#define CLIENT_CTRL_RX_Q_CNT_OFST	0
#define CLIENT_CTRL_TX_Q_CNT_OFST	8
#define CLIENT_CTRL_RX_Q_CNT(ctrl)	(((ctrl) >> CLIENT_CTRL_RX_Q_CNT_OFST) \
						& 0xFF)
#define CLIENT_CTRL_TX_Q_CNT(ctrl)	(((ctrl) >> CLIENT_CTRL_TX_Q_CNT_OFST) \
						& 0xFF)

/*
 * Shared memory used to communicate between HIF driver and host/client drivers
 * Before starting the hif driver rx_buf_pool ans rx_buf_pool_cnt should be
 * initialized with host buffers and buffers count in the pool.
 * rx_buf_pool_cnt should be >= HIF_RX_DESC_NT.
 *
 */
struct hif_shm {
	u32 rx_buf_pool_cnt; /*Number of rx buffers available*/
	/*Rx buffers required to initialize HIF rx descriptors */
	struct rte_mempool *pool;
	void *rx_buf_pool[HIF_RX_DESC_NT];
	unsigned long g_client_status[2]; /*Global client status bit mask */
	/* Client specific shared memory */
	struct hif_client_shm client[HIF_CLIENTS_MAX];
};

#define CL_DESC_OWN	BIT(31)
/* This sets owner ship to HIF driver */
#define CL_DESC_LAST	BIT(30)
/* This indicates last packet for multi buffers handling */
#define CL_DESC_FIRST	BIT(29)
/* This indicates first packet for multi buffers handling */

#define CL_DESC_BUF_LEN(x)		((x) & 0xFFFF)
#define CL_DESC_FLAGS(x)		(((x) & 0xF) << 16)
#define CL_DESC_GET_FLAGS(x)		(((x) >> 16) & 0xF)

struct rx_queue_desc {
	void *data;
	u32	ctrl; /*0-15bit len, 16-20bit flags, 31bit owner*/
	u32	client_ctrl;
};

struct tx_queue_desc {
	void *data;
	u32	ctrl; /*0-15bit len, 16-20bit flags, 31bit owner*/
};

/* HIF Rx is not working properly for 2-byte aligned buffers and
 * ip_header should be 4byte aligned for better iperformance.
 * "ip_header = 64 + 6(hif_header) + 14 (MAC Header)" will be 4byte aligned.
 * In case HW parse support:
 * "ip_header = 64 + 6(hif_header) + 16 (parse) + 14 (MAC Header)" will be
 * 4byte aligned.
 */
#define PFE_HIF_SIZE		sizeof(struct hif_hdr)

#ifdef RTE_LIBRTE_PFE_SW_PARSE
#define PFE_PKT_HEADER_SZ	PFE_HIF_SIZE
#else
#define PFE_PKT_HEADER_SZ	(PFE_HIF_SIZE + sizeof(struct pfe_parse))
#endif

#define MAX_L2_HDR_SIZE		14	/* Not correct for VLAN/PPPoE */
#define MAX_L3_HDR_SIZE		20	/* Not correct for IPv6 */
#define MAX_L4_HDR_SIZE		60	/* TCP with maximum options */
#define MAX_HDR_SIZE		(MAX_L2_HDR_SIZE + MAX_L3_HDR_SIZE \
				 + MAX_L4_HDR_SIZE)
/* Used in page mode to clamp packet size to the maximum supported by the hif
 *hw interface (<16KiB)
 */
#define MAX_PFE_PKT_SIZE	16380UL

extern unsigned int emac_txq_cnt;

int pfe_hif_lib_init(struct pfe *pfe);
void pfe_hif_lib_exit(struct pfe *pfe);
int hif_lib_client_register(struct hif_client_s *client);
int hif_lib_client_unregister(struct  hif_client_s *client);
void hif_lib_xmit_pkt(struct hif_client_s *client, unsigned int qno,
			void *data, void *data1, unsigned int len,
			u32 client_ctrl, unsigned int flags, void *client_data);
void hif_lib_indicate_client(struct hif_client_s *client, int event, int data);
int hif_lib_event_handler_start(struct hif_client_s *client, int event, int
					data);
void *hif_lib_tx_get_next_complete(struct hif_client_s *client, int qno,
				   unsigned int *flags, int count);
int pfe_hif_shm_init(struct hif_shm *hif_shm, struct rte_mempool *mb_pool);
void pfe_hif_shm_clean(struct hif_shm *hif_shm);

int hif_lib_receive_pkt(struct hif_client_rx_queue *queue,
			     struct rte_mempool *pool,
			     struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts);

#endif /* _PFE_HIF_LIB_H_ */
