/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#include "mvneta_rxtx.h"

#define MVNETA_PKT_EFFEC_OFFS (MRVL_NETA_PKT_OFFS + MV_MH_SIZE)

#define MRVL_NETA_DEFAULT_TC 0

/** Maximum number of descriptors in shadow queue. Must be power of 2 */
#define MRVL_NETA_TX_SHADOWQ_SIZE MRVL_NETA_TXD_MAX

/** Shadow queue size mask (since shadow queue size is power of 2) */
#define MRVL_NETA_TX_SHADOWQ_MASK (MRVL_NETA_TX_SHADOWQ_SIZE - 1)

/** Minimum number of sent buffers to release from shadow queue to BM */
#define MRVL_NETA_BUF_RELEASE_BURST_SIZE_MIN	16

/** Maximum number of sent buffers to release from shadow queue to BM */
#define MRVL_NETA_BUF_RELEASE_BURST_SIZE_MAX	64

#define MVNETA_COOKIE_ADDR_INVALID ~0ULL
#define MVNETA_COOKIE_HIGH_ADDR_SHIFT	(sizeof(neta_cookie_t) * 8)
#define MVNETA_COOKIE_HIGH_ADDR_MASK	(~0ULL << MVNETA_COOKIE_HIGH_ADDR_SHIFT)

#define MVNETA_SET_COOKIE_HIGH_ADDR(addr) {				\
	if (unlikely(cookie_addr_high == MVNETA_COOKIE_ADDR_INVALID))	\
		cookie_addr_high =					\
			(uint64_t)(addr) & MVNETA_COOKIE_HIGH_ADDR_MASK;\
}

#define MVNETA_CHECK_COOKIE_HIGH_ADDR(addr)		\
	((likely(cookie_addr_high ==			\
	((uint64_t)(addr) & MVNETA_COOKIE_HIGH_ADDR_MASK))) ? 1 : 0)

struct mvneta_rxq {
	struct mvneta_priv *priv;
	struct rte_mempool *mp;
	int queue_id;
	int port_id;
	int size;
	int cksum_enabled;
	uint64_t bytes_recv;
	uint64_t drop_mac;
	uint64_t pkts_processed;
};

/*
 * To use buffer harvesting based on loopback port shadow queue structure
 * was introduced for buffers information bookkeeping.
 */
struct mvneta_shadow_txq {
	int head;           /* write index - used when sending buffers */
	int tail;           /* read index - used when releasing buffers */
	u16 size;           /* queue occupied size */
	struct neta_buff_inf ent[MRVL_NETA_TX_SHADOWQ_SIZE]; /* q entries */
};

struct mvneta_txq {
	struct mvneta_priv *priv;
	int queue_id;
	int port_id;
	uint64_t bytes_sent;
	struct mvneta_shadow_txq shadow_txq;
	int tx_deferred_start;
};

static uint64_t cookie_addr_high = MVNETA_COOKIE_ADDR_INVALID;
static uint16_t rx_desc_free_thresh = MRVL_NETA_BUF_RELEASE_BURST_SIZE_MIN;

static inline int
mvneta_buffs_refill(struct mvneta_priv *priv, struct mvneta_rxq *rxq, u16 *num)
{
	struct rte_mbuf *mbufs[MRVL_NETA_BUF_RELEASE_BURST_SIZE_MAX];
	struct neta_buff_inf entries[MRVL_NETA_BUF_RELEASE_BURST_SIZE_MAX];
	int i, ret;
	uint16_t nb_desc = *num;

	/* To prevent GCC-12 warning. */
	if (unlikely(nb_desc == 0))
		return -1;

	ret = rte_pktmbuf_alloc_bulk(rxq->mp, mbufs, nb_desc);
	if (ret) {
		MVNETA_LOG(ERR, "Failed to allocate %u mbufs.", nb_desc);
		*num = 0;
		return -1;
	}

	MVNETA_SET_COOKIE_HIGH_ADDR(mbufs[0]);

	for (i = 0; i < nb_desc; i++) {
		if (unlikely(!MVNETA_CHECK_COOKIE_HIGH_ADDR(mbufs[i]))) {
			MVNETA_LOG(ERR,
				"mbuf virt high addr 0x%lx out of range 0x%lx",
				(uint64_t)mbufs[i] >> 32,
				cookie_addr_high >> 32);
			*num = 0;
			goto out;
		}
		entries[i].addr = rte_mbuf_data_iova_default(mbufs[i]);
		entries[i].cookie = (neta_cookie_t)(uint64_t)mbufs[i];
	}
	neta_ppio_inq_put_buffs(priv->ppio, rxq->queue_id, entries, num);

out:
	for (i = *num; i < nb_desc; i++)
		rte_pktmbuf_free(mbufs[i]);

	return 0;
}

/**
 * Allocate buffers from mempool
 * and store addresses in rx descriptors.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static inline int
mvneta_buffs_alloc(struct mvneta_priv *priv, struct mvneta_rxq *rxq, int *num)
{
	uint16_t nb_desc, nb_desc_burst, sent = 0;
	int ret = 0;

	nb_desc = *num;

	do {
		nb_desc_burst =
			(nb_desc < MRVL_NETA_BUF_RELEASE_BURST_SIZE_MAX) ?
			nb_desc : MRVL_NETA_BUF_RELEASE_BURST_SIZE_MAX;

		ret = mvneta_buffs_refill(priv, rxq, &nb_desc_burst);
		if (unlikely(ret || !nb_desc_burst))
			break;

		sent += nb_desc_burst;
		nb_desc -= nb_desc_burst;

	} while (nb_desc);

	*num = sent;

	return ret;
}

static inline void
mvneta_fill_shadowq(struct mvneta_shadow_txq *sq, struct rte_mbuf *buf)
{
	sq->ent[sq->head].cookie = (uint64_t)buf;
	sq->ent[sq->head].addr = buf ?
		rte_mbuf_data_iova_default(buf) : 0;

	sq->head = (sq->head + 1) & MRVL_NETA_TX_SHADOWQ_MASK;
	sq->size++;
}

static inline void
mvneta_fill_desc(struct neta_ppio_desc *desc, struct rte_mbuf *buf)
{
	neta_ppio_outq_desc_reset(desc);
	neta_ppio_outq_desc_set_phys_addr(desc, rte_pktmbuf_iova(buf));
	neta_ppio_outq_desc_set_pkt_offset(desc, 0);
	neta_ppio_outq_desc_set_pkt_len(desc, rte_pktmbuf_data_len(buf));
}

/**
 * Release already sent buffers to mempool.
 *
 * @param ppio
 *   Pointer to the port structure.
 * @param sq
 *   Pointer to the shadow queue.
 * @param qid
 *   Queue id number.
 * @param force
 *   Force releasing packets.
 */
static inline void
mvneta_sent_buffers_free(struct neta_ppio *ppio,
			 struct mvneta_shadow_txq *sq, int qid)
{
	struct neta_buff_inf *entry;
	uint16_t nb_done = 0;
	int i;
	int tail = sq->tail;

	neta_ppio_get_num_outq_done(ppio, qid, &nb_done);

	if (nb_done > sq->size) {
		MVNETA_LOG(ERR, "nb_done: %d, sq->size %d",
			   nb_done, sq->size);
		return;
	}

	for (i = 0; i < nb_done; i++) {
		entry = &sq->ent[tail];

		if (unlikely(!entry->addr)) {
			MVNETA_LOG(DEBUG,
				"Shadow memory @%d: cookie(%lx), pa(%lx)!",
				tail, (u64)entry->cookie,
				(u64)entry->addr);
			tail = (tail + 1) & MRVL_NETA_TX_SHADOWQ_MASK;
			continue;
		}

		struct rte_mbuf *mbuf;

		mbuf = (struct rte_mbuf *)
			   (cookie_addr_high | entry->cookie);
		rte_pktmbuf_free(mbuf);
		tail = (tail + 1) & MRVL_NETA_TX_SHADOWQ_MASK;
	}

	sq->tail = tail;
	sq->size -= nb_done;
}

/**
 * Return packet type information and l3/l4 offsets.
 *
 * @param desc
 *   Pointer to the received packet descriptor.
 * @param l3_offset
 *   l3 packet offset.
 * @param l4_offset
 *   l4 packet offset.
 *
 * @return
 *   Packet type information.
 */
static inline uint64_t
mvneta_desc_to_packet_type_and_offset(struct neta_ppio_desc *desc,
				    uint8_t *l3_offset, uint8_t *l4_offset)
{
	enum neta_inq_l3_type l3_type;
	enum neta_inq_l4_type l4_type;
	uint64_t packet_type;

	neta_ppio_inq_desc_get_l3_info(desc, &l3_type, l3_offset);
	neta_ppio_inq_desc_get_l4_info(desc, &l4_type, l4_offset);

	packet_type = RTE_PTYPE_L2_ETHER;

	if (NETA_RXD_GET_VLAN_INFO(desc))
		packet_type |= RTE_PTYPE_L2_ETHER_VLAN;

	switch (l3_type) {
	case NETA_INQ_L3_TYPE_IPV4_BAD:
	case NETA_INQ_L3_TYPE_IPV4_OK:
		packet_type |= RTE_PTYPE_L3_IPV4;
		break;
	case NETA_INQ_L3_TYPE_IPV6:
		packet_type |= RTE_PTYPE_L3_IPV6;
		break;
	default:
		packet_type |= RTE_PTYPE_UNKNOWN;
		MVNETA_LOG(DEBUG, "Failed to recognize l3 packet type");
		break;
	}

	switch (l4_type) {
	case NETA_INQ_L4_TYPE_TCP:
		packet_type |= RTE_PTYPE_L4_TCP;
		break;
	case NETA_INQ_L4_TYPE_UDP:
		packet_type |= RTE_PTYPE_L4_UDP;
		break;
	default:
		packet_type |= RTE_PTYPE_UNKNOWN;
		MVNETA_LOG(DEBUG, "Failed to recognize l4 packet type");
		break;
	}

	return packet_type;
}

/**
 * Prepare offload information.
 *
 * @param ol_flags
 *   Offload flags.
 * @param l3_type
 *   Pointer to the neta_ouq_l3_type structure.
 * @param l4_type
 *   Pointer to the neta_outq_l4_type structure.
 * @param gen_l3_cksum
 *   Will be set to 1 in case l3 checksum is computed.
 * @param l4_cksum
 *   Will be set to 1 in case l4 checksum is computed.
 */
static inline void
mvneta_prepare_proto_info(uint64_t ol_flags,
			  enum neta_outq_l3_type *l3_type,
			  enum neta_outq_l4_type *l4_type,
			  int *gen_l3_cksum,
			  int *gen_l4_cksum)
{
	/*
	 * Based on ol_flags prepare information
	 * for neta_ppio_outq_desc_set_proto_info() which setups descriptor
	 * for offloading.
	 * in most of the checksum cases ipv4 must be set, so this is the
	 * default value
	 */
	*l3_type = NETA_OUTQ_L3_TYPE_IPV4;
	*gen_l3_cksum = ol_flags & RTE_MBUF_F_TX_IP_CKSUM ? 1 : 0;

	if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		*l3_type = NETA_OUTQ_L3_TYPE_IPV6;
		/* no checksum for ipv6 header */
		*gen_l3_cksum = 0;
	}

	if (ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) {
		*l4_type = NETA_OUTQ_L4_TYPE_TCP;
		*gen_l4_cksum = 1;
	} else if (ol_flags & RTE_MBUF_F_TX_UDP_CKSUM) {
		*l4_type = NETA_OUTQ_L4_TYPE_UDP;
		*gen_l4_cksum = 1;
	} else {
		*l4_type = NETA_OUTQ_L4_TYPE_OTHER;
		/* no checksum for other type */
		*gen_l4_cksum = 0;
	}
}

/**
 * Get offload information from the received packet descriptor.
 *
 * @param desc
 *   Pointer to the received packet descriptor.
 *
 * @return
 *   Mbuf offload flags.
 */
static inline uint64_t
mvneta_desc_to_ol_flags(struct neta_ppio_desc *desc)
{
	uint64_t flags;
	enum neta_inq_desc_status status;

	status = neta_ppio_inq_desc_get_l3_pkt_error(desc);
	if (unlikely(status != NETA_DESC_ERR_OK))
		flags = RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	status = neta_ppio_inq_desc_get_l4_pkt_error(desc);
	if (unlikely(status != NETA_DESC_ERR_OK))
		flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	return flags;
}

/**
 * DPDK callback for transmit.
 *
 * @param txq
 *   Generic pointer transmit queue.
 * @param tx_pkts
 *   Packets to transmit.
 * @param nb_pkts
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted.
 */
static uint16_t
mvneta_tx_pkt_burst(void *txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct mvneta_txq *q = txq;
	struct mvneta_shadow_txq *sq;
	struct neta_ppio_desc descs[nb_pkts];
	int i, bytes_sent = 0;
	uint16_t num, sq_free_size;
	uint64_t addr;

	sq = &q->shadow_txq;
	if (unlikely(!nb_pkts || !q->priv->ppio))
		return 0;

	if (sq->size)
		mvneta_sent_buffers_free(q->priv->ppio,
					 sq, q->queue_id);

	sq_free_size = MRVL_NETA_TX_SHADOWQ_SIZE - sq->size - 1;
	if (unlikely(nb_pkts > sq_free_size)) {
		MVNETA_LOG(DEBUG,
			"No room in shadow queue for %d packets! %d packets will be sent.",
			nb_pkts, sq_free_size);
		nb_pkts = sq_free_size;
	}


	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *mbuf = tx_pkts[i];
		int gen_l3_cksum, gen_l4_cksum;
		enum neta_outq_l3_type l3_type;
		enum neta_outq_l4_type l4_type;

		/* Fill first mbuf info in shadow queue */
		mvneta_fill_shadowq(sq, mbuf);
		mvneta_fill_desc(&descs[i], mbuf);

		bytes_sent += rte_pktmbuf_pkt_len(mbuf);

		if (!(mbuf->ol_flags & MVNETA_TX_PKT_OFFLOADS))
			continue;
		mvneta_prepare_proto_info(mbuf->ol_flags, &l3_type, &l4_type,
					  &gen_l3_cksum, &gen_l4_cksum);

		neta_ppio_outq_desc_set_proto_info(&descs[i], l3_type, l4_type,
						   mbuf->l2_len,
						   mbuf->l2_len + mbuf->l3_len,
						   gen_l3_cksum, gen_l4_cksum);
	}
	num = nb_pkts;
	neta_ppio_send(q->priv->ppio, q->queue_id, descs, &nb_pkts);


	/* number of packets that were not sent */
	if (unlikely(num > nb_pkts)) {
		for (i = nb_pkts; i < num; i++) {
			sq->head = (MRVL_NETA_TX_SHADOWQ_SIZE + sq->head - 1) &
				MRVL_NETA_TX_SHADOWQ_MASK;
			addr = cookie_addr_high | sq->ent[sq->head].cookie;
			bytes_sent -=
				rte_pktmbuf_pkt_len((struct rte_mbuf *)addr);
		}
		sq->size -= num - nb_pkts;
	}

	q->bytes_sent += bytes_sent;

	return nb_pkts;
}

/** DPDK callback for S/G transmit.
 *
 * @param txq
 *   Generic pointer transmit queue.
 * @param tx_pkts
 *   Packets to transmit.
 * @param nb_pkts
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted.
 */
static uint16_t
mvneta_tx_sg_pkt_burst(void *txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct mvneta_txq *q = txq;
	struct mvneta_shadow_txq *sq;
	struct neta_ppio_desc descs[nb_pkts * NETA_PPIO_DESC_NUM_FRAGS];
	struct neta_ppio_sg_pkts pkts;
	uint8_t frags[nb_pkts];
	int i, j, bytes_sent = 0;
	int tail, tail_first;
	uint16_t num, sq_free_size;
	uint16_t nb_segs, total_descs = 0;
	uint64_t addr;

	sq = &q->shadow_txq;
	pkts.frags = frags;
	pkts.num = 0;

	if (unlikely(!q->priv->ppio))
		return 0;

	if (sq->size)
		mvneta_sent_buffers_free(q->priv->ppio,
					 sq, q->queue_id);
	/* Save shadow queue free size */
	sq_free_size = MRVL_NETA_TX_SHADOWQ_SIZE - sq->size - 1;

	tail = 0;
	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *mbuf = tx_pkts[i];
		struct rte_mbuf *seg = NULL;
		int gen_l3_cksum, gen_l4_cksum;
		enum neta_outq_l3_type l3_type;
		enum neta_outq_l4_type l4_type;

		nb_segs = mbuf->nb_segs;
		total_descs += nb_segs;

		/*
		 * Check if total_descs does not exceed
		 * shadow queue free size
		 */
		if (unlikely(total_descs > sq_free_size)) {
			total_descs -= nb_segs;
			MVNETA_LOG(DEBUG,
				"No room in shadow queue for %d packets! "
				"%d packets will be sent.",
				nb_pkts, i);
			break;
		}


		/* Check if nb_segs does not exceed the max nb of desc per
		 * fragmented packet
		 */
		if (unlikely(nb_segs > NETA_PPIO_DESC_NUM_FRAGS)) {
			total_descs -= nb_segs;
			MVNETA_LOG(ERR,
				"Too many segments. Packet won't be sent.");
			break;
		}

		pkts.frags[pkts.num] = nb_segs;
		pkts.num++;
		tail_first = tail;

		seg = mbuf;
		for (j = 0; j < nb_segs - 1; j++) {
			/* For the subsequent segments, set shadow queue
			 * buffer to NULL
			 */
			mvneta_fill_shadowq(sq, NULL);
			mvneta_fill_desc(&descs[tail], seg);

			tail++;
			seg = seg->next;
		}
		/* Put first mbuf info in last shadow queue entry */
		mvneta_fill_shadowq(sq, mbuf);
		/* Update descriptor with last segment */
		mvneta_fill_desc(&descs[tail++], seg);

		bytes_sent += rte_pktmbuf_pkt_len(mbuf);

		if (!(mbuf->ol_flags & MVNETA_TX_PKT_OFFLOADS))
			continue;
		mvneta_prepare_proto_info(mbuf->ol_flags, &l3_type, &l4_type,
					  &gen_l3_cksum, &gen_l4_cksum);

		neta_ppio_outq_desc_set_proto_info(&descs[tail_first],
						   l3_type, l4_type,
						   mbuf->l2_len,
						   mbuf->l2_len + mbuf->l3_len,
						   gen_l3_cksum, gen_l4_cksum);
	}
	num = total_descs;
	neta_ppio_send_sg(q->priv->ppio, q->queue_id, descs, &total_descs,
			  &pkts);

	/* number of packets that were not sent */
	if (unlikely(num > total_descs)) {
		for (i = total_descs; i < num; i++) {
			sq->head = (MRVL_NETA_TX_SHADOWQ_SIZE +
					sq->head - 1) &
					MRVL_NETA_TX_SHADOWQ_MASK;
			addr = sq->ent[sq->head].cookie;
			if (addr) {
				struct rte_mbuf *mbuf;

				mbuf = (struct rte_mbuf *)
						(cookie_addr_high | addr);
				bytes_sent -= rte_pktmbuf_pkt_len(mbuf);
			}
		}
		sq->size -= num - total_descs;
		nb_pkts = pkts.num;
	}

	q->bytes_sent += bytes_sent;

	return nb_pkts;
}

/**
 * Set tx burst function according to offload flag
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mvneta_set_tx_function(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;

	/* Use a simple Tx queue (no offloads, no multi segs) if possible */
	if (priv->multiseg) {
		MVNETA_LOG(INFO, "Using multi-segment tx callback");
		dev->tx_pkt_burst = mvneta_tx_sg_pkt_burst;
	} else {
		MVNETA_LOG(INFO, "Using single-segment tx callback");
		dev->tx_pkt_burst = mvneta_tx_pkt_burst;
	}
}

/**
 * DPDK callback for receive.
 *
 * @param rxq
 *   Generic pointer to the receive queue.
 * @param rx_pkts
 *   Array to store received packets.
 * @param nb_pkts
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received.
 */
uint16_t
mvneta_rx_pkt_burst(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct mvneta_rxq *q = rxq;
	struct neta_ppio_desc descs[nb_pkts];
	int i, ret, rx_done = 0, rx_dropped = 0;

	if (unlikely(!q || !q->priv->ppio))
		return 0;

	ret = neta_ppio_recv(q->priv->ppio, q->queue_id,
			descs, &nb_pkts);

	if (unlikely(ret < 0)) {
		MVNETA_LOG(ERR, "Failed to receive packets");
		return 0;
	}

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *mbuf;
		uint8_t l3_offset, l4_offset;
		enum neta_inq_desc_status status;
		uint64_t addr;

		addr = cookie_addr_high |
			neta_ppio_inq_desc_get_cookie(&descs[i]);
		mbuf = (struct rte_mbuf *)addr;

		rte_pktmbuf_reset(mbuf);

		/* drop packet in case of mac, overrun or resource error */
		status = neta_ppio_inq_desc_get_l2_pkt_error(&descs[i]);
		if (unlikely(status != NETA_DESC_ERR_OK)) {
			/* Release the mbuf to the mempool since
			 * it won't be transferred to tx path
			 */
			rte_pktmbuf_free(mbuf);
			q->drop_mac++;
			rx_dropped++;
			continue;
		}

		mbuf->data_off += MVNETA_PKT_EFFEC_OFFS;
		mbuf->pkt_len = neta_ppio_inq_desc_get_pkt_len(&descs[i]);
		mbuf->data_len = mbuf->pkt_len;
		mbuf->port = q->port_id;
		mbuf->packet_type =
			mvneta_desc_to_packet_type_and_offset(&descs[i],
								&l3_offset,
								&l4_offset);
		mbuf->l2_len = l3_offset;
		mbuf->l3_len = l4_offset - l3_offset;

		if (likely(q->cksum_enabled))
			mbuf->ol_flags = mvneta_desc_to_ol_flags(&descs[i]);

		rx_pkts[rx_done++] = mbuf;
		q->bytes_recv += mbuf->pkt_len;
	}
	q->pkts_processed += rx_done + rx_dropped;

	if (q->pkts_processed > rx_desc_free_thresh) {
		int buf_to_refill = rx_desc_free_thresh;

		ret = mvneta_buffs_alloc(q->priv, q, &buf_to_refill);
		if (ret)
			MVNETA_LOG(ERR, "Refill failed");
		q->pkts_processed -= buf_to_refill;
	}

	return rx_done;
}

/**
 * DPDK callback to configure the receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param conf
 *   Thresholds parameters (unused_).
 * @param mp
 *   Memory pool for buffer allocations.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
int
mvneta_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		      unsigned int socket,
		      const struct rte_eth_rxconf *conf __rte_unused,
		      struct rte_mempool *mp)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	struct mvneta_rxq *rxq;
	uint32_t frame_size, buf_size = rte_pktmbuf_data_room_size(mp);
	uint32_t max_rx_pktlen = dev->data->mtu + RTE_ETHER_HDR_LEN;

	frame_size = buf_size - RTE_PKTMBUF_HEADROOM - MVNETA_PKT_EFFEC_OFFS;

	if (frame_size < max_rx_pktlen) {
		MVNETA_LOG(ERR,
			"Mbuf size must be increased to %u bytes to hold up "
			"to %u bytes of data.",
			max_rx_pktlen + buf_size - frame_size,
			max_rx_pktlen);
		dev->data->mtu = frame_size - RTE_ETHER_HDR_LEN;
		MVNETA_LOG(INFO, "Setting MTU to %u", dev->data->mtu);
	}

	if (dev->data->rx_queues[idx]) {
		rte_free(dev->data->rx_queues[idx]);
		dev->data->rx_queues[idx] = NULL;
	}

	rxq = rte_zmalloc_socket("rxq", sizeof(*rxq), 0, socket);
	if (!rxq)
		return -ENOMEM;

	rxq->priv = priv;
	rxq->mp = mp;
	rxq->cksum_enabled = dev->data->dev_conf.rxmode.offloads &
			     RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
	rxq->queue_id = idx;
	rxq->port_id = dev->data->port_id;
	rxq->size = desc;
	rx_desc_free_thresh = RTE_MIN(rx_desc_free_thresh, (desc / 2));
	priv->ppio_params.inqs_params.tcs_params[MRVL_NETA_DEFAULT_TC].size =
		desc;

	dev->data->rx_queues[idx] = rxq;

	return 0;
}

/**
 * DPDK callback to configure the transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Transmit queue index.
 * @param desc
 *   Number of descriptors to configure in the queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param conf
 *   Tx queue configuration parameters.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
int
mvneta_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		      unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	struct mvneta_txq *txq;

	if (dev->data->tx_queues[idx]) {
		rte_free(dev->data->tx_queues[idx]);
		dev->data->tx_queues[idx] = NULL;
	}

	txq = rte_zmalloc_socket("txq", sizeof(*txq), 0, socket);
	if (!txq)
		return -ENOMEM;

	txq->priv = priv;
	txq->queue_id = idx;
	txq->port_id = dev->data->port_id;
	txq->tx_deferred_start = conf->tx_deferred_start;
	dev->data->tx_queues[idx] = txq;

	priv->ppio_params.outqs_params.outqs_params[idx].size = desc;
	priv->ppio_params.outqs_params.outqs_params[idx].weight = 1;

	return 0;
}

/**
 * DPDK callback to release the transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param qid
 *   Transmit queue index.
 */
void
mvneta_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct mvneta_txq *q = dev->data->tx_queues[qid];

	if (!q)
		return;

	rte_free(q);
}

/**
 * Return mbufs to mempool.
 *
 * @param rxq
 *    Pointer to rx queue structure
 * @param desc
 *    Array of rx descriptors
 */
static void
mvneta_recv_buffs_free(struct neta_ppio_desc *desc, uint16_t num)
{
	uint64_t addr;
	uint8_t i;

	for (i = 0; i < num; i++) {
		if (desc) {
			addr = cookie_addr_high |
					neta_ppio_inq_desc_get_cookie(desc);
			if (addr)
				rte_pktmbuf_free((struct rte_mbuf *)addr);
			desc++;
		}
	}
}

int
mvneta_alloc_rx_bufs(struct rte_eth_dev *dev)
{
	struct mvneta_priv *priv = dev->data->dev_private;
	int ret = 0, i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct mvneta_rxq *rxq = dev->data->rx_queues[i];
		int num = rxq->size;

		ret = mvneta_buffs_alloc(priv, rxq, &num);
		if (ret || num != rxq->size) {
			rte_free(rxq);
			return ret;
		}
	}

	return 0;
}

/**
 * Flush single receive queue.
 *
 * @param rxq
 *   Pointer to rx queue structure.
 * @param descs
 *   Array of rx descriptors
 */
static void
mvneta_rx_queue_flush(struct mvneta_rxq *rxq)
{
	struct neta_ppio_desc *descs;
	struct neta_buff_inf *bufs;
	uint16_t num;
	int ret, i;

	descs = rte_malloc("rxdesc", MRVL_NETA_RXD_MAX * sizeof(*descs), 0);
	if (descs == NULL) {
		MVNETA_LOG(ERR, "Failed to allocate descs.");
		return;
	}

	bufs = rte_malloc("buffs", MRVL_NETA_RXD_MAX * sizeof(*bufs), 0);
	if (bufs == NULL) {
		MVNETA_LOG(ERR, "Failed to allocate bufs.");
		rte_free(descs);
		return;
	}

	do {
		num = MRVL_NETA_RXD_MAX;
		ret = neta_ppio_recv(rxq->priv->ppio,
				     rxq->queue_id,
				     descs, &num);
		mvneta_recv_buffs_free(descs, num);
	} while (ret == 0 && num);

	rxq->pkts_processed = 0;

	num = MRVL_NETA_RXD_MAX;

	neta_ppio_inq_get_all_buffs(rxq->priv->ppio, rxq->queue_id, bufs, &num);
	MVNETA_LOG(INFO, "freeing %u unused bufs.", num);

	for (i = 0; i < num; i++) {
		uint64_t addr;
		if (bufs[i].cookie) {
			addr = cookie_addr_high | bufs[i].cookie;
			rte_pktmbuf_free((struct rte_mbuf *)addr);
		}
	}

	rte_free(descs);
	rte_free(bufs);
}

/**
 * Flush single transmit queue.
 *
 * @param txq
 *     Pointer to tx queue structure
 */
static void
mvneta_tx_queue_flush(struct mvneta_txq *txq)
{
	struct mvneta_shadow_txq *sq = &txq->shadow_txq;

	if (sq->size)
		mvneta_sent_buffers_free(txq->priv->ppio, sq,
					 txq->queue_id);

	/* free the rest of them */
	while (sq->tail != sq->head) {
		uint64_t addr = cookie_addr_high |
			sq->ent[sq->tail].cookie;
		rte_pktmbuf_free((struct rte_mbuf *)addr);
		sq->tail = (sq->tail + 1) & MRVL_NETA_TX_SHADOWQ_MASK;
	}
	memset(sq, 0, sizeof(*sq));
}

void
mvneta_flush_queues(struct rte_eth_dev *dev)
{
	int i;

	MVNETA_LOG(INFO, "Flushing rx queues");
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct mvneta_rxq *rxq = dev->data->rx_queues[i];

		mvneta_rx_queue_flush(rxq);
	}

	MVNETA_LOG(INFO, "Flushing tx queues");
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct mvneta_txq *txq = dev->data->tx_queues[i];

		mvneta_tx_queue_flush(txq);
	}
}

/**
 * DPDK callback to release the receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param qid
 *   Receive queue index.
 */
void
mvneta_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct mvneta_rxq *q = dev->data->rx_queues[qid];

	if (!q)
		return;

	/* If dev_stop was called already, mbufs are already
	 * returned to mempool and ppio is deinitialized.
	 * Skip this step.
	 */

	if (q->priv->ppio)
		mvneta_rx_queue_flush(q);

	rte_free(q);
}

/**
 * DPDK callback to get information about specific receive queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_queue_id
 *   Receive queue index.
 * @param qinfo
 *   Receive queue information structure.
 */
void
mvneta_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		    struct rte_eth_rxq_info *qinfo)
{
	struct mvneta_rxq *q = dev->data->rx_queues[rx_queue_id];

	qinfo->mp = q->mp;
	qinfo->nb_desc = q->size;
}

/**
 * DPDK callback to get information about specific transmit queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param tx_queue_id
 *   Transmit queue index.
 * @param qinfo
 *   Transmit queue information structure.
 */
void
mvneta_txq_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		    struct rte_eth_txq_info *qinfo)
{
	struct mvneta_priv *priv = dev->data->dev_private;

	qinfo->nb_desc =
		priv->ppio_params.outqs_params.outqs_params[tx_queue_id].size;
}
