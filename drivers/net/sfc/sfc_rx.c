/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_mempool.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_flow_tunnel.h"
#include "sfc_log.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_mae_counter.h"
#include "sfc_kvargs.h"
#include "sfc_tweak.h"

/*
 * Maximum number of Rx queue flush attempt in the case of failure or
 * flush timeout
 */
#define SFC_RX_QFLUSH_ATTEMPTS		(3)

/*
 * Time to wait between event queue polling attempts when waiting for Rx
 * queue flush done or failed events.
 */
#define SFC_RX_QFLUSH_POLL_WAIT_MS	(1)

/*
 * Maximum number of event queue polling attempts when waiting for Rx queue
 * flush done or failed events. It defines Rx queue flush attempt timeout
 * together with SFC_RX_QFLUSH_POLL_WAIT_MS.
 */
#define SFC_RX_QFLUSH_POLL_ATTEMPTS	(2000)

void
sfc_rx_qflush_done(struct sfc_rxq_info *rxq_info)
{
	rxq_info->state |= SFC_RXQ_FLUSHED;
	rxq_info->state &= ~SFC_RXQ_FLUSHING;
}

void
sfc_rx_qflush_failed(struct sfc_rxq_info *rxq_info)
{
	rxq_info->state |= SFC_RXQ_FLUSH_FAILED;
	rxq_info->state &= ~SFC_RXQ_FLUSHING;
}

/* This returns the running counter, which is not bounded by ring size */
unsigned int
sfc_rx_get_pushed(struct sfc_adapter *sa, struct sfc_dp_rxq *dp_rxq)
{
	SFC_ASSERT(sa->priv.dp_rx->get_pushed != NULL);

	return sa->priv.dp_rx->get_pushed(dp_rxq);
}

static int
sfc_efx_rx_qprime(struct sfc_efx_rxq *rxq)
{
	int rc = 0;

	if (rxq->evq->read_ptr_primed != rxq->evq->read_ptr) {
		rc = efx_ev_qprime(rxq->evq->common, rxq->evq->read_ptr);
		if (rc == 0)
			rxq->evq->read_ptr_primed = rxq->evq->read_ptr;
	}
	return rc;
}

static void
sfc_efx_rx_qrefill(struct sfc_efx_rxq *rxq)
{
	unsigned int free_space;
	unsigned int bulks;
	void *objs[SFC_RX_REFILL_BULK];
	efsys_dma_addr_t addr[RTE_DIM(objs)];
	unsigned int added = rxq->added;
	unsigned int id;
	unsigned int i;
	struct sfc_efx_rx_sw_desc *rxd;
	struct rte_mbuf *m;
	uint16_t port_id = rxq->dp.dpq.port_id;

	free_space = rxq->max_fill_level - (added - rxq->completed);

	if (free_space < rxq->refill_threshold)
		return;

	bulks = free_space / RTE_DIM(objs);
	/* refill_threshold guarantees that bulks is positive */
	SFC_ASSERT(bulks > 0);

	id = added & rxq->ptr_mask;
	do {
		if (unlikely(rte_mempool_get_bulk(rxq->refill_mb_pool, objs,
						  RTE_DIM(objs)) < 0)) {
			/*
			 * It is hardly a safe way to increment counter
			 * from different contexts, but all PMDs do it.
			 */
			rxq->evq->sa->eth_dev->data->rx_mbuf_alloc_failed +=
				RTE_DIM(objs);
			/* Return if we have posted nothing yet */
			if (added == rxq->added)
				return;
			/* Push posted */
			break;
		}

		for (i = 0; i < RTE_DIM(objs);
		     ++i, id = (id + 1) & rxq->ptr_mask) {
			m = objs[i];

			__rte_mbuf_raw_sanity_check(m);

			rxd = &rxq->sw_desc[id];
			rxd->mbuf = m;

			m->data_off = RTE_PKTMBUF_HEADROOM;
			m->port = port_id;

			addr[i] = rte_pktmbuf_iova(m);
		}

		efx_rx_qpost(rxq->common, addr, rxq->buf_size,
			     RTE_DIM(objs), rxq->completed, added);
		added += RTE_DIM(objs);
	} while (--bulks > 0);

	SFC_ASSERT(added != rxq->added);
	rxq->added = added;
	efx_rx_qpush(rxq->common, added, &rxq->pushed);
	rxq->dp.dpq.dbells++;
}

static uint64_t
sfc_efx_rx_desc_flags_to_offload_flags(const unsigned int desc_flags)
{
	uint64_t mbuf_flags = 0;

	switch (desc_flags & (EFX_PKT_IPV4 | EFX_CKSUM_IPV4)) {
	case (EFX_PKT_IPV4 | EFX_CKSUM_IPV4):
		mbuf_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		break;
	case EFX_PKT_IPV4:
		mbuf_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		break;
	default:
		RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN != 0);
		SFC_ASSERT((mbuf_flags & RTE_MBUF_F_RX_IP_CKSUM_MASK) ==
			   RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN);
		break;
	}

	switch ((desc_flags &
		 (EFX_PKT_TCP | EFX_PKT_UDP | EFX_CKSUM_TCPUDP))) {
	case (EFX_PKT_TCP | EFX_CKSUM_TCPUDP):
	case (EFX_PKT_UDP | EFX_CKSUM_TCPUDP):
		mbuf_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		break;
	case EFX_PKT_TCP:
	case EFX_PKT_UDP:
		mbuf_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
		break;
	default:
		RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN != 0);
		SFC_ASSERT((mbuf_flags & RTE_MBUF_F_RX_L4_CKSUM_MASK) ==
			   RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN);
		break;
	}

	return mbuf_flags;
}

static uint32_t
sfc_efx_rx_desc_flags_to_packet_type(const unsigned int desc_flags)
{
	return RTE_PTYPE_L2_ETHER |
		((desc_flags & EFX_PKT_IPV4) ?
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN : 0) |
		((desc_flags & EFX_PKT_IPV6) ?
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN : 0) |
		((desc_flags & EFX_PKT_TCP) ? RTE_PTYPE_L4_TCP : 0) |
		((desc_flags & EFX_PKT_UDP) ? RTE_PTYPE_L4_UDP : 0);
}

static const uint32_t *
sfc_efx_supported_ptypes_get(__rte_unused uint32_t tunnel_encaps)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

static void
sfc_efx_rx_set_rss_hash(struct sfc_efx_rxq *rxq, unsigned int flags,
			struct rte_mbuf *m)
{
	uint8_t *mbuf_data;


	if ((rxq->flags & SFC_EFX_RXQ_FLAG_RSS_HASH) == 0)
		return;

	mbuf_data = rte_pktmbuf_mtod(m, uint8_t *);

	if (flags & (EFX_PKT_IPV4 | EFX_PKT_IPV6)) {
		m->hash.rss = efx_pseudo_hdr_hash_get(rxq->common,
						      EFX_RX_HASHALG_TOEPLITZ,
						      mbuf_data);

		m->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}
}

static uint16_t
sfc_efx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct sfc_dp_rxq *dp_rxq = rx_queue;
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);
	unsigned int completed;
	unsigned int prefix_size = rxq->prefix_size;
	unsigned int done_pkts = 0;
	boolean_t discard_next = B_FALSE;
	struct rte_mbuf *scatter_pkt = NULL;

	if (unlikely((rxq->flags & SFC_EFX_RXQ_FLAG_RUNNING) == 0))
		return 0;

	sfc_ev_qpoll(rxq->evq);

	completed = rxq->completed;
	while (completed != rxq->pending && done_pkts < nb_pkts) {
		unsigned int id;
		struct sfc_efx_rx_sw_desc *rxd;
		struct rte_mbuf *m;
		unsigned int seg_len;
		unsigned int desc_flags;

		id = completed++ & rxq->ptr_mask;
		rxd = &rxq->sw_desc[id];
		m = rxd->mbuf;
		desc_flags = rxd->flags;

		if (discard_next)
			goto discard;

		if (desc_flags & (EFX_ADDR_MISMATCH | EFX_DISCARD))
			goto discard;

		if (desc_flags & EFX_PKT_PREFIX_LEN) {
			uint16_t tmp_size;
			int rc __rte_unused;

			rc = efx_pseudo_hdr_pkt_length_get(rxq->common,
				rte_pktmbuf_mtod(m, uint8_t *), &tmp_size);
			SFC_ASSERT(rc == 0);
			seg_len = tmp_size;
		} else {
			seg_len = rxd->size - prefix_size;
		}

		rte_pktmbuf_data_len(m) = seg_len;
		rte_pktmbuf_pkt_len(m) = seg_len;

		if (scatter_pkt != NULL) {
			if (rte_pktmbuf_chain(scatter_pkt, m) != 0) {
				rte_pktmbuf_free(scatter_pkt);
				goto discard;
			}
			/* The packet to deliver */
			m = scatter_pkt;
		}

		if (desc_flags & EFX_PKT_CONT) {
			/* The packet is scattered, more fragments to come */
			scatter_pkt = m;
			/* Further fragments have no prefix */
			prefix_size = 0;
			continue;
		}

		/* Scattered packet is done */
		scatter_pkt = NULL;
		/* The first fragment of the packet has prefix */
		prefix_size = rxq->prefix_size;

		m->ol_flags =
			sfc_efx_rx_desc_flags_to_offload_flags(desc_flags);
		m->packet_type =
			sfc_efx_rx_desc_flags_to_packet_type(desc_flags);

		/*
		 * Extract RSS hash from the packet prefix and
		 * set the corresponding field (if needed and possible)
		 */
		sfc_efx_rx_set_rss_hash(rxq, desc_flags, m);

		m->data_off += prefix_size;

		*rx_pkts++ = m;
		done_pkts++;
		continue;

discard:
		discard_next = ((desc_flags & EFX_PKT_CONT) != 0);
		rte_mbuf_raw_free(m);
		rxd->mbuf = NULL;
	}

	/* pending is only moved when entire packet is received */
	SFC_ASSERT(scatter_pkt == NULL);

	rxq->completed = completed;

	sfc_efx_rx_qrefill(rxq);

	if (rxq->flags & SFC_EFX_RXQ_FLAG_INTR_EN)
		sfc_efx_rx_qprime(rxq);

	return done_pkts;
}

static sfc_dp_rx_qdesc_npending_t sfc_efx_rx_qdesc_npending;
static unsigned int
sfc_efx_rx_qdesc_npending(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);

	if ((rxq->flags & SFC_EFX_RXQ_FLAG_RUNNING) == 0)
		return 0;

	sfc_ev_qpoll(rxq->evq);

	return rxq->pending - rxq->completed;
}

static sfc_dp_rx_qdesc_status_t sfc_efx_rx_qdesc_status;
static int
sfc_efx_rx_qdesc_status(struct sfc_dp_rxq *dp_rxq, uint16_t offset)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);

	if (unlikely(offset > rxq->ptr_mask))
		return -EINVAL;

	/*
	 * Poll EvQ to derive up-to-date 'rxq->pending' figure;
	 * it is required for the queue to be running, but the
	 * check is omitted because API design assumes that it
	 * is the duty of the caller to satisfy all conditions
	 */
	SFC_ASSERT((rxq->flags & SFC_EFX_RXQ_FLAG_RUNNING) ==
		   SFC_EFX_RXQ_FLAG_RUNNING);
	sfc_ev_qpoll(rxq->evq);

	/*
	 * There is a handful of reserved entries in the ring,
	 * but an explicit check whether the offset points to
	 * a reserved entry is neglected since the two checks
	 * below rely on the figures which take the HW limits
	 * into account and thus if an entry is reserved, the
	 * checks will fail and UNAVAIL code will be returned
	 */

	if (offset < (rxq->pending - rxq->completed))
		return RTE_ETH_RX_DESC_DONE;

	if (offset < (rxq->added - rxq->completed))
		return RTE_ETH_RX_DESC_AVAIL;

	return RTE_ETH_RX_DESC_UNAVAIL;
}

boolean_t
sfc_rx_check_scatter(size_t pdu, size_t rx_buf_size, uint32_t rx_prefix_size,
		     boolean_t rx_scatter_enabled, uint32_t rx_scatter_max,
		     const char **error)
{
	uint32_t effective_rx_scatter_max;
	uint32_t rx_scatter_bufs;

	effective_rx_scatter_max = rx_scatter_enabled ? rx_scatter_max : 1;
	rx_scatter_bufs = EFX_DIV_ROUND_UP(pdu + rx_prefix_size, rx_buf_size);

	if (rx_scatter_bufs > effective_rx_scatter_max) {
		if (rx_scatter_enabled)
			*error = "Possible number of Rx scatter buffers exceeds maximum number";
		else
			*error = "Rx scatter is disabled and RxQ mbuf pool object size is too small";
		return B_FALSE;
	}

	return B_TRUE;
}

/** Get Rx datapath ops by the datapath RxQ handle */
const struct sfc_dp_rx *
sfc_dp_rx_by_dp_rxq(const struct sfc_dp_rxq *dp_rxq)
{
	const struct sfc_dp_queue *dpq = &dp_rxq->dpq;
	struct rte_eth_dev *eth_dev;
	struct sfc_adapter_priv *sap;

	SFC_ASSERT(rte_eth_dev_is_valid_port(dpq->port_id));
	eth_dev = &rte_eth_devices[dpq->port_id];

	sap = sfc_adapter_priv_by_eth_dev(eth_dev);

	return sap->dp_rx;
}

struct sfc_rxq_info *
sfc_rxq_info_by_dp_rxq(const struct sfc_dp_rxq *dp_rxq)
{
	const struct sfc_dp_queue *dpq = &dp_rxq->dpq;
	struct rte_eth_dev *eth_dev;
	struct sfc_adapter_shared *sas;

	SFC_ASSERT(rte_eth_dev_is_valid_port(dpq->port_id));
	eth_dev = &rte_eth_devices[dpq->port_id];

	sas = sfc_adapter_shared_by_eth_dev(eth_dev);

	SFC_ASSERT(dpq->queue_id < sas->rxq_count);
	return &sas->rxq_info[dpq->queue_id];
}

struct sfc_rxq *
sfc_rxq_by_dp_rxq(const struct sfc_dp_rxq *dp_rxq)
{
	const struct sfc_dp_queue *dpq = &dp_rxq->dpq;
	struct rte_eth_dev *eth_dev;
	struct sfc_adapter *sa;

	SFC_ASSERT(rte_eth_dev_is_valid_port(dpq->port_id));
	eth_dev = &rte_eth_devices[dpq->port_id];

	sa = sfc_adapter_by_eth_dev(eth_dev);

	SFC_ASSERT(dpq->queue_id < sfc_sa2shared(sa)->rxq_count);
	return &sa->rxq_ctrl[dpq->queue_id];
}

static sfc_dp_rx_qsize_up_rings_t sfc_efx_rx_qsize_up_rings;
static int
sfc_efx_rx_qsize_up_rings(uint16_t nb_rx_desc,
			  __rte_unused struct sfc_dp_rx_hw_limits *limits,
			  __rte_unused struct rte_mempool *mb_pool,
			  unsigned int *rxq_entries,
			  unsigned int *evq_entries,
			  unsigned int *rxq_max_fill_level)
{
	*rxq_entries = nb_rx_desc;
	*evq_entries = nb_rx_desc;
	*rxq_max_fill_level = EFX_RXQ_LIMIT(*rxq_entries);
	return 0;
}

static sfc_dp_rx_qcreate_t sfc_efx_rx_qcreate;
static int
sfc_efx_rx_qcreate(uint16_t port_id, uint16_t queue_id,
		   const struct rte_pci_addr *pci_addr, int socket_id,
		   const struct sfc_dp_rx_qcreate_info *info,
		   struct sfc_dp_rxq **dp_rxqp)
{
	struct sfc_efx_rxq *rxq;
	int rc;

	rc = ENOTSUP;
	if (info->nic_dma_info->nb_regions > 0)
		goto fail_nic_dma;

	rc = ENOMEM;
	rxq = rte_zmalloc_socket("sfc-efx-rxq", sizeof(*rxq),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		goto fail_rxq_alloc;

	sfc_dp_queue_init(&rxq->dp.dpq, port_id, queue_id, pci_addr);

	rc = ENOMEM;
	rxq->sw_desc = rte_calloc_socket("sfc-efx-rxq-sw_desc",
					 info->rxq_entries,
					 sizeof(*rxq->sw_desc),
					 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_desc == NULL)
		goto fail_desc_alloc;

	/* efx datapath is bound to efx control path */
	rxq->evq = sfc_rxq_by_dp_rxq(&rxq->dp)->evq;
	if (info->flags & SFC_RXQ_FLAG_RSS_HASH)
		rxq->flags |= SFC_EFX_RXQ_FLAG_RSS_HASH;
	rxq->ptr_mask = info->rxq_entries - 1;
	rxq->batch_max = info->batch_max;
	rxq->prefix_size = info->prefix_size;
	rxq->max_fill_level = info->max_fill_level;
	rxq->refill_threshold = info->refill_threshold;
	rxq->buf_size = info->buf_size;
	rxq->refill_mb_pool = info->refill_mb_pool;

	*dp_rxqp = &rxq->dp;
	return 0;

fail_desc_alloc:
	rte_free(rxq);

fail_rxq_alloc:
fail_nic_dma:
	return rc;
}

static sfc_dp_rx_qdestroy_t sfc_efx_rx_qdestroy;
static void
sfc_efx_rx_qdestroy(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);

	rte_free(rxq->sw_desc);
	rte_free(rxq);
}


/* Use qstop and qstart functions in the case of qstart failure */
static sfc_dp_rx_qstop_t sfc_efx_rx_qstop;
static sfc_dp_rx_qpurge_t sfc_efx_rx_qpurge;


static sfc_dp_rx_qstart_t sfc_efx_rx_qstart;
static int
sfc_efx_rx_qstart(struct sfc_dp_rxq *dp_rxq,
		  __rte_unused unsigned int evq_read_ptr,
		  const efx_rx_prefix_layout_t *pinfo)
{
	/* libefx-based datapath is specific to libefx-based PMD */
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);
	struct sfc_rxq *crxq = sfc_rxq_by_dp_rxq(dp_rxq);
	int rc;

	/*
	 * libefx API is used to extract information from Rx prefix and
	 * it guarantees consistency. Just do length check to ensure
	 * that we reserved space in Rx buffers correctly.
	 */
	if (rxq->prefix_size != pinfo->erpl_length)
		return ENOTSUP;

	rxq->common = crxq->common;

	rxq->pending = rxq->completed = rxq->added = rxq->pushed = 0;

	sfc_efx_rx_qrefill(rxq);

	rxq->flags |= (SFC_EFX_RXQ_FLAG_STARTED | SFC_EFX_RXQ_FLAG_RUNNING);

	if (rxq->flags & SFC_EFX_RXQ_FLAG_INTR_EN) {
		rc = sfc_efx_rx_qprime(rxq);
		if (rc != 0)
			goto fail_rx_qprime;
	}

	return 0;

fail_rx_qprime:
	sfc_efx_rx_qstop(dp_rxq, NULL);
	sfc_efx_rx_qpurge(dp_rxq);
	return rc;
}

static void
sfc_efx_rx_qstop(struct sfc_dp_rxq *dp_rxq,
		 __rte_unused unsigned int *evq_read_ptr)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);

	rxq->flags &= ~SFC_EFX_RXQ_FLAG_RUNNING;

	/* libefx-based datapath is bound to libefx-based PMD and uses
	 * event queue structure directly. So, there is no necessity to
	 * return EvQ read pointer.
	 */
}

static void
sfc_efx_rx_qpurge(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);
	unsigned int i;
	struct sfc_efx_rx_sw_desc *rxd;

	for (i = rxq->completed; i != rxq->added; ++i) {
		rxd = &rxq->sw_desc[i & rxq->ptr_mask];
		rte_mbuf_raw_free(rxd->mbuf);
		rxd->mbuf = NULL;
		/* Packed stream relies on 0 in inactive SW desc.
		 * Rx queue stop is not performance critical, so
		 * there is no harm to do it always.
		 */
		rxd->flags = 0;
		rxd->size = 0;
	}

	rxq->flags &= ~SFC_EFX_RXQ_FLAG_STARTED;
}

static sfc_dp_rx_intr_enable_t sfc_efx_rx_intr_enable;
static int
sfc_efx_rx_intr_enable(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);
	int rc = 0;

	rxq->flags |= SFC_EFX_RXQ_FLAG_INTR_EN;
	if (rxq->flags & SFC_EFX_RXQ_FLAG_STARTED) {
		rc = sfc_efx_rx_qprime(rxq);
		if (rc != 0)
			rxq->flags &= ~SFC_EFX_RXQ_FLAG_INTR_EN;
	}
	return rc;
}

static sfc_dp_rx_intr_disable_t sfc_efx_rx_intr_disable;
static int
sfc_efx_rx_intr_disable(struct sfc_dp_rxq *dp_rxq)
{
	struct sfc_efx_rxq *rxq = sfc_efx_rxq_by_dp_rxq(dp_rxq);

	/* Cannot disarm, just disable rearm */
	rxq->flags &= ~SFC_EFX_RXQ_FLAG_INTR_EN;
	return 0;
}

struct sfc_dp_rx sfc_efx_rx = {
	.dp = {
		.name		= SFC_KVARG_DATAPATH_EFX,
		.type		= SFC_DP_RX,
		.hw_fw_caps	= SFC_DP_HW_FW_CAP_RX_EFX,
	},
	.features		= SFC_DP_RX_FEAT_INTR,
	.dev_offload_capa	= RTE_ETH_RX_OFFLOAD_CHECKSUM |
				  RTE_ETH_RX_OFFLOAD_RSS_HASH |
				  RTE_ETH_RX_OFFLOAD_KEEP_CRC,
	.queue_offload_capa	= RTE_ETH_RX_OFFLOAD_SCATTER,
	.qsize_up_rings		= sfc_efx_rx_qsize_up_rings,
	.qcreate		= sfc_efx_rx_qcreate,
	.qdestroy		= sfc_efx_rx_qdestroy,
	.qstart			= sfc_efx_rx_qstart,
	.qstop			= sfc_efx_rx_qstop,
	.qpurge			= sfc_efx_rx_qpurge,
	.supported_ptypes_get	= sfc_efx_supported_ptypes_get,
	.qdesc_npending		= sfc_efx_rx_qdesc_npending,
	.qdesc_status		= sfc_efx_rx_qdesc_status,
	.intr_enable		= sfc_efx_rx_intr_enable,
	.intr_disable		= sfc_efx_rx_intr_disable,
	.pkt_burst		= sfc_efx_recv_pkts,
};

static void
sfc_rx_qflush(struct sfc_adapter *sa, sfc_sw_index_t sw_index)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;
	unsigned int retry_count;
	unsigned int wait_count;
	int rc;

	ethdev_qid = sfc_ethdev_rx_qid_by_rxq_sw_index(sas, sw_index);
	rxq_info = &sfc_sa2shared(sa)->rxq_info[sw_index];
	SFC_ASSERT(rxq_info->state & SFC_RXQ_STARTED);

	rxq = &sa->rxq_ctrl[sw_index];

	/*
	 * Retry Rx queue flushing in the case of flush failed or
	 * timeout. In the worst case it can delay for 6 seconds.
	 */
	for (retry_count = 0;
	     ((rxq_info->state & SFC_RXQ_FLUSHED) == 0) &&
	     (retry_count < SFC_RX_QFLUSH_ATTEMPTS);
	     ++retry_count) {
		rc = efx_rx_qflush(rxq->common);
		if (rc != 0) {
			rxq_info->state |= (rc == EALREADY) ?
				SFC_RXQ_FLUSHED : SFC_RXQ_FLUSH_FAILED;
			break;
		}
		rxq_info->state &= ~SFC_RXQ_FLUSH_FAILED;
		rxq_info->state |= SFC_RXQ_FLUSHING;

		/*
		 * Wait for Rx queue flush done or failed event at least
		 * SFC_RX_QFLUSH_POLL_WAIT_MS milliseconds and not more
		 * than 2 seconds (SFC_RX_QFLUSH_POLL_WAIT_MS multiplied
		 * by SFC_RX_QFLUSH_POLL_ATTEMPTS).
		 */
		wait_count = 0;
		do {
			rte_delay_ms(SFC_RX_QFLUSH_POLL_WAIT_MS);
			sfc_ev_qpoll(rxq->evq);
		} while ((rxq_info->state & SFC_RXQ_FLUSHING) &&
			 (wait_count++ < SFC_RX_QFLUSH_POLL_ATTEMPTS));

		if (rxq_info->state & SFC_RXQ_FLUSHING)
			sfc_err(sa, "RxQ %d (internal %u) flush timed out",
				ethdev_qid, sw_index);

		if (rxq_info->state & SFC_RXQ_FLUSH_FAILED)
			sfc_err(sa, "RxQ %d (internal %u) flush failed",
				ethdev_qid, sw_index);

		if (rxq_info->state & SFC_RXQ_FLUSHED)
			sfc_notice(sa, "RxQ %d (internal %u) flushed",
				   ethdev_qid, sw_index);
	}

	sa->priv.dp_rx->qpurge(rxq_info->dp);
}

static int
sfc_rx_default_rxq_set_filter(struct sfc_adapter *sa, struct sfc_rxq *rxq)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	boolean_t need_rss = (rss->channels > 0) ? B_TRUE : B_FALSE;
	struct sfc_port *port = &sa->port;
	int rc;

	/*
	 * If promiscuous or all-multicast mode has been requested, setting
	 * filter for the default Rx queue might fail, in particular, while
	 * running over PCI function which is not a member of corresponding
	 * privilege groups; if this occurs, few iterations will be made to
	 * repeat this step without promiscuous and all-multicast flags set
	 */
retry:
	rc = efx_mac_filter_default_rxq_set(sa->nic, rxq->common, need_rss);
	if (rc == 0)
		return 0;
	else if (rc != EOPNOTSUPP)
		return rc;

	if (port->promisc) {
		sfc_warn(sa, "promiscuous mode has been requested, "
			     "but the HW rejects it");
		sfc_warn(sa, "promiscuous mode will be disabled");

		port->promisc = B_FALSE;
		sa->eth_dev->data->promiscuous = 0;
		rc = sfc_set_rx_mode_unchecked(sa);
		if (rc != 0)
			return rc;

		goto retry;
	}

	if (port->allmulti) {
		sfc_warn(sa, "all-multicast mode has been requested, "
			     "but the HW rejects it");
		sfc_warn(sa, "all-multicast mode will be disabled");

		port->allmulti = B_FALSE;
		sa->eth_dev->data->all_multicast = 0;
		rc = sfc_set_rx_mode_unchecked(sa);
		if (rc != 0)
			return rc;

		goto retry;
	}

	return rc;
}

int
sfc_rx_qstart(struct sfc_adapter *sa, sfc_sw_index_t sw_index)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;
	struct sfc_evq *evq;
	efx_rx_prefix_layout_t pinfo;
	int rc;

	SFC_ASSERT(sw_index < sfc_sa2shared(sa)->rxq_count);
	ethdev_qid = sfc_ethdev_rx_qid_by_rxq_sw_index(sas, sw_index);

	sfc_log_init(sa, "RxQ %d (internal %u)", ethdev_qid, sw_index);

	rxq_info = &sfc_sa2shared(sa)->rxq_info[sw_index];
	SFC_ASSERT(rxq_info->state == SFC_RXQ_INITIALIZED);

	rxq = &sa->rxq_ctrl[sw_index];
	evq = rxq->evq;

	rc = sfc_ev_qstart(evq, sfc_evq_sw_index_by_rxq_sw_index(sa, sw_index));
	if (rc != 0)
		goto fail_ev_qstart;

	switch (rxq_info->type) {
	case EFX_RXQ_TYPE_DEFAULT:
		rc = efx_rx_qcreate(sa->nic, rxq->hw_index, 0, rxq_info->type,
			rxq->buf_size,
			&rxq->mem, rxq_info->entries, 0 /* not used on EF10 */,
			rxq_info->type_flags, evq->common, &rxq->common);
		break;
	case EFX_RXQ_TYPE_ES_SUPER_BUFFER: {
		struct rte_mempool *mp = rxq_info->refill_mb_pool;
		struct rte_mempool_info mp_info;

		rc = rte_mempool_ops_get_info(mp, &mp_info);
		if (rc != 0) {
			/* Positive errno is used in the driver */
			rc = -rc;
			goto fail_mp_get_info;
		}
		if (mp_info.contig_block_size <= 0) {
			rc = EINVAL;
			goto fail_bad_contig_block_size;
		}
		rc = efx_rx_qcreate_es_super_buffer(sa->nic, rxq->hw_index, 0,
			mp_info.contig_block_size, rxq->buf_size,
			mp->header_size + mp->elt_size + mp->trailer_size,
			sa->rxd_wait_timeout_ns,
			&rxq->mem, rxq_info->entries, rxq_info->type_flags,
			evq->common, &rxq->common);
		break;
	}
	default:
		rc = ENOTSUP;
	}
	if (rc != 0)
		goto fail_rx_qcreate;

	rc = efx_rx_prefix_get_layout(rxq->common, &pinfo);
	if (rc != 0)
		goto fail_prefix_get_layout;

	efx_rx_qenable(rxq->common);

	rc = sa->priv.dp_rx->qstart(rxq_info->dp, evq->read_ptr, &pinfo);
	if (rc != 0)
		goto fail_dp_qstart;

	rxq_info->state |= SFC_RXQ_STARTED;

	if (ethdev_qid == 0 && !sfc_sa2shared(sa)->isolated) {
		rc = sfc_rx_default_rxq_set_filter(sa, rxq);
		if (rc != 0)
			goto fail_mac_filter_default_rxq_set;
	}

	/* It seems to be used by DPDK for debug purposes only ('rte_ether') */
	if (ethdev_qid != SFC_ETHDEV_QID_INVALID)
		sa->eth_dev->data->rx_queue_state[ethdev_qid] =
			RTE_ETH_QUEUE_STATE_STARTED;

	return 0;

fail_mac_filter_default_rxq_set:
	sfc_rx_qflush(sa, sw_index);
	sa->priv.dp_rx->qstop(rxq_info->dp, &rxq->evq->read_ptr);
	rxq_info->state = SFC_RXQ_INITIALIZED;

fail_dp_qstart:
	efx_rx_qdestroy(rxq->common);

fail_prefix_get_layout:
fail_rx_qcreate:
fail_bad_contig_block_size:
fail_mp_get_info:
	sfc_ev_qstop(evq);

fail_ev_qstart:
	return rc;
}

void
sfc_rx_qstop(struct sfc_adapter *sa, sfc_sw_index_t sw_index)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;

	SFC_ASSERT(sw_index < sfc_sa2shared(sa)->rxq_count);
	ethdev_qid = sfc_ethdev_rx_qid_by_rxq_sw_index(sas, sw_index);

	sfc_log_init(sa, "RxQ %d (internal %u)", ethdev_qid, sw_index);

	rxq_info = &sfc_sa2shared(sa)->rxq_info[sw_index];

	if (rxq_info->state == SFC_RXQ_INITIALIZED)
		return;
	SFC_ASSERT(rxq_info->state & SFC_RXQ_STARTED);

	/* It seems to be used by DPDK for debug purposes only ('rte_ether') */
	if (ethdev_qid != SFC_ETHDEV_QID_INVALID)
		sa->eth_dev->data->rx_queue_state[ethdev_qid] =
			RTE_ETH_QUEUE_STATE_STOPPED;

	rxq = &sa->rxq_ctrl[sw_index];
	sa->priv.dp_rx->qstop(rxq_info->dp, &rxq->evq->read_ptr);

	if (ethdev_qid == 0)
		efx_mac_filter_default_rxq_clear(sa->nic);

	sfc_rx_qflush(sa, sw_index);

	rxq_info->state = SFC_RXQ_INITIALIZED;

	efx_rx_qdestroy(rxq->common);

	sfc_ev_qstop(rxq->evq);
}

static uint64_t
sfc_rx_get_offload_mask(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	uint64_t no_caps = 0;

	if (encp->enc_tunnel_encapsulations_supported == 0)
		no_caps |= RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM;

	if (encp->enc_rx_include_fcs_supported == 0)
		no_caps |= RTE_ETH_RX_OFFLOAD_KEEP_CRC;

	if (encp->enc_rx_vlan_stripping_supported == 0)
		no_caps |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	return ~no_caps;
}

uint64_t
sfc_rx_get_dev_offload_caps(struct sfc_adapter *sa)
{
	uint64_t caps = sa->priv.dp_rx->dev_offload_capa;

	return caps & sfc_rx_get_offload_mask(sa);
}

uint64_t
sfc_rx_get_queue_offload_caps(struct sfc_adapter *sa)
{
	return sa->priv.dp_rx->queue_offload_capa & sfc_rx_get_offload_mask(sa);
}

static int
sfc_rx_qcheck_conf(struct sfc_adapter *sa, unsigned int rxq_max_fill_level,
		   const struct rte_eth_rxconf *rx_conf,
		   __rte_unused uint64_t offloads)
{
	int rc = 0;

	if (rx_conf->rx_thresh.pthresh != 0 ||
	    rx_conf->rx_thresh.hthresh != 0 ||
	    rx_conf->rx_thresh.wthresh != 0) {
		sfc_warn(sa,
			"RxQ prefetch/host/writeback thresholds are not supported");
	}

	if (rx_conf->rx_free_thresh > rxq_max_fill_level) {
		sfc_err(sa,
			"RxQ free threshold too large: %u vs maximum %u",
			rx_conf->rx_free_thresh, rxq_max_fill_level);
		rc = EINVAL;
	}

	if (rx_conf->rx_drop_en == 0) {
		sfc_err(sa, "RxQ drop disable is not supported");
		rc = EINVAL;
	}

	return rc;
}

static unsigned int
sfc_rx_mbuf_data_alignment(struct rte_mempool *mb_pool)
{
	uint32_t data_off;
	uint32_t order;

	/* The mbuf object itself is always cache line aligned */
	order = rte_bsf32(RTE_CACHE_LINE_SIZE);

	/* Data offset from mbuf object start */
	data_off = sizeof(struct rte_mbuf) + rte_pktmbuf_priv_size(mb_pool) +
		RTE_PKTMBUF_HEADROOM;

	order = MIN(order, rte_bsf32(data_off));

	return 1u << order;
}

static uint16_t
sfc_rx_mb_pool_buf_size(struct sfc_adapter *sa, struct rte_mempool *mb_pool)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	const uint32_t nic_align_start = MAX(1, encp->enc_rx_buf_align_start);
	const uint32_t nic_align_end = MAX(1, encp->enc_rx_buf_align_end);
	uint16_t buf_size;
	unsigned int buf_aligned;
	unsigned int start_alignment;
	unsigned int end_padding_alignment;

	/* Below it is assumed that both alignments are power of 2 */
	SFC_ASSERT(rte_is_power_of_2(nic_align_start));
	SFC_ASSERT(rte_is_power_of_2(nic_align_end));

	/*
	 * mbuf is always cache line aligned, double-check
	 * that it meets rx buffer start alignment requirements.
	 */

	/* Start from mbuf pool data room size */
	buf_size = rte_pktmbuf_data_room_size(mb_pool);

	/* Remove headroom */
	if (buf_size <= RTE_PKTMBUF_HEADROOM) {
		sfc_err(sa,
			"RxQ mbuf pool %s object data room size %u is smaller than headroom %u",
			mb_pool->name, buf_size, RTE_PKTMBUF_HEADROOM);
		return 0;
	}
	buf_size -= RTE_PKTMBUF_HEADROOM;

	/* Calculate guaranteed data start alignment */
	buf_aligned = sfc_rx_mbuf_data_alignment(mb_pool);

	/* Reserve space for start alignment */
	if (buf_aligned < nic_align_start) {
		start_alignment = nic_align_start - buf_aligned;
		if (buf_size <= start_alignment) {
			sfc_err(sa,
				"RxQ mbuf pool %s object data room size %u is insufficient for headroom %u and buffer start alignment %u required by NIC",
				mb_pool->name,
				rte_pktmbuf_data_room_size(mb_pool),
				RTE_PKTMBUF_HEADROOM, start_alignment);
			return 0;
		}
		buf_aligned = nic_align_start;
		buf_size -= start_alignment;
	} else {
		start_alignment = 0;
	}

	/* Make sure that end padding does not write beyond the buffer */
	if (buf_aligned < nic_align_end) {
		/*
		 * Estimate space which can be lost. If guaranteed buffer
		 * size is odd, lost space is (nic_align_end - 1). More
		 * accurate formula is below.
		 */
		end_padding_alignment = nic_align_end -
			MIN(buf_aligned, 1u << (rte_bsf32(buf_size) - 1));
		if (buf_size <= end_padding_alignment) {
			sfc_err(sa,
				"RxQ mbuf pool %s object data room size %u is insufficient for headroom %u, buffer start alignment %u and end padding alignment %u required by NIC",
				mb_pool->name,
				rte_pktmbuf_data_room_size(mb_pool),
				RTE_PKTMBUF_HEADROOM, start_alignment,
				end_padding_alignment);
			return 0;
		}
		buf_size -= end_padding_alignment;
	} else {
		/*
		 * Start is aligned the same or better than end,
		 * just align length.
		 */
		buf_size = EFX_P2ALIGN(uint32_t, buf_size, nic_align_end);
	}

	/*
	 * Buffer length field of a Rx descriptor may not be wide
	 * enough to store a 16-bit data count taken from an mbuf.
	 */
	return MIN(buf_size, encp->enc_rx_dma_desc_size_max);
}

int
sfc_rx_qinit(struct sfc_adapter *sa, sfc_sw_index_t sw_index,
	     uint16_t nb_rx_desc, unsigned int socket_id,
	     const struct rte_eth_rxconf *rx_conf,
	     struct rte_mempool *mb_pool)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	int rc;
	unsigned int rxq_entries;
	unsigned int evq_entries;
	unsigned int rxq_max_fill_level;
	uint64_t offloads;
	uint16_t buf_size;
	struct sfc_rxq_info *rxq_info;
	struct sfc_evq *evq;
	struct sfc_rxq *rxq;
	struct sfc_dp_rx_qcreate_info info;
	struct sfc_dp_rx_hw_limits hw_limits;
	struct sfc_port *port = &sa->port;
	uint16_t rx_free_thresh;
	const char *error;

	memset(&hw_limits, 0, sizeof(hw_limits));
	hw_limits.rxq_max_entries = sa->rxq_max_entries;
	hw_limits.rxq_min_entries = sa->rxq_min_entries;
	hw_limits.evq_max_entries = sa->evq_max_entries;
	hw_limits.evq_min_entries = sa->evq_min_entries;

	rc = sa->priv.dp_rx->qsize_up_rings(nb_rx_desc, &hw_limits, mb_pool,
					    &rxq_entries, &evq_entries,
					    &rxq_max_fill_level);
	if (rc != 0)
		goto fail_size_up_rings;
	SFC_ASSERT(rxq_entries >= sa->rxq_min_entries);
	SFC_ASSERT(rxq_entries <= sa->rxq_max_entries);
	SFC_ASSERT(rxq_max_fill_level <= nb_rx_desc);

	ethdev_qid = sfc_ethdev_rx_qid_by_rxq_sw_index(sas, sw_index);

	offloads = rx_conf->offloads;
	/* Add device level Rx offloads if the queue is an ethdev Rx queue */
	if (ethdev_qid != SFC_ETHDEV_QID_INVALID)
		offloads |= sa->eth_dev->data->dev_conf.rxmode.offloads;

	rc = sfc_rx_qcheck_conf(sa, rxq_max_fill_level, rx_conf, offloads);
	if (rc != 0)
		goto fail_bad_conf;

	buf_size = sfc_rx_mb_pool_buf_size(sa, mb_pool);
	if (buf_size == 0) {
		sfc_err(sa,
			"RxQ %d (internal %u) mbuf pool object size is too small",
			ethdev_qid, sw_index);
		rc = EINVAL;
		goto fail_bad_conf;
	}

	if (!sfc_rx_check_scatter(sa->port.pdu, buf_size,
				  encp->enc_rx_prefix_size,
				  (offloads & RTE_ETH_RX_OFFLOAD_SCATTER),
				  encp->enc_rx_scatter_max,
				  &error)) {
		sfc_err(sa, "RxQ %d (internal %u) MTU check failed: %s",
			ethdev_qid, sw_index, error);
		sfc_err(sa,
			"RxQ %d (internal %u) calculated Rx buffer size is %u vs "
			"PDU size %u plus Rx prefix %u bytes",
			ethdev_qid, sw_index, buf_size,
			(unsigned int)sa->port.pdu, encp->enc_rx_prefix_size);
		rc = EINVAL;
		goto fail_bad_conf;
	}

	SFC_ASSERT(sw_index < sfc_sa2shared(sa)->rxq_count);
	rxq_info = &sfc_sa2shared(sa)->rxq_info[sw_index];

	SFC_ASSERT(rxq_entries <= rxq_info->max_entries);
	rxq_info->entries = rxq_entries;

	if (sa->priv.dp_rx->dp.hw_fw_caps & SFC_DP_HW_FW_CAP_RX_ES_SUPER_BUFFER)
		rxq_info->type = EFX_RXQ_TYPE_ES_SUPER_BUFFER;
	else
		rxq_info->type = EFX_RXQ_TYPE_DEFAULT;

	rxq_info->type_flags |=
		(offloads & RTE_ETH_RX_OFFLOAD_SCATTER) ?
		EFX_RXQ_FLAG_SCATTER : EFX_RXQ_FLAG_NONE;

	if ((encp->enc_tunnel_encapsulations_supported != 0) &&
	    (sfc_dp_rx_offload_capa(sa->priv.dp_rx) &
	     RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) != 0)
		rxq_info->type_flags |= EFX_RXQ_FLAG_INNER_CLASSES;

	if (offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH)
		rxq_info->type_flags |= EFX_RXQ_FLAG_RSS_HASH;

	if ((sa->negotiated_rx_metadata & RTE_ETH_RX_METADATA_USER_FLAG) != 0)
		rxq_info->type_flags |= EFX_RXQ_FLAG_USER_FLAG;

	if ((sa->negotiated_rx_metadata & RTE_ETH_RX_METADATA_USER_MARK) != 0 ||
	    sfc_ft_is_active(sa))
		rxq_info->type_flags |= EFX_RXQ_FLAG_USER_MARK;

	if (port->vlan_strip)
		rxq_info->type_flags |= EFX_RXQ_FLAG_VLAN_STRIPPED_TCI;

	rc = sfc_ev_qinit(sa, SFC_EVQ_TYPE_RX, sw_index,
			  evq_entries, socket_id, &evq);
	if (rc != 0)
		goto fail_ev_qinit;

	rxq = &sa->rxq_ctrl[sw_index];
	rxq->evq = evq;
	rxq->hw_index = sw_index;
	/*
	 * If Rx refill threshold is specified (its value is non zero) in
	 * Rx configuration, use specified value. Otherwise use 1/8 of
	 * the Rx descriptors number as the default. It allows to keep
	 * Rx ring full-enough and does not refill too aggressive if
	 * packet rate is high.
	 *
	 * Since PMD refills in bulks waiting for full bulk may be
	 * refilled (basically round down), it is better to round up
	 * here to mitigate it a bit.
	 */
	rx_free_thresh = (rx_conf->rx_free_thresh != 0) ?
		rx_conf->rx_free_thresh : EFX_DIV_ROUND_UP(nb_rx_desc, 8);
	/* Rx refill threshold cannot be smaller than refill bulk */
	rxq_info->refill_threshold =
		RTE_MAX(rx_free_thresh, SFC_RX_REFILL_BULK);
	rxq_info->refill_mb_pool = mb_pool;

	if (rss->hash_support == EFX_RX_HASH_AVAILABLE && rss->channels > 0 &&
	    (offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH))
		rxq_info->rxq_flags = SFC_RXQ_FLAG_RSS_HASH;
	else
		rxq_info->rxq_flags = 0;

	if (rxq_info->type_flags & EFX_RXQ_FLAG_INGRESS_MPORT)
		rxq_info->rxq_flags |= SFC_RXQ_FLAG_INGRESS_MPORT;

	if (rxq_info->type_flags & EFX_RXQ_FLAG_VLAN_STRIPPED_TCI)
		rxq_info->rxq_flags |= SFC_RXQ_FLAG_VLAN_STRIPPED_TCI;

	rxq->buf_size = buf_size;

	rc = sfc_dma_alloc(sa, "rxq", sw_index, EFX_NIC_DMA_ADDR_RX_RING,
			   efx_rxq_size(sa->nic, rxq_info->entries),
			   socket_id, &rxq->mem);
	if (rc != 0)
		goto fail_dma_alloc;

	memset(&info, 0, sizeof(info));
	info.refill_mb_pool = rxq_info->refill_mb_pool;
	info.max_fill_level = rxq_max_fill_level;
	info.refill_threshold = rxq_info->refill_threshold;
	info.buf_size = buf_size;
	info.batch_max = encp->enc_rx_batch_max;
	info.prefix_size = encp->enc_rx_prefix_size;

	if (sfc_ft_is_active(sa))
		info.user_mark_mask = SFC_FT_USER_MARK_MASK;
	else
		info.user_mark_mask = UINT32_MAX;

	info.flags = rxq_info->rxq_flags;
	info.rxq_entries = rxq_info->entries;
	info.rxq_hw_ring = rxq->mem.esm_base;
	info.evq_hw_index = sfc_evq_sw_index_by_rxq_sw_index(sa, sw_index);
	info.evq_entries = evq_entries;
	info.evq_hw_ring = evq->mem.esm_base;
	info.hw_index = rxq->hw_index;
	info.mem_bar = sa->mem_bar.esb_base;
	info.vi_window_shift = encp->enc_vi_window_shift;
	info.fcw_offset = sa->fcw_offset;

	info.nic_dma_info = &sas->nic_dma_info;

	rc = sa->priv.dp_rx->qcreate(sa->eth_dev->data->port_id, sw_index,
				     &RTE_ETH_DEV_TO_PCI(sa->eth_dev)->addr,
				     socket_id, &info, &rxq_info->dp);
	if (rc != 0)
		goto fail_dp_rx_qcreate;

	evq->dp_rxq = rxq_info->dp;

	rxq_info->state = SFC_RXQ_INITIALIZED;

	rxq_info->deferred_start = (rx_conf->rx_deferred_start != 0);

	return 0;

fail_dp_rx_qcreate:
	sfc_dma_free(sa, &rxq->mem);

fail_dma_alloc:
	sfc_ev_qfini(evq);

fail_ev_qinit:
	rxq_info->entries = 0;

fail_bad_conf:
fail_size_up_rings:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_rx_qfini(struct sfc_adapter *sa, sfc_sw_index_t sw_index)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_ethdev_qid_t ethdev_qid;
	struct sfc_rxq_info *rxq_info;
	struct sfc_rxq *rxq;

	SFC_ASSERT(sw_index < sfc_sa2shared(sa)->rxq_count);
	ethdev_qid = sfc_ethdev_rx_qid_by_rxq_sw_index(sas, sw_index);

	if (ethdev_qid != SFC_ETHDEV_QID_INVALID)
		sa->eth_dev->data->rx_queues[ethdev_qid] = NULL;

	rxq_info = &sfc_sa2shared(sa)->rxq_info[sw_index];

	SFC_ASSERT(rxq_info->state == SFC_RXQ_INITIALIZED);

	sa->priv.dp_rx->qdestroy(rxq_info->dp);
	rxq_info->dp = NULL;

	rxq_info->state &= ~SFC_RXQ_INITIALIZED;
	rxq_info->entries = 0;

	rxq = &sa->rxq_ctrl[sw_index];

	sfc_dma_free(sa, &rxq->mem);

	sfc_ev_qfini(rxq->evq);
	rxq->evq = NULL;
}

/*
 * Mapping between RTE RSS hash functions and their EFX counterparts.
 */
static const struct sfc_rss_hf_rte_to_efx sfc_rss_hf_map[] = {
	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	  EFX_RX_HASH(IPV4_TCP, 4TUPLE) },
	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	  EFX_RX_HASH(IPV4_UDP, 4TUPLE) },
	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_IPV6_TCP_EX,
	  EFX_RX_HASH(IPV6_TCP, 4TUPLE) },
	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_IPV6_UDP_EX,
	  EFX_RX_HASH(IPV6_UDP, 4TUPLE) },
	{ RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	  EFX_RX_HASH(IPV4_TCP, 2TUPLE) | EFX_RX_HASH(IPV4_UDP, 2TUPLE) |
	  EFX_RX_HASH(IPV4, 2TUPLE) },
	{ RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_OTHER |
	  RTE_ETH_RSS_IPV6_EX,
	  EFX_RX_HASH(IPV6_TCP, 2TUPLE) | EFX_RX_HASH(IPV6_UDP, 2TUPLE) |
	  EFX_RX_HASH(IPV6, 2TUPLE) }
};

static efx_rx_hash_type_t
sfc_rx_hash_types_mask_supp(efx_rx_hash_type_t hash_type,
			    unsigned int *hash_type_flags_supported,
			    unsigned int nb_hash_type_flags_supported)
{
	efx_rx_hash_type_t hash_type_masked = 0;
	unsigned int i, j;

	for (i = 0; i < nb_hash_type_flags_supported; ++i) {
		unsigned int class_tuple_lbn[] = {
			EFX_RX_CLASS_IPV4_TCP_LBN,
			EFX_RX_CLASS_IPV4_UDP_LBN,
			EFX_RX_CLASS_IPV4_LBN,
			EFX_RX_CLASS_IPV6_TCP_LBN,
			EFX_RX_CLASS_IPV6_UDP_LBN,
			EFX_RX_CLASS_IPV6_LBN
		};

		for (j = 0; j < RTE_DIM(class_tuple_lbn); ++j) {
			unsigned int tuple_mask = EFX_RX_CLASS_HASH_4TUPLE;
			unsigned int flag;

			tuple_mask <<= class_tuple_lbn[j];
			flag = hash_type & tuple_mask;

			if (flag == hash_type_flags_supported[i])
				hash_type_masked |= flag;
		}
	}

	return hash_type_masked;
}

int
sfc_rx_hash_init(struct sfc_adapter *sa)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	uint32_t alg_mask = encp->enc_rx_scale_hash_alg_mask;
	efx_rx_hash_alg_t alg;
	unsigned int flags_supp[EFX_RX_HASH_NFLAGS];
	unsigned int nb_flags_supp;
	struct sfc_rss_hf_rte_to_efx *hf_map;
	struct sfc_rss_hf_rte_to_efx *entry;
	efx_rx_hash_type_t efx_hash_types;
	unsigned int i;
	int rc;

	if (alg_mask & (1U << EFX_RX_HASHALG_TOEPLITZ))
		alg = EFX_RX_HASHALG_TOEPLITZ;
	else if (alg_mask & (1U << EFX_RX_HASHALG_PACKED_STREAM))
		alg = EFX_RX_HASHALG_PACKED_STREAM;
	else
		return EINVAL;

	rc = efx_rx_scale_hash_flags_get(sa->nic, alg, flags_supp,
					 RTE_DIM(flags_supp), &nb_flags_supp);
	if (rc != 0)
		return rc;

	hf_map = rte_calloc_socket("sfc-rss-hf-map",
				   RTE_DIM(sfc_rss_hf_map),
				   sizeof(*hf_map), 0, sa->socket_id);
	if (hf_map == NULL)
		return ENOMEM;

	entry = hf_map;
	efx_hash_types = 0;
	for (i = 0; i < RTE_DIM(sfc_rss_hf_map); ++i) {
		efx_rx_hash_type_t ht;

		ht = sfc_rx_hash_types_mask_supp(sfc_rss_hf_map[i].efx,
						 flags_supp, nb_flags_supp);
		if (ht != 0) {
			entry->rte = sfc_rss_hf_map[i].rte;
			entry->efx = ht;
			efx_hash_types |= ht;
			++entry;
		}
	}

	rss->hash_alg = alg;
	rss->hf_map_nb_entries = (unsigned int)(entry - hf_map);
	rss->hf_map = hf_map;
	rss->hash_types = efx_hash_types;

	return 0;
}

void
sfc_rx_hash_fini(struct sfc_adapter *sa)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;

	rte_free(rss->hf_map);
}

int
sfc_rx_hf_rte_to_efx(struct sfc_adapter *sa, uint64_t rte,
		     efx_rx_hash_type_t *efx)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	efx_rx_hash_type_t hash_types = 0;
	unsigned int i;

	for (i = 0; i < rss->hf_map_nb_entries; ++i) {
		uint64_t rte_mask = rss->hf_map[i].rte;

		if ((rte & rte_mask) != 0) {
			rte &= ~rte_mask;
			hash_types |= rss->hf_map[i].efx;
		}
	}

	if (rte != 0) {
		sfc_err(sa, "unsupported hash functions requested");
		return EINVAL;
	}

	*efx = hash_types;

	return 0;
}

uint64_t
sfc_rx_hf_efx_to_rte(struct sfc_rss *rss, efx_rx_hash_type_t efx)
{
	uint64_t rte = 0;
	unsigned int i;

	for (i = 0; i < rss->hf_map_nb_entries; ++i) {
		efx_rx_hash_type_t hash_type = rss->hf_map[i].efx;

		if ((efx & hash_type) == hash_type)
			rte |= rss->hf_map[i].rte;
	}

	return rte;
}

static int
sfc_rx_process_adv_conf_rss(struct sfc_adapter *sa,
			    struct rte_eth_rss_conf *conf)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	efx_rx_hash_type_t efx_hash_types = rss->hash_types;
	uint64_t rss_hf = sfc_rx_hf_efx_to_rte(rss, efx_hash_types);
	int rc;

	if (rss->context_type != EFX_RX_SCALE_EXCLUSIVE) {
		if ((conf->rss_hf != 0 && conf->rss_hf != rss_hf) ||
		    conf->rss_key != NULL)
			return EINVAL;
	}

	if (conf->rss_hf != 0) {
		rc = sfc_rx_hf_rte_to_efx(sa, conf->rss_hf, &efx_hash_types);
		if (rc != 0)
			return rc;
	}

	if (conf->rss_key != NULL) {
		if (conf->rss_key_len != sizeof(rss->key)) {
			sfc_err(sa, "RSS key size is wrong (should be %zu)",
				sizeof(rss->key));
			return EINVAL;
		}
		rte_memcpy(rss->key, conf->rss_key, sizeof(rss->key));
	}

	rss->hash_types = efx_hash_types;

	return 0;
}

static int
sfc_rx_rss_config(struct sfc_adapter *sa)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;
	int rc = 0;

	if (rss->channels > 0) {
		rc = efx_rx_scale_mode_set(sa->nic, EFX_RSS_CONTEXT_DEFAULT,
					   rss->hash_alg, rss->hash_types,
					   B_TRUE);
		if (rc != 0)
			goto finish;

		rc = efx_rx_scale_key_set(sa->nic, EFX_RSS_CONTEXT_DEFAULT,
					  rss->key, sizeof(rss->key));
		if (rc != 0)
			goto finish;

		rc = efx_rx_scale_tbl_set(sa->nic, EFX_RSS_CONTEXT_DEFAULT,
					  rss->tbl, RTE_DIM(rss->tbl));
	}

finish:
	return rc;
}

struct sfc_rxq_info *
sfc_rxq_info_by_ethdev_qid(struct sfc_adapter_shared *sas,
			   sfc_ethdev_qid_t ethdev_qid)
{
	sfc_sw_index_t sw_index;

	SFC_ASSERT((unsigned int)ethdev_qid < sas->ethdev_rxq_count);
	SFC_ASSERT(ethdev_qid != SFC_ETHDEV_QID_INVALID);

	sw_index = sfc_rxq_sw_index_by_ethdev_rx_qid(sas, ethdev_qid);
	return &sas->rxq_info[sw_index];
}

struct sfc_rxq *
sfc_rxq_ctrl_by_ethdev_qid(struct sfc_adapter *sa, sfc_ethdev_qid_t ethdev_qid)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	sfc_sw_index_t sw_index;

	SFC_ASSERT((unsigned int)ethdev_qid < sas->ethdev_rxq_count);
	SFC_ASSERT(ethdev_qid != SFC_ETHDEV_QID_INVALID);

	sw_index = sfc_rxq_sw_index_by_ethdev_rx_qid(sas, ethdev_qid);
	return &sa->rxq_ctrl[sw_index];
}

int
sfc_rx_start(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	sfc_sw_index_t sw_index;
	int rc;

	sfc_log_init(sa, "rxq_count=%u (internal %u)", sas->ethdev_rxq_count,
		     sas->rxq_count);

	rc = efx_rx_init(sa->nic);
	if (rc != 0)
		goto fail_rx_init;

	rc = sfc_rx_rss_config(sa);
	if (rc != 0)
		goto fail_rss_config;

	for (sw_index = 0; sw_index < sas->rxq_count; ++sw_index) {
		if (sas->rxq_info[sw_index].state == SFC_RXQ_INITIALIZED &&
		    (!sas->rxq_info[sw_index].deferred_start ||
		     sas->rxq_info[sw_index].deferred_started)) {
			rc = sfc_rx_qstart(sa, sw_index);
			if (rc != 0)
				goto fail_rx_qstart;
		}
	}

	return 0;

fail_rx_qstart:
	while (sw_index-- > 0)
		sfc_rx_qstop(sa, sw_index);

fail_rss_config:
	efx_rx_fini(sa->nic);

fail_rx_init:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

void
sfc_rx_stop(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	sfc_sw_index_t sw_index;

	sfc_log_init(sa, "rxq_count=%u (internal %u)", sas->ethdev_rxq_count,
		     sas->rxq_count);

	sw_index = sas->rxq_count;
	while (sw_index-- > 0) {
		if (sas->rxq_info[sw_index].state & SFC_RXQ_STARTED)
			sfc_rx_qstop(sa, sw_index);
	}

	efx_rx_fini(sa->nic);
}

int
sfc_rx_qinit_info(struct sfc_adapter *sa, sfc_sw_index_t sw_index,
		  unsigned int extra_efx_type_flags)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_rxq_info *rxq_info = &sas->rxq_info[sw_index];
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	unsigned int max_entries;

	max_entries = encp->enc_rxq_max_ndescs;
	SFC_ASSERT(rte_is_power_of_2(max_entries));

	rxq_info->max_entries = max_entries;
	rxq_info->type_flags = extra_efx_type_flags;

	return 0;
}

static int
sfc_rx_check_mode(struct sfc_adapter *sa, struct rte_eth_rxmode *rxmode)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	uint64_t offloads_supported = sfc_rx_get_dev_offload_caps(sa) |
				      sfc_rx_get_queue_offload_caps(sa);
	struct sfc_rss *rss = &sas->rss;
	int rc = 0;

	switch (rxmode->mq_mode) {
	case RTE_ETH_MQ_RX_NONE:
		/* No special checks are required */
		break;
	case RTE_ETH_MQ_RX_RSS:
		if (rss->context_type == EFX_RX_SCALE_UNAVAILABLE) {
			sfc_err(sa, "RSS is not available");
			rc = EINVAL;
		}
		break;
	default:
		sfc_err(sa, "Rx multi-queue mode %u not supported",
			rxmode->mq_mode);
		rc = EINVAL;
	}

	/*
	 * Requested offloads are validated against supported by ethdev,
	 * so unsupported offloads cannot be added as the result of
	 * below check.
	 */
	if ((rxmode->offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM) !=
	    (offloads_supported & RTE_ETH_RX_OFFLOAD_CHECKSUM)) {
		sfc_warn(sa, "Rx checksum offloads cannot be disabled - always on (IPv4/TCP/UDP)");
		rxmode->offloads |= RTE_ETH_RX_OFFLOAD_CHECKSUM;
	}

	if ((offloads_supported & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) &&
	    (~rxmode->offloads & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM)) {
		sfc_warn(sa, "Rx outer IPv4 checksum offload cannot be disabled - always on");
		rxmode->offloads |= RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM;
	}

	return rc;
}

/**
 * Destroy excess queues that are no longer needed after reconfiguration
 * or complete close.
 */
static void
sfc_rx_fini_queues(struct sfc_adapter *sa, unsigned int nb_rx_queues)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	sfc_sw_index_t sw_index;
	sfc_ethdev_qid_t ethdev_qid;

	SFC_ASSERT(nb_rx_queues <= sas->ethdev_rxq_count);

	/*
	 * Finalize only ethdev queues since other ones are finalized only
	 * on device close and they may require additional deinitialization.
	 */
	ethdev_qid = sas->ethdev_rxq_count;
	while (--ethdev_qid >= (int)nb_rx_queues) {
		struct sfc_rxq_info *rxq_info;

		rxq_info = sfc_rxq_info_by_ethdev_qid(sas, ethdev_qid);
		if (rxq_info->state & SFC_RXQ_INITIALIZED) {
			sw_index = sfc_rxq_sw_index_by_ethdev_rx_qid(sas,
								ethdev_qid);
			sfc_rx_qfini(sa, sw_index);
		}

	}

	sas->ethdev_rxq_count = nb_rx_queues;
}

/**
 * Initialize Rx subsystem.
 *
 * Called at device (re)configuration stage when number of receive queues is
 * specified together with other device level receive configuration.
 *
 * It should be used to allocate NUMA-unaware resources.
 */
int
sfc_rx_configure(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_rss *rss = &sas->rss;
	struct rte_eth_conf *dev_conf = &sa->eth_dev->data->dev_conf;
	const unsigned int nb_rx_queues = sa->eth_dev->data->nb_rx_queues;
	const unsigned int nb_rsrv_rx_queues = sfc_nb_reserved_rxq(sas);
	const unsigned int nb_rxq_total = nb_rx_queues + nb_rsrv_rx_queues;
	bool reconfigure;
	int rc;

	sfc_log_init(sa, "nb_rx_queues=%u (old %u)",
		     nb_rx_queues, sas->ethdev_rxq_count);

	rc = sfc_rx_check_mode(sa, &dev_conf->rxmode);
	if (rc != 0)
		goto fail_check_mode;

	if (nb_rxq_total == sas->rxq_count) {
		reconfigure = true;
		goto configure_rss;
	}

	if (sas->rxq_info == NULL) {
		reconfigure = false;
		rc = ENOMEM;
		sas->rxq_info = rte_calloc_socket("sfc-rxqs", nb_rxq_total,
						  sizeof(sas->rxq_info[0]), 0,
						  sa->socket_id);
		if (sas->rxq_info == NULL)
			goto fail_rxqs_alloc;

		/*
		 * Allocate primary process only RxQ control from heap
		 * since it should not be shared.
		 */
		rc = ENOMEM;
		sa->rxq_ctrl = calloc(nb_rxq_total, sizeof(sa->rxq_ctrl[0]));
		if (sa->rxq_ctrl == NULL)
			goto fail_rxqs_ctrl_alloc;
	} else {
		struct sfc_rxq_info *new_rxq_info;
		struct sfc_rxq *new_rxq_ctrl;

		reconfigure = true;

		/* Do not uninitialize reserved queues */
		if (nb_rx_queues < sas->ethdev_rxq_count)
			sfc_rx_fini_queues(sa, nb_rx_queues);

		rc = ENOMEM;
		new_rxq_info =
			rte_realloc(sas->rxq_info,
				    nb_rxq_total * sizeof(sas->rxq_info[0]), 0);
		if (new_rxq_info == NULL && nb_rxq_total > 0)
			goto fail_rxqs_realloc;

		rc = ENOMEM;
		new_rxq_ctrl = realloc(sa->rxq_ctrl,
				       nb_rxq_total * sizeof(sa->rxq_ctrl[0]));
		if (new_rxq_ctrl == NULL && nb_rxq_total > 0)
			goto fail_rxqs_ctrl_realloc;

		sas->rxq_info = new_rxq_info;
		sa->rxq_ctrl = new_rxq_ctrl;
		if (nb_rxq_total > sas->rxq_count) {
			unsigned int rxq_count = sas->rxq_count;

			memset(&sas->rxq_info[rxq_count], 0,
			       (nb_rxq_total - rxq_count) *
			       sizeof(sas->rxq_info[0]));
			memset(&sa->rxq_ctrl[rxq_count], 0,
			       (nb_rxq_total - rxq_count) *
			       sizeof(sa->rxq_ctrl[0]));
		}
	}

	while (sas->ethdev_rxq_count < nb_rx_queues) {
		sfc_sw_index_t sw_index;

		sw_index = sfc_rxq_sw_index_by_ethdev_rx_qid(sas,
							sas->ethdev_rxq_count);
		rc = sfc_rx_qinit_info(sa, sw_index, 0);
		if (rc != 0)
			goto fail_rx_qinit_info;

		sas->ethdev_rxq_count++;
	}

	sas->rxq_count = sas->ethdev_rxq_count + nb_rsrv_rx_queues;

	if (!reconfigure) {
		rc = sfc_mae_counter_rxq_init(sa);
		if (rc != 0)
			goto fail_count_rxq_init;
	}

configure_rss:
	rss->channels = (dev_conf->rxmode.mq_mode == RTE_ETH_MQ_RX_RSS) ?
			 MIN(sas->ethdev_rxq_count, EFX_MAXRSS) : 0;

	if (rss->channels > 0) {
		struct rte_eth_rss_conf *adv_conf_rss;
		sfc_sw_index_t sw_index;

		for (sw_index = 0; sw_index < EFX_RSS_TBL_SIZE; ++sw_index)
			rss->tbl[sw_index] = sw_index % rss->channels;

		adv_conf_rss = &dev_conf->rx_adv_conf.rss_conf;
		rc = sfc_rx_process_adv_conf_rss(sa, adv_conf_rss);
		if (rc != 0)
			goto fail_rx_process_adv_conf_rss;
	}

	return 0;

fail_rx_process_adv_conf_rss:
	if (!reconfigure)
		sfc_mae_counter_rxq_fini(sa);

fail_count_rxq_init:
fail_rx_qinit_info:
fail_rxqs_ctrl_realloc:
fail_rxqs_realloc:
fail_rxqs_ctrl_alloc:
fail_rxqs_alloc:
	sfc_rx_close(sa);

fail_check_mode:
	sfc_log_init(sa, "failed %d", rc);
	return rc;
}

/**
 * Shutdown Rx subsystem.
 *
 * Called at device close stage, for example, before device shutdown.
 */
void
sfc_rx_close(struct sfc_adapter *sa)
{
	struct sfc_rss *rss = &sfc_sa2shared(sa)->rss;

	sfc_rx_fini_queues(sa, 0);
	sfc_mae_counter_rxq_fini(sa);

	rss->channels = 0;

	free(sa->rxq_ctrl);
	sa->rxq_ctrl = NULL;

	rte_free(sfc_sa2shared(sa)->rxq_info);
	sfc_sa2shared(sa)->rxq_info = NULL;
}
