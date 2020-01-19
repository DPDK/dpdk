/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>

#include "ionic.h"
#include "ionic_logs.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"

int
ionic_qcq_enable(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = q->lif;
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.oper = IONIC_Q_ENABLE,
		},
	};

	if (qcq->flags & IONIC_QCQ_F_INTR) {
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_CLEAR);
	}

	return ionic_adminq_post_wait(lif, &ctx);
}

int
ionic_qcq_disable(struct ionic_qcq *qcq)
{
	struct ionic_queue *q = &qcq->q;
	struct ionic_lif *lif = q->lif;
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.oper = IONIC_Q_DISABLE,
		},
	};

	if (qcq->flags & IONIC_QCQ_F_INTR) {
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_SET);
	}

	return ionic_adminq_post_wait(lif, &ctx);
}

int
ionic_intr_alloc(struct ionic_lif *lif, struct ionic_intr_info *intr)
{
	struct ionic_adapter *adapter = lif->adapter;
	struct ionic_dev *idev = &adapter->idev;
	unsigned long index;

	/*
	 * Note: interrupt handler is called for index = 0 only
	 * (we use interrupts for the notifyq only anyway,
	 * which hash index = 0)
	 */

	for (index = 0; index < adapter->nintrs; index++)
		if (!adapter->intrs[index])
			break;

	if (index == adapter->nintrs)
		return -ENOSPC;

	adapter->intrs[index] = true;

	ionic_intr_init(idev, intr, index);

	return 0;
}

void
ionic_intr_free(struct ionic_lif *lif, struct ionic_intr_info *intr)
{
	if (intr->index != IONIC_INTR_INDEX_NOT_ASSIGNED)
		lif->adapter->intrs[intr->index] = false;
}

static int
ionic_qcq_alloc(struct ionic_lif *lif, uint8_t type,
		uint32_t index,
		const char *base, uint32_t flags,
		uint32_t num_descs,
		uint32_t desc_size,
		uint32_t cq_desc_size,
		uint32_t sg_desc_size,
		uint32_t pid, struct ionic_qcq **qcq)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *new;
	uint32_t q_size, cq_size, sg_size, total_size;
	void *q_base, *cq_base, *sg_base;
	rte_iova_t q_base_pa = 0;
	rte_iova_t cq_base_pa = 0;
	rte_iova_t sg_base_pa = 0;
	uint32_t socket_id = rte_socket_id();
	int err;

	*qcq = NULL;

	q_size  = num_descs * desc_size;
	cq_size = num_descs * cq_desc_size;
	sg_size = num_descs * sg_desc_size;

	total_size = RTE_ALIGN(q_size, PAGE_SIZE) +
		RTE_ALIGN(cq_size, PAGE_SIZE);
	/*
	 * Note: aligning q_size/cq_size is not enough due to cq_base address
	 * aligning as q_base could be not aligned to the page.
	 * Adding PAGE_SIZE.
	 */
	total_size += PAGE_SIZE;

	if (flags & IONIC_QCQ_F_SG) {
		total_size += RTE_ALIGN(sg_size, PAGE_SIZE);
		total_size += PAGE_SIZE;
	}

	new = rte_zmalloc("ionic", sizeof(*new), 0);
	if (!new) {
		IONIC_PRINT(ERR, "Cannot allocate queue structure");
		return -ENOMEM;
	}

	new->lif = lif;
	new->flags = flags;

	new->q.info = rte_zmalloc("ionic", sizeof(*new->q.info) * num_descs, 0);
	if (!new->q.info) {
		IONIC_PRINT(ERR, "Cannot allocate queue info");
		return -ENOMEM;
	}

	new->q.type = type;

	err = ionic_q_init(lif, idev, &new->q, index, num_descs,
		desc_size, sg_desc_size, pid);
	if (err) {
		IONIC_PRINT(ERR, "Queue initialization failed");
		return err;
	}

	if (flags & IONIC_QCQ_F_INTR) {
		err = ionic_intr_alloc(lif, &new->intr);
		if (err)
			return err;

		ionic_intr_mask_assert(idev->intr_ctrl, new->intr.index,
			IONIC_INTR_MASK_SET);
	} else {
		new->intr.index = IONIC_INTR_INDEX_NOT_ASSIGNED;
	}

	err = ionic_cq_init(lif, &new->cq, &new->intr,
		num_descs, cq_desc_size);
	if (err) {
		IONIC_PRINT(ERR, "Completion queue initialization failed");
		goto err_out_free_intr;
	}

	new->base_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		base /* name */, index /* queue_idx */,
		total_size, IONIC_ALIGN, socket_id);

	if (!new->base_z) {
		IONIC_PRINT(ERR, "Cannot reserve queue DMA memory");
		err = -ENOMEM;
		goto err_out_free_intr;
	}

	new->base = new->base_z->addr;
	new->base_pa = new->base_z->iova;
	new->total_size = total_size;

	q_base = new->base;
	q_base_pa = new->base_pa;

	cq_base = (void *)RTE_ALIGN((uintptr_t)q_base + q_size, PAGE_SIZE);
	cq_base_pa = RTE_ALIGN(q_base_pa + q_size, PAGE_SIZE);

	if (flags & IONIC_QCQ_F_SG) {
		sg_base = (void *)RTE_ALIGN((uintptr_t)cq_base + cq_size,
			PAGE_SIZE);
		sg_base_pa = RTE_ALIGN(cq_base_pa + cq_size, PAGE_SIZE);
		ionic_q_sg_map(&new->q, sg_base, sg_base_pa);
	}

	IONIC_PRINT(DEBUG, "Q-Base-PA = %ju CQ-Base-PA = %ju "
		"SG-base-PA = %ju",
		q_base_pa, cq_base_pa, sg_base_pa);

	ionic_q_map(&new->q, q_base, q_base_pa);
	ionic_cq_map(&new->cq, cq_base, cq_base_pa);
	ionic_cq_bind(&new->cq, &new->q);

	*qcq = new;

	return 0;

err_out_free_intr:
	if (flags & IONIC_QCQ_F_INTR)
		ionic_intr_free(lif, &new->intr);

	return err;
}

void
ionic_qcq_free(struct ionic_qcq *qcq)
{
	if (qcq->base_z) {
		qcq->base = NULL;
		qcq->base_pa = 0;
		rte_memzone_free(qcq->base_z);
		qcq->base_z = NULL;
	}

	if (qcq->q.info) {
		rte_free(qcq->q.info);
		qcq->q.info = NULL;
	}

	rte_free(qcq);
}

static int
ionic_admin_qcq_alloc(struct ionic_lif *lif)
{
	uint32_t flags;
	int err = -ENOMEM;

	flags = 0;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_ADMINQ, 0, "admin", flags,
		IONIC_ADMINQ_LENGTH,
		sizeof(struct ionic_admin_cmd),
		sizeof(struct ionic_admin_comp),
		0,
		lif->kern_pid, &lif->adminqcq);
	if (err)
		return err;

	return 0;
}

static int
ionic_notify_qcq_alloc(struct ionic_lif *lif)
{
	uint32_t flags;
	int err = -ENOMEM;

	flags = IONIC_QCQ_F_NOTIFYQ | IONIC_QCQ_F_INTR;

	err = ionic_qcq_alloc(lif, IONIC_QTYPE_NOTIFYQ, 0, "notify",
		flags,
		IONIC_NOTIFYQ_LENGTH,
		sizeof(struct ionic_notifyq_cmd),
		sizeof(union ionic_notifyq_comp),
		0,
		lif->kern_pid, &lif->notifyqcq);
	if (err)
		return err;

	return 0;
}

static void *
ionic_bus_map_dbpage(struct ionic_adapter *adapter, int page_num)
{
	char *vaddr = adapter->bars[IONIC_PCI_BAR_DBELL].vaddr;

	if (adapter->num_bars <= IONIC_PCI_BAR_DBELL)
		return NULL;

	return (void *)&vaddr[page_num << PAGE_SHIFT];
}

int
ionic_lif_alloc(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	uint32_t socket_id = rte_socket_id();
	int dbpage_num;
	int err;

	snprintf(lif->name, sizeof(lif->name), "lif%u", lif->index);

	IONIC_PRINT(DEBUG, "Allocating Lif Info");

	rte_spinlock_init(&lif->adminq_lock);
	rte_spinlock_init(&lif->adminq_service_lock);

	lif->kern_pid = 0;

	dbpage_num = ionic_db_page_num(lif, 0);

	lif->kern_dbpage = ionic_bus_map_dbpage(adapter, dbpage_num);
	if (!lif->kern_dbpage) {
		IONIC_PRINT(ERR, "Cannot map dbpage, aborting");
		return -ENOMEM;
	}

	IONIC_PRINT(DEBUG, "Allocating Notify Queue");

	err = ionic_notify_qcq_alloc(lif);

	if (err) {
		IONIC_PRINT(ERR, "Cannot allocate notify queue");
		return err;
	}

	IONIC_PRINT(DEBUG, "Allocating Admin Queue");

	IONIC_PRINT(DEBUG, "Allocating Admin Queue");

	err = ionic_admin_qcq_alloc(lif);
	if (err) {
		IONIC_PRINT(ERR, "Cannot allocate admin queue");
		return err;
	}

	IONIC_PRINT(DEBUG, "Allocating Lif Info");

	lif->info_sz = RTE_ALIGN(sizeof(*lif->info), PAGE_SIZE);

	lif->info_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		"lif_info", 0 /* queue_idx*/,
		lif->info_sz, IONIC_ALIGN, socket_id);
	if (!lif->info_z) {
		IONIC_PRINT(ERR, "Cannot allocate lif info memory");
		return -ENOMEM;
	}

	lif->info = lif->info_z->addr;
	lif->info_pa = lif->info_z->iova;

	return 0;
}

void
ionic_lif_free(struct ionic_lif *lif)
{
	if (lif->notifyqcq) {
		ionic_qcq_free(lif->notifyqcq);
		lif->notifyqcq = NULL;
	}

	if (lif->adminqcq) {
		ionic_qcq_free(lif->adminqcq);
		lif->adminqcq = NULL;
	}

	if (lif->info) {
		rte_memzone_free(lif->info_z);
		lif->info = NULL;
	}
}

static void
ionic_lif_qcq_deinit(struct ionic_lif *lif, struct ionic_qcq *qcq)
{
	struct ionic_dev *idev = &lif->adapter->idev;

	if (!(qcq->flags & IONIC_QCQ_F_INITED))
		return;

	if (qcq->flags & IONIC_QCQ_F_INTR)
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_SET);

	qcq->flags &= ~IONIC_QCQ_F_INITED;
}

bool
ionic_adminq_service(struct ionic_cq *cq, uint32_t cq_desc_index,
		void *cb_arg __rte_unused)
{
	struct ionic_admin_comp *cq_desc_base = cq->base;
	struct ionic_admin_comp *cq_desc = &cq_desc_base[cq_desc_index];

	if (!color_match(cq_desc->color, cq->done_color))
		return false;

	ionic_q_service(cq->bound_q, cq_desc_index, cq_desc->comp_index, NULL);

	return true;
}

/* This acts like ionic_napi */
int
ionic_qcq_service(struct ionic_qcq *qcq, int budget, ionic_cq_cb cb,
		void *cb_arg)
{
	struct ionic_cq *cq = &qcq->cq;
	uint32_t work_done;

	work_done = ionic_cq_service(cq, budget, cb, cb_arg);

	return work_done;
}

static void
ionic_link_status_check(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	bool link_up;

	lif->state &= ~IONIC_LIF_F_LINK_CHECK_NEEDED;

	if (!lif->info)
		return;

	link_up = (lif->info->status.link_status == IONIC_PORT_OPER_STATUS_UP);

	if ((link_up  && adapter->link_up) ||
	    (!link_up && !adapter->link_up))
		return;

	if (link_up) {
		IONIC_PRINT(DEBUG, "Link up - %d Gbps",
			lif->info->status.link_speed);
		adapter->link_speed = lif->info->status.link_speed;
	} else {
		IONIC_PRINT(DEBUG, "Link down");
	}

	adapter->link_up = link_up;
}

static bool
ionic_notifyq_cb(struct ionic_cq *cq, uint32_t cq_desc_index, void *cb_arg)
{
	union ionic_notifyq_comp *cq_desc_base = cq->base;
	union ionic_notifyq_comp *cq_desc = &cq_desc_base[cq_desc_index];
	struct ionic_lif *lif = cb_arg;

	IONIC_PRINT(DEBUG, "Notifyq callback eid = %jd ecode = %d",
		cq_desc->event.eid, cq_desc->event.ecode);

	/* Have we run out of new completions to process? */
	if (!(cq_desc->event.eid > lif->last_eid))
		return false;

	lif->last_eid = cq_desc->event.eid;

	switch (cq_desc->event.ecode) {
	case IONIC_EVENT_LINK_CHANGE:
		IONIC_PRINT(DEBUG,
			"Notifyq IONIC_EVENT_LINK_CHANGE eid=%jd link_status=%d link_speed=%d",
			cq_desc->event.eid,
			cq_desc->link_change.link_status,
			cq_desc->link_change.link_speed);

		lif->state |= IONIC_LIF_F_LINK_CHECK_NEEDED;

		break;
	default:
		IONIC_PRINT(WARNING, "Notifyq bad event ecode=%d eid=%jd",
			cq_desc->event.ecode, cq_desc->event.eid);
		break;
	}

	return true;
}

int
ionic_notifyq_handler(struct ionic_lif *lif, int budget)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *qcq = lif->notifyqcq;
	uint32_t work_done;

	if (!(qcq->flags & IONIC_QCQ_F_INITED)) {
		IONIC_PRINT(DEBUG, "Notifyq not yet initialized");
		return -1;
	}

	ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
		IONIC_INTR_MASK_SET);

	work_done = ionic_qcq_service(qcq, budget, ionic_notifyq_cb, lif);

	if (lif->state & IONIC_LIF_F_LINK_CHECK_NEEDED)
		ionic_link_status_check(lif);

	ionic_intr_credits(idev->intr_ctrl, qcq->intr.index,
		work_done, IONIC_INTR_CRED_RESET_COALESCE);

	ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
		IONIC_INTR_MASK_CLEAR);

	return 0;
}

static int
ionic_lif_adminq_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *qcq = lif->adminqcq;
	struct ionic_queue *q = &qcq->q;
	struct ionic_q_init_comp comp;
	int err;

	ionic_dev_cmd_adminq_init(idev, qcq, lif->index, qcq->intr.index);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return err;

	ionic_dev_cmd_comp(idev, &comp);

	q->hw_type = comp.hw_type;
	q->hw_index = comp.hw_index;
	q->db = ionic_db_map(lif, q);

	IONIC_PRINT(DEBUG, "adminq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "adminq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "adminq->db %p", q->db);

	if (qcq->flags & IONIC_QCQ_F_INTR)
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_CLEAR);

	qcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

static int
ionic_lif_notifyq_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_qcq *qcq = lif->notifyqcq;
	struct ionic_queue *q = &qcq->q;
	int err;

	struct ionic_admin_ctx ctx = {
		.pending_work = true,
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.flags = (IONIC_QINIT_F_IRQ | IONIC_QINIT_F_ENA),
			.intr_index = qcq->intr.index,
			.pid = q->pid,
			.ring_size = rte_log2_u32(q->num_descs),
			.ring_base = q->base_pa,
		}
	};

	IONIC_PRINT(DEBUG, "notifyq_init.pid %d", ctx.cmd.q_init.pid);
	IONIC_PRINT(DEBUG, "notifyq_init.index %d",
		ctx.cmd.q_init.index);
	IONIC_PRINT(DEBUG, "notifyq_init.ring_base 0x%" PRIx64 "",
		ctx.cmd.q_init.ring_base);
	IONIC_PRINT(DEBUG, "notifyq_init.ring_size %d",
		ctx.cmd.q_init.ring_size);

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_type = ctx.comp.q_init.hw_type;
	q->hw_index = ctx.comp.q_init.hw_index;
	q->db = NULL;

	IONIC_PRINT(DEBUG, "notifyq->hw_type %d", q->hw_type);
	IONIC_PRINT(DEBUG, "notifyq->hw_index %d", q->hw_index);
	IONIC_PRINT(DEBUG, "notifyq->db %p", q->db);

	if (qcq->flags & IONIC_QCQ_F_INTR)
		ionic_intr_mask(idev->intr_ctrl, qcq->intr.index,
			IONIC_INTR_MASK_CLEAR);

	qcq->flags |= IONIC_QCQ_F_INITED;

	return 0;
}

int
ionic_lif_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_q_init_comp comp;
	int err;

	ionic_dev_cmd_lif_init(idev, lif->index, lif->info_pa);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	ionic_dev_cmd_comp(idev, &comp);
	if (err)
		return err;

	lif->hw_index = comp.hw_index;

	err = ionic_lif_adminq_init(lif);
	if (err)
		return err;

	err = ionic_lif_notifyq_init(lif);
	if (err)
		goto err_out_adminq_deinit;

	lif->state |= IONIC_LIF_F_INITED;

	return 0;

err_out_adminq_deinit:
	ionic_lif_qcq_deinit(lif, lif->adminqcq);

	return err;
}

void
ionic_lif_deinit(struct ionic_lif *lif)
{
	if (!(lif->state & IONIC_LIF_F_INITED))
		return;

	ionic_lif_qcq_deinit(lif, lif->notifyqcq);
	ionic_lif_qcq_deinit(lif, lif->adminqcq);

	lif->state &= ~IONIC_LIF_F_INITED;
}

int
ionic_lif_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	int err;
	unsigned int i;
	unsigned int lif_words = sizeof(ident->lif.words) /
		sizeof(ident->lif.words[0]);
	unsigned int cmd_words = sizeof(idev->dev_cmd->data) /
		sizeof(idev->dev_cmd->data[0]);
	unsigned int nwords;

	ionic_dev_cmd_lif_identify(idev, IONIC_LIF_TYPE_CLASSIC,
		IONIC_IDENTITY_VERSION_1);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return (err);

	nwords = RTE_MIN(lif_words, cmd_words);
	for (i = 0; i < nwords; i++)
		ident->lif.words[i] = ioread32(&idev->dev_cmd->data[i]);

	IONIC_PRINT(INFO, "capabilities 0x%" PRIx64 " ",
		ident->lif.capabilities);

	IONIC_PRINT(INFO, "eth.max_ucast_filters 0x%" PRIx32 " ",
		ident->lif.eth.max_ucast_filters);
	IONIC_PRINT(INFO, "eth.max_mcast_filters 0x%" PRIx32 " ",
		ident->lif.eth.max_mcast_filters);

	IONIC_PRINT(INFO, "eth.features 0x%" PRIx64 " ",
		ident->lif.eth.config.features);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_ADMINQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_ADMINQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_NOTIFYQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_NOTIFYQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_RXQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_TXQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ]);

	return 0;
}

int
ionic_lifs_size(struct ionic_adapter *adapter)
{
	struct ionic_identity *ident = &adapter->ident;
	uint32_t nlifs = ident->dev.nlifs;
	uint32_t nintrs, dev_nintrs = ident->dev.nintrs;

	adapter->max_ntxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ];
	adapter->max_nrxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ];

	nintrs = nlifs * 1 /* notifyq */;

	if (nintrs > dev_nintrs) {
		IONIC_PRINT(ERR, "At most %d intr queues supported, minimum required is %u",
			dev_nintrs, nintrs);
		return -ENOSPC;
	}

	adapter->nintrs = nintrs;

	return 0;
}
