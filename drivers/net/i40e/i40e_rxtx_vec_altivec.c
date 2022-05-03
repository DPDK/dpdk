/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010 - 2015 Intel Corporation
 * Copyright(c) 2017 IBM Corporation.
 */

#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "base/i40e_prototype.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_rxtx_vec_common.h"

#include <rte_altivec.h>

#pragma GCC diagnostic ignored "-Wcast-qual"

static inline void
i40e_rxq_rearm(struct i40e_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union i40e_rx_desc *rxdp;

	struct i40e_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;

	__vector unsigned long hdr_room = (__vector unsigned long){
						RTE_PKTMBUF_HEADROOM,
						RTE_PKTMBUF_HEADROOM};
	__vector unsigned long dma_addr0, dma_addr1;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxep,
				 RTE_I40E_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + RTE_I40E_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			dma_addr0 = (__vector unsigned long){};
			for (i = 0; i < RTE_I40E_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				vec_st(dma_addr0, 0,
				       (__vector unsigned long *)&rxdp[i].read);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_I40E_RXQ_REARM_THRESH;
		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_I40E_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		__vector unsigned long vaddr0, vaddr1;
		uintptr_t p0, p1;

		mb0 = rxep[0].mbuf;
		mb1 = rxep[1].mbuf;

		 /* Flush mbuf with pkt template.
		  * Data to be rearmed is 6 bytes long.
		  * Though, RX will overwrite ol_flags that are coming next
		  * anyway. So overwrite whole 8 bytes with one load:
		  * 6 bytes of rearm_data plus first 2 bytes of ol_flags.
		  */
		p0 = (uintptr_t)&mb0->rearm_data;
		*(uint64_t *)p0 = rxq->mbuf_initializer;
		p1 = (uintptr_t)&mb1->rearm_data;
		*(uint64_t *)p1 = rxq->mbuf_initializer;

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		vaddr0 = vec_ld(0, (__vector unsigned long *)&mb0->buf_addr);
		vaddr1 = vec_ld(0, (__vector unsigned long *)&mb1->buf_addr);

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = vec_mergel(vaddr0, vaddr0);
		dma_addr1 = vec_mergel(vaddr1, vaddr1);

		/* add headroom to pa values */
		dma_addr0 = vec_add(dma_addr0, hdr_room);
		dma_addr1 = vec_add(dma_addr1, hdr_room);

		/* flush desc with pa dma_addr */
		vec_st(dma_addr0, 0, (__vector unsigned long *)&rxdp++->read);
		vec_st(dma_addr1, 0, (__vector unsigned long *)&rxdp++->read);
	}

	rxq->rxrearm_start += RTE_I40E_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= RTE_I40E_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
}

static inline void
desc_to_olflags_v(__vector unsigned long descs[4], struct rte_mbuf **rx_pkts)
{
	__vector unsigned int vlan0, vlan1, rss, l3_l4e;

	/* mask everything except RSS, flow director and VLAN flags
	 * bit2 is for VLAN tag, bit11 for flow director indication
	 * bit13:12 for RSS indication.
	 */
	const __vector unsigned int rss_vlan_msk = (__vector unsigned int){
			(int32_t)0x1c03804, (int32_t)0x1c03804,
			(int32_t)0x1c03804, (int32_t)0x1c03804};

	/* map rss and vlan type to rss hash and vlan flag */
	const __vector unsigned char vlan_flags = (__vector unsigned char){
			0, 0, 0, 0,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0};

	const __vector unsigned char rss_flags = (__vector unsigned char){
			0, RTE_MBUF_F_RX_FDIR, 0, 0,
			0, 0, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR,
			0, 0, 0, 0,
			0, 0, 0, 0};

	const __vector unsigned char l3_l4e_flags = (__vector unsigned char){
			0,
			RTE_MBUF_F_RX_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_L4_CKSUM_BAD,
			RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD,
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD,
			RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD
					     | RTE_MBUF_F_RX_IP_CKSUM_BAD,
			0, 0, 0, 0, 0, 0, 0, 0};

	vlan0 = (__vector unsigned int)vec_mergel(descs[0], descs[1]);
	vlan1 = (__vector unsigned int)vec_mergel(descs[2], descs[3]);
	vlan0 = (__vector unsigned int)vec_mergeh(vlan0, vlan1);

	vlan1 = vec_and(vlan0, rss_vlan_msk);
	vlan0 = (__vector unsigned int)vec_perm(vlan_flags,
				(__vector unsigned char){},
				*(__vector unsigned char *)&vlan1);

	rss = vec_sr(vlan1, (__vector unsigned int){11, 11, 11, 11});
	rss = (__vector unsigned int)vec_perm(rss_flags, (__vector unsigned char){},
				*(__vector unsigned char *)&rss);

	l3_l4e = vec_sr(vlan1, (__vector unsigned int){22, 22, 22, 22});
	l3_l4e = (__vector unsigned int)vec_perm(l3_l4e_flags,
				(__vector unsigned char){},
				*(__vector unsigned char *)&l3_l4e);

	vlan0 = vec_or(vlan0, rss);
	vlan0 = vec_or(vlan0, l3_l4e);

	rx_pkts[0]->ol_flags = (uint64_t)vlan0[2];
	rx_pkts[1]->ol_flags = (uint64_t)vlan0[3];
	rx_pkts[2]->ol_flags = (uint64_t)vlan0[0];
	rx_pkts[3]->ol_flags = (uint64_t)vlan0[1];
}

#define PKTLEN_SHIFT     10

static inline void
desc_to_ptype_v(__vector unsigned long descs[4], struct rte_mbuf **rx_pkts,
		uint32_t *ptype_tbl)
{
	__vector unsigned long ptype0 = vec_mergel(descs[0], descs[1]);
	__vector unsigned long ptype1 = vec_mergel(descs[2], descs[3]);

	ptype0 = vec_sr(ptype0, (__vector unsigned long){30, 30});
	ptype1 = vec_sr(ptype1, (__vector unsigned long){30, 30});

	rx_pkts[0]->packet_type =
		ptype_tbl[(*(__vector unsigned char *)&ptype0)[0]];
	rx_pkts[1]->packet_type =
		ptype_tbl[(*(__vector unsigned char *)&ptype0)[8]];
	rx_pkts[2]->packet_type =
		ptype_tbl[(*(__vector unsigned char *)&ptype1)[0]];
	rx_pkts[3]->packet_type =
		ptype_tbl[(*(__vector unsigned char *)&ptype1)[8]];
}

/**
 * vPMD raw receive routine, only accept(nb_pkts >= RTE_I40E_DESCS_PER_LOOP)
 *
 * Notice:
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 * - floor align nb_pkts to a RTE_I40E_DESCS_PER_LOOP power-of-two
 */
static inline uint16_t
_recv_raw_pkts_vec(struct i40e_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		   uint16_t nb_pkts, uint8_t *split_packet)
{
	volatile union i40e_rx_desc *rxdp;
	struct i40e_rx_entry *sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	__vector unsigned char shuf_msk;
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	__vector unsigned short crc_adjust = (__vector unsigned short){
		0, 0,         /* ignore pkt_type field */
		rxq->crc_len, /* sub crc on pkt_len */
		0,            /* ignore high-16bits of pkt_len */
		rxq->crc_len, /* sub crc on data_len */
		0, 0, 0       /* ignore non-length fields */
		};
	__vector unsigned long dd_check, eop_check;

	/* nb_pkts has to be floor-aligned to RTE_I40E_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_I40E_DESCS_PER_LOOP);

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles
	 */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > RTE_I40E_RXQ_REARM_THRESH)
		i40e_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
			rte_cpu_to_le_32(1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/* 4 packets DD mask */
	dd_check = (__vector unsigned long){0x0000000100000001ULL,
					  0x0000000100000001ULL};

	/* 4 packets EOP mask */
	eop_check = (__vector unsigned long){0x0000000200000002ULL,
					   0x0000000200000002ULL};

	/* mask to shuffle from desc. to mbuf */
	shuf_msk = (__vector unsigned char){
		0xFF, 0xFF,   /* pkt_type set as unknown */
		0xFF, 0xFF,   /* pkt_type set as unknown */
		14, 15,       /* octet 15~14, low 16 bits pkt_len */
		0xFF, 0xFF,   /* skip high 16 bits pkt_len, zero out */
		14, 15,       /* octet 15~14, 16 bits data_len */
		2, 3,         /* octet 2~3, low 16 bits vlan_macip */
		4, 5, 6, 7    /* octet 4~7, 32bits rss */
		};

	/* Cache is empty -> need to scan the buffer rings, but first move
	 * the next 'n' mbufs into the cache
	 */
	sw_ring = &rxq->sw_ring[rxq->rx_tail];

	/* A. load 4 packet in one loop
	 * [A*. mask out 4 unused dirty field in desc]
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * [C*. extract the end-of-packet bit, if requested]
	 * D. fill info. from desc to mbuf
	 */

	for (pos = 0, nb_pkts_recd = 0; pos < nb_pkts;
			pos += RTE_I40E_DESCS_PER_LOOP,
			rxdp += RTE_I40E_DESCS_PER_LOOP) {
		__vector unsigned long descs[RTE_I40E_DESCS_PER_LOOP];
		__vector unsigned char pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		__vector unsigned short staterr, sterr_tmp1, sterr_tmp2;
		__vector unsigned long mbp1, mbp2; /* two mbuf pointer
						  * in one XMM reg.
						  */

		/* B.1 load 2 mbuf point */
		mbp1 = *(__vector unsigned long *)&sw_ring[pos];
		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load desc[3] */
		descs[3] = *(__vector unsigned long *)(rxdp + 3);
		rte_compiler_barrier();

		/* B.2 copy 2 mbuf point into rx_pkts  */
		*(__vector unsigned long *)&rx_pkts[pos] = mbp1;

		/* B.1 load 2 mbuf point */
		mbp2 = *(__vector unsigned long *)&sw_ring[pos + 2];

		/* A.1 load desc[2-0] */
		descs[2] = *(__vector unsigned long *)(rxdp + 2);
		rte_compiler_barrier();
		descs[1] = *(__vector unsigned long *)(rxdp + 1);
		rte_compiler_barrier();
		descs[0] = *(__vector unsigned long *)(rxdp);

		/* B.2 copy 2 mbuf point into rx_pkts  */
		*(__vector unsigned long *)&rx_pkts[pos + 2] =  mbp2;

		if (split_packet) {
			rte_mbuf_prefetch_part2(rx_pkts[pos]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 1]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 2]);
			rte_mbuf_prefetch_part2(rx_pkts[pos + 3]);
		}

		/* avoid compiler reorder optimization */
		rte_compiler_barrier();

		/* pkt 3,4 shift the pktlen field to be 16-bit aligned*/
		const __vector unsigned int len3 = vec_sl(
			vec_ld(0, (__vector unsigned int *)&descs[3]),
			(__vector unsigned int){0, 0, 0, PKTLEN_SHIFT});

		const __vector unsigned int len2 = vec_sl(
			vec_ld(0, (__vector unsigned int *)&descs[2]),
			(__vector unsigned int){0, 0, 0, PKTLEN_SHIFT});

		/* merge the now-aligned packet length fields back in */
		descs[3] = (__vector unsigned long)len3;
		descs[2] = (__vector unsigned long)len2;

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = vec_perm((__vector unsigned char)descs[3],
				  (__vector unsigned char){}, shuf_msk);
		pkt_mb3 = vec_perm((__vector unsigned char)descs[2],
				  (__vector unsigned char){}, shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = vec_mergel((__vector unsigned short)descs[3],
					(__vector unsigned short)descs[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = vec_mergel((__vector unsigned short)descs[1],
					(__vector unsigned short)descs[0]);
		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		pkt_mb4 = (__vector unsigned char)vec_sub(
				(__vector unsigned short)pkt_mb4, crc_adjust);
		pkt_mb3 = (__vector unsigned char)vec_sub(
				(__vector unsigned short)pkt_mb3, crc_adjust);

		/* pkt 1,2 shift the pktlen field to be 16-bit aligned*/
		const __vector unsigned int len1 = vec_sl(
			vec_ld(0, (__vector unsigned int *)&descs[1]),
			(__vector unsigned int){0, 0, 0, PKTLEN_SHIFT});
		const __vector unsigned int len0 = vec_sl(
			vec_ld(0, (__vector unsigned int *)&descs[0]),
			(__vector unsigned int){0, 0, 0, PKTLEN_SHIFT});

		/* merge the now-aligned packet length fields back in */
		descs[1] = (__vector unsigned long)len1;
		descs[0] = (__vector unsigned long)len0;

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = vec_perm((__vector unsigned char)descs[1],
				(__vector unsigned char){}, shuf_msk);
		pkt_mb1 = vec_perm((__vector unsigned char)descs[0],
				(__vector unsigned char){}, shuf_msk);

		/* C.2 get 4 pkts staterr value  */
		staterr = (__vector unsigned short)vec_mergeh(
				sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		vec_st(pkt_mb4, 0,
		 (__vector unsigned char *)&rx_pkts[pos + 3]
			->rx_descriptor_fields1
		);
		vec_st(pkt_mb3, 0,
		 (__vector unsigned char *)&rx_pkts[pos + 2]
			->rx_descriptor_fields1
		);

		/* D.2 pkt 1,2 set in_port/nb_seg and remove crc */
		pkt_mb2 = (__vector unsigned char)vec_sub(
				(__vector unsigned short)pkt_mb2, crc_adjust);
		pkt_mb1 = (__vector unsigned char)vec_sub(
				(__vector unsigned short)pkt_mb1,	crc_adjust);

		/* C* extract and record EOP bit */
		if (split_packet) {
			__vector unsigned char eop_shuf_mask =
				(__vector unsigned char){
					0xFF, 0xFF, 0xFF, 0xFF,
					0xFF, 0xFF, 0xFF, 0xFF,
					0xFF, 0xFF, 0xFF, 0xFF,
					0x04, 0x0C, 0x00, 0x08
				};

			/* and with mask to extract bits, flipping 1-0 */
			__vector unsigned char eop_bits = vec_and(
				(__vector unsigned char)vec_nor(staterr, staterr),
				(__vector unsigned char)eop_check);
			/* the staterr values are not in order, as the count
			 * of dd bits doesn't care. However, for end of
			 * packet tracking, we do care, so shuffle. This also
			 * compresses the 32-bit values to 8-bit
			 */
			eop_bits = vec_perm(eop_bits, (__vector unsigned char){},
					    eop_shuf_mask);
			/* store the resulting 32-bit value */
			*split_packet = (vec_ld(0,
					 (__vector unsigned int *)&eop_bits))[0];
			split_packet += RTE_I40E_DESCS_PER_LOOP;

			/* zero-out next pointers */
			rx_pkts[pos]->next = NULL;
			rx_pkts[pos + 1]->next = NULL;
			rx_pkts[pos + 2]->next = NULL;
			rx_pkts[pos + 3]->next = NULL;
		}

		/* C.3 calc available number of desc */
		staterr = vec_and(staterr, (__vector unsigned short)dd_check);

		/* D.3 copy final 1,2 data to rx_pkts */
		vec_st(pkt_mb2, 0,
		 (__vector unsigned char *)&rx_pkts[pos + 1]
			->rx_descriptor_fields1
		);
		vec_st(pkt_mb1, 0,
		 (__vector unsigned char *)&rx_pkts[pos]->rx_descriptor_fields1
		);
		desc_to_ptype_v(descs, &rx_pkts[pos], ptype_tbl);
		desc_to_olflags_v(descs, &rx_pkts[pos]);

		/* C.4 calc available number of desc */
		var = __builtin_popcountll((vec_ld(0,
			(__vector unsigned long *)&staterr)[0]));
		nb_pkts_recd += var;
		if (likely(var != RTE_I40E_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

 /* Notice:
  * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
  * - nb_pkts > RTE_I40E_VPMD_RX_BURST, only scan RTE_I40E_VPMD_RX_BURST
  *   numbers of DD bits
  */
uint16_t
i40e_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		   uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec(rx_queue, rx_pkts, nb_pkts, NULL);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 *
 * Notice:
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 */
static uint16_t
i40e_recv_scattered_burst_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			      uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq = rx_queue;
	uint8_t split_flags[RTE_I40E_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _recv_raw_pkts_vec(rxq, rx_pkts, nb_pkts,
			split_flags);
	if (nb_bufs == 0)
		return 0;

	/* happy day case, full burst + no packets to be joined */
	const uint64_t *split_fl64 = (uint64_t *)split_flags;

	if (rxq->pkt_first_seg == NULL &&
	    split_fl64[0] == 0 && split_fl64[1] == 0 &&
	    split_fl64[2] == 0 && split_fl64[3] == 0)
		return nb_bufs;

	/* reassemble any packets that need reassembly*/
	unsigned int i = 0;

	if (!rxq->pkt_first_seg) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
	}
	return i + reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
		&split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 */
uint16_t
i40e_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts)
{
	uint16_t retval = 0;

	while (nb_pkts > RTE_I40E_VPMD_RX_BURST) {
		uint16_t burst;

		burst = i40e_recv_scattered_burst_vec(rx_queue,
						      rx_pkts + retval,
						      RTE_I40E_VPMD_RX_BURST);
		retval += burst;
		nb_pkts -= burst;
		if (burst < RTE_I40E_VPMD_RX_BURST)
			return retval;
	}

	return retval + i40e_recv_scattered_burst_vec(rx_queue,
						      rx_pkts + retval,
						      nb_pkts);
}

static inline void
vtx1(volatile struct i40e_tx_desc *txdp,
	struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw = (I40E_TX_DESC_DTYPE_DATA |
		((uint64_t)flags  << I40E_TXD_QW1_CMD_SHIFT) |
		((uint64_t)pkt->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT));

	__vector unsigned long descriptor = (__vector unsigned long){
		pkt->buf_iova + pkt->data_off, high_qw};
	*(__vector unsigned long *)txdp = descriptor;
}

static inline void
vtx(volatile struct i40e_tx_desc *txdp,
	struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	int i;

	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

uint16_t
i40e_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;
	volatile struct i40e_tx_desc *txdp;
	struct i40e_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = I40E_TD_CMD;
	uint64_t rs = I40E_TX_DESC_CMD_RS | I40E_TD_CMD;
	int i;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->tx_rs_thresh);

	if (txq->nb_tx_free < txq->tx_free_thresh)
		i40e_tx_free_bufs(txq);

	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	nb_commit = nb_pkts;
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry(txep, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			vtx1(txdp, *tx_pkts, flags);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	I40E_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

void __rte_cold
i40e_rx_queue_release_mbufs_vec(struct i40e_rx_queue *rxq)
{
	_i40e_rx_queue_release_mbufs_vec(rxq);
}

int __rte_cold
i40e_rxq_vec_setup(struct i40e_rx_queue *rxq)
{
	return i40e_rxq_vec_setup_default(rxq);
}

int __rte_cold
i40e_txq_vec_setup(struct i40e_tx_queue __rte_unused * txq)
{
	return 0;
}

int __rte_cold
i40e_rx_vec_dev_conf_condition_check(struct rte_eth_dev *dev)
{
	return i40e_rx_vec_dev_conf_condition_check_default(dev);
}
