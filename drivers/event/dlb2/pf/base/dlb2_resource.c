/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include "dlb2_user.h"

#include "dlb2_hw_types.h"
#include "dlb2_mbox.h"
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

static void dlb2_init_domain_rsrc_lists(struct dlb2_hw_domain *domain)
{
	int i;

	dlb2_list_init_head(&domain->used_ldb_queues);
	dlb2_list_init_head(&domain->used_dir_pq_pairs);
	dlb2_list_init_head(&domain->avail_ldb_queues);
	dlb2_list_init_head(&domain->avail_dir_pq_pairs);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		dlb2_list_init_head(&domain->used_ldb_ports[i]);
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		dlb2_list_init_head(&domain->avail_ldb_ports[i]);
}

static void dlb2_init_fn_rsrc_lists(struct dlb2_function_resources *rsrc)
{
	int i;

	dlb2_list_init_head(&rsrc->avail_domains);
	dlb2_list_init_head(&rsrc->used_domains);
	dlb2_list_init_head(&rsrc->avail_ldb_queues);
	dlb2_list_init_head(&rsrc->avail_dir_pq_pairs);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		dlb2_list_init_head(&rsrc->avail_ldb_ports[i]);
}

void dlb2_hw_enable_sparse_dir_cq_mode(struct dlb2_hw *hw)
{
	union dlb2_chp_cfg_chp_csr_ctrl r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_CHP_CFG_CHP_CSR_CTRL);

	r0.field.cfg_64bytes_qe_dir_cq_mode = 1;

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_CHP_CSR_CTRL, r0.val);
}

int dlb2_hw_get_num_resources(struct dlb2_hw *hw,
			      struct dlb2_get_num_resources_args *arg,
			      bool vdev_req,
			      unsigned int vdev_id)
{
	struct dlb2_function_resources *rsrcs;
	struct dlb2_bitmap *map;
	int i;

	if (vdev_req && vdev_id >= DLB2_MAX_NUM_VDEVS)
		return -EINVAL;

	if (vdev_req)
		rsrcs = &hw->vdev[vdev_id];
	else
		rsrcs = &hw->pf;

	arg->num_sched_domains = rsrcs->num_avail_domains;

	arg->num_ldb_queues = rsrcs->num_avail_ldb_queues;

	arg->num_ldb_ports = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		arg->num_ldb_ports += rsrcs->num_avail_ldb_ports[i];

	arg->num_cos_ldb_ports[0] = rsrcs->num_avail_ldb_ports[0];
	arg->num_cos_ldb_ports[1] = rsrcs->num_avail_ldb_ports[1];
	arg->num_cos_ldb_ports[2] = rsrcs->num_avail_ldb_ports[2];
	arg->num_cos_ldb_ports[3] = rsrcs->num_avail_ldb_ports[3];

	arg->num_dir_ports = rsrcs->num_avail_dir_pq_pairs;

	arg->num_atomic_inflights = rsrcs->num_avail_aqed_entries;

	map = rsrcs->avail_hist_list_entries;

	arg->num_hist_list_entries = dlb2_bitmap_count(map);

	arg->max_contiguous_hist_list_entries =
		dlb2_bitmap_longest_set_range(map);

	arg->num_ldb_credits = rsrcs->num_avail_qed_entries;

	arg->num_dir_credits = rsrcs->num_avail_dqed_entries;

	return 0;
}

void dlb2_hw_enable_sparse_ldb_cq_mode(struct dlb2_hw *hw)
{
	union dlb2_chp_cfg_chp_csr_ctrl r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_CHP_CFG_CHP_CSR_CTRL);

	r0.field.cfg_64bytes_qe_ldb_cq_mode = 1;

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_CHP_CSR_CTRL, r0.val);
}

void dlb2_resource_free(struct dlb2_hw *hw)
{
	int i;

	if (hw->pf.avail_hist_list_entries)
		dlb2_bitmap_free(hw->pf.avail_hist_list_entries);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		if (hw->vdev[i].avail_hist_list_entries)
			dlb2_bitmap_free(hw->vdev[i].avail_hist_list_entries);
	}
}

int dlb2_resource_init(struct dlb2_hw *hw)
{
	struct dlb2_list_entry *list;
	unsigned int i;
	int ret;

	/*
	 * For optimal load-balancing, ports that map to one or more QIDs in
	 * common should not be in numerical sequence. This is application
	 * dependent, but the driver interleaves port IDs as much as possible
	 * to reduce the likelihood of this. This initial allocation maximizes
	 * the average distance between an ID and its immediate neighbors (i.e.
	 * the distance from 1 to 0 and to 2, the distance from 2 to 1 and to
	 * 3, etc.).
	 */
	u8 init_ldb_port_allocation[DLB2_MAX_NUM_LDB_PORTS] = {
		0,  7,  14,  5, 12,  3, 10,  1,  8, 15,  6, 13,  4, 11,  2,  9,
		16, 23, 30, 21, 28, 19, 26, 17, 24, 31, 22, 29, 20, 27, 18, 25,
		32, 39, 46, 37, 44, 35, 42, 33, 40, 47, 38, 45, 36, 43, 34, 41,
		48, 55, 62, 53, 60, 51, 58, 49, 56, 63, 54, 61, 52, 59, 50, 57,
	};

	/* Zero-out resource tracking data structures */
	memset(&hw->rsrcs, 0, sizeof(hw->rsrcs));
	memset(&hw->pf, 0, sizeof(hw->pf));

	dlb2_init_fn_rsrc_lists(&hw->pf);

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		memset(&hw->vdev[i], 0, sizeof(hw->vdev[i]));
		dlb2_init_fn_rsrc_lists(&hw->vdev[i]);
	}

	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		memset(&hw->domains[i], 0, sizeof(hw->domains[i]));
		dlb2_init_domain_rsrc_lists(&hw->domains[i]);
		hw->domains[i].parent_func = &hw->pf;
	}

	/* Give all resources to the PF driver */
	hw->pf.num_avail_domains = DLB2_MAX_NUM_DOMAINS;
	for (i = 0; i < hw->pf.num_avail_domains; i++) {
		list = &hw->domains[i].func_list;

		dlb2_list_add(&hw->pf.avail_domains, list);
	}

	hw->pf.num_avail_ldb_queues = DLB2_MAX_NUM_LDB_QUEUES;
	for (i = 0; i < hw->pf.num_avail_ldb_queues; i++) {
		list = &hw->rsrcs.ldb_queues[i].func_list;

		dlb2_list_add(&hw->pf.avail_ldb_queues, list);
	}

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		hw->pf.num_avail_ldb_ports[i] =
			DLB2_MAX_NUM_LDB_PORTS / DLB2_NUM_COS_DOMAINS;

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		int cos_id = i >> DLB2_NUM_COS_DOMAINS;
		struct dlb2_ldb_port *port;

		port = &hw->rsrcs.ldb_ports[init_ldb_port_allocation[i]];

		dlb2_list_add(&hw->pf.avail_ldb_ports[cos_id],
			      &port->func_list);
	}

	hw->pf.num_avail_dir_pq_pairs = DLB2_MAX_NUM_DIR_PORTS;
	for (i = 0; i < hw->pf.num_avail_dir_pq_pairs; i++) {
		list = &hw->rsrcs.dir_pq_pairs[i].func_list;

		dlb2_list_add(&hw->pf.avail_dir_pq_pairs, list);
	}

	hw->pf.num_avail_qed_entries = DLB2_MAX_NUM_LDB_CREDITS;
	hw->pf.num_avail_dqed_entries = DLB2_MAX_NUM_DIR_CREDITS;
	hw->pf.num_avail_aqed_entries = DLB2_MAX_NUM_AQED_ENTRIES;

	ret = dlb2_bitmap_alloc(&hw->pf.avail_hist_list_entries,
				DLB2_MAX_NUM_HIST_LIST_ENTRIES);
	if (ret)
		goto unwind;

	ret = dlb2_bitmap_fill(hw->pf.avail_hist_list_entries);
	if (ret)
		goto unwind;

	for (i = 0; i < DLB2_MAX_NUM_VDEVS; i++) {
		ret = dlb2_bitmap_alloc(&hw->vdev[i].avail_hist_list_entries,
					DLB2_MAX_NUM_HIST_LIST_ENTRIES);
		if (ret)
			goto unwind;

		ret = dlb2_bitmap_zero(hw->vdev[i].avail_hist_list_entries);
		if (ret)
			goto unwind;
	}

	/* Initialize the hardware resource IDs */
	for (i = 0; i < DLB2_MAX_NUM_DOMAINS; i++) {
		hw->domains[i].id.phys_id = i;
		hw->domains[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_LDB_QUEUES; i++) {
		hw->rsrcs.ldb_queues[i].id.phys_id = i;
		hw->rsrcs.ldb_queues[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_LDB_PORTS; i++) {
		hw->rsrcs.ldb_ports[i].id.phys_id = i;
		hw->rsrcs.ldb_ports[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_DIR_PORTS; i++) {
		hw->rsrcs.dir_pq_pairs[i].id.phys_id = i;
		hw->rsrcs.dir_pq_pairs[i].id.vdev_owned = false;
	}

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		hw->rsrcs.sn_groups[i].id = i;
		/* Default mode (0) is 64 sequence numbers per queue */
		hw->rsrcs.sn_groups[i].mode = 0;
		hw->rsrcs.sn_groups[i].sequence_numbers_per_queue = 64;
		hw->rsrcs.sn_groups[i].slot_use_bitmap = 0;
	}

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++)
		hw->cos_reservation[i] = 100 / DLB2_NUM_COS_DOMAINS;

	return 0;

unwind:
	dlb2_resource_free(hw);

	return ret;
}

void dlb2_clr_pmcsr_disable(struct dlb2_hw *hw)
{
	union dlb2_cfg_mstr_cfg_pm_pmcsr_disable r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_CFG_MSTR_CFG_PM_PMCSR_DISABLE);

	r0.field.disable = 0;

	DLB2_CSR_WR(hw, DLB2_CFG_MSTR_CFG_PM_PMCSR_DISABLE, r0.val);
}

static void dlb2_configure_domain_credits(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	union dlb2_chp_cfg_ldb_vas_crd r0 = { {0} };
	union dlb2_chp_cfg_dir_vas_crd r1 = { {0} };

	r0.field.count = domain->num_ldb_credits;

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_LDB_VAS_CRD(domain->id.phys_id), r0.val);

	r1.field.count = domain->num_dir_credits;

	DLB2_CSR_WR(hw, DLB2_CHP_CFG_DIR_VAS_CRD(domain->id.phys_id), r1.val);
}

static struct dlb2_ldb_port *
dlb2_get_next_ldb_port(struct dlb2_hw *hw,
		       struct dlb2_function_resources *rsrcs,
		       u32 domain_id,
		       u32 cos_id)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	RTE_SET_USED(iter);
	/*
	 * To reduce the odds of consecutive load-balanced ports mapping to the
	 * same queue(s), the driver attempts to allocate ports whose neighbors
	 * are owned by a different domain.
	 */
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[cos_id], port, iter) {
		u32 next, prev;
		u32 phys_id;

		phys_id = port->id.phys_id;
		next = phys_id + 1;
		prev = phys_id - 1;

		if (phys_id == DLB2_MAX_NUM_LDB_PORTS - 1)
			next = 0;
		if (phys_id == 0)
			prev = DLB2_MAX_NUM_LDB_PORTS - 1;

		if (!hw->rsrcs.ldb_ports[next].owned ||
		    hw->rsrcs.ldb_ports[next].domain_id.phys_id == domain_id)
			continue;

		if (!hw->rsrcs.ldb_ports[prev].owned ||
		    hw->rsrcs.ldb_ports[prev].domain_id.phys_id == domain_id)
			continue;

		return port;
	}

	/*
	 * Failing that, the driver looks for a port with one neighbor owned by
	 * a different domain and the other unallocated.
	 */
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[cos_id], port, iter) {
		u32 next, prev;
		u32 phys_id;

		phys_id = port->id.phys_id;
		next = phys_id + 1;
		prev = phys_id - 1;

		if (phys_id == DLB2_MAX_NUM_LDB_PORTS - 1)
			next = 0;
		if (phys_id == 0)
			prev = DLB2_MAX_NUM_LDB_PORTS - 1;

		if (!hw->rsrcs.ldb_ports[prev].owned &&
		    hw->rsrcs.ldb_ports[next].owned &&
		    hw->rsrcs.ldb_ports[next].domain_id.phys_id != domain_id)
			return port;

		if (!hw->rsrcs.ldb_ports[next].owned &&
		    hw->rsrcs.ldb_ports[prev].owned &&
		    hw->rsrcs.ldb_ports[prev].domain_id.phys_id != domain_id)
			return port;
	}

	/*
	 * Failing that, the driver looks for a port with both neighbors
	 * unallocated.
	 */
	DLB2_FUNC_LIST_FOR(rsrcs->avail_ldb_ports[cos_id], port, iter) {
		u32 next, prev;
		u32 phys_id;

		phys_id = port->id.phys_id;
		next = phys_id + 1;
		prev = phys_id - 1;

		if (phys_id == DLB2_MAX_NUM_LDB_PORTS - 1)
			next = 0;
		if (phys_id == 0)
			prev = DLB2_MAX_NUM_LDB_PORTS - 1;

		if (!hw->rsrcs.ldb_ports[prev].owned &&
		    !hw->rsrcs.ldb_ports[next].owned)
			return port;
	}

	/* If all else fails, the driver returns the next available port. */
	return DLB2_FUNC_LIST_HEAD(rsrcs->avail_ldb_ports[cos_id],
				   typeof(*port));
}

static int __dlb2_attach_ldb_ports(struct dlb2_hw *hw,
				   struct dlb2_function_resources *rsrcs,
				   struct dlb2_hw_domain *domain,
				   u32 num_ports,
				   u32 cos_id,
				   struct dlb2_cmd_response *resp)
{
	unsigned int i;

	if (rsrcs->num_avail_ldb_ports[cos_id] < num_ports) {
		resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; i < num_ports; i++) {
		struct dlb2_ldb_port *port;

		port = dlb2_get_next_ldb_port(hw, rsrcs,
					      domain->id.phys_id, cos_id);
		if (port == NULL) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: domain validation failed\n",
				    __func__);
			return -EFAULT;
		}

		dlb2_list_del(&rsrcs->avail_ldb_ports[cos_id],
			      &port->func_list);

		port->domain_id = domain->id;
		port->owned = true;

		dlb2_list_add(&domain->avail_ldb_ports[cos_id],
			      &port->domain_list);
	}

	rsrcs->num_avail_ldb_ports[cos_id] -= num_ports;

	return 0;
}

static int dlb2_attach_ldb_ports(struct dlb2_hw *hw,
				 struct dlb2_function_resources *rsrcs,
				 struct dlb2_hw_domain *domain,
				 struct dlb2_create_sched_domain_args *args,
				 struct dlb2_cmd_response *resp)
{
	unsigned int i, j;
	int ret;

	if (args->cos_strict) {
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			u32 num = args->num_cos_ldb_ports[i];

			/* Allocate ports from specific classes-of-service */
			ret = __dlb2_attach_ldb_ports(hw,
						      rsrcs,
						      domain,
						      num,
						      i,
						      resp);
			if (ret)
				return ret;
		}
	} else {
		unsigned int k;
		u32 cos_id;

		/*
		 * Attempt to allocate from specific class-of-service, but
		 * fallback to the other classes if that fails.
		 */
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			for (j = 0; j < args->num_cos_ldb_ports[i]; j++) {
				for (k = 0; k < DLB2_NUM_COS_DOMAINS; k++) {
					cos_id = (i + k) % DLB2_NUM_COS_DOMAINS;

					ret = __dlb2_attach_ldb_ports(hw,
								      rsrcs,
								      domain,
								      1,
								      cos_id,
								      resp);
					if (ret == 0)
						break;
				}

				if (ret < 0)
					return ret;
			}
		}
	}

	/* Allocate num_ldb_ports from any class-of-service */
	for (i = 0; i < args->num_ldb_ports; i++) {
		for (j = 0; j < DLB2_NUM_COS_DOMAINS; j++) {
			ret = __dlb2_attach_ldb_ports(hw,
						      rsrcs,
						      domain,
						      1,
						      j,
						      resp);
			if (ret == 0)
				break;
		}

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int dlb2_attach_dir_ports(struct dlb2_hw *hw,
				 struct dlb2_function_resources *rsrcs,
				 struct dlb2_hw_domain *domain,
				 u32 num_ports,
				 struct dlb2_cmd_response *resp)
{
	unsigned int i;

	if (rsrcs->num_avail_dir_pq_pairs < num_ports) {
		resp->status = DLB2_ST_DIR_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; i < num_ports; i++) {
		struct dlb2_dir_pq_pair *port;

		port = DLB2_FUNC_LIST_HEAD(rsrcs->avail_dir_pq_pairs,
					   typeof(*port));
		if (port == NULL) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: domain validation failed\n",
				    __func__);
			return -EFAULT;
		}

		dlb2_list_del(&rsrcs->avail_dir_pq_pairs, &port->func_list);

		port->domain_id = domain->id;
		port->owned = true;

		dlb2_list_add(&domain->avail_dir_pq_pairs, &port->domain_list);
	}

	rsrcs->num_avail_dir_pq_pairs -= num_ports;

	return 0;
}

static int dlb2_attach_ldb_credits(struct dlb2_function_resources *rsrcs,
				   struct dlb2_hw_domain *domain,
				   u32 num_credits,
				   struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_qed_entries < num_credits) {
		resp->status = DLB2_ST_LDB_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_qed_entries -= num_credits;
	domain->num_ldb_credits += num_credits;
	return 0;
}

static int dlb2_attach_dir_credits(struct dlb2_function_resources *rsrcs,
				   struct dlb2_hw_domain *domain,
				   u32 num_credits,
				   struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_dqed_entries < num_credits) {
		resp->status = DLB2_ST_DIR_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_dqed_entries -= num_credits;
	domain->num_dir_credits += num_credits;
	return 0;
}

static int dlb2_attach_atomic_inflights(struct dlb2_function_resources *rsrcs,
					struct dlb2_hw_domain *domain,
					u32 num_atomic_inflights,
					struct dlb2_cmd_response *resp)
{
	if (rsrcs->num_avail_aqed_entries < num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	rsrcs->num_avail_aqed_entries -= num_atomic_inflights;
	domain->num_avail_aqed_entries += num_atomic_inflights;
	return 0;
}

static int
dlb2_attach_domain_hist_list_entries(struct dlb2_function_resources *rsrcs,
				     struct dlb2_hw_domain *domain,
				     u32 num_hist_list_entries,
				     struct dlb2_cmd_response *resp)
{
	struct dlb2_bitmap *bitmap;
	int base;

	if (num_hist_list_entries) {
		bitmap = rsrcs->avail_hist_list_entries;

		base = dlb2_bitmap_find_set_bit_range(bitmap,
						      num_hist_list_entries);
		if (base < 0)
			goto error;

		domain->total_hist_list_entries = num_hist_list_entries;
		domain->avail_hist_list_entries = num_hist_list_entries;
		domain->hist_list_entry_base = base;
		domain->hist_list_entry_offset = 0;

		dlb2_bitmap_clear_range(bitmap, base, num_hist_list_entries);
	}
	return 0;

error:
	resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
	return -EINVAL;
}

static int dlb2_attach_ldb_queues(struct dlb2_hw *hw,
				  struct dlb2_function_resources *rsrcs,
				  struct dlb2_hw_domain *domain,
				  u32 num_queues,
				  struct dlb2_cmd_response *resp)
{
	unsigned int i;

	if (rsrcs->num_avail_ldb_queues < num_queues) {
		resp->status = DLB2_ST_LDB_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; i < num_queues; i++) {
		struct dlb2_ldb_queue *queue;

		queue = DLB2_FUNC_LIST_HEAD(rsrcs->avail_ldb_queues,
					    typeof(*queue));
		if (queue == NULL) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: domain validation failed\n",
				    __func__);
			return -EFAULT;
		}

		dlb2_list_del(&rsrcs->avail_ldb_queues, &queue->func_list);

		queue->domain_id = domain->id;
		queue->owned = true;

		dlb2_list_add(&domain->avail_ldb_queues, &queue->domain_list);
	}

	rsrcs->num_avail_ldb_queues -= num_queues;

	return 0;
}

static int
dlb2_domain_attach_resources(struct dlb2_hw *hw,
			     struct dlb2_function_resources *rsrcs,
			     struct dlb2_hw_domain *domain,
			     struct dlb2_create_sched_domain_args *args,
			     struct dlb2_cmd_response *resp)
{
	int ret;

	ret = dlb2_attach_ldb_queues(hw,
				     rsrcs,
				     domain,
				     args->num_ldb_queues,
				     resp);
	if (ret < 0)
		return ret;

	ret = dlb2_attach_ldb_ports(hw,
				    rsrcs,
				    domain,
				    args,
				    resp);
	if (ret < 0)
		return ret;

	ret = dlb2_attach_dir_ports(hw,
				    rsrcs,
				    domain,
				    args->num_dir_ports,
				    resp);
	if (ret < 0)
		return ret;

	ret = dlb2_attach_ldb_credits(rsrcs,
				      domain,
				      args->num_ldb_credits,
				      resp);
	if (ret < 0)
		return ret;

	ret = dlb2_attach_dir_credits(rsrcs,
				      domain,
				      args->num_dir_credits,
				      resp);
	if (ret < 0)
		return ret;

	ret = dlb2_attach_domain_hist_list_entries(rsrcs,
						   domain,
						   args->num_hist_list_entries,
						   resp);
	if (ret < 0)
		return ret;

	ret = dlb2_attach_atomic_inflights(rsrcs,
					   domain,
					   args->num_atomic_inflights,
					   resp);
	if (ret < 0)
		return ret;

	dlb2_configure_domain_credits(hw, domain);

	domain->configured = true;

	domain->started = false;

	rsrcs->num_avail_domains--;

	return 0;
}

static int
dlb2_verify_create_sched_dom_args(struct dlb2_function_resources *rsrcs,
				  struct dlb2_create_sched_domain_args *args,
				  struct dlb2_cmd_response *resp)
{
	u32 num_avail_ldb_ports, req_ldb_ports;
	struct dlb2_bitmap *avail_hl_entries;
	unsigned int max_contig_hl_range;
	int i;

	avail_hl_entries = rsrcs->avail_hist_list_entries;

	max_contig_hl_range = dlb2_bitmap_longest_set_range(avail_hl_entries);

	num_avail_ldb_ports = 0;
	req_ldb_ports = 0;
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		num_avail_ldb_ports += rsrcs->num_avail_ldb_ports[i];

		req_ldb_ports += args->num_cos_ldb_ports[i];
	}

	req_ldb_ports += args->num_ldb_ports;

	if (rsrcs->num_avail_domains < 1) {
		resp->status = DLB2_ST_DOMAIN_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_ldb_queues < args->num_ldb_queues) {
		resp->status = DLB2_ST_LDB_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	if (req_ldb_ports > num_avail_ldb_ports) {
		resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	for (i = 0; args->cos_strict && i < DLB2_NUM_COS_DOMAINS; i++) {
		if (args->num_cos_ldb_ports[i] >
		    rsrcs->num_avail_ldb_ports[i]) {
			resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	if (args->num_ldb_queues > 0 && req_ldb_ports == 0) {
		resp->status = DLB2_ST_LDB_PORT_REQUIRED_FOR_LDB_QUEUES;
		return -EINVAL;
	}

	if (rsrcs->num_avail_dir_pq_pairs < args->num_dir_ports) {
		resp->status = DLB2_ST_DIR_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_qed_entries < args->num_ldb_credits) {
		resp->status = DLB2_ST_LDB_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_dqed_entries < args->num_dir_credits) {
		resp->status = DLB2_ST_DIR_CREDITS_UNAVAILABLE;
		return -EINVAL;
	}

	if (rsrcs->num_avail_aqed_entries < args->num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (max_contig_hl_range < args->num_hist_list_entries) {
		resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
		return -EINVAL;
	}

	return 0;
}

static void
dlb2_log_create_sched_domain_args(struct dlb2_hw *hw,
				  struct dlb2_create_sched_domain_args *args,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create sched domain arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tNumber of LDB queues:          %d\n",
		    args->num_ldb_queues);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (any CoS): %d\n",
		    args->num_ldb_ports);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 0):   %d\n",
		    args->num_cos_ldb_ports[0]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 1):   %d\n",
		    args->num_cos_ldb_ports[1]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 2):   %d\n",
		    args->num_cos_ldb_ports[1]);
	DLB2_HW_DBG(hw, "\tNumber of LDB ports (CoS 3):   %d\n",
		    args->num_cos_ldb_ports[1]);
	DLB2_HW_DBG(hw, "\tStrict CoS allocation:         %d\n",
		    args->cos_strict);
	DLB2_HW_DBG(hw, "\tNumber of DIR ports:           %d\n",
		    args->num_dir_ports);
	DLB2_HW_DBG(hw, "\tNumber of ATM inflights:       %d\n",
		    args->num_atomic_inflights);
	DLB2_HW_DBG(hw, "\tNumber of hist list entries:   %d\n",
		    args->num_hist_list_entries);
	DLB2_HW_DBG(hw, "\tNumber of LDB credits:         %d\n",
		    args->num_ldb_credits);
	DLB2_HW_DBG(hw, "\tNumber of DIR credits:         %d\n",
		    args->num_dir_credits);
}

/**
 * dlb2_hw_create_sched_domain() - Allocate and initialize a DLB scheduling
 *	domain and its resources.
 * @hw:	Contains the current state of the DLB2 hardware.
 * @args: User-provided arguments.
 * @resp: Response to user.
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Return: returns < 0 on error, 0 otherwise. If the driver is unable to
 * satisfy a request, resp->status will be set accordingly.
 */
int dlb2_hw_create_sched_domain(struct dlb2_hw *hw,
				struct dlb2_create_sched_domain_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_function_resources *rsrcs;
	struct dlb2_hw_domain *domain;
	int ret;

	rsrcs = (vdev_req) ? &hw->vdev[vdev_id] : &hw->pf;

	dlb2_log_create_sched_domain_args(hw, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_sched_dom_args(rsrcs, args, resp);
	if (ret)
		return ret;

	domain = DLB2_FUNC_LIST_HEAD(rsrcs->avail_domains, typeof(*domain));
	if (domain == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no available domains\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	if (domain->configured) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: avail_domains contains configured domains.\n",
			    __func__);
		return -EFAULT;
	}

	dlb2_init_domain_rsrc_lists(domain);

	ret = dlb2_domain_attach_resources(hw, rsrcs, domain, args, resp);
	if (ret < 0) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to verify args.\n",
			    __func__);

		return ret;
	}

	dlb2_list_del(&rsrcs->avail_domains, &domain->func_list);

	dlb2_list_add(&rsrcs->used_domains, &domain->func_list);

	resp->id = (vdev_req) ? domain->id.virt_id : domain->id.phys_id;
	resp->status = 0;

	return 0;
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

static void dlb2_dir_port_cq_disable(struct dlb2_hw *hw,
				     struct dlb2_dir_pq_pair *port)
{
	union dlb2_lsp_cq_dir_dsbl reg;

	reg.field.disabled = 1;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_DIR_DSBL(port->id.phys_id), reg.val);

	dlb2_flush_csr(hw);
}

static u32 dlb2_dir_cq_token_count(struct dlb2_hw *hw,
				   struct dlb2_dir_pq_pair *port)
{
	union dlb2_lsp_cq_dir_tkn_cnt r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ_DIR_TKN_CNT(port->id.phys_id));

	/*
	 * Account for the initial token count, which is used in order to
	 * provide a CQ with depth less than 8.
	 */

	return r0.field.count - port->init_tkn_cnt;
}

static int dlb2_drain_dir_cq(struct dlb2_hw *hw,
			     struct dlb2_dir_pq_pair *port)
{
	unsigned int port_id = port->id.phys_id;
	u32 cnt;

	/* Return any outstanding tokens */
	cnt = dlb2_dir_cq_token_count(hw, port);

	if (cnt != 0) {
		struct dlb2_hcw hcw_mem[8], *hcw;
		void  *pp_addr;

		pp_addr = os_map_producer_port(hw, port_id, false);

		/* Point hcw to a 64B-aligned location */
		hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

		/*
		 * Program the first HCW for a batch token return and
		 * the rest as NOOPS
		 */
		memset(hcw, 0, 4 * sizeof(*hcw));
		hcw->cq_token = 1;
		hcw->lock_id = cnt - 1;

		dlb2_movdir64b(pp_addr, hcw);

		os_fence_hcw(hw, pp_addr);

		os_unmap_producer_port(hw, pp_addr);
	}

	return 0;
}

static void dlb2_dir_port_cq_enable(struct dlb2_hw *hw,
				    struct dlb2_dir_pq_pair *port)
{
	union dlb2_lsp_cq_dir_dsbl reg;

	reg.field.disabled = 0;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_DIR_DSBL(port->id.phys_id), reg.val);

	dlb2_flush_csr(hw);
}

static int dlb2_domain_drain_dir_cqs(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     bool toggle_port)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	int ret;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		/*
		 * Can't drain a port if it's not configured, and there's
		 * nothing to drain if its queue is unconfigured.
		 */
		if (!port->port_configured || !port->queue_configured)
			continue;

		if (toggle_port)
			dlb2_dir_port_cq_disable(hw, port);

		ret = dlb2_drain_dir_cq(hw, port);
		if (ret < 0)
			return ret;

		if (toggle_port)
			dlb2_dir_port_cq_enable(hw, port);
	}

	return 0;
}

static u32 dlb2_dir_queue_depth(struct dlb2_hw *hw,
				struct dlb2_dir_pq_pair *queue)
{
	union dlb2_lsp_qid_dir_enqueue_cnt r0;

	r0.val = DLB2_CSR_RD(hw,
			     DLB2_LSP_QID_DIR_ENQUEUE_CNT(queue->id.phys_id));

	return r0.field.count;
}

static bool dlb2_dir_queue_is_empty(struct dlb2_hw *hw,
				    struct dlb2_dir_pq_pair *queue)
{
	return dlb2_dir_queue_depth(hw, queue) == 0;
}

static bool dlb2_domain_dir_queues_empty(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *queue;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		if (!dlb2_dir_queue_is_empty(hw, queue))
			return false;
	}

	return true;
}

static int dlb2_domain_drain_dir_queues(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	int i, ret;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	for (i = 0; i < DLB2_MAX_QID_EMPTY_CHECK_LOOPS; i++) {
		ret = dlb2_domain_drain_dir_cqs(hw, domain, true);
		if (ret < 0)
			return ret;

		if (dlb2_domain_dir_queues_empty(hw, domain))
			break;
	}

	if (i == DLB2_MAX_QID_EMPTY_CHECK_LOOPS) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to empty queues\n",
			    __func__);
		return -EFAULT;
	}

	/*
	 * Drain the CQs one more time. For the queues to go empty, they would
	 * have scheduled one or more QEs.
	 */
	ret = dlb2_domain_drain_dir_cqs(hw, domain, true);
	if (ret < 0)
		return ret;

	return 0;
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

static u32 dlb2_ldb_cq_inflight_count(struct dlb2_hw *hw,
				      struct dlb2_ldb_port *port)
{
	union dlb2_lsp_cq_ldb_infl_cnt r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ_LDB_INFL_CNT(port->id.phys_id));

	return r0.field.count;
}

static u32 dlb2_ldb_cq_token_count(struct dlb2_hw *hw,
				   struct dlb2_ldb_port *port)
{
	union dlb2_lsp_cq_ldb_tkn_cnt r0;

	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ_LDB_TKN_CNT(port->id.phys_id));

	/*
	 * Account for the initial token count, which is used in order to
	 * provide a CQ with depth less than 8.
	 */

	return r0.field.token_count - port->init_tkn_cnt;
}

static int dlb2_drain_ldb_cq(struct dlb2_hw *hw, struct dlb2_ldb_port *port)
{
	u32 infl_cnt, tkn_cnt;
	unsigned int i;

	infl_cnt = dlb2_ldb_cq_inflight_count(hw, port);
	tkn_cnt = dlb2_ldb_cq_token_count(hw, port);

	if (infl_cnt || tkn_cnt) {
		struct dlb2_hcw hcw_mem[8], *hcw;
		void  *pp_addr;

		pp_addr = os_map_producer_port(hw, port->id.phys_id, true);

		/* Point hcw to a 64B-aligned location */
		hcw = (struct dlb2_hcw *)((uintptr_t)&hcw_mem[4] & ~0x3F);

		/*
		 * Program the first HCW for a completion and token return and
		 * the other HCWs as NOOPS
		 */

		memset(hcw, 0, 4 * sizeof(*hcw));
		hcw->qe_comp = (infl_cnt > 0);
		hcw->cq_token = (tkn_cnt > 0);
		hcw->lock_id = tkn_cnt - 1;

		/* Return tokens in the first HCW */
		dlb2_movdir64b(pp_addr, hcw);

		hcw->cq_token = 0;

		/* Issue remaining completions (if any) */
		for (i = 1; i < infl_cnt; i++)
			dlb2_movdir64b(pp_addr, hcw);

		os_fence_hcw(hw, pp_addr);

		os_unmap_producer_port(hw, pp_addr);
	}

	return 0;
}

static int dlb2_domain_drain_ldb_cqs(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     bool toggle_port)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int ret, i;
	RTE_SET_USED(iter);

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			if (toggle_port)
				dlb2_ldb_port_cq_disable(hw, port);

			ret = dlb2_drain_ldb_cq(hw, port);
			if (ret < 0)
				return ret;

			if (toggle_port)
				dlb2_ldb_port_cq_enable(hw, port);
		}
	}

	return 0;
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

static bool dlb2_ldb_queue_is_empty(struct dlb2_hw *hw,
				    struct dlb2_ldb_queue *queue)
{
	return dlb2_ldb_queue_depth(hw, queue) == 0;
}

static bool dlb2_domain_mapped_queues_empty(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_queue *queue;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if (queue->num_mappings == 0)
			continue;

		if (!dlb2_ldb_queue_is_empty(hw, queue))
			return false;
	}

	return true;
}

static int dlb2_domain_drain_mapped_queues(struct dlb2_hw *hw,
					   struct dlb2_hw_domain *domain)
{
	int i, ret;

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	if (domain->num_pending_removals > 0) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to unmap domain queues\n",
			    __func__);
		return -EFAULT;
	}

	for (i = 0; i < DLB2_MAX_QID_EMPTY_CHECK_LOOPS; i++) {
		ret = dlb2_domain_drain_ldb_cqs(hw, domain, true);
		if (ret < 0)
			return ret;

		if (dlb2_domain_mapped_queues_empty(hw, domain))
			break;
	}

	if (i == DLB2_MAX_QID_EMPTY_CHECK_LOOPS) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: failed to empty queues\n",
			    __func__);
		return -EFAULT;
	}

	/*
	 * Drain the CQs one more time. For the queues to go empty, they would
	 * have scheduled one or more QEs.
	 */
	ret = dlb2_domain_drain_ldb_cqs(hw, domain, true);
	if (ret < 0)
		return ret;

	return 0;
}

static void dlb2_domain_enable_ldb_cqs(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			port->enabled = true;

			dlb2_ldb_port_cq_enable(hw, port);
		}
	}
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

static void dlb2_domain_disable_ldb_cqs(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			port->enabled = false;

			dlb2_ldb_port_cq_disable(hw, port);
		}
	}
}

static void dlb2_log_reset_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 reset domain:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
}

static void dlb2_domain_disable_dir_vpps(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain,
					 unsigned int vdev_id)
{
	struct dlb2_list_entry *iter;
	union dlb2_sys_vf_dir_vpp_v r1;
	struct dlb2_dir_pq_pair *port;
	RTE_SET_USED(iter);

	r1.field.vpp_v = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		unsigned int offs;
		u32 virt_id;

		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = vdev_id * DLB2_MAX_NUM_DIR_PORTS + virt_id;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VPP_V(offs), r1.val);
	}
}

static void dlb2_domain_disable_ldb_vpps(struct dlb2_hw *hw,
					 struct dlb2_hw_domain *domain,
					 unsigned int vdev_id)
{
	struct dlb2_list_entry *iter;
	union dlb2_sys_vf_ldb_vpp_v r1;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	r1.field.vpp_v = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			unsigned int offs;
			u32 virt_id;

			if (hw->virt_mode == DLB2_VIRT_SRIOV)
				virt_id = port->id.virt_id;
			else
				virt_id = port->id.phys_id;

			offs = vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;

			DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VPP_V(offs), r1.val);
		}
	}
}

static void
dlb2_domain_disable_ldb_port_interrupts(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	union dlb2_chp_ldb_cq_int_enb r0 = { {0} };
	union dlb2_chp_ldb_cq_wd_enb r1 = { {0} };
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	r0.field.en_tim = 0;
	r0.field.en_depth = 0;

	r1.field.wd_enable = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			DLB2_CSR_WR(hw,
				    DLB2_CHP_LDB_CQ_INT_ENB(port->id.phys_id),
				    r0.val);

			DLB2_CSR_WR(hw,
				    DLB2_CHP_LDB_CQ_WD_ENB(port->id.phys_id),
				    r1.val);
		}
	}
}

static void
dlb2_domain_disable_dir_port_interrupts(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	union dlb2_chp_dir_cq_int_enb r0 = { {0} };
	union dlb2_chp_dir_cq_wd_enb r1 = { {0} };
	struct dlb2_dir_pq_pair *port;
	RTE_SET_USED(iter);

	r0.field.en_tim = 0;
	r0.field.en_depth = 0;

	r1.field.wd_enable = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		DLB2_CSR_WR(hw,
			    DLB2_CHP_DIR_CQ_INT_ENB(port->id.phys_id),
			    r0.val);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_DIR_CQ_WD_ENB(port->id.phys_id),
			    r1.val);
	}
}

static void
dlb2_domain_disable_ldb_queue_write_perms(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	int domain_offset = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES;
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_queue *queue;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		union dlb2_sys_ldb_vasqid_v r0 = { {0} };
		union dlb2_sys_ldb_qid2vqid r1 = { {0} };
		union dlb2_sys_vf_ldb_vqid_v r2 = { {0} };
		union dlb2_sys_vf_ldb_vqid2qid r3 = { {0} };
		int idx;

		idx = domain_offset + queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_LDB_VASQID_V(idx), r0.val);

		if (queue->id.vdev_owned) {
			DLB2_CSR_WR(hw,
				    DLB2_SYS_LDB_QID2VQID(queue->id.phys_id),
				    r1.val);

			idx = queue->id.vdev_id * DLB2_MAX_NUM_LDB_QUEUES +
				queue->id.virt_id;

			DLB2_CSR_WR(hw,
				    DLB2_SYS_VF_LDB_VQID_V(idx),
				    r2.val);

			DLB2_CSR_WR(hw,
				    DLB2_SYS_VF_LDB_VQID2QID(idx),
				    r3.val);
		}
	}
}

static void
dlb2_domain_disable_dir_queue_write_perms(struct dlb2_hw *hw,
					  struct dlb2_hw_domain *domain)
{
	int domain_offset = domain->id.phys_id * DLB2_MAX_NUM_DIR_PORTS;
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *queue;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		union dlb2_sys_dir_vasqid_v r0 = { {0} };
		union dlb2_sys_vf_dir_vqid_v r1 = { {0} };
		union dlb2_sys_vf_dir_vqid2qid r2 = { {0} };
		int idx;

		idx = domain_offset + queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_DIR_VASQID_V(idx), r0.val);

		if (queue->id.vdev_owned) {
			idx = queue->id.vdev_id * DLB2_MAX_NUM_DIR_PORTS +
				queue->id.virt_id;

			DLB2_CSR_WR(hw,
				    DLB2_SYS_VF_DIR_VQID_V(idx),
				    r1.val);

			DLB2_CSR_WR(hw,
				    DLB2_SYS_VF_DIR_VQID2QID(idx),
				    r2.val);
		}
	}
}

static void dlb2_domain_disable_ldb_seq_checks(struct dlb2_hw *hw,
					       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	union dlb2_chp_sn_chk_enbl r1;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	r1.field.en = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			DLB2_CSR_WR(hw,
				    DLB2_CHP_SN_CHK_ENBL(port->id.phys_id),
				    r1.val);
	}
}

static int dlb2_domain_wait_for_ldb_cqs_to_empty(struct dlb2_hw *hw,
						 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter) {
			int i;

			for (i = 0; i < DLB2_MAX_CQ_COMP_CHECK_LOOPS; i++) {
				if (dlb2_ldb_cq_inflight_count(hw, port) == 0)
					break;
			}

			if (i == DLB2_MAX_CQ_COMP_CHECK_LOOPS) {
				DLB2_HW_ERR(hw,
					    "[%s()] Internal error: failed to flush load-balanced port %d's completions.\n",
					    __func__, port->id.phys_id);
				return -EFAULT;
			}
		}
	}

	return 0;
}

static void dlb2_domain_disable_dir_cqs(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter) {
		port->enabled = false;

		dlb2_dir_port_cq_disable(hw, port);
	}
}

static void
dlb2_domain_disable_dir_producer_ports(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	union dlb2_sys_dir_pp_v r1;
	RTE_SET_USED(iter);

	r1.field.pp_v = 0;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter)
		DLB2_CSR_WR(hw,
			    DLB2_SYS_DIR_PP_V(port->id.phys_id),
			    r1.val);
}

static void
dlb2_domain_disable_ldb_producer_ports(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	union dlb2_sys_ldb_pp_v r1;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	r1.field.pp_v = 0;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			DLB2_CSR_WR(hw,
				    DLB2_SYS_LDB_PP_V(port->id.phys_id),
				    r1.val);
	}
}

static int dlb2_domain_verify_reset_success(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_ldb_queue *queue;
	int i;
	RTE_SET_USED(iter);

	/*
	 * Confirm that all the domain's queue's inflight counts and AQED
	 * active counts are 0.
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if (!dlb2_ldb_queue_is_empty(hw, queue)) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to empty ldb queue %d\n",
				    __func__, queue->id.phys_id);
			return -EFAULT;
		}
	}

	/* Confirm that all the domain's CQs inflight and token counts are 0. */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], ldb_port, iter) {
			if (dlb2_ldb_cq_inflight_count(hw, ldb_port) ||
			    dlb2_ldb_cq_token_count(hw, ldb_port)) {
				DLB2_HW_ERR(hw,
					    "[%s()] Internal error: failed to empty ldb port %d\n",
					    __func__, ldb_port->id.phys_id);
				return -EFAULT;
			}
		}
	}

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, dir_port, iter) {
		if (!dlb2_dir_queue_is_empty(hw, dir_port)) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to empty dir queue %d\n",
				    __func__, dir_port->id.phys_id);
			return -EFAULT;
		}

		if (dlb2_dir_cq_token_count(hw, dir_port)) {
			DLB2_HW_ERR(hw,
				    "[%s()] Internal error: failed to empty dir port %d\n",
				    __func__, dir_port->id.phys_id);
			return -EFAULT;
		}
	}

	return 0;
}

static void __dlb2_domain_reset_ldb_port_registers(struct dlb2_hw *hw,
						   struct dlb2_ldb_port *port)
{
	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_PP2VAS(port->id.phys_id),
		    DLB2_SYS_LDB_PP2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ2VAS(port->id.phys_id),
		    DLB2_CHP_LDB_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_PP2VDEV(port->id.phys_id),
		    DLB2_SYS_LDB_PP2VDEV_RST);

	if (port->id.vdev_owned) {
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = port->id.vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;

		DLB2_CSR_WR(hw,
			    DLB2_SYS_VF_LDB_VPP2PP(offs),
			    DLB2_SYS_VF_LDB_VPP2PP_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_VF_LDB_VPP_V(offs),
			    DLB2_SYS_VF_LDB_VPP_V_RST);
	}

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_PP_V(port->id.phys_id),
		    DLB2_SYS_LDB_PP_V_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_DSBL(port->id.phys_id),
		    DLB2_LSP_CQ_LDB_DSBL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_DEPTH(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_DEPTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_INFL_LIM(port->id.phys_id),
		    DLB2_LSP_CQ_LDB_INFL_LIM_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_LIM(port->id.phys_id),
		    DLB2_CHP_HIST_LIST_LIM_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_BASE(port->id.phys_id),
		    DLB2_CHP_HIST_LIST_BASE_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_POP_PTR(port->id.phys_id),
		    DLB2_CHP_HIST_LIST_POP_PTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_PUSH_PTR(port->id.phys_id),
		    DLB2_CHP_HIST_LIST_PUSH_PTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_INT_DEPTH_THRSH(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_INT_DEPTH_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_TMR_THRSH(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_TMR_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_INT_ENB(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_INT_ENB_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_ISR(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_ISR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL(port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_WPTR(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_WPTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TKN_CNT(port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TKN_CNT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_ADDR_L(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_ADDR_L_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_ADDR_U(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_ADDR_U_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_AT(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_AT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_PASID(port->id.phys_id),
		    DLB2_SYS_LDB_CQ_PASID_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ2VF_PF_RO(port->id.phys_id),
		    DLB2_SYS_LDB_CQ2VF_PF_RO_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTL(port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTH(port->id.phys_id),
		    DLB2_LSP_CQ_LDB_TOT_SCH_CNTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ2QID0(port->id.phys_id),
		    DLB2_LSP_CQ2QID0_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ2QID1(port->id.phys_id),
		    DLB2_LSP_CQ2QID1_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ2PRIOV(port->id.phys_id),
		    DLB2_LSP_CQ2PRIOV_RST);
}

static void dlb2_domain_reset_ldb_port_registers(struct dlb2_hw *hw,
						 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			__dlb2_domain_reset_ldb_port_registers(hw, port);
	}
}

static void
__dlb2_domain_reset_dir_port_registers(struct dlb2_hw *hw,
				       struct dlb2_dir_pq_pair *port)
{
	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ2VAS(port->id.phys_id),
		    DLB2_CHP_DIR_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_DSBL(port->id.phys_id),
		    DLB2_LSP_CQ_DIR_DSBL_RST);

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_OPT_CLR, port->id.phys_id);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_DEPTH(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_DEPTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_INT_DEPTH_THRSH(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_INT_DEPTH_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_TMR_THRSH(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_TMR_THRSH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_INT_ENB(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_INT_ENB_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_ISR(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_ISR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_WPTR(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_WPTR_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TKN_CNT(port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TKN_CNT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_ADDR_L(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_ADDR_L_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_ADDR_U(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_ADDR_U_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_AT(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_AT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_PASID(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_PASID_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_FMT(port->id.phys_id),
		    DLB2_SYS_DIR_CQ_FMT_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ2VF_PF_RO(port->id.phys_id),
		    DLB2_SYS_DIR_CQ2VF_PF_RO_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTL(port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTL_RST);

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTH(port->id.phys_id),
		    DLB2_LSP_CQ_DIR_TOT_SCH_CNTH_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_PP2VAS(port->id.phys_id),
		    DLB2_SYS_DIR_PP2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ2VAS(port->id.phys_id),
		    DLB2_CHP_DIR_CQ2VAS_RST);

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_PP2VDEV(port->id.phys_id),
		    DLB2_SYS_DIR_PP2VDEV_RST);

	if (port->id.vdev_owned) {
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		offs = port->id.vdev_id * DLB2_MAX_NUM_DIR_PORTS + virt_id;

		DLB2_CSR_WR(hw,
			    DLB2_SYS_VF_DIR_VPP2PP(offs),
			    DLB2_SYS_VF_DIR_VPP2PP_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_VF_DIR_VPP_V(offs),
			    DLB2_SYS_VF_DIR_VPP_V_RST);
	}

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_PP_V(port->id.phys_id),
		    DLB2_SYS_DIR_PP_V_RST);
}

static void dlb2_domain_reset_dir_port_registers(struct dlb2_hw *hw,
						 struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter)
		__dlb2_domain_reset_dir_port_registers(hw, port);
}

static void dlb2_domain_reset_ldb_queue_registers(struct dlb2_hw *hw,
						  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_queue *queue;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		unsigned int queue_id = queue->id.phys_id;
		int i;

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTL(queue_id),
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTH(queue_id),
			    DLB2_LSP_QID_NALDB_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTL(queue_id),
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTH(queue_id),
			    DLB2_LSP_QID_ATM_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_MAX_DEPTH(queue_id),
			    DLB2_LSP_QID_NALDB_MAX_DEPTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_LDB_INFL_LIM(queue_id),
			    DLB2_LSP_QID_LDB_INFL_LIM_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_AQED_ACTIVE_LIM(queue_id),
			    DLB2_LSP_QID_AQED_ACTIVE_LIM_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_ATM_DEPTH_THRSH(queue_id),
			    DLB2_LSP_QID_ATM_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_NALDB_DEPTH_THRSH(queue_id),
			    DLB2_LSP_QID_NALDB_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID_ITS(queue_id),
			    DLB2_SYS_LDB_QID_ITS_RST);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_ORD_QID_SN(queue_id),
			    DLB2_CHP_ORD_QID_SN_RST);

		DLB2_CSR_WR(hw,
			    DLB2_CHP_ORD_QID_SN_MAP(queue_id),
			    DLB2_CHP_ORD_QID_SN_MAP_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID_V(queue_id),
			    DLB2_SYS_LDB_QID_V_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID_CFG_V(queue_id),
			    DLB2_SYS_LDB_QID_CFG_V_RST);

		if (queue->sn_cfg_valid) {
			u32 offs[2];

			offs[0] = DLB2_RO_PIPE_GRP_0_SLT_SHFT(queue->sn_slot);
			offs[1] = DLB2_RO_PIPE_GRP_1_SLT_SHFT(queue->sn_slot);

			DLB2_CSR_WR(hw,
				    offs[queue->sn_group],
				    DLB2_RO_PIPE_GRP_0_SLT_SHFT_RST);
		}

		for (i = 0; i < DLB2_LSP_QID2CQIDIX_NUM; i++) {
			DLB2_CSR_WR(hw,
				    DLB2_LSP_QID2CQIDIX(queue_id, i),
				    DLB2_LSP_QID2CQIDIX_00_RST);

			DLB2_CSR_WR(hw,
				    DLB2_LSP_QID2CQIDIX2(queue_id, i),
				    DLB2_LSP_QID2CQIDIX2_00_RST);

			DLB2_CSR_WR(hw,
				    DLB2_ATM_QID2CQIDIX(queue_id, i),
				    DLB2_ATM_QID2CQIDIX_00_RST);
		}
	}
}

static void dlb2_domain_reset_dir_queue_registers(struct dlb2_hw *hw,
						  struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *queue;
	RTE_SET_USED(iter);

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, queue, iter) {
		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_MAX_DEPTH(queue->id.phys_id),
			    DLB2_LSP_QID_DIR_MAX_DEPTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTL(queue->id.phys_id),
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTL_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTH(queue->id.phys_id),
			    DLB2_LSP_QID_DIR_TOT_ENQ_CNTH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_LSP_QID_DIR_DEPTH_THRSH(queue->id.phys_id),
			    DLB2_LSP_QID_DIR_DEPTH_THRSH_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_DIR_QID_ITS(queue->id.phys_id),
			    DLB2_SYS_DIR_QID_ITS_RST);

		DLB2_CSR_WR(hw,
			    DLB2_SYS_DIR_QID_V(queue->id.phys_id),
			    DLB2_SYS_DIR_QID_V_RST);
	}
}

static void dlb2_domain_reset_registers(struct dlb2_hw *hw,
					struct dlb2_hw_domain *domain)
{
	dlb2_domain_reset_ldb_port_registers(hw, domain);

	dlb2_domain_reset_dir_port_registers(hw, domain);

	dlb2_domain_reset_ldb_queue_registers(hw, domain);

	dlb2_domain_reset_dir_queue_registers(hw, domain);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_CFG_LDB_VAS_CRD(domain->id.phys_id),
		    DLB2_CHP_CFG_LDB_VAS_CRD_RST);

	DLB2_CSR_WR(hw,
		    DLB2_CHP_CFG_DIR_VAS_CRD(domain->id.phys_id),
		    DLB2_CHP_CFG_DIR_VAS_CRD_RST);
}

static int dlb2_domain_reset_software_state(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain)
{
	struct dlb2_dir_pq_pair *tmp_dir_port;
	struct dlb2_ldb_queue *tmp_ldb_queue;
	struct dlb2_ldb_port *tmp_ldb_port;
	struct dlb2_list_entry *iter1;
	struct dlb2_list_entry *iter2;
	struct dlb2_function_resources *rsrcs;
	struct dlb2_dir_pq_pair *dir_port;
	struct dlb2_ldb_queue *ldb_queue;
	struct dlb2_ldb_port *ldb_port;
	struct dlb2_list_head *list;
	int ret, i;
	RTE_SET_USED(tmp_dir_port);
	RTE_SET_USED(tmp_ldb_queue);
	RTE_SET_USED(tmp_ldb_port);
	RTE_SET_USED(iter1);
	RTE_SET_USED(iter2);

	rsrcs = domain->parent_func;

	/* Move the domain's ldb queues to the function's avail list */
	list = &domain->used_ldb_queues;
	DLB2_DOM_LIST_FOR_SAFE(*list, ldb_queue, tmp_ldb_queue, iter1, iter2) {
		if (ldb_queue->sn_cfg_valid) {
			struct dlb2_sn_group *grp;

			grp = &hw->rsrcs.sn_groups[ldb_queue->sn_group];

			dlb2_sn_group_free_slot(grp, ldb_queue->sn_slot);
			ldb_queue->sn_cfg_valid = false;
		}

		ldb_queue->owned = false;
		ldb_queue->num_mappings = 0;
		ldb_queue->num_pending_additions = 0;

		dlb2_list_del(&domain->used_ldb_queues,
			      &ldb_queue->domain_list);
		dlb2_list_add(&rsrcs->avail_ldb_queues,
			      &ldb_queue->func_list);
		rsrcs->num_avail_ldb_queues++;
	}

	list = &domain->avail_ldb_queues;
	DLB2_DOM_LIST_FOR_SAFE(*list, ldb_queue, tmp_ldb_queue, iter1, iter2) {
		ldb_queue->owned = false;

		dlb2_list_del(&domain->avail_ldb_queues,
			      &ldb_queue->domain_list);
		dlb2_list_add(&rsrcs->avail_ldb_queues,
			      &ldb_queue->func_list);
		rsrcs->num_avail_ldb_queues++;
	}

	/* Move the domain's ldb ports to the function's avail list */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		list = &domain->used_ldb_ports[i];
		DLB2_DOM_LIST_FOR_SAFE(*list, ldb_port, tmp_ldb_port,
				       iter1, iter2) {
			int j;

			ldb_port->owned = false;
			ldb_port->configured = false;
			ldb_port->num_pending_removals = 0;
			ldb_port->num_mappings = 0;
			ldb_port->init_tkn_cnt = 0;
			for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++)
				ldb_port->qid_map[j].state =
					DLB2_QUEUE_UNMAPPED;

			dlb2_list_del(&domain->used_ldb_ports[i],
				      &ldb_port->domain_list);
			dlb2_list_add(&rsrcs->avail_ldb_ports[i],
				      &ldb_port->func_list);
			rsrcs->num_avail_ldb_ports[i]++;
		}

		list = &domain->avail_ldb_ports[i];
		DLB2_DOM_LIST_FOR_SAFE(*list, ldb_port, tmp_ldb_port,
				       iter1, iter2) {
			ldb_port->owned = false;

			dlb2_list_del(&domain->avail_ldb_ports[i],
				      &ldb_port->domain_list);
			dlb2_list_add(&rsrcs->avail_ldb_ports[i],
				      &ldb_port->func_list);
			rsrcs->num_avail_ldb_ports[i]++;
		}
	}

	/* Move the domain's dir ports to the function's avail list */
	list = &domain->used_dir_pq_pairs;
	DLB2_DOM_LIST_FOR_SAFE(*list, dir_port, tmp_dir_port, iter1, iter2) {
		dir_port->owned = false;
		dir_port->port_configured = false;
		dir_port->init_tkn_cnt = 0;

		dlb2_list_del(&domain->used_dir_pq_pairs,
			      &dir_port->domain_list);

		dlb2_list_add(&rsrcs->avail_dir_pq_pairs,
			      &dir_port->func_list);
		rsrcs->num_avail_dir_pq_pairs++;
	}

	list = &domain->avail_dir_pq_pairs;
	DLB2_DOM_LIST_FOR_SAFE(*list, dir_port, tmp_dir_port, iter1, iter2) {
		dir_port->owned = false;

		dlb2_list_del(&domain->avail_dir_pq_pairs,
			      &dir_port->domain_list);

		dlb2_list_add(&rsrcs->avail_dir_pq_pairs,
			      &dir_port->func_list);
		rsrcs->num_avail_dir_pq_pairs++;
	}

	/* Return hist list entries to the function */
	ret = dlb2_bitmap_set_range(rsrcs->avail_hist_list_entries,
				    domain->hist_list_entry_base,
				    domain->total_hist_list_entries);
	if (ret) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: domain hist list base doesn't match the function's bitmap.\n",
			    __func__);
		return ret;
	}

	domain->total_hist_list_entries = 0;
	domain->avail_hist_list_entries = 0;
	domain->hist_list_entry_base = 0;
	domain->hist_list_entry_offset = 0;

	rsrcs->num_avail_qed_entries += domain->num_ldb_credits;
	domain->num_ldb_credits = 0;

	rsrcs->num_avail_dqed_entries += domain->num_dir_credits;
	domain->num_dir_credits = 0;

	rsrcs->num_avail_aqed_entries += domain->num_avail_aqed_entries;
	rsrcs->num_avail_aqed_entries += domain->num_used_aqed_entries;
	domain->num_avail_aqed_entries = 0;
	domain->num_used_aqed_entries = 0;

	domain->num_pending_removals = 0;
	domain->num_pending_additions = 0;
	domain->configured = false;
	domain->started = false;

	/*
	 * Move the domain out of the used_domains list and back to the
	 * function's avail_domains list.
	 */
	dlb2_list_del(&rsrcs->used_domains, &domain->func_list);
	dlb2_list_add(&rsrcs->avail_domains, &domain->func_list);
	rsrcs->num_avail_domains++;

	return 0;
}

static int dlb2_domain_drain_unmapped_queue(struct dlb2_hw *hw,
					    struct dlb2_hw_domain *domain,
					    struct dlb2_ldb_queue *queue)
{
	struct dlb2_ldb_port *port;
	int ret, i;

	/* If a domain has LDB queues, it must have LDB ports */
	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		if (!dlb2_list_empty(&domain->used_ldb_ports[i]))
			break;
	}

	if (i == DLB2_NUM_COS_DOMAINS) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: No configured LDB ports\n",
			    __func__);
		return -EFAULT;
	}

	port = DLB2_DOM_LIST_HEAD(domain->used_ldb_ports[i], typeof(*port));

	/* If necessary, free up a QID slot in this CQ */
	if (port->num_mappings == DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		struct dlb2_ldb_queue *mapped_queue;

		mapped_queue = &hw->rsrcs.ldb_queues[port->qid_map[0].qid];

		ret = dlb2_ldb_port_unmap_qid(hw, port, mapped_queue);
		if (ret)
			return ret;
	}

	ret = dlb2_ldb_port_map_qid_dynamic(hw, port, queue, 0);
	if (ret)
		return ret;

	return dlb2_domain_drain_mapped_queues(hw, domain);
}

static int dlb2_domain_drain_unmapped_queues(struct dlb2_hw *hw,
					     struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_queue *queue;
	int ret;
	RTE_SET_USED(iter);

	/* If the domain hasn't been started, there's no traffic to drain */
	if (!domain->started)
		return 0;

	/*
	 * Pre-condition: the unattached queue must not have any outstanding
	 * completions. This is ensured by calling dlb2_domain_drain_ldb_cqs()
	 * prior to this in dlb2_domain_drain_mapped_queues().
	 */
	DLB2_DOM_LIST_FOR(domain->used_ldb_queues, queue, iter) {
		if (queue->num_mappings != 0 ||
		    dlb2_ldb_queue_is_empty(hw, queue))
			continue;

		ret = dlb2_domain_drain_unmapped_queue(hw, domain, queue);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * dlb2_reset_domain() - Reset a DLB scheduling domain and its associated
 *	hardware resources.
 * @hw:	Contains the current state of the DLB2 hardware.
 * @domain_id: Domain ID
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Note: User software *must* stop sending to this domain's producer ports
 * before invoking this function, otherwise undefined behavior will result.
 *
 * Return: returns < 0 on error, 0 otherwise.
 */
int dlb2_reset_domain(struct dlb2_hw *hw,
		      u32 domain_id,
		      bool vdev_req,
		      unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_reset_domain(hw, domain_id, vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (domain  == NULL || !domain->configured)
		return -EINVAL;

	/* Disable VPPs */
	if (vdev_req) {
		dlb2_domain_disable_dir_vpps(hw, domain, vdev_id);

		dlb2_domain_disable_ldb_vpps(hw, domain, vdev_id);
	}

	/* Disable CQ interrupts */
	dlb2_domain_disable_dir_port_interrupts(hw, domain);

	dlb2_domain_disable_ldb_port_interrupts(hw, domain);

	/*
	 * For each queue owned by this domain, disable its write permissions to
	 * cause any traffic sent to it to be dropped. Well-behaved software
	 * should not be sending QEs at this point.
	 */
	dlb2_domain_disable_dir_queue_write_perms(hw, domain);

	dlb2_domain_disable_ldb_queue_write_perms(hw, domain);

	/* Turn off completion tracking on all the domain's PPs. */
	dlb2_domain_disable_ldb_seq_checks(hw, domain);

	/*
	 * Disable the LDB CQs and drain them in order to complete the map and
	 * unmap procedures, which require zero CQ inflights and zero QID
	 * inflights respectively.
	 */
	dlb2_domain_disable_ldb_cqs(hw, domain);

	ret = dlb2_domain_drain_ldb_cqs(hw, domain, false);
	if (ret < 0)
		return ret;

	ret = dlb2_domain_wait_for_ldb_cqs_to_empty(hw, domain);
	if (ret < 0)
		return ret;

	ret = dlb2_domain_finish_unmap_qid_procedures(hw, domain);
	if (ret < 0)
		return ret;

	ret = dlb2_domain_finish_map_qid_procedures(hw, domain);
	if (ret < 0)
		return ret;

	/* Re-enable the CQs in order to drain the mapped queues. */
	dlb2_domain_enable_ldb_cqs(hw, domain);

	ret = dlb2_domain_drain_mapped_queues(hw, domain);
	if (ret < 0)
		return ret;

	ret = dlb2_domain_drain_unmapped_queues(hw, domain);
	if (ret < 0)
		return ret;

	/* Done draining LDB QEs, so disable the CQs. */
	dlb2_domain_disable_ldb_cqs(hw, domain);

	dlb2_domain_drain_dir_queues(hw, domain);

	/* Done draining DIR QEs, so disable the CQs. */
	dlb2_domain_disable_dir_cqs(hw, domain);

	/* Disable PPs */
	dlb2_domain_disable_dir_producer_ports(hw, domain);

	dlb2_domain_disable_ldb_producer_ports(hw, domain);

	ret = dlb2_domain_verify_reset_success(hw, domain);
	if (ret)
		return ret;

	/* Reset the QID and port state. */
	dlb2_domain_reset_registers(hw, domain);

	/* Hardware reset complete. Reset the domain's software state */
	ret = dlb2_domain_reset_software_state(hw, domain);
	if (ret)
		return ret;

	return 0;
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


static void dlb2_configure_ldb_queue(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     struct dlb2_ldb_queue *queue,
				     struct dlb2_create_ldb_queue_args *args,
				     bool vdev_req,
				     unsigned int vdev_id)
{
	union dlb2_sys_vf_ldb_vqid_v r0 = { {0} };
	union dlb2_sys_vf_ldb_vqid2qid r1 = { {0} };
	union dlb2_sys_ldb_qid2vqid r2 = { {0} };
	union dlb2_sys_ldb_vasqid_v r3 = { {0} };
	union dlb2_lsp_qid_ldb_infl_lim r4 = { {0} };
	union dlb2_lsp_qid_aqed_active_lim r5 = { {0} };
	union dlb2_aqed_pipe_qid_hid_width r6 = { {0} };
	union dlb2_sys_ldb_qid_its r7 = { {0} };
	union dlb2_lsp_qid_atm_depth_thrsh r8 = { {0} };
	union dlb2_lsp_qid_naldb_depth_thrsh r9 = { {0} };
	union dlb2_aqed_pipe_qid_fid_lim r10 = { {0} };
	union dlb2_chp_ord_qid_sn_map r11 = { {0} };
	union dlb2_sys_ldb_qid_cfg_v r12 = { {0} };
	union dlb2_sys_ldb_qid_v r13 = { {0} };

	struct dlb2_sn_group *sn_group;
	unsigned int offs;

	/* QID write permissions are turned on when the domain is started */
	r3.field.vasqid_v = 0;

	offs = domain->id.phys_id * DLB2_MAX_NUM_LDB_QUEUES +
		queue->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_VASQID_V(offs), r3.val);

	/*
	 * Unordered QIDs get 4K inflights, ordered get as many as the number
	 * of sequence numbers.
	 */
	r4.field.limit = args->num_qid_inflights;

	DLB2_CSR_WR(hw, DLB2_LSP_QID_LDB_INFL_LIM(queue->id.phys_id), r4.val);

	r5.field.limit = queue->aqed_limit;

	if (r5.field.limit > DLB2_MAX_NUM_AQED_ENTRIES)
		r5.field.limit = DLB2_MAX_NUM_AQED_ENTRIES;

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_AQED_ACTIVE_LIM(queue->id.phys_id),
		    r5.val);

	switch (args->lock_id_comp_level) {
	case 64:
		r6.field.compress_code = 1;
		break;
	case 128:
		r6.field.compress_code = 2;
		break;
	case 256:
		r6.field.compress_code = 3;
		break;
	case 512:
		r6.field.compress_code = 4;
		break;
	case 1024:
		r6.field.compress_code = 5;
		break;
	case 2048:
		r6.field.compress_code = 6;
		break;
	case 4096:
		r6.field.compress_code = 7;
		break;
	case 0:
	case 65536:
		r6.field.compress_code = 0;
	}

	DLB2_CSR_WR(hw,
		    DLB2_AQED_PIPE_QID_HID_WIDTH(queue->id.phys_id),
		    r6.val);

	/* Don't timestamp QEs that pass through this queue */
	r7.field.qid_its = 0;

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_QID_ITS(queue->id.phys_id),
		    r7.val);

	r8.field.thresh = args->depth_threshold;

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_ATM_DEPTH_THRSH(queue->id.phys_id),
		    r8.val);

	r9.field.thresh = args->depth_threshold;

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_NALDB_DEPTH_THRSH(queue->id.phys_id),
		    r9.val);

	/*
	 * This register limits the number of inflight flows a queue can have
	 * at one time.  It has an upper bound of 2048, but can be
	 * over-subscribed. 512 is chosen so that a single queue doesn't use
	 * the entire atomic storage, but can use a substantial portion if
	 * needed.
	 */
	r10.field.qid_fid_limit = 512;

	DLB2_CSR_WR(hw,
		    DLB2_AQED_PIPE_QID_FID_LIM(queue->id.phys_id),
		    r10.val);

	/* Configure SNs */
	sn_group = &hw->rsrcs.sn_groups[queue->sn_group];
	r11.field.mode = sn_group->mode;
	r11.field.slot = queue->sn_slot;
	r11.field.grp  = sn_group->id;

	DLB2_CSR_WR(hw, DLB2_CHP_ORD_QID_SN_MAP(queue->id.phys_id), r11.val);

	r12.field.sn_cfg_v = (args->num_sequence_numbers != 0);
	r12.field.fid_cfg_v = (args->num_atomic_inflights != 0);

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_QID_CFG_V(queue->id.phys_id), r12.val);

	if (vdev_req) {
		offs = vdev_id * DLB2_MAX_NUM_LDB_QUEUES + queue->id.virt_id;

		r0.field.vqid_v = 1;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VQID_V(offs), r0.val);

		r1.field.qid = queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VQID2QID(offs), r1.val);

		r2.field.vqid = queue->id.virt_id;

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_QID2VQID(queue->id.phys_id),
			    r2.val);
	}

	r13.field.qid_v = 1;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_QID_V(queue->id.phys_id), r13.val);
}

static int
dlb2_ldb_queue_attach_to_sn_group(struct dlb2_hw *hw,
				  struct dlb2_ldb_queue *queue,
				  struct dlb2_create_ldb_queue_args *args)
{
	int slot = -1;
	int i;

	queue->sn_cfg_valid = false;

	if (args->num_sequence_numbers == 0)
		return 0;

	for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
		struct dlb2_sn_group *group = &hw->rsrcs.sn_groups[i];

		if (group->sequence_numbers_per_queue ==
		    args->num_sequence_numbers &&
		    !dlb2_sn_group_full(group)) {
			slot = dlb2_sn_group_alloc_slot(group);
			if (slot >= 0)
				break;
		}
	}

	if (slot == -1) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no sequence number slots available\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	queue->sn_cfg_valid = true;
	queue->sn_group = i;
	queue->sn_slot = slot;
	return 0;
}

static int
dlb2_ldb_queue_attach_resources(struct dlb2_hw *hw,
				struct dlb2_hw_domain *domain,
				struct dlb2_ldb_queue *queue,
				struct dlb2_create_ldb_queue_args *args)
{
	int ret;

	ret = dlb2_ldb_queue_attach_to_sn_group(hw, queue, args);
	if (ret)
		return ret;

	/* Attach QID inflights */
	queue->num_qid_inflights = args->num_qid_inflights;

	/* Attach atomic inflights */
	queue->aqed_limit = args->num_atomic_inflights;

	domain->num_avail_aqed_entries -= args->num_atomic_inflights;
	domain->num_used_aqed_entries += args->num_atomic_inflights;

	return 0;
}

static int
dlb2_verify_create_ldb_queue_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_create_ldb_queue_args *args,
				  struct dlb2_cmd_response *resp,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	int i;

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

	if (dlb2_list_empty(&domain->avail_ldb_queues)) {
		resp->status = DLB2_ST_LDB_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	if (args->num_sequence_numbers) {
		for (i = 0; i < DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS; i++) {
			struct dlb2_sn_group *group = &hw->rsrcs.sn_groups[i];

			if (group->sequence_numbers_per_queue ==
			    args->num_sequence_numbers &&
			    !dlb2_sn_group_full(group))
				break;
		}

		if (i == DLB2_MAX_NUM_SEQUENCE_NUMBER_GROUPS) {
			resp->status = DLB2_ST_SEQUENCE_NUMBERS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	if (args->num_qid_inflights > 4096) {
		resp->status = DLB2_ST_INVALID_QID_INFLIGHT_ALLOCATION;
		return -EINVAL;
	}

	/* Inflights must be <= number of sequence numbers if ordered */
	if (args->num_sequence_numbers != 0 &&
	    args->num_qid_inflights > args->num_sequence_numbers) {
		resp->status = DLB2_ST_INVALID_QID_INFLIGHT_ALLOCATION;
		return -EINVAL;
	}

	if (domain->num_avail_aqed_entries < args->num_atomic_inflights) {
		resp->status = DLB2_ST_ATOMIC_INFLIGHTS_UNAVAILABLE;
		return -EINVAL;
	}

	if (args->num_atomic_inflights &&
	    args->lock_id_comp_level != 0 &&
	    args->lock_id_comp_level != 64 &&
	    args->lock_id_comp_level != 128 &&
	    args->lock_id_comp_level != 256 &&
	    args->lock_id_comp_level != 512 &&
	    args->lock_id_comp_level != 1024 &&
	    args->lock_id_comp_level != 2048 &&
	    args->lock_id_comp_level != 4096 &&
	    args->lock_id_comp_level != 65536) {
		resp->status = DLB2_ST_INVALID_LOCK_ID_COMP_LEVEL;
		return -EINVAL;
	}

	return 0;
}

static void
dlb2_log_create_ldb_queue_args(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_create_ldb_queue_args *args,
			       bool vdev_req,
			       unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create load-balanced queue arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID:                  %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tNumber of sequence numbers: %d\n",
		    args->num_sequence_numbers);
	DLB2_HW_DBG(hw, "\tNumber of QID inflights:    %d\n",
		    args->num_qid_inflights);
	DLB2_HW_DBG(hw, "\tNumber of ATM inflights:    %d\n",
		    args->num_atomic_inflights);
}

/**
 * dlb2_hw_create_ldb_queue() - Allocate and initialize a DLB LDB queue.
 * @hw:	Contains the current state of the DLB2 hardware.
 * @domain_id: Domain ID
 * @args: User-provided arguments.
 * @resp: Response to user.
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Return: returns < 0 on error, 0 otherwise. If the driver is unable to
 * satisfy a request, resp->status will be set accordingly.
 */
int dlb2_hw_create_ldb_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_ldb_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	int ret;

	dlb2_log_create_ldb_queue_args(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_ldb_queue_args(hw,
						domain_id,
						args,
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

	queue = DLB2_DOM_LIST_HEAD(domain->avail_ldb_queues, typeof(*queue));
	if (queue == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no available ldb queues\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	ret = dlb2_ldb_queue_attach_resources(hw, domain, queue, args);
	if (ret < 0) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: failed to attach the ldb queue resources\n",
			    __func__, __LINE__);
		return ret;
	}

	dlb2_configure_ldb_queue(hw, domain, queue, args, vdev_req, vdev_id);

	queue->num_mappings = 0;

	queue->configured = true;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list.
	 */
	dlb2_list_del(&domain->avail_ldb_queues, &queue->domain_list);

	dlb2_list_add(&domain->used_ldb_queues, &queue->domain_list);

	resp->status = 0;
	resp->id = (vdev_req) ? queue->id.virt_id : queue->id.phys_id;

	return 0;
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

static void dlb2_ldb_port_configure_pp(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain,
				       struct dlb2_ldb_port *port,
				       bool vdev_req,
				       unsigned int vdev_id)
{
	union dlb2_sys_ldb_pp2vas r0 = { {0} };
	union dlb2_sys_ldb_pp_v r4 = { {0} };

	r0.field.vas = domain->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_PP2VAS(port->id.phys_id), r0.val);

	if (vdev_req) {
		union dlb2_sys_vf_ldb_vpp2pp r1 = { {0} };
		union dlb2_sys_ldb_pp2vdev r2 = { {0} };
		union dlb2_sys_vf_ldb_vpp_v r3 = { {0} };
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		r1.field.pp = port->id.phys_id;

		offs = vdev_id * DLB2_MAX_NUM_LDB_PORTS + virt_id;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VPP2PP(offs), r1.val);

		r2.field.vdev = vdev_id;

		DLB2_CSR_WR(hw,
			    DLB2_SYS_LDB_PP2VDEV(port->id.phys_id),
			    r2.val);

		r3.field.vpp_v = 1;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_LDB_VPP_V(offs), r3.val);
	}

	r4.field.pp_v = 1;

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_PP_V(port->id.phys_id),
		    r4.val);
}

static int dlb2_ldb_port_configure_cq(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      struct dlb2_ldb_port *port,
				      uintptr_t cq_dma_base,
				      struct dlb2_create_ldb_port_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	union dlb2_sys_ldb_cq_addr_l r0 = { {0} };
	union dlb2_sys_ldb_cq_addr_u r1 = { {0} };
	union dlb2_sys_ldb_cq2vf_pf_ro r2 = { {0} };
	union dlb2_chp_ldb_cq_tkn_depth_sel r3 = { {0} };
	union dlb2_lsp_cq_ldb_tkn_depth_sel r4 = { {0} };
	union dlb2_chp_hist_list_lim r5 = { {0} };
	union dlb2_chp_hist_list_base r6 = { {0} };
	union dlb2_lsp_cq_ldb_infl_lim r7 = { {0} };
	union dlb2_chp_hist_list_push_ptr r8 = { {0} };
	union dlb2_chp_hist_list_pop_ptr r9 = { {0} };
	union dlb2_sys_ldb_cq_at r10 = { {0} };
	union dlb2_sys_ldb_cq_pasid r11 = { {0} };
	union dlb2_chp_ldb_cq2vas r12 = { {0} };
	union dlb2_lsp_cq2priov r13 = { {0} };

	/* The CQ address is 64B-aligned, and the DLB only wants bits [63:6] */
	r0.field.addr_l = cq_dma_base >> 6;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_ADDR_L(port->id.phys_id), r0.val);

	r1.field.addr_u = cq_dma_base >> 32;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_ADDR_U(port->id.phys_id), r1.val);

	/*
	 * 'ro' == relaxed ordering. This setting allows DLB2 to write
	 * cache lines out-of-order (but QEs within a cache line are always
	 * updated in-order).
	 */
	r2.field.vf = vdev_id;
	r2.field.is_pf = !vdev_req && (hw->virt_mode != DLB2_VIRT_SIOV);
	r2.field.ro = 1;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ2VF_PF_RO(port->id.phys_id), r2.val);

	if (args->cq_depth <= 8) {
		r3.field.token_depth_select = 1;
	} else if (args->cq_depth == 16) {
		r3.field.token_depth_select = 2;
	} else if (args->cq_depth == 32) {
		r3.field.token_depth_select = 3;
	} else if (args->cq_depth == 64) {
		r3.field.token_depth_select = 4;
	} else if (args->cq_depth == 128) {
		r3.field.token_depth_select = 5;
	} else if (args->cq_depth == 256) {
		r3.field.token_depth_select = 6;
	} else if (args->cq_depth == 512) {
		r3.field.token_depth_select = 7;
	} else if (args->cq_depth == 1024) {
		r3.field.token_depth_select = 8;
	} else {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: invalid CQ depth\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_TKN_DEPTH_SEL(port->id.phys_id),
		    r3.val);

	/*
	 * To support CQs with depth less than 8, program the token count
	 * register with a non-zero initial value. Operations such as domain
	 * reset must take this initial value into account when quiescing the
	 * CQ.
	 */
	port->init_tkn_cnt = 0;

	if (args->cq_depth < 8) {
		union dlb2_lsp_cq_ldb_tkn_cnt r14 = { {0} };

		port->init_tkn_cnt = 8 - args->cq_depth;

		r14.field.token_count = port->init_tkn_cnt;

		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_LDB_TKN_CNT(port->id.phys_id),
			    r14.val);
	} else {
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_LDB_TKN_CNT(port->id.phys_id),
			    DLB2_LSP_CQ_LDB_TKN_CNT_RST);
	}

	r4.field.token_depth_select = r3.field.token_depth_select;
	r4.field.ignore_depth = 0;

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_LDB_TKN_DEPTH_SEL(port->id.phys_id),
		    r4.val);

	/* Reset the CQ write pointer */
	DLB2_CSR_WR(hw,
		    DLB2_CHP_LDB_CQ_WPTR(port->id.phys_id),
		    DLB2_CHP_LDB_CQ_WPTR_RST);

	r5.field.limit = port->hist_list_entry_limit - 1;

	DLB2_CSR_WR(hw, DLB2_CHP_HIST_LIST_LIM(port->id.phys_id), r5.val);

	r6.field.base = port->hist_list_entry_base;

	DLB2_CSR_WR(hw, DLB2_CHP_HIST_LIST_BASE(port->id.phys_id), r6.val);

	/*
	 * The inflight limit sets a cap on the number of QEs for which this CQ
	 * can owe completions at one time.
	 */
	r7.field.limit = args->cq_history_list_size;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ_LDB_INFL_LIM(port->id.phys_id), r7.val);

	r8.field.push_ptr = r6.field.base;
	r8.field.generation = 0;

	DLB2_CSR_WR(hw,
		    DLB2_CHP_HIST_LIST_PUSH_PTR(port->id.phys_id),
		    r8.val);

	r9.field.pop_ptr = r6.field.base;
	r9.field.generation = 0;

	DLB2_CSR_WR(hw, DLB2_CHP_HIST_LIST_POP_PTR(port->id.phys_id), r9.val);

	/*
	 * Address translation (AT) settings: 0: untranslated, 2: translated
	 * (see ATS spec regarding Address Type field for more details)
	 */
	r10.field.cq_at = 0;

	DLB2_CSR_WR(hw, DLB2_SYS_LDB_CQ_AT(port->id.phys_id), r10.val);

	if (vdev_req && hw->virt_mode == DLB2_VIRT_SIOV) {
		r11.field.pasid = hw->pasid[vdev_id];
		r11.field.fmt2 = 1;
	}

	DLB2_CSR_WR(hw,
		    DLB2_SYS_LDB_CQ_PASID(port->id.phys_id),
		    r11.val);

	r12.field.cq2vas = domain->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_CHP_LDB_CQ2VAS(port->id.phys_id), r12.val);

	/* Disable the port's QID mappings */
	r13.field.v = 0;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(port->id.phys_id), r13.val);

	return 0;
}

static int dlb2_configure_ldb_port(struct dlb2_hw *hw,
				   struct dlb2_hw_domain *domain,
				   struct dlb2_ldb_port *port,
				   uintptr_t cq_dma_base,
				   struct dlb2_create_ldb_port_args *args,
				   bool vdev_req,
				   unsigned int vdev_id)
{
	int ret, i;

	port->hist_list_entry_base = domain->hist_list_entry_base +
				     domain->hist_list_entry_offset;
	port->hist_list_entry_limit = port->hist_list_entry_base +
				      args->cq_history_list_size;

	domain->hist_list_entry_offset += args->cq_history_list_size;
	domain->avail_hist_list_entries -= args->cq_history_list_size;

	ret = dlb2_ldb_port_configure_cq(hw,
					 domain,
					 port,
					 cq_dma_base,
					 args,
					 vdev_req,
					 vdev_id);
	if (ret < 0)
		return ret;

	dlb2_ldb_port_configure_pp(hw,
				   domain,
				   port,
				   vdev_req,
				   vdev_id);

	dlb2_ldb_port_cq_enable(hw, port);

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++)
		port->qid_map[i].state = DLB2_QUEUE_UNMAPPED;
	port->num_mappings = 0;

	port->enabled = true;

	port->configured = true;

	return 0;
}

static void
dlb2_log_create_ldb_port_args(struct dlb2_hw *hw,
			      u32 domain_id,
			      uintptr_t cq_dma_base,
			      struct dlb2_create_ldb_port_args *args,
			      bool vdev_req,
			      unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create load-balanced port arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID:                 %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tCQ depth:                  %d\n",
		    args->cq_depth);
	DLB2_HW_DBG(hw, "\tCQ hist list size:         %d\n",
		    args->cq_history_list_size);
	DLB2_HW_DBG(hw, "\tCQ base address:           0x%lx\n",
		    cq_dma_base);
	DLB2_HW_DBG(hw, "\tCoS ID:                    %u\n", args->cos_id);
	DLB2_HW_DBG(hw, "\tStrict CoS allocation:     %u\n",
		    args->cos_strict);
}

static int
dlb2_verify_create_ldb_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 uintptr_t cq_dma_base,
				 struct dlb2_create_ldb_port_args *args,
				 struct dlb2_cmd_response *resp,
				 bool vdev_req,
				 unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	int i;

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

	if (args->cos_id >= DLB2_NUM_COS_DOMAINS) {
		resp->status = DLB2_ST_INVALID_COS_ID;
		return -EINVAL;
	}

	if (args->cos_strict) {
		if (dlb2_list_empty(&domain->avail_ldb_ports[args->cos_id])) {
			resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
			return -EINVAL;
		}
	} else {
		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			if (!dlb2_list_empty(&domain->avail_ldb_ports[i]))
				break;
		}

		if (i == DLB2_NUM_COS_DOMAINS) {
			resp->status = DLB2_ST_LDB_PORTS_UNAVAILABLE;
			return -EINVAL;
		}
	}

	/* Check cache-line alignment */
	if ((cq_dma_base & 0x3F) != 0) {
		resp->status = DLB2_ST_INVALID_CQ_VIRT_ADDR;
		return -EINVAL;
	}

	if (args->cq_depth != 1 &&
	    args->cq_depth != 2 &&
	    args->cq_depth != 4 &&
	    args->cq_depth != 8 &&
	    args->cq_depth != 16 &&
	    args->cq_depth != 32 &&
	    args->cq_depth != 64 &&
	    args->cq_depth != 128 &&
	    args->cq_depth != 256 &&
	    args->cq_depth != 512 &&
	    args->cq_depth != 1024) {
		resp->status = DLB2_ST_INVALID_CQ_DEPTH;
		return -EINVAL;
	}

	/* The history list size must be >= 1 */
	if (!args->cq_history_list_size) {
		resp->status = DLB2_ST_INVALID_HIST_LIST_DEPTH;
		return -EINVAL;
	}

	if (args->cq_history_list_size > domain->avail_hist_list_entries) {
		resp->status = DLB2_ST_HIST_LIST_ENTRIES_UNAVAILABLE;
		return -EINVAL;
	}

	return 0;
}


/**
 * dlb2_hw_create_ldb_port() - Allocate and initialize a load-balanced port and
 *	its resources.
 * @hw:	Contains the current state of the DLB2 hardware.
 * @domain_id: Domain ID
 * @args: User-provided arguments.
 * @cq_dma_base: Base DMA address for consumer queue memory
 * @resp: Response to user.
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Return: returns < 0 on error, 0 otherwise. If the driver is unable to
 * satisfy a request, resp->status will be set accordingly.
 */
int dlb2_hw_create_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_ldb_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	int ret, cos_id, i;

	dlb2_log_create_ldb_port_args(hw,
				      domain_id,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_ldb_port_args(hw,
					       domain_id,
					       cq_dma_base,
					       args,
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

	if (args->cos_strict) {
		cos_id = args->cos_id;

		port = DLB2_DOM_LIST_HEAD(domain->avail_ldb_ports[cos_id],
					  typeof(*port));
	} else {
		int idx;

		for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
			idx = (args->cos_id + i) % DLB2_NUM_COS_DOMAINS;

			port = DLB2_DOM_LIST_HEAD(domain->avail_ldb_ports[idx],
						  typeof(*port));
			if (port)
				break;
		}

		cos_id = idx;
	}

	if (port == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no available ldb ports\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	if (port->configured) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: avail_ldb_ports contains configured ports.\n",
			    __func__);
		return -EFAULT;
	}

	ret = dlb2_configure_ldb_port(hw,
				      domain,
				      port,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);
	if (ret < 0)
		return ret;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list.
	 */
	dlb2_list_del(&domain->avail_ldb_ports[cos_id], &port->domain_list);

	dlb2_list_add(&domain->used_ldb_ports[cos_id], &port->domain_list);

	resp->status = 0;
	resp->id = (vdev_req) ? port->id.virt_id : port->id.phys_id;

	return 0;
}

static void
dlb2_log_create_dir_port_args(struct dlb2_hw *hw,
			      u32 domain_id,
			      uintptr_t cq_dma_base,
			      struct dlb2_create_dir_port_args *args,
			      bool vdev_req,
			      unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create directed port arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID:                 %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tCQ depth:                  %d\n",
		    args->cq_depth);
	DLB2_HW_DBG(hw, "\tCQ base address:           0x%lx\n",
		    cq_dma_base);
}

static struct dlb2_dir_pq_pair *
dlb2_get_domain_used_dir_pq(u32 id,
			    bool vdev_req,
			    struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_dir_pq_pair *port;
	RTE_SET_USED(iter);

	if (id >= DLB2_MAX_NUM_DIR_PORTS)
		return NULL;

	DLB2_DOM_LIST_FOR(domain->used_dir_pq_pairs, port, iter)
		if ((!vdev_req && port->id.phys_id == id) ||
		    (vdev_req && port->id.virt_id == id))
			return port;

	return NULL;
}

static int
dlb2_verify_create_dir_port_args(struct dlb2_hw *hw,
				 u32 domain_id,
				 uintptr_t cq_dma_base,
				 struct dlb2_create_dir_port_args *args,
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

	/*
	 * If the user claims the queue is already configured, validate
	 * the queue ID, its domain, and whether the queue is configured.
	 */
	if (args->queue_id != -1) {
		struct dlb2_dir_pq_pair *queue;

		queue = dlb2_get_domain_used_dir_pq(args->queue_id,
						    vdev_req,
						    domain);

		if (queue == NULL || queue->domain_id.phys_id !=
				domain->id.phys_id ||
				!queue->queue_configured) {
			resp->status = DLB2_ST_INVALID_DIR_QUEUE_ID;
			return -EINVAL;
		}
	}

	/*
	 * If the port's queue is not configured, validate that a free
	 * port-queue pair is available.
	 */
	if (args->queue_id == -1 &&
	    dlb2_list_empty(&domain->avail_dir_pq_pairs)) {
		resp->status = DLB2_ST_DIR_PORTS_UNAVAILABLE;
		return -EINVAL;
	}

	/* Check cache-line alignment */
	if ((cq_dma_base & 0x3F) != 0) {
		resp->status = DLB2_ST_INVALID_CQ_VIRT_ADDR;
		return -EINVAL;
	}

	if (args->cq_depth != 1 &&
	    args->cq_depth != 2 &&
	    args->cq_depth != 4 &&
	    args->cq_depth != 8 &&
	    args->cq_depth != 16 &&
	    args->cq_depth != 32 &&
	    args->cq_depth != 64 &&
	    args->cq_depth != 128 &&
	    args->cq_depth != 256 &&
	    args->cq_depth != 512 &&
	    args->cq_depth != 1024) {
		resp->status = DLB2_ST_INVALID_CQ_DEPTH;
		return -EINVAL;
	}

	return 0;
}

static void dlb2_dir_port_configure_pp(struct dlb2_hw *hw,
				       struct dlb2_hw_domain *domain,
				       struct dlb2_dir_pq_pair *port,
				       bool vdev_req,
				       unsigned int vdev_id)
{
	union dlb2_sys_dir_pp2vas r0 = { {0} };
	union dlb2_sys_dir_pp_v r4 = { {0} };

	r0.field.vas = domain->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_PP2VAS(port->id.phys_id), r0.val);

	if (vdev_req) {
		union dlb2_sys_vf_dir_vpp2pp r1 = { {0} };
		union dlb2_sys_dir_pp2vdev r2 = { {0} };
		union dlb2_sys_vf_dir_vpp_v r3 = { {0} };
		unsigned int offs;
		u32 virt_id;

		/*
		 * DLB uses producer port address bits 17:12 to determine the
		 * producer port ID. In Scalable IOV mode, PP accesses come
		 * through the PF MMIO window for the physical producer port,
		 * so for translation purposes the virtual and physical port
		 * IDs are equal.
		 */
		if (hw->virt_mode == DLB2_VIRT_SRIOV)
			virt_id = port->id.virt_id;
		else
			virt_id = port->id.phys_id;

		r1.field.pp = port->id.phys_id;

		offs = vdev_id * DLB2_MAX_NUM_DIR_PORTS + virt_id;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VPP2PP(offs), r1.val);

		r2.field.vdev = vdev_id;

		DLB2_CSR_WR(hw,
			    DLB2_SYS_DIR_PP2VDEV(port->id.phys_id),
			    r2.val);

		r3.field.vpp_v = 1;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VPP_V(offs), r3.val);
	}

	r4.field.pp_v = 1;

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_PP_V(port->id.phys_id),
		    r4.val);
}

static int dlb2_dir_port_configure_cq(struct dlb2_hw *hw,
				      struct dlb2_hw_domain *domain,
				      struct dlb2_dir_pq_pair *port,
				      uintptr_t cq_dma_base,
				      struct dlb2_create_dir_port_args *args,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	union dlb2_sys_dir_cq_addr_l r0 = { {0} };
	union dlb2_sys_dir_cq_addr_u r1 = { {0} };
	union dlb2_sys_dir_cq2vf_pf_ro r2 = { {0} };
	union dlb2_chp_dir_cq_tkn_depth_sel r3 = { {0} };
	union dlb2_lsp_cq_dir_tkn_depth_sel_dsi r4 = { {0} };
	union dlb2_sys_dir_cq_fmt r9 = { {0} };
	union dlb2_sys_dir_cq_at r10 = { {0} };
	union dlb2_sys_dir_cq_pasid r11 = { {0} };
	union dlb2_chp_dir_cq2vas r12 = { {0} };

	/* The CQ address is 64B-aligned, and the DLB only wants bits [63:6] */
	r0.field.addr_l = cq_dma_base >> 6;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_ADDR_L(port->id.phys_id), r0.val);

	r1.field.addr_u = cq_dma_base >> 32;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_ADDR_U(port->id.phys_id), r1.val);

	/*
	 * 'ro' == relaxed ordering. This setting allows DLB2 to write
	 * cache lines out-of-order (but QEs within a cache line are always
	 * updated in-order).
	 */
	r2.field.vf = vdev_id;
	r2.field.is_pf = !vdev_req && (hw->virt_mode != DLB2_VIRT_SIOV);
	r2.field.ro = 1;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ2VF_PF_RO(port->id.phys_id), r2.val);

	if (args->cq_depth <= 8) {
		r3.field.token_depth_select = 1;
	} else if (args->cq_depth == 16) {
		r3.field.token_depth_select = 2;
	} else if (args->cq_depth == 32) {
		r3.field.token_depth_select = 3;
	} else if (args->cq_depth == 64) {
		r3.field.token_depth_select = 4;
	} else if (args->cq_depth == 128) {
		r3.field.token_depth_select = 5;
	} else if (args->cq_depth == 256) {
		r3.field.token_depth_select = 6;
	} else if (args->cq_depth == 512) {
		r3.field.token_depth_select = 7;
	} else if (args->cq_depth == 1024) {
		r3.field.token_depth_select = 8;
	} else {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: invalid CQ depth\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_TKN_DEPTH_SEL(port->id.phys_id),
		    r3.val);

	/*
	 * To support CQs with depth less than 8, program the token count
	 * register with a non-zero initial value. Operations such as domain
	 * reset must take this initial value into account when quiescing the
	 * CQ.
	 */
	port->init_tkn_cnt = 0;

	if (args->cq_depth < 8) {
		union dlb2_lsp_cq_dir_tkn_cnt r13 = { {0} };

		port->init_tkn_cnt = 8 - args->cq_depth;

		r13.field.count = port->init_tkn_cnt;

		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_DIR_TKN_CNT(port->id.phys_id),
			    r13.val);
	} else {
		DLB2_CSR_WR(hw,
			    DLB2_LSP_CQ_DIR_TKN_CNT(port->id.phys_id),
			    DLB2_LSP_CQ_DIR_TKN_CNT_RST);
	}

	r4.field.token_depth_select = r3.field.token_depth_select;
	r4.field.disable_wb_opt = 0;
	r4.field.ignore_depth = 0;

	DLB2_CSR_WR(hw,
		    DLB2_LSP_CQ_DIR_TKN_DEPTH_SEL_DSI(port->id.phys_id),
		    r4.val);

	/* Reset the CQ write pointer */
	DLB2_CSR_WR(hw,
		    DLB2_CHP_DIR_CQ_WPTR(port->id.phys_id),
		    DLB2_CHP_DIR_CQ_WPTR_RST);

	/* Virtualize the PPID */
	r9.field.keep_pf_ppid = 0;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_FMT(port->id.phys_id), r9.val);

	/*
	 * Address translation (AT) settings: 0: untranslated, 2: translated
	 * (see ATS spec regarding Address Type field for more details)
	 */
	r10.field.cq_at = 0;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_CQ_AT(port->id.phys_id), r10.val);

	if (vdev_req && hw->virt_mode == DLB2_VIRT_SIOV) {
		r11.field.pasid = hw->pasid[vdev_id];
		r11.field.fmt2 = 1;
	}

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_CQ_PASID(port->id.phys_id),
		    r11.val);

	r12.field.cq2vas = domain->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_CHP_DIR_CQ2VAS(port->id.phys_id), r12.val);

	return 0;
}

static int dlb2_configure_dir_port(struct dlb2_hw *hw,
				   struct dlb2_hw_domain *domain,
				   struct dlb2_dir_pq_pair *port,
				   uintptr_t cq_dma_base,
				   struct dlb2_create_dir_port_args *args,
				   bool vdev_req,
				   unsigned int vdev_id)
{
	int ret;

	ret = dlb2_dir_port_configure_cq(hw,
					 domain,
					 port,
					 cq_dma_base,
					 args,
					 vdev_req,
					 vdev_id);

	if (ret < 0)
		return ret;

	dlb2_dir_port_configure_pp(hw,
				   domain,
				   port,
				   vdev_req,
				   vdev_id);

	dlb2_dir_port_cq_enable(hw, port);

	port->enabled = true;

	port->port_configured = true;

	return 0;
}

/**
 * dlb2_hw_create_dir_port() - Allocate and initialize a DLB directed port
 *	and queue. The port/queue pair have the same ID and name.
 * @hw:	Contains the current state of the DLB2 hardware.
 * @domain_id: Domain ID
 * @args: User-provided arguments.
 * @cq_dma_base: Base DMA address for consumer queue memory
 * @resp: Response to user.
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Return: returns < 0 on error, 0 otherwise. If the driver is unable to
 * satisfy a request, resp->status will be set accordingly.
 */
int dlb2_hw_create_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_dir_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_req,
			    unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *port;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_create_dir_port_args(hw,
				      domain_id,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_dir_port_args(hw,
					       domain_id,
					       cq_dma_base,
					       args,
					       resp,
					       vdev_req,
					       vdev_id);
	if (ret)
		return ret;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (args->queue_id != -1)
		port = dlb2_get_domain_used_dir_pq(args->queue_id,
						   vdev_req,
						   domain);
	else
		port = DLB2_DOM_LIST_HEAD(domain->avail_dir_pq_pairs,
					  typeof(*port));
	if (port == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no available dir ports\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	ret = dlb2_configure_dir_port(hw,
				      domain,
				      port,
				      cq_dma_base,
				      args,
				      vdev_req,
				      vdev_id);
	if (ret < 0)
		return ret;

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list (if it's not already there).
	 */
	if (args->queue_id == -1) {
		dlb2_list_del(&domain->avail_dir_pq_pairs, &port->domain_list);

		dlb2_list_add(&domain->used_dir_pq_pairs, &port->domain_list);
	}

	resp->status = 0;
	resp->id = (vdev_req) ? port->id.virt_id : port->id.phys_id;

	return 0;
}

static void dlb2_configure_dir_queue(struct dlb2_hw *hw,
				     struct dlb2_hw_domain *domain,
				     struct dlb2_dir_pq_pair *queue,
				     struct dlb2_create_dir_queue_args *args,
				     bool vdev_req,
				     unsigned int vdev_id)
{
	union dlb2_sys_dir_vasqid_v r0 = { {0} };
	union dlb2_sys_dir_qid_its r1 = { {0} };
	union dlb2_lsp_qid_dir_depth_thrsh r2 = { {0} };
	union dlb2_sys_dir_qid_v r5 = { {0} };

	unsigned int offs;

	/* QID write permissions are turned on when the domain is started */
	r0.field.vasqid_v = 0;

	offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_QUEUES +
		queue->id.phys_id;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_VASQID_V(offs), r0.val);

	/* Don't timestamp QEs that pass through this queue */
	r1.field.qid_its = 0;

	DLB2_CSR_WR(hw,
		    DLB2_SYS_DIR_QID_ITS(queue->id.phys_id),
		    r1.val);

	r2.field.thresh = args->depth_threshold;

	DLB2_CSR_WR(hw,
		    DLB2_LSP_QID_DIR_DEPTH_THRSH(queue->id.phys_id),
		    r2.val);

	if (vdev_req) {
		union dlb2_sys_vf_dir_vqid_v r3 = { {0} };
		union dlb2_sys_vf_dir_vqid2qid r4 = { {0} };

		offs = vdev_id * DLB2_MAX_NUM_DIR_QUEUES + queue->id.virt_id;

		r3.field.vqid_v = 1;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VQID_V(offs), r3.val);

		r4.field.qid = queue->id.phys_id;

		DLB2_CSR_WR(hw, DLB2_SYS_VF_DIR_VQID2QID(offs), r4.val);
	}

	r5.field.qid_v = 1;

	DLB2_CSR_WR(hw, DLB2_SYS_DIR_QID_V(queue->id.phys_id), r5.val);

	queue->queue_configured = true;
}

static void
dlb2_log_create_dir_queue_args(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_create_dir_queue_args *args,
			       bool vdev_req,
			       unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 create directed queue arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n", domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n", args->port_id);
}

static int
dlb2_verify_create_dir_queue_args(struct dlb2_hw *hw,
				  u32 domain_id,
				  struct dlb2_create_dir_queue_args *args,
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

	/*
	 * If the user claims the port is already configured, validate the port
	 * ID, its domain, and whether the port is configured.
	 */
	if (args->port_id != -1) {
		struct dlb2_dir_pq_pair *port;

		port = dlb2_get_domain_used_dir_pq(args->port_id,
						   vdev_req,
						   domain);

		if (port == NULL || port->domain_id.phys_id !=
				domain->id.phys_id || !port->port_configured) {
			resp->status = DLB2_ST_INVALID_PORT_ID;
			return -EINVAL;
		}
	}

	/*
	 * If the queue's port is not configured, validate that a free
	 * port-queue pair is available.
	 */
	if (args->port_id == -1 &&
	    dlb2_list_empty(&domain->avail_dir_pq_pairs)) {
		resp->status = DLB2_ST_DIR_QUEUES_UNAVAILABLE;
		return -EINVAL;
	}

	return 0;
}

/**
 * dlb2_hw_create_dir_queue() - Allocate and initialize a DLB DIR queue.
 * @hw:	Contains the current state of the DLB2 hardware.
 * @domain_id: Domain ID
 * @args: User-provided arguments.
 * @resp: Response to user.
 * @vdev_req: Request came from a virtual device.
 * @vdev_id: If vdev_req is true, this contains the virtual device's ID.
 *
 * Return: returns < 0 on error, 0 otherwise. If the driver is unable to
 * satisfy a request, resp->status will be set accordingly.
 */
int dlb2_hw_create_dir_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_dir_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	struct dlb2_dir_pq_pair *queue;
	struct dlb2_hw_domain *domain;
	int ret;

	dlb2_log_create_dir_queue_args(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_create_dir_queue_args(hw,
						domain_id,
						args,
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

	if (args->port_id != -1)
		queue = dlb2_get_domain_used_dir_pq(args->port_id,
						    vdev_req,
						    domain);
	else
		queue = DLB2_DOM_LIST_HEAD(domain->avail_dir_pq_pairs,
					   typeof(*queue));
	if (queue == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: no available dir queues\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	dlb2_configure_dir_queue(hw, domain, queue, args, vdev_req, vdev_id);

	/*
	 * Configuration succeeded, so move the resource from the 'avail' to
	 * the 'used' list (if it's not already there).
	 */
	if (args->port_id == -1) {
		dlb2_list_del(&domain->avail_dir_pq_pairs,
			      &queue->domain_list);

		dlb2_list_add(&domain->used_dir_pq_pairs,
			      &queue->domain_list);
	}

	resp->status = 0;

	resp->id = (vdev_req) ? queue->id.virt_id : queue->id.phys_id;

	return 0;
}

static bool
dlb2_port_find_slot_with_pending_map_queue(struct dlb2_ldb_port *port,
					   struct dlb2_ldb_queue *queue,
					   int *slot)
{
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		struct dlb2_ldb_port_qid_map *map = &port->qid_map[i];

		if (map->state == DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP &&
		    map->pending_qid == queue->id.phys_id)
			break;
	}

	*slot = i;

	return (i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ);
}

static void dlb2_ldb_port_change_qid_priority(struct dlb2_hw *hw,
					      struct dlb2_ldb_port *port,
					      int slot,
					      struct dlb2_map_qid_args *args)
{
	union dlb2_lsp_cq2priov r0;

	/* Read-modify-write the priority and valid bit register */
	r0.val = DLB2_CSR_RD(hw, DLB2_LSP_CQ2PRIOV(port->id.phys_id));

	r0.field.v |= 1 << slot;
	r0.field.prio |= (args->priority & 0x7) << slot * 3;

	DLB2_CSR_WR(hw, DLB2_LSP_CQ2PRIOV(port->id.phys_id), r0.val);

	dlb2_flush_csr(hw);

	port->qid_map[slot].priority = args->priority;
}

static int dlb2_verify_map_qid_slot_available(struct dlb2_ldb_port *port,
					      struct dlb2_ldb_queue *queue,
					      struct dlb2_cmd_response *resp)
{
	enum dlb2_qid_map_state state;
	int i;

	/* Unused slot available? */
	if (port->num_mappings < DLB2_MAX_NUM_QIDS_PER_LDB_CQ)
		return 0;

	/*
	 * If the queue is already mapped (from the application's perspective),
	 * this is simply a priority update.
	 */
	state = DLB2_QUEUE_MAPPED;
	if (dlb2_port_find_slot_queue(port, state, queue, &i))
		return 0;

	state = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, state, queue, &i))
		return 0;

	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i))
		return 0;

	/*
	 * If the slot contains an unmap in progress, it's considered
	 * available.
	 */
	state = DLB2_QUEUE_UNMAP_IN_PROG;
	if (dlb2_port_find_slot(port, state, &i))
		return 0;

	state = DLB2_QUEUE_UNMAPPED;
	if (dlb2_port_find_slot(port, state, &i))
		return 0;

	resp->status = DLB2_ST_NO_QID_SLOTS_AVAILABLE;
	return -EINVAL;
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

static struct dlb2_ldb_port *
dlb2_get_domain_used_ldb_port(u32 id,
			      bool vdev_req,
			      struct dlb2_hw_domain *domain)
{
	struct dlb2_list_entry *iter;
	struct dlb2_ldb_port *port;
	int i;
	RTE_SET_USED(iter);

	if (id >= DLB2_MAX_NUM_LDB_PORTS)
		return NULL;

	for (i = 0; i < DLB2_NUM_COS_DOMAINS; i++) {
		DLB2_DOM_LIST_FOR(domain->used_ldb_ports[i], port, iter)
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;

		DLB2_DOM_LIST_FOR(domain->avail_ldb_ports[i], port, iter)
			if ((!vdev_req && port->id.phys_id == id) ||
			    (vdev_req && port->id.virt_id == id))
				return port;
	}

	return NULL;
}

static int dlb2_verify_map_qid_args(struct dlb2_hw *hw,
				    u32 domain_id,
				    struct dlb2_map_qid_args *args,
				    struct dlb2_cmd_response *resp,
				    bool vdev_req,
				    unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;
	struct dlb2_ldb_queue *queue;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (domain == NULL) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (port == NULL || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (args->priority >= DLB2_QID_PRIORITIES) {
		resp->status = DLB2_ST_INVALID_PRIORITY;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);

	if (queue == NULL || !queue->configured) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	if (queue->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	if (port->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	return 0;
}

static void dlb2_log_map_qid(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_map_qid_args *args,
			     bool vdev_req,
			     unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 map QID arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    args->port_id);
	DLB2_HW_DBG(hw, "\tQueue ID:  %d\n",
		    args->qid);
	DLB2_HW_DBG(hw, "\tPriority:  %d\n",
		    args->priority);
}

int dlb2_hw_map_qid(struct dlb2_hw *hw,
		    u32 domain_id,
		    struct dlb2_map_qid_args *args,
		    struct dlb2_cmd_response *resp,
		    bool vdev_req,
		    unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	enum dlb2_qid_map_state st;
	struct dlb2_ldb_port *port;
	int ret, i, id;
	u8 prio;

	dlb2_log_map_qid(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_map_qid_args(hw,
				       domain_id,
				       args,
				       resp,
				       vdev_req,
				       vdev_id);
	if (ret)
		return ret;

	prio = args->priority;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);
	if (domain == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: domain not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);
	if (port == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);
	if (queue == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: queue not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	/*
	 * If there are any outstanding detach operations for this port,
	 * attempt to complete them. This may be necessary to free up a QID
	 * slot for this requested mapping.
	 */
	if (port->num_pending_removals)
		dlb2_domain_finish_unmap_port(hw, domain, port);

	ret = dlb2_verify_map_qid_slot_available(port, queue, resp);
	if (ret)
		return ret;

	/* Hardware requires disabling the CQ before mapping QIDs. */
	if (port->enabled)
		dlb2_ldb_port_cq_disable(hw, port);

	/*
	 * If this is only a priority change, don't perform the full QID->CQ
	 * mapping procedure
	 */
	st = DLB2_QUEUE_MAPPED;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
			DLB2_HW_ERR(hw,
				    "[%s():%d] Internal error: port slot tracking failed\n",
				    __func__, __LINE__);
			return -EFAULT;
		}

		if (prio != port->qid_map[i].priority) {
			dlb2_ldb_port_change_qid_priority(hw, port, i, args);
			DLB2_HW_DBG(hw, "DLB2 map: priority change\n");
		}

		st = DLB2_QUEUE_MAPPED;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto map_qid_done;
	}

	st = DLB2_QUEUE_UNMAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
			DLB2_HW_ERR(hw,
				    "[%s():%d] Internal error: port slot tracking failed\n",
				    __func__, __LINE__);
			return -EFAULT;
		}

		if (prio != port->qid_map[i].priority) {
			dlb2_ldb_port_change_qid_priority(hw, port, i, args);
			DLB2_HW_DBG(hw, "DLB2 map: priority change\n");
		}

		st = DLB2_QUEUE_MAPPED;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto map_qid_done;
	}

	/*
	 * If this is a priority change on an in-progress mapping, don't
	 * perform the full QID->CQ mapping procedure.
	 */
	st = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
			DLB2_HW_ERR(hw,
				    "[%s():%d] Internal error: port slot tracking failed\n",
				    __func__, __LINE__);
			return -EFAULT;
		}

		port->qid_map[i].priority = prio;

		DLB2_HW_DBG(hw, "DLB2 map: priority change only\n");

		goto map_qid_done;
	}

	/*
	 * If this is a priority change on a pending mapping, update the
	 * pending priority
	 */
	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i)) {
		if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
			DLB2_HW_ERR(hw,
				    "[%s():%d] Internal error: port slot tracking failed\n",
				    __func__, __LINE__);
			return -EFAULT;
		}

		port->qid_map[i].pending_priority = prio;

		DLB2_HW_DBG(hw, "DLB2 map: priority change only\n");

		goto map_qid_done;
	}

	/*
	 * If all the CQ's slots are in use, then there's an unmap in progress
	 * (guaranteed by dlb2_verify_map_qid_slot_available()), so add this
	 * mapping to pending_map and return. When the removal is completed for
	 * the slot's current occupant, this mapping will be performed.
	 */
	if (!dlb2_port_find_slot(port, DLB2_QUEUE_UNMAPPED, &i)) {
		if (dlb2_port_find_slot(port, DLB2_QUEUE_UNMAP_IN_PROG, &i)) {
			enum dlb2_qid_map_state st;

			if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
				DLB2_HW_ERR(hw,
					    "[%s():%d] Internal error: port slot tracking failed\n",
					    __func__, __LINE__);
				return -EFAULT;
			}

			port->qid_map[i].pending_qid = queue->id.phys_id;
			port->qid_map[i].pending_priority = prio;

			st = DLB2_QUEUE_UNMAP_IN_PROG_PENDING_MAP;

			ret = dlb2_port_slot_state_transition(hw, port, queue,
							      i, st);
			if (ret)
				return ret;

			DLB2_HW_DBG(hw, "DLB2 map: map pending removal\n");

			goto map_qid_done;
		}
	}

	/*
	 * If the domain has started, a special "dynamic" CQ->queue mapping
	 * procedure is required in order to safely update the CQ<->QID tables.
	 * The "static" procedure cannot be used when traffic is flowing,
	 * because the CQ<->QID tables cannot be updated atomically and the
	 * scheduler won't see the new mapping unless the queue's if_status
	 * changes, which isn't guaranteed.
	 */
	ret = dlb2_ldb_port_map_qid(hw, domain, port, queue, prio);

	/* If ret is less than zero, it's due to an internal error */
	if (ret < 0)
		return ret;

map_qid_done:
	if (port->enabled)
		dlb2_ldb_port_cq_enable(hw, port);

	resp->status = 0;

	return 0;
}

static void dlb2_log_unmap_qid(struct dlb2_hw *hw,
			       u32 domain_id,
			       struct dlb2_unmap_qid_args *args,
			       bool vdev_req,
			       unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB2 unmap QID arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from vdev %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tDomain ID: %d\n",
		    domain_id);
	DLB2_HW_DBG(hw, "\tPort ID:   %d\n",
		    args->port_id);
	DLB2_HW_DBG(hw, "\tQueue ID:  %d\n",
		    args->qid);
	if (args->qid < DLB2_MAX_NUM_LDB_QUEUES)
		DLB2_HW_DBG(hw, "\tQueue's num mappings:  %d\n",
			    hw->rsrcs.ldb_queues[args->qid].num_mappings);
}

static int dlb2_verify_unmap_qid_args(struct dlb2_hw *hw,
				      u32 domain_id,
				      struct dlb2_unmap_qid_args *args,
				      struct dlb2_cmd_response *resp,
				      bool vdev_req,
				      unsigned int vdev_id)
{
	enum dlb2_qid_map_state state;
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	struct dlb2_ldb_port *port;
	int slot;
	int id;

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (domain == NULL) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	if (!domain->configured) {
		resp->status = DLB2_ST_DOMAIN_NOT_CONFIGURED;
		return -EINVAL;
	}

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);

	if (port == NULL || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	if (port->domain_id.phys_id != domain->id.phys_id) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);

	if (queue == NULL || !queue->configured) {
		DLB2_HW_ERR(hw, "[%s()] Can't unmap unconfigured queue %d\n",
			    __func__, args->qid);
		resp->status = DLB2_ST_INVALID_QID;
		return -EINVAL;
	}

	/*
	 * Verify that the port has the queue mapped. From the application's
	 * perspective a queue is mapped if it is actually mapped, the map is
	 * in progress, or the map is blocked pending an unmap.
	 */
	state = DLB2_QUEUE_MAPPED;
	if (dlb2_port_find_slot_queue(port, state, queue, &slot))
		return 0;

	state = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, state, queue, &slot))
		return 0;

	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &slot))
		return 0;

	resp->status = DLB2_ST_INVALID_QID;
	return -EINVAL;
}

int dlb2_hw_unmap_qid(struct dlb2_hw *hw,
		      u32 domain_id,
		      struct dlb2_unmap_qid_args *args,
		      struct dlb2_cmd_response *resp,
		      bool vdev_req,
		      unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_queue *queue;
	enum dlb2_qid_map_state st;
	struct dlb2_ldb_port *port;
	bool unmap_complete;
	int i, ret, id;

	dlb2_log_unmap_qid(hw, domain_id, args, vdev_req, vdev_id);

	/*
	 * Verify that hardware resources are available before attempting to
	 * satisfy the request. This simplifies the error unwinding code.
	 */
	ret = dlb2_verify_unmap_qid_args(hw,
					 domain_id,
					 args,
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

	id = args->port_id;

	port = dlb2_get_domain_used_ldb_port(id, vdev_req, domain);
	if (port == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	queue = dlb2_get_domain_ldb_queue(args->qid, vdev_req, domain);
	if (queue == NULL) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: queue not found\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	/*
	 * If the queue hasn't been mapped yet, we need to update the slot's
	 * state and re-enable the queue's inflights.
	 */
	st = DLB2_QUEUE_MAP_IN_PROG;
	if (dlb2_port_find_slot_queue(port, st, queue, &i)) {
		if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
			DLB2_HW_ERR(hw,
				    "[%s():%d] Internal error: port slot tracking failed\n",
				    __func__, __LINE__);
			return -EFAULT;
		}

		/*
		 * Since the in-progress map was aborted, re-enable the QID's
		 * inflights.
		 */
		if (queue->num_pending_additions == 0)
			dlb2_ldb_queue_set_inflight_limit(hw, queue);

		st = DLB2_QUEUE_UNMAPPED;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto unmap_qid_done;
	}

	/*
	 * If the queue mapping is on hold pending an unmap, we simply need to
	 * update the slot's state.
	 */
	if (dlb2_port_find_slot_with_pending_map_queue(port, queue, &i)) {
		if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
			DLB2_HW_ERR(hw,
				    "[%s():%d] Internal error: port slot tracking failed\n",
				    __func__, __LINE__);
			return -EFAULT;
		}

		st = DLB2_QUEUE_UNMAP_IN_PROG;
		ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
		if (ret)
			return ret;

		goto unmap_qid_done;
	}

	st = DLB2_QUEUE_MAPPED;
	if (!dlb2_port_find_slot_queue(port, st, queue, &i)) {
		DLB2_HW_ERR(hw,
			    "[%s()] Internal error: no available CQ slots\n",
			    __func__);
		return -EFAULT;
	}

	if (i >= DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB2_HW_ERR(hw,
			    "[%s():%d] Internal error: port slot tracking failed\n",
			    __func__, __LINE__);
		return -EFAULT;
	}

	/*
	 * QID->CQ mapping removal is an asynchronous procedure. It requires
	 * stopping the DLB2 from scheduling this CQ, draining all inflights
	 * from the CQ, then unmapping the queue from the CQ. This function
	 * simply marks the port as needing the queue unmapped, and (if
	 * necessary) starts the unmapping worker thread.
	 */
	dlb2_ldb_port_cq_disable(hw, port);

	st = DLB2_QUEUE_UNMAP_IN_PROG;
	ret = dlb2_port_slot_state_transition(hw, port, queue, i, st);
	if (ret)
		return ret;

	/*
	 * Attempt to finish the unmapping now, in case the port has no
	 * outstanding inflights. If that's not the case, this will fail and
	 * the unmapping will be completed at a later time.
	 */
	unmap_complete = dlb2_domain_finish_unmap_port(hw, domain, port);

	/*
	 * If the unmapping couldn't complete immediately, launch the worker
	 * thread (if it isn't already launched) to finish it later.
	 */
	if (!unmap_complete && !os_worker_active(hw))
		os_schedule_work(hw);

unmap_qid_done:
	resp->status = 0;

	return 0;
}

static void
dlb2_log_pending_port_unmaps_args(struct dlb2_hw *hw,
				  struct dlb2_pending_port_unmaps_args *args,
				  bool vdev_req,
				  unsigned int vdev_id)
{
	DLB2_HW_DBG(hw, "DLB unmaps in progress arguments:\n");
	if (vdev_req)
		DLB2_HW_DBG(hw, "(Request from VF %d)\n", vdev_id);
	DLB2_HW_DBG(hw, "\tPort ID: %d\n", args->port_id);
}

int dlb2_hw_pending_port_unmaps(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_pending_port_unmaps_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_req,
				unsigned int vdev_id)
{
	struct dlb2_hw_domain *domain;
	struct dlb2_ldb_port *port;

	dlb2_log_pending_port_unmaps_args(hw, args, vdev_req, vdev_id);

	domain = dlb2_get_domain_from_id(hw, domain_id, vdev_req, vdev_id);

	if (domain == NULL) {
		resp->status = DLB2_ST_INVALID_DOMAIN_ID;
		return -EINVAL;
	}

	port = dlb2_get_domain_used_ldb_port(args->port_id, vdev_req, domain);
	if (port == NULL || !port->configured) {
		resp->status = DLB2_ST_INVALID_PORT_ID;
		return -EINVAL;
	}

	resp->id = port->num_pending_removals;

	return 0;
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
		     __attribute((unused)) struct dlb2_start_domain_args *arg,
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

		offs = domain->id.phys_id * DLB2_MAX_NUM_DIR_PORTS +
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

	queue = dlb2_get_domain_used_dir_pq(id, vdev_req, domain);
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
