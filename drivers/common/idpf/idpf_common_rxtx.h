/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_RXTX_H_
#define _IDPF_COMMON_RXTX_H_

#include <rte_mbuf.h>
#include <rte_mbuf_ptype.h>
#include <rte_mbuf_core.h>

#include "idpf_common_device.h"

#define IDPF_RX_MAX_BURST		32

#define IDPF_RX_OFFLOAD_IPV4_CKSUM		RTE_BIT64(1)
#define IDPF_RX_OFFLOAD_UDP_CKSUM		RTE_BIT64(2)
#define IDPF_RX_OFFLOAD_TCP_CKSUM		RTE_BIT64(3)
#define IDPF_RX_OFFLOAD_OUTER_IPV4_CKSUM	RTE_BIT64(6)
#define IDPF_RX_OFFLOAD_TIMESTAMP		RTE_BIT64(14)

#define IDPF_TX_OFFLOAD_IPV4_CKSUM       RTE_BIT64(1)
#define IDPF_TX_OFFLOAD_UDP_CKSUM        RTE_BIT64(2)
#define IDPF_TX_OFFLOAD_TCP_CKSUM        RTE_BIT64(3)
#define IDPF_TX_OFFLOAD_SCTP_CKSUM       RTE_BIT64(4)
#define IDPF_TX_OFFLOAD_TCP_TSO          RTE_BIT64(5)
#define IDPF_TX_OFFLOAD_MULTI_SEGS       RTE_BIT64(15)
#define IDPF_TX_OFFLOAD_MBUF_FAST_FREE   RTE_BIT64(16)

struct idpf_rx_stats {
	uint64_t mbuf_alloc_failed;
};

struct idpf_rx_queue {
	struct idpf_adapter *adapter;   /* the adapter this queue belongs to */
	struct rte_mempool *mp;         /* mbuf pool to populate Rx ring */
	const struct rte_memzone *mz;   /* memzone for Rx ring */
	volatile void *rx_ring;
	struct rte_mbuf **sw_ring;      /* address of SW ring */
	uint64_t rx_ring_phys_addr;     /* Rx ring DMA address */

	uint16_t nb_rx_desc;            /* ring length */
	uint16_t rx_tail;               /* current value of tail */
	volatile uint8_t *qrx_tail;     /* register address of tail */
	uint16_t rx_free_thresh;        /* max free RX desc to hold */
	uint16_t nb_rx_hold;            /* number of held free RX desc */
	struct rte_mbuf *pkt_first_seg; /* first segment of current packet */
	struct rte_mbuf *pkt_last_seg;  /* last segment of current packet */
	struct rte_mbuf fake_mbuf;      /* dummy mbuf */

	/* used for VPMD */
	uint16_t rxrearm_nb;       /* number of remaining to be re-armed */
	uint16_t rxrearm_start;    /* the idx we start the re-arming from */
	uint64_t mbuf_initializer; /* value to init mbufs */

	uint16_t rx_nb_avail;
	uint16_t rx_next_avail;

	uint16_t port_id;       /* device port ID */
	uint16_t queue_id;      /* Rx queue index */
	uint16_t rx_buf_len;    /* The packet buffer size */
	uint16_t rx_hdr_len;    /* The header buffer size */
	uint16_t max_pkt_len;   /* Maximum packet length */
	uint8_t rxdid;

	bool q_set;             /* if rx queue has been configured */
	bool q_started;         /* if rx queue has been started */
	bool rx_deferred_start; /* don't start this queue in dev start */
	const struct idpf_rxq_ops *ops;

	struct idpf_rx_stats rx_stats;

	/* only valid for split queue mode */
	uint8_t expected_gen_id;
	struct idpf_rx_queue *bufq1;
	struct idpf_rx_queue *bufq2;

	uint64_t offloads;
	uint32_t hw_register_set;
};

struct idpf_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

/* Structure associated with each TX queue. */
struct idpf_tx_queue {
	const struct rte_memzone *mz;		/* memzone for Tx ring */
	volatile struct idpf_flex_tx_desc *tx_ring;	/* Tx ring virtual address */
	volatile union {
		struct idpf_flex_tx_sched_desc *desc_ring;
		struct idpf_splitq_tx_compl_desc *compl_ring;
	};
	uint64_t tx_ring_phys_addr;		/* Tx ring DMA address */
	struct idpf_tx_entry *sw_ring;		/* address array of SW ring */

	uint16_t nb_tx_desc;		/* ring length */
	uint16_t tx_tail;		/* current value of tail */
	volatile uint8_t *qtx_tail;	/* register address of tail */
	/* number of used desc since RS bit set */
	uint16_t nb_used;
	uint16_t nb_free;
	uint16_t last_desc_cleaned;	/* last desc have been cleaned*/
	uint16_t free_thresh;
	uint16_t rs_thresh;

	uint16_t port_id;
	uint16_t queue_id;
	uint64_t offloads;
	uint16_t next_dd;	/* next to set RS, for VPMD */
	uint16_t next_rs;	/* next to check DD,  for VPMD */

	bool q_set;		/* if tx queue has been configured */
	bool q_started;		/* if tx queue has been started */
	bool tx_deferred_start; /* don't start this queue in dev start */
	const struct idpf_txq_ops *ops;

	/* only valid for split queue mode */
	uint16_t sw_nb_desc;
	uint16_t sw_tail;
	void **txqs;
	uint32_t tx_start_qid;
	uint8_t expected_gen_id;
	struct idpf_tx_queue *complq;
};

struct idpf_rxq_ops {
	void (*release_mbufs)(struct idpf_rx_queue *rxq);
};

struct idpf_txq_ops {
	void (*release_mbufs)(struct idpf_tx_queue *txq);
};

__rte_internal
int idpf_check_rx_thresh(uint16_t nb_desc, uint16_t thresh);
__rte_internal
int idpf_check_tx_thresh(uint16_t nb_desc, uint16_t tx_rs_thresh,
			 uint16_t tx_free_thresh);
__rte_internal
void idpf_release_rxq_mbufs(struct idpf_rx_queue *rxq);
__rte_internal
void idpf_release_txq_mbufs(struct idpf_tx_queue *txq);
__rte_internal
void idpf_reset_split_rx_descq(struct idpf_rx_queue *rxq);
__rte_internal
void idpf_reset_split_rx_bufq(struct idpf_rx_queue *rxq);
__rte_internal
void idpf_reset_split_rx_queue(struct idpf_rx_queue *rxq);
__rte_internal
void idpf_reset_single_rx_queue(struct idpf_rx_queue *rxq);
__rte_internal
void idpf_reset_split_tx_descq(struct idpf_tx_queue *txq);
__rte_internal
void idpf_reset_split_tx_complq(struct idpf_tx_queue *cq);
__rte_internal
void idpf_reset_single_tx_queue(struct idpf_tx_queue *txq);
__rte_internal
void idpf_rx_queue_release(void *rxq);
__rte_internal
void idpf_tx_queue_release(void *txq);
__rte_internal
int idpf_alloc_single_rxq_mbufs(struct idpf_rx_queue *rxq);
__rte_internal
int idpf_alloc_split_rxq_mbufs(struct idpf_rx_queue *rxq);

#endif /* _IDPF_COMMON_RXTX_H_ */
