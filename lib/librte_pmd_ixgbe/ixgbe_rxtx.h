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

#ifndef _IXGBE_RXTX_H_
#define _IXGBE_RXTX_H_


#define RTE_PMD_IXGBE_TX_MAX_BURST 32

#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
#define RTE_PMD_IXGBE_RX_MAX_BURST 32
#define RTE_IXGBE_DESCS_PER_LOOP           4
#elif defined(RTE_IXGBE_INC_VECTOR)
#define RTE_IXGBE_DESCS_PER_LOOP           4
#else
#define RTE_IXGBE_DESCS_PER_LOOP           1
#endif

#define RTE_MBUF_DATA_DMA_ADDR(mb) \
	(uint64_t) ((mb)->buf_physaddr + (uint64_t)((char *)((mb)->pkt.data) - \
	(char *)(mb)->buf_addr))

#define RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mb) \
	(uint64_t) ((mb)->buf_physaddr + RTE_PKTMBUF_HEADROOM)

#ifdef RTE_IXGBE_INC_VECTOR
#define RTE_IXGBE_VPMD_RX_BURST         32
#define RTE_IXGBE_VPMD_TX_BURST         32
#define RTE_IXGBE_RXQ_REARM_THRESH      RTE_IXGBE_VPMD_RX_BURST
#define RTE_IXGBE_TX_MAX_FREE_BUF_SZ    64
#endif

#define RX_RING_SZ ((IXGBE_MAX_RING_DESC + RTE_IXGBE_DESCS_PER_LOOP - 1) * \
		    sizeof(union ixgbe_adv_rx_desc))

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)  rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)  do {} while(0)
#endif

#define RTE_IXGBE_REGISTER_POLL_WAIT_10_MS  10
#define RTE_IXGBE_WAIT_100_US               100
#define RTE_IXGBE_VMTXSW_REGISTER_COUNT     2

/**
 * Structure associated with each descriptor of the RX ring of a RX queue.
 */
struct igb_rx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with RX descriptor. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct igb_tx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with TX desc, if any. */
	uint16_t next_id; /**< Index of next descriptor in ring. */
	uint16_t last_id; /**< Index of last scattered descriptor. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct igb_tx_entry_v {
	struct rte_mbuf *mbuf; /**< mbuf associated with TX desc, if any. */
};

/**
 * continuous entry sequence, gather by the same mempool
 */
struct igb_tx_entry_seq {
	const struct rte_mempool* pool;
	uint32_t same_pool;
};

/**
 * Structure associated with each RX queue.
 */
struct igb_rx_queue {
	struct rte_mempool  *mb_pool; /**< mbuf pool to populate RX ring. */
	volatile union ixgbe_adv_rx_desc *rx_ring; /**< RX ring virtual address. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	volatile uint32_t   *rdt_reg_addr; /**< RDT register address. */
	volatile uint32_t   *rdh_reg_addr; /**< RDH register address. */
	struct igb_rx_entry *sw_ring; /**< address of RX software ring. */
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg; /**< Last segment of current packet. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            rx_tail;  /**< current value of RDT register. */
	uint16_t            nb_rx_hold; /**< number of held free RX desc. */
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
	uint16_t rx_nb_avail; /**< nr of staged pkts ready to ret to app */
	uint16_t rx_next_avail; /**< idx of next staged pkt to ret to app */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */
#endif
#ifdef RTE_IXGBE_INC_VECTOR
	uint16_t            rxrearm_nb; /**< the idx we start the re-arming from */
	uint16_t            rxrearm_start;  /**< number of remaining to be re-armed */
	__m128i             misc_info;  /**< cache XMM combine port_id/crc/nb_segs */
#endif
	uint16_t            rx_free_thresh; /**< max free RX desc to hold. */
	uint16_t            queue_id; /**< RX queue index. */
	uint16_t            reg_idx;  /**< RX queue register index. */
	uint8_t             port_id;  /**< Device port identifier. */
	uint8_t             crc_len;  /**< 0 if CRC stripped, 4 otherwise. */
	uint8_t             drop_en;  /**< If not 0, set SRRCTL.Drop_En. */
	uint8_t             start_rx_per_q;
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
	/** need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf fake_mbuf;
	/** hold packets to return to application */
	struct rte_mbuf *rx_stage[RTE_PMD_IXGBE_RX_MAX_BURST*2];
#endif
};

/**
 * IXGBE CTX Constants
 */
enum ixgbe_advctx_num {
	IXGBE_CTX_0    = 0, /**< CTX0 */
	IXGBE_CTX_1    = 1, /**< CTX1  */
	IXGBE_CTX_NUM  = 2, /**< CTX NUMBER  */
};

/**
 * Structure to check if new context need be built
 */

struct ixgbe_advctx_info {
	uint16_t flags;           /**< ol_flags for context build. */
	uint32_t cmp_mask;        /**< compare mask for vlan_macip_lens */
	union rte_vlan_macip vlan_macip_lens; /**< vlan, mac ip length. */
};

/**
 * Structure associated with each TX queue.
 */
struct igb_tx_queue {
	/** TX ring virtual address. */
	volatile union ixgbe_adv_tx_desc *tx_ring;
	uint64_t            tx_ring_phys_addr; /**< TX ring DMA address. */
	struct igb_tx_entry *sw_ring;      /**< virtual address of SW ring. */
#ifdef RTE_IXGBE_INC_VECTOR
	/** continuous tx entry sequence within the same mempool */
	struct igb_tx_entry_seq *sw_ring_seq;
#endif
	volatile uint32_t   *tdt_reg_addr; /**< Address of TDT register. */
	uint16_t            nb_tx_desc;    /**< number of TX descriptors. */
	uint16_t            tx_tail;       /**< current value of TDT reg. */
	uint16_t            tx_free_thresh;/**< minimum TX before freeing. */
	/** Number of TX descriptors to use before RS bit is set. */
	uint16_t            tx_rs_thresh;
	/** Number of TX descriptors used since RS bit was set. */
	uint16_t            nb_tx_used;
	/** Index to last TX descriptor to have been cleaned. */
	uint16_t            last_desc_cleaned;
	/** Total number of TX descriptors ready to be allocated. */
	uint16_t            nb_tx_free;
	uint16_t tx_next_dd; /**< next desc to scan for DD bit */
	uint16_t tx_next_rs; /**< next desc to set RS bit */
	uint16_t            queue_id;      /**< TX queue index. */
	uint16_t            reg_idx;       /**< TX queue register index. */
	uint8_t             port_id;       /**< Device port identifier. */
	uint8_t             pthresh;       /**< Prefetch threshold register. */
	uint8_t             hthresh;       /**< Host threshold register. */
	uint8_t             wthresh;       /**< Write-back threshold reg. */
	uint32_t txq_flags; /**< Holds flags for this TXq */
	uint32_t            ctx_curr;      /**< Hardware context states. */
	/** Hardware context0 history. */
	struct ixgbe_advctx_info ctx_cache[IXGBE_CTX_NUM];
	struct ixgbe_txq_ops *ops;          /**< txq ops */
	uint8_t             start_tx_per_q;
};

struct ixgbe_txq_ops {
	void (*release_mbufs)(struct igb_tx_queue *txq);
	void (*free_swring)(struct igb_tx_queue *txq);
	void (*reset)(struct igb_tx_queue *txq);
};

/*
 * The "simple" TX queue functions require that the following
 * flags are set when the TX queue is configured:
 *  - ETH_TXQ_FLAGS_NOMULTSEGS
 *  - ETH_TXQ_FLAGS_NOVLANOFFL
 *  - ETH_TXQ_FLAGS_NOXSUMSCTP
 *  - ETH_TXQ_FLAGS_NOXSUMUDP
 *  - ETH_TXQ_FLAGS_NOXSUMTCP
 * and that the RS bit threshold (tx_rs_thresh) is at least equal to
 * RTE_PMD_IXGBE_TX_MAX_BURST.
 */
#define IXGBE_SIMPLE_FLAGS ((uint32_t)ETH_TXQ_FLAGS_NOMULTSEGS | \
			    ETH_TXQ_FLAGS_NOOFFLOADS)

/*
 * Populate descriptors with the following info:
 * 1.) buffer_addr = phys_addr + headroom
 * 2.) cmd_type_len = DCMD_DTYP_FLAGS | pkt_len
 * 3.) olinfo_status = pkt_len << PAYLEN_SHIFT
 */

/* Defines for Tx descriptor */
#define DCMD_DTYP_FLAGS (IXGBE_ADVTXD_DTYP_DATA |\
			 IXGBE_ADVTXD_DCMD_IFCS |\
			 IXGBE_ADVTXD_DCMD_DEXT |\
			 IXGBE_ADVTXD_DCMD_EOP)

#ifdef RTE_IXGBE_INC_VECTOR
uint16_t ixgbe_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t ixgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
int ixgbe_txq_vec_setup(struct igb_tx_queue *txq, unsigned int socket_id);
int ixgbe_rxq_vec_setup(struct igb_rx_queue *rxq, unsigned int socket_id);
int ixgbe_rx_vec_condition_check(struct rte_eth_dev *dev);
#endif

#endif
