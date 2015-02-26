/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I40E_RXTX_H_
#define _I40E_RXTX_H_

/**
 * 32 bits tx flags, high 16 bits for L2TAG1 (VLAN),
 * low 16 bits for others.
 */
#define I40E_TX_FLAG_L2TAG1_SHIFT 16
#define I40E_TX_FLAG_L2TAG1_MASK  0xffff0000
#define I40E_TX_FLAG_CSUM         ((uint32_t)(1 << 0))
#define I40E_TX_FLAG_INSERT_VLAN  ((uint32_t)(1 << 1))
#define I40E_TX_FLAG_TSYN         ((uint32_t)(1 << 2))

#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
#define RTE_PMD_I40E_RX_MAX_BURST 32
#endif

#define I40E_RXBUF_SZ_1024 1024
#define I40E_RXBUF_SZ_2048 2048

enum i40e_header_split_mode {
	i40e_header_split_none = 0,
	i40e_header_split_enabled = 1,
	i40e_header_split_always = 2,
	i40e_header_split_reserved
};

#define I40E_HEADER_SPLIT_NONE    ((uint8_t)0)
#define I40E_HEADER_SPLIT_L2      ((uint8_t)(1 << 0))
#define I40E_HEADER_SPLIT_IP      ((uint8_t)(1 << 1))
#define I40E_HEADER_SPLIT_UDP_TCP ((uint8_t)(1 << 2))
#define I40E_HEADER_SPLIT_SCTP    ((uint8_t)(1 << 3))
#define I40E_HEADER_SPLIT_ALL (I40E_HEADER_SPLIT_L2 | \
			       I40E_HEADER_SPLIT_IP | \
			       I40E_HEADER_SPLIT_UDP_TCP | \
			       I40E_HEADER_SPLIT_SCTP)

/* HW desc structure, both 16-byte and 32-byte types are supported */
#ifdef RTE_LIBRTE_I40E_16BYTE_RX_DESC
#define i40e_rx_desc i40e_16byte_rx_desc
#else
#define i40e_rx_desc i40e_32byte_rx_desc
#endif

struct i40e_rx_entry {
	struct rte_mbuf *mbuf;
};

/*
 * Structure associated with each RX queue.
 */
struct i40e_rx_queue {
	struct rte_mempool *mp; /**< mbuf pool to populate RX ring */
	volatile union i40e_rx_desc *rx_ring;/**< RX ring virtual address */
	uint64_t rx_ring_phys_addr; /**< RX ring DMA address */
	struct i40e_rx_entry *sw_ring; /**< address of RX soft ring */
	uint16_t nb_rx_desc; /**< number of RX descriptors */
	uint16_t rx_free_thresh; /**< max free RX desc to hold */
	uint16_t rx_tail; /**< current value of tail */
	uint16_t nb_rx_hold; /**< number of held free RX desc */
	struct rte_mbuf *pkt_first_seg; /**< first segment of current packet */
	struct rte_mbuf *pkt_last_seg; /**< last segment of current packet */
#ifdef RTE_LIBRTE_I40E_RX_ALLOW_BULK_ALLOC
	uint16_t rx_nb_avail; /**< number of staged packets ready */
	uint16_t rx_next_avail; /**< index of next staged packets */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */
	struct rte_mbuf fake_mbuf; /**< dummy mbuf */
	struct rte_mbuf *rx_stage[RTE_PMD_I40E_RX_MAX_BURST * 2];
#endif
	uint8_t port_id; /**< device port ID */
	uint8_t crc_len; /**< 0 if CRC stripped, 4 otherwise */
	uint16_t queue_id; /**< RX queue index */
	uint16_t reg_idx; /**< RX queue register index */
	uint8_t drop_en; /**< if not 0, set register bit */
	volatile uint8_t *qrx_tail; /**< register address of tail */
	struct i40e_vsi *vsi; /**< the VSI this queue belongs to */
	uint16_t rx_buf_len; /* The packet buffer size */
	uint16_t rx_hdr_len; /* The header buffer size */
	uint16_t max_pkt_len; /* Maximum packet length */
	uint8_t hs_mode; /* Header Split mode */
	bool q_set; /**< indicate if rx queue has been configured */
	bool rx_deferred_start; /**< don't start this queue in dev start */
};

struct i40e_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

/*
 * Structure associated with each TX queue.
 */
struct i40e_tx_queue {
	uint16_t nb_tx_desc; /**< number of TX descriptors */
	uint64_t tx_ring_phys_addr; /**< TX ring DMA address */
	volatile struct i40e_tx_desc *tx_ring; /**< TX ring virtual address */
	struct i40e_tx_entry *sw_ring; /**< virtual address of SW ring */
	uint16_t tx_tail; /**< current value of tail register */
	volatile uint8_t *qtx_tail; /**< register address of tail */
	uint16_t nb_tx_used; /**< number of TX desc used since RS bit set */
	/**< index to last TX descriptor to have been cleaned */
	uint16_t last_desc_cleaned;
	/**< Total number of TX descriptors ready to be allocated. */
	uint16_t nb_tx_free;
	/**< Number of TX descriptors to use before RS bit is set. */
	uint16_t tx_free_thresh; /**< minimum TX before freeing. */
	/** Number of TX descriptors to use before RS bit is set. */
	uint16_t tx_rs_thresh;
	uint8_t pthresh; /**< Prefetch threshold register. */
	uint8_t hthresh; /**< Host threshold register. */
	uint8_t wthresh; /**< Write-back threshold reg. */
	uint8_t port_id; /**< Device port identifier. */
	uint16_t queue_id; /**< TX queue index. */
	uint16_t reg_idx;
	uint32_t txq_flags;
	struct i40e_vsi *vsi; /**< the VSI this queue belongs to */
	uint16_t tx_next_dd;
	uint16_t tx_next_rs;
	bool q_set; /**< indicate if tx queue has been configured */
	bool tx_deferred_start; /**< don't start this queue in dev start */
};

/** Offload features */
union i40e_tx_offload {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l4_len:8; /**< L4 Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size */
		uint64_t outer_l2_len:8; /**< outer L2 Header Length */
		uint64_t outer_l3_len:16; /**< outer L3 Header Length */
	};
};

int i40e_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int i40e_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int i40e_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int i40e_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int i40e_dev_rx_queue_setup(struct rte_eth_dev *dev,
			    uint16_t queue_idx,
			    uint16_t nb_desc,
			    unsigned int socket_id,
			    const struct rte_eth_rxconf *rx_conf,
			    struct rte_mempool *mp);
int i40e_dev_tx_queue_setup(struct rte_eth_dev *dev,
			    uint16_t queue_idx,
			    uint16_t nb_desc,
			    unsigned int socket_id,
			    const struct rte_eth_txconf *tx_conf);
void i40e_dev_rx_queue_release(void *rxq);
void i40e_dev_tx_queue_release(void *txq);
uint16_t i40e_recv_pkts(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts);
uint16_t i40e_recv_scattered_pkts(void *rx_queue,
				  struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);
uint16_t i40e_xmit_pkts(void *tx_queue,
			struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
int i40e_tx_queue_init(struct i40e_tx_queue *txq);
int i40e_rx_queue_init(struct i40e_rx_queue *rxq);
void i40e_free_tx_resources(struct i40e_tx_queue *txq);
void i40e_free_rx_resources(struct i40e_rx_queue *rxq);
void i40e_dev_clear_queues(struct rte_eth_dev *dev);
void i40e_reset_rx_queue(struct i40e_rx_queue *rxq);
void i40e_reset_tx_queue(struct i40e_tx_queue *txq);
void i40e_tx_queue_release_mbufs(struct i40e_tx_queue *txq);
int i40e_alloc_rx_queue_mbufs(struct i40e_rx_queue *rxq);
void i40e_rx_queue_release_mbufs(struct i40e_rx_queue *rxq);

uint32_t i40e_dev_rx_queue_count(struct rte_eth_dev *dev,
				 uint16_t rx_queue_id);
int i40e_dev_rx_descriptor_done(void *rx_queue, uint16_t offset);

#endif /* _I40E_RXTX_H_ */
