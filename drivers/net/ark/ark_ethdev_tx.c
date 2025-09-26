/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2021 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_ethdev_tx.h"
#include "ark_global.h"
#include "ark_mpu.h"
#include "ark_ddm.h"
#include "ark_logs.h"

#define ARK_TX_META_SIZE   32
#define ARK_TX_META_OFFSET (RTE_PKTMBUF_HEADROOM - ARK_TX_META_SIZE)
#define ARK_TX_MAX_NOCHAIN (RTE_MBUF_DEFAULT_DATAROOM)

#ifndef RTE_LIBRTE_ARK_MIN_TX_PKTLEN
#define ARK_MIN_TX_PKTLEN 0
#else
#define ARK_MIN_TX_PKTLEN RTE_LIBRTE_ARK_MIN_TX_PKTLEN
#endif

/* ************************************************************************* */
struct __rte_cache_aligned ark_tx_queue {
	union ark_tx_meta *meta_q;
	struct rte_mbuf **bufs;

	/* handles for hw objects */
	struct ark_mpu_t *mpu;
	struct ark_ddm_t *ddm;

	/* Stats HW tracks bytes and packets, need to count send errors */
	uint64_t tx_errors;

	tx_user_meta_hook_fn tx_user_meta_hook;
	void *ext_user_data;

	uint32_t queue_size;
	uint32_t queue_mask;

	/* 3 indexes to the paired data rings. */
	uint32_t prod_index;		/* where to put the next one */
	uint32_t free_index;		/* mbuf has been freed */

	/* The queue Id is used to identify the HW Q */
	uint16_t phys_qid;
	/* The queue Index within the dpdk device structures */
	uint16_t queue_index;

	/* next cache line - fields written by device */
	alignas(RTE_CACHE_LINE_MIN_SIZE) RTE_MARKER cacheline1;
	volatile uint32_t cons_index;		/* hw is done, can be freed */
};

/* Forward declarations */
static int ark_tx_jumbo(struct ark_tx_queue *queue,
			    struct rte_mbuf *mbuf,
			    uint32_t *user_meta, uint8_t meta_cnt);
static int ark_tx_hw_queue_config(struct ark_tx_queue *queue);
static void free_completed_tx(struct ark_tx_queue *queue);

/* ************************************************************************* */
static inline void
ark_tx_desc_fill(struct ark_tx_queue *queue,
		     struct rte_mbuf *mbuf,
		     uint8_t  flags,
		     uint32_t *user_meta,
		     uint8_t  meta_cnt /* 0 to 5 */
		     )
{
	uint32_t tx_idx;
	union ark_tx_meta *meta;
	uint8_t m;

	/* Header */
	tx_idx = queue->prod_index & queue->queue_mask;
	meta = &queue->meta_q[tx_idx];
	meta->data_len = rte_pktmbuf_data_len(mbuf);
	meta->flags = flags;
	meta->meta_cnt = meta_cnt / 2;
	meta->user1 = meta_cnt ? (*user_meta++) : 0;
	queue->prod_index++;

	queue->bufs[tx_idx] = mbuf;

	/* 1 or 2 user meta data entries, user words 1,2 and 3,4 */
	for (m = 1; m < meta_cnt; m += 2) {
		tx_idx = queue->prod_index & queue->queue_mask;
		meta = &queue->meta_q[tx_idx];
		meta->usermeta0 = *user_meta++;
		meta->usermeta1 = *user_meta++;
		queue->prod_index++;
	}

	tx_idx = queue->prod_index & queue->queue_mask;
	meta = &queue->meta_q[tx_idx];
	meta->physaddr = rte_mbuf_data_iova(mbuf);
	queue->prod_index++;
}


/* ************************************************************************* */
uint16_t
ark_xmit_pkts(void *vtxq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct ark_tx_queue *queue;
	struct rte_mbuf *mbuf;
	uint32_t user_meta[5];

	int stat;
	uint32_t prod_index_limit;
	uint16_t nb;
	uint8_t user_len = 0;
	const uint32_t min_pkt_len = ARK_MIN_TX_PKTLEN;
	tx_user_meta_hook_fn tx_user_meta_hook;

	queue = (struct ark_tx_queue *)vtxq;
	tx_user_meta_hook = queue->tx_user_meta_hook;

	/* free any packets after the HW is done with them */
	free_completed_tx(queue);

	/* leave 4 elements mpu data */
	prod_index_limit = queue->queue_size + queue->free_index - 4;

	/* Populate the buffer bringing prod_index up to or slightly beyond
	 * prod_index_limit. Prod_index will increment by 2 or more each
	 * iteration.  Note: indexes are uint32_t, cast to (signed) int32_t
	 * to catch the slight overage case;  e.g. (200 - 201)
	 */
	for (nb = 0;
	     (nb < nb_pkts) && (int32_t)(prod_index_limit - queue->prod_index) > 0;
	     ++nb) {
		mbuf = tx_pkts[nb];

		if (min_pkt_len &&
		    unlikely(rte_pktmbuf_pkt_len(mbuf) < min_pkt_len)) {
			/* this packet even if it is small can be split,
			 * be sure to add to the end mbuf
			 */
			uint16_t to_add = min_pkt_len -
				rte_pktmbuf_pkt_len(mbuf);
			char *appended =
				rte_pktmbuf_append(mbuf, to_add);

			if (appended == 0) {
				/* This packet is in error,
				 * we cannot send it so just
				 * count it and delete it.
				 */
				queue->tx_errors += 1;
				rte_pktmbuf_free(mbuf);
				continue;
			}
			memset(appended, 0, to_add);
		}

		if (tx_user_meta_hook)
			tx_user_meta_hook(mbuf, user_meta, &user_len,
					  queue->ext_user_data);
		if (unlikely(mbuf->nb_segs != 1)) {
			stat = ark_tx_jumbo(queue, mbuf,
						user_meta, user_len);
			if (unlikely(stat != 0))
				break;		/* Queue is full */
		} else {
			ark_tx_desc_fill(queue, mbuf,
					     ARK_DDM_SOP | ARK_DDM_EOP,
					     user_meta, user_len);
		}
	}

	if (ARK_DEBUG_CORE && nb != nb_pkts) {
		ARK_PMD_LOG(DEBUG, "TX: Failure to send:"
			   " req: %" PRIU32
			   " sent: %" PRIU32
			   " prod: %" PRIU32
			   " cons: %" PRIU32
			   " free: %" PRIU32 "\n",
			   nb_pkts, nb,
			   queue->prod_index,
			   queue->cons_index,
			   queue->free_index);
		ark_mpu_dump(queue->mpu,
			     "TX Failure MPU: ",
			     queue->phys_qid);
	}

	/* let FPGA know producer index.  */
	if (likely(nb != 0))
		ark_mpu_set_producer(queue->mpu, queue->prod_index);

	return nb;
}

/* ************************************************************************* */
static int
ark_tx_jumbo(struct ark_tx_queue *queue, struct rte_mbuf *mbuf,
		 uint32_t *user_meta, uint8_t meta_cnt)
{
	struct rte_mbuf *next;
	uint32_t free_queue_space;
	uint8_t flags = ARK_DDM_SOP;

	free_queue_space = queue->queue_mask -
		(queue->prod_index - queue->free_index);
	/* We need up to 4 mbufs for first header and 2 for subsequent ones */
	if (unlikely(free_queue_space < (2U + (2U * mbuf->nb_segs))))
		return -1;

	while (mbuf != NULL) {
		next = mbuf->next;
		flags |= (next == NULL) ? ARK_DDM_EOP : 0;

		ark_tx_desc_fill(queue, mbuf, flags, user_meta, meta_cnt);

		flags &= ~ARK_DDM_SOP;	/* drop SOP flags */
		meta_cnt = 0;		/* Meta only on SOP */
		mbuf = next;
	}

	return 0;
}

/* ************************************************************************* */
int
ark_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct ark_adapter *ark = dev->data->dev_private;
	struct ark_tx_queue *queue;
	int status;

	int qidx = ark->qbase + queue_idx;

	if (!rte_is_power_of_2(nb_desc)) {
		ARK_PMD_LOG(ERR,
			    "DPDK Arkville configuration queue size"
			    " must be power of two %u (%s)\n",
			    nb_desc, __func__);
		return -1;
	}

	/* Each packet requires at least 2 mpu elements - double desc count */
	nb_desc = 2 * nb_desc;

	/* Allocate queue struct */
	queue =	rte_zmalloc_socket("Ark_txqueue",
				   sizeof(struct ark_tx_queue),
				   64,
				   socket_id);
	if (queue == 0) {
		ARK_PMD_LOG(ERR, "Failed to allocate tx "
			    "queue memory in %s\n",
			    __func__);
		return -ENOMEM;
	}

	/* we use zmalloc no need to initialize fields */
	queue->queue_size = nb_desc;
	queue->queue_mask = nb_desc - 1;
	queue->phys_qid = qidx;
	queue->queue_index = queue_idx;
	dev->data->tx_queues[queue_idx] = queue;
	queue->tx_user_meta_hook = ark->user_ext.tx_user_meta_hook;
	queue->ext_user_data = ark->user_data[dev->data->port_id];

	queue->meta_q =
		rte_zmalloc_socket("Ark_txqueue meta",
				   nb_desc * sizeof(union ark_tx_meta),
				   64,
				   socket_id);
	queue->bufs =
		rte_zmalloc_socket("Ark_txqueue bufs",
				   nb_desc * sizeof(struct rte_mbuf *),
				   64,
				   socket_id);

	if (queue->meta_q == 0 || queue->bufs == 0) {
		ARK_PMD_LOG(ERR, "Failed to allocate "
			    "queue memory in %s\n", __func__);
		rte_free(queue->meta_q);
		rte_free(queue->bufs);
		rte_free(queue);
		return -ENOMEM;
	}

	queue->ddm = RTE_PTR_ADD(ark->ddm.v, qidx * ARK_DDM_QOFFSET);
	queue->mpu = RTE_PTR_ADD(ark->mputx.v, qidx * ARK_MPU_QOFFSET);

	status = ark_tx_hw_queue_config(queue);

	if (unlikely(status != 0)) {
		rte_free(queue->meta_q);
		rte_free(queue->bufs);
		rte_free(queue);
		return -1;		/* ERROR CODE */
	}

	return 0;
}

/* ************************************************************************* */
static int
ark_tx_hw_queue_config(struct ark_tx_queue *queue)
{
	rte_iova_t queue_base, ring_base, cons_index_addr;

	/* Verify HW -- MPU */
	if (ark_mpu_verify(queue->mpu, sizeof(union ark_tx_meta)))
		return -1;

	queue_base = rte_malloc_virt2iova(queue);
	ring_base = rte_malloc_virt2iova(queue->meta_q);
	cons_index_addr =
		queue_base + offsetof(struct ark_tx_queue, cons_index);

	ark_mpu_stop(queue->mpu);
	ark_mpu_reset(queue->mpu);

	/* Stop and Reset and configure MPU */
	ark_mpu_configure(queue->mpu, ring_base, queue->queue_size, 1);

	/* Completion address in UDM */
	ark_ddm_queue_setup(queue->ddm, cons_index_addr);
	ark_ddm_queue_reset_stats(queue->ddm);

	return 0;
}

/* ************************************************************************* */
void
ark_tx_queue_release(void *vtx_queue)
{
	struct ark_tx_queue *queue;

	queue = (struct ark_tx_queue *)vtx_queue;

	ark_ddm_queue_enable(queue->ddm, 0);
	ark_mpu_stop(queue->mpu);

	queue->cons_index = queue->prod_index;
	free_completed_tx(queue);

	rte_free(queue->meta_q);
	rte_free(queue->bufs);
	rte_free(queue);
}

/* ************************************************************************* */
int
ark_tx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct ark_tx_queue *queue;
	int cnt = 0;

	queue = dev->data->tx_queues[queue_id];

	/* Wait for DDM to send out all packets. */
	while (queue->cons_index != queue->prod_index) {
		usleep(100);
		if (cnt++ > 10000)
			return -1;
	}

	ark_ddm_queue_enable(queue->ddm, 0);
	ark_mpu_stop(queue->mpu);
	free_completed_tx(queue);

	dev->data->tx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
ark_tx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct ark_tx_queue *queue;

	queue = dev->data->tx_queues[queue_id];
	if (dev->data->tx_queue_state[queue_id] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	ark_mpu_start(queue->mpu);
	ark_ddm_queue_enable(queue->ddm, 1);
	dev->data->tx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/* ************************************************************************* */
static void
free_completed_tx(struct ark_tx_queue *queue)
{
	struct rte_mbuf *mbuf;
	union ark_tx_meta *meta;
	uint32_t top_index;

	top_index = queue->cons_index;	/* read once */

	while ((int32_t)(top_index - queue->free_index) > 0) {
		meta = &queue->meta_q[queue->free_index & queue->queue_mask];
		if (likely((meta->flags & ARK_DDM_SOP) != 0)) {
			mbuf = queue->bufs[queue->free_index &
					   queue->queue_mask];
			/* ref count of the mbuf is checked in this call. */
			rte_pktmbuf_free(mbuf);
		}
		queue->free_index += (meta->meta_cnt + 2);
	}
}

/* ************************************************************************* */
void
ark_tx_queue_stats_get(void *vqueue, struct rte_eth_stats *stats)
{
	struct ark_tx_queue *queue;
	struct ark_ddm_t *ddm;
	uint64_t bytes, pkts;

	queue = vqueue;
	ddm = queue->ddm;

	bytes = ark_ddm_queue_byte_count(ddm);
	pkts = ark_ddm_queue_pkt_count(ddm);

	stats->q_opackets[queue->queue_index] = pkts;
	stats->q_obytes[queue->queue_index] = bytes;
	stats->opackets += pkts;
	stats->obytes += bytes;
	stats->oerrors += queue->tx_errors;
}

void
ark_tx_queue_stats_reset(void *vqueue)
{
	struct ark_tx_queue *queue;
	struct ark_ddm_t *ddm;

	queue = vqueue;
	ddm = queue->ddm;

	ark_ddm_queue_reset_stats(ddm);
	queue->tx_errors = 0;
}
