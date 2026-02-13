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

/* Bit mask to indicate what bits required for building TX context */
#define RTL_TX_OFFLOAD_MASK (RTE_MBUF_F_TX_IPV6 |		\
			     RTE_MBUF_F_TX_IPV4 |		\
			     RTE_MBUF_F_TX_VLAN |		\
			     RTE_MBUF_F_TX_IP_CKSUM |	        \
			     RTE_MBUF_F_TX_L4_MASK |		\
			     RTE_MBUF_F_TX_TCP_SEG)

#define MIN_PATCH_LENGTH 47
#define ETH_ZLEN	 60		/* Min. octets in frame sans FCS */

#define QUEUE_NUM_LIMIT 128

/* Struct TxDesc in kernel r8169 */
struct rtl_tx_desc {
	RTE_ATOMIC(u32) opts1;
	u32 opts2;
	u64 addr;
};

/* Structure associated with each descriptor of the TX ring of a TX queue. */
struct rtl_tx_entry {
	struct rte_mbuf *mbuf;
};

/* Structure associated with each descriptor of the RX ring of a RX queue. */
struct rtl_rx_entry {
	struct rte_mbuf *mbuf;
};

/* Structure associated with each TX queue. */
struct rtl_tx_queue {
	struct rtl_tx_desc   *hw_ring;
	struct rtl_tx_entry  *sw_ring;
	struct rtl_hw        *hw;
	uint64_t	     hw_ring_phys_addr;
	uint16_t	     nb_tx_desc;
	RTE_ATOMIC(uint32_t) tx_tail;
	uint16_t	     tx_head;
	uint16_t	     queue_id;
	uint16_t	     port_id;
	uint16_t	     tx_free_thresh;
	uint16_t	     tx_free;
	uint16_t	     hw_clo_ptr_reg;
	uint16_t	     sw_tail_ptr_reg;
	uint32_t	     NextHwDesCloPtr;
	uint32_t	     BeginHwDesCloPtr;
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

	DescOwn_V3   = DescOwn, /* Descriptor is owned by NIC. */
	RingEnd_V3   = RingEnd, /* End of descriptor ring */
	FirstFrag_V3 = (1 << 25), /* First segment of a packet */
	LastFrag_V3  = (1 << 24), /* Final segment of a packet */

	DescOwn_V4   = DescOwn, /* Descriptor is owned by NIC */
	RingEnd_V4   = RingEnd, /* End of descriptor ring */
	FirstFrag_V4 = FirstFrag, /* First segment of a packet */
	LastFrag_V4  = LastFrag, /* Final segment of a packet */

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
	RxIPF_v4       = (1 << 17), /* IP checksum failed */
	RxUDPF_v4      = (1 << 16), /* UDP/IP checksum failed */
	RxTCPF_v4      = (1 << 15), /* TCP/IP checksum failed */
	RxSCTPF_v4     = (1 << 19), /* SCTP checksum failed */
	RxVlanTag_v4   = RxVlanTag, /* VLAN tag available */

	/*@@@@@@ offset 0 of rx descriptor => bits for RTL8125 only     begin @@@@@@*/
	RxUDPT_v4      = (1 << 19),
	RxTCPT_v4      = (1 << 18),
	RxSCTP_v4      = (1 << 19),
	/*@@@@@@ offset 0 of rx descriptor => bits for RTL8125 only     end @@@@@@*/

	/*@@@@@@ offset 4 of rx descriptor => bits for RTL8125 only     begin @@@@@@*/
	RxV6F_v4       = RxV6F,
	RxV4F_v4       = RxV4F,
	/*@@@@@@ offset 4 of rx descriptor => bits for RTL8125 only     end @@@@@@*/
};

#define GTTCPHO_SHIFT  18
#define GTTCPHO_MAX    0x70U
#define GTPKTSIZE_MAX  0x3ffffU
#define TCPHO_SHIFT    18
#define TCPHO_MAX      0x3ffU
#define LSOPKTSIZE_MAX 0xffffU
#define MSS_MAX        0x07ffu /* MSS value */

typedef void (*rtl_clear_rdu_func)(struct rtl_hw *, uint16_t);
static rtl_clear_rdu_func rtl_clear_rdu;

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
rtl_mark_as_last_descriptor_v1(struct rtl_rx_desc *desc)
{
	desc->opts1 |= rte_cpu_to_le_32(RingEnd);
}

static void
rtl_mark_as_last_descriptor_v3(struct rtl_rx_descv3 *descv3)
{
	descv3->RxDescNormalDDWord4.opts1 |= rte_cpu_to_le_32(RingEnd);
}

static void
rtl_mark_as_last_descriptor_v4(struct rtl_rx_descv4 *descv4)
{
	descv4->RxDescNormalDDWord2.opts1 |= rte_cpu_to_le_32(RingEnd);
}

static void
rtl_mark_as_last_descriptor(struct rtl_hw *hw, struct rtl_rx_desc *desc)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		rtl_mark_as_last_descriptor_v3((struct rtl_rx_descv3 *)desc);
		break;
	case RX_DESC_RING_TYPE_4:
		rtl_mark_as_last_descriptor_v4((struct rtl_rx_descv4 *)desc);
		break;
	default:
		rtl_mark_as_last_descriptor_v1(desc);
		break;
	}
}

static void
rtl_reset_rx_queue(struct rtl_rx_queue *rxq)
{
	struct rtl_hw *hw = rxq->hw;
	int i;

	for (i = 0; i < rxq->nb_rx_desc; i++)
		memset(rxq->hw_ring, 0, hw->RxDescLength * rxq->nb_rx_desc);

	rtl_mark_as_last_descriptor(hw, rtl_get_rxdesc(hw, rxq->hw_ring,
						       rxq->nb_rx_desc - 1));

	rxq->rx_tail = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

uint64_t
rtl_get_rx_port_offloads(struct rtl_hw *hw)
{
	uint64_t offloads;

	offloads = RTE_ETH_RX_OFFLOAD_IPV4_CKSUM  |
		   RTE_ETH_RX_OFFLOAD_UDP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_TCP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_SCATTER     |
		   RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	if (hw->EnableRss)
		offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

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
	size = hw->RxDescLength * (nb_rx_desc + 1);
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

static inline void
rtl_set_desc_dma_addr(struct rtl_hw *hw, struct rtl_rx_desc *desc,
		      uint64_t dma_addr)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		((struct rtl_rx_descv3 *)desc)->addr = dma_addr;
		break;
	case RX_DESC_RING_TYPE_4:
		((struct rtl_rx_descv4 *)desc)->addr = dma_addr;
		break;
	default:
		desc->addr = dma_addr;
		break;
	}
}

static inline void
rtl_mark_to_asic_v1(struct rtl_rx_desc *desc, u32 rx_buf_sz)
{
	u32 eor = rte_le_to_cpu_32(desc->opts1) & RingEnd;

	desc->opts2 = 0;
	rte_wmb();
	desc->opts1 = rte_cpu_to_le_32(DescOwn | eor | rx_buf_sz);
}

static inline void
rtl_mark_to_asic_v3(struct rtl_rx_descv3 *descv3, u32 rx_buf_sz)
{
	u32 eor = rte_le_to_cpu_32(descv3->RxDescNormalDDWord4.opts1) & RingEnd;

	descv3->RxDescNormalDDWord4.opts2 = 0;
	rte_wmb();
	descv3->RxDescNormalDDWord4.opts1 = rte_cpu_to_le_32(DescOwn | eor |
							     rx_buf_sz);
}

static inline void
rtl_mark_to_asic_v4(struct rtl_rx_descv4 *descv4, u32 rx_buf_sz)
{
	u32 eor = rte_le_to_cpu_32(descv4->RxDescNormalDDWord2.opts1) & RingEnd;

	descv4->RxDescNormalDDWord2.opts2 = 0;
	rte_wmb();
	descv4->RxDescNormalDDWord2.opts1 = rte_cpu_to_le_32(DescOwn | eor |
							     rx_buf_sz);
}

static inline void
rtl_mark_to_asic(struct rtl_hw *hw, struct rtl_rx_desc *desc)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		rtl_mark_to_asic_v3((struct rtl_rx_descv3 *)desc, hw->rx_buf_sz);
		break;
	case RX_DESC_RING_TYPE_4:
		rtl_mark_to_asic_v4((struct rtl_rx_descv4 *)desc, hw->rx_buf_sz);
		break;
	default:
		rtl_mark_to_asic_v1(desc, hw->rx_buf_sz);
		break;
	}
}

static inline void
rtl_map_to_asic(struct rtl_hw *hw, struct rtl_rx_desc *desc, uint64_t dma_addr)
{
	rtl_set_desc_dma_addr(hw, desc, dma_addr);
	rtl_mark_to_asic(hw, desc);
}

static int
rtl_alloc_rx_queue_mbufs(struct rtl_rx_queue *rxq)
{
	struct rtl_rx_entry *rxe = rxq->sw_ring;
	struct rtl_hw *hw = rxq->hw;
	int i;
	uint64_t dma_addr;

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "RX mbuf alloc failed queue_id=%hu",
				     rxq->queue_id);
			return -ENOMEM;
		}

		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		rtl_map_to_asic(hw, rtl_get_rxdesc(hw, rxq->hw_ring, i),
				dma_addr);
		rxe[i].mbuf = mbuf;
	}

	rtl_mark_as_last_descriptor(hw, rtl_get_rxdesc(hw, rxq->hw_ring, i));

	return 0;
}

static void
rtl8125_hw_set_features(struct rtl_hw *hw, uint64_t offloads)
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
}

static void
rtl8168_hw_set_features(struct rtl_hw *hw, uint64_t offloads)
{
	u16 cp_cmd;

	cp_cmd = RTL_R16(hw, CPlusCmd);

	if (offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		cp_cmd |= RxChkSum;
	else
		cp_cmd &= ~RxChkSum;

	if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		cp_cmd |= RxVlan;
	else
		cp_cmd &= ~RxVlan;

	RTL_W16(hw, CPlusCmd, cp_cmd);
}

static void
rtl_hw_set_features(struct rtl_hw *hw, uint64_t offloads)
{
	if (rtl_is_8125(hw))
		rtl8125_hw_set_features(hw, offloads);
	else
		rtl8168_hw_set_features(hw, offloads);
}

static void
rtl_hw_set_rx_packet_filter(struct rtl_hw *hw)
{
	int rx_mode;

	hw->hw_ops.hw_init_rxcfg(hw);

	rx_mode = AcceptBroadcast | AcceptMyPhys;
	RTL_W32(hw, RxConfig, rx_mode | (RTL_R32(hw, RxConfig)));
}

static void
rtl8125_clear_rdu(struct rtl_hw *hw, uint16_t queue_id)
{
	if (queue_id == 0)
		RTL_W32(hw, ISR0_8125, (RxOK | RxErr | RxDescUnavail));
	else
		RTL_W8(hw, ISR1_8125 + (queue_id - 1) * 4, (BIT_0 | BIT_1));
}

static void
rtl8168_clear_rdu(struct rtl_hw *hw, uint16_t queue_id __rte_unused)
{
	RTL_W16(hw, IntrStatus, (RxOK | RxErr | RxDescUnavail));
}

static void
rtl_init_rss(struct rtl_hw *hw, u16 nb_rx_queues)
{
	u64 rand;
	int i;

	for (i = 0; i < RTL_MAX_INDIRECTION_TABLE_ENTRIES; i++)
		hw->rss_indir_tbl[i] = i & (nb_rx_queues - 1);

	for (i = 0; i < RTL_RSS_KEY_SIZE; i += sizeof(u64)) {
		rand = rte_rand();
		memcpy(&hw->rss_key[i], &rand, sizeof(u64));
	}
}

static void
rtl8125_set_rx_q_num(struct rtl_hw *hw, unsigned int num_rx_queues)
{
	u16 q_ctrl;
	u16 rx_q_num;

	rx_q_num = (u16)rte_log2_u32(num_rx_queues);
	rx_q_num &= (BIT_0 | BIT_1 | BIT_2);
	rx_q_num <<= 2;
	q_ctrl = RTL_R16(hw, Q_NUM_CTRL_8125);
	q_ctrl &= ~(BIT_2 | BIT_3 | BIT_4);
	q_ctrl |= rx_q_num;
	RTL_W16(hw, Q_NUM_CTRL_8125, q_ctrl);
}

int
rtl_rx_init(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rtl_rx_queue *rxq;
	int ret, i;
	u32 csi_tmp, max_rx_pkt_size;
	u16 nb_rx_queues = dev->data->nb_rx_queues;
	u16 desc_start_addr_lo[QUEUE_NUM_LIMIT];

	desc_start_addr_lo[0] = RxDescAddrLow;
	for (i = 1; i < nb_rx_queues; i++)
		desc_start_addr_lo[i] = (u16)(RDSAR_Q1_LOW_8125 + (i - 1) * 8);

	rxq = dev->data->rx_queues[0];
	for (i = 0; i < nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		if (!rxq->mb_pool) {
			PMD_INIT_LOG(ERR, "r8169 rx queue pool not setup!");
			return -ENOMEM;
		}

		hw->rx_buf_sz = rte_pktmbuf_data_room_size(rxq->mb_pool) -
				RTE_PKTMBUF_HEADROOM;

		RTL_W32(hw, desc_start_addr_lo[i],
			(u64)rxq->hw_ring_phys_addr & DMA_BIT_MASK(32));
		RTL_W32(hw, desc_start_addr_lo[i] + 4,
			(u64)rxq->hw_ring_phys_addr >> 32);

		ret = rtl_alloc_rx_queue_mbufs(rxq);
		if (ret) {
			PMD_INIT_LOG(ERR, "r8169 rx mbuf alloc failed!");
			return ret;
		}

		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	max_rx_pkt_size = dev->data->mtu + RTL_ETH_OVERHEAD;
	RTL_W16(hw, RxMaxSize, max_rx_pkt_size);

	dev->rx_pkt_burst = rtl_recv_pkts;
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER ||
	    max_rx_pkt_size > hw->rx_buf_sz) {
		if (!dev->data->scattered_rx)
			PMD_INIT_LOG(DEBUG, "forcing scatter mode");
		dev->rx_pkt_burst = rtl_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	rtl_enable_cfg9346_write(hw);

	if (!rtl_is_8125(hw)) {
		/* RX ftr mcu enable */
		csi_tmp = rtl_eri_read(hw, 0xDC, 1, ERIAR_ExGMAC);
		csi_tmp &= ~BIT_0;
		rtl_eri_write(hw, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
		csi_tmp |= BIT_0;
		rtl_eri_write(hw, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

		/* RSS disable */
		rtl_eri_write(hw, 0xC0, 2, 0x0000, ERIAR_ExGMAC); /* queue num = 1 */
		rtl_eri_write(hw, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);
	}

	/* RX accept type and csum vlan offload */
	rtl_hw_set_features(hw, rxq->offloads);

	rtl_disable_rxdvgate(hw);

	/* Set Rx packet filter */
	rtl_hw_set_rx_packet_filter(hw);

	rtl_disable_cfg9346_write(hw);

	if (rtl_is_8125(hw))
		rtl_clear_rdu = rtl8125_clear_rdu;
	else
		rtl_clear_rdu = rtl8168_clear_rdu;

	/* RSS_control_0 */
	if (hw->EnableRss) {
		rtl_init_rss(hw, nb_rx_queues);
		rtl8125_config_rss(hw, nb_rx_queues);
	} else {
		RTL_W32(hw, RSS_CTRL_8125, 0x00);
	}

	/* VMQ_control */
	rtl8125_set_rx_q_num(hw, nb_rx_queues);

	RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) | CmdRxEnb);

	return 0;
}

static inline bool
rtl_rx_ip_csum(struct rtl_hw *hw, uint32_t opts1, uint32_t opts2)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		return (((opts2 & RxV4F_v3) && !(opts2 & RxIPF_v3)) ||
			(opts2 & RxV6F_v3));
	case RX_DESC_RING_TYPE_4:
		return (((opts2 & RxV4F_v4) && !(opts1 & RxIPF_v4)) ||
			(opts2 & RxV6F_v4));
	default:
		return (((opts2 & RxV4F) && !(opts1 & RxIPF)) ||
			(opts2 & RxV6F));
	}
}

static inline bool
rtl_rx_l4_csum(struct rtl_hw *hw, uint32_t opts1, uint32_t opts2)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		return (((opts2 & RxTCPT_v3) && !(opts2 & RxTCPF_v3)) ||
			((opts2 & RxUDPT_v3) && !(opts2 & RxUDPF_v3)));
	case RX_DESC_RING_TYPE_4:
		return (((opts1 & RxTCPT_v4) && !(opts1 & RxTCPF_v4)) ||
			((opts1 & RxUDPT_v4) && !(opts1 & RxUDPF_v4)));
	default:
		return (((opts1 & RxTCPT) && !(opts1 & RxTCPF)) ||
			((opts1 & RxUDPT) && !(opts1 & RxUDPF)));
	}
}

static inline uint64_t
rtl_rx_desc_error_to_pkt_flags(struct rtl_rx_queue *rxq, uint32_t opts1,
			       uint32_t opts2)
{
	uint64_t pkt_flags = 0;
	struct rtl_hw *hw = rxq->hw;

	if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM))
		goto exit;

	/* RX csum offload for RTL8169*/
	if (rtl_rx_ip_csum(hw, opts1, opts2)) {
		pkt_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		if (rtl_rx_l4_csum(hw, opts1, opts2))
			pkt_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	}

exit:
	return pkt_flags;
}

static u32
rtl_rx_desc_opts1(struct rtl_hw *hw, struct rtl_rx_desc *desc)
{
	uint32_t opts1;

	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		opts1 = ((struct rtl_rx_descv3 *)desc)->RxDescNormalDDWord4.opts1;
		break;
	case RX_DESC_RING_TYPE_4:
		opts1 = ((struct rtl_rx_descv4 *)desc)->RxDescNormalDDWord2.opts1;
		break;
	default:
		opts1 = desc->opts1;
		break;
	}

	return rte_le_to_cpu_32(opts1);
}

static u32
rtl_rx_desc_opts2(struct rtl_hw *hw, struct rtl_rx_desc *desc)
{
	uint32_t opts2;

	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		opts2 = ((struct rtl_rx_descv3 *)desc)->RxDescNormalDDWord4.opts2;
		break;
	case RX_DESC_RING_TYPE_4:
		opts2 = ((struct rtl_rx_descv4 *)desc)->RxDescNormalDDWord2.opts2;
		break;
	default:
		opts2 = desc->opts2;
		break;
	}

	return rte_le_to_cpu_32(opts2);
}

static u32
rtl_get_rx_desc_hash(struct rtl_hw *hw, struct rtl_rx_desc *desc)
{
	u32 rss = 0;

	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		rss = ((struct rtl_rx_descv3 *)desc)->RxDescNormalDDWord2.RSSResult;
		break;
	case RX_DESC_RING_TYPE_4:
		rss = ((struct rtl_rx_descv4 *)desc)->RxDescNormalDDWord1.RSSResult;
		break;
	}

	return rte_le_to_cpu_32(rss);
}

static inline int
rtl_check_rx_desc_error(struct rtl_hw *hw, uint32_t opts1)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		return (opts1 & RxRES_V3);
	case RX_DESC_RING_TYPE_4:
		return (opts1 & RxRES_V4);
	default:
		return (opts1 & RxRES);
	}
}

/* PMD receive function */
uint16_t
rtl_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct rtl_rx_queue *rxq = (struct rtl_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	struct rtl_hw *hw = rxq->hw;
	struct rtl_rx_desc *rxd;
	struct rtl_rx_entry *rxe;
	struct rtl_rx_entry *sw_ring = rxq->sw_ring;
	struct rte_mbuf *new_mb;
	struct rte_mbuf *rmb;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_sw_stats *stats = &adapter->sw_stats;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;
	uint16_t tail = rxq->rx_tail;
	uint16_t pkt_len = 0;
	const uint16_t nb_rx_desc = rxq->nb_rx_desc;
	uint32_t opts1;
	uint32_t opts2;
	uint64_t dma_addr;

	RTE_ASSERT(RTL_R8(hw, ChipCmd) & CmdRxEnb);

	while (nb_rx < nb_pkts) {
		rxd = rtl_get_rxdesc(hw, rxq->hw_ring, tail);

		opts1 = rtl_rx_desc_opts1(hw, rxd);
		if (opts1 & DescOwn)
			break;

		/*
		 * This barrier is needed to keep us from reading
		 * any other fields out of the Rx descriptor until
		 * we know the status of DescOwn.
		 */
		rte_rmb();

		if (unlikely(rtl_check_rx_desc_error(hw, opts1))) {
			stats->rx_errors++;
			rtl_mark_to_asic(hw, rxd);
			nb_hold++;
			tail = (tail + 1) % nb_rx_desc;
			continue;
		}

		opts2 = rtl_rx_desc_opts2(hw, rxd);

		new_mb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (!new_mb) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u",
				   (u32)rxq->port_id, (u32)rxq->queue_id);
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
			rte_prefetch0(rtl_get_rxdesc(hw, rxq->hw_ring, tail));
		}

		/* Refill the RX desc */
		rxe->mbuf = new_mb;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mb));

		/* Setup RX descriptor */
		rtl_map_to_asic(hw, rxd, dma_addr);

		pkt_len = opts1 & 0x00003fff;
		pkt_len -= RTE_ETHER_CRC_LEN;

		rmb->data_off = RTE_PKTMBUF_HEADROOM;
		rte_prefetch1((char *)rmb->buf_addr + rmb->data_off);
		rmb->nb_segs = 1;
		rmb->next = NULL;
		rmb->pkt_len = pkt_len;
		rmb->data_len = pkt_len;
		rmb->port = rxq->port_id;

		if (hw->EnableRss)
			rmb->hash.rss = rtl_get_rx_desc_hash(hw, rxd);

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

	rxq->rx_tail = tail;

	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rte_wmb();

		/* Clear RDU */
		rtl_clear_rdu(hw, rxq->queue_id);

		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

static inline int
rtl_is_non_eop(struct rtl_hw *hw, u32 opts1)
{
	switch (hw->HwSuppRxDescType) {
	case RX_DESC_RING_TYPE_3:
		return !(opts1 & LastFrag_V3);
	case RX_DESC_RING_TYPE_4:
		return !(opts1 & LastFrag_V4);
	default:
		return !(opts1 & LastFrag);
	}
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

	/*
	 * Retrieve RX context of current packet, if any.
	 */
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;

	RTE_ASSERT(RTL_R8(hw, ChipCmd) & CmdRxEnb);

	while (nb_rx < nb_pkts) {
next_desc:
		rxd = rtl_get_rxdesc(hw, rxq->hw_ring, tail);

		opts1 = rtl_rx_desc_opts1(hw, rxd);
		if (opts1 & DescOwn)
			break;

		/*
		 * This barrier is needed to keep us from reading
		 * any other fields out of the Rx descriptor until
		 * we know the status of DescOwn.
		 */
		rte_rmb();

		if (unlikely(rtl_check_rx_desc_error(hw, opts1))) {
			stats->rx_errors++;
			rtl_mark_to_asic(hw, rxd);
			nb_hold++;
			tail = (tail + 1) % nb_rx_desc;
			continue;
		}

		opts2 = rtl_rx_desc_opts2(hw, rxd);

		new_mb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (!new_mb) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u",
				   (u32)rxq->port_id, (u32)rxq->queue_id);
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
			rte_prefetch0(rtl_get_rxdesc(hw, rxq->hw_ring, tail));
		}

		/* Refill the RX desc */
		rxe->mbuf = new_mb;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mb));

		/* Setup RX descriptor */
		rtl_map_to_asic(hw, rxd, dma_addr);

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
		if (!first_seg) {
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
		if (rtl_is_non_eop(hw, opts1)) {
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
		if (hw->EnableRss)
			first_seg->hash.rss = rtl_get_rx_desc_hash(hw, rxd);

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
		rtl_clear_rdu(hw, rxq->queue_id);

		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;

	return nb_rx;
}

/* ---------------------------------TX---------------------------------- */
static void
rtl_tx_queue_release_mbufs(struct rtl_tx_queue *txq)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	if (txq != NULL) {
		if (txq->sw_ring != NULL) {
			for (i = 0; i < txq->nb_tx_desc; i++) {
				if (txq->sw_ring[i].mbuf != NULL) {
					rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
					txq->sw_ring[i].mbuf = NULL;
				}
			}
		}
	}
}

void
rtl_tx_queue_release(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct rtl_tx_queue *txq = dev->data->tx_queues[tx_queue_id];

	PMD_INIT_FUNC_TRACE();

	if (txq != NULL) {
		rtl_tx_queue_release_mbufs(txq);
		rte_free(txq->sw_ring);
		rte_free(txq);
	}
}

void
rtl_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		 struct rte_eth_txq_info *qinfo)
{
	struct rtl_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;
}

static void
rtl_reset_tx_queue(struct rtl_tx_queue *txq)
{
	static const struct rtl_tx_desc zero_txd = {0};
	int i;

	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->hw_ring[i] = zero_txd;

	txq->hw_ring[txq->nb_tx_desc - 1].opts1 = rte_cpu_to_le_32(RingEnd);

	txq->tx_tail = 0;
	txq->tx_head = 0;
	txq->tx_free = txq->nb_tx_desc - 1;

	/* EnableTxNoClose */
	txq->NextHwDesCloPtr = 0;
	txq->BeginHwDesCloPtr = 0;
}

uint64_t
rtl_get_tx_port_offloads(void)
{
	uint64_t tx_offload_capa;

	tx_offload_capa = RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
			  RTE_ETH_TX_OFFLOAD_IPV4_CKSUM  |
			  RTE_ETH_TX_OFFLOAD_UDP_CKSUM   |
			  RTE_ETH_TX_OFFLOAD_TCP_CKSUM   |
			  RTE_ETH_TX_OFFLOAD_TCP_TSO     |
			  RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	return tx_offload_capa;
}

int
rtl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		   uint16_t nb_tx_desc, unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct rtl_tx_queue *txq;
	const struct rte_memzone *mz;
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	u32 size;

	PMD_INIT_FUNC_TRACE();

	/*
	 * If this queue existed already, free the associated memory. The
	 * queue cannot be reused in case we need to allocate memory on
	 * different socket than was previously used.
	 */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		rtl_tx_queue_release(dev, queue_idx);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	txq = rte_zmalloc_socket("r8169 TX queue", sizeof(struct rtl_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);

	if (!txq) {
		PMD_INIT_LOG(ERR, "Cannot allocate Tx queue structure");
		return -ENOMEM;
	}

	/* Setup queue */
	txq->nb_tx_desc = nb_tx_desc;
	txq->port_id = dev->data->port_id;
	txq->queue_id = queue_idx;
	txq->tx_free_thresh = tx_conf->tx_free_thresh;

	/* Allocate memory for the software ring */
	txq->sw_ring = rte_calloc("r8169 sw tx ring", nb_tx_desc,
				  sizeof(struct rtl_tx_entry), RTE_CACHE_LINE_SIZE);

	if (txq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR,
			     "Port %d: Cannot allocate software ring for queue %d",
			     txq->port_id, txq->queue_id);
		rte_free(txq);
		return -ENOMEM;
	}

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	size = sizeof(struct rtl_tx_desc) * (nb_tx_desc + 1);
	mz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, size,
				      RTL_RING_ALIGN, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR,
			     "Port %d: Cannot allocate hardware ring for queue %d",
			     txq->port_id, txq->queue_id);
		rtl_tx_queue_release(dev, txq->queue_id);
		return -ENOMEM;
	}

	txq->hw = hw;
	txq->hw_ring = mz->addr;
	txq->hw_ring_phys_addr = mz->iova;

	rtl_reset_tx_queue(txq);

	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

static void
rtl8125_set_tx_tag_num(struct rtl_hw *hw)
{
	u32 mac_ocp_data;

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE614);
	mac_ocp_data &= ~(BIT_10 | BIT_9 | BIT_8);
	switch (hw->mcfg) {
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_53:
		mac_ocp_data |= (2 << 8);
		break;
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		if (hw->EnableTxNoClose)
			mac_ocp_data |= (4 << 8);
		else
			mac_ocp_data |= (3 << 8);
		break;
	case CFG_METHOD_91:
		mac_ocp_data |= (15 << 8);
		break;
	default:
		mac_ocp_data |= (3 << 8);
		break;
	}
	rtl_mac_ocp_write(hw, 0xE614, mac_ocp_data);
}

/* Set MTPS: Max Tx Pkt Size */
static void
rtl8168_set_mtps(struct rtl_hw *hw)
{
	if (hw->mtu > RTE_ETHER_MTU)
		RTL_W8(hw, MTPS, 0x27);
	else
		RTL_W8(hw, MTPS, 0x3F);
}

static void
rtl8125_set_tx_q_num(struct rtl_hw *hw, unsigned int num_tx_queues)
{
	u16 mac_ocp_data;

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE63E);
	mac_ocp_data &= ~(BIT_11 | BIT_10);
	mac_ocp_data |= ((rte_log2_u32(num_tx_queues) & 0x03) << 10);
	rtl_mac_ocp_write(hw, 0xE63E, mac_ocp_data);
}

int
rtl_tx_init(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	struct rtl_tx_queue *txq;
	u16 nb_tx_queues = dev->data->nb_tx_queues;
	u16 desc_start_addr_lo[QUEUE_NUM_LIMIT];
	u16 hw_clo_ptr0_reg, sw_tail_ptr0_reg, reg_len;
	int i;

	desc_start_addr_lo[0] = TxDescStartAddrLow;
	for (i = 1; i < nb_tx_queues; i++)
		desc_start_addr_lo[i] = (u16)(TNPDS_Q1_LOW_8125 + (i - 1) * 8);

	switch (hw->HwSuppTxNoCloseVer) {
	case 4:
	case 5:
		hw_clo_ptr0_reg = HW_CLO_PTR0_8126;
		sw_tail_ptr0_reg = SW_TAIL_PTR0_8126;
		reg_len = 4;
		break;
	case 6:
		hw_clo_ptr0_reg = HW_CLO_PTR0_8125BP;
		sw_tail_ptr0_reg = SW_TAIL_PTR0_8125BP;
		reg_len = 8;
		break;
	default:
		hw_clo_ptr0_reg = HW_CLO_PTR0_8125;
		sw_tail_ptr0_reg = SW_TAIL_PTR0_8125;
		reg_len = 4;
		break;
	}

	for (i = 0; i < nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		RTL_W32(hw, desc_start_addr_lo[i],
			(u64)txq->hw_ring_phys_addr & DMA_BIT_MASK(32));
		RTL_W32(hw, desc_start_addr_lo[i] + 4,
			(u64)txq->hw_ring_phys_addr >> 32);

		txq->hw_clo_ptr_reg = (u16)(hw_clo_ptr0_reg + i * reg_len);
		txq->sw_tail_ptr_reg = (u16)(sw_tail_ptr0_reg + i * reg_len);

		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	rtl_enable_cfg9346_write(hw);

	if (rtl_is_8125(hw)) {
		rtl8125_set_tx_tag_num(hw);

		RTL_W8(hw, TDFNR, 0x10);
	} else {
		rtl8168_set_mtps(hw);

		RTL_W8(hw, TDFNR, 0x4);
	}

	rtl_disable_cfg9346_write(hw);

	rtl8125_set_tx_q_num(hw, nb_tx_queues);

	RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) | CmdTxEnb);

	return 0;
}

static inline uint32_t
rtl_tx_vlan_tag(struct rte_mbuf *tx_pkt, uint64_t ol_flags)
{
	return (ol_flags & RTE_MBUF_F_TX_VLAN) ?
	       (TxVlanTag | rte_bswap16(tx_pkt->vlan_tci)) :
	       0;
}

static inline int
rtl_tso_setup(struct rte_mbuf *tx_pkt, uint64_t ol_flags, u32 *opts)
{
	uint32_t mss;
	uint64_t l4_offset;

	/* Check if TCP segmentation required for this packet */
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		mss = tx_pkt->tso_segsz;
		l4_offset = tx_pkt->l2_len + tx_pkt->l3_len;
		if (l4_offset <= GTTCPHO_MAX) {
			/* Implies IP cksum in IPv4 */
			if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
				opts[0] |= GiantSendv4;
			else
				opts[0] |= GiantSendv6;

			opts[0] |= l4_offset << GTTCPHO_SHIFT;
			opts[1] |= RTE_MIN(mss, MSS_MAX) << 18;

			return 1;
		}
	}

	return 0;
}

static inline void
rtl_setup_csum_offload(struct rte_mbuf *tx_pkt, uint64_t ol_flags,
		       uint32_t *opts)
{
	uint32_t csum_cmd = 0;
	uint64_t l4_offset;

	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
		csum_cmd |= TxIPCS_C;

	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_UDP_CKSUM:
		csum_cmd |= TxUDPCS_C;
		break;
	case RTE_MBUF_F_TX_TCP_CKSUM:
		csum_cmd |= TxTCPCS_C;
		break;
	}

	if (csum_cmd != 0) {
		if (ol_flags & RTE_MBUF_F_TX_IPV6) {
			l4_offset = tx_pkt->l2_len + tx_pkt->l3_len;
			csum_cmd |= TxIPV6F_C;
			csum_cmd |= l4_offset << TCPHO_SHIFT;
		} else {
			csum_cmd |= TxIPCS_C;
		}
		opts[1] |= csum_cmd;
	}
}

static uint32_t
rtl8125_get_patch_pad_len(struct rte_mbuf *tx_pkt)
{
	uint16_t dest_port;
	uint32_t pad_len = 0;
	int udp_hdr_len = sizeof(struct rte_udp_hdr);
	int trans_data_len, l4_offset;
	struct rte_udp_hdr *udp_hdr;

	if (!(tx_pkt->l4_len && tx_pkt->data_len < 175))
		goto no_padding;

	l4_offset = tx_pkt->l2_len + tx_pkt->l3_len;
	trans_data_len = tx_pkt->data_len - l4_offset;

	if (trans_data_len > 3 && trans_data_len < MIN_PATCH_LENGTH) {
		udp_hdr = rte_pktmbuf_mtod_offset(tx_pkt, struct rte_udp_hdr *, l4_offset);
		dest_port = ntohs(udp_hdr->dst_port);
		if (dest_port == 0x13f || dest_port == 0x140) {
			pad_len = MIN_PATCH_LENGTH - trans_data_len;
			goto out;
		}
	}

	if (trans_data_len < udp_hdr_len)
		pad_len = udp_hdr_len - trans_data_len;

out:
	if ((tx_pkt->data_len + pad_len) < ETH_ZLEN)
		pad_len = ETH_ZLEN - tx_pkt->data_len;

	return pad_len;

no_padding:

	return 0;
}

static void
rtl8125_ptp_patch(struct rte_mbuf *tx_pkt)
{
	uint32_t pad_len;
	char *padding;

	if (tx_pkt->packet_type & RTE_PTYPE_L4_UDP) {
		pad_len = rtl8125_get_patch_pad_len(tx_pkt);
		if (pad_len > 0) {
			padding = rte_pktmbuf_append(tx_pkt, pad_len);
			if (unlikely(padding == NULL))
				PMD_DRV_LOG(ERR, "Not enough mbuf trailing space.");
			memset(padding, 0, pad_len);
		}
	}
}

static inline void
rtl_xmit_pkt(struct rtl_hw *hw, struct rtl_tx_queue *txq,
	     struct rte_mbuf *tx_pkt)
{
	struct rte_mbuf *m_seg;
	struct rte_eth_dev *dev = &rte_eth_devices[txq->port_id];
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_sw_stats *stats = &adapter->sw_stats;
	struct rtl_tx_desc *txd;
	struct rtl_tx_entry *txe = NULL;
	uint16_t desc_count = 0;
	const uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t tail;
	u32 len;
	u32 opts[2] = {0};
	u32 opts1;
	u32 opts2;
	int large_send;
	uint64_t buf_dma_addr;
	uint64_t ol_flags;
	uint64_t tx_ol_flags;

	/* Like cur_tx */
	tail = (uint16_t)(txq->tx_tail % nb_tx_desc);

	/* If hardware offload required */
	ol_flags = tx_pkt->ol_flags;
	tx_ol_flags = ol_flags & RTL_TX_OFFLOAD_MASK;

	opts[0] = DescOwn;
	opts[1] = rtl_tx_vlan_tag(tx_pkt, tx_ol_flags);

	large_send = rtl_tso_setup(tx_pkt, tx_ol_flags, opts);

	/* No TSO */
	if (large_send == 0) {
		rtl_setup_csum_offload(tx_pkt, tx_ol_flags, opts);

		if (hw->mcfg >= CFG_METHOD_48 && hw->mcfg <= CFG_METHOD_53)
			rtl8125_ptp_patch(tx_pkt);
	}

	for (m_seg = tx_pkt; m_seg; m_seg = m_seg->next) {
		opts1 = opts[0];
		opts2 = opts[1];

		len = m_seg->data_len;

		if (len == 0)
			break;

		txd = &txq->hw_ring[tail];

		buf_dma_addr = rte_mbuf_data_iova(m_seg);
		txd->addr = rte_cpu_to_le_64(buf_dma_addr);

		opts1 |= len;
		if (m_seg == tx_pkt)
			opts1 |= FirstFrag;
		if (!m_seg->next)
			opts1 |= LastFrag;
		if (tail == nb_tx_desc - 1)
			opts1 |= RingEnd;

		/* Store mbuf for freeing later */
		txe = &txq->sw_ring[tail];

		if (txe->mbuf)
			rte_pktmbuf_free_seg(txe->mbuf);

		txe->mbuf = m_seg;

		txd->opts2 = rte_cpu_to_le_32(opts2);
		rte_wmb();
		txd->opts1 = rte_cpu_to_le_32(opts1);

		tail = (tail + 1) % nb_tx_desc;

		desc_count++;

		stats->tx_bytes += len;
	}

	txq->tx_tail += desc_count;
	txq->tx_free -= desc_count;

	stats->tx_packets++;
}

static inline u32
rtl_fast_mod_mask(const u32 input, const u32 mask)
{
	return input > mask ? input & mask : input;
}

static u32
rtl_get_hw_clo_ptr(struct rtl_tx_queue *txq)
{
	struct rtl_hw *hw = txq->hw;

	switch (hw->HwSuppTxNoCloseVer) {
	case 3:
		return RTL_R16(hw, txq->hw_clo_ptr_reg);
	case 4:
	case 5:
	case 6:
		return RTL_R32(hw, txq->hw_clo_ptr_reg);
	default:
		return 0;
	}
}

static u32
rtl_get_opts1(struct rtl_tx_desc *txd)
{
	u32 opts1 = rte_atomic_load_explicit(&txd->opts1, rte_memory_order_acquire);

	return rte_le_to_cpu_32(opts1);
}

static void
rtl8125_tx_clean(struct rtl_hw *hw, struct rtl_tx_queue *txq)
{
	struct rtl_tx_entry *sw_ring = txq->sw_ring;
	struct rtl_tx_entry *txe;
	struct rtl_tx_desc *txd;
	const uint8_t enable_tx_no_close = hw->EnableTxNoClose;
	const uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t head = txq->tx_head;
	uint16_t desc_freed = 0;
	uint32_t tx_left;
	uint32_t tx_desc_closed;

	if (enable_tx_no_close) {
		txq->NextHwDesCloPtr = rtl_get_hw_clo_ptr(txq);
		tx_desc_closed = rtl_fast_mod_mask(txq->NextHwDesCloPtr -
						   txq->BeginHwDesCloPtr,
						   hw->MaxTxDescPtrMask);
		tx_left = RTE_MIN(((rte_atomic_load_explicit(&txq->tx_tail,
				    rte_memory_order_relaxed) % nb_tx_desc) - head),
				    tx_desc_closed);
		txq->BeginHwDesCloPtr += tx_left;
	} else {
		tx_left = (rte_atomic_load_explicit(&txq->tx_tail,
						    rte_memory_order_relaxed) % nb_tx_desc) - head;
	}

	while (tx_left > 0) {
		txd = &txq->hw_ring[head];

		if (!enable_tx_no_close && (rtl_get_opts1(txd) & DescOwn))
			break;

		txe = &sw_ring[head];
		if (txe->mbuf) {
			rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = NULL;
		}

		head = (head + 1) % nb_tx_desc;
		desc_freed++;
		tx_left--;
	}
	txq->tx_free += desc_freed;
	txq->tx_head = head;
}

static void
rtl8168_tx_clean(struct rtl_hw *hw __rte_unused, struct rtl_tx_queue *txq)
{
	struct rtl_tx_entry *sw_ring = txq->sw_ring;
	struct rtl_tx_entry *txe;
	struct rtl_tx_desc *txd;
	const uint16_t nb_tx_desc = txq->nb_tx_desc;
	const int tx_tail = txq->tx_tail % nb_tx_desc;
	int head = txq->tx_head;
	uint16_t desc_freed = 0;

	while (1) {
		txd = &txq->hw_ring[head];

		if (rtl_get_opts1(txd) & DescOwn)
			break;

		txe = &sw_ring[head];
		if (txe->mbuf) {
			rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = NULL;
		}

		head = (head + 1) % nb_tx_desc;
		desc_freed++;

		if (head == tx_tail)
			break;
	}

	txq->tx_free += desc_freed;
	txq->tx_head = head;
}

static void
rtl_tx_clean(struct rtl_hw *hw, struct rtl_tx_queue *txq)
{
	if (rtl_is_8125(hw))
		rtl8125_tx_clean(hw, txq);
	else
		rtl8168_tx_clean(hw, txq);
}

static int
rtl8125_tx_done_cleanup(void *tx_queue, uint32_t free_cnt)
{
	struct rtl_tx_queue *txq = tx_queue;
	struct rtl_hw *hw;
	struct rtl_tx_entry *sw_ring;
	struct rtl_tx_entry *txe;
	struct rtl_tx_desc *txd;
	uint8_t enable_tx_no_close;
	uint16_t nb_tx_desc;
	uint16_t head;
	uint16_t desc_freed = 0;
	uint32_t tx_left;
	uint32_t count = 0;
	uint32_t status;
	uint32_t tx_desc_closed;

	if (!txq)
		return -ENODEV;

	hw = txq->hw;
	enable_tx_no_close = hw->EnableTxNoClose;
	sw_ring = txq->sw_ring;
	nb_tx_desc = txq->nb_tx_desc;
	head = txq->tx_head;

	if (enable_tx_no_close) {
		txq->NextHwDesCloPtr = rtl_get_hw_clo_ptr(txq);
		tx_desc_closed = rtl_fast_mod_mask(txq->NextHwDesCloPtr -
						   txq->BeginHwDesCloPtr,
						   hw->MaxTxDescPtrMask);
		tx_left = RTE_MIN(((rte_atomic_load_explicit(&txq->tx_tail,
				    rte_memory_order_relaxed) % nb_tx_desc) - head),
				    tx_desc_closed);
		txq->BeginHwDesCloPtr += tx_left;
	} else {
		tx_left = (rte_atomic_load_explicit(&txq->tx_tail,
						    rte_memory_order_relaxed) % nb_tx_desc) - head;
	}

	while (tx_left > 0) {
		txd = &txq->hw_ring[head];

		status = rtl_get_opts1(txd);

		if (!enable_tx_no_close && (status & DescOwn))
			break;

		txe = &sw_ring[head];
		if (txe->mbuf) {
			rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = NULL;
		}

		head = (head + 1) % nb_tx_desc;

		desc_freed++;
		tx_left--;

		if (status & LastFrag) {
			count++;
			if (count == free_cnt)
				break;
		}
	}

	txq->tx_free += desc_freed;
	txq->tx_head = head;

	return count;
}

static int
rtl8168_tx_done_cleanup(void *tx_queue, uint32_t free_cnt)
{
	struct rtl_tx_queue *txq = tx_queue;
	struct rtl_tx_entry *sw_ring;
	struct rtl_tx_entry *txe;
	struct rtl_tx_desc *txd;
	uint16_t nb_tx_desc;
	uint16_t desc_freed = 0;
	uint32_t status;
	int tx_tail;
	int head;
	int count = 0;

	if (!txq)
		return -ENODEV;

	sw_ring = txq->sw_ring;
	nb_tx_desc = txq->nb_tx_desc;
	tx_tail = txq->tx_tail % nb_tx_desc;
	head = txq->tx_head;

	while (1) {
		txd = &txq->hw_ring[head];

		status = rtl_get_opts1(txd);

		if (status & DescOwn)
			break;

		txe = &sw_ring[head];
		if (txe->mbuf) {
			rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = NULL;
		}

		head = (head + 1) % nb_tx_desc;
		desc_freed++;

		if (status & LastFrag) {
			count++;
			if ((uint32_t)count == free_cnt)
				break;
		}

		if (head == tx_tail)
			break;
	}

	txq->tx_free += desc_freed;
	txq->tx_head = head;

	return count;
}

int
rtl_tx_done_cleanup(void *tx_queue, uint32_t free_cnt)
{
	struct rtl_tx_queue *txq = tx_queue;
	struct rtl_hw *hw = txq->hw;

	if (rtl_is_8125(hw))
		return rtl8125_tx_done_cleanup(tx_queue, free_cnt);
	else
		return rtl8168_tx_done_cleanup(tx_queue, free_cnt);
}

static void
rtl8125_doorbell(struct rtl_hw *hw, struct rtl_tx_queue *txq)
{
	if (hw->EnableTxNoClose)
		if (hw->HwSuppTxNoCloseVer > 3)
			RTL_W32(hw, txq->sw_tail_ptr_reg, txq->tx_tail);
		else
			RTL_W16(hw, txq->sw_tail_ptr_reg, txq->tx_tail);
	else
		RTL_W16(hw, TPPOLL_8125, BIT_0);
}

static void
rtl8168_doorbell(struct rtl_hw *hw)
{
	RTL_W8(hw, TxPoll, NPQ);
}

static void
rtl_doorbell(struct rtl_hw *hw, struct rtl_tx_queue *txq)
{
	if (rtl_is_8125(hw))
		rtl8125_doorbell(hw, txq);
	else
		rtl8168_doorbell(hw);
}

/* PMD transmit function */
uint16_t
rtl_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rtl_tx_queue *txq = tx_queue;
	struct rtl_hw *hw = txq->hw;
	struct rte_mbuf *tx_pkt;
	uint16_t nb_tx;

	RTE_ASSERT(RTL_R8(hw, ChipCmd) & CmdTxEnb);

	PMD_TX_LOG(DEBUG,
		   "port %d txq %d pkts: %d tx_free=%d tx_tail=%d tx_head=%d",
		   txq->port_id, txq->queue_id, nb_pkts, txq->tx_free,
		   txq->tx_tail, txq->tx_head);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;

		if (txq->tx_free < tx_pkt->nb_segs)
			break;

		/* Check mbuf is valid */
		if (tx_pkt->nb_segs == 0 || tx_pkt->pkt_len == 0 ||
		    (tx_pkt->nb_segs > 1 && tx_pkt->next == NULL))
			break;

		rtl_xmit_pkt(hw, txq, tx_pkt);
	}

	rte_wmb();

	if (nb_tx > 0)
		rtl_doorbell(hw, txq);

	PMD_TX_LOG(DEBUG, "%s %d transmitted", __func__, nb_tx);

	rtl_tx_clean(hw, txq);

	return nb_tx;
}

int
rtl_stop_queues(struct rte_eth_dev *dev)
{
	struct rtl_tx_queue *txq;
	struct rtl_rx_queue *rxq;
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		rtl_tx_queue_release_mbufs(txq);
		rtl_reset_tx_queue(txq);
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		rtl_rx_queue_release_mbufs(rxq);
		rtl_reset_rx_queue(rxq);
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
	return 0;
}

void
rtl_free_queues(struct rte_eth_dev *dev)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		rte_eth_dma_zone_free(dev, "tx_ring", i);
		rtl_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = 0;
	}
	dev->data->nb_tx_queues = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rte_eth_dma_zone_free(dev, "rx_ring", i);
		rtl_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = 0;
	}
	dev->data->nb_rx_queues = 0;
}
