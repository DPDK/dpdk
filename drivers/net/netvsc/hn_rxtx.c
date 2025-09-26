/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Microsoft Corporation
 * Copyright(c) 2013-2016 Brocade Communications Systems, Inc.
 * All rights reserved.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <malloc.h>

#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_bitmap.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <dev_driver.h>
#include <rte_net.h>
#include <bus_vmbus_driver.h>
#include <rte_spinlock.h>

#include "hn_logs.h"
#include "hn_var.h"
#include "hn_rndis.h"
#include "hn_nvs.h"
#include "ndis.h"

#define HN_NVS_SEND_MSG_SIZE \
	(sizeof(struct vmbus_chanpkt_hdr) + sizeof(struct hn_nvs_rndis))

#define HN_TXD_CACHE_SIZE	32 /* per cpu tx_descriptor pool cache */
#define HN_RXQ_EVENT_DEFAULT	2048

struct hn_rxinfo {
	uint32_t	vlan_info;
	uint32_t	csum_info;
	uint32_t	hash_info;
	uint32_t	hash_value;
};

#define HN_RXINFO_VLAN			0x0001
#define HN_RXINFO_CSUM			0x0002
#define HN_RXINFO_HASHINF		0x0004
#define HN_RXINFO_HASHVAL		0x0008
#define HN_RXINFO_ALL			\
	(HN_RXINFO_VLAN |		\
	 HN_RXINFO_CSUM |		\
	 HN_RXINFO_HASHINF |		\
	 HN_RXINFO_HASHVAL)

#define HN_NDIS_VLAN_INFO_INVALID	0xffffffff
#define HN_NDIS_RXCSUM_INFO_INVALID	0
#define HN_NDIS_HASH_INFO_INVALID	0

/*
 * Per-transmit book keeping.
 * A slot in transmit ring (chim_index) is reserved for each transmit.
 *
 * There are two types of transmit:
 *   - buffered transmit where chimney buffer is used and RNDIS header
 *     is in the buffer. mbuf == NULL for this case.
 *
 *   - direct transmit where RNDIS header is in the in  rndis_pkt
 *     mbuf is freed after transmit.
 *
 * Descriptors come from per-port pool which is used
 * to limit number of outstanding requests per device.
 */
struct hn_txdesc {
	struct rte_mbuf *m;

	uint16_t	queue_id;
	uint32_t	chim_index;
	uint32_t	chim_size;
	uint32_t	data_size;
	uint32_t	packets;

	struct rndis_packet_msg *rndis_pkt;
};

#define HN_RNDIS_PKT_LEN				\
	(sizeof(struct rndis_packet_msg) +		\
	 RNDIS_PKTINFO_SIZE(NDIS_HASH_VALUE_SIZE) +	\
	 RNDIS_PKTINFO_SIZE(NDIS_VLAN_INFO_SIZE) +	\
	 RNDIS_PKTINFO_SIZE(NDIS_LSO2_INFO_SIZE) +	\
	 RNDIS_PKTINFO_SIZE(NDIS_TXCSUM_INFO_SIZE))

#define HN_RNDIS_PKT_ALIGNED	RTE_ALIGN(HN_RNDIS_PKT_LEN, RTE_CACHE_LINE_SIZE)

/* Minimum space required for a packet */
#define HN_PKTSIZE_MIN(align) \
	RTE_ALIGN(RTE_ETHER_MIN_LEN + HN_RNDIS_PKT_LEN, align)

#define DEFAULT_TX_FREE_THRESH 32

static void
hn_update_packet_stats(struct hn_stats *stats, const struct rte_mbuf *m)
{
	uint32_t s = m->pkt_len;
	const struct rte_ether_addr *ea;

	if (s >= 1024)
		stats->size_bins[6 + (s > 1518)]++;
	else if (s <= 64)
		stats->size_bins[s >> 6]++;
	else
		stats->size_bins[32UL - rte_clz32(s) - 5]++;

	ea = rte_pktmbuf_mtod(m, const struct rte_ether_addr *);
	RTE_BUILD_BUG_ON(offsetof(struct hn_stats, broadcast) !=
			offsetof(struct hn_stats, multicast) + sizeof(uint64_t));
	if (unlikely(rte_is_multicast_ether_addr(ea)))
		(&stats->multicast)[rte_is_broadcast_ether_addr(ea)]++;
}

static inline unsigned int hn_rndis_pktlen(const struct rndis_packet_msg *pkt)
{
	return pkt->pktinfooffset + pkt->pktinfolen;
}

static inline uint32_t
hn_rndis_pktmsg_offset(uint32_t ofs)
{
	return ofs - offsetof(struct rndis_packet_msg, dataoffset);
}

static void hn_txd_init(struct rte_mempool *mp __rte_unused,
			void *opaque, void *obj, unsigned int idx)
{
	struct hn_tx_queue *txq = opaque;
	struct hn_txdesc *txd = obj;

	memset(txd, 0, sizeof(*txd));

	txd->queue_id = txq->queue_id;
	txd->chim_index = NVS_CHIM_IDX_INVALID;
	txd->rndis_pkt = (struct rndis_packet_msg *)((char *)txq->tx_rndis
		+ idx * HN_RNDIS_PKT_ALIGNED);
}

int
hn_chim_init(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;
	uint32_t i, chim_bmp_size;

	rte_spinlock_init(&hv->chim_lock);
	chim_bmp_size = rte_bitmap_get_memory_footprint(hv->chim_cnt);
	hv->chim_bmem = rte_zmalloc("hn_chim_bitmap", chim_bmp_size,
				    RTE_CACHE_LINE_SIZE);
	if (hv->chim_bmem == NULL) {
		PMD_INIT_LOG(ERR, "failed to allocate bitmap size %u",
			     chim_bmp_size);
		return -1;
	}

	hv->chim_bmap = rte_bitmap_init(hv->chim_cnt,
					hv->chim_bmem, chim_bmp_size);
	if (hv->chim_bmap == NULL) {
		PMD_INIT_LOG(ERR, "failed to init chim bitmap");
		return -1;
	}

	for (i = 0; i < hv->chim_cnt; i++)
		rte_bitmap_set(hv->chim_bmap, i);

	return 0;
}

void
hn_chim_uninit(struct rte_eth_dev *dev)
{
	struct hn_data *hv = dev->data->dev_private;

	rte_bitmap_free(hv->chim_bmap);
	rte_free(hv->chim_bmem);
	hv->chim_bmem = NULL;
}

static uint32_t hn_chim_alloc(struct hn_data *hv)
{
	uint32_t index = NVS_CHIM_IDX_INVALID;
	uint64_t slab = 0;

	rte_spinlock_lock(&hv->chim_lock);
	if (rte_bitmap_scan(hv->chim_bmap, &index, &slab)) {
		index += rte_bsf64(slab);
		rte_bitmap_clear(hv->chim_bmap, index);
	}
	rte_spinlock_unlock(&hv->chim_lock);

	return index;
}

static void hn_chim_free(struct hn_data *hv, uint32_t chim_idx)
{
	if (chim_idx >= hv->chim_cnt) {
		PMD_DRV_LOG(ERR, "Invalid chimney index %u", chim_idx);
	} else {
		rte_spinlock_lock(&hv->chim_lock);
		rte_bitmap_set(hv->chim_bmap, chim_idx);
		rte_spinlock_unlock(&hv->chim_lock);
	}
}

static void hn_reset_txagg(struct hn_tx_queue *txq)
{
	txq->agg_szleft = txq->agg_szmax;
	txq->agg_pktleft = txq->agg_pktmax;
	txq->agg_txd = NULL;
	txq->agg_prevpkt = NULL;
}

static void
hn_rx_queue_free_common(struct hn_rx_queue *rxq)
{
	if (!rxq)
		return;

	rte_free(rxq->rxbuf_info);
	rte_free(rxq->event_buf);
	rte_free(rxq);
}

int
hn_dev_tx_queue_setup(struct rte_eth_dev *dev,
		      uint16_t queue_idx, uint16_t nb_desc,
		      unsigned int socket_id,
		      const struct rte_eth_txconf *tx_conf)

{
	struct hn_data *hv = dev->data->dev_private;
	struct hn_tx_queue *txq;
	struct hn_rx_queue *rxq = NULL;
	char name[RTE_MEMPOOL_NAMESIZE];
	uint32_t tx_free_thresh;
	int err = -ENOMEM;

	PMD_INIT_FUNC_TRACE();

	tx_free_thresh = tx_conf->tx_free_thresh;
	if (tx_free_thresh == 0)
		tx_free_thresh = RTE_MIN(nb_desc / 4,
					 DEFAULT_TX_FREE_THRESH);

	if (tx_free_thresh + 3 >= nb_desc) {
		PMD_INIT_LOG(ERR,
			     "tx_free_thresh must be less than the number of TX entries minus 3(%u)."
			     " (tx_free_thresh=%u port=%u queue=%u)",
			     nb_desc - 3,
			     tx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	txq = rte_zmalloc_socket("HN_TXQ", sizeof(*txq), RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!txq)
		return -ENOMEM;

	txq->hv = hv;
	txq->chan = hv->channels[queue_idx];
	txq->port_id = dev->data->port_id;
	txq->queue_id = queue_idx;
	txq->free_thresh = tx_free_thresh;

	snprintf(name, sizeof(name),
		 "hn_txd_%u_%u", dev->data->port_id, queue_idx);

	PMD_INIT_LOG(DEBUG, "TX descriptor pool %s n=%u size=%zu",
		     name, nb_desc, sizeof(struct hn_txdesc));

	txq->tx_rndis_mz = rte_memzone_reserve_aligned(name,
			nb_desc * HN_RNDIS_PKT_ALIGNED, rte_socket_id(),
			RTE_MEMZONE_IOVA_CONTIG, HN_RNDIS_PKT_ALIGNED);
	if (!txq->tx_rndis_mz) {
		err = -rte_errno;
		goto error;
	}
	txq->tx_rndis = txq->tx_rndis_mz->addr;
	txq->tx_rndis_iova = txq->tx_rndis_mz->iova;

	txq->txdesc_pool = rte_mempool_create(name, nb_desc,
					      sizeof(struct hn_txdesc),
					      0, 0, NULL, NULL,
					      hn_txd_init, txq,
					      dev->device->numa_node, 0);
	if (txq->txdesc_pool == NULL) {
		PMD_DRV_LOG(ERR,
			    "mempool %s create failed: %d", name, rte_errno);
		goto error;
	}

	/*
	 * If there are more Tx queues than Rx queues, allocate rx_queues
	 * with event buffer so that Tx completion messages can still be
	 * received
	 */
	if (queue_idx >= dev->data->nb_rx_queues) {
		rxq = hn_rx_queue_alloc(hv, queue_idx, socket_id);

		if (!rxq) {
			err = -ENOMEM;
			goto error;
		}

		/*
		 * Don't allocate mbuf pool or rx ring.  RSS is always configured
		 * to ensure packets aren't received by this Rx queue.
		 */
		rxq->mb_pool = NULL;
		rxq->rx_ring = NULL;
	}

	txq->agg_szmax  = RTE_MIN(hv->chim_szmax, hv->rndis_agg_size);
	txq->agg_pktmax = hv->rndis_agg_pkts;
	txq->agg_align  = hv->rndis_agg_align;

	hn_reset_txagg(txq);

	err = hn_vf_tx_queue_setup(dev, queue_idx, nb_desc,
				     socket_id, tx_conf);
	if (err == 0) {
		dev->data->tx_queues[queue_idx] = txq;
		if (rxq != NULL)
			dev->data->rx_queues[queue_idx] = rxq;
		return 0;
	}

error:
	rte_mempool_free(txq->txdesc_pool);
	rte_memzone_free(txq->tx_rndis_mz);
	hn_rx_queue_free_common(rxq);
	rte_free(txq);
	return err;
}

void
hn_dev_tx_queue_info(struct rte_eth_dev *dev, uint16_t queue_id,
		     struct rte_eth_txq_info *qinfo)
{
	struct hn_tx_queue *txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->txdesc_pool->size;
	qinfo->conf.offloads = dev->data->dev_conf.txmode.offloads;
}

static struct hn_txdesc *hn_txd_get(struct hn_tx_queue *txq)
{
	struct hn_txdesc *txd;

	if (rte_mempool_get(txq->txdesc_pool, (void **)&txd)) {
		++txq->stats.ring_full;
		PMD_TX_LOG(DEBUG, "tx pool exhausted!");
		return NULL;
	}

	txd->m = NULL;
	txd->packets = 0;
	txd->data_size = 0;
	txd->chim_size = 0;

	return txd;
}

static void hn_txd_put(struct hn_tx_queue *txq, struct hn_txdesc *txd)
{
	rte_mempool_put(txq->txdesc_pool, txd);
}

void
hn_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct hn_tx_queue *txq = dev->data->tx_queues[qid];

	PMD_INIT_FUNC_TRACE();

	if (!txq)
		return;
	/*
	 * Free any Rx queues allocated for a Tx queue without a corresponding
	 * Rx queue
	 */
	if (qid >= dev->data->nb_rx_queues)
		hn_rx_queue_free_common(dev->data->rx_queues[qid]);

	rte_mempool_free(txq->txdesc_pool);

	rte_memzone_free(txq->tx_rndis_mz);
	rte_free(txq);
}

/*
 * Check the status of a Tx descriptor in the queue.
 *
 * returns:
 *  - -EINVAL              - offset outside of tx_descriptor pool.
 *  - RTE_ETH_TX_DESC_FULL - descriptor is not acknowledged by host.
 *  - RTE_ETH_TX_DESC_DONE - descriptor is available.
 */
int hn_dev_tx_descriptor_status(void *arg, uint16_t offset)
{
	const struct hn_tx_queue *txq = arg;

	hn_process_events(txq->hv, txq->queue_id, 0);

	if (offset >= rte_mempool_avail_count(txq->txdesc_pool))
		return -EINVAL;

	if (offset < rte_mempool_in_use_count(txq->txdesc_pool))
		return RTE_ETH_TX_DESC_FULL;
	else
		return RTE_ETH_TX_DESC_DONE;
}

static void
hn_nvs_send_completed(struct rte_eth_dev *dev, uint16_t queue_id,
		      unsigned long xactid, const struct hn_nvs_rndis_ack *ack)
{
	struct hn_data *hv = dev->data->dev_private;
	struct hn_txdesc *txd = (struct hn_txdesc *)xactid;
	struct hn_tx_queue *txq;

	/* Control packets are sent with xacid == 0 */
	if (!txd)
		return;

	txq = dev->data->tx_queues[queue_id];
	if (likely(ack->status == NVS_STATUS_OK)) {
		PMD_TX_LOG(DEBUG, "port %u:%u complete tx %u packets %u bytes %u",
			   txq->port_id, txq->queue_id, txd->chim_index,
			   txd->packets, txd->data_size);
		txq->stats.bytes += txd->data_size;
		txq->stats.packets += txd->packets;
	} else {
		PMD_DRV_LOG(NOTICE, "port %u:%u complete tx %u failed status %u",
			    txq->port_id, txq->queue_id, txd->chim_index, ack->status);
		++txq->stats.errors;
	}

	if (txd->chim_index != NVS_CHIM_IDX_INVALID) {
		hn_chim_free(hv, txd->chim_index);
		txd->chim_index = NVS_CHIM_IDX_INVALID;
	}

	rte_pktmbuf_free(txd->m);
	hn_txd_put(txq, txd);
}

/* Handle transmit completion events */
static void
hn_nvs_handle_comp(struct rte_eth_dev *dev, uint16_t queue_id,
		   const struct vmbus_chanpkt_hdr *pkt,
		   const void *data)
{
	const struct hn_nvs_hdr *hdr = data;

	switch (hdr->type) {
	case NVS_TYPE_RNDIS_ACK:
		hn_nvs_send_completed(dev, queue_id, pkt->xactid, data);
		break;

	default:
		PMD_DRV_LOG(NOTICE, "unexpected send completion type %u",
			   hdr->type);
	}
}

/* Parse per-packet info (meta data) */
static int
hn_rndis_rxinfo(const void *info_data, unsigned int info_dlen,
		struct hn_rxinfo *info)
{
	const struct rndis_pktinfo *pi = info_data;
	uint32_t mask = 0;

	while (info_dlen != 0) {
		const void *data;
		uint32_t dlen;

		if (unlikely(info_dlen < sizeof(*pi)))
			return -EINVAL;

		if (unlikely(info_dlen < pi->size))
			return -EINVAL;
		info_dlen -= pi->size;

		if (unlikely(pi->size & RNDIS_PKTINFO_SIZE_ALIGNMASK))
			return -EINVAL;
		if (unlikely(pi->size < pi->offset))
			return -EINVAL;

		dlen = pi->size - pi->offset;
		data = pi->data;

		switch (pi->type) {
		case NDIS_PKTINFO_TYPE_VLAN:
			if (unlikely(dlen < NDIS_VLAN_INFO_SIZE))
				return -EINVAL;
			info->vlan_info = *((const uint32_t *)data);
			mask |= HN_RXINFO_VLAN;
			break;

		case NDIS_PKTINFO_TYPE_CSUM:
			if (unlikely(dlen < NDIS_RXCSUM_INFO_SIZE))
				return -EINVAL;
			info->csum_info = *((const uint32_t *)data);
			mask |= HN_RXINFO_CSUM;
			break;

		case NDIS_PKTINFO_TYPE_HASHVAL:
			if (unlikely(dlen < NDIS_HASH_VALUE_SIZE))
				return -EINVAL;
			info->hash_value = *((const uint32_t *)data);
			mask |= HN_RXINFO_HASHVAL;
			break;

		case NDIS_PKTINFO_TYPE_HASHINF:
			if (unlikely(dlen < NDIS_HASH_INFO_SIZE))
				return -EINVAL;
			info->hash_info = *((const uint32_t *)data);
			mask |= HN_RXINFO_HASHINF;
			break;

		default:
			goto next;
		}

		if (mask == HN_RXINFO_ALL)
			break; /* All found; done */
next:
		pi = (const struct rndis_pktinfo *)
		    ((const uint8_t *)pi + pi->size);
	}

	/*
	 * Final fixup.
	 * - If there is no hash value, invalidate the hash info.
	 */
	if (!(mask & HN_RXINFO_HASHVAL))
		info->hash_info = HN_NDIS_HASH_INFO_INVALID;
	return 0;
}

static void hn_rx_buf_free_cb(void *buf __rte_unused, void *opaque)
{
	struct hn_rx_bufinfo *rxb = opaque;
	struct hn_rx_queue *rxq = rxb->rxq;
	struct hn_data *hv = rxq->hv;

	rte_atomic32_dec(&rxq->rxbuf_outstanding);
	hn_nvs_ack_rxbuf(hv, rxb->chan, rxb->xactid);
}

static struct hn_rx_bufinfo *hn_rx_buf_init(struct hn_rx_queue *rxq,
					    const struct vmbus_chanpkt_rxbuf *pkt)
{
	struct hn_rx_bufinfo *rxb;

	rxb = rxq->rxbuf_info + pkt->hdr.xactid;
	rxb->chan = rxq->chan;
	rxb->xactid = pkt->hdr.xactid;
	rxb->rxq = rxq;

	rxb->shinfo.free_cb = hn_rx_buf_free_cb;
	rxb->shinfo.fcb_opaque = rxb;
	rte_mbuf_ext_refcnt_set(&rxb->shinfo, 1);
	return rxb;
}

static void hn_rxpkt(struct hn_rx_queue *rxq, struct hn_rx_bufinfo *rxb,
		     uint8_t *data, unsigned int headroom, unsigned int dlen,
		     const struct hn_rxinfo *info)
{
	struct hn_data *hv = rxq->hv;
	struct rte_mbuf *m = NULL;
	bool use_extbuf = false;

	if (likely(rxq->mb_pool != NULL))
		m = rte_pktmbuf_alloc(rxq->mb_pool);

	if (unlikely(!m)) {
		struct rte_eth_dev *dev =
			&rte_eth_devices[rxq->port_id];

		dev->data->rx_mbuf_alloc_failed++;
		return;
	}

	/*
	 * For large packets, avoid copy if possible but need to keep
	 * some space available in receive area for later packets.
	 */
	if (hv->rx_extmbuf_enable && dlen > hv->rx_copybreak &&
	    (uint32_t)rte_atomic32_read(&rxq->rxbuf_outstanding) <
			hv->rxbuf_section_cnt / 2) {
		struct rte_mbuf_ext_shared_info *shinfo;
		const void *rxbuf;
		rte_iova_t iova;

		/*
		 * Build an external mbuf that points to receive area.
		 * Use refcount to handle multiple packets in same
		 * receive buffer section.
		 */
		rxbuf = hv->rxbuf_res.addr;
		iova = rte_mem_virt2iova(rxbuf) + RTE_PTR_DIFF(data, rxbuf);
		shinfo = &rxb->shinfo;

		/* shinfo is already set to 1 by the caller */
		if (rte_mbuf_ext_refcnt_update(shinfo, 1) == 2)
			rte_atomic32_inc(&rxq->rxbuf_outstanding);

		rte_pktmbuf_attach_extbuf(m, data, iova,
					  dlen + headroom, shinfo);
		m->data_off = headroom;
		use_extbuf = true;
	} else {
		/* Mbuf's in pool must be large enough to hold small packets */
		if (unlikely(rte_pktmbuf_tailroom(m) < dlen)) {
			rte_pktmbuf_free_seg(m);
			++rxq->stats.errors;
			return;
		}
		rte_memcpy(rte_pktmbuf_mtod(m, void *),
			   data + headroom, dlen);
	}

	m->port = rxq->port_id;
	m->pkt_len = dlen;
	m->data_len = dlen;
	m->packet_type = rte_net_get_ptype(m, NULL,
					   RTE_PTYPE_L2_MASK |
					   RTE_PTYPE_L3_MASK |
					   RTE_PTYPE_L4_MASK);

	if (info->vlan_info != HN_NDIS_VLAN_INFO_INVALID) {
		m->vlan_tci = RTE_VLAN_TCI_MAKE(NDIS_VLAN_INFO_ID(info->vlan_info),
						NDIS_VLAN_INFO_PRI(info->vlan_info),
						NDIS_VLAN_INFO_CFI(info->vlan_info));
		m->ol_flags |= RTE_MBUF_F_RX_VLAN_STRIPPED | RTE_MBUF_F_RX_VLAN;

		/* NDIS always strips tag, put it back if necessary */
		if (!hv->vlan_strip && rte_vlan_insert(&m)) {
			PMD_DRV_LOG(DEBUG, "vlan insert failed");
			++rxq->stats.errors;
			if (use_extbuf)
				rte_pktmbuf_detach_extbuf(m);
			rte_pktmbuf_free(m);
			return;
		}
	}

	if (info->csum_info != HN_NDIS_RXCSUM_INFO_INVALID) {
		if (info->csum_info & NDIS_RXCSUM_INFO_IPCS_OK)
			m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

		if (info->csum_info & (NDIS_RXCSUM_INFO_UDPCS_OK
				       | NDIS_RXCSUM_INFO_TCPCS_OK))
			m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		else if (info->csum_info & (NDIS_RXCSUM_INFO_TCPCS_FAILED
					    | NDIS_RXCSUM_INFO_UDPCS_FAILED))
			m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	}

	if (info->hash_info != HN_NDIS_HASH_INFO_INVALID) {
		m->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		m->hash.rss = info->hash_value;
	}

	PMD_RX_LOG(DEBUG,
		   "port %u:%u RX id %"PRIu64" size %u type %#x ol_flags %#"PRIx64,
		   rxq->port_id, rxq->queue_id, rxb->xactid,
		   m->pkt_len, m->packet_type, m->ol_flags);

	++rxq->stats.packets;
	rxq->stats.bytes += m->pkt_len;
	hn_update_packet_stats(&rxq->stats, m);

	if (unlikely(rte_ring_sp_enqueue(rxq->rx_ring, m) != 0)) {
		++rxq->stats.ring_full;
		PMD_RX_LOG(DEBUG, "rx ring full");
		if (use_extbuf)
			rte_pktmbuf_detach_extbuf(m);
		rte_pktmbuf_free(m);
	}
}

static void hn_rndis_rx_data(struct hn_rx_queue *rxq,
			     struct hn_rx_bufinfo *rxb,
			     void *data, uint32_t dlen)
{
	unsigned int data_off, data_len;
	unsigned int pktinfo_off, pktinfo_len;
	const struct rndis_packet_msg *pkt = data;
	struct hn_rxinfo info = {
		.vlan_info = HN_NDIS_VLAN_INFO_INVALID,
		.csum_info = HN_NDIS_RXCSUM_INFO_INVALID,
		.hash_info = HN_NDIS_HASH_INFO_INVALID,
	};
	int err;

	hn_rndis_dump(pkt);

	if (unlikely(dlen < sizeof(*pkt)))
		goto error;

	if (unlikely(dlen < pkt->len))
		goto error; /* truncated RNDIS from host */

	if (unlikely(pkt->len < pkt->datalen
		     + pkt->oobdatalen + pkt->pktinfolen))
		goto error;

	if (unlikely(pkt->datalen == 0))
		goto error;

	/* Check offsets. */
	if (unlikely(pkt->dataoffset < RNDIS_PACKET_MSG_OFFSET_MIN))
		goto error;

	if (likely(pkt->pktinfooffset > 0) &&
	    unlikely(pkt->pktinfooffset < RNDIS_PACKET_MSG_OFFSET_MIN ||
		     (pkt->pktinfooffset & RNDIS_PACKET_MSG_OFFSET_ALIGNMASK)))
		goto error;

	data_off = RNDIS_PACKET_MSG_OFFSET_ABS(pkt->dataoffset);
	data_len = pkt->datalen;
	pktinfo_off = RNDIS_PACKET_MSG_OFFSET_ABS(pkt->pktinfooffset);
	pktinfo_len = pkt->pktinfolen;

	if (likely(pktinfo_len > 0)) {
		err = hn_rndis_rxinfo((const uint8_t *)pkt + pktinfo_off,
				      pktinfo_len, &info);
		if (err)
			goto error;
	}

	/* overflow check */
	if (data_len > data_len + data_off || data_len + data_off > pkt->len)
		goto error;

	if (unlikely(data_len < RTE_ETHER_HDR_LEN))
		goto error;

	hn_rxpkt(rxq, rxb, data, data_off, data_len, &info);
	return;
error:
	++rxq->stats.errors;
}

static void
hn_rndis_receive(struct rte_eth_dev *dev, struct hn_rx_queue *rxq,
		 struct hn_rx_bufinfo *rxb, void *buf, uint32_t len)
{
	const struct rndis_msghdr *hdr = buf;

	switch (hdr->type) {
	case RNDIS_PACKET_MSG:
		if (dev->data->dev_started)
			hn_rndis_rx_data(rxq, rxb, buf, len);
		break;

	case RNDIS_INDICATE_STATUS_MSG:
		hn_rndis_link_status(dev, buf);
		break;

	case RNDIS_INITIALIZE_CMPLT:
	case RNDIS_QUERY_CMPLT:
	case RNDIS_SET_CMPLT:
		hn_rndis_receive_response(rxq->hv, buf, len);
		break;

	default:
		PMD_DRV_LOG(NOTICE,
			    "unexpected RNDIS message (type %#x len %u)",
			    hdr->type, len);
		break;
	}
}

static void
hn_nvs_handle_rxbuf(struct rte_eth_dev *dev,
		    struct hn_data *hv,
		    struct hn_rx_queue *rxq,
		    const struct vmbus_chanpkt_hdr *hdr,
		    const void *buf)
{
	const struct vmbus_chanpkt_rxbuf *pkt;
	const struct hn_nvs_hdr *nvs_hdr = buf;
	uint32_t rxbuf_sz = hv->rxbuf_res.len;
	char *rxbuf = hv->rxbuf_res.addr;
	unsigned int i, hlen, count;
	struct hn_rx_bufinfo *rxb;

	/* At minimum we need type header */
	if (unlikely(vmbus_chanpkt_datalen(hdr) < sizeof(*nvs_hdr))) {
		PMD_RX_LOG(ERR, "invalid receive nvs RNDIS");
		return;
	}

	/* Make sure that this is a RNDIS message. */
	if (unlikely(nvs_hdr->type != NVS_TYPE_RNDIS)) {
		PMD_RX_LOG(ERR, "nvs type %u, not RNDIS",
			   nvs_hdr->type);
		return;
	}

	hlen = vmbus_chanpkt_getlen(hdr->hlen);
	if (unlikely(hlen < sizeof(*pkt))) {
		PMD_RX_LOG(ERR, "invalid rxbuf chanpkt");
		return;
	}

	pkt = container_of(hdr, const struct vmbus_chanpkt_rxbuf, hdr);
	if (unlikely(pkt->rxbuf_id != NVS_RXBUF_SIG)) {
		PMD_RX_LOG(ERR, "invalid rxbuf_id 0x%08x",
			   pkt->rxbuf_id);
		return;
	}

	count = pkt->rxbuf_cnt;
	if (unlikely(hlen < offsetof(struct vmbus_chanpkt_rxbuf,
				     rxbuf[count]))) {
		PMD_RX_LOG(ERR, "invalid rxbuf_cnt %u", count);
		return;
	}

	if (pkt->hdr.xactid > hv->rxbuf_section_cnt) {
		PMD_RX_LOG(ERR, "invalid rxbuf section id %" PRIx64,
			   pkt->hdr.xactid);
		return;
	}

	/* Setup receive buffer info to allow for callback */
	rxb = hn_rx_buf_init(rxq, pkt);

	/* Each range represents 1 RNDIS pkt that contains 1 Ethernet frame */
	for (i = 0; i < count; ++i) {
		unsigned int ofs, len;

		ofs = pkt->rxbuf[i].ofs;
		len = pkt->rxbuf[i].len;

		if (unlikely(ofs + len > rxbuf_sz)) {
			PMD_RX_LOG(ERR,
				   "%uth RNDIS msg overflow ofs %u, len %u",
				   i, ofs, len);
			continue;
		}

		if (unlikely(len == 0)) {
			PMD_RX_LOG(ERR, "%uth RNDIS msg len %u", i, len);
			continue;
		}

		hn_rndis_receive(dev, rxq, rxb,
				 rxbuf + ofs, len);
	}

	/* Send ACK now if external mbuf not used */
	if (rte_mbuf_ext_refcnt_update(&rxb->shinfo, -1) == 0)
		hn_nvs_ack_rxbuf(hv, rxb->chan, rxb->xactid);
}

/*
 * Called when NVS inband events are received.
 * Send up a two part message with port_id and the NVS message
 * to the pipe to the netvsc-vf-event control thread.
 */
static void hn_nvs_handle_notify(struct rte_eth_dev *dev,
				 const struct vmbus_chanpkt_hdr *pkt,
				 const void *data)
{
	const struct hn_nvs_hdr *hdr = data;

	switch (hdr->type) {
	case NVS_TYPE_TXTBL_NOTE:
		/* Transmit indirection table has locking problems
		 * in DPDK and therefore not implemented
		 */
		PMD_DRV_LOG(DEBUG, "host notify of transmit indirection table");
		break;

	case NVS_TYPE_VFASSOC_NOTE:
		hn_nvs_handle_vfassoc(dev, pkt, data);
		break;

	default:
		PMD_DRV_LOG(INFO,
			    "got notify, nvs type %u", hdr->type);
	}
}

struct hn_rx_queue *hn_rx_queue_alloc(struct hn_data *hv,
				      uint16_t queue_id,
				      unsigned int socket_id)
{
	struct hn_rx_queue *rxq;

	rxq = rte_zmalloc_socket("HN_RXQ", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq)
		return NULL;

	rxq->hv = hv;
	rxq->chan = hv->channels[queue_id];
	rte_spinlock_init(&rxq->ring_lock);
	rxq->port_id = hv->port_id;
	rxq->queue_id = queue_id;
	rxq->event_sz = HN_RXQ_EVENT_DEFAULT;
	rxq->event_buf = rte_malloc_socket("HN_EVENTS", HN_RXQ_EVENT_DEFAULT,
					   RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq->event_buf) {
		rte_free(rxq);
		return NULL;
	}

	/* setup rxbuf_info for non-primary queue */
	if (queue_id) {
		rxq->rxbuf_info = rte_calloc("HN_RXBUF_INFO",
					hv->rxbuf_section_cnt,
					sizeof(*rxq->rxbuf_info),
					RTE_CACHE_LINE_SIZE);

		if (!rxq->rxbuf_info) {
			PMD_DRV_LOG(ERR,
				"Could not allocate rxbuf info for queue %d",
				queue_id);
			rte_free(rxq->event_buf);
			rte_free(rxq);
			return NULL;
		}
	}

	return rxq;
}

void
hn_dev_rx_queue_info(struct rte_eth_dev *dev, uint16_t queue_id,
		     struct rte_eth_rxq_info *qinfo)
{
	struct hn_rx_queue *rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->nb_desc = rxq->rx_ring->size;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
}

int
hn_dev_rx_queue_setup(struct rte_eth_dev *dev,
		      uint16_t queue_idx, uint16_t nb_desc,
		      unsigned int socket_id,
		      const struct rte_eth_rxconf *rx_conf,
		      struct rte_mempool *mp)
{
	struct hn_data *hv = dev->data->dev_private;
	char ring_name[RTE_RING_NAMESIZE];
	struct hn_rx_queue *rxq;
	unsigned int count;
	int error = -ENOMEM;

	PMD_INIT_FUNC_TRACE();

	if (queue_idx == 0) {
		rxq = hv->primary;
	} else {
		/*
		 * If the number of Tx queues was previously greater than the
		 * number of Rx queues, we may already have allocated an rxq.
		 */
		if (!dev->data->rx_queues[queue_idx])
			rxq = hn_rx_queue_alloc(hv, queue_idx, socket_id);
		else
			rxq = dev->data->rx_queues[queue_idx];

		if (!rxq)
			return -ENOMEM;
	}

	rxq->mb_pool = mp;
	count = rte_mempool_avail_count(mp) / dev->data->nb_rx_queues;
	if (nb_desc == 0 || nb_desc > count)
		nb_desc = count;

	/*
	 * Staging ring from receive event logic to rx_pkts.
	 * rx_pkts assumes caller is handling multi-thread issue.
	 * event logic has locking.
	 */
	snprintf(ring_name, sizeof(ring_name),
		 "hn_rx_%u_%u", dev->data->port_id, queue_idx);
	rxq->rx_ring = rte_ring_create(ring_name,
				       rte_align32pow2(nb_desc),
				       socket_id, 0);
	if (!rxq->rx_ring)
		goto fail;

	error = hn_vf_rx_queue_setup(dev, queue_idx, nb_desc,
				     socket_id, rx_conf, mp);
	if (error)
		goto fail;

	dev->data->rx_queues[queue_idx] = rxq;
	return 0;

fail:
	rte_ring_free(rxq->rx_ring);
	/* Only free rxq if it was created in this function. */
	if (!dev->data->rx_queues[queue_idx])
		hn_rx_queue_free_common(rxq);

	return error;
}

static void
hn_rx_queue_free(struct hn_rx_queue *rxq, bool keep_primary)
{

	if (!rxq)
		return;

	rte_ring_free(rxq->rx_ring);
	rxq->rx_ring = NULL;
	rxq->mb_pool = NULL;

	hn_vf_rx_queue_release(rxq->hv, rxq->queue_id);

	/* Keep primary queue to allow for control operations */
	if (keep_primary && rxq == rxq->hv->primary)
		return;

	hn_rx_queue_free_common(rxq);
}

void
hn_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct hn_rx_queue *rxq = dev->data->rx_queues[qid];

	PMD_INIT_FUNC_TRACE();

	hn_rx_queue_free(rxq, true);
}

/*
 * Get the number of used descriptor in a rx queue
 * For this device that means how many packets are pending in the ring.
 */
int
hn_dev_rx_queue_count(void *rx_queue)
{
	struct hn_rx_queue *rxq = rx_queue;

	return rte_ring_count(rxq->rx_ring);
}

/*
 * Check the status of a Rx descriptor in the queue
 *
 * returns:
 *  - -EINVAL               - offset outside of ring
 *  - RTE_ETH_RX_DESC_AVAIL - no data available yet
 *  - RTE_ETH_RX_DESC_DONE  - data is waiting in staging ring
 */
int hn_dev_rx_queue_status(void *arg, uint16_t offset)
{
	const struct hn_rx_queue *rxq = arg;

	hn_process_events(rxq->hv, rxq->queue_id, 0);
	if (offset >= rxq->rx_ring->capacity)
		return -EINVAL;

	if (offset < rte_ring_count(rxq->rx_ring))
		return RTE_ETH_RX_DESC_DONE;
	else
		return RTE_ETH_RX_DESC_AVAIL;
}

int
hn_dev_tx_done_cleanup(void *arg, uint32_t free_cnt)
{
	struct hn_tx_queue *txq = arg;

	return hn_process_events(txq->hv, txq->queue_id, free_cnt);
}

/*
 * Process pending events on the channel.
 * Called from both Rx queue poll and Tx cleanup
 */
uint32_t hn_process_events(struct hn_data *hv, uint16_t queue_id,
			   uint32_t tx_limit)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hv->port_id];
	struct hn_rx_queue *rxq;
	uint32_t bytes_read = 0;
	uint32_t tx_done = 0;
	int ret = 0;

	rxq = queue_id == 0 ? hv->primary : dev->data->rx_queues[queue_id];

	/*
	 * Since channel is shared between Rx and TX queue need to have a lock
	 * since DPDK does not force same CPU to be used for Rx/Tx.
	 */
	if (unlikely(!rte_spinlock_trylock(&rxq->ring_lock)))
		return 0;

	for (;;) {
		const struct vmbus_chanpkt_hdr *pkt;
		uint32_t len = rxq->event_sz;
		const void *data;

retry:
		ret = rte_vmbus_chan_recv_raw(rxq->chan, rxq->event_buf, &len);
		if (ret == -EAGAIN)
			break;	/* ring is empty */

		if (unlikely(ret == -ENOBUFS)) {
			/* event buffer not large enough to read ring */

			PMD_DRV_LOG(DEBUG,
				    "event buffer expansion (need %u)", len);
			rxq->event_sz = len + len / 4;
			rxq->event_buf = rte_realloc(rxq->event_buf, rxq->event_sz,
						     RTE_CACHE_LINE_SIZE);
			if (rxq->event_buf)
				goto retry;
			/* out of memory, no more events now */
			rxq->event_sz = 0;
			break;
		}

		if (unlikely(ret <= 0)) {
			/* This indicates a failure to communicate (or worse) */
			rte_exit(EXIT_FAILURE,
				 "vmbus ring buffer error: %d", ret);
		}

		bytes_read += ret;
		pkt = (const struct vmbus_chanpkt_hdr *)rxq->event_buf;
		data = (char *)rxq->event_buf + vmbus_chanpkt_getlen(pkt->hlen);

		switch (pkt->type) {
		case VMBUS_CHANPKT_TYPE_COMP:
			++tx_done;
			hn_nvs_handle_comp(dev, queue_id, pkt, data);
			break;

		case VMBUS_CHANPKT_TYPE_RXBUF:
			hn_nvs_handle_rxbuf(dev, hv, rxq, pkt, data);
			break;

		case VMBUS_CHANPKT_TYPE_INBAND:
			hn_nvs_handle_notify(dev, pkt, data);
			break;

		default:
			PMD_DRV_LOG(ERR, "unknown chan pkt %u", pkt->type);
			break;
		}

		if (tx_limit && tx_done >= tx_limit)
			break;
	}

	if (bytes_read > 0)
		rte_vmbus_chan_signal_read(hn_nvs_get_vmbus_device(hv), rxq->chan, bytes_read);

	rte_spinlock_unlock(&rxq->ring_lock);

	return tx_done;
}

static void hn_append_to_chim(struct hn_tx_queue *txq,
			      struct rndis_packet_msg *pkt,
			      const struct rte_mbuf *m)
{
	struct hn_txdesc *txd = txq->agg_txd;
	uint8_t *buf = (uint8_t *)pkt;
	unsigned int data_offs;

	hn_rndis_dump(pkt);

	data_offs = RNDIS_PACKET_MSG_OFFSET_ABS(pkt->dataoffset);
	txd->chim_size += pkt->len;
	txd->data_size += m->pkt_len;
	++txd->packets;
	hn_update_packet_stats(&txq->stats, m);

	for (; m; m = m->next) {
		uint16_t len = rte_pktmbuf_data_len(m);

		rte_memcpy(buf + data_offs,
			   rte_pktmbuf_mtod(m, const char *), len);
		data_offs += len;
	}
}

/*
 * Send pending aggregated data in chimney buffer (if any).
 * Returns error if send was unsuccessful because channel ring buffer
 * was full.
 */
static int hn_flush_txagg(struct hn_tx_queue *txq, bool *need_sig)

{
	struct hn_txdesc *txd = txq->agg_txd;
	struct hn_nvs_rndis rndis;
	int ret;

	if (!txd)
		return 0;

	rndis = (struct hn_nvs_rndis) {
		.type = NVS_TYPE_RNDIS,
		.rndis_mtype = NVS_RNDIS_MTYPE_DATA,
		.chim_idx = txd->chim_index,
		.chim_sz = txd->chim_size,
	};

	PMD_TX_LOG(DEBUG, "port %u:%u tx %u size %u",
		   txq->port_id, txq->queue_id, txd->chim_index, txd->chim_size);

	ret = hn_nvs_send(txq->hv, txq->chan, VMBUS_CHANPKT_FLAG_RC,
			  &rndis, sizeof(rndis), (uintptr_t)txd, need_sig);

	if (likely(ret == 0))
		hn_reset_txagg(txq);
	else if (ret == -EAGAIN) {
		PMD_TX_LOG(DEBUG, "port %u:%u channel full",
			   txq->port_id, txq->queue_id);
		++txq->stats.channel_full;
	} else {
		++txq->stats.errors;

		PMD_DRV_LOG(NOTICE, "port %u:%u send failed: %d",
			   txq->port_id, txq->queue_id, ret);
	}
	return ret;
}

/*
 * Try and find a place in a send chimney buffer to put
 * the small packet. If space is available, this routine
 * returns a pointer of where to place the data.
 * If no space, caller should try direct transmit.
 */
static void *
hn_try_txagg(struct hn_data *hv, struct hn_tx_queue *txq,
	     struct hn_txdesc *txd, uint32_t pktsize)
{
	struct hn_txdesc *agg_txd = txq->agg_txd;
	struct rndis_packet_msg *pkt;
	void *chim;

	if (agg_txd) {
		unsigned int padding, olen;

		/*
		 * Update the previous RNDIS packet's total length,
		 * it can be increased due to the mandatory alignment
		 * padding for this RNDIS packet.  And update the
		 * aggregating txdesc's chimney sending buffer size
		 * accordingly.
		 *
		 * Zero-out the padding, as required by the RNDIS spec.
		 */
		pkt = txq->agg_prevpkt;
		olen = pkt->len;
		padding = RTE_ALIGN(olen, txq->agg_align) - olen;
		if (padding > 0) {
			agg_txd->chim_size += padding;
			pkt->len += padding;
			memset((uint8_t *)pkt + olen, 0, padding);
		}

		chim = (uint8_t *)pkt + pkt->len;
		txq->agg_prevpkt = chim;
		txq->agg_pktleft--;
		txq->agg_szleft -= pktsize;
		if (txq->agg_szleft < HN_PKTSIZE_MIN(txq->agg_align)) {
			/*
			 * Probably can't aggregate more packets,
			 * flush this aggregating txdesc proactively.
			 */
			txq->agg_pktleft = 0;
		}

		hn_txd_put(txq, txd);
		return chim;
	}

	txd->chim_index = hn_chim_alloc(hv);
	if (txd->chim_index == NVS_CHIM_IDX_INVALID)
		return NULL;

	chim = (uint8_t *)hv->chim_res.addr
			+ txd->chim_index * hv->chim_szmax;

	txq->agg_txd = txd;
	txq->agg_pktleft = txq->agg_pktmax - 1;
	txq->agg_szleft = txq->agg_szmax - pktsize;
	txq->agg_prevpkt = chim;

	return chim;
}

static inline void *
hn_rndis_pktinfo_append(struct rndis_packet_msg *pkt,
			uint32_t pi_dlen, uint32_t pi_type)
{
	const uint32_t pi_size = RNDIS_PKTINFO_SIZE(pi_dlen);
	struct rndis_pktinfo *pi;

	/*
	 * Per-packet-info does not move; it only grows.
	 *
	 * NOTE:
	 * pktinfooffset in this phase counts from the beginning
	 * of rndis_packet_msg.
	 */
	pi = (struct rndis_pktinfo *)((uint8_t *)pkt + hn_rndis_pktlen(pkt));

	pkt->pktinfolen += pi_size;

	pi->size = pi_size;
	pi->type = pi_type;
	pi->offset = RNDIS_PKTINFO_OFFSET;

	return pi->data;
}

/* Put RNDIS header and packet info on packet */
static void hn_encap(struct rndis_packet_msg *pkt,
		     uint16_t queue_id,
		     const struct rte_mbuf *m)
{
	unsigned int hlen = m->l2_len + m->l3_len;
	uint32_t *pi_data;
	uint32_t pkt_hlen;

	pkt->type = RNDIS_PACKET_MSG;
	pkt->len = m->pkt_len;
	pkt->dataoffset = 0;
	pkt->datalen = m->pkt_len;
	pkt->oobdataoffset = 0;
	pkt->oobdatalen = 0;
	pkt->oobdataelements = 0;
	pkt->pktinfooffset = sizeof(*pkt);
	pkt->pktinfolen = 0;
	pkt->vchandle = 0;
	pkt->reserved = 0;

	/*
	 * Set the hash value for this packet, to the queue_id to cause
	 * TX done event for this packet on the right channel.
	 */
	pi_data = hn_rndis_pktinfo_append(pkt, NDIS_HASH_VALUE_SIZE,
					  NDIS_PKTINFO_TYPE_HASHVAL);
	*pi_data = queue_id;

	if (m->ol_flags & RTE_MBUF_F_TX_VLAN) {
		pi_data = hn_rndis_pktinfo_append(pkt, NDIS_VLAN_INFO_SIZE,
						  NDIS_PKTINFO_TYPE_VLAN);
		*pi_data = NDIS_VLAN_INFO_MAKE(RTE_VLAN_TCI_ID(m->vlan_tci),
					       RTE_VLAN_TCI_PRI(m->vlan_tci),
					       RTE_VLAN_TCI_DEI(m->vlan_tci));
	}

	if (m->ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		pi_data = hn_rndis_pktinfo_append(pkt, NDIS_LSO2_INFO_SIZE,
						  NDIS_PKTINFO_TYPE_LSO);

		if (m->ol_flags & RTE_MBUF_F_TX_IPV6) {
			*pi_data = NDIS_LSO2_INFO_MAKEIPV6(hlen,
							   m->tso_segsz);
		} else {
			*pi_data = NDIS_LSO2_INFO_MAKEIPV4(hlen,
							   m->tso_segsz);
		}
	} else if ((m->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
			RTE_MBUF_F_TX_TCP_CKSUM ||
		   (m->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
			RTE_MBUF_F_TX_UDP_CKSUM ||
		   (m->ol_flags & RTE_MBUF_F_TX_IP_CKSUM)) {
		pi_data = hn_rndis_pktinfo_append(pkt, NDIS_TXCSUM_INFO_SIZE,
						  NDIS_PKTINFO_TYPE_CSUM);
		*pi_data = 0;

		if (m->ol_flags & RTE_MBUF_F_TX_IPV6)
			*pi_data |= NDIS_TXCSUM_INFO_IPV6;
		if (m->ol_flags & RTE_MBUF_F_TX_IPV4) {
			*pi_data |= NDIS_TXCSUM_INFO_IPV4;

			if (m->ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
				*pi_data |= NDIS_TXCSUM_INFO_IPCS;
		}

		if ((m->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
				RTE_MBUF_F_TX_TCP_CKSUM)
			*pi_data |= NDIS_TXCSUM_INFO_MKTCPCS(hlen);
		else if ((m->ol_flags & RTE_MBUF_F_TX_L4_MASK) ==
				RTE_MBUF_F_TX_UDP_CKSUM)
			*pi_data |= NDIS_TXCSUM_INFO_MKUDPCS(hlen);
	}

	pkt_hlen = pkt->pktinfooffset + pkt->pktinfolen;
	/* Fixup RNDIS packet message total length */
	pkt->len += pkt_hlen;

	/* Convert RNDIS packet message offsets */
	pkt->dataoffset = hn_rndis_pktmsg_offset(pkt_hlen);
	pkt->pktinfooffset = hn_rndis_pktmsg_offset(pkt->pktinfooffset);
}

/* How many scatter gather list elements ar needed */
static unsigned int hn_get_slots(const struct rte_mbuf *m)
{
	unsigned int slots = 1; /* for RNDIS header */

	while (m) {
		unsigned int size = rte_pktmbuf_data_len(m);
		unsigned int offs = rte_mbuf_data_iova(m) & HYPERV_PAGE_MASK;

		slots += (offs + size + HYPERV_PAGE_SIZE - 1) /
				HYPERV_PAGE_SIZE;
		m = m->next;
	}

	return slots;
}

/* Build scatter gather list from chained mbuf */
static unsigned int hn_fill_sg(struct vmbus_gpa *sg,
			       const struct rte_mbuf *m)
{
	unsigned int segs = 0;

	while (m) {
		rte_iova_t addr = rte_mbuf_data_iova(m);
		unsigned int page = addr / HYPERV_PAGE_SIZE;
		unsigned int offset = addr & HYPERV_PAGE_MASK;
		unsigned int len = rte_pktmbuf_data_len(m);

		while (len > 0) {
			unsigned int bytes = RTE_MIN(len,
					HYPERV_PAGE_SIZE - offset);

			sg[segs].page = page;
			sg[segs].ofs = offset;
			sg[segs].len = bytes;
			segs++;

			++page;
			offset = 0;
			len -= bytes;
		}
		m = m->next;
	}

	return segs;
}

/* Transmit directly from mbuf */
static int hn_xmit_sg(struct hn_tx_queue *txq,
		      const struct hn_txdesc *txd, const struct rte_mbuf *m,
		      bool *need_sig)
{
	struct vmbus_gpa sg[hn_get_slots(m)];
	struct hn_nvs_rndis nvs_rndis = {
		.type = NVS_TYPE_RNDIS,
		.rndis_mtype = NVS_RNDIS_MTYPE_DATA,
		.chim_sz = txd->chim_size,
	};
	rte_iova_t addr;
	unsigned int segs;

	/* attach aggregation data if present */
	if (txd->chim_size > 0)
		nvs_rndis.chim_idx = txd->chim_index;
	else
		nvs_rndis.chim_idx = NVS_CHIM_IDX_INVALID;

	hn_rndis_dump(txd->rndis_pkt);

	/* pass IOVA of rndis header in first segment */
	addr = txq->tx_rndis_iova +
		((char *)txd->rndis_pkt - (char *)txq->tx_rndis);

	sg[0].page = addr / HYPERV_PAGE_SIZE;
	sg[0].ofs = addr & HYPERV_PAGE_MASK;
	sg[0].len = RNDIS_PACKET_MSG_OFFSET_ABS(hn_rndis_pktlen(txd->rndis_pkt));
	segs = 1;

	hn_update_packet_stats(&txq->stats, m);

	segs += hn_fill_sg(sg + 1, m);

	PMD_TX_LOG(DEBUG, "port %u:%u tx %u segs %u size %u",
		   txq->port_id, txq->queue_id, txd->chim_index,
		   segs, nvs_rndis.chim_sz);

	return hn_nvs_send_sglist(txq->hv, txq->chan, sg, segs, &nvs_rndis,
				  sizeof(nvs_rndis), (uintptr_t)txd, need_sig);
}

uint16_t
hn_xmit_pkts(void *ptxq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct hn_tx_queue *txq = ptxq;
	uint16_t queue_id = txq->queue_id;
	struct hn_data *hv = txq->hv;
	struct rte_eth_dev *vf_dev;
	bool need_sig = false;
	uint16_t nb_tx, tx_thresh;
	int ret;

	if (unlikely(hv->closed))
		return 0;

	/*
	 * Always check for events on the primary channel
	 * because that is where hotplug notifications occur.
	 */
	tx_thresh = RTE_MAX(txq->free_thresh, nb_pkts);
	if (txq->queue_id == 0 ||
	    rte_mempool_avail_count(txq->txdesc_pool) < tx_thresh)
		hn_process_events(hv, txq->queue_id, 0);

	/* Transmit over VF if present and up */
	if (hv->vf_ctx.vf_vsc_switched) {
		rte_rwlock_read_lock(&hv->vf_lock);
		vf_dev = hn_get_vf_dev(hv);
		if (hv->vf_ctx.vf_vsc_switched && vf_dev &&
		    vf_dev->data->dev_started) {
			void *sub_q = vf_dev->data->tx_queues[queue_id];

			nb_tx = (*vf_dev->tx_pkt_burst)
					(sub_q, tx_pkts, nb_pkts);
			rte_rwlock_read_unlock(&hv->vf_lock);
			return nb_tx;
		}
		rte_rwlock_read_unlock(&hv->vf_lock);
	}

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *m = tx_pkts[nb_tx];
		struct rndis_packet_msg *pkt;
		struct hn_txdesc *txd;
		uint32_t pkt_size;

		txd = hn_txd_get(txq);
		if (txd == NULL)
			break;

		if (!(m->ol_flags & RTE_MBUF_F_TX_VLAN)) {
			struct rte_ether_hdr *eh =
				rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
			struct rte_vlan_hdr *vh;

			/* Force TX vlan offloading for 801.2Q packet */
			if (eh->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
				vh = (struct rte_vlan_hdr *)(eh + 1);
				m->ol_flags |= RTE_MBUF_F_TX_VLAN;
				m->vlan_tci = rte_be_to_cpu_16(vh->vlan_tci);

				/* Copy ether header over */
				memmove(rte_pktmbuf_adj(m, sizeof(struct rte_vlan_hdr)),
					eh, 2 * RTE_ETHER_ADDR_LEN);
			}
		}
		pkt_size = m->pkt_len + HN_RNDIS_PKT_LEN;

		/* For small packets aggregate them in chimney buffer */
		if (m->pkt_len <= hv->tx_copybreak &&
		    pkt_size <= txq->agg_szmax) {
			/* If this packet will not fit, then flush  */
			if (txq->agg_pktleft == 0 ||
			    RTE_ALIGN(pkt_size, txq->agg_align) > txq->agg_szleft) {
				if (hn_flush_txagg(txq, &need_sig))
					goto fail;
			}


			pkt = hn_try_txagg(hv, txq, txd, pkt_size);
			if (unlikely(!pkt))
				break;

			hn_encap(pkt, queue_id, m);
			hn_append_to_chim(txq, pkt, m);

			rte_pktmbuf_free(m);

			/* if buffer is full, flush */
			if (txq->agg_pktleft == 0 &&
			    hn_flush_txagg(txq, &need_sig))
				goto fail;
		} else {
			/* Send any outstanding packets in buffer */
			if (txq->agg_txd && hn_flush_txagg(txq, &need_sig))
				goto fail;

			pkt = txd->rndis_pkt;
			txd->m = m;
			txd->data_size = m->pkt_len;
			++txd->packets;

			hn_encap(pkt, queue_id, m);

			ret = hn_xmit_sg(txq, txd, m, &need_sig);
			if (unlikely(ret != 0)) {
				if (ret == -EAGAIN) {
					PMD_TX_LOG(DEBUG, "sg channel full");
					++txq->stats.channel_full;
				} else {
					PMD_DRV_LOG(NOTICE, "sg send failed: %d", ret);
					++txq->stats.errors;
				}
				hn_txd_put(txq, txd);
				goto fail;
			}
		}
	}

	/* If partial buffer left, then try and send it.
	 * if that fails, then reuse it on next send.
	 */
	hn_flush_txagg(txq, &need_sig);

fail:
	if (need_sig)
		rte_vmbus_chan_signal_tx(hn_nvs_get_vmbus_device(hv), txq->chan);

	return nb_tx;
}

static uint16_t
hn_recv_vf(uint16_t vf_port, const struct hn_rx_queue *rxq,
	   struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	uint16_t i, n;

	if (unlikely(nb_pkts == 0))
		return 0;

	n = rte_eth_rx_burst(vf_port, rxq->queue_id, rx_pkts, nb_pkts);

	/* relabel the received mbufs */
	for (i = 0; i < n; i++)
		rx_pkts[i]->port = rxq->port_id;

	return n;
}

uint16_t
hn_recv_pkts(void *prxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct hn_rx_queue *rxq = prxq;
	struct hn_data *hv = rxq->hv;
	struct rte_eth_dev *vf_dev;
	uint16_t nb_rcv;

	if (unlikely(hv->closed))
		return 0;

	/* Check for new completions (and hotplug) */
	if (likely(rte_ring_count(rxq->rx_ring) < nb_pkts))
		hn_process_events(hv, rxq->queue_id, 0);

	/* Always check the vmbus path for multicast and new flows */
	nb_rcv = rte_ring_sc_dequeue_burst(rxq->rx_ring,
					   (void **)rx_pkts, nb_pkts, NULL);

	/* If VF is available, check that as well */
	if (hv->vf_ctx.vf_vsc_switched) {
		rte_rwlock_read_lock(&hv->vf_lock);
		vf_dev = hn_get_vf_dev(hv);
		if (hv->vf_ctx.vf_vsc_switched && vf_dev &&
		    vf_dev->data->dev_started)
			nb_rcv += hn_recv_vf(vf_dev->data->port_id, rxq,
					     rx_pkts + nb_rcv,
					     nb_pkts - nb_rcv);

		rte_rwlock_read_unlock(&hv->vf_lock);
	}
	return nb_rcv;
}

void
hn_dev_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct hn_rx_queue *rxq = dev->data->rx_queues[i];

		hn_rx_queue_free(rxq, false);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		hn_dev_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}
