/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>

#include <rte_interrupts.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_prefetch.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_string_fns.h>

#include "e1000_logs.h"
#include "base/e1000_api.h"
#include "e1000_ethdev.h"
#include "base/e1000_osdep.h"

#define	E1000_TXD_VLAN_SHIFT	16

#define E1000_RXDCTL_GRAN	0x01000000 /* RXDCTL Granularity */

/**
 * Structure associated with each descriptor of the RX ring of a RX queue.
 */
struct em_rx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with RX descriptor. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct em_tx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with TX desc, if any. */
	uint16_t next_id; /**< Index of next descriptor in ring. */
	uint16_t last_id; /**< Index of last scattered descriptor. */
};

/**
 * Structure associated with each RX queue.
 */
struct em_rx_queue {
	struct rte_mempool  *mb_pool;   /**< mbuf pool to populate RX ring. */
	volatile struct e1000_rx_desc *rx_ring; /**< RX ring virtual address. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	volatile uint32_t   *rdt_reg_addr; /**< RDT register address. */
	volatile uint32_t   *rdh_reg_addr; /**< RDH register address. */
	struct em_rx_entry *sw_ring;   /**< address of RX software ring. */
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg;  /**< Last segment of current packet. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            rx_tail;    /**< current value of RDT register. */
	uint16_t            nb_rx_hold; /**< number of held free RX desc. */
	uint16_t            rx_free_thresh; /**< max free RX desc to hold. */
	uint16_t            queue_id;   /**< RX queue index. */
	uint8_t             port_id;    /**< Device port identifier. */
	uint8_t             pthresh;    /**< Prefetch threshold register. */
	uint8_t             hthresh;    /**< Host threshold register. */
	uint8_t             wthresh;    /**< Write-back threshold register. */
	uint8_t             crc_len;    /**< 0 if CRC stripped, 4 otherwise. */
};

/**
 * Hardware context number
 */
enum {
	EM_CTX_0    = 0, /**< CTX0 */
	EM_CTX_NUM  = 1, /**< CTX NUM */
};

/** Offload features */
union em_vlan_macip {
	uint32_t data;
	struct {
		uint16_t l3_len:9; /**< L3 (IP) Header Length. */
		uint16_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint16_t vlan_tci;
		/**< VLAN Tag Control Identifier (CPU order). */
	} f;
};

/*
 * Compare mask for vlan_macip_len.data,
 * should be in sync with em_vlan_macip.f layout.
 * */
#define TX_VLAN_CMP_MASK        0xFFFF0000  /**< VLAN length - 16-bits. */
#define TX_MAC_LEN_CMP_MASK     0x0000FE00  /**< MAC length - 7-bits. */
#define TX_IP_LEN_CMP_MASK      0x000001FF  /**< IP  length - 9-bits. */
/** MAC+IP  length. */
#define TX_MACIP_LEN_CMP_MASK   (TX_MAC_LEN_CMP_MASK | TX_IP_LEN_CMP_MASK)

/**
 * Structure to check if new context need be built
 */
struct em_ctx_info {
	uint64_t flags;              /**< ol_flags related to context build. */
	uint32_t cmp_mask;           /**< compare mask */
	union em_vlan_macip hdrlen;  /**< L2 and L3 header lenghts */
};

/**
 * Structure associated with each TX queue.
 */
struct em_tx_queue {
	volatile struct e1000_data_desc *tx_ring; /**< TX ring address */
	uint64_t               tx_ring_phys_addr; /**< TX ring DMA address. */
	struct em_tx_entry    *sw_ring; /**< virtual address of SW ring. */
	volatile uint32_t      *tdt_reg_addr; /**< Address of TDT register. */
	uint16_t               nb_tx_desc;    /**< number of TX descriptors. */
	uint16_t               tx_tail;  /**< Current value of TDT register. */
	/**< Start freeing TX buffers if there are less free descriptors than
	     this value. */
	uint16_t               tx_free_thresh;
	/**< Number of TX descriptors to use before RS bit is set. */
	uint16_t               tx_rs_thresh;
	/** Number of TX descriptors used since RS bit was set. */
	uint16_t               nb_tx_used;
	/** Index to last TX descriptor to have been cleaned. */
	uint16_t	       last_desc_cleaned;
	/** Total number of TX descriptors ready to be allocated. */
	uint16_t               nb_tx_free;
	uint16_t               queue_id; /**< TX queue index. */
	uint8_t                port_id;  /**< Device port identifier. */
	uint8_t                pthresh;  /**< Prefetch threshold register. */
	uint8_t                hthresh;  /**< Host threshold register. */
	uint8_t                wthresh;  /**< Write-back threshold register. */
	struct em_ctx_info ctx_cache;
	/**< Hardware context history.*/
};

#if 1
#define RTE_PMD_USE_PREFETCH
#endif

#ifdef RTE_PMD_USE_PREFETCH
#define rte_em_prefetch(p)	rte_prefetch0(p)
#else
#define rte_em_prefetch(p)	do {} while(0)
#endif

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p) rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)	do {} while(0)
#endif

#ifndef DEFAULT_TX_FREE_THRESH
#define DEFAULT_TX_FREE_THRESH  32
#endif /* DEFAULT_TX_FREE_THRESH */

#ifndef DEFAULT_TX_RS_THRESH
#define DEFAULT_TX_RS_THRESH  32
#endif /* DEFAULT_TX_RS_THRESH */


/*********************************************************************
 *
 *  TX function
 *
 **********************************************************************/

/*
 * Populates TX context descriptor.
 */
static inline void
em_set_xmit_ctx(struct em_tx_queue* txq,
		volatile struct e1000_context_desc *ctx_txd,
		uint64_t flags,
		union em_vlan_macip hdrlen)
{
	uint32_t cmp_mask, cmd_len;
	uint16_t ipcse, l2len;
	struct e1000_context_desc ctx;

	cmp_mask = 0;
	cmd_len = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_C;

	l2len = hdrlen.f.l2_len;
	ipcse = (uint16_t)(l2len + hdrlen.f.l3_len);

	/* setup IPCS* fields */
	ctx.lower_setup.ip_fields.ipcss = (uint8_t)l2len;
	ctx.lower_setup.ip_fields.ipcso = (uint8_t)(l2len +
			offsetof(struct ipv4_hdr, hdr_checksum));

	/*
	 * When doing checksum or TCP segmentation with IPv6 headers,
	 * IPCSE field should be set t0 0.
	 */
	if (flags & PKT_TX_IP_CKSUM) {
		ctx.lower_setup.ip_fields.ipcse =
			(uint16_t)rte_cpu_to_le_16(ipcse - 1);
		cmd_len |= E1000_TXD_CMD_IP;
		cmp_mask |= TX_MACIP_LEN_CMP_MASK;
	} else {
		ctx.lower_setup.ip_fields.ipcse = 0;
	}

	/* setup TUCS* fields */
	ctx.upper_setup.tcp_fields.tucss = (uint8_t)ipcse;
	ctx.upper_setup.tcp_fields.tucse = 0;

	switch (flags & PKT_TX_L4_MASK) {
	case PKT_TX_UDP_CKSUM:
		ctx.upper_setup.tcp_fields.tucso = (uint8_t)(ipcse +
				offsetof(struct udp_hdr, dgram_cksum));
		cmp_mask |= TX_MACIP_LEN_CMP_MASK;
		break;
	case PKT_TX_TCP_CKSUM:
		ctx.upper_setup.tcp_fields.tucso = (uint8_t)(ipcse +
				offsetof(struct tcp_hdr, cksum));
		cmd_len |= E1000_TXD_CMD_TCP;
		cmp_mask |= TX_MACIP_LEN_CMP_MASK;
		break;
	default:
		ctx.upper_setup.tcp_fields.tucso = 0;
	}

	ctx.cmd_and_length = rte_cpu_to_le_32(cmd_len);
	ctx.tcp_seg_setup.data = 0;

	*ctx_txd = ctx;

	txq->ctx_cache.flags = flags;
	txq->ctx_cache.cmp_mask = cmp_mask;
	txq->ctx_cache.hdrlen = hdrlen;
}

/*
 * Check which hardware context can be used. Use the existing match
 * or create a new context descriptor.
 */
static inline uint32_t
what_ctx_update(struct em_tx_queue *txq, uint64_t flags,
		union em_vlan_macip hdrlen)
{
	/* If match with the current context */
	if (likely (txq->ctx_cache.flags == flags &&
			((txq->ctx_cache.hdrlen.data ^ hdrlen.data) &
			txq->ctx_cache.cmp_mask) == 0))
		return EM_CTX_0;

	/* Mismatch */
	return EM_CTX_NUM;
}

/* Reset transmit descriptors after they have been used */
static inline int
em_xmit_cleanup(struct em_tx_queue *txq)
{
	struct em_tx_entry *sw_ring = txq->sw_ring;
	volatile struct e1000_data_desc *txr = txq->tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	/* Determine the last descriptor needing to be cleaned */
	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	/* Check to make sure the last descriptor to clean is done */
	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if (! (txr[desc_to_clean_to].upper.fields.status & E1000_TXD_STAT_DD))
	{
		PMD_TX_FREE_LOG(DEBUG,
				"TX descriptor %4u is not done"
				"(port=%d queue=%d)", desc_to_clean_to,
				txq->port_id, txq->queue_id);
		/* Failed to clean any descriptors, better luck next time */
		return -(1);
	}

	/* Figure out how many descriptors will be cleaned */
	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
							desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
						last_desc_cleaned);

	PMD_TX_FREE_LOG(DEBUG,
			"Cleaning %4u TX descriptors: %4u to %4u "
			"(port=%d queue=%d)", nb_tx_to_clean,
			last_desc_cleaned, desc_to_clean_to, txq->port_id,
			txq->queue_id);

	/*
	 * The last descriptor to clean is done, so that means all the
	 * descriptors from the last descriptor that was cleaned
	 * up to the last descriptor with the RS bit set
	 * are done. Only reset the threshold descriptor.
	 */
	txr[desc_to_clean_to].upper.fields.status = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	/* No Error */
	return 0;
}

static inline uint32_t
tx_desc_cksum_flags_to_upper(uint64_t ol_flags)
{
	static const uint32_t l4_olinfo[2] = {0, E1000_TXD_POPTS_TXSM << 8};
	static const uint32_t l3_olinfo[2] = {0, E1000_TXD_POPTS_IXSM << 8};
	uint32_t tmp;

	tmp = l4_olinfo[(ol_flags & PKT_TX_L4_MASK) != PKT_TX_L4_NO_CKSUM];
	tmp |= l3_olinfo[(ol_flags & PKT_TX_IP_CKSUM) != 0];
	return tmp;
}

uint16_t
eth_em_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct em_tx_queue *txq;
	struct em_tx_entry *sw_ring;
	struct em_tx_entry *txe, *txn;
	volatile struct e1000_data_desc *txr;
	volatile struct e1000_data_desc *txd;
	struct rte_mbuf     *tx_pkt;
	struct rte_mbuf     *m_seg;
	uint64_t buf_dma_addr;
	uint32_t popts_spec;
	uint32_t cmd_type_len;
	uint16_t slen;
	uint64_t ol_flags;
	uint16_t tx_id;
	uint16_t tx_last;
	uint16_t nb_tx;
	uint16_t nb_used;
	uint64_t tx_ol_req;
	uint32_t ctx;
	uint32_t new_ctx;
	union em_vlan_macip hdrlen;

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr     = txq->tx_ring;
	tx_id   = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Determine if the descriptor ring needs to be cleaned. */
	 if (txq->nb_tx_free < txq->tx_free_thresh)
		em_xmit_cleanup(txq);

	/* TX loop */
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		new_ctx = 0;
		tx_pkt = *tx_pkts++;

		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		/*
		 * Determine how many (if any) context descriptors
		 * are needed for offload functionality.
		 */
		ol_flags = tx_pkt->ol_flags;

		/* If hardware offload required */
		tx_ol_req = (ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_L4_MASK));
		if (tx_ol_req) {
			hdrlen.f.vlan_tci = tx_pkt->vlan_tci;
			hdrlen.f.l2_len = tx_pkt->l2_len;
			hdrlen.f.l3_len = tx_pkt->l3_len;
			/* If new context to be built or reuse the exist ctx. */
			ctx = what_ctx_update(txq, tx_ol_req, hdrlen);

			/* Only allocate context descriptor if required*/
			new_ctx = (ctx == EM_CTX_NUM);
		}

		/*
		 * Keep track of how many descriptors are used this loop
		 * This will always be the number of segments + the number of
		 * Context descriptors required to transmit the packet
		 */
		nb_used = (uint16_t)(tx_pkt->nb_segs + new_ctx);

		/*
		 * The number of descriptors that must be allocated for a
		 * packet is the number of segments of that packet, plus 1
		 * Context Descriptor for the hardware offload, if any.
		 * Determine the last TX descriptor to allocate in the TX ring
		 * for the packet, starting from the current position (tx_id)
		 * in the ring.
		 */
		tx_last = (uint16_t) (tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t) (tx_last - txq->nb_tx_desc);

		PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u pktlen=%u"
			   " tx_first=%u tx_last=%u",
			   (unsigned) txq->port_id,
			   (unsigned) txq->queue_id,
			   (unsigned) tx_pkt->pkt_len,
			   (unsigned) tx_id,
			   (unsigned) tx_last);

		/*
		 * Make sure there are enough TX descriptors available to
		 * transmit the entire packet.
		 * nb_used better be less than or equal to txq->tx_rs_thresh
		 */
		while (unlikely (nb_used > txq->nb_tx_free)) {
			PMD_TX_FREE_LOG(DEBUG, "Not enough free TX descriptors "
					"nb_used=%4u nb_free=%4u "
					"(port=%d queue=%d)",
					nb_used, txq->nb_tx_free,
					txq->port_id, txq->queue_id);

			if (em_xmit_cleanup(txq) != 0) {
				/* Could not clean any descriptors */
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
		}

		/*
		 * By now there are enough free TX descriptors to transmit
		 * the packet.
		 */

		/*
		 * Set common flags of all TX Data Descriptors.
		 *
		 * The following bits must be set in all Data Descriptors:
		 *    - E1000_TXD_DTYP_DATA
		 *    - E1000_TXD_DTYP_DEXT
		 *
		 * The following bits must be set in the first Data Descriptor
		 * and are ignored in the other ones:
		 *    - E1000_TXD_POPTS_IXSM
		 *    - E1000_TXD_POPTS_TXSM
		 *
		 * The following bits must be set in the last Data Descriptor
		 * and are ignored in the other ones:
		 *    - E1000_TXD_CMD_VLE
		 *    - E1000_TXD_CMD_IFCS
		 *
		 * The following bits must only be set in the last Data
		 * Descriptor:
		 *   - E1000_TXD_CMD_EOP
		 *
		 * The following bits can be set in any Data Descriptor, but
		 * are only set in the last Data Descriptor:
		 *   - E1000_TXD_CMD_RS
		 */
		cmd_type_len = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D |
			E1000_TXD_CMD_IFCS;
		popts_spec = 0;

		/* Set VLAN Tag offload fields. */
		if (ol_flags & PKT_TX_VLAN_PKT) {
			cmd_type_len |= E1000_TXD_CMD_VLE;
			popts_spec = tx_pkt->vlan_tci << E1000_TXD_VLAN_SHIFT;
		}

		if (tx_ol_req) {
			/*
			 * Setup the TX Context Descriptor if required
			 */
			if (new_ctx) {
				volatile struct e1000_context_desc *ctx_txd;

				ctx_txd = (volatile struct e1000_context_desc *)
					&txr[tx_id];

				txn = &sw_ring[txe->next_id];
				RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);

				if (txe->mbuf != NULL) {
					rte_pktmbuf_free_seg(txe->mbuf);
					txe->mbuf = NULL;
				}

				em_set_xmit_ctx(txq, ctx_txd, tx_ol_req,
					hdrlen);

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
			}

			/*
			 * Setup the TX Data Descriptor,
			 * This path will go through
			 * whatever new/reuse the context descriptor
			 */
			popts_spec |= tx_desc_cksum_flags_to_upper(ol_flags);
		}

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/*
			 * Set up Transmit Data Descriptor.
			 */
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_dma_addr(m_seg);

			txd->buffer_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->lower.data = rte_cpu_to_le_32(cmd_type_len | slen);
			txd->upper.data = rte_cpu_to_le_32(popts_spec);

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg != NULL);

		/*
		 * The last packet data descriptor needs End Of Packet (EOP)
		 */
		cmd_type_len |= E1000_TXD_CMD_EOP;
		txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		/* Set RS bit only on threshold packets' last descriptor */
		if (txq->nb_tx_used >= txq->tx_rs_thresh) {
			PMD_TX_FREE_LOG(DEBUG,
					"Setting RS bit on TXD id=%4u "
					"(port=%d queue=%d)",
					tx_last, txq->port_id, txq->queue_id);

			cmd_type_len |= E1000_TXD_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_tx_used = 0;
		}
		txd->lower.data |= rte_cpu_to_le_32(cmd_type_len);
	}
end_of_tx:
	rte_wmb();

	/*
	 * Set the Transmit Descriptor Tail (TDT)
	 */
	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		(unsigned) txq->port_id, (unsigned) txq->queue_id,
		(unsigned) tx_id, (unsigned) nb_tx);
	E1000_PCI_REG_WRITE(txq->tdt_reg_addr, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

/*********************************************************************
 *
 *  RX functions
 *
 **********************************************************************/

static inline uint64_t
rx_desc_status_to_pkt_flags(uint32_t rx_status)
{
	uint64_t pkt_flags;

	/* Check if VLAN present */
	pkt_flags = ((rx_status & E1000_RXD_STAT_VP) ?
		PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED : 0);

	return pkt_flags;
}

static inline uint64_t
rx_desc_error_to_pkt_flags(uint32_t rx_error)
{
	uint64_t pkt_flags = 0;

	if (rx_error & E1000_RXD_ERR_IPE)
		pkt_flags |= PKT_RX_IP_CKSUM_BAD;
	if (rx_error & E1000_RXD_ERR_TCPE)
		pkt_flags |= PKT_RX_L4_CKSUM_BAD;
	return pkt_flags;
}

uint16_t
eth_em_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	volatile struct e1000_rx_desc *rx_ring;
	volatile struct e1000_rx_desc *rxdp;
	struct em_rx_queue *rxq;
	struct em_rx_entry *sw_ring;
	struct em_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct e1000_rx_desc rxd;
	uint64_t dma_addr;
	uint16_t pkt_len;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint8_t status;

	rxq = rx_queue;

	nb_rx = 0;
	nb_hold = 0;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
	while (nb_rx < nb_pkts) {
		/*
		 * The order of operations here is important as the DD status
		 * bit must not be read after any other descriptor fields.
		 * rx_ring and rxdp are pointing to volatile data so the order
		 * of accesses cannot be reordered by the compiler. If they were
		 * not volatile, they could be reordered which could lead to
		 * using invalid descriptor fields when read from rxd.
		 */
		rxdp = &rx_ring[rx_id];
		status = rxdp->status;
		if (! (status & E1000_RXD_STAT_DD))
			break;
		rxd = *rxdp;

		/*
		 * End of packet.
		 *
		 * If the E1000_RXD_STAT_EOP flag is not set, the RX packet is
		 * likely to be invalid and to be dropped by the various
		 * validation checks performed by the network stack.
		 *
		 * Allocate a new mbuf to replenish the RX ring descriptor.
		 * If the allocation fails:
		 *    - arrange for that RX descriptor to be the first one
		 *      being parsed the next time the receive function is
		 *      invoked [on the same queue].
		 *
		 *    - Stop parsing the RX ring and return immediately.
		 *
		 * This policy do not drop the packet received in the RX
		 * descriptor for which the allocation of a new mbuf failed.
		 * Thus, it allows that packet to be later retrieved if
		 * mbuf have been freed in the mean time.
		 * As a side effect, holding RX descriptors instead of
		 * systematically giving them back to the NIC may lead to
		 * RX ring exhaustion situations.
		 * However, the NIC can gracefully prevent such situations
		 * to happen by sending specific "back-pressure" flow control
		 * frames to its peer(s).
		 */
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_id=%u "
			   "status=0x%x pkt_len=%u",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) status,
			   (unsigned) rte_le_to_cpu_16(rxd.length));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u",
				   (unsigned) rxq->port_id,
				   (unsigned) rxq->queue_id);
			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_em_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_em_prefetch(&rx_ring[rx_id]);
			rte_em_prefetch(&sw_ring[rx_id]);
		}

		/* Rearm RXD: attach new mbuf and reset status to zero. */

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_dma_addr_default(nmb));
		rxdp->buffer_addr = dma_addr;
		rxdp->status = 0;

		/*
		 * Initialize the returned mbuf.
		 * 1) setup generic mbuf fields:
		 *    - number of segments,
		 *    - next segment,
		 *    - packet length,
		 *    - RX port identifier.
		 * 2) integrate hardware offload data, if any:
		 *    - RSS flag & hash,
		 *    - IP checksum flag,
		 *    - VLAN TCI, if any,
		 *    - error flags.
		 */
		pkt_len = (uint16_t) (rte_le_to_cpu_16(rxd.length) -
				rxq->crc_len);
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_packet_prefetch((char *)rxm->buf_addr + rxm->data_off);
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = pkt_len;
		rxm->data_len = pkt_len;
		rxm->port = rxq->port_id;

		rxm->ol_flags = rx_desc_status_to_pkt_flags(status);
		rxm->ol_flags = rxm->ol_flags |
				rx_desc_error_to_pkt_flags(rxd.errors);

		/* Only valid if PKT_RX_VLAN_PKT set in pkt_flags */
		rxm->vlan_tci = rte_le_to_cpu_16(rxd.special);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	/*
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed RX descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situtation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t) (nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) nb_hold,
			   (unsigned) nb_rx);
		rx_id = (uint16_t) ((rx_id == 0) ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		E1000_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

uint16_t
eth_em_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct em_rx_queue *rxq;
	volatile struct e1000_rx_desc *rx_ring;
	volatile struct e1000_rx_desc *rxdp;
	struct em_rx_entry *sw_ring;
	struct em_rx_entry *rxe;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct e1000_rx_desc rxd;
	uint64_t dma; /* Physical address of mbuf data buffer */
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint16_t data_len;
	uint8_t status;

	rxq = rx_queue;

	nb_rx = 0;
	nb_hold = 0;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;

	/*
	 * Retrieve RX context of current packet, if any.
	 */
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;

	while (nb_rx < nb_pkts) {
	next_desc:
		/*
		 * The order of operations here is important as the DD status
		 * bit must not be read after any other descriptor fields.
		 * rx_ring and rxdp are pointing to volatile data so the order
		 * of accesses cannot be reordered by the compiler. If they were
		 * not volatile, they could be reordered which could lead to
		 * using invalid descriptor fields when read from rxd.
		 */
		rxdp = &rx_ring[rx_id];
		status = rxdp->status;
		if (! (status & E1000_RXD_STAT_DD))
			break;
		rxd = *rxdp;

		/*
		 * Descriptor done.
		 *
		 * Allocate a new mbuf to replenish the RX ring descriptor.
		 * If the allocation fails:
		 *    - arrange for that RX descriptor to be the first one
		 *      being parsed the next time the receive function is
		 *      invoked [on the same queue].
		 *
		 *    - Stop parsing the RX ring and return immediately.
		 *
		 * This policy does not drop the packet received in the RX
		 * descriptor for which the allocation of a new mbuf failed.
		 * Thus, it allows that packet to be later retrieved if
		 * mbuf have been freed in the mean time.
		 * As a side effect, holding RX descriptors instead of
		 * systematically giving them back to the NIC may lead to
		 * RX ring exhaustion situations.
		 * However, the NIC can gracefully prevent such situations
		 * to happen by sending specific "back-pressure" flow control
		 * frames to its peer(s).
		 */
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_id=%u "
			   "status=0x%x data_len=%u",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) status,
			   (unsigned) rte_le_to_cpu_16(rxd.length));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (unsigned) rxq->port_id,
				   (unsigned) rxq->queue_id);
			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_em_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_em_prefetch(&rx_ring[rx_id]);
			rte_em_prefetch(&sw_ring[rx_id]);
		}

		/*
		 * Update RX descriptor with the physical address of the new
		 * data buffer of the new allocated mbuf.
		 */
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma = rte_cpu_to_le_64(rte_mbuf_data_dma_addr_default(nmb));
		rxdp->buffer_addr = dma;
		rxdp->status = 0;

		/*
		 * Set data length & data buffer address of mbuf.
		 */
		data_len = rte_le_to_cpu_16(rxd.length);
		rxm->data_len = data_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;

		/*
		 * If this is the first buffer of the received packet,
		 * set the pointer to the first mbuf of the packet and
		 * initialize its context.
		 * Otherwise, update the total length and the number of segments
		 * of the current scattered packet, and update the pointer to
		 * the last mbuf of the current packet.
		 */
		if (first_seg == NULL) {
			first_seg = rxm;
			first_seg->pkt_len = data_len;
			first_seg->nb_segs = 1;
		} else {
			first_seg->pkt_len += data_len;
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		/*
		 * If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (! (status & E1000_RXD_STAT_EOP)) {
			last_seg = rxm;
			goto next_desc;
		}

		/*
		 * This is the last buffer of the received packet.
		 * If the CRC is not stripped by the hardware:
		 *   - Subtract the CRC	length from the total packet length.
		 *   - If the last buffer only contains the whole CRC or a part
		 *     of it, free the mbuf associated to the last buffer.
		 *     If part of the CRC is also contained in the previous
		 *     mbuf, subtract the length of that CRC part from the
		 *     data length of the previous mbuf.
		 */
		rxm->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= ETHER_CRC_LEN;
			if (data_len <= ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->nb_segs--;
				last_seg->data_len = (uint16_t)
					(last_seg->data_len -
					 (ETHER_CRC_LEN - data_len));
				last_seg->next = NULL;
			} else
				rxm->data_len =
					(uint16_t) (data_len - ETHER_CRC_LEN);
		}

		/*
		 * Initialize the first mbuf of the returned packet:
		 *    - RX port identifier,
		 *    - hardware offload data, if any:
		 *      - IP checksum flag,
		 *      - error flags.
		 */
		first_seg->port = rxq->port_id;

		first_seg->ol_flags = rx_desc_status_to_pkt_flags(status);
		first_seg->ol_flags = first_seg->ol_flags |
					rx_desc_error_to_pkt_flags(rxd.errors);

		/* Only valid if PKT_RX_VLAN_PKT set in pkt_flags */
		rxm->vlan_tci = rte_le_to_cpu_16(rxd.special);

		/* Prefetch data of first segment, if configured to do so. */
		rte_packet_prefetch((char *)first_seg->buf_addr +
			first_seg->data_off);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = first_seg;

		/*
		 * Setup receipt context for a new packet.
		 */
		first_seg = NULL;
	}

	/*
	 * Record index of the next RX descriptor to probe.
	 */
	rxq->rx_tail = rx_id;

	/*
	 * Save receive context.
	 */
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	/*
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed RX descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situtation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t) (nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) nb_hold,
			   (unsigned) nb_rx);
		rx_id = (uint16_t) ((rx_id == 0) ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		E1000_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

#define	EM_MAX_BUF_SIZE     16384
#define EM_RCTL_FLXBUF_STEP 1024

static void
em_tx_queue_release_mbufs(struct em_tx_queue *txq)
{
	unsigned i;

	if (txq->sw_ring != NULL) {
		for (i = 0; i != txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
em_tx_queue_release(struct em_tx_queue *txq)
{
	if (txq != NULL) {
		em_tx_queue_release_mbufs(txq);
		rte_free(txq->sw_ring);
		rte_free(txq);
	}
}

void
eth_em_tx_queue_release(void *txq)
{
	em_tx_queue_release(txq);
}

/* (Re)set dynamic em_tx_queue fields to defaults */
static void
em_reset_tx_queue(struct em_tx_queue *txq)
{
	uint16_t i, nb_desc, prev;
	static const struct e1000_data_desc txd_init = {
		.upper.fields = {.status = E1000_TXD_STAT_DD},
	};

	nb_desc = txq->nb_tx_desc;

	/* Initialize ring entries */

	prev = (uint16_t) (nb_desc - 1);

	for (i = 0; i < nb_desc; i++) {
		txq->tx_ring[i] = txd_init;
		txq->sw_ring[i].mbuf = NULL;
		txq->sw_ring[i].last_id = i;
		txq->sw_ring[prev].next_id = i;
		prev = i;
	}

	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->nb_tx_free = (uint16_t)(nb_desc - 1);
	txq->last_desc_cleaned = (uint16_t)(nb_desc - 1);
	txq->nb_tx_used = 0;
	txq->tx_tail = 0;

	memset((void*)&txq->ctx_cache, 0, sizeof (txq->ctx_cache));
}

int
eth_em_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct em_tx_queue *txq;
	struct e1000_hw     *hw;
	uint32_t tsize;
	uint16_t tx_rs_thresh, tx_free_thresh;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of E1000_ALIGN.
	 */
	if (nb_desc % EM_TXD_ALIGN != 0 ||
			(nb_desc > E1000_MAX_RING_DESC) ||
			(nb_desc < E1000_MIN_RING_DESC)) {
		return -(EINVAL);
	}

	tx_free_thresh = tx_conf->tx_free_thresh;
	if (tx_free_thresh == 0)
		tx_free_thresh = (uint16_t)RTE_MIN(nb_desc / 4,
					DEFAULT_TX_FREE_THRESH);

	tx_rs_thresh = tx_conf->tx_rs_thresh;
	if (tx_rs_thresh == 0)
		tx_rs_thresh = (uint16_t)RTE_MIN(tx_free_thresh,
					DEFAULT_TX_RS_THRESH);

	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR, "tx_free_thresh must be less than the "
			     "number of TX descriptors minus 3. "
			     "(tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_rs_thresh > tx_free_thresh) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than or equal to "
			     "tx_free_thresh. (tx_free_thresh=%u "
			     "tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -(EINVAL);
	}

	/*
	 * If rs_bit_thresh is greater than 1, then TX WTHRESH should be
	 * set to 0. If WTHRESH is greater than zero, the RS bit is ignored
	 * by the NIC and all descriptors are written back after the NIC
	 * accumulates WTHRESH descriptors.
	 */
	if (tx_conf->tx_thresh.wthresh != 0 && tx_rs_thresh != 1) {
		PMD_INIT_LOG(ERR, "TX WTHRESH must be set to 0 if "
			     "tx_rs_thresh is greater than 1. (tx_rs_thresh=%u "
			     "port=%d queue=%d)", (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		em_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tsize = sizeof(txq->tx_ring[0]) * E1000_MAX_RING_DESC;
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, tsize,
				      RTE_CACHE_LINE_SIZE, socket_id);
	if (tz == NULL)
		return -ENOMEM;

	/* Allocate the tx queue data structure. */
	if ((txq = rte_zmalloc("ethdev TX queue", sizeof(*txq),
			RTE_CACHE_LINE_SIZE)) == NULL)
		return -ENOMEM;

	/* Allocate software ring */
	if ((txq->sw_ring = rte_zmalloc("txq->sw_ring",
			sizeof(txq->sw_ring[0]) * nb_desc,
			RTE_CACHE_LINE_SIZE)) == NULL) {
		em_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh = tx_free_thresh;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;

	txq->tdt_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_TDT(queue_idx));
	txq->tx_ring_phys_addr = rte_mem_phy2mch(tz->memseg_id, tz->phys_addr);
	txq->tx_ring = (struct e1000_data_desc *) tz->addr;

	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64,
		     txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	em_reset_tx_queue(txq);

	dev->data->tx_queues[queue_idx] = txq;
	return 0;
}

static void
em_rx_queue_release_mbufs(struct em_rx_queue *rxq)
{
	unsigned i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i != rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
em_rx_queue_release(struct em_rx_queue *rxq)
{
	if (rxq != NULL) {
		em_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

void
eth_em_rx_queue_release(void *rxq)
{
	em_rx_queue_release(rxq);
}

/* Reset dynamic em_rx_queue fields back to defaults */
static void
em_reset_rx_queue(struct em_rx_queue *rxq)
{
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

int
eth_em_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct em_rx_queue *rxq;
	struct e1000_hw     *hw;
	uint32_t rsize;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of E1000_ALIGN.
	 */
	if (nb_desc % EM_RXD_ALIGN != 0 ||
			(nb_desc > E1000_MAX_RING_DESC) ||
			(nb_desc < E1000_MIN_RING_DESC)) {
		return -EINVAL;
	}

	/*
	 * EM devices don't support drop_en functionality
	 */
	if (rx_conf->rx_drop_en) {
		PMD_INIT_LOG(ERR, "drop_en functionality not supported by "
			     "device");
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		em_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate RX ring for max possible mumber of hardware descriptors. */
	rsize = sizeof(rxq->rx_ring[0]) * E1000_MAX_RING_DESC;
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx, rsize,
				      RTE_CACHE_LINE_SIZE, socket_id);
	if (rz == NULL)
		return -ENOMEM;

	/* Allocate the RX queue data structure. */
	if ((rxq = rte_zmalloc("ethdev RX queue", sizeof(*rxq),
			RTE_CACHE_LINE_SIZE)) == NULL)
		return -ENOMEM;

	/* Allocate software ring. */
	if ((rxq->sw_ring = rte_zmalloc("rxq->sw_ring",
			sizeof (rxq->sw_ring[0]) * nb_desc,
			RTE_CACHE_LINE_SIZE)) == NULL) {
		em_rx_queue_release(rxq);
		return -ENOMEM;
	}

	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->pthresh = rx_conf->rx_thresh.pthresh;
	rxq->hthresh = rx_conf->rx_thresh.hthresh;
	rxq->wthresh = rx_conf->rx_thresh.wthresh;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->crc_len = (uint8_t) ((dev->data->dev_conf.rxmode.hw_strip_crc) ?
				0 : ETHER_CRC_LEN);

	rxq->rdt_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_RDT(queue_idx));
	rxq->rdh_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_RDH(queue_idx));
	rxq->rx_ring_phys_addr = rte_mem_phy2mch(rz->memseg_id, rz->phys_addr);
	rxq->rx_ring = (struct e1000_rx_desc *) rz->addr;

	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64,
		     rxq->sw_ring, rxq->rx_ring, rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;
	em_reset_rx_queue(rxq);

	return 0;
}

uint32_t
eth_em_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
#define EM_RXQ_SCAN_INTERVAL 4
	volatile struct e1000_rx_desc *rxdp;
	struct em_rx_queue *rxq;
	uint32_t desc = 0;

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_RX_LOG(DEBUG, "Invalid RX queue_id=%d", rx_queue_id);
		return 0;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	rxdp = &(rxq->rx_ring[rxq->rx_tail]);

	while ((desc < rxq->nb_rx_desc) &&
		(rxdp->status & E1000_RXD_STAT_DD)) {
		desc += EM_RXQ_SCAN_INTERVAL;
		rxdp += EM_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
				desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
eth_em_rx_descriptor_done(void *rx_queue, uint16_t offset)
{
	volatile struct e1000_rx_desc *rxdp;
	struct em_rx_queue *rxq = rx_queue;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return 0;
	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	rxdp = &rxq->rx_ring[desc];
	return !!(rxdp->status & E1000_RXD_STAT_DD);
}

void
em_dev_clear_queues(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct em_tx_queue *txq;
	struct em_rx_queue *rxq;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq != NULL) {
			em_tx_queue_release_mbufs(txq);
			em_reset_tx_queue(txq);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq != NULL) {
			em_rx_queue_release_mbufs(rxq);
			em_reset_rx_queue(rxq);
		}
	}
}

void
em_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		eth_em_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		eth_em_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

/*
 * Takes as input/output parameter RX buffer size.
 * Returns (BSIZE | BSEX | FLXBUF) fields of RCTL register.
 */
static uint32_t
em_rctl_bsize(__rte_unused enum e1000_mac_type hwtyp, uint32_t *bufsz)
{
	/*
	 * For BSIZE & BSEX all configurable sizes are:
	 * 16384: rctl |= (E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX);
	 *  8192: rctl |= (E1000_RCTL_SZ_8192  | E1000_RCTL_BSEX);
	 *  4096: rctl |= (E1000_RCTL_SZ_4096  | E1000_RCTL_BSEX);
	 *  2048: rctl |= E1000_RCTL_SZ_2048;
	 *  1024: rctl |= E1000_RCTL_SZ_1024;
	 *   512: rctl |= E1000_RCTL_SZ_512;
	 *   256: rctl |= E1000_RCTL_SZ_256;
	 */
	static const struct {
		uint32_t bufsz;
		uint32_t rctl;
	} bufsz_to_rctl[] = {
		{16384, (E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX)},
		{8192,  (E1000_RCTL_SZ_8192  | E1000_RCTL_BSEX)},
		{4096,  (E1000_RCTL_SZ_4096  | E1000_RCTL_BSEX)},
		{2048,  E1000_RCTL_SZ_2048},
		{1024,  E1000_RCTL_SZ_1024},
		{512,   E1000_RCTL_SZ_512},
		{256,   E1000_RCTL_SZ_256},
	};

	int i;
	uint32_t rctl_bsize;

	rctl_bsize = *bufsz;

	/*
	 * Starting from 82571 it is possible to specify RX buffer size
	 * by RCTL.FLXBUF. When this field is different from zero, the
	 * RX buffer size = RCTL.FLXBUF * 1K
	 * (e.g. t is possible to specify RX buffer size  1,2,...,15KB).
	 * It is working ok on real HW, but by some reason doesn't work
	 * on VMware emulated 82574L.
	 * So for now, always use BSIZE/BSEX to setup RX buffer size.
	 * If you don't plan to use it on VMware emulated 82574L and
	 * would like to specify RX buffer size in 1K granularity,
	 * uncomment the following lines:
	 * ***************************************************************
	 * if (hwtyp >= e1000_82571 && hwtyp <= e1000_82574 &&
	 *		rctl_bsize >= EM_RCTL_FLXBUF_STEP) {
	 *	rctl_bsize /= EM_RCTL_FLXBUF_STEP;
	 *	*bufsz = rctl_bsize;
	 *	return (rctl_bsize << E1000_RCTL_FLXBUF_SHIFT &
	 *		E1000_RCTL_FLXBUF_MASK);
	 * }
	 * ***************************************************************
	 */

	for (i = 0; i != sizeof(bufsz_to_rctl) / sizeof(bufsz_to_rctl[0]);
			i++) {
		if (rctl_bsize >= bufsz_to_rctl[i].bufsz) {
			*bufsz = bufsz_to_rctl[i].bufsz;
			return bufsz_to_rctl[i].rctl;
		}
	}

	/* Should never happen. */
	return -EINVAL;
}

static int
em_alloc_rx_queue_mbufs(struct em_rx_queue *rxq)
{
	struct em_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned i;
	static const struct e1000_rx_desc rxd_init = {
		.buffer_addr = 0,
	};

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile struct e1000_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "RX mbuf alloc failed "
				     "queue_id=%hu", rxq->queue_id);
			return -ENOMEM;
		}

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_dma_addr_default(mbuf));

		/* Clear HW ring memory */
		rxq->rx_ring[i] = rxd_init;

		rxd = &rxq->rx_ring[i];
		rxd->buffer_addr = dma_addr;
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
int
eth_em_rx_init(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw;
	struct em_rx_queue *rxq;
	uint32_t rctl;
	uint32_t rfctl;
	uint32_t rxcsum;
	uint32_t rctl_bsize;
	uint16_t i;
	int ret;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring.
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	rfctl = E1000_READ_REG(hw, E1000_RFCTL);

	/* Disable extended descriptor type. */
	rfctl &= ~E1000_RFCTL_EXTEN;
	/* Disable accelerated acknowledge */
	if (hw->mac.type == e1000_82574)
		rfctl |= E1000_RFCTL_ACK_DIS;

	E1000_WRITE_REG(hw, E1000_RFCTL, rfctl);

	/*
	 * XXX TEMPORARY WORKAROUND: on some systems with 82573
	 * long latencies are observed, like Lenovo X60. This
	 * change eliminates the problem, but since having positive
	 * values in RDTR is a known source of problems on other
	 * platforms another solution is being sought.
	 */
	if (hw->mac.type == e1000_82573)
		E1000_WRITE_REG(hw, E1000_RDTR, 0x20);

	dev->rx_pkt_burst = (eth_rx_burst_t)eth_em_recv_pkts;

	/* Determine RX bufsize. */
	rctl_bsize = EM_MAX_BUF_SIZE;
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		uint32_t buf_size;

		rxq = dev->data->rx_queues[i];
		buf_size = rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM;
		rctl_bsize = RTE_MIN(rctl_bsize, buf_size);
	}

	rctl |= em_rctl_bsize(hw->mac.type, &rctl_bsize);

	/* Configure and enable each RX queue. */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		uint64_t bus_addr;
		uint32_t rxdctl;

		rxq = dev->data->rx_queues[i];

		/* Allocate buffers for descriptor rings and setup queue */
		ret = em_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 *  call to configure
		 */
		rxq->crc_len =
			(uint8_t)(dev->data->dev_conf.rxmode.hw_strip_crc ?
							0 : ETHER_CRC_LEN);

		bus_addr = rxq->rx_ring_phys_addr;
		E1000_WRITE_REG(hw, E1000_RDLEN(i),
				rxq->nb_rx_desc *
				sizeof(*rxq->rx_ring));
		E1000_WRITE_REG(hw, E1000_RDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i), (uint32_t)bus_addr);

		E1000_WRITE_REG(hw, E1000_RDH(i), 0);
		E1000_WRITE_REG(hw, E1000_RDT(i), rxq->nb_rx_desc - 1);

		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(0));
		rxdctl &= 0xFE000000;
		rxdctl |= rxq->pthresh & 0x3F;
		rxdctl |= (rxq->hthresh & 0x3F) << 8;
		rxdctl |= (rxq->wthresh & 0x3F) << 16;
		rxdctl |= E1000_RXDCTL_GRAN;
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);

		/*
		 * Due to EM devices not having any sort of hardware
		 * limit for packet length, jumbo frame of any size
		 * can be accepted, thus we have to enable scattered
		 * rx if jumbo frames are enabled (or if buffer size
		 * is too small to accommodate non-jumbo packets)
		 * to avoid splitting packets that don't fit into
		 * one buffer.
		 */
		if (dev->data->dev_conf.rxmode.jumbo_frame ||
				rctl_bsize < ETHER_MAX_LEN) {
			if (!dev->data->scattered_rx)
				PMD_INIT_LOG(DEBUG, "forcing scatter mode");
			dev->rx_pkt_burst =
				(eth_rx_burst_t)eth_em_recv_scattered_pkts;
			dev->data->scattered_rx = 1;
		}
	}

	if (dev->data->dev_conf.rxmode.enable_scatter) {
		if (!dev->data->scattered_rx)
			PMD_INIT_LOG(DEBUG, "forcing scatter mode");
		dev->rx_pkt_burst = eth_em_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	/*
	 * Setup the Checksum Register.
	 * Receive Full-Packet Checksum Offload is mutually exclusive with RSS.
	 */
	rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);

	if (dev->data->dev_conf.rxmode.hw_ip_checksum)
		rxcsum |= E1000_RXCSUM_IPOFL;
	else
		rxcsum &= ~E1000_RXCSUM_IPOFL;
	E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);

	/* No MRQ or RSS support for now */

	/* Set early receive threshold on appropriate hw */
	if ((hw->mac.type == e1000_ich9lan ||
			hw->mac.type == e1000_pch2lan ||
			hw->mac.type == e1000_ich10lan) &&
			dev->data->dev_conf.rxmode.jumbo_frame == 1) {
		u32 rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(0));
		E1000_WRITE_REG(hw, E1000_RXDCTL(0), rxdctl | 3);
		E1000_WRITE_REG(hw, E1000_ERT, 0x100 | (1 << 13));
	}

	if (hw->mac.type == e1000_pch2lan) {
		if (dev->data->dev_conf.rxmode.jumbo_frame == 1)
			e1000_lv_jumbo_workaround_ich8lan(hw, TRUE);
		else
			e1000_lv_jumbo_workaround_ich8lan(hw, FALSE);
	}

	/* Setup the Receive Control Register. */
	if (dev->data->dev_conf.rxmode.hw_strip_crc)
		rctl |= E1000_RCTL_SECRC; /* Strip Ethernet CRC. */
	else
		rctl &= ~E1000_RCTL_SECRC; /* Do not Strip Ethernet CRC. */

	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		E1000_RCTL_RDMTS_HALF |
		(hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	/* Make sure VLAN Filters are off. */
	rctl &= ~E1000_RCTL_VFE;
	/* Don't store bad packets. */
	rctl &= ~E1000_RCTL_SBP;
	/* Legacy descriptor type. */
	rctl &= ~E1000_RCTL_DTYP_MASK;

	/*
	 * Configure support of jumbo frames, if any.
	 */
	if (dev->data->dev_conf.rxmode.jumbo_frame == 1)
		rctl |= E1000_RCTL_LPE;
	else
		rctl &= ~E1000_RCTL_LPE;

	/* Enable Receives. */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void
eth_em_tx_init(struct rte_eth_dev *dev)
{
	struct e1000_hw     *hw;
	struct em_tx_queue *txq;
	uint32_t tctl;
	uint32_t txdctl;
	uint16_t i;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Setup the Base and Length of the Tx Descriptor Rings. */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		uint64_t bus_addr;

		txq = dev->data->tx_queues[i];
		bus_addr = txq->tx_ring_phys_addr;
		E1000_WRITE_REG(hw, E1000_TDLEN(i),
				txq->nb_tx_desc *
				sizeof(*txq->tx_ring));
		E1000_WRITE_REG(hw, E1000_TDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(i), (uint32_t)bus_addr);

		/* Setup the HW Tx Head and Tail descriptor pointers. */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		/* Setup Transmit threshold registers. */
		txdctl = E1000_READ_REG(hw, E1000_TXDCTL(i));
		/*
		 * bit 22 is reserved, on some models should always be 0,
		 * on others  - always 1.
		 */
		txdctl &= E1000_TXDCTL_COUNT_DESC;
		txdctl |= txq->pthresh & 0x3F;
		txdctl |= (txq->hthresh & 0x3F) << 8;
		txdctl |= (txq->wthresh & 0x3F) << 16;
		txdctl |= E1000_TXDCTL_GRAN;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
	}

	/* Program the Transmit Control Register. */
	tctl = E1000_READ_REG(hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		 (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(hw, E1000_TCTL, tctl);
}

void
em_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct em_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;
	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
}

void
em_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct em_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;
	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.tx_rs_thresh = txq->tx_rs_thresh;
}
