/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#include <inttypes.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_bitops.h>

#include "ionic_crypto.h"

static int
iocpt_cq_init(struct iocpt_cq *cq, uint16_t num_descs)
{
	if (!rte_is_power_of_2(num_descs) ||
	    num_descs < IOCPT_MIN_RING_DESC ||
	    num_descs > IOCPT_MAX_RING_DESC) {
		IOCPT_PRINT(ERR, "%u descriptors (min: %u max: %u)",
			num_descs, IOCPT_MIN_RING_DESC, IOCPT_MAX_RING_DESC);
		return -EINVAL;
	}

	cq->num_descs = num_descs;
	cq->size_mask = num_descs - 1;
	cq->tail_idx = 0;
	cq->done_color = 1;

	return 0;
}

static void
iocpt_cq_reset(struct iocpt_cq *cq)
{
	cq->tail_idx = 0;
	cq->done_color = 1;

	memset(cq->base, 0, sizeof(struct iocpt_nop_comp) * cq->num_descs);
}

static void
iocpt_cq_map(struct iocpt_cq *cq, void *base, rte_iova_t base_pa)
{
	cq->base = base;
	cq->base_pa = base_pa;
}

uint32_t
iocpt_cq_service(struct iocpt_cq *cq, uint32_t work_to_do,
		iocpt_cq_cb cb, void *cb_arg)
{
	uint32_t work_done = 0;

	if (work_to_do == 0)
		return 0;

	while (cb(cq, cq->tail_idx, cb_arg)) {
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);
		if (cq->tail_idx == 0)
			cq->done_color = !cq->done_color;

		if (++work_done == work_to_do)
			break;
	}

	return work_done;
}

static int
iocpt_q_init(struct iocpt_queue *q, uint8_t type, uint32_t index,
	uint16_t num_descs, uint16_t num_segs, uint32_t socket_id)
{
	uint32_t ring_size;

	if (!rte_is_power_of_2(num_descs))
		return -EINVAL;

	ring_size = rte_log2_u32(num_descs);
	if (ring_size < 2 || ring_size > 16)
		return -EINVAL;

	q->type = type;
	q->index = index;
	q->num_descs = num_descs;
	q->num_segs = num_segs;
	q->size_mask = num_descs - 1;
	q->head_idx = 0;
	q->tail_idx = 0;

	q->info = rte_calloc_socket("iocpt",
				(uint64_t)num_descs * num_segs, sizeof(void *),
				rte_mem_page_size(), socket_id);
	if (q->info == NULL) {
		IOCPT_PRINT(ERR, "Cannot allocate queue info");
		return -ENOMEM;
	}

	return 0;
}

static void
iocpt_q_reset(struct iocpt_queue *q)
{
	q->head_idx = 0;
	q->tail_idx = 0;
}

static void
iocpt_q_map(struct iocpt_queue *q, void *base, rte_iova_t base_pa)
{
	q->base = base;
	q->base_pa = base_pa;
}

static void
iocpt_q_sg_map(struct iocpt_queue *q, void *base, rte_iova_t base_pa)
{
	q->sg_base = base;
	q->sg_base_pa = base_pa;
}

static void
iocpt_q_free(struct iocpt_queue *q)
{
	if (q->info != NULL) {
		rte_free(q->info);
		q->info = NULL;
	}
}

static void
iocpt_get_abs_stats(const struct iocpt_dev *dev,
		struct rte_cryptodev_stats *stats)
{
	uint32_t i;

	memset(stats, 0, sizeof(*stats));

	/* Sum up the per-queue stats counters */
	for (i = 0; i < dev->crypto_dev->data->nb_queue_pairs; i++) {
		struct rte_cryptodev_stats *q_stats = &dev->cryptoqs[i]->stats;

		stats->enqueued_count    += q_stats->enqueued_count;
		stats->dequeued_count    += q_stats->dequeued_count;
		stats->enqueue_err_count += q_stats->enqueue_err_count;
		stats->dequeue_err_count += q_stats->dequeue_err_count;
	}
}

void
iocpt_get_stats(const struct iocpt_dev *dev, struct rte_cryptodev_stats *stats)
{
	/* Retrieve the new absolute stats values */
	iocpt_get_abs_stats(dev, stats);

	/* Subtract the base stats values to get relative values */
	stats->enqueued_count    -= dev->stats_base.enqueued_count;
	stats->dequeued_count    -= dev->stats_base.dequeued_count;
	stats->enqueue_err_count -= dev->stats_base.enqueue_err_count;
	stats->dequeue_err_count -= dev->stats_base.dequeue_err_count;
}

void
iocpt_reset_stats(struct iocpt_dev *dev)
{
	uint32_t i;

	/* Erase the per-queue stats counters */
	for (i = 0; i < dev->crypto_dev->data->nb_queue_pairs; i++)
		memset(&dev->cryptoqs[i]->stats, 0,
			sizeof(dev->cryptoqs[i]->stats));

	/* Update the base stats values */
	iocpt_get_abs_stats(dev, &dev->stats_base);
}

static int
iocpt_session_write(struct iocpt_session_priv *priv,
		enum iocpt_sess_control_oper oper)
{
	struct iocpt_dev *dev = priv->dev;
	struct iocpt_admin_ctx ctx = {
		.pending_work = true,
		.cmd.sess_control = {
			.opcode = IOCPT_CMD_SESS_CONTROL,
			.type = priv->type,
			.oper = oper,
			.index = rte_cpu_to_le_32(priv->index),
			.key_len = rte_cpu_to_le_16(priv->key_len),
			.key_seg_len = (uint8_t)RTE_MIN(priv->key_len,
						IOCPT_SESS_KEY_SEG_LEN),
		},
	};
	struct iocpt_sess_control_cmd *cmd = &ctx.cmd.sess_control;
	uint16_t key_offset;
	uint8_t key_segs, seg, seg_len;
	int err;

	key_segs = ((priv->key_len - 1) >> IOCPT_SESS_KEY_SEG_SHFT) + 1;

	for (seg = 0; seg < key_segs; seg++) {
		ctx.pending_work = true;

		key_offset = seg * cmd->key_seg_len;
		seg_len = (uint8_t)RTE_MIN(priv->key_len - key_offset,
					IOCPT_SESS_KEY_SEG_LEN);
		memcpy(cmd->key, &priv->key[key_offset], seg_len);
		cmd->key_seg_idx = seg;

		/* Mark final segment */
		if (seg + 1 == key_segs)
			cmd->flags |= rte_cpu_to_le_16(IOCPT_SCTL_F_END);

		err = iocpt_adminq_post_wait(dev, &ctx);
		if (err != 0)
			return err;
	}

	return 0;
}

static int
iocpt_session_wdog(struct iocpt_dev *dev)
{
	struct iocpt_session_priv priv = {
		.dev = dev,
		.index = IOCPT_Q_WDOG_SESS_IDX,
		.type = IOCPT_SESS_AEAD_AES_GCM,
		.key_len = IOCPT_Q_WDOG_KEY_LEN,
	};

	/* Reserve session 0 for queue watchdog */
	rte_bitmap_clear(dev->sess_bm, IOCPT_Q_WDOG_SESS_IDX);

	return iocpt_session_write(&priv, IOCPT_SESS_INIT);
}

int
iocpt_session_init(struct iocpt_session_priv *priv)
{
	struct iocpt_dev *dev = priv->dev;
	uint64_t bm_slab = 0;
	uint32_t bm_pos = 0;
	int err = 0;

	rte_spinlock_lock(&dev->adminq_lock);

	if (rte_bitmap_scan(dev->sess_bm, &bm_pos, &bm_slab) > 0) {
		priv->index = bm_pos + rte_ctz64(bm_slab);
		rte_bitmap_clear(dev->sess_bm, priv->index);
	} else
		err = -ENOSPC;

	rte_spinlock_unlock(&dev->adminq_lock);

	if (err != 0) {
		IOCPT_PRINT(ERR, "session index space exhausted");
		return err;
	}

	err = iocpt_session_write(priv, IOCPT_SESS_INIT);
	if (err != 0) {
		rte_spinlock_lock(&dev->adminq_lock);
		rte_bitmap_set(dev->sess_bm, priv->index);
		rte_spinlock_unlock(&dev->adminq_lock);
		return err;
	}

	priv->flags |= IOCPT_S_F_INITED;

	return 0;
}

int
iocpt_session_update(struct iocpt_session_priv *priv)
{
	return iocpt_session_write(priv, IOCPT_SESS_UPDATE_KEY);
}

void
iocpt_session_deinit(struct iocpt_session_priv *priv)
{
	struct iocpt_dev *dev = priv->dev;
	struct iocpt_admin_ctx ctx = {
		.pending_work = true,
		.cmd.sess_control = {
			.opcode = IOCPT_CMD_SESS_CONTROL,
			.type = priv->type,
			.oper = IOCPT_SESS_DISABLE,
			.index = rte_cpu_to_le_32(priv->index),
			.key_len = rte_cpu_to_le_16(priv->key_len),
		},
	};

	(void)iocpt_adminq_post_wait(dev, &ctx);

	rte_spinlock_lock(&dev->adminq_lock);
	rte_bitmap_set(dev->sess_bm, priv->index);
	rte_spinlock_unlock(&dev->adminq_lock);

	priv->flags &= ~IOCPT_S_F_INITED;
}

static const struct rte_memzone *
iocpt_dma_zone_reserve(const char *type_name, uint16_t qid, size_t size,
			unsigned int align, int socket_id)
{
	char zone_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int err;

	err = snprintf(zone_name, sizeof(zone_name),
			"iocpt_%s_%u", type_name, qid);
	if (err >= RTE_MEMZONE_NAMESIZE) {
		IOCPT_PRINT(ERR, "Name %s too long", type_name);
		return NULL;
	}

	mz = rte_memzone_lookup(zone_name);
	if (mz != NULL)
		return mz;

	return rte_memzone_reserve_aligned(zone_name, size, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, align);
}

static int
iocpt_commonq_alloc(struct iocpt_dev *dev,
		uint8_t type,
		size_t struct_size,
		uint32_t socket_id,
		uint32_t index,
		const char *type_name,
		uint16_t flags,
		uint16_t num_descs,
		uint16_t num_segs,
		uint16_t desc_size,
		uint16_t cq_desc_size,
		uint16_t sg_desc_size,
		struct iocpt_common_q **comq)
{
	struct iocpt_common_q *new;
	uint32_t q_size, cq_size, sg_size, total_size;
	void *q_base, *cq_base, *sg_base;
	rte_iova_t q_base_pa = 0;
	rte_iova_t cq_base_pa = 0;
	rte_iova_t sg_base_pa = 0;
	size_t page_size = rte_mem_page_size();
	int err;

	*comq = NULL;

	q_size	= num_descs * desc_size;
	cq_size = num_descs * cq_desc_size;
	sg_size = num_descs * sg_desc_size;

	/*
	 * Note: aligning q_size/cq_size is not enough due to cq_base address
	 * aligning as q_base could be not aligned to the page.
	 * Adding page_size.
	 */
	total_size = RTE_ALIGN(q_size, page_size) +
		RTE_ALIGN(cq_size, page_size) + page_size;
	if (flags & IOCPT_Q_F_SG)
		total_size += RTE_ALIGN(sg_size, page_size) + page_size;

	new = rte_zmalloc_socket("iocpt", struct_size,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (new == NULL) {
		IOCPT_PRINT(ERR, "Cannot allocate queue structure");
		return -ENOMEM;
	}

	new->dev = dev;

	err = iocpt_q_init(&new->q, type, index, num_descs, num_segs,
			socket_id);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Queue initialization failed");
		goto err_free_q;
	}

	err = iocpt_cq_init(&new->cq, num_descs);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Completion queue initialization failed");
		goto err_deinit_q;
	}

	new->base_z = iocpt_dma_zone_reserve(type_name, index, total_size,
					IONIC_ALIGN, socket_id);
	if (new->base_z == NULL) {
		IOCPT_PRINT(ERR, "Cannot reserve queue DMA memory");
		err = -ENOMEM;
		goto err_deinit_cq;
	}

	new->base = new->base_z->addr;
	new->base_pa = new->base_z->iova;

	q_base = new->base;
	q_base_pa = new->base_pa;
	iocpt_q_map(&new->q, q_base, q_base_pa);

	cq_base = (void *)RTE_ALIGN((uintptr_t)q_base + q_size, page_size);
	cq_base_pa = RTE_ALIGN(q_base_pa + q_size, page_size);
	iocpt_cq_map(&new->cq, cq_base, cq_base_pa);

	if (flags & IOCPT_Q_F_SG) {
		sg_base = (void *)RTE_ALIGN((uintptr_t)cq_base + cq_size,
			page_size);
		sg_base_pa = RTE_ALIGN(cq_base_pa + cq_size, page_size);
		iocpt_q_sg_map(&new->q, sg_base, sg_base_pa);
	}

	IOCPT_PRINT(DEBUG, "q_base_pa %#jx cq_base_pa %#jx sg_base_pa %#jx",
		q_base_pa, cq_base_pa, sg_base_pa);

	*comq = new;

	return 0;

err_deinit_cq:
err_deinit_q:
	iocpt_q_free(&new->q);
err_free_q:
	rte_free(new);
	return err;
}

int
iocpt_cryptoq_alloc(struct iocpt_dev *dev, uint32_t socket_id, uint32_t index,
		uint16_t num_descs)
{
	struct iocpt_crypto_q *cptq;
	uint16_t flags = 0;
	int err;

	/* CryptoQ always supports scatter-gather */
	flags |= IOCPT_Q_F_SG;

	IOCPT_PRINT(DEBUG, "cptq %u num_descs %u num_segs %u",
		index, num_descs, 1);

	err = iocpt_commonq_alloc(dev,
		IOCPT_QTYPE_CRYPTOQ,
		sizeof(struct iocpt_crypto_q),
		socket_id,
		index,
		"crypto",
		flags,
		num_descs,
		1,
		sizeof(struct iocpt_crypto_desc),
		sizeof(struct iocpt_crypto_comp),
		sizeof(struct iocpt_crypto_sg_desc),
		(struct iocpt_common_q **)&cptq);
	if (err != 0)
		return err;

	cptq->flags = flags;

	dev->cryptoqs[index] = cptq;

	return 0;
}

struct ionic_doorbell *
iocpt_db_map(struct iocpt_dev *dev, struct iocpt_queue *q)
{
	return dev->db_pages + q->hw_type;
}

static int
iocpt_cryptoq_init(struct iocpt_crypto_q *cptq)
{
	struct iocpt_queue *q = &cptq->q;
	struct iocpt_dev *dev = cptq->dev;
	struct iocpt_cq *cq = &cptq->cq;
	struct iocpt_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IOCPT_CMD_Q_INIT,
			.type = IOCPT_QTYPE_CRYPTOQ,
			.ver = dev->qtype_info[IOCPT_QTYPE_CRYPTOQ].version,
			.index = rte_cpu_to_le_32(q->index),
			.flags = rte_cpu_to_le_16(IOCPT_QINIT_F_ENA |
						IOCPT_QINIT_F_SG),
			.intr_index = rte_cpu_to_le_16(IONIC_INTR_NONE),
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = rte_cpu_to_le_64(q->base_pa),
			.cq_ring_base = rte_cpu_to_le_64(cq->base_pa),
			.sg_ring_base = rte_cpu_to_le_64(q->sg_base_pa),
		},
	};
	int err;

	IOCPT_PRINT(DEBUG, "cptq_init.index %d", q->index);
	IOCPT_PRINT(DEBUG, "cptq_init.ring_base %#jx", q->base_pa);
	IOCPT_PRINT(DEBUG, "cptq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);
	IOCPT_PRINT(DEBUG, "cptq_init.ver %u", ctx.cmd.q_init.ver);

	iocpt_q_reset(q);
	iocpt_cq_reset(cq);

	err = iocpt_adminq_post_wait(dev, &ctx);
	if (err != 0)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = rte_le_to_cpu_32(ctx.comp.q_init.hw_index);
	q->db = iocpt_db_map(dev, q);

	IOCPT_PRINT(DEBUG, "cptq->hw_type %d", q->hw_type);
	IOCPT_PRINT(DEBUG, "cptq->hw_index %d", q->hw_index);
	IOCPT_PRINT(DEBUG, "cptq->db %p", q->db);

	cptq->flags |= IOCPT_Q_F_INITED;

	return 0;
}

static void
iocpt_cryptoq_deinit(struct iocpt_crypto_q *cptq)
{
	struct iocpt_dev *dev = cptq->dev;
	struct iocpt_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_control = {
			.opcode = IOCPT_CMD_Q_CONTROL,
			.type = IOCPT_QTYPE_CRYPTOQ,
			.index = rte_cpu_to_le_32(cptq->q.index),
			.oper = IOCPT_Q_DISABLE,
		},
	};
	unsigned long sleep_usec = 100UL * 1000;
	uint32_t sleep_cnt, sleep_max = IOCPT_CRYPTOQ_WAIT;
	int err;

	for (sleep_cnt = 0; sleep_cnt < sleep_max; sleep_cnt++) {
		ctx.pending_work = true;

		err = iocpt_adminq_post_wait(dev, &ctx);
		if (err != -EAGAIN)
			break;

		rte_delay_us_block(sleep_usec);
	}

	if (err != 0)
		IOCPT_PRINT(ERR, "Deinit queue %u returned %d after %u ms",
			cptq->q.index, err, sleep_cnt * 100);
	else
		IOCPT_PRINT(DEBUG, "Deinit queue %u returned %d after %u ms",
			cptq->q.index, err, sleep_cnt * 100);

	IOCPT_PRINT(DEBUG, "Queue %u watchdog: enq %"PRIu64" deq %"PRIu64,
		cptq->q.index, cptq->enqueued_wdogs, cptq->dequeued_wdogs);

	cptq->flags &= ~IOCPT_Q_F_INITED;
}

void
iocpt_cryptoq_free(struct iocpt_crypto_q *cptq)
{
	if (cptq == NULL)
		return;

	if (cptq->base_z != NULL) {
		rte_memzone_free(cptq->base_z);
		cptq->base_z = NULL;
		cptq->base = NULL;
		cptq->base_pa = 0;
	}

	iocpt_q_free(&cptq->q);

	rte_free(cptq);
}

static int
iocpt_adminq_alloc(struct iocpt_dev *dev)
{
	struct iocpt_admin_q *aq;
	uint16_t num_descs = IOCPT_ADMINQ_LENGTH;
	uint16_t flags = 0;
	int err;

	err = iocpt_commonq_alloc(dev,
		IOCPT_QTYPE_ADMINQ,
		sizeof(struct iocpt_admin_q),
		rte_socket_id(),
		0,
		"admin",
		flags,
		num_descs,
		1,
		sizeof(struct iocpt_admin_cmd),
		sizeof(struct iocpt_admin_comp),
		0,
		(struct iocpt_common_q **)&aq);
	if (err != 0)
		return err;

	aq->flags = flags;

	dev->adminq = aq;

	return 0;
}

static int
iocpt_adminq_init(struct iocpt_dev *dev)
{
	return iocpt_dev_adminq_init(dev);
}

static void
iocpt_adminq_deinit(struct iocpt_dev *dev)
{
	dev->adminq->flags &= ~IOCPT_Q_F_INITED;
}

static void
iocpt_adminq_free(struct iocpt_admin_q *aq)
{
	if (aq->base_z != NULL) {
		rte_memzone_free(aq->base_z);
		aq->base_z = NULL;
		aq->base = NULL;
		aq->base_pa = 0;
	}

	iocpt_q_free(&aq->q);

	rte_free(aq);
}

static int
iocpt_alloc_objs(struct iocpt_dev *dev)
{
	uint32_t bmsize, i;
	uint8_t *bm;
	int err;

	IOCPT_PRINT(DEBUG, "Crypto: %s", dev->name);

	dev->cryptoqs = rte_calloc_socket("iocpt",
				dev->max_qps, sizeof(*dev->cryptoqs),
				RTE_CACHE_LINE_SIZE, dev->socket_id);
	if (dev->cryptoqs == NULL) {
		IOCPT_PRINT(ERR, "Cannot allocate tx queues array");
		return -ENOMEM;
	}

	rte_spinlock_init(&dev->adminq_lock);
	rte_spinlock_init(&dev->adminq_service_lock);

	err = iocpt_adminq_alloc(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot allocate admin queue");
		err = -ENOMEM;
		goto err_free_cryptoqs;
	}

	dev->info_sz = RTE_ALIGN(sizeof(*dev->info), rte_mem_page_size());
	dev->info_z = iocpt_dma_zone_reserve("info", 0, dev->info_sz,
					IONIC_ALIGN, dev->socket_id);
	if (dev->info_z == NULL) {
		IOCPT_PRINT(ERR, "Cannot allocate dev info memory");
		err = -ENOMEM;
		goto err_free_adminq;
	}

	dev->info = dev->info_z->addr;
	dev->info_pa = dev->info_z->iova;

	bmsize = rte_bitmap_get_memory_footprint(dev->max_sessions);
	bm = rte_malloc_socket("iocpt", bmsize,
			RTE_CACHE_LINE_SIZE, dev->socket_id);
	if (bm == NULL) {
		IOCPT_PRINT(ERR, "Cannot allocate %uB bitmap memory", bmsize);
		err = -ENOMEM;
		goto err_free_dmazone;
	}

	dev->sess_bm = rte_bitmap_init(dev->max_sessions, bm, bmsize);
	if (dev->sess_bm == NULL) {
		IOCPT_PRINT(ERR, "Cannot initialize bitmap");
		err = -EFAULT;
		goto err_free_bm;
	}
	for (i = 0; i < dev->max_sessions; i++)
		rte_bitmap_set(dev->sess_bm, i);

	return 0;

err_free_bm:
	rte_free(bm);
err_free_dmazone:
	rte_memzone_free(dev->info_z);
	dev->info_z = NULL;
	dev->info = NULL;
	dev->info_pa = 0;
err_free_adminq:
	iocpt_adminq_free(dev->adminq);
	dev->adminq = NULL;
err_free_cryptoqs:
	rte_free(dev->cryptoqs);
	dev->cryptoqs = NULL;
	return err;
}

static int
iocpt_init(struct iocpt_dev *dev)
{
	int err;

	memset(&dev->stats_base, 0, sizeof(dev->stats_base));

	/* Uses dev_cmds */
	err = iocpt_dev_init(dev, dev->info_pa);
	if (err != 0)
		return err;

	err = iocpt_adminq_init(dev);
	if (err != 0)
		return err;

	/* Write the queue watchdog key */
	err = iocpt_session_wdog(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot setup watchdog session");
		goto err_out_adminq_deinit;
	}

	dev->state |= IOCPT_DEV_F_INITED;

	return 0;

err_out_adminq_deinit:
	iocpt_adminq_deinit(dev);

	return err;
}

void
iocpt_configure(struct iocpt_dev *dev)
{
	RTE_SET_USED(dev);
}

int
iocpt_start(struct iocpt_dev *dev)
{
	uint32_t i;
	int err;

	IOCPT_PRINT(DEBUG, "Starting %u queues",
		dev->crypto_dev->data->nb_queue_pairs);

	for (i = 0; i < dev->crypto_dev->data->nb_queue_pairs; i++) {
		err = iocpt_cryptoq_init(dev->cryptoqs[i]);
		if (err != 0)
			return err;
	}

	dev->state |= IOCPT_DEV_F_UP;

	return 0;
}

void
iocpt_stop(struct iocpt_dev *dev)
{
	uint32_t i;

	IOCPT_PRINT_CALL();

	dev->state &= ~IOCPT_DEV_F_UP;

	for (i = 0; i < dev->crypto_dev->data->nb_queue_pairs; i++) {
		struct iocpt_crypto_q *cptq = dev->cryptoqs[i];

		if (cptq->flags & IOCPT_Q_F_INITED)
			(void)iocpt_cryptoq_deinit(cptq);
	}
}

void
iocpt_deinit(struct iocpt_dev *dev)
{
	IOCPT_PRINT_CALL();

	if (!(dev->state & IOCPT_DEV_F_INITED))
		return;

	iocpt_adminq_deinit(dev);

	dev->state &= ~IOCPT_DEV_F_INITED;
}

static void
iocpt_free_objs(struct iocpt_dev *dev)
{
	void **queue_pairs = dev->crypto_dev->data->queue_pairs;
	uint32_t i;

	IOCPT_PRINT_CALL();

	for (i = 0; i < dev->crypto_dev->data->nb_queue_pairs; i++) {
		iocpt_cryptoq_free(queue_pairs[i]);
		queue_pairs[i] = NULL;
	}

	if (dev->sess_bm != NULL) {
		rte_bitmap_free(dev->sess_bm);
		rte_free(dev->sess_bm);
		dev->sess_bm = NULL;
	}

	if (dev->adminq != NULL) {
		iocpt_adminq_free(dev->adminq);
		dev->adminq = NULL;
	}

	if (dev->cryptoqs != NULL) {
		rte_free(dev->cryptoqs);
		dev->cryptoqs = NULL;
	}

	if (dev->info != NULL) {
		rte_memzone_free(dev->info_z);
		dev->info_z = NULL;
		dev->info = NULL;
		dev->info_pa = 0;
	}
}

static int
iocpt_devargs(struct rte_devargs *devargs, struct iocpt_dev *dev)
{
	RTE_SET_USED(devargs);
	RTE_SET_USED(dev);

	return 0;
}

int
iocpt_probe(void *bus_dev, struct rte_device *rte_dev,
	struct iocpt_dev_bars *bars, const struct iocpt_dev_intf *intf,
	uint8_t driver_id, uint8_t socket_id)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		"iocpt",
		sizeof(struct iocpt_dev),
		socket_id,
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	struct rte_cryptodev *cdev;
	struct iocpt_dev *dev;
	uint32_t i, sig;
	int err;

	/* Check structs (trigger error at compilation time) */
	iocpt_struct_size_checks();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		IOCPT_PRINT(ERR, "Multi-process not supported");
		err = -EPERM;
		goto err;
	}

	cdev = rte_cryptodev_pmd_create(rte_dev->name, rte_dev, &init_params);
	if (cdev == NULL) {
		IOCPT_PRINT(ERR, "Out of memory");
		err = -ENOMEM;
		goto err;
	}

	dev = cdev->data->dev_private;
	dev->crypto_dev = cdev;
	dev->bus_dev = bus_dev;
	dev->intf = intf;
	dev->driver_id = driver_id;
	dev->socket_id = socket_id;

	for (i = 0; i < bars->num_bars; i++) {
		struct ionic_dev_bar *bar = &bars->bar[i];

		IOCPT_PRINT(DEBUG,
			"bar[%u] = { .va = %p, .pa = %#jx, .len = %lu }",
			i, bar->vaddr, bar->bus_addr, bar->len);
		if (bar->vaddr == NULL) {
			IOCPT_PRINT(ERR, "Null bar found, aborting");
			err = -EFAULT;
			goto err_destroy_crypto_dev;
		}

		dev->bars.bar[i].vaddr = bar->vaddr;
		dev->bars.bar[i].bus_addr = bar->bus_addr;
		dev->bars.bar[i].len = bar->len;
	}
	dev->bars.num_bars = bars->num_bars;

	err = iocpt_devargs(rte_dev->devargs, dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot parse device arguments");
		goto err_destroy_crypto_dev;
	}

	err = iocpt_setup_bars(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot setup BARs: %d, aborting", err);
		goto err_destroy_crypto_dev;
	}

	sig = ioread32(&dev->dev_info->signature);
	if (sig != IOCPT_DEV_INFO_SIGNATURE) {
		IOCPT_PRINT(ERR, "Incompatible firmware signature %#x", sig);
		err = -EFAULT;
		goto err_destroy_crypto_dev;
	}

	for (i = 0; i < IOCPT_FWVERS_BUFLEN; i++)
		dev->fw_version[i] = ioread8(&dev->dev_info->fw_version[i]);
	dev->fw_version[IOCPT_FWVERS_BUFLEN - 1] = '\0';
	IOCPT_PRINT(DEBUG, "%s firmware: %s", dev->name, dev->fw_version);

	err = iocpt_dev_identify(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot identify device: %d, aborting",
			err);
		goto err_destroy_crypto_dev;
	}

	err = iocpt_alloc_objs(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot alloc device objects: %d", err);
		goto err_destroy_crypto_dev;
	}

	err = iocpt_init(dev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Cannot init device: %d, aborting", err);
		goto err_free_objs;
	}

	err = iocpt_assign_ops(cdev);
	if (err != 0) {
		IOCPT_PRINT(ERR, "Failed to configure opts");
		goto err_deinit_dev;
	}

	return 0;

err_deinit_dev:
	iocpt_deinit(dev);
err_free_objs:
	iocpt_free_objs(dev);
err_destroy_crypto_dev:
	rte_cryptodev_pmd_destroy(cdev);
err:
	return err;
}

int
iocpt_remove(struct rte_device *rte_dev)
{
	struct rte_cryptodev *cdev;
	struct iocpt_dev *dev;

	cdev = rte_cryptodev_pmd_get_named_dev(rte_dev->name);
	if (cdev == NULL) {
		IOCPT_PRINT(DEBUG, "Cannot find device %s", rte_dev->name);
		return -ENODEV;
	}

	dev = cdev->data->dev_private;

	iocpt_deinit(dev);

	iocpt_dev_reset(dev);

	iocpt_free_objs(dev);

	rte_cryptodev_pmd_destroy(cdev);

	return 0;
}

RTE_LOG_REGISTER_DEFAULT(iocpt_logtype, NOTICE);
