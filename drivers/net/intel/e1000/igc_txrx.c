/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_config.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_net.h>

#include "e1000_logs.h"
#include "igc_txrx.h"

#ifdef RTE_PMD_USE_PREFETCH
#define rte_igc_prefetch(p)		rte_prefetch0(p)
#else
#define rte_igc_prefetch(p)		do {} while (0)
#endif

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)		rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)		do {} while (0)
#endif

/* Multicast / Unicast table offset mask. */
#define E1000_RCTL_MO_MSK			(3u << E1000_RCTL_MO_SHIFT)

/* Loopback mode. */
#define E1000_RCTL_LBM_SHIFT		6
#define E1000_RCTL_LBM_MSK		(3u << E1000_RCTL_LBM_SHIFT)

/* Hash select for MTA */
#define E1000_RCTL_HSEL_SHIFT		8
#define E1000_RCTL_HSEL_MSK		(3u << E1000_RCTL_HSEL_SHIFT)
#define E1000_RCTL_PSP			(1u << 21)

/* Receive buffer size for header buffer */
#define IGC_SRRCTL_BSIZEHEADER_SHIFT	8

/* RX descriptor status and error flags */
#define IGC_RXD_STAT_L4CS		(1u << 5)
#define IGC_RXD_STAT_VEXT		(1u << 9)
#define IGC_RXD_STAT_LLINT		(1u << 11)
#define IGC_RXD_STAT_SCRC		(1u << 12)
#define IGC_RXD_STAT_SMDT_MASK		(3u << 13)
#define IGC_RXD_STAT_MC			(1u << 19)
#define IGC_RXD_EXT_ERR_L4E		(1u << 29)
#define IGC_RXD_EXT_ERR_IPE		(1u << 30)
#define IGC_RXD_EXT_ERR_RXE		(1u << 31)
#define IGC_RXD_RSS_TYPE_MASK		0xfu
#define IGC_RXD_PCTYPE_MASK		(0x7fu << 4)
#define IGC_RXD_ETQF_SHIFT		12
#define IGC_RXD_ETQF_MSK		(0xfu << IGC_RXD_ETQF_SHIFT)
#define IGC_RXD_VPKT			(1u << 16)

/* TXD control bits */
#define IGC_TXDCTL_PTHRESH_SHIFT	0
#define IGC_TXDCTL_HTHRESH_SHIFT	8
#define IGC_TXDCTL_WTHRESH_SHIFT	16
#define IGC_TXDCTL_PTHRESH_MSK		(0x1fu << IGC_TXDCTL_PTHRESH_SHIFT)
#define IGC_TXDCTL_HTHRESH_MSK		(0x1fu << IGC_TXDCTL_HTHRESH_SHIFT)
#define IGC_TXDCTL_WTHRESH_MSK		(0x1fu << IGC_TXDCTL_WTHRESH_SHIFT)

/* RXD control bits */
#define IGC_RXDCTL_PTHRESH_SHIFT	0
#define IGC_RXDCTL_HTHRESH_SHIFT	8
#define IGC_RXDCTL_WTHRESH_SHIFT	16
#define IGC_RXDCTL_PTHRESH_MSK		(0x1fu << IGC_RXDCTL_PTHRESH_SHIFT)
#define IGC_RXDCTL_HTHRESH_MSK		(0x1fu << IGC_RXDCTL_HTHRESH_SHIFT)
#define IGC_RXDCTL_WTHRESH_MSK		(0x1fu << IGC_RXDCTL_WTHRESH_SHIFT)

#define IGC_TSO_MAX_HDRLEN		512
#define IGC_TSO_MAX_MSS			9216

/* Bit Mask to indicate what bits required for building TX context */
#define IGC_TX_OFFLOAD_MASK (RTE_MBUF_F_TX_OUTER_IPV4 |	\
		RTE_MBUF_F_TX_IPV6 |		\
		RTE_MBUF_F_TX_IPV4 |		\
		RTE_MBUF_F_TX_VLAN |	\
		RTE_MBUF_F_TX_IP_CKSUM |	\
		RTE_MBUF_F_TX_L4_MASK |	\
		RTE_MBUF_F_TX_TCP_SEG |	\
		RTE_MBUF_F_TX_UDP_SEG | \
		RTE_MBUF_F_TX_IEEE1588_TMST)

#define IGC_TX_OFFLOAD_SEG	(RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)

#define IGC_ADVTXD_POPTS_TXSM	0x00000200 /* L4 Checksum offload request */
#define IGC_ADVTXD_POPTS_IXSM	0x00000100 /* IP Checksum offload request */

/* L4 Packet TYPE of Reserved */
#define IGC_ADVTXD_TUCMD_L4T_RSV	0x00001800

#define IGC_TX_OFFLOAD_NOTSUP_MASK (RTE_MBUF_F_TX_OFFLOAD_MASK ^ IGC_TX_OFFLOAD_MASK)

#define IGC_TS_HDR_LEN 16

static inline uint64_t
rx_desc_statuserr_to_pkt_flags(uint32_t statuserr)
{
	static uint64_t l4_chksum_flags[] = {0, 0,
			RTE_MBUF_F_RX_L4_CKSUM_GOOD,
			RTE_MBUF_F_RX_L4_CKSUM_BAD};

	static uint64_t l3_chksum_flags[] = {0, 0,
			RTE_MBUF_F_RX_IP_CKSUM_GOOD,
			RTE_MBUF_F_RX_IP_CKSUM_BAD};
	uint64_t pkt_flags = 0;
	uint32_t tmp;

	if (statuserr & E1000_RXD_STAT_VP)
		pkt_flags |= RTE_MBUF_F_RX_VLAN_STRIPPED;

	tmp = !!(statuserr & (IGC_RXD_STAT_L4CS | E1000_RXD_STAT_UDPCS));
	tmp = (tmp << 1) | (uint32_t)!!(statuserr & IGC_RXD_EXT_ERR_L4E);
	pkt_flags |= l4_chksum_flags[tmp];

	tmp = !!(statuserr & E1000_RXD_STAT_IPCS);
	tmp = (tmp << 1) | (uint32_t)!!(statuserr & IGC_RXD_EXT_ERR_IPE);
	pkt_flags |= l3_chksum_flags[tmp];

	return pkt_flags;
}

#define IGC_PACKET_TYPE_IPV4              0X01
#define IGC_PACKET_TYPE_IPV4_TCP          0X11
#define IGC_PACKET_TYPE_IPV4_UDP          0X21
#define IGC_PACKET_TYPE_IPV4_SCTP         0X41
#define IGC_PACKET_TYPE_IPV4_EXT          0X03
#define IGC_PACKET_TYPE_IPV4_EXT_SCTP     0X43
#define IGC_PACKET_TYPE_IPV6              0X04
#define IGC_PACKET_TYPE_IPV6_TCP          0X14
#define IGC_PACKET_TYPE_IPV6_UDP          0X24
#define IGC_PACKET_TYPE_IPV6_EXT          0X0C
#define IGC_PACKET_TYPE_IPV6_EXT_TCP      0X1C
#define IGC_PACKET_TYPE_IPV6_EXT_UDP      0X2C
#define IGC_PACKET_TYPE_IPV4_IPV6         0X05
#define IGC_PACKET_TYPE_IPV4_IPV6_TCP     0X15
#define IGC_PACKET_TYPE_IPV4_IPV6_UDP     0X25
#define IGC_PACKET_TYPE_IPV4_IPV6_EXT     0X0D
#define IGC_PACKET_TYPE_IPV4_IPV6_EXT_TCP 0X1D
#define IGC_PACKET_TYPE_IPV4_IPV6_EXT_UDP 0X2D
#define IGC_PACKET_TYPE_MAX               0X80
#define IGC_PACKET_TYPE_MASK              0X7F
#define IGC_PACKET_TYPE_SHIFT             0X04

static inline uint32_t
rx_desc_pkt_info_to_pkt_type(uint32_t pkt_info)
{
	static const alignas(RTE_CACHE_LINE_SIZE) uint32_t
		ptype_table[IGC_PACKET_TYPE_MAX] = {
		[IGC_PACKET_TYPE_IPV4] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4,
		[IGC_PACKET_TYPE_IPV4_EXT] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4_EXT,
		[IGC_PACKET_TYPE_IPV6] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6,
		[IGC_PACKET_TYPE_IPV4_IPV6] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6,
		[IGC_PACKET_TYPE_IPV6_EXT] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT,
		[IGC_PACKET_TYPE_IPV4_IPV6_EXT] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT,
		[IGC_PACKET_TYPE_IPV4_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
		[IGC_PACKET_TYPE_IPV6_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
		[IGC_PACKET_TYPE_IPV4_IPV6_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6 | RTE_PTYPE_INNER_L4_TCP,
		[IGC_PACKET_TYPE_IPV6_EXT_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_TCP,
		[IGC_PACKET_TYPE_IPV4_IPV6_EXT_TCP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT | RTE_PTYPE_INNER_L4_TCP,
		[IGC_PACKET_TYPE_IPV4_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
		[IGC_PACKET_TYPE_IPV6_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
		[IGC_PACKET_TYPE_IPV4_IPV6_UDP] =  RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6 | RTE_PTYPE_INNER_L4_UDP,
		[IGC_PACKET_TYPE_IPV6_EXT_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_UDP,
		[IGC_PACKET_TYPE_IPV4_IPV6_EXT_UDP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT | RTE_PTYPE_INNER_L4_UDP,
		[IGC_PACKET_TYPE_IPV4_SCTP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_SCTP,
		[IGC_PACKET_TYPE_IPV4_EXT_SCTP] = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_SCTP,
	};
	if (unlikely(pkt_info & E1000_RXDADV_PKTTYPE_ETQF))
		return RTE_PTYPE_UNKNOWN;

	pkt_info = (pkt_info >> IGC_PACKET_TYPE_SHIFT) & IGC_PACKET_TYPE_MASK;

	return ptype_table[pkt_info];
}

static inline void
rx_desc_get_pkt_info(struct igc_rx_queue *rxq, struct rte_mbuf *rxm,
		union e1000_adv_rx_desc *rxd, uint32_t staterr)
{
	uint64_t pkt_flags;
	uint32_t hlen_type_rss;
	uint16_t pkt_info;

	/* Prefetch data of first segment, if configured to do so. */
	rte_packet_prefetch((char *)rxm->buf_addr + rxm->data_off);

	rxm->port = rxq->port_id;
	hlen_type_rss = rte_le_to_cpu_32(rxd->wb.lower.lo_dword.data);
	rxm->hash.rss = rte_le_to_cpu_32(rxd->wb.lower.hi_dword.rss);
	rxm->vlan_tci = rte_le_to_cpu_16(rxd->wb.upper.vlan);

	pkt_flags = (hlen_type_rss & IGC_RXD_RSS_TYPE_MASK) ?
			RTE_MBUF_F_RX_RSS_HASH : 0;

	if (hlen_type_rss & IGC_RXD_VPKT)
		pkt_flags |= RTE_MBUF_F_RX_VLAN;

	pkt_flags |= rx_desc_statuserr_to_pkt_flags(staterr);

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		pkt_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;

	rxm->ol_flags = pkt_flags;
	pkt_info = rte_le_to_cpu_16(rxd->wb.lower.lo_dword.hs_rss.pkt_info);
	rxm->packet_type = rx_desc_pkt_info_to_pkt_type(pkt_info);
}

uint16_t
igc_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct igc_rx_queue * const rxq = rx_queue;
	volatile union e1000_adv_rx_desc * const rx_ring = rxq->rx_ring;
	struct igc_rx_entry * const sw_ring = rxq->sw_ring;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;

	while (nb_rx < nb_pkts) {
		volatile union e1000_adv_rx_desc *rxdp;
		struct igc_rx_entry *rxe;
		struct rte_mbuf *rxm;
		struct rte_mbuf *nmb;
		union e1000_adv_rx_desc rxd;
		uint32_t staterr;
		uint16_t data_len;

		/*
		 * The order of operations here is important as the DD status
		 * bit must not be read after any other descriptor fields.
		 * rx_ring and rxdp are pointing to volatile data so the order
		 * of accesses cannot be reordered by the compiler. If they were
		 * not volatile, they could be reordered which could lead to
		 * using invalid descriptor fields when read from rxd.
		 */
		rxdp = &rx_ring[rx_id];
		staterr = rte_cpu_to_le_32(rxdp->wb.upper.status_error);
		if (!(staterr & E1000_RXD_STAT_DD))
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
		PMD_RX_LOG(DEBUG,
			"port_id=%u queue_id=%u rx_id=%u staterr=0x%x data_len=%u",
			rxq->port_id, rxq->queue_id, rx_id, staterr,
			rte_le_to_cpu_16(rxd.wb.upper.length));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			unsigned int id;
			PMD_RX_LOG(DEBUG,
				"RX mbuf alloc failed, port_id=%u queue_id=%u",
				rxq->port_id, rxq->queue_id);
			id = rxq->port_id;
			rte_eth_devices[id].data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id >= rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_igc_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_igc_prefetch(&rx_ring[rx_id]);
			rte_igc_prefetch(&sw_ring[rx_id]);
		}

		/*
		 * Update RX descriptor with the physical address of the new
		 * data buffer of the new allocated mbuf.
		 */
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		rxdp->read.hdr_addr = 0;

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			rxdp->read.pkt_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb)) -
			IGC_TS_HDR_LEN;
		else
			rxdp->read.pkt_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		rxm->next = NULL;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		data_len = rte_le_to_cpu_16(rxd.wb.upper.length) - rxq->crc_len;
		/*
		 * When the RTE_ETH_RX_OFFLOAD_TIMESTAMP offload is enabled the
		 * length in the descriptor still accounts for the timestamp so
		 * it must be subtracted.
		 */
		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			data_len -= IGC_TS_HDR_LEN;
		rxm->data_len = data_len;
		rxm->pkt_len = data_len;
		rxm->nb_segs = 1;

		rx_desc_get_pkt_info(rxq, rxm, &rxd, staterr);

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
			uint32_t *ts = rte_pktmbuf_mtod_offset(rxm,
					uint32_t *, -IGC_TS_HDR_LEN);
			rxq->rx_timestamp = (uint64_t)ts[3] * NSEC_PER_SEC +
					ts[2];
			rxm->timesync = rxq->queue_id;
		}

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
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	nb_hold = nb_hold + rxq->nb_rx_hold;
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG,
			"port_id=%u queue_id=%u rx_tail=%u nb_hold=%u nb_rx=%u",
			rxq->port_id, rxq->queue_id, rx_id, nb_hold, nb_rx);
		rx_id = (rx_id == 0) ? (rxq->nb_rx_desc - 1) : (rx_id - 1);
		E1000_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

uint16_t
igc_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct igc_rx_queue * const rxq = rx_queue;
	volatile union e1000_adv_rx_desc * const rx_ring = rxq->rx_ring;
	struct igc_rx_entry * const sw_ring = rxq->sw_ring;
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg = rxq->pkt_last_seg;

	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;

	while (nb_rx < nb_pkts) {
		volatile union e1000_adv_rx_desc *rxdp;
		struct igc_rx_entry *rxe;
		struct rte_mbuf *rxm;
		struct rte_mbuf *nmb;
		union e1000_adv_rx_desc rxd;
		uint32_t staterr;
		uint16_t data_len;

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
		staterr = rte_cpu_to_le_32(rxdp->wb.upper.status_error);
		if (!(staterr & E1000_RXD_STAT_DD))
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
		PMD_RX_LOG(DEBUG,
			"port_id=%u queue_id=%u rx_id=%u staterr=0x%x data_len=%u",
			rxq->port_id, rxq->queue_id, rx_id, staterr,
			rte_le_to_cpu_16(rxd.wb.upper.length));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			unsigned int id;
			PMD_RX_LOG(DEBUG,
				"RX mbuf alloc failed, port_id=%u queue_id=%u",
				rxq->port_id, rxq->queue_id);
			id = rxq->port_id;
			rte_eth_devices[id].data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id >= rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_igc_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_igc_prefetch(&rx_ring[rx_id]);
			rte_igc_prefetch(&sw_ring[rx_id]);
		}

		/*
		 * Update RX descriptor with the physical address of the new
		 * data buffer of the new allocated mbuf.
		 */
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		rxdp->read.hdr_addr = 0;

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			rxdp->read.pkt_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb)) -
				IGC_TS_HDR_LEN;
		else
			rxdp->read.pkt_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		rxm->next = NULL;

		/*
		 * Set data length & data buffer address of mbuf.
		 */
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		data_len = rte_le_to_cpu_16(rxd.wb.upper.length);
		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
			/*
			 * When the RTE_ETH_RX_OFFLOAD_TIMESTAMP offload is enabled
			 * the pkt_addr of all software ring entries is moved forward
			 * by IGC_TS_HDR_LEN (see igc_alloc_rx_queue_mbufs()) so that
			 * when the hardware writes the packet with a prepended
			 * timestamp the actual packet data still starts at the
			 * normal data offset. The length in the descriptor still
			 * accounts for the timestamp so it needs to be subtracted.
			 * Follow-up mbufs do not have the timestamp so the data
			 * offset must be adjusted to point to the start of the packet
			 * data.
			 */
			if (first_seg == NULL)
				data_len -= IGC_TS_HDR_LEN;
			else
				rxm->data_off -= IGC_TS_HDR_LEN;
		}
		rxm->data_len = data_len;

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
		if (!(staterr & E1000_RXD_STAT_EOP)) {
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
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (data_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->nb_segs--;
				last_seg->data_len = last_seg->data_len -
					 (RTE_ETHER_CRC_LEN - data_len);
				last_seg->next = NULL;
				rxm = last_seg;
			} else {
				rxm->data_len = (uint16_t)
					(data_len - RTE_ETHER_CRC_LEN);
			}
		}

		rx_desc_get_pkt_info(rxq, first_seg, &rxd, staterr);

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
			uint32_t *ts = rte_pktmbuf_mtod_offset(first_seg,
					uint32_t *, -IGC_TS_HDR_LEN);
			rxq->rx_timestamp = (uint64_t)ts[3] * NSEC_PER_SEC +
					ts[2];
			rxm->timesync = rxq->queue_id;
		}

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = first_seg;

		/* Setup receipt context for a new packet. */
		first_seg = NULL;
	}
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
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	nb_hold = nb_hold + rxq->nb_rx_hold;
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG,
			"port_id=%u queue_id=%u rx_tail=%u nb_hold=%u nb_rx=%u",
			rxq->port_id, rxq->queue_id, rx_id, nb_hold, nb_rx);
		rx_id = (rx_id == 0) ? (rxq->nb_rx_desc - 1) : (rx_id - 1);
		E1000_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

static void
igc_rx_queue_release_mbufs(struct igc_rx_queue *rxq)
{
	unsigned int i;

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
igc_rx_queue_release(struct igc_rx_queue *rxq)
{
	igc_rx_queue_release_mbufs(rxq);
	rte_free(rxq->sw_ring);
	rte_free(rxq);
}

void eth_igc_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	if (dev->data->rx_queues[qid])
		igc_rx_queue_release(dev->data->rx_queues[qid]);
}

uint32_t eth_igc_rx_queue_count(void *rx_queue)
{
	/**
	 * Check the DD bit of a rx descriptor of each 4 in a group,
	 * to avoid checking too frequently and downgrading performance
	 * too much.
	 */
#define IGC_RXQ_SCAN_INTERVAL 4

	volatile union e1000_adv_rx_desc *rxdp;
	struct igc_rx_queue *rxq;
	uint16_t desc = 0;

	rxq = rx_queue;
	rxdp = &rxq->rx_ring[rxq->rx_tail];

	while (desc < rxq->nb_rx_desc - rxq->rx_tail) {
		if (unlikely(!(rxdp->wb.upper.status_error &
				E1000_RXD_STAT_DD)))
			return desc;
		desc += IGC_RXQ_SCAN_INTERVAL;
		rxdp += IGC_RXQ_SCAN_INTERVAL;
	}
	rxdp = &rxq->rx_ring[rxq->rx_tail + desc - rxq->nb_rx_desc];

	while (desc < rxq->nb_rx_desc &&
		(rxdp->wb.upper.status_error & E1000_RXD_STAT_DD)) {
		desc += IGC_RXQ_SCAN_INTERVAL;
		rxdp += IGC_RXQ_SCAN_INTERVAL;
	}

	return desc;
}

int eth_igc_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct igc_rx_queue *rxq = rx_queue;
	volatile uint32_t *status;
	uint32_t desc;

	if (unlikely(!rxq || offset >= rxq->nb_rx_desc))
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

static int
igc_alloc_rx_queue_mbufs(struct igc_rx_queue *rxq)
{
	struct igc_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned int i;

	/* Initialize software ring entries. */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union e1000_adv_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_DRV_LOG(ERR, "RX mbuf alloc failed, queue_id=%hu",
				rxq->queue_id);
			return -ENOMEM;
		}
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = &rxq->rx_ring[i];
		rxd->read.hdr_addr = 0;
		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			rxd->read.pkt_addr = dma_addr - IGC_TS_HDR_LEN;
		else
			rxd->read.pkt_addr = dma_addr;
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/*
 * RSS random key supplied in section 7.1.2.9.3 of the Intel I225 datasheet.
 * Used as the default key.
 */
static uint8_t default_rss_key[40] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
};

void
igc_rss_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t mrqc;

	mrqc = E1000_READ_REG(hw, E1000_MRQC);
	mrqc &= ~E1000_MRQC_ENABLE_MASK;
	E1000_WRITE_REG(hw, E1000_MRQC, mrqc);
}

void
igc_hw_rss_hash_set(struct e1000_hw *hw, struct rte_eth_rss_conf *rss_conf)
{
	uint32_t *hash_key = (uint32_t *)rss_conf->rss_key;
	uint32_t mrqc;
	uint64_t rss_hf;

	if (hash_key != NULL) {
		uint8_t i;

		/* Fill in RSS hash key */
		for (i = 0; i < IGC_HKEY_MAX_INDEX; i++)
			E1000_WRITE_REG_LE_VALUE(hw, E1000_RSSRK(i), hash_key[i]);
	}

	/* Set configured hashing protocols in MRQC register */
	rss_hf = rss_conf->rss_hf;
	mrqc = E1000_MRQC_ENABLE_RSS_4Q; /* RSS enabled. */
	if (rss_hf & RTE_ETH_RSS_IPV4)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV4;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV4_TCP;
	if (rss_hf & RTE_ETH_RSS_IPV6)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6;
	if (rss_hf & RTE_ETH_RSS_IPV6_EX)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_EX;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_TCP;
	if (rss_hf & RTE_ETH_RSS_IPV6_TCP_EX)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_TCP_EX;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV4_UDP;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_UDP;
	if (rss_hf & RTE_ETH_RSS_IPV6_UDP_EX)
		mrqc |= E1000_MRQC_RSS_FIELD_IPV6_UDP_EX;
	E1000_WRITE_REG(hw, E1000_MRQC, mrqc);
}

static void
igc_rss_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_rss_conf rss_conf;
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint16_t i;

	/* Fill in redirection table. */
	for (i = 0; i < IGC_RSS_RDT_SIZD; i++) {
		union igc_rss_reta_reg reta;
		uint16_t q_idx, reta_idx;

		q_idx = (uint8_t)((dev->data->nb_rx_queues > 1) ?
				   i % dev->data->nb_rx_queues : 0);
		reta_idx = i % sizeof(reta);
		reta.bytes[reta_idx] = q_idx;
		if (reta_idx == sizeof(reta) - 1)
			E1000_WRITE_REG_LE_VALUE(hw,
				E1000_RETA(i / sizeof(reta)), reta.dword);
	}

	/*
	 * Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	rss_conf = dev->data->dev_conf.rx_adv_conf.rss_conf;
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = default_rss_key;
	igc_hw_rss_hash_set(hw, &rss_conf);
}

int
igc_del_rss_filter(struct rte_eth_dev *dev)
{
	struct igc_rss_filter *rss_filter = IGC_DEV_PRIVATE_RSS_FILTER(dev);

	if (rss_filter->enable) {
		/* recover default RSS configuration */
		igc_rss_configure(dev);

		/* disable RSS logic and clear filter data */
		igc_rss_disable(dev);
		memset(rss_filter, 0, sizeof(*rss_filter));
		return 0;
	}
	PMD_DRV_LOG(ERR, "filter not exist!");
	return -ENOENT;
}

/* Initiate the filter structure by the structure of rte_flow_action_rss */
void
igc_rss_conf_set(struct igc_rss_filter *out,
		const struct rte_flow_action_rss *rss)
{
	out->conf.func = rss->func;
	out->conf.level = rss->level;
	out->conf.types = rss->types;

	if (rss->key_len == sizeof(out->key)) {
		memcpy(out->key, rss->key, rss->key_len);
		out->conf.key = out->key;
		out->conf.key_len = rss->key_len;
	} else {
		out->conf.key = NULL;
		out->conf.key_len = 0;
	}

	if (rss->queue_num <= IGC_RSS_RDT_SIZD) {
		memcpy(out->queue, rss->queue,
			sizeof(*out->queue) * rss->queue_num);
		out->conf.queue = out->queue;
		out->conf.queue_num = rss->queue_num;
	} else {
		out->conf.queue = NULL;
		out->conf.queue_num = 0;
	}
}

int
igc_add_rss_filter(struct rte_eth_dev *dev, struct igc_rss_filter *rss)
{
	struct rte_eth_rss_conf rss_conf = {
		.rss_key = rss->conf.key_len ?
			(void *)(uintptr_t)rss->conf.key : NULL,
		.rss_key_len = rss->conf.key_len,
		.rss_hf = rss->conf.types,
	};
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_rss_filter *rss_filter = IGC_DEV_PRIVATE_RSS_FILTER(dev);
	uint32_t i, j;

	/* check RSS type is valid */
	if ((rss_conf.rss_hf & IGC_RSS_OFFLOAD_ALL) == 0) {
		PMD_DRV_LOG(ERR,
			"RSS type(0x%" PRIx64 ") error!, only 0x%" PRIx64
			" been supported", rss_conf.rss_hf,
			(uint64_t)IGC_RSS_OFFLOAD_ALL);
		return -EINVAL;
	}

	/* check queue count is not zero */
	if (!rss->conf.queue_num) {
		PMD_DRV_LOG(ERR, "Queue number should not be 0!");
		return -EINVAL;
	}

	/* check queue id is valid */
	for (i = 0; i < rss->conf.queue_num; i++)
		if (rss->conf.queue[i] >= dev->data->nb_rx_queues) {
			PMD_DRV_LOG(ERR, "Queue id %u is invalid!",
					rss->conf.queue[i]);
			return -EINVAL;
		}

	/* only support one filter */
	if (rss_filter->enable) {
		PMD_DRV_LOG(ERR, "Only support one RSS filter!");
		return -ENOTSUP;
	}
	rss_filter->enable = 1;

	igc_rss_conf_set(rss_filter, &rss->conf);

	/* Fill in redirection table. */
	for (i = 0, j = 0; i < IGC_RSS_RDT_SIZD; i++, j++) {
		union igc_rss_reta_reg reta;
		uint16_t q_idx, reta_idx;

		if (j == rss->conf.queue_num)
			j = 0;
		q_idx = rss->conf.queue[j];
		reta_idx = i % sizeof(reta);
		reta.bytes[reta_idx] = q_idx;
		if (reta_idx == sizeof(reta) - 1)
			E1000_WRITE_REG_LE_VALUE(hw,
				E1000_RETA(i / sizeof(reta)), reta.dword);
	}

	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = default_rss_key;
	igc_hw_rss_hash_set(hw, &rss_conf);
	return 0;
}

void
igc_clear_rss_filter(struct rte_eth_dev *dev)
{
	struct igc_rss_filter *rss_filter = IGC_DEV_PRIVATE_RSS_FILTER(dev);

	if (!rss_filter->enable)
		return;

	/* recover default RSS configuration */
	igc_rss_configure(dev);

	/* disable RSS logic and clear filter data */
	igc_rss_disable(dev);
	memset(rss_filter, 0, sizeof(*rss_filter));
}

static int
igc_dev_mq_rx_configure(struct rte_eth_dev *dev)
{
	if (RTE_ETH_DEV_SRIOV(dev).active) {
		PMD_DRV_LOG(ERR, "SRIOV unsupported!");
		return -EINVAL;
	}

	switch (dev->data->dev_conf.rxmode.mq_mode) {
	case RTE_ETH_MQ_RX_RSS:
		igc_rss_configure(dev);
		break;
	case RTE_ETH_MQ_RX_NONE:
		/*
		 * configure RSS register for following,
		 * then disable the RSS logic
		 */
		igc_rss_configure(dev);
		igc_rss_disable(dev);
		break;
	default:
		PMD_DRV_LOG(ERR, "rx mode(%d) not supported!",
			dev->data->dev_conf.rxmode.mq_mode);
		return -EINVAL;
	}
	return 0;
}

int
igc_rx_init(struct rte_eth_dev *dev)
{
	struct igc_rx_queue *rxq;
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint64_t offloads = dev->data->dev_conf.rxmode.offloads;
	uint32_t max_rx_pktlen;
	uint32_t rctl;
	uint32_t rxcsum;
	uint16_t buf_size;
	uint16_t rctl_bsize;
	uint16_t i;
	int ret;

	dev->rx_pkt_burst = igc_recv_pkts;

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring.
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	/* Configure support of jumbo frames, if any. */
	if (dev->data->mtu > RTE_ETHER_MTU)
		rctl |= E1000_RCTL_LPE;
	else
		rctl &= ~E1000_RCTL_LPE;

	max_rx_pktlen = dev->data->mtu + IGC_ETH_OVERHEAD;
	/*
	 * Set maximum packet length by default, and might be updated
	 * together with enabling/disabling dual VLAN.
	 */
	E1000_WRITE_REG(hw, E1000_RLPML, max_rx_pktlen);

	/* Configure and enable each RX queue. */
	rctl_bsize = 0;
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		uint64_t bus_addr;
		uint32_t rxdctl;
		uint32_t srrctl;

		rxq = dev->data->rx_queues[i];
		rxq->flags = 0;

		if (offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			rxq->offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;

		/* Allocate buffers for descriptor rings and set up queue */
		ret = igc_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 * call to configure
		 */
		rxq->crc_len = (offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) ?
				RTE_ETHER_CRC_LEN : 0;

		bus_addr = rxq->rx_ring_phys_addr;
		E1000_WRITE_REG(hw, E1000_RDLEN(rxq->reg_idx),
				rxq->nb_rx_desc *
				sizeof(union e1000_adv_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(rxq->reg_idx),
				(uint32_t)bus_addr);

		/* set descriptor configuration */
		srrctl = E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

		srrctl |= (uint32_t)(RTE_PKTMBUF_HEADROOM / 64) <<
				IGC_SRRCTL_BSIZEHEADER_SHIFT;
		/*
		 * Configure RX buffer size.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		if (buf_size >= 1024) {
			/*
			 * Configure the BSIZEPACKET field of the SRRCTL
			 * register of the queue.
			 * Value is in 1 KB resolution, from 1 KB to 16 KB.
			 * If this field is equal to 0b, then RCTL.BSIZE
			 * determines the RX packet buffer size.
			 */

			srrctl |= ((buf_size >> E1000_SRRCTL_BSIZEPKT_SHIFT) &
				   E1000_SRRCTL_BSIZEPKT_MASK);
			buf_size = (uint16_t)((srrctl &
					E1000_SRRCTL_BSIZEPKT_MASK) <<
					E1000_SRRCTL_BSIZEPKT_SHIFT);

			/* It adds dual VLAN length for supporting dual VLAN */
			if (max_rx_pktlen > buf_size)
				dev->data->scattered_rx = 1;
		} else {
			/*
			 * Use BSIZE field of the device RCTL register.
			 */
			if (rctl_bsize == 0 || rctl_bsize > buf_size)
				rctl_bsize = buf_size;
			dev->data->scattered_rx = 1;
		}

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= E1000_SRRCTL_DROP_EN;

		E1000_WRITE_REG(hw, E1000_SRRCTL(rxq->reg_idx), srrctl);

		/* Enable this RX queue. */
		rxdctl = E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl |= ((uint32_t)rxq->pthresh << IGC_RXDCTL_PTHRESH_SHIFT) &
				IGC_RXDCTL_PTHRESH_MSK;
		rxdctl |= ((uint32_t)rxq->hthresh << IGC_RXDCTL_HTHRESH_SHIFT) &
				IGC_RXDCTL_HTHRESH_MSK;
		rxdctl |= ((uint32_t)rxq->wthresh << IGC_RXDCTL_WTHRESH_SHIFT) &
				IGC_RXDCTL_WTHRESH_MSK;
		E1000_WRITE_REG(hw, E1000_RXDCTL(rxq->reg_idx), rxdctl);
	}

	if (offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		dev->data->scattered_rx = 1;

	if (dev->data->scattered_rx) {
		PMD_DRV_LOG(DEBUG, "forcing scatter mode");
		dev->rx_pkt_burst = igc_recv_scattered_pkts;
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
	igc_dev_mq_rx_configure(dev);

	/* Update the rctl since igc_dev_mq_rx_configure may change its value */
	rctl |= E1000_READ_REG(hw, E1000_RCTL);

	/*
	 * Setup the Checksum Register.
	 * Receive Full-Packet Checksum Offload is mutually exclusive with RSS.
	 */
	rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);
	rxcsum |= E1000_RXCSUM_PCSD;

	/* Enable both L3/L4 rx checksum offload */
	if (offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
		rxcsum |= E1000_RXCSUM_IPOFL;
	else
		rxcsum &= ~E1000_RXCSUM_IPOFL;

	if (offloads &
		(RTE_ETH_RX_OFFLOAD_TCP_CKSUM | RTE_ETH_RX_OFFLOAD_UDP_CKSUM)) {
		rxcsum |= E1000_RXCSUM_TUOFL;
		offloads |= RTE_ETH_RX_OFFLOAD_SCTP_CKSUM;
	} else {
		rxcsum &= ~E1000_RXCSUM_TUOFL;
	}

	if (offloads & RTE_ETH_RX_OFFLOAD_SCTP_CKSUM)
		rxcsum |= E1000_RXCSUM_CRCOFL;
	else
		rxcsum &= ~E1000_RXCSUM_CRCOFL;

	E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);

	/* Setup the Receive Control Register. */
	if (offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rctl &= ~E1000_RCTL_SECRC; /* Do not Strip Ethernet CRC. */
	else
		rctl |= E1000_RCTL_SECRC; /* Strip Ethernet CRC. */

	rctl &= ~E1000_RCTL_MO_MSK;
	rctl &= ~E1000_RCTL_LBM_MSK;
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
			E1000_RCTL_DPF |
			(hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	if (dev->data->dev_conf.lpbk_mode == 1)
		rctl |= E1000_RCTL_LBM_MAC;

	rctl &= ~(E1000_RCTL_HSEL_MSK | E1000_RCTL_CFIEN | E1000_RCTL_CFI |
			E1000_RCTL_PSP | E1000_RCTL_PMCF);

	/* Make sure VLAN Filters are off. */
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
		uint32_t dvmolr;

		rxq = dev->data->rx_queues[i];
		E1000_WRITE_REG(hw, E1000_RDH(rxq->reg_idx), 0);
		E1000_WRITE_REG(hw, E1000_RDT(rxq->reg_idx), rxq->nb_rx_desc - 1);

		dvmolr = E1000_READ_REG(hw, E1000_DVMOLR(rxq->reg_idx));
		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			dvmolr |= E1000_DVMOLR_STRVLAN;
		else
			dvmolr &= ~E1000_DVMOLR_STRVLAN;

		if (offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
			dvmolr &= ~E1000_DVMOLR_STRCRC;
		else
			dvmolr |= E1000_DVMOLR_STRCRC;

		E1000_WRITE_REG(hw, E1000_DVMOLR(rxq->reg_idx), dvmolr);
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return 0;
}

static void
igc_reset_rx_queue(struct igc_rx_queue *rxq)
{
	static const union e1000_adv_rx_desc zeroed_desc = { {0} };
	unsigned int i;

	/* Zero out HW ring memory */
	for (i = 0; i < rxq->nb_rx_desc; i++)
		rxq->rx_ring[i] = zeroed_desc;

	rxq->rx_tail = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

int
eth_igc_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	const struct rte_memzone *rz;
	struct igc_rx_queue *rxq;
	unsigned int size;

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of IGC_RX_DESCRIPTOR_MULTIPLE.
	 */
	if (nb_desc % IGC_RX_DESCRIPTOR_MULTIPLE != 0 ||
		nb_desc > IGC_MAX_RXD || nb_desc < IGC_MIN_RXD) {
		PMD_DRV_LOG(ERR,
			"RX descriptor must be multiple of %u(cur: %u) and between %u and %u",
			IGC_RX_DESCRIPTOR_MULTIPLE, nb_desc,
			IGC_MIN_RXD, IGC_MAX_RXD);
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		igc_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the RX queue data structure. */
	rxq = rte_zmalloc("ethdev RX queue", sizeof(struct igc_rx_queue),
			  RTE_CACHE_LINE_SIZE);
	if (rxq == NULL)
		return -ENOMEM;
	rxq->offloads = rx_conf->offloads;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->pthresh = rx_conf->rx_thresh.pthresh;
	rxq->hthresh = rx_conf->rx_thresh.hthresh;
	rxq->wthresh = rx_conf->rx_thresh.wthresh;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = queue_idx;
	rxq->port_id = dev->data->port_id;

	/*
	 *  Allocate RX ring hardware descriptors. A memzone large enough to
	 *  handle the maximum ring size is allocated in order to allow for
	 *  resizing in later calls to the queue setup function.
	 */
	size = sizeof(union e1000_adv_rx_desc) * IGC_MAX_RXD;
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx, size,
				      IGC_ALIGN, socket_id);
	if (rz == NULL) {
		igc_rx_queue_release(rxq);
		return -ENOMEM;
	}
	rxq->rdt_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_RDT(rxq->reg_idx));
	rxq->rdh_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_RDH(rxq->reg_idx));
	rxq->rx_ring_phys_addr = rz->iova;
	rxq->rx_ring = (union e1000_adv_rx_desc *)rz->addr;

	/* Allocate software ring. */
	rxq->sw_ring = rte_zmalloc("rxq->sw_ring",
				   sizeof(struct igc_rx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE);
	if (rxq->sw_ring == NULL) {
		igc_rx_queue_release(rxq);
		return -ENOMEM;
	}

	PMD_DRV_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		rxq->sw_ring, rxq->rx_ring, rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;
	igc_reset_rx_queue(rxq);

	return 0;
}

/* prepare packets for transmit */
uint16_t
eth_igc_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int i, ret;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];

		/* Check some limitations for TSO in hardware */
		if (m->ol_flags & IGC_TX_OFFLOAD_SEG)
			if (m->tso_segsz > IGC_TSO_MAX_MSS ||
				m->l2_len + m->l3_len + m->l4_len >
				IGC_TSO_MAX_HDRLEN) {
				rte_errno = EINVAL;
				return i;
			}

		if (m->ol_flags & IGC_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = ENOTSUP;
			return i;
		}

#ifdef RTE_ETHDEV_DEBUG_TX
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
	}

	return i;
}

/*
 *There're some limitations in hardware for TCP segmentation offload. We
 *should check whether the parameters are valid.
 */
static inline uint64_t
check_tso_para(uint64_t ol_req, union igc_tx_offload ol_para)
{
	if (!(ol_req & IGC_TX_OFFLOAD_SEG))
		return ol_req;
	if (ol_para.tso_segsz > IGC_TSO_MAX_MSS || ol_para.l2_len +
		ol_para.l3_len + ol_para.l4_len > IGC_TSO_MAX_HDRLEN) {
		ol_req &= ~IGC_TX_OFFLOAD_SEG;
		ol_req |= RTE_MBUF_F_TX_TCP_CKSUM;
	}
	return ol_req;
}

/*
 * Check which hardware context can be used. Use the existing match
 * or create a new context descriptor.
 */
static inline uint32_t
what_advctx_update(struct igc_tx_queue *txq, uint64_t flags,
		union igc_tx_offload tx_offload)
{
	uint32_t curr = txq->ctx_curr;

	/* If match with the current context */
	if (likely(txq->ctx_cache[curr].flags == flags &&
		txq->ctx_cache[curr].tx_offload.data ==
		(txq->ctx_cache[curr].tx_offload_mask.data &
		tx_offload.data))) {
		return curr;
	}

	/* Total two context, if match with the second context */
	curr ^= 1;
	if (likely(txq->ctx_cache[curr].flags == flags &&
		txq->ctx_cache[curr].tx_offload.data ==
		(txq->ctx_cache[curr].tx_offload_mask.data &
		tx_offload.data))) {
		txq->ctx_curr = curr;
		return curr;
	}

	/* Mismatch, create new one */
	return IGC_CTX_NUM;
}

static uint32_t igc_tx_launchtime(uint64_t txtime, uint16_t port_id)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	uint64_t base_time = adapter->base_time;
	uint64_t cycle_time = adapter->cycle_time;
	uint32_t launchtime;

	launchtime = (txtime - base_time) % cycle_time;

	return rte_cpu_to_le_32(launchtime);
}

/*
 * This is a separate function, looking for optimization opportunity here
 * Rework required to go with the pre-defined values.
 */
static inline void
igc_set_xmit_ctx(struct igc_tx_queue *txq,
		volatile struct e1000_adv_tx_context_desc *ctx_txd,
		uint64_t ol_flags, union igc_tx_offload tx_offload,
		uint64_t txtime)
{
	uint32_t type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
	uint32_t ctx_curr;
	uint32_t vlan_macip_lens;
	union igc_tx_offload tx_offload_mask;

	/* Use the previous context */
	txq->ctx_curr ^= 1;
	ctx_curr = txq->ctx_curr;

	tx_offload_mask.data = 0;
	type_tucmd_mlhl = 0;

	/* Specify which HW CTX to upload. */
	mss_l4len_idx = (ctx_curr << E1000_ADVTXD_IDX_SHIFT);

	if (ol_flags & RTE_MBUF_F_TX_VLAN)
		tx_offload_mask.vlan_tci = 0xffff;

	/* check if TCP segmentation required for this packet */
	if (ol_flags & IGC_TX_OFFLOAD_SEG) {
		/* implies IP cksum in IPv4 */
		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			type_tucmd_mlhl = E1000_ADVTXD_TUCMD_IPV4 |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
		else
			type_tucmd_mlhl = E1000_ADVTXD_TUCMD_IPV6 |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;

		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_TCP;
		else
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_UDP;

		tx_offload_mask.data |= TX_TSO_CMP_MASK;
		mss_l4len_idx |= (uint32_t)tx_offload.tso_segsz <<
				E1000_ADVTXD_MSS_SHIFT;
		mss_l4len_idx |= (uint32_t)tx_offload.l4_len <<
				E1000_ADVTXD_L4LEN_SHIFT;
	} else { /* no TSO, check if hardware checksum is needed */
		if (ol_flags & (RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_L4_MASK))
			tx_offload_mask.data |= TX_MACIP_LEN_CMP_MASK;

		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			type_tucmd_mlhl = E1000_ADVTXD_TUCMD_IPV4;

		switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
		case RTE_MBUF_F_TX_TCP_CKSUM:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_TCP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			mss_l4len_idx |= (uint32_t)sizeof(struct rte_tcp_hdr)
				<< E1000_ADVTXD_L4LEN_SHIFT;
			break;
		case RTE_MBUF_F_TX_UDP_CKSUM:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_UDP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			mss_l4len_idx |= (uint32_t)sizeof(struct rte_udp_hdr)
				<< E1000_ADVTXD_L4LEN_SHIFT;
			break;
		case RTE_MBUF_F_TX_SCTP_CKSUM:
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_SCTP |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			mss_l4len_idx |= (uint32_t)sizeof(struct rte_sctp_hdr)
				<< E1000_ADVTXD_L4LEN_SHIFT;
			break;
		default:
			type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_L4T_RSV |
				E1000_ADVTXD_DTYP_CTXT | E1000_ADVTXD_DCMD_DEXT;
			break;
		}
	}

	if (!txtime) {
		txq->ctx_cache[ctx_curr].flags = ol_flags;
		txq->ctx_cache[ctx_curr].tx_offload.data =
			tx_offload_mask.data & tx_offload.data;
		txq->ctx_cache[ctx_curr].tx_offload_mask = tx_offload_mask;
	}

	ctx_txd->type_tucmd_mlhl = rte_cpu_to_le_32(type_tucmd_mlhl);
	vlan_macip_lens = (uint32_t)tx_offload.data;
	ctx_txd->vlan_macip_lens = rte_cpu_to_le_32(vlan_macip_lens);
	ctx_txd->mss_l4len_idx = rte_cpu_to_le_32(mss_l4len_idx);

	if (txtime)
		ctx_txd->u.launch_time = igc_tx_launchtime(txtime,
							   txq->port_id);
	else
		ctx_txd->u.launch_time = 0;
}

static inline uint32_t
tx_desc_vlan_flags_to_cmdtype(uint64_t ol_flags)
{
	uint32_t cmdtype;
	static uint32_t vlan_cmd[2] = {0, E1000_ADVTXD_DCMD_VLE};
	static uint32_t tso_cmd[2] = {0, E1000_ADVTXD_DCMD_TSE};
	cmdtype = vlan_cmd[(ol_flags & RTE_MBUF_F_TX_VLAN) != 0];
	cmdtype |= tso_cmd[(ol_flags & IGC_TX_OFFLOAD_SEG) != 0];
	return cmdtype;
}

static inline uint32_t
tx_desc_cksum_flags_to_olinfo(uint64_t ol_flags)
{
	static const uint32_t l4_olinfo[2] = {0, IGC_ADVTXD_POPTS_TXSM};
	static const uint32_t l3_olinfo[2] = {0, IGC_ADVTXD_POPTS_IXSM};
	uint32_t tmp;

	tmp  = l4_olinfo[(ol_flags & RTE_MBUF_F_TX_L4_MASK)  != RTE_MBUF_F_TX_L4_NO_CKSUM];
	tmp |= l3_olinfo[(ol_flags & RTE_MBUF_F_TX_IP_CKSUM) != 0];
	tmp |= l4_olinfo[(ol_flags & IGC_TX_OFFLOAD_SEG) != 0];
	return tmp;
}

uint16_t
igc_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct igc_tx_queue * const txq = tx_queue;
	struct igc_tx_entry * const sw_ring = txq->sw_ring;
	struct igc_tx_entry *txe, *txn;
	volatile union e1000_adv_tx_desc * const txr = txq->tx_ring;
	volatile union e1000_adv_tx_desc *txd;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
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
	union igc_tx_offload tx_offload = {0};
	uint64_t ts;

	tx_id = txq->tx_tail;
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
		tx_last = (uint16_t)(tx_id + tx_pkt->nb_segs - 1);

		ol_flags = tx_pkt->ol_flags;
		tx_ol_req = ol_flags & IGC_TX_OFFLOAD_MASK;

		/* If a Context Descriptor need be built . */
		if (tx_ol_req) {
			tx_offload.l2_len = tx_pkt->l2_len;
			tx_offload.l3_len = tx_pkt->l3_len;
			tx_offload.l4_len = tx_pkt->l4_len;
			tx_offload.vlan_tci = tx_pkt->vlan_tci;
			tx_offload.tso_segsz = tx_pkt->tso_segsz;
			tx_ol_req = check_tso_para(tx_ol_req, tx_offload);

			new_ctx = what_advctx_update(txq, tx_ol_req,
					tx_offload);
			/* Only allocate context descriptor if required*/
			new_ctx = (new_ctx >= IGC_CTX_NUM);
			tx_last = (uint16_t)(tx_last + new_ctx);
		}
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		PMD_TX_LOG(DEBUG,
			"port_id=%u queue_id=%u pktlen=%u tx_first=%u tx_last=%u",
			txq->port_id, txq->queue_id, pkt_len, tx_id, tx_last);

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
		if (!(txr[tx_end].wb.status & E1000_TXD_STAT_DD)) {
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
		if (tx_ol_req & IGC_TX_OFFLOAD_SEG)
			pkt_len -= (tx_pkt->l2_len + tx_pkt->l3_len +
					tx_pkt->l4_len);
		olinfo_status = (pkt_len << E1000_ADVTXD_PAYLEN_SHIFT);

		/*
		 * Timer 0 should be used to for packet timestamping,
		 * sample the packet timestamp to reg 0
		 */
		if (ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
			cmd_type_len |= E1000_ADVTXD_MAC_TSTAMP;

		if (tx_ol_req) {
			/* Setup TX Advanced context descriptor if required */
			if (new_ctx) {
				volatile struct e1000_adv_tx_context_desc *
					ctx_txd = (volatile struct
					e1000_adv_tx_context_desc *)&txr[tx_id];

				txn = &sw_ring[txe->next_id];
				RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);

				if (txe->mbuf != NULL) {
					rte_pktmbuf_free_seg(txe->mbuf);
					txe->mbuf = NULL;
				}

				if (igc_tx_timestamp_dynflag > 0) {
					ts = *RTE_MBUF_DYNFIELD(tx_pkt,
						igc_tx_timestamp_dynfield_offset,
						uint64_t *);
					igc_set_xmit_ctx(txq, ctx_txd,
						tx_ol_req, tx_offload, ts);
				} else {
					igc_set_xmit_ctx(txq, ctx_txd,
						tx_ol_req, tx_offload, 0);
				}

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
			}

			/* Setup the TX Advanced Data Descriptor */
			cmd_type_len |=
				tx_desc_vlan_flags_to_cmdtype(tx_ol_req);
			olinfo_status |=
				tx_desc_cksum_flags_to_olinfo(tx_ol_req);
			olinfo_status |= (uint32_t)txq->ctx_curr <<
					E1000_ADVTXD_IDX_SHIFT;
		}

		m_seg = tx_pkt;
		do {
			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);

			txd = &txr[tx_id];

			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Set up transmit descriptor */
			slen = (uint16_t)m_seg->data_len;
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
		txq->port_id, txq->queue_id, tx_id, nb_tx);
	txq->tx_tail = tx_id;

	return nb_tx;
}

int eth_igc_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct igc_tx_queue *txq = tx_queue;
	volatile uint32_t *status;
	uint32_t desc;

	if (unlikely(!txq || offset >= txq->nb_tx_desc))
		return -EINVAL;

	desc = txq->tx_tail + offset;
	if (desc >= txq->nb_tx_desc)
		desc -= txq->nb_tx_desc;

	status = &txq->tx_ring[desc].wb.status;
	if (*status & rte_cpu_to_le_32(E1000_TXD_STAT_DD))
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

static void
igc_tx_queue_release_mbufs(struct igc_tx_queue *txq)
{
	unsigned int i;

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
igc_tx_queue_release(struct igc_tx_queue *txq)
{
	igc_tx_queue_release_mbufs(txq);
	rte_free(txq->sw_ring);
	rte_free(txq);
}

void eth_igc_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	if (dev->data->tx_queues[qid])
		igc_tx_queue_release(dev->data->tx_queues[qid]);
}

static void
igc_reset_tx_queue_stat(struct igc_tx_queue *txq)
{
	txq->tx_head = 0;
	txq->tx_tail = 0;
	txq->ctx_curr = 0;
	memset((void *)&txq->ctx_cache, 0,
		IGC_CTX_NUM * sizeof(struct igc_advctx_info));
}

static void
igc_reset_tx_queue(struct igc_tx_queue *txq)
{
	struct igc_tx_entry *txe = txq->sw_ring;
	uint16_t i, prev;

	/* Initialize ring entries */
	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile union e1000_adv_tx_desc *txd = &txq->tx_ring[i];

		txd->wb.status = E1000_TXD_STAT_DD;
		txe[i].mbuf = NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->txd_type = E1000_ADVTXD_DTYP_DATA;
	igc_reset_tx_queue_stat(txq);
}

/*
 * clear all rx/tx queue
 */
void
igc_dev_clear_queues(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct igc_tx_queue *txq;
	struct igc_rx_queue *rxq;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq != NULL) {
			igc_tx_queue_release_mbufs(txq);
			igc_reset_tx_queue(txq);
			dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq != NULL) {
			igc_rx_queue_release_mbufs(rxq);
			igc_reset_rx_queue(rxq);
			dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
		}
	}
}

int eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct igc_tx_queue *txq;
	struct e1000_hw *hw;
	uint32_t size;

	if (nb_desc % IGC_TX_DESCRIPTOR_MULTIPLE != 0 ||
		nb_desc > IGC_MAX_TXD || nb_desc < IGC_MIN_TXD) {
		PMD_DRV_LOG(ERR,
			"TX-descriptor must be a multiple of %u and between %u and %u, cur: %u",
			IGC_TX_DESCRIPTOR_MULTIPLE,
			IGC_MAX_TXD, IGC_MIN_TXD, nb_desc);
		return -EINVAL;
	}

	hw = IGC_DEV_PRIVATE_HW(dev);

	/*
	 * The tx_free_thresh and tx_rs_thresh values are not used in the 2.5G
	 * driver.
	 */
	if (tx_conf->tx_free_thresh != 0)
		PMD_DRV_LOG(INFO,
			"The tx_free_thresh parameter is not used for the 2.5G driver");
	if (tx_conf->tx_rs_thresh != 0)
		PMD_DRV_LOG(INFO,
			"The tx_rs_thresh parameter is not used for the 2.5G driver");
	if (tx_conf->tx_thresh.wthresh == 0)
		PMD_DRV_LOG(INFO,
			"To improve 2.5G driver performance, consider setting the TX WTHRESH value to 4, 8, or 16.");

	/* Free memory prior to re-allocation if needed */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		igc_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc("ethdev TX queue", sizeof(struct igc_tx_queue),
						RTE_CACHE_LINE_SIZE);
	if (txq == NULL)
		return -ENOMEM;

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	size = sizeof(union e1000_adv_tx_desc) * IGC_MAX_TXD;
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, size,
				      IGC_ALIGN, socket_id);
	if (tz == NULL) {
		igc_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;

	txq->queue_id = queue_idx;
	txq->reg_idx = queue_idx;
	txq->port_id = dev->data->port_id;

	txq->tdt_reg_addr = E1000_PCI_REG_ADDR(hw, E1000_TDT(txq->reg_idx));
	txq->tx_ring_phys_addr = tz->iova;

	txq->tx_ring = (union e1000_adv_tx_desc *)tz->addr;
	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc("txq->sw_ring",
				   sizeof(struct igc_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE);
	if (txq->sw_ring == NULL) {
		igc_tx_queue_release(txq);
		return -ENOMEM;
	}
	PMD_DRV_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	igc_reset_tx_queue(txq);
	dev->tx_pkt_burst = igc_xmit_pkts;
	dev->tx_pkt_prepare = &eth_igc_prep_pkts;
	dev->data->tx_queues[queue_idx] = txq;
	txq->offloads = tx_conf->offloads;

	return 0;
}

int
eth_igc_tx_done_cleanup(void *txqueue, uint32_t free_cnt)
{
	struct igc_tx_queue *txq = txqueue;
	struct igc_tx_entry *sw_ring;
	volatile union e1000_adv_tx_desc *txr;
	uint16_t tx_first; /* First segment analyzed. */
	uint16_t tx_id;    /* Current segment being processed. */
	uint16_t tx_last;  /* Last segment in the current packet. */
	uint16_t tx_next;  /* First segment of the next packet. */
	uint32_t count;

	if (txq == NULL)
		return -ENODEV;

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
			if (!(txr[tx_last].wb.status &
					rte_cpu_to_le_32(E1000_TXD_STAT_DD)))
				break;

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

				/* Move to next segment. */
				tx_id = sw_ring[tx_id].next_id;
			} while (tx_id != tx_next);

			/*
			 * Increment the number of packets
			 * freed.
			 */
			count++;
			if (unlikely(count == free_cnt))
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
			if (likely(tx_id == tx_first && count != 0))
				break;

			/*
			 * Walk the list and find the next mbuf, if any.
			 */
			do {
				/* Move to next segment. */
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

	return count;
}

void
igc_tx_init(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint64_t offloads = dev->data->dev_conf.txmode.offloads;
	uint32_t tctl;
	uint32_t txdctl;
	uint16_t i;
	int err;

	/* Setup the Base and Length of the Tx Descriptor Rings. */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct igc_tx_queue *txq = dev->data->tx_queues[i];
		uint64_t bus_addr = txq->tx_ring_phys_addr;

		E1000_WRITE_REG(hw, E1000_TDLEN(txq->reg_idx),
				txq->nb_tx_desc *
				sizeof(union e1000_adv_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(txq->reg_idx),
				(uint32_t)bus_addr);

		/* Setup the HW Tx Head and Tail descriptor pointers. */
		E1000_WRITE_REG(hw, E1000_TDT(txq->reg_idx), 0);
		E1000_WRITE_REG(hw, E1000_TDH(txq->reg_idx), 0);

		/* Setup Transmit threshold registers. */
		txdctl = ((uint32_t)txq->pthresh << IGC_TXDCTL_PTHRESH_SHIFT) &
				IGC_TXDCTL_PTHRESH_MSK;
		txdctl |= ((uint32_t)txq->hthresh << IGC_TXDCTL_HTHRESH_SHIFT) &
				IGC_TXDCTL_HTHRESH_MSK;
		txdctl |= ((uint32_t)txq->wthresh << IGC_TXDCTL_WTHRESH_SHIFT) &
				IGC_TXDCTL_WTHRESH_MSK;
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(hw, E1000_TXDCTL(txq->reg_idx), txdctl);
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	if (offloads & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP) {
		err = rte_mbuf_dyn_tx_timestamp_register
			(&igc_tx_timestamp_dynfield_offset,
			 &igc_tx_timestamp_dynflag);
		if (err) {
			PMD_DRV_LOG(ERR,
				"Cannot register mbuf field/flag for timestamp");
		}
	}

	e1000_config_collision_dist(hw);

	/* Program the Transmit Control Register. */
	tctl = E1000_READ_REG(hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		 ((uint32_t)E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(hw, E1000_TCTL, tctl);
}

void
eth_igc_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct igc_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = rxq->drop_en;
	qinfo->conf.offloads = rxq->offloads;
	qinfo->conf.rx_thresh.hthresh = rxq->hthresh;
	qinfo->conf.rx_thresh.pthresh = rxq->pthresh;
	qinfo->conf.rx_thresh.wthresh = rxq->wthresh;
}

void
eth_igc_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct igc_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;
	qinfo->conf.offloads = txq->offloads;
}

void
eth_igc_vlan_strip_queue_set(struct rte_eth_dev *dev,
			uint16_t rx_queue_id, int on)
{
	struct e1000_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_rx_queue *rxq = dev->data->rx_queues[rx_queue_id];
	uint32_t reg_val;

	if (rx_queue_id >= IGC_QUEUE_PAIRS_NUM) {
		PMD_DRV_LOG(ERR, "Queue index(%u) illegal, max is %u",
			rx_queue_id, IGC_QUEUE_PAIRS_NUM - 1);
		return;
	}

	reg_val = E1000_READ_REG(hw, E1000_DVMOLR(rx_queue_id));
	if (on) {
		reg_val |= E1000_DVMOLR_STRVLAN;
		rxq->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	} else {
		reg_val &= ~(E1000_DVMOLR_STRVLAN | E1000_DVMOLR_HIDVLAN);
		rxq->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	}

	E1000_WRITE_REG(hw, E1000_DVMOLR(rx_queue_id), reg_val);
}
