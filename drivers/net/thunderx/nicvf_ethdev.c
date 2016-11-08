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
#include "nicvf_svf.h"
#include "nicvf_logs.h"

static void nicvf_dev_stop(struct rte_eth_dev *dev);
static void nicvf_dev_stop_cleanup(struct rte_eth_dev *dev, bool cleanup);
static void nicvf_vf_stop(struct rte_eth_dev *dev, struct nicvf *nic,
			  bool cleanup);

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
	struct rte_eth_dev *dev = arg;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (nicvf_reg_poll_interrupts(nic) == NIC_MBOX_MSG_BGX_LINK_CHANGE) {
		if (dev->data->dev_conf.intr_conf.lsc)
			nicvf_set_eth_link_status(nic, &dev->data->dev_link);
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	}

	rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000,
				nicvf_interrupt, dev);
}

static void
nicvf_vf_interrupt(void *arg)
{
	struct nicvf *nic = arg;

	nicvf_reg_poll_interrupts(nic);

	rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000,
				nicvf_vf_interrupt, nic);
}

static int
nicvf_periodic_alarm_start(void (fn)(void *), void *arg)
{
	return rte_eal_alarm_set(NICVF_INTR_POLL_INTERVAL_MS * 1000, fn, arg);
}

static int
nicvf_periodic_alarm_stop(void (fn)(void *), void *arg)
{
	return rte_eal_alarm_cancel(fn, arg);
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
	size_t i;

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

	for (i = 0; i < nic->sqs_count; i++)
		nic->snicvf[i]->mtu = mtu;

	return 0;
}

static int
nicvf_dev_get_regs(struct rte_eth_dev *dev, struct rte_dev_reg_info *regs)
{
	uint64_t *data = regs->data;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (data == NULL) {
		regs->length = nicvf_reg_get_count();
		regs->width = THUNDERX_REG_BYTES;
		return 0;
	}

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
	uint16_t rx_start, rx_end;
	uint16_t tx_start, tx_end;
	size_t i;

	/* RX queue indices for the first VF */
	nicvf_rx_range(dev, nic, &rx_start, &rx_end);

	/* Reading per RX ring stats */
	for (qidx = rx_start; qidx <= rx_end; qidx++) {
		if (qidx == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nicvf_hw_get_rx_qstats(nic, &rx_qstats, qidx);
		stats->q_ibytes[qidx] = rx_qstats.q_rx_bytes;
		stats->q_ipackets[qidx] = rx_qstats.q_rx_packets;
	}

	/* TX queue indices for the first VF */
	nicvf_tx_range(dev, nic, &tx_start, &tx_end);

	/* Reading per TX ring stats */
	for (qidx = tx_start; qidx <= tx_end; qidx++) {
		if (qidx == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nicvf_hw_get_tx_qstats(nic, &tx_qstats, qidx);
		stats->q_obytes[qidx] = tx_qstats.q_tx_bytes;
		stats->q_opackets[qidx] = tx_qstats.q_tx_packets;
	}

	for (i = 0; i < nic->sqs_count; i++) {
		struct nicvf *snic = nic->snicvf[i];

		if (snic == NULL)
			break;

		/* RX queue indices for a secondary VF */
		nicvf_rx_range(dev, snic, &rx_start, &rx_end);

		/* Reading per RX ring stats */
		for (qidx = rx_start; qidx <= rx_end; qidx++) {
			if (qidx == RTE_ETHDEV_QUEUE_STAT_CNTRS)
				break;

			nicvf_hw_get_rx_qstats(snic, &rx_qstats,
					       qidx % MAX_RCV_QUEUES_PER_QS);
			stats->q_ibytes[qidx] = rx_qstats.q_rx_bytes;
			stats->q_ipackets[qidx] = rx_qstats.q_rx_packets;
		}

		/* TX queue indices for a secondary VF */
		nicvf_tx_range(dev, snic, &tx_start, &tx_end);
		/* Reading per TX ring stats */
		for (qidx = tx_start; qidx <= tx_end; qidx++) {
			if (qidx == RTE_ETHDEV_QUEUE_STAT_CNTRS)
				break;

			nicvf_hw_get_tx_qstats(snic, &tx_qstats,
					       qidx % MAX_SND_QUEUES_PER_QS);
			stats->q_obytes[qidx] = tx_qstats.q_tx_bytes;
			stats->q_opackets[qidx] = tx_qstats.q_tx_packets;
		}
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
	static const uint32_t ptypes_common[] = {
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
	};
	static const uint32_t ptypes_tunnel[] = {
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_TUNNEL_GENEVE,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_NVGRE,
	};
	static const uint32_t ptypes_end = RTE_PTYPE_UNKNOWN;

	copied = sizeof(ptypes_common);
	memcpy(ptypes, ptypes_common, copied);
	if (nicvf_hw_cap(nic) & NICVF_CAP_TUNNEL_PARSING) {
		memcpy((char *)ptypes + copied, ptypes_tunnel,
			sizeof(ptypes_tunnel));
		copied += sizeof(ptypes_tunnel);
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
	uint16_t rx_start, rx_end;
	uint16_t tx_start, tx_end;

	/* Reset all primary nic counters */
	nicvf_rx_range(dev, nic, &rx_start, &rx_end);
	for (i = rx_start; i <= rx_end; i++)
		rxqs |= (0x3 << (i * 2));

	nicvf_tx_range(dev, nic, &tx_start, &tx_end);
	for (i = tx_start; i <= tx_end; i++)
		txqs |= (0x3 << (i * 2));

	nicvf_mbox_reset_stat_counters(nic, 0x3FFF, 0x1F, rxqs, txqs);

	/* Reset secondary nic queue counters */
	for (i = 0; i < nic->sqs_count; i++) {
		struct nicvf *snic = nic->snicvf[i];
		if (snic == NULL)
			break;

		nicvf_rx_range(dev, snic, &rx_start, &rx_end);
		for (i = rx_start; i <= rx_end; i++)
			rxqs |= (0x3 << ((i % MAX_CMP_QUEUES_PER_QS) * 2));

		nicvf_tx_range(dev, snic, &tx_start, &tx_end);
		for (i = tx_start; i <= tx_end; i++)
			txqs |= (0x3 << ((i % MAX_SND_QUEUES_PER_QS) * 2));

		nicvf_mbox_reset_stat_counters(snic, 0, 0, rxqs, txqs);
	}
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
nicvf_qset_cq_alloc(struct rte_eth_dev *dev, struct nicvf *nic,
		    struct nicvf_rxq *rxq, uint16_t qidx, uint32_t desc_cnt)
{
	const struct rte_memzone *rz;
	uint32_t ring_size = CMP_QUEUE_SZ_MAX * sizeof(union cq_entry_t);

	rz = rte_eth_dma_zone_reserve(dev, "cq_ring",
				      nicvf_netdev_qidx(nic, qidx), ring_size,
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
nicvf_qset_sq_alloc(struct rte_eth_dev *dev, struct nicvf *nic,
		    struct nicvf_txq *sq, uint16_t qidx, uint32_t desc_cnt)
{
	const struct rte_memzone *rz;
	uint32_t ring_size = SND_QUEUE_SZ_MAX * sizeof(union sq_entry_t);

	rz = rte_eth_dma_zone_reserve(dev, "sq",
				      nicvf_netdev_qidx(nic, qidx), ring_size,
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

static int
nicvf_qset_rbdr_alloc(struct rte_eth_dev *dev, struct nicvf *nic,
		      uint32_t desc_cnt, uint32_t buffsz)
{
	struct nicvf_rbdr *rbdr;
	const struct rte_memzone *rz;
	uint32_t ring_size;

	assert(nic->rbdr == NULL);
	rbdr = rte_zmalloc_socket("rbdr", sizeof(struct nicvf_rbdr),
				  RTE_CACHE_LINE_SIZE, nic->node);
	if (rbdr == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate mem for rbdr");
		return -ENOMEM;
	}

	ring_size = sizeof(struct rbdr_entry_t) * RBDR_QUEUE_SZ_MAX;
	rz = rte_eth_dma_zone_reserve(dev, "rbdr",
				      nicvf_netdev_qidx(nic, 0), ring_size,
				      NICVF_RBDR_BASE_ALIGN_BYTES, nic->node);
	if (rz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate mem for rbdr desc ring");
		return -ENOMEM;
	}

	memset(rz->addr, 0, ring_size);

	rbdr->phys = rz->phys_addr;
	rbdr->tail = 0;
	rbdr->next_tail = 0;
	rbdr->desc = rz->addr;
	rbdr->buffsz = buffsz;
	rbdr->qlen_mask = desc_cnt - 1;
	rbdr->rbdr_status =
		nicvf_qset_base(nic, 0) + NIC_QSET_RBDR_0_1_STATUS0;
	rbdr->rbdr_door =
		nicvf_qset_base(nic, 0) + NIC_QSET_RBDR_0_1_DOOR;

	nic->rbdr = rbdr;
	return 0;
}

static void
nicvf_rbdr_release_mbuf(struct rte_eth_dev *dev, struct nicvf *nic,
			nicvf_phys_addr_t phy)
{
	uint16_t qidx;
	void *obj;
	struct nicvf_rxq *rxq;
	uint16_t rx_start, rx_end;

	/* Get queue ranges for this VF */
	nicvf_rx_range(dev, nic, &rx_start, &rx_end);

	for (qidx = rx_start; qidx <= rx_end; qidx++) {
		rxq = dev->data->rx_queues[qidx];
		if (rxq->precharge_cnt) {
			obj = (void *)nicvf_mbuff_phy2virt(phy,
							   rxq->mbuf_phys_off);
			rte_mempool_put(rxq->pool, obj);
			rxq->precharge_cnt--;
			break;
		}
	}
}

static inline void
nicvf_rbdr_release_mbufs(struct rte_eth_dev *dev, struct nicvf *nic)
{
	uint32_t qlen_mask, head;
	struct rbdr_entry_t *entry;
	struct nicvf_rbdr *rbdr = nic->rbdr;

	qlen_mask = rbdr->qlen_mask;
	head = rbdr->head;
	while (head != rbdr->tail) {
		entry = rbdr->desc + head;
		nicvf_rbdr_release_mbuf(dev, nic, entry->full_addr);
		head++;
		head = head & qlen_mask;
	}
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

static inline int
nicvf_vf_start_tx_queue(struct rte_eth_dev *dev, struct nicvf *nic,
			uint16_t qidx)
{
	struct nicvf_txq *txq;
	int ret;

	assert(qidx < MAX_SND_QUEUES_PER_QS);

	if (dev->data->tx_queue_state[nicvf_netdev_qidx(nic, qidx)] ==
		RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	txq = dev->data->tx_queues[nicvf_netdev_qidx(nic, qidx)];
	txq->pool = NULL;
	ret = nicvf_qset_sq_config(nic, qidx, txq);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to configure sq VF%d %d %d",
			     nic->vf_id, qidx, ret);
		goto config_sq_error;
	}

	dev->data->tx_queue_state[nicvf_netdev_qidx(nic, qidx)] =
		RTE_ETH_QUEUE_STATE_STARTED;
	return ret;

config_sq_error:
	nicvf_qset_sq_reclaim(nic, qidx);
	return ret;
}

static inline int
nicvf_vf_stop_tx_queue(struct rte_eth_dev *dev, struct nicvf *nic,
		       uint16_t qidx)
{
	struct nicvf_txq *txq;
	int ret;

	assert(qidx < MAX_SND_QUEUES_PER_QS);

	if (dev->data->tx_queue_state[nicvf_netdev_qidx(nic, qidx)] ==
		RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	ret = nicvf_qset_sq_reclaim(nic, qidx);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to reclaim sq VF%d %d %d",
			     nic->vf_id, qidx, ret);

	txq = dev->data->tx_queues[nicvf_netdev_qidx(nic, qidx)];
	nicvf_tx_queue_release_mbufs(txq);
	nicvf_tx_queue_reset(txq);

	dev->data->tx_queue_state[nicvf_netdev_qidx(nic, qidx)] =
		RTE_ETH_QUEUE_STATE_STOPPED;
	return ret;
}

static inline int
nicvf_configure_cpi(struct rte_eth_dev *dev)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint16_t qidx, qcnt;
	int ret;

	/* Count started rx queues */
	for (qidx = qcnt = 0; qidx < dev->data->nb_rx_queues; qidx++)
		if (dev->data->rx_queue_state[qidx] ==
		    RTE_ETH_QUEUE_STATE_STARTED)
			qcnt++;

	nic->cpi_alg = CPI_ALG_NONE;
	ret = nicvf_mbox_config_cpi(nic, qcnt);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to configure CPI %d", ret);

	return ret;
}

static inline int
nicvf_configure_rss(struct rte_eth_dev *dev)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint64_t rsshf;
	int ret = -EINVAL;

	rsshf = nicvf_rss_ethdev_to_nic(nic,
			dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf);
	PMD_DRV_LOG(INFO, "mode=%d rx_queues=%d loopback=%d rsshf=0x%" PRIx64,
		    dev->data->dev_conf.rxmode.mq_mode,
		    dev->data->nb_rx_queues,
		    dev->data->dev_conf.lpbk_mode, rsshf);

	if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_NONE)
		ret = nicvf_rss_term(nic);
	else if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS)
		ret = nicvf_rss_config(nic, dev->data->nb_rx_queues, rsshf);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to configure RSS %d", ret);

	return ret;
}

static int
nicvf_configure_rss_reta(struct rte_eth_dev *dev)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	unsigned int idx, qmap_size;
	uint8_t qmap[RTE_MAX_QUEUES_PER_PORT];
	uint8_t default_reta[NIC_MAX_RSS_IDR_TBL_SIZE];

	if (nic->cpi_alg != CPI_ALG_NONE)
		return -EINVAL;

	/* Prepare queue map */
	for (idx = 0, qmap_size = 0; idx < dev->data->nb_rx_queues; idx++) {
		if (dev->data->rx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STARTED)
			qmap[qmap_size++] = idx;
	}

	/* Update default RSS RETA */
	for (idx = 0; idx < NIC_MAX_RSS_IDR_TBL_SIZE; idx++)
		default_reta[idx] = qmap[idx % qmap_size];

	return nicvf_rss_reta_update(nic, default_reta,
				     NIC_MAX_RSS_IDR_TBL_SIZE);
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

static void
nicvf_set_tx_function(struct rte_eth_dev *dev)
{
	struct nicvf_txq *txq;
	size_t i;
	bool multiseg = false;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if ((txq->txq_flags & ETH_TXQ_FLAGS_NOMULTSEGS) == 0) {
			multiseg = true;
			break;
		}
	}

	/* Use a simple Tx queue (no offloads, no multi segs) if possible */
	if (multiseg) {
		PMD_DRV_LOG(DEBUG, "Using multi-segment tx callback");
		dev->tx_pkt_burst = nicvf_xmit_pkts_multiseg;
	} else {
		PMD_DRV_LOG(DEBUG, "Using single-segment tx callback");
		dev->tx_pkt_burst = nicvf_xmit_pkts;
	}

	if (txq->pool_free == nicvf_single_pool_free_xmited_buffers)
		PMD_DRV_LOG(DEBUG, "Using single-mempool tx free method");
	else
		PMD_DRV_LOG(DEBUG, "Using multi-mempool tx free method");
}

static void
nicvf_set_rx_function(struct rte_eth_dev *dev)
{
	if (dev->data->scattered_rx) {
		PMD_DRV_LOG(DEBUG, "Using multi-segment rx callback");
		dev->rx_pkt_burst = nicvf_recv_pkts_multiseg;
	} else {
		PMD_DRV_LOG(DEBUG, "Using single-segment rx callback");
		dev->rx_pkt_burst = nicvf_recv_pkts;
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

	if (qidx >= MAX_SND_QUEUES_PER_QS)
		nic = nic->snicvf[qidx / MAX_SND_QUEUES_PER_QS - 1];

	qidx = qidx % MAX_SND_QUEUES_PER_QS;

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
	if (dev->data->tx_queues[nicvf_netdev_qidx(nic, qidx)] != NULL) {
		PMD_TX_LOG(DEBUG, "Freeing memory prior to re-allocation %d",
				nicvf_netdev_qidx(nic, qidx));
		nicvf_dev_tx_queue_release(
			dev->data->tx_queues[nicvf_netdev_qidx(nic, qidx)]);
		dev->data->tx_queues[nicvf_netdev_qidx(nic, qidx)] = NULL;
	}

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct nicvf_txq),
					RTE_CACHE_LINE_SIZE, nic->node);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate txq=%d",
			     nicvf_netdev_qidx(nic, qidx));
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

	if (nicvf_qset_sq_alloc(dev, nic, txq, qidx, nb_desc)) {
		PMD_INIT_LOG(ERR, "Failed to allocate mem for sq %d", qidx);
		nicvf_dev_tx_queue_release(txq);
		return -ENOMEM;
	}

	nicvf_tx_queue_reset(txq);

	PMD_TX_LOG(DEBUG, "[%d] txq=%p nb_desc=%d desc=%p phys=0x%" PRIx64,
			nicvf_netdev_qidx(nic, qidx), txq, nb_desc, txq->desc,
			txq->phys);

	dev->data->tx_queues[nicvf_netdev_qidx(nic, qidx)] = txq;
	dev->data->tx_queue_state[nicvf_netdev_qidx(nic, qidx)] =
		RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static inline void
nicvf_rx_queue_release_mbufs(struct rte_eth_dev *dev, struct nicvf_rxq *rxq)
{
	uint32_t rxq_cnt;
	uint32_t nb_pkts, released_pkts = 0;
	uint32_t refill_cnt = 0;
	struct rte_mbuf *rx_pkts[NICVF_MAX_RX_FREE_THRESH];

	if (dev->rx_pkt_burst == NULL)
		return;

	while ((rxq_cnt = nicvf_dev_rx_queue_count(dev,
				nicvf_netdev_qidx(rxq->nic, rxq->queue_id)))) {
		nb_pkts = dev->rx_pkt_burst(rxq, rx_pkts,
					NICVF_MAX_RX_FREE_THRESH);
		PMD_DRV_LOG(INFO, "nb_pkts=%d  rxq_cnt=%d", nb_pkts, rxq_cnt);
		while (nb_pkts) {
			rte_pktmbuf_free_seg(rx_pkts[--nb_pkts]);
			released_pkts++;
		}
	}


	refill_cnt += nicvf_dev_rbdr_refill(dev,
			nicvf_netdev_qidx(rxq->nic, rxq->queue_id));

	PMD_DRV_LOG(INFO, "free_cnt=%d  refill_cnt=%d",
		    released_pkts, refill_cnt);
}

static void
nicvf_rx_queue_reset(struct nicvf_rxq *rxq)
{
	rxq->head = 0;
	rxq->available_space = 0;
	rxq->recv_buffers = 0;
}

static inline int
nicvf_vf_start_rx_queue(struct rte_eth_dev *dev, struct nicvf *nic,
			uint16_t qidx)
{
	struct nicvf_rxq *rxq;
	int ret;

	assert(qidx < MAX_RCV_QUEUES_PER_QS);

	if (dev->data->rx_queue_state[nicvf_netdev_qidx(nic, qidx)] ==
		RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	/* Update rbdr pointer to all rxq */
	rxq = dev->data->rx_queues[nicvf_netdev_qidx(nic, qidx)];
	rxq->shared_rbdr = nic->rbdr;

	ret = nicvf_qset_rq_config(nic, qidx, rxq);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to configure rq VF%d %d %d",
			     nic->vf_id, qidx, ret);
		goto config_rq_error;
	}
	ret = nicvf_qset_cq_config(nic, qidx, rxq);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to configure cq VF%d %d %d",
			     nic->vf_id, qidx, ret);
		goto config_cq_error;
	}

	dev->data->rx_queue_state[nicvf_netdev_qidx(nic, qidx)] =
		RTE_ETH_QUEUE_STATE_STARTED;
	return 0;

config_cq_error:
	nicvf_qset_cq_reclaim(nic, qidx);
config_rq_error:
	nicvf_qset_rq_reclaim(nic, qidx);
	return ret;
}

static inline int
nicvf_vf_stop_rx_queue(struct rte_eth_dev *dev, struct nicvf *nic,
		       uint16_t qidx)
{
	struct nicvf_rxq *rxq;
	int ret, other_error;

	if (dev->data->rx_queue_state[nicvf_netdev_qidx(nic, qidx)] ==
		RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	ret = nicvf_qset_rq_reclaim(nic, qidx);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to reclaim rq VF%d %d %d",
			     nic->vf_id, qidx, ret);

	other_error = ret;
	rxq = dev->data->rx_queues[nicvf_netdev_qidx(nic, qidx)];
	nicvf_rx_queue_release_mbufs(dev, rxq);
	nicvf_rx_queue_reset(rxq);

	ret = nicvf_qset_cq_reclaim(nic, qidx);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to reclaim cq VF%d %d %d",
			     nic->vf_id, qidx, ret);

	other_error |= ret;
	dev->data->rx_queue_state[nicvf_netdev_qidx(nic, qidx)] =
		RTE_ETH_QUEUE_STATE_STOPPED;
	return other_error;
}

static void
nicvf_dev_rx_queue_release(void *rx_queue)
{
	PMD_INIT_FUNC_TRACE();

	rte_free(rx_queue);
}

static int
nicvf_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);
	int ret;

	if (qidx >= MAX_RCV_QUEUES_PER_QS)
		nic = nic->snicvf[(qidx / MAX_RCV_QUEUES_PER_QS - 1)];

	qidx = qidx % MAX_RCV_QUEUES_PER_QS;

	ret = nicvf_vf_start_rx_queue(dev, nic, qidx);
	if (ret)
		return ret;

	ret = nicvf_configure_cpi(dev);
	if (ret)
		return ret;

	return nicvf_configure_rss_reta(dev);
}

static int
nicvf_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	int ret;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (qidx >= MAX_SND_QUEUES_PER_QS)
		nic = nic->snicvf[(qidx / MAX_SND_QUEUES_PER_QS - 1)];

	qidx = qidx % MAX_RCV_QUEUES_PER_QS;

	ret = nicvf_vf_stop_rx_queue(dev, nic, qidx);
	ret |= nicvf_configure_cpi(dev);
	ret |= nicvf_configure_rss_reta(dev);
	return ret;
}

static int
nicvf_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (qidx >= MAX_SND_QUEUES_PER_QS)
		nic = nic->snicvf[(qidx / MAX_SND_QUEUES_PER_QS - 1)];

	qidx = qidx % MAX_SND_QUEUES_PER_QS;

	return nicvf_vf_start_tx_queue(dev, nic, qidx);
}

static int
nicvf_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);

	if (qidx >= MAX_SND_QUEUES_PER_QS)
		nic = nic->snicvf[(qidx / MAX_SND_QUEUES_PER_QS - 1)];

	qidx = qidx % MAX_SND_QUEUES_PER_QS;

	return nicvf_vf_stop_tx_queue(dev, nic, qidx);
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

	if (qidx >= MAX_RCV_QUEUES_PER_QS)
		nic = nic->snicvf[qidx / MAX_RCV_QUEUES_PER_QS - 1];

	qidx = qidx % MAX_RCV_QUEUES_PER_QS;

	/* Socket id check */
	if (socket_id != (unsigned int)SOCKET_ID_ANY && socket_id != nic->node)
		PMD_DRV_LOG(WARNING, "socket_id expected %d, configured %d",
		socket_id, nic->node);

	/* Mempool memory must be contiguous, so must be one memory segment*/
	if (mp->nb_mem_chunks != 1) {
		PMD_INIT_LOG(ERR, "Non-contiguous mempool, add more huge pages");
		return -EINVAL;
	}

	/* Mempool memory must be physically contiguous */
	if (mp->flags & MEMPOOL_F_NO_PHYS_CONTIG) {
		PMD_INIT_LOG(ERR, "Mempool memory must be physically contiguous");
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
	if (dev->data->rx_queues[nicvf_netdev_qidx(nic, qidx)] != NULL) {
		PMD_RX_LOG(DEBUG, "Freeing memory prior to re-allocation %d",
				nicvf_netdev_qidx(nic, qidx));
		nicvf_dev_rx_queue_release(
			dev->data->rx_queues[nicvf_netdev_qidx(nic, qidx)]);
		dev->data->rx_queues[nicvf_netdev_qidx(nic, qidx)] = NULL;
	}

	/* Allocate rxq memory */
	rxq = rte_zmalloc_socket("ethdev rx queue", sizeof(struct nicvf_rxq),
					RTE_CACHE_LINE_SIZE, nic->node);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate rxq=%d",
			     nicvf_netdev_qidx(nic, qidx));
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

	if (nicvf_hw_cap(nic) & NICVF_CAP_CQE_RX2)
		rxq->rbptr_offset = NICVF_CQE_RX2_RBPTR_WORD;
	else
		rxq->rbptr_offset = NICVF_CQE_RBPTR_WORD;


	/* Alloc completion queue */
	if (nicvf_qset_cq_alloc(dev, nic, rxq, rxq->queue_id, nb_desc)) {
		PMD_INIT_LOG(ERR, "failed to allocate cq %u", rxq->queue_id);
		nicvf_dev_rx_queue_release(rxq);
		return -ENOMEM;
	}

	nicvf_rx_queue_reset(rxq);

	PMD_RX_LOG(DEBUG, "[%d] rxq=%p pool=%s nb_desc=(%d/%d) phy=%" PRIx64,
			nicvf_netdev_qidx(nic, qidx), rxq, mp->name, nb_desc,
			rte_mempool_avail_count(mp), rxq->phys);

	dev->data->rx_queues[nicvf_netdev_qidx(nic, qidx)] = rxq;
	dev->data->rx_queue_state[nicvf_netdev_qidx(nic, qidx)] =
		RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static void
nicvf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	dev_info->min_rx_bufsize = ETHER_MIN_MTU;
	dev_info->max_rx_pktlen = NIC_HW_MAX_FRS;
	dev_info->max_rx_queues =
			(uint16_t)MAX_RCV_QUEUES_PER_QS * (MAX_SQS_PER_VF + 1);
	dev_info->max_tx_queues =
			(uint16_t)MAX_SND_QUEUES_PER_QS * (MAX_SQS_PER_VF + 1);
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

static nicvf_phys_addr_t
rbdr_rte_mempool_get(void *dev, void *opaque)
{
	uint16_t qidx;
	uintptr_t mbuf;
	struct nicvf_rxq *rxq;
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)dev;
	struct nicvf *nic = (struct nicvf *)opaque;
	uint16_t rx_start, rx_end;

	/* Get queue ranges for this VF */
	nicvf_rx_range(eth_dev, nic, &rx_start, &rx_end);

	for (qidx = rx_start; qidx <= rx_end; qidx++) {
		rxq = eth_dev->data->rx_queues[qidx];
		/* Maintain equal buffer count across all pools */
		if (rxq->precharge_cnt >= rxq->qlen_mask)
			continue;
		rxq->precharge_cnt++;
		mbuf = (uintptr_t)rte_pktmbuf_alloc(rxq->pool);
		if (mbuf)
			return nicvf_mbuff_virt2phy(mbuf, rxq->mbuf_phys_off);
	}
	return 0;
}

static int
nicvf_vf_start(struct rte_eth_dev *dev, struct nicvf *nic, uint32_t rbdrsz)
{
	int ret;
	uint16_t qidx;
	uint32_t total_rxq_desc, nb_rbdr_desc, exp_buffs;
	uint64_t mbuf_phys_off = 0;
	struct nicvf_rxq *rxq;
	struct rte_mbuf *mbuf;
	uint16_t rx_start, rx_end;
	uint16_t tx_start, tx_end;

	PMD_INIT_FUNC_TRACE();

	/* Userspace process exited without proper shutdown in last run */
	if (nicvf_qset_rbdr_active(nic, 0))
		nicvf_vf_stop(dev, nic, false);

	/* Get queue ranges for this VF */
	nicvf_rx_range(dev, nic, &rx_start, &rx_end);

	/*
	 * Thunderx nicvf PMD can support more than one pool per port only when
	 * 1) Data payload size is same across all the pools in given port
	 * AND
	 * 2) All mbuffs in the pools are from the same hugepage
	 * AND
	 * 3) Mbuff metadata size is same across all the pools in given port
	 *
	 * This is to support existing application that uses multiple pool/port.
	 * But, the purpose of using multipool for QoS will not be addressed.
	 *
	 */

	/* Validate mempool attributes */
	for (qidx = rx_start; qidx <= rx_end; qidx++) {
		rxq = dev->data->rx_queues[qidx];
		rxq->mbuf_phys_off = nicvf_mempool_phy_offset(rxq->pool);
		mbuf = rte_pktmbuf_alloc(rxq->pool);
		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "Failed allocate mbuf VF%d qid=%d "
				     "pool=%s",
				     nic->vf_id, qidx, rxq->pool->name);
			return -ENOMEM;
		}
		rxq->mbuf_phys_off -= nicvf_mbuff_meta_length(mbuf);
		rxq->mbuf_phys_off -= RTE_PKTMBUF_HEADROOM;
		rte_pktmbuf_free(mbuf);

		if (mbuf_phys_off == 0)
			mbuf_phys_off = rxq->mbuf_phys_off;
		if (mbuf_phys_off != rxq->mbuf_phys_off) {
			PMD_INIT_LOG(ERR, "pool params not same,%s VF%d %"
				     PRIx64, rxq->pool->name, nic->vf_id,
				     mbuf_phys_off);
			return -EINVAL;
		}
	}

	/* Check the level of buffers in the pool */
	total_rxq_desc = 0;
	for (qidx = rx_start; qidx <= rx_end; qidx++) {
		rxq = dev->data->rx_queues[qidx];
		/* Count total numbers of rxq descs */
		total_rxq_desc += rxq->qlen_mask + 1;
		exp_buffs = RTE_MEMPOOL_CACHE_MAX_SIZE + rxq->rx_free_thresh;
		exp_buffs *= dev->data->nb_rx_queues;
		if (rte_mempool_avail_count(rxq->pool) < exp_buffs) {
			PMD_INIT_LOG(ERR, "Buff shortage in pool=%s (%d/%d)",
				     rxq->pool->name,
				     rte_mempool_avail_count(rxq->pool),
				     exp_buffs);
			return -ENOENT;
		}
	}

	/* Check RBDR desc overflow */
	ret = nicvf_qsize_rbdr_roundup(total_rxq_desc);
	if (ret == 0) {
		PMD_INIT_LOG(ERR, "Reached RBDR desc limit, reduce nr desc "
			     "VF%d", nic->vf_id);
		return -ENOMEM;
	}

	/* Enable qset */
	ret = nicvf_qset_config(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to enable qset %d VF%d", ret,
			     nic->vf_id);
		return ret;
	}

	/* Allocate RBDR and RBDR ring desc */
	nb_rbdr_desc = nicvf_qsize_rbdr_roundup(total_rxq_desc);
	ret = nicvf_qset_rbdr_alloc(dev, nic, nb_rbdr_desc, rbdrsz);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rbdr alloc "
			     "VF%d", nic->vf_id);
		goto qset_reclaim;
	}

	/* Enable and configure RBDR registers */
	ret = nicvf_qset_rbdr_config(nic, 0);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to configure rbdr %d VF%d", ret,
			     nic->vf_id);
		goto qset_rbdr_free;
	}

	/* Fill rte_mempool buffers in RBDR pool and precharge it */
	ret = nicvf_qset_rbdr_precharge(dev, nic, 0, rbdr_rte_mempool_get,
					total_rxq_desc);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to fill rbdr %d VF%d", ret,
			     nic->vf_id);
		goto qset_rbdr_reclaim;
	}

	PMD_DRV_LOG(INFO, "Filled %d out of %d entries in RBDR VF%d",
		     nic->rbdr->tail, nb_rbdr_desc, nic->vf_id);

	/* Configure VLAN Strip */
	nicvf_vlan_hw_strip(nic, dev->data->dev_conf.rxmode.hw_vlan_strip);

	/* Based on the packet type(IPv4 or IPv6), the nicvf HW aligns L3 data
	 * to the 64bit memory address.
	 * The alignment creates a hole in mbuf(between the end of headroom and
	 * packet data start). The new revision of the HW provides an option to
	 * disable the L3 alignment feature and make mbuf layout looks
	 * more like other NICs. For better application compatibility, disabling
	 * l3 alignment feature on the hardware revisions it supports
	 */
	nicvf_apad_config(nic, false);

	/* Get queue ranges for this VF */
	nicvf_tx_range(dev, nic, &tx_start, &tx_end);

	/* Configure TX queues */
	for (qidx = tx_start; qidx <= tx_end; qidx++) {
		ret = nicvf_vf_start_tx_queue(dev, nic,
			qidx % MAX_SND_QUEUES_PER_QS);
		if (ret)
			goto start_txq_error;
	}

	/* Configure RX queues */
	for (qidx = rx_start; qidx <= rx_end; qidx++) {
		ret = nicvf_vf_start_rx_queue(dev, nic,
			qidx % MAX_RCV_QUEUES_PER_QS);
		if (ret)
			goto start_rxq_error;
	}

	if (!nic->sqs_mode) {
		/* Configure CPI algorithm */
		ret = nicvf_configure_cpi(dev);
		if (ret)
			goto start_txq_error;

		ret = nicvf_mbox_get_rss_size(nic);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to get rss table size");
			goto qset_rss_error;
		}

		/* Configure RSS */
		ret = nicvf_configure_rss(dev);
		if (ret)
			goto qset_rss_error;
	}

	/* Done; Let PF make the BGX's RX and TX switches to ON position */
	nicvf_mbox_cfg_done(nic);
	return 0;

qset_rss_error:
	nicvf_rss_term(nic);
start_rxq_error:
	for (qidx = rx_start; qidx <= rx_end; qidx++)
		nicvf_vf_stop_rx_queue(dev, nic, qidx % MAX_RCV_QUEUES_PER_QS);
start_txq_error:
	for (qidx = tx_start; qidx <= tx_end; qidx++)
		nicvf_vf_stop_tx_queue(dev, nic, qidx % MAX_SND_QUEUES_PER_QS);
qset_rbdr_reclaim:
	nicvf_qset_rbdr_reclaim(nic, 0);
	nicvf_rbdr_release_mbufs(dev, nic);
qset_rbdr_free:
	if (nic->rbdr) {
		rte_free(nic->rbdr);
		nic->rbdr = NULL;
	}
qset_reclaim:
	nicvf_qset_reclaim(nic);
	return ret;
}

static int
nicvf_dev_start(struct rte_eth_dev *dev)
{
	uint16_t qidx;
	int ret;
	size_t i;
	struct nicvf *nic = nicvf_pmd_priv(dev);
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;
	uint16_t mtu;
	uint32_t buffsz = 0, rbdrsz = 0;
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct nicvf_rxq *rxq;

	PMD_INIT_FUNC_TRACE();

	/* This function must be called for a primary device */
	assert_primary(nic);

	/* Validate RBDR buff size */
	for (qidx = 0; qidx < dev->data->nb_rx_queues; qidx++) {
		rxq = dev->data->rx_queues[qidx];
		mbp_priv = rte_mempool_get_priv(rxq->pool);
		buffsz = mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM;
		if (buffsz % 128) {
			PMD_INIT_LOG(ERR, "rxbuf size must be multiply of 128");
			return -EINVAL;
		}
		if (rbdrsz == 0)
			rbdrsz = buffsz;
		if (rbdrsz != buffsz) {
			PMD_INIT_LOG(ERR, "buffsz not same, qidx=%d (%d/%d)",
				     qidx, rbdrsz, buffsz);
			return -EINVAL;
		}
	}

	/* Configure loopback */
	ret = nicvf_loopback_config(nic, dev->data->dev_conf.lpbk_mode);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to configure loopback %d", ret);
		return ret;
	}

	/* Reset all statistics counters attached to this port */
	ret = nicvf_mbox_reset_stat_counters(nic, 0x3FFF, 0x1F, 0xFFFF, 0xFFFF);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to reset stat counters %d", ret);
		return ret;
	}

	/* Setup scatter mode if needed by jumbo */
	if (dev->data->dev_conf.rxmode.max_rx_pkt_len +
					    2 * VLAN_TAG_SIZE > buffsz)
		dev->data->scattered_rx = 1;
	if (rx_conf->enable_scatter)
		dev->data->scattered_rx = 1;

	/* Setup MTU based on max_rx_pkt_len or default */
	mtu = dev->data->dev_conf.rxmode.jumbo_frame ?
		dev->data->dev_conf.rxmode.max_rx_pkt_len
			-  ETHER_HDR_LEN - ETHER_CRC_LEN
		: ETHER_MTU;

	if (nicvf_dev_set_mtu(dev, mtu)) {
		PMD_INIT_LOG(ERR, "Failed to set default mtu size");
		return -EBUSY;
	}

	ret = nicvf_vf_start(dev, nic, rbdrsz);
	if (ret != 0)
		return ret;

	for (i = 0; i < nic->sqs_count; i++) {
		assert(nic->snicvf[i]);

		ret = nicvf_vf_start(dev, nic->snicvf[i], rbdrsz);
		if (ret != 0)
			return ret;
	}

	/* Configure callbacks based on scatter mode */
	nicvf_set_tx_function(dev);
	nicvf_set_rx_function(dev);

	return 0;
}

static void
nicvf_dev_stop_cleanup(struct rte_eth_dev *dev, bool cleanup)
{
	size_t i;
	int ret;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	/* Teardown secondary vf first */
	for (i = 0; i < nic->sqs_count; i++) {
		if (!nic->snicvf[i])
			continue;

		nicvf_vf_stop(dev, nic->snicvf[i], cleanup);
	}

	/* Stop the primary VF now */
	nicvf_vf_stop(dev, nic, cleanup);

	/* Disable loopback */
	ret = nicvf_loopback_config(nic, 0);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to disable loopback %d", ret);

	/* Reclaim CPI configuration */
	ret = nicvf_mbox_config_cpi(nic, 0);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to reclaim CPI config %d", ret);
}

static void
nicvf_dev_stop(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	nicvf_dev_stop_cleanup(dev, false);
}

static void
nicvf_vf_stop(struct rte_eth_dev *dev, struct nicvf *nic, bool cleanup)
{
	int ret;
	uint16_t qidx;
	uint16_t tx_start, tx_end;
	uint16_t rx_start, rx_end;

	PMD_INIT_FUNC_TRACE();

	if (cleanup) {
		/* Let PF make the BGX's RX and TX switches to OFF position */
		nicvf_mbox_shutdown(nic);
	}

	/* Disable VLAN Strip */
	nicvf_vlan_hw_strip(nic, 0);

	/* Get queue ranges for this VF */
	nicvf_tx_range(dev, nic, &tx_start, &tx_end);

	for (qidx = tx_start; qidx <= tx_end; qidx++)
		nicvf_vf_stop_tx_queue(dev, nic, qidx % MAX_SND_QUEUES_PER_QS);

	/* Get queue ranges for this VF */
	nicvf_rx_range(dev, nic, &rx_start, &rx_end);

	/* Reclaim rq */
	for (qidx = rx_start; qidx <= rx_end; qidx++)
		nicvf_vf_stop_rx_queue(dev, nic, qidx % MAX_RCV_QUEUES_PER_QS);

	/* Reclaim RBDR */
	ret = nicvf_qset_rbdr_reclaim(nic, 0);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to reclaim RBDR %d", ret);

	/* Move all charged buffers in RBDR back to pool */
	if (nic->rbdr != NULL)
		nicvf_rbdr_release_mbufs(dev, nic);

	/* Disable qset */
	ret = nicvf_qset_reclaim(nic);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to disable qset %d", ret);

	/* Disable all interrupts */
	nicvf_disable_all_interrupts(nic);

	/* Free RBDR SW structure */
	if (nic->rbdr) {
		rte_free(nic->rbdr);
		nic->rbdr = NULL;
	}
}

static void
nicvf_dev_close(struct rte_eth_dev *dev)
{
	size_t i;
	struct nicvf *nic = nicvf_pmd_priv(dev);

	PMD_INIT_FUNC_TRACE();

	nicvf_dev_stop_cleanup(dev, true);
	nicvf_periodic_alarm_stop(nicvf_interrupt, dev);

	for (i = 0; i < nic->sqs_count; i++) {
		if (!nic->snicvf[i])
			continue;

		nicvf_periodic_alarm_stop(nicvf_vf_interrupt, nic->snicvf[i]);
	}
}

static int
nicvf_request_sqs(struct nicvf *nic)
{
	size_t i;

	assert_primary(nic);
	assert(nic->sqs_count > 0);
	assert(nic->sqs_count <= MAX_SQS_PER_VF);

	/* Set no of Rx/Tx queues in each of the SQsets */
	for (i = 0; i < nic->sqs_count; i++) {
		if (nicvf_svf_empty())
			rte_panic("Cannot assign sufficient number of "
				  "secondary queues to primary VF%" PRIu8 "\n",
				  nic->vf_id);

		nic->snicvf[i] = nicvf_svf_pop();
		nic->snicvf[i]->sqs_id = i;
	}

	return nicvf_mbox_request_sqs(nic);
}

static int
nicvf_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	struct nicvf *nic = nicvf_pmd_priv(dev);
	uint8_t cqcount;

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

	assert_primary(nic);
	NICVF_STATIC_ASSERT(MAX_RCV_QUEUES_PER_QS == MAX_SND_QUEUES_PER_QS);
	cqcount = RTE_MAX(data->nb_tx_queues, data->nb_rx_queues);
	if (cqcount > MAX_RCV_QUEUES_PER_QS) {
		nic->sqs_count = RTE_ALIGN_CEIL(cqcount, MAX_RCV_QUEUES_PER_QS);
		nic->sqs_count = (nic->sqs_count / MAX_RCV_QUEUES_PER_QS) - 1;
	} else {
		nic->sqs_count = 0;
	}

	assert(nic->sqs_count <= MAX_SQS_PER_VF);

	if (nic->sqs_count > 0) {
		if (nicvf_request_sqs(nic)) {
			rte_panic("Cannot assign sufficient number of "
				  "secondary queues to PORT%d VF%" PRIu8 "\n",
				  dev->data->port_id, nic->vf_id);
		}
	}

	PMD_INIT_LOG(DEBUG, "Configured ethdev port%d hwcap=0x%" PRIx64,
		dev->data->port_id, nicvf_hw_cap(nic));

	return 0;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops nicvf_eth_dev_ops = {
	.dev_configure            = nicvf_dev_configure,
	.dev_start                = nicvf_dev_start,
	.dev_stop                 = nicvf_dev_stop,
	.link_update              = nicvf_dev_link_update,
	.dev_close                = nicvf_dev_close,
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
	.rx_queue_start           = nicvf_dev_rx_queue_start,
	.rx_queue_stop            = nicvf_dev_rx_queue_stop,
	.tx_queue_start           = nicvf_dev_tx_queue_start,
	.tx_queue_stop            = nicvf_dev_tx_queue_stop,
	.rx_queue_setup           = nicvf_dev_rx_queue_setup,
	.rx_queue_release         = nicvf_dev_rx_queue_release,
	.rx_queue_count           = nicvf_dev_rx_queue_count,
	.tx_queue_setup           = nicvf_dev_tx_queue_setup,
	.tx_queue_release         = nicvf_dev_tx_queue_release,
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

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		if (nic) {
			/* Setup callbacks for secondary process */
			nicvf_set_tx_function(eth_dev);
			nicvf_set_rx_function(eth_dev);
			return 0;
		} else {
			/* If nic == NULL than it is secondary function
			 * so ethdev need to be released by caller */
			return ENOTSUP;
		}
	}

	pci_dev = eth_dev->pci_dev;
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	nic->device_id = pci_dev->id.device_id;
	nic->vendor_id = pci_dev->id.vendor_id;
	nic->subsystem_device_id = pci_dev->id.subsystem_device_id;
	nic->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

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

	ret = nicvf_periodic_alarm_start(nicvf_interrupt, eth_dev);
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

	ret = nicvf_base_init(nic);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to execute nicvf_base_init");
		goto malloc_fail;
	}

	if (nic->sqs_mode) {
		/* Push nic to stack of secondary vfs */
		nicvf_svf_push(nic);

		/* Steal nic pointer from the device for further reuse */
		eth_dev->data->dev_private = NULL;

		nicvf_periodic_alarm_stop(nicvf_interrupt, eth_dev);
		ret = nicvf_periodic_alarm_start(nicvf_vf_interrupt, nic);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to start period alarm");
			goto fail;
		}

		/* Detach port by returning postive error number */
		return ENOTSUP;
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

	PMD_INIT_LOG(INFO, "Port %d (%x:%x) mac=%02x:%02x:%02x:%02x:%02x:%02x",
		eth_dev->data->port_id, nic->vendor_id, nic->device_id,
		nic->mac_addr[0], nic->mac_addr[1], nic->mac_addr[2],
		nic->mac_addr[3], nic->mac_addr[4], nic->mac_addr[5]);

	return 0;

malloc_fail:
	rte_free(eth_dev->data->mac_addrs);
alarm_fail:
	nicvf_periodic_alarm_stop(nicvf_interrupt, eth_dev);
fail:
	return ret;
}

static const struct rte_pci_id pci_id_nicvf_map[] = {
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_CN88XX_PASS1_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_CN88XX_PASS1_NICVF,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_CN88XX_PASS2_NICVF,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_CN81XX_NICVF,
	},
	{
		.class_id = RTE_CLASS_ANY_ID,
		.vendor_id = PCI_VENDOR_ID_CAVIUM,
		.device_id = PCI_DEVICE_ID_THUNDERX_NICVF,
		.subsystem_vendor_id = PCI_VENDOR_ID_CAVIUM,
		.subsystem_device_id = PCI_SUB_DEVICE_ID_CN83XX_NICVF,
	},
	{
		.vendor_id = 0,
	},
};

static struct eth_driver rte_nicvf_pmd = {
	.pci_drv = {
		.id_table = pci_id_nicvf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
		.probe = rte_eth_dev_pci_probe,
		.remove = rte_eth_dev_pci_remove,
	},
	.eth_dev_init = nicvf_eth_dev_init,
	.dev_private_size = sizeof(struct nicvf),
};

RTE_PMD_REGISTER_PCI(net_thunderx, rte_nicvf_pmd.pci_drv);
RTE_PMD_REGISTER_PCI_TABLE(net_thunderx, pci_id_nicvf_map);
