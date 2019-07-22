/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_ethdev_rx.h"
#include "ark_global.h"
#include "ark_logs.h"
#include "ark_mpu.h"
#include "ark_udm.h"

#define ARK_RX_META_SIZE 32
#define ARK_RX_META_OFFSET (RTE_PKTMBUF_HEADROOM - ARK_RX_META_SIZE)
#define ARK_RX_MAX_NOCHAIN (RTE_MBUF_DEFAULT_DATAROOM)

/* Forward declarations */
struct ark_rx_queue;
struct ark_rx_meta;

static void dump_mbuf_data(struct rte_mbuf *mbuf, uint16_t lo, uint16_t hi);
static void ark_ethdev_rx_dump(const char *name, struct ark_rx_queue *queue);
static uint32_t eth_ark_rx_jumbo(struct ark_rx_queue *queue,
				 struct ark_rx_meta *meta,
				 struct rte_mbuf *mbuf0,
				 uint32_t cons_index);
static inline int eth_ark_rx_seed_mbufs(struct ark_rx_queue *queue);
static int eth_ark_rx_seed_recovery(struct ark_rx_queue *queue,
				    uint32_t *pnb,
				    struct rte_mbuf **mbufs);

/* ************************************************************************* */
struct ark_rx_queue {
	/* array of mbufs to populate */
	struct rte_mbuf **reserve_q;
	/* array of physical addresses of the mbuf data pointer */
	/* This point is a virtual address */
	rte_iova_t *paddress_q;
	struct rte_mempool *mb_pool;

	struct ark_udm_t *udm;
	struct ark_mpu_t *mpu;

	uint32_t queue_size;
	uint32_t queue_mask;

	uint32_t seed_index;		/* step 1 set with empty mbuf */
	uint32_t cons_index;		/* step 3 consumed by driver */

	/* The queue Id is used to identify the HW Q */
	uint16_t phys_qid;

	/* The queue Index is used within the dpdk device structures */
	uint16_t queue_index;

	uint32_t last_cons;

	/* separate cache line */
	/* second cache line - fields only used in slow path */
	MARKER cacheline1 __rte_cache_min_aligned;

	volatile uint32_t prod_index;	/* step 2 filled by FPGA */
} __rte_cache_aligned;


/* ************************************************************************* */
static int
eth_ark_rx_hw_setup(struct rte_eth_dev *dev,
		    struct ark_rx_queue *queue,
		    uint16_t rx_queue_id __rte_unused, uint16_t rx_queue_idx)
{
	rte_iova_t queue_base;
	rte_iova_t phys_addr_q_base;
	rte_iova_t phys_addr_prod_index;

	queue_base = rte_malloc_virt2iova(queue);
	phys_addr_prod_index = queue_base +
		offsetof(struct ark_rx_queue, prod_index);

	phys_addr_q_base = rte_malloc_virt2iova(queue->paddress_q);

	/* Verify HW */
	if (ark_mpu_verify(queue->mpu, sizeof(rte_iova_t))) {
		PMD_DRV_LOG(ERR, "Illegal configuration rx queue\n");
		return -1;
	}

	/* Stop and Reset and configure MPU */
	ark_mpu_configure(queue->mpu, phys_addr_q_base, queue->queue_size, 0);

	ark_udm_write_addr(queue->udm, phys_addr_prod_index);

	/* advance the valid pointer, but don't start until the queue starts */
	ark_mpu_reset_stats(queue->mpu);

	/* The seed is the producer index for the HW */
	ark_mpu_set_producer(queue->mpu, queue->seed_index);
	dev->data->rx_queue_state[rx_queue_idx] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static inline void
eth_ark_rx_update_cons_index(struct ark_rx_queue *queue, uint32_t cons_index)
{
	queue->cons_index = cons_index;
	eth_ark_rx_seed_mbufs(queue);
	if (((cons_index - queue->last_cons) >= 64U)) {
		queue->last_cons = cons_index;
		ark_mpu_set_producer(queue->mpu, queue->seed_index);
	}
}

/* ************************************************************************* */
int
eth_ark_dev_rx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   const struct rte_eth_rxconf *rx_conf,
			   struct rte_mempool *mb_pool)
{
	static int warning1;		/* = 0 */
	struct ark_adapter *ark = dev->data->dev_private;

	struct ark_rx_queue *queue;
	uint32_t i;
	int status;

	int qidx = queue_idx;

	/* We may already be setup, free memory prior to re-allocation */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		eth_ark_dev_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	if (rx_conf != NULL && warning1 == 0) {
		warning1 = 1;
		PMD_DRV_LOG(INFO,
			    "Arkville ignores rte_eth_rxconf argument.\n");
	}

	if (RTE_PKTMBUF_HEADROOM < ARK_RX_META_SIZE) {
		PMD_DRV_LOG(ERR,
			    "Error: DPDK Arkville requires head room > %d bytes (%s)\n",
			    ARK_RX_META_SIZE, __func__);
		return -1;		/* ERROR CODE */
	}

	if (!rte_is_power_of_2(nb_desc)) {
		PMD_DRV_LOG(ERR,
			    "DPDK Arkville configuration queue size must be power of two %u (%s)\n",
			    nb_desc, __func__);
		return -1;		/* ERROR CODE */
	}

	/* Allocate queue struct */
	queue = rte_zmalloc_socket("Ark_rxqueue",
				   sizeof(struct ark_rx_queue),
				   64,
				   socket_id);
	if (queue == 0) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory in %s\n", __func__);
		return -ENOMEM;
	}

	/* NOTE zmalloc is used, no need to 0 indexes, etc. */
	queue->mb_pool = mb_pool;
	queue->phys_qid = qidx;
	queue->queue_index = queue_idx;
	queue->queue_size = nb_desc;
	queue->queue_mask = nb_desc - 1;

	queue->reserve_q =
		rte_zmalloc_socket("Ark_rx_queue mbuf",
				   nb_desc * sizeof(struct rte_mbuf *),
				   64,
				   socket_id);
	queue->paddress_q =
		rte_zmalloc_socket("Ark_rx_queue paddr",
				   nb_desc * sizeof(rte_iova_t),
				   64,
				   socket_id);

	if (queue->reserve_q == 0 || queue->paddress_q == 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to allocate queue memory in %s\n",
			    __func__);
		rte_free(queue->reserve_q);
		rte_free(queue->paddress_q);
		rte_free(queue);
		return -ENOMEM;
	}

	dev->data->rx_queues[queue_idx] = queue;
	queue->udm = RTE_PTR_ADD(ark->udm.v, qidx * ARK_UDM_QOFFSET);
	queue->mpu = RTE_PTR_ADD(ark->mpurx.v, qidx * ARK_MPU_QOFFSET);

	/* populate mbuf reserve */
	status = eth_ark_rx_seed_mbufs(queue);

	if (queue->seed_index != nb_desc) {
		PMD_DRV_LOG(ERR, "ARK: Failed to allocate %u mbufs for RX queue %d\n",
			    nb_desc, qidx);
		status = -1;
	}
	/* MPU Setup */
	if (status == 0)
		status = eth_ark_rx_hw_setup(dev, queue, qidx, queue_idx);

	if (unlikely(status != 0)) {
		struct rte_mbuf **mbuf;

		PMD_DRV_LOG(ERR, "Failed to initialize RX queue %d %s\n",
			    qidx,
			    __func__);
		/* Free the mbufs allocated */
		for (i = 0, mbuf = queue->reserve_q;
		     i < queue->seed_index; ++i, mbuf++) {
			rte_pktmbuf_free(*mbuf);
		}
		rte_free(queue->reserve_q);
		rte_free(queue->paddress_q);
		rte_free(queue);
		return -1;		/* ERROR CODE */
	}

	return 0;
}

/* ************************************************************************* */
uint16_t
eth_ark_recv_pkts_noop(void *rx_queue __rte_unused,
		       struct rte_mbuf **rx_pkts __rte_unused,
		       uint16_t nb_pkts __rte_unused)
{
	return 0;
}

/* ************************************************************************* */
uint16_t
eth_ark_recv_pkts(void *rx_queue,
		  struct rte_mbuf **rx_pkts,
		  uint16_t nb_pkts)
{
	struct ark_rx_queue *queue;
	register uint32_t cons_index, prod_index;
	uint16_t nb;
	struct rte_mbuf *mbuf;
	struct ark_rx_meta *meta;

	queue = (struct ark_rx_queue *)rx_queue;
	if (unlikely(queue == 0))
		return 0;
	if (unlikely(nb_pkts == 0))
		return 0;
	prod_index = queue->prod_index;
	cons_index = queue->cons_index;
	nb = 0;

	while (prod_index != cons_index) {
		mbuf = queue->reserve_q[cons_index & queue->queue_mask];
		/* prefetch mbuf */
		rte_mbuf_prefetch_part1(mbuf);
		rte_mbuf_prefetch_part2(mbuf);

		/* META DATA embedded in headroom */
		meta = RTE_PTR_ADD(mbuf->buf_addr, ARK_RX_META_OFFSET);

		mbuf->port = meta->port;
		mbuf->pkt_len = meta->pkt_len;
		mbuf->data_len = meta->pkt_len;
		mbuf->timestamp = meta->timestamp;
		mbuf->udata64 = meta->user_data;

		if (ARK_RX_DEBUG) {	/* debug sanity checks */
			if ((meta->pkt_len > (1024 * 16)) ||
			    (meta->pkt_len == 0)) {
				PMD_RX_LOG(DEBUG, "RX: Bad Meta Q: %u"
					   " cons: %" PRIU32
					   " prod: %" PRIU32
					   " seed_index %" PRIU32
					   "\n",
					   queue->phys_qid,
					   cons_index,
					   queue->prod_index,
					   queue->seed_index);


				PMD_RX_LOG(DEBUG, "       :  UDM"
					   " prod: %" PRIU32
					   " len: %u\n",
					   queue->udm->rt_cfg.prod_idx,
					   meta->pkt_len);
				ark_mpu_dump(queue->mpu,
					     "    ",
					     queue->phys_qid);
				dump_mbuf_data(mbuf, 0, 256);
				/* its FUBAR so fix it */
				mbuf->pkt_len = 63;
				meta->pkt_len = 63;
			}
			/* seqn is only set under debug */
			mbuf->seqn = cons_index;
		}

		if (unlikely(meta->pkt_len > ARK_RX_MAX_NOCHAIN))
			cons_index = eth_ark_rx_jumbo
				(queue, meta, mbuf, cons_index + 1);
		else
			cons_index += 1;

		rx_pkts[nb] = mbuf;
		nb++;
		if (nb >= nb_pkts)
			break;
	}

	if (unlikely(nb != 0))
		/* report next free to FPGA */
		eth_ark_rx_update_cons_index(queue, cons_index);

	return nb;
}

/* ************************************************************************* */
static uint32_t
eth_ark_rx_jumbo(struct ark_rx_queue *queue,
		 struct ark_rx_meta *meta,
		 struct rte_mbuf *mbuf0,
		 uint32_t cons_index)
{
	struct rte_mbuf *mbuf_prev;
	struct rte_mbuf *mbuf;

	uint16_t remaining;
	uint16_t data_len;
	uint16_t segments;

	/* first buf populated by called */
	mbuf_prev = mbuf0;
	segments = 1;
	data_len = RTE_MIN(meta->pkt_len, RTE_MBUF_DEFAULT_DATAROOM);
	remaining = meta->pkt_len - data_len;
	mbuf0->data_len = data_len;

	/* HW guarantees that the data does not exceed prod_index! */
	while (remaining != 0) {
		data_len = RTE_MIN(remaining,
				   RTE_MBUF_DEFAULT_DATAROOM +
				   RTE_PKTMBUF_HEADROOM);

		remaining -= data_len;
		segments += 1;

		mbuf = queue->reserve_q[cons_index & queue->queue_mask];
		mbuf_prev->next = mbuf;
		mbuf_prev = mbuf;
		mbuf->data_len = data_len;
		mbuf->data_off = 0;
		if (ARK_RX_DEBUG)
			mbuf->seqn = cons_index;	/* for debug only */

		cons_index += 1;
	}

	mbuf0->nb_segs = segments;
	return cons_index;
}

/* Drain the internal queue allowing hw to clear out. */
static void
eth_ark_rx_queue_drain(struct ark_rx_queue *queue)
{
	register uint32_t cons_index;
	struct rte_mbuf *mbuf;

	cons_index = queue->cons_index;

	/* NOT performance optimized, since this is a one-shot call */
	while ((cons_index ^ queue->prod_index) & queue->queue_mask) {
		mbuf = queue->reserve_q[cons_index & queue->queue_mask];
		rte_pktmbuf_free(mbuf);
		cons_index++;
		eth_ark_rx_update_cons_index(queue, cons_index);
	}
}

uint32_t
eth_ark_dev_rx_queue_count(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct ark_rx_queue *queue;

	queue = dev->data->rx_queues[queue_id];
	return (queue->prod_index - queue->cons_index);	/* mod arith */
}

/* ************************************************************************* */
int
eth_ark_rx_start_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct ark_rx_queue *queue;

	queue = dev->data->rx_queues[queue_id];
	if (queue == 0)
		return -1;

	dev->data->rx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	ark_mpu_set_producer(queue->mpu, queue->seed_index);
	ark_mpu_start(queue->mpu);

	ark_udm_queue_enable(queue->udm, 1);

	return 0;
}

/* ************************************************************************* */

/* Queue can be restarted.   data remains
 */
int
eth_ark_rx_stop_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct ark_rx_queue *queue;

	queue = dev->data->rx_queues[queue_id];
	if (queue == 0)
		return -1;

	ark_udm_queue_enable(queue->udm, 0);

	dev->data->rx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

/* ************************************************************************* */
static inline int
eth_ark_rx_seed_mbufs(struct ark_rx_queue *queue)
{
	uint32_t limit = queue->cons_index + queue->queue_size;
	uint32_t seed_index = queue->seed_index;

	uint32_t count = 0;
	uint32_t seed_m = queue->seed_index & queue->queue_mask;

	uint32_t nb = limit - seed_index;

	/* Handle wrap around -- remainder is filled on the next call */
	if (unlikely(seed_m + nb > queue->queue_size))
		nb = queue->queue_size - seed_m;

	struct rte_mbuf **mbufs = &queue->reserve_q[seed_m];
	int status = rte_pktmbuf_alloc_bulk(queue->mb_pool, mbufs, nb);

	if (unlikely(status != 0)) {
		/* Try to recover from lack of mbufs in pool */
		status = eth_ark_rx_seed_recovery(queue, &nb, mbufs);
		if (unlikely(status != 0)) {
			return -1;
		}
	}

	if (ARK_RX_DEBUG) {		/* DEBUG */
		while (count != nb) {
			struct rte_mbuf *mbuf_init =
				queue->reserve_q[seed_m + count];

			memset(mbuf_init->buf_addr, -1, 512);
			*((uint32_t *)mbuf_init->buf_addr) =
				seed_index + count;
			*(uint16_t *)RTE_PTR_ADD(mbuf_init->buf_addr, 4) =
				queue->phys_qid;
			count++;
		}
		count = 0;
	} /* DEBUG */
	queue->seed_index += nb;

	/* Duff's device https://en.wikipedia.org/wiki/Duff's_device */
	switch (nb % 4) {
	case 0:
		while (count != nb) {
			queue->paddress_q[seed_m++] =
				(*mbufs++)->buf_iova;
			count++;
		/* FALLTHROUGH */
	case 3:
		queue->paddress_q[seed_m++] =
			(*mbufs++)->buf_iova;
		count++;
		/* FALLTHROUGH */
	case 2:
		queue->paddress_q[seed_m++] =
			(*mbufs++)->buf_iova;
		count++;
		/* FALLTHROUGH */
	case 1:
		queue->paddress_q[seed_m++] =
			(*mbufs++)->buf_iova;
		count++;
		/* FALLTHROUGH */

		} /* while (count != nb) */
	} /* switch */

	return 0;
}

int
eth_ark_rx_seed_recovery(struct ark_rx_queue *queue,
			 uint32_t *pnb,
			 struct rte_mbuf **mbufs)
{
	int status = -1;

	/* Ignore small allocation failures */
	if (*pnb <= 64)
		return -1;

	*pnb = 64U;
	status = rte_pktmbuf_alloc_bulk(queue->mb_pool, mbufs, *pnb);
	if (status != 0) {
		PMD_DRV_LOG(ERR,
			    "ARK: Could not allocate %u mbufs from pool for RX queue %u;"
			    " %u free buffers remaining in queue\n",
			    *pnb, queue->queue_index,
			    queue->seed_index - queue->cons_index);
	}
	return status;
}

void
eth_ark_rx_dump_queue(struct rte_eth_dev *dev, uint16_t queue_id,
		      const char *msg)
{
	struct ark_rx_queue *queue;

	queue = dev->data->rx_queues[queue_id];

	ark_ethdev_rx_dump(msg, queue);
}

/* ************************************************************************* */
/* Call on device closed no user API, queue is stopped */
void
eth_ark_dev_rx_queue_release(void *vqueue)
{
	struct ark_rx_queue *queue;
	uint32_t i;

	queue = (struct ark_rx_queue *)vqueue;
	if (queue == 0)
		return;

	ark_udm_queue_enable(queue->udm, 0);
	/* Stop the MPU since pointer are going away */
	ark_mpu_stop(queue->mpu);

	/* Need to clear out mbufs here, dropping packets along the way */
	eth_ark_rx_queue_drain(queue);

	for (i = 0; i < queue->queue_size; ++i)
		rte_pktmbuf_free(queue->reserve_q[i]);

	rte_free(queue->reserve_q);
	rte_free(queue->paddress_q);
	rte_free(queue);
}

void
eth_rx_queue_stats_get(void *vqueue, struct rte_eth_stats *stats)
{
	struct ark_rx_queue *queue;
	struct ark_udm_t *udm;

	queue = vqueue;
	if (queue == 0)
		return;
	udm = queue->udm;

	uint64_t ibytes = ark_udm_bytes(udm);
	uint64_t ipackets = ark_udm_packets(udm);
	uint64_t idropped = ark_udm_dropped(queue->udm);

	stats->q_ipackets[queue->queue_index] = ipackets;
	stats->q_ibytes[queue->queue_index] = ibytes;
	stats->q_errors[queue->queue_index] = idropped;
	stats->ipackets += ipackets;
	stats->ibytes += ibytes;
	stats->imissed += idropped;
}

void
eth_rx_queue_stats_reset(void *vqueue)
{
	struct ark_rx_queue *queue;

	queue = vqueue;
	if (queue == 0)
		return;

	ark_mpu_reset_stats(queue->mpu);
	ark_udm_queue_stats_reset(queue->udm);
}

void
eth_ark_udm_force_close(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;
	struct ark_rx_queue *queue;
	uint32_t index;
	uint16_t i;

	if (!ark_udm_is_flushed(ark->udm.v)) {
		/* restart the MPUs */
		PMD_DRV_LOG(ERR, "ARK: %s UDM not flushed\n", __func__);
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			queue = (struct ark_rx_queue *)dev->data->rx_queues[i];
			if (queue == 0)
				continue;

			ark_mpu_start(queue->mpu);
			/* Add some buffers */
			index = 100000 + queue->seed_index;
			ark_mpu_set_producer(queue->mpu, index);
		}
		/* Wait to allow data to pass */
		usleep(100);

		PMD_DEBUG_LOG(DEBUG, "UDM forced flush attempt, stopped = %d\n",
				ark_udm_is_flushed(ark->udm.v));
	}
	ark_udm_reset(ark->udm.v);
}

static void
ark_ethdev_rx_dump(const char *name, struct ark_rx_queue *queue)
{
	if (queue == NULL)
		return;
	PMD_DEBUG_LOG(DEBUG, "RX QUEUE %d -- %s", queue->phys_qid, name);
	PMD_DEBUG_LOG(DEBUG, ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 "\n",
			"queue_size", queue->queue_size,
			"seed_index", queue->seed_index,
			"prod_index", queue->prod_index,
			"cons_index", queue->cons_index);

	ark_mpu_dump(queue->mpu, name, queue->phys_qid);
	ark_mpu_dump_setup(queue->mpu, queue->phys_qid);
	ark_udm_dump(queue->udm, name);
	ark_udm_dump_setup(queue->udm, queue->phys_qid);
}

/* Only used in debug.
 * This function is a raw memory dump of a portion of an mbuf's memory
 * region.  The usual function, rte_pktmbuf_dump() only shows data
 * with respect to the data_off field.  This function show data
 * anywhere in the mbuf's buffer.  This is useful for examining
 * data in the headroom or tailroom portion of an mbuf.
 */
static void
dump_mbuf_data(struct rte_mbuf *mbuf, uint16_t lo, uint16_t hi)
{
	uint16_t i, j;

	PMD_DRV_LOG(INFO, " MBUF: %p len %d, off: %d, seq: %" PRIU32 "\n", mbuf,
		mbuf->pkt_len, mbuf->data_off, mbuf->seqn);
	for (i = lo; i < hi; i += 16) {
		uint8_t *dp = RTE_PTR_ADD(mbuf->buf_addr, i);

		PMD_DRV_LOG(INFO, "  %6d:  ", i);
		for (j = 0; j < 16; j++)
			PMD_DRV_LOG(INFO, " %02x", dp[j]);

		PMD_DRV_LOG(INFO, "\n");
	}
}
