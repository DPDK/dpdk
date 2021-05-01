/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include "dlb2_user.h"

#include "dlb2_hw_types.h"
#include "dlb2_osdep.h"
#include "dlb2_osdep_bitmap.h"
#include "dlb2_osdep_types.h"
#include "dlb2_regs.h"
#include "dlb2_resource.h"

#include "../../dlb2_priv.h"
#include "../../dlb2_inline_fns.h"

#define DLB2_DOM_LIST_HEAD(head, type) \
	DLB2_LIST_HEAD((head), type, domain_list)

#define DLB2_FUNC_LIST_HEAD(head, type) \
	DLB2_LIST_HEAD((head), type, func_list)

#define DLB2_DOM_LIST_FOR(head, ptr, iter) \
	DLB2_LIST_FOR_EACH(head, ptr, domain_list, iter)

#define DLB2_FUNC_LIST_FOR(head, ptr, iter) \
	DLB2_LIST_FOR_EACH(head, ptr, func_list, iter)

#define DLB2_DOM_LIST_FOR_SAFE(head, ptr, ptr_tmp, it, it_tmp) \
	DLB2_LIST_FOR_EACH_SAFE((head), ptr, ptr_tmp, domain_list, it, it_tmp)

#define DLB2_FUNC_LIST_FOR_SAFE(head, ptr, ptr_tmp, it, it_tmp) \
	DLB2_LIST_FOR_EACH_SAFE((head), ptr, ptr_tmp, func_list, it, it_tmp)

void dlb2_hw_enable_sparse_dir_cq_mode(struct dlb2_hw *hw)
{
	union dlb2_chp_cfg_chp_csr_ctrl r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_CHP_CFG_CHP_CSR_CTRL);

	r0.field.cfg_64bytes_qe_dir_cq_mode = 1;

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_CHP_CSR_CTRL, r0.val);
}

void dlb2_hw_enable_sparse_ldb_cq_mode(struct dlb2_hw *hw)
{
	union dlb2_chp_cfg_chp_csr_ctrl r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_CHP_CFG_CHP_CSR_CTRL);

	r0.field.cfg_64bytes_qe_ldb_cq_mode = 1;

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_CHP_CSR_CTRL, r0.val);
}

/*
 * The PF driver cannot assume that a register write will affect subsequent HCW
 * writes. To ensure a write completes, the driver must read back a CSR. This
 * function only need be called for configuration that can occur after the
 * domain has started; prior to starting, applications can't send HCWs.
 */
static inline void dlb2_flush_csr(struct dlb2_hw *hw)
{
	DLB2_CSR_RD(hw, DLB2_SYS_TOTAL_VAS);
}

static u32 dlb2_dir_queue_depth(struct dlb2_hw *hw,
				struct dlb2_dir_pq_pair *queue)
{
	union dlb2_lsp_qid_dir_enqueue_cnt r0;

	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_DIR_ENQUEUE_CNT(queue->id.phys_id));

	return r0.field.count;
}

static void dlb2_ldb_port_cq_enable(struct dlb2_hw *hw,
				    struct dlb2_ldb_port *port)
{
	union dlb2_lsp_cq_ldb_dsbl reg;

	/*
	 * Don't re-enable the port if a removal is pending. The caller should
	 * mark this port as enabled (if it isn't already), and when the
	 * removal completes the port will be enabled.
	 */
	if (port->num_pending_removals)
		return;

	reg.field.disabled = 0;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_LDB_DSBL(port->id.phys_id), reg.val);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_cq_disable(struct dlb2_hw *hw,
				     struct dlb2_ldb_port *port)
{
	union dlb2_lsp_cq_ldb_dsbl reg;

	reg.field.disabled = 1;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_LDB_DSBL(port->id.phys_id), reg.val);

	dlb2_flush_csr(hw);
}

static u32 dlb2_ldb_queue_depth(struct dlb2_hw *hw,
				struct dlb2_ldb_queue *queue)
{
	union dlb2_lsp_qid_aqed_active_cnt r0;
	union dlb2_lsp_qid_atm_active r1;
	union dlb2_lsp_qid_ldb_enqueue_cnt r2;

	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_AQED_ACTIVE_CNT(queue->id.phys_id));
	r1.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_ATM_ACTIVE(queue->id.phys_id));

	r2.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_LDB_ENQUEUE_CNT(queue->id.phys_id));

	return r0.field.count + r1.field.count + r2.field.count;
}

static struct dlb2_ldb_queue *
dlb2_get_ldb_queue_from_id(struct dlb2_hw *hw,
			   u32 id,
			   bool vdev_req,
			   unsigned int vdev_id)
{
	struct dlb2_list_entry *iter1;
	struct dlb2_list_entry *iter2;
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	RTE_SET_USED(iter1);
	RTE_SET_USED(iter2);

	if (id >= DLB2_MAX_NUM_LDB_QUEUES)
		return NULL;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	if (!vdev_req)
		return &hw->rsrcs.ldb_queues[id];

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iter1) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter2)
			if (queue->id.virt_id == id)
				return queue;
	}

	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_queues, queue, iter1)
		if (queue->id.virt_id == id)
			return queue;

	return NULL;
}

static struct dlb2_hw_domain *dlb2_get_domain_from_id(struct dlb2_hw *hw,
						      u32 id,
						      bool vdev_req,
						      unsigned int vdev_id)
{
	struct dlb2_list_entry *iteration;
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;
	RTE_SET_USED(iteration);

	if (id >= DLB2_MAX_NUM_DOMAINS)
		return NULL;

	if (!vdev_req)
		return &hw->domains[id];

	rsrcs = &hw->vdev[vdev_id];

	DLB2_FUNC_LIST_FOR(rsrcs->used_domains, domain, iteration)
		if (domain->id.virt_id == id)
			return domain;

	return NULL;
}

static int dlb2_port_slot_state_transition(struct dlb2_hw *hw,
					   struct dlb2_ldb_port *port,
					   struct dlb2_ldb_queue *queue,
					   int slot,
					   enum dlb2_qid_map_state new_state)
{
	enum dlb2_qid_map_state curr_state = port->qid_map[slot].state;
	struct dlb2_hw_domain *domain;
	int domain_id;

	domain_id = port->domain_id.phys_id;

	domain = dlb2_get_domain_from_id(hw, domain_id, false, 0);
	if (domain == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: unable to find domain %d\n",
			    __func__, domain_id);
		return -EINVAL;
	}

	switch (curr_state) {
	case DLB2_QUEUE_UNMAPPED:
		switch (new_state) {
		case DLB2_QUEUE_MAPPED:
			queue->num_mappings++;
			port->num_mappings++;
			break;
		case DLB2_QUEUE_MAP_IN_PROG:
			queue->num_pending_additions++;
			domain->num_pending_additions++;
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_MAPPED:
		switch (new_state) {
		case DLB2_QUEUE_UNMAPPED:
			queue->num_mappings--;
			port->num_mappings--;
			break;
		case DLB2_QUEUE_UNMAP_IN_PROG:
			port->num_pending_removals++;
			domain->num_pending_removals++;
			break;
		case DLB2_QUEUE_MAPPED:
			/* Priority change, nothing to update */
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_MAP_IN_PROG:
		switch (new_state) {
		case DLB2_QUEUE_UNMAPPED:
			queue->num_pending_additions--;
			domain->num_pending_additions--;
			break;
		case DLB2_QUEUE_MAPPED:
			queue->num_mappings++;
			port->num_mappings++;
			queue->num_pending_additions--;
			domain->num_pending_additions--;
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_UNMAP_IN_PROG:
		switch (new_state) {
		case DLB2_QUEUE_UNMAPPED:
			port->num_pending_removals--;
			domain->num_pending_removals--;
			queue->num_mappings--;
			port->num_mappings--;
			break;
		case DLB2_QUEUE_MAPPED:
			port->num_pending_removals--;
			domain->num_pending_removals--;
			break;
		case DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP:
			/* Nothing to update */
			break;
		default:
			goto error;
		}
		break;
	case DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP:
		switch (new_state) {
		case DLB2_QUEUE_UNMAP_IN_PROG:
			/* Nothing to update */
			break;
		case DLB2_QUEUE_UNMAPPED:
			/*
			 * An UNMAP_IN_PROG_PENDING_MAP slot briefly
			 * becomes UNMAPPED before it transitions to
			 * MAP_IN_PROG.
			 */
			queue->num_mappings--;
			port->num_mappings--;
			port->num_pending_removals--;
			domain->num_pending_removals--;
			break;
		default:
			goto error;
		}
		break;
	default:
		goto error;
	}

	port->qid_map[slot].state = new_state;

	DLB2_HW_DBG(hw,
		    "[%s()] queue %d -> port %d state transition (%d -> %d)\n",
		    __func__, queue->id.phys_id, port->id.phys_id,
		    curr_state, new_state);
	return 0;

error:
	DLB2_HW_ERR(hw,
		    "[%s()] Internal error: invalid queue %d -> port %d state transition (%d -> %d)\n",
		    __func__, queue->id.phys_id, port->id.phys_id,
		    curr_state, new_state);
	return -EFAULT;
}

static bool dlb2_port_find_slot(struct dlb2_ldb_port *port,
				enum dlb2_qid_map_state state,
				int *slot)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (port->qid_map[i].state == state)
			break;
	}

	*slot = i;

	return (i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ);
}

static bool dlb2_port_find_slot_queue(struct dlb2_ldb_port *port,
				      enum dlb2_qid_map_state state,
				      struct dlb2_ldb_queue *queue,
				      int *slot)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (port->qid_map[i].state == state &&
		    port->qid_map[i].qid == queue->id.phys_id)
			break;
	}

	*slot = i;

	return (i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ);
}

/*
 * dlb2_ldb_queue_{enable, disable}_mapped_cqs() don't operate exactly as
 * their function names imply, and should only be called by the dynamic CQ
 * mapping code.
 */
static void dlb2_ldb_queue_disable_mapped_cqs(struct dlb2_hw *hw,
					      struct dlb2_hw_domain *domain,
					      struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int slot, i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			enum dlb2_qid_map_state state = DLB2_QUEUE_MAPPED;

			if (!dlb2_port_find_slot_queue(port, state,
						       queue, &slot))
				continue;

			if (port->enabled)
				dlb2_ldb_port_cq_disable(hw, port);
		}
	}
}

static void dlb2_ldb_queue_enable_mapped_cqs(struct dlb2_hw *hw,
					     struct dlb2_hw_domain *domain,
					     struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int slot, i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			enum dlb2_qid_map_state state = DLB2_QUEUE_MAPPED;

			if (!dlb2_port_find_slot_queue(port, state,
						       queue, &slot))
				continue;

			if (port->enabled)
				dlb2_ldb_port_cq_enable(hw, port);
		}
	}
}

static void dlb2_ldb_port_clear_queue_if_status(struct dlb2_hw *hw,
						struct dlb2_ldb_port *port,
						int slot)
{
	union dlb2_lsp_ldb_sched_ctrl r0 = { {0} };

	r0.field.cq = port->id.phys_id;
	r0.field.qidix = slot;
	r0.field.value = 0;
	r0.field.inflight_ok_v = 1;

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL, r0.val);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_port_set_queue_if_status(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      int slot)
{
	union dlb2_lsp_ldb_sched_ctrl r0 = { {0} };

	r0.field.cq = port->id.phys_id;
	r0.field.qidix = slot;
	r0.field.value = 1;
	r0.field.inflight_ok_v = 1;

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL, r0.val);

	dlb2_flush_csr(hw);
}

static int dlb2_ldb_port_map_qid_static(struct dlb2_hw *hw,
					struct dlb2_ldb_port *p,
					struct dlb2_ldb_queue *q,
					u8 priority)
{
	union dlb2_lsp_cq2priov r0;
	union dlb2_lsp_cq2qid0 r1;
	union dlb2_atm_qid2cqidix_00 r2;
	union dlb2_lsp_qid2cqidix_00 r3;
	union dlb2_lsp_qid2cqidix2_00 r4;
	enum dlb2_qid_map_state state;
	int i;

	/* Look for a pending or already mapped slot, else an unused slot */
	if (!dlb2_port_find_slot_queue(p, DLB2_QUEUE_MAP_IN_PROG, q, &i) &&
	    !dlb2_port_find_slot_queue(p, DLB2_QUEUE_MAPPED, q, &i) &&
	    !dlb2_port_find_slot(p, DLB2_QUEUE_UNMAPPED, &i)) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: CQ has no available QID mapping slots\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port slot tracking failed\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	/* Read-modify-write the priority and valid bit register */
	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ2PRIOV(p->id.phys_id));

	r0.field.v |= 1 << i;
	r0.field.prio |= (priority & 0x7) << i * 3;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(p->id.phys_id), r0.val);

	/* Read-modify-write the QID map register */
	if (i < 4)
		r1.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ2QID0(p->id.phys_id));
	else
		r1.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ2QID1(p->id.phys_id));

	if (i == 0 || i == 4)
		r1.field.qid_p0 = q->id.phys_id;
	if (i == 1 || i == 5)
		r1.field.qid_p1 = q->id.phys_id;
	if (i == 2 || i == 6)
		r1.field.qid_p2 = q->id.phys_id;
	if (i == 3 || i == 7)
		r1.field.qid_p3 = q->id.phys_id;

	if (i < 4)
		DLB2_CSR_WR(hw, DLB2_LSP_CQ2QID0(p->id.phys_id), r1.val);
	else
		DLB2_CSR_WR(hw, DLB2_LSP_CQ2QID1(p->id.phys_id), r1.val);

	r2.val = DLB2_CSR_RD(hw,
			     DLB2_ATM_QID2CQIDIX(q->id.phys_id,
						 p->id.phys_id / 4));

	r3.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID2CQIDIX(q->id.phys_id,
						 p->id.phys_id / 4));

	r4.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID2CQIDIX2(q->id.phys_id,
						  p->id.phys_id / 4));

	switch (p->id.phys_id % 4) {
	case 0:
		r2.field.cq_p0 |= 1 << i;
		r3.field.cq_p0 |= 1 << i;
		r4.field.cq_p0 |= 1 << i;
		break;

	case 1:
		r2.field.cq_p1 |= 1 << i;
		r3.field.cq_p1 |= 1 << i;
		r4.field.cq_p1 |= 1 << i;
		break;

	case 2:
		r2.field.cq_p2 |= 1 << i;
		r3.field.cq_p2 |= 1 << i;
		r4.field.cq_p2 |= 1 << i;
		break;

	case 3:
		r2.field.cq_p3 |= 1 << i;
		r3.field.cq_p3 |= 1 << i;
		r4.field.cq_p3 |= 1 << i;
		break;
	}

	DLB2_CSR_WR(hw,
		    DLB2_ATM_QID2CQIDIX(q->id.phys_id, p->id.phys_id / 4),
		    r2.val);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID2CQIDIX(q->id.phys_id, p->id.phys_id / 4),
		    r3.val);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID2CQIDIX2(q->id.phys_id, p->id.phys_id / 4),
		    r4.val);

	dlb2_flush_csr(hw);

	p->qid_map[i].qid = q->id.phys_id;
	p->qid_map[i].priority = priority;

	state = DLB2_QUEUE_MAPPED;

	return dlb2_port_slot_state_transition(hw, p, q, i, state);
}

static int dlb2_ldb_port_set_has_work_bits(struct dlb2_hw *hw,
					   struct dlb2_ldb_port *port,
					   struct dlb2_ldb_queue *queue,
					   int slot)
{
	union dlb2_lsp_qid_aqed_active_cnt r0;
	union dlb2_lsp_qid_ldb_enqueue_cnt r1;
	union dlb2_lsp_ldb_sched_ctrl r2 = { {0} };

	/* Set the atomic scheduling haswork bit */
	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_AQED_ACTIVE_CNT(queue->id.phys_id));

	r2.field.cq = port->id.phys_id;
	r2.field.qidix = slot;
	r2.field.value = 1;
	r2.field.rlist_haswork_v = r0.field.count > 0;

	/* Set the non-atomic scheduling haswork bit */
	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL, r2.val);

	r1.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_LDB_ENQUEUE_CNT(queue->id.phys_id));

	memset(&r2, 0, sizeof(r2));

	r2.field.cq = port->id.phys_id;
	r2.field.qidix = slot;
	r2.field.value = 1;
	r2.field.nalb_haswork_v = (r1.field.count > 0);

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL, r2.val);

	dlb2_flush_csr(hw);

	return 0;
}

static void dlb2_ldb_port_clear_has_work_bits(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      u8 slot)
{
	union dlb2_lsp_ldb_sched_ctrl r2 = { {0} };

	r2.field.cq = port->id.phys_id;
	r2.field.qidix = slot;
	r2.field.value = 0;
	r2.field.rlist_haswork_v = 1;

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL, r2.val);

	memset(&r2, 0, sizeof(r2));

	r2.field.cq = port->id.phys_id;
	r2.field.qidix = slot;
	r2.field.value = 0;
	r2.field.nalb_haswork_v = 1;

	DLB2_CSR_WR(hw, DLB2_LSP_LDB_SCHED_CTRL, r2.val);

	dlb2_flush_csr(hw);
}

static void dlb2_ldb_queue_set_inflight_limit(struct dlb2_hw *hw,
					      struct dlb2_ldb_queue *queue)
{
	union dlb2_lsp_qid_ldb_infl_lim r0 = { {0} };

	r0.field.limit = queue->num_qid_inflights;

	DLB2_CSR_WR(hw, DLB2_LSP_QID_LDB_INFL_LIM(queue->id.phys_id), r0.val);
}

static void dlb2_ldb_queue_clear_inflight_limit(struct dlb2_hw *hw,
						struct dlb2_ldb_queue *queue)
{
	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_LDB_INFL_LIM(queue->id.phys_id),
		    DLB2_LSP_QID_LDB_INFL_LIM_RST);
}

static int dlb2_ldb_port_finish_map_qid_dynamic(struct dlb2_hw *hw,
						struct dlb2_hw_domain *domain,
						struct dlb2_ldb_port *port,
						struct dlb2_ldb_queue *queue)
{
	struct dlb2_list_entry *iter;
	union dlb2_lsp_qid_ldb_infl_cnt r0;
	enum dlb2_qid_map_state state;
	int slot, ret, i;
	u8 prio;
	RTE_SET_USED(iter);

	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_LDB_INFL_CNT(queue->id.phys_id));

	if (r0.field.count) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: non-zero QID inflight count\n",
			    __func__);
		return -EINVAL;
	}

	/*
	 * Static map the port and set its corresponding has_work bits.
	 */
	state = DLB2_QUEUE_MAP_IN_PROG;
	if (!dlb2_port_find_slot_queue(port, state, queue, &slot))
		return -EINVAL;

	if (slot >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port slot tracking failed\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	prio = port->qid_map[slot].priority;

	/*
	 * Update the CQ2QID, CQ2PRIOV, and QID2CQIDX registers, and
	 * the port's qid_map state.
	 */
	ret = dlb2_ldb_port_map_qid_static(hw, port, queue, prio);
	if (ret)
		return ret;

	ret = dlb2_ldb_port_set_has_work_bits(hw, port, queue, slot);
	if (ret)
		return ret;

	/*
	 * Ensure IF_status(cq,qid) is 0 before enabling the port to
	 * prevent spurious schedules to cause the queue's inflight
	 * count to increase.
	 */
	dlb2_ldb_port_clear_queue_if_status(hw, port, slot);

	/* Reset the queue's inflight status */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			state = DLB2_QUEUE_MAPPED;
			if (!dlb2_port_find_slot_queue(port, state,
						       queue, &slot))
				continue;

			dlb2_ldb_port_set_queue_if_status(hw, port, slot);
		}
	}

	dlb2_ldb_queue_set_inflight_limit(hw, queue);

	/* Re-enable CQs mapped to this queue */
	dlb2_ldb_queue_enable_mapped_cqs(hw, domain, queue);

	/* If this queue has other mappings pending, clear its inflight limit */
	if (queue->num_pending_additions > 0)
		dlb2_ldb_queue_clear_inflight_limit(hw, queue);

	return 0;
}

/**
 * dlb2_ldb_port_map_qid_dynamic() - perform a "dynamic" QID->CQ mapping
 * @hw: dlb2_hw handle for a particular device.
 * @port: load-balanced port
 * @queue: load-balanced queue
 * @priority: queue servicing priority
 *
 * Returns 0 if the queue was mapped, 1 if the mapping is scheduled to occur
 * at a later point, and <0 if an error occurred.
 */
static int dlb2_ldb_port_map_qid_dynamic(struct dlb2_hw *hw,
					 struct dlb2_ldb_port *port,
					 struct dlb2_ldb_queue *queue,
					 u8 priority)
{
	union dlb2_lsp_qid_ldb_infl_cnt r0 = { {0} };
	enum dlb2_qid_map_state state;
	struct dlb2_hw_domain *domain;
	int domain_id, slot, ret;

	domain_id = port->domain_id.phys_id;

	domain = dlb2_get_domain_from_id(hw, domain_id, false, 0);
	if (domain == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: unable to find domain %d\n",
			    __func__, port->domain_id.phys_id);
		return -EINVAL;
	}

	/*
	 * Set the QID inflight limit to 0 to prevent further scheduling of the
	 * queue.
	 */
	DLB2_CSR_WR(hw, DLB2_LSP_QID_LDB_INFL_LIM(queue->id.phys_id), 0);

	if (!dlb2_port_find_slot(port, DLB2_QUEUE_UNMAPPED, &slot)) {
		DLB2_HW_ERR(hw,
			    "Internal error: No available unmapped slots\n");
		return -EFAULT;
	}

	if (slot >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port slot tracking failed\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	port->qid_map[slot].qid = queue->id.phys_id;
	port->qid_map[slot].priority = priority;

	state = DLB2_QUEUE_MAP_IN_PROG;
	ret = dlb2_port_slot_state_transition(hw, port, queue, slot, state);
	if (ret)
		return ret;

	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_LDB_INFL_CNT(queue->id.phys_id));

	if (r0.field.count) {
		/*
		 * The queue is owed completions so it's not safe to map it
		 * yet. Schedule a kernel thread to complete the mapping later,
		 * once software has completed all the queue's inflight events.
		 */
		if (!os_worker_active(hw))
			os_schedule_work(hw);

		return 1;
	}

	/*
	 * Disable the affected CQ, and the CQs already mapped to the QID,
	 * before reading the QID's inflight count a second time. There is an
	 * unlikely race in which the QID may schedule one more QE after we
	 * read an inflight count of 0, and disabling the CQs guarantees that
	 * the race will not occur after a re-read of the inflight count
	 * register.
	 */
	if (port->enabled)
		dlb2_ldb_port_cq_disable(hw, port);

	dlb2_ldb_queue_disable_mapped_cqs(hw, domain, queue);

	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_LDB_INFL_CNT(queue->id.phys_id));

	if (r0.field.count) {
		if (port->enabled)
			dlb2_ldb_port_cq_enable(hw, port);

		dlb2_ldb_queue_enable_mapped_cqs(hw, domain, queue);

		/*
		 * The queue is owed completions so it's not safe to map it
		 * yet. Schedule a kernel thread to complete the mapping later,
		 * once software has completed all the queue's inflight events.
		 */
		if (!os_worker_active(hw))
			os_schedule_work(hw);

		return 1;
	}

	return dlb2_ldb_port_finish_map_qid_dynamic(hw, domain, port, queue);
}

static void dlb2_domain_finish_map_port(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain,
					struct dlb2_ldb_port *port)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		union dlb2_lsp_qid_ldb_infl_cnt r0;
		struct dlb2_ldb_queue *queue;
		int qid;

		if (port->qid_map[i].state != DLB2_QUEUE_MAP_IN_PROG)
			continue;

		qid = port->qid_map[i].qid;

		queue = dlb2_get_ldb_queue_from_id(hw, qid, false, 0);

		if (queue == NULL) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: unable to find queue %d\n",
				    __func__, qid);
			continue;
		}

		r0.val = DLB2_CSR_RD(hw, DLB2_LSP_QID_LDB_INFL_CNT(qid));

		if (r0.field.count)
			continue;

		/*
		 * Disable the affected CQ, and the CQs already mapped to the
		 * QID, before reading the QID's inflight count a second time.
		 * There is an unlikely race in which the QID may schedule one
		 * more QE after we read an inflight count of 0, and disabling
		 * the CQs guarantees that the race will not occur after a
		 * re-read of the inflight count register.
		 */
		if (port->enabled)
			dlb2_ldb_port_cq_disable(hw, port);

		dlb2_ldb_queue_disable_mapped_cqs(hw, domain, queue);

		r0.val = DLB2_CSR_RD(hw, DLB2_LSP_QID_LDB_INFL_CNT(qid));

		if (r0.field.count) {
			if (port->enabled)
				dlb2_ldb_port_cq_enable(hw, port);

			dlb2_ldb_queue_enable_mapped_cqs(hw, domain, queue);

			continue;
		}

		dlb2_ldb_port_finish_map_qid_dynamic(hw, domain, port, queue);
	}
}

static unsigned int
dlb2_domain_finish_map_qid_procedures(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	if (!domain->configured || domain->num_pending_additions == 0)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			dlb2_domain_finish_map_port(hw, domain, port);
	}

	return domain->num_pending_additions;
}

static int dlb2_ldb_port_unmap_qid(struct dlb2_hw *hw,
				   struct dlb2_ldb_port *port,
				   struct dlb2_ldb_queue *queue)
{
	enum dlb2_qid_map_state mapped, in_progress, pending_map, unmapped;
	union dlb2_lsp_cq2priov r0;
	union dlb2_atm_qid2cqidix_00 r1;
	union dlb2_lsp_qid2cqidix_00 r2;
	union dlb2_lsp_qid2cqidix2_00 r3;
	u32 queue_id;
	u32 port_id;
	int i;

	/* Find the queue's slot */
	mapped = DLB2_QUEUE_MAPPED;
	in_progress = DLB2_QUEUE_UNMAP_IN_PROG;
	pending_map = DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP;

	if (!dlb2_port_find_slot_queue(port, mapped, queue, &i) &&
	    !dlb2_port_find_slot_queue(port, in_progress, queue, &i) &&
	    !dlb2_port_find_slot_queue(port, pending_map, queue, &i)) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: QID %d isn't mapped\n",
			    __func__, __LINE__, queue->id.phys_id);
		return -EFAULT;
	}

	if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port slot tracking failed\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	port_id = port->id.phys_id;
	queue_id = queue->id.phys_id;

	/* Read-modify-write the priority and valid bit register */
	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ2PRIOV(port_id));

	r0.field.v &= ~(1 << i);

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(port_id), r0.val);

	r1.val = DLB2_CSR_RD(hw,
			     DLB2_ATM_QID2CQIDIX(queue_id, port_id / 4));

	r2.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID2CQIDIX(queue_id, port_id / 4));

	r3.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID2CQIDIX2(queue_id, port_id / 4));

	switch (port_id % 4) {
	case 0:
		r1.field.cq_p0 &= ~(1 << i);
		r2.field.cq_p0 &= ~(1 << i);
		r3.field.cq_p0 &= ~(1 << i);
		break;

	case 1:
		r1.field.cq_p1 &= ~(1 << i);
		r2.field.cq_p1 &= ~(1 << i);
		r3.field.cq_p1 &= ~(1 << i);
		break;

	case 2:
		r1.field.cq_p2 &= ~(1 << i);
		r2.field.cq_p2 &= ~(1 << i);
		r3.field.cq_p2 &= ~(1 << i);
		break;

	case 3:
		r1.field.cq_p3 &= ~(1 << i);
		r2.field.cq_p3 &= ~(1 << i);
		r3.field.cq_p3 &= ~(1 << i);
		break;
	}

	DLB2_CSR_WR(hw,
		    DLB2_ATM_QID2CQIDIX(queue_id, port_id / 4),
		    r1.val);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID2CQIDIX(queue_id, port_id / 4),
		    r2.val);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID2CQIDIX2(queue_id, port_id / 4),
		    r3.val);

	dlb2_flush_csr(hw);

	unmapped = DLB2_QUEUE_UNMAPPED;

	return dlb2_port_slot_state_transition(hw, port, queue, i, unmapped);
}

static int dlb2_ldb_port_map_qid(struct dlb2_hw *hw,
				 struct dlb2_hw_domain *domain,
				 struct dlb2_ldb_port *port,
				 struct dlb2_ldb_queue *queue,
				 u8 prio)
{
	if (domain->started)
		return dlb2_ldb_port_map_qid_dynamic(hw, port, queue, prio);
	else
		return dlb2_ldb_port_map_qid_static(hw, port, queue, prio);
}

static void
dlb2_domain_finish_unmap_port_slot(struct dlb2_hw *hw,
				   struct dlb2_hw_domain *domain,
				   struct dlb2_ldb_port *port,
				   int slot)
{
	enum dlb2_qid_map_state state;
	struct dlb2_ldb_queue *queue;

	queue = &hw->rsrcs.ldb_queues[port->qid_map[slot].qid];

	state = port->qid_map[slot].state;

	/* Update the QID2CQIDX and CQ2QID vectors */
	dlb2_ldb_port_unmap_qid(hw, port, queue);

	/*
	 * Ensure the QID will not be serviced by this {CQ, slot} by clearing
	 * the has_work bits
	 */
	dlb2_ldb_port_clear_has_work_bits(hw, port, slot);

	/* Reset the {CQ, slot} to its default state */
	dlb2_ldb_port_set_queue_if_status(hw, port, slot);

	/* Re-enable the CQ if it wasn't manually disabled by the user */
	if (port->enabled)
		dlb2_ldb_port_cq_enable(hw, port);

	/*
	 * If there is a mapping that is pending this slot's removal, perform
	 * the mapping now.
	 */
	if (state == DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP) {
		struct dlb2_ldb_port_qid_map *map;
		struct dlb2_ldb_queue *map_queue;
		u8 prio;

		map = &port->qid_map[slot];

		map->qid = map->pending_qid;
		map->priority = map->pending_priority;

		map_queue = &hw->rsrcs.ldb_queues[map->qid];
		prio = map->priority;

		dlb2_ldb_port_map_qid(hw, domain, port, map_queue, prio);
	}
}

static bool dlb2_domain_finish_unmap_port(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain,
					  struct dlb2_ldb_port *port)
{
	union dlb2_lsp_cq_ldb_infl_cnt r0;
	int i;

	if (port->num_pending_removals == 0)
		return false;

	/*
	 * The unmap requires all the CQ's outstanding inflights to be
	 * completed.
	 */
	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ_LDB_INFL_CNT(port->id.phys_id));
	if (r0.field.count > 0)
		return false;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		struct dlb2_ldb_port_qid_map *map;

		map = &port->qid_map[i];

		if (map->state != DLB2_QUEUE_UNMAP_IN_PROG &&
		    map->state != DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP)
			continue;

		dlb2_domain_finish_unmap_port_slot(hw, domain, port, i);
	}

	return true;
}

static unsigned int
dlb2_domain_finish_unmap_qid_procedures(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	if (!domain->configured || domain->num_pending_removals == 0)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			dlb2_domain_finish_unmap_port(hw, domain, port);
	}

	return domain->num_pending_removals;
}

unsigned int dlb2_finish_unmap_qid_procedures(struct dlb2_hw *hw)
{
	int i, num = 0;

	/* Finish queue unmap jobs for any domain that needs it */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		struct dlb2_hw_domain *domain = &hw->domains[i];

		num += dlb2_domain_finish_unmap_qid_procedures(hw, domain);
	}

	return num;
}

unsigned int dlb2_finish_map_qid_procedures(struct dlb2_hw *hw)
{
	int i, num = 0;

	/* Finish queue map jobs for any domain that needs it */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		struct dlb2_hw_domain *domain = &hw->domains[i];

		num += dlb2_domain_finish_map_qid_procedures(hw, domain);
	}

	return num;
}

int dlb2_get_group_sequence_numbers(struct dlb2_hw *hw, unsigned int group_id)
{
	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	return hw->rsrcs.sn_groups[group_id].sequence_numbers_per_queue;
}

int dlb2_get_group_sequence_number_occupancy(struct dlb2_hw *hw,
					     unsigned int group_id)
{
	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	return dlb2_sn_group_used_slots(&hw->rsrcs.sn_groups[group_id]);
}

static void dlb2_log_set_group_sequence_numbers(struct dlb2_hw *hw,
						unsigned int group_id,
						unsigned long val)
{
	DLB2_HW_DBG(hw, "DLB2 set group sequence numbers:\n");
	DLB2_HW_DBG(hw, "\tGroup ID: %u\n", group_id);
	DLB2_HW_DBG(hw, "\tValue:    %lu\n", val);
}

int dlb2_set_group_sequence_numbers(struct dlb2_hw *hw,
				    unsigned int group_id,
				    unsigned long val)
{
	u32 valid_allocations[] = {64, 128, 256, 512, 1024};
	union dlb2_ro_pipe_grp_sn_mode r0 = { {0} };
	struct dlb2_sn_group *group;
	int mode;

	if (group_id >= DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS)
		return -EINVAL;

	group = &hw->rsrcs.sn_groups[group_id];

	/*
	 * Once the first load-balanced queue using an SN group is configured,
	 * the group cannot be changed.
	 */
	if (group->slot_use_bitmap != 0)
		return -EPERM;

	for (mode = 0; mode < DLB2_MAX_NUM_SEQUENCE_NUMBER_MODES; mode++)
		if (val == valid_allocations[mode])
			break;

	if (mode == DLB2_MAX_NUM_SEQUENCE_NUMBER_MODES)
		return -EINVAL;

	group->mode = mode;
	group->sequence_numbers_per_queue = val;

	r0.field.sn_mode_0 = hw->rsrcs.sn_groups[0].mode;
	r0.field.sn_mode_1 = hw->rsrcs.sn_groups[1].mode;

	DLB2_CSR_WR(hw, DLB2_RO_PIPE_GRP_SN_MODE, r0.val);

	dlb2_log_set_group_sequence_numbers(hw, group_id, val);

	return 0;
}

static struct dlb2_dir_pq_pair *
dlb2_get_domain_used_dir_pq(struct dlb2_hw *hw,
			    u32 id,
			    bool vdev_req,
			    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	RTE_SET_USED(iter);

	if (id >= DLB2_MAX_NUM_DIR_PORTS(hw->ver))
		return NULL;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter)
		if ((!vdev_req && port->id.phys_id == id) ||
		    (vdev_req && port->id.virt_id == id))
			return port;

	return NULL;
}

static struct dlb2_ldb_queue *
dlb2_get_domain_ldb_queue(u32 id,
			  bool vdev_req,
			  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_queue *queue;
	RTE_SET_USED(iter);

	if (id >= DLB2_MAX_NUM_LDB_QUEUES)
		return NULL;

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter)
		if ((!vdev_req && queue->id.phys_id == id) ||
		    (vdev_req && queue->id.virt_id == id))
			return queue;

	return NULL;
}

static int dlb2_verify_start_domain_args(struct dlb2_hw *hw,
					 u32 domain_id,
					 struct dlb2_cmd_response *resp,
					 bool vdev_req,
					 unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (domain == NULL) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	if (domain->started) {
		resp->status = DLB2_ST_DOMAIN_STARTED;
		return -EINVAL;
	}

	return 0;
}

static void dlb2_log_start_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 start domain arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
}

/**
 * dlb2_hw_start_domain() - Lock the domain configuration
 * @hw:	Contains the current state of the DLB2 hardware.
 * @domain_id: Domain ID
 * @arg: User-provided arguments (unused, here for ioctl callback template).
 * @resp: Response to user.
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Return: returns < 0 on error, 0 otherwise. If the driver is unable to
 * satisfy a request, resp->status will be set accordingly.
 */
int
dlb2_hw_start_domain(struct dlb2_hw *hw,
		     u32 domain_id,
		     struct dlb2_start_domain_args *arg,
		     struct dlb2_cmd_response *resp,
		     bool vdev_req,
		     unsigned int vdev_id)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *dir_queue;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_hw_domain *domain;
	int ret;
	RTE_SET_USED(arg);
	RTE_SET_USED(iter);

	dlb2_log_start_domain(hw, domain_id, vdev_req, vdev_id);

	ret = dlb2_verify_start_domain_args(hw,
					    domain_id,
					    resp,
					    vdev_req,
					    vdev_id);
	if (ret)
		return ret;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (domain == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: domain not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	/*
	 * Enable load-balanced and directed queue write permissions for the
	 * queues this domain owns. Without this, the DLB2 will drop all
	 * incoming traffic to those queues.
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, ldb_queue, iter) {
		union dlb2_sys_ldb_vasqid_v r0 = { {0} };
		unsigned int offs;

		r0.field.vasqid_v = 1;

		offs = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES +
			ldb_queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_LDB_VASQID_V(offs), r0.val);
	}

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_queue, iter) {
		union dlb2_sys_dir_vasqid_v r0 = { {0} };
		unsigned int offs;

		r0.field.vasqid_v = 1;

		offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_PORTS(hw->ver) +
			dir_queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_DIR_VASQID_V(offs), r0.val);
	}

	dlb2_flush_csr(hw);

	domain->started = true;

	resp->status = 0;

	return 0;
}

static void dlb2_log_get_dir_queue_depth(struct dlb2_hw *hw,
					 u32 domain_id,
					 u32 queue_id,
					 bool vdev_req,
					 unsigned int vf_id)
{
	DLB2_HW_DBG(hw, "DLB get directed queue depth:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from VF %d)\n", vf_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tQueue ID: %d\n", queue_id);
}

int dlb2_hw_get_dir_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_dir_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *queue;
	struct dlb2_hw_domain *domain;
	int id;

	id = domain_id;

	dlb2_log_get_dir_queue_depth(hw, domain_id, args->queue_id,
				     vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, id, vdev_req, vdev_id);
	if (domain == NULL) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	id = args->queue_id;

	queue = dlb2_get_domain_used_dir_pq(hw, id, vdev_req, domain);
	if (queue == NULL) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	resp->id = dlb2_dir_queue_depth(hw, queue);

	return 0;
}

static void dlb2_log_get_ldb_queue_depth(struct dlb2_hw *hw,
					 u32 domain_id,
					 u32 queue_id,
					 bool vdev_req,
					 unsigned int vf_id)
{
	DLB2_HW_DBG(hw, "DLB get load-balanced queue depth:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from VF %d)\n", vf_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tQueue ID: %d\n", queue_id);
}

int dlb2_hw_get_ldb_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_ldb_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;

	dlb2_log_get_ldb_queue_depth(hw, domain_id, args->queue_id,
				     vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (domain == NULL) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->queue_id, vdev_req, domain);
	if (queue == NULL) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	resp->id = dlb2_ldb_queue_depth(hw, queue);

	return 0;
}
