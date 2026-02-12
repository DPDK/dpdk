/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 NXP
 */

#include <rte_mbuf.h>
#include <rte_io.h>
#include <ethdev_driver.h>
#include "enet_regs.h"
#include "enet_ethdev.h"
#include "enet_pmd_logs.h"

/* This function does enetfec_rx_queue processing. Dequeue packet from Rx queue
 * When update through the ring, just set the empty indicator.
 */
uint16_t
enetfec_recv_pkts(void *rxq1, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct rte_mempool *pool;
	struct rte_mbuf *mbuf, *new_mbuf = NULL;
	unsigned short status;
	unsigned short pkt_len;
	int pkt_received = 0, index = 0;
	struct rte_ether_hdr *eth;
	void *data, *mbuf_data;
	struct bufdesc *bdp;
	uint16_t vlan_tag;
	struct  bufdesc_ex *ebdp = NULL;
	bool    vlan_packet_rcvd = false;
	struct enetfec_priv_rx_q *rxq  = (struct enetfec_priv_rx_q *)rxq1;
	struct rte_eth_stats *stats = &rxq->fep->stats;
	struct rte_eth_conf *eth_conf = &rxq->fep->dev->data->dev_conf;
	uint64_t rx_offloads = eth_conf->rxmode.offloads;
	pool = rxq->pool;
	bdp = rxq->bd.cur;

	/* Process the incoming packet */
	status = rte_le_to_cpu_16(rte_read16(&bdp->bd_sc));
	while ((status & RX_BD_EMPTY) == 0) {
		if (pkt_received >= nb_pkts)
			break;

		/* Check for errors. */
		status ^= RX_BD_LAST;
		if (status & (RX_BD_LG | RX_BD_SH | RX_BD_NO |
			RX_BD_CR | RX_BD_OV | RX_BD_LAST |
			RX_BD_TR)) {
			stats->ierrors++;
			if (status & RX_BD_OV) {
				/* FIFO overrun */
				/* enet_dump_rx(rxq); */
				ENETFEC_DP_LOG(DEBUG, "rx_fifo_error");
				goto rx_processing_done;
			}
			if (status & (RX_BD_LG | RX_BD_SH
						| RX_BD_LAST)) {
				/* Frame too long or too short. */
				ENETFEC_DP_LOG(DEBUG, "rx_length_error");
				if (status & RX_BD_LAST)
					ENETFEC_DP_LOG(DEBUG, "rcv is not +last");
			}
			if (status & RX_BD_CR) {     /* CRC Error */
				ENETFEC_DP_LOG(DEBUG, "rx_crc_errors");
			}
			/* Report late collisions as a frame error. */
			if (status & (RX_BD_NO | RX_BD_TR))
				ENETFEC_DP_LOG(DEBUG, "rx_frame_error");
			goto rx_processing_done;
		}

		new_mbuf = rte_pktmbuf_alloc(pool);
		if (unlikely(new_mbuf == NULL)) {
			stats->rx_nombuf++;
			break;
		}

		/* Process the incoming frame. */
		stats->ipackets++;
		pkt_len = rte_le_to_cpu_16(rte_read16(&bdp->bd_datlen));
		stats->ibytes += pkt_len;

		/* shows data with respect to the data_off field. */
		index = enet_get_bd_index(bdp, &rxq->bd);
		mbuf = rxq->rx_mbuf[index];

		data = rte_pktmbuf_mtod(mbuf, uint8_t *);
		mbuf_data = data;
		rte_prefetch0(data);
		rte_pktmbuf_append((struct rte_mbuf *)mbuf,
				pkt_len - 4);

		if (rxq->fep->quirks & QUIRK_RACC)
			data = rte_pktmbuf_adj(mbuf, 2);

		rx_pkts[pkt_received] = mbuf;

		/* Assuming Ethernet packets, doing software packet type parsing.
		 * To be replaced by HW packet parsing
		 */
		eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
		mbuf->packet_type = RTE_PTYPE_L2_ETHER;
		if (rte_be_to_cpu_16(eth->ether_type) == RTE_ETHER_TYPE_IPV4)
			mbuf->packet_type |= RTE_PTYPE_L3_IPV4;
		if (rte_be_to_cpu_16(eth->ether_type) == RTE_ETHER_TYPE_IPV6)
			mbuf->packet_type |= RTE_PTYPE_L3_IPV6;
		pkt_received++;

		/* Extract the enhanced buffer descriptor */
		ebdp = NULL;
		if (rxq->fep->bufdesc_ex)
			ebdp = (struct bufdesc_ex *)bdp;

		/* If this is a VLAN packet remove the VLAN Tag */
		vlan_packet_rcvd = false;
		if ((rx_offloads & RTE_ETH_RX_OFFLOAD_VLAN) &&
				rxq->fep->bufdesc_ex &&
				(rte_read32(&ebdp->bd_esc) &
				rte_cpu_to_le_32(BD_ENETFEC_RX_VLAN))) {
			/* Push and remove the vlan tag */
			struct rte_vlan_hdr *vlan_header =
				(struct rte_vlan_hdr *)
				((uint8_t *)data + ETH_HLEN);
			vlan_tag = rte_be_to_cpu_16(vlan_header->vlan_tci);

			vlan_packet_rcvd = true;
			memmove((uint8_t *)mbuf_data + RTE_VLAN_HLEN,
				data, RTE_ETHER_ADDR_LEN * 2);
			rte_pktmbuf_adj(mbuf, RTE_VLAN_HLEN);
		}

		if (rxq->fep->bufdesc_ex &&
			(rxq->fep->flag_csum & RX_FLAG_CSUM_EN)) {
			if ((rte_read32(&ebdp->bd_esc) &
				rte_cpu_to_le_32(RX_FLAG_CSUM_ERR)) == 0) {
				/* No checksum error - checksum is good */
				mbuf->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			} else {
				/* Checksum error detected */
				mbuf->ol_flags = RTE_MBUF_F_RX_IP_CKSUM_BAD;
			}
		}

		/* Handle received VLAN packets */
		if (vlan_packet_rcvd) {
			mbuf->vlan_tci = vlan_tag;
			mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN_STRIPPED
						| RTE_MBUF_F_RX_VLAN;
		}

		rxq->rx_mbuf[index] = new_mbuf;
		rte_write32(rte_cpu_to_le_32(rte_pktmbuf_iova(new_mbuf)),
				&bdp->bd_bufaddr);
rx_processing_done:
		/* when rx_processing_done clear the status flags
		 * for this buffer
		 */
		status &= ~RX_BD_STATS;

		/* Mark the buffer empty */
		status |= RX_BD_EMPTY;

		if (rxq->fep->bufdesc_ex) {
			struct bufdesc_ex *ebdp = (struct bufdesc_ex *)bdp;
			rte_write32(rte_cpu_to_le_32(RX_BD_INT),
				    &ebdp->bd_esc);
			rte_write32(0, &ebdp->bd_prot);
			rte_write32(0, &ebdp->bd_bdu);
		}

		/* Make sure the updates to rest of the descriptor are
		 * performed before transferring ownership.
		 */
		rte_wmb();
		rte_write16(rte_cpu_to_le_16(status), &bdp->bd_sc);

		/* Update BD pointer to next entry */
		bdp = enet_get_nextdesc(bdp, &rxq->bd);

		/* Doing this here will keep the FEC running while we process
		 * incoming frames.
		 */
		rte_write32(0, rxq->bd.active_reg_desc);
		status = rte_le_to_cpu_16(rte_read16(&bdp->bd_sc));
	}
	rxq->bd.cur = bdp;
	return pkt_received;
}

uint16_t
enetfec_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct enetfec_priv_tx_q *txq  =
			(struct enetfec_priv_tx_q *)tx_queue;
	struct rte_eth_stats *stats = &txq->fep->stats;
	struct bufdesc *bdp, *last_bdp;
	struct rte_mbuf *mbuf;
	unsigned short status;
	unsigned short buflen;
	unsigned int index, estatus = 0;
	unsigned int i, pkt_transmitted = 0;
	uint8_t *data;
	int tx_st = 1;

	while (tx_st) {
		if (pkt_transmitted >= nb_pkts) {
			tx_st = 0;
			break;
		}

		mbuf = *(tx_pkts);
		if (mbuf->nb_segs > 1) {
			ENETFEC_DP_LOG(DEBUG, "SG not supported");
			return pkt_transmitted;
		}

		tx_pkts++;
		bdp = txq->bd.cur;

		/* First clean the ring */
		index = enet_get_bd_index(bdp, &txq->bd);
		status = rte_le_to_cpu_16(rte_read16(&bdp->bd_sc));

		if (status & TX_BD_READY) {
			stats->oerrors++;
			break;
		}
		if (txq->tx_mbuf[index]) {
			rte_pktmbuf_free(txq->tx_mbuf[index]);
			txq->tx_mbuf[index] = NULL;
		}

		/* Fill in a Tx ring entry */
		last_bdp = bdp;
		status &= ~TX_BD_STATS;

		/* Set buffer length and buffer pointer */
		buflen = rte_pktmbuf_pkt_len(mbuf);
		stats->opackets++;
		stats->obytes += buflen;

		status |= (TX_BD_LAST);
		data = rte_pktmbuf_mtod(mbuf, void *);
		for (i = 0; i <= buflen; i += RTE_CACHE_LINE_SIZE)
			dccivac(data + i);

		rte_write32(rte_cpu_to_le_32(rte_pktmbuf_iova(mbuf)),
			    &bdp->bd_bufaddr);
		rte_write16(rte_cpu_to_le_16(buflen), &bdp->bd_datlen);

		if (txq->fep->bufdesc_ex) {
			struct bufdesc_ex *ebdp = (struct bufdesc_ex *)bdp;

			if (mbuf->ol_flags & (RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_TCP_CKSUM |
					RTE_MBUF_F_TX_UDP_CKSUM | RTE_MBUF_F_TX_SCTP_CKSUM))
				estatus |= TX_BD_PINS | TX_BD_IINS;

			rte_write32(0, &ebdp->bd_bdu);
			rte_write32(rte_cpu_to_le_32(estatus),
				    &ebdp->bd_esc);
		}

		index = enet_get_bd_index(last_bdp, &txq->bd);
		/* Save mbuf pointer */
		txq->tx_mbuf[index] = mbuf;

		/* Make sure the updates to rest of the descriptor are performed
		 * before transferring ownership.
		 */
		status |= (TX_BD_READY | TX_BD_TC);
		rte_wmb();
		rte_write16(rte_cpu_to_le_16(status), &bdp->bd_sc);

		/* Trigger transmission start */
		rte_write32(0, txq->bd.active_reg_desc);
		pkt_transmitted++;

		/* If this was the last BD in the ring, start at the
		 * beginning again.
		 */
		bdp = enet_get_nextdesc(last_bdp, &txq->bd);

		/* Make sure the update to bdp and tx_skbuff are performed
		 * before txq->bd.cur.
		 */
		txq->bd.cur = bdp;
	}
	return pkt_transmitted;
}
