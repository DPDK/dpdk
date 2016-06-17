/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <sys/timerfd.h>

#include <rte_alarm.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_pci.h>
#include <rte_tailq.h>

#include "base/nicvf_plat.h"

#include "nicvf_ethdev.h"
#include "nicvf_rxtx.h"
#include "nicvf_logs.h"

static inline int
nicvf_atomic_write_link_status(struct rte_eth_dev *dev,
			       struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
		*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline void
nicvf_set_eth_link_status(struct nicvf *nic, struct rte_eth_link *link)
{
	link->link_status = nic->link_up;
	link->link_duplex = ETH_LINK_AUTONEG;
	if (nic->duplex == NICVF_HALF_DUPLEX)
		link->link_duplex = ETH_LINK_HALF_DUPLEX;
	else if (nic->duplex == NICVF_FULL_DUPLEX)
		link->link_duplex = ETH_LINK_FULL_DUPLEX;
	link->link_speed = nic->speed;
	link->link_autoneg = ETH_LINK_SPEED_AUTONEG;
}

static void
nicvf_interrupt(void *arg)
{
	struct nicvf *nic = arg;

	if (nicvf_reg_poll_interrupts(nic) == NIC_MBOX_MSG_BGX_LINK_CHANGE) {
		if (nic->eth_dev->data->dev_conf.intr_conf.lsc)
			nicvf_set_eth_link_status(nic,
					&nic->eth_dev->data->dev_link);
		_rte_eth_dev_callback_process(nic->eth_dev,
				RTE_ETH_EVENT_INTR_LSC);
	}

	rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000,
				nicvf_interrupt, nic);
}

static int
nicvf_periodic_alarm_start(struct nicvf *nic)
{
	return rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000,
					nicvf_interrupt, nic);
}

static int
nicvf_periodic_alarm_stop(struct nicvf *nic)
{
	return rte_eal_alarm_cancel(nicvf_interrupt, nic);
}

/*
 * Return 0 means link status changed, -1 means not changed
 */
static int
nicvf_dev_link_update(struct rte_eth_dev *dev,
		      int wait_to_complete __rte_unused)
{
	struct rte_eth_link link;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	memset(&link, 0, sizeof(link));
	nicvf_set_eth_link_status(nic, &link);
	return nicvf_atomic_write_link_status(dev, &link);
}

static int
nicvf_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint32_t buffsz, frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	PMD_INIT_FUNC_TRACE();

	if (frame_size > NIC_HW_MAX_FRS)
		return -EINVAL;

	if (frame_size < NIC_HW_MIN_FRS)
		return -EINVAL;

	buffsz = dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM;

	/*
	 * Refuse mtu that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (!dev->data->scattered_rx &&
		(frame_size + 2 * VLAN_TAG_SIZE > buffsz))
		return -EINVAL;

	/* check <seg size> * <max_seg>  >= max_frame */
	if (dev->data->scattered_rx &&
		(frame_size + 2 * VLAN_TAG_SIZE > buffsz * NIC_HW_MAX_SEGS))
		return -EINVAL;

	if (frame_size > ETHER_MAX_LEN)
		dev->data->dev_conf.rxmode.jumbo_frame = 1;
	else
		dev->data->dev_conf.rxmode.jumbo_frame = 0;

	if (nicvf_mbox_update_hw_max_frs(nic, frame_size))
		return -EINVAL;

	/* Update max frame size */
	dev->data->dev_conf.rxmode.max_rx_pkt_len = (uint32_t)frame_size;
	nic->mtu = mtu;
	return 0;
}

static int
nicvf_dev_get_reg_length(struct rte_eth_dev *dev  __rte_unused)
{
	return nicvf_reg_get_count();
}

static int
nicvf_dev_get_regs(struct rte_eth_dev *dev, struct rte_dev_reg_info *regs)
{
	uint64_t *data = regs->data;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (data == NULL)
		return -EINVAL;

	/* Support only full register dump */
	if ((regs->length == 0) ||
		(regs->length == (uint32_t)nicvf_reg_get_count())) {
		regs->version = nic->vendor_id << 16 | nic->device_id;
		nicvf_reg_dump(nic, data);
		return 0;
	}
	return -ENOTSUP;
}

static void
nicvf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	uint16_t qidx;
	struct nicvf_hw_rx_qstats rx_qstats;
	struct nicvf_hw_tx_qstats tx_qstats;
	struct nicvf_hw_stats port_stats;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	/* Reading per RX ring stats */
	for (qidx = 0; qidx < dev->data->nb_rx_queues; qidx++) {
		if (qidx == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nicvf_hw_get_rx_qstats(nic, &rx_qstats, qidx);
		stats->q_ibytes[qidx] = rx_qstats.q_rx_bytes;
		stats->q_ipackets[qidx] = rx_qstats.q_rx_packets;
	}

	/* Reading per TX ring stats */
	for (qidx = 0; qidx < dev->data->nb_tx_queues; qidx++) {
		if (qidx == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nicvf_hw_get_tx_qstats(nic, &tx_qstats, qidx);
		stats->q_obytes[qidx] = tx_qstats.q_tx_bytes;
		stats->q_opackets[qidx] = tx_qstats.q_tx_packets;
	}

	nicvf_hw_get_stats(nic, &port_stats);
	stats->ibytes = port_stats.rx_bytes;
	stats->ipackets = port_stats.rx_ucast_frames;
	stats->ipackets += port_stats.rx_bcast_frames;
	stats->ipackets += port_stats.rx_mcast_frames;
	stats->ierrors = port_stats.rx_l2_errors;
	stats->imissed = port_stats.rx_drop_red;
	stats->imissed += port_stats.rx_drop_overrun;
	stats->imissed += port_stats.rx_drop_bcast;
	stats->imissed += port_stats.rx_drop_mcast;
	stats->imissed += port_stats.rx_drop_l3_bcast;
	stats->imissed += port_stats.rx_drop_l3_mcast;

	stats->obytes = port_stats.tx_bytes_ok;
	stats->opackets = port_stats.tx_ucast_frames_ok;
	stats->opackets += port_stats.tx_bcast_frames_ok;
	stats->opackets += port_stats.tx_mcast_frames_ok;
	stats->oerrors = port_stats.tx_drops;
}

static const uint32_t *
nicvf_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	size_t copied;
	static uint32_t ptypes[32];
	struct nicvf *nic = nicvf_pmd_priv(dev);
	static const uint32_t ptypes_pass1[] = {
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
	};
	static const uint32_t ptypes_pass2[] = {
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_TUNNEL_GENEVE,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_NVGRE,
	};
	static const uint32_t ptypes_end = RTE_PTYPE_UNKNOWN;

	copied = sizeof(ptypes_pass1);
	memcpy(ptypes, ptypes_pass1, copied);
	if (nicvf_hw_version(nic) == NICVF_PASS2) {
		memcpy((char *)ptypes + copied, ptypes_pass2,
			sizeof(ptypes_pass2));
		copied += sizeof(ptypes_pass2);
	}

	memcpy((char *)ptypes + copied, &ptypes_end, sizeof(ptypes_end));
	if (dev->rx_pkt_burst == nicvf_recv_pkts ||
		dev->rx_pkt_burst == nicvf_recv_pkts_multiseg)
		return ptypes;

	return NULL;
}

static void
nicvf_dev_stats_reset(struct rte_eth_dev *dev)
{
	int i;
	uint16_t rxqs = 0, txqs = 0;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		rxqs |= (0x3 << (i * 2));
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		txqs |= (0x3 << (i * 2));

	nicvf_mbox_reset_stat_counters(nic, 0x3FFF, 0x1F, rxqs, txqs);
}

/* Promiscuous mode enabled by default in LMAC to VF 1:1 map configuration */
static void
nicvf_dev_promisc_enable(struct rte_eth_dev *dev __rte_unused)
{
}

static inline uint64_t
nicvf_rss_ethdev_to_nic(struct nicvf *nic, uint64_t ethdev_rss)
{
	uint64_t nic_rss = 0;

	if (ethdev_rss & ETH_RSS_IPV4)
		nic_rss |= RSS_IP_ENA;

	if (ethdev_rss & ETH_RSS_IPV6)
		nic_rss |= RSS_IP_ENA;

	if (ethdev_rss & ETH_RSS_NONFRAG_IPV4_UDP)
		nic_rss |= (RSS_IP_ENA | RSS_UDP_ENA);

	if (ethdev_rss & ETH_RSS_NONFRAG_IPV4_TCP)
		nic_rss |= (RSS_IP_ENA | RSS_TCP_ENA);

	if (ethdev_rss & ETH_RSS_NONFRAG_IPV6_UDP)
		nic_rss |= (RSS_IP_ENA | RSS_UDP_ENA);

	if (ethdev_rss & ETH_RSS_NONFRAG_IPV6_TCP)
		nic_rss |= (RSS_IP_ENA | RSS_TCP_ENA);

	if (ethdev_rss & ETH_RSS_PORT)
		nic_rss |= RSS_L2_EXTENDED_HASH_ENA;

	if (nicvf_hw_cap(nic) & NICVF_CAP_TUNNEL_PARSING) {
		if (ethdev_rss & ETH_RSS_VXLAN)
			nic_rss |= RSS_TUN_VXLAN_ENA;

		if (ethdev_rss & ETH_RSS_GENEVE)
			nic_rss |= RSS_TUN_GENEVE_ENA;

		if (ethdev_rss & ETH_RSS_NVGRE)
			nic_rss |= RSS_TUN_NVGRE_ENA;
	}

	return nic_rss;
}

static inline uint64_t
nicvf_rss_nic_to_ethdev(struct nicvf *nic,  uint64_t nic_rss)
{
	uint64_t ethdev_rss = 0;

	if (nic_rss & RSS_IP_ENA)
		ethdev_rss |= (ETH_RSS_IPV4 | ETH_RSS_IPV6);

	if ((nic_rss & RSS_IP_ENA) && (nic_rss & RSS_TCP_ENA))
		ethdev_rss |= (ETH_RSS_NONFRAG_IPV4_TCP |
				ETH_RSS_NONFRAG_IPV6_TCP);

	if ((nic_rss & RSS_IP_ENA) && (nic_rss & RSS_UDP_ENA))
		ethdev_rss |= (ETH_RSS_NONFRAG_IPV4_UDP |
				ETH_RSS_NONFRAG_IPV6_UDP);

	if (nic_rss & RSS_L2_EXTENDED_HASH_ENA)
		ethdev_rss |= ETH_RSS_PORT;

	if (nicvf_hw_cap(nic) & NICVF_CAP_TUNNEL_PARSING) {
		if (nic_rss & RSS_TUN_VXLAN_ENA)
			ethdev_rss |= ETH_RSS_VXLAN;

		if (nic_rss & RSS_TUN_GENEVE_ENA)
			ethdev_rss |= ETH_RSS_GENEVE;

		if (nic_rss & RSS_TUN_NVGRE_ENA)
			ethdev_rss |= ETH_RSS_NVGRE;
	}
	return ethdev_rss;
}

static int
nicvf_dev_reta_query(struct rte_eth_dev *dev,
		     struct rte_eth_rss_reta_entry64 *reta_conf,
		     uint16_t reta_size)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint8_t tbl[NIC_MAX_RSS_IDR_TBL_SIZE];
	int ret, i, j;

	if (reta_size != NIC_MAX_RSS_IDR_TBL_SIZE) {
		RTE_LOG(ERR, PMD, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, NIC_MAX_RSS_IDR_TBL_SIZE);
		return -EINVAL;
	}

	ret = nicvf_rss_reta_query(nic, tbl, NIC_MAX_RSS_IDR_TBL_SIZE);
	if (ret)
		return ret;

	/* Copy RETA table */
	for (i = 0; i < (NIC_MAX_RSS_IDR_TBL_SIZE / RTE_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_RETA_GROUP_SIZE; j++)
			if ((reta_conf[i].mask >> j) & 0x01)
				reta_conf[i].reta[j] = tbl[j];
	}

	return 0;
}

static int
nicvf_dev_reta_update(struct rte_eth_dev *dev,
		      struct rte_eth_rss_reta_entry64 *reta_conf,
		      uint16_t reta_size)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint8_t tbl[NIC_MAX_RSS_IDR_TBL_SIZE];
	int ret, i, j;

	if (reta_size != NIC_MAX_RSS_IDR_TBL_SIZE) {
		RTE_LOG(ERR, PMD, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, NIC_MAX_RSS_IDR_TBL_SIZE);
		return -EINVAL;
	}

	ret = nicvf_rss_reta_query(nic, tbl, NIC_MAX_RSS_IDR_TBL_SIZE);
	if (ret)
		return ret;

	/* Copy RETA table */
	for (i = 0; i < (NIC_MAX_RSS_IDR_TBL_SIZE / RTE_RETA_GROUP_SIZE); i++) {
		for (j = 0; j < RTE_RETA_GROUP_SIZE; j++)
			if ((reta_conf[i].mask >> j) & 0x01)
				tbl[j] = reta_conf[i].reta[j];
	}

	return nicvf_rss_reta_update(nic, tbl, NIC_MAX_RSS_IDR_TBL_SIZE);
}

static int
nicvf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (rss_conf->rss_key)
		nicvf_rss_get_key(nic, rss_conf->rss_key);

	rss_conf->rss_key_len =  RSS_HASH_KEY_BYTE_SIZE;
	rss_conf->rss_hf = nicvf_rss_nic_to_ethdev(nic, nicvf_rss_get_cfg(nic));
	return 0;
}

static int
nicvf_dev_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint64_t nic_rss;

	if (rss_conf->rss_key &&
		rss_conf->rss_key_len != RSS_HASH_KEY_BYTE_SIZE) {
		RTE_LOG(ERR, PMD, "Hash key size mismatch %d",
				rss_conf->rss_key_len);
		return -EINVAL;
	}

	if (rss_conf->rss_key)
		nicvf_rss_set_key(nic, rss_conf->rss_key);

	nic_rss = nicvf_rss_ethdev_to_nic(nic, rss_conf->rss_hf);
	nicvf_rss_set_cfg(nic, nic_rss);
	return 0;
}

static int
nicvf_qset_cq_alloc(struct nicvf *nic, struct nicvf_rxq *rxq, uint16_t qidx,
		    uint32_t desc_cnt)
{
	const struct rte_memzone *rz;
	uint32_t ring_size = desc_cnt * sizeof(union cq_entry_t);

	rz = rte_eth_dma_zone_reserve(nic->eth_dev, "cq_ring", qidx, ring_size,
					NICVF_CQ_BASE_ALIGN_BYTES, nic->node);
	if (rz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate mem for cq hw ring");
		return -ENOMEM;
	}

	memset(rz->addr, 0, ring_size);

	rxq->phys = rz->phys_addr;
	rxq->desc = rz->addr;
	rxq->qlen_mask = desc_cnt - 1;

	return 0;
}

static int
nicvf_qset_sq_alloc(struct nicvf *nic,  struct nicvf_txq *sq, uint16_t qidx,
		    uint32_t desc_cnt)
{
	const struct rte_memzone *rz;
	uint32_t ring_size = desc_cnt * sizeof(union sq_entry_t);

	rz = rte_eth_dma_zone_reserve(nic->eth_dev, "sq", qidx, ring_size,
				NICVF_SQ_BASE_ALIGN_BYTES, nic->node);
	if (rz == NULL) {
		PMD_INIT_LOG(ERR, "Failed allocate mem for sq hw ring");
		return -ENOMEM;
	}

	memset(rz->addr, 0, ring_size);

	sq->phys = rz->phys_addr;
	sq->desc = rz->addr;
	sq->qlen_mask = desc_cnt - 1;

	return 0;
}

static inline void
nicvf_tx_queue_release_mbufs(struct nicvf_txq *txq)
{
	uint32_t head;

	head = txq->head;
	while (head != txq->tail) {
		if (txq->txbuffs[head]) {
			rte_pktmbuf_free_seg(txq->txbuffs[head]);
			txq->txbuffs[head] = NULL;
		}
		head++;
		head = head & txq->qlen_mask;
	}
}

static void
nicvf_tx_queue_reset(struct nicvf_txq *txq)
{
	uint32_t txq_desc_cnt = txq->qlen_mask + 1;

	memset(txq->desc, 0, sizeof(union sq_entry_t) * txq_desc_cnt);
	memset(txq->txbuffs, 0, sizeof(struct rte_mbuf *) * txq_desc_cnt);
	txq->tail = 0;
	txq->head = 0;
	txq->xmit_bufs = 0;
}

static void
nicvf_dev_tx_queue_release(void *sq)
{
	struct nicvf_txq *txq;

	PMD_INIT_FUNC_TRACE();

	txq = (struct nicvf_txq *)sq;
	if (txq) {
		if (txq->txbuffs != NULL) {
			nicvf_tx_queue_release_mbufs(txq);
			rte_free(txq->txbuffs);
			txq->txbuffs = NULL;
		}
		rte_free(txq);
	}
}

static int
nicvf_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t qidx,
			 uint16_t nb_desc, unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	uint16_t tx_free_thresh;
	uint8_t is_single_pool;
	struct nicvf_txq *txq;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	/* Socket id check */
	if (socket_id != (unsigned int)SOCKET_ID_ANY && socket_id != nic->node)
		PMD_DRV_LOG(WARNING, "socket_id expected %d, configured %d",
		socket_id, nic->node);

	/* Tx deferred start is not supported */
	if (tx_conf->tx_deferred_start) {
		PMD_INIT_LOG(ERR, "Tx deferred start not supported");
		return -EINVAL;
	}

	/* Roundup nb_desc to available qsize and validate max number of desc */
	nb_desc = nicvf_qsize_sq_roundup(nb_desc);
	if (nb_desc == 0) {
		PMD_INIT_LOG(ERR, "Value of nb_desc beyond available sq qsize");
		return -EINVAL;
	}

	/* Validate tx_free_thresh */
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
				tx_conf->tx_free_thresh :
				NICVF_DEFAULT_TX_FREE_THRESH);

	if (tx_free_thresh > (nb_desc) ||
		tx_free_thresh > NICVF_MAX_TX_FREE_THRESH) {
		PMD_INIT_LOG(ERR,
			"tx_free_thresh must be less than the number of TX "
			"descriptors. (tx_free_thresh=%u port=%d "
			"queue=%d)", (unsigned int)tx_free_thresh,
			(int)dev->data->port_id, (int)qidx);
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->tx_queues[qidx] != NULL) {
		PMD_TX_LOG(DEBUG, "Freeing memory prior to re-allocation %d",
				qidx);
		nicvf_dev_tx_queue_release(dev->data->tx_queues[qidx]);
		dev->data->tx_queues[qidx] = NULL;
	}

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct nicvf_txq),
					RTE_CACHE_LINE_SIZE, nic->node);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate txq=%d", qidx);
		return -ENOMEM;
	}

	txq->nic = nic;
	txq->queue_id = qidx;
	txq->tx_free_thresh = tx_free_thresh;
	txq->txq_flags = tx_conf->txq_flags;
	txq->sq_head = nicvf_qset_base(nic, qidx) + NIC_QSET_SQ_0_7_HEAD;
	txq->sq_door = nicvf_qset_base(nic, qidx) + NIC_QSET_SQ_0_7_DOOR;
	is_single_pool = (txq->txq_flags & ETH_TXQ_FLAGS_NOREFCOUNT &&
				txq->txq_flags & ETH_TXQ_FLAGS_NOMULTMEMP);

	/* Choose optimum free threshold value for multipool case */
	if (!is_single_pool) {
		txq->tx_free_thresh = (uint16_t)
		(tx_conf->tx_free_thresh == NICVF_DEFAULT_TX_FREE_THRESH ?
				NICVF_TX_FREE_MPOOL_THRESH :
				tx_conf->tx_free_thresh);
		txq->pool_free = nicvf_multi_pool_free_xmited_buffers;
	} else {
		txq->pool_free = nicvf_single_pool_free_xmited_buffers;
	}

	/* Allocate software ring */
	txq->txbuffs = rte_zmalloc_socket("txq->txbuffs",
				nb_desc * sizeof(struct rte_mbuf *),
				RTE_CACHE_LINE_SIZE, nic->node);

	if (txq->txbuffs == NULL) {
		nicvf_dev_tx_queue_release(txq);
		return -ENOMEM;
	}

	if (nicvf_qset_sq_alloc(nic, txq, qidx, nb_desc)) {
		PMD_INIT_LOG(ERR, "Failed to allocate mem for sq %d", qidx);
		nicvf_dev_tx_queue_release(txq);
		return -ENOMEM;
	}

	nicvf_tx_queue_reset(txq);

	PMD_TX_LOG(DEBUG, "[%d] txq=%p nb_desc=%d desc=%p phys=0x%" PRIx64,
			qidx, txq, nb_desc, txq->desc, txq->phys);

	dev->data->tx_queues[qidx] = txq;
	dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static void
nicvf_rx_queue_reset(struct nicvf_rxq *rxq)
{
	rxq->head = 0;
	rxq->available_space = 0;
	rxq->recv_buffers = 0;
}

static void
nicvf_dev_rx_queue_release(void *rx_queue)
{
	struct nicvf_rxq *rxq = rx_queue;

	PMD_INIT_FUNC_TRACE();

	if (rxq)
		rte_free(rxq);
}

static int
nicvf_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t qidx,
			 uint16_t nb_desc, unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	uint16_t rx_free_thresh;
	struct nicvf_rxq *rxq;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	/* Socket id check */
	if (socket_id != (unsigned int)SOCKET_ID_ANY && socket_id != nic->node)
		PMD_DRV_LOG(WARNING, "socket_id expected %d, configured %d",
		socket_id, nic->node);

	/* Mempool memory should be contiguous */
	if (mp->nb_mem_chunks != 1) {
		PMD_INIT_LOG(ERR, "Non contiguous mempool, check huge page sz");
		return -EINVAL;
	}

	/* Rx deferred start is not supported */
	if (rx_conf->rx_deferred_start) {
		PMD_INIT_LOG(ERR, "Rx deferred start not supported");
		return -EINVAL;
	}

	/* Roundup nb_desc to available qsize and validate max number of desc */
	nb_desc = nicvf_qsize_cq_roundup(nb_desc);
	if (nb_desc == 0) {
		PMD_INIT_LOG(ERR, "Value nb_desc beyond available hw cq qsize");
		return -EINVAL;
	}

	/* Check rx_free_thresh upper bound */
	rx_free_thresh = (uint16_t)((rx_conf->rx_free_thresh) ?
				rx_conf->rx_free_thresh :
				NICVF_DEFAULT_RX_FREE_THRESH);
	if (rx_free_thresh > NICVF_MAX_RX_FREE_THRESH ||
		rx_free_thresh >= nb_desc * .75) {
		PMD_INIT_LOG(ERR, "rx_free_thresh greater than expected %d",
				rx_free_thresh);
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed */
	if (dev->data->rx_queues[qidx] != NULL) {
		PMD_RX_LOG(DEBUG, "Freeing memory prior to re-allocation %d",
				qidx);
		nicvf_dev_rx_queue_release(dev->data->rx_queues[qidx]);
		dev->data->rx_queues[qidx] = NULL;
	}

	/* Allocate rxq memory */
	rxq = rte_zmalloc_socket("ethdev rx queue", sizeof(struct nicvf_rxq),
					RTE_CACHE_LINE_SIZE, nic->node);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate rxq=%d", qidx);
		return -ENOMEM;
	}

	rxq->nic = nic;
	rxq->pool = mp;
	rxq->queue_id = qidx;
	rxq->port_id = dev->data->port_id;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->rx_drop_en = rx_conf->rx_drop_en;
	rxq->cq_status = nicvf_qset_base(nic, qidx) + NIC_QSET_CQ_0_7_STATUS;
	rxq->cq_door = nicvf_qset_base(nic, qidx) + NIC_QSET_CQ_0_7_DOOR;
	rxq->precharge_cnt = 0;
	rxq->rbptr_offset = NICVF_CQE_RBPTR_WORD;

	/* Alloc completion queue */
	if (nicvf_qset_cq_alloc(nic, rxq, rxq->queue_id, nb_desc)) {
		PMD_INIT_LOG(ERR, "failed to allocate cq %u", rxq->queue_id);
		nicvf_dev_rx_queue_release(rxq);
		return -ENOMEM;
	}

	nicvf_rx_queue_reset(rxq);

	PMD_RX_LOG(DEBUG, "[%d] rxq=%p pool=%s nb_desc=(%d/%d) phy=%" PRIx64,
			qidx, rxq, mp->name, nb_desc,
			rte_mempool_count(mp), rxq->phys);

	dev->data->rx_queues[qidx] = rxq;
	dev->data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static void
nicvf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	dev_info->min_rx_bufsize = ETHER_MIN_MTU;
	dev_info->max_rx_pktlen = NIC_HW_MAX_FRS;
	dev_info->max_rx_queues = (uint16_t)MAX_RCV_QUEUES_PER_QS;
	dev_info->max_tx_queues = (uint16_t)MAX_SND_QUEUES_PER_QS;
	dev_info->max_mac_addrs = 1;
	dev_info->max_vfs = dev->pci_dev->max_vfs;

	dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_IPV4_CKSUM  |
		DEV_TX_OFFLOAD_UDP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_TSO     |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;

	dev_info->reta_size = nic->rss_info.rss_size;
	dev_info->hash_key_size = RSS_HASH_KEY_BYTE_SIZE;
	dev_info->flow_type_rss_offloads = NICVF_RSS_OFFLOAD_PASS1;
	if (nicvf_hw_cap(nic) & NICVF_CAP_TUNNEL_PARSING)
		dev_info->flow_type_rss_offloads |= NICVF_RSS_OFFLOAD_TUNNEL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = NICVF_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = NICVF_DEFAULT_TX_FREE_THRESH,
		.txq_flags =
			ETH_TXQ_FLAGS_NOMULTSEGS  |
			ETH_TXQ_FLAGS_NOREFCOUNT  |
			ETH_TXQ_FLAGS_NOMULTMEMP  |
			ETH_TXQ_FLAGS_NOVLANOFFL  |
			ETH_TXQ_FLAGS_NOXSUMSCTP,
	};
}

static int
nicvf_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	if (!rte_eal_has_hugepages()) {
		PMD_INIT_LOG(INFO, "Huge page is not configured");
		return -EINVAL;
	}

	if (txmode->mq_mode) {
		PMD_INIT_LOG(INFO, "Tx mq_mode DCB or VMDq not supported");
		return -EINVAL;
	}

	if (rxmode->mq_mode != ETH_MQ_RX_NONE &&
		rxmode->mq_mode != ETH_MQ_RX_RSS) {
		PMD_INIT_LOG(INFO, "Unsupported rx qmode %d", rxmode->mq_mode);
		return -EINVAL;
	}

	if (!rxmode->hw_strip_crc) {
		PMD_INIT_LOG(NOTICE, "Can't disable hw crc strip");
		rxmode->hw_strip_crc = 1;
	}

	if (rxmode->hw_ip_checksum) {
		PMD_INIT_LOG(NOTICE, "Rxcksum not supported");
		rxmode->hw_ip_checksum = 0;
	}

	if (rxmode->split_hdr_size) {
		PMD_INIT_LOG(INFO, "Rxmode does not support split header");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_filter) {
		PMD_INIT_LOG(INFO, "VLAN filter not supported");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_extend) {
		PMD_INIT_LOG(INFO, "VLAN extended not supported");
		return -EINVAL;
	}

	if (rxmode->enable_lro) {
		PMD_INIT_LOG(INFO, "LRO not supported");
		return -EINVAL;
	}

	if (conf->link_speeds & ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(INFO, "Setting link speed/duplex not supported");
		return -EINVAL;
	}

	if (conf->dcb_capability_en) {
		PMD_INIT_LOG(INFO, "DCB enable not supported");
		return -EINVAL;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		PMD_INIT_LOG(INFO, "Flow director not supported");
		return -EINVAL;
	}

	PMD_INIT_LOG(DEBUG, "Configured ethdev port%d hwcap=0x%" PRIx64,
		dev->data->port_id, nicvf_hw_cap(nic));

	return 0;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops nicvf_eth_dev_ops = {
	.dev_configure            = nicvf_dev_configure,
	.link_update              = nicvf_dev_link_update,
	.stats_get                = nicvf_dev_stats_get,
	.stats_reset              = nicvf_dev_stats_reset,
	.promiscuous_enable       = nicvf_dev_promisc_enable,
	.dev_infos_get            = nicvf_dev_info_get,
	.dev_supported_ptypes_get = nicvf_dev_supported_ptypes_get,
	.mtu_set                  = nicvf_dev_set_mtu,
	.reta_update              = nicvf_dev_reta_update,
	.reta_query               = nicvf_dev_reta_query,
	.rss_hash_update          = nicvf_dev_rss_hash_update,
	.rss_hash_conf_get        = nicvf_dev_rss_hash_conf_get,
	.rx_queue_setup           = nicvf_dev_rx_queue_setup,
	.rx_queue_release         = nicvf_dev_rx_queue_release,
	.tx_queue_setup           = nicvf_dev_tx_queue_setup,
	.tx_queue_release         = nicvf_dev_tx_queue_release,
	.get_reg_length           = nicvf_dev_get_reg_length,
	.get_reg                  = nicvf_dev_get_regs,
};

static int
nicvf_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	int ret;
	struct rte_pci_device *pci_dev;
	struct nicvf *nic = nicvf_pmd_priv(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &nicvf_eth_dev_ops;

	pci_dev = eth_dev->pci_dev;
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	nic->device_id = pci_dev->id.device_id;
	nic->vendor_id = pci_dev->id.vendor_id;
	nic->subsystem_device_id = pci_dev->id.subsystem_device_id;
	nic->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	nic->eth_dev = eth_dev;

	PMD_INIT_LOG(DEBUG, "nicvf: device (%x:%x) %u:%u:%u:%u",
			pci_dev->id.vendor_id, pci_dev->id.device_id,
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);

	nic->reg_base = (uintptr_t)pci_dev->mem_resource[0].addr;
	if (!nic->reg_base) {
		PMD_INIT_LOG(ERR, "Failed to map BAR0");
		ret = -ENODEV;
		goto fail;
	}

	nicvf_disable_all_interrupts(nic);

	ret = nicvf_periodic_alarm_start(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to start period alarm");
		goto fail;
	}

	ret = nicvf_mbox_check_pf_ready(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get ready message from PF");
		goto alarm_fail;
	} else {
		PMD_INIT_LOG(INFO,
			"node=%d vf=%d mode=%s sqs=%s loopback_supported=%s",
			nic->node, nic->vf_id,
			nic->tns_mode == NIC_TNS_MODE ? "tns" : "tns-bypass",
			nic->sqs_mode ? "true" : "false",
			nic->loopback_supported ? "true" : "false"
			);
	}

	if (nic->sqs_mode) {
		PMD_INIT_LOG(INFO, "Unsupported SQS VF detected, Detaching...");
		/* Detach port by returning Positive error number */
		ret = ENOTSUP;
		goto alarm_fail;
	}

	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for mac addr");
		ret = -ENOMEM;
		goto alarm_fail;
	}
	if (is_zero_ether_addr((struct ether_addr *)nic->mac_addr))
		eth_random_addr(&nic->mac_addr[0]);

	ether_addr_copy((struct ether_addr *)nic->mac_addr,
			&eth_dev->data->mac_addrs[0]);

	ret = nicvf_mbox_set_mac_addr(nic, nic->mac_addr);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to set mac addr");
		goto malloc_fail;
	}

	ret = nicvf_base_init(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to execute nicvf_base_init");
		goto malloc_fail;
	}

	ret = nicvf_mbox_get_rss_size(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get rss table size");
		goto malloc_fail;
	}

	PMD_INIT_LOG(INFO, "Port %d (%x:%x) mac=%02x:%02x:%02x:%02x:%02x:%02x",
		eth_dev->data->port_id, nic->vendor_id, nic->device_id,
		nic->mac_addr[0], nic->mac_addr[1], nic->mac_addr[2],
		nic->mac_addr[3], nic->mac_addr[4], nic->mac_addr[5]);

	return 0;

malloc_fail:
	rte_free(eth_dev->data->mac_addrs);
alarm_fail:
	nicvf_periodic_alarm_stop(nic);
fail:
	return ret;
}

static const struct rte_pci_id pci_id_nicvf_map[] = {
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_PASS1_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_THUNDERX_PASS1_NICVF,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_PASS2_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_THUNDERX_PASS2_NICVF,
	},
	{
		.vendor_id = 0,
	},
};

static struct eth_driver rte_nicvf_pmd = {
	.pci_drv = {
		.name = "rte_nicvf_pmd",
		.id_table = pci_id_nicvf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	},
	.eth_dev_init = nicvf_eth_dev_init,
	.dev_private_size = sizeof(struct nicvf),
};

static int
rte_nicvf_pmd_init(const char *name __rte_unused, const char *para __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	PMD_INIT_LOG(INFO, "librte_pmd_thunderx nicvf version %s",
			THUNDERX_NICVF_PMD_VERSION);

	rte_eth_driver_register(&rte_nicvf_pmd);
	return 0;
}

static struct rte_driver rte_nicvf_driver = {
	.name = "nicvf_driver",
	.type = PMD_PDEV,
	.init = rte_nicvf_pmd_init,
};

PMD_REGISTER_DRIVER(rte_nicvf_driver);
