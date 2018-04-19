/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VMXNET3_RING_H_
#define _VMXNET3_RING_H_

#define VMXNET3_RX_CMDRING_SIZE 2

#define VMXNET3_DRIVER_VERSION_NUM 0x01012000

/* Default ring size */
#define VMXNET3_DEF_TX_RING_SIZE 512
#define VMXNET3_DEF_RX_RING_SIZE 128

/* Default rx data ring desc size */
#define VMXNET3_DEF_RXDATA_DESC_SIZE 256

#define VMXNET3_SUCCESS 0
#define VMXNET3_FAIL   -1

#define TRUE  1
#define FALSE 0


typedef struct vmxnet3_buf_info {
	uint16_t               len;
	struct rte_mbuf        *m;
	uint64_t               bufPA;
} vmxnet3_buf_info_t;

typedef struct vmxnet3_cmd_ring {
	vmxnet3_buf_info_t     *buf_info;
	uint32_t               size;
	uint32_t               next2fill;
	uint32_t               next2comp;
	uint8_t                gen;
	uint8_t                rid;
	Vmxnet3_GenericDesc    *base;
	uint64_t               basePA;
} vmxnet3_cmd_ring_t;

static inline void
vmxnet3_cmd_ring_adv_next2fill(struct vmxnet3_cmd_ring *ring)
{
	ring->next2fill++;
	if (unlikely(ring->next2fill == ring->size)) {
		ring->next2fill = 0;
		ring->gen = (uint8_t)(ring->gen ^ 1);
	}
}

static inline void
vmxnet3_cmd_ring_adv_next2comp(struct vmxnet3_cmd_ring *ring)
{
	VMXNET3_INC_RING_IDX_ONLY(ring->next2comp, ring->size);
}

static inline uint32_t
vmxnet3_cmd_ring_desc_avail(struct vmxnet3_cmd_ring *ring)
{
	return (ring->next2comp > ring->next2fill ? 0 : ring->size) +
		   ring->next2comp - ring->next2fill - 1;
}

static inline bool
vmxnet3_cmd_ring_desc_empty(struct vmxnet3_cmd_ring *ring)
{
	return ring->next2comp == ring->next2fill;
}

typedef struct vmxnet3_comp_ring {
	uint32_t               size;
	uint32_t               next2proc;
	uint8_t                gen;
	uint8_t                intr_idx;
	Vmxnet3_GenericDesc    *base;
	uint64_t               basePA;
} vmxnet3_comp_ring_t;

struct vmxnet3_data_ring {
	struct Vmxnet3_TxDataDesc *base;
	uint32_t                  size;
	uint64_t                  basePA;
};

static inline void
vmxnet3_comp_ring_adv_next2proc(struct vmxnet3_comp_ring *ring)
{
	ring->next2proc++;
	if (unlikely(ring->next2proc == ring->size)) {
		ring->next2proc = 0;
		ring->gen = (uint8_t)(ring->gen ^ 1);
	}
}

struct vmxnet3_txq_stats {
	uint64_t        drop_total; /* # of pkts dropped by the driver,
				     * the counters below track droppings due to
				     * different reasons
				     */
	uint64_t        drop_too_many_segs;
	uint64_t        drop_tso;
	uint64_t        tx_ring_full;
};

typedef struct vmxnet3_tx_queue {
	struct vmxnet3_hw            *hw;
	struct vmxnet3_cmd_ring      cmd_ring;
	struct vmxnet3_comp_ring     comp_ring;
	struct vmxnet3_data_ring     data_ring;
	uint32_t                     qid;
	struct Vmxnet3_TxQueueDesc   *shared;
	struct vmxnet3_txq_stats     stats;
	const struct rte_memzone     *mz;
	bool                         stopped;
	uint16_t                     queue_id;      /**< Device TX queue index. */
	uint16_t                     port_id;       /**< Device port identifier. */
	uint16_t		     txdata_desc_size;
} vmxnet3_tx_queue_t;

struct vmxnet3_rxq_stats {
	uint64_t                     drop_total;
	uint64_t                     drop_err;
	uint64_t                     drop_fcs;
	uint64_t                     rx_buf_alloc_failure;
};

struct vmxnet3_rx_data_ring {
	uint8_t  *base;
	uint64_t basePA;
	uint32_t size;
};

typedef struct vmxnet3_rx_queue {
	struct rte_mempool          *mp;
	struct vmxnet3_hw           *hw;
	struct vmxnet3_cmd_ring     cmd_ring[VMXNET3_RX_CMDRING_SIZE];
	struct vmxnet3_comp_ring    comp_ring;
	struct vmxnet3_rx_data_ring data_ring;
	uint16_t                    data_desc_size;
	uint32_t                    qid1;
	uint32_t                    qid2;
	/* rqID in RCD for buffer from data ring */
	uint32_t                    data_ring_qid;
	Vmxnet3_RxQueueDesc         *shared;
	struct rte_mbuf             *start_seg;
	struct rte_mbuf             *last_seg;
	struct vmxnet3_rxq_stats    stats;
	const struct rte_memzone    *mz;
	bool                        stopped;
	uint16_t                    queue_id;      /**< Device RX queue index. */
	uint16_t                    port_id;       /**< Device port identifier. */
} vmxnet3_rx_queue_t;

#endif /* _VMXNET3_RING_H_ */
