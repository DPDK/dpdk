/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_eal.h>

#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_byteorder.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <dev_driver.h>

#include "r8169_ethdev.h"
#include "r8169_hw.h"
#include "r8169_logs.h"

/* Struct RxDesc in kernel r8169 */
struct rtl_rx_desc {
	u32 opts1;
	u32 opts2;
	u64 addr;
};

/* Structure associated with each descriptor of the RX ring of a RX queue. */
struct rtl_rx_entry {
	struct rte_mbuf *mbuf;
};

/* Structure associated with each RX queue. */
struct rtl_rx_queue {
	struct rte_mempool   *mb_pool;
	struct rtl_rx_desc   *hw_ring;
	struct rtl_rx_entry  *sw_ring;
	struct rte_mbuf      *pkt_first_seg; /* First segment of current packet. */
	struct rte_mbuf      *pkt_last_seg;  /* Last segment of current packet. */
	struct rtl_hw        *hw;
	uint64_t	     hw_ring_phys_addr;
	uint64_t	     offloads;
	uint16_t	     nb_rx_desc;
	uint16_t	     rx_tail;
	uint16_t	     nb_rx_hold;
	uint16_t	     queue_id;
	uint16_t	     port_id;
	uint16_t	     rx_free_thresh;
};

enum _DescStatusBit {
	DescOwn     = (1 << 31), /* Descriptor is owned by NIC. */
	RingEnd     = (1 << 30), /* End of descriptor ring */
	FirstFrag   = (1 << 29), /* First segment of a packet */
	LastFrag    = (1 << 28), /* Final segment of a packet */

	DescOwn_V3  = DescOwn, /* Descriptor is owned by NIC. */
	RingEnd_V3  = RingEnd, /* End of descriptor ring */
	FirstFrag_V3 = (1 << 25), /* First segment of a packet */
	LastFrag_V3 = (1 << 24), /* Final segment of a packet */

	/* TX private */
	/*------ offset 0 of TX descriptor ------*/
	LargeSend   = (1 << 27), /* TCP Large Send Offload (TSO) */
	GiantSendv4 = (1 << 26), /* TCP Giant Send Offload V4 (GSOv4) */
	GiantSendv6 = (1 << 25), /* TCP Giant Send Offload V6 (GSOv6) */
	LargeSend_DP = (1 << 16), /* TCP Large Send Offload (TSO) */
	MSSShift    = 16,        /* MSS value position */
	MSSMask     = 0x7FFU,    /* MSS value 11 bits */
	TxIPCS      = (1 << 18), /* Calculate IP checksum */
	TxUDPCS     = (1 << 17), /* Calculate UDP/IP checksum */
	TxTCPCS     = (1 << 16), /* Calculate TCP/IP checksum */
	TxVlanTag   = (1 << 17), /* Add VLAN tag */

	/*@@@@@@ offset 4 of TX descriptor => bits for RTL8169 only     begin @@@@@@*/
	TxUDPCS_C   = (1 << 31), /* Calculate UDP/IP checksum */
	TxTCPCS_C   = (1 << 30), /* Calculate TCP/IP checksum */
	TxIPCS_C    = (1 << 29), /* Calculate IP checksum */
	TxIPV6F_C   = (1 << 28), /* Indicate it is an IPv6 packet */
	/*@@@@@@ offset 4 of tx descriptor => bits for RTL8169 only     end @@@@@@*/

	/* RX private */
	/* ------ offset 0 of RX descriptor ------ */
	PID1        = (1 << 18), /* Protocol ID bit 1/2 */
	PID0        = (1 << 17), /* Protocol ID bit 2/2 */

#define RxProtoUDP  PID1
#define RxProtoTCP  PID0
#define RxProtoIP   (PID1 | PID0)
#define RxProtoMask RxProtoIP

	RxIPF       = (1 << 16), /* IP checksum failed */
	RxUDPF      = (1 << 15), /* UDP/IP checksum failed */
	RxTCPF      = (1 << 14), /* TCP/IP checksum failed */
	RxVlanTag   = (1 << 16), /* VLAN tag available */

	/*@@@@@@ offset 0 of RX descriptor => bits for RTL8169 only     begin @@@@@@*/
	RxUDPT      = (1 << 18),
	RxTCPT      = (1 << 17),
	/*@@@@@@ offset 0 of RX descriptor => bits for RTL8169 only     end @@@@@@*/

	/*@@@@@@ offset 4 of RX descriptor => bits for RTL8169 only     begin @@@@@@*/
	RxV6F       = (1 << 31),
	RxV4F       = (1 << 30),
	/*@@@@@@ offset 4 of RX descriptor => bits for RTL8169 only     end @@@@@@*/

	PID1_v3        = (1 << 29), /* Protocol ID bit 1/2 */
	PID0_v3        = (1 << 28), /* Protocol ID bit 2/2 */

#define RxProtoUDP_v3  PID1_v3
#define RxProtoTCP_v3  PID0_v3
#define RxProtoIP_v3   (PID1_v3 | PID0_v3)
#define RxProtoMask_v3 RxProtoIP_v3

	RxIPF_v3       = (1 << 26), /* IP checksum failed */
	RxUDPF_v3      = (1 << 25), /* UDP/IP checksum failed */
	RxTCPF_v3      = (1 << 24), /* TCP/IP checksum failed */
	RxSCTPF_v3     = (1 << 23), /* TCP/IP checksum failed */
	RxVlanTag_v3   = (RxVlanTag), /* VLAN tag available */

	/*@@@@@@ offset 0 of RX descriptor => bits for RTL8169 only     begin @@@@@@*/
	RxUDPT_v3      = (1 << 29),
	RxTCPT_v3      = (1 << 28),
	RxSCTP_v3      = (1 << 27),
	/*@@@@@@ offset 0 of RX descriptor => bits for RTL8169 only     end @@@@@@*/

	/*@@@@@@ offset 4 of RX descriptor => bits for RTL8169 only     begin @@@@@@*/
	RxV6F_v3       = RxV6F,
	RxV4F_v3       = RxV4F,
	/*@@@@@@ offset 4 of RX descriptor => bits for RTL8169 only     end @@@@@@*/
};
/* ---------------------------------RX---------------------------------- */

static void
rtl_rx_queue_release_mbufs(struct rtl_rx_queue *rxq)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	if (rxq != NULL) {
		if (rxq->sw_ring != NULL) {
			for (i = 0; i < rxq->nb_rx_desc; i++) {
				if (rxq->sw_ring[i].mbuf != NULL) {
					rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
					rxq->sw_ring[i].mbuf = NULL;
				}
			}
		}
	}
}

void
rtl_rx_queue_release(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct rtl_rx_queue *rxq = dev->data->rx_queues[rx_queue_id];

	PMD_INIT_FUNC_TRACE();

	if (rxq != NULL) {
		rtl_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

void
rtl_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		 struct rte_eth_rxq_info *qinfo)
{
	struct rtl_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.offloads = rxq->offloads;
}

static void
rtl_reset_rx_queue(struct rtl_rx_queue *rxq)
{
	static const struct rtl_rx_desc zero_rxd = {0};
	int i;

	for (i = 0; i < rxq->nb_rx_desc; i++)
		rxq->hw_ring[i] = zero_rxd;

	rxq->hw_ring[rxq->nb_rx_desc - 1].opts1 = rte_cpu_to_le_32(RingEnd);
	rxq->rx_tail = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

uint64_t
rtl_get_rx_port_offloads(void)
{
	uint64_t offloads;

	offloads = RTE_ETH_RX_OFFLOAD_IPV4_CKSUM  |
		   RTE_ETH_RX_OFFLOAD_UDP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_TCP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_SCATTER     |
		   RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	return offloads;
}

int
rtl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		   uint16_t nb_rx_desc, unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf,
		   struct rte_mempool *mb_pool)
{
	struct rtl_rx_queue *rxq;
	const struct rte_memzone *mz;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	uint32_t size;

	PMD_INIT_FUNC_TRACE();

	/*
	 * If this queue existed already, free the associated memory. The
	 * queue cannot be reused in case we need to allocate memory on
	 * different socket than was previously used.
	 */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		rtl_rx_queue_release(dev, queue_idx);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("r8169 RX queue", sizeof(struct rtl_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);

	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate Rx queue structure");
		return -ENOMEM;
	}

	/* Setup queue */
	rxq->mb_pool = mb_pool;
	rxq->nb_rx_desc = nb_rx_desc;
	rxq->port_id = dev->data->port_id;
	rxq->queue_id = queue_idx;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;

	/* Allocate memory for the software ring */
	rxq->sw_ring = rte_calloc("r8169 sw rx ring", nb_rx_desc,
				  sizeof(struct rtl_rx_entry), RTE_CACHE_LINE_SIZE);

	if (rxq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR,
			     "Port %d: Cannot allocate software ring for queue %d",
			     rxq->port_id, rxq->queue_id);
		rte_free(rxq);
		return -ENOMEM;
	}

	/*
	 * Allocate RX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	size = sizeof(struct rtl_rx_desc) * (nb_rx_desc + 1);
	mz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx, size,
				      RTL_RING_ALIGN, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR,
			     "Port %d: Cannot allocate software ring for queue %d",
			     rxq->port_id, rxq->queue_id);
		rtl_rx_queue_release(dev, rxq->queue_id);
		return -ENOMEM;
	}

	rxq->hw = hw;
	rxq->hw_ring = mz->addr;
	rxq->hw_ring_phys_addr = mz->iova;
	rxq->offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	rtl_reset_rx_queue(rxq);

	dev->data->rx_queues[queue_idx] = rxq;

	return 0;
}

static int
rtl_alloc_rx_queue_mbufs(struct rtl_rx_queue *rxq)
{
	struct rtl_rx_entry *rxe = rxq->sw_ring;
	struct rtl_hw *hw = rxq->hw;
	struct rtl_rx_desc *rxd;
	int i;
	uint64_t dma_addr;

	rxd = &rxq->hw_ring[0];

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "RX mbuf alloc failed queue_id=%hu",
				     rxq->queue_id);
			return -ENOMEM;
		}

		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		rxd = &rxq->hw_ring[i];
		rxd->addr = dma_addr;
		rxd->opts2 = 0;
		rte_wmb();
		rxd->opts1 = rte_cpu_to_le_32(DescOwn | hw->rx_buf_sz);
		rxe[i].mbuf = mbuf;
	}

	/* Mark as last desc */
	rxd->opts1 |= rte_cpu_to_le_32(RingEnd);

	return 0;
}

static int
rtl_hw_set_features(struct rtl_hw *hw, uint64_t offloads)
{
	u16 cp_cmd;
	u32 rx_config;

	rx_config = RTL_R32(hw, RxConfig);
	if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		rx_config |= (EnableInnerVlan | EnableOuterVlan);
	else
		rx_config &= ~(EnableInnerVlan | EnableOuterVlan);

	RTL_W32(hw, RxConfig, rx_config);

	cp_cmd = RTL_R16(hw, CPlusCmd);

	if (offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		cp_cmd |= RxChkSum;
	else
		cp_cmd &= ~RxChkSum;

	RTL_W16(hw, CPlusCmd, cp_cmd);

	return 0;
}

static void
rtl_hw_set_rx_packet_filter(struct rtl_hw *hw)
{
	int rx_mode;

	hw->hw_ops.hw_init_rxcfg(hw);

	rx_mode = AcceptBroadcast | AcceptMyPhys;
	RTL_W32(hw, RxConfig, rx_mode | (RTL_R32(hw, RxConfig)));
}

int
rtl_rx_init(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rtl_rx_queue *rxq;
	int ret;
	u32 max_rx_pkt_size;

	rxq = dev->data->rx_queues[0];

	if (rxq->mb_pool == NULL) {
		PMD_INIT_LOG(ERR, "r8169 rx queue pool not setup!");
		return -ENOMEM;
	}

	RTL_W32(hw, RxDescAddrLow, ((u64)rxq->hw_ring_phys_addr & DMA_BIT_MASK(32)));
	RTL_W32(hw, RxDescAddrHigh, ((u64)rxq->hw_ring_phys_addr >> 32));

	dev->rx_pkt_burst = rtl_recv_pkts;
	hw->rx_buf_sz = rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;

	max_rx_pkt_size = dev->data->mtu + RTL_ETH_OVERHEAD;

	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER ||
	    max_rx_pkt_size > hw->rx_buf_sz) {
		if (!dev->data->scattered_rx)
			PMD_INIT_LOG(DEBUG, "forcing scatter mode");
		dev->rx_pkt_burst = rtl_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	RTL_W16(hw, RxMaxSize, max_rx_pkt_size);

	ret = rtl_alloc_rx_queue_mbufs(rxq);
	if (ret) {
		PMD_INIT_LOG(ERR, "r8169 rx mbuf alloc failed!");
		return ret;
	}

	rtl_enable_cfg9346_write(hw);

	/* RX accept type and csum vlan offload */
	rtl_hw_set_features(hw, rxq->offloads);

	rtl_disable_rxdvgate(hw);

	/* Set Rx packet filter */
	rtl_hw_set_rx_packet_filter(hw);

	rtl_disable_cfg9346_write(hw);

	RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) | CmdRxEnb);

	dev->data->rx_queue_state[0] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static inline void
rtl_mark_to_asic(struct rtl_rx_desc *rxd, u32 size)
{
	u32 eor = rte_le_to_cpu_32(rxd->opts1) & RingEnd;

	rxd->opts1 = rte_cpu_to_le_32(DescOwn | eor | size);
}

static inline uint64_t
rtl_rx_desc_error_to_pkt_flags(struct rtl_rx_queue *rxq, uint32_t opts1,
			       uint32_t opts2)
{
	uint64_t pkt_flags = 0;

	if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM))
		goto exit;

	/* RX csum offload for RTL8169*/
	if (((opts2 & RxV4F) && !(opts1 & RxIPF)) || (opts2 & RxV6F)) {
		pkt_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		if (((opts1 & RxTCPT) && !(opts1 & RxTCPF)) ||
		    ((opts1 & RxUDPT) && !(opts1 & RxUDPF)))
			pkt_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	}

exit:
	return pkt_flags;
}

/* PMD receive function */
uint16_t
rtl_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct rtl_rx_queue *rxq = (struct rtl_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	struct rtl_hw *hw = rxq->hw;
	struct rtl_rx_desc *rxd;
	struct rtl_rx_desc *hw_ring;
	struct rtl_rx_entry *rxe;
	struct rtl_rx_entry *sw_ring = rxq->sw_ring;
	struct rte_mbuf *new_mb;
	struct rte_mbuf *rmb;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_sw_stats *stats = &adapter->sw_stats;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;
	uint16_t tail = rxq->rx_tail;
	const uint16_t nb_rx_desc = rxq->nb_rx_desc;
	uint32_t opts1;
	uint32_t opts2;
	uint16_t pkt_len = 0;
	uint64_t dma_addr;

	hw_ring = rxq->hw_ring;

	RTE_ASSERT(RTL_R8(hw, ChipCmd) & CmdRxEnb);

	while (nb_rx < nb_pkts) {
		rxd = &hw_ring[tail];

		opts1 = rte_le_to_cpu_32(rxd->opts1);
		if (opts1 & DescOwn)
			break;

		/*
		 * This barrier is needed to keep us from reading
		 * any other fields out of the Rx descriptor until
		 * we know the status of DescOwn.
		 */
		rte_rmb();

		if (unlikely(opts1 & RxRES)) {
			stats->rx_errors++;
			rtl_mark_to_asic(rxd, hw->rx_buf_sz);
			nb_hold++;
			tail = (tail + 1) % nb_rx_desc;
		} else {
			opts2 = rte_le_to_cpu_32(rxd->opts2);

			new_mb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (new_mb == NULL) {
				PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
					   "queue_id=%u",
					   (uint32_t)rxq->port_id, (uint32_t)rxq->queue_id);
				dev->data->rx_mbuf_alloc_failed++;
				break;
			}

			nb_hold++;
			rxe = &sw_ring[tail];

			rmb = rxe->mbuf;

			tail = (tail + 1) % nb_rx_desc;

			/* Prefetch next mbufs */
			rte_prefetch0(sw_ring[tail].mbuf);

			/*
			 * When next RX descriptor is on a cache-line boundary,
			 * prefetch the next 4 RX descriptors and the next 8 pointers
			 * to mbufs.
			 */
			if ((tail & 0x3) == 0) {
				rte_prefetch0(&sw_ring[tail]);
				rte_prefetch0(&hw_ring[tail]);
			}

			/* Refill the RX desc */
			rxe->mbuf = new_mb;
			dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mb));

			/* Setup RX descriptor */
			rxd->addr = dma_addr;
			rxd->opts2 = 0;
			rte_wmb();
			rtl_mark_to_asic(rxd, hw->rx_buf_sz);

			pkt_len = opts1 & 0x00003fff;
			pkt_len -= RTE_ETHER_CRC_LEN;

			rmb->data_off = RTE_PKTMBUF_HEADROOM;
			rte_prefetch1((char *)rmb->buf_addr + rmb->data_off);
			rmb->nb_segs = 1;
			rmb->next = NULL;
			rmb->pkt_len = pkt_len;
			rmb->data_len = pkt_len;
			rmb->port = rxq->port_id;

			if (opts2 & RxVlanTag)
				rmb->vlan_tci = rte_bswap16(opts2 & 0xffff);

			rmb->ol_flags = rtl_rx_desc_error_to_pkt_flags(rxq, opts1, opts2);

			/*
			 * Store the mbuf address into the next entry of the array
			 * of returned packets.
			 */
			rx_pkts[nb_rx++] = rmb;

			stats->rx_bytes += pkt_len;
			stats->rx_packets++;
		}
	}

	rxq->rx_tail = tail;

	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rte_wmb();

		/* Clear RDU */
		RTL_W32(hw, ISR0_8125, (RxOK | RxErr | RxDescUnavail));

		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

/* PMD receive function for scattered pkts */
uint16_t
rtl_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct rtl_rx_queue *rxq = (struct rtl_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	struct rtl_hw *hw = rxq->hw;
	struct rtl_rx_desc *rxd;
	struct rtl_rx_desc *hw_ring;
	struct rtl_rx_entry *rxe;
	struct rtl_rx_entry *sw_ring = rxq->sw_ring;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct rte_mbuf *new_mb;
	struct rte_mbuf *rmb;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_sw_stats *stats = &adapter->sw_stats;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;
	uint16_t data_len = 0;
	uint16_t tail = rxq->rx_tail;
	const uint16_t nb_rx_desc = rxq->nb_rx_desc;
	uint32_t opts1;
	uint32_t opts2;
	uint64_t dma_addr;

	hw_ring = rxq->hw_ring;

	/*
	 * Retrieve RX context of current packet, if any.
	 */
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;

	RTE_ASSERT(RTL_R8(hw, ChipCmd) & CmdRxEnb);

	while (nb_rx < nb_pkts) {
next_desc:
		rxd = &hw_ring[tail];

		opts1 = rte_le_to_cpu_32(rxd->opts1);
		if (opts1 & DescOwn)
			break;

		/*
		 * This barrier is needed to keep us from reading
		 * any other fields out of the Rx descriptor until
		 * we know the status of DescOwn
		 */
		rte_rmb();

		if (unlikely(opts1 & RxRES)) {
			stats->rx_errors++;
			rtl_mark_to_asic(rxd, hw->rx_buf_sz);
			nb_hold++;
			tail = (tail + 1) % nb_rx_desc;
		} else {
			opts2 = rte_le_to_cpu_32(rxd->opts2);

			new_mb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (new_mb == NULL) {
				PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
					   "queue_id=%u",
					   (uint32_t)rxq->port_id, (uint32_t)rxq->queue_id);
				dev->data->rx_mbuf_alloc_failed++;
				break;
			}

			nb_hold++;
			rxe = &sw_ring[tail];

			rmb = rxe->mbuf;

			/* Prefetch next mbufs */
			tail = (tail + 1) % nb_rx_desc;
			rte_prefetch0(sw_ring[tail].mbuf);

			/*
			 * When next RX descriptor is on a cache-line boundary,
			 * prefetch the next 4 RX descriptors and the next 8 pointers
			 * to mbufs.
			 */
			if ((tail & 0x3) == 0) {
				rte_prefetch0(&sw_ring[tail]);
				rte_prefetch0(&hw_ring[tail]);
			}

			/* Refill the RX desc */
			rxe->mbuf = new_mb;
			dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mb));

			/* Setup RX descriptor */
			rxd->addr = dma_addr;
			rxd->opts2 = 0;
			rte_wmb();
			rtl_mark_to_asic(rxd, hw->rx_buf_sz);

			data_len = opts1 & 0x00003fff;
			rmb->data_len = data_len;
			rmb->data_off = RTE_PKTMBUF_HEADROOM;

			/*
			 * If this is the first buffer of the received packet,
			 * set the pointer to the first mbuf of the packet and
			 * initialize its context.
			 * Otherwise, update the total length and the number of segments
			 * of the current scattered packet, and update the pointer to
			 * the last mbuf of the current packet.
			 */
			if (first_seg == NULL) {
				first_seg = rmb;
				first_seg->pkt_len = data_len;
				first_seg->nb_segs = 1;
			} else {
				first_seg->pkt_len += data_len;
				first_seg->nb_segs++;
				last_seg->next = rmb;
			}

			/*
			 * If this is not the last buffer of the received packet,
			 * update the pointer to the last mbuf of the current scattered
			 * packet and continue to parse the RX ring.
			 */
			if (!(opts1 & LastFrag)) {
				last_seg = rmb;
				goto next_desc;
			}

			/*
			 * This is the last buffer of the received packet.
			 */
			rmb->next = NULL;

			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (data_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rmb);
				first_seg->nb_segs--;
				last_seg->data_len = last_seg->data_len -
						     (RTE_ETHER_CRC_LEN - data_len);
				last_seg->next = NULL;
			} else {
				rmb->data_len = data_len - RTE_ETHER_CRC_LEN;
			}

			first_seg->port = rxq->port_id;

			if (opts2 & RxVlanTag)
				first_seg->vlan_tci = rte_bswap16(opts2 & 0xffff);

			first_seg->ol_flags = rtl_rx_desc_error_to_pkt_flags(rxq, opts1, opts2);

			rte_prefetch1((char *)first_seg->buf_addr + first_seg->data_off);

			/*
			 * Store the mbuf address into the next entry of the array
			 * of returned packets.
			 */
			rx_pkts[nb_rx++] = first_seg;

			stats->rx_bytes += first_seg->pkt_len;
			stats->rx_packets++;

			/*
			 * Setup receipt context for a new packet.
			 */
			first_seg = NULL;
		}
	}

	/*
	 * Record index of the next RX descriptor to probe.
	 */
	rxq->rx_tail = tail;

	/*
	 * Save receive context.
	 */
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rte_wmb();

		/* Clear RDU */
		RTL_W32(hw, ISR0_8125, (RxOK | RxErr | RxDescUnavail));

		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

/* ---------------------------------TX---------------------------------- */
int
rtl_tx_init(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

uint16_t
rtl_xmit_pkts(void *txq __rte_unused, struct rte_mbuf **tx_pkts __rte_unused,
	      uint16_t nb_pkts __rte_unused)
{
	return 0;
}

int
rtl_stop_queues(struct rte_eth_dev *dev)
{
	struct rtl_rx_queue *rxq;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[0];

	rtl_rx_queue_release_mbufs(rxq);
	rtl_reset_rx_queue(rxq);
	dev->data->rx_queue_state[0] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
rtl_free_queues(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	rte_eth_dma_zone_free(dev, "rx_ring", 0);
	rtl_rx_queue_release(dev, 0);
	dev->data->rx_queues[0] = 0;
	dev->data->nb_rx_queues = 0;
}
