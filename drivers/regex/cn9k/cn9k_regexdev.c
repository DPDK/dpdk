/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <stdio.h>
#include <unistd.h>

#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>


/* REE common headers */
#include "cn9k_regexdev.h"
#include "cn9k_regexdev_compiler.h"


/* HW matches are at offset 0x80 from RES_PTR_ADDR
 * In op structure matches starts at W5 (0x28)
 * There is a need to copy to 0x28 to 0x80 The matches that are at the tail
 * Which are 88 B. Each match holds 8 B, so up to 11 matches can be copied
 */
#define REE_NUM_MATCHES_ALIGN	11
/* The REE co-processor will write up to 254 job match structures
 * (REE_MATCH_S) starting at address [RES_PTR_ADDR] + 0x80.
 */
#define REE_MATCH_OFFSET	0x80

#define REE_MAX_RULES_PER_GROUP 0xFFFF
#define REE_MAX_GROUPS 0xFFFF


#define REE_RULE_DB_VERSION	2
#define REE_RULE_DB_REVISION	0

struct ree_rule_db_entry {
	uint8_t		type;
	uint32_t	addr;
	uint64_t	value;
};

struct ree_rule_db {
	uint32_t version;
	uint32_t revision;
	uint32_t number_of_entries;
	struct ree_rule_db_entry entries[];
} __rte_packed;

static void
qp_memzone_name_get(char *name, int size, int dev_id, int qp_id)
{
	snprintf(name, size, "cn9k_ree_lf_mem_%u:%u", dev_id, qp_id);
}

static struct roc_ree_qp *
ree_qp_create(const struct rte_regexdev *dev, uint16_t qp_id)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	uint64_t pg_sz = sysconf(_SC_PAGESIZE);
	struct roc_ree_vf *vf = &data->vf;
	const struct rte_memzone *lf_mem;
	uint32_t len, iq_len, size_div2;
	char name[RTE_MEMZONE_NAMESIZE];
	uint64_t used_len, iova;
	struct roc_ree_qp *qp;
	uint8_t *va;
	int ret;

	/* Allocate queue pair */
	qp = rte_zmalloc("CN9K Regex PMD Queue Pair", sizeof(*qp),
				ROC_ALIGN);
	if (qp == NULL) {
		cn9k_err("Could not allocate queue pair");
		return NULL;
	}

	iq_len = REE_IQ_LEN;

	/*
	 * Queue size must be in units of 128B 2 * REE_INST_S (which is 64B),
	 * and a power of 2.
	 * effective queue size to software is (size - 1) * 128
	 */
	size_div2 = iq_len >> 1;

	/* For pending queue */
	len = iq_len * RTE_ALIGN(sizeof(struct roc_ree_rid), 8);

	/* So that instruction queues start as pg size aligned */
	len = RTE_ALIGN(len, pg_sz);

	/* For instruction queues */
	len += REE_IQ_LEN * sizeof(union roc_ree_inst);

	/* Waste after instruction queues */
	len = RTE_ALIGN(len, pg_sz);

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id,
			    qp_id);

	lf_mem = rte_memzone_reserve_aligned(name, len, rte_socket_id(),
			RTE_MEMZONE_SIZE_HINT_ONLY | RTE_MEMZONE_256MB,
			RTE_CACHE_LINE_SIZE);
	if (lf_mem == NULL) {
		cn9k_err("Could not allocate reserved memzone");
		goto qp_free;
	}

	va = lf_mem->addr;
	iova = lf_mem->iova;

	memset(va, 0, len);

	/* Initialize pending queue */
	qp->pend_q.rid_queue = (struct roc_ree_rid *)va;
	qp->pend_q.enq_tail = 0;
	qp->pend_q.deq_head = 0;
	qp->pend_q.pending_count = 0;

	used_len = iq_len * RTE_ALIGN(sizeof(struct roc_ree_rid), 8);
	used_len = RTE_ALIGN(used_len, pg_sz);
	iova += used_len;

	qp->iq_dma_addr = iova;
	qp->id = qp_id;
	qp->base = roc_ree_qp_get_base(vf, qp_id);
	qp->roc_regexdev_jobid = 0;
	qp->write_offset = 0;

	ret = roc_ree_iq_enable(vf, qp, REE_QUEUE_HI_PRIO, size_div2);
	if (ret) {
		cn9k_err("Could not enable instruction queue");
		goto qp_free;
	}

	return qp;

qp_free:
	rte_free(qp);
	return NULL;
}

static int
ree_qp_destroy(const struct rte_regexdev *dev, struct roc_ree_qp *qp)
{
	const struct rte_memzone *lf_mem;
	char name[RTE_MEMZONE_NAMESIZE];
	int ret;

	roc_ree_iq_disable(qp);

	qp_memzone_name_get(name, RTE_MEMZONE_NAMESIZE, dev->data->dev_id,
			    qp->id);

	lf_mem = rte_memzone_lookup(name);

	ret = rte_memzone_free(lf_mem);
	if (ret)
		return ret;

	rte_free(qp);

	return 0;
}

static int
ree_queue_pair_release(struct rte_regexdev *dev, uint16_t qp_id)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_qp *qp = data->queue_pairs[qp_id];
	int ret;

	ree_func_trace("Queue=%d", qp_id);

	if (qp == NULL)
		return -EINVAL;

	ret = ree_qp_destroy(dev, qp);
	if (ret) {
		cn9k_err("Could not destroy queue pair %d", qp_id);
		return ret;
	}

	data->queue_pairs[qp_id] = NULL;

	return 0;
}

static struct rte_regexdev *
ree_dev_register(const char *name)
{
	struct rte_regexdev *dev;

	cn9k_ree_dbg("Creating regexdev %s\n", name);

	/* allocate device structure */
	dev = rte_regexdev_register(name);
	if (dev == NULL) {
		cn9k_err("Failed to allocate regex device for %s", name);
		return NULL;
	}

	/* allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		dev->data->dev_private =
				rte_zmalloc_socket("regexdev device private",
						sizeof(struct cn9k_ree_data),
						RTE_CACHE_LINE_SIZE,
						rte_socket_id());

		if (dev->data->dev_private == NULL) {
			cn9k_err("Cannot allocate memory for dev %s private data",
					name);

			rte_regexdev_unregister(dev);
			return NULL;
		}
	}

	return dev;
}

static int
ree_dev_unregister(struct rte_regexdev *dev)
{
	cn9k_ree_dbg("Closing regex device %s", dev->device->name);

	/* free regex device */
	rte_regexdev_unregister(dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(dev->data->dev_private);

	return 0;
}

static int
ree_dev_fini(struct rte_regexdev *dev)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;
	int i, ret;

	ree_func_trace();

	for (i = 0; i < data->nb_queue_pairs; i++) {
		ret = ree_queue_pair_release(dev, i);
		if (ret)
			return ret;
	}

	ret = roc_ree_queues_detach(vf);
	if (ret)
		cn9k_err("Could not detach queues");

	/* TEMP : should be in lib */
	rte_free(data->queue_pairs);
	rte_free(data->rules);

	roc_ree_dev_fini(vf);

	ret = ree_dev_unregister(dev);
	if (ret)
		cn9k_err("Could not destroy PMD");

	return ret;
}

static inline int
ree_enqueue(struct roc_ree_qp *qp, struct rte_regex_ops *op,
		 struct roc_ree_pending_queue *pend_q)
{
	union roc_ree_inst inst;
	union ree_res *res;
	uint32_t offset;

	if (unlikely(pend_q->pending_count >= REE_DEFAULT_CMD_QLEN)) {
		cn9k_err("Pending count %" PRIu64 " is greater than Q size %d",
		pend_q->pending_count, REE_DEFAULT_CMD_QLEN);
		return -EAGAIN;
	}
	if (unlikely(op->mbuf->data_len > REE_MAX_PAYLOAD_SIZE ||
			op->mbuf->data_len == 0)) {
		cn9k_err("Packet length %d is greater than MAX payload %d",
				op->mbuf->data_len, REE_MAX_PAYLOAD_SIZE);
		return -EAGAIN;
	}

	/* W 0 */
	inst.cn98xx.ooj = 1;
	inst.cn98xx.dg = 0;
	inst.cn98xx.doneint = 0;
	/* W 1 */
	inst.cn98xx.inp_ptr_addr = rte_pktmbuf_mtod(op->mbuf, uint64_t);
	/* W 2 */
	inst.cn98xx.inp_ptr_ctl = op->mbuf->data_len & 0x7FFF;
	inst.cn98xx.inp_ptr_ctl = inst.cn98xx.inp_ptr_ctl << 32;

	/* W 3 */
	inst.cn98xx.res_ptr_addr = (uint64_t)op;
	/* W 4 */
	inst.cn98xx.wq_ptr = 0;
	/* W 5 */
	inst.cn98xx.ggrp = 0;
	inst.cn98xx.tt = 0;
	inst.cn98xx.tag = 0;
	/* W 6 */
	inst.cn98xx.ree_job_length = op->mbuf->data_len & 0x7FFF;
	if (op->req_flags & RTE_REGEX_OPS_REQ_STOP_ON_MATCH_F)
		inst.cn98xx.ree_job_ctrl = (0x2 << 8);
	else if (op->req_flags & RTE_REGEX_OPS_REQ_MATCH_HIGH_PRIORITY_F)
		inst.cn98xx.ree_job_ctrl = (0x1 << 8);
	else
		inst.cn98xx.ree_job_ctrl = 0;
	inst.cn98xx.ree_job_id = qp->roc_regexdev_jobid;
	/* W 7 */
	inst.cn98xx.ree_job_subset_id_0 = op->group_id0;
	if (op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID1_VALID_F)
		inst.cn98xx.ree_job_subset_id_1 = op->group_id1;
	else
		inst.cn98xx.ree_job_subset_id_1 = op->group_id0;
	if (op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID2_VALID_F)
		inst.cn98xx.ree_job_subset_id_2 = op->group_id2;
	else
		inst.cn98xx.ree_job_subset_id_2 = op->group_id0;
	if (op->req_flags & RTE_REGEX_OPS_REQ_GROUP_ID3_VALID_F)
		inst.cn98xx.ree_job_subset_id_3 = op->group_id3;
	else
		inst.cn98xx.ree_job_subset_id_3 = op->group_id0;

	/* Copy REE command to Q */
	offset = qp->write_offset * sizeof(inst);
	memcpy((void *)(qp->iq_dma_addr + offset), &inst, sizeof(inst));

	pend_q->rid_queue[pend_q->enq_tail].rid = (uintptr_t)op;
	pend_q->rid_queue[pend_q->enq_tail].user_id = op->user_id;

	/* Mark result as not done */
	res = (union ree_res *)(op);
	res->s.done = 0;
	res->s.ree_err = 0;

	/* We will use soft queue length here to limit requests */
	REE_MOD_INC(pend_q->enq_tail, REE_DEFAULT_CMD_QLEN);
	pend_q->pending_count += 1;
	REE_MOD_INC(qp->roc_regexdev_jobid, 0xFFFFFF);
	REE_MOD_INC(qp->write_offset, REE_IQ_LEN);

	return 0;
}

static uint16_t
cn9k_ree_enqueue_burst(struct rte_regexdev *dev, uint16_t qp_id,
		       struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_qp *qp = data->queue_pairs[qp_id];
	struct roc_ree_pending_queue *pend_q;
	uint16_t nb_allowed, count = 0;
	struct rte_regex_ops *op;
	int ret;

	pend_q = &qp->pend_q;

	nb_allowed = REE_DEFAULT_CMD_QLEN - pend_q->pending_count;
	if (nb_ops > nb_allowed)
		nb_ops = nb_allowed;

	for (count = 0; count < nb_ops; count++) {
		op = ops[count];
		ret = ree_enqueue(qp, op, pend_q);

		if (unlikely(ret))
			break;
	}

	/*
	 * Make sure all instructions are written before DOORBELL is activated
	 */
	rte_io_wmb();

	/* Update Doorbell */
	plt_write64(count, qp->base + REE_LF_DOORBELL);

	return count;
}

static inline void
ree_dequeue_post_process(struct rte_regex_ops *ops)
{
	uint8_t ree_res_mcnt, ree_res_dmcnt;
	int off = REE_MATCH_OFFSET;
	struct ree_res_s_98 *res;
	uint16_t ree_res_status;
	uint64_t match;

	res = (struct ree_res_s_98 *)ops;
	/* store res values on stack since ops and res
	 * are using the same memory
	 */
	ree_res_status = res->ree_res_status;
	ree_res_mcnt = res->ree_res_mcnt;
	ree_res_dmcnt = res->ree_res_dmcnt;
	ops->rsp_flags = 0;
	ops->nb_actual_matches = ree_res_dmcnt;
	ops->nb_matches = ree_res_mcnt;
	if (unlikely(res->ree_err)) {
		ops->nb_actual_matches = 0;
		ops->nb_matches = 0;
	}

	if (unlikely(ree_res_status != REE_TYPE_RESULT_DESC)) {
		if (ree_res_status & REE_STATUS_PMI_SOJ_BIT)
			ops->rsp_flags |= RTE_REGEX_OPS_RSP_PMI_SOJ_F;
		if (ree_res_status & REE_STATUS_PMI_EOJ_BIT)
			ops->rsp_flags |= RTE_REGEX_OPS_RSP_PMI_EOJ_F;
		if (ree_res_status & REE_STATUS_ML_CNT_DET_BIT)
			ops->rsp_flags |= RTE_REGEX_OPS_RSP_MAX_SCAN_TIMEOUT_F;
		if (ree_res_status & REE_STATUS_MM_CNT_DET_BIT)
			ops->rsp_flags |= RTE_REGEX_OPS_RSP_MAX_MATCH_F;
		if (ree_res_status & REE_STATUS_MP_CNT_DET_BIT)
			ops->rsp_flags |= RTE_REGEX_OPS_RSP_MAX_PREFIX_F;
	}
	if (ops->nb_matches > 0) {
		/* Move the matches to the correct offset */
		off = ((ops->nb_matches < REE_NUM_MATCHES_ALIGN) ?
			ops->nb_matches : REE_NUM_MATCHES_ALIGN);
		match = (uint64_t)ops + REE_MATCH_OFFSET;
		match += (ops->nb_matches - off) *
			sizeof(union ree_match);
		memcpy((void *)ops->matches, (void *)match,
			off * sizeof(union ree_match));
	}
}

static uint16_t
cn9k_ree_dequeue_burst(struct rte_regexdev *dev, uint16_t qp_id,
		       struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_qp *qp = data->queue_pairs[qp_id];
	struct roc_ree_pending_queue *pend_q;
	int i, nb_pending, nb_completed = 0;
	volatile struct ree_res_s_98 *res;
	struct roc_ree_rid *rid;

	pend_q = &qp->pend_q;

	nb_pending = pend_q->pending_count;

	if (nb_ops > nb_pending)
		nb_ops = nb_pending;

	for (i = 0; i < nb_ops; i++) {
		rid = &pend_q->rid_queue[pend_q->deq_head];
		res = (volatile struct ree_res_s_98 *)(rid->rid);

		/* Check response header done bit if completed */
		if (unlikely(!res->done))
			break;

		ops[i] = (struct rte_regex_ops *)(rid->rid);
		ops[i]->user_id = rid->user_id;

		REE_MOD_INC(pend_q->deq_head, REE_DEFAULT_CMD_QLEN);
		pend_q->pending_count -= 1;
	}

	nb_completed = i;

	for (i = 0; i < nb_completed; i++)
		ree_dequeue_post_process(ops[i]);

	return nb_completed;
}

static int
cn9k_ree_dev_info_get(struct rte_regexdev *dev, struct rte_regexdev_info *info)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;

	ree_func_trace();

	if (info == NULL)
		return -EINVAL;

	info->driver_name = dev->device->driver->name;
	info->dev = dev->device;

	info->max_queue_pairs = vf->max_queues;
	info->max_matches = vf->max_matches;
	info->max_payload_size = REE_MAX_PAYLOAD_SIZE;
	info->max_rules_per_group = data->max_rules_per_group;
	info->max_groups = data->max_groups;
	info->regexdev_capa = data->regexdev_capa;
	info->rule_flags = data->rule_flags;

	return 0;
}

static int
cn9k_ree_dev_config(struct rte_regexdev *dev,
		    const struct rte_regexdev_config *cfg)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;
	const struct ree_rule_db *rule_db;
	uint32_t rule_db_len;
	int ret;

	ree_func_trace();

	if (cfg->nb_queue_pairs > vf->max_queues) {
		cn9k_err("Invalid number of queue pairs requested");
		return -EINVAL;
	}

	if (cfg->nb_max_matches != vf->max_matches) {
		cn9k_err("Invalid number of max matches requested");
		return -EINVAL;
	}

	if (cfg->dev_cfg_flags != 0) {
		cn9k_err("Invalid device configuration flags requested");
		return -EINVAL;
	}

	/* Unregister error interrupts */
	if (vf->err_intr_registered)
		roc_ree_err_intr_unregister(vf);

	/* Detach queues */
	if (vf->nb_queues) {
		ret = roc_ree_queues_detach(vf);
		if (ret) {
			cn9k_err("Could not detach REE queues");
			return ret;
		}
	}

	/* TEMP : should be in lib */
	if (data->queue_pairs == NULL) { /* first time configuration */
		data->queue_pairs = rte_zmalloc("regexdev->queue_pairs",
				sizeof(data->queue_pairs[0]) *
				cfg->nb_queue_pairs, RTE_CACHE_LINE_SIZE);

		if (data->queue_pairs == NULL) {
			data->nb_queue_pairs = 0;
			cn9k_err("Failed to get memory for qp meta data, nb_queues %u",
					cfg->nb_queue_pairs);
			return -ENOMEM;
		}
	} else { /* re-configure */
		uint16_t old_nb_queues = data->nb_queue_pairs;
		void **qp;
		unsigned int i;

		qp = data->queue_pairs;

		for (i = cfg->nb_queue_pairs; i < old_nb_queues; i++) {
			ret = ree_queue_pair_release(dev, i);
			if (ret < 0)
				return ret;
		}

		qp = rte_realloc(qp, sizeof(qp[0]) * cfg->nb_queue_pairs,
				RTE_CACHE_LINE_SIZE);
		if (qp == NULL) {
			cn9k_err("Failed to realloc qp meta data, nb_queues %u",
					cfg->nb_queue_pairs);
			return -ENOMEM;
		}

		if (cfg->nb_queue_pairs > old_nb_queues) {
			uint16_t new_qs = cfg->nb_queue_pairs - old_nb_queues;
			memset(qp + old_nb_queues, 0, sizeof(qp[0]) * new_qs);
		}

		data->queue_pairs = qp;
	}
	data->nb_queue_pairs = cfg->nb_queue_pairs;

	/* Attach queues */
	cn9k_ree_dbg("Attach %d queues", cfg->nb_queue_pairs);
	ret = roc_ree_queues_attach(vf, cfg->nb_queue_pairs);
	if (ret) {
		cn9k_err("Could not attach queues");
		return -ENODEV;
	}

	ret = roc_ree_msix_offsets_get(vf);
	if (ret) {
		cn9k_err("Could not get MSI-X offsets");
		goto queues_detach;
	}

	if (cfg->rule_db && cfg->rule_db_len) {
		cn9k_ree_dbg("rule_db length %d", cfg->rule_db_len);
		rule_db = (const struct ree_rule_db *)cfg->rule_db;
		rule_db_len = rule_db->number_of_entries *
				sizeof(struct ree_rule_db_entry);
		cn9k_ree_dbg("rule_db number of entries %d",
				rule_db->number_of_entries);
		if (rule_db_len > cfg->rule_db_len) {
			cn9k_err("Could not program rule db");
			ret = -EINVAL;
			goto queues_detach;
		}
		ret = roc_ree_rule_db_prog(vf, (const char *)rule_db->entries,
				rule_db_len, NULL, REE_NON_INC_PROG);
		if (ret) {
			cn9k_err("Could not program rule db");
			goto queues_detach;
		}
	}

	dev->enqueue = cn9k_ree_enqueue_burst;
	dev->dequeue = cn9k_ree_dequeue_burst;

	rte_mb();
	return 0;

queues_detach:
	roc_ree_queues_detach(vf);
	return ret;
}

static int
cn9k_ree_stop(struct rte_regexdev *dev)
{
	RTE_SET_USED(dev);

	ree_func_trace();
	return 0;
}

static int
cn9k_ree_start(struct rte_regexdev *dev)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;
	uint32_t rule_db_len = 0;
	int ret;

	ree_func_trace();

	ret = roc_ree_rule_db_len_get(vf, &rule_db_len, NULL);
	if (ret)
		return ret;
	if (rule_db_len == 0) {
		cn9k_err("Rule db not programmed");
		return -EFAULT;
	}

	return 0;
}

static int
cn9k_ree_close(struct rte_regexdev *dev)
{
	return ree_dev_fini(dev);
}

static int
cn9k_ree_queue_pair_setup(struct rte_regexdev *dev, uint16_t qp_id,
		const struct rte_regexdev_qp_conf *qp_conf)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_qp *qp;

	ree_func_trace("Queue=%d", qp_id);

	if (data->queue_pairs[qp_id] != NULL)
		ree_queue_pair_release(dev, qp_id);

	if (qp_conf->nb_desc > REE_DEFAULT_CMD_QLEN) {
		cn9k_err("Could not setup queue pair for %u descriptors",
				qp_conf->nb_desc);
		return -EINVAL;
	}
	if (qp_conf->qp_conf_flags != 0) {
		cn9k_err("Could not setup queue pair with configuration flags 0x%x",
				qp_conf->qp_conf_flags);
		return -EINVAL;
	}

	qp = ree_qp_create(dev, qp_id);
	if (qp == NULL) {
		cn9k_err("Could not create queue pair %d", qp_id);
		return -ENOMEM;
	}
	data->queue_pairs[qp_id] = qp;

	return 0;
}

static int
cn9k_ree_rule_db_compile_activate(struct rte_regexdev *dev)
{
	return cn9k_ree_rule_db_compile_prog(dev);
}

static int
cn9k_ree_rule_db_update(struct rte_regexdev *dev,
		const struct rte_regexdev_rule *rules, uint16_t nb_rules)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct rte_regexdev_rule *old_ptr;
	uint32_t i, sum_nb_rules;

	ree_func_trace("nb_rules=%d", nb_rules);

	for (i = 0; i < nb_rules; i++) {
		if (rules[i].op == RTE_REGEX_RULE_OP_REMOVE)
			break;
		if (rules[i].group_id >= data->max_groups)
			break;
		if (rules[i].rule_id >= data->max_rules_per_group)
			break;
		/* logical implication
		 * p    q    p -> q
		 * 0    0      1
		 * 0    1      1
		 * 1    0      0
		 * 1    1      1
		 */
		if ((~(rules[i].rule_flags) | data->rule_flags) == 0)
			break;
	}
	nb_rules = i;

	if (data->nb_rules == 0) {

		data->rules = rte_malloc("rte_regexdev_rules",
				nb_rules*sizeof(struct rte_regexdev_rule), 0);
		if (data->rules == NULL)
			return -ENOMEM;

		memcpy(data->rules, rules,
				nb_rules*sizeof(struct rte_regexdev_rule));
		data->nb_rules = nb_rules;
	} else {

		old_ptr = data->rules;
		sum_nb_rules = data->nb_rules + nb_rules;
		data->rules = rte_realloc(data->rules,
				sum_nb_rules * sizeof(struct rte_regexdev_rule),
							0);
		if (data->rules == NULL) {
			data->rules = old_ptr;
			return -ENOMEM;
		}
		memcpy(&data->rules[data->nb_rules], rules,
				nb_rules*sizeof(struct rte_regexdev_rule));
		data->nb_rules = sum_nb_rules;
	}
	return nb_rules;
}

static int
cn9k_ree_rule_db_import(struct rte_regexdev *dev, const char *rule_db,
		uint32_t rule_db_len)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;
	const struct ree_rule_db *ree_rule_db;
	uint32_t ree_rule_db_len;
	int ret;

	ree_func_trace("rule_db_len=%d", rule_db_len);

	ree_rule_db = (const struct ree_rule_db *)rule_db;
	ree_rule_db_len = ree_rule_db->number_of_entries *
			sizeof(struct ree_rule_db_entry);
	if (ree_rule_db_len > rule_db_len) {
		cn9k_err("Could not program rule db");
		return -EINVAL;
	}
	ret = roc_ree_rule_db_prog(vf, (const char *)ree_rule_db->entries,
			ree_rule_db_len, NULL, REE_NON_INC_PROG);
	if (ret) {
		cn9k_err("Could not program rule db");
		return -ENOSPC;
	}
	return 0;
}

static int
cn9k_ree_rule_db_export(struct rte_regexdev *dev, char *rule_db)
{
	struct cn9k_ree_data *data = dev->data->dev_private;
	struct roc_ree_vf *vf = &data->vf;
	struct ree_rule_db *ree_rule_db;
	uint32_t rule_dbi_len;
	uint32_t rule_db_len;
	int ret;

	ree_func_trace();

	ret = roc_ree_rule_db_len_get(vf, &rule_db_len, &rule_dbi_len);
	if (ret)
		return ret;

	if (rule_db == NULL) {
		rule_db_len += sizeof(struct ree_rule_db);
		return rule_db_len;
	}

	ree_rule_db = (struct ree_rule_db *)rule_db;
	ret = roc_ree_rule_db_get(vf, (char *)ree_rule_db->entries,
			rule_db_len, NULL, 0);
	if (ret) {
		cn9k_err("Could not export rule db");
		return -EFAULT;
	}
	ree_rule_db->number_of_entries =
			rule_db_len/sizeof(struct ree_rule_db_entry);
	ree_rule_db->revision = REE_RULE_DB_REVISION;
	ree_rule_db->version = REE_RULE_DB_VERSION;

	return 0;
}

static struct rte_regexdev_ops cn9k_ree_ops = {
	.dev_info_get = cn9k_ree_dev_info_get,
	.dev_configure = cn9k_ree_dev_config,
	.dev_qp_setup = cn9k_ree_queue_pair_setup,
	.dev_start = cn9k_ree_start,
	.dev_stop = cn9k_ree_stop,
	.dev_close = cn9k_ree_close,
	.dev_attr_get = NULL,
	.dev_attr_set = NULL,
	.dev_rule_db_update = cn9k_ree_rule_db_update,
	.dev_rule_db_compile_activate =
			cn9k_ree_rule_db_compile_activate,
	.dev_db_import = cn9k_ree_rule_db_import,
	.dev_db_export = cn9k_ree_rule_db_export,
	.dev_xstats_names_get = NULL,
	.dev_xstats_get = NULL,
	.dev_xstats_by_name_get = NULL,
	.dev_xstats_reset = NULL,
	.dev_selftest = NULL,
	.dev_dump = NULL,
};

static int
cn9k_ree_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		   struct rte_pci_device *pci_dev)
{
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	struct cn9k_ree_data *data;
	struct rte_regexdev *dev;
	struct roc_ree_vf *vf;
	int ret;

	ret = roc_plt_init();
	if (ret < 0) {
		plt_err("Failed to initialize platform model");
		return ret;
	}

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = ree_dev_register(name);
	if (dev == NULL) {
		ret = -ENODEV;
		goto exit;
	}

	dev->dev_ops = &cn9k_ree_ops;
	dev->device = &pci_dev->device;

	/* Get private data space allocated */
	data = dev->data->dev_private;
	vf = &data->vf;
	vf->pci_dev = pci_dev;
	ret = roc_ree_dev_init(vf);
	if (ret) {
		plt_err("Failed to initialize roc cpt rc=%d", ret);
		goto dev_unregister;
	}

	data->rule_flags = RTE_REGEX_PCRE_RULE_ALLOW_EMPTY_F |
			RTE_REGEX_PCRE_RULE_ANCHORED_F;
	data->regexdev_capa = 0;
	data->max_groups = REE_MAX_GROUPS;
	data->max_rules_per_group = REE_MAX_RULES_PER_GROUP;
	data->nb_rules = 0;

	dev->state = RTE_REGEXDEV_READY;
	return 0;

dev_unregister:
	ree_dev_unregister(dev);
exit:
	cn9k_err("Could not create device (vendor_id: 0x%x device_id: 0x%x)",
		    pci_dev->id.vendor_id, pci_dev->id.device_id);
	return ret;
}

static int
cn9k_ree_pci_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	struct rte_regexdev *dev = NULL;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_regexdev_get_device_by_name(name);

	if (dev == NULL)
		return -ENODEV;

	return ree_dev_fini(dev);
}

static struct rte_pci_id pci_id_ree_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVID_CNXK_RVU_REE_PF)
	},
	{
		.vendor_id = 0,
	}
};

static struct rte_pci_driver cn9k_regexdev_pmd = {
	.id_table = pci_id_ree_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = cn9k_ree_pci_probe,
	.remove = cn9k_ree_pci_remove,
};


RTE_PMD_REGISTER_PCI(REGEXDEV_NAME_CN9K_PMD, cn9k_regexdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(REGEXDEV_NAME_CN9K_PMD, pci_id_ree_table);
