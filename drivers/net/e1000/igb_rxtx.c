/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
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
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_prefetch.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_net.h>
#include <rte_string_fns.h>

#include "e1000_logs.h"
#include "base/e1000_api.h"
#include "e1000_ethdev.h"

#ifdef RTE_LIBRTE_IEEE1588
#define IGB_TX_IEEE1588_TMST PKT_TX_IEEE1588_TMST
#else
#define IGB_TX_IEEE1588_TMST 0
#endif
/* Bit Mask to indicate what bits required for building TX context */
#define IGB_TX_OFFLOAD_MASK (			 \
		PKT_TX_VLAN_PKT |		 \
		PKT_TX_IP_CKSUM |		 \
		PKT_TX_L4_MASK |		 \
		PKT_TX_TCP_SEG |		 \
		IGB_TX_IEEE1588_TMST)

#define IGB_TX_OFFLOAD_NOTSUP_MASK \
		(PKT_TX_OFFLOAD_MASK ^ IGB_TX_OFFLOAD_MASK)

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
 * rx queue flags
 */
enum igb_rxq_flags {
	IGB_RXQ_FLAG_LB_BSWAP_VLAN = 0x01,
};

/**
 * Structure associated with each RX queue.
 */
struct igb_rx_queue {
	struct rte_mempool  *mb_pool;   /**< mbuf pool to populate RX ring. */
	volatile union e1000_adv_rx_desc *rx_ring; /**< RX ring virtual address. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	volatile uint32_t   *rdt_reg_addr; /**< RDT register address. */
	volatile uint32_t   *rdh_reg_addr; /**< RDH register address. */
	struct igb_rx_entry *sw_ring;   /**< address of RX software ring. */
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg;  /**< Last segment of current packet. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            rx_tail;    /**< current value of RDT register. */
	uint16_t            nb_rx_hold; /**< number of held free RX desc. */
	uint16_t            rx_free_thresh; /**< max free RX desc to hold. */
	uint16_t            queue_id;   /**< RX queue index. */
	uint16_t            reg_idx;    /**< RX queue register index. */
	uint16_t            port_id;    /**< Device port identifier. */
	uint8_t             pthresh;    /**< Prefetch threshold register. */
	uint8_t             hthresh;    /**< Host threshold register. */
	uint8_t             wthresh;    /**< Write-back threshold register. */
	uint8_t             crc_len;    /**< 0 if CRC stripped, 4 otherwise. */
	uint8_t             drop_en;  /**< If not 0, set SRRCTL.Drop_En. */
	uint32_t            flags;      /**< RX flags. */
	uint64_t	    offloads;   /**< offloads of DEV_RX_OFFLOAD_* */
};

/**
 * Hardware context number
 */
enum igb_advctx_num {
	IGB_CTX_0    = 0, /**< CTX0    */
	IGB_CTX_1    = 1, /**< CTX1    */
	IGB_CTX_NUM  = 2, /**< CTX_NUM */
};

/** Offload features */
union igb_tx_offload {
	uint64_t data;
	struct {
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t vlan_tci:16;  /**< VLAN Tag Control Identifier(CPU order). */
		uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size. */

		/* uint64_t unused:8; */
	};
};

/*
 * Compare mask for igb_tx_offload.data,
 * should be in sync with igb_tx_offload layout.
 * */
#define TX_MACIP_LEN_CMP_MASK	0x000000000000FFFFULL /**< L2L3 header mask. */
#define TX_VLAN_CMP_MASK		0x00000000FFFF0000ULL /**< Vlan mask. */
#define TX_TCP_LEN_CMP_MASK		0x000000FF00000000ULL /**< TCP header mask. */
#define TX_TSO_MSS_CMP_MASK		0x00FFFF0000000000ULL /**< TSO segsz mask. */
/** Mac + IP + TCP + Mss mask. */
#define TX_TSO_CMP_MASK	\
	(TX_MACIP_LEN_CMP_MASK | TX_TCP_LEN_CMP_MASK | TX_TSO_MSS_CMP_MASK)

/**
 * Strucutre to check if new context need be built
 */
struct igb_advctx_info {
	uint64_t flags;           /**< ol_flags related to context build. */
	/** tx offload: vlan, tso, l2-l3-l4 lengths. */
	union igb_tx_offload tx_offload;
	/** compare mask for tx offload. */
	union igb_tx_offload tx_offload_mask;
};

/**
 * Structure associated with each TX queue.
 */
struct igb_tx_queue {
	volatile union e1000_adv_tx_desc *tx_ring; /**< TX ring address */
	uint64_t               tx_ring_phys_addr; /**< TX ring DMA address. */
	struct igb_tx_entry    *sw_ring; /**< virtual address of SW ring. */
	volatile uint32_t      *tdt_reg_addr; /**< Address of TDT register. */
	uint32_t               txd_type;      /**< Device-specific TXD type */
	uint16_t               nb_tx_desc;    /**< number of TX descriptors. */
	uint16_t               tx_tail; /**< Current value of TDT register. */
	uint16_t               tx_head;
	/**< Index of first used TX descriptor. */
	uint16_t               queue_id; /**< TX queue index. */
	uint16_t               reg_idx;  /**< TX queue register index. */
	uint16_t               port_id;  /**< Device port identifier. */
	uint8_t                pthresh;  /**< Prefetch threshold register. */
	uint8_t                hthresh;  /**< Host threshold register. */
	uint8_t                wthresh;  /**< Write-back threshold register. */
	uint32_t               ctx_curr;
	/**< Current used hardware descriptor. */
	uint32_t               ctx_start;
	/**< Start context position for transmit queue. */
	struct igb_advctx_info ctx_cache[IGB_CTX_NUM];
	/**< Hardware context history.*/
};

#if 1
#define RTE_PMD_USE_PREFETCH
#endif

#ifdef RTE_PMD_USE_PREFETCH
#define rte_igb_prefetch(p)	rte_prefetch0(p)
#else
#define rte_igb_prefetch(p)	do {} while(0)
#endif

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p) rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)	do {} while(0)
#endif

/*
 * Macro for VMDq feature for 1 GbE NIC.
 */
#define E1000_VMOLR_SIZE			(8)
#define IGB_TSO_MAX_HDRLEN			(512)
#define IGB_TSO_MAX_MSS				(9216)

/*********************************************************************
 *
 *  TX function
 *
 **********************************************************************/

/*
 *There're some limitations in hardware for TCP segmentation offload. We
 *should check whether the parameters are valid.
 */
static inline uint64_t
check_tso_para(uint64_t ol_req, union igb_tx_offload ol_para)
{
	if (!(ol_req & PKT_TX_TCP_SEG))
		return ol_req;
	if ((ol_para.tso_segsz > IGB_TSO_MAX_MSS) || (ol_para.l2_len +
			ol_para.l3_len + ol_para.l4_len > IGB_TSO_MAX_HDRLEN)) {
		ol_req &= ~PKT_TX_TCP_SEG;
		ol_req |= PKT_TX_TCP_CKSUM;
	}
	return ol_req;
}

/*
 * Advanced context descriptor are almost same between igb/ixgbe
 * This is a separate function, looking for optimization opportunity here
 * Rework required to go with the pre-defined values.
 */

static inline void
igbe_set_xmit_ctx(struct igb_tx_queue* txq,
		volatile struct e1000_adv_tx_context_desc *ctx_txd,
		uint64_t ol_flags, union igb_tx_offload tx_offload)
{
	uint32_t type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
	uint32_t ctx_idx, ctx_curr;
	uint32_t vlan_macip_lens;
	union igb_tx_offload tx_offload_mask;

	ctx_curr = txq->ctx_curr;
	ctx_idx = ctx_curr + txq->ctx_start;

	tx_offload_mask.data = 0;
	type_tucmd_mlhl = 0;

	/* Specify which HW CTX to upload. */
	mss_l4len_idx = (ctx_idx << E1000_ADVTXD_IDX_SHIFT);

	if (ol_flags & PKT_TX_VLAN_PKT)
		tx_offload_mask.data |= TX_VLAN_CMP_MASK;

	/* check if TCP segmentation required for this packet */
	if (ol_flags & PKT_TX_TCP_SEG) {
		/* implies IP cksum in IPv4 */
		if (ol_flags & PKT_TX_IP_CKSUM)
			type_tucmd_mlhl = E1000_ADVTXD_TUCMD_IPV4 |
				E1000_ADVTXD_TUCMD_L4T_TCP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
		else
			type_tucmd_mlhl = E1000_ADVTXD_TUCMD_IPV6 |
				E1000_ADVTXD_TUCMD_L4T_TCP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;

		tx_offload_mask.data |= TX_TSO_CMP_MASK;
		mss_l4len_idx |= tx_offload.tso_segsz << E1000_ADVTXD_MSS_SHIFT;
		mss_l4len_idx |= tx_offload.l4_len << E1000_ADVTXD_L4LEN_SHIFT;
	} else { /* no TSO, check if hardware checksum is needed */
		if (ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_L4_MASK))
			tx_offload_mask.data |= TX_MACIP_LEN_CMP_MASK;

		if (ol_flags & PKT_TX_IP_CKSUM)
			type_tucmd_mlhl = E1000_ADVTXD_TUCMD_IPV4;

		switch (ol_flags & PKT_TX_L4_MASK) {
		case PKT_TX_UDP_CKSUM:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_UDP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			mss_l4len_idx |= sizeof(struct udp_hdr) << E1000_ADVTXD_L4LEN_SHIFT;
			break;
		case PKT_TX_TCP_CKSUM:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_TCP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			mss_l4len_idx |= sizeof(struct tcp_hdr) << E1000_ADVTXD_L4LEN_SHIFT;
			break;
		case PKT_TX_SCTP_CKSUM:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_SCTP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			mss_l4len_idx |= sizeof(struct sctp_hdr) << E1000_ADVTXD_L4LEN_SHIFT;
			break;
		default:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_RSV |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			break;
		}
	}

	txq->ctx_cache[ctx_curr].flags = ol_flags;
	txq->ctx_cache[ctx_curr].tx_offload.data =
		tx_offload_mask.data & tx_offload.data;
	txq->ctx_cache[ctx_curr].tx_offload_mask = tx_offload_mask;

	ctx_txd->type_tucmd_mlhl = rte_cpu_to_le_32(type_tucmd_mlhl);
	vlan_macip_lens = (uint32_t)tx_offload.data;
	ctx_txd->vlan_macip_lens = rte_cpu_to_le_32(vlan_macip_lens);
	ctx_txd->mss_l4len_idx = rte_cpu_to_le_32(mss_l4len_idx);
	ctx_txd->seqnum_seed = 0;
}

/*
 * Check which hardware context can be used. Use the existing match
 * or create a new context descriptor.
 */
static inline uint32_t
what_advctx_update(struct igb_tx_queue *txq, uint64_t flags,
		union igb_tx_offload tx_offload)
{
	/* If match with the current context */
	if (likely((txq->ctx_cache[txq->ctx_curr].flags == flags) &&
		(txq->ctx_cache[txq->ctx_curr].tx_offload.data ==
		(txq->ctx_cache[txq->ctx_curr].tx_offload_mask.data & tx_offload.data)))) {
			return txq->ctx_curr;
	}

	/* If match with the second context */
	txq->ctx_curr ^= 1;
	if (likely((txq->ctx_cache[txq->ctx_curr].flags == flags) &&
		(txq->ctx_cache[txq->ctx_curr].tx_offload.data ==
		(txq->ctx_cache[txq->ctx_curr].tx_offload_mask.data & tx_offload.data)))) {
			return txq->ctx_curr;
	}

	/* Mismatch, use the previous context */
	return IGB_CTX_NUM;
}

static inline uint32_t
tx_desc_cksum_flags_to_olinfo(uint64_t ol_flags)
{
	static const uint32_t l4_olinfo[2] = {0, E1000_ADVTXD_POPTS_TXSM};
	static const uint32_t l3_olinfo[2] = {0, E1000_ADVTXD_POPTS_IXSM};
	uint32_t tmp;

	tmp  = l4_olinfo[(ol_flags & PKT_TX_L4_MASK)  != PKT_TX_L4_NO_CKSUM];
	tmp |= l3_olinfo[(ol_flags & PKT_TX_IP_CKSUM) != 0];
	tmp |= l4_olinfo[(ol_flags & PKT_TX_TCP_SEG) != 0];
	return tmp;
}

static inline uint32_t
tx_desc_vlan_flags_to_cmdtype(uint64_t ol_flags)
{
	uint32_t cmdtype;
	static uint32_t vlan_cmd[2] = {0, E1000_ADVTXD_DCMD_VLE};
	static uint32_t tso_cmd[2] = {0, E1000_ADVTXD_DCMD_TSE};
	cmdtype = vlan_cmd[(ol_flags & PKT_TX_VLAN_PKT) != 0];
	cmdtype |= tso_cmd[(ol_flags & PKT_TX_TCP_SEG) != 0];
	return cmdtype;
}

uint16_t
eth_igb_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	       uint16_t nb_pkts)
{
	struct igb_tx_queue *txq;
	struct igb_tx_entry *sw_ring;
	struct igb_tx_entry *txe, *txn;
	volatile union e1000_adv_tx_desc *txr;
	volatile union e1000_adv_tx_desc *txd;
	struct rte_mbuf     *tx_pkt;
	struct rte_mbuf     *m_seg;
	uint64_t buf_dma_addr;
	uint32_t olinfo_status;
	uint32_t cmd_type_len;
	uint32_t pkt_len;
	uint16_t slen;
	uint64_t ol_flags;
	uint16_t tx_end;
	uint16_t tx_id;
	uint16_t tx_last;
	uint16_t nb_tx;
	uint64_t tx_ol_req;
	uint32_t new_ctx = 0;
	uint32_t ctx = 0;
	union igb_tx_offload tx_offload = {0};

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr     = txq->tx_ring;
	tx_id   = txq->tx_tail;
	txe = &sw_ring[tx_id];

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;
		pkt_len = tx_pkt->pkt_len;

		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		/*
		 * The number of descriptors that must be allocated for a
		 * packet is the number of segments of that packet, plus 1
		 * Context Descriptor for the VLAN Tag Identifier, if any.
		 * Determine the last TX descriptor to allocate in the TX ring
		 * for the packet, starting from the current position (tx_id)
		 * in the ring.
		 */
		tx_last = (uint16_t) (tx_id + tx_pkt->nb_segs - 1);

		ol_flags = tx_pkt->ol_flags;
		tx_ol_req = ol_flags & IGB_TX_OFFLOAD_MASK;

		/* If a Context Descriptor need be built . */
		if (tx_ol_req) {
			tx_offload.l2_len = tx_pkt->l2_len;
			tx_offload.l3_len = tx_pkt->l3_len;
			tx_offload.l4_len = tx_pkt->l4_len;
			tx_offload.vlan_tci = tx_pkt->vlan_tci;
			tx_offload.tso_segsz = tx_pkt->tso_segsz;
			tx_ol_req = check_tso_para(tx_ol_req, tx_offload);

			ctx = what_advctx_update(txq, tx_ol_req, tx_offload);
			/* Only allocate context descriptor if required*/
			new_ctx = (ctx == IGB_CTX_NUM);
			ctx = txq->ctx_curr + txq->ctx_start;
			tx_last = (uint16_t) (tx_last + new_ctx);
		}
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t) (tx_last - txq->nb_tx_desc);

		PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u pktlen=%u"
			   " tx_first=%u tx_last=%u",
			   (unsigned) txq->port_id,
			   (unsigned) txq->queue_id,
			   (unsigned) pkt_len,
			   (unsigned) tx_id,
			   (unsigned) tx_last);

		/*
		 * Check if there are enough free descriptors in the TX ring
		 * to transmit the next packet.
		 * This operation is based on the two following rules:
		 *
		 *   1- Only check that the last needed TX descriptor can be
		 *      allocated (by construction, if that descriptor is free,
		 *      all intermediate ones are also free).
		 *
		 *      For this purpose, the index of the last TX descriptor
		 *      used for a packet (the "last descriptor" of a packet)
		 *      is recorded in the TX entries (the last one included)
		 *      that are associated with all TX descriptors allocated
		 *      for that packet.
		 *
		 *   2- Avoid to allocate the last free TX descriptor of the
		 *      ring, in order to never set the TDT register with the
		 *      same value stored in parallel by the NIC in the TDH
		 *      register, which makes the TX engine of the NIC enter
		 *      in a deadlock situation.
		 *
		 *      By extension, avoid to allocate a free descriptor that
		 *      belongs to the last set of free descriptors allocated
		 *      to the same packet previously transmitted.
		 */

		/*
		 * The "last descriptor" of the previously sent packet, if any,
		 * which used the last descriptor to allocate.
		 */
		tx_end = sw_ring[tx_last].last_id;

		/*
		 * The next descriptor following that "last descriptor" in the
		 * ring.
		 */
		tx_end = sw_ring[tx_end].next_id;

		/*
		 * The "last descriptor" associated with that next descriptor.
		 */
		tx_end = sw_ring[tx_end].last_id;

		/*
		 * Check that this descriptor is free.
		 */
		if (! (txr[tx_end].wb.status & E1000_TXD_STAT_DD)) {
			if (nb_tx == 0)
				return 0;
			goto end_of_tx;
		}

		/*
		 * Set common flags of all TX Data Descriptors.
		 *
		 * The following bits must be set in all Data Descriptors:
		 *   - E1000_ADVTXD_DTYP_DATA
		 *   - E1000_ADVTXD_DCMD_DEXT
		 *
		 * The following bits must be set in the first Data Descriptor
		 * and are ignored in the other ones:
		 *   - E1000_ADVTXD_DCMD_IFCS
		 *   - E1000_ADVTXD_MAC_1588
		 *   - E1000_ADVTXD_DCMD_VLE
		 *
		 * The following bits must only be set in the last Data
		 * Descriptor:
		 *   - E1000_TXD_CMD_EOP
		 *
		 * The following bits can be set in any Data Descriptor, but
		 * are only set in the last Data Descriptor:
		 *   - E1000_TXD_CMD_RS
		 */
		cmd_type_len = txq->txd_type |
			E1000_ADVTXD_DCMD_IFCS | E1000_ADVTXD_DCMD_DEXT;
		if (tx_ol_req & PKT_TX_TCP_SEG)
			pkt_len -= (tx_pkt->l2_len + tx_pkt->l3_len + tx_pkt->l4_len);
		olinfo_status = (pkt_len << E1000_ADVTXD_PAYLEN_SHIFT);
#if defined(RTE_LIBRTE_IEEE1588)
		if (ol_flags & PKT_TX_IEEE1588_TMST)
			cmd_type_len |= E1000_ADVTXD_MAC_TSTAMP;
#endif
		if (tx_ol_req) {
			/* Setup TX Advanced context descriptor if required */
			if (new_ctx) {
				volatile struct e1000_adv_tx_context_desc *
				    ctx_txd;

				ctx_txd = (volatile struct
				    e1000_adv_tx_context_desc *)
				    &txr[tx_id];

				txn = &sw_ring[txe->next_id];
				RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);

				if (txe->mbuf != NULL) {
					rte_pktmbuf_free_seg(txe->mbuf);
					txe->mbuf = NULL;
				}

				igbe_set_xmit_ctx(txq, ctx_txd, tx_ol_req, tx_offload);

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
			}

			/* Setup the TX Advanced Data Descriptor */
			cmd_type_len  |= tx_desc_vlan_flags_to_cmdtype(tx_ol_req);
			olinfo_status |= tx_desc_cksum_flags_to_olinfo(tx_ol_req);
			olinfo_status |= (ctx << E1000_ADVTXD_IDX_SHIFT);
		}

		m_seg = tx_pkt;
		do {
			txn = &sw_ring[txe->next_id];
			txd = &txr[tx_id];

			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/*
			 * Set up transmit descriptor.
			 */
			slen = (uint16_t) m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);
			txd->read.buffer_addr =
				rte_cpu_to_le_64(buf_dma_addr);
			txd->read.cmd_type_len =
				rte_cpu_to_le_32(cmd_type_len | slen);
			txd->read.olinfo_status =
				rte_cpu_to_le_32(olinfo_status);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg != NULL);

		/*
		 * The last packet data descriptor needs End Of Packet (EOP)
		 * and Report Status (RS).
		 */
		txd->read.cmd_type_len |=
			rte_cpu_to_le_32(E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
	}
 end_of_tx:
	rte_wmb();

	/*
	 * Set the Transmit Descriptor Tail (TDT).
	 */
	E1000_PCI_REG_WRITE_RELAXED(txq->tdt_reg_addr, tx_id);
	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (unsigned) txq->port_id, (unsigned) txq->queue_id,
		   (unsigned) tx_id, (unsigned) nb_tx);
	txq->tx_tail = tx_id;

	return nb_tx;
}

/*********************************************************************
 *
 *  TX prep functions
 *
 **********************************************************************/
uint16_t
eth_igb_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int i, ret;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];

		/* Check some limitations for TSO in hardware */
		if (m->ol_flags & PKT_TX_TCP_SEG)
			if ((m->tso_segsz > IGB_TSO_MAX_MSS) ||
					(m->l2_len + m->l3_len + m->l4_len >
					IGB_TSO_MAX_HDRLEN)) {
				rte_errno = -EINVAL;
				return i;
			}

		if (m->ol_flags & IGB_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = -ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
	}

	return i;
}

/*********************************************************************
 *
 *  RX functions
 *
 **********************************************************************/
#define IGB_PACKET_TYPE_IPV4              0X01
#define IGB_PACKET_TYPE_IPV4_TCP          0X11
#define IGB_PACKET_TYPE_IPV4_UDP          0X21
#define IGB_PACKET_TYPE_IPV4_SCTP         0X41
#define IGB_PACKET_TYPE_IPV4_EXT          0X03
#define IGB_PACKET_TYPE_IPV4_EXT_SCTP     0X43
#define IGB_PACKET_TYPE_IPV6              0X04
#define IGB_PACKET_TYPE_IPV6_TCP          0X14
#define IGB_PACKET_TYPE_IPV6_UDP          0X24
#define IGB_PACKET_TYPE_IPV6_EXT          0X0C
#define IGB_PACKET_TYPE_IPV6_EXT_TCP      0X1C
#define IGB_PACKET_TYPE_IPV6_EXT_UDP      0X2C
#define IGB_PACKET_TYPE_IPV4_IPV6         0X05
#define IGB_PACKET_TYPE_IPV4_IPV6_TCP     0X15
#define IGB_PACKET_TYPE_IPV4_IPV6_UDP     0X25
#define IGB_PACKET_TYPE_IPV4_IPV6_EXT     0X0D
#define IGB_PACKET_TYPE_IPV4_IPV6_EXT_TCP 0X1D
#define IGB_PACKET_TYPE_IPV4_IPV6_EXT_UDP 0X2D
#define IGB_PACKET_TYPE_MAX               0X80
#define IGB_PACKET_TYPE_MASK              0X7F
#define IGB_PACKET_TYPE_SHIFT             0X04
static inline uint32_t
igb_rxd_pkt_info_to_pkt_type(uint16_t pkt_info)
{
	static const uint32_t
		ptype_table[IGB_PACKET_TYPE_MAX] __rte_cache_aligned = {
		[IGB_PACKET_TYPE_IPV4] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4,
		[IGB_PACKET_TYPE_IPV4_EXT] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4_EXT,
		[IGB_PACKET_TYPE_IPV6] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6,
		[IGB_PACKET_TYPE_IPV4_IPV6] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6,
		[IGB_PACKET_TYPE_IPV6_EXT] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT,
		[IGB_PACKET_TYPE_IPV4_IPV6_EXT] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT,
		[IGB_PACKET_TYPE_IPV4_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
		[IGB_PACKET_TYPE_IPV6_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
		[IGB_PACKET_TYPE_IPV4_IPV6_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6 | RTE_PTYPE_INNER_L4_TCP,
		[IGB_PACKET_TYPE_IPV6_EXT_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_TCP,
		[IGB_PACKET_TYPE_IPV4_IPV6_EXT_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT | RTE_PTYPE_INNER_L4_TCP,
		[IGB_PACKET_TYPE_IPV4_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
		[IGB_PACKET_TYPE_IPV6_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
		[IGB_PACKET_TYPE_IPV4_IPV6_UDP] =  RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6 | RTE_PTYPE_INNER_L4_UDP,
		[IGB_PACKET_TYPE_IPV6_EXT_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_UDP,
		[IGB_PACKET_TYPE_IPV4_IPV6_EXT_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT | RTE_PTYPE_INNER_L4_UDP,
		[IGB_PACKET_TYPE_IPV4_SCTP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_SCTP,
		[IGB_PACKET_TYPE_IPV4_EXT_SCTP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_SCTP,
	};
	if (unlikely(pkt_info & E1000_RXDADV_PKTTYPE_ETQF))
		return RTE_PTYPE_UNKNOWN;

	pkt_info = (pkt_info >> IGB_PACKET_TYPE_SHIFT) & IGB_PACKET_TYPE_MASK;

	return ptype_table[pkt_info];
}

static inline uint64_t
rx_desc_hlen_type_rss_to_pkt_flags(struct igb_rx_queue *rxq, uint32_t hl_tp_rs)
{
	uint64_t pkt_flags = ((hl_tp_rs & 0x0F) == 0) ?  0 : PKT_RX_RSS_HASH;

#if defined(RTE_LIBRTE_IEEE1588)
	static uint32_t ip_pkt_etqf_map[8] = {
		0, 0, 0, PKT_RX_IEEE1588_PTP,
		0, 0, 0, 0,
	};

	struct rte_eth_dev dev = rte_eth_devices[rxq->port_id];
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev.data->dev_private);

	/* EtherType is in bits 8:10 in Packet Type, and not in the default 0:2 */
	if (hw->mac.type == e1000_i210)
		pkt_flags |= ip_pkt_etqf_map[(hl_tp_rs >> 12) & 0x07];
	else
		pkt_flags |= ip_pkt_etqf_map[(hl_tp_rs >> 4) & 0x07];
#else
	RTE_SET_USED(rxq);
#endif

	return pkt_flags;
}

static inline uint64_t
rx_desc_status_to_pkt_flags(uint32_t rx_status)
{
	uint64_t pkt_flags;

	/* Check if VLAN present */
	pkt_flags = ((rx_status & E1000_RXD_STAT_VP) ?
		PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED : 0);

#if defined(RTE_LIBRTE_IEEE1588)
	if (rx_status & E1000_RXD_STAT_TMST)
		pkt_flags = pkt_flags | PKT_RX_IEEE1588_TMST;
#endif
	return pkt_flags;
}

static inline uint64_t
rx_desc_error_to_pkt_flags(uint32_t rx_status)
{
	/*
	 * Bit 30: IPE, IPv4 checksum error
	 * Bit 29: L4I, L4I integrity error
	 */

	static uint64_t error_to_pkt_flags_map[4] = {
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD,
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_BAD,
		PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_GOOD,
		PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD
	};
	return error_to_pkt_flags_map[(rx_status >>
		E1000_RXD_ERR_CKSUM_BIT) & E1000_RXD_ERR_CKSUM_MSK];
}

uint16_t
eth_igb_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	       uint16_t nb_pkts)
{
	struct igb_rx_queue *rxq;
	volatile union e1000_adv_rx_desc *rx_ring;
	volatile union e1000_adv_rx_desc *rxdp;
	struct igb_rx_entry *sw_ring;
	struct igb_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	union e1000_adv_rx_desc rxd;
	uint64_t dma_addr;
	uint32_t staterr;
	uint32_t hlen_type_rss;
	uint16_t pkt_len;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint64_t pkt_flags;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
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
		staterr = rxdp->wb.upper.status_error;
		if (! (staterr & rte_cpu_to_le_32(E1000_RXD_STAT_DD)))
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
			   "staterr=0x%x pkt_len=%u",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) staterr,
			   (unsigned) rte_le_to_cpu_16(rxd.wb.upper.length));

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
		rte_igb_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_igb_prefetch(&rx_ring[rx_id]);
			rte_igb_prefetch(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;

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
		pkt_len = (uint16_t) (rte_le_to_cpu_16(rxd.wb.upper.length) -
				      rxq->crc_len);
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_packet_prefetch((char *)rxm->buf_addr + rxm->data_off);
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = pkt_len;
		rxm->data_len = pkt_len;
		rxm->port = rxq->port_id;

		rxm->hash.rss = rxd.wb.lower.hi_dword.rss;
		hlen_type_rss = rte_le_to_cpu_32(rxd.wb.lower.lo_dword.data);

		/*
		 * The vlan_tci field is only valid when PKT_RX_VLAN is
		 * set in the pkt_flags field and must be in CPU byte order.
		 */
		if ((staterr & rte_cpu_to_le_32(E1000_RXDEXT_STATERR_LB)) &&
				(rxq->flags & IGB_RXQ_FLAG_LB_BSWAP_VLAN)) {
			rxm->vlan_tci = rte_be_to_cpu_16(rxd.wb.upper.vlan);
		} else {
			rxm->vlan_tci = rte_le_to_cpu_16(rxd.wb.upper.vlan);
		}
		pkt_flags = rx_desc_hlen_type_rss_to_pkt_flags(rxq, hlen_type_rss);
		pkt_flags = pkt_flags | rx_desc_status_to_pkt_flags(staterr);
		pkt_flags = pkt_flags | rx_desc_error_to_pkt_flags(staterr);
		rxm->ol_flags = pkt_flags;
		rxm->packet_type = igb_rxd_pkt_info_to_pkt_type(rxd.wb.lower.
						lo_dword.hs_rss.pkt_info);

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
eth_igb_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct igb_rx_queue *rxq;
	volatile union e1000_adv_rx_desc *rx_ring;
	volatile union e1000_adv_rx_desc *rxdp;
	struct igb_rx_entry *sw_ring;
	struct igb_rx_entry *rxe;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	union e1000_adv_rx_desc rxd;
	uint64_t dma; /* Physical address of mbuf data buffer */
	uint32_t staterr;
	uint32_t hlen_type_rss;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint16_t data_len;
	uint64_t pkt_flags;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
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
		staterr = rxdp->wb.upper.status_error;
		if (! (staterr & rte_cpu_to_le_32(E1000_RXD_STAT_DD)))
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
			   "staterr=0x%x data_len=%u",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) staterr,
			   (unsigned) rte_le_to_cpu_16(rxd.wb.upper.length));

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
		rte_igb_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_igb_prefetch(&rx_ring[rx_id]);
			rte_igb_prefetch(&sw_ring[rx_id]);
		}

		/*
		 * Update RX descriptor with the physical address of the new
		 * data buffer of the new allocated mbuf.
		 */
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->read.pkt_addr = dma;
		rxdp->read.hdr_addr = 0;

		/*
		 * Set data length & data buffer address of mbuf.
		 */
		data_len = rte_le_to_cpu_16(rxd.wb.upper.length);
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
		if (! (staterr & E1000_RXD_STAT_EOP)) {
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
		 *      - RSS flag & hash,
		 *      - IP checksum flag,
		 *      - VLAN TCI, if any,
		 *      - error flags.
		 */
		first_seg->port = rxq->port_id;
		first_seg->hash.rss = rxd.wb.lower.hi_dword.rss;

		/*
		 * The vlan_tci field is only valid when PKT_RX_VLAN is
		 * set in the pkt_flags field and must be in CPU byte order.
		 */
		if ((staterr & rte_cpu_to_le_32(E1000_RXDEXT_STATERR_LB)) &&
				(rxq->flags & IGB_RXQ_FLAG_LB_BSWAP_VLAN)) {
			first_seg->vlan_tci =
				rte_be_to_cpu_16(rxd.wb.upper.vlan);
		} else {
			first_seg->vlan_tci =
				rte_le_to_cpu_16(rxd.wb.upper.vlan);
		}
		hlen_type_rss = rte_le_to_cpu_32(rxd.wb.lower.lo_dword.data);
		pkt_flags = rx_desc_hlen_type_rss_to_pkt_flags(rxq, hlen_type_rss);
		pkt_flags = pkt_flags | rx_desc_status_to_pkt_flags(staterr);
		pkt_flags = pkt_flags | rx_desc_error_to_pkt_flags(staterr);
		first_seg->ol_flags = pkt_flags;
		first_seg->packet_type = igb_rxd_pkt_info_to_pkt_type(rxd.wb.
					lower.lo_dword.hs_rss.pkt_info);

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

/*
 * Maximum number of Ring Descriptors.
 *
 * Since RDLEN/TDLEN should be multiple of 128bytes, the number of ring
 * desscriptors should meet the following condition:
 *      (num_ring_desc * sizeof(struct e1000_rx/tx_desc)) % 128 == 0
 */

static void
igb_tx_queue_release_mbufs(struct igb_tx_queue *txq)
{
	unsigned i;

	if (txq->sw_ring != NULL) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
igb_tx_queue_release(struct igb_tx_queue *txq)
{
	if (txq != NULL) {
		igb_tx_queue_release_mbufs(txq);
		rte_free(txq->sw_ring);
		rte_free(txq);
	}
}

void
eth_igb_tx_queue_release(void *txq)
{
	igb_tx_queue_release(txq);
}

static int
igb_tx_done_cleanup(struct igb_tx_queue *txq, uint32_t free_cnt)
{
	struct igb_tx_entry *sw_ring;
	volatile union e1000_adv_tx_desc *txr;
	uint16_t tx_first; /* First segment analyzed. */
	uint16_t tx_id;    /* Current segment being processed. */
	uint16_t tx_last;  /* Last segment in the current packet. */
	uint16_t tx_next;  /* First segment of the next packet. */
	int count;

	if (txq != NULL) {
		count = 0;
		sw_ring = txq->sw_ring;
		txr = txq->tx_ring;

		/*
		 * tx_tail is the last sent packet on the sw_ring. Goto the end
		 * of that packet (the last segment in the packet chain) and
		 * then the next segment will be the start of the oldest segment
		 * in the sw_ring. This is the first packet that will be
		 * attempted to be freed.
		 */

		/* Get last segment in most recently added packet. */
		tx_first = sw_ring[txq->tx_tail].last_id;

		/* Get the next segment, which is the oldest segment in ring. */
		tx_first = sw_ring[tx_first].next_id;

		/* Set the current index to the first. */
		tx_id = tx_first;

		/*
		 * Loop through each packet. For each packet, verify that an
		 * mbuf exists and that the last segment is free. If so, free
		 * it and move on.
		 */
		while (1) {
			tx_last = sw_ring[tx_id].last_id;

			if (sw_ring[tx_last].mbuf) {
				if (txr[tx_last].wb.status &
						E1000_TXD_STAT_DD) {
					/*
					 * Increment the number of packets
					 * freed.
					 */
					count++;

					/* Get the start of the next packet. */
					tx_next = sw_ring[tx_last].next_id;

					/*
					 * Loop through all segments in a
					 * packet.
					 */
					do {
						rte_pktmbuf_free_seg(sw_ring[tx_id].mbuf);
						sw_ring[tx_id].mbuf = NULL;
						sw_ring[tx_id].last_id = tx_id;

						/* Move to next segemnt. */
						tx_id = sw_ring[tx_id].next_id;

					} while (tx_id != tx_next);

					if (unlikely(count == (int)free_cnt))
						break;
				} else
					/*
					 * mbuf still in use, nothing left to
					 * free.
					 */
					break;
			} else {
				/*
				 * There are multiple reasons to be here:
				 * 1) All the packets on the ring have been
				 *    freed - tx_id is equal to tx_first
				 *    and some packets have been freed.
				 *    - Done, exit
				 * 2) Interfaces has not sent a rings worth of
				 *    packets yet, so the segment after tail is
				 *    still empty. Or a previous call to this
				 *    function freed some of the segments but
				 *    not all so there is a hole in the list.
				 *    Hopefully this is a rare case.
				 *    - Walk the list and find the next mbuf. If
				 *      there isn't one, then done.
				 */
				if (likely((tx_id == tx_first) && (count != 0)))
					break;

				/*
				 * Walk the list and find the next mbuf, if any.
				 */
				do {
					/* Move to next segemnt. */
					tx_id = sw_ring[tx_id].next_id;

					if (sw_ring[tx_id].mbuf)
						break;

				} while (tx_id != tx_first);

				/*
				 * Determine why previous loop bailed. If there
				 * is not an mbuf, done.
				 */
				if (sw_ring[tx_id].mbuf == NULL)
					break;
			}
		}
	} else
		count = -ENODEV;

	return count;
}

int
eth_igb_tx_done_cleanup(void *txq, uint32_t free_cnt)
{
	return igb_tx_done_cleanup(txq, free_cnt);
}

static void
igb_reset_tx_queue_stat(struct igb_tx_queue *txq)
{
	txq->tx_head = 0;
	txq->tx_tail = 0;
	txq->ctx_curr = 0;
	memset((void*)&txq->ctx_cache, 0,
		IGB_CTX_NUM * sizeof(struct igb_advctx_info));
}

static void
igb_reset_tx_queue(struct igb_tx_queue *txq, struct rte_eth_dev *dev)
{
	static const union e1000_adv_tx_desc zeroed_desc = {{0}};
	struct igb_tx_entry *txe = txq->sw_ring;
	uint16_t i, prev;
	struct e1000_hw *hw;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->tx_ring[i] = zeroed_desc;
	}

	/* Initialize ring entries */
	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile union e1000_adv_tx_desc *txd = &(txq->tx_ring[i]);

		txd->wb.status = E1000_TXD_STAT_DD;
		txe[i].mbuf = NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->txd_type = E1000_ADVTXD_DTYP_DATA;
	/* 82575 specific, each tx queue will use 2 hw contexts */
	if (hw->mac.type == e1000_82575)
		txq->ctx_start = txq->queue_id * IGB_CTX_NUM;

	igb_reset_tx_queue_stat(txq);
}

int
eth_igb_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct igb_tx_queue *txq;
	struct e1000_hw     *hw;
	uint32_t size;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of E1000_ALIGN.
	 */
	if (nb_desc % IGB_TXD_ALIGN != 0 ||
			(nb_desc > E1000_MAX_RING_DESC) ||
			(nb_desc < E1000_MIN_RING_DESC)) {
		return -EINVAL;
	}

	/*
	 * The tx_free_thresh and tx_rs_thresh values are not used in the 1G
	 * driver.
	 */
	if (tx_conf->tx_free_thresh != 0)
		PMD_INIT_LOG(INFO, "The tx_free_thresh parameter is not "
			     "used for the 1G driver.");
	if (tx_conf->tx_rs_thresh != 0)
		PMD_INIT_LOG(INFO, "The tx_rs_thresh parameter is not "
			     "used for the 1G driver.");
	if (tx_conf->tx_thresh.wthresh == 0 && hw->mac.type != e1000_82576)
		PMD_INIT_LOG(INFO, "To improve 1G driver performance, "
			     "consider setting the TX WTHRESH value to 4, 8, "
			     "or 16.");

	/* Free memory prior to re-allocation if needed */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		igb_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc("ethdev TX queue", sizeof(struct igb_tx_queue),
							RTE_CACHE_LINE_SIZE);
	if (txq == NULL)
		return -ENOMEM;

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	size = sizeof(union e1000_adv_tx_desc) * E1000_MAX_RING_DESC;
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, size,
				      E1000_ALIGN, socket_id);
	if (tz == NULL) {
		igb_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	if (txq->wthresh > 0 && hw->mac.type == e1000_82576)
		txq->wthresh = 1;
	txq->queue_id = queue_idx;
	txq->reg_idx = (uint16_t)((RTE_ETH_DEV_SRIOV(dev).active == 0) ?
		queue_idx : RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx + queue_idx);
	txq->port_id = dev->data->port_id;

	txq->tdt_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_TDT(txq->reg_idx));
	txq->tx_ring_phys_addr = tz->iova;

	txq->tx_ring = (union e1000_adv_tx_desc *) tz->addr;
	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc("txq->sw_ring",
				   sizeof(struct igb_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE);
	if (txq->sw_ring == NULL) {
		igb_tx_queue_release(txq);
		return -ENOMEM;
	}
	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64,
		     txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	igb_reset_tx_queue(txq, dev);
	dev->tx_pkt_burst = eth_igb_xmit_pkts;
	dev->tx_pkt_prepare = &eth_igb_prep_pkts;
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

static void
igb_rx_queue_release_mbufs(struct igb_rx_queue *rxq)
{
	unsigned i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
igb_rx_queue_release(struct igb_rx_queue *rxq)
{
	if (rxq != NULL) {
		igb_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

void
eth_igb_rx_queue_release(void *rxq)
{
	igb_rx_queue_release(rxq);
}

static void
igb_reset_rx_queue(struct igb_rx_queue *rxq)
{
	static const union e1000_adv_rx_desc zeroed_desc = {{0}};
	unsigned i;

	/* Zero out HW ring memory */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		rxq->rx_ring[i] = zeroed_desc;
	}

	rxq->rx_tail = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

uint64_t
igb_get_rx_port_offloads_capa(struct rte_eth_dev *dev)
{
	uint64_t rx_offload_capa;

	RTE_SET_USED(dev);
	rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP  |
			  DEV_RX_OFFLOAD_VLAN_FILTER |
			  DEV_RX_OFFLOAD_IPV4_CKSUM  |
			  DEV_RX_OFFLOAD_UDP_CKSUM   |
			  DEV_RX_OFFLOAD_TCP_CKSUM   |
			  DEV_RX_OFFLOAD_JUMBO_FRAME |
			  DEV_RX_OFFLOAD_CRC_STRIP   |
			  DEV_RX_OFFLOAD_SCATTER;

	return rx_offload_capa;
}

uint64_t
igb_get_rx_queue_offloads_capa(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t rx_queue_offload_capa;

	switch (hw->mac.type) {
	case e1000_vfadapt_i350:
		/*
		 * As only one Rx queue can be used, let per queue offloading
		 * capability be same to per port queue offloading capability
		 * for better convenience.
		 */
		rx_queue_offload_capa = igb_get_rx_port_offloads_capa(dev);
		break;
	default:
		rx_queue_offload_capa = 0;
	}
	return rx_queue_offload_capa;
}

static int
igb_check_rx_queue_offloads(struct rte_eth_dev *dev, uint64_t requested)
{
	uint64_t port_offloads = dev->data->dev_conf.rxmode.offloads;
	uint64_t queue_supported = igb_get_rx_queue_offloads_capa(dev);
	uint64_t port_supported = igb_get_rx_port_offloads_capa(dev);

	if ((requested & (queue_supported | port_supported)) != requested)
		return 0;

	if ((port_offloads ^ requested) & port_supported)
		return 0;

	return 1;
}

int
eth_igb_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct igb_rx_queue *rxq;
	struct e1000_hw     *hw;
	unsigned int size;

	if (!igb_check_rx_queue_offloads(dev, rx_conf->offloads)) {
		PMD_INIT_LOG(ERR, "%p: Rx queue offloads 0x%" PRIx64
			" don't match port offloads 0x%" PRIx64
			" or supported port offloads 0x%" PRIx64
			" or supported queue offloads 0x%" PRIx64,
			(void *)dev,
			rx_conf->offloads,
			dev->data->dev_conf.rxmode.offloads,
			igb_get_rx_port_offloads_capa(dev),
			igb_get_rx_queue_offloads_capa(dev));
		return -ENOTSUP;
	}

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of E1000_ALIGN.
	 */
	if (nb_desc % IGB_RXD_ALIGN != 0 ||
			(nb_desc > E1000_MAX_RING_DESC) ||
			(nb_desc < E1000_MIN_RING_DESC)) {
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		igb_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the RX queue data structure. */
	rxq = rte_zmalloc("ethdev RX queue", sizeof(struct igb_rx_queue),
			  RTE_CACHE_LINE_SIZE);
	if (rxq == NULL)
		return -ENOMEM;
	rxq->offloads = rx_conf->offloads;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->pthresh = rx_conf->rx_thresh.pthresh;
	rxq->hthresh = rx_conf->rx_thresh.hthresh;
	rxq->wthresh = rx_conf->rx_thresh.wthresh;
	if (rxq->wthresh > 0 &&
	    (hw->mac.type == e1000_82576 || hw->mac.type == e1000_vfadapt_i350))
		rxq->wthresh = 1;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = (uint16_t)((RTE_ETH_DEV_SRIOV(dev).active == 0) ?
		queue_idx : RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx + queue_idx);
	rxq->port_id = dev->data->port_id;
	rxq->crc_len = (uint8_t)((dev->data->dev_conf.rxmode.offloads &
			DEV_RX_OFFLOAD_CRC_STRIP) ? 0 : ETHER_CRC_LEN);

	/*
	 *  Allocate RX ring hardware descriptors. A memzone large enough to
	 *  handle the maximum ring size is allocated in order to allow for
	 *  resizing in later calls to the queue setup function.
	 */
	size = sizeof(union e1000_adv_rx_desc) * E1000_MAX_RING_DESC;
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx, size,
				      E1000_ALIGN, socket_id);
	if (rz == NULL) {
		igb_rx_queue_release(rxq);
		return -ENOMEM;
	}
	rxq->rdt_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_RDT(rxq->reg_idx));
	rxq->rdh_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_RDH(rxq->reg_idx));
	rxq->rx_ring_phys_addr = rz->iova;
	rxq->rx_ring = (union e1000_adv_rx_desc *) rz->addr;

	/* Allocate software ring. */
	rxq->sw_ring = rte_zmalloc("rxq->sw_ring",
				   sizeof(struct igb_rx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE);
	if (rxq->sw_ring == NULL) {
		igb_rx_queue_release(rxq);
		return -ENOMEM;
	}
	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64,
		     rxq->sw_ring, rxq->rx_ring, rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;
	igb_reset_rx_queue(rxq);

	return 0;
}

uint32_t
eth_igb_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
#define IGB_RXQ_SCAN_INTERVAL 4
	volatile union e1000_adv_rx_desc *rxdp;
	struct igb_rx_queue *rxq;
	uint32_t desc = 0;

	rxq = dev->data->rx_queues[rx_queue_id];
	rxdp = &(rxq->rx_ring[rxq->rx_tail]);

	while ((desc < rxq->nb_rx_desc) &&
		(rxdp->wb.upper.status_error & E1000_RXD_STAT_DD)) {
		desc += IGB_RXQ_SCAN_INTERVAL;
		rxdp += IGB_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
				desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
eth_igb_rx_descriptor_done(void *rx_queue, uint16_t offset)
{
	volatile union e1000_adv_rx_desc *rxdp;
	struct igb_rx_queue *rxq = rx_queue;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return 0;
	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	rxdp = &rxq->rx_ring[desc];
	return !!(rxdp->wb.upper.status_error & E1000_RXD_STAT_DD);
}

int
eth_igb_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct igb_rx_queue *rxq = rx_queue;
	volatile uint32_t *status;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

	if (offset >= rxq->nb_rx_desc - rxq->nb_rx_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	status = &rxq->rx_ring[desc].wb.upper.status_error;
	if (*status & rte_cpu_to_le_32(E1000_RXD_STAT_DD))
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
eth_igb_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct igb_tx_queue *txq = tx_queue;
	volatile uint32_t *status;
	uint32_t desc;

	if (unlikely(offset >= txq->nb_tx_desc))
		return -EINVAL;

	desc = txq->tx_tail + offset;
	if (desc >= txq->nb_tx_desc)
		desc -= txq->nb_tx_desc;

	status = &txq->tx_ring[desc].wb.status;
	if (*status & rte_cpu_to_le_32(E1000_TXD_STAT_DD))
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

void
igb_dev_clear_queues(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct igb_tx_queue *txq;
	struct igb_rx_queue *rxq;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq != NULL) {
			igb_tx_queue_release_mbufs(txq);
			igb_reset_tx_queue(txq, dev);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq != NULL) {
			igb_rx_queue_release_mbufs(rxq);
			igb_reset_rx_queue(rxq);
		}
	}
}

void
igb_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		eth_igb_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		eth_igb_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

/**
 * Receive Side Scaling (RSS).
 * See section 7.1.1.7 in the following document:
 *     "Intel 82576 GbE Controller Datasheet" - Revision 2.45 October 2009
 *
 * Principles:
 * The source and destination IP addresses of the IP header and the source and
 * destination ports of TCP/UDP headers, if any, of received packets are hashed
 * against a configurable random key to compute a 32-bit RSS hash result.
 * The seven (7) LSBs of the 32-bit hash result are used as an index into a
 * 128-entry redirection table (RETA).  Each entry of the RETA provides a 3-bit
 * RSS output index which is used as the RX queue index where to store the
 * received packets.
 * The following output is supplied in the RX write-back descriptor:
 *     - 32-bit result of the Microsoft RSS hash function,
 *     - 4-bit RSS type field.
 */

/*
 * RSS random key supplied in section 7.1.1.7.3 of the Intel 82576 datasheet.
 * Used as the default key.
 */
static uint8_t rss_intel_key[40] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
};

static void
igb_rss_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw;
	uint32_t mrqc;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	mrqc = E1000_READ_REG(hw, E1000_MRQC);
	mrqc &= ~E1000_MRQC_ENABLE_MASK;
	E1000_WRITE_REG(hw, E1000_MRQC, mrqc);
}

static void
igb_hw_rss_hash_set(struct e1000_hw *hw, struct rte_eth_rss_conf *rss_conf)
{
	uint8_t  *hash_key;
	uint32_t rss_key;
	uint32_t mrqc;
	uint64_t rss_hf;
	uint16_t i;

	hash_key = rss_conf->rss_key;
	if (hash_key != NULL) {
		/* Fill in RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key  = hash_key[(i * 4)];
			rss_key |= hash_key[(i * 4) + 1] << 8;
			rss_key |= hash_key[(i * 4) + 2] << 16;
			rss_key |= hash_key[(i * 4) + 3] << 24;
			E1000_WRITE_REG_ARRAY(hw, E1000_RSSRK(0), i, rss_key);
		}
	}

	/* Set configured hashing protocols in MRQC register */
	rss_hf = rss_conf->rss_hf;
	mrqc = E1000_MRQC_ENABLE_RSS_4Q; /* RSS enabled. */
	if (rss_hf & ETH_RSS_IPV4)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV4;
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV4_TCP;
	if (rss_hf & ETH_RSS_IPV6)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6;
	if (rss_hf & ETH_RSS_IPV6_EX)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_EX;
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_TCP;
	if (rss_hf & ETH_RSS_IPV6_TCP_EX)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_TCP_EX;
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV4_UDP;
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_UDP;
	if (rss_hf & ETH_RSS_IPV6_UDP_EX)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_UDP_EX;
	E1000_WRITE_REG(hw, E1000_MRQC, mrqc);
}

int
eth_igb_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct e1000_hw *hw;
	uint32_t mrqc;
	uint64_t rss_hf;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Before changing anything, first check that the update RSS operation
	 * does not attempt to disable RSS, if RSS was enabled at
	 * initialization time, or does not attempt to enable RSS, if RSS was
	 * disabled at initialization time.
	 */
	rss_hf = rss_conf->rss_hf & IGB_RSS_OFFLOAD_ALL;
	mrqc = E1000_READ_REG(hw, E1000_MRQC);
	if (!(mrqc & E1000_MRQC_ENABLE_MASK)) { /* RSS disabled */
		if (rss_hf != 0) /* Enable RSS */
			return -(EINVAL);
		return 0; /* Nothing to do */
	}
	/* RSS enabled */
	if (rss_hf == 0) /* Disable RSS */
		return -(EINVAL);
	igb_hw_rss_hash_set(hw, rss_conf);
	return 0;
}

int eth_igb_rss_hash_conf_get(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct e1000_hw *hw;
	uint8_t *hash_key;
	uint32_t rss_key;
	uint32_t mrqc;
	uint64_t rss_hf;
	uint16_t i;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	hash_key = rss_conf->rss_key;
	if (hash_key != NULL) {
		/* Return RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key = E1000_READ_REG_ARRAY(hw, E1000_RSSRK(0), i);
			hash_key[(i * 4)] = rss_key & 0x000000FF;
			hash_key[(i * 4) + 1] = (rss_key >> 8) & 0x000000FF;
			hash_key[(i * 4) + 2] = (rss_key >> 16) & 0x000000FF;
			hash_key[(i * 4) + 3] = (rss_key >> 24) & 0x000000FF;
		}
	}

	/* Get RSS functions configured in MRQC register */
	mrqc = E1000_READ_REG(hw, E1000_MRQC);
	if ((mrqc & E1000_MRQC_ENABLE_RSS_4Q) == 0) { /* RSS is disabled */
		rss_conf->rss_hf = 0;
		return 0;
	}
	rss_hf = 0;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV4)
		rss_hf |= ETH_RSS_IPV4;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV4_TCP)
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV6)
		rss_hf |= ETH_RSS_IPV6;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV6_EX)
		rss_hf |= ETH_RSS_IPV6_EX;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV6_TCP)
		rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV6_TCP_EX)
		rss_hf |= ETH_RSS_IPV6_TCP_EX;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV4_UDP)
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV6_UDP)
		rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;
	if (mrqc & E1000_MRQC_RSS_FIELD_IPV6_UDP_EX)
		rss_hf |= ETH_RSS_IPV6_UDP_EX;
	rss_conf->rss_hf = rss_hf;
	return 0;
}

static void
igb_rss_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_rss_conf rss_conf;
	struct e1000_hw *hw;
	uint32_t shift;
	uint16_t i;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Fill in redirection table. */
	shift = (hw->mac.type == e1000_82575) ? 6 : 0;
	for (i = 0; i < 128; i++) {
		union e1000_reta {
			uint32_t dword;
			uint8_t  bytes[4];
		} reta;
		uint8_t q_idx;

		q_idx = (uint8_t) ((dev->data->nb_rx_queues > 1) ?
				   i % dev->data->nb_rx_queues : 0);
		reta.bytes[i & 3] = (uint8_t) (q_idx << shift);
		if ((i & 3) == 3)
			E1000_WRITE_REG(hw, E1000_RETA(i >> 2), reta.dword);
	}

	/*
	 * Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	rss_conf = dev->data->dev_conf.rx_adv_conf.rss_conf;
	if ((rss_conf.rss_hf & IGB_RSS_OFFLOAD_ALL) == 0) {
		igb_rss_disable(dev);
		return;
	}
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rss_intel_key; /* Default hash key */
	igb_hw_rss_hash_set(hw, &rss_conf);
}

/*
 * Check if the mac type support VMDq or not.
 * Return 1 if it supports, otherwise, return 0.
 */
static int
igb_is_vmdq_supported(const struct rte_eth_dev *dev)
{
	const struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	switch (hw->mac.type) {
	case e1000_82576:
	case e1000_82580:
	case e1000_i350:
		return 1;
	case e1000_82540:
	case e1000_82541:
	case e1000_82542:
	case e1000_82543:
	case e1000_82544:
	case e1000_82545:
	case e1000_82546:
	case e1000_82547:
	case e1000_82571:
	case e1000_82572:
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
	case e1000_i210:
	case e1000_i211:
	default:
		PMD_INIT_LOG(ERR, "Cannot support VMDq feature");
		return 0;
	}
}

static int
igb_vmdq_rx_hw_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_vmdq_rx_conf *cfg;
	struct e1000_hw *hw;
	uint32_t mrqc, vt_ctl, vmolr, rctl;
	int i;

	PMD_INIT_FUNC_TRACE();

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	cfg = &dev->data->dev_conf.rx_adv_conf.vmdq_rx_conf;

	/* Check if mac type can support VMDq, return value of 0 means NOT support */
	if (igb_is_vmdq_supported(dev) == 0)
		return -1;

	igb_rss_disable(dev);

	/* RCTL: eanble VLAN filter */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl |= E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	/* MRQC: enable vmdq */
	mrqc = E1000_READ_REG(hw, E1000_MRQC);
	mrqc |= E1000_MRQC_ENABLE_VMDQ;
	E1000_WRITE_REG(hw, E1000_MRQC, mrqc);

	/* VTCTL:  pool selection according to VLAN tag */
	vt_ctl = E1000_READ_REG(hw, E1000_VT_CTL);
	if (cfg->enable_default_pool)
		vt_ctl |= (cfg->default_pool << E1000_VT_CTL_DEFAULT_POOL_SHIFT);
	vt_ctl |= E1000_VT_CTL_IGNORE_MAC;
	E1000_WRITE_REG(hw, E1000_VT_CTL, vt_ctl);

	for (i = 0; i < E1000_VMOLR_SIZE; i++) {
		vmolr = E1000_READ_REG(hw, E1000_VMOLR(i));
		vmolr &= ~(E1000_VMOLR_AUPE | E1000_VMOLR_ROMPE |
			E1000_VMOLR_ROPE | E1000_VMOLR_BAM |
			E1000_VMOLR_MPME);

		if (cfg->rx_mode & ETH_VMDQ_ACCEPT_UNTAG)
			vmolr |= E1000_VMOLR_AUPE;
		if (cfg->rx_mode & ETH_VMDQ_ACCEPT_HASH_MC)
			vmolr |= E1000_VMOLR_ROMPE;
		if (cfg->rx_mode & ETH_VMDQ_ACCEPT_HASH_UC)
			vmolr |= E1000_VMOLR_ROPE;
		if (cfg->rx_mode & ETH_VMDQ_ACCEPT_BROADCAST)
			vmolr |= E1000_VMOLR_BAM;
		if (cfg->rx_mode & ETH_VMDQ_ACCEPT_MULTICAST)
			vmolr |= E1000_VMOLR_MPME;

		E1000_WRITE_REG(hw, E1000_VMOLR(i), vmolr);
	}

	/*
	 * VMOLR: set STRVLAN as 1 if IGMAC in VTCTL is set as 1
	 * Both 82576 and 82580 support it
	 */
	if (hw->mac.type != e1000_i350) {
		for (i = 0; i < E1000_VMOLR_SIZE; i++) {
			vmolr = E1000_READ_REG(hw, E1000_VMOLR(i));
			vmolr |= E1000_VMOLR_STRVLAN;
			E1000_WRITE_REG(hw, E1000_VMOLR(i), vmolr);
		}
	}

	/* VFTA - enable all vlan filters */
	for (i = 0; i < IGB_VFTA_SIZE; i++)
		E1000_WRITE_REG(hw, (E1000_VFTA+(i*4)), UINT32_MAX);

	/* VFRE: 8 pools enabling for rx, both 82576 and i350 support it */
	if (hw->mac.type != e1000_82580)
		E1000_WRITE_REG(hw, E1000_VFRE, E1000_MBVFICR_VFREQ_MASK);

	/*
	 * RAH/RAL - allow pools to read specific mac addresses
	 * In this case, all pools should be able to read from mac addr 0
	 */
	E1000_WRITE_REG(hw, E1000_RAH(0), (E1000_RAH_AV | UINT16_MAX));
	E1000_WRITE_REG(hw, E1000_RAL(0), UINT32_MAX);

	/* VLVF: set up filters for vlan tags as configured */
	for (i = 0; i < cfg->nb_pool_maps; i++) {
		/* set vlan id in VF register and set the valid bit */
		E1000_WRITE_REG(hw, E1000_VLVF(i), (E1000_VLVF_VLANID_ENABLE | \
                        (cfg->pool_map[i].vlan_id & ETH_VLAN_ID_MAX) | \
			((cfg->pool_map[i].pools << E1000_VLVF_POOLSEL_SHIFT ) & \
			E1000_VLVF_POOLSEL_MASK)));
	}

	E1000_WRITE_FLUSH(hw);

	return 0;
}


/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/

static int
igb_alloc_rx_queue_mbufs(struct igb_rx_queue *rxq)
{
	struct igb_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned i;

	/* Initialize software ring entries. */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union e1000_adv_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "RX mbuf alloc failed "
				     "queue_id=%hu", rxq->queue_id);
			return -ENOMEM;
		}
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = &rxq->rx_ring[i];
		rxd->read.hdr_addr = 0;
		rxd->read.pkt_addr = dma_addr;
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

#define E1000_MRQC_DEF_Q_SHIFT               (3)
static int
igb_dev_mq_rx_configure(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t mrqc;

	if (RTE_ETH_DEV_SRIOV(dev).active == ETH_8_POOLS) {
		/*
		 * SRIOV active scheme
		 * FIXME if support RSS together with VMDq & SRIOV
		 */
		mrqc = E1000_MRQC_ENABLE_VMDQ;
		/* 011b Def_Q ignore, according to VT_CTL.DEF_PL */
		mrqc |= 0x3 << E1000_MRQC_DEF_Q_SHIFT;
		E1000_WRITE_REG(hw, E1000_MRQC, mrqc);
	} else if(RTE_ETH_DEV_SRIOV(dev).active == 0) {
		/*
		 * SRIOV inactive scheme
		 */
		switch (dev->data->dev_conf.rxmode.mq_mode) {
			case ETH_MQ_RX_RSS:
				igb_rss_configure(dev);
				break;
			case ETH_MQ_RX_VMDQ_ONLY:
				/*Configure general VMDQ only RX parameters*/
				igb_vmdq_rx_hw_configure(dev);
				break;
			case ETH_MQ_RX_NONE:
				/* if mq_mode is none, disable rss mode.*/
			default:
				igb_rss_disable(dev);
				break;
		}
	}

	return 0;
}

int
eth_igb_rx_init(struct rte_eth_dev *dev)
{
	struct rte_eth_rxmode *rxmode;
	struct e1000_hw     *hw;
	struct igb_rx_queue *rxq;
	uint32_t rctl;
	uint32_t rxcsum;
	uint32_t srrctl;
	uint16_t buf_size;
	uint16_t rctl_bsize;
	uint16_t i;
	int ret;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	srrctl = 0;

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring.
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	rxmode = &dev->data->dev_conf.rxmode;

	/*
	 * Configure support of jumbo frames, if any.
	 */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		rctl |= E1000_RCTL_LPE;

		/*
		 * Set maximum packet length by default, and might be updated
		 * together with enabling/disabling dual VLAN.
		 */
		E1000_WRITE_REG(hw, E1000_RLPML,
			dev->data->dev_conf.rxmode.max_rx_pkt_len +
						VLAN_TAG_SIZE);
	} else
		rctl &= ~E1000_RCTL_LPE;

	/* Configure and enable each RX queue. */
	rctl_bsize = 0;
	dev->rx_pkt_burst = eth_igb_recv_pkts;
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		uint64_t bus_addr;
		uint32_t rxdctl;

		rxq = dev->data->rx_queues[i];

		rxq->flags = 0;
		/*
		 * i350 and i354 vlan packets have vlan tags byte swapped.
		 */
		if (hw->mac.type == e1000_i350 || hw->mac.type == e1000_i354) {
			rxq->flags |= IGB_RXQ_FLAG_LB_BSWAP_VLAN;
			PMD_INIT_LOG(DEBUG, "IGB rx vlan bswap required");
		} else {
			PMD_INIT_LOG(DEBUG, "IGB rx vlan bswap not required");
		}

		/* Allocate buffers for descriptor rings and set up queue */
		ret = igb_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 *  call to configure
		 */
		rxq->crc_len = (uint8_t)(dev->data->dev_conf.rxmode.offloads &
				DEV_RX_OFFLOAD_CRC_STRIP ? 0 : ETHER_CRC_LEN);

		bus_addr = rxq->rx_ring_phys_addr;
		E1000_WRITE_REG(hw, E1000_RDLEN(rxq->reg_idx),
				rxq->nb_rx_desc *
				sizeof(union e1000_adv_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(rxq->reg_idx), (uint32_t)bus_addr);

		srrctl = E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

		/*
		 * Configure RX buffer size.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		if (buf_size >= 1024) {
			/*
			 * Configure the BSIZEPACKET field of the SRRCTL
			 * register of the queue.
			 * Value is in 1 KB resolution, from 1 KB to 127 KB.
			 * If this field is equal to 0b, then RCTL.BSIZE
			 * determines the RX packet buffer size.
			 */
			srrctl |= ((buf_size >> E1000_SRRCTL_BSIZEPKT_SHIFT) &
				   E1000_SRRCTL_BSIZEPKT_MASK);
			buf_size = (uint16_t) ((srrctl &
						E1000_SRRCTL_BSIZEPKT_MASK) <<
					       E1000_SRRCTL_BSIZEPKT_SHIFT);

			/* It adds dual VLAN length for supporting dual VLAN */
			if ((dev->data->dev_conf.rxmode.max_rx_pkt_len +
						2 * VLAN_TAG_SIZE) > buf_size){
				if (!dev->data->scattered_rx)
					PMD_INIT_LOG(DEBUG,
						     "forcing scatter mode");
				dev->rx_pkt_burst = eth_igb_recv_scattered_pkts;
				dev->data->scattered_rx = 1;
			}
		} else {
			/*
			 * Use BSIZE field of the device RCTL register.
			 */
			if ((rctl_bsize == 0) || (rctl_bsize > buf_size))
				rctl_bsize = buf_size;
			if (!dev->data->scattered_rx)
				PMD_INIT_LOG(DEBUG, "forcing scatter mode");
			dev->rx_pkt_burst = eth_igb_recv_scattered_pkts;
			dev->data->scattered_rx = 1;
		}

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= E1000_SRRCTL_DROP_EN;

		E1000_WRITE_REG(hw, E1000_SRRCTL(rxq->reg_idx), srrctl);

		/* Enable this RX queue. */
		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(rxq->reg_idx));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= (rxq->pthresh & 0x1F);
		rxdctl |= ((rxq->hthresh & 0x1F) << 8);
		rxdctl |= ((rxq->wthresh & 0x1F) << 16);
		E1000_WRITE_REG(hw, E1000_RXDCTL(rxq->reg_idx), rxdctl);
	}

	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_SCATTER) {
		if (!dev->data->scattered_rx)
			PMD_INIT_LOG(DEBUG, "forcing scatter mode");
		dev->rx_pkt_burst = eth_igb_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	/*
	 * Setup BSIZE field of RCTL register, if needed.
	 * Buffer sizes >= 1024 are not [supposed to be] setup in the RCTL
	 * register, since the code above configures the SRRCTL register of
	 * the RX queue in such a case.
	 * All configurable sizes are:
	 * 16384: rctl |= (E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX);
	 *  8192: rctl |= (E1000_RCTL_SZ_8192  | E1000_RCTL_BSEX);
	 *  4096: rctl |= (E1000_RCTL_SZ_4096  | E1000_RCTL_BSEX);
	 *  2048: rctl |= E1000_RCTL_SZ_2048;
	 *  1024: rctl |= E1000_RCTL_SZ_1024;
	 *   512: rctl |= E1000_RCTL_SZ_512;
	 *   256: rctl |= E1000_RCTL_SZ_256;
	 */
	if (rctl_bsize > 0) {
		if (rctl_bsize >= 512) /* 512 <= buf_size < 1024 - use 512 */
			rctl |= E1000_RCTL_SZ_512;
		else /* 256 <= buf_size < 512 - use 256 */
			rctl |= E1000_RCTL_SZ_256;
	}

	/*
	 * Configure RSS if device configured with multiple RX queues.
	 */
	igb_dev_mq_rx_configure(dev);

	/* Update the rctl since igb_dev_mq_rx_configure may change its value */
	rctl |= E1000_READ_REG(hw, E1000_RCTL);

	/*
	 * Setup the Checksum Register.
	 * Receive Full-Packet Checksum Offload is mutually exclusive with RSS.
	 */
	rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);
	rxcsum |= E1000_RXCSUM_PCSD;

	/* Enable both L3/L4 rx checksum offload */
	if (rxmode->offloads & DEV_RX_OFFLOAD_IPV4_CKSUM)
		rxcsum |= E1000_RXCSUM_IPOFL;
	else
		rxcsum &= ~E1000_RXCSUM_IPOFL;
	if (rxmode->offloads &
		(DEV_RX_OFFLOAD_TCP_CKSUM | DEV_RX_OFFLOAD_UDP_CKSUM))
		rxcsum |= E1000_RXCSUM_TUOFL;
	else
		rxcsum &= ~E1000_RXCSUM_TUOFL;
	if (rxmode->offloads & DEV_RX_OFFLOAD_CHECKSUM)
		rxcsum |= E1000_RXCSUM_CRCOFL;
	else
		rxcsum &= ~E1000_RXCSUM_CRCOFL;

	E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);

	/* Setup the Receive Control Register. */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_CRC_STRIP) {
		rctl |= E1000_RCTL_SECRC; /* Strip Ethernet CRC. */

		/* set STRCRC bit in all queues */
		if (hw->mac.type == e1000_i350 ||
		    hw->mac.type == e1000_i210 ||
		    hw->mac.type == e1000_i211 ||
		    hw->mac.type == e1000_i354) {
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				uint32_t dvmolr = E1000_READ_REG(hw,
					E1000_DVMOLR(rxq->reg_idx));
				dvmolr |= E1000_DVMOLR_STRCRC;
				E1000_WRITE_REG(hw, E1000_DVMOLR(rxq->reg_idx), dvmolr);
			}
		}
	} else {
		rctl &= ~E1000_RCTL_SECRC; /* Do not Strip Ethernet CRC. */

		/* clear STRCRC bit in all queues */
		if (hw->mac.type == e1000_i350 ||
		    hw->mac.type == e1000_i210 ||
		    hw->mac.type == e1000_i211 ||
		    hw->mac.type == e1000_i354) {
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				uint32_t dvmolr = E1000_READ_REG(hw,
					E1000_DVMOLR(rxq->reg_idx));
				dvmolr &= ~E1000_DVMOLR_STRCRC;
				E1000_WRITE_REG(hw, E1000_DVMOLR(rxq->reg_idx), dvmolr);
			}
		}
	}

	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		E1000_RCTL_RDMTS_HALF |
		(hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	/* Make sure VLAN Filters are off. */
	if (dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_VMDQ_ONLY)
		rctl &= ~E1000_RCTL_VFE;
	/* Don't store bad packets. */
	rctl &= ~E1000_RCTL_SBP;

	/* Enable Receives. */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers.
	 * This needs to be done after enable.
	 */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		E1000_WRITE_REG(hw, E1000_RDH(rxq->reg_idx), 0);
		E1000_WRITE_REG(hw, E1000_RDT(rxq->reg_idx), rxq->nb_rx_desc - 1);
	}

	return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void
eth_igb_tx_init(struct rte_eth_dev *dev)
{
	struct e1000_hw     *hw;
	struct igb_tx_queue *txq;
	uint32_t tctl;
	uint32_t txdctl;
	uint16_t i;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Setup the Base and Length of the Tx Descriptor Rings. */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		uint64_t bus_addr;
		txq = dev->data->tx_queues[i];
		bus_addr = txq->tx_ring_phys_addr;

		E1000_WRITE_REG(hw, E1000_TDLEN(txq->reg_idx),
				txq->nb_tx_desc *
				sizeof(union e1000_adv_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(txq->reg_idx), (uint32_t)bus_addr);

		/* Setup the HW Tx Head and Tail descriptor pointers. */
		E1000_WRITE_REG(hw, E1000_TDT(txq->reg_idx), 0);
		E1000_WRITE_REG(hw, E1000_TDH(txq->reg_idx), 0);

		/* Setup Transmit threshold registers. */
		txdctl = E1000_READ_REG(hw, E1000_TXDCTL(txq->reg_idx));
		txdctl |= txq->pthresh & 0x1F;
		txdctl |= ((txq->hthresh & 0x1F) << 8);
		txdctl |= ((txq->wthresh & 0x1F) << 16);
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(hw, E1000_TXDCTL(txq->reg_idx), txdctl);
	}

	/* Program the Transmit Control Register. */
	tctl = E1000_READ_REG(hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		 (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	e1000_config_collision_dist(hw);

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(hw, E1000_TCTL, tctl);
}

/*********************************************************************
 *
 *  Enable VF receive unit.
 *
 **********************************************************************/
int
eth_igbvf_rx_init(struct rte_eth_dev *dev)
{
	struct e1000_hw     *hw;
	struct igb_rx_queue *rxq;
	uint32_t srrctl;
	uint16_t buf_size;
	uint16_t rctl_bsize;
	uint16_t i;
	int ret;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* setup MTU */
	e1000_rlpml_set_vf(hw,
		(uint16_t)(dev->data->dev_conf.rxmode.max_rx_pkt_len +
		VLAN_TAG_SIZE));

	/* Configure and enable each RX queue. */
	rctl_bsize = 0;
	dev->rx_pkt_burst = eth_igb_recv_pkts;
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		uint64_t bus_addr;
		uint32_t rxdctl;

		rxq = dev->data->rx_queues[i];

		rxq->flags = 0;
		/*
		 * i350VF LB vlan packets have vlan tags byte swapped.
		 */
		if (hw->mac.type == e1000_vfadapt_i350) {
			rxq->flags |= IGB_RXQ_FLAG_LB_BSWAP_VLAN;
			PMD_INIT_LOG(DEBUG, "IGB rx vlan bswap required");
		} else {
			PMD_INIT_LOG(DEBUG, "IGB rx vlan bswap not required");
		}

		/* Allocate buffers for descriptor rings and set up queue */
		ret = igb_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		bus_addr = rxq->rx_ring_phys_addr;
		E1000_WRITE_REG(hw, E1000_RDLEN(i),
				rxq->nb_rx_desc *
				sizeof(union e1000_adv_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i), (uint32_t)bus_addr);

		srrctl = E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

		/*
		 * Configure RX buffer size.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		if (buf_size >= 1024) {
			/*
			 * Configure the BSIZEPACKET field of the SRRCTL
			 * register of the queue.
			 * Value is in 1 KB resolution, from 1 KB to 127 KB.
			 * If this field is equal to 0b, then RCTL.BSIZE
			 * determines the RX packet buffer size.
			 */
			srrctl |= ((buf_size >> E1000_SRRCTL_BSIZEPKT_SHIFT) &
				   E1000_SRRCTL_BSIZEPKT_MASK);
			buf_size = (uint16_t) ((srrctl &
						E1000_SRRCTL_BSIZEPKT_MASK) <<
					       E1000_SRRCTL_BSIZEPKT_SHIFT);

			/* It adds dual VLAN length for supporting dual VLAN */
			if ((dev->data->dev_conf.rxmode.max_rx_pkt_len +
						2 * VLAN_TAG_SIZE) > buf_size){
				if (!dev->data->scattered_rx)
					PMD_INIT_LOG(DEBUG,
						     "forcing scatter mode");
				dev->rx_pkt_burst = eth_igb_recv_scattered_pkts;
				dev->data->scattered_rx = 1;
			}
		} else {
			/*
			 * Use BSIZE field of the device RCTL register.
			 */
			if ((rctl_bsize == 0) || (rctl_bsize > buf_size))
				rctl_bsize = buf_size;
			if (!dev->data->scattered_rx)
				PMD_INIT_LOG(DEBUG, "forcing scatter mode");
			dev->rx_pkt_burst = eth_igb_recv_scattered_pkts;
			dev->data->scattered_rx = 1;
		}

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= E1000_SRRCTL_DROP_EN;

		E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);

		/* Enable this RX queue. */
		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= (rxq->pthresh & 0x1F);
		rxdctl |= ((rxq->hthresh & 0x1F) << 8);
		if (hw->mac.type == e1000_vfadapt) {
			/*
			 * Workaround of 82576 VF Erratum
			 * force set WTHRESH to 1
			 * to avoid Write-Back not triggered sometimes
			 */
			rxdctl |= 0x10000;
			PMD_INIT_LOG(DEBUG, "Force set RX WTHRESH to 1 !");
		}
		else
			rxdctl |= ((rxq->wthresh & 0x1F) << 16);
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
	}

	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_SCATTER) {
		if (!dev->data->scattered_rx)
			PMD_INIT_LOG(DEBUG, "forcing scatter mode");
		dev->rx_pkt_burst = eth_igb_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers.
	 * This needs to be done after enable.
	 */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		E1000_WRITE_REG(hw, E1000_RDH(i), 0);
		E1000_WRITE_REG(hw, E1000_RDT(i), rxq->nb_rx_desc - 1);
	}

	return 0;
}

/*********************************************************************
 *
 *  Enable VF transmit unit.
 *
 **********************************************************************/
void
eth_igbvf_tx_init(struct rte_eth_dev *dev)
{
	struct e1000_hw     *hw;
	struct igb_tx_queue *txq;
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
				sizeof(union e1000_adv_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(i), (uint32_t)bus_addr);

		/* Setup the HW Tx Head and Tail descriptor pointers. */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		/* Setup Transmit threshold registers. */
		txdctl = E1000_READ_REG(hw, E1000_TXDCTL(i));
		txdctl |= txq->pthresh & 0x1F;
		txdctl |= ((txq->hthresh & 0x1F) << 8);
		if (hw->mac.type == e1000_82576) {
			/*
			 * Workaround of 82576 VF Erratum
			 * force set WTHRESH to 1
			 * to avoid Write-Back not triggered sometimes
			 */
			txdctl |= 0x10000;
			PMD_INIT_LOG(DEBUG, "Force set TX WTHRESH to 1 !");
		}
		else
			txdctl |= ((txq->wthresh & 0x1F) << 16);
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
	}

}

void
igb_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct igb_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = rxq->drop_en;
	qinfo->conf.offloads = rxq->offloads;
}

void
igb_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct igb_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;
}

int
igb_config_rss_filter(struct rte_eth_dev *dev,
		struct igb_rte_flow_rss_conf *conf, bool add)
{
	uint32_t shift;
	uint16_t i, j;
	struct rte_eth_rss_conf rss_conf = conf->rss_conf;
	struct e1000_filter_info *filter_info =
		E1000_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!add) {
		if (memcmp(conf, &filter_info->rss_info,
			sizeof(struct igb_rte_flow_rss_conf)) == 0) {
			igb_rss_disable(dev);
			memset(&filter_info->rss_info, 0,
				sizeof(struct igb_rte_flow_rss_conf));
			return 0;
		}
		return -EINVAL;
	}

	if (filter_info->rss_info.num)
		return -EINVAL;

	/* Fill in redirection table. */
	shift = (hw->mac.type == e1000_82575) ? 6 : 0;
	for (i = 0, j = 0; i < 128; i++, j++) {
		union e1000_reta {
			uint32_t dword;
			uint8_t  bytes[4];
		} reta;
		uint8_t q_idx;

		if (j == conf->num)
			j = 0;
		q_idx = conf->queue[j];
		reta.bytes[i & 3] = (uint8_t)(q_idx << shift);
		if ((i & 3) == 3)
			E1000_WRITE_REG(hw, E1000_RETA(i >> 2), reta.dword);
	}

	/* Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	if ((rss_conf.rss_hf & IGB_RSS_OFFLOAD_ALL) == 0) {
		igb_rss_disable(dev);
		return 0;
	}
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rss_intel_key; /* Default hash key */
	igb_hw_rss_hash_set(hw, &rss_conf);

	rte_memcpy(&filter_info->rss_info,
		conf, sizeof(struct igb_rte_flow_rss_conf));

	return 0;
}
